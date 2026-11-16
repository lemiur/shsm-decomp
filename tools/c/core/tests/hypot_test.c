/* avp_math_hypot against CPython 3.14.8's math.hypot (corpus in hypot_corpus.h, generator in its header). */
#include "avp/avp.h"
#include "avp/fmt.h"
#include "avp/test.h"
#include "hypot_corpus.h"
#include <math.h>

AVP_TEST(hypot_matches_cpython_bit_for_bit) {
    size_t n = sizeof hypot_corpus / sizeof hypot_corpus[0];
    size_t bad = 0;
    for (size_t i = 0; i < n; i++) {
        const HypotCase *c = &hypot_corpus[i];
        double got = avp_math_hypot(c->x, (size_t)c->n);
        bool same = isnan(c->want) ? isnan(got) : memcmp(&got, &c->want, sizeof got) == 0;
        if (!same) {
            if (bad++ < 5) fprintf(stderr, "case %zu: got %a want %a\n", i, got, c->want);
        }
    }
    CHECK_EQ_I64(bad, 0);
    CHECK(n > 300);
}

AVP_TEST(hypot_small_cases) {
    double three_four[] = {3, 0, 4};
    CHECK_EQ_F64(avp_math_hypot(three_four, 3), 5.0);
    CHECK_EQ_F64(avp_math_hypot(three_four, 0), 0.0);
    double single[] = {-2.5};
    CHECK_EQ_F64(avp_math_hypot(single, 1), 2.5);
}

AVP_TEST_MAIN("hypot")
