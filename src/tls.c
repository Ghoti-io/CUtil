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
 * Thread-local storage.  See tls.h for the contract.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200112L
#endif

#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/tls.h>

#ifdef _WIN32

int gcu_tls_create(GCU_TLS * key, void (*destructor)(void * value)) {
  if (!key) {
    return -1;
  }
  // FlsAlloc rather than TlsAlloc: only the fibre API takes a destructor, and
  // it behaves identically for a thread that is not a fibre.  See tls.h.
  DWORD index = FlsAlloc((PFLS_CALLBACK_FUNCTION)destructor);
  if (index == FLS_OUT_OF_INDEXES) {
    return -1;
  }
  *key = index;
  return 0;
}

int gcu_tls_destroy(GCU_TLS * key) {
  if (!key) {
    return -1;
  }
  return FlsFree(*key) ? 0 : -1;
}

void * gcu_tls_get(GCU_TLS key) {
  return FlsGetValue(key);
}

int gcu_tls_set(GCU_TLS key, void * value) {
  return FlsSetValue(key, value) ? 0 : -1;
}

#else

int gcu_tls_create(GCU_TLS * key, void (*destructor)(void * value)) {
  if (!key) {
    return -1;
  }
  return pthread_key_create(key, destructor) == 0 ? 0 : -1;
}

int gcu_tls_destroy(GCU_TLS * key) {
  if (!key) {
    return -1;
  }
  return pthread_key_delete(*key) == 0 ? 0 : -1;
}

void * gcu_tls_get(GCU_TLS key) {
  return pthread_getspecific(key);
}

int gcu_tls_set(GCU_TLS key, void * value) {
  return pthread_setspecific(key, value) == 0 ? 0 : -1;
}

#endif
