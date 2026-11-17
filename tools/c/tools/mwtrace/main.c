/* mwtrace.exe: traces of the compiler's own decisions for one compile of a unit, read under the debugger.
 *
 *     mwtrace.exe inline <unit> [--function SYM] [--flag "<words>"]... [--drop-flag W]... [--src FILE] [--json]
 *                               [--out DIR]
 *     mwtrace.exe regalloc <unit> --function SYM [--src FILE] [--against FILE2] [--flag "<words>"]...
 *                                 [--drop-flag W]... [--json] [--out DIR]
 *     mwtrace.exe sched <unit> --function SYM [--src FILE] [--against FILE2] [--pair A,B [--block K] [--pass P]]
 *                              [--flag "<words>"]... [--drop-flag W]... [--json] [--out DIR]
 *
 * sched: what the instruction scheduler decided for one function, checked against the object (mwt_sched.h).
 *
 * inline: what the inliner decided at each call site and by which numbers, checked against the object
 * (mwt_inline.h). regalloc: what the register allocator decided for one function, checked against the -sym on
 * object's DWARF and the registers its code saves (mwt_regalloc.h). The shared pieces (the unit's compile under the
 * debugger, the object's calls) are in mwt_run.h, the per-build addresses in mwt_build.h. */
#include "mwt_inline.h"
#include "mwt_regalloc.h"
#include "mwt_sched.h"

static int run(int argc, char **argv) {
    AvpArena *a = avp_arena();
    if (argc >= 2 && !strcmp(argv[1], "inline")) return mwt_inline_main(a, argc - 2, argv + 2);
    if (argc >= 2 && !strcmp(argv[1], "regalloc")) return mwt_regalloc_main(a, argc - 2, argv + 2);
    if (argc >= 2 && !strcmp(argv[1], "sched")) return mwt_sched_main(a, argc - 2, argv + 2);
    bool help = argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"));
    FILE *f = help ? stdout : stderr;
    fprintf(f, "usage: mwtrace.exe inline <unit> [--function SYM] [--flag \"<words>\"]... [--drop-flag W]... [--src FILE]\n"
               "                              [--json] [--out DIR]\n"
               "       mwtrace.exe regalloc <unit> --function SYM [--src FILE] [--against FILE2] [--flag \"<words>\"]...\n"
               "                                [--drop-flag W]... [--json] [--out DIR]\n"
               "       mwtrace.exe sched <unit> --function SYM [--src FILE] [--against FILE2] [--pair A,B [--block K]]\n"
               "                             [--pass P] [--flag \"<words>\"]... [--drop-flag W]... [--json] [--out DIR]\n"
               "\n"
               "Traces the compiler's decisions for one compile of a unit under the debugger. Subcommands:\n"
               "  inline   the inliner's decision at each call site, its rule and numbers, checked against the object\n"
               "           (mwtrace.exe inline --help)\n"
               "  regalloc the register allocator's nodes, numbers and order for one function, checked against the -sym\n"
               "           on DWARF and the prologue; --against compares two source texts (mwtrace.exe regalloc --help)\n"
               "  sched    the instruction scheduler's nodes, edges and each selection's deciding key for one function,\n"
               "           checked against the object; --against, --pair (mwtrace.exe sched --help)\n");
    return help ? 0 : 2;
}

AVP_MAIN(run, "mwtrace")
