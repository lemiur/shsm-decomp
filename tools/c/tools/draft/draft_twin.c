/* draft.py's class evidence (vtables_of, interned_name, target_class, pair_vtables) and twin drafting
 * (choose_twin, draft_paths, make_twin_draft, twin_unit_range, cmd_twin, cmd_twin_unit). */
#include "draft.h"

#include "shsm_mangle.h"
#include "shsm_re.h"

#include <string.h>
#include <stdio.h>

static const char *const ADDR_STMT_RE = "\\b((?:fn|lbl)_[0-9A-F]{8})\\s*(?:=|\\(|;|\\[)";

static bool streq_opt(const char *x, const char *y) { return (x && y) ? !strcmp(x, y) : x == y; }
static AvpJson *jstr(AvpArena *a, const char *s) { return s ? avp_json_strz(a, s) : avp_json_null(); }
static AvpJson *jstrv(AvpArena *a, AvpStr s) { return s.p ? avp_json_str(a, s) : avp_json_null(); }

/* IDENT.findall(text) into a set (and, when list is non-NULL, every match in order). */
static void idents_of(AvpStr text, AvpSet *set, AvpStrVec *list) {
    size_t i = 0;
    while (i < text.n) {
        char c = text.p[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_') {
            size_t j = i + 1;
            while (j < text.n) {
                char d = text.p[j];
                if (!((d >= 'A' && d <= 'Z') || (d >= 'a' && d <= 'z') || (d >= '0' && d <= '9') || d == '_')) break;
                j++;
            }
            AvpStr id = avp_strn(text.p + i, j - i);
            if (set) avp_set_add(set, id);
            if (list) avp_vec_push(list, id);
            i = j;
        } else {
            i++;
        }
    }
}

static AvpStr slice(AvpStr s, i64 a, i64 b) { return avp_strn(s.p + a, (size_t)(b - a)); }

/* ---- class evidence ---------------------------------------------------------------------------------------------- */

static const char *const K_PTR[] = {"ptr"};
static const char *const K_SDA_HA_L[] = {"sda21", "ha", "l"};
static const char *const K_HA_L[] = {"ha", "l"};

AvpStrVec draft_vtables_of(DraftWorld *W, i64 address) {
    AvpStrVec out = {0};
    ShsmRefs refs = shsm_asmindex_refs_to(W->a, draft_world_ix(W), shsm_target_addr(address), K_PTR, 1);
    for (size_t i = 0; i < refs.n; i++) {
        AvpStr f = refs.v[i].from_sym;
        if (!f.p || !f.n) continue;
        bool seen = false;
        for (size_t k = 0; k < out.n && !seen; k++) seen = avp_str_eq(out.v[k], f);
        if (!seen) avp_vec_push(&out, f);
    }
    return out;
}

const HashesVarName *draft_interned_name(DraftWorld *W, const i64 *addresses, size_t n) {
    HashesVarNames *rows = hashes_var_names(W->a, W->repo, NULL, addresses, n, NULL, 0);
    if (!rows) return NULL;
    for (size_t i = 0; i < rows->n; i++) {
        const HashesVarName *r = &rows->v[i];
        if (r->kind && !strcmp(r->kind, "interned name") && r->text && r->text[0]) return r;
    }
    return NULL;
}

typedef struct {
    i64 addr;
    AvpStr sym;
} AddrSym;

static int cmp_addr_sym(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const AddrSym *p = x, *q = y;
    int c = avp_cmp_i64(p->addr, q->addr);
    return c ? c : avp_cmp_str(p->sym, q->sym);
}

const char *draft_target_class(DraftWorld *W, const char *target_sym, i64 target_addr, const DraftPairs *pairs,
                               const char *twin_unit, const char *twin_class, const char **why) {
    AvpArena *a = W->a;
    ShsmAsmIndex *ix = draft_world_ix(W);
    const char *cls = NULL;
    draft_demangled_parts(a, target_sym, &cls);
    if (cls) {
        *why = "its own mangled name";
        return cls;
    }
    AvpStrVec vts = draft_vtables_of(W, target_addr);
    for (size_t i = 0; i < vts.n; i++) {
        const char *vt = avp_cz(a, vts.v[i]);
        const char *c = NULL;
        draft_demangled_parts(a, vt, &c);
        bool is_vt = avp_str_startswith(vts.v[i], AVP_S("__vt__"));
        if (is_vt && !c) {
            AvpStr rest;
            draft_split_mangled(vts.v[i], &rest);
            AvpStr r = rest.p ? rest : AVP_S("");
            size_t k = 0;
            while (k < r.n && r.p[k] >= '0' && r.p[k] <= '9') k++;
            c = k < r.n ? avp_cz(a, avp_strn(r.p + k, r.n - k)) : NULL;
        }
        if (is_vt && c) {
            *why = avp_sprintf(a, "vtable %s", vt);
            return c;
        }
    }
    /* the twin class's interned name variable, mapped through the relocations */
    HashesVarNames *rows = hashes_var_names(a, W->repo, avp_sprintf(a, "main/%s", twin_unit), NULL, 0, NULL, 0);
    if (rows)
        for (size_t i = 0; i < rows->n; i++) {
            const HashesVarName *r = &rows->v[i];
            if (!(streq_opt(r->text, twin_class) && r->kind && !strcmp(r->kind, "interned name"))) continue;
            for (size_t k = 0; k < pairs->n; k++) {
                if (!r->var || !avp_str_eqz(pairs->v[k].tsym, r->var)) continue;
                DraftInfo gi = draft_world_info(W, pairs->v[k].gsym, 0);
                const HashesVarName *hit = NULL;
                if (gi.address != DRAFT_NONE && gi.address) hit = draft_interned_name(W, &gi.address, 1);
                if (hit) {
                    *why = avp_pyfmt(a, "interned class name of %s in %s", hit->var, hit->function).p;
                    return hit->text;
                }
            }
        }
    /* a sibling in the vtable that reads the interned name */
    for (size_t i = 0; i < vts.n; i++) {
        AvpI64Vec fns = {0};
        AvpTry t;
        if (AVP_TRY(&t)) {
            ShsmDataRefs dt = shsm_asmindex_data_targets(a, ix, shsm_target_name(vts.v[i]));
            for (size_t k = 0; k < dt.n; k++)
                if (dt.v[k].address != SHSM_NONE && dt.v[k].address) avp_vec_push(&fns, dt.v[k].address);
            avp_try_end(&t);
        } else {
            fns.n = 0;
        }
        AvpI64Vec addrs = {0};
        for (size_t k = 0; k < fns.n; k++) {
            ShsmRefs rf = shsm_asmindex_refs_from(a, ix, shsm_target_addr(fns.v[k]), K_SDA_HA_L, 3);
            for (size_t j = 0; j < rf.n; j++)
                if (rf.v[j].to_addr != SHSM_NONE && rf.v[j].to_addr) avp_vec_push(&addrs, rf.v[j].to_addr);
        }
        const HashesVarName *hit = addrs.n ? draft_interned_name(W, addrs.v, addrs.n) : NULL;
        avp_vec_free(&fns);
        avp_vec_free(&addrs);
        if (hit) {
            *why = avp_pyfmt(a, "interned class name of %s (read by a function of vtable %S)", hit->var, &vts.v[i]).p;
            return hit->text;
        }
    }
    for (size_t i = 0; i < vts.n; i++) {
        ShsmRefs rt = shsm_asmindex_refs_to(a, ix, shsm_target_name(vts.v[i]), K_HA_L, 2);
        AVP_VEC(AddrSym) stores = {0};
        for (size_t k = 0; k < rt.n; k++) {
            AvpStr fs = rt.v[k].from_sym;
            if (!fs.p || !fs.n || avp_str_eqz(fs, target_sym)) continue;
            avp_vec_push(&stores, ((AddrSym){rt.v[k].from_addr, fs}));
        }
        avp_sort(stores.v, stores.n, sizeof *stores.v, cmp_addr_sym, NULL, false);
        AVP_VEC(AddrSym) fns = {0};
        for (size_t k = 0; k < stores.n; k++) {
            if (k && !cmp_addr_sym(&stores.v[k], &stores.v[k - 1], NULL)) continue; /* the set */
            ShsmSymRow row;
            if (!shsm_asmindex_symbol(a, ix, stores.v[k].sym, &row)) continue;
            bool have = false;
            for (size_t j = 0; j < fns.n && !have; j++) have = fns.v[j].addr == row.address;
            if (!have) avp_vec_push(&fns, ((AddrSym){row.address, stores.v[k].sym}));
        }
        avp_vec_free(&stores);
        if (fns.n) {
            avp_sort(fns.v, fns.n, sizeof *fns.v, cmp_addr_sym, NULL, false);
            const char *c = avp_sprintf(a, "unk_%08llX", (unsigned long long)fns.v[0].addr);
            *why = avp_pyfmt(a, "placeholder: %S is the first function storing vtable %S (constructor or inline factory)",
                             &fns.v[0].sym, &vts.v[i])
                       .p;
            avp_vec_free(&fns);
            return c;
        }
        avp_vec_free(&fns);
    }
    *why = "placeholder: no vtable or constructor found; named after the target";
    return avp_sprintf(a, "unk_%08llX", (unsigned long long)target_addr);
}

static bool is_slot_item(const ShsmAsmItem *it) {
    return it->kind && (!strcmp(it->kind, "ptr") || !strcmp(it->kind, "4byte"));
}

void draft_pair_vtables(DraftWorld *W, i64 twin_addr, i64 target_addr, const char *twin_class, DraftSubst *s) {
    AvpArena *a = W->a;
    AvpStrVec tv = draft_vtables_of(W, twin_addr), gv = draft_vtables_of(W, target_addr);
    if (!tv.n || !gv.n || !twin_class) return;
    ShsmAsmBlock *tb = NULL, *gb = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        tb = shsm_asmindex_object(a, draft_world_ix(W), shsm_target_name(tv.v[0]), &(ShsmAsmFile *){NULL});
        if (tb) gb = shsm_asmindex_object(a, draft_world_ix(W), shsm_target_name(gv.v[0]), &(ShsmAsmFile *){NULL});
        avp_try_end(&t);
    } else {
        return;
    }
    if (!tb || !gb) return; /* None.items: the Python's AttributeError, caught */
    AVP_VEC(const ShsmAsmItem *) ti = {0}, gi = {0};
    for (size_t i = 0; i < tb->items.n; i++)
        if (is_slot_item(&tb->items.v[i])) avp_vec_push(&ti, &tb->items.v[i]);
    for (size_t i = 0; i < gb->items.n; i++)
        if (is_slot_item(&gb->items.v[i])) avp_vec_push(&gi, &gb->items.v[i]);
    if (!draft_smap_get(s->renames, gv.v[0]).p && avp_str_startswith(tv.v[0], AVP_S("__vt__")) &&
        !avp_str_startswith(gv.v[0], AVP_S("__vt__"))) {
        AvpStr nm = avp_str(draft_remangle(a, tv.v[0], s->classes, s->idents));
        draft_smap_set(a, s->renames, gv.v[0], nm);
        avp_vec_push(&s->evidence, ((DraftEvidence){AVP_S("vtable"), tv.v[0], nm,
                                                    avp_pyfmt(a, "vtable of the target: %S", &gv.v[0])}));
    }
    if (ti.n != gi.n) {
        draft_subst_unmapped(s, "vtable", gv.v[0],
                             avp_pyfmt(a, "%d slots against %d in %S", (i64)gi.n, (i64)ti.n, &tv.v[0]));
    } else {
        for (size_t i = 0; i < ti.n; i++) {
            AvpStr ts = ti.v[i]->sym, gs = gi.v[i]->sym;
            if (!ts.p || !ts.n || !gs.p || !gs.n || avp_str_eq(ts, gs) || draft_smap_get(s->renames, gs).p) continue;
            const char *c = NULL;
            draft_demangled_parts(a, avp_cz(a, ts), &c);
            AvpStr rest;
            draft_split_mangled(gs, &rest);
            if (c && !strcmp(c, twin_class) && !rest.p) {
                AvpStr nm = avp_str(draft_remangle(a, ts, s->classes, s->idents));
                draft_smap_set(a, s->renames, gs, nm);
                avp_vec_push(&s->evidence, ((DraftEvidence){AVP_S("slot"), ts, nm,
                                                            avp_pyfmt(a, "vtable slot: %S in %S", &gs, &gv.v[0])}));
            }
        }
    }
    avp_vec_free(&ti);
    avp_vec_free(&gi);
}

/* ---- twin drafting ------------------------------------------------------------------------------------------------ */

const char *draft_twin_level(const TwinsFn *f, const TwinsFn *tw) {
    return streq_opt(f->fp[TWINS_REF], tw->fp[TWINS_REF]) ? "ref" : "exact";
}

typedef struct {
    const TwinsFn *f;
} ChooseCtx;

static int cmp_twin(const void *x, const void *y, void *ctx) {
    const TwinsFn *f = ((ChooseCtx *)ctx)->f;
    const TwinsFn *p = *(TwinsFn *const *)x, *q = *(TwinsFn *const *)y;
    int a1 = !streq_opt(p->status, "Matching"), b1 = !streq_opt(q->status, "Matching");
    if (a1 != b1) return a1 - b1;
    int a2 = !streq_opt(p->lib, f->lib), b2 = !streq_opt(q->lib, f->lib);
    if (a2 != b2) return a2 - b2;
    /* the same relocation targets as the target: the twin's names need no substitution */
    int a3 = !streq_opt(p->fp[TWINS_REF], f->fp[TWINS_REF]), b3 = !streq_opt(q->fp[TWINS_REF], f->fp[TWINS_REF]);
    if (a3 != b3) return a3 - b3;
    return avp_cmp_i64(p->address, q->address);
}

TwinsFn *draft_choose_twin(DraftWorld *W, const char *target, TwinsFnVec *twins) {
    Twins *tw = draft_world_tw(W);
    TwinsFn *f = twins_resolve(tw, target);
    if (f->matched) shsm_raise(SHSM_EXC_USAGE, "%s is already matched", f->name);
    TwinsFnVec all = twins_of(W->a, tw, f, TWINS_EXACT);
    TwinsFnVec out = {0};
    for (size_t i = 0; i < all.n; i++)
        if (all.v[i]->matched && all.v[i]->source && all.v[i]->source[0]) avp_vec_push(&out, all.v[i]);
    if (!out.n) shsm_raise(SHSM_EXC_FINDING, "%s has no matched exact twin", f->name);
    ChooseCtx c = {f};
    avp_sort(out.v, out.n, sizeof *out.v, cmp_twin, &c, false);
    *twins = out;
    return f;
}

const char *draft_paths(AvpArena *a, const ShsmRepo *r, const char *out, const char *target) {
    const char *d = (out && out[0]) ? out : avp_path_join(a, draft_root(a, r), draft_safe_name(a, target));
    return avp_path_abspath(a, d);
}

static const char *read_text(AvpArena *a, const char *path, AvpTextMode m, AvpStr *out) {
    if (!avp_read_text(a, path, m, out)) shsm_raise_io();
    return path;
}

static void write_text(AvpArena *a, const char *path, AvpStr text) {
    AvpStr enc;
    if (!avp_encode_text(a, text, AVP_TM_UTF8_NL_LF, &enc)) shsm_raise_io();
    if (!avp_write_file(path, enc.p, enc.n)) shsm_raise_io();
}

static void write_json(AvpArena *a, const char *path, const AvpJson *v) {
    AvpJsonDumpOpts o = shsm_json_opts();
    write_text(a, path, avp_json_dumps(a, v, &o));
}

static bool ends_cxx(const char *src) {
    AvpStr s = avp_str(src);
    return avp_str_endswith(s, AVP_S(".cpp")) || avp_str_endswith(s, AVP_S(".cp")) ||
           avp_str_endswith(s, AVP_S(".cc"));
}

/* s.classes.pop(cls); s.strings.pop(cls); s.evidence = [e for e in s.evidence if e[1] != cls] */
static void drop_class(DraftSubst *s, const char *cls) {
    if (!cls) return;
    avp_map_del(s->classes, avp_str(cls));
    avp_map_del(s->strings, avp_str(cls));
    size_t k = 0;
    for (size_t i = 0; i < s->evidence.n; i++)
        if (!avp_str_eqz(s->evidence.v[i].old, cls)) s->evidence.v[k++] = s->evidence.v[i];
    s->evidence.n = k;
}

/* re.match(r"^(?:extern|typedef|using|class|struct|union|enum|template|namespace)\b", txt) */
static bool is_declaration(AvpStr txt) {
    ShsmReMatch m;
    return shsm_re_match(shsm_re("^(?:extern|typedef|using|class|struct|union|enum|template|namespace)\\b", 0), txt,
                         0, AVP_END, &m);
}

static AvpStr dirname_of(const char *p) { return avp_path_dirname(p); }

/* The namespaces a member function's mangled name puts its class in: `__dt__Q24FMOD19ChannelRealManual3DFv` is
 * FMOD::ChannelRealManual3D::~ChannelRealManual3D, so {"FMOD"}. Nothing for a global class or an unparsable name. */
AvpStrVec draft_namespaces_of(AvpArena *a, const char *sym) {
    AvpStrVec out = {0};
    ShsmDemangled *p = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        p = shsm_mangle_parse(a, sym);
        avp_try_end(&t);
    } else {
        p = NULL;
    }
    if (p && !strcmp(p->kind, "function"))
        for (size_t i = 0; i + 1 < (size_t)p->nscope; i++) avp_vec_push(&out, avp_str(p->scope[i]));
    return out;
}

/* `namespace X {` ... `} // namespace X`, outermost first. */
static void open_namespaces(AvpStrVec *lines, const AvpStrVec *ns, AvpArena *a) {
    for (size_t i = 0; i < ns->n; i++) avp_vec_push(lines, avp_str(avp_sprintf(a, "namespace %.*s {", AVP_SF(ns->v[i]))));
    if (ns->n) avp_vec_push(lines, AVP_S(""));
}

static void close_namespaces(AvpStrVec *lines, const AvpStrVec *ns, AvpArena *a) {
    for (size_t i = ns->n; i-- > 0;)
        avp_vec_push(lines, avp_str(avp_sprintf(a, "} // namespace %.*s", AVP_SF(ns->v[i]))));
    if (ns->n) avp_vec_push(lines, AVP_S(""));
}

/* The class block of a cloned header, wrapped in the namespaces the target's class lives in. */
AvpStr draft_wrap_class_block(AvpArena *a, AvpStr text, const char *cls, const AvpStrVec *ns) {
    i64 b0, b1;
    if (!ns->n || !cls || !draft_class_block(a, text, cls, &b0, &b1)) return text;
    AvpStrVec open = {0}, close = {0};
    open_namespaces(&open, ns, a);
    close_namespaces(&close, ns, a);
    AvpStr head = avp_str_join(a, AVP_S("\n"), open.v, open.n);
    AvpStr tail = avp_str_join(a, AVP_S("\n"), close.v, close.n);
    return avp_str(avp_sprintf(a, "%.*s%.*s\n%.*s\n\n%.*s%.*s", (int)b0, text.p, AVP_SF(head),
                               (int)(b1 - b0), text.p + b0, AVP_SF(tail), (int)(text.n - (size_t)b1), text.p + b1));
}

/* One attempt: the draft of f from the matched twin tw. Raises SHSM_EXC_FINDING when the twin has no definition to
 * clone (a compiler-generated function, or one defined elsewhere). */
static DraftTwinMeta *make_twin_draft_from(DraftWorld *W, TwinsFn *f, TwinsFn *tw, const char *out) {
    AvpArena *a = W->a;
    const ShsmRepo *repo = W->repo;
    ShsmAsmIndex *ix = draft_world_ix(W);
    ShsmAsmBlock *tblock = shsm_asmindex_function(a, ix, shsm_target_addr(tw->address), &(ShsmAsmFile *){NULL});
    ShsmAsmBlock *gblock = shsm_asmindex_function(a, ix, shsm_target_addr(f->address), &(ShsmAsmFile *){NULL});
    if (!tblock || !gblock) shsm_raise(SHSM_EXC_TYPE, "'NoneType' object has no attribute 'rows'");
    DraftPairs pairs = {0};
    AvpStrVec problems = {0};
    draft_pair_relocations(a, tblock->rows.v, tblock->rows.n, gblock->rows.v, gblock->rows.n, &pairs, &problems);
    const char *twin_sym = tw->name, *target_sym = f->name;
    const char *tcls = NULL;
    draft_demangled_parts(a, twin_sym, &tcls);
    bool is_sinit = !strncmp(twin_sym, "__sinit_", 8);
    if (is_sinit) {
        /* a static initialiser registers the unit's class: the class named after the twin's source file */
        if (ends_cxx(tw->source)) {
            AvpStr root, ext;
            avp_path_splitext(avp_cz(a, avp_path_basename(tw->source)), &root, &ext);
            tcls = avp_cz(a, root);
        } else {
            tcls = NULL;
        }
    }
    const char *gcls = NULL, *why = NULL;
    const char *twin_unit = draft_plain_unit(a, tw->unit);
    if (tcls) gcls = draft_target_class(W, target_sym, f->address, &pairs, twin_unit, tcls, &why);
    DraftSubst *s = draft_build_subst(a, &pairs, draft_world_info, W, avp_str(twin_sym), tw->address,
                                      avp_str(target_sym), f->address, tcls, gcls, why);
    if (tcls && gcls && strncmp(gcls, "unk_", 4) != 0 && !draft_smap_get(s->strings, avp_str(tcls)).p)
        draft_subst_add(s, s->strings, "string", avp_str(tcls), avp_str(gcls),
                        avp_str(avp_sprintf(a, "the class name: %s", why)));
    for (size_t i = 0; i < problems.n; i++) draft_subst_unmapped(s, "row", avp_str(target_sym), problems.v[i]);
    draft_pair_vtables(W, tw->address, f->address, tcls, s);
    AvpStrVec ns = draft_namespaces_of(a, target_sym); /* the namespaces the target's class lives in */
    /* an `fn_` placeholder has no mangled class to take them from: it is a member of the twin's class's family */
    if (!ns.n && tcls && !strncmp(target_sym, "fn_", 3)) ns = draft_namespaces_of(a, twin_sym);
    /* a class that include/ already defines is the target's declaration: the draft includes it, it does not clone one */
    const char *known_hdr = (gcls && strncmp(twin_sym, "__sinit_", 8) != 0) ? unit_decls_class_header(draft_world_decls(W), gcls) : NULL;
    /* a member that the target's own header already defines in the class (`virtual ~ChannelSoftware() {}`) has its
     * body there: a draft would define it a second time, and a header definition is the project's, not a clone */
    if (known_hdr && gcls && strncmp(twin_sym, "__sinit_", 8) != 0) {
        const char *mcls = NULL;
        const char *member = draft_demangled_parts(a, target_sym, &mcls);
        AvpStr ktext;
        if (mcls && member && avp_read_text(a, avp_path_join(a, repo->include_dir, known_hdr), AVP_TM_UTF8_REPLACE, &ktext) &&
            draft_class_defines_member(a, ktext, gcls, member))
            shsm_raise(SHSM_EXC_FINDING, "%s%s: %s::%s has its body in the class, nothing to draft", DRAFT_INLINE_DEFINED,
                       known_hdr, gcls, member);
    }
    AvpStr src;
    read_text(a, avp_path_join(a, repo->root, tw->source), AVP_TM_UTF8_REPLACE, &src);
    const char *hdr_inc = tcls ? draft_class_header(a, repo->include_dir, src, tcls, tw->source) : NULL;
    AvpStr hdr = {NULL, 0};
    if (hdr_inc && !avp_read_text(a, avp_path_join(a, repo->include_dir, hdr_inc), AVP_TM_UTF8_REPLACE, &hdr))
        shsm_raise(SHSM_EXC_FINDING, "the twin %s includes %s, which is not under include/ (a library-local include path)",
                   twin_sym, hdr_inc);
    bool in_header = false;
    if (is_sinit && tcls) {
        i64 b0, b1;
        if (!hdr.p || !draft_class_block(a, hdr, tcls, &b0, &b1)) {
            /* the unit registers no class of its file's name: a plain initialiser (the display-height pair, say) */
            drop_class(s, tcls);
            tcls = gcls = why = hdr_inc = NULL;
            hdr = (AvpStr){NULL, 0};
        }
    }
    const DraftDef *d = NULL;
    if (!is_sinit) {
        d = draft_find_def(a, src, twin_sym, tw->address);
        if (!d && hdr.p) {
            d = draft_find_def(a, hdr, twin_sym, DRAFT_NONE);
            in_header = true;
        }
        if (!d)
            shsm_raise(SHSM_EXC_FINDING,
                       "the twin %s has no definition in %s or its header (compiler-generated, or defined elsewhere)",
                       twin_sym, tw->source);
    }
    AvpStr ext_root, ext_s;
    avp_path_splitext(tw->source, &ext_root, &ext_s);
    const char *ext = avp_cz(a, ext_s);
    const char *unit_dir = avp_cz(a, dirname_of(twin_unit));
    DraftUnitOf uo = draft_world_unit_of(W, f->address);
    const char *plan_path = uo.path;
    const char *profile_unit = twin_unit, *profile_why = "twin unit";
    {
        AvpStr g1, g2, rest;
        if (!avp_str_partition(avp_str(f->lib), AVP_S("/"), &g1, &rest)) g1 = avp_str(f->lib);
        if (!avp_str_partition(avp_str(tw->lib), AVP_S("/"), &g2, &rest)) g2 = avp_str(tw->lib);
        if (!avp_str_eq(g1, g2)) {
            /* a clone across libraries: the target's library decides the flags and the directory */
            DraftProfile p;
            if (draft_infer_profile(a, repo, f->address, f->lib, f->lib != NULL, &p) && p.unit && p.unit[0]) {
                profile_unit = draft_plain_unit(a, p.unit);
                profile_why = avp_pyfmt(a, "nearest source unit of library %s (the twin is in %s)", f->lib, tw->lib).p;
                unit_dir = avp_cz(a, dirname_of(profile_unit));
                if (plan_path && plan_path[0] &&
                    !avp_str_startswith(avp_str(plan_path), avp_str(avp_sprintf(a, "%s/", unit_dir))))
                    plan_path = NULL;
            }
        }
    }
    const char *new_path;
    if (gcls && gcls[0]) {
        new_path = avp_sprintf(a, "%s/%s", unit_dir, gcls);
    } else if (plan_path && plan_path[0]) {
        new_path = draft_strip_ext(a, plan_path);
    } else {
        AvpStr rest;
        AvpStr plain = draft_split_mangled(avp_str(target_sym), &rest);
        new_path = avp_pyfmt(a, "%s/%S", unit_dir, &plain).p;
    }
    AvpStr new_base = avp_path_basename(new_path);
    AvpStr body;
    if (is_sinit) {
        /* MWCC names a static initialiser after its file */
        AvpStr ext_us = avp_str_replace(a, avp_str(ext), AVP_S("."), AVP_S("_"), -1);
        draft_smap_set(a, s->renames, avp_str(target_sym), avp_pyfmt(a, "__sinit_\\%S%S", &new_base, &ext_us));
        DraftDefs defs0 = draft_scan_source(a, src);
        DraftStmts stmts0 = draft_top_level_statements(a, src, &defs0);
        AvpStrVec kept = {0};
        for (size_t i = 0; i < stmts0.n; i++) {
            AvpStr txt = stmts0.v[i].text;
            if (is_declaration(txt)) continue;
            ShsmReMatch m;
            if (shsm_re_search(shsm_re(ADDR_STMT_RE, 0), txt, 0, AVP_END, &m)) {
                AvpStr name = shsm_re_group(&m, 1);
                if (!draft_smap_get(s->idents, name).p) {
                    /* a twin global the initialiser does not touch (constant data): not the target's */
                    draft_subst_unmapped(s, "definition", name, AVP_S("twin global outside the initialiser: left out"));
                    continue;
                }
            }
            AvpStr raw = slice(src, stmts0.v[i].start, stmts0.v[i].end);
            AvpStr nopp = shsm_re_sub(a, shsm_re("^[ \\t]*#[^\\n]*\\n?", SHSM_RE_M), AVP_S(""), raw, 0);
            AvpStr stmt = draft_substitute(a, avp_str_strip(nopp), s);
            if (avp_str_strip_chars(stmt, AVP_S(" ;\n")).n) avp_vec_push(&kept, stmt);
        }
        /* no definitions of its own: everything the initialiser does comes from its includes */
        body = kept.n ? avp_str_join(a, AVP_S("\n"), kept.v, kept.n)
                      : AVP_S("// (the definitions come from the included headers)");
        if (!kept.n) {
            /* nothing of the twin's class is initialised: the clone needs no class (and no header) */
            drop_class(s, tcls);
            gcls = why = hdr_inc = NULL;
            hdr = (AvpStr){NULL, 0};
        }
    } else {
        AvpStr from = in_header ? hdr : src;
        body = draft_substitute(a, slice(from, d->start, d->end), s);
    }
    if (in_header) {
        /* an in-class definition becomes an out-of-line one */
        body = shsm_re_sub(a, shsm_re("^(\\s*)(?:virtual\\s+|static\\s+)?", 0), AVP_S("\\1"), body, 1);
        if (gcls && gcls[0]) {
            /* `~X() {}` -> `X::~X() {}`: the member name before the first parenthesis takes the class */
            i64 paren = avp_str_find(body, AVP_S("("), 0, AVP_END);
            if (paren > 0 && avp_str_find(avp_strn(body.p, (size_t)paren), AVP_S("::"), 0, AVP_END) < 0)
                body = shsm_re_sub(a, shsm_re("(~?[A-Za-z_]\\w*)(\\s*\\()", 0),
                                   avp_str(avp_sprintf(a, "%s::\\1\\2", gcls)), body, 1);
        }
    }
    /* helpers: non-addressed definitions of the twin source the body uses (transitively) */
    DraftDefs defs = draft_scan_source(a, src);
    AvpSet *used = avp_set_new();
    idents_of(body, used, NULL);
    bool *is_helper = AVP_NEWN(a, bool, defs.n + 1);
    memset(is_helper, 0, sizeof(bool) * (defs.n + 1));
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < defs.n; i++) {
            const DraftDef *hd = &defs.v[i];
            if (hd->address != DRAFT_NONE || is_helper[i]) continue;
            if (avp_set_has(used, avp_str(hd->name)) ||
                avp_set_has(used, draft_substitute(a, avp_str(hd->name), s))) {
                is_helper[i] = true;
                idents_of(draft_substitute(a, slice(src, hd->start, hd->end), s), used, NULL);
                changed = true;
            }
        }
    }
    AVP_VEC(const DraftDef *) helpers = {0};
    for (size_t i = 0; i < defs.n; i++)
        if (is_helper[i]) avp_vec_push(&helpers, &defs.v[i]);
    /* sorted by start: scan_source's order is the start order already; a stable sort keeps it */
    for (size_t i = 1; i < helpers.n; i++)
        for (size_t j = i; j > 0 && helpers.v[j - 1]->start > helpers.v[j]->start; j--) {
            const DraftDef *tmp = helpers.v[j];
            helpers.v[j] = helpers.v[j - 1];
            helpers.v[j - 1] = tmp;
        }
    const char *new_hdr_inc = NULL;
    AvpStr header_text = {NULL, 0};
    DraftStmts stmts = draft_top_level_statements(a, src, &defs);
    AvpStrVec externs = {0}, dropped = {0};
    AvpMap *owned = avp_map_new();
    for (size_t i = 0; i < stmts.n; i++) {
        AvpStr txt = stmts.v[i].text;
        AvpStrVec ids = {0};
        idents_of(txt, NULL, &ids);
        for (size_t k = 0; k < ids.n; k++) {
            if (!draft_addr_name(ids.v[k], NULL) || avp_map_has(owned, ids.v[k])) continue;
            const char *pat = avp_pyfmt(a, "\\b%S\\b\\s*(=|\\(|;|\\[)", &ids.v[k]).p;
            ShsmReMatch m;
            if (shsm_re_search(shsm_re_try(pat, 0), txt, 0, AVP_END, &m)) {
                AvpStr *pt = AVP_NEW(a, AvpStr);
                *pt = txt;
                avp_map_put(owned, ids.v[k], NULL)->p = pt;
            }
        }
        avp_vec_free(&ids);
    }
    if (hdr.p && known_hdr) {
        new_hdr_inc = known_hdr;
    } else if (hdr.p) {
        new_hdr_inc = gcls ? avp_sprintf(a, "%s/%s.h", unit_dir, gcls) : NULL;
        header_text = draft_wrap_class_block(a, draft_clone_header(a, hdr, hdr_inc, tcls, gcls, s, used, &dropped), gcls, &ns);
        if (in_header && gcls) {
            /* the body moved out of the class to the draft's .cpp: the class keeps the declaration only, or the
             * member would be defined twice (`virtual ~Segment() {}` in the header, `Segment::~Segment() {}` here) */
            const char *mcls = NULL;
            const char *member = draft_demangled_parts(a, target_sym, &mcls);
            if (mcls && member) header_text = draft_declare_member(a, header_text, member);
        }
    }
    {
        AvpMapIter it = {0};
        while (avp_map_next(s->idents, &it)) {
            AvpVal *ov = avp_map_get(owned, it.key);
            if (!ov) continue;
            AvpStr nw = *(AvpStr *)it.val->p;
            bool in_hdr = false;
            if (header_text.p) {
                AvpStr esc = shsm_re_escape(a, nw);
                AvpStr pat = avp_pyfmt(a, "\\b%S\\b", &esc);
                ShsmReMatch m;
                in_hdr = shsm_re_search(shsm_re_try_n(pat, 0), header_text, 0, AVP_END, &m);
            }
            if (in_hdr) continue;
            const char *e = draft_extern_of(a, draft_substitute(a, *(AvpStr *)ov->p, s), avp_cz(a, nw));
            if (e) avp_vec_push(&externs, avp_str(e));
        }
    }
    for (size_t i = 0; i < dropped.n; i++)
        draft_subst_unmapped(s, "declaration", dropped.v[i],
                             AVP_S("twin-owned global not referenced by the target: left out"));
    /* identifiers in the draft that still name a twin-specific placeholder */
    AvpSet *draft_ids = avp_set_new();
    idents_of(body, draft_ids, NULL);
    for (size_t i = 0; i < helpers.n; i++)
        idents_of(draft_substitute(a, slice(src, helpers.v[i]->start, helpers.v[i]->end), s), draft_ids, NULL);
    AvpSet *known_same = avp_set_new();
    for (size_t i = 0; i < pairs.n; i++)
        if (avp_str_eq(pairs.v[i].tsym, pairs.v[i].gsym)) {
            AvpStr rest;
            avp_set_add(known_same, draft_split_mangled(pairs.v[i].tsym, &rest));
        }
    AvpSet *ident_values = avp_set_new();
    {
        AvpMapIter it = {0};
        while (avp_map_next(s->idents, &it)) avp_set_add(ident_values, *(AvpStr *)it.val->p);
    }
    AvpStrVec sorted_ids = avp_set_sorted(draft_ids);
    for (size_t i = 0; i < sorted_ids.n; i++) {
        AvpStr name = sorted_ids.v[i];
        if (!draft_addr_name(name, NULL) || avp_set_has(ident_values, name) || avp_set_has(known_same, name)) continue;
        ShsmSymRow row;
        if (shsm_asmindex_symbol(a, ix, name, &row) &&
            !strcmp(draft_plain_unit(a, row.unit.p ? avp_cz(a, row.unit) : NULL), twin_unit))
            draft_subst_unmapped(s, "referent", name, AVP_S("owned by the twin unit, not among the paired relocations"));
    }
    /* the draft text */
    AvpStrVec lines = {0};
    const char *cls_note = gcls ? avp_pyfmt(a, " Class %s: %s.", gcls, why).p : "";
    avp_vec_push(&lines, avp_pyfmt(a, "// Twin draft of %s from %s (%s).%s", target_sym, twin_sym, tw->source, cls_note));
    if (strcmp(uo.kind, "unit") != 0) avp_vec_push(&lines, AVP_S("// unit name unconfirmed"));
    avp_vec_push(&lines, AVP_S(""));
    AvpStrVec incs = draft_includes_of(src);
    bool own_inc = false; /* the target class's header is among the includes */
    for (size_t i = 0; i < incs.n; i++) {
        if (hdr_inc && avp_str_eqz(incs.v[i], hdr_inc) && new_hdr_inc) {
            avp_vec_push(&lines, avp_pyfmt(a, "#include <%s>", new_hdr_inc));
            own_inc = true;
        } else {
            avp_vec_push(&lines, avp_pyfmt(a, "#include <%S>", &incs.v[i]));
            own_inc = own_inc || (known_hdr && avp_str_eqz(incs.v[i], known_hdr));
        }
    }
    if (known_hdr && !own_inc) avp_vec_push(&lines, avp_pyfmt(a, "#include <%s>", known_hdr)); /* the twin has no header of its own */
    avp_vec_push(&lines, AVP_S(""));
    for (size_t i = 0; i < externs.n; i++) avp_vec_push(&lines, externs.v[i]);
    if (externs.n) avp_vec_push(&lines, AVP_S(""));
    if (!is_sinit) open_namespaces(&lines, &ns, a);
    for (size_t i = 0; i < helpers.n; i++) {
        AvpStr h = draft_substitute(a, slice(src, helpers.v[i]->banner, helpers.v[i]->end), s);
        avp_vec_push(&lines, avp_str_strip_chars(h, AVP_S("\n")));
        avp_vec_push(&lines, AVP_S(""));
    }
    if (is_sinit) {
        avp_vec_push(&lines, avp_pyfmt(a, "// 0x%08X: the static initialiser of these definitions", f->address));
        avp_vec_push(&lines, body);
        avp_vec_push(&lines, AVP_S(""));
    } else {
        AvpStr ren = draft_smap_get(s->renames, avp_str(target_sym));
        const char *fcls;
        const char *fname = draft_demangled_parts(a, (ren.p && ren.n) ? avp_cz(a, ren) : target_sym, &fcls);
        avp_vec_push(&lines, AVP_S("/*"));
        avp_vec_push(&lines, AVP_S("===================="));
        avp_vec_push(&lines, avp_pyfmt(a, "%s%s", gcls ? avp_sprintf(a, "%s::", gcls) : "", fname));
        avp_vec_push(&lines, AVP_S("===================="));
        avp_vec_push(&lines, AVP_S("*/"));
        avp_vec_push(&lines, avp_pyfmt(a, "// 0x%08X", f->address));
        avp_vec_push(&lines, avp_str_strip_chars(body, AVP_S("\n")));
        avp_vec_push(&lines, AVP_S(""));
    }
    if (!is_sinit) close_namespaces(&lines, &ns, a);
    AvpStr text = avp_str_join(a, AVP_S("\n"), lines.v, lines.n);
    const char *ddir = draft_paths(a, repo, out, target_sym);
    if (avp_isdir(ddir) && !avp_rmtree(ddir, 0)) shsm_raise_io();
    if (!avp_makedirs(ddir, false)) shsm_raise_io();
    const char *src_out = avp_path_join(a, ddir, avp_pyfmt(a, "%S%s", &new_base, ext).p);
    write_text(a, src_out, text);
    const char *hdr_out = NULL;
    if (header_text.p && new_hdr_inc) {
        AvpStrVec parts = avp_str_split(avp_str(new_hdr_inc), AVP_S("/"), -1);
        const char *p = avp_path_join(a, ddir, "include");
        for (size_t i = 0; i < parts.n; i++) p = avp_path_join(a, p, avp_cz(a, parts.v[i]));
        hdr_out = p;
        if (!avp_makedirs(avp_cz(a, avp_path_dirname(hdr_out)), true)) shsm_raise_io();
        write_text(a, hdr_out, header_text);
    }
    write_text(a, avp_path_join(a, ddir, "subst.tsv"), draft_subst_tsv(a, s));
    ShsmSymRow sym_row;
    bool have_row = shsm_asmindex_symbol_at(a, ix, f->address, &sym_row);
    AvpStr ren = draft_smap_get(s->renames, avp_str(target_sym));
    const char *compiled = (ren.p && ren.n) ? avp_cz(a, ren) : target_sym;
    const char *unit_path = avp_sprintf(a, "%s%s", new_path, ext);
    AvpJson *m = avp_json_obj(a);
    avp_json_set(a, m, AVP_S("target"), avp_json_strz(a, target_sym));
    avp_json_set(a, m, AVP_S("address"), avp_json_strz(a, avp_sprintf(a, "0x%08llX", (unsigned long long)f->address)));
    avp_json_set(a, m, AVP_S("size"), avp_json_int(a, f->size));
    avp_json_set(a, m, AVP_S("lib"), jstr(a, f->lib));
    avp_json_set(a, m, AVP_S("twin"), avp_json_strz(a, twin_sym));
    avp_json_set(a, m, AVP_S("twin_address"),
                 avp_json_strz(a, avp_sprintf(a, "0x%08llX", (unsigned long long)tw->address)));
    avp_json_set(a, m, AVP_S("twin_source"), jstr(a, tw->source));
    avp_json_set(a, m, AVP_S("twin_level"), avp_json_strz(a, draft_twin_level(f, tw)));
    avp_json_set(a, m, AVP_S("profile"), avp_json_strz(a, profile_unit));
    avp_json_set(a, m, AVP_S("profile_why"), avp_json_strz(a, profile_why));
    avp_json_set(a, m, AVP_S("unit_path"), avp_json_strz(a, unit_path));
    avp_json_set(a, m, AVP_S("unit_kind"), avp_json_strz(a, uo.kind));
    avp_json_set(a, m, AVP_S("unit_confidence"), uo.has_conf ? avp_json_float(a, uo.conf) : avp_json_null());
    AvpJson *rng = avp_json_null();
    if (uo.has_range) {
        rng = avp_json_arr(a);
        avp_json_arr_push(a, rng, avp_json_strz(a, avp_sprintf(a, "0x%08llX", (unsigned long long)uo.start)));
        avp_json_arr_push(a, rng, avp_json_strz(a, avp_sprintf(a, "0x%08llX", (unsigned long long)uo.end)));
    }
    avp_json_set(a, m, AVP_S("unit_range"), rng);
    avp_json_set(a, m, AVP_S("class"), jstr(a, gcls));
    avp_json_set(a, m, AVP_S("class_evidence"), jstr(a, why));
    avp_json_set(a, m, AVP_S("source"), avp_json_strz(a, src_out));
    avp_json_set(a, m, AVP_S("header"), jstr(a, hdr_out));
    avp_json_set(a, m, AVP_S("include_dir"), hdr_out ? avp_json_strz(a, avp_path_join(a, ddir, "include")) : avp_json_null());
    avp_json_set(a, m, AVP_S("asm_file"), have_row ? jstrv(a, sym_row.file) : avp_json_null());
    avp_json_set(a, m, AVP_S("asm_unit"), have_row ? jstrv(a, sym_row.unit) : avp_json_null());
    avp_json_set(a, m, AVP_S("compiled"), avp_json_strz(a, compiled));
    avp_json_set(a, m, AVP_S("subst"), draft_subst_json(a, s));
    write_json(a, avp_path_join(a, ddir, "meta.json"), m);

    DraftTwinMeta *r = AVP_NEW(a, DraftTwinMeta);
    memset(r, 0, sizeof *r);
    r->meta = m;
    r->target = target_sym;
    r->twin = twin_sym;
    r->twin_source = tw->source;
    r->source = src_out;
    r->header = hdr_out;
    r->cls = gcls;
    r->class_evidence = why;
    r->unit_path = unit_path;
    r->unit_kind = uo.kind;
    r->has_range = uo.has_range;
    r->range_start = uo.start;
    r->range_end = uo.end;
    r->subst = s;
    r->twin_level = draft_twin_level(f, tw);
    return r;
}

DraftTwinMeta *draft_first_twin_draft(const TwinsFnVec *twins, DraftTwinAttempt attempt, void *ctx) {
    char first[1024] = "";
    int first_code = SHSM_EXC_FINDING;
    for (size_t i = 0; i < twins->n; i++) {
        AvpTry t;
        if (AVP_TRY(&t)) {
            DraftTwinMeta *r = attempt(ctx, twins->v[i]);
            avp_try_end(&t);
            return r;
        }
        /* a twin without a definition says nothing about the others; anything else is not about the twin */
        if (t.code != SHSM_EXC_FINDING && t.code != SHSM_EXC_TYPE) avp_throw(t.code, "%s", t.msg);
        if (!i) {
            first_code = t.code;
            snprintf(first, sizeof first, "%s", t.msg);
        }
    }
    if (twins->n > 1)
        avp_throw(first_code, "%s (%d matched twins tried, none could be cloned)", first, (int)twins->n);
    avp_throw(first_code, "%s", first);
}

/* (lib or "").split("/")[0] */
static AvpStr lib_family(const char *lib) {
    AvpStr s = avp_str(lib ? lib : "");
    i64 i = avp_str_find(s, AVP_S("/"), 0, AVP_END);
    return i >= 0 ? avp_strn(s.p, (size_t)i) : s;
}

typedef struct {
    DraftWorld *W;
    TwinsFn *f;
    const char *out;
} TwinAttempt;

static DraftTwinMeta *twin_attempt(void *ctx, TwinsFn *tw) {
    TwinAttempt *x = ctx;
    return make_twin_draft_from(x->W, x->f, tw, x->out);
}

DraftTwinMeta *draft_make_twin_draft(DraftWorld *W, const char *target, const char *out) {
    TwinsFnVec twins;
    TwinsFn *f = draft_choose_twin(W, target, &twins);
    /* a twin from another library family is usually a coincidence of a few instructions: it is tried after every
     * twin of the target's own family */
    TwinsFnVec order = {0};
    for (size_t pass = 0; pass < 2; pass++)
        for (size_t i = 0; i < twins.n; i++)
            if (avp_str_eq(lib_family(twins.v[i]->lib), lib_family(f->lib)) == (pass == 0)) avp_vec_push(&order, twins.v[i]);
    TwinAttempt x = {W, f, out};
    return draft_first_twin_draft(&order, twin_attempt, &x);
}

/* ---- the twin command ---------------------------------------------------------------------------------------------- */

void draft_twin_unit_range(DraftWorld *W, const char *text, i64 *start, i64 *end, const char **how) {
    AvpArena *a = W->a;
    if (draft_parse_range(avp_str(text), start, end)) {
        *how = "explicit";
        return;
    }
    const ShsmUnit *u = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        u = shsm_get_unit(W->repo, avp_sprintf(a, "main/%s", draft_plain_unit(a, text)));
        avp_try_end(&t);
    } else {
        if (t.code != SHSM_EXC_KEY && t.code != SHSM_EXC_UNIT_NOT_BUILT && t.code != SHSM_EXC_USAGE)
            avp_throw(t.code, "%s", t.msg);
        u = NULL;
    }
    if (u) {
        const ShsmSymbolTable *st = draft_world_st(W);
        const ShsmSplitUnit *su = st->splits ? shsm_splits_unit(st->splits, u->path) : NULL;
        if (su)
            for (size_t k = 0; k < su->nranges; k++)
                if (!strcmp(su->ranges[k].section, ".text")) {
                    *start = (i64)su->ranges[k].start;
                    *end = (i64)su->ranges[k].end;
                    *how = avp_sprintf(a, "split of %s", u->name);
                    return;
                }
        shsm_raise(SHSM_EXC_USAGE, "%s has no .text split: give the unit as START-END", u->name);
    }
    TwinsFn *f = NULL;
    if (AVP_TRY(&t)) {
        f = twins_resolve(draft_world_tw(W), text);
        avp_try_end(&t);
    } else {
        if (t.code != SHSM_EXC_USAGE) avp_throw(t.code, "%s", t.msg);
        AvpStr txt = avp_str(text);
        shsm_raise(SHSM_EXC_USAGE, "%s",
                   avp_pyfmt(a, "give the unit as START-END, a configured unit or a function inside it (%R is none of "
                                "these)",
                             &txt)
                       .p);
    }
    DraftUnitOf uo = draft_world_unit_of(W, f->address);
    if (!uo.has_range) shsm_raise(SHSM_EXC_USAGE, "no unit range for %s: give START-END", f->name);
    *start = uo.start;
    *end = uo.end;
    if (!strcmp(uo.kind, "plan"))
        *how = avp_pyfmt(a, "unit_plan proposal for %s%s", f->name,
                         uo.has_conf ? avp_pyfmt(a, " (%.2f)", uo.conf).p : "")
                   .p;
    else
        *how = avp_pyfmt(a, "%s range of %s", uo.kind, f->name).p;
}

static void data_meta(ShsmOutput *out, const AvpJson *meta) {
    for (size_t i = 0;; i++) {
        const AvpJsonMember *mb = avp_json_member(meta, i);
        if (!mb) break;
        shsm_output_data(out, avp_cz(avp_arena(), mb->key), mb->val);
    }
}

static int cmd_twin_unit(DraftWorld *W, ShsmOutput *out, const char *unit, const char *out_dir) {
    AvpArena *a = W->a;
    i64 rs, re_;
    const char *rng_how;
    draft_twin_unit_range(W, unit, &rs, &re_, &rng_how);
    const char *base = (out_dir && out_dir[0]) ? avp_path_abspath(a, out_dir) : draft_root(a, W->repo);
    const char *rng_txt = avp_sprintf(a, "%08llX-%08llX", (unsigned long long)rs, (unsigned long long)re_);
    const char *ddir = avp_path_join(a, base, avp_sprintf(a, "_unit_%s", rng_txt));
    AvpPtrVec fns = {0};
    shsm_sym_functions(draft_world_st(W), &fns);
    AVP_VEC(DraftTwinMeta *) metas = {0};
    AvpStrVec missing = {0};
    AvpSet *units_seen = avp_set_new();
    size_t nfns = 0;
    for (size_t i = 0; i < fns.n; i++) {
        const ShsmSymbol *sym = fns.v[i];
        if (!((i64)sym->address >= rs && (i64)sym->address < re_)) continue;
        nfns++;
        AvpTry t;
        if (AVP_TRY(&t)) {
            DraftTwinMeta *m = draft_make_twin_draft(W, sym->name, avp_path_join(a, ddir, draft_safe_name(a, sym->name)));
            avp_try_end(&t);
            avp_vec_push(&metas, m);
            avp_set_add(units_seen, avp_str(m->twin_source));
        } else {
            if (t.code != SHSM_EXC_FINDING && t.code != SHSM_EXC_USAGE) avp_throw(t.code, "%s", t.msg);
            avp_vec_push(&missing, avp_str(avp_sprintf(a, "%s: %s", sym->name, t.msg)));
        }
    }
    AvpStrVec seen_sorted = avp_set_sorted(units_seen);
    if (missing.n || avp_set_len(units_seen) != 1) {
        for (size_t i = 0; i < missing.n && i < 8; i++)
            shsm_output_line(out, avp_str_concat(a, AVP_S("no twin: "), missing.v[i]));
        AVP_VEC(AvpStr) reprs = {0};
        for (size_t i = 0; i < seen_sorted.n; i++) avp_vec_push(&reprs, avp_repr_str(a, seen_sorted.v[i]));
        AvpStr lst = avp_repr_list(a, reprs.v, reprs.n);
        shsm_output_line(out, avp_pyfmt(a,
                                        "not a cloned unit: %d of %d functions drafted in %08X-%08X (%s), twin "
                                        "sources %S",
                                        (i64)metas.n, (i64)nfns, rs, re_, rng_how, &lst));
        AvpJson *dr = avp_json_arr(a), *mi = avp_json_arr(a), *ts = avp_json_arr(a);
        for (size_t i = 0; i < metas.n; i++) avp_json_arr_push(a, dr, avp_json_strz(a, metas.v[i]->target));
        for (size_t i = 0; i < missing.n; i++) avp_json_arr_push(a, mi, avp_json_str(a, missing.v[i]));
        for (size_t i = 0; i < seen_sorted.n; i++) avp_json_arr_push(a, ts, avp_json_str(a, seen_sorted.v[i]));
        shsm_output_data(out, "drafted", dr);
        shsm_output_data(out, "missing", mi);
        shsm_output_data(out, "twin_sources", ts);
        return SHSM_EXIT_FINDING;
    }
    /* one twin unit: concatenate the function drafts under one include block */
    DraftTwinMeta *first = metas.v[0];
    AvpStrVec body = {0};
    AvpSet *seen = avp_set_new();
    for (size_t i = 0; i < metas.n; i++) {
        AvpStr txt;
        read_text(a, metas.v[i]->source, AVP_TM_UTF8, &txt);
        AvpStrVec ls = avp_str_split(txt, AVP_S("\n"), -1);
        for (size_t k = 0; k < ls.n; k++) {
            AvpStr line = ls.v[k];
            bool inc = avp_str_startswith(line, AVP_S("#include"));
            if (inc || avp_str_startswith(line, AVP_S("// Twin draft")) || avp_str_startswith(line, AVP_S("// unit"))) {
                if (inc) avp_set_add(seen, line);
                continue;
            }
            avp_vec_push(&body, line);
        }
    }
    AvpStrVec incs = avp_set_sorted(seen);
    AvpStr inc_txt = avp_str_join(a, AVP_S("\n"), incs.v, incs.n);
    AvpStr body_txt = avp_str_join(a, AVP_S("\n"), body.v, body.n);
    AvpStr text = avp_pyfmt(a, "// Twin draft of %s from %s.\n// unit name unconfirmed\n\n%S\n%S", rng_txt,
                            first->twin_source, &inc_txt, &body_txt);
    const char *dst = avp_path_join(a, ddir, avp_cz(a, avp_path_basename(first->source)));
    write_text(a, dst, text);
    /* dict(first, source=..., target=..., unit_range=..., unit_kind=...): the keys keep their places */
    AvpJson *meta = avp_json_obj(a);
    for (size_t i = 0;; i++) {
        const AvpJsonMember *mb = avp_json_member(first->meta, i);
        if (!mb) break;
        avp_json_set(a, meta, mb->key, mb->val);
    }
    avp_json_set(a, meta, AVP_S("source"), avp_json_strz(a, dst));
    avp_json_set(a, meta, AVP_S("target"), avp_json_strz(a, rng_txt));
    AvpJson *ur = avp_json_arr(a);
    avp_json_arr_push(a, ur, avp_json_strz(a, avp_sprintf(a, "0x%08llX", (unsigned long long)rs)));
    avp_json_arr_push(a, ur, avp_json_strz(a, avp_sprintf(a, "0x%08llX", (unsigned long long)re_)));
    avp_json_set(a, meta, AVP_S("unit_range"), ur);
    avp_json_set(a, meta, AVP_S("unit_kind"), avp_json_strz(a, rng_how));
    write_json(a, avp_path_join(a, ddir, "meta.json"), meta);
    shsm_output_line(out, avp_pyfmt(a, "unit draft %s (%d functions) from %s; range %08X-%08X (%s)",
                                    draft_rel_path(a, W->repo, dst), (i64)metas.n, first->twin_source, rs, re_,
                                    rng_how));
    data_meta(out, meta);
    return SHSM_EXIT_OK;
}

int draft_cmd_twin(DraftWorld *W, ShsmOutput *out, const char *function, const char *unit, const char *out_dir) {
    AvpArena *a = W->a;
    if (unit && unit[0]) return cmd_twin_unit(W, out, unit, out_dir);
    if (!function || !function[0]) shsm_raise(SHSM_EXC_USAGE, "give a function or --unit");
    const char *dir = NULL;
    if (out_dir && out_dir[0]) {
        /* one subdirectory per target: two drafts given the same --out never overwrite each other */
        dir = avp_path_join(a, avp_path_abspath(a, out_dir),
                            draft_safe_name(a, twins_resolve(draft_world_tw(W), function)->name));
    }
    DraftTwinMeta *m = draft_make_twin_draft(W, function, dir);
    const DraftSubst *s = m->subst;
    shsm_output_line(out, avp_pyfmt(a, "draft %s <- twin %s (%s, %s level)", m->target, m->twin, m->twin_source,
                                        m->twin_level));
    shsm_output_line(out, avp_pyfmt(a, "source  %s", draft_rel_path(a, W->repo, m->source)));
    if (m->header) shsm_output_line(out, avp_pyfmt(a, "header  %s", draft_rel_path(a, W->repo, m->header)));
    if (m->cls) shsm_output_line(out, avp_pyfmt(a, "class   %s (%s)", m->cls, m->class_evidence));
    const char *rng = m->has_range ? avp_sprintf(a, " 0x%08llX-0x%08llX", (unsigned long long)m->range_start,
                                                 (unsigned long long)m->range_end)
                                   : "";
    shsm_output_line(out, avp_pyfmt(a, "unit    %s (%s%s)", m->unit_path, m->unit_kind, rng));
    shsm_output_line(out, avp_pyfmt(a, "substitutions %d, unmapped %d, conflicts %d (subst.tsv)", (i64)s->evidence.n,
                                    (i64)s->unmapped.n, (i64)s->conflicts.n));
    for (size_t i = 0; i < s->unmapped.n && i < 4; i++) {
        const DraftUnmapped *u = &s->unmapped.v[i];
        shsm_output_line(out, avp_pyfmt(a, "  unmapped %S %S: %S", &u->kind, &u->name, &u->why));
    }
    data_meta(out, m->meta);
    return SHSM_EXIT_OK;
}
