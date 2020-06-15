/**
 * @file shared/km/context.c
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

VOID FuseContextCreate(FUSE_CONTEXT **PContext,
    FUSE_INSTANCE *Instance, FSP_FSCTL_TRANSACT_REQ *InternalRequest);
VOID FuseContextDelete(FUSE_CONTEXT *Context);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FuseContextCreate)
#pragma alloc_text(PAGE, FuseContextDelete)
#endif

VOID FuseContextCreate(FUSE_CONTEXT **PContext,
    FUSE_INSTANCE *Instance, FSP_FSCTL_TRANSACT_REQ *InternalRequest)
{
    PAGED_CODE();

    FUSE_CONTEXT *Context;
    UINT32 Kind = 0 == InternalRequest ?
        FspFsctlTransactReservedKind : InternalRequest->Kind;

    ASSERT(FspFsctlTransactKindCount > Kind);
    if (0 == FuseOperations[Kind].Proc)
    {
        *PContext = FuseContextStatus(STATUS_INVALID_DEVICE_REQUEST);
        return;
    }

    Context = FuseAlloc(sizeof *Context);
    if (0 == Context)
    {
        *PContext = FuseContextStatus(STATUS_INSUFFICIENT_RESOURCES);
        return;
    }

    RtlZeroMemory(Context, sizeof *Context);
    Context->Instance = Instance;
    Context->InternalRequest = InternalRequest;
    Context->InternalResponse = (PVOID)&Context->InternalResponseBuf;
    Context->InternalResponse->Size = sizeof(FSP_FSCTL_TRANSACT_RSP);
    Context->InternalResponse->Kind = Kind;
    Context->InternalResponse->Hint = 0 != InternalRequest ? InternalRequest->Hint : 0;
    *PContext = Context;
}

VOID FuseContextDelete(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    if (FuseOpGuardTrue == Context->OpGuardResult)
    {
        UINT32 Kind = 0 == Context->InternalRequest ?
            FspFsctlTransactReservedKind : Context->InternalRequest->Kind;
        FuseOperations[Kind].Guard(Context, FALSE);
    }

    if (0 != Context->Fini)
        Context->Fini(Context);
    if (0 != Context->InternalRequest)
        FuseFree(Context->InternalRequest);
    if ((PVOID)&Context->InternalResponseBuf != Context->InternalResponse)
        FuseFree(Context->InternalResponse);

    DEBUGFILL(Context, sizeof *Context);
    FuseFree(Context);
}
