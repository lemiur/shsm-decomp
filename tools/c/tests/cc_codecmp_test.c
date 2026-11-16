/* Unit tests of the function comparison (lib/cc_elf, cc_listing, cc_codecmp, cc_compare) on small fixtures: a
 * synthetic project (symbols.txt, splits.txt, a DOL) in a temporary folder, retail and compiled listings as text and
 * a synthetic ELF object. The expected strings are what tools/codecmp.py gives for the same inputs. The real-tree
 * differential is tools/c/tests/diff/codecmp_diff.py. */
#include "avp/avp.h"
#include "avp/test.h"
#include "cc_compare.h"

/* ---- fixtures --------------------------------------------------------------------------------------------------- */

static void put32(AvpBuf *b, u32 v) {
    char c[4] = {(char)(v >> 24), (char)(v >> 16), (char)(v >> 8), (char)v};
    avp_buf_putn(b, c, 4);
}
static void put16(AvpBuf *b, u16 v) {
    char c[2] = {(char)(v >> 8), (char)v};
    avp_buf_putn(b, c, 2);
}
static void pad4(AvpBuf *b) {
    while (b->n % 4) avp_buf_putc(b, 0);
}

typedef struct {
    const char *name;
    u32 value, size;
    u8 type;
    u16 shndx;
} TSym;
typedef struct {
    u32 off, type, sym;
    i32 addend;
} TRel;

/* An object with sections 1 .text, 2 .rodata, 3 .symtab, 4 .strtab, 5 .shstrtab, 6 .rela.text. */
static AvpStr build_elf(const u8 *text, size_t ntext, const u8 *ro, size_t nro, const TSym *syms, size_t nsyms,
                        const TRel *rels, size_t nrels) {
    AvpBuf strtab = {0}, shstr = {0}, symtab = {0}, rela = {0}, f = {0};
    avp_buf_putc(&strtab, 0);
    for (int k = 0; k < 16; k++) avp_buf_putc(&symtab, 0);
    for (size_t k = 0; k < nsyms; k++) {
        put32(&symtab, (u32)strtab.n);
        avp_buf_putn(&strtab, syms[k].name, strlen(syms[k].name) + 1);
        put32(&symtab, syms[k].value);
        put32(&symtab, syms[k].size);
        avp_buf_putc(&symtab, (char)(0x10 | syms[k].type));
        avp_buf_putc(&symtab, 0);
        put16(&symtab, syms[k].shndx);
    }
    for (size_t k = 0; k < nrels; k++) {
        put32(&rela, rels[k].off);
        put32(&rela, (rels[k].sym << 8) | rels[k].type);
        put32(&rela, (u32)rels[k].addend);
    }
    const char *names[] = {"", ".text", ".rodata", ".symtab", ".strtab", ".shstrtab", ".rela.text"};
    u32 nameoff[7];
    for (int k = 0; k < 7; k++) {
        nameoff[k] = (u32)shstr.n;
        avp_buf_putn(&shstr, names[k], strlen(names[k]) + 1);
    }
    const AvpBuf *blobs[7] = {NULL, NULL, NULL, &symtab, &strtab, &shstr, &rela};
    u32 off[7] = {0}, size[7] = {0};
    for (int k = 0; k < 52; k++) avp_buf_putc(&f, 0);
    for (int k = 1; k < 7; k++) {
        pad4(&f);
        off[k] = (u32)f.n;
        if (k == 1) avp_buf_putn(&f, text, ntext), size[k] = (u32)ntext;
        else if (k == 2) avp_buf_putn(&f, ro, nro), size[k] = (u32)nro;
        else avp_buf_putn(&f, blobs[k]->p, blobs[k]->n), size[k] = (u32)blobs[k]->n;
    }
    pad4(&f);
    u32 shoff = (u32)f.n;
    u32 type[7] = {0, 1, 1, 2, 3, 3, 4}, link[7] = {0, 0, 0, 4, 0, 0, 3}, info[7] = {0, 0, 0, 0, 0, 0, 1};
    for (int k = 0; k < 7; k++) {
        put32(&f, nameoff[k]);
        put32(&f, type[k]);
        put32(&f, 0);
        put32(&f, 0);
        put32(&f, off[k]);
        put32(&f, size[k]);
        put32(&f, link[k]);
        put32(&f, info[k]);
        put32(&f, 4);
        put32(&f, k == 3 ? 16 : k == 6 ? 12 : 0);
    }
    u8 *h = (u8 *)f.p;
    memcpy(h, "\x7f" "ELF\x01\x02\x01", 7);
    h[17] = 1;                    /* ET_REL */
    h[19] = 20;                   /* EM_PPC */
    h[32] = (u8)(shoff >> 24), h[33] = (u8)(shoff >> 16), h[34] = (u8)(shoff >> 8), h[35] = (u8)shoff;
    h[47] = 40;                   /* shentsize */
    h[49] = 7;                    /* shnum */
    h[51] = 5;                    /* shstrndx */
    return avp_strn(f.p, f.n);
}

static void W32(u8 *p, u32 v) {
    p[0] = (u8)(v >> 24), p[1] = (u8)(v >> 16), p[2] = (u8)(v >> 8), p[3] = (u8)v;
}

static CcProject *g_project;

static CcContext *fixture_context(void) {
    if (!g_project) {
        AvpArena *a = avp_arena();
        char *root = avp_mkdtemp(a, "cc-codecmp-test-", "", NULL);
        AVP_ASSERT(root);
        AVP_ASSERT(avp_makedirs(avp_sprintf(a, "%s\\config\\R5WEA4", root), true));
        AVP_ASSERT(avp_makedirs(avp_sprintf(a, "%s\\orig\\R5WEA4\\sys", root), true));
        AVP_ASSERT(avp_write_text(avp_sprintf(a, "%s\\config\\R5WEA4\\symbols.txt", root),
                                  AVP_S("fn_80001000 = .text:0x80001000; // type:function size:0x20\n"
                                        "foo = .text:0x80003000; // type:function size:0x4\n"
                                        "bar = .text:0x80003100; // type:function size:0x4\n"
                                        "lbl_80004000 = .data:0x80004000; // type:object size:0x10\n"),
                                  AVP_TM_UTF8));
        AVP_ASSERT(avp_write_text(avp_sprintf(a, "%s\\config\\R5WEA4\\splits.txt", root),
                                  AVP_S("Sections:\n\t.text       type:code align:4\n\t.rodata     type:rodata\n\n"
                                        "test/Unit.cpp:\n\t.text       start:0x80001000 end:0x80001020\n"
                                        "\t.rodata     start:0x80002000 end:0x80002010\n"),
                                  AVP_TM_UTF8));
        /* DOL: text0 at 0x80001000 (0x20 bytes), data0 at 0x80002000 (0x10 bytes: the float 1.0f first) */
        u8 dol[0x130];
        memset(dol, 0, sizeof dol);
        W32(dol + 0, 0x100u);
        W32(dol + 72, 0x80001000u);
        W32(dol + 144, 0x20u);
        W32(dol + 28, 0x120u);
        W32(dol + 100, 0x80002000u);
        W32(dol + 172, 0x10u);
        W32(dol + 216, 0x80005000u);
        W32(dol + 220, 0x100u);
        W32(dol + 0x120, 0x3F800000u);
        AVP_ASSERT(avp_write_file(avp_sprintf(a, "%s\\orig\\R5WEA4\\sys\\main.dol", root), dol, sizeof dol));
        g_project = cc_project_new(root);
    }
    return cc_context_new(g_project, AVP_S("test/Unit"));
}

static const CcFn *one(const AvpMap *fns, const char *name) {
    AvpVal *v = avp_map_get(fns, avp_str(name));
    AVP_ASSERT(v);
    return v->p;
}

/* compare one function given as retail text, compiled text and an object */
static CcCompareResult *compare_texts(const char *retail, const char *compiled, AvpStr obj, const char *name) {
    AvpArena *a = avp_arena();
    const char *why;
    CcElf *elf = cc_elf_parse(a, obj.p, obj.n, &why);
    AVP_ASSERT(elf);
    AvpMap *r = cc_parse_listing(a, avp_str(retail), NULL);
    AvpMap *c = cc_parse_listing(a, avp_str(compiled), elf);
    CcFn *cfn = (CcFn *)one(c, name);
    const CcFn *rfn = one(r, name);
    CcFnResult *res = cc_compare_function(a, rfn, cfn, fixture_context(), true);
    return cc_compare_result_new(a, avp_str(name), avp_str(name), rfn, cfn, res);
}

static const u8 NOP4[32] = {0};

/* ---- parsing ---------------------------------------------------------------------------------------------------- */

AVP_TEST(parse_row_operands) {
    AvpArena *a = avp_arena();
    CcRow r = cc_parse_row(a, 0, AVP_S("  lwz   r0, \"foo bar\"+0x4@sda21(r13) "));
    CHECK_STR(r.text, "lwz r0, \"foo bar\"+0x4@sda21(r13)");
    CHECK_STR(r.norm, "lwz r0, SYM@sda21(r13)");
    CHECK(r.has_ref);
    CHECK_STR(r.ref.name, "foo bar");
    CHECK_EQ_I64(r.ref.off, 4);
    CHECK_STR(r.ref.kind, "sda21");
    r = cc_parse_row(a, 0, AVP_S("addi r3, r3, lbl_80004000-0x10@l"));
    CHECK_STR(r.norm, "addi r3, r3, SYM@l");
    CHECK_EQ_I64(r.ref.off, -16);
    r = cc_parse_row(a, 0, AVP_S("bl memset+0x8"));
    CHECK_STR(r.norm, "bl SYM");
    CHECK_STR(r.ref.kind, "rel24");
    CHECK_EQ_I64(r.ref.off, 8);
    r = cc_parse_row(a, 0, AVP_S("bne cr1, .L_80006804"));
    CHECK_STR(r.norm, "bne cr1, L");
    CHECK(r.has_label);
    CHECK_EQ_I64(r.label, 0x80006804);
    r = cc_parse_row(a, 0, AVP_S("b 0x1234"));
    CHECK(!r.has_ref);
    CHECK_STR(r.norm, "b 0x1234");
    r = cc_parse_row(a, 0, AVP_S("li r3, @123"));
    CHECK(r.stray);
    r = cc_parse_row(a, 0, AVP_S("blr"));
    CHECK_STR(r.norm, "blr");
    CHECK(!r.stray);
}

AVP_TEST(names) {
    i64 v;
    CHECK(cc_embedded_address(AVP_S("fn_800067CC__8unk_RampFf"), &v) && v == 0x800067CC);
    CHECK(cc_embedded_address(AVP_S("gap_00_800062B4_init"), &v) && v == 0x800062B4);
    CHECK(cc_embedded_address(AVP_S("@stringBase0_80401234"), &v) && v == 0x80401234);
    CHECK(!cc_embedded_address(AVP_S("fn_800067cc"), &v));
    CHECK(cc_is_compiler_local(AVP_S("@123")));
    CHECK(cc_is_compiler_local(AVP_S("...bss.0")));
    CHECK(cc_is_compiler_local(AVP_S("@etb_80001000")));
    CHECK(!cc_is_compiler_local(AVP_S("foo")));
    CHECK(cc_is_gap(AVP_S("gap_00_800062B4_init")));
    CHECK(!cc_is_gap(AVP_S("gap_00_800062B4")));
    AvpMap *r = avp_map_new();
    avp_map_put(r, AVP_S("fn_80001000__Fv"), NULL);
    CHECK_STR(cc_retail_name_for(AVP_S("fn_80001000__Fi"), r), "fn_80001000__Fv");
    CHECK(cc_retail_name_for(AVP_S("foo"), r).p == NULL);
}

/* ---- comparisons ------------------------------------------------------------------------------------------------ */

/* A self-recursive call: retail keeps no relocation (the word branches to the function start), the compiled object
 * relocates it against the function's own symbol. Both sides are a local branch to row 0. */
AVP_TEST(self_call_is_local_branch) {
    const char *retail = ".fn fn_80001000, global\n"
                         "/* 80001000 00000000  94 21 FF F0 */\tstwu r1, -0x10(r1)\n"
                         "/* 80001004 00000004  4B FF FF FD */\tbl fn_80001000\n"
                         "/* 80001008 00000008  4E 80 00 20 */\tblr\n"
                         ".endfn fn_80001000\n";
    const char *compiled = ".fn fn_80001000, global\n"
                           "/* 00000000 00000034  94 21 FF F0 */\tstwu r1, -0x10(r1)\n"
                           "/* 00000004 00000038  48 00 00 01 */\tbl fn_80001000\n"
                           "/* 00000008 0000003C  4E 80 00 20 */\tblr\n"
                           ".endfn fn_80001000\n";
    TSym syms[] = {{"fn_80001000", 0, 12, CC_STT_FUNC, 1}};
    TRel rels[] = {{4, CC_R_REL24, 1, 0}};
    CcCompareResult *r = compare_texts(retail, compiled, build_elf(NOP4, 12, NULL, 0, syms, 1, rels, 1), "fn_80001000");
    CHECK(r->exact);
    CHECK_EQ_U64(r->checked_targets, 1);
    CHECK_STR(r->pairs[1].retail->norm, "bl L");
    CHECK_STR(r->pairs[1].compiled->norm, "bl L");
}

/* A raw retail operand (dtk had no symbol) against a compiler-local literal: equal by a value proof. */
AVP_TEST(raw_operand_by_value) {
    const char *retail = ".fn fn_80001000, global\n"
                         "/* 80001000 00000000  3C 60 80 00 */\tlis r3, 0x8000\n"
                         "/* 80001004 00000004  C0 23 20 00 */\tlfs f1, 0x2000(r3)\n"
                         "/* 80001008 00000008  4E 80 00 20 */\tblr\n"
                         ".endfn fn_80001000\n";
    const char *compiled = ".fn fn_80001000, global\n"
                           "/* 00000000 00000034  3C 60 00 00 */\tlis r3, \"@12\"@ha\n"
                           "/* 00000004 00000038  C0 23 00 00 */\tlfs f1, \"@12\"@l(r3)\n"
                           "/* 00000008 0000003C  4E 80 00 20 */\tblr\n"
                           ".endfn fn_80001000\n";
    const u8 ro[4] = {0x3F, 0x80, 0, 0};
    TSym syms[] = {{"fn_80001000", 0, 12, CC_STT_FUNC, 1}, {"@12", 0, 4, CC_STT_OBJECT, 2}};
    TRel rels[] = {{2, CC_R_ADDR16_HA, 2, 0}, {6, CC_R_ADDR16_LO, 2, 0}};
    CcCompareResult *r = compare_texts(retail, compiled, build_elf(NOP4, 12, ro, 4, syms, 2, rels, 2), "fn_80001000");
    CHECK(r->exact);
    CHECK_EQ_U64(r->raw->notes.n, 2);
    CHECK_STR(r->raw->notes.v[0].detail,
              "raw operand 0x8000 is @12@ha: by layout the literal sits at 0x80002000, where retail holds its bytes");
    CHECK_STR(r->raw->notes.v[1].detail,
              "raw operand 0x2000 is @12@l: by layout the literal sits at 0x80002000, where retail holds its bytes");
    CHECK_EQ_U64(r->counts.equal, 3);
}

/* Without the bytes at the literal's layout address the rows stay different. */
AVP_TEST(raw_operand_wrong_bytes) {
    const char *retail = ".fn fn_80001000, global\n"
                         "/* 80001000 00000000  3C 60 80 00 */\tlis r3, 0x8000\n"
                         ".endfn fn_80001000\n";
    const char *compiled = ".fn fn_80001000, global\n"
                           "/* 00000000 00000034  3C 60 00 00 */\tlis r3, \"@12\"@ha\n"
                           ".endfn fn_80001000\n";
    const u8 ro[4] = {0x40, 0, 0, 0};
    TSym syms[] = {{"fn_80001000", 0, 4, CC_STT_FUNC, 1}, {"@12", 0, 4, CC_STT_OBJECT, 2}};
    TRel rels[] = {{2, CC_R_ADDR16_HA, 2, 0}};
    CcCompareResult *r = compare_texts(retail, compiled, build_elf(NOP4, 4, ro, 4, syms, 2, rels, 1), "fn_80001000");
    CHECK(!r->exact);
    CHECK_EQ_U64(r->counts.replace, 1);
    CHECK_EQ_U64(r->raw->notes.n, 0);
}

AVP_TEST(wrong_callee) {
    const char *retail = ".fn fn_80001000, global\n"
                         "/* 80001000 00000000  48 00 20 01 */\tbl foo\n"
                         ".endfn fn_80001000\n";
    const char *compiled = ".fn fn_80001000, global\n"
                           "/* 00000000 00000034  48 00 00 01 */\tbl bar\n"
                           ".endfn fn_80001000\n";
    TSym syms[] = {{"fn_80001000", 0, 4, CC_STT_FUNC, 1}, {"bar", 0, 0, 0, 0}};
    TRel rels[] = {{0, CC_R_REL24, 2, 0}};
    CcCompareResult *r = compare_texts(retail, compiled, build_elf(NOP4, 4, NULL, 0, syms, 2, rels, 1), "fn_80001000");
    CHECK(!r->exact);
    CHECK_EQ_U64(r->edit, 0);
    REQUIRE(r->target_mismatches->n == 1);
    CHECK_STR(r->target_mismatches->v[0].detail, "retail foo (0x80003000) != compiled bar (0x80003100)");
    CHECK(strcmp(r->target_mismatches->v[0].kind, "target") == 0);
    CHECK(strcmp(r->pairs[0].target, "mismatch") == 0);
    CHECK(cc_pair_differs(&r->pairs[0]));
    AvpStr txt = cc_compare_result_render(avp_arena(), r, false, 2, 40);
    char want[512];
    snprintf(want, sizeof want, "%s\nT   0 %-40s | bl bar\n%s",
             "fn_80001000: differs, edit 0, 1/1 rows, 1 target mismatches, 0 unresolved", "bl foo",
             "        -> retail foo (0x80003000) != compiled bar (0x80003100)");
    CHECK_CZ(avp_cz(avp_arena(), txt), want);
}

AVP_TEST(wrong_global) {
    const char *retail = ".fn fn_80001000, global\n"
                         "/* 80001000 00000000  3C 60 80 00 */\tlis r3, lbl_80004000@ha\n"
                         "/* 80001004 00000004  38 63 40 00 */\taddi r3, r3, lbl_80004000@l\n"
                         ".endfn fn_80001000\n";
    const char *compiled = ".fn fn_80001000, global\n"
                           "/* 00000000 00000034  3C 60 00 00 */\tlis r3, lbl_80004010@ha\n"
                           "/* 00000004 00000038  38 63 00 00 */\taddi r3, r3, lbl_80004010@l\n"
                           ".endfn fn_80001000\n";
    TSym syms[] = {{"fn_80001000", 0, 8, CC_STT_FUNC, 1}, {"lbl_80004010", 0, 0, 0, 0}};
    TRel rels[] = {{2, CC_R_ADDR16_HA, 2, 0}, {6, CC_R_ADDR16_LO, 2, 0}};
    CcCompareResult *r = compare_texts(retail, compiled, build_elf(NOP4, 8, NULL, 0, syms, 2, rels, 2), "fn_80001000");
    REQUIRE(r->target_mismatches->n == 2);
    CHECK_STR(r->target_mismatches->v[1].detail,
              "retail lbl_80004000 (0x80004000) != compiled lbl_80004010 (0x80004010)");
    AvpStr js = avp_json_dumps(avp_arena(), cc_compare_result_to_json(avp_arena(), r, false), NULL);
    CHECK_STR(js, "{\"name\": \"fn_80001000\", \"retail_name\": \"fn_80001000\", \"exact\": false, \"edit\": 0, "
                  "\"counts\": {\"equal\": 2, \"replace\": 0, \"delete\": 0, \"insert\": 0, \"retail_rows\": 2, "
                  "\"compiled_rows\": 2, \"target_mismatches\": 2, \"unresolved\": 0}, \"checked_targets\": 2, "
                  "\"target_mismatches\": [{\"i\": 0, \"j\": 0, \"retail\": \"lis r3, lbl_80004000@ha\", "
                  "\"compiled\": \"lis r3, lbl_80004010@ha\", \"detail\": \"retail lbl_80004000 (0x80004000) != "
                  "compiled lbl_80004010 (0x80004010)\", \"kind\": \"target\"}, {\"i\": 1, \"j\": 1, \"retail\": "
                  "\"addi r3, r3, lbl_80004000@l\", \"compiled\": \"addi r3, r3, lbl_80004010@l\", \"detail\": "
                  "\"retail lbl_80004000 (0x80004000) != compiled lbl_80004010 (0x80004010)\", \"kind\": \"target\"}], "
                  "\"unresolved\": []}");
}

AVP_TEST(branch_target) {
    const char *retail = ".fn fn_80001000, global\n"
                         "/* 80001000 00000000  2C 03 00 00 */\tcmpwi r3, 0x0\n"
                         "/* 80001004 00000004  41 82 00 08 */\tbeq .L_8000100C\n"
                         "/* 80001008 00000008  38 60 00 01 */\tli r3, 0x1\n"
                         ".L_8000100C:\n"
                         "/* 8000100C 0000000C  4E 80 00 20 */\tblr\n"
                         ".endfn fn_80001000\n";
    const char *compiled = ".fn fn_80001000, global\n"
                           "/* 00000000 00000034  2C 03 00 00 */\tcmpwi r3, 0x0\n"
                           "/* 00000004 00000038  41 82 00 04 */\tbeq .L_00000008\n"
                           ".L_00000008:\n"
                           "/* 00000008 0000003C  38 60 00 01 */\tli r3, 0x1\n"
                           "/* 0000000C 00000040  4E 80 00 20 */\tblr\n"
                           ".endfn fn_80001000\n";
    TSym syms[] = {{"fn_80001000", 0, 16, CC_STT_FUNC, 1}};
    CcCompareResult *r = compare_texts(retail, compiled, build_elf(NOP4, 16, NULL, 0, syms, 1, NULL, 0), "fn_80001000");
    CHECK(!r->exact);
    CHECK_EQ_U64(r->edit, 0);
    REQUIRE(r->target_mismatches->n == 1);
    CHECK_STR(r->target_mismatches->v[0].detail, "branches to retail row 3, compiled row 2 is retail row 2");
    CHECK(strcmp(r->target_mismatches->v[0].kind, "branch") == 0);
}

AVP_TEST(not_an_elf) {
    const char *why;
    CHECK(cc_elf_parse(avp_arena(), "\x7f" "ELF\x02\x02", 6, &why) == NULL);
    CHECK_CZ(why, "not a 32-bit big-endian ELF object");
}

AVP_TEST_MAIN("cc_codecmp")
