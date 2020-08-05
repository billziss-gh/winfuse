@echo off

setlocal
setlocal EnableDelayedExpansion

if not X%1==X set Config=%1
if not X%2==X set Arch=%2
if not X%3==X set Sample=%3
if not X%4==X set DestDir=%4

if X!Sample!==X (echo usage: build-sample Config Arch Sample [DestDir] >&2 & goto fail)

set SampleProj=
for /F "tokens=1,2 delims=*" %%k in ("!Sample!") do (
    set Sample=%%k
    set SampleProj=%%l
)

set SampleDir=%~dp0..\tst
if not exist "!SampleDir!\!Sample!" (echo sample !Sample! not found >&2 & goto fail)

if not X!DestDir!==X (
    if exist !DestDir! rmdir /s/q !DestDir!
    mkdir !DestDir!
    cd !DestDir!
    xcopy /s/e/q/y "!SampleDir!\!Sample!" .
) else (
    cd "!SampleDir!\!Sample!"
)

set vswhere="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`%vswhere% -find VC\**\vcvarsall.bat`) do (
    call "%%i" x64
)

if exist build rmdir /s/q build
if X!SampleProj!==X (
    devenv "!Sample!.sln" /build "!Config!|!Arch!"
) else (
    devenv "!Sample!.sln" /build "!Config!|!Arch!" /project "!SampleProj!"
)
if !ERRORLEVEL! neq 0 goto :fail

exit /b 0

:fail
exit /b 1
