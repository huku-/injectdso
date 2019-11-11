/* injectdylib - DSO injection for MacOS X
 * huku <huku@grhack.net>
 *
 * symbol.c - Defines functions for resolving addresses of text segment symbols
 * in FAT/Mach-O binaries and running processes.
 */
#include "includes.h"
#include "memory.h"
#include "symbol.h"



# mark - resolve_symbol() doesn't work yet
/* Resolve symbol `symbol_name' in FAT or Mach-O file `filename' and return its
 * value in `symbol_value'. This symbol resolver works for public symbols only.
 */
int resolve_symbol(const char *filename, const char *symbol_name,
        vm_address_t *symbol_value)
{
    Dl_info info;
    const char *error;
    void *handle, *value;
    int rv = 0;


    handle = dlopen(filename, RTLD_LAZY);
    if(handle)
    {
        value = dlsym(handle, symbol_name);
        if(value)
        {
            /* Return the relative offset of the symbol from the base address of
             * the container DSO. We can't assume that the value returned by 
             * `dlopen()' is the DSO's base address; we have to call `dladdr()'
             * to determine it.
             */
            dladdr(value, &info);
            *symbol_value = (vm_address_t)(info.dli_saddr - info.dli_fbase);
        }

        dlclose(handle);
    }

    error = dlerror();
    if(error)
    {
        printf("resolve_symbol: failed (%s)\n", error);
        /* Does the `dlfcn.h' API set `errno'? */
        rv = -EFAULT;
    }

    return rv;
}


/* Find the base address of DSO or executable `filename' in remote task `task'.
 * The result is returned in `base_address'.
 */
void find_base_address(vm_map_t task, const char *filename, vm_address_t *base_address) {
	
	struct task_dyld_info dyld_info;
	mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
	if (task_info(task, TASK_DYLD_INFO, (task_info_t)&dyld_info, &count) == KERN_SUCCESS)
	{
		mach_msg_type_number_t size = sizeof(struct dyld_all_image_infos);
		mach_msg_type_number_t dataCnt = (mach_msg_type_number_t)size;
		struct dyld_all_image_infos* infos;;
		mach_vm_read(task, dyld_info.all_image_info_addr, size, (vm_offset_t*)&infos, &dataCnt);
		
		
        mach_msg_type_number_t size2 = sizeof(struct dyld_image_info) * infos->infoArrayCount;
		mach_msg_type_number_t dataCnt2 = (mach_msg_type_number_t)size2;
        struct dyld_image_info* info;
		mach_vm_read(task, (mach_vm_address_t)infos->infoArray, size2, (vm_offset_t*)&info, &dataCnt2);
		
        for (int i=0; i < infos->infoArrayCount; i++) {
            mach_msg_type_number_t size3 = PATH_MAX;
			mach_msg_type_number_t dataCnt3 = (mach_msg_type_number_t)size3;

			char* path;
			mach_vm_read(task, (mach_vm_address_t)info[i].imageFilePath, size, (vm_offset_t*)&path, &dataCnt3);
			
			if(strcmp(path, filename) == 0) {
                *base_address = (vm_address_t)info[i].imageLoadAddress;
                break;
            }
        }
	}
}

int resolve_symbol_runtime(task_t task, const char *filename,
        const char *symbol_name, vm_address_t *symbol_addr)
{
    vm_address_t base_address, offset;
    int rv;

	find_base_address(task, filename, &base_address);
	if(base_address == 0x0 ) {
		fprintf(stderr, "find_base_address failed: %s\n", symbol_name);
		return 1;
	}
	
    if((rv = resolve_symbol(filename, symbol_name, &offset)) == 0)
            *symbol_addr =  base_address + offset;
	else fprintf(stderr, "resolve_symbol failed: %s\n", symbol_name);
        
        //Debug
	//printf("Sym %s:\n\tBase: %p\n\tOff:  %p\n\t= %p\n",symbol_name, (void*)base_address, (void*)offset, (void*)*symbol_addr);
	
    return rv;
}
