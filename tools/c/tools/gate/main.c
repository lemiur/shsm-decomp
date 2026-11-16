/* gate.exe: the front end of tools/c/tools/gate (a port of python tools/gate.py). */
#include "gate.h"

static int run(int argc, char **argv) { return gate_main(argc, argv, NULL); }

AVP_MAIN(run, "gate")
