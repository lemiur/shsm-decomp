/* Pure unit tests of the decision solver on synthetic inputs: the Phase 0 rules (hand-built wall documents), the
 * rulings table, case files and their edits, verdicts, the cause table, flag groups. */
#include "avp/test.h"
#include "solver.h"

static AvpJson *parse(const char *text) {
    AvpJsonError err = {0};
    AvpJson *j = avp_json_parse(avp_arena(), avp_str(text), &err);
    AVP_ASSERT(j);
    return j;
}

/* ---- mention / retail-call stubs ---- */
typedef struct {
    const char *const *named; /* callee symbols the source names */
} Names;

static bool mention_stub(void *ctx, const char *callee, const char **via) {
    const Names *n = ctx;
    for (size_t i = 0; n->named[i]; i++)
        if (!strcmp(n->named[i], callee)) {
            *via = "";
            return true;
        }
    return false;
}

/* retail's helperA calls D */
static bool retail_calls_stub(void *ctx, const char *intermediate, const char *callee) {
    AVP_UNUSED(ctx);
    return !strcmp(intermediate, "helperA") && !strcmp(callee, "D");
}

static const char *const NONE[] = {NULL};

AVP_TEST(phase0_incomplete_is_not_started) {
    AvpJson *d = parse("{\"name\":\"f\",\"retail_name\":\"f\",\"wall\":\"call set\",\"verdict\":\"INCOMPLETE? compiled "
                       "is 0% of retail's size; MIXED WALL call set: ...\",\"calls\":{\"findings\":[{\"kind\":"
                       "\"missing-in-compiled\",\"callee\":\"x\"}]}}");
    Names n = {NONE};
    SvStop st = sv_classify_stop(avp_arena(), d, true, mention_stub, &n, NULL, NULL);
    CHECK(st.cls == SV_NOT_STARTED);
    CHECK_EQ_U64(st.decisions.n, 0);
}

AVP_TEST(phase0_not_emitted) {
    AvpJson *d = parse("{\"name\":null,\"retail_name\":\"g\",\"wall\":\"call set\",\"missing\":true,\"verdict\":\"WALL "
                       "call set: not emitted by the compiled object (inlined everywhere, or not defined)\"}");
    Names n = {NONE};
    SvStop st = sv_classify_stop(avp_arena(), d, false, mention_stub, &n, NULL, NULL);
    CHECK(st.cls == SV_NOT_STARTED);
    st = sv_classify_stop(avp_arena(), d, true, mention_stub, &n, NULL, NULL);
    CHECK(st.cls == SV_INLINE);
    REQUIRE(st.decisions.n == 1);
    CHECK_CZ(st.decisions.v[0].callee, "g");
    CHECK(st.decisions.v[0].retail_calls);
}

AVP_TEST(phase0_equivalent_is_register) {
    Names n = {NONE};
    AvpJson *d = parse("{\"name\":\"f\",\"retail_name\":\"f\",\"wall\":\"referent\",\"verdict\":\"EQUIVALENT (proved; "
                       "...)\"}");
    CHECK(sv_classify_stop(avp_arena(), d, true, mention_stub, &n, NULL, NULL).cls == SV_REGISTER);
    d = parse("{\"name\":\"f\",\"retail_name\":\"f\",\"wall\":\"register/schedule\",\"verdict\":\"EQUIVALENT-LOOKING: "
              "register/schedule only (3 rows)\"}");
    CHECK(sv_classify_stop(avp_arena(), d, true, mention_stub, &n, NULL, NULL).cls == SV_REGISTER);
}

AVP_TEST(phase0_wall_classes) {
    Names n = {NONE};
    struct {
        const char *wall;
        SvClass cls;
    } cases[] = {{"referent", SV_REFERENT},
                 {"control flow", SV_CONTROL_FLOW},
                 {"register/schedule", SV_REGISTER},
                 {"other", SV_OTHER},
                 {"call set", SV_OTHER}};
    for (size_t i = 0; i < AVP_ARRAY_LEN(cases); i++) {
        AvpJson *d = parse(avp_sprintf(avp_arena(),
                                       "{\"name\":\"f\",\"retail_name\":\"f\",\"wall\":\"%s\",\"verdict\":\"WALL "
                                       "x\",\"calls\":{\"findings\":[]}}",
                                       cases[i].wall));
        CHECK_MSG(sv_classify_stop(avp_arena(), d, true, mention_stub, &n, NULL, NULL).cls == cases[i].cls, "%s",
                  cases[i].wall);
    }
}

AVP_TEST(phase0_source_call_and_inline) {
    const char *const named[] = {"named_callee", NULL};
    Names n = {named};
    /* missing-in-compiled: not named -> source-call; named -> inline (ours expanded) */
    AvpJson *d = parse("{\"name\":\"f\",\"retail_name\":\"f\",\"wall\":\"call set\",\"verdict\":\"WALL call set\","
                       "\"calls\":{\"findings\":[{\"kind\":\"missing-in-compiled\",\"callee\":\"other\"},"
                       "{\"kind\":\"missing-in-compiled\",\"callee\":\"named_callee\"},"
                       "{\"kind\":\"missing-in-compiled\",\"callee\":\"named_callee\"}]}}");
    SvStop st = sv_classify_stop(avp_arena(), d, true, mention_stub, &n, NULL, NULL);
    REQUIRE(st.decisions.n == 2);
    CHECK_CZ(st.decisions.v[0].callee, "other");
    CHECK(st.decisions.v[0].cls == SV_SOURCE_CALL);
    CHECK(st.decisions.v[1].cls == SV_INLINE);
    CHECK(st.decisions.v[1].retail_calls);
    CHECK_EQ_I64(st.decisions.v[1].count, 2);
    /* extra-in-compiled: named -> inline (retail expanded) */
    d = parse("{\"name\":\"f\",\"retail_name\":\"f\",\"wall\":\"call set\",\"verdict\":\"WALL call set\",\"calls\":{"
              "\"findings\":[{\"kind\":\"extra-in-compiled\",\"callee\":\"named_callee\"},{\"kind\":"
              "\"extra-in-compiled\",\"callee\":\"ctor\"}]}}");
    st = sv_classify_stop(avp_arena(), d, true, mention_stub, &n, NULL, NULL);
    REQUIRE(st.decisions.n == 2);
    CHECK(st.decisions.v[0].cls == SV_INLINE && !st.decisions.v[0].retail_calls);
    CHECK(st.decisions.v[1].cls == SV_SOURCE_CALL && !st.decisions.v[1].retail_calls);
    /* inlined-in-compiled is never source-call (its model check decides) */
    d = parse("{\"name\":\"f\",\"retail_name\":\"f\",\"wall\":\"call set\",\"verdict\":\"WALL call set\",\"calls\":{"
              "\"findings\":[{\"kind\":\"inlined-in-compiled\",\"callee\":\"zz\"}]}}");
    st = sv_classify_stop(avp_arena(), d, true, mention_stub, &n, NULL, NULL);
    REQUIRE(st.decisions.n == 1);
    CHECK(st.decisions.v[0].cls == SV_INLINE && st.decisions.v[0].via == NULL);
}

AVP_TEST(phase0_different_is_split) {
    const char *const named[] = {"b", NULL};
    Names n = {named};
    AvpJson *d = parse("{\"name\":\"f\",\"retail_name\":\"f\",\"wall\":\"call set\",\"verdict\":\"WALL call set\","
                       "\"calls\":{\"findings\":[{\"kind\":\"different\",\"retail\":\"a\",\"compiled\":\"b\"}]}}");
    SvStop st = sv_classify_stop(avp_arena(), d, true, mention_stub, &n, NULL, NULL);
    REQUIRE(st.decisions.n == 2);
    CHECK_CZ(st.decisions.v[0].callee, "a");
    CHECK(st.decisions.v[0].retail_calls && st.decisions.v[0].cls == SV_SOURCE_CALL);
    CHECK_CZ(st.decisions.v[1].callee, "b");
    CHECK(!st.decisions.v[1].retail_calls && st.decisions.v[1].cls == SV_INLINE);
}

AVP_TEST(phase0_drops_callee_of_expanded_intermediate) {
    const char *const named[] = {"helperA", "D", NULL};
    Names n = {named};
    AvpJson *d = parse("{\"name\":\"f\",\"retail_name\":\"f\",\"wall\":\"call set\",\"verdict\":\"WALL call set\","
                       "\"calls\":{\"findings\":[{\"kind\":\"inlined-in-compiled\",\"callee\":\"helperA\"},"
                       "{\"kind\":\"extra-in-compiled\",\"callee\":\"D\"},{\"kind\":\"extra-in-compiled\",\"callee\":"
                       "\"E\"}]}}");
    SvStop st = sv_classify_stop(avp_arena(), d, true, mention_stub, &n, retail_calls_stub, NULL);
    REQUIRE(st.decisions.n == 2);
    CHECK_CZ(st.decisions.v[0].callee, "helperA");
    REQUIRE(st.decisions.v[0].explains.n == 1);
    CHECK_CZ(st.decisions.v[0].explains.v[0], "compiled calls D, retail does not");
    CHECK_CZ(st.decisions.v[1].callee, "E");
    /* without the intermediate evidence D stays */
    st = sv_classify_stop(avp_arena(), d, true, mention_stub, &n, NULL, NULL);
    CHECK_EQ_U64(st.decisions.n, 3);
}

/* ---- rulings ---- */
AVP_TEST(rulings_parse) {
    SvRulings t;
    const char *err = NULL;
    REQUIRE(sv_rulings_parse(avp_arena(),
                             AVP_S("# comment\n\nsurviving-source\tlibjpeg/\tIJG text\nlever-flag\t-inline noauto\n"
                                   "probe-flag\t-inline deferred\tnote one\tnote two\nconst-accessor\tA\tAC\n"),
                             &t, &err));
    REQUIRE(t.n == 4);
    CHECK(t.v[0].kind == SV_RULE_SURVIVING_SOURCE);
    CHECK_CZ(t.v[0].note, "IJG text");
    CHECK_EQ_U64(t.v[1].line, 4);
    CHECK_CZ(t.v[1].note, "");
    CHECK_CZ(t.v[2].note, "note one\tnote two");
    CHECK_CZ(t.v[3].arg2, "AC");
    CHECK(sv_rulings_surviving(&t, "main/libjpeg/jmemmgr", "libjpeg/jmemmgr") != NULL);
    CHECK(sv_rulings_surviving(&t, "main/fmod/fmod", "fmod/fmod") == NULL);
    CHECK(sv_rulings_is_lever_flag(&t, "-inline noauto"));
    CHECK(!sv_rulings_is_lever_flag(&t, "-inline deferred"));
    CHECK(!sv_rulings_parse(avp_arena(), AVP_S("bogus\tx\n"), &t, &err));
    CHECK_CZ(err, "solver.tsv:1: unknown kind 'bogus'");
    CHECK(!sv_rulings_parse(avp_arena(), AVP_S("# x\nconst-accessor\tA\n"), &t, &err));
    CHECK_CZ(err, "solver.tsv:2: const-accessor takes 2 argument(s) separated by tabs");
}

/* ---- case files ---- */
static const char CASE_TEXT[] = "# a case\n"
                                "unit main/fmod/fmod_memory\n"
                                "function set__Q24FMOD7MemPoolFiii\n"
                                "expect verdict MATCH\n"
                                "expect verdict-not CLOSER\n"
                                "find <<EOF\n"
                                "    a = 1;\n"
                                "    b = 2;\n"
                                "EOF\n"
                                "replace <<EOF\n"
                                "    a = b = 0;\n"
                                "EOF\n"
                                "find: int x;\n"
                                "replace: long x;\n";

AVP_TEST(case_parse) {
    SvCase c;
    const char *err = NULL;
    REQUIRE(sv_case_parse(avp_arena(), avp_str(CASE_TEXT), "t.case", &c, &err));
    CHECK_CZ(c.unit, "main/fmod/fmod_memory");
    CHECK_CZ(c.function, "set__Q24FMOD7MemPoolFiii");
    REQUIRE(c.expects.n == 2);
    CHECK_CZ(c.expects.v[1].key, "verdict-not");
    CHECK_CZ(c.expects.v[1].value, "CLOSER");
    REQUIRE(c.edits.n == 2);
    CHECK_STR(c.edits.v[0].find, "    a = 1;\n    b = 2;");
    CHECK_STR(c.edits.v[0].replace, "    a = b = 0;");
    CHECK_STR(c.edits.v[1].find, "int x;");
    CHECK_STR(c.edits.v[1].replace, "long x;");
    CHECK(!sv_case_parse(avp_arena(), AVP_S("unit u\nfunction f\nfind: x\n"), "t.case", &c, &err));
    CHECK_CZ(err, "t.case:3: find without a replace");
    CHECK(!sv_case_parse(avp_arena(), AVP_S("function f\n"), "t.case", &c, &err));
    CHECK(!sv_case_parse(avp_arena(), AVP_S("unit u\nfunction f\nfind <<END\nx\n"), "t.case", &c, &err));
    CHECK(!sv_case_parse(avp_arena(), AVP_S("unit u\nfunction f\nexpect colour red\n"), "t.case", &c, &err));
}

AVP_TEST(case_apply) {
    SvCase c;
    const char *err = NULL;
    REQUIRE(sv_case_parse(avp_arena(), avp_str(CASE_TEXT), "t.case", &c, &err));
    AvpStr out;
    REQUIRE(sv_case_apply(avp_arena(), &c, AVP_S("void f() {\n    a = 1;\n    b = 2;\n    int x;\n}\n"), &out, &err));
    CHECK_STR(out, "void f() {\n    a = b = 0;\n    long x;\n}\n");
    /* CRLF source: the "\n" of the case matches "\r\n" */
    REQUIRE(sv_case_apply(avp_arena(), &c, AVP_S("void f() {\r\n    a = 1;\r\n    b = 2;\r\n    int x;\r\n}\r\n"), &out,
                          &err));
    CHECK_STR(out, "void f() {\r\n    a = b = 0;\r\n    long x;\r\n}\r\n");
    /* not exactly once: absent, and twice */
    CHECK(!sv_case_apply(avp_arena(), &c, AVP_S("void f() { int x; }\n"), &out, &err));
    CHECK_CZ(err, "t.case:6: the find text occurs 0 times in the source (it must occur exactly once)");
    CHECK(!sv_case_apply(avp_arena(), &c, AVP_S("    a = 1;\n    b = 2;\n int x; int x;\n"), &out, &err));
    CHECK_CZ(err, "t.case:13: the find text occurs 2 times in the source (it must occur exactly once)");
}

/* ---- verdicts ---- */
AVP_TEST(verdicts) {
    AvpArena *a = avp_arena();
    SvRecord *r = sv_record_new(a, "u", "f", SV_NOT_STARTED);
    CHECK(sv_assign_verdict(r) == SV_SKIPPED);
    r = sv_record_new(a, "u", "f", SV_INLINE);
    CHECK(sv_assign_verdict(r) == SV_CONDITION_ONLY);
    r->model_gap = true;
    CHECK(sv_assign_verdict(r) == SV_MODEL_GAP);
    r = sv_record_new(a, "u", "f", SV_REGISTER);
    SvCandidate c = {0};
    c.compiled = true;
    c.rows_before = 5;
    c.rows_after = 3;
    avp_vec_push(&r->candidates, c);
    CHECK(sv_assign_verdict(r) == SV_CLOSER);
    r->candidates.v[0].exact = true;
    r->candidates.v[0].rows_after = 0;
    CHECK(sv_assign_verdict(r) == SV_MATCH);
    avp_vec_push(&r->candidates.v[0].rules, "R17: volatile");
    CHECK(sv_assign_verdict(r) == SV_CONDITION_ONLY);
    r = sv_record_new(a, "u", "f", SV_REGISTER);
    c.exact = true;
    c.rows_after = 0;
    c.rules = (AvpCzVec){0};
    avp_vec_push(&c.broken, "other_fn");
    avp_vec_push(&r->candidates, c);
    CHECK(sv_assign_verdict(r) == SV_NO_FORM); /* closes the target only by breaking another */
    r = sv_record_new(a, "u", "f", SV_INLINE);
    SvProbe p = {0};
    p.lever = "flag -inline noauto";
    p.closes = true;
    avp_vec_push(&r->probes, p);
    CHECK(sv_assign_verdict(r) == SV_NO_FORM);
    r = sv_record_new(a, "u", "f", SV_INLINE);
    r->surviving_source = true;
    CHECK(sv_assign_verdict(r) == SV_CONDITION_ONLY); /* phase not built */
    r->phase_built = true;
    CHECK(sv_assign_verdict(r) == SV_NO_FORM);
    r = sv_record_new(a, "u", "f", SV_REGISTER);
    r->control_failed = true;
    CHECK(sv_assign_verdict(r) == SV_MODEL_GAP);
}

/* ---- causes ---- */
AVP_TEST(cause_grouping) {
    AvpArena *a = avp_arena();
    SvRecords recs = {0};
    const char *rows[][2] = {{"f1", "inline X"}, {"f2", "inline Y"}, {"f3", "inline Y"}, {"f3", "inline Y"},
                             {"f4", NULL}};
    for (size_t i = 0; i < AVP_ARRAY_LEN(rows); i++) {
        SvRecord *r = sv_record_new(a, "u", rows[i][0], rows[i][1] ? SV_INLINE : SV_REGISTER);
        r->cause = rows[i][1];
        avp_vec_push(&recs, r);
    }
    SvCauses cs = sv_group_causes(a, &recs);
    REQUIRE(cs.n == 3);
    CHECK_CZ(cs.v[0].cause, "inline Y");
    CHECK_EQ_U64(cs.v[0].count, 3);
    REQUIRE(cs.v[0].functions.n == 2);
    CHECK_CZ(cs.v[0].functions.v[0], "f2");
    CHECK_CZ(cs.v[0].functions.v[1], "f3");
    CHECK_CZ(cs.v[1].cause, "inline X");
    CHECK_CZ(cs.v[2].cause, "register");
    AvpStr tsv = sv_causes_tsv(a, &cs);
    CHECK(avp_str_startswith(tsv, AVP_S("cause\tcount\tfunctions\tverdicts\ninline Y\t3\tf2, f3\tCONDITION-ONLY 3\n")));
}

/* ---- flag groups ---- */
AVP_TEST(flag_groups_and_unit_only) {
    AvpArena *a = avp_arena();
    static const char *const mine[] = {"-O4,p", "-inline", "auto", "-pragma", "cats off", "-lang=c", "-inline", "noauto"};
    static const char *const theirs[] = {"-O4,p", "-inline", "auto", "-pragma", "cats off", "-lang=c"};
    ShsmUnit u = {0}, o = {0};
    u.has_cflags = o.has_cflags = true;
    for (size_t i = 0; i < AVP_ARRAY_LEN(mine); i++) avp_vec_push(&u.cflags, avp_str(mine[i]));
    for (size_t i = 0; i < AVP_ARRAY_LEN(theirs); i++) avp_vec_push(&o.cflags, avp_str(theirs[i]));
    SvGroups g = sv_flag_groups(a, &u.cflags);
    REQUIRE(g.n == 5);
    CHECK_CZ(sv_group_text(a, &g.v[2]), "-pragma \"cats off\"");
    const ShsmUnit *others[] = {&o};
    SvGroups only = sv_unit_only_flags(a, &u, others, 1);
    REQUIRE(only.n == 1);
    CHECK_CZ(sv_group_text(a, &only.v[0]), "-inline noauto");
    CHECK_EQ_U64(sv_unit_only_flags(a, &u, others, 0).n, 0); /* nothing to compare with */
}

AVP_TEST(source_names_and_mentions) {
    AvpArena *a = avp_arena();
    CHECK_CZ(sv_source_name(a, "free__Q24FMOD7MemPoolFPvPCciUi"), "free");
    CHECK_CZ(sv_source_name(a, "__nw__FUlii"), "new");
    CHECK_CZ(sv_source_name(a, "__ct__12CDrawingTestFv"), "CDrawingTest");
    CHECK_CZ(sv_source_name(a, "RwCameraClone"), "RwCameraClone");
    CHECK(sv_source_name(a, "<indirect>") == NULL);
    SvSource *s = sv_source_parse(a, AVP_S("static int helper(int x) { return leaf(x); }\n"
                                           "int other(void) { return 3; }\n"
                                           "int target(int y) { return helper(y) + 1; }\n"));
    REQUIRE(s->ok);
    LvFnDef d;
    REQUIRE(sv_source_find(a, s, "target", &d));
    CHECK_EQ_I64(sv_source_line(s, d.head), 3);
    CHECK(!sv_source_find(a, s, "absent", &d));
    SvMentions m = sv_source_mentions(a, s, &d);
    REQUIRE(m.ok);
    AvpVal *v = avp_map_get(m.names, AVP_S("leaf"));
    REQUIRE(v);
    CHECK_CZ((const char *)v->p, "helper");
    v = avp_map_get(m.names, AVP_S("helper"));
    REQUIRE(v);
    CHECK_CZ((const char *)v->p, "");
    CHECK(avp_map_get(m.names, AVP_S("other")) == NULL);
}

AVP_TEST_MAIN("solver")
