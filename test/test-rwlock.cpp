#include <atomic>
#include <gtest/gtest.h>
#include <ghoti.io/cutil/rwlock.h>
#include <ghoti.io/cutil/thread.h>

using namespace std;

namespace {

struct Shared {
  GCU_RWLock lock;
  atomic<int> readers_inside{0};
  atomic<int> max_concurrent_readers{0};
  atomic<int> writer_saw_readers{0};
  int value = 0;
};

void * reader(void * arg) {
  Shared * s = static_cast<Shared *>(arg);
  gcu_rwlock_read_lock(&s->lock);
  int now = ++s->readers_inside;
  int prev = s->max_concurrent_readers.load();
  while (now > prev
      && !s->max_concurrent_readers.compare_exchange_weak(prev, now)) {
  }
  gcu_thread_sleep(40);
  --s->readers_inside;
  gcu_rwlock_read_unlock(&s->lock);
  return nullptr;
}

void * writer(void * arg) {
  Shared * s = static_cast<Shared *>(arg);
  gcu_rwlock_write_lock(&s->lock);
  // If the lock is doing its job, no reader is inside while this holds it.
  if (s->readers_inside.load() != 0) {
    ++s->writer_saw_readers;
  }
  ++s->value;
  gcu_thread_sleep(5);
  if (s->readers_inside.load() != 0) {
    ++s->writer_saw_readers;
  }
  gcu_rwlock_write_unlock(&s->lock);
  return nullptr;
}

} // namespace

TEST(RWLock, CreateAndDestroyReportZero) {
  GCU_RWLock l;
  ASSERT_EQ(0, gcu_rwlock_create(&l));
  ASSERT_EQ(0, gcu_rwlock_destroy(&l));
}

TEST(RWLock, NullArgumentsAreRejected) {
  ASSERT_EQ(-1, gcu_rwlock_create(nullptr));
  ASSERT_EQ(-1, gcu_rwlock_destroy(nullptr));
  ASSERT_EQ(-1, gcu_rwlock_read_lock(nullptr));
  ASSERT_EQ(-1, gcu_rwlock_read_trylock(nullptr));
  ASSERT_EQ(-1, gcu_rwlock_read_unlock(nullptr));
  ASSERT_EQ(-1, gcu_rwlock_write_lock(nullptr));
  ASSERT_EQ(-1, gcu_rwlock_write_trylock(nullptr));
  ASSERT_EQ(-1, gcu_rwlock_write_unlock(nullptr));
}

TEST(RWLock, UncontendedCyclesReportZero) {
  GCU_RWLock l;
  ASSERT_EQ(0, gcu_rwlock_create(&l));

  ASSERT_EQ(0, gcu_rwlock_read_lock(&l));
  ASSERT_EQ(0, gcu_rwlock_read_unlock(&l));
  ASSERT_EQ(0, gcu_rwlock_write_lock(&l));
  ASSERT_EQ(0, gcu_rwlock_write_unlock(&l));
  ASSERT_EQ(0, gcu_rwlock_read_trylock(&l));
  ASSERT_EQ(0, gcu_rwlock_read_unlock(&l));
  ASSERT_EQ(0, gcu_rwlock_write_trylock(&l));
  ASSERT_EQ(0, gcu_rwlock_write_unlock(&l));

  ASSERT_EQ(0, gcu_rwlock_destroy(&l));
}

TEST(RWLock, WriteTrylockFailsWhileAReaderHoldsIt) {
  GCU_RWLock l;
  ASSERT_EQ(0, gcu_rwlock_create(&l));
  ASSERT_EQ(0, gcu_rwlock_read_lock(&l));

  // The distinguishing property.  A plain mutex would fail this only because
  // it refuses the second acquire at all; here the point is that a reader
  // present is specifically what blocks a writer.
  ASSERT_NE(0, gcu_rwlock_write_trylock(&l));

  ASSERT_EQ(0, gcu_rwlock_read_unlock(&l));
  ASSERT_EQ(0, gcu_rwlock_write_trylock(&l));
  ASSERT_EQ(0, gcu_rwlock_write_unlock(&l));
  ASSERT_EQ(0, gcu_rwlock_destroy(&l));
}

TEST(RWLock, ReadTrylockFailsWhileAWriterHoldsIt) {
  GCU_RWLock l;
  ASSERT_EQ(0, gcu_rwlock_create(&l));
  ASSERT_EQ(0, gcu_rwlock_write_lock(&l));
  ASSERT_NE(0, gcu_rwlock_read_trylock(&l));
  ASSERT_EQ(0, gcu_rwlock_write_unlock(&l));
  ASSERT_EQ(0, gcu_rwlock_destroy(&l));
}

TEST(RWLock, ReadersRunConcurrently) {
  // The reason this primitive exists rather than a mutex.  If read_lock were
  // secretly exclusive -- a mutex in disguise -- every test above would still
  // pass and this one would report a maximum of 1.
  Shared s;
  ASSERT_EQ(0, gcu_rwlock_create(&s.lock));

  constexpr int kReaders = 6;
  GCU_Thread t[kReaders];
  for (int i = 0; i < kReaders; ++i) {
    ASSERT_EQ(0, gcu_thread_create(&t[i], reader, &s));
  }
  for (int i = 0; i < kReaders; ++i) {
    ASSERT_EQ(0, gcu_thread_join(t[i]));
  }

  ASSERT_GT(s.max_concurrent_readers.load(), 1)
      << "readers never overlapped; the read lock is exclusive";
  ASSERT_EQ(0, gcu_rwlock_destroy(&s.lock));
}

TEST(RWLock, AWriterExcludesEveryReader) {
  Shared s;
  ASSERT_EQ(0, gcu_rwlock_create(&s.lock));

  constexpr int kReaders = 6;
  constexpr int kWriters = 3;
  GCU_Thread r[kReaders];
  GCU_Thread w[kWriters];
  for (int i = 0; i < kReaders; ++i) {
    ASSERT_EQ(0, gcu_thread_create(&r[i], reader, &s));
  }
  for (int i = 0; i < kWriters; ++i) {
    ASSERT_EQ(0, gcu_thread_create(&w[i], writer, &s));
  }
  for (int i = 0; i < kReaders; ++i) {
    ASSERT_EQ(0, gcu_thread_join(r[i]));
  }
  for (int i = 0; i < kWriters; ++i) {
    ASSERT_EQ(0, gcu_thread_join(w[i]));
  }

  ASSERT_EQ(0, s.writer_saw_readers.load())
      << "a writer held the lock while a reader was inside";
  ASSERT_EQ(kWriters, s.value);
  ASSERT_EQ(0, gcu_rwlock_destroy(&s.lock));
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
