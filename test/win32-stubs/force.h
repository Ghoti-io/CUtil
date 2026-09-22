/*
 * Force-included ahead of everything by the parse check.
 *
 * macros.h expands GCU_API to `__declspec(dllexport)` under _WIN32, which a
 * Linux gcc does not have.  Neutralising it here rather than editing macros.h
 * keeps the check out of the shipped headers.  See windows.h in this
 * directory for what the check is and what it is not.
 */
#ifndef GHOTI_IO_GCU_WIN32_STUBS_FORCE_H
#define GHOTI_IO_GCU_WIN32_STUBS_FORCE_H
#define __declspec(x)
#endif
