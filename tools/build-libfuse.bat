@echo off

setlocal
setlocal EnableDelayedExpansion

if not X%1==X set DESTDIR=%1

if not X!DESTDIR!==X (
    if exist !DESTDIR! rmdir /s/q !DESTDIR!
    mkdir !DESTDIR!
    pushd !DESTDIR!
    set DESTDIR=!cd!
    popd
) else (
    set DESTDIR=%~dp0..\build\VStudio\build\libfuse
    if exist !DESTDIR! rmdir /s/q !DESTDIR!
    mkdir !DESTDIR!
)

set vswhere="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`%vswhere% -find VC\**\vcvarsall.bat`) do (
    call "%%i" x64
)

rem workaround getting Cygwin's meson/ninja
set PATH=C:\Program Files\Meson;%PATH%

cd %~dp0..\ext\libfuse
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
