/* hashes.exe: tools/hashes.py (names from the string hashes and interned names of the retail binary). */
#include "hashes.h"

static int run(int argc, char **argv) { return hashes_main(argc, argv); }

AVP_MAIN(run, "hashes")
