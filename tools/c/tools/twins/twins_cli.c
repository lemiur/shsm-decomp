/* twins.py main(): the parser (the shared options also after the subcommand, cli.add_shared_options) and run(). */
#include "twins.h"

#include <string.h>

static const char DESCRIPTION[] = "Find code the binary contains more than once.";

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

static int run(void *ctx, AvpArgs *args) {
    (void)ctx;
    AvpArena *a = avp_arena();
    const char *root = NULL;
    if (avp_args_has(args, "root") && !avp_args_is_none(args, "root")) root = avp_cz(a, avp_args_str(args, "root"));
    int jobs = avp_args_is_none(args, "jobs") ? 0 : (int)avp_args_int(args, "jobs");
    Twins *tw = twins_load(root, avp_args_bool(args, "rebuild"), jobs);
    AvpStr cmd = avp_args_str(args, "cmd");
    TwinsArgs ta = {0};
    ta.all = avp_args_has(args, "all") && avp_args_bool(args, "all");
    ta.limit = avp_args_has(args, "limit") ? avp_args_int(args, "limit") : SHSM_DEFAULT_LIMIT;
    if (avp_str_eqz(cmd, "of")) {
        ta.function = avp_cz(a, avp_args_str(args, "function"));
    } else {
        ta.min_size = avp_args_int(args, "min_size");
        if (avp_args_has(args, "lib") && !avp_args_is_none(args, "lib")) ta.lib = avp_cz(a, avp_args_str(args, "lib"));
        if (avp_args_has(args, "level")) ta.level = avp_cz(a, avp_args_str(args, "level"));
        if (avp_args_has(args, "min_count")) ta.min_count = avp_args_int(args, "min_count");
    }
    /* the subcommands cap their own lists (a record can take two lines) and say how many were left out */
    TwinsOut out = {.out = shsm_output_open(args, 0, -1, -1)};
    int rc;
    if (avp_str_eqz(cmd, "of")) rc = twins_cmd_of(&ta, tw, &out);
    else if (avp_str_eqz(cmd, "unmatched")) rc = twins_cmd_unmatched(&ta, tw, &out);
    else if (avp_str_eqz(cmd, "families")) rc = twins_cmd_families(&ta, tw, &out);
    else rc = twins_cmd_stats(&ta, tw, &out);
    shsm_output_close(out.out);
    twins_close(tw);
    return rc;
}

int twins_main(int argc, char **argv) {
    AvpArgParser *ap = shsm_cli_parser("twins.exe", DESCRIPTION, true, true, true);
    avp_arg(ap, &(AvpArgDesc){.names = "--rebuild", .action = AVP_ARG_STORE_TRUE,
                              .help = "recompute the fingerprint table"});
    avp_arg(ap, &(AvpArgDesc){.names = "-j --jobs", .type = AVP_T_INT, .dflt = AVP_DEF_NONE});
    AvpArgSubparsers *sub = avp_args_subparsers(ap, &(AvpArgSubparsersDesc){.dest = "cmd", .required = true});
    AvpArgParser *sps[4];
    sps[0] = avp_args_subparser(sub, "of", &(AvpArgParserDesc){.help = "twins of one function"});
    avp_arg(sps[0], &(AvpArgDesc){.names = "function"});
    static const char *const unmatched_levels[] = {"exact", "shape", NULL};
    static const char *const all_levels[] = {"exact", "ref", "shape", NULL};
    const char *names[] = {"unmatched", "families", "stats"};
    for (int i = 0; i < 3; i++) {
        AvpArgParser *p = sps[i + 1] = avp_args_subparser(sub, names[i], NULL);
        avp_arg(p, &(AvpArgDesc){.names = "--min-size", .type = AVP_T_INT, .dflt = AVP_DEF_INT(TWINS_DEFAULT_MIN_SIZE)});
        if (i != 2) avp_arg(p, &(AvpArgDesc){.names = "--lib", .dflt = AVP_DEF_NONE});
        if (i == 0)
            avp_arg(p, &(AvpArgDesc){.names = "--level", .choices = unmatched_levels, .dflt = AVP_DEF_STR("exact")});
        if (i == 1) {
            avp_arg(p, &(AvpArgDesc){.names = "--level", .choices = all_levels, .dflt = AVP_DEF_STR("exact")});
            avp_arg(p, &(AvpArgDesc){.names = "--min-count", .type = AVP_T_INT, .dflt = AVP_DEF_INT(2)});
        }
    }
    /* subcommand options also accept the shared switches after the subcommand */
    for (int i = 0; i < 4; i++) add_shared_options(sps[i]);
    return shsm_cli_main(ap, argc, argv, run, NULL);
}
