/**
 * @file
 *
 * Cross-platform thread abstraction.
 */

#ifndef G_CUTIL_THREAD_H
#define G_CUTIL_THREAD_H

#include "cutil/libver.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/// @cond HIDDEN_SYMBOLS
#define gcu_thread_create GHOTIIO_CUTIL(gcu_thread_create)
#define gcu_thread_join GHOTIIO_CUTIL(gcu_thread_join)
#define gcu_thread_detach GHOTIIO_CUTIL(gcu_thread_detach)
#define gcu_thread_get_id GHOTIIO_CUTIL(gcu_thread_get_id)
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
#define gcu_thread_get_current_handle GHOTIIO_CUTIL(gcu_thread_get_current_handle)
#define gcu_thread_get_id_from_handle GHOTIIO_CUTIL(gcu_thread_get_id_from_handle)
#define gcu_thread_get_id_from_name GHOTIIO_CUTIL(gcu_thread_get_id_from_name)
#define gcu_thread_get_handle_from_id GHOTIIO_CUTIL(gcu_thread_get_handle_from_id)
#define gcu_thread_get_handle_from_name GHOTIIO_CUTIL(gcu_thread_get_handle_from_name)
/// @endcond

#ifdef _WIN32
#include <windows.h>
typedef HANDLE GCU_THREAD_T;
typedef DWORD (WINAPI *GCU_THREAD_FUNC)(LPVOID);
#else
#include <pthread.h>
typedef pthread_t GCU_THREAD_T;
typedef void *(*GCU_THREAD_FUNC)(void *);
#endif // _WIN32

/**
 * Create a new thread.
 *
 * @param thread Pointer to a thread handle.
 * @param func Function to run in the thread.
 * @param arg Argument to pass to the thread function.
 * @return 0 on success, -1 on failure.
 */
int gcu_thread_create(GCU_THREAD_T *thread, GCU_THREAD_FUNC func, void *arg);

/**
 * Wait for a thread to finish.
 *
 * @param thread Thread handle.
 * @return 0 on success, -1 on failure.
 */
int gcu_thread_join(GCU_THREAD_T thread);

/**
 * Detach a thread.
 *
 * @param thread Thread handle.
 * @return 0 on success, -1 on failure.
 */
int gcu_thread_detach(GCU_THREAD_T thread);

/**
 * Get the current thread ID.
 *
 * @return The current thread ID.
 */
unsigned long gcu_thread_get_id();

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
int gcu_thread_set_affinity(GCU_THREAD_T thread, unsigned long mask);

/**
 * Get the thread affinity mask.
 *
 * @param thread Thread handle.
 * @param mask Pointer to a variable to store the affinity mask.
 * @return 0 on success, -1 on failure.
 */
int gcu_thread_get_affinity(GCU_THREAD_T thread, unsigned long *mask);

/**
 * Set the thread priority.
 *
 * @param thread Thread handle.
 * @param priority Thread priority.
 * @return 0 on success, -1 on failure.
 */
int gcu_thread_set_priority(GCU_THREAD_T thread, int priority);

/**
 * Get the thread priority.
 *
 * @param thread Thread handle.
 * @param priority Pointer to a variable to store the thread priority.
 * @return 0 on success, -1 on failure.
 */
int gcu_thread_get_priority(GCU_THREAD_T thread, int *priority);

/**
 * Set the thread name.
 *
 * @param thread Thread handle.
 * @param name Thread name.
 * @return 0 on success, -1 on failure.
 */
int gcu_thread_set_name(GCU_THREAD_T thread, const char *name);

/**
 * Get the thread name.
 *
 * @param thread Thread handle.
 * @param name Pointer to a variable to store the thread name.
 * @param size Size of the name buffer.
 * @return 0 on success, -1 on failure.
 */
int gcu_thread_get_name(GCU_THREAD_T thread, char *name, size_t size);

/**
 * Get the thread name of the current thread.
 *
 * @param name Pointer to a variable to store the thread name.
 * @param size Size of the name buffer.
 * @return 0 on success, -1 on failure.
 */
int gcu_thread_get_current_name(char *name, size_t size);

/**
 * Get the thread ID of the current thread.
 *
 * @return The thread ID of the current thread.
 */
unsigned long gcu_thread_get_current_id();

/**
 * Get the thread handle of the current thread.
 *
 * @return The thread handle of the current thread.
 */
GCU_THREAD_T gcu_thread_get_current_handle();

/**
 * Get the thread ID of the specified thread.
 *
 * @param thread Thread handle.
 * @return The thread ID of the specified thread.
 */
unsigned long gcu_thread_get_id_from_handle(GCU_THREAD_T thread);

/**
 * Get the thread ID of the specified thread.
 *
 * @param name Thread name.
 * @return The thread ID of the specified thread.
 */
unsigned long gcu_thread_get_id_from_name(const char *name);

/**
 * Get the thread handle of the specified thread.
 *
 * @param id Thread ID.
 * @return The thread handle of the specified thread.
 */
GCU_THREAD_T gcu_thread_get_handle_from_id(unsigned long id);

/**
 * Get the thread handle of the specified thread.
 *
 * @param name Thread name.
 * @return The thread handle of the specified thread.
 */
GCU_THREAD_T gcu_thread_get_handle_from_name(const char *name);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // G_CUTIL_THREAD_H
