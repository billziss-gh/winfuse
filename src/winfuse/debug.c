/**
 * @file winfuse/debug.c
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

#include <winfuse/driver.h>

#if DBG
ULONG DebugRandom(VOID)
{
    static KSPIN_LOCK SpinLock = 0;
    static ULONG Seed = 1;
    KIRQL Irql;
    ULONG Result;

    KeAcquireSpinLock(&SpinLock, &Irql);

    /* see ucrt sources */
    Seed = Seed * 214013 + 2531011;
    Result = (Seed >> 16) & 0x7fff;

    KeReleaseSpinLock(&SpinLock, Irql);

    return Result;
}

BOOLEAN DebugMemory(PVOID Memory, SIZE_T Size, BOOLEAN Test)
{
    BOOLEAN Result = TRUE;
    if (!Test)
    {
        for (SIZE_T I = 0, N = Size / sizeof(UINT32); N > I; I++)
        {
            ((PUINT8)Memory)[0] = 0x0B;
            ((PUINT8)Memory)[1] = 0xAD;
            ((PUINT8)Memory)[2] = 0xF0;
            ((PUINT8)Memory)[3] = 0x0D;
            Memory = (PUINT8)Memory + sizeof(UINT32);
        }
    }
    else
    {
        for (SIZE_T I = 0, N = Size / sizeof(UINT32); N > I; I++)
        {
            if (
                ((PUINT8)Memory)[0] == 0x0B &&
                ((PUINT8)Memory)[1] == 0xAD &&
                ((PUINT8)Memory)[2] == 0xF0 &&
                ((PUINT8)Memory)[3] == 0x0D
                )
            {
                Result = FALSE;
                break;
            }
            Memory = (PUINT8)Memory + sizeof(UINT32);
        }
    }
    return Result;
}
#endif
