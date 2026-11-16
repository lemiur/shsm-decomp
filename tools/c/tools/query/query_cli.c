/* query.py build_parser, COMMANDS, run and main. */
#include "query.h"

static bool limit_conv(void *c, AvpArena *a, AvpStr in, AvpArgValue *out, AvpStr *msg) {
    (void)c;
    i64 n = 0;
    if (!shsm_limit_type(a, in, &n, msg)) return false;
    out->kind = AVP_V_INT;
    out->i = n;
    return true;
}

/* add(name, help): a subparser with cli.parser("")'s options (--root, --json, --all, --limit, --debug) as parents. */
static AvpArgParser *add(AvpArgSubparsers *sub, const char *name, const char *help) {
    AvpArgParser *p = avp_args_subparser(sub, name, &(AvpArgParserDesc){.help = help, .description = help});
    avp_arg(p, &(AvpArgDesc){.names = "--root", .metavar = "DIR",
                             .help = "checkout to work on (default: the one around the cwd)"});
    avp_arg(p, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE,
                             .help = "print one JSON document instead of text"});
    avp_arg(p, &(AvpArgDesc){.names = "--all", .action = AVP_ARG_STORE_TRUE, .help = "do not cap the output"});
    avp_arg(p, &(AvpArgDesc){.names = "--limit", .type = AVP_T_CUSTOM, .conv = limit_conv, .type_name = "limit_type",
                             .dflt = AVP_DEF_INT(SHSM_DEFAULT_LIMIT), .metavar = "N",
                             .help = "cap text output at N lines (default 40; 0 or 'all': no cap)"});
    avp_arg(p, &(AvpArgDesc){.names = "--debug", .action = AVP_ARG_STORE_TRUE, .help = "print tracebacks"});
    return p;
}

static AvpArgParser *build_parser(void) {
    AvpArgParser *ap = shsm_cli_parser("query.exe", "Ask the retail binary and the project a question, without "
                                                   "grepping 60 MB of asm.", false, false, false);
    AvpArgSubparsers *sub = avp_args_subparsers(ap, &(AvpArgSubparsersDesc){.dest = "cmd", .required = true});
    AvpArgParser *p = add(sub, "fn", "a function's header and retail assembly");
    avp_arg(p, &(AvpArgDesc){.names = "sym"});
    avp_arg(p, &(AvpArgDesc){.names = "--summary", .action = AVP_ARG_STORE_TRUE, .help = "header and counts only"});
    avp_arg(p, &(AvpArgDesc){.names = "--raw", .action = AVP_ARG_STORE_TRUE, .help = "show instruction bytes"});
    for (int i = 0; i < 2; i++) {
        p = add(sub, i ? "callees" : "callers", i ? "what the function calls" : "who calls the symbol");
        avp_arg(p, &(AvpArgDesc){.names = "sym"});
        avp_arg(p, &(AvpArgDesc){.names = "--depth", .type = AVP_T_INT, .dflt = AVP_DEF_INT(1),
                                 .help = "levels of the call tree (default 1)"});
    }
    p = add(sub, "xref", "every reference to a symbol, grouped by kind");
    avp_arg(p, &(AvpArgDesc){.names = "sym"});
    p = add(sub, "strings", "string literals a function (or every function of a unit) addresses");
    avp_arg(p, &(AvpArgDesc){.names = "target", .help = "function or unit"});
    p = add(sub, "find-string", "string literals matching text, and who references them");
    avp_arg(p, &(AvpArgDesc){.names = "text"});
    avp_arg(p, &(AvpArgDesc){.names = "-e --regex", .action = AVP_ARG_STORE_TRUE,
                             .help = "treat text as a regular expression"});
    avp_arg(p, &(AvpArgDesc){.names = "-i --ignore-case", .action = AVP_ARG_STORE_TRUE});
    p = add(sub, "find-float", "float/double literals equal to a value, and their users");
    avp_arg(p, &(AvpArgDesc){.names = "value"});
    avp_arg(p, &(AvpArgDesc){.names = "--imm", .action = AVP_ARG_STORE_TRUE,
                             .help = "also search lis/ori immediates of the float's bits"});
    p = add(sub, "find-const", "a 32-bit constant in data words and lis/addi|ori pairs");
    avp_arg(p, &(AvpArgDesc){.names = "value", .help = "hex (0x3F800000 or 3F800000) or decimal with 0d prefix"});
    p = add(sub, "data", "a data object as a C initialiser draft");
    avp_arg(p, &(AvpArgDesc){.names = "sym"});
    static const char *const views[] = {"u8", "u16", "u32", "float", "ptr", NULL};
    avp_arg(p, &(AvpArgDesc){.names = "--as", .dest = "as_view", .choices = views,
                             .help = "force an element view over the retail bytes"});
    p = add(sub, "addr", "what lives at an address");
    avp_arg(p, &(AvpArgDesc){.names = "address"});
    p = add(sub, "unit", "a unit's header and function list");
    avp_arg(p, &(AvpArgDesc){.names = "unit"});
    avp_arg(p, &(AvpArgDesc){.names = "--data", .action = AVP_ARG_STORE_TRUE, .help = "also list its data objects"});
    p = add(sub, "vtable", "a vtable's slots with target names");
    avp_arg(p, &(AvpArgDesc){.names = "target", .help = "vtable symbol or class name"});
    p = add(sub, "notes", "passages in docs/*.md and source comments that mention a term");
    avp_arg(p, &(AvpArgDesc){.names = "term"});
    AvpArgMutex *g = avp_args_mutex(p, false);
    avp_arg_mutex(g, &(AvpArgDesc){.names = "--docs-only", .action = AVP_ARG_STORE_TRUE});
    avp_arg_mutex(g, &(AvpArgDesc){.names = "--source-only", .action = AVP_ARG_STORE_TRUE});
    return ap;
}

typedef int (*QueryCmd)(QueryCtx *, AvpArgs *, ShsmOutput *);
static const struct {
    const char *name;
    QueryCmd fn;
} COMMANDS[] = {
    {"fn", query_cmd_fn},         {"callers", query_cmd_callers},       {"callees", query_cmd_callees},
    {"xref", query_cmd_xref},     {"strings", query_cmd_strings},       {"find-string", query_cmd_find_string},
    {"find-float", query_cmd_find_float}, {"find-const", query_cmd_find_const}, {"data", query_cmd_data},
    {"addr", query_cmd_addr},     {"unit", query_cmd_unit},             {"vtable", query_cmd_vtable},
    {"notes", query_cmd_notes},
};

static int run(void *c, AvpArgs *args) {
    (void)c;
    AvpArena *a = avp_arena();
    AvpStr root = avp_args_str(args, "root");
    QueryCtx *ctx = query_ctx_for_root(a, root.p ? avp_cz(a, root) : NULL);
    ShsmOutput *out = shsm_output_open(args, -1, -1, -1);
    AvpStr cmd = avp_args_str(args, "cmd");
    int rc = SHSM_EXIT_ERROR;
    for (size_t i = 0; i < sizeof COMMANDS / sizeof *COMMANDS; i++)
        if (avp_str_eqz(cmd, COMMANDS[i].name)) rc = COMMANDS[i].fn(ctx, args, out);
    shsm_output_close(out);
    query_ctx_close(ctx);
    return rc;
}

int query_main(int argc, char **argv) { return shsm_cli_main(build_parser(), argc, argv, run, NULL); }
