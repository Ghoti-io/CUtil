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
 * Atomic integers, pointers and flags.
 *
 * For the case where one thread writes a value and others read it, and taking
 * a mutex to do so would be absurd -- a counter, a "shutting down" flag, a
 * pointer published once. Anything more structured than that wants a mutex,
 * which is easier to reason about and usually not slower.
 *
 * ## Only one ordering, deliberately
 *
 * Every operation here is sequentially consistent. C11 offers six memory
 * orderings; this offers one, because the acquire/release ones are exactly as
 * fast on x86 (where they compile to the same instructions) and exactly as
 * wrong when chosen by guess, and a wrong ordering produces a bug that
 * appears on one machine and not another, under load, once.
 *
 * If you have measured that sequential consistency is your bottleneck, use
 * `<stdatomic.h>` directly and write down what you measured. That is a better
 * outcome than this file growing an `_explicit` variant of everything.
 *
 * ## These are function calls
 *
 * The operation is lock-free; the call around it is not free. That is the
 * right trade for the uses above and the wrong one inside a hot loop, where
 * the platform's own intrinsics belong.
 *
 * ## For a value computed once, prefer once.h
 *
 * The "cache a CPU feature check in an atomic int" pattern -- which two files
 * in `compress` currently hand-roll with `<stdatomic.h>` -- is what
 * gcu_once() is for. It expresses the intent, it cannot be raced into running
 * the detection twice, and it needs no ordering decision at all.
 *
 * ## Initialise before sharing
 *
 * A value is only atomic once gcu_atomic_*_init() has run, and that call is
 * not itself atomic. Initialise it before any other thread can reach it.
 * Copying one of these types after that point does not copy atomically.
 */

#ifndef GHOTI_IO_GCU_ATOMIC_H
#define GHOTI_IO_GCU_ATOMIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <ghoti.io/cutil/macros.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * An atomic 32-bit integer.
 *
 * A plain struct, not a C11 `_Atomic`: this header is included from C++,
 * where `_Atomic` does not exist and `<stdatomic.h>` is not valid. The
 * atomicity lives in the operations, in atomic.c, not in the type.
 *
 * Read and write it only through the functions below.
 */
typedef struct {
  int32_t value;   ///< Private.  Do not touch.
} GCU_Atomic_Int;

/** An atomic `size_t`, for counters and sizes. */
typedef struct {
  size_t value;    ///< Private.  Do not touch.
} GCU_Atomic_Size;

/** An atomic pointer, for publishing an object once it is fully built. */
typedef struct {
  void * value;    ///< Private.  Do not touch.
} GCU_Atomic_Ptr;

/** An atomic flag: set it, read it, clear it.  Never more than two states. */
typedef struct {
  int32_t value;   ///< Private.  Do not touch.
} GCU_Atomic_Flag;

//-----------------------------------------------------------------------------
// int32
//-----------------------------------------------------------------------------

/** Set the initial value.  Not atomic; do it before sharing. */
GCU_API void gcu_atomic_int_init(GCU_Atomic_Int * atomic, int32_t value);
/** @return The current value. */
GCU_API int32_t gcu_atomic_int_load(const GCU_Atomic_Int * atomic);
/** Replace the value. */
GCU_API void gcu_atomic_int_store(GCU_Atomic_Int * atomic, int32_t value);
/** Replace the value.  @return The value it had before. */
GCU_API int32_t gcu_atomic_int_exchange(GCU_Atomic_Int * atomic,
    int32_t value);
/** Add, wrapping on overflow.  @return The value it had before. */
GCU_API int32_t gcu_atomic_int_fetch_add(GCU_Atomic_Int * atomic,
    int32_t addend);
/** Subtract, wrapping.  @return The value it had before. */
GCU_API int32_t gcu_atomic_int_fetch_sub(GCU_Atomic_Int * atomic,
    int32_t subtrahend);
/**
 * Set to @p desired only if it currently equals @p expected.
 *
 * @param atomic The value.
 * @param expected Points to the value required; **overwritten with what was
 *   actually found** when the swap does not happen, so a retry loop can use
 *   it directly without re-reading.
 * @param desired The value to store.
 * @return true if the swap happened.
 */
GCU_API bool gcu_atomic_int_compare_exchange(GCU_Atomic_Int * atomic,
    int32_t * expected, int32_t desired);

//-----------------------------------------------------------------------------
// size_t
//-----------------------------------------------------------------------------

/** Set the initial value.  Not atomic; do it before sharing. */
GCU_API void gcu_atomic_size_init(GCU_Atomic_Size * atomic, size_t value);
/** @return The current value. */
GCU_API size_t gcu_atomic_size_load(const GCU_Atomic_Size * atomic);
/** Replace the value. */
GCU_API void gcu_atomic_size_store(GCU_Atomic_Size * atomic, size_t value);
/** Replace the value.  @return The value it had before. */
GCU_API size_t gcu_atomic_size_exchange(GCU_Atomic_Size * atomic,
    size_t value);
/** Add, wrapping on overflow.  @return The value it had before. */
GCU_API size_t gcu_atomic_size_fetch_add(GCU_Atomic_Size * atomic,
    size_t addend);
/** Subtract, wrapping.  @return The value it had before. */
GCU_API size_t gcu_atomic_size_fetch_sub(GCU_Atomic_Size * atomic,
    size_t subtrahend);
/** As gcu_atomic_int_compare_exchange(), for `size_t`. */
GCU_API bool gcu_atomic_size_compare_exchange(GCU_Atomic_Size * atomic,
    size_t * expected, size_t desired);

//-----------------------------------------------------------------------------
// pointer
//-----------------------------------------------------------------------------

/** Set the initial value.  Not atomic; do it before sharing. */
GCU_API void gcu_atomic_ptr_init(GCU_Atomic_Ptr * atomic, void * value);
/** @return The current value. */
GCU_API void * gcu_atomic_ptr_load(const GCU_Atomic_Ptr * atomic);
/** Replace the value. */
GCU_API void gcu_atomic_ptr_store(GCU_Atomic_Ptr * atomic, void * value);
/** Replace the value.  @return The value it had before. */
GCU_API void * gcu_atomic_ptr_exchange(GCU_Atomic_Ptr * atomic, void * value);
/** As gcu_atomic_int_compare_exchange(), for a pointer. */
GCU_API bool gcu_atomic_ptr_compare_exchange(GCU_Atomic_Ptr * atomic,
    void ** expected, void * desired);

//-----------------------------------------------------------------------------
// flag
//-----------------------------------------------------------------------------

/** Clear the flag.  Not atomic; do it before sharing. */
GCU_API void gcu_atomic_flag_init(GCU_Atomic_Flag * flag);
/**
 * Set the flag and report what it was.
 *
 * The primitive every other lock is built from: exactly one of the threads
 * racing here sees false.
 *
 * @return true if it was already set.
 */
GCU_API bool gcu_atomic_flag_test_and_set(GCU_Atomic_Flag * flag);
/** @return Whether the flag is set, without changing it. */
GCU_API bool gcu_atomic_flag_load(const GCU_Atomic_Flag * flag);
/** Clear the flag. */
GCU_API void gcu_atomic_flag_clear(GCU_Atomic_Flag * flag);

#ifdef __cplusplus
}
#endif

#endif // GHOTI_IO_GCU_ATOMIC_H
