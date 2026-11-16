/* check_docs: the port of tools/test_check_docs.py. Everything runs on made-up checkouts under build/tmp/check_docs/
 * (CTest's working folder is the repository root): docs/tools.md, whose verb table names build/tools/<name>.exe
 * commands, empty stand-in executables (in the checkout's exe/ folder, passed as the folder beside check_docs.exe,
 * and in its build/tools/) whose --help comes from a stub runner, docs with paths, and a stub pin table. Nothing is
 * run but check_docs.exe itself (MainTest); the real tree is never read; ledger exceptions-check is replaced by a
 * stub runner.
 *
 * Map of tools/test_check_docs.py (20 tests) to the C tests:
 *   VerbsTest.test_findings                     -> verbs_findings
 *   VerbsTest.test_accepted_forms_are_quiet     -> verbs_accepted_forms_are_quiet
 *   VerbsTest.test_undocumented_notes           -> verbs_undocumented_notes
 *   HelpParsingTest.test_invocations            -> invocations
 *   HelpParsingTest.test_tokens                 -> tokens
 *   HelpParsingTest.test_adapter_rows           -> adapter_rows
 *   PathsTest.test_paths                        -> paths
 *   PinsTest.test_pins                          -> pins
 *   PinsTest.test_version_and_files             -> pins_version_and_files
 *   PinsTest.test_toolchain_entries             -> toolchain_entries
 *   ExceptionsTest.test_clean                   -> exceptions_clean
 *   ExceptionsTest.test_findings_and_errors     -> exceptions_findings_and_errors
 *   ExceptionsTest.test_argv                    -> exceptions_argv (ledger.exe beside check_docs.exe, then
 *                                                  build/tools; exceptions_no_ledger when neither exists)
 *   TextTest.test_bom_and_line_endings          -> text_bom_and_line_endings
 *   TextTest.test_source_encoding               -> text_source_encoding
 *   TextTest.test_tabs                          -> text_tabs
 *   TextTest.test_final_newline                 -> text_final_newline
 *   TextTest.test_binary_skipped                -> text_binary_skipped
 *   TextTest.test_vscode_rules                  -> vscode_rules
 *   MainTest.test_json_and_exit                 -> main_json_and_exit (check_docs.exe run as a process)
 *   MainTest.test_text_summary_and_clean_exit   -> main_text_summary_and_clean_exit
 *   MainTest.test_bad_only                      -> main_bad_only */
#include "avp/test.h"
#include "check_docs.h"

#include <string.h>

#define A avp_arena()
#define FMT(...) avp_sprintf(A, __VA_ARGS__)

#define SHA_A "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define SHA_B "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
#define SHA_X "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
#define COMMIT "1111111111111111111111111111111111111111"
#define DOL "dddddddddddddddddddddddddddddddddddddddd"

/* AGENTS.md: the guide keeps the DOL hash (its "Byte gate" paragraph). */
static const char AGENTS[] =
    "# Agent guide\n"
    "\n"
    "**Byte gate.** The DOL hash is `" DOL "`.\n";

/* docs/tools.md: the verb table, at the line numbers it had when it sat in the guide. */
static const char TOOLS_DOC[] =
    "# Agent guide\n"
    "\n"
    "## Project adapter\n"
    "\n"
    "| verb | meaning | this project's command |\n"
    "|---|---|---|\n"
    "| `scan` | scan things | `build/tools/fake.exe scan --deep` (`--dry-run` previews; `new <path> --name N`) |\n"
    "| `bad` | wrong flags | `build/tools/fake.exe scan --nope`, then `build/tools/fake.exe bogus`; `--ghost` too |\n"
    "| `gone` | missing | `build/tools/missing.exe --x` |\n"
    "| `broken` | crashes | `build/tools/broken.exe --anything` |\n"
    "| `hand` | hand help | `build/tools/hand.exe --known` and `build/tools/hand.exe --unknown` |\n"
    "| `build` | configure | `build/tools/configure.exe progress --non-matching && build/tools/configure.exe --bogus-flag` "
    "|\n"
    "| `pipe` | alternatives | `build/tools/fake.exe new <path> [--dry-run \\| --name N]` |\n"
    "| `old` | Python | `python tools/old.py --x`, `python -m tools.common index` |\n"
    "| `other` | accepted | `build/tools/hand.exe --known`; `cmake --workflow --preset tools-build`, "
    "`git status --short`, `tools/build_c.cmd --test`, `ctest --test-dir build/tools-cmake -C Release`, "
    "`rm -rf build && tools/bin/a.exe --verbose` |\n"
    "| `late` | built tree | `build/tools/late.exe --late` |\n";

/* docs/toolchain.md: the Toolchain list. */
static const char TOOLCHAIN_DOC[] =
    "# Toolchain\n"
    "\n"
    "**Toolchain** (pinned):\n"
    "- Compiler: `Wii/1.0` and `GC/3.0a5`.\n"
    "- tool-a v1.2.3: `tools/bin/a.exe`, from\n"
    "  https://example.invalid/a/v1.2.3/a.exe\n"
    "  (sha256 `" SHA_A "`).\n"
    "- tool-b v2.0: zip `b.zip` (sha256 `" SHA_X "`).\n"
    "- permuter: https://example.invalid/permuter at commit `" COMMIT "`. Python packages `toml` in a venv\n"
    "  (`python -m venv tools/venv`, then pip install toml capstone).\n";

/* The help texts the stub runner answers with, by the executable's file name and subcommand. */
static const char FAKE_HELP[] = "usage: fake.exe [-h] [--root ROOT] {scan,new} ...\n"
                                "\n"
                                "positional arguments:\n"
                                "  {scan,new}\n"
                                "\n"
                                "options:\n"
                                "  -h, --help   show this help message and exit\n"
                                "  --root ROOT\n";
static const char FAKE_SCAN_HELP[] = "usage: fake.exe scan [-h] [--deep]\n\noptions:\n  -h, --help\n  --deep\n";
static const char FAKE_NEW_HELP[] = "usage: fake.exe new [-h] [--dry-run] [--name NAME] path\n\n"
                                    "options:\n  -h, --help\n  --dry-run\n  --name NAME\n";
static const char HAND_HELP[] = "hand.exe [--known] [--root DIR]: does things by hand\n";
static const char CONFIGURE_HELP[] = "usage: configure.exe [-h] [--non-matching] [{configure,progress}]\n\n"
                                     "options:\n  -h, --help\n  --non-matching\n";
static const char LATE_HELP[] = "usage: late.exe [-h] [--late]\n";
static const char BIN_A_HELP[] = "a.exe [--verbose]\n";

typedef struct {
    const char *exe, *sub, *text;
} StubHelp;
static const StubHelp HELPS[] = {
    {"fake.exe", NULL, FAKE_HELP},           {"fake.exe", "scan", FAKE_SCAN_HELP}, {"fake.exe", "new", FAKE_NEW_HELP},
    {"hand.exe", NULL, HAND_HELP},           {"configure.exe", NULL, CONFIGURE_HELP},
    {"late.exe", NULL, LATE_HELP},           {"a.exe", NULL, BIN_A_HELP},
};

/* The stub help runner: argv is [exe, (sub,) --help]; broken.exe (and anything unknown) cannot be read. Thread safe:
 * it reads constant tables and returns heap memory. */
static char *stub_help(void *ctx, const char *const *argv, size_t argc, const char *root) {
    (void)ctx;
    (void)root;
    AvpStr exe = avp_path_basename(argv[0]);
    const char *sub = argc == 3 ? argv[1] : NULL;
    for (size_t i = 0; i < sizeof HELPS / sizeof HELPS[0]; i++) {
        if (!avp_str_eqz(exe, HELPS[i].exe)) continue;
        if ((sub == NULL) != (HELPS[i].sub == NULL) || (sub && strcmp(sub, HELPS[i].sub) != 0)) continue;
        size_t n = strlen(HELPS[i].text);
        char *copy = avp_xmalloc(n + 1);
        memcpy(copy, HELPS[i].text, n + 1);
        return copy;
    }
    return NULL;
}

/* ---- fixtures -------------------------------------------------------------------------------------------------- */
static void wrn(const char *root, const char *rel, const char *data, size_t n) {
    char *p = avp_path_join(A, root, rel);
    AVP_ASSERT(avp_makedirs(avp_cz(A, avp_path_dirname(p)), true));
    AVP_ASSERT(avp_write_file(p, data, n));
}
static void wr(const char *root, const char *rel, const char *text) { wrn(root, rel, text, strlen(text)); }

static void mkdirs(const char *root, const char *rel) { AVP_ASSERT(avp_makedirs(avp_path_join(A, root, rel), true)); }

/* An empty folder build/tmp/check_docs/<name>, absolute. */
static const char *fresh(const char *name) {
    const char *d = avp_path_abspath(A, avp_join(A, "build", "tmp", "check_docs", name));
    if (avp_isdir(d)) AVP_ASSERT(avp_rmtree(d, AVP_RMTREE_CLEAR_READONLY));
    AVP_ASSERT(avp_makedirs(d, true));
    return d;
}

/* Fixture.setUp. The stand-in executables: exe/ is the folder beside check_docs.exe, build/tools/ the checkout's
 * own build (late.exe only there, fake.exe in both). */
static const char *fixture(const char *name) {
    const char *r = fresh(name);
    wr(r, "AGENTS.md", AGENTS);
    wr(r, "docs/tools.md", TOOLS_DOC);
    wr(r, "docs/toolchain.md", TOOLCHAIN_DOC);
    wr(r, "configure.py", "# the project configuration\n");
    static const char *const beside[] = {"fake.exe", "hand.exe", "broken.exe", "configure.exe", "ledger.exe"};
    for (size_t i = 0; i < sizeof beside / sizeof beside[0]; i++) wr(r, FMT("exe/%s", beside[i]), "");
    wr(r, "build/tools/late.exe", "");
    wr(r, "build/tools/fake.exe", "");
    static const char *const tools[] = {"fake", "orphan", "mentioned", "deeply"};
    for (size_t i = 0; i < sizeof tools / sizeof tools[0]; i++) wr(r, FMT("tools/c/tools/%s/main.c", tools[i]), "");
    wr(r, "tools/c/tools/library_only/lib.c", "");
    wr(r, "tools/fake.cmd", "@echo off\n");
    wr(r, "tools/build_c.cmd", "@echo off\n");
    wr(r, "docs/notes.md", "Run `build/tools/mentioned.exe` for this.\nAlso `deeply.exe`.\n");
    wr(r, "src/game/Thing.cpp", "int x;\n");
    wr(r, "tools/bin/a.exe", ""); /* named in docs/toolchain.md */
    mkdirs(r, "tools/venv");     /* likewise */
    return r;
}

static const char *exe_dir(const char *root) { return avp_path_join(A, root, "exe"); }

/* Fixture.found: the report lines of one check (NULL: all), findings or notes. */
static AvpStrVec found(const CdReport *rep, const char *check, bool notes) {
    AvpStrVec v = {0};
    for (size_t i = 0; i < rep->items.n; i++) {
        const CdItem *it = &rep->items.v[i];
        if ((!check || !strcmp(it->check, check)) && it->note == notes) avp_vec_push(&v, avp_str(cd_item_text(A, it)));
    }
    return v;
}

static bool any_prefix_with(const AvpStrVec *v, const char *prefix, const char *text) {
    for (size_t i = 0; i < v->n; i++)
        if (avp_str_startswith(v->v[i], avp_str(prefix)) && avp_str_contains(v->v[i], avp_str(text))) return true;
    return false;
}
static bool any_with(const AvpStrVec *v, const char *text) { return any_prefix_with(v, "", text); }
static bool has_line(const AvpStrVec *v, const char *line) {
    for (size_t i = 0; i < v->n; i++)
        if (avp_str_eqz(v->v[i], line)) return true;
    return false;
}
static AvpStr joined(const AvpStrVec *v) { return avp_str_join(A, AVP_S("\n"), v->v, v->n); }

static void dump(const AvpStrVec *v) {
    for (size_t i = 0; i < v->n; i++) fprintf(stderr, "  | %.*s\n", AVP_SF(v->v[i]));
}

/* ---- VerbsTest ------------------------------------------------------------------------------------------------- */
static CdReport verbs_report(const char *name) {
    const char *r = fixture(name);
    CdReport rep;
    cd_report_init(&rep, A);
    CdHelpEnv env = {stub_help, NULL, exe_dir(r), 4};
    cd_check_verbs(r, &rep, &env);
    return rep;
}

AVP_TEST(verbs_findings) {
    CdReport rep = verbs_report("verbs_findings");
    AvpStrVec got = found(&rep, "VERBS", false);
    static const char *const want[][2] = {
        {"docs/tools.md:8:", "build/tools/fake.exe scan does not accept --nope"},
        {"docs/tools.md:8:", "build/tools/fake.exe has no subcommand `bogus`"},
        {"docs/tools.md:8:", "accepts --ghost"},
        {"docs/tools.md:9:", "build/tools/missing.exe does not exist beside check_docs.exe or in build/tools (build the "
                         "tools with tools/build_c.cmd)"},
        {"docs/tools.md:10:", "the help of build/tools/broken.exe cannot be read (build/tools/broken.exe --help)"},
        {"docs/tools.md:11:", "build/tools/hand.exe does not accept --unknown"},
        {"docs/tools.md:12:", "build/tools/configure.exe does not accept --bogus-flag"},
        {"docs/tools.md:14:", "`python tools/old.py` names a Python tool"},
        {"docs/tools.md:14:", "`python -m tools.common` names a Python tool"},
    };
    for (size_t i = 0; i < sizeof want / sizeof want[0]; i++)
        CHECK_MSG(any_prefix_with(&got, want[i][0], want[i][1]), "missing %s %s", want[i][0], want[i][1]);
    CHECK_EQ_I64(got.n, sizeof want / sizeof want[0]);
    if (got.n != sizeof want / sizeof want[0]) dump(&got);
}

AVP_TEST(verbs_accepted_forms_are_quiet) {
    CdReport rep = verbs_report("verbs_quiet");
    AvpStrVec got = found(&rep, "VERBS", false);
    AvpStr text = avp_str_replace(A, joined(&got), AVP_S("`bogus`"), AVP_S(""), -1);
    static const char *const ok[] = {"--deep", "--dry-run", "--name",   "--known", "--non-matching", "progress",
                                     "`new`",  "--late",    "--verbose", "--workflow", "--test-dir", "--short"};
    for (size_t i = 0; i < sizeof ok / sizeof ok[0]; i++)
        CHECK_MSG(!avp_str_contains(text, avp_str(FMT(" %s", ok[i]))), "%s reported", ok[i]);
    CHECK(!any_prefix_with(&got, "docs/tools.md:15:", ""));
    CHECK(!any_prefix_with(&got, "docs/tools.md:16:", ""));
}

AVP_TEST(verbs_undocumented_notes) {
    CdReport rep = verbs_report("verbs_notes");
    AvpStrVec notes = found(&rep, "VERBS", true);
    CHECK(has_line(&notes, "tools/c/tools/orphan/main.c:1: VERBS note: build/tools/orphan.exe is not in the verb "
                           "table and mentioned in no document"));
    CHECK(any_prefix_with(&notes, "tools/c/tools/mentioned/main.c:1:", "docs/notes.md:1"));
    CHECK(any_prefix_with(&notes, "tools/c/tools/deeply/main.c:1:", "docs/notes.md:2"));
    AvpStr all = joined(&notes);
    static const char *const absent[] = {"tools/c/tools/fake/", "library_only"};
    for (size_t i = 0; i < 2; i++) CHECK_MSG(!avp_str_contains(all, avp_str(absent[i])), "%s noted", absent[i]);
    CHECK_EQ_I64(notes.n, 3);
    if (notes.n != 3) dump(&notes);
}

/* ---- HelpParsingTest ------------------------------------------------------------------------------------------- */
AVP_TEST(invocations) {
    CdInvocations inv =
        cd_parse_invocations(A, AVP_S("rm -rf build && build/tools/configure.exe && tools/bin/ninja.exe"));
    REQUIRE(inv.n == 3);
    CHECK_EQ_I64(inv.v[0].kind, CD_INV_OTHER);
    CHECK_STR(inv.v[0].program, "rm");
    CHECK_EQ_I64(inv.v[1].kind, CD_INV_TOOL);
    CHECK_STR(inv.v[1].program, "build/tools/configure.exe");
    CHECK_EQ_I64(inv.v[2].kind, CD_INV_BIN);
    CHECK_STR(inv.v[2].program, "tools/bin/ninja");
    inv = cd_parse_invocations(A, AVP_S("build/tools/match.exe <unit> [SYMBOL...]"));
    REQUIRE(inv.n == 1);
    CHECK_STR(inv.v[0].program, "build/tools/match.exe");
    CHECK_STR(inv.v[0].args, " <unit> [SYMBOL...]");
    inv = cd_parse_invocations(A, AVP_S("tools/venv/Scripts/python.exe tools/mwcc_trace/permuter_run.py <dir> -j N"));
    REQUIRE(inv.n == 1);
    CHECK_EQ_I64(inv.v[0].kind, CD_INV_PYTHON);
    CHECK_STR(inv.v[0].program, "tools/venv/Scripts/python.exe tools/mwcc_trace/permuter_run.py");
    inv = cd_parse_invocations(A, AVP_S("python -m tools.common index --rebuild"));
    REQUIRE(inv.n == 1);
    CHECK_EQ_I64(inv.v[0].kind, CD_INV_PYTHON);
    CHECK_STR(inv.v[0].program, "python -m tools.common");
    inv = cd_parse_invocations(A, AVP_S("python -I  tools/rules/check_rules.py --json"));
    REQUIRE(inv.n == 1);
    CHECK_STR(inv.v[0].program, "python -I tools/rules/check_rules.py");
    inv = cd_parse_invocations(A, AVP_S("python configure.py progress"));
    REQUIRE(inv.n == 1);
    CHECK_EQ_I64(inv.v[0].kind, CD_INV_PYTHON);
    inv = cd_parse_invocations(A, AVP_S("tools/gate.py --quick"));
    REQUIRE(inv.n == 1);
    CHECK_EQ_I64(inv.v[0].kind, CD_INV_PYTHON);
    static const char *const others[] = {"git log --oneline", "cp a b", "ctest --test-dir x -L tool",
                                         "cmake --workflow --preset tools-build", "tools/build_c.cmd --test"};
    for (size_t i = 0; i < sizeof others / sizeof others[0]; i++) {
        inv = cd_parse_invocations(A, avp_str(others[i]));
        REQUIRE(inv.n == 1);
        CHECK_MSG(inv.v[0].kind == CD_INV_OTHER, "%s", others[i]);
    }
    CHECK_EQ_I64(cd_parse_invocations(A, AVP_S("ninja changes")).n, 0);
    CHECK_EQ_I64(cd_parse_invocations(A, AVP_S("<unit> --git")).n, 0); /* a program only as the first word */
    CHECK_EQ_I64(cd_parse_invocations(A, AVP_S("build/tools-B/check_docs.exe")).n, 0);
}

AVP_TEST(tokens) {
    AvpStrVec toks = cd_command_tokens(A, AVP_S("<unit> [-v A \\| -v all] [--vary \"-O3\\|-O4,p\"] [--set F]"));
    AvpStrVec flags = {0};
    for (size_t i = 0; i < toks.n; i++)
        if (cd_is_flag(toks.v[i])) avp_vec_push(&flags, toks.v[i]);
    CHECK_STR(avp_str_join(A, AVP_S(","), flags.v, flags.n), "-v,-v,--vary,--set");
}

AVP_TEST(adapter_rows) {
    const char *r = fresh("adapter_rows");
    wr(r, "docs/tools.md", TOOLS_DOC);
    CdRows rows = cd_adapter_rows(A, r);
    AvpStrVec verbs = {0};
    for (size_t i = 0; i < rows.n; i++) avp_vec_push(&verbs, rows.v[i].verb);
    CHECK_STR(avp_str_join(A, AVP_S(","), verbs.v, verbs.n), "scan,bad,gone,broken,hand,build,pipe,old,other,late");
    REQUIRE(rows.n > 0);
    CHECK_EQ_I64(rows.v[0].lineno, 7);
}

/* The project tool lookup: beside check_docs.exe first, then the checkout's build/tools, else none. */
AVP_TEST(tool_exe) {
    const char *r = fixture("tool_exe");
    CHECK_STR(avp_str(cd_tool_exe(A, exe_dir(r), r, "fake.exe")), avp_path_join(A, exe_dir(r), "fake.exe"));
    CHECK_STR(avp_str(cd_tool_exe(A, exe_dir(r), r, "late.exe")), avp_join(A, r, "build", "tools", "late.exe"));
    CHECK(cd_tool_exe(A, exe_dir(r), r, "missing.exe") == NULL);
}

/* ---- PathsTest ------------------------------------------------------------------------------------------------- */
AVP_TEST(paths) {
    const char *r = fixture("paths");
    wr(r, "docs/references.md", "| zeldaret/tp | Twilight Princess | `src/os/OSInit.c` |\n"
                                "| doldecomp/ogws | Wii Sports | |\n");
    static const char *const paras[] = {
        "Exists: `tools/fake.cmd`, `src/game/Thing.cpp:12`, `src/game/Thing.cpp::f`, `tools/`.",
        "Missing: `tools/gone.cmd`.",
        "Placeholders: `src/game/<Unit>.cpp`, `config/<ver>/symbols.txt`, `include/game/X.h`, "
        "`tools/compilers/Wii/*`.",
        "Bad prefix: `src/nowhere/<Unit>.cpp`.",
        "Generated: `build/R5WEA4/report.json`.",
        "Case: `tools/Fake.cmd`.",
        "Foreign: `zeldaret/tp src/os/OSAlarm.c`.",
        "Other project: doldecomp/ogws keeps `src/revolution/USB`.",
        "Removed: `include/old.h` was removed.",
        "```",
        "tools/fence_missing.cmd --x",
        "```",
    };
    AvpStr readme = avp_str_join(A, AVP_S("\n\n"), (const AvpStr[]){avp_str(paras[0]), avp_str(paras[1]),
                                     avp_str(paras[2]), avp_str(paras[3]), avp_str(paras[4]), avp_str(paras[5]),
                                     avp_str(paras[6]), avp_str(paras[7]), avp_str(paras[8]), avp_str(paras[9]),
                                     avp_str(paras[10]), avp_str(paras[11])}, 12);
    wr(r, "tools/rules/README.md", FMT("%.*s\n", AVP_SF(readme)));
    mkdirs(r, "include/game");
    mkdirs(r, "config/R5WEA4");
    mkdirs(r, "tools/compilers/Wii");
    CdReport rep;
    cd_report_init(&rep, A);
    cd_check_paths(r, &rep);
    AvpStrVec got = found(&rep, "PATHS", false);
    static const char *const want[] = {
        "tools/rules/README.md:3: PATHS tools/gone.cmd does not exist",
        "tools/rules/README.md:7: PATHS src/nowhere/<Unit>.cpp: src/nowhere does not exist (before the placeholder)",
        "tools/rules/README.md:11: PATHS tools/Fake.cmd does not exist",
        "tools/rules/README.md:21: PATHS tools/fence_missing.cmd does not exist",
        "docs/tools.md:14: PATHS tools/old.py does not exist",
    };
    for (size_t i = 0; i < sizeof want / sizeof want[0]; i++) CHECK_MSG(has_line(&got, want[i]), "missing %s", want[i]);
    CHECK_EQ_I64(got.n, sizeof want / sizeof want[0]);
    if (got.n != sizeof want / sizeof want[0]) dump(&got);
    AvpStrVec notes = found(&rep, "PATHS", true);
    CHECK_EQ_I64(notes.n, 3);
    CHECK(any_prefix_with(&notes, "docs/references.md:1:", "zeldaret/tp"));
    bool usb = false, old = false;
    for (size_t i = 0; i < notes.n; i++) {
        usb = usb || (avp_str_contains(notes.v[i], AVP_S("src/revolution/USB")) &&
                      avp_str_contains(notes.v[i], AVP_S("another project")));
        old = old || (avp_str_contains(notes.v[i], AVP_S("include/old.h")) &&
                      avp_str_contains(notes.v[i], AVP_S("removal")));
    }
    CHECK(usb);
    CHECK(old);
}

/* ---- PinsTest -------------------------------------------------------------------------------------------------- */
static const char *const PACKAGES[] = {"toml", "Levenshtein", NULL};
static const char *const PACKAGES_TOML[] = {"toml", NULL};
static const char *const COMPILERS_REQUIRED[] = {"Wii/1.0", NULL};
static const char *const COMPILERS_OPTIONAL[] = {"GC/9.9", NULL};

/* fake_doctor(): the pin table of the Python test, in its order. */
static void fake_pins(DoctorPin pins[5]) {
    pins[0] = (DoctorPin){.key = "a", .version = "v1.2.3", .url = "https://example.invalid/a/v1.2.3/a.exe", .sha256 = SHA_A};
    pins[1] = (DoctorPin){.key = "b", .version = "v2.0", .asset = "b.zip", .zip_sha256 = SHA_B};
    pins[2] = (DoctorPin){.key = "permuter", .url = "https://example.invalid/permuter", .commit = COMMIT};
    pins[3] = (DoctorPin){.key = "venv", .packages = PACKAGES};
    pins[4] = (DoctorPin){.key = "dol", .sha1 = DOL};
}

AVP_TEST(pins) {
    const char *r = fixture("pins");
    DoctorPin table[5];
    fake_pins(table);
    CdPins pins = {table, 5, COMPILERS_REQUIRED, COMPILERS_OPTIONAL};
    CdReport rep;
    cd_report_init(&rep, A);
    cd_check_pins(r, &rep, &pins);
    AvpStrVec got = found(&rep, "PINS", false);
    CHECK(any_with(&got, "b.zip_sha256 " SHA_B " is not in the Toolchain list"));
    CHECK(any_with(&got, "sha256 " SHA_X " is not pinned"));
    CHECK(any_with(&got, "package Levenshtein"));
    CHECK(any_with(&got, "compiler GC/9.9"));
    CHECK_EQ_I64(got.n, 4);
    if (got.n != 4) dump(&got);
}

AVP_TEST(pins_version_and_files) {
    const char *r = fixture("pins_version");
    wr(r, "config/R5WEA4/build.sha1", "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee  build/R5WEA4/main.dol\n");
    wr(r, "config/R5WEA4/config.yml", "hash: " DOL "\n");
    wr(r, "configure.py", "config.a_tag = \"v1.2.4\"\nconfig.b_tag = \"v2.0\"\n");
    DoctorPin table[5];
    fake_pins(table);
    table[0].version = "v9.9.9";
    table[1].zip_sha256 = SHA_X;
    table[3].packages = PACKAGES_TOML;
    static const char *const none[] = {NULL};
    CdPins pins = {table, 5, COMPILERS_REQUIRED, none};
    CdReport rep;
    cd_report_init(&rep, A);
    cd_check_pins(r, &rep, &pins);
    AvpStrVec got = found(&rep, "PINS", false);
    CHECK_EQ_I64(got.n, 3);
    if (got.n != 3) dump(&got);
    CHECK(any_prefix_with(&got, "docs/toolchain.md:5:", "does not name version v9.9.9"));
    CHECK(any_prefix_with(&got, "config/R5WEA4/build.sha1:1:", ""));
    CHECK(any_prefix_with(&got, "configure.py:1:", "config.a_tag v1.2.4"));
}

AVP_TEST(toolchain_entries) {
    const char *r = fixture("toolchain_entries");
    CdEntries e = cd_toolchain_entries(A, r);
    REQUIRE(e.n == 4);
    CHECK(avp_str_contains(e.v[1].text, AVP_S(SHA_A)));
    CHECK_EQ_I64(e.v[1].lineno, 5);
}

/* ---- ExceptionsTest -------------------------------------------------------------------------------------------- */
typedef struct {
    i64 rc;
    const char *out;
    AvpStrVec argv;
} StubRun;

static bool stub_runner(void *ctx, const char *const *argv, size_t argc, const char *root, i64 *rc, AvpStr *out,
                        const char **err) {
    (void)root;
    (void)err;
    StubRun *s = ctx;
    s->argv.n = 0;
    for (size_t i = 0; i < argc; i++) avp_vec_push(&s->argv, avp_str(avp_strdup(A, argv[i])));
    *rc = s->rc;
    *out = avp_str(s->out);
    return true;
}

AVP_TEST(exceptions_clean) {
    const char *r = fixture("exc_clean");
    CdReport rep;
    cd_report_init(&rep, A);
    StubRun s = {0, "ok\n", {0}};
    CHECK_EQ_I64(cd_check_exceptions(r, &rep, exe_dir(r), stub_runner, &s), 0);
    CHECK_EQ_I64(rep.items.n, 0);
}

AVP_TEST(exceptions_findings_and_errors) {
    const char *r = fixture("exc_findings");
    CdReport rep;
    cd_report_init(&rep, A);
    StubRun s = {1, "docs/exceptions.md:12: OPEN-ITEM fn_80001234 is not in symbols.txt\nsummary: 1 finding\n", {0}};
    CHECK_EQ_I64(cd_check_exceptions(r, &rep, exe_dir(r), stub_runner, &s), 1);
    AvpStrVec got = found(&rep, "EXCEPTIONS", false);
    CHECK(has_line(&got, "docs/exceptions.md:12: EXCEPTIONS OPEN-ITEM fn_80001234 is not in symbols.txt"));
    CHECK(has_line(&got, "docs/exceptions.md:0: EXCEPTIONS summary: 1 finding"));
    CdReport rep2;
    cd_report_init(&rep2, A);
    StubRun s2 = {2, "ledger: error: no report\n", {0}};
    cd_check_exceptions(r, &rep2, exe_dir(r), stub_runner, &s2);
    bool env = false;
    for (size_t i = 0; i < rep2.items.n; i++) env = env || strstr(rep2.items.v[i].message, "environment error");
    CHECK(env);
}

/* ledger.exe beside check_docs.exe, else the checkout's build/tools/ledger.exe (D-check_docs-2). */
AVP_TEST(exceptions_argv) {
    const char *r = fixture("exc_argv");
    CdReport rep;
    cd_report_init(&rep, A);
    StubRun s = {0, "", {0}};
    cd_check_exceptions(r, &rep, exe_dir(r), stub_runner, &s);
    REQUIRE(s.argv.n == 2);
    CHECK_STR(s.argv.v[0], avp_path_join(A, exe_dir(r), "ledger.exe"));
    CHECK_STR(s.argv.v[1], "exceptions-check");
    wr(r, "build/tools/ledger.exe", "");
    mkdirs(r, "empty");
    cd_check_exceptions(r, &rep, avp_path_join(A, r, "empty"), stub_runner, &s);
    REQUIRE(s.argv.n == 2);
    CHECK_STR(s.argv.v[0], avp_join(A, r, "build", "tools", "ledger.exe"));
    CHECK_EQ_I64(rep.items.n, 0);
}

/* Neither beside check_docs.exe nor in build/tools: one finding naming the build command, nothing run. */
AVP_TEST(exceptions_no_ledger) {
    const char *r = fixture("exc_no_ledger");
    mkdirs(r, "empty");
    CdReport rep;
    cd_report_init(&rep, A);
    StubRun s = {0, "", {0}};
    CHECK_EQ_I64(cd_check_exceptions(r, &rep, avp_path_join(A, r, "empty"), stub_runner, &s), -1);
    CHECK_EQ_I64(s.argv.n, 0);
    AvpStrVec got = found(&rep, "EXCEPTIONS", false);
    CHECK_EQ_I64(got.n, 1);
    CHECK(has_line(&got, "build/tools/ledger.exe:0: EXCEPTIONS exceptions-check could not run: no ledger.exe beside "
                         "check_docs.exe or in build/tools (build the tools with tools/build_c.cmd)"));
}

/* ---- TextTest -------------------------------------------------------------------------------------------------- */
static AvpStrVec text_check(const char *rel, const char *data, size_t n, bool insert, bool trim) {
    CdReport rep;
    cd_report_init(&rep, A);
    cd_check_text_file(&rep, rel, (AvpStr){data, n}, (CdFinalNewline){insert, trim});
    return found(&rep, NULL, false);
}
#define TEXT(rel, lit) text_check(rel, lit, sizeof(lit) - 1, false, false)
#define TEXT_RULES(rel, lit) text_check(rel, lit, sizeof(lit) - 1, true, true)

static void check_only(AvpStrVec got, const char *line) {
    CHECK_EQ_I64(got.n, 1);
    if (got.n == 1)
        CHECK_STR(got.v[0], line);
    else
        dump(&got);
}

AVP_TEST(text_bom_and_line_endings) {
    check_only(TEXT("docs/a.md", "\xef\xbb\xbfhello\n"), "docs/a.md:1: TEXT UTF-8 byte-order mark");
    check_only(TEXT("tools/a.py", "a\r\nb\r\nc\nd\r\n"), "tools/a.py:3: TEXT mixed line endings: 3 CRLF, 1 LF");
    CHECK_EQ_I64(TEXT("tools/a.py", "a\r\nb\r\n").n, 0);
}

AVP_TEST(text_source_encoding) {
    /* "\u30c6\u30b9\u30c8" in cp932 */
    AvpStrVec got = TEXT("src/a.c", "int a;\n// \x83\x65\x83\x58\x83\x67\n");
    REQUIRE(got.n == 1);
    CHECK(avp_str_contains(got.v[0], AVP_S("src/a.c:2:")));
    CHECK(avp_str_contains(got.v[0], AVP_S("valid Shift JIS, not UTF-8")));
    got = TEXT("include/a.h", "int a;\n\x81\x20\n");
    REQUIRE(got.n >= 1);
    CHECK(avp_str_contains(got.v[0], AVP_S("neither UTF-8 nor Shift JIS")));
    CHECK_EQ_I64(TEXT("src/a.c", "// \xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88\n").n, 0);
    check_only(TEXT("src/a.c", "int a;\x0c\n"), "src/a.c:1: TEXT control character U+000C");
    CHECK_EQ_I64(TEXT("docs/a.md", "\xff\n").n, 0); /* encoding is checked in src/ and include/ only */
}

AVP_TEST(text_tabs) {
    check_only(TEXT("tools/a.py", "if x:\n\ty = 1\n"), "tools/a.py:2: TEXT tab in indentation (flake8 W191)");
    CHECK_EQ_I64(TEXT("tools/a.py", "X = \"\"\"\n\tdata with a tab\n\"\"\"\n").n, 0);
    CHECK_EQ_I64(TEXT("src/a.c", "\tint a;\n").n, 0);
}

AVP_TEST(text_final_newline) {
    check_only(TEXT_RULES("src/a.c", "int a;"),
               "src/a.c:1: TEXT no final newline (.vscode/settings.json insertFinalNewline)");
    CHECK_EQ_I64(TEXT_RULES("tools/a.py", "x = 1\n\n").n, 1);
    CHECK_EQ_I64(TEXT_RULES("src/a.c", "int a;\n").n, 0);
    CHECK_EQ_I64(TEXT_RULES("config/R5WEA4/baseline_report.json", "{}").n, 0);
    CHECK_EQ_I64(TEXT("src/a.c", "int a;").n, 0);
}

AVP_TEST(text_binary_skipped) { CHECK_EQ_I64(TEXT("tools/a.py", "\xef\xbb\xbf\0\0binary").n, 0); }

AVP_TEST(vscode_rules) {
    const char *r = fresh("vscode_rules");
    CdFinalNewline f = cd_vscode_rules(r);
    CHECK(!f.insert_final_newline && !f.trim_final_newlines);
    wr(r, ".vscode/settings.json", "{\n  // \"files.trimFinalNewlines\": true,\n  \"files.insertFinalNewline\": true,\n}\n");
    f = cd_vscode_rules(r);
    CHECK(f.insert_final_newline);
    CHECK(!f.trim_final_newlines);
}

/* ---- MainTest -------------------------------------------------------------------------------------------------- */
/* check_docs.exe beside the tests' folder (build/tools/check_docs.exe): --root root, then args; stdout and the code. */
static i64 run_main(const char *root, const char *const *args, size_t n, AvpStr *out) {
    const char **argv = AVP_NEWN(A, const char *, n + 4);
    argv[0] = avp_path_join(A, avp_cz(A, avp_path_dirname(avp_exe_dir())), "check_docs.exe");
    argv[1] = "--root";
    argv[2] = root;
    for (size_t i = 0; i < n; i++) argv[i + 3] = args[i];
    AvpProcResult r = avp_run(&(AvpProcSpec){.argv = argv, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE, .text = true,
                                             .timeout = 300, .arena = A});
    AVP_ASSERT(r.status == AVP_PROC_OK);
    *out = r.out;
    return r.returncode;
}

AVP_TEST(main_json_and_exit) {
    const char *r = fixture("main_json");
    wr(r, "docs/x.md", "`tools/gone.py`\n");
    AvpStr out;
    CHECK_EQ_I64(run_main(r, (const char *const[]){"--only", "paths", "--json"}, 3, &out), 1);
    AvpJson *doc = avp_json_parse(A, out, NULL);
    REQUIRE(doc);
    AvpJson *paths = avp_json_getz(avp_json_getz(doc, "summary"), "paths");
    REQUIRE(paths);
    CHECK_EQ_I64(avp_json_len(avp_json_getz(doc, "summary")), 1);
    CHECK_EQ_I64(avp_json_get_i64(paths, "findings", -1), 2);
    CHECK_EQ_I64(avp_json_get_i64(paths, "notes", -1), 0);
    AvpJson *f = avp_json_getz(doc, "findings");
    REQUIRE(avp_json_len(f) == 2);
    AvpStr a = avp_json_get_str(avp_json_at(f, 0), "file", AVP_S(""));
    AvpStr b = avp_json_get_str(avp_json_at(f, 1), "file", AVP_S(""));
    CHECK((avp_str_eqz(a, "docs/tools.md") && avp_str_eqz(b, "docs/x.md")) ||
          (avp_str_eqz(b, "docs/tools.md") && avp_str_eqz(a, "docs/x.md")));
}

AVP_TEST(main_text_summary_and_clean_exit) {
    const char *r = fixture("main_text");
    wr(r, "tools/old.py", "");
    AvpStr out;
    CHECK_EQ_I64(run_main(r, (const char *const[]){"--only", "paths"}, 2, &out), 0);
    AvpStrVec lines = avp_str_splitlines(out, false);
    REQUIRE(lines.n > 0);
    CHECK_STR(lines.v[lines.n - 1], "paths: ok, 0 notes");
}

AVP_TEST(main_bad_only) {
    const char *r = fixture("main_bad_only");
    AvpStr out;
    CHECK_EQ_I64(run_main(r, (const char *const[]){"--only", "paths,nonsense"}, 2, &out), 2);
}

AVP_TEST_MAIN("check_docs")
