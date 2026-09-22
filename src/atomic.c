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
 * Atomic operations.  See atomic.h for the contract and for why there is only
 * one memory ordering.
 *
 * GCC and Clang use the `__atomic_*` builtins rather than `<stdatomic.h>`,
 * because those operate on **ordinary** objects.  That is what lets the public
 * types be plain structs a C++ consumer can include: `_Atomic` is not a C++
 * type and `<stdatomic.h>` is not a C++ header, so a type spelled with either
 * could not appear in a header this suite's tests include.
 *
 * MSVC has no such builtins in C mode and uses the Interlocked family.
 */

#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/atomic.h>

#if defined(_MSC_VER) && !defined(__clang__)

#include <windows.h>

// Interlocked operates on LONG and LONG64.  Every cast below is between
// same-width integer types, checked by the static assertions here rather than
// assumed: a silent width mismatch would make the operation non-atomic on
// exactly the platform that cannot be tested from this workspace.
typedef char gcu_atomic_check_long[sizeof(LONG) == sizeof(int32_t) ? 1 : -1];
typedef char gcu_atomic_check_ptr[sizeof(PVOID) == sizeof(void *) ? 1 : -1];

// There is no Interlocked "load".  A compare-exchange against zero returns
// the current value and writes only if it was already zero, i.e. writes the
// value it already had -- so it reads atomically with a full barrier.  A
// plain volatile read would be adequate on x86 and wrong on ARM.
#define GCU_ATOMIC_LOAD32(p)  InterlockedCompareExchange((volatile LONG *)(p), 0, 0)
#define GCU_ATOMIC_LOAD64(p)  InterlockedCompareExchange64((volatile LONG64 *)(p), 0, 0)

void gcu_atomic_int_init(GCU_Atomic_Int * atomic, int32_t value) {
  atomic->value = value;
}

int32_t gcu_atomic_int_load(const GCU_Atomic_Int * atomic) {
  return (int32_t)GCU_ATOMIC_LOAD32(&atomic->value);
}

void gcu_atomic_int_store(GCU_Atomic_Int * atomic, int32_t value) {
  InterlockedExchange((volatile LONG *)&atomic->value, (LONG)value);
}

int32_t gcu_atomic_int_exchange(GCU_Atomic_Int * atomic, int32_t value) {
  return (int32_t)InterlockedExchange((volatile LONG *)&atomic->value,
      (LONG)value);
}

int32_t gcu_atomic_int_fetch_add(GCU_Atomic_Int * atomic, int32_t addend) {
  return (int32_t)InterlockedExchangeAdd((volatile LONG *)&atomic->value,
      (LONG)addend);
}

int32_t gcu_atomic_int_fetch_sub(GCU_Atomic_Int * atomic, int32_t subtrahend) {
  // Negating in the unsigned domain: -INT32_MIN overflows a signed int32 and
  // is undefined, and a subtract-by-INT32_MIN is a legitimate call.
  uint32_t negated = 0u - (uint32_t)subtrahend;
  return (int32_t)InterlockedExchangeAdd((volatile LONG *)&atomic->value,
      (LONG)negated);
}

bool gcu_atomic_int_compare_exchange(GCU_Atomic_Int * atomic,
    int32_t * expected, int32_t desired) {
  LONG found = InterlockedCompareExchange((volatile LONG *)&atomic->value,
      (LONG)desired, (LONG)*expected);
  if ((int32_t)found == *expected) {
    return true;
  }
  *expected = (int32_t)found;
  return false;
}

void gcu_atomic_size_init(GCU_Atomic_Size * atomic, size_t value) {
  atomic->value = value;
}

size_t gcu_atomic_size_load(const GCU_Atomic_Size * atomic) {
  return (size_t)GCU_ATOMIC_LOAD64(&atomic->value);
}

void gcu_atomic_size_store(GCU_Atomic_Size * atomic, size_t value) {
  InterlockedExchange64((volatile LONG64 *)&atomic->value, (LONG64)value);
}

size_t gcu_atomic_size_exchange(GCU_Atomic_Size * atomic, size_t value) {
  return (size_t)InterlockedExchange64((volatile LONG64 *)&atomic->value,
      (LONG64)value);
}

size_t gcu_atomic_size_fetch_add(GCU_Atomic_Size * atomic, size_t addend) {
  return (size_t)InterlockedExchangeAdd64((volatile LONG64 *)&atomic->value,
      (LONG64)addend);
}

size_t gcu_atomic_size_fetch_sub(GCU_Atomic_Size * atomic, size_t subtrahend) {
  size_t negated = (size_t)0 - subtrahend;
  return (size_t)InterlockedExchangeAdd64((volatile LONG64 *)&atomic->value,
      (LONG64)negated);
}

bool gcu_atomic_size_compare_exchange(GCU_Atomic_Size * atomic,
    size_t * expected, size_t desired) {
  LONG64 found = InterlockedCompareExchange64((volatile LONG64 *)&atomic->value,
      (LONG64)desired, (LONG64)*expected);
  if ((size_t)found == *expected) {
    return true;
  }
  *expected = (size_t)found;
  return false;
}

void gcu_atomic_ptr_init(GCU_Atomic_Ptr * atomic, void * value) {
  atomic->value = value;
}

void * gcu_atomic_ptr_load(const GCU_Atomic_Ptr * atomic) {
  return InterlockedCompareExchangePointer((PVOID volatile *)&atomic->value,
      NULL, NULL);
}

void gcu_atomic_ptr_store(GCU_Atomic_Ptr * atomic, void * value) {
  InterlockedExchangePointer((PVOID volatile *)&atomic->value, value);
}

void * gcu_atomic_ptr_exchange(GCU_Atomic_Ptr * atomic, void * value) {
  return InterlockedExchangePointer((PVOID volatile *)&atomic->value, value);
}

bool gcu_atomic_ptr_compare_exchange(GCU_Atomic_Ptr * atomic,
    void ** expected, void * desired) {
  PVOID found = InterlockedCompareExchangePointer(
      (PVOID volatile *)&atomic->value, desired, *expected);
  if (found == *expected) {
    return true;
  }
  *expected = found;
  return false;
}

void gcu_atomic_flag_init(GCU_Atomic_Flag * flag) {
  flag->value = 0;
}

bool gcu_atomic_flag_test_and_set(GCU_Atomic_Flag * flag) {
  return InterlockedExchange((volatile LONG *)&flag->value, 1) != 0;
}

bool gcu_atomic_flag_load(const GCU_Atomic_Flag * flag) {
  return GCU_ATOMIC_LOAD32(&flag->value) != 0;
}

void gcu_atomic_flag_clear(GCU_Atomic_Flag * flag) {
  InterlockedExchange((volatile LONG *)&flag->value, 0);
}

#else

// __atomic_* rather than <stdatomic.h>: these operate on ordinary objects, so
// the public types stay plain structs that a C++ translation unit can include.
// See the file comment.
#define GCU_SEQ __ATOMIC_SEQ_CST

// The builtins take a non-const pointer even for a load, and a load of a
// const object is a reasonable thing for a caller to want -- so the const is
// cast away here, in one place, rather than removed from the public
// signatures. The object is not modified.
#define GCU_UNCONST(T, p) ((T *)(size_t)(const void *)(p))

void gcu_atomic_int_init(GCU_Atomic_Int * atomic, int32_t value) {
  atomic->value = value;
}

int32_t gcu_atomic_int_load(const GCU_Atomic_Int * atomic) {
  return __atomic_load_n(GCU_UNCONST(int32_t, &atomic->value), GCU_SEQ);
}

void gcu_atomic_int_store(GCU_Atomic_Int * atomic, int32_t value) {
  __atomic_store_n(&atomic->value, value, GCU_SEQ);
}

int32_t gcu_atomic_int_exchange(GCU_Atomic_Int * atomic, int32_t value) {
  return __atomic_exchange_n(&atomic->value, value, GCU_SEQ);
}

int32_t gcu_atomic_int_fetch_add(GCU_Atomic_Int * atomic, int32_t addend) {
  return __atomic_fetch_add(&atomic->value, addend, GCU_SEQ);
}

int32_t gcu_atomic_int_fetch_sub(GCU_Atomic_Int * atomic, int32_t subtrahend) {
  return __atomic_fetch_sub(&atomic->value, subtrahend, GCU_SEQ);
}

bool gcu_atomic_int_compare_exchange(GCU_Atomic_Int * atomic,
    int32_t * expected, int32_t desired) {
  // weak=false: a spurious failure would be correct for a caller looping on
  // the result and a silent defect for one that is not, and this API does not
  // force a loop.
  return __atomic_compare_exchange_n(&atomic->value, expected, desired,
      false, GCU_SEQ, GCU_SEQ);
}

void gcu_atomic_size_init(GCU_Atomic_Size * atomic, size_t value) {
  atomic->value = value;
}

size_t gcu_atomic_size_load(const GCU_Atomic_Size * atomic) {
  return __atomic_load_n(GCU_UNCONST(size_t, &atomic->value), GCU_SEQ);
}

void gcu_atomic_size_store(GCU_Atomic_Size * atomic, size_t value) {
  __atomic_store_n(&atomic->value, value, GCU_SEQ);
}

size_t gcu_atomic_size_exchange(GCU_Atomic_Size * atomic, size_t value) {
  return __atomic_exchange_n(&atomic->value, value, GCU_SEQ);
}

size_t gcu_atomic_size_fetch_add(GCU_Atomic_Size * atomic, size_t addend) {
  return __atomic_fetch_add(&atomic->value, addend, GCU_SEQ);
}

size_t gcu_atomic_size_fetch_sub(GCU_Atomic_Size * atomic,
    size_t subtrahend) {
  return __atomic_fetch_sub(&atomic->value, subtrahend, GCU_SEQ);
}

bool gcu_atomic_size_compare_exchange(GCU_Atomic_Size * atomic,
    size_t * expected, size_t desired) {
  return __atomic_compare_exchange_n(&atomic->value, expected, desired,
      false, GCU_SEQ, GCU_SEQ);
}

void gcu_atomic_ptr_init(GCU_Atomic_Ptr * atomic, void * value) {
  atomic->value = value;
}

void * gcu_atomic_ptr_load(const GCU_Atomic_Ptr * atomic) {
  return __atomic_load_n(GCU_UNCONST(void *, &atomic->value), GCU_SEQ);
}

void gcu_atomic_ptr_store(GCU_Atomic_Ptr * atomic, void * value) {
  __atomic_store_n(&atomic->value, value, GCU_SEQ);
}

void * gcu_atomic_ptr_exchange(GCU_Atomic_Ptr * atomic, void * value) {
  return __atomic_exchange_n(&atomic->value, value, GCU_SEQ);
}

bool gcu_atomic_ptr_compare_exchange(GCU_Atomic_Ptr * atomic,
    void ** expected, void * desired) {
  return __atomic_compare_exchange_n(&atomic->value, expected, desired,
      false, GCU_SEQ, GCU_SEQ);
}

void gcu_atomic_flag_init(GCU_Atomic_Flag * flag) {
  flag->value = 0;
}

bool gcu_atomic_flag_test_and_set(GCU_Atomic_Flag * flag) {
  return __atomic_exchange_n(&flag->value, 1, GCU_SEQ) != 0;
}

bool gcu_atomic_flag_load(const GCU_Atomic_Flag * flag) {
  return __atomic_load_n(GCU_UNCONST(int32_t, &flag->value), GCU_SEQ) != 0;
}

void gcu_atomic_flag_clear(GCU_Atomic_Flag * flag) {
  __atomic_store_n(&flag->value, 0, GCU_SEQ);
}

#endif
