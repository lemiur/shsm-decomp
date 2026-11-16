/* fragments.exe: tools/fragments.py (repeated parts of functions: candidates for inlined helpers). */
#include "frag.h"

static int run(int argc, char **argv) { return fragments_main(argc, argv); }

AVP_MAIN(run, "fragments")
