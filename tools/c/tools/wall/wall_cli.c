/* The command-line half of tools/wall.py: compiling and comparing (_work_dir, object_for, compare_all, _find),
 * rendering one verdict (render), the commands (cmd_one, scan_unit, _select_units, _percent, run_scan, cmd_scan,
 * cmd_notes) and main. */
#include "wall.h"

#include "match.h"
#include "shsm_re.h"
#include "shsm_report.h"

static const char *const CLASS_ORDER[] = {WALL_REFERENT, WALL_CALL, WALL_CONTROL, WALL_OTHER, WALL_REGISTER};

/* ---- compiling ------------------------------------------------------------------------------------------------- */

const char *wall_work_dir(AvpArena *a, const ShsmRepo *r) { return avp_join(a, r->root, "build", "tmp", "wall"); }

/* os.path.join(base, *unit_path.split("/")) */
static const char *join_unit_path(AvpArena *a, const char *base, const char *path) {
    AvpStrVec parts = avp_str_split(avp_str(path), AVP_S("/"), -1);
    const char *out = base;
    for (size_t i = 0; i < parts.n; i++) out = avp_path_join(a, out, avp_cz(a, parts.v[i]));
    avp_vec_free(&parts);
    return out;
}

/* cli.compiler_messages: the Error blocks' first message, formatted as cli.first_error. */
AvpStr wall_first_error(AvpArena *a, AvpStr output) {
    const ShsmRe *head = shsm_re("^### (\\S+) ([^:]+):\\s*$", 0), *file = shsm_re("^#\\s+File:\\s*(.+?)\\s*$", 0),
                 *lnre = shsm_re("^#\\s+(\\d+):\\s?(.*)$", 0), *kind = shsm_re("^#\\s+(Error|Warning):", 0),
                 *msg = shsm_re("^#\\s+(?:\\((\\d+)\\)\\s*)?(.*?)\\s*$", 0);
    AvpStrVec lines = avp_str_splitlines(output, false);
    AvpStr result = {0};
    bool have = false, marked = false, done = false;
    AvpStr tool = {0}, kd = {0}, fname = {0}, code = {0}, message = {0}, lineno = {0};
    bool has_line = false, has_msg = false;
    for (size_t i = 0; i <= lines.n && !done; i++) {
        bool last = i == lines.n;
        AvpStr line = last ? AVP_S("") : avp_str_rstrip(lines.v[i]);
        ShsmReMatch m;
        bool is_head = !last && shsm_re_match(head, line, 0, AVP_END, &m);
        if (last || is_head) {
            /* close the current block: is it the first real error? */
            if (have && avp_str_eqz(kd, "Error") && has_msg && message.n) {
                AvpStr where = tool;
                if (fname.p) {
                    char *fz = avp_cz(a, avp_str_replace(a, fname, AVP_S("\\"), AVP_S("/"), -1));
                    where = avp_path_basename(fz);
                }
                AvpStr loc = has_line ? avp_pyfmt(a, "%S:%S", &where, &lineno) : where;
                AvpStr cd = code.n ? avp_pyfmt(a, "(%S) ", &code) : AVP_S("");
                result = avp_pyfmt(a, "%S: %S%S", &loc, &cd, &message);
                done = true;
                break;
            }
            if (last) break;
            have = true;
            tool = shsm_re_group(&m, 1);
            kd = avp_str_strip(shsm_re_group(&m, 2));
            fname = code = message = lineno = (AvpStr){0};
            has_line = has_msg = false;
            marked = !(avp_str_eqz(kd, "Compiler") || avp_str_eqz(kd, "Linker"));
            continue;
        }
        if (!have || !avp_str_startswith(line, AVP_S("#")) || has_msg) continue;
        AvpStr body = avp_str_strip(avp_strn(line.p + 1, line.n - 1));
        if (!body.n || avp_str_strip_chars(body, AVP_S("-")).n == 0) continue;
        if (shsm_re_match(file, line, 0, AVP_END, &m)) {
            fname = shsm_re_group(&m, 1);
            continue;
        }
        if (!marked) {
            if (shsm_re_match(kind, line, 0, AVP_END, &m)) {
                kd = shsm_re_group(&m, 1);
                marked = true;
                continue;
            }
            if (shsm_re_match(lnre, line, 0, AVP_END, &m) && !has_line) {
                AvpStr digits = shsm_re_group(&m, 1);
                i64 v = 0;
                bool ovf = false;
                avp_parse_int(digits, 10, &v, &ovf);
                lineno = avp_pyfmt(a, "%d", v);
                has_line = true;
            }
            continue;
        }
        if (!shsm_re_match(msg, line, 0, AVP_END, &m)) continue; /* the Python raised AttributeError here */
        code = shsm_re_matched(&m, 1) ? shsm_re_group(&m, 1) : (AvpStr){0};
        message = shsm_re_group(&m, 2);
        has_msg = true;
    }
    avp_vec_free(&lines);
    return result;
}

/* units.compile_source(unit, src, out_dir, repo=repo, low_priority=True, depfile=True) */
ShsmRun wall_compile_low(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u, const char *src, const char *out_dir) {
    if (!avp_makedirs(out_dir, true)) shsm_raise_io();
    ShsmCompileOpts opts = {0};
    opts.sjis = -1;
    opts.depfile = true;
    AvpStrVec argv = shsm_compile_argv(a, r, u, src, out_dir, &opts);
    const char **av = AVP_NEWN(a, const char *, argv.n + 1);
    for (size_t i = 0; i < argv.n; i++) av[i] = avp_cz(a, argv.v[i]);
    AvpProcSpec spec = {0};
    spec.argv = av;
    spec.cwd = r->root;
    spec.out = AVP_IO_CAPTURE;
    spec.err = AVP_IO_CAPTURE;
    spec.text = true;
    spec.errors = AVP_ERRORS_REPLACE;
    spec.timeout = 300;
    spec.creation_flags = AVP_BELOW_NORMAL_PRIORITY_CLASS;
    spec.arena = a;
    AvpProcResult pr = avp_run(&spec);
    ShsmRun res = {0};
    if (pr.status == AVP_PROC_START_FAILED) {
        const char *m = pr.message ? pr.message : "cannot start";
        if (pr.winerror == 2 || pr.winerror == 3) shsm_raise(SHSM_EXC_USAGE, "cannot run %s: %s", argv.n ? av[0] : "", m);
        shsm_raise(SHSM_EXC_OS, "%s", m);
    }
    avp_vec_free(&argv);
    res.out = pr.out.p ? pr.out : AVP_S("");
    res.err = pr.err.p ? pr.err : AVP_S("");
    res.timed_out = pr.status == AVP_PROC_TIMEOUT;
    res.returncode = res.timed_out ? -1 : pr.returncode;
    return res;
}

WallObject wall_object_for(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u, bool built, const char *source,
                           const char *tag) {
    WallObject o = {0};
    if (built) {
        const char *obj = u->base_obj ? avp_path_join(a, r->root, u->base_obj) : NULL;
        if (!obj || !avp_exists(obj))
            shsm_raise(SHSM_EXC_USAGE, "no built object for %s (run the build first or drop --built)", u->name);
        o.obj = obj;
        return o;
    }
    const char *src = source ? source : u->source;
    if (!src || !*src) shsm_raise(SHSM_EXC_USAGE, "%s has no source", u->name);
    src = avp_path_join(a, r->root, src);
    const char *out_dir = join_unit_path(a, avp_path_join(a, wall_work_dir(a, r), avp_sprintf(a, "obj%s", tag)), u->path);
    ShsmRun res = wall_compile_low(a, r, u, src, out_dir);
    if (res.timed_out || res.returncode != 0) {
        AvpStr output = shsm_run_output(a, &res);
        AvpStr why = wall_first_error(a, output);
        if (!why.p || !why.n) {
            AvpStrVec all = avp_str_splitlines(output, false), keep = {0};
            for (size_t i = 0; i < all.n; i++)
                if (avp_str_strip(all.v[i]).n) avp_vec_push(&keep, all.v[i]);
            size_t from = keep.n > 3 ? keep.n - 3 : 0;
            why = avp_str_join(a, AVP_S(" / "), keep.v + from, keep.n - from);
            avp_vec_free(&all);
            avp_vec_free(&keep);
        }
        shsm_raise(SHSM_EXC_USAGE, "compile of %s failed: %.*s", src, AVP_SF(why));
    }
    if (!source) o.inputs = match_state_inputs(a, r, u, src, match_depfile_digests(a, r, src, out_dir));
    if (o.inputs && o.inputs->kind == AVP_JSON_NULL) o.inputs = NULL;
    o.obj = shsm_object_for(a, src, out_dir);
    return o;
}

CcObjectComparison *wall_compare_all(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u, const char *obj) {
    const char *out_dir = NULL;
    AvpStr o = avp_str(avp_path_normcase(a, avp_path_abspath(a, obj)));
    if (avp_str_startswith(o, avp_str(avp_path_normcase(a, r->src_obj_dir))))
        out_dir = join_unit_path(a, avp_path_join(a, wall_work_dir(a, r), "listing"), u->path);
    CcRepo cr = {r->root, r->version};
    CcError err = {0};
    CcObjectComparison *oc = cc_compare_object(a, &cr, avp_str(u->name), obj, NULL, true, NULL, NULL, NULL, out_dir,
                                               NULL, &err);
    if (!oc) match_raise_cc(&err);
    return oc;
}

static bool is_upper_hex(char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'); }

const CcCompareResult *wall_find(AvpStr symbol, const AvpMap *results, const AvpMap *retail) {
    const AvpVal *v = avp_map_get(results, symbol);
    if (v) return v->p;
    AvpMapIter it = {0};
    while (avp_map_next(results, &it)) {
        const CcCompareResult *r = it.val->p;
        if (avp_str_eq(r->retail_name, symbol)) return r;
    }
    bool fn = symbol.n >= 11 && avp_str_startswith(symbol, AVP_S("fn_"));
    for (size_t i = 3; fn && i < 11; i++) fn = is_upper_hex(symbol.p[i]);
    if (fn) {
        AvpStr prefix = avp_strn(symbol.p, 11);
        AvpMapIter jt = {0};
        while (avp_map_next(results, &jt))
            if (avp_str_startswith(jt.key, prefix)) return jt.val->p;
    }
    if (avp_map_has(retail, symbol)) return NULL;
    shsm_raise(SHSM_EXC_KEY, "%.*s: no such function in the compiled object or the retail listing", AVP_SF(symbol));
}

/* ---- rendering ------------------------------------------------------------------------------------------------- */

static AvpJson *jget(const AvpJson *o, const char *k) { return avp_json_getz(o, k); }
static size_t jlen(const AvpJson *v) { return avp_json_is_arr(v) ? avp_json_len(v) : 0; }
static size_t capn(size_t n, size_t cap, bool verbose) { return verbose || n < cap ? n : cap; }

/* ", ".join(str(x) for x in xs[:cap]) */
static AvpStr join_cap(AvpArena *a, const AvpJson *xs, size_t cap, bool verbose, const char *sep) {
    size_t n = capn(jlen(xs), cap, verbose);
    AvpStr *items = AVP_NEWN(a, AvpStr, n + 1);
    for (size_t i = 0; i < n; i++) items[i] = wall_py_str(a, avp_json_at(xs, i));
    return avp_str_join(a, avp_str(sep), items, n);
}

static AvpStr or_dash(AvpStr s) { return s.n ? s : AVP_S("-"); }

static int str_cmp(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const AvpJsonMember *p = *(const AvpJsonMember *const *)x, *q = *(const AvpJsonMember *const *)y;
    size_t n = p->key.n < q->key.n ? p->key.n : q->key.n;
    int c = memcmp(p->key.p, q->key.p, n);
    if (c) return c;
    return p->key.n < q->key.n ? -1 : p->key.n > q->key.n;
}

/* _loops(s) */
static AvpStr loops_text(AvpArena *a, const AvpJson *s) {
    const AvpJson *lp = jget(s, "loops");
    size_t n = avp_json_is_obj(lp) ? lp->u.obj.n : 0;
    const AvpJsonMember **ms = AVP_NEWN(a, const AvpJsonMember *, n + 1);
    for (size_t i = 0; i < n; i++) ms[i] = &lp->u.obj.v[i];
    avp_sort(ms, n, sizeof *ms, str_cmp, NULL, false);
    AvpStr *items = AVP_NEWN(a, AvpStr, n + 1);
    for (size_t i = 0; i < n; i++) items[i] = avp_pyfmt(a, "%d %S", wall_py_int(ms[i]->val), &ms[i]->key);
    AvpStr out = avp_str_join(a, AVP_S(", "), items, n);
    return out.n ? out : AVP_S("none");
}

static AvpStr value_part(AvpArena *a, const AvpJson *side) {
    const AvpJson *v = jget(side, "value");
    if (avp_json_is_null(v)) return AVP_S("");
    AvpStr r = wall_py_repr(a, v);
    return avp_pyfmt(a, " %S", &r);
}

static AvpStr jsz(AvpArena *a, const AvpJson *o, const char *k) { return wall_py_str(a, jget(o, k)); }

AvpStrVec wall_render(AvpArena *a, const AvpJson *d, bool verbose) {
    AvpStrVec L = {0};
    const AvpJson *rows = jget(d, "rows");
    AvpStr name = avp_json_has(d, AVP_S("name")) ? jsz(a, d, "name") : AVP_S("?");
    AvpStr rname = avp_json_has(d, AVP_S("retail_name")) ? jsz(a, d, "retail_name") : AVP_S("?");
    AvpStr classes = join_cap(a, jget(d, "classes"), 0, true, ", ");
    if (!classes.n) classes = AVP_S("none");
    avp_vec_push(&L, avp_pyfmt(a, "%S (retail %S): %d/%d rows, %d differing, classes: %S", &name, &rname,
                               wall_py_int(jget(rows, "retail")), wall_py_int(jget(rows, "compiled")),
                               wall_py_int(jget(rows, "differing")), &classes));
    const AvpJson *ref = jget(d, "referent");
    if (avp_json_truthy(ref)) {
        avp_vec_push(&L, avp_pyfmt(a, "referent: %d row(s)", (i64)jlen(ref)));
        for (size_t i = 0; i < capn(jlen(ref), 3, verbose); i++) {
            const AvpJson *r = avp_json_at(ref, i);
            const AvpJson *ds = jget(r, "describe");
            AvpStr extra = AVP_S("");
            if (avp_json_truthy(ds)) {
                const AvpJson *ra = jget(ds, "retail"), *rb = jget(ds, "compiled");
                AvpStr ak = jsz(a, ra, "kind"), av = value_part(a, ra), bk = jsz(a, rb, "kind"), bv = value_part(a, rb),
                       same = jsz(a, ds, "same");
                extra = avp_pyfmt(a, " [%S%S / %S%S: %S]", &ak, &av, &bk, &bv, &same);
            }
            AvpStr rr = jsz(a, r, "retail"), rc = jsz(a, r, "compiled");
            avp_vec_push(&L, avp_pyfmt(a, "  row %d: %S vs %S%S", wall_py_int(jget(r, "retail_row")), &rr, &rc, &extra));
            const AvpJson *det = jget(r, "detail");
            if (avp_json_truthy(det)) {
                AvpStr ds2 = avp_u8_slice(wall_py_str(a, det), 0, 110);
                avp_vec_push(&L, avp_pyfmt(a, "    codecmp: %S", &ds2));
            }
        }
    }
    const AvpJson *c = jget(d, "calls");
    if (avp_json_truthy(jget(c, "differs")) || avp_json_truthy(jget(c, "helpers"))) {
        AvpStr ro = or_dash(join_cap(a, jget(c, "retail_only"), 4, verbose, ", ")),
               co = or_dash(join_cap(a, jget(c, "compiled_only"), 4, verbose, ", "));
        avp_vec_push(&L, avp_pyfmt(a, "call set: retail %d call(s), compiled %d, size %+d rows; retail only %S; "
                                      "compiled only %S",
                                   (i64)jlen(jget(c, "retail")), (i64)jlen(jget(c, "compiled")), wall_py_int(jget(c, "size_diff")),
                                   &ro, &co));
        const AvpJson *rd = jget(c, "readings"), *hp = jget(c, "helpers");
        size_t nr = jlen(rd), nh = jlen(hp), n = capn(nr + nh, 3, verbose);
        for (size_t i = 0; i < n; i++) {
            AvpStr x = wall_py_str(a, i < nr ? avp_json_at(rd, i) : avp_json_at(hp, i - nr));
            avp_vec_push(&L, avp_pyfmt(a, "  %S", &x));
        }
    }
    const AvpJson *cf = jget(d, "control"), *sr = jget(cf, "retail"), *sc = jget(cf, "compiled");
    const AvpJson *iso = jget(cf, "isomorphic");
    const char *iso_text = (iso && iso->kind == AVP_JSON_BOOL) ? (iso->b ? "isomorphic" : "not isomorphic") : "undecided";
    AvpStr lr = loops_text(a, sr), lc = loops_text(a, sc);
    avp_vec_push(&L, avp_pyfmt(a, "control flow: %s; blocks %d/%d, edges %d/%d, loops %S / %S, returns %d/%d, "
                                  "switches %d/%d",
                               iso_text, wall_py_int(jget(sr, "blocks")), wall_py_int(jget(sc, "blocks")),
                               wall_py_int(jget(sr, "edges")), wall_py_int(jget(sc, "edges")), &lr, &lc,
                               wall_py_int(jget(sr, "returns")), wall_py_int(jget(sc, "returns")),
                               wall_py_int(jget(sr, "switches")), wall_py_int(jget(sc, "switches"))));
    const AvpJson *fi = jget(cf, "findings");
    for (size_t i = 0; i < capn(jlen(fi), 3, verbose); i++) {
        AvpStr x = wall_py_str(a, avp_json_at(fi, i));
        avp_vec_push(&L, avp_pyfmt(a, "  %S", &x));
    }
    const AvpJson *rev = jget(d, "row_evidence");
    const AvpJson *ctl = jget(rev, WALL_CONTROL);
    size_t shown = 0;
    for (size_t i = 0; i < jlen(ctl) && (verbose || shown < 2); i++) {
        const AvpJson *e = avp_json_at(ctl, i);
        if (avp_str_startswith(wall_jstr(e, "why"), AVP_S("branch"))) continue;
        AvpStr er = jsz(a, e, "retail"), rt = jsz(a, e, "retail_text"), ct = jsz(a, e, "compiled_text"),
               why = jsz(a, e, "why");
        avp_vec_push(&L, avp_pyfmt(a, "  row %S: %S | %S (%S)", &er, &rt, &ct, &why));
        shown++;
    }
    const AvpJson *rg = jget(d, "register"), *cats = jget(d, "categories");
    const AvpJson *st = jget(rg, "stack"), *frame = jget(st, "frame");
    bool frame_differs = !wall_py_eq(avp_json_at(frame, 0), avp_json_at(frame, 1));
    if (avp_json_truthy(jget(cats, WALL_REGISTER)) || frame_differs) {
        const AvpJson *sw = jget(rg, "swapped");
        size_t ns = capn(jlen(sw), 8, verbose);
        AvpStr *items = AVP_NEWN(a, AvpStr, ns + 1);
        for (size_t i = 0; i < ns; i++) {
            const AvpJson *s = avp_json_at(sw, i);
            AvpStr x = jsz(a, s, "retail"), y = jsz(a, s, "compiled");
            items[i] = avp_pyfmt(a, "%S->%S%s", &x, &y, avp_json_truthy(jget(s, "callee_saved")) ? "*" : "");
        }
        AvpStr swt = avp_str_join(a, AVP_S(", "), items, ns);
        if (!swt.n) swt = AVP_S("identity");
        AvpStr cons = avp_json_truthy(jget(rg, "consistent"))
                          ? AVP_S("one consistent renaming")
                          : avp_str_concat(a, AVP_S("conflicts at "), join_cap(a, jget(rg, "conflicts"), 6, verbose, ", "));
        const AvpJson *nreg = jget(cats, WALL_REGISTER);
        avp_vec_push(&L, avp_pyfmt(a, "register/schedule: %d row(s); map %S (* callee-saved); %S",
                                   nreg ? wall_py_int(nreg) : 0, &swt, &cons));
        const AvpJson *mv = jget(rg, "moved");
        if (avp_json_truthy(mv)) {
            const AvpJson *mib = jget(rg, "moved_in_block");
            const char *where = (mib && mib->kind == AVP_JSON_BOOL) ? (mib->b ? " within blocks" : " (some across blocks)") : "";
            size_t nm = capn(jlen(mv), 3, verbose);
            AvpStr *ms = AVP_NEWN(a, AvpStr, nm + 1);
            for (size_t i = 0; i < nm; i++) {
                const AvpJson *m = avp_json_at(mv, i);
                AvpStr t = jsz(a, m, "text");
                ms[i] = avp_pyfmt(a, "%d->%d %S", wall_py_int(jget(m, "retail")), wall_py_int(jget(m, "compiled")), &t);
            }
            AvpStr mt = avp_str_join(a, AVP_S("; "), ms, nm);
            avp_vec_push(&L, avp_pyfmt(a, "  %d moved row(s)%s: %S", (i64)jlen(mv), where, &mt));
        }
        const AvpJson *ro = jget(st, "retail_only_slots"), *co = jget(st, "compiled_only_slots");
        if (frame_differs || avp_json_truthy(ro) || avp_json_truthy(co)) {
            AvpStr f0 = wall_py_str(a, avp_json_at(frame, 0)), f1 = wall_py_str(a, avp_json_at(frame, 1));
            AvpStr hs[2];
            const AvpJson *lists[2] = {ro, co};
            for (int k = 0; k < 2; k++) {
                size_t n = capn(jlen(lists[k]), 6, verbose);
                AvpStr *xs = AVP_NEWN(a, AvpStr, n + 1);
                for (size_t i = 0; i < n; i++) xs[i] = wall_py_hex(a, wall_py_int(avp_json_at(lists[k], i)));
                hs[k] = or_dash(avp_str_join(a, AVP_S(","), xs, n));
            }
            avp_vec_push(&L, avp_pyfmt(a, "  stack: frame %S/%S; retail-only slots %S; compiled-only slots %S", &f0, &f1,
                                       &hs[0], &hs[1]));
        }
    }
    const AvpJson *oth = jget(rev, WALL_OTHER);
    const AvpJson *unexp = jget(d, "unexplained");
    if (jlen(oth) || avp_json_truthy(unexp)) {
        avp_vec_push(&L, avp_pyfmt(a, "other: %d immediate row(s), %d unexplained row(s)", (i64)jlen(oth), wall_py_int(unexp)));
        for (size_t i = 0; i < capn(jlen(oth), 2, verbose); i++) {
            const AvpJson *e = avp_json_at(oth, i);
            AvpStr er = jsz(a, e, "retail"), rt = jsz(a, e, "retail_text"), ct = jsz(a, e, "compiled_text");
            avp_vec_push(&L, avp_pyfmt(a, "  row %S: %S | %S", &er, &rt, &ct));
        }
    }
    const AvpJson *ut = jget(d, "unresolved_targets");
    if (avp_json_truthy(ut))
        avp_vec_push(&L, avp_pyfmt(a, "note: %d relocation target(s) undecidable (codecmp)", wall_py_int(ut)));
    AvpStr pl = wall_proof_line(a, d);
    if (pl.p) avp_vec_push(&L, avp_u8_slice(pl, 0, 200));
    avp_vec_push(&L, jsz(a, d, "verdict"));
    return L;
}

AvpStr wall_phrase(const AvpJson *d) {
    AvpStr v = wall_jstr(d, "verdict");
    i64 k = avp_str_find(v, AVP_S(": "), 0, AVP_END);
    return k >= 0 ? avp_strn(v.p + k + 2, v.n - (size_t)k - 2) : v;
}

/* ---- the single form ------------------------------------------------------------------------------------------- */

static const ShsmRepo *repo_of(AvpArena *a, const AvpArgs *args) {
    AvpStr root = avp_args_str(args, "root");
    return shsm_repo_find(root.p ? avp_cz(a, root) : NULL);
}

static void data_members(ShsmOutput *out, AvpArena *a, const AvpJson *d) {
    for (size_t i = 0; avp_json_is_obj(d) && i < d->u.obj.n; i++)
        shsm_output_data(out, avp_cz(a, d->u.obj.v[i].key), d->u.obj.v[i].val);
}

static int cmd_one(void *ctx, AvpArgs *args) {
    AVP_UNUSED(ctx);
    AvpArena *a = avp_arena();
    const ShsmRepo *r = repo_of(a, args);
    const ShsmUnit *u = shsm_get_unit(r, avp_cz(a, avp_args_str(args, "unit")));
    AvpStr file = avp_args_str(args, "file");
    bool built = avp_args_bool(args, "built"), verbose = avp_args_bool(args, "verbose");
    WallObject o = file.p ? wall_object_for(a, r, u, false, avp_path_abspath(a, avp_cz(a, file)), "-file")
                          : wall_object_for(a, r, u, built, NULL, "");
    CcObjectComparison *oc = wall_compare_all(a, r, u, o.obj);
    AvpStr symbol = avp_args_str(args, "symbol");
    const CcCompareResult *res = wall_find(symbol, oc->results, oc->retail);
    if (!res) {
        AvpJson *d = avp_json_obj(a);
        avp_json_set(a, d, AVP_S("name"), avp_json_str(a, symbol));
        avp_json_set(a, d, AVP_S("retail_name"), avp_json_str(a, symbol));
        avp_json_set(a, d, AVP_S("exact"), avp_json_bool(false));
        avp_json_set(a, d, AVP_S("wall"), avp_json_strz(a, WALL_CALL));
        AvpJson *cl = avp_json_arr(a);
        avp_json_arr_push(a, cl, avp_json_strz(a, WALL_CALL));
        avp_json_set(a, d, AVP_S("classes"), cl);
        avp_json_set(a, d, AVP_S("verdict"),
                     avp_json_str(a, avp_pyfmt(a, "WALL call set: %S is not emitted by the compiled object (inlined "
                                                  "everywhere, or not defined)",
                                               &symbol)));
        ShsmOutput *out = shsm_output_open(args, -1, verbose ? 1 : 0, -1);
        shsm_output_line(out, wall_jstr(d, "verdict"));
        data_members(out, a, d);
        shsm_output_close(out);
        return SHSM_EXIT_FINDING;
    }
    const ShsmSymbolTable *symtab = shsm_load_symbols(r, true);
    AvpJson *d = wall_classify_result(a, u->name, res, r, symtab);
    if (wall_wants_proof(d, avp_args_bool(args, "prove"))) wall_apply_proof(a, d, wall_prove(a, r, u, res, NULL));
    bool exact = avp_json_truthy(jget(d, "exact"));
    ShsmOutput *out = shsm_output_open(args, 1000000, -1, -1);
    if (exact) {
        shsm_output_linez(out, "EXACT");
    } else {
        AvpStrVec lines = wall_render(a, d, verbose);
        shsm_output_lines(out, lines.v, lines.n);
        avp_vec_free(&lines);
    }
    shsm_output_data(out, "unit", avp_json_strz(a, u->name));
    data_members(out, a, d);
    shsm_output_close(out);
    return exact ? SHSM_EXIT_OK : SHSM_EXIT_FINDING;
}

/* ---- scan ------------------------------------------------------------------------------------------------------ */

typedef struct {
    const ShsmUnit *unit;
    AvpStr error;            /* .p NULL: none */
    CcObjectComparison *oc;  /* phase 1 */
    AvpJson *inputs;         /* NULL: None */
    AvpJson *functions;      /* the records, in the Python's order */
} UnitOut;

typedef struct {
    const ShsmRepo *r;
    bool built;
} Phase1;

/* scan_unit's compile and comparison (one bad unit must not sink the scan) */
static int phase1(void *ctx, size_t index, const void *item, void *result) {
    AVP_UNUSED(index);
    const Phase1 *p = ctx;
    UnitOut *o = *(UnitOut *const *)item;
    AVP_UNUSED(result);
    AvpArena *a = avp_arena();
    AvpTry t;
    if (AVP_TRY(&t)) {
        WallObject wo = wall_object_for(a, p->r, o->unit, p->built, NULL, "");
        o->inputs = wo.inputs;
        o->oc = wall_compare_all(a, p->r, o->unit, wo.obj);
        avp_try_end(&t);
    } else {
        AvpStr s = shsm_exc_str(a, t.code, t.msg);
        if (s.n) {
            AvpStrVec ls = avp_str_splitlines(s, false);
            o->error = avp_strn(avp_cz(a, ls.n ? ls.v[0] : AVP_S("")), ls.n ? ls.v[0].n : 0);
            avp_vec_free(&ls);
        } else {
            o->error = avp_str(avp_strdup(a, shsm_exc_class(t.code)));
        }
        o->oc = NULL;
    }
    return 0;
}

typedef struct {
    const ShsmRepo *r;
    const ShsmUnit *unit;
    const CcCompareResult *res;
    AvpJson *d;
    AvpJson *proof;
} ProofJob;

/* One unit's proofs: they run one after another on one CalleeLiveness (its memo makes answers depend on the order
 * of queries, as in the Python's worker); units run in parallel. */
typedef struct {
    ProofJob **jobs;
    size_t n;
} ProofGroup;

static int phase3(void *ctx, size_t index, const void *item, void *result) {
    AVP_UNUSED(ctx);
    AVP_UNUSED(index);
    AVP_UNUSED(result);
    const ProofGroup *g = *(ProofGroup *const *)item;
    if (!g->n) return 0;
    EqLive *live = eq_live_new(g->jobs[0]->r, 4);
    for (size_t k = 0; k < g->n; k++) {
        ProofJob *j = g->jobs[k];
        j->proof = wall_prove(avp_arena(), j->r, j->unit, j->res, live);
    }
    eq_live_free(live);
    return 0;
}

static i64 jobs_of(const AvpArgs *args) {
    i64 jobs = avp_args_is_none(args, "jobs") ? 0 : avp_args_int(args, "jobs");
    return jobs ? jobs : shsm_default_jobs();
}

static void run_map(i64 jobs, AvpMapFn fn, void *ctx, void **items, size_t n) {
    if (jobs <= 1 || n <= 1) {
        for (size_t i = 0; i < n; i++) fn(ctx, i, &items[i], NULL);
    } else {
        avp_pool_map(NULL, (int)AVP_MIN((i64)n, jobs), fn, ctx, items, n, sizeof(void *), NULL, 0);
    }
}

static AvpJson *json_list1(AvpArena *a, const char *s) {
    AvpJson *l = avp_json_arr(a);
    avp_json_arr_push(a, l, avp_json_strz(a, s));
    return l;
}

/* scan_unit's classification (sequential, in unit order) */
static void classify_unit(AvpArena *a, const ShsmRepo *r, UnitOut *o, const ShsmSymbolTable *symtab, bool prove_all,
                          AvpPtrVec *jobs) {
    o->functions = avp_json_arr(a);
    if (o->error.p) return;
    AvpSet *seen = avp_set_new();
    AvpMapIter it = {0};
    while (avp_map_next(o->oc->results, &it)) {
        const CcCompareResult *res = it.val->p;
        avp_set_add(seen, res->retail_name);
        if (res->exact) {
            AvpJson *f = avp_json_obj(a);
            avp_json_set(a, f, AVP_S("name"), avp_json_str(a, it.key));
            avp_json_set(a, f, AVP_S("retail_name"), avp_json_str(a, it.key));
            avp_json_set(a, f, AVP_S("exact"), avp_json_bool(true));
            avp_json_arr_push(a, o->functions, f);
            continue;
        }
        AvpJson *d = wall_classify_result(a, o->unit->name, res, r, symtab);
        avp_json_del(d, AVP_S("row_evidence"));
        if (wall_wants_proof(d, prove_all)) {
            ProofJob *j = AVP_NEW(a, ProofJob);
            j->r = r;
            j->unit = o->unit;
            j->res = res;
            j->d = d;
            avp_vec_push(jobs, j);
        }
        avp_json_arr_push(a, o->functions, d);
    }
    AvpMapIter jt = {0};
    while (avp_map_next(o->oc->retail, &jt)) {
        const CcFn *rfn = jt.val->p;
        if (avp_set_has(seen, jt.key) || !rfn->nrows) continue;
        AvpJson *f = avp_json_obj(a);
        avp_json_set(a, f, AVP_S("name"), avp_json_null());
        avp_json_set(a, f, AVP_S("retail_name"), avp_json_str(a, jt.key));
        avp_json_set(a, f, AVP_S("exact"), avp_json_bool(false));
        avp_json_set(a, f, AVP_S("wall"), avp_json_strz(a, WALL_CALL));
        avp_json_set(a, f, AVP_S("classes"), json_list1(a, WALL_CALL));
        avp_json_set(a, f, AVP_S("verdict"),
                     avp_json_strz(a, "WALL call set: not emitted by the compiled object (inlined everywhere, or not "
                                      "defined)"));
        AvpJson *rows = avp_json_obj(a);
        avp_json_set(a, rows, AVP_S("retail"), avp_json_int(a, (i64)rfn->nrows));
        avp_json_set(a, rows, AVP_S("compiled"), avp_json_int(a, 0));
        avp_json_set(a, rows, AVP_S("differing"), avp_json_int(a, (i64)rfn->nrows));
        avp_json_set(a, f, AVP_S("rows"), rows);
        avp_json_set(a, f, AVP_S("missing"), avp_json_bool(true));
        avp_json_arr_push(a, o->functions, f);
    }
    avp_set_free(seen);
}

/* _select_units(args, repo) */
static size_t select_units(AvpArena *a, const AvpArgs *args, const ShsmRepo *r, const ShsmUnit ***out) {
    const ShsmUnits *all = shsm_load_units(r);
    AvpPtrVec sel = {0};
    size_t nu = avp_args_is_none(args, "unit") ? 0 : avp_args_len(args, "unit");
    if (nu) {
        AvpStrVec us = avp_args_strs(args, "unit");
        for (size_t i = 0; i < us.n; i++) avp_vec_push(&sel, (void *)shsm_get_unit(r, avp_cz(a, us.v[i])));
        avp_vec_free(&us);
    } else {
        AvpStr status = avp_args_str(args, "status"), lib = avp_args_str(args, "lib");
        if (!status.n && !lib.n) status = AVP_S("NonMatching");
        for (size_t i = 0; i < all->n; i++) {
            const ShsmUnit *u = all->v[i];
            if (!u->source || !*u->source || shsm_unit_is_asm(u)) continue; /* assembled, not compiled */
            if (status.n && !(u->status && avp_str_eqz(status, u->status))) continue;
            if (lib.n && !(u->library && avp_str_eqz(lib, u->library))) continue;
            avp_vec_push(&sel, (void *)u);
        }
    }
    if (!sel.n) shsm_raise(SHSM_EXC_USAGE, "no units selected");
    const ShsmUnit **v = AVP_NEWN(a, const ShsmUnit *, sel.n + 1);
    for (size_t i = 0; i < sel.n; i++) v[i] = sel.v[i];
    size_t n = sel.n;
    avp_vec_free(&sel);
    *out = v;
    return n;
}

/* run_scan(args, repo, sel): the units' records with "percent"; *secs the time it took */
static UnitOut **run_scan(AvpArena *a, const AvpArgs *args, const ShsmRepo *r, const ShsmUnit **sel, size_t n,
                          double *secs) {
    double t0 = avp_time();
    bool built = avp_args_bool(args, "built");
    bool prove_all = avp_args_has(args, "prove") && avp_args_bool(args, "prove");
    i64 jobs = jobs_of(args);
    UnitOut **outs = AVP_NEWN(a, UnitOut *, n + 1);
    for (size_t i = 0; i < n; i++) {
        outs[i] = AVP_NEW(a, UnitOut);
        outs[i]->unit = sel[i];
    }
    Phase1 p1 = {r, built};
    run_map(jobs, phase1, &p1, (void **)outs, n);
    const ShsmSymbolTable *symtab = NULL;
    AvpPtrVec pjobs = {0};
    for (size_t i = 0; i < n; i++) {
        if (!outs[i]->error.p && !symtab) symtab = shsm_load_symbols(r, true);
        classify_unit(a, r, outs[i], symtab, prove_all, &pjobs);
    }
    AvpPtrVec groups = {0};
    for (size_t i = 0; i < pjobs.n;) {
        size_t k = i;
        while (k < pjobs.n && ((ProofJob *)pjobs.v[k])->unit == ((ProofJob *)pjobs.v[i])->unit) k++;
        ProofGroup *g = AVP_NEW(a, ProofGroup);
        g->jobs = (ProofJob **)&pjobs.v[i];
        g->n = k - i;
        avp_vec_push(&groups, g);
        i = k;
    }
    run_map(jobs, phase3, NULL, groups.v, groups.n);
    avp_vec_free(&groups);
    for (size_t i = 0; i < pjobs.n; i++) {
        ProofJob *j = pjobs.v[i];
        wall_apply_proof(a, j->d, j->proof);
    }
    avp_vec_free(&pjobs);
    ShsmReport *rep = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        rep = shsm_load_report(r);
        avp_try_end(&t);
    } else {
        if (!(shsm_exc_is_os(t.code) || shsm_exc_is_value(t.code))) avp_throw(t.code, "%s", t.msg);
        rep = NULL;
    }
    for (size_t i = 0; i < n; i++) {
        AvpJson *fs = outs[i]->functions;
        for (size_t k = 0; k < avp_json_len(fs); k++) {
            AvpJson *f = avp_json_at(fs, k);
            AvpJson *pct = avp_json_null();
            if (rep) {
                const ShsmFunctionReport *fr =
                    shsm_report_function(rep, avp_cz(a, wall_jstr(f, "retail_name")), outs[i]->unit->name);
                if (fr) pct = avp_json_float(a, avp_py_round(fr->fuzzy, 2));
            }
            avp_json_set(a, f, AVP_S("percent"), pct);
        }
    }
    *secs = avp_time() - t0;
    return outs;
}

static AvpStr counts_text(AvpArena *a, const AvpJson *counts) {
    size_t n = counts->u.obj.n;
    const AvpJsonMember **ms = AVP_NEWN(a, const AvpJsonMember *, n + 1);
    for (size_t i = 0; i < n; i++) ms[i] = &counts->u.obj.v[i];
    avp_sort(ms, n, sizeof *ms, str_cmp, NULL, false);
    AvpStr *items = AVP_NEWN(a, AvpStr, n + 1);
    for (size_t i = 0; i < n; i++) items[i] = avp_pyfmt(a, "%S %d", &ms[i]->key, ms[i]->val->u.i);
    return avp_str_join(a, AVP_S(", "), items, n);
}

static void count_add(AvpArena *a, AvpJson *counts, AvpStr key) {
    AvpJson *v = avp_json_get(counts, key);
    if (v) v->u.i++;
    else avp_json_set(a, counts, key, avp_json_int(a, 1));
}

static i64 count_of(const AvpJson *counts, const char *key) {
    const AvpJson *v = avp_json_getz(counts, key);
    return v ? v->u.i : 0;
}

typedef struct {
    int cls;
    i64 rows;
    const char *unit;
    AvpStr rname;
    AvpJson *f;
    AvpStr key;
} TableRow;

static int by_class_rows(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const TableRow *p = x, *q = y;
    if (p->cls != q->cls) return p->cls < q->cls ? -1 : 1;
    if (p->rows != q->rows) return p->rows < q->rows ? -1 : 1;
    return 0;
}

typedef struct {
    const char *unit;
    AvpJson *kind, *callee;
    i64 n;
} CallCount;

static int by_count_desc(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const CallCount *p = *(const CallCount *const *)x, *q = *(const CallCount *const *)y;
    return p->n > q->n ? -1 : p->n < q->n;
}

static int cmd_scan(void *ctx, AvpArgs *args) {
    AVP_UNUSED(ctx);
    AvpArena *a = avp_arena();
    const ShsmRepo *r = repo_of(a, args);
    const ShsmUnit **sel;
    size_t nsel = select_units(a, args, r, &sel);
    double secs;
    UnitOut **outs = run_scan(a, args, r, sel, nsel, &secs);
    AVP_VEC(TableRow) table = {0};
    AvpJson *counts = avp_json_obj(a), *errors = avp_json_arr(a);
    AvpMap *inl = avp_map_new();
    AvpPtrVec inl_list = {0};
    i64 proved = 0;
    for (size_t i = 0; i < nsel; i++) {
        UnitOut *o = outs[i];
        if (o->error.n)
            avp_json_arr_push(a, errors, avp_json_str(a, avp_pyfmt(a, "%s: %S", o->unit->name, &o->error)));
        for (size_t k = 0; k < avp_json_len(o->functions); k++) {
            AvpJson *f = avp_json_at(o->functions, k);
            if (avp_json_truthy(jget(f, "exact"))) continue;
            AvpStr verdict = wall_jstr(f, "verdict");
            const AvpJson *wall = jget(f, "wall");
            AvpStr key = avp_str_startswith(verdict, AVP_S("EQUIVALENT"))
                             ? AVP_S("EQUIVALENT-LOOKING")
                             : (avp_json_truthy(wall) ? wall_py_str(a, wall) : AVP_S("other"));
            if (avp_str_startswith(verdict, AVP_S("EQUIVALENT (proved"))) key = AVP_S("EQUIVALENT (proved)");
            const AvpJson *pv = jget(jget(f, "proof"), "verdict");
            if (avp_json_is_str(pv) && avp_str_eqz(pv->u.str, "EQUIVALENT")) proved++;
            if (avp_str_startswith(verdict, AVP_S("INCOMPLETE"))) key = AVP_S("incomplete body");
            else if (avp_str_find(verdict, AVP_S("MIXED "), 0, AVP_END) >= 0) key = avp_str_concat(a, key, AVP_S(" (mixed)"));
            count_add(a, counts, key);
            int cls = 9;
            for (int c = 0; c < 5; c++)
                if (avp_json_is_str(wall) && avp_str_eqz(wall->u.str, CLASS_ORDER[c])) cls = c;
            TableRow row = {cls, wall_py_int(jget(jget(f, "rows"), "retail")), o->unit->name,
                            wall_jstr(f, "retail_name"), f, key};
            avp_vec_push(&table, row);
            const AvpJson *calls = jget(f, "calls");
            const AvpJson *fi = avp_json_truthy(calls) ? jget(calls, "findings") : NULL;
            for (size_t m = 0; m < jlen(fi); m++) {
                const AvpJson *x = avp_json_at(fi, m);
                AvpJson *kind = jget(x, "kind"), *callee = jget(x, "callee");
                if (!avp_json_truthy(callee)) {
                    AvpStr xr = jsz(a, x, "retail"), xc = jsz(a, x, "compiled");
                    callee = avp_json_str(a, avp_pyfmt(a, "%S/%S", &xr, &xc));
                }
                AvpStr kr = wall_py_repr(a, kind), cr = wall_py_repr(a, callee), un = avp_str(o->unit->name);
                AvpStr parts[3] = {un, kr, cr};
                AvpStr ck = avp_str_join(a, AVP_S("\x1f"), parts, 3);
                bool created;
                AvpVal *v = avp_map_put(inl, ck, &created);
                if (created) {
                    CallCount *cc = AVP_NEW(a, CallCount);
                    cc->unit = o->unit->name;
                    cc->kind = kind;
                    cc->callee = callee;
                    v->p = cc;
                    avp_vec_push(&inl_list, cc);
                }
                ((CallCount *)v->p)->n++;
            }
        }
    }
    avp_sort(table.v, table.n, sizeof *table.v, by_class_rows, NULL, false);
    CallCount **ranked = AVP_NEWN(a, CallCount *, inl_list.n + 1);
    for (size_t i = 0; i < inl_list.n; i++) ranked[i] = inl_list.v[i];
    avp_sort(ranked, inl_list.n, sizeof *ranked, by_count_desc, NULL, false);
    const char *path = wall_walls_path(a, r);
    AvpJson *doc = avp_json_obj(a);
    avp_json_set(a, doc, AVP_S("version"), avp_json_int(a, WALLS_VERSION));
    avp_json_set(a, doc, AVP_S("seconds"), avp_json_float(a, avp_py_round(secs, 1)));
    avp_json_set(a, doc, AVP_S("built"), avp_json_bool(avp_args_bool(args, "built")));
    AvpJson *units = avp_json_arr(a), *inputs = avp_json_obj(a);
    for (size_t i = 0; i < nsel; i++) {
        avp_json_arr_push(a, units, avp_json_strz(a, outs[i]->unit->name));
        avp_json_set(a, inputs, avp_str(outs[i]->unit->name), outs[i]->inputs ? outs[i]->inputs : avp_json_null());
    }
    avp_json_set(a, doc, AVP_S("units"), units);
    avp_json_set(a, doc, AVP_S("inputs"), inputs);
    avp_json_set(a, doc, AVP_S("counts"), counts);
    avp_json_set(a, doc, AVP_S("proved"), avp_json_int(a, proved));
    avp_json_set(a, doc, AVP_S("prove_all"), avp_json_bool(avp_args_bool(args, "prove")));
    avp_json_set(a, doc, AVP_S("errors"), errors);
    AvpJson *summary = avp_json_arr(a);
    for (size_t i = 0; i < inl_list.n; i++) {
        AvpJson *e = avp_json_obj(a);
        avp_json_set(a, e, AVP_S("unit"), avp_json_strz(a, ranked[i]->unit));
        avp_json_set(a, e, AVP_S("kind"), ranked[i]->kind ? ranked[i]->kind : avp_json_null());
        avp_json_set(a, e, AVP_S("callee"), ranked[i]->callee);
        avp_json_set(a, e, AVP_S("functions"), avp_json_int(a, ranked[i]->n));
        avp_json_arr_push(a, summary, e);
    }
    avp_json_set(a, doc, AVP_S("call_summary"), summary);
    AvpJson *functions = avp_json_arr(a);
    for (size_t i = 0; i < table.n; i++) {
        AvpJson *f = match_json_copy(a, table.v[i].f);
        avp_json_set(a, f, AVP_S("unit"), avp_json_strz(a, table.v[i].unit));
        avp_json_arr_push(a, functions, f);
    }
    avp_json_set(a, doc, AVP_S("functions"), functions);
    AvpJsonDumpOpts dopts = shsm_json_opts();
    AvpStr text = avp_json_dumps(a, doc, &dopts);
    if (!avp_makedirs(avp_cz(a, avp_path_dirname(path)), true)) shsm_raise_io();
    const char *tmp = avp_sprintf(a, "%s.tmp", path);
    if (!avp_write_text(tmp, text, AVP_TM_UTF8)) shsm_raise_io();
    if (!avp_replace(tmp, path)) shsm_raise_io();
    ShsmOutput *out = shsm_output_open(args, -1, -1, -1);
    AvpStr ct = counts_text(a, counts);
    shsm_output_line(out, avp_pyfmt(a, "%d unit(s), %d non-exact function(s) in %.1f s; %S; proved equivalent "
                                       "(eqcheck): %d",
                                    (i64)nsel, (i64)table.n, secs, &ct, proved));
    for (size_t i = 0; i < avp_json_len(errors); i++) {
        AvpStr e = avp_json_at(errors, i)->u.str;
        shsm_output_line(out, avp_pyfmt(a, "error: %S", &e));
    }
    shsm_output_line(out, avp_pyfmt(a, "%-40s %-44s %7s %5s  %-18s %s", "unit", "function", "pct", "diff", "class",
                                    "evidence"));
    for (size_t i = 0; i < table.n; i++) {
        const TableRow *t = &table.v[i];
        AvpStr up = avp_u8_slice(avp_str(shsm_unit_path(a, t->unit)), -40, AVP_END);
        AvpStr rn = avp_u8_slice(t->rname, 0, 44);
        const AvpJson *pct = jget(t->f, "percent");
        AvpStr ps = avp_json_is_null(pct) ? AVP_S("-") : avp_pyfmt(a, "%.1f", avp_json_f64(pct, 0.0));
        AvpStr diff = wall_py_str(a, jget(jget(t->f, "rows"), "differing"));
        AvpStr key = avp_u8_slice(t->key, 0, 18), ph = avp_u8_slice(wall_phrase(t->f), 0, 110);
        shsm_output_line(out, avp_pyfmt(a, "%-40S %-44S %7S %5S  %-18S %S", &up, &rn, &ps, &diff, &key, &ph));
    }
    if (inl_list.n) {
        shsm_output_linez(out, "call-set summary (unit, reading, callee: functions):");
        for (size_t i = 0; i < inl_list.n && i < 15; i++) {
            AvpStr up = avp_str(shsm_unit_path(a, ranked[i]->unit)), k = wall_py_str(a, ranked[i]->kind),
                   c = wall_py_str(a, ranked[i]->callee);
            shsm_output_line(out, avp_pyfmt(a, "  %S: %S %S in %d function(s)", &up, &k, &c, ranked[i]->n));
        }
    }
    shsm_output_line(out, avp_pyfmt(a, "wrote %s", shsm_repo_rel(a, r, path)));
    data_members(out, a, doc);
    shsm_output_close(out);
    avp_map_free(inl);
    avp_vec_free(&inl_list);
    bool any = table.n > 0;
    avp_vec_free(&table);
    return any ? SHSM_EXIT_FINDING : SHSM_EXIT_OK;
}

/* ---- notes ----------------------------------------------------------------------------------------------------- */

static AvpJson *str_list(AvpArena *a, const char *const *v, size_t n) {
    AvpJson *l = avp_json_arr(a);
    for (size_t i = 0; i < n; i++) avp_json_arr_push(a, l, avp_json_strz(a, v[i]));
    return l;
}

static int cmd_notes(void *ctx, AvpArgs *args) {
    AVP_UNUSED(ctx);
    AvpArena *a = avp_arena();
    const ShsmRepo *r = repo_of(a, args);
    const ShsmUnit **sel;
    size_t nsel = select_units(a, args, r, &sel);
    const ShsmSymbolTable *st = shsm_load_symbols(r, true);
    double secs;
    UnitOut **outs = run_scan(a, args, r, sel, nsel, &secs);
    AvpMap *by_unit = avp_map_new();
    for (size_t i = 0; i < nsel; i++) avp_map_put(by_unit, avp_str(outs[i]->unit->name), NULL)->p = outs[i];
    const ShsmRe *sentence = shsm_re("(?<=\\.)\\s", 0);
    AvpJson *rows = avp_json_arr(a), *counts = avp_json_obj(a);
    for (size_t i = 0; i < nsel; i++) {
        const ShsmUnit *u = sel[i];
        const AvpVal *ov = avp_map_get(by_unit, avp_str(u->name));
        UnitOut *o = ov ? ov->p : NULL;
        AvpMap *funcs = avp_map_new();
        AvpPtrVec fvals = {0}; /* funcs.values() order */
        for (size_t k = 0; o && k < avp_json_len(o->functions); k++) {
            AvpJson *f = avp_json_at(o->functions, k);
            bool created;
            AvpVal *v = avp_map_put(funcs, wall_jstr(f, "retail_name"), &created);
            if (created) {
                v->i = (i64)fvals.n;
                avp_vec_push(&fvals, f);
            } else {
                fvals.v[v->i] = f;
            }
        }
        WallNote *notes;
        size_t nn = wall_find_notes(a, avp_path_join(a, r->root, u->source), &notes);
        for (size_t k = 0; k < nn; k++) {
            const WallNote *w = &notes[k];
            ShsmReMatch m;
            AvpStr first = shsm_re_search(sentence, w->text, 0, AVP_END, &m)
                               ? avp_strn(w->text.p, (size_t)shsm_re_start(&m, 0))
                               : w->text;
            if (w->has_addr) {
                i64 kw = wall_note_keyword(first);
                first = kw >= 0 ? avp_strn(first.p, (size_t)kw) : AVP_S("");
            }
            AvpPtrVec targets = {0}; /* AvpStr* */
            AvpMapIter it = {0};
            while (avp_map_next(funcs, &it)) {
                AvpStr esc = shsm_re_escape(a, wall_base_name(it.key));
                const char *pat = avp_cz(a, avp_pyfmt(a, "\\b%S\\b", &esc));
                if (shsm_re_search(shsm_re(pat, 0), first, 0, AVP_END, NULL)) {
                    AvpStr *t = AVP_NEW(a, AvpStr);
                    *t = avp_strn(avp_cz(a, it.key), it.key.n);
                    avp_vec_push(&targets, t);
                }
            }
            if (!targets.n && w->has_addr) {
                AvpPtrVec hit = {0};
                shsm_sym_at(st, w->addr, ".text", &hit);
                for (size_t h = 0; h < hit.n && !targets.n; h++) {
                    const ShsmSymbol *s = hit.v[h];
                    if (s->type && strcmp(s->type, "function") == 0) {
                        AvpStr *t = AVP_NEW(a, AvpStr);
                        *t = avp_str(s->name);
                        avp_vec_push(&targets, t);
                    }
                }
                avp_vec_free(&hit);
            }
            const char *stated[5];
            size_t nstated = wall_note_classes(w->text, stated);
            bool rule_note = wall_is_rule_note(w->text);
            if (!targets.n) {
                i64 bad = 0;
                for (size_t f = 0; f < fvals.n; f++)
                    if (!avp_json_truthy(jget(fvals.v[f], "exact"))) bad++;
                AvpStr status = avp_pyfmt(a, "unit note: %d non-exact function(s) now", bad);
                if (!bad) {
                    status = AVP_S("STALE unit note: every function is exact now");
                    count_add(a, counts, AVP_S("stale"));
                } else {
                    count_add(a, counts, AVP_S("unit"));
                }
                AvpJson *row = avp_json_obj(a);
                avp_json_set(a, row, AVP_S("unit"), avp_json_strz(a, u->name));
                avp_json_set(a, row, AVP_S("line"), avp_json_int(a, w->line));
                avp_json_set(a, row, AVP_S("function"), avp_json_null());
                avp_json_set(a, row, AVP_S("note"), avp_json_str(a, w->text));
                avp_json_set(a, row, AVP_S("stated"), str_list(a, stated, nstated));
                avp_json_set(a, row, AVP_S("status"), avp_json_str(a, status));
                avp_json_arr_push(a, rows, row);
                continue;
            }
            for (size_t ti = 0; ti < targets.n; ti++) {
                AvpStr t = *(AvpStr *)targets.v[ti];
                const AvpVal *fv = avp_map_get(funcs, t);
                AvpJson *f = fv ? fvals.v[fv->i] : NULL;
                AvpStr status;
                const char *key;
                if (!f) {
                    status = AVP_S("function not found in compiled object");
                    key = "unknown";
                } else if (avp_json_truthy(jget(f, "exact"))) {
                    if (rule_note) {
                        status = AVP_S("holds: rule note (MATCH-PENDING), bytes exact");
                        key = "holds";
                    } else {
                        status = AVP_S("STALE: function is exact now");
                        key = "stale";
                    }
                } else {
                    const AvpJson *found = jget(f, "classes");
                    AvpStr wall = wall_py_str(a, jget(f, "wall"));
                    bool common = false;
                    for (size_t s = 0; s < nstated && !common; s++)
                        for (size_t c = 0; c < jlen(found) && !common; c++)
                            common = wall_py_eq(avp_json_at(found, c), &(AvpJson){.kind = AVP_JSON_STRING,
                                                                                  .u.str = avp_str(stated[s])});
                    if (!nstated || rule_note) {
                        status = avp_pyfmt(a, "now %S (note names no class)", &wall);
                        key = "holds";
                    } else if (common) {
                        status = avp_pyfmt(a, "holds: %S", &wall);
                        key = "holds";
                    } else {
                        AvpStr *ss = AVP_NEWN(a, AvpStr, nstated + 1);
                        for (size_t s = 0; s < nstated; s++) ss[s] = avp_str(stated[s]);
                        AvpStr sj = avp_str_join(a, AVP_S("/"), ss, nstated);
                        AvpStr fj = join_cap(a, found, 0, true, "/");
                        if (!fj.n) fj = AVP_S("nothing");
                        status = avp_pyfmt(a, "DISAGREES: note says %S, classifier finds %S", &sj, &fj);
                        key = "disagrees";
                    }
                }
                count_add(a, counts, avp_str(key));
                AvpJson *row = avp_json_obj(a);
                avp_json_set(a, row, AVP_S("unit"), avp_json_strz(a, u->name));
                avp_json_set(a, row, AVP_S("line"), avp_json_int(a, w->line));
                avp_json_set(a, row, AVP_S("function"), avp_json_str(a, t));
                avp_json_set(a, row, AVP_S("note"), avp_json_str(a, w->text));
                avp_json_set(a, row, AVP_S("stated"), str_list(a, stated, nstated));
                avp_json_set(a, row, AVP_S("status"), avp_json_str(a, status));
                const AvpJson *verdict = f ? jget(f, "verdict") : NULL;
                avp_json_set(a, row, AVP_S("verdict"), verdict ? (AvpJson *)verdict : avp_json_null());
                avp_json_arr_push(a, rows, row);
            }
            avp_vec_free(&targets);
        }
        avp_map_free(funcs);
        avp_vec_free(&fvals);
    }
    ShsmOutput *out = shsm_output_open(args, -1, -1, -1);
    AvpStr ct = counts_text(a, counts);
    shsm_output_line(out, avp_pyfmt(a, "%d note(s) in %d unit(s) (%.1f s): %S", (i64)avp_json_len(rows), (i64)nsel, secs, &ct));
    for (size_t i = 0; i < avp_json_len(rows); i++) {
        const AvpJson *row = avp_json_at(rows, i);
        const ShsmUnit *u = shsm_get_unit(r, avp_cz(a, wall_jstr(row, "unit")));
        const AvpJson *fn = jget(row, "function");
        AvpStr fs = avp_json_is_str(fn) && fn->u.str.n ? fn->u.str : AVP_S("(unit)");
        AvpStr status = wall_jstr(row, "status"), note = avp_u8_slice(wall_jstr(row, "note"), 0, 120);
        shsm_output_line(out, avp_pyfmt(a, "%s:%d %S: %S", u->source, wall_py_int(jget(row, "line")), &fs, &status));
        shsm_output_line(out, avp_pyfmt(a, "    note: %S", &note));
    }
    shsm_output_data(out, "notes", rows);
    shsm_output_data(out, "counts", counts);
    shsm_output_close(out);
    avp_map_free(by_unit);
    return (count_of(counts, "stale") || count_of(counts, "disagrees")) ? SHSM_EXIT_FINDING : SHSM_EXIT_OK;
}

/* ---- command line ---------------------------------------------------------------------------------------------- */

static bool limit_conv(void *ctx, AvpArena *a, AvpStr in, AvpArgValue *out, AvpStr *msg) {
    AVP_UNUSED(ctx);
    i64 n = 0;
    if (!shsm_limit_type(a, in, &n, msg)) return false;
    out->kind = AVP_V_INT;
    out->i = n;
    return true;
}

/* cli.parser(description) with an epilog */
static AvpArgParser *shared_parser(const char *prog, const char *description, const char *epilog) {
    AvpArgParser *p = avp_args_parser(NULL, &(AvpArgParserDesc){.prog = prog, .description = description,
                                                               .epilog = epilog});
    avp_arg(p, &(AvpArgDesc){.names = "--root", .metavar = "DIR",
                             .help = "checkout to work on (default: the one around the cwd)"});
    avp_arg(p, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE,
                             .help = "print one JSON document instead of text"});
    avp_arg(p, &(AvpArgDesc){.names = "--all", .action = AVP_ARG_STORE_TRUE, .help = "do not cap the output"});
    avp_arg(p, &(AvpArgDesc){.names = "--limit", .type = AVP_T_CUSTOM, .conv = limit_conv, .type_name = "limit_type",
                             .dflt = AVP_DEF_INT(SHSM_DEFAULT_LIMIT), .metavar = "N",
                             .help = "cap text output at N lines (default 40; 0 or 'all': no cap)"});
    avp_arg(p, &(AvpArgDesc){.names = "--debug", .action = AVP_ARG_STORE_TRUE, .help = "print tracebacks"});
    return p;
}

int wall_main(int argc, char **argv) {
    if (argc > 1 && (strcmp(argv[1], "scan") == 0 || strcmp(argv[1], "notes") == 0)) {
        bool scan = strcmp(argv[1], "scan") == 0;
        AvpArena *a = avp_arena();
        const char *desc = avp_sprintf(a, "Classify the non-exact functions of units (%s). %s", argv[1], WALL_ORDER_NOTE);
        AvpArgParser *ap = shared_parser(scan ? "wall.exe scan" : "wall.exe notes", desc, NULL);
        avp_arg(ap, &(AvpArgDesc){.names = "--unit", .action = AVP_ARG_APPEND, .help = "unit (repeatable)"});
        avp_arg(ap, &(AvpArgDesc){.names = "--lib", .help = "configure.py library name"});
        avp_arg(ap, &(AvpArgDesc){.names = "--status", .help = "unit status (default NonMatching)"});
        avp_arg(ap, &(AvpArgDesc){.names = "--built", .action = AVP_ARG_STORE_TRUE,
                                  .help = "use the last build's objects (read in place)"});
        avp_arg(ap, &(AvpArgDesc){.names = "-j --jobs", .type = AVP_T_INT, .dflt = AVP_DEF_NONE});
        if (scan)
            avp_arg(ap, &(AvpArgDesc){.names = "--prove", .action = AVP_ARG_STORE_TRUE,
                                      .help = "run eqcheck.exe's prover on every non-exact function (default: "
                                              "only on EQUIVALENT-LOOKING ones)"});
        return shsm_cli_main(ap, argc - 1, argv + 1, scan ? cmd_scan : cmd_notes, NULL);
    }
    AvpArgParser *ap = shared_parser(
        "wall.exe", "Classify why one function does not match retail. " WALL_ORDER_NOTE,
        "keyword forms: wall.exe {scan,notes} ...  scan classifies every non-exact function of the chosen units into "
        "build/walls.json; notes re-checks the wall notes in the units' source comments against a fresh compile "
        "(stale, class disagrees, holds).  `wall.exe scan --help` and `wall.exe notes --help` show their options.");
    avp_arg(ap, &(AvpArgDesc){.names = "unit"});
    avp_arg(ap, &(AvpArgDesc){.names = "symbol"});
    AvpArgMutex *g = avp_args_mutex(ap, false);
    avp_arg_mutex(g, &(AvpArgDesc){.names = "--built", .action = AVP_ARG_STORE_TRUE,
                                   .help = "use the last build's object (read in place)"});
    avp_arg_mutex(g, &(AvpArgDesc){.names = "--file", .help = "compile this variant source with the unit's flags"});
    avp_arg(ap, &(AvpArgDesc){.names = "--verbose", .action = AVP_ARG_STORE_TRUE, .help = "lift the caps"});
    avp_arg(ap, &(AvpArgDesc){.names = "--prove", .action = AVP_ARG_STORE_TRUE,
                              .help = "run eqcheck.exe's prover even when the verdict is not EQUIVALENT-LOOKING"});
    return shsm_cli_main(ap, argc, argv, cmd_one, NULL);
}
