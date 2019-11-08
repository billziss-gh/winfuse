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
static VOID FusePrepareLookup(FUSE_CONTEXT *Context);
static VOID FusePrepareLookup_ContextFini(FUSE_CONTEXT *Context);
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
static VOID FuseOpClose_ContextFini(FUSE_CONTEXT *Context);
BOOLEAN FuseOpRead(FUSE_CONTEXT *Context);
BOOLEAN FuseOpWrite(FUSE_CONTEXT *Context);
BOOLEAN FuseOpQueryInformation(FUSE_CONTEXT *Context);
BOOLEAN FuseOpSetInformation(FUSE_CONTEXT *Context);
BOOLEAN FuseOpQueryEa(FUSE_CONTEXT *Context);
BOOLEAN FuseOpSetEa(FUSE_CONTEXT *Context);
BOOLEAN FuseOpFlushBuffers(FUSE_CONTEXT *Context);
BOOLEAN FuseOpQueryVolumeInformation(FUSE_CONTEXT *Context);
BOOLEAN FuseOpSetVolumeInformation(FUSE_CONTEXT *Context);
static BOOLEAN FuseAddDirInfo(FUSE_CONTEXT *Context,
    PSTRING Name, UINT64 NextOffset, FUSE_PROTO_ATTR *Attr,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred);
static VOID FuseOpQueryDirectory_GetDirInfoByName(FUSE_CONTEXT *Context);
static VOID FuseOpQueryDirectory_ReadDirectory(FUSE_CONTEXT *Context);
BOOLEAN FuseOpQueryDirectory(FUSE_CONTEXT *Context);
static VOID FuseOpQueryDirectory_ContextFini(FUSE_CONTEXT *Context);
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
#pragma alloc_text(PAGE, FusePrepareLookup)
#pragma alloc_text(PAGE, FusePrepareLookup_ContextFini)
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
#pragma alloc_text(PAGE, FuseOpClose_ContextFini)
#pragma alloc_text(PAGE, FuseOpRead)
#pragma alloc_text(PAGE, FuseOpWrite)
#pragma alloc_text(PAGE, FuseOpQueryInformation)
#pragma alloc_text(PAGE, FuseOpSetInformation)
#pragma alloc_text(PAGE, FuseOpQueryEa)
#pragma alloc_text(PAGE, FuseOpSetEa)
#pragma alloc_text(PAGE, FuseOpFlushBuffers)
#pragma alloc_text(PAGE, FuseOpQueryVolumeInformation)
#pragma alloc_text(PAGE, FuseOpSetVolumeInformation)
#pragma alloc_text(PAGE, FuseAddDirInfo)
#pragma alloc_text(PAGE, FuseOpQueryDirectory_GetDirInfoByName)
#pragma alloc_text(PAGE, FuseOpQueryDirectory_ReadDirectory)
#pragma alloc_text(PAGE, FuseOpQueryDirectory)
#pragma alloc_text(PAGE, FuseOpQueryDirectory_ContextFini)
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
                FuseProtoFillForget(Context); /* !coro */
            else
                FuseProtoFillBatchForget(Context); /* !coro */
            coro_yield;
        }
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

static VOID FusePrepareLookup(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    UINT32 Uid = 0, Gid = 0, Pid = 0;
    PVOID CacheGen = 0;
    PSTR PosixPath = 0;
    PWSTR FileName = 0;
    UINT64 AccessToken = 0;
    PACCESS_TOKEN AccessTokenObject = 0;
    UINT32 IsUserMode = 1;
    UINT32 HasTraversePrivilege = 0;

    switch (Context->InternalRequest->Kind)
    {
    case FspFsctlTransactCreateKind:
        FileName = (PWSTR)Context->InternalRequest->Buffer;
        AccessToken = Context->InternalRequest->Req.Create.AccessToken;
        IsUserMode = Context->InternalRequest->Req.Create.UserMode;
        HasTraversePrivilege = Context->InternalRequest->Req.Create.HasTraversePrivilege;
        break;
    case FspFsctlTransactCleanupKind:
        ASSERT(Context->InternalRequest->Req.Cleanup.Delete);
        FileName = (PWSTR)Context->InternalRequest->Buffer;
        IsUserMode = 0;
        HasTraversePrivilege = 1;
        break;
    case FspFsctlTransactSetInformationKind:
        ASSERT(FileRenameInformation ==
            Context->InternalRequest->Req.SetInformation.FileInformationClass);
        FileName = (PWSTR)(Context->InternalRequest->Buffer +
            Context->InternalRequest->Req.SetInformation.Info.Rename.NewFileName.Offset);
        AccessToken = Context->InternalRequest->Req.SetInformation.Info.Rename.AccessToken;
        IsUserMode = 1;
        HasTraversePrivilege = 1;
        break;
    default:
        ASSERT(FALSE);
        Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_INVALID_PARAMETER;
        goto exit;
    }

    if (0 != FileName)
    {
        Context->InternalResponse->IoStatus.Status = FuseCacheReferenceGen(
            FuseDeviceExtension(Context->DeviceObject)->Cache, &CacheGen);
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            goto exit;

        Context->InternalResponse->IoStatus.Status = FspPosixMapWindowsToPosixPathEx(
            FileName, &PosixPath, TRUE);
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            goto exit;
    }

    if (0 != AccessToken)
    {
        Context->InternalResponse->IoStatus.Status = ObReferenceObjectByHandle(
            FSP_FSCTL_TRANSACT_REQ_TOKEN_HANDLE(AccessToken),
            TOKEN_QUERY,
            *SeTokenObjectType,
            UserMode,
            &AccessTokenObject,
            0/*HandleInformation*/);
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            goto exit;

        Context->InternalResponse->IoStatus.Status = FuseGetTokenUid(
            AccessTokenObject, TokenUser, &Uid);
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            goto exit;

        Context->InternalResponse->IoStatus.Status = FuseGetTokenUid(
            AccessTokenObject, TokenPrimaryGroup, &Gid);
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            goto exit;

        ObDereferenceObject(AccessTokenObject);

        Pid = FSP_FSCTL_TRANSACT_REQ_TOKEN_PID(AccessToken);
    }

    Context->OrigUid = Uid;
    Context->OrigGid = Gid;
    Context->OrigPid = Pid;

    RtlInitString(&Context->Create.OrigPath, PosixPath);
    Context->Create.CacheGen = CacheGen;
    Context->Create.UserMode = UserMode;
    Context->Create.HasTraversePrivilege = HasTraversePrivilege;

    Context->Fini = FusePrepareLookup_ContextFini;

    Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
    {
        FspPosixDeletePath(PosixPath);
            /* handles NULL paths */
        FuseCacheDereferenceGen(FuseDeviceExtension(Context->DeviceObject)->Cache, CacheGen);
            /* handles NULL gens */
    }
}

static VOID FusePrepareLookup_ContextFini(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    if (FspFsctlTransactCreateKind == Context->InternalRequest->Kind &&
        0 != Context->File)
        FuseFileDelete(Context->DeviceObject, Context->File);

    FspPosixDeletePath(Context->Create.OrigPath.Buffer);
        /* handles NULL paths */
    FuseCacheDereferenceGen(FuseDeviceExtension(Context->DeviceObject)->Cache, Context->Create.CacheGen);
        /* handles NULL gens */
}

static VOID FuseLookupName(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    FUSE_PROTO_ENTRY EntryBuf, *Entry = &EntryBuf;

    coro_block (Context->CoroState)
    {
        if (!FuseCacheGetEntry(FuseDeviceExtension(Context->DeviceObject)->Cache,
            Context->Lookup.Ino, &Context->Lookup.Name, Entry))
        {
            if (FUSE_PROTO_ROOT_INO == Context->Lookup.Ino &&
                1 == Context->Lookup.Name.Length && '/' == Context->Lookup.Name.Buffer[0])
            {
                coro_await (FuseProtoSendGetattr(Context));
                if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                    coro_break;

                RtlZeroMemory(Entry, sizeof *Entry);
                Entry->nodeid = FUSE_PROTO_ROOT_INO;
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

            FuseCacheSetEntry(
                FuseDeviceExtension(Context->DeviceObject)->Cache,
                Context->Lookup.Ino, &Context->Lookup.Name, Entry);
        }

        Context->Lookup.Ino = Entry->nodeid;
        Context->Lookup.Attr = Entry->attr;
    }
}

static VOID FuseLookupPath(FUSE_CONTEXT *Context)
{
#define RootName                        (1 == Context->Create.Name.Length && '/' == Context->Create.Name.Buffer[0])
#define LastName                        (0 == Context->Create.Remain.Length)
#define UserMode                        (Context->InternalRequest->Req.Create.UserMode)
#define TravPriv                        (Context->InternalRequest->Req.Create.HasTraversePrivilege)

    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        Context->Create.Ino = FUSE_PROTO_ROOT_INO;
        while (1) /* for (;;) produces "warning C4702: unreachable code" */
        {
            FusePosixPathPrefix(&Context->Create.Remain, &Context->Create.Name, &Context->Create.Remain);

            /*
             * - RootName:
             *     - UserMode:
             *         - !LastName && !TravPriv:
             *             - Lookup
             *             - TraverseCheck
             *         - LastName:
             *             - Lookup
             *             - AccessCheck
             * - !RootName:
             *     - Lookup
             *     - UserMode:
             *         - !LastName && !TravPriv:
             *             - TraverseCheck
             *         - LastName:
             *             - AccessCheck
             */
            if (!RootName || (UserMode && (!TravPriv || LastName)))
            {
                coro_await (FuseLookupName(Context));
                if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                    coro_break;

                if (UserMode)
                {
                    if (!LastName && !TravPriv)
                    {
                        Context->InternalResponse->IoStatus.Status = FuseAccessCheck(
                            Context->Create.Attr.uid, Context->Create.Attr.gid,
                            Context->Create.Attr.mode,
                            Context->OrigUid, Context->OrigGid,
                            FILE_TRAVERSE, 0);
                        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                            coro_break;
                    }
                    else if (LastName)
                    {
                        Context->InternalResponse->IoStatus.Status = FuseAccessCheck(
                            Context->Create.Attr.uid, Context->Create.Attr.gid,
                            Context->Create.Attr.mode,
                            Context->OrigUid, Context->OrigGid,
                            Context->Create.DesiredAccess, &Context->Create.GrantedAccess);
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

        FusePosixPathSuffix(&Context->Create.OrigPath, &Context->Create.Remain, 0);

        if (Context->InternalRequest->Req.Create.HasRestorePrivilege)
            Context->Create.DesiredAccess = 0;
        else if (FlagOn(Context->InternalRequest->Req.Create.CreateOptions, FILE_DIRECTORY_FILE))
            Context->Create.DesiredAccess = FILE_ADD_SUBDIRECTORY;
        else
            Context->Create.DesiredAccess = FILE_ADD_FILE;

        coro_await (FuseLookupPath(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess =
            FlagOn(Context->InternalRequest->Req.Create.DesiredAccess, MAXIMUM_ALLOWED) ?
                IoGetFileObjectGenericMapping()->GenericAll :
                Context->InternalRequest->Req.Create.DesiredAccess;
        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess |=
            Context->InternalRequest->Req.Create.GrantedAccess;
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
        Context->Create.Remain = Context->Create.OrigPath;

        Context->Create.DesiredAccess = Context->InternalRequest->Req.Create.DesiredAccess |
            (FlagOn(Context->InternalRequest->Req.Create.CreateOptions, FILE_DELETE_ON_CLOSE) ?
                DELETE : 0);

        coro_await (FuseLookupPath(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess = Context->Create.GrantedAccess;
        if (!FlagOn(Context->InternalRequest->Req.Create.DesiredAccess, MAXIMUM_ALLOWED))
            Context->InternalResponse->Rsp.Create.Opened.GrantedAccess &= ~DELETE |
                (Context->InternalRequest->Req.Create.DesiredAccess & DELETE);
        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess |=
            Context->InternalRequest->Req.Create.GrantedAccess;
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
        Context->Create.Remain = Context->Create.OrigPath;

        Context->Create.DesiredAccess = Context->InternalRequest->Req.Create.DesiredAccess |
            (FILE_SUPERSEDE == ((Context->InternalRequest->Req.Create.CreateOptions >> 24) & 0xff) ?
                DELETE : FILE_WRITE_DATA) |
            (FlagOn(Context->InternalRequest->Req.Create.CreateOptions, FILE_DELETE_ON_CLOSE) ?
                DELETE : 0);

        coro_await (FuseLookupPath(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess = Context->Create.GrantedAccess;
        if (!FlagOn(Context->InternalRequest->Req.Create.DesiredAccess, MAXIMUM_ALLOWED))
            Context->InternalResponse->Rsp.Create.Opened.GrantedAccess &= ~(DELETE | FILE_WRITE_DATA) |
                (Context->InternalRequest->Req.Create.DesiredAccess & (DELETE | FILE_WRITE_DATA));
        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess |=
            Context->InternalRequest->Req.Create.GrantedAccess;
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
        FusePosixPathSuffix(&Context->Create.OrigPath, &Context->Create.Remain, 0);

        Context->Create.DesiredAccess = Context->InternalRequest->Req.Create.DesiredAccess;

        coro_await (FuseLookupPath(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess = Context->Create.GrantedAccess;
        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess |=
            Context->InternalRequest->Req.Create.GrantedAccess;
    }
}

static VOID FuseCreate(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    UINT32 Uid, Gid, Mode;

    coro_block (Context->CoroState)
    {
        Context->InternalResponse->IoStatus.Status = FuseFileCreate(Context->DeviceObject, &Context->File);
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->File->OpenFlags = 0x0100 | 0x0400 | 2 /*O_CREAT|O_EXCL|O_RDWR*/;

        Context->Create.Attr.rdev = 0;
        Context->Create.Attr.mode = 0777;
        if (0 != Context->InternalRequest->Req.Create.SecurityDescriptor.Offset)
        {
            Context->InternalResponse->IoStatus.Status = FspPosixMapSecurityDescriptorToPermissions(
                (PSECURITY_DESCRIPTOR)(Context->InternalRequest->Buffer +
                    Context->InternalRequest->Req.Create.SecurityDescriptor.Offset),
                &Uid, &Gid, &Mode);
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            Context->Create.Attr.mode = Mode;
            Context->Create.ChownOnCreate = Uid != Context->OrigUid || Gid != Context->OrigGid;
        }

        if (FlagOn(Context->InternalRequest->Req.Create.CreateOptions, FILE_DIRECTORY_FILE))
        {
            coro_await (FuseProtoSendMkdir(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            FuseCacheSetEntry(
                FuseDeviceExtension(Context->DeviceObject)->Cache,
                Context->Create.Ino, &Context->Create.Name, &Context->FuseResponse->rsp.mkdir.entry);

            Context->Create.Attr = Context->FuseResponse->rsp.mkdir.entry.attr;

            coro_await (FuseProtoSendOpendir(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            Context->Create.DisableCache =
                BooleanFlagOn(Context->FuseResponse->rsp.open.open_flags, FUSE_PROTO_OPEN_DIRECT_IO);

            Context->File->Ino = Context->Create.Ino;
            Context->File->Fh = Context->FuseResponse->rsp.open.fh;
            Context->File->IsDirectory = TRUE;
        }
        else
        {
            coro_await (FuseProtoSendCreate(Context));
            if (NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            {
                FuseCacheSetEntry(
                    FuseDeviceExtension(Context->DeviceObject)->Cache,
                    Context->Create.Ino, &Context->Create.Name, &Context->FuseResponse->rsp.create.entry);

                Context->Create.Attr = Context->FuseResponse->rsp.create.entry.attr;
                Context->Create.DisableCache =
                    BooleanFlagOn(Context->FuseResponse->rsp.create.open_flags, FUSE_PROTO_OPEN_DIRECT_IO);

                Context->File->Ino = Context->Create.Ino;
                Context->File->Fh = Context->FuseResponse->rsp.create.fh;
            }
            else
            {
                if (STATUS_INVALID_DEVICE_REQUEST != Context->InternalResponse->IoStatus.Status)
                    coro_break;

                coro_await (FuseProtoSendMknod(Context));
                if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                    coro_break;

                FuseCacheSetEntry(
                    FuseDeviceExtension(Context->DeviceObject)->Cache,
                    Context->Create.Ino, &Context->Create.Name, &Context->FuseResponse->rsp.mknod.entry);

                Context->Create.Attr = Context->FuseResponse->rsp.mknod.entry.attr;

                coro_await (FuseProtoSendOpen(Context));
                if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                    coro_break;

                Context->Create.DisableCache =
                    BooleanFlagOn(Context->FuseResponse->rsp.open.open_flags, FUSE_PROTO_OPEN_DIRECT_IO);

                Context->File->Ino = Context->Create.Ino;
                Context->File->Fh = Context->FuseResponse->rsp.open.fh;
            }
        }

        Context->InternalResponse->Rsp.Create.Opened.UserContext2 =
            (UINT64)(UINT_PTR)Context->File;
        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess =
            Context->Create.GrantedAccess;
        FuseAttrToFileInfo(Context->DeviceObject, &Context->Create.Attr,
            &Context->InternalResponse->Rsp.Create.Opened.FileInfo);
        Context->InternalResponse->Rsp.Create.Opened.DisableCache =
            Context->Create.DisableCache;

        if (Context->Create.ChownOnCreate)
        {
            Context->InternalResponse->IoStatus.Status = FspPosixMapSecurityDescriptorToPermissions(
                (PSECURITY_DESCRIPTOR)(Context->InternalRequest->Buffer +
                    Context->InternalRequest->Req.Create.SecurityDescriptor.Offset),
                &Uid, &Gid, &Mode);
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                goto cleanup;

            Context->Create.Attr.uid = Uid;
            Context->Create.Attr.gid = Gid;
            coro_await (FuseProtoSendCreateChown(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status) &&
                STATUS_INVALID_DEVICE_REQUEST != Context->InternalResponse->IoStatus.Status)
                goto cleanup;

            FuseAttrToFileInfo(Context->DeviceObject, &Context->FuseResponse->rsp.setattr.attr,
                &Context->InternalResponse->Rsp.Create.Opened.FileInfo);
        }

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
        Context->InternalResponse->IoStatus.Information = FILE_CREATED;

        /* ensure that ContextFini will not free the newly opened file */
        Context->File = 0;

        coro_break;

    cleanup:
        if (Context->File->IsDirectory)
            coro_await (FuseProtoSendReleasedir(Context));
        else
            coro_await (FuseProtoSendRelease(Context));
    }
}

static VOID FuseOpen(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    UINT32 GrantedAccess, Type;

    coro_block (Context->CoroState)
    {
        Context->InternalResponse->IoStatus.Status = FuseFileCreate(Context->DeviceObject, &Context->File);
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        GrantedAccess = Context->Create.GrantedAccess & (FILE_READ_DATA | FILE_WRITE_DATA);
        if (FILE_READ_DATA == GrantedAccess)
            Context->File->OpenFlags = 0/*O_RDONLY*/;
        else
        if (FILE_WRITE_DATA == GrantedAccess)
            Context->File->OpenFlags = 1/*O_WRONLY*/;
        else
        if ((FILE_READ_DATA | FILE_WRITE_DATA) == GrantedAccess)
            Context->File->OpenFlags = 2/*O_RDWR*/;

        Type = Context->Create.Attr.mode & 0170000;
        if (0120000/* S_IFLNK  */ == Type ||
            0010000/* S_IFIFO  */ == Type ||
            0020000/* S_IFCHR  */ == Type ||
            0060000/* S_IFBLK  */ == Type ||
            0140000/* S_IFSOCK */ == Type)
        {
            Context->Create.DisableCache = TRUE;

            Context->File->Ino = Context->Create.Ino;
            Context->File->IsReparsePoint = TRUE;
        }
        else
        if (0040000/* S_IFDIR  */ == Type)
        {
            coro_await (FuseProtoSendOpendir(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            Context->Create.DisableCache =
                BooleanFlagOn(Context->FuseResponse->rsp.open.open_flags, FUSE_PROTO_OPEN_DIRECT_IO);

            Context->File->Ino = Context->Create.Ino;
            Context->File->Fh = Context->FuseResponse->rsp.open.fh;
            Context->File->IsDirectory = TRUE;
        }
        else
        {
            coro_await (FuseProtoSendOpen(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            Context->Create.DisableCache =
                BooleanFlagOn(Context->FuseResponse->rsp.open.open_flags, FUSE_PROTO_OPEN_DIRECT_IO);

            Context->File->Ino = Context->Create.Ino;
            Context->File->Fh = Context->FuseResponse->rsp.open.fh;
        }

        Context->InternalResponse->Rsp.Create.Opened.UserContext2 =
            (UINT64)(UINT_PTR)Context->File;
        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess =
            Context->Create.GrantedAccess;
        FuseAttrToFileInfo(Context->DeviceObject, &Context->Create.Attr,
            &Context->InternalResponse->Rsp.Create.Opened.FileInfo);
        Context->InternalResponse->Rsp.Create.Opened.DisableCache =
            Context->Create.DisableCache;

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
        Context->InternalResponse->IoStatus.Information = FILE_OPENED;

        /* ensure that ContextFini will not free the newly opened file */
        Context->File = 0;
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

        FusePosixPathSuffix(&Context->Create.OrigPath, 0, &Context->Create.Name);
        coro_await (FuseCreate(Context));
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

            FusePosixPathSuffix(&Context->Create.OrigPath, 0, &Context->Create.Name);
            coro_await (FuseCreate(Context));
        }
        else
            coro_await (FuseOpen(Context));
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

            FusePosixPathSuffix(&Context->Create.OrigPath, 0, &Context->Create.Name);
            coro_await (FuseCreate(Context));
        }
        else
        {
            coro_await (FuseOpen(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            Context->InternalResponse->IoStatus.Information = FILE_OVERWRITTEN;
        }
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

        FusePosixPathSuffix(&Context->Create.OrigPath, 0, &Context->Create.Name);
        coro_await (FuseLookupName(Context));
        Context->InternalResponse->IoStatus.Information =
            NT_SUCCESS(Context->InternalResponse->IoStatus.Status) ?
                FILE_EXISTS : FILE_DOES_NOT_EXIST;
    }
}

BOOLEAN FuseOpCreate(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    UINT32 Disposition;

    coro_block (Context->CoroState)
    {
        if (Context->InternalRequest->Req.Create.NamedStream)
        {
            Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_OBJECT_NAME_INVALID;
            coro_break;
        }

        FusePrepareLookup(Context);
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Disposition = (Context->InternalRequest->Req.Create.CreateOptions >> 24) & 0xff;
        if (Context->InternalRequest->Req.Create.OpenTargetDirectory)
            coro_await (FuseOpCreate_FileOpenTargetDirectory(Context));
        else
        if (FILE_CREATE == Disposition)
            coro_await (FuseOpCreate_FileCreate(Context));
        else
        if (FILE_OPEN == Disposition)
            coro_await (FuseOpCreate_FileOpen(Context));
        else
        if (FILE_OPEN_IF == Disposition)
            coro_await (FuseOpCreate_FileOpenIf(Context));
        else
        if (FILE_OVERWRITE == Disposition)
            coro_await (FuseOpCreate_FileOverwrite(Context));
        else
        if (FILE_OVERWRITE_IF == Disposition || FILE_SUPERSEDE == Disposition)
            coro_await (FuseOpCreate_FileOverwriteIf(Context));
        else
            Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_INVALID_PARAMETER;
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

    coro_block (Context->CoroState)
    {
        if (Context->InternalRequest->Req.Cleanup.Delete)
        {
#if 1
            coro_break;
#else
            /* NOTE: CLEANUP cannot report failure! */

            FusePrepareLookup(Context);
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            coro_await (FuseLookupPath(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            if (Context->File->IsDirectory)
                coro_await (FuseProtoSendRmdir(Context));
            else
                coro_await (FuseProtoSendUnlink(Context));
#endif
        }
    }

    return coro_active();
}

BOOLEAN FuseOpClose(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        /* NOTE: CLOSE cannot report failure! */

        Context->File = (PVOID)(UINT_PTR)Context->InternalRequest->Req.Close.UserContext2;
        Context->Fini = FuseOpClose_ContextFini;

        if (Context->File->IsReparsePoint)
            /* reparse points are not opened; ignore */;
        else if (Context->File->IsDirectory)
            coro_await (FuseProtoSendReleasedir(Context));
        else
            coro_await (FuseProtoSendRelease(Context));
    }

    return coro_active();
}

static VOID FuseOpClose_ContextFini(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    if (0 != Context->File)
        FuseFileDelete(Context->DeviceObject, Context->File);
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

    coro_block (Context->CoroState)
    {
        Context->File = (PVOID)(UINT_PTR)Context->InternalRequest->Req.QueryDirectory.UserContext2;

        coro_await (FuseProtoSendFgetattr(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        FuseAttrToFileInfo(Context->DeviceObject, &Context->FuseResponse->rsp.getattr.attr,
            &Context->InternalResponse->Rsp.QueryInformation.FileInfo);

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
    }

    return coro_active();
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

    coro_block (Context->CoroState)
    {
        coro_await (FuseProtoSendStatfs(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->Rsp.QueryVolumeInformation.VolumeInfo.TotalSize =
            (UINT64)Context->FuseResponse->rsp.statfs.st.blocks *
            (UINT64)Context->FuseResponse->rsp.statfs.st.frsize;
        Context->InternalResponse->Rsp.QueryVolumeInformation.VolumeInfo.FreeSize =
            (UINT64)Context->FuseResponse->rsp.statfs.st.bfree *
            (UINT64)Context->FuseResponse->rsp.statfs.st.frsize;
    }

    return coro_active();
}

BOOLEAN FuseOpSetVolumeInformation(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

static BOOLEAN FuseAddDirInfo(FUSE_CONTEXT *Context,
    PSTRING Name, UINT64 NextOffset, FUSE_PROTO_ATTR *Attr,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    PAGED_CODE();

    try
    {
        NTSTATUS Result;
        PVOID BufferEnd = (PUINT8)Buffer + Length;
        FSP_FSCTL_DIR_INFO *DirInfo;
        ULONG WideNameLength, DirInfoSize, AlignedSize;
        WCHAR WideName[255];

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;

        if (0 != Name)
        {
            Result = RtlUTF8ToUnicodeN(
                WideName, sizeof WideName, &WideNameLength,
                Name->Buffer, Name->Length);
            if (STATUS_SOME_NOT_MAPPED == Result)
                Result = STATUS_SUCCESS;
            else if (!NT_SUCCESS(Result))
                return TRUE; /* return SUCCESS but IGNORE */
            FspPosixDecodeWindowsPath(WideName, WideNameLength / sizeof(WCHAR));

            DirInfoSize = sizeof(FSP_FSCTL_DIR_INFO) + WideNameLength;
            AlignedSize = FSP_FSCTL_DEFAULT_ALIGN_UP(DirInfoSize);

            Buffer = (PVOID)((PUINT8)Buffer + *PBytesTransferred);
            if ((PUINT8)Buffer + AlignedSize > (PUINT8)BufferEnd)
                return FALSE;

            DirInfo = Buffer;
            DirInfo->Size = (UINT16)DirInfoSize;
            FuseAttrToFileInfo(Context->DeviceObject, Attr, &DirInfo->FileInfo);
            DirInfo->NextOffset = NextOffset;
            RtlCopyMemory(DirInfo->FileNameBuf, WideName, WideNameLength);
        }
        else
        {
            DirInfoSize = sizeof(UINT16);
            AlignedSize = DirInfoSize;

            Buffer = (PVOID)((PUINT8)Buffer + *PBytesTransferred);
            if ((PUINT8)Buffer + AlignedSize > (PUINT8)BufferEnd)
                return FALSE;

            *(PUINT16)Buffer = 0;
        }

        *PBytesTransferred += AlignedSize;

        return TRUE;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        Context->InternalResponse->IoStatus.Status = GetExceptionCode();
        if (FsRtlIsNtstatusExpected(Context->InternalResponse->IoStatus.Status))
            Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_INVALID_USER_BUFFER;

        return FALSE;
    }
}

static VOID FuseOpQueryDirectory_GetDirInfoByName(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        {
            PWSTR FileName = (PWSTR)(Context->InternalRequest->Buffer +
                Context->InternalRequest->Req.QueryDirectory.Pattern.Offset);
            PSTR PosixName;
            Context->InternalResponse->IoStatus.Status = FspPosixMapWindowsToPosixPathEx(
                FileName, &PosixName, TRUE);
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;
            RtlInitString(&Context->QueryDirectory.OrigName, PosixName);
        }

        Context->QueryDirectory.Ino = Context->File->Ino;
        Context->QueryDirectory.Name = Context->QueryDirectory.OrigName;
        coro_await (FuseLookupName(Context));

        BOOLEAN AddDirInfoEnd = FALSE;
        if (NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            AddDirInfoEnd = FuseAddDirInfo(
                Context,
                &Context->QueryDirectory.Name,
                0,
                &Context->QueryDirectory.Attr,
                (PVOID)(UINT_PTR)Context->InternalRequest->Req.QueryDirectory.Address,
                Context->InternalRequest->Req.QueryDirectory.Length,
                &Context->QueryDirectory.BytesTransferred);
        else if (STATUS_OBJECT_NAME_NOT_FOUND == Context->InternalResponse->IoStatus.Status)
            AddDirInfoEnd = TRUE;

        if (AddDirInfoEnd)
            FuseAddDirInfo(
                Context,
                0,
                0,
                0,
                (PVOID)(UINT_PTR)Context->InternalRequest->Req.QueryDirectory.Address,
                Context->InternalRequest->Req.QueryDirectory.Length,
                &Context->QueryDirectory.BytesTransferred);
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
        Context->InternalResponse->IoStatus.Information = Context->QueryDirectory.BytesTransferred;
    }
}

static VOID FuseOpQueryDirectory_ReadDirectory(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        Context->QueryDirectory.NextOffset =
            sizeof(UINT64) + sizeof(WCHAR) == Context->InternalRequest->Req.QueryDirectory.Marker.Size ?
                *(PUINT64)(Context->InternalRequest->Buffer +
                    Context->InternalRequest->Req.QueryDirectory.Marker.Offset) :
                0;

        /*
         * The FSD has sent us a buffer of QueryDirectory.Length size that holds FSP_FSCTL_DIR_INFO
         * entries. Assuming that the average file name length is 24 we approximate how many entries
         * (N) we can fit in that buffer:
         *
         * N = QueryDirectory.Length / (sizeof(FSP_FSCTL_DIR_INFO) + (24 * sizeof(WCHAR)))
         *
         * We now approximate the FUSE READDIR buffer size required to fit N entries:
         *
         * read.size = FUSE_PROTO_RSP_HEADER_SIZE + N * (sizeof(FUSE_PROTO_DIRENT) + 24)
         */
        UINT32 N = Context->InternalRequest->Req.QueryDirectory.Length /
            (sizeof(FSP_FSCTL_DIR_INFO) + (24 * sizeof(WCHAR)));
        Context->QueryDirectory.Length = FUSE_PROTO_RSP_HEADER_SIZE +
            N * (sizeof(FUSE_PROTO_DIRENT) + 24);

        coro_await (FuseProtoSendReaddir(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        if (Context->QueryDirectory.Length < Context->FuseResponse->len)
        {
            Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_INTERNAL_ERROR;
            coro_break;
        }

        if (FUSE_PROTO_RSP_HEADER_SIZE < Context->FuseResponse->len)
        {
            Context->QueryDirectory.Buffer = FuseAlloc(Context->FuseResponse->len);
            if (0 == Context->QueryDirectory.Buffer)
            {
                Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_INSUFFICIENT_RESOURCES;
                coro_break;
            }

            RtlCopyMemory(Context->QueryDirectory.Buffer, Context->FuseResponse, Context->FuseResponse->len);
        }

        Context->QueryDirectory.BufferEndP = Context->QueryDirectory.Buffer + Context->FuseResponse->len;
        Context->QueryDirectory.BufferP = Context->QueryDirectory.Buffer + FUSE_PROTO_RSP_HEADER_SIZE;

        for (;;)
        {
            if (Context->QueryDirectory.BufferEndP <
                    Context->QueryDirectory.BufferP + FIELD_OFFSET(FUSE_PROTO_DIRENT, name) ||
                Context->QueryDirectory.BufferEndP <
                    Context->QueryDirectory.BufferP + FIELD_OFFSET(FUSE_PROTO_DIRENT, name) +
                        ((FUSE_PROTO_DIRENT *)Context->QueryDirectory.BufferP)->namelen)
                break;

            Context->QueryDirectory.Name.Length = Context->QueryDirectory.Name.MaximumLength = (USHORT)
                ((FUSE_PROTO_DIRENT *)Context->QueryDirectory.BufferP)->namelen;
            Context->QueryDirectory.Name.Buffer =
                ((FUSE_PROTO_DIRENT *)Context->QueryDirectory.BufferP)->name;

            if ((1 == Context->QueryDirectory.Name.Length &&
                '.' == Context->QueryDirectory.Name.Buffer[0]) ||
                (2 == Context->QueryDirectory.Name.Length &&
                '.' == Context->QueryDirectory.Name.Buffer[0] &&
                '.' == Context->QueryDirectory.Name.Buffer[1]))
            {
                /*
                 * If the file system gave us a real inode number try getattr on it.
                 * Otherwise try with the inode number from the file descriptor (this
                 * is obviously incorrect for the parent "..", but we are doing the
                 * best we can).
                 */
                Context->QueryDirectory.Ino =
                    FUSE_PROTO_UNKNOWN_INO != ((FUSE_PROTO_DIRENT *)Context->QueryDirectory.BufferP)->ino ?
                        ((FUSE_PROTO_DIRENT *)Context->QueryDirectory.BufferP)->ino :
                        Context->File->Ino;
                coro_await (FuseProtoSendGetattr(Context));
                if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                    coro_break;
                Context->Lookup.Attr = Context->FuseResponse->rsp.getattr.attr;
            }
            else
            {
                Context->QueryDirectory.Ino = Context->File->Ino;
                coro_await (FuseLookupName(Context));
                if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                    coro_break;
            }

            BOOLEAN Added = FuseAddDirInfo(
                Context,
                &Context->QueryDirectory.Name,
                ((FUSE_PROTO_DIRENT *)Context->QueryDirectory.BufferP)->off,
                &Context->QueryDirectory.Attr,
                (PVOID)(UINT_PTR)Context->InternalRequest->Req.QueryDirectory.Address,
                Context->InternalRequest->Req.QueryDirectory.Length,
                &Context->QueryDirectory.BytesTransferred);
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;
            if (!Added)
                break;

            Context->QueryDirectory.BufferP += FSP_FSCTL_ALIGN_UP(
                FIELD_OFFSET(FUSE_PROTO_DIRENT, name) +
                    ((FUSE_PROTO_DIRENT *)Context->QueryDirectory.BufferP)->namelen,
                8);
        }

        /* empty readdir response signifies end of dir; add WinFsp end-of-dir marker */
        if (Context->QueryDirectory.BufferP == Context->QueryDirectory.Buffer + FUSE_PROTO_RSP_HEADER_SIZE)
            FuseAddDirInfo(Context, 0, 0, 0,
                (PVOID)(UINT_PTR)Context->InternalRequest->Req.QueryDirectory.Address,
                Context->InternalRequest->Req.QueryDirectory.Length,
                &Context->QueryDirectory.BytesTransferred);

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
        Context->InternalResponse->IoStatus.Information = Context->QueryDirectory.BytesTransferred;
    }
}

BOOLEAN FuseOpQueryDirectory(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        Context->Fini = FuseOpQueryDirectory_ContextFini;
        Context->File = (PVOID)(UINT_PTR)Context->InternalRequest->Req.QueryDirectory.UserContext2;

        Context->InternalResponse->IoStatus.Status = FuseCacheReferenceGen(
            FuseDeviceExtension(Context->DeviceObject)->Cache, &Context->QueryDirectory.CacheGen);
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        if (0 != Context->InternalRequest->Req.QueryDirectory.Pattern.Size &&
            Context->InternalRequest->Req.QueryDirectory.PatternIsFileName)
            coro_await (FuseOpQueryDirectory_GetDirInfoByName(Context));
        else
            coro_await (FuseOpQueryDirectory_ReadDirectory(Context));
    }

    return coro_active();
}

static VOID FuseOpQueryDirectory_ContextFini(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    if (0 != Context->QueryDirectory.Buffer)
        FuseFree(Context->QueryDirectory.Buffer);

    FspPosixDeletePath(Context->QueryDirectory.OrigName.Buffer);
        /* handles NULL paths */
    FuseCacheDereferenceGen(FuseDeviceExtension(Context->DeviceObject)->Cache, Context->QueryDirectory.CacheGen);
        /* handles NULL gens */
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
