/* rules_vtables: R13 against classes.json, the Checker's slot check (check_rules.py L3580-4048): _src_classes,
 * _resolve_base, _virtual_names, _virtual_sigs, _peers, _pick_overridden, _model, _model_from, check_vtables, _r13,
 * _compare_subtables, _params_agree, _compare_vtable. */
#include "rules_check.h"
#include "rules_classes.h"

typedef RL_VEC(const RlSrcClass *) RlClassDefs;

/* ---- the source class index ---- */
static void src_classes(RlChecker *ck) {
    if (ck->src_classes) return;
    ck->src_classes = avp_map_new();
    AvpStrVec sorted = {0};
    for (size_t i = 0; i < ck->paths.n; i++) avp_vec_push(&sorted, ck->paths.v[i]);
    avp_sort_strs(sorted.v, sorted.n);
    for (size_t i = 0; i < sorted.n; i++) {
        RlParsed *P = rl_checker_parse(ck, sorted.v[i]);
        if (!P || !P->is_cpp) continue;
        const RlSrcClasses *cs = rl_cpp_classes(P);
        for (size_t k = 0; k < cs->n; k++) {
            bool created = false;
            AvpVal *v = avp_map_put(ck->src_classes, cs->v[k].name, &created);
            if (created) {
                v->p = AVP_NEW(ck->a, RlClassDefs);
                avp_vec_push(&ck->src_class_names, cs->v[k].name);
            }
            *RL_PUSH(ck->a, (RlClassDefs *)v->p) = &cs->v[k];
        }
    }
    avp_vec_free(&sorted);
    avp_sort_strs(ck->src_class_names.v, ck->src_class_names.n);
}

static const RlClassDefs *defs_of(RlChecker *ck, AvpStr name) {
    AvpVal *v = avp_map_get(ck->src_classes, name);
    return v ? v->p : NULL;
}

static const RlSrcClass *resolve_base(RlChecker *ck, AvpStr name, const RlSrcClass *cls) {
    src_classes(ck);
    AvpArena *a = ck->a;
    AvpStr tname = rl_template_name(name);
    AvpStrVec scope = avp_str_split(cls->name, AVP_S("::"), -1);
    scope.n--;
    const RlSrcClass *res = NULL;
    for (;;) {
        AvpStrVec q = {0};
        for (size_t i = 0; i < scope.n; i++) avp_vec_push(&q, scope.v[i]);
        avp_vec_push(&q, name);
        const RlClassDefs *d = defs_of(ck, avp_str_join(a, AVP_S("::"), q.v, q.n));
        if (d) {
            res = d->n == 1 ? d->v[0] : NULL;
            avp_vec_free(&q);
            break;
        }
        bool done = false;
        if (tname.p) {
            q.v[q.n - 1] = tname;
            d = defs_of(ck, avp_str_join(a, AVP_S("::"), q.v, q.n));
            if (d) {
                const RlSrcClass *one = NULL;
                size_t nt = 0;
                for (size_t i = 0; i < d->n; i++)
                    if (d->v[i]->template_) {
                        nt++;
                        if (!one) one = d->v[i];
                    }
                res = nt == 1 ? one : NULL;
                done = true;
            }
        }
        avp_vec_free(&q);
        if (done || !scope.n) break;
        scope.n--;
    }
    avp_vec_free(&scope);
    return res;
}

/* ---- virtual signatures ---- */
typedef struct {
    AvpStr name;
    RL_VEC(RlSig) sigs;
} SigEntry;
typedef RL_VEC(SigEntry) Sigs;
typedef struct {
    const AvpStr *v;
    size_t n;
} Stack;

static SigEntry *sigs_get(Sigs *s, AvpStr name) {
    for (size_t i = 0; i < s->n; i++)
        if (avp_str_eq(s->v[i].name, name)) return &s->v[i];
    return NULL;
}

static void sigs_extend(AvpArena *a, Sigs *s, AvpStr name, const RlSig *sig, size_t n) {
    SigEntry *e = sigs_get(s, name);
    if (!e) {
        e = RL_PUSH(a, s);
        e->name = name;
    }
    for (size_t i = 0; i < n; i++) {
        bool seen = false;
        for (size_t k = 0; k < e->sigs.n && !seen; k++) seen = rl_sig_eq(&e->sigs.v[k], &sig[i]);
        if (!seen) *RL_PUSH(a, &e->sigs) = sig[i];
    }
}

static bool in_stack(Stack st, AvpStr name) {
    for (size_t i = 0; i < st.n; i++)
        if (avp_str_eq(st.v[i], name)) return true;
    return false;
}

static Stack push(AvpArena *a, Stack st, AvpStr name) {
    AvpStr *v = AVP_NEWN(a, AvpStr, st.n + 1);
    for (size_t i = 0; i < st.n; i++) v[i] = st.v[i];
    v[st.n] = name;
    return (Stack){v, st.n + 1};
}

static i64 peers(const RlSrcClass *c, AvpStr name) {
    i64 n = 0;
    for (size_t i = 0; i < c->methods.n; i++) {
        const RlSrcMethod *m = &c->methods.v[i];
        if (avp_str_eq(m->name, name) && !m->ctor && !m->static_) n++;
    }
    return n;
}

/* _pick_overridden: the index, or -1 for None. */
static i64 pick_overridden(const RlSrcMethod *m, const RlSig *cands, size_t n, i64 npeers) {
    if (!n) return -1;
    if (avp_str_eqz(m->name, "~")) return 0;
    for (size_t k = 0; k < n; k++)
        if (rl_sig_eq(&cands[k], &m->sig)) return (i64)k;
    /* The only candidate on both sides: an override when the spellings differ only in type names (a typedef against
     * its target), never when the parameter counts differ: release(bool) hides release(), it does not override it. */
    if (n == 1 && npeers == 1 && cands[0].np == m->sig.np) return 0;
    return -1;
}

static Sigs virtual_sigs(RlChecker *ck, const RlSrcClass *c, Stack st) {
    Sigs sigs = {0};
    if (in_stack(st, c->name)) return sigs;
    AvpArena *a = ck->a;
    for (size_t b = 0; b < c->bases.n; b++) {
        const RlSrcClass *bc = resolve_base(ck, c->bases.v[b], c);
        if (!bc) continue;
        Sigs bs = virtual_sigs(ck, bc, push(a, st, c->name));
        for (size_t i = 0; i < bs.n; i++) sigs_extend(a, &sigs, bs.v[i].name, bs.v[i].sigs.v, bs.v[i].sigs.n);
    }
    /* mine: decided against the inherited signatures only, then merged in declaration order */
    RL_VEC(const RlSrcMethod *) mine = {0};
    for (size_t i = 0; i < c->methods.n; i++) {
        const RlSrcMethod *m = &c->methods.v[i];
        if (m->ctor || m->static_) continue;
        SigEntry *e = sigs_get(&sigs, m->name);
        if (m->virtual_ || pick_overridden(m, e ? e->sigs.v : NULL, e ? e->sigs.n : 0, peers(c, m->name)) >= 0)
            *RL_PUSH(a, &mine) = m;
    }
    /* mine is a dict of lists: names in first-seen order, each list in declaration order */
    for (size_t i = 0; i < mine.n; i++) {
        bool first = true;
        for (size_t k = 0; k < i && first; k++) first = !avp_str_eq(mine.v[k]->name, mine.v[i]->name);
        if (!first) continue;
        for (size_t k = i; k < mine.n; k++)
            if (avp_str_eq(mine.v[k]->name, mine.v[i]->name)) sigs_extend(a, &sigs, mine.v[k]->name, &mine.v[k]->sig, 1);
    }
    return sigs;
}

static bool has_virtuals(RlChecker *ck, const RlSrcClass *c) {
    return virtual_sigs(ck, c, (Stack){0}).n > 0;
}

static bool is_virtual_name(RlChecker *ck, const RlSrcClass *c, AvpStr name) {
    Sigs s = virtual_sigs(ck, c, (Stack){0});
    return sigs_get(&s, name) != NULL;
}

/* ---- the model ---- */
typedef struct {
    AvpStr name, owner;
    bool pure;
    i32 line;
    bool mine, implicit, new_;
    RlSig sig;
} Slot;
typedef struct {
    Slot *slots; /* NULL: failed (why) */
    size_t n;
    size_t prefix;
    AvpStr why;
} Model;

static Model fail(AvpStr why) {
    return (Model){NULL, 0, 0, why};
}

static Model model(RlChecker *ck, const RlSrcClass *c, Stack st);

static Model model_from(RlChecker *ck, const RlSrcClass *c, const RlSrcClass **bases, size_t nb, Stack st) {
    AvpArena *a = ck->a;
    Sigs inherited = {0};
    for (size_t b = 0; b < nb; b++) {
        Sigs bs = virtual_sigs(ck, bases[b], (Stack){0});
        for (size_t i = 0; i < bs.n; i++) sigs_extend(a, &inherited, bs.v[i].name, bs.v[i].sigs.v, bs.v[i].sigs.n);
    }
    RL_VEC(Slot) slots = {0};
    if (nb && has_virtuals(ck, bases[0])) {
        Model pm = model(ck, bases[0], push(a, st, c->name));
        if (!pm.slots && pm.why.p)
            return fail(rl_fmt(a, "primary base %.*s: %.*s", AVP_SF(bases[0]->name), AVP_SF(pm.why)));
        for (size_t i = 0; i < pm.n; i++) {
            Slot *s = RL_PUSH(a, &slots);
            *s = pm.slots[i];
            s->mine = false;
        }
    }
    size_t prefix = slots.n;
    bool *taken = AVP_NEWN(a, bool, prefix + 1);
    RL_VEC(const RlSrcMethod *) seen = {0};
    for (size_t i = 0; i < c->methods.n; i++) {
        const RlSrcMethod *m = &c->methods.v[i];
        if (m->ctor || m->static_) continue;
        i64 np = peers(c, m->name);
        SigEntry *be = sigs_get(&inherited, m->name);
        bool overrides = pick_overridden(m, be ? be->sigs.v : NULL, be ? be->sigs.n : 0, np) >= 0;
        if (!(m->virtual_ || overrides)) continue;
        for (size_t k = 0; k < seen.n; k++)
            if (avp_str_eq(seen.v[k]->name, m->name) && rl_sig_eq(&seen.v[k]->sig, &m->sig))
                return fail(rl_fmt(a, "virtual '%.*s' declared twice with one signature",
                                   AVP_SF(rl_class_display(a, c, m->name))));
        *RL_PUSH(a, &seen) = m;
        size_t *fr = AVP_NEWN(a, size_t, prefix + 1);
        RlSig *fs = AVP_NEWN(a, RlSig, prefix + 1);
        size_t nf = 0;
        for (size_t k = 0; k < prefix; k++)
            if (avp_str_eq(slots.v[k].name, m->name) && !taken[k]) {
                fr[nf] = k;
                fs[nf++] = slots.v[k].sig;
            }
        i64 k = pick_overridden(m, fs, nf, np);
        if (k >= 0) {
            Slot *s = &slots.v[fr[k]];
            taken[fr[k]] = true;
            s->owner = c->name;
            s->pure = m->pure;
            s->line = m->line;
            s->mine = true;
            s->implicit = false;
            s->sig = m->sig;
        } else {
            *RL_PUSH(a, &slots) = (Slot){m->name, c->name, m->pure, m->line, true, false, !overrides, m->sig};
        }
    }
    bool own_dtor = false;
    for (size_t k = 0; k < seen.n; k++)
        if (avp_str_eqz(seen.v[k]->name, "~")) own_dtor = true;
    if (sigs_get(&inherited, AVP_S("~")) && !own_dtor) {
        Slot *where = NULL;
        for (size_t k = 0; k < slots.n && !where; k++)
            if (avp_str_eqz(slots.v[k].name, "~")) where = &slots.v[k];
        if (where) {
            where->owner = c->name;
            where->pure = false;
            where->line = c->line;
            where->mine = true;
            where->implicit = true;
        } else {
            *RL_PUSH(a, &slots) = (Slot){AVP_S("~"), c->name, false, c->line, true, true, false, {NULL, 0, false}};
        }
    }
    Model r = {slots.v, slots.n, prefix, {0}};
    if (!r.slots) r.slots = AVP_NEWN(a, Slot, 1); /* an empty model is not a failure */
    return r;
}

static Model model(RlChecker *ck, const RlSrcClass *c, Stack st) {
    AvpArena *a = ck->a;
    if (!ck->r13_models) ck->r13_models = avp_map_new();
    AvpVal *v = avp_map_get(ck->r13_models, c->name);
    if (v) return *(Model *)v->p;
    if (in_stack(st, c->name)) return fail(AVP_S("the class derives from itself"));
    Model res = {0};
    if (c->virtual_bases) {
        res = fail(AVP_S("virtual base classes are not modelled"));
    } else {
        const RlSrcClass **bases = AVP_NEWN(a, const RlSrcClass *, c->bases.n + 1);
        bool ok = true;
        for (size_t b = 0; b < c->bases.n && ok; b++) {
            bases[b] = resolve_base(ck, c->bases.v[b], c);
            if (!bases[b]) {
                res = fail(rl_fmt(a, "base '%.*s' is not defined (once) in src/ or include/", AVP_SF(c->bases.v[b])));
                ok = false;
            }
        }
        if (ok) res = model_from(ck, c, bases, c->bases.n, st);
    }
    Model *m = AVP_NEW(a, Model);
    *m = res;
    avp_map_put(ck->r13_models, c->name, NULL)->p = m;
    return res;
}

/* ---- findings ---- */
enum { K_ADVISORY, K_ADDED, K_MISSING, K_OVERRIDE, K_ORDER };

static void r13(RlChecker *ck, RlParsed *P, i32 line, const char *tier, AvpStr msg, AvpStr func, int kind) {
    if (!strcmp(tier, "advisory")) kind = K_ADVISORY;
    switch (kind) {
    case K_ADDED: ck->r13.added++; break;
    case K_MISSING: ck->r13.missing++; break;
    case K_OVERRIDE: ck->r13.override_++; break;
    case K_ORDER: ck->r13.order++; break;
    default: ck->r13.advisory++; break;
    }
    rl_add(ck, P, line, "R13", tier, msg, func);
}

/* ---- JSON access ---- */
static const AvpJson *jget(const AvpJson *o, const char *k) {
    return o && o->kind == AVP_JSON_OBJECT ? avp_json_getz(o, k) : NULL;
}
static AvpStr jstr(const AvpJson *o, const char *k) {
    return rl_json_strv(jget(o, k));
}
static bool shape_only(AvpArena *a, const AvpJson *vt) {
    const AvpJson *ev = jget(vt, "group_evidence");
    AvpStr s = ev ? rl_py_str(a, ev) : AVP_S("");
    return !avp_str_startswith(s, AVP_S("stores"));
}
static bool jint(const AvpJson *v, i64 *out) {
    if (v && v->kind == AVP_JSON_INT && !v->big) {
        *out = v->u.i;
        return true;
    }
    return false;
}

/* ---- sub-tables ---- */
typedef struct {
    AvpMap *impls;   /* base virtual -> AvpSet* of implementations */
    AvpISet *consumed;
} SubResult;

static const AvpJson *word_at(AvpIMap *by_index, i64 k) {
    AvpVal *v = avp_imap_get(by_index, (u64)k);
    return v ? v->p : NULL;
}

static SubResult compare_subtables(RlChecker *ck, RlParsed *P, const RlSrcClass *c, const AvpJson *rc,
                                   RlRetailIndex *ri, AvpMap *explained) {
    AvpArena *a = ck->a;
    SubResult none = {avp_map_new(), avp_iset_new()};
    const AvpJson *vt = jget(rc, "vtable");
    const AvpJson *ownv = jget(rc, "own_vptrs");
    AvpISet *own = avp_iset_new();
    if (avp_json_truthy(ownv)) {
        for (size_t i = 0; i < avp_json_len(ownv); i++) {
            i64 x;
            if (!rl_int16(avp_json_at(ownv, i), &x, &ck->error)) return none;
            avp_iset_add(own, (u64)x);
        }
    }
    if (!avp_iset_len(own) || !c->bases.n) return none;
    const RlSrcClass **bases = AVP_NEWN(a, const RlSrcClass *, c->bases.n);
    for (size_t b = 0; b < c->bases.n; b++)
        if (!(bases[b] = resolve_base(ck, c->bases.v[b], c))) return none;
    bool has_primary = has_virtuals(ck, bases[0]);
    RL_VEC(const RlSrcClass *) secondary = {0};
    for (size_t b = has_primary ? 1 : 0; b < c->bases.n; b++)
        if (has_virtuals(ck, bases[b])) *RL_PUSH(a, &secondary) = bases[b];
    typedef struct {
        const AvpJson *g;
        i64 addend;
    } Group;
    RL_VEC(Group) groups = {0};
    const AvpJson *gl = jget(vt, "groups");
    for (size_t i = 0; i < avp_json_len(gl) && avp_json_is_arr(gl); i++) {
        const AvpJson *g = avp_json_at(gl, i);
        if (!jget(g, "this_offset")) {
            ck->error = "KeyError";
            return none;
        }
        i64 to;
        if (!rl_int16(jget(g, "this_offset"), &to, &ck->error)) return none;
        if (avp_iset_has(own, (u64)to)) *RL_PUSH(a, &groups) = (Group){g, 0};
    }
    for (size_t i = 0; i < groups.n; i++) {
        if (!jget(groups.v[i].g, "addend")) {
            ck->error = "KeyError";
            return none;
        }
        if (!rl_int16(jget(groups.v[i].g, "addend"), &groups.v[i].addend, &ck->error)) return none;
    }
    for (size_t i = 1; i < groups.n; i++) /* stable insertion sort by addend */
        for (size_t k = i; k > 0 && groups.v[k - 1].addend > groups.v[k].addend; k--) {
            Group t = groups.v[k];
            groups.v[k] = groups.v[k - 1];
            groups.v[k - 1] = t;
        }
    size_t g0 = 0;
    if (!has_primary && groups.n && groups.v[0].addend == 0) g0 = 1;
    if (!secondary.n || groups.n - g0 != secondary.n) return none;
    AvpIMap *by_index = avp_imap_new();
    const AvpJson *words = jget(vt, "words");
    for (size_t i = 0; i < avp_json_len(words) && avp_json_is_arr(words); i++) {
        const AvpJson *w = avp_json_at(words, i);
        i64 ix;
        if (jint(jget(w, "index"), &ix)) avp_imap_put(by_index, (u64)ix, NULL)->p = (void *)w;
    }
    bool shape = shape_only(a, vt);
    AvpStr vsym = rl_py_str(a, jget(vt, "symbol"));
    SubResult res = none;
    for (size_t s = 0; s < secondary.n; s++) {
        const RlSrcClass *bc = secondary.v[s];
        i64 at = groups.v[g0 + s].addend;
        AvpStr how;
        size_t ninst = 0;
        if (rl_retail_find(ri, bc, &how) || (bc->template_ && (rl_retail_instances(ri, bc, &ninst), ninst)))
            continue;
        Model bm = model(ck, bc, (Stack){0});
        if (!bm.slots) continue;
        i64 k = avp_floordiv_i64(at, 4);
        while (avp_str_eqz(jstr(word_at(by_index, k), "status"), "header")) k++;
        RL_VEC(const AvpJson *) sub = {0};
        const AvpJson *w;
        while ((w = word_at(by_index, k)) && !avp_str_eqz(jstr(w, "status"), "header") &&
               avp_str_eqz(jstr(w, "group"), "embedded")) {
            *RL_PUSH(a, &sub) = w;
            k++;
        }
        if (!sub.n) {
            while (sub.n < bm.n && (w = word_at(by_index, k)) && !avp_str_eqz(jstr(w, "status"), "header") &&
                   avp_str_eqz(jstr(w, "group"), "complete")) {
                *RL_PUSH(a, &sub) = w;
                avp_iset_add(res.consumed, (u64)k);
                k++;
            }
        }
        avp_map_put(explained, bc->name, NULL)->p = (void *)&c->name;
        AvpStr where = at < 0 ? rl_fmt(a, "%.*s (retail %.*s): secondary base %.*s, sub-table at +0x-%llX",
                                       AVP_SF(c->name), AVP_SF(vsym), AVP_SF(bc->name), (unsigned long long)(-at))
                              : rl_fmt(a, "%.*s (retail %.*s): secondary base %.*s, sub-table at +0x%llX",
                                       AVP_SF(c->name), AVP_SF(vsym), AVP_SF(bc->name), (unsigned long long)at);
        size_t lim = bm.n < sub.n ? bm.n : sub.n;
        for (size_t n = 0; n < lim; n++) {
            AvpStr impl = rl_slot_implementation(a, sub.v[n]);
            AvpStr want = bm.slots[n].name;
            if (avp_str_eqz(want, "~") || !impl.n) continue;
            bool created = false;
            AvpVal *v = avp_map_put(res.impls, want, &created);
            if (created) v->p = avp_set_new();
            avp_set_add(v->p, impl);
            AvpStr cls, nm;
            rl_demangle_member(a, impl, &cls, &nm);
            if (cls.n && !avp_str_eq(nm, want))
                r13(ck, P, c->line, "advisory",
                    rl_fmt(a, "%.*s: slot %.*s is %.*s in retail, %.*s in the source's base", AVP_SF(where),
                           AVP_SF(rl_py_str(a, jget(sub.v[n], "offset"))), AVP_SF(impl),
                           AVP_SF(rl_class_display(a, c, want))),
                    c->name, K_ADVISORY);
        }
        const char *tier = shape ? "advisory" : "match";
        for (size_t e = sub.n; e < bm.n; e++)
            r13(ck, P, c->line, tier,
                rl_fmt(a,
                       "%.*s declares %.*s, which the retail sub-table has no slot for: vtables never contain "
                       "added slots",
                       AVP_SF(where), AVP_SF(rl_class_display(a, c, bm.slots[e].name))),
                c->name, K_ADDED);
        for (size_t q = bm.n; q < sub.n; q++) {
            const AvpJson *th = jget(sub.v[q], "thunk");
            const AvpJson *tt = avp_json_truthy(th) ? jget(th, "target") : NULL;
            const AvpJson *show = avp_json_truthy(tt) ? tt : jget(sub.v[q], "target");
            r13(ck, P, c->line, tier,
                rl_fmt(a,
                       "%.*s: retail slot %.*s (%.*s) is not declared by the base: every slot of the binary's "
                       "vtable is declared",
                       AVP_SF(where), AVP_SF(rl_py_str(a, jget(sub.v[q], "offset"))), AVP_SF(rl_py_str(a, show))),
                c->name, K_MISSING);
        }
    }
    return res;
}

/* ---- the complete-object comparison ---- */
static bool params_agree(const Slot *e, const RlRetailSlot *r) {
    if (!r->params.p) return true;
    return avp_str_eqz(r->params, "v") == (e->sig.np == 0);
}

static const char *kind_name(int k) {
    static const char *const names[] = {"named", "unnamed", "null", "unresolved"};
    return names[k];
}

static AvpStr fn_name(AvpArena *a, const RlSrcClass *c, AvpStr name) {
    return rl_fmt(a, "%.*s::%.*s", AVP_SF(c->name), AVP_SF(rl_class_display(a, c, name)));
}

static void compare_vtable(RlChecker *ck, RlParsed *P, const RlSrcClass *c, const AvpJson *rc, const Model *md,
                           const SubResult *sub, AvpStr as_name) {
    AvpArena *a = ck->a;
    const AvpJson *vt = jget(rc, "vtable");
    AvpStr vsym = rl_py_str(a, jget(vt, "symbol"));
    AvpStr label = rl_fmt(a, "%.*s (retail %.*s, %.*s)", AVP_SF(c->name), AVP_SF(vsym),
                          AVP_SF(rl_py_str(a, jget(rc, "name"))));
    bool shape = shape_only(a, vt);
    const char *solid = shape ? "advisory" : NULL;
    size_t nall;
    RlRetailSlot *all = rl_retail_complete_slots(a, rc, &nall);
    RL_VEC(RlRetailSlot) retail = {0};
    for (size_t i = 0; i < nall; i++) {
        i64 ix;
        if (jint(all[i].index, &ix) && avp_iset_has(sub->consumed, (u64)ix)) continue;
        *RL_PUSH(a, &retail) = all[i];
    }
    AvpStr own_cls = as_name.n ? as_name : c->name;
    const Slot *slots = md->slots;
    size_t prefix = md->prefix;
    if (retail.n < prefix) {
        r13(ck, P, c->line, "advisory",
            rl_fmt(a,
                   "%.*s: the retail complete-object table has %zu slots, fewer than the %zu of the source's primary "
                   "base: slots not compared",
                   AVP_SF(label), retail.n, prefix),
            c->name, K_ADVISORY);
        return;
    }
#define DN(n) rl_class_display(a, c, (n))
#define PS(x) AVP_SF(rl_py_json_or_none(a, (x)))
    bool aligned = true;
    for (size_t k = 0; k < prefix; k++) {
        const Slot *e = &slots[k];
        const RlRetailSlot *r = &retail.v[k];
        if (r->kind == RL_SLOT_NAMED && !avp_str_eq(r->name, e->name)) {
            r13(ck, P, c->line, "advisory",
                rl_fmt(a, "%.*s: slot %.*s is %.*s in retail, %.*s in the source's primary base: the base part does "
                          "not line up",
                       AVP_SF(label), AVP_SF(rl_py_str(a, r->offset)), AVP_SF(r->target.p ? r->target : AVP_S("None")),
                       AVP_SF(DN(e->name))),
                c->name, K_ADVISORY);
            aligned = false;
            break;
        }
    }
    if (aligned) {
        for (size_t k = 0; k < prefix; k++) {
            const Slot *e = &slots[k];
            const RlRetailSlot *r = &retail.v[k];
            if (r->kind != RL_SLOT_NAMED || e->implicit) continue;
            AvpStr tg = r->target.p ? r->target : AVP_S("None");
            if (e->mine && !avp_str_eq(r->cls, own_cls) && !e->pure) {
                r13(ck, P, e->line, solid ? solid : "checked",
                    rl_fmt(a, "%.*s declares %.*s, but retail %.*s inherits it (slot %.*s is %.*s): an override the "
                              "binary does not have",
                           AVP_SF(c->name), AVP_SF(DN(e->name)), AVP_SF(c->name), AVP_SF(rl_py_str(a, r->offset)),
                           AVP_SF(tg)),
                    fn_name(a, c, e->name), K_OVERRIDE);
            } else if (!e->mine && avp_str_eq(r->cls, own_cls)) {
                r13(ck, P, c->line, solid ? solid : "checked",
                    rl_fmt(a, "retail %.*s overrides %.*s (slot %.*s is %.*s), but the class does not declare it",
                           AVP_SF(c->name), AVP_SF(DN(e->name)), AVP_SF(rl_py_str(a, r->offset)), AVP_SF(tg)),
                    fn_name(a, c, e->name), K_OVERRIDE);
            }
        }
    }
    const char *tier_match = solid ? solid : aligned ? "match" : "advisory";
    const char *tier_checked = solid ? solid : aligned ? "checked" : "advisory";
    const Slot *own_e = slots + prefix;
    size_t ne = md->n - prefix;
    const RlRetailSlot *own_r = retail.v + prefix;
    size_t nr = retail.n - prefix;
    bool *used = AVP_NEWN(a, bool, nr + 1);
    bool *paired = AVP_NEWN(a, bool, ne + 1);
    typedef struct {
        size_t ei, ri;
    } Pair;
    RL_VEC(Pair) pairs = {0};
    for (int strict = 1; strict >= 0; strict--) {
        for (size_t ei = 0; ei < ne; ei++) {
            if (paired[ei]) continue;
            for (size_t ri = 0; ri < nr; ri++) {
                const RlRetailSlot *r = &own_r[ri];
                if (used[ri] || (r->kind != RL_SLOT_NAMED && r->kind != RL_SLOT_UNNAMED) || !r->name.p ||
                    !avp_str_eq(r->name, own_e[ei].name))
                    continue;
                if (strict && !params_agree(&own_e[ei], r)) continue;
                used[ri] = true;
                paired[ei] = true;
                *RL_PUSH(a, &pairs) = (Pair){ei, ri};
                break;
            }
        }
    }
    {
        RL_VEC(size_t) pure_e = {0}, null_r = {0};
        for (size_t ei = 0; ei < ne; ei++)
            if (!paired[ei] && own_e[ei].pure) *RL_PUSH(a, &pure_e) = ei;
        for (size_t ri = 0; ri < nr; ri++)
            if (!used[ri] && own_r[ri].kind == RL_SLOT_NULL) *RL_PUSH(a, &null_r) = ri;
        if (pure_e.n && pure_e.n == null_r.n)
            for (size_t i = 0; i < pure_e.n; i++) {
                used[null_r.v[i]] = true;
                paired[pure_e.v[i]] = true;
                *RL_PUSH(a, &pairs) = (Pair){pure_e.v[i], null_r.v[i]};
            }
    }
    /* impls: the sub-table implementations plus the embedded groups' base targets */
    AvpMap *impls = avp_map_new();
    AvpMapIter it = {0};
    while (avp_map_next(sub->impls, &it)) {
        AvpSet *cp = avp_set_new();
        size_t pos = 0;
        AvpStr m;
        while (avp_set_next(it.val->p, &pos, &m)) avp_set_add(cp, m);
        avp_map_put(impls, it.key, NULL)->p = cp;
    }
    AvpSet *thunked = avp_set_new();
    const AvpJson *words = jget(vt, "words");
    for (size_t i = 0; i < avp_json_len(words) && avp_json_is_arr(words); i++) {
        const AvpJson *w = avp_json_at(words, i);
        const AvpJson *th = jget(w, "thunk");
        AvpStr tgt = avp_json_truthy(th) ? jstr(th, "target") : (AvpStr){0};
        if (avp_str_eqz(jstr(w, "group"), "embedded") && tgt.n) {
            avp_set_add(thunked, tgt);
            const AvpJson *bt = jget(w, "base_target");
            if (avp_json_truthy(bt)) {
                bool created = false;
                AvpVal *v = avp_map_put(impls, rl_json_strv(bt), &created);
                if (created) v->p = avp_set_new();
                avp_set_add(v->p, tgt);
            }
        }
    }
    bool *appended = AVP_NEWN(a, bool, nr + 1);
    size_t napp = 0;
    for (size_t ri = 0; ri < nr; ri++)
        if (!used[ri] && own_r[ri].target.p && avp_set_has(thunked, own_r[ri].target)) {
            appended[ri] = true;
            napp++;
        }
    for (size_t ei = 0; ei < ne; ei++) {
        const Slot *e = &own_e[ei];
        if (paired[ei] || e->new_ || e->pure || e->implicit) continue;
        AvpVal *iv = avp_map_get(impls, e->name);
        for (size_t ri = 0; ri < nr; ri++) {
            if (!appended[ri]) continue;
            if (iv && own_r[ri].target.p && avp_set_has(iv->p, own_r[ri].target)) {
                used[ri] = true;
                appended[ri] = false;
                napp--;
                paired[ei] = true;
                *RL_PUSH(a, &pairs) = (Pair){ei, ri};
                break;
            }
        }
    }
    {
        RL_VEC(size_t) ov_e = {0};
        for (size_t ei = 0; ei < ne; ei++)
            if (!paired[ei] && !own_e[ei].new_ && !own_e[ei].pure && !own_e[ei].implicit) *RL_PUSH(a, &ov_e) = ei;
        if (ov_e.n && ov_e.n == napp) {
            size_t q = 0;
            for (size_t ri = 0; ri < nr; ri++) {
                if (!appended[ri]) continue;
                size_t ei = ov_e.v[q++];
                used[ri] = true;
                paired[ei] = true;
                *RL_PUSH(a, &pairs) = (Pair){ei, ri};
                appended[ri] = false;
            }
            napp = 0;
        }
    }
    bool loose = false;
    for (size_t ri = 0; ri < nr; ri++)
        if (!used[ri] && own_r[ri].kind != RL_SLOT_NAMED) loose = true;
    for (size_t ei = 0; ei < ne; ei++) {
        if (paired[ei]) continue;
        const Slot *e = &own_e[ei];
        AvpStr what = DN(e->name);
        if (e->implicit) {
            r13(ck, P, e->line, "advisory",
                rl_fmt(a, "%.*s: the implicit destructor has no slot of its own in retail %.*s", AVP_SF(c->name),
                       AVP_SF(vsym)),
                fn_name(a, c, e->name), K_ADVISORY);
        } else if (loose) {
            r13(ck, P, e->line, "advisory",
                rl_fmt(a, "%.*s: %.*s cannot be placed: retail has unnamed, null or unresolved slots in the class's own "
                          "part",
                       AVP_SF(c->name), AVP_SF(what)),
                fn_name(a, c, e->name), K_ADVISORY);
        } else if (!e->new_) {
            r13(ck, P, e->line, tier_checked,
                rl_fmt(a, "%.*s declares %.*s, overriding a non-primary base, but retail %.*s has no slot for it: the "
                          "binary inherits it",
                       AVP_SF(c->name), AVP_SF(what), AVP_SF(vsym)),
                fn_name(a, c, e->name), K_OVERRIDE);
        } else {
            r13(ck, P, e->line, tier_match,
                rl_fmt(a, "%.*s declares the new virtual %.*s, which retail %.*s has no slot for: vtables never contain "
                          "added slots",
                       AVP_SF(c->name), AVP_SF(what), AVP_SF(vsym)),
                fn_name(a, c, e->name), K_ADDED);
        }
    }
    bool any_pure = false;
    for (size_t ei = 0; ei < ne; ei++)
        if (own_e[ei].pure) any_pure = true;
    for (size_t ri = 0; ri < nr; ri++) {
        if (used[ri]) continue;
        const RlRetailSlot *r = &own_r[ri];
        AvpStr off = rl_py_str(a, r->offset);
        AvpStr tg = r->target.p ? r->target : AVP_S("None");
        if (appended[ri]) {
            r13(ck, P, c->line, tier_match,
                rl_fmt(a, "retail %.*s has slot %.*s (%.*s), the override of a non-primary base's virtual that the "
                          "embedded groups call, but the class declares no such override: every slot of the binary's "
                          "vtable is declared",
                       AVP_SF(vsym), AVP_SF(off), AVP_SF(tg)),
                c->name, K_MISSING);
        } else if (r->kind == RL_SLOT_NAMED && avp_str_eq(r->cls, own_cls)) {
            bool declared = false;
            for (size_t i = 0; i < c->methods.n; i++)
                if (avp_str_eq(c->methods.v[i].name, r->name)) declared = true;
            if (declared && is_virtual_name(ck, c, r->name)) {
                r13(ck, P, c->line, tier_checked,
                    rl_fmt(a, "retail %.*s gives %.*s its own slot %.*s (%.*s), but the source declares it as an "
                              "override of the primary base",
                           AVP_SF(c->name), AVP_SF(DN(r->name)), AVP_SF(off), AVP_SF(tg)),
                    fn_name(a, c, r->name), K_OVERRIDE);
            } else if (declared) {
                r13(ck, P, c->line, tier_match,
                    rl_fmt(a, "retail %.*s has slot %.*s for %.*s (%.*s), which the class declares non-virtual: every "
                              "slot of the binary's vtable is declared",
                           AVP_SF(vsym), AVP_SF(off), AVP_SF(DN(r->name)), AVP_SF(tg)),
                    fn_name(a, c, r->name), K_MISSING);
            } else {
                r13(ck, P, c->line, tier_match,
                    rl_fmt(a, "retail %.*s has slot %.*s for %.*s (%.*s), which the class does not declare: every slot "
                              "of the binary's vtable is declared",
                           AVP_SF(vsym), AVP_SF(off), AVP_SF(DN(r->name)), AVP_SF(tg)),
                    fn_name(a, c, r->name), K_MISSING);
            }
        } else if (r->kind == RL_SLOT_NAMED) {
            r13(ck, P, c->line, "advisory",
                rl_fmt(a, "%.*s: own slot %.*s of retail %.*s is %.*s, a function of %.*s", AVP_SF(c->name),
                       AVP_SF(off), AVP_SF(vsym), AVP_SF(tg), AVP_SF(r->cls)),
                c->name, K_ADVISORY);
        } else if (!(r->kind == RL_SLOT_NULL && any_pure)) {
            AvpStr show = r->target.n ? r->target : avp_str(kind_name(r->kind));
            r13(ck, P, c->line, "advisory",
                rl_fmt(a, "%.*s: own slot %.*s of retail %.*s is %.*s and is not declared", AVP_SF(c->name), AVP_SF(off),
                       AVP_SF(vsym), AVP_SF(show)),
                c->name, K_ADVISORY);
        }
    }
    /* slot order is layout: pairs sorted by (ei, ri), implicit destructors left out */
    RL_VEC(Pair) order = {0};
    for (size_t i = 0; i < pairs.n; i++)
        if (!own_e[pairs.v[i].ei].implicit) *RL_PUSH(a, &order) = pairs.v[i];
    for (size_t i = 1; i < order.n; i++)
        for (size_t k = i; k > 0 && (order.v[k - 1].ei > order.v[k].ei ||
                                     (order.v[k - 1].ei == order.v[k].ei && order.v[k - 1].ri > order.v[k].ri));
             k--) {
            Pair t = order.v[k];
            order.v[k] = order.v[k - 1];
            order.v[k - 1] = t;
        }
    for (size_t k = 1; k < order.n; k++) {
        if (order.v[k].ri < order.v[k - 1].ri) {
            size_t ei = order.v[k].ei;
            RL_VEC(Pair) by_r = {0};
            for (size_t i = 0; i < order.n; i++) {
                size_t q = by_r.n;
                *RL_PUSH(a, &by_r) = order.v[i];
                while (q > 0 && by_r.v[q - 1].ri > by_r.v[q].ri) {
                    Pair t = by_r.v[q];
                    by_r.v[q] = by_r.v[q - 1];
                    by_r.v[q - 1] = t;
                    q--;
                }
            }
            AvpStr *want = AVP_NEWN(a, AvpStr, by_r.n + 1);
            for (size_t i = 0; i < by_r.n; i++) {
                AvpStr nm = own_r[by_r.v[i].ri].name;
                want[i] = DN(nm.n ? nm : AVP_S("null"));
            }
            r13(ck, P, own_e[ei].line, tier_match,
                rl_fmt(a, "%.*s declares %.*s out of retail slot order: retail %.*s lays out %.*s", AVP_SF(c->name),
                       AVP_SF(DN(own_e[ei].name)), AVP_SF(vsym),
                       AVP_SF(avp_str_join(a, AVP_S(", "), want, by_r.n))),
                fn_name(a, c, own_e[ei].name), K_ORDER);
            break;
        }
    }
#undef DN
#undef PS
}

/* ---- the driver ---- */
typedef struct {
    AvpStr name;
    RlParsed *P;
    const RlSrcClass *c;
    AvpStr how;
} Pending;

void rl_check_vtables(RlChecker *ck) {
    AvpArena *a = ck->a;
    memset(&ck->r13, 0, sizeof ck->r13);
    ck->r13.note = ck->classes_note;
    if (!ck->classes_data) {
        if (!ck->r13.note.n) ck->r13.note = AVP_S("R13 slot checks skipped: no classes.json given");
        return;
    }
    RlRetailIndex *ri = rl_retail_index(a, ck->classes_data, &ck->error);
    if (!ri) return;
    src_classes(ck);
    RL_VEC(Pending) pending = {0};
    AvpMap *explained = avp_map_new(); /* base name -> AvpStr* (the derived class) */
    for (size_t ni = 0; ni < ck->src_class_names.n && !ck->error; ni++) {
        AvpStr name = ck->src_class_names.v[ni];
        const RlClassDefs *defs = defs_of(ck, name);
        for (size_t di = 0; di < defs->n && !ck->error; di++) {
            const RlSrcClass *c = defs->v[di];
            if (!avp_set_has(ck->checked_set, c->path) || !has_virtuals(ck, c)) continue;
            ck->r13.classes++;
            RlParsed *P = rl_checker_parse(ck, c->path);
            if (defs->n > 1) {
                *RL_PUSH(a, &ck->r13.skipped) = name;
                r13(ck, P, c->line, "advisory",
                    rl_fmt(a, "class %.*s is defined %zu times: vtable slots not checked", AVP_SF(name), defs->n),
                    name, K_ADVISORY);
                continue;
            }
            const AvpJson **found;
            size_t nfound;
            AvpStr how1 = {0};
            if (c->template_) {
                found = rl_retail_instances(ri, c, &nfound);
                if (!nfound) {
                    *RL_PUSH(a, &pending) =
                        (Pending){name, P, c, AVP_S("no retail vtable of an instance of the class template")};
                    continue;
                }
                how1 = AVP_S("template instance");
            } else {
                const AvpJson *rc = rl_retail_find(ri, c, &how1);
                if (!rc) {
                    *RL_PUSH(a, &pending) = (Pending){name, P, c, how1};
                    continue;
                }
                found = AVP_NEWN(a, const AvpJson *, 1);
                found[0] = rc;
                nfound = 1;
            }
            Model md = model(ck, c, (Stack){0});
            if (!md.slots) {
                *RL_PUSH(a, &ck->r13.skipped) = name;
                r13(ck, P, c->line, "advisory",
                    rl_fmt(a, "class %.*s: %.*s; vtable slots not checked", AVP_SF(name), AVP_SF(md.why)), name,
                    K_ADVISORY);
                continue;
            }
            for (size_t f = 0; f < nfound && !ck->error; f++) {
                const AvpJson *rc = found[f];
                *RL_PUSH(a, &ck->r13.matched) =
                    (RlR13Match){name, jget(rc, "name"), jget(jget(rc, "vtable"), "symbol"), how1};
                SubResult sr = compare_subtables(ck, P, c, rc, ri, explained);
                if (ck->error) return;
                AvpStr as_name = c->template_ ? jstr(rc, "name") : (AvpStr){0};
                compare_vtable(ck, P, c, rc, &md, &sr, as_name);
            }
        }
    }
    if (ck->error) return;
    for (size_t i = 0; i < pending.n; i++) {
        Pending *p = &pending.v[i];
        AvpVal *v = avp_map_get(explained, p->name);
        if (v) {
            AvpStr d = *(AvpStr *)v->p;
            *RL_PUSH(a, &ck->r13.matched) =
                (RlR13Match){p->name, avp_json_str(a, d), NULL, rl_fmt(a, "sub-table of %.*s", AVP_SF(d))};
            continue;
        }
        *RL_PUSH(a, &ck->r13.unmatched) = p->name;
        r13(ck, p->P, p->c->line, "advisory",
            rl_fmt(a, "class %.*s: %.*s; vtable slots not checked", AVP_SF(p->name), AVP_SF(p->how)), p->name,
            K_ADVISORY);
    }
}
