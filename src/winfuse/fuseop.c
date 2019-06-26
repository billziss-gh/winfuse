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
static PSTR FusePosixPathFirstName(PSTR PosixPath);
static PSTR FusePosixPathLastSlash(PSTR PosixPath);
static NTSTATUS FuseAccessCheck(
    UINT32 FileUid, UINT32 FileGid, UINT32 FileMode,
    UINT32 OrigUid, UINT32 OrigGid, UINT32 DesiredAccess,
    PUINT32 PGrantedAccess);
static VOID FusePrepareContextNs_ContextFini(FUSE_CONTEXT *Context);
static NTSTATUS FusePrepareContextNs(FUSE_CONTEXT *Context);
static VOID FuseLookupName(FUSE_CONTEXT *Context);
static VOID FuseLookupPath(FUSE_CONTEXT *Context);
static VOID FuseCreateCheck(FUSE_CONTEXT *Context);
static VOID FuseOpenCheck(FUSE_CONTEXT *Context);
static VOID FuseOverwriteCheck(FUSE_CONTEXT *Context);
static VOID FuseOpenTargetDirectoryCheck(FUSE_CONTEXT *Context);
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
#pragma alloc_text(PAGE, FusePosixPathFirstName)
#pragma alloc_text(PAGE, FusePosixPathLastSlash)
#pragma alloc_text(PAGE, FuseAccessCheck)
#pragma alloc_text(PAGE, FusePrepareContextNs_ContextFini)
#pragma alloc_text(PAGE, FusePrepareContextNs)
#pragma alloc_text(PAGE, FuseLookupName)
#pragma alloc_text(PAGE, FuseLookupPath)
#pragma alloc_text(PAGE, FuseCreateCheck)
#pragma alloc_text(PAGE, FuseOpenCheck)
#pragma alloc_text(PAGE, FuseOverwriteCheck)
#pragma alloc_text(PAGE, FuseOpenTargetDirectoryCheck)
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
        while (&Context->ForgetList != Context->ForgetList.Flink)
        {
            if (16 > DeviceExtension->VersionMinor ||
                &Context->ForgetList == Context->ForgetList.Flink->Flink)
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

static inline PSTR FusePosixPathEnd(PSTR PosixPath)
{
    return PosixPath + strlen(PosixPath);
}

static PSTR FusePosixPathFirstName(PSTR PosixPath)
{
    PAGED_CODE();

    PSTR P;

    P = PosixPath;
    while ('/' == *P)
        P++;

    return P;
}

static PSTR FusePosixPathLastSlash(PSTR PosixPath)
{
    PAGED_CODE();

    PSTR P, LastSlash;

    LastSlash = P = PosixPath;

    while (*P)
        if ('/' == *P)
        {
            LastSlash = P;
            while ('/' == *++P)
                ;
        }
        else
            P++;

    return LastSlash;
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

static VOID FusePrepareContextNs_ContextFini(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    FspPosixDeletePath(Context->PosixPath); /* handles NULL paths */
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

    Context->PosixPath = PosixPath;
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

static VOID FuseLookupName(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    STRING Name;
    FUSE_PROTO_ENTRY Entry;

    /* executed on coro reentry! */
    Name.Length = Name.MaximumLength = (USHORT)(Context->PosixPathRem - Context->PosixName);
    Name.Buffer = Context->PosixName;

    coro_block (Context->CoroState)
    {
        if (FuseCacheGetEntry(FuseDeviceExtension(Context->DeviceObject)->Cache,
            Context->Ino, &Name, &Entry))
        {
            Context->Ino = Entry.nodeid;
            Context->FileUid = Entry.attr.uid;
            Context->FileGid = Entry.attr.gid;
            Context->FileMode = Entry.attr.mode;
            coro_break;
        }

        coro_await (FuseProtoSendLookup(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->IoStatus.Status = FuseCacheSetEntry(
            FuseDeviceExtension(Context->DeviceObject)->Cache,
            Context->Ino, &Name, &Context->FuseResponse->rsp.lookup.entry);
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->Ino = Context->FuseResponse->rsp.lookup.entry.nodeid;
        Context->FileUid = Context->FuseResponse->rsp.lookup.entry.attr.uid;
        Context->FileGid = Context->FuseResponse->rsp.lookup.entry.attr.gid;
        Context->FileMode = Context->FuseResponse->rsp.lookup.entry.attr.mode;
        coro_break;
    }
}

static VOID FuseLookupPath(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    PSTR P;

    coro_block (Context->CoroState)
    {
        P = Context->PosixPath;
        while ('/' == *P)
            P++;
        Context->PosixPathRem = Context->PosixName = P;
        Context->Ino = FUSE_PROTO_ROOT_ID;

        for (;;)
        {
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
#define RootName                        (Context->PosixName == Context->PosixPathRem)
#define LastName                        (Context->PosixPathEnd == Context->PosixPathRem)
#define UserMode                        (Context->InternalRequest->Req.Create.UserMode)
#define TravPriv                        (Context->InternalRequest->Req.Create.HasTraversePrivilege)
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
                            Context->FileUid, Context->FileGid, Context->FileMode,
                            Context->OrigUid, Context->OrigGid,
                            FILE_TRAVERSE, 0);
                        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                            coro_break;
                    }
                    else if (LastName)
                    {
                        Context->InternalResponse->IoStatus.Status = FuseAccessCheck(
                            Context->FileUid, Context->FileGid, Context->FileMode,
                            Context->OrigUid, Context->OrigGid,
                            Context->DesiredAccess, &Context->GrantedAccess);
                        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                            coro_break;
                    }
                }
            }
#undef TravPriv
#undef UserMode
#undef LastName
#undef RootName

            P = Context->PosixPathRem;
            while ('/' == *P)
                P++;
            Context->PosixName = P;
            while (Context->PosixPathEnd > P && '/' != *P)
                P++;
            Context->PosixPathRem = P;
            if (Context->PosixName == P)
                coro_break;
        }
    }
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

        Context->PosixPathEnd = FusePosixPathLastSlash(Context->PosixPath);

        if (Context->InternalRequest->Req.Create.HasRestorePrivilege)
            Context->DesiredAccess = 0;
        else if (FlagOn(Context->InternalRequest->Req.Create.CreateOptions, FILE_DIRECTORY_FILE))
            Context->DesiredAccess = FILE_ADD_SUBDIRECTORY;
        else
            Context->DesiredAccess = FILE_ADD_FILE;

        if (0 == Context->PosixName)
        {
            coro_await (FuseLookupPath(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;
        }
        else
        {
            if (Context->InternalRequest->Req.Create.UserMode)
            {
                Context->InternalResponse->IoStatus.Status = FuseAccessCheck(
                    Context->FileUid, Context->FileGid, Context->FileMode,
                    Context->OrigUid, Context->OrigGid,
                    Context->DesiredAccess, &Context->GrantedAccess);
                if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                    coro_break;
            }
        }

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
        Context->PosixPathEnd = FusePosixPathEnd(Context->PosixPath);

        Context->DesiredAccess = Context->InternalRequest->Req.Create.DesiredAccess |
            (FlagOn(Context->InternalRequest->Req.Create.CreateOptions, FILE_DELETE_ON_CLOSE) ?
                DELETE : 0);

        coro_await (FuseLookupPath(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess = Context->GrantedAccess;
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
        Context->PosixPathEnd = FusePosixPathEnd(Context->PosixPath);

        Context->DesiredAccess = Context->InternalRequest->Req.Create.DesiredAccess |
            (FILE_SUPERSEDE == ((Context->InternalRequest->Req.Create.CreateOptions >> 24) & 0xff) ?
                DELETE : FILE_WRITE_DATA) |
            (FlagOn(Context->InternalRequest->Req.Create.CreateOptions, FILE_DELETE_ON_CLOSE) ?
                DELETE : 0);

        coro_await (FuseLookupPath(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess = Context->GrantedAccess;
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
        Context->PosixPathEnd = FusePosixPathLastSlash(Context->PosixPath);

        Context->DesiredAccess = Context->InternalRequest->Req.Create.DesiredAccess;

        coro_await (FuseLookupPath(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess = Context->GrantedAccess;
        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess |=
            Context->InternalRequest->Req.Create.GrantedAccess;

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

        Context->PosixName = FusePosixPathFirstName(Context->PosixPathEnd + 1);
        coro_await (FuseProtoSendCreate(Context));

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

        coro_await (FuseProtoSendOpen(Context));

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

            Context->PosixName = FusePosixPathFirstName(Context->PosixPathEnd + 1);
            coro_await (FuseProtoSendCreate(Context));
        }
        else
            coro_await (FuseProtoSendOpen(Context));

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

        coro_await (FuseProtoSendOpen(Context));
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

            Context->PosixName = FusePosixPathFirstName(Context->PosixPathEnd + 1);
            coro_await (FuseProtoSendCreate(Context));
        }
        else
        {
            coro_await (FuseProtoSendOpen(Context));
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

        coro_await (FuseProtoSendOpen(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->PosixName = FusePosixPathFirstName(Context->PosixPathEnd + 1);
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
