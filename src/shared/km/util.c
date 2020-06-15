/**
 * @file shared/km/util.c
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

#include <shared/km/shared.h>

PVOID FuseAllocatePoolMustSucceed(POOL_TYPE PoolType, SIZE_T Size, ULONG Tag);
NTSTATUS FuseSafeCopyMemory(PVOID Dst, PVOID Src, ULONG Len);
NTSTATUS FuseGetTokenUid(PACCESS_TOKEN Token, TOKEN_INFORMATION_CLASS InfoClass, PUINT32 PUid);

#ifdef ALLOC_PRAGMA
// !#pragma alloc_text(PAGE, FuseAllocatePoolMustSucceed)
#pragma alloc_text(PAGE, FuseSafeCopyMemory)
#pragma alloc_text(PAGE, FuseGetTokenUid)
#endif

static const LONG Delays[] =
{
     10/*ms*/ * -10000,
     10/*ms*/ * -10000,
     50/*ms*/ * -10000,
     50/*ms*/ * -10000,
    100/*ms*/ * -10000,
    100/*ms*/ * -10000,
    300/*ms*/ * -10000,
};

PVOID FuseAllocatePoolMustSucceed(POOL_TYPE PoolType, SIZE_T Size, ULONG Tag)
{
    // !PAGED_CODE();

    PVOID Result;
    LARGE_INTEGER Delay;

    for (ULONG i = 0, n = sizeof(Delays) / sizeof(Delays[0]);; i++)
    {
        Result = DEBUGTEST(99) ? ExAllocatePoolWithTag(PoolType, Size, Tag) : 0;
        if (0 != Result)
            return Result;

        Delay.QuadPart = n > i ? Delays[i] : Delays[n - 1];
        KeDelayExecutionThread(KernelMode, FALSE, &Delay);
    }
}

NTSTATUS FuseSafeCopyMemory(PVOID Dst, PVOID Src, ULONG Len)
{
    PAGED_CODE();

    try
    {
        RtlCopyMemory(Dst, Src, Len);
        return STATUS_SUCCESS;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        NTSTATUS Result = GetExceptionCode();
        return FsRtlIsNtstatusExpected(Result) ? STATUS_INVALID_USER_BUFFER : Result;
    }
}

NTSTATUS FuseGetTokenUid(PACCESS_TOKEN Token, TOKEN_INFORMATION_CLASS InfoClass, PUINT32 PUid)
{
    PAGED_CODE();

    NTSTATUS Result;
    PVOID Info = 0;
    PSID *PSid;

    Result = SeQueryInformationToken(Token, InfoClass, &Info);
    if (!NT_SUCCESS(Result))
        goto exit;

    switch (InfoClass)
    {
    case TokenUser:
        PSid = &((PTOKEN_USER)Info)->User.Sid;
        break;
    case TokenOwner:
        PSid = &((PTOKEN_OWNER)Info)->Owner;
        break;
    case TokenPrimaryGroup:
        PSid = &((PTOKEN_PRIMARY_GROUP)Info)->PrimaryGroup;
        break;
    default:
        ASSERT(0);
        Result = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    Result = FspPosixMapSidToUid(*PSid, PUid);

exit:
    if (0 != Info)
        FuseFreeExternal(Info);

    return Result;
}
