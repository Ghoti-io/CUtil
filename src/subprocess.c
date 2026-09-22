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
 * Running another program.  See subprocess.h for the contract.
 *
 * The two platforms solve the same problem -- keep three streams moving at
 * once so that no order of the child's writes can wedge us -- with the tools
 * they have.  POSIX has `poll()`, so one thread watches all three.  Windows
 * anonymous pipes cannot be polled, so each stream gets a thread of its own
 * and the original thread is left free to enforce the timeout.
 */

#ifndef _WIN32
// _DEFAULT_SOURCE as well as the POSIX level, for syscall() -- see the
// close_range() call, which is worth a non-portable fast path.
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#endif

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/subprocess.h>

////////////////////////////////////////////////////////////////////////////
// Shared between the two platforms.
////////////////////////////////////////////////////////////////////////////

/// A growing byte buffer that always has room for a terminator.
typedef struct {
  char * data;
  size_t size;
  size_t capacity;
} GCU_Subprocess_Buffer;

/**
 * Append bytes, growing as needed.
 *
 * @return 0 on success, -1 if the allocation failed.
 */
static int buffer_append(GCU_Subprocess_Buffer * buffer, const char * bytes,
    size_t count) {
  if (count == 0) {
    return 0;
  }
  // The +1 is the terminator, which is maintained but not counted, so that a
  // caller who knows the output is text can use it as a string.
  if (count + 1 > SIZE_MAX - buffer->size) {
    return -1;
  }
  size_t needed = buffer->size + count + 1;
  if (needed > buffer->capacity) {
    size_t wanted = buffer->capacity ? buffer->capacity : 4096;
    while (wanted < needed) {
      if (wanted > SIZE_MAX / 2) {
        wanted = needed;
        break;
      }
      wanted *= 2;
    }
    char * grown = realloc(buffer->data, wanted);
    if (!grown) {
      return -1;
    }
    buffer->data = grown;
    buffer->capacity = wanted;
  }
  memcpy(buffer->data + buffer->size, bytes, count);
  buffer->size += count;
  buffer->data[buffer->size] = '\0';
  return 0;
}

static void buffer_release(GCU_Subprocess_Buffer * buffer) {
  free(buffer->data);
  buffer->data = NULL;
  buffer->size = 0;
  buffer->capacity = 0;
}

/// Hand a buffer's allocation to the caller's result.
static void buffer_publish(GCU_Subprocess_Buffer * buffer, char ** data,
    size_t * size) {
  *data = buffer->data;
  *size = buffer->size;
  buffer->data = NULL;
  buffer->size = 0;
  buffer->capacity = 0;
}

void gcu_subprocess_result_free(GCU_Subprocess_Result * result) {
  if (!result) {
    return;
  }
  free(result->out);
  free(result->err);
  result->out = NULL;
  result->err = NULL;
  result->out_size = 0;
  result->err_size = 0;
}

#ifdef _WIN32

////////////////////////////////////////////////////////////////////////////
// Windows
////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <ghoti.io/cutil/utf.h>

/// How long the waiting thread sleeps between checks of the output limit.
#define GCU_SUBPROCESS_SLICE_MS 25

/// What a reader or writer thread is given.
typedef struct {
  HANDLE handle;                  ///< The parent's end of one pipe.
  GCU_Subprocess_Buffer buffer;   ///< Reader only.
  const char * input;             ///< Writer only.
  size_t input_size;              ///< Writer only.
  volatile LONG * total;          ///< Bytes captured across both readers.
  size_t limit;                   ///< 0 for none.
  volatile LONG * over_limit;     ///< Set when @p limit is passed.
  bool failed;                    ///< An allocation failed.
} GCU_Subprocess_Stream;

/**
 * Append one argument to a command line the way `CommandLineToArgvW` takes
 * it apart again.
 *
 * Windows gives the child a single string and leaves the splitting to it, so
 * an argv has to be re-encoded here and decoded there.  Nearly every program
 * decodes with these rules, but a program that parses its own command line --
 * `cmd.exe` most notably -- may not, which is a difference from POSIX that no
 * wrapper can paper over.
 */
static int append_argument(GCU_Subprocess_Buffer * line, const char * argument) {
  bool needs_quotes = (*argument == '\0')
      || (strpbrk(argument, " \t\n\v\"") != NULL);
  if (!needs_quotes) {
    return buffer_append(line, argument, strlen(argument));
  }
  if (buffer_append(line, "\"", 1) != 0) {
    return -1;
  }
  for (const char * cursor = argument; ; ++cursor) {
    size_t slashes = 0;
    while (*cursor == '\\') {
      ++slashes;
      ++cursor;
    }
    // A run of backslashes is literal unless a quote follows it, in which
    // case each one has to be doubled and the quote escaped.  The run before
    // the closing quote counts as being followed by one.
    size_t emit = (*cursor == '\0' || *cursor == '"') ? slashes * 2 : slashes;
    for (size_t i = 0; i < emit; ++i) {
      if (buffer_append(line, "\\", 1) != 0) {
        return -1;
      }
    }
    if (*cursor == '\0') {
      break;
    }
    if (*cursor == '"' && buffer_append(line, "\\", 1) != 0) {
      return -1;
    }
    if (buffer_append(line, cursor, 1) != 0) {
      return -1;
    }
  }
  return buffer_append(line, "\"", 1);
}

/// Build the whole command line, UTF-16, from an argv.
static GCU_Char16 * build_command_line(const char * const * argv) {
  GCU_Subprocess_Buffer line = {NULL, 0, 0};
  for (size_t i = 0; argv[i]; ++i) {
    if (i && buffer_append(&line, " ", 1) != 0) {
      buffer_release(&line);
      return NULL;
    }
    if (append_argument(&line, argv[i]) != 0) {
      buffer_release(&line);
      return NULL;
    }
  }
  const char * text = line.data ? line.data : "";
  size_t units = gcu_utf8_to_utf16(text, NULL, 0);
  if (!units) {
    buffer_release(&line);
    return NULL;
  }
  GCU_Char16 * wide = malloc(units * sizeof(GCU_Char16));
  if (wide) {
    gcu_utf8_to_utf16(text, wide, units);
  }
  buffer_release(&line);
  return wide;
}

/**
 * Build the environment block: every entry, each NUL-terminated, with a
 * second NUL closing the list.
 *
 * Built one entry at a time because the conversion stops at a NUL, so the
 * block cannot be assembled in UTF-8 and converted in one go.
 */
static GCU_Char16 * build_environment(const char * const * entries) {
  size_t units = 1; // The closing NUL.
  for (size_t i = 0; entries[i]; ++i) {
    size_t entry = gcu_utf8_to_utf16(entries[i], NULL, 0);
    if (!entry || entry > SIZE_MAX - units) {
      return NULL;
    }
    units += entry;
  }
  GCU_Char16 * block = malloc(units * sizeof(GCU_Char16));
  if (!block) {
    return NULL;
  }
  size_t at = 0;
  for (size_t i = 0; entries[i]; ++i) {
    size_t entry = gcu_utf8_to_utf16(entries[i], block + at, units - at);
    at += entry; // Includes that entry's own terminator.
  }
  block[at] = 0;
  return block;
}

static GCU_Char16 * widen(const char * text) {
  size_t units = gcu_utf8_to_utf16(text, NULL, 0);
  if (!units) {
    return NULL;
  }
  GCU_Char16 * wide = malloc(units * sizeof(GCU_Char16));
  if (wide) {
    gcu_utf8_to_utf16(text, wide, units);
  }
  return wide;
}

static DWORD WINAPI reader_thread(LPVOID argument) {
  GCU_Subprocess_Stream * stream = (GCU_Subprocess_Stream *)argument;
  char chunk[65536];
  for (;;) {
    DWORD got = 0;
    if (!ReadFile(stream->handle, chunk, (DWORD)sizeof(chunk), &got, NULL)
        || got == 0) {
      // A closed pipe reports ERROR_BROKEN_PIPE, which is this stream's
      // end-of-file and not a failure.
      break;
    }
    if (buffer_append(&stream->buffer, chunk, (size_t)got) != 0) {
      stream->failed = true;
      break;
    }
    if (stream->limit) {
      LONG total = InterlockedExchangeAdd(stream->total, (LONG)got) + (LONG)got;
      if ((size_t)total >= stream->limit) {
        InterlockedExchange(stream->over_limit, 1);
      }
    }
  }
  return 0;
}

static DWORD WINAPI writer_thread(LPVOID argument) {
  GCU_Subprocess_Stream * stream = (GCU_Subprocess_Stream *)argument;
  size_t written = 0;
  while (written < stream->input_size) {
    size_t remaining = stream->input_size - written;
    DWORD chunk = remaining > 65536u ? 65536u : (DWORD)remaining;
    DWORD put = 0;
    if (!WriteFile(stream->handle, stream->input + written, chunk, &put, NULL)
        || put == 0) {
      // The child closed its end, or was killed.  Not an error: a program is
      // allowed to ignore its input.
      break;
    }
    written += (size_t)put;
  }
  // Closing is what gives the child end-of-file, so it happens here rather
  // than in the waiting thread, which may still be waiting.
  CloseHandle(stream->handle);
  stream->handle = INVALID_HANDLE_VALUE;
  return 0;
}

int gcu_subprocess_run(const GCU_Subprocess_Options * options,
    GCU_Subprocess_Result * result) {
  if (!result) {
    return -1;
  }
  memset(result, 0, sizeof(*result));
  if (!options || !options->argv || !options->argv[0]) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return -1;
  }

  GCU_Char16 * command = NULL;
  GCU_Char16 * environment = NULL;
  GCU_Char16 * directory = NULL;
  HANDLE in_read = INVALID_HANDLE_VALUE, in_write = INVALID_HANDLE_VALUE;
  HANDLE out_read = INVALID_HANDLE_VALUE, out_write = INVALID_HANDLE_VALUE;
  HANDLE err_read = INVALID_HANDLE_VALUE, err_write = INVALID_HANDLE_VALUE;
  HANDLE threads[3] = {NULL, NULL, NULL};
  PROCESS_INFORMATION process;
  memset(&process, 0, sizeof(process));
  volatile LONG total = 0;
  volatile LONG over_limit = 0;
  GCU_Subprocess_Stream out_stream;
  GCU_Subprocess_Stream err_stream;
  GCU_Subprocess_Stream in_stream;
  memset(&out_stream, 0, sizeof(out_stream));
  memset(&err_stream, 0, sizeof(err_stream));
  memset(&in_stream, 0, sizeof(in_stream));
  in_stream.handle = INVALID_HANDLE_VALUE;
  int returning = -1;

  command = build_command_line(options->argv);
  if (!command) {
    goto done;
  }
  if (options->environment) {
    environment = build_environment(options->environment);
    if (!environment) {
      goto done;
    }
  }
  if (options->directory) {
    directory = widen(options->directory);
    if (!directory) {
      goto done;
    }
  }

  SECURITY_ATTRIBUTES inheritable;
  inheritable.nLength = sizeof(inheritable);
  inheritable.lpSecurityDescriptor = NULL;
  inheritable.bInheritHandle = TRUE;

  if (!CreatePipe(&in_read, &in_write, &inheritable, 0)
      || !CreatePipe(&out_read, &out_write, &inheritable, 0)
      || !CreatePipe(&err_read, &err_write, &inheritable, 0)) {
    goto done;
  }
  // The parent's ends must not be inherited, or the child holds a copy of
  // every write end open and the reads never see end-of-file.
  if (!SetHandleInformation(in_write, HANDLE_FLAG_INHERIT, 0)
      || !SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0)
      || !SetHandleInformation(err_read, HANDLE_FLAG_INHERIT, 0)) {
    goto done;
  }

  STARTUPINFOW startup;
  memset(&startup, 0, sizeof(startup));
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = in_read;
  startup.hStdOutput = out_write;
  startup.hStdError = options->merge_stderr ? out_write : err_write;

  // bInheritHandles is TRUE, but only handles explicitly marked inheritable
  // travel -- which on Windows is the three above and nothing else, because
  // a handle is not inheritable unless it was created that way.
  if (!CreateProcessW(NULL, (LPWSTR)command, NULL, NULL, TRUE,
      CREATE_UNICODE_ENVIRONMENT, environment, (LPCWSTR)directory,
      &startup, &process)) {
    DWORD why = GetLastError();
    returning = GCU_SUBPROCESS_NOT_STARTED;
    SetLastError(why);
    goto done;
  }

  CloseHandle(in_read);
  in_read = INVALID_HANDLE_VALUE;
  CloseHandle(out_write);
  out_write = INVALID_HANDLE_VALUE;
  CloseHandle(err_write);
  err_write = INVALID_HANDLE_VALUE;

  out_stream.handle = out_read;
  out_stream.total = &total;
  out_stream.limit = options->output_limit;
  out_stream.over_limit = &over_limit;
  err_stream.handle = err_read;
  err_stream.total = &total;
  err_stream.limit = options->output_limit;
  err_stream.over_limit = &over_limit;
  // The writing thread takes ownership of this end, because closing it is
  // what gives the child end-of-file and that cannot wait for a thread that
  // is still waiting for the process.
  in_stream.handle = in_write;
  in_write = INVALID_HANDLE_VALUE;
  in_stream.input = options->input;
  in_stream.input_size = options->input ? options->input_size : 0;

  threads[0] = CreateThread(NULL, 0, reader_thread, &out_stream, 0, NULL);
  threads[1] = CreateThread(NULL, 0, reader_thread, &err_stream, 0, NULL);
  threads[2] = CreateThread(NULL, 0, writer_thread, &in_stream, 0, NULL);
  if (!threads[0] || !threads[1] || !threads[2]) {
    TerminateProcess(process.hProcess, 1);
  }

  GCU_Subprocess_Outcome forced = GCU_SUBPROCESS_EXITED;
  bool was_forced = false;
  ULONGLONG started = GetTickCount64();
  for (;;) {
    DWORD slice = GCU_SUBPROCESS_SLICE_MS;
    if (options->timeout <= 0 && !options->output_limit) {
      slice = INFINITE;
    }
    if (WaitForSingleObject(process.hProcess, slice) == WAIT_OBJECT_0) {
      break;
    }
    if (options->output_limit && over_limit) {
      forced = GCU_SUBPROCESS_OUTPUT_LIMIT;
      was_forced = true;
    }
    else if (options->timeout > 0
        && GetTickCount64() - started >= (ULONGLONG)options->timeout) {
      forced = GCU_SUBPROCESS_TIMED_OUT;
      was_forced = true;
    }
    if (was_forced) {
      TerminateProcess(process.hProcess, 1);
      WaitForSingleObject(process.hProcess, INFINITE);
      break;
    }
  }

  // The child is gone, so every pipe end it held is closed and each thread
  // is about to see end-of-file.
  for (int i = 0; i < 3; ++i) {
    if (threads[i]) {
      WaitForSingleObject(threads[i], INFINITE);
    }
  }

  if (out_stream.failed || err_stream.failed) {
    goto done;
  }

  DWORD code = 0;
  GetExitCodeProcess(process.hProcess, &code);
  result->outcome = was_forced ? forced : GCU_SUBPROCESS_EXITED;
  result->exit_code = (int)code;
  buffer_publish(&out_stream.buffer, &result->out, &result->out_size);
  buffer_publish(&err_stream.buffer, &result->err, &result->err_size);
  returning = 0;

done:
  buffer_release(&out_stream.buffer);
  buffer_release(&err_stream.buffer);
  for (int i = 0; i < 3; ++i) {
    if (threads[i]) {
      CloseHandle(threads[i]);
    }
  }
  if (process.hProcess) {
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
  }
  if (in_read != INVALID_HANDLE_VALUE) {
    CloseHandle(in_read);
  }
  if (in_write != INVALID_HANDLE_VALUE) {
    CloseHandle(in_write);
  }
  if (in_stream.handle != INVALID_HANDLE_VALUE) {
    CloseHandle(in_stream.handle);
  }
  if (out_read != INVALID_HANDLE_VALUE) {
    CloseHandle(out_read);
  }
  if (out_write != INVALID_HANDLE_VALUE) {
    CloseHandle(out_write);
  }
  if (err_read != INVALID_HANDLE_VALUE) {
    CloseHandle(err_read);
  }
  if (err_write != INVALID_HANDLE_VALUE) {
    CloseHandle(err_write);
  }
  free(command);
  free(environment);
  free(directory);
  return returning;
}

#else

////////////////////////////////////////////////////////////////////////////
// POSIX
////////////////////////////////////////////////////////////////////////////

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char ** environ;

/// How much is moved per readable or writable event.
#define GCU_SUBPROCESS_CHUNK 65536

static int set_close_on_exec(int descriptor) {
  int flags = fcntl(descriptor, F_GETFD);
  if (flags < 0) {
    return -1;
  }
  return fcntl(descriptor, F_SETFD, flags | FD_CLOEXEC);
}

static int set_non_blocking(int descriptor) {
  int flags = fcntl(descriptor, F_GETFL);
  if (flags < 0) {
    return -1;
  }
  return fcntl(descriptor, F_SETFL, flags | O_NONBLOCK);
}

/**
 * A pipe whose ends are both close-on-exec.
 *
 * Close-on-exec by default and opened up deliberately, rather than the other
 * way round: the three the child is meant to have are made inheritable by
 * `dup2()`, which clears the flag, and everything else then stays behind
 * without anyone having to remember it.
 */
static int make_pipe(int ends[2]) {
  if (pipe(ends) != 0) {
    return -1;
  }
  if (set_close_on_exec(ends[0]) != 0 || set_close_on_exec(ends[1]) != 0) {
    close(ends[0]);
    close(ends[1]);
    ends[0] = ends[1] = -1;
    return -1;
  }
  return 0;
}

static void close_if_open(int * descriptor) {
  if (*descriptor >= 0) {
    close(*descriptor);
    *descriptor = -1;
  }
}

/**
 * Move a descriptor out of the way of 0, 1 and 2.
 *
 * A pipe end that landed on one of them would be overwritten by the `dup2()`
 * that puts another one there.  It can only happen when the caller has closed
 * one of its own standard descriptors, which is rare enough that a bug here
 * would be very hard to find and cheap enough to rule out.
 */
static int move_above_standard(int * descriptor) {
  if (*descriptor >= 3) {
    return 0;
  }
  int moved = fcntl(*descriptor, F_DUPFD_CLOEXEC, 3);
  if (moved < 0) {
    return -1;
  }
  close(*descriptor);
  *descriptor = moved;
  return 0;
}

/**
 * Close every descriptor above 2 except one.
 *
 * Runs in the forked child, so everything here has to be async-signal-safe:
 * `close()`, `syscall()` and arithmetic are, and reading `/proc/self/fd`
 * would not be, because opening a directory allocates.
 */
static void close_inherited_descriptors(int keep, int ceiling) {
#if defined(__linux__) && defined(SYS_close_range)
  // One syscall instead of a close() for every number up to the limit, which
  // on a process with a large RLIMIT_NOFILE is the difference between
  // microseconds and a tenth of a second per launch.
  long below = (keep > 3)
      ? syscall(SYS_close_range, 3u, (unsigned)(keep - 1), 0u) : 0;
  long above = syscall(SYS_close_range, (unsigned)(keep + 1), ~0u, 0u);
  if (below == 0 && above == 0) {
    return;
  }
#endif
  for (int descriptor = 3; descriptor < ceiling; ++descriptor) {
    if (descriptor != keep) {
      close(descriptor);
    }
  }
}

/**
 * Become the child: rearrange the descriptors and exec.  Never returns.
 *
 * Everything between `fork()` and `exec()` must be async-signal-safe, because
 * in a threaded parent the child holds locks that no thread is left to
 * release -- so there is no allocation here, and the argument and environment
 * arrays were built before the fork.
 */
static void child_exec(const GCU_Subprocess_Options * options, int in_read,
    int out_write, int err_write, int fail_write, int ceiling) {
  int failure = 0;
  if (dup2(in_read, STDIN_FILENO) < 0
      || dup2(out_write, STDOUT_FILENO) < 0
      || dup2(err_write, STDERR_FILENO) < 0) {
    failure = errno;
  }
  if (!failure) {
    close_inherited_descriptors(fail_write, ceiling);
    if (options->directory && chdir(options->directory) != 0) {
      failure = errno;
    }
  }
  if (!failure) {
    if (options->environment) {
      // Assigning environ before execvp is what execvpe would do: the child
      // gets this environment, and the PATH search uses this PATH.
      environ = (char **)options->environment;
    }
    execvp(options->argv[0], (char * const *)options->argv);
    failure = errno;
  }
  // The parent is reading this pipe.  Its write end is close-on-exec, so
  // silence means the exec happened; a number means it did not, and says
  // why.
  ssize_t ignored = write(fail_write, &failure, sizeof(failure));
  (void)ignored;
  // _exit, not exit: this is a forked copy of a process that may have
  // registered handlers -- gtest's, or a sanitizer's -- which must not run
  // twice.
  _exit(127);
}

static void deadline_from_now(struct timespec * deadline, int milliseconds) {
  clock_gettime(CLOCK_MONOTONIC, deadline);
  deadline->tv_sec += milliseconds / 1000;
  deadline->tv_nsec += (long)(milliseconds % 1000) * 1000000L;
  if (deadline->tv_nsec >= 1000000000L) {
    deadline->tv_sec += 1;
    deadline->tv_nsec -= 1000000000L;
  }
}

static int milliseconds_until(const struct timespec * deadline) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  long long remaining = (long long)(deadline->tv_sec - now.tv_sec) * 1000LL
      + (deadline->tv_nsec - now.tv_nsec) / 1000000LL;
  if (remaining < 0) {
    return 0;
  }
  return remaining > INT_MAX ? INT_MAX : (int)remaining;
}

/**
 * Keep SIGPIPE off this thread for as long as we are writing to the child.
 *
 * Writing to a pipe whose reader has gone raises SIGPIPE, whose default
 * action is to kill the process -- so a child that exits without reading its
 * input would take the *parent* down with it, which is not a failure mode a
 * caller can defend against.  Blocking is per-thread and reversible;
 * `signal(SIGPIPE, SIG_IGN)` would be neither, and would change a setting the
 * whole program shares.
 *
 * Done after the fork, so that the child does not inherit the blocked mask.
 */
static bool block_sigpipe(sigset_t * saved) {
  sigset_t pipe_only;
  sigemptyset(&pipe_only);
  sigaddset(&pipe_only, SIGPIPE);
  return pthread_sigmask(SIG_BLOCK, &pipe_only, saved) == 0;
}

static void restore_sigpipe(const sigset_t * saved) {
  if (!sigismember(saved, SIGPIPE)) {
    // It was not blocked before, so anything pending now is ours and would be
    // delivered the moment the mask is restored.  Take it off the queue.
    sigset_t pipe_only;
    sigemptyset(&pipe_only);
    sigaddset(&pipe_only, SIGPIPE);
    struct timespec immediately = {0, 0};
    while (sigtimedwait(&pipe_only, NULL, &immediately) < 0
        && errno == EINTR) {
      // Retry.
    }
  }
  pthread_sigmask(SIG_SETMASK, saved, NULL);
}

int gcu_subprocess_run(const GCU_Subprocess_Options * options,
    GCU_Subprocess_Result * result) {
  if (!result) {
    return -1;
  }
  memset(result, 0, sizeof(*result));
  if (!options || !options->argv || !options->argv[0]) {
    errno = EINVAL;
    return -1;
  }

  int in[2] = {-1, -1};
  int out[2] = {-1, -1};
  int err[2] = {-1, -1};
  int fail[2] = {-1, -1};
  GCU_Subprocess_Buffer out_buffer = {NULL, 0, 0};
  GCU_Subprocess_Buffer err_buffer = {NULL, 0, 0};
  pid_t child = -1;
  int returning = -1;
  int saved_errno = 0;

  if (make_pipe(in) != 0 || make_pipe(out) != 0 || make_pipe(err) != 0
      || make_pipe(fail) != 0) {
    saved_errno = errno;
    goto done;
  }
  if (move_above_standard(&in[0]) != 0 || move_above_standard(&out[1]) != 0
      || move_above_standard(&err[1]) != 0
      || move_above_standard(&fail[1]) != 0) {
    saved_errno = errno;
    goto done;
  }

  // Read before the fork: getrlimit is not async-signal-safe.
  int ceiling = 4096;
  struct rlimit descriptors;
  if (getrlimit(RLIMIT_NOFILE, &descriptors) == 0
      && descriptors.rlim_cur != RLIM_INFINITY
      && descriptors.rlim_cur < (rlim_t)INT_MAX) {
    ceiling = (int)descriptors.rlim_cur;
  }

  child = fork();
  if (child < 0) {
    saved_errno = errno;
    goto done;
  }
  if (child == 0) {
    child_exec(options, in[0], out[1],
        options->merge_stderr ? out[1] : err[1], fail[1], ceiling);
    // child_exec does not return.
  }

  close_if_open(&in[0]);
  close_if_open(&out[1]);
  close_if_open(&err[1]);
  close_if_open(&fail[1]);

  // Did the exec happen?  Four bytes means it did not; end-of-file means the
  // child's image was replaced and the close-on-exec flag shut this pipe.
  {
    int failure = 0;
    size_t got = 0;
    while (got < sizeof(failure)) {
      ssize_t taken = read(fail[0], (char *)&failure + got,
          sizeof(failure) - got);
      if (taken == 0) {
        break;
      }
      if (taken < 0) {
        if (errno == EINTR) {
          continue;
        }
        break;
      }
      got += (size_t)taken;
    }
    if (got == sizeof(failure)) {
      int status = 0;
      while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
        // Retry.
      }
      child = -1;
      saved_errno = failure;
      returning = GCU_SUBPROCESS_NOT_STARTED;
      goto done;
    }
  }
  close_if_open(&fail[0]);

  bool feeding = options->input && options->input_size > 0;
  if (!feeding) {
    // Closing is what gives the child end-of-file on its input.
    close_if_open(&in[1]);
  }
  if (set_non_blocking(out[0]) != 0 || set_non_blocking(err[0]) != 0
      || (feeding && set_non_blocking(in[1]) != 0)) {
    saved_errno = errno;
    goto done;
  }
  if (options->merge_stderr) {
    close_if_open(&err[0]);
  }

  sigset_t saved_mask;
  bool masked = feeding && block_sigpipe(&saved_mask);

  struct timespec deadline;
  bool timed = options->timeout > 0;
  if (timed) {
    deadline_from_now(&deadline, options->timeout);
  }

  GCU_Subprocess_Outcome forced = GCU_SUBPROCESS_EXITED;
  bool was_forced = false;
  bool broke = false;
  size_t written = 0;

  while (in[1] >= 0 || out[0] >= 0 || err[0] >= 0) {
    struct pollfd watched[3];
    int count = 0;
    int in_slot = -1, out_slot = -1, err_slot = -1;
    if (in[1] >= 0) {
      watched[count].fd = in[1];
      watched[count].events = POLLOUT;
      watched[count].revents = 0;
      in_slot = count++;
    }
    if (out[0] >= 0) {
      watched[count].fd = out[0];
      watched[count].events = POLLIN;
      watched[count].revents = 0;
      out_slot = count++;
    }
    if (err[0] >= 0) {
      watched[count].fd = err[0];
      watched[count].events = POLLIN;
      watched[count].revents = 0;
      err_slot = count++;
    }

    int wait_for = -1;
    if (timed) {
      wait_for = milliseconds_until(&deadline);
      if (wait_for == 0) {
        forced = GCU_SUBPROCESS_TIMED_OUT;
        was_forced = true;
        break;
      }
    }

    int ready = poll(watched, (nfds_t)count, wait_for);
    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }
      saved_errno = errno;
      broke = true;
      break;
    }
    if (ready == 0) {
      forced = GCU_SUBPROCESS_TIMED_OUT;
      was_forced = true;
      break;
    }

    if (in_slot >= 0 && watched[in_slot].revents) {
      size_t remaining = options->input_size - written;
      size_t chunk = remaining > GCU_SUBPROCESS_CHUNK
          ? (size_t)GCU_SUBPROCESS_CHUNK : remaining;
      ssize_t put = write(in[1], options->input + written, chunk);
      if (put < 0) {
        if (errno != EINTR && errno != EAGAIN) {
          // EPIPE, normally: the child exited, or closed its input, without
          // reading what it was given.  Not a failure -- a program is allowed
          // to ignore its input.
          //
          // Learned from the failed write rather than from a POLLERR test
          // beforehand, and that is deliberate twice over.  Such a test
          // cannot cover the case, because the reader can go between the poll
          // and the write; and it would mean the write that raises SIGPIPE
          // almost never happens, so the mask that swallows it would go
          // untested while looking well covered.
          close_if_open(&in[1]);
        }
      }
      else {
        written += (size_t)put;
        if (written >= options->input_size) {
          close_if_open(&in[1]);
        }
      }
    }

    struct {
      int slot;
      int * descriptor;
      GCU_Subprocess_Buffer * buffer;
    } readable[2] = {
      {out_slot, &out[0], &out_buffer},
      {err_slot, &err[0], &err_buffer},
    };
    for (int i = 0; i < 2 && !broke; ++i) {
      if (readable[i].slot < 0 || !watched[readable[i].slot].revents) {
        continue;
      }
      char chunk[GCU_SUBPROCESS_CHUNK];
      ssize_t taken = read(*readable[i].descriptor, chunk, sizeof(chunk));
      if (taken > 0) {
        if (buffer_append(readable[i].buffer, chunk, (size_t)taken) != 0) {
          saved_errno = ENOMEM;
          broke = true;
        }
      }
      else if (taken == 0) {
        close_if_open(readable[i].descriptor);
      }
      else if (errno != EINTR && errno != EAGAIN) {
        // Treat a read error as the end of that stream; whatever went wrong
        // with the pipe, there is nothing more coming through it.
        close_if_open(readable[i].descriptor);
      }
    }
    if (broke) {
      break;
    }

    if (options->output_limit
        && out_buffer.size + err_buffer.size >= options->output_limit) {
      forced = GCU_SUBPROCESS_OUTPUT_LIMIT;
      was_forced = true;
      break;
    }
  }

  if (masked) {
    restore_sigpipe(&saved_mask);
  }

  // Stop reading as well as stop the child.  A child that started something
  // else leaves that something holding the pipes open, and waiting for them
  // after deciding to give up would be waiting for the thing we gave up on.
  close_if_open(&in[1]);
  close_if_open(&out[0]);
  close_if_open(&err[0]);

  int status = 0;
  bool reaped = false;

  if (!was_forced && !broke && timed) {
    // The streams can close before the child does -- it may close them
    // itself, or hand them to something that outlives it -- so the timeout
    // has to cover the wait as well as the reading, or it is not a bound on
    // this call at all.  Polled, because there is no portable way to wait
    // for a child and a clock at once without taking over SIGCHLD, which
    // belongs to the program and not to a library inside it.
    for (;;) {
      pid_t answer = waitpid(child, &status, WNOHANG);
      if (answer == child) {
        reaped = true;
        break;
      }
      if (answer < 0) {
        if (errno == EINTR) {
          continue;
        }
        saved_errno = errno;
        broke = true;
        break;
      }
      int left = milliseconds_until(&deadline);
      if (left == 0) {
        forced = GCU_SUBPROCESS_TIMED_OUT;
        was_forced = true;
        break;
      }
      struct timespec nap = {0, (left < 5 ? left : 5) * 1000000L};
      nanosleep(&nap, NULL);
    }
  }

  if (was_forced || broke) {
    kill(child, SIGKILL);
  }

  while (!reaped && waitpid(child, &status, 0) < 0) {
    if (errno != EINTR) {
      saved_errno = errno;
      broke = true;
      break;
    }
  }
  child = -1;
  if (broke) {
    goto done;
  }

  if (WIFEXITED(status)) {
    result->outcome = GCU_SUBPROCESS_EXITED;
    result->exit_code = WEXITSTATUS(status);
  }
  else if (WIFSIGNALED(status)) {
    result->outcome = GCU_SUBPROCESS_SIGNALED;
    result->signal = WTERMSIG(status);
  }
  if (was_forced) {
    // The signal stays -- it says how the child died -- but the outcome says
    // whose decision that was, which is what a caller needs to know.
    result->outcome = forced;
  }
  buffer_publish(&out_buffer, &result->out, &result->out_size);
  buffer_publish(&err_buffer, &result->err, &result->err_size);
  returning = 0;

done:
  buffer_release(&out_buffer);
  buffer_release(&err_buffer);
  if (child > 0) {
    int status_ignored = 0;
    kill(child, SIGKILL);
    while (waitpid(child, &status_ignored, 0) < 0 && errno == EINTR) {
      // Retry.
    }
  }
  close_if_open(&in[0]);
  close_if_open(&in[1]);
  close_if_open(&out[0]);
  close_if_open(&out[1]);
  close_if_open(&err[0]);
  close_if_open(&err[1]);
  close_if_open(&fail[0]);
  close_if_open(&fail[1]);
  if (saved_errno) {
    errno = saved_errno;
  }
  return returning;
}

#endif
