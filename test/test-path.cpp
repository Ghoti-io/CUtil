/**
 * @file
 *
 * Tests for the path module.
 *
 * The Windows cases run here, on Linux.  That is the reason
 * ::GCU_Path_Flavor is a parameter instead of a compile-time branch: without
 * it the Windows half of this module would be code that nobody could execute
 * until somebody found a Windows machine, which is how the rest of the suite
 * ends up with a WINDOWS-TODO.md entry.  Only the environment tests at the
 * bottom are host-specific.
 *
 * Copyright 2026 by Corey Pennycuff
 */

#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <ghoti.io/cutil/path.h>

using namespace std;

namespace {

/** Call a measure/write pair and return what was written. */
template <typename Fn>
string run(Fn fn, GCU_Path_Result * result_out = nullptr) {
  size_t needed = 0;
  GCU_Path_Result measured = fn(nullptr, (size_t)0, &needed);
  if (measured != GCU_PATH_OK) {
    if (result_out) {
      *result_out = measured;
    }
    return string("<") + gcu_path_result_string(measured) + ">";
  }

  // One byte more than required, pre-filled, so that a writer that runs past
  // its length is caught rather than merely tolerated.
  vector<char> buffer(needed + 2, '\x7f');
  GCU_Path_Result wrote = fn(buffer.data(), needed + 1, nullptr);
  if (result_out) {
    *result_out = wrote;
  }
  if (wrote != GCU_PATH_OK) {
    return string("<") + gcu_path_result_string(wrote) + ">";
  }
  EXPECT_EQ('\x7f', buffer[needed + 1])
      << "wrote past the length it reported";
  string written(buffer.data());
  EXPECT_EQ(needed, written.size())
      << "measured length disagrees with what was written";
  return written;
}

string normalize(GCU_Path_Flavor flavor, const char * path) {
  return run([&](char * o, size_t n, size_t * l) {
    return gcu_path_normalize(flavor, path, o, n, l);
  });
}

string dirname(GCU_Path_Flavor flavor, const char * path) {
  return run([&](char * o, size_t n, size_t * l) {
    return gcu_path_dirname(flavor, path, o, n, l);
  });
}

string join(GCU_Path_Flavor flavor, const char * a, const char * b) {
  return run([&](char * o, size_t n, size_t * l) {
    return gcu_path_join(flavor, a, b, o, n, l);
  });
}

string relative_to(GCU_Path_Flavor flavor, const char * from, const char * to,
    GCU_Path_Result * result_out = nullptr) {
  return run([&](char * o, size_t n, size_t * l) {
    return gcu_path_relative_to(flavor, from, to, nullptr, o, n, l);
  }, result_out);
}

string to_native(GCU_Path_Flavor flavor, const char * path) {
  return run([&](char * o, size_t n, size_t * l) {
    return gcu_path_to_native(flavor, path, o, n, l);
  });
}

string to_posix(GCU_Path_Flavor flavor, const char * path) {
  return run([&](char * o, size_t n, size_t * l) {
    return gcu_path_to_posix(flavor, path, o, n, l);
  });
}

/** Frees whatever an environment call handed back. */
struct Owned {
  char * value = nullptr;
  ~Owned() { gcu_path_free(nullptr, value); }
  string str() const { return value ? string(value) : string(); }
};

const GCU_Path_Flavor P = GCU_PATH_POSIX;
const GCU_Path_Flavor W = GCU_PATH_WINDOWS;

} // namespace

//////////////////////////////////////////////////////////////////////////////
// Flavour primitives
//////////////////////////////////////////////////////////////////////////////

TEST(Separator, EachFlavorWritesItsOwn) {
  EXPECT_EQ('/', gcu_path_separator(P));
  EXPECT_EQ('\\', gcu_path_separator(W));
}

TEST(Separator, WindowsAcceptsBothButPosixTreatsBackslashAsAFilename) {
  EXPECT_TRUE(gcu_path_is_separator(P, '/'));
  EXPECT_TRUE(gcu_path_is_separator(W, '/'));
  EXPECT_TRUE(gcu_path_is_separator(W, '\\'));
  // A backslash is a legal character in a POSIX filename.  Treating it as a
  // separator would silently rename the file being described.
  EXPECT_FALSE(gcu_path_is_separator(P, '\\'));
}

TEST(RootLength, PosixHasOnlyTheLeadingSlash) {
  EXPECT_EQ(0u, gcu_path_root_length(P, "a/b"));
  EXPECT_EQ(1u, gcu_path_root_length(P, "/a/b"));
  EXPECT_EQ(1u, gcu_path_root_length(P, "/"));
  EXPECT_EQ(0u, gcu_path_root_length(P, ""));
  EXPECT_EQ(0u, gcu_path_root_length(P, nullptr));
  // C: means nothing on POSIX; it is an ordinary relative filename.
  EXPECT_EQ(0u, gcu_path_root_length(P, "C:\\x"));
}

TEST(RootLength, WindowsDistinguishesEveryRootedForm) {
  EXPECT_EQ(3u, gcu_path_root_length(W, "C:\\x"));
  EXPECT_EQ(3u, gcu_path_root_length(W, "C:/x"));
  EXPECT_EQ(2u, gcu_path_root_length(W, "C:x"));
  EXPECT_EQ(2u, gcu_path_root_length(W, "C:"));
  EXPECT_EQ(1u, gcu_path_root_length(W, "\\x"));
  EXPECT_EQ(9u, gcu_path_root_length(W, "\\\\srv\\shr"));
  EXPECT_EQ(9u, gcu_path_root_length(W, "\\\\srv\\shr\\a"));
  EXPECT_EQ(7u, gcu_path_root_length(W, "\\\\?\\C:\\a"));
  EXPECT_EQ(0u, gcu_path_root_length(W, "a\\b"));
}

TEST(IsAbsolute, ADriveRelativePathIsNotAbsolute) {
  // The distinction this test exists for: all three have a non-zero root
  // length, and only one of them names a file without reference to some
  // current directory.
  EXPECT_TRUE(gcu_path_is_absolute(W, "C:\\x"));
  EXPECT_FALSE(gcu_path_is_absolute(W, "C:x"));
  EXPECT_FALSE(gcu_path_is_absolute(W, "\\x"));

  EXPECT_TRUE(gcu_path_is_absolute(W, "\\\\srv\\shr\\x"));
  EXPECT_TRUE(gcu_path_is_absolute(W, "\\\\?\\C:\\x"));
  EXPECT_FALSE(gcu_path_is_absolute(W, "a\\b"));

  EXPECT_TRUE(gcu_path_is_absolute(P, "/a"));
  EXPECT_FALSE(gcu_path_is_absolute(P, "a"));
  EXPECT_FALSE(gcu_path_is_absolute(P, ""));
  EXPECT_FALSE(gcu_path_is_absolute(P, nullptr));
}

//////////////////////////////////////////////////////////////////////////////
// Pointer-into-input accessors
//////////////////////////////////////////////////////////////////////////////

TEST(Basename, ReturnsTheFinalComponentWithoutCopying) {
  const char * path = "/a/b/c.txt";
  const char * base = gcu_path_basename(P, path);
  EXPECT_STREQ("c.txt", base);
  // It must point into the caller's string, not at a copy or a static buffer.
  EXPECT_EQ(path + 5, base);

  EXPECT_STREQ("a", gcu_path_basename(P, "a"));
  EXPECT_STREQ("a", gcu_path_basename(P, "/a"));
  EXPECT_STREQ("", gcu_path_basename(P, "/"));
  EXPECT_EQ(nullptr, gcu_path_basename(P, nullptr));
}

TEST(Basename, ATrailingSeparatorLeavesAnEmptyFinalComponent) {
  // Documented: the path is read as given.  Normalising first is the fix, and
  // the header says so.
  EXPECT_STREQ("", gcu_path_basename(P, "/a/b/"));
  string normalized = normalize(P, "/a/b/");
  EXPECT_STREQ("b", gcu_path_basename(P, normalized.c_str()));
}

TEST(Basename, WindowsStopsAtTheRootAsWellAsAtSeparators) {
  EXPECT_STREQ("file", gcu_path_basename(W, "C:file"));
  EXPECT_STREQ("file", gcu_path_basename(W, "C:\\file"));
  EXPECT_STREQ("file", gcu_path_basename(W, "C:/dir/file"));
}

TEST(Extension, ALeadingDotIsAHiddenFileNotAnExtension) {
  EXPECT_STREQ(".txt", gcu_path_extension(P, "a.txt"));
  EXPECT_STREQ(".gz", gcu_path_extension(P, "archive.tar.gz"));
  EXPECT_EQ(nullptr, gcu_path_extension(P, ".bashrc"));
  EXPECT_EQ(nullptr, gcu_path_extension(P, "plain"));
  // The dot belongs to the directory, not to the file.
  EXPECT_EQ(nullptr, gcu_path_extension(P, "dir.d/file"));
  EXPECT_STREQ(".", gcu_path_extension(P, "a."));
  EXPECT_EQ(nullptr, gcu_path_extension(P, nullptr));
}

//////////////////////////////////////////////////////////////////////////////
// dirname
//////////////////////////////////////////////////////////////////////////////

TEST(Dirname, YieldsTheParentAndNeverModifiesItsArgument) {
  char path[] = "/a/b";
  EXPECT_EQ("/a", dirname(P, path));
  EXPECT_STREQ("/a/b", path) << "POSIX dirname() mutates; this must not";

  EXPECT_EQ("/", dirname(P, "/a"));
  EXPECT_EQ("a", dirname(P, "a/b"));
  EXPECT_EQ(".", dirname(P, "a"));
  EXPECT_EQ("/", dirname(P, "/"));
  EXPECT_EQ(".", dirname(P, ""));
  EXPECT_EQ("a", dirname(P, "a/b/"));
  EXPECT_EQ("/a", dirname(P, "/a//b"));
}

TEST(Dirname, WindowsStopsAtWhicheverRootItHas) {
  EXPECT_EQ("C:\\a", dirname(W, "C:\\a\\b"));
  EXPECT_EQ("C:\\", dirname(W, "C:\\a"));
  EXPECT_EQ("C:", dirname(W, "C:a"));
  EXPECT_EQ("C:", dirname(W, "C:"));
  EXPECT_EQ("\\\\srv\\shr", dirname(W, "\\\\srv\\shr\\a"));
}

//////////////////////////////////////////////////////////////////////////////
// join
//////////////////////////////////////////////////////////////////////////////

TEST(Join, InsertsExactlyOneSeparator) {
  EXPECT_EQ("a/b", join(P, "a", "b"));
  EXPECT_EQ("a/b", join(P, "a/", "b"));
  EXPECT_EQ("/a/b", join(P, "/a", "b"));
  EXPECT_EQ("b", join(P, "", "b"));
  EXPECT_EQ("a", join(P, "a", ""));
  EXPECT_EQ("b", join(P, nullptr, "b"));
  EXPECT_EQ("a", join(P, "a", nullptr));
}

TEST(Join, AnAbsoluteRightHandSideReplacesTheLeft) {
  // A caller who supplies an absolute path means it; silently anchoring it
  // under a base is how configuration overrides stop working.
  EXPECT_EQ("/b", join(P, "/a", "/b"));
  EXPECT_EQ("D:\\y", join(W, "C:\\x", "D:\\y"));
}

TEST(Join, AWindowsRootedPathKeepsTheDriveItIsJoinedOnto) {
  // "\z" is rooted but not absolute: it means the root of whichever drive is
  // in play, which is the one the base names.
  EXPECT_EQ("C:\\z", join(W, "C:\\x\\y", "\\z"));
  EXPECT_EQ("\\\\srv\\shr\\z", join(W, "\\\\srv\\shr\\x", "\\z"));
  EXPECT_EQ("C:\\x\\y", join(W, "C:\\x", "y"));
  EXPECT_EQ("C:\\x\\y", join(W, "C:\\x\\", "y"));
}

//////////////////////////////////////////////////////////////////////////////
// normalize
//////////////////////////////////////////////////////////////////////////////

TEST(Normalize, CollapsesDotAndDuplicateSeparators) {
  EXPECT_EQ(".", normalize(P, ""));
  EXPECT_EQ(".", normalize(P, "."));
  EXPECT_EQ(".", normalize(P, "./"));
  EXPECT_EQ("a", normalize(P, "a"));
  EXPECT_EQ("a", normalize(P, "a/"));
  EXPECT_EQ("a", normalize(P, "./a"));
  EXPECT_EQ("a/b", normalize(P, "a//b"));
  EXPECT_EQ("a/b", normalize(P, "a/./b"));
  EXPECT_EQ("/a/b", normalize(P, "/a/./b/"));
  EXPECT_EQ("/", normalize(P, "/"));
  EXPECT_EQ("/", normalize(P, "//"));
  EXPECT_EQ("/", normalize(P, "///"));
}

TEST(Normalize, DotDotCancelsTheComponentBeforeIt) {
  EXPECT_EQ("a", normalize(P, "a/b/.."));
  EXPECT_EQ(".", normalize(P, "a/.."));
  EXPECT_EQ("/a/c", normalize(P, "/a/b/../c"));
  EXPECT_EQ("c", normalize(P, "a/b/../../c"));
}

TEST(Normalize, DotDotStopsAtARootButEscapesARelativePath) {
  // An absolute path has a floor; a relative one does not, and dropping the
  // leading ".." would change which file it names.
  EXPECT_EQ("/", normalize(P, "/.."));
  EXPECT_EQ("/", normalize(P, "/../.."));
  EXPECT_EQ("/", normalize(P, "/a/.."));
  EXPECT_EQ("/b", normalize(P, "/a/../../b"));

  EXPECT_EQ("..", normalize(P, ".."));
  EXPECT_EQ("..", normalize(P, "a/../.."));
  EXPECT_EQ("../a", normalize(P, "../a"));
  EXPECT_EQ("../c", normalize(P, "a/b/../../../c"));
  EXPECT_EQ("../..", normalize(P, "../.."));
}

TEST(Normalize, WindowsRewritesSeparatorsAndKeepsEachRootKind) {
  EXPECT_EQ("C:\\", normalize(W, "C:\\"));
  EXPECT_EQ("C:\\", normalize(W, "C:/"));
  EXPECT_EQ("C:\\a\\b", normalize(W, "C:/a/b"));
  EXPECT_EQ("C:\\a\\b", normalize(W, "C:\\a\\.\\b"));
  EXPECT_EQ("C:\\", normalize(W, "C:\\a\\.."));
  EXPECT_EQ("\\", normalize(W, "\\a\\.."));
  EXPECT_EQ("\\\\srv\\shr", normalize(W, "\\\\srv\\shr\\a\\.."));
  EXPECT_EQ("\\\\srv\\shr\\a\\b", normalize(W, "\\\\srv\\shr\\a\\b"));
}

TEST(Normalize, ABareDriveIsRelativeSoDotDotSurvivesIt) {
  // "C:" names the current directory *of drive C*, which has a parent, so
  // "C:.." is a real location and must not be flattened to "C:".
  EXPECT_EQ("C:a", normalize(W, "C:a"));
  EXPECT_EQ("C:", normalize(W, "C:a\\.."));
  EXPECT_EQ("C:..", normalize(W, "C:.."));
  EXPECT_EQ("C:..\\..", normalize(W, "C:..\\.."));
}

TEST(Normalize, AnExtendedWindowsPathIsLeftExactlyAsItWasGiven) {
  // Win32 hands \\?\ paths to the object manager unparsed, so "." and ".."
  // inside one are literal component names.  Resolving them would name a
  // different file, or nothing.
  EXPECT_EQ("\\\\?\\C:\\a\\..", normalize(W, "\\\\?\\C:\\a\\.."));
  EXPECT_EQ("\\\\?\\C:\\a/b", normalize(W, "\\\\?\\C:\\a/b"));
}

TEST(Normalize, MeasuringAgreesWithWritingOnEveryShape) {
  // normalize() computes its length arithmetically and then places bytes from
  // the right; the two are different code and could disagree.  This walks a
  // corpus through both and checks they do not.  The helper asserts the
  // equality, so the value here is the breadth of the corpus.
  static const char * corpus[] = {
    "", ".", "..", "/", "//", "a", "a/", "a//b", "a/./b", "a/b/..", "a/..",
    "a/../..", "../a", "/..", "/../..", "/a/..", "/a/b/../c", "./a",
    "a/b/../../c", "a/b/../../../c", "/a/./b/", "../../..", "a/../b/../c",
    "C:\\", "C:/", "C:\\a\\b", "C:/a/b", "C:\\a\\..", "C:a", "C:a\\..",
    "C:..", "C:..\\..", "\\a\\..", "\\\\srv\\shr", "\\\\srv\\shr\\a\\..",
    "\\\\srv\\shr\\a\\b", "\\x", "\\", "\\\\?\\C:\\a\\..",
  };
  for (const char * input : corpus) {
    for (GCU_Path_Flavor flavor : {P, W}) {
      string got = normalize(flavor, input);
      EXPECT_FALSE(got.empty())
          << "input \"" << input << "\" flavor " << (int)flavor;
    }
  }
}

TEST(Normalize, IsIdempotent) {
  // Normalising a normalised path must change nothing.  A result that still
  // held a "." or a doubled separator would be caught here even if the first
  // pass looked right.
  static const char * corpus[] = {
    "", ".", "..", "/", "a//b", "a/b/..", "/a/b/../c", "a/b/../../../c",
    "C:/a/b", "C:\\a\\..", "C:..", "\\\\srv\\shr\\a\\..", "\\a\\..",
  };
  for (const char * input : corpus) {
    for (GCU_Path_Flavor flavor : {P, W}) {
      string once = normalize(flavor, input);
      string twice = normalize(flavor, once.c_str());
      EXPECT_EQ(once, twice)
          << "input \"" << input << "\" flavor " << (int)flavor;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
// relative_to
//////////////////////////////////////////////////////////////////////////////

TEST(RelativeTo, WalksUpAndThenDown) {
  EXPECT_EQ(".", relative_to(P, "/a", "/a"));
  EXPECT_EQ("b", relative_to(P, "/a", "/a/b"));
  EXPECT_EQ("..", relative_to(P, "/a/b", "/a"));
  EXPECT_EQ("../c", relative_to(P, "/a/b", "/a/c"));
  EXPECT_EQ("../../d/e", relative_to(P, "/a/b/c", "/a/d/e"));
  EXPECT_EQ("b/c", relative_to(P, "/a", "/a/b/c"));
  EXPECT_EQ("../..", relative_to(P, "/a/b/c", "/a"));
}

TEST(RelativeTo, NormalisesBothSidesFirst) {
  EXPECT_EQ("c", relative_to(P, "/a/b/..", "/a/c"));
  EXPECT_EQ("../c", relative_to(P, "/a/./b", "/a//c"));
}

TEST(RelativeTo, WorksBetweenTwoRelativePaths) {
  EXPECT_EQ("../b", relative_to(P, "a", "b"));
  EXPECT_EQ("b", relative_to(P, "a", "a/b"));
  EXPECT_EQ(".", relative_to(P, "a", "a"));
}

TEST(RelativeTo, RefusesWhatCannotBeExpressed) {
  GCU_Path_Result result = GCU_PATH_OK;

  // Different drives have no path between them.  This is a fact about the
  // filesystem, not a limitation of this function.
  relative_to(W, "C:\\a", "D:\\b", &result);
  EXPECT_EQ(GCU_PATH_ERR_UNSUPPORTED, result);

  // One absolute and one relative cannot be compared without a current
  // directory, which is not an argument here.
  relative_to(P, "/a", "b", &result);
  EXPECT_EQ(GCU_PATH_ERR_UNSUPPORTED, result);
  relative_to(P, "a", "/b", &result);
  EXPECT_EQ(GCU_PATH_ERR_UNSUPPORTED, result);

  // Where a leading ".." sits depends on the same unknown directory.
  relative_to(P, "../a", "b", &result);
  EXPECT_EQ(GCU_PATH_ERR_UNSUPPORTED, result);
  relative_to(P, "a", "../b", &result);
  EXPECT_EQ(GCU_PATH_ERR_UNSUPPORTED, result);
}

TEST(RelativeTo, WindowsComparesComponentsWithoutRegardToAsciiCase) {
  EXPECT_EQ("b", relative_to(W, "C:\\a", "C:\\A\\b"));
  EXPECT_EQ("b", relative_to(W, "c:\\a", "C:\\a\\b"));
  // POSIX filesystems distinguish case, so the same pair is two directories.
  EXPECT_EQ("../A/b", relative_to(P, "/a", "/A/b"));
}

//////////////////////////////////////////////////////////////////////////////
// Separator rewriting
//////////////////////////////////////////////////////////////////////////////

TEST(ToNative, PosixCopiesUnchangedBecauseBackslashIsAFilenameCharacter) {
  EXPECT_EQ("a\\b", to_native(P, "a\\b"));
  EXPECT_EQ("a/b", to_native(P, "a/b"));
}

TEST(ToNative, WindowsRewritesForwardSlashes) {
  EXPECT_EQ("a\\b", to_native(W, "a/b"));
  EXPECT_EQ("C:\\a\\b", to_native(W, "C:/a/b"));
}

TEST(ToPosix, RewritesOnlyWhereASeparatorIsMeant) {
  EXPECT_EQ("a/b", to_posix(W, "a\\b"));
  EXPECT_EQ("C:/a/b", to_posix(W, "C:\\a\\b"));
  EXPECT_EQ("a\\b", to_posix(P, "a\\b"));
}

TEST(SeparatorRewriting, LeavesAnExtendedWindowsPathAlone) {
  // \\?\ paths are not parsed by Win32, so a "/" inside one is a literal
  // character.  Rewriting either way produces a path that names nothing.
  EXPECT_EQ("\\\\?\\C:\\a/b", to_native(W, "\\\\?\\C:\\a/b"));
  EXPECT_EQ("\\\\?\\C:\\a\\b", to_posix(W, "\\\\?\\C:\\a\\b"));
}

//////////////////////////////////////////////////////////////////////////////
// The buffer contract
//////////////////////////////////////////////////////////////////////////////

TEST(BufferContract, MeasuringReportsTheLengthWithoutABuffer) {
  size_t needed = 0;
  ASSERT_EQ(GCU_PATH_OK, gcu_path_join(P, "aa", "bb", nullptr, 0, &needed));
  EXPECT_EQ(5u, needed); // "aa/bb"
}

TEST(BufferContract, ATooSmallBufferIsRefusedAndLeftUntouched) {
  // Truncation is the failure mode this rules out: a shortened path is still
  // a valid path, and it names a different file.
  char buffer[4];
  memset(buffer, '\x7f', sizeof(buffer));
  size_t needed = 0;
  EXPECT_EQ(GCU_PATH_ERR_LIMIT,
      gcu_path_join(P, "aa", "bb", buffer, sizeof(buffer), &needed));
  EXPECT_EQ(5u, needed) << "must report the size that would work";
  for (size_t i = 0; i < sizeof(buffer); ++i) {
    EXPECT_EQ('\x7f', buffer[i]) << "byte " << i << " was written";
  }
}

TEST(BufferContract, ABufferOfExactlyTheRightSizeIsAccepted) {
  char buffer[6];
  size_t needed = 0;
  ASSERT_EQ(GCU_PATH_OK,
      gcu_path_join(P, "aa", "bb", buffer, sizeof(buffer), &needed));
  EXPECT_STREQ("aa/bb", buffer);
  EXPECT_EQ(5u, needed);

  // One byte fewer is one byte too few, because the terminator counts.
  EXPECT_EQ(GCU_PATH_ERR_LIMIT,
      gcu_path_join(P, "aa", "bb", buffer, sizeof(buffer) - 1, nullptr));
}

TEST(BufferContract, NormalizeReportsTheSameLimitAsEveryOtherOperation) {
  // normalize() has its own measuring path, so the contract is checked here
  // as well as on join().
  char buffer[3];
  memset(buffer, '\x7f', sizeof(buffer));
  size_t needed = 0;
  EXPECT_EQ(GCU_PATH_ERR_LIMIT,
      gcu_path_normalize(P, "/a/b/c", buffer, sizeof(buffer), &needed));
  EXPECT_EQ(6u, needed);
  for (size_t i = 0; i < sizeof(buffer); ++i) {
    EXPECT_EQ('\x7f', buffer[i]);
  }
}

TEST(BufferContract, NullArgumentsAreRefusedRatherThanFatal) {
  size_t needed = 0;
  EXPECT_EQ(GCU_PATH_ERR_INVALID,
      gcu_path_normalize(P, nullptr, nullptr, 0, &needed));
  EXPECT_EQ(GCU_PATH_ERR_INVALID,
      gcu_path_dirname(P, nullptr, nullptr, 0, &needed));
  EXPECT_EQ(GCU_PATH_ERR_INVALID,
      gcu_path_to_native(P, nullptr, nullptr, 0, &needed));
  EXPECT_EQ(GCU_PATH_ERR_INVALID,
      gcu_path_relative_to(P, nullptr, "/a", nullptr, nullptr, 0, &needed));

  // A size with no buffer is a caller error, not a measuring request.
  char buffer[8];
  EXPECT_EQ(GCU_PATH_ERR_INVALID,
      gcu_path_join(P, "a", "b", nullptr, sizeof(buffer), &needed));

  // An out-of-range flavour is rejected rather than defaulting to one.
  EXPECT_EQ(GCU_PATH_ERR_INVALID,
      gcu_path_join((GCU_Path_Flavor)42, "a", "b", nullptr, 0, &needed));

  EXPECT_EQ(GCU_PATH_ERR_INVALID, gcu_path_cwd(nullptr, nullptr));
  EXPECT_EQ(GCU_PATH_ERR_INVALID, gcu_path_home(nullptr, nullptr));
  EXPECT_EQ(GCU_PATH_ERR_INVALID, gcu_path_absolute(nullptr, nullptr, nullptr));
  EXPECT_EQ(GCU_PATH_ERR_INVALID,
      gcu_path_canonicalize(nullptr, nullptr, nullptr));
}

TEST(ResultString, NamesEveryValueAndRefusesNone) {
  for (int i = 0; i < GCU_PATH_RESULT_COUNT; ++i) {
    const char * text = gcu_path_result_string((GCU_Path_Result)i);
    ASSERT_NE(nullptr, text) << "result " << i;
    EXPECT_STRNE("unknown", text) << "result " << i << " has no name";
  }
  EXPECT_STREQ("unknown", gcu_path_result_string((GCU_Path_Result)999));
}

//////////////////////////////////////////////////////////////////////////////
// Environment
//////////////////////////////////////////////////////////////////////////////

TEST(Cwd, ReturnsAnAbsolutePathTheCallerOwns) {
  Owned cwd;
  ASSERT_EQ(GCU_PATH_OK, gcu_path_cwd(nullptr, &cwd.value));
  ASSERT_NE(nullptr, cwd.value);
  EXPECT_TRUE(gcu_path_is_absolute(GCU_PATH_NATIVE, cwd.value))
      << "got \"" << cwd.str() << "\"";
}

TEST(Home, IsAbsoluteAndHasNoTrailingSeparator) {
  Owned home;
  ASSERT_EQ(GCU_PATH_OK, gcu_path_home(nullptr, &home.value));
  ASSERT_NE(nullptr, home.value);
  string text = home.str();
  EXPECT_TRUE(gcu_path_is_absolute(GCU_PATH_NATIVE, home.value))
      << "got \"" << text << "\"";
  if (text.size() > 1) {
    EXPECT_FALSE(gcu_path_is_separator(GCU_PATH_NATIVE, text.back()))
        << "a trailing separator would double when joined";
  }
}

TEST(UserDirectories, EachIsAbsoluteAndJoinsCleanly) {
  struct { const char * name; GCU_Path_Result (*fn)(const GCU_Allocator *,
      char **); } cases[] = {
    {"config", gcu_path_config_dir},
    {"data", gcu_path_data_dir},
    {"cache", gcu_path_cache_dir},
    {"temp", gcu_path_temp_dir},
  };
  for (auto & c : cases) {
    Owned dir;
    ASSERT_EQ(GCU_PATH_OK, c.fn(nullptr, &dir.value)) << c.name;
    ASSERT_NE(nullptr, dir.value) << c.name;
    string text = dir.str();
    EXPECT_FALSE(text.empty()) << c.name;
    EXPECT_TRUE(gcu_path_is_absolute(GCU_PATH_NATIVE, dir.value))
        << c.name << " gave \"" << text << "\"";
    EXPECT_FALSE(gcu_path_is_separator(GCU_PATH_NATIVE, text.back()))
        << c.name << " ends in a separator, which would double on a join";
  }
}

TEST(ConfigDir, HonoursAnAbsoluteXdgOverrideAndIgnoresARelativeOne) {
#ifndef _WIN32
  const char * saved = getenv("XDG_CONFIG_HOME");
  string restore = saved ? saved : "";

  setenv("XDG_CONFIG_HOME", "/somewhere/else", 1);
  {
    Owned dir;
    ASSERT_EQ(GCU_PATH_OK, gcu_path_config_dir(nullptr, &dir.value));
#ifndef __APPLE__
    EXPECT_EQ("/somewhere/else", dir.str());
#endif
  }

  // The specification says a relative value is to be ignored, and it is
  // right to: it would land wherever the process happened to start.
  setenv("XDG_CONFIG_HOME", "relative/path", 1);
  {
    Owned dir;
    ASSERT_EQ(GCU_PATH_OK, gcu_path_config_dir(nullptr, &dir.value));
    EXPECT_TRUE(gcu_path_is_absolute(GCU_PATH_NATIVE, dir.value))
        << "a relative override was used anyway: \"" << dir.str() << "\"";
  }

  // An exported-but-empty variable means unset far more often than it means
  // the root.
  setenv("XDG_CONFIG_HOME", "", 1);
  {
    Owned dir;
    ASSERT_EQ(GCU_PATH_OK, gcu_path_config_dir(nullptr, &dir.value));
    EXPECT_TRUE(gcu_path_is_absolute(GCU_PATH_NATIVE, dir.value));
  }

  if (saved) {
    setenv("XDG_CONFIG_HOME", restore.c_str(), 1);
  }
  else {
    unsetenv("XDG_CONFIG_HOME");
  }
#endif
}

TEST(UserDirectories, ATrailingSeparatorInTheEnvironmentIsRemoved) {
#ifndef _WIN32
  // Nothing on a normal machine supplies one - $TMPDIR is usually unset and
  // $HOME rarely ends in a slash - so without setting it deliberately the
  // trimming is never executed and a regression in it would pass unseen.
  // GetTempPath() on Windows *always* ends in a separator, which is what the
  // trimming is there for in the first place.
  const char * saved_tmp = getenv("TMPDIR");
  string restore_tmp = saved_tmp ? saved_tmp : "";

  setenv("TMPDIR", "/var/tmp/", 1);
  {
    Owned dir;
    ASSERT_EQ(GCU_PATH_OK, gcu_path_temp_dir(nullptr, &dir.value));
    EXPECT_EQ("/var/tmp", dir.str());
  }

  setenv("TMPDIR", "/var/tmp///", 1);
  {
    Owned dir;
    ASSERT_EQ(GCU_PATH_OK, gcu_path_temp_dir(nullptr, &dir.value));
    EXPECT_EQ("/var/tmp", dir.str());
  }

  // The root is all separator, and trimming it away would leave nothing.
  setenv("TMPDIR", "/", 1);
  {
    Owned dir;
    ASSERT_EQ(GCU_PATH_OK, gcu_path_temp_dir(nullptr, &dir.value));
    EXPECT_EQ("/", dir.str());
  }

  if (saved_tmp) {
    setenv("TMPDIR", restore_tmp.c_str(), 1);
  }
  else {
    unsetenv("TMPDIR");
  }

  const char * saved_cfg = getenv("XDG_CONFIG_HOME");
  string restore_cfg = saved_cfg ? saved_cfg : "";
  setenv("XDG_CONFIG_HOME", "/somewhere/else/", 1);
  {
    Owned dir;
    ASSERT_EQ(GCU_PATH_OK, gcu_path_config_dir(nullptr, &dir.value));
#ifndef __APPLE__
    EXPECT_EQ("/somewhere/else", dir.str());
#endif
  }
  if (saved_cfg) {
    setenv("XDG_CONFIG_HOME", restore_cfg.c_str(), 1);
  }
  else {
    unsetenv("XDG_CONFIG_HOME");
  }
#endif
}

TEST(Absolute, ResolvesAgainstTheWorkingDirectoryWithoutTouchingTheDisk) {
  Owned cwd;
  ASSERT_EQ(GCU_PATH_OK, gcu_path_cwd(nullptr, &cwd.value));

  Owned resolved;
  ASSERT_EQ(GCU_PATH_OK,
      gcu_path_absolute("a/b", nullptr, &resolved.value));
  EXPECT_EQ(cwd.str() + "/a/b", resolved.str());

  // Nothing has to exist: this answers what the path means, not what it
  // reaches.
  Owned missing;
  ASSERT_EQ(GCU_PATH_OK,
      gcu_path_absolute("no/such/file/anywhere", nullptr, &missing.value));
  EXPECT_TRUE(gcu_path_is_absolute(GCU_PATH_NATIVE, missing.value));

  Owned already;
  ASSERT_EQ(GCU_PATH_OK,
      gcu_path_absolute("/a/./b/../c", nullptr, &already.value));
  EXPECT_EQ("/a/c", already.str());
}

TEST(Canonicalize, RequiresThePathToExistAndResolvesIt) {
  Owned resolved;
  ASSERT_EQ(GCU_PATH_OK, gcu_path_canonicalize(".", nullptr, &resolved.value));
  EXPECT_TRUE(gcu_path_is_absolute(GCU_PATH_NATIVE, resolved.value));

  // The price of an answer the filesystem has agreed to is that there has to
  // be something there.  This is the difference from gcu_path_absolute().
  char * nothing = nullptr;
  EXPECT_EQ(GCU_PATH_ERR_IO,
      gcu_path_canonicalize("/no/such/file/anywhere", nullptr, &nothing));
  EXPECT_EQ(nullptr, nothing) << "nothing may be allocated on failure";
}

TEST(Free, AcceptsNullSoCleanupPathsNeedNoGuard) {
  gcu_path_free(nullptr, nullptr);
  gcu_path_free(gcu_allocator_default(), nullptr);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
