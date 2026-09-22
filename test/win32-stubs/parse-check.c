/*
 * Parse-checks the `#ifdef _WIN32` branches of this library's headers on a
 * non-Windows compiler.  Every macro is *used*, not merely defined: an unused
 * macro is uninstantiated text and proves nothing.  See win32-stubs/windows.h.
 */

#define _WIN32 1

#include <ghoti.io/cutil/mutex.h>
#include <ghoti.io/cutil/cond.h>
#include <ghoti.io/cutil/once.h>
#include <ghoti.io/cutil/rwlock.h>
#include <ghoti.io/cutil/error.h>
#include <ghoti.io/cutil/tls.h>
#include <ghoti.io/cutil/env.h>

int main(void) {
  GCU_MUTEX_T m;
  int rc = 0;
  rc |= GCU_MUTEX_CREATE(m);
  rc |= GCU_MUTEX_LOCK(m);
  rc |= GCU_MUTEX_UNLOCK(m);
  rc |= GCU_MUTEX_TRYLOCK(m);
  rc |= GCU_MUTEX_UNLOCK(m);
  rc |= GCU_MUTEX_DESTROY(m);

  // cond.h's Windows branch: the typedef and the declarations.  The bodies
  // live in src/cond.c, which the check-win32-parse rule compiles separately
  // with -fsyntax-only against these same stubs.
  GCU_Cond cv;
  rc |= gcu_cond_create(&cv);
  rc |= gcu_cond_signal(&cv);
  rc |= gcu_cond_broadcast(&cv);
  rc |= gcu_cond_wait(&cv, &m);
  rc |= gcu_cond_timedwait(&cv, &m, 10);
  rc |= gcu_cond_destroy(&cv);

  static GCU_Once once = GCU_ONCE_INIT;
  rc |= gcu_once(&once, 0);

  GCU_RWLock rw;
  rc |= gcu_rwlock_create(&rw);
  rc |= gcu_rwlock_read_lock(&rw);
  rc |= gcu_rwlock_read_trylock(&rw);
  rc |= gcu_rwlock_read_unlock(&rw);
  rc |= gcu_rwlock_write_lock(&rw);
  rc |= gcu_rwlock_write_trylock(&rw);
  rc |= gcu_rwlock_write_unlock(&rw);
  rc |= gcu_rwlock_destroy(&rw);

  char errbuf[GCU_ERROR_STRING_MAX];
  rc |= gcu_error_last();
  rc |= gcu_error_string(2, errbuf, sizeof(errbuf));
  rc |= gcu_error_string_last(errbuf, sizeof(errbuf));

  GCU_TLS tls;
  rc |= gcu_tls_create(&tls, 0);
  rc |= gcu_tls_set(tls, 0);
  rc |= gcu_tls_get(tls) ? 1 : 0;
  rc |= gcu_tls_destroy(&tls);

  char envbuf[64];
  rc |= (int)gcu_env_get("PATH", envbuf, sizeof(envbuf));
  rc |= gcu_env_has("PATH") ? 1 : 0;
  rc |= gcu_env_set("GHOTI_X", "1");
  rc |= gcu_env_unset("GHOTI_X");

  return rc;
}
