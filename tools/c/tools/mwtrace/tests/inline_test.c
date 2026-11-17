/* mwtrace inline without the compiler: the log parser, the decisions and rules it gives, the chain attribution, the
 * self-check against an object on synthetic data (including a disagreement that must be TRACE-GAP), the #line
 * mapping of statement lines, and the per-build spec expansion. */
#include "avp/test.h"
#include "mwt_inline.h"

/* A log in the spec's line forms: caller 0x100 (link C1) calls leaf 0x200 (auto, expanded at level 0), big 0x300
 * (16 statements: not a candidate) twice, kw 0x400 (inline keyword, kind 3: queued and expanded as statements) whose
 * body calls ext 0x500 (no body here), and va 0x600 (inline keyword, varargs: kind 0). */
static const char *LOG =
    "compiler: Wii/1.0 (tools/compilers/Wii/1.0/mwcceppc.exe)\n"
    "DEF 0x200 0x80000 1 0 leaf\n"
    "KIND 0x200 6 body\n"
    "AUTO 0x200 ok 1 0\n"
    "DEFR 0x200 1 0x40000c02\n"
    "DEF 0x300 0x80000 1 0 big\n"
    "KIND 0x300 6 body\n"
    "AUTO 0x300 fail-count 16 15\n"
    "DEFR 0x300 0 0x40000402\n"
    "DEF 0x400 0x80010 1 0 kw\n"
    "DEFR 0x400 1 0x40000402\n"
    "BODY 0x400 0x500 3 0 ext\n"
    "KIND 0x400 3 body\n"
    "DEF 0x600 0x80010 1 0 va\n"
    "DEFR 0x600 1 0x40000402\n"
    "KIND 0x600 0 varargs\n"
    "CUR 0x100 1 15 0 0 0 0 0\n"
    "CHK 0xe1 0x100 0x200 0 0x80000 3 0x40000c02 0x0 leaf\n"
    "FD 2 6\n"
    "R pass\n"
    "XP 0xe1 1\n"
    "CHK 0xe2 0x100 0x300 0 0x80000 3 0x40000402 0x0 big\n"
    "R not-candidate\n"
    "CHK 0xe3 0x100 0x400 0 0x80010 3 0x40000402 0x0 kw\n"
    "FD 7 3\n"
    "R pass\n"
    "Q 0xe3 0\n"
    "XP 0xe3 0\n"
    "CHK 0xe4 0x100 0x300 0 0x80000 3 0x40000402 0x0 big\n"
    "R not-candidate\n"
    "CHK 0xe6 0x100 0x600 0 0x80010 3 0x40000402 0x0 va\n"
    "FD 1 0\n"
    "R pass\n"
    "XP 0xe6 0\n"
    "XS 0xe3\n"
    "CHK 0xe2 0x100 0x300 1 0x80000 3 0x40000402 0x0 big\n"
    "R not-candidate\n"
    "CHK 0xe5 0x100 0x500 1 0x80000 3 0x40000000 0x0 ext\n"
    "R not-candidate\n"
    "CHK 0xe4 0x100 0x300 1 0x80000 3 0x40000402 0x0 big\n"
    "R not-candidate\n"
    "CHK 0xe6 0x100 0x600 1 0x80010 3 0x40000402 0x0 va\n"
    "FD 1 0\n"
    "R pass\n"
    "XP 0xe6 0\n"
    "CI 0x100\n"
    "FC 0x300 3 0x0 big\n"
    "FC 0x500 3 0x0 ext\n"
    "FC 0x300 3 0x0 big\n"
    "FC 0x600 3 0x0 va\n"
    "CIE\n"
    "KEEP 0x100\n"
    "LN 0x100 C1\n"
    "LN 0x200 leaf__Fi\n"
    "LN 0x300 big__Fi\n"
    "LN 0x500 ext__Fi\n"
    "LN 0x600 va__Fie\n";

static MwtObjFuncs objs_with(AvpArena *a, const char *const *targets, size_t n) {
    MwtObjFuncs fs = {0};
    MwtObjFunc f = {AVP_S("C1"), {0}};
    for (size_t i = 0; i < n; i++) avp_vec_push(&f.calls, ((MwtCall){(u32)(4 * i), avp_str(targets[i])}));
    avp_vec_push(&fs, f);
    (void)a;
    return fs;
}

static const MwtSite *find_site(const MwtResult *r, const char *caller, const char *callee, int ordinal) {
    for (size_t ci = 0; ci < r->callers.n; ci++) {
        const MwtCaller *c = &r->callers.v[ci];
        if (!avp_str_eqz(c->caller, caller)) continue;
        const MwtSites *ss = c->visited ? &c->visit->sites : &c->synth;
        for (size_t i = 0; i < ss->n; i++)
            if (avp_str_eqz(ss->v[i].callee_name, callee) && ss->v[i].ordinal == ordinal) return &ss->v[i];
    }
    return NULL;
}

AVP_TEST(parse_reads_definitions_and_visits) {
    AvpArena *a = avp_arena();
    MwtTrace t;
    mwt_inline_parse(a, avp_str(LOG), &t);
    CHECK_STR(t.build, "Wii/1.0");
    REQUIRE(t.visits.n == 1);
    const MwtVisit *v = &t.visits.v[0];
    CHECK_EQ_U64(v->cur, 0x100);
    CHECK_EQ_I64(v->maxauto, 15);
    CHECK_EQ_I64(v->max_level, 1);
    CHECK(v->has_ir && v->kept && !v->reverted);
    CHECK_EQ_U64(v->ir.n, 4);
    CHECK_EQ_U64(v->sites.n, 6); /* leaf, big x2, kw, va, and ext from kw's body */
    AvpVal *d = avp_imap_get(t.defs, 0x300);
    REQUIRE(d);
    const MwtDef *big = d->p;
    CHECK_EQ_I64(big->auto_status, 2);
    CHECK_EQ_I64(big->auto_count, 16);
    CHECK_EQ_I64(big->auto_value, 15);
    const MwtDef *kw = avp_imap_get(t.defs, 0x400)->p;
    CHECK_EQ_U64(kw->body.n, 1);
    CHECK_EQ_I64(kw->kind, 3);
    CHECK_EQ_U64(t.nchecks, 9);
}

AVP_TEST(decisions_rules_and_numbers) {
    AvpArena *a = avp_arena();
    MwtTrace t;
    mwt_inline_parse(a, avp_str(LOG), &t);
    const char *calls[] = {"big__Fi", "ext__Fi", "big__Fi", "va__Fie"};
    MwtObjFuncs fs = objs_with(a, calls, 4);
    MwtResult r;
    mwt_inline_analyse(a, &t, &fs, NULL, &r);
    const MwtSite *leaf = find_site(&r, "C1", "leaf", 1);
    REQUIRE(leaf);
    CHECK_STR(leaf->decision, "expanded");
    CHECK_STR(leaf->rule, "level0");
    CHECK_EQ_I64(leaf->cost, 2);
    const MwtSite *big = find_site(&r, "C1", "big", 2);
    REQUIRE(big);
    CHECK_STR(big->decision, "called");
    CHECK_STR(big->rule, "not-candidate");
    CHECK(avp_str_startswith(big->detail, AVP_S("auto-statements")));
    CHECK_EQ_I64(big->cost, 16);
    CHECK_EQ_I64(big->limit, 15);
    const MwtSite *kw = find_site(&r, "C1", "kw", 1);
    REQUIRE(kw);
    CHECK(kw->expanded && kw->by_statements);
    const MwtSite *va = find_site(&r, "C1", "va", 1);
    REQUIRE(va);
    CHECK_STR(va->rule, "kind0");
    CHECK(avp_str_contains(va->detail, AVP_S("varargs")));
    const MwtSite *ext = find_site(&r, "C1", "ext", 1);
    REQUIRE(ext);
    CHECK_STR(ext->detail, "no-body: not defined in this unit");
    CHECK(ext->via >= 0);
    CHECK(ext->via >= 0 && avp_str_eqz(r.callers.v[0].visit->sites.v[ext->via].callee_name, "kw"));
    CHECK_EQ_U64(r.gaps.n, 0);
}

AVP_TEST(selfcheck_object_with_another_call_is_trace_gap) {
    AvpArena *a = avp_arena();
    MwtTrace t;
    mwt_inline_parse(a, avp_str(LOG), &t);
    /* leaf left as a call in the object although the trace expanded it */
    const char *calls[] = {"big__Fi", "ext__Fi", "big__Fi", "va__Fie", "leaf__Fi"};
    MwtObjFuncs fs = objs_with(a, calls, 5);
    MwtResult r;
    mwt_inline_analyse(a, &t, &fs, NULL, &r);
    REQUIRE(r.gaps.n >= 1);
    CHECK_STR(r.gaps.v[0].caller, "C1");
    CHECK_STR(r.gaps.v[0].callee, "leaf__Fi");
    CHECK_EQ_I64(r.gaps.v[0].trace_calls, 0);
    CHECK_EQ_I64(r.gaps.v[0].object_calls, 1);
    /* and one call fewer than the trace leaves: the IR has both, so the stage is after-inliner */
    const char *fewer[] = {"big__Fi", "ext__Fi", "va__Fie"};
    MwtObjFuncs f2 = objs_with(a, fewer, 3);
    MwtTrace t2;
    mwt_inline_parse(a, avp_str(LOG), &t2);
    MwtResult r2;
    mwt_inline_analyse(a, &t2, &f2, NULL, &r2);
    REQUIRE(r2.gaps.n == 1);
    CHECK_STR(r2.gaps.v[0].callee, "big__Fi");
    CHECK_STR(r2.gaps.v[0].stage, "after-inliner");
    CHECK_EQ_I64(r2.gaps.v[0].trace_calls, 2);
    CHECK_EQ_I64(r2.gaps.v[0].ir_calls, 2);
    CHECK_EQ_I64(r2.gaps.v[0].object_calls, 1);
}

AVP_TEST(selfcheck_ir_call_no_check_saw_is_trace_gap) {
    AvpArena *a = avp_arena();
    /* the IR has a third call of big, no check saw it and no body expanded at the last pass explains it */
    AvpStr log = avp_str_replace(a, avp_str(LOG), AVP_S("FC 0x600 3 0x0 va\n"), AVP_S("FC 0x600 3 0x0 va\nFC 0x300 3 0x0 big\n"), 1);
    MwtTrace t;
    mwt_inline_parse(a, log, &t);
    const char *calls[] = {"big__Fi", "ext__Fi", "big__Fi", "va__Fie", "big__Fi"};
    MwtObjFuncs fs = objs_with(a, calls, 5);
    MwtResult r;
    mwt_inline_analyse(a, &t, &fs, NULL, &r);
    REQUIRE(r.gaps.n >= 1);
    CHECK_STR(r.gaps.v[0].stage, "trace-vs-ir");
    CHECK_STR(r.gaps.v[0].callee, "big__Fi");
}

AVP_TEST(last_pass_bodies_are_accounted_from_the_ir) {
    AvpArena *a = avp_arena();
    /* kw expanded at the last level (1): its call of ext is never checked, the IR has it */
    const char *log =
        "DEF 0x400 0x80010 1 0 kw\nDEFR 0x400 1 0x40000402\nBODY 0x400 0x500 3 0 ext\nKIND 0x400 6 body\n"
        "DEF 0x200 0x80000 1 0 mid\nDEFR 0x200 1 0x40000c02\nKIND 0x200 6 body\n"
        "CUR 0x100 1 15 0 0 0 0 0\n"
        "CHK 0xe1 0x100 0x200 0 0x80000 3 0x40000c02 0x0 mid\nFD 3 6\nR pass\nXP 0xe1 1\n"
        "CHK 0xe2 0x100 0x400 1 0x80010 3 0x40000402 0x0 kw\nFD 2 6\nR pass\nXP 0xe2 1\n"
        "CI 0x100\nFC 0x500 3 0x0 ext\nCIE\nKEEP 0x100\nLN 0x100 C1\nLN 0x500 ext__Fi\n";
    MwtTrace t;
    mwt_inline_parse(a, avp_str(log), &t);
    const char *calls[] = {"ext__Fi"};
    MwtObjFuncs fs = objs_with(a, calls, 1);
    MwtResult r;
    mwt_inline_analyse(a, &t, &fs, NULL, &r);
    CHECK_EQ_U64(r.gaps.n, 0);
    const MwtSite *kw = find_site(&r, "C1", "kw", 1);
    REQUIRE(kw);
    CHECK_STR(kw->rule, "level1-cost");
    CHECK_EQ_I64(kw->cost, 2);
    CHECK_EQ_I64(kw->limit, 40);
    const MwtSite *ext = find_site(&r, "C1", "ext__Fi", 1);
    REQUIRE(ext);
    CHECK_STR(ext->rule, "last-pass");
    CHECK(ext->unchecked);
    CHECK(ext->via >= 0);
}

AVP_TEST(level_cost_refusal_and_growth_revert) {
    AvpArena *a = avp_arena();
    const char *log =
        "DEF 0x200 0x80000 1 0 rec\nDEFR 0x200 1 0x40000802\nKIND 0x200 6 body\n"
        "CUR 0x100 1 15 0 0 0 0 0\n"
        "CHK 0xe1 0x100 0x200 0 0x80000 3 0x40000802 0x0 rec\nFD 22 6\nR pass\nXP 0xe1 1\n"
        "CHK 0xe2 0x100 0x200 1 0x80000 3 0x40000802 0x0 rec\nFD 22 6\nR pass\nXP 0xe2 1\n"
        "CHK 0xe3 0x100 0x200 2 0x80000 3 0x40000802 0x0 rec\nFD 22 6\nR pass\nXP 0xe3 1\n"
        "CHK 0xe4 0x100 0x200 3 0x80000 3 0x40000802 0x0 rec\nFD 22 6\nR level3-cost\n"
        "REVERT 0x100 5000 30000\nLN 0x100 C1\nLN 0x200 rec__Fi\n";
    MwtTrace t;
    mwt_inline_parse(a, avp_str(log), &t);
    MwtResult r;
    mwt_inline_analyse(a, &t, NULL, NULL, &r);
    REQUIRE(r.callers.n == 1);
    const MwtVisit *v = r.callers.v[0].visit;
    CHECK(v->reverted);
    /* the level-0 site is called again (the growth test undid the caller), the later ones are discarded */
    CHECK_STR(v->sites.v[0].rule, "growth-revert");
    CHECK_STR(v->sites.v[0].decision, "called");
    CHECK_EQ_I64(v->sites.v[0].cost, 30000);
    CHECK_EQ_I64(v->sites.v[0].limit, 5000 + 5000 / 4);
    CHECK(v->sites.v[1].discarded && v->sites.v[3].discarded);
    /* without the revert the level-3 site is the level-cost refusal */
    MwtTrace t2;
    AvpStr log2 = avp_str_replace(a, avp_str(log), AVP_S("REVERT 0x100 5000 30000\n"), AVP_S("KEEP 0x100\n"), 1);
    mwt_inline_parse(a, log2, &t2);
    MwtResult r2;
    mwt_inline_analyse(a, &t2, NULL, NULL, &r2);
    const MwtSite *s3 = &r2.callers.v[0].visit->sites.v[3];
    CHECK_STR(s3->rule, "level3-cost");
    CHECK_EQ_I64(s3->cost, 22);
    CHECK_EQ_I64(s3->limit, 12);
    CHECK_STR(r2.callers.v[0].visit->sites.v[2].rule, "level2-cost");
    CHECK_EQ_I64(r2.callers.v[0].visit->sites.v[2].limit, 28);
}

AVP_TEST(unvisited_caller_calling_a_candidate_is_trace_gap) {
    AvpArena *a = avp_arena();
    const char *log = "DEF 0x200 0x80010 1 0 kw\nDEFR 0x200 1 0x40000402\nKIND 0x200 6 body\nLN 0x200 kw__Fv\n";
    MwtTrace t;
    mwt_inline_parse(a, avp_str(log), &t);
    const char *calls[] = {"kw__Fv"};
    MwtObjFuncs fs = objs_with(a, calls, 1);
    MwtResult r;
    mwt_inline_analyse(a, &t, &fs, NULL, &r);
    REQUIRE(r.gaps.n == 1);
    CHECK_STR(r.gaps.v[0].caller, "C1");
    CHECK_STR(r.gaps.v[0].callee, "kw__Fv");
}

AVP_TEST(debugger_errors_make_the_trace_incomplete) {
    AvpArena *a = avp_arena();
    MwtTrace t;
    mwt_inline_parse(a, AVP_S("HANDLER ERR TypeError: x (spec:1, bp 0x1; this handler is stopped)\n"), &t);
    MwtResult r;
    mwt_inline_analyse(a, &t, NULL, NULL, &r);
    REQUIRE(r.gaps.n == 1);
    CHECK_STR(r.gaps.v[0].stage, "trace");
}

/* The line of a site from a log whose statement is at the given offset of path. */
static i64 line_at(AvpArena *a, const char *path, size_t offset) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_printf(&b, "FILE 0x9 %s\nCUR 0x100 1 15 0 0 0 0 0\nSTMT 0x1 %zu 0x9\n"
                       "CHK 0xe1 0x100 0x200 0 0x0 3 0x0 0x0 f\nR not-candidate\n", path, offset);
    MwtTrace t;
    mwt_inline_parse(a, avp_buf_str(&b), &t);
    MwtResult r;
    mwt_inline_analyse(a, &t, NULL, NULL, &r);
    return r.callers.n == 1 && r.callers.v[0].visit->sites.n == 1 ? r.callers.v[0].visit->sites.v[0].line : -2;
}

/* The offset just after the n-th line end of s. */
static size_t after_newlines(const char *s, int n) {
    size_t i = 0;
    for (int k = 0; k < n && s[i]; i++)
        if (s[i] == '\n') k++;
    return i;
}

AVP_TEST(statement_lines_follow_line_directives) {
    AvpArena *a = avp_arena();
    const char *dir = avp_join(a, avp_root(), "build", "tmp", "mwtrace-test");
    REQUIRE(avp_makedirs(dir, true));
    const char *path = avp_path_join(a, dir, "lines.c");
    /* physical 1 "a", 2 "#line 10", 3.. numbered 10, 11, ... */
    const char *src = "a\n#line 10\nx\nx\nx\nx\nx\nx\nx\nx\nx\nx\nx\nx\nx\n";
    REQUIRE(avp_write_file(path, src, strlen(src)));
    CHECK_EQ_I64(line_at(a, path, 0), 1);                      /* logical 1: before the directive */
    CHECK_EQ_I64(line_at(a, path, after_newlines(src, 11)), 5); /* logical 12: physical 3 + (12 - 10) */
    CHECK_EQ_I64(line_at(a, path, after_newlines(src, 4)), -1); /* logical 5: no region numbers it */
    CHECK_EQ_I64(line_at(a, path, strlen(src)), -1);            /* the end of the file: no line */
    const char *plain = avp_path_join(a, dir, "plain.c");
    REQUIRE(avp_write_file(plain, "a\nb\nc\n", 6));
    CHECK_EQ_I64(line_at(a, plain, 4), 3);
}
AVP_TEST(spec_expands_for_both_builds) {
    AvpArena *a = avp_arena();
    const char *builds[] = {"Wii/1.0", "Wii/1.0a"};
    for (size_t i = 0; i < 2; i++) {
        const MwtBuild *b = mwt_build_find(builds[i]);
        REQUIRE(b);
        AvpStr out;
        const char *why = NULL;
        CHECK_MSG(mwt_build_expand(a, b, avp_str(MWT_INLINE_SPEC), &out, &why), "%s: %s", builds[i], why ? why : "");
        CHECK(!avp_str_contains(out, AVP_S("{")));
        u32 v = 0;
        CHECK(mwt_build_value(b, "DECIDE", &v) && v != 0);
    }
    CHECK(mwt_build_find("Wii/1.7") == NULL);
    const char *why = NULL;
    AvpStr out;
    CHECK(!mwt_build_expand(a, mwt_build_find("Wii/1.0"), AVP_S("bp {NO_SUCH_KEY}\n"), &out, &why));
    CHECK(why && strstr(why, "NO_SUCH_KEY"));
}

AVP_TEST_MAIN("mwtrace_inline")
