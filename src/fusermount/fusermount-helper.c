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

#include <winfsp/winfsp.h>

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

int wmain(int argc, wchar_t **argv)
{
    FSP_MOUNT_DESC Desc;
    WCHAR WildMountPoint[] = L"*:";
    UINT8 MountPointU8[MAX_PATH];
    UINT8 Buffer[1];
    DWORD BytesTransferred;
    NTSTATUS Result;

    if (!NT_SUCCESS(FspLoad(0)))
        fatal(1, "cannot find winfsp");

    if (2 > argc || 3 < argc)
        fatal(2, "usage: VolumeName [WinMountPoint]");

    memset(&Desc, 0, sizeof Desc);
    Desc.VolumeName = argv[1];
    Desc.MountPoint = 2 < argc ? argv[2] : 0;

    if (0 == Desc.MountPoint)
        Desc.MountPoint = WildMountPoint;

    Result = FspMountSet(&Desc);
    if (!NT_SUCCESS(Result))
        fatal(1, "cannot set Windows mount point %S (Status=%lx)", Desc.MountPoint, Result);

    if (0 == WideCharToMultiByte(CP_UTF8, 0, Desc.MountPoint, -1, MountPointU8, MAX_PATH, 0, 0))
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
