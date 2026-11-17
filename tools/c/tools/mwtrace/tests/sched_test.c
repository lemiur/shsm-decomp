/* mwtrace sched without the compiler: the log parser (nodes in program order from their reverse creation, edges and
 * their kinds from the edge adder's call sites, memory pairs, selections), the replay and both self-check stages on
 * synthetic data (each with a deliberate disagreement that must give TRACE-GAP), the opcode matcher, the --against
 * comparison and the pair query. */
#include "avp/test.h"
#include "mwt_sched.h"

/* One block of probe__Fi, the same in both passes (pass 2 with coloring 0): #0 LI r3=, #1 LIS r33=, #2 ADDI r4= r33,
 * #3 BL. Edges: #1 -> #2 raw r33 (latency 1), #1 -> #3 to-serial, #0 -> #3 and #2 -> #3 waw (latency 0). Heights 1, 2,
 * 1, 0, greatest 2: slack 1, 0, 1, 2. Cycle 0: #1 is critical and taken over #0; then #0. Cycle 1: #2, then #3. A memory
 * pair #0 -> #2 was asked (may_alias 0), and a store-store pair answered 1 by the precheck. */
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
                         "FN 0x90 0 warm_up\n"
                         "FN 0x100 1 probe\n"
                         "LN 0x100 probe__Fi\n"
                         "SD 0 1 2 4 0 0 0 0 0\n" BLOCK "RW\n"
                         "RI 0x1040\n"
                         "RI 0x1030\n"
                         "RI 0x1010\n"
                         "RI 0x1020\n"
                         "RW\n"
                         "RI 0x9999\n"
                         "SD 1 0 2 4 0 0 0 0 0\n"
                         "BV 0xa0 1 0x5 0\n" BLOCK "AS 0x1001 BLR 0xa0\n"
                         "AS 0x1020 LIS 0xb0\n"
                         "AS 0x1010 LI 0xb0\n"
                         "AS 0x1030 ADDI 0xb0\n"
                         "AS 0x1040 BL 0xb0\n";

static const char *const ROWS_Z[] = {"blr", "lis r33, table@ha", "li r3, 0x0", "addi r4, r33, table@l", "bl g"};

static const MwtSchedFunc *parse(AvpArena *a, const char *log, MwtSchedTrace *t) {
    mwt_sched_parse(a, avp_str(log), t);
    const char *why = NULL;
    return mwt_sched_find(a, t, "probe__Fi", &why);
}

static bool has_gap(const MwtSchedCheck *c, const char *stage, const char *part) {
    for (size_t i = 0; i < c->gaps.n; i++)
        if (avp_str_eqz(c->gaps.v[i].stage, stage) && avp_str_contains(c->gaps.v[i].why, avp_str(part))) return true;
    return false;
}

static bool has_line(const AvpStrVec *v, const char *part) {
    for (size_t i = 0; i < v->n; i++)
        if (avp_str_contains(v->v[i], avp_str(part))) return true;
    return false;
}

static const char *replace(AvpArena *a, const char *s, const char *from, const char *to) {
    return avp_cz(a, avp_str_replace(a, avp_str(s), avp_str(from), avp_str(to), 1));
}

AVP_TEST(parse_block) {
    AvpArena *a = avp_arena();
    MwtSchedTrace t;
    const MwtSchedFunc *f = parse(a, LOG, &t);
    CHECK(f != NULL);
    CHECK_EQ_I64(f->passes.n, 2);
    const MwtSchedPass *p1 = mwt_sched_pass(f, 1), *p2 = mwt_sched_pass(f, 2);
    CHECK(p1 && p2);
    CHECK_EQ_I64(p1->width, 2);
    CHECK(p1->coloring == 1 && p2->coloring == 0);
    const MwtSchedBlock *b = &p1->blocks.v[0];
    CHECK(b->prio_key);
    CHECK(!p2->blocks.v[0].prio_key);
    CHECK_EQ_I64(b->nodes.n, 4);
    CHECK(avp_str_eqz(b->nodes.v[0].op, "LI"));
    CHECK(avp_str_eqz(b->nodes.v[1].op, "LIS"));
    CHECK(avp_str_eqz(b->nodes.v[2].opnds, "r4= r33"));
    CHECK_EQ_I64(b->nodes.v[1].height, 2);
    CHECK_EQ_I64(b->nodes.v[1].slack, 0);
    CHECK_EQ_I64(b->maxh, 2);
    CHECK_EQ_I64(b->edges.n, 4);
    bool raw = false, serial = false;
    for (size_t i = 0; i < b->edges.n; i++) {
        const MwtSchedEdge *e = &b->edges.v[i];
        if (e->from == 1 && e->to == 2) raw = e->kinds == (1u << MWT_DEP_RAW) && e->latency == 1 && avp_str_eqz(e->regs, "r33");
        if (e->from == 1 && e->to == 3) serial = e->kinds == (1u << MWT_DEP_TO_SERIAL);
    }
    CHECK(raw && serial);
    CHECK_EQ_I64(b->mems.n, 2);
    CHECK(b->mems.v[0].kind == 'l' && b->mems.v[0].alias == 0 && b->mems.v[0].precheck == -1);
    CHECK(b->mems.v[1].kind == 'S' && b->mems.v[1].precheck == 1 && b->mems.v[1].alias == -1);
    CHECK_EQ_I64(b->sels.n, 4);
    CHECK_EQ_I64(b->sels.v[0].chosen, 1);
    CHECK_EQ_I64(b->sels.v[0].cmps.n, 1);
    CHECK(b->sels.v[0].cmps.v[0].taken);
    CHECK(b->sels.v[0].cmps.v[0].key == MWT_KEY_CRITICAL);
    CHECK_EQ_I64(b->order.n, 4);
    CHECK(b->order.v[0] == 1 && b->order.v[1] == 0 && b->order.v[2] == 2 && b->order.v[3] == 3);
    /* regalloc's numbers come from the first walk only; emitted positions from AS */
    CHECK_EQ_I64(b->nodes.v[3].ra, 1);
    CHECK_EQ_I64(b->nodes.v[1].ra, 4);
    CHECK_EQ_I64(p2->blocks.v[0].nodes.v[1].fpos, 1);
    CHECK_EQ_I64(f->emitted.n, 5);
    CHECK(p2->visits.n == 2 && p2->visits.v[0].sched == -1 && p2->visits.v[1].sched == 0);
}

AVP_TEST(compare_keys) {
    AvpArena *a = avp_arena();
    MwtSchedTrace t;
    const MwtSchedFunc *f = parse(a, LOG, &t);
    const MwtSchedBlock *b = &mwt_sched_pass(f, 1)->blocks.v[0];
    bool w = false;
    CHECK(mwt_sched_compare(b, 0, 1, 0, 0, 0, &w) == MWT_KEY_CRITICAL && w);
    CHECK(mwt_sched_compare(b, 1, 2, 0, 1, 0, &w) == MWT_KEY_SUCCESSORS && w);
    CHECK(mwt_sched_compare(b, 1, 2, 0, 0, 0, &w) == MWT_KEY_PRIORITY && w); /* ADDI 2 < LI 4, pass 1 */
    const MwtSchedBlock *b2 = &mwt_sched_pass(f, 2)->blocks.v[0];
    CHECK(mwt_sched_compare(b2, 1, 2, 0, 0, 0, &w) == MWT_KEY_ORDER && !w); /* pass 2: no key 4 */
    CHECK(mwt_sched_compare(b, 1, 1, 0, 0, 0, &w) == MWT_KEY_HEIGHT && w);
    CHECK(mwt_sched_compare(b, 1, 2, 0, -1, -1, &w) == MWT_KEY_NONE);
    CHECK(!strcmp(mwt_sched_key_words(a, b, 1, MWT_KEY_PRIORITY, 2, 0, 0, 0), "priority 2 < 4"));
}

AVP_TEST(self_check_agrees) {
    AvpArena *a = avp_arena();
    MwtSchedTrace t;
    const MwtSchedFunc *f = parse(a, LOG, &t);
    MwtSchedCheck c = {0};
    mwt_sched_check_trace(a, f, &c);
    AvpStr rows[5];
    for (int i = 0; i < 5; i++) rows[i] = avp_str(ROWS_Z[i]);
    mwt_sched_check_object(a, f, rows, 5, &c);
    CHECK_EQ_I64(c.gaps.n, 0);
    CHECK_EQ_I64(c.ops_checked, 5);
    CHECK_EQ_I64(c.regs_checked, 2); /* r3, r4 (r33 is virtual: not compared) */
    CHECK(has_line(&c.notes, "1 emitted instructions are in blocks pass 2 did not schedule"));
}

AVP_TEST(self_check_trace_gap) {
    AvpArena *a = avp_arena();
    MwtSchedTrace t;
    /* the logged take disagrees with the numbers */
    const MwtSchedFunc *f = parse(a, replace(a, LOG, "T 0x20\nI 0x20\n", "I 0x10\n"), &t);
    MwtSchedCheck c = {0};
    mwt_sched_check_trace(a, f, &c);
    CHECK(has_gap(&c, "trace", "the numbers say taken by critical"));
    CHECK_EQ_I64(c.gaps.v[0].pass, 1);
    CHECK_EQ_I64(c.gaps.v[0].block, 0);
    /* a node issued that the trace's own edges do not make ready */
    MwtSchedTrace t2;
    const MwtSchedFunc *f2 = parse(a, replace(a, LOG, "S 1 0\nCI 0x30 1\n", "S 1 0\nCI 0x30 1\nCI 0x40 0\n"), &t2);
    MwtSchedCheck c2 = {0};
    mwt_sched_check_trace(a, f2, &c2);
    CHECK(has_gap(&c2, "trace", "the trace's edges make #2 ready, the selection looked at #2 #3"));
    /* the emitted instructions are not the object's */
    MwtSchedTrace t3;
    const MwtSchedFunc *f3 = parse(a, LOG, &t3);
    AvpStr rows[5] = {AVP_S("blr"), AVP_S("lis r33, table@ha"), AVP_S("lwz r3, 0x0(r4)"), AVP_S("addi r4, r33, 0x1"),
                      AVP_S("bl g")};
    MwtSchedCheck c3 = {0};
    mwt_sched_check_object(a, f3, rows, 5, &c3);
    CHECK(has_gap(&c3, "trace", "emitted instruction 2 is LI"));
    MwtSchedCheck c4 = {0};
    mwt_sched_check_object(a, f3, NULL, 0, &c4);
    CHECK(has_gap(&c4, "trace", "no code"));
}

AVP_TEST(self_check_after_scheduler_gap) {
    AvpArena *a = avp_arena();
    MwtSchedTrace t;
    /* a later pass put ADDI before LI */
    const char *log = replace(a, LOG, "AS 0x1010 LI 0xb0\nAS 0x1030 ADDI 0xb0\n", "AS 0x1030 ADDI 0xb0\nAS 0x1010 LI 0xb0\n");
    const MwtSchedFunc *f = parse(a, log, &t);
    AvpStr rows[5] = {AVP_S("blr"), AVP_S("lis r33, table@ha"), AVP_S("addi r4, r33, table@l"), AVP_S("li r3, 0x0"),
                      AVP_S("bl g")};
    MwtSchedCheck c = {0};
    mwt_sched_check_trace(a, f, &c);
    mwt_sched_check_object(a, f, rows, 5, &c);
    CHECK_EQ_I64(c.gaps.n, 1);
    CHECK(has_gap(&c, "after-scheduler", "output position 2"));
    CHECK_EQ_I64(c.gaps.v[0].pos, 2);
    /* a later pass deleted LI: accounted, not a gap */
    MwtSchedTrace t2;
    const MwtSchedFunc *f2 = parse(a, replace(a, LOG, "AS 0x1010 LI 0xb0\n", ""), &t2);
    AvpStr rows2[4] = {AVP_S("blr"), AVP_S("lis r33, table@ha"), AVP_S("addi r4, r33, table@l"), AVP_S("bl g")};
    MwtSchedCheck c2 = {0};
    mwt_sched_check_object(a, f2, rows2, 4, &c2);
    CHECK_EQ_I64(c2.gaps.n, 0);
    CHECK(has_line(&c2.notes, "#0 LI r3= deleted after the scheduler"));
}

AVP_TEST(opcode_matcher) {
    CHECK(mwt_sched_op_matches(AVP_S("ADDI"), AVP_S("li r3, 0x0")));
    CHECK(mwt_sched_op_matches(AVP_S("RLWINM"), AVP_S("clrlwi. r4, r9, 31")));
    CHECK(mwt_sched_op_matches(AVP_S("CMPLI"), AVP_S("cmplwi r0, 0x800")));
    CHECK(mwt_sched_op_matches(AVP_S("BC"), AVP_S("bne+ .L_1")));
    CHECK(mwt_sched_op_matches(AVP_S("SUBF"), AVP_S("subf. r10, r10, r9")));
    CHECK(!mwt_sched_op_matches(AVP_S("LWZ"), AVP_S("lfs f1, 0x0(r3)")));
    CHECK(!mwt_sched_op_matches(AVP_S("ADDI"), AVP_S("lis r3, 0x1")));
}

AVP_TEST(pair_query) {
    AvpArena *a = avp_arena();
    MwtSchedTrace t;
    const MwtSchedFunc *f = parse(a, LOG, &t);
    MwtSchedPair p;
    const char *why = NULL;
    CHECK(mwt_sched_pair(a, f, 1, 0, 0, 1, &p, &why));
    CHECK(p.first == 1 && p.second == 0 && p.both_ready && p.key == MWT_KEY_CRITICAL);
    CHECK_EQ_I64(p.cycle, 0);
    CHECK(avp_str_contains(p.cmp_words, AVP_S("#0 was the incumbent; #1 displaced it by critical")));
    CHECK(mwt_sched_pair(a, f, 1, 0, 2, 0, &p, &why));
    CHECK(p.first == 0 && !p.both_ready);
    CHECK(avp_str_contains(p.held, AVP_S("earliest cycle is 1: #1 LIS r33= issued at cycle 0, edge raw r33, latency 1")));
    CHECK(mwt_sched_pair(a, f, 1, 0, 3, 2, &p, &why));
    CHECK(avp_str_contains(p.held, AVP_S("#3 waits for #2 ADDI r4= r33 (edge waw r4, latency 0; not issued yet)")));
    AvpStr text = mwt_sched_pair_text(a, f, &p);
    CHECK(avp_str_contains(text, AVP_S("was not ready")));
    int blk = -1, node = -1;
    CHECK(mwt_sched_locate(f, 2, 3, &blk, &node));
    CHECK(blk == 0 && node == 2);
    CHECK(!mwt_sched_locate(f, 2, 0, &blk, &node)); /* the blr of an unscheduled block */
    CHECK(!mwt_sched_pair(a, f, 1, 5, 0, 1, &p, &why));
    CHECK(!mwt_sched_pair(a, f, 1, 0, 0, 0, &p, &why));
}

AVP_TEST(against) {
    AvpArena *a = avp_arena();
    MwtSchedTrace ta, tb;
    const MwtSchedFunc *fa = parse(a, LOG, &ta);
    AvpStrVec same = mwt_sched_diff(a, fa, fa);
    CHECK(has_line(&same, "no selection differs"));
    /* B issues LI first in pass 1 (the take missing) and has one node more */
    const char *lb = replace(a, LOG, "T 0x20\nI 0x20\n", "I 0x10\n");
    lb = replace(a, lb, "NC 0x10 0x1010 LI 1 0x0 0x0 4\n", "NC 0x10 0x1010 LI 1 0x0 0x0 4\nNC 0x50 0x1050 MR 1 0x0 0x0 0\n");
    lb = replace(a, lb, "N 0x40 0 2 3 2\n", "N 0x40 0 2 3 2\nN 0x50 1 1 0 2\n");
    const MwtSchedFunc *fb = parse(a, lb, &tb);
    AvpStrVec d = mwt_sched_diff(a, fa, fb);
    CHECK(has_line(&d, "node only in B: #4 MR"));
    CHECK(has_line(&d, "first difference: pass 1 block 0 (index 1 / 1), selection 0, cycle 0 slot 0"));
    CHECK(has_line(&d, "(the block's node sets differ)"));
    CHECK(has_line(&d, "A: issued #1 LIS r33=; ready: #0 #1; #0: #0 was the incumbent; #1 displaced it by critical"));
    CHECK(has_line(&d, "B: issued #0 LI; ready: #0 #1")); /* the OP line after the inserted NC goes to MR */
}

AVP_TEST(trace_errors_are_kept) {
    AvpArena *a = avp_arena();
    MwtSchedTrace t;
    mwt_sched_parse(a, AVP_S("FN 0x1 1 f\nHANDLER ERR KeyError: x (spec:3, bp 0x1; this handler is stopped)\n"), &t);
    CHECK_EQ_I64(t.errors.n, 1);
    const char *why = NULL;
    CHECK(mwt_sched_find(a, &t, "f", &why) == NULL); /* never scheduled */
}

AVP_TEST_MAIN()
