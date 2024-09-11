#include <random>
#include <sstream>
#include <gtest/gtest.h>
#include <cutil/random.h>

using namespace std;

TEST(Random, MT) {
  {
    // Known seed value.
    GTU_Random_MT_State state;
    gcu_random_mt32_init(&state, 0x12345678);
    mt19937 mt(0x12345678);
    for (int i = 0; i < GCU_RANDOM_MT_STATE_SIZE32 * 2; ++i) {
      EXPECT_EQ(mt(), gcu_random_mt32_next(&state));
    }
  }
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

