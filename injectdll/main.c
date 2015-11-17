/* injectdll - DSO injection for Microsoft Windows
 * huku <huku@grhack.net>
 *
 * main.c - Holds the program entry point.
 */
#define _CRT_SECURE_NO_WARNINGS

#include "includes.h"
#include "pipe.h"

#define HAX0R_HEADER      _T("injectdll v0.1 by huku <huku@grhack.net>")
#define MAX_MODULE_COUNT  1024


/* Given a process handle and a module name, return an `HMODULE' handle to the
 * module in question.
 */
HMODULE GetRemoteDLLHandle(HANDLE hProcess, LPTSTR szModuleName)
{
    HMODULE hModule[MAX_MODULE_COUNT];
    DWORD cbNeeded, i;
    TCHAR szModulePath[MAX_PATH + 1], szRemoteName[MAX_PATH + 1];
    HMODULE ret = (HMODULE)NULL;

    /* If `szModuleName' holds a path, canonicalize it as it may be relative. */
    if(PathIsFileSpec(szModuleName) == FALSE && PathIsRelative(szModuleName))
    {
        ZeroMemory(szModulePath, sizeof(szModulePath));
        GetFullPathName(szModuleName, MAX_PATH, szModulePath, NULL);
        StringCchCopy(szModuleName, MAX_PATH, szModulePath);
    }

    cbNeeded = MAX_MODULE_COUNT * sizeof(HMODULE);
    ZeroMemory(hModule, cbNeeded);
    if(EnumProcessModulesEx(hProcess, hModule, cbNeeded, &cbNeeded,
            LIST_MODULES_ALL) == 0)
    {
        /* This error code is usually returned when trying to inject a DLL from
         * a 32bit process to a 64bit process and vice versa. Make sure the
         * "injectdll.exe" binary and the target process have the same bitness.
         */
        if(GetLastError() == ERROR_PARTIAL_COPY)
            _tprintf(_T("[*] InjectDLL and target process differ in bitness\n"));
        else
            _tprintf(_T("[*] Failed to enumerate process modules (%u)\n"),
                GetLastError());

        goto _exit;
    }

    for(i = 0; i < cbNeeded / sizeof(HMODULE); i++)
    {
        /* If `szModuleName' is a path, call `GetModuleFileNameEx()', otherwise
         * use `GetModuleBaseName()' and match the base name only.
         */
        ZeroMemory(szRemoteName, sizeof(szRemoteName));
        if(PathIsFileSpec(szModuleName) == FALSE)
            GetModuleFileNameEx(hProcess, hModule[i], szRemoteName, MAX_PATH);
        else
            GetModuleBaseName(hProcess, hModule[i], szRemoteName, MAX_PATH);

        if(StrCmpI(szRemoteName, szModuleName) == 0)
        {
            ret = hModule[i];
            break;
        }
    }

_exit:
    return ret;
}


VOID ShowUsage(PTSTR pszProgramName)
{
    _tprintf(_T("%s [-siu] <DLL> <PID>\n\n"), pszProgramName);
    _tprintf(_T("    -s    Don't resolve kernel32.dll address\n"));
    _tprintf(_T("    -i    Don't inject DLL\n"));
    _tprintf(_T("    -u    Don't unload injected DLL\n\n"));
}


INT _tmain(INT argc, PTCHAR argv[])
{
    DWORD dwRemotePid, dwMyPid;
    HANDLE hRemoteProcess, hMyProcess;
    HANDLE hRemoteThread;
    HANDLE hServerThread;
    HANDLE doneEvent;
    LPVOID lpTargetKernel32Address, lpTargetLoadLibraryAddress,
        lpTargetFreeLibraryAddress;
    LPVOID lpMyKernel32Address, lpMyLoadLibraryAddress,
        lpMyFreeLibraryAddress;
    LPVOID lpInjectedDllAddress;
    LPVOID lpMem;
    TCHAR szFileName[MAX_PATH + 1];
    PTCHAR lpFileName;
    SIZE_T cbWritten, cbToWrite;
    INT i;

    BOOLEAN bDontResolve = FALSE, bDontInject = FALSE, bDontUnload = FALSE;
    INT ret = EXIT_FAILURE;


    _tprintf(_T("\n%s\n\n"), HAX0R_HEADER);


    for(i = 1; i < argc && argv[i][0] == '-'; i++)
    {
        switch(argv[i][1])
        {
            case 's':
                bDontResolve = TRUE;
                break;
            case 'i':
                bDontInject = TRUE;
                break;
            case 'u':
                bDontUnload = TRUE;
                break;
            default:
                ShowUsage(argv[0]);
                goto _exit;
        }
    }

    if(i > argc - 2)
    {
        ShowUsage(argv[0]);
        goto _exit;
    }

    lpFileName = argv[i];
    dwRemotePid = (DWORD)_ttoi(argv[i + 1]);


    /* Make sure the DLL we were asked to load is there. */
    GetFullPathName(lpFileName, MAX_PATH, szFileName, NULL);
    if(PathFileExists(szFileName) == FALSE)
    {
        _tprintf(_T("[*] DLL \"%s\" not found\n"), szFileName);
        goto _exit;
    }

    /* Open handle to remote process. */
    hRemoteProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwRemotePid);
    if(hRemoteProcess == NULL)
    {
        _tprintf(_T("[*] Failed to open process %u (%u)\n"), dwRemotePid,
            GetLastError());
        goto _exit;
    }


    /* Start the mini pipe server in a new thread. */
    doneEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    hServerThread = StartPipeServerThread(doneEvent);


    /* Determine load address of "kernel32.dll" in this process. */
    dwMyPid = GetCurrentProcessId();
    hMyProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwMyPid);
    lpMyKernel32Address = (LPVOID)GetModuleHandle(_T("kernel32.dll"));
    _tprintf(_T("[*] Module \"kernel32.dll\" is mapped at @0x%p\n"),
        lpMyKernel32Address);


    /* Determine load address of "kernel32.dll" in remote process and resolve
     * API addresses. If `bDontResolve' was set, assume "kernel32.dll" has the
     * same base address in the remote task.
     */
    if(bDontResolve == FALSE)
    {
        if((lpTargetKernel32Address = (LPVOID)GetRemoteDLLHandle(hRemoteProcess,
            _T("kernel32.dll"))) == NULL)
        {
            _tprintf(_T("[*] Could not locate \"kernel32.dll\" in PID %d\n"),
                dwRemotePid);
            goto _exit;
        }
        _tprintf(_T("[*] Module \"kernel32.dll\" is mapped at @0x%p in PID %d\n"),
            lpTargetKernel32Address, dwRemotePid);
    }
    else
        lpTargetKernel32Address = lpMyKernel32Address;

    _tprintf(_T("[*] Resolving API addresses\n"));
    lpMyLoadLibraryAddress = (LPVOID)LoadLibrary;
    _tprintf(_T("[*] My \"LoadLibrary()\" at @0x%p\n"), lpMyLoadLibraryAddress);
    lpMyFreeLibraryAddress = (LPVOID)FreeLibrary;
    _tprintf(_T("[*] My \"FreeLibrary()\" at @0x%p\n"), lpMyFreeLibraryAddress);

    /* When `bDontResolve' is true, the following computations reduce to simple
     * assignments.
     */
    _tprintf(_T("[*] Computing remote process API addresses\n"));
    lpTargetLoadLibraryAddress = (PBYTE)lpTargetKernel32Address +
        ((PBYTE)lpMyLoadLibraryAddress - (PBYTE)lpMyKernel32Address);
    _tprintf(_T("[*] Remote \"LoadLibrary()\" at @0x%p\n"),
        lpTargetLoadLibraryAddress);
    lpTargetFreeLibraryAddress = (PBYTE)lpTargetKernel32Address +
        ((PBYTE)lpMyFreeLibraryAddress - (PBYTE)lpMyKernel32Address);
    _tprintf(_T("[*] Remote \"FreeLibrary()\" at @0x%p\n"),
        lpTargetFreeLibraryAddress);


    /* If `bDontInject' was set, don't inject the specified DLL. */
    if(bDontInject == FALSE)
    {
        /* Perform a remote allocation and copy the DLL name in it. */
        lpMem = VirtualAllocEx(hRemoteProcess, NULL, PAGE_SIZE, MEM_COMMIT,
                PAGE_READWRITE);
        if(lpMem == NULL)
        {
            _tprintf(_T("[*] Remote allocation failed (%u)\n"), GetLastError());
            CloseHandle(hRemoteProcess);
            goto _exit;
        }

        _tprintf(_T("[*] Remote allocation at @0x%p\n"), lpMem);

        cbToWrite = (_tcslen(szFileName) + 1) * sizeof(TCHAR);
        WriteProcessMemory(hRemoteProcess, lpMem, szFileName, cbToWrite,
            &cbWritten);
        if(cbWritten != cbToWrite)
        {
            _tprintf(_T("[*] Writing process %u memory failed (%u)\n"),
                dwRemotePid, GetLastError());
            VirtualFreeEx(hRemoteProcess, lpMem, 0, MEM_RELEASE);
            CloseHandle(hRemoteProcess);
            goto _exit;
        }

        /* We're now ready to inject the DLL. */
        hRemoteThread = CreateRemoteThread(hRemoteProcess, NULL, PAGE_SIZE,
            (LPTHREAD_START_ROUTINE)lpTargetLoadLibraryAddress, lpMem, 0, NULL);
        WaitForSingleObject(hRemoteThread, INFINITE);
        CloseHandle(hRemoteThread);

        /* Thread finished, free the remote allocation. */
        VirtualFreeEx(hRemoteProcess, lpMem, 0, MEM_RELEASE);
    }


    /* Find out where was the DLL injected. */
    lpInjectedDllAddress = (LPVOID)GetRemoteDLLHandle(hRemoteProcess, szFileName);
    if(lpInjectedDllAddress == NULL)
    {
        _tprintf(_T("[*] Injected DLL was not found in process %u\n"),
            dwRemotePid);
        CloseHandle(hRemoteProcess);
        goto _exit;
    }
    _tprintf(_T("[*] DLL was injected at @0x%p\n"), lpInjectedDllAddress);


    /* Unload the DLL unless the user has requested not to do so. */
    if(bDontUnload == FALSE)
    {
        _tprintf(_T("[*] Unloading injected DLL\n"));
        Sleep(5000);

        /* Now unload the DLL from the remote process. In fact, we reduce the DLL's
         * reference count until it reaches 0.
         */
        while(ReadProcessMemory(hRemoteProcess, lpInjectedDllAddress,
                (LPVOID)&cbToWrite, 1, NULL))
        {
            hRemoteThread = CreateRemoteThread(hRemoteProcess, NULL, PAGE_SIZE,
                (LPTHREAD_START_ROUTINE)lpTargetFreeLibraryAddress,
                lpInjectedDllAddress, 0, NULL);
            WaitForSingleObject(hRemoteThread, INFINITE);
            CloseHandle(hRemoteThread);
            Sleep(1000);
        }
    }

    CloseHandle(hRemoteProcess);
    CloseHandle(hMyProcess);

    _tprintf(_T("[*] Waiting for the pipe server to terminate\n"));
    SetEvent(doneEvent);
    WaitForSingleObject(hServerThread, INFINITE);
    CloseHandle(hServerThread);

    _tprintf(_T("[*] Done\n"));
    ret = EXIT_SUCCESS;

_exit:
    return ret;
}

