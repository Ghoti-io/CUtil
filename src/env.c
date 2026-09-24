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
 * Environment variables in UTF-8.  See env.h for the contract.
 */

#ifndef _WIN32
// setenv() and unsetenv() are 200112L; getenv() is ISO.
#define _POSIX_C_SOURCE 200112L
#endif

#include <stdlib.h>
#include <string.h>
#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/env.h>
#include <ghoti.io/cutil/utf.h>

//
// A name must be non-empty and must not contain '='.
//
// The '=' rule is not pedantry.  Interfaces in this family have historically
// taken "NAME=VALUE" in one string, so a name carrying an '=' sets a variable
// the caller did not name -- and a name arriving from configuration or from a
// filename is exactly where that comes from.  Rejecting it here means no
// caller has to know which of the underlying functions splits on it.
//
static bool name_is_valid(const char * name) {
  return name && name[0] != '\0' && strchr(name, '=') == NULL;
}

#ifdef _WIN32

#include <windows.h>

//
// Read the raw UTF-16 value into a freshly allocated buffer.
//
// Returns NULL when the variable is unset or on failure.  The caller frees.
// GetEnvironmentVariableW reports the required size including the terminator
// when the buffer is too small, and the copied length excluding it when it
// fits -- so the size is asked for separately rather than inferred from a
// return value that means two different things.
//
static GCU_Char16 * read_raw(const char * name) {
  size_t name_units = gcu_utf8_to_utf16(name, NULL, 0);
  if (!name_units) {
    return NULL;
  }
  GCU_Char16 * wide_name = malloc(name_units * sizeof(GCU_Char16));
  if (!wide_name) {
    return NULL;
  }
  gcu_utf8_to_utf16(name, wide_name, name_units);

  SetLastError(ERROR_SUCCESS);
  DWORD needed = GetEnvironmentVariableW((LPCWSTR)wide_name, NULL, 0);
  if (needed == 0) {
    // 0 with ERROR_ENVVAR_NOT_FOUND is "not set"; 0 otherwise is a failure.
    // Both return NULL here, and gcu_env_has() is the way to ask which.
    free(wide_name);
    return NULL;
  }

  GCU_Char16 * value = malloc((size_t)needed * sizeof(GCU_Char16));
  if (!value) {
    free(wide_name);
    return NULL;
  }
  SetLastError(ERROR_SUCCESS);
  DWORD written = GetEnvironmentVariableW((LPCWSTR)wide_name,
      (LPWSTR)value, needed);
  DWORD error = GetLastError();
  free(wide_name);

  // A racing setenv between the two calls can grow the value past `needed`.
  //
  // 0 is the copied length of an empty value as well as the failure return,
  // and the error code is what tells them apart.  Treating every 0 as a
  // failure made a variable set to "" read as unset.
  if ((written == 0 && error != ERROR_SUCCESS) || written >= needed) {
    free(value);
    return NULL;
  }
  return value;
}

size_t gcu_env_get(const char * name, char * buffer, size_t size) {
  if (!name_is_valid(name)) {
    return 0;
  }
  GCU_Char16 * value = read_raw(name);
  if (!value) {
    return 0;
  }

  size_t bytes = gcu_utf16_to_utf8(value, NULL, 0);
  if (bytes && buffer && size >= bytes) {
    gcu_utf16_to_utf8(value, buffer, size);
  }
  free(value);
  return bytes;
}

bool gcu_env_has(const char * name) {
  if (!name_is_valid(name)) {
    return false;
  }
  GCU_Char16 * value = read_raw(name);
  if (!value) {
    return false;
  }
  free(value);
  return true;
}

static int set_raw(const char * name, const char * value) {
  size_t name_units = gcu_utf8_to_utf16(name, NULL, 0);
  if (!name_units) {
    return -1;
  }
  GCU_Char16 * wide_name = malloc(name_units * sizeof(GCU_Char16));
  if (!wide_name) {
    return -1;
  }
  gcu_utf8_to_utf16(name, wide_name, name_units);

  GCU_Char16 * wide_value = NULL;
  if (value) {
    size_t value_units = gcu_utf8_to_utf16(value, NULL, 0);
    if (!value_units) {
      free(wide_name);
      return -1;
    }
    wide_value = malloc(value_units * sizeof(GCU_Char16));
    if (!wide_value) {
      free(wide_name);
      return -1;
    }
    gcu_utf8_to_utf16(value, wide_value, value_units);
  }

  // The C runtime keeps its own copy of the environment, taken at startup,
  // and getenv() reads that copy - so a change made only through
  // SetEnvironmentVariableW is invisible to getenv() in this same process,
  // and to every library that reads its settings that way.  _wputenv_s
  // updates the copy.
  //
  // It goes first because it also writes the process block, and an empty
  // string is how it *removes* a variable: the runtime cannot hold an empty
  // value.  SetEnvironmentVariableW then has the last word on the process
  // block, which can.  So after gcu_env_set(name, "") gcu_env_has() says set
  // and getenv() says unset - the runtime's limitation, and the same one
  // _putenv_s itself has.
  BOOL ok = _wputenv_s((const wchar_t *)wide_name,
      wide_value ? (const wchar_t *)wide_value : L"") == 0;
  // A NULL value removes the variable, which is what gcu_env_unset() wants.
  if (ok) {
    ok = SetEnvironmentVariableW((LPCWSTR)wide_name, (LPCWSTR)wide_value);
  }
  free(wide_name);
  free(wide_value);
  return ok ? 0 : -1;
}

int gcu_env_set(const char * name, const char * value) {
  if (!name_is_valid(name) || !value) {
    return -1;
  }
  return set_raw(name, value);
}

int gcu_env_unset(const char * name) {
  if (!name_is_valid(name)) {
    return -1;
  }
  return set_raw(name, NULL);
}

#else

size_t gcu_env_get(const char * name, char * buffer, size_t size) {
  if (!name_is_valid(name)) {
    return 0;
  }

  const char * value = getenv(name);
  if (!value) {
    return 0;
  }

  size_t bytes = strlen(value) + 1;
  // Copied rather than returned by pointer: getenv's result points into the
  // environment block, which a later setenv may free.  Nothing written when
  // it does not fit, so a truncated PATH cannot be mistaken for a whole one.
  if (buffer && size >= bytes) {
    memcpy(buffer, value, bytes);
  }
  return bytes;
}

bool gcu_env_has(const char * name) {
  return name_is_valid(name) && getenv(name) != NULL;
}

int gcu_env_set(const char * name, const char * value) {
  if (!name_is_valid(name) || !value) {
    return -1;
  }
  // 1 = overwrite.
  return setenv(name, value, 1) == 0 ? 0 : -1;
}

int gcu_env_unset(const char * name) {
  if (!name_is_valid(name)) {
    return -1;
  }
  return unsetenv(name) == 0 ? 0 : -1;
}

#endif
