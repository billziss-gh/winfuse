/**
 * @file CustomActions.cpp
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

#include <windows.h>
#include <msiquery.h>
#include <wcautil.h>
#include <strutil.h>

/*
 * Define MSITEST for MSI testing and no actual command execution.
 */
//#define MSITEST

#if !defined(MSITEST)

static DWORD WaitServiceStatus(SC_HANDLE SvcHandle, DWORD State, DWORD Timeout)
{
    SERVICE_STATUS SvcStatus;

    for (DWORD I = 0, N = (Timeout + 499) / 500; N > I; I++)
    {
        if (!QueryServiceStatus(SvcHandle, &SvcStatus))
            return GetLastError();

        if (State == SvcStatus.dwCurrentState)
            return ERROR_SUCCESS;

        Sleep(500);
    }

    return ERROR_TIMEOUT;
}

static BOOL EqualServicePaths(PWSTR ServicePath0, PWSTR ServicePath)
{
    size_t Len;
    return
        0 == wcscmp(ServicePath0, ServicePath) ||
        (L'\\' == ServicePath0[0] &&
            L'?' == ServicePath0[1] &&
            L'?' == ServicePath0[2] &&
            L'\\' == ServicePath0[3] &&
            0 == wcscmp(ServicePath0 + 4, ServicePath)) ||
        (Len = wcslen(ServicePath),
            L'"' == ServicePath0[0] &&
            0 == wcsncmp(ServicePath0 + 1, ServicePath, Len) &&
            L'"' == ServicePath0[Len + 1] &&
            L'\0' == ServicePath0[Len + 2]);
}

static DWORD AddService(PWSTR ServiceName, PWSTR ServicePath, DWORD ServiceType, DWORD StartType)
{
    SC_HANDLE ScmHandle = 0;
    SC_HANDLE SvcHandle = 0;
    SERVICE_STATUS SvcStatus;
    QUERY_SERVICE_CONFIGW *SvcConfig = 0;
    PVOID VersionInfo = 0;
    SERVICE_DESCRIPTIONW SvcDescription;
    DWORD Size;
    BOOL Created = FALSE, RebootRequired = FALSE;
    DWORD Result;

    ScmHandle = OpenSCManagerW(0, 0, SC_MANAGER_CREATE_SERVICE);
    if (0 == ScmHandle)
    {
        Result = GetLastError();
        goto exit;
    }

    SvcHandle = OpenServiceW(ScmHandle, ServiceName,
        SERVICE_CHANGE_CONFIG | SERVICE_QUERY_CONFIG | SERVICE_START | SERVICE_QUERY_STATUS);
    if (0 != SvcHandle)
    {
        if (!QueryServiceConfigW(SvcHandle, 0, 0, &Size))
        {
            Result = GetLastError();
            if (ERROR_INSUFFICIENT_BUFFER != Result)
                goto exit;
        }

        SvcConfig = (QUERY_SERVICE_CONFIGW *)HeapAlloc(GetProcessHeap(), 0, Size);
        if (0 == SvcConfig)
        {
            Result = ERROR_NO_SYSTEM_RESOURCES;
            goto exit;
        }

        if (!QueryServiceConfigW(SvcHandle, SvcConfig, Size, &Size))
        {
            Result = GetLastError();
            goto exit;
        }

        if (SvcConfig->dwServiceType != ServiceType ||
            SvcConfig->dwStartType != StartType ||
            !EqualServicePaths(SvcConfig->lpBinaryPathName, ServicePath) ||
            0 != wcscmp(SvcConfig->lpDisplayName, ServiceName))
        {
            if (!ChangeServiceConfigW(SvcHandle,
                ServiceType, StartType, SERVICE_ERROR_NORMAL, ServicePath,
                0, 0, 0, 0, 0, ServiceName))
            {
                Result = GetLastError();
                goto exit;
            }

            if (QueryServiceStatus(SvcHandle, &SvcStatus) &&
                SERVICE_RUNNING == SvcStatus.dwCurrentState)
                RebootRequired = TRUE;
        }
    }
    else
    {
        SvcHandle = CreateServiceW(ScmHandle, ServiceName, ServiceName,
            SERVICE_CHANGE_CONFIG | SERVICE_START | SERVICE_QUERY_STATUS | DELETE,
            ServiceType, StartType, SERVICE_ERROR_NORMAL, ServicePath,
            0, 0, 0, 0, 0);
        if (0 == SvcHandle)
        {
            Result = GetLastError();
            goto exit;
        }

        Created = TRUE;
    }

    Size = GetFileVersionInfoSizeW(ServicePath, &Size/*dummy*/);
    if (0 < Size)
    {
        VersionInfo = HeapAlloc(GetProcessHeap(), 0, Size);
        if (0 != VersionInfo &&
            GetFileVersionInfoW(ServicePath, 0, Size, VersionInfo) &&
            VerQueryValueW(VersionInfo, L"\\StringFileInfo\\040904b0\\FileDescription",
                (PVOID *)&SvcDescription.lpDescription, (PUINT)&Size))
        {
            ChangeServiceConfig2W(SvcHandle, SERVICE_CONFIG_DESCRIPTION, &SvcDescription);
        }
    }

    if (SERVICE_BOOT_START == StartType ||
        SERVICE_SYSTEM_START == StartType ||
        SERVICE_AUTO_START == StartType)
    {
        if (!StartService(SvcHandle, 0, 0))
        {
            Result = GetLastError();
            if (ERROR_SERVICE_ALREADY_RUNNING == Result)
                Result = ERROR_SUCCESS;
            else
                goto exit;
        }

        Result = WaitServiceStatus(SvcHandle, SERVICE_RUNNING, 5000);
        if (ERROR_SUCCESS != Result && ERROR_TIMEOUT != Result)
            goto exit;
    }

    Result = RebootRequired ? ERROR_SUCCESS_REBOOT_REQUIRED : ERROR_SUCCESS;

exit:
    if (ERROR_SUCCESS != Result && Created)
        DeleteService(SvcHandle);
    if (0 != VersionInfo)
        HeapFree(GetProcessHeap(), 0, VersionInfo);
    if (0 != SvcConfig)
        HeapFree(GetProcessHeap(), 0, SvcConfig);
    if (0 != SvcHandle)
        CloseServiceHandle(SvcHandle);
    if (0 != ScmHandle)
        CloseServiceHandle(ScmHandle);

    return Result;
}

static DWORD RemoveService(PWSTR ServiceName)
{
    SC_HANDLE ScmHandle = 0;
    SC_HANDLE SvcHandle = 0;
    SERVICE_STATUS SvcStatus;
    BOOL RebootRequired = FALSE;
    DWORD Result;

    ScmHandle = OpenSCManagerW(0, 0, SC_MANAGER_CREATE_SERVICE);
        /*
         * The SC_MANAGER_CREATE_SERVICE access right is not strictly needed here,
         * but we use it to enforce admin rights.
         */
    if (0 == ScmHandle)
    {
        Result = GetLastError();
        goto exit;
    }

    SvcHandle = OpenServiceW(ScmHandle, ServiceName,
        SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE);
    if (0 == SvcHandle)
    {
        Result = GetLastError();
        if (ERROR_SERVICE_DOES_NOT_EXIST == Result)
            Result = ERROR_SUCCESS;
        goto exit;
    }

    ControlService(SvcHandle, SERVICE_CONTROL_STOP, &SvcStatus);
    Result = WaitServiceStatus(SvcHandle, SERVICE_STOPPED, 5000);
    if (ERROR_SUCCESS != Result && ERROR_TIMEOUT != Result)
        goto exit;
    RebootRequired = ERROR_TIMEOUT == Result;

    if (!DeleteService(SvcHandle))
    {
        Result = GetLastError();
        if (ERROR_SERVICE_MARKED_FOR_DELETE == Result)
            RebootRequired = TRUE;
        else
            goto exit;
    }

    Result = RebootRequired ? ERROR_SUCCESS_REBOOT_REQUIRED : ERROR_SUCCESS;

exit:
    if (0 != SvcHandle)
        CloseServiceHandle(SvcHandle);
    if (0 != ScmHandle)
        CloseServiceHandle(ScmHandle);

    return Result;
}

#else

static DWORD AddService(PWSTR ServiceName, PWSTR ServicePath, DWORD ServiceType, DWORD StartType)
{
    WCHAR MessageBuf[1024];
    wsprintfW(MessageBuf,
        L"" __FUNCTION__ "(ServiceName=%s, ServicePath=%s, ServiceType=0x%lx, StartType=0x%lx)",
        ServiceName, ServicePath, ServiceType, StartType);
    MessageBoxW(0, MessageBuf, L"CustomActions: " __FUNCTION__, MB_ICONINFORMATION);

    return ERROR_SUCCESS;
}

static DWORD RemoveService(PWSTR ServiceName)
{
    WCHAR MessageBuf[1024];
    wsprintfW(MessageBuf,
        L"" __FUNCTION__ "(ServiceName=%s)",
        ServiceName);
    MessageBoxW(0, MessageBuf, L"CustomActions: " __FUNCTION__, MB_ICONINFORMATION);

    return ERROR_SUCCESS;
}

#endif

static DWORD DoAddService(INT Argc, PWSTR *Argv)
{
    DWORD ServiceType = SERVICE_KERNEL_DRIVER;
    DWORD StartType = SERVICE_DEMAND_START;

    if (3 > Argc || Argc > 5)
        return ERROR_INVALID_PARAMETER;

    if (4 <= Argc)
    {
        PWSTR Endp;
        ServiceType = wcstoul(Argv[3], &Endp, 0);
        if (Argv[3] == Endp || L'\0' != *Endp)
            return ERROR_INVALID_PARAMETER;
    }

    if (5 <= Argc)
    {
        PWSTR Endp;
        StartType = wcstoul(Argv[4], &Endp, 0);
        if (Argv[4] == Endp || L'\0' != *Endp)
            return ERROR_INVALID_PARAMETER;
    }

    return AddService(Argv[1], Argv[2], ServiceType, StartType);
}

static DWORD DoRemoveService(INT Argc, PWSTR *Argv)
{
    if (2 != Argc)
        return ERROR_INVALID_PARAMETER;

    return RemoveService(Argv[1]);
}

static DWORD DispatchCommand(INT Argc, PWSTR *Argv)
{
    if (1 > Argc)
        return ERROR_INVALID_PARAMETER;

    if (0 == wcscmp(L"AddService", Argv[0]))
        return DoAddService(Argc, Argv);
    else
    if (0 == wcscmp(L"RemoveService", Argv[0]))
        return DoRemoveService(Argc, Argv);
    else
        return ERROR_INVALID_PARAMETER;
}

UINT __stdcall ExecuteCommand(MSIHANDLE MsiHandle)
{
#if 0
    WCHAR MessageBuf[1024];
    wsprintfW(MessageBuf, L"DebugAttach: PID=%ld", GetCurrentProcessId());
    MessageBoxW(0, MessageBuf, L"CustomActions: " __FUNCTION__, MB_ICONSTOP);
#endif

    HRESULT hr = S_OK;
    UINT err = ERROR_SUCCESS;
    PWSTR CommandLine = 0;
    INT Argc; PWSTR *Argv = 0;
    DWORD ExitCode = ERROR_SUCCESS;

    hr = WcaInitialize(MsiHandle, __FUNCTION__);
    ExitOnFailure(hr, "Failed to initialize");

    hr = WcaGetProperty(L"CustomActionData", &CommandLine);
    ExitOnFailure(hr, "Failed to get CommandLine");

    WcaLog(LOGMSG_STANDARD, "Initialized: \"%S\"", CommandLine);

    Argv = CommandLineToArgvW(CommandLine, &Argc);
    if (0 == Argv)
        ExitWithLastError(hr, "Failed to CommandLineToArgvW");

    ExitCode = DispatchCommand(Argc, Argv);
    if (ERROR_SUCCESS_REBOOT_REQUIRED == ExitCode)
    {
        WcaDeferredActionRequiresReboot();
        ExitCode = ERROR_SUCCESS;
    }
    ExitOnWin32Error(ExitCode, hr, "Failed to DispatchCommand");

LExit:
    LocalFree(Argv);
    ReleaseStr(CommandLine);

    err = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(err);
}

UINT __stdcall CheckReboot(MSIHANDLE MsiHandle)
{
#if 0
    WCHAR MessageBuf[1024];
    wsprintfW(MessageBuf, L"DebugAttach: PID=%ld", GetCurrentProcessId());
    MessageBoxW(0, MessageBuf, L"CustomActions: " __FUNCTION__, MB_ICONSTOP);
#endif

    HRESULT hr = S_OK;
    UINT err = ERROR_SUCCESS;
    BOOL RebootRequired = FALSE;

    hr = WcaInitialize(MsiHandle, __FUNCTION__);
    ExitOnFailure(hr, "Failed to initialize");

    RebootRequired = WcaDidDeferredActionRequireReboot();
    WcaLog(LOGMSG_STANDARD, "Initialized: RebootRequired=%d", RebootRequired);

    if (RebootRequired)
        err = MsiSetMode(MsiHandle, MSIRUNMODE_REBOOTATEND, TRUE);

LExit:
    err = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(err);
}

extern "C"
BOOL WINAPI DllMain(HINSTANCE Instance, DWORD Reason, PVOID Reserved)
{
    switch(Reason)
    {
    case DLL_PROCESS_ATTACH:
        WcaGlobalInitialize(Instance);
        break;
    case DLL_PROCESS_DETACH:
        WcaGlobalFinalize();
        break;
    }

    return TRUE;
}
