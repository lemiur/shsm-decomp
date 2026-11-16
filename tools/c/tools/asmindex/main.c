/* asmindex.exe: python -m tools.common index (build or refresh build/index/asm.sqlite). */
#include "asmindex_cli.h"

static int run(int argc, char **argv) { return asmindex_main(argc, argv); }

AVP_MAIN(run, "asmindex")
