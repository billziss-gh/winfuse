/**
 * @file shared/km/shared.h
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

#ifndef SHARED_KM_SHARED_H_INCLUDED
#define SHARED_KM_SHARED_H_INCLUDED

#include <ntifs.h>
#include <winfsp/fsext.h>

/* disable warnings */
#pragma warning(disable:4100)           /* unreferenced formal parameter */
#pragma warning(disable:4127)           /* conditional expression is constant */
#pragma warning(disable:4200)           /* zero-sized array in struct/union */
#pragma warning(disable:4201)           /* nameless struct/union */

/* debug */
#if DBG
ULONG DebugRandom(VOID);
BOOLEAN DebugMemory(PVOID Memory, SIZE_T Size, BOOLEAN Test);
#define DEBUGLOG(fmt, ...)              \
    DbgPrint("[%d] " DRIVER_NAME "!" __FUNCTION__ ": " fmt "\n", KeGetCurrentIrql(), __VA_ARGS__)
#define DEBUGTEST(Percent)              (DebugRandom() <= (Percent) * 0x7fff / 100)
#define DEBUGFILL(M, S)                 DebugMemory(M, S, FALSE)
#define DEBUGGOOD(M, S)                 DebugMemory(M, S, TRUE)
#else
#define DEBUGLOG(fmt, ...)              ((void)0)
#define DEBUGTEST(Percent)              (TRUE)
#define DEBUGFILL(M, S)                 (TRUE)
#define DEBUGGOOD(M, S)                 (TRUE)
#endif

/* memory allocation */
#define FUSE_ALLOC_TAG                  'ESUF'
#define FuseAlloc(Size)                 ExAllocatePoolWithTag(PagedPool, Size, FUSE_ALLOC_TAG)
#define FuseAllocNonPaged(Size)         ExAllocatePoolWithTag(NonPagedPool, Size, FUSE_ALLOC_TAG)
#define FuseAllocMustSucceed(Size)      FuseAllocatePoolMustSucceed(PagedPool, Size, FUSE_ALLOC_TAG)
#define FuseFree(Pointer)               ExFreePoolWithTag(Pointer, FUSE_ALLOC_TAG)
#define FuseFreeExternal(Pointer)       ExFreePool(Pointer)

/* read/write locks */
#define FUSE_RWLOCK_USE_SEMAPHORE
//#define FUSE_RWLOCK_USE_ERESOURCE
typedef struct _FUSE_RWLOCK
{
#if defined(FUSE_RWLOCK_USE_SEMAPHORE)
    KSEMAPHORE OrderSem;
    KSEMAPHORE WriteSem;
    LONG Readers;
#elif defined(FUSE_RWLOCK_USE_ERESOURCE)
    ERESOURCE Resource;
#else
#error One of FUSE_RWLOCK_USE_SEMAPHORE or FUSE_RWLOCK_USE_ERESOURCE must be defined.
#endif
} FUSE_RWLOCK;
static inline
VOID FuseRwlockInitialize(FUSE_RWLOCK *Lock)
{
#if defined(FUSE_RWLOCK_USE_SEMAPHORE)
    KeInitializeSemaphore(&Lock->OrderSem, 1, 1);
    KeInitializeSemaphore(&Lock->WriteSem, 1, 1);
    Lock->Readers = 0;
#elif defined(FUSE_RWLOCK_USE_ERESOURCE)
    ExInitializeResourceLite(&Lock->Resource);
#endif
}
static inline
VOID FuseRwlockFinalize(FUSE_RWLOCK *Lock)
{
#if defined(FUSE_RWLOCK_USE_SEMAPHORE)
#elif defined(FUSE_RWLOCK_USE_ERESOURCE)
    ExDeleteResourceLite(&Lock->Resource);
#endif
}
static inline
BOOLEAN FuseRwlockEnterWriter(FUSE_RWLOCK *Lock, PVOID Owner)
{
#if defined(FUSE_RWLOCK_USE_SEMAPHORE)
    NTSTATUS Result;
    Result = FsRtlCancellableWaitForSingleObject(&Lock->OrderSem, 0, 0);
    if (STATUS_SUCCESS == Result)
    {
        Result = FsRtlCancellableWaitForSingleObject(&Lock->WriteSem, 0, 0);
        KeReleaseSemaphore(&Lock->OrderSem, 1, 1, FALSE);
    }
    return STATUS_SUCCESS == Result;
#elif defined(FUSE_RWLOCK_USE_ERESOURCE)
    ExAcquireResourceExclusiveLite(&Lock->Resource, TRUE);
    ExSetResourceOwnerPointer(&Lock->Resource, (PVOID)((UINT_PTR)Owner | 3));
    return TRUE;
#endif
}
static inline
BOOLEAN FuseRwlockEnterReader(FUSE_RWLOCK *Lock, PVOID Owner)
{
#if defined(FUSE_RWLOCK_USE_SEMAPHORE)
    NTSTATUS Result;
    Result = FsRtlCancellableWaitForSingleObject(&Lock->OrderSem, 0, 0);
    if (STATUS_SUCCESS == Result)
    {
        if (1 == InterlockedIncrement(&Lock->Readers))
        {
            Result = FsRtlCancellableWaitForSingleObject(&Lock->WriteSem, 0, 0);
            if (STATUS_SUCCESS != Result)
                InterlockedDecrement(&Lock->Readers);
        }
        KeReleaseSemaphore(&Lock->OrderSem, 1, 1, FALSE);
    }
    return STATUS_SUCCESS == Result;
#elif defined(FUSE_RWLOCK_USE_ERESOURCE)
    ExAcquireResourceSharedLite(&Lock->Resource, TRUE);
    ExSetResourceOwnerPointer(&Lock->Resource, (PVOID)((UINT_PTR)Owner | 3));
    return TRUE;
#endif
}
static inline
VOID FuseRwlockLeaveWriter(FUSE_RWLOCK *Lock, PVOID Owner)
{
#if defined(FUSE_RWLOCK_USE_SEMAPHORE)
    KeReleaseSemaphore(&Lock->WriteSem, 1, 1, FALSE);
#elif defined(FUSE_RWLOCK_USE_ERESOURCE)
    ExReleaseResourceForThreadLite(&Lock->Resource, (ERESOURCE_THREAD)((UINT_PTR)Owner | 3));
#endif
}
static inline
VOID FuseRwlockLeaveReader(FUSE_RWLOCK *Lock, PVOID Owner)
{
#if defined(FUSE_RWLOCK_USE_SEMAPHORE)
    if (0 == InterlockedDecrement(&Lock->Readers))
        KeReleaseSemaphore(&Lock->WriteSem, 1, 1, FALSE);
#elif defined(FUSE_RWLOCK_USE_ERESOURCE)
    ExReleaseResourceForThreadLite(&Lock->Resource, (ERESOURCE_THREAD)((UINT_PTR)Owner | 3));
#endif
}

#endif
