/**
 * @file
 *
 * Test the behavior of Ghoti.io CUtil hash table library.
 */

#include <sstream>
#include <gtest/gtest.h>
#include "cutil/vector.h"
#include <stdio.h>

using namespace std;

TEST(Hash, CreateEmpty) {
  auto v = gcu_vector_create(0);
  ASSERT_EQ(gcu_vector_count(v), 0);
  ASSERT_EQ(v->capacity, 0);

  // Verify insert causes a resize.
  ASSERT_TRUE(gcu_vector_append(v, gcu_type_ui32(42)));
  ASSERT_EQ(gcu_vector_count(v), 1);
  ASSERT_EQ(v->capacity, 32);

  gcu_vector_destroy(v);
}

TEST(Hash, NonEmpty) {
  auto v = gcu_vector_create(3);
  ASSERT_EQ(gcu_vector_count(v), 0);
  ASSERT_EQ(v->capacity, 3);

  // Verify that inserts
  for (size_t i = 0; i < 10000; ++i) {
    ASSERT_TRUE(gcu_vector_append(v, gcu_type_ui32(i)));
  }
  ASSERT_EQ(gcu_vector_count(v), 10000);
  ASSERT_EQ(v->capacity, 10847);

  // Verify that all the values are here, despite the reallocations.
  for (size_t i = 0; i < 10000; ++i) {
    ASSERT_EQ(v->data[i].ui32, i);
  }
  gcu_vector_destroy(v);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

