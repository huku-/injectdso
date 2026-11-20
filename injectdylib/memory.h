#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <mach/mach_vm.h>

mach_vm_size_t read_memory(task_t, mach_vm_address_t, void *, mach_vm_size_t);

mach_vm_size_t write_memory(task_t, mach_vm_address_t, void *, mach_vm_size_t);

#endif /* _MEMORY_H_ */
