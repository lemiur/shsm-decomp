/* rules_cli: check_rules.py's file selection and reports (L4625-4834). See rules_cli.h. */
#include "rules_cli.h"
#include "rules_classes.h"
#include "shsm_git.h"
#include <string.h>

static const char *const ALL_EXTS[] = {".c", ".cc", ".cp", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inc", NULL};

/* name.lower().endswith(SRC_EXTS + HDR_EXTS): the suffixes are ASCII, so an ASCII fold of the tail decides it. */
static bool source_name(AvpStr s) {
    for (int i = 0; ALL_EXTS[i]; i++) {
        size_t n = strlen(ALL_EXTS[i]);
        if (s.n < n) continue;
        bool eq = true;
        for (size_t k = 0; k < n && eq; k++) {
            char c = s.p[s.n - n + k];
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            eq = c == ALL_EXTS[i][k];
        }
        if (eq) return true;
    }
    return false;
}

/* os.path.relpath(p, root).replace("\\", "/"); false for relpath's ValueError */
static bool rel_slash(AvpArena *a, const char *p, const char *root, AvpStr *out) {
    char *r = NULL;
    if (!avp_path_relpath(a, p, root, &r)) return false;
    *out = avp_str_replace(a, avp_str(r), AVP_S("\\"), AVP_S("/"), -1);
    return true;
}

/* ---- collect_files (L4629) ---- */
bool rl_collect_files(AvpArena *a, const char *root, const AvpStrVec *paths, AvpStrVec *out, const char **err) {
    *err = NULL;
    for (size_t i = 0; i < paths->n; i++) {
        const char *p = avp_cz(a, paths->v[i]);
        const char *ap = avp_path_isabs(p) ? p : avp_path_join(a, root, p);
        if (avp_isdir(ap)) {
            AvpWalk *w = avp_walk_open(ap, true);
            AvpWalkItem item = {0};
            while (w && avp_walk_next(w, &item)) {
                avp_sort_strs(item.dirs.v, item.dirs.n); /* dn.sort() */
                AvpStrVec files = {0};
                for (size_t k = 0; k < item.files.n; k++) avp_vec_push(&files, item.files.v[k]);
                avp_sort_strs(files.v, files.n);
                const char *dp = avp_cz(a, item.dirpath);
                for (size_t k = 0; k < files.n; k++) {
                    if (!source_name(files.v[k])) continue;
                    AvpStr rel;
                    if (!rel_slash(a, avp_path_join(a, dp, avp_cz(a, files.v[k])), root, &rel)) {
                        avp_vec_free(&files);
                        avp_walk_close(w);
                        return false;
                    }
                    avp_vec_push(out, rel);
                }
                avp_vec_free(&files);
            }
            if (w) avp_walk_close(w);
        } else if (avp_isfile(ap)) {
            /* an assembly source file is never read as C, even when named (D-check_rules-asm-1) */
            size_t pn = strlen(p);
            if (pn >= 2 && p[pn - 2] == '.' && (p[pn - 1] == 's' || p[pn - 1] == 'S')) continue;
            AvpStr rel;
            if (!rel_slash(a, ap, root, &rel)) return false;
            avp_vec_push(out, rel);
        } else {
            *err = avp_sprintf(a, "no such file or directory: %s", p);
            return false;
        }
    }
    return true;
}

/* os.path.splitext(s)[0] */
static AvpStr stem(AvpArena *a, AvpStr s) {
    AvpStr r, e;
    avp_path_splitext(avp_cz(a, s), &r, &e);
    return r;
}

/* ---- units_to_files (L4646) ---- */
bool rl_units_to_files(AvpArena *a, const AvpStrVec *names, const AvpStrVec *source_paths, AvpStrVec *out,
                       const char **err) {
    *err = NULL;
    for (size_t i = 0; i < names->n; i++) {
        AvpStr n = names->v[i];
        AvpStr key = avp_str_startswith(n, AVP_S("main/")) ? avp_str_slice(n, 5, (i64)n.n) : n;
        if (source_name(key)) key = stem(a, key);
        bool found = false;
        for (size_t k = 0; k < source_paths->n; k++) {
            AvpStr p = source_paths->v[k];
            if (!avp_str_startswith(p, AVP_S("src/"))) continue;
            if (!avp_str_eq(stem(a, avp_str_slice(p, 4, (i64)p.n)), key)) continue;
            avp_vec_push(out, p);
            found = true;
        }
        if (!found) {
            *err = avp_sprintf(a, "no source file for unit '%.*s'", AVP_SF(n));
            return false;
        }
    }
    return true;
}

/* An assembly source unit (`src/<unit>.s`, user's ruling 2026-11-14): the checker reads no `.s` file, so --units
 * selects nothing for it rather than failing with "no source file" (DEVIATIONS.md D-check_rules-asm-1). */
bool rl_asm_unit(AvpArena *a, const char *root, AvpStr name) {
    AvpStr key = avp_str_startswith(name, AVP_S("main/")) ? avp_str_slice(name, 5, (i64)name.n) : name;
    if (key.n >= 2 && key.p[key.n - 2] == '.' && (key.p[key.n - 1] == 's' || key.p[key.n - 1] == 'S'))
        key = avp_strn(key.p, key.n - 2);
    if (!key.n) return false;
    return avp_isfile(avp_path_join(a, root, avp_cz(a, avp_str_concat(a, AVP_S("src/"), avp_str_concat(a, key, AVP_S(".s"))))));
}

/* ---- changed_files (L4658) ---- */
/* str(e) of subprocess's CalledProcessError for the Python's argv (OSError when git could not be started) */
static const char *git_error(AvpArena *a, const char *const *pyargv, const ShsmGitRun *r) {
    if (!r->started) return "[WinError 2] The system cannot find the file specified";
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, "Command '[");
    for (int i = 0; pyargv[i]; i++) {
        if (i) avp_buf_putz(&b, ", ");
        avp_buf_put(&b, avp_repr_str(a, avp_str(pyargv[i])));
    }
    avp_buf_printf(&b, "]' returned non-zero exit status %lld.", (long long)r->rc);
    return avp_buf_cz(&b);
}

/* git's NUL-separated name list (-z: a path with a space or a non-ASCII character stays whole). pyargv is the
 * command the Python ran (without -z), for its error text. */
static bool git_names(AvpArena *a, const char *root, const char *const *args, size_t n, const char *const *pyargv,
                      AvpStr rev, AvpStrVec *out, const char **err) {
    ShsmGitRun r = shsm_git_run(a, root, args, n, &(ShsmGitOpts){.quotepath_off = true, .binary = true,
                                                                   .soft_start = true});
    if (!shsm_git_ok(&r)) {
        *err = avp_sprintf(a, "git failed for --changed %.*s: %s", AVP_SF(rev), git_error(a, pyargv, &r));
        return false;
    }
    *out = shsm_git_split_nul(r.out);
    return true;
}

bool rl_changed_files(AvpArena *a, const char *root, AvpStr rev, AvpStrVec *out, const char **err) {
    *err = NULL;
    const char *revz = avp_cz(a, rev);
    const char *diff[] = {"diff", "--name-only", "-z", "--diff-filter=ACMR", revz, "--"};
    const char *others[] = {"ls-files", "-z", "--others", "--exclude-standard"};
    const char *py_diff[] = {"git", "diff", "--name-only", "--diff-filter=ACMR", revz, "--", NULL};
    const char *py_others[] = {"git", "ls-files", "--others", "--exclude-standard", NULL};
    AvpStrVec da = {0}, db = {0};
    if (!git_names(a, root, diff, AVP_ARRAY_LEN(diff), py_diff, rev, &da, err)) return false;
    if (!git_names(a, root, others, AVP_ARRAY_LEN(others), py_others, rev, &db, err)) {
        avp_vec_free(&da);
        return false;
    }
    AvpSet *seen = avp_set_new();
    for (int pass = 0; pass < 2; pass++) {
        const AvpStrVec *v = pass ? &db : &da;
        for (size_t i = 0; i < v->n; i++) {
            AvpStr p = v->v[i];
            if (!source_name(p)) continue;
            if (!avp_str_startswith(p, AVP_S("src/")) && !avp_str_startswith(p, AVP_S("include/"))) continue;
            if (!avp_isfile(avp_path_join(a, root, avp_cz(a, p)))) continue;
            if (avp_set_has(seen, p)) continue;
            avp_set_add(seen, p);
            avp_vec_push(out, p);
        }
    }
    avp_set_free(seen);
    avp_vec_free(&da);
    avp_vec_free(&db);
    return true;
}

/* ---- the shown findings (main, L4799) ---- */
static int by_path_line_rule(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const RlFinding *f = *(const RlFinding *const *)x, *g = *(const RlFinding *const *)y;
    int c = avp_str_cmp(f->path, g->path);
    if (c) return c;
    if (f->line != g->line) return f->line < g->line ? -1 : 1;
    return strcmp(f->rule, g->rule);
}

RlFindingPtrs rl_shown_findings(RlChecker *ck, AvpStr tiers) {
    RlFindingPtrs v = {0};
    AvpSet *want = NULL;
    if (tiers.p && tiers.n) {
        want = avp_set_new();
        AvpStrVec t = avp_str_split(tiers, AVP_S(","), -1);
        for (size_t i = 0; i < t.n; i++) avp_set_add(want, t.v[i]);
        avp_vec_free(&t);
    }
    for (size_t i = 0; i < ck->findings.n; i++) {
        const RlFinding *f = &ck->findings.v[i];
        if (want && !avp_set_has(want, avp_str(f->tier))) continue;
        *RL_PUSH(ck->a, &v) = f;
    }
    if (want) avp_set_free(want);
    avp_sort(v.v, v.n, sizeof v.v[0], by_path_line_rule, NULL, false);
    return v;
}

/* ---- summarize (L4690) ---- */
static const char *const TIERS[] = {"match", "checked", "free", "evidence", "advisory"};

static int tier_index(const char *t) {
    for (int i = 0; i < 5; i++)
        if (!strcmp(t, TIERS[i])) return i;
    return -1;
}

static i64 count_of(const RlFunctionRec *r, const char *tier) {
    for (int t = 0; t < r->ntiers; t++)
        if (!strcmp(r->tiers[t], tier)) return r->counts[t];
    return 0;
}

static bool is_matching_or_unlisted(AvpStr s) {
    return avp_str_eqz(s, "Matching") || avp_str_eqz(s, "Unlisted");
}

void rl_summarize(RlChecker *ck, AvpStrVec *out, bool all_functions) {
    AvpArena *a = ck->a;
    /* by_file: path -> counts per tier of the violations */
    AvpMap *by_file = avp_map_new();
    for (size_t i = 0; i < ck->findings.n; i++) {
        const RlFinding *f = &ck->findings.v[i];
        if (strcmp(f->status, "violation")) continue;
        bool created;
        AvpVal *v = avp_map_put(by_file, f->path, &created);
        if (created) v->p = avp_alloc(a, sizeof(i64) * 5);
        int t = tier_index(f->tier);
        if (t >= 0) ((i64 *)v->p)[t]++;
    }
    avp_vec_push(out, AVP_S(""));
    avp_vec_push(out, AVP_S("# summary by file"));
    static const i64 zero[5] = {0};
    for (size_t i = 0; i < ck->checked.n; i++) {
        AvpStr path = ck->checked.v[i];
        RlParsed *P = rl_checker_parse(ck, path);
        AvpVal *v = avp_map_get(by_file, path);
        const i64 *c = v ? (const i64 *)v->p : zero;
        AvpStr status = avp_str_startswith(path, AVP_S("src/")) ? rl_checker_status_of(ck, path) : AVP_S("header");
        avp_vec_push(out, avp_pyfmt(a, "# %-52S %-11S functions=%-3d match=%d checked=%d free=%d evidence=%d "
                                       "advisory=%d", &path, &status, (i64)P->p.funcs.n, c[0], c[1], c[2], c[3],
                                    c[4]));
    }
    avp_map_free(by_file);
    avp_vec_push(out, AVP_S(""));
    avp_vec_push(out, AVP_S("# summary by function (clean functions omitted; --functions lists all)"));
    i64 pend = 0;
    for (size_t i = 0; i < ck->functions.n; i++) {
        const RlFunctionRec *r = &ck->functions.v[i];
        if (!strcmp(r->status, "MATCH-PENDING")) pend++;
        i64 sum = 0;
        for (int t = 0; t < r->ntiers; t++) sum += r->counts[t];
        if (!strcmp(r->status, "ok") && !sum && !all_functions) continue;
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_pyfmt(&b, "# %S::%S [%d-%d] %s match=%d checked=%d free=%d evidence=%d advisory=%d", &r->path,
                      &r->name, r->line, r->end_line, r->status, count_of(r, "match"), count_of(r, "checked"),
                      count_of(r, "free"), count_of(r, "evidence"), count_of(r, "advisory"));
        if (r->match_rules.n) {
            avp_buf_putz(&b, " rules=");
            avp_buf_put(&b, avp_str_join(a, AVP_S(","), r->match_rules.v, r->match_rules.n));
        }
        if (!is_matching_or_unlisted(r->unit_status) && avp_str_startswith(r->path, AVP_S("src/")))
            avp_buf_pyfmt(&b, " (unit %S: not blocking)", &r->unit_status);
        avp_vec_push(out, avp_buf_str(&b));
    }
    i64 blocking = 0, excepted = 0, allowed = 0;
    for (size_t i = 0; i < ck->findings.n; i++) {
        const RlFinding *f = &ck->findings.v[i];
        if (rl_blocking(ck, f)) blocking++;
        if (!strcmp(f->status, "excepted")) excepted++;
        if (!strcmp(f->status, "allowed")) allowed++;
    }
    avp_vec_push(out, AVP_S(""));
    const RlR13 *r = &ck->r13;
    if (r->note.p && r->note.n) {
        avp_vec_push(out, avp_pyfmt(a, "# %S", &r->note));
    } else {
        avp_vec_push(out, avp_pyfmt(a, "# R13 slots: classes=%d matched=%d unmatched=%d not-modelled=%d added=%d "
                                       "missing=%d order=%d override=%d advisory=%d",
                                    r->classes, (i64)r->matched.n, (i64)r->unmatched.n, (i64)r->skipped.n, r->added,
                                    r->missing, r->order, r->override_, r->advisory));
    }
    avp_vec_push(out, avp_pyfmt(a, "# functions=%d match-pending=%d blocking-findings=%d exceptions=%d allowed=%d",
                                (i64)ck->functions.n, pend, blocking, excepted, allowed));
}

/* ---- main's text report (L4816) ---- */
static int by_path_name(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const RlFunctionRec *f = *(const RlFunctionRec *const *)x, *g = *(const RlFunctionRec *const *)y;
    int c = avp_str_cmp(f->path, g->path);
    return c ? c : avp_str_cmp(f->name, g->name);
}

AvpStr rl_report_text(RlChecker *ck, AvpStr tiers, bool no_summary, bool all_functions) {
    AvpArena *a = ck->a;
    RlFindingPtrs shown = rl_shown_findings(ck, tiers);
    AvpStrVec out = {0};
    bool side = false;
    for (size_t i = 0; i < shown.n; i++) {
        if (!strcmp(shown.v[i]->status, "violation")) avp_vec_push(&out, rl_finding_text(a, shown.v[i]));
        else side = true;
    }
    if (side) {
        avp_vec_push(&out, AVP_S(""));
        avp_vec_push(&out, AVP_S("# exceptions and listed originals (reported apart, not violations)"));
        for (size_t i = 0; i < shown.n; i++)
            if (strcmp(shown.v[i]->status, "violation")) avp_vec_push(&out, rl_finding_text(a, shown.v[i]));
    }
    if (!no_summary) rl_summarize(ck, &out, all_functions);
    /* sorted({(path, name) ...}) of the MATCH-PENDING functions in Matching units */
    RL_VEC(const RlFunctionRec *) pend = {0};
    for (size_t i = 0; i < ck->functions.n; i++) {
        const RlFunctionRec *r = &ck->functions.v[i];
        if (!strcmp(r->status, "MATCH-PENDING") && avp_str_eqz(rl_checker_status_of(ck, r->path), "Matching"))
            *RL_PUSH(a, &pend) = r;
    }
    avp_sort(pend.v, pend.n, sizeof pend.v[0], by_path_name, NULL, false);
    for (size_t i = 0; i < pend.n; i++) {
        if (i && !by_path_name(&pend.v[i - 1], &pend.v[i], NULL)) continue; /* a set: one line per (path, name) */
        avp_vec_push(&out, avp_pyfmt(a, "# MATCH-PENDING in a Matching unit: %S::%S", &pend.v[i]->path,
                                     &pend.v[i]->name));
    }
    avp_vec_push(&out, rl_exit_code(ck) ? AVP_S("# result: FAIL: match-tier findings in Matching units")
                                        : AVP_S("# result: ok"));
    AvpStr text = avp_str_join(a, AVP_S("\n"), out.v, out.n);
    avp_vec_free(&out);
    return avp_str_concat(a, text, AVP_S("\n"));
}

/* ---- main's --json document (L4801) ---- */
static void finding_list(AvpJsonW *w, const RlFindingPtrs *shown, const char *status) {
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < shown->n; i++)
        if (!strcmp(shown->v[i]->status, status)) rl_finding_json_full(w, shown->v[i]);
    avp_jsonw_end_array(w);
}

static void str_list(AvpJsonW *w, const AvpStr *v, size_t n) {
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < n; i++) avp_jsonw_str(w, v[i]);
    avp_jsonw_end_array(w);
}

/* sorted(HAND_REVIEWED, key=lambda r: int(r[1:])) */
static const char *const HAND_REVIEWED[] = {"R5",  "R6",  "R8",  "R10", "R12", "R16", "R20", "R23",
                                            "R24", "R27", "R28", "R29", "R30", "R32", "R34", "R36", NULL};

void rl_report_json(RlChecker *ck, AvpBuf *b, AvpStr root, AvpStr tiers) {
    RlFindingPtrs shown = rl_shown_findings(ck, tiers);
    AvpJsonW w;
    avp_jsonw_init(&w, b, &AVP_JSON_INDENT(2));
    avp_jsonw_begin_object(&w);
    avp_jsonw_kv_str(&w, "root", root);
    avp_jsonw_key(&w, AVP_S("scanned"));
    str_list(&w, ck->scan.v, ck->scan.n);
    avp_jsonw_key(&w, AVP_S("checked"));
    str_list(&w, ck->checked.v, ck->checked.n);
    avp_jsonw_key(&w, AVP_S("units"));
    avp_jsonw_begin_object(&w);
    AvpMapIter it = {0};
    while (avp_map_next(ck->cfg.unit_status, &it)) avp_jsonw_kv_str(&w, avp_cz(ck->a, it.key), *(AvpStr *)it.val->p);
    avp_jsonw_end_object(&w);
    avp_jsonw_key(&w, AVP_S("findings"));
    finding_list(&w, &shown, "violation");
    avp_jsonw_key(&w, AVP_S("exceptions"));
    finding_list(&w, &shown, "excepted");
    avp_jsonw_key(&w, AVP_S("allowed"));
    finding_list(&w, &shown, "allowed");
    avp_jsonw_key(&w, AVP_S("exception_comments"));
    avp_jsonw_begin_array(&w);
    for (size_t i = 0; i < ck->exceptions.n; i++) rl_json_exception(&w, &ck->exceptions.v[i], false);
    avp_jsonw_end_array(&w);
    avp_jsonw_key(&w, AVP_S("functions"));
    avp_jsonw_begin_array(&w);
    for (size_t i = 0; i < ck->functions.n; i++) rl_json_function(&w, &ck->functions.v[i]);
    avp_jsonw_end_array(&w);
    avp_jsonw_key(&w, AVP_S("blocking"));
    avp_jsonw_begin_array(&w);
    for (size_t i = 0; i < ck->findings.n; i++)
        if (rl_blocking(ck, &ck->findings.v[i])) rl_finding_json_full(&w, &ck->findings.v[i]);
    avp_jsonw_end_array(&w);
    avp_jsonw_key(&w, AVP_S("r13"));
    rl_json_r13(&w, &ck->r13);
    avp_jsonw_key(&w, AVP_S("hand_reviewed"));
    avp_jsonw_begin_array(&w);
    for (int i = 0; HAND_REVIEWED[i]; i++) avp_jsonw_strz(&w, HAND_REVIEWED[i]);
    avp_jsonw_end_array(&w);
    avp_jsonw_kv_i64(&w, "exit", rl_exit_code(ck));
    avp_jsonw_end_object(&w);
    avp_jsonw_finish(&w);
    avp_buf_putc(b, '\n');
}

/* ---- one run (main, L4733 below argparse) ---- */
static int run_traceback(AvpBuf *err, const char *cls, const char *why) {
    avp_buf_printf(err, "rules.exe: error: %s: %s\n", cls, why);
    return 1;
}

int rl_run(AvpArena *a, const RlRunOpts *o, AvpBuf *out, AvpBuf *err) {
    const char *root = o->root;
    const char *opt_names[] = {"--configure", "--allow"};
    const char *opt_vals[2] = {o->configure, o->allow};
    for (int i = 0; i < 2; i++)
        if (opt_vals[i] && !avp_isfile(opt_vals[i])) {
            avp_buf_printf(err, "check_rules: %s: no such file: %s\n", opt_names[i], opt_vals[i]);
            return 2;
        }
    RlSources *S = rl_load_sources(a, root);
    RlConfig cfg;
    rl_config_load(a, &cfg, root, opt_vals[0], opt_vals[1]);
    AvpStrVec scan = {0};
    const char *why = NULL;
    bool ok;
    if (o->units.n) {
        AvpStrVec units = {0};
        for (size_t i = 0; i < o->units.n; i++)
            if (!rl_asm_unit(a, root, o->units.v[i])) avp_vec_push(&units, o->units.v[i]);
        ok = rl_units_to_files(a, &units, &S->paths, &scan, &why);
    } else if (o->changed.n) {
        ok = rl_changed_files(a, root, o->changed, &scan, &why);
    } else {
        AvpStrVec paths = {0};
        for (size_t i = 0; i < o->paths.n; i++) avp_vec_push(&paths, o->paths.v[i]);
        if (!paths.n) avp_vec_push(&paths, AVP_S("src"));
        ok = rl_collect_files(a, root, &paths, &scan, &why);
    }
    if (!ok) {
        if (!why) return run_traceback(err, "ValueError", "path is on mount different from start");
        avp_buf_printf(err, "check_rules: %s\n", why);
        return 2;
    }
    /* sources[p] = open(root/p).read() for each scanned path not loaded */
    AvpStrVec paths = {0}, texts = {0};
    for (size_t i = 0; i < S->paths.n; i++) {
        avp_vec_push(&paths, S->paths.v[i]);
        avp_vec_push(&texts, S->texts.v[i]);
    }
    AvpSet *have = avp_set_new();
    for (size_t i = 0; i < scan.n; i++) {
        if (avp_map_has(S->index, scan.v[i]) || avp_set_has(have, scan.v[i])) continue;
        AvpStr text;
        if (!rl_source_text(a, S, root, scan.v[i], &text)) {
            avp_set_free(have);
            return run_traceback(err, "OSError", avp_sprintf(a, "cannot read %.*s", AVP_SF(scan.v[i])));
        }
        avp_set_add(have, scan.v[i]);
        avp_vec_push(&paths, scan.v[i]);
        avp_vec_push(&texts, text);
    }
    avp_set_free(have);
    RlChecker *ck = rl_checker_from(a, &paths, &texts, &cfg);
    for (size_t i = 0; i < scan.n; i++) avp_vec_push(&ck->scan, scan.v[i]);
    const char *cpath = o->classes ? o->classes : avp_path_join(a, root, "build/R5WEA4/classes.json");
    ck->classes_data = rl_load_classes(a, cpath, &ck->classes_note);
    rl_checker_run(ck);
    if (ck->failed) return run_traceback(err, "AttributeError", "the lexer met a character it cannot classify");
    if (ck->error) return run_traceback(err, ck->error, "classes.json");
    if (o->json)
        rl_report_json(ck, out, avp_str(root), o->tiers);
    else
        avp_buf_put(out, rl_report_text(ck, o->tiers, o->no_summary, o->functions));
    return rl_exit_code(ck);
}

/* ---- ledger.run_checker ---- */
AvpJson *rl_checker_json_of(AvpArena *a, int rc, AvpStr out, AvpStr err, const char **why) {
    *why = NULL;
    if (rc != 0 && rc != 1) {
        AvpStr e = avp_u8_slice(avp_str_strip(err), 0, 300);
        *why = avp_sprintf(a, "rule checker failed (exit %d): %.*s", rc, AVP_SF(e));
        return NULL;
    }
    AvpJsonError je;
    AvpJson *doc = avp_json_parse(a, out, &je);
    if (!doc) {
        AvpStr m = avp_json_error_str(a, &je);
        *why = avp_sprintf(a, "rule checker printed no JSON: %.*s", AVP_SF(m));
    }
    return doc;
}

AvpJson *rl_checker_json(AvpArena *a, const char *root, int *rc, const char **why) {
    RlRunOpts o = {0};
    o.root = root;
    o.json = true;
    AvpBuf out, err;
    avp_buf_init(&out, a);
    avp_buf_init(&err, a);
    int code = rl_run(a, &o, &out, &err);
    if (rc) *rc = code;
    return rl_checker_json_of(a, code, avp_buf_str(&out), avp_buf_str(&err), why);
}
