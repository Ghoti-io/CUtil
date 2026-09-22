#include <cstring>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <ghoti.io/cutil/env.h>

using namespace std;

namespace {

// Distinctive so a stale value from the developer's own shell cannot pass a
// test by coincidence.
const char * kName = "GHOTI_IO_CUTIL_TEST_VARIABLE_9F2C";

string getValue(const char * name) {
  size_t bytes = gcu_env_get(name, nullptr, 0);
  EXPECT_GT(bytes, 0u);
  vector<char> buf(bytes);
  EXPECT_EQ(bytes, gcu_env_get(name, buf.data(), buf.size()));
  return string(buf.data());
}

struct EnvTest : public ::testing::Test {
  void SetUp() override { gcu_env_unset(kName); }
  void TearDown() override { gcu_env_unset(kName); }
};

} // namespace

TEST_F(EnvTest, AnUnsetVariableMeasuresZero) {
  ASSERT_EQ(0u, gcu_env_get(kName, nullptr, 0));
  ASSERT_FALSE(gcu_env_has(kName));
}

TEST_F(EnvTest, SetThenGetRoundTrips) {
  ASSERT_EQ(0, gcu_env_set(kName, "hello"));
  ASSERT_TRUE(gcu_env_has(kName));
  ASSERT_EQ("hello", getValue(kName));
}

TEST_F(EnvTest, SetReplacesRatherThanAppending) {
  ASSERT_EQ(0, gcu_env_set(kName, "first"));
  ASSERT_EQ(0, gcu_env_set(kName, "second"));
  ASSERT_EQ("second", getValue(kName));
}

TEST_F(EnvTest, UnsetRemovesIt) {
  ASSERT_EQ(0, gcu_env_set(kName, "x"));
  ASSERT_TRUE(gcu_env_has(kName));
  ASSERT_EQ(0, gcu_env_unset(kName));
  ASSERT_FALSE(gcu_env_has(kName));
  ASSERT_EQ(0u, gcu_env_get(kName, nullptr, 0));
}

TEST_F(EnvTest, UnsettingSomethingAbsentSucceeds) {
  ASSERT_FALSE(gcu_env_has(kName));
  ASSERT_EQ(0, gcu_env_unset(kName));
}

TEST_F(EnvTest, AnEmptyValueIsSetButMeasuresOne) {
  // The distinction gcu_env_has() exists for: "" is a variable that is set,
  // and gcu_env_get() returning 1 -- just the terminator -- is not the same
  // answer as returning 0.
  ASSERT_EQ(0, gcu_env_set(kName, ""));
  ASSERT_TRUE(gcu_env_has(kName));
  ASSERT_EQ(1u, gcu_env_get(kName, nullptr, 0));
  ASSERT_EQ("", getValue(kName));
}

TEST_F(EnvTest, TooSmallADestinationWritesNothing) {
  ASSERT_EQ(0, gcu_env_set(kName, "abcdefgh"));
  size_t needed = gcu_env_get(kName, nullptr, 0);
  ASSERT_EQ(9u, needed);

  vector<char> guard(needed, '\x7f');
  // Still reports the requirement, but must not leave a partial value: a
  // truncated PATH is the difference between failing and using the wrong
  // directory.
  ASSERT_EQ(needed, gcu_env_get(kName, guard.data(), needed - 1));
  for (size_t i = 0; i < guard.size(); ++i) {
    ASSERT_EQ('\x7f', guard[i]) << "wrote a partial value at index " << i;
  }
}

TEST_F(EnvTest, ExactlyEnoughRoomWorks) {
  ASSERT_EQ(0, gcu_env_set(kName, "abcdefgh"));
  vector<char> exact(9, '\x7f');
  ASSERT_EQ(9u, gcu_env_get(kName, exact.data(), exact.size()));
  ASSERT_STREQ("abcdefgh", exact.data());
}

TEST_F(EnvTest, InvalidNamesAreRejected) {
  // An '=' in a name is the pointed case: interfaces in this family have
  // taken "NAME=VALUE" in one string, so a name carrying an '=' would set a
  // variable the caller did not name.
  //
  // READ THIS BEFORE TRUSTING IT AS COVERAGE. On glibc this test passes with
  // or without env.c's own name check, because the C library rejects all
  // three of these itself with EINVAL -- measured, not assumed. Deleting
  // `strchr(name, '=') == NULL` from name_is_valid() leaves the whole suite
  // green.
  //
  // So this pins the *contract* and not this layer's implementation of it,
  // and the check stays for two reasons the test cannot show here: the
  // contract should not depend on which C library is underneath, and Windows
  // does not reject the same set -- SetEnvironmentVariableW accepts names
  // beginning with '=', which the system itself uses for per-drive working
  // directories. A Windows-side test would isolate it; this one cannot.
  const char * bad[] = { "", "HAS=EQUALS", "=LEADING" };
  for (const char * n : bad) {
    ASSERT_EQ(0u, gcu_env_get(n, nullptr, 0)) << "accepted name: " << n;
    ASSERT_FALSE(gcu_env_has(n)) << "accepted name: " << n;
    ASSERT_EQ(-1, gcu_env_set(n, "v")) << "accepted name: " << n;
    ASSERT_EQ(-1, gcu_env_unset(n)) << "accepted name: " << n;
  }
}

TEST_F(EnvTest, NullArgumentsAreRejectedNotDereferenced) {
  ASSERT_EQ(0u, gcu_env_get(nullptr, nullptr, 0));
  ASSERT_FALSE(gcu_env_has(nullptr));
  ASSERT_EQ(-1, gcu_env_set(nullptr, "v"));
  ASSERT_EQ(-1, gcu_env_unset(nullptr));
  // A NULL value is rejected rather than treated as "unset", so the two
  // intentions cannot be confused at a call site.
  ASSERT_EQ(-1, gcu_env_set(kName, nullptr));
  ASSERT_FALSE(gcu_env_has(kName));
}

TEST_F(EnvTest, NonAsciiValuesSurvive) {
  // On Windows this is the whole point of the module: the ANSI API cannot
  // represent these and would hand back mangled bytes.  On POSIX it confirms
  // nothing in the path re-encodes.
  const char * value = "caf\xC3\xA9/\xE4\xB8\x96/\xF0\x9F\x98\x80";
  ASSERT_EQ(0, gcu_env_set(kName, value));
  ASSERT_EQ(string(value), getValue(kName));
}

TEST_F(EnvTest, AValueWithAnEqualsSignIsFine) {
  // Only the NAME may not contain '='.  A value certainly may -- query
  // strings and base64 both do -- and rejecting it would be over-reach.
  ASSERT_EQ(0, gcu_env_set(kName, "a=b=c=="));
  ASSERT_EQ("a=b=c==", getValue(kName));
}

TEST_F(EnvTest, ALongValueRoundTrips) {
  string value(4096, 'x');
  value += "|end";
  ASSERT_EQ(0, gcu_env_set(kName, value.c_str()));
  ASSERT_EQ(value, getValue(kName));
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
