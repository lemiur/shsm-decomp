/* classes decl: the model (classes_decl.h): vtable matching, slot names and kinds, declarations, rendering. */
#include "classes_decl.h"

#include "shsm_mangle.h"

#include <ctype.h>
#include <string.h>

const char *const CD_SRC_NAMES[CD_NSRC] = {"placeholder", "retail", "pairing", "sibling vtable", "derived"};

static bool is_pure(const char *s) { return !s || !*s || strcmp(s, "__pure_virtual") == 0; }

/* fn_<8 hex digits>, alone or as a mangled member's base name. */
static bool is_placeholder(const char *s) {
    if (!s || strncmp(s, "fn_", 3) != 0) return false;
    for (int i = 3; i < 11; i++)
        if (!isxdigit((unsigned char)s[i])) return false;
    return s[11] == 0 || (s[11] == '_' && s[12] == '_');
}

static ShsmDemangled *parse(AvpArena *a, const char *mangled) {
    if (!mangled || !*mangled) return NULL;
    ShsmDemangled *d = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        d = shsm_mangle_parse(a, mangled);
        avp_try_end(&t);
    } else {
        d = NULL;
    }
    if (d && (d->is_data || (d->thunk && *d->thunk))) return NULL;
    return d;
}

const char *cd_scope_of(AvpArena *a, const char *mangled) {
    ShsmDemangled *d = parse(a, mangled);
    if (!d || !d->nscope) return NULL;
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < d->nscope; i++) avp_buf_printf(&b, "%s%s", i ? "::" : "", d->scope[i]);
    return avp_cz(a, avp_buf_str(&b));
}

/* The name a retail word is known by for matching: a mangled member symbol that is no placeholder, else its pair. */
static const char *known_name(AvpArena *a, const CdWord *w) {
    if (is_pure(w->retail)) return NULL;
    if (!is_placeholder(w->retail) && cd_scope_of(a, w->retail)) return w->retail;
    return w->pair;
}

CdVtMatch cd_match_vtable(AvpArena *a, const CdWord *w, size_t n, const CdSibVt *c, size_t nc, const char *prefer) {
    CdVtMatch best = {.index = -1};
    const char **known = AVP_NEWN(a, const char *, n + 1);
    for (size_t i = 0; i < n; i++) known[i] = known_name(a, &w[i]);
    AVP_VEC(const char *) tied = {0};
    size_t best_agree = 0;
    i64 best_i = -1;
    for (size_t k = 0; k < nc; k++) {
        const CdSibVt *s = &c[k];
        if (s->n > n || s->n == 0) continue;
        bool fit = true;
        for (size_t i = s->n; i < n && fit; i++) fit = is_pure(w[i].retail);
        size_t agree = 0, named = 0, compared = 0;
        for (size_t i = 0; i < s->n && fit; i++) {
            if (is_pure(w[i].retail)) {
                compared++;
                if (is_pure(s->slots[i])) agree++;
                else fit = false;
            } else if (known[i]) {
                compared++;
                if (s->slots[i] && strcmp(s->slots[i], known[i]) == 0) agree++, named++;
                else fit = false;
            }
        }
        if (!fit || !named) continue;
        if (best_i < 0 || agree > best_agree) {
            best_i = (i64)k;
            best_agree = agree;
            best.compared = compared;
            tied.n = 0;
        } else if (agree == best_agree) {
            bool kp = prefer && strcmp(s->cls, prefer) == 0, bp = prefer && strcmp(c[best_i].cls, prefer) == 0;
            if (kp && !bp) {
                avp_vec_push(&tied, c[best_i].symbol);
                best_i = (i64)k;
                best.compared = compared;
            } else {
                avp_vec_push(&tied, s->symbol);
            }
        }
    }
    if (best_i < 0) return best;
    best.agree = best_agree;
    best.used = c[best_i].n;
    /* ties survive only when the preferred name did not decide */
    bool decided = prefer && strcmp(c[best_i].cls, prefer) == 0;
    if (tied.n && !decided) {
        avp_vec_push(&tied, c[best_i].symbol);
        best.tied = tied.v;
        best.ntied = tied.n;
        best.index = -1;
        return best;
    }
    best.index = best_i;
    return best;
}

void cd_name_slots(AvpArena *a, const CdWord *w, size_t n, const CdSibVt *sib, CdName *out) {
    for (size_t i = 0; i < n; i++) {
        CdName *o = &out[i];
        *o = (CdName){0};
        const char *sv = sib && i < sib->n ? sib->slots[i] : NULL;
        if (sv && !*sv) sv = NULL;
        if (is_pure(w[i].retail)) {
            o->pure = true;
            continue;
        }
        if (!is_placeholder(w[i].retail) && cd_scope_of(a, w[i].retail)) {
            o->name = w[i].retail;
            o->src = CD_SRC_RETAIL;
        } else if (w[i].pair) {
            o->name = w[i].pair;
            o->src = CD_SRC_PAIR;
        } else if (sv && !is_pure(sv)) {
            o->name = sv;
            o->src = CD_SRC_SIBVT;
        }
        if (o->name && sv && !is_pure(sv) && strcmp(sv, o->name) != 0) o->other = sv;
        if (o->name && o->src != CD_SRC_SIBVT && w[i].pair && strcmp(w[i].pair, o->name) != 0) o->other = w[i].pair;
    }
}

size_t cd_infer_base_count(AvpArena *a, const CdName *names, size_t n, const char *const *anc, size_t nanc) {
    size_t count = 0;
    for (size_t i = 0; i < n; i++) {
        const char *s = names[i].name ? cd_scope_of(a, names[i].name) : NULL;
        if (!s) continue;
        for (size_t k = 0; k < nanc; k++)
            if (strcmp(s, anc[k]) == 0) count = i + 1;
    }
    return count;
}

void cd_classify(AvpArena *a, const char *cls, const CdName *names, size_t n, size_t nbase, const CdName *base,
                 size_t nbase_names, int *kind) {
    for (size_t i = 0; i < n; i++) {
        if (i >= nbase) {
            kind[i] = CD_KIND_NEW;
            continue;
        }
        const CdName *b = base && i < nbase_names ? &base[i] : NULL;
        if (b && (b->name || b->pure)) {
            bool same = (b->pure && names[i].pure) ||
                        (b->name && names[i].name && strcmp(b->name, names[i].name) == 0);
            kind[i] = same ? CD_KIND_INHERITED : CD_KIND_OVERRIDE;
            continue;
        }
        if (names[i].pure) {
            kind[i] = CD_KIND_INHERITED; /* a pure slot below the base's count is the base's */
            continue;
        }
        const char *s = names[i].name ? cd_scope_of(a, names[i].name) : NULL;
        kind[i] = !s || strcmp(s, cls) == 0 ? CD_KIND_OVERRIDE : CD_KIND_INHERITED;
    }
}

/* "FMOD::System**" -> "System**" inside namespace FMOD. */
static const char *strip_ns(AvpArena *a, const char *t, const char *ns) {
    if (!ns || !*ns) return t;
    const char *pre = avp_sprintf(a, "%s::", ns);
    size_t pn = strlen(pre);
    AvpBuf b;
    avp_buf_init(&b, a);
    for (const char *p = t; *p;) {
        bool word_start = p == t || !(isalnum((unsigned char)p[-1]) || p[-1] == '_' || p[-1] == ':');
        if (word_start && strncmp(p, pre, pn) == 0) {
            p += pn;
            continue;
        }
        avp_buf_putc(&b, *p++);
    }
    return avp_cz(a, avp_buf_str(&b));
}

/* "float**" + "out" -> "float **out"; a function pointer keeps no name. */
static const char *param_decl(AvpArena *a, const char *type, const char *name) {
    if (strchr(type, '(')) return type;
    if (!name) name = "";
    size_t n = strlen(type), k = n;
    while (k > 0 && (type[k - 1] == '*' || type[k - 1] == '&')) k--;
    if (k == n) return *name ? avp_sprintf(a, "%s %s", type, name) : type;
    size_t e = k;
    while (e > 0 && type[e - 1] == ' ') e--;
    return avp_sprintf(a, "%.*s %s%s", (int)e, type, type + k, name);
}

const char *cd_method_decl(AvpArena *a, const char *cls_short, const char *ns, const char *mangled, const CdSig *sig,
                           bool pure, const char *placeholder, const char *fallback_ret) {
    ShsmDemangled *d = parse(a, mangled);
    const char *ret = sig && sig->ret ? sig->ret
                                      : avp_sprintf(a, "/* return type unconfirmed */ %s",
                                                    fallback_ret ? fallback_ret : "void");
    AvpBuf b;
    avp_buf_init(&b, a);
    if (!d) {
        avp_buf_printf(&b, "virtual %s %s()%s", ret, placeholder ? placeholder : "unknown", pure ? " = 0" : "");
        return avp_cz(a, avp_buf_str(&b));
    }
    if (d->name[0] == '~') {
        avp_buf_printf(&b, "virtual ~%s()%s", cls_short, pure ? " = 0" : "");
        return avp_cz(a, avp_buf_str(&b));
    }
    avp_buf_printf(&b, "virtual %s %s(", ret, d->name);
    bool named = sig && sig->pnames && sig->npn == d->nparams;
    for (size_t i = 0; i < d->nparams; i++) {
        const char *t = strip_ns(a, d->params[i], ns);
        avp_buf_printf(&b, "%s%s", i ? ", " : " ", param_decl(a, t, named ? sig->pnames[i] : NULL));
    }
    avp_buf_putz(&b, d->nparams ? " )" : ")");
    if (d->is_const) avp_buf_putz(&b, " const");
    if (pure) avp_buf_putz(&b, " = 0");
    return avp_cz(a, avp_buf_str(&b));
}

static const char *short_name(const char *q) {
    const char *s = q;
    for (const char *p = q; *p; p++)
        if (p[0] == ':' && p[1] == ':') s = p + 2;
    return s;
}

static void pad_to(AvpBuf *b, size_t line_start, size_t col) {
    size_t len = b->n - line_start;
    if (len < col) {
        for (size_t i = len; i < col; i++) avp_buf_putc(b, ' ');
    } else {
        avp_buf_putc(b, ' ');
    }
}

static void line_with_comment(AvpBuf *b, const char *text, const char *comment, size_t col) {
    size_t ls = b->n;
    avp_buf_printf(b, "    %s;", text);
    if (comment) {
        pad_to(b, ls, col);
        avp_buf_printf(b, "// %s", comment);
    }
    avp_buf_putc(b, '\n');
}

size_t cd_members_before_vptr(const CdClass *c) {
    if (!c->own_vptr || !c->layout.found || c->vptr_off < 0) return 0;
    size_t k = 0;
    while (k < c->layout.nm && (i64)c->layout.m[k].off < c->vptr_off) k++;
    return k;
}

void cd_render(AvpArena *a, const CdClass *c, AvpBuf *out) {
    const char *shortn = short_name(c->cls);
    AvpStr ns = shortn > c->cls ? avp_strn(c->cls, (size_t)(shortn - c->cls - 2)) : AVP_S("");
    const char *nsz = avp_cz(a, ns);
    size_t declared = 0, overrides = 0, inherited = 0;
    for (size_t i = 0; i < c->n; i++) {
        if (c->kind[i] == CD_KIND_INHERITED) inherited++;
        else declared++;
        if (c->kind[i] == CD_KIND_OVERRIDE) overrides++;
    }
    avp_buf_printf(out, "// %s: retail vtable %s (0x%08X), %zu slots", c->cls, c->vtable_symbol, c->vtable_address, c->n);
    if (c->sib_vtable) avp_buf_printf(out, " = Gormiti %s", c->sib_vtable);
    avp_buf_printf(out, "\n// Ownership: %s\n", c->owner_source);
    avp_buf_printf(out, "// Slots: %zu declared here (%zu override%s, %zu new), %zu inherited", declared, overrides,
                   overrides == 1 ? "" : "s", declared - overrides, inherited);
    if (c->base_cls && c->nbase) avp_buf_printf(out, " from %s (its %zu slot%s)", c->base_cls, c->nbase, c->nbase == 1 ? "" : "s");
    avp_buf_printf(out, "\n// Slot names: %zu retail symbols, %zu pairing, %zu Gormiti vtable order, %zu derived "
                        "overrides, %zu placeholders\n",
                   c->count_src[CD_SRC_RETAIL], c->count_src[CD_SRC_PAIR], c->count_src[CD_SRC_SIBVT],
                   c->count_src[CD_SRC_DERIVED], c->count_src[CD_SRC_NONE]);
    if (c->layout.found) avp_buf_printf(out, "// Layout: Gormiti DWARF (sibling.exe types %s)\n", shortn);
    else if (c->ndrafted)
        avp_buf_printf(out, "// Layout: none in the Gormiti DWARF; %zu member%s drafted from the retail field accesses"
                            " (classes.json), named field_0x<offset>\n",
                       c->ndrafted, c->ndrafted == 1 ? "" : "s");
    else avp_buf_printf(out, "// Layout: none in the Gormiti DWARF (members not drafted)\n");
    avp_buf_printf(out, "// Virtuals: retail vtable %s + Gormiti link map%s%s%s\n", c->vtable_symbol,
                   c->sib_vtable ? " (" : "", c->sib_vtable ? c->sib_vtable : "", c->sib_vtable ? ")" : "");
    if (c->slot_note) avp_buf_printf(out, "// Slot labels (in each slot comment): %s\n", classes_slot_legend());
    if (c->needs.n) {
        avp_buf_putz(out, "// needs:");
        for (size_t i = 0; i < c->needs.n; i++) avp_buf_printf(out, "%s %.*s", i ? "," : "", AVP_SF(c->needs.v[i]));
        avp_buf_putc(out, '\n');
    }
    if (ns.n) avp_buf_printf(out, "namespace %s {\n\n", nsz);
    for (size_t i = 0; i < c->fwd.n; i++) avp_buf_printf(out, "class %.*s;\n", AVP_SF(c->fwd.v[i]));
    if (c->fwd.n) avp_buf_putc(out, '\n');

    const char *key = c->layout.found && c->layout.key ? c->layout.key : "class";
    avp_buf_printf(out, "%s %s", key, shortn);
    if (c->layout.found && c->layout.nbases) {
        for (size_t i = 0; i < c->layout.nbases; i++) {
            const CdBase *bb = &c->layout.bases[i];
            avp_buf_printf(out, "%s%s%s %s", i ? ", " : " : ", bb->is_virtual ? "virtual " : "",
                           bb->access ? bb->access : "public", bb->name);
        }
    } else if (c->base_cls) {
        avp_buf_printf(out, " : public %s", short_name(c->base_cls));
    }
    avp_buf_putz(out, " {\npublic:\n");
    for (size_t i = 0; i < c->notes.n; i++) avp_buf_printf(out, "    // %.*s\n", AVP_SF(c->notes.v[i]));
    size_t before = cd_members_before_vptr(c);
    const size_t COL = 52;
    if (c->layout.found) {
        size_t upto = c->own_vptr ? before : c->layout.nm;
        for (size_t k = 0; k < upto; k++)
            line_with_comment(out, c->layout.m[k].decl, avp_sprintf(a, "0x%02X", c->layout.m[k].off), COL);
        if (c->own_vptr && c->vptr_off >= 0)
            avp_buf_printf(out, "    // vtable pointer at 0x%02X: the first virtual below\n", (u32)c->vptr_off);
    }
    if (declared && c->layout.found && c->layout.nm && !c->own_vptr) avp_buf_putc(out, '\n');
    const char *fallback = c->fallback_ret ? c->fallback_ret : (ns.n && strcmp(nsz, "FMOD") == 0 ? "FMOD_RESULT" : "void");
    for (size_t i = 0; i < c->n; i++) {
        if (c->kind[i] == CD_KIND_INHERITED) continue;
        const CdName *nm = &c->names[i];
        const char *ph = c->w[i].retail && !is_pure(c->w[i].retail)
                             ? c->w[i].retail
                             : avp_sprintf(a, "fn_%08X", c->vtable_address + c->w[i].offset);
        const char *decl = cd_method_decl(a, shortn, nsz, nm->name, c->sigs ? &c->sigs[i] : NULL, nm->pure, ph, fallback);
        AvpBuf cm;
        avp_buf_init(&cm, a);
        avp_buf_printf(&cm, "slot 0x%02X", c->w[i].offset);
        if (c->kind[i] == CD_KIND_OVERRIDE) avp_buf_putz(&cm, ", override");
        if (nm->src == CD_SRC_NONE && !nm->pure) avp_buf_putz(&cm, ", name and parameters unconfirmed");
        if (nm->pure && !nm->name) avp_buf_putz(&cm, ", pure: name unconfirmed");
        else if (nm->src == CD_SRC_PAIR) avp_buf_putz(&cm, ", name from the pairing");
        else if (nm->src == CD_SRC_SIBVT) avp_buf_putz(&cm, ", name from the Gormiti vtable");
        else if (nm->src == CD_SRC_DERIVED) avp_buf_putz(&cm, ", name from a derived override");
        if (nm->other) avp_buf_printf(&cm, "; the other source says %s", nm->other);
        if (c->sigs && c->sigs[i].weak) avp_buf_putz(&cm, "; weak in Gormiti: defined inline in the class");
        if (c->slot_note && c->slot_note[i]) avp_buf_printf(&cm, "; %s", c->slot_note[i]);
        line_with_comment(out, decl, avp_cz(a, avp_buf_str(&cm)), COL);
    }
    if (c->layout.found && c->own_vptr && before < c->layout.nm) {
        avp_buf_putc(out, '\n');
        for (size_t k = before; k < c->layout.nm; k++)
            line_with_comment(out, c->layout.m[k].decl, avp_sprintf(a, "0x%02X", c->layout.m[k].off), COL);
    }
    if (c->ndrafted) {
        avp_buf_putc(out, '\n');
        for (size_t k = 0; k < c->ndrafted; k++) line_with_comment(out, c->drafted[k].decl, c->drafted[k].note, COL);
    }
    avp_buf_putz(out, "};");
    if (c->layout.found) {
        avp_buf_printf(out, " // size 0x%X (Gormiti DWARF", c->layout.size);
        if (c->layout.nlayouts > 1) avp_buf_printf(out, ", %zu definitions of the name", c->layout.nlayouts);
        avp_buf_putc(out, ')');
        if (c->retail_size) avp_buf_printf(out, "; retail %s", c->retail_size);
    } else if (c->retail_size) {
        avp_buf_printf(out, " // retail size %s", c->retail_size);
    }
    avp_buf_putc(out, '\n');
    if (ns.n) avp_buf_printf(out, "\n} // namespace %s\n", nsz);
}

size_t cd_split_point(AvpArena *a, const CdWord *w, size_t n, const CdSibVt *c, size_t nc) {
    for (size_t k = 1; k + 2 < n; k++) {
        if (w[k].retail || w[k + 1].retail) continue;
        bool rest = false;
        for (size_t i = k + 2; i < n && !rest; i++) rest = w[i].retail != NULL;
        if (!rest) return 0; /* only nulls follow: padding, not a table */
        CdVtMatch m = cd_match_vtable(a, w, k, c, nc, NULL);
        i64 idx = m.index;
        for (size_t t = 0; idx < 0 && t < m.ntied; t++)
            for (size_t j = 0; j < nc && idx < 0; j++)
                if (strcmp(c[j].symbol, m.tied[t]) == 0) idx = (i64)j;
        if (idx >= 0 && c[idx].n == k) return k;
    }
    return 0;
}

AvpStrVec cd_short_matches(AvpArena *a, const char *q, const char *const *names, size_t n) {
    AvpStrVec out = {0};
    AvpMap *seen = avp_map_new();
    for (size_t i = 0; i < n; i++) {
        const char *c = names[i];
        if (!c || strcmp(c, q) == 0) continue;
        const char *s = c;
        for (const char *p = c; *p; p++)
            if (p[0] == ':' && p[1] == ':') s = p + 2;
        if (s == c || strcmp(s, q) != 0) continue;
        bool created;
        avp_map_put(seen, avp_str(c), &created);
        if (created) avp_vec_push(&out, avp_str(avp_strdup(a, c)));
    }
    avp_map_free(seen);
    return out;
}

static AvpStr squeeze(AvpArena *a, AvpStr s) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < s.n; i++) {
        if (s.n - i >= 6 && memcmp(s.p + i, "const ", 6) == 0) {
            i += 5;
            continue;
        }
        if (s.p[i] != ' ') avp_buf_putc(&b, s.p[i]);
    }
    return avp_buf_str(&b);
}

void cd_scan_callbacks(AvpArena *a, AvpMap *callbacks, const char *text) {
    AvpStr t = avp_str(text);
    for (i64 at = 0; (at = avp_str_find(t, AVP_S("(*)("), at, (i64)t.n)) >= 0;) {
        size_t s = (size_t)at + 4, e = s;
        int depth = 1;
        while (e < t.n && depth) depth += t.p[e] == '(' ? 1 : t.p[e] == ')' ? -1 : 0, e++;
        if (depth) break;
        AvpStr list = avp_strn(t.p + s, e - 1 - s);
        bool created; /* every spelling counts: two retail spellings of one shape decide nothing (NULL) */
        AvpVal *v = avp_map_put(callbacks, squeeze(a, list), &created);
        if (created) v->p = (void *)avp_cz(a, list);
        else if (v->p && !avp_str_eq(avp_str(v->p), list)) v->p = NULL;
        at = (i64)e;
    }
}

const char *cd_callback_consts(AvpArena *a, AvpMap *callbacks, const char *decl) {
    if (!decl || !callbacks || !strstr(decl, "(*")) return NULL;
    AvpStr t = avp_str(decl);
    i64 at = avp_str_find(t, AVP_S(")("), 0, (i64)t.n);
    if (at < 0) return NULL;
    size_t s = (size_t)at + 2, e = s;
    int depth = 1;
    while (e < t.n && depth) depth += t.p[e] == '(' ? 1 : t.p[e] == ')' ? -1 : 0, e++;
    if (depth) return NULL;
    AvpStr list = avp_strn(t.p + s, e - 1 - s);
    AvpVal *v = avp_map_get(callbacks, squeeze(a, list));
    if (!v || !v->p) return NULL;
    /* the retail spelling, in the DWARF printer's style (a space before each '*') */
    AvpBuf b;
    avp_buf_init(&b, a);
    for (const char *p = v->p; *p; p++) {
        if (*p == '*' && p > (const char *)v->p && p[-1] != ' ' && p[-1] != '*') avp_buf_putc(&b, ' ');
        avp_buf_putc(&b, *p);
    }
    AvpStr fixed = avp_buf_str(&b);
    if (avp_str_eq(fixed, list)) return NULL;
    return avp_sprintf(a, "%.*s%.*s%.*s", (int)s, t.p, AVP_SF(fixed), (int)(t.n - (e - 1)), t.p + e - 1);
}
