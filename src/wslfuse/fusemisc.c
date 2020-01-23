/**
 * @file wslfuse/fusemisc.c
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

typedef struct
{
    LX_DEVICE Base;
} DEVICE;

typedef struct
{
    LX_FILE Base;
    DEVICE *Device;
} FILE;

static INT FileIoctl(
    PLX_CALL_CONTEXT CallContext,
    PLX_FILE File0,
    ULONG Code,
    PVOID Buffer)
{
    //FILE *File = (FILE *)File0;
    INT Error;

    switch (Code)
    {
    case WSLFUSE_MOUNT:
        break;

    case WSLFUSE_UNMOUNT:
        break;

    default:
        Error = -EINVAL;
        goto exit;
    }

    Error = 0;

exit:
    return Error;
}

static INT FileRead(
    PLX_CALL_CONTEXT CallContext,
    PLX_FILE File,
    PVOID Buffer,
    SIZE_T Length,
    POFF_T POffset,
    PSIZE_T PBytesTransferred)
{
    return 0;
}

static INT FileWrite(
    PLX_CALL_CONTEXT CallContext,
    PLX_FILE File,
    PVOID Buffer,
    SIZE_T Length,
    POFF_T POffset,
    PSIZE_T PBytesTransferred)
{
    return 0;
}

static INT DeviceOpen(
    PLX_CALL_CONTEXT CallContext,
    PLX_DEVICE Device,
    ULONG Flags,
    PLX_FILE *PFile)
{
    static LX_FILE_CALLBACKS FileCallbacks =
    {
        .Ioctl = FileIoctl,
        .Read = FileRead,
        .Write = FileWrite,
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

    /* init fields */
    RtlZeroMemory(File, sizeof *File);
    File->Device = (DEVICE *)Device;

    *PFile = &File->Base;
    Error = 0;

exit:
    return Error;
}

static INT DeviceDelete(
    PLX_DEVICE Device)
{
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

    /* init fields other than Device->Base */
    // ...

    LxpDevMiscRegister(Instance, &Device->Base, FUSE_MINOR);

    Error = 0;

exit:
    if (0 != Device)
        VfsDeviceMinorDereference(&Device->Base);

    return Error;
}
