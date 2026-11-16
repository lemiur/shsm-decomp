/* Ports tools/test_twins.py (FingerprintTest, StatsTest) on synthetic listings and hand-built models. The real-tree
 * cases (RealTreeTest) are covered by the differential harness (diff/cases.txt). */
#include "avp/fmt.h"
#include "avp/test.h"
#include "twins.h"

#include <string.h>

/* listing(name, addr, body): a dtk-style function block; labels end with ':'. */
static void listing(AvpBuf *b, const char *name, u32 addr, const char *const *body, size_t n) {
    AvpBuf rows = {0};
    u32 a = addr;
    for (size_t i = 0; i < n; i++) {
        size_t len = strlen(body[i]);
        if (len && body[i][len - 1] == ':') {
            avp_buf_printf(&rows, "%s\n", body[i]);
            continue;
        }
        avp_buf_printf(&rows, "/* %08X %08X  60 00 00 00 */\t%s\n", a, a - 0x80000000u, body[i]);
        a += 4;
    }
    avp_buf_printf(b, "# .text:0x0 | 0x%08X | size: 0x%X\n.fn %s, global\n", addr, a - addr, name);
    avp_buf_put(b, avp_buf_str(&rows));
    avp_buf_printf(b, ".endfn %s\n", name);
    avp_buf_free(&rows);
}

static void getter(AvpBuf *b, const char *name, u32 addr, const char *sym, const char *off, const char *reg) {
    AvpArena *a = avp_arena();
    char fn[64];
    strcpy(fn, sym);
    memcpy(fn, "fn_", 3); /* sym.replace("lbl", "fn") */
    memmove(fn + 2, fn + 3, strlen(fn + 3) + 1);
    const char *body[] = {
        avp_sprintf(a, "lwz r3, %s(%s)", off, reg),
        avp_sprintf(a, "lis r5, %s@ha", sym),
        avp_sprintf(a, "addi r5, r5, %s@l", sym),
        "cmpwi r3, 0x0",
        avp_sprintf(a, "beq .L_%08X", addr + 0x18),
        avp_sprintf(a, "bl %s", fn),
        avp_sprintf(a, ".L_%08X:", addr + 0x18),
        "blr",
    };
    listing(b, name, addr, body, 8);
}

static ShsmAsmFile *parse(AvpBuf *b) { return shsm_asm_parse_text(avp_buf_str(b), "t.s", "t", 0); }

static char FP[6][3][21];

static void setup(ShsmAsmFile **out) {
    AvpBuf b = {0};
    avp_buf_putz(&b, ".text\n");
    getter(&b, "a", 0x80001000, "lbl_80002000", "0x8", "r4");
    getter(&b, "b", 0x80001100, "lbl_80002000", "0x8", "r4");  /* true duplicate of a */
    getter(&b, "c", 0x80001200, "lbl_80002100", "0x8", "r4");  /* other referents: exact twin only */
    getter(&b, "d", 0x80001300, "lbl_80002000", "0xc", "r4");  /* other offset: shape twin only */
    getter(&b, "e", 0x80001400, "lbl_80002000", "0x8", "r6");  /* other register: shape twin only */
    const char *f[] = {"lwz r3, 0x8(r4)", "blr"};
    listing(&b, "f", 0x80001500, f, 2);
    ShsmAsmFile *af = parse(&b);
    avp_buf_free(&b);
    size_t k = 0;
    for (size_t i = 0; i < af->blocks.n; i++)
        if (strcmp(af->blocks.v[i]->kind, "function") == 0) twins_fingerprint(af->blocks.v[i], FP[k++]);
    *out = af;
}

AVP_TEST(levels) {
    ShsmAsmFile *af;
    setup(&af);
    enum { A, B, C, D, E, F };
    for (int l = 0; l < 3; l++) CHECK(strcmp(FP[A][l], FP[B][l]) == 0);
    CHECK(strcmp(FP[A][0], FP[C][0]) == 0);
    CHECK(strcmp(FP[A][1], FP[C][1]) != 0);
    for (int n = D; n <= E; n++) {
        CHECK(strcmp(FP[A][0], FP[n][0]) != 0);
        CHECK(strcmp(FP[A][2], FP[n][2]) == 0);
    }
    CHECK(strcmp(FP[A][2], FP[F][2]) != 0);
    shsm_asm_free(af);
}

AVP_TEST(branch_targets_relative) {
    ShsmAsmFile *af;
    setup(&af);
    const ShsmAsmBlock *a = af->blocks.v[0];
    bool beq = false, bl = false;
    for (size_t i = 0; i < a->rows.n; i++) {
        AvpStr t[3];
        twins_normalise_row(avp_arena(), &a->rows.v[i], 0x80001000, "a", t);
        beq = beq || avp_str_eqz(t[0], "beq @+24");
        bl = bl || avp_str_eqz(t[0], "bl <rel24>");
    }
    CHECK(beq);
    CHECK(bl);
    shsm_asm_free(af);
}

AVP_TEST(self_branch) {
    AvpBuf b = {0};
    avp_buf_putz(&b, ".text\n");
    const char *body[] = {"addi r3, r3, 0x1", "b loop"};
    listing(&b, "loop", 0x80001000, body, 2);
    ShsmAsmFile *af = parse(&b);
    avp_buf_free(&b);
    const ShsmAsmBlock *l = af->blocks.v[0];
    AvpStr t[3];
    twins_normalise_row(avp_arena(), &l->rows.v[1], l->address, "loop", t);
    CHECK(avp_str_eqz(t[0], "b @+0"));
    shsm_asm_free(af);
}

AVP_TEST(shape_abstracts_registers_and_numbers) {
    ShsmAsmRow r = {0};
    r.mnemonic = AVP_S("addi");
    r.operands = AVP_S("r3, r31, -0x10");
    r.label = SHSM_NONE;
    r.sym_off = SHSM_NONE;
    r.word = SHSM_NONE;
    AvpStr t[3];
    twins_normalise_row(avp_arena(), &r, 0, NULL, t);
    CHECK(avp_str_eqz(t[2], "addi r, r, #"));
    r.mnemonic = AVP_S("cmpwi");
    r.operands = AVP_S("cr1, r3, 12");
    twins_normalise_row(avp_arena(), &r, 0, NULL, t);
    CHECK(avp_str_eqz(t[2], "cmpwi cr, r, #"));
}

/* FakeTwins(spec): (size, key, matched) per function */
typedef struct {
    i64 size;
    const char *key;
    bool matched;
} Spec;

static Twins *fake(const Spec *spec, size_t n) {
    Twins *tw = twins_new();
    for (size_t i = 0; i < n; i++) {
        TwinsFn f = {0};
        f.address = 0x80000000 + 0x100 * (i64)i;
        f.name = avp_sprintf(tw->arena, "fn%d", (int)i);
        f.unit = "u";
        f.size = spec[i].size;
        f.nrows = spec[i].size / 4;
        for (int l = 0; l < TWINS_NLEVELS; l++) f.fp[l] = spec[i].key;
        f.matched = spec[i].matched;
        f.status = "Asm";
        f.lib = "game";
        twins_add(tw, &f);
    }
    return tw;
}

AVP_TEST(threshold_and_counts) {
    Spec spec[56];
    for (int i = 0; i < 50; i++) spec[i] = (Spec){4, "blr", false};
    spec[50] = (Spec){40, "x", true};
    spec[51] = (Spec){40, "x", false};
    spec[52] = (Spec){40, "x", false};
    spec[53] = (Spec){64, "y", false};
    spec[54] = (Spec){64, "y", false};
    spec[55] = (Spec){80, "z", false};
    Twins *tw = fake(spec, 56);
    TwinsStats st[3];
    twins_compute_stats(tw, 16, st);
    CHECK_EQ_I64(st[0].functions, 6);
    CHECK_EQ_I64(st[0].groups, 2);
    CHECK_EQ_I64(st[0].unmatched_with_matched_twin, 2);
    CHECK_EQ_I64(st[0].unmatched_with_matched_twin_bytes, 80);
    CHECK_EQ_I64(st[0].unmatched_in_families, 2);
    twins_compute_stats(tw, 0, st);
    CHECK_EQ_I64(st[0].groups, 3);
    CHECK_EQ_I64(st[0].unmatched_in_families, 52);
    twins_close(tw);
}

AVP_TEST(lib_match) {
    AvpArena *a = avp_arena();
    TwinsFn f = {0};
    f.lib = "game/G3";
    CHECK(twins_lib_matches(a, &f, "game"));
    CHECK(twins_lib_matches(a, &f, "g3"));
    CHECK(twins_lib_matches(a, &f, "Game/G3"));
    CHECK(!twins_lib_matches(a, &f, "G4"));
    CHECK(!twins_lib_matches(a, &f, "gam"));
    CHECK(twins_lib_matches(a, &f, NULL));
    f.lib = "renderware";
    CHECK(!twins_lib_matches(a, &f, "game"));
}

AVP_TEST(capped_footer_counts_records) {
    Spec spec[10];
    spec[0] = (Spec){40, "x", true};
    for (int i = 0; i < 9; i++) spec[1 + i] = (Spec){40 + 4 * i, "x", false};
    Twins *tw = fake(spec, 10);
    TwinsArgs args = {.level = "exact", .min_size = 16, .limit = 3};
    TwinsOut out = {0};
    CHECK_EQ_I64(twins_cmd_unmatched(&args, tw, &out), 0);
    CHECK_EQ_I64((i64)out.lines.n, 5);
    CHECK(avp_str_eqz(out.lines.v[out.lines.n - 1], "... 6 more function(s), use --all"));
    CHECK(avp_str_eqz(out.lines.v[0], "9 unmatched function(s), 504 bytes, with a matched exact twin (min size 16)"));
    avp_vec_free(&out.lines);
    twins_close(tw);
}

AVP_TEST(families_order_and_footer) {
    Spec spec[] = {{20, "p", false}, {20, "p", false}, {48, "q", false}, {48, "q", false}, {48, "q", false},
                   {32, "r", true},  {32, "r", false}};
    Twins *tw = fake(spec, 7);
    TwinsArgs args = {.level = "exact", .min_size = 16, .min_count = 2, .limit = 1};
    TwinsOut out = {0};
    twins_cmd_families(&args, tw, &out);
    CHECK_EQ_I64((i64)out.lines.n, 3);
    CHECK(avp_str_startswith(out.lines.v[1], AVP_S("  3 x    48 =    144 bytes  game  in 1 unit(s): fn2, fn3, fn4")));
    CHECK(avp_str_eqz(out.lines.v[2], "... 1 more famil(ies), use --all"));
    avp_vec_free(&out.lines);
    twins_close(tw);
}

AVP_TEST_MAIN("twins")
