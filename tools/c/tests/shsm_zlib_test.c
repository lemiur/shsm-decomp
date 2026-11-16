/* shsm_zlib.h: zlib byte equality with CPython 3.14's zlib (levels -1, 0, 1, 6, 9 over a corpus), the streaming objects
 * and the error texts (from another project of the author's tools/c/core/tests/zip_test.c at commit
 * 49e811503d7ba5c9cff92160557df057c50a8ab3, its zlib part), and the checksums. Expected values: shsm_zlib_corpus.h. */
#include "avp/avp.h"
#include "avp/hash.h"
#include "avp/test.h"
#include "shsm_zlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "shsm_zlib_corpus.h"
#include "shsm_zlib_py.h"

/* ---- the corpus generators of gen_zip.py ---- */
static u8 *gen_rand(size_t n, u64 seed) {
    u8 *out = (u8 *)avp_xmalloc(n ? n : 1);
    u64 x = seed;
    for (size_t i = 0; i < n; i++) {
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        out[i] = (u8)((x * 0x2545F4914F6CDD1Dull) >> 56);
    }
    return out;
}
static const char *const k_words[16] = {"the", "quick", "brown", "fox", "jumps", "over", "lazy", "dog",
                                        "hello", "world", "alpha", "beta", "gamma", "delta", "omega", "lorem"};
static u8 *gen_text(size_t n, u64 seed) {
    u8 *out = (u8 *)avp_xmalloc(n ? n : 1);
    u64 x = seed;
    size_t len = 0;
    while (len < n) {
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        const char *w = k_words[(x >> 33) & 15];
        for (; *w && len < n; w++) out[len++] = (u8)*w;
        if (len < n) out[len++] = ((x >> 20) & 7) ? ' ' : '\n';
    }
    return out;
}
static u8 *gen_rep(size_t n, u64 seed) {
    AVP_UNUSED(seed);
    u8 *out = (u8 *)avp_xmalloc(n ? n : 1);
    for (size_t i = 0; i < n; i++) out[i] = (u8)"abcabcabd"[i % 9];
    return out;
}
static u8 *gen_zero(size_t n, u64 seed) {
    AVP_UNUSED(seed);
    return (u8 *)avp_xcalloc(1, n ? n : 1);
}
static u8 *gen_mixed(size_t n, u64 seed) {
    u8 *r = gen_rand(n, seed), *t = gen_text(n, seed);
    for (size_t i = 0; i < n; i++)
        if ((i / 1000) % 4 != 0) r[i] = t[i];
    avp_xfree(t);
    return r;
}
static u8 *generate(int gen, size_t n, u64 seed) {
    switch (gen) {
    case 0: return gen_rand(n, seed);
    case 1: return gen_text(n, seed);
    case 2: return gen_rep(n, seed);
    case 3: return gen_zero(n, seed);
    default: return gen_mixed(n, seed);
    }
}

AVP_TEST(zlib_compress_equals_cpython) {
    AvpArena *a = avp_arena_new(0);
    u8 *data = NULL;
    int cur_gen = -1;
    unsigned cur_n = 0;
    u64 cur_seed = 0;
    for (size_t i = 0; i < AVP_ARRAY_LEN(k_zcases); i++) {
        const ZCase *c = &k_zcases[i];
        if (!data || c->gen != cur_gen || c->n != cur_n || c->seed != cur_seed) {
            avp_xfree(data);
            data = generate(c->gen, c->n, c->seed);
            cur_gen = c->gen;
            cur_n = c->n;
            cur_seed = c->seed;
        }
        AvpArenaMark mark = avp_arena_mark(a);
        size_t cn = 0;
        u8 *z = shsm_zlib_compress(a, data, c->n, c->level, 15, &cn);
        REQUIRE(z != NULL);
        CHECK_MSG(cn == c->clen, "%s n=%u level %d: length %zu, CPython %u", k_gen_names[c->gen], c->n, c->level, cn, c->clen);
        CHECK_MSG(strcmp(avp_sha256_hex(a, z, cn), c->sha256) == 0, "%s n=%u level %d: bytes differ from CPython",
                  k_gen_names[c->gen], c->n, c->level);
        size_t dn = 0;
        ShsmZErr err;
        u8 *back = shsm_zlib_decompress(a, z, cn, 15, c->n, &dn, &err);
        CHECK_MSG(back != NULL && dn == c->n && memcmp(back, data, dn) == 0, "%s n=%u level %d: round trip", k_gen_names[c->gen],
                  c->n, c->level);
        avp_arena_reset(a, mark);
    }
    avp_xfree(data);
    avp_arena_free(a);
}

AVP_TEST(zlib_invalid_parameters) {
    AvpArena *a = avp_arena_new(0);
    size_t n;
    CHECK(shsm_zlib_compress(a, "abc", 3, 10, 15, &n) == NULL);
    CHECK(shsm_zlib_compress(a, "abc", 3, -2, 15, &n) == NULL);
    CHECK(shsm_zlib_compress(a, "abc", 3, 6, 7, &n) == NULL);
    CHECK(shsm_zcomp_new(6, 99) == NULL);
    CHECK(shsm_zdecomp_new(99) == NULL);
    avp_arena_free(a);
}

AVP_TEST(compressobj_streams_equal_cpython) {
    u8 *data = gen_text(300000, 41);
    for (size_t i = 0; i < AVP_ARRAY_LEN(k_zstreams); i++) {
        const ZStreamCase *c = &k_zstreams[i];
        ShsmZComp *o = shsm_zcomp_new(c->level, c->wbits);
        REQUIRE(o != NULL);
        AvpBytes out = {0};
        for (int k = 0; k < 30; k++) {
            CHECK(shsm_zcomp_compress(o, data + (size_t)k * 10000, 10000, &out));
            if (k % 7 == 6) CHECK(shsm_zcomp_flush(o, AVP_Z_SYNC_FLUSH, &out));
        }
        CHECK(shsm_zcomp_flush(o, AVP_Z_FINISH, &out));
        shsm_zcomp_free(o);
        AvpArena *a = avp_arena_new(0);
        CHECK_MSG(out.n == c->clen && strcmp(avp_sha256_hex(a, out.v, out.n), c->sha256) == 0,
                  "stream level %d wbits %d: %zu bytes (CPython %u)", c->level, c->wbits, out.n, c->clen);
        avp_arena_free(a);
        /* and it decompresses with the matching window bits */
        ShsmZDecomp *d = shsm_zdecomp_new(c->wbits == 31 ? 31 : c->wbits);
        AvpBytes back = {0};
        ShsmZErr err;
        CHECK(shsm_zdecomp_decompress(d, out.v, out.n, 0, &back, &err));
        CHECK(shsm_zdecomp_eof(d));
        CHECK(back.n == 300000 && memcmp(back.v, data, 300000) == 0);
        shsm_zdecomp_free(d);
        avp_vec_free(&back);
        avp_vec_free(&out);
    }
    avp_xfree(data);
}

AVP_TEST(decompress_error_texts) {
    AvpArena *a = avp_arena_new(0);
    size_t gn;
    u8 *msg = (u8 *)avp_alloc(a, 110);
    for (int i = 0; i < 10; i++) memcpy(msg + 11 * i, "hello world", 11);
    u8 *good = shsm_zlib_compress(a, msg, 110, -1, 15, &gn);
    REQUIRE(good != NULL);
    REQUIRE(gn == AVP_ARRAY_LEN(k_ztrunc));
    for (size_t cut = 0; cut < gn; cut++) {
        size_t on;
        ShsmZErr err;
        u8 *r = shsm_zlib_decompress(a, good, cut, 15, 0, &on, &err);
        CHECK_MSG(r == NULL, "cut %zu decompressed", cut);
        CHECK_MSG(err.code == k_ztrunc[cut].code && strcmp(err.msg, k_ztrunc[cut].msg) == 0, "cut %zu: \"%s\" (%d), CPython \"%s\"", cut,
                  err.msg, err.code, k_ztrunc[cut].msg);
    }
    u8 *bad[3];
    for (int k = 0; k < 3; k++) bad[k] = (u8 *)avp_memdup(a, good, gn);
    bad[0][0] ^= 1;
    bad[1][2] ^= 0xFF;
    bad[2][gn - 1] ^= 1;
    for (int k = 0; k < 3; k++) {
        size_t on;
        ShsmZErr err;
        CHECK(shsm_zlib_decompress(a, bad[k], gn, 15, 0, &on, &err) == NULL);
        CHECK_MSG(err.code == k_zbad[k].code && strcmp(err.msg, k_zbad[k].msg) == 0, "bad %d: \"%s\", CPython \"%s\"", k, err.msg,
                  k_zbad[k].msg);
    }
    /* trailing garbage after the end of the stream is ignored, as zlib.decompress does */
    u8 *tail = (u8 *)avp_alloc(a, gn + 5);
    memcpy(tail, good, gn);
    memcpy(tail + gn, "trail", 5);
    size_t on;
    u8 *r = shsm_zlib_decompress(a, tail, gn + 5, 15, 0, &on, NULL);
    CHECK(r != NULL && on == 110 && memcmp(r, msg, 110) == 0);
    avp_arena_free(a);
}

AVP_TEST(decompressobj_max_length_and_tail) {
    u8 *r = gen_rand(3000, 61), *t = gen_text(60000, 62);
    u8 *src = (u8 *)avp_xmalloc(63000);
    memcpy(src, r, 3000);
    memcpy(src + 3000, t, 60000);
    avp_xfree(r);
    avp_xfree(t);
    AvpArena *a = avp_arena_new(0);
    size_t cn;
    u8 *comp = shsm_zlib_compress(a, src, 63000, 6, 15, &cn);
    REQUIRE(comp != NULL);
    ShsmZDecomp *d = shsm_zdecomp_new(15);
    AvpBytes all = {0};
    AvpBytes feed = {0};
    avp_vec_extend(&feed, comp, 5000);
    size_t steps = 0;
    while (steps < AVP_ARRAY_LEN(k_zobj)) {
        AvpBytes out = {0};
        ShsmZErr err;
        REQUIRE(shsm_zdecomp_decompress(d, feed.v, feed.n, 4096, &out, &err));
        size_t tn, un;
        const u8 *tail = shsm_zdecomp_unconsumed_tail(d, &tn);
        shsm_zdecomp_unused_data(d, &un);
        CHECK_MSG(out.n == k_zobj[steps].out && tn == k_zobj[steps].tail && shsm_zdecomp_eof(d) == (k_zobj[steps].eof != 0) &&
                      un == k_zobj[steps].unused,
                  "step %zu: out %zu tail %zu eof %d unused %zu; CPython %u %u %u %u", steps, out.n, tn, (int)shsm_zdecomp_eof(d), un,
                  k_zobj[steps].out, k_zobj[steps].tail, k_zobj[steps].eof, k_zobj[steps].unused);
        avp_vec_extend(&all, out.v, out.n);
        avp_vec_free(&out);
        AvpBytes next = {0};
        avp_vec_extend(&next, tail, tn);
        if (steps == 0) avp_vec_extend(&next, comp + 5000, cn - 5000);
        avp_vec_free(&feed);
        feed = next;
        steps++;
        if (shsm_zdecomp_eof(d)) break;
    }
    CHECK_EQ_U64(steps, AVP_ARRAY_LEN(k_zobj));
    CHECK(all.n == 63000 && memcmp(all.v, src, 63000) == 0);
    /* after the end: more input is unused data */
    AvpBytes out = {0};
    CHECK(shsm_zdecomp_decompress(d, "xyz", 3, 0, &out, NULL));
    size_t un;
    shsm_zdecomp_unused_data(d, &un);
    CHECK_EQ_U64(un, 3);
    CHECK_EQ_U64(out.n, 0);
    avp_vec_free(&out);
    avp_vec_free(&all);
    avp_vec_free(&feed);
    shsm_zdecomp_free(d);
    avp_xfree(src);
    avp_arena_free(a);
}

/* ================================================================================================================ */

/* objcache.py's cache files: what the Python wrote decompresses here to the same bytes, and what this side writes is
 * byte for byte what zlib.compress(marshal.dumps(...)) writes (so either side reads the other's files). */
AVP_TEST(objcache_bytes_round_trip_with_python) {
    AvpArena *a = avp_arena_new(0);
    for (size_t i = 0; i < AVP_ARRAY_LEN(k_zpy); i++) {
        const ZPy *c = &k_zpy[i];
        size_t n = 0;
        ShsmZErr err;
        u8 *raw = shsm_zlib_decompress(a, c->z, c->zn, 15, 0, &n, &err);
        REQUIRE(raw != NULL);
        CHECK_MSG(n == c->rawn && memcmp(raw, c->raw, n) == 0, "%s: decompressed bytes differ", c->name);
        size_t zn = 0;
        u8 *z = shsm_zlib_compress(a, c->raw, c->rawn, -1, 15, &zn);
        REQUIRE(z != NULL);
        CHECK_MSG(zn == c->zn && memcmp(z, c->z, zn) == 0, "%s: compressed bytes differ from zlib.compress", c->name);
    }
    avp_arena_free(a);
}

AVP_TEST(crc32_and_adler32_equal_python) {
    for (size_t i = 0; i < AVP_ARRAY_LEN(k_zsum); i++) {
        const ZSum *c = &k_zsum[i];
        size_t n = strlen(c->s);
        CHECK_EQ_U64(shsm_zlib_crc32(c->s, n, 0), c->crc);
        CHECK_EQ_U64(shsm_zlib_crc32(c->s, n, 0x12345678u), c->crc_cont);
        CHECK_EQ_U64(shsm_zlib_adler32(c->s, n, 1), c->adler);
        /* in two pieces: the running value continues */
        size_t h = n / 2;
        CHECK_EQ_U64(shsm_zlib_crc32(c->s + h, n - h, shsm_zlib_crc32(c->s, h, 0)), c->crc);
        CHECK_EQ_U64(shsm_zlib_adler32(c->s + h, n - h, shsm_zlib_adler32(c->s, h, 1)), c->adler);
        /* core's table CRC-32 (hash.h) agrees */
        CHECK_EQ_U64(avp_crc32(0, c->s, n), c->crc);
    }
}

AVP_TEST_MAIN("shsm_zlib")
