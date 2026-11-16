/* witness.exe: tools/witness.py (the PS2 build as a second-compiler witness). */
#include "witness.h"

int witness_main(int argc, char **argv);

static int run(int argc, char **argv) { return witness_main(argc, argv); }

AVP_MAIN(run, "witness")
