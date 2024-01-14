/**
 * @file
 *
 * Cross-platform thread abstraction.
 */

#ifndef G_CUTIL_THREAD_H
#define G_CUTIL_THREAD_H

#include <stdbool.h>
#include <stdint.h>
#include "cutil/libver.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/// @cond HIDDEN_SYMBOLS
#define GCU_Thread GHOTIIO_CUTIL(GCU_Thread)
#define gcu_thread_constructor GHOTIIO_CUTIL(gcu_thread_constructor)
#define gcu_thread_destructor GHOTIIO_CUTIL(gcu_thread_destructor)
#define gcu_thread_create GHOTIIO_CUTIL(gcu_thread_create)
#define gcu_thread_join GHOTIIO_CUTIL(gcu_thread_join)
#define gcu_thread_detach GHOTIIO_CUTIL(gcu_thread_detach)
#define gcu_thread_is_running GHOTIIO_CUTIL(gcu_thread_is_running)
#define gcu_thread_cancel GHOTIIO_CUTIL(gcu_thread_cancel)
#define gcu_thread_sleep GHOTIIO_CUTIL(gcu_thread_sleep)
#define gcu_thread_yield GHOTIIO_CUTIL(gcu_thread_yield)
#define gcu_thread_get_num_processors GHOTIIO_CUTIL(gcu_thread_get_num_processors)
#define gcu_thread_set_affinity GHOTIIO_CUTIL(gcu_thread_set_affinity)
#define gcu_thread_get_affinity GHOTIIO_CUTIL(gcu_thread_get_affinity)
#define gcu_thread_set_priority GHOTIIO_CUTIL(gcu_thread_set_priority)
#define gcu_thread_get_priority GHOTIIO_CUTIL(gcu_thread_get_priority)
#define gcu_thread_set_name GHOTIIO_CUTIL(gcu_thread_set_name)
#define gcu_thread_get_name GHOTIIO_CUTIL(gcu_thread_get_name)
#define gcu_thread_get_current_name GHOTIIO_CUTIL(gcu_thread_get_current_name)
#define gcu_thread_get_current_id GHOTIIO_CUTIL(gcu_thread_get_current_id)
/// @endcond

#ifdef _WIN32
#include <windows.h>
typedef HANDLE GCU_THREAD_T;
typedef DWORD GCU_THREAD_FUNC_RETURN_T;
#define GCU_THREAD_FUNC_CALLING_CONVENTION WINAPI
typedef LPVOID GCU_THREAD_FUNC_ARG_T;
#else
#include <pthread.h>
typedef pthread_t GCU_THREAD_T;
typedef void * GCU_THREAD_FUNC_RETURN_T;
#define GCU_THREAD_FUNC_CALLING_CONVENTION
typedef void * GCU_THREAD_FUNC_ARG_T;
#endif // _WIN32

typedef GCU_THREAD_FUNC_RETURN_T (GCU_THREAD_FUNC_CALLING_CONVENTION *GCU_THREAD_FUNC)(GCU_THREAD_FUNC_ARG_T);

typedef uint32_t GCU_Thread;

void gcu_thread_constructor() __attribute__((constructor));
void gcu_thread_destructor() __attribute__((destructor));

/**
 * Create a new thread.
 *
 * @param thread Pointer to a thread handle.
 * @param func Function to run in the thread.
 * @param arg Argument to pass to the thread function.
 * @return 0 on success, -1 on failure.
 */
int gcu_thread_create(GCU_Thread * thread, GCU_THREAD_FUNC func, void * arg);

/**
 * Wait for a thread to finish.
 *
 * @param thread Thread handle.
 * @return 0 on success, -1 on failure.
 */
int gcu_thread_join(GCU_Thread thread);

/**
 * Detach a thread.
 *
 * @param thread Thread handle.
 * @return 0 on success, -1 on failure.
 */
int gcu_thread_detach(GCU_Thread thread);

/**
 * Sleep for a specified number of milliseconds.
 *
 * @param milliseconds Number of milliseconds to sleep.
 */
void gcu_thread_sleep(unsigned long milliseconds);

/**
 * Yield the current thread.
 */
void gcu_thread_yield();

/**
 * Get the number of logical processors on the system.
 *
 * @return The number of logical processors on the system.
 */
unsigned int gcu_thread_get_num_processors();

/**
 * Set the thread affinity mask.
 *
 * @param thread Thread handle.
 * @param mask Affinity mask.
 * @return 0 on success, -1 on failure.
 */
int gcu_thread_set_affinity(GCU_Thread thread, unsigned long mask);

/**
 * Get the thread affinity mask.
 *
 * @param thread Thread handle.
 * @param mask Pointer to a variable to store the affinity mask.
 * @return 0 on success, -1 on failure.
 */
int gcu_thread_get_affinity(GCU_Thread thread, unsigned long * mask);

/**
 * Set the thread priority.
 *
 * @param thread Thread handle.
 * @param priority Thread priority.
 * @return 0 on success, -1 on failure.
 */
int gcu_thread_set_priority(GCU_Thread thread, int priority);

/**
 * Get the thread priority.
 *
 * @param thread Thread handle.
 * @param priority Pointer to a variable to store the thread priority.
 * @return 0 on success, -1 on failure.
 */
int gcu_thread_get_priority(GCU_Thread thread, int * priority);

/**
 * Set the thread name.
 *
 * @param thread Thread handle.
 * @param name Thread name.
 * @return 0 on success, -1 on failure.
 */
int gcu_thread_set_name(GCU_Thread thread, const char * name);

/**
 * Get the thread name.
 *
 * @param thread Thread handle.
 * @param name Pointer to a variable to store the thread name.
 * @param size Size of the name buffer.
 * @return 0 on success, nonzero on failure.
 */
int gcu_thread_get_name(GCU_Thread thread, char * name, size_t size);

/**
 * Get the thread name of the current thread.
 *
 * @param name Pointer to a variable to store the thread name.
 * @param size Size of the name buffer.
 * @return 0 on success, -1 on failure.
 */
int gcu_thread_get_current_name(char * name, size_t size);

/**
 * Get the thread ID of the current thread.
 *
 * @return The thread ID of the current thread.
 */
GCU_Thread gcu_thread_get_current_id();

/**
 * Return whether or not the thread is running.
 *
 * @param thread The thread id.
 * @param is_running Pointer to a variable to store the result.
 * @return 0 on success, -1 on failure.
 */
int gcu_thread_is_running(GCU_Thread thread, bool * is_running);

/**
 * Return whether or not a thread has been joined.
 *
 * @param thread The thread id.
 * @param is_joined Pointer to a variable to store the result.
 * @return 0 on success, -1 on failure.
 */
int gcu_thread_is_joined(GCU_Thread thread, bool * is_joined);

/**
 * Return whether or not a thread has been detached.
 *
 * @param thread The thread id.
 * @param is_detached Pointer to a variable to store the result.
 * @return 0 on success, -1 on failure.
 */
int gcu_thread_is_detached(GCU_Thread thread, bool * is_detached);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // G_CUTIL_THREAD_H
