/**
 * @file
 * The default, stdlib-backed allocator and the NULL-tolerant dispatch
 * helpers declared in cutil/allocator.h.
 */

#include <stdlib.h>
#include <cutil/allocator.h>
#include <cutil/safemath.h>

static void * default_malloc(void * ctx, size_t size) {
  (void)ctx;
  return malloc(size);
}

static void * default_calloc(void * ctx, size_t nitems, size_t size) {
  (void)ctx;
  // calloc() is permitted to detect this itself, but is not required to on
  // every platform.  Check first so the contract holds everywhere.
  size_t total;
  if (!gcu_safe_mul_size(nitems, size, &total)) {
    return NULL;
  }
  return calloc(nitems, size);
}

static void * default_realloc(void * ctx, void * ptr, size_t size) {
  (void)ctx;
  return realloc(ptr, size);
}

static void default_free(void * ctx, void * ptr) {
  (void)ctx;
  free(ptr);
}

static const GCU_Allocator default_allocator = {
  .ctx = NULL,
  .malloc_fn = default_malloc,
  .calloc_fn = default_calloc,
  .realloc_fn = default_realloc,
  .free_fn = default_free,
};

const GCU_Allocator * gcu_allocator_default(void) {
  return &default_allocator;
}

void * gcu_allocator_malloc(const GCU_Allocator * allocator, size_t size) {
  if (!allocator) {
    allocator = &default_allocator;
  }
  return allocator->malloc_fn(allocator->ctx, size);
}

void * gcu_allocator_calloc(
  const GCU_Allocator * allocator, size_t nitems, size_t size) {
  if (!allocator) {
    allocator = &default_allocator;
  }
  return allocator->calloc_fn(allocator->ctx, nitems, size);
}

void * gcu_allocator_realloc(
  const GCU_Allocator * allocator, void * ptr, size_t size) {
  if (!allocator) {
    allocator = &default_allocator;
  }
  return allocator->realloc_fn(allocator->ctx, ptr, size);
}

void gcu_allocator_free(const GCU_Allocator * allocator, void * ptr) {
  if (!ptr) {
    return;
  }
  if (!allocator) {
    allocator = &default_allocator;
  }
  allocator->free_fn(allocator->ctx, ptr);
}
