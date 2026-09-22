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
 * Path manipulation, split into a purely lexical half and a half that asks
 * the operating system questions.
 *
 * The lexical half never touches the filesystem.  It takes a
 * ::GCU_Path_Flavor rather than reading `#ifdef _WIN32`, so Windows path
 * semantics - drive letters, UNC shares, the difference between rooted and
 * absolute - are ordinary functions that can be tested anywhere, including on
 * the Linux machine where this suite is actually verified.  That is the point
 * of the parameter: it converts a platform branch nobody can exercise into a
 * table of cases anybody can.
 *
 * The design and the reasoning behind each decision are recorded in
 * `documentation/path.md`.
 */

#ifndef GHOTI_IO_GCU_PATH_H
#define GHOTI_IO_GCU_PATH_H

#include <stdbool.h>
#include <stddef.h>
#include <ghoti.io/cutil/allocator.h>
#include <ghoti.io/cutil/macros.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Which set of path rules to apply.
 *
 * This is a parameter rather than a compile-time branch so that a test can
 * ask for Windows behaviour on a POSIX host.  Pass ::GCU_PATH_NATIVE unless
 * you are deliberately reasoning about the other platform's paths.
 */
typedef enum GCU_Path_Flavor {
  GCU_PATH_POSIX = 0, ///< `/` separates; a leading `/` is the only root.
  GCU_PATH_WINDOWS,   ///< `\` and `/` separate; drives and UNC shares exist.
} GCU_Path_Flavor;

/**
 * The flavour of the host this was compiled for.
 *
 * An object-like macro rather than a third enumerator, so that no function
 * has to map it onto a real flavour at runtime.
 */
#ifdef _WIN32
#define GCU_PATH_NATIVE GCU_PATH_WINDOWS
#else
#define GCU_PATH_NATIVE GCU_PATH_POSIX
#endif

/**
 * The outcome of a path operation.
 */
typedef enum GCU_Path_Result {
  GCU_PATH_OK = 0,          ///< Succeeded.
  GCU_PATH_ERR_INVALID,     ///< A caller-supplied argument is wrong.
  GCU_PATH_ERR_OOM,         ///< The allocator returned NULL.
  GCU_PATH_ERR_LIMIT,       ///< The caller's buffer is too small.
  GCU_PATH_ERR_IO,          ///< An operating-system query failed.
  GCU_PATH_ERR_UNSUPPORTED, ///< No answer exists; see the function.
  /**
   * One past the last code.  **Its value changes whenever this enum grows**,
   * which is the point of it - so it is safe to name (a `case` label keeping a
   * switch exhaustive, a bound in code compiled against this same header) and
   * unsafe to *keep*.  Never store it, transmit it, or size anything with it
   * that outlives the compilation: a `names[GCU_PATH_RESULT_COUNT]` sized
   * against
   * an older header is indexed past its end by a newer library.
   *
   * Never returned.
   */
  GCU_PATH_RESULT_COUNT,
} GCU_Path_Result;

/**
 * Name a result, for diagnostics.
 *
 * @param result The result to name.
 * @return A static string, owned by the library.  Never NULL: an
 *   out-of-range value yields `"unknown"`.
 */
GCU_API const char * gcu_path_result_string(GCU_Path_Result result);

/**
 * @name Buffer contract
 *
 * Every lexical function below writes into a caller-supplied buffer and
 * follows the same three rules.
 *
 * - Passing `out = NULL` with `out_size = 0` measures: the call returns
 *   ::GCU_PATH_OK and writes the length the result would have, excluding the
 *   terminator, to `out_len`.
 * - A buffer too small for the result *and* its terminator returns
 *   ::GCU_PATH_ERR_LIMIT and leaves the buffer untouched.  It never
 *   truncates: a truncated path that still looks like a path is worse than an
 *   error, because it names a different file.
 * - ::GCU_PATH_ERR_LIMIT is the one case where an output parameter is written
 *   on failure - `out_len` receives the required length, so that the caller
 *   can allocate and call again.  That is the whole point of the code, so it
 *   is a deliberate exception to the suite's rule that outputs are written
 *   only on success.  `out_len` may be NULL if the caller does not want it.
 *
 * The lexical half has no allocating variants on purpose.  Every result is
 * bounded by its inputs - a join is never longer than its two arguments plus
 * a separator - so the caller can always size a buffer up front.  The
 * environment half allocates, because the length of an answer from the
 * operating system cannot be known before asking.
 *
 * @{
 */

/**
 * The separator this flavour writes.
 *
 * @param flavor Which rules to apply.
 * @return `'/'` for ::GCU_PATH_POSIX, `'\\'` for ::GCU_PATH_WINDOWS.
 */
GCU_API char gcu_path_separator(GCU_Path_Flavor flavor);

/**
 * Whether a character separates components in this flavour.
 *
 * Windows accepts both `/` and `\` as separators in nearly every API, so both
 * are recognised on input even though only `\` is written on output.  POSIX
 * recognises only `/`, because `\` is a legal character in a POSIX filename
 * and treating it as a separator would corrupt real names.
 *
 * @param flavor Which rules to apply.
 * @param c The character to test.
 * @return `true` if @p c separates components.
 */
GCU_API bool gcu_path_is_separator(GCU_Path_Flavor flavor, char c);

/**
 * The length of the leading root, if any.
 *
 * The root is the prefix that component-wise operations must not walk above.
 * For ::GCU_PATH_POSIX that is a leading `/` and nothing else.  For
 * ::GCU_PATH_WINDOWS it is one of:
 *
 * | Form | Example | Root length | Absolute? |
 * | --- | --- | --- | --- |
 * | Drive, rooted | `C:\x` | 3 | yes |
 * | Drive, relative | `C:x` | 2 | **no** |
 * | Rooted, no drive | `\x` | 1 | **no** |
 * | UNC share | `\\srv\shr\x` | 10 | yes |
 * | Extended | `\\?\C:\x` | 7 | yes |
 *
 * A non-zero root length therefore does *not* mean the path is absolute;
 * `C:x` names a file relative to the current directory *of drive C*, and
 * `\x` names one relative to the current drive.  Use
 * ::gcu_path_is_absolute() for the question you probably mean.
 *
 * @param flavor Which rules to apply.
 * @param path The path to inspect.  NULL yields 0.
 * @return The number of leading bytes that form the root, or 0.
 */
GCU_API size_t gcu_path_root_length(GCU_Path_Flavor flavor, const char * path);

/**
 * Whether a path names a file without reference to any current directory.
 *
 * @param flavor Which rules to apply.
 * @param path The path to inspect.  NULL yields `false`.
 * @return `true` if @p path is fully qualified.
 */
GCU_API bool gcu_path_is_absolute(GCU_Path_Flavor flavor, const char * path);

/**
 * The final component of a path.
 *
 * Returns a pointer *into* @p path, so it allocates nothing and the result
 * lives exactly as long as @p path does.  Unlike POSIX `basename()` it does
 * not modify its argument, does not use a static buffer, and does not differ
 * between the GNU and POSIX spellings.
 *
 * The path is read exactly as given: a trailing separator means the final
 * component is empty, and `""` is what comes back.  Normalise first if that
 * is not wanted - ::gcu_path_normalize() removes trailing separators.
 *
 * @param flavor Which rules to apply.
 * @param path The path to inspect.
 * @return A pointer into @p path, or NULL if @p path is NULL.
 */
GCU_API const char * gcu_path_basename(
  GCU_Path_Flavor flavor, const char * path);

/**
 * The extension of the final component, including the dot.
 *
 * A leading dot does not begin an extension, so `.bashrc` has none.  Only the
 * last dot counts, so `archive.tar.gz` yields `".gz"`.  A dot in a directory
 * name is not an extension of the path: `dir.d/file` has none.
 *
 * @param flavor Which rules to apply.
 * @param path The path to inspect.
 * @return A pointer into @p path at the dot, or NULL if there is no
 *   extension or @p path is NULL.
 */
GCU_API const char * gcu_path_extension(
  GCU_Path_Flavor flavor, const char * path);

/**
 * Everything before the final component.
 *
 * Yields `"."` when the path has no directory part, and the root itself when
 * the final component sits directly in the root.
 *
 * @param flavor Which rules to apply.
 * @param path The path to inspect.
 * @param out Buffer to write into, or NULL to measure.
 * @param out_size Bytes available at @p out, including the terminator.
 * @param out_len Receives the length excluding the terminator.  May be NULL.
 * @return ::GCU_PATH_OK, ::GCU_PATH_ERR_INVALID or ::GCU_PATH_ERR_LIMIT.
 */
GCU_API GCU_Path_Result gcu_path_dirname(GCU_Path_Flavor flavor,
  const char * path, char * out, size_t out_size, size_t * out_len);

/**
 * Join two paths.
 *
 * If @p relative is absolute it replaces @p base entirely, which is the rule
 * every other path library uses and the one that makes configuration
 * overrides behave: a user who supplies an absolute path means it.
 *
 * Under ::GCU_PATH_WINDOWS a @p relative that is rooted but not absolute
 * (`\x`) keeps @p base's root and replaces everything after it, matching what
 * the operating system does with such a path.
 *
 * The result is not normalised; call ::gcu_path_normalize() if `.` and `..`
 * should be resolved.
 *
 * @param flavor Which rules to apply.
 * @param base The left-hand path.  NULL or `""` yields @p relative.
 * @param relative The right-hand path.  NULL or `""` yields @p base.
 * @param out Buffer to write into, or NULL to measure.
 * @param out_size Bytes available at @p out, including the terminator.
 * @param out_len Receives the length excluding the terminator.  May be NULL.
 * @return ::GCU_PATH_OK, ::GCU_PATH_ERR_INVALID or ::GCU_PATH_ERR_LIMIT.
 */
GCU_API GCU_Path_Result gcu_path_join(GCU_Path_Flavor flavor,
  const char * base, const char * relative, char * out, size_t out_size,
  size_t * out_len);

/**
 * Collapse `.`, `..`, and repeated separators, lexically.
 *
 * **This does not consult the filesystem, so it is wrong in the presence of
 * symbolic links.**  If `/a/b` is a link to `/c`, then `/a/b/..` is `/c/..`,
 * which is `/`, not the `/a` this function returns.  That is not a defect to
 * be fixed; it is the difference between asking what a path *says* and asking
 * what it *reaches*, and both questions have callers.  Use
 * ::gcu_path_canonicalize() for the second.
 *
 * Do not use this to decide whether a path escapes a sandbox.  A lexical
 * check passes paths that a symlink then carries outside; that comparison
 * must be made on canonicalised paths.
 *
 * `..` is removed along with the component before it, but never above the
 * root: `/..` is `/`.  A relative path has no such floor, so leading `..`
 * components survive - `a/../..` is `..`.  Trailing separators are dropped.
 * An empty result is reported as `"."`.  Under ::GCU_PATH_WINDOWS separators
 * are rewritten to `\`.
 *
 * @param flavor Which rules to apply.
 * @param path The path to normalise.
 * @param out Buffer to write into, or NULL to measure.
 * @param out_size Bytes available at @p out, including the terminator.
 * @param out_len Receives the length excluding the terminator.  May be NULL.
 * @return ::GCU_PATH_OK, ::GCU_PATH_ERR_INVALID or ::GCU_PATH_ERR_LIMIT.
 */
GCU_API GCU_Path_Result gcu_path_normalize(GCU_Path_Flavor flavor,
  const char * path, char * out, size_t out_size, size_t * out_len);

/**
 * Express @p to as a path relative to @p from.
 *
 * Both arguments are normalised first, and both must be rooted the same way:
 * two absolute paths, or two relative ones.  Under ::GCU_PATH_WINDOWS two
 * absolute paths on different drives have no relative path between them and
 * yield ::GCU_PATH_ERR_UNSUPPORTED - that is a fact about the filesystem, not
 * a limitation here.
 *
 * Component comparison is byte-exact under ::GCU_PATH_POSIX.  Under
 * ::GCU_PATH_WINDOWS it ignores case for ASCII letters only; full Unicode
 * case folding is not attempted, so two names differing outside ASCII are
 * treated as distinct even where the filesystem would not.
 *
 * Relative inputs that normalise to a leading `..` are refused with
 * ::GCU_PATH_ERR_UNSUPPORTED, because where they sit depends on a current
 * directory this function does not know.
 *
 * Like ::gcu_path_normalize(), this is lexical, and `..` in the result does
 * not survive a symbolic link.
 *
 * This is the one function here that takes an allocator, because it is the
 * one whose working set is not bounded by its output: it must hold both
 * normalised paths at once in order to compare them, and neither is the
 * answer.  The alternative was a fixed component limit, which is the
 * `MAX_PATH` mistake in a smaller costume.  The memory is released before the
 * call returns; nothing is handed back to free.
 *
 * @param flavor Which rules to apply.
 * @param from The directory the result is relative to.
 * @param to The path to express.
 * @param allocator Allocator for scratch space, or NULL for the default.
 * @param out Buffer to write into, or NULL to measure.
 * @param out_size Bytes available at @p out, including the terminator.
 * @param out_len Receives the length excluding the terminator.  May be NULL.
 * @return ::GCU_PATH_OK, ::GCU_PATH_ERR_INVALID, ::GCU_PATH_ERR_OOM,
 *   ::GCU_PATH_ERR_LIMIT or ::GCU_PATH_ERR_UNSUPPORTED.
 */
GCU_API GCU_Path_Result gcu_path_relative_to(GCU_Path_Flavor flavor,
  const char * from, const char * to, const GCU_Allocator * allocator,
  char * out, size_t out_size, size_t * out_len);

/**
 * Rewrite separators to the ones @p flavor writes.
 *
 * Under ::GCU_PATH_POSIX this copies the path unchanged, because `\` is a
 * legal character in a POSIX filename and rewriting it would rename the file
 * being described.
 *
 * Under ::GCU_PATH_WINDOWS, `/` becomes `\` - with one exception.  A path
 * beginning `\\?\` is passed to Win32 without any parsing, and a `/` inside
 * one is a literal character rather than a separator, so such a path is
 * copied unchanged.  Rewriting it would produce a path that names nothing.
 *
 * @param flavor Which rules to apply.
 * @param path The path to rewrite.
 * @param out Buffer to write into, or NULL to measure.
 * @param out_size Bytes available at @p out, including the terminator.
 * @param out_len Receives the length excluding the terminator.  May be NULL.
 * @return ::GCU_PATH_OK, ::GCU_PATH_ERR_INVALID or ::GCU_PATH_ERR_LIMIT.
 */
GCU_API GCU_Path_Result gcu_path_to_native(GCU_Path_Flavor flavor,
  const char * path, char * out, size_t out_size, size_t * out_len);

/**
 * Rewrite separators to `/`.
 *
 * Useful for emitting a path into a format that specifies forward slashes -
 * a URI, an archive member name, a makefile - regardless of where it was
 * read.  Under ::GCU_PATH_POSIX it copies unchanged, for the reason given on
 * ::gcu_path_to_native().  The `\\?\` exception applies here too.
 *
 * @param flavor Which rules to apply.
 * @param path The path to rewrite.
 * @param out Buffer to write into, or NULL to measure.
 * @param out_size Bytes available at @p out, including the terminator.
 * @param out_len Receives the length excluding the terminator.  May be NULL.
 * @return ::GCU_PATH_OK, ::GCU_PATH_ERR_INVALID or ::GCU_PATH_ERR_LIMIT.
 */
GCU_API GCU_Path_Result gcu_path_to_posix(GCU_Path_Flavor flavor,
  const char * path, char * out, size_t out_size, size_t * out_len);

/** @} */

/**
 * @name Matching
 * @{
 */

/**
 * How ::gcu_path_match() should read a pattern.
 */
typedef enum GCU_Path_Match_Flags {
  /** Ordinary shell-style matching. */
  GCU_PATH_MATCH_DEFAULT = 0,
  /**
   * Let `*` and `?` cross a separator.
   *
   * Off by default, because a pattern is nearly always meant to describe one
   * component:  a pattern of `src` then a separator then `*.c` should not
   * match `src/deep/nested/thing.c`.  Where crossing is wanted, spell that
   * star twice and leave this flag alone; it is for matching a string that
   * happens to contain separators rather than a path.
   */
  GCU_PATH_MATCH_STAR_CROSSES = 1u << 0,
  /**
   * Compare without regard to case, for ASCII letters only.
   *
   * Not applied automatically for ::GCU_PATH_WINDOWS.  Windows filesystems
   * fold case by a table that belongs to the volume and the version, and
   * folding UTF-8 correctly is a Unicode question rather than a path one.
   * What this flag does is documented exactly:  A-Z and a-z, nothing else.
   */
  GCU_PATH_MATCH_CASEFOLD = 1u << 1,
} GCU_Path_Match_Flags;

/**
 * Match a path against a shell-style pattern.
 *
 * Purely lexical:  nothing is opened, nothing is resolved, and a pattern is
 * never expanded into the set of files that exist.  This answers "does this
 * name match that pattern", which is the question a filter asks.  Walking a
 * directory and testing each entry is ::gcu_dir_read() plus this.
 *
 * The syntax is the usual one, and needs no regular-expression engine:
 *
 * - `?` matches one character, but never a separator.
 * - `*` matches any run of characters, but never a separator.
 * - `**` matches any run of characters including separators.
 * - `[abc]` matches one of those; `[a-z]` a range; `[!abc]` or `[^abc]`
 *   anything but.  A `]` first in the set is a literal `]`, and a separator
 *   never matches a set.
 * - `\` escapes the next character, so `\*` is a literal asterisk -
 *   under ::GCU_PATH_POSIX only.  Under ::GCU_PATH_WINDOWS a backslash
 *   separates components, and one character cannot be both that and the
 *   escape for the next; there is no escape character in that flavour.
 *
 * Matching runs in time proportional to the pattern times the path.  There is
 * no backtracking blow-up, which matters because patterns often come from
 * configuration files and sometimes from users.
 *
 * @param flavor Which separators count.
 * @param pattern The pattern.
 * @param path The string to test.
 * @param flags See ::GCU_Path_Match_Flags.  Zero is the usual behaviour.
 * @return true if it matches.  A NULL pattern or path is false.
 */
GCU_API bool gcu_path_match(GCU_Path_Flavor flavor, const char * pattern,
  const char * path, unsigned flags);

/** @} */

/**
 * @name Environment
 *
 * These ask the operating system, so they use the host's rules rather than
 * taking a flavour, and they allocate rather than measuring: the length of
 * an answer is not knowable before it arrives.
 *
 * Every one of them returns a NUL-terminated string owned by the caller,
 * allocated with the supplied allocator, and freed with ::gcu_path_free().
 * A NULL allocator means ::gcu_allocator_default().  On failure nothing is
 * allocated and `*out` is not written.
 *
 * There is deliberately no function to *change* the working directory.  A
 * process has one, shared by every thread, and a library that changes it
 * reaches into its host application and alters the meaning of every relative
 * path in the program - including ones belonging to code that has never
 * heard of this library.  Pass a base directory to whatever needs one
 * instead; that is why ::gcu_path_join() exists.
 *
 * @{
 */

/**
 * Release a string returned by any function in this section.
 *
 * @param allocator The allocator the string came from, or NULL for the
 *   default.  It must be the same one.
 * @param path The string to free.  NULL is accepted and ignored.
 */
GCU_API void gcu_path_free(const GCU_Allocator * allocator, char * path);

/**
 * The current working directory, as an absolute path.
 *
 * @param allocator Allocator for the result, or NULL for the default.
 * @param out Receives the allocated path.  Written only on success.
 * @return ::GCU_PATH_OK, ::GCU_PATH_ERR_INVALID, ::GCU_PATH_ERR_OOM or
 *   ::GCU_PATH_ERR_IO.
 */
GCU_API GCU_Path_Result gcu_path_cwd(
  const GCU_Allocator * allocator, char ** out);

/**
 * The invoking user's home directory.
 *
 * `$HOME` is consulted first and the password database second, because a
 * user who has set `$HOME` means it.  The fallback matters: `$HOME` is
 * routinely absent in daemons, containers and cron jobs, and points at the
 * wrong user under `sudo`.
 *
 * This is rarely the function you want.  Code that appends `/.myapp` to it is
 * correct on Linux and wrong on both Windows and macOS.  Ask for the
 * directory that suits the purpose - ::gcu_path_config_dir(),
 * ::gcu_path_data_dir(), ::gcu_path_cache_dir() - and let it be right on
 * each platform.
 *
 * @param allocator Allocator for the result, or NULL for the default.
 * @param out Receives the allocated path.  Written only on success.
 * @return ::GCU_PATH_OK, ::GCU_PATH_ERR_INVALID, ::GCU_PATH_ERR_OOM or
 *   ::GCU_PATH_ERR_IO when no home directory can be determined.
 */
GCU_API GCU_Path_Result gcu_path_home(
  const GCU_Allocator * allocator, char ** out);

/**
 * Where this user's configuration belongs.
 *
 * `$XDG_CONFIG_HOME`, else `~/.config` on Linux and the BSDs;
 * `~/Library/Application Support` on macOS; `%APPDATA%` on Windows.
 *
 * The directory is not created, and may not exist.
 *
 * @param allocator Allocator for the result, or NULL for the default.
 * @param out Receives the allocated path.  Written only on success.
 * @return ::GCU_PATH_OK, ::GCU_PATH_ERR_INVALID, ::GCU_PATH_ERR_OOM or
 *   ::GCU_PATH_ERR_IO.
 */
GCU_API GCU_Path_Result gcu_path_config_dir(
  const GCU_Allocator * allocator, char ** out);

/**
 * Where this user's persistent application data belongs.
 *
 * `$XDG_DATA_HOME`, else `~/.local/share` on Linux and the BSDs;
 * `~/Library/Application Support` on macOS; `%LOCALAPPDATA%` on Windows.
 *
 * The directory is not created, and may not exist.
 *
 * @param allocator Allocator for the result, or NULL for the default.
 * @param out Receives the allocated path.  Written only on success.
 * @return ::GCU_PATH_OK, ::GCU_PATH_ERR_INVALID, ::GCU_PATH_ERR_OOM or
 *   ::GCU_PATH_ERR_IO.
 */
GCU_API GCU_Path_Result gcu_path_data_dir(
  const GCU_Allocator * allocator, char ** out);

/**
 * Where this user's discardable cached data belongs.
 *
 * `$XDG_CACHE_HOME`, else `~/.cache` on Linux and the BSDs;
 * `~/Library/Caches` on macOS; `%LOCALAPPDATA%` on Windows.
 *
 * The directory is not created, and may not exist.  Anything written here
 * must be treated as removable at any time by something else.
 *
 * @param allocator Allocator for the result, or NULL for the default.
 * @param out Receives the allocated path.  Written only on success.
 * @return ::GCU_PATH_OK, ::GCU_PATH_ERR_INVALID, ::GCU_PATH_ERR_OOM or
 *   ::GCU_PATH_ERR_IO.
 */
GCU_API GCU_Path_Result gcu_path_cache_dir(
  const GCU_Allocator * allocator, char ** out);

/**
 * The directory for temporary files.
 *
 * `$TMPDIR`, else `/tmp`, on POSIX; `GetTempPath` on Windows, which consults
 * `%TMP%`, `%TEMP%`, `%USERPROFILE%` and the Windows directory in that order.
 *
 * Any trailing separator is removed, so the result joins cleanly.
 *
 * @param allocator Allocator for the result, or NULL for the default.
 * @param out Receives the allocated path.  Written only on success.
 * @return ::GCU_PATH_OK, ::GCU_PATH_ERR_INVALID, ::GCU_PATH_ERR_OOM or
 *   ::GCU_PATH_ERR_IO.
 */
GCU_API GCU_Path_Result gcu_path_temp_dir(
  const GCU_Allocator * allocator, char ** out);

/**
 * Make a path absolute, lexically.
 *
 * A relative path is joined onto the current working directory and the result
 * normalised.  An already-absolute path is normalised and returned.
 *
 * Nothing is opened, nothing has to exist, and **symbolic links are not
 * resolved**.  This answers "what does this path mean from here", which is
 * a different question from "what does this path reach" - see
 * ::gcu_path_canonicalize(), and the warning on ::gcu_path_normalize() about
 * which of the two a security check needs.
 *
 * @param path The path to resolve.
 * @param allocator Allocator for the result, or NULL for the default.
 * @param out Receives the allocated path.  Written only on success.
 * @return ::GCU_PATH_OK, ::GCU_PATH_ERR_INVALID, ::GCU_PATH_ERR_OOM or
 *   ::GCU_PATH_ERR_IO.
 */
GCU_API GCU_Path_Result gcu_path_absolute(const char * path,
  const GCU_Allocator * allocator, char ** out);

/**
 * Resolve a path to its real location, consulting the filesystem.
 *
 * Symbolic links are followed and the result is the canonical path of the
 * file itself.  **The path must exist**; this is the price of an answer the
 * filesystem has agreed to.  ::gcu_path_absolute() is the lexical
 * counterpart, and works on paths that do not exist yet.
 *
 * This is the function a containment check needs.  Canonicalise both the
 * sandbox root and the candidate, then compare; a comparison made on
 * un-canonicalised paths accepts anything a symbolic link points out of the
 * sandbox.
 *
 * @param path The path to resolve.
 * @param allocator Allocator for the result, or NULL for the default.
 * @param out Receives the allocated path.  Written only on success.
 * @return ::GCU_PATH_OK, ::GCU_PATH_ERR_INVALID, ::GCU_PATH_ERR_OOM, or
 *   ::GCU_PATH_ERR_IO when the path does not exist or cannot be resolved.
 */
GCU_API GCU_Path_Result gcu_path_canonicalize(const char * path,
  const GCU_Allocator * allocator, char ** out);

/** @} */

#ifdef __cplusplus
}
#endif

#endif // GHOTI_IO_GCU_PATH_H
