/* tools/test_unit.py FlipTest ported to C: `unit.exe flip` and `park` with --gate-cmd / --rules-cmd stubs, in a
 * git repository.
 *
 * Mapping (FlipTest.test_X -> here X): success_flips_and_prints_the_gate_line, gate_without_ok_line_reverts,
 * gate_failure_reverts, report_below_100_reverts, success_says_objdiff_passed, other_matching_unit_below_100_reverts,
 * skipped_objdiff_is_not_reported_as_passed, stale_report_is_skipped_not_passed, only_nonmatching_units_are_flipped,
 * rule_findings_revert, dirty_tree_is_refused, own_files_must_be_committed_too, untracked_file_counts_unless_ignored,
 * dry_run_changes_nothing, dry_run_reports_refusal, custom_gate_options_need_gate_cmd, already_matching,
 * check_errors_block_the_flip, park.
 * New: park_rewrites_a_multi_line_status (set_status is static in the C port; park exercises it on the multi-line
 * Object statement ParsingTest.test_configure_parse_and_status rewrote). */
#include "fixture.h"

static const char *SPLITS_REL = "config/R5WEA4/splits.txt";

typedef struct {
    Repo r;
    const char *rules_ok, *rules_bad;
} Fx;

/* setUp: a git repository with game/CFooTest.cpp made by `new` and committed (built once, copied per test) */
static Repo flip_make(void) {
    Repo r = repo_new(true);
    Cli c = RUN(r, "new", "game/CFooTest.cpp", "--text", "0x80010000-0x80010040");
    if (c.rc != 0) fprintf(stderr, "setUp: new exited %lld\n%.*s%.*s\n", (long long)c.rc, AVP_SF(c.out), AVP_SF(c.err));
    AVP_ASSERT(c.rc == 0);
    GIT(r.root, "add", "-A");
    GIT(r.root, "commit", "-q", "-m", "new unit");
    return r;
}

static Fx flip_fx(void) {
    static Repo tmpl;
    AvpArena *a = fx_a();
    Fx f;
    f.r = repo_from_template(&tmpl, flip_make);
    (void)a;
    f.rules_ok = "echo rules fine";
    f.rules_bad = "echo R17 finding& exit 1";
    return f;
}

/* gate(ok, rc, pct, other_pct, report): writes gate_stub.cmd, returns the command that runs it */
static const char *gate_full(const Fx *f, bool ok, int rc, const char *pct, const char *other_pct, bool report) {
    AvpArena *a = fx_a();
    const char *script = fx_gate_script(ok, pct, other_pct, report);
    if (rc != 0) script = avp_sprintf(a, "%sexit /b %d\n", script, rc);
    fx_put(f->r.root, "gate_stub.cmd", script, "\r\n");
    /* by full path: a bare name is not looked up in the current directory under NoDefaultCurrentDirectoryInExePath */
    return avp_sprintf(a, "\"%s\"", avp_path_join(a, f->r.root, "gate_stub.cmd"));
}
static const char *gate_ok(const Fx *f) { return gate_full(f, true, 0, "100.0", "100.0", true); }

static Cli flip(const Fx *f, const char *gate, const char *rules /* NULL: rules_ok */) {
    return RUN(f->r, "flip", "game/CFooTest.cpp", "--gate-cmd", gate, "--rules-cmd", rules ? rules : f->rules_ok,
               "--ignore-untracked");
}

static const char *status_of(const Fx *f) {
    UnitConfig *cfg = unit_parse_configure(fx_a(), fx_read_lf(f->r.root, "configure.py"));
    const UnitObj *o = unit_config_find(cfg, "game/CFooTest.cpp");
    return o ? o->status : "(no Object)";
}

AVP_TEST(success_flips_and_prints_the_gate_line) {
    Fx f = flip_fx();
    AvpStr before = fx_read(f.r.root, "configure.py");
    Cli c = flip(&f, gate_ok(&f), NULL);
    CHECK_RC(c, 0);
    CHECK_CZ(status_of(&f), "Matching");
    CHECK_IN(c.out, "gate line: build/R5WEA4/main.dol: OK");
    CHECK_IN(c.out, "rules fine");
    CHECK_IN(c.out, "$ "); /* prints what it ran */
    /* only the status token changed, CRLF kept */
    CHECK_BYTES_EQ(fx_replace(fx_read(f.r.root, "configure.py"), "Matching, \"game/CFooTest.cpp\"",
                              "NonMatching, \"game/CFooTest.cpp\""),
                   before);
    repo_cleanup(&f.r);
}

/* Each step announces itself when it starts, with the seconds since the flip began, before its own output. */
AVP_TEST(steps_are_announced_as_they_start) {
    Fx f = flip_fx();
    Cli c = flip(&f, gate_ok(&f), NULL);
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "s] step gate: the --gate-cmd command");
    CHECK_IN(c.out, "s] step rules: the rule checker");
    CHECK_BEFORE(c.out, "step gate", "gate line: build/R5WEA4/main.dol: OK");
    CHECK_BEFORE(c.out, "gate line:", "step rules");
    CHECK_BEFORE(c.out, "step rules", "rules fine");
    repo_cleanup(&f.r);
}

AVP_TEST(gate_without_ok_line_reverts) {
    Fx f = flip_fx();
    AvpStr before = fx_read(f.r.root, "configure.py");
    Cli c = flip(&f, gate_full(&f, false, 0, "100.0", "100.0", true), NULL);
    CHECK_EQ_I64(c.rc, 1);
    CHECK_IN(c.out, "no `build/R5WEA4/main.dol: OK` line");
    CHECK_NOT_IN(c.out, "is Matching");
    CHECK_BYTES_EQ(before, fx_read(f.r.root, "configure.py"));
    repo_cleanup(&f.r);
}

AVP_TEST(gate_failure_reverts) {
    Fx f = flip_fx();
    AvpStr before = fx_read(f.r.root, "configure.py");
    Cli c = flip(&f, gate_full(&f, true, 3, "100.0", "100.0", true), NULL);
    CHECK_EQ_I64(c.rc, 1);
    CHECK_IN(c.out, "the gate command exited 3");
    CHECK_BYTES_EQ(before, fx_read(f.r.root, "configure.py"));
    repo_cleanup(&f.r);
}

AVP_TEST(report_below_100_reverts) {
    Fx f = flip_fx();
    Cli c = flip(&f, gate_full(&f, true, 0, "87.5", "100.0", true), NULL);
    CHECK_EQ_I64(c.rc, 1);
    CHECK_IN(c.out, "not 100% matched");
    CHECK_CZ(status_of(&f), "NonMatching");
    repo_cleanup(&f.r);
}

AVP_TEST(success_says_objdiff_passed) {
    Fx f = flip_fx();
    Cli c = flip(&f, gate_ok(&f), NULL);
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "objdiff and the rule checker passed");
    CHECK_NOT_IN(c.out, "SKIPPED");
    repo_cleanup(&f.r);
}

AVP_TEST(other_matching_unit_below_100_reverts) {
    Fx f = flip_fx();
    AvpStr before = fx_read(f.r.root, "configure.py");
    Cli c = flip(&f, gate_full(&f, true, 0, "100.0", "99.0", true), NULL);
    CHECK_RC(c, 1);
    CHECK_IN(c.out, "other Matching unit(s) below 100%");
    CHECK_IN(c.out, "main/Runtime/a 99.0");
    CHECK_NOT_IN(c.out, "is Matching");
    CHECK_BYTES_EQ(before, fx_read(f.r.root, "configure.py"));
    repo_cleanup(&f.r);
}

AVP_TEST(skipped_objdiff_is_not_reported_as_passed) {
    Fx f = flip_fx();
    Cli c = flip(&f, gate_full(&f, true, 0, "100.0", "100.0", false), NULL);
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "objdiff check SKIPPED");
    CHECK_IN(c.out, "the objdiff check was SKIPPED");
    CHECK_NOT_IN(c.out, "objdiff and the rule checker passed");
    repo_cleanup(&f.r);
}

AVP_TEST(stale_report_is_skipped_not_passed) {
    Fx f = flip_fx();
    fx_put(f.r.root, "build/R5WEA4/report.json", "{\"units\": []}", "\n");
    AVP_ASSERT(avp_utime_ns(avp_join(fx_a(), f.r.root, "build/R5WEA4/report.json"), 1000000000LL, 1000000000LL));
    Cli c = flip(&f, gate_full(&f, true, 0, "100.0", "100.0", false), NULL);
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "older than this run");
    CHECK_NOT_IN(c.out, "objdiff and the rule checker passed");
    repo_cleanup(&f.r);
}

AVP_TEST(only_nonmatching_units_are_flipped) {
    Fx f = flip_fx();
    static const char *const statuses[] = {"Equivalent", "MatchingFor(\"R5WEA4\")"};
    for (size_t i = 0; i < 2; i++) {
        const char *st = statuses[i];
        AvpStr text = fx_replace(fx_read(f.r.root, "configure.py"), "NonMatching, \"game/CFooTest.cpp\"",
                                 avp_sprintf(fx_a(), "%s, \"game/CFooTest.cpp\"", st));
        fx_put(f.r.root, "configure.py", avp_cz(fx_a(), text), "\n");
        GIT(f.r.root, "add", "-A");
        GIT(f.r.root, "commit", "-q", "-m", avp_sprintf(fx_a(), "status %s", st));
        AvpStr before = fx_read(f.r.root, "configure.py");
        Cli c = flip(&f, gate_ok(&f), NULL);
        CHECK_MSG(c.rc == 2, "%s: exit %lld\n%.*s%.*s", st, (long long)c.rc, AVP_SF(c.out), AVP_SF(c.err));
        CHECK_IN(c.err, "flip only promotes a NonMatching unit");
        CHECK_BYTES_EQ(before, fx_read(f.r.root, "configure.py"));
        text = fx_replace(fx_read(f.r.root, "configure.py"), avp_sprintf(fx_a(), "%s, \"game/CFooTest.cpp\"", st),
                          "NonMatching, \"game/CFooTest.cpp\"");
        fx_put(f.r.root, "configure.py", avp_cz(fx_a(), text), "\n");
    }
    repo_cleanup(&f.r);
}

AVP_TEST(rule_findings_revert) {
    Fx f = flip_fx();
    Cli c = flip(&f, gate_ok(&f), f.rules_bad);
    CHECK_EQ_I64(c.rc, 1);
    CHECK_IN(c.out, "MATCH-PENDING");
    CHECK_CZ(status_of(&f), "NonMatching");
    repo_cleanup(&f.r);
}

AVP_TEST(dirty_tree_is_refused) {
    Fx f = flip_fx();
    fx_put(f.r.root, "src/game/Other.cpp", "// changed\n", "\r\n");
    Cli c = flip(&f, gate_ok(&f), NULL);
    CHECK_EQ_I64(c.rc, 1);
    CHECK_IN(c.out, "src/game/Other.cpp");
    CHECK_CZ(status_of(&f), "NonMatching");
    repo_cleanup(&f.r);
}

AVP_TEST(own_files_must_be_committed_too) {
    Fx f = flip_fx();
    AvpStr before = fx_read(f.r.root, "configure.py");
    fx_put(f.r.root, "src/game/CFooTest.cpp", "// edited\n", "\r\n");
    Cli c = flip(&f, gate_ok(&f), NULL);
    CHECK_RC(c, 1);
    CHECK_IN(c.out, "not clean");
    CHECK_BYTES_EQ(before, fx_read(f.r.root, "configure.py"));
    repo_cleanup(&f.r);
}

AVP_TEST(untracked_file_counts_unless_ignored) {
    Fx f = flip_fx();
    fx_put(f.r.root, "notes.txt", "x\n", "\n");
    Cli c = RUN(f.r, "flip", "game/CFooTest.cpp", "--gate-cmd", gate_ok(&f), "--rules-cmd", f.rules_ok);
    CHECK_EQ_I64(c.rc, 1);
    CHECK_IN(c.out, "notes.txt");
    c = flip(&f, gate_ok(&f), NULL);
    CHECK_RC(c, 0);
    repo_cleanup(&f.r);
}

AVP_TEST(dry_run_changes_nothing) {
    Fx f = flip_fx();
    AvpStr before = fx_read(f.r.root, "configure.py");
    Cli c = RUN(f.r, "flip", "game/CFooTest.cpp", "--dry-run", "--gate-cmd", gate_ok(&f), "--ignore-untracked");
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "pre-checks pass; nothing changed");
    CHECK_BYTES_EQ(before, fx_read(f.r.root, "configure.py"));
    CHECK(!fx_exists(f.r.root, "build/R5WEA4/report.json"));
    repo_cleanup(&f.r);
}

AVP_TEST(dry_run_reports_refusal) {
    Fx f = flip_fx();
    fx_put(f.r.root, "src/game/Other.cpp", "// changed\n", "\r\n");
    Cli c = RUN(f.r, "flip", "game/CFooTest.cpp", "--dry-run");
    CHECK_EQ_I64(c.rc, 1);
    CHECK_IN(c.out, "flip would REFUSE");
    CHECK_IN(c.out, "--pending-flip game/CFooTest.cpp");
    repo_cleanup(&f.r);
}

AVP_TEST(custom_gate_options_need_gate_cmd) {
    Fx f = flip_fx();
    Cli c = RUN(f.r, "flip", "game/CFooTest.cpp", "--ignore-untracked");
    CHECK_EQ_I64(c.rc, 2);
    CHECK_IN(c.err, "only with --gate-cmd");
    c = RUN(f.r, "flip", "game/CFooTest.cpp", "--rules-cmd", f.rules_ok);
    CHECK_EQ_I64(c.rc, 2);
    CHECK_IN(c.err, "only with --gate-cmd");
    repo_cleanup(&f.r);
}

AVP_TEST(already_matching) {
    Fx f = flip_fx();
    CHECK_EQ_I64(flip(&f, gate_ok(&f), NULL).rc, 0);
    GIT(f.r.root, "add", "-A");
    GIT(f.r.root, "commit", "-q", "-m", "flipped");
    Cli c = flip(&f, gate_ok(&f), NULL);
    CHECK_EQ_I64(c.rc, 2);
    CHECK_IN(c.err, "already Matching");
    repo_cleanup(&f.r);
}

AVP_TEST(check_errors_block_the_flip) {
    Fx f = flip_fx();
    AvpStr sp = fx_replace1(fx_read(f.r.root, SPLITS_REL), "end:0x80010040", "end:0x80010044");
    fx_put(f.r.root, SPLITS_REL, avp_cz(fx_a(), sp), "\n");
    GIT(f.r.root, "add", "-A");
    GIT(f.r.root, "commit", "-q", "-m", "break split");
    Cli c = flip(&f, gate_ok(&f), NULL);
    CHECK_EQ_I64(c.rc, 1);
    CHECK_IN(c.out, "`check` reports errors");
    CHECK_CZ(status_of(&f), "NonMatching");
    repo_cleanup(&f.r);
}

AVP_TEST(park) {
    Fx f = flip_fx();
    CHECK_EQ_I64(flip(&f, gate_ok(&f), NULL).rc, 0);
    Cli c = RUN(f.r, "park", "game/CFooTest.cpp");
    CHECK_EQ_I64(c.rc, 0);
    CHECK_IN(c.out, "Matching -> NonMatching");
    CHECK_CZ(status_of(&f), "NonMatching");
    c = RUN(f.r, "park", "game/CFooTest.cpp");
    CHECK_IN(c.out, "already NonMatching");
    CHECK_IN(fx_read(f.r.root, "configure.py"), "\r\n");
    repo_cleanup(&f.r);
}

AVP_TEST(park_rewrites_a_multi_line_status) {
    Repo r = repo_new(false);
    AvpStr orig = fx_read(r.root, "configure.py");
    AvpStr matching = fx_replace(orig, "NonMatching,\r\n", "Matching,\r\n"); /* After.cpp: a multi-line Object */
    CHECK_MSG(!avp_str_eq(matching, orig), "fixture has no multi-line NonMatching");
    fx_put(r.root, "configure.py", avp_cz(fx_a(), matching), "\n");
    Cli c = RUN(r, "park", "game/After.cpp");
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "Matching -> NonMatching");
    CHECK_BYTES_EQ(fx_read(r.root, "configure.py"), orig);
    UnitConfig *cfg = unit_parse_configure(fx_a(), fx_lf(fx_read(r.root, "configure.py")));
    const UnitObj *o = unit_config_find(cfg, "game/After.cpp");
    REQUIRE(o != NULL);
    CHECK_CZ(o->status, "NonMatching");
    repo_cleanup(&r);
}

UNIT_TEST_MAIN("unit")
