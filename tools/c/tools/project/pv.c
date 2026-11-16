/* Python values for the build plumbing: pv.h. Windows pathlib semantics as CPython 3.14's PureWindowsPath
 * (pathlib._parse_path, ntpath.splitroot / join / isabs, PurePath.parent / name / suffix / stem / with_suffix). */
#include "pv.h"

#include <string.h>

#include "avp/fmt.h"

/* ---- construction ---- */

static Pv g_none = {PV_NONE, {0}};
static Pv g_true = {PV_BOOL, {.b = true}};
static Pv g_false = {PV_BOOL, {.b = false}};

Pv *pv_none(void) { return &g_none; }
Pv *pv_bool(bool b) { return b ? &g_true : &g_false; }

static Pv *pv_new(AvpArena *a, PvKind kind) {
    Pv *v = AVP_NEW(a, Pv);
    v->kind = kind;
    return v;
}

Pv *pv_int(AvpArena *a, int64_t i) {
    Pv *v = pv_new(a, PV_INT);
    v->u.i = i;
    return v;
}

Pv *pv_str(AvpArena *a, AvpStr s) {
    Pv *v = pv_new(a, PV_STR);
    v->u.s = avp_str_dup(a, s);
    return v;
}

Pv *pv_strz(AvpArena *a, const char *s) { return pv_str(a, avp_str(s)); }

Pv *pv_path(AvpArena *a, AvpStr s) {
    Pv *v = pv_new(a, PV_PATH);
    v->u.s = pv_path_norm(a, s);
    return v;
}

Pv *pv_list(AvpArena *a) {
    Pv *v = pv_new(a, PV_LIST);
    v->u.list = AVP_NEW(a, PvList);
    return v;
}

Pv *pv_tuple(AvpArena *a) {
    Pv *v = pv_new(a, PV_TUPLE);
    v->u.list = AVP_NEW(a, PvList);
    return v;
}

Pv *pv_dict(AvpArena *a) {
    Pv *v = pv_new(a, PV_DICT);
    v->u.dict = AVP_NEW(a, PvDict);
    return v;
}

Pv *pv_instance(AvpArena *a, const char *cls) {
    Pv *v = pv_new(a, PV_INSTANCE);
    v->u.inst = AVP_NEW(a, PvInstance);
    v->u.inst->cls = avp_strdup(a, cls);
    v->u.inst->attrs = AVP_NEW(a, PvDict);
    return v;
}

Pv *pv_opaque(AvpArena *a, const char *tag, void *p) {
    Pv *v = pv_new(a, PV_OPAQUE);
    v->u.opaque.tag = tag;
    v->u.opaque.p = p;
    return v;
}

/* ---- lists, tuples, dicts, attributes ---- */

void pv_list_push(AvpArena *a, Pv *list, Pv *item) {
    AVP_ASSERT(list->kind == PV_LIST || list->kind == PV_TUPLE);
    PvList *l = list->u.list;
    if (l->n == l->cap) {
        size_t cap = l->cap ? l->cap * 2 : 8;
        l->v = avp_arena_realloc(a, l->v, l->cap * sizeof(Pv *), cap * sizeof(Pv *));
        l->cap = cap;
    }
    l->v[l->n++] = item;
}

void pv_list_extend(AvpArena *a, Pv *list, const Pv *items) {
    AVP_ASSERT(items->kind == PV_LIST || items->kind == PV_TUPLE);
    size_t n = items->u.list->n; /* l.extend(l) appends the items present before the call */
    for (size_t i = 0; i < n; i++) pv_list_push(a, list, items->u.list->v[i]);
}

Pv *pv_list_copy(AvpArena *a, const Pv *list) {
    Pv *r = pv_list(a);
    pv_list_extend(a, r, list);
    return r;
}

size_t pv_len(const Pv *v) {
    switch (v->kind) {
    case PV_STR: {
        size_t n = 0; /* code points */
        for (size_t i = 0; i < v->u.s.n; i++)
            if (((unsigned char)v->u.s.p[i] & 0xC0) != 0x80) n++;
        return n;
    }
    case PV_LIST:
    case PV_TUPLE: return v->u.list->n;
    case PV_DICT: return v->u.dict->n;
    default: return 0;
    }
}

static Pv **dict_slot(PvDict *d, AvpStr key) {
    for (size_t i = 0; i < d->n; i++)
        if (avp_str_eq(d->keys[i], key)) return &d->vals[i];
    return NULL;
}

static void dict_set(AvpArena *a, PvDict *d, AvpStr key, Pv *val) {
    Pv **slot = dict_slot(d, key);
    if (slot) {
        *slot = val;
        return;
    }
    if (d->n == d->cap) {
        size_t cap = d->cap ? d->cap * 2 : 8;
        d->keys = avp_arena_realloc(a, d->keys, d->cap * sizeof(AvpStr), cap * sizeof(AvpStr));
        d->vals = avp_arena_realloc(a, d->vals, d->cap * sizeof(Pv *), cap * sizeof(Pv *));
        d->cap = cap;
    }
    d->keys[d->n] = avp_str_dup(a, key);
    d->vals[d->n] = val;
    d->n++;
}

Pv *pv_dict_get(const Pv *dict, AvpStr key) {
    AVP_ASSERT(dict->kind == PV_DICT);
    Pv **slot = dict_slot(dict->u.dict, key);
    return slot ? *slot : NULL;
}

void pv_dict_set(AvpArena *a, Pv *dict, AvpStr key, Pv *val) {
    AVP_ASSERT(dict->kind == PV_DICT);
    dict_set(a, dict->u.dict, key, val);
}

Pv *pv_dict_copy(AvpArena *a, const Pv *dict) {
    AVP_ASSERT(dict->kind == PV_DICT);
    Pv *r = pv_dict(a);
    for (size_t i = 0; i < dict->u.dict->n; i++) dict_set(a, r->u.dict, dict->u.dict->keys[i], dict->u.dict->vals[i]);
    return r;
}

Pv *pv_getattr(const Pv *inst, const char *name) {
    AVP_ASSERT(inst->kind == PV_INSTANCE);
    Pv **slot = dict_slot(inst->u.inst->attrs, avp_str(name));
    return slot ? *slot : NULL;
}

void pv_setattr(AvpArena *a, Pv *inst, const char *name, Pv *val) {
    AVP_ASSERT(inst->kind == PV_INSTANCE);
    dict_set(a, inst->u.inst->attrs, avp_str(name), val);
}

bool pv_isinstance(const Pv *v, const char *cls) {
    return v->kind == PV_INSTANCE && strcmp(v->u.inst->cls, cls) == 0;
}

/* ---- Python semantics ---- */

bool pv_truthy(const Pv *v) {
    switch (v->kind) {
    case PV_NONE: return false;
    case PV_BOOL: return v->u.b;
    case PV_INT: return v->u.i != 0;
    case PV_STR: return v->u.s.n != 0;
    case PV_LIST:
    case PV_TUPLE: return v->u.list->n != 0;
    case PV_DICT: return v->u.dict->n != 0;
    default: return true; /* paths, instances, functions */
    }
}

static bool is_number(const Pv *v, int64_t *out) {
    if (v->kind == PV_INT) {
        *out = v->u.i;
        return true;
    }
    if (v->kind == PV_BOOL) {
        *out = v->u.b ? 1 : 0;
        return true;
    }
    return false;
}

bool pv_eq(const Pv *x, const Pv *y) {
    if (x == y) return true;
    int64_t i, j;
    if (is_number(x, &i) && is_number(y, &j)) return i == j;
    if (x->kind != y->kind) return false;
    switch (x->kind) {
    case PV_NONE: return true;
    case PV_STR: return avp_str_eq(x->u.s, y->u.s);
    case PV_PATH: {
        AvpArena *s = avp_scratch();
        AvpArenaMark m = avp_arena_mark(s);
        bool r = avp_str_eq(avp_str_lower(s, x->u.s), avp_str_lower(s, y->u.s));
        avp_arena_reset(s, m);
        return r;
    }
    case PV_LIST:
    case PV_TUPLE:
        if (x->u.list->n != y->u.list->n) return false;
        for (size_t k = 0; k < x->u.list->n; k++)
            if (!pv_eq(x->u.list->v[k], y->u.list->v[k])) return false;
        return true;
    case PV_DICT:
        if (x->u.dict->n != y->u.dict->n) return false;
        for (size_t k = 0; k < x->u.dict->n; k++) {
            Pv **o = dict_slot(y->u.dict, x->u.dict->keys[k]);
            if (!o || !pv_eq(x->u.dict->vals[k], *o)) return false;
        }
        return true;
    case PV_OPAQUE: return x->u.opaque.p == y->u.opaque.p && x->u.opaque.tag == y->u.opaque.tag;
    default: return false; /* instances: identity */
    }
}

static void repr_into(AvpBuf *b, AvpArena *a, const Pv *v) {
    switch (v->kind) {
    case PV_NONE: avp_buf_putz(b, "None"); break;
    case PV_BOOL: avp_buf_putz(b, v->u.b ? "True" : "False"); break;
    case PV_INT: avp_buf_printf(b, "%lld", (long long)v->u.i); break;
    case PV_STR: avp_buf_put(b, avp_repr_str(a, v->u.s)); break;
    case PV_PATH:
        avp_buf_putz(b, "WindowsPath(");
        avp_buf_put(b, avp_repr_str(a, pv_path_as_posix(a, v->u.s)));
        avp_buf_putc(b, ')');
        break;
    case PV_LIST:
    case PV_TUPLE: {
        bool tuple = v->kind == PV_TUPLE;
        avp_buf_putc(b, tuple ? '(' : '[');
        for (size_t k = 0; k < v->u.list->n; k++) {
            if (k) avp_buf_putz(b, ", ");
            repr_into(b, a, v->u.list->v[k]);
        }
        if (tuple && v->u.list->n == 1) avp_buf_putc(b, ',');
        avp_buf_putc(b, tuple ? ')' : ']');
        break;
    }
    case PV_DICT:
        avp_buf_putc(b, '{');
        for (size_t k = 0; k < v->u.dict->n; k++) {
            if (k) avp_buf_putz(b, ", ");
            avp_buf_put(b, avp_repr_str(a, v->u.dict->keys[k]));
            avp_buf_putz(b, ": ");
            repr_into(b, a, v->u.dict->vals[k]);
        }
        avp_buf_putc(b, '}');
        break;
    case PV_INSTANCE: avp_buf_printf(b, "<%s object at %p>", v->u.inst->cls, (const void *)v); break;
    case PV_OPAQUE: avp_buf_printf(b, "<%s at %p>", v->u.opaque.tag, v->u.opaque.p); break;
    }
}

AvpStr pv_repr(AvpArena *a, const Pv *v) {
    AvpBuf b;
    avp_buf_init(&b, a);
    repr_into(&b, a, v);
    return avp_buf_str(&b);
}

AvpStr pv_to_str(AvpArena *a, const Pv *v) {
    if (v->kind == PV_STR || v->kind == PV_PATH) return v->u.s;
    return pv_repr(a, v);
}

const char *pv_kind_name(const Pv *v) {
    switch (v->kind) {
    case PV_NONE: return "NoneType";
    case PV_BOOL: return "bool";
    case PV_INT: return "int";
    case PV_STR: return "str";
    case PV_PATH: return "WindowsPath";
    case PV_LIST: return "list";
    case PV_TUPLE: return "tuple";
    case PV_DICT: return "dict";
    case PV_INSTANCE: return v->u.inst->cls;
    case PV_OPAQUE: return v->u.opaque.tag;
    }
    return "object";
}

/* ---- Windows pathlib ---- */

typedef struct {
    AvpStr drv, root;
    AvpStrVec tail;
} PathParts;

static AvpStr slice(AvpStr s, size_t from, size_t to) {
    if (from > s.n) from = s.n;
    if (to > s.n) to = s.n;
    if (to < from) to = from;
    return avp_strn(s.p + from, to - from);
}

static i64 find_sep(AvpStr s, size_t from) {
    for (size_t i = from; i < s.n; i++)
        if (s.p[i] == '\\') return (i64)i;
    return -1;
}

/* ntpath.splitroot on a path whose separators are already backslashes */
static void splitroot(AvpStr p, AvpStr *drv, AvpStr *root, AvpStr *rest) {
    AvpStr empty = {"", 0};
    if (p.n >= 1 && p.p[0] == '\\') {
        if (p.n >= 2 && p.p[1] == '\\') {
            static const char unc[] = "\\\\?\\UNC\\";
            size_t start = 2;
            if (p.n >= 8) {
                bool is_unc = true;
                for (size_t i = 0; i < 8; i++) {
                    char c = p.p[i];
                    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
                    if (c != unc[i]) is_unc = false;
                }
                if (is_unc) start = 8;
            }
            i64 index = find_sep(p, start);
            i64 index2 = index < 0 ? -1 : find_sep(p, (size_t)index + 1);
            if (index2 < 0) {
                *drv = p, *root = empty, *rest = empty;
                return;
            }
            *drv = slice(p, 0, (size_t)index2);
            *root = slice(p, (size_t)index2, (size_t)index2 + 1);
            *rest = slice(p, (size_t)index2 + 1, p.n);
            return;
        }
        *drv = empty, *root = slice(p, 0, 1), *rest = slice(p, 1, p.n);
        return;
    }
    if (p.n >= 2 && p.p[1] == ':') {
        if (p.n >= 3 && p.p[2] == '\\') {
            *drv = slice(p, 0, 2), *root = slice(p, 2, 3), *rest = slice(p, 3, p.n);
            return;
        }
        *drv = slice(p, 0, 2), *root = empty, *rest = slice(p, 2, p.n);
        return;
    }
    *drv = empty, *root = empty, *rest = p;
}

static AvpStr backslashed(AvpArena *a, AvpStr s) {
    char *c = avp_alloc_noz(a, s.n + 1);
    for (size_t i = 0; i < s.n; i++) c[i] = s.p[i] == '/' ? '\\' : s.p[i];
    c[s.n] = 0;
    return avp_strn(c, s.n);
}

/* pathlib.PurePath._parse_path */
static PathParts parse_path(AvpArena *a, AvpStr s) {
    PathParts r = {0};
    r.drv = r.root = (AvpStr){"", 0};
    if (s.n == 0) return r;
    AvpStr p = backslashed(a, s), rest;
    splitroot(p, &r.drv, &r.root, &rest);
    if (r.root.n == 0 && r.drv.n && r.drv.p[0] == '\\' && r.drv.p[r.drv.n - 1] != '\\') {
        size_t nparts = 1;
        for (size_t i = 0; i < r.drv.n; i++)
            if (r.drv.p[i] == '\\') nparts++;
        if (nparts == 4) {
            /* drv_parts[2] not in '?.': a substring test, so '', '?', '.' and '?.' give no root */
            size_t i = 2, j = 2;
            while (j < r.drv.n && r.drv.p[j] != '\\') j++;
            AvpStr part2 = slice(r.drv, i, j);
            bool in = part2.n == 0 || avp_str_eq(part2, AVP_S("?")) || avp_str_eq(part2, AVP_S(".")) ||
                      avp_str_eq(part2, AVP_S("?."));
            if (!in) r.root = AVP_S("\\");
        } else if (nparts == 6) {
            r.root = AVP_S("\\");
        }
    }
    size_t start = 0;
    for (size_t i = 0; i <= rest.n; i++) {
        if (i == rest.n || rest.p[i] == '\\') {
            AvpStr part = slice(rest, start, i);
            if (part.n && !avp_str_eq(part, AVP_S("."))) avp_vec_push(&r.tail, part);
            start = i + 1;
        }
    }
    return r;
}

/* pathlib.PurePath._format_parsed_parts(...) or '.' */
static AvpStr format_parts(AvpArena *a, AvpStr drv, AvpStr root, const AvpStr *tail, size_t n) {
    AvpBuf b;
    avp_buf_init(&b, a);
    if (drv.n || root.n) {
        avp_buf_put(&b, drv);
        avp_buf_put(&b, root);
    } else if (n && tail[0].n >= 2 && tail[0].p[1] == ':') {
        avp_buf_putz(&b, ".\\");
    }
    for (size_t i = 0; i < n; i++) {
        if (i) avp_buf_putc(&b, '\\');
        avp_buf_put(&b, tail[i]);
    }
    if (b.n == 0) return AVP_S(".");
    return avp_buf_str(&b);
}

AvpStr pv_path_norm(AvpArena *a, AvpStr s) {
    PathParts pp = parse_path(a, s);
    AvpStr r = format_parts(a, pp.drv, pp.root, pp.tail.v, pp.tail.n);
    avp_vec_free(&pp.tail);
    return r;
}

static bool ieq(AvpStr x, AvpStr y) {
    if (x.n != y.n) return false;
    for (size_t i = 0; i < x.n; i++) {
        char c = x.p[i], d = y.p[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        if (d >= 'A' && d <= 'Z') d = (char)(d + 32);
        if (c != d) return false;
    }
    return true;
}

AvpStr pv_path_join(AvpArena *a, AvpStr base, AvpStr other) {
    /* ntpath.join(base, other), then parsed */
    AvpStr bd, br, bp, od, or_, op;
    splitroot(backslashed(a, base), &bd, &br, &bp);
    splitroot(backslashed(a, other), &od, &or_, &op);
    AvpBuf path;
    avp_buf_init(&path, a);
    if (or_.n) {
        if (od.n || !bd.n) bd = od;
        br = or_;
        avp_buf_put(&path, op);
    } else if (od.n && !avp_str_eq(od, bd) && !ieq(od, bd)) {
        bd = od, br = or_;
        avp_buf_put(&path, op);
    } else {
        if (od.n) bd = od;
        avp_buf_put(&path, bp);
        if (path.n && path.p[path.n - 1] != '\\') avp_buf_putc(&path, '\\');
        avp_buf_put(&path, op);
    }
    AvpBuf r;
    avp_buf_init(&r, a);
    avp_buf_put(&r, bd);
    if (path.n && !br.n && bd.n && bd.p[bd.n - 1] != ':' && bd.p[bd.n - 1] != '\\') avp_buf_putc(&r, '\\');
    else avp_buf_put(&r, br);
    avp_buf_put(&r, avp_buf_str(&path));
    return pv_path_norm(a, avp_buf_str(&r));
}

AvpStr pv_path_parent(AvpArena *a, AvpStr p) {
    PathParts pp = parse_path(a, p);
    AvpStr r = format_parts(a, pp.drv, pp.root, pp.tail.v, pp.tail.n ? pp.tail.n - 1 : 0);
    avp_vec_free(&pp.tail);
    return r;
}

AvpStr pv_path_name(AvpStr p) {
    PathParts pp = parse_path(avp_arena(), p); /* the parts are views of a copy in the long-lived arena */
    AvpStr last = pp.tail.n ? pp.tail.v[pp.tail.n - 1] : (AvpStr){"", 0};
    avp_vec_free(&pp.tail);
    return last;
}

AvpStr pv_path_suffix(AvpStr p) {
    AvpStr name = pv_path_name(p);
    size_t lead = 0;
    while (lead < name.n && name.p[lead] == '.') lead++;
    AvpStr rest = slice(name, lead, name.n);
    i64 i = avp_str_rfind(rest, AVP_S("."), 0, (i64)rest.n);
    if (i < 0) return (AvpStr){"", 0};
    return slice(rest, (size_t)i, rest.n);
}

AvpStr pv_path_stem(AvpStr p) {
    AvpStr name = pv_path_name(p);
    i64 i = avp_str_rfind(name, AVP_S("."), 0, (i64)name.n);
    if (i >= 0) {
        AvpStr stem = slice(name, 0, (size_t)i);
        for (size_t k = 0; k < stem.n; k++)
            if (stem.p[k] != '.') return stem;
    }
    return name;
}

AvpStr pv_path_with_suffix(AvpArena *a, AvpStr p, AvpStr suffix) {
    AvpStr stem = pv_path_stem(p);
    if (!stem.n) return (AvpStr){NULL, 0};
    if (suffix.n && suffix.p[0] != '.') return (AvpStr){NULL, 0};
    AvpBuf name;
    avp_buf_init(&name, a);
    avp_buf_put(&name, stem);
    avp_buf_put(&name, suffix);
    AvpStr nm = avp_buf_str(&name);
    for (size_t i = 0; i < nm.n; i++)
        if (nm.p[i] == '\\' || nm.p[i] == '/') return (AvpStr){NULL, 0};
    if (avp_str_eq(nm, AVP_S("."))) return (AvpStr){NULL, 0};
    PathParts pp = parse_path(a, p);
    if (!pp.tail.n) {
        avp_vec_free(&pp.tail);
        return (AvpStr){NULL, 0};
    }
    pp.tail.v[pp.tail.n - 1] = nm;
    AvpStr r = format_parts(a, pp.drv, pp.root, pp.tail.v, pp.tail.n);
    avp_vec_free(&pp.tail);
    return r;
}

AvpStr pv_path_as_posix(AvpArena *a, AvpStr p) {
    char *c = avp_alloc_noz(a, p.n + 1);
    for (size_t i = 0; i < p.n; i++) c[i] = p.p[i] == '\\' ? '/' : p.p[i];
    c[p.n] = 0;
    return avp_strn(c, p.n);
}

bool pv_path_is_absolute(AvpStr p) {
    /* ntpath.isabs: s[:3] with '/' as '\\' starts with ':\\' at 1, or with '\\\\' */
    char s[3] = {0, 0, 0};
    for (size_t i = 0; i < 3 && i < p.n; i++) s[i] = p.p[i] == '/' ? '\\' : p.p[i];
    size_t n = p.n < 3 ? p.n : 3;
    if (n >= 3 && s[1] == ':' && s[2] == '\\') return true;
    return n >= 2 && s[0] == '\\' && s[1] == '\\';
}
