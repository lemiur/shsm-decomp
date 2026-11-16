/* mwcc_symbols: the command-line half of tools/mwcc_trace/symbols.py (the table itself is lib/shsm_pe). */
#pragma once
#include "avp/avp.h"
#include "shsm_pe.h"

/* The Python's ROOT (the checkout, backslashes) and DEFAULT_EXE. */
const char *mwcc_symbols_root(AvpArena *a);
const char *mwcc_symbols_default_exe(AvpArena *a, const char *root);

/* '0x%x' % v as the Python formats it (negative: "0x-5"). */
char *mwcc_symbols_hex(AvpArena *a, i64 v);

/* check_all(): one line per every mwcceppc.exe under tools/compilers (os.walk order, sorted file names) into out. A table
 * that fails other than with NoDebugInfo throws (the Python's traceback); the lines before it are in out. */
void mwcc_symbols_check_all(AvpArena *a, const char *root, AvpBuf *out);

/* The --dump file text ("va\tsize\tname\tmodule\n" then one line per function, '\n' line ends). */
AvpStr mwcc_symbols_dump_text(AvpArena *a, const ShsmPeSymbols *t);

/* One --addr line (without the newline) for an address already parsed; throws IndexError for an empty table. */
AvpStr mwcc_symbols_addr_line(AvpArena *a, const ShsmPeSymbols *t, i64 va);

/* The --name lines of one name (each ending in '\n'). */
void mwcc_symbols_name_lines(AvpArena *a, const ShsmPeSymbols *t, AvpStr name, AvpBuf *out);

/* The whole tool: argv after the program name; stdout/stderr written directly; returns the exit code. */
int mwcc_symbols_main(int argc, char **argv);
