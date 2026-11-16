/* avpcore arenas: bump allocators over a chain of blocks.
 *
 * Blocks of the default size come from VirtualAlloc (fresh pages are zero, so a zeroed allocation above the block's
 * high-water mark needs no memset and never touches the pages); small test arenas use malloc. A request of half a
 * block or more gets its own block. Big blocks sit in a second chain, each stamped with the position in the normal
 * chain at which it was allocated, so that avp_arena_reset frees them together with everything allocated after the
 * mark. */
#include "avp/avp.h"

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define ARENA_DEFAULT_BLOCK ((size_t)1 << 20)
#define ARENA_VIRTUAL_MIN ((size_t)64 << 10)
#define ARENA_ALIGN 16u
#define ARENA_HDR 64u

typedef struct Block {
    struct Block *prev; /* the older block of the chain */
    size_t cap;         /* usable bytes after the header */
    size_t used;
    size_t hwm;         /* bytes above this offset are known to be zero */
    u64 serial;         /* position in the normal chain; for a big block, the serial of the normal block it follows */
    size_t at_used;     /* big blocks only: that normal block's `used` when this one was allocated */
    bool virt;
    bool big;
} Block;
_Static_assert(sizeof(Block) <= ARENA_HDR, "block header fits");

struct AvpArena {
    Block *cur;  /* newest normal block (never NULL after avp_arena_new) */
    Block *bigs; /* newest big block */
    size_t block_size;
    u64 next_serial;
    void *last;  /* the last allocation, for in-place realloc (NULL after a reset or a big allocation) */
    size_t last_n;
};

static size_t round_up(size_t n) { return (n + (ARENA_ALIGN - 1)) & ~(size_t)(ARENA_ALIGN - 1); }

static u8 *block_data(Block *b) { return (u8 *)b + ARENA_HDR; }

static Block *block_new(size_t cap) {
    size_t total = cap + ARENA_HDR;
    if (total < cap) avp_fatal("out of memory (%zu bytes)", cap);
    Block *b;
    bool virt = total >= ARENA_VIRTUAL_MIN;
    if (virt) {
        b = (Block *)VirtualAlloc(NULL, total, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!b) avp_fatal("out of memory (%zu bytes)", total);
    } else {
        b = (Block *)malloc(total);
        if (!b) avp_fatal("out of memory (%zu bytes)", total);
    }
    memset(b, 0, sizeof *b);
    b->cap = cap;
    b->virt = virt;
    b->hwm = virt ? 0 : cap; /* malloc memory is not known to be zero */
    return b;
}

static void block_free(Block *b) {
    if (b->virt) VirtualFree(b, 0, MEM_RELEASE);
    else free(b);
}

AvpArena *avp_arena_new(size_t block_size) {
    AvpArena *a = (AvpArena *)avp_xcalloc(1, sizeof *a);
    a->block_size = block_size ? round_up(block_size) : ARENA_DEFAULT_BLOCK;
    a->cur = block_new(a->block_size);
    a->cur->serial = a->next_serial++;
    return a;
}

void avp_arena_free(AvpArena *a) {
    if (!a) return;
    for (Block *b = a->cur; b;) {
        Block *p = b->prev;
        block_free(b);
        b = p;
    }
    for (Block *b = a->bigs; b;) {
        Block *p = b->prev;
        block_free(b);
        b = p;
    }
    free(a);
}

static void *alloc_impl(AvpArena *a, size_t n, bool zero) {
    AVP_ASSERT(a != NULL);
    if (n > SIZE_MAX / 4) avp_fatal("out of memory (%zu bytes)", n);
    size_t sz = round_up(n ? n : 1);
    Block *c = a->cur;
    if (sz >= a->block_size / 2) {
        /* A 16-byte marker in the normal chain tells marks taken before this allocation (used <= at_used) from the
         * ones taken after it (used > at_used). */
        if (ARENA_ALIGN > c->cap - c->used) {
            Block *nb = block_new(a->block_size);
            nb->serial = a->next_serial++;
            nb->prev = c;
            a->cur = c = nb;
        }
        Block *b = block_new(sz);
        b->big = true;
        b->used = sz;
        b->hwm = b->virt ? sz : b->hwm;
        b->serial = c->serial;
        b->at_used = c->used;
        c->used += ARENA_ALIGN;
        if (c->used > c->hwm) c->hwm = c->used;
        b->prev = a->bigs;
        a->bigs = b;
        a->last = NULL;
        void *p = block_data(b);
        if (zero && !b->virt) memset(p, 0, sz);
        return p;
    }
    if (sz > c->cap - c->used) {
        Block *b = block_new(a->block_size);
        b->serial = a->next_serial++;
        b->prev = c;
        a->cur = c = b;
    }
    size_t at = c->used;
    u8 *p = block_data(c) + at;
    c->used = at + sz;
    if (zero && at < c->hwm) memset(p, 0, AVP_MIN(sz, c->hwm - at));
    if (c->used > c->hwm) c->hwm = c->used;
    a->last = p;
    a->last_n = sz;
    return p;
}

void *avp_alloc(AvpArena *a, size_t n) { return alloc_impl(a, n, true); }
void *avp_alloc_noz(AvpArena *a, size_t n) { return alloc_impl(a, n, false); }

void *avp_arena_realloc(AvpArena *a, void *p, size_t old_n, size_t new_n) {
    if (!p) return avp_alloc_noz(a, new_n);
    if (round_up(new_n ? new_n : 1) <= round_up(old_n ? old_n : 1)) {
        if (p == a->last) {
            /* Shrinking the last allocation gives the tail back. */
            size_t sz = round_up(new_n ? new_n : 1);
            a->cur->used -= a->last_n - sz;
            a->last_n = sz;
        }
        return p;
    }
    if (p == a->last) {
        Block *c = a->cur;
        size_t start = (size_t)((u8 *)p - block_data(c));
        if (new_n <= c->cap - start && round_up(new_n) <= c->cap - start) {
            size_t sz = round_up(new_n);
            c->used = start + sz;
            if (c->used > c->hwm) c->hwm = c->used;
            a->last_n = sz;
            return p;
        }
    }
    void *q = avp_alloc_noz(a, new_n);
    memcpy(q, p, AVP_MIN(old_n, new_n));
    return q;
}

char *avp_strdup(AvpArena *a, const char *s) {
    size_t n = strlen(s);
    char *p = (char *)avp_alloc_noz(a, n + 1);
    memcpy(p, s, n + 1);
    return p;
}

void *avp_memdup(AvpArena *a, const void *p, size_t n) {
    void *q = avp_alloc_noz(a, n);
    if (n) memcpy(q, p, n);
    return q;
}

char *avp_vsprintf(AvpArena *a, const char *fmt, va_list ap) {
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (n < 0) avp_panic(__FILE__, __LINE__, "vsprintf failed for \"%s\"", fmt);
    char *p = (char *)avp_alloc_noz(a, (size_t)n + 1);
    vsnprintf(p, (size_t)n + 1, fmt, ap);
    return p;
}

char *avp_sprintf(AvpArena *a, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *p = avp_vsprintf(a, fmt, ap);
    va_end(ap);
    return p;
}

AvpArenaMark avp_arena_mark(AvpArena *a) {
    AvpArenaMark m = {a->cur, a->cur->used};
    return m;
}

void avp_arena_reset(AvpArena *a, AvpArenaMark mark) {
    Block *keep = (Block *)mark.block;
    AVP_ASSERT(keep != NULL);
    /* Big blocks allocated after the mark: they follow a newer normal block, or the same one at a later position. */
    while (a->bigs && (a->bigs->serial > keep->serial ||
                       (a->bigs->serial == keep->serial && a->bigs->at_used >= mark.used))) {
        Block *b = a->bigs;
        a->bigs = b->prev;
        block_free(b);
    }
    while (a->cur != keep) {
        Block *b = a->cur;
        AVP_ASSERT(b->prev != NULL && b->serial > keep->serial);
        a->cur = b->prev;
        block_free(b);
    }
    a->cur->used = mark.used;
    a->last = NULL;
}

static AVP_THREAD_LOCAL AvpArena *t_arena;
static AVP_THREAD_LOCAL AvpArena *t_scratch;

AvpArena *avp_arena(void) {
    if (!t_arena) t_arena = avp_arena_new(0);
    return t_arena;
}

AvpArena *avp_scratch(void) {
    if (!t_scratch) t_scratch = avp_arena_new(0);
    return t_scratch;
}
