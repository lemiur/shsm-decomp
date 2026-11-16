/* witness.py main() and run(): the parser (the shared options also after the subcommand, cli.add_shared_options),
 * the Witness, and the subcommand inside cli.Output (closed even when the subcommand raises, as the Python's
 * `with` block does). */
#include "witness_cmd.h"

#include <string.h>

static const char DESCRIPTION[] =
    "The PS2 build as a second witness for Wii functions (advisory, never gate input).";

static bool limit_conv(void *ctx, AvpArena *a, AvpStr in, AvpArgValue *out, AvpStr *msg) {
    (void)ctx;
    i64 n = 0;
    if (!shsm_limit_type(a, in, &n, msg)) return false;
    out->kind = AVP_V_INT;
    out->i = n;
    return true;
}

/* cli.add_shared_options(sp, root=True, debug=True): defaults suppressed, so a value given before the subcommand
 * is kept. */
static void add_shared_options(AvpArgParser *p) {
    avp_arg(p, &(AvpArgDesc){.names = "--root", .metavar = "DIR", .dflt = AVP_DEF_SUPPRESS,
                             .help = "checkout to work on (default: the one around the cwd)"});
    avp_arg(p, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE, .dflt = AVP_DEF_SUPPRESS,
                             .help = "print one JSON document instead of text"});
    avp_arg(p, &(AvpArgDesc){.names = "--all", .action = AVP_ARG_STORE_TRUE, .dflt = AVP_DEF_SUPPRESS,
                             .help = "do not cap the output"});
    avp_arg(p, &(AvpArgDesc){.names = "--limit", .type = AVP_T_CUSTOM, .conv = limit_conv, .type_name = "limit_type",
                             .dflt = AVP_DEF_SUPPRESS, .metavar = "N",
                             .help = "cap text output at N lines (default 40; 0 or 'all': no cap)"});
    avp_arg(p, &(AvpArgDesc){.names = "--debug", .action = AVP_ARG_STORE_TRUE, .dflt = AVP_DEF_SUPPRESS,
                             .help = "print tracebacks"});
}

static const char *opt_cz(AvpArena *a, const AvpArgs *args, const char *dest) {
    if (!avp_args_has(args, dest) || avp_args_is_none(args, dest)) return NULL;
    return avp_cz(a, avp_args_str(args, dest));
}

static int run(void *ctx, AvpArgs *args) {
    (void)ctx;
    AvpArena *a = avp_arena();
    WtArgs wa = {0};
    wa.cmd = avp_cz(a, avp_args_str(args, "cmd"));
    wa.all = avp_args_has(args, "all") && avp_args_bool(args, "all");
    wa.limit = avp_args_has(args, "limit") ? avp_args_int(args, "limit") : SHSM_DEFAULT_LIMIT;
    if (strcmp(wa.cmd, "of") == 0 || strcmp(wa.cmd, "inlined") == 0) {
        wa.function = opt_cz(a, args, "function");
    } else if (strcmp(wa.cmd, "ps2") == 0) {
        wa.address = opt_cz(a, args, "address");
    } else if (strcmp(wa.cmd, "helpers") == 0) {
        wa.lib = opt_cz(a, args, "lib");
        wa.min_callers = avp_args_int(args, "min_callers");
        wa.no_known = avp_args_bool(args, "no_known");
    } else if (strcmp(wa.cmd, "order") == 0) {
        wa.range = opt_cz(a, args, "range");
        wa.lib = opt_cz(a, args, "lib");
        wa.gap = avp_args_int(args, "gap");
    }
    const char *root = opt_cz(a, args, "root");
    int jobs = avp_args_is_none(args, "jobs") ? 0 : (int)avp_args_int(args, "jobs");
    WtWitness *w = wt_witness_open(root, opt_cz(a, args, "exe"), avp_args_bool(args, "rebuild"), jobs);
    ShsmOutput *out = shsm_output_open(args, 0, -1, -1); /* cli.Output(args, limit=cli.UNLIMITED) */
    int rc = 0;
    AvpTry t;
    if (AVP_TRY(&t)) {
        rc = wt_cmd(&wa, w, out);
        avp_try_end(&t);
        t.code = 0;
    }
    shsm_output_close(out);
    wt_witness_close(w);
    if (t.code) avp_throw(t.code, "%s", t.msg);
    return rc;
}

int witness_main(int argc, char **argv) {
    AvpArgParser *ap = shsm_cli_parser("witness.exe", DESCRIPTION, true, true, true);
    avp_arg(ap, &(AvpArgDesc){.names = "--rebuild", .action = AVP_ARG_STORE_TRUE,
                              .help = "recompute the witness tables"});
    avp_arg(ap, &(AvpArgDesc){.names = "--exe", .dflt = AVP_DEF_NONE,
                              .help = "the PS2 ELF, or an ISO image to read it from"});
    avp_arg(ap, &(AvpArgDesc){.names = "-j --jobs", .type = AVP_T_INT, .dflt = AVP_DEF_NONE});
    AvpArgSubparsers *sub = avp_args_subparsers(ap, &(AvpArgSubparsersDesc){.dest = "cmd", .required = true});
    AvpArgParser *sps[6];
    sps[0] = avp_args_subparser(
        sub, "stats", &(AvpArgParserDesc){.help = "executable facts, mapping counts per tier and library, validation"});
    sps[1] = avp_args_subparser(
        sub, "of", &(AvpArgParserDesc){.help = "a Wii function's PS2 counterpart and the call-set comparison"});
    avp_arg(sps[1], &(AvpArgDesc){.names = "function"});
    sps[2] = avp_args_subparser(
        sub, "inlined", &(AvpArgParserDesc){.help = "helpers the PS2 build keeps out of line at this Wii site"});
    avp_arg(sps[2], &(AvpArgDesc){.names = "function"});
    sps[3] = avp_args_subparser(
        sub, "helpers", &(AvpArgParserDesc){.help = "PS2 functions with no out-of-line Wii counterpart, by sites"});
    avp_arg(sps[3], &(AvpArgDesc){.names = "--lib", .dflt = AVP_DEF_NONE});
    avp_arg(sps[3], &(AvpArgDesc){.names = "--min-callers", .type = AVP_T_INT, .dflt = AVP_DEF_INT(2),
                                  .help = "Wii sites a helper needs (default 2)"});
    avp_arg(sps[3], &(AvpArgDesc){.names = "--no-known", .action = AVP_ARG_STORE_TRUE,
                                  .help = "skip the check against the known helpers"});
    sps[4] = avp_args_subparser(sub, "order", &(AvpArgParserDesc){.help = "PS2 link order of a Wii range; unit cuts"});
    avp_arg(sps[4], &(AvpArgDesc){.names = "range", .nargs = AVP_NARGS_OPT, .dflt = AVP_DEF_NONE,
                                  .help = "START-END, hex"});
    avp_arg(sps[4], &(AvpArgDesc){.names = "--lib", .dflt = AVP_DEF_NONE});
    avp_arg(sps[4], &(AvpArgDesc){.names = "--gap", .type = AVP_T_INT, .dflt = AVP_DEF_INT(WT_DEFAULT_GAP),
                                  .help = "PS2 rank distance still adjacent (default 8)"});
    sps[5] = avp_args_subparser(
        sub, "ps2", &(AvpArgParserDesc){.help = "listing of one PS2 function, calls and strings mapped to Wii names"});
    avp_arg(sps[5], &(AvpArgDesc){.names = "address"});
    /* subcommand options also accept the shared switches after the subcommand */
    for (int i = 0; i < 6; i++) add_shared_options(sps[i]);
    return shsm_cli_main(ap, argc, argv, run, NULL);
}
