/* decomp.exe: tools/decomp.py (Ghidra decompiler listings of the retail functions: reading aids, never source). */
#include "decomp.h"

static int run(int argc, char **argv) { return decomp_main(argc, argv); }

AVP_MAIN(run, "decomp")
