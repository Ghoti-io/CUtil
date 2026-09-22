/**
 * @file
 * Tests for the pluggable allocator vtable and its dispatch helpers.
 */

#include <cstring>
#include <gtest/gtest.h>

#include <ghoti.io/cutil/allocator.h>

using namespace std;

TEST(Default, IsAStableSingleton) {
  const GCU_Allocator * a = gcu_allocator_default();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a, gcu_allocator_default());
  EXPECT_NE(a->malloc_fn, nullptr);
  EXPECT_NE(a->calloc_fn, nullptr);
  EXPECT_NE(a->realloc_fn, nullptr);
  EXPECT_NE(a->free_fn, nullptr);
}

TEST(Default, RoundTrips) {
  const GCU_Allocator * a = gcu_allocator_default();
  void * p = a->malloc_fn(a->ctx, 64);
  ASSERT_NE(p, nullptr);
  memset(p, 0xAB, 64);
  p = a->realloc_fn(a->ctx, p, 128);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(((unsigned char *)p)[0], 0xABu);
  a->free_fn(a->ctx, p);
}

TEST(Default, CallocZeroes) {
  const GCU_Allocator * a = gcu_allocator_default();
  unsigned char * p = (unsigned char *)a->calloc_fn(a->ctx, 16, 4);
  ASSERT_NE(p, nullptr);
  for (int i = 0; i < 64; i++) {
    EXPECT_EQ(p[i], 0u) << "byte " << i;
  }
  a->free_fn(a->ctx, p);
}

TEST(Default, ZeroSizeRequestsAreNotNull) {
  // A NULL return has to mean failure and nothing else; malloc(0) returning
  // NULL is indistinguishable from running out of memory at the call site.
  const GCU_Allocator * a = gcu_allocator_default();

  void * p = a->malloc_fn(a->ctx, 0);
  EXPECT_NE(p, nullptr);
  a->free_fn(a->ctx, p);

  void * z = a->calloc_fn(a->ctx, 0, 16);
  EXPECT_NE(z, nullptr);
  a->free_fn(a->ctx, z);

  z = a->calloc_fn(a->ctx, 16, 0);
  EXPECT_NE(z, nullptr);
  a->free_fn(a->ctx, z);

  z = a->calloc_fn(a->ctx, 0, 0);
  EXPECT_NE(z, nullptr);
  a->free_fn(a->ctx, z);

  // realloc() is the third way to reach the same trap, and the worst of the
  // three: glibc releases the block before returning the NULL, so a caller
  // reading it as failure holds a pointer that has already been freed.
  void * r = a->malloc_fn(a->ctx, 32);
  ASSERT_NE(r, nullptr);
  void * shrunk = a->realloc_fn(a->ctx, r, 0);
  EXPECT_NE(shrunk, nullptr);
  a->free_fn(a->ctx, shrunk);

  // And reallocating from NULL allocates, as it does for realloc() itself.
  void * fresh = a->realloc_fn(a->ctx, NULL, 32);
  EXPECT_NE(fresh, nullptr);
  a->free_fn(a->ctx, fresh);
}

TEST(Default, ZeroSizeCallocIsStillZeroed) {
  // Satisfying the non-NULL guarantee with a plain malloc would hand back
  // uninitialized memory from a function whose whole contract is zeroing.
  const GCU_Allocator * a = gcu_allocator_default();
  unsigned char * p = (unsigned char *)a->calloc_fn(a->ctx, 0, 0);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p[0], 0u);
  a->free_fn(a->ctx, p);
}

TEST(Default, CallocRejectsOverflow) {
  const GCU_Allocator * a = gcu_allocator_default();
  // The product wraps; the contract requires NULL rather than a short block.
  void * p = a->calloc_fn(a->ctx, SIZE_MAX / 2 + 1, 4);
  EXPECT_EQ(p, nullptr);
}

TEST(Helpers, NullAllocatorUsesTheDefault) {
  void * p = gcu_allocator_malloc(nullptr, 32);
  ASSERT_NE(p, nullptr);
  p = gcu_allocator_realloc(nullptr, p, 64);
  ASSERT_NE(p, nullptr);
  gcu_allocator_free(nullptr, p);

  void * z = gcu_allocator_calloc(nullptr, 8, 8);
  ASSERT_NE(z, nullptr);
  gcu_allocator_free(nullptr, z);
}

TEST(Helpers, FreeIgnoresNullPointer) {
  gcu_allocator_free(nullptr, nullptr);
  gcu_allocator_free(gcu_allocator_default(), nullptr);
}

namespace {

struct Recorder {
  int mallocs;
  int callocs;
  int reallocs;
  int frees;
  void * last_ctx;
};

void * rec_malloc(void * ctx, size_t size) {
  Recorder * r = (Recorder *)ctx;
  r->mallocs++;
  r->last_ctx = ctx;
  return malloc(size);
}
void * rec_calloc(void * ctx, size_t n, size_t size) {
  Recorder * r = (Recorder *)ctx;
  r->callocs++;
  r->last_ctx = ctx;
  return calloc(n, size);
}
void * rec_realloc(void * ctx, void * p, size_t size) {
  Recorder * r = (Recorder *)ctx;
  r->reallocs++;
  r->last_ctx = ctx;
  return realloc(p, size);
}
void rec_free(void * ctx, void * p) {
  Recorder * r = (Recorder *)ctx;
  r->frees++;
  r->last_ctx = ctx;
  free(p);
}

} // namespace

TEST(Helpers, CustomAllocatorReceivesItsContext) {
  Recorder rec = {0, 0, 0, 0, nullptr};
  GCU_Allocator a = {&rec, rec_malloc, rec_calloc, rec_realloc, rec_free};

  void * p = gcu_allocator_malloc(&a, 16);
  ASSERT_NE(p, nullptr);
  p = gcu_allocator_realloc(&a, p, 32);
  ASSERT_NE(p, nullptr);
  gcu_allocator_free(&a, p);

  void * z = gcu_allocator_calloc(&a, 4, 4);
  ASSERT_NE(z, nullptr);
  gcu_allocator_free(&a, z);

  EXPECT_EQ(rec.mallocs, 1);
  EXPECT_EQ(rec.callocs, 1);
  EXPECT_EQ(rec.reallocs, 1);
  EXPECT_EQ(rec.frees, 2);
  EXPECT_EQ(rec.last_ctx, &rec);
}

TEST(Helpers, FreeOfNullDoesNotReachTheAllocator) {
  Recorder rec = {0, 0, 0, 0, nullptr};
  GCU_Allocator a = {&rec, rec_malloc, rec_calloc, rec_realloc, rec_free};
  gcu_allocator_free(&a, nullptr);
  EXPECT_EQ(rec.frees, 0);
}

int main(int argc, char ** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
