/**
 * @file wslfuse/device.c
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

#include <wslfuse/driver.h>

#define FUSE_MINOR                      229

typedef struct _FILE FILE;
typedef struct _MOUNTID MOUNTID;
typedef struct _DEVICE DEVICE;

struct _FILE
{
    LX_FILE Base;
    DEVICE *Device;
    EX_PUSH_LOCK CreateVolumeLock;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    HANDLE VolumeHandle;
    PFILE_OBJECT VolumeFileObject;
    FUSE_INSTANCE FuseInstance;
};

struct _MOUNTID
{
    LIST_ENTRY ListEntry;
    UINT64 MountId;
};

struct _DEVICE
{
    LX_DEVICE Base;
    EX_PUSH_LOCK MountListLock;
    LIST_ENTRY MountList;
        /*
         * The mount id list is maintained as a linked list.
         * This will make searches slow, but this should be ok,
         * because it is unlikely that there will be too many
         * mounts to track. If this assumption changes we may
         * have to revisit.
         */
};

static INT FileIoctlCreateVolume(
    FILE *File,
    WSLFUSE_IOCTL_CREATEVOLUME_ARG *Arg)
{
    SIZE_T VolumeParamsSize;
    UNICODE_STRING DevicePath;
    WCHAR DevicePathBuf[260/*MAX_PATH*/ + sizeof(FSP_FSCTL_VOLUME_PARAMS)];
    WCHAR *DevicePathPtr, *DevicePathEnd;
    OBJECT_ATTRIBUTES ObjectAttributes;
    IO_STATUS_BLOCK IoStatus;
    BOOLEAN InitDoneInstance = FALSE;
    HANDLE VolumeHandle = 0;
    PFILE_OBJECT VolumeFileObject = 0;
    WCHAR VolumeName[FSP_FSCTL_VOLUME_NAME_SIZEMAX / sizeof(WCHAR)];
    ULONG VolumeNameSize;

    ExAcquirePushLockExclusive(&File->CreateVolumeLock);

    if (0 != File->VolumeFileObject)
    {
        IoStatus.Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    VolumeParamsSize = 0 == Arg->VolumeParams.Version ?
        sizeof(FSP_FSCTL_VOLUME_PARAMS_V0) :
        Arg->VolumeParams.Version;
    if (sizeof(FSP_FSCTL_VOLUME_PARAMS) < VolumeParamsSize)
    {
        IoStatus.Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }
    RtlCopyMemory(&File->VolumeParams, &Arg->VolumeParams, VolumeParamsSize);

    IoStatus.Status = FuseInstanceInit(&File->FuseInstance, &File->VolumeParams, FuseInstanceLinux);
    if (!NT_SUCCESS(IoStatus.Status))
        goto exit;
    InitDoneInstance = TRUE;

    RtlInitEmptyUnicodeString(&DevicePath, DevicePathBuf, sizeof DevicePathBuf);
    RtlAppendUnicodeToString(&DevicePath, L'\0' == File->VolumeParams.Prefix[0] ?
        L"\\Device\\" FSP_FSCTL_DISK_DEVICE_NAME :
        L"\\Device\\" FSP_FSCTL_NET_DEVICE_NAME);
    RtlAppendUnicodeToString(&DevicePath, L"" FSP_FSCTL_VOLUME_PARAMS_PREFIX);
    DevicePathPtr = (PVOID)((PUINT8)DevicePath.Buffer + DevicePath.Length);
    DevicePathEnd = (PVOID)((PUINT8)DevicePathPtr + VolumeParamsSize * sizeof(WCHAR));
    for (PUINT8 VolumeParamsPtr = (PVOID)&File->VolumeParams;
        DevicePathEnd > DevicePathPtr; DevicePathPtr++, VolumeParamsPtr++)
    {
        WCHAR Value = 0xF000 | *VolumeParamsPtr;
        *DevicePathPtr = Value;
    }
    DevicePath.Length = (USHORT)((PUINT8)DevicePathPtr - (PUINT8)DevicePath.Buffer);

    InitializeObjectAttributes(
        &ObjectAttributes,
        &DevicePath,
        OBJ_KERNEL_HANDLE,
        0/*RootDirectory*/,
        0/*SecurityDescriptor*/);

    IoStatus.Status = ZwOpenFile(
        &VolumeHandle,
        0/*DesiredAccess*/,
        &ObjectAttributes,
        &IoStatus,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        0/*OpenOptions*/);
    if (!NT_SUCCESS(IoStatus.Status))
        goto exit;

    IoStatus.Status = ObReferenceObjectByHandle(
        VolumeHandle,
        0/*DesiredAccess*/,
        *IoFileObjectType,
        KernelMode,
        &VolumeFileObject,
        0/*HandleInformation*/);
    if (!NT_SUCCESS(IoStatus.Status))
        goto exit;

    IoStatus.Status = ZwFsControlFile(
        VolumeHandle,
        0/*Event*/,
        0/*ApcRoutine*/,
        0/*ApcContext*/,
        &IoStatus,
        FSP_FSCTL_VOLUME_NAME,
        0/*InputBuffer*/,
        0/*InputBufferLength*/,
        VolumeName,
        sizeof VolumeName);
    if (!NT_SUCCESS(IoStatus.Status))
        goto exit;

    RtlZeroMemory(Arg, sizeof *Arg);
    VolumeNameSize = (ULONG)(wcslen(VolumeName) + 1) * sizeof(WCHAR);
    IoStatus.Status = RtlUnicodeToUTF8N(
        Arg->VolumeName,
        sizeof Arg->VolumeName,
        &VolumeNameSize,
        VolumeName,
        VolumeNameSize);
    if (!NT_SUCCESS(IoStatus.Status))
        goto exit;

    File->VolumeHandle = VolumeHandle;
    InterlockedExchangePointer(&File->VolumeFileObject, VolumeFileObject);

    IoStatus.Status = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(IoStatus.Status))
    {
        if (0 != VolumeFileObject)
            ObDereferenceObject(VolumeFileObject);

        if (0 != VolumeHandle)
            ZwClose(VolumeHandle);

        if (InitDoneInstance)
            FuseInstanceFini(&File->FuseInstance);
    }

    ExReleasePushLockExclusive(&File->CreateVolumeLock);

    /* !!!: REVISIT */
    return NT_SUCCESS(IoStatus.Status) ? 0 : -EIO;
}

static INT FileIoctlMount(
    FILE *File,
    WSLFUSE_IOCTL_MOUNTID_ARG *Arg)
{
    DEVICE *Device = File->Device;
    MOUNTID *MountId;
    INT Error = 0;

    ExAcquirePushLockExclusive(&Device->MountListLock);

    MountId = 0;
    for (PLIST_ENTRY Entry = Device->MountList.Flink; &Device->MountList != Entry;)
    {
        MOUNTID *Temp = CONTAINING_RECORD(Entry, MOUNTID, ListEntry);
        Entry = Entry->Flink;
        if (Temp->MountId == Arg->MountId)
        {
            MountId = Temp;
            break;
        }
    }

    switch (Arg->Operation)
    {
    case '+':
        if (0 != MountId)
        {
            Error = -EEXIST;
            break;
        }

        MountId = FuseAllocNonPaged(sizeof *MountId);
        if (0 == MountId)
        {
            Error = -ENOMEM;
            break;
        }

        RtlZeroMemory(MountId, sizeof *MountId);
        MountId->MountId = Arg->MountId;
        InsertTailList(&Device->MountList, &MountId->ListEntry);
        break;

    case '-':
        if (0 == MountId)
        {
            Error = -ENOENT;
            break;
        }

        RemoveEntryList(&MountId->ListEntry);
        FuseFree(MountId);
        break;

    case '?':
        if (0 == MountId)
        {
            Error = -ENOENT;
            break;
        }
        break;

    default:
        Error = -EINVAL;
        break;
    }

    ExReleasePushLockExclusive(&Device->MountListLock);

    return Error;
}

static INT FileIoctlBegin(
    ULONG Code,
    PVOID Buffer,
    PVOID *PSystemBuffer)
{
    ULONG Size = (Code >> 16) & 0x3fff;
    PVOID SystemBuffer = 0;
    INT Error;

    *PSystemBuffer = 0;

    if (FlagOn(Code, 0xc0000000/* _IOC_WRITE | _IOC_READ */) &&
        0 != Size)
    {
        SystemBuffer = FuseAllocNonPaged(Size);
        if (0 == SystemBuffer)
        {
            Error = -ENOMEM;
            goto exit;
        }

        try
        {
            if (FlagOn(Code, 0x80000000/* _IOC_READ */))
                ProbeForWrite(Buffer, Size, 1);
            else
                ProbeForRead(Buffer, Size, 1);

            if (FlagOn(Code, 0x40000000/* _IOC_WRITE */))
                RtlCopyMemory(SystemBuffer, Buffer, Size);
            else
                RtlZeroMemory(SystemBuffer, Size);
        }
        except (EXCEPTION_EXECUTE_HANDLER)
        {
            Error = -EFAULT;
            goto exit;
        }
    }

    *PSystemBuffer = SystemBuffer;
    SystemBuffer = 0;
    Error = 0;

exit:
    if (0 != SystemBuffer)
        FuseFree(SystemBuffer);

    return Error;
}

static INT FileIoctlEnd(
    ULONG Code,
    PVOID Buffer,
    PVOID *PSystemBuffer,
    INT Error)
{
    ULONG Size = (Code >> 16) & 0x3fff;
    PVOID SystemBuffer = *PSystemBuffer;

    if (0 == Error &&
        FlagOn(Code, 0x80000000/* _IOC_READ */) &&
        0 != Size)
    {
        try
        {
            ProbeForWrite(Buffer, Size, 1);
            RtlCopyMemory(Buffer, SystemBuffer, Size);
        }
        except (EXCEPTION_EXECUTE_HANDLER)
        {
            Error = -EFAULT;
            goto exit;
        }
    }

    *PSystemBuffer = 0;

exit:
    if (0 != SystemBuffer)
        FuseFree(SystemBuffer);

    return Error;
}

static INT FileIoctl(
    PLX_CALL_CONTEXT CallContext,
    PLX_FILE File0,
    ULONG Code,
    PVOID Buffer)
{
    FILE *File = (FILE *)File0;
    PVOID SystemBuffer;
    INT Error;

    switch (Code)
    {
    case WSLFUSE_IOCTL_CREATEVOLUME:
        Error = FileIoctlBegin(Code, Buffer, &SystemBuffer);
        if (0 == Error)
        {
            Error = FileIoctlCreateVolume(File, SystemBuffer);
            Error = FileIoctlEnd(Code, Buffer, &SystemBuffer, Error);
        }
        break;

    case WSLFUSE_IOCTL_MOUNTID:
        Error = FileIoctlBegin(Code, Buffer, &SystemBuffer);
        if (0 == Error)
        {
            Error = FileIoctlMount(File, SystemBuffer);
            Error = FileIoctlEnd(Code, Buffer, &SystemBuffer, Error);
        }
        break;

    default:
        Error = -EINVAL;
        break;
    }

    return Error;
}

static INT FileRead(
    PLX_CALL_CONTEXT CallContext,
    PLX_FILE File0,
    PVOID Buffer,
    SIZE_T Length,
    POFF_T POffset,
    PSIZE_T PBytesTransferred)
{
    FILE *File = (FILE *)File0;
    ULONG OutputBufferLength = (ULONG)Length;
    PFILE_OBJECT VolumeFileObject;
    NTSTATUS Result;

    VolumeFileObject = InterlockedCompareExchangePointer(&File->VolumeFileObject, 0, 0);
    if (0 == VolumeFileObject)
        return -ENODEV;

    Result = FuseInstanceTransact(&File->FuseInstance,
        0, 0,
        Buffer, &OutputBufferLength,
        0, VolumeFileObject,
        0);
    if (!NT_SUCCESS(Result))
        return -EIO; // !!!: REVISIT

    *PBytesTransferred = OutputBufferLength;

    return 0;
}

static INT FileWrite(
    PLX_CALL_CONTEXT CallContext,
    PLX_FILE File0,
    PVOID Buffer,
    SIZE_T Length,
    POFF_T POffset,
    PSIZE_T PBytesTransferred)
{
    FILE *File = (FILE *)File0;
    ULONG InputBufferLength = (ULONG)Length;
    PFILE_OBJECT VolumeFileObject;
    NTSTATUS Result;

    VolumeFileObject = InterlockedCompareExchangePointer(&File->VolumeFileObject, 0, 0);
    if (0 == VolumeFileObject)
        return -ENODEV;

    Result = FuseInstanceTransact(&File->FuseInstance,
        Buffer, InputBufferLength,
        0, 0,
        0, VolumeFileObject,
        0);
    if (!NT_SUCCESS(Result))
        return -EIO; // !!!: REVISIT

    *PBytesTransferred = InputBufferLength;

    return 0;
}

static INT FileDelete(
    PLX_CALL_CONTEXT CallContext,
    PLX_FILE File0)
{
    FILE *File = (FILE *)File0;

    if (0 != File->VolumeFileObject)
        FuseInstanceFini(&File->FuseInstance);

    if (0 != File->VolumeFileObject)
        ObDereferenceObject(File->VolumeFileObject);

    if (0 != File->VolumeHandle)
        ZwClose(File->VolumeHandle);

    return 0;
}

static INT DeviceOpen(
    PLX_CALL_CONTEXT CallContext,
    PLX_DEVICE Device0,
    ULONG Flags,
    PLX_FILE *PFile)
{
    static LX_FILE_CALLBACKS FileCallbacks =
    {
        .Delete = FileDelete,
        .Ioctl = FileIoctl,
        .Read = FileRead,
        .Write = FileWrite,
    };
    DEVICE *Device = (DEVICE *)Device0;
    FILE *File;
    INT Error;

    *PFile = 0;

    File = (FILE *)VfsFileAllocate(sizeof *File, &FileCallbacks);
    if (0 == File)
    {
        Error = -ENOMEM;
        goto exit;
    }

    /* File: initialize fields. File->Base MUST be zeroed out. */
    RtlZeroMemory(File, sizeof *File);
    File->Device = (DEVICE *)Device;
    ExInitializePushLock(&File->CreateVolumeLock);

    *PFile = &File->Base;
    Error = 0;

exit:
    return Error;
}

static INT DeviceDelete(
    PLX_DEVICE Device0)
{
    DEVICE *Device = (DEVICE *)Device0;
    MOUNTID *MountId;

    /* Device: finalize fields */
    for (PLIST_ENTRY Entry = Device->MountList.Flink; &Device->MountList != Entry;)
    {
        MountId = CONTAINING_RECORD(Entry, MOUNTID, ListEntry);
        Entry = Entry->Flink;
        FuseFree(MountId);
    }

    return 0;
}

INT FuseMiscRegister(
    PLX_INSTANCE Instance)
{
    static LX_DEVICE_CALLBACKS DeviceCallbacks =
    {
        .Open = DeviceOpen,
        .Delete = DeviceDelete,
    };
    DEVICE *Device = 0;
    INT Error;

    Device = (DEVICE *)VfsDeviceMinorAllocate(&DeviceCallbacks, sizeof *Device);
    if (0 == Device)
    {
        Error = -ENOMEM;
        goto exit;
    }

    /* Device: initialize fields. Device->Base MUST NOT be zeroed out. */
    ExInitializePushLock(&Device->MountListLock);
    InitializeListHead(&Device->MountList);

    LxpDevMiscRegister(Instance, &Device->Base, FUSE_MINOR);

    Error = 0;

exit:
    if (0 != Device)
        VfsDeviceMinorDereference(&Device->Base);

    return Error;
}
