/* injectdylib - DSO injection for MacOS X
 * huku <huku@grhack.net>
 *
 * symbol.c - Defines functions for resolving addresses of text segment symbols
 * in FAT/Mach-O binaries and running processes.
 */
#include "includes.h"
#include "memory.h"
#include "symbol.h"



/* Resolve symbol `symbol_name' in Mach-O file `fp' and return its value in
 * `symbol_value'.
 */
static int resolve_symbol_macho(FILE *fp, const char *symbol_name,
        vm_address_t *symbol_value)
{
    mach_header_t mh;
    segment_command_t *segment, segments[MAX_NUM_SEGMENTS];
    int num_segments, num_sections, segment_map[MAX_NUM_SECTIONS];
    struct symtab_command symtab;

    nlist_t nl;
    uint32_t i, j, cmd, cmdsize;
    char *strtab;
    int rv = 0;

    long offset = ftell(fp);

    fread(&mh, sizeof(mach_header_t), 1, fp);
    memset(&symtab, 0, sizeof(struct symtab_command));
    for(i = 0, num_segments = 0, num_sections = 0; i < mh.ncmds; i++)
    {
        fread(&cmd, sizeof(uint32_t), 1, fp);
        fread(&cmdsize, sizeof(uint32_t), 1, fp);
        fseek(fp, -2 * sizeof(uint32_t), SEEK_CUR);

        switch(cmd)
        {
            case LC_SEGMENT_TYPE:
                if(num_segments < MAX_NUM_SEGMENTS)
                {
                    segment = &segments[num_segments];
                    fread(segment, sizeof(segment_command_t), 1, fp);

                    for(j = 0; j < segment->nsects &&
                            num_sections < MAX_NUM_SECTIONS; j++)
                        /* Map section number to segment number. */
                        segment_map[num_sections++] = num_segments;
                    num_segments++;
                }

                /* Skip section headers following the segment header. */
                fseek(fp, cmdsize - sizeof(segment_command_t), SEEK_CUR);
                break;

            case LC_SYMTAB:
                fread(&symtab, sizeof(struct symtab_command), 1, fp);
                break;

            default:
                fseek(fp, cmdsize, SEEK_CUR);
                break;
        }
    }

    /* Make sure we did find a symbol table in the file and its string table
     * size is non-zero.
     */
    if(symtab.cmd != LC_SYMTAB || symtab.strsize == 0)
    {
        rv = -EINVAL;
        goto _exit;
    }

    if((strtab = malloc(symtab.strsize)) == NULL)
    {
        rv = -errno;
        goto _exit;
    }

    /* Read string table of symbol names. */
    fseek(fp, offset + symtab.stroff, SEEK_SET);
    fread(strtab, sizeof(char), symtab.strsize, fp);

    /* Last byte of a string table should be zero, otherwise we might end up
     * reading invalid memory in `strcmp()' below.
     */
    if(strtab[symtab.strsize - 1] != 0)
    {
        free(strtab);
        rv = -EINVAL;
        goto _exit;
    }

    /* Read the symbol table entries. */
    fseek(fp, offset + symtab.symoff, SEEK_SET);
    for(i = 0; i < symtab.nsyms; i++)
    {
        fread(&nl, sizeof(nlist_t), 1, fp);

        if(nl.n_un.n_strx < symtab.strsize &&
                strcmp(&strtab[nl.n_un.n_strx], symbol_name) == 0)
        {
            *symbol_value = nl.n_value;

            /* For symbol's of type `N_SECT', return the relative offset of the
             * symbol from the start of the container segment.
             */
            if((nl.n_type & N_TYPE) == N_SECT && nl.n_sect - 1 < num_sections)
                *symbol_value -= segments[segment_map[nl.n_sect - 1]].vmaddr;
            break;
        }
    }

    free(strtab);

    if(i >= symtab.nsyms)
        rv = -EINVAL;

_exit:
    return rv;
}

/* Resolve symbol `symbol_name' in FAT binary `fp' and return its value in
 * `symbol_value'.
 */
static int resolve_symbol_fat(FILE *fp, const char *symbol_name,
        vm_address_t *symbol_value)
{
    struct fat_header fh;
    struct fat_arch fa;
    uint32_t i;
    long offset;
    int rv = 0;


    fread(&fh, sizeof(struct fat_header), 1, fp);
    for(i = 0; i < ntohl(fh.nfat_arch); i++)
    {
        fread(&fa, sizeof(struct fat_arch), 1, fp);
        if(htonl(fa.cputype) == CPU_TYPE)
        {
            offset = ftell(fp);
            fseek(fp, htonl(fa.offset), SEEK_SET);
            if((rv = resolve_symbol_macho(fp, symbol_name, symbol_value)) != 0)
                break;
            fseek(fp, offset, SEEK_SET);
        }
    }
    return rv;
}

/* Resolve symbol `symbol_name' in FAT or Mach-O file `filename' and return its
 * value in `symbol_value'.
 */
int resolve_symbol(const char *filename, const char *symbol_name,
        vm_address_t *symbol_value)
{
    FILE *fp;
    uint32_t magic;
    int rv = 0;

    *symbol_value = 0;

    if((fp = fopen(filename, "r")) != NULL)
    {
        fread(&magic, sizeof(uint32_t), 1, fp);
        fseek(fp, -sizeof(uint32_t), SEEK_CUR);

        if(magic == FAT_MAGIC || magic == FAT_CIGAM)
            rv = resolve_symbol_fat(fp, symbol_name, symbol_value);
        else if(magic == MH_MAGIC_1 || magic == MH_MAGIC_2)
            rv = resolve_symbol_macho(fp, symbol_name, symbol_value);
        else
            rv = -EINVAL;
        fclose(fp);
    }
    else
    {
        perror("fopen");
        rv = -errno;
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

