
#define _POSIX_C_SOURCE 200112L

#include "cutil/semaphore.h"

#ifdef _WIN32
#else
#include <time.h>
#endif // _WIN32

int gcu_semaphore_create(GCU_Semaphore * semaphore, int value) {
#ifdef _WIN32
  *semaphore = CreateSemaphore(NULL, value, 0x7FFFFFFF, NULL);
  return *semaphore != NULL;
#else
  return sem_init(semaphore, 0, value);
#endif
}

int gcu_semaphore_destroy(GCU_Semaphore * semaphore) {
#ifdef _WIN32
  return CloseHandle(*semaphore);
#else
  return sem_destroy(semaphore);
#endif
}

int gcu_semaphore_wait(GCU_Semaphore * semaphore) {
#ifdef _WIN32
  return WaitForSingleObject(*semaphore, INFINITE) == WAIT_OBJECT_0;
#else
  return sem_wait(semaphore);
#endif
}

int gcu_semaphore_signal(GCU_Semaphore * semaphore) {
#ifdef _WIN32
  return ReleaseSemaphore(*semaphore, 1, NULL);
#else
  return sem_post(semaphore);
#endif
}

int gcu_semaphore_trywait(GCU_Semaphore * semaphore) {
#ifdef _WIN32
  return WaitForSingleObject(*semaphore, 0) == WAIT_OBJECT_0;
#else
  return sem_trywait(semaphore);
#endif
}

int gcu_semaphore_getvalue(GCU_Semaphore * semaphore, int * value) {
#ifdef _WIN32
  LONG lvalue = *value;
  int result = ReleaseSemaphore(*semaphore, 0, &lvalue);
  *value = lvalue;
  return result;
#else
  return sem_getvalue(semaphore, value);
#endif
}

int gcu_semaphore_timedwait(GCU_Semaphore * semaphore, int timeout) {
#ifdef _WIN32
  return WaitForSingleObject(*semaphore, timeout) == WAIT_OBJECT_0;
#else
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += timeout / 1000;
  ts.tv_nsec += (timeout % 1000) * 1000000;
  return sem_timedwait(semaphore, &ts);
#endif
}
