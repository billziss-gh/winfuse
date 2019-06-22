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

static void transact_dotest(PWSTR DeviceName, PWSTR Prefix)
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

    CloseHandle(VolumeHandle);
}

static void transact_test(void)
{
    transact_dotest(L"WinFsp.Disk", 0);
    transact_dotest(L"WinFsp.Net", L"\\\\winfsp-tests\\share");
}

void transact_tests(void)
{
    TEST(transact_test);
}
