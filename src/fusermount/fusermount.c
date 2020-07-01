/**
 * @file fusermount.c
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

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <shared/ku/wslfuse.h>

static const char *progname;
static struct
{
    const char *mountpoint;
    const char *opts;
    int unmount;
    int lazy;
    int quiet;
} args;

struct mount_opts
{
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    char WinMountPoint[260];
    int set_attr_timeout, attr_timeout;
    int set_FileInfoTimeout,
        set_DirInfoTimeout,
        set_EaTimeout,
        set_VolumeInfoTimeout,
        set_KeepFileCache;
    unsigned max_read;
};

static void warn(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    fprintf(stderr, "%s: ", progname);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

static const char *opt_parse_arg(const char *opt, char *buf, size_t siz)
{
    const char *p = opt;
    char *q = buf, *endq = q + siz;

    if (0 == p)
        return 0;

    while (endq > q && '\0' != *p && ',' != *p)
    {
        if ('\\' == p[0] && '\0' != p[1])
            p++;
        *q++ = *p++;
    }

    if (endq == q || (buf == q && '\0' == *p))
        return 0;

    *q++ = '\0';
    if ('\0' != *p)
        p++;

    return p;
}

static void utf8_to_utf16(const char *src0, uint16_t *dst, size_t siz)
{
    const uint8_t *src = src0;
    uint16_t *dstend = dst + siz / sizeof(uint16_t);

    while (dstend > dst)
    {
        uint32_t c = *src++;
        uint32_t w = 0xfffd; /* unicode replacement char */

        if (0x80 > c)
            w = c;
        else if (0xc0 > c)
            ;
        else if (0xe0 > c)
        {
            c = (c & 0x1f) << 6;
            if (0x80 != (*src & 0x80))
                goto copy;
            c |= *src++ & 0x3f;
            w = c;
        }
        else if (0xf0 > c)
        {
            c = (c & 0x0f) << 12;
            if (0x80 != (*src & 0x80))
                goto copy;
            c |= (uint32_t)(*src++ & 0x3f) << 6;
            if (0x80 != (*src & 0x80))
                goto copy;
            c |= *src++ & 0x3f;
            w = c;
        }
        else if (0xf8 > c)
        {
            /* we do not support true UTF-16; just skip over 3 continuation bytes */
            if (0x80 != (*src & 0x80))
                goto copy;
            src++;
            if (0x80 != (*src & 0x80))
                goto copy;
            src++;
            if (0x80 != (*src & 0x80))
                goto copy;
            src++;
        }

    copy:
        *dst++ = (uint16_t)w;

        if (0 == c)
            break;
    }

    if (sizeof(uint16_t) <= siz)
        dstend[-1] = 0;
}

static int get_mnt_id(const char *mountpoint)
{
    FILE *file = 0;
    char line[4096];
    char path[4096];
    int mnt_id = -1, m;

    file = fopen("/proc/self/mountinfo", "rb");
    if (0 == file)
        goto exit;

    while (0 != fgets(line, sizeof line, file))
    {
        if (2 == sscanf(line, "%d %*d %*s %*s %4096s %*s", &m, path) &&
            0 == strcmp(mountpoint, path))
        {
            mnt_id = m;
            break;
        }
    }

    if (-1 == mnt_id)
        errno = ENODEV;

exit:
    if (0 != file)
        fclose(file);

    return mnt_id;
}

static void mount_opt_parse(struct mount_opts *mo)
{
    const char *opt;
    char optarg[4096], *optval;

    memset(mo, 0, sizeof *mo);

    mo->VolumeParams.Version = sizeof(FSP_FSCTL_VOLUME_PARAMS);
    mo->VolumeParams.FileInfoTimeout = 1000;
    mo->VolumeParams.FlushAndPurgeOnCleanup = 1;

    for (
        opt = opt_parse_arg(args.opts, optarg, sizeof optarg);
        0 != opt;
        opt = opt_parse_arg(opt, optarg, sizeof optarg))
    {
        optval = strchr(optarg, '=');
        if (0 != optval)
            *optval++ = '\0';

        if (0 == strcmp(optarg, "Volume"))
        {
            strncpy(mo->WinMountPoint, optval, sizeof mo->WinMountPoint);
            mo->WinMountPoint[sizeof mo->WinMountPoint - 1] = '\0';
            for (char *P = mo->WinMountPoint; *P; P++)
                if ('/' == *P)
                    *P = '\\';
        }
        else if (0 == strcmp(optarg, "attr_timeout"))
        {
            mo->set_attr_timeout = 1;
            mo->attr_timeout = (int)strtol(optval, 0, 10);
        }
        else if (0 == strcmp(optarg, "SectorSize"))
            mo->VolumeParams.SectorSize = (UINT16)strtoul(optval, 0, 10);
        else if (0 == strcmp(optarg, "SectorsPerAllocationUnit"))
            mo->VolumeParams.SectorsPerAllocationUnit = (UINT16)strtoul(optval, 0, 10);
        else if (0 == strcmp(optarg, "MaxComponentLength"))
            mo->VolumeParams.MaxComponentLength = (UINT16)strtoul(optval, 0, 10);
        else if (0 == strcmp(optarg, "VolumeCreationTime"))
            mo->VolumeParams.VolumeCreationTime = (UINT64)strtol(optval, 0, 0);
        else if (0 == strcmp(optarg, "VolumeSerialNumber"))
            mo->VolumeParams.VolumeSerialNumber = (UINT32)strtoul(optval, 0, 16);
        else if (0 == strcmp(optarg, "FileInfoTimeout"))
        {
            mo->set_FileInfoTimeout = 1;
            mo->VolumeParams.FileInfoTimeout = (UINT32)strtol(optval, 0, 10);
        }
        else if (0 == strcmp(optarg, "DirInfoTimeout"))
        {
            mo->set_DirInfoTimeout = 1;
            mo->VolumeParams.DirInfoTimeout = (UINT32)strtol(optval, 0, 10);
        }
        else if (0 == strcmp(optarg, "EaTimeout"))
        {
            mo->set_EaTimeout = 1;
            mo->VolumeParams.EaTimeout = (UINT32)strtol(optval, 0, 10);
        }
        else if (0 == strcmp(optarg, "VolumeInfoTimeout"))
        {
            mo->set_VolumeInfoTimeout = 1;
            mo->VolumeParams.VolumeInfoTimeout = (UINT32)strtol(optval, 0, 10);
        }
        else if (0 == strcmp(optarg, "KeepFileCache"))
            mo->set_KeepFileCache = 1;
        else if (0 == strcmp(optarg, "UNC") || 0 == strcmp(optarg, "VolumePrefix"))
        {
            utf8_to_utf16(optval, mo->VolumeParams.Prefix,
                sizeof mo->VolumeParams.Prefix);
            for (WCHAR *P = mo->VolumeParams.Prefix; *P; P++)
                if ('/' == *P)
                    *P = '\\';
        }
        else if (0 == strcmp(optarg, "FileSystemName"))
        {
            utf8_to_utf16("FUSE-", mo->VolumeParams.FileSystemName,
                sizeof mo->VolumeParams.FileSystemName);
            utf8_to_utf16(optval, mo->VolumeParams.FileSystemName + 5,
                sizeof mo->VolumeParams.FileSystemName - 5 * sizeof(WCHAR));
        }
    }

    if (!mo->set_FileInfoTimeout && mo->set_attr_timeout)
        mo->VolumeParams.FileInfoTimeout = (UINT32)(mo->attr_timeout * 1000);
    if (mo->set_DirInfoTimeout)
        mo->VolumeParams.DirInfoTimeoutValid = 1;
    if (mo->set_EaTimeout)
        mo->VolumeParams.EaTimeoutValid = 1;
    if (mo->set_VolumeInfoTimeout)
        mo->VolumeParams.VolumeInfoTimeoutValid = 1;
    if (mo->set_KeepFileCache)
        mo->VolumeParams.FlushAndPurgeOnCleanup = 0;
    mo->VolumeParams.CaseSensitiveSearch = 1;
    mo->VolumeParams.CasePreservedNames = 1;
    mo->VolumeParams.PersistentAcls = 1;
    mo->VolumeParams.ReparsePoints = 1;
    mo->VolumeParams.ReparsePointsAccessCheck = 0;
    mo->VolumeParams.NamedStreams = 0;
    mo->VolumeParams.ReadOnlyVolume = 0;
    mo->VolumeParams.PostCleanupWhenModifiedOnly = 1;
    mo->VolumeParams.PassQueryDirectoryFileName = 1;
    mo->VolumeParams.DeviceControl = 1;
    if ('\0' == mo->VolumeParams.FileSystemName[0])
        utf8_to_utf16("FUSE", mo->VolumeParams.FileSystemName,
            sizeof mo->VolumeParams.FileSystemName);

    if (mo->VolumeParams.SectorSize < 512 ||
        mo->VolumeParams.SectorSize > 4096)
        mo->VolumeParams.SectorSize = 4096;
    if (mo->VolumeParams.SectorsPerAllocationUnit == 0)
        mo->VolumeParams.SectorsPerAllocationUnit = 1;
    if (0 == mo->VolumeParams.MaxComponentLength || mo->VolumeParams.MaxComponentLength > 255)
        mo->VolumeParams.MaxComponentLength = 255;
    if (0 == mo->VolumeParams.VolumeCreationTime)
    {
        struct timeval tv = { 0 };
        gettimeofday(&tv, 0);
        mo->VolumeParams.VolumeCreationTime = (UINT64)
            ((int64_t)tv.tv_sec * 10000000 + (int64_t)tv.tv_usec * 10 + 116444736000000000LL);
    }
    if (0 == mo->VolumeParams.VolumeSerialNumber)
        mo->VolumeParams.VolumeSerialNumber =
            (UINT32)(mo->VolumeParams.VolumeCreationTime >> 32) ^
            (UINT32)(mo->VolumeParams.VolumeCreationTime & 0xffffffff);
}

static int start_mount_helper(const char *VolumeName, char MountPoint[260], pid_t *pidp, int *ofdp)
{
    int res;
    pid_t pid;
    int ifd[2] = { -1, -1 };
    int ofd[2] = { -1, -1 };
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attr;
    char *argv[4];
    char *p, *endp;
    ssize_t bytes;
    int initdone_actions = 0, initdone_attr = 0;

    if (-1 == pipe(ifd))
    {
        res = errno;
        goto exit;
    }

    if (-1 == pipe(ofd))
    {
        res = errno;
        goto exit;
    }

    res = posix_spawn_file_actions_init(&actions);
    if (0 != res)
        goto exit;
    initdone_actions = 1;

    res = posix_spawn_file_actions_addclose(&actions, ifd[0]);
    if (0 != res)
        goto exit;
    res = posix_spawn_file_actions_adddup2(&actions, ifd[1], 1);
    if (0 != res)
        goto exit;
    res = posix_spawn_file_actions_addclose(&actions, ifd[1]);
    if (0 != res)
        goto exit;

    res = posix_spawn_file_actions_addclose(&actions, ofd[1]);
    if (0 != res)
        goto exit;
    res = posix_spawn_file_actions_adddup2(&actions, ofd[0], 0);
    if (0 != res)
        goto exit;
    res = posix_spawn_file_actions_addclose(&actions, ofd[0]);
    if (0 != res)
        goto exit;

    res = posix_spawnattr_init(&attr);
    if (0 != res)
        goto exit;
    initdone_attr = 1;

    res = posix_spawnattr_setflags(&attr, POSIX_SPAWN_RESETIDS);
    if (0 != res)
        goto exit;

    argv[0] = "/usr/bin/fusermount-helper.exe";
    argv[1] = (char *)VolumeName;
    argv[2] = '\0' != MountPoint[0] ? (char *)MountPoint : 0;
    argv[3] = 0;
    res = posix_spawn(&pid, argv[0], &actions, &attr, argv, 0);
    if (0 != res)
        goto exit;
    *pidp = pid;

    close(ifd[1]); ifd[1] = -1;
    close(ofd[0]); ofd[0] = -1;

    for (p = MountPoint, endp = MountPoint + 260 - 1; endp > p; p += bytes)
    {
        bytes = read(ifd[0], p, (size_t)(endp - p));
        if (-1 == bytes)
        {
            if (EINTR != errno)
            {
                res = errno;
                goto exit;
            }
            bytes = 0;
        }
        else if (0 == bytes || 0 != memchr(p, 0, (size_t)bytes))
        {
            p += bytes;
            break;
        }
    }
    *p = '\0';

    if (MountPoint == p)
    {
        res = EINVAL;
        goto exit;
    }

    close(ifd[0]); ifd[0] = -1;

    *ofdp = ofd[1]; ofd[1] = -1;

    res = 0;

exit:
    if (initdone_attr)
        posix_spawnattr_destroy(&attr);

    if (initdone_actions)
        posix_spawn_file_actions_destroy(&actions);

    if (-1 != ofd[1])
        close(ofd[1]);
    if (-1 != ofd[0])
        close(ofd[0]);

    if (-1 != ifd[1])
        close(ifd[1]);
    if (-1 != ifd[0])
        close(ifd[0]);

    return res;
}

static void stop_mount_helper(pid_t pid, int ofd)
{
    ssize_t bytes;
    (void)bytes;

    bytes = write(ofd, "STOP", 4 + 1);
    close(ofd);

    waitpid(pid, 0, 0);
}

static void do_mount(void)
{
    int success = 0;
    struct mount_opts mo;
    const char *env;
    int commfd, fusefd = -1;
    int mounted = 0, mnt_id;
    WSLFUSE_IOCTL_CREATEVOLUME_ARG CreateArg;
    WSLFUSE_IOCTL_WINMOUNT_ARG WinMountArg;
    WSLFUSE_IOCTL_LXMOUNT_ARG LxMountArg;
    pid_t helper_pid = -1;
    int helper_fd = -1;
    struct msghdr msg;
    struct cmsghdr *cmsg;
    char cmsgbuf[CMSG_SPACE(sizeof fusefd)];
    struct iovec iov;
    char dummy = 0;
    int msgsent = 0;

    mount_opt_parse(&mo);

    /*
     * Get the socket fd sent to us by the file system (via libfuse) from the environment.
     * We will use this fd to send our /dev/fuse fd to the file system once mounting completes.
     */
    env = getenv("_FUSE_COMMFD");
    if (0 == env)
    {
        warn("mount: environment variable _FUSE_COMMFD missing");
        goto exit;
    }
    commfd = atoi(env);

    /*
     * Open the /dev/fuse device. We use the returned fd to communicate with WinFuse using
     * special ioctl's. We will also return the fd to the file system via COMMFD.
     */
    fusefd = open("/dev/fuse", O_RDWR);
    if (-1 == fusefd)
    {
        warn("mount: cannot open /dev/fuse: %s", strerror(errno));
        goto exit;
    }

    /*
     * Send an ioctl to instruct WinFuse to create a new WinFsp volume.
     */
    memset(&CreateArg, 0, sizeof CreateArg);
    memcpy(&CreateArg.VolumeParams, &mo.VolumeParams, sizeof mo.VolumeParams);
    if (-1 == ioctl(fusefd, WSLFUSE_IOCTL_CREATEVOLUME, &CreateArg))
    {
        warn("mount: cannot ioctl(C) /dev/fuse: %s", strerror(errno));
        goto exit;
    }

    /*
     * Start the Windows side helper process to create the Windows mount point.
     */
    errno = start_mount_helper(CreateArg.VolumeName, mo.WinMountPoint,
        &helper_pid, &helper_fd);
    if (0 != errno)
    {
        warn("mount: cannot set Windows mount point: %s", strerror(errno));
        goto exit;
    }

    /*
     * Associate the Windows mount point with the /dev/fuse fd.
     */
    memset(&WinMountArg, 0, sizeof WinMountArg);
    memcpy(WinMountArg.WinMountPoint, mo.WinMountPoint, sizeof WinMountArg.WinMountPoint);
    if (-1 == ioctl(fusefd, WSLFUSE_IOCTL_WINMOUNT, &WinMountArg))
    {
        warn("mount: cannot ioctl(M) /dev/fuse: %s", strerror(errno));
        goto exit;
    }

    /*
     * Send the /dev/fuse fd to the file system using COMMFD.
     */
    memset(&msg, 0, sizeof msg);
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof cmsgbuf;
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof fusefd);
    memcpy(CMSG_DATA(cmsg), &fusefd, sizeof fusefd);
    msg.msg_controllen = cmsg->cmsg_len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    iov.iov_base = &dummy;
    iov.iov_len = sizeof dummy;
    while (-1 == sendmsg(commfd, &msg, 0))
        if (EINTR != errno)
        {
            warn("mount: cannot sendmsg to _FUSE_COMMFD: %s", strerror(errno));
            goto exit;
        }
    msgsent = 1;

    /*
     * Mount the WinFsp volume using drvfs.
     */
    if (-1 == mount(mo.WinMountPoint, args.mountpoint, "drvfs", 0, 0))
    {
        warn("mount: cannot mount %s: %s", args.mountpoint, strerror(errno));
        goto exit;
    }
    mounted = 1;

    /*
     * Get the mnt_id of the newly mounted file system.
     */
    mnt_id = get_mnt_id(args.mountpoint);
    if (-1 == mnt_id)
    {
        warn("mount: cannot get mnt_id: %s", strerror(errno));
        goto exit;
    }

    /*
     * Associate the WSL mnt_id with the /dev/fuse fd.
     */
    memset(&LxMountArg, 0, sizeof LxMountArg);
    LxMountArg.Operation = '+';
    LxMountArg.LxMountId = (UINT64)mnt_id;
    if (-1 == ioctl(fusefd, WSLFUSE_IOCTL_LXMOUNT, &LxMountArg))
    {
        warn("mount: cannot ioctl(m) /dev/fuse: %s", strerror(errno));
        goto exit;
    }

    success = 1;

exit:
    if (!success)
    {
        if (mounted)
            umount2(args.mountpoint, UMOUNT_NOFOLLOW);

        if (msgsent)
        {
            memset(&LxMountArg, 0, sizeof LxMountArg);
            LxMountArg.Operation = '-';
            LxMountArg.LxMountId = (UINT64)-1LL;
            ioctl(fusefd, WSLFUSE_IOCTL_LXMOUNT, &LxMountArg);
        }
    }

    if (-1 != helper_pid)
        stop_mount_helper(helper_pid, helper_fd);

    if (-1 != fusefd)
        close(fusefd);

    if (!success)
        exit(1);

    exit(0);
}

static void do_unmount(void)
{
    int success = 0;
    int fusefd = -1;
    int mnt_id;
    WSLFUSE_IOCTL_LXMOUNT_ARG LxMountArg;

    /*
     * Open the /dev/fuse device.
     */
    fusefd = open("/dev/fuse", O_RDWR);
    if (-1 == fusefd)
    {
        warn("unmount: cannot open /dev/fuse: %s", strerror(errno));
        goto exit;
    }

    /*
     * Get the mnt_id of the previously mounted file system.
     */
    mnt_id = get_mnt_id(args.mountpoint);
    if (-1 == mnt_id)
    {
        warn("unmount: cannot get mnt_id: %s", strerror(errno));
        goto exit;
    }

    /*
     * Unmount the previously mounted file system.
     */
    if (-1 == umount2(args.mountpoint, UMOUNT_NOFOLLOW | (args.lazy ? MNT_DETACH : 0)))
    {
        warn("unmount: cannot unmount %s: %s", args.mountpoint, strerror(errno));
        goto exit;
    }

    /*
     * Disassociate the WSL mnt_id from the /dev/fuse fd.
     */
    memset(&LxMountArg, 0, sizeof LxMountArg);
    LxMountArg.Operation = '-';
    LxMountArg.LxMountId = (UINT64)mnt_id;
    if (-1 == ioctl(fusefd, WSLFUSE_IOCTL_LXMOUNT, &LxMountArg) && ENOENT != errno)
    {
        warn("unmount: cannot ioctl(m) /dev/fuse: %s", strerror(errno));
        goto exit;
    }

    success = 1;

exit:
    if (-1 != fusefd)
        close(fusefd);

    if (!success)
        exit(1);

    exit(0);
}

static void do_version(void)
{
    fprintf(stdout, "%s version: %s\n", progname, "winfuse");
    exit(0);
}

static void usage(void)
{
    warn(
        "%s: [options] mountpoint\n"
        "Options:\n"
        "    -h                          print help\n"
        "    -V                          print version\n"
        "    -o opt[,opt...]             mount options\n"
        "    -u                          unmount\n"
        "    -z                          lazy unmount\n"
        "    -q                          quiet\n"
        "",
        progname);
    exit(2);
}

int main(int argc, char *argv[])
{
    progname = basename(argv[0]);

    static struct option longopts[] =
    {
        { "help", no_argument, 0, 'h' },
        { "version", no_argument, 0, 'V' },
        { "unmount", no_argument, 0, 'u' },
        { "lazy", no_argument, 0, 'z' },
        { "quiet", no_argument, 0, 'q' },
        { 0 },
    };
    int opt;
    while (-1 != (opt = getopt_long(argc, argv, "hVo:uzq", longopts, 0)))
        switch (opt)
        {
        default:
        case 'h':
            usage();
            break;
        case 'V':
            do_version();
            break;
        case 'o':
            args.opts = optarg;
            break;
        case 'u':
            args.unmount = 1;
            break;
        case 'z':
            args.lazy = 1;
            break;
        case 'q':
            args.quiet = 1;
            break;
        }

    if (argc != optind + 1)
        usage();

    args.mountpoint = realpath(argv[optind], 0);
    if (0 == args.mountpoint)
    {
        warn("mountpoint: %s", strerror(errno));
        exit(1);
    }

    if (!args.unmount)
        do_mount();

    do_unmount();

    return 0;
}
