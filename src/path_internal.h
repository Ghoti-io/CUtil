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
 * Private helpers shared between path.c and file.c.
 *
 * Everything here is Windows-only.  It lives in a header rather than in
 * either source file because both need it, and a second copy of a conversion
 * nobody on this machine can compile is a copy that will drift unnoticed -
 * which is the defect this whole module was written to stop repeating.
 *
 * `static inline` rather than a linked symbol: each translation unit gets its
 * own copy, so there is no internal name to collide with anything and nothing
 * to namespace.
 */

#ifndef GHOTI_IO_GCU_PATH_INTERNAL_H
#define GHOTI_IO_GCU_PATH_INTERNAL_H

#include <ghoti.io/cutil/allocator.h>
#include <ghoti.io/cutil/macros.h>

#ifdef _WIN32

#include <windows.h>

/* TODO(windows): never compiled or run on Windows; see WINDOWS-TODO.md.
 * What "done" looks like: a path holding non-ASCII characters survives a
 * round trip through gcu_path_cwd() and opens through gcu_file_read(). */

/**
 * Convert UTF-16 from Win32 into UTF-8 held by @p allocator.
 *
 * The wide Win32 entry points are used throughout rather than the ANSI ones
 * because the ANSI ones go through the process code page, which cannot
 * represent every filename the filesystem accepts.  A path that merely passes
 * through one comes back naming a different file, or nothing at all.
 *
 * @param allocator Allocator for the result.
 * @param wide The NUL-terminated wide string to convert.
 * @return An allocated UTF-8 string, or NULL on failure.
 */
static inline char * gcu_path_internal_from_wide(
    const GCU_Allocator * allocator, const wchar_t * wide) {
  int needed = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
  if (needed <= 0) {
    return NULL;
  }
  char * buffer = (char *)gcu_allocator_malloc(allocator, (size_t)needed);
  if (!buffer) {
    return NULL;
  }
  if (WideCharToMultiByte(CP_UTF8, 0, wide, -1, buffer, needed, NULL, NULL)
      <= 0) {
    gcu_allocator_free(allocator, buffer);
    return NULL;
  }
  return buffer;
}

/**
 * Convert UTF-8 into UTF-16 for Win32, held by @p allocator.
 *
 * @param allocator Allocator for the result.
 * @param utf8 The NUL-terminated UTF-8 string to convert.
 * @return An allocated wide string, or NULL on failure.
 */
static inline wchar_t * gcu_path_internal_to_wide(
    const GCU_Allocator * allocator, const char * utf8) {
  int needed = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
  if (needed <= 0) {
    return NULL;
  }
  wchar_t * buffer = (wchar_t *)gcu_allocator_malloc(allocator,
      (size_t)needed * sizeof(wchar_t));
  if (!buffer) {
    return NULL;
  }
  if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buffer, needed) <= 0) {
    gcu_allocator_free(allocator, buffer);
    return NULL;
  }
  return buffer;
}

#endif // _WIN32

#endif // GHOTI_IO_GCU_PATH_INTERNAL_H
