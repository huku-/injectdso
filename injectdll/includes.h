#ifndef _INCLUDES_H_
#define _INCLUDES_H_

#include <stdio.h>

/* `UNICODE' needs to be defined for the Windows API headers. */
#define UNICODE

#include <Windows.h>
#include <Psapi.h>
#include <Shlwapi.h>

/* `_UNICODE' needs to be defined for "tchar.h" only. */
#define _UNICODE
#include <tchar.h>

/* "Strsafe.h" needs to be included after "tchar.h". */
#include <Strsafe.h>

#define PAGE_SIZE 4096

#endif /* _INCLUDES_H_ */

