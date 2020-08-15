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
    echo reg add HKLM\Software\LxDK\Services\wslfuse /v Depends /d "winfsp" /f
    echo sc start winfsp
    echo sc start lxldr
) >%~dp0..\build\VStudio\build\%Config%\deploy-setup.bat

(set LF=^
%=this line is empty=%
)
(
    set /p =sudo mknod /dev/fuse c 10 229!LF!
    set /p =sudo chmod a+w /dev/fuse!LF!
    set /p =sudo cp fusermount.out /usr/bin/fusermount!LF!
    set /p =sudo cp fusermount.out /usr/bin/fusermount3!LF!
    set /p =sudo cp fusermount-helper.exe /usr/bin/fusermount-helper.exe!LF!
    set /p =sudo chmod u+s /usr/bin/fusermount!LF!
    set /p =sudo chmod u+s /usr/bin/fusermount3!LF!
) <nul >%~dp0..\build\VStudio\build\%Config%\deploy-setup.sh

if exist %~dp0..\ext\winfsp\build\VStudio\build\%Config% (
    set WINFSP=%~dp0..\ext\winfsp\build\VStudio\build\%Config%\
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

if exist %~dp0..\ext\lxdk\build\VStudio\build\%Config% (
    set LXDK=%~dp0..\ext\lxdk\build\VStudio\build\%Config%\
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

set MEMFS_FUSE3_EXE=
if exist %~dp0..\tst\memfs-fuse3\build\%Config%\memfs-fuse3-x64.exe (
    set MEMFS_FUSE3_EXE=memfs-fuse3-x64.exe fuse3-x64.dll
)
set MEMFS_FUSE3_OUT=
if exist %~dp0..\tst\memfs-fuse3\build\%Config%\memfs-fuse3.out (
    set MEMFS_FUSE3_OUT=memfs-fuse3.out
)

set Files=
for %%f in (
    %~dp0..\build\VStudio\build\%Config%\
        winfuse-%Suffix%.sys
        wslfuse-%Suffix%.sys
        winfuse-tests-%Suffix%.exe
        wslfuse-tests.out
        fusermount.out
        fusermount-helper.exe
        deploy-setup.bat
        deploy-setup.sh
    %~dp0..\tst\memfs-fuse3\build\%Config%\
        !MEMFS_FUSE3_EXE!
        !MEMFS_FUSE3_OUT!
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
