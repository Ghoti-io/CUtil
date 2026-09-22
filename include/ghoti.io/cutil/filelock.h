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
 * Whole-file locks between processes.
 *
 * `file.h` can replace a file atomically; this is the other half, for readers
 * and for writers that need to coordinate. Locks here are **between
 * processes**. Use a mutex or a read-write lock for threads -- these are much
 * more expensive and, on POSIX, do not exclude other threads in your own
 * process reliably.
 *
 * ## The lock is advisory on POSIX and mandatory on Windows
 *
 * This is the one difference a caller cannot be insulated from, so it is
 * stated first rather than buried.
 *
 * On POSIX a lock is **advisory**: it excludes other processes that ask for
 * the same lock, and does nothing whatsoever to a process that simply opens
 * the file and writes. On Windows a lock is **mandatory**: the range is
 * locked in the kernel and an unrelated process's `WriteFile` fails.
 *
 * Two consequences, and they pull in opposite directions:
 *
 *   - Code that relies on the lock actually preventing access is correct on
 *     Windows and does nothing on Linux.
 *   - Code that locks a file and then expects some other part of the same
 *     program -- a logger, a backup, an editor -- to keep writing it works on
 *     Linux and fails on Windows.
 *
 * Write for the advisory model. Every participant takes the lock; the lock is
 * a convention the participants keep, not a wall. That is correct on both.
 *
 * ## Which file, and what it leaves behind
 *
 * The path is opened, creating it if absent, and the lock covers the whole
 * file. The file is **not** removed on unlock: deleting a lock file is racy
 * -- another process can be holding a descriptor to it, or opening it by name
 * between your unlink and its open -- so a stale zero-byte lock file is the
 * deliberate outcome. It is not a leak; it is the lock's identity.
 *
 * A lock is released by gcu_file_unlock(), and by the process exiting for any
 * reason, including a crash. That is the property that makes this usable as a
 * "only one of me at a time" guard.
 *
 * ## Re-locking the same path from the same process
 *
 * Undefined, and on POSIX the second call may succeed rather than block,
 * because the lock belongs to the open file description rather than to the
 * thread. Do not use this to exclude your own threads.
 */

#ifndef GHOTI_IO_GCU_FILELOCK_H
#define GHOTI_IO_GCU_FILELOCK_H

#include <stdbool.h>
#include <ghoti.io/cutil/macros.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A held file lock.
 *
 * Declared here rather than allocated so that it can live on the stack.  Its
 * one member is platform-specific and is not part of the API; do not read it.
 */
typedef struct {
#ifdef _WIN32
  void * handle;   ///< Private.  HANDLE, or INVALID_HANDLE_VALUE.
#else
  int fd;          ///< Private.  File descriptor, or -1.
#endif
} GCU_File_Lock;

/** gcu_file_lock() could not take the lock, and did not wait. */
#define GCU_FILE_LOCK_BUSY 1

/**
 * Take a lock on @p path, creating the file if it does not exist.
 *
 * @param lock Receives the lock.  Pass the same pointer to gcu_file_unlock().
 * @param path The file to lock, UTF-8.
 * @param exclusive true for a writer's lock, which excludes everyone; false
 *   for a reader's lock, which excludes only writers.
 * @param wait true to block until the lock is available, false to return
 *   GCU_FILE_LOCK_BUSY rather than wait.
 * @return 0 if the lock is held, GCU_FILE_LOCK_BUSY if @p wait was false and
 *   someone else holds it, -1 on failure.  @p lock is only valid on 0.
 */
GCU_API int gcu_file_lock(GCU_File_Lock * lock, const char * path,
    bool exclusive, bool wait);

/**
 * Release a lock and close the file.
 *
 * @param lock A lock from a successful gcu_file_lock().  Cleared, so that a
 *   second release is refused rather than acting on a stale descriptor.
 * @return 0 on success, -1 on failure.
 */
GCU_API int gcu_file_unlock(GCU_File_Lock * lock);

#ifdef __cplusplus
}
#endif

#endif // GHOTI_IO_GCU_FILELOCK_H
