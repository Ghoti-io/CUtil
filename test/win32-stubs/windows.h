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

/* --- appended for rwlock.h / rwlock.c --- */
#ifndef GHOTI_IO_GCU_WIN32_STUBS_RWLOCK
#define GHOTI_IO_GCU_WIN32_STUBS_RWLOCK
void AcquireSRWLockShared(SRWLOCK * SRWLock);
void ReleaseSRWLockShared(SRWLOCK * SRWLock);
unsigned char TryAcquireSRWLockShared(SRWLOCK * SRWLock);
#endif

/* --- appended for error.c --- */
#ifndef GHOTI_IO_GCU_WIN32_STUBS_ERROR
#define GHOTI_IO_GCU_WIN32_STUBS_ERROR
#define FORMAT_MESSAGE_FROM_SYSTEM     0x00001000UL
#define FORMAT_MESSAGE_IGNORE_INSERTS  0x00000200UL
typedef void * LPVOID;
typedef char * LPSTR;
DWORD FormatMessageA(DWORD dwFlags, const void * lpSource, DWORD dwMessageId,
    DWORD dwLanguageId, LPSTR lpBuffer, DWORD nSize, void * Arguments);
#endif

/* --- appended for tls.c --- */
#ifndef GHOTI_IO_GCU_WIN32_STUBS_TLS
#define GHOTI_IO_GCU_WIN32_STUBS_TLS
#define FLS_OUT_OF_INDEXES 0xFFFFFFFFUL
typedef void (* PFLS_CALLBACK_FUNCTION)(PVOID);
DWORD FlsAlloc(PFLS_CALLBACK_FUNCTION lpCallback);
BOOL  FlsFree(DWORD dwFlsIndex);
PVOID FlsGetValue(DWORD dwFlsIndex);
BOOL  FlsSetValue(DWORD dwFlsIndex, PVOID lpFlsData);
#endif

/* --- appended for env.c --- */
#ifndef GHOTI_IO_GCU_WIN32_STUBS_ENV
#define GHOTI_IO_GCU_WIN32_STUBS_ENV
#define ERROR_SUCCESS            0UL
#define ERROR_ENVVAR_NOT_FOUND 203UL
typedef unsigned short WCHAR;
typedef const WCHAR * LPCWSTR;
typedef WCHAR * LPWSTR;
void  SetLastError(DWORD dwErrCode);
DWORD GetEnvironmentVariableW(LPCWSTR lpName, LPWSTR lpBuffer, DWORD nSize);
BOOL  SetEnvironmentVariableW(LPCWSTR lpName, LPCWSTR lpValue);
#endif

/* --- appended for library.c --- */
#ifndef GHOTI_IO_GCU_WIN32_STUBS_LIBRARY
#define GHOTI_IO_GCU_WIN32_STUBS_LIBRARY
#define ERROR_INVALID_NAME 123UL
typedef struct _GCU_STUB_HINSTANCE * HMODULE;
typedef int (* FARPROC)(void);
HMODULE LoadLibraryW(LPCWSTR lpLibFileName);
BOOL    FreeLibrary(HMODULE hLibModule);
FARPROC GetProcAddress(HMODULE hModule, const char * lpProcName);
#endif

/* --- appended for filelock.c --- */
#ifndef GHOTI_IO_GCU_WIN32_STUBS_FILELOCK
#define GHOTI_IO_GCU_WIN32_STUBS_FILELOCK
#define GENERIC_READ              0x80000000UL
#define GENERIC_WRITE             0x40000000UL
#define FILE_SHARE_READ           0x00000001UL
#define FILE_SHARE_WRITE          0x00000002UL
#define OPEN_ALWAYS               4UL
#define FILE_ATTRIBUTE_NORMAL     0x80UL
#define LOCKFILE_FAIL_IMMEDIATELY 0x00000001UL
#define LOCKFILE_EXCLUSIVE_LOCK   0x00000002UL
#define ERROR_LOCK_VIOLATION      33UL
#define MAXDWORD                  0xFFFFFFFFUL
#define INVALID_HANDLE_VALUE      ((HANDLE)(long)-1)
#define ZeroMemory(d, l)          gcu_stub_zero((d), (l))
typedef void * HANDLE;
typedef struct _GCU_STUB_OVERLAPPED { unsigned long Internal; } OVERLAPPED;
void gcu_stub_zero(void * destination, unsigned long length);
HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess,
    DWORD dwShareMode, void * lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
BOOL CloseHandle(HANDLE hObject);
BOOL LockFileEx(HANDLE hFile, DWORD dwFlags, DWORD dwReserved,
    DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh,
    OVERLAPPED * lpOverlapped);
BOOL UnlockFileEx(HANDLE hFile, DWORD dwReserved,
    DWORD nNumberOfBytesToUnlockLow, DWORD nNumberOfBytesToUnlockHigh,
    OVERLAPPED * lpOverlapped);
#endif
