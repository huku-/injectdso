# injectdso - A collection of tools for injecting DSOs

huku &lt;[huku@grhack.net](mailto:huku@grhack.net)&gt;


## About

**injectdso** is a collection of tools for injecting DSOs in processes under
various operating systems. It currently consists of **injectdll** (for Microsoft
Windows), **injectdylib** (for MacOS X) and **injectoid** (for Android). Support
for Linux (**injectso**) is planned for the near future as well.

**injectdso** may be useful for tasks like:

  * Discovering and testing sandbox escape exploits

  * Testing properties of heap allocators on closed source applications

  * Hooking higher-level programming language runtimes

...and probably many more.

Tools in **injectdso** implement techniques which may differ from those used by
publicly available tools. For a description of the approach taken by each tool,
have a look at the corresponding **README.md** and **NOTES.md** files.

For bugs, comments, whatever, feel free to contact me.

