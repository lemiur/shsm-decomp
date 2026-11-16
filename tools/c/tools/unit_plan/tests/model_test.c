/* tools/test_unit_plan.py: the evidence tags, the class evidence, the ground truth and calibration, plan, the library
 * ranges and the helpers on synthetic models; plus the cache format's round trip and, on the real tree, plan_for. */
#include "avp/test.h"
#include "shsm_libraries.h"
#include "unit_plan.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define B0 0x80001000

/* fns(n, base, size): n packed functions f0..f(n-1); edit the array before building the model. */
static UpFunc *fns(AvpArena *a, size_t n, i64 base, i64 size) {
    UpFunc *f = AVP_NEWN(a, UpFunc, n);
    for (size_t i = 0; i < n; i++) {
        f[i].address = base + (i64)i * size;
        f[i].size = size;
        f[i].name = avp_sprintf(a, "f%zu", i);
        f[i].scope = "global";
        f[i].lib = "None";
        f[i].thunk = UP_NONE;
    }
    return f;
}

static void push(AvpI64Vec *v, i64 x) { avp_vec_push(v, x); }

static UpModel *model(AvpArena *a, UpFunc *f, size_t n, const UpDatum *d, size_t nd) {
    return up_model_new(a, f, n, d, nd, NULL, 0, NULL, 0, NULL, 0, NULL);
}

static UpTags tags_of(AvpArena *a, const UpModel *m, size_t g) {
    UpEvidence ev = up_gap_evidence(a, m);
    return g < ev.ngaps ? ev.tags[g] : 0;
}

static UpDatum datum(i64 addr, i64 size, const char *sec, const char *value, UpKind kind) {
    return (UpDatum){addr, size, sec, "global", value ? avp_str(value) : (AvpStr){NULL, 0}, kind};
}

/* ---- EvidenceTest ---- */

AVP_TEST(sinit_moves_past_factory_and_thunks) {
    AvpArena *a = avp_arena();
    UpFunc *f = fns(a, 6, B0, 0x10);
    f[1].sinit = true;
    push(&f[1].taken, 0x80001020);
    f[3].name = "@36@__dt__3FooFv";
    UpModel *m = model(a, f, 6, NULL, 0);
    CHECK(tags_of(a, m, 3) & UP_BIT(UP_T_SINIT));
    CHECK(!(tags_of(a, m, 1) & UP_BIT(UP_T_SINIT)));
}

AVP_TEST(dupfloat_single_free_gap) {
    AvpArena *a = avp_arena();
    UpDatum d[3] = {datum(0x2000, 4, ".sdata2", "1.0", UP_KIND_FLOAT), datum(0x2008, 4, ".sdata2", "1.0", UP_KIND_FLOAT),
                    datum(0x3000, 8, ".data", "x", UP_KIND_STRING)};
    UpFunc *f = fns(a, 5, B0, 0x10);
    push(&f[0].data, 0x2000);
    push(&f[1].data, 0x2000);
    push(&f[1].data, 0x3000);
    push(&f[2].data, 0x3000);
    push(&f[3].data, 0x2008);
    UpModel *m = model(a, f, 5, d, 3);
    CHECK(tags_of(a, m, 2) & UP_BIT(UP_T_DUPFLOAT));
    CHECK(!(tags_of(a, m, 1) & UP_BIT(UP_T_DUPFLOAT)));
}

AVP_TEST(span_vetoes_cut) {
    AvpArena *a = avp_arena();
    UpDatum d[1] = {datum(0x2000, 4, ".sdata2", "2.0", UP_KIND_FLOAT)};
    UpFunc *f = fns(a, 3, B0, 0x10);
    push(&f[0].data, 0x2000);
    f[0].size = 0x8;
    push(&f[2].data, 0x2000);
    UpModel *m = model(a, f, 3, d, 1);
    UpEvidence ev = up_gap_evidence(a, m);
    CHECK(ev.tags[0] & UP_BIT(UP_T_TEXTPAD));
    CHECK_EQ_I64(ev.strong[0], 1);
    CHECK_EQ_I64((i64)up_predict(m, &ev, NULL, 0.0).n, 0);
}

AVP_TEST(local_function_span) {
    AvpArena *a = avp_arena();
    UpFunc *f = fns(a, 3, B0, 0x10);
    f[0].size = 0x8;
    f[2].scope = "local";
    i64 lc[1][2] = {{f[0].address, f[2].address}};
    UpModel *m = up_model_new(a, f, 3, NULL, 0, (const i64(*)[2])lc, 1, NULL, 0, NULL, 0, NULL);
    UpEvidence ev = up_gap_evidence(a, m);
    CHECK_EQ_I64(ev.strong[0], 1);
    CHECK_EQ_I64(ev.strong[1], 1);
    CHECK_EQ_I64((i64)up_predict(m, &ev, NULL, 0.0).n, 0);
}

AVP_TEST(textpad_only_where_packed) {
    AvpArena *a = avp_arena();
    UpFunc *f = fns(a, 2, B0, 0x10);
    f[0].size = 0x8;
    CHECK(tags_of(a, model(a, f, 2, NULL, 0), 0) & UP_BIT(UP_T_TEXTPAD));
    i64 al[1][2] = {{f[0].address, 16}};
    UpModel *aligned = up_model_new(a, f, 2, NULL, 0, NULL, 0, (const i64(*)[2])al, 1, NULL, 0, NULL);
    CHECK(!(tags_of(a, aligned, 0) & UP_BIT(UP_T_TEXTPAD)));
}

AVP_TEST(order_datapad_nocross) {
    AvpArena *a = avp_arena();
    UpDatum d[2] = {datum(0x2000, 4, ".sdata", NULL, UP_KIND_OTHER), datum(0x2010, 4, ".sdata", NULL, UP_KIND_OTHER)};
    UpFunc *f = fns(a, 2, B0, 0x10);
    push(&f[0].data, 0x2000);
    push(&f[1].data, 0x2010);
    CHECK_EQ_U64(tags_of(a, model(a, f, 2, d, 2), 0),
                 UP_BIT(UP_T_NOCROSS) | UP_BIT(UP_T_ORDER) | UP_BIT(UP_T_DATAPAD));
    UpFunc *g = fns(a, 2, B0, 0x10); /* data used in the reverse order: not a monotonic advance */
    push(&g[0].data, 0x2010);
    push(&g[1].data, 0x2000);
    CHECK_EQ_U64(tags_of(a, model(a, g, 2, d, 2), 0), UP_BIT(UP_T_NOCROSS));
}

AVP_TEST(file_names_and_weak) {
    AvpArena *a = avp_arena();
    UpFunc *f = fns(a, 3, B0, 0x10);
    avp_vec_push(&f[0].strings, AVP_S("a.c"));
    avp_vec_push(&f[1].strings, AVP_S("b.c"));
    f[1].scope = "weak";
    UpModel *m = model(a, f, 3, NULL, 0);
    CHECK(tags_of(a, m, 0) & UP_BIT(UP_T_FILE));
    CHECK(tags_of(a, m, 1) & UP_BIT(UP_T_WEAK));
}

/* ---- ClassEvidenceTest ---- */

static bool thunk2(const char *m1, const char *o1, const char *m2, const char *o2) {
    AvpStr mn[2] = {avp_str(m1), avp_str(m2)}, ops[2] = {avp_str(o1), avp_str(o2)};
    return up_is_thunk(mn, ops, 2);
}

AVP_TEST(thunk_shape) {
    CHECK(thunk2("subi", "r3, r3, 0x24", "b", "fn_800CA0E4"));
    CHECK(thunk2("addi", "r3, r3, -0x34", "b", "fn_800CA3C0"));
    CHECK(!thunk2("addi", "r4, r3, -0x34", "b", "fn_800CA3C0"));
    CHECK(!thunk2("li", "r3, 0", "blr", ""));
}

/* class_unit(): FMVTrigger laid out as MWCC emits it, then a base-class function far away (index 8). */
static UpFunc *class_unit(AvpArena *a, const AvpJson **db) {
    UpFunc *f = fns(a, 9, B0, 0x10);
    for (int i = 0; i <= 4; i++) f[i].size = 0x0C;
    f[3].sinit = true;
    push(&f[3].taken, B0 + 0x40);
    f[5].size = 0x08;
    f[5].thunk = B0 + 0x10;
    f[6].size = 0x08;
    f[6].thunk = B0;
    f[7].size = 0x0C;
    f[8].name = "fn_X__4BaseFv";
    f[8].address = B0 + 0x1000;
    const char *text =
        "{\"classes\": ["
        "{\"name\": \"FMVTrigger\", \"members\": ["
        "{\"address\": \"0x80001000\", \"role\": \"dtor\"}, {\"address\": \"0x80001010\", \"role\": \"slot\"},"
        "{\"address\": \"0x80001020\", \"role\": \"slot\"}, {\"address\": \"0x80001040\", \"role\": \"factory\"},"
        "{\"address\": \"0x80002000\", \"role\": \"slot\"}],"
        "\"vtable\": {\"words\": [{\"status\": \"override\", \"target_address\": \"0x80001000\"},"
        "{\"status\": \"override\", \"target_address\": \"0x80001010\"},"
        "{\"status\": \"override\", \"target_address\": \"0x80001020\"},"
        "{\"status\": \"inherited\", \"target_address\": \"0x80002000\"}]}},"
        "{\"name\": \"Other\", \"members\": [{\"address\": \"0x80001070\", \"role\": \"ctor\"},"
        "{\"address\": \"0x80002000\", \"role\": \"factory\"}], \"vtable\": {\"words\": []}},"
        "{\"name\": \"ns\", \"members\": [{\"address\": \"0x80001070\", \"role\": \"mangled\"},"
        "{\"address\": \"0x80001000\", \"role\": \"mangled\"}], \"vtable\": null}]}";
    *db = avp_json_parse(a, avp_str(text), NULL);
    return f;
}

AVP_TEST(class_groups) {
    AvpArena *a = avp_arena();
    const AvpJson *db;
    UpFunc *f = class_unit(a, &db);
    size_t n = 0;
    UpClass *g = up_class_groups(a, db, f, 9, &n);
    REQUIRE(n == 1);
    CHECK_CZ(g[0].name, "FMVTrigger");
    REQUIRE(g[0].addrs.n == 7);
    for (size_t i = 0; i < 7; i++) CHECK_EQ_I64(g[0].addrs.v[i], f[i].address);
}

AVP_TEST(class_keeps_one_unit_to_the_thunks) {
    AvpArena *a = avp_arena();
    const AvpJson *db;
    UpFunc *f = class_unit(a, &db);
    size_t nc = 0;
    UpClass *cls = up_class_groups(a, db, f, 8, &nc);
    UpModel *m = up_model_new(a, f, 8, NULL, 0, NULL, 0, NULL, 0, cls, nc, NULL);
    UpEvidence ev = up_gap_evidence(a, m);
    UpCuts cuts = up_predict(m, &ev, NULL, 0.0);
    REQUIRE(cuts.n == 1);
    CHECK_EQ_U64(cuts.v[0].g, 6);
    CHECK(ev.tags[6] & UP_BIT(UP_T_CLASS));
    CHECK(ev.tags[6] & UP_BIT(UP_T_SINIT));
    UpUnits res = up_plan(a, m, f[0].address, up_fn_end(&f[7]), NULL, &ev, 0.0, NULL);
    REQUIRE(res.n == 2);
    CHECK_EQ_I64(res.v[0].start, 0x80001000);
    CHECK_EQ_I64(res.v[0].end, 0x80001068);
    CHECK_EQ_I64(res.v[1].start, 0x80001070);
    CHECK_EQ_I64(res.v[1].end, 0x8000107C);
}

AVP_TEST(without_classes_the_unit_falls_apart) {
    AvpArena *a = avp_arena();
    const AvpJson *db;
    UpFunc *f = class_unit(a, &db);
    UpModel *m = model(a, f, 8, NULL, 0);
    UpEvidence ev = up_gap_evidence(a, m);
    CHECK(up_predict(m, &ev, NULL, 0.0).n > 1);
}

AVP_TEST(sinit_tail_takes_unnamed_thunks) {
    AvpArena *a = avp_arena();
    const AvpJson *db;
    UpFunc *f = class_unit(a, &db);
    UpModel *m = model(a, f, 8, NULL, 0);
    CHECK(tags_of(a, m, 6) & UP_BIT(UP_T_SINIT));
    CHECK(!(tags_of(a, m, 4) & UP_BIT(UP_T_SINIT)));
}

AVP_TEST(stamp_follows_classes_json) {
    AvpArena *a = avp_arena();
    char *d = avp_mkdtemp(a, "up_stamp", NULL, NULL);
    REQUIRE(d != NULL);
    ShsmRepo r = {0};
    const char *none = avp_path_join(a, d, "none");
    r.symbols_txt = r.splits_txt = r.libraries_tsv = r.objdiff_json = r.configure_py = r.build_ninja = none;
    r.version_build_dir = d;
    AvpStr before = up_stamp_inputs(a, &r, "1,2,3,");
    FILE *fh = fopen(up_classes_json_path(a, &r), "wb");
    REQUIRE(fh != NULL);
    fputs("{}", fh);
    fclose(fh);
    CHECK(!avp_str_eq(before, up_stamp_inputs(a, &r, "1,2,3,")));
    avp_tempdir_cleanup(d);
}

AVP_TEST(mangled_class) {
    AvpArena *a = avp_arena();
    CHECK(up_mangled_class(a, "fn_X__17CFooFv") == NULL); /* 17 > len("CFooFv") */
    CHECK_CZ(up_mangled_class(a, "fn_X__4CFooFv"), "CFoo");
    CHECK_CZ(up_mangled_class(a, "@36@__dt__3FooFv"), "Foo");
    CHECK_CZ(up_mangled_class(a, "__ct__Q22Ns3BarFv"), "Ns::Bar");
    CHECK(up_mangled_class(a, "__ct__Q23Ns3BarFv") == NULL); /* "Ns3" eats the next length */
    CHECK_CZ(up_mangled_class(a, "__vt__4CFoo"), "CFoo");
    CHECK(up_mangled_class(a, "fn_80001000") == NULL);
    CHECK(up_mangled_class(a, "fn_X__Fi") == NULL);
}

/* ---- TruthTest ---- */

static UpModel *truth_model(AvpArena *a, UpFunc **out) {
    UpFunc *f = fns(a, 6, B0, 0x10);
    f[2].sinit = true;
    f[3].size = 0xC;
    *out = f;
    return model(a, f, 6, NULL, 0);
}

static const i64 TRUTH_RANGES[2][2] = {{B0, B0 + 0x30}, {B0 + 0x40, B0 + 0x60}};

AVP_TEST(labels) {
    AvpArena *a = avp_arena();
    UpFunc *f;
    UpModel *m = truth_model(a, &f);
    i8 *lab = up_truth(a, m, TRUTH_RANGES, 2);
    static const i8 want[5] = {0, 0, 1, 1, 0};
    for (int g = 0; g < 5; g++) CHECK_EQ_I64(lab[g], want[g]);
}

AVP_TEST(evaluate_and_plan) {
    AvpArena *a = avp_arena();
    UpFunc *f;
    UpModel *m = truth_model(a, &f);
    i8 *lab = up_truth(a, m, TRUTH_RANGES, 2);
    UpEvidence ev = up_gap_evidence(a, m);
    UpEvaluation e = up_evaluate(a, m, &ev, lab);
    CHECK_EQ_I64(e.ncuts, 2);
    CHECK_EQ_I64(e.calib->tag[UP_T_SINIT].hit, 1);
    CHECK_EQ_I64(e.calib->tag[UP_T_SINIT].total, 1);
    CHECK_EQ_I64(e.calib->tag[UP_T_TEXTPAD].hit, 1);
    CHECK_EQ_I64(e.calib->tag[UP_T_TEXTPAD].total, 1);
    UpUnits res = up_plan(a, m, f[0].address, up_fn_end(&f[5]), e.calib, &ev, 0.0, NULL);
    REQUIRE(res.n == 3);
    CHECK_EQ_I64(res.v[0].functions, 3);
    CHECK_EQ_I64(res.v[1].functions, 1);
    CHECK_EQ_I64(res.v[2].functions, 2);
    CHECK(strstr(res.v[0].command, "--text 80001000-80001030") != NULL);
    CHECK_EQ_U64(res.v[0].end_tags, UP_BIT(UP_T_SINIT));
    UpBands b = up_bands(m, &ev, e.calib, lab, 0.6);
    CHECK_EQ_I64(b.tp[0], 2);
    CHECK_EQ_I64(b.fp[0], 0);
    CHECK_EQ_I64(b.missed[0], 0);
}

/* ---- HelpersTest ---- */

static bool usage_error_find(AvpArena *a, const char *lib, const ShsmLibraries *libs) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        up_find_library(a, lib, NULL, libs);
        avp_try_end(&t);
        return false;
    }
    return t.code == SHSM_EXC_USAGE;
}

AVP_TEST(find_library) {
    AvpArena *a = avp_arena();
    const char *text = "start\tend\tlibrary\tnote\n"
                       "0x80001000\t0x80002000\tgame/G1\t\n"
                       "0x80002000\t0x80003000\tgame/G2\t\n"
                       "0x80003000\t0x80004000\tfmod\t\n"
                       "0x80005000\t0x80006000\tsdk/os\t\n"
                       "0x80007000\t0x80008000\tsdk/gx\t";
    ShsmLibraries *libs = shsm_libraries_parse(a, avp_str(text), NULL);
    UpLibRange r = up_find_library(a, "game/G2", NULL, libs);
    CHECK_EQ_I64(r.start, 0x80002000);
    CHECK_EQ_I64(r.end, 0x80003000);
    REQUIRE(r.names.n == 1);
    CHECK_STR(r.names.v[0], "game/G2");
    r = up_find_library(a, "g1", NULL, libs);
    CHECK_EQ_I64(r.start, 0x80001000);
    CHECK_EQ_I64(r.end, 0x80002000);
    r = up_find_library(a, "game", NULL, libs);
    CHECK_EQ_I64(r.start, 0x80001000);
    CHECK_EQ_I64(r.end, 0x80003000);
    REQUIRE(r.names.n == 2);
    CHECK_STR(r.names.v[1], "game/G2");
    r = up_find_library(a, "FMOD", NULL, libs);
    CHECK_EQ_I64(r.start, 0x80003000);
    CHECK(usage_error_find(a, "sdk", libs));
    CHECK(usage_error_find(a, "nothing", libs));
}

static bool usage_error_range(AvpArena *a, const char *text) {
    AvpTry t;
    i64 s, e;
    if (AVP_TRY(&t)) {
        up_parse_range(a, text, &s, &e);
        avp_try_end(&t);
        return false;
    }
    return t.code == SHSM_EXC_USAGE;
}

AVP_TEST(parse_range) {
    AvpArena *a = avp_arena();
    i64 s, e;
    up_parse_range(a, "800E0000-0x800E4000", &s, &e);
    CHECK_EQ_I64(s, 0x800E0000);
    CHECK_EQ_I64(e, 0x800E4000);
    CHECK(usage_error_range(a, "800E4000-800E0000"));
    CHECK(usage_error_range(a, "zz"));
}

AVP_TEST(path_hint) {
    AvpArena *a = avp_arena();
    AvpStrVec v = {0};
    avp_vec_push(&v, AVP_S("CColdTrigger"));
    CHECK_CZ(up_path_for(a, &v), "game/CColdTrigger");
    v.n = 0;
    avp_vec_push(&v, AVP_S("OSThread.c"));
    CHECK_CZ(up_path_for(a, &v), "<dir>/OSThread");
    v.n = 0;
    CHECK_CZ(up_path_for(a, &v), "<path>");
    avp_vec_free(&v);
}

/* ---- PS2 link order ---- */

static void ps2_at(UpFunc *f, size_t i, i64 rank) {
    f[i].has_ps2 = true;
    f[i].ps2 = rank;
}

AVP_TEST(ps2_far_apart_one_free_gap) {
    AvpArena *a = avp_arena();
    UpFunc *f = fns(a, 4, B0, 0x10);
    ps2_at(f, 1, 10);
    ps2_at(f, 2, 10 + UP_PS2_NEAR + 1);
    UpModel *m = model(a, f, 4, NULL, 0);
    CHECK(tags_of(a, m, 1) & UP_BIT(UP_T_PS2));
    CHECK(!(tags_of(a, m, 0) & UP_PS2_TAGS));
    CHECK(!(tags_of(a, m, 2) & UP_PS2_TAGS));
}

AVP_TEST(ps2_far_apart_several_free_gaps) {
    AvpArena *a = avp_arena();
    UpFunc *f = fns(a, 5, B0, 0x10);
    ps2_at(f, 0, 500);
    ps2_at(f, 3, 7);
    UpModel *m = model(a, f, 5, NULL, 0);
    CHECK(tags_of(a, m, 2) & UP_BIT(UP_T_PS2_N));
    CHECK(!(tags_of(a, m, 0) & UP_PS2_TAGS));
    CHECK(!(tags_of(a, m, 1) & UP_PS2_TAGS));
}

AVP_TEST(ps2_neighbours_mark_a_run) {
    AvpArena *a = avp_arena();
    UpFunc *f = fns(a, 5, B0, 0x10);
    ps2_at(f, 0, 30);
    ps2_at(f, 3, 29); /* order may run backwards */
    UpModel *m = model(a, f, 5, NULL, 0);
    UpEvidence ev = up_gap_evidence(a, m);
    for (size_t g = 0; g < 3; g++) CHECK_EQ_U64(ev.tags[g], UP_BIT(UP_T_PS2RUN));
    CHECK_EQ_U64(ev.tags[3], 0);
    CHECK_EQ_I64((i64)up_predict(m, &ev, NULL, 0.0).n, 0); /* evidence against a cut is no candidate */
}

AVP_TEST(ps2_too_far_or_missing_is_no_evidence) {
    AvpArena *a = avp_arena();
    UpFunc *f = fns(a, UP_PS2_SPAN + 3, B0, 0x10);
    ps2_at(f, 0, 1);
    ps2_at(f, UP_PS2_SPAN + 1, 900);
    UpModel *m = model(a, f, UP_PS2_SPAN + 3, NULL, 0);
    UpEvidence ev = up_gap_evidence(a, m);
    for (size_t g = 0; g < ev.ngaps; g++) CHECK(!(ev.tags[g] & UP_PS2_TAGS));
    UpFunc *h = fns(a, 4, B0, 0x10); /* no PS2 data at all */
    ev = up_gap_evidence(a, model(a, h, 4, NULL, 0));
    for (size_t g = 0; g < ev.ngaps; g++) CHECK_EQ_U64(ev.tags[g], 0);
}

AVP_TEST(ps2_ratio_scales_the_odds) {
    AvpArena *a = avp_arena();
    UpCalib *c = AVP_NEW(a, UpCalib);
    c->combo_index = avp_map_new();
    bool created;
    avp_map_put(c->combo_index, AVP_S("order"), &created)->i = 0;
    avp_vec_push(&c->combo_keys, AVP_S("order"));
    avp_vec_push(&c->combo, ((UpCount){6, 10}));
    c->ps2_cut[2] = (UpCount){0, 8};   /* ps2run on no true cut */
    c->ps2_other[2] = (UpCount){8, 8}; /* and on every other gap */
    CHECK(fabs(up_combine(UP_BIT(UP_T_ORDER), c) - 0.6) < 1e-9);
    double lr = up_ps2_ratio(c, 2); /* (1/10) / (9/10) */
    CHECK(fabs(lr - 1.0 / 9.0) < 1e-9);
    double odds = 1.5 * lr;
    CHECK(fabs(up_combine(UP_BIT(UP_T_ORDER) | UP_BIT(UP_T_PS2RUN), c) - odds / (1 + odds)) < 1e-9);
    /* PS2 tags alone: their own combination or tag rates */
    CHECK(up_combine(UP_BIT(UP_T_PS2), c) > 0);
}

/* ---- the cache format ---- */

AVP_TEST(model_round_trip) {
    AvpArena *a = avp_arena();
    const AvpJson *db;
    UpFunc *f = class_unit(a, &db);
    avp_vec_push(&f[1].strings, avp_strn("a\0b", 3));
    f[2].unit = "game/X";
    f[5].has_ps2 = true;
    f[5].ps2 = 42;
    size_t nc = 0;
    UpClass *cls = up_class_groups(a, db, f, 8, &nc);
    UpDatum d[1] = {{0x2000, 4, ".sdata2", "local", {"x\0y", 3}, UP_KIND_STRING}};
    i64 lc[1][2] = {{f[0].address, f[2].address}};
    i64 al[1][2] = {{f[1].address, 16}};
    UpModel *m = up_model_new(a, f, 8, d, 1, (const i64(*)[2])lc, 1, (const i64(*)[2])al, 1, cls, nc, "a note");
    AvpCacheWriter w;
    avp_cw_init(&w);
    up_model_write(&w, m);
    AvpCacheReader rd;
    avp_cr_init(&rd, w.b.v, w.b.n);
    UpModel *m2 = up_model_read(a, &rd);
    REQUIRE(m2 != NULL);
    REQUIRE(m2->n == m->n);
    for (size_t i = 0; i < m->n; i++) {
        CHECK_EQ_I64(m2->fns[i].address, m->fns[i].address);
        CHECK_EQ_I64(m2->fns[i].thunk, m->fns[i].thunk);
        CHECK_CZ(m2->fns[i].name, m->fns[i].name);
        CHECK_EQ_I64((i64)m2->fns[i].strings.n, (i64)m->fns[i].strings.n);
        CHECK((m2->fns[i].unit == NULL) == (m->fns[i].unit == NULL));
    }
    CHECK(avp_str_eq(m2->fns[1].strings.v[0], avp_strn("a\0b", 3)));
    CHECK(avp_str_eq(m2->data[0].value, avp_strn("x\0y", 3)));
    CHECK_EQ_I64((i64)m2->nclasses, (i64)nc);
    CHECK_EQ_I64((i64)m2->nlocal, 1);
    CHECK_CZ(m2->class_note, "a note");
    CHECK(!m2->fns[0].has_ps2);
    CHECK(m2->fns[5].has_ps2);
    CHECK_EQ_I64(m2->fns[5].ps2, 42);
    UpEvidence e1 = up_gap_evidence(a, m), e2 = up_gap_evidence(a, m2);
    for (size_t g = 0; g < e1.ngaps; g++) CHECK_EQ_U64(e1.tags[g], e2.tags[g]);
    avp_cw_free(&w);
}

/* ---- RealTreeTest: plan_for (the harness covers the command line) ---- */

AVP_TEST(plan_for_cached) {
    AvpArena *a = avp_arena();
    if (!avp_exists("build/index/asm.sqlite") || !avp_exists("config/R5WEA4/symbols.txt")) return; /* no real tree */
    /* the real index, read-only, and a private cache folder: the checkout's build/index/unit_plan.cbin is shared with
     * every other run of the tool (a parallel harness or test may be writing it) */
    ShsmRepo r = *shsm_repo_find(NULL);
    ShsmAsmIndex *ix = shsm_asmindex_open(&(ShsmAsmIndexOpts){.repo = &r, .readonly = true, .no_refresh = true});
    if (!shsm_asmindex_is_current(ix)) { /* the tree was regenerated and not re-indexed: its rows name files that are gone */
        shsm_asmindex_close(ix);
        printf("real tree: build/index/asm.sqlite is older than build/R5WEA4/asm; refresh it with any index-reading "
               "tool (query.exe fn <symbol>). Skipped\n");
        return;
    }
    r.index_dir = avp_mkdtemp(a, "up_plan_for", NULL, NULL);
    UpPlanDoc *d = up_plan_for(a, 0x800067A0, 0x80006C7C, &r, "low", ix, true, false);
    REQUIRE(d != NULL);
    REQUIRE(avp_isfile(up_cache_path(a, &r)));
    /* build false: answered from the cache just written, NULL when it is not current */
    d = up_plan_for(a, 0x800067A0, 0x80006C7C, &r, "low", ix, false, false);
    shsm_asmindex_close(ix);
    avp_tempdir_cleanup(r.index_dir);
    REQUIRE(d != NULL && d->units.n > 0);
    CHECK_EQ_I64(d->units.v[d->units.n - 1].end, 0x80006C7C);
    CHECK(strstr(d->header, "boundaries of each kind") != NULL);
}

AVP_TEST_MAIN("unit_plan")
