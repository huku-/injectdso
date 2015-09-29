/* injectdylib - DSO injection for MacOS X
 * huku <huku@grhack.net>
 *
 * symbol.c - Defines functions for resolving addresses of text segment symbols
 * in FAT/Mach-O binaries and running processes.
 */
#include "includes.h"
#include "memory.h"
#include "symbol.h"


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
static int find_base_address(vm_map_t task, const char *filename,
        vm_address_t *base_address)
{
    mach_header_t mh;
    struct vm_region_submap_info_64 info;
    struct dyld_all_image_infos daii;
    struct dyld_image_info dii;
    mach_msg_type_number_t count;
    vm_address_t address;
    vm_size_t size;
    uint32_t depth, daii_offset, i;
    char pathname[PATH_MAX];
    kern_return_t kr;
    int rv = 0;

    /* Iterate through all mapped regions and locate the base address of the
     * dynamic loader "/usr/lib/dyld". Theoretically, the following loop should
     * never fail. Notice that the 64bit API and structures are used for both
     * x86_64 and i386.
     */
    address = 0;
    size = 0;
    depth = 0;
    count = VM_REGION_SUBMAP_INFO_COUNT_64;
    while((kr = vm_region_recurse_64(task, &address, &size, &depth,
            (vm_region_info_64_t)&info, &count)) != KERN_INVALID_ADDRESS)
    {
        /* Just look for a valid magic number and `MH_DYLINKER'. */
        memset(&mh, 0, sizeof(mach_header_t));
        read_memory(task, address, &mh, sizeof(mach_header_t));
        if((mh.magic == MH_MAGIC_1 || mh.magic == MH_MAGIC_2) &&
                mh.filetype == MH_DYLINKER)
            break;

        if(info.is_submap)
            depth++;
        else
            address += size;
    }

    /* If the user requested the base address of "/usr/lib/dyld", then we're
     * done.
     */
    if(strcmp(filename, "/usr/lib/dyld") == 0)
    {
        *base_address = address;
    }
    /* Otherwise, iterate through all mapped libraries. */
    else
    {
        /* I discovered the following trick after studying the source code of
         * "vmmap(1)"; works like charm.
         */
        read_memory(task, address + DYLD_ALL_IMAGE_INFOS_OFFSET_OFFSET,
            &daii_offset, sizeof(uint32_t));
        read_memory(task, address + daii_offset, &daii,
            sizeof(struct dyld_all_image_infos));
        for(i = 0; i < daii.infoArrayCount; i++)
        {
            read_memory(task, (vm_address_t)(daii.infoArray + i), &dii,
                sizeof(struct dyld_image_info));
            read_memory(task, (vm_address_t)dii.imageFilePath, pathname,
                sizeof(pathname));

            if(strcmp(pathname, filename) == 0)
            {
                *base_address = (vm_address_t)dii.imageLoadAddress;
                break;
            }
        }

        if(i >= daii.infoArrayCount)
            rv = -EINVAL;
    }
    return rv;
}

/* Resolve symbol `symbol_name' from DSO `filename' in remote task `task'. The
 * symbol's value is returned in `symbol_value'.
 */
int resolve_symbol_runtime(task_t task, const char *filename,
        const char *symbol_name, vm_address_t *symbol_value)
{
    vm_address_t address, value;
    int rv;

    if((rv = find_base_address(task, filename, &address)) == 0)
    {
        if((rv = resolve_symbol(filename, symbol_name, &value)) == 0)
            *symbol_value = value + address;
    }

    return rv;
}

