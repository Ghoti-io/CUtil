/*
 * SPDX-License-Identifier: LGPL-3.0-only
 *
 * Copyright (C) 2023-2026 Corey Pennycuff
 *
 * This file is part of Ghoti.io CUtil.
 *
 * Ghoti.io CUtil is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Ghoti.io CUtil is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * @file
 *
 * Reading a whole file, and replacing one without ever leaving it partly
 * written.
 *
 * The design and the reasoning behind each decision are recorded in
 * `documentation/file.md`.
 */

#ifndef GHOTI_IO_GCU_FILE_H
#define GHOTI_IO_GCU_FILE_H

#include <stdio.h>
#include <stddef.h>
#include <ghoti.io/cutil/allocator.h>
#include <ghoti.io/cutil/macros.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Passed as `max_bytes` to read a file of any size.
 */
#define GCU_FILE_UNLIMITED ((size_t)0)

/**
 * The outcome of a file operation.
 */
typedef enum GCU_File_Result {
  GCU_FILE_OK = 0,       ///< Succeeded.
  GCU_FILE_ERR_INVALID,  ///< A caller-supplied argument is wrong.
  GCU_FILE_ERR_OOM,      ///< The allocator returned NULL.
  GCU_FILE_ERR_LIMIT,    ///< The file is larger than `max_bytes`.
  GCU_FILE_ERR_IO,       ///< Open, read, write, rename or sync failed.
  GCU_FILE_RESULT_COUNT, ///< Closes the enum.  Never returned.
} GCU_File_Result;

/**
 * Name a result, for diagnostics.
 *
 * @param result The result to name.
 * @return A static string, owned by the library.  Never NULL: an
 *   out-of-range value yields `"unknown"`.
 */
GCU_API const char * gcu_file_result_string(GCU_File_Result result);

/**
 * How hard to try to get bytes onto the disk before reporting success.
 */
typedef enum GCU_File_Sync {
  /**
   * Flush the file and ask the operating system to commit it before the
   * rename, and ask it to commit the directory entry afterwards.
   *
   * This is the zero value, so a caller who does not think about it gets the
   * durable behaviour rather than the fast one.
   */
  GCU_FILE_SYNC_FULL = 0,
  /**
   * Flush, but leave the commit to the filesystem's own writeback.
   *
   * The rename is still atomic - a reader sees the old file or the new one,
   * never a mixture - but after a power loss the new one may be absent or
   * empty even though the call reported success.  Reasonable for output that
   * can simply be regenerated; wrong for anything that cannot.
   */
  GCU_FILE_SYNC_NONE,
} GCU_File_Sync;

/**
 * What permissions the finished file should carry.
 *
 * Deliberately three values and not a `mode_t`.  An access model cannot be
 * described accurately across platforms - POSIX derives a new file's
 * permissions from the process umask, Windows inherits access control entries
 * from the parent directory and has no umask at all - so this library does not
 * try to model one.  It asks the only question it can answer on both: should
 * the file be readable by anyone other than its owner?  Anything finer is the
 * caller's to do, with its own platform's own API.
 *
 * The temporary file is owner-only for its whole life regardless of this
 * setting.  The choice is applied immediately before the rename, so content is
 * never readable before it is complete.
 */
typedef enum GCU_File_Perms {
  /**
   * Only the owner may read or write it.
   *
   * This is the zero value, so a caller who does not think about it does not
   * publish anything by omission.  It is the right choice for a token, a
   * key, a session cache - anything a caller would not put in a world-
   * readable directory on purpose.
   */
  GCU_FILE_PERMS_PRIVATE = 0,
  /**
   * Whatever this platform would have given a newly created file.
   *
   * The same permissions an ordinary `fopen()` of the destination would have
   * produced, which on POSIX means the umask is honoured and a default ACL on
   * the containing directory overrides it.  This library does not compute
   * that - it asks the operating system, because the rules that combine the
   * umask with an inherited ACL are the operating system's and reproducing
   * them here would only reproduce them wrongly.
   */
  GCU_FILE_PERMS_DEFAULT,
  /**
   * The permissions the destination already has, or ::GCU_FILE_PERMS_DEFAULT
   * if it does not exist yet.
   *
   * The right choice for replacing a file rather than creating one:  a
   * configuration file somebody has deliberately narrowed should not be
   * widened by being rewritten, and one they have deliberately widened should
   * not be narrowed.  Replacing a file is not the same act as creating it,
   * and this is the value that says so.
   */
  GCU_FILE_PERMS_PRESERVE,
} GCU_File_Perms;

/**
 * Read an entire file into memory.
 *
 * The file is read in chunks rather than sized first, so it works on inputs
 * that report no size at all - pipes, character devices, and everything under
 * `/proc`.
 *
 * The buffer handed back always has a NUL one byte past `out_len`, which is
 * not counted in it.  A text caller may therefore use the result as a C
 * string without copying it, and a binary caller can ignore the byte.
 *
 * @param path The file to read.
 * @param max_bytes Refuse a file larger than this, or ::GCU_FILE_UNLIMITED
 *   for no limit.  A file that exceeds it yields ::GCU_FILE_ERR_LIMIT and
 *   nothing is allocated - the limit is a promise, not a truncation.
 * @param allocator Allocator for the buffer, or NULL for the default.
 * @param out_data Receives the buffer, owned by the caller and released with
 *   ::gcu_file_free().  Written only on success.
 * @param out_len Receives the length, excluding the added NUL.  Written only
 *   on success.
 * @return ::GCU_FILE_OK, ::GCU_FILE_ERR_INVALID, ::GCU_FILE_ERR_OOM,
 *   ::GCU_FILE_ERR_LIMIT or ::GCU_FILE_ERR_IO.
 */
GCU_API GCU_File_Result gcu_file_read(const char * path, size_t max_bytes,
  const GCU_Allocator * allocator, void ** out_data, size_t * out_len);

/**
 * Release a buffer from ::gcu_file_read().
 *
 * @param allocator The allocator the buffer came from, or NULL for the
 *   default.  It must be the same one.
 * @param data The buffer.  NULL is accepted and ignored.
 */
GCU_API void gcu_file_free(const GCU_Allocator * allocator, void * data);

/**
 * A temporary file, open and owned by the caller.
 *
 * The struct is published so that it can live on the stack.  Every field is
 * private; reach the file through ::gcu_file_temp_stream() and its name
 * through ::gcu_file_temp_path().
 */
typedef struct GCU_File_Temp {
  char * path;                     ///< Private.  The name on disk.
  FILE * stream;                   ///< Private.  Open for reading and writing.
  const GCU_Allocator * allocator; ///< Private.  Where `path` came from.
} GCU_File_Temp;

/**
 * Create and open a uniquely named temporary file.
 *
 * The name is chosen and the file created in one step that fails if the name
 * already exists, and it is opened so that only its owner may read it.  The
 * alternative - inventing a name and then opening it - is a standing
 * invitation to have something else put a symbolic link there in between.
 *
 * Where to put it:
 *
 * - For scratch space, pass `dir = NULL` and it goes wherever
 *   ::gcu_path_temp_dir() says.
 * - To replace an existing file atomically, pass the *destination's own
 *   directory*, so that the later rename stays on one filesystem and is
 *   therefore atomic.  ::gcu_file_write_atomic() does this for you.
 *
 * The handle must be disposed of exactly once, by ::gcu_file_temp_commit()
 * or ::gcu_file_temp_abort().  Both leave it zeroed, and abort accepts a
 * zeroed handle, so `abort` may be called unconditionally on a cleanup path
 * without tracking whether commit already ran.
 *
 * @param temp Receives the handle.  Zeroed first, so it is safe to abort
 *   even if this call fails.
 * @param dir Directory to create it in, or NULL for the system temporary
 *   directory.
 * @param prefix Start of the filename, or NULL for `"tmp"`.  Six random
 *   characters are appended.
 * @param allocator Allocator for the stored name, or NULL for the default.
 * @return ::GCU_FILE_OK, ::GCU_FILE_ERR_INVALID, ::GCU_FILE_ERR_OOM or
 *   ::GCU_FILE_ERR_IO.
 */
GCU_API GCU_File_Result gcu_file_temp_create(GCU_File_Temp * temp,
  const char * dir, const char * prefix, const GCU_Allocator * allocator);

/**
 * The open stream of a temporary file.
 *
 * Positioned at the start and open for both reading and writing.
 *
 * @param temp The handle.
 * @return The stream, or NULL if @p temp is NULL or already disposed of.
 *   The handle owns it; do not `fclose()` it.
 */
GCU_API FILE * gcu_file_temp_stream(const GCU_File_Temp * temp);

/**
 * The name of a temporary file on disk.
 *
 * @param temp The handle.
 * @return The path, or NULL if @p temp is NULL or already disposed of.  The
 *   handle owns it; it stops being valid when the handle is disposed of.
 */
GCU_API const char * gcu_file_temp_path(const GCU_File_Temp * temp);

/**
 * Close a temporary file and move it into place, replacing @p dest.
 *
 * A reader sees either the whole of the old file or the whole of the new one
 * and never a mixture, provided the temporary file was created in the
 * destination's own directory - a rename across filesystems is a copy, and a
 * copy is not atomic.
 *
 * On failure the temporary file is removed, @p dest is left exactly as it
 * was, and the handle is spent either way.
 *
 * With ::GCU_FILE_SYNC_FULL the file is committed before the rename; a
 * failure there is reported, because those are the bytes.  The directory
 * entry is committed afterwards on a best-effort basis and a failure there is
 * *not* reported, because several filesystems refuse the request outright and
 * failing an otherwise complete replacement over it would be worse than the
 * weaker promise.  `documentation/file.md` says what that promise is.
 *
 * The temporary file is owner-only until this call, whatever @p perms says.
 * The permissions are applied immediately before the rename, so no reader can
 * see the content until all of it is there.  A refusal to apply them fails
 * the call and leaves @p dest untouched, because handing back a file with
 * permissions other than the ones asked for is worse than not writing it.
 *
 * @param temp The handle, from ::gcu_file_temp_create().
 * @param dest The path to replace.  It need not already exist.
 * @param sync How hard to try to reach the disk.
 * @param perms What permissions the finished file should carry.
 * @return ::GCU_FILE_OK, ::GCU_FILE_ERR_INVALID or ::GCU_FILE_ERR_IO.
 */
GCU_API GCU_File_Result gcu_file_temp_commit(GCU_File_Temp * temp,
  const char * dest, GCU_File_Sync sync, GCU_File_Perms perms);

/**
 * Close and delete a temporary file.
 *
 * This is the cleanup path, and it is a named function precisely so that it
 * cannot be forgotten the way an open-coded `remove()` can: there is exactly
 * one of it, rather than one per error branch.
 *
 * @param temp The handle.  NULL, and a handle already disposed of, are
 *   accepted and ignored.
 */
GCU_API void gcu_file_temp_abort(GCU_File_Temp * temp);

/**
 * Replace a file's contents, atomically.
 *
 * Writes @p data to a temporary file in @p path's own directory and renames
 * it over @p path.  Equivalent to ::gcu_file_temp_create(),
 * ::gcu_file_temp_stream() and ::gcu_file_temp_commit() done in order, for
 * the common case where the whole content is already in memory.
 *
 * @param path The file to replace.  It need not already exist.
 * @param data The bytes to write.  May be NULL only if @p len is 0.
 * @param len How many bytes.
 * @param sync How hard to try to reach the disk.
 * @param perms What permissions the finished file should carry.  Note that
 *   the zero value is ::GCU_FILE_PERMS_PRIVATE, not the permissions a plain
 *   `fopen()` would have produced; pass ::GCU_FILE_PERMS_DEFAULT for those.
 * @param allocator Allocator for working memory, or NULL for the default.
 *   Nothing is handed back to free.
 * @return ::GCU_FILE_OK, ::GCU_FILE_ERR_INVALID, ::GCU_FILE_ERR_OOM or
 *   ::GCU_FILE_ERR_IO.
 */
GCU_API GCU_File_Result gcu_file_write_atomic(const char * path,
  const void * data, size_t len, GCU_File_Sync sync, GCU_File_Perms perms,
  const GCU_Allocator * allocator);

#ifdef __cplusplus
}
#endif

#endif // GHOTI_IO_GCU_FILE_H
