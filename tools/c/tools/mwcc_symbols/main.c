/* mwcc_symbols.exe: a port of python tools/mwcc_trace/symbols.py (function names for mwcceppc.exe from the
 * compiler's own CodeView NB11 debug info). Exit 0; 2 when the exe cannot be read or has no NB11 (one line on
 * stderr); 1 where the Python dies with an uncaught exception (a truncated file, a bad --addr). */
#include "mwcc_symbols.h"

static int run(int argc, char **argv) { return mwcc_symbols_main(argc, argv); }

AVP_MAIN(run, "mwcc_symbols")
