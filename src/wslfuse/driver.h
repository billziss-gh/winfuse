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

#include <ntifs.h>
#include <lxdk/lxdk.h>

#include <kushared/wslfuse.h>

/* disable warnings */
#pragma warning(disable:4100)           /* unreferenced formal parameter */
#pragma warning(disable:4127)           /* conditional expression is constant */
#pragma warning(disable:4200)           /* zero-sized array in struct/union */
#pragma warning(disable:4201)           /* nameless struct/union */

#define DRIVER_NAME                     "WslFuse"

INT FuseMiscRegister(
    PLX_INSTANCE Instance);

#endif
