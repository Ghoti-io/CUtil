/**
 * @file
 * Tests for barrier.h.
 *
 * Under hang-guard.h, because every way a barrier can be wrong ends in a
 * thread that never wakes.  Six of the seven mutations tried against this
 * module were caught only by the watchdog: a barrier that is broken does not
 * return a wrong answer, it does not return.
 */

#include <atomic>
#include <vector>
#include <gtest/gtest.h>
#include <ghoti.io/cutil/barrier.h>
#include <ghoti.io/cutil/mutex.h>
#include <ghoti.io/cutil/thread.h>
#include "hang-guard.h"

using namespace std;

namespace {

// Thirty seconds against a suite that finishes in two milliseconds: every
// failure this module can have is a thread that never wakes, and the guard
// says which test it was rather than leaving a binary that died silently.
using Barrier = ghoti_test::HangGuarded<30>;

constexpr int kThreads = 4;
constexpr int kRounds = 200;

struct Shared {
  GCU_Barrier barrier;
  atomic<int> counter{0};
  atomic<int> serial{0};
  atomic<int> wrong{0};
  atomic<int> failed{0};
  int threads = kThreads;
  int rounds = kRounds;
};

/// One cycle: announce arrival, wait, then look at what everyone else did.
GCU_THREAD_FUNC_RETURN_T arriveOnce(GCU_THREAD_FUNC_ARG_T argument) {
  Shared * shared = (Shared *)argument;
  shared->counter.fetch_add(1);
  int answer = gcu_barrier_wait(&shared->barrier);
  if (answer < 0) {
    shared->failed.fetch_add(1);
  }
  if (answer == GCU_BARRIER_SERIAL) {
    shared->serial.fetch_add(1);
  }
  // Every thread must see every arrival.  A barrier that lets one through
  // early is a thread that reads a number smaller than the total.
  if (shared->counter.load() != shared->threads) {
    shared->wrong.fetch_add(1);
  }
  return (GCU_THREAD_FUNC_RETURN_T)0;
}

/// Many cycles on one barrier, with a check that no cycle bleeds into the
/// next.
GCU_THREAD_FUNC_RETURN_T arriveRepeatedly(GCU_THREAD_FUNC_ARG_T argument) {
  Shared * shared = (Shared *)argument;
  for (int round = 1; round <= shared->rounds; ++round) {
    shared->counter.fetch_add(1);

    int answer = gcu_barrier_wait(&shared->barrier);
    if (answer < 0) {
      shared->failed.fetch_add(1);
    }
    if (answer == GCU_BARRIER_SERIAL) {
      shared->serial.fetch_add(1);
    }

    // Between the two waits nobody can have started the next round, because
    // starting it means getting past the second wait, which needs everyone.
    // So this is exact, not a bound.
    if (shared->counter.load() != round * shared->threads) {
      shared->wrong.fetch_add(1);
    }

    if (gcu_barrier_wait(&shared->barrier) < 0) {
      shared->failed.fetch_add(1);
    }
  }
  return (GCU_THREAD_FUNC_RETURN_T)0;
}

GCU_THREAD_FUNC_RETURN_T arriveAndBlock(GCU_THREAD_FUNC_ARG_T argument) {
  GCU_Barrier * barrier = (GCU_Barrier *)argument;
  gcu_barrier_wait(barrier);
  return (GCU_THREAD_FUNC_RETURN_T)0;
}

/// Block until a thread is provably parked inside the barrier.
///
/// Read under the barrier's own mutex, so this is a synchronised read of a
/// private member rather than a sleep long enough to be probably true.  Once
/// `inside` is non-zero with the lock held, the thread that incremented it
/// has released the lock, which it only does by waiting.
void waitUntilSomeoneIsInside(GCU_Barrier * barrier) {
  for (;;) {
    EXPECT_EQ(0, GCU_MUTEX_LOCK(barrier->mutex));
    unsigned int inside = barrier->inside;
    EXPECT_EQ(0, GCU_MUTEX_UNLOCK(barrier->mutex));
    if (inside > 0) {
      return;
    }
    gcu_thread_sleep(1);
  }
}

} // namespace

TEST_F(Barrier, CreatingAndDestroyingReportsZero) {
  GCU_Barrier barrier;
  ASSERT_EQ(0, gcu_barrier_create(&barrier, 4));
  EXPECT_EQ(0, gcu_barrier_destroy(&barrier));
}

TEST_F(Barrier, RefusesArgumentsItCannotUse) {
  GCU_Barrier barrier;
  EXPECT_EQ(-1, gcu_barrier_create(nullptr, 4));
  EXPECT_EQ(-1, gcu_barrier_create(&barrier, 0))
      << "a barrier nobody can ever fill is not a slow barrier";
  EXPECT_EQ(-1, gcu_barrier_wait(nullptr));
  EXPECT_EQ(-1, gcu_barrier_destroy(nullptr));
}

TEST_F(Barrier, AZeroedBarrierIsNotALiveOne) {
  // The failed create above left the struct alone, so a caller who ignored
  // the return value must still not get a wait that appears to work.
  GCU_Barrier barrier = {};
  EXPECT_EQ(-1, gcu_barrier_wait(&barrier));
  EXPECT_EQ(-1, gcu_barrier_destroy(&barrier));
}

TEST_F(Barrier, ABarrierForOneTripsImmediately) {
  // Worth having rather than rejecting: it is what makes a thread count of
  // one a degenerate case instead of a special case at the call site.
  GCU_Barrier barrier;
  ASSERT_EQ(0, gcu_barrier_create(&barrier, 1));
  EXPECT_EQ(GCU_BARRIER_SERIAL, gcu_barrier_wait(&barrier));
  EXPECT_EQ(GCU_BARRIER_SERIAL, gcu_barrier_wait(&barrier));
  EXPECT_EQ(GCU_BARRIER_SERIAL, gcu_barrier_wait(&barrier));
  EXPECT_EQ(0, gcu_barrier_destroy(&barrier));
}

TEST_F(Barrier, NobodyLeavesBeforeEverybodyArrives) {
  Shared shared;
  ASSERT_EQ(0, gcu_barrier_create(&shared.barrier, kThreads));

  vector<GCU_Thread> threads(kThreads);
  for (int i = 0; i < kThreads; ++i) {
    ASSERT_EQ(0, gcu_thread_create(&threads[i], arriveOnce, &shared));
  }
  for (int i = 0; i < kThreads; ++i) {
    ASSERT_EQ(0, gcu_thread_join(threads[i]));
  }

  EXPECT_EQ(kThreads, shared.counter.load());
  EXPECT_EQ(0, shared.wrong.load())
      << "a thread left the barrier before the others had arrived";
  EXPECT_EQ(0, shared.failed.load());
  EXPECT_EQ(0, gcu_barrier_destroy(&shared.barrier));
}

TEST_F(Barrier, ExactlyOneThreadPerCycleIsToldItIsTheSerialOne) {
  Shared shared;
  ASSERT_EQ(0, gcu_barrier_create(&shared.barrier, kThreads));

  vector<GCU_Thread> threads(kThreads);
  for (int i = 0; i < kThreads; ++i) {
    ASSERT_EQ(0, gcu_thread_create(&threads[i], arriveOnce, &shared));
  }
  for (int i = 0; i < kThreads; ++i) {
    ASSERT_EQ(0, gcu_thread_join(threads[i]));
  }

  EXPECT_EQ(1, shared.serial.load())
      << "the between-phase slot has to be exactly one thread wide";
  EXPECT_EQ(0, gcu_barrier_destroy(&shared.barrier));
}

TEST_F(Barrier, TheSameBarrierSeparatesEveryPhase) {
  // The test that distinguishes a barrier from a latch, and the one a
  // hand-written version fails: with two hundred rounds and four threads the
  // first thread released is routinely back at the barrier before the last
  // one has woken, so a barrier that counts arrivals rather than cycles will
  // let it be counted into a cycle it does not belong to.
  Shared shared;
  ASSERT_EQ(0, gcu_barrier_create(&shared.barrier, kThreads));

  vector<GCU_Thread> threads(kThreads);
  for (int i = 0; i < kThreads; ++i) {
    ASSERT_EQ(0, gcu_thread_create(&threads[i], arriveRepeatedly, &shared));
  }
  for (int i = 0; i < kThreads; ++i) {
    ASSERT_EQ(0, gcu_thread_join(threads[i]));
  }

  EXPECT_EQ(kRounds * kThreads, shared.counter.load());
  EXPECT_EQ(0, shared.wrong.load())
      << "a thread saw a round's arrivals that did not add up";
  EXPECT_EQ(0, shared.failed.load());
  EXPECT_EQ(kRounds, shared.serial.load())
      << "one serial thread per cycle, over every cycle";
  EXPECT_EQ(0, gcu_barrier_destroy(&shared.barrier));
}

TEST_F(Barrier, DestroyRefusesWhileAThreadIsStillInside) {
  // POSIX leaves this undefined, which in a teardown path means a crash
  // somewhere else entirely.  Reported at the call that made the mistake
  // instead.
  GCU_Barrier barrier;
  ASSERT_EQ(0, gcu_barrier_create(&barrier, 2));

  GCU_Thread waiter;
  ASSERT_EQ(0, gcu_thread_create(&waiter, arriveAndBlock, &barrier));
  waitUntilSomeoneIsInside(&barrier);

  EXPECT_EQ(-1, gcu_barrier_destroy(&barrier));

  // Release it, and only then is destroying it allowed.
  EXPECT_EQ(GCU_BARRIER_SERIAL, gcu_barrier_wait(&barrier));
  ASSERT_EQ(0, gcu_thread_join(waiter));
  EXPECT_EQ(0, gcu_barrier_destroy(&barrier));
}

TEST_F(Barrier, DestroyingLeavesNothingUsable) {
  GCU_Barrier barrier;
  ASSERT_EQ(0, gcu_barrier_create(&barrier, 1));
  ASSERT_EQ(GCU_BARRIER_SERIAL, gcu_barrier_wait(&barrier));
  ASSERT_EQ(0, gcu_barrier_destroy(&barrier));

  EXPECT_EQ(-1, gcu_barrier_wait(&barrier))
      << "a wait on a destroyed barrier must not appear to succeed";
  EXPECT_EQ(-1, gcu_barrier_destroy(&barrier));
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
