/* fdiff: the tests of tools/mwcc_trace/test_fdiff.py, ported. The objdiff JSON fixtures are the Python test's trimmed
 * recordings for main/climax/EventHandler; the tool calls (build, objdiff, target check, which, subprocess) are
 * stubbed through FdiffEnv, so nothing builds or reads build/. */
#include "avp/test.h"
#include "fdiff.h"

#define INS(t) "{\"instruction\": {\"formatted\": \"" t "\"}}"
#define INSK(t, k) "{\"instruction\": {\"formatted\": \"" t "\"}, \"diff_kind\": \"" k "\"}"
#define GAP(k) "{\"diff_kind\": \"" k "\"}"

/* fn_801AA3C8__FPvll: register allocation differs (recorded at 95%) */
static const char DIFFERS[] =
    "{\"left\": {\"symbols\": [{\"name\": \"fn_801AA3C8__FPvll\", \"match_percent\": 95.0, \"instructions\": ["
    INSK("lwz r4, lbl_8052A574@sda21", "DIFF_ARG_MISMATCH") "," INSK("li r0, 0x0", "DIFF_ARG_MISMATCH") ","
    INSK("stwx r0, r3, r4", "DIFF_ARG_MISMATCH") "," INS("blr ") "]}]},"
    "\"right\": {\"symbols\": [{\"name\": \"fn_801AA3C8__FPvll\", \"match_percent\": 95.0, \"instructions\": ["
    INSK("lwz r0, lbl_8052A574@sda21", "DIFF_ARG_MISMATCH") "," INSK("li r4, 0x0", "DIFF_ARG_MISMATCH") ","
    INSK("stwx r4, r3, r0", "DIFF_ARG_MISMATCH") "," INS("blr ") "]}]}}";

/* __ct__12unk_801A92A8Fv: exact (recorded at 100%) */
#define CT "[" INS("li r0, 0x0") "," INS("stw r0, 0x0(r3)") "," INS("sth r0, 0x4(r3)") "," INS("sth r0, 0x6(r3)") "," \
           INS("blr ") "]"
#define CT_LEFT "{\"symbols\": [{\"name\": \"__ct__12unk_801A92A8Fv\", \"match_percent\": 100.0, \"instructions\": " CT "}]}"
static const char EXACT_DOC[] = "{\"left\": " CT_LEFT ", \"right\": " CT_LEFT "}";
static const char NO_RIGHT[] = "{\"left\": " CT_LEFT ", \"right\": {\"symbols\": []}}";

/* fn_801A9AE8__Fv (first rows, recorded at 96.07%): objdiff aligns the sides with gap entries */
static const char GAPPED[] =
    "{\"left\": {\"symbols\": [{\"name\": \"fn_801A9AE8__Fv\", \"match_percent\": 96.07229, \"instructions\": ["
    INS("mflr r0") "," INS("stw r0, 0x24(r1)") "," INS("stmw r26, 0x8(r1)") "," INSK("li r30, 0x0", "DIFF_DELETE") ","
    INS("lis r31, lbl_8047DA08@ha") "," INSK("li r27, 0x1", "DIFF_ARG_MISMATCH") "," GAP("DIFF_INSERT") ","
    INS("mr r28, r30") "," INS("bl fn_801A9940") "]}]},"
    "\"right\": {\"symbols\": [{\"name\": \"fn_801A9AE8__Fv\", \"match_percent\": 96.07229, \"instructions\": ["
    INS("mflr r0") "," INS("stw r0, 0x24(r1)") "," INS("stmw r26, 0x8(r1)") "," GAP("DIFF_DELETE") ","
    INS("lis r31, lbl_8047DA08@ha") "," INSK("li r30, 0x0", "DIFF_ARG_MISMATCH") ","
    INSK("li r27, 0x1", "DIFF_INSERT") "," INS("mr r28, r30") "," INS("bl fn_801A9940") "]}]}}";

static AvpJson *doc_of(const char *text) {
    AvpJsonError err;
    AvpJson *d = avp_json_parse(avp_arena(), avp_str(text), &err);
    AVP_ASSERT(d);
    return d;
}

/* ---- the stubbed environment (the Python test's Base) ---- */

typedef struct {
    const char *doc;
    FdiffTargets res;
    bool broken_targets;
    AvpStrVec built;
    /* Environment tests */
    bool which_none;
    i64 run_rc;
    const char *run_out, *run_err;
} Stub;

static void stub_build(void *ctx, AvpArena *a, AvpBuf *out, const char *unit) {
    (void)a, (void)out;
    Stub *s = ctx;
    avp_vec_push(&s->built, avp_str(unit));
}
static AvpJson *stub_objdiff(void *ctx, AvpArena *a, const char *unit, const char *sym) {
    (void)a, (void)unit, (void)sym;
    return doc_of(((Stub *)ctx)->doc);
}
static bool stub_targets(void *ctx, AvpArena *a, const char *unit, const char *sym, FdiffTargets *out) {
    (void)a, (void)unit, (void)sym;
    Stub *s = ctx;
    if (s->broken_targets) avp_throw(FDIFF_ENV_ERROR, "target check could not run (dtk missing)");
    *out = s->res;
    return true;
}
static char *stub_which(void *ctx, AvpArena *a, const char *name) {
    return ((Stub *)ctx)->which_none ? NULL : avp_strdup(a, name);
}
static bool stub_run(void *ctx, AvpArena *a, const char *const *argv, const char *cwd, i64 *rc, AvpStr *out,
                     AvpStr *err) {
    (void)a, (void)argv, (void)cwd;
    Stub *s = ctx;
    *rc = s->run_rc;
    *out = avp_str(s->run_out);
    *err = avp_str(s->run_err);
    return true;
}

static Stub base_stub(void) {
    Stub s = {0};
    s.doc = EXACT_DOC;
    s.res.checked = 3;
    return s;
}

static int run_main(const FdiffEnv *env, AvpStr *text, int n, const char *const *argv) {
    AvpArena *a = avp_arena();
    AvpStr args[8];
    for (int i = 0; i < n; i++) args[i] = avp_str(argv[i]);
    AvpArgs *r = avp_args_parse_list(fdiff_parser(), args, (size_t)n, false);
    AvpBuf out;
    avp_buf_init(&out, a);
    int rc = fdiff_run(env, a, r, &out);
    *text = avp_buf_str(&out);
    return rc;
}

#define MAIN(env, text, ...) \
    run_main((env), (text), (int)AVP_ARRAY_LEN(((const char *const[]){__VA_ARGS__})), (const char *const[]){__VA_ARGS__})

static FdiffEnv stub_env(Stub *s) {
    FdiffEnv e = {0};
    e.ctx = s;
    e.build_unit = stub_build;
    e.objdiff_json = stub_objdiff;
    e.target_result = stub_targets;
    return e;
}

static bool has(AvpStr text, const char *needle) { return avp_str_contains(text, avp_str(needle)); }

/* ---- ExitCodes ---- */

AVP_TEST(exact_is_0_and_builds_first) {
    Stub s = base_stub();
    FdiffEnv e = stub_env(&s);
    AvpStr text;
    CHECK_EQ_I64(MAIN(&e, &text, "main/climax/EventHandler", "__ct__12unk_801A92A8Fv"), 0);
    REQUIRE(s.built.n == 1);
    CHECK_STR(s.built.v[0], "climax/EventHandler");
    CHECK(has(text, "__ct__12unk_801A92A8Fv: 100.00%, 0 target mismatch(es) of 3 checked"));
    CHECK(avp_str_endswith(avp_str_rstrip(text), AVP_S("exact")));
}

AVP_TEST(no_build_skips_ninja) {
    Stub s = base_stub();
    FdiffEnv e = stub_env(&s);
    AvpStr text;
    CHECK_EQ_I64(MAIN(&e, &text, "climax/EventHandler", "__ct__12unk_801A92A8Fv", "--no-build"), 0);
    CHECK_EQ_I64(s.built.n, 0);
}

AVP_TEST(differing_function_is_1_with_marked_rows) {
    Stub s = base_stub();
    s.doc = DIFFERS;
    FdiffEnv e = stub_env(&s);
    AvpStr text;
    CHECK_EQ_I64(MAIN(&e, &text, "climax/EventHandler", "fn_801AA3C8__FPvll"), 1);
    CHECK(has(text, "  0 * lwz r4, lbl_8052A574@sda21"));
    CHECK(has(text, "differs"));
}

AVP_TEST(retail_symbol_missing_is_2) {
    Stub s = base_stub();
    FdiffEnv e = stub_env(&s);
    AvpStr text;
    CHECK_EQ_I64(MAIN(&e, &text, "climax/EventHandler", "fn_nosuch"), 2);
    CHECK(has(text, "error: fn_nosuch is not a symbol of the retail unit main/climax/EventHandler"));
}

AVP_TEST(compiled_symbol_missing_is_1) {
    Stub s = base_stub();
    s.doc = NO_RIGHT;
    FdiffEnv e = stub_env(&s);
    AvpStr text;
    CHECK_EQ_I64(MAIN(&e, &text, "climax/EventHandler", "__ct__12unk_801A92A8Fv"), 1);
    CHECK(has(text, "not in the compiled object"));
}

AVP_TEST(failed_target_check_is_2_unless_skipped) {
    Stub s = base_stub();
    s.broken_targets = true;
    FdiffEnv e = stub_env(&s);
    AvpStr text;
    CHECK_EQ_I64(MAIN(&e, &text, "climax/EventHandler", "__ct__12unk_801A92A8Fv"), 2);
    CHECK(has(text, "error: target check could not run"));
    CHECK_EQ_I64(MAIN(&e, &text, "climax/EventHandler", "__ct__12unk_801A92A8Fv", "--no-targets"), 0);
    CHECK(has(text, "(targets not checked)"));
}

AVP_TEST(unknown_flag_is_2) {
    AvpStr args[] = {AVP_S("u"), AVP_S("s"), AVP_S("--bogus")};
    AvpArgsExit ex = {0};
    CHECK(avp_args_try_parse(fdiff_parser(), args, 3, false, &ex) == NULL);
    CHECK_EQ_I64(ex.code, 2);
}

/* ---- Targets ---- */

AVP_TEST(matching_text_with_a_different_target_is_marked_and_fails) {
    Stub s = base_stub();
    FdiffTarget t = {1, 1, AVP_S("target"), AVP_S("lbl_80500000 resolves to 0x80500000, retail 0x80500010")};
    s.res.v = &t;
    s.res.n = 1;
    FdiffEnv e = stub_env(&s);
    AvpStr text;
    CHECK_EQ_I64(MAIN(&e, &text, "climax/EventHandler", "__ct__12unk_801A92A8Fv"), 1);
    CHECK(has(text, "  1 T stw r0, 0x0(r3)"));
    CHECK(has(text, "-> target: lbl_80500000 resolves to 0x80500000, retail 0x80500010"));
    CHECK(has(text, "1 target mismatch(es) of 3 checked"));
}

static const AvpJson *sym0(const char *doc, const char *side) {
    return avp_json_at(avp_json_getz(avp_json_getz(doc_of(doc), side), "symbols"), 0);
}

AVP_TEST(target_rows_map_through_objdiff_gaps) {
    /* retail row 6 (mr r28, r30) is objdiff row 7 on the left; compiled row 6 is row 7 on the right */
    AvpArena *a = avp_arena();
    FdiffRows rows = fdiff_rows_of(a, sym0(GAPPED, "left"), sym0(GAPPED, "right"));
    FdiffTarget ts[] = {{6, 6, AVP_S("target"), AVP_S("x")}, {7, 7, AVP_S("target"), AVP_S("call target")}};
    FdiffTargets res = {ts, 2, 3, 0};
    FdiffMarks m = fdiff_target_marks(a, &rows, &res);
    for (size_t k = 0; k < rows.n; k++) CHECK_EQ_I64(m.marks[k].n, (k == 7 || k == 8) ? 1 : 0);
    CHECK_STR(rows.v[8].fa, "bl fn_801A9940");
    CHECK_EQ_I64(m.loose.n, 0);
}

AVP_TEST(target_on_no_row_is_listed_apart) {
    AvpArena *a = avp_arena();
    FdiffRows rows = fdiff_rows_of(a, sym0(EXACT_DOC, "left"), sym0(EXACT_DOC, "right"));
    FdiffTarget t = {40, 40, AVP_S("target"), AVP_S("far")};
    FdiffTargets res = {&t, 1, 3, 0};
    FdiffMarks m = fdiff_target_marks(a, &rows, &res);
    for (size_t k = 0; k < rows.n; k++) CHECK_EQ_I64(m.marks[k].n, 0);
    CHECK_EQ_I64(m.loose.n, 1);
}

/* ---- render (beyond the Python tests: the context and the "..." separators) ---- */

AVP_TEST(render_keeps_two_rows_of_context) {
    AvpArena *a = avp_arena();
    FdiffRows rows = fdiff_rows_of(a, sym0(GAPPED, "left"), sym0(GAPPED, "right"));
    FdiffMarks m = {AVP_NEWN(a, AvpStrVec, rows.n), {0}};
    AvpStrVec lines = fdiff_render(a, &rows, &m, false);
    REQUIRE(lines.n >= 2);
    CHECK_STR(lines.v[0], "   ...");
    CHECK_STR(lines.v[1], "  1   stw r0, 0x24(r1)                   | stw r0, 0x24(r1)");
    AvpStrVec all = fdiff_render(a, &rows, &m, true);
    CHECK_EQ_I64(all.n, rows.n);
}

/* ---- Environment ---- */

static const char *temp_root(void) {
    AvpArena *a = avp_arena();
    char *d = avp_mkdtemp(a, "fdiff_test_", NULL, NULL);
    AVP_ASSERT(d);
    AVP_ASSERT(avp_makedirs(avp_path_join(a, d, "src"), true));
    AVP_ASSERT(avp_write_text(avp_join(a, d, "src", "u.c"), AVP_S(""), AVP_TM_UTF8));
    return d;
}

AVP_TEST(missing_ninja_is_an_environment_error) {
    Stub s = {0};
    s.which_none = true;
    FdiffEnv e = {0};
    e.ctx = &s;
    e.root = temp_root();
    e.which = stub_which;
    e.run = stub_run;
    AvpStr text;
    CHECK_EQ_I64(MAIN(&e, &text, "u", "s"), 2);
    CHECK(has(text, "ninja not found"));
    avp_rmtree(e.root, 0);
}

AVP_TEST(failed_build_is_an_environment_error) {
    Stub s = {0};
    s.run_rc = 1;
    s.run_out = "u.c:3: error: oops\n";
    s.run_err = "";
    FdiffEnv e = {0};
    e.ctx = &s;
    e.root = temp_root();
    e.which = stub_which;
    e.run = stub_run;
    AvpStr text;
    CHECK_EQ_I64(MAIN(&e, &text, "u", "s"), 2);
    CHECK(has(text, "ninja build/R5WEA4/src/u.o failed (exit 1)"));
    CHECK(has(text, "oops"));
    avp_rmtree(e.root, 0);
}

AVP_TEST(objdiff_failure_and_garbage_are_environment_errors) {
    Stub s = {0};
    s.run_rc = 1;
    s.run_out = "";
    s.run_err = "unit not found";
    FdiffEnv e = {0};
    e.ctx = &s;
    e.root = temp_root();
    e.which = stub_which;
    e.run = stub_run;
    AvpStr text;
    CHECK_EQ_I64(MAIN(&e, &text, "u", "s", "--no-build"), 2);
    CHECK(has(text, "objdiff-cli diff failed (exit 1): unit not found"));
    s.run_rc = 0;
    s.run_out = "panicked at ...";
    CHECK_EQ_I64(MAIN(&e, &text, "u", "s", "--no-build"), 2);
    CHECK(has(text, "objdiff-cli printed no JSON"));
    avp_rmtree(e.root, 0);
}

AVP_TEST(parse_json_messages) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        fdiff_parse_json(avp_arena(), AVP_S("[1]"));
        avp_try_end(&t);
        CHECK(false);
    } else {
        CHECK_CZ(t.msg, "objdiff-cli JSON is not an object");
    }
    if (AVP_TRY(&t)) {
        fdiff_parse_json(avp_arena(), AVP_S(""));
        avp_try_end(&t);
        CHECK(false);
    } else {
        CHECK_CZ(t.msg, "objdiff-cli printed no JSON (Expecting value: line 1 column 1 (char 0))");
    }
}

AVP_TEST_MAIN("fdiff")
