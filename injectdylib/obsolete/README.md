# Obsolete code

huku &lt;[huku@grhack.net](@grhack.net)&gt;


## Symbol resolver

Files **symbol.c** and **symbol.h** contain the old, and now unused, symbol
resolver for Mach-O and FAT files. The new resolver, which works for publicly
exported symbols only (those that appear next to an uppercase letter in the
output produced by **nm**), implements the exact same API and, as such, it's
fully compatible with the old one. However, the new version is based on
**dlopen()** and **dladdr()** and is, thus, more reliable.

If you want to hack the source of **injectdylib** and work with private symbols,
just replace **symbol.c** and **symbol.h** with the corresponding old versions
found in this directory and have fun.

