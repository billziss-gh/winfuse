@echo off

setlocal
setlocal EnableDelayedExpansion

if not X%1==X set InstallDir=%1
if X!InstallDir!==X (echo usage: build-libfuse InstallDir >&2 & goto fail)

rem installation directory for ninja
cd !InstallDir!
if errorlevel 1 goto fail
set DESTDIR=%cd%

cd %~dp0..\ext\libfuse
if errorlevel 1 goto fail

set vswhere="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`%vswhere% -find VC\**\vcvarsall.bat`) do (
    call "%%i" x64
)

rem workaround getting Cygwin's meson/ninja
set PATH=C:\Program Files\Meson;%PATH%

if exist build rmdir /s/q build
mkdir build && cd build

meson -D examples=false ..
if errorlevel 1 goto fail
ninja
if errorlevel 1 goto fail
ninja install
if errorlevel 1 goto fail

rem The pkgconfig prefix in file !InstallDir!\lib\pkgconfig\fuse3.pc
rem is erroneously set as: prefix=c:/
rem We should somehow set it to: prefix=${pcfiledir}/../..

exit /b 0

:fail
exit /b 1
