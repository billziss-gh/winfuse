/**
 * @file shared/km/proto.c
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

NTSTATUS FuseProtoPostInit(FUSE_INSTANCE *Instance);
VOID FuseProtoSendInit(FUSE_CONTEXT *Context);
NTSTATUS FuseProtoPostDestroy(FUSE_INSTANCE *Instance);
VOID FuseProtoSendDestroy(FUSE_CONTEXT *Context);
VOID FuseProtoSendLookup(FUSE_CONTEXT *Context);
NTSTATUS FuseProtoPostForget(FUSE_INSTANCE *Instance, PLIST_ENTRY ForgetList);
static VOID FuseProtoPostForget_ContextFini(FUSE_CONTEXT *Context);
VOID FuseProtoFillForget(FUSE_CONTEXT *Context);
VOID FuseProtoFillBatchForget(FUSE_CONTEXT *Context);
VOID FuseProtoSendStatfs(FUSE_CONTEXT *Context);
VOID FuseProtoSendGetattr(FUSE_CONTEXT *Context);
VOID FuseProtoSendFgetattr(FUSE_CONTEXT *Context);
VOID FuseProtoSendFtruncate(FUSE_CONTEXT *Context);
VOID FuseProtoSendFutimens(FUSE_CONTEXT *Context);
VOID FuseProtoSendLookupChown(FUSE_CONTEXT *Context);
VOID FuseProtoSendSetattr(FUSE_CONTEXT *Context);
VOID FuseProtoSendMkdir(FUSE_CONTEXT *Context);
VOID FuseProtoSendMknod(FUSE_CONTEXT *Context);
VOID FuseProtoSendRmdir(FUSE_CONTEXT *Context);
VOID FuseProtoSendUnlink(FUSE_CONTEXT *Context);
VOID FuseProtoSendRename(FUSE_CONTEXT *Context);
VOID FuseProtoSendCreate(FUSE_CONTEXT *Context);
VOID FuseProtoSendOpendir(FUSE_CONTEXT *Context);
VOID FuseProtoSendOpen(FUSE_CONTEXT *Context);
VOID FuseProtoSendReleasedir(FUSE_CONTEXT *Context);
VOID FuseProtoSendRelease(FUSE_CONTEXT *Context);
VOID FuseProtoSendReaddir(FUSE_CONTEXT *Context);
VOID FuseProtoSendRead(FUSE_CONTEXT *Context);
VOID FuseProtoSendWrite(FUSE_CONTEXT *Context);
VOID FuseProtoSendFsyncdir(FUSE_CONTEXT *Context);
VOID FuseProtoSendFsync(FUSE_CONTEXT *Context);
VOID FuseAttrToFileInfo(FUSE_INSTANCE *Instance,
    FUSE_PROTO_ATTR *Attr, FSP_FSCTL_FILE_INFO *FileInfo);
NTSTATUS FuseNtStatusFromErrno(FUSE_INSTANCE_TYPE InstanceType, INT32 Errno);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FuseProtoPostInit)
#pragma alloc_text(PAGE, FuseProtoSendInit)
#pragma alloc_text(PAGE, FuseProtoPostDestroy)
#pragma alloc_text(PAGE, FuseProtoSendDestroy)
#pragma alloc_text(PAGE, FuseProtoSendLookup)
#pragma alloc_text(PAGE, FuseProtoPostForget)
#pragma alloc_text(PAGE, FuseProtoPostForget_ContextFini)
#pragma alloc_text(PAGE, FuseProtoFillForget)
#pragma alloc_text(PAGE, FuseProtoFillBatchForget)
#pragma alloc_text(PAGE, FuseProtoSendStatfs)
#pragma alloc_text(PAGE, FuseProtoSendGetattr)
#pragma alloc_text(PAGE, FuseProtoSendFgetattr)
#pragma alloc_text(PAGE, FuseProtoSendFtruncate)
#pragma alloc_text(PAGE, FuseProtoSendFutimens)
#pragma alloc_text(PAGE, FuseProtoSendLookupChown)
#pragma alloc_text(PAGE, FuseProtoSendSetattr)
#pragma alloc_text(PAGE, FuseProtoSendMkdir)
#pragma alloc_text(PAGE, FuseProtoSendMknod)
#pragma alloc_text(PAGE, FuseProtoSendRmdir)
#pragma alloc_text(PAGE, FuseProtoSendUnlink)
#pragma alloc_text(PAGE, FuseProtoSendRename)
#pragma alloc_text(PAGE, FuseProtoSendCreate)
#pragma alloc_text(PAGE, FuseProtoSendOpendir)
#pragma alloc_text(PAGE, FuseProtoSendOpen)
#pragma alloc_text(PAGE, FuseProtoSendReleasedir)
#pragma alloc_text(PAGE, FuseProtoSendRelease)
#pragma alloc_text(PAGE, FuseProtoSendReaddir)
#pragma alloc_text(PAGE, FuseProtoSendRead)
#pragma alloc_text(PAGE, FuseProtoSendWrite)
#pragma alloc_text(PAGE, FuseProtoSendFsyncdir)
#pragma alloc_text(PAGE, FuseProtoSendFsync)
#pragma alloc_text(PAGE, FuseAttrToFileInfo)
#pragma alloc_text(PAGE, FuseNtStatusFromErrno)
#endif

#define FUSE_PROTO_SEND_BEGIN           \
    coro_block(Context->CoroState)      \
    {                                   \
        FuseContextWaitRequest(Context);
#define FUSE_PROTO_SEND_END             \
        FuseContextWaitResponse(Context);\
        Context->InternalResponse->IoStatus.Status = 0 == Context->FuseResponse->error ?\
            STATUS_SUCCESS :\
            FuseNtStatusFromErrno(Context->Instance->InstanceType, Context->FuseResponse->error);\
    }
#define FUSE_PROTO_SEND_BEGIN_(OPCODE)  \
    coro_block(Context->CoroState)      \
    {                                   \
        if (FuseInstanceGetOpcodeENOSYS(Context->Instance, FUSE_PROTO_OPCODE_ ## OPCODE))\
        {                               \
            Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_INVALID_DEVICE_REQUEST;\
            coro_break;                 \
        }                               \
        FuseContextWaitRequest(Context);
#define FUSE_PROTO_SEND_END_(OPCODE)    \
        FuseContextWaitResponse(Context);\
        Context->InternalResponse->IoStatus.Status = 0 == Context->FuseResponse->error ?\
            STATUS_SUCCESS :\
            FuseNtStatusFromErrno(Context->Instance->InstanceType, Context->FuseResponse->error);\
        if (STATUS_INVALID_DEVICE_REQUEST == Context->InternalResponse->IoStatus.Status)\
            FuseInstanceSetOpcodeENOSYS(Context->Instance, FUSE_PROTO_OPCODE_ ## OPCODE);\
    }

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

NTSTATUS FuseProtoPostInit(FUSE_INSTANCE *Instance)
{
    PAGED_CODE();

    FUSE_CONTEXT *Context;

    FuseContextCreate(&Context, Instance, 0);
    ASSERT(0 != Context);
    if (FuseContextIsStatus(Context))
        return FuseContextToStatus(Context);

    Context->InternalResponse->Hint = FUSE_PROTO_OPCODE_INIT;

    FuseIoqPostPending(Instance->Ioq, Context);

    return STATUS_SUCCESS;
}

VOID FuseProtoSendInit(FUSE_CONTEXT *Context)
    /*
     * Send INIT message.
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN

        FuseProtoInitRequest(Context,
            FUSE_PROTO_REQ_SIZE(init), FUSE_PROTO_OPCODE_INIT, 0);
        Context->FuseRequest->req.init.major = FUSE_PROTO_VERSION;
        Context->FuseRequest->req.init.minor = FUSE_PROTO_MINOR_VERSION;
        Context->FuseRequest->req.init.max_readahead = 0;   /* !!!: REVISIT */
        Context->FuseRequest->req.init.flags = 0;           /* !!!: REVISIT */

    FUSE_PROTO_SEND_END
}

NTSTATUS FuseProtoPostDestroy(FUSE_INSTANCE *Instance)
{
    PAGED_CODE();

    FUSE_CONTEXT *Context;

    FuseContextCreate(&Context, Instance, 0);
    ASSERT(0 != Context);
    if (FuseContextIsStatus(Context))
        return FuseContextToStatus(Context);

    Context->InternalResponse->Hint = FUSE_PROTO_OPCODE_DESTROY;

    FuseIoqPostPendingAndStop(Instance->Ioq, Context);

    return STATUS_SUCCESS;
}

VOID FuseProtoSendDestroy(FUSE_CONTEXT *Context)
    /*
     * Send DESTROY message.
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN

        FuseProtoInitRequest(Context,
            FUSE_PROTO_REQ_HEADER_SIZE, FUSE_PROTO_OPCODE_DESTROY, 0);

        if (0 != Context->Instance->ProtoSendDestroyHandler)
            Context->Instance->ProtoSendDestroyHandler(Context->Instance->ProtoSendDestroyData);

    FUSE_PROTO_SEND_END
}

VOID FuseProtoSendLookup(FUSE_CONTEXT *Context)
    /*
     * Send LOOKUP message.
     *
     * Context->Lookup.Ino
     *     parent directory inode number
     * Context->Lookup.Name
     *     name to lookup
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN

        FuseProtoInitRequest(Context,
            (UINT32)(FUSE_PROTO_REQ_SIZE(lookup) + Context->Lookup.Name.Length + 1),
            FUSE_PROTO_OPCODE_LOOKUP, Context->Lookup.Ino);
        ASSERT(FUSE_PROTO_REQ_SIZEMIN >= Context->FuseRequest->len);
        RtlCopyMemory(Context->FuseRequest->req.lookup.name, Context->Lookup.Name.Buffer,
            Context->Lookup.Name.Length);
        Context->FuseRequest->req.lookup.name[Context->Lookup.Name.Length] = '\0';

    FUSE_PROTO_SEND_END
}

NTSTATUS FuseProtoPostForget(FUSE_INSTANCE *Instance, PLIST_ENTRY ForgetList)
{
    PAGED_CODE();

    FUSE_CONTEXT *Context;

    FuseContextCreate(&Context, Instance, 0);
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

    FuseIoqPostPending(Instance->Ioq, Context);

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
    for (P = StartP; DEBUGTEST(90) && EndP > P && FuseCacheForgetOne(&Context->Forget.ForgetList, P); P++)
        ;

    FuseProtoInitRequest(Context,
        (UINT32)((PUINT8)P - (PUINT8)Context->FuseRequest), FUSE_PROTO_OPCODE_BATCH_FORGET, 0);
    ASSERT(FUSE_PROTO_REQ_SIZEMIN >= Context->FuseRequest->len);
    Context->FuseRequest->req.batch_forget.count = (ULONG)(P - StartP);
}

VOID FuseProtoSendStatfs(FUSE_CONTEXT *Context)
    /*
     * Send STATFS message.
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN

        FuseProtoInitRequest(Context,
            FUSE_PROTO_REQ_HEADER_SIZE, FUSE_PROTO_OPCODE_STATFS, 0);

    FUSE_PROTO_SEND_END
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

    FUSE_PROTO_SEND_BEGIN

        FuseProtoInitRequest(Context,
            FUSE_PROTO_REQ_SIZE(getattr), FUSE_PROTO_OPCODE_GETATTR, Context->Lookup.Ino);

    FUSE_PROTO_SEND_END
}

VOID FuseProtoSendFgetattr(FUSE_CONTEXT *Context)
    /*
     * Send GETATTR message given a file.
     *
     * Context->File->IsDirectory
     *     true if file is a directory
     * Context->File->Ino
     *     inode number of related file
     * Context->File->Fh
     *     handle of related file; use only when file is not a directory
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN

        if (Context->File->IsDirectory)
        {
            FuseProtoInitRequest(Context,
                FUSE_PROTO_REQ_SIZE(getattr), FUSE_PROTO_OPCODE_GETATTR, Context->File->Ino);
        }
        else
        {
            FuseProtoInitRequest(Context,
                FUSE_PROTO_REQ_SIZE(getattr), FUSE_PROTO_OPCODE_GETATTR, Context->File->Ino);
            Context->FuseRequest->req.getattr.getattr_flags = FUSE_PROTO_GETATTR_FH;
            Context->FuseRequest->req.getattr.fh = Context->File->Fh;
        }

    FUSE_PROTO_SEND_END
}

VOID FuseProtoSendFtruncate(FUSE_CONTEXT *Context)
    /*
     * Send SETATTR message for truncate.
     *
     * Context->File->Ino
     *     inode number of related file
     * Context->File->Fh
     *     handle of related file
     * Context->Setattr.Attr.size
     *     new size of file
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN

        FuseProtoInitRequest(Context,
            FUSE_PROTO_REQ_SIZE(setattr), FUSE_PROTO_OPCODE_SETATTR, Context->File->Ino);
        Context->FuseRequest->req.setattr.valid =
            FUSE_PROTO_SETATTR_FH | FUSE_PROTO_SETATTR_SIZE;
        Context->FuseRequest->req.setattr.fh = Context->File->Fh;
        Context->FuseRequest->req.setattr.size = Context->Setattr.Attr.size;

    FUSE_PROTO_SEND_END
}

VOID FuseProtoSendFutimens(FUSE_CONTEXT *Context)
    /*
     * Send SETATTR message for utimens.
     *
     * Context->File->Ino
     *     inode number of related file
     * Context->File->Fh
     *     handle of related file
     * Context->Setattr.Attr.{atime,atimensec}
     *     new access time of file
     * Context->Setattr.Attr.{mtime,mtimensec}
     *     new modification time of file
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN

        if (Context->File->IsDirectory)
        {
            FuseProtoInitRequest(Context,
                FUSE_PROTO_REQ_SIZE(setattr), FUSE_PROTO_OPCODE_SETATTR, Context->File->Ino);
        }
        else
        {
            FuseProtoInitRequest(Context,
                FUSE_PROTO_REQ_SIZE(setattr), FUSE_PROTO_OPCODE_SETATTR, Context->File->Ino);
            Context->FuseRequest->req.setattr.valid = FUSE_PROTO_SETATTR_FH;
            Context->FuseRequest->req.setattr.fh = Context->File->Fh;
        }

        if (FUSE_PROTO_UTIME_OMIT != Context->Setattr.Attr.atimensec)
        {
            Context->FuseRequest->req.setattr.valid |= FUSE_PROTO_SETATTR_ATIME;
            Context->FuseRequest->req.setattr.atime = Context->Setattr.Attr.atime;
            Context->FuseRequest->req.setattr.atimensec = Context->Setattr.Attr.atimensec;
        }
        if (FUSE_PROTO_UTIME_OMIT != Context->Setattr.Attr.mtimensec)
        {
            Context->FuseRequest->req.setattr.valid |= FUSE_PROTO_SETATTR_MTIME;
            Context->FuseRequest->req.setattr.mtime = Context->Setattr.Attr.mtime;
            Context->FuseRequest->req.setattr.mtimensec = Context->Setattr.Attr.mtimensec;
        }

    FUSE_PROTO_SEND_END
}

VOID FuseProtoSendLookupChown(FUSE_CONTEXT *Context)
    /*
     * Send SETATTR message for chown.
     *
     * Context->File->IsDirectory
     *     true if file is a directory
     * Context->File->Ino
     *     inode number of related file; use when file is a directory
     * Context->File->Fh
     *     handle of related file; use when file is not a directory
     * Context->Lookup.Attr.uid
     *     uid of new file
     * Context->Lookup.Attr.gid
     *     gid of new file
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN

        if (Context->File->IsDirectory)
        {
            FuseProtoInitRequest(Context,
                FUSE_PROTO_REQ_SIZE(setattr), FUSE_PROTO_OPCODE_SETATTR, Context->File->Ino);
            Context->FuseRequest->req.setattr.valid =
                FUSE_PROTO_SETATTR_UID | FUSE_PROTO_SETATTR_GID;
            Context->FuseRequest->req.setattr.uid = Context->Lookup.Attr.uid;
            Context->FuseRequest->req.setattr.gid = Context->Lookup.Attr.gid;
        }
        else
        {
            FuseProtoInitRequest(Context,
                FUSE_PROTO_REQ_SIZE(setattr), FUSE_PROTO_OPCODE_SETATTR, Context->File->Ino);
            Context->FuseRequest->req.setattr.valid =
                FUSE_PROTO_SETATTR_FH | FUSE_PROTO_SETATTR_UID | FUSE_PROTO_SETATTR_GID;
            Context->FuseRequest->req.setattr.fh = Context->File->Fh;
            Context->FuseRequest->req.setattr.uid = Context->Lookup.Attr.uid;
            Context->FuseRequest->req.setattr.gid = Context->Lookup.Attr.gid;
        }

    FUSE_PROTO_SEND_END
}

VOID FuseProtoSendSetattr(FUSE_CONTEXT *Context)
    /*
     * Send SETATTR message.
     *
     * Context->File->IsDirectory
     *     true if file is a directory
     * Context->File->Ino
     *     inode number of related file; use when file is a directory
     * Context->File->Fh
     *     handle of related file; use when file is not a directory
     * Context->Setattr.AttrValid
     *     valid attr fields
     * Context->Setattr.Attr
     *     new attr of file
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN

        if (Context->File->IsDirectory)
        {
            FuseProtoInitRequest(Context,
                FUSE_PROTO_REQ_SIZE(setattr), FUSE_PROTO_OPCODE_SETATTR, Context->File->Ino);
            Context->FuseRequest->req.setattr.size = Context->Setattr.Attr.size;
            Context->FuseRequest->req.setattr.atime = Context->Setattr.Attr.atime;
            Context->FuseRequest->req.setattr.mtime = Context->Setattr.Attr.mtime;
            Context->FuseRequest->req.setattr.ctime = Context->Setattr.Attr.ctime;
            Context->FuseRequest->req.setattr.atimensec = Context->Setattr.Attr.atimensec;
            Context->FuseRequest->req.setattr.mtimensec = Context->Setattr.Attr.mtimensec;
            Context->FuseRequest->req.setattr.ctimensec = Context->Setattr.Attr.ctimensec;
            Context->FuseRequest->req.setattr.mode = Context->Setattr.Attr.mode;
            Context->FuseRequest->req.setattr.uid = Context->Setattr.Attr.uid;
            Context->FuseRequest->req.setattr.gid = Context->Setattr.Attr.gid;
            Context->FuseRequest->req.setattr.valid = Context->Setattr.AttrValid;
        }
        else
        {
            FuseProtoInitRequest(Context,
                FUSE_PROTO_REQ_SIZE(setattr), FUSE_PROTO_OPCODE_SETATTR, Context->File->Ino);
            Context->FuseRequest->req.setattr.size = Context->Setattr.Attr.size;
            Context->FuseRequest->req.setattr.atime = Context->Setattr.Attr.atime;
            Context->FuseRequest->req.setattr.mtime = Context->Setattr.Attr.mtime;
            Context->FuseRequest->req.setattr.ctime = Context->Setattr.Attr.ctime;
            Context->FuseRequest->req.setattr.atimensec = Context->Setattr.Attr.atimensec;
            Context->FuseRequest->req.setattr.mtimensec = Context->Setattr.Attr.mtimensec;
            Context->FuseRequest->req.setattr.ctimensec = Context->Setattr.Attr.ctimensec;
            Context->FuseRequest->req.setattr.mode = Context->Setattr.Attr.mode;
            Context->FuseRequest->req.setattr.uid = Context->Setattr.Attr.uid;
            Context->FuseRequest->req.setattr.gid = Context->Setattr.Attr.gid;
            Context->FuseRequest->req.setattr.fh = Context->File->Fh;
            Context->FuseRequest->req.setattr.valid = Context->Setattr.AttrValid | FUSE_PROTO_SETATTR_FH;
        }

    FUSE_PROTO_SEND_END
}

VOID FuseProtoSendMkdir(FUSE_CONTEXT *Context)
    /*
     * Send MKDIR message.
     *
     * Context->Lookup.Ino
     *     parent directory inode number
     * Context->Lookup.Name
     *     name of new directory
     * Context->Lookup.Attr.mode
     *     mode of new directory
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN

        FuseProtoInitRequest(Context,
            (UINT32)(FUSE_PROTO_REQ_SIZE(mkdir) + Context->Lookup.Name.Length + 1),
            FUSE_PROTO_OPCODE_MKDIR, Context->Lookup.Ino);
        ASSERT(FUSE_PROTO_REQ_SIZEMIN >= Context->FuseRequest->len);
        Context->FuseRequest->req.mkdir.mode = Context->Lookup.Attr.mode;
        Context->FuseRequest->req.mkdir.umask = 0;          /* !!!: REVISIT */
        RtlCopyMemory(Context->FuseRequest->req.mkdir.name, Context->Lookup.Name.Buffer,
            Context->Lookup.Name.Length);
        Context->FuseRequest->req.mkdir.name[Context->Lookup.Name.Length] = '\0';

    FUSE_PROTO_SEND_END
}

VOID FuseProtoSendMknod(FUSE_CONTEXT *Context)
    /*
     * Send MKNOD message.
     *
     * Context->Lookup.Ino
     *     parent directory inode number
     * Context->Lookup.Name
     *     name of new file
     * Context->Lookup.Attr.mode
     *     mode of new file
     * Context->Lookup.Attr.rdev
     *     device number of new file (when file is a device)
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN

        FuseProtoInitRequest(Context,
            (UINT32)(FUSE_PROTO_REQ_SIZE(mknod) + Context->Lookup.Name.Length + 1),
            FUSE_PROTO_OPCODE_MKNOD, Context->Lookup.Ino);
        ASSERT(FUSE_PROTO_REQ_SIZEMIN >= Context->FuseRequest->len);
        Context->FuseRequest->req.mknod.mode = Context->Lookup.Attr.mode;
        Context->FuseRequest->req.mknod.rdev = Context->Lookup.Attr.rdev;
        Context->FuseRequest->req.mknod.umask = 0;          /* !!!: REVISIT */
        RtlCopyMemory(Context->FuseRequest->req.mknod.name, Context->Lookup.Name.Buffer,
            Context->Lookup.Name.Length);
        Context->FuseRequest->req.mknod.name[Context->Lookup.Name.Length] = '\0';

    FUSE_PROTO_SEND_END
}

VOID FuseProtoSendRmdir(FUSE_CONTEXT *Context)
    /*
     * Send RMDIR message.
     *
     * Context->Lookup.Ino
     *     parent directory inode number
     * Context->Lookup.Name
     *     name of directory
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN

        FuseProtoInitRequest(Context,
            (UINT32)(FUSE_PROTO_REQ_SIZE(rmdir) + Context->Lookup.Name.Length + 1),
            FUSE_PROTO_OPCODE_RMDIR, Context->Lookup.Ino);
        ASSERT(FUSE_PROTO_REQ_SIZEMIN >= Context->FuseRequest->len);
        RtlCopyMemory(Context->FuseRequest->req.rmdir.name, Context->Lookup.Name.Buffer,
            Context->Lookup.Name.Length);
        Context->FuseRequest->req.rmdir.name[Context->Lookup.Name.Length] = '\0';

    FUSE_PROTO_SEND_END
}

VOID FuseProtoSendUnlink(FUSE_CONTEXT *Context)
    /*
     * Send UNLINK message.
     *
     * Context->Lookup.Ino
     *     parent directory inode number
     * Context->Lookup.Name
     *     name of file
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN

        FuseProtoInitRequest(Context,
            (UINT32)(FUSE_PROTO_REQ_SIZE(unlink) + Context->Lookup.Name.Length + 1),
            FUSE_PROTO_OPCODE_UNLINK, Context->Lookup.Ino);
        ASSERT(FUSE_PROTO_REQ_SIZEMIN >= Context->FuseRequest->len);
        RtlCopyMemory(Context->FuseRequest->req.unlink.name, Context->Lookup.Name.Buffer,
            Context->Lookup.Name.Length);
        Context->FuseRequest->req.unlink.name[Context->Lookup.Name.Length] = '\0';

    FUSE_PROTO_SEND_END
}

VOID FuseProtoSendRename(FUSE_CONTEXT *Context)
    /*
     * Send RENAME message.
     *
     * Context->LookupPath.Ino
     *     old parent directory inode number
     * Context->LookupPath.Name
     *     old name of file
     * Context->LookupPath.Ino2
     *     new parent directory inode number
     * Context->LookupPath.Name2
     *     new name of file
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN

        FuseProtoInitRequest(Context, (UINT32)(FUSE_PROTO_REQ_SIZE(rename) +
            Context->LookupPath.Name.Length + 1 + Context->LookupPath.Name2.Length + 1),
            FUSE_PROTO_OPCODE_RENAME, Context->LookupPath.Ino);
        ASSERT(FUSE_PROTO_REQ_SIZEMIN >= Context->FuseRequest->len);
        Context->FuseRequest->req.rename.newdir = Context->LookupPath.Ino2;
        RtlCopyMemory(Context->FuseRequest->req.rename.name,
            Context->LookupPath.Name.Buffer,
            Context->LookupPath.Name.Length);
        Context->FuseRequest->req.rename.name[Context->LookupPath.Name.Length] = '\0';
        RtlCopyMemory(Context->FuseRequest->req.rename.name + Context->LookupPath.Name.Length + 1,
            Context->LookupPath.Name2.Buffer,
            Context->LookupPath.Name2.Length);
        (Context->FuseRequest->req.rename.name + Context->LookupPath.Name.Length + 1)
            [Context->LookupPath.Name2.Length] = '\0';

    FUSE_PROTO_SEND_END
}

VOID FuseProtoSendCreate(FUSE_CONTEXT *Context)
    /*
     * Send CREATE message.
     *
     * Context->Lookup.Ino
     *     parent directory inode number
     * Context->Lookup.Name
     *     name of new file
     * Context->Lookup.Attr.mode
     *     mode of new file
     * Context->File->OpenFlags
     *     open (O_*) flags
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN_(CREATE)

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

    FUSE_PROTO_SEND_END_(CREATE)
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

    FUSE_PROTO_SEND_BEGIN

        FuseProtoInitRequest(Context,
            FUSE_PROTO_REQ_SIZE(open), FUSE_PROTO_OPCODE_OPENDIR, Context->Lookup.Ino);
        Context->FuseRequest->req.open.flags = Context->File->OpenFlags;

    FUSE_PROTO_SEND_END
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

    FUSE_PROTO_SEND_BEGIN

        FuseProtoInitRequest(Context,
            FUSE_PROTO_REQ_SIZE(open), FUSE_PROTO_OPCODE_OPEN, Context->Lookup.Ino);
        Context->FuseRequest->req.open.flags = Context->File->OpenFlags;

    FUSE_PROTO_SEND_END
}

VOID FuseProtoSendReleasedir(FUSE_CONTEXT *Context)
    /*
     * Send RELEASEDIR message.
     *
     * Context->File->Ino
     *     inode number of related directory
     * Context->File->Fh
     *     handle of related directory
     * Context->File->OpenFlags
     *     open (O_*) flags
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN

        FuseProtoInitRequest(Context,
            FUSE_PROTO_REQ_SIZE(release), FUSE_PROTO_OPCODE_RELEASEDIR, Context->File->Ino);
        Context->FuseRequest->req.release.fh = Context->File->Fh;
        Context->FuseRequest->req.release.flags = Context->File->OpenFlags;

    FUSE_PROTO_SEND_END
}

VOID FuseProtoSendRelease(FUSE_CONTEXT *Context)
    /*
     * Send RELEASE message.
     *
     * Context->File->Ino
     *     inode number of related file
     * Context->File->Fh
     *     handle of related file
     * Context->File->OpenFlags
     *     open (O_*) flags
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN

        FuseProtoInitRequest(Context,
            FUSE_PROTO_REQ_SIZE(release), FUSE_PROTO_OPCODE_RELEASE, Context->File->Ino);
        Context->FuseRequest->req.release.fh = Context->File->Fh;
        Context->FuseRequest->req.release.flags = Context->File->OpenFlags;
        Context->FuseRequest->req.release.release_flags = 0;/* !!!: REVISIT */
        Context->FuseRequest->req.release.lock_owner = 0;   /* !!!: REVISIT */

    FUSE_PROTO_SEND_END
}

VOID FuseProtoSendReaddir(FUSE_CONTEXT *Context)
    /*
     * Send READDIR message.
     *
     * Context->File->Ino
     *     inode number of related directory
     * Context->File->Fh
     *     handle of related directory
     * Context->QueryDirectory.NextOffset
     *     offset of next directory entry or 0
     * Context->QueryDirectory.Length
     *     readdir buffer length
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN

        FuseProtoInitRequest(Context,
            FUSE_PROTO_REQ_SIZE(read), FUSE_PROTO_OPCODE_READDIR, Context->File->Ino);
        Context->FuseRequest->req.read.fh = Context->File->Fh;
        Context->FuseRequest->req.read.offset = Context->QueryDirectory.NextOffset;
        Context->FuseRequest->req.read.size = Context->QueryDirectory.Length;
        Context->FuseRequest->req.read.read_flags = 0;   /* !!!: REVISIT */
        Context->FuseRequest->req.read.lock_owner = 0;   /* !!!: REVISIT */
        Context->FuseRequest->req.read.flags = Context->File->OpenFlags;

    FUSE_PROTO_SEND_END
}

VOID FuseProtoSendRead(FUSE_CONTEXT *Context)
    /*
     * Send READ message.
     *
     * Context->File->Ino
     *     inode number of related file
     * Context->File->Fh
     *     handle of related file
     * Context->Read.Offset
     *     offset to read
     * Context->Read.Length
     *     read buffer length
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN

        FuseProtoInitRequest(Context,
            FUSE_PROTO_REQ_SIZE(read), FUSE_PROTO_OPCODE_READ, Context->File->Ino);
        Context->FuseRequest->req.read.fh = Context->File->Fh;
        Context->FuseRequest->req.read.offset = Context->Read.StartOffset + Context->Read.Offset;
        Context->FuseRequest->req.read.size = Context->Read.Length;
        Context->FuseRequest->req.read.read_flags = 0;   /* !!!: REVISIT */
        Context->FuseRequest->req.read.lock_owner = 0;   /* !!!: REVISIT */
        Context->FuseRequest->req.read.flags = Context->File->OpenFlags;

    FUSE_PROTO_SEND_END
}

VOID FuseProtoSendWrite(FUSE_CONTEXT *Context)
    /*
     * Send WRITE message.
     *
     * Context->File->Ino
     *     inode number of related file
     * Context->File->Fh
     *     handle of related file
     * Context->Write.Offset
     *     offset to write
     * Context->Write.Length
     *     write buffer length
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN

        FuseProtoInitRequest(Context,
            FUSE_PROTO_REQ_SIZE(write) + Context->Write.Length,
            FUSE_PROTO_OPCODE_WRITE, Context->File->Ino);
        Context->FuseRequest->req.write.fh = Context->File->Fh;
        Context->FuseRequest->req.write.offset = Context->Write.StartOffset + Context->Write.Offset;
        Context->FuseRequest->req.write.size = Context->Write.Length;
        Context->FuseRequest->req.write.write_flags = 0;   /* !!!: REVISIT */
        Context->FuseRequest->req.write.lock_owner = 0;   /* !!!: REVISIT */
        Context->FuseRequest->req.write.flags = Context->File->OpenFlags;

    FUSE_PROTO_SEND_END
}

VOID FuseProtoSendFsyncdir(FUSE_CONTEXT *Context)
    /*
     * Send FSYNCDIR message.
     *
     * Context->File->Ino
     *     inode number of related directory
     * Context->File->Fh
     *     handle of related directory
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN_(FSYNCDIR)

        FuseProtoInitRequest(Context,
            FUSE_PROTO_REQ_SIZE(fsync), FUSE_PROTO_OPCODE_FSYNCDIR, Context->File->Ino);
        Context->FuseRequest->req.fsync.fh = Context->File->Fh;

    FUSE_PROTO_SEND_END_(FSYNCDIR)
}

VOID FuseProtoSendFsync(FUSE_CONTEXT *Context)
    /*
     * Send FSYNC message.
     *
     * Context->File->Ino
     *     inode number of related file
     * Context->File->Fh
     *     handle of related file
     */
{
    PAGED_CODE();

    FUSE_PROTO_SEND_BEGIN_(FSYNC)

        FuseProtoInitRequest(Context,
            FUSE_PROTO_REQ_SIZE(fsync), FUSE_PROTO_OPCODE_FSYNC, Context->File->Ino);
        Context->FuseRequest->req.fsync.fh = Context->File->Fh;

    FUSE_PROTO_SEND_END_(FSYNC)
}

VOID FuseAttrToFileInfo(FUSE_INSTANCE *Instance,
    FUSE_PROTO_ATTR *Attr, FSP_FSCTL_FILE_INFO *FileInfo)
{
    PAGED_CODE();

    UINT64 AllocationUnit;

    AllocationUnit = (UINT64)Instance->VolumeParams->SectorSize *
        (UINT64)Instance->VolumeParams->SectorsPerAllocationUnit;

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

NTSTATUS FuseNtStatusFromErrno(FUSE_INSTANCE_TYPE InstanceType, INT32 Errno)
{
    PAGED_CODE();

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
        #include "errno.i"
        default:
            return STATUS_ACCESS_DENIED;
        }
    case FuseInstanceCygwin:
        switch (Errno)
        {
        #undef FUSE_ERRNO
        #define FUSE_ERRNO 67
        #include "errno.i"
        default:
            return STATUS_ACCESS_DENIED;
        }
    case FuseInstanceLinux:
        switch (Errno)
        {
        #undef FUSE_ERRNO
        #define FUSE_ERRNO 76
        #include "errno.i"
        default:
            return STATUS_ACCESS_DENIED;
        }
    }
}
