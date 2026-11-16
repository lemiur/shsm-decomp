/* neutral: object models and their comparison (tools/neutral.py object_model, diff_models). The ELF reader is
 * gatecheck's (the Python loads gatecheck.py for its Elf class). */
#include "gatecheck.h"
#include "neutral.h"

enum { STB_LOCAL = 0, SHF_ALLOC = 2, SHF_EXEC = 4 };

NeutralModel *neutral_model_new(AvpArena *a) {
    NeutralModel *m = AVP_NEW(a, NeutralModel);
    m->functions = avp_map_new();
    m->data = avp_map_new();
    return m;
}

static int cmp_i64(i64 x, i64 y) { return x < y ? -1 : x > y; }

/* a total order on relocation tuples (both sides are sorted the same way, so equal multisets compare equal) */
static int cmp_reloc(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const NeutralReloc *p = x, *q = y;
    int c = cmp_i64(p->off, q->off);
    if (!c) c = cmp_i64(p->type, q->type);
    if (!c) c = strcmp(p->target.name, q->target.name);
    if (!c) c = cmp_i64(p->target.value, q->target.value);
    if (!c) c = cmp_i64(p->target.has_extra, q->target.has_extra);
    if (!c) c = cmp_i64(p->target.extra, q->target.extra);
    return c;
}

void neutral_sort_relocs(NeutralRelocVec *v) { avp_sort(v->v, v->n, sizeof *v->v, cmp_reloc, NULL, false); }

static bool relocs_eq(const NeutralRelocVec *x, const NeutralRelocVec *y) {
    if (x->n != y->n) return false;
    for (size_t i = 0; i < x->n; i++)
        if (cmp_reloc(&x->v[i], &y->v[i], NULL)) return false;
    return true;
}

static NeutralTarget target_of(const GcElf *e, const char *const *secnames, i64 idx, i64 addend) {
    NeutralTarget t = {0};
    if (idx < 0 || (size_t)idx >= e->nsyms) {
        t.name = "?";
        t.value = idx;
        t.has_extra = true;
        t.extra = addend;
        return t;
    }
    const GcSym *s = &e->syms[idx];
    if (s->bind != STB_LOCAL || (s->name && *s->name && s->name[0] != '@' && (s->type == 1 || s->type == 2))) {
        t.name = s->name ? s->name : "";
        t.value = addend;
        return t;
    }
    t.name = (s->shndx >= 0 && (size_t)s->shndx < e->nsecs) ? secnames[s->shndx] : "?";
    t.value = s->value + addend;
    return t;
}

NeutralModel *neutral_object_model(AvpArena *a, const char *path) {
    GcElf *e = gc_elf_load(a, path);
    gc_elf_symbols(a, e);
    const char **secnames = avp_alloc(a, (e->nsecs + 1) * sizeof *secnames);
    AvpMap *count = avp_map_new();
    bool cr = false;
    for (size_t i = 0; i < e->nsecs; i++) {
        const GcSec *s = &e->secs[i];
        AvpVal *v = avp_map_put(count, avp_str(s->name), &cr);
        i64 k = cr ? 0 : v->i;
        v->i = k + 1;
        secnames[i] = k == 0 ? s->name : avp_sprintf(a, "%s#%lld", s->name, (long long)k);
    }
    avp_map_free(count);
    /* relocs: {index: relocs_for(index)} of the allocated sections */
    GcRelVec *relocs = avp_alloc(a, (e->nsecs + 1) * sizeof *relocs);
    bool *alloc = avp_alloc(a, (e->nsecs + 1) * sizeof *alloc);
    for (size_t i = 0; i < e->nsecs; i++) {
        if (!(e->secs[i].flags & SHF_ALLOC)) continue;
        alloc[i] = true;
        gc_elf_relocs_for(e, e->secs[i].index, &relocs[i]);
    }
    NeutralModel *m = neutral_model_new(a);
    for (size_t k = 0; k < e->nsyms; k++) {
        const GcSym *s = &e->syms[k];
        if (s->type != GC_STT_FUNC || !s->name || !*s->name) continue;
        if (s->shndx < 0 || (size_t)s->shndx >= e->nsecs || !alloc[s->shndx]) continue;
        const GcSec *sec = &e->secs[s->shndx];
        AvpStr data = gc_elf_section_data(e, sec);
        NeutralFunc *f = AVP_NEW(a, NeutralFunc);
        f->body = avp_str_slice(data, s->value, s->value + s->size);
        const GcRelVec *rl = &relocs[s->shndx];
        for (size_t r = 0; r < rl->n; r++) {
            if (!(s->value <= rl->v[r].offset && rl->v[r].offset < s->value + s->size)) continue;
            NeutralReloc nr = {rl->v[r].offset - s->value, rl->v[r].type,
                               target_of(e, secnames, rl->v[r].sym, rl->v[r].addend)};
            avp_vec_push(&f->relocs, nr);
        }
        neutral_sort_relocs(&f->relocs);
        avp_map_put(m->functions, avp_str(s->name), &cr)->p = f; /* a repeated name: the last one wins */
    }
    for (size_t i = 0; i < e->nsecs; i++) {
        const GcSec *sec = &e->secs[i];
        if (!(sec->flags & SHF_ALLOC) || (sec->flags & SHF_EXEC)) continue;
        NeutralData *d = AVP_NEW(a, NeutralData);
        d->size = sec->size;
        d->bytes = gc_elf_section_data(e, sec);
        const GcRelVec *rl = &relocs[i];
        for (size_t r = 0; r < rl->n; r++) {
            NeutralReloc nr = {rl->v[r].offset, rl->v[r].type, target_of(e, secnames, rl->v[r].sym, rl->v[r].addend)};
            avp_vec_push(&d->relocs, nr);
        }
        neutral_sort_relocs(&d->relocs);
        avp_map_put(m->data, avp_str(secnames[i]), &cr)->p = d;
    }
    return m;
}

static AvpStrVec union_sorted(const AvpMap *x, const AvpMap *y) {
    AvpSet *s = avp_set_new();
    AvpMapIter it = {0};
    while (avp_map_next(x, &it)) avp_set_add(s, it.key);
    it = (AvpMapIter){0};
    while (avp_map_next(y, &it)) avp_set_add(s, it.key);
    AvpStrVec out = avp_set_sorted(s);
    /* the members' memory belongs to the set: keep it alive (freed with the process) */
    return out;
}

void neutral_diff_models(const NeutralModel *old, const NeutralModel *new_, NeutralChangeVec *funcs,
                         NeutralChangeVec *data) {
    AvpArena *a = avp_arena();
    AvpStrVec names = union_sorted(old->functions, new_->functions);
    for (size_t i = 0; i < names.n; i++) {
        AvpVal *va = avp_map_get(old->functions, names.v[i]), *vb = avp_map_get(new_->functions, names.v[i]);
        const char *kind = NULL;
        if (!va) kind = "added";
        else if (!vb) kind = "removed";
        else {
            const NeutralFunc *fa = va->p, *fb = vb->p;
            if (!avp_str_eq(fa->body, fb->body)) kind = "bytes";
            else if (!relocs_eq(&fa->relocs, &fb->relocs)) kind = "relocations";
        }
        if (kind) avp_vec_push(funcs, ((NeutralChange){avp_cz(a, names.v[i]), kind}));
    }
    avp_vec_free(&names);
    names = union_sorted(old->data, new_->data);
    for (size_t i = 0; i < names.n; i++) {
        AvpVal *va = avp_map_get(old->data, names.v[i]), *vb = avp_map_get(new_->data, names.v[i]);
        const char *kind = NULL;
        if (!va) kind = "added";
        else if (!vb) kind = "removed";
        else {
            const NeutralData *da = va->p, *db = vb->p;
            if (da->size != db->size) kind = "size";
            else if (!avp_str_eq(da->bytes, db->bytes)) kind = "bytes";
            else if (!relocs_eq(&da->relocs, &db->relocs)) kind = "relocations";
        }
        if (kind) avp_vec_push(data, ((NeutralChange){avp_cz(a, names.v[i]), kind}));
    }
    avp_vec_free(&names);
}
