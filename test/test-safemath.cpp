/**
 * @file
 * Tests for the overflow-checked size arithmetic.
 */

#include <gtest/gtest.h>

#include <cutil/safemath.h>

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

int main(int argc, char ** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
