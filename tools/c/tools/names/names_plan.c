/* names.py rename planning: select_symbols, plan_symbols_only, plan_rename, plan_class_rename, edit_targets,
 * compute_edits, special_line, add_member_decls, write_plan, Plan.to_dict. */
#include "names.h"
#include "names_priv.h"

#include "avp/fmt.h"
#include "avp/fs.h"
#include "avp/sort.h"
#include "shsm_mangle.h"
#include "shsm_re.h"
#include "shsm_text.h"

#include <string.h>

#define IDENT_RE "^[A-Za-z_][A-Za-z0-9_]*$"
#define GEN_RE "^(?:@\\d+@\\S+|@\\d+|@stringBase\\d+|__sinit_\\\\\\S+_cpp|(?:dtor|sinit|jumptable|gap)_\\S+)$"

static bool re_matches(const char *pattern, const char *s) {
    return shsm_re_match(shsm_re(pattern, 0), avp_str(s), 0, AVP_END, NULL);
}

static void push(AvpArena *a, AvpStrVec *v, AvpStr s) { avp_vec_push(v, avp_str_dup(a, s)); }

static NamesPlan *plan_new(NamesCtx *cx) {
    NamesPlan *p = AVP_NEW(cx->a, NamesPlan);
    p->cx = cx;
    p->idents = avp_map_new();
    return p;
}

static void set_ident(NamesPlan *p, const char *old, const char *new_) {
    avp_map_put(p->idents, avp_str(old), NULL)->p = (void *)new_;
}

static const char *ident_of(const NamesPlan *p, AvpStr old) {
    AvpVal *v = avp_map_get(p->idents, old);
    return v ? (const char *)v->p : NULL;
}

NamesFile *names_plan_file(const NamesPlan *p, const char *rel) {
    for (size_t i = 0; i < p->files.n; i++)
        if (strcmp(p->files.v[i]->rel, rel) == 0) return p->files.v[i];
    return NULL;
}

NamesFile *names_plan_file_add(NamesPlan *p, const char *rel) {
    NamesFile *f = names_plan_file(p, rel);
    if (f) return f;
    f = AVP_NEW(p->cx->a, NamesFile);
    f->rel = avp_strdup(p->cx->a, rel);
    avp_vec_push(&p->files, f);
    return f;
}

static const char *join_scope(AvpArena *a, const char *const *sc, size_t n) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < n; i++) {
        if (i) avp_buf_putz(&b, "::");
        avp_buf_putz(&b, sc[i]);
    }
    return avp_buf_cz(&b);
}

/* ---- select_symbols ---- */

static AvpPtrVec select_symbols(NamesCtx *cx, const char *old) {
    AvpPtrVec exact = {0};
    if (names_all_by_name(cx, old, &exact)) return exact;
    AvpStrVec parts = avp_str_split(avp_str(old), AVP_S("::"), -1);
    AvpStr plain = parts.v[parts.n - 1];
    size_t k = parts.n - 1;
    const char **scope = AVP_NEWN(cx->a, const char *, k + 1);
    for (size_t i = 0; i < k; i++) scope[i] = avp_cz(cx->a, parts.v[i]);
    if (plain.n == 0) shsm_raise(SHSM_EXC_USAGE, "bad name %s", avp_cz(cx->a, avp_repr_str(cx->a, avp_str(old))));
    const AvpPtrVec *all = names_by_plain(cx, avp_cz(cx->a, plain));
    AvpPtrVec found = {0};
    for (size_t i = 0; i < all->n; i++) {
        const ShsmSymbol *s = all->v[i];
        const NamesPlain *pl = names_sym_plain(cx, s);
        bool ok = k == 0 || names_scope_eq(pl->scope, pl->nscope, scope, k) ||
                  (pl->nscope >= k && names_scope_eq(pl->scope + pl->nscope - k, k, scope, k));
        if (ok) avp_vec_push(&found, (void *)s);
    }
    if (!found.n)
        shsm_raise(SHSM_EXC_USAGE, "no symbol named %s in symbols.txt",
                   avp_cz(cx->a, avp_repr_str(cx->a, avp_str(old))));
    return found;
}

/* ---- refusal text ---- */

/* Every refusal ends with the rule that made it and the flag, if any, that lifts it. */
static const char *rule_tag(AvpArena *a, const char *rule, const char *lift) {
    if (lift) return avp_sprintf(a, "[rule %s; lifted by %s]", rule, lift);
    return avp_sprintf(a, "[rule %s; no flag lifts it]", rule);
}

static bool is_local_sym(const ShsmSymbol *s) { return s->scope && strcmp(s->scope, "local") == 0; }

static bool renamed_in(const NamesPlan *p, const ShsmSymbol *o) {
    for (size_t i = 0; i < p->symbols.n; i++)
        if (p->symbols.v[i].sym == o) return true;
    return false;
}

static const char *unit_text(const NamesCtx *cx, const ShsmSymbol *s) {
    const char *u = shsm_sym_unit_of(cx->st, s->address);
    return u ? u : "(no unit)";
}

/* ---- name collisions ---- */

/* A new name collides only with the exact name already present with non-local scope, or with a scope:local symbol of
 * that name inside the same unit's ranges (splits.txt): statics are per object. A symbol of the plan that is itself
 * renamed away is not a collision. */
static void check_collisions(NamesPlan *p, bool local) {
    NamesCtx *cx = p->cx;
    AvpArena *a = cx->a;
    for (size_t k = 0; k < p->symbols.n; k++) {
        const ShsmSymbol *s = p->symbols.v[k].sym;
        const char *nn = p->symbols.v[k].new_name;
        AvpPtrVec same = {0}, live = {0};
        names_all_by_name(cx, nn, &same);
        for (size_t i = 0; i < same.n; i++) {
            const ShsmSymbol *o = same.v[i];
            if (o != s && renamed_in(p, o)) continue;
            avp_vec_push(&live, (void *)o);
        }
        avp_vec_free(&same);
        if (!live.n) {
            avp_vec_free(&live);
            continue;
        }
        const ShsmSymbol *nonlocal = NULL;
        for (size_t i = 0; i < live.n && !nonlocal; i++)
            if (!is_local_sym(live.v[i])) nonlocal = live.v[i];
        if (nonlocal) {
            push(a, &p->conflicts,
                 avp_pyfmt(a, "%s already exists in symbols.txt (0x%08X) %s", nn, nonlocal->address,
                           rule_tag(a, "exact-name", NULL)));
        } else if (!(local || is_local_sym(s))) {
            push(a, &p->conflicts,
                 avp_pyfmt(a,
                           "%s already exists in symbols.txt only as scope:local symbols (%d, first 0x%08X) and the renamed "
                           "symbol is not local: a global of that name next to the statics is ambiguous by name; pass "
                           "--local to make the renamed line scope:local as well %s",
                           nn, live.n, ((const ShsmSymbol *)live.v[0])->address,
                           rule_tag(a, "local-needs-flag", "--local")));
        } else {
            const char *us = shsm_sym_unit_of(cx->st, s->address);
            bool clash = false;
            AvpBuf others;
            avp_buf_init(&others, a);
            for (size_t i = 0; i < live.n; i++) {
                const ShsmSymbol *o = live.v[i];
                const char *uo = shsm_sym_unit_of(cx->st, o->address);
                if (us && uo && strcmp(us, uo) == 0) {
                    clash = true;
                    push(a, &p->conflicts,
                         avp_pyfmt(a, "two scope:local symbols named %s would sit in unit %s (0x%08X and 0x%08X) %s", nn,
                                   us, s->address, o->address, rule_tag(a, "local-in-unit", NULL)));
                } else {
                    if (avp_buf_str(&others).n) avp_buf_putz(&others, ", ");
                    avp_buf_put(&others, avp_pyfmt(a, "0x%08X in %s", o->address, unit_text(cx, o)));
                }
            }
            if (!clash)
                push(a, &p->notes,
                     avp_pyfmt(a,
                               "%s exists only as scope:local symbols in other units (%s); the renamed line is "
                               "scope:local too%s%s",
                               nn, avp_buf_cz(&others), us ? ", in " : "", us ? us : ""));
        }
        avp_vec_free(&live);
    }
}

static NamesPlan *plan_symbols_only(NamesCtx *cx, const AvpPtrVec *cands, const char *new_, bool local,
                                    const char *why) {
    AvpArena *a = cx->a;
    NamesPlan *p = plan_new(cx);
    p->local = local;
    if (!re_matches("^\\S+$", new_))
        shsm_raise(SHSM_EXC_USAGE, "%s is not a symbol name", avp_cz(a, avp_repr_str(a, avp_str(new_))));
    if (cands->n != 1)
        push(a, &p->conflicts,
             avp_pyfmt(a, "%d symbols match; a symbols-only rename needs exactly one (give the full name) %s", cands->n,
                       rule_tag(a, "one-symbol", NULL)));
    AvpPtrVec same = {0};
    for (size_t i = 0; i < cands->n; i++) {
        const ShsmSymbol *s = cands->v[i];
        size_t n = names_all_by_name(cx, s->name, &same);
        if (n > 1)
            push(a, &p->conflicts,
                 avp_pyfmt(a, "%s is defined %d times in symbols.txt (repeated local name) %s", s->name, n,
                           rule_tag(a, "repeated-name", NULL)));
        NamesSymRename r = {s, new_};
        avp_vec_push(&p->symbols, r);
    }
    check_collisions(p, local);
    avp_vec_free(&same);
    push(a, &p->notes,
         avp_pyfmt(a, "symbols.txt only (%s); source, headers, docs and config are not edited", why));
    return p;
}

/* The rename that has no source spelling (a vtable, a template, a destructor, a mangled new name): symbols.txt only,
 * as with --symbols-only, and the source sites naming the old placeholder are listed for a human (compute_edits). */
static NamesPlan *fallback_symbols_only(NamesCtx *cx, const AvpPtrVec *cands, const char *new_, bool local,
                                        const char *old_plain, const char *why) {
    AvpArena *a = cx->a;
    NamesPlan *p = plan_symbols_only(cx, cands, new_, local, "automatic");
    push(a, &p->notes,
         avp_pyfmt(a, "source was not touched: %s; symbols.txt was renamed alone (the same as --symbols-only)", why));
    if (old_plain) avp_vec_push(&p->site_words, avp_str(old_plain));
    for (size_t i = 0; i < cands->n; i++) {
        AvpStr n = avp_str(((const ShsmSymbol *)cands->v[i])->name);
        bool dup = false;
        for (size_t k = 0; k < p->site_words.n && !dup; k++) dup = avp_str_eq(p->site_words.v[k], n);
        if (!dup) avp_vec_push(&p->site_words, n);
    }
    return p;
}

/* ---- plan_rename ---- */

static bool special_raw(const char *raw) {
    /* raw.startswith("__") and ^__(ct|dt|vt|op|nw|dl|nwa|dla|[a-z]{2,3})$ */
    return re_matches("^__(ct|dt|vt|op|nw|dl|nwa|dla|[a-z]{2,3})$", raw) && strncmp(raw, "__", 2) == 0;
}

static const char *addr_list(AvpArena *a, const AvpPtrVec *syms) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < syms->n; i++) {
        if (i) avp_buf_putz(&b, ", ");
        avp_buf_put(&b, avp_pyfmt(a, "0x%08X", ((const ShsmSymbol *)syms->v[i])->address));
    }
    return avp_buf_cz(&b);
}

/* Two renames of one plan that end in one name, and one address that would carry two names for the same function. */
static void check_targets(NamesPlan *p) {
    NamesCtx *cx = p->cx;
    AvpArena *a = cx->a;
    for (size_t i = 0; i < p->symbols.n; i++)
        for (size_t j = i + 1; j < p->symbols.n; j++) {
            const NamesSymRename *x = &p->symbols.v[i], *y = &p->symbols.v[j];
            if (strcmp(x->new_name, y->new_name) != 0) continue;
            if (is_local_sym(x->sym) && is_local_sym(y->sym)) {
                const char *ux = shsm_sym_unit_of(cx->st, x->sym->address), *uy = shsm_sym_unit_of(cx->st, y->sym->address);
                if (!ux || !uy || strcmp(ux, uy) != 0) continue;
            }
            push(a, &p->conflicts,
                 avp_pyfmt(a, "two symbols of this plan would both be named %s (0x%08X and 0x%08X) %s", x->new_name,
                           x->sym->address, y->sym->address, rule_tag(a, "duplicate-target", NULL)));
        }
    for (size_t i = 0; i < p->symbols.n; i++) {
        const ShsmSymbol *s = p->symbols.v[i].sym;
        const char *nn = p->symbols.v[i].new_name;
        if (!s->type || strcmp(s->type, "function") != 0) continue;
        NamesPlain np = names_plain_of(a, nn);
        AvpPtrVec at = {0};
        shsm_sym_at(cx->st, s->address, s->section, &at);
        for (size_t k = 0; k < at.n; k++) {
            const ShsmSymbol *o = at.v[k];
            if (o == s || renamed_in(p, o) || !o->type || strcmp(o->type, "function") != 0) continue;
            const NamesPlain *po = names_sym_plain(cx, o);
            if (strcmp(po->plain, np.plain) != 0 || strcmp(o->name, nn) == 0) continue;
            push(a, &p->conflicts,
                 avp_pyfmt(a, "0x%08X would carry two names for %s: %s (renamed) and %s %s", s->address, np.plain, nn,
                           o->name, rule_tag(a, "two-names", NULL)));
        }
        avp_vec_free(&at);
    }
}

/* ---- signature changes and override families ---- */

static ShsmDemangled *parse_or_null(AvpArena *a, const char *name) {
    ShsmDemangled *d = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        d = shsm_mangle_parse(a, name);
        avp_try_end(&t);
    }
    return d;
}

/* The part of a member's mangled name after "<raw>__<class>": "Fv", "CFi", ... (the parameter codes with the const
 * flag); NULL when the name does not have that shape. */
static const char *member_codes(AvpArena *a, const char *name, const NamesPlain *pl) {
    size_t rl = strlen(pl->raw);
    if (strncmp(name, pl->raw, rl) != 0 || strncmp(name + rl, "__", 2) != 0) return NULL;
    const char *rest = name + rl + 2;
    if (!pl->nscope) return rest;
    const char *cp = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        cp = shsm_mangle_scope_parts(a, pl->scope, pl->nscope);
        avp_try_end(&t);
    }
    size_t cl = cp ? strlen(cp) : 0;
    if (!cp || strncmp(rest, cp, cl) != 0) return NULL;
    return rest + cl;
}

static const char *member_class_part(AvpArena *a, const NamesPlain *pl) {
    return pl->nscope ? shsm_mangle_scope_parts(a, pl->scope, pl->nscope) : "";
}

static const char *last_scope(const NamesPlain *pl) { return pl->nscope ? pl->scope[pl->nscope - 1] : NULL; }

static void add_unique(AvpArena *a, AvpStrVec *v, AvpStr s) {
    for (size_t i = 0; i < v->n; i++)
        if (avp_str_eq(v->v[i], s)) return;
    avp_vec_push(v, avp_str_dup(a, s));
}

NamesPlan *names_plan_rename(NamesCtx *cx, const char *old, const char *new_, bool local, bool symbols_only) {
    NamesRenameOpts o = {.local = local, .symbols_only = symbols_only};
    return names_plan_rename_opts(cx, old, new_, &o);
}

NamesPlan *names_plan_rename_opts(NamesCtx *cx, const char *old, const char *new_, const NamesRenameOpts *opts) {
    AvpArena *a = cx->a;
    bool local = opts->local, symbols_only = opts->symbols_only;
    if (opts->signature && symbols_only)
        shsm_raise(SHSM_EXC_USAGE, "--signature and --symbols-only exclude each other: --symbols-only already takes "
                                   "any new name and leaves the source alone");
    if (opts->family && symbols_only)
        shsm_raise(SHSM_EXC_USAGE, "--family and --symbols-only exclude each other: a family rename edits the source");
    AvpPtrVec cands = select_symbols(cx, old);
    ShsmDemangled *sig_new = NULL;
    if (opts->signature) {
        sig_new = parse_or_null(a, new_);
        if (!sig_new || *sig_new->thunk || (sig_new->local && *sig_new->local) || sig_new->is_data)
            shsm_raise(SHSM_EXC_USAGE,
                       "--signature: %s does not demangle as a CodeWarrior function name; give the new name as the "
                       "full mangled name %s",
                       avp_cz(a, avp_repr_str(a, avp_str(new_))), rule_tag(a, "signature-demangle", NULL));
    }
    bool gen_old = true;
    for (size_t i = 0; i < cands.n; i++)
        if (!re_matches(GEN_RE, ((const ShsmSymbol *)cands.v[i])->name)) gen_old = false;
    bool gen_new = re_matches(GEN_RE, new_);
    if (gen_new && !(gen_old || symbols_only))
        shsm_raise(SHSM_EXC_USAGE,
                   "%s is a compiler-generated name form with no source spelling; rename only generated symbols to "
                   "it, or pass --symbols-only",
                   avp_cz(a, avp_repr_str(a, avp_str(new_))));
    if (symbols_only || gen_old)
        return plan_symbols_only(cx, &cands, new_, local,
                                 symbols_only ? "--symbols-only" : "compiler-generated name, no source spelling");
    NamesPlan *p = plan_new(cx);
    p->local = local;
    p->signature = opts->signature;
    bool sig_mode = opts->signature;
    /* the symbol the user named (the first one), before --family widens the set */
    const ShsmSymbol *given = cands.v[0];
    size_t given_n = cands.n;
    if (opts->family) {
        const NamesPlain *gp = names_sym_plain(cx, given);
        if (!gp->nscope || strcmp(gp->plain, given->name) == 0)
            shsm_raise(SHSM_EXC_USAGE, "--family needs a class member (a mangled name with a class): %s is not one %s",
                       given->name, rule_tag(a, "family-member", NULL));
        AvpPtrVec fam = {0};
        const AvpPtrVec *byp = names_by_plain(cx, gp->plain);
        for (size_t i = 0; i < byp->n; i++) {
            const ShsmSymbol *s = byp->v[i];
            const NamesPlain *pl = names_sym_plain(cx, s);
            if (pl->nscope && strcmp(pl->plain, s->name) != 0 && strcmp(pl->raw, gp->raw) == 0)
                avp_vec_push(&fam, (void *)s);
        }
        avp_vec_free(&cands);
        cands = fam;
        if (cands.n < 2)
            push(a, &p->notes, avp_pyfmt(a, "--family: no other symbol has the plain name %s; the family is one symbol",
                                         gp->plain));
        else
            push(a, &p->notes,
                 avp_pyfmt(a, "--family: %d symbols share the plain name %s and are renamed together, each keeping its "
                               "class",
                           cands.n, gp->plain));
    }
    /* names = sorted({s.name for s in cands}) */
    AvpStrVec names = {0};
    {
        AvpSet *seen = avp_set_new();
        for (size_t i = 0; i < cands.n; i++) avp_set_add(seen, avp_str(((const ShsmSymbol *)cands.v[i])->name));
        names = avp_set_sorted(seen);
        for (size_t i = 0; i < names.n; i++) names.v[i] = avp_str_dup(a, names.v[i]);
        avp_set_free(seen);
    }
    AvpPtrVec same = {0};
    for (size_t i = 0; i < names.n; i++) {
        const char *n = avp_cz(a, names.v[i]);
        size_t cnt = names_all_by_name(cx, n, &same);
        if (cnt > 1)
            push(a, &p->conflicts,
                 avp_pyfmt(a, "%s is defined %d times in symbols.txt (repeated local name): %s %s", n, cnt,
                           addr_list(a, &same), rule_tag(a, "repeated-name", NULL)));
    }
    {
        /* len({tuple(plain_of(s.name)[1]) for s in cands}) */
        AvpSet *scopes = avp_set_new();
        for (size_t i = 0; i < cands.n; i++) {
            const NamesPlain *pl = names_sym_plain(cx, cands.v[i]);
            AvpBuf b;
            avp_buf_init(&b, a);
            for (size_t k = 0; k < pl->nscope; k++) {
                avp_buf_putz(&b, pl->scope[k]);
                avp_buf_putc(&b, '\0');
            }
            avp_buf_put(&b, avp_pyfmt(a, "%d", pl->nscope));
            avp_set_add(scopes, avp_buf_str(&b));
        }
        size_t ns = avp_set_len(scopes);
        avp_set_free(scopes);
        if (ns > 1 && !opts->family) {
            AvpBuf b;
            avp_buf_init(&b, a);
            for (size_t i = 0; i < cands.n && i < 8; i++) {
                if (i) avp_buf_putz(&b, ", ");
                avp_buf_putz(&b, ((const ShsmSymbol *)cands.v[i])->name);
            }
            push(a, &p->conflicts,
                 avp_pyfmt(a,
                           "%r names symbols in %d scopes: %s (qualify it as Scope::name; or, if they are the overrides "
                           "of one virtual slot, pass --family to rename them together) %s",
                           old, ns, avp_buf_cz(&b), rule_tag(a, "one-scope", "--family")));
        }
    }
    const ShsmSymbol *c0 = given;
    const NamesPlain *pl0 = names_sym_plain(cx, c0);
    const char *p0 = pl0->plain, *raw0 = pl0->raw;
    if (special_raw(raw0))
        return fallback_symbols_only(
            cx, &cands, new_, local, NULL,
            avp_cz(a, avp_pyfmt(a, "%s is a special member (%s) with no plain source spelling", c0->name, p0)));
    /* the new plain name (or a full mangled name with the same signature) */
    AvpStrVec nparts = avp_str_split(avp_str(new_), AVP_S("::"), -1);
    const char *new_plain = avp_cz(a, nparts.v[nparts.n - 1]);
    if (strstr(new_, "::")) {
        bool eq = nparts.n - 1 == pl0->nscope;
        for (size_t i = 0; eq && i < pl0->nscope; i++)
            if (!avp_str_eqz(nparts.v[i], pl0->scope[i])) eq = false;
        if (!eq)
            shsm_raise(SHSM_EXC_USAGE, "a rename keeps the scope (%s); use --class to rename a class",
                       join_scope(a, pl0->scope, pl0->nscope));
    }
    const char *new_full = NULL;
    if (!re_matches(IDENT_RE, new_plain))
        return fallback_symbols_only(
            cx, &cands, new_, local, p0,
            avp_cz(a, avp_pyfmt(a, "%s is not an identifier (a template, vtable or other name with no plain source "
                                   "spelling)",
                                avp_cz(a, avp_repr_str(a, avp_str(new_plain))))));
    NamesPlain np = names_plain_of(a, new_plain);
    if (strcmp(np.plain, new_plain) == 0 && re_matches("^\\w+__(?:Q?\\d|F|CF|t\\d|PF)\\w*$", new_plain))
        return fallback_symbols_only(
            cx, &cands, new_, local, p0,
            avp_cz(a, avp_pyfmt(a, "%s is a mangled C++ name: its mangled form must not be written into source",
                                new_plain)));
    NamesSpecial *special = NULL;
    if (strcmp(np.plain, new_plain) != 0) {
        ShsmDemangled *dn = shsm_mangle_parse(a, new_plain);
        if (dn != NULL && (strcmp(dn->raw_name, "__ct") == 0 || strcmp(dn->raw_name, "__dt") == 0) && dn->nscope &&
            !*dn->thunk) {
            special = AVP_NEW(a, NamesSpecial);
            special->kind = dn->raw_name;
            special->cls = dn->scope[dn->nscope - 1];
            special->qual = join_scope(a, dn->scope, dn->nscope);
        } else if (dn != NULL && (strncmp(dn->raw_name, "__", 2) == 0 || !re_matches(IDENT_RE, np.plain))) {
            return fallback_symbols_only(
                cx, &cands, new_, local, p0,
                avp_cz(a, avp_pyfmt(a, "%s is a special member (%s): the source-side rewrite cannot be done correctly",
                                    new_plain, np.plain)));
        }
    }
    const char *const *new_scope = pl0->scope;
    size_t new_nscope = pl0->nscope;
    if (sig_mode && strcmp(np.plain, new_plain) == 0)
        shsm_raise(SHSM_EXC_USAGE, "--signature: %s does not split into a name and a signature %s",
                   avp_cz(a, avp_repr_str(a, avp_str(new_))), rule_tag(a, "signature-demangle", NULL));
    const char *old_codes = NULL, *new_codes = NULL;
    if (strcmp(np.plain, new_plain) != 0) { /* given a mangled name */
        bool unsigned_ = given_n == 1 && strcmp(p0, c0->name) == 0;
        if (opts->family && !sig_mode)
            shsm_raise(SHSM_EXC_USAGE,
                       "--family without --signature renames the plain name of every override: give the new plain name, "
                       "not the mangled %s (to change the parameters of the whole family, add --signature) %s",
                       avp_cz(a, avp_repr_str(a, avp_str(new_plain))), rule_tag(a, "family-name", "--signature"));
        if (sig_mode && given_n != 1)
            shsm_raise(SHSM_EXC_USAGE,
                       "--signature: %s names %d symbols; give the full mangled name of the one symbol (with --family "
                       "its overrides follow) %s",
                       avp_cz(a, avp_repr_str(a, avp_str(old))), given_n, rule_tag(a, "one-symbol", NULL));
        if (!unsigned_ && !sig_mode) {
            size_t nr = strlen(np.raw), r0 = strlen(raw0);
            const char *tail_new = strlen(new_plain) >= nr ? new_plain + nr : "";
            const char *tail_old = strlen(c0->name) >= r0 ? c0->name + r0 : "";
            if (given_n != 1 || strcmp(tail_new, tail_old) != 0)
                shsm_raise(SHSM_EXC_USAGE,
                           "%s does not have the signature of %s (a rename changes only the name; --signature "
                           "allows a new class or new parameter codes) %s",
                           new_plain, c0->name, rule_tag(a, "same-signature", "--signature"));
        }
        if (opts->family && sig_mode) {
            if (!names_scope_eq(np.scope, np.nscope, pl0->scope, pl0->nscope))
                shsm_raise(SHSM_EXC_USAGE,
                           "--family --signature changes the parameter codes only: %s names another class than %s; "
                           "each member keeps its own class %s",
                           new_plain, c0->name, rule_tag(a, "family-class", NULL));
            old_codes = member_codes(a, c0->name, pl0);
            new_codes = member_codes(a, new_plain, &np);
            if (!old_codes || !new_codes)
                shsm_raise(SHSM_EXC_USAGE, "--family --signature: cannot split the parameter codes off %s or %s %s",
                           c0->name, new_plain, rule_tag(a, "family-codes", NULL));
        }
        new_full = new_plain;
        new_plain = np.plain;
        if (unsigned_ || sig_mode) {
            new_scope = np.scope;
            new_nscope = np.nscope;
        }
    }
    for (size_t i = 0; i < cands.n; i++) {
        const ShsmSymbol *s = cands.v[i];
        const NamesPlain *pl = names_sym_plain(cx, s);
        const char *nn;
        size_t rl = strlen(pl->raw);
        if (old_codes) {
            const char *mc = member_codes(a, s->name, pl);
            if (!mc) {
                push(a, &p->conflicts,
                     avp_pyfmt(a, "%s: cannot locate the class and parameter codes in the mangled name %s", s->name,
                               rule_tag(a, "family-codes", NULL)));
                continue;
            }
            if (strcmp(mc, old_codes) != 0) {
                push(a, &p->conflicts,
                     avp_pyfmt(a,
                               "%s: parameter codes %s differ from the family's %s, so it is not the same slot (give "
                               "it its own rename without --family) %s",
                               s->name, mc, old_codes, rule_tag(a, "family-codes", NULL)));
                continue;
            }
            nn = avp_sprintf(a, "%s__%s%s", new_plain, member_class_part(a, pl), new_codes);
        } else if (strcmp(pl->plain, s->name) == 0) {
            nn = new_plain;
        } else if (strncmp(s->name, pl->raw, rl) == 0 && strncmp(s->name + rl, "__", 2) == 0) {
            nn = avp_sprintf(a, "%s%s", new_plain, s->name + rl);
        } else {
            push(a, &p->conflicts, avp_pyfmt(a, "%s: cannot locate the name part of the mangled name", s->name));
            continue;
        }
        if (new_full && !old_codes) nn = new_full;
        NamesSymRename r = {s, nn};
        avp_vec_push(&p->symbols, r);
        if (strcmp(nn, s->name) != 0 && strcmp(pl->plain, s->name) != 0) set_ident(p, s->name, nn);
        if (sig_mode) {
            const char *od = shsm_demangle(a, s->name), *nd = shsm_demangle(a, nn);
            push(a, &p->sig_changes, avp_pyfmt(a, "%s  ->  %s", od ? od : s->name, nd ? nd : nn));
            add_unique(a, &p->sig_words, avp_str(p0));
            if (pl->nscope) add_unique(a, &p->sig_classes, avp_str(last_scope(pl)));
            else p->sig_has_free = true;
            if (np.nscope) add_unique(a, &p->sig_classes, avp_str(last_scope(&np)));
        }
    }
    check_collisions(p, local);
    bool plain_changes = strcmp(p0, new_plain) != 0;
    if (plain_changes || special) set_ident(p, p0, new_plain);
    if (special) {
        special->name = strcmp(special->kind, "__dt") == 0 ? avp_sprintf(a, "~%s", special->cls) : special->cls;
        special->old = p0;
        p->special = special;
        set_ident(p, p0, avp_sprintf(a, "%s::%s", special->qual, special->name));
    }
    /* ambiguity: the plain identifier also names symbols outside the rename set */
    AvpSet *chosen = avp_set_new();
    for (size_t i = 0; i < cands.n; i++) avp_set_add(chosen, avp_str(((const ShsmSymbol *)cands.v[i])->name));
    AvpPtrVec others = {0};
    {
        const AvpPtrVec *byp = names_by_plain(cx, p0);
        AvpPtrVec byn = {0};
        names_all_by_name(cx, p0, &byn);
        for (int pass = 0; pass < 2; pass++) {
            const AvpPtrVec *src = pass ? &byn : byp;
            for (size_t i = 0; i < src->n; i++) {
                const ShsmSymbol *s = src->v[i];
                if (avp_set_has(chosen, avp_str(s->name))) continue;
                bool dup = false;
                for (size_t k = 0; k < others.n && !dup; k++) dup = others.v[k] == s;
                if (!dup) avp_vec_push(&others, (void *)s);
            }
        }
        avp_vec_free(&byn);
    }
    if (others.n) {
        AvpBuf b;
        avp_buf_init(&b, a);
        for (size_t i = 0; i < others.n && i < 8; i++) {
            const ShsmSymbol *s = others.v[i];
            if (i) avp_buf_putz(&b, ", ");
            avp_buf_put(&b, avp_pyfmt(a, "%s (0x%08X)", s->name, s->address));
        }
        if (!plain_changes && !special)
            /* the plain name stays: no source identifier is rewritten, so the others are untouched */
            push(a, &p->notes,
                 avp_pyfmt(a,
                           "the plain name %s also names %d other symbol(s) that are not renamed: %s (if they are the "
                           "overrides of one virtual slot, --family changes them together)",
                           p0, others.n, avp_buf_cz(&b)));
        else
            push(a, &p->conflicts,
                 avp_pyfmt(a,
                           "the plain name %s also names %d other symbol(s): %s; the source identifier would be "
                           "rewritten for them too (if they are the overrides of one virtual slot, pass --family to "
                           "rename them together) %s",
                           p0, others.n, avp_buf_cz(&b), rule_tag(a, "plain-ambiguous", "--family")));
    }
    AvpPtrVec clash = {0}, samev = {0};
    if (plain_changes || special) {
        const AvpPtrVec *byp = names_by_plain(cx, new_plain);
        for (size_t i = 0; i < byp->n; i++) {
            const ShsmSymbol *s = byp->v[i];
            if (avp_set_has(chosen, avp_str(s->name))) continue;
            bool reported = false; /* the exact-name collision is reported by check_collisions */
            for (size_t k = 0; k < p->symbols.n && !reported; k++) reported = strcmp(p->symbols.v[k].new_name, s->name) == 0;
            if (reported) continue;
            avp_vec_push(&clash, (void *)s);
            const NamesPlain *pl = names_sym_plain(cx, s);
            bool in_scope = names_scope_eq(pl->scope, pl->nscope, new_scope, new_nscope);
            for (size_t k = 0; k < cands.n && !in_scope; k++) {
                const NamesPlain *cp = names_sym_plain(cx, cands.v[k]);
                in_scope = names_scope_eq(pl->scope, pl->nscope, cp->scope, cp->nscope);
            }
            if (!in_scope) continue;
            if (is_local_sym(s) && (local || is_local_sym(c0) || !pl->nscope)) continue; /* statics: check_collisions */
            avp_vec_push(&samev, (void *)s);
        }
    }
    avp_set_free(chosen);
    if (samev.n && p->special) {
        /* constructors overload; an identical signature is caught as an existing symbol */
    } else if (samev.n) {
        AvpBuf b;
        avp_buf_init(&b, a);
        for (size_t i = 0; i < samev.n && i < 6; i++) {
            if (i) avp_buf_putz(&b, ", ");
            avp_buf_putz(&b, ((const ShsmSymbol *)samev.v[i])->name);
        }
        push(a, &p->conflicts,
             avp_pyfmt(a, "%s%s already exists: %s (the rename would make an overload set) %s",
                       pl0->nscope ? avp_sprintf(a, "%s::", join_scope(a, pl0->scope, pl0->nscope)) : "", new_plain,
                       avp_buf_cz(&b), rule_tag(a, "overload-set", NULL)));
    } else if (clash.n) {
        AvpBuf b;
        avp_buf_init(&b, a);
        for (size_t i = 0; i < clash.n && i < 6; i++) {
            if (i) avp_buf_putz(&b, ", ");
            avp_buf_putz(&b, ((const ShsmSymbol *)clash.v[i])->name);
        }
        push(a, &p->notes, avp_pyfmt(a, "the new plain name %s is already used in other scopes: %s", new_plain,
                                     avp_buf_cz(&b)));
    }
    check_targets(p);
    avp_vec_free(&same);
    return p;
}

/* ---- plan_class_rename ---- */

/* (?<![A-Za-z0-9_])word(?![A-Za-z0-9_]) */
static bool ident_char(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

static i64 word_find(AvpStr s, AvpStr w, size_t pos) {
    for (;;) {
        i64 i = avp_str_find(s, w, (i64)pos, AVP_END);
        if (i < 0) return -1;
        size_t e = (size_t)i + w.n;
        if ((i == 0 || !ident_char((unsigned char)s.p[i - 1])) && (e >= s.n || !ident_char((unsigned char)s.p[e])))
            return i;
        pos = (size_t)i + 1;
    }
}

static AvpStr word_sub(AvpArena *a, AvpStr s, AvpStr w, AvpStr repl) {
    AvpBuf b;
    avp_buf_init(&b, a);
    size_t last = 0;
    i64 i;
    while ((i = word_find(s, w, last)) >= 0) {
        avp_buf_put(&b, avp_strn(s.p + last, (size_t)i - last));
        avp_buf_put(&b, repl);
        last = (size_t)i + w.n;
    }
    avp_buf_put(&b, avp_strn(s.p + last, s.n - last));
    return avp_buf_str(&b);
}

/* tok_rx.sub: (?<![0-9])<len><old> -> <len><new> */
static const char *tok_sub(AvpArena *a, const char *name, AvpStr tok, AvpStr repl) {
    AvpStr s = avp_str(name);
    AvpBuf b;
    avp_buf_init(&b, a);
    size_t last = 0, pos = 0;
    for (;;) {
        i64 i = avp_str_find(s, tok, (i64)pos, AVP_END);
        if (i < 0) break;
        if (i > 0 && s.p[i - 1] >= '0' && s.p[i - 1] <= '9') {
            pos = (size_t)i + 1;
            continue;
        }
        avp_buf_put(&b, avp_strn(s.p + last, (size_t)i - last));
        avp_buf_put(&b, repl);
        last = pos = (size_t)i + tok.n;
    }
    avp_buf_put(&b, avp_strn(s.p + last, s.n - last));
    return avp_buf_cz(&b);
}

NamesPlan *names_plan_class_rename(NamesCtx *cx, const char *old, const char *new_) {
    AvpArena *a = cx->a;
    NamesPlan *p = plan_new(cx);
    const char *both[2] = {old, new_};
    for (int i = 0; i < 2; i++)
        if (!re_matches(IDENT_RE, both[i]))
            shsm_raise(SHSM_EXC_USAGE, "%s is not a class identifier (give the last name component)",
                       avp_cz(a, avp_repr_str(a, avp_str(both[i]))));
    AvpStr tok = avp_pyfmt(a, "%d%s", avp_u8_len(avp_str(old)), old);
    AvpStr repl = avp_pyfmt(a, "%d%s", avp_u8_len(avp_str(new_)), new_);
    AvpStr wold = avp_str(old), wnew = avp_str(new_);
    size_t found = 0;
    const ShsmSymbolTable *st = cx->st;
    for (size_t i = 0; i < st->n; i++) {
        const ShsmSymbol *s = &st->symbols[i];
        const char *d = shsm_demangle(a, s->name);
        bool mentions = d && word_find(avp_str(d), wold, 0) >= 0;
        if (d && word_find(avp_str(d), wnew, 0) >= 0) {
            push(a, &p->conflicts, avp_pyfmt(a, "class %s already exists: %s", new_, s->name));
            continue;
        }
        if (!mentions) {
            if (strcmp(s->name, old) == 0)
                push(a, &p->conflicts, avp_pyfmt(a, "%s is also a symbol name (0x%08X)", old, s->address));
            continue;
        }
        const char *nn = tok_sub(a, s->name, tok, repl);
        bool bad = strcmp(nn, s->name) == 0;
        if (!bad) {
            const char *dn = shsm_demangle(a, nn);
            AvpStr want = word_sub(a, avp_str(d), wold, wnew);
            bad = dn == NULL || !avp_str_eqz(want, dn);
        }
        if (bad) {
            push(a, &p->conflicts, avp_pyfmt(a, "%s: cannot rewrite the class part safely (%s)", s->name, d));
            continue;
        }
        if (names_has(cx, nn)) push(a, &p->conflicts, avp_pyfmt(a, "%s already exists in symbols.txt", nn));
        NamesSymRename r = {s, nn};
        avp_vec_push(&p->symbols, r);
        set_ident(p, s->name, nn);
        found++;
    }
    if (!found) shsm_raise(SHSM_EXC_USAGE, "no symbol mentions class %s", old);
    AvpPtrVec same = {0};
    for (size_t i = 0; i < p->symbols.n; i++) {
        const ShsmSymbol *s = p->symbols.v[i].sym;
        size_t n = names_all_by_name(cx, s->name, &same);
        if (n > 1) push(a, &p->conflicts, avp_pyfmt(a, "%s is defined %d times in symbols.txt", s->name, n));
    }
    avp_vec_free(&same);
    set_ident(p, old, new_);
    const char *tops[2] = {cx->repo->src_dir, cx->repo->include_dir};
    for (int t = 0; t < 2; t++) {
        AvpWalk *w = avp_walk_open(tops[t], true);
        AvpWalkItem it = {0};
        while (avp_walk_next(w, &it)) {
            for (size_t k = 0; k < it.files.n; k++) {
                const char *fn = avp_cz(a, it.files.v[k]);
                AvpStr root, ext;
                avp_path_splitext(fn, &root, &ext);
                if (avp_str_eqz(root, old)) {
                    const char *path = avp_join(a, avp_cz(a, it.dirpath), fn);
                    const char *rel = shsm_repo_rel(a, cx->repo, path);
                    push(a, &p->notes, avp_pyfmt(a, "file named after the class (not renamed): %s", rel));
                }
            }
        }
        avp_walk_close(w);
    }
    return p;
}

/* ---- files ---- */

AvpStr names_read_text(AvpArena *a, const char *path) {
    AvpBytes b = {0};
    if (!avp_read_all(a, path, &b)) shsm_raise_io();
    return avp_strn((const char *)b.v, b.n);
}

AvpStrVec names_split_nl(AvpStr text) { return avp_str_split(text, AVP_S("\n"), -1); }

bool names_src_ext(AvpStr name) {
    static const char *const EXT[] = {".c", ".cpp", ".cp", ".h", ".hpp", ".inc"};
    for (size_t i = 0; i < sizeof EXT / sizeof *EXT; i++)
        if (avp_str_endswith(name, avp_str(EXT[i]))) return true;
    return false;
}

void names_sort_strs(AvpStrVec *v) { avp_sort_strs(v->v, v->n); }

typedef struct {
    const char *rel, *path;
} Target;
typedef AVP_VEC(Target) Targets;

static void walk_files(AvpArena *a, const ShsmRepo *r, const char *top, bool (*want)(AvpStr), Targets *out) {
    AvpWalk *w = avp_walk_open(top, true);
    AvpWalkItem it = {0};
    while (avp_walk_next(w, &it)) {
        AvpStrVec fns = {0};
        avp_vec_extend(&fns, it.files.v, it.files.n);
        names_sort_strs(&fns);
        for (size_t k = 0; k < fns.n; k++) {
            if (!want(fns.v[k])) continue;
            const char *path = avp_join(a, avp_cz(a, it.dirpath), avp_cz(a, fns.v[k]));
            Target t = {shsm_repo_rel(a, r, path), path};
            avp_vec_push(out, t);
        }
        avp_vec_free(&fns);
    }
    avp_walk_close(w);
}

static bool is_md(AvpStr n) { return avp_str_endswith(n, AVP_S(".md")); }
static bool is_header(AvpStr n) { return avp_str_endswith(n, AVP_S(".h")) || avp_str_endswith(n, AVP_S(".hpp")); }

static Targets edit_targets(AvpArena *a, const ShsmRepo *r) {
    Targets out = {0};
    walk_files(a, r, r->src_dir, names_src_ext, &out);
    walk_files(a, r, r->include_dir, names_src_ext, &out);
    walk_files(a, r, avp_join(a, r->root, "docs"), is_md, &out);
    const char *extra[2] = {r->configure_py, r->config_yml};
    for (int i = 0; i < 2; i++) {
        Target t = {shsm_repo_rel(a, r, extra[i]), extra[i]};
        avp_vec_push(&out, t);
    }
    Targets kept = {0};
    for (size_t i = 0; i < out.n; i++) {
        /* generated files: the ledger and anything under build/ are never rename targets */
        if (strcmp(out.v[i].rel, "docs/progress.md") == 0 || strncmp(out.v[i].rel, "build/", 6) == 0) continue;
        if (avp_isfile(out.v[i].path)) avp_vec_push(&kept, out.v[i]);
    }
    avp_vec_free(&out);
    return kept;
}

static AvpStr rstrip_cr(AvpStr s) {
    while (s.n && s.p[s.n - 1] == '\r') s.n--;
    return s;
}

static AvpStr join_lines(AvpArena *a, const AvpStrVec *lines) {
    AvpBuf b;
    avp_buf_init(&b, a);
    bool first = true;
    for (size_t i = 0; i < lines->n; i++) {
        if (lines->v[i].p == NULL) continue; /* a line that goes */
        if (!first) avp_buf_putc(&b, '\n');
        first = false;
        avp_buf_put(&b, lines->v[i]);
    }
    return avp_str_dup(a, avp_buf_str(&b));
}

static int edit_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const NamesEdit *p = x, *q = y;
    if (p->line != q->line) return p->line < q->line ? -1 : 1;
    int c = avp_str_cmp(p->old, q->old);
    return c ? c : avp_str_cmp(p->new_, q->new_);
}

static AvpStr sub_ident(void *ctx, AvpStr m) {
    const NamesPlan *p = ctx;
    return avp_str(ident_of(p, m));
}

static AvpStr sub_const(void *ctx, AvpStr m) {
    (void)m;
    return *(const AvpStr *)ctx;
}

/* special_line: NULL p for a line that goes (queued as a member declaration). */
static AvpStr special_line(NamesPlan *p, const char *rel, size_t lineno, AvpStr line) {
    AvpArena *a = p->cx->a;
    NamesSpecial *sp = p->special;
    AvpStr old = avp_str(sp->old);
    AvpStr nw = avp_pyfmt(a, "%s::%s", sp->qual, sp->name);
    AvpStr stripped = avp_str_strip(line);
    bool comment = false;
    i64 c = avp_str_find(line, AVP_S("//"), 0, AVP_END);
    if (c >= 0 && avp_str_find(line, old, c + 2, AVP_END) >= 0) comment = true;
    if (avp_str_eq(stripped, old) || avp_str_startswith(stripped, AVP_S("//")) ||
        avp_str_startswith(stripped, AVP_S("/*")) || avp_str_startswith(stripped, AVP_S("*")) || comment) {
        NamesIdentRx *rx = names_ident_rx(a, &old, 1);
        return names_ident_sub(a, rx, line, sub_const, &nw);
    }
    AvpBuf pat;
    avp_buf_init(&pat, a);
    avp_buf_putz(&pat, "^(?P<ind>\\s*)(?:extern\\s+\"C\"\\s+)?(?:(?:static|inline|virtual)\\s+)*"
                       "(?P<ret>[A-Za-z_][\\w:<>\\s\\*&]*?)\\s+\\*?&?\\s*");
    avp_buf_put(&pat, shsm_re_escape(a, old));
    avp_buf_putz(&pat, "\\s*\\((?P<rest>.*)$");
    ShsmReMatch m;
    if (shsm_re_match(shsm_re(avp_buf_cz(&pat), 0), line, 0, AVP_END, &m) &&
        !shsm_re_match(shsm_re("^\\s*(return|else|new|delete|goto|case|throw)\\b", 0), line, 0, AVP_END, NULL)) {
        AvpStr rest = rstrip_cr(shsm_re_group_named(&m, "rest"));
        if (avp_str_endswith(avp_str(rel), AVP_S(".c")) || avp_str_endswith(avp_str(rel), AVP_S(".cpp")) ||
            avp_str_endswith(avp_str(rel), AVP_S(".cp"))) {
            AvpBuf b;
            avp_buf_init(&b, a);
            avp_buf_put(&b, shsm_re_group_named(&m, "ind"));
            avp_buf_put(&b, nw);
            avp_buf_putc(&b, '(');
            avp_buf_put(&b, rest);
            if (avp_str_endswith(line, AVP_S("\r"))) avp_buf_putc(&b, '\r');
            return avp_buf_str(&b);
        }
        ShsmReMatch mm;
        if (shsm_re_match(shsm_re("^(?P<params>[^;{]*)\\)\\s*;\\s*$", 0), rest, 0, AVP_END, &mm)) {
            AvpStr params = shsm_re_group_named(&mm, "params");
            push(a, &sp->decls, avp_pyfmt(a, "%s(%S);", sp->name, &params));
            return (AvpStr){NULL, 0};
        }
    }
    AvpStr clipped = names_clip(a, line, 60);
    push(a, &p->conflicts,
         avp_pyfmt(a, "%s:%d: %s is used here (%S); a %s has no such spelling: edit it by hand and use --symbols-only",
                   rel, lineno, sp->old, &clipped,
                   strcmp(sp->kind, "__dt") == 0 ? "destructor" : "constructor"));
    return line;
}

static AvpStr remove_ws(AvpArena *a, AvpStr s) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < s.n;) {
        size_t w = shsm_ws_len(s, i);
        if (w) {
            i += w;
            continue;
        }
        avp_buf_putc(&b, s.p[i]);
        i++;
    }
    return avp_buf_str(&b);
}

static void add_member_decls(NamesPlan *p) {
    NamesCtx *cx = p->cx;
    AvpArena *a = cx->a;
    NamesSpecial *sp = p->special;
    AvpBuf pat;
    avp_buf_init(&pat, a);
    avp_buf_putz(&pat, "^\\s*(?:class|struct)\\s+");
    avp_buf_put(&pat, shsm_re_escape(a, avp_str(sp->cls)));
    avp_buf_putz(&pat, "\\b[^;{]*\\{");
    const ShsmRe *cls_rx = shsm_re(avp_buf_cz(&pat), 0);
    AvpWalk *w = avp_walk_open(cx->repo->include_dir, true);
    AvpWalkItem it = {0};
    while (avp_walk_next(w, &it)) {
        AvpStrVec fns = {0};
        avp_vec_extend(&fns, it.files.v, it.files.n);
        names_sort_strs(&fns);
        for (size_t k = 0; k < fns.n; k++) {
            if (!is_header(fns.v[k])) continue;
            const char *path = avp_join(a, avp_cz(a, it.dirpath), avp_cz(a, fns.v[k]));
            const char *rel = shsm_repo_rel(a, cx->repo, path);
            NamesFile *f = names_plan_file(p, rel);
            AvpStr text = f ? f->text : names_read_text(a, path);
            AvpStrVec lines = names_split_nl(text);
            size_t start = lines.n;
            for (size_t i = 0; i < lines.n; i++)
                if (shsm_re_match(cls_rx, lines.v[i], 0, AVP_END, NULL)) {
                    start = i;
                    break;
                }
            if (start == lines.n) continue;
            i64 depth = 0;
            size_t end = SIZE_MAX, pub = SIZE_MAX;
            for (size_t i = start; i < lines.n; i++) {
                depth += (i64)avp_str_count(lines.v[i], AVP_S("{")) - (i64)avp_str_count(lines.v[i], AVP_S("}"));
                if (pub == SIZE_MAX && shsm_re_match(shsm_re("^\\s*public\\s*:", 0), lines.v[i], 0, AVP_END, NULL) &&
                    depth == 1)
                    pub = i;
                if (depth <= 0) {
                    end = i;
                    break;
                }
            }
            if (end == SIZE_MAX) break;
            size_t at = pub != SIZE_MAX ? pub + 1 : start + 1;
            AvpStrVec bodyv = {0};
            avp_vec_extend(&bodyv, lines.v + start, end - start);
            AvpStr body = remove_ws(a, join_lines(a, &bodyv));
            avp_vec_free(&bodyv);
            AvpStrVec ins = {0};
            for (size_t i = 0; i < sp->decls.n; i++) {
                AvpBuf dp;
                avp_buf_init(&dp, a);
                avp_buf_putz(&dp, "(?<![~\\w])");
                avp_buf_put(&dp, shsm_re_escape(a, remove_ws(a, sp->decls.v[i])));
                if (!shsm_re_search(shsm_re(avp_buf_cz(&dp), 0), body, 0, AVP_END, NULL))
                    avp_vec_push(&ins, avp_pyfmt(a, "    %S", &sp->decls.v[i]));
            }
            for (size_t i = 0; i < ins.n; i++) avp_vec_insert(&lines, at + i, ins.v[i]);
            if (!f) f = names_plan_file_add(p, rel);
            f->text = join_lines(a, &lines);
            for (size_t i = 0; i < ins.n; i++) {
                NamesEdit e = {at + 1 + i, AVP_S(""), ins.v[i]};
                avp_vec_push(&f->edits, e);
            }
            if (!ins.n)
                push(a, &p->notes, avp_pyfmt(a, "%s already declares %s in %s", sp->cls, sp->name, rel));
            avp_vec_free(&fns);
            avp_walk_close(w);
            return;
        }
        avp_vec_free(&fns);
    }
    avp_walk_close(w);
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < sp->decls.n; i++) {
        if (i) avp_buf_putz(&b, "; ");
        avp_buf_put(&b, sp->decls.v[i]);
    }
    push(a, &p->conflicts, avp_pyfmt(a, "no class %s found under include/ to take the member declaration %s", sp->cls,
                                     avp_buf_cz(&b)));
}

/* ---- comments and declaration sites ---- */

/* Whether byte pos of line s lies in a comment, given that the line starts inside a block comment or not; *end_block is
 * the state at the end of the line. String and character literals are skipped. pos beyond the line: the state at
 * the end. */
static bool scan_comments(AvpStr s, bool in_block, size_t pos, bool *end_block) {
    bool line_c = false, result = false;
    for (size_t i = 0; i < s.n; i++) {
        if (i == pos) result = in_block || line_c;
        if (line_c) continue;
        char c = s.p[i];
        if (in_block) {
            if (c == '*' && i + 1 < s.n && s.p[i + 1] == '/') {
                if (i + 1 == pos) result = true;
                in_block = false;
                i++;
            }
            continue;
        }
        if (c == '"' || c == '\'') {
            size_t j = i + 1;
            while (j < s.n && s.p[j] != c) j += s.p[j] == '\\' ? 2 : 1;
            i = j;
            continue;
        }
        if (c == '/' && i + 1 < s.n && s.p[i + 1] == '/') {
            line_c = true;
            i++;
        } else if (c == '/' && i + 1 < s.n && s.p[i + 1] == '*') {
            in_block = true;
            i++;
        }
    }
    if (pos >= s.n) result = in_block || line_c;
    *end_block = in_block;
    return result;
}

static bool type_char(unsigned char c) { return ident_char(c) || c == ' ' || c == '\t' || c == ':' || c == '<' || c == '>' || c == ',' || c == '*' || c == '&'; }

static bool keyword_head(AvpStr w) {
    static const char *const KW[] = {"return", "else", "new", "delete", "goto", "case", "throw", "sizeof", "do"};
    for (size_t i = 0; i < sizeof KW / sizeof *KW; i++)
        if (avp_str_eqz(w, KW[i])) return true;
    return false;
}

/* The first identifier of s. */
static AvpStr first_word(AvpStr s) {
    s = avp_str_strip(s);
    size_t n = 0;
    while (n < s.n && ident_char((unsigned char)s.p[n])) n++;
    return avp_strn(s.p, n);
}

/* Whether the identifier at [st, st+ln) of line heads a declaration or definition (a return type before it, or a
 * qualifier with the return type on the line above), as opposed to a call. *qual is the class it is qualified with. */
static bool is_decl_head(AvpStr line, size_t st, size_t ln, AvpStr prev, AvpStr *qual) {
    size_t e = st + ln;
    while (e < line.n && (line.p[e] == ' ' || line.p[e] == '\t')) e++;
    if (e >= line.n || line.p[e] != '(') return false;
    size_t b = st;
    *qual = AVP_S("");
    bool first = true;
    while (b >= 2 && line.p[b - 1] == ':' && line.p[b - 2] == ':') {
        size_t qe = b - 2, qs = qe;
        if (qs > 0 && line.p[qs - 1] == '>') {
            int depth = 0;
            while (qs > 0) {
                char c = line.p[--qs];
                if (c == '>') depth++;
                else if (c == '<' && --depth == 0) break;
            }
        }
        size_t ie = qs;
        while (qs > 0 && ident_char((unsigned char)line.p[qs - 1])) qs--;
        if (first) *qual = avp_strn(line.p + qs, ie - qs);
        first = false;
        b = qs;
    }
    AvpStr head = avp_str_strip(avp_strn(line.p, b));
    if (!head.n) {
        if (!qual->n) return false;
        AvpStr pv = avp_str_strip(prev);
        if (!pv.n || keyword_head(first_word(pv))) return false;
        for (size_t i = 0; i < pv.n; i++)
            if (!type_char((unsigned char)pv.p[i])) return false;
        return true;
    }
    bool letter = false;
    for (size_t i = 0; i < head.n; i++) {
        if (!type_char((unsigned char)head.p[i])) return false;
        if (ident_char((unsigned char)head.p[i])) letter = true;
    }
    if (!letter || keyword_head(first_word(head))) return false;
    char last = head.p[head.n - 1];
    return ident_char((unsigned char)last) || last == '*' || last == '&' || last == '>';
}

/* The class a header or source line sits in, by the nearest earlier class or struct head that no column-0 '}' has
 * closed; "" when none. */
static AvpStr enclosing_class(const AvpStrVec *lines, size_t idx) {
    const ShsmRe *rx = shsm_re("^\\s*(?:class|struct)\\s+(?P<n>[A-Za-z_]\\w*)\\b[^;]*$", 0);
    for (size_t k = idx; k-- > 0 && idx - k < 800;) {
        AvpStr l = lines->v[k];
        if (l.n && l.p[0] == '}') return AVP_S("");
        ShsmReMatch m;
        if (shsm_re_match(rx, l, 0, AVP_END, &m)) return shsm_re_group_named(&m, "n");
    }
    return AVP_S("");
}

/* --signature: every declaration or definition of the changed functions, for a human (the tool cannot rewrite a
 * parameter list); calls need no edit. */
static void collect_hand_sites(NamesPlan *p, const Targets *targets) {
    NamesCtx *cx = p->cx;
    AvpArena *a = cx->a;
    if (!p->sig_words.n) return;
    NamesIdentRx *rx = names_ident_rx(a, p->sig_words.v, p->sig_words.n);
    size_t ncalls = 0;
    for (size_t t = 0; t < targets->n; t++) {
        AvpStrVec parts = avp_str_split(avp_str(targets->v[t].rel), AVP_S("/"), -1);
        bool code = names_src_ext(avp_str(targets->v[t].rel)) &&
                    (avp_str_eqz(parts.v[0], "src") || avp_str_eqz(parts.v[0], "include"));
        avp_vec_free(&parts);
        if (!code) continue;
        AvpStr text = names_read_text(a, targets->v[t].path);
        size_t st, ln;
        if (!names_ident_find(rx, text, 0, &st, &ln)) continue;
        AvpStrVec lines = names_split_nl(text);
        bool in_block = false;
        for (size_t i = 0; i < lines.n; i++) {
            AvpStr line = rstrip_cr(lines.v[i]);
            bool block0 = in_block;
            scan_comments(line, block0, SIZE_MAX, &in_block);
            size_t pos = 0;
            bool listed = false;
            while (!listed && pos <= line.n && names_ident_find(rx, line, pos, &st, &ln)) {
                pos = st + (ln ? ln : 1);
                bool dummy;
                if (scan_comments(line, block0, st, &dummy)) continue;
                AvpStr qual;
                if (!is_decl_head(line, st, ln, i ? rstrip_cr(lines.v[i - 1]) : AVP_S(""), &qual)) {
                    ncalls++;
                    continue;
                }
                AvpStr ctx = qual.n ? qual : enclosing_class(&lines, i);
                bool known = false;
                for (size_t k = 0; k < p->sig_classes.n && ctx.n && !known; k++) known = avp_str_eq(p->sig_classes.v[k], ctx);
                if (ctx.n && p->sig_classes.n && !known) continue;
                AvpStr clipped = names_clip(a, line, 100);
                push(a, &p->hand_sites, avp_pyfmt(a, "%s:%d: %S", targets->v[t].rel, (i64)(i + 1), &clipped));
                listed = true;
            }
        }
        avp_vec_free(&lines);
    }
    if (ncalls)
        push(a, &p->notes,
             avp_pyfmt(a, "%d other mention(s) of %s in calls, comments or other classes need no edit unless an argument "
                           "changes",
                       ncalls, avp_cz(a, p->sig_words.v[0])));
}

void names_compute_edits(NamesPlan *p) {
    NamesCtx *cx = p->cx;
    AvpArena *a = cx->a;
    const ShsmRepo *repo = cx->repo;
    /* symbols.txt: the exact lines */
    const char *rel = shsm_repo_rel(a, repo, repo->symbols_txt);
    AvpStr text = names_read_text(a, repo->symbols_txt);
    AvpStrVec lines = names_split_nl(text);
    AVP_VEC(NamesEdit) edits = {0};
    for (size_t k = 0; k < p->symbols.n; k++) {
        const ShsmSymbol *s = p->symbols.v[k].sym;
        const char *nn = p->symbols.v[k].new_name;
        size_t i = s->lineno - 1;
        if (s->lineno >= 1 && i < lines.n && avp_str_eqz(rstrip_cr(lines.v[i]), s->raw)) {
            bool cr = avp_str_endswith(lines.v[i], AVP_S("\r"));
            AvpStr new_line = avp_pyfmt(a, "%s%s", nn, s->raw + strlen(s->name));
            if (p->local && !avp_str_contains(new_line, AVP_S("scope:local"))) {
                if (avp_str_contains(new_line, AVP_S(" scope:")))
                    new_line = shsm_re_sub(a, shsm_re(" scope:\\w+", 0), AVP_S(" scope:local"), new_line, 0);
                else
                    new_line = shsm_re_sub(a, shsm_re("( size:0x[0-9A-Fa-f]+)", 0), AVP_S("\\1 scope:local"),
                                           new_line, 1);
            }
            if (cr) new_line = avp_pyfmt(a, "%S\r", &new_line);
            NamesEdit e = {s->lineno, rstrip_cr(lines.v[i]), rstrip_cr(new_line)};
            avp_vec_push(&edits, e);
            lines.v[i] = new_line;
        } else {
            push(a, &p->conflicts,
                 avp_pyfmt(a, "symbols.txt line %d is not the line of %s (file changed?)", s->lineno, s->name));
        }
    }
    if (edits.n) {
        avp_sort(edits.v, edits.n, sizeof(NamesEdit), edit_cmp, NULL, false);
        NamesFile *f = names_plan_file_add(p, rel);
        avp_vec_extend(&f->edits, edits.v, edits.n);
        f->text = join_lines(a, &lines);
    }
    avp_vec_free(&edits);
    avp_vec_free(&lines);
    if (avp_map_len(p->idents) == 0 && !p->site_words.n) {
        if (p->sig_words.n) {
            Targets only = edit_targets(a, repo);
            collect_hand_sites(p, &only);
            avp_vec_free(&only);
        }
        return;
    }
    AvpStrVec words = avp_map_keys(p->idents);
    /* A new name that is a mangled C++ name is never written into code (.c/.cpp/.h): those mentions, and the
     * mentions of site_words, are listed for a human instead. Docs and config take the mangled form. */
    AvpStrVec code_words = {0}, site_words = {0};
    for (size_t i = 0; i < words.n; i++) {
        const char *nv = ident_of(p, words.v[i]);
        bool mangled = nv && re_matches("^\\w+__(?:Q?\\d|F|CF|t\\d|PF)\\w*$", nv);
        if (mangled) avp_vec_push(&site_words, words.v[i]);
        else avp_vec_push(&code_words, words.v[i]);
    }
    for (size_t i = 0; i < p->site_words.n; i++) avp_vec_push(&site_words, p->site_words.v[i]);
    AvpStrVec all_words = {0};
    avp_vec_extend(&all_words, words.v, words.n);
    for (size_t i = 0; i < p->site_words.n; i++) avp_vec_push(&all_words, p->site_words.v[i]);
    NamesIdentRx *rx = names_ident_rx(a, all_words.v, all_words.n);
    NamesIdentRx *rx_edit = words.n ? names_ident_rx(a, words.v, words.n) : NULL;
    NamesIdentRx *rx_code = code_words.n ? names_ident_rx(a, code_words.v, code_words.n) : NULL;
    NamesIdentRx *rx_site = site_words.n ? names_ident_rx(a, site_words.v, site_words.n) : NULL;
    size_t st0, ln0, nsites = 0, ncomments = 0;
    Targets targets = edit_targets(a, repo);
    for (size_t t = 0; t < targets.n; t++) {
        const char *trel = targets.v[t].rel;
        AvpStr ftext = names_read_text(a, targets.v[t].path);
        if (!names_ident_find(rx, ftext, 0, &st0, &ln0)) continue;
        AvpStrVec fl = names_split_nl(ftext);
        AVP_VEC(NamesEdit) fe = {0};
        AvpStrVec parts = avp_str_split(avp_str(trel), AVP_S("/"), -1);
        bool code_file = names_src_ext(avp_str(trel)) &&
                         (avp_str_eqz(parts.v[0], "src") || avp_str_eqz(parts.v[0], "include"));
        avp_vec_free(&parts);
        bool in_block = false;
        for (size_t i = 0; i < fl.n; i++) {
            AvpStr line = fl.v[i];
            bool block0 = in_block;
            if (code_file) scan_comments(rstrip_cr(line), block0, SIZE_MAX, &in_block);
            if (!names_ident_find(rx, line, 0, &st0, &ln0)) continue;
            AvpStr nl;
            if (p->special && code_file) {
                nl = special_line(p, trel, i + 1, line);
            } else if (code_file) {
                nl = rx_code ? names_ident_sub(a, rx_code, line, sub_ident, p) : line;
                if (rx_site && names_ident_find(rx_site, line, 0, &st0, &ln0)) {
                    bool dummy;
                    if (scan_comments(rstrip_cr(line), block0, st0, &dummy)) {
                        if (ncomments < 12) {
                            AvpStr clipped = names_clip(a, line, 90);
                            push(a, &p->notes,
                                 avp_pyfmt(a, "mentions in comments, not rewritten (the new name has no source "
                                              "spelling): %s:%d: %S",
                                           trel, (i64)(i + 1), &clipped));
                        }
                        ncomments++;
                    } else {
                        if (nsites < 12) {
                            AvpStr clipped = names_clip(a, line, 90);
                            push(a, &p->notes,
                                 avp_pyfmt(a, "source not edited (the new name has no source spelling): %s:%d: %S",
                                           trel, (i64)(i + 1), &clipped));
                        }
                        nsites++;
                    }
                }
            } else {
                nl = rx_edit ? names_ident_sub(a, rx_edit, line, sub_ident, p) : line;
            }
            if (nl.p == NULL || !avp_str_eq(nl, line)) {
                NamesEdit e = {i + 1, rstrip_cr(line), nl.p ? rstrip_cr(nl) : AVP_S("")};
                avp_vec_push(&fe, e);
                fl.v[i] = nl;
            }
        }
        if (fe.n) {
            NamesFile *f = names_plan_file_add(p, trel);
            avp_vec_extend(&f->edits, fe.v, fe.n);
            f->text = join_lines(a, &fl);
        }
        avp_vec_free(&fe);
        avp_vec_free(&fl);
    }
    collect_hand_sites(p, &targets);
    avp_vec_free(&targets);
    if (nsites > 12)
        push(a, &p->notes, avp_pyfmt(a, "source not edited: %d more site(s) not listed", (i64)(nsites - 12)));
    if (ncomments > 12)
        push(a, &p->notes, avp_pyfmt(a, "mentions in comments, not rewritten: %d more not listed", (i64)(ncomments - 12)));
    if (p->special && p->special->decls.n) add_member_decls(p);
    /* mentions that stay as they are; generated files (the ledger, the baseline report, anything under build/) are
     * excluded from the plan: regenerate them instead of editing them */
    const char *keep[3] = {avp_join(a, repo->root, "AGENTS.md"), repo->baseline_report_json,
                           avp_join(a, avp_join(a, repo->root, "docs"), "progress.md")};
    for (int i = 0; i < 3; i++) {
        if (!avp_isfile(keep[i])) continue;
        size_t n = names_ident_count(rx, names_read_text(a, keep[i]));
        if (n)
            push(a, &p->notes,
                 avp_pyfmt(a, "%s mentions the old name %d time(s); not edited%s", shsm_repo_rel(a, repo, keep[i]), n,
                           i == 1   ? " (generated file, excluded from the rename plan; refreshed at the next merge)"
                           : i == 2 ? " (generated ledger, excluded from the rename plan; regenerate it with "
                                      "ledger.exe progress)"
                                    : ""));
    }
    avp_vec_free(&words);
}

void names_write_plan(const NamesPlan *p) {
    AvpArena *a = p->cx->a;
    for (size_t i = 0; i < p->files.n; i++) {
        const NamesFile *f = p->files.v[i];
        AvpStrVec parts = avp_str_split(avp_str(f->rel), AVP_S("/"), -1);
        const char *path = p->cx->repo->root;
        for (size_t k = 0; k < parts.n; k++) path = avp_join(a, path, avp_cz(a, parts.v[k]));
        avp_vec_free(&parts);
        const char *tmp = avp_sprintf(a, "%s.names-tmp", path);
        if (!avp_write_file(tmp, f->text.p, f->text.n)) shsm_raise_io();
        if (!avp_replace(tmp, path)) shsm_raise_io();
    }
}

AvpJson *names_plan_to_json(AvpArena *a, const NamesPlan *p) {
    AvpJson *o = avp_json_obj(a);
    AvpJson *syms = avp_json_arr(a);
    for (size_t i = 0; i < p->symbols.n; i++) {
        AvpJson *s = avp_json_obj(a);
        avp_json_set(a, s, AVP_S("old"), avp_json_strz(a, p->symbols.v[i].sym->name));
        avp_json_set(a, s, AVP_S("new"), avp_json_strz(a, p->symbols.v[i].new_name));
        avp_json_set(a, s, AVP_S("address"), avp_json_uint(a, p->symbols.v[i].sym->address));
        avp_json_arr_push(a, syms, s);
    }
    avp_json_set(a, o, AVP_S("symbols"), syms);
    AvpJson *ids = avp_json_obj(a);
    AvpMapIter it = {0};
    while (avp_map_next(p->idents, &it)) avp_json_set(a, ids, it.key, avp_json_strz(a, (const char *)it.val->p));
    avp_json_set(a, o, AVP_S("identifiers"), ids);
    AvpJson *c = avp_json_arr(a);
    for (size_t i = 0; i < p->conflicts.n; i++) avp_json_arr_push(a, c, avp_json_str(a, p->conflicts.v[i]));
    avp_json_set(a, o, AVP_S("conflicts"), c);
    AvpJson *n = avp_json_arr(a);
    for (size_t i = 0; i < p->notes.n; i++) avp_json_arr_push(a, n, avp_json_str(a, p->notes.v[i]));
    avp_json_set(a, o, AVP_S("notes"), n);
    AvpJson *sc = avp_json_arr(a);
    for (size_t i = 0; i < p->sig_changes.n; i++) avp_json_arr_push(a, sc, avp_json_str(a, p->sig_changes.v[i]));
    avp_json_set(a, o, AVP_S("signature_changes"), sc);
    AvpJson *hs = avp_json_arr(a);
    for (size_t i = 0; i < p->hand_sites.n; i++) avp_json_arr_push(a, hs, avp_json_str(a, p->hand_sites.v[i]));
    avp_json_set(a, o, AVP_S("to_edit_by_hand"), hs);
    AvpJson *ed = avp_json_obj(a);
    for (size_t i = 0; i < p->files.n; i++) {
        const NamesFile *f = p->files.v[i];
        AvpJson *arr = avp_json_arr(a);
        for (size_t k = 0; k < f->edits.n; k++) {
            AvpJson *e = avp_json_obj(a);
            avp_json_set(a, e, AVP_S("line"), avp_json_int(a, (i64)f->edits.v[k].line));
            avp_json_set(a, e, AVP_S("old"), avp_json_str(a, f->edits.v[k].old));
            avp_json_set(a, e, AVP_S("new"), avp_json_str(a, f->edits.v[k].new_));
            avp_json_arr_push(a, arr, e);
        }
        avp_json_set(a, ed, avp_str(f->rel), arr);
    }
    avp_json_set(a, o, AVP_S("edits"), ed);
    return o;
}

/* ---- batches ---- */

NamesPlan *names_plan_new(NamesCtx *cx) { return plan_new(cx); }

void names_plan_merge(NamesPlan *dst, const NamesPlan *src) {
    AvpArena *a = dst->cx->a;
    for (size_t i = 0; i < src->symbols.n; i++) avp_vec_push(&dst->symbols, src->symbols.v[i]);
    AvpMapIter it = {0};
    while (avp_map_next(src->idents, &it)) set_ident(dst, avp_cz(a, it.key), (const char *)it.val->p);
    for (size_t i = 0; i < src->site_words.n; i++) avp_vec_push(&dst->site_words, src->site_words.v[i]);
    for (size_t i = 0; i < src->notes.n; i++) avp_vec_push(&dst->notes, src->notes.v[i]);
    for (size_t i = 0; i < src->sig_changes.n; i++) avp_vec_push(&dst->sig_changes, src->sig_changes.v[i]);
    for (size_t i = 0; i < src->conflicts.n; i++) avp_vec_push(&dst->conflicts, src->conflicts.v[i]);
}

AvpStrVec names_code_mentions(NamesCtx *cx, const AvpStrVec *words) {
    AvpArena *a = cx->a;
    AvpStrVec found = {0};
    if (!words->n) return found;
    NamesIdentRx *rx = names_ident_rx(a, words->v, words->n);
    AvpSet *seen = avp_set_new();
    Targets targets = edit_targets(a, cx->repo);
    for (size_t t = 0; t < targets.n; t++) {
        AvpStrVec parts = avp_str_split(avp_str(targets.v[t].rel), AVP_S("/"), -1);
        bool code = names_src_ext(avp_str(targets.v[t].rel)) &&
                    (avp_str_eqz(parts.v[0], "src") || avp_str_eqz(parts.v[0], "include"));
        avp_vec_free(&parts);
        if (!code) continue;
        AvpStr text = names_read_text(a, targets.v[t].path);
        size_t pos = 0, st, ln;
        while (pos < text.n && names_ident_find(rx, text, pos, &st, &ln)) {
            AvpStr w = avp_strn(text.p + st, ln);
            if (!avp_set_has(seen, w)) {
                avp_set_add(seen, w);
                avp_vec_push(&found, avp_str_dup(a, w));
            }
            pos = st + ln;
        }
    }
    avp_vec_free(&targets);
    avp_set_free(seen);
    return found;
}
