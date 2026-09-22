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
 * Making, removing and walking directories.
 *
 * Results are `GCU_File_Result` rather than a vocabulary of this module's own.
 * A directory that is not there and a file that is not there are the same
 * failure to a caller, and two identical enumerations would only make it
 * translate between them.
 *
 * The design and the reasoning behind each decision are recorded in
 * `documentation/file.md`.
 */

#ifndef GHOTI_IO_GCU_DIR_H
#define GHOTI_IO_GCU_DIR_H

#include <stdbool.h>
#include <stddef.h>
#include <ghoti.io/cutil/allocator.h>
#include <ghoti.io/cutil/file.h>
#include <ghoti.io/cutil/macros.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * An open directory being walked.
 *
 * Opaque, and disposed of by ::gcu_dir_close().  Treat the members as private.
 */
typedef struct GCU_Dir {
  void * handle;                   ///< Private.
  char * path;                     ///< Private.
  char * entry;                    ///< Private.
  const GCU_Allocator * allocator; ///< Private.
} GCU_Dir;

/**
 * Create a directory.
 *
 * One level only:  the parent must already exist.  ::gcu_dir_create_all() is
 * the one that builds a path.
 *
 * @param path The directory to create.
 * @return ::GCU_FILE_OK, ::GCU_FILE_ERR_INVALID, ::GCU_FILE_ERR_EXISTS,
 *   ::GCU_FILE_ERR_NOT_FOUND (no parent), ::GCU_FILE_ERR_ACCESS or
 *   ::GCU_FILE_ERR_IO.
 */
GCU_API GCU_File_Result gcu_dir_create(const char * path);

/**
 * Create a directory and every missing parent of it.
 *
 * Succeeds when the directory already exists, because the caller asked for it
 * to be there rather than for it to be new.  ::gcu_dir_create() is the one to
 * use when being first matters.
 *
 * A path that already exists as something other than a directory is
 * ::GCU_FILE_ERR_EXISTS, not a silent success.
 *
 * @param path The directory to ensure exists.
 * @param allocator Allocator for working memory, or NULL for the default.
 * @return ::GCU_FILE_OK, ::GCU_FILE_ERR_INVALID, ::GCU_FILE_ERR_EXISTS,
 *   ::GCU_FILE_ERR_OOM, ::GCU_FILE_ERR_ACCESS or ::GCU_FILE_ERR_IO.
 */
GCU_API GCU_File_Result gcu_dir_create_all(const char * path,
  const GCU_Allocator * allocator);

/**
 * Remove an empty directory.
 *
 * Empty only.  There is deliberately no recursive form:  deleting a tree is
 * the operation that most wants a symbolic link followed out of it by mistake,
 * and it needs a design conversation rather than a convenience function.  A
 * caller who wants one can write it over ::gcu_dir_read(), which reports
 * entry types without following links.
 *
 * @param path The directory to remove.
 * @return ::GCU_FILE_OK, ::GCU_FILE_ERR_INVALID, ::GCU_FILE_ERR_NOT_FOUND,
 *   ::GCU_FILE_ERR_NOT_EMPTY, ::GCU_FILE_ERR_ACCESS or ::GCU_FILE_ERR_IO.
 */
GCU_API GCU_File_Result gcu_dir_remove(const char * path);

/**
 * Create a uniquely named temporary directory.
 *
 * The directory is created, not merely named, and is readable only by its
 * owner - the same reasoning as ::gcu_file_temp_create(), and for the same
 * reason:  a name chosen and then used is a name somebody else can take first.
 *
 * Unlike a temporary file, nothing removes it for you.  It is a working area
 * whose lifetime the caller alone knows, and it is usually not empty when the
 * caller is finished with it.
 *
 * @param parent The directory to create it in, or NULL for the system
 *   temporary directory.
 * @param prefix A leading fragment for the name, or NULL for a default.
 * @param allocator Allocator for the returned path, or NULL for the default.
 * @param out_path Receives the path; free it with ::gcu_dir_free_path().
 * @return ::GCU_FILE_OK, ::GCU_FILE_ERR_INVALID, ::GCU_FILE_ERR_OOM or
 *   ::GCU_FILE_ERR_IO.
 */
GCU_API GCU_File_Result gcu_dir_temp_create(const char * parent,
  const char * prefix, const GCU_Allocator * allocator, char ** out_path);

/**
 * Release a path handed back by this module.
 *
 * @param allocator The allocator it came from, or NULL for the default.
 * @param path The path.  NULL is accepted and ignored.
 */
GCU_API void gcu_dir_free_path(const GCU_Allocator * allocator, char * path);

/**
 * Begin walking a directory.
 *
 * An iterator rather than a returned list, because a directory may hold more
 * entries than a caller can afford to hold at once and the caller is usually
 * looking for one of them.
 *
 * @param dir The handle to open.  Zeroed before anything can fail, so a handle
 *   from a failed open is safe to pass to ::gcu_dir_close().
 * @param path The directory to walk.
 * @param allocator Allocator for working memory, or NULL for the default.
 * @return ::GCU_FILE_OK, ::GCU_FILE_ERR_INVALID, ::GCU_FILE_ERR_NOT_FOUND,
 *   ::GCU_FILE_ERR_OOM, ::GCU_FILE_ERR_ACCESS or ::GCU_FILE_ERR_IO.
 */
GCU_API GCU_File_Result gcu_dir_open(GCU_Dir * dir, const char * path,
  const GCU_Allocator * allocator);

/**
 * Take the next entry from a walk.
 *
 * `"."` and `".."` are never reported.  They are an artefact of how the
 * filesystem stores a directory rather than things in it, and every caller
 * that has ever forgotten to skip them has walked its own parent.
 *
 * @p out_name points into storage owned by @p dir and is valid only until the
 * next call on the same handle.
 *
 * @param dir The handle.
 * @param out_name Receives the entry's name, not its path.
 * @param out_type Receives what the entry is, or NULL if that is not wanted.
 *   Reported without following a symbolic link, so a link to a directory is
 *   ::GCU_FILE_TYPE_SYMLINK.
 * @param out_done Set to true when the walk is finished; @p out_name is then
 *   unchanged.
 * @return ::GCU_FILE_OK, ::GCU_FILE_ERR_INVALID or ::GCU_FILE_ERR_IO.
 */
GCU_API GCU_File_Result gcu_dir_read(GCU_Dir * dir, const char ** out_name,
  GCU_File_Type * out_type, bool * out_done);

/**
 * Finish a walk and release the handle.
 *
 * @param dir The handle.  NULL, a zeroed handle and a handle already closed
 *   are all accepted and ignored, so this may sit on an unconditional cleanup
 *   path.
 */
GCU_API void gcu_dir_close(GCU_Dir * dir);

#ifdef __cplusplus
}
#endif

#endif // GHOTI_IO_GCU_DIR_H
