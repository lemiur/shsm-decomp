/* fragments.py main() and run(): the parser (the shared options also after the subcommand, cli.add_shared_options). */
#include "frag.h"

#include <stdio.h>
#include <string.h>

static const char DESCRIPTION[] = "Find repeated parts of functions: candidates for inlined helpers.";

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

static void log_stderr(const char *msg) {
    fflush(stdout);
    fprintf(stderr, "%s\n", msg);
    fflush(stderr);
}

static const char *opt_str(AvpArena *a, AvpArgs *args, const char *key) {
    if (!avp_args_has(args, key) || avp_args_is_none(args, key)) return NULL;
    return avp_cz(a, avp_args_str(args, key));
}

static int run(void *ctx, AvpArgs *args) {
    (void)ctx;
    AvpArena *a = avp_arena();
    const char *root = opt_str(a, args, "root");
    AvpStr cmd = avp_args_str(args, "cmd");
    bool as_json = avp_args_has(args, "json") && avp_args_bool(args, "json");
    FragArgs fa;
    memset(&fa, 0, sizeof fa);
    fa.all = avp_args_has(args, "all") && avp_args_bool(args, "all");
    fa.limit = avp_args_has(args, "limit") ? avp_args_int(args, "limit") : SHSM_DEFAULT_LIMIT;
    if (avp_str_eqz(cmd, "list")) {
        fa.lib = opt_str(a, args, "lib");
        fa.min_len = avp_args_int(args, "min_len");
        fa.min_functions = avp_args_int(args, "min_functions");
        fa.level = opt_str(a, args, "level");
        fa.unmatched = avp_args_bool(args, "unmatched");
    } else if (avp_str_eqz(cmd, "show")) {
        fa.id = opt_str(a, args, "id");
    } else if (avp_str_eqz(cmd, "in")) {
        fa.function = opt_str(a, args, "function");
        fa.level = opt_str(a, args, "level");
        fa.min_functions = avp_args_int(args, "min_functions");
        fa.overlaps = avp_args_bool(args, "overlaps");
    } else if (avp_str_eqz(cmd, "source")) {
        fa.paths = avp_args_strs(args, "paths");
        fa.lib = opt_str(a, args, "lib");
        fa.min_tokens = avp_args_int(args, "min_tokens");
        fa.min_sites = avp_args_int(args, "min_sites");
    }
    if (avp_str_eqz(cmd, "source")) {
        const ShsmRepo *r = shsm_repo_find(root);
        ShsmOutput *out = shsm_output_open(args, -1, -1, -1);
        int rc = frag_cmd_source(&fa, r, out);
        shsm_output_close(out);
        return rc;
    }
    int jobs = avp_args_is_none(args, "jobs") ? 0 : (int)avp_args_int(args, "jobs");
    Frags *fr = frags_open(root, avp_args_bool(args, "rebuild"), jobs, avp_args_int(args, "window"),
                           avp_args_int(args, "sites"), as_json ? NULL : log_stderr);
    /* `list` caps its own records and counts what it leaves out */
    ShsmOutput *out = shsm_output_open(args, avp_str_eqz(cmd, "list") ? 0 : -1, -1, -1);
    int rc;
    if (avp_str_eqz(cmd, "list")) rc = frag_cmd_list(&fa, fr, out);
    else if (avp_str_eqz(cmd, "show")) rc = frag_cmd_show(&fa, fr, out);
    else if (avp_str_eqz(cmd, "in")) rc = frag_cmd_in(&fa, fr, out);
    else if (avp_str_eqz(cmd, "known")) rc = frag_cmd_known(&fa, fr, out);
    else rc = frag_cmd_stats(&fa, fr, out);
    shsm_output_close(out);
    frags_close(fr);
    return rc;
}

int fragments_main(int argc, char **argv) {
    AvpArgParser *ap = shsm_cli_parser("fragments.exe", DESCRIPTION, true, true, true);
    avp_arg(ap, &(AvpArgDesc){.names = "--rebuild", .action = AVP_ARG_STORE_TRUE,
                              .help = "recompute the fragment tables"});
    avp_arg(ap, &(AvpArgDesc){.names = "-j --jobs", .type = AVP_T_INT, .dflt = AVP_DEF_NONE});
    avp_arg(ap, &(AvpArgDesc){.names = "--window", .type = AVP_T_INT, .dflt = AVP_DEF_INT(FRAG_DEFAULT_WINDOW),
                              .help = "seed window in rows (default %(default)s)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--sites", .type = AVP_T_INT, .dflt = AVP_DEF_INT(FRAG_DEFAULT_SITES),
                              .help = "distinct functions a seed needs (default %(default)s)"});
    AvpArgSubparsers *sub = avp_args_subparsers(ap, &(AvpArgSubparsersDesc){.dest = "cmd", .required = true});
    static const char *const levels[] = {"strict", "loose", NULL};
    AvpArgParser *sps[6];
    AvpArgParser *p = sps[0] =
        avp_args_subparser(sub, "list", &(AvpArgParserDesc){.help = "fragments ranked by functions x length"});
    avp_arg(p, &(AvpArgDesc){.names = "--lib", .dflt = AVP_DEF_NONE});
    avp_arg(p, &(AvpArgDesc){.names = "--min-len", .type = AVP_T_INT, .dflt = AVP_DEF_INT(0)});
    avp_arg(p, &(AvpArgDesc){.names = "--min-functions", .type = AVP_T_INT, .dflt = AVP_DEF_INT(FRAG_DEFAULT_SITES)});
    avp_arg(p, &(AvpArgDesc){.names = "--level", .choices = levels, .dflt = AVP_DEF_STR("loose")});
    avp_arg(p, &(AvpArgDesc){.names = "--unmatched", .action = AVP_ARG_STORE_TRUE,
                             .help = "only fragments no matched function contains"});
    p = sps[1] = avp_args_subparser(
        sub, "show", &(AvpArgParserDesc){.help = "one fragment: sequence, occurrences, varying operands, context"});
    avp_arg(p, &(AvpArgDesc){.names = "id"});
    p = sps[2] = avp_args_subparser(sub, "in",
                                    &(AvpArgParserDesc){.help = "the fragments inside one function, in address order"});
    avp_arg(p, &(AvpArgDesc){.names = "function"});
    avp_arg(p, &(AvpArgDesc){.names = "--level", .choices = levels, .dflt = AVP_DEF_NONE});
    avp_arg(p, &(AvpArgDesc){.names = "--min-functions", .type = AVP_T_INT, .dflt = AVP_DEF_INT(0)});
    avp_arg(p, &(AvpArgDesc){.names = "--overlaps", .action = AVP_ARG_STORE_TRUE,
                             .help = "every fragment occurrence, not the best per stretch"});
    sps[3] = avp_args_subparser(sub, "known",
                                &(AvpArgParserDesc){.help = "accuracy check against the helpers AGENTS.md records"});
    sps[4] = avp_args_subparser(sub, "stats", &(AvpArgParserDesc){.help = "fragment counts and build timings"});
    p = sps[5] = avp_args_subparser(sub, "source",
                                    &(AvpArgParserDesc){.help = "repeated statement groups in reconstructed source"});
    avp_arg(p, &(AvpArgDesc){.names = "paths", .nargs = AVP_NARGS_STAR});
    avp_arg(p, &(AvpArgDesc){.names = "--lib", .dflt = AVP_DEF_NONE});
    avp_arg(p, &(AvpArgDesc){.names = "--min-tokens", .type = AVP_T_INT, .dflt = AVP_DEF_INT(16)});
    avp_arg(p, &(AvpArgDesc){.names = "--min-sites", .type = AVP_T_INT, .dflt = AVP_DEF_INT(3)});
    for (int i = 0; i < 6; i++) add_shared_options(sps[i]);
    return shsm_cli_main(ap, argc, argv, run, NULL);
}
