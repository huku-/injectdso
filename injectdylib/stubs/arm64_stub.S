.globl _main
_main:
    ldr x0, _pthread_t_address
    mov x1, xzr
    adr x2, _dlopen_stub
    mov x3, xzr
    ldr x16, _pthread_create_address
    blr x16
    b _pause

_dlopen_stub:
    ldr x0, _string_address
    mov x1, #2
    ldr x16, _dlopen_address
    blr x16

    mov x0, xzr
    ldr x16, _pthread_exit_address
    blr x16

_pause:
    b _pause

_pthread_t_address:
    .quad 0x4141414141414141
_pthread_create_address:
    .quad 0x4242424242424242
_string_address:
    .quad 0x4343434343434343
_dlopen_address:
    .quad 0x4444444444444444
_pthread_exit_address:
    .quad 0x4545454545454545
