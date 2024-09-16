/**
 * @file
 *
 * This file implements cross-platform thread functions.
 */

#define _POSIX_C_SOURCE 199506L
#define _GNU_SOURCE

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "cutil/memory.h"
#include "cutil/thread.h"
#include "cutil/hash.h"

#ifdef _WIN32
#else
#include <dirent.h>
#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
#endif // _WIN32

/// @cond HIDDEN_SYMBOLS
#define GCU_Thread_Internal GHOTIIO_CUTIL(GCU_Thread_Internal)
#define gcu_thread_wrapper GHOTIIO_CUTIL(gcu_thread_wrapper)
/// @endcond

static GCU_Hash64 * gcu_thread_hash = NULL;

// Forward declaration.
typedef struct GCU_Thread_Internal GCU_Thread_Internal;


//
// This is the argument that will be passed to the thread wrapper function.
//
typedef struct {
  GCU_THREAD_FUNC func;         // The function to run.
  GCU_THREAD_FUNC_ARG_T arg;    // The argument to pass to the function.
  GCU_Thread_Internal * thread; // The thread record.
} GCU_Thread_Wrapper_Arg;


//
// This is the internal thread record that will be stored in the
// `gcu_thread_hash`.
//
typedef struct GCU_Thread_Internal {
  GCU_Thread_Wrapper_Arg wrapper_arg;    // The arguments passed when creating
                                         //   the thread.
  GCU_THREAD_T handle;                   // The thread handle.
  GCU_THREAD_FUNC_RETURN_T return_value; // The return value of the thread.
  uint32_t id;                           // The thread ID.
  bool running;                          // Whether the thread is running.
  bool joined;                           // Whether the thread has been joined.
  bool detached;                         // Whether the thread has been
                                         //   detached.
} GCU_Thread_Internal;


//
// This is a helper function to get the "handle" of the current thread.
//
static GCU_THREAD_T gcu_thread_get_current_handle() {
#ifdef _WIN32
  return GetCurrentThread();
#else
  return pthread_self();
#endif
}


//
// This is a helper function that will "wrap" the thread function, so that we
// can set the `running` flag to true before the thread function is called, and
// set it to false after the thread function returns.
//
static GCU_THREAD_FUNC_RETURN_T GCU_THREAD_FUNC_CALLING_CONVENTION gcu_thread_wrapper(void * thread_wrapper) {
  GCU_Thread_Wrapper_Arg * wrapper_arg = thread_wrapper;

  GCU_THREAD_FUNC func = wrapper_arg->func;
  GCU_THREAD_FUNC_ARG_T arg = wrapper_arg->arg;
  GCU_Thread_Internal * thread = wrapper_arg->thread;

  // Set the thread id.
  thread->id = gcu_thread_get_current_id();

  thread->running = true;
  thread->return_value = func(arg);
  thread->running = false;
  return thread->return_value;
}


//
// This will be called when the `gcu_thread_hash` is destroyed.
//
static void gcu_thread_hash_cleanup(GCU_Hash64 * hash) {
  // Lock the hash table.
  GCU_MUTEX_LOCK(hash->mutex);

  // Iterate over the hash table, and join any threads that have not been
  // detached or joined.
  GCU_Hash64_Iterator iter = gcu_hash64_iterator_get(hash);

  while (iter.exists) {
    GCU_Thread_Internal * thread = iter.value.p;
    uint32_t thread_id = thread->id;

    if (thread) {
      if (!thread->detached && !thread->joined) {
        gcu_thread_join(thread->id);
      }
      gcu_free(thread);
      iter.value.p = NULL;
    }

    gcu_hash64_remove(hash, thread_id);
    iter = gcu_hash64_iterator_next(iter);
  }

  // Unlock the hash table.
  GCU_MUTEX_UNLOCK(hash->mutex);
}

/**
 * Constructor for the thread module.
 *
 * This is called automatically when the module is loaded.  It will initialize
 * the module and prepare it for use, including allocating any memory needed by
 * the module.
 */
GCU_INIT_FUNCTION(gcu_thread_constructor) {
  gcu_thread_hash = gcu_hash64_create(gcu_thread_get_num_processors() * 3);

  // Verify that the thread hash table has been successfully allocated.
  if (gcu_thread_hash == NULL) {
    return;
  }

  // Set the destructor for the hash table.
  gcu_thread_hash->cleanup = gcu_thread_hash_cleanup;

  // Add the main thread.
  GCU_Thread_Internal *thread = gcu_calloc(sizeof(GCU_Thread_Internal), 1);

  // Verify that the main thread record has been successfully allocated.  If
  // not, destroy the thread hash table and return.
  if (!thread) {
    gcu_hash64_destroy(gcu_thread_hash);
    gcu_thread_hash = NULL;
    return;
  }

  *thread = (GCU_Thread_Internal) {
    .handle = gcu_thread_get_current_handle(),
    .return_value = 0,
    .running = true,
    .joined = false,
    .detached = false,
    .id = gcu_thread_get_current_id()
  };

  if (!gcu_hash64_set(gcu_thread_hash, thread->id, GCU_TYPE64_P(thread))) {
    // The main thread record could not be set, so clean up.
    gcu_free(thread);
    gcu_hash64_destroy(gcu_thread_hash);
    gcu_thread_hash = NULL;
  }
}

/**
 * Destructor for the thread module.
 *
 * This is called automatically when the module is unloaded.  It will wait for
 * all threads to finish before returning and it will clean up the memory
 * allocated by the module.
 */
GCU_CLEANUP_FUNCTION(gcu_thread_destructor) {
  // Verify that gcu_thread_hash has been initialized.
  if (!gcu_thread_hash) {
    return;
  }

  gcu_hash64_destroy(gcu_thread_hash);
  gcu_thread_hash = NULL;
}


int gcu_thread_create(GCU_Thread * thread, GCU_THREAD_FUNC func, void * arg) {
  // Verify that the thread hash table has been initialized.
  if (gcu_thread_hash == NULL) {
    return -1;
  }

  GCU_MUTEX_LOCK(gcu_thread_hash->mutex);

  // Allocate a new thread record.
  GCU_Thread_Internal * thread_internal = gcu_calloc(sizeof(GCU_Thread_Internal), 1);
  *thread_internal = (GCU_Thread_Internal) {
    .wrapper_arg = (GCU_Thread_Wrapper_Arg) {
      .func = func,
      .arg = arg,
      .thread = thread_internal
    },
    .handle = 0,
    .return_value = 0,
    .running = false,
    .joined = false,
    .detached = false,
    .id = 0
  };

  // TODO: Verify that the hash table can be grown.
  // The following code assumes that the hash table can be grown.

  // Create the thread.
  int failed;
#ifdef _WIN32
  thread_internal->handle = CreateThread(NULL, 0, gcu_thread_wrapper, &thread_internal->wrapper_arg, 0, NULL);
  failed = thread_internal->handle == NULL
    ? 1
    : 0;
#else
  failed = pthread_create(&thread_internal->handle, NULL, gcu_thread_wrapper, &thread_internal->wrapper_arg);
#endif

  // If the thread creation failed, remove the thread record from the hash and
  // return.
  if (failed) {
    gcu_free(thread_internal);
    GCU_MUTEX_UNLOCK(gcu_thread_hash->mutex);
    return -1;
  }

  // Wait for the thread to start and set its id.
  volatile GCU_Thread * thread_id = &thread_internal->id;
  while (!*thread_id) {
    gcu_thread_yield();
  }
  *thread = *thread_id;

  // We now have the thread id, but it is possible that the thread ID has
  // already been set in the hash table.  If this is the case, then we need to
  // reclaim the record for the current thread.
  GCU_Hash64_Value hash_value = gcu_hash64_get(gcu_thread_hash, *thread);
  if (hash_value.exists) {
    GCU_Thread_Internal * old_thread_internal = hash_value.value.p;

    // Verify that the record can be overwritten.
    // The record can be overwritten if the thread has been joined.
    // If the thread is running, then something went wrong, and we must abort.
    if (!old_thread_internal->joined) {
      // Note: This should never happen.  If it does, then that means that the
      // OS has reused a thread ID before the old thread has been joined.
      assert(false);
      gcu_free(thread_internal);
      GCU_MUTEX_UNLOCK(gcu_thread_hash->mutex);
      return -1;
    }

    // Release the memory from the old record.
    gcu_free(old_thread_internal);
  }

  // Add the thread record to the hash.
  assert(gcu_hash64_set(gcu_thread_hash, *thread, GCU_TYPE64_P(thread_internal)));

  GCU_MUTEX_UNLOCK(gcu_thread_hash->mutex);
  return 0;
}


int gcu_thread_join(GCU_Thread thread_id) {
  // Verify that the thread hash table has been initialized.
  if (!gcu_thread_hash) {
    return -1;
  }

  // Get the thread record from the hash.
  GCU_Hash64_Value hash_value = gcu_hash64_get(gcu_thread_hash, thread_id);
  if (!hash_value.exists) {
    return -1;
  }
  GCU_Thread_Internal * thread_internal = hash_value.value.p;

  // Verify that the thread has not already been joined.
  if (thread_internal->joined) {
    return -1;
  }

  // Verify that the thread has not already been detached.
  if (thread_internal->detached) {
    return -1;
  }

  // Join the thread.
  bool failed;
#ifdef _WIN32
  failed = WaitForSingleObject(thread_internal->handle, INFINITE);
#else
  failed = pthread_join(thread_internal->handle, NULL);
#endif

  // Set the joined flag.
  thread_internal->joined = !failed;

  return failed;
}


int gcu_thread_detach(GCU_Thread thread) {
  // Verify that the thread hash table has been initialized.
  if (!gcu_thread_hash) {
    return -1;
  }

  // Get the thread record from the hash.
  GCU_Hash64_Value hash_value = gcu_hash64_get(gcu_thread_hash, thread);
  if (!hash_value.exists) {
    return -1;
  }
  GCU_Thread_Internal * thread_internal = hash_value.value.p;

  // Verify that the thread has not already been joined.
  if (thread_internal->joined) {
    return -1;
  }

  // Verify that the thread has not already been detached.
  if (thread_internal->detached) {
    return -1;
  }

  // Detach the thread.
  int failed;
#ifdef _WIN32
  failed = !CloseHandle(thread_internal->handle);
#else
  failed = pthread_detach(thread);
#endif

  // Set the detached flag.
  thread_internal->detached = !failed;

  return failed;
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


int gcu_thread_set_affinity(GCU_Thread thread, unsigned long mask) {
  // Verify that the thread hash table has been initialized.
  if (!gcu_thread_hash) {
    return -1;
  }

  // Get the thread record from the hash.
  GCU_Hash64_Value hash_value = gcu_hash64_get(gcu_thread_hash, thread);
  if (!hash_value.exists) {
    return -1;
  }
  GCU_Thread_Internal * thread_internal = hash_value.value.p;

  // Verify that the thread has not already been joined.
  if (thread_internal->joined) {
    return -1;
  }

  // Verify that the thread has not already been detached.
  if (thread_internal->detached) {
    return -1;
  }

#ifdef _WIN32
  return SetThreadAffinityMask(thread_internal->handle, mask) == 0;
#else
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  for (size_t i = 0; i < sizeof(mask) * 8; i++) {
    if (mask & (1 << i)) {
      CPU_SET(i, &cpuset);
    }
  }
  return pthread_setaffinity_np(thread_internal->handle, sizeof(cpuset), &cpuset);
#endif
}


int gcu_thread_get_affinity(GCU_Thread thread, unsigned long * mask) {
  // Verify that the thread hash table has been initialized.
  if (!gcu_thread_hash) {
    return -1;
  }

  // Get the thread record from the hash.
  GCU_Hash64_Value hash_value = gcu_hash64_get(gcu_thread_hash, thread);
  if (!hash_value.exists) {
    return -1;
  }
  GCU_Thread_Internal * thread_internal = hash_value.value.p;

  // Verify that the thread has not already been joined.
  if (thread_internal->joined) {
    return -1;
  }

  // Verify that the thread has not already been detached.
  if (thread_internal->detached) {
    return -1;
  }

#ifdef _WIN32
  DWORD_PTR process_mask, system_mask;
  if (GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask)) {
    // The process mask is a superset of the thread mask, so we can use it to
    // determine the thread mask.
    // See: https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getprocessaffinitymask
    // See: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setthreadaffinitymask
    // thread_mask will contain the "old" thread mask, which we will restore.
    DWORD_PTR thread_mask = SetThreadAffinityMask(thread_internal->handle, process_mask);
    if (thread_mask) {
      SetThreadAffinityMask(thread_internal->handle, thread_mask);
      *mask = thread_mask;
      return 0;
    }
  }
  return 1;
#else
  cpu_set_t cpuset;
  int ret = pthread_getaffinity_np(thread_internal->handle, sizeof(cpuset), &cpuset);
  *mask = 0;
  for (size_t i = 0; i < sizeof(cpuset) * 8; i++) {
    if (CPU_ISSET(i, &cpuset)) {
      *mask |= 1 << i;
    }
  }
  return ret;
#endif
}


int gcu_thread_set_priority(GCU_Thread thread, int priority) {
  // Verify that the thread hash table has been initialized.
  if (!gcu_thread_hash) {
    return -1;
  }

  // Get the thread record from the hash.
  GCU_Hash64_Value hash_value = gcu_hash64_get(gcu_thread_hash, thread);
  if (!hash_value.exists) {
    return -1;
  }
  GCU_Thread_Internal * thread_internal = hash_value.value.p;

#ifdef _WIN32
  return SetThreadPriority(thread_internal->handle, priority) == 0;
#else
  struct sched_param param;
  param.sched_priority = priority;
  return pthread_setschedparam(thread_internal->handle, SCHED_OTHER, &param);
#endif
}


int gcu_thread_get_priority(GCU_Thread thread, int * priority) {
  // Verify that the thread hash table has been initialized.
  if (!gcu_thread_hash) {
    return -1;
  }

  // Get the thread record from the hash.
  GCU_Hash64_Value hash_value = gcu_hash64_get(gcu_thread_hash, thread);
  if (!hash_value.exists) {
    return -1;
  }
  GCU_Thread_Internal * thread_internal = hash_value.value.p;

#ifdef _WIN32
  *priority = GetThreadPriority(thread_internal->handle);
  return *priority == THREAD_PRIORITY_ERROR_RETURN;
#else
  struct sched_param param;
  int policy;
  int ret = pthread_getschedparam(thread_internal->handle, &policy, &param);
  *priority = param.sched_priority;
  return ret;
#endif
}


int gcu_thread_set_name(GCU_Thread thread, const char * name) {
  // Verify that the thread hash table has been initialized.
  if (!gcu_thread_hash) {
    return -1;
  }

  // Get the thread record from the hash.
  GCU_Hash64_Value hash_value = gcu_hash64_get(gcu_thread_hash, thread);
  if (!hash_value.exists) {
    return -1;
  }
  GCU_Thread_Internal * thread_internal = hash_value.value.p;

#ifdef _WIN32
  PWSTR wname = (PWSTR)gcu_calloc(sizeof(WCHAR), strlen(name) + 1);
  if (!wname) {
    return 1;
  }
  HRESULT result = MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, strlen(name) + 1);
  if (SUCCEEDED(result)) {
    result = SetThreadDescription(thread_internal->handle, wname);
  }
  gcu_free(wname);
  return result;
#else
  return pthread_setname_np(thread_internal->handle, name);
#endif
}


int gcu_thread_get_name(GCU_Thread thread, char * name, size_t size) {
  // Verify that the thread hash table has been initialized.
  if (!gcu_thread_hash) {
    return -1;
  }

  // Get the thread record from the hash.
  GCU_Hash64_Value hash_value = gcu_hash64_get(gcu_thread_hash, thread);
  if (!hash_value.exists) {
    return -1;
  }
  GCU_Thread_Internal * thread_internal = hash_value.value.p;

#ifdef _WIN32
  PWSTR threadname = NULL;
  HRESULT result = GetThreadDescription(thread_internal->handle, &threadname) == 0;
  if (SUCCEEDED(result)) {
    // Convert the thread name to UTF-8.
    result = WideCharToMultiByte(CP_UTF8, 0, threadname, -1, name, size, NULL, NULL);
    LocalFree(threadname);
  }
  return result;
#else
  return pthread_getname_np(thread_internal->handle, name, size);
#endif
}


int gcu_thread_get_current_name(char * name, size_t size) {
  return gcu_thread_get_name(gcu_thread_get_current_id(), name, size);
}


GCU_Thread gcu_thread_get_current_id() {
#ifdef _WIN32
  return GetCurrentThreadId();
#else
  return syscall(SYS_gettid);
#endif
}


int gcu_thread_is_running(GCU_Thread thread, bool * is_running) {
  // Verify that the thread hash table has been initialized.
  if (!gcu_thread_hash) {
    return -1;
  }

  // Get the thread record from the hash.
  GCU_Hash64_Value hash_value = gcu_hash64_get(gcu_thread_hash, thread);
  if (!hash_value.exists) {
    return -1;
  }

  GCU_Thread_Internal * thread_internal = hash_value.value.p;
  *is_running = thread_internal->running;
  return 0;
}


int gcu_thread_is_joined(GCU_Thread thread, bool * is_joined) {
  // Verify that the thread hash table has been initialized.
  if (!gcu_thread_hash) {
    return -1;
  }

  // Get the thread record from the hash.
  GCU_Hash64_Value hash_value = gcu_hash64_get(gcu_thread_hash, thread);
  if (!hash_value.exists) {
    return -1;
  }

  GCU_Thread_Internal * thread_internal = hash_value.value.p;
  *is_joined = thread_internal->joined;
  return 0;
}


int gcu_thread_is_detached(GCU_Thread thread, bool * is_detached) {
  // Verify that the thread hash table has been initialized.
  if (!gcu_thread_hash) {
    return -1;
  }

  // Get the thread record from the hash.
  GCU_Hash64_Value hash_value = gcu_hash64_get(gcu_thread_hash, thread);
  if (!hash_value.exists) {
    return -1;
  }

  GCU_Thread_Internal * thread_internal = hash_value.value.p;
  *is_detached = thread_internal->detached;
  return 0;
}

