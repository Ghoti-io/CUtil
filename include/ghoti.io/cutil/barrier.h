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
 * A rendezvous for a fixed number of threads: none of them leaves until all
 * of them have arrived.
 *
 * What it is for is phases.  Split an array between four threads, have each
 * fill its quarter, and the thread that finishes first must not start reading
 * the whole array while a slower one is still writing into it.  A barrier is
 * the line between "everyone is writing" and "everyone is reading", and it is
 * one call rather than a counter, a mutex and a condition variable at every
 * such line.
 *
 * ## It is reusable, and that is the hard part
 *
 * A barrier trips and immediately re-arms, so the same one separates phase 1
 * from phase 2 and phase 2 from phase 3.  That is what makes it different
 * from a one-shot latch, and it is also where a hand-written version goes
 * wrong: the thread released first can run the whole of the next phase and
 * arrive back at the barrier before the slowest thread has even woken from
 * the last one.  Counting arrivals is not enough to tell those two apart, so
 * this counts *cycles* -- a waiter waits for the cycle it joined to end, not
 * for a number to reach a threshold.
 *
 * ## One thread gets a different answer
 *
 * Exactly one of the threads in each cycle gets @ref GCU_BARRIER_SERIAL back
 * and the rest get 0.  Which one is not defined and will vary.  It is the
 * place for the work that has to happen once between phases -- swapping two
 * buffers, totalling what the others produced -- without a separate lock, and
 * it is safe there because the others are still inside the barrier.
 *
 * ## There is no timed wait, deliberately
 *
 * Every other wait in this library has one.  This does not, because a timeout
 * on a barrier does not degrade the behaviour, it destroys it: a thread that
 * gives up and leaves has still been counted out of the total, so the barrier
 * is one short forever and every remaining thread waits for an arrival that
 * can never come.  A caller who needs to bound the wait needs a different
 * structure, not this one with a clock attached.  POSIX leaves barriers
 * untimed for the same reason.
 *
 * For the same reason, every thread must agree on the count.  A barrier
 * created for four and used by three is not slow; it is stopped.
 *
 * ## Built on the mutex and the condition variable, not on a platform barrier
 *
 * Every other module here wraps what the operating system provides.  This one
 * does not, and the reason is that on POSIX there may be nothing to wrap:
 * barriers are a POSIX *option*, listed with the features an implementation
 * is permitted to leave out, and Apple's is the well-known one that does --
 * `pthread_barrier_t` does not exist there.  (Checked for glibc on this
 * machine, where `_POSIX_BARRIERS` is defined; the Darwin half is reported,
 * not measured here.)
 *
 * So the choice was three arms -- Windows, POSIX-with-barriers, and a
 * fallback for POSIX-without -- of which the tests could ever run one, or a
 * single implementation over gcu_mutex and gcu_cond that every platform and
 * every test uses.  The second is smaller, and the code that ships is the
 * code that was exercised.
 */

#ifndef GHOTI_IO_GCU_BARRIER_H
#define GHOTI_IO_GCU_BARRIER_H

#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/cond.h>
#include <ghoti.io/cutil/mutex.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returned by gcu_barrier_wait() to exactly one thread per cycle.
 *
 * Distinct from 0, which every other thread in the cycle gets, and from -1,
 * which means the call failed.
 */
#define GCU_BARRIER_SERIAL 1

/**
 * A barrier.  Create it with gcu_barrier_create() before any thread waits on
 * it; every member is private.
 */
typedef struct {
  GCU_MUTEX_T mutex;         ///< Private.
  GCU_Cond cond;             ///< Private.
  unsigned int threshold;    ///< Private.  0 means "not a live barrier".
  unsigned int arrived;      ///< Private.  Arrivals in the current cycle.
  unsigned int inside;       ///< Private.  Threads that have not yet left.
  unsigned long generation;  ///< Private.  Cycles completed.
} GCU_Barrier;

/**
 * Prepare a barrier for @p count threads.
 *
 * @param barrier The barrier to initialise.  Its previous contents, if any,
 *   are overwritten rather than released.
 * @param count How many arrivals make a cycle.  Must be at least 1; a
 *   barrier for 1 is legal and trips immediately, which is what makes a
 *   thread count of 1 a degenerate case rather than a special case at the
 *   call site.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_barrier_create(GCU_Barrier * barrier, unsigned int count);

/**
 * Wait for the rest.
 *
 * Returns once the barrier's full count of threads have called this, and not
 * before.  The barrier is ready for the next cycle as soon as it trips, so
 * the same barrier may be waited on again immediately.
 *
 * @param barrier A barrier from a successful gcu_barrier_create().
 * @return @ref GCU_BARRIER_SERIAL to one thread of the cycle, 0 to the
 *   others, -1 if the call failed.  A -1 from a barrier that was created
 *   successfully leaves it unusable: this thread was counted as having
 *   arrived and is leaving anyway, so the cycle can never complete.  Destroy
 *   it rather than waiting on it again.
 */
GCU_API int gcu_barrier_wait(GCU_Barrier * barrier);

/**
 * Release a barrier nobody is using.
 *
 * Refuses, rather than corrupting, while any thread is still inside
 * gcu_barrier_wait() -- POSIX leaves that case undefined, and an undefined
 * case in a teardown path is one that shows up as a crash somewhere else
 * entirely.  A -1 here means threads are still in it, so the mistake is
 * reported at the call that made it.
 *
 * @param barrier The barrier to release.  Cleared on success, untouched
 *   otherwise.
 * @return 0 on success, -1 if a thread is still inside or the barrier is not
 *   a live one.
 */
GCU_API int gcu_barrier_destroy(GCU_Barrier * barrier);

#ifdef __cplusplus
}
#endif

#endif // GHOTI_IO_GCU_BARRIER_H
