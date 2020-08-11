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
    winfuse-tests-x86 ^
    wslfuse-tests
set opt_tests=^
    sample-build-memfs-fuse3-x64 ^
    sample-test0-memfs-fuse3-x64 ^
    sample-test1-memfs-fuse3-x64 ^
    sample-testinf-memfs-fuse3-x64 ^
    sample-fsx0-memfs-fuse3-x64 ^
    sample-fsx1-memfs-fuse3-x64 ^
    sample-fsxinf-memfs-fuse3-x64 ^
    sample-build-memfs-fuse3-x86 ^
    sample-test0-memfs-fuse3-x86 ^
    sample-test1-memfs-fuse3-x86 ^
    sample-testinf-memfs-fuse3-x86 ^
    sample-fsx0-memfs-fuse3-x86 ^
    sample-fsx1-memfs-fuse3-x86 ^
    sample-fsxinf-memfs-fuse3-x86 ^
    sample-build-memfs-fuse3-wsl

REM disable WSL tests that do not pass on AppVeyor (because it uses old Windows/WSL build)
REM sample-test0-memfs-fuse3-wsl ^
REM sample-test1-memfs-fuse3-wsl ^
REM sample-testinf-memfs-fuse3-wsl ^
REM sample-fsx0-memfs-fuse3-wsl ^
REM sample-fsx1-memfs-fuse3-wsl ^
REM sample-fsxinf-memfs-fuse3-wsl

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

:wslfuse-tests
wsl -- sudo mknod /dev/fuse c 10 229; sudo ./wslfuse-tests.out
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-build-memfs-fuse3-x64
call :__run_sample_build memfs-fuse3 memfs-fuse3.exe x64
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-build-memfs-fuse3-x86
call :__run_sample_build memfs-fuse3 memfs-fuse3.exe x86
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-build-memfs-fuse3-wsl
call :__run_sample_build memfs-fuse3 memfs-fuse3.out x64
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:__run_sample_build
call %ProjRoot%\tools\build-sample.bat %Configuration% %3 %1*%2 "%TMP%\%1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-test0-memfs-fuse3-x64
call :__run_sample_test memfs-fuse3 x64 0
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-test1-memfs-fuse3-x64
call :__run_sample_test memfs-fuse3 x64 1
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-testinf-memfs-fuse3-x64
call :__run_sample_test memfs-fuse3 x64 -1
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-test0-memfs-fuse3-x86
call :__run_sample_test memfs-fuse3 x86 0
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-test1-memfs-fuse3-x86
call :__run_sample_test memfs-fuse3 x86 1
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-testinf-memfs-fuse3-x86
call :__run_sample_test memfs-fuse3 x86 -1
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:__run_sample_test
set TestExit=0
start "" /b "%TMP%\%1\build\%Configuration%\%1-%2.exe" -oFileInfoTimeout=%3 L:
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
pushd >nul
cd L: >nul 2>nul || (echo Unable to find drive L: >&2 & goto fail)
L:
"C:\Program Files (x86)\WinFsp\bin\winfsp-tests-x64.exe" ^
    --external --resilient ^
    create_test create_related_test create_sd_test create_notraverse_test create_backup_test ^
    create_restore_test create_share_test create_curdir_test create_namelen_test ^
    getfileinfo_test delete_test delete_pending_test delete_mmap_test delete_standby_test ^
    rename_* ^
    getvolinfo_test ^
    getsecurity_test ^
    rdwr_* ^
    flush_* ^
    lock_* ^
    querydir_* ^
    dirnotify_test ^
    exec_*
if !ERRORLEVEL! neq 0 set TestExit=1
popd
taskkill /f /im %1-%2.exe
exit /b !TestExit!

:sample-fsx0-memfs-fuse3-x64
call :__run_sample_fsx_test memfs-fuse3 x64 0
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-fsx1-memfs-fuse3-x64
call :__run_sample_fsx_test memfs-fuse3 x64 1
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-fsxinf-memfs-fuse3-x64
call :__run_sample_fsx_test memfs-fuse3 x64 -1
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-fsx0-memfs-fuse3-x86
call :__run_sample_fsx_test memfs-fuse3 x86 0
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-fsx1-memfs-fuse3-x86
call :__run_sample_fsx_test memfs-fuse3 x86 1
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-fsxinf-memfs-fuse3-x86
call :__run_sample_fsx_test memfs-fuse3 x86 -1
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:__run_sample_fsx_test
set TestExit=0
start "" /b "%TMP%\%1\build\%Configuration%\%1-%2.exe" -oFileInfoTimeout=%3 L:
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
pushd >nul
cd L: >nul 2>nul || (echo Unable to find drive L: >&2 & goto fail)
L:
"%ProjRoot%\ext\winfsp\ext\test\fstools\src\fsx\fsx.exe" -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 set TestExit=1
popd
taskkill /f /im %1-%2.exe
exit /b !TestExit!

:sample-test0-memfs-fuse3-wsl
call :__run_sample_wsl_test memfs-fuse3 0
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-test1-memfs-fuse3-wsl
call :__run_sample_wsl_test memfs-fuse3 1
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-testinf-memfs-fuse3-wsl
call :__run_sample_wsl_test memfs-fuse3 -1
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:__run_sample_wsl_test
set TestExit=0
start "" /b /d "%TMP%\%1\build\%Configuration%" wsl -- sudo sh "/mnt/c/Program Files/WinFuse/opt/wslfuse/install.sh"; mkdir -p mnt; ./%1.out -f -ocontext=FileInfoTimeout=%2,context=Volume=L: mnt
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 10 2>nul
pushd >nul
cd L: >nul 2>nul || (echo Unable to find drive L: >&2 & goto fail)
L:
"C:\Program Files (x86)\WinFsp\bin\winfsp-tests-x64.exe" ^
    --external --resilient ^
    create_test create_related_test create_sd_test create_notraverse_test create_backup_test ^
    create_restore_test create_share_test create_curdir_test create_namelen_test ^
    getfileinfo_test delete_test delete_pending_test delete_mmap_test delete_standby_test ^
    rename_* ^
    getvolinfo_test ^
    getsecurity_test ^
    rdwr_* ^
    flush_* ^
    lock_* ^
    querydir_* ^
    dirnotify_test ^
    exec_*
if !ERRORLEVEL! neq 0 set TestExit=1
popd
start "" /b /d "%TMP%\%1\build\%Configuration%" wsl -- fusermount3 -u mnt
exit /b !TestExit!

:sample-fsx0-memfs-fuse3-wsl
call :__run_sample_fsx_wsl_test memfs-fuse3 0
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-fsx1-memfs-fuse3-wsl
call :__run_sample_fsx_wsl_test memfs-fuse3 1
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-fsxinf-memfs-fuse3-wsl
call :__run_sample_fsx_wsl_test memfs-fuse3 -1
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:__run_sample_fsx_wsl_test
set TestExit=0
start "" /b /d "%TMP%\%1\build\%Configuration%" wsl -- sudo sh "/mnt/c/Program Files/WinFuse/opt/wslfuse/install.sh"; mkdir -p mnt; ./%1.out -f -ocontext=FileInfoTimeout=%2,context=Volume=L: mnt
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 10 2>nul
pushd >nul
cd L: >nul 2>nul || (echo Unable to find drive L: >&2 & goto fail)
L:
"%ProjRoot%\ext\winfsp\ext\test\fstools\src\fsx\fsx.exe" -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 set TestExit=1
popd
start "" /b /d "%TMP%\%1\build\%Configuration%" wsl -- fusermount3 -u mnt
exit /b !TestExit!

:leak-test
rem wait a bit to avoid reporting lingering allocations
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
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
