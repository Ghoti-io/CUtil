/**
 * @file
 *
 * Test the behavior of Ghoti.io Util HasParameters class..
 */

#include <sstream>
#include <gtest/gtest.h>
#include "cutil/hash.h"

using namespace std;

TEST(Hash, CreateEmpty) {
  auto t = gcu_hash_create(0);
  ASSERT_EQ(gcu_hash_count(t), 0);
  ASSERT_EQ(t->capacity, 0);
  gcu_hash_destroy(t);
}

TEST(Hash, Create) {
  auto t = gcu_hash_create(3);
  ASSERT_EQ(gcu_hash_count(t), 0);
  ASSERT_EQ(t->capacity, 7);
  gcu_hash_destroy(t);
}

TEST(Hash, Set) {
  auto t = gcu_hash_create(0);
  size_t hash = 1001;
  // Verify the list is empty.
  ASSERT_FALSE(gcu_hash_contains(t, hash));
  ASSERT_EQ(gcu_hash_count(t), 0);

  // Add one item to the list.
  ASSERT_TRUE(gcu_hash_set(t, hash, gcu_type_ui32(42)));
  ASSERT_TRUE(gcu_hash_contains(t, hash));
  ASSERT_FALSE(gcu_hash_contains(t, hash + 1));
  ASSERT_EQ(gcu_hash_get(t, hash).value.ui32, 42);
  ASSERT_EQ(gcu_hash_count(t), 1);

  // Add a second item to the list
  ASSERT_TRUE(gcu_hash_set(t, hash + 1, gcu_type_ui32(43)));
  ASSERT_TRUE(gcu_hash_contains(t, hash));
  ASSERT_TRUE(gcu_hash_contains(t, hash + 1));
  ASSERT_FALSE(gcu_hash_contains(t, hash + 2));
  ASSERT_EQ(gcu_hash_get(t, hash).value.ui32, 42);
  ASSERT_EQ(gcu_hash_get(t, hash + 1).value.ui32, 43);
  ASSERT_EQ(gcu_hash_count(t), 2);

  // Overwrite an item.
  ASSERT_TRUE(gcu_hash_set(t, hash, gcu_type_ui32(7)));
  ASSERT_TRUE(gcu_hash_contains(t, hash));
  ASSERT_TRUE(gcu_hash_contains(t, hash + 1));
  ASSERT_FALSE(gcu_hash_contains(t, hash + 2));
  ASSERT_EQ(gcu_hash_get(t, hash).value.ui32, 7);
  ASSERT_EQ(gcu_hash_get(t, hash + 1).value.ui32, 43);
  ASSERT_EQ(gcu_hash_count(t), 2);

  // Verify internal numbers.
  ASSERT_EQ(t->capacity, 65);
  ASSERT_EQ(t->entries, 2);
  ASSERT_EQ(t->removed, 0);

  // Cleanup.
  gcu_hash_destroy(t);
}

TEST(Hash, Remove) {
  auto t = gcu_hash_create(6);

  // Automatically choose collisions.
  size_t capacity = t->capacity;
  size_t hash1 = capacity / 2;
  size_t hash2 = hash1 + capacity;
  size_t hash3 = hash2 + capacity;
  size_t hash4 = hash3 + capacity;

  // Add three items that have a hash collision.
  ASSERT_TRUE(gcu_hash_set(t, hash1, gcu_type_ui32(hash1)));
  ASSERT_TRUE(gcu_hash_set(t, hash2, gcu_type_ui32(hash2)));
  ASSERT_TRUE(gcu_hash_set(t, hash3, gcu_type_ui32(hash3)));
  ASSERT_EQ(gcu_hash_get(t, hash1).value.ui32, hash1);
  ASSERT_EQ(gcu_hash_get(t, hash2).value.ui32, hash2);
  ASSERT_EQ(gcu_hash_get(t, hash3).value.ui32, hash3);
  ASSERT_EQ(t->capacity, capacity);
  ASSERT_EQ(t->entries, 3);
  ASSERT_EQ(t->removed, 0);
  ASSERT_EQ(gcu_hash_count(t), 3);

  // Collision order is 1 2 3.
  // Remove all 3.
  ASSERT_TRUE(gcu_hash_remove(t, hash1));
  ASSERT_TRUE(gcu_hash_remove(t, hash2));
  ASSERT_TRUE(gcu_hash_remove(t, hash3));
  ASSERT_EQ(t->capacity, capacity);
  ASSERT_EQ(t->entries, 3);
  ASSERT_EQ(t->removed, 3);
  ASSERT_EQ(gcu_hash_count(t), 0);

  // Add a new entry at that collision.
  ASSERT_TRUE(gcu_hash_set(t, hash4, gcu_type_ui32(hash4)));
  ASSERT_EQ(gcu_hash_get(t, hash4).value.ui32, hash4);
  ASSERT_EQ(t->capacity, capacity);
  ASSERT_EQ(t->entries, 3);
  ASSERT_EQ(t->removed, 2);
  ASSERT_EQ(gcu_hash_count(t), 1);

  // Cleanup.
  gcu_hash_destroy(t);
}

TEST(Hash, IteratorOnEmpty) {
  auto t = gcu_hash_create(6);

  GCU_Hash_Iterator iterator = gcu_hash_iterator_get(t);
  ASSERT_FALSE(iterator.exists);

  // Cleanup.
  gcu_hash_destroy(t);
}

TEST(Hash, Iterator) {
  auto t = gcu_hash_create(6);

  // Automatically choose collisions.
  size_t capacity = t->capacity;
  size_t hash1 = (capacity / 2) - 1;
  size_t hash2 = hash1 * 37;
  size_t hash3 = hash2 * 37;
  size_t hash4 = hash3 * 37;

  // Add three items that have a hash collision.
  ASSERT_TRUE(gcu_hash_set(t, hash1, gcu_type_ui32(hash1)));
  ASSERT_TRUE(gcu_hash_set(t, hash2, gcu_type_ui32(hash2)));
  ASSERT_TRUE(gcu_hash_set(t, hash3, gcu_type_ui32(hash3)));
  ASSERT_TRUE(gcu_hash_set(t, hash4, gcu_type_ui32(hash4)));

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

  GCU_Hash_Iterator iterator = gcu_hash_iterator_get(t);
  ASSERT_EQ(iterator.value.ui32, 185);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui32, 5);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui32, 6845);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui32, 253265);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash_iterator_next(iterator);
  ASSERT_FALSE(iterator.exists);

  // Cleanup.
  gcu_hash_destroy(t);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

