#include <gtest/gtest.h>
#include "cutil/semaphore.h"
#include "cutil/thread.h"

using namespace std;

TEST(Semaphore, CreateAndValueTest) {
  GCU_Semaphore s;
  // Create a semaphore with initial value 1 and verify its value.
  ASSERT_EQ(0, gcu_semaphore_create(&s, 1));
  int value = 0;
  ASSERT_EQ(0, gcu_semaphore_getvalue(&s, &value));
  ASSERT_EQ(1, value);

  // Wait on the semaphore and verify the value was decremented.
  ASSERT_EQ(0, gcu_semaphore_wait(&s));
  ASSERT_EQ(0, gcu_semaphore_getvalue(&s, &value));
  ASSERT_EQ(0, value);

  // Release the semaphore and verify the value was incremented.
  ASSERT_EQ(0, gcu_semaphore_signal(&s));
  ASSERT_EQ(0, gcu_semaphore_getvalue(&s, &value));
  ASSERT_EQ(1, value);

  // Destroy the semaphore.
  ASSERT_EQ(0, gcu_semaphore_destroy(&s));
}

TEST(Semaphore, TestAndTimedWait) {
  GCU_Semaphore s;
  // Create a semaphore with initial value 1 and wait on it.
  ASSERT_EQ(0, gcu_semaphore_create(&s, 1));
  ASSERT_EQ(0, gcu_semaphore_wait(&s));

  // Verify that trywait fails.
  ASSERT_NE(0, gcu_semaphore_trywait(&s));

  // Release the semaphore and verify that trywait succeeds.
  ASSERT_EQ(0, gcu_semaphore_signal(&s));
  ASSERT_EQ(0, gcu_semaphore_timedwait(&s, 1000));

  // Verify that timedwait times out.
  ASSERT_NE(0, gcu_semaphore_timedwait(&s, 150));

  // Release the semaphore and destroy it.
  ASSERT_EQ(0, gcu_semaphore_signal(&s));
  ASSERT_EQ(0, gcu_semaphore_destroy(&s));
}

TEST(Semaphore, Counting) {
  GCU_Semaphore s;
  // Create a semaphore with initial value 2.
  ASSERT_EQ(0, gcu_semaphore_create(&s, 2));
  int value = 0;
  ASSERT_EQ(0, gcu_semaphore_getvalue(&s, &value));
  ASSERT_EQ(2, value);

  // Wait on the semaphore twice and verify the value was decremented.
  ASSERT_EQ(0, gcu_semaphore_wait(&s));
  ASSERT_EQ(0, gcu_semaphore_getvalue(&s, &value));
  ASSERT_EQ(1, value);
  ASSERT_EQ(0, gcu_semaphore_wait(&s));
  ASSERT_EQ(0, gcu_semaphore_getvalue(&s, &value));
  ASSERT_EQ(0, value);

  // Verify that the semaphore would block with a wait.
  ASSERT_NE(0, gcu_semaphore_trywait(&s));

  // Release the semaphore twice and verify the value was incremented.
  ASSERT_EQ(0, gcu_semaphore_signal(&s));
  ASSERT_EQ(0, gcu_semaphore_getvalue(&s, &value));
  ASSERT_EQ(1, value);
  ASSERT_EQ(0, gcu_semaphore_signal(&s));
  ASSERT_EQ(0, gcu_semaphore_getvalue(&s, &value));
  ASSERT_EQ(2, value);

  // Destroy the semaphore.
  ASSERT_EQ(0, gcu_semaphore_destroy(&s));
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
