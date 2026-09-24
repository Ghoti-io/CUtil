#include <atomic>
#include <chrono>
#include <sstream>
#include <thread>
#include <gtest/gtest.h>
#include <ghoti.io/cutil/hash.h>

using namespace std;

// A note on what these tests can and cannot be credited with.
//
// Two undefined-behaviour defects were found in the hash template from
// outside this repository, by a consumer that had built cutil with clang.
// They look alike -- both are a table with no cell array being handled as
// though it had one -- but they went unseen for opposite reasons, and only
// one of them is a story about missing tests.
//
// Cloning an empty table was never done by any test here. That is a coverage
// gap and CloneOfATableWithNoCells closes it.
//
// Growing from an empty table, on the other hand, is what Hash64.Set has done
// on every run since it was written: create(0) then set(). The line was
// executed thousands of times under ASan+UBSan and reported clean, because
// forming `&data[0]` on a null pointer is undefined and *gcc's UBSan does not
// diagnose it* -- neither -fsanitize=undefined nor -fsanitize=pointer-overflow,
// both measured. clang's does. No test could have closed that one; only the
// second compiler could, which is why `check-clang` is now a test gate.

TEST(Hash64, CreateEmpty) {
  auto t = gcu_hash64_create(0);
  ASSERT_NE(t, nullptr);
  ASSERT_EQ(gcu_hash64_count(t), 0);
  ASSERT_EQ(t->capacity, 0);
  gcu_hash64_destroy(t);
}

TEST(Hash64, Create) {
  auto t = gcu_hash64_create(3);
  ASSERT_EQ(gcu_hash64_count(t), 0);
  ASSERT_EQ(t->capacity, 7);
  gcu_hash64_destroy(t);
}

TEST(Hash64, Set) {
  auto t = gcu_hash64_create(0);
  size_t hash = 1001;
  // Verify the list is empty.
  ASSERT_FALSE(gcu_hash64_contains(t, hash));
  ASSERT_EQ(gcu_hash64_count(t), 0);

  // Add one item to the list.
  ASSERT_TRUE(gcu_hash64_set(t, hash, gcu_type64_ui32(42)));
  ASSERT_TRUE(gcu_hash64_contains(t, hash));
  ASSERT_FALSE(gcu_hash64_contains(t, hash + 1));
  ASSERT_EQ(gcu_hash64_get(t, hash).value.ui32, 42);
  ASSERT_EQ(gcu_hash64_count(t), 1);

  // Add a second item to the list
  ASSERT_TRUE(gcu_hash64_set(t, hash + 1, gcu_type64_ui32(43)));
  ASSERT_TRUE(gcu_hash64_contains(t, hash));
  ASSERT_TRUE(gcu_hash64_contains(t, hash + 1));
  ASSERT_FALSE(gcu_hash64_contains(t, hash + 2));
  ASSERT_EQ(gcu_hash64_get(t, hash).value.ui32, 42);
  ASSERT_EQ(gcu_hash64_get(t, hash + 1).value.ui32, 43);
  ASSERT_EQ(gcu_hash64_count(t), 2);

  // Overwrite an item.
  ASSERT_TRUE(gcu_hash64_set(t, hash, gcu_type64_ui32(7)));
  ASSERT_TRUE(gcu_hash64_contains(t, hash));
  ASSERT_TRUE(gcu_hash64_contains(t, hash + 1));
  ASSERT_FALSE(gcu_hash64_contains(t, hash + 2));
  ASSERT_EQ(gcu_hash64_get(t, hash).value.ui32, 7);
  ASSERT_EQ(gcu_hash64_get(t, hash + 1).value.ui32, 43);
  ASSERT_EQ(gcu_hash64_count(t), 2);

  // Verify internal numbers.
  ASSERT_EQ(t->capacity, 65);
  ASSERT_EQ(t->entries, 2);
  ASSERT_EQ(t->removed, 0);

  // Cleanup.
  gcu_hash64_destroy(t);
}

TEST(Hash64, Remove) {
  auto t = gcu_hash64_create(6);

  // Automatically choose collisions.
  size_t capacity = t->capacity;
  size_t hash1 = capacity / 2;
  size_t hash2 = hash1 + capacity;
  size_t hash3 = hash2 + capacity;
  size_t hash4 = hash3 + capacity;

  // Add three items that have a hash collision.
  ASSERT_TRUE(gcu_hash64_set(t, hash1, gcu_type64_ui32(hash1)));
  ASSERT_TRUE(gcu_hash64_set(t, hash2, gcu_type64_ui32(hash2)));
  ASSERT_TRUE(gcu_hash64_set(t, hash3, gcu_type64_ui32(hash3)));
  ASSERT_EQ(gcu_hash64_get(t, hash1).value.ui32, hash1);
  ASSERT_EQ(gcu_hash64_get(t, hash2).value.ui32, hash2);
  ASSERT_EQ(gcu_hash64_get(t, hash3).value.ui32, hash3);
  ASSERT_EQ(t->capacity, capacity);
  ASSERT_EQ(t->entries, 3);
  ASSERT_EQ(t->removed, 0);
  ASSERT_EQ(gcu_hash64_count(t), 3);

  // Collision order is 1 2 3.
  // Remove all 3.
  ASSERT_TRUE(gcu_hash64_remove(t, hash1));
  ASSERT_TRUE(gcu_hash64_remove(t, hash2));
  ASSERT_TRUE(gcu_hash64_remove(t, hash3));
  ASSERT_EQ(t->capacity, capacity);
  ASSERT_EQ(t->entries, 3);
  ASSERT_EQ(t->removed, 3);
  ASSERT_EQ(gcu_hash64_count(t), 0);

  // Add a new entry at that collision.
  ASSERT_TRUE(gcu_hash64_set(t, hash4, gcu_type64_ui32(hash4)));
  ASSERT_EQ(gcu_hash64_get(t, hash4).value.ui32, hash4);
  ASSERT_EQ(t->capacity, capacity);
  ASSERT_EQ(t->entries, 3);
  ASSERT_EQ(t->removed, 2);
  ASSERT_EQ(gcu_hash64_count(t), 1);

  // Cleanup.
  gcu_hash64_destroy(t);
}

TEST(Hash64, Clone) {
  auto t = gcu_hash64_create(6);
  size_t hash = 1001;
  gcu_hash64_set(t, hash, gcu_type64_ui32(42));
  auto t2 = gcu_hash64_clone(t);
  ASSERT_NE(t2, nullptr);
  ASSERT_NE(t, t2);
  ASSERT_EQ(t->capacity, t2->capacity);
  ASSERT_EQ(t->entries, t2->entries);
  ASSERT_EQ(t->removed, t2->removed);
  ASSERT_EQ(t->supplementary_data, t2->supplementary_data);
  ASSERT_EQ(t->cleanup, t2->cleanup);
  ASSERT_NE(t->data, t2->data);
  ASSERT_TRUE(gcu_hash64_contains(t2, hash));
  ASSERT_EQ(gcu_hash64_get(t2, hash).value.ui32, 42);
  gcu_hash64_remove(t2, hash);
  ASSERT_FALSE(gcu_hash64_contains(t2, hash));
  ASSERT_TRUE(gcu_hash64_contains(t, hash));
  gcu_hash64_destroy(t);
  gcu_hash64_destroy(t2);
}

TEST(Hash64, IteratorOnEmpty) {
  auto t = gcu_hash64_create(6);

  GCU_Hash64_Iterator iterator = gcu_hash64_iterator_get(t);
  ASSERT_FALSE(iterator.exists);

  // Cleanup.
  gcu_hash64_destroy(t);
}

TEST(Hash64, Iterator) {
  auto t = gcu_hash64_create(6);

  // Automatically choose collisions.
  size_t capacity = t->capacity;
  size_t hash1 = (capacity / 2) - 1;
  size_t hash2 = hash1 * 37;
  size_t hash3 = hash2 * 37;
  size_t hash4 = hash3 * 37;

  // Add three items that have a hash collision.
  ASSERT_TRUE(gcu_hash64_set(t, hash1, gcu_type64_ui32(hash1)));
  ASSERT_TRUE(gcu_hash64_set(t, hash2, gcu_type64_ui32(hash2)));
  ASSERT_TRUE(gcu_hash64_set(t, hash3, gcu_type64_ui32(hash3)));
  ASSERT_TRUE(gcu_hash64_set(t, hash4, gcu_type64_ui32(hash4)));

  /*
  for (size_t i = 0; i < t->capacity; ++i) {
    if (t->data[i].occupied) {
      printf("%lu\t%u\t%s\n", i, t->data[i].data.ui32, t->data[i].removed ? "true" : "false");
    }
    else {
      printf("%lu\n", i);
    }
  }
  */

  GCU_Hash64_Iterator iterator = gcu_hash64_iterator_get(t);
  ASSERT_EQ(iterator.value.ui32, 185);
  ASSERT_EQ(iterator.hash, iterator.value.ui32);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash64_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui32, 5);
  ASSERT_EQ(iterator.hash, iterator.value.ui32);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash64_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui32, 6845);
  ASSERT_EQ(iterator.hash, iterator.value.ui32);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash64_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui32, 253265);
  ASSERT_EQ(iterator.hash, iterator.value.ui32);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash64_iterator_next(iterator);
  ASSERT_FALSE(iterator.exists);

  // Cleanup.
  gcu_hash64_destroy(t);
}

// Helper function for next test.
static void addOne64(GCU_Hash64 * t) {
  GCU_Hash64_Iterator i = gcu_hash64_iterator_get(t);
  while (i.exists) {
    ++*(size_t *)(t->supplementary_data);
    i = gcu_hash64_iterator_next(i);
  }
}

TEST(Hash64, Cleanup) {
  auto t = gcu_hash64_create(6);
  size_t count = 0;
  t->supplementary_data = (void *)&count;
  t->cleanup = addOne64;
  gcu_hash64_set(t, 0, gcu_type64_b(true));
  gcu_hash64_set(t, 1, gcu_type64_b(true));
  gcu_hash64_set(t, 2, gcu_type64_b(true));
  gcu_hash64_destroy(t);
  ASSERT_EQ(count, 3);
}

TEST(Hash64, InPlaceCleanup) {
  GCU_Hash64 t;
  ASSERT_TRUE(gcu_hash64_create_in_place(&t, 6));
  size_t count = 0;
  t.supplementary_data = (void *)&count;
  t.cleanup = addOne64;
  gcu_hash64_set(&t, 0, gcu_type64_b(true));
  gcu_hash64_set(&t, 1, gcu_type64_b(true));
  gcu_hash64_set(&t, 2, gcu_type64_b(true));
  gcu_hash64_destroy_in_place(&t);
  ASSERT_EQ(count, 3);
}

TEST(Hash32, CreateEmpty) {
  auto t = gcu_hash32_create(0);
  ASSERT_EQ(gcu_hash32_count(t), 0);
  ASSERT_EQ(t->capacity, 0);
  gcu_hash32_destroy(t);
}

TEST(Hash32, Create) {
  auto t = gcu_hash32_create(3);
  ASSERT_EQ(gcu_hash32_count(t), 0);
  ASSERT_EQ(t->capacity, 7);
  gcu_hash32_destroy(t);
}

TEST(Hash32, Set) {
  auto t = gcu_hash32_create(0);
  size_t hash = 1001;
  // Verify the list is empty.
  ASSERT_FALSE(gcu_hash32_contains(t, hash));
  ASSERT_EQ(gcu_hash32_count(t), 0);

  // Add one item to the list.
  ASSERT_TRUE(gcu_hash32_set(t, hash, gcu_type32_ui32(42)));
  ASSERT_TRUE(gcu_hash32_contains(t, hash));
  ASSERT_FALSE(gcu_hash32_contains(t, hash + 1));
  ASSERT_EQ(gcu_hash32_get(t, hash).value.ui32, 42);
  ASSERT_EQ(gcu_hash32_count(t), 1);

  // Add a second item to the list
  ASSERT_TRUE(gcu_hash32_set(t, hash + 1, gcu_type32_ui32(43)));
  ASSERT_TRUE(gcu_hash32_contains(t, hash));
  ASSERT_TRUE(gcu_hash32_contains(t, hash + 1));
  ASSERT_FALSE(gcu_hash32_contains(t, hash + 2));
  ASSERT_EQ(gcu_hash32_get(t, hash).value.ui32, 42);
  ASSERT_EQ(gcu_hash32_get(t, hash + 1).value.ui32, 43);
  ASSERT_EQ(gcu_hash32_count(t), 2);

  // Overwrite an item.
  ASSERT_TRUE(gcu_hash32_set(t, hash, gcu_type32_ui32(7)));
  ASSERT_TRUE(gcu_hash32_contains(t, hash));
  ASSERT_TRUE(gcu_hash32_contains(t, hash + 1));
  ASSERT_FALSE(gcu_hash32_contains(t, hash + 2));
  ASSERT_EQ(gcu_hash32_get(t, hash).value.ui32, 7);
  ASSERT_EQ(gcu_hash32_get(t, hash + 1).value.ui32, 43);
  ASSERT_EQ(gcu_hash32_count(t), 2);

  // Verify internal numbers.
  ASSERT_EQ(t->capacity, 65);
  ASSERT_EQ(t->entries, 2);
  ASSERT_EQ(t->removed, 0);

  // Cleanup.
  gcu_hash32_destroy(t);
}

TEST(Hash32, Remove) {
  auto t = gcu_hash32_create(6);

  // Automatically choose collisions.
  size_t capacity = t->capacity;
  size_t hash1 = capacity / 2;
  size_t hash2 = hash1 + capacity;
  size_t hash3 = hash2 + capacity;
  size_t hash4 = hash3 + capacity;

  // Add three items that have a hash collision.
  ASSERT_TRUE(gcu_hash32_set(t, hash1, gcu_type32_ui32(hash1)));
  ASSERT_TRUE(gcu_hash32_set(t, hash2, gcu_type32_ui32(hash2)));
  ASSERT_TRUE(gcu_hash32_set(t, hash3, gcu_type32_ui32(hash3)));
  ASSERT_EQ(gcu_hash32_get(t, hash1).value.ui32, hash1);
  ASSERT_EQ(gcu_hash32_get(t, hash2).value.ui32, hash2);
  ASSERT_EQ(gcu_hash32_get(t, hash3).value.ui32, hash3);
  ASSERT_EQ(t->capacity, capacity);
  ASSERT_EQ(t->entries, 3);
  ASSERT_EQ(t->removed, 0);
  ASSERT_EQ(gcu_hash32_count(t), 3);

  // Collision order is 1 2 3.
  // Remove all 3.
  ASSERT_TRUE(gcu_hash32_remove(t, hash1));
  ASSERT_TRUE(gcu_hash32_remove(t, hash2));
  ASSERT_TRUE(gcu_hash32_remove(t, hash3));
  ASSERT_EQ(t->capacity, capacity);
  ASSERT_EQ(t->entries, 3);
  ASSERT_EQ(t->removed, 3);
  ASSERT_EQ(gcu_hash32_count(t), 0);

  // Add a new entry at that collision.
  ASSERT_TRUE(gcu_hash32_set(t, hash4, gcu_type32_ui32(hash4)));
  ASSERT_EQ(gcu_hash32_get(t, hash4).value.ui32, hash4);
  ASSERT_EQ(t->capacity, capacity);
  ASSERT_EQ(t->entries, 3);
  ASSERT_EQ(t->removed, 2);
  ASSERT_EQ(gcu_hash32_count(t), 1);

  // Cleanup.
  gcu_hash32_destroy(t);
}

TEST(Hash32, Clone) {
  auto t = gcu_hash32_create(6);
  size_t hash = 1001;
  gcu_hash32_set(t, hash, gcu_type32_ui32(42));
  auto t2 = gcu_hash32_clone(t);
  ASSERT_NE(t2, nullptr);
  ASSERT_NE(t, t2);
  ASSERT_EQ(t->capacity, t2->capacity);
  ASSERT_EQ(t->entries, t2->entries);
  ASSERT_EQ(t->removed, t2->removed);
  ASSERT_EQ(t->supplementary_data, t2->supplementary_data);
  ASSERT_EQ(t->cleanup, t2->cleanup);
  ASSERT_NE(t->data, t2->data);
  ASSERT_TRUE(gcu_hash32_contains(t2, hash));
  ASSERT_EQ(gcu_hash32_get(t2, hash).value.ui32, 42);
  gcu_hash32_remove(t2, hash);
  ASSERT_FALSE(gcu_hash32_contains(t2, hash));
  ASSERT_TRUE(gcu_hash32_contains(t, hash));
  gcu_hash32_destroy(t);
  gcu_hash32_destroy(t2);
}

TEST(Hash32, IteratorOnEmpty) {
  auto t = gcu_hash32_create(6);

  GCU_Hash32_Iterator iterator = gcu_hash32_iterator_get(t);
  ASSERT_FALSE(iterator.exists);

  // Cleanup.
  gcu_hash32_destroy(t);
}

TEST(Hash32, Iterator) {
  auto t = gcu_hash32_create(6);

  // Automatically choose collisions.
  size_t capacity = t->capacity;
  size_t hash1 = (capacity / 2) - 1;
  size_t hash2 = hash1 * 37;
  size_t hash3 = hash2 * 37;
  size_t hash4 = hash3 * 37;

  // Add three items that have a hash collision.
  ASSERT_TRUE(gcu_hash32_set(t, hash1, gcu_type32_ui32(hash1)));
  ASSERT_TRUE(gcu_hash32_set(t, hash2, gcu_type32_ui32(hash2)));
  ASSERT_TRUE(gcu_hash32_set(t, hash3, gcu_type32_ui32(hash3)));
  ASSERT_TRUE(gcu_hash32_set(t, hash4, gcu_type32_ui32(hash4)));

  /*
  for (size_t i = 0; i < t->capacity; ++i) {
    if (t->data[i].occupied) {
      printf("%lu\t%u\t%s\n", i, t->data[i].data.ui32, t->data[i].removed ? "true" : "false");
    }
    else {
      printf("%lu\n", i);
    }
  }
  */

  GCU_Hash32_Iterator iterator = gcu_hash32_iterator_get(t);
  ASSERT_EQ(iterator.value.ui32, 185);
  ASSERT_EQ(iterator.hash, iterator.value.ui32);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash32_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui32, 5);
  ASSERT_EQ(iterator.hash, iterator.value.ui32);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash32_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui32, 6845);
  ASSERT_EQ(iterator.hash, iterator.value.ui32);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash32_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui32, 253265);
  ASSERT_EQ(iterator.hash, iterator.value.ui32);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash32_iterator_next(iterator);
  ASSERT_FALSE(iterator.exists);

  // Cleanup.
  gcu_hash32_destroy(t);
}

// Helper function for next test.
static void addOne32(GCU_Hash32 * t) {
  GCU_Hash32_Iterator i = gcu_hash32_iterator_get(t);
  while (i.exists) {
    ++*(size_t *)(t->supplementary_data);
    i = gcu_hash32_iterator_next(i);
  }
}

TEST(Hash32, Cleanup) {
  auto t = gcu_hash32_create(6);
  size_t count = 0;
  t->supplementary_data = (void *)&count;
  t->cleanup = addOne32;
  gcu_hash32_set(t, 0, gcu_type32_b(true));
  gcu_hash32_set(t, 1, gcu_type32_b(true));
  gcu_hash32_set(t, 2, gcu_type32_b(true));
  gcu_hash32_destroy(t);
  ASSERT_EQ(count, 3);
}

TEST(Hash32, InPlaceCleanup) {
  GCU_Hash32 t;
  ASSERT_TRUE(gcu_hash32_create_in_place(&t, 6));
  size_t count = 0;
  t.supplementary_data = (void *)&count;
  t.cleanup = addOne32;
  gcu_hash32_set(&t, 0, gcu_type32_b(true));
  gcu_hash32_set(&t, 1, gcu_type32_b(true));
  gcu_hash32_set(&t, 2, gcu_type32_b(true));
  gcu_hash32_destroy_in_place(&t);
  ASSERT_EQ(count, 3);
}

TEST(Hash16, CreateEmpty) {
  auto t = gcu_hash16_create(0);
  ASSERT_EQ(gcu_hash16_count(t), 0);
  ASSERT_EQ(t->capacity, 0);
  gcu_hash16_destroy(t);
}

TEST(Hash16, Create) {
  auto t = gcu_hash16_create(3);
  ASSERT_EQ(gcu_hash16_count(t), 0);
  ASSERT_EQ(t->capacity, 7);
  gcu_hash16_destroy(t);
}

TEST(Hash16, Set) {
  auto t = gcu_hash16_create(0);
  size_t hash = 1001;
  // Verify the list is empty.
  ASSERT_FALSE(gcu_hash16_contains(t, hash));
  ASSERT_EQ(gcu_hash16_count(t), 0);

  // Add one item to the list.
  ASSERT_TRUE(gcu_hash16_set(t, hash, gcu_type16_ui16(42)));
  ASSERT_TRUE(gcu_hash16_contains(t, hash));
  ASSERT_FALSE(gcu_hash16_contains(t, hash + 1));
  ASSERT_EQ(gcu_hash16_get(t, hash).value.ui16, 42);
  ASSERT_EQ(gcu_hash16_count(t), 1);

  // Add a second item to the list
  ASSERT_TRUE(gcu_hash16_set(t, hash + 1, gcu_type16_ui16(43)));
  ASSERT_TRUE(gcu_hash16_contains(t, hash));
  ASSERT_TRUE(gcu_hash16_contains(t, hash + 1));
  ASSERT_FALSE(gcu_hash16_contains(t, hash + 2));
  ASSERT_EQ(gcu_hash16_get(t, hash).value.ui16, 42);
  ASSERT_EQ(gcu_hash16_get(t, hash + 1).value.ui16, 43);
  ASSERT_EQ(gcu_hash16_count(t), 2);

  // Overwrite an item.
  ASSERT_TRUE(gcu_hash16_set(t, hash, gcu_type16_ui16(7)));
  ASSERT_TRUE(gcu_hash16_contains(t, hash));
  ASSERT_TRUE(gcu_hash16_contains(t, hash + 1));
  ASSERT_FALSE(gcu_hash16_contains(t, hash + 2));
  ASSERT_EQ(gcu_hash16_get(t, hash).value.ui16, 7);
  ASSERT_EQ(gcu_hash16_get(t, hash + 1).value.ui16, 43);
  ASSERT_EQ(gcu_hash16_count(t), 2);

  // Verify internal numbers.
  ASSERT_EQ(t->capacity, 65);
  ASSERT_EQ(t->entries, 2);
  ASSERT_EQ(t->removed, 0);

  // Cleanup.
  gcu_hash16_destroy(t);
}

TEST(Hash16, Remove) {
  auto t = gcu_hash16_create(6);

  // Automatically choose collisions.
  size_t capacity = t->capacity;
  size_t hash1 = capacity / 2;
  size_t hash2 = hash1 + capacity;
  size_t hash3 = hash2 + capacity;
  size_t hash4 = hash3 + capacity;

  // Add three items that have a hash collision.
  ASSERT_TRUE(gcu_hash16_set(t, hash1, gcu_type16_ui16(hash1)));
  ASSERT_TRUE(gcu_hash16_set(t, hash2, gcu_type16_ui16(hash2)));
  ASSERT_TRUE(gcu_hash16_set(t, hash3, gcu_type16_ui16(hash3)));
  ASSERT_EQ(gcu_hash16_get(t, hash1).value.ui16, hash1);
  ASSERT_EQ(gcu_hash16_get(t, hash2).value.ui16, hash2);
  ASSERT_EQ(gcu_hash16_get(t, hash3).value.ui16, hash3);
  ASSERT_EQ(t->capacity, capacity);
  ASSERT_EQ(t->entries, 3);
  ASSERT_EQ(t->removed, 0);
  ASSERT_EQ(gcu_hash16_count(t), 3);

  // Collision order is 1 2 3.
  // Remove all 3.
  ASSERT_TRUE(gcu_hash16_remove(t, hash1));
  ASSERT_TRUE(gcu_hash16_remove(t, hash2));
  ASSERT_TRUE(gcu_hash16_remove(t, hash3));
  ASSERT_EQ(t->capacity, capacity);
  ASSERT_EQ(t->entries, 3);
  ASSERT_EQ(t->removed, 3);
  ASSERT_EQ(gcu_hash16_count(t), 0);

  // Add a new entry at that collision.
  ASSERT_TRUE(gcu_hash16_set(t, hash4, gcu_type16_ui16(hash4)));
  ASSERT_EQ(gcu_hash16_get(t, hash4).value.ui16, hash4);
  ASSERT_EQ(t->capacity, capacity);
  ASSERT_EQ(t->entries, 3);
  ASSERT_EQ(t->removed, 2);
  ASSERT_EQ(gcu_hash16_count(t), 1);

  // Cleanup.
  gcu_hash16_destroy(t);
}

TEST(Hash16, Clone) {
  auto t = gcu_hash16_create(6);
  size_t hash = 1001;
  gcu_hash16_set(t, hash, gcu_type16_ui16(42));
  auto t2 = gcu_hash16_clone(t);
  ASSERT_NE(t2, nullptr);
  ASSERT_NE(t, t2);
  ASSERT_EQ(t->capacity, t2->capacity);
  ASSERT_EQ(t->entries, t2->entries);
  ASSERT_EQ(t->removed, t2->removed);
  ASSERT_EQ(t->supplementary_data, t2->supplementary_data);
  ASSERT_EQ(t->cleanup, t2->cleanup);
  ASSERT_NE(t->data, t2->data);
  ASSERT_TRUE(gcu_hash16_contains(t2, hash));
  ASSERT_EQ(gcu_hash16_get(t2, hash).value.ui16, 42);
  gcu_hash16_remove(t2, hash);
  ASSERT_FALSE(gcu_hash16_contains(t2, hash));
  ASSERT_TRUE(gcu_hash16_contains(t, hash));
  gcu_hash16_destroy(t);
  gcu_hash16_destroy(t2);
}

TEST(Hash16, IteratorOnEmpty) {
  auto t = gcu_hash16_create(6);

  GCU_Hash16_Iterator iterator = gcu_hash16_iterator_get(t);
  ASSERT_FALSE(iterator.exists);

  // Cleanup.
  gcu_hash16_destroy(t);
}

TEST(Hash16, Iterator) {
  auto t = gcu_hash16_create(6);

  // Automatically choose collisions.
  size_t capacity = t->capacity;
  size_t hash1 = (capacity / 2) - 1;
  size_t hash2 = hash1 * 17;
  size_t hash3 = hash2 * 17;
  size_t hash4 = hash3 * 17;

  // Add three items that have a hash collision.
  ASSERT_TRUE(gcu_hash16_set(t, hash1, gcu_type16_ui16(hash1)));
  ASSERT_TRUE(gcu_hash16_set(t, hash2, gcu_type16_ui16(hash2)));
  ASSERT_TRUE(gcu_hash16_set(t, hash3, gcu_type16_ui16(hash3)));
  ASSERT_TRUE(gcu_hash16_set(t, hash4, gcu_type16_ui16(hash4)));

  /*
  for (size_t i = 0; i < t->capacity; ++i) {
    if (t->data[i].occupied) {
      printf("%lu\t%u\t%s\n", i, t->data[i].data.ui16, t->data[i].removed ? "true" : "false");
    }
    else {
      printf("%lu\n", i);
    }
  }
  */

  GCU_Hash16_Iterator iterator = gcu_hash16_iterator_get(t);
  ASSERT_EQ(iterator.value.ui16, 1445);
  ASSERT_EQ(iterator.hash, iterator.value.ui16);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash16_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui16, 5);
  ASSERT_EQ(iterator.hash, iterator.value.ui16);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash16_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui16, 85);
  ASSERT_EQ(iterator.hash, iterator.value.ui16);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash16_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui16, 24565);
  ASSERT_EQ(iterator.hash, iterator.value.ui16);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash16_iterator_next(iterator);
  ASSERT_FALSE(iterator.exists);

  // Cleanup.
  gcu_hash16_destroy(t);
}

// Helper function for next test.
static void addOne16(GCU_Hash16 * t) {
  GCU_Hash16_Iterator i = gcu_hash16_iterator_get(t);
  while (i.exists) {
    ++*(size_t *)(t->supplementary_data);
    i = gcu_hash16_iterator_next(i);
  }
}

TEST(Hash16, Cleanup) {
  auto t = gcu_hash16_create(6);
  size_t count = 0;
  t->supplementary_data = (void *)&count;
  t->cleanup = addOne16;
  gcu_hash16_set(t, 0, gcu_type16_b(true));
  gcu_hash16_set(t, 1, gcu_type16_b(true));
  gcu_hash16_set(t, 2, gcu_type16_b(true));
  gcu_hash16_destroy(t);
  ASSERT_EQ(count, 3);
}

TEST(Hash16, InPlaceCleanup) {
  GCU_Hash16 t;
  ASSERT_TRUE(gcu_hash16_create_in_place(&t, 6));
  size_t count = 0;
  t.supplementary_data = (void *)&count;
  t.cleanup = addOne16;
  gcu_hash16_set(&t, 0, gcu_type16_b(true));
  gcu_hash16_set(&t, 1, gcu_type16_b(true));
  gcu_hash16_set(&t, 2, gcu_type16_b(true));
  gcu_hash16_destroy_in_place(&t);
  ASSERT_EQ(count, 3);
}

TEST(Hash8, CreateEmpty) {
  auto t = gcu_hash8_create(0);
  ASSERT_EQ(gcu_hash8_count(t), 0);
  ASSERT_EQ(t->capacity, 0);
  gcu_hash8_destroy(t);
}

TEST(Hash8, Create) {
  auto t = gcu_hash8_create(3);
  ASSERT_EQ(gcu_hash8_count(t), 0);
  ASSERT_EQ(t->capacity, 7);
  gcu_hash8_destroy(t);
}

TEST(Hash8, Set) {
  auto t = gcu_hash8_create(0);
  size_t hash = 1001;
  // Verify the list is empty.
  ASSERT_FALSE(gcu_hash8_contains(t, hash));
  ASSERT_EQ(gcu_hash8_count(t), 0);

  // Add one item to the list.
  ASSERT_TRUE(gcu_hash8_set(t, hash, gcu_type8_ui8(42)));
  ASSERT_TRUE(gcu_hash8_contains(t, hash));
  ASSERT_FALSE(gcu_hash8_contains(t, hash + 1));
  ASSERT_EQ(gcu_hash8_get(t, hash).value.ui8, 42);
  ASSERT_EQ(gcu_hash8_count(t), 1);

  // Add a second item to the list
  ASSERT_TRUE(gcu_hash8_set(t, hash + 1, gcu_type8_ui8(43)));
  ASSERT_TRUE(gcu_hash8_contains(t, hash));
  ASSERT_TRUE(gcu_hash8_contains(t, hash + 1));
  ASSERT_FALSE(gcu_hash8_contains(t, hash + 2));
  ASSERT_EQ(gcu_hash8_get(t, hash).value.ui8, 42);
  ASSERT_EQ(gcu_hash8_get(t, hash + 1).value.ui8, 43);
  ASSERT_EQ(gcu_hash8_count(t), 2);

  // Overwrite an item.
  ASSERT_TRUE(gcu_hash8_set(t, hash, gcu_type8_ui8(7)));
  ASSERT_TRUE(gcu_hash8_contains(t, hash));
  ASSERT_TRUE(gcu_hash8_contains(t, hash + 1));
  ASSERT_FALSE(gcu_hash8_contains(t, hash + 2));
  ASSERT_EQ(gcu_hash8_get(t, hash).value.ui8, 7);
  ASSERT_EQ(gcu_hash8_get(t, hash + 1).value.ui8, 43);
  ASSERT_EQ(gcu_hash8_count(t), 2);

  // Verify internal numbers.
  ASSERT_EQ(t->capacity, 65);
  ASSERT_EQ(t->entries, 2);
  ASSERT_EQ(t->removed, 0);

  // Cleanup.
  gcu_hash8_destroy(t);
}

TEST(Hash8, Remove) {
  auto t = gcu_hash8_create(6);

  // Automatically choose collisions.
  size_t capacity = t->capacity;
  size_t hash1 = capacity / 2;
  size_t hash2 = hash1 + capacity;
  size_t hash3 = hash2 + capacity;
  size_t hash4 = hash3 + capacity;

  // Add three items that have a hash collision.
  ASSERT_TRUE(gcu_hash8_set(t, hash1, gcu_type8_ui8(hash1)));
  ASSERT_TRUE(gcu_hash8_set(t, hash2, gcu_type8_ui8(hash2)));
  ASSERT_TRUE(gcu_hash8_set(t, hash3, gcu_type8_ui8(hash3)));
  ASSERT_EQ(gcu_hash8_get(t, hash1).value.ui8, hash1);
  ASSERT_EQ(gcu_hash8_get(t, hash2).value.ui8, hash2);
  ASSERT_EQ(gcu_hash8_get(t, hash3).value.ui8, hash3);
  ASSERT_EQ(t->capacity, capacity);
  ASSERT_EQ(t->entries, 3);
  ASSERT_EQ(t->removed, 0);
  ASSERT_EQ(gcu_hash8_count(t), 3);

  // Collision order is 1 2 3.
  // Remove all 3.
  ASSERT_TRUE(gcu_hash8_remove(t, hash1));
  ASSERT_TRUE(gcu_hash8_remove(t, hash2));
  ASSERT_TRUE(gcu_hash8_remove(t, hash3));
  ASSERT_EQ(t->capacity, capacity);
  ASSERT_EQ(t->entries, 3);
  ASSERT_EQ(t->removed, 3);
  ASSERT_EQ(gcu_hash8_count(t), 0);

  // Add a new entry at that collision.
  ASSERT_TRUE(gcu_hash8_set(t, hash4, gcu_type8_ui8(hash4)));
  ASSERT_EQ(gcu_hash8_get(t, hash4).value.ui8, hash4);
  ASSERT_EQ(t->capacity, capacity);
  ASSERT_EQ(t->entries, 3);
  ASSERT_EQ(t->removed, 2);
  ASSERT_EQ(gcu_hash8_count(t), 1);

  // Cleanup.
  gcu_hash8_destroy(t);
}

TEST(Hash8, Clone) {
  auto t = gcu_hash8_create(6);
  size_t hash = 1001;
  gcu_hash8_set(t, hash, gcu_type8_ui8(42));
  auto t2 = gcu_hash8_clone(t);
  ASSERT_NE(t2, nullptr);
  ASSERT_NE(t, t2);
  ASSERT_EQ(t->capacity, t2->capacity);
  ASSERT_EQ(t->entries, t2->entries);
  ASSERT_EQ(t->removed, t2->removed);
  ASSERT_EQ(t->supplementary_data, t2->supplementary_data);
  ASSERT_EQ(t->cleanup, t2->cleanup);
  ASSERT_NE(t->data, t2->data);
  ASSERT_TRUE(gcu_hash8_contains(t2, hash));
  ASSERT_EQ(gcu_hash8_get(t2, hash).value.ui8, 42);
  gcu_hash8_remove(t2, hash);
  ASSERT_FALSE(gcu_hash8_contains(t2, hash));
  ASSERT_TRUE(gcu_hash8_contains(t, hash));
  gcu_hash8_destroy(t);
  gcu_hash8_destroy(t2);
}

TEST(Hash8, IteratorOnEmpty) {
  auto t = gcu_hash8_create(6);

  GCU_Hash8_Iterator iterator = gcu_hash8_iterator_get(t);
  ASSERT_FALSE(iterator.exists);

  // Cleanup.
  gcu_hash8_destroy(t);
}

TEST(Hash8, Iterator) {
  auto t = gcu_hash8_create(6);

  // Automatically choose collisions.
  size_t capacity = t->capacity;
  size_t hash1 = (capacity / 2) - 1;
  size_t hash2 = hash1 * 7;
  size_t hash3 = hash2 * 7;
  size_t hash4 = hash3 / 11;

  // Add three items that have a hash collision.
  ASSERT_TRUE(gcu_hash8_set(t, hash1, gcu_type8_ui8(hash1)));
  ASSERT_TRUE(gcu_hash8_set(t, hash2, gcu_type8_ui8(hash2)));
  ASSERT_TRUE(gcu_hash8_set(t, hash3, gcu_type8_ui8(hash3)));
  ASSERT_TRUE(gcu_hash8_set(t, hash4, gcu_type8_ui8(hash4)));

  /*
  for (size_t i = 0; i < t->capacity; ++i) {
    if (t->data[i].occupied) {
      printf("%lu\t%u\t%s\n", i, t->data[i].data.ui8, t->data[i].removed ? "true" : "false");
    }
    else {
      printf("%lu\n", i);
    }
  }
  */

  GCU_Hash8_Iterator iterator = gcu_hash8_iterator_get(t);
  ASSERT_EQ(iterator.value.ui8, 5);
  ASSERT_EQ(iterator.hash, iterator.value.ui8);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash8_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui8, 35);
  ASSERT_EQ(iterator.hash, iterator.value.ui8);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash8_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui8, 22);
  ASSERT_EQ(iterator.hash, iterator.value.ui8);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash8_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui8, 245);
  ASSERT_EQ(iterator.hash, iterator.value.ui8);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash8_iterator_next(iterator);
  ASSERT_FALSE(iterator.exists);

  // Cleanup.
  gcu_hash8_destroy(t);
}

// Helper function for next test.
static void addOne8(GCU_Hash8 * t) {
  GCU_Hash8_Iterator i = gcu_hash8_iterator_get(t);
  while (i.exists) {
    ++*(size_t *)(t->supplementary_data);
    i = gcu_hash8_iterator_next(i);
  }
}

TEST(Hash8, Cleanup) {
  auto t = gcu_hash8_create(6);
  size_t count = 0;
  t->supplementary_data = (void *)&count;
  t->cleanup = addOne8;
  gcu_hash8_set(t, 0, gcu_type8_b(true));
  gcu_hash8_set(t, 1, gcu_type8_b(true));
  gcu_hash8_set(t, 2, gcu_type8_b(true));
  gcu_hash8_destroy(t);
  ASSERT_EQ(count, 3);
}

TEST(Hash8, InPlaceCleanup) {
  GCU_Hash8 t;
  ASSERT_TRUE(gcu_hash8_create_in_place(&t, 6));
  size_t count = 0;
  t.supplementary_data = (void *)&count;
  t.cleanup = addOne8;
  gcu_hash8_set(&t, 0, gcu_type8_b(true));
  gcu_hash8_set(&t, 1, gcu_type8_b(true));
  gcu_hash8_set(&t, 2, gcu_type8_b(true));
  gcu_hash8_destroy_in_place(&t);
  ASSERT_EQ(count, 3);
}

//
// Growth and the cleanup callback
//

namespace {

/** Records how many times the table's cleanup callback ran. */
void count_cleanup(GCU_Hash64 * table) {
  ++*(size_t *)table->supplementary_data;
}

} // namespace

TEST(Hash64, GrowthDoesNotRunTheCleanupCallback) {
  // Growing moves the entries into a larger table; it does not discard them.
  // Running the caller's cleanup over them frees values the table is still
  // holding, which is a use-after-free the moment one is read back.
  size_t cleanups = 0;
  GCU_Hash64 * t = gcu_hash64_create(4);
  ASSERT_NE(t, nullptr);
  t->supplementary_data = &cleanups;
  t->cleanup = count_cleanup;

  size_t capacity_before = t->capacity;
  for (size_t i = 0; i < 200; i++) {
    ASSERT_TRUE(gcu_hash64_set(t, i, gcu_type64_ui64(i)));
  }
  ASSERT_GT(t->capacity, capacity_before) << "the table must have grown";
  EXPECT_EQ(cleanups, 0u) << "cleanup ran while the table was growing";

  // Every entry must still be readable: if cleanup had freed them, this is
  // where a real consumer would read released memory.
  for (size_t i = 0; i < 200; i++) {
    GCU_Hash64_Value v = gcu_hash64_get(t, i);
    ASSERT_TRUE(v.exists) << "entry " << i << " lost during growth";
    EXPECT_EQ(v.value.ui64, i);
  }

  gcu_hash64_destroy(t);
  EXPECT_EQ(cleanups, 1u) << "cleanup should run exactly once, at destruction";
}

TEST(Hash64, CleanupSurvivesGrowth) {
  // The growth path swaps the old and new tables before destroying the
  // temporary, which used to leave the surviving table without the cleanup
  // callback - so it never ran and whatever it was responsible for leaked.
  size_t cleanups = 0;
  GCU_Hash64 t;
  ASSERT_TRUE(gcu_hash64_create_in_place(&t, 4));
  t.supplementary_data = &cleanups;
  t.cleanup = count_cleanup;

  for (size_t i = 0; i < 200; i++) {
    ASSERT_TRUE(gcu_hash64_set(&t, i, gcu_type64_ui64(i)));
  }
  EXPECT_NE(t.cleanup, nullptr) << "growth dropped the cleanup callback";
  EXPECT_EQ(t.supplementary_data, &cleanups)
      << "growth dropped the supplementary data the callback needs";

  gcu_hash64_destroy_in_place(&t);
  EXPECT_EQ(cleanups, 1u);
}

//
// Lookup cost
//

TEST(Hash64, LookupDoesNotScanTheWholeTable) {
  // get() and contains() used to walk every cell from the start of the table
  // rather than probing from the key's own bucket, which made a lookup
  // O(capacity) - worse than a linear scan of a plain array, because capacity
  // is always more than twice the entry count and the empty cells were
  // visited too.
  //
  // Correctness cannot catch that; only cost can. The per-lookup time is
  // measured in a small table and a table 1000 times larger: with bucket
  // probing the two are within noise of each other, while a full scan makes
  // the large one roughly three orders of magnitude slower. The threshold is
  // deliberately far from both, so ordinary timing jitter - or running the
  // whole suite under Valgrind, which slows both sides equally - cannot
  // reach it.
  const size_t small_entries = 100;
  const size_t large_entries = 100000;
  const size_t lookups = 200000;

  auto fill = [](size_t entries) {
    GCU_Hash64 * t = gcu_hash64_create(entries);
    for (size_t i = 0; i < entries; i++) {
      gcu_hash64_set(t, i * 2654435761u, gcu_type64_ui64(i));
    }
    return t;
  };

  auto time_lookups = [&](GCU_Hash64 * t, size_t entries) {
    auto start = std::chrono::steady_clock::now();
    uint64_t sink = 0;
    for (size_t i = 0; i < lookups; i++) {
      GCU_Hash64_Value v =
          gcu_hash64_get(t, (i % entries) * 2654435761u);
      sink += v.value.ui64;
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    // Keep the loop from being optimized away entirely.
    EXPECT_GT(sink, 0u);
    return std::chrono::duration<double>(elapsed).count();
  };

  GCU_Hash64 * small = fill(small_entries);
  GCU_Hash64 * large = fill(large_entries);
  ASSERT_EQ(gcu_hash64_count(large), large_entries);

  double small_time = time_lookups(small, small_entries);
  double large_time = time_lookups(large, large_entries);

  gcu_hash64_destroy(small);
  gcu_hash64_destroy(large);

  ASSERT_GT(small_time, 0.0);
  double ratio = large_time / small_time;
  EXPECT_LT(ratio, 25.0)
      << "lookups in a table " << (large_entries / small_entries)
      << "x larger took " << ratio
      << "x as long; that is a scan of the table, not a probe";
}

TEST(Hash64, GrowsPastTheFractionalGrowthThreshold) {
  // Growth steps to 32, then doubles, and past 1024 switches to multiplying
  // by GROWTH_FACTOR - a double, so the new size is computed in floating
  // point and converted back. No test had ever taken a table past 1024, so
  // that third branch had never run; a factor that rounded down to the
  // current capacity would leave the table unable to grow at all.
  GCU_Hash64 * t = gcu_hash64_create(4);
  ASSERT_NE(t, nullptr);

  const size_t entries = 4000;
  for (size_t i = 0; i < entries; i++) {
    ASSERT_TRUE(gcu_hash64_set(t, i * 2654435761u, gcu_type64_ui64(i)))
        << "insert " << i;
  }
  ASSERT_GT(t->capacity, 1024u) << "the fractional branch was not reached";
  EXPECT_EQ(gcu_hash64_count(t), entries);

  for (size_t i = 0; i < entries; i++) {
    GCU_Hash64_Value v = gcu_hash64_get(t, i * 2654435761u);
    ASSERT_TRUE(v.exists) << "entry " << i << " lost";
    EXPECT_EQ(v.value.ui64, i) << "entry " << i;
  }

  gcu_hash64_destroy(t);
}

TEST(Hash64, RemoveOnATableThatNeverAllocatedIsSafe) {
  // A table created with a count of zero has no cells until its first
  // insertion. remove() computed hash % capacity before checking, so this
  // divided by zero.
  GCU_Hash64 * t = gcu_hash64_create(0);
  ASSERT_NE(t, nullptr);
  ASSERT_EQ(t->capacity, 0u);

  EXPECT_FALSE(gcu_hash64_remove(t, 12345));
  EXPECT_FALSE(gcu_hash64_contains(t, 12345));
  EXPECT_FALSE(gcu_hash64_get(t, 12345).exists);

  // Still usable afterwards.
  EXPECT_TRUE(gcu_hash64_set(t, 12345, gcu_type64_ui64(7)));
  EXPECT_TRUE(gcu_hash64_get(t, 12345).exists);
  gcu_hash64_destroy(t);
}

TEST(Hash64, LookupFindsEntriesPastATombstone) {
  // Probing has to walk past removed cells rather than stopping at them, or
  // an entry that collided with something since deleted becomes unreachable.
  GCU_Hash64 * t = gcu_hash64_create(8);
  ASSERT_NE(t, nullptr);
  size_t capacity = t->capacity;

  // Three keys that land in the same bucket, so they form a probe run.
  size_t a = 5;
  size_t b = 5 + capacity;
  size_t c = 5 + (2 * capacity);
  ASSERT_TRUE(gcu_hash64_set(t, a, gcu_type64_ui64(1)));
  ASSERT_TRUE(gcu_hash64_set(t, b, gcu_type64_ui64(2)));
  ASSERT_TRUE(gcu_hash64_set(t, c, gcu_type64_ui64(3)));

  // Remove the middle of the run; the one after it must still be found.
  ASSERT_TRUE(gcu_hash64_remove(t, b));
  EXPECT_FALSE(gcu_hash64_get(t, b).exists);
  EXPECT_EQ(gcu_hash64_get(t, a).value.ui64, 1u);
  EXPECT_EQ(gcu_hash64_get(t, c).value.ui64, 3u)
      << "entry after a tombstone must remain reachable";

  // And the first of the run, too.
  ASSERT_TRUE(gcu_hash64_remove(t, a));
  EXPECT_EQ(gcu_hash64_get(t, c).value.ui64, 3u);

  gcu_hash64_destroy(t);
}

TEST(Hash64, CloneOfATableWithNoCells) {
  // A table created with a count of zero has no cell array at all -- capacity
  // 0, data null -- and cloning one used to pass that null to memcpy, ask the
  // allocator for zero bytes, and treat a null answer as failure. It also
  // produced a table whose "empty" did not look like any other empty table's.
  //
  // This one really was a coverage gap: nothing cloned an empty table. The
  // growth path below is the opposite case, so the two are worth keeping
  // straight -- see the note on Hash64.Set.
  auto t = gcu_hash64_create(0);
  ASSERT_EQ(t->capacity, 0);
  ASSERT_EQ(t->data, nullptr);

  auto t2 = gcu_hash64_clone(t);
  ASSERT_NE(t2, nullptr) << "an empty table must still clone";
  ASSERT_NE(t, t2);
  ASSERT_EQ(t2->capacity, 0);
  ASSERT_EQ(t2->data, nullptr)
      << "clone and create must agree on what an empty table looks like";
  ASSERT_EQ(gcu_hash64_count(t2), 0);

  // And the copy is a working table, not just a well-formed empty one.
  ASSERT_TRUE(gcu_hash64_set(t2, 1001, gcu_type64_ui32(7)));
  ASSERT_TRUE(gcu_hash64_contains(t2, 1001));
  ASSERT_FALSE(gcu_hash64_contains(t, 1001))
      << "the clone must not share the original's cells";

  gcu_hash64_destroy(t);
  gcu_hash64_destroy(t2);
}

TEST(Hash64, AdvancingAnExhaustedIteratorIsHarmless) {
  // The documented loop stops on `exists`, so this is about a caller who
  // steps one past the end. It should hand back an exhausted iterator rather
  // than forming a pointer into a cell array that does not exist.
  auto t = gcu_hash64_create(0);

  GCU_Hash64_Iterator iterator = gcu_hash64_iterator_get(t);
  ASSERT_FALSE(iterator.exists);
  iterator = gcu_hash64_iterator_next(iterator);
  ASSERT_FALSE(iterator.exists);

  gcu_hash64_destroy(t);
}

TEST(Hash32, CloneOfATableWithNoCells) {
  // A table created with a count of zero has no cell array at all -- capacity
  // 0, data null -- and cloning one used to pass that null to memcpy, ask the
  // allocator for zero bytes, and treat a null answer as failure. It also
  // produced a table whose "empty" did not look like any other empty table's.
  //
  // This one really was a coverage gap: nothing cloned an empty table. The
  // growth path below is the opposite case, so the two are worth keeping
  // straight -- see the note on Hash64.Set.
  auto t = gcu_hash32_create(0);
  ASSERT_EQ(t->capacity, 0);
  ASSERT_EQ(t->data, nullptr);

  auto t2 = gcu_hash32_clone(t);
  ASSERT_NE(t2, nullptr) << "an empty table must still clone";
  ASSERT_NE(t, t2);
  ASSERT_EQ(t2->capacity, 0);
  ASSERT_EQ(t2->data, nullptr)
      << "clone and create must agree on what an empty table looks like";
  ASSERT_EQ(gcu_hash32_count(t2), 0);

  // And the copy is a working table, not just a well-formed empty one.
  ASSERT_TRUE(gcu_hash32_set(t2, 1001, gcu_type32_ui32(7)));
  ASSERT_TRUE(gcu_hash32_contains(t2, 1001));
  ASSERT_FALSE(gcu_hash32_contains(t, 1001))
      << "the clone must not share the original's cells";

  gcu_hash32_destroy(t);
  gcu_hash32_destroy(t2);
}

TEST(Hash32, AdvancingAnExhaustedIteratorIsHarmless) {
  // The documented loop stops on `exists`, so this is about a caller who
  // steps one past the end. It should hand back an exhausted iterator rather
  // than forming a pointer into a cell array that does not exist.
  auto t = gcu_hash32_create(0);

  GCU_Hash32_Iterator iterator = gcu_hash32_iterator_get(t);
  ASSERT_FALSE(iterator.exists);
  iterator = gcu_hash32_iterator_next(iterator);
  ASSERT_FALSE(iterator.exists);

  gcu_hash32_destroy(t);
}

TEST(Hash16, CloneOfATableWithNoCells) {
  // A table created with a count of zero has no cell array at all -- capacity
  // 0, data null -- and cloning one used to pass that null to memcpy, ask the
  // allocator for zero bytes, and treat a null answer as failure. It also
  // produced a table whose "empty" did not look like any other empty table's.
  //
  // This one really was a coverage gap: nothing cloned an empty table. The
  // growth path below is the opposite case, so the two are worth keeping
  // straight -- see the note on Hash64.Set.
  auto t = gcu_hash16_create(0);
  ASSERT_EQ(t->capacity, 0);
  ASSERT_EQ(t->data, nullptr);

  auto t2 = gcu_hash16_clone(t);
  ASSERT_NE(t2, nullptr) << "an empty table must still clone";
  ASSERT_NE(t, t2);
  ASSERT_EQ(t2->capacity, 0);
  ASSERT_EQ(t2->data, nullptr)
      << "clone and create must agree on what an empty table looks like";
  ASSERT_EQ(gcu_hash16_count(t2), 0);

  // And the copy is a working table, not just a well-formed empty one.
  ASSERT_TRUE(gcu_hash16_set(t2, 1001, gcu_type16_ui16(7)));
  ASSERT_TRUE(gcu_hash16_contains(t2, 1001));
  ASSERT_FALSE(gcu_hash16_contains(t, 1001))
      << "the clone must not share the original's cells";

  gcu_hash16_destroy(t);
  gcu_hash16_destroy(t2);
}

TEST(Hash16, AdvancingAnExhaustedIteratorIsHarmless) {
  // The documented loop stops on `exists`, so this is about a caller who
  // steps one past the end. It should hand back an exhausted iterator rather
  // than forming a pointer into a cell array that does not exist.
  auto t = gcu_hash16_create(0);

  GCU_Hash16_Iterator iterator = gcu_hash16_iterator_get(t);
  ASSERT_FALSE(iterator.exists);
  iterator = gcu_hash16_iterator_next(iterator);
  ASSERT_FALSE(iterator.exists);

  gcu_hash16_destroy(t);
}

TEST(Hash8, CloneOfATableWithNoCells) {
  // A table created with a count of zero has no cell array at all -- capacity
  // 0, data null -- and cloning one used to pass that null to memcpy, ask the
  // allocator for zero bytes, and treat a null answer as failure. It also
  // produced a table whose "empty" did not look like any other empty table's.
  //
  // This one really was a coverage gap: nothing cloned an empty table. The
  // growth path below is the opposite case, so the two are worth keeping
  // straight -- see the note on Hash64.Set.
  auto t = gcu_hash8_create(0);
  ASSERT_EQ(t->capacity, 0);
  ASSERT_EQ(t->data, nullptr);

  auto t2 = gcu_hash8_clone(t);
  ASSERT_NE(t2, nullptr) << "an empty table must still clone";
  ASSERT_NE(t, t2);
  ASSERT_EQ(t2->capacity, 0);
  ASSERT_EQ(t2->data, nullptr)
      << "clone and create must agree on what an empty table looks like";
  ASSERT_EQ(gcu_hash8_count(t2), 0);

  // And the copy is a working table, not just a well-formed empty one.
  ASSERT_TRUE(gcu_hash8_set(t2, 1001, gcu_type8_ui8(7)));
  ASSERT_TRUE(gcu_hash8_contains(t2, 1001));
  ASSERT_FALSE(gcu_hash8_contains(t, 1001))
      << "the clone must not share the original's cells";

  gcu_hash8_destroy(t);
  gcu_hash8_destroy(t2);
}

TEST(Hash8, AdvancingAnExhaustedIteratorIsHarmless) {
  // The documented loop stops on `exists`, so this is about a caller who
  // steps one past the end. It should hand back an exhausted iterator rather
  // than forming a pointer into a cell array that does not exist.
  auto t = gcu_hash8_create(0);

  GCU_Hash8_Iterator iterator = gcu_hash8_iterator_get(t);
  ASSERT_FALSE(iterator.exists);
  iterator = gcu_hash8_iterator_next(iterator);
  ASSERT_FALSE(iterator.exists);

  gcu_hash8_destroy(t);
}

TEST(Hash64, GrowingUnderTheLockKeepsTheLock) {
  // Callers guard a table with its own mutex and may grow it while holding
  // that mutex -- the thread module does exactly this in gcu_thread_create().
  // Growth once swapped whole structs, writing a freshly initialised mutex
  // over the held one, and anyone already queued on it was never woken.  That
  // hung the thread tests on Windows; it is not rarer on Linux, which was the
  // first guess -- once a waiter is actually queued, glibc loses it every time
  // too, measured 5/5.  So: queue a waiter, grow, release, and see it get in.
  auto t = gcu_hash64_create(0);
  ASSERT_NE(t, nullptr);

  GCU_MUTEX_LOCK(t->mutex);
  std::atomic<bool> acquired{false};
  std::thread waiter([&] {
    GCU_MUTEX_LOCK(t->mutex);
    acquired = true;
    GCU_MUTEX_UNLOCK(t->mutex);
  });
  // Long enough for the waiter to be queued on the lock rather than merely
  // started.  Too short makes this pass without testing anything, never fail.
  this_thread::sleep_for(chrono::milliseconds(100));

  for (uint64_t i = 0; i < 100; ++i) {
    ASSERT_TRUE(gcu_hash64_set(t, i, gcu_type64_ui32((uint32_t)i)));
  }
  GCU_MUTEX_UNLOCK(t->mutex);

  auto deadline = chrono::steady_clock::now() + chrono::seconds(10);
  while (!acquired && chrono::steady_clock::now() < deadline) {
    this_thread::sleep_for(chrono::milliseconds(1));
  }
  if (!acquired) {
    // The waiter is stuck for good; leave it behind rather than hang here.
    waiter.detach();
    FAIL() << "a thread queued on the table's mutex was never woken";
  }
  waiter.join();
  gcu_hash64_destroy(t);
}

TEST(Hash64, GrowingUnderTheLockDoesNotReleaseIt) {
  // The silent half of the same defect, and the worse half.  A grow that reset
  // the lock word needed no waiter to do damage: with nobody queued at all the
  // lock simply came back unheld, and the next thread to ask walked into a
  // critical section whose owner still believed it held it.  No hang and no
  // diagnostic -- just two threads inside one table.
  //
  // Unlike the waiter above, nothing here sleeps or waits on a deadline: a
  // try-lock answers at once, so this fails in milliseconds rather than ten
  // seconds, and it cannot pass for having been too quick.
  auto t = gcu_hash64_create(0);
  ASSERT_NE(t, nullptr);

  GCU_MUTEX_LOCK(t->mutex);
  for (uint64_t i = 0; i < 100; ++i) {
    ASSERT_TRUE(gcu_hash64_set(t, i, gcu_type64_ui32((uint32_t)i)));
  }

  // From another thread, because the question is whether someone *else* can
  // get in.  These mutexes are not recursive, so asking from this one would
  // answer a different question on Linux and deadlock on a platform whose
  // lock was.
  std::atomic<bool> stole{false};
  std::thread thief([&] {
    if (GCU_MUTEX_TRYLOCK(t->mutex) == 0) {
      stole = true;
      GCU_MUTEX_UNLOCK(t->mutex);
    }
  });
  thief.join();

  GCU_MUTEX_UNLOCK(t->mutex);
  EXPECT_FALSE(stole) << "growing the table released a mutex that was held";
  gcu_hash64_destroy(t);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

