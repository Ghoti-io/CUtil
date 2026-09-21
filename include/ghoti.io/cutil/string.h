/*
 * SPDX-License-Identifier: LGPL-3.0-only
 *
 * Copyright (C) 2023-2026 Corey Pennycuff
 *
 * This file is part of Ghoti.io CUtil.
 *
 * Ghoti.io CUtil is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Ghoti.io CUtil is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * @file
 * A collection of string-related functions.
 */

#ifndef GHOTI_IO_GCU_STRING_H
#define GHOTI_IO_GCU_STRING_H

#include <ghoti.io/cutil/macros.h>

#include <stddef.h>
#include <ghoti.io/cutil/type.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Helper function to wrap the hash function that produces a 32-bit number
 * representing the hash.
 *
 * @param str A pointer to the string (or data block).
 * @param len The length of the data in bytes.
 * @return A 32-bit number representing the value.
 */
GCU_API uint32_t gcu_string_hash_32(char const * str, size_t len);

/**
 * Helper function to wrap the hash function that produces a 64-bit number
 * representing the hash.
 *
 * @param str A pointer to the string (or data block).
 * @param len The length of the data in bytes.
 * @return A 64-bit number representing the value.
 */
GCU_API uint64_t gcu_string_hash_64(char const * str, size_t len);

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
GCU_API void gcu_string_murmur3_32(const void * key, size_t len, uint32_t seed, void * out);

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
GCU_API void gcu_string_murmur3_x86_128(const void * key, size_t len, uint32_t seed, void * out);

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
GCU_API void gcu_string_murmur3_x64_128(const void * key, size_t len, uint32_t seed, void * out);

#ifdef __cplusplus
}
#endif

#endif //GHOTI_IO_GCU_STRING_H

