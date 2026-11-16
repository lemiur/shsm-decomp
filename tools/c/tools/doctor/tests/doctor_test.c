/* doctor: the port of tools/test_doctor.py. The pin table is checked against docs/toolchain.md (read at test time from the
 * repository root, CTest's working folder); everything else works on synthetic roots under build/tmp/doctor/, git
 * repositories made here; --fix runs with a recording command runner, so nothing is downloaded, and the hooks are
 * installed into those fixture repositories only.
 *
 * Map of tools/test_doctor.py (23 tests) to the C tests:
 *   TestPinsAgreeWithAgents.test_hashes                     -> pins_hashes
 *   TestPinsAgreeWithAgents.test_urls_commit_packages       -> pins_urls_commit_packages
 *   TestPinsAgreeWithAgents.test_compilers_named            -> pins_compilers_named
 *   TestPinsAgreeWithAgents.test_dol_contract               -> pins_dol_contract
 *   TestPinsAgreeWithAgents.test_binutils_tag_in_configure  -> pins_binutils_tag_in_configure
 *   SyntheticRoot.test_missing_and_mismatch                 -> missing_and_mismatch
 *   SyntheticRoot.test_ok_and_stale                         -> ok_and_stale
 *   SyntheticRoot.test_exit_and_json                        -> exit_and_json
 *   TestRequiredAndOptional.test_check_required_is_the_fast_subset -> check_required_is_the_fast_subset
 *   TestRequiredAndOptional.test_contract                   -> contract
 *   TestRequiredAndOptional.test_tool_output_dirs_are_not_stray -> tool_output_dirs_are_not_stray
 *   TestRequiredAndOptional.test_tool_session_and_draft_entries_are_not_stray
 *                                                           -> tool_session_and_draft_entries_are_not_stray
 *   TestRequiredAndOptional.test_advisory_lines_never_fail  -> advisory_lines_never_fail
 *   TestRequiredAndOptional.test_ghidra_status              -> ghidra_status (+ ghidra_regex: the regex's edge cases)
 *   TestRequiredAndOptional.test_index_status               -> index_status (also: asmindex.exe is the command run)
 *   TestHookLineEndings.test_pre_commit_shim_checks_out_with_lf -> pre_commit_shim_checks_out_with_lf
 *   TestNoRealTree.test_real_tree_tests_skip                -> real_tree_tests_skip (doctor's own entry; the other
 *       entries of its CASES list belong to the flagsweep, permute and mwcc_trace tests and are not doctor's)
 *   TestRealTree.test_check_required_passes_here            -> check_required_passes_here
 *   TestFix.test_all_components                             -> fix_all_components
 *   TestFix.test_stops_on_failure                           -> fix_stops_on_failure
 *   TestFix.test_binutils_refuses_other_tag                 -> fix_binutils_refuses_other_tag
 *   TestFix.test_permuter_not_git                           -> fix_permuter_not_git
 *   TestHooks.test_install_uninstall                        -> install_uninstall (+ install_writes_lf)
 * The Python runs SyntheticRoot's three tests again in each subclass (inheritance); the C runs each once. */
#include "avp/test.h"
#include "doctor.h"
#include "shsm_cli.h"
#include "shsm_git.h"

#include <string.h>

#define A avp_arena()
#define FMT(...) ((char *)avp_pyfmt(A, __VA_ARGS__).p)

/* ---- fixtures -------------------------------------------------------------------------------------------------- */
static void wr(const char *root, const char *rel, const char *text) {
    char *p = avp_path_join(A, root, rel);
    AVP_ASSERT(avp_makedirs(avp_cz(A, avp_path_dirname(p)), true));
    AVP_ASSERT(avp_write_file(p, text, strlen(text)));
}

static void age(const char *root, const char *rel, double seconds_ago_of) {
    i64 ns = (i64)(seconds_ago_of * 1e9);
    AVP_ASSERT(avp_utime_ns(avp_path_join(A, root, rel), ns, ns));
}

static double mtime(const char *root, const char *rel) {
    double t = 0;
    AVP_ASSERT(avp_getmtime(avp_path_join(A, root, rel), &t));
    return t;
}

/* A fresh git repository build/tmp/doctor/<name> (git objects are read-only: AVP_RMTREE_CLEAR_READONLY). */
static const char *fresh(const char *name) {
    const char *d = avp_path_abspath(A, avp_join(A, "build", "tmp", "doctor", name));
    if (avp_isdir(d)) AVP_ASSERT(avp_rmtree(d, AVP_RMTREE_CLEAR_READONLY));
    AVP_ASSERT(avp_makedirs(d, true));
    SHSM_GIT_CHECK(A, d, "init", "-q", "-b", "main");
    return d;
}

#define DOL "retail dol"
static char *dol_sha1(void) {
    u8 h[20];
    avp_sha1(DOL, strlen(DOL), h);
    AvpBuf b;
    avp_buf_init(&b, A);
    for (int i = 0; i < 20; i++) avp_buf_printf(&b, "%02x", h[i]);
    return avp_buf_cz(&b);
}

/* SyntheticRoot.setUp */
static const char *synthetic(const char *name) {
    const char *r = fresh(name);
    wr(r, "orig/R5WEA4/sys/main.dol", DOL);
    wr(r, "config/R5WEA4/build.sha1", FMT("%s  build/R5WEA4/main.dol\n", dol_sha1()));
    wr(r, "configure.py", "config.binutils_tag = \"2.42-2\"\n");
    wr(r, "tools/hooks/pre-commit-c", "#!/bin/sh\n" DOCTOR_HOOK_MARKER " test build/tools/pre_commit.exe\nexit 0\n");
    return r;
}

static DoctorChecks all_checks(Doctor *d) {
    DoctorChecks c = {0};
    doctor_check_all(d, &c);
    return c;
}

static const DoctorCheck *get(const DoctorChecks *c, const char *name) {
    const DoctorCheck *x = doctor_find(c, name);
    if (!x) avp_test__fail(__FILE__, __LINE__, "no check %s", name);
    AVP_ASSERT(x);
    return x;
}
#define STATUS(c, name) get(&(c), (name))->status

/* The built doctor.exe beside the tests' folder (build/tools/doctor.exe): stdout and the exit code. */
static i64 run_exe(const char *const *args, size_t n, AvpStr *out) {
    const char **argv = AVP_NEWN(A, const char *, n + 2);
    argv[0] = avp_path_join(A, avp_cz(A, avp_path_dirname(avp_exe_dir())), "doctor.exe");
    for (size_t i = 0; i < n; i++) argv[i + 1] = args[i];
    AvpProcResult r = avp_run(&(AvpProcSpec){.argv = argv, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE, .text = true,
                                             .timeout = 300, .arena = A});
    AVP_ASSERT(r.status == AVP_PROC_OK);
    *out = r.out;
    return r.returncode;
}

/* ---- TestPinsAgreeWithAgents ----------------------------------------------------------------------------------- */
/* The pins are written in docs/toolchain.md, except the DOL hash, which AGENTS.md keeps ("Byte gate"): both files. */
static AvpStrVec agents_lines(void) {
    AvpStrVec out = {0};
    static const char *const files[] = {"docs/toolchain.md", "AGENTS.md"};
    for (int f = 0; f < 2; f++) {
        AvpStr t;
        AVP_ASSERT(avp_read_text(A, files[f], AVP_TM_UTF8, &t));
        AvpStrVec l = avp_str_splitlines(t, false);
        for (size_t i = 0; i < l.n; i++) avp_vec_push(&out, l.v[i]);
    }
    return out;
}

/* The line holding value, joined with the two lines before it (a pin entry wraps). */
static AvpStr line_with(const AvpStrVec *lines, const char *value) {
    for (size_t i = 0; i < lines->n; i++)
        if (avp_str_contains(lines->v[i], avp_str(value))) {
            size_t s = i >= 2 ? i - 2 : 0;
            return avp_str_join(A, AVP_S(" "), lines->v + s, i + 1 - s);
        }
    avp_test__fail(__FILE__, __LINE__, "%s not in docs/toolchain.md or AGENTS.md", value);
    return AVP_S("");
}

AVP_TEST(pins_hashes) {
    AvpStrVec lines = agents_lines();
    for (size_t k = 0; k < doctor_npins; k++) {
        const DoctorPin *pin = &doctor_pins[k];
        const char *hashes[] = {pin->sha256, pin->zip_sha256, pin->sha1};
        for (int h = 0; h < 3; h++) {
            if (!hashes[h]) continue;
            AvpStr ln = line_with(&lines, hashes[h]);
            if (pin->version) CHECK_MSG(avp_str_contains(ln, avp_str(pin->version)), "%s", pin->key);
            if (pin->asset) CHECK_MSG(avp_str_contains(ln, avp_str(pin->asset)), "%s", pin->key);
        }
    }
}

AVP_TEST(pins_urls_commit_packages) {
    AvpStrVec lines = agents_lines();
    static const char *const keys[] = {"dtk", "objdiff-cli", "ninja"};
    for (int i = 0; i < 3; i++) line_with(&lines, doctor_pin(keys[i])->url);
    AvpStr ln = line_with(&lines, doctor_pin("permuter")->commit);
    CHECK(avp_str_contains(ln, avp_str(doctor_pin("permuter")->url)));
    ln = line_with(&lines, "pip install");
    for (const char *const *p = doctor_pin("venv")->packages; *p; p++) CHECK_MSG(avp_str_contains(ln, avp_str(*p)), "%s", *p);
}

AVP_TEST(pins_compilers_named) {
    AvpStr t;
    AVP_ASSERT(avp_read_text(A, "docs/toolchain.md", AVP_TM_UTF8, &t));
    const char *const *lists[] = {doctor_compilers_required, doctor_compilers_optional};
    for (int l = 0; l < 2; l++)
        for (const char *const *c = lists[l]; *c; c++) {
            const char *tail = strchr(*c, '/') + 1;
            CHECK_MSG(avp_str_contains(t, avp_str(*c)) || avp_str_contains(t, avp_str(tail)), "%s", *c);
        }
}

AVP_TEST(pins_dol_contract) {
    AvpStr t;
    AVP_ASSERT(avp_read_text(A, "config/R5WEA4/build.sha1", AVP_TM_UTF8, &t));
    AvpStrVec w = avp_str_split(t, (AvpStr){0}, -1);
    REQUIRE(w.n > 0);
    CHECK(avp_str_eqz(w.v[0], doctor_pin("dol")->sha1));
}

AVP_TEST(pins_binutils_tag_in_configure) {
    char *tag = doctor_configure_binutils_tag(A, avp_path_abspath(A, "."));
    REQUIRE(tag);
    CHECK_CZ(tag, "2.42-2");
    CHECK_CZ(doctor_pin("binutils")->version, "2.42-2");
}

/* ---- SyntheticRoot --------------------------------------------------------------------------------------------- */
AVP_TEST(missing_and_mismatch) {
    const char *r = synthetic("missing and mismatch");
    wr(r, "tools/bin/dtk.exe", "not dtk");
    wr(r, "build/stray.json", "{}");
    AVP_ASSERT(avp_makedirs(avp_join(A, r, "build", "R5WEA4"), true));
    Doctor d = doctor_new(r);
    DoctorChecks c = all_checks(&d);
    CHECK_CZ(STATUS(c, "dtk"), "MISMATCH");
    CHECK_CZ(STATUS(c, "objdiff-cli"), "MISSING");
    CHECK_CZ(STATUS(c, "dol"), "MISMATCH"); /* sha1 is not the pinned retail value */
    CHECK_CZ(STATUS(c, "build.ninja"), "MISSING");
    CHECK_CZ(STATUS(c, "build-root"), "WARN");
    CHECK(strstr(get(&c, "build-root")->detail, "stray.json") != NULL);
    CHECK(avp_isfile(avp_join(A, r, "build", "stray.json"))); /* never deleted */
    CHECK(!get(&c, "venv")->required);
}

AVP_TEST(ok_and_stale) {
    const char *r = synthetic("ok and stale");
    Doctor d = doctor_new(r);
    d.dol_sha1 = dol_sha1();
    wr(r, "config/R5WEA4/symbols.txt", "x");
    wr(r, "build/R5WEA4/asm/a.s", "x");
    wr(r, "build.ninja", "x");
    wr(r, "objdiff.json", "x");
    wr(r, "build/R5WEA4/classes.json", "x");
    DoctorChecks c = all_checks(&d);
    CHECK_CZ(STATUS(c, "dol"), "OK");
    CHECK_CZ(STATUS(c, "build.ninja"), "OK");
    CHECK_CZ(STATUS(c, "classes.json"), "OK");
    double old = mtime(r, "configure.py") - 100;
    age(r, "objdiff.json", old);
    age(r, "build/R5WEA4/classes.json", old);
    c = all_checks(&d);
    CHECK_CZ(STATUS(c, "objdiff.json"), "STALE");
    CHECK_CZ(STATUS(c, "classes.json"), "STALE");
    CHECK(get(&c, "classes.json")->required);
}

AVP_TEST(exit_and_json) {
    const char *r = synthetic("exit and json");
    AvpStr out;
    i64 rc = run_exe((const char *const[]){"--root", r, "--json"}, 3, &out);
    CHECK_EQ_I64(rc, 1);
    CHECK(avp_str_contains(out, AVP_S("\"ok\": false")));
}

/* ---- TestRequiredAndOptional ----------------------------------------------------------------------------------- */
typedef struct {
    size_t calls;
    i64 rc;
    const char *out;
    const char *last_argv_end;
    const char *last_argv;     /* the whole last command, joined with spaces */
} FakeRun;

static DoctorRunResult fake_runner(void *ctx, const char *const *argv, size_t argc, const char *cwd) {
    (void)cwd;
    FakeRun *f = ctx;
    f->calls++;
    f->last_argv_end = argc ? argv[argc - 1] : NULL;
    AvpStr *items = AVP_NEWN(A, AvpStr, argc ? argc : 1);
    for (size_t i = 0; i < argc; i++) items[i] = avp_str(argv[i]);
    f->last_argv = avp_cz(A, avp_str_join(A, AVP_S(" "), items, argc));
    return (DoctorRunResult){f->rc, f->out};
}

AVP_TEST(check_required_is_the_fast_subset) {
    const char *r = synthetic("fast subset");
    Doctor d = doctor_new(r);
    d.dol_sha1 = dol_sha1();
    FakeRun f = {.rc = 0, .out = ""};
    d.runner = fake_runner, d.runner_ctx = &f;
    DoctorChecks req = {0};
    doctor_check_required(&d, &req);
    static const char *const want[] = {"dtk", "objdiff-cli", "ninja", "compilers", "dol", "contract"};
    REQUIRE(req.n == 6);
    for (size_t i = 0; i < 6; i++) CHECK_CZ(req.v[i].name, want[i]);
    CHECK_EQ_I64((i64)f.calls, 0); /* no ninja to ask, no slot, no subprocess through the runner */
    Doctor real = doctor_new(r);
    DoctorChecks c = all_checks(&real);
    for (size_t i = 0; i < 6; i++) CHECK_CZ(c.v[i].name, want[i]);
    CHECK(doctor_find(&c, "ghidra") != NULL);
    CHECK(doctor_find(&c, "asm-index") != NULL);
}

AVP_TEST(contract) {
    const char *r = synthetic("contract");
    Doctor d = doctor_new(r);
    d.dol_sha1 = dol_sha1();
    DoctorChecks c = {0};
    doctor_check_required(&d, &c);
    CHECK_CZ(STATUS(c, "contract"), "MISSING"); /* no config.yml */
    wr(r, "config/R5WEA4/config.yml", FMT("object: sys/main.dol\nhash: %s\n", dol_sha1()));
    c.n = 0;
    doctor_check_required(&d, &c);
    CHECK_CZ(STATUS(c, "contract"), "OK");
    CHECK_CZ(STATUS(c, "dol"), "OK");
    wr(r, "config/R5WEA4/config.yml", "hash: 0000000000000000000000000000000000000000\n");
    c.n = 0;
    doctor_check_required(&d, &c);
    CHECK_CZ(STATUS(c, "contract"), "MISMATCH");
    CHECK(get(&c, "contract")->required);
}

AVP_TEST(tool_output_dirs_are_not_stray) {
    const char *r = synthetic("not stray");
    static const char *const dirs[] = {"gate", "neutral", "levers", "index", "tmp", "ghidra", "permute"};
    for (size_t i = 0; i < 7; i++) AVP_ASSERT(avp_makedirs(avp_join(A, r, "build", dirs[i]), true));
    wr(r, "build/walls.json", "x");
    wr(r, "build/score_history.sqlite", "x");
    Doctor d = doctor_new(r);
    DoctorChecks c = all_checks(&d);
    CHECK_MSG(!strcmp(STATUS(c, "build-root"), "OK"), "%s", get(&c, "build-root")->detail);
    wr(r, "build/odd.txt", "x");
    c = all_checks(&d);
    CHECK(strstr(get(&c, "build-root")->detail, "odd.txt") != NULL);
}

AVP_TEST(tool_session_and_draft_entries_are_not_stray) {
    const char *r = synthetic("session entries");
    AVP_ASSERT(avp_makedirs(avp_join(A, r, "build", "dolphin"), true));
    AVP_ASSERT(avp_makedirs(avp_join(A, r, "build", "draft"), true));
    wr(r, "build/eqcheck.json", "{}");
    Doctor d = doctor_new(r);
    DoctorChecks c = all_checks(&d);
    CHECK_CZ(STATUS(c, "build-root"), "OK");
}

AVP_TEST(advisory_lines_never_fail) {
    const char *r = synthetic("advisory");
    Doctor d = doctor_new(r);
    DoctorChecks c = all_checks(&d);
    static const char *const names[] = {"dolphin", "ps2-exe", "index twins", "index witness", "index unit_plan"};
    for (size_t i = 0; i < 5; i++) {
        CHECK_MSG(!strcmp(STATUS(c, names[i]), "MISSING"), "%s", names[i]);
        CHECK(get(&c, names[i])->advisory);
    }
    CHECK(strstr(get(&c, "index twins")->fix, "build/tools/twins.exe --rebuild stats") != NULL);
    AvpBuf js;
    avp_buf_init(&js, A);
    avp_buf_putz(&js, "{\"exe\": ");
    avp_json_put_string(&js, avp_str(avp_path_join(A, r, "configure.py")), false);
    avp_buf_putz(&js, "}");
    wr(r, "build/dolphin/dolphin.json", avp_buf_cz(&js));
    wr(r, "orig/_iso/SLUS_218.99", "\x7f" "ELF");
    wr(r, "build/index/twins.sqlite", "x");
    wr(r, "build/index/witness.sqlite", "x");
    wr(r, "build/index/asm.sqlite", "x");
    double old = mtime(r, "build/index/asm.sqlite") - 100;
    age(r, "build/index/witness.sqlite", old);
    age(r, "build/index/asm.sqlite", old + 50);
    c = all_checks(&d);
    CHECK_CZ(STATUS(c, "dolphin"), "OK");
    CHECK(strstr(get(&c, "dolphin")->detail, "session none") != NULL);
    CHECK_CZ(STATUS(c, "ps2-exe"), "OK");
    CHECK_CZ(get(&c, "ps2-exe")->detail, "orig/_iso/SLUS_218.99");
    CHECK_CZ(STATUS(c, "index twins"), "OK");
    CHECK_CZ(STATUS(c, "index witness"), "STALE");
    CHECK(strstr(doctor_check_line(A, get(&c, "index witness")), "witness.exe --rebuild stats") != NULL);
    /* --strict fails optional checks, never advisory ones */
    AvpStr out;
    run_exe((const char *const[]){"--root", r, "--json", "--strict"}, 4, &out);
    AvpJsonError err;
    AvpJson *doc = avp_json_parse(A, out, &err);
    REQUIRE(doc);
    AvpJson *checks = avp_json_getz(doc, "checks");
    for (size_t i = 0; i < avp_json_len(checks); i++) {
        AvpJson *x = avp_json_at(checks, i);
        if (avp_str_startswith(avp_json_get_str(x, "name", AVP_S("")), AVP_S("index ")))
            CHECK(avp_json_get_bool(x, "advisory", false));
    }
}

AVP_TEST(ghidra_status) {
    const char *r = synthetic("ghidra");
    wr(r, "build/tools/decomp.exe", "");
    Doctor d = doctor_new(r);
    d.decomp = avp_join(A, r, "build", "tools", "decomp.exe");
#define GH "ghidra:   11.4 (pinned) at X:/g/ghidra_11.4"
    static const struct {
        i64 rc;
        const char *out, *status, *text;
    } cases[] = {
        {0, GH "\nlanguage: x\nproject:  build/ghidra/p (present, input y)\nlistings: 5 of 9 functions, 0 stale, 0 failed",
         "OK", "project present"},
        {0, GH ", language PowerPC; no project (run setup)\nlistings: 0 of 9 functions, 0 stale, 0 failed", "MISSING",
         "project absent"},
        {0, "ghidra:   no Ghidra install found\nlistings: 0 of 9 functions", "MISSING", "install not found"},
        {2, "error: --json unknown", "WARN", "exited 2"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        FakeRun f = {.rc = cases[i].rc, .out = cases[i].out};
        d.runner = fake_runner, d.runner_ctx = &f;
        DoctorCheck ch = doctor_check_ghidra(&d);
        CHECK_MSG(!strcmp(ch.status, cases[i].status), "case %zu: %s", i, ch.status);
        CHECK_MSG(strstr(ch.detail, cases[i].text) != NULL, "case %zu: %s", i, ch.detail);
        CHECK(!ch.required);
    }
    FakeRun seen = {.rc = 0, .out = ""};
    d.runner_ctx = &seen;
    doctor_check_ghidra(&d);
    REQUIRE(seen.last_argv_end);
    CHECK_CZ(seen.last_argv_end, "status"); /* the text form: works with or without --json */
    d.decomp = avp_join(A, r, "build", "tools", "no_decomp.exe");
    d.runner = NULL;
    CHECK_CZ(doctor_check_ghidra(&d).status, "MISSING");
    CHECK_CZ(doctor_check_ghidra(&d).detail, "decomp.exe not built");
}

AVP_TEST(index_status) {
    const char *r = synthetic("index");
    Doctor d = doctor_new(r);
    d.runner = NULL;
    CHECK(strstr(doctor_check_index(&d).detail, "no build/R5WEA4/asm") != NULL);
    AVP_ASSERT(avp_makedirs(avp_join(A, r, "build", "R5WEA4", "asm"), true));
    static const struct {
        i64 rc;
        const char *out, *status;
    } cases[] = {
        {0, "{\"refresh\": {\"scanned\": 3, \"parsed\": 0, \"removed\": 0}, \"stats\": {\"files\": 3, \"symbols\": 7}}", "OK"},
        {0, "{\"refresh\": {\"scanned\": 3, \"parsed\": 2, \"removed\": 1}, \"stats\": {\"files\": 3, \"symbols\": 7}}", "STALE"},
        {1, "Traceback", "WARN"},
        {0, "no json", "WARN"},
    };
    for (size_t i = 0; i < 4; i++) {
        FakeRun f = {.rc = cases[i].rc, .out = cases[i].out};
        d.runner = fake_runner, d.runner_ctx = &f;
        DoctorCheck ch = doctor_check_index(&d);
        CHECK_MSG(!strcmp(ch.status, cases[i].status), "case %zu: %s", i, ch.detail);
        CHECK(!ch.required);
    }
    FakeRun f = {.rc = 0, .out = "{\"refresh\": {\"parsed\": 2, \"removed\": 1}, \"stats\": {\"files\": 3, \"symbols\": 7}}"};
    d.runner_ctx = &f;
    CHECK_CZ(doctor_check_index(&d).detail, "was behind the asm: 2 file(s) re-parsed, 1 removed (now current: 3 files, 7 symbols)");
    /* the C asmindex.exe beside doctor.exe answers, with these arguments */
    CHECK_CZ(f.last_argv, FMT("%s --stats --json --root %s", d.asmindex, r));
    CHECK(avp_str_endswith(avp_str(d.asmindex), AVP_S("asmindex.exe")));
}

AVP_TEST(ghidra_regex) {
    AvpStr v, in;
    CHECK(doctor_parse_ghidra(AVP_S("11.4 (pinned) at X:/g, language x"), &v, &in));
    CHECK(avp_str_eqz(v, "11.4") && avp_str_eqz(in, "X:/g"));
    CHECK(doctor_parse_ghidra(AVP_S("a (b) at c) at d"), &v, &in) && avp_str_eqz(in, "c) at d"));
    CHECK(doctor_parse_ghidra(AVP_S("a (b) at "), &v, &in) == false);
    CHECK(doctor_parse_ghidra(AVP_S("a (b) at , language x"), &v, &in) && avp_str_eqz(in, ", language x"));
    CHECK(!doctor_parse_ghidra(AVP_S("no Ghidra install found"), &v, &in));
}

/* ---- TestHookLineEndings --------------------------------------------------------------------------------------- */
AVP_TEST(pre_commit_shim_checks_out_with_lf) {
    ShsmGitRun r = SHSM_GIT(A, NULL, "check-attr", "eol", "--", "tools/hooks/pre-commit-c");
    if (!shsm_git_ok(&r)) return; /* not a git checkout: skipped */
    CHECK(avp_str_endswith(avp_str_strip(r.out), AVP_S("eol: lf")));
    r = SHSM_GIT(A, NULL, "ls-files", "--eol", "--", "tools/hooks/pre-commit-c");
    CHECK(avp_str_startswith(r.out, AVP_S("i/lf")));
}

/* ---- TestRealTree (skipped under SHSM_NO_REAL_TREE, as tools.common.testing.skip_unless_real_tree) ------------ */
AVP_TEST(check_required_passes_here) {
    const char *nrt = avp_getenv(A, "SHSM_NO_REAL_TREE");
    if (nrt && nrt[0]) {
        printf("check_required_passes_here ... skipped 'SHSM_NO_REAL_TREE is set'\n");
        return;
    }
    if (!avp_isfile("tools/bin/dtk.exe")) return;
    Doctor d = doctor_new(avp_path_abspath(A, "."));
    DoctorChecks c = {0};
    doctor_check_required(&d, &c);
    for (size_t i = 0; i < c.n; i++) CHECK_MSG(!strcmp(c.v[i].status, "OK"), "%s", doctor_check_line(A, &c.v[i]));
}

/* ---- TestNoRealTree: this test executable, run again with SHSM_NO_REAL_TREE=1, skips its real-tree test -------- */
AVP_TEST(real_tree_tests_skip) {
    const char *nrt = avp_getenv(A, "SHSM_NO_REAL_TREE");
    if (nrt && nrt[0]) return; /* the child run itself */
    AvpEnv *env = avp_env_copy(A, avp_env_current(A));
    avp_env_set(env, "SHSM_NO_REAL_TREE", "1");
    const char *self = avp_path_join(A, avp_exe_dir(), "test_doctor_doctor_test.exe");
    AvpProcResult r = avp_run(&(AvpProcSpec){
        .argv = (const char *const[]){self, "--filter", "check_required_passes_here", NULL},
        .env = env, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE, .text = true, .timeout = 300, .arena = A});
    REQUIRE(r.status == AVP_PROC_OK);
    CHECK_EQ_I64(r.returncode, 0);
    CHECK_MSG(avp_str_contains(r.out, AVP_S("skipped")) && avp_str_contains(r.out, AVP_S("SHSM_NO_REAL_TREE")), "%.*s",
              AVP_SF(r.out));
    CHECK(avp_str_contains(r.out, AVP_S("OK (1 tests)")));
}

/* ---- TestFix --------------------------------------------------------------------------------------------------- */
typedef struct {
    AvpStrVec calls;           /* each call joined with spaces */
    const char *fail_key;      /* cmd[1] (or cmd[0]) that fails */
    i64 fail_rc;
} FixRec;

static i64 fix_runner(void *ctx, const char *const *argv, size_t argc, const char *cwd) {
    (void)cwd;
    FixRec *f = ctx;
    AvpStr *items = AVP_NEWN(A, AvpStr, argc);
    for (size_t i = 0; i < argc; i++) items[i] = avp_str(argv[i]);
    avp_vec_push(&f->calls, avp_str_join(A, AVP_S(" "), items, argc));
    const char *key = argc > 1 ? argv[1] : argv[0];
    return f->fail_key && !strcmp(key, f->fail_key) ? f->fail_rc : 0;
}

AVP_TEST(fix_all_components) {
    const char *r = synthetic("fix all");
    wr(r, "tools/venv/Scripts/python.exe", "not python"); /* an existing venv: no system python is looked up */
    Doctor d = doctor_new(r);
    FixRec rec = {0};
    AvpBuf out;
    avp_buf_init(&out, A);
    DoctorFixer f = {.d = &d, .runner = fix_runner, .runner_ctx = &rec, .out = &out};
    CHECK(doctor_fix(&f, doctor_fix_components, 3));
    REQUIRE(rec.calls.n == 4);
    CHECK(avp_str_endswith(rec.calls.v[0], AVP_S("-m pip install toml Levenshtein capstone")));
    CHECK(avp_str_eqz(rec.calls.v[1], FMT("git clone %s tools\\permuter", doctor_pin("permuter")->url)));
    CHECK(avp_str_endswith(rec.calls.v[2], avp_str(doctor_pin("permuter")->commit)));
    CHECK(avp_str_endswith(rec.calls.v[3], AVP_S("download_tool.exe binutils build/binutils --tag 2.42-2")));
    CHECK_EQ_I64((i64)avp_str_count(avp_buf_str(&out), AVP_S("$ ")), 4); /* every command printed before it ran */
}

AVP_TEST(fix_stops_on_failure) {
    const char *r = synthetic("fix stops");
    Doctor d = doctor_new(r);
    FixRec rec = {.fail_key = "clone", .fail_rc = 128};
    AvpBuf out;
    avp_buf_init(&out, A);
    DoctorFixer f = {.d = &d, .runner = fix_runner, .runner_ctx = &rec, .out = &out};
    CHECK(!doctor_fix(&f, (const char *const[]){"permuter"}, 1));
    CHECK_EQ_I64((i64)rec.calls.n, 1);
    CHECK(avp_str_contains(avp_buf_str(&out), AVP_S("FAILED (exit 128): stopped")));
}

AVP_TEST(fix_binutils_refuses_other_tag) {
    const char *r = synthetic("fix binutils tag");
    wr(r, "configure.py", "config.binutils_tag = \"9.9\"\n");
    Doctor d = doctor_new(r);
    FixRec rec = {0};
    AvpBuf out;
    avp_buf_init(&out, A);
    DoctorFixer f = {.d = &d, .runner = fix_runner, .runner_ctx = &rec, .out = &out};
    CHECK(!doctor_fix(&f, (const char *const[]){"binutils"}, 1));
    CHECK_EQ_I64((i64)rec.calls.n, 0);
    CHECK(avp_str_contains(avp_buf_str(&out), AVP_S("binutils_tag '9.9' is not the pinned 2.42-2")));
}

AVP_TEST(fix_permuter_not_git) {
    const char *r = synthetic("fix permuter not git");
    wr(r, "tools/permuter/file.txt", "x");
    Doctor d = doctor_new(r);
    FixRec rec = {0};
    AvpBuf out;
    avp_buf_init(&out, A);
    DoctorFixer f = {.d = &d, .runner = fix_runner, .runner_ctx = &rec, .out = &out};
    CHECK(!doctor_fix(&f, (const char *const[]){"permuter"}, 1));
    CHECK_EQ_I64((i64)rec.calls.n, 0);
}

/* ---- TestHooks (fixture repositories only) --------------------------------------------------------------------- */
AVP_TEST(install_uninstall) {
    const char *r = synthetic("hooks");
    AvpBuf out;
    avp_buf_init(&out, A);
    char *hook = avp_join(A, r, ".git", "hooks", "pre-commit");
    CHECK_EQ_I64(doctor_install_hooks(r, &out), 0);
    CHECK(avp_isfile(hook));
    CHECK_EQ_I64(doctor_uninstall_hooks(r, &out), 0);
    CHECK(!avp_exists(hook));
    wr(r, ".git/hooks/pre-commit", "#!/bin/sh\necho foreign\n");
    CHECK_EQ_I64(doctor_install_hooks(r, &out), 1);
    CHECK_EQ_I64(doctor_uninstall_hooks(r, &out), 1);
    AvpStr t;
    AVP_ASSERT(avp_read_text(A, hook, AVP_TM_UTF8, &t));
    CHECK(avp_str_contains(t, AVP_S("foreign")));
}

#define C_SHIM "#!/bin/sh\n" DOCTOR_HOOK_MARKER " c\nexe=\"$top/build/tools/pre_commit.exe\"\nexit 0\n"

AVP_TEST(install_c_shim) {
    const char *r = synthetic("hooks c shim");
    wr(r, "tools/hooks/pre-commit-c", C_SHIM);
    AvpBuf out;
    avp_buf_init(&out, A);
    char *hook = avp_join(A, r, ".git", "hooks", "pre-commit");
    CHECK_CZ(doctor_hook_source(r), avp_join(A, r, "tools", "hooks", "pre-commit-c"));
    CHECK_EQ_I64(doctor_install_hooks(r, &out), 0);
    CHECK_CZ(doctor_hook_shim(hook), "pre-commit-c");
    AvpStr t;
    AVP_ASSERT(avp_read_text(A, hook, AVP_TM_UTF8, &t));
    CHECK(avp_str_eqz(t, C_SHIM));
    Doctor d = doctor_new(r);
    DoctorChecks c = all_checks(&d);
    CHECK_CZ(STATUS(c, "hooks"), "OK");
    CHECK_EQ_I64(doctor_install_hooks(r, &out), 0); /* reinstalling over the C shim */
    CHECK_EQ_I64(doctor_uninstall_hooks(r, &out), 0);
    CHECK(!avp_exists(hook));
    wr(r, ".git/hooks/pre-commit", "#!/bin/sh\necho foreign\n");
    CHECK(doctor_hook_shim(hook) == NULL);
    CHECK_EQ_I64(doctor_install_hooks(r, &out), 1);
}

AVP_TEST(install_needs_the_c_shim) {
    /* no Python fallback: a checkout without tools/hooks/pre-commit-c installs nothing */
    const char *r = synthetic("hooks no shim");
    AVP_ASSERT(avp_remove(avp_join(A, r, "tools", "hooks", "pre-commit-c")));
    wr(r, "tools/hooks/pre-commit", "#!/bin/sh\n" DOCTOR_HOOK_MARKER " py\nexit 0\n");
    AvpBuf out;
    avp_buf_init(&out, A);
    CHECK(doctor_hook_source(r) == NULL);
    CHECK_EQ_I64(doctor_install_hooks(r, &out), 2);
    CHECK(!avp_exists(avp_join(A, r, ".git", "hooks", "pre-commit")));
    CHECK(avp_str_contains(avp_buf_str(&out), AVP_S("tools/hooks/pre-commit-c not found")));
    /* an already installed older Python shim is still told apart, and uninstalls */
    wr(r, ".git/hooks/pre-commit", "#!/bin/sh\n" DOCTOR_HOOK_MARKER " py\nexit 0\n");
    CHECK_CZ(doctor_hook_shim(avp_join(A, r, ".git", "hooks", "pre-commit")), "pre-commit");
    CHECK_EQ_I64(doctor_uninstall_hooks(r, &out), 0);
}

AVP_TEST(real_shim_recognised) {
    /* the checkout's own shim carries the marker, the C executable's name, and no python fallback */
    char *c = avp_join(A, "tools", "hooks", "pre-commit-c");
    if (!avp_isfile(c)) return; /* not run from a checkout: skipped */
    CHECK_CZ(doctor_hook_shim(c), "pre-commit-c");
    AvpStr t;
    AVP_ASSERT(avp_read_text(A, c, AVP_TM_UTF8, &t));
    CHECK(!avp_str_contains(t, AVP_S("python")));
}

AVP_TEST(install_writes_lf) {
    const char *r = synthetic("hooks crlf");
    wr(r, "tools/hooks/pre-commit-c", "#!/bin/sh\r\n" DOCTOR_HOOK_MARKER " x\r\nexit 0\r\n");
    AvpBuf out;
    avp_buf_init(&out, A);
    CHECK_EQ_I64(doctor_install_hooks(r, &out), 0);
    AvpBytes b = {0};
    AVP_ASSERT(avp_read_all(A, avp_join(A, r, ".git", "hooks", "pre-commit"), &b));
    CHECK(memchr(b.v, '\r', b.n) == NULL);
}

AVP_TEST_MAIN("doctor")
