/**
 * @file
 * Tests for the counting rules kept by the inline gcu_malloc(), gcu_calloc(),
 * gcu_realloc() and gcu_free() in memory.h.
 *
 * Deliberately does *not* define GHOTIIO_CUTIL_ENABLE_MEMORY_DEBUG.  Those
 * four names are two implementations of one contract -- the debug functions in
 * memory.c, tested by test-memory.cpp, and the inline versions here -- and the
 * inline ones are what the library itself and every consumer are built with,
 * so they are the side that actually decides whether a suite's leak assertions
 * mean anything.  Testing only the debug path would leave the shipped
 * behaviour unpinned.
 */

#include <gtest/gtest.h>

#include <ghoti.io/cutil/allocator.h>
#include <ghoti.io/cutil/array.h>
#include <ghoti.io/cutil/memory.h>

using namespace std;

TEST(MemoryInline, MallocAndFreeBalance) {
  size_t alloc = gcu_get_alloc_count();
  size_t freed = gcu_get_free_count();

  void * buffer = gcu_malloc(128);
  ASSERT_NE(buffer, nullptr);
  ASSERT_EQ(gcu_get_alloc_count(), alloc + 1);
  ASSERT_EQ(gcu_get_free_count(), freed);

  gcu_free(buffer);
  ASSERT_EQ(gcu_get_alloc_count(), alloc + 1);
  ASSERT_EQ(gcu_get_free_count(), freed + 1);
}

TEST(MemoryInline, ReallocFromNullIsAnAllocation) {
  size_t alloc = gcu_get_alloc_count();
  size_t freed = gcu_get_free_count();

  void * buffer = gcu_realloc(NULL, 32);
  ASSERT_NE(buffer, nullptr);
  ASSERT_EQ(gcu_get_alloc_count(), alloc + 1);
  ASSERT_EQ(gcu_get_free_count(), freed);

  gcu_free(buffer);
  ASSERT_EQ(gcu_get_alloc_count(), alloc + 1);
  ASSERT_EQ(gcu_get_free_count(), freed + 1);
}

TEST(MemoryInline, GrowingAnExistingBlockCountsNothing) {
  size_t alloc = gcu_get_alloc_count();
  size_t freed = gcu_get_free_count();

  void * buffer = gcu_malloc(16);
  ASSERT_NE(buffer, nullptr);
  buffer = gcu_realloc(buffer, 4096);
  ASSERT_NE(buffer, nullptr);
  buffer = gcu_realloc(buffer, 24);
  ASSERT_NE(buffer, nullptr);

  // No block began or ended in either call, however far the address moved.
  ASSERT_EQ(gcu_get_alloc_count(), alloc + 1);
  ASSERT_EQ(gcu_get_free_count(), freed);

  gcu_free(buffer);
  ASSERT_EQ(gcu_get_free_count(), freed + 1);
}

TEST(MemoryInline, ReallocToZeroKeepsTheBlock) {
  size_t alloc = gcu_get_alloc_count();
  size_t freed = gcu_get_free_count();

  void * buffer = gcu_malloc(64);
  ASSERT_NE(buffer, nullptr);
  void * shrunk = gcu_realloc(buffer, 0);

  // glibc's realloc() would have released the block and returned NULL here,
  // which a caller cannot tell apart from failure and which would have left
  // the release uncounted.
  ASSERT_NE(shrunk, nullptr);
  ASSERT_EQ(gcu_get_alloc_count(), alloc + 1);
  ASSERT_EQ(gcu_get_free_count(), freed);

  gcu_free(shrunk);
  ASSERT_EQ(gcu_get_free_count(), freed + 1);
}

TEST(MemoryInline, FreeOfNullIsNotCounted) {
  size_t alloc = gcu_get_alloc_count();
  size_t freed = gcu_get_free_count();

  gcu_free(NULL);

  ASSERT_EQ(gcu_get_alloc_count(), alloc);
  ASSERT_EQ(gcu_get_free_count(), freed);
}

/// An allocator wired straight to the counted functions, which is what a
/// library adopting cutil's containers has to supply: gcu_allocator_default()
/// hands out stdlib blocks and counts nothing.
static void * counted_malloc(void * ctx, size_t size) {
  (void)ctx;
  return gcu_malloc(size ? size : 1);
}

static void * counted_calloc(void * ctx, size_t nitems, size_t size) {
  (void)ctx;
  return gcu_calloc(nitems ? nitems : 1, size ? size : 1);
}

static void * counted_realloc(void * ctx, void * ptr, size_t size) {
  (void)ctx;
  return gcu_realloc(ptr, size);
}

static void counted_free(void * ctx, void * ptr) {
  (void)ctx;
  gcu_free(ptr);
}

static const GCU_Allocator counted = {
  .ctx = NULL,
  .malloc_fn = counted_malloc,
  .calloc_fn = counted_calloc,
  .realloc_fn = counted_realloc,
  .free_fn = counted_free,
};

// The three tests below are the reason the rules above are worth anything.
// A GCU_Array takes its storage only through gcu_allocator_realloc() against
// a data pointer that starts NULL, so before the rules were settled its buffer
// was released having never been counted -- and a correct, fully cleaned-up
// array raised the free count above the allocation count.  That is not merely
// a false alarm: a net that runs negative cancels a real leak one for one, so
// one leaked block plus one correct array balanced, and the assertion that
// should have caught the leak passed.

TEST(MemoryInline, ArrayCreatedAndDestroyedBalances) {
  size_t alloc = gcu_get_alloc_count();
  size_t freed = gcu_get_free_count();

  GCU_Array * array = gcu_array_create(sizeof(int), 4, &counted);
  ASSERT_NE(array, nullptr);
  for (int i = 0; i < 64; ++i) {
    ASSERT_TRUE(gcu_array_append(array, &i));
  }
  gcu_array_destroy(array);

  ASSERT_EQ(gcu_get_alloc_count() - alloc, gcu_get_free_count() - freed);
}

TEST(MemoryInline, ArrayStolenThenDestroyedBalances) {
  size_t alloc = gcu_get_alloc_count();
  size_t freed = gcu_get_free_count();

  GCU_Array * array = gcu_array_create(sizeof(int), 4, &counted);
  ASSERT_NE(array, nullptr);
  int value = 7;
  ASSERT_TRUE(gcu_array_append(array, &value));

  size_t count = 0;
  void * stolen = gcu_array_steal(array, &count);
  ASSERT_EQ(count, (size_t)1);
  // Destroying the husk releases no buffer, so it must count no free either.
  gcu_array_destroy(array);
  gcu_allocator_free(&counted, stolen);

  ASSERT_EQ(gcu_get_alloc_count() - alloc, gcu_get_free_count() - freed);
}

TEST(MemoryInline, ALeakStillShowsAlongsideAnArray) {
  size_t alloc = gcu_get_alloc_count();
  size_t freed = gcu_get_free_count();

  // The case the old rules could not see: a genuinely leaked block used to be
  // cancelled by the phantom free an adjacent, correct array contributed.
  void * leaked = gcu_malloc(32);
  ASSERT_NE(leaked, nullptr);

  GCU_Array array;
  ASSERT_TRUE(gcu_array_create_in_place(&array, sizeof(int), 4, &counted));
  int value = 1;
  ASSERT_TRUE(gcu_array_append(&array, &value));
  gcu_array_destroy_in_place(&array);

  ASSERT_EQ(gcu_get_alloc_count() - alloc, (gcu_get_free_count() - freed) + 1);

  gcu_free(leaked);
}

int main(int argc, char ** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
