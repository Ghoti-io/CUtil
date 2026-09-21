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
 * Compiler and platform attributes for this library.
 *
 * Every header and every .c file in this library includes this file first.
 * It pulls in namespace.h (and through it libver.h), so one include supplies
 * both the version namespace and the attributes below.  See CONVENTIONS.md
 * section 4.
 */

#ifndef GHOTI_IO_GCU_MACROS_H
#define GHOTI_IO_GCU_MACROS_H

// First, so that every name below and in any header that includes this file is
// already renamed into the version namespace.
#include <ghoti.io/cutil/namespace.h>

#include <wchar.h>

//-----------------------------------------------------------------------------
// Visibility
//-----------------------------------------------------------------------------

#ifdef __cplusplus
#define GCU_EXTERN extern "C"
#else
#define GCU_EXTERN
#endif

/**
 * Marks a declaration as part of the public API.
 *
 * The library is built with -fvisibility=hidden, so a symbol without this is
 * not exported at all: it cannot collide with another version of this library,
 * and it does not appear in the dynamic symbol table.  See CONVENTIONS.md
 * section 4.
 */
#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef GHOTIIO_CUTIL_BUILD
#define GCU_API GCU_EXTERN __declspec(dllexport)
#else
#define GCU_API GCU_EXTERN __declspec(dllimport)
#endif
#else
#define GCU_API GCU_EXTERN __attribute__((visibility("default")))
#endif

/**
 * Marks an exported *variable* as part of the public API.
 *
 * Same visibility as GCU_API, but without the `extern "C"`.  A variable
 * declaration cannot carry a redundant linkage specification - `extern "C"
 * extern size_t x;` is ill-formed in C++ - so a declaration that needs both
 * `extern` and export uses this and sits inside the header's `extern "C"`
 * block like every other declaration.  See CONVENTIONS.md section 4.
 */
#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef GHOTIIO_CUTIL_BUILD
#define GCU_API_DATA __declspec(dllexport)
#else
#define GCU_API_DATA __declspec(dllimport)
#endif
#else
#define GCU_API_DATA __attribute__((visibility("default")))
#endif

/**
 * Marks an internal declaration that the test suite needs to reach.
 *
 * Exported only when GHOTIIO_CUTIL_TEST_BUILD is defined, which the Makefile
 * does for the test build and not for the installed library.  A production
 * consumer therefore cannot see or link against these.
 */
#ifdef GHOTIIO_CUTIL_TEST_BUILD
#define GCU_INTERNAL_API GCU_API
#else
#define GCU_INTERNAL_API
#endif

//-----------------------------------------------------------------------------
// Declaration attributes
//-----------------------------------------------------------------------------

/**
 * A cross-compiler macro for marking a function parameter as unused.
 */
#if defined(__GNUC__) || defined(__clang__)
#define GCU_MAYBE_UNUSED(X) __attribute__((unused)) X
#elif defined(_MSC_VER)
#define GCU_MAYBE_UNUSED(X) (void)(X)
#else
#define GCU_MAYBE_UNUSED(X) X
#endif

/**
 * A cross-compiler macro for marking a function as deprecated.
 */
#if defined(__GNUC__) || defined(__clang__)
#define GCU_DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#define GCU_DEPRECATED __declspec(deprecated)
#else
#define GCU_DEPRECATED
#endif

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

// Helper macros for signed and unsigned max values.  They are #undef'd below:
// they are only needed for the comparisons that follow, and leaving names this
// generic defined would push them onto every consumer of this library.
#define GHOTI_IO_GCU_MAX_UINT64 0xFFFFFFFFFFFFFFFF
#define GHOTI_IO_GCU_MAX_UINT32 0xFFFFFFFF
#define GHOTI_IO_GCU_MAX_UINT16 0xFFFF

#define GHOTI_IO_GCU_MAX_INT64  0x7FFFFFFFFFFFFFFF
#define GHOTI_IO_GCU_MAX_INT32  0x7FFFFFFF
#define GHOTI_IO_GCU_MAX_INT16  0x7FFF

#if (WCHAR_MAX == GHOTI_IO_GCU_MAX_UINT64) || (WCHAR_MAX == GHOTI_IO_GCU_MAX_INT64)
// 64-bit signed
#define GCU_WCHAR_WIDTH 8
#define GCU_WCHAR_SIGNED (WCHAR_MAX == GHOTI_IO_GCU_MAX_INT64)

#elif (WCHAR_MAX == GHOTI_IO_GCU_MAX_UINT32) || (WCHAR_MAX == GHOTI_IO_GCU_MAX_INT32)
// 32-bit signed
#define GCU_WCHAR_WIDTH 4
#define GCU_WCHAR_SIGNED (WCHAR_MAX == GHOTI_IO_GCU_MAX_INT32)

#elif (WCHAR_MAX == GHOTI_IO_GCU_MAX_UINT16) || (WCHAR_MAX == GHOTI_IO_GCU_MAX_INT16)
// 16-bit signed
#define GCU_WCHAR_WIDTH 2
#define GCU_WCHAR_SIGNED (WCHAR_MAX == GHOTI_IO_GCU_MAX_INT16)

#else
#error "Could not determine GCU_WCHAR_WIDTH and GCU_WCHAR_SIGNED"

#endif // GCU_WCHAR_WIDTH and GCU_WCHAR_SIGNED

#undef GHOTI_IO_GCU_MAX_UINT64
#undef GHOTI_IO_GCU_MAX_UINT32
#undef GHOTI_IO_GCU_MAX_UINT16
#undef GHOTI_IO_GCU_MAX_INT64
#undef GHOTI_IO_GCU_MAX_INT32
#undef GHOTI_IO_GCU_MAX_INT16

//-----------------------------------------------------------------------------
// Macros for declaring functions to be run before/after main.
//-----------------------------------------------------------------------------
#ifdef _MSC_VER  // If using Visual Studio

#include <windows.h>

// Define the startup macro for Visual Studio
#define GCU_INIT_FUNCTION(function_name) \
    __pragma(section(".CRT$XCU", read)) \
    __declspec(allocate(".CRT$XCU")) void (*function_name##_init)(void) = function_name; \
    static void function_name(void)

// Define the cleanup macro for Visual Studio
#define GCU_CLEANUP_FUNCTION(function_name) \
    __pragma(section(".CRT$XTU", read)) \
    __declspec(allocate(".CRT$XTU")) void (*function_name##_cleanup)(void) = function_name; \
    static void function_name(void)

#elif defined(__GNUC__) || defined(__clang__) || defined(__MINGW32__) || defined(__MINGW64__)  // If using GCC/Clang/MinGW

// Define the startup macro for GCC/Clang/MinGW
#define GCU_INIT_FUNCTION(function_name) \
    __attribute__((constructor)) static void function_name(void)

// Define the cleanup macro for GCC/Clang/MinGW
#define GCU_CLEANUP_FUNCTION(function_name) \
    __attribute__((destructor)) static void function_name(void)

#else  // Other compilers (add more cases as needed)

#error "Unsupported compiler"

#endif

#endif // GHOTI_IO_GCU_MACROS_H
