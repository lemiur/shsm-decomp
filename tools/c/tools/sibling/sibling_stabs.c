/* sibling_stabs: gcc's stabs type strings (as ECOFF local symbols carry them), parsed per file and printed as
 * declaration drafts.
 *
 * Supported: type references N and (F,N), definitions N=..., ranges r, pointers *, references &, arrays ar, structs
 * s and unions u (with C++ bases !n,..., fields name:/v<type>,<bit offset>,<bit size>; static members
 * name:<type>:<physname>; method groups name::<type>:<physname>;<access><modifier><kind> where the kind is . normal,
 * ? static or *<vtable index>;<class>; virtual, and the trailing ~%<class>; vptr owner), enums e, functions f,
 * method types # and ##, member pointers @, const k, volatile B, type attributes @s<bits>; and cross references
 * x(s|u|e)name:. Anything else becomes an "unknown stabs construct" type with a note; the parser skips to the next
 * delimiter and never reads past the string. Type numbers are per file (ECOFF stabs carry no N_BINCL sharing). */
#include "sibling_foreign.h"

#include "avp/sort.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct StType StType;
typedef struct {
    const char *name;
    StType *type;
    i64 bitpos, bitsize; /* bitpos -1: a static member */
    char access;         /* '0' private, '1' protected, '2' public, 0 unknown */
} StField;
typedef struct {
    StType *type;
    i64 bitpos;
    bool is_virtual;
    char access;
} StBase;
typedef struct {
    const char *name, *phys;
    StType *type; /* the method type (# or ##): its target is the return type */
    char access, kind; /* kind '.', '?', '*' */
    bool is_const, is_volatile;
    i64 vindex; /* -1 when not virtual */
    StType *vowner; /* the class whose vtable holds the slot */
} StMethod;
typedef struct {
    const char *name;
    i64 value;
} StEnum;

struct StType {
    char kind; /* 0 undefined, 'r' 'p'(ointer) '&' 'a' 's' 'u' 'e' 'f' 'x' '#' '@' 'k' 'B' 'n'(alias) '?' */
    const char *name; /* typedef or tag name */
    i64 num;
    StType *target, *index, *owner; /* owner: method or member pointer class; vptr owner for structs */
    i64 size, lo, hi;
    char xkind;
    AVP_VEC(StField) fields;
    AVP_VEC(StBase) bases;
    AVP_VEC(StMethod) methods;
    AVP_VEC(StEnum) enums;
    AVP_VEC(StType *) params;
    const char *note; /* unknown construct text */
};

typedef struct {
    AvpArena *a;
    AvpIMap *slots; /* type number -> StType* */
    i64 unknown;    /* constructs not understood */
} StFile;

typedef struct {
    StFile *f;
    const char *p, *end;
} St;

static StType *slot(StFile *f, i64 n) {
    bool created;
    AvpVal *v = avp_imap_put(f->slots, (u64)n, &created);
    if (created) {
        StType *t = avp_alloc(f->a, sizeof *t);
        t->num = n;
        v->p = t;
    }
    return v->p;
}

static bool at(St *s, char c) { return s->p < s->end && *s->p == c; }
static bool eat(St *s, char c) {
    if (!at(s, c)) return false;
    s->p++;
    return true;
}
static i64 number(St *s) {
    bool neg = eat(s, '-');
    i64 n = 0;
    while (s->p < s->end && isdigit((unsigned char)*s->p)) n = n * 10 + (*s->p++ - '0');
    return neg ? -n : n;
}
/* text up to the delimiter (not consumed) */
static const char *until(St *s, char d) {
    const char *b = s->p;
    while (s->p < s->end && *s->p != d) s->p++;
    return avp_sprintf(s->f->a, "%.*s", (int)(s->p - b), b);
}

static StType *typeref(St *s);

static StType *unknown(St *s, const char *from) {
    s->f->unknown++;
    StType *t = avp_alloc(s->f->a, sizeof *t);
    t->kind = '?';
    const char *b = from;
    while (s->p < s->end && *s->p != ';' && *s->p != ',') s->p++;
    t->note = avp_sprintf(s->f->a, "%.*s", (int)(s->p - b > 40 ? 40 : s->p - b), b);
    return t;
}

static void parse_struct(St *s, StType *t) {
    t->size = number(s);
    if (eat(s, '!')) {
        i64 n = number(s);
        eat(s, ',');
        for (i64 k = 0; k < n && s->p < s->end; k++) {
            StBase b = {0};
            b.is_virtual = s->p < s->end && *s->p++ == '1';
            b.access = s->p < s->end ? *s->p++ : 0;
            b.bitpos = number(s);
            eat(s, ',');
            b.type = typeref(s);
            eat(s, ';');
            avp_vec_push(&t->bases, b);
        }
    }
    while (s->p < s->end && *s->p != ';') {
        const char *nm = until(s, ':');
        if (!eat(s, ':')) break;
        if (eat(s, ':')) { /* method group */
            while (s->p < s->end && *s->p != ';') {
                StMethod m = {.name = nm, .vindex = -1};
                m.type = typeref(s);
                if (!eat(s, ':')) break;
                m.phys = until(s, ';');
                eat(s, ';');
                m.access = s->p < s->end ? *s->p++ : 0;
                char mod = s->p < s->end ? *s->p++ : 0; /* A plain, B const, C volatile, D const volatile */
                m.is_const = mod == 'B' || mod == 'D';
                m.is_volatile = mod == 'C' || mod == 'D';
                m.kind = s->p < s->end ? *s->p++ : 0;
                if (m.kind == '*') {
                    m.vindex = number(s);
                    eat(s, ';');
                    if (!at(s, ';')) m.vowner = typeref(s);
                    eat(s, ';');
                } else if (m.kind != '.' && m.kind != '?') s->f->unknown++;
                avp_vec_push(&t->methods, m);
            }
            eat(s, ';');
            continue;
        }
        StField fl = {.name = nm};
        if (eat(s, '/')) fl.access = s->p < s->end ? *s->p++ : 0;
        fl.type = typeref(s);
        if (eat(s, ':')) { /* static member: :physname; */
            until(s, ';');
            fl.bitpos = -1;
        } else {
            eat(s, ',');
            fl.bitpos = number(s);
            eat(s, ',');
            fl.bitsize = number(s);
        }
        if (!eat(s, ';')) break;
        avp_vec_push(&t->fields, fl);
    }
    eat(s, ';');
    if (at(s, '~') && s->p + 1 < s->end && s->p[1] == '%') {
        s->p += 2;
        t->owner = typeref(s);
        eat(s, ';');
    }
}

static void typedef_body(St *s, StType *t) {
    const char *from = s->p;
    if (s->p >= s->end) {
        t->kind = '?';
        t->note = "(empty)";
        return;
    }
    char c = *s->p;
    if (isdigit((unsigned char)c) || c == '(' || c == '-') {
        t->kind = 'n';
        t->target = typeref(s);
        if (t->target == t) t->kind = 'v'; /* void: N=N */
        return;
    }
    s->p++;
    switch (c) {
    case 'r':
        t->kind = 'r';
        t->target = typeref(s);
        eat(s, ';');
        t->lo = number(s);
        eat(s, ';');
        t->hi = number(s);
        eat(s, ';');
        return;
    case '*': t->kind = 'p'; t->target = typeref(s); return;
    case '&': t->kind = '&'; t->target = typeref(s); return;
    case 'k': t->kind = 'k'; t->target = typeref(s); return;
    case 'B': t->kind = 'B'; t->target = typeref(s); return;
    case 'f': t->kind = 'f'; t->target = typeref(s); return;
    case 'a':
        t->kind = 'a';
        if (at(s, 'r')) {
            s->p++;
            t->index = typeref(s);
            eat(s, ';');
            t->lo = number(s);
            eat(s, ';');
            t->hi = number(s);
            eat(s, ';');
        }
        t->target = typeref(s);
        return;
    case 's':
    case 'u':
        t->kind = c;
        parse_struct(s, t);
        return;
    case 'e':
        t->kind = 'e';
        while (s->p < s->end && *s->p != ';') {
            StEnum e = {.name = until(s, ':')};
            if (!eat(s, ':')) break;
            e.value = number(s);
            eat(s, ',');
            avp_vec_push(&t->enums, e);
        }
        eat(s, ';');
        return;
    case 'x':
        t->kind = 'x';
        t->xkind = s->p < s->end ? *s->p++ : 0;
        t->name = until(s, ':');
        eat(s, ':');
        return;
    case '#':
        t->kind = '#';
        if (eat(s, '#')) {
            t->target = typeref(s);
            eat(s, ';');
            return;
        }
        t->owner = typeref(s);
        eat(s, ',');
        t->target = typeref(s);
        while (eat(s, ',')) avp_vec_push(&t->params, typeref(s));
        eat(s, ';');
        return;
    case '@':
        if (at(s, 's')) { /* type attribute @s<bits>; then the type */
            until(s, ';');
            eat(s, ';');
            typedef_body(s, t);
            return;
        }
        t->kind = '@';
        t->owner = typeref(s);
        eat(s, ',');
        t->target = typeref(s);
        return;
    default:
        s->p = from;
        *t = *unknown(s, from);
        return;
    }
}

static StType *typeref_ex(St *s, bool define) {
    i64 n;
    const char *from = s->p;
    if (eat(s, '(')) {
        i64 fnum = number(s);
        eat(s, ',');
        n = fnum * 100000 + number(s);
        eat(s, ')');
    } else if (s->p < s->end && (isdigit((unsigned char)*s->p) || *s->p == '-')) n = number(s);
    else return unknown(s, from);
    StType *t = slot(s->f, n);
    if (define && eat(s, '=')) {
        if (t->kind && t->kind != 'x') { /* defined twice: consume the text, keep the first (a cross reference is replaced) */
            StType *scratch = avp_alloc(s->f->a, sizeof *scratch);
            scratch->num = n;
            typedef_body(s, scratch);
        } else typedef_body(s, t);
    }
    return t;
}

static StType *typeref(St *s) { return typeref_ex(s, true); }

/* the type a type stab names, already parsed: the slot of its number */
static StType *named_slot(StFile *f, const char *str, size_t n) {
    St s = {.f = f, .p = str + n + 2, .end = str + strlen(str)};
    if (at(&s, 't')) s.p++;
    StType *t = typeref_ex(&s, false);
    while (t && t->kind == 'n' && t->target && t->target != t) t = t->target;
    return t;
}

/* One stab string: name:<descriptor><type>. Registers the type and its name. */
static void parse_stab(StFile *f, const char *str) {
    const char *colon = strchr(str, ':');
    if (!colon || colon == str) return;
    St s = {.f = f, .p = colon + 1, .end = str + strlen(str)};
    char d = *s.p;
    if (d != 't' && d != 'T') return; /* only type stabs */
    s.p++;
    bool tag = d == 'T', also_typedef = false;
    if (tag && at(&s, 't')) {
        also_typedef = true;
        s.p++;
    }
    StType *t = typeref(&s);
    const char *name = avp_sprintf(f->a, "%.*s", (int)(colon - str), str);
    if (!t->name || (tag && !also_typedef && t->kind != 'n')) t->name = name;
    (void)also_typedef;
}

/* ---- rendering ---- */

static const char *tname(AvpArena *a, StType *t, int depth);

static const char *tname(AvpArena *a, StType *t, int depth) {
    if (!t) return "?";
    if (depth > 12) return "...";
    if (t->name && t->kind != 'n') return t->name;
    if (t->name && t->kind == 'n') return t->name;
    switch (t->kind) {
    case 'v': return "void";
    case 'n': return tname(a, t->target, depth + 1);
    case 'p': return avp_sprintf(a, "%s *", tname(a, t->target, depth + 1));
    case '&': return avp_sprintf(a, "%s &", tname(a, t->target, depth + 1));
    case 'k': return avp_sprintf(a, "const %s", tname(a, t->target, depth + 1));
    case 'B': return avp_sprintf(a, "volatile %s", tname(a, t->target, depth + 1));
    case 'f': return avp_sprintf(a, "%s ()", tname(a, t->target, depth + 1));
    case 'a': return avp_sprintf(a, "%s[%lld]", tname(a, t->target, depth + 1), (long long)(t->hi - t->lo + 1));
    case 'x': return t->name ? t->name : "?";
    case '#': return avp_sprintf(a, "%s (method)", tname(a, t->target, depth + 1));
    case '@': return avp_sprintf(a, "%s %s::*", tname(a, t->target, depth + 1), tname(a, t->owner, depth + 1));
    case 's': return "struct <anonymous>";
    case 'u': return "union <anonymous>";
    case 'e': return "enum <anonymous>";
    case 'r': return avp_sprintf(a, "<range %lld..%lld>", (long long)t->lo, (long long)t->hi);
    case '?': return avp_sprintf(a, "<unknown stabs: %s>", t->note ? t->note : "");
    default: return "<type not defined in this file>";
    }
}

/* a declaration "T name" with array suffixes moved behind the name */
static const char *decl(AvpArena *a, StType *t, const char *name) {
    StType *u = t;
    AvpBuf dims = {0};
    avp_buf_init(&dims, a);
    while (u && u->kind == 'a' && !u->name) {
        avp_buf_printf(&dims, "[%lld]", (long long)(u->hi - u->lo + 1));
        u = u->target;
    }
    const char *ts = tname(a, u, 0);
    size_t len = strlen(ts);
    bool tight = len && (ts[len - 1] == '*' || ts[len - 1] == '&');
    return avp_sprintf(a, "%s%s%s%s", ts, tight ? "" : " ", name, avp_buf_cz(&dims));
}

static const char *render(AvpArena *a, StType *t, bool full) {
    AvpBuf b = {0};
    avp_buf_init(&b, a);
    if (t->kind == 'e') {
        avp_buf_printf(&b, "enum %s {\n", t->name ? t->name : "");
        for (size_t k = 0; k < t->enums.n; k++)
            avp_buf_printf(&b, "    %s = %lld,\n", t->enums.v[k].name, (long long)t->enums.v[k].value);
        avp_buf_putz(&b, "};\n");
        return avp_buf_cz(&b);
    }
    if (t->kind != 's' && t->kind != 'u') {
        avp_buf_printf(&b, "typedef %s;\n", decl(a, t, t->name ? t->name : "?"));
        return avp_buf_cz(&b);
    }
    bool has_methods = t->methods.n > 0 || t->bases.n > 0;
    avp_buf_printf(&b, "%s %s", t->kind == 'u' ? "union" : has_methods ? "class" : "struct", t->name ? t->name : "");
    for (size_t k = 0; k < t->bases.n; k++) {
        StBase *bs = &t->bases.v[k];
        avp_buf_printf(&b, "%s %s%s%s /* 0x%02llX */", k ? "," : " :",
                       bs->access == '0' ? "private " : bs->access == '1' ? "protected " : "public ",
                       bs->is_virtual ? "virtual " : "", tname(a, bs->type, 0), (long long)(bs->bitpos / 8));
    }
    avp_buf_printf(&b, " { // size 0x%llX", (long long)t->size);
    if (t->owner) avp_buf_printf(&b, ", vptr from %s", t->owner == t ? "this class" : tname(a, t->owner, 0));
    avp_buf_putz(&b, "\n");
    for (size_t k = 0; k < t->fields.n; k++) {
        StField *f = &t->fields.v[k];
        const char *d = decl(a, f->type, f->name);
        if (f->bitpos < 0) {
            avp_buf_printf(&b, "    static %s;\n", d);
            continue;
        }
        bool bits = f->bitsize % 8 || f->bitpos % 8;
        const char *line = bits ? avp_sprintf(a, "%s : %lld;", d, (long long)f->bitsize) : avp_sprintf(a, "%s;", d);
        if (bits)
            avp_buf_printf(&b, "    %-48s // 0x%02llX bit %lld\n", line, (long long)(f->bitpos / 8),
                           (long long)(f->bitpos % 8));
        else avp_buf_printf(&b, "    %-48s // 0x%02llX\n", line, (long long)(f->bitpos / 8));
    }
    if (t->methods.n) {
        avp_buf_putz(&b, "    // methods\n");
        for (size_t k = 0; k < t->methods.n; k++) {
            StMethod *m = &t->methods.v[k];
            if (!full && strcmp(m->name, "__as") == 0 && strstr(m->phys, t->name ? t->name : "\x01")) continue;
            const char *ret = m->type && m->type->kind == '#' ? tname(a, m->type->target, 0) : "?";
            avp_buf_printf(&b, "    %s%s%s %s(/* %s */)%s;", m->kind == '*' ? "virtual " : "",
                           m->kind == '?' ? "static " : "", ret, m->name, m->phys, m->is_const ? " const" : "");
            if (m->kind == '*')
                avp_buf_printf(&b, " // vtable index %lld%s%s", (long long)m->vindex,
                               m->vowner && m->vowner != t ? " of " : "",
                               m->vowner && m->vowner != t ? tname(a, m->vowner, 0) : "");
            avp_buf_putz(&b, "\n");
        }
    }
    avp_buf_putz(&b, "};\n");
    return avp_buf_cz(&b);
}

/* ---- the command ---- */

typedef struct {
    const char *text;
    i64 count;
    const char *file;
} Layout;

static bool names_type(const char *str, const char *name, size_t n) {
    return strncmp(str, name, n) == 0 && str[n] == ':' && (str[n + 1] == 'T' || str[n + 1] == 't');
}

int sib_stabs_types(AvpArena *a, SibMdebug *m, const char *elf, const char *name, bool full, ShsmOutput *out) {
    if (!name) { /* list the tagged struct/class types with their file counts */
        AvpMap *tags = avp_map_new();
        for (i32 fd = 0; fd < (i32)m->fds.n; fd++) {
            SibStabVec v = {0};
            sib_md_stabs(a, m, fd, &v);
            for (size_t k = 0; k < v.n; k++) {
                const char *c = strchr(v.v[k].str, ':');
                if (!c || c[1] != 'T') continue;
                const char *eq = strchr(c, '=');
                if (!eq || (eq[1] != 's' && eq[1] != 'u' && eq[1] != 'e')) continue;
                bool created;
                avp_map_put(tags, avp_strn(v.v[k].str, (size_t)(c - v.v[k].str)), &created)->i++;
            }
            avp_vec_free(&v);
        }
        AvpStrVec keys = avp_map_keys(tags);
        avp_sort_strs(keys.v, keys.n);
        shsm_output_linez(out, avp_sprintf(a, "%zu tagged struct/class/union/enum types in %s (files defining each):",
                                           keys.n, elf));
        AvpJson *arr = avp_json_arr(a);
        for (size_t k = 0; k < keys.n; k++) {
            i64 c = avp_map_get(tags, keys.v[k])->i;
            shsm_output_linez(out, avp_sprintf(a, "  %-40s %lld", avp_cz(a, keys.v[k]), (long long)c));
            avp_json_arr_push(a, arr, avp_json_str(a, keys.v[k]));
        }
        shsm_output_data(out, "types", arr);
        avp_map_free(tags);
        return 0;
    }
    size_t n = strlen(name);
    AVP_VEC(Layout) lay = {0};
    i64 defs = 0, unknown = 0;
    for (i32 fd = 0; fd < (i32)m->fds.n; fd++) {
        SibStabVec v = {0};
        sib_md_stabs(a, m, fd, &v);
        bool any = false;
        for (size_t k = 0; k < v.n && !any; k++) any = names_type(v.v[k].str, name, n);
        if (!any) {
            avp_vec_free(&v);
            continue;
        }
        StFile f = {.a = a, .slots = avp_imap_new()};
        AVP_VEC(StType *) found = {0};
        for (size_t k = 0; k < v.n; k++) {
            parse_stab(&f, v.v[k].str);
            if (names_type(v.v[k].str, name, n)) avp_vec_push(&found, named_slot(&f, v.v[k].str, n));
        }
        for (size_t k = 0; k < found.n; k++) {
            StType *t = found.v[k];
            bool dup = false;
            for (size_t j = 0; j < k; j++) dup |= found.v[j] == t;
            if (dup || t->kind == 'x' || t->kind == 0) continue; /* the same type again, or a forward reference */
            if (!t->name) t->name = name;
            const char *text = render(a, t, full);
            defs++;
            size_t j = 0;
            for (; j < lay.n; j++)
                if (strcmp(lay.v[j].text, text) == 0) break;
            if (j == lay.n) avp_vec_push(&lay, ((Layout){text, 0, m->fds.v[fd].name}));
            lay.v[j].count++;
        }
        unknown += f.unknown;
        avp_vec_free(&found);
        avp_imap_free(f.slots);
        avp_vec_free(&v);
    }
    if (!defs) {
        shsm_output_linez(out, avp_sprintf(a, "%s: no stabs type definition in %s", name, elf));
        avp_vec_free(&lay);
        return 1;
    }
    shsm_output_linez(out, avp_sprintf(a, "%s: %lld definitions in %s, %zu distinct layout%s", name, (long long)defs, elf,
                                       lay.n, lay.n == 1 ? "" : "s"));
    shsm_output_linez(out, "// offsets and sizes are the PS2 build's (gcc 2.95, MIPS): evidence for member names, order"
                           " and types, not for the Wii layout");
    if (unknown)
        shsm_output_linez(out, avp_sprintf(a, "// note: %lld stabs constructs in these files were not understood"
                                              " (shown as <unknown stabs: ...>)", (long long)unknown));
    AvpJson *arr = avp_json_arr(a);
    for (size_t j = 0; j < lay.n; j++) {
        shsm_output_linez(out, "");
        shsm_output_linez(out, avp_sprintf(a, "// layout %zu of %zu: %lld file%s, e.g. %s", j + 1, lay.n,
                                           (long long)lay.v[j].count, lay.v[j].count == 1 ? "" : "s", lay.v[j].file));
        shsm_output_line(out, avp_str_rstrip(avp_str(lay.v[j].text)));
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("files"), avp_json_int(a, lay.v[j].count));
        avp_json_set(a, o, AVP_S("example_file"), avp_json_strz(a, lay.v[j].file));
        avp_json_set(a, o, AVP_S("text"), avp_json_strz(a, lay.v[j].text));
        avp_json_arr_push(a, arr, o);
    }
    shsm_output_data(out, "layouts", arr);
    avp_vec_free(&lay);
    return 0;
}

/* For tests: parse the stab strings given (one file) and render the named type; NULL when absent. */
const char *sib_stabs_render(AvpArena *a, const char *const *stabs, size_t n, const char *name, bool full,
                             i64 *unknown) {
    StFile f = {.a = a, .slots = avp_imap_new()};
    StType *t = NULL;
    size_t len = strlen(name);
    for (size_t k = 0; k < n; k++) {
        parse_stab(&f, stabs[k]);
        if (names_type(stabs[k], name, len) && (!t || t->kind == 'x' || !t->kind)) t = named_slot(&f, stabs[k], len);
    }
    const char *r = NULL;
    if (t && t->kind != 'x' && t->kind != 0) {
        if (!t->name) t->name = name;
        r = render(a, t, full);
    }
    if (unknown) *unknown = f.unknown;
    avp_imap_free(f.slots);
    return r;
}
