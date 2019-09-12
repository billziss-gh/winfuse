/**
 * @file transact-test.c
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

#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>
#include <process.h>
#include <strsafe.h>
#include <winfuse/proto.h>

#define FUSE_FSCTL_TRANSACT             \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0xC00 + 'F', METHOD_BUFFERED, FILE_ANY_ACCESS)

static void transact_init_dotest(PWSTR DeviceName, PWSTR Prefix)
{
    FSP_FSCTL_VOLUME_PARAMS VolumeParams = { .Version = sizeof VolumeParams };
    HANDLE VolumeHandle;
    WCHAR VolumeName[MAX_PATH];
    BOOL Success;
    NTSTATUS Result;

    wcscpy_s(VolumeParams.Prefix, sizeof VolumeParams.Prefix / sizeof(WCHAR),
        L"\\winfuse-tests\\share");
    VolumeParams.FsextControlCode = FUSE_FSCTL_TRANSACT;
    Result = FspFsctlCreateVolume(DeviceName, &VolumeParams,
        VolumeName, sizeof VolumeName, &VolumeHandle);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(0 == wcsncmp(L"\\Device\\Volume{", VolumeName, 15));
    ASSERT(INVALID_HANDLE_VALUE != VolumeHandle);

    FSP_FSCTL_DECLSPEC_ALIGN UINT8 RequestBuf[FUSE_PROTO_REQ_SIZEMIN];
    FUSE_PROTO_RSP ResponseBuf;
    FUSE_PROTO_REQ *Request = (PVOID)RequestBuf;
    FUSE_PROTO_RSP *Response = &ResponseBuf;
    DWORD BytesTransferred;

    Success = DeviceIoControl(VolumeHandle, FUSE_FSCTL_TRANSACT,
        0, 0, RequestBuf, sizeof RequestBuf, &BytesTransferred, 0);
    ASSERT(Success);

    ASSERT(BytesTransferred == Request->len);
    ASSERT(FUSE_PROTO_REQ_SIZE(init) == Request->len);
    ASSERT(FUSE_PROTO_OPCODE_INIT == Request->opcode);
    ASSERT(0 != Request->unique);
    ASSERT(0 == Request->nodeid);
    ASSERT(0 == Request->uid);
    ASSERT(0 == Request->gid);
    ASSERT(0 == Request->pid);
    ASSERT(0 == Request->padding);
    ASSERT(FUSE_PROTO_VERSION == Request->req.init.major);
    ASSERT(FUSE_PROTO_MINOR_VERSION == Request->req.init.minor);
    // max_readahead
    // flags

    memset(Response, 0, FUSE_PROTO_RSP_SIZE(init));
    Response->len = FUSE_PROTO_RSP_SIZE(init);
    Response->unique = Request->unique;
    Response->rsp.init.major = Request->req.init.major;
    Response->rsp.init.minor = Request->req.init.minor;
    // max_readahead
    // flags
    // max_background
    // congestion_threshold
    // max_write
    // time_gran
    // max_pages
    // padding
    // unused

    Success = DeviceIoControl(VolumeHandle, FUSE_FSCTL_TRANSACT,
        Response, Response->len, 0, 0, &BytesTransferred, 0);
    ASSERT(Success);

    Success = CloseHandle(VolumeHandle);
    ASSERT(Success);
}

static void transact_init_test(void)
{
    transact_init_dotest(L"WinFsp.Disk", 0);
    transact_init_dotest(L"WinFsp.Net", L"\\\\winfsp-tests\\share");
}

static unsigned __stdcall transact_lookup_dotest_thread(void *FilePath)
{
    FspDebugLog(__FUNCTION__ ": \"%S\"\n", FilePath);

    HANDLE Handle;
    Handle = CreateFileW(FilePath,
        FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    if (INVALID_HANDLE_VALUE == Handle)
        return GetLastError();
    CloseHandle(Handle);
    return 0;
}

static void transact_lookup_dotest(PWSTR DeviceName, PWSTR Prefix)
{
    FSP_FSCTL_VOLUME_PARAMS VolumeParams = { .Version = sizeof VolumeParams };
    HANDLE VolumeHandle;
    WCHAR VolumeName[MAX_PATH];
    WCHAR FilePath[MAX_PATH];
    HANDLE Thread;
    DWORD ExitCode;
    BOOL Success;
    NTSTATUS Result;

    wcscpy_s(VolumeParams.Prefix, sizeof VolumeParams.Prefix / sizeof(WCHAR),
        L"\\winfuse-tests\\share");
    VolumeParams.FsextControlCode = FUSE_FSCTL_TRANSACT;
    Result = FspFsctlCreateVolume(DeviceName, &VolumeParams,
        VolumeName, sizeof VolumeName, &VolumeHandle);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(0 == wcsncmp(L"\\Device\\Volume{", VolumeName, 15));
    ASSERT(INVALID_HANDLE_VALUE != VolumeHandle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : VolumeName);
    Thread = (HANDLE)_beginthreadex(0, 0, transact_lookup_dotest_thread, FilePath, 0, 0);
    ASSERT(0 != Thread);

    Sleep(1000); /* give some time to the thread to execute */

    FSP_FSCTL_DECLSPEC_ALIGN UINT8 RequestBuf[FUSE_PROTO_REQ_SIZEMIN];
    FUSE_PROTO_RSP ResponseBuf;
    FUSE_PROTO_REQ *Request = (PVOID)RequestBuf;
    FUSE_PROTO_RSP *Response = &ResponseBuf;
    DWORD BytesTransferred;

    Success = DeviceIoControl(VolumeHandle, FUSE_FSCTL_TRANSACT,
        0, 0, RequestBuf, sizeof RequestBuf, &BytesTransferred, 0);
    ASSERT(Success);

    ASSERT(BytesTransferred == Request->len);
    ASSERT(FUSE_PROTO_REQ_SIZE(init) == Request->len);
    ASSERT(FUSE_PROTO_OPCODE_INIT == Request->opcode);
    ASSERT(0 != Request->unique);
    ASSERT(0 == Request->nodeid);
    ASSERT(0 == Request->uid);
    ASSERT(0 == Request->gid);
    ASSERT(0 == Request->pid);
    ASSERT(0 == Request->padding);
    ASSERT(FUSE_PROTO_VERSION == Request->req.init.major);
    ASSERT(FUSE_PROTO_MINOR_VERSION == Request->req.init.minor);
    // max_readahead
    // flags

    memset(Response, 0, FUSE_PROTO_RSP_SIZE(init));
    Response->len = FUSE_PROTO_RSP_SIZE(init);
    Response->unique = Request->unique;
    Response->rsp.init.major = Request->req.init.major;
    Response->rsp.init.minor = Request->req.init.minor;
    // max_readahead
    // flags
    // max_background
    // congestion_threshold
    // max_write
    // time_gran
    // max_pages
    // padding
    // unused

    Success = DeviceIoControl(VolumeHandle, FUSE_FSCTL_TRANSACT,
        Response, Response->len, RequestBuf, sizeof RequestBuf, &BytesTransferred, 0);
    ASSERT(Success);

    while (0 == BytesTransferred)
    {
        Success = DeviceIoControl(VolumeHandle, FUSE_FSCTL_TRANSACT,
            0, 0, RequestBuf, sizeof RequestBuf, &BytesTransferred, 0);
        ASSERT(Success);
    }

    if (FUSE_PROTO_OPCODE_STATFS == Request->opcode)
    {
        ASSERT(BytesTransferred == Request->len);
        ASSERT(FUSE_PROTO_REQ_HEADER_SIZE == Request->len);
        ASSERT(FUSE_PROTO_OPCODE_STATFS == Request->opcode);
        ASSERT(0 != Request->unique);
        ASSERT(0 == Request->nodeid);
        ASSERT(0 == Request->uid);
        ASSERT(0 == Request->gid);
        ASSERT(0 == Request->pid);
        ASSERT(0 == Request->padding);

        memset(Response, 0, FUSE_PROTO_RSP_SIZE(statfs));
        Response->len = FUSE_PROTO_RSP_SIZE(statfs);
        Response->unique = Request->unique;
        Response->rsp.statfs.st.blocks = 1000;
        Response->rsp.statfs.st.bfree = 1000;
        Response->rsp.statfs.st.frsize = 4096;

        Success = DeviceIoControl(VolumeHandle, FUSE_FSCTL_TRANSACT,
            Response, Response->len, RequestBuf, sizeof RequestBuf, &BytesTransferred, 0);
        ASSERT(Success);

        while (0 == BytesTransferred)
        {
            Success = DeviceIoControl(VolumeHandle, FUSE_FSCTL_TRANSACT,
                0, 0, RequestBuf, sizeof RequestBuf, &BytesTransferred, 0);
            ASSERT(Success);
        }
    }

    ASSERT(BytesTransferred == Request->len);
    ASSERT(FUSE_PROTO_REQ_SIZE(getattr) == Request->len);
    ASSERT(FUSE_PROTO_OPCODE_GETATTR == Request->opcode);
    ASSERT(0 != Request->unique);
    ASSERT(FUSE_PROTO_ROOT_ID == Request->nodeid);
    ASSERT(0 != Request->uid);
    ASSERT(0 != Request->gid);
    ASSERT(0 != Request->pid);
    ASSERT(0 == Request->padding);
    ASSERT(0 == Request->req.getattr.getattr_flags);
    ASSERT(0 == Request->req.getattr.fh);

    memset(Response, 0, FUSE_PROTO_RSP_SIZE(getattr));
    Response->len = FUSE_PROTO_RSP_SIZE(getattr);
    Response->unique = Request->unique;
    Response->rsp.getattr.attr.ino = FUSE_PROTO_ROOT_ID;
    Response->rsp.getattr.attr.mode = 0040777;
    Response->rsp.getattr.attr.nlink = 1;
    Response->rsp.getattr.attr.uid = Request->uid;
    Response->rsp.getattr.attr.gid = Request->gid;

    Success = DeviceIoControl(VolumeHandle, FUSE_FSCTL_TRANSACT,
        Response, Response->len, 0, 0, &BytesTransferred, 0);
    ASSERT(Success);

    Success = CloseHandle(VolumeHandle);
    ASSERT(Success);

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);

    ASSERT(ERROR_OPERATION_ABORTED == ExitCode);
}

static void transact_lookup_test(void)
{
    transact_lookup_dotest(L"WinFsp.Disk", 0);
#if 0
    /*
     * This test fails because MUP appears to not be initialized
     * when transact_lookup_dotest_thread executes.
     */
    transact_lookup_dotest(L"WinFsp.Net", L"\\\\winfsp-tests\\share");
#endif
}

static HANDLE transact_open_close_dotest_VolumeHandle;

static unsigned __stdcall transact_open_close_dotest_thread(void *FilePath)
{
    FspDebugLog(__FUNCTION__ ": \"%S\"\n", FilePath);

    HANDLE Handle;
    Handle = CreateFileW(FilePath,
        FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (INVALID_HANDLE_VALUE == Handle)
        return GetLastError();
    if (INVALID_HANDLE_VALUE != transact_open_close_dotest_VolumeHandle)
    {
        Sleep(300);
        CloseHandle(transact_open_close_dotest_VolumeHandle);
    }
    CloseHandle(Handle);
    return 0;
}

static void transact_open_close_dotest(PWSTR DeviceName, PWSTR Prefix, BOOLEAN Abandon)
{
    FSP_FSCTL_VOLUME_PARAMS VolumeParams = { .Version = sizeof VolumeParams };
    HANDLE VolumeHandle;
    WCHAR VolumeName[MAX_PATH];
    WCHAR FilePath[MAX_PATH];
    HANDLE Thread;
    DWORD ExitCode;
    BOOL Success;
    NTSTATUS Result;

    wcscpy_s(VolumeParams.Prefix, sizeof VolumeParams.Prefix / sizeof(WCHAR),
        L"\\winfuse-tests\\share");
    VolumeParams.FsextControlCode = FUSE_FSCTL_TRANSACT;
    Result = FspFsctlCreateVolume(DeviceName, &VolumeParams,
        VolumeName, sizeof VolumeName, &VolumeHandle);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(0 == wcsncmp(L"\\Device\\Volume{", VolumeName, 15));
    ASSERT(INVALID_HANDLE_VALUE != VolumeHandle);

    transact_open_close_dotest_VolumeHandle = Abandon ? VolumeHandle : INVALID_HANDLE_VALUE;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : VolumeName);
    Thread = (HANDLE)_beginthreadex(0, 0, transact_open_close_dotest_thread, FilePath, 0, 0);
    ASSERT(0 != Thread);

    Sleep(1000); /* give some time to the thread to execute */

    FSP_FSCTL_DECLSPEC_ALIGN UINT8 RequestBuf[FUSE_PROTO_REQ_SIZEMIN];
    FUSE_PROTO_RSP ResponseBuf;
    FUSE_PROTO_REQ *Request = (PVOID)RequestBuf;
    FUSE_PROTO_RSP *Response = &ResponseBuf;
    DWORD BytesTransferred;

    for (BOOLEAN Loop = TRUE; Loop;)
    {
        do
        {
            Success = DeviceIoControl(VolumeHandle, FUSE_FSCTL_TRANSACT,
                0, 0, RequestBuf, sizeof RequestBuf, &BytesTransferred, 0);
            ASSERT(Success || ERROR_INVALID_HANDLE == GetLastError() || ERROR_OPERATION_ABORTED == GetLastError());
            if (!Success && (ERROR_INVALID_HANDLE == GetLastError() || ERROR_OPERATION_ABORTED == GetLastError()))
                goto loopexit;
        } while (0 == BytesTransferred);

        ASSERT(FUSE_PROTO_REQ_HEADER_SIZE <= BytesTransferred);
        ASSERT(Request->len == BytesTransferred);

        Response->len = 0;
        switch (Request->opcode)
        {
        case FUSE_PROTO_OPCODE_INIT:
            ASSERT(FUSE_PROTO_REQ_SIZE(init) == Request->len);
            ASSERT(FUSE_PROTO_OPCODE_INIT == Request->opcode);
            ASSERT(0 != Request->unique);
            ASSERT(0 == Request->nodeid);
            ASSERT(0 == Request->uid);
            ASSERT(0 == Request->gid);
            ASSERT(0 == Request->pid);
            ASSERT(0 == Request->padding);
            ASSERT(FUSE_PROTO_VERSION == Request->req.init.major);
            ASSERT(FUSE_PROTO_MINOR_VERSION == Request->req.init.minor);
            // max_readahead
            // flags

            memset(Response, 0, FUSE_PROTO_RSP_SIZE(init));
            Response->len = FUSE_PROTO_RSP_SIZE(init);
            Response->unique = Request->unique;
            Response->rsp.init.major = Request->req.init.major;
            Response->rsp.init.minor = Request->req.init.minor;
            // max_readahead
            // flags
            // max_background
            // congestion_threshold
            // max_write
            // time_gran
            // max_pages
            // padding
            // unused
            break;

        case FUSE_PROTO_OPCODE_STATFS:
            ASSERT(FUSE_PROTO_REQ_HEADER_SIZE == Request->len);
            ASSERT(FUSE_PROTO_OPCODE_STATFS == Request->opcode);
            ASSERT(0 != Request->unique);
            ASSERT(0 == Request->nodeid);
            ASSERT(0 == Request->uid);
            ASSERT(0 == Request->gid);
            ASSERT(0 == Request->pid);
            ASSERT(0 == Request->padding);

            memset(Response, 0, FUSE_PROTO_RSP_SIZE(statfs));
            Response->len = FUSE_PROTO_RSP_SIZE(statfs);
            Response->unique = Request->unique;
            Response->rsp.statfs.st.blocks = 1000;
            Response->rsp.statfs.st.bfree = 1000;
            Response->rsp.statfs.st.frsize = 4096;
            break;

        case FUSE_PROTO_OPCODE_GETATTR:
            ASSERT(FUSE_PROTO_REQ_SIZE(getattr) == Request->len);
            ASSERT(FUSE_PROTO_OPCODE_GETATTR == Request->opcode);
            ASSERT(0 != Request->unique);
            ASSERT(FUSE_PROTO_ROOT_ID == Request->nodeid);
            ASSERT(0 != Request->uid);
            ASSERT(0 != Request->gid);
            ASSERT(0 != Request->pid);
            ASSERT(0 == Request->padding);
            ASSERT(0 == Request->req.getattr.getattr_flags);
            ASSERT(0 == Request->req.getattr.fh);

            memset(Response, 0, FUSE_PROTO_RSP_SIZE(getattr));
            Response->len = FUSE_PROTO_RSP_SIZE(getattr);
            Response->unique = Request->unique;
            Response->rsp.getattr.attr.ino = FUSE_PROTO_ROOT_ID;
            Response->rsp.getattr.attr.mode = 0040777;
            Response->rsp.getattr.attr.nlink = 1;
            Response->rsp.getattr.attr.uid = Request->uid;
            Response->rsp.getattr.attr.gid = Request->gid;
            break;

        case FUSE_PROTO_OPCODE_LOOKUP:
            ASSERT(FUSE_PROTO_REQ_SIZE(lookup) + sizeof "file0" == Request->len);
            ASSERT(FUSE_PROTO_OPCODE_LOOKUP == Request->opcode);
            ASSERT(0 != Request->unique);
            ASSERT(FUSE_PROTO_ROOT_ID == Request->nodeid);
            ASSERT(0 != Request->uid);
            ASSERT(0 != Request->gid);
            ASSERT(0 != Request->pid);
            ASSERT(0 == Request->padding);
            ASSERT(0 == strcmp("file0", Request->req.lookup.name));

            memset(Response, 0, FUSE_PROTO_RSP_SIZE(lookup));
            Response->len = FUSE_PROTO_RSP_SIZE(lookup);
            Response->unique = Request->unique;
            Response->rsp.lookup.entry.nodeid = FUSE_PROTO_ROOT_ID + 1;
            Response->rsp.lookup.entry.attr.ino = FUSE_PROTO_ROOT_ID + 1;
            Response->rsp.lookup.entry.attr.mode = 0100777;
            Response->rsp.lookup.entry.attr.nlink = 1;
            Response->rsp.lookup.entry.attr.uid = Request->uid;
            Response->rsp.lookup.entry.attr.gid = Request->gid;
            break;

        case FUSE_PROTO_OPCODE_OPEN:
            ASSERT(FUSE_PROTO_REQ_SIZE(open) == Request->len);
            ASSERT(FUSE_PROTO_OPCODE_OPEN == Request->opcode);
            ASSERT(0 != Request->unique);
            ASSERT(FUSE_PROTO_ROOT_ID + 1 == Request->nodeid);
            ASSERT(0 != Request->uid);
            ASSERT(0 != Request->gid);
            ASSERT(0 != Request->pid);
            ASSERT(0 == Request->padding);
            ASSERT(0 == Request->req.open.flags);
            ASSERT(0 == Request->req.open.unused);

            memset(Response, 0, FUSE_PROTO_RSP_SIZE(open));
            Response->len = FUSE_PROTO_RSP_SIZE(open);
            Response->unique = Request->unique;
            Response->rsp.open.fh = 42;
            break;

        case FUSE_PROTO_OPCODE_RELEASE:
            ASSERT(FUSE_PROTO_REQ_SIZE(release) == Request->len);
            ASSERT(FUSE_PROTO_OPCODE_RELEASE == Request->opcode);
            ASSERT(0 != Request->unique);
            ASSERT(0 == Request->nodeid);
            ASSERT(0 == Request->uid);
            ASSERT(0 == Request->gid);
            ASSERT(0 == Request->pid);
            ASSERT(0 == Request->padding);
            ASSERT(42 == Request->req.release.fh);
            ASSERT(0 == Request->req.release.flags);
            ASSERT(0 == Request->req.release.release_flags);
            ASSERT(0 == Request->req.release.lock_owner);

            memset(Response, 0, FUSE_PROTO_RSP_HEADER_SIZE);
            Response->len = FUSE_PROTO_RSP_HEADER_SIZE;
            Response->unique = Request->unique;

            Loop = FALSE;
            break;
        }

        Success = DeviceIoControl(VolumeHandle, FUSE_FSCTL_TRANSACT,
            Response, Response->len, 0, 0, &BytesTransferred, 0);
        ASSERT(Success || ERROR_INVALID_HANDLE == GetLastError() || ERROR_OPERATION_ABORTED == GetLastError());
        if (!Success && (ERROR_INVALID_HANDLE == GetLastError() || ERROR_OPERATION_ABORTED == GetLastError()))
            goto loopexit;

        ASSERT(0 == BytesTransferred);
    }
loopexit:

    if (!Abandon)
    {
        Success = CloseHandle(VolumeHandle);
        ASSERT(Success);
    }

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);

    ASSERT(0 == ExitCode);
}

static void transact_open_close_test(void)
{
    transact_open_close_dotest(L"WinFsp.Disk", 0, FALSE);
#if 0
    /*
     * This test fails because MUP appears to not be initialized
     * when transact_lookup_dotest_thread executes.
     */
    transact_open_close_dotest(L"WinFsp.Net", L"\\\\winfsp-tests\\share", FALSE);
#endif
}

static void transact_open_abandon_test(void)
{
    transact_open_close_dotest(L"WinFsp.Disk", 0, TRUE);
#if 0
    /*
     * This test fails because MUP appears to not be initialized
     * when transact_lookup_dotest_thread executes.
     */
    transact_open_close_dotest(L"WinFsp.Net", L"\\\\winfsp-tests\\share", TRUE);
#endif
}

void transact_tests(void)
{
    TEST(transact_init_test);
    TEST(transact_lookup_test);
    TEST(transact_open_close_test);
    TEST(transact_open_abandon_test);
}
