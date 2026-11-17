/* solver.exe: the front end of tools/c/tools/solver (the decision solver). */
#include "solver.h"

static int run(int argc, char **argv) { return sv_main(argc, argv); }

AVP_MAIN(run, "solver")
