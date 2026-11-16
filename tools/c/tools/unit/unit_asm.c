/* unit: the dtk asm block parser and index (tools/unit.py lines 381-586: _scan_asm_names, _parse_asm, AsmIndex).
 * Hand-written scanners for HDR_RE, DECL_RE, TOK_RE, SUF_RE and BASE_REG_RE (the listings are 60 MB). */
#include "unit.h"

#include "shsm_asm.h"

#include <stdlib.h>
#include <string.h>

typedef AVP_VEC(const char *) CzVec;

static bool is_space(char c) { return c == ' ' || (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F); }
static bool is_word(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}
static bool is_hex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
static bool starts(AvpStr s, const char *p) {
    size_t n = strlen(p);
    return s.n >= n && !memcmp(s.p, p, n);
}

/* `^\.(fn|obj) ("[^"]+"|\S+?),\s*(\w+)?` on a line that starts with ".fn " or ".obj ": the name (unquoted) and
 * the scope word (empty when absent). */
static bool decl_match(AvpStr line, bool *is_fn, AvpStr *name, AvpStr *scope) {
    size_t i;
    if (starts(line, ".fn ")) {
        *is_fn = true;
        i = 4;
    } else if (starts(line, ".obj ")) {
        *is_fn = false;
        i = 5;
    } else {
        return false;
    }
    size_t after = 0;
    bool ok = false;
    if (i < line.n && line.p[i] == '"') {
        size_t j = i + 1;
        while (j < line.n && line.p[j] != '"') j++;
        if (j < line.n && j > i + 1 && j + 1 < line.n && line.p[j + 1] == ',') {
            *name = avp_strn(line.p + i + 1, j - i - 1);
            after = j + 2;
            ok = true;
        }
    }
    if (!ok) {
        size_t j = i;
        while (j < line.n && !is_space(line.p[j]) && line.p[j] != ',') j++;
        if (j == i || j >= line.n || line.p[j] != ',') return false;
        AvpStr nm = avp_strn(line.p + i, j - i);
        if (nm.p[0] == '"') nm = nm.n >= 2 ? avp_strn(nm.p + 1, nm.n - 2) : avp_strn(nm.p, 0);
        *name = nm;
        after = j + 1;
    }
    size_t k = after;
    while (k < line.n && is_space(line.p[k])) k++;
    size_t w = k;
    while (w < line.n && is_word(line.p[w])) w++;
    *scope = avp_strn(line.p + k, w - k);
    return true;
}

/* HDR_RE: `^# (\.[\w$]+):0x[0-9A-Fa-f]+ \| 0x([0-9A-Fa-f]{8}) \| size: 0x([0-9A-Fa-f]+)` */
static bool hdr_match(AvpStr line, AvpStr *sec, i64 *addr, i64 *size) {
    if (!starts(line, "# .")) return false;
    size_t i = 3;
    while (i < line.n && (is_word(line.p[i]) || line.p[i] == '$')) i++;
    if (i == 3) return false;
    *sec = avp_strn(line.p + 2, i - 2);
    AvpStr r = avp_strn(line.p + i, line.n - i);
    if (!starts(r, ":0x")) return false;
    size_t j = 3;
    while (j < r.n && is_hex(r.p[j])) j++;
    if (j == 3) return false;
    r = avp_strn(r.p + j, r.n - j);
    if (!starts(r, " | 0x") || r.n < 13) return false;
    for (size_t k = 5; k < 13; k++)
        if (!is_hex(r.p[k])) return false;
    u64 v = 0;
    avp_parse_uint(avp_strn(r.p + 5, 8), 16, &v);
    *addr = (i64)v;
    r = avp_strn(r.p + 13, r.n - 13);
    if (!starts(r, " | size: 0x")) return false;
    j = 11;
    while (j < r.n && is_hex(r.p[j])) j++;
    if (j == 11) return false;
    avp_parse_uint(avp_strn(r.p + 11, j - 11), 16, &v);
    *size = (i64)v;
    return true;
}

static const char *const SUFFIXES[] = {"l", "ha", "h", "sda21", "sda2", "sdarel", "sda"};

typedef struct {
    AvpArena *a;
    AvpMap *names; /* name -> interned cz */
    UnitBlockVec *blocks;
} Ctx;

static const char *lookup(Ctx *c, AvpStr t) {
    AvpVal *v = avp_map_get(c->names, t);
    return v ? v->p : NULL;
}

/* TOK_RE findall over ops, keeping the names of the index (SUF_RE stripped when the bare token is unknown). */
static void find_names(Ctx *c, AvpStr ops, CzVec *found) {
    found->n = 0;
    size_t i = 0;
    while (i < ops.n) {
        char ch = ops.p[i];
        AvpStr t = {0};
        if (ch == '"') {
            size_t j = i + 1;
            while (j < ops.n && ops.p[j] != '"') j++;
            if (j < ops.n && j > i + 1) {
                t = avp_strn(ops.p + i + 1, j - i - 1);
                i = j + 1;
            } else {
                i++;
                continue;
            }
        } else if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_' || ch == '@' || ch == '$') {
            size_t j = i + 1;
            while (j < ops.n && (is_word(ops.p[j]) || ops.p[j] == '@' || ops.p[j] == '$' || ops.p[j] == '\\' ||
                                 ops.p[j] == '.'))
                j++;
            t = avp_strn(ops.p + i, j - i);
            i = j;
        } else {
            i++;
            continue;
        }
        const char *n = lookup(c, t);
        if (!n) {
            for (size_t k = t.n; k-- > 0;)
                if (t.p[k] == '@') {
                    AvpStr suf = avp_strn(t.p + k + 1, t.n - k - 1);
                    for (size_t s = 0; s < sizeof SUFFIXES / sizeof *SUFFIXES; s++)
                        if (avp_str_eqz(suf, SUFFIXES[s])) n = lookup(c, avp_strn(t.p, k));
                    break;
                }
        }
        if (n) avp_vec_push(found, n);
    }
}

static int cmp_cz(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return strcmp(*(const char *const *)x, *(const char *const *)y);
}

/* sorted(set(v)) into the arena. */
static const char **sorted_set(AvpArena *a, CzVec *v, size_t *n) {
    avp_sort((void *)v->v, v->n, sizeof(const char *), cmp_cz, NULL, false);
    const char **out = AVP_NEWN(a, const char *, v->n ? v->n : 1);
    size_t k = 0;
    for (size_t i = 0; i < v->n; i++)
        if (!k || strcmp(out[k - 1], v->v[i])) out[k++] = v->v[i];
    *n = k;
    return out;
}

/* `\((r\d+)\)\s*$` searched in ops: the register number, or -1. */
static int base_reg(AvpStr ops) {
    size_t e = ops.n;
    while (e && is_space(ops.p[e - 1])) e--;
    if (!e || ops.p[e - 1] != ')') return -1;
    size_t d = e - 1, s = d;
    while (s && ops.p[s - 1] >= '0' && ops.p[s - 1] <= '9') s--;
    if (s == d || s < 2 || ops.p[s - 1] != 'r' || ops.p[s - 2] != '(') return -1;
    int r = 0;
    for (size_t k = s; k < d; k++) r = r * 10 + (ops.p[k] - '0');
    return (d - s) > 3 ? -1 : r;
}

/* register name "rN" -> N, or -1 */
static int reg_num(AvpStr s) {
    if (s.n < 2 || s.n > 4 || s.p[0] != 'r') return -1;
    int r = 0;
    for (size_t k = 1; k < s.n; k++) {
        if (s.p[k] < '0' || s.p[k] > '9') return -1;
        r = r * 10 + (s.p[k] - '0');
    }
    return r < 1000 ? r : -1;
}

typedef struct {
    bool active;
    const char *name, *sec, *scope;
    bool is_fn, zero, words_only;
    i64 addr, size;
    CzVec refs, stores;
    AVP_VEC(UnitCtorArg) ctor;
    const char *r3;
    const char *regs[1000];
    bool regs_used;
} Cur;

static void regs_clear(Cur *c) {
    if (c->regs_used) memset((void *)c->regs, 0, sizeof c->regs);
    c->regs_used = false;
}

static void parse_text(Ctx *c, AvpStr text, Cur *cur) {
    bool pend = false;
    AvpStr psec = {0};
    i64 paddr = 0, psize = 0;
    cur->active = false;
    CzVec found = {0};
    size_t pos = 0;
    while (pos <= text.n) {
        size_t e = pos;
        while (e < text.n && text.p[e] != '\n') e++;
        AvpStr line = avp_strn(text.p + pos, e - pos);
        pos = e + 1;
        if (!line.n) {
            if (e >= text.n) break;
            continue;
        }
        char ch = line.p[0];
        if (ch == '#') {
            AvpStr sec;
            i64 ad, sz;
            if (hdr_match(line, &sec, &ad, &sz)) {
                pend = true;
                psec = sec;
                paddr = ad;
                psize = sz;
            }
            continue;
        }
        if (ch == '.') {
            if (starts(line, ".fn ") || starts(line, ".obj ")) {
                bool is_fn;
                AvpStr nm, scope;
                if (decl_match(line, &is_fn, &nm, &scope) && pend) {
                    cur->active = true;
                    cur->name = avp_cz(c->a, nm);
                    cur->is_fn = is_fn;
                    cur->sec = avp_cz(c->a, psec);
                    cur->addr = paddr;
                    cur->size = psize;
                    cur->zero = !is_fn;
                    cur->words_only = !is_fn;
                    cur->scope = scope.n ? avp_cz(c->a, scope) : "global";
                    cur->refs.n = cur->stores.n = cur->ctor.n = 0;
                    cur->r3 = NULL;
                    regs_clear(cur);
                    pend = false;
                } else {
                    cur->active = false;
                }
            } else if (starts(line, ".endfn") || starts(line, ".endobj")) {
                if (cur->active) {
                    UnitBlock *b = AVP_NEW(c->a, UnitBlock);
                    b->name = cur->name;
                    b->kind = cur->is_fn ? "fn" : "obj";
                    b->sec = cur->sec;
                    b->addr = cur->addr;
                    b->size = cur->size;
                    b->refs = sorted_set(c->a, &cur->refs, &b->nrefs);
                    b->zero = cur->zero;
                    b->words_only = cur->words_only;
                    b->nctor = cur->ctor.n;
                    b->ctor_args = AVP_NEWN(c->a, UnitCtorArg, cur->ctor.n ? cur->ctor.n : 1);
                    if (cur->ctor.n) memcpy(b->ctor_args, cur->ctor.v, cur->ctor.n * sizeof(UnitCtorArg));
                    b->scope = cur->scope;
                    b->stores = sorted_set(c->a, &cur->stores, &b->nstores);
                    avp_vec_push(c->blocks, b);
                }
                cur->active = false;
            }
            continue;
        }
        if (!cur->active) continue;
        AvpStr rest = {0}, ops = {0};
        i64 sp = -1;
        if (ch == '/') {
            i64 tab = avp_str_findc(line, '\t');
            if (tab < 0) continue;
            rest = avp_strn(line.p + tab + 1, line.n - (size_t)tab - 1);
            sp = avp_str_findc(rest, ' ');
            ops = sp >= 0 ? avp_strn(rest.p + sp + 1, rest.n - (size_t)sp - 1) : avp_strn(rest.p, 0);
        } else if (ch == '\t') {
            if (starts(line, "\t.string") || starts(line, "\t.asci")) {
                cur->zero = false;
                cur->words_only = false;
                continue;
            }
            if (!starts(line, "\t.4byte ")) cur->words_only = false;
            if (!cur->is_fn && cur->zero && !starts(line, "\t.4byte 0x00000000") && !starts(line, "\t.2byte 0x0000") &&
                !starts(line, "\t.byte 0x00") && !starts(line, "\t.skip "))
                cur->zero = false;
            i64 s2 = avp_str_findc(line, ' ');
            ops = s2 >= 0 ? avp_strn(line.p + s2 + 1, line.n - (size_t)s2 - 1) : avp_strn(line.p, 0);
        } else {
            continue;
        }
        find_names(c, ops, &found);
        for (size_t k = 0; k < found.n; k++) avp_vec_push(&cur->refs, found.v[k]);
        if (ch == '/' && cur->is_fn) {
            AvpStr mn = sp >= 0 ? avp_strn(rest.p, (size_t)sp) : rest;
            i64 comma = avp_str_findc(ops, ',');
            AvpStr dest = avp_str_strip(comma >= 0 ? avp_strn(ops.p, (size_t)comma) : ops);
            if (starts(mn, "st")) {
                if (found.n) {
                    for (size_t k = 0; k < found.n; k++) avp_vec_push(&cur->stores, found.v[k]);
                } else {
                    int r = base_reg(ops);
                    if (r >= 0 && cur->regs[r]) avp_vec_push(&cur->stores, cur->regs[r]);
                }
            } else if (starts(mn, "b")) {
                regs_clear(cur);
            } else {
                int r = reg_num(dest);
                if (r >= 0) cur->regs[r] = NULL;
                if (avp_str_eqz(mn, "addi") && found.n && avp_str_contains(ops, AVP_S("@l")) && r >= 0) {
                    cur->regs[r] = found.v[found.n - 1];
                    cur->regs_used = true;
                }
            }
            if (starts(rest, "bl ")) {
                if (found.n && (!strncmp(found.v[0], "__ct__", 6) || !strncmp(found.v[0], "__register_global_object", 24))) {
                    UnitCtorArg ca = {found.v[0], cur->r3};
                    avp_vec_push(&cur->ctor, ca);
                }
                cur->r3 = NULL;
            } else if (starts(ops, "r3,") && !starts(rest, "cmp") && !starts(rest, "st")) {
                cur->r3 = found.n ? found.v[0] : NULL;
            }
        }
    }
    avp_vec_free(&found);
}

/* _scan_asm_names: `^\.(?:fn|obj) ("[^"]+"|\S+?),` over every line. */
static void scan_names(Ctx *c, AvpStr text) {
    size_t pos = 0;
    while (pos < text.n) {
        size_t e = pos;
        while (e < text.n && text.p[e] != '\n') e++;
        AvpStr line = avp_strn(text.p + pos, e - pos);
        pos = e + 1;
        bool is_fn;
        AvpStr nm, scope;
        if (line.n > 4 && line.p[0] == '.' && decl_match(line, &is_fn, &nm, &scope)) {
            bool created;
            AvpVal *v = avp_map_put(c->names, nm, &created);
            if (created) v->p = avp_cz(c->a, nm);
        }
    }
}

static int cmp_block_addr(const void *x, const void *y, void *ctx) {
    (void)ctx;
    i64 l = (*(UnitBlock *const *)x)->addr, r = (*(UnitBlock *const *)y)->addr;
    return l < r ? -1 : l > r;
}

static UnitIndex *build_index(AvpArena *a, UnitBlockVec *raw) {
    UnitIndex *x = AVP_NEW(a, UnitIndex);
    AvpSet *seen = avp_set_new();
    for (size_t i = 0; i < raw->n; i++) {
        UnitBlock *b = raw->v[i];
        const char *k = avp_sprintf(a, "%s\x01%lld\x01%s", b->sec, (long long)b->addr, b->name);
        if (avp_set_add(seen, avp_str(k))) avp_vec_push(&x->blocks, b);
    }
    avp_set_free(seen);
    x->by_name = avp_map_new();
    x->by_sec = avp_map_new();
    x->referrers = avp_map_new();
    UnitBlockVec sorted = {0};
    for (size_t i = 0; i < x->blocks.n; i++) {
        UnitBlock *b = x->blocks.v[i];
        avp_map_put(x->by_name, avp_str(b->name), NULL)->p = b;
        avp_vec_push(&sorted, b);
    }
    avp_sort(sorted.v, sorted.n, sizeof(UnitBlock *), cmp_block_addr, NULL, false);
    for (size_t i = 0; i < sorted.n; i++) {
        bool created;
        AvpVal *v = avp_map_put(x->by_sec, avp_str(sorted.v[i]->sec), &created);
        if (created) v->p = AVP_NEW(a, UnitBlockVec);
        avp_vec_push((UnitBlockVec *)v->p, sorted.v[i]);
    }
    avp_vec_free(&sorted);
    for (size_t i = 0; i < x->blocks.n; i++) {
        UnitBlock *b = x->blocks.v[i];
        for (size_t k = 0; k < b->nrefs; k++) {
            bool created;
            AvpVal *v = avp_map_put(x->referrers, avp_str(b->refs[k]), &created);
            if (created) v->p = AVP_NEW(a, UnitBlockVec);
            avp_vec_push((UnitBlockVec *)v->p, b);
        }
    }
    return x;
}

static UnitIndex *index_of(AvpArena *a, const AvpStr *texts, size_t n) {
    Ctx c = {a, avp_map_new(), NULL};
    UnitBlockVec raw = {0};
    c.blocks = &raw;
    for (size_t i = 0; i < n; i++) scan_names(&c, texts[i]);
    Cur *cur = avp_xcalloc(1, sizeof(Cur));
    for (size_t i = 0; i < n; i++) parse_text(&c, texts[i], cur);
    avp_vec_free(&cur->refs);
    avp_vec_free(&cur->stores);
    avp_vec_free(&cur->ctor);
    avp_xfree(cur);
    UnitIndex *x = build_index(a, &raw);
    avp_vec_free(&raw);
    return x;
}

UnitIndex *unit_index_from_texts(AvpArena *a, const AvpStr *texts, size_t n) { return index_of(a, texts, n); }

UnitIndex *unit_index_load(AvpArena *a, const char *root) {
    const char *asm_dir = avp_join(a, root, "build", UNIT_GAME, "asm");
    if (!avp_isdir(asm_dir)) unit_fail("%s does not exist: run the build once so dtk writes the retail asm", asm_dir);
    ShsmListings *L = shsm_asm_listings(a, asm_dir);
    AvpStrVec texts = {0};
    for (size_t i = 0; i < L->paths.n; i++) {
        const char *p = avp_cz(a, L->paths.v[i]);
        if (!avp_isfile(p)) continue; /* a build rewriting the directory right now */
        AvpStr t;
        if (!avp_read_text(a, p, AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_REPLACE, AVP_NL_DEFAULT), &t)) {
            fflush(stdout);
            fprintf(stderr, "warning: cannot read %s: %s\n", p, avp_cz(a, avp_io_err_str(a)));
            continue;
        }
        avp_vec_push(&texts, t);
    }
    UnitIndex *x = index_of(a, texts.v, texts.n);
    avp_vec_free(&texts);
    x->note = L->note;
    x->record = L->record;
    return x;
}

UnitBlock *unit_index_block(const UnitIndex *x, const char *name) {
    AvpVal *v = avp_map_get(x->by_name, avp_str(name));
    return v ? v->p : NULL;
}

const UnitBlockVec *unit_index_referrers(const UnitIndex *x, const char *name) {
    static const UnitBlockVec empty = {0};
    AvpVal *v = avp_map_get(x->referrers, avp_str(name));
    return v ? v->p : &empty;
}

UnitBlockVec unit_index_in_range(AvpArena *a, const UnitIndex *x, const char *sec, i64 lo, i64 hi) {
    (void)a;
    UnitBlockVec res = {0};
    AvpVal *v = avp_map_get(x->by_sec, avp_str(sec));
    if (!v) return res;
    const UnitBlockVec *bl = v->p;
    size_t l = 0, h = bl->n;
    while (l < h) {
        size_t m = (l + h) / 2;
        if (bl->v[m]->addr < lo) l = m + 1;
        else h = m;
    }
    for (size_t i = l; i < bl->n && bl->v[i]->addr < hi; i++) avp_vec_push(&res, bl->v[i]);
    return res;
}

bool unit_is_asm_source(const char *path) {
    size_t n = strlen(path);
    return n >= 2 && path[n - 2] == '.' && (path[n - 1] == 's' || path[n - 1] == 'S');
}

/* ^pad_\d{2}_[0-9A-Fa-f]{8}_[A-Za-z0-9]+$: the function dtk generates over code bytes no symbol covers (the TRK
 * vector table); in an assembly source unit the source assembles those bytes (user's ruling 2026-11-14). */
bool unit_is_pad_name(const char *s) {
    if (strncmp(s, "pad_", 4)) return false;
    char buf[64];
    size_t n = strlen(s);
    if (n >= sizeof buf) return false;
    memcpy(buf, s, n + 1);
    memcpy(buf, "gap_", 4);
    return unit_is_gap_name(buf);
}

/* GAP_RE: ^gap_\d{2}_[0-9A-Fa-f]{8}_[A-Za-z0-9]+$ */
bool unit_is_gap_name(const char *s) {
    if (strncmp(s, "gap_", 4)) return false;
    s += 4;
    for (int i = 0; i < 2; i++, s++)
        if (*s < '0' || *s > '9') return false;
    if (*s++ != '_') return false;
    for (int i = 0; i < 8; i++, s++)
        if (!is_hex(*s)) return false;
    if (*s++ != '_') return false;
    if (!*s) return false;
    for (; *s; s++)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9'))) return false;
    return true;
}

bool unit_block_refs(const UnitBlock *b, const char *name) {
    for (size_t i = 0; i < b->nrefs; i++)
        if (!strcmp(b->refs[i], name)) return true;
    return false;
}
