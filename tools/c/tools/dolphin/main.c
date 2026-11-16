/* dolphin.exe: the front end of tools/c/tools/dolphin (a port of python tools/dolphin.py). */
#include "avp/base.h"
#include "dolphin.h"

static int run(int argc, char **argv) { return dolphin_main(argc, argv); }

AVP_MAIN(run, "dolphin")
