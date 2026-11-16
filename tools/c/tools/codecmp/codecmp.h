/* codecmp: the command line of tools/codecmp.py (main, describe_result). The comparison itself is the shared library
 * (cc_codecmp, cc_listing, cc_elf); this tool is the front end over it:
 *
 *     codecmp <unit> <object> [--asm F] [-s SYM ...] [--no-targets] [--unresolved]
 *
 * prints one block per compiled function that has a retail counterpart (describe_result) and a summary line; exit 1
 * when a function is not exact. permute links this library for codecmp_describe_result. */
#pragma once
#include "avp/avp.h"
#include "cc_codecmp.h"

/* describe_result(name, r, show_unresolved, limit): the text block in cc.py --cmp's format plus the target lines
 * (no final newline). */
AvpStr codecmp_describe_result(AvpArena *a, AvpStr name, const CcFnResult *r, bool show_unresolved, size_t limit);

/* main(argv): the exit code. */
int codecmp_main(int argc, char **argv);
