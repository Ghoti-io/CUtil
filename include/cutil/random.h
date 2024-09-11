/**
 * @file
 *
 * This file implements cross-platform random number generation functions.
 */

#ifndef G_CUTIL_RANDOM_H
#define G_CUTIL_RANDOM_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stdint.h>
#include <cutil/libver.h>
#include <cutil/float.h>

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
typedef struct GTU_Random_MT_State {
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
} GTU_Random_MT_State;

/**
 * Initialize the 32-bit Mersenne Twister state with the given seed.
 *
 * @param state A pointer to the state structure to be initialized.
 * @param seed A seed value with which to initialize the state.
 */
void gcu_random_mt32_init(GTU_Random_MT_State * state, uint32_t seed);

/**
 * Generate the next random number from the 32-bit Mersenne Twister state.
 *
 * @param state A pointer to the state structure from which to generate the next
 * random number.
 * @return The next random number in the sequence.
 */
uint32_t gcu_random_mt32_next(GTU_Random_MT_State * state);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // G_CUTIL_RANDOM_H
