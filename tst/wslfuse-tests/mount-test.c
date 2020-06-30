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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
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
    ASSERT(0 == res);
    ASSERT(0 == strncmp(CreateArg.VolumeName, "\\Device\\Volume{", 15));

    res = close(fusefd);
    ASSERT(0 == res);
}

static void mount_createvol_test(void)
{
    mount_createvol_dotest(0);
    mount_createvol_dotest(L"\\\\wslfuse-tests\\share");
}

#if 0
static void mount_unmount_test(void)
{
    WSLFUSE_IOCTL_MOUNTID_ARG MountArg;
    int fusefd, res;

    fusefd = open("/dev/fuse", O_RDWR);
    ASSERT(-1 != fusefd);

    MountArg.Operation = '?';
    MountArg.MountId = 0x42424242;
    res = ioctl(fusefd, WSLFUSE_IOCTL_MOUNTID, &MountArg);
    ASSERT(-1 == res);
    ASSERT(ENOENT == errno);

    MountArg.Operation = '-';
    MountArg.MountId = 0x42424242;
    res = ioctl(fusefd, WSLFUSE_IOCTL_MOUNTID, &MountArg);
    ASSERT(-1 == res);
    ASSERT(ENOENT == errno);

    MountArg.Operation = '+';
    MountArg.MountId = 0x42424242;
    res = ioctl(fusefd, WSLFUSE_IOCTL_MOUNTID, &MountArg);
    ASSERT(0 == res);

    MountArg.Operation = '+';
    MountArg.MountId = 0x42424242;
    res = ioctl(fusefd, WSLFUSE_IOCTL_MOUNTID, &MountArg);
    ASSERT(-1 == res);
    ASSERT(EEXIST == errno);

    MountArg.Operation = '?';
    MountArg.MountId = 0x42424242;
    res = ioctl(fusefd, WSLFUSE_IOCTL_MOUNTID, &MountArg);
    ASSERT(0 == res);

    MountArg.Operation = '?';
    MountArg.MountId = 0x43434343;
    res = ioctl(fusefd, WSLFUSE_IOCTL_MOUNTID, &MountArg);
    ASSERT(-1 == res);
    ASSERT(ENOENT == errno);

    MountArg.Operation = '-';
    MountArg.MountId = 0x42424242;
    res = ioctl(fusefd, WSLFUSE_IOCTL_MOUNTID, &MountArg);
    ASSERT(0 == res);

    MountArg.Operation = '-';
    MountArg.MountId = 0x42424242;
    res = ioctl(fusefd, WSLFUSE_IOCTL_MOUNTID, &MountArg);
    ASSERT(-1 == res);
    ASSERT(ENOENT == errno);

    MountArg.Operation = '?';
    MountArg.MountId = 0x42424242;
    res = ioctl(fusefd, WSLFUSE_IOCTL_MOUNTID, &MountArg);
    ASSERT(-1 == res);
    ASSERT(ENOENT == errno);

    res = close(fusefd);
    ASSERT(0 == res);
}
#endif

void mount_tests(void)
{
    TEST(mount_createvol_test);
#if 0
    TEST(mount_unmount_test);
#endif
}
