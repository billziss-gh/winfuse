@echo off

setlocal
setlocal EnableDelayedExpansion

if not X%1==X set Config=%1
if not X%2==X set DestDir=%2

if X!Config!==X (echo usage: build-winfsp Config [DestDir] >&2 & goto fail)

if not X!DestDir!==X (
    if exist !DestDir! rmdir /s/q !DestDir!
    mkdir !DestDir!
    pushd !DestDir!
    set DestDir=!cd!
    popd
)

set vswhere="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`%vswhere% -find VC\**\vcvarsall.bat`) do (
    call "%%i" x64
)

cd %~dp0..
set VS140COMNTOOLS=
call ext\winfsp\tools\build.bat %Config%
if !ERRORLEVEL! neq 0 goto :fail

if not X!DestDir!==X (
    copy %~dp0..\ext\winfsp\build\VStudio\build\%Config%\winfsp-*.msi !DestDir!
    copy %~dp0..\ext\winfsp\build\VStudio\build\%Config%\winfsp-tests-x64.exe !DestDir!
)

exit /b 0

:fail
exit /b 1
