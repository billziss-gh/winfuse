/**
 * @file shared/ku/wslfuse.h
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

#ifndef SHARED_KU_WSLFUSE_H_INCLUDED
#define SHARED_KU_WSLFUSE_H_INCLUDED

#if defined(__linux__)
/*
 * We duplicate some of the definitions from WinFsp and WinFuse
 * to avoid having either of them as a build dependency when
 * building on Linux.
 */

#include <stdint.h>
#include <wchar.h>
#include <linux/ioctl.h>
typedef char CHAR;
typedef uint16_t WCHAR;
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;

#define FSP_FSCTL_DISK_DEVICE_NAME      "WinFsp.Disk"
#define FSP_FSCTL_NET_DEVICE_NAME       "WinFsp.Net"
#define FSP_FSCTL_VOLUME_NAME_SIZE      (64 * sizeof(WCHAR))
#define FSP_FSCTL_VOLUME_PREFIX_SIZE    (192 * sizeof(WCHAR))
#define FSP_FSCTL_VOLUME_FSNAME_SIZE    (16 * sizeof(WCHAR))
#define FSP_FSCTL_VOLUME_NAME_SIZEMAX   (FSP_FSCTL_VOLUME_NAME_SIZE + FSP_FSCTL_VOLUME_PREFIX_SIZE)
#define FSP_FSCTL_VOLUME_PARAMS_V0_FIELD_DEFN\
    UINT16 Version;                     /* set to 0 or sizeof(FSP_FSCTL_VOLUME_PARAMS) */\
    /* volume information */\
    UINT16 SectorSize;\
    UINT16 SectorsPerAllocationUnit;\
    UINT16 MaxComponentLength;          /* maximum file name component length (bytes) */\
    UINT64 VolumeCreationTime;\
    UINT32 VolumeSerialNumber;\
    /* I/O timeouts, capacity, etc. */\
    UINT32 TransactTimeout;             /* DEPRECATED: (millis; 1 sec - 10 sec) */\
    UINT32 IrpTimeout;                  /* pending IRP timeout (millis; 1 min - 10 min) */\
    UINT32 IrpCapacity;                 /* maximum number of pending IRP's (100 - 1000)*/\
    UINT32 FileInfoTimeout;             /* FileInfo/Security/VolumeInfo timeout (millis) */\
    /* FILE_FS_ATTRIBUTE_INFORMATION::FileSystemAttributes */\
    UINT32 CaseSensitiveSearch:1;       /* file system supports case-sensitive file names */\
    UINT32 CasePreservedNames:1;        /* file system preserves the case of file names */\
    UINT32 UnicodeOnDisk:1;             /* file system supports Unicode in file names */\
    UINT32 PersistentAcls:1;            /* file system preserves and enforces access control lists */\
    UINT32 ReparsePoints:1;             /* file system supports reparse points */\
    UINT32 ReparsePointsAccessCheck:1;  /* file system performs reparse point access checks */\
    UINT32 NamedStreams:1;              /* file system supports named streams */\
    UINT32 HardLinks:1;                 /* unimplemented; set to 0 */\
    UINT32 ExtendedAttributes:1;        /* file system supports extended attributes */\
    UINT32 ReadOnlyVolume:1;\
    /* kernel-mode flags */\
    UINT32 PostCleanupWhenModifiedOnly:1;   /* post Cleanup when a file was modified/deleted */\
    UINT32 PassQueryDirectoryPattern:1;     /* pass Pattern during QueryDirectory operations */\
    UINT32 AlwaysUseDoubleBuffering:1;\
    UINT32 PassQueryDirectoryFileName:1;    /* pass FileName during QueryDirectory (GetDirInfoByName) */\
    UINT32 FlushAndPurgeOnCleanup:1;        /* keeps file off "standby" list */\
    UINT32 DeviceControl:1;                 /* support user-mode ioctl handling */\
    /* user-mode flags */\
    UINT32 UmFileContextIsUserContext2:1;   /* user mode: FileContext parameter is UserContext2 */\
    UINT32 UmFileContextIsFullContext:1;    /* user mode: FileContext parameter is FullContext */\
    UINT32 UmReservedFlags:6;\
    /* additional kernel-mode flags */\
    UINT32 AllowOpenInKernelMode:1;         /* allow kernel mode to open files when possible */\
    UINT32 CasePreservedExtendedAttributes:1;   /* preserve case of EA (default is UPPERCASE) */\
    UINT32 WslFeatures:1;                   /* support features required for WSLinux */\
    UINT32 KmReservedFlags:5;\
    WCHAR Prefix[FSP_FSCTL_VOLUME_PREFIX_SIZE / sizeof(WCHAR)]; /* UNC prefix (\Server\Share) */\
    WCHAR FileSystemName[FSP_FSCTL_VOLUME_FSNAME_SIZE / sizeof(WCHAR)];
#define FSP_FSCTL_VOLUME_PARAMS_V1_FIELD_DEFN\
    /* additional fields; specify .Version == sizeof(FSP_FSCTL_VOLUME_PARAMS) */\
    UINT32 VolumeInfoTimeoutValid:1;    /* VolumeInfoTimeout field is valid */\
    UINT32 DirInfoTimeoutValid:1;       /* DirInfoTimeout field is valid */\
    UINT32 SecurityTimeoutValid:1;      /* SecurityTimeout field is valid*/\
    UINT32 StreamInfoTimeoutValid:1;    /* StreamInfoTimeout field is valid */\
    UINT32 EaTimeoutValid:1;            /* EaTimeout field is valid */\
    UINT32 KmAdditionalReservedFlags:27;\
    UINT32 VolumeInfoTimeout;           /* volume info timeout (millis); overrides FileInfoTimeout */\
    UINT32 DirInfoTimeout;              /* dir info timeout (millis); overrides FileInfoTimeout */\
    UINT32 SecurityTimeout;             /* security info timeout (millis); overrides FileInfoTimeout */\
    UINT32 StreamInfoTimeout;           /* stream info timeout (millis); overrides FileInfoTimeout */\
    UINT32 EaTimeout;                   /* EA timeout (millis); overrides FileInfoTimeout */\
    UINT32 FsextControlCode;\
    UINT32 Reserved32[1];\
    UINT64 Reserved64[2];
typedef struct
{
    FSP_FSCTL_VOLUME_PARAMS_V0_FIELD_DEFN
    FSP_FSCTL_VOLUME_PARAMS_V1_FIELD_DEFN
} FSP_FSCTL_VOLUME_PARAMS;
_Static_assert(504 == sizeof(FSP_FSCTL_VOLUME_PARAMS),
    "sizeof(FSP_FSCTL_VOLUME_PARAMS) must be 504.");
#endif

typedef union
{
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    struct
    {
        CHAR VolumeName[(FSP_FSCTL_VOLUME_NAME_SIZEMAX / sizeof(WCHAR)) * 3 / 2];
        UINT64 VolumeId;
    };
} WSLFUSE_IOCTL_CREATEVOLUME_ARG;
#if defined(__linux__)
_Static_assert(504 == sizeof(WSLFUSE_IOCTL_CREATEVOLUME_ARG),
    "sizeof(WSLFUSE_IOCTL_CREATEVOLUME_ARG) must be 504.");
#endif

typedef struct
{
    CHAR WinMountPoint[260];
} WSLFUSE_IOCTL_WINMOUNT_ARG;
#if defined(__linux__)
_Static_assert(260 == sizeof(WSLFUSE_IOCTL_WINMOUNT_ARG),
    "sizeof(WSLFUSE_IOCTL_WINMOUNT_ARG) must be 260.");
#endif

typedef struct
{
    UINT8 Operation;
    UINT64 VolumeId;
    UINT64 LxMountId;
} WSLFUSE_IOCTL_LXMOUNT_ARG;
#if defined(__linux__)
_Static_assert(24 == sizeof(WSLFUSE_IOCTL_LXMOUNT_ARG),
    "sizeof(WSLFUSE_IOCTL_LXMOUNT_ARG) must be 16.");
#endif

/*
 * _IOWR('F', 'C', WSLFUSE_IOCTL_CREATEVOLUME_ARG)
 * sh tools/ioc.c 3 70 67 504
 */
#define WSLFUSE_IOCTL_CREATEVOLUME      0xc1f84643
#if defined(__linux__)
_Static_assert(WSLFUSE_IOCTL_CREATEVOLUME == _IOWR('F', 'C', WSLFUSE_IOCTL_CREATEVOLUME_ARG),
    "WSLFUSE_IOCTL_CREATEVOLUME");
#endif

/*
 * _IOW('F', 'M', WSLFUSE_IOCTL_WINMOUNT_ARG)
 * sh tools/ioc.c 1 70 77 260
 */
#define WSLFUSE_IOCTL_WINMOUNT          0x4104464d
#if defined(__linux__)
_Static_assert(WSLFUSE_IOCTL_WINMOUNT == _IOW('F', 'M', WSLFUSE_IOCTL_WINMOUNT_ARG),
    "WSLFUSE_IOCTL_WINMOUNT");
#endif

/*
 * _IOW('F', 'm', WSLFUSE_IOCTL_LXMOUNT_ARG)
 * sh tools/ioc.c 1 70 109 24
 */
#define WSLFUSE_IOCTL_LXMOUNT           0x4018466d
#if defined(__linux__)
_Static_assert(WSLFUSE_IOCTL_LXMOUNT == _IOW('F', 'm', WSLFUSE_IOCTL_LXMOUNT_ARG),
    "WSLFUSE_IOCTL_LXMOUNT");
#endif

#endif
