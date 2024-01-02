/**
 * @file
 *
 * Test the behavior of Ghoti.io CUtil debug library and tools.
 */

#include <sstream>
#include <gtest/gtest.h>
#include "cutil/debug.h"

using namespace std;

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

