/* shsm_cache: key_of values, the entry format and the cache store. */
#include "shsm_cache.h"
#include "shsm_test_util.h"

AVP_TEST(key_of_vectors) {
    AvpArena *a = avp_arena();
    ShsmKey k;
    shsm_key_init(&k); /* key_of("Wii/1.0", ["-O4,p", "-c"], "mwcc_sjis") */
    shsm_key_strz(&k, "Wii/1.0");
    AvpStr fl[] = {AVP_S("-O4,p"), AVP_S("-c")};
    shsm_key_str_list(&k, fl, 2);
    shsm_key_strz(&k, "mwcc_sjis");
    CHECK_CZ(shsm_key_hex(a, &k), "9a67a398b91a480aede797ecda85e6c73fca8f68dd354ac85713b7086b100a5f");
    shsm_key_init(&k); /* key_of(None, None, None) */
    shsm_key_none(&k);
    shsm_key_strz(&k, NULL);
    shsm_key_none(&k);
    CHECK_CZ(shsm_key_hex(a, &k), "cb60907bfb8ecc942a6731e11186e77e267e7841cd143b2f62716527491bcbd8");
    shsm_key_init(&k); /* key_of("match-v1", "manifest", ["a", "b\\c"], [("x.exe", "ab"), ("y.exe", "cd")], "ff") */
    shsm_key_strz(&k, "match-v1");
    shsm_key_strz(&k, "manifest");
    AvpStr norm[] = {AVP_S("a"), AVP_S("b\\c")};
    shsm_key_str_list(&k, norm, 2);
    AvpStr f[] = {AVP_S("x.exe"), AVP_S("y.exe")}, s[] = {AVP_S("ab"), AVP_S("cd")};
    shsm_key_repr(&k, shsm_repr_str_pairs(a, f, s, 2));
    shsm_key_strz(&k, "ff");
    CHECK_CZ(shsm_key_hex(a, &k), "70295a7f7ee9abeb80cf40a2f1445a1e37a7ab8d2c35ab706fd5bbb849be1d97");
    shsm_key_init(&k); /* key_of(b"\x00\x01", [], ["one"], 5, "é") */
    shsm_key_bytes(&k, "\x00\x01", 2);
    shsm_key_str_list(&k, NULL, 0);
    AvpStr one[] = {AVP_S("one")};
    shsm_key_str_list(&k, one, 1);
    shsm_key_int(&k, 5);
    shsm_key_str(&k, AVP_S("\xc3\xa9"));
    CHECK_CZ(shsm_key_hex(a, &k), "4c72a99a3bbc1ce87b3c95f08521411c68fd78a6d15021d066e9b7582400e2cc");
    CHECK_STR(shsm_repr_str_tuple(a, one, 1), "('one',)");
    CHECK_STR(shsm_repr_str_pairs(a, NULL, NULL, 0), "()");
}

AVP_TEST(cache_roundtrip) {
    const char *dir = fx_dir("cache dir");
    ShsmCache *c = shsm_cache_new(avp_join(avp_arena(), dir, "store"));
    const char *key = "ab0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcd";
    AvpStr got;
    CHECK(!shsm_cache_get(avp_arena(), c, key, &got));
    const char *p = shsm_cache_put(c, key, "hello", 5);
    CHECK_CZ(p, avp_join(avp_arena(), dir, "store", "ab", key));
    AvpBytes raw = {0};
    REQUIRE(avp_read_all(NULL, p, &raw));
    CHECK_EQ_U64(raw.n, 5 + 48);
    CHECK_MEM(raw.v + 5, "SHSMCACHE1\0\0\0\0\0\0\0\x05", 18);
    REQUIRE(shsm_cache_get(avp_arena(), c, key, &got));
    CHECK_STR(got, "hello");
    const char *dst = avp_join(avp_arena(), dir, "out dir", "sub", "x.o");
    CHECK(shsm_cache_restore(c, key, dst));
    AvpStr text;
    REQUIRE(avp_read_text(avp_arena(), dst, AVP_TM_UTF8, &text));
    CHECK_STR(text, "hello");
    CHECK_EQ_I64(c->hits, 2);
    CHECK_EQ_I64(c->misses, 1);
    CHECK_EQ_I64(c->stores, 1);
    /* a damaged entry is a miss and is removed */
    raw.v[0] ^= 1;
    REQUIRE(avp_write_file(p, raw.v, raw.n));
    CHECK(!shsm_cache_contains(c, key));
    CHECK(!avp_exists(p));
    CHECK_RAISES(shsm_file_digest(avp_arena(), avp_join(avp_arena(), dir, "nope.txt")), SHSM_EXC_FILE_NOT_FOUND, NULL);
    CHECK_CZ(shsm_file_digest(avp_arena(), dst), "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
}

AVP_TEST_MAIN("shsm_cache")
