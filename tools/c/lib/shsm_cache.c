/* Content-addressed file cache (tools/common/cache.py), disk-compatible with it. */
#include "shsm_cache.h"

#include "avp/fmt.h"
#include "avp/fs.h"
#include "shsm_cli.h"

#define MAGIC "SHSMCACHE1"
#define MAGIC_LEN 10
#define STAMP_LEN (MAGIC_LEN + 8 + 30)

/* ---- keys ---- */

void shsm_key_init(ShsmKey *k) { avp_hasher_init(&k->h, AVP_HASH_SHA256); }

void shsm_key_bytes(ShsmKey *k, const void *p, size_t n) {
    u8 len[8];
    for (int i = 0; i < 8; i++) len[i] = (u8)((u64)n >> (56 - 8 * i)); /* struct.pack(">Q", len) */
    avp_hasher_update(&k->h, len, 8);
    avp_hasher_update(&k->h, p, n);
}

void shsm_key_str(ShsmKey *k, AvpStr s) { shsm_key_bytes(k, s.p, s.n); }
void shsm_key_strz(ShsmKey *k, const char *s) {
    if (s) shsm_key_str(k, avp_str(s));
    else shsm_key_none(k);
}
void shsm_key_repr(ShsmKey *k, AvpStr repr) { shsm_key_bytes(k, repr.p, repr.n); }
void shsm_key_none(ShsmKey *k) { shsm_key_repr(k, AVP_S("None")); }

void shsm_key_int(ShsmKey *k, i64 v) {
    char buf[32];
    int n = snprintf(buf, sizeof buf, "%lld", (long long)v);
    shsm_key_bytes(k, buf, (size_t)n);
}

void shsm_key_str_list(ShsmKey *k, const AvpStr *v, size_t n) {
    AvpArena *a = avp_scratch();
    AvpArenaMark m = avp_arena_mark(a);
    shsm_key_repr(k, shsm_repr_str_tuple(a, v, n));
    avp_arena_reset(a, m);
}

char *shsm_key_hex(AvpArena *a, ShsmKey *k) {
    u8 d[AVP_SHA256_LEN];
    avp_hasher_final(&k->h, d);
    return avp_hex(a, d, sizeof d);
}

AvpStr shsm_repr_str_tuple(AvpArena *a, const AvpStr *v, size_t n) {
    AvpStr *items = AVP_NEWN(a, AvpStr, n + 1);
    for (size_t i = 0; i < n; i++) items[i] = avp_repr_str(a, v[i]);
    return avp_repr_tuple(a, items, n);
}

AvpStr shsm_repr_str_pairs(AvpArena *a, const AvpStr *firsts, const AvpStr *seconds, size_t n) {
    AvpStr *items = AVP_NEWN(a, AvpStr, n + 1);
    for (size_t i = 0; i < n; i++) {
        AvpStr pair[2] = {avp_repr_str(a, firsts[i]), avp_repr_str(a, seconds[i])};
        items[i] = avp_repr_tuple(a, pair, 2);
    }
    return avp_repr_tuple(a, items, n);
}

char *shsm_file_digest(AvpArena *a, const char *path) {
    char *h = avp_file_digest_hex(a, AVP_HASH_SHA256, path);
    if (h) return h;
    /* open(path, "rb") raised: reproduce its error */
    AvpFile *f = avp_fopen(path, "rb", AVP_TM_UTF8);
    if (!f) shsm_raise_io();
    avp_fclose(f);
    shsm_raise(SHSM_EXC_OS, "[Errno 5] Input/output error: %s", path);
}

/* ---- cache ---- */

const char *shsm_cache_default_root(AvpArena *a) {
    char *env = avp_getenv(a, "SHSM_CACHE_DIR");
    if (env && *env) return env;
    char *base = avp_getenv(a, "LOCALAPPDATA");
    if (!base || !*base) base = avp_join(a, avp_path_expanduser(a, "~"), ".cache");
    return avp_join(a, base, "shsm-cache");
}

ShsmCache *shsm_cache_new(const char *root) {
    ShsmCache *c = AVP_NEW(avp_arena(), ShsmCache);
    c->root = (root && *root) ? avp_strdup(avp_arena(), root) : shsm_cache_default_root(avp_arena());
    return c;
}

const char *shsm_cache_path(AvpArena *a, const ShsmCache *c, const char *key) {
    return avp_join(a, c->root, avp_cz(a, avp_strn(key, AVP_MIN(strlen(key), 2))), key);
}

static void make_stamp(const void *payload, size_t n, u8 out[STAMP_LEN]) {
    memcpy(out, MAGIC, MAGIC_LEN);
    for (int i = 0; i < 8; i++) out[MAGIC_LEN + i] = (u8)((u64)n >> (56 - 8 * i));
    u8 d[AVP_SHA256_LEN];
    avp_sha256(payload, n, d);
    memcpy(out + MAGIC_LEN + 8, d, 30);
}

static bool cache_read(AvpArena *a, const ShsmCache *c, const char *key, AvpStr *out) {
    const char *p = shsm_cache_path(a, c, key);
    AvpBytes data = {0};
    if (!avp_read_all(a, p, &data)) return false;
    if (data.n >= STAMP_LEN) {
        size_t n = data.n - STAMP_LEN;
        u8 stamp[STAMP_LEN];
        make_stamp(data.v, n, stamp);
        if (memcmp(stamp, data.v + n, STAMP_LEN) == 0) {
            avp_utime_now(p); /* recency for prune(); errors ignored */
            *out = avp_strn((const char *)data.v, n);
            return true;
        }
    }
    avp_remove(p); /* damaged or truncated */
    return false;
}

bool shsm_cache_get(AvpArena *a, ShsmCache *c, const char *key, AvpStr *out) {
    if (cache_read(a, c, key, out)) {
        c->hits++;
        return true;
    }
    c->misses++;
    return false;
}

/* mkstemp(prefix=".tmp-", dir=d) + write the parts + os.replace(tmp, dst); raises on an OS error. */
static void write_atomic(const char *d, const char *dst, const void *p1, size_t n1, const void *p2, size_t n2,
                         bool remove_tmp_on_error) {
    AvpArena *a = avp_arena();
    char *tmp = NULL;
    AvpFile *f = avp_mkstemp(a, ".tmp-", "", d, &tmp);
    if (!f) shsm_raise_io();
    bool ok = avp_fwrite(f, p1, n1) && (!n2 || avp_fwrite(f, p2, n2));
    ok = avp_fclose(f) && ok;
    if (ok && avp_replace(tmp, dst)) return;
    int exc = shsm_io_exc_code();
    AvpStr msg = avp_io_err_str(a);
    if (remove_tmp_on_error) avp_remove(tmp);
    shsm_raise(exc, "%.*s", AVP_SF(msg));
}

const char *shsm_cache_put(ShsmCache *c, const char *key, const void *data, size_t n) {
    AvpArena *a = avp_arena();
    const char *p = shsm_cache_path(a, c, key);
    const char *d = avp_cz(a, avp_path_dirname(p));
    if (!avp_makedirs(d, true)) shsm_raise_io();
    u8 stamp[STAMP_LEN];
    make_stamp(data, n, stamp);
    write_atomic(d, p, data, n, stamp, STAMP_LEN, true);
    c->stores++;
    return p;
}

const char *shsm_cache_store(ShsmCache *c, const char *key, const char *src_path) {
    AvpBytes data = {0};
    if (!avp_read_all(NULL, src_path, &data)) shsm_raise_io();
    const char *p = shsm_cache_put(c, key, data.v, data.n);
    avp_vec_free(&data);
    return p;
}

bool shsm_cache_restore(ShsmCache *c, const char *key, const char *dst_path) {
    AvpArena *a = avp_arena();
    AvpStr data;
    if (!shsm_cache_get(a, c, key, &data)) return false;
    const char *d = avp_cz(a, avp_path_dirname(avp_path_abspath(a, dst_path)));
    if (!avp_makedirs(d, true)) shsm_raise_io();
    write_atomic(d, dst_path, data.p, data.n, NULL, 0, false);
    return true;
}

bool shsm_cache_contains(ShsmCache *c, const char *key) {
    AvpArena *a = avp_scratch();
    AvpArenaMark m = avp_arena_mark(a);
    AvpStr out;
    bool r = cache_read(a, c, key, &out);
    avp_arena_reset(a, m);
    return r;
}

bool shsm_cache_remove(ShsmCache *c, const char *key) {
    AvpArena *a = avp_scratch();
    AvpArenaMark m = avp_arena_mark(a);
    bool r = avp_remove(shsm_cache_path(a, c, key));
    avp_arena_reset(a, m);
    return r;
}
