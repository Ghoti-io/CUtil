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
 * A growable array of fixed-size elements.
 *
 * `GCU_Array` stores elements *by value*, whatever their size, which is what
 * separates it from @ref GCU_Vector64 and its siblings: those hold a
 * `GCU_TypeN_Union` per slot and so can only carry pointers and small
 * scalars.  Reach for `GCU_Array` when the elements are structs (vertices,
 * table rows, parser stack frames, diagnostics) and for `GCU_VectorN` when
 * they really are machine words.
 *
 * Memory comes from a caller-supplied @ref GCU_Allocator, so a library that
 * already exposes pluggable allocation can hand its own allocator straight
 * through.  Passing `NULL` uses gcu_allocator_default().
 *
 * The array does **not** own its elements.  If they hold pointers, freeing
 * those is the caller's job; the `cleanup` hook is called on destruction to
 * make that convenient.
 *
 * There is no internal locking, and no mutex field.  Callers that share an
 * array across threads must synchronize externally.
 */

#ifndef GHOTI_IO_GCU_ARRAY_H
#define GHOTI_IO_GCU_ARRAY_H

#include <stdbool.h>
#include <stddef.h>
#include <ghoti.io/cutil/allocator.h>
#include <ghoti.io/cutil/macros.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct GCU_Array GCU_Array;

/**
 * Pointer to a function called when an array is destroyed, before its
 * backing storage is released.
 *
 * Use it to release anything the elements themselves own.  The array's
 * `count` and `data` are still valid when it runs.
 *
 * @param array The array about to be destroyed.
 */
typedef void (* GCU_Array_Cleanup)(GCU_Array * array);

/**
 * A growable array of fixed-size elements.
 *
 * `data` may be read and written directly for bulk work; treat `count` and
 * `capacity` as read-only unless you are prepared to maintain the invariant
 * `count <= capacity` yourself.  Any pointer into `data` is invalidated by
 * an operation that grows the array.
 */
struct GCU_Array {
  size_t element_size;             ///< Bytes per element.  Never zero.
  size_t count;                    ///< Elements currently stored.
  size_t capacity;                 ///< Elements that fit without reallocating.
  void * data;                     ///< Backing storage, or NULL when empty.
  const GCU_Allocator * allocator; ///< Allocator used for `data`.
  void * supplementary_data;       ///< User-defined.
  GCU_Array_Cleanup cleanup;       ///< User-defined destruction hook.
};

/**
 * Allocate and initialize an array.
 *
 * @param element_size Bytes per element.  Must be non-zero.
 * @param count Elements to reserve up front; 0 to allocate lazily.
 * @param allocator The allocator to use, or `NULL` for the default.  It must
 *   outlive the array.
 * @return The new array, or `NULL` on failure.
 */
GCU_API GCU_Array * gcu_array_create(
  size_t element_size, size_t count, const GCU_Allocator * allocator);

/**
 * Initialize an array in caller-provided memory.
 *
 * Use this for an array embedded in another struct.  Pair it with
 * gcu_array_destroy_in_place().
 *
 * @param array The memory to initialize.
 * @param element_size Bytes per element.  Must be non-zero.
 * @param count Elements to reserve up front; 0 to allocate lazily.
 * @param allocator The allocator to use, or `NULL` for the default.  It must
 *   outlive the array.
 * @return `true` on success.  On failure the array is left zeroed and safe to
 *   pass to gcu_array_destroy_in_place().
 */
GCU_API bool gcu_array_create_in_place(GCU_Array * array, size_t element_size,
  size_t count, const GCU_Allocator * allocator);

/**
 * Destroy an array created by gcu_array_create() and free the struct itself.
 *
 * Calls `cleanup` first, if set.  `NULL` is ignored.
 *
 * @param array The array to destroy.
 */
GCU_API void gcu_array_destroy(GCU_Array * array);

/**
 * Release an array's storage without freeing the struct.
 *
 * Calls `cleanup` first, if set.  Safe to call twice, and safe on a zeroed
 * struct.
 *
 * @param array The array to tear down.
 */
GCU_API void gcu_array_destroy_in_place(GCU_Array * array);

/**
 * Ensure the array can hold at least `count` elements without reallocating.
 *
 * Never shrinks.  On failure the array is unchanged.
 *
 * @param array The array to operate on.
 * @param count The capacity required.
 * @return `true` on success, `false` on overflow or allocation failure.
 */
GCU_API bool gcu_array_reserve(GCU_Array * array, size_t count);

/**
 * Set the element count, zero-filling any new elements.
 *
 * Growing reserves as needed; shrinking keeps the existing capacity and does
 * not run `cleanup` over the discarded elements.
 *
 * @param array The array to operate on.
 * @param count The new element count.
 * @return `true` on success, `false` on overflow or allocation failure.
 */
GCU_API bool gcu_array_resize(GCU_Array * array, size_t count);

/**
 * Release capacity beyond the current count.
 *
 * A failure to shrink is not an error the caller needs to act on: the array
 * remains valid at its previous capacity.
 *
 * @param array The array to operate on.
 * @return `true` if the capacity now equals the count.
 */
GCU_API bool gcu_array_shrink_to_fit(GCU_Array * array);

/**
 * Set the count to zero, keeping the allocated capacity.
 *
 * Does not run `cleanup`.
 *
 * @param array The array to operate on.
 */
GCU_API void gcu_array_clear(GCU_Array * array);

/**
 * Copy one element onto the end of the array.
 *
 * @param array The array to operate on.
 * @param element The element to copy.  Must be at least `element_size` bytes.
 * @return `true` on success, `false` on overflow or allocation failure.
 */
GCU_API bool gcu_array_append(GCU_Array * array, const void * element);

/**
 * Copy `n` contiguous elements onto the end of the array.
 *
 * Either all of them are appended or none are.
 *
 * @param array The array to operate on.
 * @param elements The elements to copy.  May be `NULL` only when `n` is 0.
 * @param n The number of elements to copy.
 * @return `true` on success, `false` on overflow or allocation failure.
 */
GCU_API bool gcu_array_append_n(GCU_Array * array, const void * elements, size_t n);

/**
 * Grow the array by one zeroed element and return a pointer to it.
 *
 * This is the natural replacement for the `if (count == capacity) grow();
 * items[count++] = value;` idiom: the caller writes the new element in place
 * and never has to size the allocation.
 *
 * The returned pointer is invalidated by the next operation that grows the
 * array.
 *
 * @param array The array to operate on.
 * @return A pointer to the new element, or `NULL` on failure (in which case
 *   the count is unchanged).
 */
GCU_API void * gcu_array_emplace(GCU_Array * array);

/**
 * Grow the array by `n` zeroed elements and return a pointer to the first.
 *
 * @param array The array to operate on.
 * @param n The number of elements to add.
 * @return A pointer to the first new element, or `NULL` on failure (in which
 *   case the count is unchanged).  Returns a non-`NULL` pointer to the end of
 *   the array when `n` is 0 and the array has storage.
 */
GCU_API void * gcu_array_emplace_n(GCU_Array * array, size_t n);

/**
 * Grow the array by `n` elements whose contents are unspecified, and return a
 * pointer to the first.
 *
 * This is gcu_array_emplace_n() without the zeroing, for the caller who is
 * about to write every byte of the span anyway - building into an output
 * buffer, decoding into a run of elements, rendering a glyph run.  For that
 * caller the zeroing is a write of the whole span that the next statement
 * discards.
 *
 * The bytes are not merely unset but genuinely unspecified: the array reuses
 * its storage, so they are as likely to be a previous element as they are to
 * be zero.  **Reading one before writing it is a bug**, and one that ordinary
 * testing hides, because a fresh allocation usually happens to be zero.  If
 * the caller fills in some fields and leaves others, it wants
 * gcu_array_emplace_n() instead; the zeroing is the safe default and stays
 * the default.
 *
 * @param array The array to operate on.
 * @param n The number of elements to add.
 * @return A pointer to the first new element, or `NULL` on failure (in which
 *   case the count is unchanged).  Returns a non-`NULL` pointer to the end of
 *   the array when `n` is 0 and the array has storage.
 */
GCU_API void * gcu_array_extend_n(GCU_Array * array, size_t n);

/**
 * Remove the last element, optionally copying it out.
 *
 * @param array The array to operate on.
 * @param out Receives a copy of the removed element; may be `NULL` to
 *   discard it.
 * @return `true` if an element was removed, `false` if the array was empty.
 */
GCU_API bool gcu_array_pop(GCU_Array * array, void * out);

/**
 * Get a pointer to the element at an index.
 *
 * @param array The array to operate on.
 * @param index The zero-based index.
 * @return A pointer to the element, or `NULL` if the index is out of range.
 */
GCU_API void * gcu_array_at(const GCU_Array * array, size_t index);

/**
 * Get a pointer to the last element.
 *
 * Parsers that keep a stack in an array read the top constantly; this saves
 * writing `gcu_array_at(a, a->count - 1)` with its underflow hazard.
 *
 * @param array The array to operate on.
 * @return A pointer to the last element, or `NULL` if the array is empty.
 */
GCU_API void * gcu_array_back(const GCU_Array * array);

/**
 * Remove the element at an index, shifting the remainder down.
 *
 * Preserves order, and costs O(count - index).
 *
 * @param array The array to operate on.
 * @param index The zero-based index to remove.
 * @param out Receives a copy of the removed element; may be `NULL`.
 * @return `true` on success, `false` if the index is out of range.
 */
GCU_API bool gcu_array_remove_at(GCU_Array * array, size_t index, void * out);

/**
 * Remove the element at an index by moving the last element into its place.
 *
 * Does not preserve order, and costs O(1).
 *
 * @param array The array to operate on.
 * @param index The zero-based index to remove.
 * @param out Receives a copy of the removed element; may be `NULL`.
 * @return `true` on success, `false` if the index is out of range.
 */
GCU_API bool gcu_array_swap_remove(GCU_Array * array, size_t index, void * out);

/**
 * Get the number of elements in the array.
 *
 * @param array The array to operate on; `NULL` counts as empty.
 * @return The element count.
 */
GCU_API size_t gcu_array_count(const GCU_Array * array);

/**
 * Hand the backing storage to the caller and reset the array to empty.
 *
 * For code that builds a result with an array and then returns a plain
 * pointer and length, as the format parsers do.  The caller becomes
 * responsible for releasing the block through the same allocator.
 *
 * @param array The array to empty.
 * @param out_count Receives the element count; may be `NULL`.
 * @return The storage, or `NULL` if the array was empty.
 */
GCU_API void * gcu_array_steal(GCU_Array * array, size_t * out_count);

#ifdef __cplusplus
}
#endif

#endif //GHOTI_IO_GCU_ARRAY_H
