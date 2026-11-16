/* Ports tools/test_witness.py TestDecoder and TestElfAndRecovery (hand-assembled words, a synthetic ELF), plus the
 * executable's path errors and a synthetic ISO 9660 image booted through SYSTEM.CNF. */
#include "avp/test.h"
#include "shsm_cli.h"
#include "witness_ps2.h"

#include <stdio.h>
#include <string.h>

enum { ZERO = 0, AT = 1, V0 = 2, A0 = 4, SP = 29, RA = 31 };

static u32 i_type(u32 op, u32 rs, u32 rt, i32 imm) { return (op << 26) | (rs << 21) | (rt << 16) | ((u32)imm & 0xFFFF); }
static u32 jal(u32 t) { return (3u << 26) | ((t >> 2) & 0x3FFFFFF); }
static u32 j(u32 t) { return (2u << 26) | ((t >> 2) & 0x3FFFFFF); }
static u32 addiu(u32 rt, u32 rs, i32 imm) { return i_type(9, rs, rt, imm); }
static u32 lui(u32 rt, i32 imm) { return i_type(15, 0, rt, imm); }

#define JR_RA ((u32)RA << 21 | 8)
#define NOP 0u

static WtInsn dec(u32 w, u32 pc) {
    WtInsn i;
    wt_decode(w, pc, &i);
    return i;
}

AVP_TEST(decoder_calls_and_jumps) {
    char buf[WT_INSN_TEXT];
    WtInsn i = dec(jal(0x123450), 0x100000);
    CHECK_STR(avp_str(wt_kind_name((WtKind)i.kind)), "call");
    CHECK_EQ_U64(i.target, 0x123450);
    CHECK_EQ_I64(i.dst, RA);
    i = dec(j(0x100100), 0x100000);
    CHECK(i.kind == WT_K_JUMP);
    CHECK_EQ_U64(i.target, 0x100100);
    CHECK(dec(JR_RA, 0).kind == WT_K_RET);
    i = dec((u32)V0 << 21 | 8, 0);
    CHECK(i.kind == WT_K_JUMPR);
    CHECK_EQ_I64(i.rs, V0);
    i = dec((u32)V0 << 21 | (u32)RA << 11 | 9, 0);
    CHECK(i.kind == WT_K_CALLR);
    CHECK_EQ_I64(i.rs, V0);
    CHECK_CZ(wt_format_insn(&i, buf), "jalr v0");
}

AVP_TEST(decoder_branches_with_delay_slots) {
    WtInsn i = dec(i_type(4, V0, ZERO, -1), 0x1000); /* beq v0, zero, .  (back-edge) */
    CHECK(i.kind == WT_K_BRANCH);
    CHECK_EQ_U64(i.target, 0x1000);
    CHECK(!i.likely);
    i = dec(i_type(21, V0, A0, 3), 0x1000); /* bnel: likely */
    CHECK_CZ(i.mn, "bnel");
    CHECK_EQ_U64(i.target, 0x1010);
    CHECK(i.likely);
    CHECK_CZ(dec(i_type(4, 0, 0, 2), 0x1000).mn, "b");
    i = dec(i_type(1, A0, 1, 4), 0x2000); /* bgez */
    CHECK_CZ(i.mn, "bgez");
    CHECK_EQ_U64(i.target, 0x2014);
    i = dec((17u << 26) | (8u << 21) | (1u << 16) | 2, 0x3000);
    CHECK_CZ(i.mn, "bc1t");
    CHECK(i.kind == WT_K_BRANCH);
    CHECK_EQ_U64(i.target, 0x300C);
}

AVP_TEST(decoder_immediates_loads_stores) {
    char buf[WT_INSN_TEXT];
    WtInsn i = dec(addiu(SP, SP, -32), 0);
    CHECK(i.kind == WT_K_IMM);
    CHECK_CZ(i.mn, "addiu");
    CHECK_EQ_I64(i.imm, -32);
    CHECK_EQ_I64(i.dst, SP);
    i = dec(i_type(13, ZERO, V0, 0x8000), 0); /* ori: zero-extended */
    CHECK_CZ(i.mn, "ori");
    CHECK_EQ_I64(i.imm, 0x8000);
    i = dec(lui(AT, 0x3F80), 0);
    CHECK(i.kind == WT_K_LUI);
    CHECK_EQ_I64(i.imm, 0x3F80);
    CHECK_EQ_I64(i.rt, AT);
    i = dec(i_type(35, SP, V0, -8), 0); /* lw v0, -8(sp) */
    CHECK(i.kind == WT_K_LOAD);
    CHECK_CZ(i.mn, "lw");
    CHECK_EQ_I64(i.imm, -8);
    CHECK_EQ_I64(i.rs, SP);
    CHECK_EQ_I64(i.dst, V0);
    i = dec(i_type(49, A0, 2, 4), 0); /* lwc1 $f2, 4(a0) */
    CHECK_CZ(i.mn, "lwc1");
    CHECK(i.fp);
    CHECK_EQ_I64(i.dst, -1);
    i = dec(i_type(63, SP, RA, 0), 0); /* sd ra, 0(sp) */
    CHECK(i.kind == WT_K_STORE);
    CHECK_CZ(i.mn, "sd");
    i = dec((17u << 26) | (4u << 21) | ((u32)AT << 16) | (0u << 11), 0);
    CHECK(i.kind == WT_K_MTC1);
    CHECK_EQ_I64(i.rt, AT);
    CHECK_EQ_I64(i.rd, 0);
    i = dec((u32)A0 << 21 | (u32)V0 << 11 | 45, 0);
    CHECK_CZ(wt_format_insn(&i, buf), "move v0, a0");
    CHECK(dec(0, 0).kind == WT_K_NOP);
}

AVP_TEST(decoder_formats) {
    char buf[WT_INSN_TEXT];
    WtInsn i = dec(i_type(13, A0, V0, 0x8000), 0);
    CHECK_CZ(wt_format_insn(&i, buf), "ori v0, a0, 0x8000");
    i = dec(addiu(SP, SP, -32), 0);
    CHECK_CZ(wt_format_insn(&i, buf), "addiu sp, sp, -32");
    i = dec(i_type(49, A0, 2, 4), 0);
    CHECK_CZ(wt_format_insn(&i, buf), "lwc1 $f2, 4(a0)");
    i = dec(i_type(5, V0, A0, -1), 0x1000);
    CHECK_CZ(wt_format_insn(&i, buf), "bne v0, a0, 0x1000");
    i = dec(0xFC000000u | 1, 0); /* opcode 63 is sd: a store */
    CHECK_CZ(wt_format_insn(&i, buf), "sd zero, 1(zero)");
    i = dec(0xE8000000u, 0); /* opcode 58: not in any table */
    CHECK_CZ(wt_format_insn(&i, buf), ".word 0xE8000000");
}

/* make_elf(text, rodata): a minimal ELF32 LE MIPS executable with .text, .rodata and a section-name table. */
static void put16(AvpBytes *b, u32 v) {
    avp_vec_push(b, (u8)v);
    avp_vec_push(b, (u8)(v >> 8));
}
static void put32(AvpBytes *b, u32 v) {
    put16(b, v & 0xFFFF);
    put16(b, v >> 16);
}
static void putn(AvpBytes *b, const void *p, size_t n) {
    for (size_t k = 0; k < n; k++) avp_vec_push(b, ((const u8 *)p)[k]);
}

static AvpBytes make_elf(const u32 *text, size_t ntext, const u8 *rodata, size_t nro) {
    const u32 text_addr = 0x100000, ro_addr = 0x101000, entry = 0x100000;
    static const char names[] = "\0.text\0.rodata\0.shstrtab\0";
    size_t nnames = sizeof names - 1; /* the literal's own NUL is the last one */
    u32 tb = (u32)(4 * ntext), off_text = 52, off_ro = off_text + tb, off_names = off_ro + (u32)nro;
    u32 shoff = (off_names + (u32)nnames + 3) & ~3u;
    AvpBytes b = {0};
    putn(&b, "\x7f" "ELF", 4);
    static const u8 ident[12] = {1, 1, 1};
    putn(&b, ident, 12);
    put16(&b, 2);
    put16(&b, 8);
    put32(&b, 1);
    put32(&b, entry);
    put32(&b, 0);
    put32(&b, shoff);
    put32(&b, 0);
    put16(&b, 52);
    put16(&b, 32);
    put16(&b, 0);
    put16(&b, 40);
    put16(&b, 4);
    put16(&b, 3);
    for (size_t k = 0; k < ntext; k++) put32(&b, text[k]);
    putn(&b, rodata, nro);
    putn(&b, names, nnames);
    while (b.n < shoff) avp_vec_push(&b, 0);
    for (int k = 0; k < 40; k++) avp_vec_push(&b, 0);
    const u32 sh[3][10] = {{1, 1, 6, text_addr, off_text, tb, 0, 0, 4, 0},
                           {7, 1, 2, ro_addr, off_ro, (u32)nro, 0, 0, 4, 0},
                           {15, 3, 0, 0, off_names, (u32)nnames, 0, 0, 1, 0}};
    for (int s = 0; s < 3; s++)
        for (int k = 0; k < 10; k++) put32(&b, sh[s][k]);
    return b;
}

#define F1 0x100030u
static const u32 *program(size_t *n) {
    static u32 p[32];
    size_t k = 0;
    /* f0 at 0x100000: a frame, a string argument formed across the jal's delay slot, a float immediate */
    p[k++] = addiu(SP, SP, -16);
    p[k++] = lui(A0, 0x10);
    p[k++] = jal(F1);
    p[k++] = addiu(A0, A0, 0x1001); /* delay slot: a0 = 0x101001 -> "hello" */
    p[k++] = lui(AT, 0x3F80);
    p[k++] = (17u << 26) | (4u << 21) | ((u32)AT << 16); /* mtc1 at, $f0 -> 1.0 */
    p[k++] = JR_RA;
    p[k++] = addiu(SP, SP, 16); /* delay slot */
    p[k++] = NOP;
    p[k++] = NOP;
    p[k++] = NOP;
    p[k++] = NOP;
    /* f1 at 0x100030: an integer constant, a loop, a forward branch over a mid-function return */
    p[k++] = addiu(V0, ZERO, 1234);
    p[k++] = i_type(5, V0, ZERO, -1); /* bne v0, zero, 100034 (back-edge) */
    p[k++] = NOP;
    p[k++] = i_type(4, A0, ZERO, 3); /* beq a0, zero, 10004C (past the jr below) */
    p[k++] = NOP;
    p[k++] = JR_RA;
    p[k++] = NOP;
    p[k++] = addiu(V0, ZERO, 7);
    p[k++] = JR_RA;
    p[k++] = NOP;
    /* f2 at 0x100058: never called, found by the sweep after f1's return */
    p[k++] = JR_RA;
    p[k++] = NOP;
    *n = k;
    return p;
}

static WtElf *test_elf(void) {
    size_t n;
    const u32 *p = program(&n);
    static const u8 ro[] = {0, 'h', 'e', 'l', 'l', 'o', 0, 0};
    AvpBytes b = make_elf(p, n, ro, sizeof ro);
    return wt_elf_new(avp_arena(), b.v, b.n);
}

AVP_TEST(elf_sections_and_strings) {
    WtElf *e = test_elf();
    size_t n;
    const u32 *p = program(&n);
    CHECK_EQ_I64(e->machine, 8);
    REQUIRE(e->sections.n == 2);
    CHECK_CZ(e->sections.v[0].kind, "text");
    CHECK_CZ(e->sections.v[1].kind, "data");
    CHECK_CZ(e->sections.v[0].name, ".text");
    CHECK_CZ(wt_elf_cstring(avp_arena(), e, 0x101001), "hello");
    CHECK(wt_elf_cstring(avp_arena(), e, 0x101002) == NULL); /* not at a string start */
    u32 w = 0;
    CHECK(wt_elf_word(e, 0x100000, &w));
    CHECK_EQ_U64(w, p[0]);
}

AVP_TEST(recovery_and_features) {
    WtSide fns;
    WtRuns runs;
    WtFacts facts;
    wt_build_ps2_elf(avp_arena(), test_elf(), &fns, &runs, &facts);
    REQUIRE(fns.v.n == 3);
    CHECK_EQ_I64(fns.v.v[0]->addr, 0x100000);
    CHECK_EQ_I64(fns.v.v[1]->addr, F1);
    CHECK_EQ_I64(fns.v.v[2]->addr, 0x100058);
    WtFn *f0 = wt_side_get(&fns, 0x100000), *f1 = wt_side_get(&fns, F1);
    REQUIRE(f0 && f1);
    CHECK_EQ_I64(f0->size, 0x20);
    CHECK_EQ_I64(f1->size, 0x28); /* the mid-function jr does not end it */
    REQUIRE(f0->calls.n == 1);
    CHECK_EQ_I64(f0->calls.v[0], F1);
    CHECK_EQ_I64(f0->sites.v[0], 0x100008);
    CHECK_EQ_I64(f0->frame, 16);
    CHECK(avp_set_len(f0->strs) == 1 && avp_set_has(f0->strs, AVP_S("hello")));
    CHECK(avp_pyintset_len(f0->floats) == 1 && avp_pyintset_has(f0->floats, 0x3F800000));
    CHECK(avp_pyintset_has(f1->ints, 1234));
    CHECK_EQ_I64(f1->back, 1);
    CHECK_EQ_I64(facts.machine, 8);
    CHECK_EQ_I64(facts.entry, 0x100000);
    CHECK_EQ_I64(facts.words, 24);
    CHECK_EQ_I64(facts.gp, WT_NONE);
    CHECK(facts.sections.n == 2);
    CHECK(runs.n == 0);
}

static int raise_code(void (*fn)(void *), void *ctx, char *msg, size_t cap) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        fn(ctx);
        avp_try_end(&t);
        return 0;
    }
    snprintf(msg, cap, "%s", t.msg);
    return t.code;
}

static void parse_garbage(void *ctx) {
    (void)ctx;
    static const char g[] = "not an elf at all";
    wt_elf_new(avp_arena(), (const u8 *)g, sizeof g - 1);
}

static void find_missing(void *ctx) { wt_find_exe(NULL, (const char *)ctx); }

AVP_TEST(iso_free_path_errors) {
    char msg[1024];
    CHECK_EQ_I64(raise_code(parse_garbage, NULL, msg, sizeof msg), SHSM_EXC_USAGE);
    CHECK_CZ(msg, "not a 32-bit little-endian ELF");
    CHECK_EQ_I64(raise_code(find_missing, "build/tmp/witness-test/no-such.elf", msg, sizeof msg), SHSM_EXC_USAGE);
    CHECK_CZ(msg, "--exe build/tmp/witness-test/no-such.elf: no such file");
}

/* A tiny ISO 9660 image: SYSTEM.CNF boots SLUS_123.45 (the synthetic ELF). */
static void dir_record(u8 *d, size_t *at, const char *name, size_t nl, u32 extent, u32 size, bool dir) {
    size_t ln = 33 + nl + ((33 + nl) & 1);
    u8 *r = d + *at;
    r[0] = (u8)ln;
    for (int k = 0; k < 4; k++) {
        r[2 + k] = (u8)(extent >> (8 * k));
        r[10 + k] = (u8)(size >> (8 * k));
    }
    r[25] = dir ? 2 : 0;
    r[32] = (u8)nl;
    memcpy(r + 33, name, nl);
    *at += ln;
}

AVP_TEST(iso_boot_executable) {
    AvpArena *a = avp_arena();
    WtElf *e = test_elf();
    static const char cnf[] = "BOOT2 = cdrom0:\\SLUS_123.45;1\r\nVER = 1.00\r\n";
    size_t nsec = 24;
    u8 *img = avp_alloc(a, nsec * 2048);
    u8 *pvd = img + 16 * 2048;
    pvd[0] = 1;
    memcpy(pvd + 1, "CD001", 5);
    size_t at = 0;
    u8 *root = img + 18 * 2048;
    dir_record(root, &at, "\0", 1, 18, 2048, true); /* the "." entry */
    dir_record(root, &at, "\1", 1, 18, 2048, true);
    dir_record(root, &at, "SYSTEM.CNF;1", 12, 19, (u32)(sizeof cnf - 1), false);
    dir_record(root, &at, "slus_123.45;1", 13, 20, (u32)e->size, false);
    size_t rat = 0;
    dir_record(pvd + 156, &rat, "\0", 1, 18, 2048, true);
    memcpy(img + 19 * 2048, cnf, sizeof cnf - 1);
    memcpy(img + 20 * 2048, e->data, e->size);
    REQUIRE(avp_makedirs("build/tmp/witness-test", true));
    const char *path = "build/tmp/witness-test/test.iso";
    REQUIRE(avp_write_file(path, img, nsec * 2048));
    WtExe exe = wt_find_exe(NULL, path);
    CHECK_CZ(exe.member, "boot");
    AvpBytes data = {0};
    CHECK_CZ(wt_load_exe(a, exe, &data), "test.iso:SLUS_123.45");
    REQUIRE(data.n == e->size);
    CHECK(memcmp(data.v, e->data, e->size) == 0);
}

AVP_TEST_MAIN("witness")
