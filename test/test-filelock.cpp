#include <cstdio>
#include <cstring>
#include <string>
#include <gtest/gtest.h>
#include <ghoti.io/cutil/filelock.h>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace std;

#ifndef GCU_TEST_LOCK_DIR
#error "GCU_TEST_LOCK_DIR must name a writable directory; see the Makefile"
#endif

namespace {

string lockPath(const char * name) {
  return string(GCU_TEST_LOCK_DIR) + "/" + name + ".lock";
}

#ifndef _WIN32
// Ask a *separate process* whether it can take the lock.
//
// It has to be a separate process: a POSIX lock belongs to the open file
// description, so a second attempt from this process may succeed and would
// prove nothing.  The child uses _exit so that neither gtest's teardown nor
// ASan's atexit reporting runs in a forked copy.
int childCanLock(const string & path, bool exclusive) {
  pid_t pid = fork();
  if (pid == 0) {
    // The alarm turns a hang into a failure.
    //
    // Every call here passes wait=false, so none of them may block. If one
    // does -- which is what dropping LOCK_NB from the implementation causes
    // -- then without this the child waits forever, the parent waits on it
    // forever, and the whole suite stops with no failing test to point at.
    // SIGALRM kills the child instead, WIFEXITED is false, and the parent
    // reports which test it was. A gate that hangs is worse than one that
    // fails, because a hang gets attributed to the machine.
    alarm(5);
    GCU_File_Lock lock;
    int result = gcu_file_lock(&lock, path.c_str(), exclusive, false);
    if (result == 0) {
      gcu_file_unlock(&lock);
    }
    _exit(result == 0 ? 0 : (result == GCU_FILE_LOCK_BUSY ? 1 : 2));
  }
  EXPECT_GT(pid, 0);
  int status = 0;
  EXPECT_EQ(pid, waitpid(pid, &status, 0));
  EXPECT_TRUE(WIFEXITED(status))
      << "the child did not exit normally; a wait=false call blocked";
  if (!WIFEXITED(status)) {
    return -1;
  }
  return WEXITSTATUS(status);
}
constexpr int kChildGotIt = 0;
constexpr int kChildBusy = 1;
#endif

} // namespace

TEST(FileLock, TakingAndReleasingReportsZero) {
  string path = lockPath("basic");
  GCU_File_Lock lock;
  ASSERT_EQ(0, gcu_file_lock(&lock, path.c_str(), true, true));
  ASSERT_EQ(0, gcu_file_unlock(&lock));
  remove(path.c_str());
}

TEST(FileLock, TheFileIsCreatedIfAbsent) {
  string path = lockPath("created");
  remove(path.c_str());

  GCU_File_Lock lock;
  ASSERT_EQ(0, gcu_file_lock(&lock, path.c_str(), true, true));
  ASSERT_EQ(0, gcu_file_unlock(&lock));

  FILE * f = fopen(path.c_str(), "rb");
  ASSERT_NE(nullptr, f) << "the lock file was not created";
  fclose(f);
  remove(path.c_str());
}

TEST(FileLock, TheFileSurvivesUnlock) {
  // Documented behaviour, pinned because the tempting "tidy up" is racy:
  // another process can be opening the path between an unlink and its open.
  string path = lockPath("survives");
  GCU_File_Lock lock;
  ASSERT_EQ(0, gcu_file_lock(&lock, path.c_str(), true, true));
  ASSERT_EQ(0, gcu_file_unlock(&lock));

  FILE * f = fopen(path.c_str(), "rb");
  ASSERT_NE(nullptr, f) << "unlock removed the lock file";
  fclose(f);
  remove(path.c_str());
}

TEST(FileLock, UnlockClearsTheLockSoASecondReleaseIsRefused) {
  string path = lockPath("double");
  GCU_File_Lock lock;
  ASSERT_EQ(0, gcu_file_lock(&lock, path.c_str(), true, true));
  ASSERT_EQ(0, gcu_file_unlock(&lock));

  // Reaching into the private member deliberately, because the observable
  // behaviour does not distinguish the cases. Without the clear, the second
  // release still returns -1 -- close() rejects the stale descriptor with
  // EBADF -- so a black-box assertion passes either way, and the mutation of
  // deleting the clear survived until this line was added.
  //
  // The clear is worth pinning rather than shrugging at: between the close
  // and a second release, another thread's open() can be handed the same
  // descriptor number, and then the second close() closes an unrelated file.
  // That is a real defect whose symptom appears somewhere else entirely.
#ifdef _WIN32
  ASSERT_EQ(nullptr, lock.handle) << "unlock left a stale handle";
#else
  ASSERT_EQ(-1, lock.fd) << "unlock left a stale descriptor";
#endif

  ASSERT_EQ(-1, gcu_file_unlock(&lock));
  remove(path.c_str());
}

TEST(FileLock, NullArgumentsAreRejectedNotDereferenced) {
  string path = lockPath("nulls");
  GCU_File_Lock lock;
  ASSERT_EQ(-1, gcu_file_lock(nullptr, path.c_str(), true, true));
  ASSERT_EQ(-1, gcu_file_lock(&lock, nullptr, true, true));
  ASSERT_EQ(-1, gcu_file_unlock(nullptr));
}

TEST(FileLock, AnUnwritablePathFails) {
  GCU_File_Lock lock;
  ASSERT_EQ(-1, gcu_file_lock(&lock,
      "/nonexistent-directory-a8f3/nope.lock", true, true));
}

#ifndef _WIN32

TEST(FileLock, AnExclusiveLockExcludesAnotherProcess) {
  // The property the whole module exists for. Without it every test above
  // still passes, because they only ever lock and unlock in one process.
  string path = lockPath("exclusive");
  GCU_File_Lock lock;
  ASSERT_EQ(0, gcu_file_lock(&lock, path.c_str(), true, true));

  ASSERT_EQ(kChildBusy, childCanLock(path, true))
      << "another process took an exclusive lock that was already held";
  ASSERT_EQ(kChildBusy, childCanLock(path, false))
      << "another process took a shared lock while a writer held it";

  ASSERT_EQ(0, gcu_file_unlock(&lock));
  remove(path.c_str());
}

TEST(FileLock, ReleasingLetsAnotherProcessIn) {
  string path = lockPath("released");
  GCU_File_Lock lock;
  ASSERT_EQ(0, gcu_file_lock(&lock, path.c_str(), true, true));
  ASSERT_EQ(kChildBusy, childCanLock(path, true));
  ASSERT_EQ(0, gcu_file_unlock(&lock));

  ASSERT_EQ(kChildGotIt, childCanLock(path, true))
      << "the lock was not released";
  remove(path.c_str());
}

TEST(FileLock, SharedLocksDoNotExcludeEachOther) {
  // The difference between this and an exclusive lock. A shared lock
  // implemented as an exclusive one passes every other test here.
  string path = lockPath("shared");
  GCU_File_Lock lock;
  ASSERT_EQ(0, gcu_file_lock(&lock, path.c_str(), false, true));

  ASSERT_EQ(kChildGotIt, childCanLock(path, false))
      << "two readers could not hold the lock at once";
  // ...but a writer is still excluded.
  ASSERT_EQ(kChildBusy, childCanLock(path, true))
      << "a writer took the lock while a reader held it";

  ASSERT_EQ(0, gcu_file_unlock(&lock));
  remove(path.c_str());
}

TEST(FileLock, WaitingIsWhatTheWaitFlagControls) {
  // With wait=false the call must report BUSY rather than block. The attempt
  // happens in a child carrying alarm(5), so a call that blocks is killed and
  // reported as a failure here rather than hanging the suite -- see
  // childCanLock().
  string path = lockPath("nowait");
  GCU_File_Lock lock;
  ASSERT_EQ(0, gcu_file_lock(&lock, path.c_str(), true, true));
  ASSERT_EQ(kChildBusy, childCanLock(path, true));
  ASSERT_EQ(0, gcu_file_unlock(&lock));
  remove(path.c_str());
}

TEST(FileLock, ExitingReleasesTheLock) {
  // The property that makes this usable as a "only one of me at a time"
  // guard: a process that dies holding the lock does not wedge the file.
  string path = lockPath("onexit");

  pid_t pid = fork();
  if (pid == 0) {
    GCU_File_Lock lock;
    int rc = gcu_file_lock(&lock, path.c_str(), true, false);
    // Deliberately not unlocked: the exit is what must release it.
    _exit(rc == 0 ? 0 : 1);
  }
  ASSERT_GT(pid, 0);
  int status = 0;
  ASSERT_EQ(pid, waitpid(pid, &status, 0));
  ASSERT_TRUE(WIFEXITED(status));
  ASSERT_EQ(0, WEXITSTATUS(status)) << "the child could not take the lock";

  GCU_File_Lock lock;
  ASSERT_EQ(0, gcu_file_lock(&lock, path.c_str(), true, false))
      << "the lock outlived the process that held it";
  ASSERT_EQ(0, gcu_file_unlock(&lock));
  remove(path.c_str());
}

#endif // !_WIN32

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
