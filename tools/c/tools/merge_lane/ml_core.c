/* ml_core: merge_lane.py's helpers, Ctx (git, run) and driver (parse_args, run, main). */
#include "merge_lane.h"
#include "shsm_cli.h"
#include "shsm_text.h"
#include <stdio.h>
#include <string.h>

static const char *const STEP_ORDER[] = {"preflight", "rebase", "gate", "changes", "rules", "layout", "ledger", "merge"};
static const char *const CORE_STEPS[] = {"rebase", "gate", "changes", "rules", "layout"};
static const char *const UPDATE_NEEDS[] = {"gate", "changes", "rules", "layout"};
static const MlStepFn STEP_FUNCS[] = {ml_step_preflight, ml_step_rebase, ml_step_gate, ml_step_changes,
                                      ml_step_rules, ml_step_layout, ml_step_ledger, ml_step_merge};
#define NSTEPS (sizeof(STEP_ORDER) / sizeof(STEP_ORDER[0]))
#define NCORE (sizeof(CORE_STEPS) / sizeof(CORE_STEPS[0]))

/* ---- small helpers ----------------------------------------------------------------------------------------------- */
void ml_out(Ml *ml, const char *line) {
    if (ml->full_log) {
        avp_fwrite(ml->full_log, line, strlen(line));
        avp_fwrite(ml->full_log, "\n", 1);
        avp_fflush(ml->full_log);
    }
    if (ml->hooks && ml->hooks->out) {
        avp_buf_putz(ml->hooks->out, line);
        avp_buf_putc(ml->hooks->out, '\n');
        return;
    }
    fputs(line, stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

void ml_fail(Ml *ml, const char *msg, const char *summary) {
    ml->fail_msg = msg ? msg : "";
    ml->fail_summary = summary;
    avp_throw(ML_STEP_FAIL, "StepFail");
}

const char *ml_join_ev(Ml *ml, const AvpStrVec *ev) {
    return avp_cz(ml->a, avp_str_join(ml->a, AVP_S("\n"), ev->v, ev->n));
}

void ml_ev(Ml *ml, AvpStrVec *ev, const char *line) {
    avp_vec_push(ev, avp_str(avp_strdup(ml->a, line)));
}

AvpStrVec ml_tail(AvpArena *a, AvpStr text, size_t n) {
    (void)a;
    AvpStrVec lines = avp_str_splitlines(text, false), keep = {0};
    for (size_t i = 0; i < lines.n; i++)
        if (avp_str_strip(lines.v[i]).n) avp_vec_push(&keep, lines.v[i]);
    if (keep.n > n) {
        AvpStrVec t = {0};
        for (size_t i = keep.n - n; i < keep.n; i++) avp_vec_push(&t, keep.v[i]);
        return t;
    }
    return keep;
}

void ml_ev_tail(Ml *ml, AvpStrVec *ev, AvpStr text, size_t n) {
    AvpStrVec t = ml_tail(ml->a, text, n);
    for (size_t i = 0; i < t.n; i++) ml_ev(ml, ev, ML_F(ml, "    %S", &t.v[i]));
}

const char *ml_norm(AvpArena *a, const char *path) {
    return avp_path_normcase(a, avp_path_realpath(a, path));
}

bool ml_norm_eq(AvpArena *a, const char *x, const char *y) {
    return strcmp(ml_norm(a, x), ml_norm(a, y)) == 0;
}

const char *ml_short(AvpArena *a, const char *sha) {
    return avp_cz(a, avp_u8_slice(avp_str(sha ? sha : ""), 0, 9));
}

const char *ml_unit_of(AvpArena *a, AvpStr src_rel) {
    const char *p = avp_cz(a, src_rel);
    AvpStr root, ext;
    avp_path_splitext(p, &root, &ext);
    return avp_sprintf(a, "main/%.*s", AVP_SF(root));
}

/* OBJ_RE: Object\(\s*(Matching|NonMatching|Equivalent)\s*,\s*"([^"]+)" over the lines not starting with '#'. */
static size_t skip_ws(AvpStr s, size_t i) {
    while (i < s.n) {
        size_t w = shsm_ws_len(s, i);
        if (!w) break;
        i += w;
    }
    return i;
}

AvpMap *ml_parse_objects(AvpArena *a, AvpStr text) {
    AvpStrVec lines = avp_str_split(text, AVP_S("\n"), -1), keep = {0};
    for (size_t i = 0; i < lines.n; i++)
        if (!avp_str_startswith(avp_str_lstrip(lines.v[i]), AVP_S("#"))) avp_vec_push(&keep, lines.v[i]);
    AvpStr s = avp_str_join(a, AVP_S("\n"), keep.v, keep.n);
    AvpMap *m = avp_map_new();
    static const char *const ST[] = {"Matching", "NonMatching", "Equivalent"};
    size_t i = 0;
    while (i < s.n) {
        i64 k = ML_FIND(avp_str_slice(s, (i64)i, AVP_END), AVP_S("Object("));
        if (k < 0) break;
        size_t at = i + (size_t)k, j = skip_ws(s, at + 7);
        const char *st = NULL;
        for (int t = 0; t < 3 && !st; t++) {
            size_t n = strlen(ST[t]);
            if (j + n <= s.n && memcmp(s.p + j, ST[t], n) == 0) st = ST[t], j += n;
        }
        if (st) {
            j = skip_ws(s, j);
            if (j < s.n && s.p[j] == ',') {
                j = skip_ws(s, j + 1);
                if (j < s.n && s.p[j] == '"') {
                    size_t q = j + 1;
                    while (q < s.n && s.p[q] != '"') q++;
                    if (q < s.n && q > j + 1) {
                        AvpVal *v = avp_map_put(m, avp_str_dup(a, avp_strn(s.p + j + 1, q - j - 1)), NULL);
                        v->p = (void *)st;
                        i = q + 1;
                        continue;
                    }
                }
            }
        }
        i = at + 1;
    }
    return m;
}

const char *ml_classify_conflict(AvpStr path) {
    if (avp_str_eqz(path, "config/R5WEA4/baseline_report.json") || avp_str_eqz(path, "config/R5WEA4/report.json") ||
        avp_str_eqz(path, "README.md") || avp_str_startswith(path, AVP_S("build/")))
        return "generated";
    if (avp_str_eqz(path, ML_PROGRESS)) return "ledger";
    if (avp_str_eqz(path, "docs/exceptions.md")) return "exceptions";
    return "manual";
}

const char *ml_resolve_tool(AvpArena *a, const char *root, const char *name) {
    const char *cands[2] = {avp_sprintf(a, "%s.exe", name), name};
    for (int i = 0; i < 2; i++) {
        const char *p = avp_path_joinv(a, 4, (const char *const[]){root, "tools", "bin", cands[i]});
        if (avp_isfile(p)) return p;
    }
    return avp_which(a, name, NULL, NULL);
}

AvpStr ml_decode(AvpArena *a, AvpStr bytes) {
    AvpStr out;
    if (!avp_decode_text(a, bytes, AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_REPLACE, AVP_NL_EMPTY), &out)) return bytes;
    return out;
}

void ml_write_text(const char *path, AvpStr text) {
    if (!avp_write_text(path, text, AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_STRICT, AVP_NL_DEFAULT))) shsm_raise_io();
}

bool ml_read_text(AvpArena *a, const char *path, AvpStr *text) {
    AvpBytes b = {0};
    if (!avp_read_all(a, path, &b)) return false;
    return avp_decode_text(a, (AvpStr){(const char *)b.v, b.n}, AVP_TM_UTF8_REPLACE, text);
}

/* ---- git ----------------------------------------------------------------------------------------------------------- */
MlGit ml_git(Ml *ml, const char *cwd, const AvpEnv *env, const char *const *args) {
    size_t n = 0;
    while (args[n]) n++;
    AvpArena *a = ml->a;
    const char *where = cwd ? cwd : ml->slot;
    MlGit res = {0};
    if (ml->hooks && ml->hooks->git && ml->hooks->git(ml->hooks->ctx, ml, args, n, where, &res)) return res;
    const char **av = AVP_NEWN(a, const char *, n + 6);
    size_t k = 0;
    av[k++] = "git";
    av[k++] = "-c";
    av[k++] = "core.quotepath=off";
    av[k++] = "-C";
    av[k++] = where;
    for (size_t i = 0; i < n; i++) av[k++] = args[i];
    av[k] = NULL;
    AvpProcResult p = avp_run(&(AvpProcSpec){.argv = av, .env = env, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE,
                                             .arena = a});
    if (p.status == AVP_PROC_START_FAILED) avp_throw(AVP_ERR_PROC, "%s", p.message ? p.message : "cannot run git");
    res.rc = p.returncode;
    res.out = ml_decode(a, p.out);
    res.err = ml_decode(a, p.err);
    return res;
}

const char *ml_rev(Ml *ml, const char *rev, const char *cwd) {
    MlGit g = ML_GIT_IN(ml, cwd, "rev-parse", "--verify", "-q", avp_sprintf(ml->a, "%s^{commit}", rev));
    return g.rc == 0 ? avp_cz(ml->a, avp_str_strip(g.out)) : NULL;
}

bool ml_show_file(Ml *ml, const char *rev, const char *path, AvpStr *bytes) {
    const char *av[] = {"git", "-C", ml->slot, "cat-file", "blob", avp_sprintf(ml->a, "%s:%s", rev, path), NULL};
    AvpProcResult p = avp_run(&(AvpProcSpec){.argv = av, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE, .arena = ml->a});
    if (p.status == AVP_PROC_START_FAILED) avp_throw(AVP_ERR_PROC, "%s", p.message ? p.message : "cannot run git");
    if (p.returncode != 0) return false;
    *bytes = p.out;
    return true;
}

MlWorktrees ml_worktrees(Ml *ml) {
    MlGit g = ML_GIT(ml, "worktree", "list", "--porcelain");
    MlWorktrees res = {0};
    AvpStrVec lines = avp_str_splitlines(g.out, false);
    bool cur = false;
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr ln = lines.v[i];
        if (avp_str_startswith(ln, AVP_S("worktree "))) {
            avp_vec_push(&res, ((MlWorktree){avp_cz(ml->a, avp_str_strip(avp_str_slice(ln, 9, AVP_END))), "", NULL}));
            cur = true;
        } else if (cur && avp_str_startswith(ln, AVP_S("HEAD "))) {
            res.v[res.n - 1].head = avp_cz(ml->a, avp_str_strip(avp_str_slice(ln, 5, AVP_END)));
        } else if (cur && avp_str_startswith(ln, AVP_S("branch "))) {
            res.v[res.n - 1].branch = avp_cz(ml->a, avp_str_strip(avp_str_slice(ln, 7, AVP_END)));
        }
    }
    return res;
}

AvpStrVec ml_status_lines(Ml *ml, const char *cwd, bool untracked) {
    MlGit g = untracked ? ML_GIT_IN(ml, cwd, "status", "--porcelain")
                        : ML_GIT_IN(ml, cwd, "status", "--porcelain", "--untracked-files=no");
    if (g.rc != 0) ml_fail(ml, ML_F(ml, "git status failed: %S", ml_sp(ml->a, avp_str_strip(g.err))), NULL);
    AvpStrVec lines = avp_str_splitlines(g.out, false), res = {0};
    for (size_t i = 0; i < lines.n; i++)
        if (avp_str_strip(lines.v[i]).n) avp_vec_push(&res, lines.v[i]);
    return res;
}

static bool is_unmerged_code(AvpStr c) {
    static const char *const U[] = {"DD", "AU", "UD", "UA", "DU", "AA", "UU"};
    for (int i = 0; i < 7; i++)
        if (avp_str_eqz(c, U[i])) return true;
    return false;
}

MlUnmergedVec ml_unmerged(Ml *ml) {
    MlUnmergedVec res = {0};
    AvpStrVec st = ml_status_lines(ml, NULL, true);
    for (size_t i = 0; i < st.n; i++) {
        AvpStr code = avp_u8_slice(st.v[i], 0, 2);
        if (!is_unmerged_code(code)) continue;
        AvpStr path = avp_str_strip_chars(avp_str_strip(avp_u8_slice(st.v[i], 3, AVP_END)), AVP_S("\""));
        avp_vec_push(&res, ((MlUnmerged){code, path}));
    }
    return res;
}

AvpStrVec ml_op_in_progress(Ml *ml, const char *cwd) {
    static const char *const NAMES[][2] = {{"rebase-merge", "rebase"}, {"rebase-apply", "rebase"},
                                           {"MERGE_HEAD", "merge"},    {"CHERRY_PICK_HEAD", "cherry-pick"},
                                           {"REVERT_HEAD", "revert"}};
    const char *where = cwd ? cwd : ml->slot;
    AvpStrVec found = {0};
    for (int i = 0; i < 5; i++) {
        MlGit g = ML_GIT_IN(ml, where, "rev-parse", "--git-path", NAMES[i][0]);
        if (g.rc != 0) continue;
        const char *p = avp_cz(ml->a, avp_str_strip(g.out));
        if (!avp_path_isabs(p)) p = avp_path_join(ml->a, where, p);
        bool have = false;
        for (size_t k = 0; k < found.n; k++) have |= avp_str_eqz(found.v[k], NAMES[i][1]);
        if (avp_exists(p) && !have) avp_vec_push(&found, avp_str(NAMES[i][1]));
    }
    return found;
}

const char *ml_onto_ref(Ml *ml) { return ml->onto_rev ? ml->onto_rev : ml->onto; }

const char *ml_log_path(Ml *ml, const char *name) { return avp_path_join(ml->a, ml->log_dir, name); }

/* ---- commands --------------------------------------------------------------------------------------------------- */
i64 ml_run_cmd(Ml *ml, const char *key, const char *log_name, const MlExtra *extra, const char *cwd, AvpStr *text) {
    static const MlExtra none = {0};
    if (!extra) extra = &none;
    avp_makedirs(ml->log_dir, true);
    const char *log = ml_log_path(ml, log_name);
    const char *where = cwd ? cwd : ml->slot;
    i64 rc;
    if (ml->hooks && ml->hooks->command && ml->hooks->command(ml->hooks->ctx, ml, key, extra, &rc, text)) {
        ml_write_text(log, *text);
        return rc;
    }
    return ml_default_cmd(ml, key, log, extra, where, text);
}

/* ---- driver ------------------------------------------------------------------------------------------------------- */
static const char *default_repo(AvpArena *a) {
    const char *d = avp_exe_dir();
    for (int i = 0; i < 6 && d && *d; i++) {
        if (avp_exists(avp_path_join(a, d, ".git")) && avp_isdir(avp_path_join(a, d, "tools")))
            return avp_path_abspath(a, d);
        AvpStr up = avp_path_dirname(d);
        if (up.n == strlen(d)) break;
        d = avp_cz(a, up);
    }
    return avp_path_abspath(a, avp_path_join(a, avp_exe_dir(), "../.."));
}

static bool in_list(const char *const *list, size_t n, AvpStr s) {
    for (size_t i = 0; i < n; i++)
        if (avp_str_eqz(s, list[i])) return true;
    return false;
}

static bool vec_has(const AvpStrVec *v, const char *s) {
    for (size_t i = 0; i < v->n; i++)
        if (avp_str_eqz(v->v[i], s)) return true;
    return false;
}

static int arg_error(AvpArgParser *p, const char *msg) {
    AvpStr t = avp_args_error_text(p, avp_str(msg));
    fwrite(t.p, 1, t.n, stderr);
    fflush(stderr);
    return 2;
}

typedef struct {
    const char *name, *status, *summary;
    double seconds;
} StepResult;

int ml_main(int argc, char **argv, const MlHooks *hooks) {
    AvpArena *a = avp_arena();
    AvpArgParser *p = avp_args_parser(a, &(AvpArgParserDesc){
        .prog = "merge_lane.exe",
        .description = "Run the mechanical part of the lane merge procedure in a lane's worktree.",
        .formatter = AVP_FORMAT_RAW_DESCRIPTION});
    avp_arg(p, &(AvpArgDesc){.names = "--slot", .required = true, .help = "the lane's worktree"});
    avp_arg(p, &(AvpArgDesc){.names = "--branch", .required = true, .help = "the lane branch"});
    avp_arg(p, &(AvpArgDesc){.names = "--onto", .dflt = AVP_DEF_STR("main"),
                             .help = "branch to rebase onto and merge into (default main)"});
    avp_arg(p, &(AvpArgDesc){.names = "--steps", .dflt = AVP_DEF_STR("rebase,gate,changes,rules,layout"),
                             .help = "comma list out of rebase,gate,changes,rules,layout"});
    avp_arg(p, &(AvpArgDesc){.names = "--update", .action = AVP_ARG_STORE_TRUE,
                             .help = "refresh baseline and ledger and commit them in the lane"});
    avp_arg(p, &(AvpArgDesc){.names = "--merge", .action = AVP_ARG_STORE_TRUE,
                             .help = "fast-forward --onto to the lane tip (needs --update, --reviewed)"});
    avp_arg(p, &(AvpArgDesc){.names = "--take-union-units", .action = AVP_ARG_STORE_TRUE,
                             .help = "a rebase conflict that is only lists of whole units in configure.py or splits.txt: keep both sides' distinct units, the lane's version of a unit both name (never by default; the hunks and this suggestion are printed anyway)"});
    avp_arg(p, &(AvpArgDesc){.names = "--reviewed", .action = AVP_ARG_STORE_TRUE,
                             .help = "the reader-test review was done"});
    avp_arg(p, &(AvpArgDesc){.names = "--log-dir", .dflt = AVP_DEF_NONE});
    avp_arg(p, &(AvpArgDesc){.names = "--repo", .dflt = AVP_DEF_STR(default_repo(a)),
                             .help = "this tool's repository (default: the checkout holding this script)"});
    AvpStrVec args = {0};
    for (int i = 1; i < argc; i++) avp_vec_push(&args, avp_str(argv[i]));
    AvpArgsExit ex = {0};
    AvpArgs *r = avp_args_try_parse(p, args.v, args.n, false, &ex);
    if (!r) {
        fwrite(ex.out.p, 1, ex.out.n, stdout);
        fflush(stdout);
        fwrite(ex.err.p, 1, ex.err.n, stderr);
        fflush(stderr);
        return ex.code;
    }
    AvpStrVec steps = {0};
    AvpStrVec parts = avp_str_split(avp_args_str(r, "steps"), AVP_S(","), -1);
    for (size_t i = 0; i < parts.n; i++)
        if (avp_str_strip(parts.v[i]).n) avp_vec_push(&steps, avp_str_strip(parts.v[i]));
    AvpStrVec bad = {0};
    for (size_t i = 0; i < steps.n; i++)
        if (!in_list(CORE_STEPS, NCORE, steps.v[i])) avp_vec_push(&bad, steps.v[i]);
    if (bad.n)
        return arg_error(p, avp_sprintf(a, "unknown step(s): %.*s (choose from rebase, gate, changes, rules, layout)",
                                        AVP_SF(avp_str_join(a, AVP_S(", "), bad.v, bad.n))));
    bool update = avp_args_bool(r, "update"), merge = avp_args_bool(r, "merge"), reviewed = avp_args_bool(r, "reviewed");
    if (update) {
        AvpStrVec missing = {0};
        for (size_t i = 0; i < 4; i++)
            if (!vec_has(&steps, UPDATE_NEEDS[i])) avp_vec_push(&missing, avp_str(UPDATE_NEEDS[i]));
        if (missing.n)
            return arg_error(p, avp_sprintf(a, "--update needs the steps %.*s (missing from --steps)",
                                            AVP_SF(avp_str_join(a, AVP_S(", "), missing.v, missing.n))));
    }
    if (merge && !update)
        return arg_error(p, "--merge needs --update (the merged tip must carry the refreshed baseline and ledger)");
    if (merge && !reviewed)
        return arg_error(p, "--merge needs --reviewed: the reader-test review (procedure step 5) is not done by this tool");

    Ml *ml = AVP_NEW(a, Ml);
    ml->a = a;
    ml->hooks = hooks;
    ml->slot = avp_path_abspath(a, avp_cz(a, avp_args_str(r, "slot")));
    ml->branch = avp_cz(a, avp_args_str(r, "branch"));
    ml->onto = avp_cz(a, avp_args_str(r, "onto"));
    ml->repo = avp_path_abspath(a, avp_cz(a, avp_args_str(r, "repo")));
    ml->update = update, ml->merge = merge, ml->reviewed = reviewed;
    ml->take_union = avp_args_bool(r, "take_union_units");
    ml->steps = steps;
    AvpStr ld = avp_args_str(r, "log_dir");
    if (!avp_args_is_none(r, "log_dir")) {
        ml->log_dir = avp_path_abspath(a, avp_cz(a, ld));
    } else {
        AvpStr name = avp_path_basename(avp_cz(a, avp_str_rstrip_chars(avp_str(ml->slot), AVP_S("/\\"))));
        ml->log_dir = avp_path_joinv(a, 3, (const char *const[]){avp_cz(a, avp_path_dirname(ml->slot)),
                                                                    "merge_lane_logs", avp_cz(a, name)});
    }
    ml->passed = avp_set_new();
    /* the run's whole output, kept beside the step logs: a caller's redirect into the slot (build/ is emptied by the gate)
     * is not the only copy */
    const char *merge_log = ml_log_path(ml, "merge.log");
    if (avp_makedirs(ml->log_dir, true)) ml->full_log = avp_fopen(merge_log, "wb", AVP_TM_UTF8);

    AvpStrVec wanted = {0};
    avp_vec_push(&wanted, AVP_S("preflight"));
    for (size_t i = 0; i < NCORE; i++)
        if (vec_has(&steps, CORE_STEPS[i])) avp_vec_push(&wanted, avp_str(CORE_STEPS[i]));
    if (update) avp_vec_push(&wanted, AVP_S("ledger"));
    if (merge) avp_vec_push(&wanted, AVP_S("merge"));

    StepResult results[NSTEPS];
    const char *failed = NULL;
    ml_out(ml, ML_F(ml, "merge_lane: slot %s, branch %s, onto %s", ml->slot, ml->branch, ml->onto));
    ml_out(ml, ML_F(ml, "logs: %s", ml->log_dir));
    if (ml->full_log) ml_out(ml, ML_F(ml, "full log: %s", merge_log));
    for (size_t s = 0; s < NSTEPS; s++) {
        StepResult *res = &results[s];
        *res = (StepResult){STEP_ORDER[s], "SKIP", "", 0.0};
        if (!vec_has(&wanted, STEP_ORDER[s])) {
            res->summary = "not requested";
            continue;
        }
        if (failed) {
            res->summary = ML_F(ml, "not run: stopped at %s", failed);
            continue;
        }
        ml_out(ml, "");
        ml_out(ml, ML_F(ml, "== %s ==", STEP_ORDER[s]));
        double t0 = avp_time();
        AvpStrVec ev = {0};
        AvpTry t;
        if (AVP_TRY(&t)) {
            if (hooks && hooks->before_step) hooks->before_step(hooks->ctx, ml, STEP_ORDER[s]);
            const char *summary = STEP_FUNCS[s](ml, &ev);
            avp_try_end(&t);
            res->status = "PASS", res->summary = summary;
            avp_set_add(ml->passed, avp_str(STEP_ORDER[s]));
            for (size_t i = 0; i < ev.n; i++) ml_out(ml, ML_F(ml, "   %S", &ev.v[i]));
        } else if (t.code == ML_STEP_FAIL) {
            res->status = "FAIL";
            AvpStr msg = avp_str(ml->fail_msg);
            AvpStrVec lines = avp_str_splitlines(msg, false);
            res->summary = ml->fail_summary ? ml->fail_summary : (msg.n && lines.n ? avp_cz(a, lines.v[0]) : "failed");
            for (size_t i = 0; i < lines.n; i++) ml_out(ml, ML_F(ml, "   %S", &lines.v[i]));
            failed = STEP_ORDER[s];
        } else {
            res->status = "FAIL";
            res->summary = ML_F(ml, "internal error: %s: %s", shsm_exc_class(t.code), t.msg);
            ml_out(ml, ML_F(ml, "   %s", res->summary));
            failed = STEP_ORDER[s];
        }
        res->seconds = avp_time() - t0;
        ml_out(ml, ML_F(ml, "   -> %s (%.1fs)", res->status, res->seconds));
    }
    ml_out(ml, "");
    ml_out(ml, "== summary ==");
    ml_out(ml, ML_F(ml, "%-10s %-5s %7s  %s", "step", "", "time", "evidence"));
    for (size_t s = 0; s < NSTEPS; s++) {
        StepResult *res = &results[s];
        AvpStrVec lines = avp_str_splitlines(avp_str(res->summary), false);
        AvpStr first = res->summary[0] && lines.n ? lines.v[0] : AVP_S("");
        AvpStr cut = avp_u8_slice(first, 0, 110);
        ml_out(ml, ML_F(ml, "%-10s %-5s %6.1fs  %S", res->name, res->status, res->seconds, &cut));
    }
    if (failed) {
        ml_out(ml, "");
        ml_out(ml, ML_F(ml, "STOPPED at %s: nothing after it was run.", failed));
        return 1;
    }
    ml_out(ml, "");
    if (merge) {
        ml_out(ml, "MERGED (not pushed). Reader-test review was attested with --reviewed.");
    } else {
        ml_out(ml, "All requested steps passed.");
        if (avp_set_has(ml->passed, AVP_S("changes")))
            ml_out(ml, ML_F(ml, "REMINDER: the reader-test review (procedure step 5) is not mechanical. Read the sample "
                                "in %s (new and improved functions, 100%% jumps first).",
                            ml_log_path(ml, "review-sample.txt")));
        if (!update)
            ml_out(ml, "Next: re-run with --update, then --update --merge --reviewed once the review is done.");
        else
            ml_out(ml, "Next: after the review, re-run with --update --merge --reviewed (steps are cheap to repeat: "
                       "the gate rebuilds from clean).");
    }
    return 0;
}
