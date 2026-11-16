/* str.casefold(): a CPython corpus (casefold_corpus.h) and the digest of the fold of every code point. */
#include "avp/avp.h"
#include "avp/test.h"

#include "casefold_corpus.h"

AVP_TEST(casefold_corpus) {
    AvpArena *a = avp_scratch();
    for (size_t i = 0; i < AVP_ARRAY_LEN(casefold_cases); i++) {
        AvpStr got = avp_str_casefold(a, avp_str(casefold_cases[i].in));
        CHECK_MSG(avp_str_eqz(got, casefold_cases[i].want), "case %zu: got \"%.*s\", want \"%s\"", i, AVP_SF(got),
                  casefold_cases[i].want);
    }
}

AVP_TEST(casefold_every_code_point) {
    u64 h = 0xcbf29ce484222325ULL;
    for (u32 cp = 0; cp < 0x110000; cp++) {
        if (cp >= 0xD800 && cp <= 0xDFFF) continue;
        char in[5];
        size_t n = avp_u8_put(in, cp);
        AvpArena *a = avp_scratch();
        AvpArenaMark mk = avp_arena_mark(a);
        AvpStr got = avp_str_casefold(a, avp_strn(in, n));
        for (size_t i = 0; i < got.n; i++) h = (h ^ (u8)got.p[i]) * 0x100000001b3ULL;
        avp_arena_reset(a, mk);
    }
    CHECK(h == casefold_all_digest);
}

AVP_TEST(casefold_matches_lower_on_ascii) {
    AvpArena *a = avp_scratch();
    CHECK_STR(avp_str_casefold(a, AVP_S("AbC-Z_9")), "abc-z_9");
    CHECK_STR(avp_str_casefold(a, AVP_S("\xC3\x9F")), "ss");
    CHECK_STR(avp_str_casefold(a, AVP_S("\xCF\x82")), "\xCF\x83");
}
AVP_TEST_MAIN("str_casefold")
