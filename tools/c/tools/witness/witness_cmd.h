/* witness_cmd: the Witness methods (header, resolve, resolve_ps2, wname, ps2_label, helpers) and the subcommands of
 * tools/witness.py (cmd_of, cmd_inlined, cmd_helpers, cmd_order, cmd_ps2, cmd_stats), with known_check and the
 * known-helper site scan (witness_known.c). */
#ifndef WITNESS_CMD_H
#define WITNESS_CMD_H

#include "witness_db.h"

#include "shsm_cli.h"

/* the parsed arguments the subcommands read */
typedef struct WtArgs {
    const char *cmd;
    const char *function; /* of, inlined */
    const char *address;  /* ps2 */
    const char *range;    /* order: NULL = None */
    const char *lib;      /* helpers, order: NULL = None */
    i64 min_callers, gap;
    bool no_known;
    bool all;
    i64 limit;            /* args.limit (0 = no cap) */
} WtArgs;

/* run(args) once the Witness is open: dispatches to cmd_<cmd>. Output is closed by the caller. */
int wt_cmd(const WtArgs *args, WtWitness *w, ShsmOutput *out);

/* tools/fragments.py known_sites and KNOWN: fragments_known.h (tool_fragments), fragments_known_functions. */

#endif
