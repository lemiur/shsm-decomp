/* sibling: proposed unit ranges (sibling_units.c) on hand-built pairings, and the UNUSED positions of the map. */
#include "avp/test.h"
#include "sibling.h"

#include <string.h>

static SibFn *add(SibFnVec *v, i64 address, i64 size, const char *name, const char *object) {
    SibFn f = {0};
    f.address = address;
    f.size = size;
    f.name = name;
    f.object = object;
    f.lib = object ? "L.a" : "retail";
    f.order = -1;
    f.peer = -1;
    avp_vec_push(v, f);
    return &v->v[v->n - 1];
}

static void pair(SibFnVec *s, size_t i, SibFnVec *g, size_t j) {
    s->v[i].peer = (i64)j, s->v[i].tier = SIB_EXACT;
    g->v[j].peer = (i64)i, g->v[j].tier = SIB_EXACT;
}

/* Size and map capacity only suggest an owner: such a function is a candidate and stays out of the proposed range. */
AVP_TEST(gap_by_size_and_capacity) {
    AvpArena *a = avp_arena();
    SibFnVec s = {0}, g = {0};
    add(&s, 0x1000, 0x40, "A1", NULL);
    add(&s, 0x1040, 0x30, "fn_00001040", NULL); /* a's static: same size as a_static */
    add(&s, 0x1070, 0x80, "fn_00001070", NULL); /* b's first member, another revision: size close to b_head */
    add(&s, 0x10F0, 0x20, "B1", NULL);
    add(&s, 0x1110, 0x20, "B2", NULL);
    add(&g, 0x9000, 0x40, "A1", "a.o")->order = 0;
    add(&g, 0x9040, 0x30, "a_static", "a.o")->order = 1;
    add(&g, 0x9100, 0x84, "b_head", "b.o")->order = 0;
    add(&g, 0x9184, 0x20, "B1", "b.o")->order = 1;
    add(&g, 0x91A4, 0x20, "B2", "b.o")->order = 2;
    pair(&s, 0, &g, 0);
    pair(&s, 3, &g, 3);
    pair(&s, 4, &g, 4);
    SibSpanRef spans[2] = {{"L.a", "a.o", 0, 0, true}, {"L.a", "b.o", 3, 4, true}};
    SibProposal *p = sibling_propose_units(a, &s, &g, NULL, NULL, spans, 2, 1u << SIB_EXACT);
    CHECK(p[0].proposed && p[1].proposed);
    CHECK(p[0].start == 0x1000 && p[0].end == 0x1040);
    CHECK(p[1].start == 0x10F0 && p[1].end == 0x1130);
    REQUIRE(p[0].after.n == 1);
    CHECK(p[0].after.v[0].candidate && !p[0].after.v[0].mine && !p[0].after.v[0].assigned);
    CHECK(strstr(p[0].after.v[0].why, "candidate (size only)") != NULL && strstr(p[0].after.v[0].why, "a_static") != NULL);
    REQUIRE(p[1].before.n == 1);
    CHECK(p[1].before.v[0].candidate && !p[1].before.v[0].mine);
}

/* A call edge to a paired function of the object assigns; a function between two assigned ones is assigned; the
 * candidate beyond the last assigned one is left out and names the other object when its map has a near size. */
AVP_TEST(gap_assigned_by_call_edge_and_alternative) {
    AvpArena *a = avp_arena();
    SibFnVec s = {0}, g = {0};
    add(&s, 0x1000, 0x40, "A1", NULL);
    SibFn *st = add(&s, 0x1040, 0x30, "fn_00001040", NULL); /* calls A1: a.o's */
    static const char *calls_a1[] = {"A1"};
    st->callees = calls_a1, st->ncallees = 1;
    add(&s, 0x1070, 0x30, "fn_00001070", NULL); /* size only: a_static 0x30, b_near 0x32 */
    add(&s, 0x10A0, 0x20, "B1", NULL);
    add(&g, 0x9000, 0x40, "A1", "a.o")->order = 0;
    add(&g, 0x9040, 0x30, "a_static", "a.o")->order = 1;
    add(&g, 0x9100, 0x32, "b_near", "b.o")->order = 0;
    add(&g, 0x9132, 0x20, "B1", "b.o")->order = 1;
    pair(&s, 0, &g, 0);
    pair(&s, 3, &g, 3);
    SibSpanRef spans[2] = {{"L.a", "a.o", 0, 0, true}, {"L.a", "b.o", 3, 3, true}};
    SibProposal *p = sibling_propose_units(a, &s, &g, NULL, NULL, spans, 2, 1u << SIB_EXACT);
    REQUIRE(p[0].after.n == 2);
    CHECK(p[0].after.v[0].mine && p[0].after.v[0].assigned);
    CHECK(strstr(p[0].after.v[0].why, "calls A1") != NULL);
    CHECK(p[0].after.v[1].candidate && !p[0].after.v[1].mine);
    CHECK(strstr(p[0].after.v[1].why, "alternative: b.o, its unpaired b_near") != NULL);
    CHECK(p[0].end == 0x1070); /* the range ends at the last assigned function */
}

AVP_TEST(gap_unused_entries_and_ambiguity) {
    AvpArena *a = avp_arena();
    SibFnVec s = {0}, g = {0};
    add(&s, 0x1000, 0x40, "A1", NULL);
    add(&s, 0x1040, 0x10, "fn_00001040", NULL); /* no size evidence; both objects have room for one: ambiguous */
    add(&s, 0x1050, 0x20, "B1", NULL);
    add(&g, 0x9000, 0x40, "A1", "a.o")->order = 0;
    add(&g, 0x9100, 0x20, "B1", "b.o")->order = 0;
    pair(&s, 0, &g, 0);
    pair(&s, 2, &g, 1);
    SibMapSymVec unused = {0};
    SibMapSym ua = {0, 0x50, "a_dropped", "L.a", "a.o", 0, 1}; /* after A1 */
    SibMapSym ub = {0, 0x60, "b_dropped", "L.a", "b.o", 0, 0}; /* before B1 */
    avp_vec_push(&unused, ua);
    avp_vec_push(&unused, ub);
    SibSpanRef spans[2] = {{"L.a", "a.o", 0, 0, true}, {"L.a", "b.o", 2, 2, true}};
    SibProposal *p = sibling_propose_units(a, &s, &g, &unused, NULL, spans, 2, 1u << SIB_EXACT);
    size_t amb = 0;
    for (size_t k = 0; k < p[0].after.n; k++) amb += !p[0].after.v[k].assigned;
    for (size_t k = 0; k < p[1].before.n; k++) amb += !p[1].before.v[k].assigned;
    CHECK(amb >= 1);
    /* no evidence of its own: neither range takes it */
    CHECK(p[0].end == 0x1040 && p[1].start == 0x1050);
    /* without UNUSED entries neither object has room: it is left to neither */
    SibProposal *q = sibling_propose_units(a, &s, &g, NULL, NULL, spans, 2, 1u << SIB_EXACT);
    CHECK(q[0].end == 0x1040 && q[1].start == 0x1050);
    avp_vec_free(&unused);
}

AVP_TEST(gap_unclean_span_not_proposed) {
    AvpArena *a = avp_arena();
    SibFnVec s = {0}, g = {0};
    add(&s, 0x1000, 0x40, "A1", NULL);
    add(&g, 0x9000, 0x40, "A1", "a.o")->order = 0;
    pair(&s, 0, &g, 0);
    SibSpanRef spans[1] = {{"L.a", "a.o", 0, 0, false}};
    SibProposal *p = sibling_propose_units(a, &s, &g, NULL, NULL, spans, 1, 1u << SIB_EXACT);
    CHECK(!p[0].proposed);
    CHECK(p[0].start == 0x1000 && p[0].end == 0x1040);
}

AVP_TEST(map_unused_position) {
    AvpArena *a = avp_arena();
    static const char MAP[] = ".text section layout\n"
                              "  00000000 000044 80008140 00004320  4 f0 \tL.a x.o\n"
                              "  UNUSED   000060 ........ ........    dropped L.a x.o\n"
                              "  00000044 000004 80008184 00004364  4 f1 \tL.a x.o\n"
                              "  UNUSED   000010 ........ ........    first L.a y.o\n";
    SibMap *m = sibling_parse_map(a, avp_str(MAP));
    REQUIRE(m->unused.n == 2);
    CHECK(m->unused.v[0].before == 1); /* after f0, before f1 */
    CHECK(m->unused.v[1].before == 0);
}

AVP_TEST_MAIN("sibling")
