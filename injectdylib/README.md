# injectdylib - DSO injection for MacOS X

huku &lt;[huku@grhack.net](mailto:huku@grhack.net)&gt;


## About

**injectdylib** is a tool, part of **injectdso**, that allows one to inject
arbitrary **.dylib** files in running processes under MacOS X.


## Compiling injectdylib

Just run **make**.

If you want to inject a library on a 32bit process, uncomment the corresponding
**CFLAGS** and **LDFLAGS** in the accompanying **Makefile** and run **make** to
produce a 32bit binary.


## Using injectdylib

Let's see how loading a DSO in a 32bit process looks like.

First, uncomment the appropriate **CFLAGS** and **LDFLAGS** in **Makefile** and
run **make**. The output should look like the following.

```
$ make
cc -Wall -O2 -g -m32   -c -o memory.o memory.c
cc -Wall -O2 -g -m32   -c -o symbol.o symbol.c
cc -Wall -O2 -g -m32   -c -o main.o main.c
cc memory.o symbol.o main.o -o injectdylib -m32
```

If any warnings are produced, don't hesitate to report them to me.

Next, create and run a sample 32bit program.

```
$ cat test.c 
#include <stdio.h>
#include <unistd.h>

int main()
{ 
    printf("%d\n", getpid());
    while(1) 
        sleep(10000); 
    return 0;
}
$ cc -Wall -m32 test.c -o test
$ ./test 
4921
```

Fire up **gdb** and attach to it.

```
# gdb -q /tmp/test 4921
...
(gdb) c
```

Open up another terminal and use **injectdylib** to load **libtidy.dylib** in 
PID 4921.

```
# ./injectdylib /usr/lib/libtidy.dylib 4921
[*] Will attempt to inject "/usr/lib/libtidy.dylib" in PID 4921
[*] Resolving symbols on remote task
[*] Resolved "_pthread_create" at @0x9aa52315
[*] Resolved "_dlopen" at @0x997b0a4f
[*] Resolved "_pthread_exit" at @0x9aa5197f
[*] Allocated memory on remote task at @0x200000
[*] Creating remote thread #1
[*] Resuming thread #1 and spawning thread #2
[*] Waiting 5 seconds for the threads to settle down
[*] Terminating thread #1
[*] Done
```

Now switch to the debugger, the output indicates that a new library was loaded.

```
(gdb) c
Reading symbols for shared libraries . done
```

To verify that **libtidy.dylib** was loaded, press Ctrl+C and use the command 
**info shared**.


## Useful resources

  * [mach\_vm\_region\_recurse, mapping memory and shared libraries on osx](http://stackoverflow.com/questions/6963625/mach-vm-region-recurse-mapping-memory-and-shared-libraries-on-osx)

  * [mach\_inject](https://github.com/rentzsch/mach_inject) by @rentzsch

For a description of the injection technique implemented in **injectdylib** have
a look at **NOTES.md**.

