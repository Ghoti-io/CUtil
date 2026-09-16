/**
 * @file
 * Tests for the overflow-checked size arithmetic.
 */

#include <gtest/gtest.h>

#include <ghoti.io/cutil/safemath.h>

using namespace std;

TEST(Add, NormalCases) {
  size_t r = 999;
  EXPECT_TRUE(gcu_safe_add_size(0, 0, &r));
  EXPECT_EQ(r, 0u);
  EXPECT_TRUE(gcu_safe_add_size(2, 3, &r));
  EXPECT_EQ(r, 5u);
  EXPECT_TRUE(gcu_safe_add_size(SIZE_MAX, 0, &r));
  EXPECT_EQ(r, SIZE_MAX);
  EXPECT_TRUE(gcu_safe_add_size(SIZE_MAX - 1, 1, &r));
  EXPECT_EQ(r, SIZE_MAX);
}

TEST(Add, OverflowLeavesResultUntouched) {
  size_t r = 12345;
  EXPECT_FALSE(gcu_safe_add_size(SIZE_MAX, 1, &r));
  EXPECT_EQ(r, 12345u);
  EXPECT_FALSE(gcu_safe_add_size(SIZE_MAX, SIZE_MAX, &r));
  EXPECT_EQ(r, 12345u);
}

TEST(Mul, NormalCases) {
  size_t r = 999;
  EXPECT_TRUE(gcu_safe_mul_size(0, SIZE_MAX, &r));
  EXPECT_EQ(r, 0u);
  EXPECT_TRUE(gcu_safe_mul_size(SIZE_MAX, 0, &r));
  EXPECT_EQ(r, 0u);
  EXPECT_TRUE(gcu_safe_mul_size(6, 7, &r));
  EXPECT_EQ(r, 42u);
  EXPECT_TRUE(gcu_safe_mul_size(SIZE_MAX, 1, &r));
  EXPECT_EQ(r, SIZE_MAX);
}

TEST(Mul, OverflowLeavesResultUntouched) {
  size_t r = 12345;
  EXPECT_FALSE(gcu_safe_mul_size(SIZE_MAX, 2, &r));
  EXPECT_EQ(r, 12345u);
  EXPECT_FALSE(gcu_safe_mul_size(SIZE_MAX / 2 + 1, 2, &r));
  EXPECT_EQ(r, 12345u);
}

TEST(Mul, BoundaryIsExact) {
  size_t r = 0;
  // The largest count of 8-byte elements that still fits.
  EXPECT_TRUE(gcu_safe_mul_size(SIZE_MAX / 8, 8, &r));
  EXPECT_EQ(r, (SIZE_MAX / 8) * 8);
  EXPECT_FALSE(gcu_safe_mul_size(SIZE_MAX / 8 + 1, 8, &r));
}

TEST(Sub, NormalCases) {
  size_t r = 999;
  EXPECT_TRUE(gcu_safe_sub_size(5, 3, &r));
  EXPECT_EQ(r, 2u);
  EXPECT_TRUE(gcu_safe_sub_size(5, 5, &r));
  EXPECT_EQ(r, 0u);
}

TEST(Sub, BorrowLeavesResultUntouched) {
  size_t r = 12345;
  EXPECT_FALSE(gcu_safe_sub_size(3, 5, &r));
  EXPECT_EQ(r, 12345u);
  EXPECT_FALSE(gcu_safe_sub_size(0, 1, &r));
  EXPECT_EQ(r, 12345u);
}

TEST(MulAdd, NormalCase) {
  size_t r = 0;
  EXPECT_TRUE(gcu_safe_mul_add_size(10, 4, 1, &r));
  EXPECT_EQ(r, 41u);
}

TEST(MulAdd, DetectsOverflowInEitherStep) {
  size_t r = 12345;
  // Overflow in the multiplication.
  EXPECT_FALSE(gcu_safe_mul_add_size(SIZE_MAX, 2, 0, &r));
  EXPECT_EQ(r, 12345u);
  // Multiplication fits exactly; the addition is what overflows.
  EXPECT_FALSE(gcu_safe_mul_add_size(SIZE_MAX, 1, 1, &r));
  EXPECT_EQ(r, 12345u);
}

TEST(Add3, NormalCase) {
  size_t r = 0;
  EXPECT_TRUE(gcu_safe_add3_size(1, 2, 3, &r));
  EXPECT_EQ(r, 6u);
}

TEST(Add3, DetectsOverflowInEitherStep) {
  size_t r = 12345;
  // The first addition overflows.
  EXPECT_FALSE(gcu_safe_add3_size(SIZE_MAX, 1, 0, &r));
  EXPECT_EQ(r, 12345u);
  // The first fits exactly; the second is what overflows.
  EXPECT_FALSE(gcu_safe_add3_size(SIZE_MAX - 1, 1, 1, &r));
  EXPECT_EQ(r, 12345u);
}

//
// Fixed-width variants. size_t is 64-bit here, so the 32-bit helpers are the
// only ones whose boundary differs from the size_t versions above; both are
// checked at their own limits rather than the platform's.
//

TEST(AddU64, NormalAndOverflow) {
  uint64_t r = 999;
  EXPECT_TRUE(gcu_safe_add_u64(2, 3, &r));
  EXPECT_EQ(r, 5u);
  EXPECT_TRUE(gcu_safe_add_u64(UINT64_MAX - 1, 1, &r));
  EXPECT_EQ(r, UINT64_MAX);

  r = 12345;
  EXPECT_FALSE(gcu_safe_add_u64(UINT64_MAX, 1, &r));
  EXPECT_EQ(r, 12345u);
}

TEST(MulU64, NormalAndOverflow) {
  uint64_t r = 999;
  EXPECT_TRUE(gcu_safe_mul_u64(0, UINT64_MAX, &r));
  EXPECT_EQ(r, 0u);
  EXPECT_TRUE(gcu_safe_mul_u64(6, 7, &r));
  EXPECT_EQ(r, 42u);
  EXPECT_TRUE(gcu_safe_mul_u64(UINT64_MAX, 1, &r));
  EXPECT_EQ(r, UINT64_MAX);

  r = 12345;
  EXPECT_FALSE(gcu_safe_mul_u64(UINT64_MAX, 2, &r));
  EXPECT_EQ(r, 12345u);
}

TEST(AddU32, OverflowsAtItsOwnWidth) {
  uint32_t r = 999;
  EXPECT_TRUE(gcu_safe_add_u32(UINT32_MAX - 1, 1, &r));
  EXPECT_EQ(r, UINT32_MAX);

  // Would fit comfortably in a size_t; must still be rejected.
  r = 12345;
  EXPECT_FALSE(gcu_safe_add_u32(UINT32_MAX, 1, &r));
  EXPECT_EQ(r, 12345u);
}

TEST(MulU32, OverflowsAtItsOwnWidth) {
  uint32_t r = 999;
  EXPECT_TRUE(gcu_safe_mul_u32(0, UINT32_MAX, &r));
  EXPECT_EQ(r, 0u);
  EXPECT_TRUE(gcu_safe_mul_u32(65535, 65535, &r));
  EXPECT_EQ(r, 4294836225u);

  r = 12345;
  EXPECT_FALSE(gcu_safe_mul_u32(65536, 65536, &r));
  EXPECT_EQ(r, 12345u);
  EXPECT_FALSE(gcu_safe_mul_u32(UINT32_MAX, 2, &r));
  EXPECT_EQ(r, 12345u);
}

//
// Differential against the compiler's own overflow builtins.
//
// Every case above is a value someone chose. This one is the whole input space
// around the boundaries, checked against the arithmetic the compiler does
// natively - which is the same arithmetic these functions compile to when
// GCU_HAS_BUILTIN_OVERFLOW is defined.
//
// That makes this test nearly free in the ordinary build, where it compares
// the builtins against themselves. It is the point of the portable build:
// test-safemath-portable compiles the same source with
// GCU_SAFEMATH_NO_BUILTINS, so these calls take the hand-written body - the
// one an MSVC build compiles, and the one no test on GCC or Clang could reach
// before - while the right-hand side of each comparison still uses the
// builtins.
//

namespace {

uint64_t xorshift(uint64_t & s) {
  s ^= s << 13;
  s ^= s >> 7;
  s ^= s << 17;
  return s;
}

// Values clustered where the answers change: zero, one, the maxima, powers of
// two, and either side of the 32-bit boundary. Uniform random values would
// almost never overflow a 64-bit multiply and would test nothing.
uint64_t interesting(uint64_t & s) {
  switch (xorshift(s) % 8u) {
  case 0:
    return 0;
  case 1:
    return 1;
  case 2:
    return UINT64_MAX;
  case 3:
    return UINT64_MAX - (xorshift(s) % 4u);
  case 4:
    return (uint64_t)1 << (xorshift(s) % 64u);
  case 5:
    return ((uint64_t)1 << 32) + (xorshift(s) % 8u) - 4u;
  case 6:
    return xorshift(s) >> (xorshift(s) % 64u);
  default:
    return xorshift(s);
  }
}

} // namespace

TEST(AgainstTheBuiltins, EveryOperationAgreesAroundTheBoundaries) {
  uint64_t s = 0xDEADBEEFCAFEF00Dull;
  const long trials = 200000;

  for (long i = 0; i < trials; i++) {
    const uint64_t a = interesting(s);
    const uint64_t b = interesting(s);

    {
      size_t got = 0, want = 0;
      const bool ok = gcu_safe_add_size((size_t)a, (size_t)b, &got);
      const bool ref = !__builtin_add_overflow((size_t)a, (size_t)b, &want);
      ASSERT_EQ(ok, ref) << "add_size(" << a << ", " << b << ")";
      if (ok) {
        ASSERT_EQ(got, want);
      }
    }
    {
      size_t got = 0, want = 0;
      const bool ok = gcu_safe_mul_size((size_t)a, (size_t)b, &got);
      const bool ref = !__builtin_mul_overflow((size_t)a, (size_t)b, &want);
      ASSERT_EQ(ok, ref) << "mul_size(" << a << ", " << b << ")";
      if (ok) {
        ASSERT_EQ(got, want);
      }
    }
    {
      uint64_t got = 0, want = 0;
      const bool ok = gcu_safe_add_u64(a, b, &got);
      const bool ref = !__builtin_add_overflow(a, b, &want);
      ASSERT_EQ(ok, ref) << "add_u64(" << a << ", " << b << ")";
      if (ok) {
        ASSERT_EQ(got, want);
      }
    }
    {
      uint64_t got = 0, want = 0;
      const bool ok = gcu_safe_mul_u64(a, b, &got);
      const bool ref = !__builtin_mul_overflow(a, b, &want);
      ASSERT_EQ(ok, ref) << "mul_u64(" << a << ", " << b << ")";
      if (ok) {
        ASSERT_EQ(got, want);
      }
    }
    {
      const uint32_t x = (uint32_t)a, y = (uint32_t)b;
      uint32_t got = 0, want = 0;
      const bool ok = gcu_safe_add_u32(x, y, &got);
      const bool ref = !__builtin_add_overflow(x, y, &want);
      ASSERT_EQ(ok, ref) << "add_u32(" << x << ", " << y << ")";
      if (ok) {
        ASSERT_EQ(got, want);
      }
    }
    {
      const uint32_t x = (uint32_t)a, y = (uint32_t)b;
      uint32_t got = 0, want = 0;
      const bool ok = gcu_safe_mul_u32(x, y, &got);
      const bool ref = !__builtin_mul_overflow(x, y, &want);
      ASSERT_EQ(ok, ref) << "mul_u32(" << x << ", " << y << ")";
      if (ok) {
        ASSERT_EQ(got, want);
      }
    }
  }
}

// Say which body was compiled, so a run of test-safemath-portable that had
// quietly taken the builtin path could not be mistaken for a run that
// exercised the portable one.
TEST(AgainstTheBuiltins, TheExpectedBodyWasCompiled) {
#ifdef GCU_HAS_BUILTIN_OVERFLOW
  const bool builtins = true;
#else
  const bool builtins = false;
#endif
#ifdef GCU_SAFEMATH_NO_BUILTINS
  EXPECT_FALSE(builtins)
      << "GCU_SAFEMATH_NO_BUILTINS was defined but the builtin body was still "
         "selected; this binary is not testing the portable path";
#else
  EXPECT_TRUE(builtins)
      << "this compiler has no overflow builtins, so the ordinary build is "
         "already taking the portable path";
#endif
}

int main(int argc, char ** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
