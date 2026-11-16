/* Ports tools/test_fragments.py (RowTest, RepeatTest, SourceTest's _single_call) on synthetic listings. The
 * real-tree cases (RealTreeTest) and the source candidates are covered by the differential harness (diff/cases.txt). */
#include "avp/fmt.h"
#include "avp/fs.h"
#include "avp/test.h"
#include "frag.h"
#include "fragments_known.h"

#include <string.h>

static const char *const PROLOGUE[] = {"stwu r1, -0x20(r1)", "mflr r0", "stw r0, 0x24(r1)", "stw r31, 0x1c(r1)",
                                       "stw r30, 0x18(r1)"};
static const char *const EPILOGUE[] = {"lwz r0, 0x24(r1)", "lwz r31, 0x1c(r1)", "lwz r30, 0x18(r1)",
                                       "mtlr r0", "addi r1, r1, 0x20", "blr"};
static const char *const FILLER[] = {"ori", "xori", "andi.", "andis.", "oris", "xoris", "mulli", "subfic", "addic",
                                     "addis"};

typedef AVP_VEC(const char *) Body;

static void push_n(Body *b, const char *const *rows, size_t n) {
    for (size_t i = 0; i < n; i++) avp_vec_push(b, rows[i]);
}

static void filler(AvpArena *a, Body *b, int i) { avp_vec_push(b, avp_sprintf(a, "%s r4, r4, 0x%x", FILLER[i], 3 + i)); }

static void helper(AvpArena *a, Body *b, const char *x, const char *y, const char *z, const char *off) {
    avp_vec_push(b, avp_sprintf(a, "lwz %s, %s(%s)", x, off, y));
    avp_vec_push(b, avp_sprintf(a, "cmpwi %s, 0x0", x));
    avp_vec_push(b, avp_sprintf(a, "lwz %s, 0x4(%s)", z, y));
    avp_vec_push(b, avp_sprintf(a, "addi %s, %s, 0x1", z, z));
    avp_vec_push(b, avp_sprintf(a, "stw %s, 0x4(%s)", z, y));
    avp_vec_push(b, avp_sprintf(a, "slwi %s, %s, 2", x, x));
    avp_vec_push(b, avp_sprintf(a, "add %s, %s, %s", z, z, x));
    avp_vec_push(b, avp_sprintf(a, "stw %s, 0x8(%s)", z, y));
}

static void listing(AvpBuf *out, const char *name, u32 addr, const Body *body) {
    AvpBuf rows = {0};
    u32 a = addr;
    for (size_t i = 0; i < body->n; i++) {
        size_t len = strlen(body->v[i]);
        if (len && body->v[i][len - 1] == ':') {
            avp_buf_printf(&rows, "%s\n", body->v[i]);
            continue;
        }
        avp_buf_printf(&rows, "/* %08X %08X  60 00 00 00 */\t%s\n", a, a - 0x80000000u, body->v[i]);
        a += 4;
    }
    avp_buf_printf(out, "# .text:0x0 | 0x%08X | size: 0x%X\n.fn %s, global\n", addr, a - addr, name);
    avp_buf_put(out, avp_buf_str(&rows));
    avp_buf_printf(out, ".endfn %s\n", name);
    avp_buf_free(&rows);
}

static const char *const REGS[][3] = {{"r3", "r4", "r5"}, {"r6", "r7", "r8"}, {"r9", "r10", "r11"},
                                      {"r0", "r12", "r31"}, {"r29", "r30", "r28"}};
static const char *const OFFS[] = {"0x10", "0x14", "0x18", "0x1c", "0x20"};

static void make_functions(AvpArena *a, AvpBuf *out, const char *const (*regs)[3]) {
    for (int i = 0; i < 5; i++) {
        Body b = {0};
        push_n(&b, PROLOGUE, 5);
        filler(a, &b, i);
        helper(a, &b, regs[i][0], regs[i][1], regs[i][2], OFFS[i]);
        filler(a, &b, i + 5);
        push_n(&b, EPILOGUE, 6);
        listing(out, avp_sprintf(a, "fn_%d", i), 0x80001000u + 0x100u * (u32)i, &b);
        avp_vec_free(&b);
    }
}

/* corpus_of(text): both levels, rows normalised as build_corpus does */
static FragCorpus *corpus_of(AvpArena *a, AvpStr text) {
    AvpStr full = avp_pyfmt(a, ".text\n%S", &text);
    ShsmAsmFile *af = shsm_asm_parse_text(full, "t.s", "t", 0);
    FragCorpus *c = frag_corpus_new(FRAG_NLEVELS);
    for (size_t i = 0; i < af->blocks.n; i++) {
        const ShsmAsmBlock *b = af->blocks.v[i];
        if (strcmp(b->kind, "function") != 0) continue;
        size_t n = b->rows.n;
        bool *flags = avp_alloc(a, (n ? n : 1) * sizeof *flags);
        frag_frame_flags(b->rows.v, n, flags);
        FragRowIn *rows = avp_alloc(a, (n ? n : 1) * sizeof *rows);
        for (size_t r = 0; r < n; r++) {
            if (flags[r]) {
                rows[r].sep = true;
                continue;
            }
            FragNorm x;
            frag_norm_row(a, &b->rows.v[r], b->address, b->name, &x);
            rows[r].tok[FRAG_STRICT] = x.strict;
            rows[r].tok[FRAG_LOOSE] = x.loose;
            rows[r].regs = avp_memdup(a, x.regs, sizeof x.regs);
            rows[r].nregs = x.nregs;
            rows[r].disp = x.disp;
        }
        i32 fi = frag_corpus_add(c, rows, n);
        c->fns.v[fi].name = avp_cz(c->arena, b->name);
        c->fns.v[fi].address = b->address;
    }
    shsm_asm_free(af);
    return c;
}

static ShsmAsmFile *parse(AvpArena *a, const char *text) {
    return shsm_asm_parse_text(avp_pyfmt(a, ".text\n%s", text), "t.s", "t", 0);
}

AVP_TEST(prologue_epilogue_flags) {
    AvpArena *a = avp_arena();
    AvpBuf t = {0};
    Body b = {0};
    push_n(&b, PROLOGUE, 5);
    avp_vec_push(&b, "li r3, 0x1");
    push_n(&b, EPILOGUE, 6);
    listing(&t, "f", 0x80001000u, &b);
    ShsmAsmFile *af = parse(a, avp_cz(a, avp_buf_str(&t)));
    const ShsmAsmBlock *blk = af->blocks.v[0];
    bool flags[12];
    REQUIRE(blk->rows.n == 12);
    frag_frame_flags(blk->rows.v, 12, flags);
    for (int i = 0; i < 12; i++) CHECK(flags[i] == (i != 5));
    shsm_asm_free(af);
    avp_vec_free(&b);
    avp_buf_free(&t);
}

AVP_TEST(spill_through_r1_is_code) {
    AvpArena *a = avp_arena();
    AvpBuf t = {0};
    Body b = {0};
    avp_vec_push(&b, "stw r3, 0x8(r1)");
    avp_vec_push(&b, "lfs f1, 0xc(r1)");
    avp_vec_push(&b, "stfs f31, 0x10(r1)");
    listing(&t, "f", 0x80001000u, &b);
    ShsmAsmFile *af = parse(a, avp_cz(a, avp_buf_str(&t)));
    bool flags[3];
    frag_frame_flags(af->blocks.v[0]->rows.v, 3, flags);
    CHECK(!flags[0] && !flags[1] && !flags[2]);
    shsm_asm_free(af);
    avp_vec_free(&b);
    avp_buf_free(&t);
}

AVP_TEST(levels_and_branches) {
    AvpArena *a = avp_arena();
    AvpBuf t = {0};
    Body b = {0};
    avp_vec_push(&b, "lwz r5, 0x88(r31)");
    avp_vec_push(&b, "beq .L_80001010");
    avp_vec_push(&b, "lwz r0, lbl_80500000@sda21(r13)");
    avp_vec_push(&b, "bl fn_80206348");
    avp_vec_push(&b, ".L_80001010:");
    avp_vec_push(&b, "b fn_80001000");
    listing(&t, "fn_80001000", 0x80001000u, &b);
    ShsmAsmFile *af = parse(a, avp_cz(a, avp_buf_str(&t)));
    const ShsmAsmBlock *blk = af->blocks.v[0];
    FragNorm x;
    frag_norm_row(a, &blk->rows.v[0], blk->address, blk->name, &x);
    CHECK_STR(x.strict, "lwz %r, 0x88(%r)");
    CHECK_STR(x.loose, "lwz %r, #(%r)");
    CHECK(x.nregs == 2 && x.regs[0] == 5 && x.regs[1] == 31);
    frag_norm_row(a, &blk->rows.v[1], blk->address, blk->name, &x);
    CHECK_STR(x.strict, "beq @+3");
    CHECK_STR(x.loose, "beq @fwd");
    CHECK_STR(x.disp, "beq @+3");
    frag_norm_row(a, &blk->rows.v[2], blk->address, blk->name, &x);
    CHECK_STR(x.strict, "lwz %r, lbl_80500000@sda21");
    CHECK_STR(x.loose, "lwz %r, <sda21>@sda21");
    frag_norm_row(a, &blk->rows.v[3], blk->address, blk->name, &x);
    CHECK_STR(x.loose, "bl fn_80206348");
    frag_norm_row(a, &blk->rows.v[4], blk->address, blk->name, &x);
    CHECK_STR(x.strict, "b @-4");
    CHECK_STR(x.loose, "b @back");
    shsm_asm_free(af);
    avp_vec_free(&b);
    avp_buf_free(&t);
}

AVP_TEST(helper_on_different_registers) {
    AvpArena *a = avp_arena();
    AvpBuf t = {0};
    make_functions(a, &t, REGS);
    FragCorpus *c = corpus_of(a, avp_buf_str(&t));
    FragRepeats r = frag_repeats(c, FRAG_LOOSE, 6, 4);
    REQUIRE(r.n == 1);
    CHECK(r.v[0].length == 8);
    REQUIRE(r.v[0].occ.n == 5);
    for (int i = 0; i < 5; i++) CHECK(r.v[0].occ.v[i].fi == i && r.v[0].occ.v[i].p == 6);
    FragRepeats s = frag_repeats(c, FRAG_STRICT, 6, 4);
    REQUIRE(s.n == 1);
    CHECK(s.v[0].length == 7);
    for (int i = 0; i < 5; i++) CHECK(s.v[0].occ.v[i].fi == i && s.v[0].occ.v[i].p == 7);
    /* render marks the varying offset */
    FragRow rows[5 * 8];
    for (int o = 0; o < 5; o++) frag_corpus_rows(c, r.v[0].occ.v[o].fi, r.v[0].occ.v[o].p, 8, rows + o * 8);
    FragRender rr = frag_render(a, rows, 5, 8);
    REQUIRE(rr.lines.n == 8);
    CHECK_STR(rr.lines.v[0], "lwz rA, <varies: 0x10|0x14|0x18|+2>(rB)");
    CHECK_STR(rr.lines.v[1], "cmpwi rA, 0x0");
    CHECK(rr.cols.n == 1);
    for (int o = 0; o < 5; o++) CHECK_STR(rr.values[o].v[0], OFFS[o]);
    frag_render_free(&rr, 5);
    frag_repeats_free(&r);
    frag_repeats_free(&s);
    frag_corpus_free(c);
    avp_buf_free(&t);
}

AVP_TEST(prologue_not_a_fragment) {
    AvpArena *a = avp_arena();
    AvpBuf t = {0};
    for (int i = 0; i < 6; i++) {
        Body b = {0};
        push_n(&b, PROLOGUE, 5);
        filler(a, &b, i);
        push_n(&b, EPILOGUE, 6);
        listing(&t, avp_sprintf(a, "g_%d", i), 0x80002000u + 0x100u * (u32)i, &b);
        avp_vec_free(&b);
    }
    FragCorpus *c = corpus_of(a, avp_buf_str(&t));
    FragRepeats r = frag_repeats(c, FRAG_LOOSE, 3, 4);
    CHECK(r.n == 0);
    frag_repeats_free(&r);
    frag_corpus_free(c);
    avp_buf_free(&t);
}

AVP_TEST(inconsistent_renaming_splits) {
    AvpArena *a = avp_arena();
    static const char *const regs[][3] = {{"r3", "r4", "r5"}, {"r6", "r7", "r8"}, {"r9", "r10", "r11"},
                                          {"r0", "r12", "r31"}, {"r5", "r4", "r5"}};
    AvpBuf t = {0};
    make_functions(a, &t, regs);
    FragCorpus *c = corpus_of(a, avp_buf_str(&t));
    FragRepeats r = frag_repeats(c, FRAG_LOOSE, 6, 4);
    REQUIRE(r.n == 1);
    CHECK(r.v[0].length == 8);
    REQUIRE(r.v[0].occ.n == 4);
    for (int i = 0; i < 4; i++) CHECK(r.v[0].occ.v[i].fi == i);
    frag_repeats_free(&r);
    frag_corpus_free(c);
    avp_buf_free(&t);
}

static FragRepeat rep(i32 length, const i32 (*occ)[2], size_t n) {
    FragRepeat r = {length, {0}};
    for (size_t i = 0; i < n; i++) avp_vec_push(&r.occ, ((FragPos){occ[i][0], occ[i][1]}));
    return r;
}

AVP_TEST(drop_contained) {
    static const i32 big[][2] = {{0, 0}, {1, 0}, {2, 0}}, inner[][2] = {{0, 2}, {1, 2}, {2, 2}},
                     other[][2] = {{0, 2}, {1, 2}, {3, 0}};
    FragRepeats in = {0};
    avp_vec_push(&in, rep(10, big, 3));
    avp_vec_push(&in, rep(6, inner, 3));
    avp_vec_push(&in, rep(6, other, 3));
    FragRepeats k = frag_drop_contained(in);
    REQUIRE(k.n == 2);
    CHECK(k.v[0].length == 10 && k.v[0].occ.v[0].p == 0);
    CHECK(k.v[1].length == 6 && k.v[1].occ.v[2].fi == 3);
    frag_repeats_free(&k);
}

AVP_TEST(switch_chain_is_boilerplate) {
    AvpArena *a = avp_arena();
    AvpBuf t = {0};
    for (int i = 0; i < 5; i++) {
        Body b = {0};
        filler(a, &b, i);
        for (int k = 0; k < 4; k++) {
            avp_vec_push(&b, avp_sprintf(a, "cmpwi r3, 0x%x", k));
            avp_vec_push(&b, "beq .L_80003100");
        }
        filler(a, &b, i + 5);
        listing(&t, avp_sprintf(a, "s_%d", i), 0x80003000u + 0x200u * (u32)i, &b);
        avp_vec_free(&b);
    }
    FragCorpus *c = corpus_of(a, avp_buf_str(&t));
    FragRepeats r = frag_repeats(c, FRAG_LOOSE, 6, 4);
    CHECK(r.n > 0);
    for (size_t i = 0; i < r.n; i++) CHECK(frag_boilerplate(c, FRAG_LOOSE, &r.v[i]) != NULL);
    for (size_t i = 0; i < r.n; i++) CHECK(strcmp(frag_boilerplate(c, FRAG_LOOSE, &r.v[i]), "switch") == 0);
    frag_repeats_free(&r);
    frag_corpus_free(c);
    avp_buf_free(&t);
}

static bool single_call(const char *const *t, size_t n) {
    AvpStr v[16];
    for (size_t i = 0; i < n; i++) v[i] = avp_str(t[i]);
    return frag_single_call(v, n);
}

AVP_TEST(single_call_and_trim) {
    static const char *const a1[] = {"ApplyShared", "(", "$", ",", "1", ")", ";"};
    static const char *const a2[] = {"return", "a", "->", "Get", "(", ")", ";"};
    static const char *const a3[] = {"$", "->", "n", "=", "$", "->", "n", "+", "1", ";"};
    CHECK(single_call(a1, 7));
    CHECK(single_call(a2, 7));
    CHECK(!single_call(a3, 10));
    /* "x ; $ -> n = 1 ; y (": whole statements from after the first ';' */
    static const char *const t[] = {"x", ";", "$", "->", "n", "=", "1", ";", "y", "("};
    AvpStr v[10];
    for (int i = 0; i < 10; i++) v[i] = avp_str(t[i]);
    i64 s, e;
    const char *kind;
    REQUIRE(frag_trim(v, 10, false, &s, &e, &kind));
    CHECK(s == 2 && e == 8 && strcmp(kind, "statements") == 0);
}

AVP_TEST(compact_and_known_table) {
    AvpArena *a = avp_arena();
    AvpStrVec p = {0};
    avp_vec_push(&p, AVP_S("lwz rA, 0x0(rB)"));
    avp_vec_push(&p, AVP_S("cmpwi rA, 0x0"));
    CHECK_STR(frag_compact(a, &p, 150), "lwz rA, 0x0(rB); cmpwi rA, 0x0");
    CHECK_STR(frag_compact(a, &p, 10), "lwz rA,...");
    avp_vec_free(&p);
    CHECK(strcmp(FRAGMENTS_KNOWN[0].name, "string hash") == 0 && FRAGMENTS_KNOWN[0].stated == -1);
    CHECK(strcmp(FRAGMENTS_KNOWN[1].name, "display-height pair") == 0 && FRAGMENTS_KNOWN[1].stated == 76);
    CHECK(strcmp(FRAGMENTS_KNOWN[2].name, "save-data key check") == 0 && FRAGMENTS_KNOWN[2].stated == 5);
    CHECK(frag_lib_matches(a, "game/G3", "G3") && frag_lib_matches(a, "game/G3", "Game/") &&
          frag_lib_matches(a, NULL, "unknown") && !frag_lib_matches(a, "sdk/os", "game"));
}

AVP_TEST(in_function_reads_the_database_as_it_stands) {
    AvpArena *a = avp_arena();
    FragmentsInFunction out;
    bool have = avp_isfile("build/index/fragments.sqlite");
    CHECK(fragments_in_function(a, NULL, 0x800C62B0, 0, NULL, &out) == have);
    for (size_t i = 0; i < out.n; i++) {
        CHECK(out.v[i].offset >= 0 && out.v[i].length > 0 && out.v[i].pattern.n == (size_t)out.v[i].length);
        avp_vec_free(&out.v[i].pattern);
    }
    avp_vec_free(&out);
}

AVP_TEST_MAIN("fragments")
