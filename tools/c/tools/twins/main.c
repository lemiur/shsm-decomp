/* twins.exe: tools/twins.py (code the binary contains more than once). */
#include "twins.h"

static int run(int argc, char **argv) { return twins_main(argc, argv); }

AVP_MAIN(run, "twins")
