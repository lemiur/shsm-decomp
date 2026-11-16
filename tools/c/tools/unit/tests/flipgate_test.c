/* tools/test_unit.py FlipGatePyTest ported to C: flip's default gate, gate.exe --pending-flip, here a committed
 * batch stub (tools/gate.cmd, run in place of gate.exe through UNIT_GATE_ENV) that records its argv and the
 * configure.py it saw (build/stub_argv.txt, build/stub_cfg.txt instead of the Python test's build/stub.json).
 *
 * Mapping (FlipGatePyTest.test_X -> here X): green_pending_gate_leaves_the_edit_uncommitted,
 * red_gate_reverts_and_prints_the_red_line_and_attribution, only_ok_pending_for_this_unit_is_accepted (the five
 * subTests in one test), dirty_tree_refused_before_any_edit, non_nonmatching_unit_refused. */
#include "fixture.h"

typedef struct {
    Repo r;
    AvpStr before; /* configure.py as committed */
} Fx;

/* tools/gate.cmd: writes its argv (one per line) and the configure.py it saw to build/, prints lines, exits rc. */
static void stub_lines(const Fx *f, int rc, const char *const *lines, size_t n) {
    AvpBuf b;
    avp_buf_init(&b, fx_a());
    avp_buf_putz(&b, "@echo off\n"
                     "if not exist build mkdir build\n"
                     "type nul > build\\stub_argv.txt\n"
                     ":loop\n"
                     "if \"%~1\"==\"\" goto done\n"
                     ">>build\\stub_argv.txt echo %~1\n"
                     "shift\n"
                     "goto loop\n"
                     ":done\n"
                     "copy /y configure.py build\\stub_cfg.txt >nul\n");
    for (size_t i = 0; i < n; i++) avp_buf_printf(&b, "echo(%s\n", lines[i]);
    avp_buf_printf(&b, "exit /b %d\n", rc);
    fx_put(f->r.root, "tools/gate.cmd", avp_buf_cz(&b), "\r\n");
}
#define STUB(f, rc, ...) \
    stub_lines((f), (rc), (const char *const[]){__VA_ARGS__}, sizeof((const char *const[]){__VA_ARGS__}) / sizeof(char *))

static void restub_lines(Fx *f, int rc, const char *const *lines, size_t n) {
    stub_lines(f, rc, lines, n);
    GIT(f->r.root, "commit", "-q", "-am", "stub");
    f->before = fx_read(f->r.root, "configure.py");
}
#define RESTUB(f, rc, ...) \
    restub_lines((f), (rc), (const char *const[]){__VA_ARGS__}, sizeof((const char *const[]){__VA_ARGS__}) / sizeof(char *))

/* setUp: a git repository with game/CFooTest.cpp made by `new`, a green pending-flip stub gate, all committed
 * (built once, copied per test) */
static Repo gate_make(void) {
    Fx f;
    f.r = repo_new(true);
    Cli c = RUN(f.r, "new", "game/CFooTest.cpp", "--text", "0x80010000-0x80010040");
    if (c.rc != 0) fprintf(stderr, "setUp: new exited %lld\n%.*s%.*s\n", (long long)c.rc, AVP_SF(c.out), AVP_SF(c.err));
    AVP_ASSERT(c.rc == 0);
    STUB(&f, 3, "GATE OK-PENDING " FX_SHA " units=3 functions=9 pending-flip=game/CFooTest.cpp (12s): not a banked "
                "gate, commit configure.py and gate again");
    GIT(f.r.root, "add", "-A");
    GIT(f.r.root, "commit", "-q", "-m", "new unit");
    return f.r;
}

static Fx gate_fx(void) {
    static Repo tmpl;
    Fx f;
    f.r = repo_from_template(&tmpl, gate_make);
    f.before = fx_read(f.r.root, "configure.py");
    /* the stub in place of gate.exe beside unit.exe (unit.exe runs as a child and inherits the variable) */
    AVP_ASSERT(avp_setenv(UNIT_GATE_ENV, avp_path_join(fx_a(), f.r.root, "tools\\gate.cmd")));
    return f;
}

static const char *status_of(const Fx *f) {
    UnitConfig *cfg = unit_parse_configure(fx_a(), fx_read_lf(f->r.root, "configure.py"));
    const UnitObj *o = unit_config_find(cfg, "game/CFooTest.cpp");
    return o ? o->status : "(no Object)";
}

AVP_TEST(green_pending_gate_leaves_the_edit_uncommitted) {
    Fx f = gate_fx();
    Cli c = RUN(f.r, "flip", "game/CFooTest.cpp");
    CHECK_RC(c, 0);
    CHECK_CZ(status_of(&f), "Matching");
    AvpStr argv = fx_read(f.r.root, "build/stub_argv.txt");
    AvpStrVec parts = avp_str_splitlines(argv, false); /* the stub wrote it in text mode: CRLF */
    REQUIRE(parts.n >= 2);
    CHECK_STR(parts.v[parts.n - 2], "--pending-flip");
    CHECK_STR(parts.v[parts.n - 1], "game/CFooTest.cpp");
    /* gated with the edit in place */
    CHECK_IN(fx_read(f.r.root, "build/stub_cfg.txt"), "Object(Matching, \"game/CFooTest.cpp\"");
    CHECK_IN(c.out, "gate line: GATE OK-PENDING " FX_SHA);
    CHECK_IN(c.out, "git add configure.py");
    CHECK_IN(c.out, "not committed");
    Cli st = GIT(f.r.root, "status", "--porcelain");
    CHECK_IN(st.out, "configure.py");
    CHECK_IN(fx_read(f.r.root, "configure.py"), "\r\n");
    repo_cleanup(&f.r);
}

static AvpStr last_line(AvpStr text) {
    AvpStrVec l = avp_str_splitlines(text, false);
    AvpStr res = AVP_S("");
    for (size_t i = l.n; i-- > 0;)
        if (avp_str_strip(l.v[i]).n) {
            res = avp_str_strip(l.v[i]);
            break;
        }
    return res;
}

AVP_TEST(gate_line_is_the_last_line_and_the_layout_check_reports) {
    Fx f = gate_fx();
    Cli c = RUN(f.r, "flip", "game/CFooTest.cpp");
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "layout check skipped: no built object build/R5WEA4/src/game/CFooTest.o");
    CHECK_BEFORE(c.out, "layout check skipped", "gate line: GATE OK-PENDING");
    CHECK(avp_str_startswith(last_line(c.out), AVP_S("GATE OK-PENDING " FX_SHA)));
    repo_cleanup(&f.r);
}

AVP_TEST(red_gate_with_a_size_change_hints_force_active) {
    Fx f = gate_fx();
    RESTUB(&f, 1, "GATE RED checksum: build/R5WEA4/main.dol differs from the contract");
    /* the gate's built DOL is not the size of retail's */
    fx_put(f.r.root, "build/gate/tree/build/R5WEA4/main.dol", "abc", "\n");
    /* nothing outside the unit may reference its symbols: After.cpp read lbl_80500004, point it elsewhere */
    const char *asm_rel = "build/R5WEA4/asm/auto_03_80000000_text.s";
    fx_put(f.r.root, asm_rel,
           avp_cz(fx_a(), fx_replace(fx_read_lf(f.r.root, asm_rel), "lbl_80500004@sda21", "lbl_80500010@sda21")), "\n");
    Cli c = RUN(f.r, "flip", "game/CFooTest.cpp");
    CHECK_RC(c, 1);
    CHECK_IN(c.out, "hint: the gate's DOL is 3 bytes (retail 272)");
    CHECK_IN(c.out, "list lbl_80500000 under force_active in config/R5WEA4/config.yml");
    CHECK(avp_str_startswith(last_line(c.out), AVP_S("GATE RED checksum")));
    /* without a size change there is no hint */
    AvpBuf b;
    avp_buf_init(&b, fx_a());
    for (int i = 0; i < 0x110; i++) avp_buf_putc(&b, 'x');
    fx_put(f.r.root, "build/gate/tree/build/R5WEA4/main.dol", avp_buf_cz(&b), "\n");
    c = RUN(f.r, "flip", "game/CFooTest.cpp");
    CHECK_RC(c, 1);
    CHECK_NOT_IN(c.out, "hint:");
    repo_cleanup(&f.r);
}

AVP_TEST(red_gate_reverts_and_prints_the_red_line_and_attribution) {
    Fx f = gate_fx();
    AvpArena *a = fx_a();
    const char *lines[104];
    size_t n = 0;
    lines[n++] = "[RED ] checksum      3.0s  build/R5WEA4/main.dol differs from the contract";
    lines[n++] = "    0x80010010-0x80010014 .text main/game/CFooTest fn_80010000";
    for (unsigned i = 0; i < 100; i++) lines[n++] = avp_sprintf(a, "    0x%08X  retail 00  built 01", 0x80020000u + 4 * i);
    lines[n++] = "GATE RED checksum: build/R5WEA4/main.dol differs from the contract";
    restub_lines(&f, 1, lines, n);
    Cli c = RUN(f.r, "flip", "game/CFooTest.cpp");
    CHECK_RC(c, 1);
    CHECK_BYTES_EQ(fx_read(f.r.root, "configure.py"), f.before);
    CHECK_IN(c.out, "FAILED: gate.exe exited 1: GATE RED checksum: build/R5WEA4/main.dol differs");
    CHECK_IN(c.out, "main/game/CFooTest fn_80010000");
    CHECK_IN(c.out, "configure.py restored");
    CHECK(avp_str_startswith(last_line(c.out), AVP_S("GATE RED checksum: build/R5WEA4/main.dol differs")));
    repo_cleanup(&f.r);
}

AVP_TEST(only_ok_pending_for_this_unit_is_accepted) {
    Fx f = gate_fx();
    static const struct {
        int rc;
        const char *line;
    } cases[] = {
        {0, "GATE OK " FX_SHA " units=3 functions=9 (12s)"},
        {3, "GATE OK-PENDING " FX_SHA " units=3 functions=9 pending-flip=game/Other.cpp (12s): x"},
        {0, "GATE OK-PENDING " FX_SHA " units=3 functions=9 pending-flip=game/CFooTest.cpp (12s): x"},
        {3, "GATE DIRTY " FX_SHA " units=3 functions=9 (12s): tree not clean"},
        {3, "no gate line at all"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
        RESTUB(&f, cases[i].rc, cases[i].line);
        Cli c = RUN(f.r, "flip", "game/CFooTest.cpp");
        CHECK_MSG(c.rc == 1, "case %zu (rc %d, \"%s\"): exit %lld\n%.*s%.*s", i, cases[i].rc, cases[i].line,
                  (long long)c.rc, AVP_SF(c.out), AVP_SF(c.err));
        CHECK_MSG(!strcmp(status_of(&f), "NonMatching"), "case %zu: status %s", i, status_of(&f));
        CHECK_MSG(avp_str_eq(fx_read(f.r.root, "configure.py"), f.before), "case %zu: configure.py changed", i);
    }
    repo_cleanup(&f.r);
}

AVP_TEST(dirty_tree_refused_before_any_edit) {
    Fx f = gate_fx();
    fx_put(f.r.root, "notes.txt", "x\n", "\n");
    Cli c = RUN(f.r, "flip", "game/CFooTest.cpp");
    CHECK_EQ_I64(c.rc, 1);
    CHECK_IN(c.out, "notes.txt");
    CHECK(!fx_exists(f.r.root, "build/stub_argv.txt")); /* the gate never ran */
    CHECK_BYTES_EQ(fx_read(f.r.root, "configure.py"), f.before);
    repo_cleanup(&f.r);
}

AVP_TEST(non_nonmatching_unit_refused) {
    Fx f = gate_fx();
    AvpStr text = fx_replace(fx_read(f.r.root, "configure.py"), "NonMatching, \"game/CFooTest.cpp\"",
                             "Equivalent, \"game/CFooTest.cpp\"");
    fx_put(f.r.root, "configure.py", avp_cz(fx_a(), text), "\n");
    GIT(f.r.root, "commit", "-q", "-am", "equivalent");
    Cli c = RUN(f.r, "flip", "game/CFooTest.cpp");
    CHECK_EQ_I64(c.rc, 2);
    CHECK_IN(c.err, "flip only promotes a NonMatching unit");
    CHECK(!fx_exists(f.r.root, "build/stub_argv.txt"));
    repo_cleanup(&f.r);
}

UNIT_TEST_MAIN("unit")
