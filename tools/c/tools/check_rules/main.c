/* check_rules.exe: tools/rules/check_rules.py in C (PORTING.md). The command line is the Python's (main(), L4733):
 *
 *     check_rules [paths ...] [--units UNIT ...] [--changed REV] [--json] [--root ROOT] [--tier TIER] [--no-summary]
 *                 [--functions] [--allow ALLOW] [--configure CONFIGURE] [--classes CLASSES] [--no-cache]
 *
 * with the same text report, --json document and exit codes (0 clean, 1 match-tier findings in Matching units,
 * 2 usage). The port's proofs keep three hidden dumps (they bypass the parser when one of them is the first
 * argument or appears anywhere on the line):
 *
 *     check_rules --dump-facts FILE [FILE ...] [--root DIR]
 *     check_rules --dump-facts-all [--root DIR]
 *
 * one compact JSON object per file and line (rules_lex.md "Facts dump"), the same bytes as
 * python -I tools/c/tools/check_rules/diff/dump_facts.py with the same arguments;
 *
 *     check_rules --dump-findings [FILE ...] [--dump-list LIST] [--dump-rules R4,R17] [--root DIR]
 *
 * the raw findings of Checker.check_file (rules_check.md), the same bytes as diff/dump_findings.py;
 *
 *     check_rules --dump-final [FILE ...] [--dump-list LIST] [--root DIR] [--classes P] [--configure P] [--allow P]
 *
 * the state Checker.run() leaves (rules_check.md "The final dump"), the same bytes as diff/dump_final.py. */
#include "rules_check.h"
#include "rules_classes.h"
#include "rules_cli.h"
#include <stdio.h>
#include <string.h>

static int usage(const char *msg) {
    fprintf(stderr, "check_rules: error: %s\n", msg);
    return 2;
}

static void finding_line(AvpBuf *b, const RlFinding *f) {
    AvpJsonW w;
    avp_jsonw_init(&w, b, &AVP_JSON_COMPACT);
    rl_finding_json(&w, f);
    avp_jsonw_finish(&w);
    avp_buf_putc(b, '\n');
}

static int dump_findings(const char *root, AvpStrVec *files, const char *rules) {
    AvpArena *a = avp_arena();
    RlConfig cfg;
    rl_config_load(a, &cfg, root, NULL, NULL);
    RlChecker *ck = rl_checker_new(a, root, files, &cfg);
    if (!ck) {
        fputs("{\"error\":\"FileNotFoundError\"}\n", stdout);
        return 0;
    }
    AvpStrVec scan = {0};
    if (files->n) {
        for (size_t i = 0; i < files->n; i++) avp_vec_push(&scan, files->v[i]);
    } else {
        for (size_t i = 0; i < ck->paths.n; i++) avp_vec_push(&scan, ck->paths.v[i]);
        avp_sort_strs(scan.v, scan.n);
    }
    rl_checker_prepare(ck);
    for (size_t i = 0; i < scan.n && !ck->failed; i++) rl_check_file(ck, rl_checker_parse(ck, scan.v[i]));
    if (ck->failed) {
        fputs("{\"error\":\"AttributeError\"}\n", stdout);
        return 0;
    }
    AvpSet *want = NULL;
    if (rules) {
        want = avp_set_new();
        AvpStrVec r = avp_str_split(avp_str(rules), AVP_S(","), -1);
        for (size_t i = 0; i < r.n; i++) avp_set_add(want, r.v[i]);
    }
    AvpBuf b;
    avp_buf_init(&b, avp_arena_new(0));
    for (size_t i = 0; i < ck->findings.n; i++) {
        const RlFinding *f = &ck->findings.v[i];
        if (want && !avp_set_has(want, avp_str(f->rule))) continue;
        finding_line(&b, f);
        if (b.n > (1u << 20)) {
            fwrite(b.p, 1, b.n, stdout);
            b.n = 0;
        }
    }
    fwrite(b.p, 1, b.n, stdout);
    return 0;
}

/* --dump-final: run() over the scan (default: every source in sorted order), then the state (rules_check.md). */
static int dump_final(const char *root, AvpStrVec *files, const char *classes, const char *configure,
                      const char *allow) {
    AvpArena *a = avp_arena();
    RlConfig cfg;
    rl_config_load(a, &cfg, root, configure, allow);
    RlChecker *ck = rl_checker_new(a, root, files, &cfg);
    if (!ck) {
        fputs("{\"error\":\"FileNotFoundError\"}\n", stdout);
        return 0;
    }
    if (files->n) {
        for (size_t i = 0; i < files->n; i++) avp_vec_push(&ck->scan, files->v[i]);
    } else {
        for (size_t i = 0; i < ck->paths.n; i++) avp_vec_push(&ck->scan, ck->paths.v[i]);
        avp_sort_strs(ck->scan.v, ck->scan.n);
    }
    const char *cpath = classes ? classes : avp_path_join(a, root, "build/R5WEA4/classes.json");
    ck->classes_data = rl_load_classes(a, cpath, &ck->classes_note);
    rl_checker_run(ck);
    if (ck->failed || ck->error) {
        printf("{\"error\":\"%s\"}\n", ck->failed ? "AttributeError" : ck->error);
        return 0;
    }
    AvpBuf b;
    avp_buf_init(&b, avp_arena_new(0));
    rl_dump_final(ck, &b);
    fwrite(b.p, 1, b.n, stdout);
    return 0;
}

static int run_dump(int argc, char **argv) {
    const char *root = ".";
    bool dump = false, all = false, findings = false, final = false;
    const char *rules = NULL, *classes = NULL, *configure = NULL, *allow = NULL;
    AvpStrVec files = {0};
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--root")) {
            if (++i >= argc) return usage("argument --root: expected one argument");
            root = argv[i];
        } else if (!strcmp(argv[i], "--dump-facts")) {
            dump = true;
        } else if (!strcmp(argv[i], "--dump-facts-all")) {
            dump = all = true;
        } else if (!strcmp(argv[i], "--dump-findings")) {
            findings = true;
        } else if (!strcmp(argv[i], "--dump-final")) {
            final = true;
        } else if (!strcmp(argv[i], "--classes") || !strcmp(argv[i], "--configure") || !strcmp(argv[i], "--allow")) {
            const char *opt = argv[i];
            if (++i >= argc) {
                fprintf(stderr, "check_rules: error: argument %s: expected one argument\n", opt);
                return 2;
            }
            if (!strcmp(opt, "--classes")) classes = argv[i];
            else if (!strcmp(opt, "--configure")) configure = argv[i];
            else allow = argv[i];
        } else if (!strcmp(argv[i], "--dump-rules")) {
            if (++i >= argc) return usage("argument --dump-rules: expected one argument");
            rules = argv[i];
        } else if (!strcmp(argv[i], "--dump-list")) {
            if (++i >= argc) return usage("argument --dump-list: expected one argument");
            AvpStr text;
            if (!rl_read_source(avp_arena(), argv[i], &text)) return usage("--dump-list: cannot read the list");
            AvpStrVec lines = avp_str_split(text, AVP_S("\n"), -1);
            for (size_t k = 0; k < lines.n; k++) {
                AvpStr ln = avp_str_strip(lines.v[k]);
                if (ln.n) avp_vec_push(&files, ln);
            }
            avp_vec_free(&lines);
        } else if ((dump || findings || final) && argv[i][0] != '-') {
            avp_vec_push(&files, avp_str(argv[i]));
        } else {
            fprintf(stderr, "check_rules: error: unrecognized argument with a dump: %s\n", argv[i]);
            return 2;
        }
    }
    if (final) return dump_final(root, &files, classes, configure, allow);
    if (findings) return dump_findings(root, &files, rules);
    if (!dump) return usage("no dump selected");
    if (!all && !files.n) return usage("argument --dump-facts: expected at least one argument");
    AvpArena *a = avp_arena();
    RlSources *S = rl_load_sources(a, root);
    AvpMap *storage = rl_project_storage_macros(S);
    AvpStrVec *targets = all ? &S->paths : &files;
    for (size_t i = 0; i < targets->n; i++) {
        AvpStr rel = targets->v[i];
        AvpArena *fa = avp_arena_new(0);
        AvpStr text;
        if (!rl_source_text(fa, S, root, rel, &text)) {
            fprintf(stderr, "check_rules: error: cannot read %.*s\n", AVP_SF(rel));
            avp_arena_free(fa);
            return 2;
        }
        RlParsed *P = rl_parse(fa, rel, text, storage);
        AvpBuf b;
        avp_buf_init(&b, fa);
        AvpJsonW w;
        avp_jsonw_init(&w, &b, &AVP_JSON_COMPACT);
        rl_dump_facts(&w, P);
        avp_jsonw_finish(&w);
        avp_buf_putc(&b, '\n');
        fwrite(b.p, 1, b.n, stdout);
        rl_parsed_free(P);
        avp_arena_free(fa);
    }
    return 0;
}

static bool is_dump_flag(const char *s) {
    return !strcmp(s, "--dump-facts") || !strcmp(s, "--dump-facts-all") || !strcmp(s, "--dump-findings") ||
           !strcmp(s, "--dump-final");
}

/* main() (L4733) */
static int run(int argc, char **argv) {
    for (int i = 1; i < argc; i++)
        if (is_dump_flag(argv[i])) return run_dump(argc, argv);
    AvpArena *a = avp_arena();
    AvpArgParser *p = avp_args_parser(a, &(AvpArgParserDesc){
        .prog = "check_rules.exe",
        .description = "Rule checker for the SHSM matching decomp (see tools/rules/README.md)."});
    avp_arg(p, &(AvpArgDesc){.names = "paths", .nargs = AVP_NARGS_STAR, .help = "files or directories (default: src/)"});
    avp_arg(p, &(AvpArgDesc){.names = "--units", .nargs = AVP_NARGS_PLUS, .metavar = "UNIT",
                             .help = "unit names (main/<path without extension>)"});
    avp_arg(p, &(AvpArgDesc){.names = "--changed", .metavar = "REV",
                             .help = "check source files changed against a git revision"});
    avp_arg(p, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE, .help = "machine-readable output"});
    avp_arg(p, &(AvpArgDesc){.names = "--root", .dflt = AVP_DEF_NONE,
                             .help = "repository root (default: two levels above this script)"});
    avp_arg(p, &(AvpArgDesc){.names = "--tier", .dflt = AVP_DEF_NONE,
                             .help = "comma list of tiers to print: match,checked,free,evidence,advisory"});
    avp_arg(p, &(AvpArgDesc){.names = "--no-summary", .action = AVP_ARG_STORE_TRUE,
                             .help = "omit the per-file and per-function summary"});
    avp_arg(p, &(AvpArgDesc){.names = "--functions", .action = AVP_ARG_STORE_TRUE,
                             .help = "list every function in the summary"});
    avp_arg(p, &(AvpArgDesc){.names = "--allow", .dflt = AVP_DEF_NONE,
                             .help = "exceptions record (default: docs/exceptions.md)"});
    avp_arg(p, &(AvpArgDesc){.names = "--configure", .dflt = AVP_DEF_NONE,
                             .help = "configure.py to read unit status from"});
    avp_arg(p, &(AvpArgDesc){.names = "--classes", .dflt = AVP_DEF_NONE,
                             .help = "classes.json of classes.exe scan, for the R13 slot check (default: "
                                     "build/R5WEA4/classes.json)"});
    avp_arg(p, &(AvpArgDesc){.names = "--no-cache", .action = AVP_ARG_STORE_TRUE,
                             .help = "do not read or write the per-file cache (also SHSM_RULES_CACHE=0); the output "
                                     "is the same"});
    AvpArgs *r = avp_args_parse(p, argc, argv);
    /* --no-cache and SHSM_RULES_CACHE: the C tool has no per-file cache yet (PORTING.md, stage 6); the Python's
     * output is the same with and without its cache, so both settings give the uncached run. */
    AvpStr root_arg = avp_args_str(r, "root");
    RlRunOpts o = {0};
    o.root = root_arg.n ? avp_cz(a, root_arg) : avp_path_abspath(a, avp_path_join(a, avp_exe_dir(), "../.."));
    AvpStr v = avp_args_str(r, "configure");
    o.configure = v.n ? avp_cz(a, v) : NULL;
    v = avp_args_str(r, "allow");
    o.allow = v.n ? avp_cz(a, v) : NULL;
    v = avp_args_str(r, "classes");
    o.classes = v.n ? avp_cz(a, v) : NULL;
    o.units = avp_args_strs(r, "units");
    o.changed = avp_args_str(r, "changed");
    o.paths = avp_args_strs(r, "paths");
    o.json = avp_args_bool(r, "json");
    o.no_summary = avp_args_bool(r, "no_summary");
    o.functions = avp_args_bool(r, "functions");
    o.tiers = avp_args_str(r, "tier");
    AvpBuf out, err;
    avp_buf_init(&out, avp_arena_new(0));
    avp_buf_init(&err, avp_arena_new(0));
    int rc = rl_run(a, &o, &out, &err);
    fwrite(out.p, 1, out.n, stdout);
    fflush(stdout);
    fwrite(err.p, 1, err.n, stderr);
    return rc;
}

AVP_MAIN(run, "check_rules")
