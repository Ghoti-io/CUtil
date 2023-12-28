/**
 * @file
 * Header file for debugging-related functions.
 */

#ifndef GHOTIIO_CUTIL_DEBUG_H
#define GHOTIIO_CUTIL_DEBUG_H

#include <stdbool.h>
#include <stdlib.h>

#include "cutil/libver.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @cond HIDDEN_SYMBOLS
#define gcu_malloc GHOTIIO_CUTIL(gcu_malloc)
#define gcu_calloc GHOTIIO_CUTIL(gcu_calloc)
#define gcu_realloc GHOTIIO_CUTIL(gcu_realloc)
#define gcu_free GHOTIIO_CUTIL(gcu_free)
#define gcu_mem_start GHOTIIO_CUTIL(gcu_mem_start)
#define gcu_mem_stop GHOTIIO_CUTIL(gcu_mem_stop)
/// @endcond

/**
 * Wrapper for the standard malloc() function.
 * 
 * @param size The number of bytes requested.
 * @param file The name of the file from which the function was called.
 * @param line The line number on which the function was called.
 * @returns The beginning byte of the allocated memory.
 */
void * gcu_malloc(size_t size, const char * file, size_t line);

/**
 * Wrapper for the standard calloc() function.
 * 
 * @param nitems The number of items to allocate.
 * @param size The number of bytes in each item.
 * @param file The name of the file from which the function was called.
 * @param line The line number on which the function was called.
 * @returns The beginning byte of the allocated memory.
 */
void * gcu_calloc(size_t nitems, size_t size, const char * file, size_t line);

/**
 * Wrapper for the standard realloc() function.
 *
 * @param pointer The beginning byte of the currently allocated memory.
 * @param size The newly requested size.
 * @param file The name of the file from which the function was called.
 * @param line The line number on which the function was called.
 * @returns The beginning byte of the reallocated memory.
 */
void * gcu_realloc(void * pointer, size_t size, const char * file, size_t line);

/**
 * Wrapper for the standard free() function.
 * @param pointer The beginning byte of the currently allocated memory.
 * @param file The name of the file from which the function was called.
 * @param line The line number on which the function was called.
 */
void gcu_free(void * pointer, const char * file, size_t line);

/**
 * Signal that intercepted memory management calls should be logged to stderr.
 */
void gcu_mem_start(void);

/**
 * Signal that intercepted memory management calls should no longer be logged
 * to stderr.
 */
void gcu_mem_stop(void);

#ifndef GHOTIIO_CUTIL_DEBUG_DO_NOT_REDECLARE_MEMORY_FUNCTIONS

/*
 * By defining `GHOTIIO_CUTIL_DEBUG_DO_NOT_REDECLARE_MEMORY_FUNCTIONS`,
 * The function names will be properly defined (above), but the standard
 * `malloc()`, `realloc()`, and `free()` will *not* be redefined.  Proper
 * compilation of `debug.c` depends on this behavior.
 */
#define malloc(size) gcu_malloc(size, __FILE__, __LINE__)
#define calloc(nitems, size) gcu_calloc(nitems, size, __FILE__, __LINE__)
#define realloc(pointer, size) gcu_realloc(pointer, size, __FILE__, __LINE__)
#define free(pointer) gcu_free(pointer, __FILE__, __LINE__)

#endif // GHOTIIO_CUTIL_DEBUG_DO_NOT_REDECLARE_MEMORY_FUNCTIONS

#ifdef __cplusplus
}
#endif

#endif // GHOTIIO_CUTIL_DEBUG_H

