/**
 * @file winfuse/util.c
 *
 * @copyright 2019 Bill Zissimopoulos
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

PVOID FuseAllocatePoolMustSucceed(POOL_TYPE PoolType, SIZE_T Size, ULONG Tag);
NTSTATUS FuseGetTokenUid(PACCESS_TOKEN Token, TOKEN_INFORMATION_CLASS InfoClass, PUINT32 PUid);
NTSTATUS FuseSendTransactInternalIrp(PDEVICE_OBJECT DeviceObject, PFILE_OBJECT FileObject,
    FSP_FSCTL_TRANSACT_RSP *Response, FSP_FSCTL_TRANSACT_REQ **PRequest);

#ifdef ALLOC_PRAGMA
// !#pragma alloc_text(PAGE, FuseAllocatePoolMustSucceed)
#pragma alloc_text(PAGE, FuseGetTokenUid)
#pragma alloc_text(PAGE, FuseSendTransactInternalIrp)
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

NTSTATUS FuseSendTransactInternalIrp(PDEVICE_OBJECT DeviceObject, PFILE_OBJECT FileObject,
    FSP_FSCTL_TRANSACT_RSP *Response, FSP_FSCTL_TRANSACT_REQ **PRequest)
{
    PAGED_CODE();

    /*
     * This function uses IoBuildDeviceIoControlRequest to build an IRP and then send it
     * to the WinFsp driver. IoBuildDeviceIoControlRequest places the IRP in the IRP queue
     * thus allowing it to be cancelled with CancelSynchronousIo.
     *
     * Special kernel APC's must be enabled:
     * See https://www.osr.com/blog/2018/02/14/beware-iobuilddeviceiocontrolrequest/
     */
    ASSERT(!KeAreAllApcsDisabled());

    NTSTATUS Result;
    IO_STATUS_BLOCK IoStatus;
    PIRP Irp;
    PIO_STACK_LOCATION IrpSp;

    if (0 != PRequest)
        *PRequest = 0;

    if (0 == DeviceObject)
        DeviceObject = IoGetRelatedDeviceObject(FileObject);

    Irp = IoBuildDeviceIoControlRequest(FSP_FSCTL_TRANSACT_INTERNAL,
        DeviceObject,
        Response,
        0 != Response ? Response->Size : 0,
        PRequest,
        0 != PRequest ? sizeof(PVOID) : 0,
        FALSE,
        0,
        &IoStatus);
    if (0 == Irp)
        return STATUS_INSUFFICIENT_RESOURCES;

    /*
     * IoBuildDeviceIoControlRequest builds an IOCTL IRP without a FileObject.
     * Patch it so that it is an FSCTL IRP with a FileObject. Mark it as
     * IRP_SYNCHRONOUS_API so that CancelSynchronousIo can cancel it.
     */
    Irp->Flags |= IRP_SYNCHRONOUS_API;
    IrpSp = IoGetNextIrpStackLocation(Irp);
    IrpSp->MajorFunction = IRP_MJ_FILE_SYSTEM_CONTROL;
    IrpSp->MinorFunction = IRP_MN_USER_FS_REQUEST;
    IrpSp->FileObject = FileObject;

    Result = IoCallDriver(DeviceObject, Irp);
    ASSERT(STATUS_PENDING != Result);

    return Result;
}
