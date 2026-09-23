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
 * Run-time library loading.  See library.h for the contract.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200112L
#endif

#include <stdlib.h>
#include <string.h>
#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/error.h>
#include <ghoti.io/cutil/library.h>
#include <ghoti.io/cutil/utf.h>

#ifdef _WIN32

#include <windows.h>

// Windows reports through GetLastError(), which the next call overwrites.
// Captured at the point of failure so that gcu_library_error() describes the
// failure the caller is asking about rather than whatever happened since.
static DWORD last_error = ERROR_SUCCESS;

static GCU_Char16 * widen(const char * utf8) {
  size_t units = gcu_utf8_to_utf16(utf8, NULL, 0);
  if (!units) {
    return NULL;
  }
  GCU_Char16 * wide = malloc(units * sizeof(GCU_Char16));
  if (!wide) {
    return NULL;
  }
  gcu_utf8_to_utf16(utf8, wide, units);
  return wide;
}

int gcu_library_open(GCU_Library * library, const char * path) {
  if (!library || !path) {
    return -1;
  }
  GCU_Char16 * wide = widen(path);
  if (!wide) {
    last_error = ERROR_INVALID_NAME;
    return -1;
  }

  HMODULE handle = LoadLibraryW((LPCWSTR)wide);
  free(wide);
  if (!handle) {
    last_error = GetLastError();
    return -1;
  }
  *library = handle;
  last_error = ERROR_SUCCESS;
  return 0;
}

int gcu_library_close(GCU_Library * library) {
  if (!library || !*library) {
    return -1;
  }
  if (!FreeLibrary(*library)) {
    last_error = GetLastError();
    return -1;
  }
  *library = NULL;
  last_error = ERROR_SUCCESS;
  return 0;
}

GCU_Library_Function gcu_library_symbol(GCU_Library library,
    const char * name) {
  if (!library || !name) {
    return NULL;
  }
  FARPROC symbol = GetProcAddress(library, name);
  if (!symbol) {
    last_error = GetLastError();
    return NULL;
  }
  // A success clears what an earlier failure left, as dlsym() does on the
  // POSIX side: otherwise an error from a lookup the caller has moved past is
  // reported against one that worked.
  last_error = ERROR_SUCCESS;
  // No union here, unlike the POSIX branch below: GetProcAddress already
  // returns a function pointer, so this is an ordinary and fully defined
  // conversion between two function pointer types.  Routing it through a
  // void* would have *introduced* the problem the union exists to contain --
  // which is what -Wpedantic said when it was written that way.
  return (GCU_Library_Function)symbol;
}

int gcu_library_error(char * buffer, size_t size) {
  if (!buffer || size == 0) {
    return -1;
  }
  buffer[0] = '\0';
  if (last_error == ERROR_SUCCESS) {
    return -1;
  }

  // gcu_error_string() rather than FormatMessage directly: FormatMessage
  // fails outright on a buffer too small for the message instead of
  // truncating it, and the error module already works around that.
  int result = gcu_error_string((int)last_error, buffer, size);
  // Cleared on read, matching dlerror(), so the two platforms behave the same
  // way for a caller that checks twice.
  last_error = ERROR_SUCCESS;
  return result;
}

#else

#include <dlfcn.h>

int gcu_library_open(GCU_Library * library, const char * path) {
  if (!library || !path) {
    return -1;
  }
  // Clear any stale message first, so that a failure here is described by
  // this failure and not by an earlier one nobody read.
  dlerror();

  // RTLD_NOW rather than RTLD_LAZY: a missing symbol should be an error at
  // the point of loading, where the caller is checking a return value, not a
  // crash at the point of first call, somewhere else entirely.
  void * handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    return -1;
  }
  *library = handle;
  return 0;
}

int gcu_library_close(GCU_Library * library) {
  if (!library || !*library) {
    return -1;
  }
  dlerror();
  if (dlclose(*library) != 0) {
    return -1;
  }
  *library = NULL;
  return 0;
}

GCU_Library_Function gcu_library_symbol(GCU_Library library,
    const char * name) {
  if (!library || !name) {
    return NULL;
  }
  dlerror();
  void * symbol = dlsym(library, name);
  if (!symbol) {
    return NULL;
  }

  // dlsym hands back a void*, and C does not define converting an object
  // pointer to a function pointer.  POSIX requires it to work, so it is sound
  // in practice; a union keeps it implementation-defined rather than
  // undefined, and keeps it in this one place instead of at every call site.
  union {
    void * object;
    GCU_Library_Function function;
  } pun;
  pun.object = symbol;
  return pun.function;
}

int gcu_library_error(char * buffer, size_t size) {
  if (!buffer || size == 0) {
    return -1;
  }
  buffer[0] = '\0';

  // dlerror() clears the message as a side effect of reading it, which is why
  // every entry point above clears it before doing anything: otherwise a
  // caller could read a message belonging to a call that already succeeded.
  const char * message = dlerror();
  if (!message) {
    return -1;
  }

  size_t length = strlen(message);
  if (length >= size) {
    length = size - 1;
  }
  memcpy(buffer, message, length);
  buffer[length] = '\0';
  return 0;
}

#endif
