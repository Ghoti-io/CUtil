/**
 * @file
 * Identity of this build, and the machinery that stamps it onto a name.
 *
 * This file holds two things and nothing else: where the version token comes
 * from, and the macros that paste it onto an identifier.  The list of names to
 * stamp is in namespace.h; the compiler and platform attributes are in
 * macros.h.  See CONVENTIONS.md section 4.
 */

#ifndef GHOTI_IO_GCU_LIBVER_H
#define GHOTI_IO_GCU_LIBVER_H

/**
 * GHOTIIO_CUTIL_NAME and GHOTIIO_CUTIL_VERSION come from here.  They are
 * generated at build time from the Makefile's BRANCH, so that the token inside
 * every exported symbol is the same one that names the .pc file, the install
 * directory and the shared library.
 */
#include <ghoti.io/cutil/libver_gen.h>

/**
 * Macro to generate a "namespaced" version of an identifier.
 *
 * Notice, we cannot use GHOTIIO_CUTIL_CONCAT2(), because the preprocessor dies
 * in some cases with nested use (see vector.template.c).
 *
 * @param NAME The name which will be prepended with the `GHOTIIO_CUTIL_NAME`.
 */
#define GHOTIIO_CUTIL(NAME) GHOTIIO_CUTIL_RENAME(GHOTIIO_CUTIL_NAME, _ ## NAME)

/**
 * Helper macro to concatenate the `#define`s properly.  It requires two levels
 * of processing.
 *
 * This macro should only be called by the `GHOTIIO_CUTIL_RENAME()` macro.
 *
 * @param a The first part of the identifier.
 * @param b The second part of the identifier.
 * @returns The concatenation of `a` to `b`.
 */
#define GHOTIIO_CUTIL_RENAME_INNER(a,b) a ## b

/**
 * Helper macro to concatenate the `#define`s properly.  It requires two levels
 * of processing.
 *
 * @param a The first part of the identifier.
 * @param b The second part of the identifier.
 * @returns A call to the `GHOTIIO_CUTIL_RENAME_INNER()` macro.
 */
#define GHOTIIO_CUTIL_RENAME(a,b) GHOTIIO_CUTIL_RENAME_INNER(a,b)

/**
 * Helper macro to concatenate the identifiers.  It requires two levels of
 * processing.
 *
 * This macro should not be called directly.  It should only be called by
 * GHOTIIO_CUTIL_CONCAT2().
 *
 * @param a The first part of the identifier.
 * @param b The second part of the identifier.
 * @returns The concatenation of `a` to `b`.
 */
#define GHOTIIO_CUTIL_CONCAT2_INNER(a,b) a ## b

/**
 * Helper macro to concatenate the identifiers.  It requires two levels
 * of processing.
 *
 * This macro may be called directly.
 *
 * @param a The first part of the identifier.
 * @param b The second part of the identifier.
 * @returns A call to the `GHOTIIO_CUTIL_CONCAT2_INNER()` macro.
 */
#define GHOTIIO_CUTIL_CONCAT2(a,b) GHOTIIO_CUTIL_CONCAT2_INNER(a,b)

/**
 * Helper macro to concatenate the identifiers.  It requires two levels of
 * processing.
 *
 * This macro should not be called directly.  It should only be called by
 * GHOTIIO_CUTIL_CONCAT3().
 *
 * @param a The first part of the identifier.
 * @param b The second part of the identifier.
 * @param c The third part of the identifier.
 * @returns The concatenation of `a` to `b` to `c`.
 */
#define GHOTIIO_CUTIL_CONCAT3_INNER(a,b,c) a ## b ## c

/**
 * Helper macro to concatenate the identifiers.  It requires two levels
 * of processing.
 *
 * This macro may be called directly.
 *
 * @param a The first part of the identifier.
 * @param b The second part of the identifier.
 * @param c The third part of the identifier.
 * @returns A call to the `GHOTIIO_CUTIL_CONCAT3_INNER()` macro.
 */
#define GHOTIIO_CUTIL_CONCAT3(a,b,c) GHOTIIO_CUTIL_CONCAT3_INNER(a,b,c)

#endif // GHOTI_IO_GCU_LIBVER_H
