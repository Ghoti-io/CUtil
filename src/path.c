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
 * Path manipulation: a lexical half that takes its rules as a parameter, and
 * an environment half that asks the operating system.
 *
 * The design and the reasoning behind each decision are recorded in
 * `documentation/path.md`.
 */

/* getpwuid_r() and realpath() are POSIX, and this library compiles with
 * -std=c17, which declares neither.  The feature-test macro goes at the very
 * top, before any header: glibc reads it when the first one is included and
 * ignores it afterwards.  The suite spells it this way in cutil's thread.c
 * and in chron's zone/local.c. */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <ghoti.io/cutil/path.h>
#include "path_internal.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <unistd.h>
#endif

/**
 * The longest path the environment functions will assemble before giving up.
 *
 * This is a guard against a hostile or broken environment variable, not a
 * limit on paths: every lexical function here is bounded only by its inputs.
 */
#define GCU_PATH_ENV_MAX ((size_t)65536)

//////////////////////////////////////////////////////////////////////////////
// Flavour primitives
//////////////////////////////////////////////////////////////////////////////

char gcu_path_separator(GCU_Path_Flavor flavor) {
  return flavor == GCU_PATH_WINDOWS ? '\\' : '/';
}

bool gcu_path_is_separator(GCU_Path_Flavor flavor, char c) {
  if (c == '/') {
    return true;
  }
  return flavor == GCU_PATH_WINDOWS && c == '\\';
}

/** ASCII only, deliberately: see gcu_path_relative_to()'s documentation. */
static char path_lower(char c) {
  return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static bool path_is_drive_letter(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/**
 * Whether a Windows path opens with the `\\?\` or `\\.\` prefix, which Win32
 * hands to the object manager without parsing.
 */
static bool path_is_extended(GCU_Path_Flavor flavor, const char * path) {
  if (flavor != GCU_PATH_WINDOWS || !path) {
    return false;
  }
  return gcu_path_is_separator(flavor, path[0])
      && gcu_path_is_separator(flavor, path[1])
      && (path[2] == '?' || path[2] == '.')
      && gcu_path_is_separator(flavor, path[3]);
}

size_t gcu_path_root_length(GCU_Path_Flavor flavor, const char * path) {
  if (!path || !path[0]) {
    return 0;
  }

  if (flavor == GCU_PATH_POSIX) {
    return gcu_path_is_separator(flavor, path[0]) ? 1 : 0;
  }

  // \\?\C:\ and \\?\UNC\server\share both begin with a four-byte prefix; what
  // follows is parsed the same way as an ordinary root so that the two forms
  // do not need separate walkers.
  size_t base = 0;
  if (path_is_extended(flavor, path)) {
    base = 4;
    if (path_is_drive_letter(path[base]) && path[base + 1] == ':') {
      return gcu_path_is_separator(flavor, path[base + 2])
          ? base + 3 : base + 2;
    }
    // \\?\UNC\server\share - step over "UNC" and fall into the share walk.
    if (path_lower(path[base]) == 'u' && path_lower(path[base + 1]) == 'n'
        && path_lower(path[base + 2]) == 'c'
        && gcu_path_is_separator(flavor, path[base + 3])) {
      size_t i = base + 4;
      while (path[i] && !gcu_path_is_separator(flavor, path[i])) {
        ++i;
      }
      if (path[i]) {
        ++i;
      }
      while (path[i] && !gcu_path_is_separator(flavor, path[i])) {
        ++i;
      }
      return i;
    }
    return base;
  }

  // \\server\share
  if (gcu_path_is_separator(flavor, path[0])
      && gcu_path_is_separator(flavor, path[1])) {
    size_t i = 2;
    while (path[i] && !gcu_path_is_separator(flavor, path[i])) {
      ++i;
    }
    if (path[i]) {
      ++i;
    }
    while (path[i] && !gcu_path_is_separator(flavor, path[i])) {
      ++i;
    }
    return i;
  }

  // C:\ is rooted; C: alone is relative to that drive's own directory.
  if (path_is_drive_letter(path[0]) && path[1] == ':') {
    return gcu_path_is_separator(flavor, path[2]) ? 3 : 2;
  }

  if (gcu_path_is_separator(flavor, path[0])) {
    return 1;
  }
  return 0;
}

bool gcu_path_is_absolute(GCU_Path_Flavor flavor, const char * path) {
  if (!path || !path[0]) {
    return false;
  }
  if (flavor == GCU_PATH_POSIX) {
    return gcu_path_is_separator(flavor, path[0]);
  }
  if (path_is_extended(flavor, path)) {
    return true;
  }
  if (gcu_path_is_separator(flavor, path[0])
      && gcu_path_is_separator(flavor, path[1])) {
    return true;
  }
  // A drive letter alone (C:x) names a file relative to that drive's current
  // directory, and a leading separator without one (\x) names a file relative
  // to the current drive.  Neither is absolute.
  return path_is_drive_letter(path[0]) && path[1] == ':'
      && gcu_path_is_separator(flavor, path[2]);
}

/**
 * Whether `..` has a floor: a root it must not climb above.
 *
 * Every rooted form has one.  A bare drive (`C:`) does not, because what it
 * is relative to - that drive's current directory - can itself have a parent.
 */
static bool path_has_floor(GCU_Path_Flavor flavor, const char * path,
    size_t root_len) {
  if (root_len == 0) {
    return false;
  }
  if (flavor == GCU_PATH_WINDOWS && root_len == 2 && path[1] == ':') {
    return false;
  }
  return true;
}

//////////////////////////////////////////////////////////////////////////////
// Pointer-into-input accessors
//////////////////////////////////////////////////////////////////////////////

const char * gcu_path_basename(GCU_Path_Flavor flavor, const char * path) {
  if (!path) {
    return NULL;
  }
  size_t root_len = gcu_path_root_length(flavor, path);
  size_t len = strlen(path);
  size_t start = root_len;
  for (size_t i = root_len; i < len; ++i) {
    if (gcu_path_is_separator(flavor, path[i])) {
      start = i + 1;
    }
  }
  return path + start;
}

const char * gcu_path_extension(GCU_Path_Flavor flavor, const char * path) {
  const char * base = gcu_path_basename(flavor, path);
  if (!base || !base[0]) {
    return NULL;
  }
  const char * dot = NULL;
  for (const char * p = base; *p; ++p) {
    if (*p == '.') {
      dot = p;
    }
  }
  // A leading dot makes a hidden file, not an extension.
  if (!dot || dot == base) {
    return NULL;
  }
  return dot;
}

//////////////////////////////////////////////////////////////////////////////
// The buffer contract
//////////////////////////////////////////////////////////////////////////////

/**
 * A write cursor that counts when it has nowhere to write.
 *
 * Every emitter below runs twice: once with `buf` NULL to learn the length,
 * and once for real, only if the caller's buffer turned out to be big enough.
 * Running the same code both times is what keeps the measured length and the
 * written one from drifting apart.
 */
typedef struct path_writer {
  char * buf;  ///< NULL while measuring.
  size_t cap;  ///< Bytes available at `buf`, excluding the terminator.
  size_t len;  ///< Bytes emitted so far.
} path_writer;

static void path_put(path_writer * w, char c) {
  if (w->buf && w->len < w->cap) {
    w->buf[w->len] = c;
  }
  ++w->len;
}

static void path_put_bytes(path_writer * w, const char * bytes, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    path_put(w, bytes[i]);
  }
}

/** Emit a root, rewriting its separators to the ones this flavour writes. */
static void path_put_root(path_writer * w, GCU_Path_Flavor flavor,
    const char * path, size_t root_len) {
  char sep = gcu_path_separator(flavor);
  for (size_t i = 0; i < root_len; ++i) {
    path_put(w, gcu_path_is_separator(flavor, path[i]) ? sep : path[i]);
  }
}

/** An emitter: fills @p w, and may refuse. */
typedef GCU_Path_Result (*path_emitter)(
  path_writer * w, GCU_Path_Flavor flavor, const char * a, const char * b);

/**
 * Run an emitter under the buffer contract.
 *
 * The measuring pass runs first and unconditionally, so a caller who supplied
 * a buffer that does not fit is told the size it needs without a byte of that
 * buffer being touched.
 */
static GCU_Path_Result path_run(path_emitter emit, GCU_Path_Flavor flavor,
    const char * a, const char * b, char * out, size_t out_size,
    size_t * out_len) {
  if (flavor != GCU_PATH_POSIX && flavor != GCU_PATH_WINDOWS) {
    return GCU_PATH_ERR_INVALID;
  }
  if (!out && out_size) {
    return GCU_PATH_ERR_INVALID;
  }

  path_writer measure = {NULL, 0, 0};
  GCU_Path_Result result = emit(&measure, flavor, a, b);
  if (result != GCU_PATH_OK) {
    return result;
  }

  if (out_len) {
    *out_len = measure.len;
  }
  if (!out) {
    return GCU_PATH_OK;
  }
  if (measure.len + 1 > out_size) {
    return GCU_PATH_ERR_LIMIT;
  }

  path_writer write = {out, out_size - 1, 0};
  result = emit(&write, flavor, a, b);
  if (result != GCU_PATH_OK) {
    return result;
  }
  out[write.len] = '\0';
  return GCU_PATH_OK;
}

//////////////////////////////////////////////////////////////////////////////
// Component iteration
//////////////////////////////////////////////////////////////////////////////

/**
 * Step to the component left of @p end, skipping separators.
 *
 * Iterating right to left is what lets ::gcu_path_normalize() resolve `..`
 * without a stack: a `..` seen on the way left simply cancels the next
 * ordinary component it meets.  Going the other way would need to remember
 * every component it had passed, which is an allocation whose size depends on
 * the input - exactly what this module is trying not to require.
 */
static bool path_prev_component(GCU_Path_Flavor flavor, const char * path,
    size_t floor, size_t * end, size_t * comp_start, size_t * comp_end) {
  size_t e = *end;
  while (e > floor && gcu_path_is_separator(flavor, path[e - 1])) {
    --e;
  }
  if (e == floor) {
    *end = e;
    return false;
  }
  size_t s = e;
  while (s > floor && !gcu_path_is_separator(flavor, path[s - 1])) {
    --s;
  }
  *comp_start = s;
  *comp_end = e;
  *end = s;
  return true;
}

static bool path_component_is(const char * path, size_t start, size_t end,
    const char * literal) {
  size_t n = strlen(literal);
  return (end - start) == n && memcmp(path + start, literal, n) == 0;
}

//////////////////////////////////////////////////////////////////////////////
// Lexical operations
//////////////////////////////////////////////////////////////////////////////

static GCU_Path_Result path_emit_dirname(path_writer * w,
    GCU_Path_Flavor flavor, const char * path, const char * unused) {
  (void)unused;
  if (!path) {
    return GCU_PATH_ERR_INVALID;
  }

  size_t root_len = gcu_path_root_length(flavor, path);
  size_t len = strlen(path);

  // Trailing separators are not a component, so "/a/b/" has the same parent
  // as "/a/b".
  while (len > root_len && gcu_path_is_separator(flavor, path[len - 1])) {
    --len;
  }

  size_t cut = root_len;
  bool found = false;
  for (size_t i = root_len; i < len; ++i) {
    if (gcu_path_is_separator(flavor, path[i])) {
      cut = i;
      found = true;
    }
  }

  if (!found) {
    if (root_len) {
      path_put_root(w, flavor, path, root_len);
    }
    else {
      path_put(w, '.');
    }
    return GCU_PATH_OK;
  }

  while (cut > root_len && gcu_path_is_separator(flavor, path[cut - 1])) {
    --cut;
  }
  if (cut <= root_len) {
    path_put_root(w, flavor, path, root_len);
    return GCU_PATH_OK;
  }

  path_put_root(w, flavor, path, root_len);
  char sep = gcu_path_separator(flavor);
  for (size_t i = root_len; i < cut; ++i) {
    path_put(w, gcu_path_is_separator(flavor, path[i]) ? sep : path[i]);
  }
  return GCU_PATH_OK;
}

GCU_Path_Result gcu_path_dirname(GCU_Path_Flavor flavor, const char * path,
    char * out, size_t out_size, size_t * out_len) {
  return path_run(path_emit_dirname, flavor, path, NULL, out, out_size,
      out_len);
}

static GCU_Path_Result path_emit_join(path_writer * w, GCU_Path_Flavor flavor,
    const char * base, const char * relative) {
  if (!base) {
    base = "";
  }
  if (!relative) {
    relative = "";
  }

  if (!relative[0]) {
    path_put_bytes(w, base, strlen(base));
    return GCU_PATH_OK;
  }
  if (!base[0] || gcu_path_is_absolute(flavor, relative)) {
    path_put_bytes(w, relative, strlen(relative));
    return GCU_PATH_OK;
  }

  // A Windows path that is rooted but not absolute ("\x") keeps the drive it
  // is joined onto and replaces everything after it, which is what the
  // operating system does with one.
  if (flavor == GCU_PATH_WINDOWS
      && gcu_path_is_separator(flavor, relative[0])) {
    size_t root_len = gcu_path_root_length(flavor, base);
    while (root_len > 0 && gcu_path_is_separator(flavor, base[root_len - 1])) {
      --root_len;
    }
    path_put_bytes(w, base, root_len);
    path_put_bytes(w, relative, strlen(relative));
    return GCU_PATH_OK;
  }

  size_t base_len = strlen(base);
  path_put_bytes(w, base, base_len);
  if (!gcu_path_is_separator(flavor, base[base_len - 1])) {
    path_put(w, gcu_path_separator(flavor));
  }
  path_put_bytes(w, relative, strlen(relative));
  return GCU_PATH_OK;
}

GCU_Path_Result gcu_path_join(GCU_Path_Flavor flavor, const char * base,
    const char * relative, char * out, size_t out_size, size_t * out_len) {
  return path_run(path_emit_join, flavor, base, relative, out, out_size,
      out_len);
}

/**
 * Survey the components of a path once, right to left.
 *
 * Reports how many survive normalisation, how many bytes they occupy, and how
 * many leading `..` the result needs.  ::path_emit_normalize() runs this to
 * size the result and then walks the same path again to place it, which is
 * why both directions agree about what survives.
 */
static void path_survey(GCU_Path_Flavor flavor, const char * path,
    size_t root_len, bool floored, size_t * out_kept, size_t * out_bytes,
    size_t * out_leading) {
  size_t end = strlen(path);
  size_t skip = 0;
  size_t kept = 0;
  size_t bytes = 0;
  size_t cs = 0;
  size_t ce = 0;

  while (path_prev_component(flavor, path, root_len, &end, &cs, &ce)) {
    if (path_component_is(path, cs, ce, ".")) {
      continue;
    }
    if (path_component_is(path, cs, ce, "..")) {
      ++skip;
      continue;
    }
    if (skip) {
      --skip;
      continue;
    }
    ++kept;
    bytes += ce - cs;
  }

  *out_kept = kept;
  *out_bytes = bytes;
  // A `..` that runs out of components to cancel escapes a relative path and
  // is simply absorbed by a rooted one, because a root has no parent.
  *out_leading = floored ? 0 : skip;
}

static GCU_Path_Result path_emit_normalize(path_writer * w,
    GCU_Path_Flavor flavor, const char * path, const char * unused) {
  (void)unused;
  if (!path) {
    return GCU_PATH_ERR_INVALID;
  }

  // An extended Windows path is handed to the object manager verbatim, with
  // no parsing of "." or ".." at all, so normalising one would change which
  // file it names.
  if (path_is_extended(flavor, path)) {
    path_put_bytes(w, path, strlen(path));
    return GCU_PATH_OK;
  }

  size_t root_len = gcu_path_root_length(flavor, path);
  bool floored = path_has_floor(flavor, path, root_len);
  bool drive_relative =
      flavor == GCU_PATH_WINDOWS && root_len == 2 && path[1] == ':';
  bool root_ends_separator =
      root_len > 0 && gcu_path_is_separator(flavor, path[root_len - 1]);
  // A UNC root ("\\server\share") ends on a share name rather than a
  // separator, so one has to be put back before the first component.
  bool separator_after_root =
      root_len > 0 && !root_ends_separator && !drive_relative;

  size_t kept = 0;
  size_t bytes = 0;
  size_t leading = 0;
  path_survey(flavor, path, root_len, floored, &kept, &bytes, &leading);

  size_t components = kept + leading;
  if (components == 0) {
    if (root_len) {
      path_put_root(w, flavor, path, root_len);
    }
    else {
      path_put(w, '.');
    }
    return GCU_PATH_OK;
  }

  size_t total = root_len + bytes + (leading * 2) + (components - 1)
      + (separator_after_root ? 1 : 0);

  // Measuring only needs the length, and the placement below writes from the
  // right, which it cannot do without a buffer to write into.
  if (!w->buf) {
    w->len = total;
    return GCU_PATH_OK;
  }

  char sep = gcu_path_separator(flavor);
  size_t pos = total;
  size_t placed = 0;
  size_t end = strlen(path);
  size_t skip = 0;
  size_t cs = 0;
  size_t ce = 0;

  while (path_prev_component(flavor, path, root_len, &end, &cs, &ce)) {
    if (path_component_is(path, cs, ce, ".")) {
      continue;
    }
    if (path_component_is(path, cs, ce, "..")) {
      ++skip;
      continue;
    }
    if (skip) {
      --skip;
      continue;
    }
    if (placed) {
      w->buf[--pos] = sep;
    }
    size_t n = ce - cs;
    pos -= n;
    memcpy(w->buf + pos, path + cs, n);
    ++placed;
  }

  for (size_t i = 0; i < leading; ++i) {
    if (placed) {
      w->buf[--pos] = sep;
    }
    pos -= 2;
    w->buf[pos] = '.';
    w->buf[pos + 1] = '.';
    ++placed;
  }

  if (separator_after_root) {
    w->buf[--pos] = sep;
  }
  for (size_t i = 0; i < root_len; ++i) {
    w->buf[i] = gcu_path_is_separator(flavor, path[i]) ? sep : path[i];
  }

  w->len = total;
  return GCU_PATH_OK;
}

GCU_Path_Result gcu_path_normalize(GCU_Path_Flavor flavor, const char * path,
    char * out, size_t out_size, size_t * out_len) {
  return path_run(path_emit_normalize, flavor, path, NULL, out, out_size,
      out_len);
}

static GCU_Path_Result path_emit_separators(path_writer * w,
    GCU_Path_Flavor flavor, const char * path, char target) {
  if (!path) {
    return GCU_PATH_ERR_INVALID;
  }
  size_t len = strlen(path);
  // POSIX has nothing to rewrite - a backslash there is part of a filename -
  // and an extended Windows path is not parsed, so a slash inside one is a
  // literal too.
  if (flavor == GCU_PATH_POSIX || path_is_extended(flavor, path)) {
    path_put_bytes(w, path, len);
    return GCU_PATH_OK;
  }
  for (size_t i = 0; i < len; ++i) {
    path_put(w, gcu_path_is_separator(flavor, path[i]) ? target : path[i]);
  }
  return GCU_PATH_OK;
}

static GCU_Path_Result path_emit_to_native(path_writer * w,
    GCU_Path_Flavor flavor, const char * path, const char * unused) {
  (void)unused;
  return path_emit_separators(w, flavor, path, gcu_path_separator(flavor));
}

static GCU_Path_Result path_emit_to_posix(path_writer * w,
    GCU_Path_Flavor flavor, const char * path, const char * unused) {
  (void)unused;
  return path_emit_separators(w, flavor, path, '/');
}

GCU_Path_Result gcu_path_to_native(GCU_Path_Flavor flavor, const char * path,
    char * out, size_t out_size, size_t * out_len) {
  return path_run(path_emit_to_native, flavor, path, NULL, out, out_size,
      out_len);
}

GCU_Path_Result gcu_path_to_posix(GCU_Path_Flavor flavor, const char * path,
    char * out, size_t out_size, size_t * out_len) {
  return path_run(path_emit_to_posix, flavor, path, NULL, out, out_size,
      out_len);
}

//////////////////////////////////////////////////////////////////////////////
// relative_to
//////////////////////////////////////////////////////////////////////////////

static bool path_roots_match(GCU_Path_Flavor flavor, const char * a,
    const char * b) {
  size_t ra = gcu_path_root_length(flavor, a);
  size_t rb = gcu_path_root_length(flavor, b);
  if (ra != rb) {
    return false;
  }
  for (size_t i = 0; i < ra; ++i) {
    char ca = a[i];
    char cb = b[i];
    if (flavor == GCU_PATH_WINDOWS) {
      ca = path_lower(ca);
      cb = path_lower(cb);
    }
    if (ca != cb) {
      return false;
    }
  }
  return true;
}

/** Step to the component right of @p start, skipping separators. */
static bool path_next_component(GCU_Path_Flavor flavor, const char * path,
    size_t * start, size_t * comp_start, size_t * comp_end) {
  size_t s = *start;
  while (path[s] && gcu_path_is_separator(flavor, path[s])) {
    ++s;
  }
  if (!path[s]) {
    *start = s;
    return false;
  }
  size_t e = s;
  while (path[e] && !gcu_path_is_separator(flavor, path[e])) {
    ++e;
  }
  *comp_start = s;
  *comp_end = e;
  *start = e;
  return true;
}

static bool path_components_equal(GCU_Path_Flavor flavor, const char * a,
    size_t as, size_t ae, const char * b, size_t bs, size_t be) {
  if ((ae - as) != (be - bs)) {
    return false;
  }
  for (size_t i = 0; i < ae - as; ++i) {
    char ca = a[as + i];
    char cb = b[bs + i];
    if (flavor == GCU_PATH_WINDOWS) {
      ca = path_lower(ca);
      cb = path_lower(cb);
    }
    if (ca != cb) {
      return false;
    }
  }
  return true;
}

/** Normalise into memory owned by @p allocator.  NULL on failure. */
static char * path_normalize_alloc(GCU_Path_Flavor flavor, const char * path,
    const GCU_Allocator * allocator) {
  size_t need = 0;
  if (gcu_path_normalize(flavor, path, NULL, 0, &need) != GCU_PATH_OK) {
    return NULL;
  }
  char * buffer = (char *)gcu_allocator_malloc(allocator, need + 1);
  if (!buffer) {
    return NULL;
  }
  if (gcu_path_normalize(flavor, path, buffer, need + 1, NULL)
      != GCU_PATH_OK) {
    gcu_allocator_free(allocator, buffer);
    return NULL;
  }
  return buffer;
}

GCU_Path_Result gcu_path_relative_to(GCU_Path_Flavor flavor,
    const char * from, const char * to, const GCU_Allocator * allocator,
    char * out, size_t out_size, size_t * out_len) {
  if (flavor != GCU_PATH_POSIX && flavor != GCU_PATH_WINDOWS) {
    return GCU_PATH_ERR_INVALID;
  }
  if (!from || !to || (!out && out_size)) {
    return GCU_PATH_ERR_INVALID;
  }
  if (!allocator) {
    allocator = gcu_allocator_default();
  }

  char * nf = path_normalize_alloc(flavor, from, allocator);
  char * nt = path_normalize_alloc(flavor, to, allocator);
  if (!nf || !nt) {
    gcu_allocator_free(allocator, nf);
    gcu_allocator_free(allocator, nt);
    return GCU_PATH_ERR_OOM;
  }

  GCU_Path_Result result = GCU_PATH_OK;

  // Two paths with different roots - different drives, or one absolute and
  // one not - have no relative path between them to express.
  if (gcu_path_is_absolute(flavor, nf) != gcu_path_is_absolute(flavor, nt)
      || !path_roots_match(flavor, nf, nt)) {
    result = GCU_PATH_ERR_UNSUPPORTED;
  }
  // Where a leading ".." sits depends on a current directory this function
  // was not given, so the answer would be a guess.
  else if (nf[0] == '.' && nf[1] == '.'
      && (nf[2] == '\0' || gcu_path_is_separator(flavor, nf[2]))) {
    result = GCU_PATH_ERR_UNSUPPORTED;
  }
  else if (nt[0] == '.' && nt[1] == '.'
      && (nt[2] == '\0' || gcu_path_is_separator(flavor, nt[2]))) {
    result = GCU_PATH_ERR_UNSUPPORTED;
  }

  if (result != GCU_PATH_OK) {
    gcu_allocator_free(allocator, nf);
    gcu_allocator_free(allocator, nt);
    return result;
  }

  size_t root_len = gcu_path_root_length(flavor, nf);
  size_t fi = root_len;
  size_t ti = root_len;
  size_t fcs = 0;
  size_t fce = 0;
  size_t tcs = 0;
  size_t tce = 0;

  // Walk off the shared prefix.  A normalised path has no "." or ".."
  // components left in it, so the comparison is a plain one.
  for (;;) {
    size_t fi_save = fi;
    size_t ti_save = ti;
    bool have_f = path_next_component(flavor, nf, &fi, &fcs, &fce);
    bool have_t = path_next_component(flavor, nt, &ti, &tcs, &tce);
    if (!have_f || !have_t
        || !path_components_equal(flavor, nf, fcs, fce, nt, tcs, tce)) {
      fi = fi_save;
      ti = ti_save;
      break;
    }
  }

  // "." is what a normalised empty path is called, and it has no components
  // to walk off; treat it as already at the shared prefix.
  bool from_is_dot = strcmp(nf, ".") == 0;
  bool to_is_dot = strcmp(nt, ".") == 0;

  size_t ups = 0;
  if (!from_is_dot) {
    size_t scan = fi;
    while (path_next_component(flavor, nf, &scan, &fcs, &fce)) {
      ++ups;
    }
  }

  path_writer measure = {NULL, 0, 0};
  char sep = gcu_path_separator(flavor);
  size_t placed = 0;

  for (size_t pass = 0; pass < 2; ++pass) {
    path_writer * w = &measure;
    path_writer write = {out, out_size ? out_size - 1 : 0, 0};
    if (pass == 1) {
      w = &write;
    }
    placed = 0;

    for (size_t i = 0; i < ups; ++i) {
      if (placed) {
        path_put(w, sep);
      }
      path_put(w, '.');
      path_put(w, '.');
      ++placed;
    }
    if (!to_is_dot) {
      size_t scan = ti;
      size_t cs = 0;
      size_t ce = 0;
      while (path_next_component(flavor, nt, &scan, &cs, &ce)) {
        if (placed) {
          path_put(w, sep);
        }
        path_put_bytes(w, nt + cs, ce - cs);
        ++placed;
      }
    }
    if (!placed) {
      path_put(w, '.');
    }

    if (pass == 0) {
      if (out_len) {
        *out_len = measure.len;
      }
      if (!out) {
        break;
      }
      if (measure.len + 1 > out_size) {
        result = GCU_PATH_ERR_LIMIT;
        break;
      }
    }
    else {
      out[write.len] = '\0';
    }
  }

  gcu_allocator_free(allocator, nf);
  gcu_allocator_free(allocator, nt);
  return result;
}

//////////////////////////////////////////////////////////////////////////////
// Environment
//////////////////////////////////////////////////////////////////////////////

const char * gcu_path_result_string(GCU_Path_Result result) {
  switch (result) {
    case GCU_PATH_OK:              return "ok";
    case GCU_PATH_ERR_INVALID:     return "invalid argument";
    case GCU_PATH_ERR_OOM:         return "out of memory";
    case GCU_PATH_ERR_LIMIT:       return "buffer too small";
    case GCU_PATH_ERR_IO:          return "operating system query failed";
    case GCU_PATH_ERR_UNSUPPORTED: return "no such path";
    case GCU_PATH_RESULT_COUNT:    break;
  }
  return "unknown";
}

void gcu_path_free(const GCU_Allocator * allocator, char * path) {
  if (!path) {
    return;
  }
  if (!allocator) {
    allocator = gcu_allocator_default();
  }
  gcu_allocator_free(allocator, path);
}

/** Copy a NUL-terminated string into allocator memory. */
static char * path_dup(const GCU_Allocator * allocator, const char * source,
    size_t len) {
  char * copy = (char *)gcu_allocator_malloc(allocator, len + 1);
  if (!copy) {
    return NULL;
  }
  memcpy(copy, source, len);
  copy[len] = '\0';
  return copy;
}

/** Copy a string, dropping any trailing separators above the root. */
static char * path_dup_trimmed(const GCU_Allocator * allocator,
    const char * source) {
  size_t len = strlen(source);
  size_t root_len = gcu_path_root_length(GCU_PATH_NATIVE, source);
  while (len > root_len
      && gcu_path_is_separator(GCU_PATH_NATIVE, source[len - 1])) {
    --len;
  }
  return path_dup(allocator, source, len);
}

/**
 * Read an environment variable that is expected to hold a path.
 *
 * An empty value is treated as absent, because an exported-but-empty variable
 * is how a shell says "unset" far more often than it means "the root".
 */
static const char * path_env(const char * name) {
  const char * value = getenv(name);
  if (!value || !value[0]) {
    return NULL;
  }
  if (strlen(value) > GCU_PATH_ENV_MAX) {
    return NULL;
  }
  return value;
}

#ifdef _WIN32

/* TODO(windows): none of the branches in this file have been run on Windows.
 * Verifying them needs a MSYS2 MINGW64 machine.  What "done" looks like: test-path's environment cases pass, and a path holding
 * non-ASCII characters survives a round trip through gcu_path_cwd(). */

/** Read an environment variable as UTF-8, through the wide API. */
static char * path_env_wide(const GCU_Allocator * allocator,
    const wchar_t * name) {
  DWORD needed = GetEnvironmentVariableW(name, NULL, 0);
  if (needed == 0) {
    return NULL;
  }
  wchar_t * wide =
      (wchar_t *)gcu_allocator_malloc(allocator, needed * sizeof(wchar_t));
  if (!wide) {
    return NULL;
  }
  DWORD written = GetEnvironmentVariableW(name, wide, needed);
  if (written == 0 || written >= needed) {
    gcu_allocator_free(allocator, wide);
    return NULL;
  }
  char * result = gcu_path_internal_from_wide(allocator, wide);
  gcu_allocator_free(allocator, wide);
  return result;
}

GCU_Path_Result gcu_path_cwd(const GCU_Allocator * allocator, char ** out) {
  if (!out) {
    return GCU_PATH_ERR_INVALID;
  }
  if (!allocator) {
    allocator = gcu_allocator_default();
  }
  DWORD needed = GetCurrentDirectoryW(0, NULL);
  if (needed == 0) {
    return GCU_PATH_ERR_IO;
  }
  wchar_t * wide =
      (wchar_t *)gcu_allocator_malloc(allocator, needed * sizeof(wchar_t));
  if (!wide) {
    return GCU_PATH_ERR_OOM;
  }
  if (GetCurrentDirectoryW(needed, wide) == 0) {
    gcu_allocator_free(allocator, wide);
    return GCU_PATH_ERR_IO;
  }
  char * utf8 = gcu_path_internal_from_wide(allocator, wide);
  gcu_allocator_free(allocator, wide);
  if (!utf8) {
    return GCU_PATH_ERR_OOM;
  }
  *out = utf8;
  return GCU_PATH_OK;
}

GCU_Path_Result gcu_path_home(const GCU_Allocator * allocator, char ** out) {
  if (!out) {
    return GCU_PATH_ERR_INVALID;
  }
  if (!allocator) {
    allocator = gcu_allocator_default();
  }
  char * value = path_env_wide(allocator, L"USERPROFILE");
  if (!value) {
    return GCU_PATH_ERR_IO;
  }
  char * trimmed = path_dup_trimmed(allocator, value);
  gcu_allocator_free(allocator, value);
  if (!trimmed) {
    return GCU_PATH_ERR_OOM;
  }
  *out = trimmed;
  return GCU_PATH_OK;
}

/** %APPDATA% and %LOCALAPPDATA%, falling back to the profile directory. */
static GCU_Path_Result path_known_dir(const GCU_Allocator * allocator,
    const wchar_t * variable, const char * fallback_suffix, char ** out) {
  char * value = path_env_wide(allocator, variable);
  if (value) {
    char * trimmed = path_dup_trimmed(allocator, value);
    gcu_allocator_free(allocator, value);
    if (!trimmed) {
      return GCU_PATH_ERR_OOM;
    }
    *out = trimmed;
    return GCU_PATH_OK;
  }

  char * home = NULL;
  GCU_Path_Result result = gcu_path_home(allocator, &home);
  if (result != GCU_PATH_OK) {
    return result;
  }
  size_t need = 0;
  if (gcu_path_join(GCU_PATH_NATIVE, home, fallback_suffix, NULL, 0, &need)
      != GCU_PATH_OK) {
    gcu_path_free(allocator, home);
    return GCU_PATH_ERR_IO;
  }
  char * joined = (char *)gcu_allocator_malloc(allocator, need + 1);
  if (!joined) {
    gcu_path_free(allocator, home);
    return GCU_PATH_ERR_OOM;
  }
  result = gcu_path_join(GCU_PATH_NATIVE, home, fallback_suffix, joined,
      need + 1, NULL);
  gcu_path_free(allocator, home);
  if (result != GCU_PATH_OK) {
    gcu_allocator_free(allocator, joined);
    return result;
  }
  *out = joined;
  return GCU_PATH_OK;
}

GCU_Path_Result gcu_path_config_dir(const GCU_Allocator * allocator,
    char ** out) {
  if (!out) {
    return GCU_PATH_ERR_INVALID;
  }
  if (!allocator) {
    allocator = gcu_allocator_default();
  }
  return path_known_dir(allocator, L"APPDATA", "AppData\\Roaming", out);
}

GCU_Path_Result gcu_path_data_dir(const GCU_Allocator * allocator,
    char ** out) {
  if (!out) {
    return GCU_PATH_ERR_INVALID;
  }
  if (!allocator) {
    allocator = gcu_allocator_default();
  }
  return path_known_dir(allocator, L"LOCALAPPDATA", "AppData\\Local", out);
}

GCU_Path_Result gcu_path_cache_dir(const GCU_Allocator * allocator,
    char ** out) {
  if (!out) {
    return GCU_PATH_ERR_INVALID;
  }
  if (!allocator) {
    allocator = gcu_allocator_default();
  }
  return path_known_dir(allocator, L"LOCALAPPDATA", "AppData\\Local", out);
}

GCU_Path_Result gcu_path_temp_dir(const GCU_Allocator * allocator,
    char ** out) {
  if (!out) {
    return GCU_PATH_ERR_INVALID;
  }
  if (!allocator) {
    allocator = gcu_allocator_default();
  }
  DWORD needed = GetTempPathW(0, NULL);
  if (needed == 0) {
    return GCU_PATH_ERR_IO;
  }
  wchar_t * wide =
      (wchar_t *)gcu_allocator_malloc(allocator, (needed + 1)
          * sizeof(wchar_t));
  if (!wide) {
    return GCU_PATH_ERR_OOM;
  }
  if (GetTempPathW(needed + 1, wide) == 0) {
    gcu_allocator_free(allocator, wide);
    return GCU_PATH_ERR_IO;
  }
  char * utf8 = gcu_path_internal_from_wide(allocator, wide);
  gcu_allocator_free(allocator, wide);
  if (!utf8) {
    return GCU_PATH_ERR_OOM;
  }
  // GetTempPath always ends in a separator; the rest of this module does not.
  char * trimmed = path_dup_trimmed(allocator, utf8);
  gcu_allocator_free(allocator, utf8);
  if (!trimmed) {
    return GCU_PATH_ERR_OOM;
  }
  *out = trimmed;
  return GCU_PATH_OK;
}

GCU_Path_Result gcu_path_canonicalize(const char * path,
    const GCU_Allocator * allocator, char ** out) {
  if (!path || !out) {
    return GCU_PATH_ERR_INVALID;
  }
  if (!allocator) {
    allocator = gcu_allocator_default();
  }
  wchar_t * wide = gcu_path_internal_to_wide(allocator, path);
  if (!wide) {
    return GCU_PATH_ERR_OOM;
  }
  HANDLE handle = CreateFileW(wide, 0,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  gcu_allocator_free(allocator, wide);
  if (handle == INVALID_HANDLE_VALUE) {
    return GCU_PATH_ERR_IO;
  }
  DWORD needed = GetFinalPathNameByHandleW(handle, NULL, 0, FILE_NAME_NORMALIZED);
  if (needed == 0) {
    CloseHandle(handle);
    return GCU_PATH_ERR_IO;
  }
  wchar_t * resolved =
      (wchar_t *)gcu_allocator_malloc(allocator, (needed + 1)
          * sizeof(wchar_t));
  if (!resolved) {
    CloseHandle(handle);
    return GCU_PATH_ERR_OOM;
  }
  DWORD written = GetFinalPathNameByHandleW(handle, resolved, needed + 1,
      FILE_NAME_NORMALIZED);
  CloseHandle(handle);
  if (written == 0) {
    gcu_allocator_free(allocator, resolved);
    return GCU_PATH_ERR_IO;
  }
  char * utf8 = gcu_path_internal_from_wide(allocator, resolved);
  gcu_allocator_free(allocator, resolved);
  if (!utf8) {
    return GCU_PATH_ERR_OOM;
  }
  *out = utf8;
  return GCU_PATH_OK;
}

#else // _WIN32

GCU_Path_Result gcu_path_cwd(const GCU_Allocator * allocator, char ** out) {
  if (!out) {
    return GCU_PATH_ERR_INVALID;
  }
  if (!allocator) {
    allocator = gcu_allocator_default();
  }

  // getcwd() reports a buffer that is merely too small the same way it
  // reports real failures on some systems, so the size is grown until it
  // either succeeds or passes the point where a real path could live.
  size_t size = 256;
  for (;;) {
    char * buffer = (char *)gcu_allocator_malloc(allocator, size);
    if (!buffer) {
      return GCU_PATH_ERR_OOM;
    }
    if (getcwd(buffer, size)) {
      *out = buffer;
      return GCU_PATH_OK;
    }
    gcu_allocator_free(allocator, buffer);
    if (size >= GCU_PATH_ENV_MAX) {
      return GCU_PATH_ERR_IO;
    }
    size *= 2;
  }
}

GCU_Path_Result gcu_path_home(const GCU_Allocator * allocator, char ** out) {
  if (!out) {
    return GCU_PATH_ERR_INVALID;
  }
  if (!allocator) {
    allocator = gcu_allocator_default();
  }

  const char * home = path_env("HOME");
  if (home) {
    char * copy = path_dup_trimmed(allocator, home);
    if (!copy) {
      return GCU_PATH_ERR_OOM;
    }
    *out = copy;
    return GCU_PATH_OK;
  }

  // $HOME is absent in daemons, in containers and under some cron
  // implementations, and names the invoking user rather than the target one
  // under sudo.  The password database is the authority when it is missing.
  size_t size = 1024;
  long suggested = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (suggested > 0 && (size_t)suggested > size) {
    size = (size_t)suggested;
  }
  for (;;) {
    char * scratch = (char *)gcu_allocator_malloc(allocator, size);
    if (!scratch) {
      return GCU_PATH_ERR_OOM;
    }
    struct passwd record;
    struct passwd * found = NULL;
    int error = getpwuid_r(getuid(), &record, scratch, size, &found);
    if (error == 0 && found && found->pw_dir && found->pw_dir[0]) {
      char * copy = path_dup_trimmed(allocator, found->pw_dir);
      gcu_allocator_free(allocator, scratch);
      if (!copy) {
        return GCU_PATH_ERR_OOM;
      }
      *out = copy;
      return GCU_PATH_OK;
    }
    gcu_allocator_free(allocator, scratch);
    if (error != ERANGE || size >= GCU_PATH_ENV_MAX) {
      return GCU_PATH_ERR_IO;
    }
    size *= 2;
  }
}

/**
 * An XDG-style directory: the variable if it is set to an absolute path,
 * otherwise the home directory with a suffix appended.
 *
 * The specification says a relative value is to be ignored, and it is right
 * to: a relative cache directory would land wherever the process happened to
 * be started, which is not a location anything could find again.
 */
static GCU_Path_Result path_user_dir(const GCU_Allocator * allocator,
    const char * variable, const char * suffix, char ** out) {
  const char * value = variable ? path_env(variable) : NULL;
  if (value && gcu_path_is_absolute(GCU_PATH_NATIVE, value)) {
    char * copy = path_dup_trimmed(allocator, value);
    if (!copy) {
      return GCU_PATH_ERR_OOM;
    }
    *out = copy;
    return GCU_PATH_OK;
  }

  char * home = NULL;
  GCU_Path_Result result = gcu_path_home(allocator, &home);
  if (result != GCU_PATH_OK) {
    return result;
  }
  if (!suffix || !suffix[0]) {
    *out = home;
    return GCU_PATH_OK;
  }

  size_t need = 0;
  result = gcu_path_join(GCU_PATH_NATIVE, home, suffix, NULL, 0, &need);
  if (result != GCU_PATH_OK) {
    gcu_path_free(allocator, home);
    return result;
  }
  char * joined = (char *)gcu_allocator_malloc(allocator, need + 1);
  if (!joined) {
    gcu_path_free(allocator, home);
    return GCU_PATH_ERR_OOM;
  }
  result = gcu_path_join(GCU_PATH_NATIVE, home, suffix, joined, need + 1,
      NULL);
  gcu_path_free(allocator, home);
  if (result != GCU_PATH_OK) {
    gcu_allocator_free(allocator, joined);
    return result;
  }
  *out = joined;
  return GCU_PATH_OK;
}

GCU_Path_Result gcu_path_config_dir(const GCU_Allocator * allocator,
    char ** out) {
  if (!out) {
    return GCU_PATH_ERR_INVALID;
  }
  if (!allocator) {
    allocator = gcu_allocator_default();
  }
#ifdef __APPLE__
  return path_user_dir(allocator, NULL, "Library/Application Support", out);
#else
  return path_user_dir(allocator, "XDG_CONFIG_HOME", ".config", out);
#endif
}

GCU_Path_Result gcu_path_data_dir(const GCU_Allocator * allocator,
    char ** out) {
  if (!out) {
    return GCU_PATH_ERR_INVALID;
  }
  if (!allocator) {
    allocator = gcu_allocator_default();
  }
#ifdef __APPLE__
  return path_user_dir(allocator, NULL, "Library/Application Support", out);
#else
  return path_user_dir(allocator, "XDG_DATA_HOME", ".local/share", out);
#endif
}

GCU_Path_Result gcu_path_cache_dir(const GCU_Allocator * allocator,
    char ** out) {
  if (!out) {
    return GCU_PATH_ERR_INVALID;
  }
  if (!allocator) {
    allocator = gcu_allocator_default();
  }
#ifdef __APPLE__
  return path_user_dir(allocator, NULL, "Library/Caches", out);
#else
  return path_user_dir(allocator, "XDG_CACHE_HOME", ".cache", out);
#endif
}

GCU_Path_Result gcu_path_temp_dir(const GCU_Allocator * allocator,
    char ** out) {
  if (!out) {
    return GCU_PATH_ERR_INVALID;
  }
  if (!allocator) {
    allocator = gcu_allocator_default();
  }
  const char * value = path_env("TMPDIR");
  char * copy = path_dup_trimmed(allocator, value ? value : "/tmp");
  if (!copy) {
    return GCU_PATH_ERR_OOM;
  }
  *out = copy;
  return GCU_PATH_OK;
}

GCU_Path_Result gcu_path_canonicalize(const char * path,
    const GCU_Allocator * allocator, char ** out) {
  if (!path || !out) {
    return GCU_PATH_ERR_INVALID;
  }
  if (!allocator) {
    allocator = gcu_allocator_default();
  }
  // realpath(NULL) allocates with malloc(), which is not this library's
  // allocator, so the result is copied across and the original released.
  char * resolved = realpath(path, NULL);
  if (!resolved) {
    return GCU_PATH_ERR_IO;
  }
  char * copy = path_dup(allocator, resolved, strlen(resolved));
  free(resolved);
  if (!copy) {
    return GCU_PATH_ERR_OOM;
  }
  *out = copy;
  return GCU_PATH_OK;
}

#endif // _WIN32

GCU_Path_Result gcu_path_absolute(const char * path,
    const GCU_Allocator * allocator, char ** out) {
  if (!path || !out) {
    return GCU_PATH_ERR_INVALID;
  }
  if (!allocator) {
    allocator = gcu_allocator_default();
  }

  char * base = NULL;
  const char * joined_base = "";
  if (!gcu_path_is_absolute(GCU_PATH_NATIVE, path)) {
    GCU_Path_Result result = gcu_path_cwd(allocator, &base);
    if (result != GCU_PATH_OK) {
      return result;
    }
    joined_base = base;
  }

  size_t need = 0;
  GCU_Path_Result result =
      gcu_path_join(GCU_PATH_NATIVE, joined_base, path, NULL, 0, &need);
  if (result != GCU_PATH_OK) {
    gcu_path_free(allocator, base);
    return result;
  }
  char * joined = (char *)gcu_allocator_malloc(allocator, need + 1);
  if (!joined) {
    gcu_path_free(allocator, base);
    return GCU_PATH_ERR_OOM;
  }
  result = gcu_path_join(GCU_PATH_NATIVE, joined_base, path, joined, need + 1,
      NULL);
  gcu_path_free(allocator, base);
  if (result != GCU_PATH_OK) {
    gcu_allocator_free(allocator, joined);
    return result;
  }

  char * normalized = path_normalize_alloc(GCU_PATH_NATIVE, joined, allocator);
  gcu_allocator_free(allocator, joined);
  if (!normalized) {
    return GCU_PATH_ERR_OOM;
  }
  *out = normalized;
  return GCU_PATH_OK;
}

/** Fold an ASCII letter, and nothing else.  See GCU_PATH_MATCH_CASEFOLD. */
static char path_fold(char c, unsigned flags) {
  if ((flags & GCU_PATH_MATCH_CASEFOLD) && c >= 'A' && c <= 'Z') {
    return (char)(c - 'A' + 'a');
  }
  return c;
}

/**
 * Test one character against a `[...]` set, and say where the set ends.
 *
 * @param p The pattern, positioned at the `[`.
 * @param end Receives the index just past the closing `]`.
 * @return Whether @p c is in the set.  An unterminated set never matches, and
 *   @p end is left at the `[` so the caller can treat it as a literal.
 */
static bool path_match_set(GCU_Path_Flavor flavor, const char * p, char c,
    unsigned flags, size_t * end) {
  size_t i = 1;
  bool negated = false;
  if (p[i] == '!' || p[i] == '^') {
    negated = true;
    ++i;
  }
  // A ']' immediately after the opening (or after the negation) is a literal
  // member rather than the end of the set, which is how the shell has always
  // spelled "a set containing a closing bracket".
  bool found = false;
  bool first = true;
  for (; p[i] && (p[i] != ']' || first); ++i) {
    first = false;
    if (p[i + 1] == '-' && p[i + 2] && p[i + 2] != ']') {
      char lo = path_fold(p[i], flags);
      char hi = path_fold(p[i + 2], flags);
      char t = path_fold(c, flags);
      if (lo <= t && t <= hi) {
        found = true;
      }
      i += 2;
      continue;
    }
    if (path_fold(p[i], flags) == path_fold(c, flags)) {
      found = true;
    }
  }
  if (p[i] != ']') {
    // Unterminated. Refuse rather than guess: treating the rest of the
    // pattern as a set would make a typo match far more than it looks like.
    *end = 0;
    return false;
  }
  *end = i + 1;
  // A separator is never in a set, however the set is written, so that a
  // component pattern cannot escape its component through a character class.
  if (gcu_path_is_separator(flavor, c)) {
    return false;
  }
  return negated ? !found : found;
}

/** Match one pattern element against one character, without wildcards. */
static bool path_match_one(GCU_Path_Flavor flavor, const char * p, char c,
    unsigned flags, size_t * advance) {
  // Only where it is not already a separator. Under the Windows flavour a
  // backslash separates components, and one character cannot be both that and
  // the escape for the next one.
  if (*p == '\\' && p[1] && !gcu_path_is_separator(flavor, '\\')) {
    *advance = 2;
    return path_fold(p[1], flags) == path_fold(c, flags);
  }
  if (*p == '?') {
    *advance = 1;
    return (flags & GCU_PATH_MATCH_STAR_CROSSES)
        || !gcu_path_is_separator(flavor, c);
  }
  if (*p == '[') {
    size_t end = 0;
    bool in = path_match_set(flavor, p, c, flags, &end);
    if (end == 0) {
      *advance = 1;
      return path_fold('[', flags) == path_fold(c, flags);
    }
    *advance = end;
    return in;
  }
  *advance = 1;
  // Either separator matches either spelling under the Windows flavour, so a
  // pattern written with one kind of slash matches a path written with the
  // other.
  if (gcu_path_is_separator(flavor, *p) && gcu_path_is_separator(flavor, c)) {
    return true;
  }
  return path_fold(*p, flags) == path_fold(c, flags);
}

bool gcu_path_match(GCU_Path_Flavor flavor, const char * pattern,
    const char * path, unsigned flags) {
  if (!pattern || !path) {
    return false;
  }

  size_t i = 0; // Into path.
  size_t j = 0; // Into pattern.
  // Two backtrack points rather than one. The classic algorithm keeps only the
  // most recent star, which is enough when every star is equal; here a `*`
  // cannot cross a separator and a `**` can, so a `*` that runs out at a
  // separator has to be able to fall back to an earlier `**`.
  size_t star = (size_t)-1, star_mark = 0;
  size_t deep = (size_t)-1, deep_mark = 0;

  for (;;) {
    if (path[i]) {
      if (pattern[j] == '*') {
        bool crosses = (pattern[j + 1] == '*')
            || (flags & GCU_PATH_MATCH_STAR_CROSSES);
        j += (pattern[j + 1] == '*') ? 2 : 1;
        if (crosses) {
          deep = j;
          deep_mark = i;
          star = (size_t)-1;
        }
        else {
          star = j;
          star_mark = i;
        }
        continue;
      }
      size_t advance = 0;
      if (pattern[j]
          && path_match_one(flavor, pattern + j, path[i], flags, &advance)) {
        j += advance;
        ++i;
        continue;
      }
    }
    else if (pattern[j] == '*') {
      j += (pattern[j + 1] == '*') ? 2 : 1;
      continue;
    }
    else if (!pattern[j]) {
      return true;
    }

    // No progress. Give the most recent star one more character, if it is
    // allowed to have it.
    if (star != (size_t)-1 && path[star_mark]
        && !gcu_path_is_separator(flavor, path[star_mark])) {
      ++star_mark;
      i = star_mark;
      j = star;
      continue;
    }
    if (deep != (size_t)-1 && path[deep_mark]) {
      ++deep_mark;
      i = deep_mark;
      j = deep;
      star = (size_t)-1;
      continue;
    }
    return false;
  }
}
