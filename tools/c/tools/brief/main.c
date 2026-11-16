/* brief.exe: tools/brief.py (the starting briefing for one function or one unit). */
#include "brief.h"

static int run(int argc, char **argv) { return brief_main(argc, argv); }

AVP_MAIN(run, "brief")
