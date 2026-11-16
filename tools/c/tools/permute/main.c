/* permute.exe: the front end of tools/c/tools/permute (a port of python tools/permute.py). */
#include "permute.h"

static int run(int argc, char **argv) { return permute_main(argc, argv); }

AVP_MAIN(run, "permute")
