/* unit_infer: tools/unit.py lines 909-1227 (text_range_findings, section_gap, gap_hint, claimed_by, weak_closure,
 * weak_run_after, infer_data, find_ctors) and 1808-1838 (unit_xref, _registration_object). */
#include "unit_infer.h"

#include <string.h>

const char *const UNIT_DATA_SECTIONS[UNIT_N_DATA_SECTIONS] = {".rodata", ".data", ".bss", ".sdata",
                                                              ".sbss",   ".sdata2", ".sbss2"};
const char *const UNIT_SPLIT_SECTIONS[10] = {".text", ".ctors", ".dtors", ".rodata", ".data",
                                             ".bss",  ".sdata", ".sbss",  ".sdata2", ".sbss2"};

static bool is_data_sec(const char *s) {
    for (int i = 0; i < UNIT_N_DATA_SECTIONS; i++)
        if (!strcmp(UNIT_DATA_SECTIONS[i], s)) return true;
    return false;
}
static bool is_bss_sec(const char *s) { return !strcmp(s, ".bss") || !strcmp(s, ".sbss") || !strcmp(s, ".sbss2"); }

static void add(AvpArena *a, UnitFindings *f, const char *level, const char *msg) {
    (void)a;
    UnitFinding x = {level, msg};
    avp_vec_push(f, x);
}

UnitFindings unit_text_range_findings(UnitProject *p, const char *path, i64 lo, i64 hi, const UnitSplits *splits,
                                      const UnitConfig *cfg, const char *exclude_path) {
    AvpArena *a = p->a;
    UnitFindings res = {0};
    i64 align = unit_func_align_for(path);
    UnitSymPtrVec funcs = unit_text_funcs(p, lo, hi, ".text");
    if (!funcs.n) {
        add(a, &res, "ERROR", avp_sprintf(a, ".text %08llX-%08llX contains no function in symbols.txt", lo, hi));
        return res;
    }
    UnitSymPtrVec every = unit_text_funcs(p, 0, 0xFFFFFFFF, ".text");
    for (size_t i = 0; i < every.n; i++) {
        const UnitSym *s = every.v[i];
        i64 end = s->addr + s->size;
        if (s->addr < lo && lo < end)
            add(a, &res, "ERROR",
                avp_sprintf(a, "%s (0x%08llX-0x%08llX) straddles the start 0x%08llX", s->name, s->addr, end, lo));
        if (s->addr < hi && hi < end)
            add(a, &res, "ERROR",
                avp_sprintf(a, "%s (0x%08llX-0x%08llX) straddles the end 0x%08llX", s->name, s->addr, end, hi));
    }
    const UnitSym *first = funcs.v[0], *last = funcs.v[funcs.n - 1];
    i64 last_end = last->addr + last->size;
    if (first->addr != lo)
        add(a, &res, "WARN",
            avp_sprintf(a, "range starts 0x%llX bytes before the first function %s (0x%08llX)", first->addr - lo,
                        first->name, first->addr));
    if (lo % align) add(a, &res, "INFO", avp_sprintf(a, "start 0x%08llX is not %lld-byte aligned", lo, align));
    i64 gaps = 0;
    for (size_t i = 0; i + 1 < funcs.n; i++) {
        i64 g = funcs.v[i + 1]->addr - (funcs.v[i]->addr + funcs.v[i]->size);
        gaps += g >= align ? g : 0;
    }
    if (gaps)
        add(a, &res, "WARN",
            avp_sprintf(a, "%lld bytes inside the range are covered by no function symbol (padding or a missing symbol)",
                        gaps));
    i64 tail = hi - last_end;
    const UnitSplit *nxt = NULL;
    for (size_t i = 0; i < splits->n && !nxt; i++) {
        const UnitRange *t = unit_ranges_get(&splits->v[i].ranges, ".text");
        if ((t ? t->lo : 0) == hi && (!exclude_path || strcmp(splits->v[i].path, exclude_path))) nxt = &splits->v[i];
    }
    bool nxt_is_source = nxt && unit_config_find(cfg, nxt->path);
    if (tail < 0) {
        add(a, &res, "ERROR",
            avp_sprintf(a, "the last function %s ends at 0x%08llX, after the range end", last->name, last_end));
    } else if (tail > 0) {
        const char *why = nxt_is_source ? avp_sprintf(a, "next split %s is a source unit", nxt->path)
                                        : "the next unit is a dtk asm unit";
        if (nxt_is_source && tail < align)
            add(a, &res, "INFO",
                avp_sprintf(a,
                            "range ends %lld bytes after the last instruction (0x%08llX); %s, alignment padding is "
                            "accepted",
                            tail, last_end, why));
        else
            add(a, &res, "ERROR",
                avp_sprintf(a,
                            "range ends %lld bytes after the last instruction (0x%08llX) but %s: a source unit followed "
                            "by an asm unit must end its .text at its last instruction (use END=0x%08llX)",
                            tail, last_end, why, last_end));
    } else {
        add(a, &res, "OK", avp_sprintf(a, ".text ends at the last instruction (0x%08llX)", last_end));
    }
    return res;
}

/* ---- data inference ---- */

const char *unit_claimed_by(const UnitSplits *splits, const UnitBlock *t) {
    for (size_t i = 0; i < splits->n; i++) {
        const UnitRange *r = unit_ranges_get(&splits->v[i].ranges, t->sec);
        if (r && r->lo <= t->addr && t->addr < r->hi) return splits->v[i].path;
    }
    return NULL;
}

AVP_NORETURN static void type_error(void) {
    avp_throw(UNIT_UNCAUGHT, "TypeError: tools.unit.section_gap() argument after * must be an iterable, not NoneType");
}

static const char *gap_hint(AvpArena *a, const UnitSplits *splits, const char *sec, bool has_text, i64 lo, i64 hi,
                            const UnitBlock *t) {
    if (!has_text) type_error();
    const UnitSplit *prev = NULL, *nxt = NULL;
    for (size_t i = 0; i < splits->n; i++) {
        const UnitSplit *s = &splits->v[i];
        const UnitRange *tr = unit_ranges_get(&s->ranges, ".text"), *r = unit_ranges_get(&s->ranges, sec);
        if (!tr || !r) continue;
        if (tr->hi <= lo && (!prev || tr->lo > unit_ranges_get(&prev->ranges, ".text")->lo)) prev = s;
        else if (tr->lo >= hi && (!nxt || tr->lo < unit_ranges_get(&nxt->ranges, ".text")->lo)) nxt = s;
    }
    if (!prev && !nxt) return "";
    i64 ga = prev ? unit_ranges_get(&prev->ranges, sec)->hi : 0;
    i64 gb = nxt ? unit_ranges_get(&nxt->ranges, sec)->lo : 0xFFFFFFFF;
    const char *where = prev && nxt ? avp_sprintf(a, "%s and %s", prev->path, nxt->path) : prev ? prev->path : nxt->path;
    if (ga <= t->addr && t->addr + t->size <= gb)
        return avp_sprintf(a, "; lies in the %s gap between the neighbouring units (%s)", sec, where);
    return avp_sprintf(a, "; outside the %s gap between the neighbouring units (%s)", sec, where);
}

static bool in_map(AvpMap *m, const char *k) { return avp_map_has(m, avp_str(k)); }
static bool in_vec(const AvpStrVec *v, const char *k) {
    for (size_t i = 0; i < v->n; i++)
        if (avp_str_eqz(v->v[i], k)) return true;
    return false;
}
static void map_set(AvpMap *m, const char *k, void *v) { avp_map_put(m, avp_str(k), NULL)->p = v; }

typedef struct {
    UnitIndex *x;
    AvpMap *inside, *included;
} RefCtx;

static bool referrers_ok(RefCtx *c, const UnitBlock *t) {
    const UnitBlockVec *r = unit_index_referrers(c->x, t->name);
    for (size_t i = 0; i < r->n; i++) {
        const char *n = r->v[i]->name;
        if (!(in_map(c->inside, n) || in_map(c->included, n) || !strcmp(n, t->name))) return false;
    }
    return true;
}

static AvpMap *weak_closure(UnitIndex *x, const UnitSplits *splits, AvpMap *included, const AvpStrVec *excludes) {
    AvpMap *cand = avp_map_new();
    bool changed = true;
    while (changed) {
        changed = false;
        UnitBlockVec snap = {0};
        AvpMapIter it = {0};
        while (avp_map_next(included, &it)) avp_vec_push(&snap, it.val->p);
        it = (AvpMapIter){0};
        while (avp_map_next(cand, &it)) avp_vec_push(&snap, it.val->p);
        for (size_t i = 0; i < snap.n; i++) {
            UnitBlock *b = snap.v[i];
            for (size_t k = 0; k < b->nrefs; k++) {
                UnitBlock *u = unit_index_block(x, b->refs[k]);
                if (!u || strcmp(u->kind, "obj") || !is_data_sec(u->sec) || in_map(included, u->name) ||
                    in_map(cand, u->name) || in_vec(excludes, u->name) || unit_claimed_by(splits, u))
                    continue;
                bool only_ours = in_map(cand, b->name);
                if (only_ours) {
                    const UnitBlockVec *r = unit_index_referrers(x, u->name);
                    for (size_t j = 0; j < r->n && only_ours; j++) {
                        const char *n = r->v[j]->name;
                        only_ours = in_map(included, n) || in_map(cand, n) || !strcmp(n, u->name);
                    }
                }
                if (!strcmp(u->scope, "weak") || only_ours) {
                    map_set(cand, u->name, u);
                    changed = true;
                }
            }
        }
        avp_vec_free(&snap);
    }
    return cand;
}

static UnitBlockVec weak_run_after(AvpArena *a, UnitIndex *x, const UnitSplits *splits, const char *sec, i64 hi,
                                   AvpMap *weak) {
    UnitBlockVec run = {0};
    i64 pos = hi;
    for (;;) {
        UnitBlockVec r = unit_index_in_range(a, x, sec, pos, pos + UNIT_WEAK_RUN_SLOP);
        UnitBlock *nb = r.n ? r.v[0] : NULL;
        avp_vec_free(&r);
        if (!nb || unit_claimed_by(splits, nb)) break;
        if (in_map(weak, nb->name)) avp_vec_push(&run, nb);
        else if (!(unit_is_gap_name(nb->name) && nb->zero && !unit_index_referrers(x, nb->name)->n)) break;
        pos = nb->addr + (nb->size > 1 ? nb->size : 1);
    }
    return run;
}

static const char *describe(AvpArena *a, const UnitBlock *t) {
    return avp_sprintf(a, "%s (%s 0x%08llX, size 0x%llX)", t->name, t->sec, t->addr, t->size);
}

static int cmp_block_addr(const void *x, const void *y, void *ctx) {
    (void)ctx;
    i64 l = (*(UnitBlock *const *)x)->addr, r = (*(UnitBlock *const *)y)->addr;
    return l < r ? -1 : l > r;
}

/* Why a data object the unit's code reads is taken without --include-data, or NULL when it is not. */
static const char *auto_take_reason(AvpArena *a, RefCtx *c, const UnitBlock *t, i64 lo, i64 hi) {
    if (strcmp(t->kind, "obj")) return NULL;
    const UnitBlockVec *r = unit_index_referrers(c->x, t->name);
    size_t inside = 0;
    const char *outside = NULL;
    for (size_t i = 0; i < r->n; i++) {
        const char *n = r->v[i]->name;
        if (in_map(c->inside, n)) inside++;
        else if (!outside && strcmp(n, t->name)) outside = n;
    }
    if (!inside) return NULL;
    if (!strncmp(t->name, "__vt__", 6))
        return outside ? avp_sprintf(a, "vtable read by this unit's code, also referenced by %s outside (an inlined "
                                        "constructor): taken by default",
                                     outside)
                       : "vtable read only by this unit's code: taken by default";
    if (!strncmp(t->name, "jumptable_", 10) && !outside) return "jump table read only by this unit's code: taken by default";
    if (t->words_only && t->nrefs) {
        for (size_t k = 0; k < t->nrefs; k++) {
            const UnitBlock *u = unit_index_block(c->x, t->refs[k]);
            if (!u || strcmp(u->kind, "fn") || u->addr < lo || u->addr >= hi) return NULL;
        }
        return outside ? avp_sprintf(a, "table of this unit's function addresses (a vtable), also referenced by %s "
                                        "outside: taken by default",
                                     outside)
                       : "table of this unit's function addresses (a vtable): taken by default";
    }
    return NULL;
}

/* The objects just outside the unit's range of sec: each one is reported as not taken, with the reason and the exact
 * --include-data that would take it (padding between objects is not reported). */
static void neighbour_notes(AvpArena *a, UnitIndex *x, RefCtx *c, const UnitSplits *splits, const char *sec, i64 rlo,
                            i64 rhi, AvpStrVec *notes) {
    AvpVal *sv = avp_map_get(x->by_sec, avp_str(sec));
    if (!sv) return;
    const UnitBlockVec *all = sv->p;
    size_t first_after = 0;
    while (first_after < all->n && all->v[first_after]->addr < rhi) first_after++;
    size_t before_end = 0; /* one past the last block that ends at or before rlo */
    while (before_end < all->n && all->v[before_end]->addr + all->v[before_end]->size <= rlo) before_end++;
    enum { PER_SIDE = 2 };
    UnitBlockVec nb = {0};
    for (size_t i = before_end >= PER_SIDE ? before_end - PER_SIDE : 0; i < before_end; i++) avp_vec_push(&nb, all->v[i]);
    for (size_t i = first_after; i < all->n && i < first_after + PER_SIDE; i++) avp_vec_push(&nb, all->v[i]);
    for (size_t i = 0; i < nb.n; i++) {
        const UnitBlock *t = nb.v[i];
        const UnitBlockVec *r = unit_index_referrers(x, t->name);
        if (unit_is_gap_name(t->name) && t->zero && !r->n) continue;
        const char *side = t->addr >= rhi ? "after" : "before";
        const char *owner = unit_claimed_by(splits, t);
        const char *why;
        if (owner) {
            why = avp_sprintf(a, "belongs to split %s", owner);
        } else {
            const char *out = NULL;
            for (size_t k = 0; k < r->n && !out; k++)
                if (!in_map(c->inside, r->v[k]->name) && !in_map(c->included, r->v[k]->name)) out = r->v[k]->name;
            if (out) why = avp_sprintf(a, "read by %s, not by this unit's code: not taken (--include-data %s takes it)", out, t->name);
            else if (!r->n) why = avp_sprintf(a, "referenced by no block: not taken (--include-data %s takes it)", t->name);
            else why = avp_sprintf(a, "read only by this unit's own objects but outside its range: not taken (--include-data %s takes it)", t->name);
        }
        avp_vec_push(notes, avp_str(avp_sprintf(a, "  0x%08llX size 0x%llX %s: neighbour %s the range, %s", t->addr, t->size,
                                                t->name, side, why)));
    }
    avp_vec_free(&nb);
}

/* The first referrer of t not in the sets, or "?". */
static const char *first_outside(RefCtx *c, const char *name, bool also_included) {
    const UnitBlockVec *r = unit_index_referrers(c->x, name);
    for (size_t i = 0; i < r->n; i++) {
        const char *n = r->v[i]->name;
        if (!in_map(c->inside, n) && !(also_included && in_map(c->included, n))) return n;
    }
    return "?";
}

UnitDataResult *unit_infer_data(UnitProject *p, UnitIndex *x, bool has_text, i64 lo, i64 hi, const UnitSplits *splits,
                                const AvpStrVec *includes, const AvpStrVec *excludes) {
    AvpArena *a = p->a;
    UnitDataResult *res = AVP_NEW(a, UnitDataResult);
    UnitBlockVec fn_blocks = {0};
    if (!has_text) {
        lo = hi = 0;
        if (!includes->n) unit_fail("a unit without --text is data-only and needs --include-data SYMBOL...");
    } else {
        UnitBlockVec r = unit_index_in_range(a, x, ".text", lo, hi);
        for (size_t i = 0; i < r.n; i++)
            if (!strcmp(r.v[i]->kind, "fn")) avp_vec_push(&fn_blocks, r.v[i]);
        avp_vec_free(&r);
        if (!fn_blocks.n) {
            avp_vec_push(&res->warnings, AVP_S("no asm blocks for this .text range: run the build so dtk writes "
                                               "build/R5WEA4/asm; no data inferred"));
            res->no_asm = true;
            return res;
        }
    }
    RefCtx c = {x, avp_map_new(), avp_map_new()};
    for (size_t i = 0; i < fn_blocks.n; i++) map_set(c.inside, fn_blocks.v[i]->name, fn_blocks.v[i]);
    UnitSymPtrVec tf = unit_text_funcs(p, lo, hi, ".text");
    UnitSymPtrVec sinit = unit_sinit_funcs(p, &tf);
    AvpMap *defined = avp_map_new(); /* name -> the static initialiser's name */
    for (size_t i = 0; i < fn_blocks.n; i++) {
        UnitBlock *fb = fn_blocks.v[i];
        bool is_sinit = false;
        for (size_t k = 0; k < sinit.n; k++)
            if (!strcmp(sinit.v[k]->name, fb->name)) is_sinit = true;
        if (!is_sinit) continue;
        AvpCzVec names = {0};
        for (size_t k = 0; k < fb->nstores; k++) avp_vec_push(&names, (char *)fb->stores[k]);
        for (size_t k = 0; k < fb->nctor; k++)
            if (fb->ctor_args[k].arg) avp_vec_push(&names, (char *)fb->ctor_args[k].arg);
        for (size_t k = 0; k < names.n; k++) {
            UnitBlock *t = unit_index_block(x, names.v[k]);
            if (t && is_bss_sec(t->sec) && !in_vec(excludes, names.v[k]) && !in_map(defined, names.v[k]))
                map_set(defined, names.v[k], (void *)fb->name);
        }
        avp_vec_free(&names);
    }
    AvpMap *pool = avp_map_new();
    for (size_t i = 0; i < fn_blocks.n; i++)
        for (size_t k = 0; k < fn_blocks.v[i]->nrefs; k++) {
            UnitBlock *t = unit_index_block(x, fn_blocks.v[i]->refs[k]);
            if (t && strcmp(t->sec, ".text") && strcmp(t->sec, ".init") && is_data_sec(t->sec) && !in_map(pool, t->name))
                map_set(pool, t->name, t);
        }
    AvpStrVec keys = avp_map_keys(pool);
    for (size_t i = 0; i < keys.n; i++) {
        const char *n = avp_cz(a, keys.v[i]);
        UnitBlock *t = avp_map_get(pool, keys.v[i])->p;
        const char *owner = unit_claimed_by(splits, t);
        if (owner && !in_vec(excludes, n)) {
            avp_map_del(pool, avp_str(n));
            avp_vec_push(&res->left, avp_str(avp_sprintf(a, "%s: already in split %s", describe(a, t), owner)));
        }
    }
    avp_vec_free(&keys);
    for (size_t i = 0; i < includes->n; i++) {
        const char *n = avp_cz(a, includes->v[i]);
        UnitBlock *t = unit_index_block(x, n);
        if (!t) unit_fail("--include-data %s: no such symbol in the asm", n);
        const char *owner = unit_claimed_by(splits, t);
        if (owner) unit_fail("--include-data %s: %s 0x%08llX already belongs to split %s", n, t->sec, t->addr, owner);
        map_set(pool, n, t);
    }
    for (size_t i = 0; i < excludes->n; i++) {
        const char *n = avp_cz(a, excludes->v[i]);
        AvpVal *v = avp_map_get(pool, avp_str(n));
        UnitBlock *t = v ? v->p : NULL;
        if (v) avp_map_del(pool, avp_str(n));
        if (!t) t = unit_index_block(x, n);
        if (!t) unit_fail("--exclude-data %s: no such symbol in the asm", n);
        avp_vec_push(&res->left, avp_str(avp_sprintf(a,
                                                     "%s: excluded by --exclude-data (give it to its unit with `unit.exe "
                                                     "new <path> --include-data %s`)",
                                                     describe(a, t), n)));
    }
    /* default takes: what the unit's own code reads and nothing but this unit can own (a vtable, a table of the
     * unit's function addresses, a jump table) is taken like an --include-data object */
    AvpStrVec eff = {0};
    avp_vec_extend(&eff, includes->v, includes->n);
    AvpMap *auto_why = avp_map_new();
    if (has_text) {
        AvpStrVec pk = avp_map_keys(pool);
        for (size_t i = 0; i < pk.n; i++) {
            UnitBlock *t = avp_map_get(pool, pk.v[i])->p;
            if (in_vec(includes, t->name) || in_vec(excludes, t->name)) continue;
            const char *why = auto_take_reason(a, &c, t, lo, hi);
            if (!why) continue;
            map_set(auto_why, t->name, (void *)why);
            avp_vec_push(&eff, avp_str(t->name));
        }
        avp_vec_free(&pk);
    }
    bool changed = true;
    while (changed) {
        changed = false;
        UnitBlockVec snap = {0};
        AvpMapIter it = {0};
        while (avp_map_next(pool, &it)) avp_vec_push(&snap, it.val->p);
        for (size_t i = 0; i < snap.n; i++) {
            UnitBlock *t = snap.v[i];
            if (in_map(c.included, t->name)) continue;
            if (in_vec(&eff, t->name) || in_map(defined, t->name) || referrers_ok(&c, t)) {
                map_set(c.included, t->name, t);
                changed = true;
                for (size_t k = 0; k < t->nrefs; k++) {
                    UnitBlock *u = unit_index_block(x, t->refs[k]);
                    if (u && is_data_sec(u->sec) && !in_vec(excludes, u->name) && !in_map(pool, u->name)) {
                        const char *owner = unit_claimed_by(splits, u);
                        if (owner)
                            avp_vec_push(&res->left,
                                         avp_str(avp_sprintf(a, "%s: already in split %s", describe(a, u), owner)));
                        else
                            map_set(pool, u->name, u);
                    }
                }
            }
        }
        avp_vec_free(&snap);
    }
    AvpMap *shared = avp_map_new();
    {
        AvpMapIter it = {0};
        while (avp_map_next(pool, &it))
            if (!avp_map_has(c.included, it.key)) avp_map_put(shared, it.key, NULL)->p = it.val->p;
    }
    AvpMap *weak = weak_closure(x, splits, c.included, excludes);
    UnitBlockVec shared_sorted = {0};
    {
        AvpMapIter it = {0};
        while (avp_map_next(shared, &it)) avp_vec_push(&shared_sorted, it.val->p);
        avp_sort(shared_sorted.v, shared_sorted.n, sizeof(UnitBlock *), cmp_block_addr, NULL, false);
    }
    for (int si = 0; si < UNIT_N_DATA_SECTIONS; si++) {
        const char *sec = UNIT_DATA_SECTIONS[si];
        UnitBlockVec anchors = {0};
        AvpMapIter it = {0};
        while (avp_map_next(c.included, &it))
            if (!strcmp(((UnitBlock *)it.val->p)->sec, sec)) avp_vec_push(&anchors, it.val->p);
        UnitBlockVec run = {0};
        if (anchors.n) {
            i64 top = 0;
            for (size_t i = 0; i < anchors.n; i++)
                if (i == 0 || anchors.v[i]->addr + anchors.v[i]->size > top) top = anchors.v[i]->addr + anchors.v[i]->size;
            run = weak_run_after(a, x, splits, sec, top, weak);
        }
        AvpMap *run_names = avp_map_new();
        for (size_t i = 0; i < run.n; i++) {
            map_set(run_names, run.v[i]->name, run.v[i]);
            avp_vec_push(&anchors, run.v[i]);
        }
        if (!anchors.n) {
            AvpStrVec notes = {0};
            for (size_t i = 0; i < shared_sorted.n; i++) {
                UnitBlock *t = shared_sorted.v[i];
                if (strcmp(t->sec, sec)) continue;
                avp_vec_push(&notes,
                             avp_str(avp_sprintf(a,
                                                 "  0x%08llX size 0x%llX %s: shared with %s; no private object of %s "
                                                 "anchors a range: NOT taken (--include-data %s if this unit defines "
                                                 "it)%s",
                                                 t->addr, t->size, t->name, first_outside(&c, t->name, false), sec,
                                                 t->name, gap_hint(a, splits, sec, has_text, lo, hi, t))));
            }
            if (notes.n) {
                res->notes[si] = notes;
                res->has_notes[si] = true;
            }
            continue;
        }
        i64 lo_a = 0, hi_a = 0;
        for (size_t i = 0; i < anchors.n; i++) {
            UnitBlock *t = anchors.v[i];
            if (i == 0 || t->addr < lo_a) lo_a = t->addr;
            if (i == 0 || t->addr + t->size > hi_a) hi_a = t->addr + t->size;
        }
        AvpStrVec notes = {0};
        UnitBlockVec sorted = {0};
        avp_vec_extend(&sorted, anchors.v, anchors.n);
        avp_sort(sorted.v, sorted.n, sizeof(UnitBlock *), cmp_block_addr, NULL, false);
        for (size_t i = 0; i < sorted.n; i++) {
            UnitBlock *t = sorted.v[i];
            const char *why;
            if (in_map(run_names, t->name)) {
                why = avp_sprintf(a, "%s reached from this unit's data, contiguous after it: taken (first emitted here "
                                     "in link order)",
                                  !strcmp(t->scope, "weak") ? "weak object" : "referenced only by weak objects");
            } else if (in_map(auto_why, t->name)) {
                why = (const char *)avp_map_get(auto_why, avp_str(t->name))->p;
            } else if (in_vec(includes, t->name) && !referrers_ok(&c, t)) {
                why = "forced by --include-data";
            } else if (in_map(defined, t->name) && !referrers_ok(&c, t)) {
                why = avp_sprintf(a, "defined by %s (stored to or constructed there), used elsewhere too",
                                  (const char *)avp_map_get(defined, avp_str(t->name))->p);
            } else {
                const UnitBlockVec *r = unit_index_referrers(x, t->name);
                if (!r->n) why = "referenced only by no block";
                else if (r->n == 1) why = avp_sprintf(a, "referenced only by %s", r->v[0]->name);
                else why = avp_sprintf(a, "referenced only by %s, %s", r->v[0]->name, r->v[1]->name);
            }
            avp_vec_push(&notes, avp_str(avp_sprintf(a, "  0x%08llX size 0x%llX %s: %s", t->addr, t->size, t->name, why)));
        }
        UnitBlockVec take = {0};
        avp_vec_extend(&take, anchors.v, anchors.n);
        for (size_t i = 0; i < shared_sorted.n; i++) {
            UnitBlock *t = shared_sorted.v[i];
            if (strcmp(t->sec, sec) || in_map(run_names, t->name)) continue;
            const char *out = first_outside(&c, t->name, true);
            if (lo_a <= t->addr && t->addr + t->size <= hi_a) {
                avp_vec_push(&take, t);
                avp_vec_push(&notes, avp_str(avp_sprintf(a,
                                                         "  0x%08llX size 0x%llX %s: shared with %s but inside the hull "
                                                         "of the private objects: taken",
                                                         t->addr, t->size, t->name, out)));
            } else {
                i64 dist = t->addr >= hi_a ? t->addr - hi_a : lo_a - (t->addr + t->size);
                const char *tag = dist <= 0x40 ? "ADJACENT, ambiguous" : "defined elsewhere";
                avp_vec_push(&notes, avp_str(avp_sprintf(a,
                                                         "  0x%08llX size 0x%llX %s: shared with %s, outside the hull "
                                                         "(%s): NOT taken (--include-data %s takes it)%s",
                                                         t->addr, t->size, t->name, out, tag, t->name,
                                                         gap_hint(a, splits, sec, has_text, lo, hi, t))));
            }
        }
        i64 rlo = 0, rhi = 0;
        AvpMap *taken = avp_map_new();
        for (size_t i = 0; i < take.n; i++) {
            UnitBlock *t = take.v[i];
            if (i == 0 || t->addr < rlo) rlo = t->addr;
            if (i == 0 || t->addr + t->size > rhi) rhi = t->addr + t->size;
            map_set(taken, t->name, t);
        }
        UnitBlockVec in = unit_index_in_range(a, x, sec, rlo, rhi);
        for (size_t i = 0; i < in.n; i++) {
            UnitBlock *b = in.v[i];
            if (in_map(taken, b->name)) continue;
            const char *ext = first_outside(&c, b->name, true);
            if (strcmp(ext, "?")) {
                avp_vec_push(&res->warnings,
                             avp_str(avp_sprintf(a,
                                                 "%s 0x%08llX %s lies inside the range but is referenced by %s: it may "
                                                 "belong to another unit",
                                                 sec, b->addr, b->name, ext)));
            }
        }
        avp_vec_free(&in);
        for (;;) {
            UnitBlockVec r = unit_index_in_range(a, x, sec, rhi, rhi + 1);
            UnitBlock *nb = NULL;
            for (size_t i = 0; i < r.n && !nb; i++)
                if (r.v[i]->addr == rhi) nb = r.v[i];
            avp_vec_free(&r);
            if (nb && unit_is_gap_name(nb->name) && nb->zero && nb->size <= UNIT_MAX_PAD_EXTEND &&
                !unit_index_referrers(x, nb->name)->n) {
                avp_vec_push(&notes, avp_str(avp_sprintf(a,
                                                         "  0x%08llX size 0x%llX %s: unreferenced zero padding after "
                                                         "the last object: taken",
                                                         nb->addr, nb->size, nb->name)));
                rhi = nb->addr + nb->size;
            } else {
                break;
            }
        }
        unit_ranges_set(&res->ranges, sec, rlo, rhi);
        neighbour_notes(a, x, &c, splits, sec, rlo, rhi, &notes);
        res->notes[si] = notes;
        res->has_notes[si] = true;
        UnitBlockVec own = unit_index_in_range(a, x, sec, rlo, rhi);
        avp_vec_extend(&res->owned, own.v, own.n);
        avp_vec_free(&own);
    }
    AvpMap *owned_names = avp_map_new();
    for (size_t i = 0; i < res->owned.n; i++) map_set(owned_names, res->owned.v[i]->name, NULL);
    AvpMapIter it = {0};
    while (avp_map_next(shared, &it))
        if (!avp_map_has(owned_names, it.key)) avp_vec_push(&res->shared, avp_str_dup(a, it.key));
    avp_sort_strs(res->shared.v, res->shared.n);
    return res;
}

static int cmp_i64(const void *x, const void *y, void *ctx) {
    (void)ctx;
    i64 l = *(const i64 *)x, r = *(const i64 *)y;
    return l < r ? -1 : l > r;
}

bool unit_find_ctors(UnitProject *p, const UnitSymPtrVec *funcs, i64 *lo, i64 *hi, AvpStrVec *notes) {
    AvpArena *a = p->a;
    UnitSymPtrVec sinit = unit_sinit_funcs(p, funcs);
    notes->n = 0;
    if (!sinit.n) {
        avp_vec_push(notes, AVP_S("no static initialiser (function listed in the DOL's .ctors table) in the range: no "
                                  ".ctors entry"));
        return false;
    }
    AvpI64Vec ents = {0};
    for (size_t i = 0; i < sinit.n; i++) {
        const UnitSym *f = sinit.v[i];
        i64 hit = 0;
        size_t n = unit_ctor_hits(p, f->addr, &hit, 1);
        if (n != 1) {
            notes->n = 0;
            avp_vec_push(notes, avp_str(avp_sprintf(a, "%s (0x%08llX) appears %zu times in the .ctors table", f->name,
                                                    f->addr, n)));
            return false;
        }
        avp_vec_push(&ents, hit);
        avp_vec_push(notes, avp_str(avp_sprintf(a, "  0x%08llX: entry for %s", hit, f->name)));
    }
    avp_sort(ents.v, ents.n, sizeof(i64), cmp_i64, NULL, false);
    if (ents.v[ents.n - 1] - ents.v[0] + 4 != 4 * (i64)ents.n) {
        notes->n = 0;
        avp_vec_push(notes, AVP_S("the unit's .ctors entries are not contiguous"));
        return false;
    }
    *lo = ents.v[0];
    *hi = ents.v[ents.n - 1] + 4;
    return true;
}

void unit_xref(AvpArena *a, const UnitIndex *x, const UnitRanges *ranges, AvpSet **inside, AvpMap **ext_hits) {
    AvpSet *in = avp_set_new();
    UnitBlockVec unit_blocks = {0};
    for (size_t i = 0; i < ranges->n; i++) {
        UnitBlockVec r = unit_index_in_range(a, x, ranges->v[i].sec, ranges->v[i].lo, ranges->v[i].hi);
        for (size_t k = 0; k < r.n; k++) {
            avp_vec_push(&unit_blocks, r.v[k]);
            avp_set_add(in, avp_str(r.v[k]->name));
        }
        avp_vec_free(&r);
    }
    AvpMap *ext = avp_map_new();
    for (size_t i = 0; i < unit_blocks.n; i++) {
        UnitBlock *b = unit_blocks.v[i];
        if (!strcmp(b->scope, "local")) continue;
        const UnitBlockVec *r = unit_index_referrers(x, b->name);
        for (size_t k = 0; k < r->n; k++) {
            if (avp_set_has(in, avp_str(r->v[k]->name))) continue;
            bool created;
            AvpVal *v = avp_map_put(ext, avp_str(b->name), &created);
            if (created) v->p = AVP_NEW(a, AvpStrVec);
            avp_vec_push((AvpStrVec *)v->p, avp_str(r->v[k]->name));
        }
    }
    avp_vec_free(&unit_blocks);
    *inside = in;
    *ext_hits = ext;
}

const char *unit_registration_object(const UnitIndex *x, const UnitSymPtrVec *sinit_funcs) {
    for (size_t i = 0; i < sinit_funcs->n; i++) {
        UnitBlock *b = unit_index_block(x, sinit_funcs->v[i]->name);
        if (!b) continue;
        for (size_t k = 0; k < b->nctor; k++)
            if (strstr(b->ctor_args[k].callee, "unk_801A8F6C") && b->ctor_args[k].arg) return b->ctor_args[k].arg;
    }
    return NULL;
}
