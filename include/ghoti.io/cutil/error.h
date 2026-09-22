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
 * The last OS error, and its message, without caring which OS.
 *
 * Two platforms, two unrelated error namespaces: POSIX reports through
 * `errno`, Windows through `GetLastError()`, and the codes do not correspond.
 * Anything portable that reports "why did that fail" needs the pair of them
 * behind one name, and every library in this suite would otherwise write it
 * again.
 *
 * ## The code is opaque
 *
 * gcu_error_last() returns whatever the platform uses.  **Do not compare it
 * against a constant, store it, or send it anywhere it will be interpreted
 * later** -- `2` is `ENOENT` on POSIX and `ERROR_FILE_NOT_FOUND` on Windows
 * by coincidence, and the coincidences run out immediately after that. The
 * code's only guaranteed use is being handed straight back to
 * gcu_error_string().
 *
 * ## The message goes in your buffer
 *
 * Everything here writes into a caller-supplied buffer and never allocates,
 * which is what makes it thread-safe. `strerror()` is not thread-safe;
 * `strerror_r()` has two incompatible signatures depending on feature-test
 * macros; `FormatMessageW()` allocates unless told not to. Those three
 * problems are the reason this file exists rather than a one-line macro.
 *
 * The buffer is always NUL-terminated on success, including when the message
 * had to be truncated to fit.
 *
 * ## Read it immediately
 *
 * Both `errno` and `GetLastError()` are overwritten by the *next* failing
 * call, and on Windows by some succeeding ones. Call gcu_error_last() on the
 * line after the failure, before anything else -- including anything in this
 * library.
 */

#ifndef GHOTI_IO_GCU_ERROR_H
#define GHOTI_IO_GCU_ERROR_H

#include <stddef.h>
#include <ghoti.io/cutil/macros.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The smallest buffer that is always enough.
 *
 * Any message this library can produce fits in this many bytes including the
 * terminator. A smaller buffer is allowed and truncates.
 */
#define GCU_ERROR_STRING_MAX 256

/**
 * The calling thread's last OS error code.
 *
 * `errno` on POSIX, `GetLastError()` on Windows. Opaque -- see the file
 * comment. Both are per-thread, so this reports this thread's failure and not
 * another's.
 *
 * @return The platform error code.  0 conventionally means "no error", but a
 *   successful call is not required to clear it, so only read it after a call
 *   that actually reported failure.
 */
GCU_API int gcu_error_last(void);

/**
 * Write a human-readable message for @p code into @p buffer.
 *
 * Always NUL-terminates @p buffer when it reports success, truncating if the
 * message does not fit. On failure @p buffer is set to an empty string rather
 * than left with whatever it held, so a caller that ignores the return value
 * prints nothing instead of stale text.
 *
 * @param code A code from gcu_error_last().
 * @param buffer Destination.
 * @param size Bytes available in @p buffer, including the terminator.
 * @return 0 on success, -1 if @p buffer is NULL, @p size is 0, or the
 *   platform could not describe the code.
 */
GCU_API int gcu_error_string(int code, char * buffer, size_t size);

/**
 * gcu_error_string() for the calling thread's last error.
 *
 * Equivalent to `gcu_error_string(gcu_error_last(), buffer, size)`, and
 * subject to the same warning about reading it immediately.
 *
 * @param buffer Destination.
 * @param size Bytes available in @p buffer, including the terminator.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_error_string_last(char * buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif // GHOTI_IO_GCU_ERROR_H
