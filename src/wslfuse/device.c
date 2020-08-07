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
typedef struct _MOUNT MOUNT;
typedef struct _DEVICE DEVICE;

struct _FILE
{
    LX_FILE Base;
    LIST_ENTRY ListEntry;
    DEVICE *Device;
    MOUNT *Mount;
    EX_PUSH_LOCK VolumeLock;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    FUSE_INSTANCE *FuseInstance;
    HANDLE VolumeHandle;
    PFILE_OBJECT VolumeFileObject;
    HANDLE WinMountHandle;
};

struct _MOUNT
{
    LONG RefCount;
    LIST_ENTRY ListEntry;
    UINT64 LxMountId;
    FILE *File;
};

struct _DEVICE
{
    LX_DEVICE Base;
    EX_PUSH_LOCK MountListLock;
    LIST_ENTRY MountList;
};

static VOID FuseMiscFileListInsert(FILE *File);
static VOID FuseMiscFileListRemove(FILE *File);

static inline MOUNT *MountCreate(VOID)
{
    MOUNT *Mount;

    Mount = FuseAllocNonPaged(sizeof *Mount);
    if (0 == Mount)
        return 0;

    RtlZeroMemory(Mount, sizeof *Mount);
    Mount->RefCount = 1;
    Mount->LxMountId = (UINT64)-1LL;

    return Mount;
}

static inline VOID MountReference(MOUNT *Mount)
{
    InterlockedIncrement(&Mount->RefCount);
}

static inline VOID MountDereference(MOUNT *Mount)
{
    LONG RefCount;

    RefCount = InterlockedDecrement(&Mount->RefCount);
    if (0 == RefCount)
        FuseFree(Mount);
}

static VOID FileProtoSendDestroyHandler(PVOID File0)
{
    FILE *File = (FILE *)File0;
    IO_STATUS_BLOCK IoStatus;

    ZwFsControlFile(
        File->VolumeHandle,
        0/*Event*/,
        0/*ApcRoutine*/,
        0/*ApcContext*/,
        &IoStatus,
        FSP_FSCTL_STOP,
        0/*InputBuffer*/,
        0/*InputBufferLength*/,
        0/*OutputBuffer*/,
        0/*OutputBufferLength*/);
}

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
    FUSE_INSTANCE *FuseInstance = 0;
    BOOLEAN InitDoneInstance = FALSE;
    HANDLE VolumeHandle = 0;
    PFILE_OBJECT VolumeFileObject = 0;
    WCHAR VolumeName[FSP_FSCTL_VOLUME_NAME_SIZEMAX / sizeof(WCHAR)];
    ULONG VolumeNameSize;

    ExAcquirePushLockExclusive(&File->VolumeLock);

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

    FuseInstance = FuseAllocNonPaged(sizeof *FuseInstance);
    if (0 == FuseInstance)
    {
        IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    IoStatus.Status = FuseInstanceInit(FuseInstance, &File->VolumeParams, FuseInstanceLinux);
    if (!NT_SUCCESS(IoStatus.Status))
        goto exit;
    FuseInstance->ProtoSendDestroyHandler = FileProtoSendDestroyHandler;
    FuseInstance->ProtoSendDestroyData = File;
    File->VolumeParams.TransactTimeout = 3000; /* transact timeout allows poll with LxpThreadWait */
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

    File->FuseInstance = FuseInstance;
    File->VolumeHandle = VolumeHandle;
    InterlockedExchangePointer(&File->VolumeFileObject, VolumeFileObject);

    FuseMiscFileListInsert(File);

    IoStatus.Status = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(IoStatus.Status))
    {
        if (0 != VolumeFileObject)
            ObDereferenceObject(VolumeFileObject);

        if (0 != VolumeHandle)
            ZwClose(VolumeHandle);

        if (InitDoneInstance)
            FuseInstanceFini(FuseInstance);

        if (0 != FuseInstance)
            FuseFree(FuseInstance);
    }

    ExReleasePushLockExclusive(&File->VolumeLock);

    /* !!!: REVISIT */
    return NT_SUCCESS(IoStatus.Status) ? 0 : -EIO;
}

static INT FileIoctlWinMount(
    FILE *File,
    WSLFUSE_IOCTL_WINMOUNT_ARG *Arg)
{
    UNICODE_STRING WinMountPoint;
    WCHAR WinMountPointBuf[4 + sizeof Arg->WinMountPoint];
    ULONG WinMountPointSize;
    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE WinMountHandle;
    NTSTATUS Result;

    ExAcquirePushLockExclusive(&File->VolumeLock);

    if (0 != File->WinMountHandle)
    {
        Result = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    Arg->WinMountPoint[sizeof Arg->WinMountPoint - 1] = '\0';
    WinMountPointSize = (ULONG)strlen(Arg->WinMountPoint);

    WinMountPointBuf[0] = '\\';
    WinMountPointBuf[1] = '?';
    WinMountPointBuf[2] = '?';
    WinMountPointBuf[3] = '\\';

    Result = RtlUTF8ToUnicodeN(
        WinMountPointBuf + 4,
        sizeof WinMountPointBuf - 4,
        &WinMountPointSize,
        Arg->WinMountPoint,
        WinMountPointSize);
    if (!NT_SUCCESS(Result))
        goto exit;

    WinMountPoint.Length = WinMountPoint.MaximumLength =
        (USHORT)(4 * sizeof(WCHAR) + WinMountPointSize);
    WinMountPoint.Buffer = WinMountPointBuf;

    InitializeObjectAttributes(
        &ObjectAttributes,
        &WinMountPoint,
        OBJ_KERNEL_HANDLE,
        0/*RootDirectory*/,
        0/*SecurityDescriptor*/);

    Result = ZwOpenSymbolicLinkObject(&WinMountHandle, 0, &ObjectAttributes);
    if (!NT_SUCCESS(Result))
        goto exit;

    File->WinMountHandle = WinMountHandle;

    Result = STATUS_SUCCESS;

exit:
    ExReleasePushLockExclusive(&File->VolumeLock);

    /* !!!: REVISIT */
    return NT_SUCCESS(Result) ? 0 : -EIO;
}

static INT FileIoctlLxMount(
    FILE *File,
    WSLFUSE_IOCTL_LXMOUNT_ARG *Arg)
{
    DEVICE *Device = File->Device;
    MOUNT *Mount;
    FILE *MountFile;
    INT Error = 0;

    ExAcquirePushLockExclusive(&Device->MountListLock);

    if ((UINT64)-1LL == Arg->LxMountId)
        Mount = File->Mount;
    else
    {
        Mount = 0;
        for (PLIST_ENTRY Entry = Device->MountList.Flink; &Device->MountList != Entry;)
        {
            MOUNT *Temp = CONTAINING_RECORD(Entry, MOUNT, ListEntry);
            Entry = Entry->Flink;
            if (Temp->LxMountId == Arg->LxMountId)
            {
                Mount = Temp;
                break;
            }
        }
    }

    switch (Arg->Operation)
    {
    case '+':
        if (0 != Mount)
        {
            Error = -EEXIST;
            break;
        }

        Mount = File->Mount;
        if ((UINT64)-1LL != Mount->LxMountId)
        {
            Error = -EINVAL;
            break;
        }

        MountReference(Mount);
        Mount->LxMountId = Arg->LxMountId;
        InsertTailList(&Device->MountList, &Mount->ListEntry);
        break;

    case '-':
        if (0 == Mount)
        {
            Error = -ENOENT;
            break;
        }

        if ((UINT64)-1LL == Mount->LxMountId)
        {
            Error = -EINVAL;
            break;
        }

        MountFile = Mount->File;

        RemoveEntryList(&Mount->ListEntry);
        Mount->LxMountId = (UINT64)-1LL;
        MountDereference(Mount);

        if (0 != MountFile)
            FuseProtoPostDestroy(MountFile->FuseInstance);
                /* ignore errors */
        break;

    case '?':
        if (0 == Mount)
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
    INT (*IoctlProc)(FILE *File, PVOID Arg);
    FILE *File = (FILE *)File0;
    PVOID SystemBuffer;
    INT Error;

    switch (Code)
    {
    case WSLFUSE_IOCTL_CREATEVOLUME:
        IoctlProc = FileIoctlCreateVolume;
        break;

    case WSLFUSE_IOCTL_WINMOUNT:
        IoctlProc = FileIoctlWinMount;
        break;

    case WSLFUSE_IOCTL_LXMOUNT:
        IoctlProc = FileIoctlLxMount;
        break;

    default:
        return -EINVAL;
    }

    Error = FileIoctlBegin(Code, Buffer, &SystemBuffer);
    if (0 == Error)
    {
        Error = IoctlProc(File, SystemBuffer);
        Error = FileIoctlEnd(Code, Buffer, &SystemBuffer, Error);
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
    ULONG OutputBufferLength;
    PFILE_OBJECT VolumeFileObject;
    NTSTATUS Result;

    VolumeFileObject = InterlockedCompareExchangePointer(&File->VolumeFileObject, 0, 0);
    if (0 == VolumeFileObject)
        return -ENODEV;

    for (;;)
    {
        OutputBufferLength = (ULONG)Length;
        Result = FuseInstanceTransact(File->FuseInstance,
            0, 0,
            Buffer, &OutputBufferLength,
            0, VolumeFileObject,
            0);
        if (!NT_SUCCESS(Result))
            return STATUS_CANCELLED == Result ? -ENODEV : -EIO;
        if (0 != OutputBufferLength)
            break;

        LARGE_INTEGER ZeroTimeout = { 0 };
        Result = LxpThreadWait(0, &ZeroTimeout, FALSE);
        if (STATUS_TIMEOUT != Result)
            return -EINTR;
    }

    *PBytesTransferred = OutputBufferLength;

    return 0;
}

static INT FileWriteVector(
    PLX_CALL_CONTEXT CallContext,
    PLX_FILE File0,
    PLX_IOVECTOR IoVector,
    POFF_T POffset,
    ULONG Flags,
    PSIZE_T PBytesTransferred)
{
    FILE *File = (FILE *)File0;
    FUSE_PROTO_RSP FuseResponseBuf, *FuseResponse = &FuseResponseBuf;
    ULONG InputBufferLength;
    ULONG OutputBufferLength = 0;
    PFILE_OBJECT VolumeFileObject;
    NTSTATUS Result;
    INT Error;

    VolumeFileObject = InterlockedCompareExchangePointer(&File->VolumeFileObject, 0, 0);
    if (0 == VolumeFileObject)
        return -ENODEV;

    InputBufferLength = 0;
    for (ULONG I = 0; (ULONG)IoVector->Count > I; I++)
        InputBufferLength += (ULONG)IoVector->Vector[I].Length;
    if (FUSE_PROTO_RSP_HEADER_SIZE > InputBufferLength)
        return -EINVAL;

    try
    {
        ULONG L = sizeof(FUSE_PROTO_RSP) < InputBufferLength ?
            FUSE_PROTO_RSP_HEADER_SIZE : sizeof(FUSE_PROTO_RSP);
        PUINT8 P = (PUINT8)&FuseResponseBuf, EndP = P + L;
        for (ULONG I = 0; (ULONG)IoVector->Count > I && EndP > P; I++)
        {
            L = (ULONG)(EndP - P);
            if (L > (ULONG)IoVector->Vector[I].Length)
                L = (ULONG)IoVector->Vector[I].Length;
            RtlCopyMemory(P, IoVector->Vector[I].Buffer, L);
            P += L;
        }

        if (sizeof(FUSE_PROTO_RSP) < InputBufferLength)
        {
            P = FuseAlloc(InputBufferLength);
            if (0 == P)
            {
                Error = -ENOMEM;
                goto exit;
            }
            FuseResponse = (PVOID)P;

            L = InputBufferLength;
            EndP = P + L;
            for (ULONG I = 0; (ULONG)IoVector->Count > I && EndP > P; I++)
            {
                L = (ULONG)(EndP - P);
                if (L > (ULONG)IoVector->Vector[I].Length)
                    L = (ULONG)IoVector->Vector[I].Length;
                RtlCopyMemory(P, IoVector->Vector[I].Buffer, L);
                P += L;
            }
        }
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        Error = -EFAULT;
        goto exit;
    }

    Result = FuseInstanceTransact(File->FuseInstance,
        FuseResponse, InputBufferLength,
        0, &OutputBufferLength,
        0, VolumeFileObject,
        0);
    if (!NT_SUCCESS(Result))
    {
        Error = STATUS_CANCELLED == Result ? -ENODEV : -EIO;
        goto exit;
    }

    *PBytesTransferred = InputBufferLength;

    Error = 0;

exit:
    if (&FuseResponseBuf != FuseResponse)
        FuseFree(FuseResponse);

    return Error;
}

static INT FileDelete(
    PLX_CALL_CONTEXT CallContext,
    PLX_FILE File0)
{
    FILE *File = (FILE *)File0;
    DEVICE *Device = File->Device;

    if (0 != File->VolumeFileObject)
        FuseMiscFileListRemove(File);

    ExAcquirePushLockExclusive(&Device->MountListLock);
    File->Mount->File = 0;
    ExReleasePushLockExclusive(&Device->MountListLock);

    if (0 != File->WinMountHandle)
        ZwClose(File->WinMountHandle);

    if (0 != File->VolumeFileObject)
    {
        ObDereferenceObject(File->VolumeFileObject);
        ZwClose(File->VolumeHandle);

        FuseInstanceFini(File->FuseInstance);
        FuseFree(File->FuseInstance);
    }

    MountDereference(File->Mount);

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
        .WriteVector = FileWriteVector,
    };
    DEVICE *Device = (DEVICE *)Device0;
    MOUNT *Mount = 0;
    FILE *File;
    INT Error;

    *PFile = 0;

    Mount = MountCreate();
    if (0 == Mount)
    {
        Error = -ENOMEM;
        goto exit;
    }

    File = (FILE *)VfsFileAllocate(sizeof *File, &FileCallbacks);
    if (0 == File)
    {
        Error = -ENOMEM;
        goto exit;
    }

    /* File: initialize fields. File->Base MUST be zeroed out. */
    RtlZeroMemory(File, sizeof *File);
    File->Device = (DEVICE *)Device;
    File->Mount = Mount;
    ExInitializePushLock(&File->VolumeLock);

    Mount->File = File;
    Mount = 0;

    *PFile = &File->Base;
    Error = 0;

exit:
    if (0 != Mount)
        MountDereference(Mount);

    return Error;
}

static INT DeviceDelete(
    PLX_DEVICE Device0)
{
    DEVICE *Device = (DEVICE *)Device0;
    MOUNT *Mount;

    for (PLIST_ENTRY Entry = Device->MountList.Flink; &Device->MountList != Entry;)
    {
        Mount = CONTAINING_RECORD(Entry, MOUNT, ListEntry);
        Entry = Entry->Flink;
        MountDereference(Mount);
    }

    return 0;
}

static EX_PUSH_LOCK FuseMiscLock;
static LIST_ENTRY FuseMiscFileList;
static KTIMER FuseMiscTimer;
static KDPC FuseMiscTimerDpc;
static WORK_QUEUE_ITEM FuseMiscTimerItem;
static BOOLEAN FuseMiscTimerInitDone;

static VOID FuseMiscFileListInsert(FILE *File)
{
    ExAcquirePushLockExclusive(&FuseMiscLock);

    InsertTailList(&FuseMiscFileList, &File->ListEntry);

    ExReleasePushLockExclusive(&FuseMiscLock);
}

static VOID FuseMiscFileListRemove(FILE *File)
{
    ExAcquirePushLockExclusive(&FuseMiscLock);

    RemoveEntryList(&File->ListEntry);

    ExReleasePushLockExclusive(&FuseMiscLock);
}

static VOID FuseMiscExpirationRoutine(PVOID Context)
{
    UINT64 InterruptTime;
    FILE *File;

    InterruptTime = KeQueryInterruptTime();

    ExAcquirePushLockShared(&FuseMiscLock);

    for (PLIST_ENTRY Entry = FuseMiscFileList.Flink; &FuseMiscFileList != Entry;)
    {
        File = CONTAINING_RECORD(Entry, FILE, ListEntry);
        Entry = Entry->Flink;
        FuseInstanceExpirationRoutine(File->FuseInstance, InterruptTime);
    }

    ExReleasePushLockShared(&FuseMiscLock);
}

static VOID FuseMiscTimerDpcRoutine(PKDPC Dpc,
    PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
{
    // !PAGED_CODE();

    ExQueueWorkItem(&FuseMiscTimerItem, DelayedWorkQueue);
}

static VOID FuseMiscTimerInit(VOID)
{
    PAGED_CODE();

    ExAcquirePushLockExclusive(&FuseMiscLock);

    if (!FuseMiscTimerInitDone)
    {
        KeInitializeTimer(&FuseMiscTimer);
        KeInitializeDpc(&FuseMiscTimerDpc, FuseMiscTimerDpcRoutine, 0);
        ExInitializeWorkItem(&FuseMiscTimerItem, FuseMiscExpirationRoutine, 0);

        LONG Period = 1000; /* 1000ms = 1s */
        LARGE_INTEGER DueTime;
        DueTime.QuadPart = -10000LL * Period;
        KeSetTimerEx(&FuseMiscTimer, DueTime, Period, &FuseMiscTimerDpc);

        FuseMiscTimerInitDone = TRUE;
    }

    ExReleasePushLockExclusive(&FuseMiscLock);
}

INT FuseMiscRegister(PLX_INSTANCE Instance)
{
    static LX_DEVICE_CALLBACKS DeviceCallbacks =
    {
        .Open = DeviceOpen,
        .Delete = DeviceDelete,
    };
    DEVICE *Device = 0;
    INT Error;

    FuseMiscTimerInit();

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

VOID FuseMiscInitialize(VOID)
{
    ExInitializePushLock(&FuseMiscLock);
    InitializeListHead(&FuseMiscFileList);
}
