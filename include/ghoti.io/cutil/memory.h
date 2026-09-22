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
 * Header file for memory-related functions.
 *
 * For cross-platform memory functions, use the gcu_malloc(), gcu_calloc(),
 * gcu_realloc(), and gcu_free() in this library.
 *
 * To enable logging and debugging, define `GHOTIIO_CUTIL_ENABLE_MEMORY_DEBUG`
 * before including this file.  Then, all code compiled with this option will
 * have memory logging enabled.
 *
 * Logging to `stderr` is enabled by default when the afore-mention define is
 * enabled.  It may be disabled by calling gcu_mem_stop(), and re-enabled by
 * calling gcu_mem_start().
 *
 * You may need to control the logging, but also need to control when the
 * logging starts and stops externally.  Obviously, if this header is included,
 * then memory management will also be logged, but this feature can be modified
 * by the use of a `#define` *before* including the header.
 */

#ifndef GHOTI_IO_GCU_MEMORY_H
#define GHOTI_IO_GCU_MEMORY_H

#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <stdlib.h>
#endif

#include <ghoti.io/cutil/macros.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Instruct Ghoti.io CUtils library that intercepted memory management calls
 * should be logged to stderr.
 */
GCU_API void gcu_mem_start(void);

/**
 * Instruct Ghoti.io CUtils library that intercepted memory management calls
 * should no longer be logged to stderr.
 */
GCU_API void gcu_mem_stop(void);

/**
 * Cross-platform wrapper for the standard malloc() function.
 *
 * This function should not be called directly. Call gcu_malloc() instead.
 *
 * @param size The number of bytes requested.
 * @param file The name of the file from which the function was called.
 * @param line The line number on which the function was called.
 * @returns The beginning byte of the allocated memory.
 */
GCU_API void * gcu_malloc_debug(size_t size, const char * file, size_t line);

/**
 * Cross-platform wrapper for the standard calloc() function.
 *
 * This function should not be called directly. Call gcu_calloc() instead.
 *
 * @param nitems The number of items to allocate.
 * @param size The number of bytes in each item.
 * @param file The name of the file from which the function was called.
 * @param line The line number on which the function was called.
 * @returns The beginning byte of the allocated memory.
 */
GCU_API void * gcu_calloc_debug(size_t nitems, size_t size, const char * file, size_t line);

/**
 * Cross-platform wrapper for the standard realloc() function.
 *
 * This function should not be called directly. Call gcu_realloc() instead.
 *
 * @param pointer The beginning byte of the currently allocated memory.
 * @param size The newly requested size.
 * @param file The name of the file from which the function was called.
 * @param line The line number on which the function was called.
 * @returns The beginning byte of the reallocated memory.
 */
GCU_API void * gcu_realloc_debug(void * pointer, size_t size, const char * file, size_t line);

/**
 * Wrapper for the standard free() function.
 *
 * This function should not be called directly. Call gcu_free() instead.
 *
 * @param pointer The beginning byte of the currently allocated memory.
 * @param file The name of the file from which the function was called.
 * @param line The line number on which the function was called.
 */
GCU_API void gcu_free_debug(void * pointer, const char * file, size_t line);

/**
 * Get the number of times memory has been allocated.
 *
 * The count tracks blocks, not calls.  A successful gcu_malloc() or
 * gcu_calloc() is counted, and so is a gcu_realloc() against a `NULL`
 * pointer, which allocates a block rather than growing one.  Growing or
 * shrinking an existing block is not counted, because no block begins or ends
 * there.
 *
 * An allocation that *fails* is still counted, which is wrong and is known to
 * be wrong.  It is left alone because neither sanitizer gate will let a test
 * provoke one: ASan makes an overflowing calloc() fatal, and valgrind rejects
 * an absurd malloc() size outright, so the fix could not be pinned by a test
 * in the suite that has to keep it.
 *
 * Subtracting gcu_get_free_count() from this therefore gives the number of
 * blocks still outstanding, which is what makes the two comparable at the end
 * of a program.
 *
 * @returns The number of times memory has been allocated.
 */
GCU_API size_t gcu_get_alloc_count(void);

/**
 * Get the number of times memory has been freed.
 *
 * A gcu_free() of a real pointer is counted.  A gcu_free() of `NULL` is not,
 * because it releases nothing; counting it would subtract from the net and so
 * hide a leak of exactly the same size.
 *
 * @returns The number of times memory has been freed.
 */
GCU_API size_t gcu_get_free_count(void);

/**
 * Reset the memory allocation and free counts to zero.
 */
GCU_API void gcu_memory_reset_counts(void);

/**
 * The number of times memory has been allocated.
 *
 * Do not access this variable directly.  Use gcu_get_alloc_count() instead.
 * It appears here simply so that gcu_malloc() and gcu_calloc() can be inlined.
 */
GCU_API_DATA extern size_t gcu_memory_alloc_count;

/**
 * The number of times memory has been freed.
 *
 * Do not access this variable directly.  Use gcu_get_free_count() instead.
 * It appears here simply so that gcu_free() can be inlined.
 */
GCU_API_DATA extern size_t gcu_memory_free_count;

#if DOXYGEN

/**
 * Cross-platform wrapper for the standard malloc() function.
 *
 * @param size The number of bytes requested.
 * @returns The beginning byte of the allocated memory.
 */
void * gcu_malloc(size_t size);

/**
 * Cross-platform wrapper for the standard calloc() function.
 *
 * @param nitems The number of items to allocate.
 * @param size The number of bytes in each item.
 * @returns The beginning byte of the allocated memory.
 */
void * gcu_calloc(size_t nitems, size_t size);

/**
 * Cross-platform wrapper for the standard realloc() function.
 *
 * Two departures from `realloc()`, both so that `NULL` means failure and
 * nothing else:
 *
 * - A `NULL` @p pointer allocates, and the allocation is counted.  This is
 *   how cutil's own containers obtain their first buffer.
 * - A zero @p size is treated as a size of one rather than releasing the
 *   block.  To release it, call gcu_free().
 *
 * @param pointer The beginning byte of the currently allocated memory, or
 *   `NULL` to allocate a new block.
 * @param size The newly requested size.
 * @returns The beginning byte of the reallocated memory, or `NULL` if the
 *   request could not be satisfied, in which case @p pointer is untouched.
 */
void * gcu_realloc(void * pointer, size_t size);

/**
 * Wrapper for the standard free() function.
 *
 * A `NULL` @p pointer is accepted and does nothing, as in `free()`.  It is
 * not counted; see gcu_get_free_count().
 *
 * @param pointer The beginning byte of the currently allocated memory, or
 *   `NULL`.
 */
void gcu_free(void * pointer);

#endif // DOXYGEN

#ifdef GHOTIIO_CUTIL_ENABLE_MEMORY_DEBUG // Log memory accesses.

/// @cond HIDDEN_SYMBOLS
#define gcu_malloc(size) gcu_malloc_debug(size, __FILE__, __LINE__)
#define gcu_calloc(nitems, size) gcu_calloc_debug(nitems, size, __FILE__, __LINE__)
#define gcu_realloc(pointer, size) gcu_realloc_debug(pointer, size, __FILE__, __LINE__)
#define gcu_free(pointer) gcu_free_debug(pointer, __FILE__, __LINE__)
/// @endcond

#else // Don't log memory access

/// @cond HIDDEN_SYMBOLS
#define gcu_malloc GHOTIIO_CUTIL(gcu_malloc)
#define gcu_calloc GHOTIIO_CUTIL(gcu_calloc)
#define gcu_realloc GHOTIIO_CUTIL(gcu_realloc)
#define gcu_free GHOTIIO_CUTIL(gcu_free)
/// @endcond

// Since we aren't doing any debugging, define the functions here in the header
// so that they can be inline-optimized.

#ifdef _WIN32 // Windows target

static inline void * gcu_malloc(size_t size) {
  ++gcu_memory_alloc_count;
  return HeapAlloc(GetProcessHeap(), 0, size);
}

static inline void * gcu_calloc(size_t nitems, size_t size) {
  ++gcu_memory_alloc_count;
  return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, nitems * size);
}

static inline void * gcu_realloc(void * pointer, size_t size) {
  // Reallocating from NULL is this block's first allocation rather than a
  // growth, so route it through gcu_malloc() and have it counted as one.
  // HeapReAlloc() separately requires a pointer it issued, so this is also
  // the only form that would work at all here.
  if (!pointer) {
    return gcu_malloc(size);
  }
  // Never shrink to nothing; see the note in the Linux version below.
  return HeapReAlloc(GetProcessHeap(), 0, pointer, size ? size : 1);
}

static inline void gcu_free(void * pointer) {
  // Freeing NULL releases nothing, so it is not counted; see the note in the
  // Linux version below.
  if (!pointer) {
    return;
  }
  ++gcu_memory_free_count;
  HeapFree(GetProcessHeap(), 0, pointer);
}

#else // Linux target

static inline void * gcu_malloc(size_t size) {
  ++gcu_memory_alloc_count;
  return malloc(size);
}

static inline void * gcu_calloc(size_t nitems, size_t size) {
  ++gcu_memory_alloc_count;
  return calloc(nitems, size);
}

static inline void * gcu_realloc(void * pointer, size_t size) {
  // Reallocating from NULL is this block's first allocation rather than a
  // growth, and the gcu_free() that eventually matches it will be counted.
  // Routing it through gcu_malloc() is what keeps the two sides paired: this
  // is how every cutil container obtains its storage, so leaving it uncounted
  // made a correct container look like a leak, and -- worse -- made a real
  // leak elsewhere cancel out against it.
  if (!pointer) {
    return gcu_malloc(size);
  }
  // Never shrink to nothing.  realloc(p, 0) releases the block and returns
  // NULL on glibc, which a caller cannot tell apart from failure and which
  // would leave that free uncounted.  This is the same call gcu_allocator_
  // default() already makes for a zero-size malloc(), for the same reason.
  //
  // Growing or shrinking an existing block is neither an allocation nor a
  // free, and a failed realloc() leaves the original alive, so nothing is
  // counted on this path either way.
  return realloc(pointer, size ? size : 1);
}

static inline void gcu_free(void * pointer) {
  // Freeing NULL releases nothing.  Counting it would subtract from the net,
  // which is worse than a false positive: it cancels a genuine leak one for
  // one, and the assertion that should have caught that leak passes.
  if (!pointer) {
    return;
  }
  ++gcu_memory_free_count;
  free(pointer);
}

#endif // _WIN32/Linux

#endif // GHOTIIO_CUTIL_ENABLE_MEMORY_DEBUG

#ifdef __cplusplus
}
#endif

#endif // GHOTI_IO_GCU_MEMORY_H

