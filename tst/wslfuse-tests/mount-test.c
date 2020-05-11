/**
 * @file mount-test.c
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

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <shared/ku/wslfuse.h>
#include <tlib/testsuite.h>

static void mount_createvol_dotest(wchar_t *Prefix)
{
    WSLFUSE_IOCTL_CREATEVOLUME_ARG CreateArg = { .VolumeParams.Version = sizeof(FSP_FSCTL_VOLUME_PARAMS)};
    int fusefd, res;

    if (0 != Prefix && L'\\' == Prefix[0] && L'\\' == Prefix[1])
    {
        /* note that on Linux: sizeof(wchar_t) != sizeof(WCHAR) */
        wchar_t *P = Prefix + 1;
        WCHAR *Q = CreateArg.VolumeParams.Prefix;
        while (0 != (*Q++ = (WCHAR)*P++))
            ;
    }

    fusefd = open("/dev/fuse", O_RDWR);
    ASSERT(-1 != fusefd);

    res = ioctl(fusefd, WSLFUSE_IOCTL_CREATEVOLUME, &CreateArg);
    ASSERT(-1 != res);

    res = close(fusefd);
    ASSERT(-1 != res);
}

static void mount_createvol_test(void)
{
    mount_createvol_dotest(0);
    mount_createvol_dotest(L"\\\\winfuse-tests\\share");
}

static void mount_unmount_test(void)
{
}

void mount_tests(void)
{
    TEST(mount_createvol_test);
    TEST(mount_unmount_test);
}
