/* libraries.tsv: tools/common/libraries.py. */
#include "shsm_libraries.h"

#include "avp/fmt.h"
#include "avp/fs.h"
#include "avp/map.h"
#include "avp/sort.h"
#include "shsm_cli.h"

#include <string.h>

static int cmp_row(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const ShsmLibRow *a = x, *b = y;
    if (a->start != b->start) return a->start < b->start ? -1 : 1;
    if (a->end != b->end) return a->end < b->end ? -1 : 1;
    int c = avp_str_cmp(a->library, b->library);
    return c ? c : avp_str_cmp(a->note, b->note);
}

/* str.strip() of a cell */
static AvpStr strip(AvpStr s) { return avp_str_strip(s); }

ShsmLibraries *shsm_libraries_parse(AvpArena *a, AvpStr text, const char *path) {
    ShsmLibraries *l = AVP_NEW(a, ShsmLibraries);
    l->path = path ? avp_strdup(a, path) : NULL;
    const char *where = path ? path : "libraries";
    AvpStrVec lines = avp_str_splitlines(text, false);
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr ln = lines.v[i];
        if (!avp_str_strip(ln).n || (ln.n && ln.p[0] == '#')) continue;
        while (ln.n && ln.p[ln.n - 1] == '\r') ln.n--; /* rstrip("\r") */
        AvpStrVec cells = {0};
        size_t s = 0;
        for (size_t k = 0; k <= ln.n; k++)
            if (k == ln.n || ln.p[k] == '\t') {
                avp_vec_push(&cells, avp_strn(ln.p + s, k - s));
                s = k + 1;
            }
        if (avp_str_eq(cells.v[0], AVP_S("start"))) {
            avp_vec_free(&cells);
            continue;
        }
        if (cells.n < 3) {
            avp_vec_free(&cells);
            shsm_raise(SHSM_EXC_VALUE, "%s:%zu: expected start, end, library, note", where, i + 1);
        }
        i64 st, en;
        if (!avp_parse_int(cells.v[0], 16, &st, NULL) || !avp_parse_int(cells.v[1], 16, &en, NULL)) {
            avp_vec_free(&cells);
            shsm_raise(SHSM_EXC_VALUE, "%s:%zu: bad address", where, i + 1);
        }
        ShsmLibRow r = {st, en, strip(cells.v[2]), cells.n > 3 ? strip(cells.v[3]) : AVP_S("")};
        avp_vec_push(&l->rows, r);
        avp_vec_free(&cells);
    }
    avp_sort(l->rows.v, l->rows.n, sizeof *l->rows.v, cmp_row, NULL, false);
    return l;
}

ShsmLibraries *shsm_libraries_load(AvpArena *a, const ShsmRepo *r) {
    const char *path = r->libraries_tsv;
    if (!avp_isfile(path)) {
        ShsmLibraries *l = AVP_NEW(a, ShsmLibraries);
        l->path = avp_strdup(a, path);
        return l;
    }
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TM_UTF8, &text)) shsm_raise_io();
    return shsm_libraries_parse(a, text, path);
}

typedef struct {
    bool has_stamp;
    i64 size, mtime;
    ShsmLibraries *libs;
} Loaded;

const ShsmLibraries *shsm_load_libraries(const ShsmRepo *r) {
    static AVP_THREAD_LOCAL AvpMap *cache;
    AvpArena *a = avp_arena();
    if (!cache) cache = avp_map_new();
    AvpStat st;
    bool has = avp_stat(r->libraries_tsv, &st);
    AvpStr key = avp_str_lower(a, avp_str(r->libraries_tsv)); /* os.path.normcase */
    AvpVal *v = avp_map_get(cache, key);
    Loaded *hit = v ? v->p : NULL;
    if (!hit || hit->has_stamp != has || (has && (hit->size != st.size || hit->mtime != st.mtime_ns))) {
        hit = AVP_NEW(a, Loaded);
        hit->has_stamp = has;
        hit->size = has ? st.size : 0;
        hit->mtime = has ? st.mtime_ns : 0;
        hit->libs = shsm_libraries_load(a, r);
        avp_map_put(cache, key, NULL)->p = hit;
    }
    return hit->libs;
}

const ShsmLibRow *shsm_libraries_row_of(const ShsmLibraries *l, i64 address) {
    if (address == INT64_MIN) return NULL;
    size_t lo = 0, hi = l->rows.n; /* bisect_right on the starts */
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (address < l->rows.v[mid].start) hi = mid;
        else lo = mid + 1;
    }
    if (lo == 0) return NULL;
    const ShsmLibRow *row = &l->rows.v[lo - 1];
    return row->start <= address && address < row->end ? row : NULL;
}

const char *shsm_libraries_lookup(AvpArena *a, const ShsmLibraries *l, i64 address) {
    const ShsmLibRow *row = shsm_libraries_row_of(l, address);
    return row ? avp_cz(a, row->library) : NULL;
}

const char *shsm_library_of(AvpArena *a, i64 address, const ShsmRepo *r) {
    return shsm_libraries_lookup(a, shsm_load_libraries(r), address);
}

AvpStrVec shsm_libraries_names(const ShsmLibraries *l) {
    AvpStrVec out = {0};
    AvpSet *seen = avp_set_new();
    for (size_t i = 0; i < l->rows.n; i++)
        if (avp_set_add(seen, l->rows.v[i].library)) avp_vec_push(&out, l->rows.v[i].library);
    avp_set_free(seen);
    return out;
}

AvpStrVec shsm_libraries_problems(AvpArena *a, const ShsmLibraries *l, bool have_range, i64 start, i64 end) {
    AvpStrVec out = {0};
    const ShsmLibRow *r = l->rows.v;
    size_t n = l->rows.n;
    for (size_t i = 0; i < n; i++)
        if (r[i].end <= r[i].start)
            avp_vec_push(&out, (avp_pyfmt(a, "%s: empty range 0x%08X-0x%08X", avp_cz(a, r[i].library),
                                                 r[i].start, r[i].end)));
    for (size_t i = 0; i + 1 < n; i++)
        if (r[i + 1].start < r[i].end)
            avp_vec_push(&out, (avp_pyfmt(a, "overlap: %s 0x%08X-0x%08X and %s 0x%08X-0x%08X",
                                                 avp_cz(a, r[i].library), r[i].start, r[i].end,
                                                 avp_cz(a, r[i + 1].library), r[i + 1].start, r[i + 1].end)));
    if (have_range) {
        i64 pos = start;
        for (size_t i = 0; i < n; i++) {
            if (r[i].end <= start || r[i].start >= end) continue;
            if (r[i].start > pos) avp_vec_push(&out, avp_pyfmt(a, "gap: 0x%08X-0x%08X", pos, r[i].start));
            if (r[i].end > pos) pos = r[i].end;
        }
        if (pos < end) avp_vec_push(&out, avp_pyfmt(a, "gap: 0x%08X-0x%08X", pos, end));
    }
    return out;
}

AvpStr shsm_library_group_of(const char *lib) {
    AvpStr s = avp_str(lib && *lib ? lib : "unknown");
    const char *slash = memchr(s.p, '/', s.n);
    return slash ? avp_strn(s.p, (size_t)(slash - s.p)) : s;
}
