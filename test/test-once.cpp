#include <atomic>
#include <gtest/gtest.h>
#include <ghoti.io/cutil/once.h>
#include <ghoti.io/cutil/thread.h>

using namespace std;

namespace {

atomic<int> run_count{0};
atomic<int> observers{0};
GCU_Once contended = GCU_ONCE_INIT;

void count_a_run(void) {
  // Sleep inside the routine so that the other threads are provably still
  // blocked when the count is taken, rather than having raced past.
  gcu_thread_sleep(30);
  ++run_count;
}

void * contender(void *) {
  gcu_once(&contended, count_a_run);
  // Every thread must observe the completed initialisation, not merely a
  // started one:  gcu_once returns only after the routine has returned.
  if (run_count.load() == 1) {
    ++observers;
  }
  return nullptr;
}

} // namespace

TEST(Once, RoutineRunsOnTheFirstCall) {
  static GCU_Once once = GCU_ONCE_INIT;
  static int calls = 0;
  ASSERT_EQ(0, gcu_once(&once, [](){ ++calls; }));
  ASSERT_EQ(1, calls);
}

TEST(Once, RoutineDoesNotRunAgain) {
  static GCU_Once once = GCU_ONCE_INIT;
  static int calls = 0;
  for (int i = 0; i < 25; ++i) {
    ASSERT_EQ(0, gcu_once(&once, [](){ ++calls; }));
  }
  ASSERT_EQ(1, calls) << "ran " << calls << " times";
}

TEST(Once, NullArgumentsAreRejected) {
  static GCU_Once once = GCU_ONCE_INIT;
  ASSERT_EQ(-1, gcu_once(nullptr, [](){}));
  ASSERT_EQ(-1, gcu_once(&once, nullptr));
  // Rejecting a bad call must not consume the one run.
  static int calls = 0;
  ASSERT_EQ(0, gcu_once(&once, [](){ ++calls; }));
  ASSERT_EQ(1, calls);
}

TEST(Once, ExactlyOneOfManyThreadsRunsIt) {
  constexpr int kThreads = 16;
  GCU_Thread t[kThreads];
  for (int i = 0; i < kThreads; ++i) {
    ASSERT_EQ(0, gcu_thread_create(&t[i], contender, nullptr));
  }
  for (int i = 0; i < kThreads; ++i) {
    ASSERT_EQ(0, gcu_thread_join(t[i]));
  }

  ASSERT_EQ(1, run_count.load()) << "the routine ran " << run_count.load()
                                 << " times";
  // The point of the primitive: the losers waited, they did not skip.  A
  // plain "test and set" without the wait would let a loser return while the
  // routine was still sleeping, and see run_count still 0.
  ASSERT_EQ(kThreads, observers.load())
      << observers.load() << " of " << kThreads
      << " threads saw completed initialisation";
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
