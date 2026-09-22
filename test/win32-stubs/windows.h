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
