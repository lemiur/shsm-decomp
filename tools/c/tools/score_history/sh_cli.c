/* The command line (build_parser, main) and score_history_run. */
#include "sh_priv.h"
#include "shsm_cli.h"
#include "shsm_repo.h"

#include <string.h>

#define BASELINE "config/R5WEA4/baseline_report.json"
#define DEFAULT_DB "build/score_history.sqlite"

static AvpArgParser *build_parser(AvpArena *a) {
    AvpArgParser *ap = avp_args_parser(a, &(AvpArgParserDesc){
                                              .prog = "score_history.exe",
                                              .description = "Per-function score history (see the module docstring).",
                                          });
    avp_arg(ap, &(AvpArgDesc){.names = "--db", .help = "database path (default: build/score_history.sqlite under the repo)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--repo", .dflt = AVP_DEF_NONE, .help = "repository root (default: this tool's repository)"});
    AvpArgSubparsers *sub = avp_args_subparsers(ap, &(AvpArgSubparsersDesc){.dest = "cmd", .required = true});

    AvpArgParser *r = avp_args_subparser(sub, "record", &(AvpArgParserDesc){.help = "record one snapshot"});
    avp_arg(r, &(AvpArgDesc){.names = "--report",
                             .help = "objdiff report JSON (default: build report; with --rev: baseline at REV)"});
    AvpArgMutex *g = avp_args_mutex(r, false);
    avp_arg_mutex(g, &(AvpArgDesc){.names = "--rev", .help = "record git revision REV (sources via git)"});
    avp_arg_mutex(g, &(AvpArgDesc){.names = "--worktree", .action = AVP_ARG_STORE_TRUE,
                                   .help = "record the working tree (the default)"});
    avp_arg(r, &(AvpArgDesc){.names = "--include-stale", .action = AVP_ARG_STORE_TRUE,
                             .help = "worktree: also units whose source is newer than the report"});
    avp_arg(r, &(AvpArgDesc){.names = "--baseline", .dflt = AVP_DEF_STR(BASELINE), .help = AVP_ARG_SUPPRESS});

    AvpArgParser *b = avp_args_subparser(sub, "backfill", &(AvpArgParserDesc){.help = "record every committed baseline_report.json"});
    avp_arg(b, &(AvpArgDesc){.names = "--first-parent", .action = AVP_ARG_STORE_TRUE, .help = "follow first-parent history only"});
    avp_arg(b, &(AvpArgDesc){.names = "--limit", .type = AVP_T_INT, .help = "only the newest N revisions"});
    avp_arg(b, &(AvpArgDesc){.names = "--baseline", .dflt = AVP_DEF_STR(BASELINE), .help = AVP_ARG_SUPPRESS});

    AvpArgParser *s = avp_args_subparser(sub, "status", &(AvpArgParserDesc){.help = "current / best / historical per function"});
    avp_arg(s, &(AvpArgDesc){.names = "--unit", .help = "one unit (main/<path>, or a prefix / suffix of it)"});
    avp_arg(s, &(AvpArgDesc){.names = "--lost", .action = AVP_ARG_STORE_TRUE, .help = "historical > best, with the recovery command"});
    avp_arg(s, &(AvpArgDesc){.names = "--regressed", .action = AVP_ARG_STORE_TRUE, .help = "current < previous snapshot"});
    avp_arg(s, &(AvpArgDesc){.names = "--vs", .help = "compare against this snapshot (rev prefix) instead of the previous one"});
    avp_arg(s, &(AvpArgDesc){.names = "--all", .action = AVP_ARG_STORE_TRUE, .help = "list every function"});
    avp_arg(s, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE});

    AvpArgParser *w = avp_args_subparser(sub, "show", &(AvpArgParserDesc){.help = "timeline of one function"});
    avp_arg(w, &(AvpArgDesc){.names = "symbol", .help = "symbol (any name it had), 0xADDRESS, or a unique prefix"});
    avp_arg(w, &(AvpArgDesc){.names = "--source", .nargs = AVP_NARGS_OPT, .cnst = AVP_DEF_STR(""),
                             .help = "print the stored source of the latest (or HASH-prefixed) version"});
    avp_arg(w, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE});
    return ap;
}

static const char *opt_str(AvpArena *a, const AvpArgs *r, const char *dest) {
    if (!avp_args_has(r, dest) || avp_args_is_none(r, dest)) return NULL;
    return avp_cz(a, avp_args_str(r, dest));
}

int score_history_run(AvpArena *a, int argc, const char *const *argv, AvpBuf *out, AvpBuf *err) {
    AvpArgParser *ap = build_parser(a);
    AvpStr *args = avp_alloc(a, (size_t)(argc > 0 ? argc : 1) * sizeof(AvpStr));
    for (int i = 0; i < argc; i++) args[i] = avp_str(argv[i]);
    AvpArgsExit ex = {0};
    AvpArgs *r = avp_args_try_parse(ap, args, (size_t)argc, false, &ex);
    if (!r) {
        avp_buf_put(out, ex.out);
        avp_buf_put(err, ex.err);
        return ex.code;
    }
    const char *repo_arg = opt_str(a, r, "repo");
    const char *root = repo_arg;
    if (!root) {
        AvpTry rt;
        if (AVP_TRY(&rt)) {
            root = shsm_find_root(NULL);
            avp_try_end(&rt);
        } else {
            root = avp_root();
        }
    }
    const char *repo = avp_path_abspath(a, root);
    const char *dbp = opt_str(a, r, "db");
    if (!dbp) dbp = avp_path_join(a, repo, DEFAULT_DB);
    ShCtx c = {a, NULL, repo, out, NULL};
    const char *cmd = opt_str(a, r, "cmd");
    int rc = 0;
    AvpTry t;
    if (AVP_TRY(&t)) {
        sh_db_open(&c, dbp);
        if (!strcmp(cmd, "record")) {
            ShRecordOpts o = {0};
            o.rev = opt_str(a, r, "rev");
            o.rev_given = o.rev != NULL;
            o.report = opt_str(a, r, "report");
            o.report_given = o.report != NULL;
            o.include_stale = avp_args_bool(r, "include_stale");
            o.baseline = opt_str(a, r, "baseline");
            rc = sh_cmd_record(&c, &o);
        } else if (!strcmp(cmd, "backfill")) {
            bool lg = !avp_args_is_none(r, "limit");
            rc = sh_cmd_backfill(&c, opt_str(a, r, "baseline"), avp_args_bool(r, "first_parent"), lg,
                                 lg ? avp_args_int(r, "limit") : 0);
        } else if (!strcmp(cmd, "status")) {
            ShStatusOpts o = {0};
            o.unit = opt_str(a, r, "unit");
            if (o.unit && !*o.unit) o.unit = NULL; /* `if args.unit:` */
            o.vs = opt_str(a, r, "vs");
            if (o.vs && !*o.vs) o.vs = NULL;       /* `if vs` */
            o.lost = avp_args_bool(r, "lost");
            o.regressed = avp_args_bool(r, "regressed");
            o.all = avp_args_bool(r, "all");
            o.json = avp_args_bool(r, "json");
            rc = sh_cmd_status(&c, &o);
        } else {
            const char *src = opt_str(a, r, "source");
            rc = sh_cmd_show(&c, opt_str(a, r, "symbol"), src != NULL, src ? src : "", avp_args_bool(r, "json"));
        }
        avp_try_end(&t);
    } else {
        if (t.code == SH_EXC_EXIT) {
            avp_buf_putz(err, t.msg);
            avp_buf_putc(err, '\n');
        } else {
            /* an uncaught Python exception: a traceback ending in the exception line, exit 1 */
            avp_buf_putz(err, "score_history.exe: error: ");
            if (t.code == SH_EXC_RUNTIME) avp_buf_putz(err, "RuntimeError: ");
            else if (t.code < SH_EXC_CRASH && t.code >= 64) avp_buf_pyfmt(err, "%s: ", shsm_exc_class(t.code));
            avp_buf_putz(err, t.msg);
            avp_buf_putc(err, '\n');
        }
        rc = 1;
    }
    sh_db_close(&c);
    return rc;
}
