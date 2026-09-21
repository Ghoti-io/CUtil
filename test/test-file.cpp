/**
 * @file
 *
 * Tests for the file module.
 *
 * Copyright 2026 by Corey Pennycuff
 */

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <gtest/gtest.h>
#include <ghoti.io/cutil/file.h>
#include <ghoti.io/cutil/path.h>

#ifndef _WIN32
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#endif

using namespace std;

namespace {

/** A scratch directory that removes itself and anything left in it. */
class Scratch : public testing::Test {
protected:
  string dir;

  void SetUp() override {
#ifndef _WIN32
    char * temp_root = nullptr;
    ASSERT_EQ(GCU_PATH_OK, gcu_path_temp_dir(nullptr, &temp_root));
    string tmpl = string(temp_root) + "/gcu-file-test-XXXXXX";
    gcu_path_free(nullptr, temp_root);
    vector<char> buffer(tmpl.begin(), tmpl.end());
    buffer.push_back('\0');
    ASSERT_NE(nullptr, mkdtemp(buffer.data()));
    dir = buffer.data();
#endif
  }

  void TearDown() override {
#ifndef _WIN32
    if (dir.empty()) {
      return;
    }
    if (DIR * d = opendir(dir.c_str())) {
      while (struct dirent * e = readdir(d)) {
        string name = e->d_name;
        if (name != "." && name != "..") {
          remove((dir + "/" + name).c_str());
        }
      }
      closedir(d);
    }
    rmdir(dir.c_str());
#endif
  }

  string at(const string & name) const { return dir + "/" + name; }

  /** Everything in the scratch directory, sorted. */
  vector<string> entries() const {
    vector<string> found;
#ifndef _WIN32
    if (DIR * d = opendir(dir.c_str())) {
      while (struct dirent * e = readdir(d)) {
        string name = e->d_name;
        if (name != "." && name != "..") {
          found.push_back(name);
        }
      }
      closedir(d);
    }
#endif
    sort(found.begin(), found.end());
    return found;
  }
};

void put(const string & path, const string & bytes) {
  FILE * f = fopen(path.c_str(), "wb");
  ASSERT_NE(nullptr, f);
  if (!bytes.empty()) {
    ASSERT_EQ(bytes.size(), fwrite(bytes.data(), 1, bytes.size(), f));
  }
  ASSERT_EQ(0, fclose(f));
}

/** Read through the module under test, returning content and length. */
struct Read {
  void * data = nullptr;
  size_t len = 0;
  GCU_File_Result result = GCU_FILE_OK;

  explicit Read(const string & path, size_t max = GCU_FILE_UNLIMITED) {
    result = gcu_file_read(path.c_str(), max, nullptr, &data, &len);
  }
  ~Read() { gcu_file_free(nullptr, data); }
  string str() const {
    return data ? string((const char *)data, len) : string();
  }
  const char * chars() const { return (const char *)data; }
};

bool exists(const string & path) {
  struct stat info;
  return stat(path.c_str(), &info) == 0;
}

} // namespace

//////////////////////////////////////////////////////////////////////////////
// Reading
//////////////////////////////////////////////////////////////////////////////

TEST_F(Scratch, ReadReturnsEveryByteAndAddsATerminatorPastTheEnd) {
  put(at("f"), "hello world");
  Read r(at("f"));
  ASSERT_EQ(GCU_FILE_OK, r.result);
  EXPECT_EQ(11u, r.len);
  EXPECT_EQ("hello world", r.str());
  // The terminator is past the length, so a text caller may use the buffer as
  // a C string and a binary caller can ignore it.
  EXPECT_EQ('\0', r.chars()[r.len]);
  EXPECT_STREQ("hello world", r.chars());
}

TEST_F(Scratch, ReadOfAnEmptyFileSucceedsWithZeroLength) {
  put(at("empty"), "");
  Read r(at("empty"));
  ASSERT_EQ(GCU_FILE_OK, r.result);
  EXPECT_EQ(0u, r.len);
  ASSERT_NE(nullptr, r.data) << "a zero-length read must still give a buffer";
  EXPECT_EQ('\0', r.chars()[0]);
}

TEST_F(Scratch, ReadKeepsEmbeddedNulBytes) {
  // The added terminator must not be mistaken for the end of the content.
  string binary("a\0b\0\0c", 6);
  put(at("bin"), binary);
  Read r(at("bin"));
  ASSERT_EQ(GCU_FILE_OK, r.result);
  EXPECT_EQ(6u, r.len);
  EXPECT_EQ(binary, r.str());
}

TEST_F(Scratch, ReadCrossesTheChunkBoundaryItGrowsAt) {
  // Larger than one read, so the growth path runs rather than being skipped.
  string big;
  for (int i = 0; i < 40000; ++i) {
    big.push_back((char)('a' + (i % 26)));
  }
  put(at("big"), big);
  Read r(at("big"));
  ASSERT_EQ(GCU_FILE_OK, r.result);
  EXPECT_EQ(big.size(), r.len);
  EXPECT_EQ(big, r.str());
}

TEST(FileRead, WorksOnAFileThatReportsNoSize) {
#ifdef __linux__
  // Everything under /proc reports a size of zero.  An implementation that
  // seeks to the end to size the file first comes back with nothing at all,
  // which is why this one reads in chunks instead.
  Read r("/proc/version");
  ASSERT_EQ(GCU_FILE_OK, r.result);
  EXPECT_GT(r.len, 0u) << "read nothing from a file that reports no size";
  EXPECT_NE(string::npos, r.str().find("Linux"));
#endif
}

TEST_F(Scratch, ReadOfAMissingFileFailsAndAllocatesNothing) {
  void * data = (void *)0x1;
  size_t len = 99;
  EXPECT_EQ(GCU_FILE_ERR_IO,
      gcu_file_read(at("nope").c_str(), GCU_FILE_UNLIMITED, nullptr, &data,
          &len));
  EXPECT_EQ((void *)0x1, data) << "outputs must be untouched on failure";
  EXPECT_EQ(99u, len);
}

TEST_F(Scratch, ReadRefusesAFileOverTheLimitRatherThanTruncating) {
  put(at("f"), "0123456789");
  void * data = nullptr;
  size_t len = 0;
  // A limit is a promise.  Handing back the first five bytes would be a
  // different file that still looks like a file.
  EXPECT_EQ(GCU_FILE_ERR_LIMIT,
      gcu_file_read(at("f").c_str(), 5, nullptr, &data, &len));
  EXPECT_EQ(nullptr, data);
}

TEST_F(Scratch, ReadAcceptsAFileExactlyAtTheLimit) {
  put(at("f"), "0123456789");
  Read exact(at("f"), 10);
  EXPECT_EQ(GCU_FILE_OK, exact.result);
  EXPECT_EQ(10u, exact.len);

  // One byte under is one byte too many.
  Read over(at("f"), 9);
  EXPECT_EQ(GCU_FILE_ERR_LIMIT, over.result);

  // And an empty file passes a limit of zero bytes... which means unlimited.
  Read unlimited(at("f"), GCU_FILE_UNLIMITED);
  EXPECT_EQ(GCU_FILE_OK, unlimited.result);
  EXPECT_EQ(10u, unlimited.len);
}

TEST_F(Scratch, ReadOfADirectoryIsAnError) {
  void * data = nullptr;
  size_t len = 0;
  EXPECT_NE(GCU_FILE_OK,
      gcu_file_read(dir.c_str(), GCU_FILE_UNLIMITED, nullptr, &data, &len));
  EXPECT_EQ(nullptr, data);
}

//////////////////////////////////////////////////////////////////////////////
// Temporary files
//////////////////////////////////////////////////////////////////////////////

TEST_F(Scratch, TempCreateOpensAFileInTheDirectoryItWasGiven) {
  GCU_File_Temp temp;
  ASSERT_EQ(GCU_FILE_OK,
      gcu_file_temp_create(&temp, dir.c_str(), "pre", nullptr));
  string path = gcu_file_temp_path(&temp);
  EXPECT_NE(nullptr, gcu_file_temp_stream(&temp));
  // Being in the destination's directory is what makes the later rename
  // atomic, so it is a property worth asserting rather than assuming.
  EXPECT_EQ(dir, path.substr(0, dir.size()));
  EXPECT_NE(string::npos, path.find("/pre"));
  EXPECT_TRUE(exists(path));
  gcu_file_temp_abort(&temp);
}

TEST_F(Scratch, TempCreateOpensAFileOnlyItsOwnerCanRead) {
#ifndef _WIN32
  GCU_File_Temp temp;
  ASSERT_EQ(GCU_FILE_OK,
      gcu_file_temp_create(&temp, dir.c_str(), nullptr, nullptr));
  struct stat info;
  ASSERT_EQ(0, stat(gcu_file_temp_path(&temp), &info));
  // A temporary file that anyone can read is a way to leak whatever is being
  // written through it.
  EXPECT_EQ(0, info.st_mode & (S_IRWXG | S_IRWXO))
      << "mode was " << oct << (info.st_mode & 07777);
  gcu_file_temp_abort(&temp);
#endif
}

TEST_F(Scratch, TempCreateProducesADifferentNameEachTime) {
  GCU_File_Temp a;
  GCU_File_Temp b;
  ASSERT_EQ(GCU_FILE_OK, gcu_file_temp_create(&a, dir.c_str(), "x", nullptr));
  ASSERT_EQ(GCU_FILE_OK, gcu_file_temp_create(&b, dir.c_str(), "x", nullptr));
  EXPECT_STRNE(gcu_file_temp_path(&a), gcu_file_temp_path(&b));
  gcu_file_temp_abort(&a);
  gcu_file_temp_abort(&b);
}

TEST_F(Scratch, TempCreateWithoutADirectoryUsesTheSystemTemporaryDirectory) {
  char * expected = nullptr;
  ASSERT_EQ(GCU_PATH_OK, gcu_path_temp_dir(nullptr, &expected));
  GCU_File_Temp temp;
  ASSERT_EQ(GCU_FILE_OK,
      gcu_file_temp_create(&temp, nullptr, "scratch", nullptr));
  string path = gcu_file_temp_path(&temp);
  EXPECT_EQ(string(expected), path.substr(0, strlen(expected)));
  gcu_path_free(nullptr, expected);
  gcu_file_temp_abort(&temp);
}

TEST_F(Scratch, TempAbortRemovesTheFileAndEmptiesTheHandle) {
  GCU_File_Temp temp;
  ASSERT_EQ(GCU_FILE_OK,
      gcu_file_temp_create(&temp, dir.c_str(), "t", nullptr));
  string path = gcu_file_temp_path(&temp);
  ASSERT_TRUE(exists(path));

  gcu_file_temp_abort(&temp);
  EXPECT_FALSE(exists(path)) << "abort left the temporary file behind";
  EXPECT_EQ(nullptr, gcu_file_temp_path(&temp));
  EXPECT_EQ(nullptr, gcu_file_temp_stream(&temp));
  EXPECT_TRUE(entries().empty());
}

TEST_F(Scratch, TempAbortIsSafeOnAHandleThatWasNeverOpenedOrIsAlreadySpent) {
  // This is the property that lets a caller put abort on an unconditional
  // cleanup path without tracking whether commit already ran - which is how
  // the hand-written copies this replaces came to forget the cleanup on one
  // branch and not the other.
  gcu_file_temp_abort(nullptr);

  GCU_File_Temp zeroed;
  memset(&zeroed, 0, sizeof zeroed);
  gcu_file_temp_abort(&zeroed);

  GCU_File_Temp failed;
  EXPECT_NE(GCU_FILE_OK,
      gcu_file_temp_create(&failed, "/no/such/directory", "t", nullptr));
  gcu_file_temp_abort(&failed);

  GCU_File_Temp twice;
  ASSERT_EQ(GCU_FILE_OK,
      gcu_file_temp_create(&twice, dir.c_str(), "t", nullptr));
  gcu_file_temp_abort(&twice);
  gcu_file_temp_abort(&twice);

  GCU_File_Temp committed;
  ASSERT_EQ(GCU_FILE_OK,
      gcu_file_temp_create(&committed, dir.c_str(), "t", nullptr));
  ASSERT_EQ(GCU_FILE_OK,
      gcu_file_temp_commit(&committed, at("done").c_str(),
          GCU_FILE_SYNC_FULL));
  gcu_file_temp_abort(&committed);
  EXPECT_TRUE(exists(at("done"))) << "abort after commit deleted the result";
}

TEST_F(Scratch, TempCommitMovesTheContentIntoPlaceAndLeavesNoTemporary) {
  GCU_File_Temp temp;
  ASSERT_EQ(GCU_FILE_OK,
      gcu_file_temp_create(&temp, dir.c_str(), "t", nullptr));
  string temp_path = gcu_file_temp_path(&temp);
  ASSERT_EQ(5u, fwrite("abcde", 1, 5, gcu_file_temp_stream(&temp)));

  ASSERT_EQ(GCU_FILE_OK,
      gcu_file_temp_commit(&temp, at("out").c_str(), GCU_FILE_SYNC_FULL));

  EXPECT_FALSE(exists(temp_path));
  Read r(at("out"));
  ASSERT_EQ(GCU_FILE_OK, r.result);
  EXPECT_EQ("abcde", r.str());
  EXPECT_EQ(vector<string>{"out"}, entries());
}

TEST_F(Scratch, TempCommitReplacesAFileThatAlreadyExists) {
  put(at("out"), "the old contents, which are longer");
  GCU_File_Temp temp;
  ASSERT_EQ(GCU_FILE_OK,
      gcu_file_temp_create(&temp, dir.c_str(), "t", nullptr));
  ASSERT_EQ(3u, fwrite("new", 1, 3, gcu_file_temp_stream(&temp)));
  ASSERT_EQ(GCU_FILE_OK,
      gcu_file_temp_commit(&temp, at("out").c_str(), GCU_FILE_SYNC_FULL));

  Read r(at("out"));
  ASSERT_EQ(GCU_FILE_OK, r.result);
  EXPECT_EQ("new", r.str()) << "the old content survived the replacement";
  EXPECT_EQ(vector<string>{"out"}, entries());
}

TEST_F(Scratch, CommitIsRefusedOnAHandleWithNothingOpen) {
  GCU_File_Temp zeroed;
  memset(&zeroed, 0, sizeof zeroed);
  EXPECT_EQ(GCU_FILE_ERR_INVALID,
      gcu_file_temp_commit(&zeroed, at("x").c_str(), GCU_FILE_SYNC_FULL));
  EXPECT_EQ(GCU_FILE_ERR_INVALID,
      gcu_file_temp_commit(nullptr, at("x").c_str(), GCU_FILE_SYNC_FULL));

  GCU_File_Temp temp;
  ASSERT_EQ(GCU_FILE_OK,
      gcu_file_temp_create(&temp, dir.c_str(), "t", nullptr));
  EXPECT_EQ(GCU_FILE_ERR_INVALID,
      gcu_file_temp_commit(&temp, nullptr, GCU_FILE_SYNC_FULL));
  gcu_file_temp_abort(&temp);
}

TEST_F(Scratch, CommitIntoAMissingDirectoryFailsAndRemovesTheTemporary) {
  // The temporary file is created successfully and it is the *rename* that
  // fails, which is the only path on which commit has litter of its own to
  // clean up.  Nothing else in this suite reaches it.
  GCU_File_Temp temp;
  ASSERT_EQ(GCU_FILE_OK,
      gcu_file_temp_create(&temp, dir.c_str(), "t", nullptr));
  string temp_path = gcu_file_temp_path(&temp);
  ASSERT_EQ(3u, fwrite("abc", 1, 3, gcu_file_temp_stream(&temp)));

  EXPECT_EQ(GCU_FILE_ERR_IO,
      gcu_file_temp_commit(&temp, (dir + "/no/such/dir/out").c_str(),
          GCU_FILE_SYNC_FULL));

  EXPECT_FALSE(exists(temp_path)) << "a failed commit left its temporary";
  EXPECT_TRUE(entries().empty());
  // Spent either way, so an unconditional abort afterwards is still safe.
  EXPECT_EQ(nullptr, gcu_file_temp_path(&temp));
  gcu_file_temp_abort(&temp);
}

//////////////////////////////////////////////////////////////////////////////
// write_atomic
//////////////////////////////////////////////////////////////////////////////

TEST_F(Scratch, WriteAtomicRoundTripsAndLeavesNothingBehind) {
  string payload("some\0bytes", 10);
  ASSERT_EQ(GCU_FILE_OK,
      gcu_file_write_atomic(at("out").c_str(), payload.data(), payload.size(),
          GCU_FILE_SYNC_FULL, nullptr));
  Read r(at("out"));
  ASSERT_EQ(GCU_FILE_OK, r.result);
  EXPECT_EQ(payload, r.str());
  // A temporary left in the directory is how the copy this replaces failed.
  EXPECT_EQ(vector<string>{"out"}, entries());
}

TEST_F(Scratch, WriteAtomicReplacesAnExistingFileAndAcceptsZeroLength) {
  put(at("out"), "previous");
  ASSERT_EQ(GCU_FILE_OK,
      gcu_file_write_atomic(at("out").c_str(), nullptr, 0,
          GCU_FILE_SYNC_FULL, nullptr));
  Read r(at("out"));
  ASSERT_EQ(GCU_FILE_OK, r.result);
  EXPECT_EQ(0u, r.len);
  EXPECT_EQ(vector<string>{"out"}, entries());
}

TEST_F(Scratch, WriteAtomicWritesTheContentWithoutSyncingToo) {
  // SYNC_NONE changes only the durability promise, never the content.
  ASSERT_EQ(GCU_FILE_OK,
      gcu_file_write_atomic(at("out").c_str(), "xyz", 3, GCU_FILE_SYNC_NONE,
          nullptr));
  Read r(at("out"));
  ASSERT_EQ(GCU_FILE_OK, r.result);
  EXPECT_EQ("xyz", r.str());
}

TEST_F(Scratch, WriteAtomicIntoAMissingDirectoryFailsAndLeavesNoLitter) {
  EXPECT_NE(GCU_FILE_OK,
      gcu_file_write_atomic((dir + "/no/such/dir/out").c_str(), "x", 1,
          GCU_FILE_SYNC_FULL, nullptr));
  EXPECT_TRUE(entries().empty());
}

TEST_F(Scratch, WriteAtomicDoesNotUseTheSystemTemporaryDirectory) {
#ifndef _WIN32
  // The temporary must be created beside the destination, because a rename
  // between filesystems is a copy and a copy is not atomic.  Pointing $TMPDIR
  // at nothing makes the difference observable: a version that reached for
  // the system temporary directory cannot create its file at all, while the
  // correct one never looks there.
  const char * saved = getenv("TMPDIR");
  string restore = saved ? saved : "";
  setenv("TMPDIR", "/no/such/temporary/directory", 1);

  GCU_File_Result result = gcu_file_write_atomic(at("out").c_str(), "abc", 3,
      GCU_FILE_SYNC_FULL, nullptr);

  if (saved) {
    setenv("TMPDIR", restore.c_str(), 1);
  }
  else {
    unsetenv("TMPDIR");
  }

  ASSERT_EQ(GCU_FILE_OK, result);
  Read r(at("out"));
  ASSERT_EQ(GCU_FILE_OK, r.result);
  EXPECT_EQ("abc", r.str());
  EXPECT_EQ(vector<string>{"out"}, entries());
#endif
}

TEST_F(Scratch, NullArgumentsAreRefusedRatherThanFatal) {
  void * data = nullptr;
  size_t len = 0;
  EXPECT_EQ(GCU_FILE_ERR_INVALID,
      gcu_file_read(nullptr, GCU_FILE_UNLIMITED, nullptr, &data, &len));
  EXPECT_EQ(GCU_FILE_ERR_INVALID,
      gcu_file_read(at("x").c_str(), GCU_FILE_UNLIMITED, nullptr, nullptr,
          &len));
  EXPECT_EQ(GCU_FILE_ERR_INVALID,
      gcu_file_read(at("x").c_str(), GCU_FILE_UNLIMITED, nullptr, &data,
          nullptr));
  EXPECT_EQ(GCU_FILE_ERR_INVALID,
      gcu_file_temp_create(nullptr, nullptr, nullptr, nullptr));
  EXPECT_EQ(GCU_FILE_ERR_INVALID,
      gcu_file_write_atomic(nullptr, "x", 1, GCU_FILE_SYNC_FULL, nullptr));
  // Bytes may only be NULL when there are none of them.
  EXPECT_EQ(GCU_FILE_ERR_INVALID,
      gcu_file_write_atomic(at("x").c_str(), nullptr, 5, GCU_FILE_SYNC_FULL,
          nullptr));
  EXPECT_EQ(nullptr, gcu_file_temp_stream(nullptr));
  EXPECT_EQ(nullptr, gcu_file_temp_path(nullptr));
  gcu_file_free(nullptr, nullptr);
}

TEST(FileResultString, NamesEveryValueAndRefusesNone) {
  for (int i = 0; i < GCU_FILE_RESULT_COUNT; ++i) {
    const char * text = gcu_file_result_string((GCU_File_Result)i);
    ASSERT_NE(nullptr, text) << "result " << i;
    EXPECT_STRNE("unknown", text) << "result " << i << " has no name";
  }
  EXPECT_STREQ("unknown", gcu_file_result_string((GCU_File_Result)999));
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
