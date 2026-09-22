#include <cstring>
#include <string>
#include <gtest/gtest.h>
#include <ghoti.io/cutil/library.h>

using namespace std;

#ifndef GCU_TEST_PLUGIN_PATH
#error "GCU_TEST_PLUGIN_PATH must name the test plugin; see the Makefile rule"
#endif
#ifndef GCU_TEST_BROKEN_PLUGIN_PATH
#error "GCU_TEST_BROKEN_PLUGIN_PATH must name the unresolvable plugin"
#endif

namespace {

typedef int (*adder)(int, int);
typedef const char * (*namer)(void);

struct LibraryTest : public ::testing::Test {
  GCU_Library lib{};
  void SetUp() override {
    ASSERT_EQ(0, gcu_library_open(&lib, GCU_TEST_PLUGIN_PATH))
        << "could not open " << GCU_TEST_PLUGIN_PATH;
  }
  void TearDown() override {
    if (lib) {
      EXPECT_EQ(0, gcu_library_close(&lib));
    }
  }
};

} // namespace

TEST_F(LibraryTest, OpenSucceedsAndClosesCleanly) {
  ASSERT_NE(nullptr, lib);
}

TEST_F(LibraryTest, ASymbolCanBeFoundAndCalled) {
  // Finding the symbol is half of it; calling it is what shows the pointer is
  // the function and not merely non-NULL.
  adder add = reinterpret_cast<adder>(gcu_library_symbol(lib,
      "gcu_test_plugin_add"));
  ASSERT_NE(nullptr, add);
  ASSERT_EQ(7, add(3, 4));
  ASSERT_EQ(-1, add(2, -3));
}

TEST_F(LibraryTest, ASecondSymbolIsADifferentFunction) {
  // Guards against an implementation that returns the same address for every
  // name, which would pass the test above.
  adder add = reinterpret_cast<adder>(gcu_library_symbol(lib,
      "gcu_test_plugin_add"));
  namer name = reinterpret_cast<namer>(gcu_library_symbol(lib,
      "gcu_test_plugin_name"));
  ASSERT_NE(nullptr, add);
  ASSERT_NE(nullptr, name);
  ASSERT_NE(reinterpret_cast<void *>(add), reinterpret_cast<void *>(name));
  ASSERT_STREQ("ghoti-test-plugin", name());
}

TEST_F(LibraryTest, AMissingSymbolIsNullAndReportsWhy) {
  ASSERT_EQ(nullptr, gcu_library_symbol(lib, "gcu_test_plugin_absent"));

  char message[512];
  ASSERT_EQ(0, gcu_library_error(message, sizeof(message)));
  ASSERT_GT(strlen(message), 0u);
}

TEST_F(LibraryTest, ReadingTheErrorClearsIt) {
  // dlerror() clears on read and this API matches it, so a caller checking
  // twice does not see a stale message attributed to a call that succeeded.
  ASSERT_EQ(nullptr, gcu_library_symbol(lib, "gcu_test_plugin_absent"));

  char first[512];
  ASSERT_EQ(0, gcu_library_error(first, sizeof(first)));

  char second[512];
  ASSERT_EQ(-1, gcu_library_error(second, sizeof(second)));
  ASSERT_STREQ("", second) << "stale message left after a read";
}

TEST_F(LibraryTest, ASuccessfulLookupLeavesNoError) {
  // The failure this guards: entry points that do not clear dlerror() first
  // let a message from an earlier failed call be reported against a later
  // successful one.
  //
  // The earlier error is deliberately NOT read here, so that the stale
  // message is still outstanding when the successful lookup runs.
  //
  // READ THIS BEFORE TRUSTING IT AS COVERAGE. This still passes with the
  // dlerror() call removed from gcu_library_symbol(), because glibc's dlsym
  // clears the error itself on success -- measured directly, not assumed.
  // POSIX does not require that, so the explicit clear stays; its effect is
  // simply not observable on this platform, and no test here can isolate it.
  // What this test does pin is the contract: a successful lookup reports no
  // error, however the implementation arranges it.
  ASSERT_EQ(nullptr, gcu_library_symbol(lib, "gcu_test_plugin_absent"));

  ASSERT_NE(nullptr, gcu_library_symbol(lib, "gcu_test_plugin_add"));
  char message[512];
  ASSERT_EQ(-1, gcu_library_error(message, sizeof(message)))
      << "reported an error against a lookup that succeeded: " << message;
}

TEST(Library, AnUnresolvableSymbolIsRefusedAtOpenTime) {
  // RTLD_NOW, not RTLD_LAZY. A library with a symbol nothing can resolve must
  // fail here, where the caller is checking a return value -- not succeed and
  // crash at the first call, somewhere else, with no failing call to blame.
  //
  // This needs its own plugin: the ordinary one resolves completely, so lazy
  // and now behave identically against it and the choice is untestable.
  GCU_Library lib{};
  ASSERT_EQ(-1, gcu_library_open(&lib, GCU_TEST_BROKEN_PLUGIN_PATH))
      << "opened a library with an unresolvable symbol";

  char message[512];
  ASSERT_EQ(0, gcu_library_error(message, sizeof(message)));
  ASSERT_GT(strlen(message), 0u);
}

TEST(Library, OpeningSomethingAbsentFails) {
  GCU_Library lib{};
  ASSERT_EQ(-1, gcu_library_open(&lib, "/nonexistent-a8f3/libnothing.so"));

  char message[512];
  ASSERT_EQ(0, gcu_library_error(message, sizeof(message)));
  ASSERT_GT(strlen(message), 0u);
}

TEST(Library, OpeningSomethingThatIsNotALibraryFails) {
  GCU_Library lib{};
  // A real file that is not a loadable object.
  ASSERT_EQ(-1, gcu_library_open(&lib, "Makefile"));
}

TEST(Library, NullArgumentsAreRejectedNotDereferenced) {
  GCU_Library lib{};
  ASSERT_EQ(-1, gcu_library_open(nullptr, GCU_TEST_PLUGIN_PATH));
  ASSERT_EQ(-1, gcu_library_open(&lib, nullptr));
  ASSERT_EQ(-1, gcu_library_close(nullptr));
  // Closing a handle that was never opened must not call the loader with it.
  GCU_Library empty{};
  ASSERT_EQ(-1, gcu_library_close(&empty));
  ASSERT_EQ(nullptr, gcu_library_symbol(nullptr, "gcu_test_plugin_add"));

  ASSERT_EQ(0, gcu_library_open(&lib, GCU_TEST_PLUGIN_PATH));
  ASSERT_EQ(nullptr, gcu_library_symbol(lib, nullptr));
  ASSERT_EQ(0, gcu_library_close(&lib));
}

TEST(Library, CloseClearsTheHandle) {
  // So that a double close is caught by the NULL check rather than handed to
  // the loader, where it is undefined.
  GCU_Library lib{};
  ASSERT_EQ(0, gcu_library_open(&lib, GCU_TEST_PLUGIN_PATH));
  ASSERT_EQ(0, gcu_library_close(&lib));
  ASSERT_EQ(nullptr, lib);
  ASSERT_EQ(-1, gcu_library_close(&lib));
}

TEST(Library, ErrorFitsASmallBufferWithoutOverrunningIt) {
  GCU_Library lib{};
  ASSERT_EQ(-1, gcu_library_open(&lib, "/nonexistent-a8f3/libnothing.so"));

  struct { char before; char buf[8]; char after; } guarded;
  guarded.before = '\x7f';
  guarded.after = '\x7f';
  ASSERT_EQ(0, gcu_library_error(guarded.buf, sizeof(guarded.buf)));
  ASSERT_LT(strlen(guarded.buf), sizeof(guarded.buf));
  ASSERT_EQ('\x7f', guarded.before);
  ASSERT_EQ('\x7f', guarded.after);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
