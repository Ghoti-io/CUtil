/**
 * @file
 *
 * Tests for the directory module.
 *
 * Copyright 2026 by Corey Pennycuff
 */

#include <algorithm>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <gtest/gtest.h>
#include <ghoti.io/cutil/dir.h>
#include <ghoti.io/cutil/file.h>
#include <ghoti.io/cutil/path.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#endif

using namespace std;

namespace {

/** A scratch directory that removes itself and everything under it. */
class Scratch : public testing::Test {
protected:
  string dir;

  void SetUp() override {
    char * made = nullptr;
    ASSERT_EQ(GCU_FILE_OK,
        gcu_dir_temp_create(nullptr, "gcu-dir-test", nullptr, &made));
    dir = made;
    gcu_dir_free_path(nullptr, made);
  }

  void TearDown() override {
    if (!dir.empty()) {
      wipe(dir);
    }
  }

  /**
   * Remove a tree, deliberately NOT through the module under test.
   *
   * An earlier version of this walked with gcu_dir_read(), which made the
   * fixture depend on the very behaviour the tests check.  Breaking the skip
   * of "." then sent every teardown into infinite recursion, so the suite
   * hung instead of failing - and a hung run reports no failures at all,
   * which reads exactly like a mutation nothing caught.
   */
  static void wipe(const string & path) {
#ifdef _WIN32
    WIN32_FIND_DATAA e;
    HANDLE d = FindFirstFileA((path + "/*").c_str(), &e);
    if (d != INVALID_HANDLE_VALUE) {
      vector<pair<string, bool>> found;
      do {
        string name = e.cFileName;
        if (name != "." && name != "..") {
          // A reparse point is removed as itself, never followed, which is
          // what lstat() gives the POSIX arm below.
          bool is_dir = (e.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
              && !(e.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
          found.emplace_back(name, is_dir);
        }
      } while (FindNextFileA(d, &e));
      FindClose(d);
      for (auto & [name, is_dir] : found) {
        string child = path + "/" + name;
        if (is_dir) {
          wipe(child);
        }
        else {
          SetFileAttributesA(child.c_str(), FILE_ATTRIBUTE_NORMAL);
          DeleteFileA(child.c_str());
        }
      }
    }
    RemoveDirectoryA(path.c_str());
#else
    if (DIR * d = opendir(path.c_str())) {
      vector<string> found;
      while (struct dirent * e = readdir(d)) {
        string name = e->d_name;
        if (name != "." && name != "..") {
          found.push_back(name);
        }
      }
      closedir(d);
      for (auto & name : found) {
        string child = path + "/" + name;
        struct stat info;
        if (lstat(child.c_str(), &info) == 0 && S_ISDIR(info.st_mode)) {
          wipe(child);
        }
        else {
          remove(child.c_str());
        }
      }
    }
    rmdir(path.c_str());
#endif
  }

  string at(const string & name) const { return dir + "/" + name; }

  /** Every name in a directory, sorted. */
  vector<string> names(const string & path) const {
    vector<string> found;
    GCU_Dir d;
    if (gcu_dir_open(&d, path.c_str(), nullptr) == GCU_FILE_OK) {
      const char * name = nullptr;
      bool done = false;
      while (gcu_dir_read(&d, &name, nullptr, &done) == GCU_FILE_OK && !done) {
        found.push_back(name);
      }
      gcu_dir_close(&d);
    }
    sort(found.begin(), found.end());
    return found;
  }
};

/**
 * Make a symbolic link, or say why the test cannot.
 *
 * Windows creates one only with SeCreateSymbolicLinkPrivilege or with
 * Developer Mode on, and an ordinary account has neither; that is the
 * machine's configuration, not a defect, so the caller skips.
 */
bool make_symlink(const string & target, const string & link) {
#ifdef _WIN32
  return CreateSymbolicLinkA(link.c_str(), target.c_str(),
      SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0;
#else
  return symlink(target.c_str(), link.c_str()) == 0;
#endif
}

void put(const string & path, const string & bytes) {
  ASSERT_EQ(GCU_FILE_OK,
      gcu_file_write_atomic(path.c_str(), bytes.data(), bytes.size(),
          GCU_FILE_SYNC_NONE, GCU_FILE_PERMS_DEFAULT, nullptr));
}


TEST_F(Scratch, CreateMakesOneLevelAndSaysWhenItIsAlreadyThere) {
  EXPECT_EQ(GCU_FILE_OK, gcu_dir_create(at("one").c_str()));
  EXPECT_TRUE(gcu_file_is_directory(at("one").c_str()));
  EXPECT_EQ(GCU_FILE_ERR_EXISTS, gcu_dir_create(at("one").c_str()));
}

TEST_F(Scratch, CreateWillNotBuildAPathAndSaysWhichPartIsMissing) {
  EXPECT_EQ(GCU_FILE_ERR_NOT_FOUND,
      gcu_dir_create(at("no/such/parent").c_str()));
  EXPECT_FALSE(gcu_file_exists(at("no").c_str()));
}

TEST_F(Scratch, CreateAllBuildsEveryMissingLevel) {
  ASSERT_EQ(GCU_FILE_OK,
      gcu_dir_create_all(at("a/b/c/d").c_str(), nullptr));
  EXPECT_TRUE(gcu_file_is_directory(at("a").c_str()));
  EXPECT_TRUE(gcu_file_is_directory(at("a/b").c_str()));
  EXPECT_TRUE(gcu_file_is_directory(at("a/b/c").c_str()));
  EXPECT_TRUE(gcu_file_is_directory(at("a/b/c/d").c_str()));
}

TEST_F(Scratch, CreateAllSucceedsWhenTheDirectoryIsAlreadyThere) {
  // The caller asked for it to be there, not for it to be new.
  ASSERT_EQ(GCU_FILE_OK, gcu_dir_create_all(at("x/y").c_str(), nullptr));
  EXPECT_EQ(GCU_FILE_OK, gcu_dir_create_all(at("x/y").c_str(), nullptr));
  EXPECT_EQ(GCU_FILE_OK, gcu_dir_create_all(at("x").c_str(), nullptr));
}

TEST_F(Scratch, CreateAllToleratesRepeatedAndTrailingSeparators) {
  ASSERT_EQ(GCU_FILE_OK,
      gcu_dir_create_all((dir + "//p///q//").c_str(), nullptr));
  EXPECT_TRUE(gcu_file_is_directory(at("p/q").c_str()));
}

TEST_F(Scratch, CreateAllRefusesWhenSomethingElseIsInTheWay) {
  put(at("blocker"), "not a directory");
  // Must not report success just because the name is taken: the caller asked
  // for a directory and there is not one.
  EXPECT_EQ(GCU_FILE_ERR_EXISTS,
      gcu_dir_create_all(at("blocker").c_str(), nullptr));
  EXPECT_EQ(GCU_FILE_ERR_EXISTS,
      gcu_dir_create_all(at("blocker/under").c_str(), nullptr));
  EXPECT_FALSE(gcu_file_is_directory(at("blocker").c_str()));
}

TEST_F(Scratch, CreateAllAcceptsARootThatAlreadyExists) {
  EXPECT_EQ(GCU_FILE_OK, gcu_dir_create_all("/", nullptr));
  EXPECT_EQ(GCU_FILE_OK, gcu_dir_create_all(dir.c_str(), nullptr));
}

// The walk starts past the root rather than at zero.  That is not observable
// here: on POSIX the only root is "/", and starting at zero simply produces an
// empty first component that the loop skips anyway.  It matters on Windows,
// where the root of "C:\\x" is three characters and starting at zero would try
// to create "C:" as a directory.  Sabotaging the root skip therefore fails
// nothing in this suite, and that is recorded rather than papered over.

TEST_F(Scratch, RemoveTakesAnEmptyDirectoryAndRefusesAFullOne) {
  ASSERT_EQ(GCU_FILE_OK, gcu_dir_create(at("empty").c_str()));
  EXPECT_EQ(GCU_FILE_OK, gcu_dir_remove(at("empty").c_str()));
  EXPECT_FALSE(gcu_file_exists(at("empty").c_str()));

  ASSERT_EQ(GCU_FILE_OK, gcu_dir_create(at("full").c_str()));
  put(at("full/thing"), "x");
  EXPECT_EQ(GCU_FILE_ERR_NOT_EMPTY, gcu_dir_remove(at("full").c_str()));
  EXPECT_TRUE(gcu_file_is_directory(at("full").c_str()));

  EXPECT_EQ(GCU_FILE_ERR_NOT_FOUND, gcu_dir_remove(at("never").c_str()));
}

TEST_F(Scratch, ReadReportsEveryEntryAndNeitherDotNorDotDot) {
  put(at("one"), "1");
  put(at("two"), "2");
  ASSERT_EQ(GCU_FILE_OK, gcu_dir_create(at("three").c_str()));

  vector<string> expected{"one", "three", "two"};
  EXPECT_EQ(expected, names(dir));
}

TEST_F(Scratch, ReadOfAnEmptyDirectoryIsDoneImmediately) {
  ASSERT_EQ(GCU_FILE_OK, gcu_dir_create(at("bare").c_str()));
  GCU_Dir d;
  ASSERT_EQ(GCU_FILE_OK, gcu_dir_open(&d, at("bare").c_str(), nullptr));
  const char * name = nullptr;
  bool done = false;
  EXPECT_EQ(GCU_FILE_OK, gcu_dir_read(&d, &name, nullptr, &done));
  EXPECT_TRUE(done);
  gcu_dir_close(&d);
}

TEST_F(Scratch, ReadReportsWhatEachEntryIs) {
  put(at("f"), "x");
  ASSERT_EQ(GCU_FILE_OK, gcu_dir_create(at("d").c_str()));
#ifdef _WIN32
  if (!make_symlink(at("f"), at("l"))) {
    GTEST_SKIP() << "this account cannot create symbolic links";
  }
#else
  ASSERT_TRUE(make_symlink(at("f"), at("l")));
#endif

  GCU_Dir dh;
  ASSERT_EQ(GCU_FILE_OK, gcu_dir_open(&dh, dir.c_str(), nullptr));
  const char * name = nullptr;
  GCU_File_Type type;
  bool done = false;
  vector<pair<string, GCU_File_Type>> found;
  while (gcu_dir_read(&dh, &name, &type, &done) == GCU_FILE_OK && !done) {
    found.push_back({name, type});
  }
  gcu_dir_close(&dh);
  sort(found.begin(), found.end());

  ASSERT_EQ(3u, found.size());
  EXPECT_EQ("d", found[0].first);
  EXPECT_EQ(GCU_FILE_TYPE_DIRECTORY, found[0].second);
  EXPECT_EQ("f", found[1].first);
  EXPECT_EQ(GCU_FILE_TYPE_REGULAR, found[1].second);
  // A link is reported as a link, not as what it points at: a walk that
  // follows links is a walk that can leave the tree it was asked about.
  EXPECT_EQ("l", found[2].first);
  EXPECT_EQ(GCU_FILE_TYPE_SYMLINK, found[2].second);
}

TEST_F(Scratch, OpeningSomethingThatIsNotADirectoryFails) {
  put(at("f"), "x");
  GCU_Dir d;
  EXPECT_EQ(GCU_FILE_ERR_NOT_FOUND, gcu_dir_open(&d, at("f").c_str(),
      nullptr));
  EXPECT_EQ(GCU_FILE_ERR_NOT_FOUND,
      gcu_dir_open(&d, at("absent").c_str(), nullptr));
  // Zeroed by a failed open, so closing it is safe.
  gcu_dir_close(&d);
}

TEST_F(Scratch, CloseIsSafeOnAHandleThatWasNeverOpenedOrIsAlreadySpent) {
  gcu_dir_close(nullptr);
  GCU_Dir zeroed;
  memset(&zeroed, 0, sizeof zeroed);
  gcu_dir_close(&zeroed);
  gcu_dir_close(&zeroed);

  GCU_Dir d;
  ASSERT_EQ(GCU_FILE_OK, gcu_dir_open(&d, dir.c_str(), nullptr));
  gcu_dir_close(&d);
  gcu_dir_close(&d);
}

TEST_F(Scratch, TempDirectoryIsCreatedUniqueAndOwnerOnly) {
  char * a = nullptr;
  char * b = nullptr;
  ASSERT_EQ(GCU_FILE_OK,
      gcu_dir_temp_create(dir.c_str(), "work", nullptr, &a));
  ASSERT_EQ(GCU_FILE_OK,
      gcu_dir_temp_create(dir.c_str(), "work", nullptr, &b));

  EXPECT_STRNE(a, b) << "two calls must not choose the same name";
  EXPECT_TRUE(gcu_file_is_directory(a));
  EXPECT_TRUE(gcu_file_is_directory(b));

#ifndef _WIN32
  // Windows has no mode bits to read: stat() synthesises 0777 for any
  // writable directory, and ownership lives in the ACL.  What the Windows
  // arm does about "owner only" is not asserted here.
  struct stat info;
  ASSERT_EQ(0, stat(a, &info));
  EXPECT_EQ(0700, (int)(info.st_mode & 07777));
#endif

  gcu_dir_free_path(nullptr, a);
  gcu_dir_free_path(nullptr, b);
}

TEST_F(Scratch, TempDirectoryFallsBackToTheSystemLocation) {
  char * made = nullptr;
  ASSERT_EQ(GCU_FILE_OK,
      gcu_dir_temp_create(nullptr, nullptr, nullptr, &made));
  EXPECT_TRUE(gcu_file_is_directory(made));
  EXPECT_EQ(GCU_FILE_OK, gcu_dir_remove(made));
  gcu_dir_free_path(nullptr, made);
}

TEST_F(Scratch, TempDirectoryUnderAMissingParentFails) {
  char * made = (char *)0x1;
  EXPECT_NE(GCU_FILE_OK,
      gcu_dir_temp_create(at("no/such").c_str(), "w", nullptr, &made));
  EXPECT_EQ(nullptr, made) << "the output must be cleared on failure";
}

TEST_F(Scratch, NullArgumentsAreRefusedRatherThanFatal) {
  EXPECT_EQ(GCU_FILE_ERR_INVALID, gcu_dir_create(nullptr));
  EXPECT_EQ(GCU_FILE_ERR_INVALID, gcu_dir_create(""));
  EXPECT_EQ(GCU_FILE_ERR_INVALID, gcu_dir_create_all(nullptr, nullptr));
  EXPECT_EQ(GCU_FILE_ERR_INVALID, gcu_dir_remove(nullptr));
  EXPECT_EQ(GCU_FILE_ERR_INVALID,
      gcu_dir_temp_create(nullptr, nullptr, nullptr, nullptr));
  EXPECT_EQ(GCU_FILE_ERR_INVALID, gcu_dir_open(nullptr, "x", nullptr));

  GCU_Dir d;
  EXPECT_EQ(GCU_FILE_ERR_INVALID, gcu_dir_open(&d, nullptr, nullptr));
  const char * name = nullptr;
  bool done = false;
  EXPECT_EQ(GCU_FILE_ERR_INVALID,
      gcu_dir_read(nullptr, &name, nullptr, &done));
  EXPECT_EQ(GCU_FILE_ERR_INVALID, gcu_dir_read(&d, &name, nullptr, &done));
  gcu_dir_free_path(nullptr, nullptr);
}


} // namespace

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
