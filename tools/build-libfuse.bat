@echo off

setlocal
setlocal EnableDelayedExpansion

if not X%1==X set Config=%1
if not X%2==X set Arch=%2
if not X%3==X set DESTDIR=%3

if X!Config!==X (echo usage: build-libfuse Config Arch [DestDir] >&2 & goto fail)

if X!Config!==XDebug set Buildtype=debug
if X!Config!==XRelease set Buildtype=release

rem ninja uses DESTDIR for installation
if not X!DESTDIR!==X (
    if exist !DESTDIR! rmdir /s/q !DESTDIR!
    mkdir !DESTDIR!
    pushd !DESTDIR!
    set DESTDIR=!cd!
    popd
)

set vswhere="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`%vswhere% -find VC\**\vcvarsall.bat`) do (
    call "%%i" !Arch!
)

rem workaround getting Cygwin's meson/ninja
set PATH=C:\Program Files\Meson;%PATH%

cd %~dp0..\ext\libfuse
if exist build\!Config!\!Arch! rmdir /s/q build\!Config!\!Arch!
mkdir build\!Config!\!Arch! && cd build\!Config!\!Arch!

meson --buildtype=!Buildtype! -D examples=false ..\..\..
if errorlevel 1 goto fail
ninja
if errorlevel 1 goto fail
if not X!DestDir!==X (
    ninja install
    if errorlevel 1 goto fail
)

rem The pkgconfig prefix in file !InstallDir!\lib\pkgconfig\fuse3.pc
rem is erroneously set as: prefix=c:/
rem We should somehow set it to: prefix=${pcfiledir}/../..

exit /b 0

:fail
exit /b 1
