/* sibling_foreign: index, pair and report a foreign-architecture sibling (sibling_foreign.h). */
#include "sibling_foreign.h"

#include "avp/fs.h"
#include "shsm_asmindex.h"
#include "shsm_mangle.h"
#include "witness_map.h"
#include "witness_ps2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NT 8
static const char *const TIERS[NT] = {"string", "table", "constant", "call", "callees", "callers", "seq", "seq-edge"};
#define TIER_SEQ 7 /* 1-based tier numbers: TIERS[t - 1] */
#define TIER_EDGE 8
#define MIN_NAMED 20
#define MIN_PRECISION 95.0

static int tier_num(const char *t) {
    for (int k = 0; k < NT; k++)
        if (t && strcmp(t, TIERS[k]) == 0) return k + 1;
    return 0;
}

/* ---- sqlite helpers ---- */

static void fail(sqlite3 *db) { shsm_raise(SHSM_EXC_SQLITE, "%s", sqlite3_errmsg(db)); }
static void x_exec(sqlite3 *db, const char *sql) {
    if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) fail(db);
}
static sqlite3_stmt *x_prep(sqlite3 *db, const char *sql) {
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) fail(db);
    return st;
}
static const char *col(AvpArena *a, sqlite3_stmt *st, int i) {
    const unsigned char *t = sqlite3_column_text(st, i);
    return t ? avp_strdup(a, (const char *)t) : NULL;
}
static void bind_text(sqlite3_stmt *st, int i, const char *s) {
    if (s) sqlite3_bind_text(st, i, s, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(st, i);
}
static void meta_set(Sibling *sb, const char *k, const char *v) {
    sqlite3_stmt *st = x_prep(sb->db, "insert or replace into meta(key, value) values(?, ?)");
    bind_text(st, 1, k);
    bind_text(st, 2, v);
    if (sqlite3_step(st) != SQLITE_DONE) fail(sb->db);
    sqlite3_finalize(st);
}
static const char *meta_get(AvpArena *a, Sibling *sb, const char *k) {
    sqlite3_stmt *st = x_prep(sb->db, "select value from meta where key = ?");
    bind_text(st, 1, k);
    const char *v = sqlite3_step(st) == SQLITE_ROW ? col(a, st, 0) : NULL;
    sqlite3_finalize(st);
    return v;
}

static void create_tables(Sibling *sb) {
    x_exec(sb->db, "create table if not exists xfiles(fd integer primary key, path text, nfn integer)");
    x_exec(sb->db, "create table if not exists xfn(address integer primary key, size integer, name text,"
                   " demangled text, fd integer, ord integer, symtab integer, strs text, floats text, ints text,"
                   " callees text, ncallers integer)");
    x_exec(sb->db, "create table if not exists xpair(shsm integer primary key, name text, size integer, lib text,"
                   " unit text, gr integer, tier text, evidence text)");
}

bool sibling_is_foreign(Sibling *sb) {
    const char *v = meta_get(avp_scratch(), sb, "foreign");
    return v && strcmp(v, "1") == 0;
}

bool sibling_elf_is_foreign(const char *elf) {
    if (!elf) return false;
    FILE *fp = fopen(elf, "rb");
    if (!fp) return false;
    u8 h[20] = {0};
    size_t n = fread(h, 1, sizeof h, fp);
    fclose(fp);
    if (n < 20 || memcmp(h, "\x7f" "ELF", 4) != 0) return false;
    u16 mach = h[5] == 2 ? (u16)(h[18] << 8 | h[19]) : (u16)(h[19] << 8 | h[18]);
    return mach == SIB_EM_MIPS;
}

/* ---- the function list ---- */

static int xfn_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const SibXFn *a = x, *b = y;
    return a->address < b->address ? -1 : a->address > b->address;
}

static bool is_code(const SibElfSec *s) { return (s->flags & 4) && strcmp(s->name, ".text") == 0; }

SibXFnVec sib_foreign_functions(AvpArena *a, const SibElf *e, const SibMdebug *m) {
    SibXFnVec v = {0};
    const SibElfSec *text = sib_elf_section(e, ".text");
    i64 lo = text && is_code(text) ? text->addr : 0, hi = text ? (i64)text->addr + text->size : 0;
    AvpIMap *at = avp_imap_new();
    for (size_t k = 0; k < e->syms.n; k++) {
        const SibElfSym *y = &e->syms.v[k];
        if (y->type != 2 || !y->size || y->value < lo || y->value >= hi) continue; /* STT_FUNC in .text */
        bool created;
        AvpVal *slot = avp_imap_put(at, y->value, &created);
        if (!created) {
            SibXFn *f = &v.v[slot->i];
            if (y->bind == 1 && f->name[0] == '_' && y->name[0] != '_') f->name = y->name; /* prefer a plain global */
            continue;
        }
        slot->i = (i64)v.n;
        avp_vec_push(&v, ((SibXFn){.address = y->value, .size = y->size, .name = y->name, .file = -1,
                                   .from_symtab = true}));
    }
    if (m) {
        for (size_t k = 0; k < m->pds.n; k++) {
            const SibPdr *p = &m->pds.v[k];
            if (p->adr < lo || p->adr >= hi) continue;
            bool created;
            AvpVal *slot = avp_imap_put(at, p->adr, &created);
            if (created) {
                slot->i = (i64)v.n;
                avp_vec_push(&v, ((SibXFn){.address = p->adr, .name = p->name, .file = p->fd}));
            } else v.v[slot->i].file = p->fd;
        }
    }
    avp_imap_free(at);
    avp_sort(v.v, v.n, sizeof *v.v, xfn_cmp, NULL, false);
    for (size_t k = 0; k < v.n; k++) {
        SibXFn *f = &v.v[k];
        if (!f->size) f->size = (k + 1 < v.n ? v.v[k + 1].address : hi) - f->address;
        if (!f->name || !*f->name) f->name = avp_sprintf(a, "proc_%08llX", (long long)f->address);
        f->demangled = sib_gnu_demangle(a, f->name);
    }
    sib_foreign_group(v.v, v.n);
    return v;
}

void sib_foreign_group(SibXFn *v, size_t n) {
    i32 run = -1;
    for (size_t k = 0; k < n; k++) {
        if (v[k].file >= 0) v[k].group = v[k].file;
        else {
            if (k == 0 || v[k - 1].file >= 0) run++;
            v[k].group = -2 - run;
        }
    }
    AvpIMap *cnt = avp_imap_new();
    for (size_t k = 0; k < n; k++) {
        bool created;
        v[k].order = avp_imap_put(cnt, (u64)(i64)v[k].group, &created)->i++;
    }
    avp_imap_free(cnt);
}

/* ---- seq ---- */

static bool size_ok(i64 x, i64 y) {
    if (x < 48 && y < 48) return true;
    if (x <= 0 || y <= 0) return false;
    return x <= 4 * y && y <= 4 * x;
}

typedef AVP_VEC(AvpI64Vec) Members;

/* append the foreign indices strictly between orders lo and hi of a group; false when one of them is paired */
static bool between(const Members *m, const AvpIMap *gi, i32 group, i64 lo, i64 hi, const SibSeqNode *g,
                    AvpI64Vec *out) {
    AvpVal *v = avp_imap_get(gi, (u64)(i64)group);
    if (!v) return false;
    const AvpI64Vec *mem = &m->v[v->i];
    for (size_t k = 0; k < mem->n; k++) {
        if ((i64)k <= lo || (i64)k >= hi) continue;
        if (g[mem->v[k]].peer >= 0) return false;
        avp_vec_push(out, mem->v[k]);
    }
    return true;
}
static i64 group_size(const Members *m, const AvpIMap *gi, i32 group) {
    AvpVal *v = avp_imap_get(gi, (u64)(i64)group);
    return v ? (i64)m->v[v->i].n : 0;
}

i64 sib_seq_fill(SibSeqNode *s, size_t ns, SibSeqNode *g, const SibXFn *gx, size_t ng, int tier, int tier_edge) {
    Members mem = {0};
    AvpIMap *gidx = avp_imap_new();
    for (size_t k = 0; k < ng; k++) {
        bool created;
        AvpVal *v = avp_imap_put(gidx, (u64)(i64)gx[k].group, &created);
        if (created) {
            v->i = (i64)mem.n;
            avp_vec_push_zero(&mem);
        }
        avp_vec_push(&mem.v[v->i], (i64)k); /* address order = order in the group */
    }
    i64 added = 0;
    size_t prev = (size_t)-1;
    AvpI64Vec cand = {0};
    for (size_t i = 0; i < ns; i++) {
        if (s[i].peer < 0 || s[i].tier == tier || (tier_edge && s[i].tier == tier_edge)) continue;
        if (prev != (size_t)-1 && i - prev > 1) {
            const SibXFn *a = &gx[s[prev].peer], *b = &gx[s[i].peer];
            size_t gap = i - prev - 1;
            int use = 0;
            cand.n = 0;
            if (a->group == b->group && a->order < b->order) {
                if (between(&mem, gidx, a->group, a->order, b->order, g, &cand) && cand.n == gap) use = tier;
            } else if (tier_edge && a->group != b->group) {
                /* the tail of a's group and the head of b's: a file boundary inside the gap */
                if (between(&mem, gidx, a->group, a->order, group_size(&mem, gidx, a->group), g, &cand) &&
                    between(&mem, gidx, b->group, -1, b->order, g, &cand) && cand.n == gap && gap)
                    use = tier_edge;
            }
            for (size_t r = prev + 1; use && r < i; r++)
                if (strcmp(s[r].unit, s[prev].unit) != 0 || strcmp(s[r].unit, s[i].unit) != 0) use = 0;
            for (size_t r = prev + 1, k = 0; use && r < i; r++, k++)
                if (!size_ok(s[r].size, g[cand.v[k]].size)) use = 0;
            for (size_t r = prev + 1, k = 0; use && r < i; r++, k++) {
                s[r].peer = cand.v[k];
                s[r].tier = use;
                g[cand.v[k]].peer = (i64)r;
                g[cand.v[k]].tier = use;
                added++;
            }
        }
        prev = i;
    }
    avp_vec_free(&cand);
    for (size_t k = 0; k < mem.n; k++) avp_vec_free(&mem.v[k]);
    avp_vec_free(&mem);
    avp_imap_free(gidx);
    return added;
}

/* ---- loading the ELF ---- */

typedef struct {
    const char *path;
    AvpBytes data;
    SibElf elf;
    SibMdebug md;
    bool has_md;
    const char *md_note;
} Foreign;

static Foreign *load_foreign(AvpArena *a, const char *path) {
    Foreign *f = avp_alloc(a, sizeof *f);
    f->path = path;
    if (!avp_read_all(a, path, &f->data)) shsm_raise(SHSM_EXC_USAGE, "cannot read %s", path);
    const char *err = NULL;
    if (!sib_elf_parse(a, f->data.v, f->data.n, &f->elf, &err)) shsm_raise(SHSM_EXC_USAGE, "%s: %s", path, err);
    const SibElfSec *md = sib_elf_section(&f->elf, ".mdebug");
    if (md) {
        f->has_md = sib_md_parse(a, f->data.v, f->data.n, md->offset, md->size, &f->md, &err);
        if (!f->has_md) f->md_note = err;
    } else f->md_note = "no .mdebug section: .symtab only, source files unknown";
    return f;
}

static const char *elf_of(AvpArena *a, Sibling *sb, AvpArgs *args) {
    const char *e = NULL;
    if (avp_args_has(args, "elf") && !avp_args_is_none(args, "elf")) e = avp_cz(a, avp_args_str(args, "elf"));
    if (!e) e = meta_get(a, sb, "foreign.elf");
    if (!e) shsm_raise(SHSM_EXC_USAGE, "sibling %s: no ELF recorded; run index --name %s --elf PATH", sb->name, sb->name);
    return e;
}

/* ---- features as text ---- */

static const char *join_set(AvpArena *a, AvpSet *s) {
    if (!s) return "";
    AvpStrVec v = avp_set_sorted(s);
    AvpBuf b = {0};
    avp_buf_init(&b, a);
    for (size_t k = 0; k < v.n; k++) {
        if (k) avp_buf_putc(&b, '\x1f');
        avp_buf_put(&b, v.v[k]);
    }
    avp_vec_free(&v);
    return avp_buf_cz(&b);
}
static const char *join_ints(AvpArena *a, AvpPyIntSet *s) {
    if (!s) return "";
    AvpBuf b = {0};
    avp_buf_init(&b, a);
    size_t pos = 0;
    i64 x;
    bool first = true;
    while (avp_pyintset_next(s, &pos, &x)) {
        avp_buf_printf(&b, "%s0x%llX", first ? "" : ",", (unsigned long long)x);
        first = false;
    }
    return avp_buf_cz(&b);
}

/* ---- index ---- */

static void build_ps2_side(AvpArena *a, Foreign *f, const SibXFnVec *fns, WtSide *side, WtRuns *runs) {
    WtElf *we = wt_elf_new(a, f->data.v, f->data.n);
    i64 *spans = avp_alloc(a, sizeof *spans * 2 * (fns->n + 1));
    for (size_t k = 0; k < fns->n; k++) {
        spans[2 * k] = fns->v[k].address;
        spans[2 * k + 1] = fns->v[k].address + fns->v[k].size;
    }
    WtFacts facts;
    wt_build_ps2_spans(a, we, spans, fns->n, side, runs, &facts);
}

static int cmd_index(Sibling *sb, const char *elf, ShsmOutput *out) {
    AvpArena *a = avp_arena();
    Foreign *f = load_foreign(a, elf);
    if (f->elf.machine != SIB_EM_MIPS) shsm_raise(SHSM_EXC_USAGE, "%s is not a MIPS ELF", elf);
    SibXFnVec fns = sib_foreign_functions(a, &f->elf, f->has_md ? &f->md : NULL);
    WtSide side;
    WtRuns runs;
    build_ps2_side(a, f, &fns, &side, &runs);
    AvpIMap *idx = avp_imap_new();
    for (size_t k = 0; k < fns.n; k++) avp_imap_put(idx, (u64)fns.v[k].address, NULL)->i = (i64)k;
    i64 *callers = avp_alloc(a, sizeof *callers * (fns.n + 1));
    for (size_t k = 0; k < side.v.n; k++) {
        AvpPyIntSet *seen = avp_pyintset_new();
        for (size_t c = 0; c < side.v.v[k]->calls.n; c++) {
            i64 t = side.v.v[k]->calls.v[c];
            AvpVal *v = t == WT_NONE ? NULL : avp_imap_get(idx, (u64)t);
            if (v && avp_pyintset_add(seen, t)) callers[v->i]++;
        }
        avp_pyintset_free(seen);
    }
    create_tables(sb);
    x_exec(sb->db, "begin");
    x_exec(sb->db, "delete from xfiles");
    x_exec(sb->db, "delete from xfn");
    x_exec(sb->db, "delete from xpair");
    sqlite3_stmt *st = x_prep(sb->db, "insert into xfn values(?,?,?,?,?,?,?,?,?,?,?,?)");
    i64 attributed = 0, mdonly = 0, demangled = 0, mangled = 0, analysed = 0;
    i64 *perfile = avp_alloc(a, sizeof *perfile * (f->md.fds.n + 1));
    for (size_t k = 0; k < fns.n; k++) {
        const SibXFn *x = &fns.v[k];
        WtFn *w = wt_side_get(&side, x->address);
        sqlite3_bind_int64(st, 1, x->address);
        sqlite3_bind_int64(st, 2, x->size);
        bind_text(st, 3, x->name);
        bind_text(st, 4, x->demangled);
        sqlite3_bind_int64(st, 5, x->file);
        sqlite3_bind_int64(st, 6, x->order);
        sqlite3_bind_int(st, 7, x->from_symtab);
        bind_text(st, 8, w ? join_set(a, w->strs) : "");
        bind_text(st, 9, w ? join_ints(a, w->floats) : "");
        bind_text(st, 10, w ? join_ints(a, w->ints) : "");
        AvpBuf cb = {0};
        avp_buf_init(&cb, a);
        for (size_t c = 0; w && c < w->calls.n; c++) {
            AvpVal *v = w->calls.v[c] == WT_NONE ? NULL : avp_imap_get(idx, (u64)w->calls.v[c]);
            avp_buf_printf(&cb, "%s%s", c ? "," : "", v ? fns.v[v->i].name : "?");
        }
        bind_text(st, 11, avp_buf_cz(&cb));
        sqlite3_bind_int64(st, 12, callers[k]);
        if (sqlite3_step(st) != SQLITE_DONE) fail(sb->db);
        sqlite3_reset(st);
        if (x->file >= 0) {
            attributed++;
            perfile[x->file]++;
        }
        if (!x->from_symtab) mdonly++;
        if (strstr(x->name + 1, "__") || strncmp(x->name, "_$_", 3) == 0) {
            mangled++;
            if (x->demangled) demangled++;
        }
        if (w) analysed++;
    }
    sqlite3_finalize(st);
    st = x_prep(sb->db, "insert into xfiles values(?,?,?)");
    for (size_t k = 0; k < f->md.fds.n; k++) {
        sqlite3_bind_int64(st, 1, (i64)k);
        bind_text(st, 2, f->md.fds.v[k].name);
        sqlite3_bind_int64(st, 3, perfile[k]);
        if (sqlite3_step(st) != SQLITE_DONE) fail(sb->db);
        sqlite3_reset(st);
    }
    sqlite3_finalize(st);
    meta_set(sb, "foreign", "1");
    meta_set(sb, "foreign.elf", elf);
    meta_set(sb, "foreign.paired", NULL);
    x_exec(sb->db, "commit");
    i64 symfn = (i64)fns.n - mdonly;
    shsm_output_linez(out, avp_sprintf(a, "%s: foreign sibling %s (MIPS, %s)", sb->name, elf,
                                       f->has_md ? "with .mdebug" : "no .mdebug"));
    if (f->has_md) {
        const SibMdebug *m = &f->md;
        shsm_output_linez(out, avp_sprintf(a, "  .mdebug HDRR: %d file descriptors, %d procedures, %d local symbols,"
                                              " %d bytes of local strings, %d external symbols, %d line entries",
                                           m->ifdMax, m->ipdMax, m->isymMax, m->issMax, m->iextMax, m->ilineMax));
        i64 dead = 0;
        for (size_t k = 0; k < m->pds.n; k++) dead += m->pds.v[k].adr == 0xFFFFFFFFu;
        shsm_output_linez(out, avp_sprintf(a, "  procedures at 0xFFFFFFFF (copies the linker discarded: inline and"
                                              " template instances of other files): %lld",
                                           (long long)dead));
    } else shsm_output_linez(out, avp_sprintf(a, "  %s", f->md_note));
    shsm_output_linez(out, avp_sprintf(a, "  functions %zu: %lld from .symtab, %lld only in the procedure table"
                                          " (statics); %lld analysed by the MIPS model",
                                       fns.n, (long long)symfn, (long long)mdonly, (long long)analysed));
    shsm_output_linez(out, avp_sprintf(a, "  source files %zu; functions attributed to a file %lld (by procedure"
                                          " descriptor), unattributed %lld",
                                       f->md.fds.n, (long long)attributed, (long long)((i64)fns.n - attributed)));
    shsm_output_linez(out, avp_sprintf(a, "  gnu-v2 manglings %lld, demangled %lld", (long long)mangled,
                                       (long long)demangled));
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("functions"), avp_json_int(a, (i64)fns.n));
    avp_json_set(a, o, AVP_S("symtab"), avp_json_int(a, symfn));
    avp_json_set(a, o, AVP_S("mdebug_only"), avp_json_int(a, mdonly));
    avp_json_set(a, o, AVP_S("files"), avp_json_int(a, (i64)f->md.fds.n));
    avp_json_set(a, o, AVP_S("attributed"), avp_json_int(a, attributed));
    avp_json_set(a, o, AVP_S("mangled"), avp_json_int(a, mangled));
    avp_json_set(a, o, AVP_S("demangled"), avp_json_int(a, demangled));
    shsm_output_data(out, "index", o);
    avp_imap_free(idx);
    return 0;
}

/* ---- ground truth ---- */

static const char *shsm_qual(AvpArena *a, const char *name) {
    if (!name) return "";
    if (!strstr(name, "__")) return name;
    AvpTry t;
    const char *q = name;
    if (AVP_TRY(&t)) {
        q = shsm_qualified(a, name);
        avp_try_end(&t);
    }
    return q ? q : name;
}
static const char *gr_qual(AvpArena *a, const char *name) {
    const char *q = sib_gnu_qualified(a, name);
    return q ? q : name;
}
static const char *last_part(const char *q) {
    const char *p = strrchr(q, ':');
    return p ? p + 1 : q;
}
static bool loose_eq(const char *x, const char *y) {
    while (*x == '_') x++;
    while (*y == '_') y++;
    return _stricmp(x, y) == 0;
}
/* "same" (qualified names equal), "near" (equal but for leading underscores and case, or the same last component:
 * another class or namespace) or "differ" */
static bool gcc_initialiser(const char *n) {
    return strncmp(n, "__static_initialization_and_destruction", 39) == 0 || strncmp(n, "_GLOBAL_$I$", 11) == 0;
}
static const char *agreement(AvpArena *a, const char *shsm, const char *gr) {
    const char *qs = shsm_qual(a, shsm), *qg = gr_qual(a, gr);
    if (strcmp(qs, qg) == 0) return "same";
    /* the two compilers' names for one thing: a file's static initialiser, the global operators */
    if (sibling_is_initialiser(shsm) && gcc_initialiser(gr)) return "same";
    static const char *const OPS[][2] = {{"operator new", "__builtin_new"}, {"operator delete", "__builtin_delete"},
                                          {"operator new[]", "__builtin_vec_new"},
                                          {"operator delete[]", "__builtin_vec_delete"}};
    for (int k = 0; k < 4; k++)
        if (strcmp(qs, OPS[k][0]) == 0 && strcmp(qg, OPS[k][1]) == 0) return "same";
    if (loose_eq(qs, qg) || loose_eq(last_part(qs), last_part(qg))) return "near";
    return "differ";
}

/* ---- the in-memory pairing ---- */

typedef struct {
    i64 address, size, gr; /* gr: index into fns, -1 */
    const char *name, *lib, *unit, *tier, *ev;
} XRow;
typedef AVP_VEC(XRow) XRowVec;

typedef struct {
    SibXFnVec fns;
    AvpStrVec files;
    XRowVec rows; /* every retail function, address order */
    AvpIMap *gr_idx;
} XState;

static int xrow_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const XRow *a = x, *b = y;
    return a->address < b->address ? -1 : a->address > b->address;
}

static XState *load_state(AvpArena *a, Sibling *sb, bool need_pairs) {
    create_tables(sb);
    XState *x = avp_alloc(a, sizeof *x);
    sqlite3_stmt *st = x_prep(sb->db, "select address, size, name, demangled, fd, ord, symtab from xfn order by address");
    while (sqlite3_step(st) == SQLITE_ROW) {
        SibXFn f = {0};
        f.address = sqlite3_column_int64(st, 0);
        f.size = sqlite3_column_int64(st, 1);
        f.name = col(a, st, 2);
        f.demangled = col(a, st, 3);
        f.file = (i32)sqlite3_column_int64(st, 4);
        f.order = sqlite3_column_int64(st, 5);
        f.from_symtab = sqlite3_column_int(st, 6) != 0;
        avp_vec_push(&x->fns, f);
    }
    sqlite3_finalize(st);
    if (!x->fns.n) shsm_raise(SHSM_EXC_USAGE, "sibling %s has no index: run index --name %s --elf PATH", sb->name, sb->name);
    st = x_prep(sb->db, "select fd, path from xfiles order by fd");
    while (sqlite3_step(st) == SQLITE_ROW) avp_vec_push(&x->files, avp_str(col(a, st, 1)));
    sqlite3_finalize(st);
    x->gr_idx = avp_imap_new();
    for (size_t k = 0; k < x->fns.n; k++) avp_imap_put(x->gr_idx, (u64)x->fns.v[k].address, NULL)->i = (i64)k;
    sib_foreign_group(x->fns.v, x->fns.n);
    st = x_prep(sb->db, "select shsm, name, size, lib, unit, gr, tier, evidence from xpair order by shsm");
    while (sqlite3_step(st) == SQLITE_ROW) {
        XRow r = {0};
        r.address = sqlite3_column_int64(st, 0);
        r.name = col(a, st, 1);
        r.size = sqlite3_column_int64(st, 2);
        r.lib = col(a, st, 3);
        r.unit = col(a, st, 4);
        r.gr = -1;
        if (sqlite3_column_type(st, 5) != SQLITE_NULL) {
            AvpVal *v = avp_imap_get(x->gr_idx, (u64)sqlite3_column_int64(st, 5));
            if (v) r.gr = v->i;
        }
        r.tier = col(a, st, 6);
        r.ev = col(a, st, 7);
        if (!r.lib) r.lib = "unknown";
        if (!r.unit) r.unit = "";
        avp_vec_push(&x->rows, r);
    }
    sqlite3_finalize(st);
    if (need_pairs && !x->rows.n)
        shsm_raise(SHSM_EXC_USAGE, "sibling %s is not paired yet: run pair --name %s", sb->name, sb->name);
    return x;
}

static const char *file_of(const XState *x, i64 gr) {
    if (gr < 0) return "";
    i32 fd = x->fns.v[gr].file;
    return fd >= 0 && (size_t)fd < x->files.n ? avp_cz(avp_scratch(), x->files.v[fd]) : "";
}
static const char *short_path(const char *p) {
    const char *k = strstr(p, "/code/");
    if (k) return k + 6;
    k = strstr(p, "dailyphysics/");
    return k ? k : p;
}

/* ---- precision ---- */

typedef struct {
    i64 named[NT + 1], same[NT + 1], near[NT + 1], paired[NT + 1];
    double prec[NT + 1];
    bool dflt[NT + 1];
} Prec;

static void measure(AvpArena *a, const XState *x, Prec *p) {
    memset(p, 0, sizeof *p);
    for (size_t i = 0; i < x->rows.n; i++) {
        const XRow *r = &x->rows.v[i];
        int t = tier_num(r->tier);
        if (!t || r->gr < 0) continue;
        p->paired[t]++;
        if (sibling_is_placeholder(r->name)) continue;
        const char *gn = x->fns.v[r->gr].name;
        if (strncmp(gn, "proc_", 5) == 0) continue;
        p->named[t]++;
        const char *ag = agreement(a, r->name, gn);
        if (strcmp(ag, "same") == 0) p->same[t]++;
        else if (strcmp(ag, "near") == 0) p->near[t]++;
    }
    for (int t = 1; t <= NT; t++) {
        p->prec[t] = p->named[t] ? 100.0 * (double)p->same[t] / (double)p->named[t] : 0.0;
        p->dflt[t] = p->named[t] >= MIN_NAMED && p->prec[t] >= MIN_PRECISION;
    }
}

static unsigned tiers_arg(AvpArena *a, AvpArgs *args, const Prec *p) {
    const char *t = NULL;
    if (avp_args_has(args, "tier") && !avp_args_is_none(args, "tier")) t = avp_cz(a, avp_args_str(args, "tier"));
    unsigned bits = 0;
    if (!t) {
        for (int k = 1; k <= NT; k++)
            if (p->dflt[k]) bits |= 1u << k;
        return bits;
    }
    if (strcmp(t, "all") == 0) return 0xFFFFu;
    AvpStrVec parts = avp_str_split(avp_str(t), AVP_S(","), -1);
    for (size_t k = 0; k < parts.n; k++) {
        const char *n = avp_cz(a, avp_str_strip(parts.v[k]));
        int i = tier_num(n);
        if (!i) shsm_raise(SHSM_EXC_USAGE, "unknown tier %s (string, table, constant, call, callees, callers, seq, seq-edge, all)", n);
        bits |= 1u << i;
    }
    avp_vec_free(&parts);
    return bits;
}

static const char *lib_arg(AvpArena *a, AvpArgs *args) {
    if (avp_args_has(args, "lib") && !avp_args_is_none(args, "lib")) return avp_cz(a, avp_args_str(args, "lib"));
    return NULL;
}

static bool in_tiers(const XRow *r, unsigned bits) { return r->gr >= 0 && (bits >> tier_num(r->tier) & 1u); }

/* ---- pair ---- */

static int cmd_stats(AvpArena *a, Sibling *sb, XState *x, AvpArgs *args, ShsmOutput *out, bool after_pair);

/* rows: every retail function with its pair in c, address order */
static void collect_rows(XState *x, const WtSide *wii, const WtCorr *c) {
    x->rows.n = 0;
    for (size_t k = 0; k < wii->v.n; k++) {
        const WtFn *w = wii->v.v[k];
        XRow r = {.address = w->addr, .size = w->size, .gr = -1, .name = w->name ? w->name : "",
                  .lib = w->lib ? w->lib : "unknown", .unit = w->unit ? w->unit : ""};
        const WtPairEntry *pe = wt_corr_pair(c, w->addr);
        AvpVal *v = pe ? avp_imap_get(x->gr_idx, (u64)pe->p.ps2) : NULL;
        if (v) {
            r.gr = v->i;
            r.tier = pe->p.tier;
            r.ev = pe->p.ev;
        }
        avp_vec_push(&x->rows, r);
    }
    avp_sort(x->rows.v, x->rows.n, sizeof *x->rows.v, xrow_cmp, NULL, false);
}

static int cmd_pair(Sibling *sb, AvpArgs *args, ShsmOutput *out) {
    AvpArena *a = avp_arena();
    const char *elf = elf_of(a, sb, args);
    XState *x = load_state(a, sb, false);
    Foreign *f = load_foreign(a, elf);
    WtSide ps2, wii;
    WtRuns pruns, wruns;
    build_ps2_side(a, f, &x->fns, &ps2, &pruns);
    ShsmAsmIndex *ix = shsm_asmindex_open(&(ShsmAsmIndexOpts){.repo = sb->repo, .readonly = true});
    int jobs = 0;
    if (avp_args_has(args, "jobs") && !avp_args_is_none(args, "jobs")) jobs = (int)avp_args_int(args, "jobs");
    wt_build_wii(a, sb->repo, ix, jobs, &wii, &wruns);
    WtCorr *c = wt_corr_new(&wii, &ps2, &wruns, &pruns, NULL);
    i64 seq = 0, edge = 0;
    int iters = 0;
    SibSeqNode *sn = avp_alloc(a, sizeof *sn * (wii.v.n + 1)), *gn = avp_alloc(a, sizeof *gn * (x->fns.n + 1));
    for (; iters < 6; iters++) {
        /* witness's tiers (again after each seq round: new pairs give the call tiers new anchors), then seq */
        wt_corr_run(c, 30);
        collect_rows(x, &wii, c);
        for (size_t k = 0; k < x->fns.n; k++)
            gn[k] = (SibSeqNode){.address = x->fns.v[k].address, .size = x->fns.v[k].size, .peer = -1, .unit = ""};
        for (size_t k = 0; k < x->rows.n; k++) {
            XRow *r = &x->rows.v[k];
            sn[k] = (SibSeqNode){.address = r->address, .size = r->size, .peer = r->gr, .tier = r->gr >= 0 ? 1 : 0,
                                 .unit = *r->unit ? r->unit : r->lib};
            if (r->gr >= 0) {
                gn[r->gr].peer = (i64)k;
                gn[r->gr].tier = 1;
            }
        }
        i64 n = sib_seq_fill(sn, x->rows.n, gn, x->fns.v, x->fns.n, TIER_SEQ, TIER_EDGE);
        for (size_t k = 0; k < x->rows.n; k++) {
            if (sn[k].tier != TIER_SEQ && sn[k].tier != TIER_EDGE) continue;
            bool is_edge = sn[k].tier == TIER_EDGE;
            wt_corr_add(c, x->rows.v[k].address, x->fns.v[sn[k].peer].address, TIERS[sn[k].tier - 1],
                        is_edge ? "the tail and head of two adjacent source files, same order and count"
                                : "between two pairs of one source file, same order and count");
            seq += !is_edge;
            edge += is_edge;
        }
        if (!n) break;
    }
    collect_rows(x, &wii, c);
    x_exec(sb->db, "begin");
    x_exec(sb->db, "delete from xpair");
    sqlite3_stmt *st = x_prep(sb->db, "insert into xpair values(?,?,?,?,?,?,?,?)");
    for (size_t k = 0; k < x->rows.n; k++) {
        const XRow *r = &x->rows.v[k];
        sqlite3_bind_int64(st, 1, r->address);
        bind_text(st, 2, r->name);
        sqlite3_bind_int64(st, 3, r->size);
        bind_text(st, 4, r->lib);
        bind_text(st, 5, r->unit);
        if (r->gr >= 0) sqlite3_bind_int64(st, 6, x->fns.v[r->gr].address);
        else sqlite3_bind_null(st, 6);
        bind_text(st, 7, r->gr >= 0 ? r->tier : NULL);
        bind_text(st, 8, r->gr >= 0 ? r->ev : NULL);
        if (sqlite3_step(st) != SQLITE_DONE) fail(sb->db);
        sqlite3_reset(st);
    }
    sqlite3_finalize(st);
    meta_set(sb, "foreign.paired", "1");
    x_exec(sb->db, "commit");
    shsm_output_linez(out, avp_sprintf(a, "paired %zu retail functions with %s: %d witness+seq iterations, seq added"
                                          " %lld, seq-edge added %lld",
                                       x->rows.n, sb->name, iters + 1, (long long)seq, (long long)edge));
    wt_corr_free(c);
    shsm_asmindex_close(ix);
    return cmd_stats(a, sb, x, args, out, true);
}

/* ---- stats ---- */

static int cmd_stats(AvpArena *a, Sibling *sb, XState *x, AvpArgs *args, ShsmOutput *out, bool after_pair) {
    (void)after_pair;
    Prec p;
    measure(a, x, &p);
    const char *lib = lib_arg(a, args);
    /* coverage per library */
    AvpMap *libs = avp_map_new();
    AVP_VEC(i64 *) cnt = {0};
    AvpStrVec order = {0};
    for (size_t i = 0; i < x->rows.n; i++) {
        const XRow *r = &x->rows.v[i];
        bool created;
        AvpVal *v = avp_map_put(libs, avp_str(r->lib), &created);
        if (created) {
            v->i = (i64)cnt.n;
            avp_vec_push(&cnt, avp_alloc(a, sizeof(i64) * (NT + 3)));
            avp_vec_push(&order, avp_str(r->lib));
        }
        i64 *c = cnt.v[v->i];
        c[0]++;
        c[NT + 2] += r->size;
        int t = r->gr >= 0 ? tier_num(r->tier) : 0;
        c[t ? t : NT + 1]++;
    }
    shsm_output_linez(out, avp_sprintf(a, "pairing with %s (%zu foreign functions, %zu source files; another compiler"
                                          " and CPU: no tier compares bodies byte for byte)",
                                       sb->name, x->fns.n, x->files.n));
    AvpBuf h = {0};
    avp_buf_init(&h, a);
    avp_buf_printf(&h, "%-18s %9s %10s |", "library", "functions", "bytes");
    for (int t = 1; t <= NT; t++) avp_buf_printf(&h, " %8s", TIERS[t - 1]);
    avp_buf_printf(&h, " | %8s", "unpaired");
    shsm_output_linez(out, avp_buf_cz(&h));
    AvpJson *lj = avp_json_arr(a);
    for (size_t k = 0; k < order.n; k++) {
        const char *ln = avp_cz(a, order.v[k]);
        if (lib && strcmp(lib, ln) != 0) continue;
        i64 *c = cnt.v[avp_map_get(libs, order.v[k])->i];
        AvpBuf b = {0};
        avp_buf_init(&b, a);
        avp_buf_printf(&b, "%-18s %9lld %10lld |", ln, (long long)c[0], (long long)c[NT + 2]);
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("library"), avp_json_strz(a, ln));
        avp_json_set(a, o, AVP_S("functions"), avp_json_int(a, c[0]));
        for (int t = 1; t <= NT; t++) {
            avp_buf_printf(&b, " %8lld", (long long)c[t]);
            avp_json_set(a, o, avp_str(TIERS[t - 1]), avp_json_int(a, c[t]));
        }
        avp_buf_printf(&b, " | %8lld", (long long)c[NT + 1]);
        avp_json_set(a, o, AVP_S("unpaired"), avp_json_int(a, c[NT + 1]));
        shsm_output_linez(out, avp_buf_cz(&b));
        avp_json_arr_push(a, lj, o);
    }
    shsm_output_data(out, "libraries", lj);
    shsm_output_linez(out, avp_sprintf(a, "precision on named retail functions against %s's names (same = equal"
                                          " qualified name; near = equal but for underscores/case or the class;"
                                          " default tier: >= %.0f%% same on >= %d named):",
                                       sb->name, MIN_PRECISION, MIN_NAMED));
    AvpJson *pj = avp_json_obj(a);
    for (int t = 1; t <= NT; t++) {
        shsm_output_linez(out, avp_sprintf(a, "  %-9s paired %6lld  named %5lld  same %5lld  near %4lld  differ %4lld"
                                              "  precision %6.2f%%%s",
                                           TIERS[t - 1], (long long)p.paired[t], (long long)p.named[t],
                                           (long long)p.same[t], (long long)p.near[t],
                                           (long long)(p.named[t] - p.same[t] - p.near[t]), p.prec[t],
                                           p.dflt[t] ? "  default" : p.named[t] < MIN_NAMED ? "  (too few named)"
                                                                                           : "  below 95%: not default"));
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("paired"), avp_json_int(a, p.paired[t]));
        avp_json_set(a, o, AVP_S("named"), avp_json_int(a, p.named[t]));
        avp_json_set(a, o, AVP_S("same"), avp_json_int(a, p.same[t]));
        avp_json_set(a, o, AVP_S("near"), avp_json_int(a, p.near[t]));
        avp_json_set(a, o, AVP_S("default"), avp_json_bool(p.dflt[t]));
        avp_json_set(a, pj, avp_str(TIERS[t - 1]), o);
    }
    shsm_output_data(out, "precision", pj);
    /* disagreements */
    shsm_output_linez(out, "disagreements (retail name != foreign name):");
    AvpJson *dj = avp_json_arr(a);
    for (int pass = 0; pass < 2; pass++)
        for (size_t i = 0; i < x->rows.n; i++) {
            const XRow *r = &x->rows.v[i];
            if (r->gr < 0 || sibling_is_placeholder(r->name) || (lib && strcmp(lib, r->lib) != 0)) continue;
            const char *gname = x->fns.v[r->gr].name;
            if (strncmp(gname, "proc_", 5) == 0) continue;
            const char *ag = agreement(a, r->name, gname);
            if (strcmp(ag, pass ? "near" : "differ") != 0) continue;
            shsm_output_linez(out, avp_sprintf(a, "  %-6s %-8s 0x%08llX %-10s %s != %s [%s]", ag, r->tier,
                                               (long long)r->address, r->lib, shsm_qual(a, r->name),
                                               gr_qual(a, gname), short_path(file_of(x, r->gr))));
            AvpJson *o = avp_json_obj(a);
            avp_json_set(a, o, AVP_S("kind"), avp_json_strz(a, ag));
            avp_json_set(a, o, AVP_S("tier"), avp_json_strz(a, r->tier));
            avp_json_set(a, o, AVP_S("address"), avp_json_int(a, r->address));
            avp_json_set(a, o, AVP_S("retail"), avp_json_strz(a, r->name));
            avp_json_set(a, o, AVP_S("foreign"), avp_json_strz(a, gname));
            avp_json_arr_push(a, dj, o);
        }
    shsm_output_data(out, "disagreements", dj);
    avp_map_free(libs);
    avp_vec_free(&cnt);
    avp_vec_free(&order);
    return 0;
}

/* ---- of ---- */

static int cmd_of(AvpArena *a, Sibling *sb, XState *x, AvpArgs *args, ShsmOutput *out) {
    const char *target = avp_cz(a, avp_args_str(args, "target"));
    i64 row = -1, gr = -1;
    char *end = NULL;
    const char *t = strncmp(target, "fn_", 3) == 0 ? target + 3 : target;
    unsigned long long addr = strtoull(t, &end, 16);
    for (size_t i = 0; i < x->rows.n && row < 0; i++)
        if (strcmp(x->rows.v[i].name, target) == 0 || (end && end != t && (i64)addr == x->rows.v[i].address &&
                                                        (!*end || *end == '_')))
            row = (i64)i;
    if (row < 0)
        for (size_t k = 0; k < x->fns.n && gr < 0; k++)
            if (strcmp(x->fns.v[k].name, target) == 0 ||
                (x->fns.v[k].demangled && strcmp(gr_qual(a, x->fns.v[k].name), target) == 0))
                gr = (i64)k;
    if (row < 0 && gr >= 0)
        for (size_t i = 0; i < x->rows.n; i++)
            if (x->rows.v[i].gr == gr) row = (i64)i;
    if (row < 0 && gr < 0) {
        shsm_output_linez(out, avp_sprintf(a, "%s: neither a retail nor a %s function", target, sb->name));
        return 1;
    }
    AvpJson *o = avp_json_obj(a);
    if (row >= 0) {
        const XRow *r = &x->rows.v[row];
        shsm_output_linez(out, avp_sprintf(a, "retail 0x%08llX %s (%lld bytes, %s, %s)", (long long)r->address, r->name,
                                           (long long)r->size, r->lib, *r->unit ? r->unit : "unsplit"));
        avp_json_set(a, o, AVP_S("retail"), avp_json_strz(a, r->name));
        avp_json_set(a, o, AVP_S("address"), avp_json_int(a, r->address));
        if (r->gr >= 0) {
            gr = r->gr;
            shsm_output_linez(out, avp_sprintf(a, "  tier %s: %s", r->tier, r->ev ? r->ev : ""));
            avp_json_set(a, o, AVP_S("tier"), avp_json_strz(a, r->tier));
        } else shsm_output_linez(out, avp_sprintf(a, "  no %s counterpart", sb->name));
    }
    if (gr >= 0) {
        const SibXFn *f = &x->fns.v[gr];
        shsm_output_linez(out, avp_sprintf(a, "%s 0x%08llX %s (%lld bytes)%s", sb->name, (long long)f->address, f->name,
                                           (long long)f->size, f->from_symtab ? "" : " [static: procedure table only]"));
        if (f->demangled) shsm_output_linez(out, avp_sprintf(a, "  demangled %s", f->demangled));
        shsm_output_linez(out, avp_sprintf(a, "  file %s, function %lld of the file", file_of(x, gr),
                                           (long long)f->order + 1));
        sqlite3_stmt *st = x_prep(sb->db, "select strs, floats, ints, callees, ncallers from xfn where address = ?");
        sqlite3_bind_int64(st, 1, f->address);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const char *s = col(a, st, 0);
            AvpBuf b = {0};
            avp_buf_init(&b, a);
            for (const char *p = s ? s : ""; *p; p++) avp_buf_putz(&b, *p == '\x1f' ? "\", \"" : avp_sprintf(a, "%c", *p));
            shsm_output_linez(out, avp_sprintf(a, "  strings \"%s\"", avp_buf_cz(&b)));
            shsm_output_linez(out, avp_sprintf(a, "  floats %s; ints %s", col(a, st, 1), col(a, st, 2)));
            shsm_output_linez(out, avp_sprintf(a, "  callees %s; callers %lld", col(a, st, 3),
                                               (long long)sqlite3_column_int64(st, 4)));
        }
        sqlite3_finalize(st);
        avp_json_set(a, o, AVP_S("foreign"), avp_json_strz(a, f->name));
        avp_json_set(a, o, AVP_S("foreign_address"), avp_json_int(a, f->address));
        avp_json_set(a, o, AVP_S("file"), avp_json_strz(a, file_of(x, gr)));
    }
    shsm_output_data(out, "of", o);
    return 0;
}

/* ---- names ---- */

static int cmd_names(AvpArena *a, Sibling *sb, XState *x, AvpArgs *args, ShsmOutput *out) {
    Prec p;
    measure(a, x, &p);
    unsigned bits = tiers_arg(a, args, &p);
    const char *lib = lib_arg(a, args);
    AvpBuf tl = {0};
    avp_buf_init(&tl, a);
    for (int t = 1; t <= NT; t++)
        if (bits >> t & 1u) avp_buf_printf(&tl, "%s%s", tl.n ? "," : "", TIERS[t - 1]);
    shsm_output_linez(out, avp_sprintf(a, "proposed renames from %s (tiers %s; nothing is applied; C++ names are"
                                          " printed demangled: no CodeWarrior mangling is produced)",
                                       sb->name, tl.n ? avp_buf_cz(&tl) : "none"));
    AvpJson *arr = avp_json_arr(a);
    i64 n = 0, cpp = 0;
    for (size_t i = 0; i < x->rows.n; i++) {
        const XRow *r = &x->rows.v[i];
        if (!in_tiers(r, bits) || !sibling_is_placeholder(r->name) || (lib && strcmp(lib, r->lib) != 0)) continue;
        const SibXFn *f = &x->fns.v[r->gr];
        if (strncmp(f->name, "proc_", 5) == 0) continue;
        bool is_cpp = f->demangled != NULL;
        n++;
        cpp += is_cpp;
        shsm_output_linez(out, avp_sprintf(a, "  0x%08llX %-24s -> %s%s  [%s, %s]", (long long)r->address, r->name,
                                           is_cpp ? f->demangled : f->name, is_cpp ? "  (C++)" : "", r->tier,
                                           short_path(file_of(x, r->gr))));
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("address"), avp_json_int(a, r->address));
        avp_json_set(a, o, AVP_S("old"), avp_json_strz(a, r->name));
        avp_json_set(a, o, AVP_S("new"), avp_json_strz(a, is_cpp ? f->demangled : f->name));
        avp_json_set(a, o, AVP_S("cpp"), avp_json_bool(is_cpp));
        avp_json_set(a, o, AVP_S("tier"), avp_json_strz(a, r->tier));
        avp_json_arr_push(a, arr, o);
    }
    shsm_output_linez(out, avp_sprintf(a, "%lld proposals (%lld C names transfer as they are, %lld C++ names need a"
                                          " CodeWarrior mangling)",
                                       (long long)n, (long long)(n - cpp), (long long)cpp));
    shsm_output_data(out, "names", arr);
    return 0;
}

/* ---- units ---- */

static int cmd_units(AvpArena *a, Sibling *sb, XState *x, AvpArgs *args, ShsmOutput *out) {
    Prec p;
    measure(a, x, &p);
    unsigned bits = tiers_arg(a, args, &p);
    const char *lib = lib_arg(a, args);
    /* the retail functions considered, address order */
    AVP_VEC(size_t) sel = {0};
    for (size_t i = 0; i < x->rows.n; i++)
        if (!lib || strcmp(lib, x->rows.v[i].lib) == 0) avp_vec_push(&sel, i);
    shsm_output_linez(out, avp_sprintf(a, "retail address runs per %s source file (%s; %zu retail functions)",
                                       sb->name, lib ? lib : "every library", sel.n));
    AvpJson *arr = avp_json_arr(a);
    size_t k = 0;
    i64 runs = 0, ordered_runs = 0;
    i64 first_paired = -1, last_paired = -1, paired_total = 0;
    AvpISet *files_hit = avp_iset_new();
    while (k < sel.n) {
        const XRow *r = &x->rows.v[sel.v[k]];
        if (!in_tiers(r, bits)) {
            k++;
            continue;
        }
        i32 grp = x->fns.v[r->gr].group, fd = x->fns.v[r->gr].file;
        size_t start = k, last = k;
        i64 paired = 0, unpaired_inside = 0;
        bool ordered = true;
        i64 prev_order = -1;
        for (size_t j = k; j < sel.n; j++) {
            const XRow *q = &x->rows.v[sel.v[j]];
            if (!in_tiers(q, bits)) continue;
            if (x->fns.v[q->gr].group != grp) break;
            if (x->fns.v[q->gr].order < prev_order) ordered = false;
            prev_order = x->fns.v[q->gr].order;
            last = j;
        }
        for (size_t j = start; j <= last; j++) {
            if (in_tiers(&x->rows.v[sel.v[j]], bits)) paired++;
            else unpaired_inside++;
        }
        const XRow *e = &x->rows.v[sel.v[last]];
        i64 nfile = 0;
        i64 gfirst = -1;
        for (size_t g = 0; g < x->fns.n; g++)
            if (x->fns.v[g].group == grp) {
                nfile++;
                if (gfirst < 0) gfirst = x->fns.v[g].address;
            }
        const char *path = fd >= 0 && (size_t)fd < x->files.n
                               ? avp_cz(a, x->files.v[fd])
                               : avp_sprintf(a, "(no debug info: the run at 0x%08llX)", (long long)gfirst);
        shsm_output_linez(out, avp_sprintf(a, "  0x%08llX-0x%08llX %4lld paired %3lld unpaired inside, %s order;"
                                              " file has %lld functions: %s",
                                           (long long)r->address, (long long)(e->address + e->size), (long long)paired,
                                           (long long)unpaired_inside, ordered ? "same" : "OTHER", (long long)nfile,
                                           short_path(path)));
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("start"), avp_json_int(a, r->address));
        avp_json_set(a, o, AVP_S("end"), avp_json_int(a, e->address + e->size));
        avp_json_set(a, o, AVP_S("file"), avp_json_strz(a, path));
        avp_json_set(a, o, AVP_S("paired"), avp_json_int(a, paired));
        avp_json_set(a, o, AVP_S("unpaired_inside"), avp_json_int(a, unpaired_inside));
        avp_json_set(a, o, AVP_S("ordered"), avp_json_bool(ordered));
        avp_json_set(a, o, AVP_S("file_functions"), avp_json_int(a, nfile));
        avp_json_arr_push(a, arr, o);
        runs++;
        ordered_runs += ordered;
        if (fd >= 0) avp_iset_add(files_hit, (u64)fd);
        if (first_paired < 0) first_paired = r->address;
        last_paired = e->address + e->size;
        paired_total += paired;
        k = last + 1;
    }
    shsm_output_linez(out, avp_sprintf(a, "%lld runs (%lld in file order), %lld paired functions from %zu files;"
                                          " paired extent 0x%08llX-0x%08llX",
                                       (long long)runs, (long long)ordered_runs, (long long)paired_total,
                                       avp_iset_len(files_hit), (long long)first_paired, (long long)last_paired));
    shsm_output_data(out, "units", arr);
    avp_iset_free(files_hit);
    avp_vec_free(&sel);
    return 0;
}

/* ---- dispatch ---- */

int sibling_foreign_run(Sibling *sb, AvpStr cmd, AvpArgs *args, ShsmOutput *out) {
    AvpArena *a = avp_arena();
    create_tables(sb);
    if (avp_str_eqz(cmd, "index")) return cmd_index(sb, elf_of(a, sb, args), out);
    if (avp_str_eqz(cmd, "types")) {
        const char *elf = elf_of(a, sb, args);
        Foreign *f = load_foreign(a, elf);
        if (!f->has_md) shsm_raise(SHSM_EXC_USAGE, "%s: %s", elf, f->md_note);
        const char *name = NULL;
        if (avp_args_has(args, "type") && !avp_args_is_none(args, "type")) name = avp_cz(a, avp_args_str(args, "type"));
        bool full = avp_args_has(args, "full") && avp_args_bool(args, "full");
        return sib_stabs_types(a, &f->md, elf, name, full, out);
    }
    if (avp_str_eqz(cmd, "fn") || avp_str_eqz(cmd, "cu"))
        shsm_raise(SHSM_EXC_USAGE, "%s: not available for a stabs sibling (%s); use of and types", avp_cz(a, cmd),
                   sb->name);
    if (avp_str_eqz(cmd, "apply"))
        shsm_raise(SHSM_EXC_USAGE, "apply: not available for a foreign sibling (%s): names proposes, nothing is applied",
                   sb->name);
    if (avp_str_eqz(cmd, "pair")) return cmd_pair(sb, args, out);
    XState *x = load_state(a, sb, true);
    if (avp_str_eqz(cmd, "of")) return cmd_of(a, sb, x, args, out);
    if (avp_str_eqz(cmd, "names")) return cmd_names(a, sb, x, args, out);
    if (avp_str_eqz(cmd, "units")) return cmd_units(a, sb, x, args, out);
    return cmd_stats(a, sb, x, args, out, false);
}
