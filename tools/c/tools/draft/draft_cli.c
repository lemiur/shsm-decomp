/* draft.py build_parser() and main(): the parser (every subcommand, the shared options also after the subcommand,
 * cli.add_shared_options(sp, root=True, debug=True)) and the dispatch (run). */
#include "draft.h"

#include <string.h>

static const char DESCRIPTION[] = "Draft pipeline: select, draft from matched twins, grade, promote.";

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

static AvpArgParser *build_parser(void) {
    AvpArgParser *ap = shsm_cli_parser("draft.exe", DESCRIPTION, true, true, true);
    AvpArgSubparsers *sub = avp_args_subparsers(ap, &(AvpArgSubparsersDesc){.dest = "cmd", .required = true});
    AvpArgParser *sps[8];
    AvpArgParser *p;

    static const char *const KINDS[] = {"twin", "small", "leaf", "near", NULL};
    p = sps[0] = avp_args_subparser(sub, "select", &(AvpArgParserDesc){.help = "the queue of draft targets"});
    avp_arg(p, &(AvpArgDesc){.names = "--kind", .choices = KINDS});
    avp_arg(p, &(AvpArgDesc){.names = "--lib"});
    avp_arg(p, &(AvpArgDesc){.names = "--functions", .action = AVP_ARG_STORE_TRUE,
                             .help = "one line per function instead of per class"});

    p = sps[1] = avp_args_subparser(sub, "packet", &(AvpArgParserDesc){.help = "the briefing and grading contract"});
    avp_arg(p, &(AvpArgDesc){.names = "target"});
    avp_arg(p, &(AvpArgDesc){.names = "--unit", .action = AVP_ARG_STORE_TRUE,
                             .help = "the target is a unit or START-END"});

    p = sps[2] = avp_args_subparser(sub, "twin", &(AvpArgParserDesc){.help = "the mechanical draft from a matched twin"});
    avp_arg(p, &(AvpArgDesc){.names = "function", .nargs = AVP_NARGS_OPT});
    avp_arg(p, &(AvpArgDesc){.names = "--unit",
                             .help = "draft a whole cloned unit: START-END (explicit), a configured unit, or a function "
                                     "inside it (the range unit_plan.exe proposes)"});
    avp_arg(p, &(AvpArgDesc){.names = "--out",
                             .help = "base directory: each draft goes to its own subdirectory (<target>/ or "
                                     "_unit_START-END/); may be on another drive"});

    p = sps[3] = avp_args_subparser(sub, "grade", &(AvpArgParserDesc){.help = "compile and score a draft"});
    avp_arg(p, &(AvpArgDesc){.names = "target", .help = "symbol, or START-END for a unit draft"});
    avp_arg(p, &(AvpArgDesc){.names = "--file", .required = true});
    avp_arg(p, &(AvpArgDesc){.names = "--as-unit"});
    avp_arg(p, &(AvpArgDesc){.names = "--profile", .help = "compile with this unit's flags"});

    p = sps[4] = avp_args_subparser(sub, "promote",
                                    &(AvpArgParserDesc){.help = "put a graded draft into the tree as a parked unit"});
    avp_arg(p, &(AvpArgDesc){.names = "path", .help = "unit path, e.g. game/CThing.cpp"});
    avp_arg(p, &(AvpArgDesc){.names = "--file", .required = true});
    avp_arg(p, &(AvpArgDesc){.names = "--header"});
    avp_arg(p, &(AvpArgDesc){.names = "--text", .help = "START-END of the unit's .text"});
    avp_arg(p, &(AvpArgDesc){.names = "--dry-run", .action = AVP_ARG_STORE_TRUE});
    avp_arg(p, &(AvpArgDesc){.names = "--allow-small-unit",
                             .action = AVP_ARG_STORE_TRUE,
                             .help = "accept a one-function or lone-initialiser range whose unit_plan cuts are not both high"});

    p = sps[5] = avp_args_subparser(sub, "reject", &(AvpArgParserDesc){.help = "record a failed approach"});
    avp_arg(p, &(AvpArgDesc){.names = "symbol"});
    avp_arg(p, &(AvpArgDesc){.names = "--reason", .required = true});
    avp_arg(p, &(AvpArgDesc){.names = "--approach", .dflt = AVP_DEF_STR("twin")});

    sps[6] = avp_args_subparser(sub, "rejects", &(AvpArgParserDesc){.help = "the rejects table"});

    p = sps[7] = avp_args_subparser(sub, "sweep", &(AvpArgParserDesc){.help = "twin and grade over the twin queue"});
    avp_arg(p, &(AvpArgDesc){.names = "--lib"});
    avp_arg(p, &(AvpArgDesc){.names = "-j --jobs", .type = AVP_T_INT, .dflt = AVP_DEF_INT(1)});

    for (int i = 0; i < 8; i++) add_shared_options(sps[i]);
    return ap;
}

/* an optional string argument: NULL when absent or None */
static const char *arg_opt(AvpArgs *args, const char *dest) {
    if (!avp_args_has(args, dest) || avp_args_is_none(args, dest)) return NULL;
    return avp_cz(avp_arena(), avp_args_str(args, dest));
}

/* main's `any(a == "--limit" or a.startswith("--limit=") for a in argv)`: sweep caps its queue only then */
static bool g_limit_given;

static int run(void *ctx, AvpArgs *args) {
    (void)ctx;
    AvpStr cmd = avp_args_str(args, "cmd");
    DraftWorld *W = draft_world_new(arg_opt(args, "root"));
    ShsmOutput *out = shsm_output_open(args, -1, -1, -1);
    i64 limit = avp_args_has(args, "limit") ? avp_args_int(args, "limit") : SHSM_DEFAULT_LIMIT;
    bool all = avp_args_has(args, "all") && avp_args_bool(args, "all");
    int rc;
    if (avp_str_eqz(cmd, "select"))
        rc = draft_cmd_select(W, out, arg_opt(args, "kind"), arg_opt(args, "lib"), avp_args_bool(args, "functions"),
                              limit, all);
    else if (avp_str_eqz(cmd, "twin"))
        rc = draft_cmd_twin(W, out, arg_opt(args, "function"), arg_opt(args, "unit"), arg_opt(args, "out"));
    else if (avp_str_eqz(cmd, "grade"))
        rc = draft_cmd_grade(W, out, arg_opt(args, "target"), arg_opt(args, "file"), arg_opt(args, "as_unit"),
                             arg_opt(args, "profile"));
    else if (avp_str_eqz(cmd, "promote"))
        rc = draft_cmd_promote(W, out, arg_opt(args, "path"), arg_opt(args, "file"), arg_opt(args, "header"),
                               arg_opt(args, "text"), avp_args_bool(args, "dry_run"),
                               avp_args_bool(args, "allow_small_unit"));
    else if (avp_str_eqz(cmd, "reject"))
        rc = draft_cmd_reject(W, out, arg_opt(args, "symbol"), arg_opt(args, "reason"), arg_opt(args, "approach"));
    else if (avp_str_eqz(cmd, "rejects"))
        rc = draft_cmd_rejects(W, out);
    else if (avp_str_eqz(cmd, "sweep"))
        rc = draft_cmd_sweep(W, out, arg_opt(args, "lib"), limit, g_limit_given, (int)avp_args_int(args, "jobs"));
    else
        rc = draft_cmd_packet(W, out, arg_opt(args, "target"), avp_args_bool(args, "unit"));
    shsm_output_close(out);
    draft_world_close(W);
    return rc;
}

int draft_main(int argc, char **argv) {
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--limit") || !strncmp(argv[i], "--limit=", 8)) g_limit_given = true;
    return shsm_cli_main(build_parser(), argc, argv, run, NULL);
}
