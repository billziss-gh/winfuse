/**
 * @file transact-test.c
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
#include <tlib/testsuite.h>
#include <process.h>
#include <strsafe.h>
#include <shared/km/proto.h>

#define FUSE_FSCTL_TRANSACT             \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0xC00 + 'F', METHOD_BUFFERED, FILE_ANY_ACCESS)

static void transact_init_dotest(PWSTR DeviceName, PWSTR Prefix)
{
    FSP_FSCTL_VOLUME_PARAMS VolumeParams = { .Version = sizeof VolumeParams };
    HANDLE VolumeHandle;
    WCHAR VolumeName[MAX_PATH];
    BOOL Success;
    NTSTATUS Result;

    if (0 != Prefix && L'\\' == Prefix[0] && L'\\' == Prefix[1])
        wcscpy_s(VolumeParams.Prefix, sizeof VolumeParams.Prefix / sizeof(WCHAR),
            Prefix + 1);
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
    transact_init_dotest(L"WinFsp.Net", L"\\\\winfuse-tests\\share");
}

static HANDLE transact_open_close_dotest_VolumeHandle;
static HANDLE transact_open_close_dotest_MainThread;

static unsigned __stdcall transact_open_close_dotest_thread(void *FilePath)
{
    FspDebugLog(__FUNCTION__ ": \"%S\"\n", FilePath);

    HANDLE Handle;
    Handle = CreateFileW(FilePath,
        FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (INVALID_HANDLE_VALUE == Handle)
        return GetLastError();
    if (INVALID_HANDLE_VALUE != transact_open_close_dotest_VolumeHandle)
    {
        Sleep(300);
        CloseHandle(transact_open_close_dotest_VolumeHandle);
    }
    if (0 != transact_open_close_dotest_MainThread)
    {
        Sleep(300);
        if (!CancelSynchronousIo(transact_open_close_dotest_MainThread))
        {
            DWORD LastError = GetLastError();
            CloseHandle(Handle);
            return LastError;
        }
    }
    CloseHandle(Handle);
    return 0;
}

static void transact_open_close_dotest(PWSTR DeviceName, PWSTR Prefix, int Scenario)
{
    FSP_FSCTL_VOLUME_PARAMS VolumeParams = { .Version = sizeof VolumeParams };
    HANDLE VolumeHandle;
    WCHAR VolumeName[MAX_PATH];
    WCHAR FilePath[MAX_PATH];
    HANDLE Thread, MainThread = 0;
    DWORD ExitCode;
    BOOL Success;
    NTSTATUS Result;

    if (0 != Prefix && L'\\' == Prefix[0] && L'\\' == Prefix[1])
        wcscpy_s(VolumeParams.Prefix, sizeof VolumeParams.Prefix / sizeof(WCHAR),
            Prefix + 1);
    VolumeParams.FsextControlCode = FUSE_FSCTL_TRANSACT;
    Result = FspFsctlCreateVolume(DeviceName, &VolumeParams,
        VolumeName, sizeof VolumeName, &VolumeHandle);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(0 == wcsncmp(L"\\Device\\Volume{", VolumeName, 15));
    ASSERT(INVALID_HANDLE_VALUE != VolumeHandle);

    transact_open_close_dotest_VolumeHandle = 'CLOS' == Scenario ? VolumeHandle : INVALID_HANDLE_VALUE;
    transact_open_close_dotest_MainThread = 'CNCL' == Scenario &&
        DuplicateHandle(
            GetCurrentProcess(),
            GetCurrentThread(),
            GetCurrentProcess(),
            &MainThread,
            0,
            FALSE,
            DUPLICATE_SAME_ACCESS) ?
        MainThread : 0;

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
            ASSERT(FUSE_PROTO_ROOT_INO == Request->nodeid || FUSE_PROTO_ROOT_INO + 1 == Request->nodeid);
            ASSERT(0 == Request->padding);
            ASSERT(0 == Request->req.getattr.getattr_flags);
            ASSERT(0 == Request->req.getattr.fh);

            memset(Response, 0, FUSE_PROTO_RSP_SIZE(getattr));
            Response->len = FUSE_PROTO_RSP_SIZE(getattr);
            Response->unique = Request->unique;
            Response->rsp.getattr.attr.ino = Request->nodeid;
            Response->rsp.getattr.attr.mode = 0040777;
            Response->rsp.getattr.attr.nlink = 1;
            Response->rsp.getattr.attr.uid = Request->uid;
            Response->rsp.getattr.attr.gid = Request->gid;
            break;

        case FUSE_PROTO_OPCODE_LOOKUP:
            ASSERT(FUSE_PROTO_REQ_SIZE(lookup) + sizeof "file0" == Request->len);
            ASSERT(FUSE_PROTO_OPCODE_LOOKUP == Request->opcode);
            ASSERT(0 != Request->unique);
            ASSERT(FUSE_PROTO_ROOT_INO == Request->nodeid);
            ASSERT(0 != Request->uid);
            ASSERT(0 != Request->gid);
            ASSERT(0 != Request->pid);
            ASSERT(0 == Request->padding);
            ASSERT(0 == strcmp("file0", Request->req.lookup.name));

            memset(Response, 0, FUSE_PROTO_RSP_SIZE(lookup));
            Response->len = FUSE_PROTO_RSP_SIZE(lookup);
            Response->unique = Request->unique;
            Response->rsp.lookup.entry.nodeid = FUSE_PROTO_ROOT_INO + 1;
            Response->rsp.lookup.entry.attr.ino = FUSE_PROTO_ROOT_INO + 1;
            Response->rsp.lookup.entry.attr.mode = 0040777;
            Response->rsp.lookup.entry.attr.nlink = 1;
            Response->rsp.lookup.entry.attr.uid = Request->uid;
            Response->rsp.lookup.entry.attr.gid = Request->gid;
            break;

        case FUSE_PROTO_OPCODE_FORGET:
        case FUSE_PROTO_OPCODE_BATCH_FORGET:
            continue;

        case FUSE_PROTO_OPCODE_OPENDIR:
        case FUSE_PROTO_OPCODE_OPEN:
            ASSERT(FUSE_PROTO_REQ_SIZE(open) == Request->len);
            ASSERT(FUSE_PROTO_OPCODE_OPENDIR == Request->opcode || FUSE_PROTO_OPCODE_OPEN == Request->opcode);
            ASSERT(0 != Request->unique);
            ASSERT(FUSE_PROTO_ROOT_INO == Request->nodeid || FUSE_PROTO_ROOT_INO + 1 == Request->nodeid);
            ASSERT(0 != Request->uid);
            ASSERT(0 != Request->gid);
            ASSERT(0 != Request->pid);
            ASSERT(0 == Request->padding);
            ASSERT(0 == Request->req.open.flags);
            ASSERT(0 == Request->req.open.unused);

            memset(Response, 0, FUSE_PROTO_RSP_SIZE(open));
            Response->len = FUSE_PROTO_RSP_SIZE(open);
            Response->unique = Request->unique;
            Response->rsp.open.fh = 100 + Request->nodeid;
            break;

        case FUSE_PROTO_OPCODE_RELEASEDIR:
        case FUSE_PROTO_OPCODE_RELEASE:
            ASSERT(FUSE_PROTO_REQ_SIZE(release) == Request->len);
            ASSERT(FUSE_PROTO_OPCODE_RELEASEDIR == Request->opcode || FUSE_PROTO_OPCODE_RELEASE == Request->opcode);
            ASSERT(0 != Request->unique);
            ASSERT(FUSE_PROTO_ROOT_INO == Request->nodeid || FUSE_PROTO_ROOT_INO + 1 == Request->nodeid);
            ASSERT(0 == Request->uid);
            ASSERT(0 == Request->gid);
            ASSERT(0 == Request->pid);
            ASSERT(0 == Request->padding);
            ASSERT(
                100 + FUSE_PROTO_ROOT_INO == Request->req.release.fh ||
                100 + FUSE_PROTO_ROOT_INO + 1 == Request->req.release.fh);
            ASSERT(0 == Request->req.release.flags);
            ASSERT(0 == Request->req.release.release_flags);
            ASSERT(0 == Request->req.release.lock_owner);

            memset(Response, 0, FUSE_PROTO_RSP_HEADER_SIZE);
            Response->len = FUSE_PROTO_RSP_HEADER_SIZE;
            Response->unique = Request->unique;

            if ('BOGU' == Scenario)
                Response->unique = Response->unique ^ rand();

            if (100 + FUSE_PROTO_ROOT_INO + 1 == Request->req.release.fh)
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

    if ('CLOS' != Scenario)
    {
        Success = CloseHandle(VolumeHandle);
        ASSERT(Success);
    }
    if ('CNCL' == Scenario)
    {
        Success = CloseHandle(MainThread);
        ASSERT(Success);
    }

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);

    ASSERT(0 == ExitCode);
}

static void transact_open_close_test(void)
{
    transact_open_close_dotest(L"WinFsp.Disk", 0, 0);
    transact_open_close_dotest(L"WinFsp.Net", L"\\\\winfuse-tests\\share", 0);
}

static void transact_open_abandon_test(void)
{
    transact_open_close_dotest(L"WinFsp.Disk", 0, 'CLOS');
    transact_open_close_dotest(L"WinFsp.Net", L"\\\\winfuse-tests\\share", 'CLOS');
}

static void transact_open_cancel_test(void)
{
    transact_open_close_dotest(L"WinFsp.Disk", 0, 'CNCL');
    transact_open_close_dotest(L"WinFsp.Net", L"\\\\winfuse-tests\\share", 'CNCL');
}

static void transact_open_bogus_test(void)
{
    transact_open_close_dotest(L"WinFsp.Disk", 0, 'BOGU');
    transact_open_close_dotest(L"WinFsp.Net", L"\\\\winfuse-tests\\share", 'BOGU');
}

void transact_tests(void)
{
    TEST(transact_init_test);
    TEST(transact_open_close_test);
    TEST(transact_open_abandon_test);
    TEST(transact_open_cancel_test);
    TEST(transact_open_bogus_test);
}
