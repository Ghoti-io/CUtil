#include <gtest/gtest.h>
#include <ghoti.io/cutil/mutex.h>
#include <ghoti.io/cutil/thread.h>

using namespace std;

TEST(Mutex, CreateAndDestroy) {
  GCU_MUTEX_T m;
  // Create returns 0 on success on both platforms.
  ASSERT_EQ(0, GCU_MUTEX_CREATE(m));
  GCU_MUTEX_DESTROY(m);
}

TEST(Mutex, EveryMacroReportsZeroOnSuccess) {
  // The regression test for the defect this contract was written to close:
  // the Windows branch expanded UNLOCK to ReleaseMutex and DESTROY to
  // CloseHandle, both of which return NON-zero on success, while the pthread
  // branch returned 0.  CREATE and TRYLOCK were already asserted below and
  // above; LOCK, UNLOCK and DESTROY were called as bare statements, so the
  // two that inverted were the two nothing checked.
  //
  // This can only run the branch this platform compiles.  The other one is
  // parse-checked by test/win32-stubs/, which cannot check a return value --
  // so read this as pinning the contract, not as verifying Windows.
  GCU_MUTEX_T m;
  ASSERT_EQ(0, GCU_MUTEX_CREATE(m));
  ASSERT_EQ(0, GCU_MUTEX_LOCK(m));
  ASSERT_EQ(0, GCU_MUTEX_UNLOCK(m));
  ASSERT_EQ(0, GCU_MUTEX_TRYLOCK(m));
  ASSERT_EQ(0, GCU_MUTEX_UNLOCK(m));
  ASSERT_EQ(0, GCU_MUTEX_DESTROY(m));
}

TEST(Mutex, LockAndUnlock) {
  GCU_MUTEX_T m;
  ASSERT_EQ(0, GCU_MUTEX_CREATE(m));

  // An uncontended lock/unlock cycle must succeed.
  GCU_MUTEX_LOCK(m);
  GCU_MUTEX_UNLOCK(m);

  // The mutex must be reusable after being unlocked.
  GCU_MUTEX_LOCK(m);
  GCU_MUTEX_UNLOCK(m);

  GCU_MUTEX_DESTROY(m);
}

TEST(Mutex, TrylockSucceedsWhenFree) {
  GCU_MUTEX_T m;
  ASSERT_EQ(0, GCU_MUTEX_CREATE(m));

  // Trylock on a free mutex acquires it and reports 0.  Note that this is a
  // zero-on-success convention, not a boolean:  `if (GCU_MUTEX_TRYLOCK(m))`
  // reads as "failed to acquire".
  ASSERT_EQ(0, GCU_MUTEX_TRYLOCK(m));
  GCU_MUTEX_UNLOCK(m);

  GCU_MUTEX_DESTROY(m);
}

namespace {

/// Arguments for the contention thread below.
struct TrylockProbe {
  GCU_MUTEX_T * mutex;  ///< The mutex to probe, already held by the caller.
  int result;           ///< The value GCU_MUTEX_TRYLOCK() reported.
};

/**
 * Attempt to acquire a mutex that the main thread already holds, and record
 * the result.  A second thread is required because the POSIX default mutex
 * type gives undefined behavior when a thread relocks a mutex it already
 * owns, so the contended case cannot be tested on a single thread.
 */
GCU_THREAD_FUNC_RETURN_T GCU_THREAD_FUNC_CALLING_CONVENTION
trylock_probe(GCU_THREAD_FUNC_ARG_T arg) {
  TrylockProbe * probe = (TrylockProbe *)arg;
  probe->result = GCU_MUTEX_TRYLOCK(*probe->mutex);

  // If the lock was unexpectedly acquired, release it so that the main
  // thread's unlock is not operating on a mutex held by a dead thread.
  if (probe->result == 0) {
    GCU_MUTEX_UNLOCK(*probe->mutex);
  }
  return (GCU_THREAD_FUNC_RETURN_T)0;
}

}

TEST(Mutex, TrylockFailsWhenHeld) {
  GCU_MUTEX_T m;
  ASSERT_EQ(0, GCU_MUTEX_CREATE(m));

  GCU_MUTEX_LOCK(m);

  // A second thread must not be able to acquire the held mutex.
  TrylockProbe probe = {&m, 0};
  GCU_Thread thread;
  ASSERT_EQ(0, gcu_thread_create(&thread, trylock_probe, &probe));
  ASSERT_EQ(0, gcu_thread_join(thread));
  ASSERT_NE(0, probe.result);

  GCU_MUTEX_UNLOCK(m);

  // Once released, the mutex may be acquired again.
  ASSERT_EQ(0, GCU_MUTEX_TRYLOCK(m));
  GCU_MUTEX_UNLOCK(m);

  GCU_MUTEX_DESTROY(m);
}

TEST(Mutex, MutualExclusion) {
  GCU_MUTEX_T m;
  ASSERT_EQ(0, GCU_MUTEX_CREATE(m));

  // Two threads incrementing a shared counter under the mutex must not lose
  // any increments to a race.
  static GCU_MUTEX_T * shared_mutex = &m;
  static size_t counter = 0;
  counter = 0;

  struct Worker {
    static GCU_THREAD_FUNC_RETURN_T GCU_THREAD_FUNC_CALLING_CONVENTION
    run(GCU_THREAD_FUNC_ARG_T arg) {
      (void)arg;
      for (size_t i = 0; i < 10000; ++i) {
        GCU_MUTEX_LOCK(*shared_mutex);
        ++counter;
        GCU_MUTEX_UNLOCK(*shared_mutex);
      }
      return (GCU_THREAD_FUNC_RETURN_T)0;
    }
  };

  GCU_Thread a, b;
  ASSERT_EQ(0, gcu_thread_create(&a, Worker::run, nullptr));
  ASSERT_EQ(0, gcu_thread_create(&b, Worker::run, nullptr));
  ASSERT_EQ(0, gcu_thread_join(a));
  ASSERT_EQ(0, gcu_thread_join(b));

  ASSERT_EQ((size_t)20000, counter);

  GCU_MUTEX_DESTROY(m);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
