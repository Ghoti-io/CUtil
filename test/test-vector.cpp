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

// Helper function for next test.
static void addOne64(GCU_Vector64 * vector) {
  for (size_t i = 0; i < gcu_vector64_count(vector); ++i) {
    ++*(size_t *)(vector->supplementary_data);
  }
}

TEST(Vector64, Cleanup) {
  auto v = gcu_vector64_create(3);
  size_t count = 0;
  v->supplementary_data = (void *)&count;
  v->cleanup = addOne64;
  ASSERT_TRUE(gcu_vector64_append(v, gcu_type64_b(true)));
  ASSERT_TRUE(gcu_vector64_append(v, gcu_type64_b(true)));
  ASSERT_TRUE(gcu_vector64_append(v, gcu_type64_b(true)));
  gcu_vector64_destroy(v);
  ASSERT_EQ(count, 3);
}

TEST(Vector64, InPlace64) {
  GCU_Vector64 v;
  ASSERT_TRUE(gcu_vector64_create_in_place(&v, 0));
  ASSERT_EQ(gcu_vector64_count(&v), 0);
  ASSERT_EQ(v.capacity, 0);

  // Verify insert causes a resize.
  ASSERT_TRUE(gcu_vector64_append(&v, gcu_type64_ui64(42)));
  ASSERT_EQ(gcu_vector64_count(&v), 1);
  ASSERT_EQ(v.capacity, 32);

  gcu_vector64_destroy_in_place(&v);
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

// Helper function for next test.
static void addOne32(GCU_Vector32 * vector) {
  for (size_t i = 0; i < gcu_vector32_count(vector); ++i) {
    ++*(size_t *)(vector->supplementary_data);
  }
}

TEST(Vector32, Cleanup) {
  auto v = gcu_vector32_create(3);
  size_t count = 0;
  v->supplementary_data = (void *)&count;
  v->cleanup = addOne32;
  ASSERT_TRUE(gcu_vector32_append(v, gcu_type32_b(true)));
  ASSERT_TRUE(gcu_vector32_append(v, gcu_type32_b(true)));
  ASSERT_TRUE(gcu_vector32_append(v, gcu_type32_b(true)));
  gcu_vector32_destroy(v);
  ASSERT_EQ(count, 3);
}

TEST(Vector32, InPlace32) {
  GCU_Vector32 v;
  ASSERT_TRUE(gcu_vector32_create_in_place(&v, 0));
  ASSERT_EQ(gcu_vector32_count(&v), 0);
  ASSERT_EQ(v.capacity, 0);

  // Verify insert causes a resize.
  ASSERT_TRUE(gcu_vector32_append(&v, gcu_type32_ui32(42)));
  ASSERT_EQ(gcu_vector32_count(&v), 1);
  ASSERT_EQ(v.capacity, 32);

  gcu_vector32_destroy_in_place(&v);
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

// Helper function for next test.
static void addOne16(GCU_Vector16 * vector) {
  for (size_t i = 0; i < gcu_vector16_count(vector); ++i) {
    ++*(size_t *)(vector->supplementary_data);
  }
}

TEST(Vector16, Cleanup) {
  auto v = gcu_vector16_create(3);
  size_t count = 0;
  v->supplementary_data = (void *)&count;
  v->cleanup = addOne16;
  ASSERT_TRUE(gcu_vector16_append(v, gcu_type16_b(true)));
  ASSERT_TRUE(gcu_vector16_append(v, gcu_type16_b(true)));
  ASSERT_TRUE(gcu_vector16_append(v, gcu_type16_b(true)));
  gcu_vector16_destroy(v);
  ASSERT_EQ(count, 3);
}

TEST(Vector16, InPlace16) {
  GCU_Vector16 v;
  ASSERT_TRUE(gcu_vector16_create_in_place(&v, 0));
  ASSERT_EQ(gcu_vector16_count(&v), 0);
  ASSERT_EQ(v.capacity, 0);

  // Verify insert causes a resize.
  ASSERT_TRUE(gcu_vector16_append(&v, gcu_type16_ui16(42)));
  ASSERT_EQ(gcu_vector16_count(&v), 1);
  ASSERT_EQ(v.capacity, 32);

  gcu_vector16_destroy_in_place(&v);
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

// Helper function for next test.
static void addOne8(GCU_Vector8 * vector) {
  for (size_t i = 0; i < gcu_vector8_count(vector); ++i) {
    ++*(size_t *)(vector->supplementary_data);
  }
}

TEST(Vector8, Cleanup) {
  auto v = gcu_vector8_create(3);
  size_t count = 0;
  v->supplementary_data = (void *)&count;
  v->cleanup = addOne8;
  ASSERT_TRUE(gcu_vector8_append(v, gcu_type8_b(true)));
  ASSERT_TRUE(gcu_vector8_append(v, gcu_type8_b(true)));
  ASSERT_TRUE(gcu_vector8_append(v, gcu_type8_b(true)));
  gcu_vector8_destroy(v);
  ASSERT_EQ(count, 3);
}

TEST(Vector8, InPlace8) {
  GCU_Vector8 v;
  ASSERT_TRUE(gcu_vector8_create_in_place(&v, 0));
  ASSERT_EQ(gcu_vector8_count(&v), 0);
  ASSERT_EQ(v.capacity, 0);

  // Verify insert causes a resize.
  ASSERT_TRUE(gcu_vector8_append(&v, gcu_type8_ui8(42)));
  ASSERT_EQ(gcu_vector8_count(&v), 1);
  ASSERT_EQ(v.capacity, 32);

  gcu_vector8_destroy_in_place(&v);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

