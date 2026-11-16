/* unit.exe: the front end of tools/c/tools/unit (a port of python tools/unit.py). */
#include "unit_cmd.h"

static int run(int argc, char **argv) { return unit_main(argc, argv); }

AVP_MAIN(run, "unit")
