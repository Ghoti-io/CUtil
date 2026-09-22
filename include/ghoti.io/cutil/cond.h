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
 * Cross-platform condition variables.
 *
 * A condition variable lets a thread wait until some predicate over shared
 * state becomes true, without spinning.  It always pairs with the mutex that
 * protects that state: the waiter must hold the mutex, the wait releases it
 * while blocked and reacquires it before returning, and the signaller changes
 * the state under the same mutex.
 *
 * ## Always loop on the predicate
 *
 * A wait may return without the predicate being true.  This is not a defect
 * to be worked around; it is how both platforms are specified, and code that
 * ignores it is broken in a way that testing rarely reproduces:
 *
 *     GCU_MUTEX_LOCK(m);
 *     while (!ready) {                     // while, never if
 *       gcu_cond_wait(&cv, &m);
 *     }
 *     consume();
 *     GCU_MUTEX_UNLOCK(m);
 *
 * Three separate reasons the predicate can be false on return: a spurious
 * wakeup, a broadcast that woke several waiters where only one can proceed,
 * and another thread winning the mutex between the signal and this thread
 * reacquiring it.  The loop handles all three and costs nothing.
 *
 * ## Timeouts
 *
 * `gcu_cond_timedwait()` takes **milliseconds as a plain `int`**, not a chron
 * type: cutil is the root of the dependency graph and cannot depend on a
 * sibling.  A negative timeout waits forever.
 *
 * Unlike `gcu_semaphore_timedwait()`, which reports a timeout and a failure
 * identically as -1, this returns `GCU_COND_TIMEDOUT` for a timeout and -1
 * only for an error.  A caller that cannot tell "nothing happened yet" from
 * "the primitive is broken" has to treat both as fatal or neither.
 *
 * The POSIX implementation uses `CLOCK_MONOTONIC` where the platform offers
 * it, so that a timeout is not extended or cut short by a change to the wall
 * clock.
 */

#ifndef GHOTI_IO_GCU_COND_H
#define GHOTI_IO_GCU_COND_H

#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/mutex.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DOXYGEN
/** An opaque condition variable.  Must not be copied once created. */
typedef void GCU_Cond;
#endif // DOXYGEN

#ifdef _WIN32
typedef CONDITION_VARIABLE GCU_Cond;
#else
#include <pthread.h>
typedef pthread_cond_t GCU_Cond;
#endif

/**
 * Returned by gcu_cond_timedwait() when the timeout elapsed.
 *
 * Distinct from both 0 (the wait returned, check your predicate) and -1 (the
 * call failed).
 */
#define GCU_COND_TIMEDOUT 1

/**
 * Create a condition variable.
 *
 * @param cond Pointer to the condition variable to initialise.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_cond_create(GCU_Cond * cond);

/**
 * Destroy a condition variable.
 *
 * No thread may be waiting on it.  On Windows this is a no-op, because a
 * CONDITION_VARIABLE owns no resources; it is still required, so that code
 * written against this API is correct on both platforms.
 *
 * @param cond Pointer to the condition variable to destroy.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_cond_destroy(GCU_Cond * cond);

/**
 * Wake one thread waiting on the condition variable, if any.
 *
 * Call it with the mutex held or not held; holding it is usually what you
 * want, because releasing first lets the woken thread run and immediately
 * block again on a mutex you still hold.
 *
 * @param cond Pointer to the condition variable.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_cond_signal(GCU_Cond * cond);

/**
 * Wake every thread waiting on the condition variable.
 *
 * @param cond Pointer to the condition variable.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_cond_broadcast(GCU_Cond * cond);

/**
 * Release the mutex and block until woken, then reacquire the mutex.
 *
 * The caller must hold @p mutex.  **Re-test your predicate in a loop** -- see
 * the file comment.
 *
 * @param cond Pointer to the condition variable.
 * @param mutex The mutex the caller holds, protecting the predicate.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_cond_wait(GCU_Cond * cond, GCU_MUTEX_T * mutex);

/**
 * As gcu_cond_wait(), but give up after @p timeout milliseconds.
 *
 * The mutex is reacquired before returning, on the timeout path as well as
 * the woken path.
 *
 * @param cond Pointer to the condition variable.
 * @param mutex The mutex the caller holds, protecting the predicate.
 * @param timeout Milliseconds to wait; negative waits forever.
 * @return 0 if woken, GCU_COND_TIMEDOUT if the timeout elapsed, -1 on failure.
 */
GCU_API int gcu_cond_timedwait(GCU_Cond * cond, GCU_MUTEX_T * mutex,
    int timeout);

#ifdef __cplusplus
}
#endif

#endif // GHOTI_IO_GCU_COND_H
