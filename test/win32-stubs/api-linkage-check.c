/*
 * Checks that GCU_API and GCU_API_DATA have three distinct states on Windows,
 * from a Linux compiler, at preprocessing time.
 *
 * Compiled three times with -fsyntax-only, once per state; which state is
 * being checked comes from the same -D a real consumer would pass, so the
 * expectation below is indexed by exactly the thing under test.
 *
 * ## Why this cannot reuse the sibling parse check
 *
 * parse-check.c force-includes win32-stubs/force.h, which defines
 * `__declspec(x)` away to nothing.  That is right for its job and fatal for
 * this one: with the decoration erased, all three states look identical, so
 * that check passes just as happily against a header with no static arm.
 *
 * ## Why a number rather than a string
 *
 * A Linux compiler has no `__declspec`, so something has to stand in for it.
 * Standing in with a *value* rather than with nothing is what makes the three
 * states tell themselves apart, and it keeps the whole check inside `#if`:
 * nothing is emitted, nothing is linked, and there is no binary to go stale
 * against a flag change.  Summing from zero is what turns the static state's
 * "expands to nothing" into a value `#if` can read -- an empty expansion is
 * not an expression, and `#if GCU_API` on it is an error rather than a zero.
 */

#define _WIN32 1

#define GHOTI_DECOR_dllexport +1
#define GHOTI_DECOR_dllimport +2
#define __declspec(x) GHOTI_DECOR_##x

/* GCU_API also carries GCU_EXTERN, which is empty in C, so after the
 * substitution above each macro is either nothing or one of the two terms. */
#define GHOTI_DECOR_OF(macro) (0 macro)

#define GHOTI_DECOR_NONE 0
#define GHOTI_DECOR_EXPORT 1
#define GHOTI_DECOR_IMPORT 2

#include <ghoti.io/cutil/macros.h>

#if defined(GHOTIIO_CUTIL_BUILD)
#define GHOTI_WANT GHOTI_DECOR_EXPORT
#elif defined(GHOTIIO_CUTIL_STATIC)
#define GHOTI_WANT GHOTI_DECOR_NONE
#else
#define GHOTI_WANT GHOTI_DECOR_IMPORT
#endif

#if GHOTI_DECOR_OF(GCU_API) != GHOTI_WANT
#error "GCU_API has the wrong Windows linkage for this state. A consumer linking libghoti.io-cutil-0.a must get neither dllexport nor dllimport: an archive has no __imp_ thunks. See GCU_API in include/ghoti.io/cutil/macros.h."
#endif

#if GHOTI_DECOR_OF(GCU_API_DATA) != GHOTI_WANT
#error "GCU_API_DATA has the wrong Windows linkage for this state. See GCU_API in include/ghoti.io/cutil/macros.h."
#endif

/* -pedantic-errors rejects a translation unit with no declaration in it. */
typedef int ghoti_io_gcu_api_linkage_checked;
