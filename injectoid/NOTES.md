# Notes

huku &lt;[huku@grhack.net](mailto:huku@grhack.net)&gt;


## Injection technique overview

**injectoid** uses a technique similar to **injectso64** [01], however, it
differs in the following:

  * **injectoid** will attach to all threads of a multi-threaded process and
    will stop all threads. One thread will be forced to call `dlopen()` while
    others are still stalled. I believe this results in a more consistent [02]
    treatment of applications with several threads (most Android applications
    are multi-threaded).

  * **injectoid** uses `PTRACE_GETREGSET` and `PTRACE_SETREGSET` to read and
    write registers respectively, as this seems to be the most portable and,
    nowadays preferred, way.

At first, I tried to use **/proc/\<PID\>/mem** to read and write memory in order
to avoid stupid `PTRACE_PEEKDATA` and `PTRACE_POKEDATA` alignment issues, but it
looks like on Aarch64 there's no `llseek()` (`__ARCH_WANT_SYS_LLSEEK` not set)
and thus 32-bit tasks cannot seek to arbitrary 32-bit addresses. Additionally,
`process_vm_readv()` and `process_vm_writev()` are also not available.


## References

[01] <https://github.com/ice799/injectso64>

[02] <http://stackoverflow.com/questions/18577956/how-to-use-ptrace-to-get-a-consistent-view-of-multiple-threads>

