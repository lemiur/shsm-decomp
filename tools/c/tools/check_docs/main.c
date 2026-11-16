/* check_docs.exe: the front end of tools/c/tools/check_docs (a port of python tools/check_docs.py). */
#include "check_docs.h"

static int run(int argc, char **argv) { return check_docs_main(argc, argv); }

AVP_MAIN(run, "check_docs")
