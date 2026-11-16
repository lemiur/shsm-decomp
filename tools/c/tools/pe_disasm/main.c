/* pe_disasm.exe: a port of python tools/mwcc_trace/pe_disasm.py (strings, xref, func, callers, callees over a Win32
 * x86 PE). Exit 0; 1 with the module docstring on stdout for a short command line or an unknown command; 1 with a
 * one-line traceback on stderr where the Python dies with an uncaught exception. */
#include "pe_disasm.h"

static int run(int argc, char **argv) { return pe_disasm_main(argc, argv); }

AVP_MAIN(run, "pe_disasm")
