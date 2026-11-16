/* slot.exe: the front end of tools/c/tools/slot (a port of python tools/slot.py). */
#include "slot.h"

static int run(int argc, char **argv) { return slot_main(argc, argv); }

AVP_MAIN(run, "slot")
