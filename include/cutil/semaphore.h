/**
 * @file
 *
 * Cross-platform semaphore implementation.
 */

#ifndef G_CUTIL_SEMAPHORE_H
#define G_CUTIL_SEMAPHORE_H

#include "cutil/libver.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DOXYGEN
/**
 * Cross-platform semaphore type.
 */
typedef void GCU_Semaphore;
#endif // DOXYGEN

/// @cond HIDDEN_SYMBOLS
#define GCU_Semaphore GHOTIIO_CUTIL(GCU_Semaphore)
#define gcu_semaphore_create GHOTIIO_CUTIL(gcu_semaphore_create)
#define gcu_semaphore_destroy GHOTIIO_CUTIL(gcu_semaphore_destroy)
#define gcu_semaphore_wait GHOTIIO_CUTIL(gcu_semaphore_wait)
#define gcu_semaphore_signal GHOTIIO_CUTIL(gcu_semaphore_signal)
#define gcu_semaphore_trywait GHOTIIO_CUTIL(gcu_semaphore_trywait)
#define gcu_semaphore_getvalue GHOTIIO_CUTIL(gcu_semaphore_getvalue)
#define gcu_semaphore_timedwait GHOTIIO_CUTIL(gcu_semaphore_timedwait)
/// @endcond

#ifdef _WIN32
#include <windows.h>
typedef HANDLE GCU_Semaphore;
#else
#include <semaphore.h>
typedef sem_t GCU_Semaphore;
#endif

/**
 * Create a semaphore.
 *
 * @param semaphore Pointer to semaphore to create.
 * @param value Initial value of semaphore.
 * @return 0 on success, -1 on failure.
 */
int gcu_semaphore_create(GCU_Semaphore * semaphore, int value);


/**
 * Destroy a semaphore.
 *
 * @param semaphore Semaphore to destroy.
 * @return 0 on success, -1 on failure.
 */
int gcu_semaphore_destroy(GCU_Semaphore * semaphore);

/**
 * Wait on a semaphore.
 *
 * @param semaphore Semaphore to wait on.
 * @return 0 on success, -1 on failure.
 */
int gcu_semaphore_wait(GCU_Semaphore * semaphore);

/**
 * Signal (post/release) a semaphore.
 *
 * @param semaphore Semaphore to post.
 * @return 0 on success, -1 on failure.
 */
int gcu_semaphore_signal(GCU_Semaphore * semaphore);

/**
 * Try to wait on a semaphore.
 *
 * @param semaphore Semaphore to wait on.
 * @return 0 on success, -1 on failure.
 */
int gcu_semaphore_trywait(GCU_Semaphore * semaphore);

/**
 * Get the value of a semaphore.
 *
 * @param semaphore Semaphore to get value of.
 * @param value Pointer to value to set.
 * @return 0 on success, -1 on failure.
 */
int gcu_semaphore_getvalue(GCU_Semaphore * semaphore, int * value);

/**
 * Wait on a semaphore for a specified time.
 *
 * @param semaphore Semaphore to wait on.
 * @param timeout Timeout in milliseconds.
 * @return 0 on success, -1 on failure.
 */
int gcu_semaphore_timedwait(GCU_Semaphore * semaphore, int timeout);

#ifdef __cplusplus
}
#endif

#endif // G_CUTIL_SEMAPHORE_H
