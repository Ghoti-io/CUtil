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
 * Loading a shared library at run time and finding symbols in it.
 *
 * `dlopen`/`dlsym` on POSIX, `LoadLibraryW`/`GetProcAddress` on Windows, with
 * the path taken as UTF-8 on both.
 *
 * ## The symbol comes back as a function pointer
 *
 * gcu_library_symbol() returns a `GCU_Library_Function`, not a `void *`.
 *
 * `dlsym` returns `void *`, and converting an object pointer to a function
 * pointer is not defined by C -- it works on every platform anyone ships on,
 * which is why POSIX requires it, but it is still a conversion the standard
 * declines to describe. Returning `void *` here would push that conversion to
 * every call site; returning a function pointer keeps it in one place, inside
 * a union, in this library. The caller's cast from one *function* pointer
 * type to another is ordinary, defined C.
 *
 *     typedef int (*adder)(int, int);
 *     adder add = (adder)gcu_library_symbol(lib, "add");
 *
 * A symbol whose value is legitimately NULL cannot be distinguished from one
 * that is absent. Call gcu_library_error() to tell them apart.
 *
 * ## Names and paths
 *
 * Nothing here guesses at a filename. A caller wanting `libfoo.so` on Linux
 * and `foo.dll` on Windows chooses that itself, because the correct answer
 * depends on how the thing being loaded was built and named -- a library that
 * guessed would be wrong for plugins, wrong for versioned sonames, and
 * silently right often enough to be trusted.
 *
 * ## Errors
 *
 * gcu_library_error() reports the last failure **from this module**, not the
 * process's last OS error. On POSIX that is `dlerror()`, which lives in a
 * namespace of its own and is not `errno`, which is why error.h cannot serve
 * here. Like `dlerror()`, reading it clears it.
 */

#ifndef GHOTI_IO_GCU_LIBRARY_H
#define GHOTI_IO_GCU_LIBRARY_H

#include <stddef.h>
#include <ghoti.io/cutil/macros.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DOXYGEN
/** A handle to a loaded shared library. */
typedef void * GCU_Library;
#endif // DOXYGEN

#ifdef _WIN32
#include <windows.h>
typedef HMODULE GCU_Library;
#else
typedef void * GCU_Library;
#endif

/**
 * A pointer to a function of unknown signature.
 *
 * Cast it to the real signature before calling.  See the file comment for why
 * symbols come back as this rather than as `void *`.
 */
typedef void (*GCU_Library_Function)(void);

/**
 * Load a shared library.
 *
 * @param library Receives the handle on success; untouched on failure.
 * @param path Path to the library, UTF-8.  Interpreted by the platform
 *   loader, so its search rules apply to a bare filename.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_library_open(GCU_Library * library, const char * path);

/**
 * Unload a shared library.
 *
 * Every pointer obtained from it becomes invalid, including any still being
 * executed -- so this belongs after the last call into it, not merely after
 * the last reference to it.
 *
 * @param library Pointer to the handle to close.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_library_close(GCU_Library * library);

/**
 * Find a symbol.
 *
 * @param library A handle from gcu_library_open().
 * @param name The symbol's name, exactly as exported.
 * @return The symbol, or NULL if it was not found.
 */
GCU_API GCU_Library_Function gcu_library_symbol(GCU_Library library,
    const char * name);

/**
 * Describe the last failure from this module, and clear it.
 *
 * @param buffer Destination; always NUL-terminated on success, truncating if
 *   needed.  Emptied rather than left stale when there is nothing to report.
 * @param size Bytes available, including the terminator.
 * @return 0 if a message was written, -1 if there was no error to report or
 *   @p buffer is unusable.
 */
GCU_API int gcu_library_error(char * buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif // GHOTI_IO_GCU_LIBRARY_H
