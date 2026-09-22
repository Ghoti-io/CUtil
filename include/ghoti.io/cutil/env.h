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
 * Environment variables, in UTF-8, on both platforms.
 *
 * Names and values are UTF-8 here regardless of platform. On Windows the
 * environment is natively UTF-16 and the `A`-suffixed API converts through
 * the process's ANSI code page, which is not UTF-8 by default and cannot
 * represent most of it -- so a value containing any non-ASCII character comes
 * back mangled or truncated from `getenv()` there. This module uses the `W`
 * API and converts through utf.h, which is why that module was written first.
 *
 * ## Reading follows the same shape as utf.h
 *
 * gcu_env_get() returns the bytes the value needs including the terminator,
 * and 0 when the variable is not set. Pass a NULL destination to measure.
 * A destination too small has nothing written to it, so a caller cannot
 * mistake a truncated value for a whole one -- which for a PATH or a
 * directory name is the difference between failing and using the wrong place.
 *
 * ## These functions are not thread-safe against each other
 *
 * Not a defect here; it is the interface POSIX specifies. `getenv()` returns
 * a pointer into the environment block, and a concurrent `setenv()` may free
 * it. This module copies the value out before returning, which closes the
 * window it can close, but a `gcu_env_set()` racing a `gcu_env_get()` in
 * another thread is still undefined.
 *
 * The practical rule is the one the C library has always implied: **set the
 * environment during start-up, from one thread, before anything else runs.**
 * If you need mutable per-thread configuration after that, the environment is
 * the wrong place for it -- see tls.h.
 *
 * ## Names
 *
 * A name may not be empty and may not contain `=`; both are rejected rather
 * than passed through, because `putenv`-style interfaces would interpret the
 * `=` and silently set a different variable than the caller named.
 */

#ifndef GHOTI_IO_GCU_ENV_H
#define GHOTI_IO_GCU_ENV_H

#include <stdbool.h>
#include <stddef.h>
#include <ghoti.io/cutil/macros.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Read an environment variable as UTF-8.
 *
 * @param name The variable's name, UTF-8, non-empty, without `=`.
 * @param buffer Destination, or NULL to measure only.
 * @param size Bytes available in @p buffer, including the terminator.
 * @return Bytes required including the terminator, or 0 if @p name is
 *   invalid or the variable is not set.  Nothing is written unless @p buffer
 *   is non-NULL and @p size is at least the returned value.
 */
GCU_API size_t gcu_env_get(const char * name, char * buffer, size_t size);

/**
 * Whether a variable is set, including when its value is the empty string.
 *
 * gcu_env_get() returning 1 -- just a terminator -- and returning 0 are
 * different answers: the first is a variable set to "", the second is a
 * variable that does not exist.  This asks the question directly so the
 * distinction is not lost in an off-by-one.
 *
 * @param name The variable's name.
 * @return true if set, false if not set or @p name is invalid.
 */
GCU_API bool gcu_env_has(const char * name);

/**
 * Set a variable, replacing any current value.
 *
 * @param name The variable's name, UTF-8, non-empty, without `=`.
 * @param value The value, UTF-8.  May be empty; may not be NULL -- use
 *   gcu_env_unset() to remove a variable, so that the two intentions cannot
 *   be confused at a call site.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_env_set(const char * name, const char * value);

/**
 * Remove a variable.  Removing one that is not set succeeds.
 *
 * @param name The variable's name.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_env_unset(const char * name);

#ifdef __cplusplus
}
#endif

#endif // GHOTI_IO_GCU_ENV_H
