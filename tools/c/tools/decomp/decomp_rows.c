/* decomp_rows: symbols.txt -> the apply script's input (decomp.py BUILTINS, _outside_templates, param_spec,
 * known_classes, signature, symbol_rows, format_rows, parse_rows, write_rows, read_rows, diff_rows, affected). */
#include "decomp.h"
#include "shsm_mangle.h"
#include "shsm_re.h"

#include <string.h>

static const char *const BUILTINS[][2] = {
    {"void", "void"},          {"bool", "bool"},
    {"char", "char"},          {"signed char", "char"},
    {"unsigned char", "uchar"}, {"short", "short"},
    {"unsigned short", "ushort"}, {"int", "int"},
    {"unsigned int", "uint"},  {"long", "long"},
    {"unsigned long", "ulong"}, {"long long", "longlong"},
    {"unsigned long long", "ulonglong"}, {"float", "float"},
    {"double", "double"},      {"long double", "double"},
    {"wchar_t", "wchar"},
};

static const char *builtin(AvpStr base) {
    for (size_t i = 0; i < sizeof BUILTINS / sizeof BUILTINS[0]; i++)
        if (avp_str_eqz(base, BUILTINS[i][0])) return BUILTINS[i][1];
    return NULL;
}

/* _outside_templates(text): every <...> argument list blanked (byte for byte here: the names are ASCII, and the
 * text and its blanked copy stay aligned either way). */
static char *outside_templates(AvpArena *a, AvpStr t) {
    char *out = avp_alloc(a, t.n + 1);
    int depth = 0;
    for (size_t i = 0; i < t.n; i++) {
        char ch = t.p[i];
        if (ch == '<') depth++;
        out[i] = depth == 0 ? ch : ' ';
        if (ch == '>') depth--;
    }
    return out;
}

const char *dc_param_spec(AvpArena *a, const char *text) {
    AvpStr t = avp_str_strip(avp_str(text));
    if (avp_str_eqz(t, "...")) return "...";
    char *flat = outside_templates(a, t);
    AvpStr fl = avp_strn(flat, t.n);
    if (avp_str_contains(fl, AVP_S("::*"))) return NULL; /* pointer to member: a 12-byte object under CodeWarrior */
    if (avp_str_findc(fl, '(') >= 0) return "code*";
    /* arrays: spans of \[[^\]]*\] in flat, removed from both (right to left) */
    char *tt = avp_alloc(a, t.n + 1), *ff = avp_alloc(a, t.n + 1);
    size_t n = 0, arrays = 0;
    for (size_t i = 0; i < t.n;) {
        if (flat[i] == '[') {
            size_t j = i + 1;
            while (j < t.n && flat[j] != ']') j++;
            if (j < t.n) { /* a complete [..] */
                arrays++;
                i = j + 1;
                continue;
            }
        }
        tt[n] = t.p[i];
        ff[n] = flat[i];
        n++;
        i++;
    }
    size_t stars = arrays;
    AvpBuf base;
    avp_buf_init(&base, a);
    for (size_t i = 0; i < n; i++) {
        if (ff[i] == '*' || ff[i] == '&') stars++;
        else avp_buf_putc(&base, tt[i]);
    }
    AvpStr b = avp_buf_str(&base);
    b = shsm_re_sub(a, shsm_re("\\b(const|volatile)\\b", 0), AVP_S(" "), b, 0);
    b = shsm_re_sub(a, shsm_re("\\s+", 0), AVP_S(" "), b, 0);
    b = avp_str_strip(b);
    AvpBuf out;
    avp_buf_init(&out, a);
    const char *bi = builtin(b);
    if (bi) {
        avp_buf_putz(&out, bi);
    } else {
        if (!b.n) return NULL;
        if (stars == 0) return "u4";
        avp_buf_putz(&out, "S:");
        avp_buf_put(&out, b);
    }
    for (size_t i = 0; i < stars; i++) avp_buf_putc(&out, '*');
    return avp_buf_cz(&out);
}

static const char *join_scope(AvpArena *a, const ShsmDemangled *d) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < d->nscope; i++) {
        if (i) avp_buf_putz(&b, "::");
        avp_buf_putz(&b, d->scope[i]);
    }
    return avp_buf_cz(&b);
}

AvpSet *dc_known_classes(AvpArena *a, const ShsmSymbolTable *t) {
    AvpSet *out = avp_set_new();
    for (size_t i = 0; i < t->n; i++) {
        const char *name = t->symbols[i].name;
        if (!strstr(name, "__")) continue;
        if (strncmp(name, "__vt__", 6) && strncmp(name, "__ct__", 6) && strncmp(name, "__dt__", 6)) continue;
        ShsmDemangled *d = shsm_mangle_parse(a, name);
        if (d && d->nscope) avp_set_add(out, avp_str(join_scope(a, d)));
    }
    avp_set_del(out, AVP_S(""));
    return out;
}

void dc_signature(AvpArena *a, const char *name, const AvpSet *classes, const char **this_, const char **params) {
    *this_ = "";
    *params = "-";
    ShsmDemangled *d = shsm_mangle_parse(a, name);
    if (!d || strcmp(d->kind, "function") != 0 || (d->thunk && *d->thunk) || d->local) return;
    const char *scope = join_scope(a, d);
    const char *this_class = "";
    if (*scope) {
        if (!avp_set_has(classes, avp_str(scope))) return;
        this_class = scope;
    }
    AvpBuf b;
    avp_buf_init(&b, a);
    size_t count = 0;
    bool only_void = d->nparams == 1;
    for (size_t i = 0; i < d->nparams; i++) {
        const char *s = dc_param_spec(a, d->params[i]);
        if (!s) return;
        if (strcmp(s, "void") != 0) only_void = false;
        if (count++) avp_buf_putc(&b, ';');
        avp_buf_putz(&b, s);
    }
    if (only_void) avp_buf_clear(&b), count = 0; /* specs == ["void"]: [] */
    if (strcmp(d->raw_name, "__dt") == 0) {
        /* CodeWarrior destructors take a hidden delete flag (a short) and test the whole register */
        if (count++) avp_buf_putc(&b, ';');
        avp_buf_putz(&b, "int");
    }
    *this_ = this_class;
    *params = avp_buf_cz(&b);
}

static const char *clean(AvpArena *a, const char *text) {
    if (!text) return "";
    AvpStr s = avp_str_replace(a, avp_str(text), AVP_S("\t"), AVP_S(" "), -1);
    s = avp_str_replace(a, s, AVP_S("\n"), AVP_S(" "), -1);
    return avp_cz(a, s);
}

DcRows dc_symbol_rows(AvpArena *a, const ShsmSymbolTable *t) {
    AvpSet *classes = dc_known_classes(a, t);
    DcRows rows = {0};
    for (size_t i = 0; i < t->n; i++) {
        const ShsmSymbol *s = &t->symbols[i];
        if (!s->type) continue;
        DcRow r = {0};
        r.address = (i64)s->address;
        r.size = s->has_size ? (i64)s->size : 0;
        r.name = s->name;
        if (strcmp(s->type, "function") == 0) {
            dc_signature(a, s->name, classes, &r.this_, &r.params);
            r.kind = "F";
            r.demangled = clean(a, shsm_demangle(a, s->name));
            r.dtype = "";
        } else if (strcmp(s->type, "object") == 0 || strcmp(s->type, "label") == 0) {
            bool typed = s->data && (!strcmp(s->data, "float") || !strcmp(s->data, "double") || !strcmp(s->data, "string"));
            r.kind = strcmp(s->type, "object") == 0 ? "O" : "L";
            r.demangled = clean(a, shsm_demangle(a, s->name));
            r.dtype = typed ? s->data : "";
            r.this_ = "";
            r.params = "";
        } else {
            continue;
        }
        avp_vec_push(&rows, r);
    }
    avp_set_free(classes);
    return rows;
}

AvpStr dc_format_rows(AvpArena *a, const DcRow *rows, size_t n) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < n; i++) {
        const DcRow *r = &rows[i];
        AvpStr line = avp_pyfmt(a, "%s\t%08X\t%d\t%s\t%s\t%s\t%s\t%s\n", r->kind, r->address, r->size, r->name,
                                r->demangled, r->dtype, r->this_, r->params);
        avp_buf_put(&b, line);
    }
    return avp_buf_str(&b);
}

DcRows dc_parse_rows(AvpArena *a, AvpStr text) {
    DcRows rows = {0};
    AvpStrVec lines = avp_str_splitlines(text, false);
    for (size_t i = 0; i < lines.n; i++) {
        AvpStrVec c = avp_str_split(lines.v[i], AVP_S("\t"), -1);
        if (c.n == 8) {
            DcRow r = {avp_cz(a, c.v[0]), dc_int16(c.v[1]), dc_int10(c.v[2]), avp_cz(a, c.v[3]),
                       avp_cz(a, c.v[4]), avp_cz(a, c.v[5]), avp_cz(a, c.v[6]), avp_cz(a, c.v[7])};
            avp_vec_push(&rows, r);
        }
        avp_vec_free(&c);
    }
    avp_vec_free(&lines);
    return rows;
}

void dc_write_rows(AvpArena *a, const char *path, const DcRow *rows, size_t n) {
    AvpStr dir = avp_path_dirname(path);
    if (dir.n && !avp_makedirs(avp_cz(a, dir), true)) shsm_raise_io();
    AvpStr text = dc_format_rows(a, rows, n);
    if (!avp_write_file(path, text.p, text.n)) shsm_raise_io();
}

DcRows dc_read_rows(AvpArena *a, const char *path) {
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TM_UTF8, &text)) {
        if (shsm_io_exc_code() == SHSM_EXC_VALUE) shsm_raise_io(); /* a decode error is not an OSError */
        return (DcRows){0};
    }
    return dc_parse_rows(a, text);
}

bool dc_row_eq(const DcRow *x, const DcRow *y) {
    return !strcmp(x->kind, y->kind) && x->address == y->address && x->size == y->size && !strcmp(x->name, y->name) &&
           !strcmp(x->demangled, y->demangled) && !strcmp(x->dtype, y->dtype) && !strcmp(x->this_, y->this_) &&
           !strcmp(x->params, y->params);
}

/* key(r) = (kind, address, name) as a map key */
static AvpStr row_key(AvpArena *a, const char *kind, i64 address, const char *name) {
    return avp_pyfmt(a, "%s\x01%d\x01%s", kind, address, name);
}

/* {key(r): r for r in rows}: insertion order of first appearance, the last row of a key kept */
static AvpMap *rows_by_key(AvpArena *a, const DcRows *rows, AvpStrVec *order) {
    AvpMap *m = avp_map_new();
    for (size_t i = 0; i < rows->n; i++) {
        const DcRow *r = &rows->v[i];
        bool created = false;
        AvpStr k = row_key(a, r->kind, r->address, r->name);
        avp_map_put(m, k, &created)->p = (void *)r;
        if (created) avp_vec_push(order, k);
    }
    return m;
}

static int cmp_delta(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const DcRow *p = x, *q = y;
    int rp = strcmp(p->kind, "R") != 0, rq = strcmp(q->kind, "R") != 0;
    if (rp != rq) return rp - rq;
    return p->address < q->address ? -1 : p->address > q->address;
}

static int cmp_range(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const DcRange *p = x, *q = y;
    if (p->start != q->start) return p->start < q->start ? -1 : 1;
    return p->end < q->end ? -1 : p->end > q->end;
}

void dc_diff_rows(AvpArena *a, const DcRows *old, const DcRows *new_, DcRows *delta, DcRanges *ranges,
                  AvpISet **funcs) {
    AvpStrVec old_keys = {0}, new_keys = {0};
    AvpMap *old_by = rows_by_key(a, old, &old_keys);
    AvpMap *new_by = rows_by_key(a, new_, &new_keys);
    *delta = (DcRows){0};
    *ranges = (DcRanges){0};
    *funcs = avp_iset_new();
    for (size_t i = 0; i < new_keys.n; i++) {
        const DcRow *r = avp_map_get(new_by, new_keys.v[i])->p;
        AvpVal *o = avp_map_get(old_by, new_keys.v[i]);
        if (!o || !dc_row_eq(o->p, r)) avp_vec_push(delta, *r);
    }
    for (size_t i = 0; i < old_keys.n; i++) {
        if (avp_map_has(new_by, old_keys.v[i])) continue;
        DcRow r = *(const DcRow *)avp_map_get(old_by, old_keys.v[i])->p;
        r.kind = "R";
        avp_vec_push(delta, r);
    }
    for (size_t i = 0; i < delta->n; i++) {
        const DcRow *r = &delta->v[i];
        DcRange rg = {r->address, r->address + (r->size > 1 ? r->size : 1)};
        avp_vec_push(ranges, rg);
        /* r[0] == "R" and any(o[0] == "F" and o[1] == r[1] for o in old_by.values() if o[3] == r[3]): an old row
         * keyed (F, address, name) */
        if (!strcmp(r->kind, "F") ||
            (!strcmp(r->kind, "R") && avp_map_has(old_by, row_key(a, "F", r->address, r->name))))
            avp_iset_add(*funcs, (u64)r->address);
    }
    avp_sort(delta->v, delta->n, sizeof(DcRow), cmp_delta, NULL, false);
    avp_sort(ranges->v, ranges->n, sizeof(DcRange), cmp_range, NULL, false);
    avp_map_free(old_by);
    avp_map_free(new_by);
    avp_vec_free(&old_keys);
    avp_vec_free(&new_keys);
}

AvpStrVec dc_affected(AvpArena *a, const AvpJson *index, const DcRanges *ranges, const AvpISet *funcs) {
    AvpStrVec out = {0};
    i64 *starts = AVP_NEWN(a, i64, ranges->n + 1);
    for (size_t i = 0; i < ranges->n; i++) starts[i] = ranges->v[i].start;
    AvpJson *fs = avp_json_is_obj(index) ? avp_json_getz(index, "functions") : NULL;
    for (size_t i = 0; fs && i < avp_json_len(fs); i++) {
        const AvpJsonMember *m = avp_json_member(fs, i);
        i64 addr = dc_int16(m->key);
        if (avp_iset_has(funcs, (u64)addr)) {
            avp_vec_push(&out, m->key);
            continue;
        }
        AvpJson *refs = avp_json_is_obj(m->val) ? avp_json_getz(m->val, "refs") : NULL;
        for (size_t j = 0; refs && j < avp_json_len(refs); j++) {
            AvpJson *ref = avp_json_at(refs, j);
            if (!ref || ref->kind != AVP_JSON_STRING) shsm_raise(SHSM_EXC_TYPE, "int() can't convert non-string with explicit base");
            i64 r = dc_int16(ref->u.str);
            i64 k = (i64)avp_bisect_right_i64(starts, ranges->n, r) - 1;
            if (k >= 0 && ranges->v[k].start <= r && r < ranges->v[k].end) {
                avp_vec_push(&out, m->key);
                break;
            }
        }
    }
    avp_sort_strs(out.v, out.n);
    return out;
}
