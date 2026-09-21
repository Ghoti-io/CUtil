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
 * Cross-platform semaphore implementation.
 */

#ifndef GHOTI_IO_GCU_SEMAPHORE_H
#define GHOTI_IO_GCU_SEMAPHORE_H

#include <ghoti.io/cutil/macros.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DOXYGEN
/**
 * Cross-platform semaphore type.
 */
typedef void GCU_Semaphore;
#endif // DOXYGEN


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
GCU_API int gcu_semaphore_create(GCU_Semaphore * semaphore, int value);


/**
 * Destroy a semaphore.
 *
 * @param semaphore Semaphore to destroy.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_semaphore_destroy(GCU_Semaphore * semaphore);

/**
 * Wait on a semaphore.
 *
 * @param semaphore Semaphore to wait on.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_semaphore_wait(GCU_Semaphore * semaphore);

/**
 * Signal (post/release) a semaphore.
 *
 * @param semaphore Semaphore to post.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_semaphore_signal(GCU_Semaphore * semaphore);

/**
 * Try to wait on a semaphore.
 *
 * @param semaphore Semaphore to wait on.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_semaphore_trywait(GCU_Semaphore * semaphore);

/**
 * Get the value of a semaphore.
 *
 * @param semaphore Semaphore to get value of.
 * @param value Pointer to value to set.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_semaphore_getvalue(GCU_Semaphore * semaphore, int * value);

/**
 * Wait on a semaphore for a specified time.
 *
 * @param semaphore Semaphore to wait on.
 * @param timeout Timeout in milliseconds.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_semaphore_timedwait(GCU_Semaphore * semaphore, int timeout);

#ifdef __cplusplus
}
#endif

#endif // GHOTI_IO_GCU_SEMAPHORE_H
