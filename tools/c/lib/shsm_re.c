/* shsm_re: Python's `re` API on PCRE2 (see shsm_re.h). This file holds the pattern cache, the matching entry points
 * with per-thread match data and JIT stacks, the scan loops (finditer, findall, split, sub) with Python's
 * empty-match rules, and the replacement templates. The Python-to-PCRE2 translation is in shsm_re_translate.c. */
#include "shsm_re_priv.h"
#include <windows.h>
#include <stdio.h>
#include "pcre2.h"

enum { V_SEARCH, V_MATCH, V_FULL, V_COUNT };

struct ShsmRe {
    char *pattern; /* NUL-terminated copy of the Python pattern */
    size_t pattern_len;
    int flags_given;
    ShsmReTrans tr;
    pcre2_code *volatile code[V_COUNT]; /* compiled lazily: search, anchored (match), anchored + end-anchored */
};

/* ---- a small concurrent string-keyed table (pattern cache, template cache) ------------------------------- */

typedef struct {
    u64 h;
    char *key;
    size_t klen;
    void *val;
} CEnt;
typedef struct {
    CEnt *e;
    size_t cap, n;
    SRWLOCK lock;
} CTab;

static CTab g_pat_tab = {NULL, 0, 0, SRWLOCK_INIT};
static CTab g_tpl_tab = {NULL, 0, 0, SRWLOCK_INIT};

static u64 hash_bytes(const void *p, size_t n) {
    const u8 *b = p;
    u64 h = 1469598103934665603ull;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; }
    return h | 1;
}

static CEnt *ctab_probe(const CTab *t, const void *key, size_t klen, u64 h) {
    if (!t->cap) return NULL;
    size_t mask = t->cap - 1, i = (size_t)h & mask;
    for (;;) {
        CEnt *e = &t->e[i];
        if (!e->h) return NULL;
        if (e->h == h && e->klen == klen && memcmp(e->key, key, klen) == 0) return e;
        i = (i + 1) & mask;
    }
}

static void *ctab_get(CTab *t, const void *key, size_t klen, u64 h) {
    AcquireSRWLockShared(&t->lock);
    CEnt *e = ctab_probe(t, key, klen, h);
    void *v = e ? e->val : NULL;
    ReleaseSRWLockShared(&t->lock);
    return v;
}

/* Inserts val unless the key is there; returns the value in the table. */
static void *ctab_put(CTab *t, const void *key, size_t klen, u64 h, void *val) {
    AcquireSRWLockExclusive(&t->lock);
    CEnt *e = ctab_probe(t, key, klen, h);
    if (e) {
        void *v = e->val;
        ReleaseSRWLockExclusive(&t->lock);
        return v;
    }
    if ((t->n + 1) * 10 > t->cap * 7) {
        size_t nc = t->cap ? t->cap * 2 : 256;
        CEnt *ne = avp_xcalloc(nc, sizeof *ne);
        for (size_t i = 0; i < t->cap; i++) {
            if (!t->e[i].h) continue;
            size_t j = (size_t)t->e[i].h & (nc - 1);
            while (ne[j].h) j = (j + 1) & (nc - 1);
            ne[j] = t->e[i];
        }
        avp_xfree(t->e);
        t->e = ne;
        t->cap = nc;
    }
    size_t j = (size_t)h & (t->cap - 1);
    while (t->e[j].h) j = (j + 1) & (t->cap - 1);
    t->e[j].h = h;
    t->e[j].key = avp_xmalloc(klen ? klen : 1);
    if (klen) memcpy(t->e[j].key, key, klen);
    t->e[j].klen = klen;
    t->e[j].val = val;
    t->n++;
    ReleaseSRWLockExclusive(&t->lock);
    return val;
}

/* ---- per-thread PCRE2 state -------------------------------------------------------------------------------- */

typedef struct {
    pcre2_match_data *md;
    u32 md_cap; /* ovector pairs */
    pcre2_match_context *mctx;
    pcre2_jit_stack *stack;
    const char *valid_p; /* shsm_re_assume_valid */
    size_t valid_n;
} ReTls;
static AVP_THREAD_LOCAL ReTls g_tls;

static ReTls *tls_for(u32 pairs) {
    ReTls *T = &g_tls;
    if (!T->mctx) {
        T->mctx = pcre2_match_context_create(NULL);
        AVP_ASSERT(T->mctx);
        /* Python has no backtracking limit: set ours as high as PCRE2 allows. */
        pcre2_set_match_limit(T->mctx, 0xFFFFFFFFu);
        pcre2_set_depth_limit(T->mctx, 0xFFFFFFFFu);
        pcre2_set_heap_limit(T->mctx, 0x3FFFFFFFu); /* KiB */
        T->stack = pcre2_jit_stack_create(64 * 1024, 256u * 1024 * 1024, NULL);
        if (T->stack) pcre2_jit_stack_assign(T->mctx, NULL, T->stack);
    }
    if (!T->md || T->md_cap < pairs) {
        if (T->md) pcre2_match_data_free(T->md);
        T->md_cap = pairs < 8 ? 8 : pairs;
        T->md = pcre2_match_data_create(T->md_cap, NULL);
        AVP_ASSERT(T->md);
    }
    return T;
}

void shsm_re_assume_valid(AvpStr s) {
    g_tls.valid_p = s.p;
    g_tls.valid_n = s.n;
}

static bool utf8_valid(const u8 *p, size_t n) {
    size_t i = 0;
    while (i < n) {
        u8 c = p[i];
        if (c < 0x80) {
            /* skip ASCII runs 8 bytes at a time */
            while (i + 8 <= n) {
                u64 w;
                memcpy(&w, p + i, 8);
                if (w & 0x8080808080808080ull) break;
                i += 8;
            }
            if (i < n && p[i] < 0x80) i++;
            continue;
        }
        size_t k;
        u32 lo = 0x80, hi = 0xBF;
        if (c >= 0xC2 && c <= 0xDF) k = 1;
        else if (c >= 0xE0 && c <= 0xEF) { k = 2; if (c == 0xE0) lo = 0xA0; if (c == 0xED) hi = 0x9F; }
        else if (c >= 0xF0 && c <= 0xF4) { k = 3; if (c == 0xF0) lo = 0x90; if (c == 0xF4) hi = 0x8F; }
        else return false;
        if (i + k >= n) return false;
        if (p[i + 1] < lo || p[i + 1] > hi) return false;
        for (size_t j = 2; j <= k; j++) if ((p[i + j] & 0xC0) != 0x80) return false;
        i += k + 1;
    }
    return true;
}

/* ---- compiling --------------------------------------------------------------------------------------------- */

static pcre2_code *compile_variant(const ShsmRe *re, int variant, char *err, size_t errcap) {
    u32 opts = PCRE2_NEVER_BACKSLASH_C;
    if (!re->tr.bytes) opts |= PCRE2_UTF;
    if (re->tr.ucp) opts |= PCRE2_UCP;
    if (re->tr.caseless) opts |= PCRE2_CASELESS;
    if (variant >= V_MATCH) opts |= PCRE2_ANCHORED;
    if (variant == V_FULL) opts |= PCRE2_ENDANCHORED;
    pcre2_compile_context *cc = pcre2_compile_context_create(NULL);
    AVP_ASSERT(cc);
    pcre2_set_newline(cc, PCRE2_NEWLINE_LF);
    int ec = 0;
    PCRE2_SIZE eo = 0;
    pcre2_code *code = pcre2_compile((PCRE2_SPTR)re->tr.pcre, re->tr.pcre_len, opts, &ec, &eo, cc);
    pcre2_compile_context_free(cc);
    if (!code) {
        PCRE2_UCHAR buf[160];
        pcre2_get_error_message(ec, buf, sizeof buf);
        snprintf(err, errcap, "%s (translated pattern offset %zu)", (const char *)buf, (size_t)eo);
        return NULL;
    }
    pcre2_jit_compile(code, PCRE2_JIT_COMPLETE); /* a failure leaves the interpreter */
    return code;
}

static pcre2_code *get_code(const ShsmRe *re_c, int variant) {
    ShsmRe *re = (ShsmRe *)re_c;
    pcre2_code *c = ReadPointerAcquire((PVOID volatile *)&re->code[variant]);
    if (c) return c;
    char err[256];
    c = compile_variant(re, variant, err, sizeof err);
    if (!c) avp_panic(__FILE__, __LINE__, "regex variant failed to compile: %s", err);
    pcre2_code *prev = InterlockedCompareExchangePointer((PVOID volatile *)&re->code[variant], c, NULL);
    if (prev) {
        pcre2_code_free(c);
        c = prev;
    }
    return c;
}

bool shsm_re_compile(AvpStr pattern, int flags, ShsmRe **out, char *err, size_t errcap) {
    if (!pattern.p) pattern.p = "";
    size_t klen = 4 + pattern.n;
    u8 stack_key[260];
    u8 *key = klen <= sizeof stack_key ? stack_key : avp_xmalloc(klen);
    memcpy(key, &flags, 4);
    memcpy(key + 4, pattern.p, pattern.n);
    u64 h = hash_bytes(key, klen);
    ShsmRe *re = ctab_get(&g_pat_tab, key, klen, h);
    if (re) {
        if (key != stack_key) avp_xfree(key);
        *out = re;
        return true;
    }
    re = avp_xcalloc(1, sizeof *re);
    re->pattern = avp_xmalloc(pattern.n + 1);
    memcpy(re->pattern, pattern.p, pattern.n);
    re->pattern[pattern.n] = 0;
    re->pattern_len = pattern.n;
    re->flags_given = flags;
    bool ok = shsm_re_translate(pattern, flags, &re->tr, err, errcap);
    if (ok && re->tr.ngroups > SHSM_RE_MAX_GROUPS) {
        snprintf(err, errcap, "too many groups (avpcore supports %d)", SHSM_RE_MAX_GROUPS);
        shsm_re_trans_free(&re->tr);
        ok = false;
    }
    if (ok) {
        char perr[256];
        pcre2_code *c = compile_variant(re, V_SEARCH, perr, sizeof perr);
        if (!c) {
            snprintf(err, errcap, "%s", perr);
            shsm_re_trans_free(&re->tr);
            ok = false;
        } else {
            u32 cnt = 0;
            pcre2_pattern_info(c, PCRE2_INFO_CAPTURECOUNT, &cnt);
            AVP_ASSERT((int)cnt == re->tr.ngroups);
            re->code[V_SEARCH] = c;
        }
    }
    if (!ok) {
        avp_xfree(re->pattern);
        avp_xfree(re);
        if (key != stack_key) avp_xfree(key);
        return false;
    }
    ShsmRe *winner = ctab_put(&g_pat_tab, key, klen, h, re);
    if (winner != re) {
        pcre2_code_free(re->code[V_SEARCH]);
        shsm_re_trans_free(&re->tr);
        avp_xfree(re->pattern);
        avp_xfree(re);
    }
    if (key != stack_key) avp_xfree(key);
    *out = winner;
    return true;
}

ShsmRe *shsm_re_try_n(AvpStr pattern, int flags) {
    ShsmRe *re;
    char err[512];
    if (!shsm_re_compile(pattern, flags, &re, err, sizeof err)) avp_throw(AVP_ERR_REGEX, "%s", err);
    return re;
}

ShsmRe *shsm_re_try(const char *pattern, int flags) { return shsm_re_try_n(avp_str(pattern), flags); }

ShsmRe *shsm_re(const char *pattern, int flags) {
    ShsmRe *re;
    char err[512];
    if (!shsm_re_compile(avp_str(pattern), flags, &re, err, sizeof err))
        avp_panic(__FILE__, __LINE__, "invalid regular expression %s: %s", pattern, err);
    return re;
}

AvpStr shsm_re_pattern(const ShsmRe *re) { return avp_strn(re->pattern, re->pattern_len); }
int shsm_re_flags(const ShsmRe *re) { return re->tr.pyflags; }
int shsm_re_groups(const ShsmRe *re) { return re->tr.ngroups; }
AvpStr shsm_re_translated(const ShsmRe *re) { return avp_strn(re->tr.pcre, re->tr.pcre_len); }

int shsm_re_group_index(const ShsmRe *re, const char *name) {
    for (int g = 1; g <= re->tr.ngroups; g++)
        if (re->tr.names[g] && strcmp(re->tr.names[g], name) == 0) return g;
    return -1;
}

const char *shsm_re_group_name(const ShsmRe *re, int g) {
    AVP_ASSERT(g >= 1 && g <= re->tr.ngroups);
    return re->tr.names[g];
}

/* ---- matching ---------------------------------------------------------------------------------------------- */

static bool ancestor(const ShsmRe *re, int a, int b) {
    for (int g = re->tr.parent[b]; g; g = re->tr.parent[g]) if (g == a) return true;
    return false;
}

/* Python's lastindex: the group that closed last among the groups that took part. With the final spans that is
 * the one with the largest end; for equal ends an enclosing group closes after the groups inside it, and
 * otherwise the later group closes later. */
static int compute_lastindex(const ShsmRe *re, const i64 *sp) {
    int best = -1;
    for (int g = 1; g <= re->tr.ngroups; g++) {
        if (sp[2 * g] < 0) continue;
        if (best < 0 || sp[2 * g + 1] > sp[2 * best + 1] ||
            (sp[2 * g + 1] == sp[2 * best + 1] && !ancestor(re, best, g)))
            best = g;
    }
    return best;
}

/* One PCRE2 call. Returns true on a match and fills m (when not NULL). */
static bool run(const ShsmRe *re, int variant, AvpStr s, size_t pos, size_t endpos, u32 opts, ShsmReMatch *m) {
    ReTls *T = tls_for((u32)re->tr.ngroups + 1);
    pcre2_code *code = get_code(re, variant);
    int rc = pcre2_match(code, (PCRE2_SPTR)s.p, endpos, pos, opts | PCRE2_NO_UTF_CHECK, T->md, T->mctx);
    if (rc == PCRE2_ERROR_JIT_STACKLIMIT)
        rc = pcre2_match(code, (PCRE2_SPTR)s.p, endpos, pos, opts | PCRE2_NO_UTF_CHECK | PCRE2_NO_JIT, T->md, T->mctx);
    if (rc == PCRE2_ERROR_NOMATCH) return false;
    if (rc < 0) {
        PCRE2_UCHAR buf[160];
        pcre2_get_error_message(rc, buf, sizeof buf);
        avp_throw(AVP_ERR_REGEX, "regex matching failed: %s", (const char *)buf);
    }
    if (m) {
        const PCRE2_SIZE *ov = pcre2_get_ovector_pointer(T->md);
        int ng = re->tr.ngroups;
        m->re = re;
        m->string = s;
        for (int g = 0; g <= ng; g++) {
            if (g < rc && ov[2 * g] != PCRE2_UNSET) {
                m->span[2 * g] = (i64)ov[2 * g];
                m->span[2 * g + 1] = (i64)ov[2 * g + 1];
            } else {
                m->span[2 * g] = m->span[2 * g + 1] = -1;
            }
        }
        m->lastindex = ng ? compute_lastindex(re, m->span) : -1;
    }
    return true;
}

static void check_subject(const ShsmRe *re, AvpStr s, size_t endpos) {
    if (re->tr.bytes || endpos == 0) return;
    const ReTls *T = &g_tls;
    if (T->valid_p && s.p >= T->valid_p && s.p + endpos <= T->valid_p + T->valid_n) return;
    if (!utf8_valid((const u8 *)s.p, endpos)) avp_throw(AVP_ERR_FORMAT, "re: the subject is not valid UTF-8");
}

/* Python's pos / endpos clamping; false when pos > endpos (no match possible). */
static bool clamp_range(AvpStr *s, i64 pos, i64 endpos, size_t *p, size_t *e) {
    if (!s->p) s->p = "";
    if (pos < 0) pos = 0;
    if (pos > (i64)s->n) pos = (i64)s->n;
    if (endpos < 0) endpos = 0;
    if (endpos > (i64)s->n) endpos = (i64)s->n;
    *p = (size_t)pos;
    *e = (size_t)endpos;
    return pos <= endpos;
}

static bool do_match(const ShsmRe *re, int variant, AvpStr s, i64 pos, i64 endpos, ShsmReMatch *m) {
    size_t p, e;
    if (!clamp_range(&s, pos, endpos, &p, &e)) return false;
    check_subject(re, s, e);
    if (m) { m->pos = (i64)p; m->endpos = (i64)e; }
    return run(re, variant, s, p, e, 0, m);
}

bool shsm_re_search(const ShsmRe *re, AvpStr s, i64 pos, i64 endpos, ShsmReMatch *m) {
    return do_match(re, V_SEARCH, s, pos, endpos, m);
}
bool shsm_re_match(const ShsmRe *re, AvpStr s, i64 pos, i64 endpos, ShsmReMatch *m) {
    return do_match(re, V_MATCH, s, pos, endpos, m);
}
bool shsm_re_fullmatch(const ShsmRe *re, AvpStr s, i64 pos, i64 endpos, ShsmReMatch *m) {
    return do_match(re, V_FULL, s, pos, endpos, m);
}

/* ---- match accessors ------------------------------------------------------------------------------------- */

i64 shsm_re_start(const ShsmReMatch *m, int g) {
    AVP_ASSERT(g >= 0 && g <= m->re->tr.ngroups);
    return m->span[2 * g];
}
i64 shsm_re_end(const ShsmReMatch *m, int g) {
    AVP_ASSERT(g >= 0 && g <= m->re->tr.ngroups);
    return m->span[2 * g + 1];
}
bool shsm_re_matched(const ShsmReMatch *m, int g) { return shsm_re_start(m, g) >= 0; }
AvpStr shsm_re_group(const ShsmReMatch *m, int g) {
    i64 s = shsm_re_start(m, g);
    if (s < 0) return (AvpStr){NULL, 0};
    return (AvpStr){m->string.p + s, (size_t)(m->span[2 * g + 1] - s)};
}
AvpStr shsm_re_group_named(const ShsmReMatch *m, const char *name) {
    int g = shsm_re_group_index(m->re, name);
    AVP_ASSERT(g >= 0);
    return shsm_re_group(m, g);
}
const char *shsm_re_lastgroup(const ShsmReMatch *m) {
    return m->lastindex > 0 ? m->re->tr.names[m->lastindex] : NULL;
}

/* ---- finditer -------------------------------------------------------------------------------------------- */

void shsm_re_iter_init(ShsmReIter *it, const ShsmRe *re, AvpStr s, i64 pos, i64 endpos) {
    size_t p, e;
    memset(it, 0, sizeof *it);
    it->re = re;
    it->done = !clamp_range(&s, pos, endpos, &p, &e);
    it->s = s;
    it->pos = (i64)p;
    it->endpos = (i64)e;
    if (!it->done) check_subject(re, s, e);
}

bool shsm_re_iter_next(ShsmReIter *it, ShsmReMatch *m) {
    if (it->done) return false;
    u32 opts = it->must_advance ? PCRE2_NOTEMPTY_ATSTART : 0;
    m->pos = it->pos;
    m->endpos = it->endpos;
    if (!run(it->re, V_SEARCH, it->s, (size_t)it->pos, (size_t)it->endpos, opts, m)) {
        it->done = true;
        return false;
    }
    it->must_advance = m->span[1] == m->span[0];
    it->pos = m->span[1];
    return true;
}

/* ---- findall / split ------------------------------------------------------------------------------------- */

ShsmReFindall shsm_re_findall(const ShsmRe *re, AvpStr s, i64 pos, i64 endpos) {
    ShsmReFindall r = {0};
    int ng = re->tr.ngroups;
    r.width = ng <= 1 ? 1 : ng;
    ShsmReIter it;
    ShsmReMatch m;
    shsm_re_iter_init(&it, re, s, pos, endpos);
    while (shsm_re_iter_next(&it, &m)) {
        if (ng == 0) {
            avp_vec_push(&r.v, shsm_re_group(&m, 0));
        } else {
            for (int g = 1; g <= ng; g++) {
                AvpStr x = shsm_re_group(&m, g);
                if (!x.p) x.p = "";
                avp_vec_push(&r.v, x);
                if (ng == 1) break;
            }
        }
        r.count++;
    }
    return r;
}

AvpStr shsm_re_findall_at(const ShsmReFindall *r, size_t i, int j) {
    AVP_ASSERT(i < r->count && j >= 0 && j < r->width);
    return r->v.v[i * (size_t)r->width + (size_t)j];
}

void shsm_re_findall_free(ShsmReFindall *r) {
    avp_vec_free(&r->v);
    r->count = 0;
}

AvpStrVec shsm_re_split(const ShsmRe *re, AvpStr s, int maxsplit) {
    AvpStrVec out = {0};
    if (!s.p) s.p = "";
    int ng = re->tr.ngroups;
    ShsmReIter it;
    ShsmReMatch m;
    shsm_re_iter_init(&it, re, s, 0, AVP_END);
    i64 last = 0;
    int n = 0;
    while ((maxsplit == 0 || n < maxsplit) && shsm_re_iter_next(&it, &m)) {
        i64 b = m.span[0];
        avp_vec_push(&out, avp_strn(s.p + last, (size_t)(b - last)));
        for (int g = 1; g <= ng; g++) avp_vec_push(&out, shsm_re_group(&m, g));
        n++;
        last = m.span[1];
    }
    avp_vec_push(&out, avp_strn(s.p + last, s.n - (size_t)last));
    return out;
}

/* ---- templates --------------------------------------------------------------------------------------------- */

typedef struct {
    int group; /* -1: literal */
    u32 off, len;
} ReTplPiece;
typedef struct ReTpl {
    int n;
    ReTplPiece *pc;
    char *lit;
} ReTpl;

AVP_NORETURN static void tpl_err(AvpStr t, bool bytes, size_t pos, AVP_PRINTF_FMT const char *fmt, ...) {
    char msg[200], full[400];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    shsm_re_format_error(full, sizeof full, msg, t, pos, bytes);
    avp_throw(AVP_ERR_REGEX, "%s", full);
}

static bool tpl_is_ident(const u8 *s, size_t n, bool bytes) {
    if (n == 0) return false;
    for (size_t k = 0; k < n; k++) {
        u8 c = s[k];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (k > 0 && c >= '0' && c <= '9') ||
                  (c >= 0x80 && !bytes);
        if (!ok) return false;
    }
    return true;
}

static ReTpl *tpl_parse(const ShsmRe *re, AvpStr t) {
    bool bytes = re->tr.bytes;
    int ngroups = re->tr.ngroups;
    const u8 *p = (const u8 *)t.p;
    size_t n = t.n, i = 0;
    AvpBuf lit = {0};
    ReTplPiece *pcs = NULL;
    int npc = 0, cap = 0;
    u32 lit_start = 0; /* start of the current literal run inside lit */

#define FLUSH_LIT()                                                                                  \
    do {                                                                                             \
        if (lit.n > lit_start) {                                                                     \
            if (npc == cap) { cap = cap ? cap * 2 : 8; pcs = avp_xrealloc(pcs, (size_t)cap * sizeof *pcs); } \
            pcs[npc++] = (ReTplPiece){-1, lit_start, (u32)(lit.n - lit_start)};                      \
            lit_start = (u32)lit.n;                                                                  \
        }                                                                                            \
    } while (0)
#define ADD_GROUP(gi)                                                                                \
    do {                                                                                             \
        FLUSH_LIT();                                                                                 \
        if (npc == cap) { cap = cap ? cap * 2 : 8; pcs = avp_xrealloc(pcs, (size_t)cap * sizeof *pcs); } \
        pcs[npc++] = (ReTplPiece){(gi), 0, 0};                                                       \
    } while (0)

    while (i < n) {
        if (p[i] != '\\') {
            size_t j = i;
            while (j < n && p[j] != '\\') j++;
            avp_buf_putn(&lit, p + i, j - i);
            i = j;
            continue;
        }
        size_t bs = i++;
        if (i >= n) tpl_err(t, bytes, bs, "bad escape (end of pattern)");
        u8 c = p[i];
        size_t clen = 1;
        if (!bytes && c >= 0x80) clen = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : 2;
        i += clen;
        if (c == 'g') {
            if (i >= n || p[i] != '<') tpl_err(t, bytes, i, "missing <");
            i++;
            size_t st = i;
            while (i < n && p[i] != '>') i++;
            if (i >= n) {
                if (i == st) tpl_err(t, bytes, i, "missing group name");
                tpl_err(t, bytes, st, "missing >, unterminated name");
            }
            if (i == st) tpl_err(t, bytes, i, "missing group name");
            const u8 *name = p + st;
            size_t nl = i - st;
            i++;
            bool digits = true;
            for (size_t k = 0; k < nl; k++) if (name[k] < '0' || name[k] > '9') digits = false;
            long long idx;
            if (!digits) {
                if (!tpl_is_ident(name, nl, bytes))
                    tpl_err(t, bytes, st, "bad character in group name '%.*s'", (int)nl, name);
                idx = -1;
                for (int g = 1; g <= ngroups; g++)
                    if (re->tr.names[g] && strlen(re->tr.names[g]) == nl && memcmp(re->tr.names[g], name, nl) == 0) idx = g;
                if (idx < 0) avp_throw(AVP_ERR_REGEX, "unknown group name '%.*s'", (int)nl, name);
            } else {
                idx = 0;
                for (size_t k = 0; k < nl && idx < (1ll << 40); k++) idx = idx * 10 + (name[k] - '0');
                if (idx >= (1ll << 30)) tpl_err(t, bytes, st, "invalid group reference %lld", idx);
            }
            if (idx > ngroups) tpl_err(t, bytes, st, "invalid group reference %lld", idx);
            ADD_GROUP((int)idx);
        } else if (c == '0') {
            u32 v = 0;
            int more = 0;
            while (more < 2 && i < n && p[i] >= '0' && p[i] <= '7') { v = v * 8 + (u32)(p[i] - '0'); i++; more++; }
            v &= 0xFF;
            if (bytes) avp_buf_putc(&lit, (char)v); else avp_buf_putcp(&lit, v);
        } else if (c >= '1' && c <= '9') {
            u32 group = (u32)(c - '0');
            bool isoctal = false;
            if (i < n && p[i] >= '0' && p[i] <= '9') {
                u8 d2 = p[i++];
                group = group * 10 + (u32)(d2 - '0');
                if (c <= '7' && d2 <= '7' && i < n && p[i] >= '0' && p[i] <= '7') {
                    u32 v = (u32)(c - '0') * 64 + (u32)(d2 - '0') * 8 + (u32)(p[i] - '0');
                    i++;
                    if (v > 0377) tpl_err(t, bytes, bs, "octal escape value %.*s outside of range 0-0o377", (int)(i - bs), p + bs);
                    if (bytes) avp_buf_putc(&lit, (char)v); else avp_buf_putcp(&lit, v);
                    isoctal = true;
                }
            }
            if (!isoctal) {
                if ((int)group > ngroups) tpl_err(t, bytes, bs + 1, "invalid group reference %u", group);
                ADD_GROUP((int)group);
            }
        } else {
            char esc = 0;
            switch (c) {
            case 'a': esc = 7; break;
            case 'b': esc = 8; break;
            case 'f': esc = 12; break;
            case 'n': esc = 10; break;
            case 'r': esc = 13; break;
            case 't': esc = 9; break;
            case 'v': esc = 11; break;
            case '\\': esc = '\\'; break;
            default: break;
            }
            if (esc) {
                avp_buf_putc(&lit, esc);
            } else {
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) tpl_err(t, bytes, bs, "bad escape \\%c", c);
                avp_buf_putc(&lit, '\\');
                avp_buf_putn(&lit, p + bs + 1, clen);
            }
        }
    }
    FLUSH_LIT();
#undef FLUSH_LIT
#undef ADD_GROUP
    ReTpl *tp = avp_xcalloc(1, sizeof *tp);
    tp->n = npc;
    tp->pc = pcs;
    tp->lit = avp_xmalloc(lit.n + 1);
    if (lit.n) memcpy(tp->lit, lit.p, lit.n);
    tp->lit[lit.n] = 0;
    avp_buf_free(&lit);
    return tp;
}

static const ReTpl *tpl_get(const ShsmRe *re, AvpStr t) {
    size_t klen = sizeof re + t.n;
    u8 stack_key[200];
    u8 *key = klen <= sizeof stack_key ? stack_key : avp_xmalloc(klen);
    memcpy(key, &re, sizeof re);
    if (t.n) memcpy(key + sizeof re, t.p, t.n);
    u64 h = hash_bytes(key, klen);
    ReTpl *tp = ctab_get(&g_tpl_tab, key, klen, h);
    if (!tp) {
        tp = tpl_parse(re, t);
        ReTpl *w = ctab_put(&g_tpl_tab, key, klen, h, tp);
        if (w != tp) {
            avp_xfree(tp->pc);
            avp_xfree(tp->lit);
            avp_xfree(tp);
            tp = w;
        }
    }
    if (key != stack_key) avp_xfree(key);
    return tp;
}

static void tpl_expand(AvpBuf *out, const ReTpl *tp, const ShsmReMatch *m) {
    for (int k = 0; k < tp->n; k++) {
        const ReTplPiece *pc = &tp->pc[k];
        if (pc->group < 0) {
            avp_buf_putn(out, tp->lit + pc->off, pc->len);
        } else {
            i64 st = m->span[2 * pc->group];
            if (st >= 0) avp_buf_putn(out, m->string.p + st, (size_t)(m->span[2 * pc->group + 1] - st));
        }
    }
}

void shsm_re_expand(AvpBuf *out, const ShsmReMatch *m, AvpStr tmpl) {
    if (!tmpl.p) tmpl.p = "";
    if (!memchr(tmpl.p, '\\', tmpl.n)) {
        avp_buf_put(out, tmpl);
        return;
    }
    tpl_expand(out, tpl_get(m->re, tmpl), m);
}

/* ---- sub / subn -------------------------------------------------------------------------------------------- */

static AvpStr sub_impl(AvpArena *a, const ShsmRe *re, const ReTpl *tp, AvpStr lit_repl, ShsmReSubFn fn, void *ctx,
                       AvpStr s, int count, size_t *n_out) {
    if (!s.p) s.p = "";
    ShsmReIter it;
    ShsmReMatch m;
    AvpBuf out;
    avp_buf_init(&out, a);
    shsm_re_iter_init(&it, re, s, 0, AVP_END);
    size_t n = 0;
    i64 last = 0;
    while ((count == 0 || (int)n < count) && shsm_re_iter_next(&it, &m)) {
        i64 b = m.span[0];
        if (last < b) avp_buf_putn(&out, s.p + last, (size_t)(b - last));
        if (fn) fn(ctx, &m, &out);
        else if (tp) tpl_expand(&out, tp, &m);
        else avp_buf_put(&out, lit_repl);
        last = m.span[1];
        n++;
    }
    if (n_out) *n_out = n;
    if (n == 0) return s;
    if ((size_t)last < s.n) avp_buf_putn(&out, s.p + last, s.n - (size_t)last);
    AvpStr r = avp_buf_str(&out);
    return r.p ? r : AVP_S("");
}

AvpStr shsm_re_subn(AvpArena *a, const ShsmRe *re, AvpStr repl, AvpStr s, int count, size_t *n) {
    if (!repl.p) repl.p = "";
    const ReTpl *tp = NULL;
    if (memchr(repl.p, '\\', repl.n)) tp = tpl_get(re, repl);
    return sub_impl(a, re, tp, repl, NULL, NULL, s, count, n);
}

AvpStr shsm_re_sub(AvpArena *a, const ShsmRe *re, AvpStr repl, AvpStr s, int count) {
    return shsm_re_subn(a, re, repl, s, count, NULL);
}

AvpStr shsm_re_sub_fn(AvpArena *a, const ShsmRe *re, ShsmReSubFn fn, void *ctx, AvpStr s, int count, size_t *n) {
    return sub_impl(a, re, NULL, AVP_S(""), fn, ctx, s, count, n);
}

/* ---- escape ------------------------------------------------------------------------------------------------ */

AvpStr shsm_re_escape(AvpArena *a, AvpStr s) {
    static const char special[] = "()[]{}?*+-|^$\\.&~# \t\n\r\v\f";
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < s.n; i++) {
        char c = s.p[i];
        if (c && strchr(special, c)) avp_buf_putc(&b, '\\');
        avp_buf_putc(&b, c);
    }
    AvpStr r = avp_buf_str(&b);
    return r.p ? r : AVP_S("");
}
