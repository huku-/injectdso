# Notes

Scraps of notes on **injectdylib** development.

huku &lt;[huku@grhack.net](mailto:huku@grhack.net)&gt;


## Injection technique overview

**injectdylib** is very similar to the public DLL injection tools for Microsoft
Windows. It actually implements the MacOS X equivalent of the standard
**VirtualAllocEx()** + **CreateRemoteThread()** technique. Briefly, it allocates
memory on a remote task, fills it with the required parameters, initializes a
stack segment and spawns a new thread that calls **dlopen()**. However, the
aforementioned steps are not as straightforward as on Microsoft Windows and some
tricks need to be employed.

To begin with, **injectdylib** calls **vm_allocate()** to allocate memory in the
remote task's address space. The use of this memory region is twofold; it acts
both as the new thread's stack segment and as a temporary store for parameters
that will be passed to library calls made by the new thread (e.g. **dlopen()**
arguments).

Next, **thread_create()** is issued, followed by **thread_set_state()**
and **thread_resume()**. This creates a new thread and sets it to execute
arbitrary code (see **stubs/stubs.h**). We name this thread *thread #1*.

Threads created via the Mach API, like thread #1 above, are **kernel
threads**; they are not **POSIX** compatible threads like those spawned by
**pthread_create()**. The main difference is that the first don't have TSD
(Thread Specific Data) associated with them while the latter do (have a look at
**libpthread**'s [01] source code for more information). Practically, thread #1
cannot execute code like **dlopen()**, since most useful library calls will
segfault trying to access TSD data which is not there.

One option to avoid this problem is to initialize the TSD by hand as expected
by the **libc** runtime, but this is a lot of work and, most importantly,
it's not portable across MacOS X versions. To bypass this limitation,
**injectdylib** forces thread #1 to directly call **pthread_create()** so
that a **POSIX** compatible thread is immediatelly spawned (we call this
thread *thread #2*). Thread #2 calls **dlopen()** to load a user specified
**.dylib** file and terminates normally via a call to **pthread_exit()**.

Once **pthread_create()** is called, thread #1 enters a simple loop waiting for
**injectdylib** to kill it via **thread_terminate()**.


## Resolving symbols in remote tasks

Older versions of **injectdylib** included a simple parser for Mach-O/FAT
binaries. The parser was used to lookup symbols and calculate their relative
offsets from the beginning of the text segment. Relative offsets were later
used to compute symbols' absolute addresses in the target task by considering
the container DSO's load address. This symbol resolver could lookup both
public and private symbols (uppercase and lowercase letter in **nm** output
respectively). However, it turned out to be more than **injectdylib**
required, since we eventually didn't use any private symbols (it turned out
to be really helpful during development though). The old symbol resolver's
code was moved in **obsolete/**.

Newer vesions of **injectdylib** use **dlsym()** and **dladdr()** to discover
the relative offset of a symbol within a DSO. Although this won't give any
insight on private symbols, we believe it's more simple and effective. Once
the relative offset of a symbol is known, we can proceed to compute its
absolute value just like we did in the original symbol resolver; we look up
the container DSO's text segment address in the remote task and add to it
the relative offset of the symbol in question.

Last but not least, there's a thing called **CoreSymbolication** [02]. It's good,
but it's very Apple-ish, so I decided not to use it.


## References

[01] <https://opensource.apple.com/tarballs/libpthread/libpthread-105.1.4.tar.gz>

[02] <http://stackoverflow.com/questions/17445960/finding-offsets-of-local-symbols-in-shared-libraries-programmatically-on-os-x>

