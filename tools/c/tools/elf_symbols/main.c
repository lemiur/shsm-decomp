/* elf_symbols.exe: the front end of tools/c/tools/elf_symbols (a port of python tools/scripts/elf_symbols.py). */
#include "elf_symbols.h"

static int run(int argc, char **argv) { return elf_symbols_main(argc, argv); }

AVP_MAIN(run, "elf_symbols")
