/* pick.exe: tools/pick.py (ranked work lists for the matching campaign). */
#include "pick.h"

static int run(int argc, char **argv) { return pick_main(argc, argv); }

AVP_MAIN(run, "pick")
