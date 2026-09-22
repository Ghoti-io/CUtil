/*
 * Parse-checks the `#ifdef _WIN32` branches of this library's headers on a
 * non-Windows compiler.  Every macro is *used*, not merely defined: an unused
 * macro is uninstantiated text and proves nothing.  See win32-stubs/windows.h.
 */

#define _WIN32 1

#include <ghoti.io/cutil/mutex.h>

int main(void) {
  GCU_MUTEX_T m;
  int rc = 0;
  rc |= GCU_MUTEX_CREATE(m);
  rc |= GCU_MUTEX_LOCK(m);
  rc |= GCU_MUTEX_UNLOCK(m);
  rc |= GCU_MUTEX_TRYLOCK(m);
  rc |= GCU_MUTEX_UNLOCK(m);
  rc |= GCU_MUTEX_DESTROY(m);
  return rc;
}
