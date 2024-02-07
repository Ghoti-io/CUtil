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

#ifndef GHOTIIO_CUTIL_DEBUG_H
#define GHOTIIO_CUTIL_DEBUG_H

#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <stdlib.h>
#endif

#include "cutil/libver.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @cond HIDDEN_SYMBOLS
#define gcu_mem_start GHOTIIO_CUTIL(gcu_mem_start)
#define gcu_mem_stop GHOTIIO_CUTIL(gcu_mem_stop)
#define gcu_malloc_debug GHOTIIO_CUTIL(gcu_malloc_debug)
#define gcu_calloc_debug GHOTIIO_CUTIL(gcu_calloc_debug)
#define gcu_realloc_debug GHOTIIO_CUTIL(gcu_realloc_debug)
#define gcu_free_debug GHOTIIO_CUTIL(gcu_free_debug)
#define gcu_get_alloc_count GHOTIIO_CUTIL(gcu_get_alloc_count)
#define gcu_get_free_count GHOTIIO_CUTIL(gcu_get_free_count)
/// @endcond

/**
 * Instruct Ghoti.io CUtils library that intercepted memory management calls
 * should be logged to stderr.
 */
void gcu_mem_start(void);

/**
 * Instruct Ghoti.io CUtils library that intercepted memory management calls
 * should no longer be logged to stderr.
 */
void gcu_mem_stop(void);

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
void * gcu_malloc_debug(size_t size, const char * file, size_t line);

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
void * gcu_calloc_debug(size_t nitems, size_t size, const char * file, size_t line);

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
void * gcu_realloc_debug(void * pointer, size_t size, const char * file, size_t line);

/**
 * Wrapper for the standard free() function.
 *
 * This function should not be called directly. Call gcu_free() instead.
 *
 * @param pointer The beginning byte of the currently allocated memory.
 * @param file The name of the file from which the function was called.
 * @param line The line number on which the function was called.
 */
void gcu_free_debug(void * pointer, const char * file, size_t line);

/**
 * Get the number of times memory has been allocated.
 *
 * Calls to gcu_malloc() and gcu_calloc() are counted.  Calls to gcu_realloc()
 * are not counted.
 *
 * @returns The number of times memory has been allocated.
 */
size_t gcu_get_alloc_count(void);

/**
 * Get the number of times memory has been freed.
 *
 * Calls to gcu_free() are counted.
 *
 * @returns The number of times memory has been freed.
 */
size_t gcu_get_free_count(void);

/**
 * The number of times memory has been allocated.
 *
 * Do not access this variable directly.  Use gcu_get_alloc_count() instead.
 * It appears here simply so that gcu_malloc() and gcu_calloc() can be inlined.
 */
extern size_t gcu_memory_alloc_count;

/**
 * The number of times memory has been freed.
 *
 * Do not access this variable directly.  Use gcu_get_free_count() instead.
 * It appears here simply so that gcu_free() can be inlined.
 */
extern size_t gcu_memory_free_count;

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
 * @param pointer The beginning byte of the currently allocated memory.
 * @param size The newly requested size.
 * @returns The beginning byte of the reallocated memory.
 */
void * gcu_realloc(void * pointer, size_t size);

/**
 * Wrapper for the standard free() function.
 *
 * @param pointer The beginning byte of the currently allocated memory.
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
  return HeapReAlloc(GetProcessHeap(), 0, pointer, size);
}

static inline void gcu_free(void * pointer) {
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
  return realloc(pointer, size);
}

static inline void gcu_free(void * pointer) {
  ++gcu_memory_free_count;
  free(pointer);
}

#endif // _WIN32/Linux

#endif // GHOTIIO_CUTIL_ENABLE_MEMORY_DEBUG

#ifdef __cplusplus
}
#endif

#endif // GHOTIIO_CUTIL_DEBUG_H

