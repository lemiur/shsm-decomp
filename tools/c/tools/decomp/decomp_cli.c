/* decomp_cli: decomp.py main(): the parser (cli.parser plus five subcommands; show and status take the shared
 * output options after the subcommand too, cli.add_shared_options) and the dispatch. */
#include "decomp.h"

#include <string.h>

static const char DESCRIPTION[] = "Ghidra decompiler listings of the retail functions (reading aids, never source).";

static bool limit_conv(void *ctx, AvpArena *a, AvpStr in, AvpArgValue *out, AvpStr *msg) {
    (void)ctx;
    i64 n = 0;
    if (!shsm_limit_type(a, in, &n, msg)) return false;
    out->kind = AVP_V_INT;
    out->i = n;
    return true;
}

/* cli.add_shared_options(p): --json, --all, --limit with suppressed defaults */
static void add_shared_options(AvpArgParser *p) {
    avp_arg(p, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE, .dflt = AVP_DEF_SUPPRESS,
                             .help = "print one JSON document instead of text"});
    avp_arg(p, &(AvpArgDesc){.names = "--all", .action = AVP_ARG_STORE_TRUE, .dflt = AVP_DEF_SUPPRESS,
                             .help = "do not cap the output"});
    avp_arg(p, &(AvpArgDesc){.names = "--limit", .type = AVP_T_CUSTOM, .conv = limit_conv, .type_name = "limit_type",
                             .dflt = AVP_DEF_SUPPRESS, .metavar = "N",
                             .help = "cap text output at N lines (default 40; 0 or 'all': no cap)"});
}

static const char *opt_cz(AvpArena *a, const AvpArgs *args, const char *dest) {
    if (!avp_args_has(args, dest) || avp_args_is_none(args, dest)) return NULL;
    return avp_cz(a, avp_args_str(args, dest));
}

static int run(void *ctx, AvpArgs *args) {
    (void)ctx;
    AvpArena *a = avp_arena();
    DcArgs g = {0};
    g.args = args;
    g.command = avp_cz(a, avp_args_str(args, "command"));
    g.root = opt_cz(a, args, "root");
    const char *c = g.command;
    if (!strcmp(c, "setup")) {
        g.ghidra = opt_cz(a, args, "ghidra");
        g.force = avp_args_bool(args, "force");
        return dc_cmd_setup(&g);
    }
    if (!strcmp(c, "export") || !strcmp(c, "refresh")) {
        g.jobs = avp_args_int(args, "jobs");
        g.timeout = avp_args_int(args, "timeout");
        if (!strcmp(c, "refresh")) return dc_cmd_refresh(&g);
        AvpStrVec syms = avp_args_strs(args, "symbols");
        g.symbols = AVP_NEWN(a, const char *, syms.n + 1);
        for (size_t i = 0; i < syms.n; i++) g.symbols[i] = avp_cz(a, syms.v[i]);
        g.nsymbols = syms.n;
        avp_vec_free(&syms);
        g.every = avp_args_bool(args, "every");
        g.missing = avp_args_bool(args, "missing");
        g.unit = opt_cz(a, args, "unit");
        g.lib = opt_cz(a, args, "lib");
        return dc_cmd_export(&g);
    }
    if (!strcmp(c, "show")) {
        g.symbol = opt_cz(a, args, "symbol");
        g.timeout = avp_args_int(args, "timeout");
        return dc_cmd_show(&g);
    }
    g.failures = avp_args_bool(args, "failures");
    return dc_cmd_status(&g);
}

int decomp_main(int argc, char **argv) {
    AvpArena *a = avp_arena();
    AvpArgParser *ap = shsm_cli_parser("decomp.exe", DESCRIPTION, true, true, true);
    AvpArgSubparsers *sub = avp_args_subparsers(ap, &(AvpArgSubparsersDesc){.dest = "command", .required = true});
    AvpArgParser *p = avp_args_subparser(
        sub, "setup", &(AvpArgParserDesc){.help = "locate Ghidra, import and analyse the image, apply symbols.txt"});
    avp_arg(p, &(AvpArgDesc){.names = "--ghidra", .metavar = "DIR", .help = "Ghidra install directory"});
    avp_arg(p, &(AvpArgDesc){.names = "--force", .action = AVP_ARG_STORE_TRUE, .help = "rebuild an existing project"});
    int cpus = shsm_default_jobs() - 2;
    i64 jobs = cpus < 1 ? 1 : cpus > 16 ? 16 : cpus;
    p = avp_args_subparser(sub, "export", &(AvpArgParserDesc){.help = "decompile functions into build/ghidra/decomp"});
    avp_arg(p, &(AvpArgDesc){.names = "symbols", .nargs = AVP_NARGS_STAR, .help = "function names or addresses"});
    avp_arg(p, &(AvpArgDesc){.names = "--all --all-functions", .dest = "every", .action = AVP_ARG_STORE_TRUE,
                             .help = "every symbols.txt function"});
    avp_arg(p, &(AvpArgDesc){.names = "--unit", .help = "the functions of one unit (main/<path> or <path>)"});
    avp_arg(p, &(AvpArgDesc){.names = "--lib", .help = "the functions of every unit of one configure.py library"});
    avp_arg(p, &(AvpArgDesc){.names = "--missing", .action = AVP_ARG_STORE_TRUE,
                             .help = "skip functions that already have a listing"});
    avp_arg(p, &(AvpArgDesc){.names = "-j --jobs", .type = AVP_T_INT, .dflt = AVP_DEF_INT(jobs),
                             .help = avp_sprintf(a, "decompiler workers (default %lld)", (long long)jobs)});
    avp_arg(p, &(AvpArgDesc){.names = "--timeout", .type = AVP_T_INT, .dflt = AVP_DEF_INT(60),
                             .help = "per-function timeout in seconds (default 60)"});
    p = avp_args_subparser(sub, "show", &(AvpArgParserDesc){.help = "print the listing of one function"});
    avp_arg(p, &(AvpArgDesc){.names = "symbol"});
    avp_arg(p, &(AvpArgDesc){.names = "--timeout", .type = AVP_T_INT, .dflt = AVP_DEF_INT(60)});
    add_shared_options(p);
    p = avp_args_subparser(sub, "refresh",
                           &(AvpArgParserDesc){.help = "re-apply changed symbols and re-export what they touch"});
    avp_arg(p, &(AvpArgDesc){.names = "-j --jobs", .type = AVP_T_INT, .dflt = AVP_DEF_INT(jobs)});
    avp_arg(p, &(AvpArgDesc){.names = "--timeout", .type = AVP_T_INT, .dflt = AVP_DEF_INT(60)});
    p = avp_args_subparser(sub, "status", &(AvpArgParserDesc){.help = "install, project and listing counts"});
    avp_arg(p, &(AvpArgDesc){.names = "--failures", .action = AVP_ARG_STORE_TRUE,
                             .help = "list the functions that failed"});
    add_shared_options(p);
    return shsm_cli_main(ap, argc, argv, run, NULL);
}
