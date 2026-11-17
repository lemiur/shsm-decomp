/* The scheduler's part of the solver without the compiler: the order test on synthetic rows (the same instructions in
 * another order; a register rename; a reorder whose defined registers are renamed; a reorder with another immediate;
 * an instruction on one side only; a window across a branch), and the condition words for each selection key and for
 * a held edge, from a synthetic trace (the log form of mwtrace's own sched test) and from hand-made pairs. */
#include "avp/test.h"
#include "solver.h"

static SvORow *rows(AvpArena *a, const char *const *norm, size_t n) {
    SvORow *r = AVP_NEWN(a, SvORow, n + 1);
    for (size_t i = 0; i < n; i++) {
        r[i].norm = norm[i];
        r[i].branch = norm[i][0] == 'b';
        r[i].target = false;
    }
    return r;
}

static SvOrderDiff diff(AvpArena *a, const char *const *r, size_t nr, const char *const *o, size_t no) {
    return sv_order_diff(a, rows(a, r, nr), nr, rows(a, o, no), no);
}

AVP_TEST(mask) {
    AvpArena *a = avp_arena();
    CHECK(!strcmp(sv_mask_regs(a, "lwz r3, 0x4(r4)"), "lwz R, 0x4(R)"));
    CHECK(!strcmp(sv_mask_regs(a, "fadds f1, f2, f31"), "fadds F, F, F"));
    CHECK(!strcmp(sv_mask_regs(a, "lis r3, SYM@ha"), "lis R, SYM@ha"));
}

/* the same instructions, registers included, in another order: an order difference */
AVP_TEST(order_same_instructions_reordered) {
    AvpArena *a = avp_arena();
    static const char *const R[] = {"lbz r9, 0x1e(r3)", "slwi r10, r5, 2", "slwi r11, r0, 2", "add r12, r10, r11", "blr"};
    static const char *const O[] = {"lbz r9, 0x1e(r3)", "slwi r11, r0, 2", "slwi r10, r5, 2", "add r12, r10, r11", "blr"};
    SvOrderDiff d = diff(a, R, 5, O, 5);
    CHECK(d.reorder);
    CHECK_EQ_I64(d.row, 1);
    CHECK_EQ_I64(d.end, 3);
    CHECK_EQ_I64(d.ours_at, 2);
    CHECK_EQ_I64(d.retail_at, 2);
}

/* the same instructions in the same order with other registers: a register difference */
AVP_TEST(order_register_rename) {
    AvpArena *a = avp_arena();
    static const char *const R[] = {"lbzx r12, r4, r5", "lbzx r28, r4, r9", "cmplw r12, r28", "blr"};
    static const char *const O[] = {"lbzx r28, r4, r5", "lbzx r31, r4, r9", "cmplw r28, r31", "blr"};
    SvOrderDiff d = diff(a, R, 4, O, 4);
    CHECK(!d.reorder);
    CHECK_EQ_I64(d.row, 0);
    CHECK(strstr(d.why, "same order with other registers") != NULL);
}

/* a mix: reordered, and the registers the window defines renamed: still an order difference; reordered with another
 * immediate: not the same instructions */
AVP_TEST(order_mix) {
    AvpArena *a = avp_arena();
    static const char *const R[] = {"clrlslwi r30, r5, 24, 2", "clrlslwi r11, r0, 24, 2", "subf r30, r5, r30", "blr"};
    static const char *const O[] = {"clrlslwi r11, r0, 24, 2", "clrlslwi r12, r5, 24, 2", "subf r12, r5, r12", "blr"};
    SvOrderDiff d = diff(a, R, 4, O, 4);
    CHECK(d.reorder);
    CHECK_EQ_I64(d.row, 0);
    CHECK_EQ_I64(d.ours_at, 1);
    CHECK_EQ_I64(d.retail_at, 1);
    static const char *const O2[] = {"clrlslwi r11, r0, 24, 3", "clrlslwi r12, r5, 24, 2", "subf r12, r5, r12", "blr"};
    SvOrderDiff d2 = diff(a, R, 4, O2, 4);
    CHECK(!d2.reorder);
    /* reads of r5 and of r0 are different instructions even though they mask alike: neither a rename nor a reorder */
    static const char *const R3[] = {"slwi r3, r5, 2", "blr"};
    static const char *const O3[] = {"slwi r3, r0, 2", "blr"};
    SvOrderDiff d3 = diff(a, R3, 2, O3, 2);
    CHECK(!d3.reorder);
    CHECK(strstr(d3.why, "no window") != NULL);
}

/* an instruction present on one side only */
AVP_TEST(order_one_side_only) {
    AvpArena *a = avp_arena();
    static const char *const R[] = {"li r3, 0x0", "addi r4, r3, 0x1", "blr"};
    static const char *const O[] = {"li r3, 0x0", "mr r5, r3", "addi r4, r3, 0x1", "blr"};
    SvOrderDiff d = diff(a, R, 3, O, 4);
    CHECK(!d.reorder);
    static const char *const O2[] = {"li r3, 0x0"};
    SvOrderDiff d2 = diff(a, R, 3, O2, 1);
    CHECK(!d2.reorder);
    CHECK(strstr(d2.why, "one side only") != NULL);
}

/* a window does not cross a branch: the two halves are two blocks */
AVP_TEST(order_not_across_a_branch) {
    AvpArena *a = avp_arena();
    static const char *const R[] = {"li r3, 0x1", "beq .L_1", "li r4, 0x2", "blr"};
    static const char *const O[] = {"li r4, 0x2", "beq .L_1", "li r3, 0x1", "blr"};
    SvOrderDiff d = diff(a, R, 4, O, 4);
    CHECK(!d.reorder);
}

/* The synthetic block of mwtrace's sched test: #0 LI r3=, #1 LIS r33=, #2 ADDI r4= r33, #3 BL; #1 is critical at
 * cycle 0 and taken over #0; #2 waits for #1's latency; #3 waits for #2 (waw). */
#define BLOCK                                                                                                         \
    "BV 0xb0 4 0x4 1\n"                                                                                               \
    "SB 0xb0 1 4 0x4 0x1010 2 1 0\n"                                                                                  \
    "NC 0x40 0x1040 BL 0 0x8 0x0 0\n"                                                                                 \
    "OP 4 3 3\n"                                                                                                      \
    "OP 4 4 3\n"                                                                                                      \
    "NC 0x30 0x1030 ADDI 1 0x0 0x0 2\n"                                                                               \
    "OP 4 4 2\n"                                                                                                      \
    "EA 0x30 0x40 0 0x5bc027\n"                                                                                       \
    "EN 0\n"                                                                                                          \
    "OP 4 33 1\n"                                                                                                     \
    "NC 0x20 0x1020 LIS 1 0x0 0x0 4\n"                                                                                \
    "OP 4 33 2\n"                                                                                                     \
    "EA 0x20 0x30 1 0x5bbff1\n"                                                                                       \
    "EN 1\n"                                                                                                          \
    "EA 0x20 0x40 0 0x5bbae2\n"                                                                                       \
    "EN 0\n"                                                                                                          \
    "NC 0x10 0x1010 LI 1 0x0 0x0 4\n"                                                                                 \
    "OP 4 3 2\n"                                                                                                      \
    "EA 0x10 0x40 0 0x5bc027\n"                                                                                       \
    "EN 0\n"                                                                                                          \
    "MA ls 0x10 0x30 0\n"                                                                                             \
    "MP ss 0x10 0x40 1\n"                                                                                             \
    "N 0x10 1 1 0 2\n"                                                                                                \
    "N 0x20 2 0 0 2\n"                                                                                                \
    "N 0x30 1 1 1 2\n"                                                                                                \
    "N 0x40 0 2 3 2\n"                                                                                                \
    "S 0 0\n"                                                                                                         \
    "CI 0x10 1\n"                                                                                                     \
    "INC 0x10\n"                                                                                                      \
    "CI 0x20 1\n"                                                                                                     \
    "T 0x20\n"                                                                                                        \
    "I 0x20\n"                                                                                                        \
    "U 0x30 0 1 0\n"                                                                                                  \
    "U 0x40 2 0 0\n"                                                                                                  \
    "S 0 1\n"                                                                                                         \
    "CI 0x10 1\n"                                                                                                     \
    "INC 0x10\n"                                                                                                      \
    "I 0x10\n"                                                                                                        \
    "U 0x40 1 0 0\n"                                                                                                  \
    "CY 0\n"                                                                                                          \
    "S 1 0\n"                                                                                                         \
    "CI 0x30 1\n"                                                                                                     \
    "INC 0x30\n"                                                                                                      \
    "I 0x30\n"                                                                                                        \
    "U 0x40 0 1 0\n"                                                                                                  \
    "S 1 1\n"                                                                                                         \
    "CI 0x40 1\n"                                                                                                     \
    "INC 0x40\n"                                                                                                      \
    "I 0x40\n"                                                                                                        \
    "CY 1\n"                                                                                                          \
    "BE\n"

static const char *LOG = "compiler: Wii/1.0 (tools/compilers/Wii/1.0/mwcceppc.exe)\n"
                         "FN 0x100 1 probe\n"
                         "LN 0x100 probe__Fi\n"
                         "SD 0 1 2 4 0 0 0 0 0\n" BLOCK "RW\n"
                         "RI 0x1040\n"
                         "RI 0x1030\n"
                         "RI 0x1010\n"
                         "RI 0x1020\n"
                         "SD 1 0 2 4 0 0 0 0 0\n"
                         "BV 0xa0 1 0x5 0\n" BLOCK "AS 0x1001 BLR 0xa0\n"
                         "AS 0x1020 LIS 0xb0\n"
                         "AS 0x1010 LI 0xb0\n"
                         "AS 0x1030 ADDI 0xb0\n"
                         "AS 0x1040 BL 0xb0\n";

static const MwtSchedFunc *parse(AvpArena *a, MwtSchedTrace *t) {
    mwt_sched_parse(a, avp_str(LOG), t);
    const char *why = NULL;
    return mwt_sched_find(a, t, "probe__Fi", &why);
}

AVP_TEST(condition_from_a_trace) {
    AvpArena *a = avp_arena();
    MwtSchedTrace t;
    const MwtSchedFunc *f = parse(a, &t);
    REQUIRE(f);
    MwtSchedPair p;
    const char *why = NULL;
    /* critical: retail issues the LI first, ours the LIS */
    REQUIRE(mwt_sched_pair(a, f, 2, 0, 0, 1, &p, &why));
    const char *c = sv_sched_condition(a, "li r3, 0x0", "lis r33, table@ha", &p, 4, NULL, -1, true);
    CHECK(strstr(c, "retail issues `li r3, 0x0` first: in ours it lost to `lis r33, table@ha` on critical (slack 0 <= "
                    "cycle 0; #0 slack 1)") != NULL);
    CHECK(strstr(c, "at cycle 0 of block 0 (index 4), pass 2; pass 2 decided") != NULL);
    CHECK(!strcmp(sv_sched_cause(a, &p), "schedule: critical"));
    /* held by a raw edge's latency */
    REQUIRE(mwt_sched_pair(a, f, 1, 0, 2, 0, &p, &why));
    c = sv_sched_condition(a, "addi r4, r33, table@l", "li r3, 0x0", &p, 4, NULL, -1, true);
    CHECK(strstr(c, "was not ready when `li r3, 0x0` issued at cycle 0 of block 0 (index 4), pass 1") != NULL);
    CHECK(strstr(c, "edge raw r33, latency 1") != NULL);
    CHECK(!strcmp(sv_sched_cause(a, &p), "schedule: held by raw latency"));
    /* held by an unissued predecessor (waw) */
    REQUIRE(mwt_sched_pair(a, f, 1, 0, 3, 2, &p, &why));
    CHECK(!strcmp(sv_sched_cause(a, &p), "schedule: held by waw"));
}

static MwtSchedPair ready_pair(int pass, MwtSchedKey k, const char *words) {
    MwtSchedPair p = {0};
    p.pass = pass;
    p.block = 3;
    p.first = 1;
    p.second = 2;
    p.cycle = 2;
    p.both_ready = true;
    p.key = k;
    p.key_words = avp_str(words);
    p.cmp_words = AVP_S("");
    p.held = AVP_S("");
    return p;
}

AVP_TEST(condition_each_key) {
    AvpArena *a = avp_arena();
    static const struct {
        MwtSchedKey k;
        const char *words, *cause;
    } K[] = {{MWT_KEY_SUCCESSORS, "successors 2 > 1", "schedule: successors"},
             {MWT_KEY_HEIGHT, "height 14 > 9", "schedule: height"},
             {MWT_KEY_PRIORITY, "priority 2 < 4", "schedule: priority"}};
    for (size_t i = 0; i < AVP_ARRAY_LEN(K); i++) {
        MwtSchedPair p = ready_pair(2, K[i].k, K[i].words);
        const char *c = sv_sched_condition(a, "R", "O", &p, 9, NULL, -1, true);
        CHECK(strstr(c, avp_sprintf(a, "lost to `O` on %s (the winner's number first) at cycle 2 of block 3 (index 9), "
                                       "pass 2; pass 2 decided",
                                    K[i].words)) != NULL);
        CHECK(!strcmp(sv_sched_cause(a, &p), K[i].cause));
    }
    /* pass 2 keeps its input order (program order): pass 1 decided, by its own key */
    MwtSchedPair p2 = ready_pair(2, MWT_KEY_ORDER, "program order (#1 before #2; slack, successors 0, height 3 equal)");
    MwtSchedPair p1 = ready_pair(1, MWT_KEY_HEIGHT, "height 5 > 4");
    const char *c = sv_sched_condition(a, "R", "O", &p2, 9, &p1, 7, true);
    CHECK(strstr(c, "pass 2 kept its input order, which pass 1 made: in pass 1 it lost to `O` on height 5 > 4") != NULL);
    CHECK(strstr(c, "(index 7), pass 1; pass 1 decided") != NULL);
    CHECK(!strcmp(sv_sched_cause(a, &p1), "schedule: height"));
    /* both passes by program order */
    MwtSchedPair q1 = ready_pair(1, MWT_KEY_ORDER, "program order (#1 before #2)");
    c = sv_sched_condition(a, "R", "O", &p2, 9, &q1, 7, true);
    CHECK(strstr(c, "both passes keep the program order") != NULL);
    CHECK(!strcmp(sv_sched_cause(a, &q1), "schedule: order"));
}

AVP_TEST(held_kinds) {
    AvpArena *a = avp_arena();
    CHECK(!strcmp(sv_sched_held_kind(a, "#3 waits for #2 LWZ r4= r3 (edge mem-store-load, latency 2; not issued yet)"),
                  "mem-store-load"));
    CHECK(!strcmp(sv_sched_held_kind(a, "#2's earliest cycle is 1: #1 LIS r33= issued at cycle 0, edge raw r33, latency 1"),
                  "raw latency"));
    CHECK(!strcmp(sv_sched_held_kind(a, "#2 was ready by its dependences, but the machine model's can-issue answered 0"),
                  "can-issue 0"));
    CHECK(!strcmp(sv_sched_held_kind(a, ""), ""));
}

AVP_TEST_MAIN()
