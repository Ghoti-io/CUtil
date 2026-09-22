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
 * Cross-platform condition variables.  See cond.h for the contract.
 */

// clock_gettime() needs 199309L and pthread_condattr_setclock() needs
// 200112L; -std=c17 is strict ISO, which hides both.  Spelled the way dir.c,
// path.c, file.c and semaphore.c spell it, and before any include, since a
// feature test macro set after the first header has already been ignored.
#ifndef _WIN32
#define _POSIX_C_SOURCE 200112L
#endif

#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/cond.h>

#ifdef _WIN32

// Windows CONDITION_VARIABLE pairs with SRWLOCK, which is what GCU_MUTEX_T
// became when the mutex stopped being a recursive kernel object.  That
// pairing is the reason this file has no timespec arithmetic: the wait takes
// a relative millisecond count directly.

int gcu_cond_create(GCU_Cond * cond) {
  if (!cond) {
    return -1;
  }
  InitializeConditionVariable(cond);
  return 0;
}

int gcu_cond_destroy(GCU_Cond * cond) {
  // A CONDITION_VARIABLE owns nothing, so there is nothing to release.  The
  // call still exists, and still rejects NULL, so that code written against
  // this API behaves the same way on both platforms.
  return cond ? 0 : -1;
}

int gcu_cond_signal(GCU_Cond * cond) {
  if (!cond) {
    return -1;
  }
  WakeConditionVariable(cond);
  return 0;
}

int gcu_cond_broadcast(GCU_Cond * cond) {
  if (!cond) {
    return -1;
  }
  WakeAllConditionVariable(cond);
  return 0;
}

int gcu_cond_wait(GCU_Cond * cond, GCU_MUTEX_T * mutex) {
  if (!cond || !mutex) {
    return -1;
  }
  return SleepConditionVariableSRW(cond, mutex, INFINITE, 0) ? 0 : -1;
}

int gcu_cond_timedwait(GCU_Cond * cond, GCU_MUTEX_T * mutex, int timeout) {
  if (!cond || !mutex) {
    return -1;
  }
  DWORD ms = timeout < 0 ? INFINITE : (DWORD)timeout;
  if (SleepConditionVariableSRW(cond, mutex, ms, 0)) {
    return 0;
  }
  return GetLastError() == ERROR_TIMEOUT ? GCU_COND_TIMEDOUT : -1;
}

#else

#include <errno.h>
#include <time.h>

// Which clock the timeout is measured against.
//
// The pthread default is CLOCK_REALTIME, which is the wall clock: an NTP step
// or a manual date change while a thread is blocked lengthens or shortens the
// timeout by however far the clock moved.  A timeout is a duration, so it
// should be measured on a clock that only counts forward.  Set at create time
// via the condattr, because pthread_cond_timedwait takes an absolute deadline
// and has no way to say which clock that deadline is on.
#if defined(CLOCK_MONOTONIC) && !defined(__APPLE__)
#define GCU_COND_CLOCK CLOCK_MONOTONIC
#else
// macOS has no pthread_condattr_setclock.  Fall back to the wall clock there
// and accept the step sensitivity rather than failing to build.
#define GCU_COND_CLOCK CLOCK_REALTIME
#endif

int gcu_cond_create(GCU_Cond * cond) {
  if (!cond) {
    return -1;
  }

#if GCU_COND_CLOCK == CLOCK_REALTIME
  return pthread_cond_init(cond, NULL) == 0 ? 0 : -1;
#else
  pthread_condattr_t attr;
  if (pthread_condattr_init(&attr) != 0) {
    return -1;
  }
  int result = -1;
  if (pthread_condattr_setclock(&attr, GCU_COND_CLOCK) == 0
      && pthread_cond_init(cond, &attr) == 0) {
    result = 0;
  }
  // Destroying the attr does not affect a condition variable already
  // initialised from it; the attr is copied, not referenced.
  pthread_condattr_destroy(&attr);
  return result;
#endif
}

int gcu_cond_destroy(GCU_Cond * cond) {
  if (!cond) {
    return -1;
  }
  return pthread_cond_destroy(cond) == 0 ? 0 : -1;
}

int gcu_cond_signal(GCU_Cond * cond) {
  if (!cond) {
    return -1;
  }
  return pthread_cond_signal(cond) == 0 ? 0 : -1;
}

int gcu_cond_broadcast(GCU_Cond * cond) {
  if (!cond) {
    return -1;
  }
  return pthread_cond_broadcast(cond) == 0 ? 0 : -1;
}

int gcu_cond_wait(GCU_Cond * cond, GCU_MUTEX_T * mutex) {
  if (!cond || !mutex) {
    return -1;
  }
  return pthread_cond_wait(cond, mutex) == 0 ? 0 : -1;
}

int gcu_cond_timedwait(GCU_Cond * cond, GCU_MUTEX_T * mutex, int timeout) {
  if (!cond || !mutex) {
    return -1;
  }

  if (timeout < 0) {
    return gcu_cond_wait(cond, mutex);
  }

  struct timespec deadline;
  if (clock_gettime(GCU_COND_CLOCK, &deadline) != 0) {
    return -1;
  }

  // Split the milliseconds rather than multiplying into tv_nsec first: a
  // timeout near INT_MAX milliseconds is about 24 days, and 2^31 * 1000000
  // overflows a 32-bit long.  tv_sec is time_t, which is where the seconds
  // belong anyway.
  deadline.tv_sec += timeout / 1000;
  deadline.tv_nsec += (long)(timeout % 1000) * 1000000L;
  if (deadline.tv_nsec >= 1000000000L) {
    deadline.tv_nsec -= 1000000000L;
    deadline.tv_sec += 1;
  }

  int rc = pthread_cond_timedwait(cond, mutex, &deadline);
  if (rc == 0) {
    return 0;
  }
  return rc == ETIMEDOUT ? GCU_COND_TIMEDOUT : -1;
}

#endif
