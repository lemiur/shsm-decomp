/* Ports tools/test_witness.py TestCorrespondence: made-up call graphs (anchors and propagation, one-to-many
 * inlining, a wrapper inlined and a substitute, conflicting votes, incompatible strings, tables, validate counts). */
#include "avp/test.h"
#include "witness_map.h"

#include <stdio.h>
#include <string.h>

#define NOCALLS ((const i64[]){0}), 0
#define NOSTRS ((const char *const[]){NULL}), 0
#define CALLS(...) ((const i64[]){__VA_ARGS__}), (sizeof((const i64[]){__VA_ARGS__}) / sizeof(i64))
#define STRS(...) ((const char *const[]){__VA_ARGS__}), (sizeof((const char *const[]){__VA_ARGS__}) / sizeof(char *))

/* fn(addr, n, calls, strs): FnInfo(addr, 4 * n, n, calls=calls, sites=range(len(calls)), strs=strs,
 * name="fn_%X" % addr). */
static void fn(WtSide *s, i64 addr, i64 n, const i64 *calls, size_t ncalls, const char *const *strs, size_t nstrs) {
    WtFn *f = wt_fn_new(addr);
    f->size = 4 * n;
    f->n = n;
    for (size_t k = 0; k < ncalls; k++) {
        avp_vec_push(&f->calls, calls[k]);
        avp_vec_push(&f->sites, (i64)k);
    }
    for (size_t k = 0; k < nstrs; k++) avp_set_add(f->strs, avp_str(strs[k]));
    char name[32];
    snprintf(name, sizeof name, "fn_%llX", (unsigned long long)addr);
    f->name = avp_strdup(avp_arena(), name);
    wt_side_put(s, f);
}

static WtSide wii, ps2;

static void sides(void) {
    wt_side_init(&wii);
    wt_side_init(&ps2);
}

static WtCorr *model(void) {
    sides();
    fn(&wii, 1, 10, CALLS(2, 3), STRS("AnchorOne"));
    fn(&wii, 2, 10, NOCALLS, STRS("AnchorTwo"));
    fn(&wii, 3, 10, CALLS(4), STRS("AnchorThree"));
    fn(&wii, 4, 12, NOCALLS, NOSTRS);
    fn(&wii, 6, 40, NOCALLS, STRS("Six", "HelperSix"));
    fn(&ps2, 101, 10, CALLS(102, 150, 103), STRS("AnchorOne"));
    fn(&ps2, 102, 10, NOCALLS, STRS("AnchorTwo"));
    fn(&ps2, 150, 5, NOCALLS, NOSTRS); /* a helper the Wii build inlined into 1 */
    fn(&ps2, 103, 10, CALLS(104), STRS("AnchorThree"));
    fn(&ps2, 104, 14, NOCALLS, NOSTRS);
    fn(&ps2, 106, 30, CALLS(160), STRS("Six"));
    fn(&ps2, 160, 8, NOCALLS, STRS("HelperSix")); /* inlined into 6: its string lands there */
    return wt_corr_run(wt_corr_new(&wii, &ps2, NULL, NULL, NULL), 30);
}

static bool has_pair(const WtCorr *c, i64 w, i64 p, const char *tier) {
    const WtPairEntry *e = wt_corr_pair(c, w);
    return e && e->p.ps2 == p && (!tier || strcmp(e->p.tier, tier) == 0);
}

AVP_TEST(anchors_and_propagation) {
    WtCorr *c = model();
    CHECK(has_pair(c, 1, 101, "string"));
    CHECK(has_pair(c, 4, 104, "call"));
    CHECK(!wt_corr_rev(c, 150, NULL));
    wt_corr_free(c);
}

AVP_TEST(one_to_many_inlining) {
    WtCorr *c = model();
    CHECK(has_pair(c, 6, 106, NULL)); /* the caller, not the helper */
    bool found = false;
    for (size_t k = 0; k < c->inl.n; k++)
        if (c->inl.v[k].wii == 6 && c->inl.v[k].helper == 160 && c->inl.v[k].parent == 106) found = true;
    CHECK(found);
    WtCallDiff d;
    wt_call_diff(c, 1, 101, &d);
    REQUIRE(d.ps2_only.n == 1);
    CHECK_EQ_I64(d.ps2_only.v[0], 150);
    REQUIRE(d.both.n == 2);
    CHECK(d.both.v[0].wii == 2 && d.both.v[0].ps2 == 102);
    CHECK(d.both.v[1].wii == 3 && d.both.v[1].ps2 == 103);
    wt_call_diff_free(&d);
    WtInls rows = wt_corr_inlined_rows(c);
    found = false;
    for (size_t k = 0; k < rows.n; k++)
        if (rows.v[k].wii == 1 && rows.v[k].helper == 150 && rows.v[k].parent == 101 &&
            strcmp(rows.v[k].ev, "call set") == 0)
            found = true;
    CHECK(found);
    avp_vec_free(&rows);
    wt_corr_free(c);
}

AVP_TEST(wrapper_inlined_and_substitute) {
    sides();
    fn(&wii, 1, 10, CALLS(5, 7), STRS("Root"));
    fn(&wii, 5, 10, NOCALLS, STRS("Five"));
    fn(&wii, 7, 10, NOCALLS, NOSTRS);
    fn(&ps2, 101, 10, CALLS(120, 130), STRS("Root"));
    fn(&ps2, 105, 10, NOCALLS, STRS("Five"));
    fn(&ps2, 120, 10, CALLS(105), NOSTRS);
    fn(&ps2, 130, 200, NOCALLS, NOSTRS);
    WtCorr *c = wt_corr_run(wt_corr_new(&wii, &ps2, NULL, NULL, NULL), 30);
    WtCallDiff d;
    wt_call_diff(c, 1, 101, &d);
    bool in_p = false, in_w = false;
    for (size_t k = 0; k < d.ps2_only.n; k++) in_p |= d.ps2_only.v[k] == 120; /* wrapper of 5, inlined on Wii */
    for (size_t k = 0; k < d.wii_only.n; k++) in_w |= d.wii_only.v[k] == 5;
    CHECK(in_p);
    CHECK(!in_w);
    wt_call_diff_free(&d);
    wt_corr_free(c);
}

AVP_TEST(conflicting_votes_left_alone) {
    sides();
    fn(&wii, 1, 10, CALLS(3), STRS("A1"));
    fn(&wii, 2, 10, CALLS(3), STRS("A2"));
    fn(&wii, 3, 10, NOCALLS, NOSTRS);
    fn(&ps2, 11, 10, CALLS(13), STRS("A1"));
    fn(&ps2, 12, 10, CALLS(14), STRS("A2"));
    fn(&ps2, 13, 10, NOCALLS, NOSTRS);
    fn(&ps2, 14, 10, NOCALLS, NOSTRS);
    WtCorr *c = wt_corr_run(wt_corr_new(&wii, &ps2, NULL, NULL, NULL), 30);
    CHECK(!wt_corr_pair(c, 3));
    wt_corr_free(c);
}

AVP_TEST(incompatible_strings_block) {
    sides();
    fn(&wii, 1, 10, CALLS(3), STRS("Root"));
    fn(&wii, 3, 10, NOCALLS, STRS("Wii text"));
    fn(&ps2, 11, 10, CALLS(13), STRS("Root"));
    fn(&ps2, 13, 10, NOCALLS, STRS("PS2 text"));
    WtCorr *c = wt_corr_run(wt_corr_new(&wii, &ps2, NULL, NULL, NULL), 30);
    CHECK(!wt_corr_pair(c, 3));
    wt_corr_free(c);
}

static WtRun run_of(const char *kinds, const i64 *vals) {
    WtRun r = {0};
    for (size_t k = 0; kinds[k]; k++) {
        WtRunItem it = {kinds[k], kinds[k] == 'f' ? vals[k] : 0, NULL};
        avp_vec_push(&r, it);
    }
    return r;
}

AVP_TEST(tables) {
    sides();
    char text[16];
    for (i64 a = 1; a < 6; a++) {
        snprintf(text, sizeof text, "Str%lld", (long long)a);
        const char *s[1] = {avp_strdup(avp_arena(), text)};
        fn(&wii, a, 10, NOCALLS, s, a < 3 ? 1 : 0);
        fn(&ps2, a + 100, 10, NOCALLS, s, a < 3 ? 1 : 0);
    }
    WtRuns rw = {0}, rp = {0};
    avp_vec_push(&rw, run_of("ffoff", (const i64[]){1, 2, 0, 3, 4}));
    avp_vec_push(&rp, run_of("ffoff", (const i64[]){101, 102, 0, 103, 104}));
    WtCorr *c = wt_corr_run(wt_corr_new(&wii, &ps2, &rw, &rp, NULL), 30);
    CHECK(has_pair(c, 3, 103, "table"));
    CHECK(has_pair(c, 4, 104, "table"));
    CHECK(!wt_corr_pair(c, 5));
    wt_corr_free(c);
}

AVP_TEST(validate_counts) {
    sides();
    char text[16];
    for (i64 a = 1; a < 40; a++) {
        snprintf(text, sizeof text, "T%lld", (long long)a);
        const char *s[1] = {avp_strdup(avp_arena(), text)};
        fn(&wii, a, 10, NOCALLS, s, 1);
        fn(&ps2, a + 1000, 10, NOCALLS, s, 1);
    }
    WtCorr *c = wt_corr_run(wt_corr_new(&wii, &ps2, NULL, NULL, NULL), 30);
    WtValidation v;
    wt_validate(&wii, &ps2, NULL, NULL, c->anchor_of, &v);
    CHECK_EQ_I64(v.correct + v.wrong + v.missed, v.held_out);
    CHECK_EQ_I64(v.wrong, 0);
    CHECK(v.held_out > 0);
    avp_vec_free(&v.by_tier);
    wt_corr_free(c);
}

AVP_TEST(hkey_and_ev_strings) {
    /* int(hashlib.sha1(b"1").hexdigest()[:8], 16): sha1("1") = 356a192b... */
    CHECK_EQ_I64(wt_hkey(1), 0x356a192b);
    AvpStr ts[3] = {AVP_S("ab"), AVP_S("abcd"), AVP_S("xyzw")};
    CHECK_CZ(wt_ev_strings(avp_arena(), ts, 3), "3 unique string(s): \"abcd\", \"xyzw\"");
    AvpStr one[1] = {AVP_S("caf\xc3\xa9 0123456789012345678901234567890123456789")};
    CHECK_CZ(wt_ev_strings(avp_arena(), one, 1),
             "1 unique string(s): \"caf\\u00e9 01234567890123456789012345678901234\"");
}

AVP_TEST_MAIN("witness")
