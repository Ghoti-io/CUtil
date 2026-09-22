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
 * Conversion between UTF-8 and UTF-16, and validation of both.
 *
 * This library treats text as UTF-8 everywhere, which is the right choice on
 * POSIX and not a choice Windows offers: its `W` APIs take UTF-16, and its
 * `A` APIs take the process's ANSI code page, which is not UTF-8 by default
 * and cannot represent most of it. `file.c`, `path.c` and `dir.c` already do
 * the right thing privately -- they call `_wfopen`, `CreateFileW` and
 * `FindFirstFileW` -- but the conversion was not exported, so a consumer
 * calling any Windows API of their own had to write it again.
 *
 * ## One implementation, not two
 *
 * There is no `#ifdef _WIN32` in this module. `MultiByteToWideChar` would
 * have done the Windows half, and then the two platforms would disagree
 * about every edge case -- which malformed sequences are rejected, whether a
 * lone surrogate survives a round trip -- and only one of the two arms would
 * ever be compiled here. A hand-written converter is about a hundred lines,
 * behaves identically everywhere, and is testable on the machine you are
 * sitting at. On Windows a `char16_t *` and a `wchar_t *` are the same
 * sixteen-bit code units, so the result feeds a `W` API after a cast.
 *
 * ## Strict, and why
 *
 * Both directions **reject** malformed input rather than substituting
 * U+FFFD:
 *
 *   - overlong encodings (`C0 80` for NUL, and longer forms generally)
 *   - surrogate code points encoded in UTF-8 (`ED A0 80`, sometimes called
 *     CESU-8 or WTF-8)
 *   - anything above U+10FFFF
 *   - truncated or orphaned continuation bytes
 *   - unpaired surrogates in UTF-16
 *
 * Substitution is right for a text renderer and wrong here. These conversions
 * exist to build filesystem paths and OS arguments, where silently replacing
 * a byte you did not understand means operating on a *different path* than
 * the caller named. An overlong encoding in particular is a classic way to
 * smuggle a `/` past a validator that ran before the conversion.
 *
 * ## Measuring and converting
 *
 * Both functions return the space the result needs, **including the
 * terminator**, and return 0 for invalid input. Pass a NULL destination to
 * measure without converting; pass a destination too small and nothing is
 * written, so a caller cannot mistake a partial result for a whole one:
 *
 *     size_t units = gcu_utf8_to_utf16(path, NULL, 0);
 *     if (!units) { ... invalid UTF-8 ... }
 *     char16_t * wide = malloc(units * sizeof(char16_t));
 *     gcu_utf8_to_utf16(path, wide, units);
 */

#ifndef GHOTI_IO_GCU_UTF_H
#define GHOTI_IO_GCU_UTF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <ghoti.io/cutil/macros.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A UTF-16 code unit.
 *
 * Spelled as a fixed-width integer rather than `wchar_t`, which is 16 bits on
 * Windows and 32 on Linux, so a `wchar_t` buffer means two different things.
 * On Windows this is layout-compatible with `wchar_t` and may be cast.
 */
typedef uint16_t GCU_Char16;

/**
 * Convert a NUL-terminated UTF-8 string to UTF-16.
 *
 * @param utf8 The input.  Must be valid, strictly -- see the file comment.
 * @param out Destination, or NULL to measure only.
 * @param out_units Units available in @p out, including the terminator.
 * @return Units required including the terminator, or 0 if @p utf8 is NULL or
 *   not valid UTF-8.  Nothing is written unless @p out is non-NULL and
 *   @p out_units is at least the returned value.
 */
GCU_API size_t gcu_utf8_to_utf16(const char * utf8, GCU_Char16 * out,
    size_t out_units);

/**
 * Convert a NUL-terminated UTF-16 string to UTF-8.
 *
 * @param utf16 The input.  Unpaired surrogates are rejected.
 * @param out Destination, or NULL to measure only.
 * @param out_bytes Bytes available in @p out, including the terminator.
 * @return Bytes required including the terminator, or 0 if @p utf16 is NULL
 *   or contains an unpaired surrogate.  Nothing is written unless @p out is
 *   non-NULL and @p out_bytes is at least the returned value.
 */
GCU_API size_t gcu_utf16_to_utf8(const GCU_Char16 * utf16, char * out,
    size_t out_bytes);

/**
 * Whether a NUL-terminated string is valid UTF-8 by the strict rules above.
 *
 * @param utf8 The string to check.
 * @return true if valid; false if NULL or malformed.
 */
GCU_API bool gcu_utf8_is_valid(const char * utf8);

#ifdef __cplusplus
}
#endif

#endif // GHOTI_IO_GCU_UTF_H
