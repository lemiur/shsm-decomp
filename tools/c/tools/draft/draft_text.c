/* draft.py's names and paths (lines 112-189), the substitution map (Subst, pair_relocations, own_address_name,
 * build_subst, TOKEN/substitute: lines 192-345), clone_header (691-720) and the grading helpers screen,
 * marked_functions, compiled_name_for and rename_explains (939-987). */
#include "draft.h"

#include "shsm_mangle.h"
#include "shsm_re.h"

#include <string.h>

/* ---- small helpers ---------------------------------------------------------------------------------------------- */

const char *draft_root(AvpArena *a, const ShsmRepo *r) {
    return avp_path_join(a, avp_path_join(a, r->root, "build"), "draft");
}

const char *draft_rejects_path(AvpArena *a, const ShsmRepo *r) {
    return avp_path_join(a, r->config_dir, "draft_rejects.tsv");
}

static const char *slashes(AvpArena *a, const char *p) {
    return avp_cz(a, avp_str_replace(a, avp_str(p), AVP_S("\\"), AVP_S("/"), -1));
}

const char *draft_rel_path(AvpArena *a, const ShsmRepo *r, const char *path) {
    AvpTry t;
    const char *out = NULL;
    if (AVP_TRY(&t)) {
        out = shsm_repo_rel(a, r, path);
        avp_try_end(&t);
        return out;
    }
    if (t.code != SHSM_EXC_VALUE) avp_throw(t.code, "%s", t.msg);
    return slashes(a, avp_path_abspath(a, path));
}

const char *draft_safe_name(AvpArena *a, const char *sym) {
    char *out = avp_cz(a, avp_str(sym));
    for (char *p = out; *p; p++)
        if (strchr("\\/:*?\"<>|", *p)) *p = '_';
    return out;
}

const char *draft_strip_ext(AvpArena *a, const char *path) {
    if (!path || !*path) return path;
    AvpStr root, ext;
    avp_path_splitext(path, &root, &ext);
    return avp_cz(a, root);
}

const char *draft_plain_unit(AvpArena *a, const char *u) {
    AvpStr s = avp_str(u);
    if (avp_str_startswith(s, AVP_S("main/"))) s = avp_str_slice(s, 5, AVP_END);
    return draft_strip_ext(a, avp_cz(a, s));
}

static i64 hex_value(AvpStr s) {
    i64 v = 0;
    for (size_t i = 0; i < s.n; i++) {
        char c = s.p[i];
        v = v * 16 + (c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10);
    }
    return v;
}

bool draft_parse_range(AvpStr text, i64 *start, i64 *end) {
    const ShsmRe *re = shsm_re("^(?:0x)?([0-9A-Fa-f]{8})\\s*-\\s*(?:0x)?([0-9A-Fa-f]{8})$", 0);
    ShsmReMatch m;
    if (!shsm_re_match(re, avp_str_strip(text), 0, AVP_END, &m)) return false;
    *start = hex_value(shsm_re_group(&m, 1));
    *end = hex_value(shsm_re_group(&m, 2));
    return true;
}

AvpStr draft_split_mangled(AvpStr sym, AvpStr *rest) {
    i64 i = avp_str_find(sym, AVP_S("__"), avp_str_startswith(sym, AVP_S("__")) ? 2 : 1, AVP_END);
    if (i <= 0) {
        *rest = (AvpStr){NULL, 0};
        return sym;
    }
    *rest = avp_str_slice(sym, i + 2, AVP_END);
    if (!rest->p) rest->p = sym.p + sym.n; /* an empty rest is "", not None */
    return avp_strn(sym.p, (size_t)i);
}

const char *draft_demangled_parts(AvpArena *a, const char *sym, const char **cls) {
    *cls = NULL;
    ShsmDemangled *p = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        p = shsm_mangle_parse(a, sym);
        avp_try_end(&t);
    } else {
        p = NULL;
    }
    if (!p) return sym;
    const char *name = p->raw_name && *p->raw_name ? p->raw_name : p->name;
    if (strcmp(p->kind, "function") == 0 && p->nscope && name &&
        (strcmp(name, "__ct") == 0 || strcmp(name, "__dt") == 0)) {
        const char *last = p->scope[p->nscope - 1];
        name = strcmp(name, "__ct") == 0 ? last : avp_sprintf(a, "~%s", last);
    }
    if (p->nscope) *cls = p->scope[p->nscope - 1];
    return name ? name : p->name;
}

const char *draft_c_escape(AvpArena *a, AvpStr text) {
    AvpBuf b;
    avp_buf_init(&b, a);
    const char *p = text.p, *end = text.p + text.n;
    while (p < end) {
        u32 cp = avp_u8_next(&p, end);
        if (cp == '\\') avp_buf_putz(&b, "\\\\");
        else if (cp == '"') avp_buf_putz(&b, "\\\"");
        else if (cp == '\n') avp_buf_putz(&b, "\\n");
        else if (cp == '\t') avp_buf_putz(&b, "\\t");
        else if (cp < 32 || cp > 126) avp_buf_printf(&b, "\\x%02X", (unsigned)cp);
        else avp_buf_putc(&b, (char)cp);
    }
    return avp_buf_cz(&b);
}

bool draft_addr_name(AvpStr name, i64 *addr) {
    ShsmReMatch m;
    if (!shsm_re_match(shsm_re("^(?:fn|lbl)_([0-9A-F]{8})$", 0), name, 0, AVP_END, &m)) return false;
    if (addr) *addr = hex_value(shsm_re_group(&m, 1));
    return true;
}

/* ---- the substitution map ---------------------------------------------------------------------------------------- */

AvpStr draft_smap_get(const AvpMap *m, AvpStr key) {
    AvpVal *v = avp_map_get(m, key);
    return v ? *(const AvpStr *)v->p : (AvpStr){NULL, 0};
}

void draft_smap_set(AvpArena *a, AvpMap *m, AvpStr key, AvpStr val) {
    AvpStr *box = AVP_NEW(a, AvpStr);
    *box = avp_str_dup(a, val);
    if (!box->p) box->p = "";
    avp_map_put(m, key, NULL)->p = box;
}

DraftSubst *draft_subst_new(AvpArena *a) {
    DraftSubst *s = AVP_NEW(a, DraftSubst);
    s->a = a;
    s->idents = avp_map_new();
    s->classes = avp_map_new();
    s->strings = avp_map_new();
    s->renames = avp_map_new();
    s->addresses = avp_imap_new();
    return s;
}

/* a copy; p NULL (None) stays NULL */
static AvpStr dup(AvpArena *a, AvpStr s) {
    if (!s.p) return s;
    AvpStr d = avp_str_dup(a, s);
    if (!d.p) d.p = "";
    return d;
}

void draft_subst_add(DraftSubst *s, AvpMap *table, const char *kind, AvpStr old, AvpStr new_, AvpStr why) {
    AvpStr cur = draft_smap_get(table, old);
    if (cur.p && !avp_str_eq(cur, new_)) {
        avp_vec_push(&s->conflicts, avp_pyfmt(s->a, "%s %S -> %S and %S", kind, &old, &cur, &new_));
        return;
    }
    if (!cur.p) {
        draft_smap_set(s->a, table, old, new_);
        avp_vec_push(&s->evidence, ((DraftEvidence){avp_str(kind), dup(s->a, old), dup(s->a, new_), dup(s->a, why)}));
    }
}

void draft_subst_unmapped(DraftSubst *s, const char *kind, AvpStr name, AvpStr why) {
    avp_vec_push(&s->unmapped, ((DraftUnmapped){avp_str(kind), dup(s->a, name), dup(s->a, why)}));
}

static void put_str(AvpBuf *b, AvpStr s) { avp_buf_put(b, s.p ? s : AVP_S("None")); } /* str(x) */

static AvpJson *smap_json(AvpArena *a, const AvpMap *m) {
    AvpJson *o = avp_json_obj(a);
    AvpMapIter it = {0};
    while (avp_map_next(m, &it)) avp_json_set(a, o, it.key, avp_json_str(a, *(const AvpStr *)it.val->p));
    return o;
}

static AvpJson *strs_json(AvpArena *a, const AvpStr *v, size_t n) {
    AvpJson *arr = avp_json_arr(a);
    for (size_t i = 0; i < n; i++) avp_json_arr_push(a, arr, v[i].p ? avp_json_str(a, v[i]) : avp_json_null());
    return arr;
}

AvpJson *draft_subst_json(AvpArena *a, const DraftSubst *s) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("idents"), smap_json(a, s->idents));
    avp_json_set(a, o, AVP_S("classes"), smap_json(a, s->classes));
    avp_json_set(a, o, AVP_S("strings"), smap_json(a, s->strings));
    AvpJson *addr = avp_json_obj(a);
    AvpIMapIter it = {0};
    while (avp_imap_next(s->addresses, &it))
        avp_json_set(a, addr, avp_str(avp_sprintf(a, "0x%08llX", (unsigned long long)it.key)),
                     avp_json_strz(a, avp_sprintf(a, "0x%08llX", (unsigned long long)it.val->i)));
    avp_json_set(a, o, AVP_S("addresses"), addr);
    avp_json_set(a, o, AVP_S("renames"), smap_json(a, s->renames));
    AvpJson *ev = avp_json_arr(a);
    for (size_t i = 0; i < s->evidence.n; i++) avp_json_arr_push(a, ev, strs_json(a, &s->evidence.v[i].kind, 4));
    avp_json_set(a, o, AVP_S("evidence"), ev);
    AvpJson *un = avp_json_arr(a);
    for (size_t i = 0; i < s->unmapped.n; i++) avp_json_arr_push(a, un, strs_json(a, &s->unmapped.v[i].kind, 3));
    avp_json_set(a, o, AVP_S("unmapped"), un);
    avp_json_set(a, o, AVP_S("conflicts"), strs_json(a, s->conflicts.v, s->conflicts.n));
    return o;
}

AvpStr draft_subst_tsv(AvpArena *a, const DraftSubst *s) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, "kind\told\tnew\tevidence");
    for (size_t i = 0; i < s->evidence.n; i++) {
        const DraftEvidence *e = &s->evidence.v[i];
        avp_buf_putc(&b, '\n');
        put_str(&b, e->kind);
        avp_buf_putc(&b, '\t');
        put_str(&b, e->old);
        avp_buf_putc(&b, '\t');
        put_str(&b, e->new_);
        avp_buf_putc(&b, '\t');
        put_str(&b, e->why);
    }
    for (size_t i = 0; i < s->unmapped.n; i++) {
        const DraftUnmapped *u = &s->unmapped.v[i];
        avp_buf_putz(&b, "\nunmapped-");
        put_str(&b, u->kind);
        avp_buf_putc(&b, '\t');
        put_str(&b, u->name);
        avp_buf_putz(&b, "\t\t");
        put_str(&b, u->why);
    }
    for (size_t i = 0; i < s->conflicts.n; i++) {
        avp_buf_putz(&b, "\nconflict\t");
        put_str(&b, s->conflicts.v[i]);
        avp_buf_putz(&b, "\t\t");
    }
    avp_buf_putc(&b, '\n');
    return avp_buf_str(&b);
}

void draft_pair_relocations(AvpArena *a, const ShsmAsmRow *twin, size_t ntwin, const ShsmAsmRow *target,
                            size_t ntarget, DraftPairs *pairs, AvpStrVec *problems) {
    if (ntwin != ntarget)
        avp_vec_push(problems, avp_str(avp_sprintf(a, "row counts differ (%zu, %zu)", ntwin, ntarget)));
    size_t n = ntwin < ntarget ? ntwin : ntarget;
    for (size_t i = 0; i < n; i++) {
        const ShsmAsmRow *ra = &twin[i], *rb = &target[i];
        if (!ra->reloc && !rb->reloc) continue;
        if (!ra->sym.n || !rb->sym.n) {
            avp_vec_push(problems, avp_str(avp_sprintf(a, "relocation on one side only at 0x%08llX",
                                                       (unsigned long long)rb->address)));
            continue;
        }
        avp_vec_push(pairs, ((DraftPair){ra->sym, ra->sym_off == SHSM_NONE ? 0 : ra->sym_off, rb->sym,
                                         rb->sym_off == SHSM_NONE ? 0 : rb->sym_off}));
    }
}

bool draft_own_address_name(AvpStr sym, i64 address) {
    AvpStr rest;
    AvpStr plain = draft_split_mangled(sym, &rest);
    i64 addr;
    return draft_addr_name(plain, &addr) && address != DRAFT_NONE && addr == address;
}

const char *draft_remangle(AvpArena *a, AvpStr sym, const AvpMap *classes, const AvpMap *idents) {
    AvpStr rest;
    AvpStr plain = draft_split_mangled(sym, &rest);
    AvpStr mapped = draft_smap_get(idents, plain);
    if (mapped.p) plain = mapped;
    if (!rest.p) return avp_cz(a, plain);
    AvpMapIter it = {0};
    while (avp_map_next(classes, &it)) {
        AvpStr old = it.key, new_ = *(const AvpStr *)it.val->p;
        AvpStr esc = shsm_re_escape(a, old);
        AvpStr pat = avp_pyfmt(a, "(?<!\\d)%d%S", (int)avp_u8_len(old), &esc);
        AvpStr repl = avp_pyfmt(a, "%d%S", (int)avp_u8_len(new_), &new_);
        rest = shsm_re_sub(a, shsm_re_try_n(pat, 0), repl, rest, 0);
    }
    return avp_sprintf(a, "%.*s__%.*s", AVP_SF(plain), AVP_SF(rest));
}

DraftSubst *draft_build_subst(AvpArena *a, const DraftPairs *pairs, DraftInfoFn info, void *info_ctx,
                              AvpStr twin_sym, i64 twin_addr, AvpStr target_sym, i64 target_addr,
                              const char *twin_class, const char *target_class, const char *class_why) {
    DraftSubst *s = draft_subst_new(a);
    if (twin_class && target_class && strcmp(twin_class, target_class) != 0)
        draft_subst_add(s, s->classes, "class", avp_str(twin_class), avp_str(target_class),
                        class_why ? avp_str(class_why) : (AvpStr){NULL, 0});
    avp_imap_put(s->addresses, (u64)twin_addr, NULL)->i = target_addr;
    DraftPairs pending = {0}; /* (gs, 0, ts, 0) */
    size_t total = pairs->n + 1;
    for (size_t k = 0; k < total; k++) {
        DraftPair p = k == 0 ? (DraftPair){twin_sym, 0, target_sym, 0} : pairs->v[k - 1];
        const char *why = k == 0 ? "the function itself" : "relocation";
        AvpStr ts = p.tsym, gs = p.gsym;
        if (avp_str_eq(ts, gs) && p.toff == p.goff) continue;
        DraftInfo ti = info(info_ctx, ts, p.toff), gi = info(info_ctx, gs, p.goff);
        if (ti.string.p && gi.string.p) {
            if (!avp_str_eq(ti.string, gi.string))
                draft_subst_add(s, s->strings, "string", ti.string, gi.string,
                                avp_pyfmt(a, "%S+%lld -> %S+%lld", &ts, (long long)p.toff, &gs, (long long)p.goff));
            continue;
        }
        if (avp_str_eq(ts, gs)) {
            draft_subst_unmapped(s, "offset", ts,
                                 avp_pyfmt(a, "same symbol, offsets %lld and %lld", (long long)p.toff, (long long)p.goff));
            continue;
        }
        const char *tcls, *gcls;
        const char *tplain = draft_demangled_parts(a, avp_cz(a, ts), &tcls);
        const char *gplain = draft_demangled_parts(a, avp_cz(a, gs), &gcls);
        AvpStr grest, trest;
        AvpStr gbase = draft_split_mangled(gs, &grest);
        draft_split_mangled(ts, &trest);
        bool g_mangled = grest.p != NULL;
        if (tcls && gcls && strcmp(tcls, gcls) != 0)
            draft_subst_add(s, s->classes, "class", avp_str(tcls), avp_str(gcls),
                            avp_pyfmt(a, "scope of %S -> %S", &ts, &gs));
        if (draft_own_address_name(ts, ti.address)) {
            AvpStr new_plain = g_mangled ? avp_str(gplain) : gbase;
            if (!avp_str_eq(avp_str(tplain), new_plain))
                draft_subst_add(s, s->idents, "ident", avp_str(tplain), new_plain,
                                avp_pyfmt(a, "%s: %S -> %S", why, &ts, &gs));
        } else if (!g_mangled && strcmp(tplain, gplain) != 0 && !draft_addr_name(avp_str(gplain), NULL) && !trest.p) {
            /* the target already has its real (retail) name: it wins */
            draft_subst_add(s, s->idents, "ident", avp_str(tplain), avp_str(gplain),
                            avp_pyfmt(a, "%s: %S -> %S (the target's own name)", why, &ts, &gs));
        } else if (strcmp(tplain, gplain) != 0 && g_mangled) {
            draft_subst_unmapped(s, "name", ts,
                                 avp_pyfmt(a, "named %s in the target (%S): source keeps %s", gplain, &gs, tplain));
        } else if (draft_addr_name(gs, NULL)) {
            /* the target copy still has its placeholder name: the draft defines the twin's name (in the target's
             * class), so the retail symbol takes that name */
            avp_vec_push(&pending, ((DraftPair){gs, 0, ts, 0}));
        }
    }
    for (size_t k = 0; k < pending.n; k++)
        draft_smap_set(a, s->renames, pending.v[k].tsym,
                       avp_str(draft_remangle(a, pending.v[k].gsym, s->classes, s->idents)));
    avp_vec_free(&pending);
    return s;
}

/* ---- substitute ------------------------------------------------------------------------------------------------- */

static const char TOKEN[] = "(?P<str>\"(?:\\\\.|[^\"\\\\\\n])*\")|(?P<chr>'(?:\\\\.|[^'\\\\\\n])*')"
                            "|(?P<hex>\\b0x[0-9A-Fa-f]{8}\\b)|(?P<id>[A-Za-z_][A-Za-z0-9_]*)";

typedef struct {
    AvpArena *a;
    const DraftSubst *s;
    AvpMap *strings; /* '"%s"' % c_escape(k) -> '"%s"' % c_escape(v) */
    AvpMap *upper;   /* k.upper() -> v.upper() */
} SubstCtx;

static bool ascii_upper_only(AvpStr t) { /* t == t.upper() for an ASCII identifier */
    for (size_t i = 0; i < t.n; i++)
        if (t.p[i] >= 'a' && t.p[i] <= 'z') return false;
    return true;
}

static void rep(void *ctx, const ShsmReMatch *m, AvpBuf *out) {
    SubstCtx *c = ctx;
    AvpStr t = shsm_re_group(m, 0);
    if (shsm_re_matched(m, 1)) {
        AvpStr v = draft_smap_get(c->strings, t);
        avp_buf_put(out, v.p ? v : t);
        return;
    }
    if (shsm_re_matched(m, 2)) {
        avp_buf_put(out, t);
        return;
    }
    if (shsm_re_matched(m, 3)) {
        AvpVal *v = avp_imap_get(c->s->addresses, (u64)hex_value(avp_str_slice(t, 2, AVP_END)));
        if (!v) avp_buf_put(out, t);
        else avp_buf_printf(out, "0x%08llX", (unsigned long long)v->i);
        return;
    }
    AvpStr v = draft_smap_get(c->s->idents, t);
    if (v.p) {
        avp_buf_put(out, v);
        return;
    }
    v = draft_smap_get(c->s->classes, t);
    if (v.p) {
        avp_buf_put(out, v);
        return;
    }
    if (avp_str_isupper(t) || (avp_str_contains(t, AVP_S("_")) && ascii_upper_only(t))) {
        AvpMapIter it = {0};
        while (avp_map_next(c->upper, &it))
            if (avp_str_contains(t, it.key)) t = avp_str_replace(c->a, t, it.key, *(const AvpStr *)it.val->p, -1);
    }
    avp_buf_put(out, t);
}

AvpStr draft_substitute(AvpArena *a, AvpStr text, const DraftSubst *s) {
    SubstCtx c = {a, s, avp_map_new(), avp_map_new()};
    AvpMapIter it = {0};
    while (avp_map_next(s->strings, &it)) {
        AvpStr k = avp_str(avp_sprintf(a, "\"%s\"", draft_c_escape(a, it.key)));
        AvpStr v = avp_str(avp_sprintf(a, "\"%s\"", draft_c_escape(a, *(const AvpStr *)it.val->p)));
        draft_smap_set(a, c.strings, k, v);
    }
    it = (AvpMapIter){0};
    while (avp_map_next(s->classes, &it))
        draft_smap_set(a, c.upper, avp_str_upper(a, it.key), avp_str_upper(a, *(const AvpStr *)it.val->p));
    AvpStr out = shsm_re_sub_fn(a, shsm_re(TOKEN, 0), rep, &c, text, 0, NULL);
    avp_map_free(c.strings);
    avp_map_free(c.upper);
    return out;
}

/* ---- clone_header ----------------------------------------------------------------------------------------------- */

AvpStr draft_clone_header(AvpArena *a, AvpStr hdr, const char *hdr_inc, const char *tcls, const char *gcls,
                          const DraftSubst *s, const AvpSet *used, AvpStrVec *dropped) {
    i64 bs = 0, be = 0;
    bool blk = tcls && draft_class_block(a, hdr, tcls, &bs, &be);
    AvpStrVec lines = {0};
    AvpStr up = avp_str_upper(a, avp_str(gcls)); /* (gcls or "").upper() */
    AvpStr guard = avp_pyfmt(a, "GAME_%S_H", &up);
    avp_vec_push(&lines, avp_pyfmt(a, "#ifndef %S", &guard));
    avp_vec_push(&lines, avp_pyfmt(a, "#define %S", &guard));
    avp_vec_push(&lines, AVP_S(""));
    AvpStrVec incs = draft_includes_of(hdr);
    for (size_t i = 0; i < incs.n; i++) avp_vec_push(&lines, avp_pyfmt(a, "#include <%S>", &incs.v[i]));
    avp_vec_free(&incs);
    ShsmReFindall f = shsm_re_findall(shsm_re("^\\s*(?:class|struct)\\s+(\\w+)[^;{]*\\{", SHSM_RE_M), hdr, 0, AVP_END);
    bool others = false;
    for (size_t i = 0; i < f.count && !others; i++)
        others = !tcls || !avp_str_eqz(shsm_re_findall_at(&f, i, 0), tcls);
    shsm_re_findall_free(&f);
    if (others) avp_vec_push(&lines, avp_str(avp_sprintf(a, "#include <%s>", hdr_inc ? hdr_inc : "None")));
    avp_vec_push(&lines, AVP_S(""));
    if (blk) {
        AvpStr decl = draft_substitute(a, avp_strn(hdr.p + bs, (size_t)(be - bs)), s);
        AvpStr repl = avp_str(avp_sprintf(a, "/*\n * Same layout and members as %s (twin draft). Member names "
                                             "invented.\n */\n", tcls));
        decl = shsm_re_sub(a, shsm_re("^/\\*.*?\\*/\\s*\\n", SHSM_RE_S), repl, decl, 1);
        avp_vec_push(&lines, decl);
        avp_vec_push(&lines, AVP_S(""));
    }
    const ShsmRe *ext = shsm_re("^\\s*extern\\b.*?\\b((?:fn|lbl)_[0-9A-F]{8})\\b", 0);
    AvpStrVec hl = avp_str_split(hdr, AVP_S("\n"), -1);
    for (size_t i = 0; i < hl.n; i++) {
        ShsmReMatch m;
        if (!shsm_re_match(ext, hl.v[i], 0, AVP_END, &m)) continue;
        AvpStr name = shsm_re_group(&m, 1);
        if (!avp_map_has(s->idents, name) && !avp_set_has(used, name)) {
            avp_vec_push(dropped, name);
            continue;
        }
        avp_vec_push(&lines, draft_substitute(a, hl.v[i], s));
    }
    avp_vec_free(&hl);
    avp_vec_push(&lines, AVP_S(""));
    avp_vec_push(&lines, AVP_S("#endif"));
    avp_vec_push(&lines, AVP_S(""));
    AvpStr out = avp_str_join(a, AVP_S("\n"), lines.v, lines.n);
    avp_vec_free(&lines);
    return out;
}

/* ---- grading helpers -------------------------------------------------------------------------------------------- */

/* Ghidra-style artefacts: a draft carrying any of them was pasted from a decompiler listing */
static const char *const SCREEN[][2] = {
    {"\\bparam_\\d+\\b", "Ghidra parameter name"},
    {"\\b[a-z]{1,2}Var\\d+\\b", "Ghidra variable name"},
    {"\\blocal_[0-9a-fA-F]+\\b", "Ghidra stack-slot name"},
    {"\\b(?:DAT|PTR|LAB|FUN|SUB|switchD)_[0-9a-fA-F]{6,8}\\b", "Ghidra address label"},
    {"\\bundefined[1248]?\\b", "Ghidra type"},
    {"\\b(?:extraout|unaff|in)_[A-Za-z0-9]+\\b", "Ghidra register artefact"},
    {"\\b(?:CONCAT|SUB|ZEXT|SEXT)\\d\\d\\s*\\(", "Ghidra pseudo-op"},
    {"\\(code\\s*\\*\\)", "Ghidra code pointer cast"},
    {"reading aid", "the listing's reading-aid notice"},
};

DraftScreenHits draft_screen(AvpStr text) {
    DraftScreenHits hits = {0};
    AvpStrVec lines = avp_str_split(text, AVP_S("\n"), -1);
    for (size_t i = 0; i < lines.n; i++) {
        for (size_t k = 0; k < sizeof SCREEN / sizeof *SCREEN; k++) {
            ShsmReMatch m;
            if (shsm_re_search(shsm_re(SCREEN[k][0], 0), lines.v[i], 0, AVP_END, &m))
                avp_vec_push(&hits, ((DraftScreenHit){(i64)i + 1, SCREEN[k][1], shsm_re_group(&m, 0)}));
        }
    }
    avp_vec_free(&lines);
    return hits;
}

DraftMarks draft_marked_functions(AvpArena *a, AvpStr text) {
    DraftDefs defs = draft_scan_source(a, text);
    DraftMarks out = {0};
    for (size_t i = 0; i < defs.n; i++)
        if (defs.v[i].address != DRAFT_NONE)
            avp_vec_push(&out, ((DraftMark){defs.v[i].address, defs.v[i].name, defs.v[i].cls}));
    avp_vec_free(&defs);
    return out;
}

const char *draft_compiled_name_for(AvpArena *a, const char *const *names, size_t n, const char *name,
                                    const char *cls) {
    bool no_cls = !cls || !*cls;
    if (no_cls)
        for (size_t i = 0; i < n; i++)
            if (strcmp(names[i], name) == 0) return names[i];
    for (size_t i = 0; i < n; i++) {
        const char *c;
        const char *p = draft_demangled_parts(a, names[i], &c);
        bool c_none = !c || !*c;
        if (strcmp(p, name) == 0 && (c_none ? no_cls : (!no_cls && strcmp(c, cls) == 0))) return names[i];
    }
    for (size_t i = 0; i < n; i++) {
        AvpStr rest;
        if (avp_str_eqz(draft_split_mangled(avp_str(names[i]), &rest), name) && (!cls || strstr(names[i], cls)))
            return names[i];
    }
    return NULL;
}

typedef struct {
    const AvpMap *renames;
} RenCtx;

static void ren(void *ctx, const ShsmReMatch *m, AvpBuf *out) {
    avp_buf_put(out, draft_smap_get(((RenCtx *)ctx)->renames, shsm_re_group(m, 1)));
}

static int by_len_desc(const void *x, const void *y, void *ctx) {
    (void)ctx;
    size_t a = avp_u8_len(*(const AvpStr *)x), b = avp_u8_len(*(const AvpStr *)y);
    return a > b ? -1 : a < b;
}

bool draft_rename_explains(AvpArena *a, AvpStr retail, AvpStr compiled, AvpStr detail, const AvpMap *renames) {
    if (!avp_map_len(renames)) return false;
    AvpStrVec keys = avp_map_keys(renames);
    avp_sort(keys.v, keys.n, sizeof *keys.v, by_len_desc, NULL, false);
    AvpBuf pat;
    avp_buf_init(&pat, a);
    avp_buf_putz(&pat, "(?<![\\w$@])(");
    for (size_t i = 0; i < keys.n; i++) {
        if (i) avp_buf_putc(&pat, '|');
        avp_buf_put(&pat, shsm_re_escape(a, keys.v[i]));
    }
    avp_buf_putz(&pat, ")(?![\\w$])");
    avp_vec_free(&keys);
    RenCtx c = {renames};
    AvpStr rt = retail.p ? retail : AVP_S(""), ct = compiled.p ? compiled : AVP_S("");
    if (!avp_str_eq(shsm_re_sub_fn(a, shsm_re_try_n(avp_buf_str(&pat), 0), ren, &c, rt, 0, NULL), ct)) return false;
    const ShsmRe *pt = shsm_re("points to (\\S+?)\\+0x[0-9A-Fa-f]+ \\(0x[0-9A-Fa-f]+\\), retail 0x[0-9A-Fa-f]+ is (\\S+)", 0);
    ShsmReFindall f = shsm_re_findall(pt, detail.p ? detail : AVP_S(""), 0, AVP_END);
    bool ok = true;
    for (size_t i = 0; i < f.count && ok; i++) {
        AvpStr comp = shsm_re_findall_at(&f, i, 0), ret = shsm_re_findall_at(&f, i, 1);
        AvpStr v = draft_smap_get(renames, ret);
        if (!avp_str_eq(v.p ? v : ret, comp)) ok = false;
    }
    shsm_re_findall_free(&f);
    return ok;
}

bool draft_class_defines_member(AvpArena *a, AvpStr header, const char *cls, const char *member) {
    i64 b0, b1;
    if (!draft_class_block(a, header, cls, &b0, &b1)) return false;
    AvpStr esc = shsm_re_escape(a, avp_str(member));
    AvpStr pat = avp_pyfmt(a, "(?:^|[^\\w:~])%S\\s*\\([^;{)]*\\)\\s*(?:const\\s*)?\\{", &esc);
    ShsmReMatch m;
    return shsm_re_search(shsm_re_try_n(pat, 0), avp_strn(header.p + b0, (size_t)(b1 - b0)), 0, AVP_END, &m);
}

AvpStr draft_declare_member(AvpArena *a, AvpStr header, const char *member) {
    AvpStr esc = shsm_re_escape(a, avp_str(member));
    AvpStr pat = avp_pyfmt(a, "([^\\w:~])(%S\\s*\\([^;{)]*\\)(?:\\s*const)?)\\s*\\{[^{}]*\\}", &esc);
    return shsm_re_sub(a, shsm_re_try_n(pat, 0), AVP_S("\\1\\2;"), header, 1);
}
