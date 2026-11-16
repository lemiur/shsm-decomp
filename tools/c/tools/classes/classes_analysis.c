/* classes_analysis.c: part 1 of `class Analysis` (tools/classes.py lines 926-1427, plus vt_at and vt_addr): the
 * vtable collection of __init__, vptr store sites, groups, base candidates, base images and slot classification.
 * Where the Python would raise (a list index out of range, a None compared with a string while sorting spans) the
 * port raises the same exception class through shsm_raise; the real tree reaches neither. */
#include "classes_analysis.h"

#include "shsm_cli.h"

const char *const classes_kind_names[5] = {"ctor", "dtor", "factory", "alloc", "other"};

static bool eq(AvpStr x, AvpStr y) { return avp_str_eq(x, y); }
static bool startz(AvpStr s, const char *z) {
    size_t n = strlen(z);
    return s.n >= n && memcmp(s.p, z, n) == 0;
}
static AvpStr cat(AvpArena *a, const char *pre, AvpStr s) { return avp_str_concat(a, avp_str(pre), s); }
static bool truthy(AvpStr s) { return s.p != NULL && s.n > 0; }

/* words[i] with Python list indexing (a negative index counts from the end; out of range raises IndexError) */
static AvpStr word_py(const ClassesWords *w, i64 i) {
    i64 n = (i64)w->n;
    if (i < 0) i += n;
    if (i < 0 || i >= n) shsm_raise(SHSM_EXC_VALUE, "list index out of range");
    return w->v[i];
}

/* Python "%X" of an int: a negative value prints as "-" and the magnitude */
static void put_pyhex(AvpBuf *b, i64 v) {
    if (v < 0) avp_buf_printf(b, "-%llX", (unsigned long long)(0 - (u64)v));
    else avp_buf_printf(b, "%llX", (unsigned long long)v);
}

const ClassesGroupEnt *classes_group_get(const ClassesGroup *g, i64 a) {
    for (size_t i = 0; i < g->n; i++)
        if (g->v[i].a == a) return &g->v[i];
    return NULL;
}

bool classes_an_complete_off(const ClassesAnalysis *an, const ClassesVtable *vt, i64 *off) {
    AVP_UNUSED(an);
    const ClassesGroupEnt *e = classes_group_get(&vt->group, 0);
    if (!e || !e->has_o) return false;
    *off = e->o;
    return true;
}

static ClassesVtable *vt_rec(const ClassesAnalysis *an, AvpStr name) {
    ClassesVtable *r = classes_an_vt(an, name);
    if (!r) shsm_raise(SHSM_EXC_KEY, "%.*s", AVP_SF(name));
    return r;
}

/* ---- vt_addr / vt_at ------------------------------------------------------------------------------------------ */

static bool is_uhex(char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'); }

bool classes_an_vt_addr(const ClassesAnalysis *an, AvpStr vt, i64 *addr) {
    /* ^(.*)\+0x([0-9A-F]+)$: only the last "+0x" can be followed by hex digits alone */
    if (!avp_map_has(an->prog->objs, vt)) {
        i64 pos = avp_str_rfind(vt, AVP_S("+0x"), 0, AVP_END);
        if (pos >= 0 && (size_t)pos + 3 < vt.n) {
            bool ok = true;
            u64 x = 0;
            for (size_t i = (size_t)pos + 3; i < vt.n && ok; i++) {
                char c = vt.p[i];
                if (!is_uhex(c)) ok = false;
                else x = x * 16 + (u64)(c <= '9' ? c - '0' : c - 'A' + 10);
            }
            bool nl = false;
            for (i64 i = 0; i < pos; i++)
                if (vt.p[i] == '\n') nl = true;
            if (ok && !nl) {
                i64 a;
                if (!classes_an_vt_addr(an, avp_strn(vt.p, (size_t)pos), &a)) return false;
                *addr = a + (i64)x;
                return true;
            }
        }
    }
    ClassesObj *o = classes_obj(an->prog, vt);
    if (o && o->has_loc) {
        *addr = o->addr;
        return true;
    }
    ClassesSymbol *s = classes_symbol(an->prog, vt);
    if (s) {
        *addr = s->addr;
        return true;
    }
    return false;
}

static int cmp_start(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const ClassesVtStart *p = x, *q = y;
    int c = avp_cmp_i64(p->addr, q->addr);
    return c ? c : avp_cmp_str(p->vt->name, q->vt->name);
}

ClassesVtable *classes_an_vt_at(ClassesAnalysis *an, i64 base, i64 add, i64 *off) {
    if (!an->vt_sorted_built) {
        an->vt_sorted = AVP_NEWN(an->a, ClassesVtStart, an->nvt ? an->nvt : 1);
        for (size_t i = 0; i < an->nvt; i++) {
            i64 a;
            if (classes_an_vt_addr(an, an->vts[i]->name, &a)) {
                an->vt_sorted[an->nvt_sorted].addr = a;
                an->vt_sorted[an->nvt_sorted].vt = an->vts[i];
                an->nvt_sorted++;
            }
        }
        avp_sort(an->vt_sorted, an->nvt_sorted, sizeof(ClassesVtStart), cmp_start, NULL, false);
        an->vt_sorted_built = true;
    }
    i64 a = base + add;
    size_t lo = 0, hi = an->nvt_sorted; /* bisect_right on the starts */
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (a < an->vt_sorted[mid].addr) hi = mid;
        else lo = mid + 1;
    }
    if (lo == 0) return NULL;
    const ClassesVtStart *s = &an->vt_sorted[lo - 1];
    if (a - s->addr < 4 * (i64)s->vt->words->n) {
        *off = a - s->addr;
        return s->vt;
    }
    return NULL;
}

/* ---- __init__: the vtables ------------------------------------------------------------------------------------ */

typedef AVP_VEC(ClassesVtable *) VtVec;

static ClassesVtable *add_vt(ClassesAnalysis *an, VtVec *order, AvpStr name, ClassesWords *words) {
    bool created;
    AvpVal *v = avp_map_put(an->vt, name, &created);
    if (!created) {
        ClassesVtable *r = v->p;
        r->words = words;
        return r;
    }
    ClassesVtable *r = AVP_NEW(an->a, ClassesVtable);
    r->name = avp_str_dup(an->a, name);
    r->index = order->n;
    r->words = words;
    v->p = r;
    avp_vec_push(order, r);
    return r;
}

ClassesAnalysis *classes_analysis_part1(AvpArena *a, ClassesProgram *prog, const ClassesRegions *regions) {
    return classes_analysis_part1_rule(a, prog, regions, 0);
}

ClassesAnalysis *classes_analysis_part1_rule(AvpArena *a, ClassesProgram *prog, const ClassesRegions *regions,
                                             int base_rule) {
    ClassesAnalysis *an = AVP_NEW(a, ClassesAnalysis);
    an->a = a;
    an->prog = prog;
    an->base_rule = base_rule;
    if (regions) an->data_regions = *regions;
    an->rtti = avp_map_new();
    AvpMap *found = classes_find_vtables(a, prog, an->rtti);
    an->vt = avp_map_new_cap(avp_map_len(found) * 2);
    VtVec order = {0};
    AvpMapIter it = {0};
    while (avp_map_next(found, &it)) add_vt(an, &order, it.key, it.val->p);
    ClassesEmbeddedList emb = classes_find_embedded_vtables(a, prog, found);
    an->embedded_by_sym = avp_map_new();
    for (size_t i = 0; i < emb.n; i++) {
        ClassesEmbedded *e = &emb.v[i];
        AvpStr nm = avp_str(avp_sprintf(a, "%.*s+0x%llX", AVP_SF(e->sym), (unsigned long long)e->base));
        if (e->base < 0) { /* "%X" of a negative int */
            AvpBuf b;
            avp_buf_init(&b, a);
            avp_buf_put(&b, e->sym);
            avp_buf_putz(&b, "+0x");
            put_pyhex(&b, e->base);
            nm = avp_buf_str(&b);
        }
        ClassesVtable *r = add_vt(an, &order, nm, &e->words);
        ClassesEmbeddedName en = {e->sym, e->base, r->name};
        avp_vec_push(&an->embedded, en);
        if (e->has_rtti) avp_map_put(an->rtti, r->name, NULL)->p = &e->rtti;
    }
    for (size_t i = 0; i < an->embedded.n; i++) {
        ClassesEmbeddedName *en = &an->embedded.v[i];
        bool created;
        AvpVal *v = avp_map_put(an->embedded_by_sym, en->sym, &created);
        if (created) v->p = AVP_NEW(a, ClassesEmbeddedAtList);
        ClassesEmbeddedAt at = {en->base, en->name};
        avp_vec_push((ClassesEmbeddedAtList *)v->p, at);
    }
    an->vts = order.v;
    an->nvt = order.n;
    for (size_t i = 0; i < an->nvt; i++) {
        AvpVal *v = avp_map_get(an->rtti, an->vts[i]->name);
        an->vts[i]->rtti = v ? v->p : NULL;
    }
    classes_an_sites(an);
    classes_an_groups(an);
    classes_an_bases(an);
    classes_an_images(an);
    classes_an_slots(an);
    return an;
}

/* ---- _sites ----------------------------------------------------------------------------------------------------- */

typedef struct {
    AvpStr tok;
    AVP_VEC(ClassesSt) sts;
} TokStores;

static size_t final_of(const ClassesSite *s, i64 off) {
    size_t i;
    for (i = 0; i < s->final.n; i++)
        if (s->final.v[i].off == off) break;
    AVP_ASSERT(i < s->final.n); /* every store's offset is a key of final */
    return s->final.v[i].st;
}

void classes_an_sites(ClassesAnalysis *an) {
    ClassesProgram *prog = an->prog;
    an->sites_of_func = avp_map_new();
    AVP_VEC(TokStores) by_tok = {0};
    AVP_VEC(i64) lo = {0};
    AVP_VEC(bool) dup = {0};
    AvpMapIter it = {0};
    while (avp_map_next(prog->info, &it)) {
        AvpStr fname = it.key;
        const ClassesInfo *inf = it.val->p;
        for (size_t i = 0; i < by_tok.n; i++) avp_vec_clear(&by_tok.v[i].sts);
        size_t ntok = 0;
        for (size_t i = 0; i < inf->stores.n; i++) {
            const ClassesStore *st = &inf->stores.v[i];
            ClassesSt e = {st->index, st->off, {0}, 0};
            ClassesVtable *r = classes_an_vt(an, st->sym);
            if (r) {
                e.vt = r->name;
                e.add = st->addend;
            } else {
                /* sym+addend can name a vtable that dtk did not split off, or the next object */
                i64 base, o;
                if (!classes_an_vt_addr(an, st->sym, &base)) continue;
                r = classes_an_vt_at(an, base, st->addend, &o);
                if (!r) continue;
                e.vt = r->name;
                e.add = o;
            }
            size_t t = 0;
            while (t < ntok && !eq(by_tok.v[t].tok, st->tok)) t++;
            if (t == ntok) {
                if (ntok == by_tok.n) avp_vec_push_zero(&by_tok);
                by_tok.v[t].tok = st->tok;
                ntok++;
            }
            avp_vec_push(&by_tok.v[t].sts, e);
        }
        size_t first_site = an->sites.n;
        for (size_t t = 0; t < ntok; t++) {
            ClassesSite *s = avp_vec_push_zero(&an->sites);
            s->func = fname;
            s->token = by_tok.v[t].tok;
            avp_vec_extend(&s->stores, by_tok.v[t].sts.v, by_tok.v[t].sts.n);
            for (size_t i = 0; i < s->stores.n; i++) {
                size_t k = 0;
                while (k < s->final.n && s->final.v[k].off != s->stores.v[i].off) k++;
                if (k == s->final.n) {
                    ClassesFinal f = {s->stores.v[i].off, i};
                    avp_vec_push(&s->final, f);
                } else {
                    s->final.v[k].st = i;
                }
            }
            for (size_t i = 0; i < s->stores.n; i++) {
                size_t f = final_of(s, s->stores.v[i].off);
                if (f != i && !eq(s->stores.v[f].vt, s->stores.v[i].vt)) avp_vec_push(&s->over, i);
            }
            for (size_t k = 0; k < s->final.n; k++) {
                AvpStr v = s->stores.v[s->final.v[k].st].vt;
                bool seen = false;
                for (size_t j = 0; j < s->vts.n && !seen; j++) seen = eq(s->vts.v[j], v);
                if (!seen) avp_vec_push(&s->vts, v);
            }
            avp_sort_strs(s->vts.v, s->vts.n);
            if (eq(s->token, AVP_S("a3"))) {
                bool dtor = startz(fname, "__dt__") || inf->calls_dl;
                s->kind = dtor ? CLASSES_K_DTOR : CLASSES_K_CTOR;
            } else if (classes_info_nw(inf, s->token)) {
                s->kind = CLASSES_K_FACTORY;
            } else {
                s->kind = CLASSES_K_ALLOC; /* an object returned by another call */
            }
            /* the owner: the one vtable, or (member objects with vtables of their own) the vtable installed
             * lowest, when no vtable is installed twice with one addend below it */
            avp_vec_clear(&lo);
            avp_vec_clear(&dup);
            for (size_t j = 0; j < s->vts.n; j++) {
                bool has = false, d = false;
                i64 m = 0;
                for (size_t k = 0; k < s->final.n; k++) {
                    const ClassesSt *st = &s->stores.v[s->final.v[k].st];
                    if (!eq(st->vt, s->vts.v[j])) continue;
                    if (!has || st->off < m) m = st->off;
                    has = true;
                    for (size_t q = 0; q < k && !d; q++) {
                        const ClassesSt *st2 = &s->stores.v[s->final.v[q].st];
                        if (eq(st2->vt, s->vts.v[j]) && st2->add == st->add) d = true;
                    }
                }
                avp_vec_push(&lo, m);
                avp_vec_push(&dup, d);
            }
            size_t first = 0;
            for (size_t j = 1; j < s->vts.n; j++)
                if (lo.v[j] < lo.v[first]) first = j; /* vts is sorted: the first lowest is min by (lo, v) */
            bool owned = !dup.v[first];
            if (owned && s->vts.n > 1) {
                bool hm = false;
                i64 m = 0;
                for (size_t j = 0; j < s->vts.n; j++) {
                    if (j == first) continue;
                    if (!hm || lo.v[j] < m) m = lo.v[j];
                    hm = true;
                }
                if (lo.v[first] == m) owned = false;
            }
            if (owned) s->owner = s->vts.v[first];
            for (size_t j = 0; j < s->vts.n; j++) {
                if (owned && j == first) continue;
                ClassesSiteMember mm = {s->vts.v[j], lo.v[j]};
                avp_vec_push(&s->members, mm);
            }
        }
        if (an->sites.n > first_site)
            avp_map_put(an->sites_of_func, fname, NULL)->u = ((u64)first_site << 32) | (u64)(an->sites.n - first_site);
    }
    for (size_t i = 0; i < by_tok.n; i++) avp_vec_free(&by_tok.v[i].sts);
    avp_vec_free(&by_tok);
    avp_vec_free(&lo);
    avp_vec_free(&dup);
}

AvpStr classes_an_vt_of_ctor(const ClassesAnalysis *an, AvpStr fname, unsigned kinds) {
    AvpVal *v = avp_map_get(an->sites_of_func, fname);
    AvpStr none = {0};
    if (!v) return none;
    size_t first = (size_t)(v->u >> 32), n = (size_t)(v->u & 0xFFFFFFFFu);
    for (size_t i = first; i < first + n; i++) {
        const ClassesSite *s = &an->sites.v[i];
        if (s->owner.p && (kinds & CLASSES_KIND_BIT(s->kind))) return s->owner;
    }
    return none;
}

/* ---- _groups ---------------------------------------------------------------------------------------------------- */

typedef struct {
    i64 a, o;
} Pair;
typedef AVP_VEC(Pair) Pairs;
typedef AVP_VEC(Pairs *) PairsList;

static void pairs_set(Pairs *p, i64 a, i64 o) {
    for (size_t i = 0; i < p->n; i++)
        if (p->v[i].a == a) {
            p->v[i].o = o;
            return;
        }
    Pair x = {a, o};
    avp_vec_push(p, x);
}

static ClassesInstallers *inst_of(ClassesVtable *r) {
    r->has_inst = true;
    return &r->inst;
}

void classes_an_groups(ClassesAnalysis *an) {
    AvpArena *a = an->a;
    PairsList *own = AVP_NEWN(a, PairsList, an->nvt + 1), *mixed = AVP_NEWN(a, PairsList, an->nvt + 1);
    for (size_t si = 0; si < an->sites.n; si++) {
        ClassesSite *s = &an->sites.v[si];
        for (size_t j = 0; j < s->vts.n; j++) {
            ClassesVtable *r = vt_rec(an, s->vts.v[j]);
            Pairs *pairs = AVP_NEW(a, Pairs);
            for (size_t k = 0; k < s->final.n; k++) {
                const ClassesSt *st = &s->stores.v[s->final.v[k].st];
                if (eq(st->vt, r->name)) pairs_set(pairs, st->add, st->off);
            }
            if (s->owner.p && eq(r->name, s->owner)) {
                avp_vec_push(&own[r->index], pairs);
                avp_vec_push(&inst_of(r)->k[s->kind], s->func);
                for (size_t m = 0; m < s->members.n; m++) {
                    ClassesMemberObj mo = {s->members.v[m].lo, s->members.v[m].vt};
                    bool seen = false;
                    for (size_t q = 0; q < r->member_objects.n && !seen; q++)
                        seen = r->member_objects.v[q].off == mo.off && eq(r->member_objects.v[q].vt, mo.vt);
                    if (!seen) avp_vec_push(&r->member_objects, mo);
                }
            } else {
                i64 mn = pairs->v[0].o;
                for (size_t k = 1; k < pairs->n; k++)
                    if (pairs->v[k].o < mn) mn = pairs->v[k].o;
                Pairs *norm = AVP_NEW(a, Pairs);
                for (size_t k = 0; k < pairs->n; k++) {
                    Pair x = {pairs->v[k].a, pairs->v[k].o - mn};
                    avp_vec_push(norm, x);
                }
                avp_vec_push(&mixed[r->index], norm);
                avp_vec_push(&inst_of(r)->k[CLASSES_K_OTHER], s->func);
            }
        }
    }
    for (size_t vi = 0; vi < an->nvt; vi++) {
        ClassesVtable *r = an->vts[vi];
        const PairsList *src = own[vi].n ? &own[vi] : mixed[vi].n ? &mixed[vi] : NULL;
        if (src) {
            ClassesGroup *g = &r->group;
            for (size_t p = 0; p < src->n; p++) {
                for (size_t k = 0; k < src->v[p]->n; k++) {
                    Pair x = src->v[p]->v[k];
                    const ClassesGroupEnt *e = classes_group_get(g, x.a);
                    if (e && e->o != x.o) {
                        AvpBuf b;
                        avp_buf_init(&b, a);
                        avp_buf_putz(&b, "addend 0x");
                        put_pyhex(&b, x.a);
                        avp_buf_putz(&b, ": this+0x");
                        put_pyhex(&b, e->o);
                        avp_buf_putz(&b, " vs this+0x");
                        put_pyhex(&b, x.o);
                        avp_vec_push(&r->conflicts, avp_buf_str(&b));
                    }
                    if (!e) {
                        ClassesGroupEnt ne = {x.a, true, x.o};
                        avp_vec_push(g, ne);
                    }
                }
            }
            if (!classes_group_get(g, 0)) {
                /* stores seen only for embedded groups; the complete group header is word 0 */
                ClassesGroupEnt ne = {0, false, 0};
                avp_vec_push(g, ne);
            }
            r->group_ev = own[vi].n ? AVP_S("stores") : AVP_S("stores (mixed site, offsets normalised)");
        } else {
            ClassesGroupEnt z = {0, false, 0};
            avp_vec_push(&r->group, z);
            const ClassesWords *w = r->words;
            size_t i = 2;
            while (i + 1 < w->n) {
                if (!w->v[i].p && !w->v[i + 1].p && i + 2 < w->n && w->v[i + 2].p) {
                    ClassesGroupEnt e = {(i64)i * 4, false, 0};
                    if (!classes_group_get(&r->group, e.a)) avp_vec_push(&r->group, e);
                    i += 2;
                    continue;
                }
                i++;
            }
            r->group_ev = AVP_S("shape only (zero-pair split)");
        }
    }
    for (size_t vi = 0; vi < an->nvt; vi++) {
        avp_vec_free(&own[vi]);
        avp_vec_free(&mixed[vi]);
    }
}

/* ---- _bases ----------------------------------------------------------------------------------------------------- */

bool classes_an_looks_dtor(const ClassesAnalysis *an, AvpStr fname) {
    ClassesFunc *f = classes_func(an->prog, fname);
    if (!f || f->insns.n < 4) return false;
    for (size_t i = 0; i < f->insns.n && i < 6; i++) {
        const ClassesInsn *in = &f->insns.v[i];
        if (startz(in->mn, "cmpwi") && in->nops >= 1 && avp_str_eqz(in->ops[0], "r3") &&
            (avp_str_eqz(in->ops[in->nops - 1], "0x0") || avp_str_eqz(in->ops[in->nops - 1], "0")))
            return true;
    }
    return false;
}

static void add_member_named(ClassesVtable *r, i64 off, AvpStr cls, AvpStr ev) {
    for (size_t i = 0; i < r->member_named.n; i++) {
        const ClassesMemberNamed *m = &r->member_named.v[i];
        if (m->off == off && eq(m->cls, cls) && eq(m->ev, ev)) return;
    }
    ClassesMemberNamed m = {off, cls, ev};
    avp_vec_push(&r->member_named, m);
}

static void add_cand(ClassesVtable *r, bool is_name, AvpStr b, bool is_store, i64 off, AvpStr ev) {
    ClassesCand c = {is_name, b, is_store, off, ev};
    avp_vec_push(&r->cand, c);
}

static void add_dtor(ClassesAnalysis *an, ClassesVtable *r, AvpStr f) {
    if (!r->dtor_funcs) {
        r->dtor_funcs = avp_set_new();
        avp_vec_push(&an->dtor_order, r->name);
    }
    avp_set_add(r->dtor_funcs, f);
}

void classes_an_bases(ClassesAnalysis *an) {
    ClassesProgram *prog = an->prog;
    AvpArena *a = an->a;
    an->ctor_of_ctor = avp_map_new();
    an->ctor_of_dtor = avp_map_new();
    for (size_t i = 0; i < an->sites.n; i++) {
        ClassesSite *s = &an->sites.v[i];
        if (!s->owner.p || (s->kind != CLASSES_K_CTOR && s->kind != CLASSES_K_DTOR)) continue;
        bool created;
        AvpVal *v = avp_map_put(s->kind == CLASSES_K_CTOR ? an->ctor_of_ctor : an->ctor_of_dtor, s->func, &created);
        if (created) v->p = &s->owner;
    }
    for (size_t si = 0; si < an->sites.n; si++) {
        ClassesSite *s = &an->sites.v[si];
        if (!s->owner.p) continue;
        ClassesVtable *r = vt_rec(an, s->owner);
        const ClassesInfo *inf = classes_info(prog, s->func);
        i64 first = 0;
        bool hf = false;
        for (size_t k = 0; k < s->stores.n; k++)
            if (eq(s->stores.v[k].vt, r->name) && (!hf || s->stores.v[k].idx < first)) {
                first = s->stores.v[k].idx;
                hf = true;
            }
        if (s->kind == CLASSES_K_CTOR || s->kind == CLASSES_K_FACTORY || s->kind == CLASSES_K_ALLOC) {
            for (size_t c = 0; c < inf->calls.n; c++) {
                const ClassesCall *call = &inf->calls.v[c];
                const ClassesVal *a3 = &call->args[0];
                if (a3->kind != CLASSES_V_OBJ || !eq(a3->name, s->token)) continue;
                AvpStr tgt = call->target;
                if (call->index >= first) {
                    /* constructed after the vptrs are set: a member */
                    if (startz(tgt, "__ct__") && !classes_an_vt_of_ctor(an, tgt, CLASSES_KIND_BIT(CLASSES_K_CTOR)).p &&
                        a3->v > 0) {
                        AvpStr cl = classes_mangled_class(a, tgt);
                        if (truthy(cl)) add_member_named(r, a3->v, cl, cat(a, "ctor-call ", tgt));
                    }
                    continue;
                }
                if (an->base_rule == 1 && classes_br_ctor_call(an, r, s, call)) continue;
                AvpStr bvt = classes_an_vt_of_ctor(an, tgt, CLASSES_KIND_BIT(CLASSES_K_CTOR));
                if (bvt.p && !eq(bvt, r->name)) {
                    add_cand(r, false, bvt, false, a3->v, cat(a, "ctor-call ", tgt));
                } else if (startz(tgt, "__ct__")) {
                    AvpStr cl = classes_mangled_class(a, tgt);
                    if (truthy(cl)) add_cand(r, true, cl, false, a3->v, cat(a, "ctor-call ", tgt));
                }
            }
            for (size_t k = 0; k < s->over.n; k++) {
                const ClassesSt *st = &s->stores.v[s->over.v[k]];
                if (st->add == 0 && eq(s->stores.v[final_of(s, st->off)].vt, r->name))
                    add_cand(r, false, st->vt, true, st->off, cat(a, "inlined-ctor store in ", s->func));
                    /* base rule 1: an inlined constructor's vptr, overwritten by the owner's: vptr evidence */
                    if (an->base_rule == 1) r->cand.v[r->cand.n - 1].tier = CLASSES_BR_VPTR;
            }
        }
        if (s->kind == CLASSES_K_DTOR) add_dtor(an, r, s->func);
    }
    /* destructors in the dtor slot that store nothing themselves */
    for (size_t vi = 0; vi < an->nvt; vi++) {
        ClassesVtable *r = an->vts[vi];
        for (size_t k = 0; k < r->words->n; k++) {
            AvpStr w = r->words->v[k];
            if (!w.p) continue;
            i64 adj;
            AvpStr t = classes_resolve_thunk(prog, w, &adj);
            bool d = startz(t, "__dt__");
            if (!d) {
                const ClassesInfo *ti = classes_info(prog, t);
                d = ti && ti->calls_dl && CLASSES_NONE(classes_mangled_class(a, t)) && classes_an_looks_dtor(an, t);
            }
            if (d) add_dtor(an, r, t);
        }
    }
    for (size_t di = 0; di < an->dtor_order.n; di++) {
        ClassesVtable *r = vt_rec(an, an->dtor_order.v[di]);
        AvpStrVec fns = avp_set_sorted(r->dtor_funcs);
        for (size_t fi = 0; fi < fns.n; fi++) {
            const ClassesInfo *inf = classes_info(prog, fns.v[fi]);
            if (!inf) continue;
            for (size_t c = 0; c < inf->calls.n; c++) {
                const ClassesCall *call = &inf->calls.v[c];
                const ClassesVal *a3 = &call->args[0];
                if (a3->kind != CLASSES_V_OBJ || !avp_str_eqz(a3->name, "a3")) continue;
                AvpStr tgt = call->target;
                AvpStr bvt = {0};
                AvpVal *v = avp_map_get(an->ctor_of_dtor, tgt);
                if (v) bvt = *(AvpStr *)v->p;
                if (!bvt.p) {
                    for (size_t d2 = 0; d2 < an->dtor_order.n; d2++) {
                        ClassesVtable *r2 = vt_rec(an, an->dtor_order.v[d2]);
                        if (r2 != r && avp_set_has(r2->dtor_funcs, tgt)) {
                            bvt = r2->name;
                            break;
                        }
                    }
                }
                if (an->base_rule == 1 && !eq(bvt, r->name) && classes_br_dtor_call(an, r, call, bvt))
                    continue;
                if (bvt.p && !eq(bvt, r->name)) {
                    add_cand(r, false, bvt, false, a3->v, cat(a, "dtor-call ", tgt));
                } else if (!bvt.p && startz(tgt, "__dt__") && !avp_set_has(r->dtor_funcs, tgt)) {
                    /* a class without a vtable destroyed at this+o: a member (or a non-polymorphic base; the two
                     * cannot be told apart here) */
                    AvpStr cl = classes_mangled_class(a, tgt);
                    if (truthy(cl)) add_member_named(r, a3->v, cl, cat(a, "dtor-call ", tgt));
                }
            }
        }
        avp_vec_free(&fns);
    }
}

/* ---- base images (_images and its helpers) ---------------------------------------------------------------------- */

size_t classes_an_fit_len(const ClassesVtable *bvt) {
    const ClassesWords *bw = bvt->words;
    size_t n = bw->n;
    if (!startz(bvt->name, "__vt__")) return n; /* only sized symbols keep trailing zeros (vtable_prefix) */
    while (n > 3 && !bw->v[n - 1].p) n--;
    return n;
}

/* _image_at: the word index in vt where bvt's table is copied for a base at offset off; false = None */
static bool image_at(const ClassesAnalysis *an, const ClassesVtable *vt, const ClassesVtable *bvt, i64 off, i64 *w) {
    i64 bco;
    if (!classes_an_complete_off(an, bvt, &bco)) return false;
    i64 want = off + bco;
    for (size_t i = 0; i < vt->group.n; i++) {
        const ClassesGroupEnt *e = &vt->group.v[i];
        if (e->has_o && e->o == want && classes_mod4(e->a) == 0) {
            i64 x = classes_floordiv4(e->a);
            if (x + (i64)classes_an_fit_len(bvt) <= (i64)vt->words->n) {
                *w = x;
                return true;
            }
        }
    }
    return false;
}

static bool compatible(const ClassesVtable *vt, const ClassesVtable *bvt, i64 w) {
    const ClassesWords *words = vt->words;
    size_t bn = classes_an_fit_len(bvt);
    for (size_t k = 0; k < bn; k++) {
        AvpStr x = bvt->words->v[k], y = word_py(words, w + (i64)k);
        if (x.p && !y.p) return false;
    }
    for (size_t i = 0; i < bvt->group.n; i++) {
        i64 a = bvt->group.v[i].a;
        if (classes_mod4(a) != 0) continue;
        i64 k = classes_floordiv4(a);
        if (word_py(words, w + k).p || (w + k + 1 < (i64)words->n && word_py(words, w + k + 1).p)) return false;
    }
    return true;
}

static bool is_desc(const ClassesAnalysis *an, const ClassesVtable *a, AvpStr b, int depth) {
    if (depth > 32) return true;
    for (size_t i = 0; i < a->bases.n; i++) {
        const ClassesBase *x = a->bases.v[i];
        if (x->base_is_name) continue;
        if (eq(x->base, b)) return true;
        const ClassesVtable *br = classes_an_vt(an, x->base);
        if (br ? is_desc(an, br, b, depth + 1) : depth + 1 > 32) return true;
    }
    return false;
}

bool classes_an_is_descendant(const ClassesAnalysis *an, AvpStr a, AvpStr b, int depth) {
    if (depth > 32) return true;
    const ClassesVtable *r = classes_an_vt(an, a);
    return r ? is_desc(an, r, b, depth) : false; /* bases.get(a, []) */
}

/* (target, toff) in _ancestors(x): through polymorphic bases with an offset */
static bool ancestors_has(const ClassesAnalysis *an, const ClassesVtable *x, AvpStr target, i64 toff, int depth) {
    if (depth > 32) return false;
    for (size_t i = 0; i < x->bases.n; i++) {
        const ClassesBase *b = x->bases.v[i];
        if (b->base_is_name || !b->has_offset) continue;
        if (b->offset == toff && eq(b->base, target)) return true;
        const ClassesVtable *br = classes_an_vt(an, b->base);
        if (br && ancestors_has(an, br, target, toff - b->offset, depth + 1)) return true;
    }
    return false;
}

static void prune_ancestors(ClassesAnalysis *an) {
    for (size_t vi = 0; vi < an->nvt; vi++) {
        ClassesVtable *r = an->vts[vi];
        ClassesBaseList bl = r->bases, keep = {0};
        for (size_t i = 0; i < bl.n; i++) {
            ClassesBase *b = bl.v[i];
            if (b->base_is_name || !b->has_offset) {
                avp_vec_push(&keep, b);
                continue;
            }
            ClassesBase *via = NULL;
            for (size_t j = 0; j < bl.n && !via; j++) {
                ClassesBase *o = bl.v[j];
                if (o == b || o->base_is_name || !o->has_offset) continue;
                const ClassesVtable *orr = classes_an_vt(an, o->base);
                if (orr && ancestors_has(an, orr, b->base, b->offset - o->offset, 0)) via = o;
            }
            if (via) {
                avp_vec_push(&via->evidence,
                             avp_str(avp_sprintf(an->a, "inlined-parent (grandparent vt:%.*s reached directly)",
                                                 AVP_SF(b->base))));
                continue;
            }
            avp_vec_push(&keep, b);
        }
        r->bases = keep; /* the old list's storage stays (never freed) */
    }
}

typedef struct {
    u32 vt, k;
} FnRef;
typedef AVP_VEC(FnRef) FnRefs;

typedef struct {
    ClassesAnalysis *an;
    FnRefs *fn_index;
    i64 *score; /* per vtable, in the current _overlap call */
    u32 *stamp;
    u32 gen;
    AVP_VEC(u32) order;
} Overlap;

static bool covered(const ClassesAnalysis *an, const ClassesVtable *vt, i64 w) {
    for (size_t i = 0; i < vt->bases.n; i++) {
        const ClassesBase *b = vt->bases.v[i];
        if (!b->has_word || b->base_is_name) continue;
        const ClassesVtable *br = vt_rec(an, b->base);
        if (w >= b->word && w < b->word + (i64)br->words->n) return true;
    }
    return false;
}

static int cmp_tied(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const ClassesVtable *p = *(ClassesVtable *const *)x, *q = *(ClassesVtable *const *)y;
    int c = avp_cmp_i64(p->exclusive, q->exclusive);
    if (c) return c;
    c = avp_cmp_u64(p->words->n, q->words->n);
    return c ? c : avp_cmp_str(p->name, q->name);
}

/* _overlap(vt, w0): the best base for the group at word w0 by slot overlap; NULL = None */
static ClassesVtable *overlap(Overlap *ov, const ClassesVtable *vt, i64 w0, i64 *score_out) {
    ClassesAnalysis *an = ov->an;
    const ClassesWords *words = vt->words;
    i64 n = (i64)words->n;
    ov->gen++;
    avp_vec_clear(&ov->order);
    for (i64 k = w0 + 2; k < n; k++) {
        i64 idx = k < 0 ? k + n : k;
        if (idx < 0) shsm_raise(SHSM_EXC_VALUE, "list index out of range");
        i32 fi = vt->word_fi[idx];
        if (fi < 0) continue;
        const FnRefs *refs = &ov->fn_index[fi];
        for (size_t i = 0; i < refs->n; i++) {
            u32 b = refs->v[i].vt;
            if (b != vt->index && k - (i64)refs->v[i].k == w0) {
                if (ov->stamp[b] != ov->gen) {
                    ov->stamp[b] = ov->gen;
                    ov->score[b] = 0;
                    avp_vec_push(&ov->order, b);
                }
                ov->score[b]++;
            }
        }
    }
    AVP_VEC(ClassesVtable *) ok = {0};
    for (size_t i = 0; i < ov->order.n; i++) {
        ClassesVtable *b = an->vts[ov->order.v[i]];
        i64 score = ov->score[b->index];
        if (w0 + (i64)classes_an_fit_len(b) > n) continue;
        i64 nonnull = 0;
        for (size_t k = 2; k < b->words->n; k++)
            if (b->words->v[k].p) nonnull++;
        if (score * 2 < nonnull || !compatible(vt, b, w0)) continue;
        avp_vec_push(&ok, b);
    }
    if (!ok.n) return NULL;
    i64 top = ov->score[ok.v[0]->index];
    for (size_t i = 1; i < ok.n; i++)
        if (ov->score[ok.v[i]->index] > top) top = ov->score[ok.v[i]->index];
    AVP_VEC(ClassesVtable *) tied = {0}, kept = {0};
    for (size_t i = 0; i < ok.n; i++)
        if (ov->score[ok.v[i]->index] == top) avp_vec_push(&tied, ok.v[i]);
    /* a tie between a class and its own descendant (siblings of this class match as well as their common
     * parent): keep the ancestor, the conservative answer */
    for (size_t i = 0; i < tied.n; i++) {
        bool desc = false;
        for (size_t j = 0; j < tied.n && !desc; j++)
            if (j != i && !eq(tied.v[j]->name, tied.v[i]->name) && is_desc(an, tied.v[i], tied.v[j]->name, 0))
                desc = true;
        if (!desc) avp_vec_push(&kept, tied.v[i]);
    }
    ClassesVtable **list = kept.n ? kept.v : tied.v;
    size_t ln = kept.n ? kept.n : tied.n;
    avp_sort(list, ln, sizeof(ClassesVtable *), cmp_tied, NULL, false);
    ClassesVtable *best = list[0];
    avp_vec_free(&ok);
    avp_vec_free(&tied);
    avp_vec_free(&kept);
    *score_out = top;
    return best;
}

static ClassesBase *find_seen(ClassesBaseList *seen, bool is_name, AvpStr b, i64 off) {
    for (size_t i = 0; i < seen->n; i++) {
        ClassesBase *x = seen->v[i];
        if (x->base_is_name == is_name && x->offset == off && eq(x->base, b)) return x;
    }
    return NULL;
}

static int cmp_i64p(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return avp_cmp_i64(*(const i64 *)x, *(const i64 *)y);
}

/* Base rule 1: trailing null words of a dtk-split base table that a derived table proves to be padding. A derived
 * table repeats every slot of its base's, so when vptr evidence places base B at word w of derived D and D ends
 * before B does, B's words from D's end on (all null) are no slots: dtk's split ran B to the next object's
 * alignment (lbl_803E2548, 0x20 bytes for five slots: unk_800FEB28's and unk_800D2DE4's tables end after the fifth).
 * B's table is cut there (the shortest such D), so its image covers no word of a derived class's own part. Sized
 * `__vt__` tables keep their trailing nulls (pure slots, vtable_prefix). The cut is a copy: the program's word list
 * is left as it is. */
static void trim_padding(ClassesAnalysis *an) {
    AvpArena *a = an->a;
    for (size_t vi = 0; vi < an->nvt; vi++) {
        const ClassesVtable *r = an->vts[vi];
        for (size_t ci = 0; ci < r->cand.n; ci++) {
            const ClassesCand *c = &r->cand.v[ci];
            if (c->b_is_name || c->tier != CLASSES_BR_VPTR || !c->has_word || c->word < 0) continue;
            ClassesVtable *br = vt_rec(an, c->b);
            if (!br || br == r || startz(br->name, "__vt__")) continue;
            i64 n = (i64)r->words->n - c->word;
            if (n < 3 || n >= (i64)br->words->n) continue;
            bool pad = true;
            for (size_t k = (size_t)n; k < br->words->n && pad; k++) pad = !br->words->v[k].p;
            if (!pad) continue;
            ClassesWords *w = AVP_NEW(a, ClassesWords);
            *w = *br->words;
            w->n = (size_t)n;
            w->cap = 0; /* shares the program's array: never grown */
            br->words = w;
        }
    }
}

/* Base rule 1: a parent whose constructor and destructor were inlined without a vptr store of their own. The
 * derived constructor then calls the grandparent's constructor and the vptr evidence names the grandparent
 * (AreaTriggerBox: it calls AreaTriggerBase's constructor and destructor, and __vt__19AreaTriggerBoxLogic is stored
 * nowhere). A table that no function installs, descends from the proven base and shares more slots with the
 * derived table at the base's word than any other table (overlap) is that parent: a derived table can share a
 * parent's own overrides only by inheriting them. It replaces the proven base, which it reaches itself; the note
 * is the reference's _prune_ancestors form. */
static void inlined_parents(ClassesAnalysis *an, Overlap *ov) {
    AvpArena *a = an->a;
    for (size_t vi = 0; vi < an->nvt; vi++) {
        ClassesVtable *r = an->vts[vi];
        for (size_t i = 0; i < r->bases.n; i++) {
            ClassesBase *x = r->bases.v[i];
            if (x->base_is_name || !x->has_word || !x->evidence.n ||
                avp_str_startswith(x->evidence.v[0], AVP_S("slot-overlap")))
                continue;
            i64 score;
            ClassesVtable *p = overlap(ov, r, x->word, &score);
            if (!p || eq(p->name, x->base) || p->has_inst || is_desc(an, p, r->name, 0) || !is_desc(an, p, x->base, 0))
                continue;
            AvpStr gp = x->base;
            x->base = p->name;
            avp_vec_push(&x->evidence, avp_str(avp_sprintf(a, "slot-overlap (%lld slots)", (long long)score)));
            avp_vec_push(&x->evidence, avp_str(avp_sprintf(a, "inlined-parent (grandparent vt:%.*s reached directly)",
                                                           AVP_SF(gp))));
        }
    }
}

void classes_an_images(ClassesAnalysis *an) {
    AvpArena *a = an->a;
    AVP_VEC(i64) offs = {0};
    AVP_VEC(bool) offs_ok = {0};
    if (an->base_rule == 1) trim_padding(an);
    for (size_t vi = 0; vi < an->nvt; vi++) {
        ClassesVtable *r = an->vts[vi];
        ClassesBaseList seen = {0};
        if (an->base_rule == 1) { /* every candidate's resolved base offset, for classes_br_admit */
            avp_vec_clear(&offs);
            avp_vec_clear(&offs_ok);
            for (size_t ci = 0; ci < r->cand.n; ci++) {
                const ClassesCand *c = &r->cand.v[ci];
                i64 off = c->off, bco = 0;
                bool ok = !c->off_is_store || (!c->b_is_name && classes_an_complete_off(an, vt_rec(an, c->b), &bco));
                avp_vec_push(&offs, c->off_is_store ? off - bco : off);
                avp_vec_push(&offs_ok, ok);
            }
        }
        for (size_t ci = 0; ci < r->cand.n; ci++) {
            const ClassesCand *c = &r->cand.v[ci];
            if (an->base_rule == 1 && !classes_br_admit(r, ci, offs.v, offs_ok.v)) continue;
            if (c->b_is_name) {
                ClassesBase *x = find_seen(&seen, true, c->b, c->off);
                if (!x) {
                    x = AVP_NEW(a, ClassesBase);
                    x->base_is_name = true;
                    x->base = c->b;
                    x->has_offset = true;
                    x->offset = c->off;
                    avp_vec_push(&seen, x);
                }
                avp_vec_push(&x->evidence, c->ev);
                continue;
            }
            ClassesVtable *br = vt_rec(an, c->b);
            i64 off = c->off;
            if (c->off_is_store) { /* inlined store: offset of the base's vptr */
                i64 bco;
                if (!classes_an_complete_off(an, br, &bco)) continue;
                off = c->off - bco;
            }
            i64 w;
            ClassesBase *known = an->base_rule == 1 ? find_seen(&seen, false, c->b, off) : NULL;
            if (known) { /* base rule 1: more evidence for a base already placed (classes_baserule.c) */
                bool has = false;
                for (size_t k = 0; k < known->evidence.n && !has; k++) has = eq(known->evidence.v[k], c->ev);
                if (!has) avp_vec_push(&known->evidence, c->ev);
                continue;
            }
            if (c->has_word) { /* base rule 1: the vptr evidence placed the table */
                w = c->word;
                if (w + (i64)classes_an_fit_len(br) > (i64)r->words->n || !compatible(r, br, w)) continue;
            } else if (!image_at(an, r, br, off, &w) || !compatible(r, br, w)) {
                continue;
            }
            ClassesBase *x = find_seen(&seen, false, c->b, off);
            if (!x) {
                x = AVP_NEW(a, ClassesBase);
                x->base = br->name;
                x->has_offset = true;
                x->offset = off;
                x->has_word = true;
                x->word = w;
                avp_vec_push(&x->evidence, c->ev);
                avp_vec_push(&seen, x);
            } else {
                bool has = false;
                for (size_t k = 0; k < x->evidence.n && !has; k++) has = eq(x->evidence.v[k], c->ev);
                if (!has) avp_vec_push(&x->evidence, c->ev);
            }
        }
        r->bases = seen;
        if (an->base_rule == 1) classes_br_finish(an, r);
    }
    avp_vec_free(&offs);
    avp_vec_free(&offs_ok);
    /* slot overlap, for groups no base image covers */
    AvpMap *fn_map = avp_map_new();
    AVP_VEC(FnRefs) fn_index = {0};
    for (size_t vi = 0; vi < an->nvt; vi++) {
        ClassesVtable *r = an->vts[vi];
        r->word_fi = AVP_NEWN(a, i32, r->words->n + 1);
        for (size_t k = 0; k < r->words->n; k++) {
            AvpStr w = r->words->v[k];
            if (!w.p) {
                r->word_fi[k] = -1;
                continue;
            }
            bool created;
            AvpVal *v = avp_map_put(fn_map, w, &created);
            if (created) {
                v->i = (i64)fn_index.n;
                avp_vec_push_zero(&fn_index);
            }
            r->word_fi[k] = (i32)v->i;
            FnRef ref = {(u32)vi, (u32)k};
            avp_vec_push(&fn_index.v[v->i], ref);
        }
    }
    for (size_t vi = 0; vi < an->nvt; vi++) {
        ClassesVtable *r = an->vts[vi];
        i64 n = 0;
        for (size_t k = 0; k < r->words->n; k++) /* a word used once in all tables is unique in its own */
            if (r->word_fi[k] >= 0 && fn_index.v[r->word_fi[k]].n == 1) n++;
        r->exclusive = n;
    }
    Overlap ov = {0};
    ov.an = an;
    ov.fn_index = fn_index.v;
    ov.score = AVP_NEWN(a, i64, an->nvt + 1);
    ov.stamp = AVP_NEWN(a, u32, an->nvt + 1);
    AvpI64Vec adds = {0};
    for (int round = 0; round < 3; round++) {
        bool changed = false;
        for (size_t vi = 0; vi < an->nvt; vi++) {
            ClassesVtable *r = an->vts[vi];
            avp_vec_clear(&adds);
            for (size_t i = 0; i < r->group.n; i++) avp_vec_push(&adds, r->group.v[i].a);
            avp_sort(adds.v, adds.n, sizeof(i64), cmp_i64p, NULL, false);
            for (size_t i = 0; i < adds.n; i++) {
                i64 ad = adds.v[i];
                if (classes_mod4(ad)) continue;
                i64 w0 = classes_floordiv4(ad);
                if (covered(an, r, w0)) continue;
                i64 score;
                ClassesVtable *bvt = overlap(&ov, r, w0, &score);
                if (!bvt) continue;
                i64 bco;
                bool hb = classes_an_complete_off(an, bvt, &bco);
                const ClassesGroupEnt *e = classes_group_get(&r->group, ad);
                bool has_off = hb && e && e->has_o;
                i64 off = has_off ? e->o - bco : 0;
                if (is_desc(an, bvt, r->name, 0) || (has_off && off < 0)) continue;
                /* The reference keys this base by slot overlap alone. Under base rule 1 the constructor evidence has
                 * already placed every base it proves (classes_br_*, through the candidates above), so a group
                 * reaching here has no such evidence and slot overlap stays the fallback (classes_baserule.c). */
                ClassesBase *x = AVP_NEW(a, ClassesBase);
                x->base = bvt->name;
                x->has_offset = has_off;
                x->offset = off;
                x->has_word = true;
                x->word = w0;
                avp_vec_push(&x->evidence, avp_str(avp_sprintf(a, "slot-overlap (%lld slots)", (long long)score)));
                avp_vec_push(&r->bases, x);
                changed = true;
            }
        }
        if (!changed) break;
    }
    if (an->base_rule == 1) inlined_parents(an, &ov);
    avp_vec_free(&adds);
    avp_vec_free(&ov.order);
    prune_ancestors(an);
}

/* ---- slot classification ---------------------------------------------------------------------------------------- */

static int cmp_span(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const ClassesSpan *p = x, *q = y;
    int c = avp_cmp_i64(p->start, q->start);
    if (c) return c;
    c = avp_cmp_i64(p->end, q->end);
    if (c) return c;
    if (p->base.p && q->base.p) {
        c = avp_cmp_str(p->base, q->base);
        if (c) return c;
    } else if (p->base.p || q->base.p) {
        shsm_raise(SHSM_EXC_TYPE, "'<' not supported between instances of '%s' and '%s'",
                   p->base.p ? "str" : "NoneType", q->base.p ? "str" : "NoneType");
    }
    return (int)p->uncertain - (int)q->uncertain;
}

ClassesSpans classes_an_spans(const ClassesAnalysis *an, const ClassesVtable *vt, int depth) {
    ClassesSpans out = {0};
    if (depth > 32) return out;
    const ClassesWords *words = vt->words;
    for (size_t i = 0; i < vt->bases.n; i++) {
        const ClassesBase *b = vt->bases.v[i];
        if (!b->has_word || b->base_is_name) continue;
        const ClassesVtable *br = vt_rec(an, b->base);
        if (b->word == 0) {
            ClassesSpans sub = classes_an_spans(an, br, depth + 1);
            avp_vec_extend(&out, sub.v, sub.n);
            avp_vec_free(&sub);
        } else {
            ClassesSpan s = {b->word, b->word + (i64)br->words->n, br->name, false};
            avp_vec_push(&out, s);
        }
    }
    size_t ncov = out.n;
    AvpI64Vec adds = {0};
    for (size_t i = 0; i < vt->group.n; i++) {
        i64 a = vt->group.v[i].a;
        if (classes_mod4(a) == 0 && a) avp_vec_push(&adds, classes_floordiv4(a));
    }
    avp_sort(adds.v, adds.n, sizeof(i64), cmp_i64p, NULL, false);
    for (size_t i = 0; i < adds.n; i++) {
        i64 w0 = adds.v[i];
        bool cov = false;
        for (size_t s = 0; s < ncov && !cov; s++) cov = out.v[s].start <= w0 && w0 < out.v[s].end;
        if (cov) continue;
        size_t j = i + 1;
        while (j < adds.n && adds.v[j] <= w0) j++;
        if (j < adds.n) {
            ClassesSpan s = {w0, adds.v[j], {0}, true};
            avp_vec_push(&out, s);
            continue;
        }
        const ClassesGroupEnt *e = classes_group_get(&vt->group, w0 * 4);
        bool has_this = e && e->has_o;
        i64 k = w0 + 2;
        while (k < (i64)words->n) {
            AvpStr f = word_py(words, k);
            if (!f.p) {
                k++;
                continue;
            }
            i64 adj;
            classes_resolve_thunk(an->prog, f, &adj);
            if (adj == 0 || (has_this && adj != e->o)) break;
            k++;
        }
        ClassesSpan s = {w0, k, {0}, true};
        avp_vec_push(&out, s);
    }
    avp_vec_free(&adds);
    avp_sort(out.v, out.n, sizeof(ClassesSpan), cmp_span, NULL, false);
    return out;
}

typedef struct {
    i64 w;
    const ClassesVtable *b;
} Image;

void classes_an_slots(ClassesAnalysis *an) {
    ClassesProgram *prog = an->prog;
    AvpISet *heads = avp_iset_new();
    AVP_VEC(Image) images = {0};
    for (size_t vi = 0; vi < an->nvt; vi++) {
        ClassesVtable *r = an->vts[vi];
        const ClassesWords *words = r->words;
        ClassesSpans spans = classes_an_spans(an, r, 0);
        avp_vec_clear(&images);
        for (size_t i = 0; i < r->bases.n; i++) {
            const ClassesBase *b = r->bases.v[i];
            if (b->has_word && !b->base_is_name) {
                Image im = {b->word, vt_rec(an, b->base)};
                avp_vec_push(&images, im);
            }
        }
        avp_iset_free(heads);
        heads = avp_iset_new();
        for (size_t i = 0; i < r->group.n; i++) {
            i64 a = r->group.v[i].a;
            if (classes_mod4(a) == 0) {
                avp_iset_add(heads, (u64)classes_floordiv4(a));
                avp_iset_add(heads, (u64)(classes_floordiv4(a) + 1));
            }
        }
        for (size_t m = 0; m < images.n; m++)
            for (size_t i = 0; i < images.v[m].b->group.n; i++) {
                i64 a = images.v[m].b->group.v[i].a;
                if (classes_mod4(a) == 0) {
                    avp_iset_add(heads, (u64)(images.v[m].w + classes_floordiv4(a)));
                    avp_iset_add(heads, (u64)(images.v[m].w + classes_floordiv4(a) + 1));
                }
            }
        for (size_t s = 0; s < spans.n; s++) {
            avp_iset_add(heads, (u64)spans.v[s].start);
            avp_iset_add(heads, (u64)(spans.v[s].start + 1));
        }
        for (size_t k = 0; k < words->n; k++) {
            AvpStr f = words->v[k];
            ClassesSlot *ent = avp_vec_push_zero(&r->slots);
            ent->index = (i64)k;
            ent->target = f;
            if (avp_iset_has(heads, (u64)k)) {
                ent->status = AVP_S("header");
                if (r->rtti)
                    for (size_t q = 0; q < r->rtti->n; q++)
                        if (r->rtti->v[q].k == (i64)k) {
                            ent->has_rtti = true;
                            ent->rtti = r->rtti->v[q].sym;
                            ent->rtti_offset = r->rtti->v[q].off;
                            break;
                        }
                continue;
            }
            if (f.p) {
                i64 adj;
                AvpStr t = classes_resolve_thunk(prog, f, &adj);
                if (adj) {
                    ent->has_thunk = true;
                    ent->thunk_adjust = adj;
                    ent->thunk_target = t;
                }
            }
            AvpStr status = {0};
            const ClassesVtable *base = NULL;
            AvpStr base_target = {0};
            for (size_t m = 0; m < images.n; m++) {
                i64 w = images.v[m].w;
                const ClassesWords *bw = images.v[m].b->words;
                if (w <= (i64)k && (i64)k < w + (i64)bw->n) {
                    base = images.v[m].b;
                    base_target = bw->v[(i64)k - w];
                    bool same = (!base_target.p && !f.p) || (base_target.p && f.p && eq(base_target, f));
                    status = same ? AVP_S("inherited") : AVP_S("override");
                    break;
                }
            }
            const ClassesSpan *span = NULL;
            for (size_t s = 0; s < spans.n; s++)
                if (spans.v[s].start <= (i64)k && (i64)k < spans.v[s].end) {
                    span = &spans.v[s];
                    break;
                }
            if (!status.p) status = (span && !span->base.p) ? AVP_S("unresolved") : AVP_S("new");
            if (!f.p && avp_str_eqz(status, "new")) status = AVP_S("null");
            ent->status = status;
            if (base) {
                ent->has_base = true;
                ent->base = base->name;
                ent->base_target = base_target;
                for (size_t m = 0; m < images.n; m++)
                    if (images.v[m].b == base) {
                        ent->base_index = (i64)k - images.v[m].w;
                        break;
                    }
            }
            ent->group = span ? AVP_S("embedded") : AVP_S("complete");
        }
        avp_vec_free(&spans);
    }
    avp_iset_free(heads);
    avp_vec_free(&images);
}
