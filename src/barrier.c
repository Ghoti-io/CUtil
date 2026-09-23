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
 * A reusable thread barrier.  See barrier.h for the contract, including why
 * this is built on gcu_mutex and gcu_cond rather than on a platform barrier.
 *
 * There is no `#ifdef _WIN32` in this file, and that is the point: the two
 * primitives underneath it have already answered the platform question.
 */

#include <string.h>
#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/barrier.h>
#include <ghoti.io/cutil/cond.h>
#include <ghoti.io/cutil/mutex.h>

int gcu_barrier_create(GCU_Barrier * barrier, unsigned int count) {
  if (!barrier || count == 0) {
    return -1;
  }
  memset(barrier, 0, sizeof(*barrier));
  if (GCU_MUTEX_CREATE(barrier->mutex) != 0) {
    return -1;
  }
  if (gcu_cond_create(&barrier->cond) != 0) {
    GCU_MUTEX_DESTROY(barrier->mutex);
    memset(barrier, 0, sizeof(*barrier));
    return -1;
  }
  // Set last, so that a barrier whose construction failed part way through
  // is indistinguishable from one that was never created: threshold is what
  // the other two functions test before they touch the mutex.
  barrier->threshold = count;
  return 0;
}

int gcu_barrier_wait(GCU_Barrier * barrier) {
  // Tested before the lock, not after, because a zeroed or destroyed barrier
  // has a zeroed mutex and locking one of those is undefined -- the check has
  // to come from somewhere that does not require the lock to be valid.
  if (!barrier || barrier->threshold == 0) {
    return -1;
  }
  if (GCU_MUTEX_LOCK(barrier->mutex) != 0) {
    return -1;
  }

  int result = 0;
  barrier->inside++;

  if (++barrier->arrived == barrier->threshold) {
    // Last in.  Re-arm before releasing anyone, because the first thread out
    // may be back here before the last one has woken.
    barrier->arrived = 0;
    barrier->generation++;
    result = GCU_BARRIER_SERIAL;
    gcu_cond_broadcast(&barrier->cond);
  }
  else {
    // The cycle, not the count, is what this waits on.
    //
    // Waiting for `arrived` to reach zero, or for any predicate over the
    // count, cannot work on a barrier that re-arms: by the time a slow waiter
    // wakes, the count may have been reset and partly refilled by threads
    // already in the *next* cycle, which looks exactly like the state it went
    // to sleep in.  A generation only ever moves forwards, so "the cycle I
    // joined has ended" stays true once it becomes true, however long this
    // thread takes to notice.
    unsigned long cycle = barrier->generation;
    while (barrier->generation == cycle) {
      if (gcu_cond_wait(&barrier->cond, &barrier->mutex) != 0) {
        // Cannot be recovered from: this thread has been counted into a
        // cycle it is about to abandon, so the cycle will never complete.
        // Reported rather than retried, since retrying is an endless loop on
        // a condition variable that is not working.
        result = -1;
        break;
      }
    }
  }

  // Counted down here rather than at the top of the cycle, so that "no thread
  // is inside" means what gcu_barrier_destroy() needs it to mean: not "the
  // last cycle finished" but "nobody is still touching this mutex".
  barrier->inside--;
  GCU_MUTEX_UNLOCK(barrier->mutex);
  return result;
}

int gcu_barrier_destroy(GCU_Barrier * barrier) {
  if (!barrier || barrier->threshold == 0) {
    return -1;
  }
  if (GCU_MUTEX_LOCK(barrier->mutex) != 0) {
    return -1;
  }
  if (barrier->inside != 0) {
    GCU_MUTEX_UNLOCK(barrier->mutex);
    return -1;
  }
  GCU_MUTEX_UNLOCK(barrier->mutex);

  int cond_released = gcu_cond_destroy(&barrier->cond);
  int mutex_released = GCU_MUTEX_DESTROY(barrier->mutex);
  memset(barrier, 0, sizeof(*barrier));
  return (cond_released == 0 && mutex_released == 0) ? 0 : -1;
}
