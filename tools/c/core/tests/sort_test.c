/* Tests of sort.h: stability and ordering of the sorts against naive references, bisect against a linear scan, and
 * heapq pop orders against the orders CPython 3.14.8 produced (sort_heap_corpus.h, generator in its comment). */
#include "avp/sort.h"
#include "avp/test.h"
#include "sort_heap_corpus.h"

static u64 lcg_next(u64 *x) {
    *x = *x * 6364136223846793005ull + 1442695040888963407ull;
    return *x;
}

typedef struct {
    i32 key;
    u32 pos; /* original position: tells stability */
} Item;

static int cmp_item(const void *a, const void *b, void *ctx) {
    AVP_UNUSED(ctx);
    const Item *x = a, *y = b;
    return (x->key > y->key) - (x->key < y->key);
}

static void check_sorted_stable(const Item *a, size_t n, bool reverse, const char *what) {
    for (size_t i = 1; i < n; i++) {
        bool ordered = reverse ? a[i - 1].key >= a[i].key : a[i - 1].key <= a[i].key;
        bool stable = a[i - 1].key != a[i].key || a[i - 1].pos < a[i].pos;
        if (!ordered || !stable) {
            CHECK_MSG(false, "%s: n %zu reverse %d: wrong at %zu", what, n, reverse, i);
            return;
        }
    }
}

AVP_TEST(sort_generic_stable) {
    u64 x = 7;
    size_t sizes[] = {0, 1, 2, 3, 15, 16, 17, 31, 32, 33, 100, 1000, 4097, 50000};
    int ranges[] = {1, 2, 5, 40, 100000};
    for (size_t si = 0; si < AVP_ARRAY_LEN(sizes); si++)
        for (size_t ri = 0; ri < AVP_ARRAY_LEN(ranges); ri++)
            for (int rev = 0; rev < 2; rev++) {
                size_t n = sizes[si];
                Item *a = avp_xmalloc((n + 1) * sizeof(Item));
                for (size_t i = 0; i < n; i++) {
                    a[i].key = (i32)((lcg_next(&x) >> 33) % (u64)ranges[ri]);
                    a[i].pos = (u32)i;
                }
                avp_sort(a, n, sizeof(Item), cmp_item, NULL, rev != 0);
                check_sorted_stable(a, n, rev != 0, "random");
                avp_xfree(a);
            }
}

AVP_TEST(sort_presorted_and_reversed) {
    size_t n = 5000;
    Item *a = avp_xmalloc(n * sizeof(Item));
    for (int mode = 0; mode < 4; mode++) {
        for (size_t i = 0; i < n; i++) {
            a[i].key = (i32)(mode == 0 ? i / 3 : mode == 1 ? (n - i) / 3 : mode == 2 ? (i % 100 == 0 ? 0 : i) : 7);
            a[i].pos = (u32)i;
        }
        avp_sort(a, n, sizeof(Item), cmp_item, NULL, false);
        check_sorted_stable(a, n, false, "presorted");
        for (size_t i = 0; i < n; i++) a[i].pos = (u32)i; /* sorted now: sort again in reverse, keys with equal runs */
        avp_sort(a, n, sizeof(Item), cmp_item, NULL, true);
        check_sorted_stable(a, n, true, "reverse of sorted");
    }
    avp_xfree(a);
}

/* a wide element, to take the heap-allocated temporary path */
typedef struct {
    i32 key;
    u8 pad[300];
    u32 pos;
} Wide;

static int cmp_wide(const void *a, const void *b, void *ctx) {
    AVP_UNUSED(ctx);
    const Wide *x = a, *y = b;
    return (x->key > y->key) - (x->key < y->key);
}

AVP_TEST(sort_wide_elements) {
    u64 x = 3;
    size_t n = 300;
    Wide *a = avp_xcalloc(n, sizeof(Wide));
    for (size_t i = 0; i < n; i++) {
        a[i].key = (i32)((lcg_next(&x) >> 33) % 10);
        a[i].pos = (u32)i;
        memset(a[i].pad, (int)i, sizeof a[i].pad);
    }
    avp_sort(a, n, sizeof(Wide), cmp_wide, NULL, false);
    for (size_t i = 1; i < n; i++) CHECK(a[i - 1].key < a[i].key || (a[i - 1].key == a[i].key && a[i - 1].pos < a[i].pos));
    for (size_t i = 0; i < n; i++) CHECK(a[i].pad[0] == (u8)a[i].pos && a[i].pad[299] == (u8)a[i].pos);
    avp_xfree(a);
}

typedef struct {
    int calls;
} Count;
static int cmp_count(const void *a, const void *b, void *ctx) {
    ((Count *)ctx)->calls++;
    return cmp_item(a, b, NULL);
}

AVP_TEST(sort_ctx_and_sorted_input_cost) {
    size_t n = 10000;
    Item *a = avp_xmalloc(n * sizeof(Item));
    for (size_t i = 0; i < n; i++) {
        a[i].key = (i32)i;
        a[i].pos = (u32)i;
    }
    Count c = {0};
    avp_sort(a, n, sizeof(Item), cmp_count, &c, false);
    CHECK_MSG(c.calls < (int)(3 * n), "sorted input took %d comparisons", c.calls);
    avp_xfree(a);
}

AVP_TEST(sort_typed) {
    u64 x = 11;
    for (size_t n = 0; n < 200; n += (n < 40 ? 1 : 37)) {
        i64 *a = avp_xmalloc((n + 1) * sizeof(i64));
        u64 *u = avp_xmalloc((n + 1) * sizeof(u64));
        double *d = avp_xmalloc((n + 1) * sizeof(double));
        AvpStr *s = avp_xmalloc((n + 1) * sizeof(AvpStr));
        static char names[200][8];
        for (size_t i = 0; i < n; i++) {
            u64 r = lcg_next(&x) >> 33;
            a[i] = (i64)(r % 50) - 25;
            u[i] = r * 0x100000001ull;
            d[i] = (double)((i64)(r % 1000) - 500) / 8.0;
            int len = (int)(r % 5);
            for (int j = 0; j < len; j++) names[i][j] = (char)('a' + (r >> (j * 3)) % 3);
            s[i] = avp_strn(names[i], (size_t)len);
        }
        avp_sort_i64(a, n);
        avp_sort_u64(u, n);
        avp_sort_f64(d, n);
        avp_sort_strs(s, n);
        for (size_t i = 1; i < n; i++) {
            CHECK(a[i - 1] <= a[i]);
            CHECK(u[i - 1] <= u[i]);
            CHECK(d[i - 1] <= d[i]);
            CHECK(avp_cmp_str(s[i - 1], s[i]) <= 0);
        }
        avp_xfree(a);
        avp_xfree(u);
        avp_xfree(d);
        avp_xfree(s);
    }
    /* sorted(["b", "", "ab", "a", "abc", "B"]) -> ['', 'B', 'a', 'ab', 'abc', 'b'] */
    AvpStr w[] = {AVP_S("b"), AVP_S(""), AVP_S("ab"), AVP_S("a"), AVP_S("abc"), AVP_S("B")};
    avp_sort_strs(w, 6);
    const char *want[] = {"", "B", "a", "ab", "abc", "b"};
    for (int i = 0; i < 6; i++) CHECK_STR(w[i], want[i]);
}

AVP_TEST(compare_helpers) {
    CHECK(avp_cmp_i64(-1, 1) < 0 && avp_cmp_i64(5, 5) == 0 && avp_cmp_i64(INT64_MAX, INT64_MIN) > 0);
    CHECK(avp_cmp_u64(1, UINT64_MAX) < 0 && avp_cmp_u64(UINT64_MAX, 1) > 0 && avp_cmp_u64(3, 3) == 0);
    CHECK(avp_cmp_f64(1.5, 2.5) < 0 && avp_cmp_f64(2.5, 1.5) > 0 && avp_cmp_f64(0.0, -0.0) == 0);
    CHECK(avp_cmp_str(AVP_S("a"), AVP_S("b")) < 0 && avp_cmp_str(AVP_S("ab"), AVP_S("a")) > 0 &&
          avp_cmp_str(AVP_S(""), AVP_S("")) == 0 && avp_cmp_str(AVP_S("\xc3\xa9"), AVP_S("z")) > 0);
}

static int cmp_probe_item(const void *probe, const void *elem, void *ctx) {
    AVP_UNUSED(ctx);
    i32 k = *(const i32 *)probe;
    const Item *e = elem;
    return (k > e->key) - (k < e->key);
}

AVP_TEST(bisect) {
    u64 x = 5;
    for (size_t n = 0; n < 60; n++) {
        for (int range = 1; range <= 30; range += 7) {
            i64 *a = avp_xmalloc((n + 1) * sizeof(i64));
            u64 *u = avp_xmalloc((n + 1) * sizeof(u64));
            Item *it = avp_xmalloc((n + 1) * sizeof(Item));
            for (size_t i = 0; i < n; i++) a[i] = (i64)((lcg_next(&x) >> 33) % (u64)range) - range / 2;
            avp_sort_i64(a, n);
            for (size_t i = 0; i < n; i++) {
                u[i] = (u64)(a[i] + 100);
                it[i].key = (i32)a[i];
                it[i].pos = (u32)i;
            }
            for (i64 p = -range; p <= range; p++) {
                size_t left = 0, right = 0;
                for (size_t i = 0; i < n; i++) {
                    if (a[i] < p) left++;
                    if (a[i] <= p) right++;
                }
                CHECK(avp_bisect_left_i64(a, n, p) == left);
                CHECK(avp_bisect_right_i64(a, n, p) == right);
                CHECK(avp_bisect_left_u64(u, n, (u64)(p + 100)) == left);
                CHECK(avp_bisect_right_u64(u, n, (u64)(p + 100)) == right);
                i32 probe = (i32)p;
                CHECK(avp_bisect_left(it, n, sizeof(Item), &probe, cmp_probe_item, NULL) == left);
                CHECK(avp_bisect_right(it, n, sizeof(Item), &probe, cmp_probe_item, NULL) == right);
            }
            avp_xfree(a);
            avp_xfree(u);
            avp_xfree(it);
        }
    }
}

/* ---- heapq ---- */
typedef struct {
    i32 prio;
    i32 tag;
} HeapItem;

static int cmp_heap(const void *a, const void *b, void *ctx) {
    AVP_UNUSED(ctx);
    const HeapItem *x = a, *y = b;
    return (x->prio > y->prio) - (x->prio < y->prio); /* by priority only: ties are what is tested */
}

static u64 fnv_fold(u64 h, u64 x) { return (h ^ x) * 0x100000001b3ull; }

AVP_TEST(heapq_pop_orders) {
    for (size_t si = 0; si < AVP_ARRAY_LEN(heap_scenarios); si++) {
        const HeapScenario *sc = &heap_scenarios[si];
        size_t cap = sc->initial + sc->count + 1;
        HeapItem *heap = avp_xmalloc(cap * sizeof(HeapItem));
        size_t n = 0;
        u64 x = sc->seed;
        i32 tag = 0;
        for (u32 i = 0; i < sc->initial; i++) {
            heap[n].prio = (i32)((lcg_next(&x) >> 33) % sc->nprio);
            heap[n].tag = tag++;
            n++;
        }
        avp_heapify(heap, n, sizeof(HeapItem), cmp_heap, NULL);
        bool heap0_ok = true;
        for (size_t i = 0; i < sc->heap0_n; i++)
            if ((u32)heap[i].tag != sc->heap0[i]) heap0_ok = false;
        CHECK_MSG(heap0_ok, "scenario %zu: array after heapify differs", si);
        u64 h = 0xcbf29ce484222325ull;
        size_t npops = 0;
        bool first_ok = true;
        HeapItem out;
        for (u32 i = 0; i < sc->count; i++) {
            u64 r = lcg_next(&x);
            if (((r >> 20) & 3) == 0 && n > 0) {
                avp_heap_pop(heap, &n, sizeof(HeapItem), &out, cmp_heap, NULL);
                if (npops < sc->nfirst && (u32)out.tag != sc->first[npops]) first_ok = false;
                h = fnv_fold(h, (u64)out.tag);
                npops++;
            } else {
                HeapItem it = {(i32)((r >> 33) % sc->nprio), tag++};
                avp_heap_push(heap, &n, sizeof(HeapItem), &it, cmp_heap, NULL);
            }
        }
        while (n > 0) {
            avp_heap_pop(heap, &n, sizeof(HeapItem), &out, cmp_heap, NULL);
            if (npops < sc->nfirst && (u32)out.tag != sc->first[npops]) first_ok = false;
            h = fnv_fold(h, (u64)out.tag);
            npops++;
        }
        CHECK_MSG(npops == sc->npops, "scenario %zu: %zu pops, expected %zu", si, npops, sc->npops);
        CHECK_MSG(first_ok, "scenario %zu: first pops differ", si);
        CHECK_MSG(h == sc->hash, "scenario %zu: pop order checksum differs", si);
        avp_xfree(heap);
    }
}

AVP_TEST(heapq_wide_items_and_discarded_pop) {
    Wide *heap = avp_xcalloc(64, sizeof(Wide));
    size_t n = 0;
    for (i32 k = 20; k > 0; k--) {
        Wide w = {0};
        w.key = k % 7;
        w.pos = (u32)k;
        memset(w.pad, k, sizeof w.pad);
        avp_heap_push(heap, &n, sizeof(Wide), &w, cmp_wide, NULL);
    }
    i32 last = -1;
    while (n > 0) {
        Wide out;
        avp_heap_pop(heap, &n, sizeof(Wide), &out, cmp_wide, NULL);
        CHECK(out.key >= last && out.pad[100] == (u8)out.pos);
        last = out.key;
        if (n == 3) avp_heap_pop(heap, &n, sizeof(Wide), NULL, cmp_wide, NULL); /* out == NULL drops the item */
    }
    avp_xfree(heap);
}

AVP_TEST_MAIN("sort")
