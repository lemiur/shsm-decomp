/* gate_steps: the Gate of tools/gate.py: its steps, the driver (execute), the lock and the snapshot. */
#include "gate.h"
#include "doctor.h"
#include "shsm_re.h"
#include "shsm_repo.h"

#define GATE_EXC_JSON 200 /* json.JSONDecodeError (a ValueError whose class name the crash line shows) */

static const char *exc_class(int code) { return code == GATE_EXC_JSON ? "JSONDecodeError" : shsm_exc_class(code); }

/* ---- construction and plumbing --------------------------------------------------------------------------------- */
Gate *gate_new(AvpArena *a, const char *root, const GateOpts *opts, GateRunner runner, void *runner_ctx, AvpBuf *out) {
    Gate *g = AVP_NEW(a, Gate);
    g->a = a;
    g->root = avp_path_abspath(a, root);
    g->opts = *opts;
    g->runner = runner ? runner : gate_default_runner;
    g->runner_ctx = runner_ctx;
    g->out = out;
    g->version = shsm_read_version(g->root);
    g->gate_dir = avp_path_joinv(a, 3, (const char *const[]){g->root, "build", "gate"});
    g->tree = avp_path_join(a, g->gate_dir, "tree");
    g->env = avp_env_current(a);
    if (opts->no_objcache) avp_env_set(g->env, "SHSM_OBJCACHE", "0");
    g->pending = (opts->pending_flip && *opts->pending_flip) ? opts->pending_flip : NULL;
    for (size_t i = 0; i < GATE_NSTEPS; i++) gate_step_init(a, &g->steps[i], gate_step_names[i]);
    g->dol_sha1 = "";
    g->exe_dir = avp_exe_dir();
    GateStepFn fns[GATE_NSTEPS] = {gate_step_preflight, gate_step_build,  gate_step_checksum, gate_step_coverage,
                                   gate_step_regressions, gate_step_layout, gate_step_rules, gate_step_ledger};
    memcpy(g->step_fn, fns, sizeof fns);
    g->check_tools = gate_check_tools_doctor;
    g->load_units = gate_load_units_real;
    g->indexer = (GateIndexer){gate_index_linked, NULL};
    g->rules_tools = (GateRulesTools){gate_classes_scan_linked, gate_check_rules_linked, NULL};
    return g;
}

static void emit(AvpBuf *out, const char *text) {
    if (out) {
        avp_buf_putz(out, text);
        avp_buf_putc(out, '\n');
    } else {
        fputs(text, stdout);
        fputc('\n', stdout);
        fflush(stdout);
    }
}

void gate_say(Gate *g, const char *text) {
    if (!g->opts.json) emit(g->out, text);
}

GateRun gate_run(Gate *g, const char *const *argv, size_t argc, const char *cwd, double timeout) {
    return g->runner(g->runner_ctx, g->a, argv, argc, cwd ? cwd : g->tree, g->env, timeout);
}

GateRun gate_git(Gate *g, const char *const *args, size_t n) {
    const char **av = AVP_NEWN(g->a, const char *, n + 3);
    av[0] = "git";
    av[1] = "-C";
    av[2] = g->root;
    for (size_t i = 0; i < n; i++) av[3 + i] = args[i];
    return g->runner(g->runner_ctx, g->a, av, n + 3, g->root, g->env, 0);
}

static GateRun git_fn(void *ctx, AvpArena *a, const char *const *args, size_t n) {
    (void)a;
    return gate_git(ctx, args, n);
}

static const char *vpath(Gate *g, const char *base, const char *name) {
    return avp_path_joinv(g->a, 4, (const char *const[]){base ? base : g->tree, "build", g->version, name});
}

static const char *rel_v(Gate *g, const char *name) { return avp_sprintf(g->a, "build/%s/%s", g->version, name); }

/* <exe_dir>/<name>.exe + args (a sub-tool beside the gate) */
static const char **exe_argv(Gate *g, const char *name, size_t *n, const char *const *args, size_t nargs) {
    const char **av = AVP_NEWN(g->a, const char *, nargs + 2);
    av[0] = avp_path_join(g->a, g->exe_dir, avp_sprintf(g->a, "%s.exe", name));
    for (size_t i = 0; i < nargs; i++) av[1 + i] = args[i];
    *n = nargs + 1;
    return av;
}

static void fail_tail(Gate *g, GateStep *st, const char *reason, const GateRun *r, size_t n) {
    gate_step_fail(st, reason);
    AvpStrVec t = gate_tail(g->a, gate_run_output(g->a, r), n);
    gate_step_lines_indented(g->a, st, &t);
}

static const char *tail_repr(Gate *g, const GateRun *r, size_t n) {
    AvpStrVec t = gate_tail(g->a, gate_run_output(g->a, r), n);
    return gate_repr_str_list(g->a, t.v, t.n);
}

static AvpJson *rc_json(Gate *g, const GateRun *r) {
    return r->timed_out ? avp_json_null() : avp_json_int(g->a, r->returncode);
}

static AvpJson *str_list_json(AvpArena *a, const AvpStrVec *v, size_t cap) {
    AvpJson *arr = avp_json_arr(a);
    for (size_t i = 0; i < v->n && i < cap; i++) avp_json_arr_push(a, arr, avp_json_str(a, v->v[i]));
    return arr;
}

/* read_contract(...)[i] raising what the Python raised */
static GateContract contract_or_raise(Gate *g, const char *base) {
    const char *cfg = avp_path_joinv(g->a, 3, (const char *const[]){base, "config", g->version});
    GateContract c;
    const char *err = NULL;
    avp_io_clear();
    if (!gate_read_contract(g->a, avp_path_join(g->a, cfg, "build.sha1"), avp_path_join(g->a, cfg, "config.yml"),
                            base, &c, &err)) {
        if (avp_io_err()->kind != 0) shsm_raise_io();
        shsm_raise(SHSM_EXC_VALUE, "%s", err);
    }
    return c;
}

/* ---- preflight ------------------------------------------------------------------------------------------------- */
static int cmp_cz(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return strcmp(*(const char *const *)x, *(const char *const *)y);
}

const char *gate_check_pending(Gate *g, const GatePorcelainVec *entries) {
    AvpArena *a = g->a;
    if (!(g->dirty.n == 1 && avp_str_eqz(g->dirty.v[0], "configure.py"))) {
        AvpStrVec other = {0};
        for (size_t i = 0; i < g->dirty.n; i++)
            if (!avp_str_eqz(g->dirty.v[i], "configure.py")) avp_vec_push(&other, g->dirty.v[i]);
        if (other.n) {
            AvpStr j = avp_str_join(a, AVP_S(", "), other.v, other.n < 5 ? other.n : 5);
            return avp_sprintf(a, "the tree has changes other than configure.py: %.*s", AVP_SF(j));
        }
        return "configure.py is unchanged from HEAD";
    }
    for (size_t i = 0; i < entries->n; i++)
        if (avp_str_eqz(entries->v[i].code, "??") && avp_str_eqz(entries->v[i].path, "configure.py"))
            return "configure.py is untracked";
    GateRun h = GATE_GIT(g, "show", "HEAD:configure.py");
    if (!gate_run_ok(&h)) return avp_sprintf(a, "git show HEAD:configure.py failed: %s", tail_repr(g, &h, 1));
    AvpStr work;
    if (!avp_read_text(a, avp_path_join(a, g->root, "configure.py"), AVP_TM_UTF8, &work))
        return avp_sprintf(a, "cannot read configure.py: %.*s", AVP_SF(avp_io_err_str(a)));
    return gate_check_pending_flip(a, h.out, work, g->pending);
}

static DoctorRunResult doctor_adapter(void *ctx, const char *const *argv, size_t argc, const char *cwd) {
    Gate *g = ctx;
    GateRun r = g->runner(g->runner_ctx, g->a, argv, argc, cwd ? cwd : g->root, g->env, 60);
    return (DoctorRunResult){r.timed_out ? -1 : r.returncode, avp_cz(g->a, avp_str_strip(r.out))};
}

void gate_check_tools_doctor(Gate *g, AvpStrVec *problems) {
    AvpArena *a = g->a;
    Doctor d = doctor_new(g->root);
    d.runner = doctor_adapter;
    d.runner_ctx = g;
    DoctorChecks cs = {0};
    doctor_check_required(&d, &cs);
    for (size_t i = 0; i < cs.n; i++)
        if (strcmp(cs.v[i].status, "OK") != 0)
            avp_vec_push(problems, avp_str(avp_sprintf(a, "%s %s: %s", cs.v[i].status, cs.v[i].name, cs.v[i].detail)));
}

void gate_step_preflight(Gate *g, GateStep *st) {
    AvpArena *a = g->a;
    GateRun r = GATE_GIT(g, "status", "--porcelain=v1", "-z", "--untracked-files=all");
    if (!gate_run_ok(&r)) {
        gate_step_fail(st, avp_sprintf(a, "git status failed: %s", tail_repr(g, &r, 1)));
        return;
    }
    GatePorcelainVec entries = gate_parse_porcelain_z(r.out);
    AvpSet *seen = avp_set_new();
    for (size_t i = 0; i < entries.n; i++) {
        const char *p = avp_cz(a, entries.v[i].path);
        if (!gate_is_allowed_dirty(a, p)) avp_set_add(seen, entries.v[i].path);
    }
    g->dirty = avp_set_sorted(seen);
    avp_json_set(a, st->data, AVP_S("dirty"), str_list_json(a, &g->dirty, (size_t)-1));
    AvpStrVec shown = {0};
    for (size_t i = 0; i < g->dirty.n && i < 20; i++)
        avp_vec_push(&shown, avp_str(avp_sprintf(a, "    %.*s", AVP_SF(g->dirty.v[i]))));
    if (g->pending) {
        const char *why = gate_check_pending(g, &entries);
        if (*why) {
            gate_step_fail(st, avp_sprintf(a, "--pending-flip %s: %s", g->pending, why));
            for (size_t i = 0; i < shown.n; i++) avp_vec_push(&st->lines, shown.v[i]);
            return;
        }
        avp_json_set(a, st->data, AVP_S("pending_flip"), avp_json_strz(a, g->pending));
        gate_step_line(a, st, avp_sprintf(a, "pending flip: the only change from HEAD is %s NonMatching -> Matching",
                                          g->pending));
    } else if (g->dirty.n && !g->opts.allow_dirty) {
        AvpStr j = avp_str_join(a, AVP_S(", "), g->dirty.v, g->dirty.n < 3 ? g->dirty.n : 3);
        gate_step_fail(st, avp_sprintf(a, "dirty tree: %zu path(s), e.g. %.*s (commit first, or --allow-dirty)",
                                       g->dirty.n, AVP_SF(j)));
        for (size_t i = 0; i < shown.n; i++) avp_vec_push(&st->lines, shown.v[i]);
        return;
    }
    const char *cfg = avp_path_joinv(a, 3, (const char *const[]){g->root, "config", g->version});
    const char *why = NULL, *want = NULL;
    if (!gate_verify_contract(a, avp_path_join(a, cfg, "build.sha1"), avp_path_join(a, cfg, "config.yml"), g->root,
                              NULL, &why, &want)) {
        gate_step_fail(st, avp_sprintf(a, "hash contract: %s", why));
        return;
    }
    gate_step_line(a, st, why);
    AvpStrVec miss = {0};
    g->check_tools(g, &miss);
    if (miss.n) {
        AvpStr j = avp_str_join(a, AVP_S("; "), miss.v, miss.n);
        gate_step_fail(st, avp_sprintf(a, "pinned tools: %.*s", AVP_SF(j)));
        return;
    }
    gate_step_line(a, st, "doctor check_required: pinned tools, compilers, retail DOL and contract OK");
    if (g->pending) {
        gate_step_ok(st, avp_sprintf(a, "pending flip of %s verified, contract and tools verified", g->pending));
        return;
    }
    if (g->dirty.n) gate_step_line(a, st, avp_sprintf(a, "DIRTY (--allow-dirty): %zu path(s)", g->dirty.n));
    gate_step_ok(st, g->dirty.n ? "contract and tools verified" : "clean tree, contract and tools verified");
}

/* ---- build ----------------------------------------------------------------------------------------------------- */
static AvpStrVec tracked_files(Gate *g) {
    GateRun r = GATE_GIT(g, "ls-files", "-z", "--cached", "--others", "--exclude-standard");
    if (!gate_run_ok(&r)) {
        AvpStr o = avp_u8_slice(avp_str_strip(gate_run_output(g->a, &r)), 0, 200);
        shsm_raise(SHSM_EXC_USAGE, "git ls-files failed: %.*s", AVP_SF(o));
    }
    AvpSet *s = avp_set_new();
    AvpStrVec parts = avp_str_split(r.out, AVP_S("\0"), -1);
    for (size_t i = 0; i < parts.n; i++)
        if (parts.v[i].n) avp_set_add(s, parts.v[i]);
    avp_vec_free(&parts);
    return avp_set_sorted(s);
}

i64 gate_materialise(Gate *g) {
    AvpArena *a = g->a;
    const char *old = avp_sprintf(a, "%s.old", g->tree);
    gate_remove_tree(a, old);
    if (avp_lexists(g->tree)) {
        if (gate_has_reparse_points(a, g->tree)) gate_remove_tree(a, g->tree); /* refuses with the link named */
        if (!avp_replace(g->tree, old)) shsm_raise_io();
    }
    if (!avp_makedirs(g->tree, false)) shsm_raise_io();
    i64 n = 0;
    AvpStrVec files = tracked_files(g);
    for (size_t i = 0; i < files.n; i++) {
        const char *rel = avp_cz(a, files.v[i]);
        const char *src = avp_path_join(a, g->root, rel);
        if (!avp_isfile(src)) continue; /* deleted in the working tree (already reported as dirty) */
        const char *dst = avp_path_join(a, g->tree, rel);
        if (!avp_makedirs(avp_cz(a, avp_path_dirname(dst)), true)) shsm_raise_io();
        if (!avp_copyfile(src, dst)) shsm_raise_io();
        n++;
    }
    static const char *const exes[] = {"dtk.exe", "objdiff-cli.exe", "ninja.exe"};
    for (size_t i = 0; i < 3; i++)
        gate_link_or_copy(a, avp_path_joinv(a, 4, (const char *const[]){g->root, "tools", "bin", exes[i]}),
                          avp_path_joinv(a, 4, (const char *const[]){g->tree, "tools", "bin", exes[i]}));
    GateContract c = contract_or_raise(g, g->root);
    char *rel = NULL;
    if (!avp_path_relpath(a, c.retail, g->root, &rel)) shsm_raise_io();
    gate_link_or_copy(a, c.retail, avp_path_join(a, g->tree, rel));
    const char *sjis = avp_path_joinv(a, 4, (const char *const[]){g->root, "build", "tools", "sjiswrap.exe"});
    if (avp_isfile(sjis))
        gate_link_or_copy(a, sjis, avp_path_joinv(a, 4, (const char *const[]){g->tree, "build", "tools", "sjiswrap.exe"}));
    return n;
}

void gate_step_build(Gate *g, GateStep *st) {
    AvpArena *a = g->a;
    double t = avp_time();
    i64 n = gate_materialise(g);
    double tree_s = avp_py_round(avp_time() - t, 2);
    avp_json_set(a, st->data, AVP_S("tree_seconds"), avp_json_float(a, tree_s));
    gate_step_line(a, st, avp_cz(a, avp_pyfmt(a, "tree: %d files copied into %s (%.1fs)", n, g->tree, tree_s)));
    const char *compilers = avp_path_joinv(a, 3, (const char *const[]){g->root, "tools", "compilers"});
    GateRun r = gate_run(g, GATE_ARGV_(gate_configure_exe(a, &g->opts, g->exe_dir), "--compilers", compilers), NULL, 0);
    double conf_s = avp_py_round(r.seconds, 2);
    avp_json_set(a, st->data, AVP_S("configure_seconds"), avp_json_float(a, conf_s));
    if (!gate_run_ok(&r)) {
        fail_tail(g, st, avp_sprintf(a, "configure exited %s", gate_rc_str(a, &r)), &r, 12);
        return;
    }
    const char *dol = vpath(g, NULL, "main.dol");
    if (avp_exists(dol)) avp_remove(dol);
    if (avp_exists(dol)) {
        gate_step_fail(st, avp_sprintf(a, "cannot delete %s before the link", dol));
        return;
    }
    const char *av[6];
    size_t n_av = 0;
    av[n_av++] = avp_path_joinv(a, 4, (const char *const[]){g->tree, "tools", "bin", "ninja.exe"});
    if (g->opts.jobs) {
        av[n_av++] = "-j";
        av[n_av++] = avp_sprintf(a, "%lld", (long long)g->opts.jobs);
    }
    av[n_av++] = rel_v(g, "main.dol");
    av[n_av++] = rel_v(g, "report.json");
    r = gate_run(g, av, n_av, NULL, 0);
    if (!avp_makedirs(g->gate_dir, true) ||
        !avp_write_text(avp_path_join(a, g->gate_dir, "ninja.log"), gate_run_output(a, &r), AVP_TM_UTF8))
        shsm_raise_io();
    double ninja_s = avp_py_round(r.seconds, 2);
    avp_json_set(a, st->data, AVP_S("ninja_seconds"), avp_json_float(a, ninja_s));
    if (!gate_run_ok(&r)) {
        fail_tail(g, st, avp_sprintf(a, "ninja exited %s (log build/gate/ninja.log)", gate_rc_str(a, &r)), &r, 12);
        return;
    }
    if (!avp_isfile(dol)) {
        gate_step_fail(st, avp_sprintf(a, "ninja exited 0 but %s is missing", dol));
        return;
    }
    gate_step_ok(st, avp_cz(a, avp_pyfmt(a, "configure and ninja exited 0 (configure %.1fs, ninja %.1fs)", conf_s,
                                         ninja_s)));
}

/* ---- checksum -------------------------------------------------------------------------------------------------- */
static bool has_ok_line(AvpArena *a, AvpStr output, const char *name) {
    AvpStrVec ls = avp_str_splitlines(gate_plain(a, output), false);
    const char *want = avp_sprintf(a, "%s: OK", name);
    bool hit = false;
    for (size_t i = 0; i < ls.n && !hit; i++) hit = avp_str_eqz(avp_str_strip(ls.v[i]), want);
    avp_vec_free(&ls);
    return hit;
}

void gate_step_checksum(Gate *g, GateStep *st) {
    AvpArena *a = g->a;
    const char *sha_rel = avp_sprintf(a, "config/%s/build.sha1", g->version);
    GateRun r = gate_run(g, GATE_ARGV_(avp_path_joinv(a, 4, (const char *const[]){g->tree, "tools", "bin", "dtk.exe"}),
                                       "shasum", "-c", sha_rel),
                         NULL, 0);
    const char *dol_rel = rel_v(g, "main.dol");
    bool dtk_ok = gate_run_ok(&r) && has_ok_line(a, gate_run_output(a, &r), dol_rel);
    const char *want = contract_or_raise(g, g->tree).sha1;
    g->dol_sha1 = gate_sha1_file(a, vpath(g, NULL, "main.dol"));
    bool own_ok = strcmp(g->dol_sha1, want) == 0;
    avp_json_set(a, st->data, AVP_S("dtk_exit"), rc_json(g, &r));
    avp_json_set(a, st->data, AVP_S("sha1"), avp_json_strz(a, g->dol_sha1));
    avp_json_set(a, st->data, AVP_S("contract"), avp_json_strz(a, want));
    if (dtk_ok != own_ok) {
        gate_step_fail(st, avp_sprintf(a, "dtk shasum (exit %s) and hashlib (%.12s) disagree: a tool bug",
                                       gate_rc_str(a, &r), g->dol_sha1));
        return;
    }
    if (dtk_ok) {
        gate_step_ok(st, avp_sprintf(a, "%s: OK (sha1 %s)", dol_rel, g->dol_sha1));
        return;
    }
    fail_tail(g, st, avp_sprintf(a, "%s sha1 %.12s, contract %.12s", dol_rel, g->dol_sha1, want), &r, 4);
    size_t n;
    const char **av = exe_argv(g, "gatecheck", &n, GATE_ARGV_("--root", g->tree, "attribute"));
    GateRun at = gate_run(g, av, n, NULL, 0);
    gate_step_line(a, st, avp_sprintf(a, "gatecheck attribute (exit %s):", gate_rc_str(a, &at)));
    AvpStrVec ls = avp_str_splitlines(at.out, false);
    AvpStrVec names = {0};
    for (size_t i = 0; i < ls.n; i++) {
        if (i < 60) gate_step_line(a, st, avp_sprintf(a, "    %.*s", AVP_SF(ls.v[i])));
        if (names.n < 3 && (avp_str_startswith(ls.v[i], AVP_S("==")) || avp_str_startswith(ls.v[i], AVP_S("  "))))
            avp_vec_push(&names, avp_str_strip(ls.v[i]));
    }
    if (names.n) {
        AvpStr j = avp_str_join(a, AVP_S(" | "), names.v, names.n);
        st->reason = avp_sprintf(a, "%s; first: %.*s", st->reason, AVP_SF(j));
    }
}

/* ---- coverage -------------------------------------------------------------------------------------------------- */
void gate_step_coverage(Gate *g, GateStep *st) {
    AvpArena *a = g->a;
    ShsmReport *rep = shsm_read_report(vpath(g, NULL, "report.json"));
    g->report = rep;
    GateUnits units = {0};
    g->load_units(g->load_units_ctx, a, g->tree, g->version, &units);
    AvpStrVec bad = {0};
    i64 nfun = 0, nmatch = 0;
    for (size_t i = 0; i < units.n; i++) {
        const GateUnit *u = &units.v[i];
        if (!u->status || strcmp(u->status, "Matching") != 0) continue;
        nmatch++;
    }
    if (!nmatch) {
        gate_step_fail(st, "no Matching unit in configure.py: nothing is linked from source");
        return;
    }
    for (size_t i = 0; i < units.n; i++) {
        const GateUnit *u = &units.v[i];
        if (!u->status || strcmp(u->status, "Matching") != 0) continue;
        const ShsmUnitReport *ur = shsm_report_unit(rep, u->name);
        if (!ur) {
            avp_vec_push(&bad, avp_str(avp_sprintf(a, "%s: not in the report", u->name)));
            continue;
        }
        if (!ur->complete)
            avp_vec_push(&bad, avp_str(avp_sprintf(a, "%s: Matching but not linked from source in the report", u->name)));
        AvpStrVec low = {0};
        AvpMapIter it = {0};
        size_t nf = 0;
        while (avp_map_next(ur->first_by_name, &it)) {
            nf++;
            const ShsmFunctionReport *f = &ur->functions[it.val->i];
            if (f->fuzzy < 100.0 - GATE_EPS) avp_vec_push(&low, avp_str(f->name));
        }
        avp_sort_strs(low.v, low.n);
        if (low.n || ur->fuzzy < 100.0 - GATE_EPS) {
            AvpStr j = avp_str_join(a, AVP_S(", "), low.v, low.n < 4 ? low.n : 4);
            avp_vec_push(&bad, avp_pyfmt(a, "%s: %.2f%%, %d function(s) below 100%%: %S", u->name, ur->fuzzy, low.n, &j));
        }
        nfun += (i64)nf;
    }
    AvpJson *objs = avp_json_obj(a);
    for (size_t i = 0; i < units.n; i++) {
        const GateUnit *u = &units.v[i];
        if (!u->base_obj || !*u->base_obj) continue;
        const char *p = avp_path_join(a, g->tree, u->base_obj);
        if (avp_isfile(p)) avp_json_set(a, objs, avp_str(u->name), avp_json_strz(a, gate_sha1_file(a, p)));
    }
    g->units_linked = nmatch;
    g->functions_linked = nfun;
    avp_json_set(a, st->data, AVP_S("units"), avp_json_int(a, nmatch));
    avp_json_set(a, st->data, AVP_S("functions"), avp_json_int(a, nfun));
    avp_json_set(a, st->data, AVP_S("objects"), objs);
    if (bad.n) {
        gate_step_fail(st, avp_sprintf(a, "%zu Matching unit(s) not at 100%%: %.*s", bad.n, AVP_SF(bad.v[0])));
        for (size_t i = 0; i < bad.n && i < 40; i++) gate_step_line(a, st, avp_sprintf(a, "    %.*s", AVP_SF(bad.v[i])));
        return;
    }
    if (nfun == 0) {
        gate_step_fail(st, "Matching units hold no function: nothing verified");
        return;
    }
    gate_step_ok(st, avp_sprintf(a, "%lld Matching units linked from source at 100%%, %lld functions", (long long)nmatch,
                                 (long long)nfun));
}

/* ---- regressions ----------------------------------------------------------------------------------------------- */
static AvpJson *read_json_file(AvpArena *a, const char *path) {
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TM_UTF8, &text)) shsm_raise_io();
    AvpJsonError e;
    AvpJson *doc = avp_json_parse(a, text, &e);
    if (!doc) {
        AvpStr m = avp_json_error_str(a, &e);
        avp_throw(GATE_EXC_JSON, "%.*s", AVP_SF(m));
    }
    return doc;
}

static AvpJson *fall_json(AvpArena *a, const GateFall *f) {
    AvpJson *arr = avp_json_arr(a);
    char b1[32], b2[32];
    avp_float_repr(f->old, b1);
    avp_float_repr(f->new_, b2);
    avp_json_arr_push(a, arr, avp_json_strz(a, f->unit));
    avp_json_arr_push(a, arr, avp_json_strz(a, f->name));
    avp_json_arr_push(a, arr, avp_json_strz(a, b1));
    avp_json_arr_push(a, arr, avp_json_strz(a, f->has_new ? b2 : "None"));
    avp_json_arr_push(a, arr, avp_json_strz(a, f->label ? f->label : ""));
    return arr;
}

static AvpStrVec key_diff(AvpArena *a, const GateFalls *x, const GateFalls *y) {
    AvpStrVec out = {0};
    for (size_t i = 0; i < x->n; i++)
        if (!gate_falls_get(y, x->v[i].key)) avp_vec_push(&out, avp_str(x->v[i].key));
    avp_sort_strs(out.v, out.n);
    if (out.n > 5) out.n = 5;
    (void)a;
    return out;
}

const char *gate_baseline_rev(Gate *g) {
    GateRun r = GATE_GIT(g, "log", "-1", "--format=%H", "--", avp_sprintf(g->a, "config/%s/baseline_report.json", g->version));
    return gate_run_ok(&r) ? avp_cz(g->a, avp_str_strip(r.out)) : "";
}

void gate_step_regressions(Gate *g, GateStep *st) {
    AvpArena *a = g->a;
    const char *base_path = avp_path_joinv(a, 4, (const char *const[]){g->tree, "config", g->version, "baseline_report.json"});
    ShsmReport *base = shsm_read_report(base_path);
    ShsmReport *fresh = g->report ? g->report : shsm_read_report(vpath(g, NULL, "report.json"));
    i64 nbase = 0;
    for (size_t i = 0; i < base->nunits; i++) nbase += (i64)avp_map_len(base->units[i]->first_by_name);
    if (nbase == 0) {
        gate_step_fail(st, "the baseline has no function: nothing to compare");
        return;
    }
    GateFalls ours = {0};
    gate_fallen_functions(a, base, fresh, NULL, &ours);
    if (!avp_copyfile(base_path, vpath(g, NULL, "baseline.json"))) shsm_raise_io();
    GateRun r = gate_run(g,
                         GATE_ARGV_(avp_path_joinv(a, 4, (const char *const[]){g->tree, "tools", "bin", "ninja.exe"}),
                                    rel_v(g, "report_changes.json")),
                         NULL, 0);
    if (!gate_run_ok(&r)) {
        fail_tail(g, st, avp_sprintf(a, "ninja report_changes exited %s", gate_rc_str(a, &r)), &r, 12);
        return;
    }
    AvpJson *cj = read_json_file(a, vpath(g, NULL, "report_changes.json"));
    GateUnitIndex any = gate_all_unit_index(a, read_json_file(a, vpath(g, NULL, "report.json")));
    GateScores sc = gate_func_scores(a, cj);
    GateScores merged = gate_merge_by_symbol(a, &sc);
    GateFalls theirs = {0};
    for (size_t i = 0; i < merged.n; i++) {
        const GateScore *e = &merged.v[i];
        if (!e->has_from) continue;
        bool has_to = e->has_to;
        double to = e->to;
        const GateHit *hit = e->has_addr_int ? gate_unit_index_addr(&any, e->addr_int)
                                             : gate_unit_index_name(a, &any, e->unit, e->onto_name);
        if (hit && (!has_to || hit->score > to)) {
            to = hit->score;
            has_to = true;
        }
        if ((!has_to && e->from > GATE_EPS) || (has_to && to < e->from - GATE_EPS)) {
            GateFall f = {.has_addr = e->has_addr_int, .addr = e->addr_int, .unit = e->unit, .name = e->onto_name,
                          .old = e->from, .has_new = has_to, .new_ = to};
            f.key = e->has_addr_int ? gate_addr_key(a, e->addr_int) : gate_tuple_key(a, e->unit, e->onto_name);
            gate_falls_put(&theirs, f);
        }
    }
    AvpStrVec only_o = key_diff(a, &ours, &theirs), only_t = key_diff(a, &theirs, &ours);
    bool differ = ours.n != theirs.n || only_o.n || only_t.n;
    if (differ) {
        gate_step_fail(st, avp_sprintf(a, "compare_reports (%zu) and ninja changes (%zu) disagree: a tool bug", ours.n,
                                       theirs.n));
        gate_step_line(a, st, avp_sprintf(a, "    only compare_reports: %s", gate_repr_str_list(a, only_o.v, only_o.n)));
        gate_step_line(a, st, avp_sprintf(a, "    only ninja changes: %s", gate_repr_str_list(a, only_t.v, only_t.n)));
        return;
    }
    avp_json_set(a, st->data, AVP_S("fallen"), avp_json_int(a, (i64)ours.n));
    if (!ours.n) {
        gate_step_ok(st, avp_sprintf(a,
                                     "no function below the baseline (%lld baseline functions; compare_reports and "
                                     "ninja changes agree)",
                                     (long long)nbase));
        return;
    }
    const char *brev = gate_baseline_rev(g);
    /* a function the lane's resplit moved out of its unit's range is linked from retail asm again: not a regression */
    GateFalls remaining = {0};
    AvpStrVec left_lines = {0};
    {
        AvpStr lane_splits, lane_symbols, base_splits;
        const char *cfg = avp_sprintf(a, "config/%s/", g->version);
        bool ok = avp_read_text(a, avp_path_join(a, g->tree, avp_sprintf(a, "%ssplits.txt", cfg)), AVP_TM_UTF8_REPLACE,
                                &lane_splits) &&
                  avp_read_text(a, avp_path_join(a, g->tree, avp_sprintf(a, "%ssymbols.txt", cfg)), AVP_TM_UTF8_REPLACE,
                                &lane_symbols);
        GateResplit *rs = NULL;
        if (ok && brev && *brev) {
            GateRun sr = GATE_GIT(g, "show", avp_sprintf(a, "%s:%ssplits.txt", brev, cfg));
            if (gate_run_ok(&sr)) {
                base_splits = sr.out;
                rs = gate_resplit_new(a, &lane_splits, &base_splits, &lane_symbols);
            }
        }
        for (size_t i = 0; i < ours.n; i++) {
            const GateFall *f = &ours.v[i];
            GateLeftUnit l;
            if (gate_resplit_left(a, rs, f->unit, f->name, f->has_addr, f->addr, f->old, &l))
                avp_vec_push(&left_lines,
                             avp_str(gate_left_unit_line(a, f->name, f->old, f->has_new, f->new_, f->unit, &l)));
            else
                gate_falls_put(&remaining, *f);
        }
    }
    avp_json_set(a, st->data, AVP_S("left_unit"), avp_json_int(a, (i64)left_lines.n));
    if (left_lines.n) {
        gate_step_line(a, st, GATE_LEFT_UNIT_HEADER);
        for (size_t i = 0; i < left_lines.n && i < 40; i++) gate_step_line(a, st, avp_cz(a, left_lines.v[i]));
    }
    if (!remaining.n) {
        gate_step_ok(st, avp_sprintf(a, "no edited function below the baseline (%zu left their unit)", left_lines.n));
        return;
    }
    ours = remaining;
    GateFalls edited = {0}, unchanged = {0}, placed = {0};
    gate_classify_falls(a, g->tree, git_fn, g, brev, g->report, &ours, &g->indexer, &edited, &unchanged, &placed);
    AvpJson *ej = avp_json_arr(a), *uj = avp_json_arr(a);
    for (size_t i = 0; i < edited.n; i++) avp_json_arr_push(a, ej, fall_json(a, &edited.v[i]));
    for (size_t i = 0; i < unchanged.n; i++) avp_json_arr_push(a, uj, fall_json(a, &unchanged.v[i]));
    avp_json_set(a, st->data, AVP_S("edited"), ej);
    avp_json_set(a, st->data, AVP_S("unchanged"), uj);
    avp_json_set(a, st->data, AVP_S("placeholder_removed"), avp_json_int(a, (i64)placed.n));
    if (placed.n) {
        gate_step_line(a, st, GATE_PLACEHOLDER_HEADER);
        for (size_t i = 0; i < placed.n && i < 40; i++) gate_step_line(a, st, gate_format_fall(a, &placed.v[i]));
    }
    if (unchanged.n) {
        gate_step_line(a, st, avp_sprintf(a, "informational: %zu fall(s) in unchanged functions", unchanged.n));
        for (size_t i = 0; i < unchanged.n && i < 20; i++) gate_step_line(a, st, gate_format_fall(a, &unchanged.v[i]));
    }
    if (edited.n) {
        gate_step_line(a, st, "regressed (source changed since the baseline):");
        for (size_t i = 0; i < edited.n && i < 40; i++) gate_step_line(a, st, gate_format_fall(a, &edited.v[i]));
        gate_step_fail(st, avp_sprintf(a, "%zu edited function(s) below the baseline, e.g. %s", edited.n, edited.v[0].name));
        return;
    }
    gate_step_ok(st, avp_sprintf(a, "no edited function below the baseline (%zu informational fall(s), %zu left their unit, %zu placeholder(s) removed)",
                                 unchanged.n, left_lines.n, placed.n));
}

/* ---- layout ---------------------------------------------------------------------------------------------------- */
void gate_step_layout(Gate *g, GateStep *st) {
    AvpArena *a = g->a;
    size_t n_av;
    const char **av = exe_argv(g, "gatecheck", &n_av,
                               GATE_ARGV_("--root", g->tree, "layout", "--all", "--status", "Matching"));
    GateRun r = gate_run(g, av, n_av, NULL, 0);
    ShsmReMatch m;
    i64 n = 0;
    if (shsm_re_search(shsm_re("^units checked: (\\d+)", SHSM_RE_M), r.out, 0, AVP_END, &m)) {
        bool overflow = false;
        avp_parse_int(shsm_re_group(&m, 1), 10, &n, &overflow);
    }
    avp_json_set(a, st->data, AVP_S("units"), avp_json_int(a, n));
    bool rc0 = !r.timed_out && r.returncode == 0;
    if (rc0 && n > 0) {
        gate_step_ok(st, avp_sprintf(a, "static data layout matches in %lld Matching unit(s)", (long long)n));
        return;
    }
    AvpStrVec ls = avp_str_splitlines(r.out, false), diffs = {0};
    for (size_t i = 0; i < ls.n; i++)
        if (avp_str_startswith(ls.v[i], AVP_S("DIFF")) || avp_str_startswith(ls.v[i], AVP_S("ERR")))
            avp_vec_push(&diffs, ls.v[i]);
    if (rc0) {
        gate_step_fail(st, "gatecheck layout checked no unit");
        return;
    }
    AvpStrVec t = gate_tail(a, gate_run_output(a, &r), 12);
    AvpStr first;
    if (diffs.n) {
        first = avp_str_strip(diffs.v[0]);
    } else {
        AvpStrVec t1 = gate_tail(a, gate_run_output(a, &r), 1);
        first = t1.n ? t1.v[0] : AVP_S("no output");
    }
    gate_step_fail(st, avp_sprintf(a, "gatecheck layout exit %s: %.*s", gate_rc_str(a, &r), AVP_SF(first)));
    if (diffs.n) {
        if (diffs.n > 30) diffs.n = 30;
        gate_step_lines_indented(a, st, &diffs);
    } else {
        gate_step_lines_indented(a, st, &t);
    }
}

/* ---- rules ----------------------------------------------------------------------------------------------------- */
void gate_step_rules(Gate *g, GateStep *st) {
    AvpArena *a = g->a;
    GateRun c = g->rules_tools.scan(g->rules_tools.ctx, a, g->tree);
    if (!gate_run_ok(&c)) {
        fail_tail(g, st, avp_sprintf(a, "classes scan exited %s", gate_rc_str(a, &c)), &c, 12);
        return;
    }
    GateRun r = g->rules_tools.check(g->rules_tools.ctx, a, g->tree, false, NULL, 0);
    const char *err = NULL;
    AvpJson *doc = gate_parse_json_text(a, r.out, &err);
    if (!doc) {
        fail_tail(g, st, avp_sprintf(a, "check_rules exit %s produced no JSON: %s", gate_rc_str(a, &r), err), &r, 12);
        return;
    }
    g->rules_json = avp_path_join(a, g->gate_dir, "rules.json");
    if (!avp_write_text(g->rules_json, avp_json_dumps(a, doc, NULL), AVP_TM_UTF8)) shsm_raise_io();
    const AvpJson *scanned = avp_json_getz(doc, "scanned"), *findings = avp_json_getz(doc, "findings");
    const AvpJson *blocking = avp_json_getz(doc, "blocking");
    size_t ns = scanned ? avp_json_len(scanned) : 0, nf = findings ? avp_json_len(findings) : 0;
    size_t nb = blocking ? avp_json_len(blocking) : 0;
    avp_json_set(a, st->data, AVP_S("scanned"), avp_json_int(a, (i64)ns));
    avp_json_set(a, st->data, AVP_S("findings"), avp_json_int(a, (i64)nf));
    avp_json_set(a, st->data, AVP_S("blocking"), avp_json_int(a, (i64)nb));
    if (ns == 0) {
        gate_step_fail(st, "check_rules scanned no file");
        return;
    }
    if (nb || !gate_run_ok(&r)) {
        gate_step_fail(st, avp_sprintf(a, "%zu blocking [match] finding(s) in Matching units (check_rules exit %s)", nb,
                                       gate_rc_str(a, &r)));
        for (size_t i = 0; i < nb && i < 30; i++) {
            const AvpJson *b = avp_json_at(blocking, i);
            gate_step_line(a, st, avp_sprintf(a, "    %s:%s: %s [%s] %s", gate_json_pystr(a, avp_json_getz(b, "path")),
                                              gate_json_pystr(a, avp_json_getz(b, "line")),
                                              gate_json_pystr(a, avp_json_getz(b, "rule")),
                                              gate_json_pystr(a, avp_json_getz(b, "tier")),
                                              gate_json_pystr(a, avp_json_getz(b, "message"))));
        }
        return;
    }
    gate_step_ok(st, avp_sprintf(a, "no blocking [match] finding (%zu files scanned, %zu findings)", ns, nf));
}

/* ---- ledger ---------------------------------------------------------------------------------------------------- */
void gate_step_ledger(Gate *g, GateStep *st) {
    AvpArena *a = g->a;
    const char *rep = rel_v(g, "report.json");
    const char *pa[8] = {"progress", "--root", g->tree, "--report", rep, "--check"};
    const char *ea[7] = {"exceptions-check", "--root", g->tree, "--report", rep};
    size_t np = 6, ne = 5;
    if (g->rules_json) {
        pa[np++] = "--checker-json";
        pa[np++] = g->rules_json;
        ea[ne++] = "--checker-json";
        ea[ne++] = g->rules_json;
    }
    size_t n1, n2;
    const char **av1 = exe_argv(g, "ledger", &n1, pa, np);
    GateRun p = gate_run(g, av1, n1, NULL, 0);
    const char **av2 = exe_argv(g, "ledger", &n2, ea, ne);
    GateRun e = gate_run(g, av2, n2, NULL, 0);
    AvpStrVec problems = {0};
    if (!gate_run_ok(&p))
        avp_vec_push(&problems, avp_str(avp_sprintf(a, "docs/progress.md is not current (ledger progress --check exit %s)",
                                                    gate_rc_str(a, &p))));
    if (!gate_run_ok(&e)) avp_vec_push(&problems, avp_str(avp_sprintf(a, "exceptions-check exit %s", gate_rc_str(a, &e))));
    if (!gate_run_ok(&e)) {
        AvpStrVec t = gate_tail(a, gate_run_output(a, &e), 8);
        gate_step_lines_indented(a, st, &t);
    }
    if (!problems.n) {
        gate_step_ok(st, "ledger current, exceptions record consistent");
        return;
    }
    AvpStr j = avp_str_join(a, AVP_S("; "), problems.v, problems.n);
    if (g->opts.merge) {
        gate_step_fail(st, avp_cz(a, j));
        return;
    }
    gate_step_warn(st, avp_sprintf(a, "%.*s (warning; red with --merge)", AVP_SF(j)));
}

/* ---- driver ---------------------------------------------------------------------------------------------------- */
static const char *take_lock(Gate *g) {
    AvpArena *a = g->a;
    if (!avp_makedirs(g->gate_dir, true)) shsm_raise_io();
    const char *p = avp_path_join(a, g->gate_dir, "lock");
    AvpFile *f = avp_fopen(p, "xb", AVP_TM_UTF8);
    if (!f) {
        if (!avp_io_is(EEXIST)) shsm_raise_io();
        double mt = 0;
        if (!avp_getmtime(p, &mt)) shsm_raise_io();
        double age = avp_time() - mt;
        if (age < 4 * 3600)
            shsm_raise(SHSM_EXC_USAGE, "%s",
                       avp_cz(a, avp_pyfmt(a, "another gate holds %s (%.0f min old); delete it if that run is gone", p,
                                           age / 60)));
        if (!avp_remove(p)) shsm_raise_io();
        f = avp_fopen(p, "xb", AVP_TM_UTF8);
        if (!f) shsm_raise_io();
    }
    char tb[32];
    avp_float_repr(avp_time(), tb);
    const char *body = avp_sprintf(a, "{\"pid\": %u, \"time\": %s}", avp_getpid(), tb);
    avp_fwrite(f, body, strlen(body));
    avp_fclose(f);
    return p;
}

static void write_snapshot(Gate *g, bool ok, const char *line, double seconds) {
    AvpArena *a = g->a;
    GateRun head = GATE_GIT(g, "rev-parse", "HEAD");
    AvpJson *funcs = avp_json_obj(a);
    if (g->report) {
        for (size_t i = 0; i < g->report->nunits; i++) {
            const ShsmUnitReport *u = g->report->units[i];
            if (!avp_map_len(u->first_by_name)) continue;
            AvpJson *fs = avp_json_obj(a);
            AvpMapIter it = {0};
            while (avp_map_next(u->first_by_name, &it))
                avp_json_set(a, fs, it.key, avp_json_float(a, u->functions[it.val->i].fuzzy));
            avp_json_set(a, funcs, avp_str(u->name), fs);
        }
    }
    const GateStep *cov = NULL;
    for (size_t i = 0; i < GATE_NSTEPS; i++)
        if (strcmp(g->steps[i].name, "coverage") == 0) cov = &g->steps[i];
    AvpJson *snap = avp_json_obj(a);
    avp_json_set(a, snap, AVP_S("result"), avp_json_strz(a, ok ? "OK" : (g->pending_ok ? "OK-PENDING" : "RED")));
    avp_json_set(a, snap, AVP_S("line"), avp_json_strz(a, line));
    avp_json_set(a, snap, AVP_S("pending_flip"), g->pending ? avp_json_strz(a, g->pending) : avp_json_null());
    avp_json_set(a, snap, AVP_S("commit"),
                 gate_run_ok(&head) ? avp_json_str(a, avp_str_strip(head.out)) : avp_json_null());
    avp_json_set(a, snap, AVP_S("dirty"), avp_json_bool(g->dirty.n > 0));
    avp_json_set(a, snap, AVP_S("dirty_paths"), str_list_json(a, &g->dirty, 200));
    AvpStr now;
    avp_strftime_now(a, "%Y-%m-%dT%H:%M:%S", &now);
    avp_json_set(a, snap, AVP_S("time"), avp_json_str(a, now));
    avp_json_set(a, snap, AVP_S("seconds"), avp_json_float(a, avp_py_round(seconds, 1)));
    const char *oc = avp_env_get(g->env, "SHSM_OBJCACHE");
    avp_json_set(a, snap, AVP_S("objcache"), avp_json_strz(a, oc ? oc : "(unset: build default)"));
    avp_json_set(a, snap, AVP_S("tree"), avp_json_strz(a, g->tree));
    avp_json_set(a, snap, AVP_S("dol_sha1"), avp_json_strz(a, g->dol_sha1));
    AvpJson *steps = avp_json_arr(a);
    for (size_t i = 0; i < GATE_NSTEPS; i++) {
        AvpJson *d = gate_step_dict(a, &g->steps[i]);
        AvpJson *data = avp_json_obj(a);
        const AvpJson *src = g->steps[i].data;
        for (size_t k = 0; k < avp_json_len(src); k++) {
            const AvpJsonMember *mb = avp_json_member(src, k);
            if (!avp_str_eqz(mb->key, "objects")) avp_json_set(a, data, mb->key, mb->val);
        }
        avp_json_set(a, d, AVP_S("data"), data);
        avp_json_arr_push(a, steps, d);
    }
    avp_json_set(a, snap, AVP_S("steps"), steps);
    const AvpJson *objs = cov ? avp_json_getz(cov->data, "objects") : NULL;
    avp_json_set(a, snap, AVP_S("objects"), objs ? (AvpJson *)objs : avp_json_obj(a));
    avp_json_set(a, snap, AVP_S("functions"), funcs);
    if (!avp_makedirs(g->gate_dir, true)) shsm_raise_io();
    AvpJsonDumpOpts o = AVP_JSON_INDENT(1);
    AvpStr text = avp_json_dumps(a, snap, &o);
    for (int k = 0; k < (ok ? 2 : 1); k++) {
        const char *name = k == 0 ? "last.json" : "last-ok.json";
        const char *dst = avp_path_join(a, g->gate_dir, name);
        const char *tmp = avp_sprintf(a, "%s.tmp", dst);
        if (!avp_write_text(tmp, text, AVP_TM_UTF8) || !avp_replace(tmp, dst)) shsm_raise_io();
    }
    g->snapshot = snap;
}

bool gate_execute(Gate *g, const char **line) {
    AvpArena *a = g->a;
    double t0 = avp_time();
    const char *lock = take_lock(g);
    bool halted = false, build_ok = false;
    for (size_t i = 0; i < GATE_NSTEPS; i++) {
        GateStep *st = &g->steps[i];
        if (halted) {
            st->reason = "not run: an earlier step is red";
            continue;
        }
        if (gate_needs_build(st->name) && !build_ok) {
            st->reason = "not run: the build did not complete";
            continue;
        }
        double t = avp_time();
        AvpTry tr;
        if (AVP_TRY(&tr)) {
            g->step_fn[i](g, st);
            avp_try_end(&tr);
        } else if (tr.code == SHSM_EXC_USAGE) {
            avp_remove(lock);
            shsm_raise(SHSM_EXC_USAGE, "%s", tr.msg);
        } else {
            AvpStr s = shsm_exc_str(a, tr.code, tr.msg);
            gate_step_fail(st, avp_sprintf(a, "%s crashed: %s: %.*s", st->name, exc_class(tr.code), AVP_SF(s)));
        }
        st->seconds = avp_time() - t;
        if (strcmp(st->name, "build") == 0) build_ok = st->status == GATE_PASS;
        const char *mark = st->status == GATE_PASS ? "ok  " : st->status == GATE_WARN ? "WARN"
                                                         : st->status == GATE_FAIL ? "RED "
                                                                                   : "--  ";
        gate_say(g, avp_cz(a, avp_pyfmt(a, "[%s] %-11s %6.1fs  %s", mark, st->name, st->seconds, st->reason)));
        if (st->status != GATE_PASS || g->opts.verbose)
            for (size_t k = 0; k < st->lines.n; k++) gate_say(g, avp_cz(a, st->lines.v[k]));
        if (st->status == GATE_FAIL && !g->opts.keep_going) halted = true;
    }
    avp_remove(lock);
    double seconds = avp_time() - t0;
    bool ok = gate_final_line(a, g->steps, GATE_NSTEPS, g->dirty.n > 0, g->opts.merge, g->dol_sha1, g->units_linked,
                              g->functions_linked, seconds, g->pending, line);
    g->pending_ok = g->pending && strncmp(*line, "GATE OK-PENDING ", 16) == 0;
    write_snapshot(g, ok, *line, seconds);
    AvpTry tr;
    if (AVP_TRY(&tr)) {
        gate_remove_tree(a, avp_sprintf(a, "%s.old", g->tree));
        avp_try_end(&tr);
    } else if (tr.code == SHSM_EXC_USAGE || shsm_exc_is_os(tr.code)) {
        AvpStr s = shsm_exc_str(a, tr.code, tr.msg);
        gate_say(g, avp_sprintf(a, "note: could not delete %s.old: %.*s", g->tree, AVP_SF(s)));
    } else {
        shsm_raise(tr.code, "%s", tr.msg);
    }
    return ok;
}
