/* neutral.exe: the front end of tools/c/tools/neutral (a port of python tools/neutral.py). */
#include "neutral.h"

static int run(int argc, char **argv) { return neutral_main(argc, argv); }

AVP_MAIN(run, "neutral")
