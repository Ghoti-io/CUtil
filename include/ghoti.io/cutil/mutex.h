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
 * This file implements cross-platform mutex functions.
 */

#ifndef GHOTI_IO_GCU_MUTEX_H
#define GHOTI_IO_GCU_MUTEX_H

#include <ghoti.io/cutil/macros.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DOXYGEN
/**
 * Cross-platform mutex type.
 */
typedef void* GCU_MUTEX_T;

/**
 * Cross-platform mutex creation.
 * 
 * @param x The variable which will hold the mutex.
 * @return 0 on success, non-zero on failure.
 */
#define GCU_MUTEX_CREATE(x)

/**
 * Cross-platform mutex cleanup.
 * 
 * @param x Mutex.
 */
#define GCU_MUTEX_DESTROY(x)

/**
 * Cross-platform mutex lock.
 * 
 * @param x Mutex.
 */
#define GCU_MUTEX_LOCK(x)

/**
 * Cross-platform mutex unlock.
 * 
 * @param x Mutex.
 */
#define GCU_MUTEX_UNLOCK(x)

/**
 * Cross-platform mutex trylock.
 * 
 * @param x Mutex.
 * @return 0 if the lock was acquired, non-zero otherwise.  Note that this is
 *   a zero-on-success convention, not a boolean:  writing
 *   `if (GCU_MUTEX_TRYLOCK(x))` tests for *failure* to acquire.
 */
#define GCU_MUTEX_TRYLOCK(x)

#endif // DOXYGEN

#ifdef _WIN32
#include <windows.h>

#define GCU_MUTEX_T          HANDLE
#define GCU_MUTEX_CREATE(x)  !((x) = CreateMutex(NULL, FALSE, NULL))
#define GCU_MUTEX_DESTROY(x) CloseHandle(x)
#define GCU_MUTEX_LOCK(x)    WaitForSingleObject((x), INFINITE)
#define GCU_MUTEX_UNLOCK(x)  ReleaseMutex(x)
#define GCU_MUTEX_TRYLOCK(x) WaitForSingleObject((x), 0)

#else
#include <pthread.h>

#define GCU_MUTEX_T          pthread_mutex_t
#define GCU_MUTEX_CREATE(x)  pthread_mutex_init(&(x), NULL)
#define GCU_MUTEX_DESTROY(x) pthread_mutex_destroy(&(x))
#define GCU_MUTEX_LOCK(x)    pthread_mutex_lock(&(x))
#define GCU_MUTEX_UNLOCK(x)  pthread_mutex_unlock(&(x))
#define GCU_MUTEX_TRYLOCK(x) pthread_mutex_trylock(&(x))

#endif

#ifdef __cplusplus
}
#endif

#endif // GHOTI_IO_GCU_MUTEX_H
