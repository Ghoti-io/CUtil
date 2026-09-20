/**
 * @file
 *
 * Tests for the sequencer (reorder buffer).
 *
 * The cases that earn their place here are the ones that fail against a
 * plausible wrong implementation:  completion order not mattering, *every*
 * blocked waiter being released rather than one, teardown not freeing memory
 * out from under a sleeping thread, and a ring that wraps.
 *
 * Copyright 2026 by Corey Pennycuff
 */

#include <gtest/gtest.h>
#include <ghoti.io/cutil/sequencer.h>
#include <ghoti.io/cutil/thread.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

using namespace std;

namespace {

/**
 * Tears a sequencer down however the test ends.
 *
 * A sequencer that outlives its test leaves any helper thread blocked in its
 * semaphore forever, and the process hangs on the way out instead of
 * reporting the failure.  A failed ASSERT returns from the test body
 * immediately, which is exactly when that happens, so teardown cannot be the
 * last statement of the test.
 */
/**
 * Joins a helper thread however the test ends.
 *
 * A failed ASSERT returns from the test body immediately, and a joinable
 * std::thread destroyed at that point calls std::terminate -- which loses
 * the failure message that would have explained what went wrong.  Shutting
 * the sequencer down first is what lets a blocked helper actually leave.
 */
struct ThreadGuard {
  thread t;
  GCU_Sequencer * seq;

  ThreadGuard(thread && thread_in, GCU_Sequencer * s)
    : t(std::move(thread_in)), seq(s) {}
  ThreadGuard(const ThreadGuard &) = delete;
  ThreadGuard & operator=(const ThreadGuard &) = delete;

  void join() {
    if (t.joinable()) {
      t.join();
    }
  }

  ~ThreadGuard() {
    if (t.joinable()) {
      if (seq) {
        gcu_sequencer_shutdown(seq);
      }
      t.join();
    }
  }
};

struct SeqGuard {
  GCU_Sequencer * seq;

  explicit SeqGuard(GCU_Sequencer * s) : seq(s) {}
  SeqGuard(const SeqGuard &) = delete;
  SeqGuard & operator=(const SeqGuard &) = delete;

  void destroy() {
    if (seq) {
      gcu_sequencer_destroy(seq);
      seq = nullptr;
    }
  }

  ~SeqGuard() {
    destroy();
  }
};

/// A payload distinguishable from every other, so that a mis-ordered result
/// is caught rather than merely a miscounted one.
int g_items[256];

void * item(size_t i) {
  return &g_items[i];
}

size_t index_of(void * p) {
  return (size_t)((int *)p - g_items);
}

/**
 * Fails the test rather than hanging forever if a condition never arrives.
 *
 * stdout is block-buffered when the suite's output is redirected to a log,
 * so a diagnostic printed here would be lost if the process later hung.
 * Flushing makes the deadline message survive.
 */
template <typename F>
bool wait_until(F predicate, int timeout_ms = 5000) {
  for (int waited = 0; waited < timeout_ms; waited += 5) {
    if (predicate()) {
      return true;
    }
    gcu_thread_sleep(5);
  }
  fflush(stdout);
  return false;
}

} // namespace

//
// Ordering:  the whole point of the module.
//

TEST(Sequencer, ResultsComeBackInSubmissionOrder) {
  GCU_Sequencer * seq = gcu_sequencer_create(nullptr);
  ASSERT_NE(seq, nullptr);
  SeqGuard guard(seq);

  const size_t n = 32;
  vector<uint64_t> tickets(n);
  for (size_t i = 0; i < n; ++i) {
    ASSERT_EQ(gcu_sequencer_submit(seq, item(i), &tickets[i]),
      GCU_SEQUENCER_OK);
  }

  // Complete in exactly the wrong order.  A queue that hands back whatever
  // finished first would return 31, 30, 29 ... here.
  for (size_t i = n; i-- > 0;) {
    ASSERT_EQ(gcu_sequencer_complete(seq, tickets[i], (int)i),
      GCU_SEQUENCER_OK);
  }

  for (size_t i = 0; i < n; ++i) {
    void * payload = nullptr;
    int status = -1;
    ASSERT_EQ(gcu_sequencer_next(seq, &payload, &status), GCU_SEQUENCER_OK);
    EXPECT_EQ(index_of(payload), i);
    EXPECT_EQ(status, (int)i);
  }

  guard.destroy();
}

TEST(Sequencer, AnUnfinishedHeadHoldsBackFinishedFollowers) {
  GCU_Sequencer * seq = gcu_sequencer_create(nullptr);
  ASSERT_NE(seq, nullptr);
  SeqGuard guard(seq);

  uint64_t t0, t1, t2;
  ASSERT_EQ(gcu_sequencer_submit(seq, item(0), &t0), GCU_SEQUENCER_OK);
  ASSERT_EQ(gcu_sequencer_submit(seq, item(1), &t1), GCU_SEQUENCER_OK);
  ASSERT_EQ(gcu_sequencer_submit(seq, item(2), &t2), GCU_SEQUENCER_OK);

  ASSERT_EQ(gcu_sequencer_complete(seq, t1, 0), GCU_SEQUENCER_OK);
  ASSERT_EQ(gcu_sequencer_complete(seq, t2, 0), GCU_SEQUENCER_OK);

  // Two of the three are finished, but not the one at the head.
  EXPECT_FALSE(gcu_sequencer_is_ready(seq));
  EXPECT_EQ(gcu_sequencer_try_next(seq, nullptr, nullptr),
    GCU_SEQUENCER_NOT_READY);
  EXPECT_EQ(gcu_sequencer_count_outstanding(seq), 3u);

  ASSERT_EQ(gcu_sequencer_complete(seq, t0, 0), GCU_SEQUENCER_OK);
  EXPECT_TRUE(gcu_sequencer_is_ready(seq));

  // All three now come out at once, in order.
  for (size_t i = 0; i < 3; ++i) {
    void * payload = nullptr;
    ASSERT_EQ(gcu_sequencer_try_next(seq, &payload, nullptr),
      GCU_SEQUENCER_OK);
    EXPECT_EQ(index_of(payload), i);
  }
  EXPECT_EQ(gcu_sequencer_count_outstanding(seq), 0u);

  guard.destroy();
}

TEST(Sequencer, ACollectorWaitsForTheHeadAndIsWokenByIt) {
  GCU_Sequencer * seq = gcu_sequencer_create(nullptr);
  ASSERT_NE(seq, nullptr);
  SeqGuard guard(seq);

  uint64_t t0, t1;
  ASSERT_EQ(gcu_sequencer_submit(seq, item(0), &t0), GCU_SEQUENCER_OK);
  ASSERT_EQ(gcu_sequencer_submit(seq, item(1), &t1), GCU_SEQUENCER_OK);
  ASSERT_EQ(gcu_sequencer_complete(seq, t1, 0), GCU_SEQUENCER_OK);

  atomic<int> got{-1};
  atomic<bool> returned{false};
  thread collector([&]() {
    void * payload = nullptr;
    if (gcu_sequencer_next(seq, &payload, nullptr) == GCU_SEQUENCER_OK) {
      got.store((int)index_of(payload));
    }
    returned.store(true);
  });

  // It must still be blocked:  only the *second* item is finished.
  gcu_thread_sleep(50);
  EXPECT_FALSE(returned.load());

  ASSERT_EQ(gcu_sequencer_complete(seq, t0, 0), GCU_SEQUENCER_OK);
  EXPECT_TRUE(wait_until([&]() { return returned.load(); }))
    << "collector was never woken by the head completing";
  collector.join();
  EXPECT_EQ(got.load(), 0);

  guard.destroy();
}

//
// The waiter-count bug:  one post per waiter, not one post.
//

TEST(Sequencer, EveryBlockedCollectorIsReleasedNotJustOne) {
  GCU_Sequencer * seq = gcu_sequencer_create(nullptr);
  ASSERT_NE(seq, nullptr);
  SeqGuard guard(seq);

  const size_t n = 4;
  vector<uint64_t> tickets(n);
  for (size_t i = 0; i < n; ++i) {
    ASSERT_EQ(gcu_sequencer_submit(seq, item(i), &tickets[i]),
      GCU_SEQUENCER_OK);
  }

  // Four collectors, all blocked:  nothing is complete yet.
  atomic<int> finished{0};
  vector<thread> collectors;
  for (size_t i = 0; i < n; ++i) {
    collectors.emplace_back([&]() {
      void * payload = nullptr;
      if (gcu_sequencer_next(seq, &payload, nullptr) == GCU_SEQUENCER_OK) {
        finished.fetch_add(1);
      }
    });
  }
  ASSERT_TRUE(wait_until([&]() {
    return gcu_sequencer_count_outstanding(seq) == n;
  }));
  gcu_thread_sleep(50);
  EXPECT_EQ(finished.load(), 0);

  // Complete every item.  A single post per completion would release one
  // thread and leave the other three asleep forever.
  for (size_t i = 0; i < n; ++i) {
    ASSERT_EQ(gcu_sequencer_complete(seq, tickets[i], 0), GCU_SEQUENCER_OK);
  }

  bool all_done = wait_until([&]() { return finished.load() == (int)n; });
  EXPECT_TRUE(all_done) << "only " << finished.load() << " of " << n
    << " collectors were released";
  if (!all_done) {
    // Do not join threads that are still blocked; that hangs the suite.
    gcu_sequencer_shutdown(seq);
  }
  for (auto & t : collectors) {
    t.join();
  }

  guard.destroy();
}

TEST(Sequencer, EveryBlockedProducerIsReleasedNotJustOne) {
  GCU_Sequencer_Config config = {};
  config.capacity = 1;
  GCU_Sequencer * seq = gcu_sequencer_create(&config);
  ASSERT_NE(seq, nullptr);
  SeqGuard guard(seq);

  // Fill the one slot, so that every producer below must block.
  uint64_t held;
  ASSERT_EQ(gcu_sequencer_submit(seq, item(0), &held), GCU_SEQUENCER_OK);
  ASSERT_EQ(held, 0u);

  const size_t n = 3;
  atomic<int> submitted{0};
  vector<thread> producers;
  for (size_t i = 0; i < n; ++i) {
    producers.emplace_back([&, i]() {
      uint64_t ticket = 0;
      if (gcu_sequencer_submit_wait(seq, item(10 + i), &ticket)
          == GCU_SEQUENCER_OK) {
        submitted.fetch_add(1);
      }
    });
  }

  gcu_thread_sleep(50);
  ASSERT_EQ(submitted.load(), 0) << "a full sequencer admitted a producer";

  // Free the slot once per producer.  Submissions are serialised by the
  // sequencer's own mutex, so the tickets are 0, 1, 2, 3 in that order even
  // though which thread gets which is arbitrary.
  //
  // An implementation that posted once rather than once per waiter would
  // release the first producer and leave the other two asleep here.
  bool released_all = true;
  for (uint64_t k = 0; k <= n; ++k) {
    if (!wait_until([&]() {
          return gcu_sequencer_count_outstanding(seq) == 1;
        })) {
      released_all = false;
      break;
    }
    ASSERT_EQ(gcu_sequencer_complete(seq, k, 0), GCU_SEQUENCER_OK);
    ASSERT_EQ(gcu_sequencer_next(seq, nullptr, nullptr), GCU_SEQUENCER_OK);
  }

  EXPECT_TRUE(released_all) << "only " << submitted.load() << " of " << n
    << " producers were released by freeing a slot each";
  if (!released_all) {
    // Do not join threads that are still blocked; that hangs the suite.
    gcu_sequencer_shutdown(seq);
  }
  for (auto & t : producers) {
    t.join();
  }
  EXPECT_EQ(submitted.load(), (int)n);

  guard.destroy();
}

//
// Teardown with threads still inside.
//

TEST(Sequencer, DestroyReleasesACollectorWaitingForever) {
  GCU_Sequencer * seq = gcu_sequencer_create(nullptr);
  ASSERT_NE(seq, nullptr);

  uint64_t ticket;
  ASSERT_EQ(gcu_sequencer_submit(seq, item(0), &ticket), GCU_SEQUENCER_OK);

  // Never completed:  this collector can only be released by teardown.
  atomic<bool> returned{false};
  atomic<int> result{-1};
  thread collector([&]() {
    result.store((int)gcu_sequencer_next(seq, nullptr, nullptr));
    returned.store(true);
  });

  gcu_thread_sleep(50);
  ASSERT_FALSE(returned.load());

  gcu_sequencer_destroy(seq);

  EXPECT_TRUE(wait_until([&]() { return returned.load(); }))
    << "destroy did not release the blocked collector";
  collector.join();
  EXPECT_EQ(result.load(), (int)GCU_SEQUENCER_SHUTDOWN);
}

TEST(Sequencer, DestroyReleasesAProducerWaitingForASlot) {
  GCU_Sequencer_Config config = {};
  config.capacity = 1;
  GCU_Sequencer * seq = gcu_sequencer_create(&config);
  ASSERT_NE(seq, nullptr);

  uint64_t ticket;
  ASSERT_EQ(gcu_sequencer_submit(seq, item(0), &ticket), GCU_SEQUENCER_OK);

  atomic<bool> returned{false};
  atomic<int> result{-1};
  thread producer([&]() {
    uint64_t mine = 0;
    result.store((int)gcu_sequencer_submit_wait(seq, item(1), &mine));
    returned.store(true);
  });

  gcu_thread_sleep(50);
  ASSERT_FALSE(returned.load());

  gcu_sequencer_destroy(seq);

  EXPECT_TRUE(wait_until([&]() { return returned.load(); }))
    << "destroy did not release the blocked producer";
  producer.join();
  EXPECT_EQ(result.load(), (int)GCU_SEQUENCER_SHUTDOWN);
}

TEST(Sequencer, ShutdownLeavesTheObjectUsableAndIsIdempotent) {
  GCU_Sequencer * seq = gcu_sequencer_create(nullptr);
  ASSERT_NE(seq, nullptr);
  SeqGuard guard(seq);

  uint64_t ticket;
  ASSERT_EQ(gcu_sequencer_submit(seq, item(0), &ticket), GCU_SEQUENCER_OK);

  EXPECT_FALSE(gcu_sequencer_is_shutting_down(seq));
  gcu_sequencer_shutdown(seq);
  gcu_sequencer_shutdown(seq);
  EXPECT_TRUE(gcu_sequencer_is_shutting_down(seq));

  uint64_t other;
  EXPECT_EQ(gcu_sequencer_submit(seq, item(1), &other), GCU_SEQUENCER_SHUTDOWN);
  EXPECT_EQ(gcu_sequencer_next(seq, nullptr, nullptr), GCU_SEQUENCER_SHUTDOWN);
  EXPECT_EQ(gcu_sequencer_try_next(seq, nullptr, nullptr),
    GCU_SEQUENCER_SHUTDOWN);
  EXPECT_EQ(gcu_sequencer_complete(seq, ticket, 0), GCU_SEQUENCER_SHUTDOWN);
  EXPECT_EQ(gcu_sequencer_reset(seq), GCU_SEQUENCER_SHUTDOWN);

  guard.destroy();
}

//
// Capacity and the ring.
//

TEST(Sequencer, SubmitReportsFullRatherThanDeadlockingASelfCollector) {
  GCU_Sequencer_Config config = {};
  config.capacity = 4;
  GCU_Sequencer * seq = gcu_sequencer_create(&config);
  ASSERT_NE(seq, nullptr);
  SeqGuard guard(seq);

  EXPECT_EQ(gcu_sequencer_capacity(seq), 4u);

  // The single-threaded shape the blocking call would deadlock: submit until
  // told to stop, collect one, carry on.  Sixteen items through a buffer of
  // four, on one thread, with no other thread in existence.
  size_t submitted = 0;
  size_t collected = 0;
  const size_t total = 16;
  vector<uint64_t> live;

  while (collected < total) {
    uint64_t ticket = 0;
    if (submitted < total
        && gcu_sequencer_submit(seq, item(submitted), &ticket)
          == GCU_SEQUENCER_OK) {
      // Complete immediately; the point here is the capacity protocol.
      ASSERT_EQ(gcu_sequencer_complete(seq, ticket, (int)submitted),
        GCU_SEQUENCER_OK);
      ++submitted;
      continue;
    }

    void * payload = nullptr;
    int status = -1;
    ASSERT_EQ(gcu_sequencer_next(seq, &payload, &status), GCU_SEQUENCER_OK);
    EXPECT_EQ(index_of(payload), collected);
    EXPECT_EQ(status, (int)collected);
    ++collected;
  }

  EXPECT_EQ(submitted, total);
  EXPECT_EQ(gcu_sequencer_count_outstanding(seq), 0u);

  guard.destroy();
}

TEST(Sequencer, ABoundedRingWrapsRepeatedly) {
  GCU_Sequencer_Config config = {};
  config.capacity = 3;
  GCU_Sequencer * seq = gcu_sequencer_create(&config);
  ASSERT_NE(seq, nullptr);
  SeqGuard guard(seq);

  // Far more items than slots, so the ring index wraps many times.  An
  // implementation that indexed by raw ticket would run off the end.
  for (size_t i = 0; i < 100; ++i) {
    uint64_t ticket = 0;
    ASSERT_EQ(gcu_sequencer_submit(seq, item(i % 200), &ticket),
      GCU_SEQUENCER_OK) << "at item " << i;
    ASSERT_EQ(gcu_sequencer_complete(seq, ticket, (int)i), GCU_SEQUENCER_OK);

    void * payload = nullptr;
    int status = -1;
    ASSERT_EQ(gcu_sequencer_next(seq, &payload, &status), GCU_SEQUENCER_OK);
    EXPECT_EQ(index_of(payload), i % 200);
    EXPECT_EQ(status, (int)i);
  }

  guard.destroy();
}

TEST(Sequencer, TheRingGrowsPastItsInitialSizeKeepingOrder) {
  // Unbounded, so the ring must grow well past its initial 16 slots while
  // items are outstanding -- the case where growth has to re-seat every live
  // slot at a new index rather than simply reallocate.
  GCU_Sequencer * seq = gcu_sequencer_create(nullptr);
  ASSERT_NE(seq, nullptr);
  SeqGuard guard(seq);

  const size_t n = 200;
  vector<uint64_t> tickets(n);
  for (size_t i = 0; i < n; ++i) {
    ASSERT_EQ(gcu_sequencer_submit(seq, item(i % 256), &tickets[i]),
      GCU_SEQUENCER_OK) << "at item " << i;
  }
  EXPECT_EQ(gcu_sequencer_count_outstanding(seq), n);

  // Complete out of order across the growth boundaries.
  for (size_t i = 1; i < n; i += 2) {
    ASSERT_EQ(gcu_sequencer_complete(seq, tickets[i], (int)i),
      GCU_SEQUENCER_OK);
  }
  for (size_t i = 0; i < n; i += 2) {
    ASSERT_EQ(gcu_sequencer_complete(seq, tickets[i], (int)i),
      GCU_SEQUENCER_OK);
  }

  for (size_t i = 0; i < n; ++i) {
    void * payload = nullptr;
    int status = -1;
    ASSERT_EQ(gcu_sequencer_next(seq, &payload, &status), GCU_SEQUENCER_OK)
      << "at item " << i;
    EXPECT_EQ(index_of(payload), i % 256);
    EXPECT_EQ(status, (int)i);
  }

  guard.destroy();
}

TEST(Sequencer, GrowthAlsoHappensPartWayAroundTheRing) {
  // Advance the head off zero first, so that the live items straddle the
  // wrap point when the ring doubles.  Growth that copied the raw array
  // rather than re-seating by ticket would scramble the order here.
  GCU_Sequencer * seq = gcu_sequencer_create(nullptr);
  ASSERT_NE(seq, nullptr);
  SeqGuard guard(seq);

  for (size_t i = 0; i < 13; ++i) {
    uint64_t ticket = 0;
    ASSERT_EQ(gcu_sequencer_submit(seq, item(0), &ticket), GCU_SEQUENCER_OK);
    ASSERT_EQ(gcu_sequencer_complete(seq, ticket, 0), GCU_SEQUENCER_OK);
    ASSERT_EQ(gcu_sequencer_next(seq, nullptr, nullptr), GCU_SEQUENCER_OK);
  }

  const size_t n = 40;
  vector<uint64_t> tickets(n);
  for (size_t i = 0; i < n; ++i) {
    ASSERT_EQ(gcu_sequencer_submit(seq, item(i), &tickets[i]),
      GCU_SEQUENCER_OK);
  }
  for (size_t i = n; i-- > 0;) {
    ASSERT_EQ(gcu_sequencer_complete(seq, tickets[i], (int)i),
      GCU_SEQUENCER_OK);
  }
  for (size_t i = 0; i < n; ++i) {
    void * payload = nullptr;
    int status = -1;
    ASSERT_EQ(gcu_sequencer_next(seq, &payload, &status), GCU_SEQUENCER_OK);
    EXPECT_EQ(index_of(payload), i);
    EXPECT_EQ(status, (int)i);
  }

  guard.destroy();
}

//
// Misuse.
//

TEST(Sequencer, EmptyIsReportedAsEmptyNotAsAnError) {
  GCU_Sequencer * seq = gcu_sequencer_create(nullptr);
  ASSERT_NE(seq, nullptr);
  SeqGuard guard(seq);

  EXPECT_EQ(gcu_sequencer_next(seq, nullptr, nullptr), GCU_SEQUENCER_EMPTY);
  EXPECT_EQ(gcu_sequencer_try_next(seq, nullptr, nullptr),
    GCU_SEQUENCER_EMPTY);
  EXPECT_FALSE(gcu_sequencer_is_ready(seq));
  EXPECT_EQ(gcu_sequencer_count_outstanding(seq), 0u);

  guard.destroy();
}

TEST(Sequencer, BadTicketsAreRefusedNotActedOn) {
  GCU_Sequencer * seq = gcu_sequencer_create(nullptr);
  ASSERT_NE(seq, nullptr);
  SeqGuard guard(seq);

  uint64_t ticket;
  ASSERT_EQ(gcu_sequencer_submit(seq, item(0), &ticket), GCU_SEQUENCER_OK);

  // Never issued.
  EXPECT_EQ(gcu_sequencer_complete(seq, ticket + 1, 0), GCU_SEQUENCER_ERROR);
  EXPECT_EQ(gcu_sequencer_complete(seq, ticket + 999, 0), GCU_SEQUENCER_ERROR);

  // Issued once, completed once.
  EXPECT_EQ(gcu_sequencer_complete(seq, ticket, 7), GCU_SEQUENCER_OK);
  EXPECT_EQ(gcu_sequencer_complete(seq, ticket, 9), GCU_SEQUENCER_ERROR);

  // The double completion must not have overwritten the first status.
  int status = -1;
  ASSERT_EQ(gcu_sequencer_next(seq, nullptr, &status), GCU_SEQUENCER_OK);
  EXPECT_EQ(status, 7);

  // Already collected.
  EXPECT_EQ(gcu_sequencer_complete(seq, ticket, 0), GCU_SEQUENCER_ERROR);

  guard.destroy();
}

TEST(Sequencer, NullArgumentsAreRefusedNotFatal) {
  EXPECT_EQ(gcu_sequencer_next(nullptr, nullptr, nullptr),
    GCU_SEQUENCER_ERROR);
  EXPECT_EQ(gcu_sequencer_try_next(nullptr, nullptr, nullptr),
    GCU_SEQUENCER_ERROR);
  EXPECT_EQ(gcu_sequencer_complete(nullptr, 0, 0), GCU_SEQUENCER_ERROR);
  EXPECT_EQ(gcu_sequencer_reset(nullptr), GCU_SEQUENCER_ERROR);
  EXPECT_EQ(gcu_sequencer_count_outstanding(nullptr), 0u);
  EXPECT_EQ(gcu_sequencer_capacity(nullptr), 0u);
  EXPECT_FALSE(gcu_sequencer_is_ready(nullptr));
  EXPECT_FALSE(gcu_sequencer_is_shutting_down(nullptr));
  EXPECT_FALSE(gcu_sequencer_create_in_place(nullptr, nullptr));

  uint64_t ticket;
  EXPECT_EQ(gcu_sequencer_submit(nullptr, nullptr, &ticket),
    GCU_SEQUENCER_ERROR);

  // No crash, no effect.
  gcu_sequencer_destroy(nullptr);
  gcu_sequencer_destroy_in_place(nullptr);
  gcu_sequencer_shutdown(nullptr);
}

//
// Lifecycle.
//

TEST(Sequencer, InPlaceLifecycle) {
  GCU_Sequencer seq;
  GCU_Sequencer_Config config = {};
  config.capacity = 2;
  ASSERT_TRUE(gcu_sequencer_create_in_place(&seq, &config));

  uint64_t ticket;
  EXPECT_EQ(gcu_sequencer_submit(&seq, item(5), &ticket), GCU_SEQUENCER_OK);
  EXPECT_EQ(gcu_sequencer_complete(&seq, ticket, 0), GCU_SEQUENCER_OK);

  void * payload = nullptr;
  EXPECT_EQ(gcu_sequencer_next(&seq, &payload, nullptr), GCU_SEQUENCER_OK);
  EXPECT_EQ(index_of(payload), 5u);

  gcu_sequencer_destroy_in_place(&seq);
}

TEST(Sequencer, ResetDropsOutstandingItemsAndRestartsTickets) {
  GCU_Sequencer * seq = gcu_sequencer_create(nullptr);
  ASSERT_NE(seq, nullptr);
  SeqGuard guard(seq);

  uint64_t first, second;
  ASSERT_EQ(gcu_sequencer_submit(seq, item(0), &first), GCU_SEQUENCER_OK);
  ASSERT_EQ(gcu_sequencer_submit(seq, item(1), &second), GCU_SEQUENCER_OK);
  ASSERT_EQ(gcu_sequencer_complete(seq, first, 0), GCU_SEQUENCER_OK);
  EXPECT_EQ(gcu_sequencer_count_outstanding(seq), 2u);

  ASSERT_EQ(gcu_sequencer_reset(seq), GCU_SEQUENCER_OK);
  EXPECT_EQ(gcu_sequencer_count_outstanding(seq), 0u);
  EXPECT_EQ(gcu_sequencer_next(seq, nullptr, nullptr), GCU_SEQUENCER_EMPTY);

  // Ticket numbering restarts, and an old ticket is no longer valid.
  uint64_t fresh;
  ASSERT_EQ(gcu_sequencer_submit(seq, item(9), &fresh), GCU_SEQUENCER_OK);
  EXPECT_EQ(fresh, 0u);

  guard.destroy();
}

//
// Concurrency end to end.
//

TEST(Sequencer, ManyWorkersFinishOutOfOrderAndOrderIsStillExact) {
  GCU_Sequencer_Config config = {};
  config.capacity = 8;
  GCU_Sequencer * seq = gcu_sequencer_create(&config);
  ASSERT_NE(seq, nullptr);
  SeqGuard guard(seq);

  const size_t total = 256;

  // One thread submits and collects -- the shape that must not deadlock --
  // while workers complete tickets after randomly varied delays.
  atomic<bool> stop{false};
  vector<thread> workers;
  atomic<uint64_t> next_to_complete{0};
  atomic<uint64_t> issued{0};

  for (int w = 0; w < 4; ++w) {
    workers.emplace_back([&, w]() {
      while (!stop.load()) {
        uint64_t t = next_to_complete.load();
        if (t >= issued.load()) {
          gcu_thread_sleep(1);
          continue;
        }
        if (!next_to_complete.compare_exchange_strong(t, t + 1)) {
          continue;
        }
        // Stagger so completions genuinely interleave out of order.
        if ((t + (uint64_t)w) % 3 == 0) {
          gcu_thread_sleep(1);
        }
        gcu_sequencer_complete(seq, t, (int)t);
      }
    });
  }

  size_t submitted = 0;
  size_t collected = 0;
  while (collected < total) {
    if (submitted < total) {
      uint64_t ticket = 0;
      GCU_Sequencer_Result r = gcu_sequencer_submit(seq, item(submitted % 256),
        &ticket);
      if (r == GCU_SEQUENCER_OK) {
        ++submitted;
        issued.store(ticket + 1);
        continue;
      }
      ASSERT_EQ(r, GCU_SEQUENCER_FULL);
    }

    void * payload = nullptr;
    int status = -1;
    ASSERT_EQ(gcu_sequencer_next(seq, &payload, &status), GCU_SEQUENCER_OK)
      << "at item " << collected;
    ASSERT_EQ(index_of(payload), collected % 256) << "out of order at "
      << collected;
    ASSERT_EQ(status, (int)collected);
    ++collected;
  }

  stop.store(true);
  for (auto & t : workers) {
    t.join();
  }
  EXPECT_EQ(submitted, total);

  guard.destroy();
}

TEST(Sequencer, ASeparateProducerMayBlockSafelyWhileAnotherThreadCollects) {
  GCU_Sequencer_Config config = {};
  config.capacity = 4;
  GCU_Sequencer * seq = gcu_sequencer_create(&config);
  ASSERT_NE(seq, nullptr);
  SeqGuard guard(seq);

  const size_t total = 64;

  // The blocking submit is safe here precisely because a *different* thread
  // collects.  This is the arrangement its documentation permits.
  atomic<size_t> produced{0};
  ThreadGuard producer(thread([&]() {
    for (size_t i = 0; i < total; ++i) {
      uint64_t ticket = 0;
      if (gcu_sequencer_submit_wait(seq, item(i % 256), &ticket)
          != GCU_SEQUENCER_OK) {
        return;
      }
      gcu_sequencer_complete(seq, ticket, (int)i);
      produced.fetch_add(1);
    }
  }), seq);

  for (size_t i = 0; i < total; ++i) {
    void * payload = nullptr;
    int status = -1;
    GCU_Sequencer_Result r = GCU_SEQUENCER_EMPTY;

    // A collector may outrun its producer, and gcu_sequencer_next() reports
    // that as EMPTY rather than blocking:  there is nothing outstanding for
    // a completion to ever make ready, and blocking would be waiting on a
    // submission the sequencer has no reason to expect.  Retrying is what
    // the contract asks of a consumer that does not know when its producer
    // will get there.
    ASSERT_TRUE(wait_until([&]() {
      r = gcu_sequencer_next(seq, &payload, &status);
      return r != GCU_SEQUENCER_EMPTY;
    })) << "producer never reached item " << i;

    ASSERT_EQ(r, GCU_SEQUENCER_OK) << "at item " << i;
    EXPECT_EQ(index_of(payload), i % 256);
    EXPECT_EQ(status, (int)i);
  }

  producer.join();
  EXPECT_EQ(produced.load(), total);

  guard.destroy();
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
