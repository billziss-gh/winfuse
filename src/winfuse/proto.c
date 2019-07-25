/**
 * @file winfuse/proto.c
 *
 * @copyright 2019 Bill Zissimopoulos
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

#include <winfuse/driver.h>

NTSTATUS FuseProtoPostInit(PDEVICE_OBJECT DeviceObject);
VOID FuseProtoSendInit(FUSE_CONTEXT *Context);
VOID FuseProtoSendLookup(FUSE_CONTEXT *Context);
NTSTATUS FuseProtoPostForget(PDEVICE_OBJECT DeviceObject, PLIST_ENTRY ForgetList);
static VOID FuseProtoPostForget_ContextFini(FUSE_CONTEXT *Context);
VOID FuseProtoFillForget(FUSE_CONTEXT *Context);
VOID FuseProtoFillBatchForget(FUSE_CONTEXT *Context);
VOID FuseProtoSendGetattr(FUSE_CONTEXT *Context);
VOID FuseProtoSendMkdir(FUSE_CONTEXT *Context);
VOID FuseProtoSendMknod(FUSE_CONTEXT *Context);
VOID FuseProtoSendRmdir(FUSE_CONTEXT *Context);
VOID FuseProtoSendUnlink(FUSE_CONTEXT *Context);
VOID FuseProtoSendCreate(FUSE_CONTEXT *Context);
VOID FuseProtoSendChownOnCreate(FUSE_CONTEXT *Context);
VOID FuseProtoSendOpendir(FUSE_CONTEXT *Context);
VOID FuseProtoSendOpen(FUSE_CONTEXT *Context);
VOID FuseProtoSendReleasedir(FUSE_CONTEXT *Context);
VOID FuseProtoSendRelease(FUSE_CONTEXT *Context);
VOID FuseAttrToFileInfo(PDEVICE_OBJECT DeviceObject,
    FUSE_PROTO_ATTR *Attr, FSP_FSCTL_FILE_INFO *FileInfo);
NTSTATUS FuseNtStatusFromErrno(INT32 Errno);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FuseProtoPostInit)
#pragma alloc_text(PAGE, FuseProtoSendInit)
#pragma alloc_text(PAGE, FuseProtoSendLookup)
#pragma alloc_text(PAGE, FuseProtoPostForget)
#pragma alloc_text(PAGE, FuseProtoPostForget_ContextFini)
#pragma alloc_text(PAGE, FuseProtoFillForget)
#pragma alloc_text(PAGE, FuseProtoFillBatchForget)
#pragma alloc_text(PAGE, FuseProtoSendGetattr)
#pragma alloc_text(PAGE, FuseProtoSendMkdir)
#pragma alloc_text(PAGE, FuseProtoSendMknod)
#pragma alloc_text(PAGE, FuseProtoSendRmdir)
#pragma alloc_text(PAGE, FuseProtoSendUnlink)
#pragma alloc_text(PAGE, FuseProtoSendCreate)
#pragma alloc_text(PAGE, FuseProtoSendChownOnCreate)
#pragma alloc_text(PAGE, FuseProtoSendOpendir)
#pragma alloc_text(PAGE, FuseProtoSendOpen)
#pragma alloc_text(PAGE, FuseProtoSendReleasedir)
#pragma alloc_text(PAGE, FuseProtoSendRelease)
#pragma alloc_text(PAGE, FuseAttrToFileInfo)
#pragma alloc_text(PAGE, FuseNtStatusFromErrno)
#endif

static inline VOID FuseProtoInitRequest(FUSE_CONTEXT *Context,
    UINT32 len, UINT32 opcode, UINT64 nodeid)
{
    Context->FuseRequest->len = len;
    Context->FuseRequest->opcode = opcode;
    Context->FuseRequest->unique = (UINT64)(UINT_PTR)Context;
    Context->FuseRequest->nodeid = nodeid;
    Context->FuseRequest->uid = Context->OrigUid;
    Context->FuseRequest->gid = Context->OrigGid;
    Context->FuseRequest->pid = Context->OrigPid;
}

NTSTATUS FuseProtoPostInit(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FUSE_CONTEXT *Context;

    FuseContextCreate(&Context, DeviceObject, 0);
    ASSERT(0 != Context);
    if (FuseContextIsStatus(Context))
        return FuseContextToStatus(Context);

    Context->InternalResponse->Hint = FUSE_PROTO_OPCODE_INIT;

    FuseIoqPostPending(FuseDeviceExtension(DeviceObject)->Ioq, Context);

    return STATUS_SUCCESS;
}

VOID FuseProtoSendInit(FUSE_CONTEXT *Context)
    /*
     * Send INIT message.
     */
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        FuseProtoInitRequest(Context,
            FUSE_PROTO_REQ_SIZE(init), FUSE_PROTO_OPCODE_INIT, 0);
        Context->FuseRequest->req.init.major = FUSE_PROTO_VERSION;
        Context->FuseRequest->req.init.minor = FUSE_PROTO_MINOR_VERSION;
        Context->FuseRequest->req.init.max_readahead = 0;   /* !!!: REVISIT */
        Context->FuseRequest->req.init.flags = 0;           /* !!!: REVISIT */
        coro_yield;

        if (0 != Context->FuseResponse->error)
            Context->InternalResponse->IoStatus.Status =
                FuseNtStatusFromErrno(Context->FuseResponse->error);
    }
}

VOID FuseProtoSendLookup(FUSE_CONTEXT *Context)
    /*
     * Send LOOKUP message.
     *
     * Context->Lookup.Ino
     *     parent directory inode number
     * Context->Lookup.Name.Length
     *     name to lookup
     */
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        FuseProtoInitRequest(Context,
            (UINT32)(FUSE_PROTO_REQ_SIZE(lookup) + Context->Lookup.Name.Length + 1),
            FUSE_PROTO_OPCODE_LOOKUP, Context->Lookup.Ino);
        ASSERT(FUSE_PROTO_REQ_SIZEMIN >= Context->FuseRequest->len);
        RtlCopyMemory(Context->FuseRequest->req.lookup.name, Context->Lookup.Name.Buffer,
            Context->Lookup.Name.Length);
        Context->FuseRequest->req.lookup.name[Context->Lookup.Name.Length] = '\0';
        coro_yield;

        if (0 != Context->FuseResponse->error)
            Context->InternalResponse->IoStatus.Status =
                FuseNtStatusFromErrno(Context->FuseResponse->error);
    }
}

NTSTATUS FuseProtoPostForget(PDEVICE_OBJECT DeviceObject, PLIST_ENTRY ForgetList)
{
    PAGED_CODE();

    FUSE_CONTEXT *Context;

    FuseContextCreate(&Context, DeviceObject, 0);
    ASSERT(0 != Context);
    if (FuseContextIsStatus(Context))
        return FuseContextToStatus(Context);

    Context->Fini = FuseProtoPostForget_ContextFini;
    Context->InternalResponse->Hint = FUSE_PROTO_OPCODE_FORGET;

    ASSERT(ForgetList != ForgetList->Flink);
    Context->Forget.ForgetList = *ForgetList;
    /* fixup first/last list entry */
    Context->Forget.ForgetList.Flink->Blink = &Context->Forget.ForgetList;
    Context->Forget.ForgetList.Blink->Flink = &Context->Forget.ForgetList;

    FuseIoqPostPending(FuseDeviceExtension(DeviceObject)->Ioq, Context);

    return STATUS_SUCCESS;
}

static VOID FuseProtoPostForget_ContextFini(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    FuseCacheDeleteForgotten(&Context->Forget.ForgetList);
}

VOID FuseProtoFillForget(FUSE_CONTEXT *Context)
    /*
     * Fill FORGET message. This message is used to forget a single inode number.
     *
     * Context->Forget.ForgetList
     *     list that contains items to forget
     */
{
    PAGED_CODE();

    FUSE_PROTO_FORGET_ONE ForgetOne;
    BOOLEAN Ok;

    Ok = FuseCacheForgetOne(&Context->Forget.ForgetList, &ForgetOne);
    ASSERT(Ok);

    FuseProtoInitRequest(Context,
        FUSE_PROTO_REQ_SIZE(forget), FUSE_PROTO_OPCODE_FORGET, ForgetOne.nodeid);
    Context->FuseRequest->req.forget.nlookup = ForgetOne.nlookup;
}

VOID FuseProtoFillBatchForget(FUSE_CONTEXT *Context)
    /*
     * Fill BATCH_FORGET message. This message is used to forget multiple inode numbers.
     *
     * Context->Forget.ForgetList
     *     list that contains items to forget
     */
{
    PAGED_CODE();

    FUSE_PROTO_FORGET_ONE *StartP, *EndP, *P;

    StartP = (PVOID)((PUINT8)Context->FuseRequest + FUSE_PROTO_REQ_SIZE(batch_forget));
    EndP = (PVOID)((PUINT8)StartP + (FUSE_PROTO_REQ_SIZEMIN - FUSE_PROTO_REQ_SIZE(batch_forget)));
    for (P = StartP; EndP > P && FuseCacheForgetOne(&Context->Forget.ForgetList, P); P++)
        ;

    FuseProtoInitRequest(Context,
        (UINT32)((PUINT8)P - (PUINT8)Context->FuseRequest), FUSE_PROTO_OPCODE_BATCH_FORGET, 0);
    ASSERT(FUSE_PROTO_REQ_SIZEMIN >= Context->FuseRequest->len);
    Context->FuseRequest->req.batch_forget.count = (ULONG)(P - StartP);
}

VOID FuseProtoSendGetattr(FUSE_CONTEXT *Context)
    /*
     * Send GETATTR message.
     *
     * Context->Lookup.Ino
     *     inode number to get attributes for
     */
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        FuseProtoInitRequest(Context,
            FUSE_PROTO_REQ_SIZE(getattr), FUSE_PROTO_OPCODE_GETATTR, Context->Lookup.Ino);
        coro_yield;

        if (0 != Context->FuseResponse->error)
            Context->InternalResponse->IoStatus.Status =
                FuseNtStatusFromErrno(Context->FuseResponse->error);
    }
}

VOID FuseProtoSendMkdir(FUSE_CONTEXT *Context)
    /*
     * Send MKDIR message.
     *
     * Context->Lookup.Ino
     *     parent directory inode number
     * Context->Lookup.Name.Length
     *     name of new directory
     * Context->Lookup.Attr.mode
     *     mode of new directory
     */
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        FuseProtoInitRequest(Context,
            (UINT32)(FUSE_PROTO_REQ_SIZE(mkdir) + Context->Lookup.Name.Length + 1),
            FUSE_PROTO_OPCODE_MKDIR, Context->Lookup.Ino);
        ASSERT(FUSE_PROTO_REQ_SIZEMIN >= Context->FuseRequest->len);
        Context->FuseRequest->req.mkdir.mode = Context->Lookup.Attr.mode;
        Context->FuseRequest->req.mkdir.umask = 0;          /* !!!: REVISIT */
        RtlCopyMemory(Context->FuseRequest->req.mkdir.name, Context->Lookup.Name.Buffer,
            Context->Lookup.Name.Length);
        Context->FuseRequest->req.mkdir.name[Context->Lookup.Name.Length] = '\0';
        coro_yield;

        if (0 != Context->FuseResponse->error)
            Context->InternalResponse->IoStatus.Status =
                FuseNtStatusFromErrno(Context->FuseResponse->error);
    }
}

VOID FuseProtoSendMknod(FUSE_CONTEXT *Context)
    /*
     * Send MKNOD message.
     *
     * Context->Lookup.Ino
     *     parent directory inode number
     * Context->Lookup.Name.Length
     *     name of new file
     * Context->Lookup.Attr.mode
     *     mode of new file
     * Context->Lookup.Attr.rdev
     *     device number of new file (when file is a device)
     */
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        FuseProtoInitRequest(Context,
            (UINT32)(FUSE_PROTO_REQ_SIZE(mknod) + Context->Lookup.Name.Length + 1),
            FUSE_PROTO_OPCODE_MKDIR, Context->Lookup.Ino);
        ASSERT(FUSE_PROTO_REQ_SIZEMIN >= Context->FuseRequest->len);
        Context->FuseRequest->req.mknod.mode = Context->Lookup.Attr.mode;
        Context->FuseRequest->req.mknod.rdev = Context->Lookup.Attr.rdev;
        Context->FuseRequest->req.mknod.umask = 0;          /* !!!: REVISIT */
        RtlCopyMemory(Context->FuseRequest->req.mknod.name, Context->Lookup.Name.Buffer,
            Context->Lookup.Name.Length);
        Context->FuseRequest->req.mknod.name[Context->Lookup.Name.Length] = '\0';
        coro_yield;

        if (0 != Context->FuseResponse->error)
            Context->InternalResponse->IoStatus.Status =
                FuseNtStatusFromErrno(Context->FuseResponse->error);
    }
}

VOID FuseProtoSendRmdir(FUSE_CONTEXT *Context)
    /*
     * Send RMDIR message.
     *
     * Context->Lookup.Ino
     *     parent directory inode number
     * Context->Lookup.Name.Length
     *     name of directory
     */
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        FuseProtoInitRequest(Context,
            (UINT32)(FUSE_PROTO_REQ_SIZE(rmdir) + Context->Lookup.Name.Length + 1),
            FUSE_PROTO_OPCODE_RMDIR, Context->Lookup.Ino);
        ASSERT(FUSE_PROTO_REQ_SIZEMIN >= Context->FuseRequest->len);
        RtlCopyMemory(Context->FuseRequest->req.rmdir.name, Context->Lookup.Name.Buffer,
            Context->Lookup.Name.Length);
        Context->FuseRequest->req.rmdir.name[Context->Lookup.Name.Length] = '\0';
        coro_yield;

        if (0 != Context->FuseResponse->error)
            Context->InternalResponse->IoStatus.Status =
                FuseNtStatusFromErrno(Context->FuseResponse->error);
    }
}

VOID FuseProtoSendUnlink(FUSE_CONTEXT *Context)
    /*
     * Send UNLINK message.
     *
     * Context->Lookup.Ino
     *     parent directory inode number
     * Context->Lookup.Name.Length
     *     name of file
     */
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        FuseProtoInitRequest(Context,
            (UINT32)(FUSE_PROTO_REQ_SIZE(unlink) + Context->Lookup.Name.Length + 1),
            FUSE_PROTO_OPCODE_UNLINK, Context->Lookup.Ino);
        ASSERT(FUSE_PROTO_REQ_SIZEMIN >= Context->FuseRequest->len);
        RtlCopyMemory(Context->FuseRequest->req.unlink.name, Context->Lookup.Name.Buffer,
            Context->Lookup.Name.Length);
        Context->FuseRequest->req.unlink.name[Context->Lookup.Name.Length] = '\0';
        coro_yield;

        if (0 != Context->FuseResponse->error)
            Context->InternalResponse->IoStatus.Status =
                FuseNtStatusFromErrno(Context->FuseResponse->error);
    }
}

VOID FuseProtoSendCreate(FUSE_CONTEXT *Context)
    /*
     * Send CREATE message.
     *
     * Context->Lookup.Ino
     *     parent directory inode number
     * Context->Lookup.Name.Length
     *     name of new file
     * Context->Lookup.Attr.mode
     *     mode of new file
     * Context->File->OpenFlags
     *     open (O_*) flags
     */
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        FuseProtoInitRequest(Context,
            (UINT32)(FUSE_PROTO_REQ_SIZE(create) + Context->Lookup.Name.Length + 1),
            FUSE_PROTO_OPCODE_CREATE, Context->Lookup.Ino);
        ASSERT(FUSE_PROTO_REQ_SIZEMIN >= Context->FuseRequest->len);
        Context->FuseRequest->req.create.flags = Context->File->OpenFlags;
        Context->FuseRequest->req.create.mode = Context->Lookup.Attr.mode;
        Context->FuseRequest->req.create.umask = 0;         /* !!!: REVISIT */
        RtlCopyMemory(Context->FuseRequest->req.create.name, Context->Lookup.Name.Buffer,
            Context->Lookup.Name.Length);
        Context->FuseRequest->req.create.name[Context->Lookup.Name.Length] = '\0';
        coro_yield;

        if (0 != Context->FuseResponse->error)
            Context->InternalResponse->IoStatus.Status =
                FuseNtStatusFromErrno(Context->FuseResponse->error);
    }
}

VOID FuseProtoSendChownOnCreate(FUSE_CONTEXT *Context)
    /*
     * Send SETATTR message after directory/file creation.
     *
     * Context->InternalRequest->Req.Create.CreateOptions & FILE_DIRECTORY_FILE
     *     determines whether operation is applied to file (Context->File->Fh)
     *     or directory (Context->Lookup.Ino)
     * Context->File->Fh
     *     handle of related file; valid when used on file
     * Context->Lookup.Ino
     *     inode number of related directory; valid when used on directory
     * Context->Lookup.Attr.uid
     *     uid of new file
     * Context->Lookup.Attr.gid
     *     gid of new file
     */
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        if (FlagOn(Context->InternalRequest->Req.Create.CreateOptions, FILE_DIRECTORY_FILE))
        {
            FuseProtoInitRequest(Context,
                FUSE_PROTO_REQ_SIZE(setattr), FUSE_PROTO_OPCODE_SETATTR, Context->Lookup.Ino);
            Context->FuseRequest->req.setattr.valid =
                FUSE_PROTO_SETATTR_UID | FUSE_PROTO_SETATTR_GID;
            Context->FuseRequest->req.setattr.uid = Context->Lookup.Attr.uid;
            Context->FuseRequest->req.setattr.gid = Context->Lookup.Attr.gid;
        }
        else
        {
            FuseProtoInitRequest(Context,
                FUSE_PROTO_REQ_SIZE(setattr), FUSE_PROTO_OPCODE_SETATTR, 0);
            Context->FuseRequest->req.setattr.valid =
                FUSE_PROTO_SETATTR_FH | FUSE_PROTO_SETATTR_UID | FUSE_PROTO_SETATTR_GID;
            Context->FuseRequest->req.setattr.fh = Context->File->Fh;
            Context->FuseRequest->req.setattr.uid = Context->Lookup.Attr.uid;
            Context->FuseRequest->req.setattr.gid = Context->Lookup.Attr.gid;
        }
        coro_yield;

        if (0 != Context->FuseResponse->error)
            Context->InternalResponse->IoStatus.Status =
                FuseNtStatusFromErrno(Context->FuseResponse->error);
    }
}

VOID FuseProtoSendOpendir(FUSE_CONTEXT *Context)
    /*
     * Send OPENDIR message.
     *
     * Context->Lookup.Ino
     *     inode number of directory to open
     */
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        FuseProtoInitRequest(Context,
            FUSE_PROTO_REQ_SIZE(open), FUSE_PROTO_OPCODE_OPENDIR, Context->Lookup.Ino);
        coro_yield;

        if (0 != Context->FuseResponse->error)
            Context->InternalResponse->IoStatus.Status =
                FuseNtStatusFromErrno(Context->FuseResponse->error);
    }
}

VOID FuseProtoSendOpen(FUSE_CONTEXT *Context)
    /*
     * Send OPEN message.
     *
     * Context->Lookup.Ino
     *     inode number of file to open
     */
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        FuseProtoInitRequest(Context,
            FUSE_PROTO_REQ_SIZE(open), FUSE_PROTO_OPCODE_OPEN, Context->Lookup.Ino);
        Context->FuseRequest->req.open.flags = Context->File->OpenFlags;
        coro_yield;

        if (0 != Context->FuseResponse->error)
            Context->InternalResponse->IoStatus.Status =
                FuseNtStatusFromErrno(Context->FuseResponse->error);
    }
}

VOID FuseProtoSendReleasedir(FUSE_CONTEXT *Context)
    /*
     * Send RELEASEDIR message.
     *
     * Context->File->Fh
     *     handle of related directory
     * Context->File->OpenFlags
     *     open (O_*) flags
     */
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        FuseProtoInitRequest(Context,
            FUSE_PROTO_REQ_SIZE(release), FUSE_PROTO_OPCODE_RELEASEDIR, 0);
        Context->FuseRequest->req.release.fh = Context->File->Fh;
        Context->FuseRequest->req.release.flags = Context->File->OpenFlags;
        coro_yield;

        if (0 != Context->FuseResponse->error)
            Context->InternalResponse->IoStatus.Status =
                FuseNtStatusFromErrno(Context->FuseResponse->error);
    }
}

VOID FuseProtoSendRelease(FUSE_CONTEXT *Context)
    /*
     * Send RELEASE message.
     *
     * Context->File->Fh
     *     handle of related file
     * Context->File->OpenFlags
     *     open (O_*) flags
     */
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        FuseProtoInitRequest(Context,
            FUSE_PROTO_REQ_SIZE(release), FUSE_PROTO_OPCODE_RELEASE, 0);
        Context->FuseRequest->req.release.fh = Context->File->Fh;
        Context->FuseRequest->req.release.flags = Context->File->OpenFlags;
        Context->FuseRequest->req.release.release_flags = 0;/* !!!: REVISIT */
        Context->FuseRequest->req.release.lock_owner = 0;   /* !!!: REVISIT */
        coro_yield;

        if (0 != Context->FuseResponse->error)
            Context->InternalResponse->IoStatus.Status =
                FuseNtStatusFromErrno(Context->FuseResponse->error);
    }
}

VOID FuseAttrToFileInfo(PDEVICE_OBJECT DeviceObject,
    FUSE_PROTO_ATTR *Attr, FSP_FSCTL_FILE_INFO *FileInfo)
{
    PAGED_CODE();

    FUSE_DEVICE_EXTENSION *DeviceExtension = FuseDeviceExtension(DeviceObject);
    UINT64 AllocationUnit;

    AllocationUnit = (UINT64)DeviceExtension->VolumeParams->SectorSize *
        (UINT64)DeviceExtension->VolumeParams->SectorsPerAllocationUnit;

    switch (Attr->mode & 0170000)
    {
    case 0040000: /* S_IFDIR */
        FileInfo->FileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        FileInfo->ReparseTag = 0;
        break;
    case 0010000: /* S_IFIFO */
    case 0020000: /* S_IFCHR */
    case 0060000: /* S_IFBLK */
    case 0140000: /* S_IFSOCK */
        FileInfo->FileAttributes = FILE_ATTRIBUTE_REPARSE_POINT;
        FileInfo->ReparseTag = IO_REPARSE_TAG_NFS;
        break;
    case 0120000: /* S_IFLNK */
        /* !!!: if target is directory FILE_ATTRIBUTE_DIRECTORY must also be set! */
        FileInfo->FileAttributes = FILE_ATTRIBUTE_REPARSE_POINT;
        FileInfo->ReparseTag = IO_REPARSE_TAG_SYMLINK;
        break;
    default:
        FileInfo->FileAttributes = 0;
        FileInfo->ReparseTag = 0;
        break;
    }

    FileInfo->FileSize = Attr->size;
    FileInfo->AllocationSize =
        (FileInfo->FileSize + AllocationUnit - 1) / AllocationUnit * AllocationUnit;
    FuseUnixTimeToFileTime(Attr->atime, Attr->atimensec, &FileInfo->LastAccessTime);
    FuseUnixTimeToFileTime(Attr->mtime, Attr->mtimensec, &FileInfo->LastWriteTime);
    FuseUnixTimeToFileTime(Attr->ctime, Attr->ctimensec, &FileInfo->ChangeTime);
    FileInfo->CreationTime = FileInfo->ChangeTime;
    FileInfo->IndexNumber = Attr->ino;
    FileInfo->HardLinks = 0;
    FileInfo->EaSize = 0;
}

NTSTATUS FuseNtStatusFromErrno(INT32 Errno)
{
    PAGED_CODE();

    switch (Errno)
    {
    #undef FUSE_ERRNO
    #define FUSE_ERRNO 87
    #include <winfuse/errno.i>
    default:
        return STATUS_ACCESS_DENIED;
    }
}
