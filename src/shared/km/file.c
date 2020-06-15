/**
 * @file shared/km/file.c
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

VOID FuseFileInstanceInit(FUSE_INSTANCE *Instance)
{
    KeInitializeSpinLock(&Instance->FileListLock);
    InitializeListHead(&Instance->FileList);
}

VOID FuseFileInstanceFini(FUSE_INSTANCE *Instance)
{
    FUSE_FILE *File;

    for (PLIST_ENTRY Entry = Instance->FileList.Flink; &Instance->FileList != Entry;)
    {
        File = CONTAINING_RECORD(Entry, FUSE_FILE, ListEntry);
        Entry = Entry->Flink;
        FuseCacheDereferenceItem(Instance->Cache, File->CacheItem);
        FuseFree(File);
    }
}

NTSTATUS FuseFileCreate(FUSE_INSTANCE *Instance, FUSE_FILE **PFile)
{
    KIRQL Irql;
    FUSE_FILE *File;

    *PFile = 0;

    File = FuseAllocNonPaged(sizeof *File);
        /* spinlocks must operate on non-paged memory */
    if (0 == File)
        return STATUS_INSUFFICIENT_RESOURCES;

    RtlZeroMemory(File, sizeof *File);

    KeAcquireSpinLock(&Instance->FileListLock, &Irql);
    InsertTailList(&Instance->FileList, &File->ListEntry);
    KeReleaseSpinLock(&Instance->FileListLock, Irql);

    *PFile = File;

    return STATUS_SUCCESS;
}

VOID FuseFileDelete(FUSE_INSTANCE *Instance, FUSE_FILE *File)
{
    KIRQL Irql;

    KeAcquireSpinLock(&Instance->FileListLock, &Irql);
    RemoveEntryList(&File->ListEntry);
    KeReleaseSpinLock(&Instance->FileListLock, Irql);

    FuseCacheDereferenceItem(Instance->Cache, File->CacheItem);

    DEBUGFILL(File, sizeof *File);
    FuseFree(File);
}
