#ifndef _SYMBOL_H_
#define _SYMBOL_H_

#include <mach/mach_vm.h>

int resolve_symbol_runtime(task_t, const char *, const char *, mach_vm_address_t *);

#endif /* _SYMBOL_H_ */
