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
#pragma warning(disable:4127)           /* conditional expression is constant */
#pragma warning(disable:4200)           /* zero-sized array in struct/union */
#pragma warning(disable:4201)           /* nameless struct/union */

#include <winfuse/coro.h>
#include <winfuse/proto.h>

#define DRIVER_NAME                     "WinFuse"

/* debug */
#if DBG
ULONG DebugRandom(VOID);
BOOLEAN DebugMemoryChangeTest(PVOID Memory, SIZE_T Size, BOOLEAN Test);
#define DEBUGLOG(fmt, ...)              \
    DbgPrint("[%d] " DRIVER_NAME "!" __FUNCTION__ ": " fmt "\n", KeGetCurrentIrql(), __VA_ARGS__)
#define DEBUGTEST(Percent)              (DebugRandom() <= (Percent) * 0x7fff / 100)
#else
#define DEBUGLOG(fmt, ...)              ((void)0)
#define DEBUGTEST(Percent)              (TRUE)
#endif

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
    KSPIN_LOCK FileListLock;
    LIST_ENTRY FileList;
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

/* FUSE files */
typedef struct _FUSE_FILE
{
    LIST_ENTRY ListEntry;
    UINT64 Ino;
    UINT64 Fh;
    UINT32 OpenFlags;
    UINT32 IsDirectory:1;
    UINT32 IsReparsePoint:1;
    PVOID CacheItem;
} FUSE_FILE;
VOID FuseFileDeviceInit(PDEVICE_OBJECT DeviceObject);
VOID FuseFileDeviceFini(PDEVICE_OBJECT DeviceObject);
NTSTATUS FuseFileCreate(PDEVICE_OBJECT DeviceObject, FUSE_FILE **PFile);
VOID FuseFileDelete(PDEVICE_OBJECT DeviceObject, FUSE_FILE *File);

/* FUSE processing context */
typedef struct _FUSE_CONTEXT FUSE_CONTEXT;
typedef VOID FUSE_CONTEXT_FINI(FUSE_CONTEXT *Context);
typedef BOOLEAN FUSE_PROCESS_DISPATCH(FUSE_CONTEXT *Context);
typedef struct _FUSE_CONTEXT_LOOKUP
{
    PVOID CacheGen;
    PVOID CacheItem;
    UINT64 Ino;
    STRING Name;
    FUSE_PROTO_ATTR Attr;
} FUSE_CONTEXT_LOOKUP;
typedef struct _FUSE_CONTEXT_FORGET
{
    LIST_ENTRY ForgetList;
} FUSE_CONTEXT_FORGET;
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
    FUSE_FILE *File;
    union
    {
        FUSE_CONTEXT_LOOKUP Lookup;
        FUSE_CONTEXT_FORGET Forget;
        struct
        {
            FUSE_CONTEXT_LOOKUP;
            STRING OrigPath;
            STRING Remain;
            UINT32 DesiredAccess, GrantedAccess;
            UINT32 UserMode:1;
            UINT32 HasTraversePrivilege:1;
            UINT32 DisableCache:1;
            UINT32 ChownOnCreate:1;
        } LookupPath;
        struct
        {
            FUSE_PROTO_ATTR Attr;
        } Setattr;
        struct
        {
            FUSE_CONTEXT_LOOKUP;
            STRING OrigName;
            UINT64 NextOffset;
            UINT32 Length;
            ULONG BytesTransferred;
            PUINT8 Buffer, BufferEndP, BufferP;
        } QueryDirectory;
    };
};
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
#define FuseContextWaitRequest(C)       do { while (0 == (C)->FuseRequest) coro_yield; } while (0,0)
#define FuseContextWaitResponse(C)      do { coro_yield; } while (0 == (C)->FuseResponse)
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
typedef struct _FUSE_CACHE_GEN FUSE_CACHE_GEN;
NTSTATUS FuseCacheCreate(ULONG Capacity, BOOLEAN CaseInsensitive, FUSE_CACHE **PCache);
VOID FuseCacheDelete(FUSE_CACHE *Cache);
VOID FuseCacheExpirationRoutine(FUSE_CACHE *Cache,
    PDEVICE_OBJECT DeviceObject, UINT64 ExpirationTime);
NTSTATUS FuseCacheReferenceGen(FUSE_CACHE *Cache, PVOID *PGen);
VOID FuseCacheDereferenceGen(FUSE_CACHE *Cache, PVOID Gen);
BOOLEAN FuseCacheGetEntry(FUSE_CACHE *Cache, UINT64 ParentIno, PSTRING Name,
    FUSE_PROTO_ENTRY *Entry, PVOID *PItem);
VOID FuseCacheSetEntry(FUSE_CACHE *Cache, UINT64 ParentIno, PSTRING Name,
    FUSE_PROTO_ENTRY *Entry, PVOID *PItem);
VOID FuseCacheReferenceItem(FUSE_CACHE *Cache, PVOID Item);
VOID FuseCacheDereferenceItem(FUSE_CACHE *Cache, PVOID Item);
VOID FuseCacheDeleteForgotten(PLIST_ENTRY ForgetList);
BOOLEAN FuseCacheForgetOne(PLIST_ENTRY ForgetList, FUSE_PROTO_FORGET_ONE *PForgetOne);

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
VOID FuseProtoSendStatfs(FUSE_CONTEXT *Context);
VOID FuseProtoSendGetattr(FUSE_CONTEXT *Context);
VOID FuseProtoSendFgetattr(FUSE_CONTEXT *Context);
VOID FuseProtoSendFtruncate(FUSE_CONTEXT *Context);
VOID FuseProtoSendMkdir(FUSE_CONTEXT *Context);
VOID FuseProtoSendMknod(FUSE_CONTEXT *Context);
VOID FuseProtoSendRmdir(FUSE_CONTEXT *Context);
VOID FuseProtoSendUnlink(FUSE_CONTEXT *Context);
VOID FuseProtoSendCreate(FUSE_CONTEXT *Context);
VOID FuseProtoSendCreateChown(FUSE_CONTEXT *Context);
VOID FuseProtoSendOpendir(FUSE_CONTEXT *Context);
VOID FuseProtoSendOpen(FUSE_CONTEXT *Context);
VOID FuseProtoSendReleasedir(FUSE_CONTEXT *Context);
VOID FuseProtoSendRelease(FUSE_CONTEXT *Context);
VOID FuseProtoSendReaddir(FUSE_CONTEXT *Context);
VOID FuseAttrToFileInfo(PDEVICE_OBJECT DeviceObject,
    FUSE_PROTO_ATTR *Attr, FSP_FSCTL_FILE_INFO *FileInfo);
static inline
VOID FuseFileTimeToUnixTime(UINT64 FileTime0, PUINT64 sec, PUINT32 nsec)
{
    INT64 FileTime = (INT64)FileTime0 - 116444736000000000LL;
    INT32 Remain = FileTime % 10000000;
    *sec = (UINT64)(FileTime / 10000000);
    *nsec = (UINT32)(0 <= Remain ? Remain : Remain + 10000000) * 100;
}
static inline
VOID FuseUnixTimeToFileTime(UINT64 sec, UINT32 nsec, PUINT64 PFileTime)
{
    INT64 FileTime = (INT64)sec * 10000000 + (INT64)nsec / 100 + 116444736000000000LL;
    *PFileTime = FileTime;
}
NTSTATUS FuseNtStatusFromErrno(INT32 Errno);

/* paths */
VOID FusePosixPathPrefix(PSTRING Path, PSTRING Prefix, PSTRING Remain);
VOID FusePosixPathSuffix(PSTRING Path, PSTRING Remain, PSTRING Suffix);

/* utility */
PVOID FuseAllocatePoolMustSucceed(POOL_TYPE PoolType, SIZE_T Size, ULONG Tag);
NTSTATUS FuseGetTokenUid(PACCESS_TOKEN Token, TOKEN_INFORMATION_CLASS InfoClass, PUINT32 PUid);

/* memory allocation */
#define FUSE_ALLOC_TAG                  'ESUF'
#define FuseAlloc(Size)                 ExAllocatePoolWithTag(PagedPool, Size, FUSE_ALLOC_TAG)
#define FuseAllocNonPaged(Size)         ExAllocatePoolWithTag(NonPagedPool, Size, FUSE_ALLOC_TAG)
#define FuseAllocMustSucceed(Size)      FuseAllocatePoolMustSucceed(PagedPool, Size, FUSE_ALLOC_TAG)
#define FuseFree(Pointer)               ExFreePoolWithTag(Pointer, FUSE_ALLOC_TAG)
#define FuseFreeExternal(Pointer)       ExFreePool(Pointer)

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

#endif
