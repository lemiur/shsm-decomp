/* match.exe: the front end of tools/c/tools/match (a port of python tools/match.py). */
#include "match.h"

static int run(int argc, char **argv) { return match_main(argc, argv); }

AVP_MAIN(run, "match")
