/* flagsweep.exe: the front end of tools/c/tools/flagsweep (a port of python tools/flagsweep.py). */
#include "flagsweep.h"

static int run(int argc, char **argv) { return flagsweep_main(argc, argv); }

AVP_MAIN(run, "flagsweep")
