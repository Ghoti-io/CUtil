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
 * The last OS error and its message.  See error.h for the contract.
 */

#ifndef _WIN32
// 200112L selects the XSI strerror_r, `int strerror_r(int, char *, size_t)`.
// _GNU_SOURCE must NOT be defined here: glibc then supplies a *different*
// function of the same name returning `char *`, which may leave the caller's
// buffer untouched and return a pointer to a static string instead. The two
// are distinguished only by feature-test macros, so this file states which it
// wants and does not include anything before saying so.
#define _POSIX_C_SOURCE 200112L
#endif

#include <string.h>
#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/error.h>

#ifdef _WIN32

#include <windows.h>

int gcu_error_last(void) {
  return (int)GetLastError();
}

int gcu_error_string(int code, char * buffer, size_t size) {
  if (!buffer || size == 0) {
    return -1;
  }
  buffer[0] = '\0';

  // FORMAT_MESSAGE_ALLOCATE_BUFFER is deliberately absent: formatting into a
  // stack buffer is what keeps this allocation-free and therefore usable from
  // a failure path that may itself be out of memory.
  //
  // It formats into its own buffer rather than the caller's because
  // FormatMessage does not truncate: given too little room it fails with
  // ERROR_INSUFFICIENT_BUFFER and writes nothing, where the contract promises
  // a truncated message.
  char full[GCU_ERROR_STRING_MAX];
  DWORD written = FormatMessageA(
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, (DWORD)code, 0, full, (DWORD)sizeof(full), NULL);

  if (written == 0) {
    return -1;
  }

  // FormatMessage appends ".\r\n" to most system messages, which is wrong in
  // the middle of a sentence and wrong in a log line.
  while (written > 0
      && (full[written - 1] == '\n' || full[written - 1] == '\r'
          || full[written - 1] == '.' || full[written - 1] == ' ')) {
    --written;
  }

  size_t keep = written < size - 1 ? written : size - 1;
  memcpy(buffer, full, keep);
  buffer[keep] = '\0';
  return 0;
}

#else

#include <errno.h>

int gcu_error_last(void) {
  return errno;
}

int gcu_error_string(int code, char * buffer, size_t size) {
  if (!buffer || size == 0) {
    return -1;
  }
  buffer[0] = '\0';

  // XSI strerror_r returns 0, or an error number.  It returns ERANGE when the
  // buffer was too small, having written as much as fit -- a truncated
  // message is the documented outcome of a small buffer, not a failure, so
  // that case reports success.
  int rc = strerror_r(code, buffer, size);
  if (rc == 0) {
    return 0;
  }
  if (rc == ERANGE) {
    // Some implementations do not terminate on ERANGE.  Do it here rather
    // than trusting it.
    buffer[size - 1] = '\0';
    return 0;
  }

  buffer[0] = '\0';
  return -1;
}

#endif

int gcu_error_string_last(char * buffer, size_t size) {
  // gcu_error_last() first: a NULL-buffer check that ran before it would be
  // reading errno after this function's own argument validation, which is
  // exactly the "something else ran in between" the header warns about.
  int code = gcu_error_last();
  return gcu_error_string(code, buffer, size);
}
