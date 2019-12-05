/**
 * @file winfuse/file.c
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

VOID FuseFileDeviceInit(PDEVICE_OBJECT DeviceObject)
{
    FUSE_DEVICE_EXTENSION *DeviceExtension = FuseDeviceExtension(DeviceObject);

    KeInitializeSpinLock(&DeviceExtension->FileListLock);
    InitializeListHead(&DeviceExtension->FileList);
}

VOID FuseFileDeviceFini(PDEVICE_OBJECT DeviceObject)
{
    FUSE_DEVICE_EXTENSION *DeviceExtension = FuseDeviceExtension(DeviceObject);
    FUSE_FILE *File;

    for (PLIST_ENTRY Entry = DeviceExtension->FileList.Flink; &DeviceExtension->FileList != Entry;)
    {
        File = CONTAINING_RECORD(Entry, FUSE_FILE, ListEntry);
        Entry = Entry->Flink;
        FuseCacheDereferenceItem(DeviceExtension->Cache, File->CacheItem);
        FuseFree(File);
    }
}

NTSTATUS FuseFileCreate(PDEVICE_OBJECT DeviceObject, FUSE_FILE **PFile)
{
    FUSE_DEVICE_EXTENSION *DeviceExtension = FuseDeviceExtension(DeviceObject);
    KIRQL Irql;
    FUSE_FILE *File;

    *PFile = 0;

    File = FuseAllocNonPaged(sizeof *File);
        /* spinlocks must operate on non-paged memory */
    if (0 == File)
        return STATUS_INSUFFICIENT_RESOURCES;

    RtlZeroMemory(File, sizeof *File);

    KeAcquireSpinLock(&DeviceExtension->FileListLock, &Irql);
    InsertTailList(&DeviceExtension->FileList, &File->ListEntry);
    KeReleaseSpinLock(&DeviceExtension->FileListLock, Irql);

    *PFile = File;

    return STATUS_SUCCESS;
}

VOID FuseFileDelete(PDEVICE_OBJECT DeviceObject, FUSE_FILE *File)
{
    FUSE_DEVICE_EXTENSION *DeviceExtension = FuseDeviceExtension(DeviceObject);
    KIRQL Irql;

    KeAcquireSpinLock(&DeviceExtension->FileListLock, &Irql);
    RemoveEntryList(&File->ListEntry);
    KeReleaseSpinLock(&DeviceExtension->FileListLock, Irql);

    FuseCacheDereferenceItem(DeviceExtension->Cache, File->CacheItem);

    DEBUGFILL(File, sizeof *File);
    FuseFree(File);
}
