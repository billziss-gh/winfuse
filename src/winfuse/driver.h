/**
 * @file winfuse/driver.h
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

#ifndef WINFUSE_DRIVER_H_INCLUDED
#define WINFUSE_DRIVER_H_INCLUDED

#define DRIVER_NAME                     "WinFuse"
#include <shared/km/shared.h>
#include <winfuse/coro.h>
#include <winfuse/proto.h>

#define FUSE_FSCTL_TRANSACT             \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0xC00 + 'F', METHOD_BUFFERED, FILE_ANY_ACCESS)

/* device management */
typedef struct _FUSE_DEVICE_EXTENSION
{
    FSP_FSCTL_VOLUME_PARAMS *VolumeParams;
    FUSE_RWLOCK OpGuardLock;
    PVOID Ioq;
    PVOID Cache;
    KEVENT InitEvent;
    UINT32 VersionMajor, VersionMinor;
    KSPIN_LOCK FileListLock;
    LIST_ENTRY FileList;
    /*
     * The following bitmap is used to remember which opcodes have returned ENOSYS.
     *
     * It is assumed that an opcode that returned ENOSYS once, will continue returning
     * ENOSYS in the future. Thus an expensive message to user space can be eliminated.
     *
     * This bitmap may be accessed from multiple threads without locking. This is
     * because the bitmap is used for optimization purposes ONLY, and it is ok for
     * extraneous requests (that will result in ENOSYS) to be sent to the user mode
     * file system.
     */
    UINT32 OpcodeENOSYS[2];
} FUSE_DEVICE_EXTENSION;
extern FSP_FSEXT_PROVIDER FuseProvider;
static inline
FUSE_DEVICE_EXTENSION *FuseDeviceExtension(PDEVICE_OBJECT DeviceObject)
{
    return (PVOID)((PUINT8)DeviceObject->DeviceExtension + FuseProvider.DeviceExtensionOffset);
}
static inline
BOOLEAN FuseInstanceGetOpcodeENOSYS(FUSE_DEVICE_EXTENSION *Instance, UINT32 Opcode)
{
    ASSERT(sizeof Instance->OpcodeENOSYS / sizeof Instance->OpcodeENOSYS[0] > (Opcode >> 5));
    ASSERT(8 * sizeof Instance->OpcodeENOSYS[0] > (Opcode & 0x1f));
    return !!(Instance->OpcodeENOSYS[Opcode >> 5] & (1 << (Opcode & 0x1f)));
}
static inline
VOID FuseInstanceSetOpcodeENOSYS(FUSE_DEVICE_EXTENSION *Instance, UINT32 Opcode)
{
    ASSERT(sizeof Instance->OpcodeENOSYS / sizeof Instance->OpcodeENOSYS[0] > (Opcode >> 5));
    ASSERT(8 * sizeof Instance->OpcodeENOSYS[0] > (Opcode & 0x1f));
    Instance->OpcodeENOSYS[Opcode >> 5] |= (1 << (Opcode & 0x1f));
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
VOID FuseFileInstanceInit(FUSE_DEVICE_EXTENSION *Instance);
VOID FuseFileInstanceFini(FUSE_DEVICE_EXTENSION *Instance);
NTSTATUS FuseFileCreate(FUSE_DEVICE_EXTENSION *Instance, FUSE_FILE **PFile);
VOID FuseFileDelete(FUSE_DEVICE_EXTENSION *Instance, FUSE_FILE *File);

/* FUSE processing context */
typedef struct _FUSE_CONTEXT FUSE_CONTEXT;
typedef VOID FUSE_CONTEXT_FINI(FUSE_CONTEXT *Context);
typedef BOOLEAN FUSE_OPERATION_PROC(FUSE_CONTEXT *Context);
typedef INT FUSE_OPERATION_GUARD(FUSE_CONTEXT *Context, BOOLEAN Acquire);
enum
{
    FuseOpGuardCancel = -1,
    FuseOpGuardFalse = 0,
    FuseOpGuardTrue = 1,
};
typedef struct _FUSE_OPERATION
{
    FUSE_OPERATION_PROC *Proc;
    FUSE_OPERATION_GUARD *Guard;
} FUSE_OPERATION;
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
typedef struct _FUSE_CONTEXT_SETATTR
{
    FUSE_PROTO_ATTR Attr;
    UINT32 AttrValid;
} FUSE_CONTEXT_SETATTR;
struct _FUSE_CONTEXT
{
    FUSE_CONTEXT *DictNext;
    LIST_ENTRY ListEntry;
    FUSE_CONTEXT_FINI *Fini;
    FUSE_DEVICE_EXTENSION *Instance;
    FSP_FSCTL_TRANSACT_REQ *InternalRequest;
    FSP_FSCTL_TRANSACT_RSP *InternalResponse;
    FSP_FSCTL_DECLSPEC_ALIGN UINT8 InternalResponseBuf[sizeof(FSP_FSCTL_TRANSACT_RSP)];
    FUSE_PROTO_REQ *FuseRequest;
    FUSE_PROTO_RSP *FuseResponse;
    ULONG FuseRequestLength;
    INT OpGuardResult;
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
            UINT32 Chown:1;
            UINT32 RenameIsNonExistent:1;
            UINT32 RenameIsDirectory:1;
            /* 2 path operations (rename) */
            STRING OrigPath2;
            STRING Name2;
            UINT64 Ino2;
        } LookupPath;
        FUSE_CONTEXT_SETATTR Setattr;
        struct
        {
            FUSE_PROTO_ATTR Attr;
            UINT64 StartOffset;
            UINT32 Remain;
            UINT32 Offset;
            UINT32 Length;
        } Read, Write;
        struct
        {
            FUSE_CONTEXT_LOOKUP;
            STRING OrigName;
            UINT64 NextOffset;
            UINT32 Length;
            ULONG BytesTransferred;
            PUINT8 Buffer, BufferEndP, BufferP;
        } QueryDirectory;
        struct
        {
            FUSE_CONTEXT_SETATTR;
            PSECURITY_DESCRIPTOR SecurityDescriptor;
        } Security;
    };
};
VOID FuseContextCreate(FUSE_CONTEXT **PContext,
    PDEVICE_OBJECT DeviceObject, FSP_FSCTL_TRANSACT_REQ *InternalRequest);
VOID FuseContextDelete(FUSE_CONTEXT *Context);
static inline
INT FuseOpGuardResult_(BOOLEAN RwlockResult)
{
    ASSERT(
        ( RwlockResult && FuseOpGuardTrue   == (((INT)RwlockResult - 1) | 1)) ||
        (!RwlockResult && FuseOpGuardCancel == (((INT)RwlockResult - 1) | 1)));
    return ((INT)RwlockResult - 1) | 1;
}
static inline
INT FuseOpGuardAcquireExclusive(FUSE_CONTEXT *Context)
{
    return FuseOpGuardResult_(FuseRwlockEnterWriter(&Context->Instance->OpGuardLock, Context));
}
static inline
INT FuseOpGuardAcquireShared(FUSE_CONTEXT *Context)
{
    return FuseOpGuardResult_(FuseRwlockEnterReader(&Context->Instance->OpGuardLock, Context));
}
static inline
INT FuseOpGuardReleaseExclusive(FUSE_CONTEXT *Context)
{
    FuseRwlockLeaveWriter(&Context->Instance->OpGuardLock, Context);
    return FuseOpGuardFalse;
}
static inline
INT FuseOpGuardReleaseShared(FUSE_CONTEXT *Context)
{
    FuseRwlockLeaveReader(&Context->Instance->OpGuardLock, Context);
    return FuseOpGuardFalse;
}
#define FuseContextStatus(S)            \
    (                                   \
        ASSERT(0xC0000000 == ((UINT32)(S) & 0xFFFF0000)),\
        (FUSE_CONTEXT *)(UINT_PTR)((UINT32)(S) & 0x0000FFFF)\
    )
#define FuseContextIsStatus(C)          ((UINT_PTR)0x0000FFFF >= (UINT_PTR)(C))
#define FuseContextToStatus(C)          ((NTSTATUS)(0xC0000000 | (UINT32)(UINT_PTR)(C)))
#define FuseContextWaitRequest(C)       do { while (0 == (C)->FuseRequest) coro_yield; } while (0,0)
#define FuseContextWaitResponse(C)      do { coro_yield; } while (0 == (C)->FuseResponse)
extern FUSE_OPERATION FuseOperations[];

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
VOID FuseCacheRemoveEntry(FUSE_CACHE *Cache, UINT64 ParentIno, PSTRING Name);
VOID FuseCacheReferenceItem(FUSE_CACHE *Cache, PVOID Item);
VOID FuseCacheDereferenceItem(FUSE_CACHE *Cache, PVOID Item);
VOID FuseCacheQuickExpireItem(FUSE_CACHE *Cache, PVOID Item);
VOID FuseCacheDeleteForgotten(PLIST_ENTRY ForgetList);
BOOLEAN FuseCacheForgetOne(PLIST_ENTRY ForgetList, FUSE_PROTO_FORGET_ONE *PForgetOne);

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
VOID FuseProtoSendFutimens(FUSE_CONTEXT *Context);
VOID FuseProtoSendLookupChown(FUSE_CONTEXT *Context);
VOID FuseProtoSendSetattr(FUSE_CONTEXT *Context);
VOID FuseProtoSendMkdir(FUSE_CONTEXT *Context);
VOID FuseProtoSendMknod(FUSE_CONTEXT *Context);
VOID FuseProtoSendRmdir(FUSE_CONTEXT *Context);
VOID FuseProtoSendUnlink(FUSE_CONTEXT *Context);
VOID FuseProtoSendRename(FUSE_CONTEXT *Context);
VOID FuseProtoSendCreate(FUSE_CONTEXT *Context);
VOID FuseProtoSendOpendir(FUSE_CONTEXT *Context);
VOID FuseProtoSendOpen(FUSE_CONTEXT *Context);
VOID FuseProtoSendReleasedir(FUSE_CONTEXT *Context);
VOID FuseProtoSendRelease(FUSE_CONTEXT *Context);
VOID FuseProtoSendReaddir(FUSE_CONTEXT *Context);
VOID FuseProtoSendRead(FUSE_CONTEXT *Context);
VOID FuseProtoSendWrite(FUSE_CONTEXT *Context);
VOID FuseProtoSendFsyncdir(FUSE_CONTEXT *Context);
VOID FuseProtoSendFsync(FUSE_CONTEXT *Context);
VOID FuseAttrToFileInfo(FUSE_DEVICE_EXTENSION *Instance,
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
NTSTATUS FuseSafeCopyMemory(PVOID Dst, PVOID Src, ULONG Len);
NTSTATUS FuseGetTokenUid(PACCESS_TOKEN Token, TOKEN_INFORMATION_CLASS InfoClass, PUINT32 PUid);

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
