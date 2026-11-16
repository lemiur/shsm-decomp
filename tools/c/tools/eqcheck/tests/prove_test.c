/* Tests for eq_prove: prove_functions over the listings of tools/test_eqcheck.py's TestEquivalent (and extra hand
 * cases for explain, merges with stack slots, calls, tail calls, conditional returns, io and the return-register
 * retry), compared as the whole result (json.dumps without "seconds"); TestMutationsOnRealCode's functions and their
 * mutants; and an identity sweep (retail against itself, one CalleeLiveness per unit; functions are keyed by symbol and looked up in whichever retail unit holds them now),
 * each compared by verdict line and a hash of the whole result. Expected values (prove_expected.h) are printed by
 * tools/eqcheck.py on the same inputs. The compiled objects of the last build are not tested here (they change with the source): the
 * differential harness (diff/cases.txt, --built) compares the C and Python provers on them live. The real-tree parts are skipped when build/R5WEA4/asm is absent or
 * SHSM_NO_REAL_TREE is set (not "0"); a recorded function that no listing of the tree holds now (renamed or re-split) is skipped and
 * counted in the printed line, not failed. */
#include "avp/test.h"
#include "eq_prove.h"
#include "prove_expected.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BASE 0x80001000LL

static CcFn *fn_of(AvpArena *a, const char *text) {
    AvpStrVec lines = avp_str_split(avp_str(text), AVP_S("\n"), -1);
    CcRow *rows = AVP_NEWN(a, CcRow, lines.n ? lines.n : 1);
    size_t n = 0;
    for (size_t i = 0; i < lines.n; i++) rows[n++] = cc_parse_row(a, BASE + 4 * (i64)i, avp_str_dup(a, lines.v[i]));
    avp_vec_free(&lines);
    return cc_fn_new(a, AVP_S("f"), rows, n, NULL, false, 0, false, 0);
}

static u64 fnv(AvpStr s) {
    u64 h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < s.n; i++) h = (h ^ (u8)s.p[i]) * 0x100000001b3ull;
    return h;
}

static AvpStr dumps(AvpArena *a, const AvpJson *r) {
    AvpJsonDumpOpts o = {0};
    return avp_json_dumps(a, r, &o);
}

AVP_TEST(hand_cases) {
    AvpArena *a = avp_arena_new(0);
    for (size_t k = 0; k < sizeof PROVE_CASES / sizeof *PROVE_CASES; k++) {
        const ProveCase *c = &PROVE_CASES[k];
        CcFn *fa = fn_of(a, c->a), *fb = fn_of(a, c->b);
        EqRet ret = {c->ret, c->nret};
        AvpJson *r = eq_prove_functions_ex(a, fa, fb, NULL, ret, c->explain != 0, NULL, NULL, 60, false);
        AvpStr got = dumps(a, r);
        CHECK_MSG(avp_str_eqz(got, c->expected), "%s:\n  got  %.*s\n  want %s", c->name, AVP_SF(got), c->expected);
    }
    /* "seconds" is added last, as a float */
    CcFn *f = fn_of(a, "li r3, 0x1\nblr");
    AvpJson *r = eq_prove_functions(a, f, f, NULL, EQ_RET_DEFAULT, false, NULL, NULL);
    const AvpJsonMember *last = avp_json_member(r, avp_json_len(r) - 1);
    CHECK(last && avp_str_eqz(last->key, "seconds"));
    avp_arena_free(a);
}

AVP_TEST(helpers) {
    AvpArena *a = avp_arena_new(0);
    CHECK_CZ(eq_slot_locname(a, 8, 4), "r1+0x8/4");
    CHECK_CZ(eq_slot_locname(a, -0x10, 8), "r1-0x10/8");
    CHECK_CZ(eq_slot_locname(a, 0, 2), "r1+0x0/2");
    AvpStr n, o;
    CHECK(eq_quoted_search(AVP_S("lis r3, \"@123\"@ha"), &n, &o) && avp_str_eqz(n, "@123") && !o.p);
    CHECK(eq_quoted_search(AVP_S("bl \"a@b\"+0x1C"), &n, &o) && avp_str_eqz(n, "a@b") && avp_str_eqz(o, "0x1C"));
    CHECK(eq_quoted_search(AVP_S("x \"\"y\" z"), &n, &o) && avp_str_eqz(n, "y"));
    CHECK(eq_quoted_search(AVP_S("x \"q\"+0xg"), &n, &o) && avp_str_eqz(n, "q") && !o.p);
    CHECK(!eq_quoted_search(AVP_S("x \"\" y"), &n, &o));
    CHECK(!eq_quoted_search(AVP_S("no quote"), &n, &o));
    avp_arena_free(a);
}

static bool real_tree(void) {
    const char *e = getenv("SHSM_NO_REAL_TREE");
    if (e && *e && strcmp(e, "0") != 0) return false;
    return avp_isdir("build/R5WEA4/asm");
}

/* A copy of fn with rows replaced: edits "<index>\t<text>\n"... (the Python's _replace). */
static CcFn *replaced(AvpArena *a, const CcFn *f, const char *edits) {
    if (!edits) return (CcFn *)f;
    CcRow *rows = AVP_NEWN(a, CcRow, f->nrows ? f->nrows : 1);
    if (f->nrows) memcpy(rows, f->rows, f->nrows * sizeof *rows);
    AvpStrVec lines = avp_str_split(avp_str(edits), AVP_S("\n"), -1);
    for (size_t i = 0; i < lines.n; i++) {
        if (!lines.v[i].n) continue;
        size_t tab = (size_t)avp_str_find(lines.v[i], AVP_S("\t"), 0, AVP_END);
        size_t idx = (size_t)strtoul(avp_cz(a, avp_str_slice(lines.v[i], 0, (i64)tab)), NULL, 10);
        AVP_ASSERT(idx < f->nrows);
        rows[idx] = cc_parse_row(a, f->rows[idx].addr, avp_str_dup(a, avp_str_slice(lines.v[i], (i64)tab + 1, AVP_END)));
    }
    avp_vec_free(&lines);
    return cc_fn_new(a, f->name, rows, f->nrows, NULL, false, 0, false, 0);
}

typedef struct {
    const char *unit;
    AvpMap *retail;
    CcContext *ctx;
} UnitData;

static UnitData unit_data(const char *unit) {
    static UnitData last;
    if (last.unit && strcmp(last.unit, unit) == 0) return last;
    CcRepo repo = {avp_root(), "R5WEA4"};
    CcError err = {0};
    last.unit = unit;
    /* a unit that a rename or a re-split removed has no listing now: its functions are looked up elsewhere (locate) */
    last.retail = cc_load_retail(&repo, avp_str(unit), NULL, false, &err);
    last.ctx = last.retail ? cc_repo_context(&repo, avp_str(unit)) : NULL;
    return last;
}


/* Where a function lives now. The expected values are keyed by function name; the unit recorded beside each is only
 * where the function sat when they were generated. Splitting code into new units moves functions, so a unit whose
 * listing no longer holds the function is replaced by the unit whose listing does (the `.fn <name>,` blocks of
 * build/R5WEA4/asm). The first unit found wins when a local name occurs in several. */
typedef struct {
    const char *unit, *name;
} Loc;

static Loc *new_loc(const char *unit, AvpStr name) {
    Loc *l = malloc(sizeof *l);
    char *n = malloc(name.n + 1);
    memcpy(n, name.p, name.n);
    n[name.n] = 0;
    l->unit = unit;
    l->name = n;
    return l;
}

static AvpMap *fn_index(void) {
    static AvpMap *idx;
    if (idx) return idx;
    idx = avp_map_new();
    static const char top[] = "build/R5WEA4/asm";
    const size_t toplen = sizeof top - 1;
    AvpArena *a = avp_arena_new(0);
    AvpWalk *w = avp_walk_open(top, true);
    AvpWalkItem it = {0};
    while (avp_walk_next(w, &it)) {
        for (size_t i = 0; i < it.files.n; i++) {
            AvpStr f = it.files.v[i];
            if (f.n < 3 || memcmp(f.p + f.n - 2, ".s", 2) != 0) continue;
            const char *path = avp_sprintf(a, "%.*s/%.*s", (int)it.dirpath.n, it.dirpath.p, (int)f.n, f.p);
            AvpStr text;
            if (!avp_read_text(a, path, AVP_TM_UTF8_REPLACE, &text)) continue;
            AvpStr rel = it.dirpath.n > toplen ? avp_str_slice(it.dirpath, (i64)toplen + 1, AVP_END) : AVP_S("");
            char *unit = _strdup(rel.n ? avp_sprintf(a, "main/%.*s/%.*s", (int)rel.n, rel.p, (int)f.n - 2, f.p)
                                       : avp_sprintf(a, "main/%.*s", (int)f.n - 2, f.p));
            for (char *c = unit; *c; c++)
                if (*c == '\\') *c = '/';
            i64 pos = 0;
            for (;;) {
                i64 at = avp_str_find(text, AVP_S("\n.fn "), pos, AVP_END);
                if (at < 0) break;
                i64 s = at + 5, e = avp_str_find(text, AVP_S(","), s, AVP_END);
                i64 nl = avp_str_find(text, AVP_S("\n"), s, AVP_END);
                pos = s;
                if (e < 0 || (nl >= 0 && nl < e)) continue;
                AvpStr name = avp_str_slice(text, s, e);
                Loc *loc = NULL;
                bool created;
                AvpVal *v = avp_map_put(idx, name, &created);
                if (created) v->p = loc = new_loc(unit, name);
                /* The same function under a later name: the demangled-signature suffix a rename adds. */
                if (name.n > 11 && memcmp(name.p, "fn_", 3) == 0) {
                    AvpVal *pv = avp_map_put(idx, avp_str(avp_sprintf(a, "~%.*s", 11, name.p)), &created);
                    if (created) pv->p = loc ? loc : new_loc(unit, name);
                }
                pos = e;
            }
        }
    }
    avp_walk_close(w);
    avp_arena_free(a);
    return idx;
}

/* The unit and symbol under which a recorded function is found in the retail listings today: where it was recorded
 * when that still holds it, else the unit whose listing has the symbol, else (a renamed function) the one whose
 * symbol starts with the same fn_<address>. Unlocated functions keep the recorded pair (the caller reports them). */
static void locate(const char *unit, const char *name, const char **unit_out, const char **name_out) {
    *unit_out = unit;
    *name_out = name;
    UnitData u = unit_data(unit);
    if (u.retail && avp_map_get(u.retail, avp_str(name))) return;
    AvpVal *v = avp_map_get(fn_index(), avp_str(name));
    if (!v && strncmp(name, "fn_", 3) == 0 && strlen(name) >= 11) {
        char key[16];
        snprintf(key, sizeof key, "~%.11s", name);
        v = avp_map_get(fn_index(), avp_str(key));
    }
    if (!v) return;
    *unit_out = ((const Loc *)v->p)->unit;
    *name_out = ((const Loc *)v->p)->name;
}

static void prove_one(AvpArena *a, const CcFn *rf, const CcFn *cf, CcContext *ctx, EqLive *live, const char **line,
                      u64 *hash) {
    AvpTry tr;
    if (AVP_TRY(&tr)) {
        AvpJson *r = eq_prove_functions_ex(a, rf, cf, ctx, EQ_RET_DEFAULT, false, NULL, live, 60, false);
        avp_try_end(&tr);
        *line = avp_cz(a, avp_json_get_str(r, "line", AVP_S("")));
        *hash = fnv(dumps(a, r));
        return;
    }
    const EqExc *e = eq_exc_caught(&tr);
    *line = avp_sprintf(a, "EXC %s: %s", eq_exc_class(e->code), eq_exc_str(a, e));
    *hash = 0;
}

AVP_TEST(mutations_on_real_code) {
    if (!real_tree()) {
        printf("real tree: build/R5WEA4/asm absent, skipped\n");
        return;
    }
    AvpArena *a = avp_arena_new(0);
    size_t n = sizeof MUT_CASES / sizeof *MUT_CASES, bad = 0, mutants = 0, gone = 0;
    for (size_t k = 0; k < n; k++) {
        const MutCase *c = &MUT_CASES[k];
        const char *unit, *name;
        locate(c->unit, c->name, &unit, &name);
        UnitData u = unit_data(unit);
        AvpVal *v = u.retail ? avp_map_get(u.retail, avp_str(name)) : NULL;
        if (!v) { /* renamed or re-split since the expected values were recorded: nothing to compare */
            gone++;
            continue;
        }
        const CcFn *f = v->p;
        CcFn *o = replaced(a, f, c->orig_edits), *m = replaced(a, f, c->mut_edits);
        const char *line;
        u64 h;
        prove_one(a, o, m, u.ctx, NULL, &line, &h);
        if (c->mut_edits) {
            mutants++;
            CHECK_MSG(strncmp(line, "EQUIVALENT", 10) != 0, "%s %s: a mutant proved equivalent", unit, c->name);
        }
        if (strcmp(line, c->line) != 0 || h != c->hash) {
            bad++;
            CHECK_MSG(false, "%s %s (mutant %d):\n  got  %s (%016llX)\n  want %s (%016llX)", unit, c->name,
                      c->mut_edits != NULL, line, (unsigned long long)h, c->line, c->hash);
        }
    }
    printf("mutations: %zu entries (%zu mutants), %zu differ from the Python", n, mutants, bad);
    if (gone) printf("; %zu skipped: their function is in no listing of this tree now (renamed or re-split)", gone);
    printf("\n");
    avp_arena_free(a);
}

AVP_TEST(identity_sweep) {
    if (!real_tree() || !avp_isfile("build/index/asm.sqlite")) {
        printf("real tree: build/R5WEA4/asm or the asm index absent, skipped\n");
        return;
    }
    AvpArena *a = avp_arena_new(0);
    size_t n = sizeof SWEEP_CASES / sizeof *SWEEP_CASES, agree = 0, equiv = 0, gone = 0;
    EqLive *live = NULL;
    const char *cur = NULL;
    for (size_t k = 0; k < n; k++) {
        const SweepCase *c = &SWEEP_CASES[k];
        const char *unit, *name;
        locate(c->unit, c->name, &unit, &name);
        if (!cur || strcmp(cur, unit) != 0) {
            if (live) eq_live_free(live);
            live = eq_live_new(NULL, 4);
            cur = unit;
        }
        UnitData u = unit_data(unit);
        AvpVal *v = u.retail ? avp_map_get(u.retail, avp_str(name)) : NULL;
        if (!v) { /* renamed or re-split since the expected values were recorded: nothing to compare */
            gone++;
            continue;
        }
        const CcFn *f = v->p;
        const char *line;
        u64 h;
        prove_one(a, f, f, u.ctx, live, &line, &h);
        if (strcmp(line, c->line) == 0 && h == c->hash) agree++;
        else
            CHECK_MSG(false, "%s %s:\n  got  %s (%016llX)\n  want %s (%016llX)", unit, c->name, line,
                      (unsigned long long)h, c->line, c->hash);
        if (strncmp(c->line, "EQUIVALENT", 10) == 0) equiv++;
    }
    if (live) eq_live_free(live);
    printf("identity sweep: %zu functions, %zu agree with the Python (%zu EQUIVALENT there)", n, agree, equiv);
    if (gone) printf("; %zu skipped: their function is in no listing of this tree now (renamed or re-split)", gone);
    printf("\n");
    avp_arena_free(a);
}

AVP_TEST_MAIN("eqcheck prove")
