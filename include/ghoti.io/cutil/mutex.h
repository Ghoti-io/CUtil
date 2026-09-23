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
 * This file implements cross-platform mutex functions.
 *
 * ## The contract
 *
 * Every macro here evaluates to an `int` that is **0 on success** and
 * non-zero on failure, on every platform.  Write `if (GCU_MUTEX_LOCK(m)) {
 * ... }` and it means the same thing everywhere.
 *
 * That sounds like it should go without saying, and it did not: the Windows
 * branch used to expand `GCU_MUTEX_UNLOCK` to `ReleaseMutex` and
 * `GCU_MUTEX_DESTROY` to `CloseHandle`, both of which return **non-zero on
 * success** -- the exact opposite of the pthread calls the other branch used.
 * `GCU_MUTEX_CREATE` already carried a `!` to normalise `CreateMutex`, so the
 * problem had been seen once and fixed in one place out of three.  Nothing
 * caught it because none of the 67 call sites in this library reads the
 * return value.
 *
 * ## The mutex is not recursive, on either platform
 *
 * Locking a mutex you already hold is a deadlock.  This also used to differ:
 * a Windows `CreateMutex` handle **is** recursive, while a pthread mutex
 * initialised with a NULL attribute is not, so the same double-lock was a
 * working program on one platform and a hang on the other.  Measured before
 * the change: `GCU_MUTEX_TRYLOCK` on a mutex held by the calling thread
 * returned 16 (`EBUSY`) here and would have returned 0 ("acquired") there.
 *
 * Windows now uses `SRWLOCK`, which is non-recursive like the pthread
 * default.  It is also a user-space lock rather than a kernel object, so it
 * does not cost a syscall per operation the way a `HANDLE` mutex does -- this
 * library takes and releases these locks roughly ninety times in `pool.c` and
 * `sequencer.c` alone.
 *
 * An `SRWLOCK` must not be copied or moved once initialised.  Neither must a
 * `pthread_mutex_t`, so any code that broke under the new type was already
 * broken under the old one.
 *
 * ## Portability note
 *
 * The Windows branch below is not compiled by this workspace's Linux build.
 * It is parse-checked against stub declarations by `test/win32-stubs/`, which
 * catches the syntax-error class but not semantics -- see the comment there.
 */

#ifndef GHOTI_IO_GCU_MUTEX_H
#define GHOTI_IO_GCU_MUTEX_H

#include <ghoti.io/cutil/macros.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DOXYGEN
/** An opaque, non-recursive mutex.  Must not be copied once created. */
typedef void* GCU_MUTEX_T;
/** Initialise a mutex.  Evaluates to 0 on success. */
#define GCU_MUTEX_CREATE(x)
/** Release a mutex's resources.  Evaluates to 0 on success. */
#define GCU_MUTEX_DESTROY(x)
/** Block until the mutex is held.  Evaluates to 0 on success. */
#define GCU_MUTEX_LOCK(x)
/** Release a held mutex.  Evaluates to 0 on success. */
#define GCU_MUTEX_UNLOCK(x)
/** Acquire without blocking.  Evaluates to 0 if acquired, non-zero if not. */
#define GCU_MUTEX_TRYLOCK(x)
#endif // DOXYGEN

#ifdef _WIN32
#include <windows.h>

#define GCU_MUTEX_T          SRWLOCK

// InitializeSRWLock and the acquire/release calls return void and cannot
// fail, so each is wrapped in a function that yields the 0 the contract above
// promises.  A comma expression such as `(f(&x), 0)` says the same thing, but
// GCC reports its unused right-hand side as -Wunused-value wherever the macro
// is used as a statement, which is almost everywhere.  SRWLOCK needs no
// teardown; DESTROY still takes its argument so that it cannot become an
// unused variable.
static inline int gcu_mutex_create_srw_(SRWLOCK * m) { InitializeSRWLock(m); return 0; }
static inline int gcu_mutex_destroy_srw_(SRWLOCK * m) { (void)m; return 0; }
static inline int gcu_mutex_lock_srw_(SRWLOCK * m) { AcquireSRWLockExclusive(m); return 0; }
static inline int gcu_mutex_unlock_srw_(SRWLOCK * m) { ReleaseSRWLockExclusive(m); return 0; }

#define GCU_MUTEX_CREATE(x)  gcu_mutex_create_srw_(&(x))
#define GCU_MUTEX_DESTROY(x) gcu_mutex_destroy_srw_(&(x))
#define GCU_MUTEX_LOCK(x)    gcu_mutex_lock_srw_(&(x))
#define GCU_MUTEX_UNLOCK(x)  gcu_mutex_unlock_srw_(&(x))

// The only one that reports anything: TryAcquireSRWLockExclusive returns
// non-zero when it acquired the lock, which is backwards from the contract,
// so it is inverted here rather than at 67 call sites.
#define GCU_MUTEX_TRYLOCK(x) (TryAcquireSRWLockExclusive(&(x)) ? 0 : 1)

#else
#include <pthread.h>

#define GCU_MUTEX_T          pthread_mutex_t

// Every pthread call here already returns 0 on success, which is the contract.
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
