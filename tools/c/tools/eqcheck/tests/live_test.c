/* Tests for eq_live: CalleeLiveness.analyse on hand-written listings (no index: the Python's ix = False), the
 * _BRANCH and _HELPER patterns, _rpo, and CalleeLiveness() on a sample of real functions (real tree: skipped when
 * build/index/asm.sqlite is absent). Expected lines (live_expected.h) are printed by tools/eqcheck.py on the same
 * inputs, with one CalleeLiveness queried in the same order. tools/test_eqcheck.py has no direct liveness test. */
#include "avp/test.h"
#include "eq_live.h"
#include "shsm_asmindex.h"
#include "live_expected.h"

#include <stdio.h>
#include "../../wall/tests/wall_testrows.h"

static const char *const LISTINGS[] = {
    "addi r3, r3, 0x1\nblr",
    "add r3, r4, r5\nblr",
    "mflr r0\nstw r0, 0x4(r1)\nbl foo\nlwz r0, 0x4(r1)\nmtlr r0\nblr",
    "bl _savegpr_25 @_savegpr_25\naddi r11, r1, 0x20\nblr",
    "li r5, 0x0\nlwzx r6, r3, r5\naddi r5, r5, 0x4\ncmpw r5, r4\nblt .1\nmr r3, r6\nblr",
    "stmw r27, 0xc(r1)\nlmw r27, 0xc(r1)\nblr",
    "lwzu r3, 0x4(r4)\nstwu r1, -0x10(r1)\nblr",
    "li r3, 0x0\nb foo @foo",
    "li r3, 0x0\nbctr",
    "fmr f1, f2\nfcmpu cr0, f1, f2\nblr",
    "cmpwi r3, 0x0\nbeqlr\nli r3, 0x0\nblr",
    "rlwimi r3, r4, 0x0, 16, 31\nblr",
    "bl\nbctrl\nblrl\nmr r3, r12\nblr",
    "li r3, 0x1\nb .0 @f",
    "mr r4, r3\nbdnzf lt, .0\nblr",
    "li r0, 0x1\nsc\nblr",
    "blr\nli r3, 0x0\nblr",
    "mr. r3, r4\nbne .5\nli r3, 0x0\nb .6\nli r4, 0x1\naddi r3, r4, 0x2\nblr",
    "stw r3, 0x8(r1)\nlfs f0, 0x0(r3)\nfadds f1, f0, f2\nps_merge00 f3, f4, f5\npsq_l f6, 0x0(r3), 0, qr0\nblr",
    "lmw r3, 0x0(r1)\nblr",
    "lbzux r5, r6, r7\nstbux r5, r6, r8\nnot r9, r10\nextsh r11, r12\nblr",
    "",
};
static const char *const BRANCHES[] = {"b",      "bl",    "blr",   "blrl",   "bctr",    "bctrl", "beq",
                                       "bnel",   "bnllr", "bnlr",  "bdnz",   "bdz",     "bdnzf", "bdnzlr",
                                       "beqctr", "bltl",  "bns",   "bun",    "bnu",     "bgectrl", "bc",
                                       "bclr",   "bx",    "blelr", "bdzne"};
static const char *const HELPERS[] = {"_savegpr_25", "_restfpr_14", "_savegpr_", "_savegpr_25x",
                                      "__savegpr_25", "_savegprx_1", "_restgpr_031"};

static size_t g_line;

static void expect(const char *line) {
    size_t nexp = sizeof LIVE_EXPECTED / sizeof *LIVE_EXPECTED;
    CHECK_MSG(g_line < nexp && strcmp(line, LIVE_EXPECTED[g_line]) == 0, "line %zu: got %s, want %s", g_line, line,
              g_line < nexp ? LIVE_EXPECTED[g_line] : "(end)");
    g_line++;
}

/* rows_of() of the Python script: "mn ops[ @sym]" per line; a last operand ".N" is a label at row N's address. */
static size_t rows_of(AvpArena *a, const char *text, ShsmAsmRow **out) {
    AvpStrVec lines = avp_str_split(avp_str(text), AVP_S("\n"), -1);
    ShsmAsmRow *rows = AVP_NEWN(a, ShsmAsmRow, lines.n + 1);
    size_t n = 0;
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr line = lines.v[i];
        if (!line.n) continue;
        ShsmAsmRow *r = &rows[n];
        r->address = 0x80001000 + 4 * (i64)n;
        r->word = SHSM_NONE;
        r->label = SHSM_NONE;
        r->sym = (AvpStr){NULL, 0};
        i64 at = avp_str_find(line, AVP_S(" @"), 0, AVP_END);
        if (at >= 0) {
            r->sym = avp_str_slice(line, at + 2, AVP_END);
            line = avp_str_slice(line, 0, at);
        }
        i64 sp = avp_str_find(line, AVP_S(" "), 0, AVP_END);
        r->mnemonic = sp >= 0 ? avp_str_slice(line, 0, sp) : line;
        r->operands = sp >= 0 ? avp_str_slice(line, sp + 1, AVP_END) : AVP_S("");
        i64 comma = avp_str_rfind(r->operands, AVP_S(","), 0, AVP_END);
        AvpStr last = avp_str_strip(comma >= 0 ? avp_str_slice(r->operands, comma + 1, AVP_END) : r->operands);
        if (avp_str_startswith(last, AVP_S("."))) {
            i64 k = 0;
            bool ovf;
            avp_parse_int(avp_str_slice(last, 1, AVP_END), 10, &k, &ovf);
            r->label = 0x80001000 + 4 * k;
        }
        n++;
    }
    *out = rows;
    return n;
}

AVP_TEST(analyse_branch_helper_real) {
    AvpArena *a = avp_arena();
    g_line = 0;
    EqLive *lv = eq_live_new(NULL, 4);
    eq_live_disable_index(lv);
    for (size_t i = 0; i < sizeof LISTINGS / sizeof *LISTINGS; i++) {
        ShsmAsmRow *rows;
        size_t n = rows_of(a, LISTINGS[i], &rows);
        u32 live = 0, writes = 0;
        eq_live_analyse(lv, rows, n, 0, &live, &writes);
        expect(avp_sprintf(a, "%07X %07X", live, writes));
    }
    eq_live_free(lv);
    for (size_t i = 0; i < sizeof BRANCHES / sizeof *BRANCHES; i++) {
        EqBranch b;
        if (!eq_branch_match(avp_str(BRANCHES[i]), &b)) {
            expect(avp_sprintf(a, "%s:-", BRANCHES[i]));
            continue;
        }
        expect(avp_sprintf(a, "%s:%.*s,%.*s,%.*s,%.*s", BRANCHES[i], AVP_SF(b.ctr), AVP_SF(b.cc), AVP_SF(b.lr),
                           AVP_SF(b.link)));
    }
    for (size_t i = 0; i < sizeof HELPERS / sizeof *HELPERS; i++) {
        AvpStr kind, cls;
        i64 num;
        if (!eq_helper_match(avp_str(HELPERS[i]), &kind, &cls, &num)) {
            expect(avp_sprintf(a, "%s:-", HELPERS[i]));
            continue;
        }
        /* the Python prints the digit group as text: it is the name after its last '_' */
        AvpStr name = avp_str(HELPERS[i]);
        AvpStr digits = avp_str_slice(name, avp_str_rfind(name, AVP_S("_"), 0, AVP_END) + 1, AVP_END);
        expect(avp_sprintf(a, "%s:%.*s,%.*s,%.*s", HELPERS[i], AVP_SF(kind), AVP_SF(cls), AVP_SF(digits)));
        i64 want = 0;
        bool ovf;
        avp_parse_int(digits, 10, &want, &ovf);
        CHECK_EQ_I64(num, want);
    }
    expect(avp_sprintf(a, "STD %07X ALL %07X", EQ_STD_ARGS, EQ_ALL_ARGS));
    if (!avp_isfile("build/index/asm.sqlite")) {
        printf("real tree: build/index/asm.sqlite absent, skipped\n");
        return;
    }
    /* The recorded real-tree lines are read through build/index/asm.sqlite, which the test opens without refreshing.
     * An index older than the asm listings (the tree was regenerated by a re-split or a rename and nothing re-indexed
     * it yet) names functions that moved and files that are gone: those lines say nothing about the tool then. A
     * current index answers by address, so renames and re-splits do not change the lines. */
    {
        ShsmAsmIndex *ix = shsm_asmindex_open(&(ShsmAsmIndexOpts){.readonly = true, .no_refresh = true});
        bool current = ix && shsm_asmindex_is_current(ix);
        if (ix) shsm_asmindex_close(ix);
        if (!current) {
            printf("real tree: build/index/asm.sqlite is older than build/R5WEA4/asm; run any index-reading tool "
                   "(query.exe fn <symbol>) to refresh it. Skipped\n");
            return;
        }
    }
    lv = eq_live_new(NULL, 4);
    size_t nexp = sizeof LIVE_EXPECTED / sizeof *LIVE_EXPECTED;
    while (g_line < nexp) {
        unsigned long long addr = 0;
        AVP_ASSERT(sscanf(LIVE_EXPECTED[g_line], "real 0x%llX", &addr) == 1);
        u32 live, writes;
        if (eq_live_call(lv, (i64)addr, &live, &writes))
            expect(avp_sprintf(a, "real 0x%08llX %07X %07X", addr, live, writes));
        else
            expect(avp_sprintf(a, "real 0x%08llX None", addr));
    }
    eq_live_free(lv);
}

AVP_TEST(masks) {
    CHECK_EQ_I64(eq_abitz("r0"), 1);
    CHECK_EQ_I64(eq_abitz("r3"), 2);
    CHECK_EQ_I64(eq_abitz("f13"), 1 << 24);
    CHECK_EQ_I64(eq_abitz("r13"), 0);
    CHECK_EQ_I64(eq_abitz("cr0"), 0);
    AvpStr names[3] = {AVP_S("r11"), AVP_S("f1"), AVP_S("r31")};
    CHECK_EQ_I64(eq_reg_mask(names, 3), (1 << 9) | (1 << 12));
}

static const char RPO1[] = "cmpwi r3, 0x0|beq .L_4|subi r3, r3, 0x1|b .L_0|blr";
static const char RPO2[] = "cmpwi r3, 0x0|beq .L_4|li r3, 0x1|b .L_6|li r3, 0x2|blr|li r3, 0x3|blr";
static const char RPO3[] = "cmpwi r3, 0x0|beq .L_4|li r3, 0x1|b .L_5|li r3, 0x2|blr|li r3, 0x3|blr";

/* _rpo of the listing (rows separated by '|'; ".L_<n>" names row n) as "b0 b1 ... ". */
static AvpStr rpo_of(AvpArena *a, const char *src, size_t *nblocks) {
    AvpStr text = avp_str_replace(a, avp_str(src), AVP_S("|"), AVP_S("\n"), -1);
    WallCfg *g = wall_cfg_of_fn(a, wall_test_fn(a, avp_cz(a, text)));
    *nblocks = g->nblocks;
    int *order;
    size_t n = eq_rpo(a, g, &order);
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < n; i++) avp_buf_printf(&b, "%d ", order[i]);
    return avp_buf_str(&b);
}

AVP_TEST(rpo) {
    /* orders printed by tools/eqcheck.py _rpo on the same listings */
    AvpArena *a = avp_arena();
    size_t nb;
    CHECK_STR(rpo_of(a, RPO1, &nb), "0 1 2 "); /* 0 -> (2, 1), 1 -> 0 (a loop), 2 ret */
    CHECK_EQ_I64((i64)nb, 3);
    CHECK_STR(rpo_of(a, RPO2, &nb), "0 1 3 2 "); /* 0 -> (2, 1), 1 -> 3, 2 ret, 3 ret */
    CHECK_EQ_I64((i64)nb, 4);
    CHECK_STR(rpo_of(a, RPO3, &nb), "0 1 2 3 "); /* a diamond and an unreached block 4 */
    CHECK_EQ_I64((i64)nb, 5);
}

AVP_TEST_MAIN("eqcheck_live")
