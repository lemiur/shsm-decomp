/* eqcheck.exe: the front end of tools/c/tools/eqcheck (a port of python tools/eqcheck.py). */
#include "eq_cli.h"

static int run(int argc, char **argv) { return eq_main(argc, argv); }

AVP_MAIN(run, "eqcheck")
