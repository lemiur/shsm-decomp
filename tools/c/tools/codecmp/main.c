/* codecmp.exe: the front end of tools/c/tools/codecmp (a port of python tools/codecmp.py's command line). */
#include "codecmp.h"

static int run(int argc, char **argv) { return codecmp_main(argc, argv); }

AVP_MAIN(run, "codecmp")
