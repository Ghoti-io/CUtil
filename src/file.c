/**
 * @file
 *
 * Reading a whole file, and replacing one atomically.
 *
 * The design and the reasoning behind each decision are recorded in
 * `documentation/file.md`.
 *
 * Copyright 2026 by Corey Pennycuff
 */

/* mkstemp() and fsync() are POSIX, and this library compiles with -std=c17,
 * which declares neither.  The feature-test macro goes at the very top,
 * before any header: glibc reads it when the first one is included and
 * ignores it afterwards.  The suite spells it this way in cutil's thread.c
 * and path.c. */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ghoti.io/cutil/file.h>
#include <ghoti.io/cutil/path.h>
#include "path_internal.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

/** How much is read at a time before the buffer is grown. */
#define GCU_FILE_CHUNK ((size_t)8192)

/** The six characters mkstemp() replaces, plus the terminator. */
#define GCU_FILE_TEMPLATE "XXXXXX"

const char * gcu_file_result_string(GCU_File_Result result) {
  switch (result) {
    case GCU_FILE_OK:           return "ok";
    case GCU_FILE_ERR_INVALID:  return "invalid argument";
    case GCU_FILE_ERR_OOM:      return "out of memory";
    case GCU_FILE_ERR_LIMIT:    return "file is larger than the limit";
    case GCU_FILE_ERR_IO:       return "file operation failed";
    case GCU_FILE_RESULT_COUNT: break;
  }
  return "unknown";
}

void gcu_file_free(const GCU_Allocator * allocator, void * data) {
  if (!data) {
    return;
  }
  if (!allocator) {
    allocator = gcu_allocator_default();
  }
  gcu_allocator_free(allocator, data);
}

//////////////////////////////////////////////////////////////////////////////
// Opening
//////////////////////////////////////////////////////////////////////////////

/**
 * Open a path whose bytes are UTF-8.
 *
 * On Windows this goes through the wide entry point.  fopen() there takes the
 * path in the process code page, which cannot represent every name the
 * filesystem accepts, so a path merely passing through it opens a different
 * file or none at all.
 */
static FILE * file_open(const char * path, const char * mode,
    const GCU_Allocator * allocator) {
#ifdef _WIN32
  /* TODO(windows): never compiled or run on Windows; see WINDOWS-TODO.md. */
  wchar_t * wide_path = gcu_path_internal_to_wide(allocator, path);
  if (!wide_path) {
    return NULL;
  }
  wchar_t * wide_mode = gcu_path_internal_to_wide(allocator, mode);
  if (!wide_mode) {
    gcu_allocator_free(allocator, wide_path);
    return NULL;
  }
  FILE * stream = _wfopen(wide_path, wide_mode);
  gcu_allocator_free(allocator, wide_path);
  gcu_allocator_free(allocator, wide_mode);
  return stream;
#else
  (void)allocator;
  return fopen(path, mode);
#endif
}

/** Remove a path whose bytes are UTF-8. */
static void file_remove(const char * path, const GCU_Allocator * allocator) {
#ifdef _WIN32
  /* TODO(windows): never compiled or run on Windows; see WINDOWS-TODO.md. */
  wchar_t * wide = gcu_path_internal_to_wide(allocator, path);
  if (wide) {
    _wremove(wide);
    gcu_allocator_free(allocator, wide);
  }
#else
  (void)allocator;
  remove(path);
#endif
}

//////////////////////////////////////////////////////////////////////////////
// Reading
//////////////////////////////////////////////////////////////////////////////

GCU_File_Result gcu_file_read(const char * path, size_t max_bytes,
    const GCU_Allocator * allocator, void ** out_data, size_t * out_len) {
  if (!path || !out_data || !out_len) {
    return GCU_FILE_ERR_INVALID;
  }
  if (!allocator) {
    allocator = gcu_allocator_default();
  }

  FILE * stream = file_open(path, "rb", allocator);
  if (!stream) {
    return GCU_FILE_ERR_IO;
  }

  // One byte over the limit is enough to know the file exceeds it, so a
  // limited read never holds more than it is allowed to plus that byte.  The
  // extra byte on top is the NUL, which is not part of the content.
  size_t ceiling = 0;
  if (max_bytes != GCU_FILE_UNLIMITED) {
    ceiling = max_bytes + 2;
  }

  size_t capacity = GCU_FILE_CHUNK;
  if (ceiling && ceiling < capacity) {
    capacity = ceiling;
  }

  char * buffer = (char *)gcu_allocator_malloc(allocator, capacity);
  if (!buffer) {
    fclose(stream);
    return GCU_FILE_ERR_OOM;
  }

  size_t used = 0;
  GCU_File_Result result = GCU_FILE_OK;

  for (;;) {
    // The file is read rather than sized, because a pipe, a character device
    // and anything under /proc all report a size of zero and would come back
    // empty from a seek-and-tell.
    if (used + 1 >= capacity) {
      size_t wanted = capacity * 2;
      if (ceiling && wanted > ceiling) {
        wanted = ceiling;
      }
      if (wanted <= capacity) {
        // Cannot grow, and the limit is what stopped it.
        result = GCU_FILE_ERR_LIMIT;
        break;
      }
      char * grown = (char *)gcu_allocator_realloc(allocator, buffer, wanted);
      if (!grown) {
        result = GCU_FILE_ERR_OOM;
        break;
      }
      buffer = grown;
      capacity = wanted;
    }

    size_t room = capacity - used - 1;
    size_t got = fread(buffer + used, 1, room, stream);
    used += got;

    if (max_bytes != GCU_FILE_UNLIMITED && used > max_bytes) {
      result = GCU_FILE_ERR_LIMIT;
      break;
    }
    if (got < room) {
      if (ferror(stream)) {
        result = GCU_FILE_ERR_IO;
      }
      break;
    }
  }

  fclose(stream);

  if (result != GCU_FILE_OK) {
    gcu_allocator_free(allocator, buffer);
    return result;
  }

  // Not counted in the length: it is there so that a text caller can use the
  // result as a C string without copying it.
  buffer[used] = '\0';
  *out_data = buffer;
  *out_len = used;
  return GCU_FILE_OK;
}

//////////////////////////////////////////////////////////////////////////////
// Temporary files
//////////////////////////////////////////////////////////////////////////////

FILE * gcu_file_temp_stream(const GCU_File_Temp * temp) {
  return temp ? temp->stream : NULL;
}

const char * gcu_file_temp_path(const GCU_File_Temp * temp) {
  return temp ? temp->path : NULL;
}

GCU_File_Result gcu_file_temp_create(GCU_File_Temp * temp, const char * dir,
    const char * prefix, const GCU_Allocator * allocator) {
  if (!temp) {
    return GCU_FILE_ERR_INVALID;
  }
  // Zeroed before anything can fail, so that a caller who aborts after a
  // failed create is aborting a handle that is safe to abort.
  memset(temp, 0, sizeof *temp);
  if (!allocator) {
    allocator = gcu_allocator_default();
  }

  char * owned_dir = NULL;
  if (!dir) {
    if (gcu_path_temp_dir(allocator, &owned_dir) != GCU_PATH_OK) {
      return GCU_FILE_ERR_IO;
    }
    dir = owned_dir;
  }
  if (!prefix) {
    prefix = "tmp";
  }

  size_t stem = 0;
  GCU_Path_Result joined =
      gcu_path_join(GCU_PATH_NATIVE, dir, prefix, NULL, 0, &stem);
  if (joined != GCU_PATH_OK) {
    gcu_path_free(allocator, owned_dir);
    return GCU_FILE_ERR_INVALID;
  }

  size_t total = stem + sizeof GCU_FILE_TEMPLATE;
  char * path = (char *)gcu_allocator_malloc(allocator, total);
  if (!path) {
    gcu_path_free(allocator, owned_dir);
    return GCU_FILE_ERR_OOM;
  }
  joined = gcu_path_join(GCU_PATH_NATIVE, dir, prefix, path, stem + 1, NULL);
  gcu_path_free(allocator, owned_dir);
  if (joined != GCU_PATH_OK) {
    gcu_allocator_free(allocator, path);
    return GCU_FILE_ERR_IO;
  }
  memcpy(path + stem, GCU_FILE_TEMPLATE, sizeof GCU_FILE_TEMPLATE);

#ifdef _WIN32
  /* TODO(windows): never compiled or run on Windows; see WINDOWS-TODO.md.
   * _mktemp_s only chooses the name; _O_CREAT | _O_EXCL is what makes taking
   * it a single step that fails rather than following something already
   * there. */
  wchar_t * wide = gcu_path_internal_to_wide(allocator, path);
  if (!wide) {
    gcu_allocator_free(allocator, path);
    return GCU_FILE_ERR_OOM;
  }
  errno_t named = _wmktemp_s(wide, wcslen(wide) + 1);
  if (named != 0) {
    gcu_allocator_free(allocator, wide);
    gcu_allocator_free(allocator, path);
    return GCU_FILE_ERR_IO;
  }
  int fd = _wopen(wide, _O_CREAT | _O_EXCL | _O_BINARY | _O_RDWR,
      _S_IREAD | _S_IWRITE);
  if (fd < 0) {
    gcu_allocator_free(allocator, wide);
    gcu_allocator_free(allocator, path);
    return GCU_FILE_ERR_IO;
  }
  char * chosen = gcu_path_internal_from_wide(allocator, wide);
  gcu_allocator_free(allocator, wide);
  if (!chosen) {
    _close(fd);
    gcu_allocator_free(allocator, path);
    return GCU_FILE_ERR_OOM;
  }
  gcu_allocator_free(allocator, path);
  path = chosen;
  FILE * stream = _fdopen(fd, "w+b");
  if (!stream) {
    _close(fd);
    file_remove(path, allocator);
    gcu_allocator_free(allocator, path);
    return GCU_FILE_ERR_IO;
  }
#else
  // mkstemp() chooses the name and creates the file in one step that fails if
  // the name is taken, and opens it 0600.  Choosing a name and then opening
  // it is the classic way to follow somebody else's symbolic link.
  int fd = mkstemp(path);
  if (fd < 0) {
    gcu_allocator_free(allocator, path);
    return GCU_FILE_ERR_IO;
  }
  FILE * stream = fdopen(fd, "w+b");
  if (!stream) {
    // The file exists by now, so closing the descriptor is not enough.  This
    // is the branch that the two hand-written copies in `text` disagree
    // about, and the reason this is one function rather than several.
    close(fd);
    file_remove(path, allocator);
    gcu_allocator_free(allocator, path);
    return GCU_FILE_ERR_IO;
  }
#endif

  temp->path = path;
  temp->stream = stream;
  temp->allocator = allocator;
  return GCU_FILE_OK;
}

void gcu_file_temp_abort(GCU_File_Temp * temp) {
  if (!temp || !temp->path) {
    return;
  }
  if (temp->stream) {
    fclose(temp->stream);
  }
  file_remove(temp->path, temp->allocator);
  gcu_allocator_free(temp->allocator, temp->path);
  memset(temp, 0, sizeof *temp);
}

/**
 * Ask the operating system to commit a stream's bytes.
 *
 * fflush() only moves them out of stdio and into the kernel.  Without this
 * step the rename can be recorded while the content behind it is not, which
 * leaves a file that exists, has the right name, and is empty.
 */
static bool file_sync_stream(FILE * stream) {
#ifdef _WIN32
  /* TODO(windows): never compiled or run on Windows; see WINDOWS-TODO.md. */
  return _commit(_fileno(stream)) == 0;
#else
  return fsync(fileno(stream)) == 0;
#endif
}

/**
 * Ask the operating system to commit the directory entry itself.
 *
 * Best effort by design: several filesystems refuse fsync on a directory, and
 * failing a replacement that has otherwise completed would be worse than the
 * weaker promise.  See `documentation/file.md`.
 */
static void file_sync_directory(const char * path,
    const GCU_Allocator * allocator) {
#ifdef _WIN32
  /* Windows has no directory handle to commit; MoveFileEx with
   * MOVEFILE_WRITE_THROUGH already carries the request. */
  (void)path;
  (void)allocator;
#else
  size_t need = 0;
  if (gcu_path_dirname(GCU_PATH_NATIVE, path, NULL, 0, &need)
      != GCU_PATH_OK) {
    return;
  }
  char * dir = (char *)gcu_allocator_malloc(allocator, need + 1);
  if (!dir) {
    return;
  }
  if (gcu_path_dirname(GCU_PATH_NATIVE, path, dir, need + 1, NULL)
      == GCU_PATH_OK) {
    int fd = open(dir, O_RDONLY);
    if (fd >= 0) {
      (void)fsync(fd);
      close(fd);
    }
  }
  gcu_allocator_free(allocator, dir);
#endif
}

/** Move @p source over @p dest, replacing it. */
static bool file_replace(const char * source, const char * dest,
    GCU_File_Sync sync, const GCU_Allocator * allocator) {
#ifdef _WIN32
  /* TODO(windows): never compiled or run on Windows; see WINDOWS-TODO.md.
   * rename() on Windows refuses an existing destination; MoveFileEx is the
   * call that replaces one, and it is atomic for a same-volume move. */
  wchar_t * wide_source = gcu_path_internal_to_wide(allocator, source);
  wchar_t * wide_dest = gcu_path_internal_to_wide(allocator, dest);
  bool moved = false;
  if (wide_source && wide_dest) {
    DWORD flags = MOVEFILE_REPLACE_EXISTING;
    if (sync == GCU_FILE_SYNC_FULL) {
      flags |= MOVEFILE_WRITE_THROUGH;
    }
    moved = MoveFileExW(wide_source, wide_dest, flags) != 0;
  }
  gcu_allocator_free(allocator, wide_source);
  gcu_allocator_free(allocator, wide_dest);
  return moved;
#else
  (void)sync;
  (void)allocator;
  return rename(source, dest) == 0;
#endif
}

GCU_File_Result gcu_file_temp_commit(GCU_File_Temp * temp, const char * dest,
    GCU_File_Sync sync) {
  if (!temp || !temp->path || !temp->stream || !dest) {
    return GCU_FILE_ERR_INVALID;
  }

  const GCU_Allocator * allocator = temp->allocator;
  GCU_File_Result result = GCU_FILE_OK;

  if (fflush(temp->stream) != 0) {
    result = GCU_FILE_ERR_IO;
  }
  // These are the bytes, so a refusal here is reported.
  if (result == GCU_FILE_OK && sync == GCU_FILE_SYNC_FULL
      && !file_sync_stream(temp->stream)) {
    result = GCU_FILE_ERR_IO;
  }
  // Closed before the rename: Windows will not move a file that is open.
  if (fclose(temp->stream) != 0) {
    result = GCU_FILE_ERR_IO;
  }
  temp->stream = NULL;

  bool moved = false;
  if (result == GCU_FILE_OK) {
    moved = file_replace(temp->path, dest, sync, allocator);
    if (!moved) {
      result = GCU_FILE_ERR_IO;
    }
  }

  if (moved) {
    if (sync == GCU_FILE_SYNC_FULL) {
      file_sync_directory(dest, allocator);
    }
  }
  else {
    // Nothing reached the destination, so the destination is untouched and
    // the temporary file is litter.
    file_remove(temp->path, allocator);
  }

  gcu_allocator_free(allocator, temp->path);
  memset(temp, 0, sizeof *temp);
  return result;
}

GCU_File_Result gcu_file_write_atomic(const char * path, const void * data,
    size_t len, GCU_File_Sync sync, const GCU_Allocator * allocator) {
  if (!path || (!data && len)) {
    return GCU_FILE_ERR_INVALID;
  }
  if (!allocator) {
    allocator = gcu_allocator_default();
  }

  // The temporary file goes in the destination's own directory, because a
  // rename between filesystems is a copy and a copy is not atomic.
  size_t need = 0;
  if (gcu_path_dirname(GCU_PATH_NATIVE, path, NULL, 0, &need)
      != GCU_PATH_OK) {
    return GCU_FILE_ERR_INVALID;
  }
  char * dir = (char *)gcu_allocator_malloc(allocator, need + 1);
  if (!dir) {
    return GCU_FILE_ERR_OOM;
  }
  if (gcu_path_dirname(GCU_PATH_NATIVE, path, dir, need + 1, NULL)
      != GCU_PATH_OK) {
    gcu_allocator_free(allocator, dir);
    return GCU_FILE_ERR_IO;
  }

  GCU_File_Temp temp;
  GCU_File_Result result =
      gcu_file_temp_create(&temp, dir, gcu_path_basename(GCU_PATH_NATIVE,
          path), allocator);
  gcu_allocator_free(allocator, dir);
  if (result != GCU_FILE_OK) {
    return result;
  }

  if (len && fwrite(data, 1, len, gcu_file_temp_stream(&temp)) != len) {
    gcu_file_temp_abort(&temp);
    return GCU_FILE_ERR_IO;
  }

  return gcu_file_temp_commit(&temp, path, sync);
}
