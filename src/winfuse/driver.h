/**
 * @file winfuse/driver.h
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

#ifndef WINFUSE_DRIVER_H_INCLUDED
#define WINFUSE_DRIVER_H_INCLUDED

#include <ntifs.h>
#include <winfsp/fsext.h>

/* disable warnings */
#pragma warning(disable:4100)           /* unreferenced formal parameter */
#pragma warning(disable:4200)           /* zero-sized array in struct/union */

#include <winfuse/coro.h>
#include <winfuse/proto.h>

#define FUSE_FSCTL_TRANSACT             \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0xC00 + 'F', METHOD_BUFFERED, FILE_ANY_ACCESS)

/* device management */
typedef struct _FUSE_DEVICE_EXTENSION
{
    FSP_FSCTL_VOLUME_PARAMS *VolumeParams;
    PVOID Ioq;
    PVOID Cache;
    KEVENT InitEvent;
    UINT32 VersionMajor, VersionMinor;
} FUSE_DEVICE_EXTENSION;
NTSTATUS FuseDeviceInit(PDEVICE_OBJECT DeviceObject, FSP_FSCTL_VOLUME_PARAMS *VolumeParams);
VOID FuseDeviceFini(PDEVICE_OBJECT DeviceObject);
VOID FuseDeviceExpirationRoutine(PDEVICE_OBJECT DeviceObject, UINT64 ExpirationTime);
NTSTATUS FuseDeviceTransact(PDEVICE_OBJECT DeviceObject, PIRP Irp);
extern FSP_FSEXT_PROVIDER FuseProvider;
static inline
FUSE_DEVICE_EXTENSION *FuseDeviceExtension(PDEVICE_OBJECT DeviceObject)
{
    return (PVOID)((PUINT8)DeviceObject->DeviceExtension + FuseProvider.DeviceExtensionOffset);
}

/* FUSE processing context */
#pragma warning(push)
#pragma warning(disable:4201)           /* nameless struct/union */
typedef struct _FUSE_CONTEXT FUSE_CONTEXT;
typedef VOID FUSE_CONTEXT_FINI(FUSE_CONTEXT *Context);
typedef BOOLEAN FUSE_PROCESS_DISPATCH(FUSE_CONTEXT *Context);
struct _FUSE_CONTEXT
{
    FUSE_CONTEXT *DictNext;
    LIST_ENTRY ListEntry;
    FUSE_CONTEXT_FINI *Fini;
    PDEVICE_OBJECT DeviceObject;
    FSP_FSCTL_TRANSACT_REQ *InternalRequest;
    FSP_FSCTL_TRANSACT_RSP *InternalResponse;
    FSP_FSCTL_DECLSPEC_ALIGN UINT8 InternalResponseBuf[sizeof(FSP_FSCTL_TRANSACT_RSP)];
    FUSE_PROTO_REQ *FuseRequest;
    FUSE_PROTO_RSP *FuseResponse;
    SHORT CoroState[16];
    UINT32 OrigUid, OrigGid, OrigPid;
    UINT64 Ino;
    union
    {
        struct
        {
            STRING OrigPath;
            STRING Remain, Name;
            UINT32 FileUid, FileGid, FileMode;
            UINT32 DesiredAccess, GrantedAccess;
        } Lookup;
        struct
        {
            LIST_ENTRY ForgetList;
        } Forget;
    };
};
#pragma warning(pop)
VOID FuseContextCreate(FUSE_CONTEXT **PContext,
    PDEVICE_OBJECT DeviceObject, FSP_FSCTL_TRANSACT_REQ *InternalRequest);
VOID FuseContextDelete(FUSE_CONTEXT *Context);
#define FuseContextStatus(S)            \
    (                                   \
        ASSERT(0xC0000000 == ((UINT32)(S) & 0xFFFF0000)),\
        (FUSE_CONTEXT *)(UINT_PTR)((UINT32)(S) & 0x0000FFFF)\
    )
#define FuseContextIsStatus(C)          ((UINT_PTR)0x0000FFFF >= (UINT_PTR)(C))
#define FuseContextToStatus(C)          ((NTSTATUS)(0xC0000000 | (UINT32)(UINT_PTR)(C)))
extern FUSE_PROCESS_DISPATCH *FuseProcessFunction[];

/* FUSE I/O queue */
typedef struct _FUSE_IOQ FUSE_IOQ;
NTSTATUS FuseIoqCreate(FUSE_IOQ **PIoq);
VOID FuseIoqDelete(FUSE_IOQ *Ioq);
VOID FuseIoqStartProcessing(FUSE_IOQ *Ioq, FUSE_CONTEXT *Context);
FUSE_CONTEXT *FuseIoqEndProcessing(FUSE_IOQ *Ioq, UINT64 Unique);
VOID FuseIoqPostPending(FUSE_IOQ *Ioq, FUSE_CONTEXT *Context);
FUSE_CONTEXT *FuseIoqNextPending(FUSE_IOQ *Ioq); /* does not block! */

/* FUSE "entry" cache */
typedef struct _FUSE_CACHE FUSE_CACHE;
NTSTATUS FuseCacheCreate(ULONG Capacity, BOOLEAN CaseInsensitive, FUSE_CACHE **PCache);
VOID FuseCacheDelete(FUSE_CACHE *Cache);
VOID FuseCacheDeleteItems(PLIST_ENTRY ItemList);
BOOLEAN FuseCacheForgetNextItem(PLIST_ENTRY ItemList, PUINT64 PIno);
VOID FuseCacheInvalidateExpired(FUSE_CACHE *Cache, UINT64 ExpirationTime,
    PDEVICE_OBJECT DeviceObject);
BOOLEAN FuseCacheGetEntry(FUSE_CACHE *Cache, UINT64 ParentIno, PSTRING Name,
    FUSE_PROTO_ENTRY *Entry);
NTSTATUS FuseCacheSetEntry(FUSE_CACHE *Cache, UINT64 ParentIno, PSTRING Name,
    FUSE_PROTO_ENTRY *Entry);

/* FUSE processing functions */
FUSE_PROCESS_DISPATCH FuseOpReserved;
FUSE_PROCESS_DISPATCH FuseOpCreate;
FUSE_PROCESS_DISPATCH FuseOpOverwrite;
FUSE_PROCESS_DISPATCH FuseOpCleanup;
FUSE_PROCESS_DISPATCH FuseOpClose;
FUSE_PROCESS_DISPATCH FuseOpRead;
FUSE_PROCESS_DISPATCH FuseOpWrite;
FUSE_PROCESS_DISPATCH FuseOpQueryInformation;
FUSE_PROCESS_DISPATCH FuseOpSetInformation;
FUSE_PROCESS_DISPATCH FuseOpQueryEa;
FUSE_PROCESS_DISPATCH FuseOpSetEa;
FUSE_PROCESS_DISPATCH FuseOpFlushBuffers;
FUSE_PROCESS_DISPATCH FuseOpQueryVolumeInformation;
FUSE_PROCESS_DISPATCH FuseOpSetVolumeInformation;
FUSE_PROCESS_DISPATCH FuseOpQueryDirectory;
FUSE_PROCESS_DISPATCH FuseOpFileSystemControl;
FUSE_PROCESS_DISPATCH FuseOpDeviceControl;
FUSE_PROCESS_DISPATCH FuseOpQuerySecurity;
FUSE_PROCESS_DISPATCH FuseOpSetSecurity;
FUSE_PROCESS_DISPATCH FuseOpQueryStreamInformation;

/* protocol implementation */
NTSTATUS FuseProtoPostInit(PDEVICE_OBJECT DeviceObject);
VOID FuseProtoSendInit(FUSE_CONTEXT *Context);
VOID FuseProtoSendLookup(FUSE_CONTEXT *Context);
NTSTATUS FuseProtoPostForget(PDEVICE_OBJECT DeviceObject, PLIST_ENTRY ForgetList);
VOID FuseProtoFillForget(FUSE_CONTEXT *Context);
VOID FuseProtoFillBatchForget(FUSE_CONTEXT *Context);
VOID FuseProtoSendGetattr(FUSE_CONTEXT *Context);
VOID FuseProtoSendCreate(FUSE_CONTEXT *Context);
VOID FuseProtoSendOpen(FUSE_CONTEXT *Context);

/* paths */
VOID FusePosixPathPrefix(PSTRING Path, PSTRING Prefix, PSTRING Remain);
VOID FusePosixPathSuffix(PSTRING Path, PSTRING Remain, PSTRING Suffix);

/* utility */
NTSTATUS FuseGetTokenUid(HANDLE Token, TOKEN_INFORMATION_CLASS InfoClass, PUINT32 PUid);
NTSTATUS FuseSendTransactInternalIrp(PDEVICE_OBJECT DeviceObject, PFILE_OBJECT FileObject,
    FSP_FSCTL_TRANSACT_RSP *Response, FSP_FSCTL_TRANSACT_REQ **PRequest);
NTSTATUS FuseNtStatusFromErrno(INT32 Errno);

/* memory allocation */
#define FUSE_ALLOC_TAG                  'ESUF'
#define FuseAlloc(Size)                 ExAllocatePoolWithTag(PagedPool, Size, FUSE_ALLOC_TAG)
#define FuseAllocNonPaged(Size)         ExAllocatePoolWithTag(NonPagedPool, Size, FUSE_ALLOC_TAG)
#define FuseFree(Pointer)               ExFreePoolWithTag(Pointer, FUSE_ALLOC_TAG)

/* hash mix */
/* Based on the MurmurHash3 fmix32/fmix64 function:
 * See: https://code.google.com/p/smhasher/source/browse/trunk/MurmurHash3.cpp?r=152#68
 */
static inline
UINT32 FuseHashMix32(UINT32 h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}
static inline
UINT64 FuseHashMix64(UINT64 k)
{
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}
static inline
ULONG FuseHashMixPointer(PVOID Pointer)
{
#if _WIN64
    return (ULONG)FuseHashMix64((UINT64)Pointer);
#else
    return (ULONG)FuseHashMix32((UINT32)Pointer);
#endif
}

/* timeouts */
#define FuseTimeoutInfinity32           ((UINT32)-1L)
#define FuseTimeoutInfinity64           ((UINT64)-1LL)
static inline
UINT64 FuseTimeoutFromMillis(UINT32 Millis)
{
    /* if Millis is 0 or -1 then sign-extend else 10000ULL * Millis */
    return 1 >= Millis + 1 ? (INT64)(INT32)Millis : 10000ULL * Millis;
}
static inline
UINT64 FuseExpirationTimeFromMillis(UINT32 Millis)
{
    /* if Millis is 0 or -1 then sign-extend else KeQueryInterruptTime() + 10000ULL * Millis */
    return 1 >= Millis + 1 ? (INT64)(INT32)Millis : KeQueryInterruptTime() + 10000ULL * Millis;
}
static inline
UINT64 FuseExpirationTimeFromTimeout(UINT64 Timeout)
{
    /* if Timeout is 0 or -1 then Timeout else KeQueryInterruptTime() + Timeout */
    return 1 >= Timeout + 1 ? Timeout : KeQueryInterruptTime() + Timeout;
}
static inline
BOOLEAN FuseExpirationTimeValid(UINT64 ExpirationTime)
{
    /* if ExpirationTime is 0 or -1 then ExpirationTime else KeQueryInterruptTime() < ExpirationTime */
    return 1 >= ExpirationTime + 1 ? (0 != ExpirationTime) : (KeQueryInterruptTime() < ExpirationTime);
}
static inline
BOOLEAN FuseExpirationTimeValidEx(UINT64 ExpirationTime, UINT64 CurrentTime)
{
    /* if ExpirationTime is 0 or -1 then ExpirationTime else CurrentTime < ExpirationTime */
    return 1 >= ExpirationTime + 1 ? (0 != ExpirationTime) : (CurrentTime < ExpirationTime);
}
static inline
BOOLEAN FuseExpirationTimeValid2(UINT64 ExpirationTime, UINT64 CurrentTime)
{
    return CurrentTime < ExpirationTime;
}

#endif
