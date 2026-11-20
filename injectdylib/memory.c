/* injectdylib - DSO injection for MacOS X
 * huku <huku@grhack.net>
 *
 * memory.c - Functions for reading and writing to a remote task's memory.
 */
#include <mach/mach.h>

#include "memory.h"

/* Read memory from remote task `task'. */
mach_vm_size_t read_memory(task_t task, mach_vm_address_t address, void *buf,
        mach_vm_size_t size)
{
    mach_vm_size_t outsize;
    mach_vm_address_t vmbuf = (mach_vm_address_t)buf;
    if(mach_vm_read_overwrite(task, address, size, vmbuf, &outsize) != KERN_SUCCESS)
        outsize = 0;
    return outsize;
}

/* Write memory to remote task `task'. */
mach_vm_size_t write_memory(task_t task, mach_vm_address_t address, void *buf,
        mach_vm_size_t size)
{
    mach_vm_address_t vmbuf = (mach_vm_address_t)buf;
    if(mach_vm_write(task, address, vmbuf, size) != KERN_SUCCESS)
        size = 0;
    return size;
}
