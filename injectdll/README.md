# injectdll - DSO injection for Microsoft Windows

huku &lt;[huku@grhack.net](mailto:huku@grhack.net)&gt;


## About

**injectdll** is a tool, part of **injectdso**, that allows one to inject
arbitrary **.dll** files in running processes under Microsoft Windows.

This tool differs from similar ones of the public domain in the following 
aspects:

  * Doesn't assume that system DLLs like **kernel32.dll** have the same base
  address in all processes (however, this can change by using **-s**)

  * Waits for the remote thread to exit and return status information

  * Unloads the injected DLL from the target process using **FreeLibrary()** 
  thus also releasing any mandatory locks, so that the DLL can later be 
  re-injected. Saves valuable time when developing sandbox escapes ;)

  * Starts a pipe server so that a remote thread can talk back to **injectdll**
  (low integrity processes, like sandboxed children for example, are allowed to
  connect and send data)

**injectdll** was inspired from @tyranids work on IE 11 sandbox escapes.


## Compiling injectdll

Fire up **cmd.exe** and run the following **.bat** file (Visual Studio is
required):

```
Z:\injectdll>"C:\Program Files\Microsoft Visual Studio 10.0\VC\vcvarsall.bat"
Setting environment for using Microsoft Visual Studio 2010 x86 tools.
```

Now run **nmake** as shown below:

```
Z:\injectdll>nmake

Microsoft (R) Program Maintenance Utility Version 10.00.30319.01
Copyright (C) Microsoft Corporation.  All rights reserved.

        cl /nologo /O2 /Wall /wd4054 /wd4055 /wd4255 /wd4668 /wd4710 /wd4711 
        /wd4820 /wd4917 /MTd /GS /Zi /c main.c pipe.c
main.c
pipe.c
Generating Code...
        link main.obj pipe.obj /OUT:injectdll.exe /nologo /DEBUG /DYNAMICBASE 
        /NXCOMPAT /PDB:injectdll.pdb  /SAFESEH /WX /MACHINE:X86 
        /DEFAULTLIB:Psapi.lib /DEFAULTLIB:Shlwapi.lib
```

To build a 64-bit binary, use the following commands instead:

```
Z:\injectdll>"C:\Program Files\Microsoft Visual Studio 10.0\VC\vcvarsall.bat" amd64
Z:\injectdll>nmake /F Makefile.amd64
```


## Using injectdll

Fire up **calc.exe**:

```
Z:\injectdll>tasklist /FI "IMAGENAME eq calc.exe"

Image Name                     PID Session Name        Session#    Mem Usage
========================= ======== ================ =========== ============
calc.exe                      3628 Console                    1      9,332 K
```

Nothing stops you from injecting an executable in a remote task :)

```
Z:\injectdll>injectdll.exe C:\Windows\system32\cmd.exe 3628

injectdll v0.1 by huku <huku@grhack.net>

[*] Starting server at \\.\pipe\injectdll
[*] Module "kernel32.dll" is mapped at @0x758E0000 in PID 3628
[*] Module "kernel32.dll" is mapped at @0x758E0000
[*] Resolving API addresses
[*] My "LoadLibrary()" at @0x7592EF32
[*] My "FreeLibrary()" at @0x7592EF57
[*] Computing remote process API addresses
[*] Remote "LoadLibrary()" at @0x7592EF32
[*] Remote "FreeLibrary()" at @0x7592EF57
[*] Remote allocation at @0x00A90000
[*] DLL was injected at @0x4A020000
[*] Unloading injected DLL
[*] Waiting for the pipe server to terminate
[*] Server stopped
[*] Done
```


## Useful resources

  * [InjectDLL](https://github.com/tyranid/IE11SandboxEscapes/tree/master/InjectDll) 
  by [@tyranid](https://github.com/tyranid)

  * [DLL Injection with CreateRemoteThread](http://stackoverflow.com/questions/22750112/dll-injection-with-createremotethread)

  * [Low integrity to medium/high integrity pipe security descriptor](http://stackoverflow.com/questions/9589141/low-integrity-to-medium-high-integrity-pipe-security-descriptor)

For a description of the injection technique implemented in **injectdll** have
a look at **NOTES.md**.

