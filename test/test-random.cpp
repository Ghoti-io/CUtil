#include <random>
#include <sstream>
#include <gtest/gtest.h>
#include <cutil/random.h>

using namespace std;

TEST(Random, MT) {
  {
    // Known seed value, 32-bit.
    uint32_t seed = 0x12345678;
    GTU_Random_MT32_State state;
    gcu_random_mt32_init(&state, seed);
    mt19937 mt(seed);
    for (int i = 0; i < GCU_RANDOM_MT_STATE_SIZE32 * 2; ++i) {
      EXPECT_EQ(mt(), gcu_random_mt32_next(&state));
    }
  }
  {
    // Known seed value, 64-bit.
    uint64_t seed = 0x1234567890ABCDEF;
    GTU_Random_MT64_State state;
    gcu_random_mt64_init(&state, seed);
    mt19937_64 mt(seed);
    for (int i = 0; i < GCU_RANDOM_MT_STATE_SIZE64 * 2; ++i) {
      EXPECT_EQ(mt(), gcu_random_mt64_next(&state));
    }
  }
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

