/**
 * @file
 *
 * Test the behavior of Ghoti.io CUtil type library.
 */

#include <sstream>
#include <gtest/gtest.h>
#include "cutil/type.h"

using namespace std;

TEST(Type, Union) {
  int x = 0;

  ASSERT_EQ(gcu_type64_p(&x).p, &x);
  ASSERT_EQ(gcu_type64_ui64(5000000000).ui64, 5000000000);
  ASSERT_EQ(gcu_type64_ui32(3000000000).ui32, 3000000000);
  ASSERT_EQ(gcu_type64_ui16(50000).ui16, 50000);
  ASSERT_EQ(gcu_type64_ui8(200).ui8, 200);
  ASSERT_EQ(gcu_type64_i64(-2000000000).i64, -2000000000);
  ASSERT_EQ(gcu_type64_i32(-1000000000).i32, -1000000000);
  ASSERT_EQ(gcu_type64_i16(-20000).i16, -20000);
  ASSERT_EQ(gcu_type64_i8(-100).i8, -100);
  ASSERT_EQ(gcu_type64_f64(10000000.5).f64, 10000000.5);
  ASSERT_EQ(gcu_type64_f32(1000000.5).f32, 1000000.5);
  ASSERT_EQ(gcu_type64_wc('A').wc, (wchar_t)'A');
  ASSERT_EQ(gcu_type64_c('A').c, 'A');

  ASSERT_EQ(gcu_type32_ui32(3000000000).ui32, 3000000000);
  ASSERT_EQ(gcu_type32_ui16(50000).ui16, 50000);
  ASSERT_EQ(gcu_type32_ui8(200).ui8, 200);
  ASSERT_EQ(gcu_type32_i32(-1000000000).i32, -1000000000);
  ASSERT_EQ(gcu_type32_i16(-20000).i16, -20000);
  ASSERT_EQ(gcu_type32_i8(-100).i8, -100);
  ASSERT_EQ(gcu_type32_f32(1000000.5).f32, 1000000.5);
#if GCU_WCHAR_WIDTH <= 4
  ASSERT_EQ(gcu_type32_wc('A').wc, (wchar_t)'A');
#endif
  ASSERT_EQ(gcu_type32_c('A').c, 'A');

  ASSERT_EQ(gcu_type16_ui16(50000).ui16, 50000);
  ASSERT_EQ(gcu_type16_ui8(200).ui8, 200);
  ASSERT_EQ(gcu_type16_i16(-20000).i16, -20000);
  ASSERT_EQ(gcu_type16_i8(-100).i8, -100);
  ASSERT_EQ(gcu_type16_c('A').c, 'A');

  ASSERT_EQ(gcu_type8_ui8(200).ui8, 200);
  ASSERT_EQ(gcu_type8_i8(-100).i8, -100);
  ASSERT_EQ(gcu_type8_c('A').c, 'A');
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

