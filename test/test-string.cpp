#include <sstream>
#include <gtest/gtest.h>
#include "cutil/string.h"

using namespace std;

TEST(Murmur3, SeedCheck) {
  // Verify that changing the seed will result in a different hash for empty
  // strings.
  {
    uint32_t out1, out2;
    gcu_string_murmur3_32("", 0, 0, &out1);
    gcu_string_murmur3_32("", 0, 1, &out2);
    ASSERT_NE(out1, out2);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x86_128("", 0, 0, &out1);
    gcu_string_murmur3_x86_128("", 0, 1, &out2);
    ASSERT_NE(out1[0], out2[0]);
    ASSERT_NE(out1[1], out2[1]);
    ASSERT_EQ(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x64_128("", 0, 0, &out1);
    gcu_string_murmur3_x64_128("", 0, 1, &out2);
    ASSERT_NE(out1[0], out2[0]);
    ASSERT_NE(out1[1], out2[1]);
    ASSERT_EQ(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
  // Verify that changing the seed will result in a different hash for a
  // non-empty string.
  {
    uint32_t out1, out2;
    gcu_string_murmur3_32("hello", 5, 0, &out1);
    gcu_string_murmur3_32("hello", 5, 1, &out2);
    ASSERT_NE(out1, out2);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x86_128("hello", 5, 0, &out1);
    gcu_string_murmur3_x86_128("hello", 5, 1, &out2);
    ASSERT_NE(out1[0], out2[0]);
    ASSERT_NE(out1[1], out2[1]);
    ASSERT_NE(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x64_128("hello", 5, 0, &out1);
    gcu_string_murmur3_x64_128("hello", 5, 1, &out2);
    ASSERT_NE(out1[0], out2[0]);
    ASSERT_NE(out1[1], out2[1]);
    ASSERT_NE(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
}

TEST(Murmur3, HashCheck) {
  // Verify that an empty string has a different hash from a string containing
  // a null character.
  {
    uint32_t out1, out2;
    gcu_string_murmur3_32("", 0, 0, &out1);
    gcu_string_murmur3_32("\0", 1, 0, &out2);
    ASSERT_NE(out1, out2);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x86_128("", 0, 0, &out1);
    gcu_string_murmur3_x86_128("\0", 1, 0, &out2);
    ASSERT_NE(out1[0], out2[0]);
    ASSERT_NE(out1[1], out2[1]);
    ASSERT_EQ(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x64_128("", 0, 0, &out1);
    gcu_string_murmur3_x64_128("\0", 1, 0, &out2);
    ASSERT_NE(out1[0], out2[0]);
    ASSERT_NE(out1[1], out2[1]);
    ASSERT_EQ(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
  // Verify that different strings have different hashes.
  {
    uint32_t out1, out2;
    gcu_string_murmur3_32("foo", 3, 0, &out1);
    gcu_string_murmur3_32("bar", 3, 0, &out2);
    ASSERT_NE(out1, out2);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x86_128("foo", 3, 0, &out1);
    gcu_string_murmur3_x86_128("bar", 3, 0, &out2);
    ASSERT_NE(out1[0], out2[0]);
    ASSERT_NE(out1[1], out2[1]);
    ASSERT_NE(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x64_128("foo", 3, 0, &out1);
    gcu_string_murmur3_x64_128("bar", 3, 0, &out2);
    ASSERT_NE(out1[0], out2[0]);
    ASSERT_NE(out1[1], out2[1]);
    ASSERT_NE(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
}

TEST(Murmur3, Length) {
  // Verify that the length is respected (first two characters matching).
  {
    uint32_t out1, out2;
    gcu_string_murmur3_32("bar", 2, 0, &out1);
    gcu_string_murmur3_32("baz", 2, 0, &out2);
    ASSERT_EQ(out1, out2);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x86_128("bar", 2, 0, &out1);
    gcu_string_murmur3_x86_128("baz", 2, 0, &out2);
    ASSERT_EQ(out1[0], out2[0]);
    ASSERT_EQ(out1[1], out2[1]);
    ASSERT_NE(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x64_128("bar", 2, 0, &out1);
    gcu_string_murmur3_x64_128("baz", 2, 0, &out2);
    ASSERT_EQ(out1[0], out2[0]);
    ASSERT_EQ(out1[1], out2[1]);
    ASSERT_NE(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
  // Verify that the length is respected (first two characters matching).
  {
    uint32_t out1, out2;
    gcu_string_murmur3_32("bar", 3, 0, &out1);
    gcu_string_murmur3_32("baz", 3, 0, &out2);
    ASSERT_NE(out1, out2);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x86_128("bar", 3, 0, &out1);
    gcu_string_murmur3_x86_128("baz", 3, 0, &out2);
    ASSERT_NE(out1[0], out2[0]);
    ASSERT_NE(out1[1], out2[1]);
    ASSERT_NE(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x64_128("bar", 3, 0, &out1);
    gcu_string_murmur3_x64_128("baz", 3, 0, &out2);
    ASSERT_NE(out1[0], out2[0]);
    ASSERT_NE(out1[1], out2[1]);
    ASSERT_NE(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
}

TEST(Hash, HelperFunction32) {
  ASSERT_EQ(gcu_string_hash_32("", 0), gcu_string_hash_32("", 0));
  ASSERT_NE(gcu_string_hash_32("", 0), gcu_string_hash_32("\0", 1));
  ASSERT_EQ(gcu_string_hash_32("a", 1), gcu_string_hash_32("a", 1));
  ASSERT_NE(gcu_string_hash_32("a", 1), gcu_string_hash_32("b", 1));
  ASSERT_NE(gcu_string_hash_32("hello world!hello world!hello world!hello world!", 48), gcu_string_hash_32("Hello World!Hello World!Hello World!Hello World!", 48));
}

TEST(Hash, HelperFunction64) {
  ASSERT_EQ(gcu_string_hash_64("", 0), gcu_string_hash_64("", 0));
  ASSERT_NE(gcu_string_hash_64("", 0), gcu_string_hash_64("\0", 1));
  ASSERT_EQ(gcu_string_hash_64("a", 1), gcu_string_hash_64("a", 1));
  ASSERT_NE(gcu_string_hash_64("a", 1), gcu_string_hash_64("b", 1));
  ASSERT_NE(gcu_string_hash_64("hello world!hello world!hello world!hello world!", 48), gcu_string_hash_64("Hello World!Hello World!Hello World!Hello World!", 48));
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

