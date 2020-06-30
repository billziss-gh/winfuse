/**
 * @file shared/km/fuseop.c
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

static BOOLEAN FuseOpReserved_Init(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpReserved_Destroy(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpReserved_Forget(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpReserved(FUSE_CONTEXT *Context);
static VOID FuseLookup(FUSE_CONTEXT *Context);
static NTSTATUS FuseAccessCheck(
    UINT32 FileUid, UINT32 FileGid, UINT32 FileMode,
    UINT32 OrigUid, UINT32 OrigGid, UINT32 DesiredAccess,
    PUINT32 PGrantedAccess);
static VOID FusePrepareLookupPath(FUSE_CONTEXT *Context);
static VOID FusePrepareLookupPath2(FUSE_CONTEXT *Context);
static VOID FusePrepareLookupPath_ContextFini(FUSE_CONTEXT *Context);
static VOID FuseLookupPath(FUSE_CONTEXT *Context);
static VOID FuseCreateCheck(FUSE_CONTEXT *Context);
static VOID FuseOpenCheck(FUSE_CONTEXT *Context);
static VOID FuseOverwriteCheck(FUSE_CONTEXT *Context);
static VOID FuseOpenTargetDirectoryCheck(FUSE_CONTEXT *Context);
static VOID FuseRenameCheck(FUSE_CONTEXT *Context);
static VOID FuseCreate(FUSE_CONTEXT *Context);
static VOID FuseOpen(FUSE_CONTEXT *Context);
static VOID FuseOpCreate_FileCreate(FUSE_CONTEXT *Context);
static VOID FuseOpCreate_FileOpen(FUSE_CONTEXT *Context);
static VOID FuseOpCreate_FileOpenIf(FUSE_CONTEXT *Context);
static VOID FuseOpCreate_FileOverwrite(FUSE_CONTEXT *Context);
static VOID FuseOpCreate_FileOverwriteIf(FUSE_CONTEXT *Context);
static VOID FuseOpCreate_FileOpenTargetDirectory(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpCreate(FUSE_CONTEXT *Context);
static INT FuseOgCreate(FUSE_CONTEXT *Context, BOOLEAN Acquire);
static BOOLEAN FuseOpOverwrite(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpCleanup(FUSE_CONTEXT *Context);
static INT FuseOgCleanup(FUSE_CONTEXT *Context, BOOLEAN Acquire);
static BOOLEAN FuseOpClose(FUSE_CONTEXT *Context);
static VOID FuseOpClose_ContextFini(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpRead(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpWrite(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpQueryInformation(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpSetInformation_SetBasicInfo(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpSetInformation_SetAllocationSize(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpSetInformation_SetFileSize(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpSetInformation_SetDelete(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpSetInformation_Rename(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpSetInformation(FUSE_CONTEXT *Context);
static INT FuseOgSetInformation(FUSE_CONTEXT *Context, BOOLEAN Acquire);
static BOOLEAN FuseOpQueryEa(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpSetEa(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpFlushBuffers(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpQueryVolumeInformation(FUSE_CONTEXT *Context);
static BOOLEAN FuseAddDirInfo(FUSE_CONTEXT *Context,
    PSTRING Name, UINT64 NextOffset, FUSE_PROTO_ATTR *Attr,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred);
static VOID FuseOpQueryDirectory_GetDirInfoByName(FUSE_CONTEXT *Context);
static VOID FuseOpQueryDirectory_ReadDirectory(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpQueryDirectory(FUSE_CONTEXT *Context);
static VOID FuseOpQueryDirectory_ContextFini(FUSE_CONTEXT *Context);
static INT FuseOgQueryDirectory(FUSE_CONTEXT *Context, BOOLEAN Acquire);
static BOOLEAN FuseOpFileSystemControl(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpDeviceControl(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpQuerySecurity(FUSE_CONTEXT *Context);
static BOOLEAN FuseOpSetSecurity(FUSE_CONTEXT *Context);
static VOID FuseSecurity_ContextFini(FUSE_CONTEXT *Context);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FuseOpReserved_Init)
#pragma alloc_text(PAGE, FuseOpReserved_Destroy)
#pragma alloc_text(PAGE, FuseOpReserved_Forget)
#pragma alloc_text(PAGE, FuseOpReserved)
#pragma alloc_text(PAGE, FuseLookup)
#pragma alloc_text(PAGE, FuseAccessCheck)
#pragma alloc_text(PAGE, FusePrepareLookupPath)
#pragma alloc_text(PAGE, FusePrepareLookupPath2)
#pragma alloc_text(PAGE, FusePrepareLookupPath_ContextFini)
#pragma alloc_text(PAGE, FuseLookupPath)
#pragma alloc_text(PAGE, FuseCreateCheck)
#pragma alloc_text(PAGE, FuseOpenCheck)
#pragma alloc_text(PAGE, FuseOverwriteCheck)
#pragma alloc_text(PAGE, FuseOpenTargetDirectoryCheck)
#pragma alloc_text(PAGE, FuseRenameCheck)
#pragma alloc_text(PAGE, FuseCreate)
#pragma alloc_text(PAGE, FuseOpen)
#pragma alloc_text(PAGE, FuseOpCreate_FileCreate)
#pragma alloc_text(PAGE, FuseOpCreate_FileOpen)
#pragma alloc_text(PAGE, FuseOpCreate_FileOpenIf)
#pragma alloc_text(PAGE, FuseOpCreate_FileOverwrite)
#pragma alloc_text(PAGE, FuseOpCreate_FileOverwriteIf)
#pragma alloc_text(PAGE, FuseOpCreate_FileOpenTargetDirectory)
#pragma alloc_text(PAGE, FuseOpCreate)
#pragma alloc_text(PAGE, FuseOgCreate)
#pragma alloc_text(PAGE, FuseOpOverwrite)
#pragma alloc_text(PAGE, FuseOpCleanup)
#pragma alloc_text(PAGE, FuseOgCleanup)
#pragma alloc_text(PAGE, FuseOpClose)
#pragma alloc_text(PAGE, FuseOpClose_ContextFini)
#pragma alloc_text(PAGE, FuseOpRead)
#pragma alloc_text(PAGE, FuseOpWrite)
#pragma alloc_text(PAGE, FuseOpQueryInformation)
#pragma alloc_text(PAGE, FuseOpSetInformation_SetBasicInfo)
#pragma alloc_text(PAGE, FuseOpSetInformation_SetAllocationSize)
#pragma alloc_text(PAGE, FuseOpSetInformation_SetFileSize)
#pragma alloc_text(PAGE, FuseOpSetInformation_SetDelete)
#pragma alloc_text(PAGE, FuseOpSetInformation_Rename)
#pragma alloc_text(PAGE, FuseOpSetInformation)
#pragma alloc_text(PAGE, FuseOgSetInformation)
#pragma alloc_text(PAGE, FuseOpQueryEa)
#pragma alloc_text(PAGE, FuseOpSetEa)
#pragma alloc_text(PAGE, FuseOpFlushBuffers)
#pragma alloc_text(PAGE, FuseOpQueryVolumeInformation)
#pragma alloc_text(PAGE, FuseAddDirInfo)
#pragma alloc_text(PAGE, FuseOpQueryDirectory_GetDirInfoByName)
#pragma alloc_text(PAGE, FuseOpQueryDirectory_ReadDirectory)
#pragma alloc_text(PAGE, FuseOpQueryDirectory)
#pragma alloc_text(PAGE, FuseOpQueryDirectory_ContextFini)
#pragma alloc_text(PAGE, FuseOgQueryDirectory)
#pragma alloc_text(PAGE, FuseOpFileSystemControl)
#pragma alloc_text(PAGE, FuseOpDeviceControl)
#pragma alloc_text(PAGE, FuseOpQuerySecurity)
#pragma alloc_text(PAGE, FuseOpSetSecurity)
#pragma alloc_text(PAGE, FuseSecurity_ContextFini)
#endif

static BOOLEAN FuseOpReserved_Init(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        coro_await (FuseProtoSendInit(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        FUSE_INSTANCE *Instance = Context->Instance;
        if (FUSE_PROTO_VERSION != Context->FuseResponse->rsp.init.major)
        {
            Instance->VersionMajor = (UINT32)-1;
            KeSetEvent(&Instance->InitEvent, 1, FALSE);

            Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_CONNECTION_REFUSED;
            coro_break;
        }

        Instance->VersionMajor = Context->FuseResponse->rsp.init.major;
        Instance->VersionMinor = Context->FuseResponse->rsp.init.minor;
        // !!!: REVISIT
        KeSetEvent(&Instance->InitEvent, 1, FALSE);

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
    }

    return coro_active();
}

static BOOLEAN FuseOpReserved_Destroy(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        coro_await (FuseProtoSendDestroy(Context));

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
    }

    return coro_active();
}

static BOOLEAN FuseOpReserved_Forget(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    FUSE_INSTANCE *Instance = Context->Instance;

    ASSERT(!IsListEmpty(&Context->Forget.ForgetList));
    if (16 > Instance->VersionMinor ||
        &Context->Forget.ForgetList == Context->Forget.ForgetList.Flink->Flink ||
        DEBUGTEST(10))
        FuseProtoFillForget(Context);
    else
        FuseProtoFillBatchForget(Context);

    return FALSE;
}

static BOOLEAN FuseOpReserved(FUSE_CONTEXT *Context)
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

static VOID FuseLookup(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    FUSE_PROTO_ENTRY EntryBuf, *Entry = &EntryBuf;
    PVOID CacheItem;

    coro_block (Context->CoroState)
    {
        if (!FuseCacheGetEntry(Context->Instance->Cache,
            Context->Lookup.Ino, &Context->Lookup.Name, Entry, &CacheItem))
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
                Context->Instance->Cache,
                Context->Lookup.Ino, &Context->Lookup.Name, Entry, &CacheItem);
        }

        Context->Lookup.CacheItem = CacheItem;
        Context->Lookup.Ino = Entry->nodeid;
        Context->Lookup.Attr = Entry->attr;

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
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

static VOID FusePrepareLookupPath(FUSE_CONTEXT *Context)
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
            Context->Instance->Cache, &CacheGen);
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

    ASSERT(0 == Context->LookupPath.OrigPath.Buffer);
    RtlInitString(&Context->LookupPath.OrigPath, PosixPath);
    Context->LookupPath.CacheGen = CacheGen;
    Context->LookupPath.UserMode = IsUserMode;
    Context->LookupPath.HasTraversePrivilege = HasTraversePrivilege;

    Context->Fini = FusePrepareLookupPath_ContextFini;

    Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
    {
        FspPosixDeletePath(PosixPath);
            /* handles NULL paths */
        FuseCacheDereferenceGen(Context->Instance->Cache, CacheGen);
            /* handles NULL gens */
    }
}

static VOID FusePrepareLookupPath2(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    PSTR PosixPath = 0;
    PWSTR FileName = 0;

    switch (Context->InternalRequest->Kind)
    {
    case FspFsctlTransactSetInformationKind:
        ASSERT(FileRenameInformation ==
            Context->InternalRequest->Req.SetInformation.FileInformationClass);
        FileName = (PWSTR)Context->InternalRequest->Buffer;
        break;
    default:
        ASSERT(FALSE);
        Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_INVALID_PARAMETER;
        goto exit;
    }

    if (0 != FileName)
    {
        Context->InternalResponse->IoStatus.Status = FspPosixMapWindowsToPosixPathEx(
            FileName, &PosixPath, TRUE);
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            goto exit;
    }

    ASSERT(0 == Context->LookupPath.OrigPath.Buffer);
    ASSERT(0 != Context->LookupPath.OrigPath2.Buffer);
    RtlInitString(&Context->LookupPath.OrigPath, PosixPath);

    Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
    {
        FspPosixDeletePath(PosixPath);
            /* handles NULL paths */
    }
}

static VOID FusePrepareLookupPath_ContextFini(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    if (FspFsctlTransactCreateKind == Context->InternalRequest->Kind &&
        0 != Context->File)
        FuseFileDelete(Context->Instance, Context->File);

    FspPosixDeletePath(Context->LookupPath.OrigPath2.Buffer);
        /* handles NULL paths */
    FspPosixDeletePath(Context->LookupPath.OrigPath.Buffer);
        /* handles NULL paths */
    FuseCacheDereferenceGen(Context->Instance->Cache, Context->LookupPath.CacheGen);
        /* handles NULL gens */
}

static VOID FuseLookupPath(FUSE_CONTEXT *Context)
{
#define RootName                        (1 == Context->LookupPath.Name.Length && '/' == Context->LookupPath.Name.Buffer[0])
#define LastName                        (0 == Context->LookupPath.Remain.Length)
#define UserMode                        (Context->LookupPath.UserMode)
#define TravPriv                        (Context->LookupPath.HasTraversePrivilege)

    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        Context->LookupPath.Ino = FUSE_PROTO_ROOT_INO;
        DEBUGFILL(&Context->Lookup.Attr, sizeof Context->Lookup.Attr);
        while (1) /* for (;;) produces "warning C4702: unreachable code" */
        {
            FusePosixPathPrefix(&Context->LookupPath.Remain, &Context->LookupPath.Name, &Context->LookupPath.Remain);

            /*
             * - RootName:
             *     - UserMode:
             *         - !LastName && !TravPriv:
             *             - Lookup
             *             - TraverseCheck
             *         - LastName:
             *             - Lookup
             *             - AccessCheck
             *     - !UserMode:
             *         - LastName:
             *             - Lookup
             * - !RootName:
             *     - Lookup
             *     - UserMode:
             *         - !LastName && !TravPriv:
             *             - TraverseCheck
             *         - LastName:
             *             - AccessCheck
             */
            if (!RootName || LastName || (UserMode && !TravPriv))
            {
                coro_await (FuseLookup(Context));
                if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                    coro_break;

                if (UserMode)
                {
                    if (!LastName && !TravPriv)
                    {
                        Context->InternalResponse->IoStatus.Status = FuseAccessCheck(
                            Context->LookupPath.Attr.uid, Context->LookupPath.Attr.gid,
                            Context->LookupPath.Attr.mode,
                            Context->OrigUid, Context->OrigGid,
                            FILE_TRAVERSE, 0);
                        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                            coro_break;
                    }
                    else if (LastName)
                    {
                        Context->InternalResponse->IoStatus.Status = FuseAccessCheck(
                            Context->LookupPath.Attr.uid, Context->LookupPath.Attr.gid,
                            Context->LookupPath.Attr.mode,
                            Context->OrigUid, Context->OrigGid,
                            Context->LookupPath.DesiredAccess, &Context->LookupPath.GrantedAccess);
                        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                            coro_break;
                    }
                }
            }

            if (LastName)
            {
                ASSERT(DEBUGGOOD(&Context->Lookup.Attr, sizeof Context->Lookup.Attr));
                break;
            }
        }

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
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

        if (Context->InternalRequest->Req.Create.HasRestorePrivilege)
            Context->LookupPath.DesiredAccess = 0;
        else if (FlagOn(Context->InternalRequest->Req.Create.CreateOptions, FILE_DIRECTORY_FILE))
            Context->LookupPath.DesiredAccess = FILE_ADD_SUBDIRECTORY;
        else
            Context->LookupPath.DesiredAccess = FILE_ADD_FILE;

        FusePosixPathSuffix(&Context->LookupPath.OrigPath, &Context->LookupPath.Remain, 0);
        coro_await (FuseLookupPath(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess =
            FlagOn(Context->InternalRequest->Req.Create.DesiredAccess, MAXIMUM_ALLOWED) ?
                IoGetFileObjectGenericMapping()->GenericAll :
                Context->InternalRequest->Req.Create.DesiredAccess;
        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess |=
            Context->InternalRequest->Req.Create.GrantedAccess;

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
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
        Context->LookupPath.DesiredAccess = Context->InternalRequest->Req.Create.DesiredAccess |
            (FlagOn(Context->InternalRequest->Req.Create.CreateOptions, FILE_DELETE_ON_CLOSE) ?
                DELETE : 0);

        Context->LookupPath.Remain = Context->LookupPath.OrigPath;
        coro_await (FuseLookupPath(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess = Context->LookupPath.GrantedAccess;
        if (!FlagOn(Context->InternalRequest->Req.Create.DesiredAccess, MAXIMUM_ALLOWED))
            Context->InternalResponse->Rsp.Create.Opened.GrantedAccess &= ~DELETE |
                (Context->InternalRequest->Req.Create.DesiredAccess & DELETE);
        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess |=
            Context->InternalRequest->Req.Create.GrantedAccess;

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
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
        Context->LookupPath.DesiredAccess = Context->InternalRequest->Req.Create.DesiredAccess |
            (FILE_SUPERSEDE == ((Context->InternalRequest->Req.Create.CreateOptions >> 24) & 0xff) ?
                DELETE : FILE_WRITE_DATA) |
            (FlagOn(Context->InternalRequest->Req.Create.CreateOptions, FILE_DELETE_ON_CLOSE) ?
                DELETE : 0);

        Context->LookupPath.Remain = Context->LookupPath.OrigPath;
        coro_await (FuseLookupPath(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess = Context->LookupPath.GrantedAccess;
        if (!FlagOn(Context->InternalRequest->Req.Create.DesiredAccess, MAXIMUM_ALLOWED))
            Context->InternalResponse->Rsp.Create.Opened.GrantedAccess &= ~(DELETE | FILE_WRITE_DATA) |
                (Context->InternalRequest->Req.Create.DesiredAccess & (DELETE | FILE_WRITE_DATA));
        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess |=
            Context->InternalRequest->Req.Create.GrantedAccess;

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
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
        Context->LookupPath.DesiredAccess = Context->InternalRequest->Req.Create.DesiredAccess;

        FusePosixPathSuffix(&Context->LookupPath.OrigPath, &Context->LookupPath.Remain, 0);
        coro_await (FuseLookupPath(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess = Context->LookupPath.GrantedAccess;
        Context->InternalResponse->Rsp.Create.Opened.GrantedAccess |=
            Context->InternalRequest->Req.Create.GrantedAccess;

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
    }
}

static VOID FuseRenameCheck(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    /*
     * RenameCheck consists of checking the new file name for DELETE access.
     *
     * The following assumptions are being made here for a file that is going
     * to be replaced:
     * -   The new file is in the same directory as the old one. In that case
     *     there is no need for traverse access checks as they have been already
     *     performed (if necessary) when opening the file under the existing file
     *     name.
     * -   The new file is in a different directory than the old one. In that case
     *     NTOS called us with SL_OPEN_TARGET_DIRECTORY and we performed any
     *     necessary traverse access checks at that time.
     */

    coro_block (Context->CoroState)
    {
        Context->LookupPath.DesiredAccess = 0;

        FusePosixPathSuffix(&Context->LookupPath.OrigPath, &Context->LookupPath.Remain, 0);
        coro_await (FuseLookupPath(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->LookupPath.Ino2 = Context->LookupPath.Ino;

        FusePosixPathSuffix(&Context->LookupPath.OrigPath, 0, &Context->LookupPath.Name);
        coro_await (FuseLookup(Context));
        if (NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
        {
            if (0 != Context->InternalRequest->Req.SetInformation.Info.Rename.AccessToken)
            {
                Context->InternalResponse->IoStatus.Status = FuseAccessCheck(
                    Context->LookupPath.Attr.uid, Context->LookupPath.Attr.gid,
                    Context->LookupPath.Attr.mode,
                    Context->OrigUid, Context->OrigGid,
                    DELETE, &Context->LookupPath.GrantedAccess);
                if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                    coro_break;
            }

            Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
        }

        Context->LookupPath.OrigPath2 = Context->LookupPath.OrigPath;
        Context->LookupPath.Name2 = Context->LookupPath.Name;

        Context->LookupPath.Ino = 0;
        Context->LookupPath.OrigPath.Length = Context->LookupPath.OrigPath.MaximumLength = 0;
        Context->LookupPath.OrigPath.Buffer = 0;
        Context->LookupPath.Name.Length = Context->LookupPath.Name.MaximumLength = 0;
        Context->LookupPath.Name.Buffer = 0;
    }
}

static VOID FuseCreate(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        Context->InternalResponse->IoStatus.Status = FuseFileCreate(Context->Instance, &Context->File);
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->File->OpenFlags = 0x0100 | 0x0400 | 2 /*O_CREAT|O_EXCL|O_RDWR*/;

        Context->LookupPath.Attr.rdev = 0;
        Context->LookupPath.Attr.mode = 0777;
        if (0 != Context->InternalRequest->Req.Create.SecurityDescriptor.Offset)
        {
            UINT32 Uid, Gid, Mode;

            Uid = Context->OrigUid;
            Gid = Context->OrigGid;
            Context->InternalResponse->IoStatus.Status = FspPosixMapSecurityDescriptorToPermissions(
                (PSECURITY_DESCRIPTOR)(Context->InternalRequest->Buffer +
                    Context->InternalRequest->Req.Create.SecurityDescriptor.Offset),
                (PVOID)((UINT_PTR)&Uid | 1), (PVOID)((UINT_PTR)&Gid | 1), &Mode);
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            Context->LookupPath.Attr.mode = Mode;
            Context->LookupPath.Chown = Uid != Context->OrigUid || Gid != Context->OrigGid;
        }

        if (FlagOn(Context->InternalRequest->Req.Create.CreateOptions, FILE_DIRECTORY_FILE))
        {
            coro_await (FuseProtoSendMkdir(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            FuseCacheSetEntry(
                Context->Instance->Cache,
                Context->LookupPath.Ino, &Context->LookupPath.Name,
                &Context->FuseResponse->rsp.mkdir.entry,
                &Context->LookupPath.CacheItem);

            Context->LookupPath.Ino = Context->FuseResponse->rsp.mkdir.entry.nodeid;
            Context->LookupPath.Attr = Context->FuseResponse->rsp.mkdir.entry.attr;

            coro_await (FuseProtoSendOpendir(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            Context->LookupPath.DisableCache =
                BooleanFlagOn(Context->FuseResponse->rsp.open.open_flags, FUSE_PROTO_OPEN_DIRECT_IO);

            Context->File->Ino = Context->LookupPath.Ino;
            Context->File->Fh = Context->FuseResponse->rsp.open.fh;
            Context->File->IsDirectory = TRUE;
            Context->File->CacheItem = Context->LookupPath.CacheItem;
            FuseCacheReferenceItem(Context->Instance->Cache,
                Context->File->CacheItem);
        }
        else
        {
            coro_await (FuseProtoSendCreate(Context));
            if (NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            {
                FuseCacheSetEntry(
                    Context->Instance->Cache,
                    Context->LookupPath.Ino, &Context->LookupPath.Name,
                    &Context->FuseResponse->rsp.create.entry,
                    &Context->LookupPath.CacheItem);

                Context->LookupPath.Ino = Context->FuseResponse->rsp.create.entry.nodeid;
                Context->LookupPath.Attr = Context->FuseResponse->rsp.create.entry.attr;
                Context->LookupPath.DisableCache =
                    BooleanFlagOn(Context->FuseResponse->rsp.create.open_flags, FUSE_PROTO_OPEN_DIRECT_IO);

                Context->File->Ino = Context->LookupPath.Ino;
                Context->File->Fh = Context->FuseResponse->rsp.create.fh;
                Context->File->CacheItem = Context->LookupPath.CacheItem;
                FuseCacheReferenceItem(Context->Instance->Cache,
                    Context->File->CacheItem);
            }
            else
            {
                if (STATUS_INVALID_DEVICE_REQUEST != Context->InternalResponse->IoStatus.Status)
                    coro_break;

                coro_await (FuseProtoSendMknod(Context));
                if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                    coro_break;

                FuseCacheSetEntry(
                    Context->Instance->Cache,
                    Context->LookupPath.Ino, &Context->LookupPath.Name,
                    &Context->FuseResponse->rsp.mknod.entry,
                    &Context->LookupPath.CacheItem);

                Context->LookupPath.Ino = Context->FuseResponse->rsp.mknod.entry.nodeid;
                Context->LookupPath.Attr = Context->FuseResponse->rsp.mknod.entry.attr;

                coro_await (FuseProtoSendOpen(Context));
                if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                    coro_break;

                Context->LookupPath.DisableCache =
                    BooleanFlagOn(Context->FuseResponse->rsp.open.open_flags, FUSE_PROTO_OPEN_DIRECT_IO);

                Context->File->Ino = Context->LookupPath.Ino;
                Context->File->Fh = Context->FuseResponse->rsp.open.fh;
                Context->File->CacheItem = Context->LookupPath.CacheItem;
                FuseCacheReferenceItem(Context->Instance->Cache,
                    Context->File->CacheItem);
            }
        }

        Context->InternalResponse->Rsp.Create.Opened.UserContext2 =
            (UINT64)(UINT_PTR)Context->File;
        FuseAttrToFileInfo(Context->Instance, &Context->LookupPath.Attr,
            &Context->InternalResponse->Rsp.Create.Opened.FileInfo);
        Context->InternalResponse->Rsp.Create.Opened.DisableCache =
            Context->LookupPath.DisableCache;

        if (Context->LookupPath.Chown)
        {
            UINT32 Uid, Gid, Mode;

            Uid = Context->OrigUid;
            Gid = Context->OrigGid;
            Context->InternalResponse->IoStatus.Status = FspPosixMapSecurityDescriptorToPermissions(
                (PSECURITY_DESCRIPTOR)(Context->InternalRequest->Buffer +
                    Context->InternalRequest->Req.Create.SecurityDescriptor.Offset),
                (PVOID)((UINT_PTR)&Uid | 1), (PVOID)((UINT_PTR)&Gid | 1), &Mode);
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                goto cleanup;

            Context->LookupPath.Attr.uid = Uid;
            Context->LookupPath.Attr.gid = Gid;
            coro_await (FuseProtoSendLookupChown(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status) &&
                STATUS_INVALID_DEVICE_REQUEST != Context->InternalResponse->IoStatus.Status)
                goto cleanup;

            FuseAttrToFileInfo(Context->Instance, &Context->FuseResponse->rsp.setattr.attr,
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

    coro_block (Context->CoroState)
    {
        Context->InternalResponse->IoStatus.Status = FuseFileCreate(Context->Instance, &Context->File);
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        UINT32 GrantedAccess = Context->InternalResponse->Rsp.Create.Opened.GrantedAccess;
        switch (GrantedAccess & (FILE_READ_DATA | FILE_WRITE_DATA))
        {
        default:
        case FILE_READ_DATA:
            Context->File->OpenFlags = 0/*O_RDONLY*/;
            break;
        case FILE_WRITE_DATA:
            Context->File->OpenFlags = 1/*O_WRONLY*/;
            break;
        case FILE_READ_DATA | FILE_WRITE_DATA:
            Context->File->OpenFlags = 2/*O_RDWR*/;
            break;
        }

        UINT32 Type = Context->LookupPath.Attr.mode & 0170000;
        if (0120000/* S_IFLNK  */ == Type ||
            0010000/* S_IFIFO  */ == Type ||
            0020000/* S_IFCHR  */ == Type ||
            0060000/* S_IFBLK  */ == Type ||
            0140000/* S_IFSOCK */ == Type)
        {
            Context->LookupPath.DisableCache = TRUE;

            Context->File->Ino = Context->LookupPath.Ino;
            Context->File->IsReparsePoint = TRUE;
        }
        else
        if (0040000/* S_IFDIR  */ == Type)
        {
            coro_await (FuseProtoSendOpendir(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            Context->LookupPath.DisableCache =
                BooleanFlagOn(Context->FuseResponse->rsp.open.open_flags, FUSE_PROTO_OPEN_DIRECT_IO);

            Context->File->Ino = Context->LookupPath.Ino;
            Context->File->Fh = Context->FuseResponse->rsp.open.fh;
            Context->File->IsDirectory = TRUE;
        }
        else
        {
            /*
             * Some Windows applications specify FILE_APPEND_DATA without
             * FILE_WRITE_DATA when opening files for appending.
             *
             * NOTE: GrantedAccess has been initialized above and there is no coro_yield or
             * coro_await between that initialization and this use.
             */
            if (GrantedAccess & FILE_APPEND_DATA)
            {
                if (Context->File->OpenFlags == 0)
                    Context->File->OpenFlags = 1/* O_WRONLY */;
                Context->File->OpenFlags |= 8/*O_APPEND*/;
            }

            coro_await (FuseProtoSendOpen(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            Context->LookupPath.DisableCache =
                BooleanFlagOn(Context->FuseResponse->rsp.open.open_flags, FUSE_PROTO_OPEN_DIRECT_IO);

            Context->File->Ino = Context->LookupPath.Ino;
            Context->File->Fh = Context->FuseResponse->rsp.open.fh;
        }

        Context->File->CacheItem = Context->LookupPath.CacheItem;
        FuseCacheReferenceItem(Context->Instance->Cache,
            Context->File->CacheItem);

        Context->InternalResponse->Rsp.Create.Opened.UserContext2 =
            (UINT64)(UINT_PTR)Context->File;
        FuseAttrToFileInfo(Context->Instance, &Context->LookupPath.Attr,
            &Context->InternalResponse->Rsp.Create.Opened.FileInfo);
        Context->InternalResponse->Rsp.Create.Opened.DisableCache =
            Context->LookupPath.DisableCache;

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

        FusePosixPathSuffix(&Context->LookupPath.OrigPath, 0, &Context->LookupPath.Name);
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

            FusePosixPathSuffix(&Context->LookupPath.OrigPath, 0, &Context->LookupPath.Name);
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

            FusePosixPathSuffix(&Context->LookupPath.OrigPath, 0, &Context->LookupPath.Name);
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

        FusePosixPathSuffix(&Context->LookupPath.OrigPath, 0, &Context->LookupPath.Name);
        coro_await (FuseLookup(Context));
        Context->InternalResponse->IoStatus.Information =
            NT_SUCCESS(Context->InternalResponse->IoStatus.Status) ?
                FILE_EXISTS : FILE_DOES_NOT_EXIST;
        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
    }
}

static BOOLEAN FuseOpCreate(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        if (Context->InternalRequest->Req.Create.NamedStream)
        {
            Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_OBJECT_NAME_INVALID;
            coro_break;
        }

        FusePrepareLookupPath(Context);
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        UINT32 Disposition = (Context->InternalRequest->Req.Create.CreateOptions >> 24) & 0xff;
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

static INT FuseOgCreate(FUSE_CONTEXT *Context, BOOLEAN Acquire)
{
    PAGED_CODE();

    if (Acquire)
    {
        INT Result;
        if (FILE_OPEN != ((Context->InternalRequest->Req.Create.CreateOptions >> 24) & 0xff))
            Result = FuseOpGuardAcquireExclusive(Context);
        else
            Result = FuseOpGuardAcquireShared(Context);
#if DBG
        /*
         * In debug builds we add an artificial delay to our opens to test alertable locks.
         */
        if (Result)
        {
            UNICODE_STRING LockDly, FileName;
            RtlInitUnicodeString(&LockDly, L"\\$LOCKDLY");
            FileName.Length = FileName.MaximumLength =
                Context->InternalRequest->FileName.Size - sizeof(WCHAR);
            FileName.Buffer = (PVOID)(
                Context->InternalRequest->Buffer + Context->InternalRequest->FileName.Offset);
            if (RtlEqualUnicodeString(&LockDly, &FileName, FALSE))
            {
                LARGE_INTEGER Delay;
                Delay.QuadPart = 5000/*ms*/ * -10000LL;
                KeDelayExecutionThread(KernelMode, FALSE, &Delay);
            }
        }
#endif
        return Result;
    }
    else
    {
        if (FILE_OPEN != ((Context->InternalRequest->Req.Create.CreateOptions >> 24) & 0xff))
            return FuseOpGuardReleaseExclusive(Context);
        else
            return FuseOpGuardReleaseShared(Context);
    }
}

static BOOLEAN FuseOpOverwrite(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        Context->File = (PVOID)(UINT_PTR)Context->InternalRequest->Req.Overwrite.UserContext2;

        //Context->Setattr.Attr.size = 0;
        coro_await (FuseProtoSendFtruncate(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        coro_await (FuseProtoSendFgetattr(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        FuseCacheQuickExpireItem(Context->Instance->Cache,
            Context->File->CacheItem);

        FuseAttrToFileInfo(Context->Instance, &Context->FuseResponse->rsp.getattr.attr,
            &Context->InternalResponse->Rsp.Overwrite.FileInfo);

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
    }

    return coro_active();
}

static BOOLEAN FuseOpCleanup(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        if (Context->InternalRequest->Req.Cleanup.Delete)
        {
            /* NOTE: CLEANUP cannot report failure! */

            Context->File = (PVOID)(UINT_PTR)Context->InternalRequest->Req.Cleanup.UserContext2;

            FusePrepareLookupPath(Context);
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            FusePosixPathSuffix(&Context->LookupPath.OrigPath, &Context->LookupPath.Remain, 0);
            coro_await (FuseLookupPath(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            FusePosixPathSuffix(&Context->LookupPath.OrigPath, 0, &Context->LookupPath.Name);
            if (Context->File->IsDirectory)
                coro_await (FuseProtoSendRmdir(Context));
            else
                coro_await (FuseProtoSendUnlink(Context));

            FuseCacheRemoveEntry(
                Context->Instance->Cache,
                Context->Lookup.Ino, &Context->Lookup.Name);

            Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
        }
    }

    return coro_active();
}

static INT FuseOgCleanup(FUSE_CONTEXT *Context, BOOLEAN Acquire)
{
    PAGED_CODE();

    if (Acquire)
    {
        if (Context->InternalRequest->Req.Cleanup.Delete)
            return FuseOpGuardAcquireExclusive(Context);
        else
            return FuseOpGuardFalse;
    }
    else
    {
        if (Context->InternalRequest->Req.Cleanup.Delete)
            return FuseOpGuardReleaseExclusive(Context);
        else
            return FuseOpGuardFalse;
    }
}

static BOOLEAN FuseOpClose(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        /* NOTE: CLOSE cannot report failure! */

        Context->Fini = FuseOpClose_ContextFini;
        Context->File = (PVOID)(UINT_PTR)Context->InternalRequest->Req.Close.UserContext2;

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
        FuseFileDelete(Context->Instance, Context->File);
}

static BOOLEAN FuseOpRead(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        Context->File = (PVOID)(UINT_PTR)Context->InternalRequest->Req.Read.UserContext2;

        Context->Read.StartOffset = Context->InternalRequest->Req.Read.Offset;
        Context->Read.Remain = Context->InternalRequest->Req.Read.Length;

        Context->Read.Offset = 0;
        while (0 != Context->Read.Remain)
        {
            Context->Read.Length = Context->Read.Remain;
#if DBG
            if (DEBUGTEST(10) && Context->Read.Length > 512)
                Context->Read.Length = 512;
#endif
#if 0
            FUSE_DEVICE_EXTENSION *Instance = FuseDeviceExtension(Context->DeviceObject);
            if (Context->Read.Length > Instance->VolumeParams.MaxRead)
                Context->Read.Length = Instance->VolumeParams.MaxRead;
#endif

            coro_await (FuseProtoSendRead(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            UINT32 BytesTransferred = Context->FuseResponse->len - FUSE_PROTO_RSP_HEADER_SIZE;
            if (Context->Read.Length < BytesTransferred)
            {
                Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_INTERNAL_ERROR;
                coro_break;
            }

            Context->InternalResponse->IoStatus.Status = FuseSafeCopyMemory(
                (PUINT8)(UINT_PTR)Context->InternalRequest->Req.Read.Address + Context->Write.Offset,
                (PUINT8)Context->FuseResponse + FUSE_PROTO_RSP_HEADER_SIZE,
                BytesTransferred);
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            Context->Read.Remain -= BytesTransferred;
            Context->Read.Offset += BytesTransferred;

            if (Context->Read.Length > BytesTransferred)
                break;
        }

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
        Context->InternalResponse->IoStatus.Information = Context->Read.Offset;
        if (0 == Context->InternalResponse->IoStatus.Information)
            Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_END_OF_FILE;
    }

    return coro_active();
}

static BOOLEAN FuseOpWrite(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        Context->File = (PVOID)(UINT_PTR)Context->InternalRequest->Req.Write.UserContext2;

        coro_await (FuseProtoSendFgetattr(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->Write.Attr = Context->FuseResponse->rsp.getattr.attr;

        UINT64 EndOffset;
        Context->Write.StartOffset = Context->InternalRequest->Req.Write.Offset;
        if (Context->InternalRequest->Req.Write.ConstrainedIo)
        {
            if (Context->Write.StartOffset >= Context->Write.Attr.size)
            {
                FuseAttrToFileInfo(Context->Instance, &Context->Write.Attr,
                    &Context->InternalResponse->Rsp.Write.FileInfo);
                Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
                Context->InternalResponse->IoStatus.Information = 0;
                coro_break;
            }
            EndOffset = Context->Write.StartOffset + Context->InternalRequest->Req.Write.Length;
            if (EndOffset > Context->Write.Attr.size)
                EndOffset = Context->Write.Attr.size;
        }
        else
        {
            if ((UINT64)-1LL == Context->Write.StartOffset)
                Context->Write.StartOffset = Context->Write.Attr.size;
            EndOffset = Context->Write.StartOffset + Context->InternalRequest->Req.Write.Length;
        }
        Context->Write.Remain = (UINT32)(EndOffset - Context->Write.StartOffset);

        Context->Write.Offset = 0;
        while (0 != Context->Write.Remain)
        {
            FuseContextWaitRequest(Context);

            Context->Write.Length = Context->Write.Remain;
#if DBG
            if (DEBUGTEST(10) && Context->Write.Length > 512)
                Context->Write.Length = 512;
#endif
            if (Context->Write.Length > Context->FuseRequestLength - FUSE_PROTO_REQ_SIZE(write))
                Context->Write.Length = Context->FuseRequestLength - FUSE_PROTO_REQ_SIZE(write);

            Context->InternalResponse->IoStatus.Status = FuseSafeCopyMemory(
                (PUINT8)Context->FuseRequest + FUSE_PROTO_REQ_SIZE(write),
                (PUINT8)(UINT_PTR)Context->InternalRequest->Req.Write.Address + Context->Write.Offset,
                Context->Write.Length);
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            coro_await (FuseProtoSendWrite(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            UINT32 BytesTransferred = Context->FuseResponse->rsp.write.size;
            if (Context->Write.Length < BytesTransferred)
            {
                Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_INTERNAL_ERROR;
                coro_break;
            }

            Context->Write.Remain -= BytesTransferred;
            Context->Write.Offset += BytesTransferred;

            if (Context->Write.Length > BytesTransferred)
                break;
        }

        if (Context->Write.Attr.size < Context->Write.StartOffset + Context->Write.Offset)
            Context->Write.Attr.size = Context->Write.StartOffset + Context->Write.Offset;

        FuseCacheQuickExpireItem(Context->Instance->Cache,
            Context->File->CacheItem);

        FuseAttrToFileInfo(Context->Instance, &Context->Write.Attr,
            &Context->InternalResponse->Rsp.Write.FileInfo);

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
        Context->InternalResponse->IoStatus.Information = Context->Write.Offset;
    }

    return coro_active();
}

static BOOLEAN FuseOpQueryInformation(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        Context->File = (PVOID)(UINT_PTR)Context->InternalRequest->Req.QueryInformation.UserContext2;

        coro_await (FuseProtoSendFgetattr(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        FuseAttrToFileInfo(Context->Instance, &Context->FuseResponse->rsp.getattr.attr,
            &Context->InternalResponse->Rsp.QueryInformation.FileInfo);

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
    }

    return coro_active();
}

static BOOLEAN FuseOpSetInformation_SetBasicInfo(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        Context->File = (PVOID)(UINT_PTR)Context->InternalRequest->Req.SetInformation.UserContext2;

        if (0 != Context->InternalRequest->Req.SetInformation.Info.Basic.LastAccessTime ||
            0 != Context->InternalRequest->Req.SetInformation.Info.Basic.LastWriteTime)
        {
            if (0 != Context->InternalRequest->Req.SetInformation.Info.Basic.LastAccessTime)
                FuseFileTimeToUnixTime(Context->InternalRequest->Req.SetInformation.Info.Basic.LastAccessTime,
                    &Context->Setattr.Attr.atime, &Context->Setattr.Attr.atimensec);
            else
                Context->Setattr.Attr.atimensec = FUSE_PROTO_UTIME_OMIT;
            if (0 != Context->InternalRequest->Req.SetInformation.Info.Basic.LastWriteTime)
                FuseFileTimeToUnixTime(Context->InternalRequest->Req.SetInformation.Info.Basic.LastWriteTime,
                    &Context->Setattr.Attr.mtime, &Context->Setattr.Attr.mtimensec);
            else
                Context->Setattr.Attr.mtimensec = FUSE_PROTO_UTIME_OMIT;

            coro_await (FuseProtoSendFutimens(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;
        }

        coro_await (FuseProtoSendFgetattr(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        FuseCacheQuickExpireItem(Context->Instance->Cache,
            Context->File->CacheItem);

        FuseAttrToFileInfo(Context->Instance, &Context->FuseResponse->rsp.getattr.attr,
            &Context->InternalResponse->Rsp.SetInformation.FileInfo);

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
    }

    return coro_active();
}

static BOOLEAN FuseOpSetInformation_SetAllocationSize(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        Context->File = (PVOID)(UINT_PTR)Context->InternalRequest->Req.SetInformation.UserContext2;

        coro_await (FuseProtoSendFgetattr(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        if (Context->FuseResponse->rsp.getattr.attr.size >
            Context->InternalRequest->Req.SetInformation.Info.Allocation.AllocationSize)
        {
            Context->Setattr.Attr.size =
                Context->InternalRequest->Req.SetInformation.Info.Allocation.AllocationSize;
            coro_await (FuseProtoSendFtruncate(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            coro_await (FuseProtoSendFgetattr(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;
        }

        FuseCacheQuickExpireItem(Context->Instance->Cache,
            Context->File->CacheItem);

        FuseAttrToFileInfo(Context->Instance, &Context->FuseResponse->rsp.getattr.attr,
            &Context->InternalResponse->Rsp.SetInformation.FileInfo);

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
    }

    return coro_active();
}

static BOOLEAN FuseOpSetInformation_SetFileSize(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        Context->File = (PVOID)(UINT_PTR)Context->InternalRequest->Req.SetInformation.UserContext2;

        Context->Setattr.Attr.size =
            Context->InternalRequest->Req.SetInformation.Info.EndOfFile.FileSize;
        coro_await (FuseProtoSendFtruncate(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        coro_await (FuseProtoSendFgetattr(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        FuseCacheQuickExpireItem(Context->Instance->Cache,
            Context->File->CacheItem);

        FuseAttrToFileInfo(Context->Instance, &Context->FuseResponse->rsp.getattr.attr,
            &Context->InternalResponse->Rsp.SetInformation.FileInfo);

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
    }

    return coro_active();
}

static BOOLEAN FuseOpSetInformation_SetDelete(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        Context->File = (PVOID)(UINT_PTR)Context->InternalRequest->Req.SetInformation.UserContext2;

        if (Context->InternalRequest->Req.SetInformation.Info.Disposition.Delete &&
            Context->File->IsDirectory && !Context->File->IsReparsePoint)
        {
            Context->QueryDirectory.NextOffset = 0;
            Context->QueryDirectory.Length =
                FSP_FSCTL_ALIGN_UP(sizeof(FUSE_PROTO_DIRENT) + 1, 8) +
                FSP_FSCTL_ALIGN_UP(sizeof(FUSE_PROTO_DIRENT) + 2, 8) +
                FSP_FSCTL_ALIGN_UP(sizeof(FUSE_PROTO_DIRENT) + 255, 8);
                /* enough for ".", ".." and an entry with a name up to 255 chars long */

            coro_await (FuseProtoSendReaddir(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;

            if (FUSE_PROTO_RSP_HEADER_SIZE + Context->QueryDirectory.Length < Context->FuseResponse->len)
            {
                Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_INTERNAL_ERROR;
                coro_break;
            }

            PUINT8 BufferEndP = (PUINT8)Context->FuseResponse + Context->FuseResponse->len;
            PUINT8 BufferP = (PUINT8)Context->FuseResponse + FUSE_PROTO_RSP_HEADER_SIZE;
            for (;;)
            {
                if (BufferEndP <
                        BufferP + FIELD_OFFSET(FUSE_PROTO_DIRENT, name) ||
                    BufferEndP <
                        BufferP + FIELD_OFFSET(FUSE_PROTO_DIRENT, name) +
                            ((FUSE_PROTO_DIRENT *)BufferP)->namelen)
                    break;

                FUSE_PROTO_DIRENT *Dirent = (FUSE_PROTO_DIRENT *)BufferP;
                if ((1 == Dirent->namelen && '.' == Dirent->name[0]) ||
                    (2 == Dirent->namelen && '.' == Dirent->name[0] && '.' == Dirent->name[1]))
                    /*ignore*/;
                else
                {
                    Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_DIRECTORY_NOT_EMPTY;
                    coro_break;
                }

                BufferP += FSP_FSCTL_ALIGN_UP(
                    FIELD_OFFSET(FUSE_PROTO_DIRENT, name) +
                        ((FUSE_PROTO_DIRENT *)BufferP)->namelen,
                    8);
            }
        }

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
    }

    return coro_active();
}

static BOOLEAN FuseOpSetInformation_Rename(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        Context->File = (PVOID)(UINT_PTR)Context->InternalRequest->Req.SetInformation.UserContext2;

        FusePrepareLookupPath(Context);
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        coro_await (FuseRenameCheck(Context));
        if (NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            Context->LookupPath.RenameIsDirectory = 0040000 == (Context->LookupPath.Attr.mode & 0170000);
        else
        if (STATUS_OBJECT_PATH_NOT_FOUND == Context->InternalResponse->IoStatus.Status ||
            STATUS_OBJECT_NAME_NOT_FOUND == Context->InternalResponse->IoStatus.Status)
            Context->LookupPath.RenameIsNonExistent = 1;
        else
            coro_break;

        FusePrepareLookupPath2(Context);
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        FusePosixPathSuffix(&Context->LookupPath.OrigPath, &Context->LookupPath.Remain, 0);
        coro_await (FuseLookupPath(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        FusePosixPathSuffix(&Context->LookupPath.OrigPath, 0, &Context->LookupPath.Name);

        if (!Context->LookupPath.RenameIsNonExistent &&
            (Context->Instance->VolumeParams->CaseSensitiveSearch ||
                Context->LookupPath.Ino != Context->LookupPath.Ino2 ||
                !RtlEqualString(&Context->LookupPath.Name, &Context->LookupPath.Name2, TRUE)))
        {
            if (0 == Context->InternalRequest->Req.SetInformation.Info.Rename.AccessToken)
            {
                Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_OBJECT_NAME_COLLISION;
                coro_break;
            }

            if (Context->LookupPath.RenameIsDirectory)
            {
                Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_ACCESS_DENIED;
                coro_break;
            }
        }

        coro_await (FuseProtoSendRename(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        FuseCacheRemoveEntry(
            Context->Instance->Cache,
            Context->LookupPath.Ino, &Context->LookupPath.Name);

        FuseCacheRemoveEntry(
            Context->Instance->Cache,
            Context->LookupPath.Ino2, &Context->LookupPath.Name2);

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
    }

    return coro_active();
}

static BOOLEAN FuseOpSetInformation(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    switch (Context->InternalRequest->Req.SetInformation.FileInformationClass)
    {
    case FileBasicInformation:
        return FuseOpSetInformation_SetBasicInfo(Context);
    case FileAllocationInformation:
        return FuseOpSetInformation_SetAllocationSize(Context);
    case FileEndOfFileInformation:
        return FuseOpSetInformation_SetFileSize(Context);
    case FileDispositionInformation:
        return FuseOpSetInformation_SetDelete(Context);
    case FileRenameInformation:
        return FuseOpSetInformation_Rename(Context);
    default:
        Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_INVALID_DEVICE_REQUEST;
        return FALSE;
    }
}

static INT FuseOgSetInformation(FUSE_CONTEXT *Context, BOOLEAN Acquire)
{
    PAGED_CODE();

    if (Acquire)
    {
        if (FileRenameInformation == Context->InternalRequest->Req.SetInformation.FileInformationClass)
            return FuseOpGuardAcquireExclusive(Context);
        else
        if (FileDispositionInformation == Context->InternalRequest->Req.SetInformation.FileInformationClass)
            return FuseOpGuardAcquireShared(Context);
        else
            return FuseOpGuardFalse;
    }
    else
    {
        if (FileRenameInformation == Context->InternalRequest->Req.SetInformation.FileInformationClass)
            return FuseOpGuardReleaseExclusive(Context);
        else
        if (FileDispositionInformation == Context->InternalRequest->Req.SetInformation.FileInformationClass)
            return FuseOpGuardReleaseShared(Context);
        else
            return FuseOpGuardFalse;
    }
}

static BOOLEAN FuseOpQueryEa(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

static BOOLEAN FuseOpSetEa(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

static BOOLEAN FuseOpFlushBuffers(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        Context->File = (PVOID)(UINT_PTR)Context->InternalRequest->Req.FlushBuffers.UserContext2;

        if (0 == Context->File)
        {
            /* FUSE cannot flush volumes */
            Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
            coro_break;
        }

        if (Context->File->IsDirectory)
            coro_await (FuseProtoSendFsyncdir(Context));
        else
            coro_await (FuseProtoSendFsync(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status) &&
            STATUS_INVALID_DEVICE_REQUEST != Context->InternalResponse->IoStatus.Status)
            coro_break;

        coro_await (FuseProtoSendFgetattr(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        FuseCacheQuickExpireItem(Context->Instance->Cache,
            Context->File->CacheItem);

        FuseAttrToFileInfo(Context->Instance, &Context->FuseResponse->rsp.getattr.attr,
            &Context->InternalResponse->Rsp.FlushBuffers.FileInfo);

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
    }

    return coro_active();
}

static BOOLEAN FuseOpQueryVolumeInformation(FUSE_CONTEXT *Context)
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

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
    }

    return coro_active();
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
            FuseAttrToFileInfo(Context->Instance, Attr, &DirInfo->FileInfo);
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
        coro_await (FuseLookup(Context));

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
        Context->QueryDirectory.Length = N * (sizeof(FUSE_PROTO_DIRENT) + 24);

        coro_await (FuseProtoSendReaddir(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        if (FUSE_PROTO_RSP_HEADER_SIZE + Context->QueryDirectory.Length < Context->FuseResponse->len)
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
                coro_await (FuseLookup(Context));
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

static BOOLEAN FuseOpQueryDirectory(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        Context->Fini = FuseOpQueryDirectory_ContextFini;
        Context->File = (PVOID)(UINT_PTR)Context->InternalRequest->Req.QueryDirectory.UserContext2;

        Context->InternalResponse->IoStatus.Status = FuseCacheReferenceGen(
            Context->Instance->Cache, &Context->QueryDirectory.CacheGen);
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
    FuseCacheDereferenceGen(Context->Instance->Cache, Context->QueryDirectory.CacheGen);
        /* handles NULL gens */
}

static INT FuseOgQueryDirectory(FUSE_CONTEXT *Context, BOOLEAN Acquire)
{
    PAGED_CODE();

    if (Acquire)
        return FuseOpGuardAcquireShared(Context);
    else
        return FuseOpGuardReleaseShared(Context);
}

static BOOLEAN FuseOpFileSystemControl(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

static BOOLEAN FuseOpDeviceControl(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    return FALSE;
}

static BOOLEAN FuseOpQuerySecurity(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        Context->Fini = FuseSecurity_ContextFini;
        Context->File = (PVOID)(UINT_PTR)Context->InternalRequest->Req.QuerySecurity.UserContext2;

        coro_await (FuseProtoSendFgetattr(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->IoStatus.Status = FspPosixMapPermissionsToSecurityDescriptor(
            Context->FuseResponse->rsp.getattr.attr.uid,
            Context->FuseResponse->rsp.getattr.attr.gid,
            Context->FuseResponse->rsp.getattr.attr.mode,
            &Context->Security.SecurityDescriptor);
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        ULONG Length = RtlLengthSecurityDescriptor(Context->Security.SecurityDescriptor);
        if (FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX < Length)
        {
            Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_INVALID_SECURITY_DESCR;
            coro_break;
        }

        PVOID InternalResponse = FuseAlloc(sizeof *Context->InternalResponse + Length);
        if (0 == InternalResponse)
        {
            Context->InternalResponse->IoStatus.Status = (UINT32)STATUS_INSUFFICIENT_RESOURCES;
            coro_break;
        }
        RtlZeroMemory(InternalResponse, sizeof *Context->InternalResponse);

        Context->InternalResponse = InternalResponse;
        Context->InternalResponse->Size = (UINT16)(sizeof *Context->InternalResponse + Length);
        Context->InternalResponse->Kind = Context->InternalRequest->Kind;
        Context->InternalResponse->Hint = Context->InternalRequest->Hint;
        Context->InternalResponse->Rsp.QuerySecurity.SecurityDescriptor.Offset = 0;
        Context->InternalResponse->Rsp.QuerySecurity.SecurityDescriptor.Size = (UINT16)Length;

        /* RtlCopyMemory is safe here, because all buffers are in-kernel */
        RtlCopyMemory(
            Context->InternalResponse->Buffer, Context->Security.SecurityDescriptor, Length);

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
    }

    return coro_active();
}

static BOOLEAN FuseOpSetSecurity(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    coro_block (Context->CoroState)
    {
        Context->Fini = FuseSecurity_ContextFini;
        Context->File = (PVOID)(UINT_PTR)Context->InternalRequest->Req.SetSecurity.UserContext2;

        coro_await (FuseProtoSendFgetattr(Context));
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->InternalResponse->IoStatus.Status = FspPosixMapPermissionsToSecurityDescriptor(
            Context->FuseResponse->rsp.getattr.attr.uid,
            Context->FuseResponse->rsp.getattr.attr.gid,
            Context->FuseResponse->rsp.getattr.attr.mode,
            &Context->Security.SecurityDescriptor);
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        /*
         * BEGIN:
         * No coro_yield or coro_await allowed, because Context->FuseResponse->rsp.getattr.attr
         * must remain valid.
         */

        PSECURITY_DESCRIPTOR NewSecurityDescriptor = Context->Security.SecurityDescriptor;
        Context->InternalResponse->IoStatus.Status = SeSetSecurityDescriptorInfo(
            0,
            (PSECURITY_INFORMATION)&Context->InternalRequest->Req.SetSecurity.SecurityInformation,
            (PSECURITY_DESCRIPTOR)Context->InternalRequest->Buffer,
            &NewSecurityDescriptor,
            PagedPool,
            IoGetFileObjectGenericMapping());
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;
        FuseFreeExternal(Context->Security.SecurityDescriptor);
        Context->Security.SecurityDescriptor = NewSecurityDescriptor;

        Context->InternalResponse->IoStatus.Status = FspPosixMapSecurityDescriptorToPermissions(
            Context->Security.SecurityDescriptor,
            &Context->Security.Attr.uid,
            &Context->Security.Attr.gid,
            &Context->Security.Attr.mode);
        if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
            coro_break;

        Context->Security.Attr.mode &= 07777;
        Context->Security.Attr.mode |= (Context->FuseResponse->rsp.getattr.attr.mode & 0170000);

        Context->Security.AttrValid = 0;
        Context->Security.AttrValid |=
            Context->Security.Attr.uid != Context->FuseResponse->rsp.getattr.attr.uid ?
                FUSE_PROTO_SETATTR_UID : 0;
        Context->Security.AttrValid |=
            Context->Security.Attr.gid != Context->FuseResponse->rsp.getattr.attr.gid ?
                FUSE_PROTO_SETATTR_GID : 0;
        Context->Security.AttrValid |=
            Context->Security.Attr.mode != Context->FuseResponse->rsp.getattr.attr.mode ?
                FUSE_PROTO_SETATTR_MODE : 0;

        /*
         * END:
         * No coro_yield or coro_await allowed, because Context->FuseResponse->rsp.getattr.attr
         * must remain valid.
         */

        if (0 != Context->Security.AttrValid)
        {
            coro_await (FuseProtoSendSetattr(Context));
            if (!NT_SUCCESS(Context->InternalResponse->IoStatus.Status))
                coro_break;
        }

        Context->InternalResponse->IoStatus.Status = STATUS_SUCCESS;
    }

    return coro_active();
}

static VOID FuseSecurity_ContextFini(FUSE_CONTEXT *Context)
{
    PAGED_CODE();

    if (0 != Context->Security.SecurityDescriptor)
        FuseFreeExternal(Context->Security.SecurityDescriptor);
}

FUSE_OPERATION FuseOperations[FspFsctlTransactKindCount] =
{
    /* FspFsctlTransactReservedKind */
    { FuseOpReserved },

    /* FspFsctlTransactCreateKind */
    { FuseOpCreate, FuseOgCreate },

    /* FspFsctlTransactOverwriteKind */
    { FuseOpOverwrite },

    /* FspFsctlTransactCleanupKind */
    { FuseOpCleanup, FuseOgCleanup },

    /* FspFsctlTransactCloseKind */
    { FuseOpClose },

    /* FspFsctlTransactReadKind */
    { FuseOpRead },

    /* FspFsctlTransactWriteKind */
    { FuseOpWrite },

    /* FspFsctlTransactQueryInformationKind */
    { FuseOpQueryInformation },

    /* FspFsctlTransactSetInformationKind */
    { FuseOpSetInformation, FuseOgSetInformation },

    /* FspFsctlTransactQueryEaKind */
    { 0 },

    /* FspFsctlTransactSetEaKind */
    { 0 },

    /* FspFsctlTransactFlushBuffersKind */
    { FuseOpFlushBuffers },

    /* FspFsctlTransactQueryVolumeInformationKind */
    { FuseOpQueryVolumeInformation },

    /* FspFsctlTransactSetVolumeInformationKind */
    { 0 },

    /* FspFsctlTransactQueryDirectoryKind */
    { FuseOpQueryDirectory, FuseOgQueryDirectory },

    /* FspFsctlTransactFileSystemControlKind */
    { 0 },

    /* FspFsctlTransactDeviceControlKind */
    { 0 },

    /* FspFsctlTransactShutdownKind */
    { 0 },

    /* FspFsctlTransactLockControlKind */
    { 0 },

    /* FspFsctlTransactQuerySecurityKind */
    { FuseOpQuerySecurity },

    /* FspFsctlTransactSetSecurityKind */
    { FuseOpSetSecurity },

    /* FspFsctlTransactQueryStreamInformationKind */
    { 0 },
};
