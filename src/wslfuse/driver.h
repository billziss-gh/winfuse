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

#include <shared/km/shared.h>
#include <shared/ku/wslfuse.h>
#include <lxdk/lxdk.h>

INT FuseMiscRegister(PLX_INSTANCE Instance);
VOID FuseMiscInitialize(VOID);

#endif
