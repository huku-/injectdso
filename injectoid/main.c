/* injectoid - DSO injection for Android
 * huku <huku@grhack.net>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/ptrace.h>
#include <dirent.h>
#include <dlfcn.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <linux/elf.h>

#include "symbol.h"


/* Offset from stack pointer where the filename of the DSO to be injected will
 * be written to in the target process.
 */
#define SP_OFF 128


#ifdef __LP64__
#define LINKER_PATH "/system/bin/linker64"
#else
#define LINKER_PATH "/system/bin/linker"
#endif


/* Architecture specific macros. */
#if defined(__aarch64__)

/* See "arch/arm64/include/uapi/asm/ptrace.h" for more information. */
typedef uint64_t reg_t;
typedef struct user_regs_struct regs_t;

#define ARG0(x) ((x).regs[0])
#define ARG1(x) ((x).regs[1])
#define ARG2(x) ((x).regs[2])
#define ARG3(x) ((x).regs[3])
#define SP(x)   ((x).sp)
#define LR(x)   ((x).regs[30])
#define PC(x)   ((x).pc)

#elif defined(__arm__)

/* See "arch/arm/include/uapi/asm/ptrace.h" for more information. */
typedef uint32_t reg_t;
typedef struct user_regs regs_t;

#define ARG0(x) ((x).uregs[0])
#define ARG1(x) ((x).uregs[1])
#define ARG2(x) ((x).uregs[2])
#define ARG3(x) ((x).uregs[3])
#define SP(x)   ((x).uregs[13])
#define LR(x)   ((x).uregs[14])
#define PC(x)   ((x).uregs[15])
#define CPSR(x) ((x).uregs[16])

/* See "arch/x86/include/uapi/asm/ptrace.h" for more information. */
#elif defined(__x86_64__)

typedef unsigned long reg_t;
typedef struct pt_regs regs_t;

#define ARG0(x) ((x).rdi)
#define ARG1(x) ((x).rsi)
#define ARG2(x) ((x).rdx)
#define ARG3(x) ((x).rcx)
#define SP(x)   ((x).rsp)
#define LR(x)   (SP(x))
#define PC(x)   ((x).rip)

/* See "arch/x86/include/uapi/asm/ptrace.h" for more information. */
#elif defined(__i386__)

typedef unsigned long reg_t;
typedef struct pt_regs regs_t;

#define ARG0(x) (SP(x) + 4)
#define ARG1(x) (SP(x) + 8)
#define ARG2(x) (SP(x) + 12)
#define ARG3(x) (SP(x) + 16)
#define SP(x)   ((x).esp)
#define LR(x)   (SP(x))
#define PC(x)   ((x).eip)

#else
#error Unsupported architecture
#endif


/* Helper macro for Android emulator targets. Writes a register value to an
 * arbitrary memory location. Used for writing return addresses and arguments
 * in the debuggee's stack.
 */
#if defined(__x86_64__) || defined(__i386__)
#define WRITE_REG(pid, addr, value) ({ \
    reg_t _value = value;              \
    size_t _size = sizeof(reg_t);      \
    (write_memory((pid), (addr), &_value, _size) != (ssize_t)_size) ? -1 : 0; \
})
#endif


#define DLOPEN_NAME1 "__dl__Z9do_dlopenPKciPK17android_dlextinfo"     /* Android 6 */
#define DLOPEN_NAME2 "__dl__Z9do_dlopenPKciPK17android_dlextinfoPv"   /* Android 7-7.1 */
#define DLOPEN_NAME3 "__dl__Z9do_dlopenPKciPK17android_dlextinfoPKv"  /* Android 8-9 */


/* Get address of linker in debuggee's memory. */
static void *get_linker_addr(pid_t pid)
{
    char buf[BUFSIZ], *tok[6];
    int i;
    FILE *fp;

    void *r = NULL;

    snprintf(buf, sizeof(buf), "/proc/%d/maps", pid);

    if((fp = fopen(buf, "r")) == NULL)
    {
        perror("get_linker_addr: fopen");
        goto ret;
    }

    while(fgets(buf, sizeof(buf), fp))
    {
        i = strlen(buf);
        if(i > 0 && buf[i - 1] == '\n')
            buf[i - 1] = 0;

        tok[0] = strtok(buf, " ");
        for(i = 1; i < 6; i++)
            tok[i] = strtok(NULL, " ");

        if(tok[5] && strcmp(tok[5], LINKER_PATH) == 0)
        {
            r = (void *)strtoul(tok[0], NULL, 16);
            goto close;
        }
    }

close:
    fclose(fp);

ret:
    return r;
}


/* Get threads of PID `pid'. Returns 0 on success, or -1 on failure. On success
 * `*thr_listp' holds `*thr_cntp' PIDs.
 */
static int get_thr_list(pid_t pid, pid_t **thr_listp, int *thr_cntp)
{
    char dirname[PATH_MAX];
    DIR *dir;
    struct dirent *ent;
    pid_t *thr_list = NULL;
    int i = 0, size = 0, r = -1;


    snprintf(dirname, sizeof(dirname), "/proc/%d/task", pid);

    if((dir = opendir(dirname)) == NULL)
    {
        perror("get_thr_list: opendir");
        goto ret;
    }

    while((ent = readdir(dir)))
    {
        if(strcmp(ent->d_name, ".") && strcmp(ent->d_name, ".."))
        {
            if(i >= size)
            {
                size += 64;

                if((thr_list = realloc(thr_list, size * sizeof(pid))) == NULL)
                {
                    perror("get_thr_list: realloc");
                    free(thr_list);
                    goto close;
                }
            }

            thr_list[i] = strtoul(ent->d_name, NULL, 10);
            i += 1;
        }
    }

    *thr_listp = thr_list;
    *thr_cntp = i;

    r = 0;

close:
    closedir(dir);

ret:
    return r;
}


/* Attach to PID `pid' and, if `all_thrs' is true, to all of its threads as well. */
static int attach(pid_t pid, char all_thrs)
{
    pid_t *thr_list, tid;
    int status, thr_cnt, i, r = -1;

    if(all_thrs)
    {
        if(get_thr_list(pid, &thr_list, &thr_cnt) != 0)
            goto ret;
    }
    else
    {
        thr_list = &pid;
        thr_cnt = 1;
    }

    for(i = 0; i < thr_cnt; i++)
    {
        tid = thr_list[i];

#ifdef DEBUG
        printf("[*] Attaching %d/%d (PID %d)\n", i + 1, thr_cnt, tid);
#endif

        if(ptrace(PTRACE_ATTACH, tid, NULL, NULL) != 0)
        {
            perror("attach: ptrace");
            goto detach;
        }

        do
        {
            if(waitpid(tid, &status, __WALL) < 0)
            {
                perror("attach: waitpid");
                goto detach;
            }
        }
        while(WIFSTOPPED(status) == 0 || WSTOPSIG(status) != SIGSTOP);
    }

    r = 0;

detach:
    if(r)
        while(--i >= 0)
            ptrace(PTRACE_DETACH, thr_list[i], NULL, NULL);

    if(all_thrs)
        free(thr_list);

ret:
    return r;
}


/* Detach PID `pid' and, if `all_thrs' is true, all of its threads as well. */
static int detach(pid_t pid, char all_thrs)
{
    pid_t *thr_list, tid;
    int thr_cnt, i, r = -1;

    if(all_thrs)
    {
        if(get_thr_list(pid, &thr_list, &thr_cnt) != 0)
            goto ret;
    }
    else
    {
        thr_list = &pid;
        thr_cnt = 1;
    }

    for(i = 0; i < thr_cnt; i++)
    {
        tid = thr_list[i];

#ifdef DEBUG
        printf("[*] Detaching %d/%d (PID %d)\n", i + 1, thr_cnt, tid);
#endif

        if(ptrace(PTRACE_DETACH, tid, NULL, NULL) != 0)
        {
            perror("detach: ptrace");
            goto free;
        }
    }

    r = 0;

free:
    if(all_thrs)
        free(thr_list);

ret:
    return r;
}


/* Read `size' bytes of data from address `addr' in PID `pid' into `buf'.
 * Returns actual number of bytes read on success and -1 on failure.
 */
static ssize_t read_memory(pid_t pid, void *addr, void *buf, size_t size)
{
    size_t i, rem;
    long word;

    ssize_t r = -1;

    if(size > SSIZE_MAX)
        goto ret;

    i = 0;
    while(i < size)
    {
        errno = 0;
        word = ptrace(PTRACE_PEEKDATA, pid, addr + i, NULL);

        if(errno)
            break;

        rem = size - i < sizeof(word) ? size - i : sizeof(word);

        memcpy(buf + i, &word, rem);
        i += rem;
    }

    /* An error of `EIO' is returned when we have hit an unmapped address. We
     * don't assume this to be an error, we just return the number of bytes that
     * were actually read. It's up to the user to check the return value.
     */
    if(errno && errno != EIO)
    {
        perror("read_memory: ptrace");
        goto ret;
    }

    r = (ssize_t)i;

ret:
    return r;
}


/* Write contents of `buf' of size `size' at address `addr' in PID `pid'.
 * Returns actual number of bytes written on success and -1 on failure.
 */
static ssize_t write_memory(pid_t pid, void *addr, void *buf, size_t size)
{
    size_t i, rem;
    long word;

    ssize_t r = -1;


    if(size > SSIZE_MAX)
        goto ret;

    i = 0;
    while(i < size)
    {
        rem = size - i < sizeof(word) ? size - i : sizeof(word);

        if(rem < sizeof(word) &&
                read_memory(pid, addr + i, &word, sizeof(word)) != sizeof(word))
            goto ret;

        memcpy(&word, buf + i, rem);

        errno = 0;
        ptrace(PTRACE_POKEDATA, pid, addr + i, word);

        if(errno)
            break;

        i += rem;
    }

    /* See comment in `read_memory()' above. */
    if(errno && errno != EIO)
    {
        perror("write_memory: ptrace");
        goto ret;
    }

    r = (ssize_t)i;

ret:
    return r;
}


/* Read registers of PID `pid'. */
static int read_registers(pid_t pid, regs_t *regs)
{
    struct iovec iov;

    int r = -1;

    iov.iov_base = regs;
    iov.iov_len = sizeof(*regs);

    if(ptrace(PTRACE_GETREGSET, pid, NT_PRSTATUS, &iov) != 0)
    {
        perror("read_registers: ptrace");
        goto ret;
    }

    r = 0;

ret:
    return r;
}


/* Write registers of PID `pid'. */
static int write_registers(pid_t pid, regs_t *regs)
{
    struct iovec iov;

    int r = -1;

    iov.iov_base = regs;
    iov.iov_len = sizeof(*regs);

    if(ptrace(PTRACE_SETREGSET, pid, NT_PRSTATUS, &iov) != 0)
    {
        perror("write_registers: ptrace");
        goto ret;
    }

    r = 0;

ret:
    return r;
}


/* Resume thread whose PID is `pid' and wait until it segfaults. Since we have
 * attached to all threads of the target process, only the specified thread is
 * actually resumed.
 */
static int resume_and_wait(pid_t pid)
{
    int status, r = -1;

    if(ptrace(PTRACE_CONT, pid, NULL, NULL) != 0)
    {
        perror("resume_and_wait: ptrace");
        goto ret;
    }

    do
    {
        if(waitpid(pid, &status, __WALL) < 0)
        {
            perror("resume_and_wait: waitpid");
            goto ret;
        }
    }
    while(WIFSTOPPED(status) == 0 ||
        (WSTOPSIG(status) != SIGSEGV && WSTOPSIG(status) != SIGBUS));

    r = 0;

ret:
    return r;
}


/* Force the target to call `dlopen()'. Modify its registers and stack contents
 * and continue execution until a segmentation fault is caught. Return 0 on
 * success and -1 on failure.
 */
static int force_dlopen(pid_t pid, char *filename)
{
    void *linker_addr, *dlopen_off, *dlopen_addr;
    regs_t regs;
    size_t size;

    int r = -1;

    if(resolve_symbol(LINKER_PATH, DLOPEN_NAME1, &dlopen_off) != 0 &&
            resolve_symbol(LINKER_PATH, DLOPEN_NAME2, &dlopen_off) != 0 &&
            resolve_symbol(LINKER_PATH, DLOPEN_NAME3, &dlopen_off) != 0)
    {
        printf("[*] Could not resolve dlopen()\n");
        goto ret;
    }

    if((linker_addr = get_linker_addr(pid)) == NULL)
    {
        printf("[*] Linker not found in PID %d\n", pid);
        goto ret;
    }

    dlopen_addr = linker_addr + (ptrdiff_t)dlopen_off;
    printf("[*] Resolved dlopen() at %p\n", dlopen_addr);

    if(read_registers(pid, &regs) != 0)
        goto ret;

#if defined(__aarch64__)
    /* Prepare `do_dlopen()' input arguments. On Android >= 7, we set the 4th
     * argument to a value that emulates `__builtin_return_address()', so that
     * our DSO is loaded in the correct namespace.
     */
    ARG0(regs) = SP(regs) + SP_OFF;
    ARG1(regs) = RTLD_NOW | RTLD_GLOBAL;
    ARG2(regs) = 0;
    ARG3(regs) = PC(regs);

    /* We set the new PC and also set LR to force the debuggee to crash. */
    LR(regs) = 0xffffffffffffffff;
    PC(regs) = (reg_t)dlopen_addr;

#elif defined(__arm__)
    ARG0(regs) = SP(regs) + SP_OFF;
    ARG1(regs) = RTLD_NOW | RTLD_GLOBAL;
    ARG2(regs) = 0;
    ARG3(regs) = PC(regs);
    LR(regs) = 0xffffffff;

    /* Force switch to Thumb mode. */
    PC(regs) = (reg_t)dlopen_addr | 1;
    CPSR(regs) |= 1 << 5;

#elif defined(__x86_64__)
    ARG0(regs) = SP(regs) + SP_OFF;
    ARG1(regs) = RTLD_NOW | RTLD_GLOBAL;
    ARG2(regs) = 0;
    ARG3(regs) = PC(regs);

    if(WRITE_REG(pid, (void *)LR(regs), 0xffffffffffffffff) != 0)
        goto ret;

    /* XXX: What the fuck #1. */
    PC(regs) = (reg_t)dlopen_addr + 2;

#elif defined(__i386__)
    if(WRITE_REG(pid, (void *)ARG0(regs), SP(regs) + SP_OFF) != 0 ||
            WRITE_REG(pid, (void *)ARG1(regs), RTLD_NOW | RTLD_GLOBAL) != 0 ||
            WRITE_REG(pid, (void *)ARG2(regs), 0) != 0 ||
            WRITE_REG(pid, (void *)ARG3(regs), PC(regs)) != 0 ||
            WRITE_REG(pid, (void *)LR(regs), 0xffffffff) != 0)
        goto ret;

    /* XXX: What the fuck #2. */
    PC(regs) = (reg_t)dlopen_addr + 2;
#endif

    printf("[*] Modifying target's state\n");

    if(write_registers(pid, &regs) != 0)
        goto ret;

    size = strlen(filename) + 1;
    if(write_memory(pid, (void *)SP(regs) + SP_OFF, filename, size) != (ssize_t)size)
        goto ret;

    printf("[*] Waiting for target to throw SIGSEGV or SIGBUS\n");

    if(resume_and_wait(pid) != 0)
        goto ret;

    r = 0;

ret:
    return r;
}


/* Attach to PID `pid', take a snapshot, modify its state to have it call
 * `dlopen()', restore the previously saved snapshot and detach.
 */
static int inject(pid_t pid, char *filename, char all_thrs)
{
    regs_t regs;
    char buf[PAGE_SIZE];
    ssize_t size;

    int r = -1;

    if(attach(pid, all_thrs) != 0)
        goto ret;

    if(read_registers(pid, &regs) != 0)
        goto ret;

    if((size = read_memory(pid, (void *)SP(regs), buf, sizeof(buf))) < 0)
        goto ret;

    r = 0;

    if(force_dlopen(pid, filename) != 0)
        r = -1;

    if(write_memory(pid, (void *)SP(regs), buf, size) != size)
        r = -1;

    if(write_registers(pid, &regs) != 0)
        r = -1;

    if(detach(pid, all_thrs) != 0)
        r = -1;

ret:
    return r;
}


void usage(void)
{
    printf("injectoid [-a] <DSO> <PID>\n");
    exit(EXIT_FAILURE);
}


int main(int argc, char *argv[])
{
    pid_t pid;
    char all_thrs = 0;
    int ch, r = EXIT_FAILURE;

    while((ch = getopt(argc, argv, "a")) != -1)
    {
        switch(ch)
        {
            case 'a':
                all_thrs = 1;
                break;

            default:
                usage();
                break;
        }
    }

    argc -= optind;
    argv += optind;

    if(argc != 2)
    {
        usage();
        goto ret;
    }

    if((pid = atoi(argv[1])) <= 0)
        goto ret;

    if(all_thrs)
        printf("[*] Suspending all threads and injecting %s in PID %d\n", argv[0], pid);
    else
        printf("[*] Injecting %s in PID %d\n", argv[0], pid);

    if(inject(pid, argv[0], all_thrs) != 0)
        goto ret;

    printf("[*] Done\n");

    r = EXIT_SUCCESS;

ret:
    return r;
}

