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
 * Run an initialiser exactly once, whichever thread gets there first.
 *
 * Every other thread reaching the same `GCU_Once` blocks until the winner's
 * routine has returned, so a caller can rely on the initialisation being
 * complete, not merely started, the moment gcu_once() returns.
 *
 * ## Why not a constructor
 *
 * This library already has GCU_INIT_FUNCTION, which runs before `main()`.
 * That is the right tool for something that must happen unconditionally, and
 * the wrong one for anything that can fail: a constructor returns `void` to a
 * caller that does not exist. `thread.c` shows the cost -- its constructor
 * allocates a hash table, returns early if the allocation fails, and leaves
 * every one of its twelve public entry points to check for the NULL that
 * failure leaves behind.
 *
 * Constructors also have no ordering guarantee between translation units, so
 * one that depends on another's having run is relying on link order.
 *
 * gcu_once() is the lazy alternative: it runs on first use, in a defined
 * order relative to the caller, and the routine can record its own failure
 * where the caller will look for it.
 *
 * ## Usage
 *
 *     static GCU_Once ready = GCU_ONCE_INIT;
 *     static bool init_failed = false;
 *     static Thing * thing = NULL;
 *
 *     static void build_thing(void) {
 *       thing = make_thing();
 *       init_failed = thing == NULL;
 *     }
 *
 *     Thing * get_thing(void) {
 *       gcu_once(&ready, build_thing);
 *       return init_failed ? NULL : thing;
 *     }
 *
 * The routine takes no argument and returns nothing, which is the intersection
 * of what the two platforms offer. State goes where `build_thing` above puts
 * it: in variables the caller can read afterwards.
 *
 * A `GCU_Once` must have static storage duration or otherwise outlive every
 * thread that touches it, must be initialised with GCU_ONCE_INIT, and must
 * not be copied.
 *
 * If the routine itself calls gcu_once() on the same object, the result is a
 * deadlock on both platforms.  Neither detects it.
 */

#ifndef GHOTI_IO_GCU_ONCE_H
#define GHOTI_IO_GCU_ONCE_H

#include <ghoti.io/cutil/macros.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DOXYGEN
/** A one-time initialisation flag.  Initialise with GCU_ONCE_INIT. */
typedef void GCU_Once;
/** The only valid initial value for a GCU_Once. */
#define GCU_ONCE_INIT
#endif // DOXYGEN

#ifdef _WIN32
#include <windows.h>
typedef INIT_ONCE GCU_Once;
#define GCU_ONCE_INIT INIT_ONCE_STATIC_INIT
#else
#include <pthread.h>
typedef pthread_once_t GCU_Once;
#define GCU_ONCE_INIT PTHREAD_ONCE_INIT
#endif

/**
 * Run @p routine exactly once for this @p once object.
 *
 * Blocks until the routine has returned, whether this thread ran it or
 * another did.
 *
 * @param once Pointer to a GCU_Once initialised with GCU_ONCE_INIT.
 * @param routine The initialiser.  Called at most once, ever.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_once(GCU_Once * once, void (*routine)(void));

#ifdef __cplusplus
}
#endif

#endif // GHOTI_IO_GCU_ONCE_H
