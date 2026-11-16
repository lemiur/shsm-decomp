/* levers.exe: the front end of tools/c/tools/levers (a port of python tools/levers.py). */
#include "levers_search.h"

static int run(int argc, char **argv) { return lv_main(argc, argv); }

AVP_MAIN(run, "levers")
