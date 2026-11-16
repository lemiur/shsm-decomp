/* tools/test_hashes.py on synthetic input: the hash functions against the retail archives' ground truth and check
 * values, the exclusions and chance arithmetic, the instruction-pair and DOL readers, the static-initialiser reader,
 * words, derivations, var_status and the vars filters. The real-tree tests are covered by the harness. */
#include "avp/fs.h"
#include "avp/sort.h"
#include "avp/test.h"
#include "hashes.h"

#include <string.h>

static AvpStr B(const char *s) { return avp_str(s); }

/* ---- hash functions ---- */

static const struct {
    const char *s;
    u32 v;
} ARC_TRUTH[] = {
    {"CombatAnims", 0xB636F7AE},          {"AFX_MixerSettings.csv", 0x53AA1026},
    {"CPlayer.Harry", 0x6DDA294E},        {"CPlayerCostume.Default", 0xCB2FB783},
    {"EnemySkin.AbstractStage1", 0x4EB0949E}, {"EnemySkin.AbstractStage2", 0x4EB0949D},
    {"LoadSave.arc", 0xC45E0D59},         {"HomeMenuEng.arc", 0xED2E14EE},
    {"IT_CrashIntro.TexStream", 0xB629017C}, {"afx_lookat.snd", 0x7A14D705},
};

AVP_TEST(arcname_ground_truth) {
    for (size_t i = 0; i < sizeof ARC_TRUTH / sizeof *ARC_TRUTH; i++)
        CHECK_EQ_U64(hashes_arcname(B(ARC_TRUTH[i].s)), ARC_TRUTH[i].v);
}

AVP_TEST(arcname_basename_and_case) {
    CHECK_EQ_U64(hashes_arcname(B("data\\sub/CombatAnims")), 0xB636F7AE);
    CHECK_EQ_U64(hashes_arcname(B("COMBATANIMS")), 0xB636F7AE);
    CHECK_EQ_U64(hashes_arcname(avp_strn("CombatAnims\0junk", 16)), 0xB636F7AE);
    CHECK_EQ_U64(hashes_arcname(B("")), 0);
}

AVP_TEST(djb2) {
    CHECK_EQ_U64(hashes_djb2(B(""), -1), 5381);
    CHECK_EQ_U64(hashes_djb2(B("a"), -1), 5381 * 33 + 97);
    CHECK_EQ_U64(hashes_djb2(B("ab"), -1), (u32)((5381 * 33 + 97) * 33 + 98));
    CHECK_EQ_U64(hashes_djb2(B("\xe9"), -1), 5381 * 33 - 23); /* signed char (extsb) */
    CHECK_EQ_U64(hashes_djb2(B("abc"), 1), hashes_djb2(B("a"), -1));
    char x40[44];
    memset(x40, 'x', 40);
    memcpy(x40 + 40, "\0yy", 3);
    CHECK_EQ_U64(hashes_djb2(avp_strn(x40, 40), -1), hashes_djb2(avp_strn(x40, 43), -1));
}

AVP_TEST(djb2fold_clears_bit5_of_every_byte) {
    CHECK_EQ_U64(hashes_djb2fold(B("abc"), -1), hashes_djb2(B("ABC"), -1));
    CHECK_EQ_U64(hashes_djb2fold(B("a"), -1), 5381 * 33 + 0x41);
    CHECK_EQ_U64(hashes_djb2fold(B("_"), -1), 5381 * 33 + 0x5F);
    CHECK_EQ_U64(hashes_djb2fold(B("0"), -1), 5381 * 33 + 0x10); /* digits change too */
    CHECK_EQ_U64(hashes_djb2fold(B("\xe9"), -1), (u32)(5381 * 33 + (0xC9 - 0x100)));
}

AVP_TEST(eventxor) {
    CHECK_EQ_U64(hashes_eventxor(B("a")), 97);
    CHECK_EQ_U64(hashes_eventxor(B("ab")), (97 << 1) ^ 98);
    CHECK_EQ_U64(hashes_eventxor(B("\xff")), 0xFFFFFFFF); /* (signed char)-1 */
}

AVP_TEST(crc32_check_value) { CHECK_EQ_U64(hashes_crc32(B("123456789")), 0xCBF43926); }

AVP_TEST(registry) {
    const char *names[] = {"arcname", "djb2", "djb2fold", "eventxor"};
    for (int i = 0; i < HASHES_NAME_COUNT; i++) {
        CHECK(strcmp(HASHES[i].name, names[i]) == 0);
        CHECK(HASHES[i].name_hash);
    }
    REQUIRE(hashes_by_name("crc32") != NULL);
    CHECK(!hashes_by_name("crc32")->name_hash);
    CHECK(hashes_by_name("nosuch") == NULL);
}

/* ---- classification ---- */

AVP_TEST(exclusions) {
    CHECK(strcmp(hashes_excluded(0x1234), "small integer") == 0);
    CHECK(strcmp(hashes_excluded(0xFFFFFFF0), "small integer") == 0);
    CHECK(strcmp(hashes_excluded(0x80529C30), "address") == 0);
    CHECK(strcmp(hashes_excluded(0xCC006000), "address") == 0);
    CHECK(strcmp(hashes_excluded(0x00FF0000), "bit mask") == 0);
    CHECK(strcmp(hashes_excluded(0x7FFFFFFF), "bit mask") == 0);
    CHECK(strcmp(hashes_excluded(0xFF00FFFF), "bit mask") == 0); /* complement of a run */
    CHECK(strcmp(hashes_excluded(0x00100000), "bit mask") == 0);
    CHECK(hashes_excluded(0xB636F7AE) == NULL);
    CHECK(hashes_excluded(0x00FF00FF) == NULL);
}

AVP_TEST(chance) {
    CHECK_EQ_F64(hashes_expected_chance(1 << 16, 1 << 16), 1.0);
    CHECK_EQ_F64(hashes_expected_chance(5471, 2284), 5471.0 * 2284 / 4294967296.0);
    CHECK(strcmp(hashes_strength(0.005), "strong") == 0);
    CHECK(strcmp(hashes_strength(0.05), "medium") == 0);
    CHECK(strcmp(hashes_strength(0.5), "weak") == 0);
}

/* ---- code words and the DOL ---- */

static u32 lis(u32 rd, i32 imm) { return 15u << 26 | rd << 21 | ((u32)imm & 0xFFFF); }
static u32 addi(u32 rd, u32 ra, i32 imm) { return 14u << 26 | rd << 21 | ra << 16 | ((u32)imm & 0xFFFF); }
static u32 ori(u32 ra, u32 rs, u32 imm) { return 24u << 26 | rs << 21 | ra << 16 | imm; }
static u32 addis(u32 rd, u32 ra, i32 imm) { return 15u << 26 | rd << 21 | ra << 16 | ((u32)imm & 0xFFFF); }
static u32 cmplwi(u32 ra, u32 imm) { return 10u << 26 | ra << 16 | imm; }
#define NOP 0x60000000u

static bool has_const(const HashesConsts *c, u32 v, const char *kind, i64 addr) {
    for (size_t i = 0; i < c->n; i++)
        if (c->v[i].value == v && strcmp(c->v[i].kind, kind) == 0 && c->v[i].address == addr) return true;
    return false;
}

AVP_TEST(code_word_pairs) {
    u32 words[] = {lis(3, 0x1234), NOP, addi(4, 3, 0x5678),          /* lis/addi, other destination */
                   lis(5, 0xB637), addi(5, 5, -0x0852),               /* carry: 0xB636F7AE */
                   lis(6, 0xDEAD), ori(6, 6, 0xBEEF),
                   addis(0, 3, -0x1234), cmplwi(0, 0x5678),           /* compare with 0x12345678 */
                   lis(7, 0x1111), lis(7, 0x2222), addi(7, 7, 1)};    /* reload ends the first pair */
    HashesConsts got = {0};
    hashes_scan_code_words(words, sizeof words / sizeof *words, 0x80000000, &got);
    CHECK(has_const(&got, 0x12345678, "lis/addi", 0x80000000));
    CHECK(has_const(&got, 0xB636F7AE, "lis/addi", 0x8000000C));
    CHECK(has_const(&got, 0xDEADBEEF, "lis/ori", 0x80000014));
    CHECK(has_const(&got, 0x12345678, "addis/cmplwi", 0x8000001C));
    CHECK(has_const(&got, 0x22220001, "lis/addi", 0x80000028));
    for (size_t i = 0; i < got.n; i++) CHECK(got.v[i].value != 0x11110001);
    avp_vec_free(&got);
}

static void put32(u8 *p, u32 v) {
    p[0] = (u8)(v >> 24), p[1] = (u8)(v >> 16), p[2] = (u8)(v >> 8), p[3] = (u8)v;
}

AVP_TEST(dol) {
    AvpArena *a = avp_arena();
    static const char DATA[] = "CombatAnims\0ab\0\x01xyz\0";
    size_t dn = sizeof DATA - 1 + 4;
    dn += (4 - dn % 4) % 4;
    size_t total = 0x100 + 8 + dn;
    u8 *blob = avp_alloc(a, total);
    put32(blob + 0, 0x100);
    put32(blob + 18 * 4, 0x80003000);
    put32(blob + 36 * 4, 8);
    put32(blob + 7 * 4, 0x108);
    put32(blob + 25 * 4, 0x80004000);
    put32(blob + 43 * 4, (u32)dn);
    put32(blob + 0x100, lis(3, 0xB637));
    put32(blob + 0x104, addi(3, 3, -0x0852));
    memcpy(blob + 0x108, DATA, sizeof DATA - 1);
    put32(blob + 0x108 + sizeof DATA - 1, 0xB636F7AE);
    ShsmDol *d = shsm_dol_parse(a, blob, total);
    AvpStr s;
    REQUIRE(hashes_cstring(d, 0x80004000, &s));
    CHECK(avp_str_eqz(s, "CombatAnims"));
    CHECK(!hashes_cstring(d, 0x8000400F, &s)); /* starts at a control byte */
    HashesDolStrings strs = hashes_dol_strings(a, d);
    bool combat = false, ab = false, xyz = false;
    for (size_t i = 0; i < strs.n; i++) {
        if (avp_str_eqz(strs.v[i].text, "CombatAnims")) combat = strs.v[i].address == 0x80004000;
        ab |= avp_str_eqz(strs.v[i].text, "ab");
        xyz |= avp_str_eqz(strs.v[i].text, "xyz");
    }
    CHECK(combat);
    CHECK(ab);
    CHECK(xyz);
    avp_vec_free(&strs);
    HashesConsts consts = hashes_code_constants(d);
    CHECK(has_const(&consts, 0xB636F7AE, "lis/addi", 0x80003000));
    CHECK(has_const(&consts, 0xB636F7AE, "data", 0x80004014));
    avp_vec_free(&consts);
}

/* ---- the static-initialiser reader ---- */

static const char SINIT_S[] =
    "# .text:0x0 | 0x80087700 | size: 0x40\n"
    ".fn __sinit_\\\\Thing_cpp, local\n"
    "/* 80087700 00083740  3C 60 80 3F */\tlis r3, lbl_803E80B0@ha\n"
    "/* 80087704 00083744  38 63 80 B0 */\taddi r3, r3, lbl_803E80B0@l\n"
    "/* 80087708 00083748  48 12 03 7D */\tbl fn_801A7A84__FPCc\n"
    "/* 8008770C 0008374C  90 6D 80 00 */\tstw r3, lbl_80529688@sda21(r0)\n"
    "/* 80087710 00083750  3C 80 80 3F */\tlis r31, lbl_803ED318@ha\n"
    "/* 80087714 00083754  38 84 D3 18 */\taddi r31, r31, lbl_803ED318@l\n"
    "/* 80087718 00083758  38 7F 01 3C */\taddi r3, r31, 0x13c\n"
    "/* 8008771C 0008375C  48 12 03 69 */\tbl fn_801A7A84__FPCc\n"
    "/* 80087720 00083760  7C 7E 1B 78 */\tmr r30, r3\n"
    "/* 80087724 00083764  93 CD 80 08 */\tstw r30, lbl_80529690@sda21(r0)\n"
    "/* 80087728 00083768  38 6D 80 04 */\tli r3, lbl_8052968C@sda21\n"
    "/* 8008772C 0008376C  38 9F 00 00 */\taddi r4, r31, 0x0\n"
    "/* 80087730 00083770  48 12 18 3D */\tbl __ct__12unk_801A8F6CFv\n"
    "/* 80087734 00083774  80 6D 80 10 */\tlwz r3, lbl_80529698@sda21(r0)\n"
    "/* 80087738 00083778  48 12 03 4D */\tbl fn_801B6020\n"
    "/* 8008773C 0008377C  90 6D 80 0C */\tstw r3, lbl_80529694@sda21(r0)\n"
    ".L_80087740:\n"
    "/* 80087740 00083780  48 12 03 4D */\tbl fn_801A7A84__FPCc\n"
    "/* 80087744 00083784  90 6D 80 14 */\tstw r3, lbl_8052969C@sda21(r0)\n"
    "/* 80087748 00083788  4E 80 00 20 */\tblr\n"
    ".endfn __sinit_\\\\Thing_cpp\n";

static bool sym_addr(void *ctx, AvpStr sym, i64 *addr) {
    (void)ctx;
    if (avp_str_startswith(sym, AVP_S("lbl_")) && sym.n == 12) {
        u64 v;
        if (avp_parse_uint(avp_strn(sym.p + 4, 8), 16, &v)) {
            *addr = (i64)v;
            return true;
        }
    }
    return false;
}

static bool deref(void *ctx, bool ptr, i64 address, AvpStr *text, const char **via) {
    (void)ctx;
    if (ptr) {
        if (address != 0x80529698) return false;
        *text = AVP_S("ViaPointer");
        *via = "ptr";
        return true;
    }
    const char *t = address == 0x803E80B0 ? "CHitEffectManager" : address == 0x803ED318 + 0x13C ? "AreaTriggerBox"
                                                              : address == 0x803ED318 ? "Pooled" : NULL;
    if (!t) return false;
    *text = avp_str(t);
    *via = "literal";
    return true;
}

static HashesPairs pairs_of_sinit(void) {
    ShsmAsmFile *f = shsm_asm_parse_text(avp_strn(SINIT_S, sizeof SINIT_S - 1), "t.s", "t", 0);
    static const HashesCallSpec INTERN = {"intern", "r3", false}, DJB2 = {"djb2", "r3", false},
                                REG = {"registry", "r4", true};
    AvpMap *calls = avp_map_new();
    avp_map_put(calls, AVP_S("fn_801A7A84__FPCc"), NULL)->p = (void *)&INTERN;
    avp_map_put(calls, AVP_S("fn_801B6020"), NULL)->p = (void *)&DJB2;
    avp_map_put(calls, AVP_S("__ct__12unk_801A8F6CFv"), NULL)->p = (void *)&REG;
    HashesReader rd = {NULL, NULL, sym_addr, deref};
    HashesPairs p = hashes_read_pairs(avp_arena(), f->blocks.v[0], calls, &rd);
    for (size_t i = 0; i < p.n; i++) { /* the views must outlive the parsed file */
        p.v[i].var = avp_str_dup(avp_arena(), p.v[i].var);
        p.v[i].text = avp_str_dup(avp_arena(), p.v[i].text);
    }
    avp_map_free(calls);
    shsm_asm_free(f);
    return p;
}

static bool has_pair(const HashesPairs *p, const char *var, const char *text, const char *call, const char *via) {
    for (size_t i = 0; i < p->n; i++)
        if (avp_str_eqz(p->v[i].var, var) && avp_str_eqz(p->v[i].text, text) && strcmp(p->v[i].call, call) == 0 &&
            strcmp(p->v[i].via, via) == 0)
            return true;
    return false;
}

AVP_TEST(static_initialiser) {
    HashesPairs p = pairs_of_sinit();
    CHECK_EQ_U64(p.n, 4);
    CHECK(has_pair(&p, "lbl_80529688", "CHitEffectManager", "intern", "literal"));
    CHECK(has_pair(&p, "lbl_80529690", "AreaTriggerBox", "intern", "literal")); /* pooled base + offset, via mr */
    CHECK(has_pair(&p, "lbl_8052968C", "Pooled", "registry", "literal"));
    CHECK(has_pair(&p, "lbl_80529694", "ViaPointer", "djb2", "ptr"));
    avp_vec_free(&p);
}

AVP_TEST(state_cleared_at_labels) {
    HashesPairs p = pairs_of_sinit();
    for (size_t i = 0; i < p.n; i++) CHECK(!avp_str_eqz(p.v[i].var, "lbl_8052969C"));
    avp_vec_free(&p);
}

/* ---- words, derivations, var_status ---- */

AVP_TEST(read_words) {
    AvpArena *a = avp_arena();
    char *d = avp_mkdtemp(a, "hashes", NULL, NULL);
    REQUIRE(d != NULL);
    const char *t = avp_path_join(a, d, "names.txt"), *b = avp_path_join(a, d, "blob.bin");
    static const char TEXT[] = "CombatAnims\nfoo = \"Bar.csv\", baz\n\n";
    static const char BLOB[] = "\0\0\x01" "CPlayer.Harry\0\x02" "ab\0";
    REQUIRE(avp_write_file(t, TEXT, sizeof TEXT - 1));
    REQUIRE(avp_write_file(b, BLOB, sizeof BLOB - 1));
    AvpStrVec paths = {0};
    avp_vec_push(&paths, avp_str(t));
    avp_vec_push(&paths, avp_str(b));
    AvpSet *w = hashes_read_words(&paths);
    CHECK(avp_set_has(w, AVP_S("CombatAnims")));
    CHECK(avp_set_has(w, AVP_S("Bar.csv")));
    CHECK(avp_set_has(w, AVP_S("baz")));
    CHECK(avp_set_has(w, AVP_S("CPlayer.Harry")));
    CHECK(!avp_set_has(w, AVP_S("ab")));
    avp_set_free(w);
    avp_vec_free(&paths);
    avp_rmtree(d, 0);
}

AVP_TEST(format_derivations) {
    AvpStr corpus[] = {AVP_S("%s.snd"), AVP_S("%s/%d"), AVP_S("a %s b"), AVP_S("afx_lookat"), AVP_S("two words")};
    AvpStrVec fmts = {0}, bases = {0};
    hashes_format_derivations(avp_arena(), corpus, 5, &fmts, &bases);
    REQUIRE(fmts.n == 1);
    CHECK(avp_str_eqz(fmts.v[0], "%s.snd"));
    REQUIRE(bases.n == 1);
    CHECK(avp_str_eqz(bases.v[0], "afx_lookat"));
    avp_vec_free(&fmts);
    avp_vec_free(&bases);
}

AVP_TEST(var_status) {
    AvpMap *names = avp_map_new();
    AvpSet *s = avp_set_new();
    avp_set_add(s, AVP_S("AreaTriggerBox"));
    avp_map_put(names, AVP_S("lbl_80529C30"), NULL)->p = s;
    CHECK(strcmp(hashes_var_status(names, AVP_S("lbl_80529C30"), AVP_S("AreaTriggerBox")), "named") == 0);
    CHECK(strcmp(hashes_var_status(names, AVP_S("lbl_80529C30"), AVP_S("Other")), "mismatch") == 0);
    CHECK(strcmp(hashes_var_status(names, AVP_S("lbl_80529C38"), AVP_S("X")), "new") == 0);
    avp_set_free(s);
    avp_map_free(names);
}

/* ---- vars filters ---- */

#define SINIT "__sinit_\\Foo_cpp"
static const HashesVarRow VARS_ROWS[] = {
    {0x80529B18, "lbl_80529B18", 0, "Foo", "intern", "literal", 1, "fn_800CA1F4", 0x800CA1F4, 1, "main/foo"},
    {0x80529B1C, "lbl_80529B1C", 0, "Foo", "registry", "literal", 2, "fn_800CA1F4", 0x800CA1F4, 1, "main/foo"},
    {0x80529B20, "lbl_80529B20", 0, "(Foo.Size)", "djb2", "literal", 3, SINIT, 0x800CA200, 1, "main/foo"},
    {0x80529C28, "lbl_80529C28", 0, "Bar", "intern", "literal", 4, "fn_800D11B0", 0x800D11B0, 1, "main/bar"},
    {0x80529C30, "lbl_80529C30", 0, "(Hash)", "djb2", "literal", 5, "Update__3BarFv", 0x800D1200, 0, "main/bar"},
};

/* cmd_vars over VARS_ROWS with one pair already named (lbl_80529B1C = "Foo"); *vars gets the sorted variable names. */
static int run_vars(HashesArgs *args, bool named, AvpStrVec *vars, HashesOut *o) {
    HashesVarRows rows = {0};
    for (size_t i = 0; i < 5; i++) avp_vec_push(&rows, VARS_ROWS[i]);
    AvpMap *names = avp_map_new();
    AvpSet *s = avp_set_new();
    avp_set_add(s, AVP_S("Foo"));
    if (named) avp_map_put(names, AVP_S("lbl_80529B1C"), NULL)->p = s;
    *o = (HashesOut){0};
    int rc = hashes_cmd_vars_rows(args, &rows, names, o);
    *vars = (AvpStrVec){0};
    AvpJson *pairs = avp_json_getz(o->data, "pairs");
    for (size_t i = 0; i < avp_json_len(pairs); i++)
        avp_vec_push(vars, avp_json_get_str(avp_json_at(pairs, i), "var", AVP_S("")));
    avp_sort_strs(vars->v, vars->n);
    avp_set_free(s);
    avp_map_free(names);
    avp_vec_free(&rows);
    return rc;
}

static bool vars_are(const AvpStrVec *v, const char *const *want, size_t n) {
    if (v->n != n) return false;
    for (size_t i = 0; i < n; i++)
        if (!avp_str_eqz(v->v[i], want[i])) return false;
    return true;
}

AVP_TEST(pattern_filters_on_variable_text_function_and_unit) {
    AvpStrVec v;
    HashesOut o;
    static const char *const FOO[] = {"lbl_80529B18", "lbl_80529B1C", "lbl_80529B20"};
    static const char *const C28[] = {"lbl_80529C28"};
    static const char *const BAR[] = {"lbl_80529C28", "lbl_80529C30"};
    run_vars(&(HashesArgs){.pattern = "foo"}, true, &v, &o);
    CHECK(vars_are(&v, FOO, 3));
    run_vars(&(HashesArgs){.grep = "80529C28"}, true, &v, &o);
    CHECK(vars_are(&v, C28, 1));
    run_vars(&(HashesArgs){.pattern = "main/bar"}, true, &v, &o);
    CHECK(vars_are(&v, BAR, 2));
    CHECK_EQ_I64(run_vars(&(HashesArgs){.pattern = "nosuch"}, true, &v, &o), 1);
}

AVP_TEST(class_selects_registry_string_and_function_scope) {
    AvpStrVec v;
    HashesOut o;
    static const char *const FOO[] = {"lbl_80529B18", "lbl_80529B1C", "lbl_80529B20"};
    static const char *const BAR[] = {"lbl_80529C28", "lbl_80529C30"};
    run_vars(&(HashesArgs){.cls = "Foo"}, true, &v, &o);
    CHECK(vars_are(&v, FOO, 3)); /* sinit's djb2 too */
    run_vars(&(HashesArgs){.cls = "bar"}, true, &v, &o);
    CHECK(vars_are(&v, BAR, 2)); /* Bar::Update's hash */
    CHECK_EQ_I64(run_vars(&(HashesArgs){.cls = "Nothing"}, true, &v, &o), 1);
}

AVP_TEST(filters_compose_with_status_and_call) {
    AvpStrVec v;
    HashesOut o;
    static const char *const NEW[] = {"lbl_80529B18", "lbl_80529B20"};
    static const char *const B18[] = {"lbl_80529B18"};
    static const char *const B1C[] = {"lbl_80529B1C"};
    run_vars(&(HashesArgs){.cls = "Foo", .status = "new"}, true, &v, &o);
    CHECK(vars_are(&v, NEW, 2));
    run_vars(&(HashesArgs){.cls = "Foo", .status = "new", .call = "intern"}, true, &v, &o);
    CHECK(vars_are(&v, B18, 1));
    run_vars(&(HashesArgs){.pattern = "Foo", .grep = "B1C", .status = "named"}, true, &v, &o);
    CHECK(vars_are(&v, B1C, 1));
    CHECK_EQ_I64(run_vars(&(HashesArgs){.cls = "Bar", .status = "named"}, true, &v, &o), 1);
}

AVP_TEST(by_class_groups_and_reports_counts) {
    AvpStrVec v;
    HashesOut o;
    run_vars(&(HashesArgs){.by_class = true}, true, &v, &o);
    AvpJson *classes = avp_json_getz(o.data, "classes");
    CHECK_EQ_U64(avp_json_len(classes), 2);
    CHECK_EQ_I64(avp_json_get_i64(classes, "Foo", -1), 3);
    CHECK_EQ_I64(avp_json_get_i64(classes, "Bar", -1), 2);
    run_vars(&(HashesArgs){.by_class = true}, false, &v, &o);
    i64 bar = -1, foo = -1, n = 0;
    for (size_t i = 0; i < o.lines.n; i++) {
        if (!avp_str_startswith(o.lines.v[i], AVP_S("class "))) continue;
        n++;
        if (avp_str_eqz(o.lines.v[i], "class Bar (2)")) bar = (i64)i;
        if (avp_str_eqz(o.lines.v[i], "class Foo (3)")) foo = (i64)i;
    }
    CHECK_EQ_I64(n, 2);
    CHECK(bar >= 0 && foo > bar);
}

/* ---- var_names (the API other tools use) ---- */

#include <sqlite3.h>

AVP_TEST(var_names) {
    AvpArena *a = avp_arena();
    char *d = avp_mkdtemp(a, "hashes", NULL, NULL);
    REQUIRE(d != NULL);
    ShsmRepo *r = shsm_repo_new(d, "R5WEA4");
    CHECK(hashes_var_names(a, r, "main/foo", NULL, 0, NULL, 0) == NULL); /* no database: None */
    REQUIRE(avp_makedirs(r->index_dir, true));
    sqlite3 *db;
    REQUIRE(sqlite3_open(hashes_db_path(a, r), &db) == SQLITE_OK);
    REQUIRE(sqlite3_exec(db, "CREATE TABLE vars (var_addr INTEGER, var TEXT, var_off INTEGER, text TEXT, call TEXT,"
                             " via TEXT, site INTEGER, func TEXT, func_addr INTEGER, sinit INTEGER, unit TEXT);"
                             "INSERT INTO vars VALUES (16, 'lbl_b', 0, 'B', 'registry', 'literal', 2, 'f', 100, 1,"
                             " 'main/foo');"
                             "INSERT INTO vars VALUES (8, 'lbl_a', 4, 'A', 'intern', 'literal', 1, 'g', 200, 0,"
                             " 'main/bar');"
                             "INSERT INTO vars VALUES (32, 'lbl_c', 0, 'C', 'djb2', 'ptr', 3, 'h', 300, 0, '');",
                         NULL, NULL, NULL) == SQLITE_OK);
    sqlite3_close(db);
    HashesVarNames *v = hashes_var_names(a, r, "foo.cpp", NULL, 0, NULL, 0); /* any unit spelling */
    REQUIRE(v != NULL);
    REQUIRE(v->n == 1);
    CHECK(strcmp(v->v[0].var, "lbl_b") == 0 && v->v[0].sinit && strcmp(v->v[0].kind, "class-registry object") == 0);
    i64 addrs[] = {32}, fns[] = {200};
    v = hashes_var_names(a, r, NULL, addrs, 1, fns, 1);
    REQUIRE(v != NULL);
    REQUIRE(v->n == 2); /* ORDER BY var_addr */
    CHECK(strcmp(v->v[0].var, "lbl_a") == 0 && v->v[0].var_off == 4 && strcmp(v->v[0].kind, "interned name") == 0);
    CHECK(strcmp(v->v[1].var, "lbl_c") == 0 && strcmp(v->v[1].unit, "") == 0);
    AvpJson *j = hashes_var_name_json(a, &v->v[1]);
    CHECK(avp_json_get_i64(j, "function_addr", -1) == 300);
    v = hashes_var_names(a, r, NULL, NULL, 0, NULL, 0);
    REQUIRE(v != NULL);
    CHECK(v->n == 0);
    avp_rmtree(d, 0);
}

AVP_TEST_MAIN("hashes")
