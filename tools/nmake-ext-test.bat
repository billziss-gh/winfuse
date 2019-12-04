@echo off

setlocal
setlocal EnableDelayedExpansion

set vswhere="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`%vswhere% -find VC\**\vcvarsall.bat`) do (
    call "%%i" x64
)

cd %~dp0..\ext\winfsp\ext\test
nmake /f Nmakefile
