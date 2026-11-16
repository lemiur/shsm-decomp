/* eq_cli: the command line of tools/eqcheck.py (L2258-2495): _work_dir, object_for, _ret_regs, cmd_one,
 * _select_units, _scan_unit, _reason_key, cmd_scan, cmd_selfcheck and main.
 *
 *     eqcheck <unit> <symbol> [--built | --file F] [--ret REGS] [--explain]   one function (cmd_one)
 *     eqcheck scan [--unit U]... [--lib L] [--status S] [-j N] [--ret REGS] [--built]
 *     eqcheck selfcheck [--unit U]... [--lib L] [--status S] [-j N] [--ret REGS] [--sample N] [--identity]
 *
 * Output text, --json documents, build/eqcheck.json and exit codes are the Python's (timings aside: the "seconds"
 * values and the "%.2f s" / "in %.1f s" figures measure the C run). scan and selfcheck run their units on threads
 * (DEVIATIONS D-eqcheck-8). */
#pragma once
#include "avp/avp.h"
#include "eq_prove.h"
#include "shsm_repo.h"
#include "shsm_units.h"

/* _ret_regs(text): EQ_RET_DEFAULT for NULL, "" or "auto"; n 0 for "void" (or a text of commas and blanks only);
 * else the comma-separated names, stripped, each in REG_LOCS. Raises SHSM_EXC_USAGE "--ret: unknown register X".
 * The names are allocated in a. */
EqRet eq_ret_regs(AvpArena *a, const char *text);

/* _reason_key(f) of a scan record (an object with "unmodelled" and "reason"): "unmodelled: <first word>" when
 * unmodelled is set, else the reason with every hex and decimal number replaced by N, cut at " [" and to 90 code
 * points. In arena a. */
AvpStr eq_reason_key(AvpArena *a, const AvpJson *f);

/* _work_dir(repo): <root>\build\tmp\eqcheck */
const char *eq_work_dir(AvpArena *a, const ShsmRepo *r);

/* object_for(unit, repo, built, source, tag): the object to compare, and the listing directory (NULL = None):
 * built: the last build's object (SHSM_EXC_USAGE "no built object for <unit> (run the build first or drop
 * --built)" when absent) with its listing in build/tmp/eqcheck/listing/<unit path>; else a private compile of
 * source (NULL: the unit's own) into build/tmp/eqcheck/obj<tag>/<unit path> (SHSM_EXC_USAGE "<unit> has no source",
 * "compile of <src> failed: <first error or the last three output lines>"). */
typedef struct {
    const char *obj;
    const char *out_dir;
} EqObject;
EqObject eq_object_for(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u, bool built, const char *source,
                       const char *tag);

/* main(argv): argv[0] is the program; returns the exit code. */
int eq_main(int argc, char **argv);
