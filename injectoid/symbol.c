#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <elf.h>
#include <errno.h>

#include "symbol.h"


static ssize_t read_strtab(FILE *fp, elf_shdr *shdr, char **datap)
{
    elf_word sh_size;
    long cur_off;
    char *data;


    sh_size = shdr->sh_size;

    if((size_t)sh_size > SIZE_MAX - 1)
    {
        fprintf(stderr, "read_strtab: %s", strerror(EFBIG));
        goto _ret;
    }


    cur_off = ftell(fp);

    if(fseek(fp, shdr->sh_offset, SEEK_SET) != 0)
    {
        perror("read_strtab: fseek");
        goto _ret;
    }

    if((data = malloc(sh_size + 1)) == NULL)
    {
        perror("read_strtab: malloc");
        goto _ret;
    }

    if(fread(data, 1, sh_size, fp) != sh_size)
    {
        perror("read_strtab: fread");
        goto _free;
    }

    data[sh_size] = 0;

    if(fseek(fp, cur_off, SEEK_SET) != 0)
    {
        perror("read_strtab: fseek");
        goto _free;
    }

    *datap = data;

    return (ssize_t)sh_size;

_free:
    free(data);

_ret:
    return -1;
}


static int resolve_symbol_from_symtab(FILE *fp, elf_shdr *symtab, char *strtab,
        size_t strtab_size, const char *symname, void **symval)
{
    elf_word i, num_syms;
    elf_sym sym;
    long cur_off;

    int r = -1;

    cur_off = ftell(fp);

    if(fseek(fp, symtab->sh_offset, SEEK_SET) != 0)
    {
        perror("resolve_symbol_from_symtab: fseek");
        goto _ret;
    }

    num_syms = symtab->sh_size / sizeof(elf_sym);

    for(i = 0; i < num_syms; i++)
    {
        if(fread(&sym, sizeof(elf_sym), 1, fp) != 1)
        {
            perror("resolve_symbol_from_symtab: fread");
            goto _ret;
        }

        if(sym.st_name < strtab_size &&
                strcmp(&strtab[sym.st_name], symname) == 0)
        {
            *symval = (void *)sym.st_value;
            break;
        }
    }

    if(fseek(fp, cur_off, SEEK_SET) != 0)
    {
        perror("resolve_symbol_from_symtab: fseek");
        goto _ret;
    }

    if(i < num_syms)
        r = 0;

_ret:
    return r;
}


static int resolve_symbol_from_sections(FILE *fp, elf_shdr *shdrs,
        elf_half num_sects, const char *symname, void **symval)
{
    elf_half i;
    elf_shdr *shdr, *strtab_shdr;
    char *strtab;
    ssize_t strtab_size;

    int r = -1;

    for(i = 0; i < num_sects; i++)
    {
        shdr = &shdrs[i];

        if(shdr->sh_type == SHT_SYMTAB && shdr->sh_link < num_sects)
        {
            strtab_shdr = &shdrs[shdr->sh_link];

            if((strtab_size = read_strtab(fp, strtab_shdr, &strtab)) < 0)
                goto _ret;

            r = resolve_symbol_from_symtab(fp, shdr, strtab, (size_t)strtab_size,
                symname, symval);

            free(strtab);

            if(r == 0)
                goto _ret;
        }

    }

_ret:
    return r;
}


/* Resolve symbol named `symname' from ELF file `filename' and return the symbol's
 * value in `*symval'. Returns 0 on success, or -1 on failure. The following code
 * makes so many assumptions that listing them all here is pointless; we just
 * needed to make it as straightforward and minimal as possible.
 */
int resolve_symbol(const char *filename, const char *symname, void **symval)
{
    FILE *fp;
    elf_ehdr ehdr;
    elf_shdr *shdrs;
    elf_half shnum;

    int r = -1;

    if((fp = fopen(filename, "r")) == NULL)
    {
        perror("resolve_symbol: fopen");
        goto _ret;
    }

    if(fread(&ehdr, sizeof(ehdr), 1, fp) != 1)
    {
        perror("resolve_symbol: fread");
        goto _close;
    }

    if(fseek(fp, ehdr.e_shoff, SEEK_SET) != 0)
    {
        perror("resolve_symbol: fseek");
        goto _close;
    }

    shnum = ehdr.e_shnum;

    if((shdrs = calloc(shnum, sizeof(elf_shdr))) == NULL)
    {
        perror("resolve_symbol: calloc");
        goto _close;
    }

    if(fread(shdrs, sizeof(elf_shdr), shnum, fp) != shnum)
    {
        perror("resolve_symbol: fread");
        goto _free;
    }

    r = resolve_symbol_from_sections(fp, shdrs, shnum, symname, symval);

_free:
    free(shdrs);

_close:
    fclose(fp);

_ret:
    return r;
}

