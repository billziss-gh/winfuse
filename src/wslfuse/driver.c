/**
 * @file wslfuse/driver.c
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

#include <ntifs.h>
#include <lxdk/lxdk.h>

#pragma warning(disable:4100)           /* unreferenced formal parameter */

#define LOG(Format, ...)                DbgPrint("%s" Format "\n", __FUNCTION__, __VA_ARGS__)
#define POOLTAG                         'LXDK'
#define BUFSIZE                         1024

typedef struct
{
    LX_DEVICE Base;
    PUINT8 Buffer;
} DEVICE;

typedef struct
{
    LX_FILE Base;
    DEVICE *Device;
    EX_PUSH_LOCK Lock;
    OFF_T Offset;
} FILE;

static INT FileDelete(
    PLX_CALL_CONTEXT CallContext,
    PLX_FILE File)
{
    INT Error;

    Error = 0;

    LOG("(File=%p) = %d", File, Error);
    return Error;
}

static INT FileFlush(
    PLX_CALL_CONTEXT CallContext,
    PLX_FILE File)
{
    INT Error;

    Error = 0;

    LOG("(File=%p) = %d", File, Error);
    return Error;
}

static INT FileIoctl(
    PLX_CALL_CONTEXT CallContext,
    PLX_FILE File0,
    ULONG Code,
    PVOID Buffer)
{
    FILE *File = (FILE *)File0;
    INT Error;

    switch (Code)
    {
    case 0x8ead:
        try
        {
            ProbeForWrite(Buffer, sizeof(ULONG), 1);
            RtlCopyMemory(Buffer, File->Device->Buffer, sizeof(ULONG));
        }
        except (EXCEPTION_EXECUTE_HANDLER)
        {
            Error = -EFAULT;
            goto exit;
        }
        break;

    case 0x817e:
        try
        {
            ProbeForRead(Buffer, sizeof(ULONG), 1);
            RtlCopyMemory(File->Device->Buffer, Buffer, sizeof(ULONG));
        }
        except (EXCEPTION_EXECUTE_HANDLER)
        {
            Error = -EFAULT;
            goto exit;
        }

    default:
        Error = -EINVAL;
        goto exit;
    }

    Error = 0;

exit:
    LOG("(File=%p, Code=%lx) = %d", File, Code, Error);
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
    OFF_T Offset, EndOffset;
    INT Error;

    *PBytesTransferred = 0;

    if (0 == POffset)
    {
        ExAcquirePushLockExclusive(&File->Lock);
        Offset = File->Offset;
    }
    else
        Offset = *POffset;
    EndOffset = Offset + Length;
    if (Offset > BUFSIZE)
        Offset = BUFSIZE;
    if (EndOffset > BUFSIZE)
        EndOffset = BUFSIZE;

    try
    {
        ProbeForWrite(Buffer, EndOffset - Offset, 1);
        RtlCopyMemory(Buffer, File->Device->Buffer + Offset, EndOffset - Offset);
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        Error = -EFAULT;
        goto exit;
    }

    if (0 == POffset)
    {
        File->Offset = EndOffset;
        ExReleasePushLockExclusive(&File->Lock);
    }
    else
        *POffset = EndOffset;

    *PBytesTransferred = EndOffset - Offset;
    Error = 0;

exit:
    LOG("(File=%p, Length=%lu, Offset=%lx, *PBytesTransferred=%lu) = %d",
        File,
        (unsigned)Length,
        (unsigned)(0 != POffset ? *POffset : -1),
        (unsigned)*PBytesTransferred,
        Error);
    return Error;
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
    OFF_T Offset, EndOffset;
    INT Error;

    *PBytesTransferred = 0;

    if (0 == POffset)
    {
        ExAcquirePushLockExclusive(&File->Lock);
        Offset = File->Offset;
    }
    else
        Offset = *POffset;
    EndOffset = Offset + Length;
    if (Offset > BUFSIZE)
        Offset = BUFSIZE;
    if (EndOffset > BUFSIZE)
        EndOffset = BUFSIZE;

    try
    {
        ProbeForRead(Buffer, EndOffset - Offset, 1);
        RtlCopyMemory(File->Device->Buffer + Offset, Buffer, EndOffset  - Offset);
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        Error = -EFAULT;
        goto exit;
    }

    if (0 == POffset)
    {
        File->Offset += Length;
        ExReleasePushLockExclusive(&File->Lock);
    }
    else
        *POffset += Length;

    *PBytesTransferred = Length;
    Error = 0;

exit:
    LOG("(File=%p, Length=%lu, Offset=%lx, *PBytesTransferred=%lu) = %d",
        File,
        (unsigned)Length,
        (unsigned)(0 != POffset ? *POffset : -1),
        (unsigned)*PBytesTransferred,
        Error);
    return Error;
}

static INT FileSeek(
    PLX_CALL_CONTEXT CallContext,
    PLX_FILE File0,
    OFF_T Offset,
    INT Whence,
    POFF_T PResultOffset)
{
    FILE *File = (FILE *)File0;
    INT Error;

    ExAcquirePushLockExclusive(&File->Lock);

    switch (Whence)
    {
    case 0/* SEEK_SET */:
        File->Offset = Offset;
        break;

    case 1/* SEEK_CUR */:
        File->Offset = File->Offset + Offset;
        break;

    case 2/* SEEK_END */:
        File->Offset = BUFSIZE + Offset; /* we do not maintain file "size" so best we can do! */
        break;

    default:
        Error = -EINVAL;
        goto exit;
    }

    *PResultOffset = File->Offset;

    ExReleasePushLockExclusive(&File->Lock);

    Error = 0;

exit:
    LOG("(File=%p, Offset=%lx, Whence=%d, *PResultOffset=%lx) = %d",
        File,
        (unsigned)Offset,
        Whence,
        (unsigned)*PResultOffset,
        Error);
    return Error;
}

static INT DeviceOpen(
    PLX_CALL_CONTEXT CallContext,
    PLX_DEVICE Device,
    ULONG Flags,
    PLX_FILE *PFile)
{
    static LX_FILE_CALLBACKS FileCallbacks =
    {
        .Delete = FileDelete,
        .Flush = FileFlush,
        .Ioctl = FileIoctl,
        .Read = FileRead,
        .Write = FileWrite,
        .Seek = FileSeek,
    };
    FILE *File;
    INT Error;

    *PFile = 0;

    File = (FILE *)VfsFileAllocate(sizeof *File, &FileCallbacks);
    if (0 == File)
    {
        Error = -ENOMEM;
        goto exit;
    }

    RtlZeroMemory(File, sizeof *File);
    ExInitializePushLock(&File->Lock);
    File->Device = (DEVICE *)Device;

    *PFile = &File->Base;
    Error = 0;

exit:
    LOG("(File=%p, Flags=%lx) = %d", File, Flags, Error);
    return Error;
}

static INT DeviceDelete(
    PLX_DEVICE Device0)
{
    DEVICE *Device = (DEVICE *)Device0;
    INT Error;

    if (0 != Device->Buffer)
        ExFreePoolWithTag(Device->Buffer, POOLTAG);
    Device->Buffer = 0;

    Error = 0;

    LOG("(Device=%p) = %d", Device, Error);
    return Error;
}

static INT CreateInitialNamespace(
    PLX_INSTANCE Instance)
{
    static LX_DEVICE_CALLBACKS DeviceCallbacks =
    {
        .Open = DeviceOpen,
        .Delete = DeviceDelete,
    };
    PVOID Buffer = 0;
    DEVICE *Device = 0;
    INT Error;

    Buffer = ExAllocatePoolWithTag(NonPagedPool, BUFSIZE, POOLTAG);
    if (0 == Buffer)
    {
        Error = -ENOMEM;
        goto exit;
    }

    Device = (DEVICE *)VfsDeviceMinorAllocate(&DeviceCallbacks, sizeof *Device);
    if (0 == Device)
    {
        Error = -ENOMEM;
        goto exit;
    }

    RtlZeroMemory(Buffer, BUFSIZE);
    Device->Buffer = Buffer;
    Buffer = 0;

    LxpDevMiscRegister(Instance, &Device->Base, 0x5BABE);

    Error = 0;

exit:
    if (0 != Device)
        VfsDeviceMinorDereference(&Device->Base);

    if (0 != Buffer)
        ExFreePoolWithTag(Buffer, POOLTAG);

    LOG("(Instance=%p) = %d", Instance, Error);
    return Error;
}

NTSTATUS DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath)
{
#if DBG
    if (!KD_DEBUGGER_NOT_PRESENT)
        DbgBreakPoint();
#endif

    return LxldrRegisterService(DriverObject, TRUE, CreateInitialNamespace);
}
