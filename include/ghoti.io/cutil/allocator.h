/**
 * @file
 * A pluggable allocator vtable shared across the Ghoti.io libraries.
 *
 * Several Ghoti.io libraries need to let their callers supply memory
 * management (arena allocation, instrumentation, embedded pools).  Each had
 * independently defined the same four-function vtable; this header is the one
 * definition they can all agree on.
 *
 * A `NULL` allocator pointer always means "use gcu_allocator_default()", so
 * functions taking an allocator may be called without one.
 */

#ifndef GHOTI_IO_GCU_ALLOCATOR_H
#define GHOTI_IO_GCU_ALLOCATOR_H

#include <stddef.h>
#include <ghoti.io/cutil/macros.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * A caller-supplied memory management strategy.
 *
 * All four function pointers must be provided.  Each receives the `ctx`
 * pointer from this struct as its first argument, so a single allocator
 * implementation can serve many independent pools.
 *
 * The semantics match the C standard library equivalents, with two additions:
 * `calloc_fn` must treat overflow of `nitems * size` as an allocation failure
 * and return `NULL` rather than allocating a truncated block, and a zero-size
 * request should return a usable non-`NULL` pointer rather than `NULL`, so
 * that `NULL` always means failure.  The default allocator does both; a
 * custom one is expected to as well, because callers check for `NULL` and
 * nothing else.
 */
typedef struct GCU_Allocator {
  void * ctx;                                            ///< User-defined, passed to each call.
  void * (*malloc_fn)(void * ctx, size_t size);          ///< malloc() equivalent.
  void * (*calloc_fn)(void * ctx, size_t nitems, size_t size); ///< calloc() equivalent.
  void * (*realloc_fn)(void * ctx, void * ptr, size_t size);   ///< realloc() equivalent.
  void (*free_fn)(void * ctx, void * ptr);               ///< free() equivalent.
} GCU_Allocator;

/**
 * Get the default, stdlib-backed allocator.
 *
 * The returned pointer is to a process-global constant and never needs to be
 * freed.  Its `calloc_fn` returns `NULL` on multiplication overflow, and
 * neither it nor `malloc_fn` returns `NULL` for a zero-size request.
 *
 * @return A pointer to the default allocator.
 */
GCU_API const GCU_Allocator * gcu_allocator_default(void);

/**
 * Allocate through an allocator, defaulting when none is supplied.
 *
 * @param allocator The allocator to use, or `NULL` for the default.
 * @param size The number of bytes requested.
 * @return The allocated memory, or `NULL` on failure.
 */
GCU_API void * gcu_allocator_malloc(const GCU_Allocator * allocator, size_t size);

/**
 * Allocate zeroed memory through an allocator, defaulting when none is
 * supplied.
 *
 * @param allocator The allocator to use, or `NULL` for the default.
 * @param nitems The number of items to allocate.
 * @param size The size of each item.
 * @return The allocated memory, or `NULL` on failure (including overflow).
 */
GCU_API void * gcu_allocator_calloc(
  const GCU_Allocator * allocator, size_t nitems, size_t size);

/**
 * Resize an allocation through an allocator, defaulting when none is supplied.
 *
 * @param allocator The allocator to use, or `NULL` for the default.
 * @param ptr The allocation to resize, or `NULL` to allocate afresh.
 * @param size The new size in bytes.
 * @return The resized memory, or `NULL` on failure (in which case `ptr` is
 *   still valid).
 */
GCU_API void * gcu_allocator_realloc(
  const GCU_Allocator * allocator, void * ptr, size_t size);

/**
 * Release an allocation through an allocator, defaulting when none is
 * supplied.
 *
 * @param allocator The allocator to use, or `NULL` for the default.
 * @param ptr The allocation to release.  `NULL` is ignored.
 */
GCU_API void gcu_allocator_free(const GCU_Allocator * allocator, void * ptr);

#ifdef __cplusplus
}
#endif

#endif //GHOTI_IO_GCU_ALLOCATOR_H
