/* Tests for eq_terms: tools/test_eqcheck.py TestTerms (test_linear_and_masks, test_relocation_halves_fold), plus a
 * sequence of constructions over every simplifier rule whose show() text, node id, table size, nz and flags are
 * compared line by line with what tools/eqcheck.py prints on the same sequence (terms_expected.h). */
#include "avp/test.h"
#include "eq_live.h"
#include "eq_terms.h"
#include "terms_expected.h"

#include <stdio.h>

AVP_TEST(test_linear_and_masks) {
    EqTerms *t = eq_terms_new(avp_arena());
    EqNode *x = eq_entry(t, "r3"), *y = eq_entry(t, "r4");
    CHECK(eq_add(t, x, y) == eq_add(t, y, x));
    EqNode *c5 = eq_c(t, 5);
    EqNode *s = eq_add(t, x, c5);
    CHECK(eq_sub(t, s, eq_c(t, 5)) == x);
    EqNode *m4 = eq_mulc(t, x, 4);
    CHECK(m4 == eq_rot_mask(t, x, 2, eq_mask_of(0, 29))); /* slwi */
    EqNode *s8 = eq_shr(t, x, 8);
    CHECK(s8 == eq_rot_mask(t, x, 24, eq_mask_of(8, 31))); /* srwi */
    EqNode *b = eq_and_mask(t, x, 0xFF);
    CHECK(eq_and_mask(t, b, 0xFF) == b);
    CHECK(eq_and_mask(t, eq_sext(t, b, 8), 0xFF) == b); /* clrlwi of extsb */
    EqNode *e1 = eq_eq(t, x, y);
    CHECK(e1 == eq_eq(t, y, x));
    eq_terms_free(t);
}

AVP_TEST(test_relocation_halves_fold) {
    EqTerms *t = eq_terms_new(avp_arena());
    EqNode *s = eq_sym(t, "gThing", 0);
    EqNode *h = eq_node1(t, EQ_OP_HA, s, EQ_MASK);
    EqNode *ha = eq_mulc(t, h, 0x10000);
    EqNode *lo = eq_node1(t, EQ_OP_LOS, s, EQ_MASK);
    CHECK(eq_add(t, ha, lo) == s);
    eq_terms_free(t);
}

static EqTerms *g_t;
static size_t g_line;

static void r(const char *label, EqNode *n, int depth) {
    AvpArena *a = avp_arena();
    char *line = avp_sprintf(a, "%s|%s|%d|%zu|%u|%d%d%d", label, eq_show(a, n, depth), n->id, eq_terms_len(g_t),
                             n->nz, n->glob, n->local, n->sp);
    size_t nexp = sizeof TERMS_EXPECTED / sizeof *TERMS_EXPECTED;
    CHECK_MSG(g_line < nexp && strcmp(line, TERMS_EXPECTED[g_line]) == 0, "line %zu: got %s, want %s", g_line, line,
              g_line < nexp ? TERMS_EXPECTED[g_line] : "(end)");
    g_line++;
}
#define R(label, n) r(label, (n), EQ_SHOW_DEPTH)

static void pair_line(const char *label, EqArg p, EqArg q) {
    AvpArena *a = avp_arena();
    char *line = avp_sprintf(a, "%s|%s|%s", label, eq_show_arg(a, p, EQ_SHOW_DEPTH), eq_show_arg(a, q, EQ_SHOW_DEPTH));
    size_t nexp = sizeof TERMS_EXPECTED / sizeof *TERMS_EXPECTED;
    CHECK_MSG(g_line < nexp && strcmp(line, TERMS_EXPECTED[g_line]) == 0, "line %zu: got %s, want %s", g_line, line,
              g_line < nexp ? TERMS_EXPECTED[g_line] : "(end)");
    g_line++;
}

static EqNode *leaf_s(EqTerms *t, const char *op, const char *s) {
    EqArg a = eq_as(s);
    return eq_leaf_d(t, eq_op_id(op), &a, 1);
}

AVP_TEST(constructions_match_python) {
    EqTerms *t = eq_terms_new(avp_arena());
    g_t = t;
    g_line = 0;
    EqNode *x = eq_entry(t, "r3"), *y = eq_entry(t, "r4"), *z = eq_entry(t, "r5");
    EqNode *cr = eq_entry(t, "cr2");
    R("x", x);
    R("cr", cr);
    R("sp0", t->sp0);
    R("add", eq_add(t, x, y));
    R("add2", eq_add(t, y, x));
    {
        EqNode *c = eq_c(t, 5);
        EqNode *s = eq_add(t, x, c);
        EqNode *c2 = eq_c(t, 5);
        R("sub", eq_sub(t, s, c2));
    }
    {
        EqNode *c = eq_c(t, 2);
        EqTerm ts[3] = {{x, 3}, {y, -1}, {c, 4}};
        R("lin3", eq_lin(t, 7, ts, 3));
        EqTerm ts2[1] = {{x, -4}};
        R("linneg", eq_lin(t, -0x10, ts2, 1));
    }
    R("mulc4", eq_mulc(t, x, 4));
    {
        EqNode *c = eq_c(t, 8);
        EqNode *s = eq_add(t, t->sp0, c);
        R("mulc_sp", eq_mulc(t, s, 2));
        EqNode *c2 = eq_c(t, -0x20);
        R("sp_off", eq_add(t, t->sp0, c2));
    }
    R("rot_slwi", eq_rot_mask(t, x, 2, eq_mask_of(0, 29)));
    R("rot_srwi", eq_rot_mask(t, x, 24, eq_mask_of(8, 31)));
    R("rot_gen", eq_rot_mask(t, x, 5, eq_mask_of(20, 3)));
    {
        EqNode *c = eq_c(t, 0x12345678);
        R("rot_c", eq_rot_mask(t, c, 8, eq_mask_of(4, 27)));
    }
    EqNode *b = eq_and_mask(t, x, 0xFF);
    R("andm", b);
    R("andm2", eq_and_mask(t, b, 0xFF));
    R("sextb", eq_sext(t, b, 8));
    {
        EqNode *s = eq_sext(t, b, 8);
        R("clr_sext", eq_and_mask(t, s, 0xFF));
    }
    R("sext16", eq_sext(t, x, 16));
    {
        EqNode *c = eq_c(t, 0x8001);
        R("sext16c", eq_sext(t, c, 16));
        EqNode *s = eq_sext(t, x, 8);
        R("sext_s", eq_sext(t, s, 16));
    }
    {
        EqNode *c = eq_c(t, 0xF0F0);
        EqNode *m = eq_and_mask(t, z, 0xFF00);
        EqNode *xs[4] = {x, y, c, m};
        R("and3", eq_andv(t, xs, 4));
        EqNode *c2 = eq_c(t, 0xFFFF);
        EqNode *xs2[3] = {x, x, c2};
        R("and_dup", eq_andv(t, xs2, 3));
    }
    {
        EqNode *c = eq_c(t, 0x10);
        EqNode *c1 = eq_c(t, 1);
        EqNode *o = eq_or(t, z, c1);
        EqNode *xs[4] = {x, y, c, o};
        R("or", eq_orv(t, xs, 4));
        EqNode *cm = eq_c(t, -1);
        R("or_mask", eq_or(t, x, cm));
    }
    {
        EqNode *c3 = eq_c(t, 3);
        EqNode *xs[5] = {x, y, x, c3, z};
        R("xor", eq_xorv(t, xs, 5));
        R("xor_self", eq_xor(t, x, x));
        R("not", eq_not(t, x));
        EqNode *n = eq_not(t, x);
        R("notnot", eq_not(t, n));
    }
    R("shr", eq_shr(t, x, 4));
    {
        EqNode *s = eq_shr(t, x, 4);
        R("shrshr", eq_shr(t, s, 3));
        R("shr_b", eq_shr(t, b, 9));
        EqNode *c = eq_c(t, 0x80000000);
        R("shr_c", eq_shr(t, c, 31));
    }
    R("sra", eq_sra(t, x, 3));
    {
        EqNode *c = eq_c(t, 0x80000010);
        R("sra_c", eq_sra(t, c, 4));
        R("sra_pos", eq_sra(t, b, 2));
    }
    R("mul", eq_mul(t, y, x));
    {
        EqNode *c = eq_c(t, 3);
        R("mul_c", eq_mul(t, c, x));
    }
    R("eq", eq_eq(t, x, y));
    R("eq2", eq_eq(t, y, x));
    {
        EqNode *c = eq_c(t, 3);
        EqNode *c2 = eq_c(t, 3);
        R("eqc", eq_eq(t, c, c2));
        EqNode *z0 = eq_c(t, 0);
        R("eqz", eq_eq(t, x, z0));
    }
    R("lts", eq_lt(t, true, x, y));
    {
        EqNode *z0 = eq_c(t, 0);
        R("ltu0", eq_lt(t, false, x, z0));
        EqNode *z1 = eq_c(t, 0);
        R("ltu0b", eq_lt(t, false, z1, x));
        EqNode *a = eq_c(t, 0xFFFFFFFF), *c = eq_c(t, 1);
        R("ltc", eq_lt(t, true, a, c));
        EqNode *a2 = eq_c(t, 0xFFFFFFFF), *c2 = eq_c(t, 1);
        R("ltcu", eq_lt(t, false, a2, c2));
    }
    EqNode *e1 = eq_eq(t, x, y), *e2 = eq_lt(t, true, x, z);
    {
        EqNode *xs[3] = {e1, e2, t->T};
        R("band", eq_bandv(t, xs, 3));
        EqNode *n = eq_bnot(t, e1);
        R("band_contra", eq_band(t, e1, n));
        EqNode *ys[3] = {e2, e1, t->F};
        R("bor", eq_borv(t, ys, 3));
        R("bor_t", eq_bor(t, e2, t->T));
        EqNode *bb = eq_band(t, e1, e2);
        R("bandnest", eq_band(t, bb, cr));
    }
    {
        EqNode *n = eq_bnot(t, e1);
        R("bxor", eq_bxor(t, n, e2));
        EqNode *n1 = eq_bnot(t, e1);
        EqNode *n2 = eq_bnot(t, n1);
        R("bxor2", eq_bxor(t, e2, n2));
        R("bxorT", eq_bxor(t, t->T, e1));
    }
    R("b2i", eq_b2i(t, e1));
    R("bitof", eq_bit_of(t, x, 3));
    {
        EqNode *c = eq_c(t, 0x10000000);
        R("bitofc", eq_bit_of(t, c, 3));
    }
    EqNode *crs[32];
    for (int k = 0; k < 32; k++) {
        char nm[8];
        snprintf(nm, sizeof nm, "cr%d", k);
        crs[k] = eq_entry(t, nm);
    }
    EqNode *crw = eq_node_n(t, EQ_OP_CRW, crs, 32, EQ_MASK);
    R("crw", crw);
    R("rot_crw", eq_rot_mask(t, crw, 4, eq_mask_of(28, 31)));
    EqNode *s = eq_sym(t, "gThing", 0);
    R("sym", s);
    R("symoff", eq_sym(t, "gThing", 0x10));
    EqNode *hn = eq_node1(t, EQ_OP_HA, s, EQ_MASK);
    EqNode *ha = eq_mulc(t, hn, 0x10000);
    R("ha", ha);
    {
        EqNode *lo = eq_node1(t, EQ_OP_LOS, s, EQ_MASK);
        R("fold", eq_add(t, ha, lo));
    }
    {
        EqNode *s2 = eq_sym(t, "gOther", 0x24);
        EqNode *h2 = eq_node1(t, EQ_OP_HA, s2, EQ_MASK);
        EqNode *m2 = eq_mulc(t, h2, 0x10000);
        EqNode *l2 = eq_node1(t, EQ_OP_LOS, s2, EQ_MASK);
        R("fold2", eq_add3(t, m2, l2, x));
    }
    {
        EqNode *hh = eq_node1(t, EQ_OP_HI, s, EQ_MASK);
        EqNode *hi = eq_mulc(t, hh, 0x10000);
        EqNode *lou = eq_node1(t, EQ_OP_LOU, s, EQ_MASK);
        R("hilo", eq_or(t, hi, lou));
    }
    {
        EqNode *c8 = eq_c(t, 8);
        EqNode *ad = eq_add(t, x, c8);
        EqNode *mem = leaf_s(t, "E", "mem");
        EqArg args[3] = {eq_ai(4), eq_an(ad), eq_an(mem)};
        R("ld", eq_node(t, EQ_OP_LD, args, 3, EQ_MASK));
    }
    {
        EqArg nocr6 = {0};
        EqNode *nc = eq_leaf_d(t, EQ_OP_NOCR6, &nocr6, 0);
        EqArg args[4] = {eq_as("r3"), eq_an(x), eq_as("cr1[eq]"), eq_an(nc)};
        R("args", eq_node(t, EQ_OP_ARGS, args, 4, EQ_MASK));
    }
    {
        EqArg a[3] = {eq_ai(3), eq_as("p"), eq_as("r31")};
        R("bv", eq_leaf(t, EQ_OP_BV, a, 3, false, false, false, EQ_MASK));
        R("u", eq_unknown(t, "retail", "lr@call1"));
        EqArg c[2] = {eq_as("@123"), eq_ai(-8)};
        R("csym", eq_leaf(t, EQ_OP_CSYM, c, 2, true, true, false, EQ_MASK));
        EqArg fr[2] = {eq_ai(2), eq_as("frame")};
        EqNode *frame = eq_leaf(t, EQ_OP_BV, fr, 2, false, false, false, EQ_MASK);
        EqArg fa[3] = {eq_an(frame), eq_ai(-0x18), eq_ai(4)};
        R("fa", eq_node(t, EQ_OP_FA, fa, 3, EQ_MASK));
    }
    {
        EqNode *m = eq_mul(t, x, y);
        m = eq_mul(t, m, z);
        m = eq_mul(t, m, x);
        m = eq_mul(t, m, y);
        m = eq_mul(t, m, z);
        EqNode *c1 = eq_c(t, 1);
        r("deep", eq_add(t, m, c1), 3);
    }
    {
        EqNode *ab[2] = {y, x};
        R("op_comm", eq_op(t, eq_op_id("fadds"), ab, 2, true, EQ_MASK));
        R("op", eq_op(t, eq_op_id("fsubs"), ab, 2, false, EQ_MASK));
    }
    {
        EqArg one = eq_ai(1), tr = eq_ab(true);
        R("boolint", eq_leaf_d(t, EQ_OP_BOOL, &one, 1));
        R("intleaf", eq_leaf_d(t, eq_op_id("k"), &one, 1));
        R("boolleaf2", eq_leaf_d(t, eq_op_id("k"), &tr, 1));
        EqArg ex[4] = {eq_an(x), eq_ai(8), eq_ai(2), eq_ai(2)};
        R("ext", eq_node(t, EQ_OP_EXT, ex, 4, 0xFFFF));
    }
    {
        EqTerm ts[2] = {{x, 0x7FFFFFFF}, {y, 0x80000000ll}};
        R("big", eq_lin(t, 0, ts, 2));
        R("pow2", eq_mulc(t, b, 0x100));
    }
    /* part 2: allocation-order probes */
    {
        EqNode *h2 = eq_node1(t, EQ_OP_HA, x, EQ_MASK);
        EqNode *m = eq_mulc(t, h2, 2);
        R("ha_nofold", eq_add(t, m, y));
        EqNode *hy = eq_node1(t, EQ_OP_HA, y, EQ_MASK);
        EqNode *ly = eq_node1(t, EQ_OP_LOS, y, EQ_MASK);
        EqNode *hz = eq_node1(t, EQ_OP_HA, z, EQ_MASK);
        EqTerm ts[4] = {{hy, 0x10000}, {ly, 1}, {hz, 0x10000}, {z, 3}};
        R("ha_two", eq_lin(t, 0, ts, 4));
        EqNode *c18 = eq_c(t, 0x18);
        EqNode *ad = eq_add(t, x, c18);
        EqNode *hd = eq_node1(t, EQ_OP_HA, ad, EQ_MASK);
        EqNode *c18b = eq_c(t, 0x18);
        EqNode *ad2 = eq_add(t, x, c18b);
        EqNode *lo = eq_node1(t, EQ_OP_LOS, ad2, EQ_MASK);
        EqTerm ts2[3] = {{hd, 0x10000}, {lo, 1}, {x, -1}};
        R("ha_lin", eq_lin(t, 4, ts2, 3));
        EqTerm ts3[5] = {{z, 1}, {x, 1}, {z, -1}, {y, 2}, {z, 5}};
        R("zero_keep", eq_lin(t, 0, ts3, 5));
    }
    {
        EqNode *xy = eq_and(t, x, y);
        R("andm_and", eq_and_mask(t, xy, 0xFF00));
        EqNode *c1 = eq_c(t, 0xF0), *c2 = eq_c(t, 0x3C);
        R("and_c_only", eq_and(t, c1, c2));
        EqNode *c0 = eq_c(t, 0);
        R("and_zero", eq_and(t, x, c0));
        EqNode *o = eq_or(t, x, y);
        EqNode *f1 = eq_c(t, 5), *f2 = eq_c(t, 5);
        EqNode *xs[3] = {o, f1, f2};
        R("xor_or", eq_xorv(t, xs, 3));
        EqNode *k1 = eq_c(t, 1);
        EqNode *in[3] = {x, y, k1};
        EqNode *inner = eq_xorv(t, in, 3);
        EqNode *k2 = eq_c(t, 1);
        EqNode *out[3] = {inner, y, k2};
        R("xor_nest", eq_xorv(t, out, 3));
        R("or_dup", eq_or(t, x, x));
    }
    {
        EqNode *c = eq_c(t, 0xFFFFFF80);
        R("sext_neg", eq_sext(t, c, 8));
        EqNode *s28 = eq_shr(t, x, 28);
        R("sext_small", eq_sext(t, s28, 8));
        EqNode *c8 = eq_c(t, 0x80000000);
        R("sra_big", eq_sra(t, c8, 40));
        EqNode *s16 = eq_sext(t, x, 16);
        R("sra_sext", eq_sra(t, s16, 4));
        R("rot_low", eq_rot_mask(t, x, 4, 0xF));
        EqNode *cf = eq_c(t, 0xF0000001);
        R("rot_lowc", eq_rot_mask(t, cf, 4, 0xF));
        R("rot_crw0", eq_rot_mask(t, crw, 0, 0xF0000000));
        R("lt_same", eq_lt(t, true, x, x));
    }
    {
        EqNode *n2 = eq_bnot(t, e2);
        EqNode *xs[3] = {e2, n2, e1};
        R("bor_contra", eq_borv(t, xs, 3));
        EqNode *b1 = eq_band(t, e1, cr);
        EqNode *b2 = eq_band(t, e2, cr);
        R("band_flat", eq_band(t, b1, b2));
        R("bxor_same", eq_bxor(t, e1, e1));
        R("bxor_F", eq_bxor(t, e1, t->F));
        EqNode *c0 = eq_c(t, 0);
        R("eq_neg", eq_eq(t, c0, x));
        EqNode *sa = eq_sym(t, "a", 0);
        EqNode *sb = eq_sym(t, "b", 0);
        R("eq_sym", eq_eq(t, sa, sb));
        EqArg u[2] = {eq_as("retail"), eq_as("lr@call1")};
        R("leaf_flags", eq_leaf(t, EQ_OP_U, u, 2, true, false, false, EQ_MASK));
    }
    {
        R("mulc0", eq_mulc(t, x, 0));
        EqNode *c3 = eq_c(t, 3);
        EqNode *a3 = eq_add(t, x, c3);
        R("mulc_neg", eq_mulc(t, a3, -1));
        R("shl0", eq_shl(t, x, 32));
        R("shl31", eq_shl(t, x, 31));
        R("shr_neg", eq_shr(t, x, -1));
        EqNode *s24 = eq_shr(t, x, 24);
        R("nz_pow2", eq_mulc(t, s24, 0x10));
        R("nz_sp", eq_shl(t, t->sp0, 3));
        int cz = eq_op_id("cntlzw");
        R("op_nz", eq_op(t, cz, &x, 1, false, 0x3F));
        EqNode *cnt = eq_op(t, cz, &x, 1, false, 0x3F);
        R("cntshr", eq_shr(t, cnt, 5));
    }
    {
        EqNode *m1 = eq_mul(t, x, y);
        EqNode *c4 = eq_c(t, 4);
        EqNode *a1 = eq_add(t, m1, c4);
        EqNode *m2 = eq_mul(t, x, z);
        EqNode *c4b = eq_c(t, 4);
        EqNode *a2 = eq_add(t, m2, c4b);
        EqNode *mem1 = leaf_s(t, "E", "mem");
        EqArg l1[3] = {eq_ai(4), eq_an(a1), eq_an(mem1)};
        EqNode *ld1 = eq_node(t, EQ_OP_LD, l1, 3, EQ_MASK);
        EqNode *mem2 = leaf_s(t, "E", "mem");
        EqArg l2[3] = {eq_ai(4), eq_an(a2), eq_an(mem2)};
        EqNode *ld2 = eq_node(t, EQ_OP_LD, l2, 3, EQ_MASK);
        EqArgPair *path;
        size_t np = eq_diff_path(avp_arena(), eq_an(ld1), eq_an(ld2), &path);
        for (size_t i = 0; i < np; i++) {
            char lab[16];
            snprintf(lab, sizeof lab, "dp%zu", i);
            pair_line(lab, path[i].a, path[i].b);
        }
        EqArgPair fd = eq_first_diff(eq_an(a1), eq_an(a2));
        pair_line("fd", fd.a, fd.b);
    }
    CHECK_EQ_I64((i64)g_line, (i64)(sizeof TERMS_EXPECTED / sizeof *TERMS_EXPECTED));
    eq_terms_free(t);
}

AVP_TEST(show_values) {
    AvpArena *a = avp_arena();
    CHECK_CZ(eq_show_arg(a, eq_ai(-24), 6), "0x-18");
    CHECK_CZ(eq_show_arg(a, eq_ai(-9), 6), "-9");
    CHECK_CZ(eq_show_arg(a, eq_ai(10), 6), "0xA");
    CHECK_CZ(eq_show_arg(a, eq_ab(true), 6), "True");
    CHECK_CZ(eq_show_arg(a, eq_as("r3"), 6), "r3");
    CHECK_CZ(eq_str_arg(a, eq_ai(-24)), "-24");
}

AVP_TEST(constants_and_masks) {
    CHECK_EQ_I64(eq_mask_of(0, 31), 0xFFFFFFFF);
    CHECK_EQ_I64(eq_mask_of(16, 31), 0xFFFF);
    CHECK_EQ_I64(eq_mask_of(31, 0), 0x80000001);
    CHECK_EQ_I64(eq_wmask(1), 0xFF);
    CHECK_EQ_I64(eq_wmask(2), 0xFFFF);
    CHECK_EQ_I64(eq_wmask(8), 0xFFFFFFFF);
    CHECK_EQ_I64(eq_s16(0x8000), -0x8000);
    CHECK_EQ_I64(eq_s32(0xFFFFFFFF), -1);
    CHECK_EQ_I64(eq_s32(-1), -1);
    CHECK_CZ(EQ_CALLEE_SAVED[EQ_NCALLEE_SAVED - 1], "lr");
    CHECK_CZ(EQ_REG_LOCS[96], "ca");
    CHECK_CZ(EQ_VOLATILE[30], "cr20");
    CHECK(eq_is_single_op(eq_op_id("fadds")));
    CHECK(eq_is_single_op(EQ_OP_PSQ1));
    CHECK(!eq_is_single_op(EQ_OP_PSQ));
    CHECK(eq_op_id("c") == EQ_OP_C);
    CHECK(eq_op_id("rotlv") == EQ_OP_ROTLV);
    int id = eq_op_id("divwu");
    CHECK(id >= EQ_OP__KNOWN && id == eq_op_id("divwu"));
    CHECK_CZ(eq_op_name(id), "divwu");
}

AVP_TEST_MAIN("eqcheck_terms")
