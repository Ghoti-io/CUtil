#include <gtest/gtest.h>
#include "cutil/thread.h"

#include <iostream>

using namespace std;

#define SLEEP_MS 10

struct thread_status {
  bool is_running;
  bool run;
};

GCU_THREAD_FUNC_RETURN_T GCU_THREAD_FUNC_CALLING_CONVENTION loop(GCU_THREAD_FUNC_ARG_T status) {
  volatile bool & run = ((thread_status *)status)->run;
  bool & is_running = ((thread_status *)status)->is_running;
  is_running = true;
  while (run) {
    gcu_thread_yield();
  };
  is_running = false;
  return GCU_THREAD_FUNC_RETURN_T{};
};

GCU_THREAD_FUNC_RETURN_T GCU_THREAD_FUNC_CALLING_CONVENTION doNothing([[maybe_unused]] GCU_THREAD_FUNC_ARG_T arg) {
  if (arg != NULL) {
    *((GCU_Thread *)arg) = gcu_thread_get_current_id();
  }
  return GCU_THREAD_FUNC_RETURN_T{};
}

TEST(Thread, NonexistentThreads) {
  // Check that the thread functions return an error when the thread does not exist.
  GCU_Thread thread = 0;
  bool result = false;
  unsigned long mask = 0;
  int priority = 0;
  EXPECT_EQ(-1, gcu_thread_is_running(thread, &result));
  EXPECT_EQ(-1, gcu_thread_is_joined(thread, &result));
  EXPECT_EQ(-1, gcu_thread_is_detached(thread, &result));
  EXPECT_EQ(-1, gcu_thread_join(thread));
  EXPECT_EQ(-1, gcu_thread_detach(thread));
  EXPECT_EQ(-1, gcu_thread_set_name(thread, "Hello World!"));
  EXPECT_EQ(-1, gcu_thread_get_name(thread, NULL, 0));
  EXPECT_EQ(-1, gcu_thread_get_affinity(thread, &mask));
  EXPECT_EQ(-1, gcu_thread_set_affinity(thread, 0));
  EXPECT_EQ(-1, gcu_thread_get_priority(thread, &priority));
  EXPECT_EQ(-1, gcu_thread_set_priority(thread, 0));
}

TEST(Thread, SingleThreadLifetime) {
  // Create an infinite loop thread that listens for a signal.
  // The signal will be sent from the main thread.
  // The thread will be stopped from the main thread.
  // The thread will be joined from the main thread.
  thread_status status = {
    .is_running = false,
    .run = true
  };
  bool result = false;

  // Create the thread
  GCU_Thread thread;
  EXPECT_EQ(0, gcu_thread_create(&thread, loop, &status));
  EXPECT_NE(thread, 0);

  // Check the thread state.
  EXPECT_TRUE(status.is_running);
  EXPECT_EQ(0, gcu_thread_is_running(thread, &result));
  EXPECT_TRUE(result);
  EXPECT_EQ(0, gcu_thread_is_joined(thread, &result));
  EXPECT_FALSE(result);
  EXPECT_EQ(0, gcu_thread_is_detached(thread, &result));
  EXPECT_FALSE(result);

  // Stop the loop
  status.run = false;

  // Wait so that the thread can stop.
  volatile bool * is_running = &status.is_running;
  while (*is_running) {
    gcu_thread_yield();
  }


  // Check the thread state.
  EXPECT_EQ(status.is_running, false);
  EXPECT_EQ(0, gcu_thread_is_running(thread, &result));
  EXPECT_FALSE(result);

  // Join the thread.
  EXPECT_EQ(0, gcu_thread_join(thread));
  EXPECT_EQ(0, gcu_thread_is_running(thread, &result));
  EXPECT_FALSE(result);
  EXPECT_EQ(0, gcu_thread_is_joined(thread, &result));
  EXPECT_TRUE(result);
  EXPECT_EQ(0, gcu_thread_is_detached(thread, &result));
  EXPECT_FALSE(result);
}

TEST(Thread, ID) {
  // Create a thread and check its ID.
  GCU_Thread thread;
  uint64_t child_thread_id = 0;
  uint64_t parent_thread_id = gcu_thread_get_current_id();
  EXPECT_NE(0, parent_thread_id);
  EXPECT_EQ(0, gcu_thread_create(&thread, doNothing, &child_thread_id));
  EXPECT_EQ(0, gcu_thread_join(thread));
  EXPECT_EQ(thread, child_thread_id);
  EXPECT_NE(thread, 0);
  EXPECT_NE(child_thread_id, 0);
  EXPECT_NE(parent_thread_id, child_thread_id);
  EXPECT_NE(parent_thread_id, thread);
  EXPECT_EQ(parent_thread_id, gcu_thread_get_current_id());
}

TEST(Thread, NameFunctions) {
  // Create a thread and check its name.
  thread_status status = {
    .is_running = false,
    .run = true
  };
  char parent_buffer[PATH_MAX] = {0};
  char child_buffer[PATH_MAX] = {0};
  char new_name[] = "Hello World!";
  char smallBuffer[] = "";
  GCU_Thread child_thread;
  EXPECT_EQ(0, gcu_thread_create(&child_thread, loop, &status));

  // Verify that the parent and child thread have the same name.
  EXPECT_EQ(0, gcu_thread_get_current_name(parent_buffer, sizeof(parent_buffer)));
  EXPECT_NE(string(parent_buffer), string());
  EXPECT_NE(0, gcu_thread_get_name(child_thread, smallBuffer, sizeof(smallBuffer)));
  EXPECT_EQ(0, gcu_thread_get_name(child_thread, child_buffer, sizeof(child_buffer)));
  EXPECT_EQ(string(parent_buffer), string(child_buffer));

  // Set the child thread name.
  EXPECT_EQ(0, gcu_thread_set_name(child_thread, new_name));
  EXPECT_EQ(0, gcu_thread_get_name(child_thread, child_buffer, sizeof(child_buffer)));
  EXPECT_EQ(string(new_name), string(child_buffer));

  status.run = false;
  gcu_thread_join(child_thread);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
