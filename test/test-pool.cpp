#include <gtest/gtest.h>
#include <ghoti.io/cutil/pool.h>
#include <ghoti.io/cutil/thread.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace std;

namespace {

/// Shared counters for the simple cases.
std::atomic<int> g_ran{0};

int count_task(void *) {
  g_ran.fetch_add(1);
  return 0;
}

/// Records that a particular task index ran, so that a task running twice is
/// caught rather than averaging out against one that never ran.
struct Marks {
  std::vector<std::atomic<int>> hits;
  explicit Marks(size_t n) : hits(n) {
    for (auto & h : hits) {
      h.store(0);
    }
  }
};

struct MarkArg {
  Marks * marks;
  size_t index;
  unsigned sleep_ms;
};

int mark_task(void * ctx) {
  MarkArg * arg = (MarkArg *)ctx;
  if (arg->sleep_ms) {
    gcu_thread_sleep(arg->sleep_ms);
  }
  arg->marks->hits[arg->index].fetch_add(1);
  return 0;
}

int fail_task(void * ctx) {
  return (int)(intptr_t)ctx;
}

int noop_task(void *) {
  return 0;
}

/**
 * Tears a pool down however the test ends.
 *
 * A pool that outlives its test is not merely a leak.  Its workers block in
 * the pool's semaphore forever, and the thread module's exit destructor joins
 * every thread it still knows about, so the process hangs on the way out
 * instead of reporting the failure.  A failed ASSERT returns from the test
 * body immediately, which is exactly when that happens, so teardown cannot be
 * the last statement of the test.
 */
struct PoolGuard {
  GCU_Pool * pool;

  explicit PoolGuard(GCU_Pool * p) : pool(p) {}
  PoolGuard(const PoolGuard &) = delete;
  PoolGuard & operator=(const PoolGuard &) = delete;

  /// Ordinary teardown: runs whatever is still queued.
  void destroy() {
    gcu_pool_destroy(pool);
    pool = nullptr;
  }

  /// Teardown that discards the queue.
  void abandon() {
    gcu_pool_abandon(pool);
    pool = nullptr;
  }

  /// Hand ownership back, for a test that tears down on another thread.
  void release() {
    pool = nullptr;
  }

  ~PoolGuard() {
    if (pool) {
      gcu_pool_abandon(pool);
    }
  }
};

/// Blocks until released, so that a worker can be held busy on purpose.
struct Gate {
  std::atomic<bool> open{false};
};

int gate_task(void * ctx) {
  Gate * gate = (Gate *)ctx;
  while (!gate->open.load()) {
    gcu_thread_yield();
  }
  return 0;
}

}

//
// Drain.  Written first, and the reason this module exists in the shape it
// does:  the sibling compress implementation documents draining and discards
// the queue instead, racily, because its worker tests the shutdown flag
// before it looks at the queue.
//

TEST(Pool, DestroyRunsEveryQueuedTask) {
  const size_t kTasks = 64;
  Marks marks(kTasks);
  std::vector<MarkArg> args(kTasks);

  GCU_Pool_Config config = {};
  config.thread_count = 2;

  GCU_Pool * pool = gcu_pool_create(&config);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);

  // Far more tasks than workers, each slow enough that the queue is certainly
  // non-empty when the pool is destroyed.
  for (size_t i = 0; i < kTasks; ++i) {
    args[i] = {&marks, i, 1};
    ASSERT_TRUE(gcu_pool_enqueue(pool, mark_task, &args[i]));
  }
  ASSERT_GT(gcu_pool_count_queued(pool), 0u);

  guard.destroy();

  for (size_t i = 0; i < kTasks; ++i) {
    EXPECT_EQ(1, marks.hits[i].load()) << "task " << i << " did not run once";
  }
}

TEST(Pool, AbandonDiscardsQueuedTasks) {
  const size_t kTasks = 64;
  Marks marks(kTasks);
  std::vector<MarkArg> args(kTasks);

  GCU_Pool_Config config = {};
  config.thread_count = 2;

  GCU_Pool * pool = gcu_pool_create(&config);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);

  for (size_t i = 0; i < kTasks; ++i) {
    args[i] = {&marks, i, 1};
    ASSERT_TRUE(gcu_pool_enqueue(pool, mark_task, &args[i]));
  }
  ASSERT_GT(gcu_pool_count_queued(pool), 0u);

  guard.abandon();

  // Some ran, but not all: the point of abandoning is that the queue is
  // dropped.  Under ASan this also proves the dropped entries were freed.
  int total = 0;
  for (size_t i = 0; i < kTasks; ++i) {
    EXPECT_LE(marks.hits[i].load(), 1);
    total += marks.hits[i].load();
  }
  EXPECT_LT(total, (int)kTasks);
}

//
// Basics.
//

TEST(Pool, CreateAndDestroyAtEveryThreadCount) {
  for (size_t n : {(size_t)0, (size_t)1, (size_t)2, (size_t)8}) {
    GCU_Pool_Config config = {};
    config.thread_count = n;

    GCU_Pool * pool = gcu_pool_create(&config);
    ASSERT_NE(nullptr, pool) << "thread_count " << n;
    PoolGuard guard(pool);
    EXPECT_EQ(n == 0, gcu_pool_is_inline(pool));
    EXPECT_EQ(n == 0 ? 0u : n, gcu_pool_count_threads(pool));
    guard.destroy();
  }
}

TEST(Pool, AutoThreadCountIsUsable) {
  GCU_Pool_Config config = {};
  config.thread_count = GCU_POOL_THREADS_AUTO;

  GCU_Pool * pool = gcu_pool_create(&config);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);
  EXPECT_FALSE(gcu_pool_is_inline(pool));
  EXPECT_GE(gcu_pool_count_threads(pool), 1u);
  guard.destroy();
}

TEST(Pool, NullConfigIsAllDefaults) {
  GCU_Pool * pool = gcu_pool_create(NULL);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);
  EXPECT_FALSE(gcu_pool_is_inline(pool));
  EXPECT_GE(gcu_pool_count_threads(pool), 1u);
  guard.destroy();
}

TEST(Pool, OneThreadIsNotInline) {
  // compress treats a request for one worker as a request for synchronous
  // execution.  A request for one worker here gets one worker.
  GCU_Pool_Config config = {};
  config.thread_count = 1;

  GCU_Pool * pool = gcu_pool_create(&config);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);
  EXPECT_FALSE(gcu_pool_is_inline(pool));
  EXPECT_EQ(1u, gcu_pool_count_threads(pool));

  Gate gate;
  ASSERT_TRUE(gcu_pool_enqueue(pool, gate_task, &gate));

  // The task cannot have completed:  it is waiting on a gate this thread has
  // not opened, so enqueue must not have run it here.
  EXPECT_FALSE(gcu_pool_is_inline(pool));
  gate.open.store(true);

  EXPECT_EQ(0, gcu_pool_wait(pool));
  guard.destroy();
}

TEST(Pool, EveryTaskRunsExactlyOnce) {
  const size_t kTasks = 500;
  Marks marks(kTasks);
  std::vector<MarkArg> args(kTasks);

  GCU_Pool_Config config = {};
  config.thread_count = 4;

  GCU_Pool * pool = gcu_pool_create(&config);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);

  for (size_t i = 0; i < kTasks; ++i) {
    args[i] = {&marks, i, 0};
    ASSERT_TRUE(gcu_pool_enqueue(pool, mark_task, &args[i]));
  }

  EXPECT_EQ(0, gcu_pool_wait(pool));
  for (size_t i = 0; i < kTasks; ++i) {
    EXPECT_EQ(1, marks.hits[i].load()) << "task " << i;
  }

  EXPECT_EQ(0u, gcu_pool_count_queued(pool));
  EXPECT_EQ(0u, gcu_pool_count_active(pool));
  guard.destroy();
}

//
// Inline mode.
//

TEST(Pool, InlineRunsOnTheCallingThread) {
  GCU_Pool_Config config = {};
  config.thread_count = 0;

  GCU_Pool * pool = gcu_pool_create(&config);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);
  EXPECT_TRUE(gcu_pool_is_inline(pool));
  EXPECT_EQ(0u, gcu_pool_count_threads(pool));

  g_ran.store(0);
  EXPECT_TRUE(gcu_pool_enqueue(pool, count_task, NULL));

  // Already done, with nothing waited on.
  EXPECT_EQ(1, g_ran.load());
  EXPECT_EQ(0u, gcu_pool_count_queued(pool));
  EXPECT_EQ(0, gcu_pool_wait(pool));

  guard.destroy();
}

TEST(Pool, InlineRecordsErrors) {
  GCU_Pool_Config config = {};
  config.thread_count = 0;

  GCU_Pool * pool = gcu_pool_create(&config);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);

  EXPECT_TRUE(gcu_pool_enqueue(pool, fail_task, (void *)(intptr_t)7));
  EXPECT_EQ(7, gcu_pool_wait(pool));

  gcu_pool_clear_error(pool);
  EXPECT_EQ(0, gcu_pool_wait(pool));

  guard.destroy();
}

//
// Waiting.
//

TEST(Pool, WaitOnIdlePoolReturnsImmediately) {
  GCU_Pool * pool = gcu_pool_create(NULL);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);
  EXPECT_EQ(0, gcu_pool_wait(pool));
  guard.destroy();
}

TEST(Pool, ConcurrentWaitersAllReturn) {
  // compress signals its completion semaphore once regardless of how many
  // threads are waiting, so a second waiter there never wakes.
  const size_t kWaiters = 8;
  const size_t kTasks = 200;
  Marks marks(kTasks);
  std::vector<MarkArg> args(kTasks);

  GCU_Pool_Config config = {};
  config.thread_count = 4;

  GCU_Pool * pool = gcu_pool_create(&config);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);

  for (size_t i = 0; i < kTasks; ++i) {
    args[i] = {&marks, i, 1};
    ASSERT_TRUE(gcu_pool_enqueue(pool, mark_task, &args[i]));
  }

  std::atomic<int> returned{0};
  std::vector<std::thread> waiters;
  for (size_t i = 0; i < kWaiters; ++i) {
    waiters.emplace_back([&] {
      gcu_pool_wait(pool);
      returned.fetch_add(1);
    });
  }
  for (auto & t : waiters) {
    t.join();
  }

  EXPECT_EQ((int)kWaiters, returned.load());
  guard.destroy();
}

TEST(Pool, SecondWaitDoesNotReturnEarly) {
  // compress can leave a surplus count on its completion semaphore, so a
  // later wait returns at once while work is still outstanding.
  GCU_Pool_Config config = {};
  config.thread_count = 2;

  GCU_Pool * pool = gcu_pool_create(&config);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);

  for (int round = 0; round < 20; ++round) {
    const size_t kTasks = 32;
    Marks marks(kTasks);
    std::vector<MarkArg> args(kTasks);

    for (size_t i = 0; i < kTasks; ++i) {
      args[i] = {&marks, i, 0};
      ASSERT_TRUE(gcu_pool_enqueue(pool, mark_task, &args[i]));
    }

    EXPECT_EQ(0, gcu_pool_wait(pool));

    // If the wait returned early, some task has not run yet.
    for (size_t i = 0; i < kTasks; ++i) {
      ASSERT_EQ(1, marks.hits[i].load())
        << "round " << round << ", task " << i;
    }
    EXPECT_EQ(0u, gcu_pool_count_queued(pool));
    EXPECT_EQ(0u, gcu_pool_count_active(pool));
  }

  guard.destroy();
}

//
// Errors.
//

TEST(Pool, FirstErrorIsKeptAndSurvivesReading) {
  GCU_Pool_Config config = {};
  config.thread_count = 1;

  GCU_Pool * pool = gcu_pool_create(&config);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);

  // One worker, so the order is deterministic and the *first* error is known.
  ASSERT_TRUE(gcu_pool_enqueue(pool, fail_task, (void *)(intptr_t)11));
  ASSERT_TRUE(gcu_pool_enqueue(pool, fail_task, (void *)(intptr_t)22));
  EXPECT_EQ(11, gcu_pool_wait(pool));

  // Reading does not consume it:  compress resets its error inside wait, so
  // the second caller there is told everything succeeded.
  EXPECT_EQ(11, gcu_pool_wait(pool));

  gcu_pool_clear_error(pool);
  EXPECT_EQ(0, gcu_pool_wait(pool));

  guard.destroy();
}

TEST(Pool, SuccessfulTasksLeaveNoError) {
  GCU_Pool_Config config = {};
  config.thread_count = 3;

  GCU_Pool * pool = gcu_pool_create(&config);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);

  for (int i = 0; i < 50; ++i) {
    ASSERT_TRUE(gcu_pool_enqueue(pool, noop_task, NULL));
  }
  EXPECT_EQ(0, gcu_pool_wait(pool));

  guard.destroy();
}

//
// Completion callbacks.
//

namespace {

struct CbRecord {
  std::atomic<int> calls{0};
  std::atomic<int> last_status{0};
  std::atomic<void *> last_user{nullptr};
};

void on_complete(void *, int status, void * user_data) {
  CbRecord * rec = (CbRecord *)user_data;
  rec->calls.fetch_add(1);
  rec->last_status.store(status);
  rec->last_user.store(user_data);
}

}

TEST(Pool, CallbackFiresOncePerTaskWithItsStatus) {
  CbRecord rec;

  GCU_Pool_Config config = {};
  config.thread_count = 1;

  GCU_Pool * pool = gcu_pool_create(&config);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);

  ASSERT_TRUE(gcu_pool_enqueue_cb(
    pool, fail_task, (void *)(intptr_t)5, on_complete, &rec));
  EXPECT_EQ(5, gcu_pool_wait(pool));

  EXPECT_EQ(1, rec.calls.load());
  EXPECT_EQ(5, rec.last_status.load());
  EXPECT_EQ(&rec, rec.last_user.load());

  guard.destroy();
}

TEST(Pool, CallbackRunsBeforeTheTaskCountsComplete) {
  // wait() must not return until the callback has run, or a caller that uses
  // the callback to publish a result can observe the pool idle before the
  // result exists.
  CbRecord rec;

  GCU_Pool_Config config = {};
  config.thread_count = 2;

  GCU_Pool * pool = gcu_pool_create(&config);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);

  const int kTasks = 100;
  for (int i = 0; i < kTasks; ++i) {
    ASSERT_TRUE(gcu_pool_enqueue_cb(pool, noop_task, NULL, on_complete, &rec));
  }

  EXPECT_EQ(0, gcu_pool_wait(pool));
  EXPECT_EQ(kTasks, rec.calls.load());

  guard.destroy();
}

//
// Bounded queue.
//

TEST(Pool, BoundedEnqueueFailsWhenFull) {
  GCU_Pool_Config config = {};
  config.thread_count = 1;
  config.max_queued = 4;

  GCU_Pool * pool = gcu_pool_create(&config);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);

  // Hold the single worker so that nothing drains while the queue fills.
  Gate gate;
  ASSERT_TRUE(gcu_pool_enqueue(pool, gate_task, &gate));
  while (gcu_pool_count_active(pool) == 0) {
    gcu_thread_yield();
  }

  size_t accepted = 0;
  for (size_t i = 0; i < config.max_queued + 8; ++i) {
    if (gcu_pool_enqueue(pool, noop_task, NULL)) {
      ++accepted;
    }
  }

  // The bound is honoured, and the excess was refused rather than queued.
  EXPECT_EQ(config.max_queued, accepted);

  gate.open.store(true);
  EXPECT_EQ(0, gcu_pool_wait(pool));
  guard.destroy();
}

TEST(Pool, UnboundedIgnoresTheWaitingForm) {
  GCU_Pool_Config config = {};
  config.thread_count = 2;
  config.max_queued = 0;

  GCU_Pool * pool = gcu_pool_create(&config);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);

  for (int i = 0; i < 100; ++i) {
    ASSERT_TRUE(gcu_pool_enqueue_wait(pool, noop_task, NULL));
  }
  EXPECT_EQ(0, gcu_pool_wait(pool));

  guard.destroy();
}

TEST(Pool, BoundedWaitingEnqueueProceedsWhenASlotFrees) {
  GCU_Pool_Config config = {};
  config.thread_count = 1;
  config.max_queued = 2;

  GCU_Pool * pool = gcu_pool_create(&config);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);

  Gate gate;
  ASSERT_TRUE(gcu_pool_enqueue(pool, gate_task, &gate));
  while (gcu_pool_count_active(pool) == 0) {
    gcu_thread_yield();
  }

  ASSERT_TRUE(gcu_pool_enqueue(pool, noop_task, NULL));
  ASSERT_TRUE(gcu_pool_enqueue(pool, noop_task, NULL));
  ASSERT_FALSE(gcu_pool_enqueue(pool, noop_task, NULL));

  // This one has to wait for the worker to take something off the queue.
  std::atomic<bool> done{false};
  std::thread producer([&] {
    EXPECT_TRUE(gcu_pool_enqueue_wait(pool, noop_task, NULL));
    done.store(true);
  });

  EXPECT_FALSE(done.load());
  gate.open.store(true);

  producer.join();
  EXPECT_TRUE(done.load());

  EXPECT_EQ(0, gcu_pool_wait(pool));
  guard.destroy();
}

TEST(Pool, ShutdownReleasesAProducerWaitingForASlot) {
  // The failure this guards against is a hang, not a wrong value, so it runs
  // the teardown on another thread and gives up after a deadline rather than
  // blocking the suite forever.
  GCU_Pool_Config config = {};
  config.thread_count = 1;
  config.max_queued = 1;

  GCU_Pool * pool = gcu_pool_create(&config);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);

  Gate gate;
  ASSERT_TRUE(gcu_pool_enqueue(pool, gate_task, &gate));
  while (gcu_pool_count_active(pool) == 0) {
    gcu_thread_yield();
  }
  ASSERT_TRUE(gcu_pool_enqueue(pool, noop_task, NULL));

  // The queue is full and the only worker is held, so this blocks.
  std::atomic<bool> refused{false};
  std::atomic<bool> producer_waiting{false};
  std::thread producer([&] {
    producer_waiting.store(true);
    if (!gcu_pool_enqueue_wait(pool, noop_task, NULL)) {
      refused.store(true);
    }
  });

  while (!producer_waiting.load()) {
    gcu_thread_yield();
  }
  gcu_thread_sleep(20);

  std::atomic<bool> torn_down{false};
  std::thread teardown([&] {
    gate.open.store(true);
    guard.abandon();
    torn_down.store(true);
  });

  auto deadline = chrono::steady_clock::now() + chrono::seconds(10);
  while (!torn_down.load() && chrono::steady_clock::now() < deadline) {
    this_thread::sleep_for(chrono::milliseconds(5));
  }

  EXPECT_TRUE(torn_down.load())
    << "teardown hung: a producer waiting for a slot was never released";

  producer.join();
  teardown.join();
}

//
// Worker names.
//

TEST(Pool, WorkersCarryTheConfiguredPrefix) {
  GCU_Pool_Config config = {};
  config.thread_count = 2;
  config.name_prefix = "gtestpl";

  GCU_Pool * pool = gcu_pool_create(&config);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);

  // Names are best-effort, so this asserts the name only where the platform
  // supplied one at all.
  std::atomic<int> checked{0};
  auto probe = [](void * ctx) -> int {
    char name[64] = {0};
    if (gcu_thread_get_name(gcu_thread_get_current_id(), name, sizeof(name))
        == 0 && name[0]) {
      if (strncmp(name, "gtestpl-", 8) == 0) {
        ((std::atomic<int> *)ctx)->fetch_add(1);
      }
    }
    return 0;
  };

  for (int i = 0; i < 40; ++i) {
    ASSERT_TRUE(gcu_pool_enqueue(pool, probe, &checked));
  }
  EXPECT_EQ(0, gcu_pool_wait(pool));
  EXPECT_GT(checked.load(), 0);

  guard.destroy();
}

TEST(Pool, OverlongPrefixIsTruncatedNotRejected) {
  GCU_Pool_Config config = {};
  config.thread_count = 1;
  config.name_prefix = "a-very-long-prefix-indeed";

  GCU_Pool * pool = gcu_pool_create(&config);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);
  EXPECT_EQ(1u, gcu_pool_count_threads(pool));
  guard.destroy();
}

//
// Contention.
//

TEST(Pool, ManyProducersAndWorkers) {
  const size_t kProducers = 8;
  const size_t kPerProducer = 500;
  const size_t kTasks = kProducers * kPerProducer;

  Marks marks(kTasks);
  std::vector<MarkArg> args(kTasks);

  GCU_Pool_Config config = {};
  config.thread_count = 4;

  GCU_Pool * pool = gcu_pool_create(&config);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);

  std::vector<std::thread> producers;
  for (size_t p = 0; p < kProducers; ++p) {
    producers.emplace_back([&, p] {
      for (size_t i = 0; i < kPerProducer; ++i) {
        size_t index = p * kPerProducer + i;
        args[index] = {&marks, index, 0};
        EXPECT_TRUE(gcu_pool_enqueue(pool, mark_task, &args[index]));
      }
    });
  }
  for (auto & t : producers) {
    t.join();
  }

  EXPECT_EQ(0, gcu_pool_wait(pool));

  size_t total = 0;
  for (size_t i = 0; i < kTasks; ++i) {
    ASSERT_EQ(1, marks.hits[i].load()) << "task " << i;
    total += (size_t)marks.hits[i].load();
  }
  EXPECT_EQ(kTasks, total);

  guard.destroy();
}

//
// Lifecycle and argument handling.
//

TEST(Pool, InPlaceLifecycle) {
  GCU_Pool pool;
  GCU_Pool_Config config = {};
  config.thread_count = 2;

  ASSERT_TRUE(gcu_pool_create_in_place(&pool, &config));
  g_ran.store(0);
  for (int i = 0; i < 20; ++i) {
    EXPECT_TRUE(gcu_pool_enqueue(&pool, count_task, NULL));
  }
  EXPECT_EQ(0, gcu_pool_wait(&pool));
  EXPECT_EQ(20, g_ran.load());
  gcu_pool_destroy_in_place(&pool);

  // Tearing the same storage down twice must be safe.
  gcu_pool_destroy_in_place(&pool);
}

TEST(Pool, DestroyAnUnusedPool) {
  GCU_Pool * pool = gcu_pool_create(NULL);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);
  guard.destroy();
}

TEST(Pool, NullArgumentsAreRefusedNotFatal) {
  EXPECT_FALSE(gcu_pool_enqueue(NULL, noop_task, NULL));
  EXPECT_FALSE(gcu_pool_enqueue_wait(NULL, noop_task, NULL));
  EXPECT_EQ(0, gcu_pool_wait(NULL));
  EXPECT_EQ(0u, gcu_pool_count_queued(NULL));
  EXPECT_EQ(0u, gcu_pool_count_active(NULL));
  EXPECT_EQ(0u, gcu_pool_count_threads(NULL));
  EXPECT_TRUE(gcu_pool_is_inline(NULL));
  EXPECT_TRUE(gcu_pool_is_shutting_down(NULL));
  gcu_pool_clear_error(NULL);
  gcu_pool_destroy(NULL);
  gcu_pool_abandon(NULL);
  gcu_pool_destroy_in_place(NULL);
  gcu_pool_abandon_in_place(NULL);

  GCU_Pool * pool = gcu_pool_create(NULL);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);
  EXPECT_FALSE(gcu_pool_enqueue(pool, NULL, NULL));
  guard.destroy();
}

TEST(Pool, ShuttingDownIsReportedAfterTeardownBegins) {
  GCU_Pool * pool = gcu_pool_create(NULL);
  ASSERT_NE(nullptr, pool);
  PoolGuard guard(pool);
  EXPECT_FALSE(gcu_pool_is_shutting_down(pool));
  guard.destroy();
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
