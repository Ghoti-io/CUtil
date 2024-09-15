/**
 * @file
 * Header file used to control the version numbering and function namespace
 * for all of the library.
 */

#ifndef GHOTIIO_CUTIL_LIBVER_H
#define GHOTIIO_CUTIL_LIBVER_H

#include <wchar.h>

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
 * This macro should only be called by the `GHOTIIO_CUTIL_CONCAT()` macro.
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
 * Helper macro to concatenate the identifiers.  It reuires two levels of
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
 * Helper macro to concatenate the identifiers.  It reuires two levels of
 * processing.
 *
 * This macro should not be called directly.  It should only be called by
 * GHOTIIO_CUTIL_CONCAT2().
 *
 * @param a The first part of the identifier.
 * @param b The second part of the identifier.
 * @param c The third part of the identifier.
 * @returns The concatenation of `a` to `b` to `c`..
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

//-----------------------------------------------------------------------------
// Compiler Definitions
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Type-Related Definitions
//-----------------------------------------------------------------------------

#if DOXYGEN
/**
 * Indicate the size of the `wchar` type.
 */
#define GCU_WCHAR_WIDTH

/**
 * Indicate whether the `wchar` type is signed in this implementation.
 */
#define GCU_WCHAR_SIGNED
#endif // DOXYGEN

// Define helper macros for signed and unsigned max values
#define MAX_UINT64 0xFFFFFFFFFFFFFFFF
#define MAX_UINT32 0xFFFFFFFF
#define MAX_UINT16 0xFFFF

#define MAX_INT64  0x7FFFFFFFFFFFFFFF
#define MAX_INT32  0x7FFFFFFF
#define MAX_INT16  0x7FFF


#if (WCHAR_MAX == MAX_UINT64) || (WCHAR_MAX == MAX_INT64)
// 64-bit signed
#define GCU_WCHAR_WIDTH 8
#define GCU_WCHAR_SIGNED (WCHAR_MAX == MAX_INT64)

#elif (WCHAR_MAX == MAX_UINT32) || (WCHAR_MAX == MAX_INT32)
// 32-bit signed
#define GCU_WCHAR_WIDTH 4
#define GCU_WCHAR_SIGNED (WCHAR_MAX == MAX_INT32)

#elif (WCHAR_MAX == MAX_UINT16) || (WCHAR_MAX == MAX_INT16)
// 16-bit signed
#define GCU_WCHAR_WIDTH 2
#define GCU_WCHAR_SIGNED (WCHAR_MAX == MAX_INT16)

#else
#error "Could not determine GCU_WCHAR_WIDTH and GCU_WCHAR_SIGNED"

#endif // GCU_WCHAR_WIDTH and GCU_WCHAR_SIGNED


//-----------------------------------------------------------------------------
// Macros for declaring functions to be run before/after main.
//-----------------------------------------------------------------------------
#ifdef _MSC_VER  // If using Visual Studio

#include <windows.h>

// Define the startup macro for Visual Studio
#define GTA_INIT_FUNCTION(function_name) \
    __pragma(section(".CRT$XCU", read)) \
    __declspec(allocate(".CRT$XCU")) void (*function_name##_init)(void) = function_name; \
    static void function_name(void)

// Define the cleanup macro for Visual Studio
#define GTA_CLEANUP_FUNCTION(function_name) \
    __pragma(section(".CRT$XTU", read)) \
    __declspec(allocate(".CRT$XTU")) void (*function_name##_cleanup)(void) = function_name; \
    static void function_name(void)

#elif defined(__GNUC__) || defined(__clang__)  // If using GCC/Clang

// Define the startup macro for GCC/Clang
#define GTA_INIT_FUNCTION(function_name) \
    __attribute__((constructor)) static void function_name(void)

// Define the cleanup macro for GCC/Clang
#define GTA_CLEANUP_FUNCTION(function_name) \
    __attribute__((destructor)) static void function_name(void)

#else  // Other compilers (add more cases as needed)

// Default no-op macros for unsupported compilers
#define GTA_INIT_FUNCTION(function_name)
#define GTA_CLEANUP_FUNCTION(function_name)

#endif

//-----------------------------------------------------------------------------

#endif // GHOTIIO_CUTIL_LIBVER_H
