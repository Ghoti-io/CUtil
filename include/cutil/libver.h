/**
 * @file
 * Header file used to control the version numbering and function namespace
 * for all of the library.
 */

#ifndef GHOTIIO_CUTIL_LIBVER_H
#define GHOTIIO_CUTIL_LIBVER_H

/**
 * Used in conjunction with the GHOTIIO_CUTIL... macros to produce a namespaced
 * function name for use by all exported functions in this library.
 */
#define GHOTIIO_CUTIL_NAME ghotiio_cutil_dev

/**
 * String representation of the version, provided as a convenience to the
 * programmer.
 */
#define GHOTIIO_CUTIL_VERSION "dev"

/**
 * Macro to generate a "namespaced" version of an identifier.
 *
 * @param NAME The name which will be prepended with the `GHOTIIO_CUTIL_NAME`.
 */
#define GHOTIIO_CUTIL(NAME) GHOTIIO_CUTIL_CONCAT(GHOTIIO_CUTIL_NAME, _ ## NAME)

/**
 * Helper macro to concatenate the `#define`s properly.  It requires two levels
 * of processing.
 *
 * This macro should only be called by the `GHOTIIO_CUTIL_CONCAT()` macro.
 *
 * @param a The first part of the identifier.
 * @param b The second part of the identifier.
 * @returns The concatenation of `a` to `b`.
 */
#define GHOTIIO_CUTIL_CONCAT_INNER(a,b) a ## b

/**
 * Helper macro to concatenate the `#define`s properly.  It requires two levels
 * of processing.
 *
 * @param a The first part of the identifier.
 * @param b The second part of the identifier.
 * @returns A call to the `GHOTIIO_CUTIL_CONCAT_INNER()` macro.
 */
#define GHOTIIO_CUTIL_CONCAT(a,b) GHOTIIO_CUTIL_CONCAT_INNER(a,b)

#endif // GHOTIIO_CUTIL_LIBVER_H
 
