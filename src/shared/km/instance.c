/**
 * @file shared/km/instance.c
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

NTSTATUS FuseInstanceInit(FUSE_INSTANCE *Instance,
    FSP_FSCTL_VOLUME_PARAMS *VolumeParams,
    FUSE_INSTANCE_TYPE InstanceType);
VOID FuseInstanceFini(FUSE_INSTANCE *Instance);
VOID FuseInstanceExpirationRoutine(FUSE_INSTANCE *Instance, UINT64 ExpirationTime);
NTSTATUS FuseInstanceTransact(FUSE_INSTANCE *Instance,
    FUSE_PROTO_RSP *FuseResponse, ULONG InputBufferLength,
    FUSE_PROTO_REQ *FuseRequest, PULONG POutputBufferLength,
    PDEVICE_OBJECT DeviceObject, PFILE_OBJECT FileObject,
    PIRP CancellableIrp);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FuseInstanceInit)
#pragma alloc_text(PAGE, FuseInstanceFini)
#pragma alloc_text(PAGE, FuseInstanceExpirationRoutine)
#pragma alloc_text(PAGE, FuseInstanceTransact)
#endif

NTSTATUS FuseInstanceInit(FUSE_INSTANCE *Instance,
    FSP_FSCTL_VOLUME_PARAMS *VolumeParams,
    FUSE_INSTANCE_TYPE InstanceType)
{
    PAGED_CODE();

    NTSTATUS Result;

    RtlZeroMemory(Instance, sizeof *Instance);
    Instance->VolumeParams = VolumeParams;
    Instance->InstanceType = InstanceType;

    FuseRwlockInitialize(&Instance->OpGuardLock);

    Result = FuseIoqCreate(&Instance->Ioq);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = FuseCacheCreate(0, 0/*!VolumeParams->CaseSensitiveSearch*/, &Instance->Cache);
    if (!NT_SUCCESS(Result))
        goto exit;

    FuseFileInstanceInit(Instance);

    KeInitializeEvent(&Instance->InitEvent, NotificationEvent, FALSE);
    Result = FuseProtoPostInit(Instance);
    if (!NT_SUCCESS(Result))
        goto exit;

    /* ensure that VolumeParams can be used for FUSE operations */
    VolumeParams->CaseSensitiveSearch = 1; /* revisit FuseCacheCreate above if this changes */
    VolumeParams->CasePreservedNames = 1;
    VolumeParams->PersistentAcls = 1;
    VolumeParams->ReparsePoints = 1;
    VolumeParams->ReparsePointsAccessCheck = 0;
    VolumeParams->NamedStreams = 0;
    VolumeParams->ReadOnlyVolume = 0;
    VolumeParams->PostCleanupWhenModifiedOnly = 1;
    VolumeParams->PassQueryDirectoryFileName = 1;
    VolumeParams->DeviceControl = 1;
    VolumeParams->DirectoryMarkerAsNextOffset = 1;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
    {
        if (0 != Instance->Cache)
            FuseCacheDelete(Instance->Cache);

        if (0 != Instance->Ioq)
            FuseIoqDelete(Instance->Ioq);

        FuseRwlockFinalize(&Instance->OpGuardLock);

        RtlZeroMemory(Instance, sizeof *Instance);
    }

    return Result;
}

VOID FuseInstanceFini(FUSE_INSTANCE *Instance)
{
    PAGED_CODE();

    /*
     * The order of finalization is IMPORTANT:
     *
     * FuseIoqDelete must precede FuseFileInstanceFini, because the Ioq may contain Contexts
     * that hold File's.
     *
     * FuseIoqDelete must precede FuseCacheDelete, because the Ioq may contain Contexts
     * that hold CacheGen references.
     *
     * FuseFileInstanceFini must precede FuseCacheDelete, because some Files may hold
     * CacheItem references.
     */

    FuseIoqDelete(Instance->Ioq);

    FuseFileInstanceFini(Instance);

    FuseCacheDelete(Instance->Cache);

    FuseRwlockFinalize(&Instance->OpGuardLock);
}

VOID FuseInstanceExpirationRoutine(FUSE_INSTANCE *Instance, UINT64 ExpirationTime)
{
    PAGED_CODE();

    FuseCacheExpirationRoutine(Instance->Cache, Instance, ExpirationTime);
}

NTSTATUS FuseInstanceTransact(FUSE_INSTANCE *Instance,
    FUSE_PROTO_RSP *FuseResponse, ULONG InputBufferLength,
    FUSE_PROTO_REQ *FuseRequest, PULONG POutputBufferLength,
    PDEVICE_OBJECT DeviceObject, PFILE_OBJECT FileObject,
    PIRP CancellableIrp)
{
    PAGED_CODE();

    ULONG OutputBufferLength = *POutputBufferLength;
    FSP_FSCTL_TRANSACT_REQ *InternalRequest = 0;
    FSP_FSCTL_TRANSACT_RSP InternalResponse;
    FUSE_CONTEXT *Context;
    BOOLEAN Continue;
    NTSTATUS Result;

    *POutputBufferLength = 0;

    /* check parameters */
    if (0 != FuseResponse)
    {
        if (FUSE_PROTO_RSP_HEADER_SIZE > InputBufferLength ||
            FUSE_PROTO_RSP_HEADER_SIZE > FuseResponse->len ||
            FuseResponse->len > InputBufferLength)
            return STATUS_INVALID_PARAMETER;
    }
    if (0 != FuseRequest)
    {
        if (FUSE_PROTO_REQ_SIZEMIN > OutputBufferLength)
            return STATUS_BUFFER_TOO_SMALL;
    }

    if (0 != FuseResponse)
    {
        Context = FuseIoqEndProcessing(Instance->Ioq, FuseResponse->unique);
        if (0 == Context)
            goto request;

#if DBG
        if (fuse_debug & fuse_debug_dp)
            FuseDebugLogResponse(FuseResponse);
#endif

        Continue = FuseContextProcess(Context, FuseResponse, 0, 0);

        if (Continue)
            FuseIoqPostPending(Instance->Ioq, Context);
        else if (0 == Context->InternalRequest)
            FuseContextDelete(Context);
        else
        {
            ASSERT(FspFsctlTransactReservedKind != Context->InternalResponse->Kind);

            Result = FspFsextProviderTransact(
                DeviceObject, FileObject, Context->InternalResponse, 0);
            FuseContextDelete(Context);
            if (!NT_SUCCESS(Result))
                goto exit;
        }
    }

request:
    if (0 != FuseRequest)
    {
        RtlZeroMemory(FuseRequest, sizeof(FUSE_PROTO_REQ));

        Context = FuseIoqNextPending(Instance->Ioq);
        if (0 == Context)
        {
            UINT32 VersionMajor = Instance->VersionMajor;
            _ReadWriteBarrier();
                /*
                 * Compiler barrier only.
                 *
                 * A full memory barrier is not needed here, because:
                 *
                 * - WaitForSingleObject acts on a NotificationEvent that stays signaled.
                 * - WaitForSingleObject is a memory barrier.
                 */
            if (0 == VersionMajor)
            {
                Result = FsRtlCancellableWaitForSingleObject(&Instance->InitEvent,
                    0, CancellableIrp);
                if (STATUS_TIMEOUT == Result || STATUS_THREAD_IS_TERMINATING == Result)
                    Result = STATUS_CANCELLED;
                if (!NT_SUCCESS(Result))
                    goto exit;
                ASSERT(STATUS_SUCCESS == Result);

                VersionMajor = Instance->VersionMajor;
            }
            if ((UINT32)-1 == VersionMajor)
            {
                Result = STATUS_ACCESS_DENIED;
                goto exit;
            }

            Result = FspFsextProviderTransact(
                DeviceObject, FileObject, 0, &InternalRequest);
            if (!NT_SUCCESS(Result))
                goto exit;
            if (0 == InternalRequest)
            {
                Result = STATUS_SUCCESS;
                goto exit;
            }

            ASSERT(FspFsctlTransactReservedKind != InternalRequest->Kind);

            FuseContextCreate(&Context, Instance, InternalRequest);
            ASSERT(0 != Context);

            Continue = FALSE;
            if (!FuseContextIsStatus(Context))
            {
                InternalRequest = 0;
                Continue = FuseContextProcess(Context, 0, FuseRequest, OutputBufferLength);
            }
        }
        else
        {
            ASSERT(!FuseContextIsStatus(Context));
            Continue = FuseContextProcess(Context, 0, FuseRequest, OutputBufferLength);
        }

        if (Continue)
        {
            ASSERT(!FuseContextIsStatus(Context));
            FuseIoqStartProcessing(Instance->Ioq, Context);
        }
        else if (FuseContextIsStatus(Context))
        {
            ASSERT(0 != InternalRequest);
            RtlZeroMemory(&InternalResponse, sizeof InternalResponse);
            InternalResponse.Size = sizeof InternalResponse;
            InternalResponse.Kind = InternalRequest->Kind;
            InternalResponse.Hint = InternalRequest->Hint;
            InternalResponse.IoStatus.Status = FuseContextToStatus(Context);
            Result = FspFsextProviderTransact(
                DeviceObject, FileObject, &InternalResponse, 0);
            if (!NT_SUCCESS(Result))
                goto exit;
        }
        else if (0 == Context->InternalRequest)
        {
            switch (Context->InternalResponse->Hint)
            {
            case FUSE_PROTO_OPCODE_FORGET:
            case FUSE_PROTO_OPCODE_BATCH_FORGET:
                if (!IsListEmpty(&Context->Forget.ForgetList))
                    FuseIoqPostPending(Instance->Ioq, Context);
                else
                    FuseContextDelete(Context);
                break;
            }
        }
        else
        {
            Result = FspFsextProviderTransact(
                DeviceObject, FileObject, Context->InternalResponse, 0);
            FuseContextDelete(Context);
            if (!NT_SUCCESS(Result))
                goto exit;
        }

        *POutputBufferLength = FuseRequest->len;

#if DBG
        if (fuse_debug & fuse_debug_dp)
            FuseDebugLogRequest(FuseRequest);
#endif
    }

    Result = STATUS_SUCCESS;

exit:
    if (0 != InternalRequest)
        FuseFreeExternal(InternalRequest);

    return Result;
}
