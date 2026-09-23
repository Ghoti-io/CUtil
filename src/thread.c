/*
 * SPDX-License-Identifier: LGPL-3.0-only
 *
 * Copyright (C) 2023-2026 Corey Pennycuff
 *
 * This file is part of Ghoti.io CUtil.
 *
 * Ghoti.io CUtil is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Ghoti.io CUtil is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * @file
 *
 * This file implements cross-platform thread functions.
 */

#define _POSIX_C_SOURCE 199506L
#define _GNU_SOURCE

#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/memory.h>
#include <ghoti.io/cutil/thread.h>
#include <ghoti.io/cutil/hash.h>
#include <ghoti.io/cutil/semaphore.h>

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
  GCU_Semaphore started;        // Posted once the new thread has recorded its
                                //   id.  See gcu_thread_create().
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
  // Written by the thread itself as it starts and finishes, and read by any
  // thread through gcu_thread_is_running(), so it crosses a thread boundary
  // with no lock between the two.  It cannot be placed under the hash mutex:
  // gcu_thread_hash_cleanup() holds that mutex while it joins, so a thread
  // taking it on the way out to clear its own flag would deadlock against
  // the join that is waiting for it.
  atomic_bool running;                   // Whether the thread is running.
  bool joined;                           // Whether the thread has been joined.
  bool detached;                         // Whether the thread has been
                                         //   detached.
} GCU_Thread_Internal;


//
// This is a helper function to get the "handle" of the current thread.
//
static GCU_THREAD_T gcu_thread_get_current_handle(void) {
#ifdef _WIN32
  return GetCurrentThread();
#else
  return pthread_self();
#endif
}


//
// This is a helper function to determine whether a stored handle refers to the
// calling thread.  Thread ids cannot be used for this: a process that forks
// gets a new id for the thread that survives into the child, while the handle
// stays meaningful, so an id comparison stops recognising the main thread as
// the current one the moment fork() is called.
//
static bool gcu_thread_handle_is_current(GCU_THREAD_T handle) {
#ifdef _WIN32
  // TODO(windows): never compiled. GetCurrentThread() returns a pseudo-handle
  // and GetThreadId() on one of those wants checking rather than assuming.
  return GetThreadId(handle) == GetThreadId(GetCurrentThread());
#else
  return pthread_equal(handle, pthread_self()) != 0;
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

  // Release the creating thread.  Both writes above are sequenced before this
  // signal and the creator's wait is sequenced after it, so the creator sees
  // them without a race.  Nothing below this line may touch wrapper_arg: the
  // creator destroys the semaphore as soon as it wakes.
  gcu_semaphore_signal(&wrapper_arg->started);

  thread->return_value = func(arg);
  thread->running = false;
  return thread->return_value;
}


//
// Everything below reaches a thread record through the hash, and the hash is
// mutable:  gcu_thread_create() inserts, and frees the previous record when
// the OS reuses a thread id.  A lookup therefore has to hold the hash mutex,
// and - just as importantly - nothing may dereference a record after
// releasing it.  These helpers copy out what a caller needs, by value, while
// the lock is held.
//

//
// What the platform calls need from a record.
//
typedef struct {
  GCU_THREAD_T handle; // The platform thread handle.
  bool joined;         // Whether the thread has been joined.
  bool detached;       // Whether the thread has been detached.
  bool running;        // Whether the thread is running.
  bool exists;         // Whether a record was found at all.
} GCU_Thread_Snapshot;


//
// Copy a record's contents out under the hash mutex.
//
static GCU_Thread_Snapshot gcu_thread_snapshot(GCU_Thread thread) {
  GCU_Thread_Snapshot snapshot = {0};

  GCU_MUTEX_LOCK(gcu_thread_hash->mutex);

  GCU_Hash64_Value hash_value = gcu_hash64_get(gcu_thread_hash, thread);
  if (hash_value.exists) {
    GCU_Thread_Internal * thread_internal = hash_value.value.p;
    snapshot.handle = thread_internal->handle;
    snapshot.joined = thread_internal->joined;
    snapshot.detached = thread_internal->detached;
    // Atomic, because the thread itself writes this one and cannot take the
    // lock to do it:  the cleanup path joins while holding the mutex, so a
    // thread clearing its own flag on the way out would deadlock against the
    // join waiting for it.
    snapshot.running = thread_internal->running;
    snapshot.exists = true;
  }

  GCU_MUTEX_UNLOCK(gcu_thread_hash->mutex);
  return snapshot;
}


//
// Claim the exclusive right to join or detach a thread.
//
// Joining and detaching each have to read the flags, act, and then write them
// back, and the acting part blocks - so the read and the write cannot be one
// critical section.  Leaving the gap unguarded let two callers both observe a
// thread as unjoined and both reach pthread_join() on it, which POSIX leaves
// undefined; the same gap let a join and a detach both proceed on one thread.
// Setting the flag at the moment of the check closes it:  whoever sets it
// owns the operation, and everyone else is refused.
//
// Returns the handle by value, so that the blocking call that follows does
// not touch a record the lock no longer protects.
//
static bool gcu_thread_claim(GCU_Thread thread, bool for_detach,
    GCU_THREAD_T * handle, GCU_Thread_Internal ** claimed) {
  bool acquired = false;

  GCU_MUTEX_LOCK(gcu_thread_hash->mutex);

  GCU_Hash64_Value hash_value = gcu_hash64_get(gcu_thread_hash, thread);
  if (hash_value.exists) {
    GCU_Thread_Internal * thread_internal = hash_value.value.p;

    if (!thread_internal->joined && !thread_internal->detached) {
      if (for_detach) {
        thread_internal->detached = true;
      }
      else {
        thread_internal->joined = true;
      }
      *handle = thread_internal->handle;
      *claimed = thread_internal;
      acquired = true;
    }
  }

  GCU_MUTEX_UNLOCK(gcu_thread_hash->mutex);
  return acquired;
}


//
// Give a claim back, for an operation that then failed.
//
// The record is re-found rather than reused:  gcu_thread_create() may have
// freed and replaced it while the operation was in flight, and in that case
// the thread this claim referred to is gone and there is nothing to undo.
//
static void gcu_thread_unclaim(
    GCU_Thread thread, bool for_detach, GCU_Thread_Internal * claimed) {
  GCU_MUTEX_LOCK(gcu_thread_hash->mutex);

  GCU_Hash64_Value hash_value = gcu_hash64_get(gcu_thread_hash, thread);
  if (hash_value.exists && hash_value.value.p == claimed) {
    if (for_detach) {
      claimed->detached = false;
    }
    else {
      claimed->joined = false;
    }
  }

  GCU_MUTEX_UNLOCK(gcu_thread_hash->mutex);
}


//
// Join a thread by handle, touching neither the hash nor its mutex, so that
// it is callable from a context that already holds the lock.
//
static int gcu_thread_join_handle(GCU_THREAD_T handle) {
#ifdef _WIN32
  return WaitForSingleObject(handle, INFINITE) != WAIT_OBJECT_0;
#else
  return pthread_join(handle, NULL);
#endif
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
        if (!gcu_thread_handle_is_current(thread->handle)) {
          // We cannot join the current thread, into itself.
          //
          // The handle form, not gcu_thread_join():  this runs with the hash
          // mutex held, and the public call now takes that same mutex.
          gcu_thread_join_handle(thread->handle);
          thread->joined = true;
        }
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

#ifndef _WIN32
//
// This runs in the child of a fork().
//
// Only the calling thread survives a fork.  Every other record in the hash
// names a thread that does not exist in this process, so the join that the
// module's destructor would otherwise perform at exit is undefined behaviour -
// AddressSanitizer reports it as joining an already joined thread.  Marking
// them detached leaves the records to be freed without being joined.
//
// The records are not rekeyed even though the surviving thread's id has
// changed, because that would mean allocating while the child holds whatever
// locks were held at the moment of the fork.  The cleanup path identifies the
// current thread by its handle instead, which survives the fork intact.
//
static void gcu_thread_atfork_child(void) {
  if (!gcu_thread_hash) {
    return;
  }

  // The mutex may have been held by a thread that did not survive the fork.
  GCU_MUTEX_CREATE(gcu_thread_hash->mutex);

  GCU_Hash64_Iterator iter = gcu_hash64_iterator_get(gcu_thread_hash);
  while (iter.exists) {
    GCU_Thread_Internal * thread = iter.value.p;
    if (thread && !gcu_thread_handle_is_current(thread->handle)) {
      thread->detached = true;
      thread->running = false;
    }
    iter = gcu_hash64_iterator_next(iter);
  }
}
#endif // _WIN32


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
    return;
  }

#ifndef _WIN32
  // Keep the bookkeeping honest across fork().
  pthread_atfork(NULL, NULL, gcu_thread_atfork_child);
#endif // _WIN32
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

  // The handshake that hands the new thread's id back to this one.
  if (gcu_semaphore_create(&thread_internal->wrapper_arg.started, 0) != 0) {
    gcu_free(thread_internal);
    GCU_MUTEX_UNLOCK(gcu_thread_hash->mutex);
    return -1;
  }

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
    gcu_semaphore_destroy(&thread_internal->wrapper_arg.started);
    gcu_free(thread_internal);
    GCU_MUTEX_UNLOCK(gcu_thread_hash->mutex);
    return -1;
  }

  // Wait for the thread to start and record its id.
  //
  // This was a spin on a `volatile` read of thread_internal->id.  `volatile`
  // is not a synchronisation primitive in C: it keeps the compiler from
  // caching the value in a register, but it supplies neither atomicity nor
  // any ordering between the two threads, so the read raced the new thread's
  // write.  It happened to work on x86, whose memory model is strong enough
  // to hide it; it is not guaranteed anywhere, and ThreadSanitizer reports
  // it.  A semaphore gives the ordering the spin only assumed.
  gcu_semaphore_wait(&thread_internal->wrapper_arg.started);
  gcu_semaphore_destroy(&thread_internal->wrapper_arg.started);
  *thread = thread_internal->id;

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

  // Claim the join.  This both refuses a thread that is already joined or
  // detached and stops a second caller from reaching pthread_join() on the
  // same thread while this one is blocked in it.
  GCU_THREAD_T handle;
  GCU_Thread_Internal * claimed;
  if (!gcu_thread_claim(thread_id, false, &handle, &claimed)) {
    return -1;
  }

  int failed = gcu_thread_join_handle(handle);

  // The claim was taken before the join could be attempted, so an attempt
  // that failed has to give it back.
  if (failed) {
    gcu_thread_unclaim(thread_id, false, claimed);
  }

  return failed
    ? -1 // The thread could not be joined.
    : 0; // The thread was joined successfully.
}


int gcu_thread_detach(GCU_Thread thread) {
  // Verify that the thread hash table has been initialized.
  if (!gcu_thread_hash) {
    return -1;
  }

  // Claim the detach, for the same reason a join is claimed:  a detach and a
  // join must not both proceed on one thread.
  GCU_THREAD_T handle;
  GCU_Thread_Internal * claimed;
  if (!gcu_thread_claim(thread, true, &handle, &claimed)) {
    return -1;
  }

  // Detach the thread.
  //
  // The handle, not the id.  This passed `thread` - a GCU_Thread, which is a
  // uint32_t thread id - to pthread_detach(), which takes a pthread_t.  The
  // two are different values, so the call detached whatever that id happened
  // to alias and could not have worked; nothing caught it because no test
  // detaches a live thread and checks the result.
  int failed;
#ifdef _WIN32
  failed = !CloseHandle(handle);
#else
  failed = pthread_detach(handle);
#endif

  if (failed) {
    gcu_thread_unclaim(thread, true, claimed);
  }

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


void gcu_thread_yield(void) {
#ifdef _WIN32
  Sleep(0);
#else
  sched_yield();
#endif
}


unsigned int gcu_thread_get_num_processors(void) {
#ifdef _WIN32
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  // Clamped for symmetry with the POSIX branch.  GetSystemInfo() does not
  // report failure and dwNumberOfProcessors is documented as at least 1, so
  // this is not expected to trigger.
  return sysinfo.dwNumberOfProcessors < 1
    ? 1
    : (unsigned int)sysinfo.dwNumberOfProcessors;
#else
  // sysconf() returns a long and reports failure as -1, which would convert
  // to UINT_MAX rather than to zero.  _SC_NPROCESSORS_ONLN is a glibc/BSD
  // extension rather than base POSIX, and it does fail where /proc or /sys is
  // unavailable, so the failure is reachable and must not escape: the module
  // constructor sizes its thread table from this value, and a wrapped count
  // there fails the allocation and leaves the whole module inoperable.
  long count = sysconf(_SC_NPROCESSORS_ONLN);
  return count < 1 ? 1 : (unsigned int)count;
#endif
}


int gcu_thread_set_affinity(GCU_Thread thread, unsigned long mask) {
  // Verify that the thread hash table has been initialized.
  if (!gcu_thread_hash) {
    return -1;
  }

  // Copy the record out under the lock; see gcu_thread_snapshot().
  GCU_Thread_Snapshot snapshot = gcu_thread_snapshot(thread);
  if (!snapshot.exists) {
    return -1;
  }

  // Verify that the thread has not already been joined.
  if (snapshot.joined) {
    return -1;
  }

  // Verify that the thread has not already been detached.
  if (snapshot.detached) {
    return -1;
  }

#ifdef _WIN32
  return SetThreadAffinityMask(snapshot.handle, mask) == 0;
#else
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  for (size_t i = 0; i < sizeof(mask) * 8; i++) {
    if (mask & (1 << i)) {
      CPU_SET(i, &cpuset);
    }
  }
  return pthread_setaffinity_np(snapshot.handle, sizeof(cpuset), &cpuset);
#endif
}


int gcu_thread_get_affinity(GCU_Thread thread, unsigned long * mask) {
  // Verify that the thread hash table has been initialized.
  if (!gcu_thread_hash) {
    return -1;
  }

  // Copy the record out under the lock; see gcu_thread_snapshot().
  GCU_Thread_Snapshot snapshot = gcu_thread_snapshot(thread);
  if (!snapshot.exists) {
    return -1;
  }

  // Verify that the thread has not already been joined.
  if (snapshot.joined) {
    return -1;
  }

  // Verify that the thread has not already been detached.
  if (snapshot.detached) {
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
    DWORD_PTR thread_mask = SetThreadAffinityMask(snapshot.handle, process_mask);
    if (thread_mask) {
      SetThreadAffinityMask(snapshot.handle, thread_mask);
      *mask = thread_mask;
      return 0;
    }
  }
  return 1;
#else
  cpu_set_t cpuset;
  int ret = pthread_getaffinity_np(snapshot.handle, sizeof(cpuset), &cpuset);
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

  // Copy the record out under the lock; see gcu_thread_snapshot().
  GCU_Thread_Snapshot snapshot = gcu_thread_snapshot(thread);
  if (!snapshot.exists) {
    return -1;
  }

#ifdef _WIN32
  return SetThreadPriority(snapshot.handle, priority) == 0;
#else
  struct sched_param param;
  param.sched_priority = priority;
  return pthread_setschedparam(snapshot.handle, SCHED_OTHER, &param);
#endif
}


int gcu_thread_get_priority(GCU_Thread thread, int * priority) {
  // Verify that the thread hash table has been initialized.
  if (!gcu_thread_hash) {
    return -1;
  }

  // Copy the record out under the lock; see gcu_thread_snapshot().
  GCU_Thread_Snapshot snapshot = gcu_thread_snapshot(thread);
  if (!snapshot.exists) {
    return -1;
  }

#ifdef _WIN32
  *priority = GetThreadPriority(snapshot.handle);
  return *priority == THREAD_PRIORITY_ERROR_RETURN;
#else
  struct sched_param param;
  int policy;
  int ret = pthread_getschedparam(snapshot.handle, &policy, &param);
  *priority = param.sched_priority;
  return ret;
#endif
}


int gcu_thread_set_name(GCU_Thread thread, const char * name) {
  // Verify that the thread hash table has been initialized.
  if (!gcu_thread_hash) {
    return -1;
  }

  // Copy the record out under the lock; see gcu_thread_snapshot().
  GCU_Thread_Snapshot snapshot = gcu_thread_snapshot(thread);
  if (!snapshot.exists) {
    return -1;
  }

#ifdef _WIN32
  size_t wname_len = MultiByteToWideChar(CP_UTF8, 0, name, -1, NULL, 0);
  if (!wname_len) {
    // The conversion failed.
    return -1;
  }

  PWSTR wname = (PWSTR)gcu_calloc(sizeof(WCHAR), wname_len + 1);
  if (!wname) {
    // Memory allocation failed.
    return -1;
  }

  // Actually convert the name.
  if (!MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, wname_len + 1)) {
    // The conversion failed.
    gcu_free(wname);
    return -1;
  }

  // Set the thread name.
  HRESULT result = SetThreadDescription(snapshot.handle, wname);
  gcu_free(wname);
  return SUCCEEDED(result)
    ? 0   // The thread name was set successfully.
    : -1; // The thread name could not be set.
#else
  return pthread_setname_np(snapshot.handle, name);
#endif
}


int gcu_thread_get_name(GCU_Thread thread, char * name, size_t size) {
  // Verify that the thread hash table has been initialized.
  if (!gcu_thread_hash) {
    return -1;
  }

  // Copy the record out under the lock; see gcu_thread_snapshot().
  GCU_Thread_Snapshot snapshot = gcu_thread_snapshot(thread);
  if (!snapshot.exists) {
    return -1;
  }

#ifdef _WIN32
  PWSTR threadname = NULL;
  HRESULT result = GetThreadDescription(snapshot.handle, &threadname) == 0;

  if (SUCCEEDED(result) && threadname) {
    // Convert the thread name to UTF-8.
    int conversion_result = WideCharToMultiByte(CP_UTF8, 0, threadname, -1, name, size, NULL, NULL);
    LocalFree(threadname);

    if (conversion_result == 0) {
      // The conversion failed.
      return -1;
    }

    // Everything went well.
    return 0;
  }

  // The thread name could not be retrieved.
  return -1;

#else
  return pthread_getname_np(snapshot.handle, name, size);
#endif
}


int gcu_thread_get_current_name(char * name, size_t size) {
  return gcu_thread_get_name(gcu_thread_get_current_id(), name, size);
}


GCU_Thread gcu_thread_get_current_id(void) {
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

  // Copy the record out under the lock; see gcu_thread_snapshot().
  GCU_Thread_Snapshot snapshot = gcu_thread_snapshot(thread);
  if (!snapshot.exists) {
    return -1;
  }

  *is_running = snapshot.running;
  return 0;
}


int gcu_thread_is_joined(GCU_Thread thread, bool * is_joined) {
  // Verify that the thread hash table has been initialized.
  if (!gcu_thread_hash) {
    return -1;
  }

  // Copy the record out under the lock; see gcu_thread_snapshot().
  GCU_Thread_Snapshot snapshot = gcu_thread_snapshot(thread);
  if (!snapshot.exists) {
    return -1;
  }

  *is_joined = snapshot.joined;
  return 0;
}


int gcu_thread_is_detached(GCU_Thread thread, bool * is_detached) {
  // Verify that the thread hash table has been initialized.
  if (!gcu_thread_hash) {
    return -1;
  }

  // Copy the record out under the lock; see gcu_thread_snapshot().
  GCU_Thread_Snapshot snapshot = gcu_thread_snapshot(thread);
  if (!snapshot.exists) {
    return -1;
  }

  *is_detached = snapshot.detached;
  return 0;
}

