@echo off

setlocal
setlocal EnableDelayedExpansion

set Config=Debug
set Suffix=x64
set Deploy=C:\Deploy\winfuse
set Target=Win10DBG
set Chkpnt=winfuse
if not X%1==X set Target=%1
if not X%2==X set Chkpnt=%2

(
    echo sc create WinFsp type=filesys binPath=%%~dp0winfsp-%SUFFIX%.sys
    echo sc create LxLdr type=kernel binPath=%%~dp0lxldr.sys
    echo sc create WinFuse type=kernel binPath=%%~dp0winfuse-%SUFFIX%.sys
    echo sc create WslFuse type=kernel binPath=%%~dp0wslfuse-%SUFFIX%.sys
    echo reg add HKLM\Software\WinFsp\Fsext /v 00093118 /d "winfuse" /f /reg:32
    echo reg add HKLM\Software\LxDK\Services\wslfuse /f
    echo sc start winfsp
    echo sc start lxldr
) > %~dp0..\build\VStudio\build\%Config%\deploy-setup.bat

if exist %~dp0ext\winfsp\build\VStudio\build\%Config% (
    set WINFSP=%~dp0ext\winfsp\build\VStudio\build\%Config%\
) else (
    set RegKey="HKLM\SOFTWARE\WinFsp"
    set RegVal="InstallDir"
    reg query !RegKey! /v !RegVal! /reg:32 >nul 2>&1
    if !ERRORLEVEL! equ 0 (
        for /f "tokens=2,*" %%i in ('reg query !RegKey! /v !RegVal! /reg:32 ^| findstr !RegVal!') do (
            set WINFSP=%%jbin\
        )
    )
    if not exist "!WINFSP!" (echo cannot find WinFsp installation >&2 & goto fail)
)

if exist %~dp0ext\lxdk\build\VStudio\build\%Config% (
    set LXDK=%~dp0ext\lxdk\build\VStudio\build\%Config%\
) else (
    set RegKey="HKLM\SOFTWARE\LxDK"
    set RegVal="InstallDir"
    reg query !RegKey! /v !RegVal! >nul 2>&1
    if !ERRORLEVEL! equ 0 (
        for /f "tokens=2,*" %%i in ('reg query !RegKey! /v !RegVal! ^| findstr !RegVal!') do (
            set LXDK=%%jbin\
        )
    )
    if not exist "!LXDK!" (echo cannot find LxDK installation >&2 & goto fail)
)

set Files=
for %%f in (
    %~dp0..\build\VStudio\build\%Config%\
        winfuse-%Suffix%.sys
        wslfuse-%Suffix%.sys
        winfuse-tests-%Suffix%.exe
        wslfuse-tests.out
        fusermount.out
        deploy-setup.bat
    "!WINFSP!"
        winfsp-%Suffix%.sys
        winfsp-%Suffix%.dll
    "!LXDK!"
        lxldr.sys
    ) do (
    set File=%%~f
    if [!File:~-1!] == [\] (
        set Dir=!File!
    ) else (
        if not [!Files!] == [] set Files=!Files!,
        set Files=!Files!'!Dir!!File!'
    )
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "& '%~dp0deploy.ps1' -Name '%Target%' -CheckpointName '%Chkpnt%' -Files !Files! -Destination '%Deploy%'"
