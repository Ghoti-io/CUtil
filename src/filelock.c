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
 * Whole-file locks between processes.  See filelock.h for the contract, and
 * for the advisory/mandatory difference this file cannot paper over.
 */

#ifndef _WIN32
// open() and O_CLOEXEC are 200809L; flock() is BSD and needs _DEFAULT_SOURCE.
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#endif

#include <stdlib.h>
#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/filelock.h>
#include <ghoti.io/cutil/utf.h>

#ifdef _WIN32

#include <windows.h>

int gcu_file_lock(GCU_File_Lock * lock, const char * path, bool exclusive,
    bool wait) {
  if (!lock || !path) {
    return -1;
  }

  size_t units = gcu_utf8_to_utf16(path, NULL, 0);
  if (!units) {
    return -1;
  }
  GCU_Char16 * wide = malloc(units * sizeof(GCU_Char16));
  if (!wide) {
    return -1;
  }
  gcu_utf8_to_utf16(path, wide, units);

  // FILE_SHARE_READ | FILE_SHARE_WRITE so that opening the file does not
  // itself exclude anyone:  the lock is what excludes, not the open.  Without
  // this, a second process could not even reach LockFileEx to discover the
  // lock, and would fail with a sharing violation instead of waiting.
  HANDLE handle = CreateFileW((LPCWSTR)wide, GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
      FILE_ATTRIBUTE_NORMAL, NULL);
  free(wide);
  if (handle == INVALID_HANDLE_VALUE) {
    return -1;
  }

  DWORD flags = 0;
  if (exclusive) {
    flags |= LOCKFILE_EXCLUSIVE_LOCK;
  }
  if (!wait) {
    flags |= LOCKFILE_FAIL_IMMEDIATELY;
  }

  OVERLAPPED overlapped;
  ZeroMemory(&overlapped, sizeof(overlapped));

  // MAXDWORD/MAXDWORD locks the whole file regardless of its current length,
  // which matters because the file is usually empty and a length-based range
  // would lock nothing.
  if (!LockFileEx(handle, flags, 0, MAXDWORD, MAXDWORD, &overlapped)) {
    DWORD error = GetLastError();
    CloseHandle(handle);
    return error == ERROR_LOCK_VIOLATION ? GCU_FILE_LOCK_BUSY : -1;
  }

  lock->handle = handle;
  return 0;
}

int gcu_file_unlock(GCU_File_Lock * lock) {
  if (!lock || !lock->handle || lock->handle == INVALID_HANDLE_VALUE) {
    return -1;
  }

  OVERLAPPED overlapped;
  ZeroMemory(&overlapped, sizeof(overlapped));
  BOOL unlocked = UnlockFileEx(lock->handle, 0, MAXDWORD, MAXDWORD,
      &overlapped);
  BOOL closed = CloseHandle(lock->handle);

  // Cleared whatever happened:  leaving a handle a caller might release again
  // is worse than losing the fact that one of the two calls failed.
  lock->handle = NULL;
  return (unlocked && closed) ? 0 : -1;
}

#else

#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

int gcu_file_lock(GCU_File_Lock * lock, const char * path, bool exclusive,
    bool wait) {
  if (!lock || !path) {
    return -1;
  }

  // O_CLOEXEC so that a fork/exec does not hand the lock to a child that
  // knows nothing about it and then outlives the parent holding it open.
  int fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC, 0666);
  if (fd < 0) {
    return -1;
  }

  // flock, not fcntl.  An fcntl lock is owned by the *process* and is
  // released when any descriptor to the file is closed -- so an unrelated
  // open-and-close of the same path elsewhere in the program silently drops
  // the lock.  A flock lock belongs to this open file description and does
  // not have that failure.
  int operation = exclusive ? LOCK_EX : LOCK_SH;
  if (!wait) {
    operation |= LOCK_NB;
  }

  while (flock(fd, operation) != 0) {
    // A blocking flock is interruptible; a signal is not a reason to report
    // that the file is locked by someone else.
    if (errno == EINTR) {
      continue;
    }
    int held_by_another = (errno == EWOULDBLOCK);
    close(fd);
    return held_by_another ? GCU_FILE_LOCK_BUSY : -1;
  }

  lock->fd = fd;
  return 0;
}

int gcu_file_unlock(GCU_File_Lock * lock) {
  if (!lock || lock->fd < 0) {
    return -1;
  }

  // close() releases the flock on its own, but the unlock is explicit so that
  // the release is not a side effect a later refactor can drop by reordering.
  int unlocked = flock(lock->fd, LOCK_UN);
  int closed = close(lock->fd);

  lock->fd = -1;
  return (unlocked == 0 && closed == 0) ? 0 : -1;
}

#endif
