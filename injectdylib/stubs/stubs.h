#ifndef _STUB_H_
#define _STUB_H_

uint8_t stub[] =
#if defined __x86_64__
    /* _main:
     *     movq $0x4141414141414141, %rdi
     *     xorq %rsi, %rsi
     *     jmp _get_dlopen_stub
     */
    "\x48\xbf\x41\x41\x41\x41\x41\x41\x41\x41"
    "\x48\x31\xf6"
    "\xe9\x15\x00\x00\x00"

    /* _set_dlopen_stub:
     *     popq %rdx
     *     xorq %rcx, %rcx
     *     movq $0x4242424242424242, %rax
     *     call *%rax
     *     jmp _pause
     */
    "\x5a"
    "\x48\x31\xc9"
    "\x48\xb8\x42\x42\x42\x42\x42\x42\x42\x42"
    "\xff\xd0"
    "\xe9\x31\x00\x00\x00"

    /* _get_dlopen_stub:
     *     call _set_dlopen_stub
     */
    "\xe8\xe6\xff\xff\xff"

    /* _dlopen_stub:
     *     movq $0x4343434343434343, %rdi
     *     movq $2, %rsi
     *     movq $0x4444444444444444, %rax
     *     call *%rax
     *     xorq %rdi, %rdi
     *     movq $0x4545454545454545, %rax
     *     call *%rax
     */
    "\x48\xbf\x43\x43\x43\x43\x43\x43\x43\x43"
    "\x48\xc7\xc6\x02\x00\x00\x00"
    "\x48\xb8\x44\x44\x44\x44\x44\x44\x44\x44"
    "\xff\xd0"
    "\x48\x31\xff"
    "\x48\xb8\x45\x45\x45\x45\x45\x45\x45\x45"
    "\xff\xd0"

    /* _pause:
     *     nop
     *     jmp _pause
     */
    "\x90"
    "\xeb\xfd";
#elif defined __i386__
    /* _main:
     *     pushl $0
     *     jmp _get_dlopen_stub
     */
    "\x6a\x00"
    "\xeb\x10"

    /* _set_dlopen_stub:
     *     pushl $0
     *     pushl $0x41414141
     *     movl $0x42424242, %eax
     *     call *%eax
     *     jmp _pause
     */
    "\x6a\x00"
    "\x68\x41\x41\x41\x41"
    "\xb8\x42\x42\x42\x42"
    "\xff\xd0"
    "\xeb\x1c"

    /* _get_dlopen_stub:
     *     call _set_dlopen_stub
     */
    "\xe8\xeb\xff\xff\xff"

    /* _dlopen_stub:
     *     pushl $2
     *     pushl $0x43434343
     *     movl $0x44444444, %eax
     *     call *%eax
     *     pushl $0
     *     movl $0x45454545, %eax
     *     call *%eax
     */
    "\x6a\x02"
    "\x68\x43\x43\x43\x43"
    "\xb8\x44\x44\x44\x44"
    "\xff\xd0"
    "\x6a\x00"
    "\xb8\x45\x45\x45\x45"
    "\xff\xd0"

    /* _pause:
     *     nop
     *     jmp _pause
     */
    "\x90"
    "\xeb\xfd";
#endif

#if defined __x86_64__
#define PTHREAD_T_OFFSET       2
#define PTHREAD_CREATE_OFFSET  24
#define STRING_OFFSET          46
#define DLOPEN_OFFSET          63
#define PTHREAD_EXIT_OFFSET    78
#elif defined __i386__
#define PTHREAD_T_OFFSET       7
#define PTHREAD_CREATE_OFFSET  12
#define STRING_OFFSET          28
#define DLOPEN_OFFSET          33
#define PTHREAD_EXIT_OFFSET    42
#endif

#endif /* _STUB_H_ */
