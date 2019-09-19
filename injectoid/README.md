# injectoid - DSO injection for Android

huku &lt;[huku@grhack.net](mailto:huku@grhack.net)&gt;

## About

**injectoid** is a tool, part of **injectdso**, that allows one to inject
arbitrary shared libraries in running processes under Android.


## Compiling injectoid

Create a standalone cross-compile toolchain for ARM or Aarch64 using the Android
NDK [01], set **CROSS\_COMPILE** and run **make** as shown below:

```
CROSS_COMPILE=/tmp/arm/bin/arm-linux-androideabi- make
```

For more information on the injection technique implemented in **injectoid**
have a look at **NOTES.md**.


## Using injectoid

Push **injectoid** and the DSO you would like to inject on your smartphone's
**/data/local/tmp**.

```
$ adb push injectoid /data/local/tmp/
$ adb push libtest.so /data/local/tmp/
```

If your device is running Android >= 8, you need to create a **tmpfs** mount
point and place your library there in order to bypass namespace protections
implemented in the Android linker binary (see [02], [03], [04] and [05]),
otherwise **/data/local/tmp** will do just fine.

```
$ cd /data/local/tmp
$ mkdir tmpfs
$ mount -t tmpfs tmpfs tmpfs/
$ mv libtest.so tmpfs/
```

Normally, for the following to succeed, SELinux should be disabled/bypassed
and/or your device should be rooted.

```
$ ./injectoid /data/local/tmp/tmpfs/libtest.so 14313
[*] Injecting /data/local/tmp/tmpfs/libtest.so in PID 14313
[*] Resolved dlopen() at 0xf7505ba4
[*] Modifying target's state
[*] Waiting for target to throw SIGSEGV or SIGBUS
[*] Done
```

Verify that **libtest.so** has been successfully loaded by checking
**/proc/\<PID\>/maps**.

Optionally, use **-a** to have **injectoid** pause all threads of an application
before modifying the target thread.

```
$ ./injectoid -a /data/local/tmp/tmpfs/libtest.so 14313
```

## References

[01] <https://developer.android.com/ndk/guides/standalone_toolchain.html>

[02] <http://androidxref.com/8.1.0_r33/xref/bionic/linker/linker.cpp#2160>

[03] <http://androidxref.com/9.0.0_r3/xref/bionic/linker/linker.cpp#2280>

[04] <http://androidxref.com/8.1.0_r33/xref/bionic/linker/linker.cpp#1231>

[05] <http://androidxref.com/9.0.0_r3/xref/bionic/linker/linker.cpp#1241>

