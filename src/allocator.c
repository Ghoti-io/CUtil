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
 * The default, stdlib-backed allocator and the NULL-tolerant dispatch
 * helpers declared in cutil/allocator.h.
 */

#include <stdlib.h>
#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/allocator.h>
#include <ghoti.io/cutil/safemath.h>

static void * default_malloc(void * ctx, size_t size) {
  (void)ctx;
  // A zero-size request is allowed to return NULL, which a caller cannot tell
  // apart from running out of memory.  That ambiguity has already produced a
  // real bug in this suite, so hand back one byte instead.
  return malloc(size ? size : 1);
}

static void * default_calloc(void * ctx, size_t nitems, size_t size) {
  (void)ctx;
  // calloc() is permitted to detect this itself, but is not required to on
  // every platform.  Check first so the contract holds everywhere.
  size_t total;
  if (!gcu_safe_mul_size(nitems, size, &total)) {
    return NULL;
  }
  // As above, and zeroed rather than merely allocated: a caller asking for
  // zero zeroed items should not receive uninitialized memory.
  return calloc(1, total ? total : 1);
}

static void * default_realloc(void * ctx, void * ptr, size_t size) {
  (void)ctx;
  // The same rule the two above keep, for the same reason: realloc(p, 0)
  // releases the block and hands back NULL on glibc, and a caller cannot tell
  // that apart from a failure that left the block alive.  Shrink to one byte
  // and let gcu_free() be the only way to release anything.
  return realloc(ptr, size ? size : 1);
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
