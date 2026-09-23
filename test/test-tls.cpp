#include <atomic>
#include <cstdlib>
#include <gtest/gtest.h>
#include <ghoti.io/cutil/thread.h>
#include <ghoti.io/cutil/tls.h>

using namespace std;

namespace {

GCU_TLS key;
atomic<int> destructor_calls{0};
atomic<int> mismatches{0};

void free_value(void * value) {
  ++destructor_calls;
  free(value);
}

// Each thread writes its own number, sleeps so the others are provably
// interleaved, then checks it still reads back its own.
GCU_THREAD_FUNC_RETURN_T GCU_THREAD_FUNC_CALLING_CONVENTION per_thread_value(GCU_THREAD_FUNC_ARG_T arg) {
  intptr_t mine = reinterpret_cast<intptr_t>(arg);
  int * slot = static_cast<int *>(malloc(sizeof(int)));
  *slot = static_cast<int>(mine);
  gcu_tls_set(key, slot);

  gcu_thread_sleep(20);

  int * read_back = static_cast<int *>(gcu_tls_get(key));
  if (!read_back || *read_back != static_cast<int>(mine)) {
    ++mismatches;
  }
  return (GCU_THREAD_FUNC_RETURN_T)0;
}

} // namespace

TEST(TLS, CreateAndDestroyReportZero) {
  GCU_TLS k;
  ASSERT_EQ(0, gcu_tls_create(&k, nullptr));
  ASSERT_EQ(0, gcu_tls_destroy(&k));
}

TEST(TLS, NullKeyPointerIsRejected) {
  ASSERT_EQ(-1, gcu_tls_create(nullptr, nullptr));
  ASSERT_EQ(-1, gcu_tls_destroy(nullptr));
}

TEST(TLS, AFreshKeyReadsAsNull) {
  GCU_TLS k;
  ASSERT_EQ(0, gcu_tls_create(&k, nullptr));
  ASSERT_EQ(nullptr, gcu_tls_get(k));
  ASSERT_EQ(0, gcu_tls_destroy(&k));
}

TEST(TLS, SetThenGetReturnsTheValue) {
  GCU_TLS k;
  ASSERT_EQ(0, gcu_tls_create(&k, nullptr));
  int marker = 42;
  ASSERT_EQ(0, gcu_tls_set(k, &marker));
  ASSERT_EQ(&marker, gcu_tls_get(k));
  ASSERT_EQ(0, gcu_tls_set(k, nullptr));
  ASSERT_EQ(nullptr, gcu_tls_get(k));
  ASSERT_EQ(0, gcu_tls_destroy(&k));
}

TEST(TLS, EachThreadSeesOnlyItsOwnValue) {
  // The whole point.  A key backed by one shared slot passes every test
  // above and fails this one.
  ASSERT_EQ(0, gcu_tls_create(&key, free_value));
  destructor_calls = 0;
  mismatches = 0;

  constexpr int kThreads = 12;
  GCU_Thread t[kThreads];
  for (int i = 0; i < kThreads; ++i) {
    ASSERT_EQ(0, gcu_thread_create(&t[i], per_thread_value,
        reinterpret_cast<void *>(static_cast<intptr_t>(i + 1))));
  }
  for (int i = 0; i < kThreads; ++i) {
    ASSERT_EQ(0, gcu_thread_join(t[i]));
  }

  ASSERT_EQ(0, mismatches.load())
      << mismatches.load() << " threads read another thread's value";

  // And the destructor ran once per thread that held a value -- which is
  // also what stops the twelve mallocs above from leaking under ASan.
  ASSERT_EQ(kThreads, destructor_calls.load());
  ASSERT_EQ(0, gcu_tls_destroy(&key));
}

TEST(TLS, TheMainThreadKeepsItsValueAcrossCalls) {
  GCU_TLS k;
  ASSERT_EQ(0, gcu_tls_create(&k, nullptr));
  int a = 1;
  ASSERT_EQ(0, gcu_tls_set(k, &a));

  // An unrelated thread comes and goes; this thread's slot is untouched.
  GCU_Thread t;
  ASSERT_EQ(0, gcu_thread_create(&t, [](GCU_THREAD_FUNC_ARG_T) -> GCU_THREAD_FUNC_RETURN_T {
    return (GCU_THREAD_FUNC_RETURN_T)0;
  }, nullptr));
  ASSERT_EQ(0, gcu_thread_join(t));

  ASSERT_EQ(&a, gcu_tls_get(k));
  ASSERT_EQ(0, gcu_tls_destroy(&k));
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
