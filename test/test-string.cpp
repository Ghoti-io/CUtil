#include <cstdint>
#include <cstring>
#include <sstream>
#include <gtest/gtest.h>
#include <ghoti.io/cutil/string.h>

using namespace std;

TEST(Murmur3, SeedCheck) {
  // Verify that changing the seed will result in a different hash for empty
  // strings.
  {
    uint32_t out1, out2;
    gcu_string_murmur3_32("", 0, 0, &out1);
    gcu_string_murmur3_32("", 0, 1, &out2);
    ASSERT_NE(out1, out2);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x86_128("", 0, 0, &out1);
    gcu_string_murmur3_x86_128("", 0, 1, &out2);
    ASSERT_NE(out1[0], out2[0]);
    ASSERT_NE(out1[1], out2[1]);
    ASSERT_EQ(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x64_128("", 0, 0, &out1);
    gcu_string_murmur3_x64_128("", 0, 1, &out2);
    ASSERT_NE(out1[0], out2[0]);
    ASSERT_NE(out1[1], out2[1]);
    ASSERT_EQ(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
  // Verify that changing the seed will result in a different hash for a
  // non-empty string.
  {
    uint32_t out1, out2;
    gcu_string_murmur3_32("hello", 5, 0, &out1);
    gcu_string_murmur3_32("hello", 5, 1, &out2);
    ASSERT_NE(out1, out2);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x86_128("hello", 5, 0, &out1);
    gcu_string_murmur3_x86_128("hello", 5, 1, &out2);
    ASSERT_NE(out1[0], out2[0]);
    ASSERT_NE(out1[1], out2[1]);
    ASSERT_NE(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x64_128("hello", 5, 0, &out1);
    gcu_string_murmur3_x64_128("hello", 5, 1, &out2);
    ASSERT_NE(out1[0], out2[0]);
    ASSERT_NE(out1[1], out2[1]);
    ASSERT_NE(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
}

TEST(Murmur3, HashCheck) {
  // Verify that an empty string has a different hash from a string containing
  // a null character.
  {
    uint32_t out1, out2;
    gcu_string_murmur3_32("", 0, 0, &out1);
    gcu_string_murmur3_32("\0", 1, 0, &out2);
    ASSERT_NE(out1, out2);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x86_128("", 0, 0, &out1);
    gcu_string_murmur3_x86_128("\0", 1, 0, &out2);
    ASSERT_NE(out1[0], out2[0]);
    ASSERT_NE(out1[1], out2[1]);
    ASSERT_EQ(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x64_128("", 0, 0, &out1);
    gcu_string_murmur3_x64_128("\0", 1, 0, &out2);
    ASSERT_NE(out1[0], out2[0]);
    ASSERT_NE(out1[1], out2[1]);
    ASSERT_EQ(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
  // Verify that different strings have different hashes.
  {
    uint32_t out1, out2;
    gcu_string_murmur3_32("foo", 3, 0, &out1);
    gcu_string_murmur3_32("bar", 3, 0, &out2);
    ASSERT_NE(out1, out2);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x86_128("foo", 3, 0, &out1);
    gcu_string_murmur3_x86_128("bar", 3, 0, &out2);
    ASSERT_NE(out1[0], out2[0]);
    ASSERT_NE(out1[1], out2[1]);
    ASSERT_NE(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x64_128("foo", 3, 0, &out1);
    gcu_string_murmur3_x64_128("bar", 3, 0, &out2);
    ASSERT_NE(out1[0], out2[0]);
    ASSERT_NE(out1[1], out2[1]);
    ASSERT_NE(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
}

TEST(Murmur3, Length) {
  // Verify that the length is respected (first two characters matching).
  {
    uint32_t out1, out2;
    gcu_string_murmur3_32("bar", 2, 0, &out1);
    gcu_string_murmur3_32("baz", 2, 0, &out2);
    ASSERT_EQ(out1, out2);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x86_128("bar", 2, 0, &out1);
    gcu_string_murmur3_x86_128("baz", 2, 0, &out2);
    ASSERT_EQ(out1[0], out2[0]);
    ASSERT_EQ(out1[1], out2[1]);
    ASSERT_NE(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x64_128("bar", 2, 0, &out1);
    gcu_string_murmur3_x64_128("baz", 2, 0, &out2);
    ASSERT_EQ(out1[0], out2[0]);
    ASSERT_EQ(out1[1], out2[1]);
    ASSERT_NE(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
  // Verify that the length is respected (first two characters matching).
  {
    uint32_t out1, out2;
    gcu_string_murmur3_32("bar", 3, 0, &out1);
    gcu_string_murmur3_32("baz", 3, 0, &out2);
    ASSERT_NE(out1, out2);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x86_128("bar", 3, 0, &out1);
    gcu_string_murmur3_x86_128("baz", 3, 0, &out2);
    ASSERT_NE(out1[0], out2[0]);
    ASSERT_NE(out1[1], out2[1]);
    ASSERT_NE(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
  {
    uint64_t out1[2], out2[2];
    gcu_string_murmur3_x64_128("bar", 3, 0, &out1);
    gcu_string_murmur3_x64_128("baz", 3, 0, &out2);
    ASSERT_NE(out1[0], out2[0]);
    ASSERT_NE(out1[1], out2[1]);
    ASSERT_NE(out1[0], out1[1]);
    ASSERT_NE(out2[0], out2[1]);
  }
}

TEST(Hash, HelperFunction32) {
  ASSERT_EQ(gcu_string_hash_32("", 0), gcu_string_hash_32("", 0));
  ASSERT_NE(gcu_string_hash_32("", 0), gcu_string_hash_32("\0", 1));
  ASSERT_EQ(gcu_string_hash_32("a", 1), gcu_string_hash_32("a", 1));
  ASSERT_NE(gcu_string_hash_32("a", 1), gcu_string_hash_32("b", 1));
  ASSERT_NE(gcu_string_hash_32("hello world!hello world!hello world!hello world!", 48), gcu_string_hash_32("Hello World!Hello World!Hello World!Hello World!", 48));
}

TEST(Hash, HelperFunction64) {
  ASSERT_EQ(gcu_string_hash_64("", 0), gcu_string_hash_64("", 0));
  ASSERT_NE(gcu_string_hash_64("", 0), gcu_string_hash_64("\0", 1));
  ASSERT_EQ(gcu_string_hash_64("a", 1), gcu_string_hash_64("a", 1));
  ASSERT_NE(gcu_string_hash_64("a", 1), gcu_string_hash_64("b", 1));
  ASSERT_NE(gcu_string_hash_64("hello world!hello world!hello world!hello world!", 48), gcu_string_hash_64("Hello World!Hello World!Hello World!Hello World!", 48));
}

// Every other assertion in this file is self-consistency: a hash equals
// itself, and differs from a neighbour's.  An implementation with a wrong
// rotation constant, a wrong finalizer or wrong tail handling satisfies all
// of them, which is how `gcu_string_murmur3_x86_128()` shipped for three
// years with three of its four fmix32 calls missing their leading step.
//
// This is the external anchor.  Appleby's SMHasher publishes one verification
// value per function, computed by hashing keys {0}, {0,1}, {0,1,2} ...
// {0..254} -- key i under seed 256-i -- concatenating the results and hashing
// that array under seed 0.  The first four bytes of the answer, little
// endian, are the value.
//
// It is worth being clear about why three of these in one test is stronger
// than three separate ones: the procedure is transcribed here from the
// reference, so a mistake in the transcription is as likely as a mistake in
// the library.  Two functions that were already correct reproduce their
// published values under the same code that judges the third.  The
// transcription cannot be wrong and let those two pass.
//
// It is also a wide test for four lines: every length from 0 to 255, so all
// four tail residues at 32 bits and all sixteen at 128, under 256 seeds.
typedef void (*Murmur3Fn)(const void *, size_t, uint32_t, void *);

static uint32_t murmur3_verification(Murmur3Fn hash, int hashbits) {
  const int hashbytes = hashbits / 8;
  uint8_t key[256] = {};
  uint8_t hashes[16 * 256] = {};
  uint8_t final_hash[16] = {};

  for (int i = 0; i < 256; ++i) {
    key[i] = static_cast<uint8_t>(i);
    hash(key, static_cast<size_t>(i), static_cast<uint32_t>(256 - i),
      &hashes[i * hashbytes]);
  }
  hash(hashes, static_cast<size_t>(hashbytes * 256), 0, final_hash);

  return static_cast<uint32_t>(final_hash[0])
    | (static_cast<uint32_t>(final_hash[1]) << 8)
    | (static_cast<uint32_t>(final_hash[2]) << 16)
    | (static_cast<uint32_t>(final_hash[3]) << 24);
}

TEST(Murmur3, MatchesAppplebysPublishedVerificationValues) {
  EXPECT_EQ(0xB0F57EE3u, murmur3_verification(gcu_string_murmur3_32, 32));
  EXPECT_EQ(0xB3ECE62Au, murmur3_verification(gcu_string_murmur3_x86_128, 128));
  EXPECT_EQ(0x6384BA69u, murmur3_verification(gcu_string_murmur3_x64_128, 128));
}

// The verification value above covers every length but reports one bit: it
// broke.  These localise it to a length, which is usually enough to name the
// arm -- a tail residue, the first full block, the block loop.
//
// Provenance, because it decides what a failure here means: these are *not*
// independent.  They were taken from this implementation once the test above
// passed, so they pin the algorithm the verification value already
// identified.  A disagreement here is a regression in this library, never
// evidence about what MurmurHash3 is.
TEST(Murmur3, KnownAnswersPerLength) {
  // "abcdefghijklmnopqrstuvwxyz0123456789", seed 0, prefixes of each length.
  static const char k[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  static const struct {
    size_t len;
    uint32_t h32;
    uint64_t h128_86[2];
    uint64_t h128_64[2];
  } expected[] = {
    {  0, 0x00000000U, { 0x0000000000000000ULL, 0x0000000000000000ULL }, { 0x0000000000000000ULL, 0x0000000000000000ULL } },
    {  1, 0x3C2569B2U, { 0x5556B01BA794933CULL, 0x5556B01B5556B01BULL }, { 0x85555565F6597889ULL, 0xE6B53A48510E895AULL } },
    {  2, 0x9BBFD75FU, { 0x25BE3010158451DFULL, 0x25BE301025BE3010ULL }, { 0x938B11EA16ED1B2EULL, 0xE65EA7019B52D4ADULL } },
    {  3, 0xB3DD93FAU, { 0xA2B006A575CDC6D1ULL, 0xA2B006A5A2B006A5ULL }, { 0xB4963F3F3FAD7867ULL, 0x3BA2744126CA2D52ULL } },
    {  4, 0x43ED676AU, { 0x45AFC62E96B6CCAAULL, 0x45AFC62E45AFC62EULL }, { 0xB87BB7D64656CD4FULL, 0xF2003E886073E875ULL } },
    {  5, 0xE89B9AF6U, { 0x5D24C5BCC5402EFBULL, 0x5A7201775A720177ULL }, { 0x2036D091F496BBB8ULL, 0xC5C7EEA04BCFEC8CULL } },
    {  6, 0x6181C085U, { 0xA1AF2721E17CB90AULL, 0xA9BEDFF9A9BEDFF9ULL }, { 0xE47D86BFACA3BF55ULL, 0xB07109993321845CULL } },
    {  7, 0x883C9B06U, { 0x09863ADE90B541D9ULL, 0x4A7769284A776928ULL }, { 0xA6CD2F9FC09EE499ULL, 0x1C3AA23AB155BBB6ULL } },
    {  8, 0x49DDCCC4U, { 0xADB11487AEF41136ULL, 0xFA6C8092FA6C8092ULL }, { 0xCC8A0AB037EF8C02ULL, 0x48890D60EB6940A1ULL } },
    {  9, 0x421406F0U, { 0x2206596FAD058C1CULL, 0xDD94417C14D27D10ULL }, { 0x0547C0CFF13C7964ULL, 0x79B53DF5B741E033ULL } },
    { 10, 0x88927791U, { 0x9A37900EF5D92EA7ULL, 0x692FF17FC792AA2AULL }, { 0xB6C15B0D772F8C99ULL, 0xA24D85DC8C651AC9ULL } },
    { 11, 0x5F3B25DFU, { 0xCA1F111EA0C8C9DDULL, 0xBD5E975782FEF12DULL }, { 0xA895D0B8DF789D02ULL, 0xBB7C31E2455AE771ULL } },
    { 12, 0xA36F3D27U, { 0xF48224C0738503FEULL, 0xD28CE55327D4DFADULL }, { 0x8EF39BB1E67AE194ULL, 0x1F9E303272FF621CULL } },
    { 13, 0xF212161BU, { 0xDBAEA1ECC13AA215ULL, 0xD92CDE70F41C3566ULL }, { 0x1648288DA7C0FA73ULL, 0x2E657BFF0DE7CC7FULL } },
    { 14, 0xF8526DF0U, { 0x605D1FD775EC118EULL, 0xF1D4894D946F45F4ULL }, { 0x91D094A7F5C375E0ULL, 0xEE096027D26A3324ULL } },
    { 15, 0x9D09F7D2U, { 0x4A1AD5AEEE6D1D69ULL, 0xADA761A884CE1457ULL }, { 0x8ABE2451890C2FFBULL, 0x6A548C2D9C962A61ULL } },
    { 16, 0xE76291EDU, { 0x90B912569FD27627ULL, 0xD193BA45E4CE8B21ULL }, { 0xC4CA3CA3224CB723ULL, 0x4333D695B331EB1AULL } },
    { 17, 0xB6655E4AU, { 0x64515C6F0445A4D3ULL, 0x1D11079DD96DDC2AULL }, { 0x7564747F88BDA657ULL, 0xECDA499DA1110DE4ULL } },
    { 32, 0xD14E3386U, { 0xD074E5DFDBAB3052ULL, 0xC5407F427F2EADEAULL }, { 0x16A127B539E20AE3ULL, 0xEDCB0722A1FEBF68ULL } },
    { 36, 0x5B4C2C16U, { 0xB64963E9DEAF2841ULL, 0xD1A2CF60B5137B76ULL }, { 0xF2596F5EB4FF5E3AULL, 0x3996B9372E6D94C0ULL } },
  };

  for (auto const & e : expected) {
    uint32_t out32;
    uint64_t out128[2];

    gcu_string_murmur3_32(k, e.len, 0, &out32);
    EXPECT_EQ(e.h32, out32) << "murmur3_32 at length " << e.len;

    gcu_string_murmur3_x86_128(k, e.len, 0, out128);
    EXPECT_EQ(e.h128_86[0], out128[0]) << "x86_128 lo at length " << e.len;
    EXPECT_EQ(e.h128_86[1], out128[1]) << "x86_128 hi at length " << e.len;

    gcu_string_murmur3_x64_128(k, e.len, 0, out128);
    EXPECT_EQ(e.h128_64[0], out128[0]) << "x64_128 lo at length " << e.len;
    EXPECT_EQ(e.h128_64[1], out128[1]) << "x64_128 hi at length " << e.len;
  }
}

// A key is a run of bytes.  Where those bytes happen to sit in memory is not
// part of the question being asked, so the same bytes must hash the same from
// every offset -- and the functions must be free of undefined behaviour while
// doing it.  Hashing a substring, an offset into a buffer, or a field inside a
// packed struct are all ordinary things to ask of a (const void *, size_t)
// API, and all of them can hand it an odd address.
//
// Note which half of that this test can see on its own.  x86 performs
// unaligned loads happily, so the pointer-cast version these functions used to
// use returned the *correct* answer from a misaligned key and every assertion
// below passed against it.  The undefined behaviour is only visible to an
// instrument: under `make test-asan` the old code stops at
//
//     src/string.c:76: runtime error: load of misaligned address ...
//     for type 'const uint32_t', which requires 4 byte alignment
//
// so this test has teeth in the sanitizer build and is a property check in the
// release build.  That is also why the defect survived: every key in this file
// was a string literal and every key the consumers pass comes from malloc, so
// nothing had ever handed these functions an odd address to begin with.
TEST(Murmur3, AnswerDoesNotDependOnKeyAlignment) {
  // Lengths spanning both 128-bit tail residues and several whole blocks.
  static const size_t lengths[] = { 1, 3, 4, 7, 8, 15, 16, 17, 31, 32, 33, 64 };

  alignas(8) unsigned char payload[64];
  for (size_t i = 0; i < sizeof payload; ++i) {
    payload[i] = static_cast<unsigned char>(i * 7 + 1);
  }

  for (size_t len : lengths) {
    // The answers from an aligned key, to compare every offset against.
    uint32_t want32;
    uint64_t want86[2], want64[2];
    gcu_string_murmur3_32(payload, len, 0, &want32);
    gcu_string_murmur3_x86_128(payload, len, 0, want86);
    gcu_string_murmur3_x64_128(payload, len, 0, want64);

    for (size_t offset = 1; offset < 8; ++offset) {
      alignas(8) unsigned char backing[8 + sizeof payload];
      unsigned char * shifted = backing + offset;
      memcpy(shifted, payload, len);
      ASSERT_NE(0u, reinterpret_cast<uintptr_t>(shifted) % 8)
        << "the shifted key was supposed to be misaligned";

      uint32_t got32;
      uint64_t got86[2], got64[2];
      gcu_string_murmur3_32(shifted, len, 0, &got32);
      gcu_string_murmur3_x86_128(shifted, len, 0, got86);
      gcu_string_murmur3_x64_128(shifted, len, 0, got64);

      EXPECT_EQ(want32, got32)
        << "murmur3_32, length " << len << ", offset " << offset;
      EXPECT_EQ(want86[0], got86[0]) << "x86_128 lo, length " << len
        << ", offset " << offset;
      EXPECT_EQ(want86[1], got86[1]) << "x86_128 hi, length " << len
        << ", offset " << offset;
      EXPECT_EQ(want64[0], got64[0]) << "x64_128 lo, length " << len
        << ", offset " << offset;
      EXPECT_EQ(want64[1], got64[1]) << "x64_128 hi, length " << len
        << ", offset " << offset;
    }
  }
}

// The output pointer is a void * too, and had the same cast on the way out.
TEST(Murmur3, WritesToAnUnalignedOutputBuffer) {
  alignas(8) unsigned char out[8 + 16];
  for (size_t offset = 1; offset < 8; ++offset) {
    uint32_t want32;
    uint64_t want128[2];
    gcu_string_murmur3_32("alignment", 9, 0, &want32);
    gcu_string_murmur3_x64_128("alignment", 9, 0, want128);

    memset(out, 0, sizeof out);
    gcu_string_murmur3_32("alignment", 9, 0, out + offset);
    EXPECT_EQ(0, memcmp(out + offset, &want32, sizeof want32))
      << "murmur3_32 output at offset " << offset;

    memset(out, 0, sizeof out);
    gcu_string_murmur3_x64_128("alignment", 9, 0, out + offset);
    EXPECT_EQ(0, memcmp(out + offset, want128, sizeof want128))
      << "x64_128 output at offset " << offset;
  }
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

