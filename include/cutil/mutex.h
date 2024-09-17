/**
 * @file
 * 
 * This file implements cross-platform mutex functions.
 */

#ifndef G_CUTIL_MUTEX_H
#define G_CUTIL_MUTEX_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DOXYGEN
/**
 * Cross-platform mutex type.
 */
typedef void* GCU_MUTEX_TYPE;

/**
 * Cross-platform mutex creation.
 * 
 * @param x The variable which will hold the mutex.
 * @return 0 on success, non-zero on failure.
 */
#define GCU_MUTEX_CREATE(x)

/**
 * Cross-platform mutex cleanup.
 * 
 * @param x Mutex.
 */
#define GCU_MUTEX_DESTROY(x)

/**
 * Cross-platform mutex lock.
 * 
 * @param x Mutex.
 */
#define GCU_MUTEX_LOCK(x)

/**
 * Cross-platform mutex unlock.
 * 
 * @param x Mutex.
 */
#define GCU_MUTEX_UNLOCK(x)

/**
 * Cross-platform mutex trylock.
 * 
 * @param x Mutex.
 * @return true if lock was acquired, false otherwise.
 */
#define GCU_MUTEX_TRYLOCK(x)

#endif // DOXYGEN

#ifdef _WIN32
#include <windows.h>

#define GCU_MUTEX_T          HANDLE
#define GCU_MUTEX_CREATE(x)  !((x) = CreateMutex(NULL, FALSE, NULL))
#define GCU_MUTEX_DESTROY(x) CloseHandle(x)
#define GCU_MUTEX_LOCK(x)    WaitForSingleObject((x), INFINITE)
#define GCU_MUTEX_UNLOCK(x)  ReleaseMutex(x)
#define GCU_MUTEX_TRYLOCK(x) WaitForSingleObject((x), 0)

#else
#include <pthread.h>

#define GCU_MUTEX_T          pthread_mutex_t
#define GCU_MUTEX_CREATE(x)  pthread_mutex_init(&(x), NULL)
#define GCU_MUTEX_DESTROY(x) pthread_mutex_destroy(&(x))
#define GCU_MUTEX_LOCK(x)    pthread_mutex_lock(&(x))
#define GCU_MUTEX_UNLOCK(x)  pthread_mutex_unlock(&(x))
#define GCU_MUTEX_TRYLOCK(x) pthread_mutex_trylock(&(x))

#endif

#ifdef __cplusplus
}
#endif

#endif // G_CUTIL_MUTEX_H
