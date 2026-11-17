/* Pure unit tests of the trace conditions (solver_trace.c) on synthetic traces: the inline site text and condition for
 * each decision kind, the cause-table label, the reclassification of a gap after the inliner, the register condition
 * (one sweep, across sweeps, the order already holding, temporaries only), the inverted-pair reading and the
 * same-sweep filter of an unevidenced declaration swap. */
#include "avp/test.h"
#include "solver.h"

static SvInlSite site(const char *decision, const char *rule, const char *reason, i64 st, i64 max) {
    SvInlSite s = {0};
    s.callee = "X";
    s.decision = decision;
    s.rule = rule;
    s.detail = "";
    s.reason = reason;
    s.cost = s.limit = -1;
    s.statements = st;
    s.max_statements = max;
    s.level = !strcmp(decision, "expanded") ? 0 : -1;
    s.line = 12;
    return s;
}

static bool has(const char *s, const char *needle) { return s && strstr(s, needle) != NULL; }

AVP_TEST(inline_ours_expands_auto) {
    AvpArena *a = avp_arena();
    SvInlSite s = site("expanded", "level0", "auto-inline", 10, 15);
    const char *t = sv_inl_site_text(a, &s);
    CHECK(has(t, "expands X (auto-inline candidate: 10 statements, limit 15; level 0 has no size limit)"));
    const char *c = sv_inl_condition(a, &s, true);
    CHECK(has(c, "X must not be a candidate (more than 15 statements, or no auto-inline) or must not be defined before "
                 "this call"));
    CHECK_CZ(sv_inl_rule_label(a, &s), "auto-inline: under 16 statements");
}

AVP_TEST(inline_ours_calls_too_many_statements) {
    AvpArena *a = avp_arena();
    SvInlSite s = site("called", "not-candidate", "auto-statements", 16, 15);
    CHECK(has(sv_inl_site_text(a, &s), "calls X (not a candidate: 16 statements, limit 15)"));
    CHECK(has(sv_inl_condition(a, &s, false), "X must have at most 15 statements (ours 16)"));
    CHECK_CZ(sv_inl_rule_label(a, &s), "auto-inline: over 15 statements");
}

AVP_TEST(inline_keyword_definition_order_noauto_cost) {
    AvpArena *a = avp_arena();
    SvInlSite k = site("expanded", "level0", "inline keyword", -1, -1);
    CHECK(has(sv_inl_site_text(a, &k), "candidate: inline keyword"));
    CHECK(has(sv_inl_condition(a, &k, true), "must not carry the inline keyword"));
    CHECK_CZ(sv_inl_rule_label(a, &k), "inline keyword");
    SvInlSite d = site("called", "not-candidate", "not-yet-defined", -1, -1);
    CHECK(has(sv_inl_condition(a, &d, false), "X must be defined before this call"));
    CHECK_CZ(sv_inl_rule_label(a, &d), "definition order");
    SvInlSite n = site("called", "not-visited", "noauto", -1, -1);
    CHECK(has(sv_inl_site_text(a, &n), "no inline keyword and auto-inline is off"));
    CHECK(has(sv_inl_condition(a, &n, false), "must carry the inline keyword"));
    SvInlSite c = site("called", "level1-cost", "level1-cost", 3, 15);
    c.cost = 52;
    c.limit = 40;
    c.level = 1;
    CHECK(has(sv_inl_site_text(a, &c), "level 1: cost 52, limit 40"));
    CHECK(has(sv_inl_condition(a, &c, false), "cost at level 1 must be at most 40 (ours 52)"));
    /* a number the trace did not give is said so */
    SvInlSite u = site("expanded", "level0", "auto-inline", -1, -1);
    CHECK(has(sv_inl_site_text(a, &u), "statements not read"));
    CHECK(has(sv_inl_condition(a, &u, true), "the statement limit was not read"));
}

AVP_TEST(inline_gap_reclassification) {
    AvpArena *a = avp_arena();
    SvInlGap g = {"after-inliner", 15, 15, 11, 15, ""};
    CHECK_EQ_I64(sv_inl_gap_kind(&g), 1);
    CHECK(has(sv_inl_gap_counts(a, &g), "trace 15, post-inline IR 15, object 11, retail 15"));
    g.stage = "simple-optimizer";
    CHECK_EQ_I64(sv_inl_gap_kind(&g), 1);
    g.retail_calls = 12; /* the inliner's count is not retail's: not reclassified */
    CHECK_EQ_I64(sv_inl_gap_kind(&g), 2);
    static const char *const MODEL[] = {"trace-vs-ir", "trace-vs-object", "trace"};
    for (int i = 0; i < 3; i++) {
        SvInlGap m = {MODEL[i], 15, 15, 11, 15, ""};
        CHECK_EQ_I64(sv_inl_gap_kind(&m), 0);
    }
}

static SvRaNode node(const char *name, char kind, int index, int sweep, int sel, int reg) {
    SvRaNode n = {0};
    n.name = name;
    n.kind = kind;
    n.index = n.root = index;
    n.sweep = sweep;
    n.push_pos = index - 32;
    n.select_pos = sel;
    n.degree = 30;
    n.push_degree = 12;
    n.push_how = "low";
    n.cost = 7;
    n.reg = reg;
    n.retail_reg = -1;
    n.def_at = -1;
    return n;
}

AVP_TEST(register_one_sweep) {
    AvpArena *a = avp_arena();
    SvRaFacts f = {0};
    f.cls = MWT_RC_GPR;
    f.K = 29;
    f.var = node("a", 'l', 40, 1, 1, 30);
    f.var.retail_reg = 31;
    f.holder = node("b", 'l', 45, 1, 0, 31);
    f.have_holder = true;
    const char *shape = NULL;
    const char *c = sv_ra_condition(a, &f, &shape);
    CHECK_CZ(shape, "one sweep");
    CHECK(has(c, "retail has `a` in r31"));
    CHECK(has(c, "`a` must be coloured before `b`"));
    CHECK(has(c, "in ours both are pushed in sweep 1, `b` has the higher index (45 against 40)"));
    CHECK(has(c, "declaring `a` before `b`")); /* the declaration-order rule, stated within one sweep only */
    CHECK(has(c, "`a` (node 40, local): sweep 1"));
}

AVP_TEST(register_across_sweeps) {
    AvpArena *a = avp_arena();
    SvRaFacts f = {0};
    f.cls = MWT_RC_GPR;
    f.K = 29;
    f.var = node("x", 'l', 40, 1, 1, 30);
    f.var.retail_reg = 31;
    f.holder = node("this", 'p', 32, 2, 0, 31);
    f.have_holder = true;
    const char *shape = NULL;
    const char *c = sv_ra_condition(a, &f, &shape);
    CHECK_CZ(shape, "across sweeps");
    CHECK(has(c, "`this` is pushed in sweep 2"));
    CHECK(has(c, "the declaration order of the locals cannot move it"));
    CHECK(!has(c, "declaring"));
}

AVP_TEST(register_order_holds_and_temporaries) {
    AvpArena *a = avp_arena();
    SvRaFacts f = {0};
    f.cls = MWT_RC_GPR;
    f.K = 29;
    f.var = node("x", 'l', 45, 1, 0, 30);
    f.var.retail_reg = 31;
    f.holder = node("y", 'l', 40, 1, 1, 31);
    f.have_holder = true;
    const char *shape = NULL;
    const char *c = sv_ra_condition(a, &f, &shape);
    CHECK_CZ(shape, "order holds");
    CHECK(has(c, "is already coloured first"));
    SvRaNode x = node(NULL, 't', 50, 1, 2, 3), y = node(NULL, 't', 51, 1, 3, 4);
    x.def_op = "addi";
    x.def_at = 12;
    y.def_op = "lwz";
    y.def_at = 15;
    const char *t = sv_ra_temps_condition(a, MWT_RC_GPR, 3, 4, &x, 1, &y, 2);
    CHECK(has(t, "temporaries only: ours has r3 where retail has r4"));
    CHECK(has(t, "first defined by `addi` at instruction 12"));
    CHECK(has(t, "first defined by `lwz` at instruction 15"));
    CHECK(has(t, "first of 2 nodes"));
    CHECK(has(t, "no named variable differs"));
}

AVP_TEST(same_sweep_filter) {
    AvpArena *a = avp_arena();
    static const char *const FROM[] = {"a", "b", "c"}, *const TO[] = {"b", "a", "c"};
    AvpCzVec pairs = sv_inverted_pairs(a, FROM, 3, TO, 3);
    CHECK_EQ_I64((i64)pairs.n, 1);
    CHECK_CZ(pairs.v[0], "a\x1f" "b");
    /* a synthetic pass: a and b in sweep 1, c in sweep 2 */
    MwtRaPass p = {0};
    p.nreal = 32;
    const char *names[] = {"c", "b", "a"};
    int sweeps[] = {2, 1, 1};
    for (int i = 0; i < 3; i++) {
        MwtRaNode n = {0};
        n.index = n.alias = 32 + i;
        n.name = avp_str(names[i]);
        n.kind = 'l';
        n.push_sweep = sweeps[i];
        avp_vec_push(&p.nodes, n);
    }
    CHECK_EQ_I64(sv_swap_same_sweep(&p, &pairs), 1);
    static const char *const TO2[] = {"c", "a", "b"};
    AvpCzVec apart = sv_inverted_pairs(a, FROM, 3, TO2, 3); /* a-c, b-c: across sweeps only */
    CHECK_EQ_I64((i64)apart.n, 2);
    CHECK_EQ_I64(sv_swap_same_sweep(&p, &apart), 0);
    AvpCzVec none = {0};
    avp_vec_push(&none, "q\x1f" "r");
    CHECK_EQ_I64(sv_swap_same_sweep(&p, &none), -1); /* the trace has neither: kept */
    /* a coalesced node takes its root's sweep */
    p.nodes.v[0].alias = 33; /* c into b */
    CHECK_EQ_I64(sv_swap_same_sweep(&p, &apart), 1);
}

AVP_TEST_MAIN("solver_trace")
