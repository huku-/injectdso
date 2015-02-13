/* injectdll - DSO injection for Microsoft Windows
 * huku <huku@grhack.net>
 *
 * pipe.c - Mini named pipe server used to receive data from a remote process. 
 */
#include "includes.h"
#include "pipe.h"


/* Fire up the pipe server and wait for connections. */
DWORD WINAPI startPipeServer(LPVOID lpThreadParameter)
{
    HANDLE hPipe;
    DWORD dwRead, error;
    COMMTIMEOUTS timeouts;
    TCHAR szBuffer[BUFSIZ + 1];

    HANDLE evt = (HANDLE)lpThreadParameter;
    DWORD dwModeNoWait = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_NOWAIT;
    DWORD dwModeWait = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT;

    _tprintf(_T("[*] Starting server at %s\n"), PIPE_NAME);

    hPipe = CreateNamedPipe(PIPE_NAME, PIPE_ACCESS_DUPLEX, dwModeNoWait,
        PIPE_UNLIMITED_INSTANCES, PAGE_SIZE, PAGE_SIZE, INFINITE, NULL);
    if(hPipe == INVALID_HANDLE_VALUE)
    {
        _tprintf(_T("[*] Failed to create named pipe (%u)\n"), GetLastError());
        goto _exit;
    }

    ZeroMemory(&timeouts, sizeof(COMMTIMEOUTS));
    timeouts.ReadTotalTimeoutConstant = 1000;
    SetCommTimeouts(hPipe, &timeouts);

    while(WaitForSingleObject(evt, 0) != WAIT_OBJECT_0)
    {
        ConnectNamedPipe(hPipe, NULL);
        if(GetLastError() == ERROR_PIPE_CONNECTED)
        {
            _tprintf(_T("[*] Client connected\n"));
            SetNamedPipeHandleState(hPipe, &dwModeWait, NULL, NULL);

            error = ERROR_SUCCESS;
            while(error != ERROR_BROKEN_PIPE)
            {
                dwRead = 0;
                ZeroMemory(szBuffer, sizeof(szBuffer));
                ReadFile(hPipe, szBuffer, BUFSIZ, &dwRead, NULL);
               
                if(dwRead != 0)
                    _tprintf(_T(">>> %s"), szBuffer);
                error = GetLastError();
            }
            _tprintf(_T("[*] Client disconnected\n"));

            SetNamedPipeHandleState(hPipe, &dwModeNoWait, NULL, NULL);
            DisconnectNamedPipe(hPipe);
        }
    }

    _tprintf(_T("[*] Server stopped\n"));

    CloseHandle(hPipe);
_exit:

    return 0;
}

/* Create a thread to handle the pipe server. Event `evt' is used to signal the
 * server to stop listening. 
 */
HANDLE startPipeServerThread(HANDLE evt)
{
    return CreateThread(NULL, PAGE_SIZE, startPipeServer, evt, 0, NULL);
}

