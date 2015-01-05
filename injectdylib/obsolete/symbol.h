#ifndef _SYMBOL_H_
#define _SYMBOL_H_

#include "includes.h"

/* Architecture specific macros. */
#if defined __x86_64__
#define CPU_TYPE         CPU_TYPE_X86_64
#define LC_SEGMENT_TYPE  LC_SEGMENT_64
#define MH_MAGIC_1       MH_MAGIC_64
#define MH_MAGIC_2       MH_CIGAM_64
#elif defined __i386__
#define CPU_TYPE         CPU_TYPE_X86
#define LC_SEGMENT_TYPE  LC_SEGMENT
#define MH_MAGIC_1       MH_MAGIC
#define MH_MAGIC_2       MH_CIGAM
#endif

/* Architecture specific type definitions. */
#if defined __x86_64__
typedef struct mach_header_64 mach_header_t;
typedef struct segment_command_64 segment_command_t;
typedef struct nlist_64 nlist_t;
#elif defined __i386__
typedef struct mach_header mach_header_t;
typedef struct segment_command segment_command_t;
typedef struct nlist nlist_t;
#endif


#define MAX_NUM_SECTIONS 128
#define MAX_NUM_SEGMENTS 64

int resolve_symbol(const char *, const char *, vm_address_t *);
int resolve_symbol_runtime(task_t, const char *, const char *, vm_address_t *);

#endif /* _SYMBOL_H_ */
