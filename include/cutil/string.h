/**
 * @file
 * A collection of string-related functions.
 */

#ifndef GHOTIIO_CUTIL_STRING_H
#define GHOTIIO_CUTIL_STRING_H

#include <stddef.h>
#include "cutil/type.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @cond HIDDEN_SYMBOLS
#define gcu_string_hash_32 GHOTIIO_CUTIL(gcu_string_hash_32)
#define gcu_string_hash_64 GHOTIIO_CUTIL(gcu_string_hash_64)
#define gcu_string_murmur3_32 GHOTIIO_CUTIL(gcu_string_murmur3_32)
#define gcu_string_murmur3_x86_128 GHOTIIO_CUTIL(gcu_string_murmur3_x86_128)
#define gcu_string_murmur3_x64_128 GHOTIIO_CUTIL(gcu_string_murmur3_x64_128)
/// @endcond

/**
 * Helper function to wrap the hash function that produces a 32-bit number
 * representing the hash.
 *
 * @param str A pointer to the string (or data block).
 * @param len The length of the data in bytes.
 * @return A 32-bit number representing the value.
 */
uint32_t gcu_string_hash_32(char const * str, size_t len);

/**
 * Helper function to wrap the hash function that produces a 64-bit number
 * representing the hash.
 *
 * @param str A pointer to the string (or data block).
 * @param len The length of the data in bytes.
 * @return A 64-bit number representing the value.
 */
uint64_t gcu_string_hash_64(char const * str, size_t len);

/**
 * Get 32-bit hash using the MurmurHash3 by Appleby.
 *
 * MurmurHash3 hashing algorithm, was created and put into the public domain by
 * Austin Appleby, originally in C++.
 * https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
 *
 * @param key A pointer to the start of the source data.
 * @param len The size of the data in bytes.
 * @param seed A seed value for the initial hash.
 * @param out A pointer to a 32-bit (4-byte) buffer into which the hash may be
 *   written.  The caller must supply the buffer.
 */
void gcu_string_murmur3_32(const void * key, size_t len, uint32_t seed, void * out);

/**
 * Get 128-bit hash using the MurmurHash3 for x86 architecture by Appleby.
 *
 * The x86 version does not produce the same hash as the x64 version, by design
 * by Appleby.
 *
 * MurmurHash3 hashing algorithm, was created and put into the public domain by
 * Austin Appleby, originally in C++.
 * https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
 *
 * @param key A pointer to the start of the source data.
 * @param len The size of the data in bytes.
 * @param seed A seed value for the initial hash.
 * @param out A pointer to a 128-bit (16-byte) buffer into which the hash may
 *   be written.  The caller must supply the buffer.
 */
void gcu_string_murmur3_x86_128(const void * key, size_t len, uint32_t seed, void * out);

/**
 * Get 128-bit hash using the MurmurHash3 for x64 architecture by Appleby.
 *
 * The x86 version does not produce the same hash as the x64 version, by design
 * by Appleby.
 *
 * MurmurHash3 hashing algorithm, was created and put into the public domain by
 * Austin Appleby, originally in C++.
 * https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
 *
 * @param key A pointer to the start of the source data.
 * @param len The size of the data in bytes.
 * @param seed A seed value for the initial hash.
 * @param out A pointer to a 128-bit (16-byte) buffer into which the hash may
 *   be written.  The caller must supply the buffer.
 */
void gcu_string_murmur3_x64_128(const void * key, size_t len, uint32_t seed, void * out);

#ifdef __cplusplus
}
#endif

#endif //GHOTIIO_CUTIL_STRING_H

