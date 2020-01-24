/**
 * @file km/shared.h
 *
 * @copyright 2019-2020 Bill Zissimopoulos
 */
/*
 * This file is part of WinFuse.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * Affero General Public License version 3 as published by the Free
 * Software Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the AGPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#ifndef KM_SHARED_H_INCLUDED
#define KM_SHARED_H_INCLUDED

#include <ntifs.h>

/* disable warnings */
#pragma warning(disable:4100)           /* unreferenced formal parameter */
#pragma warning(disable:4127)           /* conditional expression is constant */
#pragma warning(disable:4200)           /* zero-sized array in struct/union */
#pragma warning(disable:4201)           /* nameless struct/union */

/* debug */
#if DBG
ULONG DebugRandom(VOID);
BOOLEAN DebugMemory(PVOID Memory, SIZE_T Size, BOOLEAN Test);
#define DEBUGLOG(fmt, ...)              \
    DbgPrint("[%d] " DRIVER_NAME "!" __FUNCTION__ ": " fmt "\n", KeGetCurrentIrql(), __VA_ARGS__)
#define DEBUGTEST(Percent)              (DebugRandom() <= (Percent) * 0x7fff / 100)
#define DEBUGFILL(M, S)                 DebugMemory(M, S, FALSE)
#define DEBUGGOOD(M, S)                 DebugMemory(M, S, TRUE)
#else
#define DEBUGLOG(fmt, ...)              ((void)0)
#define DEBUGTEST(Percent)              (TRUE)
#define DEBUGFILL(M, S)                 (TRUE)
#define DEBUGGOOD(M, S)                 (TRUE)
#endif

#endif
