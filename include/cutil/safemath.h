/**
 * @file
 * Overflow-checked arithmetic on `size_t`.
 *
 * Every one of these returns `false` and leaves `*result` untouched when the
 * operation would overflow, so a size computation can be validated before it
 * reaches an allocator.  Where the compiler provides `__builtin_*_overflow`
 * these compile to a single instruction plus a branch.
 */

#ifndef GHOTIIO_CUTIL_SAFEMATH_H
#define GHOTIIO_CUTIL_SAFEMATH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
#define GCU_HAS_BUILTIN_OVERFLOW 1
#endif

/**
 * Add two sizes, detecting overflow.
 *
 * @param a The first addend.
 * @param b The second addend.
 * @param result Receives `a + b` on success; untouched on overflow.
 * @return `true` when the sum is representable, `false` otherwise.
 */
static inline bool gcu_safe_add_size(size_t a, size_t b, size_t * result) {
#ifdef GCU_HAS_BUILTIN_OVERFLOW
  size_t tmp;
  if (__builtin_add_overflow(a, b, &tmp)) {
    return false;
  }
  *result = tmp;
  return true;
#else
  if (a > SIZE_MAX - b) {
    return false;
  }
  *result = a + b;
  return true;
#endif
}

/**
 * Multiply two sizes, detecting overflow.
 *
 * @param a The first factor.
 * @param b The second factor.
 * @param result Receives `a * b` on success; untouched on overflow.
 * @return `true` when the product is representable, `false` otherwise.
 */
static inline bool gcu_safe_mul_size(size_t a, size_t b, size_t * result) {
#ifdef GCU_HAS_BUILTIN_OVERFLOW
  size_t tmp;
  if (__builtin_mul_overflow(a, b, &tmp)) {
    return false;
  }
  *result = tmp;
  return true;
#else
  if (a != 0 && b > SIZE_MAX / a) {
    return false;
  }
  *result = a * b;
  return true;
#endif
}

/**
 * Subtract two sizes, detecting borrow.
 *
 * @param a The minuend.
 * @param b The subtrahend.
 * @param result Receives `a - b` on success; untouched when `b > a`.
 * @return `true` when `b <= a`, `false` otherwise.
 */
static inline bool gcu_safe_sub_size(size_t a, size_t b, size_t * result) {
  if (b > a) {
    return false;
  }
  *result = a - b;
  return true;
}

/**
 * Add three sizes, detecting overflow at either step.
 *
 * @param a The first addend.
 * @param b The second addend.
 * @param c The third addend.
 * @param result Receives the total on success; untouched on overflow.
 * @return `true` when the total is representable, `false` otherwise.
 */
static inline bool gcu_safe_add3_size(
  size_t a, size_t b, size_t c, size_t * result) {
  size_t partial;
  if (!gcu_safe_add_size(a, b, &partial)) {
    return false;
  }
  return gcu_safe_add_size(partial, c, result);
}

/**
 * Compute `(a * b) + c`, detecting overflow at either step.
 *
 * This is the shape of nearly every buffer sizing calculation: a count times
 * an element size, plus a header or terminator.
 *
 * @param a The first factor.
 * @param b The second factor.
 * @param c The addend.
 * @param result Receives the total on success; untouched on overflow.
 * @return `true` when the total is representable, `false` otherwise.
 */
static inline bool gcu_safe_mul_add_size(
  size_t a, size_t b, size_t c, size_t * result) {
  size_t product;
  if (!gcu_safe_mul_size(a, b, &product)) {
    return false;
  }
  return gcu_safe_add_size(product, c, result);
}

/**
 * Add two 64-bit values, detecting overflow.
 *
 * `size_t` is not 64 bits everywhere, so a calculation on a file format's
 * own 64-bit fields needs its own width rather than the platform's.
 *
 * @param a The first addend.
 * @param b The second addend.
 * @param result Receives `a + b` on success; untouched on overflow.
 * @return `true` when the sum is representable, `false` otherwise.
 */
static inline bool gcu_safe_add_u64(uint64_t a, uint64_t b, uint64_t * result) {
#ifdef GCU_HAS_BUILTIN_OVERFLOW
  uint64_t tmp;
  if (__builtin_add_overflow(a, b, &tmp)) {
    return false;
  }
  *result = tmp;
  return true;
#else
  if (a > UINT64_MAX - b) {
    return false;
  }
  *result = a + b;
  return true;
#endif
}

/**
 * Multiply two 64-bit values, detecting overflow.
 *
 * @param a The first factor.
 * @param b The second factor.
 * @param result Receives `a * b` on success; untouched on overflow.
 * @return `true` when the product is representable, `false` otherwise.
 */
static inline bool gcu_safe_mul_u64(uint64_t a, uint64_t b, uint64_t * result) {
#ifdef GCU_HAS_BUILTIN_OVERFLOW
  uint64_t tmp;
  if (__builtin_mul_overflow(a, b, &tmp)) {
    return false;
  }
  *result = tmp;
  return true;
#else
  if (a != 0 && b > UINT64_MAX / a) {
    return false;
  }
  *result = a * b;
  return true;
#endif
}

/**
 * Add two 32-bit values, detecting overflow.
 *
 * @param a The first addend.
 * @param b The second addend.
 * @param result Receives `a + b` on success; untouched on overflow.
 * @return `true` when the sum is representable, `false` otherwise.
 */
static inline bool gcu_safe_add_u32(uint32_t a, uint32_t b, uint32_t * result) {
#ifdef GCU_HAS_BUILTIN_OVERFLOW
  uint32_t tmp;
  if (__builtin_add_overflow(a, b, &tmp)) {
    return false;
  }
  *result = tmp;
  return true;
#else
  if (a > UINT32_MAX - b) {
    return false;
  }
  *result = a + b;
  return true;
#endif
}

/**
 * Multiply two 32-bit values, detecting overflow.
 *
 * @param a The first factor.
 * @param b The second factor.
 * @param result Receives `a * b` on success; untouched on overflow.
 * @return `true` when the product is representable, `false` otherwise.
 */
static inline bool gcu_safe_mul_u32(uint32_t a, uint32_t b, uint32_t * result) {
#ifdef GCU_HAS_BUILTIN_OVERFLOW
  uint32_t tmp;
  if (__builtin_mul_overflow(a, b, &tmp)) {
    return false;
  }
  *result = tmp;
  return true;
#else
  if (a != 0 && b > UINT32_MAX / a) {
    return false;
  }
  *result = a * b;
  return true;
#endif
}

#ifdef __cplusplus
}
#endif

#endif //GHOTIIO_CUTIL_SAFEMATH_H
