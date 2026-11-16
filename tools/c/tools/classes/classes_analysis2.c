/* classes_analysis2.c: part 2 of `class Analysis` (tools/classes.py lines 1430-1934): _names, _mangled_only,
 * _members_and_fields, _finish, _depth, _est_size, _ranges, _propagate, _layout, _fref, _word_json, stats,
 * region_of, _regions and to_json, plus _ctype (1937-1950).
 *
 * Counter.most_common keeps the first-inserted key on ties, sorts are stable and compare strings by code point, as
 * the Python does. Where the Python raises (prog.info[f] for a member without a .fn, sorting a None factory address
 * against an int) the port raises the same exception class. */
#include "classes_analysis.h"

#include "shsm_cli.h"
#include "shsm_text.h"

static bool eq(AvpStr x, AvpStr y) { return avp_str_eq(x, y); }
static bool eqz(AvpStr x, const char *z) { return x.p != NULL && avp_str_eqz(x, z); }
static bool startz(AvpStr s, const char *z) {
    size_t n = strlen(z);
    return s.p != NULL && s.n >= n && memcmp(s.p, z, n) == 0;
}
static bool truthy(AvpStr s) { return s.p != NULL && s.n > 0; }
static AvpStr cat(AvpArena *a, const char *pre, AvpStr s) { return avp_str_concat(a, avp_str(pre), s); }

static AvpJson *jstr(AvpArena *a, AvpStr s) { return s.p ? avp_json_str_ref(a, s) : avp_json_null(); }
static AvpJson *jstrz(AvpArena *a, const char *z) { return avp_json_str_ref(a, avp_str(z)); }
static AvpJson *jhx(AvpArena *a, bool has, i64 v) {
    return has ? avp_json_str_ref(a, classes_hx(a, v)) : avp_json_null();
}
static void put(AvpArena *a, AvpJson *o, const char *k, AvpJson *v) { avp_json_set_ref(a, o, avp_str(k), v); }

static AvpJson *jfaddr(const ClassesAnalysis *an, AvpStr f) {
    i64 v = 0;
    bool has = classes_faddr(an->prog, f, &v);
    return jhx(an->a, has, v);
}
static i64 faddr0(const ClassesAnalysis *an, AvpStr f) { /* prog.faddr(f) or 0 */
    i64 v = 0;
    return classes_faddr(an->prog, f, &v) ? v : 0;
}
/* _fref(f) */
static AvpJson *fref(const ClassesAnalysis *an, AvpStr f) {
    AvpJson *o = avp_json_obj(an->a);
    put(an->a, o, "name", jstr(an->a, f));
    put(an->a, o, "address", jfaddr(an, f));
    return o;
}

static ClassesRec *rec_of(const ClassesAnalysis *an, AvpStr key) {
    AvpVal *v = avp_map_get(an->classes, key);
    return v ? (ClassesRec *)v->p : NULL;
}
static AvpStr *factory_of(const ClassesAnalysis *an, AvpStr f) {
    AvpVal *v = avp_map_get(an->factories, f);
    return v ? (AvpStr *)v->p : NULL;
}
static AvpSet *tokens_of(ClassesAnalysis *an, AvpStr f, bool create) {
    bool created = false;
    if (!create) {
        AvpVal *v = avp_map_get(an->factory_tokens, f);
        return v ? (AvpSet *)v->p : NULL;
    }
    AvpVal *v = avp_map_put(an->factory_tokens, f, &created);
    if (created) v->p = avp_set_new();
    return (AvpSet *)v->p;
}

static int cmp_str_p(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return avp_cmp_str(*(const AvpStr *)x, *(const AvpStr *)y);
}

/* ---- _names ---------------------------------------------------------------------------------------------------- */

/* re.match(r"^[A-Za-z_][A-Za-z0-9_:]*$", txt) ($ also before one final newline) */
static bool is_ident_text(AvpStr t) {
    size_t n = t.n;
    if (n && t.p[n - 1] == '\n') n--;
    if (n == 0) return false;
    char c = t.p[0];
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_')) return false;
    for (size_t i = 1; i < n; i++) {
        c = t.p[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == ':'))
            return false;
    }
    return true;
}

typedef struct {
    AvpStr name;
    i64 n;
} Count;
typedef AVP_VEC(Count) Counts;
static void count_add(Counts *c, AvpStr name, i64 n) {
    for (size_t i = 0; i < c->n; i++)
        if (eq(c->v[i].name, name)) {
            c->v[i].n += n;
            return;
        }
    Count e = {name, n};
    avp_vec_push(c, e);
}
static int cmp_count_desc(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return avp_cmp_i64(((const Count *)y)->n, ((const Count *)x)->n);
}

static const char *const k_order[5] = {"vt-symbol", "mangled-ctor", "mangled-dtor", "registry-string", "mangled-slot"};
static int order_index(AvpStr src) {
    for (int i = 0; i < 5; i++)
        if (eqz(src, k_order[i])) return i;
    return 5;
}
static int cmp_cand(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const ClassesNameCand *p = x, *q = y;
    int pu = startz(p->name, "unk_"), qu = startz(q->name, "unk_");
    if (pu != qu) return pu - qu;
    return order_index(p->source) - order_index(q->source);
}

/* "unk_%08X" % v */
static AvpStr unk_name(AvpArena *a, i64 v) {
    if (v < 0) return avp_str(avp_sprintf(a, "unk_-%07llX", (unsigned long long)(0 - (u64)v)));
    return avp_str(avp_sprintf(a, "unk_%08llX", (unsigned long long)v));
}

static ClassesRec *add_rec(ClassesAnalysis *an, AvpStr key, AvpStr name, AvpStr src, ClassesVtable *vt) {
    ClassesRec *c = AVP_NEW(an->a, ClassesRec);
    c->index = an->recs.n;
    c->key = key;
    c->name = name;
    c->name_source = src;
    c->vt = vt;
    c->member_idx = avp_map_new();
    c->fields.idx = avp_imap_new();
    bool created;
    AvpVal *v = avp_map_put(an->classes, key, &created);
    AVP_ASSERT(created);
    v->p = c;
    avp_vec_push(&an->recs, c);
    return c;
}

static void an_names(ClassesAnalysis *an) {
    AvpArena *a = an->a;
    ClassesProgram *prog = an->prog;
    an->factories = avp_map_new();
    an->factory_tokens = avp_map_new();
    an->registry = avp_map_new();
    an->classes = avp_map_new();
    for (size_t i = 0; i < an->sites.n; i++) {
        const ClassesSite *s = &an->sites.v[i];
        if (s->kind != CLASSES_K_FACTORY || CLASSES_NONE(s->owner)) continue;
        bool created;
        AvpVal *v = avp_map_put(an->factories, s->func, &created);
        if (created) {
            AvpStr *p = AVP_NEW(a, AvpStr);
            *p = s->owner;
            v->p = p;
        }
        if (eq(*(AvpStr *)v->p, s->owner)) avp_set_add(tokens_of(an, s->func, true), s->token);
    }
    AvpMapIter it = {0};
    while (avp_map_next(prog->info, &it)) {
        AvpStr fname = it.key;
        const ClassesInfo *inf = it.val->p;
        if (avp_map_has(an->factories, fname)) continue;
        /* the last constructor called on the new object is the most derived one */
        for (size_t k = 0; k < inf->calls.n; k++) {
            const ClassesCall *c = &inf->calls.v[k];
            const ClassesVal *a3 = &c->args[0];
            if (a3->kind == CLASSES_V_OBJ && classes_info_nw(inf, a3->name) && a3->v == 0) {
                AvpStr bvt = classes_an_vt_of_ctor(an, c->target, CLASSES_KIND_BIT(CLASSES_K_CTOR));
                if (!CLASSES_NONE(bvt)) {
                    bool created;
                    AvpVal *v = avp_map_put(an->factories, fname, &created);
                    AvpStr *p = AVP_NEW(a, AvpStr);
                    *p = bvt;
                    v->p = p;
                    AvpVal *t = avp_map_put(an->factory_tokens, fname, &created);
                    AvpSet *set = avp_set_new();
                    avp_set_add(set, a3->name);
                    t->p = set;
                }
            }
        }
    }
    /* registry: vt -> [(string, sinit, factory, size, target)] */
    it = (AvpMapIter){0};
    while (avp_map_next(prog->info, &it)) {
        AvpStr fname = it.key;
        const ClassesInfo *inf = it.val->p;
        for (size_t k = 0; k < inf->calls.n; k++) {
            const ClassesCall *c = &inf->calls.v[k];
            size_t nstr = 0, nfac = 0, nimm = 0;
            AvpStr str0 = {0}, fac0 = {0};
            i64 imm0 = 0;
            for (int r = 0; r < 8; r++) {
                const ClassesVal *v = &c->args[r];
                if (v->kind == CLASSES_V_SDA || v->kind == CLASSES_V_SYM) {
                    if (v->kind == CLASSES_V_SYM && v->v == 0 && avp_map_has(an->factories, v->name)) {
                        if (nfac++ == 0) fac0 = v->name;
                        continue;
                    }
                    AvpStr txt = classes_cstring(prog, v->name, v->kind == CLASSES_V_SYM ? v->v : 0);
                    if (truthy(txt) && is_ident_text(txt))
                        if (nstr++ == 0) str0 = txt;
                } else if (v->kind == CLASSES_V_IMM) {
                    if (nimm++ == 0) imm0 = v->v;
                }
            }
            if (nstr == 1 && nfac == 1) {
                AvpStr vt = *factory_of(an, fac0);
                bool created;
                AvpVal *rv = avp_map_put(an->registry, vt, &created);
                if (created) rv->p = AVP_NEW(a, ClassesRegList);
                ClassesRegEnt e = {str0, fname, fac0, nimm == 1, nimm == 1 ? imm0 : 0, c->target};
                avp_vec_push((ClassesRegList *)rv->p, e);
            }
        }
    }
    for (size_t i = 0; i < an->nvt; i++) {
        ClassesVtable *r = an->vts[i];
        AVP_VEC(ClassesNameCand) cands = {0};
        AvpStr c = classes_vt_symbol_class(a, r->name);
        if (truthy(c)) {
            ClassesNameCand e = {c, AVP_S("vt-symbol")};
            avp_vec_push(&cands, e);
        }
        for (int kind = CLASSES_K_CTOR; kind <= CLASSES_K_DTOR; kind++) {
            const AvpStrVec *l = &r->inst.k[kind];
            for (size_t k = 0; k < l->n; k++) {
                c = classes_mangled_class(a, l->v[k]);
                if (truthy(c)) {
                    ClassesNameCand e = {c, kind == CLASSES_K_CTOR ? AVP_S("mangled-ctor") : AVP_S("mangled-dtor")};
                    avp_vec_push(&cands, e);
                }
            }
        }
        Counts cnt = {0};
        for (size_t k = 0; k < r->slots.n; k++) {
            const ClassesSlot *ent = &r->slots.v[k];
            if ((eqz(ent->status, "new") || eqz(ent->status, "override")) && eqz(ent->group, "complete") &&
                truthy(ent->target)) {
                AvpStr t = ent->has_thunk ? ent->thunk_target : ent->target;
                c = classes_mangled_class(a, t);
                if (truthy(c)) count_add(&cnt, c, 1);
            }
        }
        if (cnt.n) {
            avp_sort(cnt.v, cnt.n, sizeof(Count), cmp_count_desc, NULL, false);
            ClassesNameCand e = {cnt.v[0].name, AVP_S("mangled-slot")};
            avp_vec_push(&cands, e);
            for (size_t k = 1; k < cnt.n; k++) {
                ClassesNameCand e2 = {cnt.v[k].name, AVP_S("mangled-slot (minority)")};
                avp_vec_push(&cands, e2);
            }
        }
        avp_vec_free(&cnt);
        AvpVal *rv = avp_map_get(an->registry, r->name);
        if (rv) {
            const ClassesRegList *l = rv->p;
            for (size_t k = 0; k < l->n; k++) {
                ClassesNameCand e = {l->v[k].string, AVP_S("registry-string")};
                avp_vec_push(&cands, e);
            }
        }
        avp_sort(cands.v, cands.n, sizeof(ClassesNameCand), cmp_cand, NULL, false);
        AvpStr name, src;
        if (cands.n) {
            name = cands.v[0].name;
            src = cands.v[0].source;
        } else {
            bool has_ctor = false;
            i64 ctor0 = 0;
            const AvpStrVec *l = &r->inst.k[CLASSES_K_CTOR];
            for (size_t k = 0; k < l->n; k++) {
                i64 v;
                if (classes_faddr(prog, l->v[k], &v) && (!has_ctor || v < ctor0)) {
                    ctor0 = v;
                    has_ctor = true;
                }
            }
            /* facs = sorted(prog.faddr(f) for f, v in factories.items() if v == vt): unused, but a None among two or
             * more addresses makes sorted() raise TypeError */
            size_t nfacs = 0;
            bool fac_none = false;
            AvpMapIter fi = {0};
            while (avp_map_next(an->factories, &fi)) {
                if (!eq(*(AvpStr *)fi.val->p, r->name)) continue;
                i64 v;
                nfacs++;
                if (!classes_faddr(prog, fi.key, &v)) fac_none = true;
            }
            if (fac_none && nfacs > 1)
                shsm_raise(SHSM_EXC_TYPE, "'<' not supported between instances of 'NoneType' and 'int'");
            if (has_ctor) {
                name = unk_name(a, ctor0);
                src = AVP_S("placeholder (constructor address)");
            } else {
                i64 va = 0;
                if (!classes_an_vt_addr(an, r->name, &va)) va = 0;
                name = unk_name(a, va);
                src = AVP_S("placeholder (vtable address)");
            }
        }
        ClassesRec *rec = add_rec(an, cat(a, "vt:", r->name), name, src, r);
        for (size_t k = 0; k < cands.n; k++) {
            bool seen = false;
            for (size_t j = 0; j < rec->cands.n && !seen; j++)
                seen = eq(rec->cands.v[j].name, cands.v[k].name) && eq(rec->cands.v[j].source, cands.v[k].source);
            if (!seen) avp_vec_push(&rec->cands, cands.v[k]);
        }
        avp_vec_free(&cands);
    }
}

/* ---- _mangled_only ---------------------------------------------------------------------------------------------- */

static void an_mangled_only(ClassesAnalysis *an) {
    AvpArena *a = an->a;
    AvpSet *have = avp_set_new();
    for (size_t i = 0; i < an->recs.n; i++) avp_set_add(have, an->recs.v[i]->name);
    AvpSet *names = avp_set_new();
    AvpMapIter it = {0};
    while (avp_map_next(an->prog->funcs, &it)) {
        if (startz(it.key, "@")) continue;
        AvpStr c = classes_mangled_class(a, it.key);
        if (truthy(c) && !avp_set_has(have, c)) avp_set_add(names, c);
    }
    AvpStrVec sorted = avp_set_sorted(names);
    for (size_t i = 0; i < sorted.n; i++) {
        AvpStr c = avp_str_dup(a, sorted.v[i]);
        ClassesRec *rec = add_rec(an, cat(a, "name:", c), c, AVP_S("mangled"), NULL);
        ClassesNameCand e = {c, AVP_S("mangled")};
        avp_vec_push(&rec->cands, e);
    }
    avp_vec_free(&sorted);
    avp_set_free(names);
    avp_set_free(have);
}

/* ---- _members_and_fields ---------------------------------------------------------------------------------------- */

static void member_add(ClassesRec *c, AvpStr f, const char *role) { /* members.setdefault(f, role) */
    bool created;
    AvpVal *v = avp_map_put(c->member_idx, f, &created);
    if (!created) return;
    v->u = c->members.n;
    ClassesMember m = {f, avp_str(role)};
    avp_vec_push(&c->members, m);
}

static ClassesField *field_get(ClassesFields *fs, i64 off) { /* fields.setdefault(off, {...}) */
    bool created;
    AvpVal *v = avp_imap_put(fs->idx, (u64)off, &created);
    if (!created) return fs->v.v[v->u];
    ClassesField *fl = avp_xcalloc(1, sizeof(ClassesField));
    fl->off = off;
    fl->funcs = avp_set_new();
    v->u = fs->v.n;
    avp_vec_push(&fs->v, fl);
    return fl;
}
static void use_add(ClassesField *fl, int width, AvpStr kind, i64 n) { /* Counter[(width, kind)] += n */
    for (size_t i = 0; i < fl->uses.n; i++)
        if (fl->uses.v[i].width == width && eq(fl->uses.v[i].kind, kind)) {
            fl->uses.v[i].n += n;
            return;
        }
    ClassesUse u = {width, kind, n};
    avp_vec_push(&fl->uses, u);
}

typedef struct {
    AvpStr name;
    i64 addr;
} AddrName;
static int cmp_addr(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return avp_cmp_i64(((const AddrName *)x)->addr, ((const AddrName *)y)->addr);
}

static void an_members_and_fields(ClassesAnalysis *an) {
    AvpArena *a = an->a;
    ClassesProgram *prog = an->prog;
    AvpMap *by_name = avp_map_new();
    for (size_t i = 0; i < an->recs.n; i++) {
        bool created;
        AvpVal *v = avp_map_put(by_name, an->recs.v[i]->name, &created);
        v->i = created ? 1 : v->i + 1;
    }
    AvpMap *mangled_members = avp_map_new();
    AvpMapIter it = {0};
    while (avp_map_next(prog->funcs, &it)) {
        if (startz(it.key, "@")) continue;
        AvpStr c = classes_mangled_class(a, it.key);
        if (!truthy(c)) continue;
        bool created;
        AvpVal *v = avp_map_put(mangled_members, c, &created);
        if (created) v->p = AVP_NEW(a, AvpStrVec);
        avp_vec_push((AvpStrVec *)v->p, it.key);
    }
    for (size_t i = 0; i < an->recs.n; i++) {
        ClassesRec *cls = an->recs.v[i];
        ClassesVtable *r = cls->vt;
        if (r) {
            for (size_t k = 0; k < r->slots.n; k++) {
                const ClassesSlot *ent = &r->slots.v[k];
                if ((eqz(ent->status, "new") || eqz(ent->status, "override")) && truthy(ent->target)) {
                    AvpStr t = ent->has_thunk ? ent->thunk_target : ent->target;
                    if (avp_map_has(prog->funcs, t)) member_add(cls, t, "slot");
                }
            }
            for (int kind = CLASSES_K_CTOR; kind <= CLASSES_K_DTOR; kind++)
                for (size_t k = 0; k < r->inst.k[kind].n; k++)
                    member_add(cls, r->inst.k[kind].v[k], kind == CLASSES_K_CTOR ? "ctor" : "dtor");
            /* a Python set of strings: insertion order here (DEVIATIONS.md) */
            if (r->dtor_funcs) {
                size_t pos = 0;
                AvpStr f;
                while (avp_set_next(r->dtor_funcs, &pos, &f)) member_add(cls, f, "dtor");
            }
        }
        if (avp_map_get(by_name, cls->name)->i == 1 || !r) {
            AvpVal *v = avp_map_get(mangled_members, cls->name);
            if (v) {
                const AvpStrVec *l = v->p;
                for (size_t k = 0; k < l->n; k++) member_add(cls, l->v[k], "mangled");
            }
        }
        if (r) {
            AvpMapIter fi = {0};
            while (avp_map_next(an->factories, &fi))
                if (eq(*(AvpStr *)fi.val->p, r->name)) member_add(cls, fi.key, "factory");
        }
        size_t n = cls->members.n;
        AddrName *order = avp_xmalloc((n ? n : 1) * sizeof(AddrName));
        for (size_t k = 0; k < n; k++) {
            order[k].name = cls->members.v[k].name;
            order[k].addr = faddr0(an, order[k].name);
        }
        avp_sort(order, n, sizeof(AddrName), cmp_addr, NULL, false);
        for (size_t k = 0; k < n; k++) {
            AvpStr f = order[k].name;
            const ClassesInfo *inf = classes_info(prog, f);
            if (!inf) shsm_raise(SHSM_EXC_KEY, "%.*s", AVP_SF(f));
            const ClassesMember *m = &cls->members.v[avp_map_get(cls->member_idx, f)->u];
            const AvpSet *toks = NULL;
            bool a3_only = true;
            AvpStr *fv = factory_of(an, f);
            if (eqz(m->role, "factory") || (fv && r && eq(*fv, r->name))) {
                toks = tokens_of(an, f, false);
                a3_only = false;
            }
            for (size_t x = 0; x < inf->access.n; x++) {
                const ClassesAccess *acc = &inf->access.v[x];
                if (a3_only ? !eqz(acc->tok, "a3") : (!toks || !avp_set_has(toks, acc->tok))) continue;
                ClassesField *fl = field_get(&cls->fields, acc->off);
                use_add(fl, acc->width, acc->kind, 1);
                avp_set_add(fl->funcs, f);
                if (eqz(acc->rw, "load")) fl->loads++;
                else fl->stores++;
                if (acc->sign == 1) fl->sign = true;
                if (acc->pointer) fl->pointer = true;
            }
        }
        avp_xfree(order);
    }
    avp_map_free(by_name);
}

/* ---- _finish ---------------------------------------------------------------------------------------------------- */

typedef struct {
    i64 n;
    AvpStr src;
} SizeEnt;
static void size_set(AvpArena *a, AvpMap *size, AvpStr vt, i64 n, AvpStr src) { /* size.setdefault(vt, (n, src)) */
    bool created;
    AvpVal *v = avp_map_put(size, vt, &created);
    if (!created) return;
    SizeEnt *e = AVP_NEW(a, SizeEnt);
    e->n = n;
    e->src = src;
    v->p = e;
}

/* _word_json(e) */
static AvpJson *word_json(ClassesAnalysis *an, const ClassesSlot *e) {
    AvpArena *a = an->a;
    AvpJson *o = avp_json_obj(a);
    put(a, o, "index", avp_json_int(a, e->index));
    put(a, o, "offset", jhx(a, true, e->index * 4));
    put(a, o, "target", jstr(a, e->target));
    if (e->has_thunk) {
        AvpJson *t = avp_json_obj(a);
        put(a, t, "adjust", avp_json_int(a, e->thunk_adjust));
        put(a, t, "target", jstr(a, e->thunk_target));
        put(a, o, "thunk", t);
    }
    put(a, o, "status", jstr(a, e->status));
    if (e->has_rtti) {
        put(a, o, "rtti", jstr(a, e->rtti));
        put(a, o, "rtti_offset", jhx(a, true, e->rtti_offset));
    }
    if (e->has_base) {
        AvpVal *k = avp_map_get(an->key_of_vt, e->base);
        put(a, o, "base", jstr(a, k ? ((ClassesRec *)k->p)->name : e->base));
        put(a, o, "base_target", jstr(a, e->base_target));
        put(a, o, "base_index", avp_json_int(a, e->base_index));
    }
    if (!CLASSES_NONE(e->group)) put(a, o, "group", jstr(a, e->group));
    if (!CLASSES_NONE(e->target)) put(a, o, "target_address", jfaddr(an, e->target));
    return o;
}

static int cmp_group(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return avp_cmp_i64(((const ClassesGroupEnt *)x)->a, ((const ClassesGroupEnt *)y)->a);
}
static int cmp_mo(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const ClassesMemberObj *p = x, *q = y;
    int c = avp_cmp_i64(p->off, q->off);
    return c ? c : avp_cmp_str(p->vt, q->vt);
}
static int cmp_mn(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const ClassesMemberNamed *p = x, *q = y;
    int c = avp_cmp_i64(p->off, q->off);
    if (c) return c;
    c = avp_cmp_str(p->cls, q->cls);
    return c ? c : avp_cmp_str(p->ev, q->ev);
}
typedef struct {
    i64 off;
    AvpJson *obj, *evidence;
} MoEnt;
static int cmp_moent(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return avp_cmp_i64(((const MoEnt *)x)->off, ((const MoEnt *)y)->off);
}

/* e with every "vt:X" replaced by the class name, X the first `vt:(\S+)` match, when "vt:X" is a class key */
static AvpStr evidence_named(ClassesAnalysis *an, AvpStr e) {
    for (size_t i = 0; i + 3 < e.n; i++) {
        if (memcmp(e.p + i, "vt:", 3) != 0) continue;
        size_t j = i + 3;
        while (j < e.n && shsm_ws_len(e, j) == 0) {
            j++;
            while (j < e.n && ((u8)e.p[j] & 0xC0) == 0x80) j++;
        }
        if (j == i + 3) continue; /* no \S after this "vt:": search on */
        AvpStr key = {e.p + i, j - i};
        ClassesRec *c = rec_of(an, key);
        if (!c) return e;
        return avp_str_replace(an->a, e, key, c->name, -1);
    }
    return e;
}

static void an_finish_vt(ClassesAnalysis *an, ClassesRec *cls, AvpMap *size) {
    AvpArena *a = an->a;
    ClassesProgram *prog = an->prog;
    ClassesVtable *r = cls->vt;
    AvpStr vt = r->name;
    i64 plus = avp_str_find(vt, AVP_S("+0x"), 0, AVP_END);
    AvpStr objname = plus >= 0 ? (AvpStr){vt.p, (size_t)plus} : vt;
    ClassesObj *obj = classes_obj(prog, objname);
    if (!obj) shsm_raise(SHSM_EXC_KEY, "%.*s", AVP_SF(objname));
    cls->unit = obj->unit;
    AvpJson *vtab = avp_json_obj(a);
    put(a, vtab, "symbol", jstr(a, vt));
    cls->has_vt_address = classes_an_vt_addr(an, vt, &cls->vt_address);
    put(a, vtab, "address", jhx(a, cls->has_vt_address, cls->vt_address));
    put(a, vtab, "size", jhx(a, true, (i64)r->words->n * 4));
    ClassesGroup g = {0};
    avp_vec_extend(&g, r->group.v, r->group.n);
    avp_sort(g.v, g.n, sizeof(ClassesGroupEnt), cmp_group, NULL, false);
    AvpJson *groups = avp_json_arr(a);
    for (size_t k = 0; k < g.n; k++) {
        AvpJson *o = avp_json_obj(a);
        put(a, o, "addend", jhx(a, true, g.v[k].a));
        put(a, o, "this_offset", jhx(a, g.v[k].has_o, g.v[k].o));
        avp_json_arr_push(a, groups, o);
    }
    put(a, vtab, "groups", groups);
    put(a, vtab, "group_evidence", jstr(a, r->group_ev));
    AvpJson *gc = avp_json_arr(a);
    for (size_t k = 0; k < r->conflicts.n; k++) avp_json_arr_push(a, gc, jstr(a, r->conflicts.v[k]));
    put(a, vtab, "group_conflicts", gc);
    AvpJson *words = avp_json_arr(a);
    for (size_t k = 0; k < r->slots.n; k++) avp_json_arr_push(a, words, word_json(an, &r->slots.v[k]));
    put(a, vtab, "words", words);
    cls->vtable = vtab;

    cls->ctors = avp_json_arr(a);
    for (size_t k = 0; k < r->inst.k[CLASSES_K_CTOR].n; k++)
        avp_json_arr_push(a, cls->ctors, fref(an, r->inst.k[CLASSES_K_CTOR].v[k]));
    AvpSet *ds = avp_set_new();
    for (size_t k = 0; k < r->inst.k[CLASSES_K_DTOR].n; k++) avp_set_add(ds, r->inst.k[CLASSES_K_DTOR].v[k]);
    if (r->dtor_funcs) {
        size_t pos = 0;
        AvpStr f;
        while (avp_set_next(r->dtor_funcs, &pos, &f)) avp_set_add(ds, f);
    }
    AvpStrVec dsorted = avp_set_sorted(ds);
    cls->dtors = avp_json_arr(a);
    for (size_t k = 0; k < dsorted.n; k++) avp_json_arr_push(a, cls->dtors, fref(an, avp_str_dup(a, dsorted.v[k])));
    avp_vec_free(&dsorted);
    avp_set_free(ds);
    AvpStrVec facs = {0};
    AvpMapIter fi = {0};
    while (avp_map_next(an->factories, &fi))
        if (eq(*(AvpStr *)fi.val->p, vt)) avp_vec_push(&facs, fi.key);
    avp_sort(facs.v, facs.n, sizeof(AvpStr), cmp_str_p, NULL, false);
    cls->factories = avp_json_arr(a);
    for (size_t k = 0; k < facs.n; k++) avp_json_arr_push(a, cls->factories, fref(an, facs.v[k]));
    avp_vec_free(&facs);
    cls->other_installers = avp_json_arr(a);
    for (size_t k = 0; k < r->inst.k[CLASSES_K_OTHER].n; k++)
        avp_json_arr_push(a, cls->other_installers, fref(an, r->inst.k[CLASSES_K_OTHER].v[k]));
    cls->registry = avp_json_arr(a);
    AvpVal *rv = avp_map_get(an->registry, vt);
    if (rv) {
        const ClassesRegList *l = rv->p;
        for (size_t k = 0; k < l->n; k++) {
            AvpJson *o = avp_json_obj(a);
            put(a, o, "string", jstr(a, l->v[k].string));
            put(a, o, "sinit", jstr(a, l->v[k].sinit));
            put(a, o, "factory", jstr(a, l->v[k].factory));
            put(a, o, "size", jhx(a, l->v[k].has_size, l->v[k].size));
            avp_json_arr_push(a, cls->registry, o);
        }
    }
    for (size_t k = 0; k < r->bases.n; k++) {
        const ClassesBase *b = r->bases.v[k];
        ClassesClsBase cb = {0};
        AvpVal *kv = avp_map_get(b->base_is_name ? an->key_of_name : an->key_of_vt, b->base);
        cb.rec = kv ? kv->p : NULL;
        cb.key = cb.rec ? cb.rec->key : (AvpStr){0};
        cb.cls = (cb.rec && !b->base_is_name) ? cb.rec->name : b->base;
        for (size_t x = 0; x < b->evidence.n; x++) avp_vec_push(&cb.evidence, evidence_named(an, b->evidence.v[x]));
        cb.has_offset = b->has_offset;
        cb.offset = b->offset;
        cb.has_word = b->has_word;
        cb.word = b->word;
        avp_vec_push(&cls->bases, cb);
    }
    AvpVal *sv = avp_map_get(size, vt);
    if (sv) {
        const SizeEnt *e = sv->p;
        cls->has_size = true;
        cls->size = e->n;
        cls->size_source = e->src;
    }
    for (size_t k = 0; k < r->group.n; k++) {
        if (!r->group.v[k].has_o) continue;
        bool seen = false;
        for (size_t x = 0; x < cls->vptrs.n && !seen; x++) seen = cls->vptrs.v[x] == r->group.v[k].o;
        if (!seen) avp_vec_push(&cls->vptrs, r->group.v[k].o);
    }
    avp_sort_i64(cls->vptrs.v, cls->vptrs.n);
    AVP_VEC(MoEnt) mo = {0};
    AvpIMap *mo_idx = avp_imap_new();
    AVP_VEC(ClassesMemberObj) mos = {0};
    avp_vec_extend(&mos, r->member_objects.v, r->member_objects.n);
    avp_sort(mos.v, mos.n, sizeof(ClassesMemberObj), cmp_mo, NULL, false);
    for (size_t k = 0; k < mos.n; k++) {
        bool created;
        AvpVal *v = avp_imap_put(mo_idx, (u64)mos.v[k].off, &created);
        if (created) {
            AvpVal *kv = avp_map_get(an->key_of_vt, mos.v[k].vt);
            MoEnt e = {mos.v[k].off, avp_json_obj(a), avp_json_arr(a)};
            put(a, e.obj, "offset", jhx(a, true, e.off));
            put(a, e.obj, "class", jstr(a, kv ? ((ClassesRec *)kv->p)->name : mos.v[k].vt));
            put(a, e.obj, "evidence", e.evidence);
            v->u = mo.n;
            avp_vec_push(&mo, e);
        }
        avp_json_arr_push(a, mo.v[v->u].evidence, jstr(a, cat(a, "installs vtable ", mos.v[k].vt)));
    }
    AVP_VEC(ClassesMemberNamed) mns = {0};
    avp_vec_extend(&mns, r->member_named.v, r->member_named.n);
    avp_sort(mns.v, mns.n, sizeof(ClassesMemberNamed), cmp_mn, NULL, false);
    for (size_t k = 0; k < mns.n; k++) {
        bool created;
        AvpVal *v = avp_imap_put(mo_idx, (u64)mns.v[k].off, &created);
        if (created) {
            MoEnt e = {mns.v[k].off, avp_json_obj(a), avp_json_arr(a)};
            put(a, e.obj, "offset", jhx(a, true, e.off));
            put(a, e.obj, "class", jstr(a, mns.v[k].cls));
            put(a, e.obj, "evidence", e.evidence);
            v->u = mo.n;
            avp_vec_push(&mo, e);
        }
        avp_json_arr_push(a, mo.v[v->u].evidence, jstr(a, mns.v[k].ev));
    }
    avp_sort(mo.v, mo.n, sizeof(MoEnt), cmp_moent, NULL, false);
    cls->member_objects = avp_json_arr(a);
    for (size_t k = 0; k < mo.n; k++) avp_json_arr_push(a, cls->member_objects, mo.v[k].obj);
    avp_vec_free(&mo);
    avp_vec_free(&mos);
    avp_vec_free(&mns);
    avp_vec_free(&g);
    avp_imap_free(mo_idx);
}

static void an_finish_plain(ClassesAnalysis *an, ClassesRec *cls) {
    AvpArena *a = an->a;
    cls->unit = (AvpStr){0};
    if (cls->members.n) {
        ClassesFunc *f = classes_func(an->prog, cls->members.v[0].name);
        if (!f) shsm_raise(SHSM_EXC_KEY, "%.*s", AVP_SF(cls->members.v[0].name));
        cls->unit = f->unit;
    }
    cls->ctors = avp_json_arr(a);
    cls->dtors = avp_json_arr(a);
    for (size_t k = 0; k < cls->members.n; k++) {
        AvpStr f = cls->members.v[k].name;
        if (startz(f, "__ct__")) avp_json_arr_push(a, cls->ctors, fref(an, f));
    }
    for (size_t k = 0; k < cls->members.n; k++) {
        AvpStr f = cls->members.v[k].name;
        if (startz(f, "__dt__")) avp_json_arr_push(a, cls->dtors, fref(an, f));
    }
    cls->factories = avp_json_arr(a);
    cls->other_installers = avp_json_arr(a);
    cls->registry = avp_json_arr(a);
    cls->member_objects = avp_json_arr(a);
}

/* ---- sizes, ranges, propagation, layout ------------------------------------------------------------------------- */

static i64 field_maxw(const ClassesField *fl) {
    i64 m = fl->uses.v[0].width;
    for (size_t i = 1; i < fl->uses.n; i++)
        if (fl->uses.v[i].width > m) m = fl->uses.v[i].width;
    return m;
}

static int depth_of(const ClassesRec *c, int n) { /* _depth */
    if (n > 32) return n;
    bool any = false;
    int best = 0;
    for (size_t i = 0; i < c->bases.n; i++) {
        if (!c->bases.v[i].rec) continue;
        int d = depth_of(c->bases.v[i].rec, n + 1);
        if (!any || d > best) best = d;
        any = true;
    }
    return any ? best : n;
}

typedef struct {
    i64 off, end;
    ClassesRec *bk;
    AvpStr name;
} Range;
typedef AVP_VEC(Range) Ranges;
static Ranges ranges_of(ClassesRec *c, int n);

static i64 est_size(ClassesRec *c, int n) { /* _est_size */
    if (c->has_size) return c->size;
    i64 hi = 0;
    for (size_t i = 0; i < c->fields.v.n; i++) {
        const ClassesField *fl = c->fields.v.v[i];
        i64 e = fl->off + field_maxw(fl);
        if (e > hi) hi = e;
    }
    for (size_t i = 0; i < c->vptrs.n; i++)
        if (c->vptrs.v[i] + 4 > hi) hi = c->vptrs.v[i] + 4;
    if (n < 32) {
        Ranges rs = ranges_of(c, n + 1);
        for (size_t i = 0; i < rs.n; i++)
            if (rs.v[i].end > hi) hi = rs.v[i].end;
        avp_vec_free(&rs);
    }
    return (hi + 3) & ~(i64)3;
}

typedef struct {
    const ClassesClsBase *b;
    i64 off;
} BaseOff;
static int cmp_baseoff(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return avp_cmp_i64(((const BaseOff *)x)->off, ((const BaseOff *)y)->off);
}

static Ranges ranges_of(ClassesRec *c, int n) { /* _ranges */
    Ranges out = {0};
    size_t nb = c->bases.n;
    if (!nb) return out;
    BaseOff *bl = avp_xmalloc(nb * sizeof(BaseOff));
    for (size_t i = 0; i < nb; i++) {
        bl[i].b = &c->bases.v[i];
        bl[i].off = c->bases.v[i].has_offset ? c->bases.v[i].offset : 0;
    }
    avp_sort(bl, nb, sizeof(BaseOff), cmp_baseoff, NULL, false);
    AvpI64Vec bounds = {0};
    for (size_t i = 0; i < nb; i++) avp_vec_push(&bounds, bl[i].off);
    avp_vec_extend(&bounds, c->vptrs.v, c->vptrs.n);
    avp_sort_i64(bounds.v, bounds.n);
    for (size_t i = 0; i < nb; i++) {
        const ClassesClsBase *b = bl[i].b;
        i64 off = bl[i].off;
        ClassesRec *bk = b->rec;
        bool has_bsz = false;
        i64 bsz = 0;
        bool exact = bk && bk->has_size;
        if (bk && n < 32) {
            bsz = est_size(bk, n + 1);
            has_bsz = true;
        }
        if (!exact) {
            for (size_t x = 0; x < bounds.n; x++) {
                if (bounds.v[x] <= off) continue;
                if (!has_bsz || bounds.v[x] > off + bsz) {
                    bsz = bounds.v[x] - off;
                    has_bsz = true;
                }
                break;
            }
        }
        if (has_bsz && bsz != 0) {
            Range rg = {off, off + bsz, bk, b->cls};
            avp_vec_push(&out, rg);
        }
    }
    avp_vec_free(&bounds);
    avp_xfree(bl);
    return out;
}

typedef struct {
    ClassesRec *c;
    int depth;
} DepthEnt;
static int cmp_depth_desc(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return ((const DepthEnt *)y)->depth - ((const DepthEnt *)x)->depth;
}

static void an_propagate(ClassesAnalysis *an) {
    size_t n = an->recs.n;
    i64 *est0 = avp_xmalloc((n ? n : 1) * sizeof(i64));
    for (size_t i = 0; i < n; i++) {
        ClassesRec *c = an->recs.v[i];
        if (c->has_size) {
            est0[i] = c->size;
            continue;
        }
        i64 hi = 0;
        for (size_t k = 0; k < c->fields.v.n; k++) {
            const ClassesField *fl = c->fields.v.v[k];
            if (fl->off + field_maxw(fl) > hi) hi = fl->off + field_maxw(fl);
        }
        for (size_t k = 0; k < c->vptrs.n; k++)
            if (c->vptrs.v[k] + 4 > hi) hi = c->vptrs.v[k] + 4;
        est0[i] = (hi + 3) & ~(i64)3;
    }
    DepthEnt *order = avp_xmalloc((n ? n : 1) * sizeof(DepthEnt));
    for (size_t i = 0; i < n; i++) {
        order[i].c = an->recs.v[i];
        order[i].depth = depth_of(an->recs.v[i], 0);
    }
    avp_sort(order, n, sizeof(DepthEnt), cmp_depth_desc, NULL, false);
    for (size_t i = 0; i < n; i++) {
        ClassesRec *cls = order[i].c;
        for (size_t k = 0; k < cls->bases.n; k++) {
            const ClassesClsBase *b = &cls->bases.v[k];
            ClassesRec *bk = b->rec;
            if (!bk || bk == cls) continue;
            i64 off = b->has_offset ? b->offset : 0;
            i64 end = off + est0[bk->index];
            size_t nf = cls->fields.v.n;
            for (size_t x = 0; x < nf; x++) {
                const ClassesField *fl = cls->fields.v.v[x];
                if (!(off <= fl->off && fl->off < end)) continue;
                ClassesField *t = field_get(&bk->fields, fl->off - off);
                for (size_t u = 0; u < fl->uses.n; u++) use_add(t, fl->uses.v[u].width, fl->uses.v[u].kind, fl->uses.v[u].n);
                size_t pos = 0;
                AvpStr f;
                while (avp_set_next(fl->funcs, &pos, &f)) avp_set_add(t->funcs, f);
                t->loads += fl->loads;
                t->stores += fl->stores;
                t->sign = t->sign || fl->sign;
                t->pointer = t->pointer || fl->pointer;
                if (!t->via) t->via = avp_set_new();
                avp_set_add(t->via, cls->name);
            }
        }
    }
    avp_xfree(order);
    avp_xfree(est0);
}

const char *classes_field_ctype(int width, AvpStr kind, const ClassesField *fl, AvpArena *a) {
    if (eqz(kind, "float")) return "f32";
    if (eqz(kind, "double")) return "f64";
    if (eqz(kind, "paired")) return width == 8 ? "f32[2]" : "f32";
    if (width == 1) return fl->sign ? "s8" : "u8";
    if (width == 2) return fl->sign ? "s16" : "u16";
    if (width == 4) return fl->pointer ? "void*" : "u32";
    return avp_sprintf(a, "u8[%d]", width);
}

static int cmp_use(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const ClassesUse *p = x, *q = y;
    int c = avp_cmp_i64(p->width, q->width);
    return c ? c : avp_cmp_str(p->kind, q->kind);
}
static int cmp_field_off(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return avp_cmp_i64((*(ClassesField *const *)x)->off, (*(ClassesField *const *)y)->off);
}

static void an_layout(ClassesAnalysis *an, ClassesRec *cls, int depth) {
    AvpArena *a = an->a;
    if (cls->done || depth > 32) return;
    cls->done = true;
    Ranges ranges = ranges_of(cls, 0);
    i64 base_end = 0;
    for (size_t i = 0; i < ranges.n; i++)
        if (ranges.v[i].end > base_end) base_end = ranges.v[i].end;
    for (size_t k = 0; k < cls->bases.n; k++) {
        ClassesClsBase *b = &cls->bases.v[k];
        if (b->rec && !b->rec->has_size)
            for (size_t i = 0; i < ranges.n; i++)
                if (ranges.v[i].bk == b->rec) {
                    b->has_size_est = true;
                    b->size_est = ranges.v[i].end - ranges.v[i].off;
                }
    }
    i64 hi = base_end;
    for (size_t k = 0; k < cls->vptrs.n; k++) {
        i64 v = cls->vptrs.v[k];
        if (v + 4 > hi) hi = v + 4;
        bool inh = false;
        for (size_t i = 0; i < ranges.n && !inh; i++) inh = ranges.v[i].off <= v && v < ranges.v[i].end;
        if (!inh) avp_vec_push(&cls->own_vptrs, v);
    }
    size_t nf = cls->fields.v.n;
    ClassesField **fs = avp_xmalloc((nf ? nf : 1) * sizeof(ClassesField *));
    if (nf) memcpy(fs, cls->fields.v.v, nf * sizeof(ClassesField *));
    avp_sort(fs, nf, sizeof(ClassesField *), cmp_field_off, NULL, false);
    AvpJson *out = avp_json_arr(a), *conflicts = avp_json_arr(a);
    i64 *ent_w = avp_xmalloc((nf ? nf : 1) * sizeof(i64));
    bool *ent_conf = avp_xmalloc((nf ? nf : 1) * sizeof(bool));
    for (size_t i = 0; i < nf; i++) {
        const ClassesField *fl = fs[i];
        i64 off = fl->off;
        size_t top = 0; /* most_common(1): the highest count, first inserted on ties */
        for (size_t u = 1; u < fl->uses.n; u++)
            if (fl->uses.v[u].n > fl->uses.v[top].n) top = u;
        int w = fl->uses.v[top].width;
        AvpStr kind = fl->uses.v[top].kind;
        AvpJson *ent = avp_json_obj(a);
        put(a, ent, "offset", jhx(a, true, off));
        put(a, ent, "width", avp_json_int(a, w));
        put(a, ent, "kind", jstr(a, kind));
        bool is_vptr = false;
        for (size_t k = 0; k < cls->vptrs.n && !is_vptr; k++) is_vptr = cls->vptrs.v[k] == off;
        put(a, ent, "type", jstrz(a, is_vptr && w == 4 ? "vptr" : classes_field_ctype(w, kind, fl, a)));
        put(a, ent, "loads", avp_json_int(a, fl->loads));
        put(a, ent, "stores", avp_json_int(a, fl->stores));
        put(a, ent, "functions", avp_json_int(a, (i64)avp_set_len(fl->funcs)));
        if (fl->via && avp_set_len(fl->via)) {
            AvpStrVec via = avp_set_sorted(fl->via);
            AvpJson *arr = avp_json_arr(a);
            for (size_t k = 0; k < via.n; k++) avp_json_arr_push(a, arr, avp_json_str(a, via.v[k]));
            avp_vec_free(&via);
            put(a, ent, "seen_in_derived", arr);
        }
        for (size_t r = 0; r < ranges.n; r++)
            if (ranges.v[r].off <= off && off < ranges.v[r].end) {
                put(a, ent, "inherited_from", jstr(a, ranges.v[r].name));
                break;
            }
        ent_conf[i] = false;
        if (fl->uses.n > 1) {
            ClassesUse *us = avp_xmalloc(fl->uses.n * sizeof(ClassesUse));
            memcpy(us, fl->uses.v, fl->uses.n * sizeof(ClassesUse));
            avp_sort(us, fl->uses.n, sizeof(ClassesUse), cmp_use, NULL, false);
            AvpJson *arr = avp_json_arr(a);
            for (size_t u = 0; u < fl->uses.n; u++)
                avp_json_arr_push(a, arr, jstrz(a, avp_sprintf(a, "%d-byte %.*s x%lld", us[u].width,
                                                                AVP_SF(us[u].kind), (long long)us[u].n)));
            avp_xfree(us);
            put(a, ent, "conflict", arr);
            avp_json_arr_push(a, conflicts, jhx(a, true, off));
            ent_conf[i] = true;
        }
        ent_w[i] = w;
        avp_json_arr_push(a, out, ent);
        if (off + field_maxw(fl) > hi) hi = off + field_maxw(fl);
    }
    /* overlaps */
    bool has_prev = false;
    i64 p0 = 0, p1 = 0;
    for (size_t i = 0; i < nf; i++) {
        i64 o = fs[i]->off;
        if (has_prev && o < p0 + p1 && !ent_conf[i]) {
            AvpJson *arr = avp_json_arr(a);
            avp_json_arr_push(a, arr, jstrz(a, avp_sprintf(a, "overlaps field at %.*s", AVP_SF(classes_hx(a, p0)))));
            put(a, avp_json_at(out, i), "conflict", arr);
            avp_json_arr_push(a, conflicts, jhx(a, true, o));
        }
        if (!has_prev || o + ent_w[i] > p0 + p1) {
            p0 = o;
            p1 = ent_w[i];
            has_prev = true;
        }
    }
    cls->fields_json = out;
    cls->field_conflicts = conflicts;
    cls->has_base_end = base_end != 0;
    cls->base_end = base_end;
    if (!cls->has_size) {
        cls->has_size_est = true;
        cls->size_est = (hi + 3) & ~(i64)3;
    }
    avp_xfree(ent_conf);
    avp_xfree(ent_w);
    avp_xfree(fs);
    avp_vec_free(&ranges);
}

static void an_finish(ClassesAnalysis *an) {
    AvpArena *a = an->a;
    ClassesProgram *prog = an->prog;
    an->key_of_vt = avp_map_new();
    an->key_of_name = avp_map_new();
    for (size_t i = 0; i < an->recs.n; i++) {
        ClassesRec *c = an->recs.v[i];
        bool created;
        if (c->vt) avp_map_put(an->key_of_vt, c->vt->name, &created)->p = c;
        AvpVal *v = avp_map_put(an->key_of_name, c->name, &created);
        if (created) v->p = c;
    }
    /* sizes */
    AvpMap *size = avp_map_new();
    for (size_t i = 0; i < an->sites.n; i++) {
        const ClassesSite *s = &an->sites.v[i];
        if (s->kind != CLASSES_K_FACTORY || CLASSES_NONE(s->owner)) continue;
        const ClassesInfo *inf = classes_info(prog, s->func);
        if (!inf) shsm_raise(SHSM_EXC_KEY, "%.*s", AVP_SF(s->func));
        const ClassesNew *nw = classes_info_nw(inf, s->token);
        if (nw) size_set(a, size, s->owner, nw->size, cat(a, "new in ", s->func));
    }
    AvpMapIter it = {0};
    while (avp_map_next(an->factories, &it)) {
        const ClassesInfo *inf = classes_info(prog, it.key);
        if (!inf) shsm_raise(SHSM_EXC_KEY, "%.*s", AVP_SF(it.key));
        for (size_t k = 0; k < inf->nw.n; k++)
            size_set(a, size, *(AvpStr *)it.val->p, inf->nw.v[k].size, cat(a, "new in ", it.key));
    }
    it = (AvpMapIter){0};
    while (avp_map_next(an->registry, &it)) {
        const ClassesRegList *l = it.val->p;
        for (size_t k = 0; k < l->n; k++)
            if (l->v[k].has_size) size_set(a, size, it.key, l->v[k].size, cat(a, "registry in ", l->v[k].sinit));
    }
    for (size_t i = 0; i < an->recs.n; i++) {
        ClassesRec *c = an->recs.v[i];
        if (c->vt) an_finish_vt(an, c, size);
        else an_finish_plain(an, c);
    }
    avp_map_free(size);
    /* derived classes' accesses inside a base's range, then base ranges and fields */
    an_propagate(an);
    for (size_t i = 0; i < an->recs.n; i++) an_layout(an, an->recs.v[i], 0);
    for (size_t i = 0; i < an->recs.n; i++) {
        ClassesRec *c = an->recs.v[i];
        for (size_t k = 0; k < c->bases.n; k++)
            if (c->bases.v[k].rec) avp_vec_push(&c->bases.v[k].rec->derived, c->key);
    }
}

void classes_analysis_part2(ClassesAnalysis *an) {
    an_names(an);
    an_mangled_only(an);
    an_members_and_fields(an);
    an_finish(an);
}

ClassesAnalysis *classes_analysis_new(AvpArena *a, ClassesProgram *prog, const ClassesRegions *regions) {
    return classes_analysis_new_rule(a, prog, regions, 0);
}

ClassesAnalysis *classes_analysis_new_rule(AvpArena *a, ClassesProgram *prog, const ClassesRegions *regions,
                                           int base_rule) {
    ClassesAnalysis *an = classes_analysis_part1_rule(a, prog, regions, base_rule);
    classes_analysis_part2(an);
    return an;
}

/* ---- stats and to_json ------------------------------------------------------------------------------------------ */

static AvpStr first_word(AvpStr s) { /* s.split(" ")[0] */
    for (size_t i = 0; i < s.n; i++)
        if (s.p[i] == ' ') return (AvpStr){s.p, i};
    return s;
}

static AvpJson *counts_json(AvpArena *a, const Counts *c) {
    AvpJson *o = avp_json_obj(a);
    for (size_t i = 0; i < c->n; i++) avp_json_set_ref(a, o, c->v[i].name, avp_json_int(a, c->v[i].n));
    return o;
}

AvpJson *classes_analysis_stats(ClassesAnalysis *an) {
    AvpArena *a = an->a;
    i64 installed = 0, mixed = 0, shape = 0, multi = 0;
    for (size_t i = 0; i < an->nvt; i++) {
        const ClassesVtable *r = an->vts[i];
        if (eqz(r->group_ev, "stores")) installed++;
        if (startz(r->group_ev, "stores (")) mixed++;
        if (startz(r->group_ev, "shape")) shape++;
        if (r->group.n > 1) multi++;
    }
    i64 nvtc = 0, named = 0, unk = 0, with_bases = 0, edges = 0, name_conflicts = 0, field_conf = 0, mobj = 0;
    Counts src = {0}, evid = {0};
    AvpMap *names = avp_map_new();
    for (size_t i = 0; i < an->recs.n; i++) {
        const ClassesRec *c = an->recs.v[i];
        bool created;
        AvpVal *nv = avp_map_put(names, c->name, &created);
        nv->i = created ? 1 : nv->i + 1;
        if (c->bases.n) with_bases++;
        edges += (i64)c->bases.n;
        for (size_t k = 0; k < c->bases.n; k++)
            for (size_t x = 0; x < c->bases.v[k].evidence.n; x++)
                count_add(&evid, first_word(c->bases.v[k].evidence.v[x]), 1);
        if (avp_json_len(c->field_conflicts)) field_conf++;
        mobj += (i64)avp_json_len(c->member_objects);
        if (!c->vt) continue;
        nvtc++;
        count_add(&src, first_word(c->name_source), 1);
        bool ph = startz(c->name_source, "placeholder") || startz(c->name, "unk_");
        if (ph) unk++;
        else named++;
        for (size_t k = 1; k < c->cands.n; k++)
            if (!eq(c->cands.v[k].name, c->cands.v[0].name)) {
                name_conflicts++;
                break;
            }
    }
    AvpJson *o = avp_json_obj(a);
    put(a, o, "vtables", avp_json_int(a, (i64)an->nvt));
    put(a, o, "vtables_installed", avp_json_int(a, installed));
    put(a, o, "vtables_mixed_site", avp_json_int(a, mixed));
    put(a, o, "vtables_shape_only", avp_json_int(a, shape));
    put(a, o, "multi_group_vtables", avp_json_int(a, multi));
    put(a, o, "classes", avp_json_int(a, (i64)an->recs.n));
    put(a, o, "classes_with_vtable", avp_json_int(a, nvtc));
    put(a, o, "classes_mangled_only", avp_json_int(a, (i64)an->recs.n - nvtc));
    put(a, o, "vtable_class_name_sources", counts_json(a, &src));
    put(a, o, "named_vtable_classes", avp_json_int(a, named));
    put(a, o, "unk_vtable_classes", avp_json_int(a, unk));
    put(a, o, "classes_with_bases", avp_json_int(a, with_bases));
    put(a, o, "base_edges", avp_json_int(a, edges));
    put(a, o, "base_evidence", counts_json(a, &evid));
    AvpStrVec coll = {0};
    AvpMapIter it = {0};
    while (avp_map_next(names, &it))
        if (it.val->i > 1) avp_vec_push(&coll, it.key);
    avp_sort_strs(coll.v, coll.n);
    AvpJson *ca = avp_json_arr(a);
    for (size_t i = 0; i < coll.n; i++) avp_json_arr_push(a, ca, avp_json_str(a, coll.v[i]));
    put(a, o, "name_collisions", ca);
    put(a, o, "name_conflicts", avp_json_int(a, name_conflicts));
    put(a, o, "classes_with_field_conflicts", avp_json_int(a, field_conf));
    put(a, o, "member_objects", avp_json_int(a, mobj));
    /* _regions: named/unk counts by where the vtable sits in .data */
    AvpJson *reg = avp_json_obj(a);
    for (size_t i = 0; i < an->recs.n; i++) {
        const ClassesRec *c = an->recs.v[i];
        if (!c->vt) continue;
        i64 addr = c->has_vt_address ? c->vt_address : 0;
        AvpStr lib = {0};
        for (size_t k = 0; k < an->data_regions.n; k++) {
            if (an->data_regions.v[k].start > addr) break;
            lib = an->data_regions.v[k].library;
        }
        AvpStr region = CLASSES_NONE(lib) ? (AvpStr){0} : classes_region_of_library(lib);
        if (CLASSES_NONE(region)) region = AVP_S("other");
        AvpJson *d = avp_json_get(reg, region);
        if (!d) {
            d = avp_json_obj(a);
            put(a, d, "named", avp_json_int(a, 0));
            put(a, d, "unk", avp_json_int(a, 0));
            avp_json_set_ref(a, reg, region, d);
        }
        const char *k = startz(c->name, "unk_") ? "unk" : "named";
        put(a, d, k, avp_json_int(a, avp_json_get_i64(d, k, 0) + 1));
    }
    put(a, o, "vtable_classes_by_region", reg);
    avp_vec_free(&src);
    avp_map_free(names);
    avp_vec_free(&evid);
    avp_vec_free(&coll);
    return o;
}

static int cmp_members(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return avp_cmp_i64(((const AddrName *)x)->addr, ((const AddrName *)y)->addr);
}

static AvpJson *class_json(ClassesAnalysis *an, ClassesRec *c) {
    AvpArena *a = an->a;
    AvpJson *o = avp_json_obj(a);
    put(a, o, "key", jstr(a, c->key));
    put(a, o, "name", jstr(a, c->name));
    put(a, o, "name_source", jstr(a, c->name_source));
    AvpJson *nc = avp_json_arr(a);
    for (size_t i = 0; i < c->cands.n; i++) {
        AvpJson *e = avp_json_obj(a);
        put(a, e, "name", jstr(a, c->cands.v[i].name));
        put(a, e, "source", jstr(a, c->cands.v[i].source));
        avp_json_arr_push(a, nc, e);
    }
    put(a, o, "name_candidates", nc);
    put(a, o, "vtable_symbol", c->vt ? jstr(a, c->vt->name) : avp_json_null());
    size_t n = c->members.n;
    AddrName *ms = avp_xmalloc((n ? n : 1) * sizeof(AddrName));
    for (size_t i = 0; i < n; i++) {
        ms[i].name = c->members.v[i].name;
        ms[i].addr = faddr0(an, ms[i].name);
    }
    avp_sort(ms, n, sizeof(AddrName), cmp_members, NULL, false);
    AvpJson *mem = avp_json_arr(a);
    for (size_t i = 0; i < n; i++) {
        AvpJson *e = avp_json_obj(a);
        put(a, e, "name", jstr(a, ms[i].name));
        put(a, e, "address", jfaddr(an, ms[i].name));
        put(a, e, "role", jstr(a, c->members.v[avp_map_get(c->member_idx, ms[i].name)->u].role));
        avp_json_arr_push(a, mem, e);
    }
    avp_xfree(ms);
    put(a, o, "members", mem);
    put(a, o, "unit", jstr(a, c->unit));
    put(a, o, "vtable", c->vtable ? c->vtable : avp_json_null());
    put(a, o, "ctors", c->ctors);
    put(a, o, "dtors", c->dtors);
    put(a, o, "factories", c->factories);
    put(a, o, "other_installers", c->other_installers);
    put(a, o, "registry", c->registry);
    AvpJson *bl = avp_json_arr(a);
    for (size_t i = 0; i < c->bases.n; i++) {
        const ClassesClsBase *b = &c->bases.v[i];
        AvpJson *e = avp_json_obj(a);
        put(a, e, "class", jstr(a, b->cls));
        put(a, e, "key", jstr(a, b->key));
        put(a, e, "offset", jhx(a, b->has_offset, b->offset));
        put(a, e, "vtable_word", b->has_word ? avp_json_int(a, b->word) : avp_json_null());
        put(a, e, "primary", avp_json_bool(b->has_word && b->word == 0));
        AvpJson *ev = avp_json_arr(a);
        for (size_t k = 0; k < b->evidence.n; k++) avp_json_arr_push(a, ev, jstr(a, b->evidence.v[k]));
        put(a, e, "evidence", ev);
        if (b->has_size_est) put(a, e, "size_estimate", jhx(a, true, b->size_est));
        avp_json_arr_push(a, bl, e);
    }
    put(a, o, "bases", bl);
    if (c->has_size) {
        put(a, o, "size", jhx(a, true, c->size));
        put(a, o, "size_source", jstr(a, c->size_source));
    }
    AvpJson *vp = avp_json_arr(a);
    for (size_t i = 0; i < c->vptrs.n; i++) avp_json_arr_push(a, vp, jhx(a, true, c->vptrs.v[i]));
    put(a, o, "vptrs", vp);
    put(a, o, "member_objects", c->member_objects);
    put(a, o, "fields", c->fields_json);
    put(a, o, "field_conflicts", c->field_conflicts);
    put(a, o, "base_end", jhx(a, c->has_base_end, c->base_end));
    AvpJson *ov = avp_json_arr(a);
    for (size_t i = 0; i < c->own_vptrs.n; i++) avp_json_arr_push(a, ov, jhx(a, true, c->own_vptrs.v[i]));
    put(a, o, "own_vptrs", ov);
    if (c->has_size_est) put(a, o, "size_estimate", avp_json_int(a, c->size_est));
    AvpJson *dv = avp_json_arr(a);
    for (size_t i = 0; i < c->derived.n; i++) avp_json_arr_push(a, dv, jstr(a, c->derived.v[i]));
    put(a, o, "derived", dv);
    return o;
}

static int cmp_class_order(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const ClassesRec *p = *(ClassesRec *const *)x, *q = *(ClassesRec *const *)y;
    int pn = p->vt == NULL, qn = q->vt == NULL;
    if (pn != qn) return pn - qn;
    i64 pa = p->vt && p->has_vt_address ? p->vt_address : 0, qa = q->vt && q->has_vt_address ? q->vt_address : 0;
    int c = avp_cmp_i64(pa, qa);
    return c ? c : avp_cmp_str(p->name, q->name);
}

AvpJson *classes_analysis_to_json(ClassesAnalysis *an, AvpJson *inputs) {
    AvpArena *a = an->a;
    AvpJson *methods = avp_json_obj(a);
    AvpMapIter it = {0};
    while (avp_map_next(an->prog->funcs, &it)) {
        const ClassesFunc *f = it.val->p;
        AvpStr mt = classes_method_text(f);
        if (truthy(mt)) avp_json_set_ref(a, methods, f->name, avp_json_str_ref(a, mt));
    }
    AvpJson *out = avp_json_obj(a);
    if (inputs && avp_json_truthy(inputs)) put(a, out, "inputs", inputs);
    put(a, out, "stats", classes_analysis_stats(an));
    put(a, out, "methods", methods);
    size_t n = an->recs.n;
    ClassesRec **order = avp_xmalloc((n ? n : 1) * sizeof(ClassesRec *));
    if (n) memcpy(order, an->recs.v, n * sizeof(ClassesRec *));
    avp_sort(order, n, sizeof(ClassesRec *), cmp_class_order, NULL, false);
    AvpJson *cl = avp_json_arr_cap(a, n);
    for (size_t i = 0; i < n; i++) avp_json_arr_push(a, cl, class_json(an, order[i]));
    avp_xfree(order);
    put(a, out, "classes", cl);
    return out;
}
