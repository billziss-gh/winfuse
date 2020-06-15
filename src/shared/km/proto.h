/**
 * @file shared/km/proto.h
 *
 * This file is derived from libfuse/include/fuse_kernel.h:
 *     FUSE: Filesystem in Userspace
 *     Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
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

#ifndef SHARED_KM_PROTO_H_INCLUDED
#define SHARED_KM_PROTO_H_INCLUDED

#define FUSE_PROTO_VERSION              7
#define FUSE_PROTO_MINOR_VERSION        29

#define FUSE_PROTO_ROOT_INO             1

#define FUSE_PROTO_UNKNOWN_INO          0xffffffff

enum FUSE_PROTO_OPCODE
{
    FUSE_PROTO_OPCODE_LOOKUP            = 1,
    FUSE_PROTO_OPCODE_FORGET            = 2,
    FUSE_PROTO_OPCODE_GETATTR           = 3,
    FUSE_PROTO_OPCODE_SETATTR           = 4,
    FUSE_PROTO_OPCODE_READLINK          = 5,
    FUSE_PROTO_OPCODE_SYMLINK           = 6,
    FUSE_PROTO_OPCODE_MKNOD             = 8,
    FUSE_PROTO_OPCODE_MKDIR             = 9,
    FUSE_PROTO_OPCODE_UNLINK            = 10,
    FUSE_PROTO_OPCODE_RMDIR             = 11,
    FUSE_PROTO_OPCODE_RENAME            = 12,
    FUSE_PROTO_OPCODE_LINK              = 13,
    FUSE_PROTO_OPCODE_OPEN              = 14,
    FUSE_PROTO_OPCODE_READ              = 15,
    FUSE_PROTO_OPCODE_WRITE             = 16,
    FUSE_PROTO_OPCODE_STATFS            = 17,
    FUSE_PROTO_OPCODE_RELEASE           = 18,
    FUSE_PROTO_OPCODE_FSYNC             = 20,
    FUSE_PROTO_OPCODE_SETXATTR          = 21,
    FUSE_PROTO_OPCODE_GETXATTR          = 22,
    FUSE_PROTO_OPCODE_LISTXATTR         = 23,
    FUSE_PROTO_OPCODE_REMOVEXATTR       = 24,
    FUSE_PROTO_OPCODE_FLUSH             = 25,
    FUSE_PROTO_OPCODE_INIT              = 26,
    FUSE_PROTO_OPCODE_OPENDIR           = 27,
    FUSE_PROTO_OPCODE_READDIR           = 28,
    FUSE_PROTO_OPCODE_RELEASEDIR        = 29,
    FUSE_PROTO_OPCODE_FSYNCDIR          = 30,
    FUSE_PROTO_OPCODE_GETLK             = 31,
    FUSE_PROTO_OPCODE_SETLK             = 32,
    FUSE_PROTO_OPCODE_SETLKW            = 33,
    FUSE_PROTO_OPCODE_ACCESS            = 34,
    FUSE_PROTO_OPCODE_CREATE            = 35,
    FUSE_PROTO_OPCODE_INTERRUPT         = 36,
    FUSE_PROTO_OPCODE_BMAP              = 37,
    FUSE_PROTO_OPCODE_DESTROY           = 38,
    FUSE_PROTO_OPCODE_IOCTL             = 39,
    FUSE_PROTO_OPCODE_POLL              = 40,
    FUSE_PROTO_OPCODE_NOTIFY_REPLY      = 41,
    FUSE_PROTO_OPCODE_BATCH_FORGET      = 42,
    FUSE_PROTO_OPCODE_FALLOCATE         = 43,
    FUSE_PROTO_OPCODE_READDIRPLUS       = 44,
    FUSE_PROTO_OPCODE_RENAME2           = 45,
    FUSE_PROTO_OPCODE_LSEEK             = 46,
    FUSE_PROTO_OPCODE_COPY_FILE_RANGE   = 47,
};

enum FUSE_PROTO_NOTIFY_CODE
{
    FUSE_PROTO_NOTIFY_POLL              = 1,
    FUSE_PROTO_NOTIFY_INVAL_INODE       = 2,
    FUSE_PROTO_NOTIFY_INVAL_ENTRY       = 3,
    FUSE_PROTO_NOTIFY_STORE             = 4,
    FUSE_PROTO_NOTIFY_RETRIEVE          = 5,
    FUSE_PROTO_NOTIFY_DELETE            = 6,
    FUSE_PROTO_NOTIFY_CODE_MAX,
};

enum
{
    FUSE_PROTO_SETATTR_MODE             = (1 << 0),
    FUSE_PROTO_SETATTR_UID              = (1 << 1),
    FUSE_PROTO_SETATTR_GID              = (1 << 2),
    FUSE_PROTO_SETATTR_SIZE             = (1 << 3),
    FUSE_PROTO_SETATTR_ATIME            = (1 << 4),
    FUSE_PROTO_SETATTR_MTIME            = (1 << 5),
    FUSE_PROTO_SETATTR_FH               = (1 << 6),
    FUSE_PROTO_SETATTR_ATIME_NOW        = (1 << 7),
    FUSE_PROTO_SETATTR_MTIME_NOW        = (1 << 8),
    FUSE_PROTO_SETATTR_LOCKOWNER        = (1 << 9),
    FUSE_PROTO_SETATTR_CTIME            = (1 << 10),

    FUSE_PROTO_OPEN_DIRECT_IO           = (1 << 0),
    FUSE_PROTO_OPEN_KEEP_CACHE          = (1 << 1),
    FUSE_PROTO_OPEN_NONSEEKABLE         = (1 << 2),
    FUSE_PROTO_OPEN_CACHE_DIR           = (1 << 3),

    FUSE_PROTO_GETATTR_FH               = (1 << 0),

    FUSE_PROTO_INIT_ASYNC_READ          = (1 << 0),
    FUSE_PROTO_INIT_POSIX_LOCKS         = (1 << 1),
    FUSE_PROTO_INIT_FILE_OPS            = (1 << 2),
    FUSE_PROTO_INIT_ATOMIC_O_TRUNC      = (1 << 3),
    FUSE_PROTO_INIT_EXPORT_SUPPORT      = (1 << 4),
    FUSE_PROTO_INIT_BIG_WRITES          = (1 << 5),
    FUSE_PROTO_INIT_DONT_MASK           = (1 << 6),
    FUSE_PROTO_INIT_SPLICE_WRITE        = (1 << 7),
    FUSE_PROTO_INIT_SPLICE_MOVE         = (1 << 8),
    FUSE_PROTO_INIT_SPLICE_READ         = (1 << 9),
    FUSE_PROTO_INIT_FLOCK_LOCKS         = (1 << 10),
    FUSE_PROTO_INIT_HAS_IOCTL_DIR       = (1 << 11),
    FUSE_PROTO_INIT_AUTO_INVAL_DATA     = (1 << 12),
    FUSE_PROTO_INIT_DO_READDIRPLUS      = (1 << 13),
    FUSE_PROTO_INIT_READDIRPLUS_AUTO    = (1 << 14),
    FUSE_PROTO_INIT_ASYNC_DIO           = (1 << 15),
    FUSE_PROTO_INIT_WRITEBACK_CACHE     = (1 << 16),
    FUSE_PROTO_INIT_NO_OPEN_SUPPORT     = (1 << 17),
    FUSE_PROTO_INIT_PARALLEL_DIROPS     = (1 << 18),
    FUSE_PROTO_INIT_HANDLE_KILLPRIV     = (1 << 19),
    FUSE_PROTO_INIT_POSIX_ACL           = (1 << 20),
    FUSE_PROTO_INIT_ABORT_ERROR         = (1 << 21),
    FUSE_PROTO_INIT_MAX_PAGES           = (1 << 22),
    FUSE_PROTO_INIT_CACHE_SYMLINKS      = (1 << 23),
    FUSE_PROTO_INIT_NO_OPENDIR_SUPPORT  = (1 << 24),

    FUSE_PROTO_IOCTL_COMPAT             = (1 << 0),
    FUSE_PROTO_IOCTL_UNRESTRICTED       = (1 << 1),
    FUSE_PROTO_IOCTL_RETRY              = (1 << 2),
    FUSE_PROTO_IOCTL_32BIT              = (1 << 3),
    FUSE_PROTO_IOCTL_DIR                = (1 << 4),

    FUSE_PROTO_LK_FLOCK                 = (1 << 0),

    FUSE_PROTO_POLL_SCHEDULE_NOTIFY     = (1 << 0),

    FUSE_PROTO_READ_LOCKOWNER           = (1 << 1),

    FUSE_PROTO_RELEASE_FLUSH            = (1 << 0),
    FUSE_PROTO_RELEASE_FLOCK_UNLOCK     = (1 << 1),

    FUSE_PROTO_WRITE_CACHE              = (1 << 0),
    FUSE_PROTO_WRITE_LOCKOWNER          = (1 << 1),
};

enum
{
    FUSE_PROTO_UTIME_NOW                = ((1 << 30) - 1),
    FUSE_PROTO_UTIME_OMIT               = ((1 << 30) - 2),
};

typedef struct
{
    UINT64 blocks;
    UINT64 bfree;
    UINT64 bavail;
    UINT64 files;
    UINT64 ffree;
    UINT32 bsize;
    UINT32 namelen;
    UINT32 frsize;
    UINT32 padding;
    UINT32 spare[6];
} FUSE_PROTO_STATFS;

typedef struct
{
    UINT64 ino;
    UINT64 size;
    UINT64 blocks;
    UINT64 atime;
    UINT64 mtime;
    UINT64 ctime;
    UINT32 atimensec;
    UINT32 mtimensec;
    UINT32 ctimensec;
    UINT32 mode;
    UINT32 nlink;
    UINT32 uid;
    UINT32 gid;
    UINT32 rdev;
    UINT32 blksize;
    UINT32 padding;
} FUSE_PROTO_ATTR;

typedef struct
{
    UINT64 nodeid;
    UINT64 generation;
    UINT64 entry_valid;
    UINT64 attr_valid;
    UINT32 entry_valid_nsec;
    UINT32 attr_valid_nsec;
    FUSE_PROTO_ATTR attr;
} FUSE_PROTO_ENTRY;

typedef struct
{
    UINT64 ino;
    UINT64 off;
    UINT32 namelen;
    UINT32 type;
    CHAR name[];
} FUSE_PROTO_DIRENT;

typedef struct
{
    FUSE_PROTO_ENTRY entry;
    FUSE_PROTO_DIRENT dirent;
} FUSE_PROTO_DIRENTPLUS;

typedef struct
{
    UINT64 start;
    UINT64 end;
    UINT32 type;
    UINT32 pid;
} FUSE_PROTO_FILE_LOCK;

typedef struct
{
    UINT64 base;
    UINT64 len;
} FUSE_PROTO_IOCTL_IOVEC;

typedef struct
{
    UINT64 nodeid;
    UINT64 nlookup;
} FUSE_PROTO_FORGET_ONE;

typedef struct
{
    UINT32 len;
    UINT32 opcode;
    UINT64 unique;
    UINT64 nodeid;
    UINT32 uid;
    UINT32 gid;
    UINT32 pid;
    UINT32 padding;
    union
    {
        struct
        {
            /* LOOKUP */
            CHAR name[];
        } lookup;
        struct
        {
            /* FORGET */
            UINT64 nlookup;
        } forget;
        struct
        {
            /* GETATTR */
            UINT32 getattr_flags;
            UINT32 dummy;
            UINT64 fh;
        } getattr;
        struct
        {
            /* SETATTR */
            UINT32 valid;
            UINT32 padding;
            UINT64 fh;
            UINT64 size;
            UINT64 lock_owner;
            UINT64 atime;
            UINT64 mtime;
            UINT64 ctime;
            UINT32 atimensec;
            UINT32 mtimensec;
            UINT32 ctimensec;
            UINT32 mode;
            UINT32 unused4;
            UINT32 uid;
            UINT32 gid;
            UINT32 unused5;
        } setattr;
        struct
        {
            /* SYMLINK */
            CHAR name[]; /* 2 names */
        } symlink;
        struct
        {
            /* MKNOD */
            UINT32 mode;
            UINT32 rdev;
            UINT32 umask;
            UINT32 padding;
            CHAR name[];
        } mknod;
        struct
        {
            /* MKDIR */
            UINT32 mode;
            UINT32 umask;
            CHAR name[];
        } mkdir;
        struct
        {
            /* UNLINK */
            CHAR name[];
        } unlink;
        struct
        {
            /* RMDIR */
            CHAR name[];
        } rmdir;
        struct
        {
            /* RENAME */
            UINT64 newdir;
            CHAR name[]; /* 2 names */
        } rename;
        struct
        {
            /* LINK */
            UINT64 oldnodeid;
            CHAR name[];
        } link;
        struct
        {
            /* OPEN, OPENDIR */
            UINT32 flags;
            UINT32 unused;
        } open;
        struct
        {
            /* READ, READDIR, READDIRPLUS */
            UINT64 fh;
            UINT64 offset;
            UINT32 size;
            UINT32 read_flags;
            UINT64 lock_owner;
            UINT32 flags;
            UINT32 padding;
        } read;
        struct
        {
            /* WRITE */
            UINT64 fh;
            UINT64 offset;
            UINT32 size;
            UINT32 write_flags;
            UINT64 lock_owner;
            UINT32 flags;
            UINT32 padding;
        } write;
        struct
        {
            /* RELEASE, RELEASEDIR */
            UINT64 fh;
            UINT32 flags;
            UINT32 release_flags;
            UINT64 lock_owner;
        } release;
        struct
        {
            /* FSYNC, FSYNCDIR */
            UINT64 fh;
            UINT32 fsync_flags;
            UINT32 padding;
        } fsync;
        struct
        {
            /* SETXATTR */
            UINT32 size;
            UINT32 flags;
            CHAR name[];
        } setxattr;
        struct
        {
            /* GETXATTR */
            UINT32 size;
            UINT32 padding;
            CHAR name[];
        } getxattr;
        struct
        {
            /* LISTXATTR */
            UINT32 size;
            UINT32 padding;
        } listxattr;
        struct
        {
            /* REMOVEXATTR */
            CHAR name[];
        } removexattr;
        struct
        {
            /* FLUSH */
            UINT64 fh;
            UINT32 unused;
            UINT32 padding;
            UINT64 lock_owner;
        } flush;
        struct
        {
            /* INIT */
            UINT32 major;
            UINT32 minor;
            UINT32 max_readahead;
            UINT32 flags;
        } init;
        struct
        {
            /* GETLK, SETLK, SETLKW */
            UINT64 fh;
            UINT64 owner;
            FUSE_PROTO_FILE_LOCK lk;
            UINT32 lk_flags;
            UINT32 padding;
        } lk;
        struct
        {
            /* ACCESS */
            UINT32 mask;
            UINT32 padding;
        } access;
        struct
        {
            /* CREATE */
            UINT32 flags;
            UINT32 mode;
            UINT32 umask;
            UINT32 padding;
            CHAR name[];
        } create;
        struct
        {
            /* INTERRUPT */
            UINT64 unique;
        } interrupt;
        struct
        {
            /* BMAP */
            UINT64 block;
            UINT32 blocksize;
            UINT32 padding;
        } bmap;
        struct
        {
            /* IOCTL */
            UINT64 fh;
            UINT32 flags;
            UINT32 cmd;
            UINT64 arg;
            UINT32 in_size;
            UINT32 out_size;
        } ioctl;
        struct
        {
            /* POLL */
            UINT64 fh;
            UINT64 kh;
            UINT32 flags;
            UINT32 events;
        } poll;
        struct
        {
            /* NOTIFY_REPLY */
            UINT64 dummy1;
            UINT64 offset;
            UINT32 size;
            UINT32 dummy2;
            UINT64 dummy3;
            UINT64 dummy4;
        } notify_reply;
        struct
        {
            /* BATCH_FORGET */
            UINT32 count;
            UINT32 dummy;
        } batch_forget;
        struct
        {
            /* FALLOCATE */
            UINT64 fh;
            UINT64 offset;
            UINT64 length;
            UINT32 mode;
            UINT32 padding;
        } fallocate;
        struct
        {
            /* RENAME2 */
            UINT64 newdir;
            UINT32 flags;
            UINT32 padding;
            CHAR name[]; /* 2 names */
        } rename2;
        struct
        {
            /* LSEEK */
            UINT64 fh;
            UINT64 offset;
            UINT32 whence;
            UINT32 padding;
        } lseek;
        struct
        {
            /* COPY_FILE_RANGE */
            UINT64 fh_in;
            UINT64 off_in;
            UINT64 nodeid_out;
            UINT64 fh_out;
            UINT64 off_out;
            UINT64 len;
            UINT64 flags;
        } copy_file_range;
    } req;
} FUSE_PROTO_REQ;
#define FUSE_PROTO_REQ_HEADER_SIZE      ((ULONG)FIELD_OFFSET(FUSE_PROTO_REQ, req))
#define FUSE_PROTO_REQ_SIZEMIN          8192 // FUSE_MIN_READ_BUFFER
#define FUSE_PROTO_REQ_SIZE(F)          RTL_SIZEOF_THROUGH_FIELD(FUSE_PROTO_REQ, req.F)

typedef struct
{
    UINT32 len;
    INT32 error;
    UINT64 unique;
    union
    {
        struct
        {
            /* LOOKUP */
            FUSE_PROTO_ENTRY entry;
        } lookup;
        struct
        {
            /* GETATTR */
            UINT64 attr_valid;
            UINT32 attr_valid_nsec;
            UINT32 dummy;
            FUSE_PROTO_ATTR attr;
        } getattr;
        struct
        {
            /* SETATTR */
            UINT64 attr_valid;
            UINT32 attr_valid_nsec;
            UINT32 dummy;
            FUSE_PROTO_ATTR attr;
        } setattr;
        struct
        {
            /* SYMLINK */
            FUSE_PROTO_ENTRY entry;
        } symlink;
        struct
        {
            /* MKNOD */
            FUSE_PROTO_ENTRY entry;
        } mknod;
        struct
        {
            /* MKDIR */
            FUSE_PROTO_ENTRY entry;
        } mkdir;
        struct
        {
            /* LINK */
            FUSE_PROTO_ENTRY entry;
        } link;
        struct
        {
            /* OPEN, OPENDIR */
            UINT64 fh;
            UINT32 open_flags;
            UINT32 padding;
        } open;
        struct
        {
            /* WRITE */
            UINT32 size;
            UINT32 padding;
        } write;
        struct
        {
            /* STATFS */
            FUSE_PROTO_STATFS st;
        } statfs;
        struct
        {
            /* GETXATTR */
            UINT32 size;
            UINT32 padding;
        } getxattr;
        struct
        {
            /* LISTXATTR */
            UINT32 size;
            UINT32 padding;
        } listxattr;
        struct
        {
            /* INIT */
            UINT32 major;
            UINT32 minor;
            UINT32 max_readahead;
            UINT32 flags;
            UINT16 max_background;
            UINT16 congestion_threshold;
            UINT32 max_write;
            UINT32 time_gran;
            UINT16 max_pages;
            UINT16 padding;
            UINT32 unused[8];
        } init;
        struct
        {
            /* GETLK */
            FUSE_PROTO_FILE_LOCK lk;
        } lk;
        struct
        {
            /* CREATE */
            FUSE_PROTO_ENTRY entry;
            UINT64 fh;
            UINT32 open_flags;
            UINT32 padding;
        } create;
        struct
        {
            /* BMAP */
            UINT64 block;
        } bmap;
        struct
        {
            /* IOCTL */
            INT32 result;
            UINT32 flags;
            UINT32 in_iovs;
            UINT32 out_iovs;
        } ioctl;
        struct
        {
            /* POLL */
            UINT32 revents;
            UINT32 padding;
        } poll;
        struct
        {
            /* LSEEK */
            UINT64 offset;
        } lseek;
        struct
        {
            /* COPY_FILE_RANGE */
            UINT32 size;
            UINT32 padding;
        } copy_file_range;
        /* notify */
        struct
        {
            /* NOTIFY_POLL */
            UINT64 kh;
        } notify_poll;
        struct
        {
            /* NOTIFY_INVAL_INODE */
            UINT64 ino;
            INT64 off;
            INT64 len;
        } notify_inval_inode;
        struct
        {
            /* NOTIFY_INVAL_ENTRY */
            UINT64 parent;
            UINT32 namelen;
            UINT32 padding;
        } notify_inval_entry;
        struct
        {
            /* NOTIFY_STORE */
            UINT64 nodeid;
            UINT64 offset;
            UINT32 size;
            UINT32 padding;
        } notify_store;
        struct
        {
            /* NOTIFY_RETRIEVE */
            UINT64 notify_unique;
            UINT64 nodeid;
            UINT64 offset;
            UINT32 size;
            UINT32 padding;
        } notify_retrieve;
        struct
        {
            /* NOTIFY_DELETE */
            UINT64 parent;
            UINT64 child;
            UINT32 namelen;
            UINT32 padding;
        } notify_delete;
    } rsp;
} FUSE_PROTO_RSP;
#define FUSE_PROTO_RSP_HEADER_SIZE      ((ULONG)FIELD_OFFSET(FUSE_PROTO_RSP, rsp))
#define FUSE_PROTO_RSP_SIZE(F)          RTL_SIZEOF_THROUGH_FIELD(FUSE_PROTO_RSP, rsp.F)

#endif
