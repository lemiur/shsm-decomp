/* A port of tools/scripts/elf_symbols.py: ELF32 little-endian MIPS section/symbol dumper with a compact GNU v2
 * demangler and a string-mining fallback. */
#pragma once
#include "avp/avp.h"

int elf_symbols_main(int argc, char **argv);

/* demangle_v2(sym): the demangled name, or false (Python None). `sym` holds the latin-1 bytes of the symbol; the
 * result holds latin-1 bytes too (the components of the symbol). */
bool elf_demangle_v2(AvpArena *a, AvpStr sym, AvpStr *out);

/* detect_scheme(names): "itanium", "gnu-v2" or "none". */
const char *elf_detect_scheme(const AvpStr *names, size_t n);

/* mine_strings(data): source-path strings, A::B::member strings and class-name-like literals, each in first-seen
 * order without repeats. */
void elf_mine_strings(AvpArena *a, AvpStr data, AvpStrVec *paths, AvpStrVec *quals, AvpStrVec *cnames);
