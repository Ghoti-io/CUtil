#include <sstream>
#include <gtest/gtest.h>

// By defining GHOTIIO_CUTIL_ENABLE_MEMORY_DEBUG, we can intercept memory
// allocation and deallocation calls and write them to stderr. This is useful
// for debugging memory leaks and other memory-related issues.
#define GHOTIIO_CUTIL_ENABLE_MEMORY_DEBUG
#include <ghoti.io/cutil/memory.h>

using namespace std;

/// @cond HIDDEN_SYMBOLS
struct ParsedMalloc {
  string type;
  void * pointer;
  size_t count;
  size_t size;
  string file;
  size_t line;
};

struct ParsedCalloc {
  string type;
  void * pointer;
  size_t nitems;
  size_t count;
  size_t size;
  string file;
  size_t line;
};

struct ParsedRealloc {
  string type;
  void * pointerOld;
  size_t size;
  void * pointerNew;
  string file;
  size_t line;
};

struct ParsedFree {
  string type;
  void * pointer;
  size_t count;
  string file;
  size_t line;
};

ParsedMalloc parseMalloc(const string & str) {
  ParsedMalloc p{};
  stringstream ss{str};
  char ignore;
  ss >> p.type
    >> ignore
    >> p.count
    >> ignore
    >> hex >> p.pointer >> dec
    >> ignore
    >> p.size
    >> ignore;
  while (isspace(ss.peek())) {
    ss.ignore();
  }
  getline(ss, p.file, '(');
  ss >> p.line;
  return p;
}

ParsedCalloc parseCalloc(const string & str) {
  ParsedCalloc p{};
  stringstream ss{str};
  char ignore;
  ss >> p.type
    >> ignore
    >> p.count
    >> ignore
    >> hex >> p.pointer >> dec
    >> ignore
    >> p.nitems
    >> ignore
    >> p.size
    >> ignore;
  while (isspace(ss.peek())) {
    ss.ignore();
  }
  getline(ss, p.file, '(');
  ss >> p.line;
  return p;
}

ParsedRealloc parseRealloc(const string & str) {
  ParsedRealloc p{};
  stringstream ss{str};
  char ignore;
  ss >> p.type
    >> ignore
    >> hex >> p.pointerOld >> dec
    >> ignore
    >> p.size
    >> ignore >> ignore
    >> hex >> p.pointerNew >> dec
    >> ignore;
  while (isspace(ss.peek())) {
    ss.ignore();
  }
  getline(ss, p.file, '(');
  ss >> p.line;
  return p;
}

ParsedFree parseFree(const string & str) {
  ParsedFree p{};
  stringstream ss{str};
  char ignore;
  ss >> p.type
    >> ignore
    >> p.count
    >> ignore
    >> hex >> p.pointer >> dec
    >> ignore;
  while (isspace(ss.peek())) {
    ss.ignore();
  }
  getline(ss, p.file, '(');
  ss >> p.line;
  return p;
}
/// @endcond

TEST(Memory, MallocReallocFree) {
  void * buffer;
  size_t previous_alloc_count = gcu_get_alloc_count();
  size_t previous_free_count = gcu_get_free_count();
  {
    // Verify that the malloc is captured.
    testing::internal::CaptureStderr();
    buffer = gcu_malloc(1024);
    auto out = testing::internal::GetCapturedStderr();
    ParsedMalloc expected {
      .type = "malloc",
      .pointer = buffer,
      .count = 0, // will be ignored in test
      .size = 1024,
      .file = __FILE__,
      .line = __LINE__ - 8,
    };
    auto pMalloc = parseMalloc(out);
    ASSERT_EQ(pMalloc.type, expected.type);
    ASSERT_EQ(pMalloc.pointer, expected.pointer);
    ASSERT_EQ(pMalloc.size, expected.size);
    ASSERT_EQ(pMalloc.file, expected.file);
    ASSERT_EQ(pMalloc.line, expected.line);
  }
  // Verify that the counts are correct.
  ASSERT_EQ(gcu_get_alloc_count(), previous_alloc_count + 1);
  ASSERT_EQ(gcu_get_free_count(), previous_free_count);
  {
    // Verify that the realloc is captured.
    testing::internal::CaptureStderr();
    auto newBuffer = gcu_realloc(buffer, 100000);
    auto out = testing::internal::GetCapturedStderr();
    ParsedRealloc expected {
      .type = "realloc",
      .pointerOld = buffer,
      .size = 100000,
      .pointerNew = newBuffer,
      .file = __FILE__,
      .line = __LINE__ - 8,
    };
    buffer = newBuffer;
    auto pMalloc = parseRealloc(out);
    ASSERT_EQ(pMalloc.type, expected.type);
    ASSERT_EQ(pMalloc.pointerOld, expected.pointerOld);
    ASSERT_EQ(pMalloc.size, expected.size);
    ASSERT_EQ(pMalloc.pointerNew, expected.pointerNew);
    ASSERT_EQ(pMalloc.file, expected.file);
    ASSERT_EQ(pMalloc.line, expected.line);
  }
  // Verify that the counts are correct.
  ASSERT_EQ(gcu_get_alloc_count(), previous_alloc_count + 1);
  ASSERT_EQ(gcu_get_free_count(), previous_free_count);
  {
    // Verify that the free is captured.
    testing::internal::CaptureStderr();
    gcu_free(buffer);
    auto out = testing::internal::GetCapturedStderr();
    ParsedFree expected {
      .type = "free",
      .pointer = buffer,
      .count = 0, // will be ignored in test
      .file = __FILE__,
      .line = __LINE__ - 7,
    };
    auto pMalloc = parseFree(out);
    ASSERT_EQ(pMalloc.type, expected.type);
    ASSERT_EQ(pMalloc.pointer, expected.pointer);
    ASSERT_EQ(pMalloc.file, expected.file);
    ASSERT_EQ(pMalloc.line, expected.line);
  }
  // Verify that the counts are correct.
  ASSERT_EQ(gcu_get_alloc_count(), previous_alloc_count + 1);
  ASSERT_EQ(gcu_get_free_count(), previous_free_count + 1);
}


TEST(Memory, CallocFree) {
  void * buffer;
  size_t previous_alloc_count = gcu_get_alloc_count();
  size_t previous_free_count = gcu_get_free_count();
  {
    // Verify that the calloc is captured.
    testing::internal::CaptureStderr();
    buffer = gcu_calloc(4, 1024);
    auto out = testing::internal::GetCapturedStderr();
    ParsedCalloc expected {
      .type = "calloc",
      .pointer = buffer,
      .nitems = 4,
      .count = 0, // will be ignored in test
      .size = 1024,
      .file = __FILE__,
      .line = __LINE__ - 9,
    };
    auto pMalloc = parseCalloc(out);
    ASSERT_EQ(pMalloc.type, expected.type);
    ASSERT_EQ(pMalloc.pointer, expected.pointer);
    ASSERT_EQ(pMalloc.nitems, expected.nitems);
    ASSERT_EQ(pMalloc.size, expected.size);
    ASSERT_EQ(pMalloc.file, expected.file);
    ASSERT_EQ(pMalloc.line, expected.line);
  }
  // Verify that the counts are correct.
  ASSERT_EQ(gcu_get_alloc_count(), previous_alloc_count + 1);
  ASSERT_EQ(gcu_get_free_count(), previous_free_count);
  {
    // Verify that the free is captured.
    testing::internal::CaptureStderr();
    gcu_free(buffer);
    auto out = testing::internal::GetCapturedStderr();
    ParsedFree expected {
      .type = "free",
      .pointer = buffer,
      .count = 0, // will be ignored in test
      .file = __FILE__,
      .line = __LINE__ - 7,
    };
    auto pMalloc = parseFree(out);
    ASSERT_EQ(pMalloc.type, expected.type);
    ASSERT_EQ(pMalloc.pointer, expected.pointer);
    ASSERT_EQ(pMalloc.file, expected.file);
    ASSERT_EQ(pMalloc.line, expected.line);
  }
  // Verify that the counts are correct.
  ASSERT_EQ(gcu_get_alloc_count(), previous_alloc_count + 1);
  ASSERT_EQ(gcu_get_free_count(), previous_free_count + 1);
}

TEST(Memory, StopStartCapture) {
  size_t previous_alloc_count = gcu_get_alloc_count();
  size_t previous_free_count = gcu_get_free_count();
  {
    // Verify that stderr is being produced.
    testing::internal::CaptureStderr();
    auto buffer = gcu_malloc(1024);
    gcu_free(buffer);
    auto out = testing::internal::GetCapturedStderr();
    ASSERT_NE(out, "");
  }
  {
    // Verify that we can stop the writing to stderr.
    gcu_mem_stop();
    testing::internal::CaptureStderr();
    auto buffer = gcu_malloc(1024);
    gcu_free(buffer);
    auto out = testing::internal::GetCapturedStderr();
    ASSERT_EQ(out, "");
    gcu_mem_start();
  }
  {
    // Verify that we can resume the writing to stderr.
    testing::internal::CaptureStderr();
    auto buffer = gcu_malloc(1024);
    gcu_free(buffer);
    auto out = testing::internal::GetCapturedStderr();
    ASSERT_NE(out, "");
  }
  // Verify that the counts are correct.
  ASSERT_EQ(gcu_get_alloc_count(), previous_alloc_count + 3);
  ASSERT_EQ(gcu_get_free_count(), previous_free_count + 3);
}

// The three rules below are what make gcu_get_alloc_count() minus
// gcu_get_free_count() the number of blocks still outstanding.  They are
// stated once in memory.h and implemented twice -- here in the debug
// functions, and again in the inline versions the library itself is built
// with -- so each one is pinned on both sides.  test-memory-inline.cpp is the
// other side.

TEST(Memory, ReallocFromNullIsAnAllocation) {
  size_t previous_alloc_count = gcu_get_alloc_count();
  size_t previous_free_count = gcu_get_free_count();

  testing::internal::CaptureStderr();
  void * buffer = gcu_realloc(NULL, 32);
  auto out = testing::internal::GetCapturedStderr();

  ASSERT_NE(buffer, nullptr);
  // It allocated a block, so it is counted as an allocation...
  ASSERT_EQ(gcu_get_alloc_count(), previous_alloc_count + 1);
  ASSERT_EQ(gcu_get_free_count(), previous_free_count);
  // ...but the trace still reports the call the program actually made.
  ASSERT_EQ(out.substr(0, 7), "realloc");

  gcu_mem_stop();
  gcu_free(buffer);
  gcu_mem_start();

  // And the free that matches it now has an allocation to match.
  ASSERT_EQ(gcu_get_alloc_count(), previous_alloc_count + 1);
  ASSERT_EQ(gcu_get_free_count(), previous_free_count + 1);
}

TEST(Memory, ReallocToZeroKeepsTheBlock) {
  gcu_mem_stop();
  size_t previous_alloc_count = gcu_get_alloc_count();
  size_t previous_free_count = gcu_get_free_count();

  void * buffer = gcu_malloc(64);
  ASSERT_NE(buffer, nullptr);
  void * shrunk = gcu_realloc(buffer, 0);

  // glibc's realloc() would have released the block and handed back NULL,
  // which a caller cannot tell apart from failure and which would have left
  // the free uncounted.  The block survives instead.
  ASSERT_NE(shrunk, nullptr);
  ASSERT_EQ(gcu_get_alloc_count(), previous_alloc_count + 1);
  ASSERT_EQ(gcu_get_free_count(), previous_free_count);

  gcu_free(shrunk);
  ASSERT_EQ(gcu_get_free_count(), previous_free_count + 1);
  gcu_mem_start();
}

TEST(Memory, FreeOfNullIsNotCounted) {
  size_t previous_alloc_count = gcu_get_alloc_count();
  size_t previous_free_count = gcu_get_free_count();

  testing::internal::CaptureStderr();
  gcu_free(NULL);
  auto out = testing::internal::GetCapturedStderr();

  // Nothing was released, so nothing is counted.  Counting it would subtract
  // from the net and cancel a genuine leak of exactly the same size.
  ASSERT_EQ(gcu_get_alloc_count(), previous_alloc_count);
  ASSERT_EQ(gcu_get_free_count(), previous_free_count);
  // The call is still traced, because the log records what the program did.
  ASSERT_EQ(out.substr(0, 4), "free");
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

