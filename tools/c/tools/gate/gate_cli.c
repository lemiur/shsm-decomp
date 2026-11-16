/* gate_cli: build_parser and main of tools/gate.py. */
#include "gate.h"

static const char DESCRIPTION[] =
    "The byte gate as one command: preflight, clean rebuild, checksum, coverage, regressions, layout, rules, ledger.";

static AvpArgParser *build_parser(AvpArena *a) {
    AvpArgParser *ap = avp_args_parser(a, &(AvpArgParserDesc){.prog = "gate", .description = DESCRIPTION,
                                                              .formatter = AVP_FORMAT_RAW_DESCRIPTION});
    avp_arg(ap, &(AvpArgDesc){.names = "--root", .dflt = AVP_DEF_NONE, .help = "repository root (default: this checkout)"});
    AvpArgMutex *mode = avp_args_mutex(ap, false);
    avp_arg_mutex(mode, &(AvpArgDesc){.names = "--quick", .action = AVP_ARG_STORE_TRUE,
                                      .help = "incremental check of reachable units (not a gate)"});
    avp_arg_mutex(mode, &(AvpArgDesc){.names = "--selftest", .action = AVP_ARG_STORE_TRUE,
                                      .help = "prove the gate rejects a changed DOL"});
    avp_arg(ap, &(AvpArgDesc){.names = "--since", .metavar = "REV",
                              .help = "--quick: compare against REV (default: last full gate)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--keep-going", .action = AVP_ARG_STORE_TRUE,
                              .help = "run every step after a red one"});
    avp_arg(ap, &(AvpArgDesc){.names = "--merge", .action = AVP_ARG_STORE_TRUE,
                              .help = "ledger and exceptions problems are red"});
    avp_arg(ap, &(AvpArgDesc){.names = "--allow-dirty", .action = AVP_ARG_STORE_TRUE,
                              .help = "gate a dirty tree (result DIRTY, never OK)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--pending-flip", .metavar = "UNIT",
                              .help = "unit.exe flip's gate: the only change from HEAD is UNIT NonMatching -> Matching "
                                      "in configure.py; green is GATE OK-PENDING, exit 3, never OK"});
    avp_arg(ap, &(AvpArgDesc){.names = "--no-objcache", .action = AVP_ARG_STORE_TRUE, .help = "build with SHSM_OBJCACHE=0"});
    avp_arg(ap, &(AvpArgDesc){.names = "--configure-exe", .metavar = "PATH",
                              .help = "the configure executable (default: configure.exe beside the gate)"});
    avp_arg(ap, &(AvpArgDesc){.names = "-j --jobs", .type = AVP_T_INT, .help = "ninja -j"});
    avp_arg(ap, &(AvpArgDesc){.names = "-v --verbose", .action = AVP_ARG_STORE_TRUE,
                              .help = "print every step's details"});
    avp_arg(ap, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE, .help = "print the snapshot as JSON"});
    avp_arg(ap, &(AvpArgDesc){.names = "--debug", .action = AVP_ARG_STORE_TRUE, .help = "tracebacks on errors"});
    return ap;
}

static const char *opt_str(AvpArena *a, AvpArgs *r, const char *dest) {
    if (avp_args_is_none(r, dest)) return NULL;
    return avp_cz(a, avp_args_str(r, dest));
}

static void print_out(const char *text) {
    fputs(text, stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

static const char *result_json(AvpArena *a, const char *line) {
    AvpJson *d = avp_json_obj(a);
    avp_json_set(a, d, AVP_S("result"), avp_json_strz(a, line));
    return avp_cz(a, avp_json_dumps(a, d, NULL));
}

static int run(AvpArena *a, const GateOpts *o, const GateMainHooks *h) {
    const char *root = avp_path_abspath(a, o->root ? o->root : avp_join(a, avp_exe_dir(), "..", ".."));
    if (o->since && !o->quick) shsm_raise(SHSM_EXC_USAGE, "--since needs --quick");
    if (o->pending_flip && (o->quick || o->selftest || o->allow_dirty))
        shsm_raise(SHSM_EXC_USAGE, "--pending-flip does not combine with --quick, --selftest or --allow-dirty");
    GateRunner runner = h && h->runner ? h->runner : gate_default_runner;
    void *rctx = h ? h->runner_ctx : NULL;
    const char *exe_dir = h && h->exe_dir ? h->exe_dir : avp_exe_dir();
    GateRulesTools tools = {0}; /* the linked checker and scanner */
    if (h && h->tools_via_runner) {
        GateRunnerTools *rt = AVP_NEW(a, GateRunnerTools);
        *rt = (GateRunnerTools){runner, rctx, NULL};
        tools = gate_rules_tools_via_runner(rt);
    }
    const char *line = NULL;
    if (o->selftest) {
        GateSelftest s = {runner, rctx, NULL, exe_dir};
        bool ok = gate_run_selftest(a, o, root, &s, &line);
        print_out(o->json ? result_json(a, line) : line);
        return ok ? 0 : 1;
    }
    if (o->quick) {
        GateQuick q = {runner, rctx, NULL, gate_load_units_real, NULL, {gate_index_linked, NULL}, tools, exe_dir, NULL,
                       NULL};
        bool ok = gate_run_quick(a, o, root, &q, &line);
        print_out(o->json ? result_json(a, line) : line);
        return ok ? 0 : 1;
    }
    Gate *g = gate_new(a, root, o, runner, rctx, NULL);
    g->exe_dir = exe_dir;
    if (h && h->check_tools) g->check_tools = h->check_tools;
    if (tools.scan) g->rules_tools = tools;
    if (h && h->setup) h->setup(g, h->setup_ctx);
    bool ok = gate_execute(g, &line);
    if (o->json) {
        AvpJson *d = avp_json_obj(a);
        for (size_t i = 0; i < avp_json_len(g->snapshot); i++) {
            const AvpJsonMember *m = avp_json_member(g->snapshot, i);
            if (avp_str_eqz(m->key, "functions") || avp_str_eqz(m->key, "objects")) continue;
            avp_json_set(a, d, m->key, m->val);
        }
        AvpJsonDumpOpts jo = AVP_JSON_INDENT(1);
        print_out(avp_cz(a, avp_json_dumps(a, d, &jo)));
    } else {
        print_out(line);
    }
    if (g->pending_ok) return GATE_EXIT_OK_PENDING;
    return ok ? 0 : 1;
}

int gate_main(int argc, char **argv, const GateMainHooks *hooks) {
    AvpArena *a = avp_arena();
    AvpArgParser *ap = build_parser(a);
    AvpArgs *r = avp_args_parse(ap, argc, argv);
    GateOpts o = {0};
    o.root = opt_str(a, r, "root");
    o.quick = avp_args_bool(r, "quick");
    o.selftest = avp_args_bool(r, "selftest");
    o.since = opt_str(a, r, "since");
    o.keep_going = avp_args_bool(r, "keep_going");
    o.merge = avp_args_bool(r, "merge");
    o.allow_dirty = avp_args_bool(r, "allow_dirty");
    o.pending_flip = opt_str(a, r, "pending_flip");
    o.no_objcache = avp_args_bool(r, "no_objcache");
    o.configure_exe = opt_str(a, r, "configure_exe");
    if (o.configure_exe && *o.configure_exe) o.configure_exe = avp_path_abspath(a, o.configure_exe);
    else o.configure_exe = NULL;
    o.jobs = avp_args_is_none(r, "jobs") ? 0 : avp_args_int(r, "jobs");
    o.verbose = avp_args_bool(r, "verbose");
    o.json = avp_args_bool(r, "json");
    o.debug = avp_args_bool(r, "debug");
    /* Python tests `args.since` / `args.pending_flip` for truth: an empty string is no value */
    if (o.root && !*o.root) o.root = NULL;
    if (o.since && !*o.since) o.since = NULL;
    if (o.pending_flip && !*o.pending_flip) o.pending_flip = NULL;
    int rc = 2;
    AvpTry t;
    if (AVP_TRY(&t)) {
        rc = run(a, &o, hooks);
        avp_try_end(&t);
    } else {
        fflush(stdout);
        if (t.code == SHSM_EXC_USAGE) {
            fprintf(stderr, "gate: error: %s\n", t.msg);
        } else {
            AvpStr s = shsm_exc_str(a, t.code, t.msg);
            fprintf(stderr, "gate: error: %s: %.*s\n", t.code == 200 ? "JSONDecodeError" : shsm_exc_class(t.code),
                    AVP_SF(s));
        }
        rc = 2;
    }
    return rc;
}
