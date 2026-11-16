/* Tests of avp_pyintset_copy / avp_pyintset_update / avp_pyintset_or (CPython's set_merge): the iteration order of
 * {f} | A | B | C for 300 generated cases, expected values from CPython 3.14.8 (map_pyintset_or_corpus.h; the
 * generator is a Python snippet: random.Random(20261103), lists of addresses added in order to a set, the listed
 * values discarded afterwards, then list({f} | A | B | C)). */
#include "avp/map.h"
#include "avp/test.h"
#include "map_pyintset_or_corpus.h"

static AvpPyIntSet *build(const long long *add, int na, const long long *del, int nd) {
    AvpPyIntSet *s = avp_pyintset_new();
    for (int i = 0; i < na; i++) avp_pyintset_add(s, add[i]);
    if (na > 4)
        for (int i = 0; i < nd; i++) avp_pyintset_discard(s, del[i]);
    return s;
}

AVP_TEST(pyintset_or_matches_cpython) {
    for (size_t k = 0; k < sizeof or_cases / sizeof or_cases[0]; k++) {
        const OrCase *c = &or_cases[k];
        AvpPyIntSet *a = build(c->a, c->na, c->da, c->nda), *b = build(c->b, c->nb, c->db, c->ndb),
                    *cc = build(c->c, c->nc, c->dc, c->ndc);
        AvpPyIntSet *lit = avp_pyintset_new();
        avp_pyintset_add(lit, c->f);
        AvpPyIntSet *r = avp_pyintset_or(lit, a);
        AvpPyIntSet *r2 = avp_pyintset_or(r, b);
        AvpPyIntSet *r3 = avp_pyintset_or(r2, cc);
        size_t pos = 0, n = 0;
        i64 m;
        bool ok = avp_pyintset_len(r3) == (size_t)c->nr;
        while (avp_pyintset_next(r3, &pos, &m)) {
            if (n >= (size_t)c->nr || m != c->r[n]) ok = false;
            n++;
        }
        CHECK_MSG(ok && n == (size_t)c->nr, "case %zu", k);
    }
}

AVP_TEST_MAIN("map_pyintset_or")
