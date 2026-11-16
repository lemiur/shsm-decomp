/* gate_quick: --quick, the lane's cheap check between full gates (run_quick of tools/gate.py, with
 * parse_ninja_deps, changed_files and reachable_units). Not a gate. */
#include "gate.h"
#include "shsm_re.h"

static void print_line(AvpBuf *out, const char *text) {
    if (out) {
        avp_buf_putz(out, text);
        avp_buf_putc(out, '\n');
    } else {
        fputs(text, stdout);
        fputc('\n', stdout);
    }
}

AvpMap *gate_parse_ninja_deps(AvpArena *a, AvpStr text, const char *root) {
    AvpMap *deps = avp_map_new();
    AvpStrVec *cur = NULL;
    const char *nc = avp_path_normcase(a, avp_path_abspath(a, root));
    AvpStr rootn = avp_str_replace(a, avp_str(nc), AVP_S("\\"), AVP_S("/"), -1);
    rootn = avp_str_concat(a, avp_str_rstrip_chars(rootn, AVP_S("/")), AVP_S("/"));
    ShsmRe *drive = shsm_re("^[A-Za-z]:/", 0);
    AvpStrVec lines = avp_str_splitlines(text, false);
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr line = lines.v[i];
        if (!avp_str_strip(line).n) {
            cur = NULL;
            continue;
        }
        bool indented = avp_str_startswith(line, AVP_S(" "));
        AvpStr before, after;
        if (!indented && avp_str_partition(line, AVP_S(": #deps"), &before, &after)) {
            AvpStr key = avp_str_replace(a, before, AVP_S("\\"), AVP_S("/"), -1);
            bool created = false;
            AvpVal *v = avp_map_put(deps, key, &created);
            v->p = AVP_NEW(a, AvpStrVec); /* deps[cur] = [] */
            cur = v->p;
        } else if (cur && indented) {
            AvpStr p = avp_str_replace(a, avp_str_strip(line), AVP_S("\\"), AVP_S("/"), -1);
            const char *pc = avp_cz(a, p);
            ShsmReMatch m;
            if (avp_path_isabs(pc) || shsm_re_match(drive, p, 0, AVP_END, &m)) {
                AvpStr pn = avp_str_replace(a, avp_str(avp_path_normcase(a, pc)), AVP_S("\\"), AVP_S("/"), -1);
                if (avp_str_startswith(pn, rootn)) p = avp_str_slice(p, (i64)avp_u8_len(rootn), AVP_END);
            }
            avp_vec_push(cur, p);
        }
    }
    avp_vec_free(&lines);
    return deps;
}

AvpMap *gate_reachable_units(AvpArena *a, const GateUnits *units, const AvpStrVec *files, AvpMap *deps,
                             const char *root) {
    AvpSet *fset = avp_set_new(), *lower = avp_set_new();
    for (size_t i = 0; i < files->n; i++)
        avp_set_add(fset, avp_str_replace(a, files->v[i], AVP_S("\\"), AVP_S("/"), -1));
    AvpMap *out = avp_map_new();
    bool created = false;
    if (avp_set_has(fset, AVP_S("configure.py"))) {
        for (size_t i = 0; i < units->n; i++)
            if (units->v[i].has_source) avp_map_put(out, avp_str(units->v[i].name), &created)->p = "configure.py changed";
        return out;
    }
    size_t pos = 0;
    AvpStr mbr;
    while (avp_set_next(fset, &pos, &mbr)) avp_set_add(lower, avp_str_lower(a, mbr));
    for (size_t i = 0; i < units->n; i++) {
        const GateUnit *u = &units->v[i];
        if (!u->has_source) continue;
        if (u->source && avp_set_has(fset, avp_str_replace(a, avp_str(u->source), AVP_S("\\"), AVP_S("/"), -1))) {
            avp_map_put(out, avp_str(u->name), &created)->p = "source changed";
            continue;
        }
        const char *obj = u->base_obj ? u->base_obj : "";
        if (avp_path_isabs(obj)) {
            char *rel = NULL;
            if (!avp_path_relpath(a, obj, root, &rel)) shsm_raise_io();
            obj = rel;
        }
        AvpVal *dv = avp_map_get(deps, avp_str_replace(a, avp_str(obj), AVP_S("\\"), AVP_S("/"), -1));
        AvpStrVec *ds = dv ? dv->p : NULL;
        for (size_t k = 0; ds && k < ds->n; k++) {
            if (avp_set_has(lower, avp_str_lower(a, ds->v[k]))) {
                avp_map_put(out, avp_str(u->name), &created)->p = avp_sprintf(a, "includes %.*s", AVP_SF(ds->v[k]));
                break;
            }
        }
    }
    return out;
}

typedef struct {
    GateQuick *q;
    const char *root;
    const AvpEnv *env;
} QuickGit;

static GateRun quick_git(void *ctx, AvpArena *a, const char *const *args, size_t n) {
    QuickGit *c = ctx;
    const char **av = AVP_NEWN(a, const char *, n + 3);
    av[0] = "git";
    av[1] = "-C";
    av[2] = c->root;
    for (size_t i = 0; i < n; i++) av[3 + i] = args[i];
    return c->q->runner(c->q->runner_ctx, a, av, n + 3, c->root, c->env, 0);
}

static AvpStrVec changed_files(AvpArena *a, QuickGit *g, const char *rev) {
    GateRun r = quick_git(g, a, (const char *const[]){"diff", "--name-only", rev}, 3);
    if (!gate_run_ok(&r)) {
        AvpStr o = avp_u8_slice(avp_str_strip(gate_run_output(a, &r)), 0, 200);
        shsm_raise(SHSM_EXC_USAGE, "git diff %s failed: %.*s", rev, AVP_SF(o));
    }
    GateRun u = quick_git(g, a, (const char *const[]){"ls-files", "--others", "--exclude-standard"}, 3);
    AvpSet *s = avp_set_new();
    for (int k = 0; k < 2; k++) {
        AvpStrVec ls = avp_str_splitlines(k == 0 ? r.out : u.out, false);
        for (size_t i = 0; i < ls.n; i++) {
            AvpStr t = avp_str_strip(ls.v[i]);
            if (t.n) avp_set_add(s, t);
        }
        avp_vec_free(&ls);
    }
    return avp_set_sorted(s);
}

static bool red(AvpArena *a, AvpBuf *out, const char *check, const char *why, const AvpStrVec *lines,
                const char **line) {
    for (size_t i = 0; lines && i < lines->n; i++) print_line(out, avp_cz(a, lines->v[i]));
    *line = avp_sprintf(a, "QUICK RED %s: %s", check, why);
    return false;
}

static AvpStrVec indent_all(AvpArena *a, const AvpStrVec *v, size_t cap) {
    AvpStrVec out = {0};
    for (size_t i = 0; i < v->n && i < cap; i++) avp_vec_push(&out, avp_str(avp_sprintf(a, "    %.*s", AVP_SF(v->v[i]))));
    return out;
}

static void append_label(AvpArena *a, GateFalls *dst, const GateFalls *src, const char *suffix) {
    for (size_t i = 0; i < src->n; i++) {
        GateFall f = src->v[i];
        f.label = avp_sprintf(a, "%s%s", f.label ? f.label : "", suffix);
        /* a list in the Python: keys never collide across the two sources, but keep every entry */
        f.key = avp_sprintf(a, "%zu\x01%s", dst->n, f.key);
        gate_falls_put(dst, f);
    }
}

bool gate_run_quick(AvpArena *a, const GateOpts *opts, const char *root, GateQuick *q, const char **line) {
    AvpBuf *out = q->out;
    double t0 = avp_time();
    const char *version = shsm_read_version(root);
    AvpEnv *env = avp_env_current(a);
    if (opts->no_objcache) avp_env_set(env, "SHSM_OBJCACHE", "0");
    const char *rev = opts->since;
    AvpJson *snap = NULL;
    const char *snap_path = avp_path_joinv(a, 4, (const char *const[]){root, "build", "gate", "last-ok.json"});
    if (avp_isfile(snap_path)) {
        AvpStr text;
        if (!avp_read_text(a, snap_path, AVP_TM_UTF8, &text)) shsm_raise_io();
        AvpJsonError e;
        snap = avp_json_parse(a, text, &e);
        if (!snap) shsm_raise(SHSM_EXC_VALUE, "%.*s", AVP_SF(avp_json_error_str(a, &e)));
    }
    const AvpJson *snap_commit = snap ? avp_json_getz(snap, "commit") : NULL;
    if (!rev || !*rev) {
        if (!snap || !avp_json_truthy(snap_commit))
            shsm_raise(SHSM_EXC_USAGE, "no full gate snapshot (build/gate/last-ok.json); give --since REV");
        rev = gate_json_pystr(a, snap_commit);
    }
    QuickGit g = {q, root, env};
    AvpStrVec files = changed_files(a, &g, rev);
    const char *cav[1] = {gate_configure_exe(a, opts, q->exe_dir)};
    GateRun r = q->runner(q->runner_ctx, a, cav, 1, root, env, 0);
    if (!gate_run_ok(&r)) {
        AvpStrVec t = gate_tail(a, gate_run_output(a, &r), 12);
        return red(a, out, "build", avp_sprintf(a, "configure exited %s", gate_rc_str(a, &r)), &t, line);
    }
    const char *ninja = avp_path_joinv(a, 4, (const char *const[]){root, "tools", "bin", "ninja.exe"});
    const char *av[3] = {ninja};
    size_t nav = 1;
    if (opts->jobs) {
        av[nav++] = "-j";
        av[nav++] = avp_sprintf(a, "%lld", (long long)opts->jobs);
    }
    GateRun b = q->runner(q->runner_ctx, a, av, nav, root, env, 0);
    if (!gate_run_ok(&b)) {
        AvpStrVec t = gate_tail(a, gate_run_output(a, &b), 12);
        return red(a, out, "build", avp_sprintf(a, "ninja exited %s", gate_rc_str(a, &b)), &t, line);
    }
    const char *dtk = avp_path_joinv(a, 4, (const char *const[]){root, "tools", "bin", "dtk.exe"});
    GateRun c = q->runner(q->runner_ctx, a,
                          (const char *const[]){dtk, "shasum", "-c", avp_sprintf(a, "config/%s/build.sha1", version)}, 4,
                          root, env, 0);
    bool c_ok = false;
    if (gate_run_ok(&c)) {
        const char *want = avp_sprintf(a, "build/%s/main.dol: OK", version);
        AvpStrVec ls = avp_str_splitlines(gate_plain(a, gate_run_output(a, &c)), false);
        for (size_t i = 0; i < ls.n && !c_ok; i++) c_ok = avp_str_eqz(avp_str_strip(ls.v[i]), want);
        avp_vec_free(&ls);
    }
    if (!c_ok) {
        AvpStrVec t = gate_tail(a, gate_run_output(a, &c), 4);
        return red(a, out, "checksum",
                   avp_sprintf(a, "build/%s/main.dol differs from the contract (dtk exit %s)", version, gate_rc_str(a, &c)),
                   &t, line);
    }
    GateUnits units = {0};
    q->load_units(q->load_units_ctx, a, root, version, &units);
    GateRun d = q->runner(q->runner_ctx, a, (const char *const[]){ninja, "-t", "deps"}, 3, root, env, 0);
    AvpMap *deps = gate_run_ok(&d) ? gate_parse_ninja_deps(a, d.out, root) : avp_map_new();
    AvpMap *reach = gate_reachable_units(a, &units, &files, deps, root);
    if (!avp_map_len(reach))
        return red(a, out, "empty",
                   avp_sprintf(a, "no unit reachable from %zu changed file(s) since %.*s", files.n,
                               AVP_SF(avp_u8_slice(avp_str(rev), 0, 12))),
                   NULL, line);
    ShsmReport *fresh = shsm_read_report(avp_path_joinv(a, 4, (const char *const[]){root, "build", version, "report.json"}));
    ShsmReport *base =
        shsm_read_report(avp_path_joinv(a, 4, (const char *const[]){root, "config", version, "baseline_report.json"}));
    const AvpJson *snapfun = NULL;
    if (!opts->since || !*opts->since || (snap && avp_json_is_str(snap_commit) && avp_str_eqz(snap_commit->u.str, rev)))
        snapfun = snap ? avp_json_getz(snap, "functions") : NULL;
    AvpStrVec reach_sorted = avp_map_keys(reach);
    avp_sort_strs(reach_sorted.v, reach_sorted.n);
    AvpSet *reach_set = avp_set_new();
    for (size_t i = 0; i < reach_sorted.n; i++) avp_set_add(reach_set, reach_sorted.v[i]);
    GateFalls fallen = {0}, snap_fallen = {0};
    gate_fallen_functions(a, base, fresh, reach_set, &fallen);
    for (size_t i = 0; i < reach_sorted.n; i++) {
        const char *name = avp_cz(a, reach_sorted.v[i]);
        const ShsmUnitReport *ur = shsm_report_unit(fresh, name);
        if (!ur) continue;
        const AvpJson *su = snapfun ? avp_json_getz(snapfun, name) : NULL;
        AvpMapIter it = {0};
        while (avp_map_next(ur->first_by_name, &it)) {
            const ShsmFunctionReport *f = &ur->functions[it.val->i];
            const AvpJson *pv = avp_json_is_obj(su) ? avp_json_get(su, avp_str(f->name)) : NULL;
            if (!pv || pv->kind == AVP_JSON_NULL) continue;
            double prev = avp_json_f64(pv, 0.0);
            const char *key = f->has_address ? gate_addr_key(a, (i64)f->address) : gate_tuple_key(a, name, f->name);
            if (f->fuzzy < prev - GATE_EPS && !gate_falls_get(&fallen, key)) {
                GateFall sf = {.key = gate_tuple_key(a, name, f->name), .unit = name, .name = f->name, .old = prev,
                               .has_new = true, .new_ = f->fuzzy};
                gate_falls_put(&snap_fallen, sf);
            }
        }
    }
    GateFalls edited = {0}, unchanged = {0};
    if (fallen.n || snap_fallen.n) {
        for (int k = 0; k < 2; k++) {
            const GateFalls *src = k == 0 ? &fallen : &snap_fallen;
            if (!src->n) continue;
            const char *crev = rev;
            if (k == 0) {
                GateRun br = quick_git(&g, a,
                                       (const char *const[]){"log", "-1", "--format=%H", "--",
                                                             avp_sprintf(a, "config/%s/baseline_report.json", version)},
                                       5);
                crev = gate_run_ok(&br) ? avp_cz(a, avp_str_strip(br.out)) : "";
            }
            GateFalls e = {0}, u = {0};
            if (q->classify) q->classify(q->classify_ctx, a, src, &e, &u);
            else gate_classify_falls(a, root, quick_git, &g, crev, fresh, src, &q->indexer, &e, &u, NULL);
            append_label(a, &edited, &e, k == 0 ? ", baseline" : ", last gate");
            append_label(a, &unchanged, &u, k == 0 ? ", baseline" : ", last gate");
        }
    }
    AvpStrVec problems = {0};
    for (size_t i = 0; i < edited.n; i++) {
        const GateFall *x = &edited.v[i];
        const char *nw = x->has_new ? avp_cz(a, avp_pyfmt(a, "%.2f%%", x->new_)) : "gone";
        avp_vec_push(&problems, avp_pyfmt(a, "%s %s: %.2f%% -> %s (%s)", x->unit, x->name, x->old, nw, x->label));
    }
    if (unchanged.n) {
        print_line(out, avp_sprintf(a, "informational: %zu fall(s) in unchanged functions (not blocking)", unchanged.n));
        for (size_t i = 0; i < unchanged.n && i < 20; i++) print_line(out, gate_format_fall(a, &unchanged.v[i]));
    }
    i64 nfun = 0, same_obj = 0;
    const AvpJson *snap_objs = snap ? avp_json_getz(snap, "objects") : NULL;
    for (size_t i = 0; i < reach_sorted.n; i++) {
        const char *name = avp_cz(a, reach_sorted.v[i]);
        const GateUnit *u = NULL;
        for (size_t k = 0; k < units.n; k++)
            if (strcmp(units.v[k].name, name) == 0) u = &units.v[k];
        const ShsmUnitReport *ur = shsm_report_unit(fresh, name);
        if (!ur) {
            avp_vec_push(&problems, avp_str(avp_sprintf(a, "%s: not in the report", name)));
            continue;
        }
        nfun += (i64)avp_map_len(ur->first_by_name);
        if (u && u->status && strcmp(u->status, "Matching") == 0 && ur->fuzzy < 100.0 - GATE_EPS)
            avp_vec_push(&problems, avp_pyfmt(a, "%s: Matching unit at %.2f%%", name, ur->fuzzy));
        const char *o = (u && u->base_obj && *u->base_obj) ? avp_path_join(a, root, u->base_obj) : NULL;
        if (o && snap && avp_isfile(o)) {
            const AvpJson *so = avp_json_is_obj(snap_objs) ? avp_json_getz(snap_objs, name) : NULL;
            if (avp_json_is_str(so) && avp_str_eqz(so->u.str, gate_sha1_file(a, o))) same_obj++;
        }
    }
    if (problems.n) {
        AvpStrVec ls = indent_all(a, &problems, 40);
        return red(a, out, "scores", avp_sprintf(a, "%zu problem(s), e.g. %.*s", problems.n, AVP_SF(problems.v[0])), &ls,
                   line);
    }
    const char **srcs = AVP_NEWN(a, const char *, files.n + 1);
    size_t ns = 0;
    for (size_t i = 0; i < files.n; i++) {
        const char *f = avp_cz(a, files.v[i]);
        if (strncmp(f, "src/", 4) == 0 && avp_isfile(avp_path_join(a, root, f))) srcs[ns++] = f;
    }
    if (ns) {
        GateRun rr = q->rules_tools.check ? q->rules_tools.check(q->rules_tools.ctx, a, root, true, srcs, ns)
                                          : gate_check_rules_linked(NULL, a, root, true, srcs, ns);
        const char *err = NULL;
        AvpJson *doc = gate_parse_json_text(a, rr.out, &err);
        if (!doc) {
            AvpStrVec t = gate_tail(a, gate_run_output(a, &rr), 12);
            return red(a, out, "rules", avp_sprintf(a, "check_rules exit %s produced no JSON", gate_rc_str(a, &rr)), &t,
                       line);
        }
        const AvpJson *bl = avp_json_getz(doc, "blocking");
        if (avp_json_truthy(bl) || !gate_run_ok(&rr)) {
            size_t nb = bl ? avp_json_len(bl) : 0;
            AvpStrVec ls = {0};
            for (size_t i = 0; i < nb && i < 20; i++) {
                const AvpJson *x = avp_json_at(bl, i);
                avp_vec_push(&ls, avp_str(avp_sprintf(a, "    %s:%s: %s %s", gate_json_pystr(a, avp_json_getz(x, "path")),
                                                      gate_json_pystr(a, avp_json_getz(x, "line")),
                                                      gate_json_pystr(a, avp_json_getz(x, "rule")),
                                                      gate_json_pystr(a, avp_json_getz(x, "message")))));
            }
            return red(a, out, "rules", avp_sprintf(a, "%zu blocking [match] finding(s) in changed files", nb), &ls, line);
        }
    }
    for (size_t i = 0; i < reach_sorted.n && i < 30; i++) {
        AvpVal *v = avp_map_get(reach, reach_sorted.v[i]);
        print_line(out, avp_cz(a, avp_pyfmt(a, "    %-60S %s", &reach_sorted.v[i], (const char *)v->p)));
    }
    AvpStr rev12 = avp_u8_slice(avp_str(rev), 0, 12);
    *line = avp_cz(a, avp_pyfmt(a,
                                "QUICK OK (not a gate) units=%d functions=%d since=%S files=%d, checksum OK, %d/%d "
                                "objects identical to the last gate, %d informational fall(s) (%.0fs)",
                                avp_map_len(reach), nfun, &rev12, files.n,
                                same_obj, avp_map_len(reach), unchanged.n, avp_time() - t0));
    return true;
}
