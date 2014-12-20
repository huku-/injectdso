/* injectdylib - DSO injection for MacOS X
 * huku <huku@grhack.net>
 *
 * memory.c - Functions for reading and writing to a remote task's memory.
 */
#include "includes.h"
#include "memory.h"

/* Read memory from remote task `task'. */
mach_msg_type_number_t read_memory(task_t task, vm_address_t address, void *buf,
        mach_msg_type_number_t size)
{
    vm_size_t outsize;
    vm_address_t vmbuf = (vm_address_t)buf;
    if(vm_read_overwrite(task, address, size, vmbuf, &outsize) != KERN_SUCCESS)
        outsize = 0;
    return outsize;
}

/* Write memory to remote task `task'. */
mach_msg_type_number_t write_memory(task_t task, vm_address_t address,
        void *buf, mach_msg_type_number_t size)
{
    vm_address_t vmbuf = (vm_address_t)buf;
    if(vm_write(task, address, vmbuf, size) != KERN_SUCCESS)
        size = 0;
    return size;
}

