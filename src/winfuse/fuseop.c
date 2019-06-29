/**
 * @file winfuse/fuseop.c
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

static BOOLEAN FuseOpReserved_Init(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpReserved_Destroy(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpReserved_Forget(FUSE_CONTEXT *Context);
BOOLEAN FuseOpReserved(FUSE_CONTEXT *Context);
static NTSTATUS FuseAccessCheck(
    UINT32 FileUid, UINT32 FileGid, UINT32 FileMode,
    UINT32 OrigUid, UINT32 OrigGid, UINT32 DesiredAccess,
    PUINT32 PGrantedAccess);
static NTSTATUS FusePrepareContextNs(FUSE_CONTEXT *Context);
static VOID FusePrepareContextNs_ContextFini(FUSE_CONTEXT *Context);
static VOID FuseLookupName(FUSE_CONTEXT *Context);
static VOID FuseLookupPath(FUSE_CONTEXT *Context);
static VOID FuseCreateCheck(FUSE_CONTEXT *Context);
static VOID FuseOpenCheck(FUSE_CONTEXT *Context);
static VOID FuseOverwriteCheck(FUSE_CONTEXT *Context);
static VOID FuseOpenTargetDirectoryCheck(FUSE_CONTEXT *Context);
static VOID FuseCreate(FUSE_CONTEXT *Context);
static VOID FuseOpen(FUSE_CONTEXT *Context);
static VOID FuseOpCreate_FileCreate(FUSE_CONTEXT *Context);
static VOID FuseOpCreate_FileOpen(FUSE_CONTEXT *Context);
static VOID FuseOpCreate_FileOpenIf(FUSE_CONTEXT *Context);
static VOID FuseOpCreate_FileOverwrite(FUSE_CONTEXT *Context);
static VOID FuseOpCreate_FileOverwriteIf(FUSE_CONTEXT *Context);
static VOID FuseOpCreate_FileOpenTargetDirectory(FUSE_CONTEXT *Context);
BOOLEAN FuseOpCreate(FUSE_CONTEXT *Context);
BOOLEAN FuseOpOverwrite(FUSE_CONTEXT *Context);
BOOLEAN FuseOpCleanup(FUSE_CONTEXT *Context);
BOOLEAN FuseOpClose(FUSE_CONTEXT *Context);
BOOLEAN FuseOpRead(FUSE_CONTEXT *Context);
BOOLEAN FuseOpWrite(FUSE_CONTEXT *Context);
BOOLEAN FuseOpQueryInformation(FUSE_CONTEXT *Context);
BOOLEAN FuseOpSetInformation(FUSE_CONTEXT *Context);
BOOLEAN FuseOpQueryEa(FUSE_CONTEXT *Context);
BOOLEAN FuseOpSetEa(FUSE_CONTEXT *Context);
BOOLEAN FuseOpFlushBuffers(FUSE_CONTEXT *Context);
BOOLEAN FuseOpQueryVolumeInformation(FUSE_CONTEXT *Context);
BOOLEAN FuseOpSetVolumeInformation(FUSE_CONTEXT *Context);
BOOLEAN FuseOpQueryDirectory(FUSE_CONTEXT *Context);
BOOLEAN FuseOpFileSystemControl(FUSE_CONTEXT *Context);
BOOLEAN FuseOpDeviceControl(FUSE_CONTEXT *Context);
BOOLEAN FuseOpQuerySecurity(FUSE_CONTEXT *Context);
BOOLEAN FuseOpSetSecurity(FUSE_CONTEXT *Context);
BOOLEAN FuseOpQueryStreamInformation(FUSE_CONTEXT *Context);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FuseOpReserved_Init)
#pragma alloc_text(PAGE, FuseOpReserved_Destroy)
#pragma alloc_text(PAGE, FuseOpReserved_Forget)
#pragma alloc_text(PAGE, FuseOpReserved)
#pragma alloc_text(PAGE, FuseAccessCheck)
#pragma alloc_text(PAGE, FusePrepareContextNs)
#pragma alloc_text(PAGE, FusePrepareContextNs_ContextFini)
#pragma alloc_text(PAGE, FuseLookupName)
#pragma alloc_text(PAGE, FuseLookupPath)
#pragma alloc_text(PAGE, FuseCreateCheck)
#pragma alloc_text(PAGE, FuseOpenCheck)
#pragma alloc_text(PAGE, FuseOverwriteCheck)
#pragma alloc_text(PAGE, FuseOpenTargetDirectoryCheck)
#pragma alloc_text(PAGE, FuseCreate)
#pragma alloc_text(PAGE, FuseOpen)
#pragma alloc_text(PAGE, FuseOpCreate_FileCreate)
#pragma alloc_text(PAGE, FuseOpCreate_FileOpen)
#pragma alloc_text(PAGE, FuseOpCreate_FileOpenIf)
#pragma alloc_text(PAGE, FuseOpCreate_FileOverwrite)
#pragma alloc_text(PAGE, FuseOpCreate_FileOverwriteIf)
#pragma alloc_text(PAGE, FuseOpCreate_FileOpenTargetDirectory)
#pragma alloc_text(PAGE, FuseOpCreate)
#pragma alloc_text(PAGE, FuseOpOverwrite)
#pragma alloc_text(PAGE, FuseOpCleanup)
#pragma alloc_text(PAGE, FuseOpClose)
#pragma alloc_text(PAGE, FuseOpRead)
#pragma alloc_text(PAGE, FuseOpWrite)
#pragma alloc_text(PAGE, FuseOpQueryInformation)
#pragma alloc_text(PAGE, FuseOpSetInformation)
#pragma alloc_text(PAGE, FuseOpQueryEa)
#pragma alloc_text(PAGE, FuseOpSetEa)
#pragma alloc_text(PAGE, FuseOpFlushBuffers)
#pragma alloc_text(PAGE, FuseOpQueryVolumeInformation)
#pragma alloc_text(PAGE, FuseOpSetVolumeInformation)
#pragma alloc_text(PAGE, FuseOpQueryDirectory)
#pragma alloc_text(PAGE, FuseOpFileSystemControl)
#pragma alloc_text(PAGE, FuseOpDeviceControl)
#pragma alloc_text(PAGE, FuseOpQuerySecurity)
#pragma alloc_text(PAGE, FuseOpSetSecurity)
#pragma alloc_text(PAGE, FuseOpQueryStreamInformation)
#endif

static BOOLEAN FuseOpReserved_Init(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    FUSE_DEVICE_EXTENSION *DeviceExtension;

    coro_block (Context->CoroState)
    {
        coro_await (FuseProtoSendInit(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        DeviceExtension = FuseDeviceExtension(Context->DeviceObject);

        if (FUSE_PROTO_VERSION != Context->FuseResponse->rsp.init.major)
        {
            DeviceExtension->VersionMajor = (UINT32)-1;
            KeSetEvent(&DeviceExtension->InitEvent, 1, FALSE);

            Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_CONNECTION_REFUSED;
            coro_break;
        }

        DeviceExtension->VersionMajor = Context->FuseResponse->rsp.init.major;
        DeviceExtension->VersionMinor = Context->FuseResponse->rsp.init.minor;
        // !!!: REVISIT
        KeSetEvent(&DeviceExtension->InitEvent, 1, FALSE);

        coro_break;
    }

    return coro_active();
}

static BOOLEAN FuseOpReserved_Destroy(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

static BOOLEAN FuseOpReserved_Forget(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    FUSE_DEVICE_EXTENSION *DeviceExtension = FuseDeviceExtension(Context->DeviceObject);

    coro_block (Context->CoroState)
    {
        while (&Context->Forget.ForgetList != Context->Forget.ForgetList.Flink)
        {
            if (16 > DeviceExtension->VersionMinor ||
                &Context->Forget.ForgetList == Context->Forget.ForgetList.Flink->Flink)
            {
                FuseProtoFillForget(Context); /* !coro */
                coro_yield;
            }
            else
            {
                FuseProtoFillBatchForget(Context); /* !coro */
                coro_yield;
            }
        }

        coro_break;
    }

    return coro_active();
}

BOOLEAN FuseOpReserved(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    switch (Context->InternalResponse->Hint)
    {
    case FUSE_PROTO_OPCODE_INIT:
        return FuseOpReserved_Init(Context);
    case FUSE_PROTO_OPCODE_DESTROY:
        return FuseOpReserved_Destroy(Context);
    case FUSE_PROTO_OPCODE_FORGET:
    case FUSE_PROTO_OPCODE_BATCH_FORGET:
        return FuseOpReserved_Forget(Context);
    default:
        return FALSE;
    }
}

/* code borrowed from winfsp/src/ku/posix.c - see there for comments/explanations */
#define FusePosixDefaultPerm            \
    (SYNCHRONIZE | READ_CONTROL | FILE_READ_ATTRIBUTES | FILE_READ_EA)
#define FusePosixOwnerDefaultPerm       \
    (FusePosixDefaultPerm | DELETE | WRITE_DAC | WRITE_OWNER | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA)
static inline ACCESS_MASK FusePosixMapPermissionToAccessMask(UINT32 Mode, UINT32 Perm)
{
    ACCESS_MASK DeleteChild = 0040000 == (Mode & 0041000) ? FILE_DELETE_CHILD : 0;
    return
        ((Perm & 4) ? FILE_READ_DATA : 0) |
        ((Perm & 2) ? FILE_WRITE_ATTRIBUTES | FILE_WRITE_DATA | FILE_APPEND_DATA | DeleteChild : 0) |
        ((Perm & 1) ? FILE_EXECUTE : 0);
}

static NTSTATUS FuseAccessCheck(
    UINT32 FileUid, UINT32 FileGid, UINT32 FileMode,
    UINT32 OrigUid, UINT32 OrigGid, UINT32 DesiredAccess,
    PUINT32 PGrantedAccess)
{
    PAGED_CODE();

    UINT32 FileAccess, RequiredAccess;

    if (OrigUid == FileUid)
        FileAccess = FusePosixOwnerDefaultPerm |
            FusePosixMapPermissionToAccessMask(FileMode & ~001000, (FileMode & 0700) >> 6);
    else if (OrigGid == FileGid)
        FileAccess = FusePosixDefaultPerm |
            FusePosixMapPermissionToAccessMask(FileMode, (FileMode & 0070) >> 3);
    else
        FileAccess = FusePosixDefaultPerm |
            FusePosixMapPermissionToAccessMask(FileMode, (FileMode & 0007));

    RequiredAccess = DesiredAccess & (STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL);

    if (RequiredAccess == (FileAccess & RequiredAccess))
    {
        if (0 != PGrantedAccess)
            *PGrantedAccess = FlagOn(DesiredAccess, MAXIMUM_ALLOWED) ? FileAccess : RequiredAccess;
        return STATUS_SUCCESS;
    }
    else
    {
        if (0 != PGrantedAccess)
            *PGrantedAccess = 0;
        return STATUS_ACCESS_DENIED;
    }
}

static NTSTATUS FusePrepareContextNs(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    FSP_FSCTL_TRANSACT_REQ *InternalRequest = Context->InternalRequest;
    UINT32 Uid = (UINT32)-1, Gid = (UINT32)-1, Pid = (UINT32)-1;
    PSTR PosixPath = 0;
    PWSTR FileName = 0;
    UINT64 AccessToken = 0;
    NTSTATUS Result;

    if (FspFsctlTransactCreateKind == InternalRequest->Kind)
    {
        FileName = (PWSTR)InternalRequest->Buffer;
        AccessToken = InternalRequest->Req.Create.AccessToken;
    }
    else if (FspFsctlTransactSetInformationKind == InternalRequest->Kind &&
        FileRenameInformation == InternalRequest->Req.SetInformation.FileInformationClass)
    {
        FileName = (PWSTR)(InternalRequest->Buffer +
            InternalRequest->Req.SetInformation.Info.Rename.NewFileName.Offset);
        AccessToken = InternalRequest->Req.SetInformation.Info.Rename.AccessToken;
    }

    if (0 != FileName)
    {
        Result = FspPosixMapWindowsToPosixPathEx(FileName, &PosixPath, TRUE);
        if (!NT_SUCCESS(Result))
            goto exit;
    }

    if (0 != AccessToken)
    {
        Result = FuseGetTokenUid(
            FSP_FSCTL_TRANSACT_REQ_TOKEN_HANDLE(AccessToken),
            TokenUser,
            &Uid);
        if (!NT_SUCCESS(Result))
            goto exit;

        Result = FuseGetTokenUid(
            FSP_FSCTL_TRANSACT_REQ_TOKEN_HANDLE(AccessToken),
            TokenPrimaryGroup,
            &Gid);
        if (!NT_SUCCESS(Result))
            goto exit;

        Pid = FSP_FSCTL_TRANSACT_REQ_TOKEN_PID(AccessToken);
    }

    RtlInitString(&Context->Lookup.OrigPath, PosixPath);
    Context->OrigUid = Uid;
    Context->OrigGid = Gid;
    Context->OrigPid = Pid;

    Context->Fini = FusePrepareContextNs_ContextFini;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
        FspPosixDeletePath(PosixPath); /* handles NULL paths */

    return Result;
}

static VOID FusePrepareContextNs_ContextFini(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    FspPosixDeletePath(Context->Lookup.OrigPath.Buffer); /* handles NULL paths */
}

static VOID FuseLookupName(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    FUSE_PROTO_ENTRY EntryBuf, *Entry = &EntryBuf;

    coro_block (Context->CoroState)
    {
        if (!FuseCacheGetEntry(FuseDeviceExtension(Context->DeviceObject)->Cache,
            Context->Ino, &Context->Lookup.Name, Entry))
        {
            if (FUSE_PROTO_ROOT_ID == Context->Ino &&
                1 == Context->Lookup.Name.Length && '/' == Context->Lookup.Name.Buffer[0])
            {
                coro_await (FuseProtoSendGetattr(Context));
                if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                    coro_break;

                Entry->nodeid = FUSE_PROTO_ROOT_ID;
                Entry->entry_valid = Entry->attr_valid =
                    Context->FuseResponse->rsp.getattr.attr_valid;
                Entry->entry_valid_nsec = Entry->attr_valid_nsec =
                    Context->FuseResponse->rsp.getattr.attr_valid_nsec;
                Entry->attr = Context->FuseResponse->rsp.getattr.attr;
            }
            else
            {
                coro_await (FuseProtoSendLookup(Context));
                if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                    coro_break;

                Entry = &Context->FuseResponse->rsp.lookup.entry;
            }

            Context->InternalResponse->IoStatus.Status = FuseCacheSetEntry(
                FuseDeviceExtension(Context->DeviceObject)->Cache,
                Context->Ino, &Context->Lookup.Name, Entry);
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;
        }

        Context->Ino = Entry->nodeid;
        Context->Lookup.Attr = Entry->attr;
        coro_break;
    }
}

static VOID FuseLookupPath(FUSE_CONTEXT *Context)
{
#define RootName                        (1 == Context->Lookup.Name.Length && '/' == Context->Lookup.Name.Buffer[0])
#define LastName                        (0 == Context->Lookup.Remain.Length)
#define UserMode                        (Context->InternalRequest->Req.Create.UserMode)
#define TravPriv                        (Context->InternalRequest->Req.Create.HasTraversePrivilege)

    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        Context->Ino = FUSE_PROTO_ROOT_ID;
        for (;;)
        {
            FusePosixPathPrefix(&Context->Lookup.Remain, &Context->Lookup.Name, &Context->Lookup.Remain);

            /*
             * - RootName:
             *     - UserMode:
             *         - !LastName && TravPriv:
             *             - Lookup
             *             - TraverseCheck
             *         - LastName:
             *             - Lookup
             *             - AccessCheck
             * - !RootName:
             *     - Lookup
             *     - UserMode:
             *         - !LastName && TravPriv:
             *             - TraverseCheck
             *         - LastName:
             *             - AccessCheck
             */
            if (!RootName || (UserMode && (TravPriv || LastName)))
            {
                coro_await (FuseLookupName(Context));
                if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                    coro_break;

                if (UserMode)
                {
                    if (!LastName && TravPriv)
                    {
                        Context->InternalResponse->IoStatus.Status = FuseAccessCheck(
                            Context->Lookup.Attr.uid, Context->Lookup.Attr.gid,
                            Context->Lookup.Attr.mode,
                            Context->OrigUid, Context->OrigGid,
                            FILE_TRAVERSE, 0);
                        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                            coro_break;
                    }
                    else if (LastName)
                    {
                        Context->InternalResponse->IoStatus.Status = FuseAccessCheck(
                            Context->Lookup.Attr.uid, Context->Lookup.Attr.gid,
                            Context->Lookup.Attr.mode,
                            Context->OrigUid, Context->OrigGid,
                            Context->Lookup.DesiredAccess, &Context->Lookup.GrantedAccess);
                        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                            coro_break;
                    }
                }
            }

            if (LastName)
                coro_break;
        }
    }

#undef TravPriv
#undef UserMode
#undef LastName
#undef RootName
}

static VOID FuseCreateCheck(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    /*
     * CreateCheck for a new file consists of checking the parent directory
     * for the FILE_ADD_SUBDIRECTORY or FILE_ADD_FILE rights (depending on
     * whether we are creating a file or directory).
     *
     * If the access check succeeds and MAXIMUM_ALLOWED has been requested
     * then we go ahead and grant all access to the creator.
     */

    coro_block (Context->CoroState)
    {
        if (Context->InternalRequest->Req.Create.HasTrailingBackslash &&
            !FlagOn(Context->InternalRequest->Req.Create.CreateOptions, FILE_DIRECTORY_FILE))
        {
            Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_OBJECT_NAME_INVALID;
            coro_break;
        }

        FusePosixPathSuffix(&Context->Lookup.OrigPath, &Context->Lookup.Remain, 0);

        if (Context->InternalRequest->Req.Create.HasRestorePrivilege)
            Context->Lookup.DesiredAccess = 0;
        else if (FlagOn(Context->InternalRequest->Req.Create.CreateOptions, FILE_DIRECTORY_FILE))
            Context->Lookup.DesiredAccess = FILE_ADD_SUBDIRECTORY;
        else
            Context->Lookup.DesiredAccess = FILE_ADD_FILE;

        coro_await (FuseLookupPath(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess =
            FlagOn(Context->InternalRequest->Req.Create.DesiredAccess, MAXIMUM_ALLOWED) ?
                IoGetFileObjectGenericMapping()->GenericAll :
                Context->InternalRequest->Req.Create.DesiredAccess;
        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess |=
            Context->InternalRequest->Req.Create.GrantedAccess;

        coro_break;
    }
}

static VOID FuseOpenCheck(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    /*
     * OpenCheck consists of checking the file for the desired access,
     * unless FILE_DELETE_ON_CLOSE is requested in which case we also
     * check for DELETE access.
     *
     * If the access check succeeds and MAXIMUM_ALLOWED was not requested
     * then we reset the DELETE access based on whether it was actually
     * requested in DesiredAccess.
     */

    coro_block (Context->CoroState)
    {
        Context->Lookup.Remain = Context->Lookup.OrigPath;

        Context->Lookup.DesiredAccess = Context->InternalRequest->Req.Create.DesiredAccess |
            (FlagOn(Context->InternalRequest->Req.Create.CreateOptions, FILE_DELETE_ON_CLOSE) ?
                DELETE : 0);

        coro_await (FuseLookupPath(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess = Context->Lookup.GrantedAccess;
        if (!FlagOn(Context->InternalRequest->Req.Create.DesiredAccess, MAXIMUM_ALLOWED))
            Context->InternalResponse->Rsp.Create.Opened.GrantedAccess &= ~DELETE |
                (Context->InternalRequest->Req.Create.DesiredAccess & DELETE);
        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess |=
            Context->InternalRequest->Req.Create.GrantedAccess;

        coro_break;
    }
}

static VOID FuseOverwriteCheck(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    /*
     * OverwriteCheck consists of checking the file for the desired access,
     * unless FILE_DELETE_ON_CLOSE is requested in which case we also
     * check for DELETE access. Furthermore we grant DELETE or FILE_WRITE_DATA
     * access based on whether this is a Supersede or Overwrite operation.
     *
     * If the access check succeeds and MAXIMUM_ALLOWED was not requested
     * then we reset the DELETE and FILE_WRITE_DATA accesses based on whether
     * they were actually requested in DesiredAccess.
     */

    coro_block (Context->CoroState)
    {
        Context->Lookup.Remain = Context->Lookup.OrigPath;

        Context->Lookup.DesiredAccess = Context->InternalRequest->Req.Create.DesiredAccess |
            (FILE_SUPERSEDE == ((Context->InternalRequest->Req.Create.CreateOptions >> 24) & 0xff) ?
                DELETE : FILE_WRITE_DATA) |
            (FlagOn(Context->InternalRequest->Req.Create.CreateOptions, FILE_DELETE_ON_CLOSE) ?
                DELETE : 0);

        coro_await (FuseLookupPath(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess = Context->Lookup.GrantedAccess;
        if (!FlagOn(Context->InternalRequest->Req.Create.DesiredAccess, MAXIMUM_ALLOWED))
            Context->InternalResponse->Rsp.Create.Opened.GrantedAccess &= ~(DELETE | FILE_WRITE_DATA) |
                (Context->InternalRequest->Req.Create.DesiredAccess & (DELETE | FILE_WRITE_DATA));
        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess |=
            Context->InternalRequest->Req.Create.GrantedAccess;

        coro_break;
    }
}

static VOID FuseOpenTargetDirectoryCheck(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    /*
     * OpenTargetDirectoryCheck consists of checking the parent directory
     * for the desired access.
     */

    coro_block (Context->CoroState)
    {
        FusePosixPathSuffix(&Context->Lookup.OrigPath, &Context->Lookup.Remain, 0);

        Context->Lookup.DesiredAccess = Context->InternalRequest->Req.Create.DesiredAccess;

        coro_await (FuseLookupPath(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess = Context->Lookup.GrantedAccess;
        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess |=
            Context->InternalRequest->Req.Create.GrantedAccess;

        coro_break;
    }
}

static VOID FuseCreate(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        /* !!!: security descriptor */

        if (FlagOn(Context->InternalRequest->Req.Create.CreateOptions, FILE_DIRECTORY_FILE))
        {
            coro_await (FuseProtoSendMkdir(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            coro_await (FuseProtoSendOpendir(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;
        }
        else
        {
            coro_await (FuseProtoSendCreate(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            {
                if (STATUS_INVALID_DEVICE_REQUEST == Context->InternalResponse->IoStatus.Status)
                {
                    coro_await (FuseProtoSendMknod(Context));
                    if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                        coro_break;

                    coro_await (FuseProtoSendOpen(Context));
                    if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                        coro_break;
                }

                coro_break;
            }
        }

#if 0
        if (CreateOptions & FILE_DIRECTORY_FILE)
        {
            if (0 != f->ops.mkdir)
            {
                err = f->ops.mkdir(contexthdr->PosixPath, Mode);
                if (0 != err)
                {
                    Result = fsp_fuse_ntstatus_from_errno(f->env, err);
                    goto exit;
                }

                if (0 != f->ops.opendir)
                {
                    err = f->ops.opendir(contexthdr->PosixPath, &fi);
                    Result = fsp_fuse_ntstatus_from_errno(f->env, err);
                }
                else
                {
                    fi.fh = -1;
                    Result = STATUS_SUCCESS;
                }
            }
            else
                Result = STATUS_INVALID_DEVICE_REQUEST;
        }
        else
        {
            if (0 != f->ops.create)
            {
                err = f->ops.create(contexthdr->PosixPath, Mode, &fi);
                Result = fsp_fuse_ntstatus_from_errno(f->env, err);
            }
            else if (0 != f->ops.mknod && 0 != f->ops.open)
            {
                err = f->ops.mknod(contexthdr->PosixPath, Mode, 0);
                if (0 != err)
                {
                    Result = fsp_fuse_ntstatus_from_errno(f->env, err);
                    goto exit;
                }

                err = f->ops.open(contexthdr->PosixPath, &fi);
                Result = fsp_fuse_ntstatus_from_errno(f->env, err);
            }
            else
                Result = STATUS_INVALID_DEVICE_REQUEST;
        }
#endif

        coro_break;
    }

#if 0
    struct fuse *f = FileSystem->UserContext;
    struct fuse_context *context = fsp_fuse_get_context(f->env);
    struct fsp_fuse_context_header *contexthdr = FSP_FUSE_HDR_FROM_CONTEXT(context);
    UINT32 Uid, Gid, Mode;
    FSP_FSCTL_FILE_INFO FileInfoBuf;
    struct fsp_fuse_file_desc *filedesc = 0;
    struct fuse_file_info fi;
    BOOLEAN Opened = FALSE;
    int err;
    NTSTATUS Result;

    Uid = context->uid;
    Gid = context->gid;
    Mode = 0777;
    if (0 != SecurityDescriptor)
    {
        Result = FspPosixMapSecurityDescriptorToPermissions(SecurityDescriptor,
            &Uid, &Gid, &Mode);
        if (!NT_SUCCESS(Result))
            goto exit;
    }
    Mode &= ~context->umask;
    if (CreateOptions & FILE_DIRECTORY_FILE)
    {
        if (f->set_create_dir_umask)
            Mode = 0777 & ~f->create_dir_umask;
        else
        if (f->set_create_umask)
            Mode = 0777 & ~f->create_umask;
    }
    else
    {
        if (f->set_create_file_umask)
            Mode = 0777 & ~f->create_file_umask;
        else
        if (f->set_create_umask)
            Mode = 0777 & ~f->create_umask;
    }

    memset(&fi, 0, sizeof fi);
    if ('C' == f->env->environment) /* Cygwin */
        fi.flags = 0x0200 | 0x0800 | 2 /*O_CREAT|O_EXCL|O_RDWR*/;
    else
        fi.flags = 0x0100 | 0x0400 | 2 /*O_CREAT|O_EXCL|O_RDWR*/;

    if (CreateOptions & FILE_DIRECTORY_FILE)
    {
        if (0 != f->ops.mkdir)
        {
            err = f->ops.mkdir(contexthdr->PosixPath, Mode);
            if (0 != err)
            {
                Result = fsp_fuse_ntstatus_from_errno(f->env, err);
                goto exit;
            }

            if (0 != f->ops.opendir)
            {
                err = f->ops.opendir(contexthdr->PosixPath, &fi);
                Result = fsp_fuse_ntstatus_from_errno(f->env, err);
            }
            else
            {
                fi.fh = -1;
                Result = STATUS_SUCCESS;
            }
        }
        else
            Result = STATUS_INVALID_DEVICE_REQUEST;
    }
    else
    {
        if (0 != f->ops.create)
        {
            err = f->ops.create(contexthdr->PosixPath, Mode, &fi);
            Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        }
        else if (0 != f->ops.mknod && 0 != f->ops.open)
        {
            err = f->ops.mknod(contexthdr->PosixPath, Mode, 0);
            if (0 != err)
            {
                Result = fsp_fuse_ntstatus_from_errno(f->env, err);
                goto exit;
            }

            err = f->ops.open(contexthdr->PosixPath, &fi);
            Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        }
        else
            Result = STATUS_INVALID_DEVICE_REQUEST;
    }
    if (!NT_SUCCESS(Result))
        goto exit;

    Opened = TRUE;

    if (0 != FileAttributes &&
        0 != (f->conn_want & FSP_FUSE_CAP_STAT_EX) && 0 != f->ops.chflags)
    {
        err = f->ops.chflags(contexthdr->PosixPath,
            fsp_fuse_intf_MapFileAttributesToFlags(CreateOptions & FILE_DIRECTORY_FILE ?
                FileAttributes : FileAttributes | FILE_ATTRIBUTE_ARCHIVE));
        Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        if (!NT_SUCCESS(Result) && STATUS_INVALID_DEVICE_REQUEST != Result)
            goto exit;
    }

    if ((Uid != context->uid || Gid != context->gid) &&
        0 != f->ops.chown)
    {
        err = f->ops.chown(contexthdr->PosixPath, Uid, Gid);
        Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        if (!NT_SUCCESS(Result) && STATUS_INVALID_DEVICE_REQUEST != Result)
            goto exit;
    }

    if (0 != ExtraBuffer)
    {
        if (!ExtraBufferIsReparsePoint)
        {
            Result = FspFileSystemEnumerateEa(FileSystem,
                fsp_fuse_intf_SetEaEntry, contexthdr->PosixPath, ExtraBuffer, ExtraLength);
            if (!NT_SUCCESS(Result))
                goto exit;
        }
        else
        {
            /* !!!: revisit: WslFeatures, GetFileInfoFunnel, GetReparsePointEx, SetReparsePoint */
            Result = STATUS_INVALID_PARAMETER;
            goto exit;
        }
    }
    /*
     * Ignore fuse_file_info::direct_io, fuse_file_info::keep_cache.
     * NOTE: Originally WinFsp dit not support disabling the cache manager
     * for an individual file. This is now possible and we should revisit.
     *
     * Ignore fuse_file_info::nonseekable.
     */

    Result = fsp_fuse_intf_GetFileInfoEx(FileSystem, contexthdr->PosixPath, &fi,
        &Uid, &Gid, &Mode, &FileInfoBuf);
    if (!NT_SUCCESS(Result))
        goto exit;

    *PFileDesc = filedesc;
    memcpy(FileInfo, &FileInfoBuf, sizeof FileInfoBuf);

    filedesc->PosixPath = contexthdr->PosixPath;
    filedesc->IsDirectory = !!(FileInfoBuf.FileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    filedesc->IsReparsePoint = FALSE;
    filedesc->OpenFlags = fi.flags;
    filedesc->FileHandle = fi.fh;
    filedesc->DirBuffer = 0;
    contexthdr->PosixPath = 0;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
    {
        if (Opened)
        {
            if (CreateOptions & FILE_DIRECTORY_FILE)
            {
                if (0 != f->ops.releasedir)
                    f->ops.releasedir(filedesc->PosixPath, &fi);
            }
            else
            {
                if (0 != f->ops.release)
                    f->ops.release(filedesc->PosixPath, &fi);
            }
        }

        MemFree(filedesc);
    }

    return Result;
#endif
#if 0
    NTSTATUS Result;
    UINT32 GrantedAccess;
    PSECURITY_DESCRIPTOR ParentDescriptor;
    FSP_FSCTL_TRANSACT_FULL_CONTEXT FullContext;
    FSP_FSCTL_OPEN_FILE_INFO OpenFileInfo;
    PSECURITY_DESCRIPTOR OpenDescriptor = 0;

    Result = FspFileSystemCreateCheck(FileSystem, Request, Response, TRUE,
        &GrantedAccess, &ParentDescriptor);
    if (!NT_SUCCESS(Result) || STATUS_REPARSE == Result)
        return Result;

    Result = FspCreateSecurityDescriptor(FileSystem, Request, ParentDescriptor, &OpenDescriptor);
    FspDeleteSecurityDescriptor(ParentDescriptor, FspAccessCheckEx);
    if (!NT_SUCCESS(Result))
        return Result;

    FullContext.UserContext = 0;
    FullContext.UserContext2 = 0;
    memset(&OpenFileInfo, 0, sizeof OpenFileInfo);
    OpenFileInfo.NormalizedName = (PVOID)Response->Buffer;
    OpenFileInfo.NormalizedNameSize = FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX;
    if (0 != FileSystem->Interface->CreateEx)
        Result = FileSystem->Interface->CreateEx(FileSystem,
            (PWSTR)Request->Buffer, Request->Req.Create.CreateOptions, GrantedAccess,
            Request->Req.Create.FileAttributes, OpenDescriptor, Request->Req.Create.AllocationSize,
            0 != Request->Req.Create.Ea.Size ?
                (PVOID)(Request->Buffer + Request->Req.Create.Ea.Offset) : 0,
            Request->Req.Create.Ea.Size,
            Request->Req.Create.EaIsReparsePoint,
            AddrOfFileContext(FullContext), &OpenFileInfo.FileInfo);
    else
        Result = FileSystem->Interface->Create(FileSystem,
            (PWSTR)Request->Buffer, Request->Req.Create.CreateOptions, GrantedAccess,
            Request->Req.Create.FileAttributes, OpenDescriptor, Request->Req.Create.AllocationSize,
            AddrOfFileContext(FullContext), &OpenFileInfo.FileInfo);
    if (!NT_SUCCESS(Result))
    {
        FspDeleteSecurityDescriptor(OpenDescriptor, FspCreateSecurityDescriptor);
        return Result;
    }

    if (FSP_FSCTL_TRANSACT_PATH_SIZEMAX >= OpenFileInfo.NormalizedNameSize)
    {
        Response->Size = (UINT16)(sizeof *Response + OpenFileInfo.NormalizedNameSize);
        Response->Rsp.Create.Opened.FileName.Offset = 0;
        Response->Rsp.Create.Opened.FileName.Size = (UINT16)OpenFileInfo.NormalizedNameSize;
    }

    if (0 != OpenDescriptor)
    {
        FspFileSystemOpCreate_SetOpenDescriptor(Response, OpenDescriptor);
        FspDeleteSecurityDescriptor(OpenDescriptor, FspCreateSecurityDescriptor);
    }

    Response->IoStatus.Information = FILE_CREATED;
    SetFileContext(Response->Rsp.Create.Opened, FullContext);
    Response->Rsp.Create.Opened.GrantedAccess = GrantedAccess;
    memcpy(&Response->Rsp.Create.Opened.FileInfo,
        &OpenFileInfo.FileInfo, sizeof OpenFileInfo.FileInfo);
    return STATUS_SUCCESS;
#endif
}

static VOID FuseOpen(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    VOID (*Fn)(FUSE_CONTEXT *) = 0;

    coro_block (Context->CoroState)
    {
        switch (Context->Lookup.Attr.mode & 0170000)
        {
        case 0040000: /* S_IFDIR */
            /* FILE_ATTRIBUTE_DIRECTORY */
            Fn = FuseProtoSendOpendir;
            break;
        case 0010000: /* S_IFIFO */
        case 0020000: /* S_IFCHR */
        case 0060000: /* S_IFBLK */
        case 0140000: /* S_IFSOCK */
            /* FILE_ATTRIBUTE_REPARSE_POINT/IO_REPARSE_TAG_NFS */
            Fn = 0;
            break;
        case 0120000: /* S_IFLNK */
            /* FILE_ATTRIBUTE_REPARSE_POINT/IO_REPARSE_TAG_SYMLINK */
            Fn = 0;
            break;
        default:
            Fn = FuseProtoSendOpen;
            break;
        }

        if (0 == Fn)
        {
            // !!!: REVISIT!
            Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_OBJECT_NAME_NOT_FOUND;
            coro_break;
        }

        coro_await (Fn(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->IoStatus.Information = FILE_OPENED;
        Context->InternalResponse->Rsp.Create.Opened.UserContext2 =
            Context->FuseResponse->rsp.open.fh;
        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess =
            Context->Lookup.GrantedAccess;
        FuseAttrToFileInfo(Context->DeviceObject, &Context->Lookup.Attr,
            &Context->InternalResponse->Rsp.Create.Opened.FileInfo);
        Context->InternalResponse->Rsp.Create.Opened.DisableCache =
            BooleanFlagOn(Context->FuseResponse->rsp.open.open_flags, FUSE_PROTO_OPEN_DIRECT_IO);

        coro_break;
    }
}

static VOID FuseOpCreate_FileCreate(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        coro_await (FuseCreateCheck(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        FusePosixPathSuffix(&Context->Lookup.OrigPath, 0, &Context->Lookup.Name);
        coro_await (FuseCreate(Context));

        coro_break;
    }
}

static VOID FuseOpCreate_FileOpen(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        coro_await (FuseOpenCheck(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        coro_await (FuseOpen(Context));

        coro_break;
    }
}

static VOID FuseOpCreate_FileOpenIf(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        coro_await (FuseOpenCheck(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
        {
            if (STATUS_OBJECT_NAME_NOT_FOUND != Context->InternalResponse->IoStatus.Status)
                coro_break;

            coro_await (FuseCreateCheck(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            FusePosixPathSuffix(&Context->Lookup.OrigPath, 0, &Context->Lookup.Name);
            coro_await (FuseCreate(Context));
        }
        else
            coro_await (FuseOpen(Context));

        coro_break;
    }
}

static VOID FuseOpCreate_FileOverwrite(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        coro_await (FuseOverwriteCheck(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        coro_await (FuseOpen(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->IoStatus.Information = FILE_OVERWRITTEN;

        coro_break;
    }
}

static VOID FuseOpCreate_FileOverwriteIf(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        coro_await (FuseOverwriteCheck(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
        {
            if (STATUS_OBJECT_NAME_NOT_FOUND != Context->InternalResponse->IoStatus.Status)
                coro_break;

            coro_await (FuseCreateCheck(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            FusePosixPathSuffix(&Context->Lookup.OrigPath, 0, &Context->Lookup.Name);
            coro_await (FuseCreate(Context));
        }
        else
        {
            coro_await (FuseOpen(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            Context->InternalResponse->IoStatus.Information = FILE_OVERWRITTEN;
        }

        coro_break;
    }
}

static VOID FuseOpCreate_FileOpenTargetDirectory(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        coro_await (FuseOpenTargetDirectoryCheck(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        coro_await (FuseOpen(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        FusePosixPathSuffix(&Context->Lookup.OrigPath, 0, &Context->Lookup.Name);
        coro_await (FuseLookupName(Context));
        Context->InternalResponse->IoStatus.Information =
            NT_SUCCESS(Context->InternalResponse->IoStatus.Status) ?
                FILE_EXISTS : FILE_DOES_NOT_EXIST;

        coro_break;
    }
}

BOOLEAN FuseOpCreate(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    VOID (*Fn)(FUSE_CONTEXT *) = 0;
    NTSTATUS Result;

    coro_block (Context->CoroState)
    {
        if (Context->InternalRequest->Req.Create.NamedStream)
        {
            Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_OBJECT_NAME_INVALID;
            coro_break;
        }

        Result = FusePrepareContextNs(Context);
        if (!NT_SUCCESS(Result))
        {
            Context->InternalResponse->IoStatus.Status = Result;
            coro_break;
        }

        if (Context->InternalRequest->Req.Create.OpenTargetDirectory)
            Fn = FuseOpCreate_FileOpenTargetDirectory;
        else
            switch ((Context->InternalRequest->Req.Create.CreateOptions >> 24) & 0xff)
            {
            case FILE_CREATE:
                Fn = FuseOpCreate_FileCreate;
                break;
            case FILE_OPEN:
                Fn = FuseOpCreate_FileOpen;
                break;
            case FILE_OPEN_IF:
                Fn = FuseOpCreate_FileOpenIf;
                break;
            case FILE_OVERWRITE:
                Fn = FuseOpCreate_FileOverwrite;
                break;
            case FILE_OVERWRITE_IF:
            case FILE_SUPERSEDE:
                Fn = FuseOpCreate_FileOverwriteIf;
                break;
            }

        if (0 != Fn)
            coro_await (Fn(Context));
        else
            Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_INVALID_PARAMETER;

        coro_break;
    }

    return coro_active();
}

BOOLEAN FuseOpOverwrite(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

BOOLEAN FuseOpCleanup(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

BOOLEAN FuseOpClose(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

BOOLEAN FuseOpRead(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

BOOLEAN FuseOpWrite(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

BOOLEAN FuseOpQueryInformation(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

BOOLEAN FuseOpSetInformation(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

BOOLEAN FuseOpQueryEa(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

BOOLEAN FuseOpSetEa(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

BOOLEAN FuseOpFlushBuffers(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

BOOLEAN FuseOpQueryVolumeInformation(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

BOOLEAN FuseOpSetVolumeInformation(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

BOOLEAN FuseOpQueryDirectory(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

BOOLEAN FuseOpFileSystemControl(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

BOOLEAN FuseOpDeviceControl(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

BOOLEAN FuseOpQuerySecurity(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

BOOLEAN FuseOpSetSecurity(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

BOOLEAN FuseOpQueryStreamInformation(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}
