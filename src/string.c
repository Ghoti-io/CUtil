/**
 */

#include <stdlib.h>
#include <string.h>
#include <cutil/string.h>

#define ROTL32(a,b) ((a << b) | (a >> (32 - b)))
#define ROTL64(a,b) ((a << b) | (a >> (64 - b)))

uint32_t gcu_string_hash_32(char const * str, size_t len) {
  uint32_t buf;
  gcu_string_murmur3_32(str, len, 0, &buf);
  return buf;
}

#if SIZE_MAX == 18446744073709551615u

uint64_t gcu_string_hash_64(char const * str, size_t len) {
  uint64_t buf[2];
  gcu_string_murmur3_x64_128(str, len, 0, buf);
  return (int64_t)(buf[0] ^ buf[1]);
}

#else

size_t gcu_string_hash_64(char const * str, size_t len) {
  int64_t buf[2];
  gcu_string_murmur3_x86_128(str, len, 0, buf);
  return buf[0] ^ buf[1];
}

#endif

void gcu_string_murmur3_32(const void * key, size_t len, uint32_t seed, void * out) {
  // Reference variables.
  // Note, in Appleby's original C++ code, he uses a (uint8_t *) for `data`.
  // TODO: verify that data is byte-aligned.  If not, perform a parital hash,
  //   similar to the "tail" section below.
  //   This may not be necessary for desktop computers, but probably is for
  //   mobile processors.
  const uint32_t * data = (const uint32_t *) key;
  const int nblocks = len / 4;

  // The seed.
  uint32_t h1 = seed;

  // All constants provided by Appleby
  const uint32_t c1 = 0xcc9e2d51;
  const uint32_t c2 = 0x1b873593;

  // Main body, process 4 bytes at a time.
  uint32_t k1;
  for (int i = 0; i < nblocks; i++) {
    k1 = *data;
    ++data;

    k1 *= c1;
    k1 = ROTL32(k1, 15);
    k1 *= c2;

    h1 ^= k1;
    h1 = ROTL32(h1, 13);
    h1 = (h1 * 5) + 0xe6546b64;
  }

  // The tail (in case we are processing something whose data is not a
  // multiple of 4 in bytes.
  //
  // The original line read like this, with `data` typed as (uint8_t *):
  // const uint8_t * tail = (const uint8_t *) (data + (nblocks * 4));
  //
  // But, since we have been incrementing the `data` pointer, it will have an
  // equivalent value.
  const uint8_t * const tail = (uint8_t *)data;
  k1 = 0;
  switch (len & 3) {
    case 3:
      k1 ^= tail[2] << 16;
      // fall through
    case 2:
      k1 ^= tail[1] << 8;
      // fall through
    case 1:
      k1 ^= tail[0];
      k1 *= c1;
      k1 = ROTL32(k1, 15);
      k1 *= c2;
      h1 ^= k1;
  };


  // The finalizer.
  h1 ^= len;

  // This is an inline of fmix32()
  h1 ^= h1 >> 16;
  h1 *= 0x85ebca6b;
  h1 ^= h1 >> 13;
  h1 *= 0xc2b2ae35;
  h1 ^= h1 >> 16;

  // Populate the final hash value.
  *(uint32_t *)out = h1;
}

void gcu_string_murmur3_x86_128(const void * key, size_t len, uint32_t seed, void * out ) {
  // Note, in Appleby's original C++ code, he uses a (uint8_t *) for `data`.
  //
  // TODO: verify that data is byte-aligned.  If not, perform a parital hash,
  //   similar to the "tail" section below.
  //   This may not be necessary for desktop computers, but probably is for
  //   mobile processors.
  //
  // Note: In Appleby's original C++ code, there are a lot more multiplications
  // and other [] accesses.  By incrementing the `data` pointer instead, we
  // eliminate a lot of these multiplications.
  const uint32_t * data = (uint32_t *) key;
  const int nblocks = len / 16;

  uint32_t h1 = seed;
  uint32_t h2 = seed;
  uint32_t h3 = seed;
  uint32_t h4 = seed;

  // All constants provided by Appleby.
  const uint32_t c1 = 0x239b961b; 
  const uint32_t c2 = 0xab0e9789;
  const uint32_t c3 = 0x38b34ae5; 
  const uint32_t c4 = 0xa1e38b93;

  // Main body, process 128 bits (16 bytes) at a time.
  for(int i = 0; i < nblocks; ++i) {
    uint32_t k1 = *data;
    uint32_t k2 = *(data + 1);
    uint32_t k3 = *(data + 2);
    uint32_t k4 = *(data + 3);
    data += 4;

    k1 *= c1;
    k1 = ROTL32(k1, 15);
    k1 *= c2;
    h1 ^= k1;

    h1 = ROTL32(h1, 19);
    h1 += h2;
    h1 = (h1 * 5) + 0x561ccd1b;

    k2 *= c2;
    k2 = ROTL32(k2, 16);
    k2 *= c3;
    h2 ^= k2;

    h2 = ROTL32(h2, 17);
    h2 += h3;
    h2 = (h2 * 5) + 0x0bcaa747;

    k3 *= c3;
    k3 = ROTL32(k3, 17);
    k3 *= c4;
    h3 ^= k3;

    h3 = ROTL32(h3, 15);
    h3 += h4;
    h3 = (h3 * 5) + 0x96cd1c35;

    k4 *= c4;
    k4 = ROTL32(k4, 18);
    k4 *= c1;
    h4 ^= k4;

    h4 = ROTL32(h4, 13);
    h4 += h1;
    h4 = (h4 * 5) + 0x32ac3b17;
  }

  // The tail (in case we are processing something whose length is not a
  // multiple of 16 bytes).
  //
  // The original line read like this:
  // const uint8_t * tail = (const uint8_t*)(data + nblocks*16);
  // const uint8_t * tail = (const uint8_t *) (key + (nblocks * 16));
  //
  // But, since we have been incrementing the `data` pointer, it will have an
  // equivalent value.
  const uint8_t * const tail = (uint8_t *)data;

  uint32_t k1 = 0;
  uint32_t k2 = 0;
  uint32_t k3 = 0;
  uint32_t k4 = 0;

  switch(len & 15) {
    case 15:
      k4 ^= tail[14] << 16;
      // fall through
    case 14:
      k4 ^= tail[13] << 8;
      // fall through
    case 13:
      k4 ^= tail[12] << 0;
      k4 *= c4;
      k4 = ROTL32(k4, 18);
      k4 *= c1;
      h4 ^= k4;

      // fall through
    case 12:
      k3 ^= tail[11] << 24;
      // fall through
    case 11:
      k3 ^= tail[10] << 16;
      // fall through
    case 10:
      k3 ^= tail[9] << 8;
      // fall through
    case 9:
      k3 ^= tail[8] << 0;
      k3 *= c3;
      k3 = ROTL32(k3, 17);
      k3 *= c4;
      h3 ^= k3;

      // fall through
    case 8:
      k2 ^= tail[7] << 24;
      // fall through
    case 7:
      k2 ^= tail[6] << 16;
      // fall through
    case 6:
      k2 ^= tail[5] << 8;
      // fall through
    case 5:
      k2 ^= tail[4] << 0;
      k2 *= c2;
      k2 = ROTL32(k2, 16);
      k2 *= c3;
      h2 ^= k2;

      // fall through
    case 4:
      k1 ^= tail[3] << 24;
      // fall through
    case 3:
      k1 ^= tail[2] << 16;
      // fall through
    case 2:
      k1 ^= tail[1] << 8;
      // fall through
    case 1:
      k1 ^= tail[0] << 0;
      k1 *= c1;
      k1 = ROTL32(k1, 15);
      k1 *= c2;
      h1 ^= k1;
  };

  // The finalizer.
  h1 ^= len;
  h2 ^= len;
  h3 ^= len;
  h4 ^= len;

  h1 += h2;
  h1 += h3;
  h1 += h4;
  h2 += h1;
  h3 += h1;
  h4 += h1;

  // Inline of fmix32() from Appleby.
  h1 ^= h1 >> 16;
  h1 *= 0x85ebca6b;
  h1 ^= h1 >> 13;
  h1 *= 0xc2b2ae35;
  h1 ^= h1 >> 16;

  // Inline of fmix32() from Appleby.
  h2 *= 0x85ebca6b;
  h2 ^= h2 >> 13;
  h2 *= 0xc2b2ae35;
  h2 ^= h2 >> 16;

  // Inline of fmix32() from Appleby.
  h3 *= 0x85ebca6b;
  h3 ^= h3 >> 13;
  h3 *= 0xc2b2ae35;
  h3 ^= h3 >> 16;

  // Inline of fmix32() from Appleby.
  h4 *= 0x85ebca6b;
  h4 ^= h4 >> 13;
  h4 *= 0xc2b2ae35;
  h4 ^= h4 >> 16;

  h1 += h2;
  h1 += h3;
  h1 += h4;
  h2 += h1;
  h3 += h1;
  h4 += h1;

  ((uint32_t*)out)[0] = h1;
  ((uint32_t*)out)[1] = h2;
  ((uint32_t*)out)[2] = h3;
  ((uint32_t*)out)[3] = h4;
}

void gcu_string_murmur3_x64_128(const void * key, size_t len, uint32_t seed, void * out) {
  // Note, in Appleby's original C++ code, he uses a (uint8_t *) for `data`.
  //
  // TODO: verify that data is byte-aligned.  If not, perform a parital hash,
  //   similar to the "tail" section below.
  //   This may not be necessary for desktop computers, but probably is for
  //   mobile processors.
  //
  // Note: In Appleby's original C++ code, there are a lot more multiplications
  // and other [] accesses.  By incrementing the `data` pointer instead, we
  // eliminate a lot of these multiplications.
  const uint64_t * data = (const uint64_t *) key;
  const int nblocks = len / 16;

  uint64_t h1 = seed;
  uint64_t h2 = seed;

  // All constants provided by Appleby.
  const uint64_t c1 = 0x87c37b91114253d5ULL;
  const uint64_t c2 = 0x4cf5ad432745937fULL;

  // Main body, process 128 bits (16 bytes) at a time.
  for (int i = 0; i < nblocks; ++i) {
    uint64_t k1 = *data;
    uint64_t k2 = *(data + 1);
    data += 2;

    k1 *= c1;
    k1 = ROTL64(k1, 31);
    k1 *= c2;
    h1 ^= k1;

    h1 = ROTL64(h1, 27);
    h1 += h2;
    h1 = (h1 * 5) + 0x52dce729;

    k2 *= c2;
    k2 = ROTL64(k2, 33);
    k2 *= c1;
    h2 ^= k2;

    h2 = ROTL64(h2, 31);
    h2 += h1;
    h2 = (h2 * 5) + 0x38495ab5;
  }

  // The tail (in case we are processing something whose length is not a
  // multiple of 16 bytes).
  //
  // The original line read like this:
  // const uint8_t * tail = (const uint8_t *) (key + (nblocks * 16));
  //
  // But, since we have been incrementing the `data` pointer, it will have an
  // equivalent value.
  const uint8_t * const tail = (uint8_t *)data;
  uint64_t k1 = 0;
  uint64_t k2 = 0;

  switch (len & 15) {
    case 15:
      k2 ^= (uint64_t)(tail[14]) << 48;
      // fall through
    case 14:
      k2 ^= (uint64_t)(tail[13]) << 40;
      // fall through
    case 13:
      k2 ^= (uint64_t)(tail[12]) << 32;
      // fall through
    case 12:
      k2 ^= (uint64_t)(tail[11]) << 24;
      // fall through
    case 11:
      k2 ^= (uint64_t)(tail[10]) << 16;
      // fall through
    case 10:
      k2 ^= (uint64_t)(tail[9]) << 8;
      // fall through
    case 9:
      k2 ^= (uint64_t)(tail[8]) << 0;
      k2 *= c2;
      k2 = ROTL64(k2, 33);
      k2 *= c1;
      h2 ^= k2;

      // fall through
    case 8:
      k1 ^= (uint64_t)(tail[7]) << 56;
      // fall through
    case 7:
      k1 ^= (uint64_t)(tail[6]) << 48;
      // fall through
    case 6:
      k1 ^= (uint64_t)(tail[5]) << 40;
      // fall through
    case 5:
      k1 ^= (uint64_t)(tail[4]) << 32;
      // fall through
    case 4:
      k1 ^= (uint64_t)(tail[3]) << 24;
      // fall through
    case 3:
      k1 ^= (uint64_t)(tail[2]) << 16;
      // fall through
    case 2:
      k1 ^= (uint64_t)(tail[1]) << 8;
      // fall through
    case 1:
      k1 ^= (uint64_t)(tail[0]) << 0;
      k1 *= c1;
      k1 = ROTL64(k1, 31);
      k1 *= c2;
      h1 ^= k1;
  };

  // The finalizer.
  h1 ^= len;
  h2 ^= len;

  h1 += h2;
  h2 += h1;

  // Inline of fmix64() from Appleby.
  h1 ^= h1 >> 33;
  h1 *= 0xff51afd7ed558ccdULL;
  h1 ^= h1 >> 33;
  h1 *= 0xc4ceb9fe1a85ec53ULL;
  h1 ^= h1 >> 33;

  // Inline of fmix64() from Appleby.
  h2 ^= h2 >> 33;
  h2 *= 0xff51afd7ed558ccdULL;
  h2 ^= h2 >> 33;
  h2 *= 0xc4ceb9fe1a85ec53ULL;
  h2 ^= h2 >> 33;

  h1 += h2;
  h2 += h1;

  // Populate the final hash value.
  ((uint64_t *) out)[0] = h1;
  ((uint64_t *) out)[1] = h2;
}

