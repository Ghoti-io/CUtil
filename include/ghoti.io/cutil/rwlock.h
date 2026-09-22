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
 * Cross-platform read-write locks.
 *
 * Any number of readers may hold the lock at once; a writer excludes
 * everyone.  Worth reaching for over GCU_MUTEX_T only when reads genuinely
 * dominate and are not trivially short -- a read-write lock does more
 * bookkeeping than a mutex, so for a lock held across three instructions it
 * is slower, not faster.
 *
 * ## Why unlocking is two functions
 *
 * POSIX has one `pthread_rwlock_unlock` that works for either mode, because
 * the implementation records which mode the caller holds.  A Windows
 * `SRWLOCK` does not: `ReleaseSRWLockShared` and `ReleaseSRWLockExclusive`
 * are different calls, and using the wrong one corrupts the lock.
 *
 * So the caller has to say, and the API asks. The alternative -- tracking the
 * mode in this library so a single `unlock` could infer it -- cannot be done
 * correctly for the shared case without a second lock around the bookkeeping,
 * which would serialise exactly the readers the whole primitive exists to let
 * run together.
 *
 * This is the platform with the weaker guarantee deciding the shape, which is
 * the right way round: an API that promises more than one implementation can
 * deliver is a bug on that implementation.
 *
 * ## Not recursive, and not upgradable
 *
 * Taking a read lock you already hold may deadlock, and is undefined here.
 * There is no way to upgrade a read lock to a write lock; release and
 * reacquire, and re-test whatever you read, because the state can change in
 * between.
 */

#ifndef GHOTI_IO_GCU_RWLOCK_H
#define GHOTI_IO_GCU_RWLOCK_H

#include <ghoti.io/cutil/macros.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DOXYGEN
/** An opaque read-write lock.  Must not be copied once created. */
typedef void GCU_RWLock;
#endif // DOXYGEN

#ifdef _WIN32
#include <windows.h>
typedef SRWLOCK GCU_RWLock;
#else
#include <pthread.h>
typedef pthread_rwlock_t GCU_RWLock;
#endif

/**
 * Create a read-write lock.
 *
 * @param lock Pointer to the lock to initialise.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_rwlock_create(GCU_RWLock * lock);

/**
 * Destroy a read-write lock.  No thread may hold or be waiting for it.
 *
 * A no-op on Windows, where an SRWLOCK owns no resources.  Still required, so
 * that correct code is the same on both platforms.
 *
 * @param lock Pointer to the lock to destroy.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_rwlock_destroy(GCU_RWLock * lock);

/**
 * Block until the lock is held for reading.  Other readers may hold it too.
 *
 * @param lock Pointer to the lock.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_rwlock_read_lock(GCU_RWLock * lock);

/**
 * Take the lock for reading without blocking.
 *
 * @param lock Pointer to the lock.
 * @return 0 if acquired, non-zero if not.
 */
GCU_API int gcu_rwlock_read_trylock(GCU_RWLock * lock);

/**
 * Release a lock held for reading.  See the file comment on why this is not
 * the same call as gcu_rwlock_write_unlock().
 *
 * @param lock Pointer to the lock.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_rwlock_read_unlock(GCU_RWLock * lock);

/**
 * Block until the lock is held for writing, excluding everyone else.
 *
 * @param lock Pointer to the lock.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_rwlock_write_lock(GCU_RWLock * lock);

/**
 * Take the lock for writing without blocking.
 *
 * @param lock Pointer to the lock.
 * @return 0 if acquired, non-zero if not.
 */
GCU_API int gcu_rwlock_write_trylock(GCU_RWLock * lock);

/**
 * Release a lock held for writing.
 *
 * @param lock Pointer to the lock.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_rwlock_write_unlock(GCU_RWLock * lock);

#ifdef __cplusplus
}
#endif

#endif // GHOTI_IO_GCU_RWLOCK_H
