/**
 * Tests for macros.h -- the header every other header in this library
 * includes first, and until now the only one with no test of its own.
 *
 * What is here is what can be checked from inside one process on one
 * compiler.  Two things deliberately are not:
 *
 *   - GCU_CLEANUP_FUNCTION.  Its effect happens after main() returns, so
 *     nothing inside the process can observe it.  The destructor side is
 *     already exercised by src/thread.c, whose post-main free is the offset
 *     documented in documentation/memory.md.
 *   - The non-GCC spellings of GCU_MAYBE_UNUSED, GCU_DEPRECATED, GCU_API and
 *     GCU_API_DATA.  Only the branch this compiler takes is compiled at all,
 *     which is how GCU_MAYBE_UNUSED's MSVC branch stayed a syntax error.
 */

#include <wchar.h>
#include <string>
#include <gtest/gtest.h>
#include <ghoti.io/cutil/macros.h>

// debug.h declares nothing of its own -- it is a placeholder that includes
// macros.h.  Including it here is what keeps it compiling under C++, which is
// the only thing test-debug.cpp ever checked.
#include <ghoti.io/cutil/debug.h>

using namespace std;

#define GCU_TEST_STRINGIFY_(X) #X
#define GCU_TEST_STRINGIFY(X) GCU_TEST_STRINGIFY_(X)

// Set before main() runs if GCU_INIT_FUNCTION does what it says.
static bool init_function_ran = false;
static bool init_function_had_run_at_main = false;

GCU_INIT_FUNCTION(test_macros_constructor) {
  init_function_ran = true;
}

TEST(Macros, WcharWidthMatchesTheType) {
  ASSERT_EQ((size_t)GCU_WCHAR_WIDTH, sizeof(wchar_t));
}

TEST(Macros, WcharSignedMatchesTheType) {
  // The library's answer, and the truth it is supposed to be reporting.
  ASSERT_EQ((bool)GCU_WCHAR_SIGNED, (bool)((wchar_t)-1 < (wchar_t)0));
}

TEST(Macros, WcharSignedAgreesWithItselfInThePreprocessor) {
  // The failure this pins was silent in exactly this direction: GCU_WCHAR_SIGNED
  // expanded to a comparison against a helper that had been #undef'd, so an
  // `#if` saw an undefined identifier, read it as 0, and answered "unsigned"
  // while the expression form did not compile at all.
#if GCU_WCHAR_SIGNED
  bool preprocessor_says = true;
#else
  bool preprocessor_says = false;
#endif
  ASSERT_EQ(preprocessor_says, (bool)GCU_WCHAR_SIGNED);
  ASSERT_EQ(preprocessor_says, (bool)((wchar_t)-1 < (wchar_t)0));
}

TEST(Macros, WcharMacrosExpandToLiterals) {
  // Both are documented as usable in an expression and in an `#if`, which is
  // only true while they expand to a literal.  An expression referring to a
  // helper defined in macros.h would pass the two tests above inside this
  // library -- where the helper may still be live -- and fail in a consumer.
  string signedness = GCU_TEST_STRINGIFY(GCU_WCHAR_SIGNED);
  ASSERT_TRUE(signedness == "0" || signedness == "1") << "was: " << signedness;

  string width = GCU_TEST_STRINGIFY(GCU_WCHAR_WIDTH);
  ASSERT_TRUE(width == "2" || width == "4" || width == "8") << "was: " << width;
}

TEST(Macros, HelperMaximaDoNotEscapeTheHeader) {
  // macros.h #undef's these on the way out so that names this generic do not
  // land on every consumer.  That is also what broke GCU_WCHAR_SIGNED, so the
  // two facts belong next to each other.
#ifdef GHOTI_IO_GCU_MAX_INT64
  FAIL() << "GHOTI_IO_GCU_MAX_INT64 escaped macros.h";
#endif
#ifdef GHOTI_IO_GCU_MAX_INT32
  FAIL() << "GHOTI_IO_GCU_MAX_INT32 escaped macros.h";
#endif
#ifdef GHOTI_IO_GCU_MAX_INT16
  FAIL() << "GHOTI_IO_GCU_MAX_INT16 escaped macros.h";
#endif
#ifdef GHOTI_IO_GCU_MAX_UINT64
  FAIL() << "GHOTI_IO_GCU_MAX_UINT64 escaped macros.h";
#endif
#ifdef GHOTI_IO_GCU_MAX_UINT32
  FAIL() << "GHOTI_IO_GCU_MAX_UINT32 escaped macros.h";
#endif
#ifdef GHOTI_IO_GCU_MAX_UINT16
  FAIL() << "GHOTI_IO_GCU_MAX_UINT16 escaped macros.h";
#endif
  SUCCEED();
}

// GCU_MAYBE_UNUSED wraps a declaration, so the check is that this file still
// compiles: it is built with -Wall -Wextra -Werror, `unread` below is genuinely
// never read, and -Wextra implies -Wunused-parameter.  If the macro stops
// suppressing -- or goes back to expanding to a statement -- this file stops
// building rather than failing at run time.
static int maybe_unused_parameter(GCU_MAYBE_UNUSED(int unread), int used) {
  return used;
}

TEST(Macros, MaybeUnusedWrapsADeclaration) {
  ASSERT_EQ(maybe_unused_parameter(0, 7), 7);

  // The other position it is documented for: a local nothing goes on to read.
  GCU_MAYBE_UNUSED(int unread_local) = 3;
}

TEST(Macros, InitFunctionRunsBeforeMain) {
  ASSERT_TRUE(init_function_ran);
  // Ran, and ran early enough: main() saw it already set, so this is a
  // constructor and not merely a function something happened to call.
  ASSERT_TRUE(init_function_had_run_at_main);
}

int main(int argc, char** argv) {
  init_function_had_run_at_main = init_function_ran;
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
