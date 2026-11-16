/* classes decl: the front end (classes_decl.h): gathers retail vtables (classes.json), the pairing and the sibling's
 * __vt__ objects and DWARF, builds each class's CdClass and prints the declaration, the symbols.txt changes it
 * implies, or (--stats) the coverage per library group. */
#include "classes_decl.h"

#include "avp/fs.h"
#include "shsm_libraries.h"
#include "shsm_mangle.h"
#include "shsm_repo.h"
#include "sibling_dwarf.h"
#include "sibling_vtables.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    AvpArena *a;
    const ClassesDb *db;
    const ShsmRepo *repo;
    const char *root;
    Sibling *sb;
    AvpIMap *peers;
    SibElf *elf;
    const char *elf_path;
    SdInfo *dw;
    AvpIMap *sub_at;   /* subprogram low pc -> die index + 1 */
    AvpMap *types;     /* type name -> AVP_VEC of die indices (as a heap vector) */
    CdSibVt *sv;
    size_t nsv;
    AvpStrVec hpaths, htexts; /* include/ headers (repo-relative paths) */
    AvpMap *scopes;    /* symbols.txt name -> scope word */
    AvpMap *sym_lines; /* symbols.txt name -> its whole line */
    const ShsmLibraries *libs;
    AvpIMap *tables;   /* ClassesClass* -> Tables* (tables_of) */
    const ClassesClass *second; /* find_class matched the second table of this class's object */
    AvpMap *callbacks; /* callback parameter list without const -> the retail mangled spelling (callback_consts) */
    AvpMap *sib_lib;   /* sibling function name -> its map library ("" for a game object) */
} Ctx;

typedef AVP_VEC(u32) U32Vec;

/* ---- loading ---- */

static void load_headers(Ctx *x) {
    const char *top = avp_path_join(x->a, x->root, "include");
    AvpWalk *w = avp_walk_open(top, true);
    AvpWalkItem it = {0};
    while (w && avp_walk_next(w, &it)) {
        for (size_t i = 0; i < it.files.n; i++) {
            AvpStr f = it.files.v[i];
            if (f.n < 2 || !(avp_str_endswith(f, AVP_S(".h")) || avp_str_endswith(f, AVP_S(".hpp")))) continue;
            const char *p = avp_path_join(x->a, avp_cz(x->a, it.dirpath), avp_cz(x->a, f));
            AvpBytes b = {0};
            if (!avp_read_all(x->a, p, &b)) continue;
            char *rel = NULL;
            if (!avp_path_relpath(x->a, p, x->root, &rel)) rel = (char *)p;
            for (char *q = rel; *q; q++)
                if (*q == '\\') *q = '/';
            avp_vec_push(&x->hpaths, avp_str(rel));
            avp_vec_push(&x->htexts, avp_strn((const char *)b.v, b.n));
        }
    }
    if (w) avp_walk_close(w);
}

static void load_scopes(Ctx *x, const char *symbols) {
    x->scopes = avp_map_new();
    x->sym_lines = avp_map_new();
    AvpBytes b = {0};
    if (!symbols || !avp_read_all(x->a, symbols, &b)) return;
    AvpStr t = avp_strn((const char *)b.v, b.n);
    AvpStrVec lines = avp_str_split(t, AVP_S("\n"), -1);
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr l = lines.v[i];
        i64 eq = avp_str_find(l, AVP_S(" = "), 0, (i64)l.n);
        i64 sc = avp_str_find(l, AVP_S("scope:"), 0, (i64)l.n);
        if (eq <= 0 || l.p[0] == '#') continue;
        avp_map_put(x->sym_lines, avp_strn(l.p, (size_t)eq), NULL)->p = avp_cz(x->a, avp_str_rstrip(l));
        if (sc < 0) { /* dtk's default scope */
            avp_map_put(x->scopes, avp_strn(l.p, (size_t)eq), NULL)->p = "global";
            continue;
        }
        size_t s = (size_t)sc + 6, e = s;
        while (e < l.n && isalpha((unsigned char)l.p[e])) e++;
        avp_map_put(x->scopes, avp_strn(l.p, (size_t)eq), NULL)->p = avp_cz(x->a, avp_strn(l.p + s, e - s));
    }
    avp_vec_free(&lines);
}

/* The command that sets the scope of symbols.txt entry `name`; NULL when symbols.txt has no entry. */
static const char *attr_command(Ctx *x, const char *name, const char *scope) {
    AvpVal *v = x->sym_lines ? avp_map_get(x->sym_lines, avp_str(name)) : NULL;
    if (!v) return NULL;
    bool plain = true;
    for (const char *q = name; *q; q++) plain &= isalnum((unsigned char)*q) || *q == '_';
    return avp_sprintf(x->a, "build/tools/names.exe attr %s%s%s --scope %s --apply", plain ? "" : "\"", name,
                       plain ? "" : "\"", scope);
}

static void load_sibling(Ctx *x) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        x->sb = sibling_open(x->repo, SIBLING_DEFAULT_NAME);
        avp_try_end(&t);
    } else {
        x->sb = NULL;
    }
    x->peers = x->sb ? sibling_peer_map(x->a, x->sb) : avp_imap_new();
    x->elf_path = x->sb ? sibling_elf_path(x->a, x->sb)
                        : shsm_repo_shared_path(x->a, x->repo, "orig/_ref/gormiti_wii/Exe/GormitiFinal.elf");
    AvpBytes b = {0};
    x->sub_at = avp_imap_new();
    x->types = avp_map_new();
    if (!avp_isfile(x->elf_path) || !avp_read_all(x->a, x->elf_path, &b)) {
        fprintf(stderr, "classes.exe: warning: sibling ELF not readable (%s): no sibling vtables or DWARF\n", x->elf_path);
        return;
    }
    AvpStr bytes = avp_strn((const char *)b.v, b.n);
    x->elf = sibling_elf_parse(x->a, bytes);
    if (x->elf) {
        x->sv = AVP_NEWN(x->a, CdSibVt, x->elf->nvt + 1);
        for (size_t i = 0; i < x->elf->nvt; i++) {
            const SibVtable *v = &x->elf->vt[i];
            x->sv[i] = (CdSibVt){v->symbol, v->cls, v->slots, v->n, v->weak};
        }
        x->nsv = x->elf->nvt;
    }
    SdSections sec;
    if (sd_elf_sections(bytes, &sec)) {
        x->dw = sd_parse(x->a, &sec);
        for (size_t k = 0; k < x->dw->ndies; k++) {
            const SdDie *d = &x->dw->dies[k];
            if (d->tag == SD_TAG_subprogram && (d->flags & SD_F_LOW)) {
                bool created;
                AvpVal *v = avp_imap_put(x->sub_at, d->low, &created);
                if (created) v->i = (i64)k + 1;
            }
            if (d->name && (sd_is_aggregate(d->tag) || d->tag == SD_TAG_enumeration_type || d->tag == SD_TAG_typedef ||
                            d->tag == SD_TAG_base_type)) {
                bool created;
                AvpVal *v = avp_map_put(x->types, avp_str(d->name), &created);
                if (created) v->p = avp_alloc(x->a, sizeof(U32Vec));
                avp_vec_push((U32Vec *)v->p, (u32)k);
            }
        }
    }
}

/* ---- the retail side ---- */

static CdWord *raw_words(Ctx *x, const ClassesClass *c, size_t *n) {
    AVP_VEC(CdWord) v = {0};
    if (!c->vtable) {
        *n = 0;
        return NULL;
    }
    for (size_t i = 0; i < c->vtable->n_words; i++) {
        const ClassesWord *w = &c->vtable->words[i];
        if (strcmp(w->status, "header") == 0) continue;
        if (w->group && strcmp(w->group, "complete") != 0) continue;
        i64 off = 0, addr = 0;
        classes_hex(w->offset, &off);
        CdWord o = {(u32)off, w->target, 0, NULL};
        if (w->target && classes_hex(w->target_address, &addr)) {
            o.address = (u32)addr;
            AvpVal *p = avp_imap_get(x->peers, (u64)addr);
            if (p) o.pair = ((SibPeer *)p->p)->name;
        }
        avp_vec_push(&v, o);
    }
    *n = v.n;
    return v.v;
}

/* One retail data object can hold two adjacent vtables (a class's table directly followed by its pure interface
 * base's): the scan starts a table only where a constructor stores the vptr, and an interface is never stored. The
 * second table starts with the header pair every table of this binary begins with (offset 0, RTTI 0): two null
 * words. The object splits there when the words before the pair equal a Gormiti table of exactly that length. */
typedef struct {
    CdWord *w, *w2; /* the first table's slots; the second's (offsets relative to its own start), NULL: none */
    size_t n, n2;
    u32 off2;       /* the second table's byte offset in the object */
} Tables;


static const Tables *tables_of(Ctx *x, const ClassesClass *c) {
    if (!x->tables) x->tables = avp_imap_new();
    bool created;
    AvpVal *v = avp_imap_put(x->tables, (u64)(uintptr_t)c, &created);
    if (!created) return v->p;
    Tables *t = AVP_NEW(x->a, Tables);
    t->w = raw_words(x, c, &t->n);
    size_t k = t->n ? cd_split_point(x->a, t->w, t->n, x->sv, x->nsv) : 0;
    if (k) {
        u32 start = t->w[k].offset; /* the second table's header pair */
        t->n2 = t->n - k - 2;
        t->w2 = AVP_NEWN(x->a, CdWord, t->n2 + 1);
        for (size_t i = 0; i < t->n2; i++) {
            t->w2[i] = t->w[k + 2 + i];
            t->w2[i].offset -= start;
        }
        t->off2 = start;
        t->n = k;
    }
    v->p = t;
    return t;
}

static u32 hexu(const char *s);
/* The C type a classes.json draft field is declared with (u8 -> unsigned char, f32 -> float, void* ...). */
static const char *draft_ctype(AvpArena *a, const ClassesField *f) {
    static const char *const map[][2] = {{"u8", "unsigned char"}, {"s8", "signed char"}, {"u16", "unsigned short"},
                                         {"s16", "short"},        {"u32", "unsigned int"}, {"s32", "int"},
                                         {"f32", "float"},        {"f64", "double"},       {"void*", "void *"}};
    for (size_t k = 0; k < sizeof map / sizeof *map; k++)
        if (strcmp(f->type, map[k][0]) == 0) return map[k][1];
    return avp_sprintf(a, "unsigned char /* %s */", f->type);
}

/* The retail library (libraries.tsv) of a class: that of the first function its vtable points to (the tables
 * themselves lie in .data, which libraries.tsv does not map). */
static const char *class_library(Ctx *x, const ClassesClass *rc) {
    for (size_t i = 0; rc->vtable && i < rc->vtable->n_words; i++) {
        i64 addr = 0;
        const ClassesWord *w = &rc->vtable->words[i];
        if (w->target && classes_hex(w->target_address, &addr)) return shsm_library_of(x->a, addr, x->repo);
    }
    return NULL;
}

/* A Gormiti table matched by slot names alone can belong to an unrelated game class of the same shape. A retail
 * table of a middleware library (libraries.tsv: not game/ or climax/) accepts only a Gormiti table whose slot
 * functions come from an archive (the map's library: fmodex.a, homebuttonLib.a); NULL when they agree, else why not. */
static const char *lib_disagrees(Ctx *x, const ClassesClass *rc, const CdSibVt *sv) {
    if (!rc->vtable || !x->sb) return NULL;
    const char *lib = class_library(x, rc);
    if (!lib || !strncmp(lib, "game/", 5) || !strncmp(lib, "climax/", 7) || !strcmp(lib, "unknown")) return NULL;
    if (!x->sib_lib) {
        x->sib_lib = avp_map_new();
        if (sibling_has_index(x->sb)) {
            SibFnVec g = sibling_load_sibling(x->a, x->sb);
            for (size_t i = 0; i < g.n; i++)
                if (g.v[i].name) avp_map_put(x->sib_lib, avp_str(g.v[i].name), NULL)->p = (void *)(g.v[i].lib ? g.v[i].lib : "");
            avp_vec_free(&g);
        }
    }
    for (size_t i = 0; i < sv->n; i++) {
        if (!sv->slots[i] || !strcmp(sv->slots[i], "__pure_virtual")) continue;
        AvpVal *v = avp_map_get(x->sib_lib, avp_str(sv->slots[i]));
        if (!v) continue;
        const char *sl = v->p;
        if (*sl) return NULL;
        return avp_sprintf(x->a, "its slot %s is game code (no library in the map), the retail table is %s's", sv->slots[i], lib);
    }
    return NULL;
}

/* The slots of the class's own table (the first one when its object holds two). */
static CdWord *retail_words(Ctx *x, const ClassesClass *c, size_t *n) {
    const Tables *t = tables_of(x, c);
    *n = t->n;
    return t->w;
}

static u32 hexu(const char *s) {
    i64 v = 0;
    return classes_hex(s, &v) ? (u32)v : 0;
}

/* ---- DWARF ---- */

static const SdDie *type_def(Ctx *x, const char *name, i64 want_size, size_t *ndistinct, const AvpStr *ns_first,
                             const char **passed) {
    if (ndistinct) *ndistinct = 0;
    if (!x->dw) return NULL;
    AvpVal *v = avp_map_get(x->types, avp_str(name));
    if (!v) return NULL;
    U32Vec *ix = v->p;
    const SdDie *first = NULL, *sized = NULL, *in_ns = NULL;
    u32 sizes[16];
    size_t ns_n = 0;
    for (size_t i = 0; i < ix->n; i++) {
        const SdDie *d = &x->dw->dies[ix->v[i]];
        if (!sd_is_aggregate(d->tag) || !(d->flags & SD_F_CHILDREN) || !d->byte_size) continue;
        if (!first) first = d;
        if (want_size > 0 && d->byte_size == (u32)want_size && !sized) sized = d;
        if (ns_first && ns_first->n && !in_ns) {
            const char *p = x->dw->cus[d->cu].name;
            for (size_t k = 0; p && p[k] && !in_ns; k++)
                if (_strnicmp(p + k, ns_first->p, ns_first->n) == 0) in_ns = d;
        }
        bool seen = false;
        for (size_t k = 0; k < ns_n; k++) seen |= sizes[k] == d->byte_size;
        if (!seen && ns_n < 16) sizes[ns_n++] = d->byte_size;
    }
    if (ndistinct) *ndistinct = ns_n;
    if (sized) return sized;
    /* several definitions of the bare name: the one compiled in a unit whose path names the namespace
     * (FMOD::EventGroup from ...\FMOD\EventSource.cpp, not the game's Audio::EventGroup from AiDebug.cpp) */
    if (in_ns && in_ns != first && ns_n > 1 && passed) *passed = x->dw->cus[first->cu].name;
    return in_ns && ns_n > 1 ? in_ns : first;
}

static const char *access_kw(u8 acc) { return acc == 2 ? "protected" : acc == 3 ? "private" : "public"; }

static const char *short_of(const char *q);
static const char *ns_of(AvpArena *a, const char *q);

/* The callback parameter lists the retail mangled symbols spell (cd_scan_callbacks), loaded on first use. */
static const char *callback_consts(Ctx *x, const char *decl) {
    if (!decl || !strstr(decl, "(*")) return NULL;
    if (!x->callbacks) {
        x->callbacks = avp_map_new();
        AvpMapIter it = {0};
        while (x->scopes && avp_map_next(x->scopes, &it)) {
            const char *name = avp_cz(x->a, it.key);
            if (!strstr(name, "PF")) continue;
            const char *d = shsm_demangle_or_name(x->a, name);
            if (d && strstr(d, "(*)(")) cd_scan_callbacks(x->a, x->callbacks, d);
        }
    }
    return cd_callback_consts(x->a, x->callbacks, decl);
}

/* The DWARF layout of a class given by its qualified name (MWCC's DWARF names it without the namespace). */
static CdLayout layout_of(Ctx *x, const char *qname, i64 want_size, const char **passed) {
    CdLayout L = {0};
    size_t nd = 0;
    AvpStr ns = avp_str(ns_of(x->a, qname));
    i64 sep = avp_str_find(ns, AVP_S("::"), 0, (i64)ns.n);
    if (sep > 0) ns.n = (size_t)sep;
    const SdDie *t = type_def(x, short_of(qname), want_size, &nd, &ns, passed);
    if (!t) return L;
    const SdInfo *d = x->dw;
    u32 i = sd_index(d, t);
    L.found = true;
    L.key = t->tag == SD_TAG_structure_type ? "struct" : t->tag == SD_TAG_union_type ? "union" : "class";
    L.size = t->byte_size;
    L.nlayouts = nd;
    AVP_VEC(CdBase) bases = {0};
    AVP_VEC(CdMember) mem = {0};
    for (u32 k = i + 1; k < t->next; k++) {
        const SdDie *m = &d->dies[k];
        if (m->parent != i) continue;
        if (m->tag == SD_TAG_inheritance) {
            CdBase b = {sd_type_name(x->a, d, m->type), access_kw(m->accessibility), m->member_loc,
                        sd_type_size(d, m->type), m->virtuality != 0};
            avp_vec_push(&bases, b);
        } else if (m->tag == SD_TAG_member) {
            const char *decl = sd_decl(x->a, d, m->type, m->name ? m->name : "");
            if (m->flags & SD_F_BITS) decl = avp_sprintf(x->a, "%s : %u", decl, m->bit_size);
            u32 size = (m->flags & SD_F_BITS) && (m->flags & SD_F_SIZE) ? m->byte_size : sd_type_size(d, m->type);
            CdMember mm = {decl, (m->flags & SD_F_MEMBER_LOC) ? m->member_loc : 0, size};
            avp_vec_push(&mem, mm);
        }
    }
    L.bases = bases.v;
    L.nbases = bases.n;
    L.m = mem.v;
    L.nm = mem.n;
    return L;
}

/* Whether [off, off+4) is covered by a base or member of L. */
static bool covered(const CdLayout *L, u32 off, u32 n) {
    for (size_t i = 0; i < L->nbases; i++)
        if (off < L->bases[i].off + L->bases[i].size && L->bases[i].off < off + n) return true;
    for (size_t i = 0; i < L->nm; i++) {
        u32 sz = L->m[i].size ? L->m[i].size : 1;
        if (off < L->m[i].off + sz && L->m[i].off < off + n) return true;
    }
    return false;
}

static CdSig sig_of(Ctx *x, u32 sib_addr) {
    CdSig s = {0};
    if (!x->dw || !sib_addr) return s;
    AvpVal *v = avp_imap_get(x->sub_at, sib_addr);
    if (!v) return s;
    const SdInfo *d = x->dw;
    u32 i = (u32)(v->i - 1);
    const SdDie *f = &d->dies[i];
    s.ret = f->type ? sd_type_name(x->a, d, f->type) : "void";
    AVP_VEC(const char *) pn = {0};
    for (u32 k = i + 1; k < f->next; k++) {
        const SdDie *p = &d->dies[k];
        if (p->parent != i || p->tag != SD_TAG_formal_parameter) continue;
        if (p->name && strcmp(p->name, "this") == 0) continue;
        avp_vec_push(&pn, p->name ? p->name : "");
    }
    s.pnames = pn.v;
    s.npn = pn.n;
    return s;
}

static u32 sib_addr_of(Ctx *x, const CdWord *w, const CdName *nm) {
    if (w->address) {
        AvpVal *p = avp_imap_get(x->peers, w->address);
        if (p) return ((SibPeer *)p->p)->address;
    }
    if (x->elf && nm->name) {
        const SibElfSym *y = sibling_elf_sym(x->elf, nm->name);
        if (y && y->type == 2) return y->address;
    }
    return 0;
}

/* ---- types the declaration depends on ---- */

static const char *const BUILTIN[] = {"void", "char", "short", "int", "long", "float", "double", "bool", "signed",
                                      "unsigned", "const", "volatile", "wchar_t", "class", "struct", "enum", NULL};

static bool is_builtin(AvpStr w) {
    for (int i = 0; BUILTIN[i]; i++)
        if (avp_str_eqz(w, BUILTIN[i])) return true;
    return false;
}

/* The include/ header that defines name, NULL when none does. */
static const char *g_defined_kw; /* the keyword of the last defined_in hit: "class", "struct", ..., "typedef" */
static const char *defined_in(Ctx *x, const char *name) {
    g_defined_kw = NULL;
    size_t n = strlen(name);
    for (size_t h = 0; h < x->htexts.n; h++) {
        AvpStr t = x->htexts.v[h];
        i64 at = 0;
        while ((at = avp_str_find(t, avp_str(name), at, (i64)t.n)) >= 0) {
            size_t s = (size_t)at, e = s + n;
            at = (i64)e;
            if ((s && (isalnum((unsigned char)t.p[s - 1]) || t.p[s - 1] == '_')) ||
                (e < t.n && (isalnum((unsigned char)t.p[e]) || t.p[e] == '_')))
                continue;
            size_t q = e;
            while (q < t.n && (t.p[q] == ' ' || t.p[q] == '\t')) q++;
            char nx = q < t.n ? t.p[q] : 0;
            size_t p = s;
            while (p && (t.p[p - 1] == ' ' || t.p[p - 1] == '\t')) p--;
            size_t ws = p;
            while (ws && (isalpha((unsigned char)t.p[ws - 1]))) ws--;
            AvpStr prev = avp_strn(t.p + ws, p - ws);
            bool tagged = avp_str_eqz(prev, "class") || avp_str_eqz(prev, "struct") || avp_str_eqz(prev, "enum") ||
                          avp_str_eqz(prev, "union");
            if (tagged && (nx == '{' || (nx == ':' && !(q + 1 < t.n && t.p[q + 1] == ':')) || nx == '\r' || nx == '\n'))
                return g_defined_kw = avp_str_eqz(prev, "class") ? "class" : "other", x->hpaths.v[h].p;
            if (nx == ';' && p && t.p[p - 1] == '}') return g_defined_kw = "other", x->hpaths.v[h].p;
            if (nx == ';') {
                size_t ls = s;
                while (ls && t.p[ls - 1] != '\n') ls--;
                while (ls < s && (t.p[ls] == ' ' || t.p[ls] == '\t')) ls++;
                if (avp_str_startswith(avp_strn(t.p + ls, s - ls), AVP_S("typedef"))) return g_defined_kw = "other", x->hpaths.v[h].p;
            }
        }
    }
    return NULL;
}

typedef struct {
    AvpMap *seen;      /* name -> 1 pointer only, 2 by value */
    AvpStrVec order;
} Deps;

static void deps_scan(AvpArena *a, Deps *dp, const char *text, const char *ns, const char *self) {
    AvpStr t = avp_str(text);
    for (size_t i = 0; i < t.n;) {
        if (!(isalpha((unsigned char)t.p[i]) || t.p[i] == '_')) {
            i++;
            continue;
        }
        size_t s = i;
        while (i < t.n && (isalnum((unsigned char)t.p[i]) || t.p[i] == '_' || (t.p[i] == ':' && i + 1 < t.n && t.p[i + 1] == ':')))
            i += t.p[i] == ':' ? 2 : 1;
        AvpStr w = avp_strn(t.p + s, i - s);
        /* a declarator name follows its type: after '*', '&' or a type word other than a qualifier */
        size_t bk = s;
        while (bk && t.p[bk - 1] == ' ') bk--;
        if (bk && (t.p[bk - 1] == '*' || t.p[bk - 1] == '&')) continue;
        if (bk && bk < s && (isalnum((unsigned char)t.p[bk - 1]) || t.p[bk - 1] == '_')) {
            size_t ws = bk;
            while (ws && (isalnum((unsigned char)t.p[ws - 1]) || t.p[ws - 1] == '_')) ws--;
            AvpStr pw = avp_strn(t.p + ws, bk - ws);
            if (!avp_str_eqz(pw, "const") && !avp_str_eqz(pw, "volatile") && !avp_str_eqz(pw, "unsigned") &&
                !avp_str_eqz(pw, "signed") && !avp_str_eqz(pw, "struct") && !avp_str_eqz(pw, "class") &&
                !avp_str_eqz(pw, "enum"))
                continue;
        }
        if (ns && *ns && avp_str_startswith(w, avp_str(ns)) && w.n > strlen(ns) + 2 && w.p[strlen(ns)] == ':')
            w = avp_strn(w.p + strlen(ns) + 2, w.n - strlen(ns) - 2);
        if (is_builtin(w) || avp_str_eqz(w, self) || (s && t.p[s - 1] == '/')) continue;
        /* a member or parameter name follows a type: only identifiers that start a declaration word count */
        size_t q = i;
        while (q < t.n && t.p[q] == ' ') q++;
        bool ptr = q < t.n && (t.p[q] == '*' || t.p[q] == '&');
        if (!ptr && q + 5 <= t.n && strncmp(t.p + q, "const", 5) == 0) {
            size_t r = q + 5;
            while (r < t.n && t.p[r] == ' ') r++;
            ptr = r < t.n && (t.p[r] == '*' || t.p[r] == '&');
        }
        bool created;
        AvpVal *v = avp_map_put(dp->seen, w, &created);
        if (created) avp_vec_push(&dp->order, avp_str(avp_cz(a, w)));
        if (!ptr || v->i == 2) v->i = 2;
        else v->i = 1;
    }
}

/* ---- one class ---- */

typedef struct {
    CdClass c;
    const ClassesClass *retail;
    const char *ns;
    AvpStrVec symbols; /* symbols.txt lines */
    AvpStrVec chain;   /* qualified ancestors, primary base first */
    double named_frac;
    bool sib_identified;
} Built;

static const char *qualify(AvpArena *a, const char *ns, const char *shortn) {
    return ns && *ns ? avp_sprintf(a, "%s::%s", ns, shortn) : shortn;
}

static const char *short_of(const char *q) {
    const char *s = q;
    for (const char *p = q; *p; p++)
        if (p[0] == ':' && p[1] == ':') s = p + 2;
    return s;
}

static const char *ns_of(AvpArena *a, const char *q) {
    const char *s = short_of(q);
    return s > q ? avp_cz(a, avp_strn(q, (size_t)(s - q - 2))) : "";
}

static const ClassesClass *retail_by_name(Ctx *x, const char *q) {
    const ClassesClass *c = classes_by_name(x->db, q);
    return c && c->vtable ? c : NULL;
}

/* The pure slots' names from the derived classes' overrides at the same offset. */
static void name_pure_from_derived(Ctx *x, const ClassesClass *c, const CdWord *w, CdName *names, size_t n) {
    if (!c) return;
    for (size_t i = 0; i < n; i++) {
        if (!names[i].pure || names[i].name) continue;
        for (size_t k = 0; k < c->derived.n && !names[i].name; k++) {
            const ClassesClass *dc = classes_by_key(x->db, c->derived.v[k]);
            if (!dc || !dc->vtable) continue;
            size_t dn = 0;
            CdWord *dw = retail_words(x, dc, &dn);
            for (size_t j = 0; j < dn; j++) {
                if (dw[j].offset != w[i].offset) continue;
                CdName one;
                cd_name_slots(x->a, &dw[j], 1, NULL, &one);
                if (one.name) {
                    names[i].name = one.name;
                    names[i].src = CD_SRC_DERIVED;
                }
            }
        }
    }
}

static void add_symbol(Built *b, AvpArena *a, const char *fmt_line) { avp_vec_push(&b->symbols, avp_str(avp_strdup(a, fmt_line))); }

static Built *build(Ctx *x, const ClassesClass *rc, const CdWord *forced, size_t nforced, const char *forced_name,
                    bool light) {
    AvpArena *a = x->a;
    Built *B = AVP_NEW(a, Built);
    B->retail = rc;
    CdClass *c = &B->c;
    size_t n = 0;
    CdWord *w = forced ? (CdWord *)forced : retail_words(x, rc, &n);
    if (forced) n = nforced;
    c->vtable_symbol = rc && rc->vtable ? rc->vtable->symbol : "(none: the base part of the derived table)";
    c->vtable_address = rc && rc->vtable ? hexu(rc->vtable->address) : 0;
    const Tables *tb = rc && rc->vtable && !forced ? tables_of(x, rc) : NULL;
    bool second = tb && tb->w2 && x->second == rc;
    if (second) { /* the interface table that follows the class's own in the same object */
        w = tb->w2;
        n = tb->n2;
        c->vtable_symbol = avp_sprintf(a, "%s+0x%X", rc->vtable->symbol, tb->off2);
        c->vtable_address += tb->off2;
    }
    if (tb && tb->w2) {
        CdVtMatch om = cd_match_vtable(a, second ? tb->w : tb->w2, second ? tb->n : tb->n2, x->sv, x->nsv, NULL);
        const char *other = om.index >= 0 ? avp_sprintf(a, "%s (= Gormiti %s)", x->sv[om.index].cls, x->sv[om.index].symbol)
                                          : "no Gormiti table equals it";
        avp_vec_push(&c->notes, avp_str(avp_sprintf(
                                    a, "TWO TABLES: retail object %s holds %s (%zu slots) and at +0x%X %s (%zu slots): %s;"
                                       " drafted here: the %s",
                                    rc->vtable->symbol, second ? "a table" : "this table", tb->n, tb->off2,
                                    second ? "this table" : "a second table", tb->n2, other, second ? "second" : "first")));
    }
    CdVtMatch m = {.index = -1};
    if (!forced) m = cd_match_vtable(a, w, n, x->sv, x->nsv, rc ? rc->name : NULL);
    const CdSibVt *sib = m.index >= 0 ? &x->sv[m.index] : NULL;
    const CdSibVt *tied0 = NULL; /* ambiguous: the first fitting table names the slots and the class, flagged */
    for (size_t k = 0; m.ntied && k < x->nsv && !tied0; k++)
        if (strcmp(x->sv[k].symbol, m.tied[0]) == 0) tied0 = &x->sv[k];
    if (!sib && tied0) sib = tied0;
    const char *why_not = sib && rc ? lib_disagrees(x, rc, sib) : NULL;
    if (why_not) {
        avp_vec_push(&c->notes, avp_str(avp_sprintf(a, "REJECTED: Gormiti %s (%s): %s", sib->symbol, sib->cls, why_not)));
        sib = tied0 = NULL;
        m.index = -1;
        m.ntied = 0;
    }
    size_t used = sib ? sib->n : n;
    CdName *names = AVP_NEWN(a, CdName, used + 1);
    cd_name_slots(a, w, used, sib, names);
    name_pure_from_derived(x, rc, w, names, used);
    /* ownership */
    const char *owner = NULL, *why = NULL;
    if (forced_name) {
        owner = forced_name;
        why = "the primary base of the class it was generated with (its slots are that table's prefix)";
    } else if (sib && sib == tied0) {
        owner = sib->cls;
        why = avp_sprintf(a, "AMBIGUOUS: %zu Gormiti tables have these %zu slots; the first, %s, is drafted (the "
                             "constructor or the derived classes must decide)", m.ntied, sib->n, sib->symbol);
    } else if (sib) {
        owner = sib->cls;
        why = avp_sprintf(a, "Gormiti %s has the same %zu slots (%zu of them compared)", sib->symbol, sib->n, m.compared);
        B->sib_identified = true;
    } else if (rc && rc->name_source && strstr(rc->name_source, "ctor")) {
        owner = rc->name;
        why = avp_sprintf(a, "the constructor's mangled class (classes.json %s)", rc->name_source);
    } else {
        for (size_t i = used; i-- > 0 && !owner;)
            if (names[i].name && names[i].src != CD_SRC_DERIVED) owner = cd_scope_of(a, names[i].name);
        why = owner ? "the class of its last named slot (no sibling vtable fits)" : NULL;
        if (!owner) {
            owner = rc && rc->name ? rc->name : "unknown";
            why = avp_sprintf(a, "the classes.json name (%s)", rc && rc->name_source ? rc->name_source : "none");
        }
    }
    if (m.ntied) {
        AvpBuf t;
        avp_buf_init(&t, a);
        for (size_t i = 0; i < m.ntied; i++) avp_buf_printf(&t, "%s%s", i ? ", " : "", m.tied[i]);
        avp_vec_push(&c->notes, avp_str(avp_sprintf(a, "VTABLE AMBIGUOUS: Gormiti tables with these slots: %s", avp_cz(a, avp_buf_str(&t)))));
    }
    if (rc && rc->name && strcmp(rc->name, owner) != 0 && !forced_name && !second)
        avp_vec_push(&c->notes, avp_str(avp_sprintf(a, "OWNER: classes.json calls this vtable %s; the evidence says %s", rc->name, owner)));
    c->cls = owner;
    c->owner_source = why;
    B->ns = ns_of(a, owner);
    const char *shortn = short_of(owner);

    /* layout and chain */
    i64 want = 0;
    if (rc && !second && rc->size) classes_hex(rc->size, &want);
    const char *passed = NULL;
    c->layout = layout_of(x, owner, want, &passed);
    for (size_t k = 0; k < c->layout.nm; k++) {
        const char *fixed = callback_consts(x, c->layout.m[k].decl);
        if (fixed) c->layout.m[k].decl = fixed;
    }
    if (passed)
        avp_vec_push(&c->notes, avp_str(avp_sprintf(a, "LAYOUT: the %s defined in a unit of its namespace is drafted; another class of the same bare name (%s) was passed over", shortn, passed)));
    if (c->layout.found && rc && rc->vtable && *B->ns && x->dw) {
        /* MWCC's DWARF names types without their namespace: a middleware class (nw4hbm::lyt::Window) finds a game
         * class of the same bare name when the sibling compiled that library without DWARF (no compile unit path
         * names the namespace) */
        const char *lib = class_library(x, rc);
        bool middleware = lib && strncmp(lib, "game/", 5) && strncmp(lib, "climax/", 7) && strcmp(lib, "unknown");
        AvpStr first = avp_str(B->ns);
        i64 sep = avp_str_find(first, AVP_S("::"), 0, (i64)first.n);
        if (sep > 0) first.n = (size_t)sep;
        bool has_cu = false;
        for (size_t k = 0; middleware && k < x->dw->ncus && !has_cu; k++) {
            const char *p = x->dw->cus[k].name;
            for (size_t i = 0; p && p[i] && !has_cu; i++) has_cu = _strnicmp(p + i, first.p, first.n) == 0;
        }
        if (middleware && !has_cu) {
            avp_vec_push(&c->notes, avp_str(avp_sprintf(a, "LAYOUT REJECTED: Gormiti's DWARF has no %.*s compile unit; its %s"
                                                           " (0x%X bytes) is another class of the same bare name",
                                                        AVP_SF(first), shortn, c->layout.size)));
            c->layout = (CdLayout){0};
        }
    }
    const char *cur = owner;
    CdLayout curl = c->layout;
    for (int depth = 0; depth < 8; depth++) {
        const char *base = NULL;
        if (curl.found && curl.nbases && curl.bases[0].off == 0) base = qualify(a, ns_of(a, cur), curl.bases[0].name);
        else if (!curl.found && depth == 0 && rc && rc->n_bases && rc->bases[0].primary) {
            const ClassesClass *bc = classes_by_key(x->db, rc->bases[0].key);
            base = bc && bc->name ? bc->name : NULL;
        }
        if (!base) break;
        avp_vec_push(&B->chain, avp_str(base));
        cur = base;
        curl = layout_of(x, base, 0, NULL);
    }
    /* base slot count and names */
    size_t nbase = 0;
    CdName *bnames = NULL;
    size_t nbn = 0;
    const char *bcls = B->chain.n ? B->chain.v[0].p : NULL;
    const SibVtable *bsv = bcls && x->elf ? sibling_elf_vtable(x->elf, bcls) : NULL;
    const ClassesClass *brc = bcls ? retail_by_name(x, bcls) : NULL;
    if (bsv) {
        nbase = bsv->n;
        bnames = AVP_NEWN(a, CdName, nbase + 1);
        for (size_t i = 0; i < nbase; i++) {
            bnames[i].pure = !bsv->slots[i] || strcmp(bsv->slots[i], "__pure_virtual") == 0;
            bnames[i].name = bnames[i].pure ? NULL : bsv->slots[i];
        }
        nbn = nbase;
    } else if (brc) {
        size_t bn = 0;
        CdWord *bw = retail_words(x, brc, &bn);
        while (bn && !bw[bn - 1].retail) bn--; /* a shape-only table's trailing nulls */
        bnames = AVP_NEWN(a, CdName, bn + 1);
        cd_name_slots(a, bw, bn, NULL, bnames);
        nbase = nbn = bn;
    } else if (B->chain.n) {
        const char **anc = AVP_NEWN(a, const char *, B->chain.n + 1);
        for (size_t i = 0; i < B->chain.n; i++) anc[i] = B->chain.v[i].p;
        nbase = cd_infer_base_count(a, names, used, anc, B->chain.n);
        if (nbase) avp_vec_push(&c->notes, avp_str(avp_sprintf(a, "base slot count %zu inferred from the slot names (no vtable of %s in either binary)", nbase, bcls)));
    }
    if (nbase > used) nbase = used;
    c->nbase = nbase;
    c->base_cls = bcls;
    int *kind = AVP_NEWN(a, int, used + 1);
    cd_classify(a, owner, names, used, nbase, bnames, nbn, kind);
    for (size_t i = nbase; i < used; i++) {
        const char *s = names[i].name && names[i].src != CD_SRC_DERIVED ? cd_scope_of(a, names[i].name) : NULL;
        if (s && strcmp(s, owner) != 0)
            for (size_t k = 0; k < B->chain.n; k++)
                if (strcmp(s, B->chain.v[k].p) == 0)
                    avp_vec_push(&c->notes, avp_str(avp_sprintf(a, "slot 0x%02X is new here but names %s, an ancestor: the base's slot count may be short", w[i].offset, s)));
    }
    c->w = w;
    c->names = names;
    c->kind = kind;
    c->n = used;
    c->sib_vtable = sib ? sib->symbol : NULL;
    size_t named = 0;
    for (size_t i = 0; i < used; i++) {
        c->count_src[names[i].src]++;
        if (names[i].name) named++;
    }
    c->count_src[CD_SRC_NONE] = 0;
    for (size_t i = 0; i < used; i++)
        if (!names[i].name) c->count_src[CD_SRC_NONE]++;
    B->named_frac = used ? (double)named / (double)used : 1.0;
    if (light) return B;

    /* signatures */
    CdSig *sigs = AVP_NEWN(a, CdSig, used + 1);
    for (size_t i = 0; i < used; i++)
        if (kind[i] != CD_KIND_INHERITED && names[i].name)
        {
            u32 sa = sib_addr_of(x, &w[i], &names[i]);
            sigs[i] = sig_of(x, sa);
            const char *fn = sa && x->elf ? sibling_elf_fn_at(x->elf, sa) : NULL;
            const SibElfSym *y = fn ? sibling_elf_sym(x->elf, fn) : NULL;
            sigs[i].weak = y && y->bind == 2 && names[i].src != CD_SRC_DERIVED;
        }
    c->sigs = sigs;

    /* who each slot belongs to, from the classes.json chain of bases (the sibling vtable decides the declaration; this
     * says where the slot comes from) */
    if (rc && rc->vtable && !forced && !second) {
        const char **notes = AVP_NEWN(a, const char *, used + 1);
        for (size_t i = 0; i < used; i++) {
            for (size_t k = 0; k < rc->vtable->n_words; k++) {
                const ClassesWord *cw = &rc->vtable->words[k];
                i64 off = 0;
                if (!cw->group || strcmp(cw->group, "complete") != 0 || !classes_hex(cw->offset, &off) || off != (i64)w[i].offset)
                    continue;
                ClassesSlotLabel lab;
                if (classes_slot_label(a, x->db, rc, cw, &lab)) notes[i] = classes_slot_label_text(a, cw, &lab);
                break;
            }
        }
        c->slot_note = notes;
    }

    /* vptr */
    bool base_poly = nbase > 0 || bsv || brc;
    if (!base_poly && bcls) {
        /* a base whose DWARF layout leaves an aligned word to the vtable pointer and that has no vtable in either
         * binary: an abstract interface (FMOD::MusicSystem), whose pure slots cannot be told from the derived ones */
        CdLayout bl = layout_of(x, bcls, 0, NULL);
        for (u32 o = 0; bl.found && o + 4 <= bl.size && !base_poly; o += 4) base_poly = !covered(&bl, o, 4);
        if (base_poly)
            avp_vec_push(&c->notes, avp_str(avp_sprintf(a, "BASE %s holds a vtable pointer (an uncovered word in its DWARF layout) but has no vtable in either binary: an abstract interface; which of the slots below are its pure virtuals (overridden here) is not decided", bcls)));
    }
    c->own_vptr = !base_poly;
    c->vptr_off = -1;
    if (c->own_vptr) {
        if (rc && rc->own_vptrs.n) c->vptr_off = hexu(rc->own_vptrs.v[0]);
        else if (c->layout.found)
            for (u32 o = 0; o + 4 <= c->layout.size; o += 4)
                if (!covered(&c->layout, o, 4)) {
                    c->vptr_off = o;
                    break;
                }
    }
    /* layout checks */
    if (rc && !second && rc->size) c->retail_size = avp_sprintf(a, "%s (%s)", rc->size, rc->size_source ? rc->size_source : "exact");
    else if (rc && !second && rc->has_size_estimate) c->retail_size = avp_sprintf(a, "~0x%llX (estimate from accesses)", (long long)rc->size_estimate);
    if (c->layout.found && rc && !second) {
        CdLayout *L = &c->layout;
        if (want && (u32)want != L->size)
            avp_vec_push(&c->notes, avp_str(avp_sprintf(a, "LAYOUT DIFFERS: retail size %s, Gormiti 0x%X", rc->size, L->size)));
        if (!want && rc->has_size_estimate && rc->size_estimate > (i64)L->size)
            avp_vec_push(&c->notes, avp_str(avp_sprintf(a, "LAYOUT DIFFERS: retail accesses reach 0x%llX, Gormiti size 0x%X", (long long)rc->size_estimate, L->size)));
        if (c->own_vptr && c->vptr_off >= 0 && covered(L, (u32)c->vptr_off, 4))
            avp_vec_push(&c->notes, avp_str(avp_sprintf(a, "LAYOUT DIFFERS: retail vptr at 0x%X is a Gormiti member", (u32)c->vptr_off)));
        size_t shown = 0;
        for (size_t i = 0; i < rc->n_fields && shown < 6; i++) {
            const ClassesField *f = &rc->fields[i];
            if (f->inherited_from || (f->type && strcmp(f->type, "vptr") == 0)) continue;
            u32 off = hexu(f->offset);
            if ((i64)off == c->vptr_off) continue;
            bool own_vptr_word = false;
            for (size_t k = 0; k < rc->own_vptrs.n; k++) own_vptr_word |= hexu(rc->own_vptrs.v[k]) == off;
            if (own_vptr_word) continue;
            if (off >= L->size) {
                avp_vec_push(&c->notes, avp_str(avp_sprintf(a, "LAYOUT DIFFERS: retail accesses 0x%X (%s), past Gormiti's size 0x%X", off, f->type, L->size)));
                shown++;
            } else if (!covered(L, off, (u32)(f->width > 0 ? 1 : 1))) {
                bool tail_vptr = c->own_vptr || base_poly;
                if (!(tail_vptr && off + 4 <= L->size && off % 4 == 0 && L->size - off <= 4)) {
                    avp_vec_push(&c->notes, avp_str(avp_sprintf(a, "LAYOUT DIFFERS: retail accesses 0x%X (%s), a gap in the Gormiti layout", off, f->type)));
                    shown++;
                }
            }
        }
    }
    /* no Gormiti type: the fields retail reads (classes.json drafts them from the accesses) are all there is */
    if (!c->layout.found && rc && !second) {
        c->drafted = avp_alloc(a, (rc->n_fields + 1) * sizeof *c->drafted);
        for (size_t i = 0; i < rc->n_fields; i++) {
            const ClassesField *f = &rc->fields[i];
            if (f->inherited_from || !f->type || strcmp(f->type, "vptr") == 0) continue;
            u32 off = hexu(f->offset);
            bool is_vptr_word = (i64)off == c->vptr_off;
            for (size_t k = 0; k < rc->own_vptrs.n; k++) is_vptr_word |= hexu(rc->own_vptrs.v[k]) == off;
            if (is_vptr_word) continue;
            CdMember dm = {0};
            dm.off = off;
            dm.size = (u32)(f->width > 0 ? f->width : 1);
            dm.decl = avp_sprintf(a, "%s field_0x%X", draft_ctype(a, f), off);
            dm.note = avp_sprintf(a, "0x%02X, %s: retail %lld load%s, %lld store%s", off, f->type, (long long)f->loads,
                                 f->loads == 1 ? "" : "s", (long long)f->stores, f->stores == 1 ? "" : "s");
            c->drafted[c->ndrafted++] = dm;
        }
    }

    /* dependencies */
    Deps dp = {avp_map_new(), {0}};
    AvpBuf all;
    avp_buf_init(&all, a);
    for (size_t i = 0; i < used; i++) {
        if (kind[i] == CD_KIND_INHERITED || !names[i].name) continue;
        const char *d = cd_method_decl(a, shortn, B->ns, names[i].name, &sigs[i], false, "x", "int");
        const char *lp = strchr(d, '(');
        const char *head = strstr(d, "virtual ") ? d + 8 : d;
        if (sigs[i].ret) avp_buf_printf(&all, "%s r;\n", sigs[i].ret);
        if (lp) avp_buf_printf(&all, "%s\n", lp);
        (void)head;
    }
    for (size_t k = 0; k < c->layout.nm; k++) avp_buf_printf(&all, "%s;\n", c->layout.m[k].decl);
    for (size_t k = 0; k < c->layout.nbases; k++) avp_buf_printf(&all, "%s b;\n", c->layout.bases[k].name);
    if (!c->layout.found && bcls) avp_buf_printf(&all, "%s b;\n", short_of(bcls));
    deps_scan(a, &dp, avp_cz(a, avp_buf_str(&all)), B->ns, shortn);
    for (size_t i = 0; i < dp.order.n; i++) {
        const char *nm = dp.order.v[i].p;
        i64 how = avp_map_get(dp.seen, avp_str(nm))->i;
        const SdDie *t = NULL;
        AvpVal *tv = x->dw ? avp_map_get(x->types, avp_str(nm)) : NULL;
        if (tv) t = &x->dw->dies[((U32Vec *)tv->p)->v[0]];
        /* only C++ classes are forward-declared in the class's namespace; C API structs (FMOD_VECTOR) live at
         * file scope in the headers that define them */
        const char *where = defined_in(x, nm);
        const char *kw = g_defined_kw;
        bool param_name = !t && !where && islower((unsigned char)nm[0]);
        if (param_name) continue; /* a parameter or member name, not a type */
        /* MWCC tags C structs as classes too, so an all-capitals name (FMOD_OS_SEMAPHORE) counts as a C API type */
        bool c_api = true;
        for (const char *q = nm; *q; q++) c_api &= !islower((unsigned char)*q);
        bool fwd = how == 1 && (where ? strcmp(kw, "class") == 0
                                      : !c_api && (!t || t->tag == SD_TAG_class_type));
        if (fwd) {
            avp_vec_push(&c->fwd, avp_str(nm));
            continue;
        }
        const char *what = t ? (t->tag == SD_TAG_enumeration_type ? "enum" : t->tag == SD_TAG_typedef ? "typedef" : t->tag == SD_TAG_base_type ? "type" : "class") : "type";
        avp_vec_push(&c->needs, avp_str(avp_sprintf(a, "%s %s (%s)", what, nm, where ? where : "not in include/")));
    }

    /* symbols.txt */
    for (size_t i = 0; i < used; i++) {
        if (!w[i].retail || !names[i].name) continue;
        if (names[i].src == CD_SRC_PAIR || names[i].src == CD_SRC_SIBVT)
            if (strcmp(w[i].retail, names[i].name) != 0)
                add_symbol(B, a, avp_sprintf(a, "rename %s -> %s (slot 0x%02X, %s)", w[i].retail, names[i].name, w[i].offset, CD_SRC_NAMES[names[i].src]));
        const char *sibname = names[i].src == CD_SRC_RETAIL && w[i].pair ? w[i].pair : names[i].name;
        const SibElfSym *y = x->elf ? sibling_elf_sym(x->elf, sibname) : NULL;
        AvpVal *cur_scope = avp_map_get(x->scopes, avp_str(w[i].retail));
        const char *rs = cur_scope ? cur_scope->p : NULL;
        if (y && y->bind == 2 && (!rs || strcmp(rs, "weak") != 0) && kind[i] != CD_KIND_INHERITED)
        {
            const char *paste = attr_command(x, w[i].retail, "weak");
            add_symbol(B, a, avp_sprintf(a, "scope:weak %s (Gormiti binds %s weak; symbols.txt has %s)%s%s", w[i].retail, sibname,
                                         rs ? rs : "no entry", paste ? "; run: " : "", paste ? paste : ""));
        }
    }
    if (rc && rc->vtable && sib && strcmp(rc->vtable->symbol, sib->symbol) != 0) {
        AvpVal *cur_scope = avp_map_get(x->scopes, avp_str(rc->vtable->symbol));
        add_symbol(B, a, avp_sprintf(a, "rename %s -> %s scope:%s (vtable; symbols.txt has %s)", rc->vtable->symbol, sib->symbol, sib->weak ? "weak" : "global", cur_scope ? (const char *)cur_scope->p : "no entry"));
    } else if (rc && rc->vtable && sib) {
        AvpVal *cur_scope = avp_map_get(x->scopes, avp_str(rc->vtable->symbol));
        const char *want_scope = sib->weak ? "weak" : "global";
        if (cur_scope && strcmp(cur_scope->p, want_scope) != 0) {
            const char *paste = attr_command(x, rc->vtable->symbol, want_scope);
            add_symbol(B, a, avp_sprintf(a, "scope:%s %s (vtable; symbols.txt has %s)%s%s", want_scope, rc->vtable->symbol,
                                         (const char *)cur_scope->p, paste ? "; run: " : "", paste ? paste : ""));
        }
    }
    return B;
}

/* The retail class for a query: classes.json's lookup, else the retail table that equals the named Gormiti one. */
static const ClassesClass *find_class(Ctx *x, const char *q) {
    /* a Gormiti class first: classes.json may give its name to another table (a wrong attribution) */
    const SibVtable *v = x->elf ? sibling_elf_vtable(x->elf, q) : NULL;
    if (!v) return classes_find(x->db, q);
    CdSibVt one = {v->symbol, v->cls, v->slots, v->n, v->weak};
    for (size_t i = 0; i < x->db->n; i++) {
        const ClassesClass *rc = &x->db->classes[i];
        if (!rc->vtable) continue;
        size_t n = 0;
        CdWord *w = retail_words(x, rc, &n);
        if (cd_match_vtable(x->a, w, n, &one, 1, NULL).index == 0 && !lib_disagrees(x, rc, &one)) return rc;
    }
    for (size_t i = 0; i < x->db->n; i++) { /* the second table of an object that holds two */
        const ClassesClass *rc = &x->db->classes[i];
        if (!rc->vtable) continue;
        const Tables *t = tables_of(x, rc);
        if (t->w2 && cd_match_vtable(x->a, t->w2, t->n2, &one, 1, NULL).index == 0) {
            x->second = rc;
            return rc;
        }
    }
    const ClassesClass *c = classes_find(x->db, q);
    if (c && c->vtable) return c;
    /* no table equals it slot for slot (another revision): the retail table whose slots pair with most of the
     * Gormiti table's functions, at the same index, at least half of them */
    const ClassesClass *best = NULL;
    size_t best_hits = 0, named = 0;
    for (size_t k = 0; k < v->n; k++) named += v->slots[k] && strcmp(v->slots[k], "__pure_virtual") != 0;
    for (size_t i = 0; i < x->db->n; i++) {
        const ClassesClass *rc = &x->db->classes[i];
        if (!rc->vtable) continue;
        size_t n = 0, hits = 0;
        CdWord *w = retail_words(x, rc, &n);
        for (size_t k = 0; k < n && k < v->n; k++)
            hits += w[k].pair && v->slots[k] && strcmp(w[k].pair, v->slots[k]) == 0;
        if (hits > best_hits) best = rc, best_hits = hits;
    }
    if (best && named && 2 * best_hits >= named) {
        fprintf(stderr, "classes.exe: %s: no retail table equals Gormiti %s; the table whose slots pair with %zu of its"
                        " %zu named slots is drafted\n", q, v->symbol, best_hits, named);
        return best;
    }
    return c;
}

/* A query name as the sibling's tables and classes.json spell it: as given when either knows it, else the unique
 * qualified name whose last component it is. Several: the candidates are listed and the query fails. */
static const char *resolve_name(Ctx *x, const char *q) {
    if ((x->elf && sibling_elf_vtable(x->elf, q)) || classes_find(x->db, q)) return q;
    if (strstr(q, "::")) return q;
    size_t nn = (x->elf ? x->elf->nvt : 0) + x->db->n, k = 0;
    const char **names = AVP_NEWN(x->a, const char *, nn + 1);
    for (size_t i = 0; x->elf && i < x->elf->nvt; i++) names[k++] = x->elf->vt[i].cls;
    for (size_t i = 0; i < x->db->n; i++) names[k++] = x->db->classes[i].name;
    AvpStrVec cands = cd_short_matches(x->a, q, names, k);
    if (cands.n == 1) {
        const char *r = avp_cz(x->a, cands.v[0]);
        fprintf(stderr, "classes.exe: %s resolves to %s\n", q, r);
        avp_vec_free(&cands);
        return r;
    }
    if (cands.n > 1) {
        AvpBuf b;
        avp_buf_init(&b, x->a);
        for (size_t i = 0; i < cands.n; i++) avp_buf_printf(&b, "%s%.*s", i ? ", " : "", AVP_SF(cands.v[i]));
        shsm_raise(SHSM_EXC_USAGE, "%s is ambiguous: %s (pass the qualified name)", q, avp_buf_cz(&b));
    }
    avp_vec_free(&cands);
    return q;
}

/* ---- output ---- */

static const char *guard_of(AvpArena *a, const char *path) {
    AvpStr b = avp_path_basename(path);
    AvpBuf g;
    avp_buf_init(&g, a);
    avp_buf_putc(&g, '_');
    for (size_t i = 0; i < b.n; i++)
        avp_buf_putc(&g, isalnum((unsigned char)b.p[i]) ? (char)toupper((unsigned char)b.p[i]) : '_');
    return avp_cz(a, avp_buf_str(&g));
}

static AvpJson *class_json(AvpArena *a, const Built *B, const char *text) {
    const CdClass *c = &B->c;
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("class"), avp_json_strz(a, c->cls));
    avp_json_set(a, o, AVP_S("vtable"), avp_json_strz(a, c->vtable_symbol));
    avp_json_set(a, o, AVP_S("sibling_vtable"), c->sib_vtable ? avp_json_strz(a, c->sib_vtable) : avp_json_null());
    avp_json_set(a, o, AVP_S("owner_source"), avp_json_strz(a, c->owner_source));
    avp_json_set(a, o, AVP_S("base"), c->base_cls ? avp_json_strz(a, c->base_cls) : avp_json_null());
    avp_json_set(a, o, AVP_S("base_slots"), avp_json_int(a, (i64)c->nbase));
    avp_json_set(a, o, AVP_S("layout"), avp_json_bool(c->layout.found));
    AvpJson *slots = avp_json_arr(a);
    static const char *const KIND[] = {"", "inherited", "override", "new"};
    for (size_t i = 0; i < c->n; i++) {
        AvpJson *s = avp_json_obj(a);
        avp_json_set(a, s, AVP_S("offset"), avp_json_strz(a, avp_sprintf(a, "0x%X", c->w[i].offset)));
        avp_json_set(a, s, AVP_S("retail"), c->w[i].retail ? avp_json_strz(a, c->w[i].retail) : avp_json_null());
        avp_json_set(a, s, AVP_S("name"), c->names[i].name ? avp_json_strz(a, c->names[i].name) : avp_json_null());
        avp_json_set(a, s, AVP_S("source"), avp_json_strz(a, c->names[i].name ? CD_SRC_NAMES[c->names[i].src] : (c->names[i].pure ? "pure" : "placeholder")));
        avp_json_set(a, s, AVP_S("kind"), avp_json_strz(a, KIND[c->kind[i]]));
        avp_json_arr_push(a, slots, s);
    }
    avp_json_set(a, o, AVP_S("slots"), slots);
    AvpJson *notes = avp_json_arr(a);
    for (size_t i = 0; i < c->notes.n; i++) avp_json_arr_push(a, notes, avp_json_str(a, c->notes.v[i]));
    avp_json_set(a, o, AVP_S("notes"), notes);
    AvpJson *sy = avp_json_arr(a);
    for (size_t i = 0; i < B->symbols.n; i++) avp_json_arr_push(a, sy, avp_json_str(a, B->symbols.v[i]));
    avp_json_set(a, o, AVP_S("symbols"), sy);
    if (text) avp_json_set(a, o, AVP_S("text"), avp_json_strz(a, text));
    return o;
}

static void print_json(AvpArena *a, const AvpJson *v) {
    AvpStr s = avp_json_dumps(a, v, &AVP_JSON_INDENT(1));
    fwrite(s.p, 1, s.n, stdout);
    fputc('\n', stdout);
}

static const char *group_of(const char *lib) {
    if (!lib) return "other";
    if (strcmp(lib, "fmod") == 0) return "fmod";
    if (strncmp(lib, "climax/", 7) == 0) return "climax";
    if (strncmp(lib, "game/", 5) == 0) return "game";
    return "other";
}

static int run_stats(Ctx *x, const CdArgs *args) {
    AvpArena *a = x->a;
    static const char *const G[] = {"fmod", "climax", "game", "other"};
    i64 total[4] = {0}, named90[4] = {0}, layout[4] = {0}, sibvt[4] = {0}, slots[4] = {0}, named_slots[4] = {0};
    for (size_t i = 0; i < x->db->n; i++) {
        const ClassesClass *rc = &x->db->classes[i];
        if (!rc->vtable) continue;
        Built *B = build(x, rc, NULL, 0, NULL, true);
        if (!B->c.n) continue;
        i64 addr = 0;
        for (size_t k = 0; k < B->c.n && !addr; k++) addr = B->c.w[k].address;
        if (rc->n_ctors) classes_hex(rc->ctors[0].address, &addr);
        const char *lib = addr && x->libs ? shsm_libraries_lookup(a, x->libs, addr) : NULL;
        const char *g = group_of(lib);
        int gi = 0;
        while (strcmp(G[gi], g) != 0) gi++;
        total[gi]++;
        if (B->named_frac >= 0.9) named90[gi]++;
        if (B->c.layout.found) layout[gi]++;
        if (B->sib_identified) sibvt[gi]++;
        slots[gi] += (i64)B->c.n;
        named_slots[gi] += (i64)(B->c.n - B->c.count_src[CD_SRC_NONE]);
    }
    if (args->json) {
        AvpJson *o = avp_json_obj(a);
        for (int gi = 0; gi < 4; gi++) {
            AvpJson *r = avp_json_obj(a);
            avp_json_set(a, r, AVP_S("classes"), avp_json_int(a, total[gi]));
            avp_json_set(a, r, AVP_S("named_90"), avp_json_int(a, named90[gi]));
            avp_json_set(a, r, AVP_S("sibling_vtable"), avp_json_int(a, sibvt[gi]));
            avp_json_set(a, r, AVP_S("layout"), avp_json_int(a, layout[gi]));
            avp_json_set(a, r, AVP_S("slots"), avp_json_int(a, slots[gi]));
            avp_json_set(a, r, AVP_S("named_slots"), avp_json_int(a, named_slots[gi]));
            avp_json_set(a, o, avp_str(G[gi]), r);
        }
        print_json(a, o);
        return 0;
    }
    printf("polymorphic classes (retail vtables) by library group: slots named >= 90%%, vtable identified as a "
           "Gormiti __vt__, member layout in the Gormiti DWARF\n");
    printf("%-8s %8s %10s %10s %8s %14s\n", "group", "classes", "named>=90%", "sibling-vt", "layout", "slots named");
    for (int gi = 0; gi < 4; gi++)
        printf("%-8s %8lld %10lld %10lld %8lld %7lld/%-6lld\n", G[gi], (long long)total[gi], (long long)named90[gi],
               (long long)sibvt[gi], (long long)layout[gi], (long long)named_slots[gi], (long long)slots[gi]);
    return 0;
}

int classes_decl_run(AvpArena *a, const ClassesDb *db, const CdArgs *args) {
    Ctx x = {0};
    x.a = a;
    x.db = db;
    x.root = args->root;
    x.repo = shsm_repo_find(args->root);
    load_sibling(&x);
    if (args->stats) {
        AvpTry t;
        if (AVP_TRY(&t)) {
            x.libs = shsm_libraries_load(a, x.repo);
            avp_try_end(&t);
        } else {
            x.libs = NULL;
        }
        return run_stats(&x, args);
    }
    load_headers(&x);
    load_scopes(&x, args->symbols);
    const char *q = resolve_name(&x, args->cls);
    const ClassesClass *rc = find_class(&x, q);
    if (!rc && avp_map_has(x.types, avp_str(short_of(q))))
        shsm_raise(SHSM_EXC_USAGE, "%s: no vtable (not polymorphic): use `sibling.exe types %s`", q, short_of(q));
    if (!rc) shsm_raise(SHSM_EXC_USAGE, "class not found: %s (neither classes.json nor a Gormiti __vt__ names it)", q);
    if (!rc->vtable)
        shsm_raise(SHSM_EXC_USAGE, "%s has no retail vtable (not polymorphic, or its table was not found): its "
                                   "layout is `sibling.exe types %s`", rc->name ? rc->name : q, short_of(q));
    Built *main_b = build(&x, rc, NULL, 0, NULL, false);
    AVP_VEC(Built *) out = {0};
    if (args->with_bases) {
        /* each base not yet defined in include/, base first */
        const CdWord *w = main_b->c.w;
        for (size_t i = main_b->chain.n; i-- > 0;) {
            const char *bq = main_b->chain.v[i].p;
            if (defined_in(&x, short_of(bq))) continue;
            const ClassesClass *brc = retail_by_name(&x, bq);
            if (!brc) brc = x.elf && sibling_elf_vtable(x.elf, bq) ? find_class(&x, bq) : NULL;
            Built *bb = NULL;
            if (brc) {
                bb = build(&x, brc, NULL, 0, NULL, false);
            } else if (i == 0 && main_b->c.nbase) {
                bb = build(&x, NULL, w, main_b->c.nbase, bq, false);
            } else {
                continue; /* a base with no slots of its own: not polymorphic, its layout is `sibling types` */
            }
            avp_vec_push(&out, bb);
        }
    }
    avp_vec_push(&out, main_b);
    /* a base drafted in the same output is no outside need */
    for (size_t i = 0; i < out.n; i++) {
        AvpStrVec *nd = &out.v[i]->c.needs;
        size_t keep = 0;
        for (size_t k = 0; k < nd->n; k++) {
            bool mine = false;
            for (size_t j = 0; j < out.n; j++)
                mine |= avp_str_startswith(nd->v[k], avp_str(avp_sprintf(a, "class %s (", short_of(out.v[j]->c.cls))));
            if (!mine) nd->v[keep++] = nd->v[k];
        }
        nd->n = keep;
    }
    AvpBuf text;
    avp_buf_init(&text, a);
    for (size_t i = 0; i < out.n; i++) {
        if (i) avp_buf_putc(&text, '\n');
        cd_render(a, &out.v[i]->c, &text);
    }
    if (args->header) {
        if (avp_exists(args->header) && !args->force)
            shsm_raise(SHSM_EXC_USAGE, "%s exists (pass --force to overwrite)", args->header);
        const char *g = guard_of(a, args->header);
        AvpBuf h;
        avp_buf_init(&h, a);
        avp_buf_printf(&h, "/*\n * %s: class declaration draft (classes.exe decl %s); check every unconfirmed line.\n */\n",
                       main_b->c.cls, args->cls);
        avp_buf_printf(&h, "#ifndef %s\n#define %s\n\n", g, g);
        /* the headers that already declare what the classes need (the `needs:` lines that name one) */
        AvpMap *inc = avp_map_new();
        for (size_t i = 0; i < out.n; i++)
            for (size_t k = 0; k < out.v[i]->c.needs.n; k++) {
                AvpStr s = out.v[i]->c.needs.v[k];
                i64 lp = avp_str_find(s, AVP_S("(include/"), 0, (i64)s.n);
                if (lp < 0 || s.p[s.n - 1] != ')') continue;
                AvpStr path = avp_strn(s.p + lp + 9, s.n - (size_t)lp - 10);
                bool created;
                avp_map_put(inc, path, &created);
                if (created) avp_buf_printf(&h, "#include <%.*s>\n", AVP_SF(path));
            }
        if (avp_map_len(inc)) avp_buf_putc(&h, '\n');
        avp_buf_put(&h, avp_buf_str(&text));
        avp_buf_printf(&h, "\n#endif\n");
        AvpStr body = avp_buf_str(&h);
        if (!avp_write_file(args->header, body.p, body.n)) shsm_raise_io();
    }
    if (args->json) {
        AvpJson *o = avp_json_obj(a);
        AvpJson *cl = avp_json_arr(a);
        for (size_t i = 0; i < out.n; i++) avp_json_arr_push(a, cl, class_json(a, out.v[i], NULL));
        avp_json_set(a, o, AVP_S("classes"), cl);
        avp_json_set(a, o, AVP_S("text"), avp_json_str(a, avp_buf_str(&text)));
        if (args->header) avp_json_set(a, o, AVP_S("header"), avp_json_strz(a, args->header));
        print_json(a, o);
        return 0;
    }
    if (!args->header) {
        AvpStr s = avp_buf_str(&text);
        fwrite(s.p, 1, s.n, stdout);
    } else {
        printf("wrote %s\n", args->header);
    }
    size_t total = 0;
    for (size_t i = 0; i < out.n; i++) total += out.v[i]->symbols.n;
    if (total) {
        printf("\nsymbols.txt changes implied (not applied):\n");
        size_t shown = 0;
        for (size_t i = 0; i < out.n; i++)
            for (size_t k = 0; k < out.v[i]->symbols.n; k++) {
                if (!args->all && args->limit > 0 && (i64)shown >= args->limit) continue;
                printf("  %.*s\n", AVP_SF(out.v[i]->symbols.v[k]));
                shown++;
            }
        if (shown < total) printf("  ... %zu more (--all)\n", total - shown);
    }
    return 0;
}
