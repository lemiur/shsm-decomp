/* classes_stamp.c: the scan driver of classes.py (check_inputs, scanner_digest, content_key, _cache, _current,
 * run_scan) around the scan engine of classes_scan.h. The scanner digest and the cache-key prefix are the C tool's
 * own (DEVIATIONS.md D-classes-1). */
#include "classes.h"
#include "classes_scan.h"
#include "shsm_cache.h"
#include "shsm_cli.h"

#include <string.h>

static bool ends_s(AvpStr fn) { return fn.n >= 2 && fn.p[fn.n - 2] == '.' && fn.p[fn.n - 1] == 's'; }

void classes_check_inputs(const char *asm_dir, const char *symbols) {
    if (!avp_isdir(asm_dir))
        shsm_raise(SHSM_EXC_USAGE, "asm directory not found: %s (run ninja to split the DOL, or pass --asm)", asm_dir);
    bool any = false;
    AvpWalk *w = avp_walk_open(asm_dir, true);
    if (w) {
        AvpWalkItem it = {0};
        while (!any && avp_walk_next(w, &it))
            for (size_t i = 0; i < it.files.n && !any; i++) any = ends_s(it.files.v[i]);
        avp_walk_close(w);
    }
    if (!any) shsm_raise(SHSM_EXC_USAGE, "no .s files under the asm directory: %s", asm_dir);
    if (!avp_isfile(symbols)) shsm_raise(SHSM_EXC_USAGE, "symbols file not found: %s (pass --symbols)", symbols);
}

static char *hex_digest(AvpArena *a, AvpHasher *h) {
    u8 d[32];
    avp_hasher_final(h, d);
    char *s = avp_alloc(a, 65);
    for (int i = 0; i < 32; i++) snprintf(s + 2 * i, 3, "%02x", d[i]);
    return s;
}

static void update_file(AvpHasher *h, const char *path) {
    AvpFileView v;
    if (!avp_view_open(path, &v)) shsm_raise_io();
    avp_hasher_update(h, v.p, v.n);
    avp_view_close(&v);
}

int classes_default_base_rule(AvpArena *a) {
    const char *v = avp_getenv_or(a, "SHSM_CLASSES_BASE_RULE", "");
    return strcmp(v, "slots") == 0 ? CLASSES_BASE_RULE_SLOTS : CLASSES_BASE_RULE_CTOR;
}

char *classes_scanner_digest(AvpArena *a, const char *splits) {
    AvpHasher h;
    avp_hasher_init(&h, AVP_HASH_SHA256);
    avp_hasher_update(&h, "classes-c\0", 10);
    const char *stamp = avp_tools_stamp();
    avp_hasher_update(&h, stamp, strlen(stamp));
    avp_hasher_update(&h, "\0", 1);
    if (splits && *splits && avp_isfile(splits)) update_file(&h, splits);
    return hex_digest(a, &h);
}

static void put_len(AvpHasher *h, u64 n) {
    u8 b[8];
    for (int i = 0; i < 8; i++) b[i] = (u8)(n >> (8 * (7 - i)));
    avp_hasher_update(h, b, 8);
}

typedef struct {
    AvpStr rel;
    const char *full;
} SFile;
static int cmp_sfile(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const SFile *p = x, *q = y;
    int c = avp_str_cmp(p->rel, q->rel);
    return c ? c : strcmp(p->full, q->full);
}

char *classes_content_key(AvpArena *a, const char *asm_dir, const char *symbols, const char *scanner) {
    AvpHasher h;
    avp_hasher_init(&h, AVP_HASH_SHA256);
    avp_hasher_update(&h, "classes-scan-c1\0", 16);
    avp_hasher_update(&h, scanner, strlen(scanner));
    avp_hasher_update(&h, "\0", 1);
    AvpFileView v;
    if (!avp_view_open(symbols, &v)) shsm_raise_io();
    put_len(&h, v.n);
    avp_hasher_update(&h, v.p, v.n);
    avp_view_close(&v);
    AVP_VEC(SFile) files = {0};
    AvpWalk *w = avp_walk_open(asm_dir, true);
    if (w) {
        AvpWalkItem it = {0};
        while (avp_walk_next(w, &it)) {
            for (size_t i = 0; i < it.files.n; i++) {
                if (!ends_s(it.files.v[i])) continue;
                char *full = avp_path_join(a, avp_cz(a, it.dirpath), avp_cz(a, it.files.v[i]));
                char *rel = NULL;
                if (!avp_path_relpath(a, full, asm_dir, &rel)) {
                    avp_walk_close(w);
                    shsm_raise_io();
                }
                SFile f = {avp_str_replace(a, avp_str(rel), AVP_S("\\"), AVP_S("/"), -1), full};
                avp_vec_push(&files, f);
            }
        }
        avp_walk_close(w);
    }
    avp_sort(files.v, files.n, sizeof *files.v, cmp_sfile, NULL, false);
    for (size_t i = 0; i < files.n; i++) {
        if (!avp_view_open(files.v[i].full, &v)) {
            avp_vec_free(&files);
            shsm_raise_io();
        }
        put_len(&h, files.v[i].rel.n);
        avp_hasher_update(&h, files.v[i].rel.p, files.v[i].rel.n);
        put_len(&h, v.n);
        avp_hasher_update(&h, v.p, v.n);
        avp_view_close(&v);
    }
    avp_vec_free(&files);
    return hex_digest(a, &h);
}

AvpJson *classes_current(AvpArena *a, const char *out, const AvpJson *stamp, const char *scanner) {
    AvpBytes raw = {0};
    if (!avp_read_all(a, out, &raw)) return NULL; /* OSError */
    AvpJson *db = avp_json_parse(a, avp_strn((const char *)raw.v, raw.n), &(AvpJsonError){0});
    if (!db) return NULL; /* ValueError (UnicodeDecodeError included) */
    const AvpJson *rec = avp_json_is_obj(db) ? avp_json_getz(db, "inputs") : NULL;
    if (!avp_json_is_obj(rec)) return NULL;
    const AvpJson *sc = avp_json_getz(rec, "scanner");
    if (!sc || sc->kind != AVP_JSON_STRING || !avp_str_eqz(sc->u.str, scanner)) return NULL;
    ClassesStampKey k1, k2;
    if (!classes_stamp_key(rec, &k1)) return NULL;
    classes_stamp_key(stamp, &k2);
    if (!classes_stamp_key_eq(&k1, &k2)) return NULL;
    if (!avp_json_is_obj(avp_json_getz(db, "stats"))) return NULL;
    return db;
}

AvpStr classes_dump_file_text(AvpArena *a, const AvpJson *doc) {
    AvpJsonDumpOpts o = AVP_JSON_INDENT(1);
    AvpStr text = avp_json_dumps(a, doc, &o);
    AvpStr bytes;
    if (!avp_encode_text(a, text, AVP_TM_UTF8, &bytes)) shsm_raise_io();
    return bytes;
}

/* str(v) of a statistics value: a string as it is, anything else as its repr */
static void py_repr(AvpBuf *b, const AvpJson *v) {
    switch (v ? v->kind : AVP_JSON_NULL) {
    case AVP_JSON_NULL: avp_buf_putz(b, "None"); break;
    case AVP_JSON_BOOL: avp_buf_putz(b, v->b ? "True" : "False"); break;
    case AVP_JSON_INT:
        if (v->big) avp_buf_put(b, v->u.str);
        else avp_buf_printf(b, "%lld", (long long)v->u.i);
        break;
    case AVP_JSON_FLOAT: {
        char f[32];
        avp_float_repr(v->u.f, f);
        avp_buf_putz(b, f);
        break;
    }
    case AVP_JSON_STRING: avp_buf_repr_str(b, v->u.str); break;
    case AVP_JSON_ARRAY:
        avp_buf_putc(b, '[');
        for (size_t i = 0; i < v->u.arr.n; i++) {
            if (i) avp_buf_putz(b, ", ");
            py_repr(b, v->u.arr.v[i]);
        }
        avp_buf_putc(b, ']');
        break;
    default:
        avp_buf_putc(b, '{');
        for (size_t i = 0; i < v->u.obj.n; i++) {
            const AvpJsonMember *m = avp_json_member(v, i);
            if (i) avp_buf_putz(b, ", ");
            avp_buf_repr_str(b, m->key);
            avp_buf_putz(b, ": ");
            py_repr(b, m->val);
        }
        avp_buf_putc(b, '}');
        break;
    }
}

AvpJson *classes_run_scan(AvpArena *a, const ClassesScanArgs *args, bool quiet) {
    const ClassesPaths *p = &args->paths;
    classes_check_inputs(p->asm_dir, p->symbols);
    AvpJson *stamp = classes_input_stamp(a, p->asm_dir, p->symbols);
    /* the ctor rule (D-classes-2) records its own scanner value, so neither rule's classes.json counts as current
     * for the other; the content key composes the scanner value, so each rule has its own cache entries */
    bool use_cache = !args->no_cache && strcmp(avp_getenv_or(a, "SHSM_CLASSES_CACHE", "1"), "0") != 0;
    char *scanner = classes_scanner_digest(a, p->splits);
    if (args->base_rule == CLASSES_BASE_RULE_CTOR) scanner = avp_sprintf(a, "%s+base-rule=ctor", scanner);
    avp_json_set(a, stamp, AVP_S("scanner"), avp_json_strz(a, scanner));
    const char *how = "", *note = NULL;
    AvpJson *db = use_cache ? classes_current(a, p->out, stamp, scanner) : NULL;
    if (db) {
        how = " (inputs unchanged since the last scan: kept)";
    } else {
        char *key = NULL;
        ShsmCache *cache = NULL;
        if (use_cache) {
            key = classes_content_key(a, p->asm_dir, p->symbols, scanner);
            cache = shsm_cache_new(NULL);
            AvpStr data;
            if (shsm_cache_get(a, cache, key, &data)) {
                AvpJson *d = avp_json_parse(a, data, &(AvpJsonError){0});
                if (avp_json_is_obj(d)) {
                    avp_json_set(a, d, AVP_S("inputs"), stamp);
                    db = d;
                    how = " (restored from the cache)";
                }
            }
        }
        if (!db) {
            ClassesScanOpts so = {.asm_dir = p->asm_dir,
                                  .symbols_path = p->symbols,
                                  .splits_path = p->splits && *p->splits && avp_isfile(p->splits) ? p->splits : NULL,
                                  .base_rule = args->base_rule,
                                  .note = &note};
            db = classes_scan(a, &so, stamp);
            if (note && !quiet) fprintf(stderr, "note: %s\n", note);
            if (cache) {
                AvpStr payload = avp_json_dumps(a, db, NULL);
                AvpTry t;
                if (AVP_TRY(&t)) {
                    shsm_cache_put(cache, key, payload.p, payload.n);
                    avp_try_end(&t);
                } else if (!shsm_exc_is_os(t.code)) {
                    avp_throw(t.code, "%s", t.msg ? t.msg : "");
                }
            }
        }
        char *dir = avp_cz(a, avp_path_dirname(avp_path_abspath(a, p->out)));
        if (!avp_makedirs(dir, true)) shsm_raise_io();
        AvpStr bytes = classes_dump_file_text(a, db);
        if (!avp_write_file(p->out, bytes.p, bytes.n)) shsm_raise_io();
    }
    if (!quiet) {
        AvpJson *stats = avp_json_getz(db, "stats");
        AvpBuf b;
        avp_buf_init(&b, a);
        if (args->json) {
            AvpJsonDumpOpts o = AVP_JSON_INDENT(1);
            avp_json_dump_buf(&b, stats, &o);
            avp_buf_putz(&b, "\n");
        } else {
            for (size_t i = 0; i < avp_json_len(stats); i++) {
                const AvpJsonMember *m = avp_json_member(stats, i);
                AvpBuf v;
                avp_buf_init(&v, a);
                if (m->val->kind == AVP_JSON_STRING) avp_buf_put(&v, m->val->u.str);
                else py_repr(&v, m->val);
                AvpStr vs = avp_buf_str(&v);
                avp_buf_pyfmt(&b, "%-32S %S\n", &m->key, &vs);
            }
            char *shown = NULL;
            if (!args->root || !avp_path_relpath(a, p->out, args->root, &shown)) shown = (char *)p->out;
            avp_buf_pyfmt(&b, "wrote %s%s\n", shown, how);
        }
        fwrite(b.p ? b.p : "", 1, b.n, stdout);
    }
    return db;
}
