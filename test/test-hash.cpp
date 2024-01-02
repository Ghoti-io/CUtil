#include <sstream>
#include <gtest/gtest.h>
#include "cutil/hash.h"

using namespace std;

TEST(Hash64, CreateEmpty) {
  auto t = gcu_hash64_create(0);
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
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash64_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui32, 5);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash64_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui32, 6845);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash64_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui32, 253265);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash64_iterator_next(iterator);
  ASSERT_FALSE(iterator.exists);

  // Cleanup.
  gcu_hash64_destroy(t);
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
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash32_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui32, 5);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash32_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui32, 6845);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash32_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui32, 253265);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash32_iterator_next(iterator);
  ASSERT_FALSE(iterator.exists);

  // Cleanup.
  gcu_hash32_destroy(t);
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
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash16_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui16, 5);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash16_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui16, 85);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash16_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui16, 24565);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash16_iterator_next(iterator);
  ASSERT_FALSE(iterator.exists);

  // Cleanup.
  gcu_hash16_destroy(t);
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
  size_t hash4 = hash3 * 7;

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
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash8_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui8, 35);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash8_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui8, 245);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash8_iterator_next(iterator);
  ASSERT_EQ(iterator.value.ui8, 179);
  ASSERT_TRUE(iterator.exists);

  iterator = gcu_hash8_iterator_next(iterator);
  ASSERT_FALSE(iterator.exists);

  // Cleanup.
  gcu_hash8_destroy(t);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

