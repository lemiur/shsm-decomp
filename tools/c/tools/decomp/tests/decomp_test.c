/* Ports tools/test_decomp.py LocateTests, HeadlessArgvTests, SymbolRowTests, ElfTests and RefreshTests. */
#include "avp/test.h"
#include "decomp.h"

#include <stdlib.h>
#include <string.h>

/* REQUIRE for helpers that return a value: a failed requirement ends the test executable. */
#define MUST(cond) do { if (!(cond)) { avp_test__fail(__FILE__, __LINE__, "%s (required)", #cond); exit(1); } } while (0)

static const char SYMBOLS_TXT[] =
    "memcpy = .init:0x80005F34; // type:function size:0x29C scope:global\n"
    "__ct__4CFooFv = .text:0x80010000; // type:function size:0x40 scope:global\n"
    "__dt__4CFooFv = .text:0x80010040; // type:function size:0x40 scope:global\n"
    "Update__4CFooFfPC4CBarRi = .text:0x80010080; // type:function size:0x80 scope:global\n"
    "Init__Q24FMOD6MemoryFv = .text:0x80010100; // type:function size:0x20 scope:global\n"
    "fn_80010120__FPFi_vUc = .text:0x80010120; // type:function size:0x20 scope:global\n"
    "fn_80010140__FPCce = .text:0x80010140; // type:function size:0x20 scope:global\n"
    "fn_80010160__F9EResource = .text:0x80010160; // type:function size:0x20 scope:global\n"
    "lbl_80300000 = .rodata:0x80300000; // type:object size:0x4 data:float\n"
    "lbl_80300008 = .rodata:0x80300008; // type:object size:0x8 data:double\n"
    "lbl_80300010 = .rodata:0x80300010; // type:object size:0x6 data:string\n"
    "lbl_80526720 = .sdata:0x80526720; // type:object size:0x4 data:4byte\n"
    "lbl_8052B5E0 = .sdata2:0x8052B5E0; // type:object size:0x4 data:float\n"
    "__vt__4CFoo = .data:0x80400000; // type:object size:0x10 scope:global\n";

#define EXPECT_RAISE(code_, stmt)                                                                                    \
    do {                                                                                                             \
        AvpTry t_;                                                                                                   \
        if (AVP_TRY(&t_)) {                                                                                          \
            stmt;                                                                                                    \
            avp_try_end(&t_);                                                                                        \
            CHECK_MSG(false, "no exception from %s", #stmt);                                                         \
        } else {                                                                                                     \
            CHECK_EQ_I64(t_.code, (code_));                                                                          \
        }                                                                                                            \
    } while (0)

static ShsmSymbolTable *make_table(const char *text) {
    return shsm_parse_symbols(avp_arena(), avp_str(text), NULL);
}

static const char *tmpdir(const char *name) {
    AvpArena *a = avp_arena();
    const char *d = avp_path_abspath(a, avp_join(a, "build", "tmp", "decomp_test", name));
    avp_rmtree(d, AVP_RMTREE_IGNORE_ERRORS);
    MUST(avp_makedirs(d, true));
    return d;
}

static void put_file(const char *path, const char *text) {
    AvpArena *a = avp_arena();
    REQUIRE(avp_makedirs(avp_cz(a, avp_path_dirname(path)), true));
    REQUIRE(avp_write_file(path, text, strlen(text)));
}

/* fake_install(root, version, ldefs_in, language, nest) */
static const char *fake_install(const char *root, const char *version, const char *ldefs_in, const char *nest) {
    AvpArena *a = avp_arena();
    const char *inst = nest ? avp_path_join(a, root, nest) : root;
    put_file(avp_join(a, inst, "Ghidra", "application.properties"),
          avp_sprintf(a, "application.version=%s\napplication.release.name=PUBLIC\napplication.java.min=21\n", version));
    put_file(dc_headless_script(a, inst), "@echo off\n");
    if (ldefs_in)
        put_file(avp_join(a, ldefs_in, "GameCubeLoader", "data", "languages", "gekko.ldefs"),
              avp_sprintf(a, "<language id=\"%s\"/>", DC_GEKKO_LANGUAGE));
    return inst;
}

/* ---- LocateTests ---- */

AVP_TEST(nested_zip_layout_resolves) {
    AvpArena *a = avp_arena();
    const char *tmp = tmpdir("nested");
    const char *outer = avp_path_join(a, tmp, "ghidra_12.0.1_PUBLIC_20260114");
    const char *inst = fake_install(outer, "12.0.1", NULL, "ghidra_12.0.1_PUBLIC");
    CHECK_CZ(dc_resolve_install(a, outer), avp_path_abspath(a, inst));
    CHECK(dc_resolve_install(a, avp_path_join(a, tmp, "nothing")) == NULL);
}

AVP_TEST(prefers_candidate_with_gekko_language) {
    AvpArena *a = avp_arena();
    const char *tmp = tmpdir("prefers");
    const char *appdata = avp_path_join(a, tmp, "appdata");
    const char *ia = fake_install(avp_path_join(a, tmp, "a"), "12.0.1", NULL, NULL);
    const char *ib = fake_install(avp_path_join(a, tmp, "b"), "11.4", NULL, NULL);
    fake_install(ib, "11.4", avp_join(a, appdata, "ghidra", "ghidra_11.4_PUBLIC", "Extensions"), NULL);
    const char *cands[] = {ia, ib};
    const char *how = NULL;
    DcInstall info = dc_locate_ghidra(a, NULL, NULL, cands, 2, appdata, &how);
    CHECK_CZ(info.install, avp_path_abspath(a, ib));
    CHECK_CZ(info.language, DC_GEKKO_LANGUAGE);
    CHECK(strstr(how, "Gekko") != NULL);
}

AVP_TEST(falls_back_to_stock_language) {
    AvpArena *a = avp_arena();
    const char *tmp = tmpdir("fallback");
    const char *ia = fake_install(avp_path_join(a, tmp, "a"), "12.0.1", NULL, NULL);
    const char *how = NULL;
    DcInstall info = dc_locate_ghidra(a, NULL, NULL, &ia, 1, avp_path_join(a, tmp, "appdata"), &how);
    CHECK_CZ(info.language, DC_DEFAULT_LANGUAGE);
    CHECK_EQ_I64(info.java_min, 21);
    CHECK_CZ(how, "first candidate found");
}

AVP_TEST(explicit_choice_wins_and_is_checked) {
    AvpArena *a = avp_arena();
    const char *tmp = tmpdir("explicit");
    const char *appdata = avp_path_join(a, tmp, "appdata");
    const char *ia = fake_install(avp_path_join(a, tmp, "a"), "12.0.1", NULL, NULL);
    const char *ib = fake_install(avp_path_join(a, tmp, "b"), "11.4", NULL, NULL);
    const char *how = NULL;
    DcInstall info = dc_locate_ghidra(a, ia, ib, NULL, 0, appdata, &how);
    CHECK_CZ(info.install, avp_path_abspath(a, ia));
    CHECK_CZ(how, "--ghidra");
    info = dc_locate_ghidra(a, NULL, ib, &ia, 1, appdata, &how);
    CHECK_CZ(how, "GHIDRA_INSTALL_DIR");
    const char *missing = avp_path_join(a, tmp, "missing");
    EXPECT_RAISE(SHSM_EXC_USAGE, dc_locate_ghidra(a, missing, NULL, &missing, 0, appdata, &how));
    EXPECT_RAISE(SHSM_EXC_USAGE, dc_locate_ghidra(a, NULL, NULL, &missing, 1, appdata, &how));
}

AVP_TEST(java_version_parse) {
    CHECK_EQ_I64(dc_parse_java_version(AVP_S("java version \"24.0.2\" 2025-07-15")), 24);
    CHECK_EQ_I64(dc_parse_java_version(AVP_S("openjdk version \"21.0.4\"")), 21);
    CHECK_EQ_I64(dc_parse_java_version(AVP_S("java version \"1.8.0_202\"")), 8);
    CHECK_EQ_I64(dc_parse_java_version(AVP_S("nothing")), -1);
}

/* ---- HeadlessArgvTests ---- */

static AvpStr joined(AvpArena *a, const char **argv, size_t from, size_t n) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = from; i < n; i++) {
        if (i > from) avp_buf_putc(&b, '|');
        avp_buf_putz(&b, argv[i]);
    }
    return avp_buf_str(&b);
}

AVP_TEST(import_line) {
    AvpArena *a = avp_arena();
    const char *pre_args[] = {"pre", "s.tsv", "8052e720"}, *post_args[] = {"post", "s.tsv"};
    DcScript pre = {"A.java", pre_args, 3}, post = {"A.java", post_args, 2};
    DcHeadlessOpts o = {.import_path = "in.elf", .processor = DC_GEKKO_LANGUAGE, .cspec = "default", .pre = &pre,
                        .npre = 1, .post = &post, .npost = 1, .overwrite = true, .script_path = "S"};
    size_t n;
    const char **argv = dc_headless_argv(a, "G", "P", &o, &n);
    CHECK_CZ(argv[0], dc_headless_script(a, "G"));
    CHECK_CZ(argv[1], "P");
    CHECK_CZ(argv[2], DECOMP_PROJECT_NAME);
    CHECK_STR(joined(a, argv, 3, n), "-import|in.elf|-processor|" DC_GEKKO_LANGUAGE "|-cspec|default|-scriptPath|S|"
                                     "-preScript|A.java|pre|s.tsv|8052e720|-postScript|A.java|post|s.tsv|-overwrite");
    CHECK(argv[n] == NULL);
}

AVP_TEST(process_line) {
    AvpArena *a = avp_arena();
    const char *post_args[] = {"out", "8", "60"};
    DcScript post = {"E.java", post_args, 3};
    DcHeadlessOpts o = {.process = "main.elf", .no_analysis = true, .read_only = true, .post = &post, .npost = 1,
                        .script_path = "S"};
    size_t n;
    const char **argv = dc_headless_argv(a, "G", "P", &o, &n);
    CHECK_STR(joined(a, argv, 3, n), "-process|main.elf|-scriptPath|S|-postScript|E.java|out|8|60|-noanalysis|-readOnly");
}

AVP_TEST(bat_unsafe_arguments_refused) {
    AvpArena *a = avp_arena();
    size_t n;
    const char *empty[] = {"", "x"}, *bang[] = {"a!b"};
    DcScript s1 = {"E.java", empty, 2}, s2 = {"E.java", bang, 1};
    DcHeadlessOpts o1 = {.post = &s1, .npost = 1}, o2 = {.post = &s2, .npost = 1};
    EXPECT_RAISE(SHSM_EXC_USAGE, dc_headless_argv(a, "G", "P", &o1, &n));
    EXPECT_RAISE(SHSM_EXC_USAGE, dc_headless_argv(a, "G", "P", &o2, &n));
}

/* ---- SymbolRowTests ---- */

static const DcRow *row(const DcRows *rows, const char *name) {
    for (size_t i = 0; i < rows->n; i++)
        if (!strcmp(rows->v[i].name, name)) return &rows->v[i];
    return NULL;
}

AVP_TEST(kinds_and_sizes) {
    DcRows rows = dc_symbol_rows(avp_arena(), make_table(SYMBOLS_TXT));
    const DcRow *r = row(&rows, "memcpy");
    REQUIRE(r);
    CHECK_CZ(r->kind, "F");
    CHECK_EQ_I64(r->address, 0x80005F34);
    CHECK_EQ_I64(r->size, 0x29C);
    CHECK_CZ(r->params, "-"); /* unmangled: no signature */
    CHECK_CZ(row(&rows, "lbl_80300000")->kind, "O");
    CHECK_CZ(row(&rows, "lbl_80300000")->dtype, "float");
    CHECK_CZ(row(&rows, "lbl_80300008")->dtype, "double");
    CHECK_CZ(row(&rows, "lbl_80300010")->dtype, "string");
    CHECK_CZ(row(&rows, "lbl_80526720")->dtype, "");
}

AVP_TEST(member_signatures) {
    DcRows rows = dc_symbol_rows(avp_arena(), make_table(SYMBOLS_TXT));
    CHECK_CZ(row(&rows, "__ct__4CFooFv")->this_, "CFoo");
    CHECK_CZ(row(&rows, "__ct__4CFooFv")->params, "");
    CHECK_CZ(row(&rows, "__dt__4CFooFv")->this_, "CFoo");
    CHECK_CZ(row(&rows, "__dt__4CFooFv")->params, "int");
    const DcRow *u = row(&rows, "Update__4CFooFfPC4CBarRi");
    CHECK_CZ(u->this_, "CFoo");
    CHECK_CZ(u->params, "float;S:CBar*;int*");
    CHECK_CZ(u->demangled, "CFoo::Update(float, const CBar*, int&)");
}

AVP_TEST(unknown_scope_gets_no_signature) {
    DcRows rows = dc_symbol_rows(avp_arena(), make_table(SYMBOLS_TXT));
    CHECK_CZ(row(&rows, "Init__Q24FMOD6MemoryFv")->this_, "");
    CHECK_CZ(row(&rows, "Init__Q24FMOD6MemoryFv")->params, "-");
}

AVP_TEST(free_function_params) {
    DcRows rows = dc_symbol_rows(avp_arena(), make_table(SYMBOLS_TXT));
    CHECK_CZ(row(&rows, "fn_80010120__FPFi_vUc")->params, "code*;uchar");
    CHECK_CZ(row(&rows, "fn_80010140__FPCce")->params, "char*;...");
    CHECK_CZ(row(&rows, "fn_80010160__F9EResource")->params, "u4");
    CHECK_CZ(row(&rows, "fn_80010160__F9EResource")->this_, "");
}

AVP_TEST(param_spec) {
    AvpArena *a = avp_arena();
    CHECK_CZ(dc_param_spec(a, "const char*"), "char*");
    CHECK_CZ(dc_param_spec(a, "unsigned long long"), "ulonglong");
    CHECK_CZ(dc_param_spec(a, "nw4hbm::ut::Color&"), "S:nw4hbm::ut::Color*");
    CHECK_CZ(dc_param_spec(a, "TList<int*>*"), "S:TList<int*>*");
    CHECK_CZ(dc_param_spec(a, "float[3]"), "float*");
    CHECK_CZ(dc_param_spec(a, "void (*)(int)"), "code*");
    CHECK(dc_param_spec(a, "int CFoo::*") == NULL);
}

AVP_TEST(rows_round_trip) {
    AvpArena *a = avp_arena();
    DcRows rows = dc_symbol_rows(a, make_table(SYMBOLS_TXT));
    DcRows back = dc_parse_rows(a, dc_format_rows(a, rows.v, rows.n));
    REQUIRE(back.n == rows.n);
    for (size_t i = 0; i < rows.n; i++) CHECK(dc_row_eq(&rows.v[i], &back.v[i]));
}

AVP_TEST(sda_fallback_from_sections) {
    u64 sda, sda2;
    dc_sda_bases(avp_arena(), NULL, make_table(SYMBOLS_TXT), &sda, &sda2);
    CHECK_EQ_U64(sda, 0x8052E720);
    CHECK_EQ_U64(sda2, 0x805335E0);
}

/* ---- ElfTests ---- */

static size_t put32(u8 *p, size_t off, u32 v) {
    p[off] = (u8)(v >> 24), p[off + 1] = (u8)(v >> 16), p[off + 2] = (u8)(v >> 8), p[off + 3] = (u8)v;
    return off + 4;
}
static size_t put16(u8 *p, size_t off, u32 v) {
    p[off] = (u8)(v >> 8), p[off + 1] = (u8)v;
    return off + 2;
}

AVP_TEST(reads_symbols_from_a_synthetic_elf) {
    AvpArena *a = avp_arena();
    static const char names[] = "\0_SDA_BASE_\0_SDA2_BASE_\0";
    size_t names_n = sizeof names - 1;
    u8 buf[512] = {0};
    memcpy(buf, "\x7f" "ELF\x01\x02", 6);
    size_t strtab_off = 52, symtab_off = strtab_off + names_n, syms_n = 48, shoff = symtab_off + syms_n;
    memcpy(buf + strtab_off, names, names_n);
    size_t o = symtab_off + 16; /* symbol 0 is all zero */
    o = put32(buf, o, 1), o = put32(buf, o, 0x8052E720), o = put32(buf, o, 0), o += 2, o = put16(buf, o, 0xFFF1);
    o = put32(buf, o, 12), o = put32(buf, o, 0x805335E0), o = put32(buf, o, 0), o += 2, o = put16(buf, o, 0xFFF1);
    put32(buf, 0x20, (u32)shoff);
    put16(buf, 0x2E, 40);
    put16(buf, 0x30, 3);
    size_t sh = shoff + 40;
    u32 strtab_hdr[10] = {0, 3, 0, 0, (u32)strtab_off, (u32)names_n, 0, 0, 1, 0};
    u32 symtab_hdr[10] = {0, 2, 0, 0, (u32)symtab_off, (u32)syms_n, 1, 1, 4, 16};
    for (int i = 0; i < 10; i++) put32(buf, sh + 4 * (size_t)i, strtab_hdr[i]);
    for (int i = 0; i < 10; i++) put32(buf, sh + 40 + 4 * (size_t)i, symtab_hdr[i]);
    const char *p = avp_path_join(a, tmpdir("elf"), "x.elf");
    REQUIRE(avp_write_file(p, buf, sh + 80));
    const char *want[] = {"_SDA_BASE_", "_SDA2_BASE_"};
    bool has[2];
    u64 val[2];
    dc_elf_symbols(a, p, want, 2, has, val);
    CHECK(has[0] && has[1]);
    CHECK_EQ_U64(val[0], 0x8052E720);
    CHECK_EQ_U64(val[1], 0x805335E0);
    u64 sda, sda2;
    dc_sda_bases(a, p, make_table(""), &sda, &sda2);
    CHECK_EQ_U64(sda, 0x8052E720);
    CHECK_EQ_U64(sda2, 0x805335E0);
}

/* ---- RefreshTests ---- */

static AvpJson *jparse(const char *text) {
    AvpJsonError err = {0};
    AvpJson *v = avp_json_parse(avp_arena(), avp_str(text), &err);
    MUST(v != NULL);
    return v;
}

AVP_TEST(diff_and_affected) {
    AvpArena *a = avp_arena();
    DcRows old = dc_symbol_rows(a, make_table(SYMBOLS_TXT));
    AvpStr text = avp_str_replace(a, avp_str(SYMBOLS_TXT), AVP_S("lbl_80526720"), AVP_S("gCounter"), -1);
    text = avp_str_replace(a, text, AVP_S("fn_80010140__FPCce = .text:0x80010140; // type:function size:0x20 scope:global\n"),
                           AVP_S(""), -1);
    DcRows new_ = dc_symbol_rows(a, make_table(avp_cz(a, text)));
    DcRows delta;
    DcRanges ranges;
    AvpISet *funcs;
    dc_diff_rows(a, &old, &new_, &delta, &ranges, &funcs);
    REQUIRE(delta.n == 3);
    CHECK_CZ(delta.v[0].kind, "R"); /* removals are applied first */
    int seen = 0;
    for (size_t i = 0; i < delta.n; i++) {
        const DcRow *r = &delta.v[i];
        if (!strcmp(r->kind, "O") && !strcmp(r->name, "gCounter")) seen |= 1;
        if (!strcmp(r->kind, "R") && !strcmp(r->name, "fn_80010140__FPCce")) seen |= 2;
        if (!strcmp(r->kind, "R") && !strcmp(r->name, "lbl_80526720")) seen |= 4;
    }
    CHECK_EQ_I64(seen, 7);
    CHECK(avp_iset_has(funcs, 0x80010140));
    CHECK_EQ_I64((i64)avp_iset_len(funcs), 1);
    AvpJson *index = jparse("{\"functions\": {\"80010000\": {\"refs\": [\"80526720\"]}, "
                            "\"80010040\": {\"refs\": [\"80526722\"]}, \"80010080\": {\"refs\": [\"80300000\"]}, "
                            "\"80010140\": {\"refs\": []}}}");
    AvpStrVec got = dc_affected(a, index, &ranges, funcs);
    REQUIRE(got.n == 3);
    CHECK_STR(got.v[0], "80010000");
    CHECK_STR(got.v[1], "80010040");
    CHECK_STR(got.v[2], "80010140");
}

AVP_TEST(no_change_no_delta) {
    AvpArena *a = avp_arena();
    DcRows rows = dc_symbol_rows(a, make_table(SYMBOLS_TXT));
    DcRows delta;
    DcRanges ranges;
    AvpISet *funcs;
    dc_diff_rows(a, &rows, &rows, &delta, &ranges, &funcs);
    CHECK(delta.n == 0 && ranges.n == 0 && avp_iset_len(funcs) == 0);
}

AVP_TEST(stale_entries) {
    AvpStrVec s = dc_stale_entries(avp_arena(), jparse("{\"functions\": {\"1\": {\"stamp\": \"a\"}, \"2\": {\"stamp\": \"b\"}}}"), "a");
    REQUIRE(s.n == 1);
    CHECK_STR(s.v[0], "2");
}

AVP_TEST(merge_results) {
    AvpArena *a = avp_arena();
    const char *p = avp_path_join(a, tmpdir("merge"), "r.tsv");
    put_file(p, "address\tname\tfile\tstatus\treason\trefs\n"
             "80010000\t__ct__4CFooFv\t80010000___ct__4CFooFv.c\tok\t\t80526720,80300000\n"
             "80010040\t__dt__4CFooFv\t80010040___dt__4CFooFv.c\tfailed\ttimed out after 60s\t\n");
    AvpJson *index = jparse("{\"functions\": {}}");
    i64 ok, failed;
    dc_merge_results(a, index, p, avp_json_strz(a, "abc"), avp_json_strz(a, "11.4"), &ok, &failed);
    CHECK_EQ_I64(ok, 1);
    CHECK_EQ_I64(failed, 1);
    AvpJson *e = decomp_index_entry(index, 0x80010000);
    REQUIRE(e);
    AvpJson *refs = avp_json_getz(e, "refs");
    REQUIRE(avp_json_len(refs) == 2);
    CHECK_STR(avp_json_at(refs, 0)->u.str, "80526720");
    CHECK_STR(avp_json_at(refs, 1)->u.str, "80300000");
    CHECK_STR(avp_json_getz(e, "stamp")->u.str, "abc");
    CHECK_STR(avp_json_getz(e, "ghidra")->u.str, "11.4");
    CHECK_STR(avp_json_getz(decomp_index_entry(index, 0x80010040), "reason")->u.str, "timed out after 60s");
}

AVP_TEST_MAIN("decomp")
