/* draft.py's select (the draft queue: is_leaf, select), the queue by class (class_owners, class_label,
 * group_by_class, select_classes), the select cache shared with the Python (select_inputs, file_stamp, cached:
 * build/draft/select_cache.json), sweep_verdicts, sweep and the select and sweep commands (_unit_text, cmd_select,
 * cmd_sweep). */
#include "draft.h"
#include "classes.h"
#include "shsm_re.h"

#include <stdlib.h>
#include <string.h>

/* ---- small JSON helpers ------------------------------------------------------------------------------------------ */

static AvpJson *jstr_or_null(AvpArena *a, const char *s) { return s ? avp_json_strz(a, s) : avp_json_null(); }

/* str(value) of a JSON value as the Python prints it with %s: a string as is, None, ints, floats, True/False. */
static const char *pystr(AvpArena *a, const AvpJson *v) {
    if (!v || avp_json_is_null(v)) return "None";
    if (avp_json_is_str(v)) return avp_cz(a, avp_json_str_or(v, AVP_S("")));
    if (avp_json_is_int(v) || avp_json_is_num(v)) return avp_cz(a, avp_json_dumps(a, v, &(AvpJsonDumpOpts){0}));
    if (avp_json_truthy(v) && !avp_json_is_arr(v) && !avp_json_is_obj(v)) return "True";
    if (!avp_json_is_arr(v) && !avp_json_is_obj(v)) return "False";
    return avp_cz(a, avp_json_dumps(a, v, &(AvpJsonDumpOpts){0}));
}

static bool jstr_eq(const AvpJson *v, const char *s) {
    return v && avp_json_is_str(v) && avp_str_eqz(avp_json_str_or(v, AVP_S("")), s);
}

/* s[:n] counted in code points */
static const char *cut(AvpArena *a, const char *s, i64 n) { return avp_cz(a, avp_u8_slice(avp_str(s), 0, n)); }

/* the four unit_of keys select and select_classes add to a row */
static void put_unit_of(AvpArena *a, AvpJson *o, DraftUnitOf u) {
    avp_json_set(a, o, AVP_S("unit"), jstr_or_null(a, u.path));
    avp_json_set(a, o, AVP_S("unit_kind"), avp_json_strz(a, u.kind));
    avp_json_set(a, o, AVP_S("confidence"), u.has_conf ? avp_json_float(a, u.conf) : avp_json_null());
    avp_json_set(a, o, AVP_S("unit_range"),
                 u.has_range ? avp_json_str(a, avp_pyfmt(a, "%08X-%08X", u.start, u.end)) : avp_json_null());
}

/* ---- select ------------------------------------------------------------------------------------------------------ */

static bool is_leaf(DraftWorld *W, const TwinsFn *f) {
    bool leaf = false;
    AvpTry t;
    if (AVP_TRY(&t)) {
        static const char *const CALL[] = {"call"};
        ShsmRefs r = shsm_asmindex_refs_from(W->a, draft_world_ix(W), shsm_target_addr(f->address), CALL, 1);
        leaf = r.n == 0;
        avp_try_end(&t);
    }
    return leaf; /* the Python's `except Exception: return False` */
}

typedef struct {
    TwinsFn *f;
    int order;    /* twin 0, small 1, leaf 2, near 3 */
    bool cross;
    bool again;   /* entry["reject"] = "again" */
    TwinsFn *twin; /* ex[0], NULL when ex is empty */
} SelRow;

static const char *const KIND_NAMES[] = {"twin", "small", "leaf", "near"};

static int cmp_rows(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const SelRow *p = x, *q = y;
    if (p->order != q->order) return p->order < q->order ? -1 : 1;
    if (p->cross != q->cross) return p->cross ? 1 : -1;
    if (p->f->size != q->f->size) return p->f->size < q->f->size ? -1 : 1;
    if (p->f->address != q->f->address) return p->f->address < q->f->address ? -1 : 1;
    return 0;
}

static TwinsFnVec matched_sourced(AvpArena *a, const Twins *tw, const TwinsFn *f, int level) {
    TwinsFnVec all = twins_of(a, tw, f, level), out = {0};
    for (size_t i = 0; i < all.n; i++)
        if (all.v[i]->matched && all.v[i]->source && all.v[i]->source[0]) avp_vec_push(&out, all.v[i]);
    return out;
}

/* (f.lib or "").split("/")[0] */
static AvpStr lib_family(const char *lib) {
    AvpStr s = avp_str(lib ? lib : "");
    i64 i = avp_str_find(s, AVP_S("/"), 0, AVP_END);
    return i >= 0 ? avp_strn(s.p, (size_t)i) : s;
}

AvpJson *draft_select(DraftWorld *W, const char *kind, const char *lib, i64 limit, bool plan) {
    AvpArena *a = W->a;
    Twins *tw = draft_world_tw(W);
    DraftRejects rj_rows = draft_load_rejects(a, W->repo);
    AvpMap *rejects = avp_map_new(); /* symbol -> row (the last one wins) */
    for (size_t i = 0; i < rj_rows.n; i++) avp_map_put(rejects, avp_str(rj_rows.v[i].symbol), NULL)->p = &rj_rows.v[i];
    AVP_VEC(SelRow) rows = {0};
    for (size_t i = 0; i < tw->fns.n; i++) {
        TwinsFn *f = tw->fns.v[i];
        if (f->matched || (lib && lib[0] && !twins_lib_matches(a, f, lib))) continue;
        TwinsFnVec ex = {0};
        if (f->size >= TWINS_DEFAULT_MIN_SIZE) ex = matched_sourced(a, tw, f, TWINS_EXACT);
        int k = -1;
        if (ex.n)
            k = 0;
        else if (f->size <= DRAFT_SMALL_MAX)
            k = 1;
        else if (f->size <= DRAFT_LEAF_MAX && is_leaf(W, f))
            k = 2;
        else {
            TwinsFnVec near = matched_sourced(a, tw, f, TWINS_SHAPE);
            if (near.n) {
                k = 3;
                ex = near;
            }
        }
        if (k < 0 || (kind && kind[0] && strcmp(KIND_NAMES[k], kind) != 0)) continue;
        /* the twin shown is the one a twin draft prefers: a ref-level twin first */
        TwinsFn *shown = ex.n ? ex.v[0] : NULL;
        for (size_t j = 0; j < ex.n; j++)
            if (!strcmp(draft_twin_level(f, ex.v[j]), "ref")) {
                shown = ex.v[j];
                break;
            }
        SelRow r = {f, k, false, false, shown};
        if (ex.n) {
            AvpStr fam = lib_family(f->lib);
            r.cross = true;
            for (size_t j = 0; j < ex.n; j++)
                if (avp_str_eq(lib_family(ex.v[j]->lib), fam)) r.cross = false;
        }
        AvpVal *rv = avp_map_get(rejects, avp_str(f->name));
        if (rv) {
            const DraftReject *rj = rv->p;
            if (ex.n && rj->basis[0] && strcmp(rj->basis, draft_basis_of(a, W->repo, ex.v, ex.n)) != 0)
                r.again = true;
            else
                continue;
        }
        avp_vec_push(&rows, r);
    }
    avp_map_free(rejects);
    /* cheapest first; a twin from another library family is usually a coincidence of a few instructions */
    avp_sort(rows.v, rows.n, sizeof *rows.v, cmp_rows, NULL, false);
    size_t n = limit > 0 && (size_t)limit < rows.n ? (size_t)limit : rows.n;
    AvpJson *out = avp_json_arr(a);
    for (size_t i = 0; i < n; i++) {
        const SelRow *r = &rows.v[i];
        AvpJson *e = avp_json_obj(a);
        avp_json_set(a, e, AVP_S("symbol"), avp_json_strz(a, r->f->name));
        avp_json_set(a, e, AVP_S("address"), avp_json_str(a, avp_pyfmt(a, "0x%08X", r->f->address)));
        avp_json_set(a, e, AVP_S("size"), avp_json_int(a, r->f->size));
        avp_json_set(a, e, AVP_S("kind"), avp_json_strz(a, KIND_NAMES[r->order]));
        avp_json_set(a, e, AVP_S("lib"), jstr_or_null(a, r->f->lib));
        avp_json_set(a, e, AVP_S("twin"), r->twin ? avp_json_strz(a, r->twin->name) : avp_json_null());
        avp_json_set(a, e, AVP_S("twin_source"), r->twin ? jstr_or_null(a, r->twin->source) : avp_json_null());
        avp_json_set(a, e, AVP_S("twin_level"),
                     r->twin ? avp_json_strz(a, draft_twin_level(r->f, r->twin)) : avp_json_null());
        avp_json_set(a, e, AVP_S("cross"), avp_json_bool(r->cross));
        if (r->again) avp_json_set(a, e, AVP_S("reject"), avp_json_strz(a, "again"));
        if (plan) put_unit_of(a, e, draft_world_unit_of(W, r->f->address));
        avp_json_arr_push(a, out, e);
    }
    avp_vec_free(&rows);
    return out;
}

/* ---- select by class --------------------------------------------------------------------------------------------- */

static const char *const OWN_ROLES[] = {"ctor", "dtor", "factory", "mangled", "sinit", NULL};

/* int(v, 16) for a string, else the value (an int): false when neither */
static bool addr_value(const AvpJson *v, i64 *out) {
    if (avp_json_is_int(v)) {
        *out = avp_json_i64(v, 0);
        return true;
    }
    if (!avp_json_is_str(v)) return false;
    AvpStr s = avp_str_strip(avp_json_str_or(v, AVP_S("")));
    bool neg = false;
    if (s.n && (s.p[0] == '-' || s.p[0] == '+')) {
        neg = s.p[0] == '-';
        s = avp_str_slice(s, 1, AVP_END);
    }
    if (s.n > 2 && s.p[0] == '0' && (s.p[1] == 'x' || s.p[1] == 'X')) s = avp_str_slice(s, 2, AVP_END);
    if (!s.n) return false;
    i64 x = 0;
    for (size_t i = 0; i < s.n; i++) {
        char c = s.p[i];
        int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0) {
            if (c == '_' && i > 0 && i + 1 < s.n) continue;
            return false;
        }
        x = x * 16 + d;
    }
    *out = neg ? -x : x;
    return true;
}

/* _addr_name: the address of an fn_/lbl_ placeholder name */
static bool addr_name(void *ctx, AvpStr name, i64 *addr) {
    (void)ctx;
    return draft_addr_name(name, addr);
}

typedef struct {
    const AvpJson *c;
    AVP_VEC(i64) addrs;      /* _fns: insertion order */
    AVP_VEC(const char *) roles; /* role per address (NULL: None) */
    AvpIMap *index;          /* address -> position + 1 */
} OwnClass;

static void fns_set(OwnClass *k, i64 addr, const char *role, bool setdefault) {
    bool created;
    AvpVal *v = avp_imap_put(k->index, (u64)addr, &created);
    if (created) {
        v->i = (i64)k->addrs.n + 1;
        avp_vec_push(&k->addrs, addr);
        avp_vec_push(&k->roles, role);
    } else if (!setdefault) {
        k->roles.v[v->i - 1] = role;
    }
}

static bool own_role(const char *role) {
    for (int i = 0; role && OWN_ROLES[i]; i++)
        if (!strcmp(role, OWN_ROLES[i])) return true;
    return false;
}

/* class_owners: {function address: class index}; each class's addrs/roles reduced to the functions it owns */
static AvpIMap *class_owners(AvpArena *a, OwnClass *cls, size_t n, DraftAddrOf addr_of, void *addr_ctx) {
    AvpIMap *best_d = avp_imap_new(), *owners = avp_imap_new();
    for (size_t ci = 0; ci < n; ci++) {
        OwnClass *k = &cls[ci];
        k->index = avp_imap_new();
        AvpJson *members = avp_json_getz(k->c, "members");
        for (size_t i = 0; avp_json_is_arr(members) && i < avp_json_len(members); i++) {
            AvpJson *m = avp_json_at(members, i);
            i64 ad;
            if (!addr_value(avp_json_getz(m, "address"), &ad)) continue;
            AvpJson *role = avp_json_getz(m, "role");
            fns_set(k, ad, avp_json_is_str(role) ? avp_cz(a, avp_json_str_or(role, AVP_S(""))) : NULL, false);
        }
        AvpJson *registry = avp_json_getz(k->c, "registry");
        for (size_t i = 0; avp_json_is_arr(registry) && i < avp_json_len(registry); i++) {
            AvpJson *s = avp_json_getz(avp_json_at(registry, i), "sinit");
            i64 ad;
            if (avp_json_truthy(s) && avp_json_is_str(s) && addr_of(addr_ctx, avp_json_str_or(s, AVP_S("")), &ad))
                fns_set(k, ad, "sinit", true);
        }
        for (size_t i = 0; i < k->addrs.n; i++) {
            i64 x = k->addrs.v[i], d = (i64)1 << 32;
            bool any = false;
            for (size_t j = 0; j < k->addrs.n; j++)
                if (own_role(k->roles.v[j])) {
                    i64 dd = llabs(x - k->addrs.v[j]);
                    if (!any || dd < d) d = dd;
                    any = true;
                }
            bool created;
            AvpVal *bd = avp_imap_put(best_d, (u64)x, &created);
            if (created || d < bd->i) { /* min over (d, claim order): the first class wins a tie */
                bd->i = d;
                avp_imap_put(owners, (u64)x, NULL)->i = (i64)ci;
            }
        }
    }
    avp_imap_free(best_d);
    for (size_t ci = 0; ci < n; ci++) {
        OwnClass *k = &cls[ci];
        size_t w = 0;
        for (size_t i = 0; i < k->addrs.n; i++)
            if (avp_imap_get(owners, (u64)k->addrs.v[i])->i == (i64)ci) {
                k->addrs.v[w] = k->addrs.v[i];
                k->roles.v[w] = k->roles.v[i];
                w++;
            }
        k->addrs.n = k->roles.n = w;
    }
    return owners;
}

/* class_label(c, interned): (name, how) as JSON values */
static void class_label(AvpArena *a, const OwnClass *k, const AvpIMap *interned, AvpJson **name, AvpJson **how) {
    const AvpJson *c = k->c;
    AvpJson *ns = avp_json_getz(c, "name_source");
    const char *nss = ns ? pystr(a, ns) : "";
    if (strncmp(nss, "placeholder", 11) != 0) {
        *name = avp_json_get_or(c, AVP_S("name"), avp_json_null());
        *how = avp_json_truthy(ns) ? ns : avp_json_strz(a, "classes.json");
        return;
    }
    AVP_VEC(i64) sorted = {0};
    for (size_t i = 0; i < k->addrs.n; i++) avp_vec_push(&sorted, k->addrs.v[i]);
    avp_sort_i64(sorted.v, sorted.n);
    for (size_t i = 0; interned && i < sorted.n; i++) {
        AvpVal *v = avp_imap_get(interned, (u64)sorted.v[i]);
        if (v) {
            *name = avp_json_strz(a, v->p);
            *how = avp_json_strz(a, "interned name");
            avp_vec_free(&sorted);
            return;
        }
    }
    avp_vec_free(&sorted);
    AvpJson *vt = avp_json_getz(c, "vtable_symbol");
    if (!avp_json_truthy(vt)) {
        AvpJson *vtab = avp_json_getz(c, "vtable");
        vt = avp_json_truthy(vtab) ? avp_json_getz(vtab, "symbol") : NULL;
    }
    if (!avp_json_truthy(vt)) vt = avp_json_getz(c, "name");
    if (!avp_json_truthy(vt)) vt = avp_json_getz(c, "key");
    *name = vt ? vt : avp_json_null();
    *how = avp_json_strz(a, "vtable symbol");
}

typedef struct {
    AvpJson *cls, *how, *vtable, *object_size;
    AvpISet *fns;
    i64 queued, sweep_match;
    AvpJson *symbols;
    i64 functions, code_size, first;
    double coverage;
    bool none;
} Group;

static int cmp_groups(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const Group *p = *(Group *const *)x, *q = *(Group *const *)y;
    if (p->none != q->none) return p->none ? 1 : -1;
    if (p->coverage != q->coverage) return p->coverage < q->coverage ? -1 : 1;
    if (p->queued != q->queued) return p->queued > q->queued ? -1 : 1;
    if (p->code_size != q->code_size) return p->code_size > q->code_size ? -1 : 1;
    if (p->first != q->first) return p->first < q->first ? -1 : 1;
    return 0;
}

static bool fn_matched(const AvpIMap *fn_info, i64 addr) {
    AvpVal *v = fn_info ? avp_imap_get(fn_info, (u64)addr) : NULL;
    return v && ((const TwinsFn *)v->p)->matched;
}

static i64 fn_size(const AvpIMap *fn_info, i64 addr) {
    AvpVal *v = fn_info ? avp_imap_get(fn_info, (u64)addr) : NULL;
    return v ? ((const TwinsFn *)v->p)->size : 0;
}

AvpJson *draft_group_by_class(AvpArena *a, const AvpJson *entries, const AvpJson *classes, const AvpIMap *interned,
                              const AvpMap *sweep, const AvpIMap *fn_info, DraftAddrOf addr_of, void *addr_ctx) {
    if (!addr_of) addr_of = addr_name;
    size_t nc = avp_json_is_arr(classes) ? avp_json_len(classes) : 0;
    OwnClass *cls = AVP_NEWN(a, OwnClass, nc ? nc : 1);
    for (size_t i = 0; i < nc; i++) cls[i].c = avp_json_at(classes, i);
    AvpIMap *owners = class_owners(a, cls, nc, addr_of, addr_ctx);
    AVP_VEC(i64) owned = {0};
    AvpIMapIter it = {0};
    while (avp_imap_next(owners, &it)) avp_vec_push(&owned, (i64)it.key);
    avp_sort_i64(owned.v, owned.n);
    AvpMap *index = avp_map_new(); /* group key -> Group * */
    AVP_VEC(Group *) groups = {0};
    for (size_t i = 0; avp_json_is_arr(entries) && i < avp_json_len(entries); i++) {
        AvpJson *e = avp_json_at(entries, i);
        i64 ad = 0;
        addr_value(avp_json_getz(e, "address"), &ad);
        const char *sym = pystr(a, avp_json_getz(e, "symbol"));
        AvpVal *ov = avp_imap_get(owners, (u64)ad);
        i64 ci = ov ? ov->i : -1;
        if (ci < 0) {
            size_t j = avp_bisect_left_i64(owned.v, owned.n, ad);
            if (j >= 1 && ad - owned.v[j - 1] <= DRAFT_ADJACENT) ci = avp_imap_get(owners, (u64)owned.v[j - 1])->i;
        }
        const char *key = ci >= 0 ? pystr(a, avp_json_getz(cls[ci].c, "key")) : avp_sprintf(a, "fn:%s", sym);
        bool created;
        AvpVal *gv = avp_map_put(index, avp_str(key), &created);
        Group *g;
        if (created) {
            g = AVP_NEW(a, Group);
            g->fns = avp_iset_new();
            if (ci >= 0) {
                const OwnClass *k = &cls[ci];
                class_label(a, k, interned, &g->cls, &g->how);
                g->vtable = avp_json_get_or(k->c, AVP_S("vtable_symbol"), avp_json_null());
                g->object_size = avp_json_get_or(k->c, AVP_S("size"), avp_json_null());
                for (size_t x = 0; x < k->addrs.n; x++)
                    if (!fn_matched(fn_info, k->addrs.v[x])) avp_iset_add(g->fns, (u64)k->addrs.v[x]);
            } else {
                g->cls = avp_json_str(a, avp_pyfmt(a, "(no class) %s", sym));
                g->how = avp_json_strz(a, "none");
                g->vtable = g->object_size = avp_json_null();
            }
            g->symbols = avp_json_arr(a);
            gv->p = g;
            avp_vec_push(&groups, g);
        }
        g = gv->p;
        avp_iset_add(g->fns, (u64)ad);
        g->queued++;
        avp_json_arr_push(a, g->symbols, avp_json_strz(a, sym));
        if (sweep && avp_map_len(sweep)) {
            AvpVal *sv = avp_map_get(sweep, avp_str(sym));
            if (sv && jstr_eq(sv->p, "MATCH")) g->sweep_match++;
        }
    }
    for (size_t i = 0; i < groups.n; i++) {
        Group *g = groups.v[i];
        g->functions = (i64)avp_iset_len(g->fns);
        size_t pos = 0;
        u64 m;
        bool first = true;
        while (avp_iset_next(g->fns, &pos, &m)) {
            g->code_size += fn_size(fn_info, (i64)m);
            if (first || (i64)m < g->first) g->first = (i64)m;
            first = false;
        }
        g->none = jstr_eq(g->how, "none");
        g->coverage = -(double)g->queued / (double)(g->functions > 1 ? g->functions : 1);
    }
    /* classes first (a classless function is not a unit to choose), each by coverage */
    avp_sort(groups.v, groups.n, sizeof *groups.v, cmp_groups, NULL, false);
    AvpJson *out = avp_json_arr(a);
    for (size_t i = 0; i < groups.n; i++) {
        Group *g = groups.v[i];
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("class"), g->cls);
        avp_json_set(a, o, AVP_S("how"), g->how);
        avp_json_set(a, o, AVP_S("vtable"), g->vtable);
        avp_json_set(a, o, AVP_S("object_size"), g->object_size);
        avp_json_set(a, o, AVP_S("queued"), avp_json_int(a, g->queued));
        avp_json_set(a, o, AVP_S("sweep_match"), sweep ? avp_json_int(a, g->sweep_match) : avp_json_null());
        avp_json_set(a, o, AVP_S("symbols"), g->symbols);
        avp_json_set(a, o, AVP_S("functions"), avp_json_int(a, g->functions));
        avp_json_set(a, o, AVP_S("code_size"), avp_json_int(a, g->code_size));
        avp_json_set(a, o, AVP_S("first"), avp_json_str(a, avp_pyfmt(a, "0x%08X", g->first)));
        avp_json_arr_push(a, out, o);
        avp_iset_free(g->fns);
    }
    for (size_t i = 0; i < nc; i++) {
        avp_vec_free(&cls[i].addrs);
        avp_vec_free(&cls[i].roles);
        avp_imap_free(cls[i].index);
    }
    avp_vec_free(&groups);
    avp_vec_free(&owned);
    avp_map_free(index);
    avp_imap_free(owners);
    return out;
}

/* ---- the cache ------------------------------------------------------------------------------------------------- */

static const char *classes_json(AvpArena *a, const ShsmRepo *r) {
    return avp_path_join(a, r->version_build_dir, "classes.json");
}

static const char *sweep_json(AvpArena *a, const ShsmRepo *r) {
    return avp_path_join(a, draft_root(a, r), "sweep.json");
}

AvpStrVec draft_select_inputs(AvpArena *a, const ShsmRepo *r) {
    const char *idx = r->index_dir;
    const char *paths[] = {
        r->symbols_txt, r->splits_txt, r->libraries_tsv, draft_rejects_path(a, r), r->configure_py, r->report_json,
        r->baseline_report_json, avp_path_join(a, r->version_build_dir, "config.json"), classes_json(a, r),
        sweep_json(a, r), avp_path_join(a, idx, "asm.sqlite"), avp_path_join(a, idx, "twins.sqlite"),
        avp_path_join(a, idx, "hashes.sqlite"), avp_path_join(a, idx, "unit_plan.pickle"),
    };
    AvpStrVec out = {0};
    for (size_t i = 0; i < sizeof paths / sizeof *paths; i++) avp_vec_push(&out, avp_str(paths[i]));
    return out;
}

const char *draft_file_stamp(AvpArena *a, const char *const *paths, size_t n) {
    AvpStrVec parts = {0};
    for (size_t i = 0; i < n; i++) {
        AvpStr items[3];
        items[0] = avp_repr_str(a, avp_str(avp_path_normcase(a, avp_path_abspath(a, paths[i]))));
        AvpStat st;
        if (avp_stat(paths[i], &st)) {
            items[1] = avp_repr_i64(a, st.size);
            items[2] = avp_repr_i64(a, st.mtime_ns);
            avp_vec_push(&parts, avp_repr_tuple(a, items, 3));
        } else {
            items[1] = AVP_S("None");
            avp_vec_push(&parts, avp_repr_tuple(a, items, 2));
        }
    }
    avp_io_clear();
    AvpStr text = avp_repr_list(a, parts.v, parts.n);
    avp_vec_free(&parts);
    return avp_sha1_hex(a, text.p, text.n);
}

AvpJson *draft_cached(AvpArena *a, const ShsmRepo *r, const char *key, DraftComputeFn compute, void *ctx,
                      const AvpStrVec *inputs, bool *hit) {
    const char *path = avp_path_join(a, draft_root(a, r), "select_cache.json");
    AvpStrVec in = inputs ? *inputs : draft_select_inputs(a, r);
    const char **ps = AVP_NEWN(a, const char *, in.n + 1);
    for (size_t i = 0; i < in.n; i++) ps[i] = avp_cz(a, in.v[i]);
    const char *stamp = draft_file_stamp(a, ps, in.n);
    AvpJson *d = NULL;
    AvpStr text;
    if (avp_read_text(a, path, AVP_TM_UTF8, &text)) {
        AvpJsonError je;
        d = avp_json_parse(a, text, &je);
    }
    avp_io_clear();
    if (!avp_json_is_obj(d) || !jstr_eq(avp_json_getz(d, "stamp"), stamp) ||
        !avp_json_is_obj(avp_json_getz(d, "entries"))) {
        d = avp_json_obj(a);
        avp_json_set(a, d, AVP_S("stamp"), avp_json_strz(a, stamp));
        avp_json_set(a, d, AVP_S("entries"), avp_json_obj(a));
    }
    AvpJson *entries = avp_json_getz(d, "entries");
    AvpJson *have = avp_json_getz(entries, key);
    if (have) {
        *hit = true;
        return have;
    }
    AvpJson *val = compute(ctx);
    avp_json_set(a, entries, avp_str(key), val);
    /* json.dump(d) through a temporary file and os.replace; an OSError is ignored */
    char *dir = avp_cz(a, avp_path_dirname(path));
    if (avp_makedirs(dir, true)) {
        const char *tmp = avp_sprintf(a, "%s.%u.tmp", path, avp_getpid());
        AvpStr body = avp_json_dumps(a, d, &(AvpJsonDumpOpts){0});
        if (avp_write_text(tmp, body, AVP_TM_UTF8_NL_LF)) avp_replace(tmp, path);
    }
    avp_io_clear();
    *hit = false;
    return val;
}

/* ---- sweep verdicts, select_classes ----------------------------------------------------------------------------- */

AvpMap *draft_sweep_verdicts(AvpArena *a, const ShsmRepo *r, const char **why) {
    const char *path = sweep_json(a, r);
    AvpStat st;
    *why = NULL;
    if (!avp_stat(path, &st)) {
        avp_io_clear();
        *why = "no build/draft/sweep.json: sweep column empty (run build/tools/draft.exe sweep)";
        return NULL;
    }
    const char *newer[] = {r->symbols_txt, r->configure_py, r->report_json};
    for (int i = 0; i < 3; i++) {
        AvpStat s2;
        if (avp_stat(newer[i], &s2)) {
            if (s2.mtime_ns > st.mtime_ns) {
                AvpStr base = avp_path_basename(newer[i]);
                *why = avp_cz(a, avp_pyfmt(a,
                                           "build/draft/sweep.json is older than %S: sweep column empty (run "
                                           "build/tools/draft.exe sweep)",
                                           &base));
                return NULL;
            }
        }
        avp_io_clear();
    }
    AvpStr text;
    AvpJson *doc = NULL;
    if (avp_read_text(a, path, AVP_TM_UTF8, &text)) {
        AvpJsonError je;
        doc = avp_json_parse(a, text, &je);
    }
    avp_io_clear();
    if (!doc) {
        *why = "build/draft/sweep.json unreadable: sweep column empty";
        return NULL;
    }
    AvpJson *res = avp_json_is_obj(doc) ? avp_json_getz(doc, "results") : NULL;
    AvpMap *m = avp_map_new();
    for (size_t i = 0; avp_json_is_arr(res) && i < avp_json_len(res); i++) {
        AvpJson *x = avp_json_at(res, i);
        AvpJson *sym = avp_json_getz(x, "symbol");
        if (!avp_json_truthy(sym)) continue;
        avp_map_put(m, avp_str(pystr(a, sym)), NULL)->p = avp_json_get_or(x, AVP_S("verdict"), avp_json_null());
    }
    return m;
}

static bool world_addr_of(void *ctx, AvpStr name, i64 *addr) {
    DraftWorld *W = ctx;
    if (draft_addr_name(name, addr)) return true;
    ShsmSymRow row;
    if (!shsm_asmindex_symbol(W->a, draft_world_ix(W), name, &row)) return false;
    *addr = row.address;
    return true;
}

AvpJson *draft_select_classes(DraftWorld *W, const char *kind, const char *lib) {
    AvpArena *a = W->a;
    const ShsmRepo *r = W->repo;
    AvpJson *entries = draft_select(W, kind, lib, 0, false);
    AvpJson *notes = avp_json_arr(a), *classes = avp_json_arr(a);
    AvpStr text;
    AvpJson *db = NULL;
    if (avp_read_text(a, classes_json(a, r), AVP_TM_UTF8, &text)) {
        AvpJsonError je;
        db = avp_json_parse(a, text, &je);
    }
    avp_io_clear();
    if (db) {
        AvpJson *cl = avp_json_is_obj(db) ? avp_json_getz(db, "classes") : NULL;
        if (avp_json_is_arr(cl)) classes = cl;
        const char *why = NULL;
        AvpTry t;
        if (AVP_TRY(&t)) { /* staleness is advisory here (the Python's `except Exception`) */
            ClassesDb tmp = {.doc = db, .inputs = avp_json_is_obj(db) ? avp_json_getz(db, "inputs") : NULL};
            why = classes_stale_reason(a, &tmp, r->asm_dir, r->symbols_txt);
            avp_try_end(&t);
        }
        if (why)
            avp_json_arr_push(
                a, notes,
                avp_json_str(a, avp_pyfmt(a, "build/R5WEA4/classes.json is stale (%s): run build/tools/classes.exe scan",
                                          why)));
    } else {
        avp_json_arr_push(a, notes,
                          avp_json_strz(a, "no build/R5WEA4/classes.json: classes unknown (run "
                                           "build/tools/classes.exe scan)"));
    }
    AvpISet *placeholder = avp_iset_new();
    for (size_t i = 0; i < avp_json_len(classes); i++) {
        AvpJson *c = avp_json_at(classes, i);
        AvpJson *ns = avp_json_getz(c, "name_source");
        if (!ns || strncmp(pystr(a, ns), "placeholder", 11) != 0) continue;
        AvpJson *members = avp_json_getz(c, "members");
        for (size_t j = 0; avp_json_is_arr(members) && j < avp_json_len(members); j++) {
            i64 ad;
            if (addr_value(avp_json_getz(avp_json_at(members, j), "address"), &ad)) avp_iset_add(placeholder, (u64)ad);
        }
    }
    AvpIMap *interned = avp_imap_new();
    if (avp_iset_len(placeholder)) {
        AVP_VEC(i64) fns = {0};
        size_t pos = 0;
        u64 m;
        while (avp_iset_next(placeholder, &pos, &m)) avp_vec_push(&fns, (i64)m);
        HashesVarNames *vn = hashes_var_names(a, r, NULL, NULL, 0, fns.v, fns.n);
        for (size_t i = 0; vn && i < vn->n; i++) {
            const HashesVarName *x = &vn->v[i];
            if (x->kind && !strcmp(x->kind, "interned name") && x->text && x->text[0] &&
                avp_iset_has(placeholder, (u64)x->function_addr)) {
                bool created;
                AvpVal *v = avp_imap_put(interned, (u64)x->function_addr, &created);
                if (created) v->p = (void *)x->text;
            }
        }
        avp_vec_free(&fns);
    }
    avp_iset_free(placeholder);
    const char *why = NULL;
    AvpMap *sweep = draft_sweep_verdicts(a, r, &why);
    if (why) avp_json_arr_push(a, notes, avp_json_strz(a, why));
    Twins *tw = draft_world_tw(W);
    AvpIMap *fn_info = avp_imap_new();
    for (size_t i = 0; i < tw->fns.n; i++) avp_imap_put(fn_info, (u64)tw->fns.v[i]->address, NULL)->p = tw->fns.v[i];
    AvpJson *groups = draft_group_by_class(a, entries, classes, interned, sweep, fn_info, world_addr_of, W);
    for (size_t i = 0; i < avp_json_len(groups); i++) {
        AvpJson *g = avp_json_at(groups, i);
        i64 first = 0;
        addr_value(avp_json_getz(g, "first"), &first);
        put_unit_of(a, g, draft_world_unit_of(W, first));
    }
    avp_imap_free(fn_info);
    avp_imap_free(interned);
    if (sweep) avp_map_free(sweep);
    AvpJson *doc = avp_json_obj(a);
    avp_json_set(a, doc, AVP_S("groups"), groups);
    avp_json_set(a, doc, AVP_S("notes"), notes);
    return doc;
}

/* ---- cmd_select -------------------------------------------------------------------------------------------------- */

/* _unit_text(e) */
static const char *unit_text(AvpArena *a, const AvpJson *e) {
    AvpJson *u = avp_json_getz(e, "unit"), *kind = avp_json_getz(e, "unit_kind"), *rng = avp_json_getz(e, "unit_range");
    const char *unit = avp_json_truthy(u) ? pystr(a, u) : "?";
    if (jstr_eq(kind, "plan")) {
        AvpJson *conf = avp_json_getz(e, "confidence");
        const char *c = avp_json_is_null(conf) ? "-" : avp_cz(a, avp_pyfmt(a, "%.2f", avp_json_f64(conf, 0)));
        return avp_cz(a, avp_pyfmt(a, "%s [plan %s %s]", avp_json_truthy(u) ? pystr(a, u) : "<path>", pystr(a, rng), c));
    }
    if (avp_json_truthy(rng)) return avp_cz(a, avp_pyfmt(a, "%s [%s %s]", unit, pystr(a, kind), pystr(a, rng)));
    return unit;
}

typedef struct {
    DraftWorld *W;
    const char *kind, *lib;
    i64 limit;
} SelectCtx;

static AvpJson *compute_classes(void *ctx) {
    SelectCtx *c = ctx;
    return draft_select_classes(c->W, c->kind, c->lib);
}

static AvpJson *compute_functions(void *ctx) {
    SelectCtx *c = ctx;
    return draft_select(c->W, c->kind, c->lib, c->limit, true);
}

int draft_cmd_select(DraftWorld *W, ShsmOutput *out, const char *kind, const char *lib, bool functions, i64 limit,
                     bool all) {
    AvpArena *a = W->a;
    i64 lim = all || !limit ? 0 : limit;
    bool hit = false;
    SelectCtx ctx = {W, kind, lib, lim};
    if (!functions) {
        const char *key = avp_cz(a, avp_pyfmt(a, "classes|%s|%s", kind, lib));
        AvpJson *doc = draft_cached(a, W->repo, key, compute_classes, &ctx, NULL, &hit);
        AvpJson *groups = avp_json_getz(doc, "groups"), *notes = avp_json_getz(doc, "notes");
        for (size_t i = 0; i < avp_json_len(notes); i++)
            shsm_output_line(out, avp_pyfmt(a, "note: %s", pystr(a, avp_json_at(notes, i))));
        static const char LINE[] = "%-36s %-7s %-5s %-6s %-7s %s";
        shsm_output_line(out, avp_pyfmt(a, LINE, "class (name evidence)", "queued", "sweep", "object", "code",
                                        "unit (proposed range)"));
        size_t n = avp_json_len(groups);
        if (lim && (size_t)lim < n) n = (size_t)lim;
        for (size_t i = 0; i < n; i++) {
            AvpJson *g = avp_json_at(groups, i);
            const char *label = avp_cz(a, avp_pyfmt(a, "%s (%s)", pystr(a, avp_json_getz(g, "class")),
                                                    pystr(a, avp_json_getz(g, "how"))));
            AvpJson *sm = avp_json_getz(g, "sweep_match"), *os = avp_json_getz(g, "object_size");
            shsm_output_line(out, avp_pyfmt(a, LINE, cut(a, label, 36),
                                            avp_cz(a, avp_pyfmt(a, "%d/%d", avp_json_get_i64(g, "queued", 0),
                                                                avp_json_get_i64(g, "functions", 0))),
                                            avp_json_is_null(sm) ? "-" : pystr(a, sm),
                                            avp_json_truthy(os) ? pystr(a, os) : "-",
                                            avp_cz(a, avp_pyfmt(a, "0x%X", avp_json_get_i64(g, "code_size", 0))),
                                            unit_text(a, g)));
        }
        shsm_output_data(out, "classes", groups);
        shsm_output_data(out, "count", avp_json_int(a, (i64)avp_json_len(groups)));
        shsm_output_data(out, "notes", notes);
        shsm_output_data(out, "cached", avp_json_bool(hit));
        return 0;
    }
    const char *key = avp_cz(a, lim ? avp_pyfmt(a, "functions|%s|%s|%d", kind, lib, lim)
                                    : avp_pyfmt(a, "functions|%s|%s|None", kind, lib));
    AvpJson *rows = draft_cached(a, W->repo, key, compute_functions, &ctx, NULL, &hit);
    for (size_t i = 0; i < avp_json_len(rows); i++) {
        AvpJson *e = avp_json_at(rows, i);
        AvpJson *u = avp_json_getz(e, "unit"), *twin = avp_json_getz(e, "twin"), *lb = avp_json_getz(e, "lib");
        const char *unit = jstr_eq(avp_json_getz(e, "unit_kind"), "plan") ? unit_text(a, e)
                           : avp_json_truthy(u)                          ? pystr(a, u)
                                                                         : "?";
        const char *tw = avp_json_truthy(twin) ? avp_cz(a, avp_pyfmt(a, "  twin %s", cut(a, pystr(a, twin), 50))) : "";
        shsm_output_line(out, avp_pyfmt(a, "%-5s %s %5d %-44s %-12s %s%s%s", pystr(a, avp_json_getz(e, "kind")),
                                        pystr(a, avp_json_getz(e, "address")), avp_json_get_i64(e, "size", 0),
                                        cut(a, pystr(a, avp_json_getz(e, "symbol")), 44),
                                        avp_json_truthy(lb) ? pystr(a, lb) : "?", unit, tw,
                                        avp_json_truthy(avp_json_getz(e, "reject")) ? "  (rejected: worth another look)"
                                                                                    : ""));
    }
    shsm_output_data(out, "targets", rows);
    shsm_output_data(out, "count", avp_json_int(a, (i64)avp_json_len(rows)));
    shsm_output_data(out, "cached", avp_json_bool(hit));
    return 0;
}

/* ---- sweep ------------------------------------------------------------------------------------------------------- */

typedef struct {
    AvpJson *target;       /* the select() row */
    const DraftTwinMeta *meta; /* NULL: err */
    const char *err;
} SweepItem;

typedef struct {
    DraftWorld *W;
    int jobs;
    const char *root;
} SweepCtx;

static AVP_THREAD_LOCAL DraftWorld *t_world;

static AvpJson *fail_row(AvpArena *a, const AvpJson *t, AvpStr reason) {
    /* a member the target's header already defines in its class is not a failed draft: there is nothing to draft */
    bool skipped = avp_str_contains(reason, avp_str(DRAFT_INLINE_DEFINED));
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("symbol"), avp_json_get_or(t, AVP_S("symbol"), avp_json_null()));
    avp_json_set(a, o, AVP_S("lib"), avp_json_get_or(t, AVP_S("lib"), avp_json_null()));
    avp_json_set(a, o, AVP_S("verdict"), avp_json_strz(a, skipped ? "SKIPPED" : "FAIL"));
    avp_json_set(a, o, AVP_S("reason"), avp_json_str(a, reason));
    return o;
}

static int sweep_one(void *ctx, size_t index, const void *item, void *result) {
    (void)index;
    SweepCtx *c = ctx;
    const SweepItem *it = item;
    AvpJson **res = result;
    if (it->err) {
        *res = fail_row(avp_arena(), it->target, avp_str(it->err));
        return 0;
    }
    DraftWorld *W = c->W;
    if (c->jobs > 1 && avp_pool_worker_index() >= 0) {
        if (!t_world) t_world = draft_world_new(c->root); /* the asm index connection is per thread */
        W = t_world;
    }
    AvpArena *a = W->a;
    DraftGrade *g = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        g = draft_grade(W, it->meta->target, it->meta->source, NULL, NULL, it->meta->meta, NULL);
        avp_try_end(&t);
    } else {
        AvpStr msg = shsm_exc_str(a, t.code, t.msg);
        *res = fail_row(a, it->target, avp_pyfmt(a, "grade: %S", &msg));
        return 0;
    }
    const char *v = g->verdict;
    if (!strcmp(v, "MATCH") && g->blocking) v = "MATCH-BLOCKED";
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("symbol"), avp_json_get_or(it->target, AVP_S("symbol"), avp_json_null()));
    avp_json_set(a, o, AVP_S("lib"), avp_json_get_or(it->target, AVP_S("lib"), avp_json_null()));
    avp_json_set(a, o, AVP_S("verdict"), avp_json_strz(a, v));
    /* a skipped R13 (classes.json absent or stale) blocks every draft: say so rather than "rules/screen" */
    const char *blocked = !g->blocking ? "" : g->rules.r13 ? DRAFT_R13_SKIPPED : "rules/screen";
    const char *reason = g->reason && g->reason[0] ? g->reason : blocked;
    avp_json_set(a, o, AVP_S("reason"), avp_json_strz(a, reason));
    AvpJson *lines = avp_json_arr(a);
    for (size_t i = 0; i < g->lines.n; i++) avp_json_arr_push(a, lines, avp_json_str(a, g->lines.v[i]));
    avp_json_set(a, o, AVP_S("lines"), lines);
    const AvpJson *m = it->meta->meta;
    avp_json_set(a, o, AVP_S("draft"), avp_json_get_or(m, AVP_S("source"), avp_json_null()));
    avp_json_set(a, o, AVP_S("unit_path"), avp_json_get_or(m, AVP_S("unit_path"), avp_json_null()));
    avp_json_set(a, o, AVP_S("unit_range"), avp_json_get_or(m, AVP_S("unit_range"), avp_json_null()));
    avp_json_set(a, o, AVP_S("twin"), avp_json_get_or(m, AVP_S("twin"), avp_json_null()));
    *res = o;
    return 0;
}

AvpJson *draft_sweep(DraftWorld *W, const char *lib, i64 limit, int jobs) {
    AvpArena *a = W->a;
    AvpJson *targets = draft_select(W, "twin", lib, limit, false);
    size_t n = avp_json_len(targets);
    SweepItem *items = AVP_NEWN(a, SweepItem, n ? n : 1);
    for (size_t i = 0; i < n; i++) {
        items[i].target = avp_json_at(targets, i);
        const char *sym = pystr(a, avp_json_getz(items[i].target, "symbol"));
        AvpTry t;
        if (AVP_TRY(&t)) {
            items[i].meta = draft_make_twin_draft(W, sym, NULL);
            avp_try_end(&t);
        } else {
            AvpStr msg = shsm_exc_str(a, t.code, t.msg);
            i64 nl = avp_str_find(msg, AVP_S("\n"), 0, AVP_END);
            if (nl >= 0) msg = avp_strn(msg.p, (size_t)nl);
            items[i].err = avp_cz(a, avp_pyfmt(a, "draft: %S", &msg));
        }
    }
    AvpJson **results = AVP_NEWN(a, AvpJson *, n ? n : 1);
    SweepCtx ctx = {W, jobs, W->repo->root};
    avp_pool_map(NULL, jobs > 1 ? jobs : 1, sweep_one, &ctx, items, n, sizeof *items, results, sizeof *results);
    AvpJson *out = avp_json_arr(a);
    for (size_t i = 0; i < n; i++) avp_json_arr_push(a, out, results[i]);
    return out;
}

typedef struct {
    AvpStr key;
    i64 n;
} Count;

static int cmp_count_desc(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const Count *p = x, *q = y;
    return p->n > q->n ? -1 : p->n < q->n ? 1 : 0;
}

static int cmp_count_key(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return avp_str_cmp(((const Count *)x)->key, ((const Count *)y)->key);
}

int draft_cmd_sweep(DraftWorld *W, ShsmOutput *out, const char *lib, i64 limit, bool limit_given, int jobs) {
    AvpArena *a = W->a;
    AvpJson *res = draft_sweep(W, lib, limit_given && limit ? limit : 0, jobs);
    AvpMap *totals = avp_map_new(), *reasons = avp_map_new();
    /* the Python's pattern starts with a literal backspace (U+0008, a `\b` typed into its raw string), so the
     * placeholder alternative never matches and only hex and decimal numbers become N: reproduced (D-draft-11) */
    const ShsmRe *norm = shsm_re("\x08(?:fn|lbl)_[0-9A-F]{8}\\w*|0x[0-9A-F]+|\\d+", 0);
    size_t nres = avp_json_len(res);
    for (size_t i = 0; i < nres; i++) {
        AvpJson *r = avp_json_at(res, i);
        AvpJson *v = avp_json_getz(r, "verdict");
        const char *vs = pystr(a, v);
        avp_map_put(totals, avp_str(vs), NULL)->i++;
        if (!jstr_eq(v, "MATCH")) {
            AvpJson *rs = avp_json_getz(r, "reason");
            AvpStr reason = avp_json_truthy(rs) ? avp_str(pystr(a, rs)) : AVP_S("");
            AvpStr key = avp_u8_slice(shsm_re_sub(a, norm, AVP_S("N"), reason, 0), 0, 90);
            avp_map_put(reasons, key, NULL)->i++;
        }
    }
    AVP_VEC(Count) tv = {0}, rv = {0};
    AvpMapIter it = {0};
    AvpJson *tj = avp_json_obj(a), *rj = avp_json_obj(a);
    while (avp_map_next(totals, &it)) {
        avp_vec_push(&tv, ((Count){avp_str_dup(a, it.key), it.val->i}));
        avp_json_set(a, tj, it.key, avp_json_int(a, it.val->i));
    }
    it = (AvpMapIter){0};
    while (avp_map_next(reasons, &it)) {
        avp_vec_push(&rv, ((Count){avp_str_dup(a, it.key), it.val->i}));
        avp_json_set(a, rj, it.key, avp_json_int(a, it.val->i));
    }
    avp_sort(tv.v, tv.n, sizeof *tv.v, cmp_count_key, NULL, false);
    avp_sort(rv.v, rv.n, sizeof *rv.v, cmp_count_desc, NULL, false);
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, "totals: ");
    for (size_t i = 0; i < tv.n; i++) {
        if (i) avp_buf_putz(&b, ", ");
        avp_buf_put(&b, avp_pyfmt(a, "%S %d", &tv.v[i].key, tv.v[i].n));
    }
    avp_buf_put(&b, avp_pyfmt(a, " (of %d)", (i64)nres));
    shsm_output_line(out, avp_buf_str(&b));
    for (size_t i = 0; i < rv.n && i < 8; i++) shsm_output_line(out, avp_pyfmt(a, "  %4d  %S", rv.v[i].n, &rv.v[i].key));
    for (size_t i = 0; i < nres; i++) {
        AvpJson *r = avp_json_at(res, i);
        AvpJson *lb = avp_json_getz(r, "lib"), *rs = avp_json_getz(r, "reason");
        shsm_output_line(out, avp_pyfmt(a, "%-13s %-44s %-10s %s", pystr(a, avp_json_getz(r, "verdict")),
                                        cut(a, pystr(a, avp_json_getz(r, "symbol")), 44),
                                        avp_json_truthy(lb) ? pystr(a, lb) : "?",
                                        avp_json_truthy(rs) ? pystr(a, rs) : ""));
    }
    const char *path = sweep_json(a, W->repo);
    if (!avp_makedirs(avp_cz(a, avp_path_dirname(path)), true)) shsm_raise_io();
    AvpJson *doc = avp_json_obj(a);
    avp_json_set(a, doc, AVP_S("totals"), tj);
    avp_json_set(a, doc, AVP_S("reasons"), rj);
    avp_json_set(a, doc, AVP_S("results"), res);
    AvpJsonDumpOpts opts = AVP_JSON_INDENT(1);
    if (!avp_write_text(path, avp_json_dumps(a, doc, &opts), AVP_TM_UTF8_NL_LF)) shsm_raise_io();
    shsm_output_data(out, "totals", tj);
    shsm_output_data(out, "reasons", rj);
    shsm_output_data(out, "results", res);
    shsm_output_data(out, "path", avp_json_strz(a, path));
    avp_vec_free(&tv);
    avp_vec_free(&rv);
    avp_map_free(totals);
    avp_map_free(reasons);
    return 0;
}
