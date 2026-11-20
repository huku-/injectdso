/* injectdylib - DSO injection for MacOS X
 * huku <huku@grhack.net>
 *
 * symbol.c - Defines functions for resolving addresses of text segment symbols
 * in FAT/Mach-O binaries and running processes.
 */
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <mach-o/dyld_images.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#include "memory.h"
#include "symbol.h"

/* Default base addresses for 32-bit and 64-bit executables. */
#define BASE_ADDRESS_32BIT 0x1000
#define BASE_ADDRESS_64BIT 0x100000000


/* Read string from remote task. Use a static buffer to make things faster. */
static char *copyin_string(task_t task, mach_vm_address_t address)
{
    static char buf[BUFSIZ];
    mach_vm_size_t size;
    kern_return_t kr;
    char *r = NULL;

    if((kr = mach_vm_read_overwrite(task, address, sizeof(buf) - 1,
            (mach_vm_address_t)&buf, &size)) != KERN_SUCCESS)
    {
        fprintf(stderr, "copyin_string: mach_vm_read_overwrite failed (%d)\n", kr);
        goto _ret;
    }

    buf[size] = 0;

    r = &buf[0];

_ret:
    return r;
}


static mach_vm_address_t find_symbol(task_t task,
        mach_vm_address_t header_addr, const char *symbol_name,
        char *in_shared_cache)
{
    struct mach_header header;
    struct load_command command;
    struct symtab_command symtab;
    char *seg_name;
    kern_return_t kr;
    mach_vm_size_t size;
    mach_vm_address_t symtab_addr, linkedit_addr, text_addr, command_addr, r = -1;

    if((kr = mach_vm_read_overwrite(task, header_addr, sizeof(header),
            (mach_vm_address_t)&header, &size)) != KERN_SUCCESS)
    {
        fprintf(stderr, "find_symbol: mach_vm_read_overwrite failed (%d)\n", kr);
        goto _ret;
    }

    *in_shared_cache = !!((header.flags & MH_DYLIB_IN_CACHE) == MH_DYLIB_IN_CACHE);

    if(header.magic != MH_MAGIC && header.magic != MH_MAGIC_64)
    {
        fprintf(stderr, "Invalid header magic at %p, skipping it\n",
            (void *)header_addr);
        goto _ret;
    }

    /* Locate symbol table, __TEXT and __LINKEDIT segments. */
    symtab_addr = linkedit_addr = text_addr = -1;
    command_addr = header_addr + (header.magic == MH_MAGIC_64 ?
        sizeof(struct mach_header_64) : sizeof(struct mach_header));

    for(uint32_t i = 0; i < header.ncmds; i++)
    {
        if((kr = mach_vm_read_overwrite(task, command_addr, sizeof(command),
                (mach_vm_address_t)&command, &size)) != KERN_SUCCESS)
        {
            fprintf(stderr, "find_symbol: mach_vm_read_overwrite failed (%d)\n", kr);
            goto _ret;
        }

        if(command.cmd == LC_SYMTAB)
        {
            symtab_addr = command_addr;
        }
        else if(command.cmd == LC_SEGMENT || command.cmd == LC_SEGMENT_64)
        {
            seg_name = copyin_string(task, command_addr + sizeof(command));
            if(seg_name)
            {
                if(strcmp(seg_name, SEG_TEXT) == 0)
                    text_addr = command_addr;
                else if(strcmp(seg_name, SEG_LINKEDIT) == 0)
                    linkedit_addr = command_addr;
            }
        }

        if(command.cmdsize < sizeof(command))
            break;
        command_addr += command.cmdsize;
    }

    if(symtab_addr == -1 || linkedit_addr == -1 || text_addr == -1)
    {
        fprintf(stderr, "Invalid Mach-O image header, skipping\n");
        goto _ret;
    }

    if((kr = mach_vm_read_overwrite(task, symtab_addr, sizeof(symtab),
            (mach_vm_address_t)&symtab, &size)) != KERN_SUCCESS)
    {
        fprintf(stderr, "find_symbol: mach_vm_read_overwrite failed (%d)\n", kr);
        goto _ret;
    }

    if(header.magic == MH_MAGIC_64)
    {
        struct segment_command_64 linkedit;
        struct segment_command_64 text;
        struct nlist_64 sym;

        if((kr = mach_vm_read_overwrite(task, linkedit_addr, sizeof(linkedit),
                (mach_vm_address_t)&linkedit, &size)) != KERN_SUCCESS)
        {
            fprintf(stderr, "find_symbol: mach_vm_read_overwrite failed (%d)\n", kr);
            goto _ret;
        }

        if((kr = mach_vm_read_overwrite(task, text_addr, sizeof(text),
                (mach_vm_address_t)&text, &size)) != KERN_SUCCESS)
        {
            fprintf(stderr, "find_symbol: mach_vm_read_overwrite failed (%d)\n", kr);
            goto _ret;
        }

        uint64_t file_slide = linkedit.vmaddr - text.vmaddr - linkedit.fileoff;
        uint64_t strings = header_addr + symtab.stroff + file_slide;
        uint64_t sym_addr = header_addr + symtab.symoff + file_slide;

        for(uint32_t i = 0; i < symtab.nsyms; i++)
        {
            if((kr = mach_vm_read_overwrite(task, sym_addr, sizeof(sym),
                    (mach_vm_address_t)&sym, &size)) != KERN_SUCCESS)
            {
                fprintf(stderr, "find_symbol: mach_vm_read_overwrite failed (%d)\n", kr);
                goto _ret;
            }

            if(sym.n_value)
            {
                char *symname = copyin_string(task, strings + sym.n_un.n_strx);

                /* Ignore the leading "_" character in a symbol name if any. */
                if(symname && strcmp(*symname == '_' ? symname + 1 : symname,
                        symbol_name) == 0)
                {
                    r = (mach_vm_address_t)sym.n_value;
                    break;
                }
            }

            sym_addr += size;
        }
    }
    else
    {
        struct segment_command linkedit;
        struct segment_command text;
        struct nlist sym;

        if((kr = mach_vm_read_overwrite(task, linkedit_addr, sizeof(linkedit),
                (mach_vm_address_t)&linkedit, &size)) != KERN_SUCCESS)
        {
            fprintf(stderr, "find_symbol: mach_vm_read_overwrite failed (%d)\n", kr);
            goto _ret;
        }

        if((kr = mach_vm_read_overwrite(task, text_addr, sizeof(text),
                (mach_vm_address_t)&text, &size)) != KERN_SUCCESS)
        {
            fprintf(stderr, "find_symbol: mach_vm_read_overwrite failed (%d)\n", kr);
            goto _ret;
        }

        uint32_t file_slide = linkedit.vmaddr - text.vmaddr - linkedit.fileoff;
        uint32_t strings = header_addr + symtab.stroff + file_slide;
        uint32_t sym_addr = header_addr + symtab.symoff + file_slide;

        for(uint32_t i = 0; i < symtab.nsyms; i++)
        {
            if((kr = mach_vm_read_overwrite(task, sym_addr, sizeof(sym),
                    (mach_vm_address_t)&sym, &size)) != KERN_SUCCESS)
            {
                fprintf(stderr, "find_symbol: mach_vm_read_overwrite failed (%d)\n", kr);
                goto _ret;
            }

            if(sym.n_value)
            {
                char *symname = copyin_string(task, strings + sym.n_un.n_strx);

                /* Ignore the leading "_" character in a symbol name if any. */
                if(symname && strcmp(*symname == '_' ? symname + 1 : symname,
                        symbol_name) == 0)
                {
                    r = (mach_vm_address_t)sym.n_value;
                    break;
                }
            }

            sym_addr += size;
        }
    }

_ret:
    return r;
}


static int find_base_address(task_t task, const char *filename,
        mach_vm_address_t *base_address, uint64_t *shared_cache_slide)
{
    char path[PATH_MAX];
    struct task_dyld_info dyld_info;
    struct dyld_all_image_infos image_infos;
    struct dyld_image_info *image_info;
    mach_vm_size_t size;
    kern_return_t kr;
    mach_msg_type_number_t task_sz, all_size = 0;
    int r = -1;

    task_sz = TASK_DYLD_INFO_COUNT;
    if((kr = task_info(task, TASK_DYLD_INFO, (task_info_t)&dyld_info,
            &task_sz)) != KERN_SUCCESS)
    {
        fprintf(stderr, "find_base_address: task_info failed (%d)\n", kr);
        goto _ret;
    }

    if((kr = mach_vm_read_overwrite(task, dyld_info.all_image_info_addr,
            sizeof(image_infos), (mach_vm_address_t)&image_infos,
            &size)) != KERN_SUCCESS)
    {
        fprintf(stderr, "find_base_address: mach_vm_read_overwrite failed (%d)\n", kr);
        goto _ret;
    }

    *shared_cache_slide = image_infos.sharedCacheSlide;


    size = image_infos.infoArrayCount * sizeof(*image_info);
    if((kr = mach_vm_read(task, (mach_vm_address_t)image_infos.infoArray, size,
            (vm_offset_t *)&image_info, &all_size)) != KERN_SUCCESS)
    {
        fprintf(stderr, "find_base_address: mach_vm_read failed (%d)\n", kr);
        goto _ret;
    }

    for(uint32_t i = 0; i < image_infos.infoArrayCount; i++)
    {
        if((kr = mach_vm_read_overwrite(task,
                (mach_vm_address_t)image_info[i].imageFilePath, sizeof(path) - 1,
                (mach_vm_address_t)&path[0], &size)) != KERN_SUCCESS)
        {
            fprintf(stderr, "find_base_address: mach_vm_read_overwrite failed (%d)\n", kr);
            goto _free;
        }

        path[size] = 0;

        if(strcmp(path, filename) == 0)
        {
            *base_address = (mach_vm_address_t)image_info[i].imageLoadAddress;
            r = 0;
            goto _free;
        }
    }

_free:
    mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)image_info, all_size);

_ret:
    return r;
}


int resolve_symbol_runtime(task_t task, const char *filename,
        const char *symbol_name, mach_vm_address_t *symbol_addrp)
{
    mach_vm_address_t base_address, symbol_addr;
    char in_shared_cache;
    int r = -1;

    uint64_t shared_cache_slide = 0x0;
    if(find_base_address(task, filename, &base_address, &shared_cache_slide) != 0)
        goto _ret;

    if((symbol_addr = find_symbol(task, base_address, symbol_name,
            &in_shared_cache)) == -1)
    {
        fprintf(stderr, "resolve_symbol_runtime: Failed to locate symbol %s\n",
            symbol_name);
        goto _ret;
    }

    if(!in_shared_cache)
    {
        /* If not in shared cache, add the base address. */
        if(base_address > BASE_ADDRESS_64BIT && symbol_addr < BASE_ADDRESS_64BIT)
        {
            /* 64-bit target. */
            symbol_addr += base_address;
        }
        if(base_address < BASE_ADDRESS_64BIT && symbol_addr < BASE_ADDRESS_32BIT)
        {
            /* 32-bit target. */
            symbol_addr += base_address;
        }
    }

    if(in_shared_cache && symbol_addr > 0)
        symbol_addr += shared_cache_slide;
    *symbol_addrp = symbol_addr;

    r = 0;

_ret:
    return r;
}
