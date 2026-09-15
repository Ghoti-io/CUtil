/**
 * @file
 * Tests for the generic fixed-size-element array.
 */

#include <cstring>
#include <gtest/gtest.h>

#include <cutil/array.h>

using namespace std;

/// A struct large enough that storing it by value actually matters; this is
/// the case GCU_VectorN cannot serve.
struct Vertex {
  float x, y, z;
  float u, v;
  unsigned int tag;
};

static bool operator==(const Vertex & a, const Vertex & b) {
  return memcmp(&a, &b, sizeof(Vertex)) == 0;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

TEST(Create, Heap) {
  GCU_Array * a = gcu_array_create(sizeof(Vertex), 0, nullptr);
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->element_size, sizeof(Vertex));
  EXPECT_EQ(a->count, 0u);
  EXPECT_EQ(a->capacity, 0u);
  EXPECT_EQ(a->data, nullptr);
  gcu_array_destroy(a);
}

TEST(Create, HeapWithReservation) {
  GCU_Array * a = gcu_array_create(sizeof(Vertex), 16, nullptr);
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->capacity, 16u);
  EXPECT_EQ(a->count, 0u);
  EXPECT_NE(a->data, nullptr);
  gcu_array_destroy(a);
}

TEST(Create, InPlace) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 4, nullptr));
  EXPECT_EQ(a.capacity, 4u);
  gcu_array_destroy_in_place(&a);
  EXPECT_EQ(a.data, nullptr);
  EXPECT_EQ(a.capacity, 0u);
}

TEST(Create, ZeroElementSizeRejected) {
  EXPECT_EQ(gcu_array_create(0, 4, nullptr), nullptr);

  GCU_Array a;
  EXPECT_FALSE(gcu_array_create_in_place(&a, 0, 4, nullptr));
  // Must still be safe to tear down after a failed creation.
  gcu_array_destroy_in_place(&a);
}

TEST(Destroy, NullIsIgnored) {
  gcu_array_destroy(nullptr);
  gcu_array_destroy_in_place(nullptr);
}

TEST(Destroy, InPlaceIsIdempotent) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 4, nullptr));
  gcu_array_destroy_in_place(&a);
  gcu_array_destroy_in_place(&a);
}

TEST(Destroy, CleanupRuns) {
  static int calls;
  static size_t count_seen;
  calls = 0;
  count_seen = 0;

  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 0, nullptr));
  a.cleanup = [](GCU_Array * arr) {
    calls++;
    count_seen = arr->count;
  };
  int v = 7;
  ASSERT_TRUE(gcu_array_append(&a, &v));
  ASSERT_TRUE(gcu_array_append(&a, &v));

  gcu_array_destroy_in_place(&a);
  EXPECT_EQ(calls, 1);
  // The elements are still readable while cleanup runs.
  EXPECT_EQ(count_seen, 2u);

  // A second teardown must not run it again.
  gcu_array_destroy_in_place(&a);
  EXPECT_EQ(calls, 1);
}

// ---------------------------------------------------------------------------
// Append / emplace
// ---------------------------------------------------------------------------

TEST(Append, StoresStructsByValue) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(Vertex), 0, nullptr));

  Vertex v = {1.f, 2.f, 3.f, 0.5f, 0.25f, 99u};
  ASSERT_TRUE(gcu_array_append(&a, &v));

  // Mutating the source must not affect the stored copy.
  v.x = 1000.f;

  Vertex * stored = (Vertex *)gcu_array_at(&a, 0);
  ASSERT_NE(stored, nullptr);
  EXPECT_FLOAT_EQ(stored->x, 1.f);
  EXPECT_EQ(stored->tag, 99u);

  gcu_array_destroy_in_place(&a);
}

TEST(Append, GrowsAndPreservesOrder) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 0, nullptr));

  for (int i = 0; i < 1000; i++) {
    ASSERT_TRUE(gcu_array_append(&a, &i)) << "append " << i;
  }
  EXPECT_EQ(gcu_array_count(&a), 1000u);
  EXPECT_GE(a.capacity, 1000u);

  for (int i = 0; i < 1000; i++) {
    EXPECT_EQ(*(int *)gcu_array_at(&a, (size_t)i), i);
  }
  gcu_array_destroy_in_place(&a);
}

TEST(Append, NullElementRejected) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 0, nullptr));
  EXPECT_FALSE(gcu_array_append(&a, nullptr));
  EXPECT_EQ(gcu_array_count(&a), 0u);
  gcu_array_destroy_in_place(&a);
}

TEST(AppendN, CopiesABlock) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 0, nullptr));

  int src[5] = {10, 20, 30, 40, 50};
  ASSERT_TRUE(gcu_array_append_n(&a, src, 5));
  ASSERT_TRUE(gcu_array_append_n(&a, src, 5));
  EXPECT_EQ(gcu_array_count(&a), 10u);
  EXPECT_EQ(*(int *)gcu_array_at(&a, 0), 10);
  EXPECT_EQ(*(int *)gcu_array_at(&a, 9), 50);

  gcu_array_destroy_in_place(&a);
}

TEST(AppendN, ZeroCountSucceedsWithoutSource) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 0, nullptr));
  EXPECT_TRUE(gcu_array_append_n(&a, nullptr, 0));
  EXPECT_EQ(gcu_array_count(&a), 0u);
  gcu_array_destroy_in_place(&a);
}

TEST(AppendN, NullSourceWithCountRejected) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 0, nullptr));
  EXPECT_FALSE(gcu_array_append_n(&a, nullptr, 3));
  EXPECT_EQ(gcu_array_count(&a), 0u);
  gcu_array_destroy_in_place(&a);
}

TEST(Emplace, ReturnsZeroedSlot) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(Vertex), 0, nullptr));

  Vertex * v = (Vertex *)gcu_array_emplace(&a);
  ASSERT_NE(v, nullptr);
  Vertex zero;
  memset(&zero, 0, sizeof(zero));
  EXPECT_TRUE(*v == zero);

  v->tag = 42u;
  EXPECT_EQ(((Vertex *)gcu_array_at(&a, 0))->tag, 42u);
  EXPECT_EQ(gcu_array_count(&a), 1u);

  gcu_array_destroy_in_place(&a);
}

TEST(Emplace, ReusedSlotIsRezeroed) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 4, nullptr));

  int * first = (int *)gcu_array_emplace(&a);
  *first = 12345;
  gcu_array_clear(&a);

  int * again = (int *)gcu_array_emplace(&a);
  ASSERT_EQ(again, first) << "capacity should have been reused";
  EXPECT_EQ(*again, 0) << "emplace must zero the slot even when reusing memory";

  gcu_array_destroy_in_place(&a);
}

TEST(EmplaceN, ZeroesTheWholeRun) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 0, nullptr));

  int * block = (int *)gcu_array_emplace_n(&a, 32);
  ASSERT_NE(block, nullptr);
  EXPECT_EQ(gcu_array_count(&a), 32u);
  for (int i = 0; i < 32; i++) {
    EXPECT_EQ(block[i], 0) << "index " << i;
  }
  gcu_array_destroy_in_place(&a);
}

// ---------------------------------------------------------------------------
// Access
// ---------------------------------------------------------------------------

TEST(At, OutOfRangeReturnsNull) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 8, nullptr));
  int v = 1;
  ASSERT_TRUE(gcu_array_append(&a, &v));

  EXPECT_NE(gcu_array_at(&a, 0), nullptr);
  // Index 1 is within capacity but past the count: still out of range.
  EXPECT_EQ(gcu_array_at(&a, 1), nullptr);
  EXPECT_EQ(gcu_array_at(&a, 1000), nullptr);
  EXPECT_EQ(gcu_array_at(nullptr, 0), nullptr);

  gcu_array_destroy_in_place(&a);
}

TEST(Back, EmptyReturnsNull) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 8, nullptr));
  EXPECT_EQ(gcu_array_back(&a), nullptr);

  int v = 3;
  ASSERT_TRUE(gcu_array_append(&a, &v));
  v = 4;
  ASSERT_TRUE(gcu_array_append(&a, &v));
  EXPECT_EQ(*(int *)gcu_array_back(&a), 4);

  gcu_array_destroy_in_place(&a);
}

TEST(Count, NullIsEmpty) {
  EXPECT_EQ(gcu_array_count(nullptr), 0u);
}

// ---------------------------------------------------------------------------
// Removal
// ---------------------------------------------------------------------------

TEST(Pop, ActsAsAStack) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 0, nullptr));
  for (int i = 0; i < 5; i++) {
    ASSERT_TRUE(gcu_array_append(&a, &i));
  }

  int out = -1;
  for (int i = 4; i >= 0; i--) {
    ASSERT_TRUE(gcu_array_pop(&a, &out));
    EXPECT_EQ(out, i);
  }
  EXPECT_FALSE(gcu_array_pop(&a, &out));
  EXPECT_EQ(gcu_array_count(&a), 0u);

  gcu_array_destroy_in_place(&a);
}

TEST(Pop, DiscardingOutputIsAllowed) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 0, nullptr));
  int v = 9;
  ASSERT_TRUE(gcu_array_append(&a, &v));
  EXPECT_TRUE(gcu_array_pop(&a, nullptr));
  EXPECT_EQ(gcu_array_count(&a), 0u);
  gcu_array_destroy_in_place(&a);
}

TEST(RemoveAt, PreservesOrder) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 0, nullptr));
  for (int i = 0; i < 5; i++) {
    ASSERT_TRUE(gcu_array_append(&a, &i)); // 0 1 2 3 4
  }

  int out = -1;
  ASSERT_TRUE(gcu_array_remove_at(&a, 1, &out));
  EXPECT_EQ(out, 1);
  ASSERT_EQ(gcu_array_count(&a), 4u);

  int expected[] = {0, 2, 3, 4};
  for (size_t i = 0; i < 4; i++) {
    EXPECT_EQ(*(int *)gcu_array_at(&a, i), expected[i]) << "index " << i;
  }

  // Removing the final element takes the memmove-free path.
  ASSERT_TRUE(gcu_array_remove_at(&a, 3, &out));
  EXPECT_EQ(out, 4);
  EXPECT_EQ(gcu_array_count(&a), 3u);

  EXPECT_FALSE(gcu_array_remove_at(&a, 3, &out));

  gcu_array_destroy_in_place(&a);
}

TEST(SwapRemove, MovesTheLastElementIn) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 0, nullptr));
  for (int i = 0; i < 5; i++) {
    ASSERT_TRUE(gcu_array_append(&a, &i)); // 0 1 2 3 4
  }

  int out = -1;
  ASSERT_TRUE(gcu_array_swap_remove(&a, 1, &out));
  EXPECT_EQ(out, 1);
  ASSERT_EQ(gcu_array_count(&a), 4u);

  int expected[] = {0, 4, 2, 3};
  for (size_t i = 0; i < 4; i++) {
    EXPECT_EQ(*(int *)gcu_array_at(&a, i), expected[i]) << "index " << i;
  }

  gcu_array_destroy_in_place(&a);
}

TEST(SwapRemove, RemovingTheLastElementIsSelfSafe) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 0, nullptr));
  int v = 77;
  ASSERT_TRUE(gcu_array_append(&a, &v));

  int out = -1;
  ASSERT_TRUE(gcu_array_swap_remove(&a, 0, &out));
  EXPECT_EQ(out, 77);
  EXPECT_EQ(gcu_array_count(&a), 0u);

  gcu_array_destroy_in_place(&a);
}

// ---------------------------------------------------------------------------
// Capacity
// ---------------------------------------------------------------------------

TEST(Reserve, NeverShrinks) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 100, nullptr));
  ASSERT_TRUE(gcu_array_reserve(&a, 10));
  EXPECT_EQ(a.capacity, 100u);
  gcu_array_destroy_in_place(&a);
}

TEST(Reserve, AvoidsReallocation) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 0, nullptr));
  ASSERT_TRUE(gcu_array_reserve(&a, 256));
  void * base = a.data;

  for (int i = 0; i < 256; i++) {
    ASSERT_TRUE(gcu_array_append(&a, &i));
  }
  EXPECT_EQ(a.data, base) << "reserved capacity should not have been exceeded";

  gcu_array_destroy_in_place(&a);
}

TEST(Reserve, OverflowIsRejected) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(Vertex), 0, nullptr));
  // count * element_size cannot be represented.
  EXPECT_FALSE(gcu_array_reserve(&a, SIZE_MAX / 2));
  EXPECT_EQ(a.capacity, 0u);
  gcu_array_destroy_in_place(&a);
}

TEST(Resize, GrowsWithZeroes) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 0, nullptr));
  int v = 5;
  ASSERT_TRUE(gcu_array_append(&a, &v));

  ASSERT_TRUE(gcu_array_resize(&a, 10));
  EXPECT_EQ(gcu_array_count(&a), 10u);
  EXPECT_EQ(*(int *)gcu_array_at(&a, 0), 5) << "existing elements preserved";
  for (size_t i = 1; i < 10; i++) {
    EXPECT_EQ(*(int *)gcu_array_at(&a, i), 0) << "index " << i;
  }

  gcu_array_destroy_in_place(&a);
}

TEST(Resize, ShrinkKeepsCapacity) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 0, nullptr));
  ASSERT_TRUE(gcu_array_resize(&a, 64));
  size_t capacity = a.capacity;

  ASSERT_TRUE(gcu_array_resize(&a, 2));
  EXPECT_EQ(gcu_array_count(&a), 2u);
  EXPECT_EQ(a.capacity, capacity);

  gcu_array_destroy_in_place(&a);
}

TEST(ShrinkToFit, ReleasesSpareCapacity) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 1000, nullptr));
  int v = 1;
  ASSERT_TRUE(gcu_array_append(&a, &v));

  ASSERT_TRUE(gcu_array_shrink_to_fit(&a));
  EXPECT_EQ(a.capacity, 1u);
  EXPECT_EQ(*(int *)gcu_array_at(&a, 0), 1);

  gcu_array_destroy_in_place(&a);
}

TEST(ShrinkToFit, EmptyArrayReleasesEverything) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 1000, nullptr));
  ASSERT_TRUE(gcu_array_shrink_to_fit(&a));
  EXPECT_EQ(a.capacity, 0u);
  EXPECT_EQ(a.data, nullptr);
  // Still usable afterwards.
  int v = 3;
  EXPECT_TRUE(gcu_array_append(&a, &v));
  gcu_array_destroy_in_place(&a);
}

TEST(Clear, KeepsCapacity) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 0, nullptr));
  for (int i = 0; i < 20; i++) {
    ASSERT_TRUE(gcu_array_append(&a, &i));
  }
  size_t capacity = a.capacity;

  gcu_array_clear(&a);
  EXPECT_EQ(gcu_array_count(&a), 0u);
  EXPECT_EQ(a.capacity, capacity);

  gcu_array_clear(nullptr);

  gcu_array_destroy_in_place(&a);
}

// ---------------------------------------------------------------------------
// Steal
// ---------------------------------------------------------------------------

TEST(Steal, TransfersOwnership) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 0, nullptr));
  for (int i = 0; i < 4; i++) {
    ASSERT_TRUE(gcu_array_append(&a, &i));
  }

  size_t count = 0;
  int * data = (int *)gcu_array_steal(&a, &count);
  ASSERT_NE(data, nullptr);
  EXPECT_EQ(count, 4u);
  EXPECT_EQ(data[3], 3);

  EXPECT_EQ(a.data, nullptr);
  EXPECT_EQ(a.count, 0u);
  EXPECT_EQ(a.capacity, 0u);

  gcu_allocator_free(nullptr, data);

  // The array remains usable after a steal.
  int v = 8;
  EXPECT_TRUE(gcu_array_append(&a, &v));
  EXPECT_EQ(gcu_array_count(&a), 1u);
  gcu_array_destroy_in_place(&a);
}

TEST(Steal, EmptyArrayYieldsNull) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 0, nullptr));
  size_t count = 99;
  EXPECT_EQ(gcu_array_steal(&a, &count), nullptr);
  EXPECT_EQ(count, 0u);
  gcu_array_destroy_in_place(&a);
}

// ---------------------------------------------------------------------------
// Allocator integration
// ---------------------------------------------------------------------------

namespace {

struct CountingCtx {
  size_t live;
  size_t allocations;
  size_t fail_after; // 0 = never fail
};

void * counting_malloc(void * ctx, size_t size) {
  CountingCtx * c = (CountingCtx *)ctx;
  if (c->fail_after && c->allocations >= c->fail_after) {
    return nullptr;
  }
  c->allocations++;
  c->live++;
  return malloc(size);
}

void * counting_calloc(void * ctx, size_t nitems, size_t size) {
  CountingCtx * c = (CountingCtx *)ctx;
  if (c->fail_after && c->allocations >= c->fail_after) {
    return nullptr;
  }
  c->allocations++;
  c->live++;
  return calloc(nitems, size);
}

void * counting_realloc(void * ctx, void * ptr, size_t size) {
  CountingCtx * c = (CountingCtx *)ctx;
  if (c->fail_after && c->allocations >= c->fail_after) {
    return nullptr;
  }
  c->allocations++;
  if (!ptr) {
    c->live++;
  }
  return realloc(ptr, size);
}

void counting_free(void * ctx, void * ptr) {
  CountingCtx * c = (CountingCtx *)ctx;
  if (ptr) {
    c->live--;
  }
  free(ptr);
}

GCU_Allocator make_counting(CountingCtx * ctx) {
  GCU_Allocator a;
  a.ctx = ctx;
  a.malloc_fn = counting_malloc;
  a.calloc_fn = counting_calloc;
  a.realloc_fn = counting_realloc;
  a.free_fn = counting_free;
  return a;
}

} // namespace

TEST(Allocator, AllStorageComesFromTheSuppliedAllocator) {
  CountingCtx ctx = {0, 0, 0};
  GCU_Allocator alloc = make_counting(&ctx);

  GCU_Array * a = gcu_array_create(sizeof(Vertex), 0, &alloc);
  ASSERT_NE(a, nullptr);
  for (int i = 0; i < 500; i++) {
    Vertex v = {(float)i, 0, 0, 0, 0, (unsigned)i};
    ASSERT_TRUE(gcu_array_append(a, &v));
  }
  EXPECT_GT(ctx.allocations, 1u);
  gcu_array_destroy(a);

  // Both the struct and the storage must have come back.
  EXPECT_EQ(ctx.live, 0u);
}

TEST(Allocator, CreateFailureFreesTheStruct) {
  // Allow exactly one allocation: the GCU_Array itself. The reservation then
  // fails, and the struct must be returned rather than leaked.
  CountingCtx ctx = {0, 0, 1};
  GCU_Allocator alloc = make_counting(&ctx);

  EXPECT_EQ(gcu_array_create(sizeof(Vertex), 16, &alloc), nullptr);
  EXPECT_EQ(ctx.live, 0u);
}

TEST(Allocator, AppendFailureLeavesTheArrayIntact) {
  CountingCtx ctx = {0, 0, 0};
  GCU_Allocator alloc = make_counting(&ctx);

  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 4, &alloc));
  for (int i = 0; i < 4; i++) {
    ASSERT_TRUE(gcu_array_append(&a, &i));
  }

  // Refuse every further allocation; the fifth append must fail cleanly.
  ctx.fail_after = ctx.allocations;
  int v = 4;
  EXPECT_FALSE(gcu_array_append(&a, &v));
  EXPECT_EQ(gcu_array_count(&a), 4u) << "count must not advance on failure";
  for (int i = 0; i < 4; i++) {
    EXPECT_EQ(*(int *)gcu_array_at(&a, (size_t)i), i) << "data preserved";
  }

  ctx.fail_after = 0;
  gcu_array_destroy_in_place(&a);
  EXPECT_EQ(ctx.live, 0u);
}

TEST(Allocator, NullMeansDefault) {
  GCU_Array a;
  ASSERT_TRUE(gcu_array_create_in_place(&a, sizeof(int), 4, nullptr));
  EXPECT_EQ(a.allocator, nullptr);
  int v = 1;
  EXPECT_TRUE(gcu_array_append(&a, &v));
  gcu_array_destroy_in_place(&a);
}

int main(int argc, char ** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
