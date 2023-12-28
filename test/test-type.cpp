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

  ASSERT_EQ(gcu_type_p(&x).p, &x);
  ASSERT_EQ(gcu_type_ui64(5000000000).ui64, 5000000000);
  ASSERT_EQ(gcu_type_ui32(3000000000).ui32, 3000000000);
  ASSERT_EQ(gcu_type_ui16(50000).ui16, 50000);
  ASSERT_EQ(gcu_type_ui8(200).ui8, 200);
  ASSERT_EQ(gcu_type_i64(-2000000000).i64, -2000000000);
  ASSERT_EQ(gcu_type_i32(-1000000000).i32, -1000000000);
  ASSERT_EQ(gcu_type_i16(-20000).i16, -20000);
  ASSERT_EQ(gcu_type_i8(-100).i8, -100);
  ASSERT_EQ(gcu_type_c('A').c, 'A');
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

