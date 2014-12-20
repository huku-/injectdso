#ifndef _MEMORY_H_
#define _MEMORY_H_

#include "includes.h"

mach_msg_type_number_t read_memory(task_t, vm_address_t, void *,
    mach_msg_type_number_t);

mach_msg_type_number_t write_memory(task_t, vm_address_t, void *,
    mach_msg_type_number_t);

#endif /* _MEMORY_H_ */
