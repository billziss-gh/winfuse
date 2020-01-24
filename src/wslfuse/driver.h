/**
 * @file wslfuse/driver.h
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

#ifndef WSLFUSE_DRIVER_H_INCLUDED
#define WSLFUSE_DRIVER_H_INCLUDED

#define DRIVER_NAME                     "WslFuse"
#include <km/shared.h>
#include <lxdk/lxdk.h>
#include <ku/wslfuse.h>

/* memory allocation */
#define FUSE_ALLOC_TAG                  'ESUF'
#define FuseAlloc(Size)                 ExAllocatePoolWithTag(PagedPool, Size, FUSE_ALLOC_TAG)
#define FuseAllocNonPaged(Size)         ExAllocatePoolWithTag(NonPagedPool, Size, FUSE_ALLOC_TAG)
#define FuseFree(Pointer)               ExFreePoolWithTag(Pointer, FUSE_ALLOC_TAG)
#define FuseFreeExternal(Pointer)       ExFreePool(Pointer)

INT FuseMiscRegister(
    PLX_INSTANCE Instance);

#endif
