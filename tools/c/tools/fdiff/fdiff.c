/* fdiff.exe logic: the functions of python tools/mwcc_trace/fdiff.py. */
#include "fdiff.h"
#include "cc_codecmp.h"

#define EXACT (100.0 - 1e-6)

static const char DESCRIPTION[] =
    "Side-by-side retail/compiled listing of one function, from objdiff-cli JSON, with relocation targets.";

static const char EPILOG[] =
    "Usage: build/tools/fdiff.exe <unit> <symbol> [--all] [--no-build] [--no-targets]\n"
    "  <unit>        e.g. MSL_C/MSL_Common/Src/mem_funcs (with or without \"main/\")\n"
    "  --all         print every row (default: only rows that differ, with 2 rows context)\n"
    "  --no-build    do not rebuild the unit's object first (compare the object as it is)\n"
    "  --no-targets  skip the relocation-target check (codecmp)\n"
    "\n"
    "Rebuilds the unit's object first (tools/bin/ninja.exe when present, else ninja on PATH), then prints the\n"
    "match percent and the rows as \"retail | compiled\": '*' marks a row objdiff reports as different, 'T' a row\n"
    "whose instruction text matches but whose relocation or branch target differs (codecmp, which\n"
    "resolves every target to a retail address); the target detail follows the row.\n"
    "\n"
    "Exit code: 0 when the function is exact (objdiff 100% and no target mismatch), 1 when it differs (or the\n"
    "compiled object lacks it), 2 on an environment error (a tool missing, the build or objdiff failing, objdiff\n"
    "printing no JSON, the symbol not in the retail unit, the target check unable to run).\n";

AvpArgParser *fdiff_parser(void) {
    AvpArgParser *ap = avp_args_parser(NULL, &(AvpArgParserDesc){.prog = "fdiff.exe",
                                                                 .description = DESCRIPTION,
                                                                 .epilog = EPILOG,
                                                                 .formatter = AVP_FORMAT_RAW_DESCRIPTION});
    avp_arg(ap, &(AvpArgDesc){.names = "unit"});
    avp_arg(ap, &(AvpArgDesc){.names = "symbol"});
    avp_arg(ap, &(AvpArgDesc){.names = "--all", .action = AVP_ARG_STORE_TRUE, .help = "print every row"});
    avp_arg(ap, &(AvpArgDesc){.names = "--no-build", .action = AVP_ARG_STORE_TRUE, .help = "skip the ninja step"});
    avp_arg(ap, &(AvpArgDesc){.names = "--no-targets", .action = AVP_ARG_STORE_TRUE,
                              .help = "skip the relocation-target check"});
    return ap;
}

/* ---- small helpers -------------------------------------------------------------------------------------------- */

/* s[-n:] counted in code points. */
static AvpStr tail_cp(AvpStr s, size_t n) {
    size_t i = s.n, cps = 0;
    while (i > 0 && cps < n) {
        i--;
        while (i > 0 && ((u8)s.p[i] & 0xC0) == 0x80) i--;
        cps++;
    }
    return avp_strn(s.p + i, s.n - i);
}

/* The Python ROOT (os.path.abspath form: backslashes). */
static const char *root_of(const FdiffEnv *env) {
    return env->root ? env->root : avp_path_normpath(avp_arena(), avp_root());
}

static char *do_which(const FdiffEnv *env, AvpArena *a, const char *name) {
    if (env->which) return env->which(env->ctx, a, name);
    return avp_which(a, name, NULL, NULL);
}

static bool do_run(const FdiffEnv *env, AvpArena *a, const char *const *argv, const char *cwd, i64 *rc, AvpStr *out,
                   AvpStr *err) {
    if (env->run) return env->run(env->ctx, a, argv, cwd, rc, out, err);
    AvpProcSpec spec = {0};
    spec.argv = argv;
    spec.cwd = cwd;
    spec.out = AVP_IO_CAPTURE;
    spec.err = AVP_IO_CAPTURE;
    spec.text = true;
    spec.errors = AVP_ERRORS_REPLACE;
    spec.arena = a;
    AvpProcResult r = avp_run(&spec);
    if (r.status == AVP_PROC_START_FAILED) {
        *out = AVP_S("");
        *err = avp_str(r.message);
        return false;
    }
    *rc = r.returncode;
    *out = r.out;
    *err = r.err;
    return true;
}

/* tool(name): tools/bin/<name>.exe or tools/bin/<name> when present, else <name> on PATH. */
static const char *tool(const FdiffEnv *env, AvpArena *a, const char *name) {
    const char *root = root_of(env);
    const char *cands[] = {avp_sprintf(a, "%s.exe", name), name};
    for (size_t i = 0; i < 2; i++) {
        const char *p = avp_join(a, root, "tools", "bin", cands[i]);
        if (avp_isfile(p)) return p;
    }
    char *p = do_which(env, a, name);
    if (!p) avp_throw(FDIFF_ENV_ERROR, "%s not found: neither tools/bin/%s.exe nor %s on PATH", name, name, name);
    return p;
}

/* object_path(unit): build/R5WEA4/src/<unit>.o when src/<unit>.{c,cp,cpp} exists, else NULL. */
static const char *object_path(const FdiffEnv *env, AvpArena *a, const char *unit) {
    static const char *const exts[] = {".c", ".cp", ".cpp"};
    for (size_t i = 0; i < 3; i++) {
        const char *p = avp_join(a, root_of(env), "src", avp_sprintf(a, "%s%s", unit, exts[i]));
        if (avp_exists(p)) return avp_sprintf(a, "build/R5WEA4/src/%s.o", unit);
    }
    return NULL;
}

/* A process that could not start: the Python died with a FileNotFoundError traceback (exit 1). */
static AVP_NORETURN void start_failed(const char *exe, AvpStr why) {
    (void)exe;
    avp_throw(FDIFF_CRASH, "FileNotFoundError: %.*s", AVP_SF(why));
}

static void build_unit(const FdiffEnv *env, AvpArena *a, AvpBuf *out, const char *unit) {
    if (env->build_unit) {
        env->build_unit(env->ctx, a, out, unit);
        return;
    }
    const char *obj = object_path(env, a, unit);
    if (!obj) avp_throw(FDIFF_ENV_ERROR, "no source file src/%s.{c,cp,cpp}: nothing to build", unit);
    const char *ninja = tool(env, a, "ninja");
    const char *argv[] = {ninja, obj, NULL};
    i64 rc = 0;
    AvpStr so, se;
    if (!do_run(env, a, argv, root_of(env), &rc, &so, &se)) start_failed(ninja, se);
    if (rc) {
        avp_buf_put(out, tail_cp(so, 3000));
        avp_throw(FDIFF_ENV_ERROR, "ninja %s failed (exit %lld)", obj, (long long)rc);
    }
}

AvpJson *fdiff_parse_json(AvpArena *a, AvpStr text) {
    AvpJsonError jerr;
    AvpJson *doc = avp_json_parse(a, text, &jerr);
    if (!doc) {
        AvpStr why = avp_json_error_str(a, &jerr);
        avp_throw(FDIFF_ENV_ERROR, "objdiff-cli printed no JSON (%.*s)", AVP_SF(why));
    }
    if (!avp_json_is_obj(doc)) avp_throw(FDIFF_ENV_ERROR, "objdiff-cli JSON is not an object");
    return doc;
}

static AvpJson *objdiff_json(const FdiffEnv *env, AvpArena *a, const char *unit, const char *sym) {
    if (env->objdiff_json) return env->objdiff_json(env->ctx, a, unit, sym);
    const char *exe = tool(env, a, "objdiff-cli");
    const char *argv[] = {exe, "diff", "-p", ".", "-u", avp_sprintf(a, "main/%s", unit), "-o", "-", sym, NULL};
    i64 rc = 0;
    AvpStr so, se;
    if (!do_run(env, a, argv, root_of(env), &rc, &so, &se)) start_failed(exe, se);
    if (rc) {
        AvpStr why = tail_cp(avp_str_strip(se.n ? se : so), 500);
        avp_throw(FDIFF_ENV_ERROR, "objdiff-cli diff failed (exit %lld): %.*s", (long long)rc, AVP_SF(why));
    }
    return fdiff_parse_json(a, so);
}

const AvpJson *fdiff_find_symbol(const AvpJson *doc, const char *side, AvpStr sym) {
    const AvpJson *s = avp_json_getz(doc, side);
    const AvpJson *syms = avp_json_truthy(s) ? avp_json_getz(s, "symbols") : NULL;
    if (!avp_json_truthy(syms) || !avp_json_is_arr(syms)) return NULL;
    for (size_t i = 0; i < avp_json_len(syms); i++) {
        const AvpJson *e = avp_json_at(syms, i);
        const AvpJson *n = avp_json_getz(e, "name");
        if (avp_json_is_str(n) && avp_str_eq(avp_json_str_or(n, AVP_S("")), sym)) return e;
    }
    return NULL;
}

static bool target_result(const FdiffEnv *env, AvpArena *a, const char *unit, const char *sym, FdiffTargets *out) {
    if (env->target_result) return env->target_result(env->ctx, a, unit, sym, out);
    const char *root = root_of(env);
    const char *asm_path = avp_join(a, root, "build", "R5WEA4", "asm", avp_sprintf(a, "%s.s", unit));
    const char *obj = object_path(env, a, unit);
    if (!obj || !avp_exists(avp_path_join(a, root, obj)))
        avp_throw(FDIFF_ENV_ERROR, "target check: no compiled object %s", obj ? obj : "(no source)");
    AvpStr err = {0};
    AvpMap *retail = cc_load_asm(a, asm_path, &err);
    if (!retail)
        avp_throw(FDIFF_ENV_ERROR, "target check could not run (%s: %.*s); --no-targets skips it", avp_io_err_class(),
                  AVP_SF(err));
    AvpMap *compiled = cc_load_object(a, avp_path_join(a, root, obj), NULL, &err);
    if (!compiled)
        avp_throw(FDIFF_ENV_ERROR, "target check could not run (CalledProcessError: %.*s); --no-targets skips it",
                  AVP_SF(err));
    CcContext *ctx = cc_context_new(cc_project_new(root), avp_str(unit));
    AvpStr s = avp_str(sym);
    AvpVal *rv = avp_map_get(retail, s);
    if (!rv) avp_throw(FDIFF_ENV_ERROR, "target check: %s not in %s", sym, asm_path);
    AvpVal *cv = avp_map_get(compiled, s);
    if (!cv) return false;
    CcFnResult *r = cc_compare_function(a, rv->p, cv->p, ctx, true);
    out->n = r->targets.n;
    out->v = AVP_NEWN(a, FdiffTarget, r->targets.n ? r->targets.n : 1);
    for (size_t i = 0; i < r->targets.n; i++) {
        const CcTargetDiff *t = &r->targets.v[i];
        out->v[i] = (FdiffTarget){t->i, t->j, avp_str(t->kind), t->detail};
    }
    out->checked = r->checked_targets;
    out->nunresolved = r->unresolved.n;
    return true;
}

/* ---- rows ----------------------------------------------------------------------------------------------------- */

static const AvpJson *instructions(const AvpJson *side) {
    if (!avp_json_truthy(side)) return NULL;
    const AvpJson *v = avp_json_getz(side, "instructions");
    return avp_json_truthy(v) && avp_json_is_arr(v) ? v : NULL;
}

FdiffRows fdiff_rows_of(AvpArena *a, const AvpJson *left, const AvpJson *right) {
    const AvpJson *li = instructions(left), *ri = instructions(right);
    size_t nl = li ? avp_json_len(li) : 0, nr = ri ? avp_json_len(ri) : 0, n = nl > nr ? nl : nr;
    FdiffRows rows = {AVP_NEWN(a, FdiffRow, n ? n : 1), n};
    i64 ni = 0, nj = 0;
    for (size_t k = 0; k < n; k++) {
        const AvpJson *ea = k < nl ? avp_json_at(li, k) : NULL;
        const AvpJson *eb = k < nr ? avp_json_at(ri, k) : NULL;
        const AvpJson *ia = ea ? avp_json_getz(ea, "instruction") : NULL;
        const AvpJson *ib = eb ? avp_json_getz(eb, "instruction") : NULL;
        bool has_a = ia && !avp_json_is_null(ia), has_b = ib && !avp_json_is_null(ib);
        FdiffRow *r = &rows.v[k];
        r->k = k;
        r->i = has_a ? ni++ : -1;
        r->j = has_b ? nj++ : -1;
        r->diff = avp_json_truthy(ea ? avp_json_getz(ea, "diff_kind") : NULL) ||
                  avp_json_truthy(eb ? avp_json_getz(eb, "diff_kind") : NULL);
        r->fa = avp_json_truthy(ia) ? avp_json_get_str(ia, "formatted", AVP_S("")) : AVP_S("");
        r->fb = avp_json_truthy(ib) ? avp_json_get_str(ib, "formatted", AVP_S("")) : AVP_S("");
    }
    return rows;
}

FdiffMarks fdiff_target_marks(AvpArena *a, const FdiffRows *rows, const FdiffTargets *res) {
    FdiffMarks m = {AVP_NEWN(a, AvpStrVec, rows->n ? rows->n : 1), {0}};
    for (size_t t = 0; res && t < res->n; t++) {
        const FdiffTarget *td = &res->v[t];
        i64 k = -1, ki = -1;
        for (size_t r = 0; r < rows->n; r++) {
            const FdiffRow *row = &rows->v[r];
            if (row->i == (i64)td->i && row->j >= 0 && row->j == (i64)td->j) k = (i64)row->k;
            if (row->i == (i64)td->i) ki = (i64)row->k;
        }
        if (k < 0) k = ki;
        AvpStr line = avp_pyfmt(a, "%S: %S", &td->kind, &td->detail);
        if (k < 0) avp_vec_push(&m.loose, avp_pyfmt(a, "retail row %d / compiled row %d: %S", td->i, td->j, &line));
        else avp_vec_push(&m.marks[k], line);
    }
    return m;
}

AvpStrVec fdiff_render(AvpArena *a, const FdiffRows *rows, const FdiffMarks *marks, bool show_all) {
    AvpStrVec out = {0};
    size_t n = rows->n;
    bool *keep = AVP_NEWN(a, bool, n + 1);
    for (size_t k = 0; k < n; k++) {
        if (!(show_all || rows->v[k].diff || marks->marks[k].n)) continue;
        for (i64 x = (i64)k - 2; x <= (i64)k + 2; x++)
            if (x >= 0 && x < (i64)n) keep[x] = true;
    }
    i64 last = -2;
    for (size_t k = 0; k < n; k++) {
        if (!keep[k]) continue;
        const FdiffRow *r = &rows->v[k];
        if ((i64)k != last + 1 && (last >= 0 || k > 0)) avp_vec_push(&out, AVP_S("   ..."));
        const char *flag = r->diff ? "*" : (marks->marks[k].n ? "T" : " ");
        avp_vec_push(&out, avp_pyfmt(a, "%3d %s %-34S | %S", k, flag, &r->fa, &r->fb));
        for (size_t i = 0; i < marks->marks[k].n; i++)
            avp_vec_push(&out, avp_pyfmt(a, "        -> %S", &marks->marks[k].v[i]));
        last = (i64)k;
    }
    return out;
}

/* float(pct) with the Python's fallback to 0.0. */
static double percent(const AvpJson *v) {
    if (!v) return 0.0;
    switch (v->kind) {
    case AVP_JSON_INT:
    case AVP_JSON_FLOAT: return avp_json_f64(v, 0.0);
    case AVP_JSON_BOOL: return v->b ? 1.0 : 0.0;
    case AVP_JSON_STRING: {
        double d;
        return avp_parse_float(avp_str_strip(v->u.str), &d) ? d : 0.0;
    }
    default: return 0.0;
    }
}

static void line(AvpBuf *out, AvpStr s) {
    avp_buf_put(out, s);
    avp_buf_putc(out, '\n');
}

static int run(const FdiffEnv *env, AvpArena *a, const AvpArgs *args, AvpBuf *out) {
    AvpStr u = avp_args_str(args, "unit");
    const char *sym = avp_args_str(args, "symbol").p;
    AvpStr ssym = avp_str(sym);
    if (avp_str_startswith(u, AVP_S("main/"))) u = avp_strn(u.p + 5, u.n - 5);
    const char *unit = avp_str_dup(a, u).p;
    if (!avp_args_bool(args, "no_build")) build_unit(env, a, out, unit);
    AvpJson *doc = objdiff_json(env, a, unit, sym);
    const AvpJson *left = fdiff_find_symbol(doc, "left", ssym), *right = fdiff_find_symbol(doc, "right", ssym);
    if (!left) avp_throw(FDIFF_ENV_ERROR, "%s is not a symbol of the retail unit main/%s", sym, unit);
    if (!right) {
        line(out, avp_pyfmt(a, "%s: not in the compiled object (the source does not define it, or does not compile)",
                            sym));
        return 1;
    }
    double pct = percent(avp_json_getz(right, "match_percent"));
    FdiffRows rows = fdiff_rows_of(a, left, right);
    FdiffMarks marks = {AVP_NEWN(a, AvpStrVec, rows.n ? rows.n : 1), {0}};
    FdiffTargets res = {0};
    bool have = false;
    if (!avp_args_bool(args, "no_targets")) {
        if (!target_result(env, a, unit, sym, &res)) {
            line(out, avp_pyfmt(a, "%s: not in the compiled object", sym));
            return 1;
        }
        have = true;
        marks = fdiff_target_marks(a, &rows, &res);
    }
    AvpBuf head;
    avp_buf_init(&head, a);
    avp_buf_pyfmt(&head, "%s: %.2f%%", sym, pct);
    if (have) {
        avp_buf_pyfmt(&head, ", %d target mismatch(es) of %d checked", res.n, res.checked);
        if (res.nunresolved) avp_buf_pyfmt(&head, ", %d unresolved", res.nunresolved);
    } else {
        avp_buf_putz(&head, " (targets not checked)");
    }
    line(out, avp_buf_str(&head));
    AvpStrVec lines = fdiff_render(a, &rows, &marks, avp_args_bool(args, "all"));
    for (size_t i = 0; i < lines.n; i++) line(out, lines.v[i]);
    for (size_t i = 0; i < marks.loose.n; i++) line(out, avp_pyfmt(a, "   T %S", &marks.loose.v[i]));
    bool exact = pct >= EXACT && res.n == 0;
    line(out, exact ? AVP_S("exact") : AVP_S("differs"));
    avp_vec_free(&lines);
    return exact ? 0 : 1;
}

int fdiff_run(const FdiffEnv *env, AvpArena *a, const AvpArgs *args, AvpBuf *out) {
    AvpTry t;
    int rc;
    if (AVP_TRY(&t)) {
        rc = run(env, a, args, out);
        avp_try_end(&t);
        return rc;
    }
    if (t.code == FDIFF_CRASH) {
        AvpStr so = avp_buf_str(out);
        fwrite(so.p, 1, so.n, stdout);
        avp_buf_clear(out);
        fflush(stdout);
        fputs(t.msg, stderr);
        fputc('\n', stderr);
        return 1;
    }
    if (t.code != FDIFF_ENV_ERROR) avp_throw(t.code, "%s", t.msg);
    line(out, avp_pyfmt(a, "error: %s", t.msg));
    return 2;
}
