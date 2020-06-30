/**
 * @file fusermount-helper.c
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

#include <windows.h>
#include <winternl.h>

#define PROGNAME                        "fusermount-helper"

#undef RtlFillMemory
#undef RtlMoveMemory
NTSYSAPI VOID NTAPI RtlFillMemory(VOID *Destination, DWORD Length, BYTE Fill);
NTSYSAPI VOID NTAPI RtlMoveMemory(VOID *Destination, CONST VOID *Source, DWORD Length);

#pragma function(memset)
#pragma function(memcpy)
static inline
void *memset(void *dst, int val, size_t siz)
{
    RtlFillMemory(dst, (DWORD)siz, val);
    return dst;
}
static inline
void *memcpy(void *dst, const void *src, size_t siz)
{
    RtlMoveMemory(dst, src, (DWORD)siz);
    return dst;
}

#define warn(format, ...)               \
    printlog(GetStdHandle(STD_ERROR_HANDLE), PROGNAME ": " format, __VA_ARGS__)
#define fatal(ExitCode, format, ...)    (warn(format, __VA_ARGS__), ExitProcess(ExitCode))

static void vprintlog(HANDLE h, const char *format, va_list ap)
{
    char buf[1024];
        /* wvsprintf is only safe with a 1024 byte buffer */
    size_t len;
    DWORD BytesTransferred;

    wvsprintfA(buf, format, ap);
    buf[sizeof buf - 1] = '\0';

    len = lstrlenA(buf);
    buf[len++] = '\n';

    WriteFile(h, buf, (DWORD)len, &BytesTransferred, 0);
}

static void printlog(HANDLE h, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vprintlog(h, format, ap);
    va_end(ap);
}

NTSTATUS NTAPI NtOpenSymbolicLinkObject(PHANDLE LinkHandle,
    ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes);
NTSTATUS NTAPI NtMakeTemporaryObject(HANDLE Handle);
NTSTATUS NTAPI NtClose(HANDLE Handle);

static BOOL MountSet_Drive(PWSTR VolumeName, PWSTR MountPoint, PHANDLE PMountHandle)
{
    WCHAR SymlinkBuf[6];
    UNICODE_STRING Symlink;
    OBJECT_ATTRIBUTES Obja;
    NTSTATUS Result;

    *PMountHandle = 0;

    if (!DefineDosDeviceW(DDD_RAW_TARGET_PATH, MountPoint, VolumeName))
        return FALSE;

    memcpy(SymlinkBuf, L"\\??\\X:", sizeof SymlinkBuf);
    SymlinkBuf[4] = MountPoint[0];
    Symlink.Length = Symlink.MaximumLength = sizeof SymlinkBuf;
    Symlink.Buffer = SymlinkBuf;

    memset(&Obja, 0, sizeof Obja);
    Obja.Length = sizeof Obja;
    Obja.ObjectName = &Symlink;
    Obja.Attributes = OBJ_CASE_INSENSITIVE;

    Result = NtOpenSymbolicLinkObject(PMountHandle, DELETE, &Obja);
    if (NT_SUCCESS(Result))
    {
        Result = NtMakeTemporaryObject(*PMountHandle);
        if (!NT_SUCCESS(Result))
        {
            NtClose(*PMountHandle);
            *PMountHandle = 0;
        }
    }

    return TRUE;
}

static BOOL MountSet(PWSTR VolumeName, PWSTR MountPoint, PHANDLE PMountHandle)
{
    *PMountHandle = 0;

    if (L'*' == MountPoint[0] && ':' == MountPoint[1] && L'\0' == MountPoint[2])
    {
        DWORD Drives;
        WCHAR Drive;

        Drives = GetLogicalDrives();
        if (0 == Drives)
            return FALSE;

        for (Drive = 'Z'; 'D' <= Drive; Drive--)
            if (0 == (Drives & (1 << (Drive - 'A'))))
            {
                MountPoint[0] = Drive;
                if (MountSet_Drive(VolumeName, MountPoint, PMountHandle))
                    return TRUE;
            }
        MountPoint[0] = L'*';
        SetLastError(ERROR_NO_SUCH_DEVICE);
            /* error code chosen for WinFsp compatibility (FspMountSet) */
        return FALSE;
    }
    else if (
        (
            (L'A' <= MountPoint[0] && MountPoint[0] <= L'Z') ||
            (L'a' <= MountPoint[0] && MountPoint[0] <= L'z')
        ) && L':' == MountPoint[1] && L'\0' == MountPoint[2])
        return MountSet_Drive(VolumeName, MountPoint, PMountHandle);
    else
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
}

int wmain(int argc, wchar_t **argv)
{
    PWSTR VolumeName;
    PWSTR MountPoint;
    HANDLE MountHandle;
    WCHAR WildMountPoint[] = L"*:";
    UINT8 MountPointU8[MAX_PATH];
    UINT8 Buffer[1];
    DWORD BytesTransferred;

    if (2 > argc || 3 < argc)
        fatal(2, "usage: VolumeName [WinMountPoint]");

    VolumeName = argv[1];
    MountPoint = 2 < argc ? argv[2] : WildMountPoint;

    if (!MountSet(VolumeName, MountPoint, &MountHandle))
        fatal(1, "cannot set Windows mount point %S (LastError=%lu)", MountPoint, GetLastError());

    if (0 == WideCharToMultiByte(CP_UTF8, 0, MountPoint, -1, MountPointU8, MAX_PATH, 0, 0))
        fatal(1, "invalid Windows mount point (LastError=%lu)", GetLastError());

    BytesTransferred = lstrlenA(MountPointU8) + 1;
    if (!WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), MountPointU8, BytesTransferred, &BytesTransferred, 0))
        fatal(1, "cannot write to stdout (LastError=%lu)", GetLastError());

    if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), Buffer, sizeof Buffer, &BytesTransferred, 0))
        fatal(1, "cannot read from stdin (LastError=%lu)", GetLastError());

    return 0;
}

void wmainCRTStartup(void)
{
    DWORD Argc;
    PWSTR *Argv;

    Argv = CommandLineToArgvW(GetCommandLineW(), &Argc);
    if (0 == Argv)
        ExitProcess(GetLastError());

    ExitProcess(wmain(Argc, Argv));
}
