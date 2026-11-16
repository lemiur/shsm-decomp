/* unit_plan.py: the evidence per gap (_spans, _cover, gap_evidence, class_spans), predict and combine, the ground
 * truth and its scoring (truth, evaluate, bands, accuracy_header), and plan (plan, _path_for, _conf). */
#include "unit_plan.h"

#include "shsm_re.h"
#include "shsm_symbols.h"

#include <string.h>

static const char *const PRIVATE_SECTIONS[] = {".sdata2", ".rodata", ".sbss2"};

static bool is_private_section(const char *s) {
    for (size_t i = 0; i < 3; i++)
        if (s && strcmp(s, PRIVATE_SECTIONS[i]) == 0) return true;
    return false;
}

static int cmp_i64(const void *x, const void *y, void *ctx) {
    (void)ctx;
    i64 a = *(const i64 *)x, b = *(const i64 *)y;
    return a < b ? -1 : a > b;
}

/* ---- spans ---- */

typedef struct {
    i64 lo, hi;
    bool strong;
} Span;
typedef AVP_VEC(Span) Spans;

/* users: {data address: [function indices]} in dict order (keys and their index lists). */
typedef struct {
    AvpI64Vec addrs;
    AVP_VEC(AvpI64Vec) idx;
} Users;

static Users users_of(const UpModel *m) {
    Users u = {0};
    AvpIMap *at = avp_imap_new();
    for (size_t i = 0; i < m->n; i++) {
        const UpFunc *f = &m->fns[i];
        for (size_t k = 0; k < f->data.n; k++) {
            bool created;
            AvpVal *v = avp_imap_put(at, (u64)f->data.v[k], &created);
            if (created) {
                v->i = (i64)u.addrs.n;
                avp_vec_push(&u.addrs, f->data.v[k]);
                avp_vec_push_zero(&u.idx);
            }
            avp_vec_push(&u.idx.v[v->i], (i64)i);
        }
    }
    avp_imap_free(at);
    return u;
}

static Spans spans_of(const UpModel *m, const Users *u) {
    Spans sp = {0};
    for (size_t k = 0; k < u->addrs.n; k++) {
        const UpDatum *d = up_model_datum(m, u->addrs.v[k]);
        const AvpI64Vec *idx = &u->idx.v[k];
        i64 lo = idx->v[0], hi = idx->v[idx->n - 1]; /* indices were appended in increasing order */
        if (lo == hi || !d) continue;
        bool priv = is_private_section(d->section) || d->kind != UP_KIND_OTHER ||
                    (d->scope && strcmp(d->scope, "local") == 0);
        if (priv && hi - lo <= UP_MAX_SPAN) avp_vec_push(&sp, ((Span){lo, hi, true}));
        else if (!priv && hi - lo <= UP_LOOSE_SPAN) avp_vec_push(&sp, ((Span){lo, hi, false}));
    }
    for (size_t k = 0; k < m->nlocal; k++) {
        i64 i = up_model_index(m, m->local_calls[k][0]), j = up_model_index(m, m->local_calls[k][1]);
        if (i >= 0 && j >= 0 && (i > j ? i - j : j - i) <= UP_MAX_SPAN)
            avp_vec_push(&sp, ((Span){i < j ? i : j, i < j ? j : i, true}));
    }
    return sp;
}

static i32 *cover(AvpArena *a, size_t n, const Spans *sp, bool strong_only) {
    i32 *diff = AVP_NEWN(a, i32, n + 1);
    for (size_t k = 0; k < sp->n; k++)
        if (sp->v[k].strong || !strong_only) {
            diff[sp->v[k].lo] += 1;
            diff[sp->v[k].hi] -= 1;
        }
    i32 *out = AVP_NEWN(a, i32, n ? n : 1);
    i32 run = 0;
    for (size_t g = 0; g + 1 < n; g++) {
        run += diff[g];
        out[g] = run;
    }
    return out;
}

/* class_spans(m): inside[g] = the gap closing the merged class group (-1: none); is_end[g]. */
static void class_spans(AvpArena *a, const UpModel *m, i64 *inside, bool *is_end) {
    AVP_VEC(i64) iv = {0}; /* pairs */
    for (size_t c = 0; c < m->nclasses; c++) {
        i64 lo = INT64_MAX, hi = INT64_MIN;
        size_t found = 0;
        for (size_t k = 0; k < m->classes[c].addrs.n; k++) {
            i64 i = up_model_index(m, m->classes[c].addrs.v[k]);
            if (i < 0) continue;
            found++;
            if (i < lo) lo = i;
            if (i > hi) hi = i;
        }
        if (found >= 2) {
            avp_vec_push(&iv, lo);
            avp_vec_push(&iv, hi);
        }
    }
    size_t ni = iv.n / 2;
    i64 *pairs = AVP_NEWN(a, i64, iv.n ? iv.n : 1);
    if (iv.n) memcpy(pairs, iv.v, iv.n * sizeof *pairs);
    avp_vec_free(&iv);
    /* sort (lo, hi) tuples */
    for (size_t i = 1; i < ni; i++) {
        i64 l = pairs[2 * i], h = pairs[2 * i + 1];
        size_t j = i;
        while (j && (pairs[2 * (j - 1)] > l || (pairs[2 * (j - 1)] == l && pairs[2 * (j - 1) + 1] > h))) {
            pairs[2 * j] = pairs[2 * (j - 1)];
            pairs[2 * j + 1] = pairs[2 * (j - 1) + 1];
            j--;
        }
        pairs[2 * j] = l;
        pairs[2 * j + 1] = h;
    }
    AVP_VEC(i64) merged = {0};
    for (size_t i = 0; i < ni; i++) {
        i64 lo = pairs[2 * i], hi = pairs[2 * i + 1];
        if (merged.n && lo < merged.v[merged.n - 1]) {
            if (hi > merged.v[merged.n - 1]) merged.v[merged.n - 1] = hi;
        } else {
            avp_vec_push(&merged, lo);
            avp_vec_push(&merged, hi);
        }
    }
    for (size_t i = 0; i < merged.n; i += 2) {
        for (i64 g = merged.v[i]; g < merged.v[i + 1]; g++) inside[g] = merged.v[i + 1];
        is_end[merged.v[i + 1]] = true;
    }
    avp_vec_free(&merged);
}

static bool free_gap(const UpModel *m, const i32 *strong, i64 g) {
    return g >= 0 && g < (i64)m->n - 1 && strong[g] == 0;
}

/* the window rule of dupfloat, dupstr and file: one free gap between hi1 and lo2 gets `tag`, several the last
 * gets `tag~` */
static void window_tag(const UpModel *m, const i32 *strong, UpTags *tags, i64 hi1, i64 lo2, int tag) {
    i64 count = 0, last = -1;
    for (i64 g = hi1; g < lo2; g++)
        if (free_gap(m, strong, g)) {
            count++;
            last = g;
        }
    if (count == 1) tags[last] |= UP_BIT(tag);
    else if (count) tags[last] |= UP_BIT(tag + 1);
}

typedef struct {
    i64 addr, lo, hi;
} Copy;

static int cmp_copy(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const Copy *a = x, *b = y;
    if (a->addr != b->addr) return a->addr < b->addr ? -1 : 1;
    if (a->lo != b->lo) return a->lo < b->lo ? -1 : 1;
    return a->hi < b->hi ? -1 : a->hi > b->hi;
}

typedef struct {
    i64 lo, hi;
    AvpStr name;
} FileUse;

typedef AVP_VEC(Copy) CopyVec;

static int cmp_file(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const FileUse *a = x, *b = y;
    if (a->lo != b->lo) return a->lo < b->lo ? -1 : 1;
    if (a->hi != b->hi) return a->hi < b->hi ? -1 : 1;
    return avp_str_cmp(a->name, b->name);
}

/* per-section address lists of a few functions' data (sections in first-seen order) */
typedef struct {
    const char *sec[16];
    i64 lo[16], hi[16];
    size_t n;
} SecRange;

static void sec_add(SecRange *s, const char *sec, i64 addr) {
    for (size_t i = 0; i < s->n; i++)
        if (strcmp(s->sec[i], sec) == 0) {
            if (addr < s->lo[i]) s->lo[i] = addr;
            if (addr > s->hi[i]) s->hi[i] = addr;
            return;
        }
    AVP_ASSERT(s->n < 16);
    s->sec[s->n] = sec;
    s->lo[s->n] = s->hi[s->n] = addr;
    s->n++;
}

static const UpPads *pads_of(const UpModel *m, const char *sec) {
    for (size_t i = 0; i < m->npads; i++)
        if (strcmp(m->pads[i].section, sec) == 0) return &m->pads[i];
    return NULL;
}

UpEvidence up_gap_evidence(AvpArena *a, const UpModel *m) {
    size_t n = m->n;
    UpEvidence ev = {0};
    ev.ngaps = n ? n - 1 : 0;
    Users users = users_of(m);
    Spans sp = spans_of(m, &users);
    ev.strong = cover(a, n, &sp, true);
    ev.loose = cover(a, n, &sp, false);
    avp_vec_free(&sp);
    const i32 *strong = ev.strong;
    UpTags *tags = ev.tags = AVP_NEWN(a, UpTags, n ? n : 1);
    i64 *inside = AVP_NEWN(a, i64, n ? n : 1);
    bool *is_end = AVP_NEWN(a, bool, n ? n : 1);
    for (size_t g = 0; g < n; g++) inside[g] = -1;
    class_spans(a, m, inside, is_end);
    for (size_t g = 0; g < n; g++) {
        if (inside[g] >= 0) tags[g] |= UP_BIT(UP_T_INCLASS);
        if (is_end[g] && g + 1 < n) tags[g] |= UP_BIT(UP_T_CLASS);
    }
    /* sinit, moved past the trailing factory and thunks, and on to the end of a class group */
    for (size_t i = 0; i < n; i++) {
        const UpFunc *f = &m->fns[i];
        if (!f->sinit) continue;
        size_t g = i;
        int other = 0;
        while (g + 1 < n && !m->fns[g + 1].sinit) {
            const UpFunc *nx = &m->fns[g + 1];
            bool taken = false;
            for (size_t k = 0; !taken && k < f->taken.n; k++) taken = f->taken.v[k] == nx->address;
            if (nx->thunk != UP_NONE || nx->name[0] == '@') g++;
            else if (taken && other < UP_TAIL_MAX) {
                g++;
                other++;
            } else break;
        }
        while (g < n && inside[g] >= 0) g = (size_t)inside[g];
        if (g + 1 < n) tags[g] |= UP_BIT(UP_T_SINIT);
    }
    /* duplicate constants */
    for (int pass = 0; pass < 2; pass++) {
        UpKind kind = pass == 0 ? UP_KIND_FLOAT : UP_KIND_STRING;
        int tag = pass == 0 ? UP_T_DUPFLOAT : UP_T_DUPSTR;
        AvpMap *groups = avp_map_new();
        AVP_VEC(CopyVec *) lists = {0};
        AvpArena *s = avp_scratch();
        AvpArenaMark mk = avp_arena_mark(s);
        for (size_t k = 0; k < users.addrs.n; k++) {
            const UpDatum *d = up_model_datum(m, users.addrs.v[k]);
            if (!d || d->kind != kind || !d->value.p || !(kind == UP_KIND_STRING || is_private_section(d->section)))
                continue;
            AvpBuf key;
            avp_buf_init(&key, s);
            avp_buf_printf(&key, "%zu:%s|", strlen(d->section), d->section);
            avp_buf_put(&key, d->value);
            bool created;
            AvpVal *v = avp_map_put(groups, avp_buf_str(&key), &created);
            if (created) {
                v->i = (i64)lists.n;
                avp_vec_push(&lists, AVP_NEW(a, CopyVec));
            }
            const AvpI64Vec *idx = &users.idx.v[k];
            avp_vec_push(lists.v[v->i], ((Copy){d->address, idx->v[0], idx->v[idx->n - 1]}));
        }
        for (size_t c = 0; c < lists.n; c++) {
            Copy *cs = lists.v[c]->v;
            size_t nc = lists.v[c]->n;
            avp_sort(cs, nc, sizeof *cs, cmp_copy, NULL, false);
            for (size_t k = 0; k + 1 < nc; k++) {
                if (cs[k].hi >= cs[k + 1].lo) continue;
                window_tag(m, strong, tags, cs[k].hi, cs[k + 1].lo, tag);
            }
            avp_vec_free(lists.v[c]);
        }
        avp_vec_free(&lists);
        avp_map_free(groups);
        avp_arena_reset(s, mk);
    }
    /* unit file names */
    {
        AvpMap *files = avp_map_new();
        AVP_VEC(FileUse) uses = {0};
        const ShsmRe *re = shsm_re("\\.(?:c|cpp)$", 0);
        for (size_t i = 0; i < n; i++)
            for (size_t k = 0; k < m->fns[i].strings.n; k++) {
                AvpStr t = m->fns[i].strings.v[k];
                ShsmReMatch mm;
                if (!shsm_re_search(re, t, 0, AVP_END, &mm)) continue;
                bool created;
                AvpVal *v = avp_map_put(files, t, &created);
                if (created) {
                    v->i = (i64)uses.n;
                    avp_vec_push(&uses, ((FileUse){(i64)i, (i64)i, t}));
                } else {
                    FileUse *u = &uses.v[v->i];
                    if ((i64)i < u->lo) u->lo = (i64)i;
                    if ((i64)i > u->hi) u->hi = (i64)i;
                }
            }
        avp_sort(uses.v, uses.n, sizeof *uses.v, cmp_file, NULL, false);
        for (size_t k = 0; k + 1 < uses.n; k++)
            if (uses.v[k].hi < uses.v[k + 1].lo) window_tag(m, strong, tags, uses.v[k].hi, uses.v[k + 1].lo, UP_T_FILE);
        avp_vec_free(&uses);
        avp_map_free(files);
    }
    /* weak copies close their unit */
    for (size_t g = 0; g + 1 < n; g++)
        if (strcmp(m->fns[g].scope, "weak") == 0 && strcmp(m->fns[g + 1].scope, "weak") != 0)
            tags[g] |= UP_BIT(UP_T_WEAK);
    /* nocross between neighbouring anchors */
    AvpI64Vec anchors = {0};
    for (size_t i = 0; i < n; i++)
        if (m->fns[i].data.n) avp_vec_push(&anchors, (i64)i);
    for (size_t k = 0; k + 1 < anchors.n; k++) {
        i64 x = anchors.v[k], y = anchors.v[k + 1];
        bool all = true;
        for (i64 g = x; all && g < y; g++) all = free_gap(m, strong, g);
        if (all) tags[y - 1] |= UP_BIT(UP_T_NOCROSS);
    }
    /* padding between packed functions */
    for (size_t g = 0; g + 1 < n; g++) {
        const UpFunc *f = &m->fns[g], *nx = &m->fns[g + 1];
        AvpVal *al = avp_imap_get(m->align, (u64)f->address);
        if (up_fn_end(f) < nx->address && !(al && al->i == 16)) tags[g] |= UP_BIT(UP_T_TEXTPAD);
    }
    /* data evidence between neighbouring anchors */
    for (size_t k = 0; k + 1 < anchors.n; k++) {
        i64 x = anchors.v[k], y = anchors.v[k + 1];
        SecRange left = {0}, right = {0};
        for (i64 i = x - 2 > 0 ? x - 2 : 0; i <= x; i++)
            for (size_t j = 0; j < m->fns[i].data.n; j++) {
                const UpDatum *d = up_model_datum(m, m->fns[i].data.v[j]);
                if (d) sec_add(&left, d->section, d->address);
            }
        for (i64 i = y; i < (i64)n && i < y + 3; i++)
            for (size_t j = 0; j < m->fns[i].data.n; j++) {
                const UpDatum *d = up_model_datum(m, m->fns[i].data.v[j]);
                if (d) sec_add(&right, d->section, d->address);
            }
        size_t shared[16], ns = 0, rk[16];
        for (size_t i = 0; i < left.n; i++)
            for (size_t j = 0; j < right.n; j++)
                if (strcmp(left.sec[i], right.sec[j]) == 0) {
                    rk[ns] = j;
                    shared[ns++] = i;
                }
        if (!ns) continue;
        bool advance = true;
        for (size_t s = 0; advance && s < ns; s++) advance = left.hi[shared[s]] < right.lo[rk[s]];
        if (!advance) continue;
        tags[y - 1] |= UP_BIT(UP_T_ORDER);
        for (size_t s = 0; s < ns; s++) {
            const UpPads *p = pads_of(m, left.sec[shared[s]]);
            if (!p) continue;
            i64 maxl = left.hi[shared[s]], minr = right.lo[rk[s]];
            size_t lo = 0, hi = p->points.n;
            while (lo < hi) {
                size_t mid = (lo + hi) / 2;
                if (maxl < p->points.v[mid]) hi = mid;
                else lo = mid + 1;
            }
            if (lo < p->points.n && p->points.v[lo] <= minr) {
                tags[y - 1] |= UP_BIT(UP_T_DATAPAD);
                break;
            }
        }
    }
    avp_vec_free(&anchors);
    /* PS2 link order between neighbouring mapped functions: PS2 neighbours mark every gap between them ps2run,
     * functions far apart on PS2 put ps2 (one free gap) or ps2~ (several) on the window rule's gap */
    {
        i64 prev = -1;
        for (size_t i = 0; i < n; i++) {
            if (!m->fns[i].has_ps2) continue;
            if (prev >= 0 && (i64)i - prev <= UP_PS2_SPAN) {
                i64 d = m->fns[i].ps2 - m->fns[prev].ps2;
                if (d < 0) d = -d;
                if (d <= UP_PS2_NEAR)
                    for (i64 g = prev; g < (i64)i; g++) tags[g] |= UP_BIT(UP_T_PS2RUN);
                else
                    window_tag(m, strong, tags, prev, (i64)i, UP_T_PS2);
            }
            prev = (i64)i;
        }
    }
    for (size_t k = 0; k < users.idx.n; k++) avp_vec_free(&users.idx.v[k]);
    avp_vec_free(&users.idx);
    avp_vec_free(&users.addrs);
    return ev;
}

/* ---- predict, combine ---- */

double up_combine(UpTags ts, const UpCalib *calib) {
    if (!calib) return 0.5;
    /* PS2 link order on top of other evidence: the probability without it, its odds times the calibrated
     * likelihood ratio of each PS2 tag present (combinations are calibrated without the PS2 tags: up_evaluate) */
    UpTags base = ts & ~UP_PS2_TAGS;
    if ((ts & UP_PS2_TAGS) && (base & (UP_BIT(UP_NPOSITIVE) - 1))) {
        double p = up_combine(base, calib);
        if (p <= 0 || p >= 1) return p;
        double odds = p / (1 - p);
        for (int k = 0; k < UP_NPS2; k++)
            if (ts & UP_BIT(UP_PS2_TAG[k])) odds *= up_ps2_ratio(calib, k);
        return odds / (1 + odds);
    }
    AvpArena *s = avp_scratch();
    AvpArenaMark mk = avp_arena_mark(s);
    AvpStr key = up_tag_key(s, ts);
    AvpVal *v = avp_map_get(calib->combo_index, key);
    avp_arena_reset(s, mk);
    if (v && calib->combo.v[v->i].total >= 5) return (double)calib->combo.v[v->i].hit / (double)calib->combo.v[v->i].total;
    /* the product over the tags (the Python's set order; here sorted by name: D-unit_plan-2) */
    double miss = 1.0;
    AvpStrVec names = up_tag_names(ts);
    for (size_t i = 0; i < names.n; i++) {
        int t = -1;
        for (int k = 0; k < UP_NPOSITIVE; k++)
            if (avp_str_eqz(names.v[i], UP_TAG_NAMES[k])) t = k;
        if (t < 0) continue; /* not a positive tag */
        i64 hit = 0, total = 0;
        if (calib->has_tag[t]) {
            hit = calib->tag[t].hit;
            total = calib->tag[t].total;
        }
        miss *= 1 - (total ? (double)hit / (double)total : 0.3);
    }
    avp_vec_free(&names);
    return 1 - miss;
}

const int UP_PS2_TAG[UP_NPS2] = {UP_T_PS2, UP_T_PS2_N, UP_T_PS2RUN};

double up_ps2_ratio(const UpCalib *calib, int k) {
    const UpCount *c = &calib->ps2_cut[k], *o = &calib->ps2_other[k];
    return ((double)c->hit + 1) / ((double)c->total + 2) / (((double)o->hit + 1) / ((double)o->total + 2));
}

UpCuts up_predict(const UpModel *m, const UpEvidence *ev, const UpCalib *calib, double min_p) {
    (void)m;
    UpCuts out = {0};
    for (size_t g = 0; g < ev->ngaps; g++) {
        UpTags ts = ev->tags[g];
        if (!(ts & (UP_BIT(UP_NPOSITIVE) - 1)) || (ts & UP_BIT(UP_T_INCLASS)) ||
            (ev->strong[g] && !(ts & UP_BIT(UP_T_SINIT))))
            continue;
        double p = up_combine(ts, calib);
        if (p >= min_p) avp_vec_push(&out, ((UpCut){g, ts, p}));
    }
    return out;
}

/* ---- ground truth and check ---- */

static int cmp_range(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const i64 *a = x, *b = y;
    if (a[0] != b[0]) return a[0] < b[0] ? -1 : 1;
    return a[1] < b[1] ? -1 : a[1] > b[1];
}

i8 *up_truth(AvpArena *a, const UpModel *m, const i64 (*ranges)[2], size_t nr) {
    i64(*rs)[2] = AVP_NEWN(a, UpPair, nr ? nr : 1);
    if (nr) memcpy(rs, ranges, nr * sizeof *rs);
    avp_sort(rs, nr, sizeof *rs, cmp_range, NULL, false);
    size_t n = m->n;
    i8 *lab = AVP_NEWN(a, i8, n ? n : 1);
    for (size_t g = 0; g + 1 < n; g++) {
        i64 addr[2] = {m->fns[g].address, m->fns[g + 1].address};
        i64 k[2];
        bool in[2];
        for (int s = 0; s < 2; s++) {
            size_t lo = 0, hi = nr;
            while (lo < hi) {
                size_t mid = (lo + hi) / 2;
                if (addr[s] < rs[mid][0]) hi = mid;
                else lo = mid + 1;
            }
            k[s] = (i64)lo - 1;
            in[s] = k[s] >= 0 && rs[k[s]][0] <= addr[s] && addr[s] < rs[k[s]][1];
        }
        if (in[0] && in[1]) lab[g] = k[0] != k[1];
        else if (in[0] || in[1]) lab[g] = 1;
        else lab[g] = -1;
    }
    return lab;
}

i8 *up_truth_splits(AvpArena *a, const UpModel *m, const ShsmRepo *r, size_t *nunits) {
    const ShsmSplits *sp = shsm_load_splits(r);
    AVP_VEC(i64) rs = {0};
    for (size_t u = 0; u < sp->nunits; u++)
        for (size_t k = 0; k < sp->units[u].nranges; k++)
            if (strcmp(sp->units[u].ranges[k].section, ".text") == 0) {
                avp_vec_push(&rs, (i64)sp->units[u].ranges[k].start);
                avp_vec_push(&rs, (i64)sp->units[u].ranges[k].end);
                break;
            }
    if (nunits) *nunits = sp->nunits;
    i8 *lab = up_truth(a, m, (const i64(*)[2])rs.v, rs.n / 2);
    avp_vec_free(&rs);
    return lab;
}

static const char *gap_lib(const UpModel *m, size_t g) {
    return m->fns[g + 1].unit ? m->fns[g + 1].lib : m->fns[g].lib;
}

UpEvaluation up_evaluate(AvpArena *a, const UpModel *m, const UpEvidence *ev, const i8 *lab) {
    UpEvaluation e = {0};
    UpCalib *c = e.calib = AVP_NEW(a, UpCalib);
    c->combo_index = avp_map_new();
    UpCuts pred = up_predict(m, ev, NULL, 0.0);
    size_t pi = 0;
    for (size_t g = 0; g < ev->ngaps; g++) {
        if (lab[g] < 0) continue;
        bool is_cut = lab[g] == 1;
        while (pi < pred.n && pred.v[pi].g < g) pi++;
        if (pi >= pred.n || pred.v[pi].g != g) continue;
        UpTags ts = pred.v[pi].tags;
        /* the combination without PS2 tags when other positive evidence remains (combine applies their ratios) */
        bool other = (ts & ~UP_PS2_TAGS & (UP_BIT(UP_NPOSITIVE) - 1)) != 0;
        AvpStr key = up_tag_key(a, other ? ts & ~UP_PS2_TAGS : ts);
        bool created;
        AvpVal *v = avp_map_put(c->combo_index, key, &created);
        if (created) {
            v->i = (i64)c->combo.n;
            avp_vec_push(&c->combo_keys, key);
            avp_vec_push_zero(&c->combo);
        }
        c->combo.v[v->i].hit += is_cut;
        c->combo.v[v->i].total += 1;
        if (other)
            for (int k = 0; k < UP_NPS2; k++) {
                UpCount *pc = is_cut ? &c->ps2_cut[k] : &c->ps2_other[k];
                pc->hit += (ts & UP_BIT(UP_PS2_TAG[k])) != 0;
                pc->total += 1;
            }
        /* tags in name order within a gap (the Python iterates a set: D-unit_plan-1) */
        AvpStrVec names = up_tag_names(ts);
        for (size_t i = 0; i < names.n; i++)
            for (int t = 0; t < UP_NPOSITIVE; t++)
                if (avp_str_eqz(names.v[i], UP_TAG_NAMES[t])) {
                    if (!c->has_tag[t]) {
                        c->has_tag[t] = true;
                        c->tag_order[c->ntags++] = (u8)t;
                    }
                    c->tag[t].hit += is_cut;
                    c->tag[t].total += 1;
                }
        avp_vec_free(&names);
    }
    for (int t = 0; t < UP_NPOSITIVE; t++)
        if (!c->has_tag[t]) {
            c->has_tag[t] = true;
            c->tag_order[c->ntags++] = (u8)t;
        }
    for (size_t g = 0; g < ev->ngaps; g++) {
        if (lab[g] != 1) continue;
        e.ncuts++;
        for (int t = 0; t < UP_NPOSITIVE; t++)
            if (ev->tags[g] & UP_BIT(t)) e.recall[t]++;
    }
    avp_vec_free(&pred);
    return e;
}

UpBands up_bands(const UpModel *m, const UpEvidence *ev, const UpCalib *calib, const i8 *lab, double lib_threshold) {
    UpBands b = {0};
    size_t ng = ev->ngaps;
    double *p = AVP_NEWN(avp_scratch(), double, ng ? ng : 1);
    for (size_t g = 0; g < ng; g++) p[g] = -1;
    UpCuts pred = up_predict(m, ev, calib, 0.0);
    for (size_t i = 0; i < pred.n; i++) p[pred.v[i].g] = pred.v[i].p;
    avp_vec_free(&pred);
    static const double TH[3] = {0.9, 0.6, 0.0};
    for (int k = 0; k < 3; k++)
        for (size_t g = 0; g < ng; g++) {
            if (lab[g] < 0) continue;
            if (lab[g] == 1 && p[g] >= TH[k]) b.tp[k]++;
            if (lab[g] == 0 && p[g] >= TH[k]) b.fp[k]++;
            if (lab[g] == 1 && p[g] < TH[k]) b.missed[k]++;
        }
    AvpMap *at = avp_map_new();
    for (size_t g = 0; g < ng; g++) {
        if (lab[g] < 0) continue;
        const char *lib = gap_lib(m, g);
        bool created;
        AvpVal *v = avp_map_put(at, avp_str(lib), &created);
        if (created) {
            v->i = (i64)b.libs.n;
            avp_vec_push(&b.libs, ((UpLibRow){lib, 0, 0, 0, 0}));
        }
        UpLibRow *row = &b.libs.v[v->i];
        row->cuts += lab[g];
        if (p[g] >= lib_threshold) {
            if (lab[g]) row->tp++;
            else row->fp++;
        } else if (lab[g]) row->missed++;
    }
    avp_map_free(at);
    return b;
}

AvpStr up_accuracy_header(AvpArena *a, const UpCalib *calib) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, "boundaries of each kind were right this often on known units: ");
    bool first = true;
    for (int t = 0; t < UP_NPOSITIVE; t++) {
        if (!calib->has_tag[t] || !calib->tag[t].total) continue;
        i64 pct = avp_py_round_int(100.0 * (double)calib->tag[t].hit / (double)calib->tag[t].total);
        avp_buf_printf(&b, "%s%s %lld%% (%lld)", first ? "" : ", ", UP_TAG_NAMES[t], (long long)pct,
                       (long long)calib->tag[t].total);
        first = false;
    }
    return avp_buf_str(&b);
}

AvpJson *up_calib_tag_json(AvpArena *a, const UpCalib *calib) {
    AvpJson *o = avp_json_obj(a);
    for (size_t i = 0; i < calib->ntags; i++) {
        int t = calib->tag_order[i];
        AvpJson *pair = avp_json_arr(a);
        avp_json_arr_push(a, pair, avp_json_int(a, calib->tag[t].hit));
        avp_json_arr_push(a, pair, avp_json_int(a, calib->tag[t].total));
        avp_json_set(a, o, avp_str(UP_TAG_NAMES[t]), pair);
    }
    return o;
}

/* ---- plan ---- */

const char *up_path_for(AvpArena *a, const AvpStrVec *names) {
    ShsmReMatch mm;
    for (size_t i = 0; i < names->n; i++)
        if (shsm_re_fullmatch(shsm_re("C[A-Z]\\w+", 0), names->v[i], 0, AVP_END, &mm))
            return avp_sprintf(a, "game/%s", avp_cz(a, names->v[i]));
    for (size_t i = 0; i < names->n; i++)
        if (shsm_re_search(shsm_re("([\\w]+)\\.(cpp|c)$", 0), names->v[i], 0, AVP_END, &mm))
            return avp_sprintf(a, "<dir>/%s", avp_cz(a, shsm_re_group(&mm, 1)));
    return "<path>";
}

const char *up_conf(bool has, double p) {
    if (!has) return "-";
    return p >= 0.9 ? "high" : p >= 0.6 ? "medium" : "low";
}

double up_min_p(const char *min_conf) {
    if (!min_conf || strcmp(min_conf, "low") == 0) return 0.0;
    if (strcmp(min_conf, "medium") == 0) return 0.6;
    if (strcmp(min_conf, "high") == 0) return 0.9;
    return -1;
}

UpUnits up_plan(AvpArena *a, const UpModel *m, i64 start, i64 end, const UpCalib *calib, const UpEvidence *ev,
                double min_p, const i8 *known) {
    i64 first = -1, last = -1;
    for (size_t i = 0; i < m->n; i++)
        if (start <= m->fns[i].address && m->fns[i].address < end) {
            if (first < 0) first = (i64)i;
            last = (i64)i;
        }
    if (first < 0) shsm_raise(SHSM_EXC_USAGE, "no functions in 0x%08llX-0x%08llX", (long long)start, (long long)end);
    size_t ng = ev->ngaps;
    bool *is_cut = AVP_NEWN(a, bool, ng ? ng : 1);
    UpTags *cut_tags = AVP_NEWN(a, UpTags, ng ? ng : 1);
    double *cut_p = AVP_NEWN(a, double, ng ? ng : 1);
    UpCuts pred = up_predict(m, ev, calib, min_p);
    for (size_t i = 0; i < pred.n; i++) {
        i64 g = (i64)pred.v[i].g;
        if (first <= g && g < last && !(known && known[g] == 0)) {
            is_cut[g] = true;
            cut_tags[g] = pred.v[i].tags;
            cut_p[g] = pred.v[i].p;
        }
    }
    avp_vec_free(&pred);
    if (known)
        for (i64 g = first; g < last; g++)
            if (known[g] == 1) {
                is_cut[g] = true;
                cut_tags[g] = UP_BIT(UP_T_KNOWN) | (ev->tags[g] & ~UP_BIT(UP_T_INCLASS));
                cut_p[g] = 1.0;
            }
    UpUnits res = {0};
    i64 ustart = first;
    for (i64 i = first; i <= last; i++) {
        if (i < last && !is_cut[i]) continue;
        /* the unit [ustart, i] */
        UpUnit u = {0};
        const UpFunc *f0 = &m->fns[ustart], *fl = &m->fns[i];
        u.start = f0->address;
        u.end = up_fn_end(fl);
        u.functions = i - ustart + 1;
        AvpStrVec names = {0};
        bool all_unit = true;
        for (i64 k = ustart; k <= i; k++) {
            const UpFunc *f = &m->fns[k];
            for (size_t j = 0; j < f->strings.n; j++) {
                bool seen = false;
                for (size_t q = 0; !seen && q < names.n; q++) seen = avp_str_eq(names.v[q], f->strings.v[j]);
                if (!seen) avp_vec_push(&names, f->strings.v[j]);
            }
            if (f->unit) {
                bool seen = false;
                for (size_t q = 0; !seen && q < u.known_units.n; q++) seen = avp_str_eqz(u.known_units.v[q], f->unit);
                if (!seen) avp_vec_push(&u.known_units, avp_str(f->unit));
            } else all_unit = false;
            if (f->sinit) u.sinit = true;
        }
        avp_sort_strs(u.known_units.v, u.known_units.n);
        if (i < last) {
            u.end_tags = cut_tags[i];
            u.has_conf = true;
            u.confidence = cut_p[i];
        }
        const char *path = up_path_for(a, &names);
        for (size_t q = 0; q < names.n && q < 6; q++) avp_vec_push(&u.names, names.v[q]);
        avp_vec_free(&names);
        if (u.known_units.n == 1 && all_unit)
            u.command = avp_sprintf(a, "# already split: %s", avp_cz(a, u.known_units.v[0]));
        else
            u.command = avp_sprintf(a, "build/tools/unit.exe new %s --text %08llX-%08llX --dry-run", path,
                                    (long long)u.start, (long long)u.end);
        avp_vec_push(&res, u);
        ustart = i + 1;
    }
    return res;
}

AvpJson *up_unit_json(AvpArena *a, const UpUnit *u) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("start"), avp_json_int(a, u->start));
    avp_json_set(a, o, AVP_S("end"), avp_json_int(a, u->end));
    avp_json_set(a, o, AVP_S("functions"), avp_json_int(a, u->functions));
    AvpJson *tags = avp_json_arr(a);
    AvpStrVec tn = up_tag_names(u->end_tags);
    for (size_t i = 0; i < tn.n; i++) avp_json_arr_push(a, tags, avp_json_str(a, tn.v[i]));
    avp_vec_free(&tn);
    avp_json_set(a, o, AVP_S("end_tags"), tags);
    avp_json_set(a, o, AVP_S("confidence"), u->has_conf ? avp_json_float(a, u->confidence) : avp_json_null());
    AvpJson *names = avp_json_arr(a);
    for (size_t i = 0; i < u->names.n; i++) avp_json_arr_push(a, names, avp_json_str(a, u->names.v[i]));
    avp_json_set(a, o, AVP_S("names"), names);
    AvpJson *known = avp_json_arr(a);
    for (size_t i = 0; i < u->known_units.n; i++) avp_json_arr_push(a, known, avp_json_str(a, u->known_units.v[i]));
    avp_json_set(a, o, AVP_S("known_units"), known);
    avp_json_set(a, o, AVP_S("sinit"), avp_json_bool(u->sinit));
    avp_json_set(a, o, AVP_S("command"), avp_json_strz(a, u->command));
    return o;
}
