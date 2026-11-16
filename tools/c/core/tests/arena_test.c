/* Arena, vector and allocator tests. */
#include "avp/avp.h"
#include "avp/test.h"

#include <windows.h>

AVP_TEST(alloc_alignment_and_zero) {
    AvpArena *a = avp_arena_new(4096);
    for (size_t n = 0; n < 300; n++) {
        u8 *p = (u8 *)avp_alloc(a, n);
        REQUIRE(p != NULL);
        CHECK(((uintptr_t)p & 15) == 0);
        for (size_t i = 0; i < n; i++) CHECK(p[i] == 0);
        memset(p, 0xAB, n);
    }
    avp_arena_free(a);
}

AVP_TEST(alloc_zero_gives_unique_pointers) {
    AvpArena *a = avp_arena_new(0);
    void *p = avp_alloc(a, 0), *q = avp_alloc(a, 0);
    CHECK(p != NULL && q != NULL && p != q);
    avp_arena_free(a);
}

AVP_TEST(default_arena_virtual_blocks_are_zero) {
    AvpArena *a = avp_arena_new(0);
    for (int round = 0; round < 40; round++) {
        u8 *p = (u8 *)avp_alloc(a, 100000);
        for (size_t i = 0; i < 100000; i += 997) CHECK(p[i] == 0);
        memset(p, 0xCD, 100000);
    }
    avp_arena_free(a);
}

AVP_TEST(mark_reset_reuses_and_rezeroes) {
    AvpArena *a = avp_arena_new(0);
    u8 *keep = (u8 *)avp_alloc(a, 64);
    memset(keep, 1, 64);
    AvpArenaMark m = avp_arena_mark(a);
    u8 *x = (u8 *)avp_alloc(a, 1000);
    memset(x, 0x77, 1000);
    avp_arena_reset(a, m);
    u8 *y = (u8 *)avp_alloc(a, 1000);
    CHECK(y == x);
    for (int i = 0; i < 1000; i++) CHECK(y[i] == 0);
    for (int i = 0; i < 64; i++) CHECK(keep[i] == 1);
    avp_arena_free(a);
}

AVP_TEST(reset_frees_later_blocks) {
    AvpArena *a = avp_arena_new(4096);
    void *first = avp_alloc(a, 100);
    AvpArenaMark m = avp_arena_mark(a);
    for (int i = 0; i < 200; i++) memset(avp_alloc(a, 1500), 0x11, 1500);
    avp_arena_reset(a, m);
    void *again = avp_alloc(a, 100);
    CHECK((u8 *)again == (u8 *)first + 112);
    avp_arena_free(a);
}

AVP_TEST(big_requests_get_own_block_and_reset_frees_them) {
    AvpArena *a = avp_arena_new(4096);
    u8 *small1 = (u8 *)avp_alloc(a, 32);
    u8 *big1 = (u8 *)avp_alloc(a, 100000); /* before the mark: survives the reset */
    memset(big1, 5, 100000);
    AvpArenaMark m = avp_arena_mark(a);
    u8 *big2 = (u8 *)avp_alloc(a, 50000);
    memset(big2, 6, 50000);
    avp_arena_reset(a, m);
    CHECK(big1[0] == 5 && big1[99999] == 5);
    CHECK(small1 != NULL);
    /* Many cycles must not leak or crash. */
    for (int i = 0; i < 100; i++) {
        AvpArenaMark mm = avp_arena_mark(a);
        memset(avp_alloc(a, 70000), 9, 70000);
        memset(avp_alloc(a, 10), 9, 10);
        avp_arena_reset(a, mm);
    }
    CHECK(big1[50000] == 5);
    avp_arena_free(a);
}

AVP_TEST(realloc_grows_in_place_when_last) {
    AvpArena *a = avp_arena_new(0);
    char *p = (char *)avp_alloc(a, 16);
    memcpy(p, "0123456789abcde", 16);
    char *q = (char *)avp_arena_realloc(a, p, 16, 5000);
    CHECK(q == p);
    CHECK_MEM(q, "0123456789abcde", 16);
    for (int i = 16; i < 5000; i++) q[i] = 'x';
    char *other = (char *)avp_alloc(a, 8);
    CHECK(other != NULL);
    char *r = (char *)avp_arena_realloc(a, q, 5000, 9000);
    CHECK(r != q); /* no longer the last allocation: moved */
    CHECK_MEM(r, "0123456789abcde", 16);
    CHECK(r[4999] == 'x');
    char *s = (char *)avp_arena_realloc(a, r, 9000, 8);
    CHECK(s == r);
    avp_arena_free(a);
}

AVP_TEST(realloc_null_and_block_overflow) {
    AvpArena *a = avp_arena_new(4096);
    char *p = (char *)avp_arena_realloc(a, NULL, 0, 100);
    CHECK(p != NULL);
    memset(p, 'a', 100);
    p = (char *)avp_arena_realloc(a, p, 100, 1000);
    p = (char *)avp_arena_realloc(a, p, 1000, 1900);
    p = (char *)avp_arena_realloc(a, p, 1900, 30000); /* grows past a block */
    for (int i = 0; i < 100; i++) CHECK(p[i] == 'a');
    avp_arena_free(a);
}

AVP_TEST(string_helpers) {
    AvpArena *a = avp_arena_new(0);
    char *s = avp_strdup(a, "hello");
    CHECK_CZ(s, "hello");
    char *f = avp_sprintf(a, "%d-%s-%05.1f", 42, "x", 3.14159);
    CHECK_CZ(f, "42-x-003.1");
    char *big = avp_sprintf(a, "%*s", 5000, "z");
    CHECK(strlen(big) == 5000 && big[4999] == 'z');
    u8 raw[3] = {1, 2, 3};
    u8 *d = (u8 *)avp_memdup(a, raw, 3);
    CHECK_MEM(d, raw, 3);
    CHECK(avp_memdup(a, raw, 0) != NULL);
    avp_arena_free(a);
}

AVP_TEST(thread_arenas_are_distinct_and_stable) {
    AvpArena *a = avp_arena(), *s = avp_scratch();
    CHECK(a != s);
    CHECK(a == avp_arena());
    CHECK(s == avp_scratch());
    AvpArenaMark m = avp_arena_mark(s);
    char *t = avp_strdup(s, "scratch");
    CHECK_CZ(t, "scratch");
    avp_arena_reset(s, m);
}

static DWORD WINAPI thread_main(void *arg) {
    AvpArena **out = (AvpArena **)arg;
    *out = avp_arena();
    for (int i = 0; i < 1000; i++) memset(avp_alloc(*out, 5000), 1, 5000);
    return 0;
}

AVP_TEST(thread_local_arena_per_thread) {
    AvpArena *other = NULL;
    HANDLE h = CreateThread(NULL, 0, thread_main, &other, 0, NULL);
    REQUIRE(h != NULL);
    WaitForSingleObject(h, INFINITE);
    CloseHandle(h);
    CHECK(other != NULL && other != avp_arena());
}

AVP_TEST(vec_push_insert_remove) {
    AvpI32Vec v = {0};
    for (i32 i = 0; i < 1000; i++) avp_vec_push(&v, i);
    CHECK_EQ_U64(v.n, 1000);
    CHECK(v.cap >= 1000);
    avp_vec_insert(&v, 0, -1);
    avp_vec_insert(&v, v.n, 5000);
    CHECK_EQ_I64(v.v[0], -1);
    CHECK_EQ_I64(avp_vec_last(&v), 5000);
    avp_vec_remove(&v, 0);
    CHECK_EQ_I64(v.v[0], 0);
    CHECK_EQ_I64(avp_vec_pop(&v), 5000);
    i32 ext[3] = {7, 8, 9};
    avp_vec_extend(&v, ext, 3);
    CHECK_EQ_U64(v.n, 1003);
    CHECK_EQ_I64(v.v[1002], 9);
    avp_vec_clear(&v);
    CHECK_EQ_U64(v.n, 0);
    avp_vec_free(&v);
    CHECK(v.v == NULL && v.cap == 0);
}

AVP_TEST(vec_push_zero_and_struct) {
    typedef struct {
        int a;
        double b;
    } Pair;
    AVP_VEC(Pair) v = {0};
    Pair *p = avp_vec_push_zero(&v);
    CHECK(p->a == 0 && p->b == 0.0);
    p->a = 3;
    avp_vec_push(&v, ((Pair){4, 2.5}));
    CHECK_EQ_U64(v.n, 2);
    CHECK(v.v[1].b == 2.5 && v.v[0].a == 3);
    avp_vec_reserve(&v, 100000);
    CHECK(v.cap >= 100000 && v.v[0].a == 3);
    avp_vec_free(&v);
}

AVP_TEST(xalloc_family) {
    char *p = (char *)avp_xmalloc(0);
    CHECK(p != NULL);
    p = (char *)avp_xrealloc(p, 100);
    memset(p, 1, 100);
    avp_xfree(p);
    int *z = (int *)avp_xcalloc(10, sizeof(int));
    for (int i = 0; i < 10; i++) CHECK(z[i] == 0);
    avp_xfree(z);
    avp_xfree(NULL);
}

AVP_TEST_MAIN("arena")
