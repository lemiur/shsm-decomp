/* gate_util: the small helpers, the hash contract, the pending-flip check, the runner and the Step results of
 * tools/gate.py. */
#include "gate.h"
#include "shsm_re.h"

const char *const gate_step_names[GATE_NSTEPS] = {"preflight", "build",  "checksum", "coverage",
                                                   "regressions", "layout", "rules",    "ledger"};

bool gate_needs_build(const char *step) {
    static const char *const nb[] = {"checksum", "coverage", "regressions", "layout", "rules", "ledger"};
    for (size_t i = 0; i < 6; i++)
        if (strcmp(step, nb[i]) == 0) return true;
    return false;
}

const char *const gate_allow_dirty[] = {"build/*",       "*.sqlite",          "*.sqlite-wal", "*.sqlite-shm",
                                        "*.sqlite-journal", "*/__pycache__/*", "__pycache__/*", "*.pyc",
                                        NULL};

/* ---- the runner ------------------------------------------------------------------------------------------------ */
GateRun gate_default_runner(void *ctx, AvpArena *a, const char *const *argv, size_t argc, const char *cwd,
                            const AvpEnv *env, double timeout) {
    (void)ctx;
    const char **av = AVP_NEWN(a, const char *, argc + 1);
    for (size_t i = 0; i < argc; i++) av[i] = argv[i];
    AvpProcSpec spec = {0};
    spec.argv = av;
    spec.cwd = cwd;
    spec.env = env;
    spec.out = AVP_IO_CAPTURE;
    spec.err = AVP_IO_CAPTURE;
    spec.text = true;
    spec.errors = AVP_ERRORS_REPLACE;
    spec.timeout = timeout > 0 ? timeout : 0;
    spec.arena = a;
    double t0 = avp_perf_counter();
    AvpProcResult r = avp_run(&spec);
    GateRun out = {0};
    out.seconds = avp_perf_counter() - t0;
    if (r.status == AVP_PROC_START_FAILED) {
        const char *m = r.message ? r.message : "cannot start";
        if (r.winerror == 2 || r.winerror == 3) shsm_raise(SHSM_EXC_USAGE, "cannot run %s: %s", argc ? argv[0] : "", m);
        shsm_raise(SHSM_EXC_OS, "%s", m);
    }
    out.out = r.out.p ? r.out : AVP_S("");
    out.err = r.err.p ? r.err : AVP_S("");
    if (r.status == AVP_PROC_TIMEOUT) {
        out.timed_out = true;
        out.returncode = -1;
    } else {
        out.returncode = r.returncode;
    }
    return out;
}

AvpStr gate_run_output(AvpArena *a, const GateRun *r) { return avp_str_concat(a, r->out, r->err); }

const char *gate_rc_str(AvpArena *a, const GateRun *r) {
    return r->timed_out ? "None" : avp_sprintf(a, "%lld", (long long)r->returncode);
}

/* ---- small helpers --------------------------------------------------------------------------------------------- */
char *gate_sha1_file(AvpArena *a, const char *path) {
    char *hex = avp_file_digest_hex(a, AVP_HASH_SHA1, path);
    if (!hex) shsm_raise_io();
    return hex;
}

AvpStr gate_plain(AvpArena *a, AvpStr text) {
    return shsm_re_sub(a, shsm_re("\\x1b\\[[0-9;]*[A-Za-z]", 0), AVP_S(""), text, 0);
}

AvpStrVec gate_tail(AvpArena *a, AvpStr text, size_t n) {
    AvpStrVec all = avp_str_splitlines(gate_plain(a, text), false);
    AvpStrVec out = {0};
    for (size_t i = 0; i < all.n; i++)
        if (avp_str_strip(all.v[i]).n) avp_vec_push(&out, all.v[i]);
    avp_vec_free(&all);
    if (out.n > n) {
        memmove(out.v, out.v + (out.n - n), n * sizeof(AvpStr));
        out.n = n;
    }
    return out;
}

bool gate_is_allowed_dirty(AvpArena *a, const char *path) {
    const char *p = avp_cz(a, avp_str_replace(a, avp_str(path), AVP_S("\\"), AVP_S("/"), -1));
    for (size_t i = 0; gate_allow_dirty[i]; i++)
        if (avp_fnmatch(p, gate_allow_dirty[i])) return true;
    return false;
}

GatePorcelainVec gate_parse_porcelain_z(AvpStr text) {
    GatePorcelainVec out = {0};
    AvpStrVec parts = avp_str_split(text, AVP_S("\0"), -1);
    size_t i = 0;
    while (i < parts.n) {
        AvpStr e = parts.v[i++];
        if (avp_u8_len(e) < 4) continue;
        AvpStr code = avp_u8_slice(e, 0, 2), path = avp_u8_slice(e, 3, AVP_END);
        avp_vec_push(&out, ((GatePorcelain){code, path}));
        if (avp_str_contains(code, AVP_S("R")) || avp_str_contains(code, AVP_S("C"))) {
            if (i < parts.n && parts.v[i].n) avp_vec_push(&out, ((GatePorcelain){code, parts.v[i]}));
            i++;
        }
    }
    avp_vec_free(&parts);
    return out;
}

AvpJson *gate_parse_json_text(AvpArena *a, AvpStr text, const char **err) {
    i64 i = avp_str_findc(text, '{');
    if (i < 0) {
        *err = "no JSON object in the output";
        return NULL;
    }
    AvpStr s = avp_str_slice(text, i, AVP_END);
    AvpJsonError e;
    AvpJson *doc = avp_json_parse(a, s, &e);
    if (!doc && e.kind == AVP_JSON_E_DECODE && avp_str_eqz(e.msg, "Extra data")) {
        /* raw_decode stops after the first document: parse the prefix up to the extra data */
        AvpStr head = avp_u8_slice(s, 0, e.pos);
        doc = avp_json_parse(a, head, &e);
    }
    if (!doc) {
        *err = avp_cz(a, avp_json_error_str(a, &e));
        return NULL;
    }
    return doc;
}

const char *gate_json_pystr(AvpArena *a, const AvpJson *v) {
    if (!v || v->kind == AVP_JSON_NULL) return "None";
    switch (v->kind) {
    case AVP_JSON_BOOL: return v->b ? "True" : "False";
    case AVP_JSON_STRING: return avp_cz(a, v->u.str);
    case AVP_JSON_FLOAT: {
        char buf[32];
        avp_float_repr(v->u.f, buf);
        return avp_strdup(a, buf);
    }
    default: return avp_cz(a, avp_json_dumps(a, v, NULL));
    }
}

const char *gate_repr_str_list(AvpArena *a, const AvpStr *items, size_t n) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putc(&b, '[');
    for (size_t i = 0; i < n; i++) {
        if (i) avp_buf_putz(&b, ", ");
        avp_buf_put(&b, avp_repr_str(a, items[i]));
    }
    avp_buf_putc(&b, ']');
    return avp_buf_cz(&b);
}

const char *gate_has_reparse_points(AvpArena *a, const char *top) {
    AvpWalk *w = avp_walk_open(top, true);
    AvpWalkItem it = {0};
    const char *found = NULL;
    while (!found && avp_walk_next(w, &it)) {
        for (int pass = 0; pass < 2 && !found; pass++) {
            const AvpStrVec *names = pass == 0 ? &it.dirs : &it.files;
            for (size_t i = 0; i < names->n && !found; i++) {
                const char *p = avp_path_join(a, avp_cz(a, it.dirpath), avp_cz(a, names->v[i]));
                AvpStat st;
                if (!avp_lstat(p, &st)) continue;
                if (AVP_S_ISLNK(st.mode) || (st.file_attributes & AVP_FILE_ATTRIBUTE_REPARSE_POINT)) found = p;
            }
        }
    }
    avp_walk_close(w);
    return found;
}

void gate_remove_tree(AvpArena *a, const char *top) {
    if (!avp_lexists(top)) return;
    const char *bad = gate_has_reparse_points(a, top);
    if (bad)
        shsm_raise(SHSM_EXC_USAGE,
                   "refusing to delete %s: it contains a junction or symlink (%s); remove the link itself first "
                   "(os.rmdir / `cmd /c rmdir`)",
                   top, bad);
    if (!avp_rmtree(top, AVP_RMTREE_CLEAR_READONLY)) shsm_raise_io();
}

void gate_link_or_copy(AvpArena *a, const char *src, const char *dst) {
    if (!avp_makedirs(avp_cz(a, avp_path_dirname(dst)), true)) shsm_raise_io();
    if (avp_link(src, dst)) return;
    if (!avp_copy2(src, dst)) shsm_raise_io();
}

/* ---- the hash contract ----------------------------------------------------------------------------------------- */
static bool fail_io(AvpArena *a, const char **err) {
    *err = avp_cz(a, avp_io_err_str(a));
    return false;
}

bool gate_read_contract(AvpArena *a, const char *build_sha1, const char *config_yml, const char *root,
                        GateContract *out, const char **err) {
    AvpStr text;
    if (!avp_read_text(a, build_sha1, AVP_TM_UTF8, &text)) return fail_io(a, err);
    AvpStrVec words = avp_str_split(text, (AvpStr){0}, -1);
    ShsmReMatch m;
    if (words.n < 2 || !shsm_re_fullmatch(shsm_re("[0-9a-fA-F]{40}", 0), words.v[0], 0, AVP_END, &m)) {
        *err = avp_sprintf(a, "%s is not `<sha1>  <path>`", build_sha1);
        return false;
    }
    const char *yml_hash = NULL, *base = NULL, *obj = NULL;
    AvpStrVec lines;
    if (!avp_read_lines(a, config_yml, AVP_TM_UTF8, &lines)) return fail_io(a, err);
    ShsmRe *re = shsm_re("^(hash|object_base|object):\\s*(\\S+)", 0);
    for (size_t i = 0; i < lines.n; i++) {
        if (!shsm_re_match(re, lines.v[i], 0, AVP_END, &m)) continue;
        AvpStr k = shsm_re_group(&m, 1);
        AvpStr v = avp_str_strip_chars(shsm_re_group(&m, 2), AVP_S("'\""));
        if (avp_str_eqz(k, "hash")) yml_hash = avp_cz(a, avp_str_lower(a, v));
        else if (avp_str_eqz(k, "object_base")) base = avp_cz(a, v);
        else obj = avp_cz(a, v);
    }
    if (!yml_hash || !obj) {
        *err = avp_sprintf(a, "%s has no hash: or object: line", config_yml);
        return false;
    }
    const char *parts[] = {root, base ? base : "", obj};
    out->sha1 = avp_cz(a, avp_str_lower(a, words.v[0]));
    out->path = avp_cz(a, words.v[1]);
    out->yml_hash = yml_hash;
    out->retail = avp_path_joinv(a, 3, parts);
    avp_vec_free(&words);
    return true;
}

bool gate_verify_contract(AvpArena *a, const char *build_sha1, const char *config_yml, const char *root,
                          const char *retail, const char **why, const char **sha1) {
    GateContract c;
    *sha1 = NULL;
    if (!gate_read_contract(a, build_sha1, config_yml, root, &c, why)) return false;
    *sha1 = c.sha1;
    retail = retail ? retail : c.retail;
    if (strcmp(c.yml_hash, c.sha1) != 0) {
        *why = avp_sprintf(a, "config.yml hash %.12s differs from build.sha1 %.12s", c.yml_hash, c.sha1);
        return false;
    }
    if (!avp_isfile(retail)) {
        *why = avp_sprintf(a, "retail DOL %s is missing", retail);
        return false;
    }
    const char *got = gate_sha1_file(a, retail);
    if (strcmp(got, c.sha1) != 0) {
        *why = avp_sprintf(a, "retail DOL %s has sha1 %.12s, the contract says %.12s", retail, got, c.sha1);
        return false;
    }
    *why = avp_sprintf(a, "contract %s (build.sha1, config.yml, retail DOL agree)", c.sha1);
    return true;
}

/* ---- --pending-flip -------------------------------------------------------------------------------------------- */
const char *gate_unit_stem(AvpArena *a, const char *unit) {
    AvpStr u = avp_str_strip(avp_str_replace(a, avp_str(unit), AVP_S("\\"), AVP_S("/"), -1));
    if (avp_str_startswith(u, AVP_S("main/"))) u = avp_str_slice(u, 5, AVP_END);
    const char *cz = avp_cz(a, u);
    AvpStr root, ext;
    avp_path_splitext(cz, &root, &ext);
    if (avp_str_eqz(ext, ".c") || avp_str_eqz(ext, ".cpp") || avp_str_eqz(ext, ".cp") || avp_str_eqz(ext, ".s"))
        return avp_cz(a, root);
    return cz;
}

static AvpStr crlf_to_lf(AvpArena *a, AvpStr s) { return avp_str_replace(a, s, AVP_S("\r\n"), AVP_S("\n"), -1); }

const char *gate_check_pending_flip(AvpArena *a, AvpStr head_text, AvpStr work_text, const char *unit) {
    AvpStr head = crlf_to_lf(a, head_text), work = crlf_to_lf(a, work_text);
    const char *stem = gate_unit_stem(a, unit);
    ShsmRe *re = shsm_re("Object\\(\\s*(NonMatching|Matching|Equivalent)\\s*,\\s*\"([^\"]+)\"", 0);
    ShsmReIter it;
    ShsmReMatch m, hit = {0};
    size_t nhits = 0;
    shsm_re_iter_init(&it, re, head, 0, AVP_END);
    while (shsm_re_iter_next(&it, &m)) {
        i64 ms = shsm_re_start(&m, 0);
        i64 line_start = avp_str_rfind(head, AVP_S("\n"), 0, ms) + 1;
        if (avp_str_contains(avp_str_slice(head, line_start, ms), AVP_S("#"))) continue; /* commented out */
        AvpStr r, e;
        const char *g2 = avp_cz(a, shsm_re_group(&m, 2));
        avp_path_splitext(g2, &r, &e);
        if (avp_str_eqz(r, stem)) {
            if (nhits++ == 0) hit = m;
        }
    }
    if (nhits == 0) return avp_sprintf(a, "%s has no Object entry in HEAD's configure.py", stem);
    if (nhits > 1) return avp_sprintf(a, "%s has %zu Object entries in HEAD's configure.py", stem, nhits);
    AvpStr g1 = shsm_re_group(&hit, 1), g2 = shsm_re_group(&hit, 2);
    if (!avp_str_eqz(g1, "NonMatching"))
        return avp_sprintf(a, "%.*s is %.*s at HEAD, not NonMatching", AVP_SF(g2), AVP_SF(g1));
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_put(&b, avp_str_slice(head, 0, shsm_re_start(&hit, 1)));
    avp_buf_putz(&b, "Matching");
    avp_buf_put(&b, avp_str_slice(head, shsm_re_end(&hit, 1), AVP_END));
    AvpStr want = avp_buf_str(&b);
    if (avp_str_eq(work, head))
        return avp_sprintf(a, "configure.py is unchanged from HEAD (%.*s is still NonMatching)", AVP_SF(g2));
    if (!avp_str_eq(work, want)) {
        AvpStrVec hl = avp_str_split(head, AVP_S("\n"), -1), wl = avp_str_split(work, AVP_S("\n"), -1);
        size_t mx = hl.n > wl.n ? hl.n : wl.n, shown = 0;
        AvpBuf d;
        avp_buf_init(&d, a);
        for (size_t i = 0; i < mx && shown < 8; i++) {
            bool same = i < hl.n && i < wl.n && avp_str_eq(hl.v[i], wl.v[i]);
            if (same) continue;
            if (shown++) avp_buf_putz(&d, ", ");
            avp_buf_printf(&d, "%zu", i + 1);
        }
        avp_vec_free(&hl);
        avp_vec_free(&wl);
        return avp_sprintf(a,
                           "configure.py differs from HEAD by more than %.*s's status NonMatching -> Matching "
                           "(differing line(s) %s)",
                           AVP_SF(g2), avp_buf_cz(&d));
    }
    return "";
}

/* ---- steps ----------------------------------------------------------------------------------------------------- */
const char *gate_status_name(GateStatus s) {
    switch (s) {
    case GATE_PASS: return "pass";
    case GATE_WARN: return "warn";
    case GATE_FAIL: return "fail";
    default: return "not-run";
    }
}

void gate_step_init(AvpArena *a, GateStep *st, const char *name) {
    memset(st, 0, sizeof *st);
    st->name = name;
    st->reason = "";
    st->data = avp_json_obj(a);
}

GateStep *gate_step_fail(GateStep *st, const char *reason) {
    st->status = GATE_FAIL;
    st->reason = reason;
    return st;
}

GateStep *gate_step_ok(GateStep *st, const char *reason) {
    st->status = GATE_PASS;
    st->reason = reason;
    return st;
}

GateStep *gate_step_warn(GateStep *st, const char *reason) {
    st->status = GATE_WARN;
    st->reason = reason;
    return st;
}

void gate_step_line(AvpArena *a, GateStep *st, const char *line) { avp_vec_push(&st->lines, avp_str(avp_strdup(a, line))); }

void gate_step_lines_indented(AvpArena *a, GateStep *st, const AvpStrVec *lines) {
    for (size_t i = 0; i < lines->n; i++) gate_step_line(a, st, avp_sprintf(a, "    %.*s", AVP_SF(lines->v[i])));
}

AvpJson *gate_step_dict(AvpArena *a, const GateStep *st) {
    AvpJson *d = avp_json_obj(a);
    avp_json_set(a, d, AVP_S("name"), avp_json_strz(a, st->name));
    avp_json_set(a, d, AVP_S("status"), avp_json_strz(a, gate_status_name(st->status)));
    avp_json_set(a, d, AVP_S("reason"), avp_json_strz(a, st->reason));
    avp_json_set(a, d, AVP_S("seconds"), avp_json_float(a, avp_py_round(st->seconds, 2)));
    AvpJson *lines = avp_json_arr(a);
    for (size_t i = 0; i < st->lines.n && i < 200; i++) avp_json_arr_push(a, lines, avp_json_str(a, st->lines.v[i]));
    avp_json_set(a, d, AVP_S("lines"), lines);
    avp_json_set(a, d, AVP_S("data"), st->data);
    return d;
}

bool gate_verdict(AvpArena *a, GateStep *steps, size_t n, bool dirty, bool merge, GateStep **bad) {
    *bad = NULL;
    for (size_t k = 0; k < GATE_NSTEPS; k++) {
        const char *name = gate_step_names[k];
        GateStep *s = NULL;
        for (size_t i = 0; i < n; i++)
            if (strcmp(steps[i].name, name) == 0) {
                s = &steps[i];
                break;
            }
        if (!s) {
            GateStep *m = AVP_NEW(a, GateStep);
            gate_step_init(a, m, name);
            *bad = gate_step_fail(m, "step missing");
            return false;
        }
        if (s->status == GATE_PASS) continue;
        if (s->status == GATE_WARN && strcmp(name, "ledger") == 0 && !merge) continue;
        if (s->status == GATE_NOT_RUN && !*s->reason) s->reason = "did not run";
        *bad = s;
        return false;
    }
    return !dirty;
}

bool gate_final_line(AvpArena *a, GateStep *steps, size_t n, bool dirty, bool merge, const char *dol_sha1,
                     i64 units, i64 functions, double seconds, const char *pending, const char **line) {
    GateStep *bad = NULL;
    bool ok = gate_verdict(a, steps, n, dirty && !pending, merge, &bad);
    if (ok && pending) {
        *line = avp_cz(a, avp_pyfmt(a,
                                    "GATE OK-PENDING %s units=%d functions=%d pending-flip=%s (%.0fs): not a banked "
                                    "gate, commit configure.py and gate again",
                                    dol_sha1, units, functions, pending, seconds));
        return false;
    }
    if (ok) {
        *line = avp_cz(a, avp_pyfmt(a, "GATE OK %s units=%d functions=%d (%.0fs)", dol_sha1, units, functions, seconds));
        return true;
    }
    if (!bad) {
        *line = avp_cz(a, avp_pyfmt(a, "GATE DIRTY %s units=%d functions=%d (%.0fs): tree not clean, not a gate result",
                                    dol_sha1, units, functions, seconds));
        return false;
    }
    AvpStr why = avp_str(*bad->reason ? bad->reason : gate_status_name(bad->status));
    AvpStrVec ls = avp_str_splitlines(why, false);
    AvpStr first = ls.n ? ls.v[0] : AVP_S("");
    *line = avp_sprintf(a, "GATE RED %s: %.*s", bad->name, AVP_SF(first));
    avp_vec_free(&ls);
    return false;
}

/* ---- units ----------------------------------------------------------------------------------------------------- */
#include "shsm_units.h"

void gate_load_units_real(void *ctx, AvpArena *a, const char *root, const char *version, GateUnits *out) {
    (void)ctx;
    const ShsmUnits *us = shsm_load_units(shsm_repo_new(root, version));
    for (size_t i = 0; i < us->n; i++) {
        const ShsmUnit *u = us->v[i];
        GateUnit g = {u->name, u->path, u->status, u->source, u->base_obj, shsm_unit_has_source(u)};
        (void)a;
        avp_vec_push(out, g);
    }
}
