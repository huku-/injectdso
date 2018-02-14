#ifndef _SYMBOL_H_
#define _SYMBOL_H_

#if defined(__arm__)
typedef Elf32_Word elf_word;
typedef Elf32_Half elf_half;
typedef Elf32_Ehdr elf_ehdr;
typedef Elf32_Shdr elf_shdr;
typedef Elf32_Sym elf_sym;
#elif defined(__aarch64__)
typedef Elf64_Word elf_word;
typedef Elf64_Half elf_half;
typedef Elf64_Ehdr elf_ehdr;
typedef Elf64_Shdr elf_shdr;
typedef Elf64_Sym elf_sym;
#else
#error "Unsupported architecture"
#endif

int resolve_symbol(const char *, const char *, void **);

#endif /* _SYMBOL_H_ */
