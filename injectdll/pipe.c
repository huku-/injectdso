/* injectdll - DSO injection for Microsoft Windows
 * huku <huku@grhack.net>
 *
 * pipe.c - Mini named pipe server used to receive data from a remote process. 
 */
#include "includes.h"
#include "pipe.h"


/* Create a DACL that will allow everyone to have full control over our pipe. */
static VOID BuildDACL(PSECURITY_DESCRIPTOR pDescriptor)
{
    PSID pSid;
    EXPLICIT_ACCESS ea;
    PACL pAcl;

    SID_IDENTIFIER_AUTHORITY sia = SECURITY_WORLD_SID_AUTHORITY;

    AllocateAndInitializeSid(&sia, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0,
        &pSid);

    ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));
    ea.grfAccessPermissions = FILE_ALL_ACCESS;
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance = NO_INHERITANCE;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea.Trustee.ptstrName = (LPTSTR)pSid;

    if(SetEntriesInAcl(1, &ea, NULL, &pAcl) == ERROR_SUCCESS)
    {
        if(SetSecurityDescriptorDacl(pDescriptor, TRUE, pAcl, FALSE) == 0)
            _tprintf(_T("[*] Failed to set DACL (%u)\n"), GetLastError());
    }
    else
        _tprintf(_T("[*] Failed to add ACE in DACL (%u)\n"), GetLastError());
}


/* Create a SACL that will allow low integrity processes connect to our pipe. */
static VOID BuildSACL(PSECURITY_DESCRIPTOR pDescriptor)
{
    PSID pSid;
    PACL pAcl;

    SID_IDENTIFIER_AUTHORITY sia = SECURITY_MANDATORY_LABEL_AUTHORITY;
    DWORD dwACLSize = sizeof(ACL) + sizeof(SYSTEM_MANDATORY_LABEL_ACE) +
        GetSidLengthRequired(1);

    pAcl = (PACL)LocalAlloc(LPTR, dwACLSize);
    InitializeAcl(pAcl, dwACLSize, ACL_REVISION);

    AllocateAndInitializeSid(&sia, 1, SECURITY_MANDATORY_LOW_RID, 0, 0, 0, 0,
        0, 0, 0, &pSid);

    if(AddMandatoryAce(pAcl, ACL_REVISION, 0, SYSTEM_MANDATORY_LABEL_NO_WRITE_UP,
            pSid) == TRUE)
    {
        if(SetSecurityDescriptorSacl(pDescriptor, TRUE, pAcl, FALSE) == 0)
            _tprintf(_T("[*] Failed to set SACL (%u)\n"), GetLastError());
    }
    else
        _tprintf(_T("[*] Failed to add ACE in SACL (%u)\n"), GetLastError());
}


/* Initialize security attributes to be used by `CreateNamedPipe()' below. */
static VOID InitSecurityAttributes(PSECURITY_ATTRIBUTES pAttributes)
{
    PSECURITY_DESCRIPTOR pDescriptor;

    pDescriptor = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR,
        SECURITY_DESCRIPTOR_MIN_LENGTH);
    InitializeSecurityDescriptor(pDescriptor, SECURITY_DESCRIPTOR_REVISION);

    BuildDACL(pDescriptor);
    BuildSACL(pDescriptor);

    pAttributes->nLength = sizeof(SECURITY_ATTRIBUTES);
    pAttributes->lpSecurityDescriptor = pDescriptor;
    pAttributes->bInheritHandle = TRUE;
}


/* Fire up the pipe server and wait for connections. */
DWORD WINAPI StartPipeServer(LPVOID lpThreadParameter)
{
    SECURITY_ATTRIBUTES sa;
    HANDLE hPipe;
    DWORD dwRead, error;
    COMMTIMEOUTS timeouts;
    TCHAR szBuffer[BUFSIZ + 1];

    HANDLE evt = (HANDLE)lpThreadParameter;
    DWORD dwModeNoWait = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_NOWAIT;
    DWORD dwModeWait = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT;

    DWORD ret = EXIT_FAILURE;


    _tprintf(_T("[*] Starting server at %s\n"), PIPE_NAME);


    InitSecurityAttributes(&sa);

    hPipe = CreateNamedPipe(PIPE_NAME, PIPE_ACCESS_DUPLEX, dwModeNoWait,
        PIPE_UNLIMITED_INSTANCES, PAGE_SIZE, PAGE_SIZE, INFINITE, &sa);
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

            /* XXX: `select()' style interactive I/O maybe? */
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
    ret = EXIT_SUCCESS;

_exit:
    return ret;
}

/* Create a thread to handle the pipe server. Event `evt' is used to signal the
 * server to stop listening. 
 */
HANDLE StartPipeServerThread(HANDLE evt)
{
    return CreateThread(NULL, PAGE_SIZE, StartPipeServer, evt, 0, NULL);
}

