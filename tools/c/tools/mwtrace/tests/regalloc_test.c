/* mwtrace regalloc without the compiler: the log parser (nodes, coalescing, the inliner's renames, simplify's sweeps
 * and optimistic choice, select's pops and nonvolatile registers), the saved-register reader, the two self-checks on
 * synthetic data (including disagreements that must give a gap), and the --against comparison of two traces. */
#include "avp/test.h"
#include "mwt_regalloc.h"

/* One function, f__Fi (obj 0x100), GPR only: parameter x (node 32), locals a (33), b (34), an inlined callee's local
 * t (renamed @20, node 35), a temporary 36 coalesced into a (33), a temporary 37 coalesced into r3. Simplify: sweep 1
 * pushes 35 (degree < K); every other node has degree >= K, so 34 is pushed optimistically (spill cost 2 / degree 30
 * is the smallest), then a sweep pushes 32 and 33. Select pops 33 (r31 added), 32 (r30 added), 34, 35. */
static const char *LOG =
    "compiler: Wii/1.0 (tools/compilers/Wii/1.0/mwcceppc.exe)\n"
    "IB 0x900 helper\n"
    "INL l @20 t\n"
    "IBR\n"
    "FN 0x100 1 f\n"
    "LN 0x100 f__Fi\n"
    "IG 4 0 32 38\n"
    "CX 3 32 interferes\n"
    "CO 33 36\n"
    "CO 3 37\n"
    "CX 32 3 interferes\n"
    "ARG 0x200 34 4 32 0 x\n"
    "LOC 0x300 2 4 33 0 a\n"
    "LOC 0x400 2 4 34 0 b\n"
    "LOC 0x500 2 4 35 0 @20\n"
    "LOC 0x600 0 0 0 0 big\n"
    "DEF 36 4 ADDI\n"
    "DEF 36 9 ADDI\n"
    "DEF 35 2 LWZ\n"
    "N 32 32 0x0 31 65535 0x200 31 0x0\n"
    "N 33 33 0x8 30 65535 0x300 30 0x0\n"
    "N 34 34 0x0 30 65535 0x400 30 0x0\n"
    "N 35 35 0x0 5 65535 0x500 5 0x0\n"
    "N 36 33 0x4 2 65535 0x0 2 0x0\n"
    "N 37 3 0x4 1 3 0x0 1 0x0\n"
    "K 4 29 0\n"
    "SW\n"
    "P1 35 5\n"
    "SW\n"
    "EST\n"
    "CAND 34 2 30 0x0\n"
    "CAND 33 90 30 0x0\n"
    "CAND 32 40 31 0x0\n"
    "OPT 34 2 30\n"
    "SW\n"
    "P2 32 20\n"
    "P2 33 19\n"
    "SW\n"
    "VOL 0x1ff9\n"
    "AVAIL 0xffffdff9\n"
    "SEL 33 0x8 90\n"
    "E 33 0\n"
    "E 33 3\n"
    "E 33 4\n"
    "FREE 0x0\n"
    "PICK -1\n"
    "NV 31\n"
    "SEL 33 0x8 90\n"
    "FREE 0x80000000\n"
    "PICK 31\n"
    "SEL 32 0x0 40\n"
    "FREE 0x0\n"
    "PICK -1\n"
    "NV 30\n"
    "SEL 32 0x0 40\n"
    "FREE 0x40000000\n"
    "PICK 30\n"
    "SEL 34 0x0 2\n"
    "FREE 0x1ff9\n"
    "PICK 0\n"
    "SEL 35 0x0 0\n"
    "FREE 0x1ff0\n"
    "PICK 4\n"
    "SELR 1\n"
    "RWV 32\n"
    "RWV 33\n"
    "RWV 34\n"
    "RWV 35\n"
    "FN 0x700 0 other\n";

static const MwtRaPass *gpr(const MwtRaFunc *f) { return mwt_ra_final(f, MWT_RC_GPR); }
static const MwtRaNode *node(const MwtRaPass *p, int i) { return &p->nodes.v[i - p->nreal]; }

AVP_TEST(parse_nodes_and_order) {
    AvpArena *a = avp_arena();
    MwtRaTrace t;
    mwt_ra_parse(a, avp_str(LOG), &t);
    CHECK_EQ_I64(t.funcs.n, 2);
    const char *why = NULL;
    const MwtRaFunc *f = mwt_ra_find(a, &t, "f__Fi", &why);
    CHECK(f != NULL);
    CHECK(mwt_ra_find(a, &t, "other", &why) == NULL); /* not traced (the --function filter) */
    const MwtRaPass *p = gpr(f);
    CHECK(p != NULL);
    CHECK_EQ_I64(p->K, 29);
    CHECK_EQ_I64(p->nodes.n, 6);
    CHECK(p->estimated);
    CHECK_EQ_I64(p->sweeps, 4);
    CHECK_EQ_I64(p->ok, 1);
    /* simplify: 35 low (sweep 1), 34 optimistic (sweep 2), 32 and 33 after it (sweep 3) */
    CHECK_EQ_I64(p->push.n, 4);
    CHECK_EQ_I64(p->push.v[0], 35);
    CHECK_EQ_I64(p->push.v[1], 34);
    CHECK_EQ_I64(node(p, 34)->push_how, MWT_PUSH_OPTIMISTIC);
    CHECK_EQ_I64(node(p, 33)->push_how, MWT_PUSH_AFTER);
    CHECK_EQ_I64(node(p, 33)->push_sweep, 3);
    CHECK_EQ_I64(p->choices.n, 1);
    CHECK_EQ_I64(p->choices.v[0].cands.n, 3);
    CHECK_EQ_I64(p->choices.v[0].chosen, 34);
    /* select: each node once, in pop order; the retries add r31 and r30 */
    CHECK_EQ_I64(p->select.n, 4);
    CHECK_EQ_I64(p->select.v[0], 33);
    CHECK_EQ_I64(node(p, 33)->select_pos, 0);
    CHECK_EQ_I64(node(p, 33)->nv_added, 31);
    CHECK_EQ_I64(node(p, 33)->free_mask, 0); /* the first try's mask */
    CHECK_EQ_I64(node(p, 33)->cost, 90);
    CHECK_EQ_I64(p->nv.n, 2);
    CHECK_EQ_I64(node(p, 33)->phys.n, 3);
    /* registers: a coalesced node takes its root's colour; one coalesced into r3 is r3 */
    CHECK_EQ_I64(node(p, 33)->final_reg, 31);
    CHECK_EQ_I64(node(p, 36)->final_reg, 31);
    CHECK_EQ_I64(node(p, 37)->final_reg, 3);
    CHECK_EQ_I64(node(p, 33)->members.n, 1);
    CHECK_EQ_I64(node(p, 33)->members.v[0], 36);
    /* names: parameter, local, an inlined callee's local, a temporary with its first definition */
    CHECK_EQ_I64(node(p, 32)->kind, 'p');
    CHECK_EQ_I64(node(p, 33)->kind, 'l');
    CHECK(avp_str_eqz(node(p, 35)->orig, "t"));
    CHECK(avp_str_eqz(node(p, 35)->inl_from, "helper"));
    CHECK(avp_str_eqz(node(p, 35)->key, "helper:t"));
    CHECK(avp_str_eqz(mwt_ra_label(a, node(p, 35)), "helper:t (@20)"));
    CHECK_EQ_I64(node(p, 36)->kind, 't');
    CHECK_EQ_I64(node(p, 36)->ndefs, 2);
    CHECK(avp_str_eqz(mwt_ra_label(a, node(p, 36)), "temp (#4 ADDI)"));
    /* coalescing: refusals deduplicated across rounds, merges kept */
    size_t refused = 0, merged = 0;
    for (size_t i = 0; i < p->merges.n; i++) {
        if (p->merges.v[i].why.n) refused++;
        else merged++;
    }
    CHECK_EQ_I64(refused, 1);
    CHECK_EQ_I64(merged, 2);
}

AVP_TEST(source_name_and_masks) {
    AvpArena *a = avp_arena();
    CHECK(avp_str_eqz(mwt_ra_source_name(AVP_S("set__Q24FMOD7MemPoolFiii")), "set"));
    CHECK(avp_str_eqz(mwt_ra_source_name(AVP_S("__ct__Q24FMOD4TestFv")), "__ct"));
    CHECK(avp_str_eqz(mwt_ra_source_name(AVP_S("WorldSyncCamera")), "WorldSyncCamera"));
    CHECK(avp_str_eqz(mwt_ra_source_name(AVP_S("_rwFreeListFreeReal")), "_rwFreeListFreeReal"));
    CHECK(!strcmp(mwt_ra_mask(a, 0x1ff9, MWT_RC_GPR), "r0 r3-r12"));
    CHECK(!strcmp(mwt_ra_mask(a, 0xc0000000, MWT_RC_FPR), "f30-f31"));
    CHECK(!strcmp(mwt_ra_mask(a, 0, MWT_RC_GPR), "none"));
}

AVP_TEST(saved_registers) {
    static const char *const ROWS[] = {"stwu r1, -0x30(r1)", "mflr r0", "stw r0, 0x34(r1)", "stw r31, 0x2c(r1)",
                                       "stmw r27, 0x8(r1)", "stfd f31, 0x20(r1)", "psq_st f30, 0x18(r1), 0, qr0",
                                       "stw r5, 0x8(r1)", "stw r30, 0x0(r3)", "bl _savefpr_28"};
    AvpStr rows[10];
    for (int i = 0; i < 10; i++) rows[i] = avp_str(ROWS[i]);
    u32 g = 0, f = 0;
    mwt_ra_saved_regs(rows, 10, &g, &f);
    CHECK_EQ_U64(g, 0xf8000000u); /* r27-r31; r5 is volatile, r30 into r3's memory is no save */
    CHECK_EQ_U64(f, 0xf0000000u); /* f28-f31 (_savefpr_28 and the two stores) */
    /* a frame pointer (mr r31, r1) is saved by stmw but no allocator register */
    AvpStr fp[3] = {AVP_S("stwu r1, -0x50(r1)"), AVP_S("stmw r14, 0x8(r1)"), AVP_S("mr r31, r1")};
    mwt_ra_saved_regs(fp, 3, &g, &f);
    CHECK_EQ_U64(g, 0x7fffc000u); /* r14-r30 */
}

static MwtDwVars dw_of(const char *const *names, const char *const *locs, size_t n) {
    MwtDwVars v = {0};
    for (size_t i = 0; i < n; i++) {
        MwtDwVar x = {avp_str(names[i]), avp_str(locs[i]), false};
        avp_vec_push(&v, x);
    }
    return v;
}

AVP_TEST(self_check_agrees_and_disagrees) {
    AvpArena *a = avp_arena();
    MwtRaTrace t;
    mwt_ra_parse(a, avp_str(LOG), &t);
    const char *why = NULL;
    const MwtRaFunc *f = mwt_ra_find(a, &t, "f__Fi", &why);
    static const char *const N[] = {"x", "a", "b"};
    static const char *const L[] = {"r30", "r31", "r0"};
    MwtDwVars dw = dw_of(N, L, 3);
    MwtRaCheck c;
    mwt_ra_check(a, f, &dw, true, 0xc0000000u, 0, true, &c);
    CHECK_EQ_I64(c.gaps.n, 0);
    CHECK_EQ_I64(c.checked.n, 3);
    CHECK_EQ_U64(c.trace_nv[MWT_RC_GPR], 0xc0000000u);
    bool big = false, inl = false;
    for (size_t i = 0; i < c.uncovered.n; i++) {
        if (avp_str_startswith(c.uncovered.v[i], AVP_S("big: not in a register"))) big = true;
        if (avp_str_find(c.uncovered.v[i], AVP_S("an inlined object of helper"), 0, (i64)c.uncovered.v[i].n) >= 0) inl = true;
    }
    CHECK(big);
    CHECK(inl);
    /* a variable in another register: a dwarf gap naming it */
    static const char *const L2[] = {"r30", "r29", "r0"};
    MwtDwVars dw2 = dw_of(N, L2, 3);
    mwt_ra_check(a, f, &dw2, true, 0xc0000000u, 0, true, &c);
    CHECK_EQ_I64(c.gaps.n, 1);
    CHECK(avp_str_eqz(c.gaps.v[0].what, "dwarf"));
    CHECK(avp_str_startswith(c.gaps.v[0].why, AVP_S("a: the trace gives r31, the -sym on DWARF r29")));
    /* a DWARF register variable the trace does not have */
    static const char *const N3[] = {"x", "a", "b", "ghost"};
    static const char *const L3[] = {"r30", "r31", "r0", "r28"};
    MwtDwVars dw3 = dw_of(N3, L3, 4);
    mwt_ra_check(a, f, &dw3, true, 0xc0000000u, 0, true, &c);
    CHECK_EQ_I64(c.gaps.n, 1);
    CHECK(avp_str_startswith(c.gaps.v[0].why, AVP_S("ghost:")));
    /* a saved register the trace does not assign: a prologue gap naming the first */
    mwt_ra_check(a, f, &dw, true, 0xe0000000u, 0, true, &c);
    CHECK_EQ_I64(c.gaps.n, 1);
    CHECK(avp_str_eqz(c.gaps.v[0].what, "prologue"));
    CHECK(avp_str_startswith(c.gaps.v[0].why, AVP_S("r29: not assigned by the trace, saved by the code")));
}

AVP_TEST(not_rewritten_variable_keeps_its_virtual_register) {
    AvpArena *a = avp_arena();
    /* node 34 (p) is coloured r0 but the rewrite skips it: the DWARF holds register number 34, printed f2 */
    const char *log = "FN 0x100 1 g\n"
                      "IG 4 0 32 35\n"
                      "ARG 0x200 34 4 32 0 this\n"
                      "ARG 0x300 34 4 34 0 p\n"
                      "N 32 32 0x0 3 65535 0x200 3 0x0\n"
                      "N 33 33 0x0 3 65535 0x0 3 0x0\n"
                      "N 34 34 0x0 0 65535 0x300 0 0x0\n"
                      "K 4 29 0\nSW\nP1 32 3\nP1 33 2\nP1 34 0\nSW\n"
                      "VOL 0x1ff9\nAVAIL 0xffffdff9\n"
                      "SEL 34 0x2 0\nFREE 0x1ff9\nPICK 0\n"
                      "SEL 33 0x2 0\nFREE 0x1ff9\nPICK 0\n"
                      "SEL 32 0x2 0\nFREE 0x1ff8\nPICK 3\n"
                      "SELR 1\nRWV 32\n";
    MwtRaTrace t;
    mwt_ra_parse(a, avp_str(log), &t);
    const char *why = NULL;
    const MwtRaFunc *f = mwt_ra_find(a, &t, "g", &why);
    CHECK(f != NULL);
    static const char *const N[] = {"this", "p"};
    static const char *const L[] = {"r3", "f2"};
    MwtDwVars dw = dw_of(N, L, 2);
    MwtRaCheck c;
    mwt_ra_check(a, f, &dw, true, 0, 0, true, &c);
    CHECK_EQ_I64(c.gaps.n, 0);
    static const char *const L2[] = {"r3", "r0"};
    MwtDwVars dw2 = dw_of(N, L2, 2);
    mwt_ra_check(a, f, &dw2, true, 0, 0, true, &c);
    CHECK_EQ_I64(c.gaps.n, 1);
}

static bool any_starts(const AvpStrVec *v, const char *s) {
    for (size_t i = 0; i < v->n; i++)
        if (avp_str_startswith(v->v[i], avp_str(s))) return true;
    return false;
}

/* A DWARF location the dump cannot read is not a disagreement: the variable is listed as not covered, with the reason;
 * readable locations of the same name that disagree still are. */
AVP_TEST(unreadable_dwarf_location_is_not_covered) {
    AvpArena *a = avp_arena();
    MwtRaTrace t;
    mwt_ra_parse(a, avp_str(LOG), &t);
    const char *why = NULL;
    const MwtRaFunc *f = mwt_ra_find(a, &t, "f__Fi", &why);
    static const char *const N[] = {"x", "a", "b"};
    static const char *const L[] = {"r30", "[invalid]", "r0"};
    MwtDwVars dw = dw_of(N, L, 3);
    MwtRaCheck c;
    mwt_ra_check(a, f, &dw, true, 0xc0000000u, 0, true, &c);
    CHECK_EQ_I64(c.gaps.n, 0);
    CHECK_EQ_I64(c.checked.n, 2);
    CHECK(any_starts(&c.uncovered, "a: the -sym on DWARF has no readable location ([invalid]; trace r31)"));
    /* one readable register that disagrees, one unreadable: still a gap */
    static const char *const N2[] = {"x", "a", "a", "b"};
    static const char *const L2[] = {"r30", "[invalid]", "r29", "r0"};
    MwtDwVars dw2 = dw_of(N2, L2, 4);
    mwt_ra_check(a, f, &dw2, true, 0xc0000000u, 0, true, &c);
    CHECK_EQ_I64(c.gaps.n, 1);
}

/* Two variables of one name in two scopes, one in a register and one in memory (its address taken): the DWARF's stack
 * location is the namesake's, the register one is compared. Without an in-memory namesake in the trace the stack
 * location is a disagreement. */
AVP_TEST(stack_namesake_is_compared_apart) {
    AvpArena *a = avp_arena();
    const char *log = "FN 0x100 1 g\n"
                      "IG 4 0 32 34\n"
                      "ARG 0x200 34 4 32 0 this\n"
                      "LOC 0x300 2 4 33 0 v\n"
                      "LOC 0x400 0 0 0 0 v\n"
                      "N 32 32 0x0 3 65535 0x200 3 0x0\n"
                      "N 33 33 0x0 3 65535 0x300 3 0x0\n"
                      "K 4 29 0\nSW\nP1 32 3\nP1 33 2\nSW\n"
                      "VOL 0x1ff9\nAVAIL 0xffffdff9\n"
                      "SEL 33 0x2 0\nFREE 0x1ff9\nPICK 8\n"
                      "SEL 32 0x2 0\nFREE 0x1ff8\nPICK 3\n"
                      "SELR 1\nRWV 32\nRWV 33\n";
    MwtRaTrace t;
    mwt_ra_parse(a, avp_str(log), &t);
    const char *why = NULL;
    const MwtRaFunc *f = mwt_ra_find(a, &t, "g", &why);
    CHECK(f != NULL);
    static const char *const N[] = {"this", "v", "v"};
    static const char *const L[] = {"r3", "r8", "r1+0x8"};
    MwtDwVars dw = dw_of(N, L, 3);
    MwtRaCheck c;
    mwt_ra_check(a, f, &dw, true, 0, 0, true, &c);
    CHECK_EQ_I64(c.gaps.n, 0);
    CHECK(any_starts(&c.checked, "v r8 (and 1 stack location(s) of that name"));
    static const char *const N2[] = {"this", "v", "v", "v"};
    static const char *const L2[] = {"r3", "r8", "r1+0x8", "r1+0xc"};
    MwtDwVars dw2 = dw_of(N2, L2, 4);
    mwt_ra_check(a, f, &dw2, true, 0, 0, true, &c);
    CHECK_EQ_I64(c.gaps.n, 1);
    CHECK(avp_str_startswith(c.gaps.v[0].why, AVP_S("v: the -sym on DWARF has 2 stack location(s) of that name")));
}

/* Trace B of the same function: b declared first, so the two locals trade nodes 33 and 34 and registers. */
static const char *LOG_B =
    "FN 0x100 1 f\n"
    "LN 0x100 f__Fi\n"
    "IG 4 0 32 36\n"
    "ARG 0x200 34 4 32 0 x\n"
    "LOC 0x400 2 4 33 0 b\n"
    "LOC 0x300 2 4 34 0 a\n"
    "DEF 35 3 LWZ\n"
    "N 32 32 0x0 20 65535 0x200 20 0x0\n"
    "N 33 33 0x0 20 65535 0x400 20 0x0\n"
    "N 34 34 0x0 20 65535 0x300 20 0x0\n"
    "N 35 35 0x0 2 65535 0x0 2 0x0\n"
    "K 4 29 0\nSW\nP1 32 20\nP1 33 19\nP1 34 18\nP1 35 1\nSW\n"
    "VOL 0x1ff9\nAVAIL 0xffffdff9\n"
    "SEL 35 0x2 0\nFREE 0x1ff9\nPICK 0\n"
    "SEL 34 0x2 0\nFREE 0x0\nPICK -1\nNV 31\nSEL 34 0x2 0\nFREE 0x80000000\nPICK 31\n"
    "SEL 33 0x2 0\nFREE 0x0\nPICK -1\nNV 30\nSEL 33 0x2 0\nFREE 0x40000000\nPICK 30\n"
    "SEL 32 0x2 0\nFREE 0x0\nPICK -1\nNV 29\nSEL 32 0x2 0\nFREE 0x20000000\nPICK 29\n"
    "SELR 1\n";
static const char *LOG_A =
    "FN 0x100 1 f\n"
    "LN 0x100 f__Fi\n"
    "IG 4 0 32 36\n"
    "ARG 0x200 34 4 32 0 x\n"
    "LOC 0x300 2 4 33 0 a\n"
    "LOC 0x400 2 4 34 0 b\n"
    "DEF 35 3 LWZ\n"
    "N 32 32 0x0 20 65535 0x200 20 0x0\n"
    "N 33 33 0x0 20 65535 0x300 20 0x0\n"
    "N 34 34 0x0 20 65535 0x400 20 0x0\n"
    "N 35 35 0x0 2 65535 0x0 2 0x0\n"
    "K 4 29 0\nSW\nP1 32 20\nP1 33 19\nP1 34 18\nP1 35 1\nSW\n"
    "VOL 0x1ff9\nAVAIL 0xffffdff9\n"
    "SEL 35 0x2 0\nFREE 0x1ff9\nPICK 0\n"
    "SEL 34 0x2 0\nFREE 0x0\nPICK -1\nNV 31\nSEL 34 0x2 0\nFREE 0x80000000\nPICK 31\n"
    "SEL 33 0x2 0\nFREE 0x0\nPICK -1\nNV 30\nSEL 33 0x2 0\nFREE 0x40000000\nPICK 30\n"
    "SEL 32 0x2 0\nFREE 0x0\nPICK -1\nNV 29\nSEL 32 0x2 0\nFREE 0x20000000\nPICK 29\n"
    "SELR 1\n";

static bool has_line(const AvpStrVec *v, const char *prefix) {
    for (size_t i = 0; i < v->n; i++)
        if (avp_str_startswith(v->v[i], avp_str(prefix))) return true;
    return false;
}

AVP_TEST(against_two_traces) {
    AvpArena *a = avp_arena();
    MwtRaTrace ta, tb;
    mwt_ra_parse(a, avp_str(LOG_A), &ta);
    mwt_ra_parse(a, avp_str(LOG_B), &tb);
    const char *why = NULL;
    const MwtRaFunc *fa = mwt_ra_find(a, &ta, "f__Fi", &why), *fb = mwt_ra_find(a, &tb, "f__Fi", &why);
    CHECK(fa && fb);
    AvpStrVec d = mwt_ra_diff(a, fa, fb);
    for (size_t i = 0; i < d.n; i++) printf("  %.*s\n", AVP_SF(d.v[i]));
    CHECK(has_line(&d, "`a`: r30 in A, r31 in B; node 33 in A, 34 in B; selected 3rd in A, 2nd in B"));
    CHECK(has_line(&d, "`b`: r31 in A, r30 in B; node 34 in A, 33 in B; selected 2nd in A, 3rd in B"));
    CHECK(has_line(&d, "GPR select 2nd: A node 34 b"));
    CHECK(has_line(&d, "GPR simplify 2nd: A node 33 a"));
    CHECK(!has_line(&d, "`x`")); /* the same register in both */
    /* the same trace twice: no difference */
    AvpStrVec same = mwt_ra_diff(a, fa, fa);
    CHECK(has_line(&same, "GPR nodes: the same sequence in A and B"));
    CHECK(has_line(&same, "GPR select: the same in A and B"));
    CHECK(!has_line(&same, "`"));
}

AVP_TEST(trace_errors_are_kept) {
    AvpArena *a = avp_arena();
    MwtRaTrace t;
    mwt_ra_parse(a, AVP_S("FN 0x1 1 f\nHANDLER ERR KeyError: x (spec:3, bp 0x1; this handler is stopped)\n"), &t);
    CHECK_EQ_I64(t.errors.n, 1);
}

AVP_TEST_MAIN()
