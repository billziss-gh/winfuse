@echo off

setlocal
setlocal EnableDelayedExpansion

set CONFIG=Debug
set SUFFIX=x64
set TARGET_MACHINE=WIN10DBG
if not X%1==X set TARGET_MACHINE=%1
set TARGET_ACCOUNT=\Users\%USERNAME%\Downloads\winfuse\
set TARGET=\\%TARGET_MACHINE%%TARGET_ACCOUNT%

cd %~dp0..

if exist ext\winfsp\build\VStudio\build\%CONFIG% (
    set WINFSP=!cd!\ext\winfsp\build\VStudio\build\%CONFIG%\
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

mkdir %TARGET% 2>nul
for %%f in (winfuse-%SUFFIX%.sys winfuse-tests-%SUFFIX%.exe) do (
    copy build\VStudio\build\%CONFIG%\%%f %TARGET% >nul
)
for %%f in (winfsp-%SUFFIX%.sys winfsp-%SUFFIX%.dll) do (
    copy "!WINFSP!%%f" %TARGET% >nul
)
if exist tst\memfs-fuse3\build\%CONFIG% (
    for %%f in (memfs-fuse3-%SUFFIX%.exe fuse3.dll) do (
        copy tst\memfs-fuse3\build\%CONFIG%\%%f %TARGET% >nul
    )
)

echo sc delete WinFsp                                                            >%TARGET%kminst.bat
echo sc delete WinFuse                                                          >>%TARGET%kminst.bat
echo sc create WinFsp type=filesys binPath=%%~dp0winfsp-%SUFFIX%.sys            >>%TARGET%kminst.bat
echo sc create WinFuse type=kernel binPath=%%~dp0winfuse-%SUFFIX%.sys           >>%TARGET%kminst.bat
echo reg add HKLM\Software\WinFsp\Fsext /v 00093118 /d "winfuse" /f /reg:32     >>%TARGET%kminst.bat
exit /b 0

:fail
exit /b 1
