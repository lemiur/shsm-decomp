/* classes_read.c: the readers of classes.py (cmd_show, chain, render_header, _null_decl, _decl, _spaced, cmd_tree,
 * _ctype). Output goes into a buffer; where the Python raises after printing part of its output, the reader stops
 * at the same point and returns the exception's text. */
#include "classes.h"

#include <string.h>

static bool truthy(const char *s) { return s && *s; }
static bool eqz(const char *x, const char *y) { return x && y && strcmp(x, y) == 0; }

static const char *key_error(AvpArena *a, const char *key) {
    return avp_cz(a, avp_pyfmt(a, "KeyError: %r", key ? key : "None"));
}

/* "-0x%X" % -v if v < 0 else "0x%X" % v */
static const char *hx(AvpArena *a, i64 v) {
    if (v < 0) return avp_cz(a, avp_pyfmt(a, "-0x%X", -v));
    return avp_cz(a, avp_pyfmt(a, "0x%X", v));
}

static void join_strs(AvpBuf *b, const char *sep, ClassesStrs s) {
    for (size_t i = 0; i < s.n; i++) {
        if (i) avp_buf_putz(b, sep);
        avp_buf_putz(b, s.v[i] ? s.v[i] : "None");
    }
}

/* ---- show ------------------------------------------------------------------------------------------------------ */
static void chain(AvpArena *a, const ClassesDb *db, const ClassesClass *c, int depth, AvpBuf *out, size_t *count) {
    for (size_t i = 0; i < c->n_bases; i++) {
        const ClassesBase *b = &c->bases[i];
        for (int k = 0; k < depth; k++) avp_buf_putz(out, "  ");
        avp_buf_pyfmt(out, "%s at %s%s  [", b->cls, truthy(b->offset) ? b->offset : "?",
                      b->primary ? " (primary)" : "");
        join_strs(out, "; ", b->evidence);
        avp_buf_putz(out, "]\n");
        (*count)++;
        const ClassesClass *x = classes_by_key(db, b->key);
        if (x && depth < 16) chain(a, db, x, depth + 1, out, count);
    }
}

static void fn_list(AvpBuf *out, const char *k, const ClassesFn *f, size_t n) {
    if (!n) return;
    avp_buf_pyfmt(out, "  %s: ", k);
    for (size_t i = 0; i < n; i++) avp_buf_pyfmt(out, "%s%s %s", i ? ", " : "", f[i].address, f[i].name);
    avp_buf_putz(out, "\n");
}

const char *classes_show(AvpArena *a, const ClassesDb *db, const ClassesClass *c, AvpBuf *out) {
    avp_buf_pyfmt(out, "%s   (name: %s)\n", c->name, c->name_source);
    bool first = true;
    for (size_t i = 0; i < c->n_name_candidates; i++) {
        const ClassesNameCand *x = &c->name_candidates[i];
        if (eqz(x->name, c->name)) continue;
        avp_buf_putz(out, first ? "  other name candidates: " : ", ");
        avp_buf_pyfmt(out, "%s (%s)", x->name, x->source);
        first = false;
    }
    if (!first) avp_buf_putz(out, "\n");
    avp_buf_pyfmt(out, "  unit: %s\n", c->unit);
    if (truthy(c->size))
        avp_buf_pyfmt(out, "  size: %s (%s)\n", c->size, c->size_source);
    else
        avp_buf_pyfmt(out, "  size: ~0x%X (estimate from accesses)\n", c->has_size_estimate ? c->size_estimate : 0);
    fn_list(out, "ctors", c->ctors, c->n_ctors);
    fn_list(out, "dtors", c->dtors, c->n_dtors);
    fn_list(out, "factories", c->factories, c->n_factories);
    fn_list(out, "other_installers", c->other_installers, c->n_other_installers);
    for (size_t i = 0; i < c->n_registry; i++) {
        const ClassesRegistry *r = &c->registry[i];
        avp_buf_pyfmt(out, "  registry: \"%s\" in %s, factory %s, size %s\n", r->string, r->sinit, r->factory, r->size);
    }
    avp_buf_putz(out, "bases:\n");
    size_t lines = 0;
    chain(a, db, c, 1, out, &lines);
    if (!lines) avp_buf_putz(out, "  (none found)\n");
    if (c->derived.n) {
        AvpBuf line;
        avp_buf_init(&line, a);
        for (size_t i = 0; i < c->derived.n; i++) {
            const ClassesClass *d = classes_by_key(db, c->derived.v[i]);
            if (!d) return key_error(a, c->derived.v[i]);
            avp_buf_pyfmt(&line, "%s%s", i ? ", " : "", d->name);
        }
        AvpStr ls = avp_buf_str(&line);
        avp_buf_pyfmt(out, "derived: %S\n", &ls);
    }
    const ClassesVtable *vt = c->vtable;
    if (vt) {
        avp_buf_pyfmt(out, "vtable %s at %s, size %s; groups (%s): ", vt->symbol, vt->address, vt->size,
                      vt->group_evidence);
        for (size_t i = 0; i < vt->n_groups; i++)
            avp_buf_pyfmt(out, "%s+%s -> this+%s", i ? ", " : "", vt->groups[i].addend,
                          truthy(vt->groups[i].this_offset) ? vt->groups[i].this_offset : "?");
        avp_buf_putz(out, "\n");
        for (size_t i = 0; i < vt->group_conflicts.n; i++)
            avp_buf_pyfmt(out, "  group conflict: %s\n", vt->group_conflicts.v[i]);
        avp_buf_pyfmt(out, "  slot labels: %s\n", classes_slot_legend());
        for (size_t i = 0; i < vt->n_words; i++) {
            const ClassesWord *w = &vt->words[i];
            if (eqz(w->status, "header")) {
                avp_buf_pyfmt(out, "  %5s  ---- header", w->offset);
                if (truthy(w->rtti)) avp_buf_pyfmt(out, "  RTTI %s", w->rtti);
                avp_buf_putz(out, "\n");
                continue;
            }
            if (!avp_json_getz(w->json, "target")) return key_error(a, "target");
            AvpBuf t;
            avp_buf_init(&t, a);
            avp_buf_putz(&t, truthy(w->target) ? w->target : "0 (null)");
            if (w->has_thunk) {
                if (!w->thunk_target && !avp_json_getz(avp_json_getz(w->json, "thunk"), "target"))
                    return key_error(a, "target");
                avp_buf_pyfmt(&t, "  [thunk -%s -> %s]", hx(a, w->thunk_adjust), w->thunk_target);
            }
            AvpBuf extra;
            avp_buf_init(&extra, a);
            if (eqz(w->status, "inherited") || eqz(w->status, "override")) {
                if (!avp_json_getz(w->json, "base")) return key_error(a, "base");
                avp_buf_pyfmt(&extra, "  <- %s", w->base);
                if (eqz(w->status, "override")) {
                    if (!w->has_base_target) return key_error(a, "base_target");
                    avp_buf_pyfmt(&extra, " (was %s)", truthy(w->base_target) ? w->base_target : "null");
                }
            }
            ClassesSlotLabel lab;
            if (classes_slot_label(a, db, c, w, &lab))
                avp_buf_pyfmt(&extra, "  [%s]", classes_slot_label_text(a, w, &lab));
            if (!avp_json_getz(w->json, "group")) return key_error(a, "group");
            AvpStr ts = avp_buf_str(&t), es = avp_buf_str(&extra);
            avp_buf_pyfmt(out, "  %5s  %-10s %-9s %S%S\n", w->offset, w->status, w->group, &ts, &es);
        }
    }
    for (size_t i = 0; i < c->n_member_objects; i++) {
        const ClassesMemberObject *m = &c->member_objects[i];
        avp_buf_pyfmt(out, "member object at %s: %s  [", m->offset, m->cls);
        join_strs(out, "; ", m->evidence);
        avp_buf_putz(out, "]\n");
    }
    avp_buf_putz(out, "fields:\n");
    if (c->own_vptrs.n) {
        avp_buf_putz(out, "  own vptrs at ");
        join_strs(out, ", ", c->own_vptrs);
        avp_buf_putz(out, "\n");
    }
    for (size_t i = 0; i < c->n_fields; i++) {
        const ClassesField *f = &c->fields[i];
        avp_buf_pyfmt(out, "  %6s  %-7s %d-byte %-7s loads %d stores %d in %d fn", f->offset, f->type, f->width,
                      f->kind, f->loads, f->stores, f->functions);
        if (truthy(f->inherited_from)) avp_buf_pyfmt(out, "  (in %s)", f->inherited_from);
        if (f->conflict.n) {
            avp_buf_putz(out, "  CONFLICT: ");
            join_strs(out, "; ", f->conflict);
        }
        avp_buf_putz(out, "\n");
    }
    return NULL;
}

const char *classes_show_json(AvpArena *a, const ClassesClass *c, AvpBuf *out) {
    AvpJsonDumpOpts o = AVP_JSON_INDENT(1);
    avp_json_dump_buf(out, c->json, &o);
    avp_buf_putz(out, "\n");
    (void)a;
    return NULL;
}

/* ---- header ---------------------------------------------------------------------------------------------------- */
/* _spaced: `fn(A&)` -> `fn( A& )` */
static AvpStr spaced(AvpArena *a, AvpStr sig) {
    i64 i = avp_str_findc(sig, '(');
    i64 j = avp_str_rfind(sig, AVP_S(")"), 0, (i64)sig.n);
    if (i < 0 || j < 0) return sig;
    AvpStr inner = avp_str_slice(sig, i + 1, j);
    AvpStr head = avp_str_slice(sig, 0, i), tail = avp_str_slice(sig, j + 1, (i64)sig.n);
    if (!inner.n || avp_str_eqz(inner, "void")) return avp_pyfmt(a, "%S()%S", &head, &tail);
    return avp_pyfmt(a, "%S( %S )%S", &head, &inner, &tail);
}

/* re.sub(r"^@\d+@", "", s) */
static AvpStr strip_thunk_prefix(AvpStr s) {
    if (!s.n || s.p[0] != '@') return s;
    size_t i = 1;
    while (i < s.n && s.p[i] >= '0' && s.p[i] <= '9') i++;
    if (i == 1 || i >= s.n || s.p[i] != '@') return s;
    return avp_str_slice(s, (i64)i + 1, (i64)s.n);
}

/* s.split("__")[0] */
static AvpStr before_dunder(AvpStr s) {
    i64 k = avp_str_find(s, AVP_S("__"), 0, (i64)s.n);
    return k < 0 ? s : avp_str_slice(s, 0, k);
}

/* x.get("thunk", {}).get("target", x["target"]) */
static const char *slot_target(const ClassesWord *w) { return w->has_thunk && w->thunk_target ? w->thunk_target : w->target; }

static const char *null_decl(AvpArena *a, const ClassesDb *db, const ClassesClass *c, const ClassesWord *w) {
    for (size_t i = 0; i < db->n; i++) {
        const ClassesVtable *vt = db->classes[i].vtable;
        if (!vt) continue;
        for (size_t k = 0; k < vt->n_words; k++) {
            const ClassesWord *x = &vt->words[k];
            if (!(eqz(x->base, c->name) && x->has_base_index && x->base_index == w->index && eqz(x->status, "override")))
                continue;
            const char *t = slot_target(x);
            const char *meth = classes_method(db, t);
            if (meth && *meth && meth[0] != '~') {
                AvpStr sp = spaced(a, avp_str(meth));
                return avp_cz(a, avp_pyfmt(a, "    virtual void    %S = 0;   // null slot; signature from %s", &sp, t));
            }
            if (truthy(t) && strncmp(t, "__", 2) != 0) {
                AvpStr nm = before_dunder(strip_thunk_prefix(avp_str(t)));
                return avp_cz(a, avp_pyfmt(a, "    virtual void    %S() = 0;   // null slot; name from %s, signature unknown",
                                           &nm, t));
            }
        }
    }
    AvpStr off = avp_u8_slice(avp_str(w->offset), 2, (i64)strlen(w->offset));
    return avp_cz(a, avp_pyfmt(a, "    virtual void    vslot_%S() = 0;   // null slot (pure virtual?); no override seen", &off));
}

/* _decl(meth, fname, cname, override); NULL fname with no meth is the Python's TypeError in re.sub */
static const char *decl(AvpArena *a, const char *meth, const char *fname, const char *cname, bool override,
                        const char **crash) {
    bool unknown = false;
    AvpStr m;
    if (!meth) {
        if (!fname) {
            *crash = "TypeError: expected string or bytes-like object, got 'NoneType'";
            return NULL;
        }
        AvpStr base = strip_thunk_prefix(avp_str(fname));
        if (avp_str_startswith(base, AVP_S("__dt__"))) {
            m = AVP_S("~X()");
        } else {
            AvpStr nm = avp_str_startswith(base, AVP_S("__")) ? base : before_dunder(base);
            m = avp_pyfmt(a, "%S()", &nm);
            unknown = true;
        }
    } else {
        m = avp_str(meth);
    }
    AvpStr txt;
    if (m.n && m.p[0] == '~') {
        txt = avp_pyfmt(a, "    virtual         ~%s();", cname);
    } else {
        AvpStr sp = spaced(a, m);
        txt = avp_pyfmt(a, "    virtual void    %S;", &sp);
    }
    if (override && unknown) return avp_cz(a, avp_pyfmt(a, "%S   // override; signature unknown", &txt));
    if (override) return avp_cz(a, avp_pyfmt(a, "%S   // override", &txt));
    if (unknown) return avp_cz(a, avp_pyfmt(a, "%S   // signature unknown", &txt));
    return avp_cz(a, txt);
}

typedef struct {
    i64 off;
    const ClassesBase *b;
} BaseOrd;
static int cmp_base(const void *x, const void *y, void *ctx) {
    (void)ctx;
    i64 p = ((const BaseOrd *)x)->off, q = ((const BaseOrd *)y)->off;
    return p < q ? -1 : p > q;
}

static i64 hex_or_zero(const char *s) {
    i64 v = 0;
    if (!classes_hex(s, &v)) return 0;
    return v;
}

static bool in_list(const char *const *v, size_t n, const char *s) {
    for (size_t i = 0; i < n; i++)
        if ((v[i] == NULL && s == NULL) || eqz(v[i], s)) return true;
    return false;
}

const char *classes_header(AvpArena *a, const ClassesDb *db, const ClassesClass *c, AvpBuf *out) {
    AvpBuf b;
    avp_buf_init(&b, a);
    const char *name = c->name;
    avp_buf_putz(&b, "/*\n");
    AvpBuf desc;
    avp_buf_init(&desc, a);
    int nd = 0;
#define DESC_SEP() (nd++ ? avp_buf_putz(&desc, ", ") : (void)0)
    if (c->vtable) {
        DESC_SEP();
        avp_buf_pyfmt(&desc, "vtable %s", c->vtable->symbol);
    }
    if (c->n_ctors) {
        DESC_SEP();
        avp_buf_putz(&desc, "constructor ");
        for (size_t i = 0; i < c->n_ctors; i++) avp_buf_pyfmt(&desc, "%s%s", i ? ", " : "", c->ctors[i].address);
    }
    if (c->n_dtors) {
        DESC_SEP();
        avp_buf_putz(&desc, "destructor ");
        for (size_t i = 0; i < c->n_dtors; i++) avp_buf_pyfmt(&desc, "%s%s", i ? ", " : "", c->dtors[i].address);
    }
    if (truthy(c->size)) {
        DESC_SEP();
        avp_buf_pyfmt(&desc, "size %s", c->size);
    }
#undef DESC_SEP
    avp_buf_pyfmt(&b, " * %s (%s).\n", name, nd ? avp_buf_cz(&desc) : "no vtable");
    if (strncmp(c->name_source, "placeholder", 11) == 0)
        avp_buf_pyfmt(&b, " * Name invented (%s).\n", c->name_source);
    else
        avp_buf_pyfmt(&b, " * Name from: %s.\n", c->name_source);
    avp_buf_putz(&b, " * Skeleton from classes.exe: return types are placeholders.\n");
    if (c->vtable) avp_buf_pyfmt(&b, " * Slot labels: %s.\n", classes_slot_legend());
    avp_buf_putz(&b, " */\n");
    BaseOrd *bases = AVP_NEWN(a, BaseOrd, c->n_bases ? c->n_bases : 1);
    for (size_t i = 0; i < c->n_bases; i++) {
        bases[i].b = &c->bases[i];
        bases[i].off = truthy(c->bases[i].offset) ? hex_or_zero(c->bases[i].offset) : 0;
    }
    avp_sort(bases, c->n_bases, sizeof *bases, cmp_base, NULL, false);
    avp_buf_pyfmt(&b, "class %s", name);
    for (size_t i = 0; i < c->n_bases; i++) avp_buf_pyfmt(&b, "%spublic %s", i ? ", " : " : ", bases[i].b->cls);
    avp_buf_putz(&b, " {\npublic:\n");
    size_t ndecls = 0;
    const ClassesVtable *vt = c->vtable;
    if (vt) {
        const ClassesWord **order = AVP_NEWN(a, const ClassesWord *, vt->n_words ? vt->n_words : 1);
        size_t no = 0;
        for (size_t i = 0; i < vt->n_words; i++)
            if (eqz(vt->words[i].group, "complete")) order[no++] = &vt->words[i];
        for (size_t i = 0; i < vt->n_words; i++)
            if (eqz(vt->words[i].group, "embedded")) order[no++] = &vt->words[i];
        const char **overridden = AVP_NEWN(a, const char *, vt->n_words ? vt->n_words : 1);
        size_t nov = 0;
        for (size_t i = 0; i < vt->n_words; i++) {
            const ClassesWord *w = &vt->words[i];
            if (eqz(w->status, "override") && truthy(w->target)) overridden[nov++] = slot_target(w);
        }
        const char **seen = AVP_NEWN(a, const char *, vt->n_words ? vt->n_words : 1);
        size_t nseen = 0;
        const char **dtor_names = AVP_NEWN(a, const char *, c->n_dtors ? c->n_dtors : 1);
        for (size_t i = 0; i < c->n_dtors; i++) dtor_names[i] = c->dtors[i].name;
        for (size_t i = 0; i < no; i++) {
            const ClassesWord *w = order[i];
            const char *st = w->status;
            if (eqz(st, "null") && eqz(w->group, "complete")) {
                avp_buf_pyfmt(&b, "%s\n", null_decl(a, db, c, w));
                ndecls++;
                continue;
            }
            if (!eqz(st, "new") && !eqz(st, "override")) continue;
            if (!eqz(w->group, "complete") && !eqz(st, "override")) continue;
            const char *t = slot_target(w);
            if (in_list(seen, nseen, t)) continue;
            seen[nseen++] = t;
            const char *meth = classes_method(db, t);
            if (t && in_list(dtor_names, c->n_dtors, t) && (meth == NULL || meth[0] != '~')) meth = "~X()";
            const char *crash = NULL;
            const char *d = decl(a, meth, t, name, t && in_list(overridden, nov, t), &crash);
            if (crash) return crash; /* render_header raised before anything was printed */
            ClassesSlotLabel lab;
            if ((eqz(st, "override") || eqz(st, "new")) && classes_slot_label(a, db, c, w, &lab) && lab.introducer &&
                !eqz(lab.introducer, name)) {
                const char *lt = classes_slot_label_text(a, w, &lab);
                d = strstr(d, "//") ? avp_sprintf(a, "%s; slot %s %s", d, w->offset, lt)
                                    : avp_sprintf(a, "%s   // slot %s %s", d, w->offset, lt);
            }
            avp_buf_pyfmt(&b, "%s\n", d);
            ndecls++;
        }
        bool any = false;
        for (size_t i = 0; i < vt->n_words; i++) {
            if (!eqz(vt->words[i].status, "unresolved")) continue;
            avp_buf_putz(&b, any ? ", " : "    // embedded group with no known base: slots ");
            avp_buf_putz(&b, vt->words[i].offset);
            any = true;
        }
        if (any) {
            avp_buf_putz(&b, "\n");
            ndecls++;
        }
    }
    i64 start = truthy(c->base_end) ? hex_or_zero(c->base_end) : 0;
    i64 end = truthy(c->size) ? hex_or_zero(c->size) : (c->has_size_estimate ? c->size_estimate : 0);
    size_t nown = 0;
    for (size_t i = 0; i < c->n_fields; i++)
        if (!truthy(c->fields[i].inherited_from)) nown++;
    if (nown || end > start) {
        if (ndecls) avp_buf_putz(&b, "\n");
        i64 pos = start;
        for (size_t i = 0; i < c->n_fields; i++) {
            const ClassesField *f = &c->fields[i];
            if (truthy(f->inherited_from)) continue;
            i64 o = hex_or_zero(f->offset);
            if (o < pos) {
                avp_buf_pyfmt(&b, "    // 0x%X: %s access overlapping the field before\n", o, f->type);
                continue;
            }
            if (o > pos) avp_buf_pyfmt(&b, "    u8      field_0x%X[0x%X];  // 0x%X, unknown\n", pos, o - pos, pos);
            const ClassesMemberObject *mo = NULL; /* {offset: m} keeps the last object at an offset */
            for (size_t k = 0; k < c->n_member_objects; k++)
                if (hex_or_zero(c->member_objects[k].offset) == o) mo = &c->member_objects[k];
            if (mo) {
                avp_buf_pyfmt(&b, "    // 0x%X: %s member object (", o, mo->cls);
                join_strs(&b, "; ", mo->evidence);
                avp_buf_putz(&b, ")\n");
            }
            bool vp = false;
            for (size_t k = 0; k < c->own_vptrs.n; k++)
                if (hex_or_zero(c->own_vptrs.v[k]) == o) vp = true;
            if (vp) {
                avp_buf_pyfmt(&b, "    // 0x%X: vptr\n", o);
            } else {
                const char *typ = f->type, *arr = "";
                if (eqz(typ, "f32[2]")) {
                    typ = "f32";
                    arr = "[2]";
                }
                avp_buf_pyfmt(&b, "    %-7s field_0x%X%s;  // 0x%X", typ, o, arr, o);
                if (f->conflict.n) {
                    avp_buf_putz(&b, "  CONFLICT: ");
                    join_strs(&b, "; ", f->conflict);
                }
                avp_buf_putz(&b, "\n");
            }
            pos = o + f->width;
        }
        if (end > pos)
            avp_buf_pyfmt(&b, "    u8      field_0x%X[0x%X];  // 0x%X, unknown%s\n", pos, end - pos, pos,
                          truthy(c->size) ? "" : " (size estimated)");
    }
    avp_buf_putz(&b, "};\n");
    avp_buf_put(out, avp_buf_str(&b));
    return NULL;
}

/* ---- tree ------------------------------------------------------------------------------------------------------ */
typedef struct {
    AvpArena *a;
    const ClassesDb *db;
    AvpBuf *out;
    AvpSet *done;
    const char *crash;
} Tree;

static int cmp_name(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return strcmp((*(const ClassesClass *const *)x)->name, (*(const ClassesClass *const *)y)->name);
}

/* the derived classes of c as classes (idx[k]); false (crash set) on a key no class has */
static bool derived_of(Tree *t, const ClassesClass *c, const ClassesClass ***out) {
    const ClassesClass **v = AVP_NEWN(t->a, const ClassesClass *, c->derived.n ? c->derived.n : 1);
    for (size_t i = 0; i < c->derived.n; i++) {
        v[i] = classes_by_key(t->db, c->derived.v[i]);
        if (!v[i]) {
            t->crash = key_error(t->a, c->derived.v[i]);
            return false;
        }
    }
    *out = v;
    return true;
}

static AvpJson *node(Tree *t, const ClassesClass *c, int d) {
    AvpJson *o = avp_json_obj(t->a);
    avp_json_set(t->a, o, AVP_S("name"), avp_json_strz(t->a, c->name));
    avp_json_set(t->a, o, AVP_S("key"), avp_json_strz(t->a, c->key));
    AvpJson *ch = avp_json_arr(t->a);
    if (d < 32) {
        const ClassesClass **der;
        if (!derived_of(t, c, &der)) return NULL;
        for (size_t i = 0; i < c->derived.n; i++) {
            AvpJson *n = node(t, der[i], d + 1);
            if (!n) return NULL;
            avp_json_arr_push(t->a, ch, n);
        }
    }
    avp_json_set(t->a, o, AVP_S("children"), ch);
    return o;
}

static bool walk(Tree *t, const ClassesClass *c, int d) {
    AvpBuf *o = t->out;
    for (int i = 0; i < d; i++) avp_buf_putz(o, "  ");
    avp_buf_pyfmt(o, "%s  [%s]", c->name, c->vtable ? c->vtable->address : "no vtable");
    size_t nmore = 0, next = 0;
    for (size_t i = 0; i < c->n_bases; i++) {
        if (classes_by_key(t->db, c->bases[i].key)) nmore++;
        else next++;
    }
    if (nmore > 1) {
        avp_buf_putz(o, "  bases: ");
        size_t k = 0;
        for (size_t i = 0; i < c->n_bases; i++) {
            const ClassesBase *b = &c->bases[i];
            if (!classes_by_key(t->db, b->key)) continue;
            avp_buf_pyfmt(o, "%s%s@%s", k++ ? ", " : "", b->cls, truthy(b->offset) ? b->offset : "?");
        }
    }
    if (next) {
        avp_buf_putz(o, "  bases without a vtable: ");
        size_t k = 0;
        for (size_t i = 0; i < c->n_bases; i++) {
            const ClassesBase *b = &c->bases[i];
            if (classes_by_key(t->db, b->key)) continue;
            avp_buf_pyfmt(o, "%s%s@%s", k++ ? ", " : "", b->cls, truthy(b->offset) ? b->offset : "?");
        }
    }
    avp_buf_putz(o, "\n");
    if (avp_set_has(t->done, avp_str(c->key)) || d > 32) return true;
    avp_set_add(t->done, avp_str(c->key));
    const ClassesClass **der;
    if (!derived_of(t, c, &der)) return false;
    avp_sort((void *)der, c->derived.n, sizeof *der, cmp_name, NULL, false);
    for (size_t i = 0; i < c->derived.n; i++)
        if (!walk(t, der[i], d + 1)) return false;
    return true;
}

const char *classes_tree(AvpArena *a, const ClassesDb *db, const ClassesClass *root, bool all, bool json, AvpBuf *out) {
    Tree t = {.a = a, .db = db, .out = out};
    const ClassesClass **roots = AVP_NEWN(a, const ClassesClass *, db->n ? db->n : 1);
    size_t nr = 0;
    if (root) {
        roots[nr++] = root;
    } else {
        for (size_t i = 0; i < db->n; i++) {
            const ClassesClass *c = &db->classes[i];
            bool has_known_base = false;
            for (size_t k = 0; k < c->n_bases; k++)
                if (classes_by_key(db, c->bases[k].key)) has_known_base = true;
            if (has_known_base) continue;
            if (!all && !c->derived.n && !c->n_bases) continue;
            roots[nr++] = c;
        }
    }
    if (json) {
        AvpJson *arr = avp_json_arr(a);
        for (size_t i = 0; i < nr; i++) {
            AvpJson *n = node(&t, roots[i], 0);
            if (!n) return t.crash;
            avp_json_arr_push(a, arr, n);
        }
        AvpJsonDumpOpts o = AVP_JSON_INDENT(1);
        avp_json_dump_buf(out, arr, &o);
        avp_buf_putz(out, "\n");
        return NULL;
    }
    t.done = avp_set_new();
    avp_sort((void *)roots, nr, sizeof *roots, cmp_name, NULL, false);
    for (size_t i = 0; i < nr; i++) {
        if (!walk(&t, roots[i], 0)) {
            avp_set_free(t.done);
            return t.crash;
        }
    }
    avp_set_free(t.done);
    if (!root && !all) {
        size_t lone = 0;
        for (size_t i = 0; i < db->n; i++)
            if (!db->classes[i].n_bases && !db->classes[i].derived.n) lone++;
        avp_buf_pyfmt(out, "(%d classes with no base and no derived class not shown; --all lists them)\n", lone);
    }
    return NULL;
}

/* ---- _ctype ---------------------------------------------------------------------------------------------------- */
const char *classes_ctype(AvpArena *a, i64 width, const char *kind, bool is_signed, bool pointer) {
    if (eqz(kind, "float")) return "f32";
    if (eqz(kind, "double")) return "f64";
    if (eqz(kind, "paired")) return width == 8 ? "f32[2]" : "f32";
    if (width == 1) return is_signed ? "s8" : "u8";
    if (width == 2) return is_signed ? "s16" : "u16";
    if (width == 4) return pointer ? "void*" : "u32";
    return avp_cz(a, avp_pyfmt(a, "u8[%d]", width));
}
