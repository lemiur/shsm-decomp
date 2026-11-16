/* names.exe: tools/names.py (demangle, mangle, find, rename and check symbol names). */
#include "names.h"

static int run(int argc, char **argv) { return names_main(argc, argv); }

AVP_MAIN(run, "names")
