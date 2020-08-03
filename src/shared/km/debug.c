/**
 * @file shared/km/debug.c
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

#include <shared/km/shared.h>

#if DBG
ULONG DebugRandom(VOID)
{
    static KSPIN_LOCK SpinLock = 0;
    static ULONG Seed = 1;
    KIRQL Irql;
    ULONG Result;

    KeAcquireSpinLock(&SpinLock, &Irql);

    /* see ucrt sources */
    Seed = Seed * 214013 + 2531011;
    Result = (Seed >> 16) & 0x7fff;

    KeReleaseSpinLock(&SpinLock, Irql);

    return Result;
}

BOOLEAN DebugMemory(PVOID Memory, SIZE_T Size, BOOLEAN Test)
{
    BOOLEAN Result = TRUE;
    if (!Test)
    {
        for (SIZE_T I = 0, N = Size / sizeof(UINT32); N > I; I++)
        {
            ((PUINT8)Memory)[0] = 0x0B;
            ((PUINT8)Memory)[1] = 0xAD;
            ((PUINT8)Memory)[2] = 0xF0;
            ((PUINT8)Memory)[3] = 0x0D;
            Memory = (PUINT8)Memory + sizeof(UINT32);
        }
    }
    else
    {
        for (SIZE_T I = 0, N = Size / sizeof(UINT32); N > I; I++)
        {
            if (
                ((PUINT8)Memory)[0] == 0x0B &&
                ((PUINT8)Memory)[1] == 0xAD &&
                ((PUINT8)Memory)[2] == 0xF0 &&
                ((PUINT8)Memory)[3] == 0x0D
                )
            {
                Result = FALSE;
                break;
            }
            Memory = (PUINT8)Memory + sizeof(UINT32);
        }
    }
    return Result;
}

int _snprintf(char *buf, size_t siz, const char *fmt, ...);

static const char *FuseErrnoSignSym(INT32 Errno)
{
    if (0 < Errno)
        return "+";
    else if (0 > Errno)
        return "-";
    else
        return "";
}

static const char *FuseErrnoSym(FUSE_INSTANCE_TYPE InstanceType, INT32 Errno)
{
    if (0 > Errno)
        Errno = -Errno;

    switch (InstanceType)
    {
    default:
    case FuseInstanceWindows:
        switch (Errno)
        {
        #undef FUSE_ERRNO
        #define FUSE_ERRNO 87
        #include "errnosym.i"
        default:
            return "errno:UNKNOWN";
        }
    case FuseInstanceCygwin:
        switch (Errno)
        {
        #undef FUSE_ERRNO
        #define FUSE_ERRNO 67
        #include "errnosym.i"
        default:
            return "errno:UNKNOWN";
        }
    case FuseInstanceLinux:
        switch (Errno)
        {
        #undef FUSE_ERRNO
        #define FUSE_ERRNO 76
        #include "errnosym.i"
        default:
            return "errno:UNKNOWN";
        }
    }
}

static const char *FuseOpcodeSym(UINT32 Opcode)
{
#define SYM(x)                          case FUSE_PROTO_OPCODE_ ## x: return #x;

    switch (Opcode)
    {
    case 0:
        return "EMPTY";
    SYM(LOOKUP)
    SYM(FORGET)
    SYM(GETATTR)
    SYM(SETATTR)
    SYM(READLINK)
    SYM(SYMLINK)
    SYM(MKNOD)
    SYM(MKDIR)
    SYM(UNLINK)
    SYM(RMDIR)
    SYM(RENAME)
    SYM(LINK)
    SYM(OPEN)
    SYM(READ)
    SYM(WRITE)
    SYM(STATFS)
    SYM(RELEASE)
    SYM(FSYNC)
    SYM(SETXATTR)
    SYM(GETXATTR)
    SYM(LISTXATTR)
    SYM(REMOVEXATTR)
    SYM(FLUSH)
    SYM(INIT)
    SYM(OPENDIR)
    SYM(READDIR)
    SYM(RELEASEDIR)
    SYM(FSYNCDIR)
    SYM(GETLK)
    SYM(SETLK)
    SYM(SETLKW)
    SYM(ACCESS)
    SYM(CREATE)
    SYM(INTERRUPT)
    SYM(BMAP)
    SYM(DESTROY)
    SYM(IOCTL)
    SYM(POLL)
    SYM(NOTIFY_REPLY)
    SYM(BATCH_FORGET)
    SYM(FALLOCATE)
    SYM(READDIRPLUS)
    SYM(RENAME2)
    SYM(LSEEK)
    SYM(COPY_FILE_RANGE)
    default:
        return "INVALID";
    }

#undef SYM
}

static const char *FuseDebugLogTimeString(UINT64 time, UINT32 timensec, char *Buf)
{
    UINT64 FileTime;
    TIME_FIELDS TimeFields;

    if (0 == time && 0 == timensec)
        Buf[0] = '0', Buf[1] = '\0';
    else
    {
        FuseUnixTimeToFileTime(time, timensec, &FileTime);
        RtlTimeToTimeFields((PLARGE_INTEGER)&FileTime, &TimeFields);
        _snprintf(Buf, 32, "%04hu-%02hu-%02huT%02hu:%02hu:%02hu.%03huZ",
            TimeFields.Year, TimeFields.Month, TimeFields.Day,
            TimeFields.Hour, TimeFields.Minute, TimeFields.Second,
            TimeFields.Milliseconds);
    }

    return Buf;
}

static const char *FuseDebugLogAttrString(FUSE_PROTO_ATTR *Attr, char *Buf)
{
    char atimebuf[32], mtimebuf[32], ctimebuf[32];
    _snprintf(Buf, 512,
        "{ino=%llu, size=%llu, blocks=%llu, atime=%s, mtime=%s, ctime=%s, "
            "mode=0%06o, nlink=%u, uid=%u, gid=%u, rdev=0x%x, blksize=%u}",
        Attr->ino,
        Attr->size,
        Attr->blocks,
        FuseDebugLogTimeString(Attr->atime, Attr->atimensec, atimebuf),
        FuseDebugLogTimeString(Attr->mtime, Attr->mtimensec, mtimebuf),
        FuseDebugLogTimeString(Attr->ctime, Attr->ctimensec, ctimebuf),
        Attr->mode,
        Attr->nlink,
        Attr->uid,
        Attr->gid,
        Attr->rdev,
        Attr->blksize);
    return Buf;
}

static const char *FuseDebugLogEntryString(FUSE_PROTO_ENTRY *Entry, char *Buf)
{
    int Len0, Len1;
    Len0 = _snprintf(Buf, 256,
        "{nodeid=%llu, generation=%llu, entry_valid=%llu, entry_valid_nsec=%u, "
            "attr_valid=%llu, attr_valid_nsec=%u, attr=",
        Entry->nodeid,
        Entry->generation,
        Entry->entry_valid,
        Entry->entry_valid_nsec,
        Entry->attr_valid,
        Entry->attr_valid_nsec);
    FuseDebugLogAttrString(&Entry->attr, Buf + Len0);
    Len1 = Len0 + (int)strlen(Buf + Len0);
    Buf[Len1] = '}';
    Buf[Len1 + 1] = '\0';
    return Buf;
}

VOID FuseDebugLogRequest(FUSE_PROTO_REQ *Request)
{
#define LOG(fmt, ...)                   \
    DbgPrint("[%d] FUSE: %p[%06x]: <<%s[ino=%llu,uid=%u:%u,pid=%u] " fmt "\n",\
        KeGetCurrentIrql(),             \
        (PVOID)(UINT_PTR)Request->unique,\
        Request->len,                   \
        FuseOpcodeSym(Request->opcode), \
        Request->nodeid,                \
        Request->uid,                   \
        Request->gid,                   \
        Request->pid,                   \
        __VA_ARGS__)

    if (0 == Request->len)
    {
        LOG("", 0);
        return;
    }

    FUSE_CONTEXT *Context = (PVOID)(UINT_PTR)Request->unique;
    0 != Context ? Context->DebugLogOpcode = Request->opcode : 0;

    char atimebuf[32], mtimebuf[32], ctimebuf[32];
    switch (Request->opcode)
    {
    case FUSE_PROTO_OPCODE_LOOKUP:
        LOG("name=\"%s\"",
            Request->req.lookup.name);
        break;
    case FUSE_PROTO_OPCODE_FORGET:
        LOG("nlookup=%llu",
            Request->req.forget.nlookup);
        break;
    case FUSE_PROTO_OPCODE_GETATTR:
        LOG("fh=%llu, flags=%u",
            Request->req.getattr.fh,
            Request->req.getattr.getattr_flags);
        break;
    case FUSE_PROTO_OPCODE_SETATTR:
        LOG("fh=%llu, valid=%x, size=%llu, atime=%s, mtime=%s, ctime=%s, mode=0%03o, uid=%u, gid=%u",
            Request->req.setattr.fh,
            Request->req.setattr.valid,
            Request->req.setattr.size,
            FuseDebugLogTimeString(Request->req.setattr.atime, Request->req.setattr.atimensec, atimebuf),
            FuseDebugLogTimeString(Request->req.setattr.mtime, Request->req.setattr.mtimensec, mtimebuf),
            FuseDebugLogTimeString(Request->req.setattr.ctime, Request->req.setattr.ctimensec, ctimebuf),
            Request->req.setattr.mode,
            Request->req.setattr.uid,
            Request->req.setattr.gid);
        break;
    case FUSE_PROTO_OPCODE_SYMLINK:
        LOG("name=\"%s\", target=\"%s\"",
            Request->req.symlink.name,
            Request->req.symlink.name + strlen(Request->req.symlink.name) + 1);
        break;
    case FUSE_PROTO_OPCODE_MKNOD:
        LOG("name=\"%s\", mode=0%06o, rdev=0x%x, umask=0%03o",
            Request->req.mknod.name,
            Request->req.mknod.mode,
            Request->req.mknod.rdev,
            Request->req.mknod.umask);
        break;
    case FUSE_PROTO_OPCODE_MKDIR:
        LOG("name=\"%s\", mode=0%06o, umask=0%03o",
            Request->req.mkdir.name,
            Request->req.mkdir.mode,
            Request->req.mkdir.umask);
        break;
    case FUSE_PROTO_OPCODE_UNLINK:
        LOG("name=\"%s\"",
            Request->req.unlink.name);
        break;
    case FUSE_PROTO_OPCODE_RMDIR:
        LOG("name=\"%s\"",
            Request->req.rmdir.name);
        break;
    case FUSE_PROTO_OPCODE_RENAME:
        LOG("oldname=\"%s\", newdir=%llu, newname=\"%s\"",
            Request->req.rename.name,
            Request->req.rename.newdir,
            Request->req.rename.name + strlen(Request->req.rename.name) + 1);
        break;
    case FUSE_PROTO_OPCODE_LINK:
        LOG("oldino=%llu, name=\"%s\"",
            Request->req.link.oldnodeid,
            Request->req.link.name);
        break;
    case FUSE_PROTO_OPCODE_OPEN:
    case FUSE_PROTO_OPCODE_OPENDIR:
        LOG("open_flags=0x%x",
            Request->req.open.flags);
        break;
    case FUSE_PROTO_OPCODE_READ:
    case FUSE_PROTO_OPCODE_READDIR:
    case FUSE_PROTO_OPCODE_READDIRPLUS:
        LOG("fh=%llu, offset=%llu, size=%u, open_flags=0x%x",
            Request->req.read.fh,
            Request->req.read.offset,
            Request->req.read.size,
            Request->req.read.flags);
        break;
    case FUSE_PROTO_OPCODE_WRITE:
        LOG("fh=%llu, offset=%llu, size=%u, open_flags=0x%x",
            Request->req.write.fh,
            Request->req.write.offset,
            Request->req.write.size,
            Request->req.write.flags);
        break;
    case FUSE_PROTO_OPCODE_RELEASE:
    case FUSE_PROTO_OPCODE_RELEASEDIR:
        LOG("fh=%llu, flags=0x%x, open_flags=0x%x",
            Request->req.release.fh,
            Request->req.release.release_flags,
            Request->req.release.flags);
        break;
    case FUSE_PROTO_OPCODE_FSYNC:
    case FUSE_PROTO_OPCODE_FSYNCDIR:
        LOG("fh=%llu, flags=0x%x",
            Request->req.fsync.fh,
            Request->req.fsync.fsync_flags);
        break;
    case FUSE_PROTO_OPCODE_SETXATTR:
        LOG("name=\"%s\", size=%u, flags=0x%x",
            Request->req.setxattr.name,
            Request->req.setxattr.size,
            Request->req.setxattr.flags);
        break;
    case FUSE_PROTO_OPCODE_GETXATTR:
        LOG("name=\"%s\", size=%u",
            Request->req.getxattr.name,
            Request->req.getxattr.size);
        break;
    case FUSE_PROTO_OPCODE_LISTXATTR:
        LOG("size=%u",
            Request->req.listxattr.size);
        break;
    case FUSE_PROTO_OPCODE_REMOVEXATTR:
        LOG("name=\"%s\"",
            Request->req.removexattr.name);
        break;
    case FUSE_PROTO_OPCODE_FLUSH:
        LOG("fh=%llu",
            Request->req.flush.fh);
        break;
    case FUSE_PROTO_OPCODE_INIT:
        LOG("major=%u, minor=%u, flags=0x%x, max_readahead=%u",
            Request->req.init.major,
            Request->req.init.minor,
            Request->req.init.flags,
            Request->req.init.max_readahead);
        break;
    case FUSE_PROTO_OPCODE_ACCESS:
        LOG("mask=0%o",
            Request->req.access.mask);
        break;
    case FUSE_PROTO_OPCODE_CREATE:
        LOG("name=\"%s\", mode=0%06o, umask=0%03o, open_flags=0x%x",
            Request->req.create.name,
            Request->req.create.mode,
            Request->req.create.umask,
            Request->req.create.flags);
        break;
    case FUSE_PROTO_OPCODE_BATCH_FORGET:
        LOG("count=%u",
            Request->req.batch_forget.count);
        break;
    default:
        LOG("", 0);
        break;
    }

#undef LOG
}

VOID FuseDebugLogResponse(FUSE_PROTO_RSP *Response)
{
#define LOG(fmt, ...)                   \
    DbgPrint("[%d] FUSE: %p[%06x]: >>%s[err=%s%s] " fmt "\n",\
        KeGetCurrentIrql(),             \
        (PVOID)(UINT_PTR)Response->unique,\
        Response->len,                  \
        FuseOpcodeSym(Opcode),          \
        FuseErrnoSignSym(Response->error),\
        FuseErrnoSym(InstanceType, Response->error),\
        __VA_ARGS__)

    FUSE_CONTEXT *Context = (PVOID)(UINT_PTR)Response->unique;
    FUSE_INSTANCE_TYPE InstanceType = 0 != Context ? Context->Instance->InstanceType : FuseInstanceWindows;
    UINT32 Opcode = 0 != Context ? Context->DebugLogOpcode : 0;

    if (0 != Response->error)
    {
        LOG("", 0);
        return;
    }

    char entrybuf[256 + 512];
    switch (Opcode)
    {
    case FUSE_PROTO_OPCODE_LOOKUP:
        LOG("entry=%s",
            FuseDebugLogEntryString(&Response->rsp.lookup.entry, entrybuf));
        break;
    case FUSE_PROTO_OPCODE_GETATTR:
        LOG("attr_valid=%llu, attr_valid_nsec=%u, attr=%s",
            Response->rsp.getattr.attr_valid,
            Response->rsp.getattr.attr_valid_nsec,
            FuseDebugLogAttrString(&Response->rsp.getattr.attr, entrybuf));
        break;
    case FUSE_PROTO_OPCODE_SETATTR:
        LOG("attr_valid=%llu, attr_valid_nsec=%u, attr=%s",
            Response->rsp.setattr.attr_valid,
            Response->rsp.setattr.attr_valid_nsec,
            FuseDebugLogAttrString(&Response->rsp.setattr.attr, entrybuf));
        break;
    case FUSE_PROTO_OPCODE_SYMLINK:
        LOG("entry=%s",
            FuseDebugLogEntryString(&Response->rsp.symlink.entry, entrybuf));
        break;
    case FUSE_PROTO_OPCODE_MKNOD:
        LOG("entry=%s",
            FuseDebugLogEntryString(&Response->rsp.mknod.entry, entrybuf));
        break;
    case FUSE_PROTO_OPCODE_MKDIR:
        LOG("entry=%s",
            FuseDebugLogEntryString(&Response->rsp.mkdir.entry, entrybuf));
        break;
    case FUSE_PROTO_OPCODE_LINK:
        LOG("entry=%s",
            FuseDebugLogEntryString(&Response->rsp.link.entry, entrybuf));
        break;
    case FUSE_PROTO_OPCODE_OPEN:
    case FUSE_PROTO_OPCODE_OPENDIR:
        LOG("fh=%llu, open_flags=0x%x",
            Response->rsp.open.fh,
            Response->rsp.open.open_flags);
        break;
    case FUSE_PROTO_OPCODE_WRITE:
        LOG("size=%u",
            Response->rsp.write.size);
        break;
    case FUSE_PROTO_OPCODE_STATFS:
        LOG("st={blocks=%llu, bfree=%llu, bavail=%llu, files=%llu, ffree=%llu, bsize=%u, frsize=%u, namelen=%u}",
            Response->rsp.statfs.st.blocks,
            Response->rsp.statfs.st.bfree,
            Response->rsp.statfs.st.bavail,
            Response->rsp.statfs.st.files,
            Response->rsp.statfs.st.ffree,
            Response->rsp.statfs.st.bsize,
            Response->rsp.statfs.st.frsize,
            Response->rsp.statfs.st.namelen);
        break;
    case FUSE_PROTO_OPCODE_GETXATTR:
        LOG("size=%u",
            Response->rsp.getxattr.size);
        break;
    case FUSE_PROTO_OPCODE_LISTXATTR:
        LOG("size=%u",
            Response->rsp.listxattr.size);
        break;
    case FUSE_PROTO_OPCODE_INIT:
        LOG("major=%u, minor=%u, flags=0x%x, max_readahead=%u, max_write=%u",
            Response->rsp.init.major,
            Response->rsp.init.minor,
            Response->rsp.init.flags,
            Response->rsp.init.max_readahead,
            Response->rsp.init.max_write);
        break;
    case FUSE_PROTO_OPCODE_CREATE:
        LOG("entry=%s, fh=%llu, open_flags=0x%x",
            FuseDebugLogEntryString(&Response->rsp.create.entry, entrybuf),
            Response->rsp.create.fh,
            Response->rsp.create.open_flags);
        break;
    default:
        LOG("", 0);
        break;
    }

#undef LOG
}
#endif
