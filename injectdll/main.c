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


INT _tmain(INT argc, PTCHAR argv[])
{
    BOOLEAN bAssumeSameBase;
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

    INT ret = EXIT_FAILURE;


    _tprintf(_T("\n%s\n\n"), HAX0R_HEADER);

    if(argc == 4 && _tcscmp(argv[1], _T("-s")) == 0)
    {
        bAssumeSameBase = TRUE;
        lpFileName = argv[2];
        dwRemotePid = (DWORD)_ttoi(argv[3]);
    }
    else if(argc == 3)
    {
        bAssumeSameBase = FALSE;
        lpFileName = argv[1];
        dwRemotePid = (DWORD)_ttoi(argv[2]);
    }
    else
    {
        _tprintf(_T("Usage: %s [-s] <DLL> <PID>\n"), argv[0]);
        goto _exit;
    }

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
    hServerThread = startPipeServerThread(doneEvent);


    /* Determine load address of "kernel32.dll" in this process. */
    dwMyPid = GetCurrentProcessId();
    hMyProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwMyPid);
    lpMyKernel32Address = (LPVOID)GetModuleHandle(_T("kernel32.dll"));
    _tprintf(_T("[*] Module \"kernel32.dll\" is mapped at @0x%p\n"),
        lpMyKernel32Address);


    /* Determine load address of "kernel32.dll" in remote process and resolve
     * API addresses.
     */
    if(bAssumeSameBase)
    {
        lpTargetKernel32Address = lpMyKernel32Address;
    }
    else
    {
        lpTargetKernel32Address = (LPVOID)GetRemoteDLLHandle(hRemoteProcess,
            _T("kernel32.dll"));
        if(lpTargetKernel32Address == NULL)
        {
            _tprintf(_T("[*] Could not locate \"kernel32.dll\" in PID %d\n"),
                dwRemotePid);
            goto _exit;
        }
    }
    _tprintf(_T("[*] Module \"kernel32.dll\" is mapped at @0x%p in PID %d\n"),
        lpTargetKernel32Address, dwRemotePid);

    _tprintf(_T("[*] Resolving API addresses\n"));
    lpMyLoadLibraryAddress = (LPVOID)LoadLibrary;
    _tprintf(_T("[*] My \"LoadLibrary()\" at @0x%p\n"), lpMyLoadLibraryAddress);
    lpMyFreeLibraryAddress = (LPVOID)FreeLibrary;
    _tprintf(_T("[*] My \"FreeLibrary()\" at @0x%p\n"), lpMyFreeLibraryAddress);

    /* When `bSameBaseAddress' is true, the following computations reduce to 
     * simple assignments.
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

    /* Find out where was the DLL injected. */
    lpInjectedDllAddress = (LPVOID)GetRemoteDLLHandle(hRemoteProcess,
        szFileName);
    if(lpInjectedDllAddress == NULL)
    {
        _tprintf(_T("[*] Injected DLL was not found in process %u\n"),
            dwRemotePid);
        VirtualFreeEx(hRemoteProcess, lpMem, 0, MEM_RELEASE);
        CloseHandle(hRemoteProcess);
        goto _exit;
    }
    _tprintf(_T("[*] DLL was injected at @0x%p\n"), lpInjectedDllAddress);


    Sleep(5000);


    /* Now unload the DLL from the remote process. In fact, we reduce the DLL's
     * reference count until it reaches 0.
     */
    _tprintf(_T("[*] Unloading injected DLL\n"));
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


    VirtualFreeEx(hRemoteProcess, lpMem, 0, MEM_RELEASE);
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

