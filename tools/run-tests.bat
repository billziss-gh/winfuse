@echo off

setlocal
setlocal EnableDelayedExpansion

set Configuration=Release
if not X%1==X set Configuration=%1

cd %~dp0..
set ProjRoot=%cd%

cd build\VStudio
if not exist build\%Configuration% echo === No tests found >&2 & goto fail
cd build\%Configuration%

set dfl_tests=^
    winfuse-tests-x64 ^
    winfuse-tests-x86
set opt_tests=
    sample-memfs-fuse3-x64 ^
    sample-memfs-fuse3-x86

set tests=
for %%f in (%dfl_tests%) do (
    if X%2==X (
        set tests=!tests! %%f
    ) else (
        set test=%%f
        if not "X!test:%2=!"=="X!test!" set tests=!tests! %%f
    )
)
for %%f in (%opt_tests%) do (
    if X%2==X (
        rem
    ) else (
        set test=%%f
        if not "X!test:%2=!"=="X!test!" set tests=!tests! %%f
    )
)

set testpass=0
set testfail=0
for %%f in (%tests%) do (
    echo === Running %%f

    if defined APPVEYOR (
        appveyor AddTest "%%f" -FileName None -Framework None -Outcome Running
    )

    pushd %cd%
    call :%%f
    popd

    if !ERRORLEVEL! neq 0 (
        set /a testfail=testfail+1

        echo === Failed %%f

        if defined APPVEYOR (
            appveyor UpdateTest "%%f" -FileName None -Framework None -Outcome Failed -Duration 0
        )
    ) else (
        set /a testpass=testpass+1

        echo === Passed %%f

        if defined APPVEYOR (
            appveyor UpdateTest "%%f" -FileName None -Framework None -Outcome Passed -Duration 0
        )
    )
    echo:
)

set /a total=testpass+testfail
echo === Total: %testpass%/%total%
call :leak-test
if !ERRORLEVEL! neq 0 goto fail
if not %testfail%==0 goto fail

exit /b 0

:fail
exit /b 1

:winfuse-tests-x64
winfuse-tests-x64 +*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfuse-tests-x86
winfuse-tests-x86 +*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-memfs-fuse3-x64
call :__run_sample_test memfs-fuse3 x64
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-memfs-fuse3-x86
call :__run_sample_test memfs-fuse3 x86
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:__run_sample_test
set TestExit=0
call %ProjRoot%\tools\build-sample.bat %Configuration% %1 %2 "%TMP%\%1"
if !ERRORLEVEL! neq 0 goto fail
start "" /b "%TMP%\%1\build\%Configuration%\%1-%2.exe" L:
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
pushd >nul
cd L: >nul 2>nul || (echo Unable to find drive L: >&2 & goto fail)
L:
"%ProjRoot%\ext\winfsp\build\VStudio\build\%Configuration%\winfsp-tests-x64.exe" ^
    --external --resilient
if !ERRORLEVEL! neq 0 set TestExit=1
popd
taskkill /f /im %1-%2.exe
rmdir /s/q "%TMP%\%1"
exit /b !TestExit!

:leak-test
for /F "tokens=1,2 delims=:" %%i in ('verifier /query ^| findstr ^
    /c:"Current Pool Allocations:" ^
    /c:"CurrentPagedPoolAllocations:" ^
    /c:"CurrentNonPagedPoolAllocations:"'
    ) do (

    set FieldName=%%i
    set FieldName=!FieldName: =!

    set FieldValue=%%j
    set FieldValue=!FieldValue: =!
    set FieldValue=!FieldValue:^(=!
    set FieldValue=!FieldValue:^)=!

    if X!FieldName!==XCurrentPoolAllocations (
        for /F "tokens=1,2 delims=/" %%k in ("!FieldValue!") do (
            set NonPagedAlloc=%%k
            set PagedAlloc=%%l
        )
    ) else if X!FieldName!==XCurrentPagedPoolAllocations (
        set PagedAlloc=!FieldValue!
    ) else if X!FieldName!==XCurrentNonPagedPoolAllocations (
        set NonPagedAlloc=!FieldValue!
    )
)
set /A TotalAlloc=PagedAlloc+NonPagedAlloc
if !TotalAlloc! equ 0 (
    echo === Leaks: None
) else (
    echo === Leaks: !NonPagedAlloc! NP / !PagedAlloc! P
    goto fail
)
exit /b 0
