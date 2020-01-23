/**
 * @file kushared/wslfuse.h
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

#ifndef KUSHARED_WSLFUSE_H_INCLUDED
#define KUSHARED_WSLFUSE_H_INCLUDED

#if defined(__linux__)
#include <stdint.h>
#include <wchar.h>
typedef char CHAR;
typedef wchar_t WCHAR;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
#endif

/*
 * We duplicate some of the definitions from WinFsp and WinFuse
 * to avoid having either of them as a build dependency when
 * building on Linux.
 */

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

typedef struct
{
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    CHAR MountPoint[4096];
} WSLFUSE_IOCTL_MOUNT_ARG;

typedef struct
{
    CHAR MountPoint[4096];
} WSLFUSE_IOCTL_UNMOUNT_ARG;

/*
 * _IOW('F', 'M', sizeof(WSLFUSE_IOCTL_MOUNT_ARG))
 * sh tools/ioc.c 1 70 77 4600
 */
#define WSLFUSE_IOCTL_MOUNT             0x51f8464d

/*
 * _IOW('F', 'U', sizeof(WSLFUSE_IOCTL_UNMOUNT_ARG))
 * sh tools/ioc.c 1 70 85 4096
 */
#define WSLFUSE_IOCTL_UNMOUNT           0x50004655

#endif