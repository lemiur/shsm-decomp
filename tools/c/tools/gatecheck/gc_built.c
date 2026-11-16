/* gatecheck layout, built side: the data sections of a relocatable object and the linker's dead stripping
 * (gatecheck.py live_symbols, _relocs_of, BuiltSection, built_sections). */
#include "gatecheck.h"

typedef struct {
    i64 start, end, index;
} Owner;
typedef AVP_VEC(Owner) OwnerVec;

static int cmp_owner(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const Owner *a = x, *b = y;
    int c = avp_cmp_i64(a->start, b->start);
    if (!c) c = avp_cmp_i64(a->end, b->end);
    return c ? c : avp_cmp_i64(a->index, b->index);
}

static i64 owner_of(const OwnerVec *owners, size_t nsecs, i64 shndx, i64 off) {
    if (shndx < 0 || shndx >= (i64)nsecs) return -1;
    const OwnerVec *l = &owners[shndx];
    if (!l->n) return -1;
    /* bisect_right on the starts */
    size_t lo = 0, hi = l->n;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (off < l->v[mid].start)
            hi = mid;
        else
            lo = mid + 1;
    }
    for (i64 i = (i64)lo - 1; i >= 0; i--) {
        if (l->v[i].start <= off && off < l->v[i].end) return l->v[i].index;
        if (l->v[i].end <= off && i > 0 && l->v[i - 1].end <= off) break;
    }
    return -1;
}

static bool set_has(const AvpSet *s, const char *name) { return s && avp_set_has(s, avp_str(name)); }

u8 *gc_live_symbols(AvpArena *a, GcElf *e, const AvpSet *kept, const AvpSet *dropped) {
    gc_elf_symbols(a, e);
    size_t ns = e->nsyms;
    OwnerVec *owners = AVP_NEWN(a, OwnerVec, e->nsecs + 1);
    for (size_t k = 0; k < ns; k++) {
        const GcSym *s = &e->syms[k];
        if (s->type == GC_STT_SECTION || s->type == GC_STT_FILE || !s->name[0] || s->shndx == 0 ||
            s->shndx == GC_SHN_ABS || s->shndx >= (i64)e->nsecs || s->size == 0)
            continue;
        avp_vec_push(&owners[s->shndx], ((Owner){s->value, s->value + s->size, s->index}));
    }
    for (size_t k = 0; k < e->nsecs; k++) avp_sort(owners[k].v, owners[k].n, sizeof(Owner), cmp_owner, NULL, false);
    u8 *live = AVP_NEWN(a, u8, ns + 1);
    AvpI64Vec work = {0};
    for (size_t k = 0; k < ns; k++) {
        const GcSym *s = &e->syms[k];
        if (set_has(kept, s->name) && s->shndx != 0 && s->type != GC_STT_SECTION && s->type != GC_STT_FILE &&
            !set_has(dropped, s->name)) {
            live[s->index] = 1;
            avp_vec_push(&work, s->index);
        }
    }
    AvpI64Vec *edges = AVP_NEWN(a, AvpI64Vec, ns + 1);
    GcRelVec rels = {0};
    AvpI64Vec tgts = {0};
    for (size_t k = 0; k < e->nsecs; k++) {
        const GcSec *sec = &e->secs[k];
        if (sec->type != GC_SHT_RELA && sec->type != GC_SHT_REL) continue;
        i64 src_sec = sec->info;
        const char *src_name =
            src_sec < (i64)e->nsecs ? avp_cz(a, gc_base_section(avp_str(e->secs[src_sec].name))) : "";
        bool root = gc_is_root_section(src_name);
        avp_vec_clear(&rels);
        gc_elf_relocs_of(e, sec, &rels);
        for (size_t ri = 0; ri < rels.n; ri++) {
            const GcRel *r = &rels.v[ri];
            if (r->sym >= (i64)ns) continue;
            const GcSym *t = &e->syms[r->sym];
            avp_vec_clear(&tgts);
            if (strncmp(t->name, "...", 3) == 0 && t->shndx != 0 && t->shndx != GC_SHN_ABS) {
                if (t->shndx < (i64)e->nsecs)
                    for (size_t q = 0; q < owners[t->shndx].n; q++) avp_vec_push(&tgts, owners[t->shndx].v[q].index);
            } else if (t->type == GC_STT_SECTION) {
                avp_vec_push(&tgts, owner_of(owners, e->nsecs, t->shndx, r->addend));
            } else if (t->shndx == 0 || t->shndx >= 0xFF00) {
            } else {
                avp_vec_push(&tgts, t->index);
            }
            i64 src = owner_of(owners, e->nsecs, src_sec, r->offset);
            for (size_t q = 0; q < tgts.n; q++) {
                i64 tgt = tgts.v[q];
                if (tgt < 0) continue;
                if (dropped && set_has(dropped, e->syms[tgt].name)) continue;
                if (src < 0 || root) {
                    if (!live[tgt]) {
                        live[tgt] = 1;
                        avp_vec_push(&work, tgt);
                    }
                } else {
                    avp_vec_push(&edges[src], tgt);
                }
            }
        }
    }
    while (work.n) {
        i64 n = avp_vec_pop(&work);
        for (size_t q = 0; q < edges[n].n; q++) {
            i64 t = edges[n].v[q];
            if (!live[t]) {
                live[t] = 1;
                avp_vec_push(&work, t);
            }
        }
    }
    for (size_t k = 0; k < ns; k++) avp_vec_free(&edges[k]);
    for (size_t k = 0; k < e->nsecs; k++) avp_vec_free(&owners[k]);
    avp_vec_free(&work);
    avp_vec_free(&rels);
    avp_vec_free(&tgts);
    return live;
}

GcBuiltSection *gc_built_get(const GcBuiltSections *s, const char *name) {
    for (size_t i = 0; i < s->v.n; i++)
        if (strcmp(s->v.v[i]->name, name) == 0) return s->v.v[i];
    return NULL;
}

static int cmp_sec_name(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return strcmp((*(GcSec *const *)x)->name, (*(GcSec *const *)y)->name);
}
static int cmp_sym_value(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const GcSym *a = *(GcSym *const *)x, *b = *(GcSym *const *)y;
    int c = avp_cmp_i64(a->value, b->value);
    return c ? c : avp_cmp_i64(b->size, a->size);
}
static int cmp_str(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return strcmp(*(const char *const *)x, *(const char *const *)y);
}

/* the entries of a sorted reloc dict with lo <= off < hi, re-keyed off - shift */
static size_t lower_bound(const GcRelocVec *v, i64 off) {
    size_t lo = 0, hi = v->n;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (v->v[mid].off < off)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

static GcRelocVec relocs_range(const GcRelocVec *v, i64 lo, i64 hi, i64 shift) {
    GcRelocVec out = {0};
    for (size_t i = lower_bound(v, lo); i < v->n && v->v[i].off < hi; i++) {
        GcReloc r = v->v[i];
        r.off -= shift;
        avp_vec_push(&out, r);
    }
    return out;
}

GcBuiltSections *gc_built_sections(AvpArena *a, GcElf *e, const AvpSet *kept, const GcDedupeVec *dedupe) {
    gc_elf_symbols(a, e);
    size_t ns = e->nsyms;
    u8 *live = kept ? gc_live_symbols(a, e, kept, NULL) : NULL;
    const char **dedup_of = AVP_NEWN(a, const char *, ns + 1);
    if (live && dedupe && dedupe->n) {
        /* a name this unit's own retail asm defines is not a duplicate */
        AvpMap *drop = avp_map_new();
        for (size_t i = 0; i < dedupe->n; i++)
            if (!avp_set_has(kept, avp_str(dedupe->v[i].name)))
                avp_map_put(drop, avp_str(dedupe->v[i].name), NULL)->p = (void *)dedupe->v[i].unit;
        AvpSet *defined = avp_set_new();
        for (size_t k = 0; k < ns; k++)
            if (e->syms[k].shndx != 0 && e->syms[k].shndx != GC_SHN_ABS) avp_set_add(defined, avp_str(e->syms[k].name));
        AVP_VEC(const char *) present = {0};
        AvpMapIter it = {0};
        while (avp_map_next(drop, &it))
            if (avp_set_has(defined, it.key)) avp_vec_push(&present, avp_cz(a, it.key));
        avp_sort(present.v, present.n, sizeof(const char *), cmp_str, NULL, false);
        if (present.n) {
            u8 *full = live;
            AvpSet *all = avp_set_new();
            for (size_t i = 0; i < present.n; i++) avp_set_add(all, avp_str(present.v[i]));
            live = gc_live_symbols(a, e, kept, all);
            for (size_t i = 0; i < present.n; i++) {
                AvpSet *one = avp_set_new();
                avp_set_add(one, avp_str(present.v[i]));
                u8 *l1 = gc_live_symbols(a, e, kept, one);
                const char *u = avp_map_get(drop, avp_str(present.v[i]))->p;
                for (size_t k = 0; k < ns; k++)
                    if (full[k] && !l1[k] && !dedup_of[k]) dedup_of[k] = u;
                avp_set_free(one);
            }
            const char *u0 = avp_map_get(drop, avp_str(present.v[0]))->p;
            for (size_t k = 0; k < ns; k++)
                if (full[k] && !live[k] && !dedup_of[k]) dedup_of[k] = u0;
            avp_set_free(all);
        }
        avp_vec_free(&present);
        avp_set_free(defined);
        avp_map_free(drop);
    }
    /* group the data sections by base name, in order of first appearance */
    AVP_VEC(const char *) names = {0};
    AVP_VEC(AVP_VEC(GcSec *)) groups = {0};
    for (size_t k = 0; k < e->nsecs; k++) {
        GcSec *sec = &e->secs[k];
        if ((sec->type != 1 && sec->type != GC_SHT_NOBITS) || sec->size == 0) continue;
        const char *b = avp_cz(a, gc_base_section(avp_str(sec->name)));
        if (!gc_is_data_section(b)) continue;
        size_t gi = 0;
        while (gi < names.n && strcmp(names.v[gi], b) != 0) gi++;
        if (gi == names.n) {
            avp_vec_push(&names, b);
            avp_vec_push_zero(&groups);
        }
        avp_vec_push(&groups.v[gi], sec);
    }
    GcBuiltSections *out = AVP_NEW(a, GcBuiltSections);
    AVP_VEC(GcSym *) cands = {0};
    GcRelVec rels = {0};
    for (size_t gi = 0; gi < names.n; gi++) {
        const char *name = names.v[gi];
        GcSec **parts = groups.v[gi].v;
        size_t np = groups.v[gi].n;
        avp_sort(parts, np, sizeof(GcSec *), cmp_sec_name, NULL, false);
        GcBuiltSection *bs = AVP_NEW(a, GcBuiltSection);
        bs->name = name;
        bs->nobits = gc_is_nobits_section(name);
        i64 base = 0;
        i64 *bases = AVP_NEWN(a, i64, np);
        AvpStr *datas = AVP_NEWN(a, AvpStr, np);
        bool *hasdata = AVP_NEWN(a, bool, np);
        for (size_t pi = 0; pi < np; pi++) {
            GcSec *sec = parts[pi];
            i64 al = sec->align > 1 ? sec->align : 1;
            base = (base + al - 1) / al * al;
            if (sec->align > bs->align) bs->align = sec->align;
            bool has = sec->type != GC_SHT_NOBITS;
            AvpStr data = has ? gc_elf_section_data(e, sec) : AVP_S("");
            /* relocs: {offset + base: (target, addend)} */
            avp_vec_clear(&rels);
            gc_elf_relocs_for(e, sec->index, &rels);
            GcRelocVec relocs = {0};
            for (size_t ri = 0; ri < rels.n; ri++) {
                const GcRel *r = &rels.v[ri];
                const char *tname;
                if (r->sym < (i64)ns) {
                    const GcSym *t = &e->syms[r->sym];
                    tname = t->name;
                    if (t->type == GC_STT_SECTION || !tname[0])
                        tname = avp_sprintf(a, "@%s", t->shndx < (i64)e->nsecs ? e->secs[t->shndx].name : "?");
                } else {
                    tname = "?";
                }
                GcReloc rr = {r->offset + base, tname,
                              r->addend ? avp_sprintf(a, "%+lld", (long long)r->addend) : ""};
                avp_vec_push(&relocs, rr);
            }
            gc_relocs_dict(&relocs);
            avp_vec_clear(&cands);
            for (size_t k = 0; k < ns; k++) {
                GcSym *s = &e->syms[k];
                if (s->shndx == sec->index && s->type != GC_STT_SECTION && s->type != GC_STT_FILE && s->name[0])
                    avp_vec_push(&cands, s);
            }
            avp_sort(cands.v, cands.n, sizeof(GcSym *), cmp_sym_value, NULL, false);
            AVP_VEC(GcSym *) objs = {0};
            i64 end = 0;
            for (size_t q = 0; q < cands.n; q++) {
                GcSym *s = cands.v[q];
                if (s->size == 0 || s->value < end) continue;
                avp_vec_push(&objs, s);
                end = s->value + s->size;
            }
            i64 pos = 0;
            for (size_t q = 0; q <= objs.n; q++) {
                GcSym *s = q < objs.n ? objs.v[q] : NULL;
                i64 nxt = s ? s->value : sec->size;
                if (nxt > pos) {
                    AvpStr gapdata = has ? avp_str_slice(data, pos, nxt) : AVP_S("");
                    bool nonzero = false;
                    for (size_t z = 0; z < gapdata.n && !nonzero; z++) nonzero = gapdata.p[z] != 0;
                    size_t z = lower_bound(&relocs, base + pos);
                    bool has_rel = z < relocs.n && relocs.v[z].off < base + nxt;
                    if ((has && nonzero) || has_rel || (!objs.n && !has)) {
                        GcObj *o = gc_obj_new(a, "<unnamed>", base + pos, nxt - pos,
                                              has ? (const u8 *)(gapdata.p ? gapdata.p : "") : NULL,
                                              (i64)gapdata.n, false, true);
                        o->relocs = relocs_range(&relocs, base + pos, base + nxt, base + pos);
                        avp_vec_push(&bs->objs, o);
                    }
                }
                if (s) {
                    AvpStr od = has ? avp_str_slice(data, s->value, s->value + s->size) : AVP_S("");
                    GcObj *o = gc_obj_new(a, s->name, base + s->value, s->size,
                                          has ? (const u8 *)(od.p ? od.p : "") : NULL, (i64)od.n, false, false);
                    o->relocs = relocs_range(&relocs, base + s->value, base + s->value + s->size, base + s->value);
                    if (live && !live[s->index] && !gc_is_root_section(name)) {
                        o->dead = true;
                        o->dedup = dedup_of[s->index];
                    }
                    avp_vec_push(&bs->objs, o);
                    pos = s->value + s->size;
                }
            }
            avp_vec_free(&objs);
            avp_vec_extend(&bs->relocs, relocs.v, relocs.n);
            avp_vec_free(&relocs);
            bases[pi] = base;
            datas[pi] = data;
            hasdata[pi] = has;
            base += sec->size;
        }
        gc_relocs_dict(&bs->relocs);
        bs->size = base;
        if (!bs->nobits) {
            u8 *buf = AVP_NEWN(a, u8, (size_t)base + 1);
            for (size_t pi = 0; pi < np; pi++)
                if (hasdata[pi]) memcpy(buf + bases[pi], datas[pi].p, datas[pi].n);
            bs->data = buf;
            bs->dlen = base;
        }
        bool prev_dead = false;
        for (size_t q = 0; q < bs->objs.n; q++) {
            GcObj *o = bs->objs.v[q];
            if (o->dead) {
                prev_dead = true;
            } else {
                o->after_dead = prev_dead;
                prev_dead = false;
            }
        }
        avp_vec_push(&out->v, bs);
    }
    avp_vec_free(&cands);
    avp_vec_free(&rels);
    return out;
}
