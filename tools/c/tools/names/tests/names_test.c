/* tools/test_names.py ported: the synthetic checkout of tools/common/_fixture.py extended as the Python test extends
 * it (source, headers, docs, config.yml and a compiled object), names.exe run on it with --json, and rename --apply
 * run only on these fixture copies (build/tmp/names-test/<test>). The real-tree smoke test runs dry runs only. Also
 * unit checks of the helpers the tool exposes (split_params, clip, def_name, plain_of, elf_functions, the planner). */
#include "avp/fs.h"
#include "avp/json.h"
#include "avp/test.h"
#include "names.h"
#include "shsm_cli.h"

#include <string.h>

#define V "TESTV1"

static const char CONFIGURE[] = "\n"
                                "DEFAULT_VERSION = 0\n"
                                "VERSIONS = [\n"
                                "    \"TESTV1\",  # 0: test\n"
                                "]\n"
                                "config.libs = [\n"
                                "    {\n"
                                "        \"lib\": \"game\",\n"
                                "        \"objects\": [\n"
                                "            Object(Matching, \"game/Alpha.cpp\"),\n"
                                "            Object(\n"
                                "                NonMatching,\n"
                                "                \"game/Beta.c\",\n"
                                "                extra_cflags=[\"-O0\"],\n"
                                "            ),\n"
                                "            Object(Equivalent, \"game/Gamma.c\"),\n"
                                "        ],\n"
                                "    },\n"
                                "]\n";

static const char SYMBOLS[] =
    "fn_80001000__Fv = .text:0x80001000; // type:function size:0x10 scope:global\n"
    "helper = .text:0x80001010; // type:function size:0x8 scope:local align:4\n"
    "fn_80001018__8unk_RampFf = .text:0x80001018; // type:function size:0xC\n"
    "jumper = .text:0x80001100; // type:function size:0x8 scope:global\n"
    "lbl_80002000 = .data:0x80002000; // type:object size:0x6 scope:global data:string\n"
    "lbl_80002008 = .data:0x80002008; // type:object size:0xC scope:global\n"
    "lbl_80003000 = .sdata2:0x80003000; // type:object size:0x4 scope:local data:float\n"
    "lbl_80003008 = .sdata2:0x80003008; // type:object size:0x8 scope:local data:double hidden\n"
    "start_label = .text:0x80001000; // type:label scope:global\n"
    /* EXTRA_SYMBOLS */
    "Tick__8unk_RampFv = .text:0x80001024; // type:function size:0x4 scope:global\n"
    "Tick__8unk_ZetaFv = .text:0x80001028; // type:function size:0x4 scope:global\n"
    "__ct__8unk_RampFv = .text:0x8000102C; // type:function size:0x4 scope:global\n"
    "fn_80001030__FR8unk_Ramp = .text:0x80001030; // type:function size:0x4 scope:global\n"
    "__vt__8unk_Ramp = .data:0x80002100; // type:object size:0xC scope:global\n"
    "fn_80001034 = .text:0x80001034; // type:function size:0x4 scope:global\n"
    "displayHeightF = .sbss:0x80004000; // type:object size:0x4 scope:local\n"
    "lbl_80004008 = .sbss:0x80004008; // type:object size:0x4 scope:global\n";

static const char SPLITS[] = "Sections:\n"
                             "\t.text       type:code align:8\n"
                             "\t.data       type:data align:32\n"
                             "\n"
                             "game/Alpha.cpp:\n"
                             "\t.text       start:0x80001000 end:0x80001024\n"
                             "\t.data       start:0x80002000 end:0x80002014\n"
                             "\t.sdata2     start:0x80003000 end:0x80003010\n"
                             "\n"
                             "game/Beta.c:\n"
                             "\t.text       start:0x80001100 end:0x80001108\n";

static const char OBJDIFF[] =
    "{\"units\": [{\"name\": \"main/game/Alpha\", \"target_path\": \"build/TESTV1/obj/game/Alpha.o\", \"base_path\": "
    "\"build/TESTV1/src/game/Alpha.o\", \"metadata\": {\"complete\": true, \"source_path\": \"src/game/Alpha.cpp\", "
    "\"progress_categories\": [\"game\"], \"auto_generated\": false}}, {\"name\": \"main/game/Beta\", "
    "\"target_path\": \"build/TESTV1/obj/game/Beta.o\", \"base_path\": \"build/TESTV1/src/game/Beta.o\", "
    "\"metadata\": {\"complete\": false, \"source_path\": \"src/game/Beta.c\", \"progress_categories\": "
    "[\"game\"]}}, {\"name\": \"main/auto_01_80005000_text\", \"target_path\": "
    "\"build/TESTV1/obj/auto_01_80005000_text.o\", \"metadata\": {\"auto_generated\": true}}]}";

static const char NINJA[] = "# Source files\n"
                            "# game/Alpha.cpp: game (linked True)\n"
                            "build build\\TESTV1\\src\\game\\Alpha.o: mwcc_sjis src\\game\\Alpha.cpp | $\n"
                            "    tools\\compilers\\$mw_version\\mwcceppc.exe || pre-compile\n"
                            "  mw_version = Wii\\1.0\n"
                            "  cflags = -nodefaults -proc gekko -pragma \"cats off\" -O4,p $\n"
                            "      -i include -lang=c++\n"
                            "  basedir = build\\TESTV1\\src\\game\n"
                            "\n"
                            "# game/Beta.c: game (linked\n"
                            "# False)\n"
                            "build $\n"
                            "    build\\TESTV1\\src\\game\\Beta.o: $\n"
                            "    mwcc $\n"
                            "    src\\game\\Beta.c\n"
                            "  mw_version = GC\\3.0a5\n"
                            "  cflags = -O4,p -O0\n";

static const char ALPHA_CPP[] = "#include <game/Alpha.h>\n"
                                "\n"
                                "/*\n"
                                "====================\n"
                                "unk_Ramp::fn_80001018\n"
                                "====================\n"
                                "*/\n"
                                "// 0x80001018\n"
                                "void unk_Ramp::fn_80001018( float dt ) {\n"
                                "    helper();\n"
                                "}\n"
                                "\n"
                                "// 0x80001000\n"
                                "void wrong_name( void ) {\n"
                                "}\n"
                                "\n"
                                "unk_Ramp::unk_Ramp() {\n"
                                "} // 0x8000102C\n"
                                "\n"
                                "void fn_80001030( unk_Ramp& ramp ) {\n"
                                "    ramp.fn_80001018( 1.0f );\n"
                                "} // 0x80001030\n";

static const char BETA_C[] = "void jumper( void ) {\n}\n\nvoid deadfn( void ) {\n} // no copy in retail\n";

static const char ALPHA_H[] = "class unk_Ramp {\n"
                              "public:\n"
                              "    unk_Ramp();\n"
                              "    void fn_80001018( float dt );\n"
                              "    void Tick();\n"
                              "};\n";

static const char DOC[] = "# Exceptions record\n"
                          "\n"
                          "| `game/Alpha` | `fn_80001018__8unk_RampFf` | R17 |\n"
                          "\n"
                          "The ramp member `fn_80001018` of `unk_Ramp` keeps `fn_80001018x` apart.\n";

static const char CONFIG_YML[] = "object: sys/main.dol\nforce_active:\n  - fn_80001018__8unk_RampFf\n  - helper\n";

static const char STUB_CPP[] = "/*\n====================\nfn_80001040\n====================\n*/\n// 0x80001040\n"
                               "extern \"C\" void fn_80001040() {\n    // TODO\n}\n";
static const char STUB_H[] = "#ifdef __cplusplus\nextern \"C\" {\n#endif\nvoid fn_80001040();\n#ifdef __cplusplus\n}\n"
                             "#endif\n";
static const char GEN_SYMBOLS[] = "fn_80001040 = .text:0x80001040; // type:function size:0x4 scope:global\n"
                                  "__sinit_\\Alpha_cpp = .text:0x80001044; // type:function size:0x4 scope:local\n"
                                  "@36@Tick__8unk_RampFv = .text:0x80001048; // type:function size:0x8 scope:global\n"
                                  "@stringBase0 = .rodata:0x80003000; // type:object size:0x4 scope:local "
                                  "data:string\n";

/* ---- fixture ---- */

static void w(const char *root, const char *rel, const char *text) {
    AvpArena *a = avp_arena();
    const char *p = avp_join(a, root, rel);
    avp_makedirs(avp_cz(a, avp_path_dirname(p)), true);
    AVP_ASSERT(avp_write_file(p, text, strlen(text)));
}

static void append(const char *root, const char *rel, const char *text) {
    AvpArena *a = avp_arena();
    const char *p = avp_join(a, root, rel);
    AvpBytes b = {0};
    AVP_ASSERT(avp_read_all(a, p, &b));
    AvpBuf nb;
    avp_buf_init(&nb, a);
    avp_buf_putn(&nb, b.v, b.n);
    avp_buf_putz(&nb, text);
    AvpStr all = avp_buf_str(&nb);
    AVP_ASSERT(avp_write_file(p, all.p, all.n));
}

static void be32(AvpBuf *b, u32 v) {
    char c[4] = {(char)(v >> 24), (char)(v >> 16), (char)(v >> 8), (char)v};
    avp_buf_putn(b, c, 4);
}
static void be16(AvpBuf *b, u32 v) {
    char c[2] = {(char)(v >> 8), (char)v};
    avp_buf_putn(b, c, 2);
}
static void pad4(AvpBuf *b) {
    while (avp_buf_str(b).n % 4) avp_buf_putc(b, 0);
}

/* make_elf: an ELF32 big-endian object with one .text section and FUNC symbols (name, binding) */
static void make_elf(const char *path, const char *const *names, const int *binds, size_t n) {
    AvpArena *a = avp_arena();
    AvpBuf strtab, symtab, body, out;
    avp_buf_init(&strtab, a);
    avp_buf_init(&symtab, a);
    avp_buf_init(&body, a);
    avp_buf_init(&out, a);
    avp_buf_putc(&strtab, 0);
    for (int i = 0; i < 16; i++) avp_buf_putc(&symtab, 0);
    for (size_t i = 0; i < n; i++) {
        u32 off = (u32)avp_buf_str(&strtab).n;
        avp_buf_putz(&strtab, names[i]);
        avp_buf_putc(&strtab, 0);
        be32(&symtab, off);
        be32(&symtab, 0);
        be32(&symtab, 4);
        avp_buf_putc(&symtab, (char)((binds[i] << 4) | 2));
        avp_buf_putc(&symtab, 0);
        be16(&symtab, 1);
    }
    static const char SHSTR[] = "\0.text\0.symtab\0.strtab\0.shstrtab\0";
    AvpStr parts[4] = {AVP_S("\0\0\0\0"), avp_buf_str(&symtab), avp_buf_str(&strtab), {SHSTR, sizeof SHSTR - 1}};
    u32 offs[4];
    for (int i = 0; i < 4; i++) {
        offs[i] = 0x34 + (u32)avp_buf_str(&body).n;
        avp_buf_put(&body, parts[i]);
        pad4(&body);
    }
    u32 shoff = 0x34 + (u32)avp_buf_str(&body).n;
    avp_buf_putn(&out, "\x7f" "ELF\x01\x02\x01", 7);
    for (int i = 0; i < 9; i++) avp_buf_putc(&out, 0);
    be16(&out, 1);
    be16(&out, 20);
    be32(&out, 1);
    be32(&out, 0);
    be32(&out, 0);
    be32(&out, shoff);
    be32(&out, 0);
    be16(&out, 0x34);
    be16(&out, 0);
    be16(&out, 0);
    be16(&out, 40);
    be16(&out, 5);
    be16(&out, 4);
    avp_buf_put(&out, avp_buf_str(&body));
    for (int i = 0; i < 40; i++) avp_buf_putc(&out, 0);
    u32 sh[4][10] = {{1, 1, 6, 0, offs[0], 4, 0, 0, 4, 0},
                     {7, 2, 0, 0, offs[1], (u32)parts[1].n, 3, 1, 4, 16},
                     {15, 3, 0, 0, offs[2], (u32)parts[2].n, 0, 0, 1, 0},
                     {23, 3, 0, 0, offs[3], (u32)parts[3].n, 0, 0, 1, 0}};
    for (int i = 0; i < 4; i++)
        for (int k = 0; k < 10; k++) be32(&out, sh[i][k]);
    avp_makedirs(avp_cz(a, avp_path_dirname(path)), true);
    AVP_ASSERT(avp_write_file(path, avp_buf_str(&out).p, avp_buf_str(&out).n));
}

static const char *make_checkout(const char *name, bool special) {
    AvpArena *a = avp_arena();
    const char *root = avp_path_abspath(a, avp_join(a, "build", "tmp", "names-test", name));
    if (avp_isdir(root)) avp_rmtree(root, 0);
    AVP_ASSERT(avp_makedirs(root, true));
    w(root, "configure.py", CONFIGURE);
    w(root, "tools/.keep", "");
    w(root, "config/" V "/symbols.txt", SYMBOLS);
    w(root, "config/" V "/splits.txt", SPLITS);
    w(root, "objdiff.json", OBJDIFF);
    w(root, "build.ninja", NINJA);
    w(root, "config/" V "/baseline_report.json", "{\"units\": [{\"functions\": [{\"name\": \"fn_80001000__Fv\"}, "
                                                 "{\"name\": \"helper\"}, {\"name\": \"jumper\"}]}]}");
    w(root, "src/game/Alpha.cpp", ALPHA_CPP);
    w(root, "src/game/Beta.c", BETA_C);
    w(root, "include/game/Alpha.h", ALPHA_H);
    w(root, "include/game/unk_Ramp.h", "#include <game/Alpha.h>\n");
    w(root, "docs/exceptions.md", DOC);
    w(root, "config/" V "/config.yml", CONFIG_YML);
    w(root, "AGENTS.md", "see fn_80001018__8unk_RampFf\n");
    static const char *const FN[] = {"jumper", "orphan", "deadfn", "inline_thing", "local_one"};
    static const int BIND[] = {1, 1, 2 - 1, 2, 0};
    make_elf(avp_join(a, root, "build", V, "src", "game", "Beta.o"), FN, BIND, 5);
    if (special) {
        append(root, "config/" V "/symbols.txt", GEN_SYMBOLS);
        w(root, "src/game/Stub.cpp", STUB_CPP);
        w(root, "include/game/Stub.h", STUB_H);
    }
    return root;
}

static const char *read_rel(const char *root, const char *rel) {
    AvpArena *a = avp_arena();
    AvpBytes b = {0};
    AVP_ASSERT(avp_read_all(a, avp_join(a, root, rel), &b));
    return avp_cz(a, avp_strn((const char *)b.v, b.n));
}

typedef struct {
    i64 rc;
    AvpStr out, err;
    AvpJson *j;
} Run;

/* names.exe <args> --root root [--json] */
static Run run_names(const char *root, bool json, const char *const *args, size_t n) {
    AvpArena *a = avp_arena();
    const char **argv = AVP_NEWN(a, const char *, n + 5);
    size_t k = 0;
    argv[k++] = avp_join(a, avp_exe_dir(), "..", "names.exe");
    for (size_t i = 0; i < n; i++) argv[k++] = args[i];
    argv[k++] = "--root";
    argv[k++] = root;
    if (json) argv[k++] = "--json";
    ShsmRun r = shsm_run(a, argv, k, NULL, 300);
    Run out = {r.returncode, r.out, r.err, NULL};
    if (json) {
        AvpJsonError e;
        out.j = avp_json_parse(a, r.out, &e);
    }
    return out;
}
#define RUN(root, json, ...) run_names(root, json, (const char *const[]){__VA_ARGS__}, \
                                       sizeof((const char *const[]){__VA_ARGS__}) / sizeof(const char *))

static bool any_contains(const AvpJson *arr, const char *needle, const char *needle2) {
    for (size_t i = 0; i < avp_json_len(arr); i++) {
        AvpStr s = avp_json_str_or(avp_json_at(arr, i), AVP_S(""));
        if (avp_str_contains(s, avp_str(needle)) && (!needle2 || avp_str_contains(s, avp_str(needle2)))) return true;
    }
    return false;
}

/* ---- NamesHelpTest ---- */

AVP_TEST(top_level_help_lists_every_subcommand) {
    Run r = RUN(avp_root(), false, "--help");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(avp_str_contains(r.out, AVP_S("{demangle,mangle,find,rename,attr,add,check} ...")));
}

/* ---- NamesBasicsTest ---- */

AVP_TEST(demangle) {
    const char *root = make_checkout("basics-demangle", false);
    Run r = RUN(root, true, "demangle", "fn_80001018__8unk_RampFf", "memcpy");
    CHECK_EQ_I64(r.rc, 0);
    REQUIRE(r.j);
    const AvpJson *names = avp_json_getz(r.j, "names");
    CHECK_STR(avp_json_get_str(avp_json_at(names, 0), "demangled", AVP_S("")), "unk_Ramp::fn_80001018(float)");
    CHECK(avp_json_is_null(avp_json_getz(avp_json_at(names, 1), "demangled")));
}

AVP_TEST(mangle) {
    const char *root = make_checkout("basics-mangle", false);
    Run r = RUN(root, true, "mangle", "--class", "unk_Ramp", "--name", "fn_80001018", "--params", "float");
    REQUIRE(r.j);
    CHECK_STR(avp_json_get_str(r.j, "mangled", AVP_S("")), "fn_80001018__8unk_RampFf");
    r = RUN(root, true, "mangle", "--name", "Foo", "--params", "const char*, int", "--class", "A::B", "--const");
    REQUIRE(r.j);
    CHECK_STR(avp_json_get_str(r.j, "demangled", AVP_S("")), "A::B::Foo(const char*, int) const");
    r = RUN(root, true, "mangle", "--name", "f", "--params", "void");
    REQUIRE(r.j);
    CHECK_STR(avp_json_get_str(r.j, "mangled", AVP_S("")), "f__Fv");
}

AVP_TEST(find) {
    const char *root = make_checkout("basics-find", false);
    Run r = RUN(root, true, "find", "unk_ramp");
    REQUIRE(r.j);
    const AvpJson *syms = avp_json_getz(r.j, "symbols");
    bool member = false, vt = false;
    for (size_t i = 0; i < avp_json_len(syms); i++) {
        AvpStr n = avp_json_get_str(avp_json_at(syms, i), "name", AVP_S(""));
        member |= avp_str_eqz(n, "fn_80001018__8unk_RampFf");
        vt |= avp_str_eqz(n, "__vt__8unk_Ramp");
    }
    CHECK(member && vt);
    CHECK_EQ_I64(RUN(root, false, "find", "zzz").rc, 1);
}

AVP_TEST(check) {
    const char *root = make_checkout("basics-check", false);
    Run r = RUN(root, true, "check");
    CHECK_EQ_I64(r.rc, 1);
    REQUIRE(r.j);
    const AvpJson *ns = avp_json_getz(r.j, "no_symbol");
    REQUIRE(avp_json_len(ns) == 2);
    CHECK_STR(avp_json_get_str(avp_json_at(ns, 0), "name", AVP_S("")), "orphan");
    CHECK_STR(avp_json_get_str(avp_json_at(ns, 1), "name", AVP_S("")), "local_one");
    const AvpJson *counts = avp_json_getz(r.j, "counts");
    CHECK_EQ_I64(avp_json_get_i64(counts, "weak", -1), 1);
    CHECK_EQ_I64(avp_json_get_i64(counts, "dead_marked", -1), 1);
    CHECK_EQ_I64(avp_json_get_i64(counts, "address_comments", -1), 4);
    const AvpJson *ad = avp_json_getz(r.j, "address");
    REQUIRE(avp_json_len(ad) == 1);
    CHECK_EQ_I64(avp_json_get_i64(avp_json_at(ad, 0), "line", 0), 13);
    CHECK_STR(avp_json_get_str(avp_json_at(ad, 0), "function", AVP_S("")), "wrong_name");
}

/* ---- RenameTest ---- */

static const char *const SNAP[] = {"config/" V "/symbols.txt", "src/game/Alpha.cpp", "include/game/Alpha.h",
                                   "docs/exceptions.md", "config/" V "/config.yml"};

static const char *snapshot(const char *root) {
    AvpBuf b;
    avp_buf_init(&b, avp_arena());
    for (size_t i = 0; i < 5; i++) avp_buf_putz(&b, read_rel(root, SNAP[i]));
    return avp_buf_cz(&b);
}

AVP_TEST(member_dry_run_then_apply) {
    const char *root = make_checkout("rename-member", false);
    const char *before = snapshot(root);
    const char *olds[3] = {"fn_80001018", "fn_80001018__8unk_RampFf", "unk_Ramp::fn_80001018"};
    Run r = {0};
    for (int i = 0; i < 3; i++) {
        r = RUN(root, true, "rename", olds[i], "UpdateRamp");
        CHECK_EQ_I64(r.rc, 0);
        REQUIRE(r.j);
        const AvpJson *s = avp_json_at(avp_json_getz(r.j, "symbols"), 0);
        CHECK_EQ_U64(avp_json_len(avp_json_getz(r.j, "symbols")), 1);
        CHECK_STR(avp_json_get_str(s, "old", AVP_S("")), "fn_80001018__8unk_RampFf");
        CHECK_STR(avp_json_get_str(s, "new", AVP_S("")), "UpdateRamp__8unk_RampFf");
        CHECK_EQ_I64(avp_json_get_i64(s, "address", 0), 0x80001018);
        CHECK(!avp_json_get_bool(r.j, "applied", true));
    }
    CHECK(strcmp(snapshot(root), before) == 0);
    const AvpJson *ed = avp_json_getz(r.j, "edits");
    CHECK_EQ_U64(avp_json_len(ed), 5);
    for (size_t i = 0; i < 5; i++) CHECK(avp_json_getz(ed, SNAP[i]) != NULL);
    CHECK(any_contains(avp_json_getz(r.j, "notes"), "AGENTS.md", NULL));
    r = RUN(root, true, "rename", "fn_80001018", "UpdateRamp", "--apply");
    REQUIRE(r.j);
    CHECK(avp_json_get_bool(r.j, "applied", false));
    const char *syms = read_rel(root, "config/" V "/symbols.txt");
    CHECK(strstr(syms, "UpdateRamp__8unk_RampFf = ") != NULL);
    CHECK(strstr(syms, "fn_80001018__8unk_RampFf") == NULL);
    const char *src = read_rel(root, "src/game/Alpha.cpp");
    CHECK(strstr(src, "void unk_Ramp::UpdateRamp( float dt ) {") != NULL);
    CHECK(strstr(src, "ramp.UpdateRamp( 1.0f );") != NULL);
    const char *doc = read_rel(root, "docs/exceptions.md");
    CHECK(strstr(doc, "`UpdateRamp__8unk_RampFf`") != NULL);
    CHECK(strstr(doc, "`fn_80001018x`") != NULL);
    CHECK(strstr(read_rel(root, "config/" V "/config.yml"), "  - UpdateRamp__8unk_RampFf") != NULL);
    CHECK(strstr(read_rel(root, "AGENTS.md"), "fn_80001018__8unk_RampFf") != NULL);
}

AVP_TEST(plain_function_rename) {
    const char *root = make_checkout("rename-plain", false);
    Run r = RUN(root, true, "rename", "helper", "HelperCheck", "--apply");
    REQUIRE(r.j);
    CHECK_STR(avp_json_get_str(avp_json_at(avp_json_getz(r.j, "symbols"), 0), "new", AVP_S("")), "HelperCheck");
    CHECK(strstr(read_rel(root, "src/game/Alpha.cpp"), "    HelperCheck();") != NULL);
    CHECK(strstr(read_rel(root, "config/" V "/config.yml"), "  - HelperCheck") != NULL);
}

AVP_TEST(refusals) {
    const char *root = make_checkout("rename-refusals", false);
    const char *before = snapshot(root);
    Run r = RUN(root, true, "rename", "helper", "jumper", "--apply");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "already exists", NULL));
    r = RUN(root, true, "rename", "Tick", "Step");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "2 scopes", NULL));
    r = RUN(root, true, "rename", "unk_Ramp::Tick", "Step", "--apply");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "also names", "Tick__8unk_ZetaFv"));
    r = RUN(root, true, "rename", "fn_80001018", "Tick");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "overload", NULL));
    CHECK(strcmp(snapshot(root), before) == 0);
    CHECK_EQ_I64(RUN(root, false, "rename", "nosuch", "x").rc, 2);
    CHECK_EQ_I64(RUN(root, false, "rename", "__ct__8unk_RampFv", "x").rc, 0); /* a special member: symbols-only */
    CHECK_EQ_I64(RUN(root, false, "rename", "helper", "bad name").rc, 2);
}

AVP_TEST(placeholder_takes_a_signature) {
    const char *root = make_checkout("rename-signature", false);
    Run r = RUN(root, true, "rename", "fn_80001034", "fn_80001034__8unk_RampFv", "--apply");
    REQUIRE(r.j);
    CHECK_STR(avp_json_get_str(avp_json_at(avp_json_getz(r.j, "symbols"), 0), "new", AVP_S("")),
              "fn_80001034__8unk_RampFv");
    CHECK(strstr(read_rel(root, "config/" V "/symbols.txt"), "fn_80001034__8unk_RampFv = .text:0x80001034;") != NULL);
    CHECK_EQ_I64(RUN(root, true, "rename", "fn_80001018__8unk_RampFf", "fn_80001018__8unk_RampFv").rc, 2);
    r = RUN(root, true, "rename", "jumper", "Tick__8unk_RampFv");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "already exists", NULL));
}

AVP_TEST(local_static_rename) {
    const char *root = make_checkout("rename-local", false);
    Run r = RUN(root, true, "rename", "lbl_80004008", "displayHeightF");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "already exists", NULL));
    r = RUN(root, true, "rename", "lbl_80004008", "displayHeightF", "--local", "--apply");
    CHECK(avp_json_get_bool(r.j, "applied", false));
    CHECK(strstr(read_rel(root, "config/" V "/symbols.txt"),
                 "displayHeightF = .sbss:0x80004008; // type:object size:0x4 scope:local") != NULL);
    r = RUN(root, true, "rename", "fn_80001034", "jumper", "--local");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "already exists", NULL));
}

AVP_TEST(class_rename) {
    const char *root = make_checkout("rename-class", false);
    Run r = RUN(root, true, "rename", "--class", "unk_Ramp", "CRumbleRamp", "--apply");
    REQUIRE(r.j);
    static const char *const WANT[][2] = {{"fn_80001018__8unk_RampFf", "fn_80001018__11CRumbleRampFf"},
                                          {"Tick__8unk_RampFv", "Tick__11CRumbleRampFv"},
                                          {"__ct__8unk_RampFv", "__ct__11CRumbleRampFv"},
                                          {"fn_80001030__FR8unk_Ramp", "fn_80001030__FR11CRumbleRamp"},
                                          {"__vt__8unk_Ramp", "__vt__11CRumbleRamp"}};
    const AvpJson *syms = avp_json_getz(r.j, "symbols");
    CHECK_EQ_U64(avp_json_len(syms), 5);
    for (size_t i = 0; i < 5 && i < avp_json_len(syms); i++) {
        bool found = false;
        for (size_t k = 0; k < 5; k++)
            found |= avp_str_eqz(avp_json_get_str(avp_json_at(syms, i), "old", AVP_S("")), WANT[k][0]) &&
                     avp_str_eqz(avp_json_get_str(avp_json_at(syms, i), "new", AVP_S("")), WANT[k][1]);
        CHECK(found);
    }
    CHECK(any_contains(avp_json_getz(r.j, "notes"), "include/game/unk_Ramp.h", NULL));
    const char *src = read_rel(root, "src/game/Alpha.cpp");
    CHECK(strstr(src, "CRumbleRamp::CRumbleRamp() {") != NULL);
    CHECK(strstr(src, "void fn_80001030( CRumbleRamp& ramp ) {") != NULL);
    CHECK(strstr(read_rel(root, "docs/exceptions.md"), "`fn_80001018__11CRumbleRampFf`") != NULL);
    CHECK(strstr(read_rel(root, "config/" V "/symbols.txt"), "Tick__8unk_ZetaFv") != NULL);
    r = RUN(root, true, "rename", "--class", "CRumbleRamp", "unk_Zeta");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "already exists", NULL));
}

/* ---- SpecialRenameTest ---- */

AVP_TEST(destructor_target_rewrites_stub_and_header) {
    const char *root = make_checkout("special-dtor", true);
    Run r = RUN(root, true, "rename", "fn_80001040", "__dt__8unk_RampFv", "--apply");
    REQUIRE(r.j);
    CHECK(avp_json_get_bool(r.j, "applied", false));
    const char *cpp = read_rel(root, "src/game/Stub.cpp");
    CHECK(strstr(cpp, "unk_Ramp::~unk_Ramp() {") != NULL);
    CHECK(strstr(cpp, "extern") == NULL);
    CHECK(strstr(cpp, "fn_80001040") == NULL);
    CHECK(strstr(read_rel(root, "include/game/Stub.h"), "fn_80001040") == NULL);
    const char *alpha = read_rel(root, "include/game/Alpha.h");
    CHECK(strstr(alpha, "public:\n    ~unk_Ramp();") != NULL);
    CHECK(strstr(cpp, "extern \"C\" void ~") == NULL && strstr(alpha, "extern \"C\" void ~") == NULL);
    CHECK(strstr(read_rel(root, "config/" V "/symbols.txt"), "__dt__8unk_RampFv = .text:0x80001040;") != NULL);
}

AVP_TEST(constructor_target_with_parameters) {
    const char *root = make_checkout("special-ctor", true);
    w(root, "src/game/Stub.cpp",
      "/*\n====================\nfn_80001040\n====================\n*/\n// 0x80001040\n"
      "extern \"C\" void fn_80001040( float dt ) {\n    // TODO\n}\n");
    w(root, "include/game/Stub.h",
      "#ifdef __cplusplus\nextern \"C\" {\n#endif\nvoid fn_80001040( float dt );\n#ifdef __cplusplus\n}\n#endif\n");
    Run r = RUN(root, true, "rename", "fn_80001040", "__ct__8unk_RampFf", "--apply");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(strstr(read_rel(root, "src/game/Stub.cpp"), "unk_Ramp::unk_Ramp( float dt ) {") != NULL);
    CHECK(strstr(read_rel(root, "include/game/Alpha.h"), "    unk_Ramp( float dt );") != NULL);
}

AVP_TEST(special_target_refused_when_a_use_cannot_be_rewritten) {
    const char *root = make_checkout("special-use", true);
    append(root, "src/game/Stub.cpp", "void caller() {\n    fn_80001040();\n}\n");
    const char *before = read_rel(root, "src/game/Stub.cpp");
    Run r = RUN(root, true, "rename", "fn_80001040", "__dt__8unk_RampFv", "--apply");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "Stub.cpp:", "--symbols-only"));
    CHECK(strcmp(read_rel(root, "src/game/Stub.cpp"), before) == 0);
    CHECK(strstr(read_rel(root, "config/" V "/symbols.txt"), "__dt__8unk_RampFv") == NULL);
}

AVP_TEST(operator_target_falls_back_to_symbols_only) {
    const char *root = make_checkout("special-op", true);
    const char *stub = read_rel(root, "src/game/Stub.cpp");
    Run r = RUN(root, true, "rename", "fn_80001040", "__nw__FUl", "--apply");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(any_contains(avp_json_getz(r.j, "notes"), "source was not touched", "special member"));
    CHECK(strcmp(read_rel(root, "src/game/Stub.cpp"), stub) == 0);
}

AVP_TEST(vtable_target_falls_back_to_symbols_only_and_lists_the_source_sites) {
    const char *root = make_checkout("special-vt", true);
    const char *stub = read_rel(root, "src/game/Stub.cpp");
    Run r = RUN(root, true, "rename", "fn_80001040", "__vt__8unk_Zeta", "--apply");
    CHECK_EQ_I64(r.rc, 0);
    REQUIRE(r.j);
    CHECK(avp_json_get_bool(r.j, "applied", false));
    const AvpJson *ed = avp_json_getz(r.j, "edits");
    CHECK(avp_json_len(ed) == 1 && avp_json_getz(ed, "config/" V "/symbols.txt"));
    CHECK(strstr(read_rel(root, "config/" V "/symbols.txt"), "__vt__8unk_Zeta = .text:0x80001040;") != NULL);
    CHECK(strcmp(read_rel(root, "src/game/Stub.cpp"), stub) == 0); /* the mangled form never enters source */
    CHECK(any_contains(avp_json_getz(r.j, "notes"), "source was not touched", NULL));
    CHECK(any_contains(avp_json_getz(r.j, "notes"), "source not edited", "src/game/Stub.cpp:"));
}

AVP_TEST(template_and_destructor_names_fall_back_to_symbols_only) {
    const char *root = make_checkout("special-tpl", true);
    Run r = RUN(root, true, "rename", "fn_80001040", "push<int>", "--apply");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(any_contains(avp_json_getz(r.j, "notes"), "source was not touched", "not an identifier"));
    CHECK(strstr(read_rel(root, "config/" V "/symbols.txt"), "push<int> = .text:0x80001040;") != NULL);
    /* an old name that is itself a special member: nothing to do with source either */
    r = RUN(root, true, "rename", "__vt__8unk_Ramp", "lbl_80002100");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(any_contains(avp_json_getz(r.j, "notes"), "source was not touched", "special member"));
}

AVP_TEST(generated_files_are_not_rename_targets) {
    const char *root = make_checkout("rename-generated", false);
    w(root, "docs/progress.md", "| helper | exact |\n");
    w(root, "build/tmp/stale.cpp", "void x() { helper(); }\n");
    Run r = RUN(root, true, "rename", "helper", "HelperCheck", "--apply");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(strcmp(read_rel(root, "docs/progress.md"), "| helper | exact |\n") == 0);
    CHECK(strcmp(read_rel(root, "build/tmp/stale.cpp"), "void x() { helper(); }\n") == 0);
    CHECK(any_contains(avp_json_getz(r.j, "notes"), "docs/progress.md mentions", "generated ledger"));
    CHECK(avp_json_getz(avp_json_getz(r.j, "edits"), "docs/progress.md") == NULL);
    CHECK(strstr(read_rel(root, "src/game/Alpha.cpp"), "    HelperCheck();") != NULL);
}

AVP_TEST(generated_names_edit_symbols_only) {
    const char *root = make_checkout("special-gen", true);
    Run r = RUN(root, true, "rename", "__sinit_\\Alpha_cpp", "__sinit_\\Beta_cpp", "--apply");
    REQUIRE(r.j);
    CHECK_STR(avp_json_get_str(avp_json_at(avp_json_getz(r.j, "symbols"), 0), "new", AVP_S("")),
              "__sinit_\\Beta_cpp");
    const AvpJson *ed = avp_json_getz(r.j, "edits");
    CHECK(avp_json_len(ed) == 1 && avp_json_getz(ed, "config/" V "/symbols.txt"));
    CHECK(any_contains(avp_json_getz(r.j, "notes"), "symbols.txt only", NULL));
    r = RUN(root, true, "rename", "@36@Tick__8unk_RampFv", "@36@Step__8unk_RampFv", "--apply");
    ed = avp_json_getz(r.j, "edits");
    CHECK(avp_json_len(ed) == 1 && avp_json_getz(ed, "config/" V "/symbols.txt"));
    r = RUN(root, true, "rename", "@stringBase0", "@stringBase1");
    CHECK_STR(avp_json_get_str(avp_json_at(avp_json_getz(r.j, "symbols"), 0), "new", AVP_S("")), "@stringBase1");
    CHECK(strstr(read_rel(root, "config/" V "/symbols.txt"), "@36@Step__8unk_RampFv = .text:0x80001048;") != NULL);
    CHECK_EQ_I64(RUN(root, false, "rename", "fn_80001040", "@36@x").rc, 2);
}

AVP_TEST(symbols_only_flag) {
    const char *root = make_checkout("special-symonly", true);
    const char *snap = read_rel(root, "src/game/Stub.cpp");
    Run r = RUN(root, true, "rename", "fn_80001040", "Weird::Name", "--symbols-only", "--apply");
    const AvpJson *ed = avp_json_getz(r.j, "edits");
    CHECK(avp_json_len(ed) == 1 && avp_json_getz(ed, "config/" V "/symbols.txt"));
    CHECK(strcmp(read_rel(root, "src/game/Stub.cpp"), snap) == 0);
    CHECK(strstr(read_rel(root, "config/" V "/symbols.txt"), "Weird::Name = .text:0x80001040") != NULL);
}

/* ---- statics, families and signatures ---- */

static const char STATIC_SYMBOLS[] =
    "Counter = .sdata:0x80005000; // type:object size:0x4 scope:local data:4byte\n"
    "Twin = .sdata:0x80005004; // type:object size:0x4 scope:local data:4byte\n"
    "lbl_80005008 = .sdata:0x80005008; // type:object size:0x4 scope:global data:4byte\n"
    "Other = .sdata:0x8000500C; // type:object size:0x4 scope:local data:4byte\n";

static const char *make_statics(const char *name) {
    const char *root = make_checkout(name, false);
    append(root, "config/" V "/symbols.txt", STATIC_SYMBOLS);
    append(root, "config/" V "/splits.txt",
           "\ngame/Gamma.c:\n\t.sdata       start:0x80005000 end:0x80005008\n"
           "\ngame/Delta.c:\n\t.sdata       start:0x80005008 end:0x80005010\n");
    return root;
}

static const char *last_line(AvpStr out) {
    AvpArena *a = avp_arena();
    while (out.n && (out.p[out.n - 1] == '\n' || out.p[out.n - 1] == '\r')) out.n--;
    size_t i = out.n;
    while (i && out.p[i - 1] != '\n') i--;
    return avp_cz(a, avp_strn(out.p + i, out.n - i));
}

AVP_TEST(collisions_are_exact_names_or_two_locals_in_a_unit) {
    const char *root = make_statics("collide");
    /* a global renamed to a name that only locals of other units carry: --local is needed, and the message says so */
    Run r = RUN(root, true, "rename", "lbl_80005008", "Counter");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "--local", "local-needs-flag"));
    r = RUN(root, true, "rename", "lbl_80005008", "Counter", "--local");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(any_contains(avp_json_getz(r.j, "notes"), "other units", "game/Gamma"));
    /* a local renamed to the name of a local of another unit goes through without --local */
    r = RUN(root, true, "rename", "Other", "Counter", "--apply");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(strstr(read_rel(root, "config/" V "/symbols.txt"),
                 "Counter = .sdata:0x8000500C; // type:object size:0x4 scope:local") != NULL);
    /* two locals of one name in one unit's ranges */
    r = RUN(root, true, "rename", "Twin", "Counter");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "unit game/Gamma", "local-in-unit"));
    /* the exact name present with non-local scope */
    r = RUN(root, true, "rename", "Twin", "jumper");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "already exists", "exact-name"));
}

AVP_TEST(a_name_in_another_class_is_not_a_collision) {
    const char *root = make_checkout("collide-class", false);
    /* Tick exists in unk_Ramp and unk_Zeta; a free function may take the name */
    Run r = RUN(root, true, "rename", "fn_80001030__FR8unk_Ramp", "Tick");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(any_contains(avp_json_getz(r.j, "notes"), "other scopes", NULL));
}

static const char FAMILY_SYMBOLS[] =
    "fn_80001060__8unk_RampFv = .text:0x80001060; // type:function size:0x4 scope:global\n"
    "fn_80001060__8unk_ZetaFv = .text:0x80001064; // type:function size:0x4 scope:global\n"
    "fn_80001070__8unk_RampFv = .text:0x80001070; // type:function size:0x4 scope:global\n"
    "fn_80001070__8unk_ZetaFi = .text:0x80001074; // type:function size:0x4 scope:global\n";

static const char *make_family(const char *name) {
    const char *root = make_checkout(name, false);
    append(root, "config/" V "/symbols.txt", FAMILY_SYMBOLS);
    w(root, "include/game/Alpha.h",
      "class unk_Ramp {\npublic:\n    unk_Ramp();\n    void fn_80001018( float dt );\n    void Tick();\n"
      "    virtual void fn_80001060();\n    virtual void fn_80001070();\n};\n");
    w(root, "include/game/Zeta.h",
      "class unk_Zeta : public unk_Ramp {\npublic:\n    virtual void fn_80001060();  // override\n"
      "    virtual void fn_80001070( int x );  // override\n};\n"
      "class unk_Other {\npublic:\n    void fn_80001060( int x );\n};\n");
    w(root, "src/game/Zeta.cpp",
      "void unk_Zeta::fn_80001060() {\n    unk_Ramp::fn_80001060();\n}\n\nvoid unk_Other::fn_80001060( int x ) {\n}\n");
    return root;
}

AVP_TEST(signature_change_is_opt_in_and_lists_the_sites_to_edit) {
    const char *root = make_family("sig-member");
    const char *before = snapshot(root);
    Run r = RUN(root, false, "rename", "fn_80001018__8unk_RampFf", "fn_80001018__8unk_RampFv");
    CHECK_EQ_I64(r.rc, 2); /* unchanged: a rename keeps the signature */
    CHECK(strstr(avp_cz(avp_arena(), r.err), "--signature") != NULL);
    r = RUN(root, true, "rename", "fn_80001018__8unk_RampFf", "fn_80001018__8unk_RampFv", "--signature");
    CHECK_EQ_I64(r.rc, 0);
    REQUIRE(r.j);
    CHECK(!avp_json_get_bool(r.j, "applied", true));
    CHECK_EQ_U64(avp_json_len(avp_json_getz(r.j, "symbols")), 1);
    CHECK(any_contains(avp_json_getz(r.j, "signature_changes"), "unk_Ramp::fn_80001018(float)",
                       "unk_Ramp::fn_80001018()"));
    const AvpJson *hs = avp_json_getz(r.j, "to_edit_by_hand");
    CHECK_EQ_U64(avp_json_len(hs), 2); /* the definition and the declaration, not the call */
    CHECK(any_contains(hs, "src/game/Alpha.cpp:9", "void unk_Ramp::fn_80001018( float dt ) {"));
    CHECK(any_contains(hs, "include/game/Alpha.h:4", "void fn_80001018( float dt );"));
    CHECK(avp_json_getz(avp_json_getz(r.j, "edits"), "src/game/Alpha.cpp") == NULL); /* the plain name did not change */
    CHECK(strcmp(snapshot(root), before) == 0);
    r = RUN(root, false, "rename", "fn_80001018__8unk_RampFf", "fn_80001018__8unk_RampFv", "--signature");
    CHECK(strstr(last_line(r.out), "DRY RUN: NOTHING WAS WRITTEN. Re-run with --apply") != NULL);
    r = RUN(root, true, "rename", "fn_80001018__8unk_RampFf", "fn_80001018__8unk_RampFv", "--signature", "--apply");
    CHECK(avp_json_get_bool(r.j, "applied", false));
    CHECK(strstr(read_rel(root, "config/" V "/symbols.txt"), "fn_80001018__8unk_RampFv = ") != NULL);
    CHECK(strcmp(read_rel(root, "src/game/Alpha.cpp"), ALPHA_CPP) == 0);
}

AVP_TEST(signature_change_to_another_class_and_the_refusals) {
    const char *root = make_family("sig-class");
    Run r = RUN(root, true, "rename", "fn_80001030__FR8unk_Ramp", "fn_80001030__FR8unk_Zeta", "--signature");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(any_contains(avp_json_getz(r.j, "to_edit_by_hand"), "src/game/Alpha.cpp:20",
                       "void fn_80001030( unk_Ramp& ramp )"));
    /* the new name must demangle */
    CHECK_EQ_I64(RUN(root, false, "rename", "fn_80001034", "Tick", "--signature").rc, 2);
    CHECK_EQ_I64(RUN(root, false, "rename", "fn_80001034", "Tick__8unk_Zeta", "--signature").rc, 2);
    /* the new mangled name must not exist */
    r = RUN(root, true, "rename", "fn_80001030__FR8unk_Ramp", "Tick__8unk_RampFv", "--signature");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "already exists", "exact-name"));
    /* one address, two names for one function */
    append(root, "config/" V "/symbols.txt",
           "fn_80001080__Fi = .text:0x80001080; // type:function size:0x4 scope:global\n"
           "fn_80001080__Ff = .text:0x80001080; // type:function size:0x4 scope:global\n");
    r = RUN(root, true, "rename", "fn_80001080__Fi", "fn_80001080__Fl", "--signature");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "two names", "two-names"));
    CHECK_EQ_I64(RUN(root, false, "rename", "fn_80001034", "fn_80001034__Fv", "--signature", "--symbols-only").rc, 2);
}

AVP_TEST(family_changes_every_override_and_refuses_another_slot) {
    const char *root = make_family("family");
    Run r = RUN(root, true, "rename", "fn_80001060__8unk_RampFv", "fn_80001060__8unk_RampFi", "--signature",
                "--family");
    CHECK_EQ_I64(r.rc, 0);
    REQUIRE(r.j);
    const AvpJson *syms = avp_json_getz(r.j, "symbols");
    CHECK_EQ_U64(avp_json_len(syms), 2);
    CHECK_STR(avp_json_get_str(avp_json_at(syms, 1), "new", AVP_S("")), "fn_80001060__8unk_ZetaFi");
    const AvpJson *hs = avp_json_getz(r.j, "to_edit_by_hand");
    CHECK(any_contains(hs, "include/game/Alpha.h:6", "virtual void fn_80001060();"));
    CHECK(any_contains(hs, "include/game/Zeta.h:3", NULL));
    CHECK(any_contains(hs, "src/game/Zeta.cpp:1", NULL));
    CHECK(!any_contains(hs, "Zeta.h:8", NULL));   /* unk_Other::fn_80001060( int ): another class */
    CHECK(!any_contains(hs, "Zeta.cpp:5", NULL));
    CHECK(!any_contains(hs, "Zeta.cpp:2", NULL)); /* a call */
    /* a member with other codes is not the same slot: refused, naming it */
    r = RUN(root, true, "rename", "fn_80001070__8unk_RampFv", "fn_80001070__8unk_RampFl", "--signature", "--family");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "fn_80001070__8unk_ZetaFi", "family-codes"));
    CHECK(strstr(read_rel(root, "config/" V "/symbols.txt"), "fn_80001070__8unk_RampFv") != NULL);
    /* the class is kept per member: the new name may not name another class */
    CHECK_EQ_I64(RUN(root, false, "rename", "fn_80001060__8unk_RampFv", "fn_80001060__8unk_ZetaFi", "--signature",
                     "--family").rc, 2);
}

AVP_TEST(plain_rename_of_a_family_needs_family) {
    const char *root = make_family("family-plain");
    Run r = RUN(root, true, "rename", "fn_80001060__8unk_RampFv", "Step");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "also names", "--family"));
    r = RUN(root, true, "rename", "fn_80001060__8unk_RampFv", "Step", "--family", "--apply");
    CHECK_EQ_I64(r.rc, 0);
    CHECK_EQ_U64(avp_json_len(avp_json_getz(r.j, "symbols")), 2);
    CHECK(strstr(read_rel(root, "config/" V "/symbols.txt"), "Step__8unk_ZetaFv = ") != NULL);
    CHECK(strstr(read_rel(root, "include/game/Zeta.h"), "virtual void Step();  // override") != NULL);
    CHECK_EQ_I64(RUN(root, false, "rename", "fn_80001070__8unk_RampFv", "Step__8unk_RampFv", "--family").rc, 2);
}

AVP_TEST(the_last_line_says_what_was_written) {
    const char *root = make_checkout("last-line", false);
    Run r = RUN(root, false, "rename", "helper", "HelperCheck");
    CHECK(strstr(last_line(r.out), "DRY RUN: NOTHING WAS WRITTEN. Re-run with --apply") != NULL);
    /* the output cap cuts the listing, never that line */
    r = RUN(root, false, "rename", "helper", "HelperCheck", "--limit", "4");
    CHECK(strstr(last_line(r.out), "DRY RUN: NOTHING WAS WRITTEN") != NULL);
    CHECK(strstr(avp_cz(avp_arena(), r.out), "more lines") != NULL);
    r = RUN(root, false, "rename", "helper", "jumper");
    CHECK(strstr(last_line(r.out), "REFUSED: 1 conflict(s); NOTHING WAS WRITTEN") != NULL);
    r = RUN(root, false, "rename", "helper", "HelperCheck", "--apply");
    CHECK(strstr(last_line(r.out), "APPLIED: wrote") != NULL);
}

AVP_TEST(placeholder_mentions_in_comments_are_listed_not_rewritten) {
    const char *root = make_checkout("comment-mentions", false);
    append(root, "src/game/Alpha.cpp",
           "\n// the table lbl_80002008 is filled at startup\n/* block\n   lbl_80002008 */\nint x = lbl_80002008;\n");
    Run r = RUN(root, true, "rename", "lbl_80002008", "__vt__8unk_Zeta", "--apply");
    CHECK_EQ_I64(r.rc, 0);
    const AvpJson *n = avp_json_getz(r.j, "notes");
    CHECK(any_contains(n, "mentions in comments, not rewritten", "src/game/Alpha.cpp:24"));
    CHECK(any_contains(n, "mentions in comments, not rewritten", "src/game/Alpha.cpp:26"));
    CHECK(any_contains(n, "source not edited", "src/game/Alpha.cpp:27"));
    CHECK(!any_contains(n, "mentions in comments", "src/game/Alpha.cpp:27"));
    CHECK(strstr(read_rel(root, "src/game/Alpha.cpp"), "lbl_80002008 */") != NULL);
}

/* ---- NamesRealTreeTest (dry runs only) ---- */

AVP_TEST(real_tree_find_and_dry_run) {
    const char *root = avp_root();
    if (!avp_isfile(avp_join(avp_arena(), root, "config", "R5WEA4", "symbols.txt"))) return;
    Run r = RUN(root, true, "find", "unk_Ramp");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(r.j && avp_json_len(avp_json_getz(r.j, "symbols")) > 0);
    r = RUN(root, true, "rename", "fn_800067CC", "UpdateRamp");
    CHECK_EQ_I64(r.rc, 0);
    REQUIRE(r.j);
    CHECK(!avp_json_get_bool(r.j, "applied", true));
    CHECK_STR(avp_json_get_str(avp_json_at(avp_json_getz(r.j, "symbols"), 0), "new", AVP_S("")),
              "UpdateRamp__8unk_RampFf");
}

/* ---- helpers ---- */

AVP_TEST(split_params_and_clip) {
    AvpArena *a = avp_arena();
    AvpStrVec v = names_split_params(a, AVP_S("int, Foo<A, B>, void (*)(int, char), "));
    REQUIRE(v.n == 3);
    CHECK_STR(v.v[0], "int");
    CHECK_STR(v.v[1], " Foo<A, B>");
    CHECK_STR(v.v[2], " void (*)(int, char)");
    v = names_split_params(a, AVP_S("int,,float"));
    CHECK_EQ_U64(v.n, 3);
    CHECK_STR(names_clip(a, AVP_S("  abc  "), 150), "abc");
    CHECK_STR(names_clip(a, AVP_S("abcdefgh"), 6), "abc...");
    CHECK_STR(names_clip(a, AVP_S("\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9"), 6),
              "\xc3\xa9\xc3\xa9\xc3\xa9...");
}

AVP_TEST(def_name_forms) {
    AvpArena *a = avp_arena();
    CHECK_CZ(names_def_name(a, AVP_S("void A::f( int x ) {")), "A::f");
    CHECK_CZ(names_def_name(a, AVP_S("GLOBAL(void)")), "");
    CHECK(names_def_name(a, AVP_S("    x();")) == NULL);
    CHECK(names_def_name(a, AVP_S("if (x) {")) == NULL);
    CHECK(names_def_name(a, AVP_S("int x = f(1)")) == NULL);
    CHECK(names_def_name(a, AVP_S("bool A::operator==( const A& o ) const {")) == NULL); /* "=" in the head */
    CHECK_CZ(names_def_name(a, AVP_S("A& A::operator+( int ) {")), "A::operator+");
    CHECK_CZ(names_def_name(a, AVP_S("unk_Ramp::~unk_Ramp() {")), "unk_Ramp::~unk_Ramp");
}

AVP_TEST(plain_of_forms) {
    AvpArena *a = avp_arena();
    NamesPlain p = names_plain_of(a, "fn_800067CC__8unk_RampFf");
    CHECK_CZ(p.plain, "fn_800067CC");
    CHECK_EQ_U64(p.nscope, 1);
    CHECK_CZ(p.scope[0], "unk_Ramp");
    p = names_plain_of(a, "__ct__8unk_RampFv");
    CHECK_CZ(p.plain, "unk_Ramp");
    CHECK_CZ(p.raw, "__ct");
    p = names_plain_of(a, "memcpy");
    CHECK_CZ(p.plain, "memcpy");
    CHECK_EQ_U64(p.nscope, 0);
    p = names_plain_of(a, "@36@Tick__8unk_RampFv");
    CHECK_CZ(p.plain, "@36@Tick__8unk_RampFv");
}

AVP_TEST(planner_api) {
    const char *root = make_checkout("planner-api", false);
    ShsmRepo *repo = shsm_repo_new(root, NULL);
    NamesCtx *cx = names_ctx_new(repo, NULL);
    NamesPlan *p = names_plan_rename(cx, "helper", "HelperCheck", false, false);
    names_compute_edits(p);
    CHECK_EQ_U64(p->conflicts.n, 0);
    REQUIRE(p->symbols.n == 1);
    CHECK_CZ(p->symbols.v[0].new_name, "HelperCheck");
    NamesFile *f = names_plan_file(p, "src/game/Alpha.cpp");
    REQUIRE(f && f->edits.n == 1);
    CHECK_STR(f->edits.v[0].new_, "    HelperCheck();");
    names_write_plan(p);
    CHECK(strstr(read_rel(root, "src/game/Alpha.cpp"), "    HelperCheck();") != NULL);
    AvpTry t;
    int code = 0;
    if (AVP_TRY(&t)) {
        names_plan_rename(names_ctx_new(repo, NULL), "nosuch", "x", false, false);
        avp_try_end(&t);
    } else {
        code = t.code;
    }
    CHECK_EQ_I64(code, SHSM_EXC_USAGE);
}

/* ---- attr ---- */

static const char ATTR_SYMBOLS[] =
    "bigobj = .sdata:0x80006000; // type:object size:0x20 scope:global data:4byte\n"
    "inner = .sdata:0x80006010; // type:object size:0x4 scope:global data:4byte\n"
    "Dup = .sdata:0x80006100; // type:object size:0x4 scope:local data:4byte\n"
    "Dup = .sdata:0x80006104; // type:object size:0x4 scope:global data:4byte\n"
    "Solo = .sdata:0x80006108; // type:object size:0x4 data:4byte\n";

static const char *make_attr(const char *name) {
    const char *root = make_checkout(name, false);
    append(root, "config/" V "/symbols.txt", ATTR_SYMBOLS);
    append(root, "config/" V "/splits.txt",
           "\ngame/Eps.c:\n\t.sdata       start:0x80006000 end:0x8000610C\n");
    return root;
}

static AvpStr status_of(const Run *r) { return avp_json_get_str(r->j, "status", AVP_S("")); }

AVP_TEST(attr_scope_replaces_or_inserts_and_dry_run_writes_nothing) {
    const char *root = make_attr("attr-scope");
    const char *before = read_rel(root, "config/" V "/symbols.txt");
    Run r = RUN(root, true, "attr", "fn_80001000__Fv", "--scope", "weak");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(avp_str_startswith(status_of(&r), AVP_S("DRY RUN: NOTHING WAS WRITTEN")));
    CHECK(strcmp(read_rel(root, "config/" V "/symbols.txt"), before) == 0);
    /* an existing scope token is replaced in place */
    r = RUN(root, true, "attr", "fn_80001000__Fv", "--scope", "weak", "--apply");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(avp_str_startswith(status_of(&r), AVP_S("APPLIED")));
    CHECK(strstr(read_rel(root, "config/" V "/symbols.txt"),
                 "fn_80001000__Fv = .text:0x80001000; // type:function size:0x10 scope:weak\n") != NULL);
    /* no scope token: inserted after size, before the data: token */
    r = RUN(root, true, "attr", "Solo", "--scope", "local", "--apply");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(strstr(read_rel(root, "config/" V "/symbols.txt"),
                 "Solo = .sdata:0x80006108; // type:object size:0x4 scope:local data:4byte\n") != NULL);
    /* the same attribute again changes nothing */
    r = RUN(root, true, "attr", "Solo", "--scope", "local", "--apply");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(avp_str_startswith(status_of(&r), AVP_S("NO CHANGE")));
    r = RUN(root, false, "attr", "fn_80001000__Fv", "--scope", "weak");
    CHECK(strstr(last_line(r.out), "NO CHANGE") != NULL);
}

AVP_TEST(attr_align_goes_after_scope) {
    const char *root = make_attr("attr-align");
    Run r = RUN(root, true, "attr", "inner", "--align", "8", "--apply");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(strstr(read_rel(root, "config/" V "/symbols.txt"),
                 "inner = .sdata:0x80006010; // type:object size:0x4 scope:global align:8 data:4byte\n") != NULL);
    r = RUN(root, true, "attr", "inner", "--align", "16", "--apply");
    CHECK(strstr(read_rel(root, "config/" V "/symbols.txt"), "scope:global align:16 data:4byte\n") != NULL);
    CHECK_EQ_I64(RUN(root, false, "attr", "inner", "--align", "6").rc, 2);
    CHECK_EQ_I64(RUN(root, false, "attr", "inner").rc, 2); /* nothing to change */
    CHECK_EQ_I64(RUN(root, false, "attr", "nosuch", "--scope", "weak").rc, 2);
}

AVP_TEST(attr_never_edits_splits_without_resplit) {
    const char *root = make_attr("attr-no-resplit");
    const char *syms = read_rel(root, "config/" V "/symbols.txt");
    const char *spl = read_rel(root, "config/" V "/splits.txt");
    /* shrinking the last symbol of a section would leave the range end past it: refused, with the needed end */
    Run r = RUN(root, true, "attr", "lbl_80002008", "--size", "0x8", "--apply");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(avp_str_startswith(status_of(&r), AVP_S("REFUSED")));
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "would need end 0x80002010", "use unit.exe resplit, or pass --resplit"));
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "game/Alpha", "section-end-moves"));
    /* growing it past the range end is refused the same way */
    r = RUN(root, true, "attr", "lbl_80002008", "--size", "0x10", "--apply");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "would need end 0x80002018", "--resplit"));
    CHECK(strcmp(read_rel(root, "config/" V "/symbols.txt"), syms) == 0);
    CHECK(strcmp(read_rel(root, "config/" V "/splits.txt"), spl) == 0);
}

AVP_TEST(attr_resplit_moves_the_section_end) {
    const char *root = make_attr("attr-shrink");
    /* the dry run lists the splits.txt change */
    Run r = RUN(root, true, "attr", "lbl_80002008", "--size", "0x8", "--resplit");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(avp_str_startswith(status_of(&r), AVP_S("DRY RUN")));
    CHECK(any_contains(avp_json_getz(r.j, "notes"), "--resplit", "0x80002010"));
    r = RUN(root, true, "attr", "lbl_80002008", "--size", "0x8", "--resplit", "--apply");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(avp_str_startswith(status_of(&r), AVP_S("APPLIED: wrote 2 edit(s) in 2 file(s)")));
    CHECK(strstr(read_rel(root, "config/" V "/symbols.txt"),
                 "lbl_80002008 = .data:0x80002008; // type:object size:0x8 scope:global\n") != NULL);
    const char *splits = read_rel(root, "config/" V "/splits.txt");
    CHECK(strstr(splits, "\t.data       start:0x80002000 end:0x80002010\n") != NULL);
    CHECK(strstr(splits, "\t.text       start:0x80001000 end:0x80001024\n") != NULL); /* the other ranges stay */
    CHECK(any_contains(avp_json_getz(r.j, "notes"), "the .data range of unit game/Alpha", "0x80002010"));
    /* a shrink that leaves the range end alone (not the last symbol): symbols.txt only */
    r = RUN(root, true, "attr", "lbl_80002000", "--size", "4", "--apply");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(avp_str_startswith(status_of(&r), AVP_S("APPLIED: wrote 1 edit(s) in 1 file(s)")));
}

AVP_TEST(attr_size_refusals) {
    const char *root = make_attr("attr-size-refuse");
    const char *syms = read_rel(root, "config/" V "/symbols.txt");
    const char *spl = read_rel(root, "config/" V "/splits.txt");
    /* into the next symbol */
    Run r = RUN(root, true, "attr", "lbl_80002000", "--size", "0xA", "--apply");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "run into lbl_80002008", "size-grows-into-next"));
    CHECK(avp_str_startswith(status_of(&r), AVP_S("REFUSED")));
    /* past the unit's range */
    r = RUN(root, true, "attr", "lbl_80002008", "--size", "0x10", "--apply");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "past the end", "size-past-unit-end"));
    /* a freed range that holds another symbol */
    r = RUN(root, true, "attr", "bigobj", "--size", "8", "--apply");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "holds inner", "freed-range-not-empty"));
    CHECK_EQ_I64(RUN(root, false, "attr", "bigobj", "--size", "0").rc, 2);
    CHECK(strcmp(read_rel(root, "config/" V "/symbols.txt"), syms) == 0);
    CHECK(strcmp(read_rel(root, "config/" V "/splits.txt"), spl) == 0);
    /* shrinking past the last inner symbol is fine, and bigobj is the last symbol of nothing: no splits edit */
    r = RUN(root, true, "attr", "bigobj", "--size", "0x14", "--apply");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(strcmp(read_rel(root, "config/" V "/splits.txt"), spl) == 0);
}

AVP_TEST(attr_names_shared_by_several_symbols_need_an_address) {
    const char *root = make_attr("attr-ambiguous");
    Run r = RUN(root, true, "attr", "Dup", "--scope", "weak", "--apply");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "2 symbols are named Dup", "ambiguous-name"));
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "0x80006104 in game/Eps", NULL));
    /* the global Dup may not become a second local of the unit */
    r = RUN(root, true, "attr", "0x80006104", "--scope", "local", "--apply");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "two scope:local symbols named Dup", "local-in-unit"));
    /* by address it is addressable and may become weak */
    r = RUN(root, true, "attr", "0x80006104", "--scope", "weak", "--apply");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(strstr(read_rel(root, "config/" V "/symbols.txt"),
                 "Dup = .sdata:0x80006104; // type:object size:0x4 scope:weak data:4byte\n") != NULL);
}

/* ---- add ---- */

AVP_TEST(add_inserts_the_line_in_address_order_in_the_neighbours_form) {
    const char *root = make_attr("add-insert");
    const char *before = read_rel(root, "config/" V "/symbols.txt");
    Run r = RUN(root, true, "add", "Gap", "--section", ".sdata", "--address", "0x80006020", "--size", "0x10", "--data",
                "4byte");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(avp_str_startswith(status_of(&r), AVP_S("DRY RUN: NOTHING WAS WRITTEN")));
    CHECK(strcmp(read_rel(root, "config/" V "/symbols.txt"), before) == 0);
    r = RUN(root, true, "add", "Gap", "--section", ".sdata", "--address", "0x80006020", "--size", "0x10", "--data",
            "4byte", "--apply");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(avp_str_startswith(status_of(&r), AVP_S("APPLIED: wrote 1 edit(s) in 1 file(s)")));
    /* after `inner` (0x80006010), before `Dup` (0x80006100) */
    CHECK(strstr(read_rel(root, "config/" V "/symbols.txt"),
                 "inner = .sdata:0x80006010; // type:object size:0x4 scope:global data:4byte\n"
                 "Gap = .sdata:0x80006020; // type:object size:0x10 data:4byte\n"
                 "Dup = .sdata:0x80006100;") != NULL);
    /* the new name is now taken, and so is its address */
    CHECK_EQ_I64(RUN(root, false, "add", "Gap", "--section", ".sdata", "--address", "0x80006040", "--size", "0x4").rc, 1);
    CHECK_EQ_I64(RUN(root, false, "add", "Other", "--section", ".sdata", "--address", "0x80006020", "--size", "0x4").rc, 1);
}

AVP_TEST(add_takes_scope_align_and_type) {
    const char *root = make_attr("add-attrs");
    Run r = RUN(root, true, "add", "gGlobal", "--section", ".sdata", "--address", "0x80006030", "--size", "0x8",
                "--type", "object", "--scope", "weak", "--align", "8", "--apply");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(strstr(read_rel(root, "config/" V "/symbols.txt"),
                 "gGlobal = .sdata:0x80006030; // type:object size:0x8 scope:weak align:8\n") != NULL);
}

AVP_TEST(add_refusals) {
    const char *root = make_attr("add-refuse");
    const char *syms = read_rel(root, "config/" V "/symbols.txt");
    /* the address is already named */
    Run r = RUN(root, true, "add", "X", "--section", ".sdata", "--address", "0x80006100", "--size", "0x4", "--apply");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "already named Dup", "address-named"));
    CHECK(avp_str_startswith(status_of(&r), AVP_S("REFUSED")));
    /* inside a neighbour: the suggestion shrinks it with attr */
    r = RUN(root, true, "add", "X", "--section", ".sdata", "--address", "0x80006008", "--size", "0x4", "--apply");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "inside bigobj", "names.exe attr bigobj --size 0x8"));
    /* the range runs into the next symbol */
    r = RUN(root, true, "add", "X", "--section", ".sdata", "--address", "0x800060F8", "--size", "0x10", "--apply");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "runs into Dup", "overlaps-neighbour"));
    /* outside the section's ranges, and in another section than the range's */
    r = RUN(root, true, "add", "X", "--section", ".sdata", "--address", "0x80007000", "--size", "0x4", "--apply");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "outside the .sdata ranges", "outside-section"));
    r = RUN(root, true, "add", "X", "--section", ".bss", "--address", "0x80006020", "--size", "0x4", "--apply");
    CHECK_EQ_I64(r.rc, 1);
    /* a name that exists */
    r = RUN(root, true, "add", "inner", "--section", ".sdata", "--address", "0x80006020", "--size", "0x4", "--apply");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(any_contains(avp_json_getz(r.j, "conflicts"), "already exists", "name-exists"));
    /* bad values are usage errors */
    CHECK_EQ_I64(RUN(root, false, "add", "X", "--section", ".sdata", "--address", "6020", "--size", "0x4").rc, 2);
    CHECK_EQ_I64(RUN(root, false, "add", "X", "--section", ".sdata", "--address", "0x80006020", "--size", "0").rc, 2);
    CHECK(strcmp(read_rel(root, "config/" V "/symbols.txt"), syms) == 0);
}

AVP_TEST(elf_reader) {
    AvpArena *a = avp_arena();
    const char *root = make_checkout("elf", false);
    NamesElfFuncs fns = {0};
    REQUIRE(names_elf_functions(a, avp_join(a, root, "build", V, "src", "game", "Beta.o"), &fns));
    REQUIRE(fns.n == 5);
    CHECK_CZ(fns.v[0].name, "jumper");
    CHECK_EQ_I64(fns.v[3].bind, 2);
    CHECK_EQ_I64(fns.v[4].bind, 0);
    CHECK(!names_elf_functions(a, avp_join(a, root, "AGENTS.md"), &fns));
}

AVP_TEST_MAIN("names")
