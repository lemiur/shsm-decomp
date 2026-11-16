/* ml_deps: merge_lane.py's DEFAULT_COMMANDS.
 *
 *   configure            configure.exe beside this executable (the C build plumbing; it reads configure.py)
 *   ninja, ninja_changes {ninja} [changes], {ninja} resolved per run as resolve_tool does
 *   attribute, layout    gatecheck.exe beside this executable (the C port of tools/gatecheck.py)
 *   ledger_*             ledger.exe beside this executable (the C port of tools/ledger.py)
 *   classes_scan         linked: tool_classes' scan under its default constructor/destructor base rule
 *   rules_changed/_tree  linked: tool_check_rules, main()'s --json path
 * A run command writes "$ <argv>" and its merged output to the log, as Ctx.run did; a linked one writes its text. */
#include "merge_lane.h"
#include "classes.h"
#include "classes_scan.h"
#include "rules_check.h"
#include "rules_classes.h"
#include "rules_cli.h"
#include "rules_sources.h"
#include "shsm_cli.h"
#include <stdio.h>
#include <string.h>

static i64 run_logged(Ml *ml, const char *const *argv, size_t argc, const char *log, const char *cwd, AvpStr *text) {
    AvpArena *a = ml->a;
    AvpStrVec parts = {0};
    for (size_t i = 0; i < argc; i++) avp_vec_push(&parts, avp_str(argv[i]));
    AvpBuf head;
    avp_buf_init(&head, a);
    avp_buf_putz(&head, "$ ");
    avp_buf_put(&head, avp_str_join(a, AVP_S(" "), parts.v, parts.n));
    avp_buf_putz(&head, "\n");
    if (!avp_write_file(log, head.p, head.n)) shsm_raise_io();
    const char **av = AVP_NEWN(a, const char *, argc + 1);
    for (size_t i = 0; i < argc; i++) av[i] = argv[i];
    av[argc] = NULL;
    AvpProcResult p = avp_run(&(AvpProcSpec){.argv = av, .cwd = cwd, .out = AVP_IO_FILE, .out_path = log,
                                             .out_append = true, .err = AVP_IO_STDOUT, .arena = a});
    i64 rc = p.returncode;
    if (p.status == AVP_PROC_START_FAILED) {
        AvpStr line = avp_str(avp_sprintf(a, "failed to start: %s\n", p.message ? p.message : "?"));
        avp_append_file(log, line.p, line.n);
        rc = 127;
    }
    if (!ml_read_text(a, log, text)) *text = AVP_S("");
    return rc;
}

static const char *beside(Ml *ml, const char *name) {
    return avp_path_join(ml->a, avp_exe_dir(), avp_sprintf(ml->a, "%s.exe", name));
}

/* ---- linked: classes scan ------------------------------------------------------------------------------------------- */
static i64 classes_linked(Ml *ml, const char *root, AvpStr *text) {
    AvpArena *a = ml->a;
    ClassesScanArgs sa = {0};
    sa.paths = classes_default_paths(a, root);
    if (!avp_isfile(sa.paths.splits)) sa.paths.splits = NULL;
    sa.base_rule = CLASSES_BASE_RULE_CTOR; /* fixed: SHSM_CLASSES_BASE_RULE is ignored, as by the gate */
    sa.root = root;
    AvpTry t;
    if (AVP_TRY(&t)) {
        classes_check_inputs(sa.paths.asm_dir, sa.paths.symbols);
        classes_run_scan(a, &sa, true);
        avp_try_end(&t);
        *text = avp_str(avp_sprintf(a, "classes scan (linked, base rule ctor): %s\n", sa.paths.out));
        return 0;
    }
    *text = avp_str(avp_sprintf(a, "classes.exe: error: %s\n", t.msg));
    return t.code == SHSM_EXC_USAGE ? 2 : 1;
}

/* ---- linked: check_rules ---------------------------------------------------------------------------------------- */
/* check_rules.py [--changed REV | --root ROOT --classes C --no-summary] --json: main()'s --json path. */
static i64 rules_linked(Ml *ml, const char *root, const char *changed, const char *classes, AvpStr *text) {
    AvpArena *a = ml->a;
    AvpTry t;
    if (AVP_TRY(&t)) {
        RlSources *S = rl_load_sources(a, root);
        RlConfig cfg;
        rl_config_load(a, &cfg, root, NULL, NULL);
        AvpStrVec scan = {0};
        const char *err = NULL;
        bool ok;
        if (changed) {
            ok = rl_changed_files(a, root, avp_str(changed), &scan, &err);
        } else {
            AvpStrVec paths = {0};
            avp_vec_push(&paths, AVP_S("src"));
            ok = rl_collect_files(a, root, &paths, &scan, &err);
        }
        if (!ok) {
            avp_try_end(&t);
            if (!err) {
                *text = AVP_S("merge_lane.exe: error: ValueError: path is on mount different from start\n");
                return 1;
            }
            *text = avp_str(avp_sprintf(a, "check_rules: %s\n", err));
            return 2;
        }
        AvpStrVec paths = {0}, texts = {0};
        for (size_t i = 0; i < S->paths.n; i++) {
            avp_vec_push(&paths, S->paths.v[i]);
            avp_vec_push(&texts, S->texts.v[i]);
        }
        AvpSet *have = avp_set_new();
        for (size_t i = 0; i < scan.n; i++) {
            if (avp_map_has(S->index, scan.v[i]) || avp_set_has(have, scan.v[i])) continue;
            AvpStr src;
            if (!rl_source_text(a, S, root, scan.v[i], &src)) {
                avp_try_end(&t);
                *text = avp_str(avp_sprintf(a, "merge_lane.exe: error: OSError: cannot read %.*s\n",
                                            AVP_SF(scan.v[i])));
                return 1;
            }
            avp_set_add(have, scan.v[i]);
            avp_vec_push(&paths, scan.v[i]);
            avp_vec_push(&texts, src);
        }
        RlChecker *ck = rl_checker_from(a, &paths, &texts, &cfg);
        for (size_t i = 0; i < scan.n; i++) avp_vec_push(&ck->scan, scan.v[i]);
        const char *cpath = classes ? classes : avp_path_join(a, root, "build/R5WEA4/classes.json");
        ck->classes_data = rl_load_classes(a, cpath, &ck->classes_note);
        rl_checker_run(ck);
        avp_try_end(&t);
        if (ck->failed) {
            *text = AVP_S("merge_lane.exe: error: AttributeError: the lexer met a character it cannot "
                          "classify\n");
            return 1;
        }
        if (ck->error) {
            *text = avp_str(avp_sprintf(a, "merge_lane.exe: error: %s: classes.json\n", ck->error));
            return 1;
        }
        AvpBuf b;
        avp_buf_init(&b, a);
        rl_report_json(ck, &b, avp_str(root), (AvpStr){0});
        *text = avp_buf_str(&b);
        return rl_exit_code(ck);
    }
    *text = avp_str(avp_sprintf(a, "merge_lane.exe: error: %s: %s\n", shsm_exc_class(t.code), t.msg));
    return 1;
}

/* ---- dispatch ------------------------------------------------------------------------------------------------------- */
i64 ml_default_cmd(Ml *ml, const char *key, const char *log, const MlExtra *extra, const char *cwd, AvpStr *text) {
    AvpArena *a = ml->a;
    i64 rc;
    if (!strcmp(key, "classes_scan")) {
        rc = classes_linked(ml, cwd, text);
        ml_write_text(log, *text);
        return rc;
    }
    if (!strcmp(key, "rules_changed") || !strcmp(key, "rules_tree")) {
        bool tree = !strcmp(key, "rules_tree");
        const char *root = tree ? (extra->root ? extra->root : ml->slot) : cwd;
        rc = rules_linked(ml, root, tree ? NULL : ml_onto_ref(ml), tree ? extra->classes : NULL, text);
        ml_write_text(log, *text);
        return rc;
    }
    const char *av[8];
    size_t n = 0;
    if (!strcmp(key, "ninja") || !strcmp(key, "ninja_changes")) {
        const char *ninja = ml_resolve_tool(a, cwd, "ninja");
        if (!ninja) {
            const char *p1[] = {cwd, "tools", "bin", "ninja.exe"}, *p2[] = {cwd, "tools", "bin", "ninja"};
            *text = avp_pyfmt(a, "ninja not found: neither %s nor %s exists and there is no ninja on PATH\n",
                              avp_path_joinv(a, 4, p1), avp_path_joinv(a, 4, p2));
            ml_write_text(log, *text);
            return 127;
        }
        av[n++] = ninja;
        if (!strcmp(key, "ninja_changes")) av[n++] = "changes";
    } else if (!strcmp(key, "build_tools")) {
        av[n++] = "cmd", av[n++] = "/c", av[n++] = "tools\\build_c.cmd";
    } else if (!strcmp(key, "configure")) {
        av[n++] = beside(ml, "configure");
    } else if (!strcmp(key, "attribute") || !strcmp(key, "layout")) {
        av[n++] = beside(ml, "gatecheck"), av[n++] = "--root", av[n++] = cwd;
        if (!strcmp(key, "attribute")) {
            av[n++] = "attribute";
        } else {
            av[n++] = "layout", av[n++] = "--all", av[n++] = "--status", av[n++] = "Matching";
        }
    } else if (!strcmp(key, "ledger_progress") || !strcmp(key, "ledger_exceptions")) {
        av[n++] = beside(ml, "ledger"), av[n++] = !strcmp(key, "ledger_progress") ? "progress" : "exceptions-check";
        av[n++] = "--root", av[n++] = cwd, av[n++] = "--report", av[n++] = ML_BASELINE;
    } else {
        avp_throw(AVP_ERR_OTHER, "KeyError: '%s'", key);
    }
    return run_logged(ml, av, n, log, cwd, text);
}
