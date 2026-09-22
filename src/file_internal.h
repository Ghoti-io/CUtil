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
 * One translation from the platform's errno to this library's vocabulary,
 * shared by `file.c` and `dir.c`.
 *
 * It was written twice before it was written once, which is the same mistake
 * `documentation/file.md` section 2 is about, committed inside the module that
 * exists because of it.
 */

#ifndef GHOTI_IO_GCU_FILE_INTERNAL_H
#define GHOTI_IO_GCU_FILE_INTERNAL_H

#include <errno.h>
#include <ghoti.io/cutil/file.h>
#include <ghoti.io/cutil/macros.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Turn the platform's own complaint into this library's vocabulary.
 *
 * The distinctions drawn are the ones a caller can act on.  The dividing line
 * is not severity but **determinism**:  `GCU_FILE_ERR_IO` means the filesystem
 * tried and something went wrong, which a caller may reasonably retry or
 * report as a device problem.  A failure to resolve the path is not that - it
 * will never succeed on a retry, and it is a statement about the path rather
 * than about the device - so those become `GCU_FILE_ERR_NOT_FOUND` however
 * they are spelled:  the path is absent, a component of it is not a directory,
 * or it goes round a loop of symbolic links.
 *
 * `ENAMETOOLONG` is the one that looks like it belongs with those and does
 * not.  It is equally deterministic, but it says the *caller's argument*
 * cannot name anything on this filesystem rather than that nothing is there,
 * and the caller's answer to it is to fix its input rather than to create the
 * file.  That is `GCU_FILE_ERR_INVALID`.
 *
 * Everything else stays `GCU_FILE_ERR_IO`.  Nobody can usefully branch on the
 * difference between `EIO` and `ENXIO`.
 */
static inline GCU_File_Result gcu_file_internal_from_errno(int code) {
  switch (code) {
    case ENOENT:
    case ENOTDIR:
#if defined(ELOOP)
    case ELOOP:
#endif
      return GCU_FILE_ERR_NOT_FOUND;
#if defined(ENAMETOOLONG)
    case ENAMETOOLONG:
      return GCU_FILE_ERR_INVALID;
#endif
    case EEXIST:
      return GCU_FILE_ERR_EXISTS;
    case EACCES:
    case EPERM:
    // Reachable only from the writing paths - a read-only filesystem still
    // permits reads - but the table is shared, so it is stated once.
    case EROFS:
      return GCU_FILE_ERR_ACCESS;
    // Guarded on the value, not merely on the name: some platforms define
    // ENOTEMPTY as EEXIST, and a duplicate case label does not compile.
#if defined(ENOTEMPTY) && ENOTEMPTY != EEXIST
    case ENOTEMPTY:
      return GCU_FILE_ERR_NOT_EMPTY;
#endif
    case ENOMEM:
      return GCU_FILE_ERR_OOM;
    default:
      return GCU_FILE_ERR_IO;
  }
}

#ifdef __cplusplus
}
#endif

#endif // GHOTI_IO_GCU_FILE_INTERNAL_H
