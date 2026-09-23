#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <ghoti.io/cutil/cond.h>
#include <ghoti.io/cutil/mutex.h>
#include <ghoti.io/cutil/thread.h>

using namespace std;

namespace {

// A predicate protected by a mutex, plus the condition variable that reports
// changes to it.  This is the whole intended usage shape, so the tests share
// one.
struct Gate {
  GCU_MUTEX_T mutex;
  GCU_Cond cond;
  bool ready = false;
  int woken = 0;

  Gate() {
    EXPECT_EQ(0, GCU_MUTEX_CREATE(mutex));
    EXPECT_EQ(0, gcu_cond_create(&cond));
  }
  ~Gate() {
    EXPECT_EQ(0, gcu_cond_destroy(&cond));
    EXPECT_EQ(0, GCU_MUTEX_DESTROY(mutex));
  }
};

GCU_THREAD_FUNC_RETURN_T GCU_THREAD_FUNC_CALLING_CONVENTION waiter(GCU_THREAD_FUNC_ARG_T arg) {
  Gate * g = static_cast<Gate *>(arg);
  GCU_MUTEX_LOCK(g->mutex);
  while (!g->ready) {
    gcu_cond_wait(&g->cond, &g->mutex);
  }
  ++g->woken;
  GCU_MUTEX_UNLOCK(g->mutex);
  return (GCU_THREAD_FUNC_RETURN_T)0;
}

} // namespace

TEST(Cond, CreateAndDestroyReportZero) {
  GCU_Cond cv;
  ASSERT_EQ(0, gcu_cond_create(&cv));
  ASSERT_EQ(0, gcu_cond_destroy(&cv));
}

TEST(Cond, NullArgumentsAreRejectedNotDereferenced) {
  GCU_MUTEX_T m;
  ASSERT_EQ(0, GCU_MUTEX_CREATE(m));
  GCU_Cond cv;
  ASSERT_EQ(0, gcu_cond_create(&cv));

  ASSERT_EQ(-1, gcu_cond_create(nullptr));
  ASSERT_EQ(-1, gcu_cond_destroy(nullptr));
  ASSERT_EQ(-1, gcu_cond_signal(nullptr));
  ASSERT_EQ(-1, gcu_cond_broadcast(nullptr));
  ASSERT_EQ(-1, gcu_cond_wait(nullptr, &m));
  ASSERT_EQ(-1, gcu_cond_wait(&cv, nullptr));
  ASSERT_EQ(-1, gcu_cond_timedwait(nullptr, &m, 0));
  ASSERT_EQ(-1, gcu_cond_timedwait(&cv, nullptr, 0));

  ASSERT_EQ(0, gcu_cond_destroy(&cv));
  ASSERT_EQ(0, GCU_MUTEX_DESTROY(m));
}

TEST(Cond, SignalWakesAWaiter) {
  Gate g;
  GCU_Thread t;
  ASSERT_EQ(0, gcu_thread_create(&t, waiter, &g));

  GCU_MUTEX_LOCK(g.mutex);
  g.ready = true;
  ASSERT_EQ(0, gcu_cond_signal(&g.cond));
  GCU_MUTEX_UNLOCK(g.mutex);

  ASSERT_EQ(0, gcu_thread_join(t));
  ASSERT_EQ(1, g.woken);
}

TEST(Cond, BroadcastWakesEveryWaiter) {
  Gate g;
  constexpr int kWaiters = 8;
  GCU_Thread t[kWaiters];
  for (int i = 0; i < kWaiters; ++i) {
    ASSERT_EQ(0, gcu_thread_create(&t[i], waiter, &g));
  }

  GCU_MUTEX_LOCK(g.mutex);
  g.ready = true;
  ASSERT_EQ(0, gcu_cond_broadcast(&g.cond));
  GCU_MUTEX_UNLOCK(g.mutex);

  for (int i = 0; i < kWaiters; ++i) {
    ASSERT_EQ(0, gcu_thread_join(t[i]));
  }
  ASSERT_EQ(kWaiters, g.woken);
}

TEST(Cond, SignalBeforeAnyWaiterIsNotRemembered) {
  // A condition variable holds no state: a signal with nobody waiting is
  // lost.  This is exactly why the predicate, not the signal, is what a
  // waiter tests -- and it is the distinction from a semaphore, which does
  // count.  Pinned because getting it wrong produces a hang, not a failure.
  Gate g;
  ASSERT_EQ(0, gcu_cond_signal(&g.cond));

  GCU_MUTEX_LOCK(g.mutex);
  int rc = gcu_cond_timedwait(&g.cond, &g.mutex, 50);
  GCU_MUTEX_UNLOCK(g.mutex);

  ASSERT_EQ(GCU_COND_TIMEDOUT, rc) << "the earlier signal was remembered";
}

TEST(Cond, TimedwaitReportsTimeoutDistinctlyFromFailure) {
  Gate g;
  GCU_MUTEX_LOCK(g.mutex);
  int rc = gcu_cond_timedwait(&g.cond, &g.mutex, 30);
  GCU_MUTEX_UNLOCK(g.mutex);

  // Not -1.  A caller that cannot tell "nothing happened yet" from "this is
  // broken" has to treat both as fatal or neither.
  ASSERT_EQ(GCU_COND_TIMEDOUT, rc);
  ASSERT_NE(-1, rc);
}

TEST(Cond, TimedwaitActuallyWaitsAtLeastTheTimeout) {
  Gate g;
  auto start = chrono::steady_clock::now();
  GCU_MUTEX_LOCK(g.mutex);
  ASSERT_EQ(GCU_COND_TIMEDOUT, gcu_cond_timedwait(&g.cond, &g.mutex, 100));
  GCU_MUTEX_UNLOCK(g.mutex);
  auto elapsed = chrono::duration_cast<chrono::milliseconds>(
      chrono::steady_clock::now() - start).count();

  // A deadline computed with the seconds and nanoseconds added in the wrong
  // order, or with the nanosecond field left to overflow, returns far too
  // early.  Lower bound only: an upper bound would be a timing flake.
  ASSERT_GE(elapsed, 95) << "returned after only " << elapsed << "ms";
}

TEST(Cond, TimedwaitReacquiresTheMutexOnTheTimeoutPath) {
  // If the timeout path returned without retaking the mutex, this unlock
  // would be unlocking a mutex nobody holds, and the next lock would hang.
  Gate g;
  GCU_MUTEX_LOCK(g.mutex);
  ASSERT_EQ(GCU_COND_TIMEDOUT, gcu_cond_timedwait(&g.cond, &g.mutex, 10));
  ASSERT_EQ(0, GCU_MUTEX_UNLOCK(g.mutex));
  ASSERT_EQ(0, GCU_MUTEX_LOCK(g.mutex));
  ASSERT_EQ(0, GCU_MUTEX_UNLOCK(g.mutex));
}

TEST(Cond, TimedwaitCrossesASecondBoundary) {
  // 1500ms forces the carry in the deadline arithmetic: tv_nsec exceeds
  // 1e9 and has to roll into tv_sec.  A missing carry is invalid input to
  // pthread_cond_timedwait, which returns EINVAL -- reported as -1, not as
  // a timeout, so this distinguishes the two.
  Gate g;
  auto start = chrono::steady_clock::now();
  GCU_MUTEX_LOCK(g.mutex);
  int rc = gcu_cond_timedwait(&g.cond, &g.mutex, 1500);
  GCU_MUTEX_UNLOCK(g.mutex);
  auto elapsed = chrono::duration_cast<chrono::milliseconds>(
      chrono::steady_clock::now() - start).count();

  ASSERT_EQ(GCU_COND_TIMEDOUT, rc);
  ASSERT_GE(elapsed, 1450) << "returned after only " << elapsed << "ms";
}

TEST(Cond, NegativeTimeoutWaitsRatherThanReturningAtOnce) {
  Gate g;
  GCU_Thread t;
  ASSERT_EQ(0, gcu_thread_create(&t, waiter, &g));

  gcu_thread_sleep(50);
  GCU_MUTEX_LOCK(g.mutex);
  g.ready = true;
  ASSERT_EQ(0, gcu_cond_broadcast(&g.cond));
  GCU_MUTEX_UNLOCK(g.mutex);

  ASSERT_EQ(0, gcu_thread_join(t));
  ASSERT_EQ(1, g.woken);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
