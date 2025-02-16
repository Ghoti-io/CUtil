
#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <cutil/semaphore.h>

#ifdef _WIN32
#else
#include <time.h>
#endif // _WIN32

int gcu_semaphore_create(GCU_Semaphore * semaphore, int value) {
#ifdef _WIN32
  *semaphore = CreateSemaphore(NULL, value, 0x7FFFFFFF, NULL);
  return *semaphore == NULL
    ? -1 // Failed to create semaphore.
    : 0; // Successfully created semaphore.
#else
  return sem_init(semaphore, 0, value);
#endif
}

int gcu_semaphore_destroy(GCU_Semaphore * semaphore) {
#ifdef _WIN32
  return CloseHandle(*semaphore)
    ? 0   // Successfully destroyed semaphore.
    : -1; // Failed to destroy semaphore.
#else
  return sem_destroy(semaphore);
#endif
}

int gcu_semaphore_wait(GCU_Semaphore * semaphore) {
#ifdef _WIN32
  return WaitForSingleObject(*semaphore, INFINITE) == WAIT_OBJECT_0
    ? 0   // Successfully waited on semaphore.
    : -1; // Failed to wait on semaphore.
#else
  return sem_wait(semaphore);
#endif
}

int gcu_semaphore_signal(GCU_Semaphore * semaphore) {
#ifdef _WIN32
  return ReleaseSemaphore(*semaphore, 1, NULL)
    ? 0   // Successfully signaled semaphore.
    : -1; // Failed to signal semaphore.
#else
  return sem_post(semaphore);
#endif
}

int gcu_semaphore_trywait(GCU_Semaphore * semaphore) {
#ifdef _WIN32
  return WaitForSingleObject(*semaphore, 0) == WAIT_OBJECT_0
    ? 0   // Successfully waited on semaphore.
    : -1; // Failed to wait on semaphore.
#else
  return sem_trywait(semaphore);
#endif
}

int gcu_semaphore_getvalue(GCU_Semaphore * semaphore, int * value) {
#ifdef _WIN32
  // Windows does not provide a direct way to get the value of a semaphore,
  // so we have to do some trickery to figure out the value.

  // Try to acquire the semaphore without blocking.
  DWORD result = WaitForSingleObject(*semaphore, 0);

  // We either successfully acquired the semaphore, or it was unavailable.
  if (result == WAIT_OBJECT_0) {
    // Successfully acquired, so release it and increment the value.
    LONG previous_count;
    ReleaseSemaphore(*semaphore, 1, &previous_count);
    *value = previous_count + 1;
    return 0;
  }
  else if (result == WAIT_TIMEOUT) {
    // Semaphore is unavailable, meaning the value is 0.
    *value = 0;
    return 0;
  }
  // Some error occurred.
  return -1;
#else
  return sem_getvalue(semaphore, value);
#endif
}

int gcu_semaphore_timedwait(GCU_Semaphore * semaphore, int timeout) {
#ifdef _WIN32
  return WaitForSingleObject(*semaphore, timeout) == WAIT_OBJECT_0
    ? 0   // Successfully waited on semaphore.
    : -1; // Failed to wait on semaphore.
#else
  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
    // Failed to get current time.
    return -1;
  }
  ts.tv_sec += timeout / 1000;
  ts.tv_nsec += (timeout % 1000) * 1000000;
  return sem_timedwait(semaphore, &ts);
#endif
}
