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
 * Implementation of the growable fixed-size-element array declared in
 * cutil/array.h.
 */

#include <string.h>
#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/array.h>
#include <ghoti.io/cutil/safemath.h>

/// Smallest allocation made for a lazily-grown array, in elements.
#define GCU_ARRAY_MIN_CAPACITY 8

/**
 * Pick the next capacity for an array that needs to hold `needed` elements.
 *
 * Growth is 1.5x rather than 2x: it keeps reallocation amortized while
 * letting freed blocks be reused by later growth, and it is computed in
 * integers so there is no rounding to reason about.  When 1.5x is not enough
 * (a bulk append), the requested size is used directly.
 */
static size_t next_capacity(size_t current, size_t needed) {
  size_t candidate = current < GCU_ARRAY_MIN_CAPACITY
    ? GCU_ARRAY_MIN_CAPACITY
    : current;

  // candidate + candidate/2, guarded against wrapping on absurd capacities.
  size_t grown;
  if (gcu_safe_add_size(candidate, candidate / 2, &grown)) {
    candidate = grown;
  }

  return candidate < needed ? needed : candidate;
}

/**
 * Reallocate `array->data` to hold exactly `capacity` elements.
 *
 * Newly exposed bytes are left uninitialized; callers that expose them to
 * users zero them themselves.
 */
static bool set_capacity(GCU_Array * array, size_t capacity) {
  if (capacity == array->capacity) {
    return true;
  }

  size_t bytes;
  if (!gcu_safe_mul_size(capacity, array->element_size, &bytes)) {
    return false;
  }

  if (capacity == 0) {
    gcu_allocator_free(array->allocator, array->data);
    array->data = NULL;
    array->capacity = 0;
    return true;
  }

  void * grown = gcu_allocator_realloc(array->allocator, array->data, bytes);
  if (!grown) {
    return false;
  }

  array->data = grown;
  array->capacity = capacity;
  return true;
}

/// Byte offset of an element index.  The caller must have range-checked it.
static void * element_ptr(const GCU_Array * array, size_t index) {
  return (char *)array->data + (index * array->element_size);
}

GCU_Array * gcu_array_create(
  size_t element_size, size_t count, const GCU_Allocator * allocator) {
  if (!element_size) {
    return NULL;
  }

  GCU_Array * array =
    (GCU_Array *)gcu_allocator_malloc(allocator, sizeof(GCU_Array));
  if (!array) {
    return NULL;
  }

  if (!gcu_array_create_in_place(array, element_size, count, allocator)) {
    gcu_allocator_free(allocator, array);
    return NULL;
  }

  return array;
}

bool gcu_array_create_in_place(GCU_Array * array, size_t element_size,
  size_t count, const GCU_Allocator * allocator) {
  if (!array) {
    return false;
  }

  *array = (GCU_Array){
    .element_size = element_size,
    .count = 0,
    .capacity = 0,
    .data = NULL,
    .allocator = allocator,
    .supplementary_data = NULL,
    .cleanup = NULL,
  };

  if (!element_size) {
    // The struct is already zeroed, so destroy_in_place() remains safe, but
    // refuse to hand back an array whose element size can never be satisfied.
    return false;
  }

  if (count && !set_capacity(array, count)) {
    return false;
  }

  return true;
}

void gcu_array_destroy(GCU_Array * array) {
  if (!array) {
    return;
  }
  const GCU_Allocator * allocator = array->allocator;
  gcu_array_destroy_in_place(array);
  gcu_allocator_free(allocator, array);
}

void gcu_array_destroy_in_place(GCU_Array * array) {
  if (!array) {
    return;
  }
  if (array->cleanup) {
    array->cleanup(array);
    // A cleanup hook that destroys the array itself would otherwise leave us
    // freeing a stale pointer below.
    array->cleanup = NULL;
  }
  gcu_allocator_free(array->allocator, array->data);
  array->data = NULL;
  array->count = 0;
  array->capacity = 0;
}

bool gcu_array_reserve(GCU_Array * array, size_t count) {
  if (!array || !array->element_size) {
    return false;
  }
  if (count <= array->capacity) {
    return true;
  }
  return set_capacity(array, count);
}

bool gcu_array_resize(GCU_Array * array, size_t count) {
  if (!array || !array->element_size) {
    return false;
  }

  if (count <= array->count) {
    array->count = count;
    return true;
  }

  if (!gcu_array_reserve(array, count)) {
    return false;
  }

  memset(element_ptr(array, array->count), 0,
    (count - array->count) * array->element_size);
  array->count = count;
  return true;
}

bool gcu_array_shrink_to_fit(GCU_Array * array) {
  if (!array || !array->element_size) {
    return false;
  }
  if (array->count == array->capacity) {
    return true;
  }
  return set_capacity(array, array->count);
}

void gcu_array_clear(GCU_Array * array) {
  if (array) {
    array->count = 0;
  }
}

void * gcu_array_emplace_n(GCU_Array * array, size_t n) {
  if (!array || !array->element_size) {
    return NULL;
  }

  size_t needed;
  if (!gcu_safe_add_size(array->count, n, &needed)) {
    return NULL;
  }

  if (needed > array->capacity) {
    if (!set_capacity(array, next_capacity(array->capacity, needed))) {
      // Fall back to the exact size: a 1.5x request can fail on a large array
      // when the precise one would have succeeded.
      if (!set_capacity(array, needed)) {
        return NULL;
      }
    }
  }

  if (!array->data) {
    // n == 0 on an array that was never allocated.  There is no valid
    // one-past-the-end pointer to return.
    return NULL;
  }

  void * first = element_ptr(array, array->count);
  if (n) {
    memset(first, 0, n * array->element_size);
    array->count = needed;
  }
  return first;
}

void * gcu_array_emplace(GCU_Array * array) {
  return gcu_array_emplace_n(array, 1);
}

bool gcu_array_append(GCU_Array * array, const void * element) {
  if (!element) {
    return false;
  }
  void * slot = gcu_array_emplace_n(array, 1);
  if (!slot) {
    return false;
  }
  memcpy(slot, element, array->element_size);
  return true;
}

bool gcu_array_append_n(GCU_Array * array, const void * elements, size_t n) {
  if (!array || !array->element_size) {
    return false;
  }
  if (!n) {
    return true;
  }
  if (!elements) {
    return false;
  }
  void * slot = gcu_array_emplace_n(array, n);
  if (!slot) {
    return false;
  }
  memcpy(slot, elements, n * array->element_size);
  return true;
}

bool gcu_array_pop(GCU_Array * array, void * out) {
  if (!array || !array->count) {
    return false;
  }
  array->count--;
  if (out) {
    memcpy(out, element_ptr(array, array->count), array->element_size);
  }
  return true;
}

void * gcu_array_at(const GCU_Array * array, size_t index) {
  if (!array || index >= array->count) {
    return NULL;
  }
  return element_ptr(array, index);
}

void * gcu_array_back(const GCU_Array * array) {
  if (!array || !array->count) {
    return NULL;
  }
  return element_ptr(array, array->count - 1);
}

bool gcu_array_remove_at(GCU_Array * array, size_t index, void * out) {
  if (!array || index >= array->count) {
    return false;
  }

  void * slot = element_ptr(array, index);
  if (out) {
    memcpy(out, slot, array->element_size);
  }

  size_t trailing = array->count - index - 1;
  if (trailing) {
    memmove(slot, (char *)slot + array->element_size,
      trailing * array->element_size);
  }
  array->count--;
  return true;
}

bool gcu_array_swap_remove(GCU_Array * array, size_t index, void * out) {
  if (!array || index >= array->count) {
    return false;
  }

  void * slot = element_ptr(array, index);
  if (out) {
    memcpy(out, slot, array->element_size);
  }

  array->count--;
  if (index != array->count) {
    memcpy(slot, element_ptr(array, array->count), array->element_size);
  }
  return true;
}

size_t gcu_array_count(const GCU_Array * array) {
  return array ? array->count : 0;
}

void * gcu_array_steal(GCU_Array * array, size_t * out_count) {
  if (!array) {
    if (out_count) {
      *out_count = 0;
    }
    return NULL;
  }

  void * data = array->data;
  if (out_count) {
    *out_count = array->count;
  }

  array->data = NULL;
  array->count = 0;
  array->capacity = 0;
  return data;
}
