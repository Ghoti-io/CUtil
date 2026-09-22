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
 * Thread-local storage with a destructor.
 *
 * One key, one pointer per thread. Every thread starts holding NULL for a
 * newly created key, and a thread's value is invisible to every other thread.
 *
 * ## Why not `_Thread_local`
 *
 * C11's `_Thread_local` is simpler and faster and should be preferred when it
 * fits. It does not fit when the value is an owned allocation, because there
 * is no hook to free it as a thread exits -- the storage disappears and
 * whatever it pointed at leaks. This API exists for that case: the
 * @p destructor passed to gcu_tls_create() runs on each thread that holds a
 * non-NULL value, on that thread, as it exits.
 *
 * ## Windows uses fibre-local storage, deliberately
 *
 * `TlsAlloc` has no destructor callback; `FlsAlloc` does, and behaves the
 * same way for a thread that is not a fibre -- which is every thread this
 * library creates. Taking the fibre API to get the callback is the only way
 * to offer the same contract on both platforms, and a contract that silently
 * leaks on one of them is worse than not offering it.
 *
 * ## The rules that bite
 *
 * The destructor does **not** run for the thread that calls
 * gcu_tls_destroy(), and does not run for values still held when the process
 * exits. It runs on thread exit and nowhere else, so a value belonging to the
 * main thread is the caller's to release.
 *
 * gcu_tls_get() returns NULL both for "never set" and for "set to NULL". If
 * you need to tell those apart, store a pointer to something rather than a
 * sentinel.
 */

#ifndef GHOTI_IO_GCU_TLS_H
#define GHOTI_IO_GCU_TLS_H

#include <ghoti.io/cutil/macros.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DOXYGEN
/** A thread-local storage key. */
typedef void * GCU_TLS;
#endif // DOXYGEN

#ifdef _WIN32
#include <windows.h>
typedef DWORD GCU_TLS;
#else
#include <pthread.h>
typedef pthread_key_t GCU_TLS;
#endif

/**
 * Create a key.  Every thread's value for it begins as NULL.
 *
 * @param key Pointer to the key to initialise.
 * @param destructor Called with a thread's non-NULL value as that thread
 *   exits, on that thread.  NULL for no destructor.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_tls_create(GCU_TLS * key, void (*destructor)(void * value));

/**
 * Release a key.  Does not run the destructor for any thread's value.
 *
 * @param key Pointer to the key to release.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_tls_destroy(GCU_TLS * key);

/**
 * This thread's value for @p key.
 *
 * @param key The key.
 * @return The value, or NULL if this thread has not set one.
 */
GCU_API void * gcu_tls_get(GCU_TLS key);

/**
 * Set this thread's value for @p key.
 *
 * Does not run the destructor on the value being replaced; if the old value
 * was owned, fetch and release it first.
 *
 * @param key The key.
 * @param value The value.  NULL suppresses the destructor for this thread.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_tls_set(GCU_TLS key, void * value);

#ifdef __cplusplus
}
#endif

#endif // GHOTI_IO_GCU_TLS_H
