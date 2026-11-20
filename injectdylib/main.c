/* injectdylib - DSO injection for MacOS X
 * huku <huku@grhack.net>
 *
 * main.c - Holds the program entry point and `remote_dlopen()', the function
 * that does all the dirty work.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mach/mach.h>

#include "symbol.h"
#include "memory.h"

#include "stubs/stubs.h"


#define ALIGN8(x)    (((x) + 7) & ~7)
#define MEGABYTES(x) ((x) << 20)


/* Force the DSO `filename' to be loaded on the remote task `task'. */
static int remote_dlopen(task_t task, const char *filename)
{
    thread_act_t thread;
    mach_vm_address_t address, pthread_t_address, string_address;
    vm_size_t size;
    kern_return_t kr;
#ifdef __x86_64__
    x86_thread_state64_t ts;
    int flavor = x86_THREAD_STATE64;
    mach_msg_type_number_t state_cnt = x86_THREAD_STATE64_COUNT;
#elif defined __i386__
    x86_thread_state32_t ts;
    int flavor = x86_THREAD_STATE32;
    mach_msg_type_number_t state_cnt = x86_THREAD_STATE32_COUNT;
#elif defined __arm64__
    arm_thread_state64_t ts;
    int flavor = ARM_THREAD_STATE64;
    mach_msg_type_number_t state_cnt = ARM_THREAD_STATE64_COUNT;
#else
#error Unsupported architecture
#endif
    int rv = 0;

    printf("[*] Resolving symbols on remote task\n");

    /* Set the address of `pthread_create()' in `stub[]'. */
    if((rv = resolve_symbol_runtime(task, "/usr/lib/system/libsystem_pthread.dylib",
            "pthread_create_from_mach_thread", &address)) != 0)
    {
        fprintf(stderr, "Failed to resolve \"pthread_create_from_mach_thread\"\n");
        goto _exit;
    }
    printf("[*] Resolved \"pthread_create_from_mach_thread\" at @0x%lx\n",
        (uintptr_t)address);
    *(mach_vm_address_t *)&stub[PTHREAD_CREATE_OFFSET] = address;

    /* Set the address of `dlopen()' in `stub[]'. */
    if((rv = resolve_symbol_runtime(task, "/usr/lib/system/libdyld.dylib",
            "dlopen", &address)) != 0)
    {
        printf("Failed to resolve \"dlopen\"\n");
        goto _exit;
    }
    printf("[*] Resolved \"dlopen\" at @0x%lx\n", (uintptr_t)address);
    *(mach_vm_address_t *)&stub[DLOPEN_OFFSET] = address;

    /* Set the address of `pthread_exit()' in `stub[]'. */
    if((rv = resolve_symbol_runtime(task, "/usr/lib/system/libsystem_pthread.dylib",
        "pthread_exit", &address)) != 0)
    {
        printf("Failed to resolve \"pthread_exit\"\n");
        goto _exit;
    }
    printf("[*] Resolved \"pthread_exit\" at @0x%lx\n", (uintptr_t)address);
    *(mach_vm_address_t *)&stub[PTHREAD_EXIT_OFFSET] = address;


    /* Now allocate some memory on the remote task and mark it as readable,
     * writable and executable.
     */
    size = MEGABYTES(1);
    if((kr = mach_vm_allocate(task, &address, size, VM_FLAGS_ANYWHERE)) != KERN_SUCCESS)
    {
        printf("mach_vm_allocate: failed (%d)\n", kr);
        rv = -kr;
        goto _exit;
    }

    /* Make memory R+W due to W^X protections. */
    if((kr = mach_vm_protect(task, address, size, 0,
            VM_PROT_READ | VM_PROT_WRITE)) != KERN_SUCCESS)
    {
        printf("mach_vm_protect: failed (%d)\n", kr);
        mach_vm_deallocate(task, address, size);
        rv = -kr;
        goto _exit;
    }

    printf("[*] Allocated memory on remote task at @0x%lx\n", (uintptr_t)address);


    /* Set the address of the string holding the filename of the DSO to load
     * (i.e. first argument of `dlopen()') in `stub[]'.
     */
    string_address = ALIGN8(address + size / 2);
    write_memory(task, string_address, (void *)filename, strlen(filename) + 1);
    *(mach_vm_address_t *)&stub[STRING_OFFSET] = string_address;

    /* This is where the `pthread_t' value returned by `pthread_create()' will
     * be stored.
     */
    pthread_t_address = ALIGN8(string_address + strlen(filename) + 1);
    *(mach_vm_address_t *)&stub[PTHREAD_T_OFFSET] = pthread_t_address;

    /* Write modified `stub[]' in remote task's memory. */
    write_memory(task, address, stub, sizeof(stub));

    printf("[*] Creating remote thread #1\n");

    /* Make memory R+X now. */
    if((kr = mach_vm_protect(task, address, size / 2, 0,
            VM_PROT_READ | VM_PROT_EXECUTE)) != KERN_SUCCESS)
    {
        printf("mach_vm_protect: failed (%d)\n", kr);
        mach_vm_deallocate(task, address, size);
        rv = -kr;
        goto _exit;
    }

    /* Create remote thread and set its initial CPU state. */
    if((kr = thread_create(task, &thread)) != KERN_SUCCESS)
    {
        mach_vm_deallocate(task, address, size);
        printf("thread_create: failed (%d)\n", kr);
        rv = -kr;
        goto _exit;
    }

    /* Setup initial thread state depending on the target architecture. */
#ifdef __x86_64__
    memset(&ts, 0, sizeof(x86_thread_state64_t));
    ts.__rip = address;
    ts.__rsp = address + size - 32;
#elif defined __i386__
    memset(&ts, 0, sizeof(x86_thread_state32_t));
    ts.__eip = address;
    ts.__esp = address + size - 32;
#elif defined __arm64__
    memset(&ts, 0, sizeof(arm_thread_state64_t));
    ts.__pc = address;
    ts.__sp = address + size - 32;
#endif

    if((kr = thread_set_state(thread, flavor, (thread_state_t)&ts,
            state_cnt)) != KERN_SUCCESS)
    {
        mach_vm_deallocate(task, address, size);
        thread_terminate(thread);
        printf("thread_set_state: failed (%d)\n", kr);
        rv = -kr;
        goto _exit;
    }

    /* Resume the thread and wait for 5 seconds until `pthread_create()' is
     * called. After that, kill the thread that called `pthread_create()'.
     */
    printf("[*] Resuming thread #1 and spawning thread #2\n");
    thread_resume(thread);

    printf("[*] Waiting 5 seconds for the threads to settle down\n");
    sleep(5);

    printf("[*] Terminating thread #1\n");
    thread_terminate(thread);

_exit:
    return rv;
}

int main(int argc, char *argv[])
{
    mach_port_t task;
    kern_return_t kr;
    int pid, rv = EXIT_FAILURE;

    if(argc != 3)
    {
        printf("%s <.dylib> <pid>\n", argv[0]);
        goto _exit;
    }

    pid = atoi(argv[2]);
    printf("[*] Will attempt to inject \"%s\" in PID %d\n", argv[1], pid);

    if((kr = task_for_pid(mach_task_self(), pid, &task)) != KERN_SUCCESS)
    {
        printf("task_for_pid: failed (%d)\n", kr);
        goto _exit;
    }

    if(remote_dlopen(task, argv[1]) != 0)
        goto _exit;

    rv = EXIT_SUCCESS;

    printf("[*] Done\n");

_exit:
    return rv;
}
