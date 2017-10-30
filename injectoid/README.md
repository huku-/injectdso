# injectoid - DSO injection for Android

huku &lt;[huku@grhack.net](mailto:huku@grhack.net)&gt;

## About

**injectoid** is a tool, part of **injectdso**, that allows one to inject
arbitrary shared libraries in running processes under Android.


## Compiling injectoid

  * Make sure **adb** is in your **PATH**.

  * Plug your smartphone on a USB port. The **Makefile** will pull the linker
    binaries in order to pre-compute certain function offsets.

  * Create a cross-compile toolchain for ARM or Aarch64 using Android NDK [01].

  * Set **CROSS\_COMPILE** and run **make** as shown below:

    ```
    CROSS_COMPILE=/tmp/arm/bin/arm-linux-androideabi- make
    ```

For more information on the injection technique implemented in **injectoid**
have a look at **NOTES.md**.


## Using injectoid

Upload **injectoid** on your smartphone's **/data/local/tmp**.

```
$ adb push injectoid /data/local/tmp/
```

Normally, for the following to succeed, you need a rooted device.

```
$ cd /data/local/tmp
$ ./injectoid /data/local/tmp/libtest.so 14313
[*] Injecting /data/local/tmp/libtest.so in PID 14313
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
$ ./injectoid -a /data/local/tmp/libtest.so 14313
```

## References

[01] <https://developer.android.com/ndk/guides/standalone_toolchain.html>

