/**
 * @file
 * Tests for subprocess.h.
 *
 * Every test runs under hang-guard.h, because the failures this module is
 * most likely to have are hangs: a pipe that nobody drains, a wait that no
 * clock bounds.  A hang stops the whole suite with no failing test to point
 * at and gets blamed on the machine.
 */

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <ghoti.io/cutil/subprocess.h>
#include "hang-guard.h"

#ifndef _WIN32
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace std;

#ifndef GCU_TEST_SUBPROCESS_DIR
#error "GCU_TEST_SUBPROCESS_DIR must name a writable directory; see the Makefile"
#endif

namespace {

// The longest test here waits ten seconds on purpose, so the guard is well
// clear of that.  See hang-guard.h.
using Subprocess = ghoti_test::HangGuarded<60>;

/// A NULL-terminated argv built from a list, for readability at the call site.
struct Argv {
  vector<const char *> slots;
  Argv(initializer_list<const char *> words) {
    for (const char * word : words) {
      slots.push_back(word);
    }
    slots.push_back(nullptr);
  }
  const char * const * operator()() const { return slots.data(); }
};

/// Run a shell script, which is the easiest program to ask for a specific
/// behaviour.  Note that the *library* never involves a shell -- this is one
/// program among others, named explicitly, which is exactly what subprocess.h
/// says to do when shell syntax is what you want.
int runScript(const string & script, GCU_Subprocess_Result * result,
    GCU_Subprocess_Options options = {}) {
  Argv argv{"sh", "-c", script.c_str()};
  options.argv = argv();
  return gcu_subprocess_run(&options, result);
}

string outputOf(const GCU_Subprocess_Result & result) {
  return result.out ? string(result.out, result.out_size) : string();
}

string errorOf(const GCU_Subprocess_Result & result) {
  return result.err ? string(result.err, result.err_size) : string();
}

} // namespace

TEST_F(Subprocess, ReportsTheExitCode) {
  GCU_Subprocess_Result result;
  ASSERT_EQ(0, runScript("exit 7", &result));
  EXPECT_EQ(GCU_SUBPROCESS_EXITED, result.outcome);
  EXPECT_EQ(7, result.exit_code);
  EXPECT_EQ(0, result.signal);
  gcu_subprocess_result_free(&result);
}

TEST_F(Subprocess, CapturesStandardOutput) {
  GCU_Subprocess_Result result;
  ASSERT_EQ(0, runScript("printf 'hello world'", &result));
  EXPECT_EQ("hello world", outputOf(result));
  EXPECT_EQ("", errorOf(result));
  gcu_subprocess_result_free(&result);
}

TEST_F(Subprocess, KeepsTheTwoStreamsApart) {
  GCU_Subprocess_Result result;
  ASSERT_EQ(0, runScript("printf out; printf err >&2", &result));
  EXPECT_EQ("out", outputOf(result));
  EXPECT_EQ("err", errorOf(result));
  gcu_subprocess_result_free(&result);
}

TEST_F(Subprocess, MergeStderrPutsBothInTheOutput) {
  GCU_Subprocess_Options options = {};
  options.merge_stderr = true;
  GCU_Subprocess_Result result;
  ASSERT_EQ(0, runScript("printf err >&2", &result, options));
  EXPECT_EQ("err", outputOf(result));
  EXPECT_EQ("", errorOf(result))
      << "merge_stderr promises err stays empty, not that it duplicates";
  gcu_subprocess_result_free(&result);
}

TEST_F(Subprocess, LooksTheProgramUpOnPath) {
  // "sh" with no separator is a PATH lookup; "/bin/sh" is not.  Both should
  // work, and the test says so rather than leaving the distinction implied by
  // whichever spelling the other tests happen to use.
  GCU_Subprocess_Result byPath;
  Argv relative{"sh", "-c", "exit 4"};
  GCU_Subprocess_Options options = {};
  options.argv = relative();
  ASSERT_EQ(0, gcu_subprocess_run(&options, &byPath));
  EXPECT_EQ(4, byPath.exit_code);
  gcu_subprocess_result_free(&byPath);

  GCU_Subprocess_Result byName;
  Argv absolute{"/bin/sh", "-c", "exit 4"};
  options.argv = absolute();
  ASSERT_EQ(0, gcu_subprocess_run(&options, &byName));
  EXPECT_EQ(4, byName.exit_code);
  gcu_subprocess_result_free(&byName);
}

TEST_F(Subprocess, SendsInputToTheChild) {
  string payload = "the quick brown fox";
  GCU_Subprocess_Options options = {};
  options.input = payload.c_str();
  options.input_size = payload.size();
  GCU_Subprocess_Result result;
  ASSERT_EQ(0, runScript("cat", &result, options));
  EXPECT_EQ(payload, outputOf(result));
  gcu_subprocess_result_free(&result);
}

TEST_F(Subprocess, TheChildSeesEndOfFileWithNoInput) {
  // Not the same as "no input was sent": a child whose stdin stays open
  // blocks in `cat` forever, and the difference only shows on a child that
  // reads.
  GCU_Subprocess_Result result;
  ASSERT_EQ(0, runScript("cat; echo done", &result));
  EXPECT_EQ("done\n", outputOf(result));
  gcu_subprocess_result_free(&result);
}

TEST_F(Subprocess, MoreThanAPipeWillHoldOnBothStreamsAtOnce) {
  // The deadlock this module exists to prevent.  The child fills stderr
  // *first*, and a pipe holds 64 KiB on Linux, so an implementation that
  // drains stdout to end-of-file and only then reads stderr waits for a
  // stdout that the child cannot reach.  Every test above would still pass.
  //
  // The timeout is what turns that into a failure rather than a hang: the
  // same loop enforces it, so a wedged loop reports GCU_SUBPROCESS_TIMED_OUT
  // and this test fails in ten seconds with a name attached.
  const size_t bulk = 1000000;
  GCU_Subprocess_Options options = {};
  options.timeout = 10000;
  GCU_Subprocess_Result result;
  ASSERT_EQ(0, runScript(
      "head -c 1000000 /dev/zero | tr '\\0' 'e' >&2; "
      "head -c 1000000 /dev/zero | tr '\\0' 'o'",
      &result, options));
  EXPECT_EQ(GCU_SUBPROCESS_EXITED, result.outcome);
  EXPECT_EQ(bulk, result.out_size);
  EXPECT_EQ(bulk, result.err_size);
  EXPECT_EQ(string(bulk, 'o'), outputOf(result));
  EXPECT_EQ(string(bulk, 'e'), errorOf(result));
  gcu_subprocess_result_free(&result);
}

TEST_F(Subprocess, MoreInputThanAPipeWillHold) {
  // The same question from the other side: the parent cannot write two
  // megabytes into a 64 KiB pipe in one go, so it has to keep reading the
  // child's output while it writes.
  string payload(2000000, 'z');
  GCU_Subprocess_Options options = {};
  options.input = payload.c_str();
  options.input_size = payload.size();
  options.timeout = 10000;
  GCU_Subprocess_Result result;
  ASSERT_EQ(0, runScript("cat", &result, options));
  EXPECT_EQ(GCU_SUBPROCESS_EXITED, result.outcome);
  EXPECT_EQ(payload.size(), result.out_size);
  gcu_subprocess_result_free(&result);
}

TEST_F(Subprocess, AMissingProgramIsNotAnExitCode) {
  // The reason GCU_SUBPROCESS_NOT_STARTED exists.  A shell reports "not
  // found" as 127 and a program is free to exit 127 on purpose; from the
  // number alone the caller cannot tell a wrong path from an answer.
  GCU_Subprocess_Result chose127;
  ASSERT_EQ(0, runScript("exit 127", &chose127));
  EXPECT_EQ(GCU_SUBPROCESS_EXITED, chose127.outcome);
  EXPECT_EQ(127, chose127.exit_code);
  gcu_subprocess_result_free(&chose127);

  Argv missing{"ghoti-io-no-such-program-exists", "--help"};
  GCU_Subprocess_Options options = {};
  options.argv = missing();
  GCU_Subprocess_Result notRun;
  EXPECT_EQ(GCU_SUBPROCESS_NOT_STARTED, gcu_subprocess_run(&options, &notRun));
#ifndef _WIN32
  EXPECT_EQ(ENOENT, errno) << "the reason should survive in the error slot";
#endif
}

TEST_F(Subprocess, AFileThatIsNotExecutableIsNotStarted) {
  string path = string(GCU_TEST_SUBPROCESS_DIR) + "/not-executable";
  FILE * file = fopen(path.c_str(), "w");
  ASSERT_NE(nullptr, file);
  fputs("#!/bin/sh\necho hi\n", file);
  fclose(file);
#ifndef _WIN32
  ASSERT_EQ(0, chmod(path.c_str(), 0600));
#endif

  Argv argv{path.c_str()};
  GCU_Subprocess_Options options = {};
  options.argv = argv();
  GCU_Subprocess_Result result;
  EXPECT_EQ(GCU_SUBPROCESS_NOT_STARTED, gcu_subprocess_run(&options, &result));
#ifndef _WIN32
  EXPECT_EQ(EACCES, errno);
#endif
  remove(path.c_str());
}

TEST_F(Subprocess, RunsInTheDirectoryItIsGiven) {
  string marker = string(GCU_TEST_SUBPROCESS_DIR) + "/where-am-i";
  FILE * file = fopen(marker.c_str(), "w");
  ASSERT_NE(nullptr, file);
  fputs("found", file);
  fclose(file);

  GCU_Subprocess_Options options = {};
  options.directory = GCU_TEST_SUBPROCESS_DIR;
  GCU_Subprocess_Result result;
  ASSERT_EQ(0, runScript("cat where-am-i", &result, options));
  EXPECT_EQ("found", outputOf(result));
  gcu_subprocess_result_free(&result);
  remove(marker.c_str());
}

TEST_F(Subprocess, ADirectoryThatCannotBeEnteredIsNotStarted) {
  // Not "runs somewhere unexpected", which is what a chdir whose failure is
  // ignored would give, and which no output could distinguish from a correct
  // run against a stale file.
  GCU_Subprocess_Options options = {};
  options.directory = "/ghoti-io/no/such/directory";
  Argv argv{"sh", "-c", "pwd"};
  options.argv = argv();
  GCU_Subprocess_Result result;
  EXPECT_EQ(GCU_SUBPROCESS_NOT_STARTED, gcu_subprocess_run(&options, &result));
#ifndef _WIN32
  EXPECT_EQ(ENOENT, errno);
#endif
}

TEST_F(Subprocess, ReplacesTheEnvironmentWhenGiven) {
  const char * entries[] = {
    "GHOTI_TEST_MARKER=present",
    "PATH=/bin:/usr/bin",
    nullptr,
  };
  ASSERT_EQ(0, setenv("GHOTI_TEST_INHERITED", "parent", 1));
  GCU_Subprocess_Options options = {};
  options.environment = entries;
  GCU_Subprocess_Result result;
  ASSERT_EQ(0, runScript(
      "printf '%s/%s' \"$GHOTI_TEST_MARKER\" \"$GHOTI_TEST_INHERITED\"",
      &result, options));
  EXPECT_EQ("present/", outputOf(result))
      << "the environment replaces, it does not add to";
  gcu_subprocess_result_free(&result);
  unsetenv("GHOTI_TEST_INHERITED");
}

TEST_F(Subprocess, InheritsTheEnvironmentWhenNotGiven) {
  ASSERT_EQ(0, setenv("GHOTI_TEST_INHERITED", "parent", 1));
  GCU_Subprocess_Result result;
  ASSERT_EQ(0, runScript("printf '%s' \"$GHOTI_TEST_INHERITED\"", &result));
  EXPECT_EQ("parent", outputOf(result));
  gcu_subprocess_result_free(&result);
  unsetenv("GHOTI_TEST_INHERITED");
}

TEST_F(Subprocess, TheTimeoutKillsAChildThatWillNotFinish) {
  GCU_Subprocess_Options options = {};
  options.timeout = 300;
  GCU_Subprocess_Result result;
  auto started = chrono::steady_clock::now();
  ASSERT_EQ(0, runScript("sleep 30", &result, options));
  auto elapsed = chrono::duration_cast<chrono::milliseconds>(
      chrono::steady_clock::now() - started).count();
  EXPECT_EQ(GCU_SUBPROCESS_TIMED_OUT, result.outcome);
  EXPECT_LT(elapsed, 5000) << "the timeout did not bound the call";
  gcu_subprocess_result_free(&result);
}

TEST_F(Subprocess, TheTimeoutCoversTheWaitAndNotOnlyTheReading) {
  // The child closes both streams and then keeps running.  The reading loop
  // ends immediately at end-of-file, so a timeout enforced only there is no
  // bound at all: the call would block in the wait for thirty seconds with a
  // 300 ms deadline set.
  GCU_Subprocess_Options options = {};
  options.timeout = 300;
  GCU_Subprocess_Result result;
  auto started = chrono::steady_clock::now();
  ASSERT_EQ(0, runScript("exec >&- 2>&-; sleep 30", &result, options));
  auto elapsed = chrono::duration_cast<chrono::milliseconds>(
      chrono::steady_clock::now() - started).count();
  EXPECT_EQ(GCU_SUBPROCESS_TIMED_OUT, result.outcome);
  EXPECT_LT(elapsed, 5000);
  gcu_subprocess_result_free(&result);
}

TEST_F(Subprocess, TheOutputLimitStopsAChildThatNeverStops) {
  GCU_Subprocess_Options options = {};
  options.output_limit = 64 * 1024;
  GCU_Subprocess_Result result;
  ASSERT_EQ(0, runScript("yes ghoti", &result, options));
  EXPECT_EQ(GCU_SUBPROCESS_OUTPUT_LIMIT, result.outcome);
  EXPECT_GE(result.out_size, options.output_limit);
  EXPECT_LT(result.out_size, options.output_limit + 1024 * 1024)
      << "the limit stopped it, but not near where it was asked to";
  gcu_subprocess_result_free(&result);
}

TEST_F(Subprocess, NoOutputLimitMeansNoLimitAndNotADefault) {
  // Zero is the unconstrained value.  A built-in default hiding behind it
  // would truncate this for a reason nothing at the call site mentions.
  GCU_Subprocess_Options options = {};
  options.output_limit = 0;
  options.timeout = 10000;
  GCU_Subprocess_Result result;
  ASSERT_EQ(0, runScript("head -c 3000000 /dev/zero | tr '\\0' 'q'",
      &result, options));
  EXPECT_EQ(GCU_SUBPROCESS_EXITED, result.outcome);
  EXPECT_EQ(3000000u, result.out_size);
  gcu_subprocess_result_free(&result);
}

TEST_F(Subprocess, ASignalIsReportedAsSomethingOtherThanAnExitCode) {
#ifndef _WIN32
  GCU_Subprocess_Result result;
  ASSERT_EQ(0, runScript("kill -TERM $$", &result));
  EXPECT_EQ(GCU_SUBPROCESS_SIGNALED, result.outcome);
  EXPECT_EQ(SIGTERM, result.signal);
  gcu_subprocess_result_free(&result);
#endif
}

#ifndef _WIN32
TEST_F(Subprocess, AChildThatIgnoresItsInputDoesNotTakeUsWithIt) {
  // Writing to a pipe whose reader has gone raises SIGPIPE, which by default
  // kills the process doing the writing -- so without the mask this test does
  // not fail, it ends the binary.  The child exits without reading a byte of
  // the four megabytes queued for it.
  string payload(4000000, 'x');
  GCU_Subprocess_Options options = {};
  options.input = payload.c_str();
  options.input_size = payload.size();
  options.timeout = 10000;
  GCU_Subprocess_Result result;
  ASSERT_EQ(0, runScript("exit 3", &result, options));
  EXPECT_EQ(GCU_SUBPROCESS_EXITED, result.outcome);
  EXPECT_EQ(3, result.exit_code);
  gcu_subprocess_result_free(&result);
}

TEST_F(Subprocess, TheSignalMaskIsPutBackAfterwards) {
  // Blocking SIGPIPE is this library's business for the length of one call.
  // Leaving it blocked, or leaving one queued to be delivered the moment the
  // caller unblocks it, would be a change to the caller's process that
  // outlives the call -- and the run above cannot tell the two apart.
  sigset_t before;
  sigemptyset(&before);
  ASSERT_EQ(0, pthread_sigmask(SIG_SETMASK, nullptr, &before));
  ASSERT_FALSE(sigismember(&before, SIGPIPE));

  string payload(4000000, 'x');
  GCU_Subprocess_Options options = {};
  options.input = payload.c_str();
  options.input_size = payload.size();
  options.timeout = 10000;
  GCU_Subprocess_Result result;
  ASSERT_EQ(0, runScript("exit 0", &result, options));
  gcu_subprocess_result_free(&result);

  sigset_t after;
  sigemptyset(&after);
  EXPECT_EQ(0, pthread_sigmask(SIG_SETMASK, nullptr, &after));
  EXPECT_FALSE(sigismember(&after, SIGPIPE)) << "left blocked";

  sigset_t queued;
  sigemptyset(&queued);
  EXPECT_EQ(0, sigpending(&queued));
  EXPECT_FALSE(sigismember(&queued, SIGPIPE)) << "left pending";
}
#endif

#ifdef __linux__
TEST_F(Subprocess, TheChildDoesNotInheritOurOpenDescriptors) {
  // An inherited descriptor keeps a file, a lock or a socket open for as long
  // as the child lives, and a child that outlives the parent holds them after
  // the parent has gone.  Checked through /proc, so this test is Linux-only
  // by construction rather than skipped at run time.
  string path = string(GCU_TEST_SUBPROCESS_DIR) + "/inherited";
  FILE * file = fopen(path.c_str(), "w");
  ASSERT_NE(nullptr, file);
  fclose(file);

  int descriptor = open(path.c_str(), O_RDONLY);
  ASSERT_GE(descriptor, 3);
  // Deliberately not close-on-exec: that is the caller's descriptor, in the
  // state a caller would normally leave it.
  ASSERT_EQ(0, fcntl(descriptor, F_SETFD, 0));

  GCU_Subprocess_Result result;
  string script = "if [ -e /proc/self/fd/" + to_string(descriptor)
      + " ]; then printf inherited; else printf closed; fi";
  ASSERT_EQ(0, runScript(script, &result));
  EXPECT_EQ("closed", outputOf(result));
  gcu_subprocess_result_free(&result);

  close(descriptor);
  remove(path.c_str());
}
#endif

TEST_F(Subprocess, CapturedOutputIsTerminatedAndMayStillContainNul) {
  GCU_Subprocess_Result result;
  ASSERT_EQ(0, runScript("printf 'a\\0b'", &result));
  ASSERT_EQ(3u, result.out_size);
  EXPECT_EQ(0, memcmp(result.out, "a\0b", 3));
  EXPECT_EQ('\0', result.out[result.out_size])
      << "the terminator is past the size, not instead of it";
  gcu_subprocess_result_free(&result);
}

TEST_F(Subprocess, AChildThatWritesNothingLeavesTheBuffersNull) {
  GCU_Subprocess_Result result;
  ASSERT_EQ(0, runScript("exit 0", &result));
  EXPECT_EQ(nullptr, result.out);
  EXPECT_EQ(0u, result.out_size);
  EXPECT_EQ(nullptr, result.err);
  EXPECT_EQ(0u, result.err_size);
  gcu_subprocess_result_free(&result);
}

TEST_F(Subprocess, RefusesArgumentsItCannotUse) {
  GCU_Subprocess_Result result;
  Argv argv{"sh", "-c", "exit 0"};
  GCU_Subprocess_Options options = {};
  options.argv = argv();

  EXPECT_EQ(-1, gcu_subprocess_run(&options, nullptr));
  EXPECT_EQ(-1, gcu_subprocess_run(nullptr, &result));

  GCU_Subprocess_Options noArgv = {};
  EXPECT_EQ(-1, gcu_subprocess_run(&noArgv, &result));

  const char * empty[] = {nullptr};
  GCU_Subprocess_Options emptyArgv = {};
  emptyArgv.argv = empty;
  EXPECT_EQ(-1, gcu_subprocess_run(&emptyArgv, &result));
}

TEST_F(Subprocess, TheResultIsClearedEvenWhenTheRunFails) {
  GCU_Subprocess_Result result;
  ASSERT_EQ(0, runScript("printf leftover", &result));
  ASSERT_NE(nullptr, result.out);
  gcu_subprocess_result_free(&result);

  // A caller who reuses the struct must not be handed the previous run's
  // pointer to free a second time.
  Argv missing{"ghoti-io-no-such-program-exists"};
  GCU_Subprocess_Options options = {};
  options.argv = missing();
  EXPECT_EQ(GCU_SUBPROCESS_NOT_STARTED, gcu_subprocess_run(&options, &result));
  EXPECT_EQ(nullptr, result.out);
  EXPECT_EQ(nullptr, result.err);
}

TEST_F(Subprocess, FreeingIsSafeOnNothingAndSafeTwice) {
  gcu_subprocess_result_free(nullptr);
  GCU_Subprocess_Result zeroed = {};
  gcu_subprocess_result_free(&zeroed);

  GCU_Subprocess_Result result;
  ASSERT_EQ(0, runScript("printf x", &result));
  gcu_subprocess_result_free(&result);
  gcu_subprocess_result_free(&result);
  SUCCEED();
}

TEST_F(Subprocess, ArgumentsArePassedThroughWithoutAShellTouchingThem) {
  // The property that makes this safe to hand user input: a space, a quote, a
  // semicolon and a glob are all just bytes in one argument.
  string hostile = "a b; rm -rf /* \"quoted\" $HOME";
  Argv argv{"sh", "-c", "printf '%s' \"$1\"", "sh", hostile.c_str()};
  GCU_Subprocess_Options options = {};
  options.argv = argv();
  GCU_Subprocess_Result result;
  ASSERT_EQ(0, gcu_subprocess_run(&options, &result));
  EXPECT_EQ(hostile, outputOf(result));
  gcu_subprocess_result_free(&result);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
