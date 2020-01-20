@echo off

setlocal
setlocal EnableDelayedExpansion

if not X%1==X set Config=%1
if not X%2==X set DestDir=%2

if X!Config!==X (echo usage: build-lxdk Config [DestDir] >&2 & goto fail)

if not X!DestDir!==X (
    if exist !DestDir! rmdir /s/q !DestDir!
    mkdir !DestDir!
    pushd !DestDir!
    set DestDir=!cd!
    popd
)

cd %~dp0..
call ext\lxdk\tools\build.bat %Config%
if !ERRORLEVEL! neq 0 goto :fail

if not X!DestDir!==X (
    copy %~dp0..\ext\lxdk\build\VStudio\build\%Config%\lxdk-*.msi !DestDir!
)

exit /b 0

:fail
exit /b 1
