/* query.exe: python tools/query.py (questions about the retail binary and the project, from the asm index). */
#include "query.h"

static int run(int argc, char **argv) { return query_main(argc, argv); }

AVP_MAIN(run, "query")
