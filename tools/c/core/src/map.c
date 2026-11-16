/* Insertion-ordered hash maps and sets (map.h). One table implementation serves the four public kinds:
 *
 *   entries   an array in insertion order: {hash, key word 0, key word 1, value}; a deleted entry stays in place
 *             (key tag 0) until the next growth compacts the array, so iteration order is Python's dict order and
 *             the pointers handed out stay valid until the next put.
 *   index     an open-addressing table of 64-bit slots {hash high 32 bits, entry number + 1}, linear probing,
 *             tombstones for deleted keys, never more than half full (counting dead entries), so a probe almost
 *             always ends at the first or second slot and rejects a mismatch without touching the entries.
 *
 * String keys are copied into a chunked arena owned by the map (the entry points into it). Integer keys live in
 * the entry itself. The hash of a byte string is a wyhash-style 128-bit-multiply mix (not Python's hash: no output
 * depends on it, since iteration is in insertion order). */
#include "avp/map.h"
#include "avp/sort.h"
#include <intrin.h>

typedef struct {
    u64 hash;
    u64 k0;  /* string key: pointer; integer key: the key */
    u64 k1;  /* key tag: string length + 1, or 1 for an integer key; 0 marks a deleted entry */
    AvpVal val;
} Entry;

typedef struct Chunk {
    struct Chunk *next;
    size_t cap, used;
    /* the bytes follow */
} Chunk;

typedef struct {
    Entry *e;
    u64 *idx;
    size_t n;        /* entries used, dead ones included */
    size_t cap;      /* entries allocated: a power of two, or 0 before the first put */
    size_t live;
    size_t mask;     /* index size - 1 */
    bool str;
    Chunk *chunks;   /* string key storage, newest first */
} Tab;

#define SLOT_TOMB 0xFFFFFFFFull /* entry number field of a deleted key's slot */
#define SLOT_LOW 0xFFFFFFFFull
#define SLOT_HIGH 0xFFFFFFFF00000000ull
#define MIN_CAP 8

static inline u64 mum(u64 a, u64 b) {
    u64 hi;
    u64 lo = _umul128(a, b, &hi);
    return lo ^ hi;
}
static inline u64 rd8(const u8 *p) { u64 v; memcpy(&v, p, 8); return v; }
static inline u64 rd4(const u8 *p) { u32 v; memcpy(&v, p, 4); return v; }

static const u64 S0 = 0xa0761d6478bd642full, S1 = 0xe7037ed1a0b428dbull, S2 = 0x8ebc6af09c88c6e3ull,
                 S3 = 0x589965cc75374cc3ull;

static u64 hash_bytes(const char *s, size_t n) {
    const u8 *p = (const u8 *)s;
    u64 seed = S0, a, b;
    if (n <= 16) {
        if (n >= 4) {
            a = (rd4(p) << 32) | rd4(p + ((n >> 3) << 2));
            b = (rd4(p + n - 4) << 32) | rd4(p + n - 4 - ((n >> 3) << 2));
        } else if (n > 0) {
            a = ((u64)p[0] << 16) | ((u64)p[n >> 1] << 8) | p[n - 1];
            b = 0;
        } else {
            a = b = 0;
        }
    } else {
        size_t i = n;
        if (i > 48) {
            u64 see1 = seed, see2 = seed;
            do {
                seed = mum(rd8(p) ^ S1, rd8(p + 8) ^ seed);
                see1 = mum(rd8(p + 16) ^ S2, rd8(p + 24) ^ see1);
                see2 = mum(rd8(p + 32) ^ S3, rd8(p + 40) ^ see2);
                p += 48;
                i -= 48;
            } while (i > 48);
            seed ^= see1 ^ see2;
        }
        while (i > 16) {
            seed = mum(rd8(p) ^ S1, rd8(p + 8) ^ seed);
            i -= 16;
            p += 16;
        }
        a = rd8(p + i - 16);
        b = rd8(p + i - 8);
    }
    return mum(S1 ^ n, mum(a ^ S1, b ^ seed));
}

static inline u64 hash_int(u64 k) { return mum(k ^ S0, S1 ^ 0x9e3779b97f4a7c15ull); }

static Tab *tab_new(bool str, size_t expected) {
    Tab *t = avp_xcalloc(1, sizeof *t);
    t->str = str;
    if (expected > 0) {
        size_t cap = MIN_CAP;
        while (cap < expected) cap <<= 1;
        t->cap = cap;
        t->e = avp_xmalloc(cap * sizeof(Entry));
        t->mask = cap * 2 - 1;
        t->idx = avp_xcalloc(cap * 2, sizeof(u64));
    }
    return t;
}

static void chunks_free(Chunk *c) {
    while (c) {
        Chunk *next = c->next;
        avp_xfree(c);
        c = next;
    }
}

static void tab_free(Tab *t) {
    if (!t) return;
    chunks_free(t->chunks);
    avp_xfree(t->e);
    avp_xfree(t->idx);
    avp_xfree(t);
}

static void tab_clear(Tab *t) {
    chunks_free(t->chunks);
    t->chunks = NULL;
    t->n = t->live = 0;
    if (t->idx) memset(t->idx, 0, (t->mask + 1) * sizeof(u64));
}

/* Copies a key into the map's storage and appends a NUL after it. */
static const char *key_store(Tab *t, const char *s, size_t n) {
    Chunk *c = t->chunks;
    if (!c || c->cap - c->used < n + 1) {
        size_t cap = c ? c->cap * 2 : 4096;
        if (cap > ((size_t)1 << 20)) cap = (size_t)1 << 20;
        if (cap < n + 1) cap = n + 1;
        Chunk *nc = avp_xmalloc(sizeof(Chunk) + cap);
        nc->next = t->chunks;
        nc->cap = cap;
        nc->used = 0;
        t->chunks = nc;
        c = nc;
    }
    char *dst = (char *)(c + 1) + c->used;
    if (n) memcpy(dst, s, n);
    dst[n] = 0;
    c->used += n + 1;
    return dst;
}

static inline bool key_eq(const Tab *t, const Entry *e, u64 k0, u64 k1) {
    if (!t->str) return e->k0 == k0;
    return e->k1 == k1 &&
           (k1 == 1 || memcmp((const void *)(uintptr_t)e->k0, (const void *)(uintptr_t)k0, (size_t)(k1 - 1)) == 0);
}

/* Finds the entry of a key; returns NULL when absent. */
static Entry *tab_find(const Tab *t, u64 h, u64 k0, u64 k1) {
    if (!t->cap) return NULL;
    u64 tag = h >> 32;
    size_t i = (size_t)h & t->mask;
    for (;;) {
        u64 s = t->idx[i];
        if (s == 0) return NULL;
        if ((s & SLOT_LOW) != SLOT_TOMB && (s >> 32) == tag) {
            Entry *e = &t->e[(s & SLOT_LOW) - 1];
            if (e->hash == h && key_eq(t, e, k0, k1)) return e;
        }
        i = (i + 1) & t->mask;
    }
}

/* Rebuilds the entries (compacted, same order) and the index at the given entry capacity. */
static void tab_rebuild(Tab *t, size_t cap) {
    AVP_ASSERT(cap < SLOT_TOMB);
    Entry *ne = avp_xmalloc(cap * sizeof(Entry));
    size_t m = 0;
    for (size_t i = 0; i < t->n; i++)
        if (t->e[i].k1) ne[m++] = t->e[i];
    avp_xfree(t->e);
    avp_xfree(t->idx);
    t->e = ne;
    t->n = m;
    t->cap = cap;
    t->mask = cap * 2 - 1;
    t->idx = avp_xcalloc(cap * 2, sizeof(u64));
    for (size_t i = 0; i < m; i++) {
        size_t j = (size_t)ne[i].hash & t->mask;
        while (t->idx[j]) j = (j + 1) & t->mask;
        t->idx[j] = (ne[i].hash & SLOT_HIGH) | (u64)(i + 1);
    }
}

/* Appends a new entry for a key known to be absent. */
static Entry *tab_append(Tab *t, u64 h, u64 k0, u64 k1) {
    if (t->n == t->cap) {
        if (!t->cap) tab_rebuild(t, MIN_CAP);
        else if (t->live * 4 < t->n * 3) tab_rebuild(t, t->cap); /* a quarter or more is dead: compact in place */
        else tab_rebuild(t, t->cap * 2);
    }
    size_t j = (size_t)h & t->mask;
    while (t->idx[j] != 0 && (t->idx[j] & SLOT_LOW) != SLOT_TOMB) j = (j + 1) & t->mask;
    Entry *e = &t->e[t->n];
    e->hash = h;
    e->k0 = k0;
    e->k1 = k1;
    e->val.u = 0;
    t->idx[j] = (h & SLOT_HIGH) | (u64)(t->n + 1);
    t->n++;
    t->live++;
    return e;
}

static Entry *tab_put(Tab *t, u64 h, u64 k0, u64 k1, bool *created) {
    Entry *e = tab_find(t, h, k0, k1);
    if (created) *created = (e == NULL);
    return e ? e : tab_append(t, h, k0, k1);
}

static bool tab_del(Tab *t, u64 h, u64 k0, u64 k1) {
    if (!t->cap) return false;
    u64 tag = h >> 32;
    size_t i = (size_t)h & t->mask;
    for (;;) {
        u64 s = t->idx[i];
        if (s == 0) return false;
        if ((s & SLOT_LOW) != SLOT_TOMB && (s >> 32) == tag) {
            Entry *e = &t->e[(s & SLOT_LOW) - 1];
            if (e->hash == h && key_eq(t, e, k0, k1)) {
                e->k1 = 0;
                t->idx[i] = SLOT_TOMB;
                t->live--;
                return true;
            }
        }
        i = (i + 1) & t->mask;
    }
}

/* Entry operations by key kind. A string key is copied into the map when it is inserted. An empty key points at a
 * literal so that the key word is never NULL. */
static inline u64 skey_k0(AvpStr k) { return (u64)(uintptr_t)(k.n ? k.p : ""); }

static Entry *sfind(const Tab *t, AvpStr k) {
    return tab_find(t, hash_bytes(k.p, k.n), skey_k0(k), (u64)k.n + 1);
}

static Entry *sput(Tab *t, AvpStr k, bool *created) {
    u64 h = hash_bytes(k.p, k.n);
    Entry *e = tab_find(t, h, skey_k0(k), (u64)k.n + 1);
    if (created) *created = (e == NULL);
    if (e) return e;
    const char *own = key_store(t, k.p, k.n); /* the new entry points at the map's own bytes */
    return tab_append(t, h, (u64)(uintptr_t)own, (u64)k.n + 1);
}

static bool sdel(Tab *t, AvpStr k) { return tab_del(t, hash_bytes(k.p, k.n), skey_k0(k), (u64)k.n + 1); }

static inline AvpStr entry_str(const Entry *e) {
    return (AvpStr){(const char *)(uintptr_t)e->k0, (size_t)(e->k1 - 1)};
}

/* The next live entry at or after *pos, or NULL. */
static const Entry *tab_next(const Tab *t, size_t *pos) {
    while (*pos < t->n) {
        const Entry *e = &t->e[(*pos)++];
        if (e->k1) return e;
    }
    return NULL;
}

/* ---- AvpMap ---- */
struct AvpMap { Tab t; };
AvpMap *avp_map_new(void) { return (AvpMap *)tab_new(true, 0); }
AvpMap *avp_map_new_cap(size_t expected) { return (AvpMap *)tab_new(true, expected); }
void avp_map_free(AvpMap *m) { if (m) tab_free(&m->t); }
void avp_map_clear(AvpMap *m) { tab_clear(&m->t); }
size_t avp_map_len(const AvpMap *m) { return m->t.live; }
AvpVal *avp_map_get(const AvpMap *m, AvpStr key) {
    Entry *e = sfind(&m->t, key);
    return e ? &e->val : NULL;
}
bool avp_map_has(const AvpMap *m, AvpStr key) { return sfind(&m->t, key) != NULL; }
AvpVal *avp_map_put(AvpMap *m, AvpStr key, bool *created) { return &sput(&m->t, key, created)->val; }
bool avp_map_del(AvpMap *m, AvpStr key) { return sdel(&m->t, key); }
bool avp_map_next(const AvpMap *m, AvpMapIter *it) {
    const Entry *e = tab_next(&m->t, &it->pos);
    if (!e) return false;
    it->key = entry_str(e);
    it->val = (AvpVal *)&e->val;
    return true;
}
AvpStrVec avp_map_keys(const AvpMap *m) {
    AvpStrVec v = {0};
    avp_vec_reserve(&v, m->t.live);
    for (size_t i = 0; i < m->t.n; i++)
        if (m->t.e[i].k1) v.v[v.n++] = entry_str(&m->t.e[i]);
    return v;
}

/* ---- AvpIMap ---- */
struct AvpIMap { Tab t; };
AvpIMap *avp_imap_new(void) { return (AvpIMap *)tab_new(false, 0); }
AvpIMap *avp_imap_new_cap(size_t expected) { return (AvpIMap *)tab_new(false, expected); }
void avp_imap_free(AvpIMap *m) { if (m) tab_free(&m->t); }
void avp_imap_clear(AvpIMap *m) { tab_clear(&m->t); }
size_t avp_imap_len(const AvpIMap *m) { return m->t.live; }
AvpVal *avp_imap_get(const AvpIMap *m, u64 key) {
    Entry *e = tab_find(&m->t, hash_int(key), key, 1);
    return e ? &e->val : NULL;
}
bool avp_imap_has(const AvpIMap *m, u64 key) { return tab_find(&m->t, hash_int(key), key, 1) != NULL; }
AvpVal *avp_imap_put(AvpIMap *m, u64 key, bool *created) {
    return &tab_put(&m->t, hash_int(key), key, 1, created)->val;
}
bool avp_imap_del(AvpIMap *m, u64 key) { return tab_del(&m->t, hash_int(key), key, 1); }
bool avp_imap_next(const AvpIMap *m, AvpIMapIter *it) {
    const Entry *e = tab_next(&m->t, &it->pos);
    if (!e) return false;
    it->key = e->k0;
    it->val = (AvpVal *)&e->val;
    return true;
}

/* ---- AvpSet / AvpISet ---- */
struct AvpSet { Tab t; };
struct AvpISet { Tab t; };
AvpSet *avp_set_new(void) { return (AvpSet *)tab_new(true, 0); }
void avp_set_free(AvpSet *s) { if (s) tab_free(&s->t); }
size_t avp_set_len(const AvpSet *s) { return s->t.live; }
bool avp_set_add(AvpSet *s, AvpStr member) {
    bool created;
    sput(&s->t, member, &created);
    return created;
}
bool avp_set_has(const AvpSet *s, AvpStr member) { return sfind(&s->t, member) != NULL; }
bool avp_set_del(AvpSet *s, AvpStr member) { return sdel(&s->t, member); }
bool avp_set_next(const AvpSet *s, size_t *pos, AvpStr *member) {
    const Entry *e = tab_next(&s->t, pos);
    if (!e) return false;
    *member = entry_str(e);
    return true;
}
AvpStrVec avp_set_sorted(const AvpSet *s) {
    AvpStrVec v = {0};
    avp_vec_reserve(&v, s->t.live);
    for (size_t i = 0; i < s->t.n; i++)
        if (s->t.e[i].k1) v.v[v.n++] = entry_str(&s->t.e[i]);
    avp_sort_strs(v.v, v.n);
    return v;
}

AvpISet *avp_iset_new(void) { return (AvpISet *)tab_new(false, 0); }
void avp_iset_free(AvpISet *s) { if (s) tab_free(&s->t); }
size_t avp_iset_len(const AvpISet *s) { return s->t.live; }
bool avp_iset_add(AvpISet *s, u64 member) {
    bool created;
    tab_put(&s->t, hash_int(member), member, 1, &created);
    return created;
}
bool avp_iset_has(const AvpISet *s, u64 member) { return tab_find(&s->t, hash_int(member), member, 1) != NULL; }
bool avp_iset_del(AvpISet *s, u64 member) { return tab_del(&s->t, hash_int(member), member, 1); }
bool avp_iset_next(const AvpISet *s, size_t *pos, u64 *member) {
    const Entry *e = tab_next(&s->t, pos);
    if (!e) return false;
    *member = e->k0;
    return true;
}
AvpU64Vec avp_iset_sorted(const AvpISet *s) {
    AvpU64Vec v = {0};
    avp_vec_reserve(&v, s->t.live);
    for (size_t i = 0; i < s->t.n; i++)
        if (s->t.e[i].k1) v.v[v.n++] = s->t.e[i].k0;
    avp_sort_u64(v.v, v.n);
    return v;
}
