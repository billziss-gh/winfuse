@echo off

setlocal

set CONFIG=Debug
set SUFFIX=x64
set TARGET_MACHINE=WIN10DBG
if not X%1==X set TARGET_MACHINE=%1
set TARGET_ACCOUNT=\Users\%USERNAME%\Downloads\winfuse\
set TARGET=\\%TARGET_MACHINE%%TARGET_ACCOUNT%

cd %~dp0..
mkdir %TARGET% 2>nul
for %%f in (winfuse-%SUFFIX%.sys winfuse-tests-%SUFFIX%.exe) do (
    copy build\VStudio\build\%CONFIG%\%%f %TARGET% >nul
)
for %%f in (winfsp-%SUFFIX%.sys winfsp-%SUFFIX%.dll) do (
    copy ext\winfsp\build\VStudio\build\%CONFIG%\%%f %TARGET% >nul
)

echo sc delete WinFsp                                                            >%TARGET%kminst.bat
echo sc delete WinFuse                                                          >>%TARGET%kminst.bat
echo sc create WinFsp type=filesys binPath=%%~dp0winfsp-%SUFFIX%.sys            >>%TARGET%kminst.bat
echo sc create WinFuse type=kernel binPath=%%~dp0winfuse-%SUFFIX%.sys           >>%TARGET%kminst.bat
echo reg add HKLM\Software\WinFsp\Fsext /v 00093118 /d "winfuse" /f /reg:32     >>%TARGET%kminst.bat
