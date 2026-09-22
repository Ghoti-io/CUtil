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
 * Cross-platform read-write locks.  See rwlock.h for the contract.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200112L
#endif

#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/rwlock.h>

#ifdef _WIN32

int gcu_rwlock_create(GCU_RWLock * lock) {
  if (!lock) {
    return -1;
  }
  InitializeSRWLock(lock);
  return 0;
}

int gcu_rwlock_destroy(GCU_RWLock * lock) {
  // An SRWLOCK owns nothing.  The call exists so that code written against
  // this API is correct on POSIX too, where the destroy is real.
  return lock ? 0 : -1;
}

int gcu_rwlock_read_lock(GCU_RWLock * lock) {
  if (!lock) {
    return -1;
  }
  AcquireSRWLockShared(lock);
  return 0;
}

int gcu_rwlock_read_trylock(GCU_RWLock * lock) {
  if (!lock) {
    return -1;
  }
  // Returns non-zero when it acquired, which is backwards from this
  // library's zero-on-success convention.
  return TryAcquireSRWLockShared(lock) ? 0 : 1;
}

int gcu_rwlock_read_unlock(GCU_RWLock * lock) {
  if (!lock) {
    return -1;
  }
  ReleaseSRWLockShared(lock);
  return 0;
}

int gcu_rwlock_write_lock(GCU_RWLock * lock) {
  if (!lock) {
    return -1;
  }
  AcquireSRWLockExclusive(lock);
  return 0;
}

int gcu_rwlock_write_trylock(GCU_RWLock * lock) {
  if (!lock) {
    return -1;
  }
  return TryAcquireSRWLockExclusive(lock) ? 0 : 1;
}

int gcu_rwlock_write_unlock(GCU_RWLock * lock) {
  if (!lock) {
    return -1;
  }
  ReleaseSRWLockExclusive(lock);
  return 0;
}

#else

int gcu_rwlock_create(GCU_RWLock * lock) {
  if (!lock) {
    return -1;
  }
  return pthread_rwlock_init(lock, NULL) == 0 ? 0 : -1;
}

int gcu_rwlock_destroy(GCU_RWLock * lock) {
  if (!lock) {
    return -1;
  }
  return pthread_rwlock_destroy(lock) == 0 ? 0 : -1;
}

int gcu_rwlock_read_lock(GCU_RWLock * lock) {
  if (!lock) {
    return -1;
  }
  return pthread_rwlock_rdlock(lock) == 0 ? 0 : -1;
}

int gcu_rwlock_read_trylock(GCU_RWLock * lock) {
  if (!lock) {
    return -1;
  }
  // Non-zero on failure already (EBUSY), which is the convention.
  return pthread_rwlock_tryrdlock(lock) == 0 ? 0 : 1;
}

// POSIX records which mode the holder took, so one unlock serves both.  The
// two entry points exist because Windows cannot do that; see rwlock.h.
int gcu_rwlock_read_unlock(GCU_RWLock * lock) {
  if (!lock) {
    return -1;
  }
  return pthread_rwlock_unlock(lock) == 0 ? 0 : -1;
}

int gcu_rwlock_write_lock(GCU_RWLock * lock) {
  if (!lock) {
    return -1;
  }
  return pthread_rwlock_wrlock(lock) == 0 ? 0 : -1;
}

int gcu_rwlock_write_trylock(GCU_RWLock * lock) {
  if (!lock) {
    return -1;
  }
  return pthread_rwlock_trywrlock(lock) == 0 ? 0 : 1;
}

int gcu_rwlock_write_unlock(GCU_RWLock * lock) {
  if (!lock) {
    return -1;
  }
  return pthread_rwlock_unlock(lock) == 0 ? 0 : -1;
}

#endif
