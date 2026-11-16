/* sibling: the pairing tiers on hand-built function lists. */
#include "avp/test.h"
#include "sibling.h"

#include <string.h>

/* fn(v, name, fp, nrows, size, object, callees...) appended at the next address */
static void fn(SibFnVec *v, const char *name, const char *fp, i64 nrows, i64 size, const char *object,
               const char *c1, const char *c2) {
    AvpArena *a = avp_arena();
    SibFn f = {0};
    f.address = 0x80000000 + (i64)v->n * 0x100;
    f.name = name;
    strncpy(f.exact, fp, 20);
    strncpy(f.ref, fp, 20);
    f.nrows = nrows;
    f.size = size;
    f.object = object;
    f.lib = "";
    f.peer = -1;
    size_t n = (c1 != NULL) + (c2 != NULL);
    if (n) {
        f.callees = avp_alloc(a, n * sizeof *f.callees);
        f.callees[0] = c1 ? c1 : c2;
        if (n == 2) f.callees[1] = c2;
        f.ncallees = n;
    }
    avp_vec_push(v, f);
}

static int tier_of(const SibFnVec *v, const char *name) {
    for (size_t i = 0; i < v->n; i++)
        if (strcmp(v->v[i].name, name) == 0) return v->v[i].tier;
    return -1;
}

static const char *peer_of(const SibFnVec *s, const SibFnVec *g, const char *name) {
    for (size_t i = 0; i < s->n; i++)
        if (strcmp(s->v[i].name, name) == 0) return s->v[i].peer >= 0 ? g->v[s->v[i].peer].name : NULL;
    return NULL;
}

AVP_TEST(exact_and_gap) {
    SibFnVec s = {0}, g = {0};
    SibPairCounts c;
    fn(&s, "fn_A", "A", 10, 40, "u", NULL, NULL);
    fn(&s, "fn_t1", "tiny", 2, 8, "u", NULL, NULL);   /* small: only through the gap */
    fn(&s, "fn_d1", "dup", 8, 32, "u", NULL, NULL);
    fn(&s, "fn_d2", "dup", 8, 32, "u", NULL, NULL);
    fn(&s, "fn_B", "B", 10, 40, "u", NULL, NULL);
    fn(&s, "fn_lone", "tiny2", 2, 8, "u", NULL, NULL); /* small and unique, no gap: unpaired */
    fn(&g, "A", "A", 10, 40, "o.obj", NULL, NULL);
    fn(&g, "t1", "tiny", 2, 8, "o.obj", NULL, NULL);
    fn(&g, "d1", "dup", 8, 32, "o.obj", NULL, NULL);
    fn(&g, "d2", "dup", 8, 32, "o.obj", NULL, NULL);
    fn(&g, "B", "B", 10, 40, "o.obj", NULL, NULL);
    fn(&g, "lone", "tiny2", 2, 8, "o.obj", NULL, NULL);
    sibling_pair(&s, &g, &c);
    CHECK(tier_of(&s, "fn_A") == SIB_EXACT);
    CHECK(tier_of(&s, "fn_B") == SIB_EXACT);
    CHECK(tier_of(&s, "fn_t1") == SIB_EXACT_SEQ);
    CHECK(tier_of(&s, "fn_d1") == SIB_EXACT_SEQ);
    CHECK_CZ(peer_of(&s, &g, "fn_d1"), "d1");
    CHECK_CZ(peer_of(&s, &g, "fn_d2"), "d2");
    CHECK(tier_of(&s, "fn_lone") == SIB_NONE);
    CHECK(c.exact == 2 && c.exact_seq == 3);
    avp_vec_free(&s);
    avp_vec_free(&g);
}

AVP_TEST(gap_ambiguity_and_crossing) {
    SibFnVec s = {0}, g = {0};
    SibPairCounts c;
    fn(&s, "fn_A", "A", 10, 40, "u", NULL, NULL);
    fn(&s, "fn_d1", "dup", 8, 32, "u", NULL, NULL);
    fn(&s, "fn_d2", "dup", 8, 32, "u", NULL, NULL);
    fn(&s, "fn_x", "X", 2, 8, "u", NULL, NULL);
    fn(&s, "fn_y", "Y", 2, 8, "u", NULL, NULL);
    fn(&s, "fn_B", "B", 10, 40, "u", NULL, NULL);
    fn(&g, "A", "A", 10, 40, "o.obj", NULL, NULL);
    fn(&g, "d1", "dup", 8, 32, "o.obj", NULL, NULL);
    fn(&g, "d2", "dup", 8, 32, "o.obj", NULL, NULL);
    fn(&g, "d3", "dup", 8, 32, "o.obj", NULL, NULL); /* three against two: ambiguous */
    fn(&g, "y", "Y", 2, 8, "o.obj", NULL, NULL);     /* X and Y swapped: the pairs would cross */
    fn(&g, "x", "X", 2, 8, "o.obj", NULL, NULL);
    fn(&g, "B", "B", 10, 40, "o.obj", NULL, NULL);
    sibling_pair(&s, &g, &c);
    CHECK(tier_of(&s, "fn_d1") == SIB_NONE);
    CHECK(tier_of(&s, "fn_d2") == SIB_NONE);
    CHECK(tier_of(&s, "fn_x") == SIB_NONE);
    CHECK(tier_of(&s, "fn_y") == SIB_NONE);
    avp_vec_free(&s);
    avp_vec_free(&g);
}

AVP_TEST(gap_needs_consecutive_anchors) {
    SibFnVec s = {0}, g = {0};
    SibPairCounts c;
    /* A and B are consecutive in the retail order but C sits between them in the sibling: no gap */
    fn(&s, "fn_A", "A", 10, 40, "u", NULL, NULL);
    fn(&s, "fn_t", "tiny", 2, 8, "u", NULL, NULL);
    fn(&s, "fn_B", "B", 10, 40, "u", NULL, NULL);
    fn(&s, "fn_C", "C", 10, 40, "u", NULL, NULL);
    fn(&g, "A", "A", 10, 40, "o.obj", NULL, NULL);
    fn(&g, "C", "C", 10, 40, "o.obj", NULL, NULL);
    fn(&g, "t", "tiny", 2, 8, "o.obj", NULL, NULL);
    fn(&g, "B", "B", 10, 40, "o.obj", NULL, NULL);
    sibling_pair(&s, &g, &c);
    CHECK(c.exact == 3);
    CHECK(tier_of(&s, "fn_t") == SIB_NONE);
    avp_vec_free(&s);
    avp_vec_free(&g);
}

AVP_TEST(calls_tier) {
    SibFnVec s = {0}, g = {0};
    SibPairCounts c;
    /* two identical wrappers in the sibling, one in retail; their callees are exact pairs */
    fn(&s, "fn_W", "wrap", 8, 32, "u", "fn_X1", NULL);
    fn(&s, "fn_X1", "X1", 20, 80, "u", NULL, NULL);
    fn(&s, "fn_X2", "X2", 20, 80, "u", NULL, NULL);
    fn(&g, "X2", "X2", 20, 80, "o.obj", NULL, NULL);
    fn(&g, "W2", "wrap", 8, 32, "o.obj", "X2", NULL);
    fn(&g, "X1", "X1", 20, 80, "p.obj", NULL, NULL);
    fn(&g, "W1", "wrap", 8, 32, "p.obj", "X1", NULL);
    sibling_pair(&s, &g, &c);
    CHECK(tier_of(&s, "fn_W") == SIB_CALLS);
    CHECK_CZ(peer_of(&s, &g, "fn_W"), "W1");
    CHECK(c.calls == 1);
    avp_vec_free(&s);
    avp_vec_free(&g);
}

AVP_TEST(callee_contradiction_undoes_exact) {
    SibFnVec s = {0}, g = {0};
    SibPairCounts c;
    fn(&s, "fn_P", "P", 10, 40, "u", "fn_Q", NULL);
    fn(&s, "fn_Q", "Q", 10, 40, "u", NULL, NULL);
    fn(&s, "fn_R", "R", 10, 40, "u", NULL, NULL);
    fn(&g, "P", "P", 10, 40, "o.obj", "R", NULL); /* calls R, but R pairs with fn_R, not with fn_Q */
    fn(&g, "Q", "Q", 10, 40, "o.obj", NULL, NULL);
    fn(&g, "R", "R", 10, 40, "o.obj", NULL, NULL);
    sibling_pair(&s, &g, &c);
    CHECK(tier_of(&s, "fn_P") == SIB_NONE);
    CHECK(tier_of(&s, "fn_Q") == SIB_EXACT);
    CHECK(c.rejected == 1);
    avp_vec_free(&s);
    avp_vec_free(&g);
}

AVP_TEST(seq_size_same_object_only) {
    SibFnVec s = {0}, g = {0};
    SibPairCounts c;
    fn(&s, "fn_A", "A", 10, 40, "u", NULL, NULL);
    fn(&s, "fn_m", "m1", 6, 24, "u", NULL, NULL);
    fn(&s, "fn_B", "B", 10, 40, "u", NULL, NULL);
    fn(&s, "fn_n", "n1", 6, 24, "u", NULL, NULL);
    fn(&s, "fn_C", "C", 10, 40, "u", NULL, NULL);
    fn(&s, "fn_k", "k1", 6, 24, "u", NULL, NULL);
    fn(&s, "fn_D", "D", 10, 40, "u", NULL, NULL);
    fn(&g, "A", "A", 10, 40, "o.obj", NULL, NULL);
    fn(&g, "m", "m2", 6, 24, "o.obj", NULL, NULL); /* same size, other body, same object: seq-size */
    fn(&g, "B", "B", 10, 40, "o.obj", NULL, NULL);
    fn(&g, "n", "n2", 7, 28, "o.obj", NULL, NULL); /* other size: nothing */
    fn(&g, "C", "C", 10, 40, "o.obj", NULL, NULL);
    fn(&g, "k", "k2", 6, 24, "p.obj", NULL, NULL); /* the gap spans two objects: nothing */
    fn(&g, "D", "D", 10, 40, "p.obj", NULL, NULL);
    sibling_pair(&s, &g, &c);
    CHECK(tier_of(&s, "fn_m") == SIB_SEQ_SIZE);
    CHECK(tier_of(&s, "fn_n") == SIB_NONE);
    CHECK(tier_of(&s, "fn_k") == SIB_NONE);
    CHECK(c.seq_size == 1);
    avp_vec_free(&s);
    avp_vec_free(&g);
}

AVP_TEST(initialisers_stay_unpaired) {
    SibFnVec s = {0}, g = {0};
    SibPairCounts c;
    fn(&s, "fn_A", "A", 10, 40, "u", NULL, NULL);
    fn(&s, "__sinit_X_cpp", "init", 12, 48, "u", NULL, NULL); /* same body as the sibling's, other data */
    fn(&s, "fn_ctor_entry", "init2", 12, 48, "u", NULL, NULL);
    s.v[2].initialiser = true;                                 /* named like a function, but a .ctors entry */
    fn(&s, "fn_B", "B", 10, 40, "u", NULL, NULL);
    fn(&g, "A", "A", 10, 40, "o.obj", NULL, NULL);
    fn(&g, "__sinit_Y_cpp", "init", 12, 48, "o.obj", NULL, NULL);
    fn(&g, "other", "init2", 12, 48, "o.obj", NULL, NULL);
    fn(&g, "B", "B", 10, 40, "o.obj", NULL, NULL);
    sibling_pair(&s, &g, &c);
    CHECK(tier_of(&s, "fn_A") == SIB_EXACT);
    CHECK(tier_of(&s, "__sinit_X_cpp") == SIB_NONE);
    CHECK(tier_of(&s, "fn_ctor_entry") == SIB_NONE);
    CHECK(tier_of(&g, "__sinit_Y_cpp") == SIB_NONE);
    CHECK(tier_of(&g, "other") == SIB_NONE);
    CHECK(c.exact == 2 && c.exact_seq == 0 && c.seq_size == 0);
    avp_vec_free(&s);
    avp_vec_free(&g);
}

AVP_TEST(fn_placeholders) {
    CHECK(sibling_is_fn_placeholder("fn_8032C1E0"));
    CHECK(sibling_is_fn_placeholder("fn_8032C1E0__Q24FMOD11AsyncThreadFv"));
    CHECK(!sibling_is_fn_placeholder("fn_8032C1E"));
    CHECK(!sibling_is_fn_placeholder("fn_8032C1E0x"));
    CHECK(!sibling_is_fn_placeholder("lbl_8032C1E0"));
    CHECK(!sibling_is_fn_placeholder("threadFunc__Q24FMOD11AsyncThreadFv"));
}

static void rfn(SibFnVec *v, i64 address, i64 size, const char *name) {
    SibFn f = {0};
    f.address = address;
    f.size = size;
    f.name = name;
    f.peer = -1;
    avp_vec_push(v, f);
}

AVP_TEST(resolve_names_and_addresses) {
    AvpArena *a = avp_arena();
    SibFnVec s = {0}, g = {0};
    rfn(&s, 0x8032C1E0, 0x2BC, "fn_8032C1E0__Q24FMOD11AsyncThreadFv");
    rfn(&s, 0x8032C49C, 0x10, "fn_8032C49C");
    rfn(&g, 0x80455F94, 0x2BC, "threadFunc__Q24FMOD11AsyncThreadFv");
    CHECK(sibling_resolve(a, &s, "0x8032C1E0", true) == 0);
    CHECK(sibling_resolve(a, &s, "8032C1E0", true) == 0);
    CHECK(sibling_resolve(a, &s, "0x8032C200", true) == 0);   /* inside the function */
    CHECK(sibling_resolve(a, &s, "fn_8032C1E0", true) == 0);  /* the placeholder without its mangled suffix */
    CHECK(sibling_resolve(a, &s, "fn_8032C49C", true) == 1);
    CHECK(sibling_resolve(a, &s, "fn_8032C1E0__Q24FMOD11AsyncThreadFv", true) == 0);
    CHECK(sibling_resolve(a, &s, "fn_8032C1E1", true) == 0);  /* an address inside, spelled as a placeholder */
    CHECK(sibling_resolve(a, &s, "0x80000000", true) == -1);
    CHECK(sibling_resolve(a, &s, "nosuch", true) == -1);
    CHECK(sibling_resolve(a, &g, "threadFunc__Q24FMOD11AsyncThreadFv", false) == 0);
    CHECK(sibling_resolve(a, &g, "FMOD::AsyncThread::threadFunc()", false) == 0);
    CHECK(sibling_resolve(a, &g, "0x80455F94", false) == -1); /* sibling addresses are not retail addresses */
    avp_vec_free(&s);
    avp_vec_free(&g);
}

AVP_TEST(special_names_and_thunks) {
    AvpArena *a = avp_arena();
    CHECK(sibling_is_ctor_dtor(a, "__ct__Q24FMOD15ChannelSoftwareFv"));
    CHECK(sibling_is_ctor_dtor(a, "__dt__Q24FMOD15ChannelSoftwareFv"));
    CHECK(sibling_is_ctor_dtor(a, "__dt__10CPlaneListFv"));
    CHECK(!sibling_is_ctor_dtor(a, "init__Q24FMOD15ChannelSoftwareFv"));
    CHECK(!sibling_is_ctor_dtor(a, "__ct__Fv"));                       /* no class: not a member */
    CHECK(!sibling_is_ctor_dtor(a, "@36@__dt__Q24FMOD15ChannelSoftwareFv")); /* a thunk is not the destructor */
    const char *t = sibling_thunk_note(a, "@36@__dt__Q25Audio11MaterialMapFv");
    CHECK(t && strstr(t, "offset 36") != NULL && strstr(t, "Audio::MaterialMap::~MaterialMap()") != NULL);
    CHECK(sibling_thunk_note(a, "__dt__Q25Audio11MaterialMapFv") == NULL);
    CHECK(sibling_thunk_note(a, "@x") == NULL);
}

AVP_TEST(same_named_sibling_function) {
    AvpArena *a = avp_arena();
    SibFnVec s = {0}, g = {0};
    rfn(&s, 0x80001000, 0x1108, "load__Q24FMOD11EventGroupIFPPv");
    rfn(&s, 0x80002000, 0x40, "fn_80002000");
    rfn(&s, 0x80003000, 0x20, "twice__Fv");
    rfn(&g, 0x80455F94, 0x1250, "load__Q24FMOD11EventGroupIFPPv");
    rfn(&g, 0x80456000, 0x40, "fn_80002000");
    rfn(&g, 0x80456100, 0x20, "twice__Fv");
    rfn(&g, 0x80456200, 0x24, "twice__Fv");
    size_t n = 0;
    CHECK(sibling_same_named(&g, "load__Q24FMOD11EventGroupIFPPv", &n) == 0 && n == 1);
    CHECK(sibling_same_named(&g, "fn_80002000", &n) == -1 && n == 0); /* a placeholder names nothing */
    CHECK(sibling_same_named(&g, "twice__Fv", &n) == 2 && n == 2);
    CHECK(sibling_same_named(&g, "absent__Fv", &n) == -1);
    const char *note = sibling_same_named_note(a, &s.v[0], &g);
    CHECK(note && strstr(note, "no exact pair; same-named sibling function exists (size 0x1250 vs 0x1108") != NULL);
    CHECK(note && strstr(note, "sibling.exe fn 'load__Q24FMOD11EventGroupIFPPv' --sibling") != NULL);
    CHECK(sibling_same_named_note(a, &s.v[1], &g) == NULL);
    note = sibling_same_named_note(a, &s.v[2], &g);
    CHECK(note && strstr(note, "2 same-named sibling functions") != NULL);
    avp_vec_free(&s);
    avp_vec_free(&g);
}

AVP_TEST(tier_names) {
    CHECK(sibling_tier_index("exact") == SIB_EXACT);
    CHECK(sibling_tier_index("exact-seq") == SIB_EXACT_SEQ);
    CHECK(sibling_tier_index("calls") == SIB_CALLS);
    CHECK(sibling_tier_index("seq-size") == SIB_SEQ_SIZE);
    CHECK(sibling_tier_index("bogus") == 0);
}

AVP_TEST_MAIN("sibling")
