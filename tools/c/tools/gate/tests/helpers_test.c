/* HelperTests of tools/test_gate.py.
 *   test_porcelain                         -> porcelain
 *   test_allow_list                        -> allow_list
 *   test_plain                             -> plain
 *   test_ninja_deps_and_reach              -> ninja_deps_and_reach
 *   test_quick_refuses_empty_work_list_and_never_prints_gate_ok -> quick_refuses_empty_work_list
 *   test_quick_unchanged_fall_is_informational -> quick_unchanged_fall_is_informational
 *   test_quick_edited_fall_is_red          -> quick_edited_fall_is_red
 *   test_fallen_functions                  -> fallen_functions
 *   test_remove_tree_refuses_links         -> remove_tree_refuses_links (a junction: needs no symlink privilege)
 * Added: the regression helpers ported from merge_lane.py / score_history.py (gate_regress.h). */
#include "gate_fixtures.h"

AVP_TEST(porcelain) {
    AvpStr z = avp_strn(" M a.c\0R  new.c\0old.c\0?? b.h\0", sizeof(" M a.c\0R  new.c\0old.c\0?? b.h\0") - 1);
    GatePorcelainVec got = gate_parse_porcelain_z(z);
    REQUIRE(got.n == 4);
    CHECK_STR(got.v[0].path, "a.c");
    CHECK_STR(got.v[1].path, "new.c");
    CHECK_STR(got.v[2].path, "old.c");
    CHECK_STR(got.v[3].path, "b.h");
}

AVP_TEST(allow_list) {
    CHECK(gate_is_allowed_dirty(TA, "build/score_history.sqlite"));
    CHECK(gate_is_allowed_dirty(TA, "tools/__pycache__/gate.cpython-314.pyc"));
    CHECK(!gate_is_allowed_dirty(TA, "src/game/PadRumble.cpp"));
    CHECK(!gate_is_allowed_dirty(TA, "config/R5WEA4/baseline_report.json"));
}

AVP_TEST(plain) { CHECK_STR(gate_plain(TA, AVP_S("x: \x1b[32mOK\x1b[39m")), "x: OK"); }

static GateUnits two_units(void) {
    GateUnits us = {0};
    GateUnit a = {"main/game/A", "game/A", "NonMatching", "src/game/A.cpp", "X:/w/slot/build/R5WEA4/src/game/A.o", true};
    GateUnit b = {"main/game/B", "game/B", "NonMatching", "src/game/B.cpp", "X:/w/slot/build/R5WEA4/src/game/B.o", true};
    avp_vec_push(&us, a);
    avp_vec_push(&us, b);
    return us;
}

static AvpStrVec files1(const char *f) {
    AvpStrVec v = {0};
    avp_vec_push(&v, avp_str(f));
    return v;
}

AVP_TEST(ninja_deps_and_reach) {
    AvpStr text = AVP_S("build/R5WEA4/src/game/A.o: #deps 2, deps mtime 1 (VALID)\n"
                        "    src/game/A.cpp\n    X:/w/slot/include/game/V.h\n\n"
                        "build/R5WEA4/src/game/B.o: #deps 1, deps mtime 1 (VALID)\n    src/game/B.cpp\n\n");
    AvpMap *deps = gate_parse_ninja_deps(TA, text, "X:/w/slot");
    AvpVal *v = avp_map_get(deps, AVP_S("build/R5WEA4/src/game/A.o"));
    REQUIRE(v);
    AvpStrVec *a = v->p;
    REQUIRE(a->n == 2);
    CHECK_STR(a->v[0], "src/game/A.cpp");
    CHECK_STR(a->v[1], "include/game/V.h");
    GateUnits us = two_units();
    AvpStrVec f = files1("include/game/V.h");
    AvpMap *r = gate_reachable_units(TA, &us, &f, deps, "X:/w/slot");
    CHECK_EQ_I64(avp_map_len(r), 1);
    CHECK(avp_map_has(r, AVP_S("main/game/A")));
    f = files1("src/game/B.cpp");
    r = gate_reachable_units(TA, &us, &f, deps, "X:/w/slot");
    CHECK(avp_map_len(r) == 1 && avp_map_has(r, AVP_S("main/game/B")));
    f = files1("configure.py");
    r = gate_reachable_units(TA, &us, &f, deps, "X:/w/slot");
    CHECK_EQ_I64(avp_map_len(r), 2);
    f = files1("docs/x.md");
    r = gate_reachable_units(TA, &us, &f, deps, "X:/w/slot");
    CHECK_EQ_I64(avp_map_len(r), 0);
}

static void no_units(void *ctx, AvpArena *a, const char *root, const char *version, GateUnits *out) {
    (void)ctx, (void)a, (void)root, (void)version, (void)out;
}

AVP_TEST(quick_refuses_empty_work_list) {
    const char *tmp = tf_tmpdir();
    tf_make_root(tmp, NULL, NULL);
    TfEntry t[] = {{"diff", 0, "docs/x.md\n", NULL, NULL},
                   {"ls-files", 0, "", NULL, NULL},
                   {"shasum", 0, "build/R5WEA4/main.dol: OK\n", NULL, NULL}};
    TfRunner r = {t, 3, {0}};
    AvpBuf out;
    avp_buf_init(&out, TA);
    GateQuick q = {tf_run, &r, &out, no_units, NULL, {0}, tf_tools(&r), NULL, NULL, NULL};
    GateOpts o = tf_opts();
    o.quick = true;
    o.since = "HEAD";
    const char *line;
    CHECK(!gate_run_quick(TA, &o, tmp, &q, &line));
    CHECK_MSG(tf_starts(line, "QUICK RED empty: "), "%s", line);
    CHECK(!tf_contains(line, "GATE OK"));
    avp_tempdir_cleanup(tmp);
}

/* An objdiff report: units = [(unit, [(name, address, fuzzy)])]. */
typedef struct {
    const char *name;
    i64 addr;
    double p;
} F;
typedef struct {
    const char *unit;
    F fs[3];
    size_t n;
} U;

static void write_report(const char *path, const U *us, size_t n) {
    AvpBuf b;
    avp_buf_init(&b, TA);
    avp_buf_putz(&b, "{\"units\": [");
    for (size_t i = 0; i < n; i++) {
        double mn = 100.0;
        for (size_t k = 0; k < us[i].n; k++)
            if (k == 0 || us[i].fs[k].p < mn) mn = us[i].fs[k].p;
        char mb[32];
        avp_float_repr(mn, mb);
        avp_buf_printf(&b, "%s{\"name\": \"%s\", \"measures\": {\"fuzzy_match_percent\": %s}, \"functions\": [",
                       i ? ", " : "", us[i].unit, mb);
        for (size_t k = 0; k < us[i].n; k++) {
            char sb[32];
            avp_float_repr(us[i].fs[k].p, sb);
            avp_buf_printf(&b, "%s{\"name\": \"%s\", \"fuzzy_match_percent\": %s, \"metadata\": {\"virtual_address\": "
                               "\"%lld\"}}",
                           k ? ", " : "", us[i].fs[k].name, sb, (long long)us[i].fs[k].addr);
        }
        avp_buf_putz(&b, "]}");
    }
    avp_buf_putz(&b, "]}");
    tf_write(path, avp_buf_cz(&b));
}

static void quick_units(void *ctx, AvpArena *a, const char *root, const char *version, GateUnits *out) {
    (void)ctx, (void)a, (void)root, (void)version;
    GateUnit ua = {"main/game/A", "game/A", "NonMatching", "src/game/A.cpp", NULL, true};
    GateUnit uz = {"main/game/Z", "game/Z", "NonMatching", "src/game/Z.cpp", NULL, true};
    avp_vec_push(out, ua);
    avp_vec_push(out, uz);
}

typedef struct {
    bool edit_b;      /* fn_B is edited, the rest unchanged */
    GateFalls seen;
} Split;

static void split_falls(void *ctx, AvpArena *a, const GateFalls *fallen, GateFalls *edited, GateFalls *unchanged) {
    (void)a;
    Split *s = ctx;
    for (size_t i = 0; i < fallen->n; i++) {
        GateFall f = fallen->v[i];
        gate_falls_put(&s->seen, f);
        bool e = s->edit_b && strcmp(f.name, "fn_B") == 0;
        f.label = e ? "function source changed" : "function source identical";
        gate_falls_put(e ? edited : unchanged, f);
    }
}

static bool run_quick_with(bool edit_b, const char **line, AvpBuf *out, Split *s) {
    const char *tmp = tf_tmpdir();
    tf_make_root(tmp, NULL, NULL);
    U base[] = {{"main/game/A", {{"fn_A", 0x80001000, 100.0}, {"fn_B", 0x80001010, 100.0}, {"fn_C", 0x80001020, 100.0}}, 3},
                {"main/game/Z", {{"fn_Z", 0x80002000, 100.0}}, 1}};
    U fresh[] = {{"main/game/A", {{"fn_A", 0x80001000, 100.0}, {"fn_B", 0x80001010, 90.0}, {"fn_C", 0x80001020, 95.0}}, 3},
                 {"main/game/Z", {{"fn_Z", 0x80002000, 10.0}}, 1}}; /* not reachable: never looked at */
    write_report(tf_join(tmp, "config/R5WEA4/baseline_report.json"), base, 2);
    write_report(tf_join(tmp, "build/R5WEA4/report.json"), fresh, 2);
    TfEntry t[] = {{"diff", 0, "src/game/A.cpp\n", NULL, NULL},
                   {"ls-files", 0, "", NULL, NULL},
                   {"shasum", 0, "build/R5WEA4/main.dol: OK\n", NULL, NULL}};
    TfRunner r = {t, 3, {0}};
    *s = (Split){edit_b, {0}};
    GateQuick q = {tf_run, &r, out, quick_units, NULL, {0}, tf_tools(&r), NULL, split_falls, s};
    GateOpts o = tf_opts();
    o.quick = true;
    o.since = "HEAD";
    bool ok = gate_run_quick(TA, &o, tmp, &q, line);
    avp_tempdir_cleanup(tmp);
    return ok;
}

AVP_TEST(quick_unchanged_fall_is_informational) {
    AvpBuf out;
    avp_buf_init(&out, TA);
    const char *line;
    Split s;
    bool ok = run_quick_with(false, &line, &out, &s);
    CHECK_MSG(ok, "%s", line);
    CHECK(tf_starts(line, "QUICK OK (not a gate) "));
    CHECK(tf_contains(line, "2 informational fall(s)"));
    CHECK(tf_contains(avp_buf_cz(&out), "informational: 2 fall(s) in unchanged functions"));
    REQUIRE(s.seen.n == 2); /* fn_Z not reachable */
    CHECK_CZ(s.seen.v[0].name, "fn_B");
    CHECK_CZ(s.seen.v[1].name, "fn_C");
}

AVP_TEST(quick_edited_fall_is_red) {
    AvpBuf out;
    avp_buf_init(&out, TA);
    const char *line;
    Split s;
    CHECK(!run_quick_with(true, &line, &out, &s));
    CHECK_MSG(tf_starts(line, "QUICK RED scores: 1 problem(s), e.g. main/game/A fn_B: 100.00% -> 90.00%"), "%s", line);
    CHECK(tf_contains(avp_buf_cz(&out), "function source changed, baseline"));
}

AVP_TEST(fallen_functions) {
    const char *tmp = tf_tmpdir();
    U b[] = {{"main/A", {{"f", 1, 100.0}, {"g", 2, 50.0}}, 2}, {"main/B", {{"h", 3, 100.0}}, 1}};
    U f[] = {{"main/A", {{"f", 1, 99.0}, {"g", 2, 60.0}}, 2}, {"main/B", {{0}}, 0}};
    write_report(tf_join(tmp, "b.json"), b, 2);
    write_report(tf_join(tmp, "f.json"), f, 2);
    ShsmReport *base = shsm_read_report(tf_join(tmp, "b.json"));
    ShsmReport *fresh = shsm_read_report(tf_join(tmp, "f.json"));
    GateFalls out = {0};
    gate_fallen_functions(TA, base, fresh, NULL, &out);
    REQUIRE(out.n == 2);
    CHECK_CZ(out.v[0].key, "1");
    CHECK(out.v[0].has_new && out.v[0].new_ == 99.0 && out.v[0].old == 100.0);
    CHECK_CZ(out.v[0].unit, "main/A");
    CHECK_CZ(out.v[1].key, "3");
    CHECK(!out.v[1].has_new);
    AvpSet *only = avp_set_new();
    avp_set_add(only, AVP_S("main/B"));
    GateFalls ob = {0};
    gate_fallen_functions(TA, base, fresh, only, &ob);
    REQUIRE(ob.n == 1);
    CHECK_CZ(ob.v[0].key, "3");
    avp_tempdir_cleanup(tmp);
}

AVP_TEST(remove_tree_refuses_links) {
    const char *tmp = tf_tmpdir();
    const char *target = tf_join(tmp, "target"), *top = tf_join(tmp, "top"), *link = tf_join(top, "link");
    avp_makedirs(target, true);
    tf_write(tf_join(target, "keep"), "");
    avp_makedirs(top, true);
    if (!avp_junction_create(link, target)) {
        printf("skipped: cannot create a junction here\n");
        avp_tempdir_cleanup(tmp);
        return;
    }
    int code = 0;
    const char *msg = NULL;
    TF_RAISES(code, msg, gate_remove_tree(TA, top));
    CHECK_EQ_I64(code, SHSM_EXC_USAGE);
    CHECK(tf_contains(msg, "junction or symlink"));
    CHECK(avp_exists(tf_join(target, "keep")));
    avp_rmdir(link); /* the link only */
    avp_tempdir_cleanup(tmp);
}

/* ---- the merge_lane / score_history helpers of gate_regress.h -------------------------------------------------- */
AVP_TEST(sh_demangle) {
    bool sinit;
    AvpStrVec c = gate_sh_demangle(TA, "__ct__Q24Game6PlayerFv", &sinit);
    REQUIRE(c.n == 3);
    CHECK_STR(c.v[0], "Game::Player::Player");
    CHECK_STR(c.v[1], "Player::Player");
    CHECK_STR(c.v[2], "__ct__Q24Game6PlayerFv");
    c = gate_sh_demangle(TA, "__dt__8unk_RampFv", &sinit);
    REQUIRE(c.n == 2);
    CHECK_STR(c.v[0], "unk_Ramp::~unk_Ramp");
    c = gate_sh_demangle(TA, "@123@__as__3FooCFRC3Foo", &sinit);
    CHECK_STR(c.v[0], "Foo::operator=");
    c = gate_sh_demangle(TA, "fn_80001000", &sinit);
    CHECK(c.n == 1 && !sinit);
    gate_sh_demangle(TA, "__sinit_\\PadRumble_cpp", &sinit);
    CHECK(sinit);
}

AVP_TEST(func_scores_and_merge) {
    AvpJsonError e;
    AvpJson *cj = avp_json_parse(
        TA,
        AVP_S("{\"units\": [{\"name\": \"main/b\", \"functions\": [{\"name\": \"f\", \"metadata\": {\"virtual_address\": "
              "\"16\"}, \"from\": {\"fuzzy_match_percent\": 50}, \"to\": {\"fuzzy_match_percent\": 40}}]},"
              "{\"name\": \"main/a\", \"functions\": [{\"name\": \"f2\", \"metadata\": {\"virtual_address\": \"16\"}, "
              "\"to\": {\"fuzzy_match_percent\": 45}}, {\"name\": \"g\", \"from\": {}}]}]}"),
        &e);
    REQUIRE(cj);
    GateScores s = gate_func_scores(TA, cj);
    REQUIRE(s.n == 3);
    CHECK(s.v[0].has_from && s.v[0].from == 50.0 && s.v[0].to == 40.0);
    CHECK(s.v[2].has_from && s.v[2].from == 0.0 && !s.v[2].has_to);
    GateScores m = gate_merge_by_symbol(TA, &s);
    REQUIRE(m.n == 2);
    /* sorted by (unit, key): main/a first; address 16 merges a's f2 (to 45) with b's f (from 50, to 40) */
    CHECK(m.v[0].has_addr_int && m.v[0].addr_int == 16);
    CHECK(m.v[0].to == 45.0 && m.v[0].from == 50.0);
    CHECK_CZ(m.v[0].unit, "main/b");
    CHECK_CZ(m.v[0].name, "f2");
    CHECK_CZ(m.v[0].onto_name, "f");
    CHECK_EQ_I64(gate_json_function_regressions(cj), 1);
}

AVP_TEST(changes_text) {
    GateChangeRows rows = gate_parse_changes_text(
        TA, AVP_S("[1/2] x\n   fn_A | fuzzy_match | 100.00% ->  90.50%\n main/game/A | matched_code | 1.0% -> 2.0%\n"
                  "| Name | Type | Before | After |\n"));
    REQUIRE(rows.n == 2);
    CHECK_CZ(rows.v[0].name, "fn_A");
    CHECK(rows.v[0].from == 100.0 && rows.v[0].to == 90.5);
    CHECK(!gate_is_unit_row(&rows.v[0], NULL));
    CHECK(gate_is_unit_row(&rows.v[1], NULL));
}

AVP_TEST(file_index_lookup) {
    AvpJsonError e;
    AvpJson *d = avp_json_parse(TA,
                                AVP_S("{\"funcs\": [{\"name\": \"fA\", \"line\": 1, \"end\": 3, \"addr\": 4096, \"hash\": "
                                      "\"h1\"}, {\"name\": \"Foo::Bar\", \"line\": 5, \"end\": 7, \"addr\": null, "
                                      "\"hash\": \"h2\"}], \"unit_hash\": \"u\", \"rest_hash\": \"r\", \"parsed\": true}"),
                                &e);
    REQUIRE(d);
    GateFileIndex *fi = gate_file_index_new(TA, d);
    const char *basis;
    const AvpJson *f = gate_file_index_lookup(TA, fi, "zzz", true, 4096, &basis);
    CHECK(f && strcmp(basis, "addr") == 0);
    f = gate_file_index_lookup(TA, fi, "Bar__3FooFv", false, 0, &basis);
    CHECK(f && strcmp(basis, "name") == 0);
    f = gate_file_index_lookup(TA, fi, "__sinit_\\x_cpp", false, 0, &basis);
    CHECK(!f && strcmp(basis, "sinit") == 0);
    f = gate_file_index_lookup(TA, fi, "nothing", false, 0, &basis);
    CHECK(!f && strcmp(basis, "unit") == 0);
}

AVP_TEST_MAIN("gate")
