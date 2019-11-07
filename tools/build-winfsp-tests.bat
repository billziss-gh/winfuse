@echo off

setlocal
setlocal EnableDelayedExpansion

if not X%1==X set DestDir=%1

if not X!DestDir!==X (
    if exist !DestDir! rmdir /s/q !DestDir!
    mkdir !DestDir!
    pushd !DestDir!
    set DestDir=!cd!
    popd
) else (
    set DestDir=%~dp0..\build\VStudio\build\winfsp-tests
    if exist !DestDir! rmdir /s/q !DestDir!
    mkdir !DestDir!
)

set vswhere="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`%vswhere% -find VC\**\vcvarsall.bat`) do (
    call "%%i" x64
)

cd %~dp0..\ext\winfsp\build\VStudio
if exist build rmdir /s/q build

devenv winfsp.sln /build "Release|x64"
if errorlevel 1 goto fail

copy build\Release\winfsp-tests-x64.exe !DestDir!
copy build\Release\winfsp-x64.dll !DestDir!

exit /b 0

:fail
exit /b 1
