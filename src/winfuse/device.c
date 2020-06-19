/**
 * @file winfuse/device.c
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

static NTSTATUS FuseDeviceInit(PDEVICE_OBJECT DeviceObject, FSP_FSCTL_VOLUME_PARAMS *VolumeParams);
static VOID FuseDeviceFini(PDEVICE_OBJECT DeviceObject);
static VOID FuseDeviceExpirationRoutine(PDEVICE_OBJECT DeviceObject, UINT64 ExpirationTime);
static NTSTATUS FuseDeviceTransact(PDEVICE_OBJECT DeviceObject, PIRP Irp);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FuseDeviceInit)
#pragma alloc_text(PAGE, FuseDeviceFini)
#pragma alloc_text(PAGE, FuseDeviceExpirationRoutine)
#pragma alloc_text(PAGE, FuseDeviceTransact)
#endif

static NTSTATUS FuseDeviceInit(PDEVICE_OBJECT DeviceObject, FSP_FSCTL_VOLUME_PARAMS *VolumeParams)
{
    PAGED_CODE();

    FUSE_INSTANCE *Instance = FuseInstanceFromDeviceObject(DeviceObject);
    NTSTATUS Result;

    KeEnterCriticalRegion();

    Result = FuseInstanceInit(Instance, VolumeParams, FuseInstanceWindows);

    KeLeaveCriticalRegion();

    return Result;
}

static VOID FuseDeviceFini(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FUSE_INSTANCE *Instance = FuseInstanceFromDeviceObject(DeviceObject);

    KeEnterCriticalRegion();

    FuseInstanceFini(Instance);

    KeLeaveCriticalRegion();
}

static VOID FuseDeviceExpirationRoutine(PDEVICE_OBJECT DeviceObject, UINT64 ExpirationTime)
{
    PAGED_CODE();

    FUSE_INSTANCE *Instance = FuseInstanceFromDeviceObject(DeviceObject);

    KeEnterCriticalRegion();

    FuseInstanceExpirationRoutine(Instance, ExpirationTime);

    KeLeaveCriticalRegion();
}

static NTSTATUS FuseDeviceTransact(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PAGED_CODE();

    ASSERT(KeAreApcsDisabled());

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    ASSERT(IRP_MJ_FILE_SYSTEM_CONTROL == IrpSp->MajorFunction);
    ASSERT(IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction);
    ASSERT(FUSE_FSCTL_TRANSACT == IrpSp->Parameters.FileSystemControl.FsControlCode);
    ASSERT(METHOD_BUFFERED == (IrpSp->Parameters.FileSystemControl.FsControlCode & 3));
    ASSERT(IrpSp->FileObject->FsContext2 == DeviceObject);

    FUSE_INSTANCE *Instance = FuseInstanceFromDeviceObject(DeviceObject);
    ULONG InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    FUSE_PROTO_RSP *FuseResponse = 0 != InputBufferLength ? Irp->AssociatedIrp.SystemBuffer : 0;
    FUSE_PROTO_REQ *FuseRequest = 0 != OutputBufferLength ? Irp->AssociatedIrp.SystemBuffer : 0;
    NTSTATUS Result;

    Result = FuseInstanceTransact(Instance,
        FuseResponse, InputBufferLength,
        FuseRequest, &OutputBufferLength,
        IrpSp->DeviceObject, IrpSp->FileObject,
        Irp);

    Irp->IoStatus.Information = OutputBufferLength;

    return Result;
}

FSP_FSEXT_PROVIDER FuseProvider =
{
    /* Version */
    sizeof FuseProvider,

    /* DeviceTransactCode */
    FUSE_FSCTL_TRANSACT,

    /* DeviceExtensionSize */
    sizeof(FUSE_INSTANCE),

    /* DeviceInit */
    FuseDeviceInit,

    /* DeviceFini */
    FuseDeviceFini,

    /* DeviceExpirationRoutine */
    FuseDeviceExpirationRoutine,

    /* DeviceTransact */
    FuseDeviceTransact,
};
