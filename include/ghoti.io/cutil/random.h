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
 *
 * This file implements cross-platform random number generation functions.
 */

#ifndef GHOTI_IO_GCU_RANDOM_H
#define GHOTI_IO_GCU_RANDOM_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stdint.h>
#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/float.h>

/**
 * The number of elements in the state array for the 32-bit Mersenne Twister.
 */
#define GCU_RANDOM_MT_STATE_SIZE32 624

/**
 * The number of elements in the state array for the 64-bit Mersenne Twister.
 */
#define GCU_RANDOM_MT_STATE_SIZE64 312


/**
 * The state structure for the 32-bit Mersenne Twister.
 */
typedef struct GCU_Random_MT32_State {
  /**
   * The state array for the Mersenne Twister.
   */
  uint32_t state_array[GCU_RANDOM_MT_STATE_SIZE32];
  /**
   * The index into the state array, which is used to determine the next value
   * to be generated.
   *
   * This value is always in the range [0, n-1].
   */
  size_t state_index;
} GCU_Random_MT32_State;

/**
 * Initialize the 32-bit Mersenne Twister state with the given seed.
 *
 * @param state A pointer to the state structure to be initialized.
 * @param seed A seed value with which to initialize the state.
 */
GCU_API void gcu_random_mt32_init(GCU_Random_MT32_State * state, uint32_t seed);

/**
 * Generate the next random number from the 32-bit Mersenne Twister state.
 *
 * @param state A pointer to the state structure from which to generate the next
 * random number.
 * @return The next random number in the sequence.
 */
GCU_API uint32_t gcu_random_mt32_next(GCU_Random_MT32_State * state);

/**
 * The state structure for the 64-bit Mersenne Twister.
 */
typedef struct GCU_Random_MT64_State {
  /**
   * The state array for the Mersenne Twister.
   */
  uint64_t state_array[GCU_RANDOM_MT_STATE_SIZE64];
  /**
   * The index into the state array, which is used to determine the next value
   * to be generated.
   *
   * This value is always in the range [0, n-1].
   */
  size_t state_index;
} GCU_Random_MT64_State;

/**
 * Initialize the 64-bit Mersenne Twister state with the given seed.
 *
 * @param state A pointer to the state structure to be initialized.
 * @param seed A seed value with which to initialize the state.
 */
GCU_API void gcu_random_mt64_init(GCU_Random_MT64_State * state, uint64_t seed);

/**
 * Generate the next random number from the 64-bit Mersenne Twister state.
 *
 * @param state A pointer to the state structure from which to generate the next
 * random number.
 * @return The next random number in the sequence.
 */
GCU_API uint64_t gcu_random_mt64_next(GCU_Random_MT64_State * state);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // GHOTI_IO_GCU_RANDOM_H
