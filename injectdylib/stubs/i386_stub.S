.globl _main
_main:
    # Start pushing `pthread_create()' arguments in reverse order.
    pushl $0
    jmp _get_dlopen_stub

_set_dlopen_stub:
    # Address of `_dlopen_stub' already in stack.
    pushl $0
    # "$0x41414141" will be replaced with a writable location and "$0x42424242"
    # with the address of `pthread_create()'.
    pushl $0x41414141
    movl $0x42424242, %eax
    call *%eax
    jmp _pause

_get_dlopen_stub:
    call _set_dlopen_stub

_dlopen_stub:
    # "$0x43434343" will be replaced with the address of a string holding the
    # path to the library to be loaded and "$0x44444444" with the address of
    # `dlopen()'.
    pushl $2
    pushl $0x43434343
    movl $0x44444444, %eax
    call *%eax

    # "$0x45454545" will be replaced with the address of `pthread_exit()'.
    pushl $0
    movl $0x45454545, %eax
    call *%eax

    # We can't terminate ourselves because a remote thread owns us. Sleep and 
    # wait until we're killed.
_pause:
    nop
    jmp _pause
