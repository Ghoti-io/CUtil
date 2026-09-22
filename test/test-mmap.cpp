#include <cstdio>
#include <cstring>
#include <string>
#include <gtest/gtest.h>
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <ghoti.io/cutil/mmap.h>

using namespace std;

#ifndef GCU_TEST_MMAP_DIR
#error "GCU_TEST_MMAP_DIR must name a writable directory; see the Makefile"
#endif

namespace {

string pathFor(const char * name) {
  return string(GCU_TEST_MMAP_DIR) + "/mmap-" + name + ".tmp";
}

void writeFile(const string & path, const string & contents) {
  FILE * f = fopen(path.c_str(), "wb");
  ASSERT_NE(nullptr, f);
  if (!contents.empty()) {
    ASSERT_EQ(contents.size(), fwrite(contents.data(), 1, contents.size(), f));
  }
  ASSERT_EQ(0, fclose(f));
}

string readFile(const string & path) {
  FILE * f = fopen(path.c_str(), "rb");
  EXPECT_NE(nullptr, f);
  if (!f) {
    return "";
  }
  string out;
  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
    out.append(buf, n);
  }
  fclose(f);
  return out;
}

} // namespace

TEST(Mmap, ReadsTheFileContents) {
  string path = pathFor("read");
  writeFile(path, "hello mapped world");

  GCU_Mapped_File map;
  ASSERT_EQ(0, gcu_mmap_open(&map, path.c_str(), false));
  ASSERT_NE(nullptr, map.data);
  ASSERT_EQ(18u, map.size);
  ASSERT_EQ(0, memcmp(map.data, "hello mapped world", 18));
  ASSERT_EQ(0, gcu_mmap_close(&map));
  remove(path.c_str());
}

TEST(Mmap, AnEmptyFileMapsToNothingAndSucceeds) {
  // Documented: mmap cannot map zero bytes and CreateFileMapping refuses a
  // zero-length file, so this would naturally be an error. It is not one,
  // because "the file is empty" is an ordinary answer.
  string path = pathFor("empty");
  writeFile(path, "");

  GCU_Mapped_File map;
  ASSERT_EQ(0, gcu_mmap_open(&map, path.c_str(), false));
  ASSERT_EQ(nullptr, map.data);
  ASSERT_EQ(0u, map.size);
  ASSERT_EQ(0, gcu_mmap_sync(&map)) << "sync on an empty mapping must be a no-op";
  ASSERT_EQ(0, gcu_mmap_close(&map));
  // An empty mapping is still a mapping that was opened, so closing it twice
  // is refused exactly as for a non-empty one. Making the empty case release
  // its handle immediately would leave this struct byte-identical to a closed
  // one, and the second close would report success.
  ASSERT_EQ(-1, gcu_mmap_close(&map));
  remove(path.c_str());
}

TEST(Mmap, AMissingFileIsNotCreated) {
  string path = pathFor("absent");
  remove(path.c_str());

  GCU_Mapped_File map;
  ASSERT_EQ(-1, gcu_mmap_open(&map, path.c_str(), false));

  FILE * f = fopen(path.c_str(), "rb");
  ASSERT_EQ(nullptr, f) << "a failed map created the file";
}

TEST(Mmap, ADirectoryIsRefused) {
  // mmap of a directory fails anyway; the explicit S_ISREG check is what also
  // refuses a character device, which would map happily and report size 0 --
  // indistinguishable from an empty file.
  GCU_Mapped_File map;
  ASSERT_EQ(-1, gcu_mmap_open(&map, GCU_TEST_MMAP_DIR, false));
}

#ifndef _WIN32
TEST(Mmap, ACharacterDeviceIsRefusedRatherThanLookingEmpty) {
  GCU_Mapped_File map;
  ASSERT_EQ(-1, gcu_mmap_open(&map, "/dev/zero", false))
      << "/dev/zero mapped as though it were an empty file";
}
#endif

TEST(Mmap, WritesThroughTheMappingReachTheFile) {
  string path = pathFor("write");
  writeFile(path, "AAAAAAAAAA");

  GCU_Mapped_File map;
  ASSERT_EQ(0, gcu_mmap_open(&map, path.c_str(), true));
  ASSERT_EQ(10u, map.size);
  memcpy(map.data, "BBBBB", 5);
  // MS_ASYNC vs MS_SYNC is NOT pinned by this, and no test here can pin it:
  // both put the bytes in the page cache, so a subsequent read() sees them
  // either way. The two differ only across a machine crash. Mutation
  // confirmed: switching to MS_ASYNC leaves the whole suite green. MS_SYNC
  // stays because the only reason to call this function is to know the bytes
  // have landed, and MS_ASYNC returns before they have.
  ASSERT_EQ(0, gcu_mmap_sync(&map));
  ASSERT_EQ(0, gcu_mmap_close(&map));

  ASSERT_EQ("BBBBBAAAAA", readFile(path));
  remove(path.c_str());
}

TEST(Mmap, ClosingFlushesWithoutAnExplicitSync) {
  string path = pathFor("closeflush");
  writeFile(path, "0123456789");

  GCU_Mapped_File map;
  ASSERT_EQ(0, gcu_mmap_open(&map, path.c_str(), true));
  memcpy(map.data, "xxxx", 4);
  ASSERT_EQ(0, gcu_mmap_close(&map));

  ASSERT_EQ("xxxx456789", readFile(path));
  remove(path.c_str());
}

#ifndef _WIN32
TEST(Mmap, AskingToWriteAFileYouCannotWriteFails) {
  // The checkable half of "writable means writable". Asserting that a store
  // into a read-only mapping faults is not available: it raises SIGSEGV and
  // kills the process rather than failing a test. This asks the question the
  // other way round -- a writable open must go through O_RDWR and therefore
  // must be refused by the permissions.
  if (geteuid() == 0) {
    GTEST_SKIP() << "running as root; file permissions do not apply";
  }
  string path = pathFor("perm");
  writeFile(path, "read only");
  ASSERT_EQ(0, chmod(path.c_str(), 0444));

  GCU_Mapped_File map;
  ASSERT_EQ(-1, gcu_mmap_open(&map, path.c_str(), true))
      << "opened a read-only file for writing";
  // ...and read-only still works on the same file.
  ASSERT_EQ(0, gcu_mmap_open(&map, path.c_str(), false));
  ASSERT_EQ(0, gcu_mmap_close(&map));

  chmod(path.c_str(), 0644);
  remove(path.c_str());
}
#endif

TEST(Mmap, AReadOnlyMappingIsReadOnly) {
  // Not asserting that a store faults -- that would kill the process rather
  // than fail a test. Asserting the weaker, checkable thing: the file is
  // unchanged after a read-only mapping's lifetime.
  string path = pathFor("readonly");
  writeFile(path, "unchanged");

  GCU_Mapped_File map;
  ASSERT_EQ(0, gcu_mmap_open(&map, path.c_str(), false));
  ASSERT_EQ(0, gcu_mmap_sync(&map)) << "sync on a read-only mapping must succeed";
  ASSERT_EQ(0, gcu_mmap_close(&map));

  ASSERT_EQ("unchanged", readFile(path));
  remove(path.c_str());
}

TEST(Mmap, CloseClearsTheMappingSoASecondCloseIsRefused) {
  string path = pathFor("double");
  writeFile(path, "data");

  GCU_Mapped_File map;
  ASSERT_EQ(0, gcu_mmap_open(&map, path.c_str(), false));
  ASSERT_EQ(0, gcu_mmap_close(&map));
  // Cleared, so a second close is refused here rather than handed to munmap
  // with a stale pointer -- which would unmap whatever now lives there.
  ASSERT_EQ(nullptr, map.data);
  ASSERT_EQ(0u, map.size);
  ASSERT_EQ(-1, gcu_mmap_close(&map));
  remove(path.c_str());
}

TEST(Mmap, NullArgumentsAreRejectedNotDereferenced) {
  string path = pathFor("nulls");
  writeFile(path, "x");
  GCU_Mapped_File map;
  ASSERT_EQ(-1, gcu_mmap_open(nullptr, path.c_str(), false));
  ASSERT_EQ(-1, gcu_mmap_open(&map, nullptr, false));
  ASSERT_EQ(-1, gcu_mmap_close(nullptr));
  ASSERT_EQ(-1, gcu_mmap_sync(nullptr));
  remove(path.c_str());
}

TEST(Mmap, ALargeFileMapsEntirely) {
  // Big enough to span many pages, so a size taken from the wrong place --
  // one page, or the page-rounded length -- shows up.
  string path = pathFor("large");
  string contents;
  contents.reserve(1 << 20);
  for (int i = 0; i < (1 << 20); ++i) {
    contents.push_back(static_cast<char>(i & 0xFF));
  }
  writeFile(path, contents);

  GCU_Mapped_File map;
  ASSERT_EQ(0, gcu_mmap_open(&map, path.c_str(), false));
  ASSERT_EQ(contents.size(), map.size);
  ASSERT_EQ(0, memcmp(map.data, contents.data(), contents.size()));
  // The last byte specifically: a mapping one page short still passes a
  // comparison that stops early.
  ASSERT_EQ(contents.back(), static_cast<const char *>(map.data)[map.size - 1]);
  ASSERT_EQ(0, gcu_mmap_close(&map));
  remove(path.c_str());
}

TEST(Mmap, ANonPageMultipleSizeIsReportedExactly) {
  // The file is 4097 bytes: one byte past a page. The reported size must be
  // the file's, not the mapping's rounded-up length.
  string path = pathFor("odd");
  string contents(4097, 'z');
  contents[4096] = 'Q';
  writeFile(path, contents);

  GCU_Mapped_File map;
  ASSERT_EQ(0, gcu_mmap_open(&map, path.c_str(), false));
  ASSERT_EQ(4097u, map.size) << "size was rounded to a page boundary";
  ASSERT_EQ('Q', static_cast<const char *>(map.data)[4096]);
  ASSERT_EQ(0, gcu_mmap_close(&map));
  remove(path.c_str());
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
