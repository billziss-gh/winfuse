/*
 * Description:
 *     On debug builds of the WinFuse driver, when the special file \$LOCKDLY is created/opened
 *     an artificial delay is introduced. This allows enough time for forcible termination of the
 *     file system (e.g. using Ctrl-C). If using non-alertable locks (kernel ERESOURCE) then the
 *     file system process should become unkillable and the lockdly.exe process should hang. If our
 *     custom implementation of alertable read-write locks (using semaphores and alertable waits)
 *     is used, then this should not happen.
 *
 * Compile:
 *     - cl lockdly.c
 *
 * Reproduce:
 *     - lockdly.exe
 *     - Quickly kill the file system process (e.g. switch to its command windows and Ctrl-C).
 *
 * Correct behavior:
 *     - Normal program termination.
 *
 * Incorrect behavior:
 *     - File system process and/or lockdly.exe process hang and become unkillable.
 */

#include <windows.h>

DWORD WINAPI ThreadProc(PVOID Param)
{
    HANDLE Handle = CreateFileW(
        L"$LOCKDLY",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
        0);
    if (INVALID_HANDLE_VALUE == Handle)
        return 1;
    
    return 0;
}

int wmain(int argc, wchar_t *argv[])
{
    HANDLE Thread = CreateThread(0, 0, ThreadProc, 0, 0, 0);
    if (0 == Thread)
        return 1;
    
    Sleep(1000);
    
    HANDLE Handle = CreateFileW(
        L"$LOCKDLY",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0,
        OPEN_EXISTING,
        0,
        0);
    if (INVALID_HANDLE_VALUE == Handle)
        return 1;

    WaitForSingleObject(Thread, INFINITE);

    DWORD ExitCode;
    GetExitCodeThread(Thread, &ExitCode);
    return ExitCode;
}
