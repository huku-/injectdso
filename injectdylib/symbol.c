/* injectdylib - DSO injection for MacOS X
 * huku <huku@grhack.net>
 *
 * symbol.c - Defines functions for resolving addresses of text segment symbols
 * in FAT/Mach-O binaries and running processes.
 */
#include "includes.h"
#include "memory.h"
#include "symbol.h"


/** This magic Mach-O header flag implies that image was loaded from dyld shared cache */
#define kImageFromSharedCacheFlag 0x80000000
/** @see _copyin_string() */
#define kRemoteStringBufferSize 2048
/** Default base addresses for 32- and 64-bit executables */
#define ki386DefaultBaseAddress 0x1000
#define kx86_64DefaultBaseAddress 0x100000000

static char *_copyin_string(task_t task, mach_vm_address_t pointer)
{
	assert(pointer > 0);
	int err = KERN_FAILURE;

	/* Since calls to mach_vm_read_overwrite() are expensive we'll just use
	 * a rather big buffer insead of reading char-by-char.
	 */
	// FIXME: what about the size of this buffer?
	// Users can requst symbols with very long names (e.g. C++ mangled method names, etc)
	char buf[kRemoteStringBufferSize] = {0};
	mach_vm_size_t sample_size = sizeof(buf);
	err = mach_vm_read_overwrite(task, pointer, sample_size,
								 (mach_vm_address_t)&buf, &sample_size);
	assert(err == KERN_SUCCESS);
	buf[kRemoteStringBufferSize-1] = '\0';

	char *result = strdup(buf);
	return result;
}


mach_vm_address_t find_symbol_in_remote_image(task_t task, mach_vm_address_t remote_header, const char *symbol_name, bool *imageFromSharedCache)
{
	assert(symbol_name);
	assert(imageFromSharedCache);
	int err = KERN_FAILURE;

	if (remote_header == 0) {
		return 0;
	}

	mach_vm_size_t size = sizeof(struct mach_header);
	struct mach_header header = {0};
	err = mach_vm_read_overwrite(task, remote_header, size, (mach_vm_address_t)&header, &size);

	bool sixtyfourbit = (header.magic == MH_MAGIC_64);
	*imageFromSharedCache = ((header.flags & kImageFromSharedCacheFlag) == kImageFromSharedCacheFlag);

	/* We don't support anything but i386 and x86_64 */
	if (header.magic != MH_MAGIC && header.magic != MH_MAGIC_64) {
		printf("ERROR: found image with unsupported architecture at %p, skipping it.\n", (void *)remote_header);
		return 0;
	}

	/**
	 * Let's implement some nlist()
	 */
	mach_vm_address_t symtab_addr = 0;
	mach_vm_address_t linkedit_addr = 0;
	mach_vm_address_t text_addr = 0;

	size_t mach_header_size = sizeof(struct mach_header);
	if (sixtyfourbit) {
		mach_header_size = sizeof(struct mach_header_64);
	}
	mach_vm_address_t command_addr = remote_header + mach_header_size;
	struct load_command command = {0};
	size = sizeof(command);

	for (uint32_t i = 0; i < header.ncmds; i++) {
		err = mach_vm_read_overwrite(task, command_addr, size, (mach_vm_address_t)&command, &size);

		if (command.cmd == LC_SYMTAB) {
			symtab_addr = command_addr;
		} else if (command.cmd == LC_SEGMENT || command.cmd == LC_SEGMENT_64) {
			/* struct load_command only has two fields (cmd & cmdsize), while its "child" type
			 * struct segment_command has way more fields including `segname` at index 3, so we just
			 * pretend that we have a real segment_command and skip first two fields away */
			size_t segname_field_offset = sizeof(command);
			mach_vm_address_t segname_addr = command_addr + segname_field_offset;
			char *segname = _copyin_string(task, segname_addr);
			if (0 == strcmp(SEG_TEXT, segname)) {
				text_addr = command_addr;
			} else if (0 == strcmp(SEG_LINKEDIT, segname)) {
				linkedit_addr = command_addr;
			}
			free(segname);
		}
		// go to next load command
		command_addr += command.cmdsize;
	}

	if (!symtab_addr || !linkedit_addr || !text_addr) {
		printf("Invalid Mach-O image header, skipping...\n");
		return 0;
	}

	struct symtab_command symtab = {0};
	size = sizeof(struct symtab_command);
	err = mach_vm_read_overwrite(task, symtab_addr, size, (mach_vm_address_t)&symtab, &size);

	// FIXME: find a way to remove the copypasted code below
	// These two snippets share all the logic, but differs in structs and integers
	// they use for reading the data from a target process (32- or 64-bit layout).
	if (sixtyfourbit) {
		struct segment_command_64 linkedit = {0};
		size = sizeof(struct segment_command_64);
		err = mach_vm_read_overwrite(task, linkedit_addr, size,
									 (mach_vm_address_t)&linkedit, &size);
		struct segment_command_64 text = {0};
		err = mach_vm_read_overwrite(task, text_addr, size, (mach_vm_address_t)&text, &size);

		uint64_t file_slide = linkedit.vmaddr - text.vmaddr - linkedit.fileoff;
		uint64_t strings = remote_header + symtab.stroff + file_slide;
		uint64_t sym_addr = remote_header + symtab.symoff + file_slide;

		for (uint32_t i = 0; i < symtab.nsyms; i++) {
			struct nlist_64 sym = {{0}};
			size = sizeof(struct nlist_64);
			err = mach_vm_read_overwrite(task, sym_addr, size, (mach_vm_address_t)&sym, &size);
			sym_addr += size;

			if (!sym.n_value) continue;

			uint64_t symname_addr = strings + sym.n_un.n_strx;
			char *symname = _copyin_string(task, symname_addr);
			/* Ignore the leading "_" character in a symbol name */
			if (0 == strcmp(symbol_name, symname+1)) {
				free(symname);
				return (mach_vm_address_t)sym.n_value;
			}
			free(symname);
		}
	} else {
		struct segment_command linkedit = {0};
		size = sizeof(struct segment_command);
		err = mach_vm_read_overwrite(task, linkedit_addr, size,
									 (mach_vm_address_t)&linkedit, &size);
		struct segment_command text = {0};
		err = mach_vm_read_overwrite(task, text_addr, size, (mach_vm_address_t)&text, &size);

		uint32_t file_slide = linkedit.vmaddr - text.vmaddr - linkedit.fileoff;
		uint32_t strings = (uint32_t)remote_header + symtab.stroff + file_slide;
		uint32_t sym_addr = (uint32_t)remote_header + symtab.symoff + file_slide;

		for (uint32_t i = 0; i < symtab.nsyms; i++) {
			struct nlist sym = {{0}};
			size = sizeof(struct nlist);
			err = mach_vm_read_overwrite(task, sym_addr, size, (mach_vm_address_t)&sym, &size);
			sym_addr += size;

			if (!sym.n_value) continue;

			uint32_t symname_addr = strings + sym.n_un.n_strx;
			char *symname = _copyin_string(task, symname_addr);
			/* Ignore the leading "_" character in a symbol name */
			if (0 == strcmp(symbol_name, symname+1)) {
				free(symname);
				return (mach_vm_address_t)sym.n_value;
			}
			free(symname);
		}
	}

fail:
	return 0;
}



void find_base_address(task_t task, const char *filename, mach_vm_address_t *base_address, uint64_t *shared_cache_slide) {
	
	struct task_dyld_info dyld_info;
	mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
	if (task_info(task, TASK_DYLD_INFO, (task_info_t)&dyld_info, &count) == KERN_SUCCESS)
	{
		mach_msg_type_number_t size = sizeof(struct dyld_all_image_infos);
		//mach_msg_type_number_t dataCnt = (mach_msg_type_number_t)size;
		struct dyld_all_image_infos* infos;
		mach_vm_read(task, dyld_info.all_image_info_addr, size, (vm_offset_t*)&infos, &size);
		
		*shared_cache_slide = infos->sharedCacheSlide;
		
        mach_msg_type_number_t size2 = sizeof(struct dyld_image_info) * infos->infoArrayCount;
		//mach_msg_type_number_t dataCnt2 = (mach_msg_type_number_t)size2;
        struct dyld_image_info* info;
		mach_vm_read(task, (mach_vm_address_t)infos->infoArray, size2, (vm_offset_t*)&info, &size2);
		
        for (int i=0; i < infos->infoArrayCount; i++) {
            mach_msg_type_number_t size3 = PATH_MAX;
			//mach_msg_type_number_t dataCnt3 = (mach_msg_type_number_t)size3;

			char* path;
			mach_vm_read(task, (mach_vm_address_t)info[i].imageFilePath, size, (vm_offset_t*)&path, &size3);
			
			if(strcmp(path, filename) == 0) {
                *base_address = (mach_vm_address_t)info[i].imageLoadAddress;
                break;
            }
        }
	}
}


int resolve_symbol_runtime(task_t task, const char *filename,
        const char *symbol_name, mach_vm_address_t *symbol_addr)
{
    mach_vm_address_t base_address;

	uint64_t shared_cache_slide = 0x0;
	find_base_address(task, filename, &base_address, &shared_cache_slide);
	if(base_address == 0x0 ) {
		fprintf(stderr, "find_base_address failed: %s\n", symbol_name);
		return 1;
	}

	bool imageFromSharedCache;
	*symbol_addr = find_symbol_in_remote_image(task, base_address, symbol_name, &imageFromSharedCache);
	
	if (!imageFromSharedCache) {
		/**
		 * On some setups dyld shared cache doesn't contain some system libraries.
		 * In this case we have to append a base_address+ASLR value to the result.
		 */
		if (base_address > kx86_64DefaultBaseAddress && *symbol_addr < kx86_64DefaultBaseAddress) {
			/* x86_64 target */
			*symbol_addr += base_address;
		}
		if (base_address < kx86_64DefaultBaseAddress && *symbol_addr < ki386DefaultBaseAddress) {
			/* i386 target */
			*symbol_addr += base_address;
		}
	}
	if (imageFromSharedCache && *symbol_addr > 0) {
		*symbol_addr += shared_cache_slide;
	}

	
	
	if (*symbol_addr==0x0) return 1;
	else return 0;
}
