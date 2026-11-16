/* hashes.py build_parser() and run(): every subcommand takes the shared options of cli.parser("") and --rebuild
 * (argparse parents=[common], conflict_handler="resolve"), with their real defaults, so a value given after the
 * subcommand, or its default, replaces one given before it (argparse copies the subparser's namespace over). */
#include "hashes.h"

#include <string.h>

static const char DESCRIPTION[] = "Recover names from string hashes and interned names in the retail binary.";

static bool limit_conv(void *ctx, AvpArena *a, AvpStr in, AvpArgValue *out, AvpStr *msg) {
    (void)ctx;
    i64 n = 0;
    if (!shsm_limit_type(a, in, &n, msg)) return false;
    out->kind = AVP_V_INT;
    out->i = n;
    return true;
}

/* parents=[common]: cli.parser("")'s options and --rebuild */
static void add_common(AvpArgParser *p) {
    avp_arg(p, &(AvpArgDesc){.names = "--root", .metavar = "DIR", .dflt = AVP_DEF_NONE,
                             .help = "checkout to work on (default: the one around the cwd)"});
    avp_arg(p, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE,
                             .help = "print one JSON document instead of text"});
    avp_arg(p, &(AvpArgDesc){.names = "--all", .action = AVP_ARG_STORE_TRUE, .help = "do not cap the output"});
    avp_arg(p, &(AvpArgDesc){.names = "--limit", .type = AVP_T_CUSTOM, .conv = limit_conv, .type_name = "limit_type",
                             .dflt = AVP_DEF_INT(SHSM_DEFAULT_LIMIT), .metavar = "N",
                             .help = "cap text output at N lines (default 40; 0 or 'all': no cap)"});
    avp_arg(p, &(AvpArgDesc){.names = "--debug", .action = AVP_ARG_STORE_TRUE, .help = "print tracebacks"});
    avp_arg(p, &(AvpArgDesc){.names = "--rebuild", .action = AVP_ARG_STORE_TRUE,
                             .help = "rebuild build/index/hashes.sqlite"});
}

static AvpArgParser *add(AvpArgSubparsers *sub, const char *name, const char *help) {
    AvpArgParser *p = avp_args_subparser(sub, name, &(AvpArgParserDesc){.help = help, .description = help});
    add_common(p);
    return p;
}

static const char *opt_cz(AvpArena *a, const AvpArgs *args, const char *dest) {
    if (!avp_args_has(args, dest) || avp_args_is_none(args, dest)) return NULL;
    return avp_cz(a, avp_args_str(args, dest));
}

static AvpStrVec opt_list(const AvpArgs *args, const char *dest) {
    if (!avp_args_has(args, dest) || avp_args_is_none(args, dest)) return (AvpStrVec){0};
    return avp_args_strs(args, dest);
}

static int run(void *ctx, AvpArgs *args) {
    (void)ctx;
    AvpArena *a = avp_arena();
    HashesArgs ha = {0};
    ha.cmd = avp_cz(a, avp_args_str(args, "cmd"));
    ha.root = opt_cz(a, args, "root");
    ha.unverified = avp_args_has(args, "unverified") && avp_args_bool(args, "unverified");
    if (strcmp(ha.cmd, "scan") == 0) {
        ha.functions = opt_list(args, "functions");
        ha.units = opt_list(args, "unit");
        ha.ranges = opt_list(args, "range");
        ha.lib = opt_cz(a, args, "lib");
        ha.no_arc = avp_args_bool(args, "no_arc");
    } else if (strcmp(ha.cmd, "of") == 0) {
        ha.strings = opt_list(args, "strings");
    } else if (strcmp(ha.cmd, "find") == 0) {
        ha.values = opt_list(args, "values");
        ha.words = opt_list(args, "words");
    } else if (strcmp(ha.cmd, "vars") == 0) {
        ha.status = opt_cz(a, args, "status");
        ha.pattern = opt_cz(a, args, "pattern");
        ha.grep = opt_cz(a, args, "grep");
        ha.cls = opt_cz(a, args, "cls");
        ha.call = opt_cz(a, args, "call");
        ha.by_class = avp_args_bool(args, "by_class");
    }
    const ShsmRepo *repo = shsm_repo_find(ha.root);
    ShsmAsmIndex *ix = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        ix = shsm_asmindex_open(&(ShsmAsmIndexOpts){.repo = repo, .readonly = true});
        avp_try_end(&t);
    } else {
        if (t.code == SHSM_EXC_FILE_NOT_FOUND) shsm_raise(SHSM_EXC_USAGE, "%s", t.msg ? t.msg : "");
        avp_throw(t.code, "%s", t.msg ? t.msg : "");
    }
    HashesDb *h = hashes_db_open(repo, ix, avp_args_bool(args, "rebuild"));
    AvpSet *verified = hashes_verified(h);
    HashesOut out = {.out = shsm_output_open(args, -1, -1, -1)};
    int rc;
    if (strcmp(ha.cmd, "scan") == 0) rc = hashes_cmd_scan(&ha, h, verified, &out);
    else if (strcmp(ha.cmd, "of") == 0) rc = hashes_cmd_of(&ha, h, verified, &out);
    else if (strcmp(ha.cmd, "find") == 0) rc = hashes_cmd_find(&ha, h, verified, &out);
    else if (strcmp(ha.cmd, "vars") == 0) rc = hashes_cmd_vars(&ha, h, verified, &out);
    else if (strcmp(ha.cmd, "stats") == 0) rc = hashes_cmd_stats(&ha, h, verified, &out);
    else rc = hashes_cmd_functions(&ha, h, verified, &out);
    shsm_output_close(out.out);
    avp_set_free(verified);
    hashes_db_close(h);
    shsm_asmindex_close(ix);
    return rc;
}

int hashes_main(int argc, char **argv) {
    AvpArgParser *ap = shsm_cli_parser("hashes.exe", DESCRIPTION, true, true, true);
    AvpArgSubparsers *sub = avp_args_subparsers(ap, &(AvpArgSubparsersDesc){.dest = "cmd", .required = true});
    AvpArgParser *p = add(sub, "scan", "constants and archive entries that are hashes of corpus strings");
    avp_arg(p, &(AvpArgDesc){.names = "functions", .nargs = AVP_NARGS_STAR,
                             .help = "functions (names or addresses) to scan"});
    avp_arg(p, &(AvpArgDesc){.names = "--lib", .help = "a configure.py library, or a unit path prefix (game, climax)"});
    avp_arg(p, &(AvpArgDesc){.names = "--unit", .action = AVP_ARG_APPEND, .help = "a unit (repeatable)"});
    avp_arg(p, &(AvpArgDesc){.names = "--range", .action = AVP_ARG_APPEND,
                             .help = "START-END address range, hex (repeatable)"});
    avp_arg(p, &(AvpArgDesc){.names = "--no-arc", .action = AVP_ARG_STORE_TRUE,
                             .help = "leave out the archive entry tables"});
    avp_arg(p, &(AvpArgDesc){.names = "--unverified", .action = AVP_ARG_STORE_TRUE,
                             .help = "also use hash functions without ground truth"});
    p = add(sub, "of", "a string's value under each hash function, and where each value occurs");
    avp_arg(p, &(AvpArgDesc){.names = "strings", .nargs = AVP_NARGS_PLUS});
    p = add(sub, "find", "corpus strings whose hash is a value");
    avp_arg(p, &(AvpArgDesc){.names = "values", .nargs = AVP_NARGS_PLUS, .help = "hex values"});
    avp_arg(p, &(AvpArgDesc){.names = "--words", .action = AVP_ARG_APPEND,
                             .help = "extra names: a text or binary file (repeatable)"});
    avp_arg(p, &(AvpArgDesc){.names = "--unverified", .action = AVP_ARG_STORE_TRUE,
                             .help = "also use hash functions without ground truth"});
    p = add(sub, "vars", "variables filled from a string literal: interned names (the headers' \"class-name hash\" "
                         "variables), class-registry objects, and djb2 hashes");
    AvpArgMutex *g = avp_args_mutex(p, false);
    avp_arg_mutex(g, &(AvpArgDesc){.names = "--new", .dest = "status", .action = AVP_ARG_STORE_CONST,
                                   .cnst = AVP_DEF_STR("new")});
    avp_arg_mutex(g, &(AvpArgDesc){.names = "--named", .dest = "status", .action = AVP_ARG_STORE_CONST,
                                   .cnst = AVP_DEF_STR("named")});
    avp_arg_mutex(g, &(AvpArgDesc){.names = "--mismatch", .dest = "status", .action = AVP_ARG_STORE_CONST,
                                   .cnst = AVP_DEF_STR("mismatch")});
    avp_arg(p, &(AvpArgDesc){.names = "pattern", .nargs = AVP_NARGS_OPT,
                             .help = "keep the pairs whose variable, string, function or unit contains this "
                                     "(case-insensitive)"});
    avp_arg(p, &(AvpArgDesc){.names = "--grep", .help = "same as PATTERN; both must match when both are given"});
    avp_arg(p, &(AvpArgDesc){.names = "--class", .dest = "cls", .metavar = "C",
                             .help = "only the pairs of class C: its interned/registry string, or functions in its "
                                     "scope/sinit"});
    avp_arg(p, &(AvpArgDesc){.names = "--by-class", .dest = "by_class", .action = AVP_ARG_STORE_TRUE,
                             .help = "group the listing by class"});
    static const char *const CALLS[] = {"intern", "djb2", "djb2fold", "registry", NULL};
    avp_arg(p, &(AvpArgDesc){.names = "--call", .choices = CALLS,
                             .help = "intern: interned names; registry: class-registry objects; djb2, djb2fold: "
                                     "hashes"});
    add(sub, "stats", "corpus, constants, ground-truth verification per hash function");
    add(sub, "functions", "the hash functions and the evidence for each");
    return shsm_cli_main(ap, argc, argv, run, NULL);
}
