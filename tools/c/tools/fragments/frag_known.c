/* fragments.py known helpers: scan_listing, _site_hash, _site_display, _site_savekey, KNOWN, known_sites. The line
 * patterns are the same code as witness's private copy (tools/c/tools/witness/witness_known.c), which can fold onto
 * fragments_known.h. */
#include "fragments_known.h"

#include "avp/fmt.h"
#include "avp/fs.h"
#include "shsm_cli.h"
#include "shsm_asmindex.h"
#include "sqlite3.h"

#include <string.h>



typedef struct Row {
    i64 addr;
    AvpStr mn, ops;
} Row;
typedef AVP_VEC(Row) Rows;
typedef AVP_VEC(Rows) RowsList;

/* str.isspace for the bytes a listing can hold (ASCII whitespace and the separators \x1c-\x1f); see D-witness-6 */
static bool is_space(char c) {
    unsigned char u = (unsigned char)c;
    return u == ' ' || (u >= 0x09 && u <= 0x0D) || (u >= 0x1C && u <= 0x1F);
}

static bool is_hex_upper(char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'); }

/* _FN = r'^\.fn\s+("[^"]*"|\S+?),' -> the name (group 1 stripped of '"'), or false */
static bool match_fn(AvpStr line, AvpStr *name) {
    if (line.n < 4 || memcmp(line.p, ".fn", 3) != 0 || !is_space(line.p[3])) return false;
    size_t i = 3;
    while (i < line.n && is_space(line.p[i])) i++;
    /* \s+ backtracks, but neither alternative can start with whitespace, so the run is taken whole */
    size_t s = i;
    if (s < line.n && line.p[s] == '"') {
        size_t j = s + 1;
        while (j < line.n && line.p[j] != '"') j++;
        if (j < line.n && j + 1 < line.n && line.p[j + 1] == ',') {
            *name = avp_str_strip_chars(avp_strn(line.p + s, j + 1 - s), AVP_S("\""));
            return true;
        }
    }
    /* \S+?, : the shortest run of non-space characters followed by a comma */
    size_t j = s;
    while (j < line.n && !is_space(line.p[j])) {
        if (line.p[j] == ',' && j > s) {
            *name = avp_str_strip_chars(avp_strn(line.p + s, j - s), AVP_S("\""));
            return true;
        }
        j++;
    }
    return false;
}

/* _INSN on line.rstrip("\n") -> (address, mnemonic, operands stripped) */
static bool match_insn(AvpStr line, Row *row) {
    static const char pat[] = "/* XXXXXXXX XXXXXXXX  ___________ */\t";
    size_t plen = sizeof pat - 1;
    if (line.n < plen + 1) return false;
    for (size_t k = 0; k < plen; k++) {
        char c = line.p[k], want = pat[k];
        if (want == 'X') {
            if (!is_hex_upper(c)) return false;
        } else if (want == '_') {
            if (!is_hex_upper(c) && c != ' ') return false;
        } else if (c != want) {
            return false;
        }
    }
    for (size_t k = plen; k < line.n; k++)
        if (line.p[k] == '\n') return false; /* '.' does not match a newline */
    size_t i = plen;
    if (is_space(line.p[i])) return false; /* (\S+) needs one character */
    size_t m0 = i;
    while (i < line.n && !is_space(line.p[i])) i++;
    u64 addr = 0;
    avp_parse_uint(avp_strn(line.p + 3, 8), 16, &addr);
    row->addr = (i64)addr;
    row->mn = avp_strn(line.p + m0, i - m0);
    row->ops = avp_str_strip(avp_strn(line.p + i, line.n - i));
    return true;
}

static bool eqz(AvpStr s, const char *z) { return avp_str_eqz(s, z); }

/* rows[lo:hi] with Python slice clamping */
static void span(size_t n, i64 lo, i64 hi, size_t *a, size_t *b) {
    if (lo < 0) lo = 0;
    if (hi > (i64)n) hi = (i64)n;
    if (hi < lo) hi = lo;
    *a = (size_t)lo;
    *b = (size_t)hi;
}

/* o.replace(" ", "") */
static AvpStr nospace(AvpArena *a, AvpStr o) { return avp_str_replace(a, o, AVP_S(" "), AVP_S(""), -1); }

static bool contains(AvpStr hay, const char *needle) { return avp_str_find(hay, avp_str(needle), 0, AVP_END) >= 0; }

static bool site_hash(AvpArena *a, const Row *rows, size_t n, size_t i) {
    if (!eqz(rows[i].mn, "extsb")) return false;
    size_t lo, hi;
    span(n, (i64)i - 2, (i64)i + 3, &lo, &hi);
    bool xr = false, sl = false;
    for (size_t k = lo; k < hi; k++)
        if (eqz(rows[k].mn, "xor")) xr = true;
    for (size_t k = lo; k < hi; k++)
        if (eqz(rows[k].mn, "slwi") && avp_str_endswith(nospace(a, rows[k].ops), AVP_S(",1"))) sl = true;
    return xr && sl;
}

static bool display_load(const Row *r) { return eqz(r->mn, "lwz") && contains(r->ops, "lbl_8052861C"); }

static bool site_display(const Row *rows, size_t n, size_t i) {
    if (!eqz(rows[i].mn, "lwz") || !contains(rows[i].ops, "lbl_8052861C")) return false;
    size_t lo, hi;
    span(n, (i64)i - 4, (i64)i, &lo, &hi);
    for (size_t k = lo; k < hi; k++)
        if (display_load(&rows[k])) return false;
    span(n, (i64)i + 1, (i64)i + 5, &lo, &hi);
    bool after = false;
    for (size_t k = lo; k < hi; k++)
        if (display_load(&rows[k])) after = true;
    if (!after) return false;
    span(n, (i64)i - 6, (i64)i + 7, &lo, &hi);
    for (size_t k = lo; k < hi; k++)
        if (eqz(rows[k].mn, "xoris")) return true;
    return false;
}

static bool site_savekey(AvpArena *a, const Row *rows, size_t n, size_t i) {
    if (!eqz(rows[i].mn, "li") || !avp_str_endswith(nospace(a, rows[i].ops), AVP_S(",0x1"))) return false;
    AvpStrVec xs = avp_str_split(rows[i].ops, AVP_S(","), -1);
    AvpStr x = xs.v[0];
    avp_vec_free(&xs);
    size_t lo, hi;
    span(n, (i64)i - 1, (i64)i + 3, &lo, &hi);
    const Row *w = rows + lo;
    size_t wn = hi - lo;
    if (wn < 3 || !eqz(w[0].mn, "lwz")) return false;
    AvpStrVec ks = avp_str_split(w[0].ops, AVP_S(","), -1);
    AvpStr key = ks.v[0];
    avp_vec_free(&ks);
    AvpStr want_key = avp_pyfmt(a, "%S,0x0", &key), want_x = avp_pyfmt(a, "%S,0x0", &x);
    bool has_key = false, has_x = false;
    for (size_t k = 2; k < wn; k++) {
        if (!eqz(w[k].mn, "cmpwi")) continue;
        AvpStr c = nospace(a, w[k].ops);
        if (avp_str_eq(c, want_key)) has_key = true;
        if (avp_str_eq(c, want_x)) has_x = true;
    }
    if (!has_key) return false;
    return has_x || (wn > 3 && eqz(w[3].mn, "blr")) || wn == 3;
}

/* scan_listing(path): {function name: rows}, names in first-seen order (a repeated .fn appends to its rows) */
static void scan_listing(AvpArena *a, const char *path, AvpMap *names, RowsList *lists) {
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TM_UTF8_REPLACE, &text)) shsm_raise_io();
    Rows *cur = NULL;
    size_t pos = 0;
    while (pos < text.n) {
        const char *nl = memchr(text.p + pos, '\n', text.n - pos);
        size_t end = nl ? (size_t)(nl - text.p) + 1 : text.n;
        AvpStr line = avp_strn(text.p + pos, end - pos); /* with its "\n", as `for line in fh` gives it */
        pos = end;
        AvpStr name;
        if (match_fn(line, &name)) {
            bool created;
            AvpVal *v = avp_map_put(names, name, &created);
            if (created) {
                v->i = (i64)lists->n;
                avp_vec_push_zero(lists);
            }
            cur = &lists->v[v->i];
            continue;
        }
        if (avp_str_startswith(line, AVP_S(".endfn"))) {
            cur = NULL;
            continue;
        }
        if (cur) {
            AvpStr body = avp_str_rstrip_chars(line, AVP_S("\n"));
            Row row;
            if (match_insn(body, &row)) avp_vec_push(cur, row);
        }
    }
}

const FragmentsKnown FRAGMENTS_KNOWN[FRAGMENTS_NKNOWN] = {
    {"string hash", -1,
     "inlined `hash = ( hash << 1 ) ^ *c` loop (HashString, src/climax/EventHandler.cpp); count not stated"},
    {"display-height pair", 76, "include/game/DisplayHeight.h; AGENTS.md: opens about 76 game static initialisers"},
    {"save-data key check", 5,
     "CheckSaveDataKey, include/game/SaveData.h; docs: five sites (plus the out-of-line fn_800C63AC)"},
};

static AvpStrVec listing_paths(AvpArena *a, struct ShsmAsmIndex *ix) {
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(ix->db, "select distinct path from files", -1, &st, NULL) != SQLITE_OK)
        shsm_raise(SHSM_EXC_SQLITE, "%s", avp_strdup(a, sqlite3_errmsg(ix->db)));
    AvpStrVec paths = {0};
    int rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        const char *p = (const char *)sqlite3_column_text(st, 0);
        avp_vec_push(&paths, avp_str(avp_strdup(a, p ? p : "")));
    }
    if (rc != SQLITE_DONE) {
        const char *msg = avp_strdup(a, sqlite3_errmsg(ix->db));
        sqlite3_finalize(st);
        avp_vec_free(&paths);
        shsm_raise(SHSM_EXC_SQLITE, "%s", msg);
    }
    sqlite3_finalize(st);
    return paths;
}

void fragments_known_sites(AvpArena *a, const ShsmRepo *r, struct ShsmAsmIndex *ix,
                           FragmentsKnownSites out[FRAGMENTS_NKNOWN]) {
    for (int k = 0; k < FRAGMENTS_NKNOWN; k++) memset(&out[k], 0, sizeof out[k]);
    AvpStrVec paths = listing_paths(a, ix);
    for (size_t f = 0; f < paths.n; f++) {
        AvpArena *t = avp_arena_new(0);
        AvpMap *names = avp_map_new();
        RowsList lists = {0};
        scan_listing(t, avp_path_join(t, r->asm_dir, avp_cz(t, paths.v[f])), names, &lists);
        AvpStrVec keys = avp_map_keys(names);
        for (size_t k = 0; k < keys.n; k++) {
            const Rows *rows = &lists.v[avp_map_get(names, keys.v[k])->i];
            AvpStr fn = {NULL, 0};
            for (size_t i = 0; i < rows->n; i++) {
                bool hit[FRAGMENTS_NKNOWN];
                hit[0] = site_hash(t, rows->v, rows->n, i);
                hit[1] = site_display(rows->v, rows->n, i);
                hit[2] = site_savekey(t, rows->v, rows->n, i);
                for (int h = 0; h < FRAGMENTS_NKNOWN; h++) {
                    if (!hit[h]) continue;
                    if (!fn.p) fn = avp_str_dup(a, keys.v[k]);
                    FragmentsKnownSite s = {fn, rows->v[i].addr, rows->v[i].addr - rows->v[0].addr};
                    avp_vec_push(&out[h], s);
                }
            }
        }
        avp_vec_free(&keys);
        for (size_t k = 0; k < lists.n; k++) avp_vec_free(&lists.v[k]);
        avp_vec_free(&lists);
        avp_map_free(names);
        avp_arena_free(t);
    }
    avp_vec_free(&paths);
}

void fragments_known_functions(AvpArena *a, const ShsmRepo *r, struct ShsmAsmIndex *ix,
                               AvpSet *out[FRAGMENTS_NKNOWN]) {
    FragmentsKnownSites sites[FRAGMENTS_NKNOWN];
    fragments_known_sites(a, r, ix, sites);
    for (int k = 0; k < FRAGMENTS_NKNOWN; k++) {
        out[k] = avp_set_new();
        for (size_t i = 0; i < sites[k].n; i++) avp_set_add(out[k], sites[k].v[i].fn);
        avp_vec_free(&sites[k]);
    }
}
