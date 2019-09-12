@echo off

setlocal
setlocal EnableDelayedExpansion

set Configuration=Release

if not X%1==X set Configuration=%1

set vswhere="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`%vswhere% -find VC\**\vcvarsall.bat`) do (
    call "%%i" x64
)

cd %~dp0..\build\VStudio

if exist build\ for /R build\ %%d in (%Configuration%) do (
    if exist "%%d" rmdir /s/q "%%d"
)

devenv winfuse.sln /build "%Configuration%|x64"
if errorlevel 1 goto fail

exit /b 0

:fail
exit /b 1
