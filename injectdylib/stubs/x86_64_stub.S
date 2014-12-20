.globl _main
_main:
    # "$0x4141414141414141" will be replaced with a writable location (this is
    # where the `pthread_t' of `pthread_create()' will be stored).
    movq $0x4141414141414141, %rdi
    xorq %rsi, %rsi
    jmp _get_dlopen_stub

_set_dlopen_stub:
    popq %rdx 
    xorq %rcx, %rcx
    # "$0x4242424242424242" will be replaced with the address of 
    # `pthread_create()'.
    movq $0x4242424242424242, %rax
    call *%rax
    jmp _pause

_get_dlopen_stub:
    call _set_dlopen_stub

_dlopen_stub:
    # "$0x4343434343434343" will be replaced with the address of the string 
    # holding the path to the library to be loaded and "$0x4444444444444444" 
    # with the address of `dlopen()'.
    movq $0x4343434343434343, %rdi
    movq $2, %rsi
    movq $0x4444444444444444, %rax
    call *%rax

    # "$0x4545454545454545" will be replaced with the address of 
    # `pthread_exit()'.
    xorq %rdi, %rdi
    movq $0x4545454545454545, %rax
    call *%rax

    # We can't terminate ourselves because a remote thread owns us. Sleep and 
    # wait until we're killed.
_pause:
    nop
    jmp _pause
