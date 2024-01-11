/**
 * @file
 *
 * This file implements cross-platform thread functions.
 */

#define _POSIX_C_SOURCE 199309L
#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include "cutil/thread.h"

#ifdef _WIN32
#else
#include <dirent.h>
#include <sched.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#endif // _WIN32

int gcu_thread_create(GCU_THREAD_T *thread, GCU_THREAD_FUNC func, void *arg) {
#ifdef _WIN32
  *thread = CreateThread(NULL, 0, func, arg, 0, NULL);
  return *thread == NULL;
#else
  return pthread_create(thread, NULL, func, arg);
#endif
}


int gcu_thread_join(GCU_THREAD_T thread) {
#ifdef _WIN32
  return WaitForSingleObject(thread, INFINITE);
#else
  return pthread_join(thread, NULL);
#endif
}


int gcu_thread_detach(GCU_THREAD_T thread) {
#ifdef _WIN32
  return CloseHandle(thread);
#else
  return pthread_detach(thread);
#endif
}


unsigned long gcu_thread_get_id() {
#ifdef _WIN32
  return GetCurrentThreadId();
#else
  return pthread_self();
#endif
}


void gcu_thread_sleep(unsigned long milliseconds) {
#ifdef _WIN32
  Sleep(milliseconds);
#else
  struct timespec ts;
  ts.tv_sec = milliseconds / 1000;
  ts.tv_nsec = (milliseconds % 1000) * 1000000;
  nanosleep(&ts, NULL);
#endif
}


void gcu_thread_yield() {
#ifdef _WIN32
  Sleep(0);
#else
  sched_yield();
#endif
}


unsigned int gcu_thread_get_num_processors() {
#ifdef _WIN32
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  return sysinfo.dwNumberOfProcessors;
#else
  return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}


int gcu_thread_set_affinity(GCU_THREAD_T thread, unsigned long mask) {
#ifdef _WIN32
  return SetThreadAffinityMask(thread, mask) == 0;
#else
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  for (size_t i = 0; i < sizeof(mask) * 8; i++) {
    if (mask & (1 << i)) {
      CPU_SET(i, &cpuset);
    }
  }
  return pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
#endif
}


int gcu_thread_get_affinity(GCU_THREAD_T thread, unsigned long *mask) {
#ifdef _WIN32
  DWORD_PTR process_mask, system_mask;
  if (GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask)) {
    DWORD_PTR thread_mask = SetThreadAffinityMask(thread, process_mask);
    if (thread_mask) {
      SetThreadAffinityMask(thread, thread_mask);
      *mask = thread_mask;
      return 0;
    }
  }
  return 1;
#else
  cpu_set_t cpuset;
  int ret = pthread_getaffinity_np(thread, sizeof(cpuset), &cpuset);
  *mask = 0;
  for (size_t i = 0; i < sizeof(cpuset) * 8; i++) {
    if (CPU_ISSET(i, &cpuset)) {
      *mask |= 1 << i;
    }
  }
  return ret;
#endif
}


int gcu_thread_set_priority(GCU_THREAD_T thread, int priority) {
#ifdef _WIN32
  return SetThreadPriority(thread, priority) == 0;
#else
  struct sched_param param;
  param.sched_priority = priority;
  return pthread_setschedparam(thread, SCHED_OTHER, &param);
#endif
}


int gcu_thread_get_priority(GCU_THREAD_T thread, int *priority) {
#ifdef _WIN32
  *priority = GetThreadPriority(thread);
  return *priority == THREAD_PRIORITY_ERROR_RETURN;
#else
  struct sched_param param;
  int policy;
  int ret = pthread_getschedparam(thread, &policy, &param);
  *priority = param.sched_priority;
  return ret;
#endif
}


int gcu_thread_set_name(GCU_THREAD_T thread, const char *name) {
#ifdef _WIN32
  return SetThreadDescription(thread, name) == 0;
#else
  return pthread_setname_np(thread, name);
#endif
}


int gcu_thread_get_name(GCU_THREAD_T thread, char *name, size_t size) {
#ifdef _WIN32
  return GetThreadDescription(thread, name, size) == 0;
#else
  return pthread_getname_np(thread, name, size);
#endif
}


int gcu_thread_get_current_name(char *name, size_t size) {
  return gcu_thread_get_name(gcu_thread_get_current_handle(), name, size);
}


unsigned long gcu_thread_get_current_id() {
  return gcu_thread_get_id_from_handle(gcu_thread_get_current_handle());
}


GCU_THREAD_T gcu_thread_get_current_handle() {
#ifdef _WIN32
  return GetCurrentThread();
#else
  return pthread_self();
#endif
}


unsigned long gcu_thread_get_id_from_handle(GCU_THREAD_T thread) {
#ifdef _WIN32
  return GetThreadId(thread);
#else
  return thread;
#endif
}


unsigned long gcu_thread_get_id_from_name(const char *name) {
  return gcu_thread_get_id_from_handle(gcu_thread_get_handle_from_name(name));
}


GCU_THREAD_T gcu_thread_get_handle_from_id(unsigned long id) {
#ifdef _WIN32
  return OpenThread(THREAD_ALL_ACCESS, FALSE, id);
#else
  return id;
#endif
}


GCU_THREAD_T gcu_thread_get_handle_from_name(const char *name) {
#ifdef _WIN32
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snapshot != INVALID_HANDLE_VALUE) {
    THREADENTRY32 entry;
    entry.dwSize = sizeof(entry);
    if (Thread32First(snapshot, &entry)) {
      do {
        if (strcmp(entry.szExeFile, name) == 0) {
          HANDLE thread = OpenThread(THREAD_ALL_ACCESS, FALSE, entry.th32ThreadID);
          if (thread != NULL) {
            CloseHandle(snapshot);
            return thread;
          }
        }
      } while (Thread32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
  }
  return NULL;
#else
  DIR *dir = opendir("/proc/self/task");
  if (dir != NULL) {
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
      if (entry->d_type == DT_DIR) {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "/proc/self/task/%s/comm", entry->d_name);
        FILE *file = fopen(path, "r");
        if (file != NULL) {
          char buf[PATH_MAX];
          if (fgets(buf, sizeof(buf), file) != NULL) {
            if (strcmp(buf, name) == 0) {
              fclose(file);
              closedir(dir);
              return strtoul(entry->d_name, NULL, 10);
            }
          }
          fclose(file);
        }
      }
    }
    closedir(dir);
  }
  return 0;
#endif
}

