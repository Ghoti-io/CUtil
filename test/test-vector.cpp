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

TEST(Vector64, CreateEmpty) {
  auto v = gcu_vector64_create(0);
  ASSERT_EQ(gcu_vector64_count(v), 0);
  ASSERT_EQ(v->capacity, 0);

  // Verify insert causes a resize.
  ASSERT_TRUE(gcu_vector64_append(v, gcu_type64_ui32(42)));
  ASSERT_EQ(gcu_vector64_count(v), 1);
  ASSERT_EQ(v->capacity, 32);

  gcu_vector64_destroy(v);
}

TEST(Vector64, NonEmpty) {
  auto v = gcu_vector64_create(3);
  ASSERT_EQ(gcu_vector64_count(v), 0);
  ASSERT_EQ(v->capacity, 3);

  // Verify that inserts
  for (size_t i = 0; i < 10000; ++i) {
    ASSERT_TRUE(gcu_vector64_append(v, gcu_type64_ui32(i)));
  }
  ASSERT_EQ(gcu_vector64_count(v), 10000);
  ASSERT_EQ(v->capacity, 10847);

  // Verify that all the values are here, despite the reallocations.
  for (size_t i = 0; i < 10000; ++i) {
    ASSERT_EQ(v->data[i].ui32, i);
  }
  gcu_vector64_destroy(v);
}

TEST(Vector32, CreateEmpty) {
  auto v = gcu_vector32_create(0);
  ASSERT_EQ(gcu_vector32_count(v), 0);
  ASSERT_EQ(v->capacity, 0);

  // Verify insert causes a resize.
  ASSERT_TRUE(gcu_vector32_append(v, gcu_type32_ui32(42)));
  ASSERT_EQ(gcu_vector32_count(v), 1);
  ASSERT_EQ(v->capacity, 32);

  gcu_vector32_destroy(v);
}

TEST(Vector32, NonEmpty) {
  auto v = gcu_vector32_create(3);
  ASSERT_EQ(gcu_vector32_count(v), 0);
  ASSERT_EQ(v->capacity, 3);

  // Verify that inserts
  for (size_t i = 0; i < 10000; ++i) {
    ASSERT_TRUE(gcu_vector32_append(v, gcu_type32_ui32(i)));
  }
  ASSERT_EQ(gcu_vector32_count(v), 10000);
  ASSERT_EQ(v->capacity, 10847);

  // Verify that all the values are here, despite the reallocations.
  for (size_t i = 0; i < 10000; ++i) {
    ASSERT_EQ(v->data[i].ui32, i);
  }
  gcu_vector32_destroy(v);
}

TEST(Vector16, CreateEmpty) {
  auto v = gcu_vector16_create(0);
  ASSERT_EQ(gcu_vector16_count(v), 0);
  ASSERT_EQ(v->capacity, 0);

  // Verify insert causes a resize.
  ASSERT_TRUE(gcu_vector16_append(v, gcu_type16_ui16(42)));
  ASSERT_EQ(gcu_vector16_count(v), 1);
  ASSERT_EQ(v->capacity, 32);

  gcu_vector16_destroy(v);
}

TEST(Vector16, NonEmpty) {
  auto v = gcu_vector16_create(3);
  ASSERT_EQ(gcu_vector16_count(v), 0);
  ASSERT_EQ(v->capacity, 3);

  // Verify that inserts
  for (size_t i = 0; i < 10000; ++i) {
    ASSERT_TRUE(gcu_vector16_append(v, gcu_type16_ui16(i)));
  }
  ASSERT_EQ(gcu_vector16_count(v), 10000);
  ASSERT_EQ(v->capacity, 10847);

  // Verify that all the values are here, despite the reallocations.
  for (size_t i = 0; i < 10000; ++i) {
    ASSERT_EQ(v->data[i].ui16, i);
  }
  gcu_vector16_destroy(v);
}

TEST(Vector8, CreateEmpty) {
  auto v = gcu_vector8_create(0);
  ASSERT_EQ(gcu_vector8_count(v), 0);
  ASSERT_EQ(v->capacity, 0);

  // Verify insert causes a resize.
  ASSERT_TRUE(gcu_vector8_append(v, gcu_type8_ui8(42)));
  ASSERT_EQ(gcu_vector8_count(v), 1);
  ASSERT_EQ(v->capacity, 32);

  gcu_vector8_destroy(v);
}

TEST(Vector8, NonEmpty) {
  auto v = gcu_vector8_create(3);
  ASSERT_EQ(gcu_vector8_count(v), 0);
  ASSERT_EQ(v->capacity, 3);

  // Verify that inserts
  for (size_t i = 0; i < 10000; ++i) {
    ASSERT_TRUE(gcu_vector8_append(v, gcu_type8_ui8(i % 256)));
  }
  ASSERT_EQ(gcu_vector8_count(v), 10000);
  ASSERT_EQ(v->capacity, 10847);

  // Verify that all the values are here, despite the reallocations.
  for (size_t i = 0; i < 10000; ++i) {
    ASSERT_EQ(v->data[i].ui8, i % 256);
  }
  gcu_vector8_destroy(v);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

