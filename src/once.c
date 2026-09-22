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
 * Run an initialiser exactly once.  See once.h for the contract.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200112L
#endif

#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/once.h>

#ifdef _WIN32

// InitOnceExecuteOnce's callback takes three arguments and returns BOOL,
// where pthread_once's takes none and returns void.  The caller's routine is
// handed through the Parameter slot and called by this adapter, so that the
// public signature is the intersection of the two platforms rather than the
// union.
//
// The routine travels as a void* and comes back as a function pointer.  C
// does not define that conversion, so it goes through a union rather than a
// cast: a union member read is implementation-defined, which is a weaker
// claim than undefined, and this is the narrowest place to make it.
typedef union {
  void * object;
  void (*routine)(void);
} gcu_once_pun;

static BOOL CALLBACK gcu_once_adapter(PINIT_ONCE once, PVOID parameter,
    PVOID * context) {
  (void)once;
  (void)context;

  gcu_once_pun pun;
  pun.object = parameter;
  pun.routine();
  return TRUE;
}

int gcu_once(GCU_Once * once, void (*routine)(void)) {
  if (!once || !routine) {
    return -1;
  }

  gcu_once_pun pun;
  pun.routine = routine;
  return InitOnceExecuteOnce(once, gcu_once_adapter, pun.object, NULL)
      ? 0 : -1;
}

#else

int gcu_once(GCU_Once * once, void (*routine)(void)) {
  if (!once || !routine) {
    return -1;
  }
  return pthread_once(once, routine) == 0 ? 0 : -1;
}

#endif
