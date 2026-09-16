/**
 * @file
 * test-safemath.cpp compiled against the portable body of every function.
 *
 * safemath.h has two implementations of each operation - the compiler's
 * overflow builtins, and a hand-written check for compilers without them -
 * and selects between them at compile time. On GCC and Clang it always
 * selects the builtins, which left the hand-written half, the half an MSVC
 * build compiles, executed by nothing.
 *
 * Defining GCU_SAFEMATH_NO_BUILTINS before the include selects the other half.
 * Including the test source rather than copying its cases is deliberate: a
 * copy is a copy that stops matching, and the whole point is that both bodies
 * face the same assertions.
 */

#define GCU_SAFEMATH_NO_BUILTINS 1

#include "test-safemath.cpp"
