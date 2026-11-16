/* wall.exe: the front end of tools/c/tools/wall (a port of python tools/wall.py). */
#include "wall.h"

static int run(int argc, char **argv) { return wall_main(argc, argv); }

AVP_MAIN(run, "wall")
