/* check_rules: the self-test of tools/rules/test_check_rules.py in CTest (stage 5). One test per Python test (named
 * <Class>_<test>, listed in selftest_tests.h), each replaying what gen_selftest.py recorded while the Python ran it:
 *   run   a Checker built from the recorded sources and tables, run through the library (rl_checker_run), its
 *         --dump-final text compared byte for byte with the Python's, plus the probe the test's assertions read
 *         (finding texts, blocking flags, is_cpp, functions, variables, include closure, resolve_include);
 *   main  the command line: the recorded tree is rewritten under a scratch root, build/tools/check_rules.exe is run
 *         with the recorded arguments, and its exit code, stdout and stderr are compared with the Python's;
 *   call  the helper called directly (parse_units, parse_include_dirs, Allow.from_markdown, demangle_member,
 *         template_name, lex, uses_cpp_syntax, parse_cpp_classes), its result compared with the Python's.
 * Regenerate the data with gen_selftest.py; the test fails on a case it cannot read. */
#include "avp/test.h"
#include "rules_check.h"
#include "rules_classes.h"
#include "rules_cli.h"
#include <stdlib.h>
#include <string.h>

#define DATA_PATH "tools/c/tools/check_rules/tests/selftest_cases.json"

static const AvpJson *g_doc;

static const AvpJson *doc(void) {
    if (g_doc) return g_doc;
    AvpArena *a = avp_arena_new(0);
    AvpBytes bytes = {0};
    if (!avp_read_all(a, DATA_PATH, &bytes)) {
        fprintf(stderr, "selftest: cannot read %s (run the tests from the repository root)\n", DATA_PATH);
        return NULL;
    }
    AvpJsonError err;
    g_doc = avp_json_parse(a, (AvpStr){(const char *)bytes.v, bytes.n}, &err);
    if (!g_doc) fprintf(stderr, "selftest: %s is not valid JSON\n", DATA_PATH);
    return g_doc;
}

/* ---- helpers --------------------------------------------------------------------------------------------------- */
static AvpStr jstr(const AvpJson *o, const char *key) { return avp_json_get_str(o, key, (AvpStr){0}); }
static AvpStr sv(const AvpJson *v) { return avp_json_str_or(v, (AvpStr){0}); }
static bool is_none(const AvpJson *v) { return avp_json_is_null(v); }

/* Compares two texts; on a difference reports the first differing line. */
static void expect_text(const char *id, const char *what, AvpStr got, AvpStr want) {
    if (avp_str_eq(got, want)) return;
    size_t i = 0;
    while (i < got.n && i < want.n && got.p[i] == want.p[i]) i++;
    size_t ls = i;
    while (ls > 0 && want.p[ls - 1] != '\n') ls--;
    size_t le = i;
    while (le < want.n && want.p[le] != '\n') le++;
    size_t ge = ls;
    while (ge < got.n && got.p[ge] != '\n') ge++;
    avp_test__fail(__FILE__, __LINE__, "%s: %s differs (got %zu bytes, want %zu), first difference at byte %zu\n  want: %.*s\n  got:  %.*s",
                   id, what, got.n, want.n, i, (int)(le - ls > 400 ? 400 : le - ls), want.p + ls,
                   (int)((ge > ls ? ge - ls : 0) > 400 ? 400 : (ge > ls ? ge - ls : 0)), got.p + ls);
}

static AvpStr pair_key(AvpArena *a, AvpStr x, AvpStr y) {
    return avp_str_concat(a, avp_str_concat(a, x, AVP_S("\x01")), y);
}

/* ---- run ------------------------------------------------------------------------------------------------------- */
static void fill_set(AvpArena *a, AvpSet **set, const AvpJson *arr, bool pairs) {
    *set = avp_set_new();
    for (size_t i = 0; i < avp_json_len(arr); i++) {
        const AvpJson *e = avp_json_at(arr, i);
        avp_set_add(*set, pairs ? pair_key(a, sv(avp_json_at(e, 0)), sv(avp_json_at(e, 1))) : sv(e));
    }
}

static RlChecker *build_checker(AvpArena *a, const AvpJson *rec) {
    AvpStrVec paths = {0}, texts = {0};
    const AvpJson *srcs = avp_json_getz(rec, "sources");
    for (size_t i = 0; i < avp_json_len(srcs); i++) {
        avp_vec_push(&paths, sv(avp_json_at(avp_json_at(srcs, i), 0)));
        avp_vec_push(&texts, sv(avp_json_at(avp_json_at(srcs, i), 1)));
    }
    RlConfig cfg = {0};
    cfg.unit_status = avp_map_new();
    cfg.include_map = avp_map_new();
    const AvpJson *us = avp_json_getz(rec, "unit_status");
    for (size_t i = 0; i < avp_json_len(us); i++) {
        const AvpJsonMember *m = avp_json_member(us, i);
        AvpStr *v = AVP_NEW(a, AvpStr);
        *v = sv(m->val);
        avp_map_put(cfg.unit_status, m->key, NULL)->p = v;
    }
    const AvpJson *im = avp_json_getz(rec, "include_map");
    for (size_t i = 0; i < avp_json_len(im); i++) {
        const AvpJsonMember *m = avp_json_member(im, i);
        AvpStrVec *dirs = AVP_NEW(a, AvpStrVec);
        for (size_t k = 0; k < avp_json_len(m->val); k++) avp_vec_push(dirs, sv(avp_json_at(m->val, k)));
        avp_map_put(cfg.include_map, m->key, NULL)->p = dirs;
    }
    fill_set(a, &cfg.original_asm, avp_json_getz(rec, "original_asm"), true);
    fill_set(a, &cfg.asm_helpers, avp_json_getz(rec, "asm_helpers"), true);
    fill_set(a, &cfg.rulings, avp_json_getz(rec, "rulings"), true);
    fill_set(a, &cfg.pending, avp_json_getz(rec, "pending"), false);
    cfg.allow_source = jstr(rec, "allow_source");
    RlChecker *ck = rl_checker_from(a, &paths, &texts, &cfg);
    const AvpJson *scan = avp_json_getz(rec, "scan");
    for (size_t i = 0; i < avp_json_len(scan); i++) avp_vec_push(&ck->scan, sv(avp_json_at(scan, i)));
    const AvpJson *classes = avp_json_getz(rec, "classes");
    if (!is_none(classes)) {
        AvpStr text = avp_json_dumps(a, classes, &AVP_JSON_COMPACT);
        AvpJsonError err;
        ck->classes_data = avp_json_parse(a, text, &err);
    }
    const AvpJson *note = avp_json_getz(rec, "classes_note");
    if (!is_none(note)) ck->classes_note = sv(note);
    return ck;
}

static void probe_check(const char *id, RlChecker *ck, const AvpJson *probe, const AvpJson *rec) {
    AvpArena *a = ck->a;
    const AvpJson *texts = avp_json_getz(probe, "texts"), *blocking = avp_json_getz(probe, "blocking");
    REQUIRE(avp_json_len(texts) == ck->findings.n);
    for (size_t i = 0; i < ck->findings.n; i++) {
        expect_text(id, "finding text", rl_finding_text(a, &ck->findings.v[i]), sv(avp_json_at(texts, i)));
        CHECK_MSG(rl_blocking(ck, &ck->findings.v[i]) == avp_json_bool_or(avp_json_at(blocking, i), false),
                  "%s: blocking(finding %zu)", id, i);
    }
    const AvpJson *paths = avp_json_getz(probe, "paths");
    for (size_t i = 0; i < avp_json_len(paths); i++) {
        const AvpJsonMember *m = avp_json_member(paths, i);
        RlParsed *P = rl_checker_parse(ck, m->key);
        REQUIRE(P != NULL);
        CHECK_MSG(P->is_cpp == avp_json_get_bool(m->val, "is_cpp", false), "%s: %.*s is_cpp", id, AVP_SF(m->key));
        const AvpJson *funcs = avp_json_getz(m->val, "funcs"), *vars = avp_json_getz(m->val, "vars");
        CHECK_MSG(avp_json_len(funcs) == P->p.funcs.n, "%s: %.*s has %zu functions, want %zu", id, AVP_SF(m->key),
                  P->p.funcs.n, avp_json_len(funcs));
        for (size_t k = 0; k < avp_json_len(funcs) && k < P->p.funcs.n; k++) {
            const AvpJson *f = avp_json_at(funcs, k);
            CHECK_MSG(avp_str_eq(P->p.funcs.v[k].name, sv(avp_json_at(f, 0))) &&
                          P->p.funcs.v[k].is_static == avp_json_bool_or(avp_json_at(f, 1), false),
                      "%s: %.*s function %zu", id, AVP_SF(m->key), k);
        }
        CHECK_MSG(avp_json_len(vars) == P->p.vars.n, "%s: %.*s has %zu variables, want %zu", id, AVP_SF(m->key),
                  P->p.vars.n, avp_json_len(vars));
        for (size_t k = 0; k < avp_json_len(vars) && k < P->p.vars.n; k++) {
            const AvpJson *v = avp_json_at(vars, k);
            CHECK_MSG(avp_str_eq(P->p.vars.v[k].name, sv(avp_json_at(v, 0))) &&
                          P->p.vars.v[k].is_static == avp_json_bool_or(avp_json_at(v, 1), false) &&
                          P->p.vars.v[k].is_extern == avp_json_bool_or(avp_json_at(v, 2), false),
                      "%s: %.*s variable %zu", id, AVP_SF(m->key), k);
        }
        const AvpJson *clo = avp_json_getz(m->val, "closure");
        const AvpStrVec *got = rl_checker_closure(ck, m->key);
        CHECK_MSG(got->n == avp_json_len(clo), "%s: closure(%.*s) has %zu paths, want %zu", id, AVP_SF(m->key),
                  got->n, avp_json_len(clo));
        for (size_t k = 0; k < avp_json_len(clo) && k < got->n; k++)
            CHECK_MSG(avp_str_eq(got->v[k], sv(avp_json_at(clo, k))), "%s: closure(%.*s)[%zu] = %.*s", id,
                      AVP_SF(m->key), k, AVP_SF(got->v[k]));
    }
    const AvpJson *res = avp_json_getz(rec, "resolve");
    for (size_t i = 0; i < avp_json_len(res); i++) {
        const AvpJson *r = avp_json_at(res, i);
        AvpStr got = rl_resolve_include(ck, sv(avp_json_at(r, 0)), sv(avp_json_at(r, 1)), false, (AvpStr){0});
        const AvpJson *want = avp_json_at(r, 2);
        if (is_none(want)) CHECK_MSG(got.p == NULL, "%s: resolve_include(%.*s) should be None", id, AVP_SF(sv(avp_json_at(r, 1))));
        else CHECK_MSG(avp_str_eq(got, sv(want)), "%s: resolve_include(%.*s, %.*s) = %.*s, want %.*s", id,
                       AVP_SF(sv(avp_json_at(r, 0))), AVP_SF(sv(avp_json_at(r, 1))), AVP_SF(got), AVP_SF(sv(want)));
    }
}

static void do_run(const char *id, const AvpJson *rec) {
    AvpArena *a = avp_arena_new(0);
    RlChecker *ck = build_checker(a, rec);
    REQUIRE(ck != NULL);
    rl_checker_run(ck);
    CHECK_MSG(!ck->failed && !ck->error, "%s: the run failed (%s)", id, ck->error ? ck->error : "lexer");
    if (ck->failed || ck->error) return;
    AvpBuf b;
    avp_buf_init(&b, a);
    rl_dump_final(ck, &b);
    expect_text(id, "the final dump", avp_buf_str(&b), jstr(rec, "expect"));
    probe_check(id, ck, avp_json_getz(rec, "probe"), rec);
}

/* ---- main ------------------------------------------------------------------------------------------------------ */
static AvpStr norm_root(AvpArena *a, AvpStr s, const char *root) {
    AvpStr r1 = avp_str(root);
    AvpStr r2 = avp_str_replace(a, r1, AVP_S("\\"), AVP_S("/"), -1);   /* the slash form */
    AvpStr r3 = avp_str_replace(a, r1, AVP_S("\\"), AVP_S("\\\\"), -1); /* as --json escapes it */
    s = avp_str_replace(a, s, r3, AVP_S("@ROOT@"), -1);
    s = avp_str_replace(a, s, r1, AVP_S("@ROOT@"), -1);
    return avp_str_replace(a, s, r2, AVP_S("@ROOT@"), -1);
}

/* The stamp classes.py records, made on the tree as it stands now (asm count, bytes, newest mtime; symbols file). */
static bool make_stamp(AvpArena *a, const char *asm_dir, const char *sym, i64 key[5]) {
    i64 files = 0, size = 0, newest = 0;
    AvpWalk *w = avp_walk_open(asm_dir, true);
    if (w) {
        AvpWalkItem it;
        while (avp_walk_next(w, &it)) {
            for (size_t k = 0; k < it.files.n; k++) {
                if (!avp_str_endswith(it.files.v[k], AVP_S(".s"))) continue;
                AvpStat st;
                if (!avp_stat(avp_path_join(a, avp_cz(a, it.dirpath), avp_cz(a, it.files.v[k])), &st)) {
                    avp_walk_close(w);
                    return false;
                }
                files++;
                size += st.size;
                if (st.mtime_ns > newest) newest = st.mtime_ns;
            }
        }
        avp_walk_close(w);
    }
    AvpStat st;
    if (!avp_stat(sym, &st)) return false;
    key[0] = files;
    key[1] = size;
    key[2] = newest;
    key[3] = st.size;
    key[4] = st.mtime_ns;
    return true;
}

static void put_file(AvpArena *a, const char *root, AvpStr rel, AvpStr text) {
    char *path = avp_path_join(a, root, avp_cz(a, rel));
    avp_makedirs(avp_cz(a, avp_path_dirname(path)), true);
    REQUIRE(avp_write_file(path, text.p ? text.p : "", text.n));
}

static void do_main(const char *id, const AvpJson *rec) {
    AvpArena *a = avp_arena_new(0);
    char *root = avp_mkdtemp(a, "chkrules", NULL, NULL);
    REQUIRE(root != NULL);
    const AvpJson *files = avp_json_getz(rec, "files"), *stamps = avp_json_getz(rec, "stamps");
    for (size_t i = 0; i < avp_json_len(files); i++) {
        const AvpJson *f = avp_json_at(files, i);
        CHECK_MSG(avp_json_has(f, AVP_S("text")), "%s: a file recorded as binary", id);
        AvpStr rel = jstr(f, "path");
        put_file(a, root, rel, jstr(f, "text"));
    }
    /* classes.json with its input stamp: made on the tree, then moved the way the Python's differed */
    for (size_t k = 0; k < avp_json_len(stamps); k++) {
        const AvpJson *s = avp_json_at(stamps, k);
        AvpStr asm_rel = jstr(s, "asm"), sym_rel = jstr(s, "symbols");
        char *asm_dir = avp_path_join(a, root, avp_cz(a, asm_rel)), *sym = avp_path_join(a, root, avp_cz(a, sym_rel));
        i64 key[5] = {0, 0, 0, 0, 0};
        if (!avp_json_get_bool(s, "gone", false)) CHECK_MSG(make_stamp(a, asm_dir, sym, key), "%s: cannot stamp", id);
        if (avp_json_get_bool(s, "asm_differs", false)) key[2] += 1;
        if (avp_json_get_bool(s, "symbols_differs", false)) key[4] += 1;
        AvpJson *d = avp_json_parse(a, avp_json_dumps(a, avp_json_getz(s, "doc"), &AVP_JSON_COMPACT), &(AvpJsonError){0});
        AvpJson *as = avp_json_obj(a), *sy = avp_json_obj(a), *in = avp_json_obj(a);
        avp_json_set(a, as, AVP_S("dir"), avp_json_strz(a, asm_dir));
        avp_json_set(a, as, AVP_S("files"), avp_json_int(a, key[0]));
        avp_json_set(a, as, AVP_S("bytes"), avp_json_int(a, key[1]));
        avp_json_set(a, as, AVP_S("mtime_ns"), avp_json_int(a, key[2]));
        avp_json_set(a, sy, AVP_S("path"), avp_json_strz(a, sym));
        avp_json_set(a, sy, AVP_S("bytes"), avp_json_int(a, key[3]));
        avp_json_set(a, sy, AVP_S("mtime_ns"), avp_json_int(a, key[4]));
        avp_json_set(a, in, AVP_S("asm"), as);
        avp_json_set(a, in, AVP_S("symbols"), sy);
        avp_json_set(a, d, AVP_S("inputs"), in);
        put_file(a, root, jstr(s, "file"), avp_json_dumps(a, d, &AVP_JSON_COMPACT));
    }
    /* the command */
    const AvpJson *argv = avp_json_getz(rec, "argv");
    const char *exe = getenv("SHSM_CHECK_RULES_EXE");
    if (!exe) exe = avp_path_join(a, avp_path_join(a, avp_exe_dir(), ".."), "check_rules.exe");
    REQUIRE(avp_isfile(exe));
    const char **av = AVP_NEWN(a, const char *, avp_json_len(argv) + 2);
    size_t n = 0;
    av[n++] = exe;
    for (size_t i = 0; i < avp_json_len(argv); i++)
        av[n++] = avp_cz(a, avp_str_replace(a, sv(avp_json_at(argv, i)), AVP_S("@ROOT@"), avp_str(root), -1));
    av[n] = NULL;
    AvpProcResult r = avp_run_text(a, NULL, av);
    REQUIRE(r.status == AVP_PROC_OK);
    CHECK_MSG(r.returncode == avp_json_get_i64(rec, "code", -2), "%s: exit code %lld, want %lld", id,
              (long long)r.returncode, (long long)avp_json_get_i64(rec, "code", -2));
    expect_text(id, "stdout", norm_root(a, r.out, root), jstr(rec, "stdout"));
    expect_text(id, "stderr", norm_root(a, r.err, root), jstr(rec, "stderr"));
    avp_tempdir_cleanup(root);
}

/* ---- direct calls ---------------------------------------------------------------------------------------------- */
static void check_str_set(const char *id, const char *what, const AvpSet *got, const AvpJson *want, bool pairs,
                          AvpArena *a) {
    CHECK_MSG(avp_set_len(got) == avp_json_len(want), "%s: %s has %zu members, want %zu", id, what, avp_set_len(got),
              avp_json_len(want));
    for (size_t i = 0; i < avp_json_len(want); i++) {
        const AvpJson *e = avp_json_at(want, i);
        AvpStr key = pairs ? pair_key(a, sv(avp_json_at(e, 0)), sv(avp_json_at(e, 1))) : sv(e);
        CHECK_MSG(avp_set_has(got, key), "%s: %s lacks %.*s", id, what, AVP_SF(sv(avp_json_at(e, 0))));
    }
}

static void do_call(const char *id, const AvpJson *rec) {
    AvpArena *a = avp_arena_new(0);
    AvpStr fn = jstr(rec, "fn"), arg = jstr(rec, "arg");
    const AvpJson *want = avp_json_getz(rec, "result");
    if (avp_str_eqz(fn, "parse_units")) {
        RlConfig cfg = {0};
        rl_parse_units(a, &cfg, arg);
        size_t have = cfg.unit_status ? avp_map_len(cfg.unit_status) : 0;
        CHECK_MSG(have == avp_json_len(want), "%s: parse_units gave %zu units, want %zu", id, have, avp_json_len(want));
        for (size_t i = 0; i < avp_json_len(want); i++) {
            const AvpJsonMember *m = avp_json_member(want, i);
            AvpVal *v = cfg.unit_status ? avp_map_get(cfg.unit_status, m->key) : NULL;
            CHECK_MSG(v && avp_str_eq(*(AvpStr *)v->p, sv(m->val)), "%s: unit %.*s", id, AVP_SF(m->key));
        }
    } else if (avp_str_eqz(fn, "parse_include_dirs")) {
        RlConfig cfg = {0};
        rl_parse_include_dirs(a, &cfg, arg);
        size_t have = cfg.include_map ? avp_map_len(cfg.include_map) : 0;
        CHECK_MSG(have == avp_json_len(want), "%s: parse_include_dirs gave %zu units, want %zu", id, have, avp_json_len(want));
        for (size_t i = 0; i < avp_json_len(want); i++) {
            const AvpJsonMember *m = avp_json_member(want, i);
            AvpVal *v = cfg.include_map ? avp_map_get(cfg.include_map, m->key) : NULL;
            REQUIRE(v != NULL);
            const AvpStrVec *dirs = v->p;
            CHECK_MSG(dirs->n == avp_json_len(m->val), "%s: %.*s has %zu directories", id, AVP_SF(m->key), dirs->n);
            for (size_t k = 0; k < dirs->n && k < avp_json_len(m->val); k++)
                CHECK_MSG(avp_str_eq(dirs->v[k], sv(avp_json_at(m->val, k))), "%s: %.*s directory %zu", id,
                          AVP_SF(m->key), k);
        }
    } else if (avp_str_eqz(fn, "allow_from_markdown")) {
        RlConfig cfg = {0};
        rl_allow_from_markdown(a, &cfg, arg, AVP_S(""));
        check_str_set(id, "original_asm", cfg.original_asm, avp_json_getz(want, "original_asm"), true, a);
        check_str_set(id, "asm_helpers", cfg.asm_helpers, avp_json_getz(want, "asm_helpers"), true, a);
        check_str_set(id, "rulings", cfg.rulings, avp_json_getz(want, "rulings"), true, a);
        check_str_set(id, "pending", cfg.pending, avp_json_getz(want, "pending"), false, a);
    } else if (avp_str_eqz(fn, "demangle_member")) {
        AvpStr cls = {0}, name = {0};
        rl_demangle_member(a, arg, &cls, &name);
        const AvpJson *wc = avp_json_at(want, 0), *wn = avp_json_at(want, 1);
        CHECK_MSG(is_none(wc) ? cls.p == NULL : avp_str_eq(cls, sv(wc)), "%s: class of %.*s", id, AVP_SF(arg));
        CHECK_MSG(is_none(wn) ? name.p == NULL : avp_str_eq(name, sv(wn)), "%s: name of %.*s", id, AVP_SF(arg));
    } else if (avp_str_eqz(fn, "template_name")) {
        AvpStr got = rl_template_name(arg);
        CHECK_MSG(is_none(want) ? got.p == NULL : avp_str_eq(got, sv(want)), "%s: template_name(%.*s)", id, AVP_SF(arg));
    } else if (avp_str_eqz(fn, "lex")) {
        RlLexed lx = rl_lex(a, arg, 1, true);
        REQUIRE(lx.ok);
        const AvpJson *wt = avp_json_getz(want, "toks"), *wc = avp_json_getz(want, "comments"),
                      *wd = avp_json_getz(want, "directives");
        CHECK_MSG(lx.toks.n == avp_json_len(wt), "%s: %zu tokens, want %zu", id, lx.toks.n, avp_json_len(wt));
        for (size_t i = 0; i < lx.toks.n && i < avp_json_len(wt); i++) {
            const AvpJson *t = avp_json_at(wt, i);
            CHECK_MSG(avp_str_eq(lx.toks.v[i].t, sv(avp_json_at(t, 0))) &&
                          avp_str_eqz(sv(avp_json_at(t, 1)), rl_kind_names[lx.toks.v[i].k]) &&
                          lx.toks.v[i].line == avp_json_i64(avp_json_at(t, 2), -1),
                      "%s: token %zu (%.*s)", id, i, AVP_SF(lx.toks.v[i].t));
        }
        CHECK_MSG(lx.comments.n == avp_json_len(wc), "%s: %zu comments, want %zu", id, lx.comments.n, avp_json_len(wc));
        for (size_t i = 0; i < lx.comments.n && i < avp_json_len(wc); i++) {
            const AvpJson *c = avp_json_at(wc, i);
            CHECK_MSG(lx.comments.v[i].line == avp_json_i64(avp_json_at(c, 0), -1) &&
                          lx.comments.v[i].end_line == avp_json_i64(avp_json_at(c, 1), -1) &&
                          avp_str_eq(lx.comments.v[i].text, sv(avp_json_at(c, 2))) &&
                          lx.comments.v[i].trailing == avp_json_bool_or(avp_json_at(c, 3), false),
                      "%s: comment %zu", id, i);
        }
        CHECK_MSG(lx.directives.n == avp_json_len(wd), "%s: %zu directives, want %zu", id, lx.directives.n,
                  avp_json_len(wd));
        for (size_t i = 0; i < lx.directives.n && i < avp_json_len(wd); i++) {
            const AvpJson *d = avp_json_at(wd, i);
            CHECK_MSG(avp_str_eq(lx.directives.v[i].name, sv(avp_json_at(d, 0))) &&
                          lx.directives.v[i].line == avp_json_i64(avp_json_at(d, 1), -1) &&
                          lx.directives.v[i].end_line == avp_json_i64(avp_json_at(d, 2), -1) &&
                          avp_str_eq(lx.directives.v[i].rest, sv(avp_json_at(d, 3))),
                      "%s: directive %zu", id, i);
        }
    } else if (avp_str_eqz(fn, "uses_cpp_syntax")) {
        RlLexed lx = rl_lex(a, arg, 1, true);
        REQUIRE(lx.ok);
        CHECK_MSG(rl_uses_cpp_syntax(lx.toks.v, lx.toks.n) == avp_json_bool_or(want, false), "%s: uses_cpp_syntax(%.*s)",
                  id, AVP_SF(arg));
    } else if (avp_str_eqz(fn, "parse_cpp_classes")) {
        const AvpJson *args = avp_json_getz(rec, "arg");
        RlParsed *P = rl_parse(a, sv(avp_json_at(args, 0)), sv(avp_json_at(args, 1)), NULL);
        const RlSrcClasses *cl = rl_cpp_classes(P);
        CHECK_MSG(cl->n == avp_json_len(want), "%s: %zu classes, want %zu", id, cl->n, avp_json_len(want));
        for (size_t i = 0; i < cl->n && i < avp_json_len(want); i++) {
            const AvpJson *wc = avp_json_at(want, i), *wm = avp_json_getz(wc, "methods");
            const RlSrcClass *c = &cl->v[i];
            CHECK_MSG(avp_str_eq(c->name, jstr(wc, "name")), "%s: class %zu name", id, i);
            CHECK_MSG(c->methods.n == avp_json_len(wm), "%s: %zu methods, want %zu", id, c->methods.n, avp_json_len(wm));
            for (size_t k = 0; k < c->methods.n && k < avp_json_len(wm); k++) {
                const AvpJson *m = avp_json_at(wm, k), *wp = avp_json_getz(m, "params");
                const RlSrcMethod *sm = &c->methods.v[k];
                CHECK_MSG(avp_str_eq(sm->name, jstr(m, "name")) && sm->sig.const_ == avp_json_get_bool(m, "const", false) &&
                              sm->sig.np == avp_json_len(wp),
                          "%s: method %.*s", id, AVP_SF(sm->name));
                for (u32 p = 0; p < sm->sig.np && p < avp_json_len(wp); p++)
                    CHECK_MSG(avp_str_eq(sm->sig.params[p], sv(avp_json_at(wp, p))), "%s: %.*s parameter %u", id,
                              AVP_SF(sm->name), p);
            }
        }
    } else {
        avp_test__fail(__FILE__, __LINE__, "%s: unknown recorded call %.*s", id, AVP_SF(fn));
    }
}

/* ---- the mapped tests -------------------------------------------------------------------------------------------- */
static void do_real_configure(const char *id) {
    AvpArena *a = avp_arena_new(0);
    AvpStr text;
    if (!avp_isfile("configure.py")) return; /* the Python skips: no configure.py */
    REQUIRE(rl_read_source(a, "configure.py", &text));
    RlConfig cfg = {0};
    rl_parse_units(a, &cfg, text);
    rl_parse_include_dirs(a, &cfg, text);
    REQUIRE(cfg.unit_status && cfg.include_map);
    CHECK_MSG(avp_map_len(cfg.unit_status) == avp_map_len(cfg.include_map), "%s: %zu units, %zu with directories", id,
              avp_map_len(cfg.unit_status), avp_map_len(cfg.include_map));
    AvpMapIter it = {0};
    while (avp_map_next(cfg.include_map, &it)) {
        CHECK_MSG(avp_map_has(cfg.unit_status, it.key), "%s: %.*s has directories but no status", id, AVP_SF(it.key));
        const AvpStrVec *dirs = it.val->p;
        CHECK_MSG(dirs->n > 0 && avp_str_eqz(dirs->v[0], "include"), "%s: %.*s does not start with include", id,
                  AVP_SF(it.key));
    }
}

static void do_smoke(const char *id) {
    AvpArena *a = avp_arena_new(0);
    if (!avp_isdir("src")) return; /* the Python skips: no src/ */
    RlConfig cfg;
    rl_config_load(a, &cfg, ".", NULL, NULL);
    AvpStrVec scan = {0}, one = {0};
    const char *err = NULL;
    avp_vec_push(&one, AVP_S("src"));
    REQUIRE(rl_collect_files(a, ".", &one, &scan, &err));
    RlChecker *ck = rl_checker_new(a, ".", &scan, &cfg);
    REQUIRE(ck != NULL);
    for (size_t i = 0; i < scan.n; i++) avp_vec_push(&ck->scan, scan.v[i]);
    ck->classes_data = rl_load_classes(a, "build/R5WEA4/classes.json", &ck->classes_note);
    rl_checker_run(ck);
    REQUIRE(!ck->failed && !ck->error);
    int blocking = 0;
    for (size_t i = 0; i < ck->findings.n; i++) {
        if (!rl_blocking(ck, &ck->findings.v[i])) continue;
        blocking++;
        fprintf(stderr, "%s: %.*s\n", id, AVP_SF(rl_finding_text(a, &ck->findings.v[i])));
    }
    CHECK_MSG(blocking == 0, "%s: %d blocking findings in the live tree", id, blocking);
    const char *hdrs[] = {"include/zlib/zlib.h", "include/libjpeg/jpeglib.h"};
    for (int h = 0; h < 2; h++) {
        if (!avp_map_has(ck->index, avp_str(hdrs[h]))) continue;
        bool found = false;
        for (size_t i = 0; i < ck->checked.n; i++)
            if (avp_str_eqz(ck->checked.v[i], hdrs[h])) found = true;
        CHECK_MSG(found, "%s: %s is not among the checked paths", id, hdrs[h]);
    }
}

static void do_tier_table(const char *id) {
    const AvpJson *rules = avp_json_getz(doc(), "rules");
    REQUIRE(rules != NULL);
    for (size_t i = 0; i < avp_json_len(rules); i++) {
        const AvpJsonMember *m = avp_json_member(rules, i);
        const char *t = rl_tier_of(avp_cz(avp_arena(), m->key));
        CHECK_MSG(t && avp_str_eq(avp_str(t), sv(m->val)), "%s: tier of %.*s", id, AVP_SF(m->key));
    }
    for (int n = 1; n < 80; n++) {
        char name[16];
        snprintf(name, sizeof name, "R%d", n);
        CHECK_MSG((rl_tier_of(name) != NULL) == avp_json_has(rules, avp_str(name)), "%s: %s is in one table only", id, name);
    }
}

/* ---- one Python test -------------------------------------------------------------------------------------------- */
static void run_entry(const char *id) {
    const AvpJson *tests = avp_json_getz(doc(), "tests");
    REQUIRE(tests != NULL);
    const AvpJson *entry = NULL;
    for (size_t i = 0; i < avp_json_len(tests) && !entry; i++)
        if (avp_str_eqz(jstr(avp_json_at(tests, i), "id"), id)) entry = avp_json_at(tests, i);
    REQUIRE(entry != NULL);
    AvpStr kind = jstr(entry, "kind");
    if (avp_str_eqz(kind, "skip")) {
        fprintf(stderr, "selftest: %s not ported: %.*s\n", id, AVP_SF(jstr(entry, "reason")));
        return;
    }
    if (avp_str_eqz(kind, "real")) {
        do_real_configure(id);
        return;
    }
    if (avp_str_eqz(kind, "smoke")) {
        do_smoke(id);
        return;
    }
    if (avp_str_eqz(kind, "table")) {
        do_tier_table(id);
        return;
    }
    const AvpJson *recs = avp_json_getz(entry, "recs");
    REQUIRE(avp_json_len(recs) > 0);
    for (size_t i = 0; i < avp_json_len(recs); i++) {
        const AvpJson *r = avp_json_at(recs, i);
        AvpStr k = jstr(r, "kind");
        if (avp_str_eqz(k, "run")) do_run(id, r);
        else if (avp_str_eqz(k, "main")) do_main(id, r);
        else if (avp_str_eqz(k, "call")) do_call(id, r);
        else avp_test__fail(__FILE__, __LINE__, "%s: unknown record kind", id);
    }
}

#define SELFTEST(name, id) AVP_TEST(name) { run_entry(id); }
#include "selftest_tests.h"

AVP_TEST_MAIN("check_rules_selftest")
