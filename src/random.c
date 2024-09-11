
#include <cutil/random.h>

// For reference, see the Mersenne Twister pseudocode from Wikipedia:
// https://en.wikipedia.org/wiki/Mersenne_Twister


void gcu_random_mt32_init(GCU_Random_MT32_State * state, uint32_t seed) {
  uint32_t* state_array = (uint32_t*)&state->state_array;
  state_array[0] = seed;
  for (int i = 1; i < GCU_RANDOM_MT_STATE_SIZE32; ++i) {
    uint32_t x = state_array[i - 1];
    state_array[i] = ((x ^ (x >> 30)) * 1812433253UL) + i; 
  }
  state->state_index = 0;
}


uint32_t gcu_random_mt32_next(GCU_Random_MT32_State * state) {
  // 32-bit Mersenne Twister by Matsumoto and Nishimura, 1998
  // Values taken from https://en.cppreference.com/w/cpp/numeric/random/mersenne_twister_engine
  const uint32_t upper_mask = (~(uint32_t)0) << 31; // 0x80000000
  const uint32_t lower_mask = ~upper_mask;          // 0x7FFFFFFF
  const uint32_t state_size = GCU_RANDOM_MT_STATE_SIZE32; // n
  const uint32_t middle_word = 397;          // m
  const uint32_t multiplier = 0x9908B0DFUL;  // a
  const uint32_t tempering_u = 11;           // u
  const uint32_t tempering_d = 0xFFFFFFFF;   // d
  const uint32_t tempering_s = 7;            // s
  const uint32_t tempering_b = 0x9D2C5680;   // b
  const uint32_t tempering_t = 15;           // t
  const uint32_t tempering_c = 0xEFC60000;   // c
  const uint32_t tempering_l = 18;           // l

  uint32_t* state_array = (uint32_t*)&state->state_array;
  size_t current_index = state->state_index;

  // Compute the index which is one element behind the current index, wrapped.
  size_t next_index = (current_index == state_size - 1)
    ? 0
    : current_index + 1;

  // Compute the index which is 397 elements behind the current index, wrapped.
  uint32_t middle_index = ((current_index + middle_word) < state_size)
    ? current_index + middle_word
    : current_index + middle_word - state_size;

  // Compute the new state value.
  uint32_t x = (state_array[current_index] & upper_mask) | (state_array[next_index] & lower_mask);
  x = state_array[middle_index] ^ (x >> 1) ^ (x & 1 ? multiplier : 0);
  state_array[current_index] = x;

  // Increment the state index (wrap-around included).
  state->state_index = next_index;

  // Temper the value.
  x ^= (x >> tempering_u) & tempering_d;
  x ^= (x << tempering_s) & tempering_b;
  x ^= (x << tempering_t) & tempering_c;
  x ^= x >> tempering_l;
  return x;
}


void gcu_random_mt64_init(GCU_Random_MT64_State * state, uint64_t seed) {
  uint64_t* state_array = (uint64_t*)&state->state_array;
  state_array[0] = seed;
  for (int i = 1; i < GCU_RANDOM_MT_STATE_SIZE64; ++i) {
    uint64_t x = state_array[i - 1];
    state_array[i] = ((x ^ (x >> 62)) * 6364136223846793005ULL) + i; 
  }
}


uint64_t gcu_random_mt64_next(GCU_Random_MT64_State * state) {
  // 64-bit Mersenne Twister by Matsumoto and Nishimura, 2000
  // Values taken from https://en.cppreference.com/w/cpp/numeric/random/mersenne_twister_engine
  const uint64_t upper_mask = (~(uint64_t)0) << 31; // 0xFFFFFFFF80000000
  const uint64_t lower_mask = ~upper_mask;          // 0x000000007FFFFFFF
  const uint64_t state_size = GCU_RANDOM_MT_STATE_SIZE64; // n
  const uint64_t middle_word = 156;                       // m
  const uint64_t multiplier = 0xB5026F5AA96619E9ULL;  // a
  const uint64_t tempering_u = 29;                    // u
  const uint64_t tempering_d = 0x5555555555555555ULL; // d
  const uint64_t tempering_s = 17;                    // s
  const uint64_t tempering_b = 0x71D67FFFEDA60000ULL; // b
  const uint64_t tempering_t = 37;                    // t
  const uint64_t tempering_c = 0xFFF7EEE000000000ULL; // c
  const uint64_t tempering_l = 43;                    // l

  uint64_t* state_array = (uint64_t*)&state->state_array;
  size_t current_index = state->state_index;

  // Compute the index which is one element behind the current index, wrapped.
  size_t next_index = (current_index == state_size - 1)
    ? 0
    : current_index + 1;

  // Compute the index which is 156 elements behind the current index, wrapped.
  uint64_t middle_index = ((current_index + middle_word) < state_size)
    ? current_index + middle_word
    : current_index + middle_word - state_size;

  // Compute the new state value.
  uint64_t x = (state_array[current_index] & upper_mask) | (state_array[next_index] & lower_mask);
  x = state_array[middle_index] ^ (x >> 1) ^ (x & 1 ? multiplier : 0);
  state_array[current_index] = x;

  // Increment the state index (wrap-around included).
  state->state_index = next_index;

  // Temper the value.
  x ^= (x >> tempering_u) & tempering_d;
  x ^= (x << tempering_s) & tempering_b;
  x ^= (x << tempering_t) & tempering_c;
  x ^= x >> tempering_l;
  return x;
}
