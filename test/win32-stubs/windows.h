/*
 * Minimal stand-ins for the Win32 declarations this library's headers use.
 *
 * NOT a Windows SDK and not used by any Windows build.  Its only job is to
 * let a Linux compiler *parse* the `#ifdef _WIN32` branches, which are
 * otherwise never tokenised here -- a syntax error in one of them survives
 * indefinitely, which is exactly how GCU_MAYBE_UNUSED's Windows branch stayed
 * a syntax error from the initial commit until 2026-09-22.
 *
 * This catches the syntax-error class and nothing else.  It does not check
 * that the real API has these signatures, and it cannot check semantics.  A
 * stub whose signature drifts from the SDK's would parse and still be wrong,
 * so keep these declarations copied from the documented prototypes.
 */

#ifndef GHOTI_IO_GCU_WIN32_STUBS_WINDOWS_H
#define GHOTI_IO_GCU_WIN32_STUBS_WINDOWS_H

#ifndef _WIN32
#error "win32-stubs/windows.h included without _WIN32; this is a parse check only"
#endif

typedef struct _GCU_STUB_SRWLOCK { void * Ptr; } SRWLOCK;

void InitializeSRWLock(SRWLOCK * SRWLock);
void AcquireSRWLockExclusive(SRWLOCK * SRWLock);
void ReleaseSRWLockExclusive(SRWLOCK * SRWLock);
unsigned char TryAcquireSRWLockExclusive(SRWLOCK * SRWLock);

#endif // GHOTI_IO_GCU_WIN32_STUBS_WINDOWS_H

/* --- appended for cond.h / cond.c --- */
#ifndef GHOTI_IO_GCU_WIN32_STUBS_COND
#define GHOTI_IO_GCU_WIN32_STUBS_COND

typedef unsigned long DWORD;
typedef int BOOL;

#define TRUE  1
#define FALSE 0

#define INFINITE      0xFFFFFFFFUL
#define ERROR_TIMEOUT 1460UL

DWORD GetLastError(void);

typedef struct _GCU_STUB_CONDITION_VARIABLE { void * Ptr; } CONDITION_VARIABLE;

void InitializeConditionVariable(CONDITION_VARIABLE * ConditionVariable);
void WakeConditionVariable(CONDITION_VARIABLE * ConditionVariable);
void WakeAllConditionVariable(CONDITION_VARIABLE * ConditionVariable);
BOOL SleepConditionVariableSRW(CONDITION_VARIABLE * ConditionVariable,
    SRWLOCK * SRWLock, DWORD dwMilliseconds, unsigned long Flags);

#endif

/* --- appended for once.h / once.c --- */
#ifndef GHOTI_IO_GCU_WIN32_STUBS_ONCE
#define GHOTI_IO_GCU_WIN32_STUBS_ONCE

#define CALLBACK
#define INIT_ONCE_STATIC_INIT { 0 }

typedef void * PVOID;
typedef struct _GCU_STUB_INIT_ONCE { void * Ptr; } INIT_ONCE, * PINIT_ONCE;

typedef BOOL (CALLBACK * PINIT_ONCE_FN)(PINIT_ONCE, PVOID, PVOID *);

BOOL InitOnceExecuteOnce(PINIT_ONCE InitOnce, PINIT_ONCE_FN InitFn,
    PVOID Parameter, PVOID * Context);

#endif
