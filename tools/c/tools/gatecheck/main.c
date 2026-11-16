/* gatecheck.exe: the front end of tools/c/tools/gatecheck (a port of python tools/gatecheck.py). */
#include "gatecheck.h"

static int run(int argc, char **argv) { return gc_main(argc, argv); }

AVP_MAIN(run, "gatecheck")
