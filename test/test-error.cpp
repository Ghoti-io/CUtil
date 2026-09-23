#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <gtest/gtest.h>
#include <ghoti.io/cutil/error.h>

using namespace std;

TEST(Error, LastReportsTheCodeAFailedCallSet) {
  errno = 0;
  // A call that must fail, with a code both platforms have a name for.
  FILE * f = fopen("/nonexistent-directory-a8f3/nonexistent-file", "rb");
  int code = gcu_error_last();
  ASSERT_EQ(nullptr, f);
  ASSERT_NE(0, code);
}

TEST(Error, StringProducesANonEmptyMessage) {
  char buffer[GCU_ERROR_STRING_MAX];
  ASSERT_EQ(0, gcu_error_string(ENOENT, buffer, sizeof(buffer)));
  ASSERT_GT(strlen(buffer), 0u);
}

TEST(Error, DifferentCodesProduceDifferentMessages) {
  // Guards against an implementation that "succeeds" by writing a constant.
  char a[GCU_ERROR_STRING_MAX];
  char b[GCU_ERROR_STRING_MAX];
  ASSERT_EQ(0, gcu_error_string(ENOENT, a, sizeof(a)));
  ASSERT_EQ(0, gcu_error_string(EACCES, b, sizeof(b)));
  ASSERT_STRNE(a, b);
}

TEST(Error, StringRejectsABadDestination) {
  char buffer[GCU_ERROR_STRING_MAX];
  ASSERT_EQ(-1, gcu_error_string(ENOENT, nullptr, sizeof(buffer)));
  ASSERT_EQ(-1, gcu_error_string(ENOENT, buffer, 0));
  ASSERT_EQ(-1, gcu_error_string_last(nullptr, 16));
  ASSERT_EQ(-1, gcu_error_string_last(buffer, 0));
}

TEST(Error, FailureEmptiesTheBufferRatherThanLeavingItStale) {
  // A caller who ignores the return value must print nothing, not the
  // previous message.  This is the difference between a silent bug and a
  // misleading one.
  char buffer[GCU_ERROR_STRING_MAX];
  ASSERT_EQ(0, gcu_error_string(ENOENT, buffer, sizeof(buffer)));
  ASSERT_GT(strlen(buffer), 0u);

  // An implausible code.  If the platform describes it anyway that is fine --
  // then the call succeeded and the buffer is legitimately non-empty.
  if (gcu_error_string(0x6FFFFFFF, buffer, sizeof(buffer)) != 0) {
    ASSERT_STREQ("", buffer) << "stale message left after a failure";
  }
}

TEST(Error, SmallBufferTruncatesAndStaysTerminated) {
  char full[GCU_ERROR_STRING_MAX];
  ASSERT_EQ(0, gcu_error_string(ENOENT, full, sizeof(full)));
  ASSERT_GT(strlen(full), 4u) << "need a message long enough to truncate";

  // A one-past-the-end write here is what ASan is in the suite to catch, so
  // the buffer is sized exactly and bracketed by canaries.
  struct { char before; char buf[5]; char after; } guarded;
  guarded.before = '\x7f';
  guarded.after = '\x7f';

  ASSERT_EQ(0, gcu_error_string(ENOENT, guarded.buf, sizeof(guarded.buf)));
  ASSERT_LT(strlen(guarded.buf), sizeof(guarded.buf))
      << "not terminated within the buffer";
  ASSERT_EQ('\x7f', guarded.before);
  ASSERT_EQ('\x7f', guarded.after);
}

TEST(Error, ExactlyOneByteHoldsOnlyTheTerminator) {
  char one[1] = { 'x' };
  // Either it reports success with an empty string, or it reports failure.
  // What it must not do is write a character and no terminator.
  int rc = gcu_error_string(ENOENT, one, sizeof(one));
  ASSERT_EQ('\0', one[0]) << "wrote a byte into a buffer with no room";
  ASSERT_TRUE(rc == 0 || rc == -1);
}

TEST(Error, StringLastAgreesWithStringOfLast) {
  errno = 0;
  FILE * f = fopen("/nonexistent-directory-a8f3/nonexistent-file", "rb");
  ASSERT_EQ(nullptr, f);
  // Read back rather than assumed: POSIX says ENOENT, but Windows tells a
  // missing directory (ERROR_PATH_NOT_FOUND) from a missing file
  // (ERROR_FILE_NOT_FOUND), and this path has neither.
  int last = gcu_error_last();

  char viaLast[GCU_ERROR_STRING_MAX];
  ASSERT_EQ(0, gcu_error_string_last(viaLast, sizeof(viaLast)));

  char viaCode[GCU_ERROR_STRING_MAX];
  ASSERT_EQ(0, gcu_error_string(last, viaCode, sizeof(viaCode)));
  ASSERT_STREQ(viaCode, viaLast);
}

TEST(Error, MessageHasNoTrailingNewlineOrPeriod) {
  // Windows FormatMessage appends ".\r\n", which is wrong inside a sentence
  // and wrong in a log line.  Asserted on both platforms so the POSIX build
  // pins the shape the Windows build has to produce.
  char buffer[GCU_ERROR_STRING_MAX];
  ASSERT_EQ(0, gcu_error_string(ENOENT, buffer, sizeof(buffer)));
  size_t n = strlen(buffer);
  ASSERT_GT(n, 0u);
  ASSERT_NE('\n', buffer[n - 1]);
  ASSERT_NE('\r', buffer[n - 1]);
  ASSERT_NE('.', buffer[n - 1]);
  ASSERT_NE(' ', buffer[n - 1]);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
