#include <climits>
#include <cstdint>
#include <gtest/gtest.h>
#include <ghoti.io/cutil/atomic.h>
#include <ghoti.io/cutil/thread.h>

using namespace std;

namespace {

constexpr int kThreads = 8;
constexpr int kPerThread = 20000;

GCU_Atomic_Int shared_int;
GCU_Atomic_Size shared_size;
GCU_Atomic_Flag shared_flag;
GCU_Atomic_Int winners;

GCU_THREAD_FUNC_RETURN_T GCU_THREAD_FUNC_CALLING_CONVENTION hammer_add(GCU_THREAD_FUNC_ARG_T) {
  for (int i = 0; i < kPerThread; ++i) {
    gcu_atomic_int_fetch_add(&shared_int, 1);
  }
  return (GCU_THREAD_FUNC_RETURN_T)0;
}

GCU_THREAD_FUNC_RETURN_T GCU_THREAD_FUNC_CALLING_CONVENTION hammer_add_sub(GCU_THREAD_FUNC_ARG_T) {
  for (int i = 0; i < kPerThread; ++i) {
    gcu_atomic_size_fetch_add(&shared_size, 3);
    gcu_atomic_size_fetch_sub(&shared_size, 2);
  }
  return (GCU_THREAD_FUNC_RETURN_T)0;
}

GCU_THREAD_FUNC_RETURN_T GCU_THREAD_FUNC_CALLING_CONVENTION race_for_the_flag(GCU_THREAD_FUNC_ARG_T) {
  if (!gcu_atomic_flag_test_and_set(&shared_flag)) {
    gcu_atomic_int_fetch_add(&winners, 1);
  }
  return (GCU_THREAD_FUNC_RETURN_T)0;
}

// A counter incremented with load-then-store instead of fetch_add: the
// operation the atomic exists to replace. Used only to show the test can
// detect a lost update at all.
GCU_Atomic_Int unsafe_counter;
GCU_THREAD_FUNC_RETURN_T GCU_THREAD_FUNC_CALLING_CONVENTION hammer_unsafely(GCU_THREAD_FUNC_ARG_T) {
  for (int i = 0; i < kPerThread; ++i) {
    int32_t v = gcu_atomic_int_load(&unsafe_counter);
    gcu_atomic_int_store(&unsafe_counter, v + 1);
  }
  return (GCU_THREAD_FUNC_RETURN_T)0;
}

} // namespace

TEST(Atomic, IntSingleThreadedOperations) {
  GCU_Atomic_Int a;
  gcu_atomic_int_init(&a, 5);
  ASSERT_EQ(5, gcu_atomic_int_load(&a));

  gcu_atomic_int_store(&a, -7);
  ASSERT_EQ(-7, gcu_atomic_int_load(&a));

  ASSERT_EQ(-7, gcu_atomic_int_exchange(&a, 11));
  ASSERT_EQ(11, gcu_atomic_int_load(&a));

  ASSERT_EQ(11, gcu_atomic_int_fetch_add(&a, 4));
  ASSERT_EQ(15, gcu_atomic_int_load(&a));

  ASSERT_EQ(15, gcu_atomic_int_fetch_sub(&a, 20));
  ASSERT_EQ(-5, gcu_atomic_int_load(&a));
}

TEST(Atomic, IntCompareExchangeReportsWhatItFound) {
  GCU_Atomic_Int a;
  gcu_atomic_int_init(&a, 10);

  int32_t expected = 99;
  ASSERT_FALSE(gcu_atomic_int_compare_exchange(&a, &expected, 42));
  // The documented behaviour a retry loop depends on: on failure, `expected`
  // is overwritten with the value actually found, so the caller need not
  // re-read.
  ASSERT_EQ(10, expected);
  ASSERT_EQ(10, gcu_atomic_int_load(&a));

  ASSERT_TRUE(gcu_atomic_int_compare_exchange(&a, &expected, 42));
  ASSERT_EQ(10, expected) << "expected was modified on success";
  ASSERT_EQ(42, gcu_atomic_int_load(&a));
}

TEST(Atomic, SubtractingTheMostNegativeValueDoesNotOverflow) {
  // The Windows branch negates the subtrahend to reuse ExchangeAdd, and
  // -INT32_MIN is undefined in the signed domain. Pinned on both platforms so
  // the branch that cannot run here still has its contract stated.
  GCU_Atomic_Int a;
  gcu_atomic_int_init(&a, 0);
  ASSERT_EQ(0, gcu_atomic_int_fetch_sub(&a, INT32_MIN));
  ASSERT_EQ(INT32_MIN, gcu_atomic_int_load(&a));
}

TEST(Atomic, SizeSingleThreadedOperations) {
  GCU_Atomic_Size a;
  gcu_atomic_size_init(&a, 0);
  ASSERT_EQ(0u, gcu_atomic_size_load(&a));

  ASSERT_EQ(0u, gcu_atomic_size_fetch_add(&a, 100));
  ASSERT_EQ(100u, gcu_atomic_size_load(&a));
  ASSERT_EQ(100u, gcu_atomic_size_fetch_sub(&a, 40));
  ASSERT_EQ(60u, gcu_atomic_size_load(&a));
  ASSERT_EQ(60u, gcu_atomic_size_exchange(&a, 7));
  ASSERT_EQ(7u, gcu_atomic_size_load(&a));

  size_t expected = 7;
  ASSERT_TRUE(gcu_atomic_size_compare_exchange(&a, &expected, SIZE_MAX));
  ASSERT_EQ(SIZE_MAX, gcu_atomic_size_load(&a));
  // Wrapping is defined for an unsigned type and is the documented behaviour.
  ASSERT_EQ(SIZE_MAX, gcu_atomic_size_fetch_add(&a, 1));
  ASSERT_EQ(0u, gcu_atomic_size_load(&a));
}

TEST(Atomic, PointerOperations) {
  int one = 1;
  int two = 2;
  GCU_Atomic_Ptr p;
  gcu_atomic_ptr_init(&p, nullptr);
  ASSERT_EQ(nullptr, gcu_atomic_ptr_load(&p));

  gcu_atomic_ptr_store(&p, &one);
  ASSERT_EQ(&one, gcu_atomic_ptr_load(&p));
  ASSERT_EQ(&one, gcu_atomic_ptr_exchange(&p, &two));
  ASSERT_EQ(&two, gcu_atomic_ptr_load(&p));

  void * expected = &one;
  ASSERT_FALSE(gcu_atomic_ptr_compare_exchange(&p, &expected, nullptr));
  ASSERT_EQ(&two, expected);
  ASSERT_TRUE(gcu_atomic_ptr_compare_exchange(&p, &expected, nullptr));
  ASSERT_EQ(nullptr, gcu_atomic_ptr_load(&p));
}

TEST(Atomic, FlagOperations) {
  GCU_Atomic_Flag f;
  gcu_atomic_flag_init(&f);
  ASSERT_FALSE(gcu_atomic_flag_load(&f));

  ASSERT_FALSE(gcu_atomic_flag_test_and_set(&f)) << "first set reported as a repeat";
  ASSERT_TRUE(gcu_atomic_flag_load(&f));
  ASSERT_TRUE(gcu_atomic_flag_test_and_set(&f)) << "second set reported as first";

  gcu_atomic_flag_clear(&f);
  ASSERT_FALSE(gcu_atomic_flag_load(&f));
  ASSERT_FALSE(gcu_atomic_flag_test_and_set(&f));
}

TEST(Atomic, LoadingThroughAConstPointerCompilesAndReads) {
  // The signatures take const, and the builtins underneath do not, so the
  // const is cast away in one place. If that ever stops compiling, it stops
  // here rather than in a consumer.
  GCU_Atomic_Int a;
  gcu_atomic_int_init(&a, 3);
  const GCU_Atomic_Int * ca = &a;
  ASSERT_EQ(3, gcu_atomic_int_load(ca));
}

TEST(Atomic, FetchAddLosesNothingUnderContention) {
  // The point of the module. 160,000 increments across 8 threads must all
  // land.
  gcu_atomic_int_init(&shared_int, 0);
  GCU_Thread t[kThreads];
  for (int i = 0; i < kThreads; ++i) {
    ASSERT_EQ(0, gcu_thread_create(&t[i], hammer_add, nullptr));
  }
  for (int i = 0; i < kThreads; ++i) {
    ASSERT_EQ(0, gcu_thread_join(t[i]));
  }
  ASSERT_EQ(kThreads * kPerThread, gcu_atomic_int_load(&shared_int));
}

TEST(Atomic, TheContentionTestCanDetectALostUpdate) {
  // Without this, FetchAddLosesNothingUnderContention proves nothing: if the
  // threads never actually overlapped, a non-atomic increment would pass it
  // too. Load-then-store over the same shape must lose updates; if it does
  // not, the contention above is not real and the test above is not a test.
  //
  // Attempted more than once, because losing an update needs two threads
  // inside the same load-store window at the same instant, which needs them
  // to be running at the same instant. With every core busy -- which is what
  // running the whole suite looks like -- they time-slice instead, and a
  // slice boundary falls in that window only occasionally. Measured: 0
  // failures in 60 runs on an idle machine, 2 in 30 with all 12 cores
  // saturated. A single attempt is a bet on the scheduler, and losing it
  // reads as a defect in the atomics.
  //
  // The window is deliberately NOT widened to make a loss more likely. This
  // is a control for the test above, so it has to contend the same way that
  // test does; a control with an easier shape would pass while telling you
  // nothing about the shape you care about.
  constexpr int kAttempts = 8;
  int32_t total = 0;
  for (int attempt = 0; attempt < kAttempts; ++attempt) {
    gcu_atomic_int_init(&unsafe_counter, 0);
    GCU_Thread t[kThreads];
    for (int i = 0; i < kThreads; ++i) {
      ASSERT_EQ(0, gcu_thread_create(&t[i], hammer_unsafely, nullptr));
    }
    for (int i = 0; i < kThreads; ++i) {
      ASSERT_EQ(0, gcu_thread_join(t[i]));
    }
    total = gcu_atomic_int_load(&unsafe_counter);
    if (total < kThreads * kPerThread) {
      return;
    }
  }
  FAIL() << "load-then-store lost nothing in " << kAttempts << " attempts, so "
            "the threads never ran concurrently and the atomic test above is "
            "not exercising anything (last total " << total << " of "
         << (kThreads * kPerThread) << ")";
}

TEST(Atomic, SizeAddAndSubtractBalanceUnderContention) {
  gcu_atomic_size_init(&shared_size, 0);
  GCU_Thread t[kThreads];
  for (int i = 0; i < kThreads; ++i) {
    ASSERT_EQ(0, gcu_thread_create(&t[i], hammer_add_sub, nullptr));
  }
  for (int i = 0; i < kThreads; ++i) {
    ASSERT_EQ(0, gcu_thread_join(t[i]));
  }
  ASSERT_EQ((size_t)(kThreads * kPerThread), gcu_atomic_size_load(&shared_size));
}

TEST(Atomic, ExactlyOneThreadWinsTheFlag) {
  gcu_atomic_flag_init(&shared_flag);
  gcu_atomic_int_init(&winners, 0);

  GCU_Thread t[kThreads];
  for (int i = 0; i < kThreads; ++i) {
    ASSERT_EQ(0, gcu_thread_create(&t[i], race_for_the_flag, nullptr));
  }
  for (int i = 0; i < kThreads; ++i) {
    ASSERT_EQ(0, gcu_thread_join(t[i]));
  }
  ASSERT_EQ(1, gcu_atomic_int_load(&winners))
      << gcu_atomic_int_load(&winners) << " threads saw the flag unset";
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
