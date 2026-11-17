/* Pure unit tests of Phase 3 and the Phase 4 output on synthetic inputs: the solver's own control-flow forms
 * (else-after-return, merge-ifs), --apply and each of its refusals (applied into a temporary tree: no real source
 * file is touched), per-stop timing, and the two summary tables. */
#include "../../../tests/shsm_test_util.h"
#include "avp/test.h"
#include "solver.h"

/* ---- the solver's own forms ------------------------------------------------------------------------------------ */

/* The tries of sv_p3_own_forms on one function of a text (no compile: the filter only). */
static SvTries own_forms(const char *text, const char *symbol) {
    AvpArena *a = avp_arena();
    SvRun *run = AVP_NEW(a, SvRun);
    run->a = a;
    SvP2 *p = AVP_NEW(a, SvP2);
    p->run = run;
    p->text = avp_str(text);
    p->fn_symbol = p->symbol = symbol;
    p->deadline = avp_time() + 1000;
    p->x = lv_ctx_new(a, p->text, symbol);
    SvTries out = {0};
    SvGen g = {p, &out, avp_set_new(), lv_slice(p->x, p->x->span0, p->x->span1), 0, 0, NULL, 0};
    sv_p3_own_forms(&g, p->x);
    return out;
}

static const SvTry *find_form(const SvTries *t, const char *form, const char *needle) {
    for (size_t i = 0; i < t->n; i++)
        if (!strcmp(t->v[i].form, form) && avp_str_contains(t->v[i].text, avp_str(needle))) return &t->v[i];
    return NULL;
}

static size_t count_form(const SvTries *t, const char *form) {
    size_t n = 0;
    for (size_t i = 0; i < t->n; i++) n += !strcmp(t->v[i].form, form);
    return n;
}

AVP_TEST(bare_block_after_returning_if_becomes_else) {
    const char *src = "int f(int a, int b) {\n"
                      "    if (a) {\n"
                      "        b = 1;\n"
                      "        return b;\n"
                      "    }\n"
                      "    {\n"
                      "        int t = b;\n"
                      "        b = t + 1;\n"
                      "    }\n"
                      "    return b;\n"
                      "}\n";
    SvTries t = own_forms(src, "f");
    const SvTry *x = find_form(&t, "else-after-return", "} else {");
    REQUIRE(x);
    CHECK(!avp_str_contains(x->text, AVP_S("}\n    {\n        int t")));
    CHECK(avp_str_contains(x->text, AVP_S("    return b;\n}\n")));
    CHECK(!x->probe);
}

AVP_TEST(else_arm_after_returning_if_becomes_bare_block) {
    const char *src = "int f(int a, int b) {\n"
                      "    if (a) {\n"
                      "        return 1;\n"
                      "    } else {\n"
                      "        b = 2;\n"
                      "    }\n"
                      "    return b;\n"
                      "}\n";
    SvTries t = own_forms(src, "f");
    const SvTry *x = find_form(&t, "else-after-return", "    }\n    {\n        b = 2;");
    REQUIRE(x);
    CHECK(!avp_str_contains(x->text, AVP_S("else")));
}

AVP_TEST(no_else_form_when_the_if_does_not_return) {
    const char *src = "int f(int a, int b) {\n"
                      "    if (a) {\n"
                      "        b = 1;\n"
                      "    }\n"
                      "    {\n"
                      "        b = b + 1;\n"
                      "    }\n"
                      "    return b;\n"
                      "}\n";
    SvTries t = own_forms(src, "f");
    CHECK_EQ_U64(count_form(&t, "else-after-return"), 0);
}

AVP_TEST(consecutive_ifs_with_the_same_returning_body_merge) {
    const char *src = "int f(int a, int b) {\n"
                      "    if (a & 1) {\n"
                      "        return 0;\n"
                      "    }\n"
                      "\n"
                      "    if (b == 2) {\n"
                      "        return 0;\n"
                      "    }\n"
                      "    return a + b;\n"
                      "}\n";
    SvTries t = own_forms(src, "f");
    const SvTry *x = find_form(&t, "merge-ifs", "if (a & 1 || b == 2) {\n        return 0;\n    }\n    return a + b;");
    REQUIRE(x);
    CHECK(avp_str_contains(x->text, AVP_S("if (a & 1 || b == 2)")));
}

AVP_TEST(ifs_with_other_bodies_do_not_merge) {
    const char *src = "int f(int a, int b) {\n"
                      "    int r = 0;\n"
                      "    if (a & 1) {\n"
                      "        r = 1;\n"
                      "    }\n"
                      "    if (b == 2) {\n"
                      "        r = 1;\n"
                      "    }\n"
                      "    if (a) {\n"
                      "        return 1;\n"
                      "    }\n"
                      "    if (b) {\n"
                      "        return 2;\n"
                      "    }\n"
                      "    return r;\n"
                      "}\n";
    SvTries t = own_forms(src, "f");
    CHECK_EQ_U64(count_form(&t, "merge-ifs"), 0); /* an assignment body can fall through, and 1 differs from 2 */
}

AVP_TEST(or_condition_splits_into_two_returning_ifs) {
    const char *src = "int f(int a, int b) {\n"
                      "    if (a == 1 || b == 2) {\n"
                      "        return 7;\n"
                      "    }\n"
                      "    return b;\n"
                      "}\n";
    SvTries t = own_forms(src, "f");
    const SvTry *x = find_form(&t, "merge-ifs", "if (a == 1) {\n        return 7;\n    }\n\n    if (b == 2) {");
    REQUIRE(x);
}

/* ---- --apply and its refusals ---------------------------------------------------------------------------------- */

typedef struct {
    const char *root, *out;
    SvRecords recs;
} Tree;

static SvCandidate cand(const char *form, const char *text, bool exact, i64 before, i64 after) {
    SvCandidate c = {0};
    c.form = form;
    c.desc = "a form";
    c.text = avp_str(text);
    c.compiled = true;
    c.exact = exact;
    c.rows_before = before;
    c.rows_after = after;
    return c;
}

/* A temporary checkout with src/unit.cpp (and src/other.cpp), and a saved run of function `f` with candidates
 * 1 exact, 2 closer, 3 exact but breaks g, 4 closer with a rule finding, 5 not closer, 6 not compiled, and probe 7. */
static Tree make_tree(const char *name, bool from_case) {
    AvpArena *a = avp_arena();
    Tree t = {0};
    t.root = fx_dir(name);
    t.out = avp_join(a, t.root, "build", "solver");
    fx_write(t.root, "src/unit.cpp", "old text\n");
    fx_write(t.root, "src/other.cpp", "other text\n");
    SvRecord *r = sv_record_new(a, "main/unit", "f", SV_CONTROL_FLOW);
    r->source = "src/unit.cpp";
    r->source_sha1 = avp_sha1_hex(a, "old text\n", 9);
    r->from_case = from_case;
    avp_vec_push(&r->candidates, cand("merge-ifs", "new 1\n", true, 3, 0));
    avp_vec_push(&r->candidates, cand("control", "new 2\n", false, 3, 1));
    SvCandidate c3 = cand("expr", "new 3\n", true, 3, 0);
    avp_vec_push(&c3.broken, "g");
    avp_vec_push(&r->candidates, c3);
    SvCandidate c4 = cand("expr", "new 4\n", false, 3, 1);
    avp_vec_push(&c4.rules, "R17: a new fake form");
    avp_vec_push(&r->candidates, c4);
    avp_vec_push(&r->candidates, cand("expr", "new 5\n", false, 3, 3));
    SvCandidate c6 = cand("expr", "new 6\n", true, 3, 0);
    c6.compiled = false;
    avp_vec_push(&r->candidates, c6);
    SvProbe p = {0};
    p.lever = "flag -inline noauto";
    p.closes = true;
    avp_vec_push(&r->probes, p);
    avp_vec_push(&t.recs, r);
    sv_number_records(&t.recs);
    AVP_ASSERT(sv_apply_save(a, t.out, &t.recs));
    return t;
}

static bool file_is(const char *root, const char *rel, const char *text) {
    AvpArena *a = avp_arena();
    AvpBytes b = {0};
    if (!avp_read_all(a, avp_join(a, root, rel), &b)) return false;
    return b.n == strlen(text) && !memcmp(b.v, text, b.n);
}

static void refused(const Tree *t, i64 n, const char *needle) {
    AvpArena *a = avp_arena();
    SvApplied r = sv_apply(a, t->root, t->out, "f", n);
    CHECK_MSG(!r.ok, "candidate %lld was applied", (long long)n);
    CHECK_MSG(r.error && strstr(r.error, needle), "refusal %lld: '%s' does not mention '%s'", (long long)n,
              r.error ? r.error : "(none)", needle);
    CHECK(file_is(t->root, "src/unit.cpp", "old text\n")); /* a refusal writes nothing */
    CHECK(file_is(t->root, "src/other.cpp", "other text\n"));
}

AVP_TEST(numbering_counts_candidates_then_probes_per_function) {
    AvpArena *a = avp_arena();
    SvRecords recs = {0};
    SvRecord *r1 = sv_record_new(a, "u", "f", SV_INLINE), *r2 = sv_record_new(a, "u", "f", SV_INLINE),
             *r3 = sv_record_new(a, "u", "g", SV_REGISTER);
    avp_vec_push(&r1->candidates, cand("x", "t", true, 2, 0));
    avp_vec_push(&r1->probes, (SvProbe){0});
    avp_vec_push(&r2->candidates, cand("x", "t", true, 2, 0));
    avp_vec_push(&r2->probes, (SvProbe){0});
    avp_vec_push(&r3->candidates, cand("x", "t", true, 2, 0));
    avp_vec_push(&recs, r1);
    avp_vec_push(&recs, r2);
    avp_vec_push(&recs, r3);
    sv_number_records(&recs);
    CHECK_EQ_I64(r1->candidates.v[0].number, 1);
    CHECK_EQ_I64(r2->candidates.v[0].number, 2);
    CHECK_EQ_I64(r1->probes.v[0].number, 3);
    CHECK_EQ_I64(r2->probes.v[0].number, 4);
    CHECK_EQ_I64(r3->candidates.v[0].number, 1);
}

AVP_TEST(apply_refuses_what_the_verdict_rules_would_not_count) {
    Tree t = make_tree("solver-apply-refusals", false);
    refused(&t, 7, "probe");
    refused(&t, 5, "neither exact nor closer");
    refused(&t, 3, "breaks g");
    refused(&t, 4, "rule finding");
    refused(&t, 6, "did not compile");
    refused(&t, 99, "no candidate #99");
    refused(&t, 0, "no candidate #0");
    AvpArena *a = avp_arena();
    SvApplied nr = sv_apply(a, t.root, t.out, "never_ran", 1);
    CHECK(!nr.ok && strstr(nr.error, "no recorded run of never_ran"));
}

AVP_TEST(apply_refuses_a_case_run) {
    Tree t = make_tree("solver-apply-case", true);
    refused(&t, 1, "--case");
}

AVP_TEST(apply_refuses_a_dirty_source) {
    Tree t = make_tree("solver-apply-dirty", false);
    fx_write(t.root, "src/unit.cpp", "old text, edited since the run\n");
    AvpArena *a = avp_arena();
    SvApplied r = sv_apply(a, t.root, t.out, "f", 1);
    CHECK(!r.ok && strstr(r.error, "not the file the run read"));
    CHECK(file_is(t.root, "src/unit.cpp", "old text, edited since the run\n"));
    CHECK(file_is(t.root, "src/other.cpp", "other text\n"));
}

AVP_TEST(apply_writes_the_one_source_file_only) {
    Tree t = make_tree("solver-apply-ok", false);
    AvpArena *a = avp_arena();
    SvApplied r = sv_apply(a, t.root, t.out, "f", 2); /* the closer candidate */
    REQUIRE(r.ok);
    CHECK_CZ(r.unit, "main/unit");
    CHECK_CZ(r.source, "src/unit.cpp");
    CHECK_CZ(r.form, "control");
    CHECK_EQ_I64(r.rows_after, 1);
    CHECK(file_is(t.root, "src/unit.cpp", "new 2\n"));
    CHECK(file_is(t.root, "src/other.cpp", "other text\n"));
    /* the source changed: the same run no longer applies */
    SvApplied again = sv_apply(a, t.root, t.out, "f", 1);
    CHECK(!again.ok);
    CHECK(file_is(t.root, "src/unit.cpp", "new 2\n"));
}

AVP_TEST(a_later_run_replaces_the_saved_candidates_of_its_function) {
    Tree t = make_tree("solver-apply-replace", false);
    AvpArena *a = avp_arena();
    SvRecords recs = {0};
    SvRecord *r = sv_record_new(a, "main/unit", "f", SV_CONTROL_FLOW);
    r->source = "src/unit.cpp";
    r->source_sha1 = avp_sha1_hex(a, "old text\n", 9);
    avp_vec_push(&recs, r);
    sv_number_records(&recs);
    REQUIRE(sv_apply_save(a, t.out, &recs));
    SvApplied res = sv_apply(a, t.root, t.out, "f", 1);
    CHECK(!res.ok && strstr(res.error, "no recorded run of f"));
}

/* ---- timing and the tables ------------------------------------------------------------------------------------- */

AVP_TEST(timing_of_solved_stops) {
    AvpArena *a = avp_arena();
    SvRecords recs = {0};
    const double secs[] = {1.0, 9.0, 3.0, 5.0, 100.0};
    for (size_t i = 0; i < AVP_ARRAY_LEN(secs); i++) {
        SvRecord *r = sv_record_new(a, "u", avp_sprintf(a, "f%zu", i), SV_REGISTER);
        r->seconds = secs[i];
        r->ran_candidates = i != 4; /* the last stop ran nothing: not a solved stop */
        r->budget_hit = i == 1 || i == 4;
        avp_vec_push(&recs, r);
    }
    SvTiming t = sv_timing(&recs);
    CHECK_EQ_U64(t.stops, 5);
    CHECK_EQ_U64(t.solved, 4);
    CHECK_EQ_U64(t.budget_hit, 2);
    CHECK(t.max == 9.0);
    CHECK(t.median == 4.0); /* 1 3 5 9 */
    CHECK(t.total_solved == 18.0);
    SvRecords none = {0};
    t = sv_timing(&none);
    CHECK_EQ_U64(t.solved, 0);
    CHECK(t.median == 0.0 && t.max == 0.0);
}

static bool has_line(const AvpStrVec *l, const char *needle) {
    for (size_t i = 0; i < l->n; i++)
        if (avp_str_contains(l->v[i], avp_str(needle))) return true;
    return false;
}

AVP_TEST(tables_one_row_per_record_and_one_per_cause) {
    AvpArena *a = avp_arena();
    SvRecords recs = {0};
    SvRecord *m = sv_record_new(a, "main/u1", "fixed", SV_CONTROL_FLOW);
    m->cause = "control-flow: extra blocks in ours";
    m->rows_differing = 3;
    m->rows_retail = 168;
    avp_vec_push(&m->candidates, cand("merge-ifs", "t", true, 3, 0));
    SvRecord *d = sv_record_new(a, "main/u2", "declswap", SV_REGISTER);
    d->cause = "register: dwarf-order";
    SvCandidate dc = cand("decl", "t", false, 6, 2);
    dc.desc = "declare `a` before `b`";
    avp_vec_push(&d->candidates, dc);
    SvRecord *pr = sv_record_new(a, "main/u3", "probed", SV_INLINE);
    pr->cause = "inline X";
    SvProbe p = {0};
    p.lever = "flag -inline noauto";
    p.result = "closes";
    p.closes = true;
    avp_vec_push(&pr->probes, p);
    SvRecord *none = sv_record_new(a, "main/u4", "bare", SV_OTHER);
    avp_vec_push(&recs, m);
    avp_vec_push(&recs, d);
    avp_vec_push(&recs, pr);
    avp_vec_push(&recs, none);
    for (size_t i = 0; i < recs.n; i++) sv_assign_verdict(recs.v[i]);
    sv_number_records(&recs);
    SvCauses causes = sv_group_causes(a, &recs);
    AvpStrVec lines = {0};
    sv_tables(a, &recs, &causes, &lines);
    REQUIRE(lines.n > 0);
    CHECK(avp_str_contains(lines.v[0], AVP_S("table 1")));
    CHECK(has_line(&lines, "table 2"));
    CHECK(has_line(&lines, "#1 merge-ifs"));
    CHECK(has_line(&lines, "3/168"));
    /* the declaration-order candidate carries its marker in the table row */
    CHECK(has_line(&lines, "declaration order unevidenced (no sibling DWARF)"));
    CHECK(has_line(&lines, "probe #1 flag -inline noauto: closes"));
    CHECK(has_line(&lines, "MATCH 1"));
    CHECK(has_line(&lines, "CLOSER 1"));
    /* the tables end the output: the last line belongs to table 2 */
    CHECK(avp_str_contains(lines.v[lines.n - 1], AVP_S("bare")) || avp_str_contains(lines.v[lines.n - 1], AVP_S("other")) ||
          avp_str_contains(lines.v[lines.n - 1], AVP_S("inline X")) || avp_str_contains(lines.v[lines.n - 1], AVP_S("register")) ||
          avp_str_contains(lines.v[lines.n - 1], AVP_S("control-flow")));
    CHECK(sv_candidate_unevidenced(&dc));
    CHECK(!sv_candidate_unevidenced(&m->candidates.v[0]));
    AvpStr tsv = sv_causes_tsv(a, &causes);
    CHECK(avp_str_contains(tsv, AVP_S("\tMATCH 1\n")));
}

AVP_TEST_MAIN("solver_phase3")
