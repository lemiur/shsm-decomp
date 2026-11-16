/* The content-addressed file cache (tools/common/cache.py), on the same disk format and with the same keys, so the C
 * and Python tools share entries: <root>/<key[:2]>/<key> holds the payload followed by a 48-byte stamp
 * (b"SHSMCACHE1", the payload length as 8 big-endian bytes, the payload's sha256 cut to 30 bytes). key_of hashes
 * length-prefixed parts: bytes as they are, str as UTF-8, any other value as repr() of it with lists made tuples
 * (the caller passes that repr text; shsm_repr_* build the common ones). Not ported: entries, size, prune. */
#pragma once
#include "avp/hash.h"
#include "avp/str.h"

/* ---- keys ---- */
typedef struct {
    AvpHasher h;
} ShsmKey;
void shsm_key_init(ShsmKey *k);
void shsm_key_bytes(ShsmKey *k, const void *p, size_t n);  /* a bytes part */
void shsm_key_str(ShsmKey *k, AvpStr s);                   /* a str part */
void shsm_key_strz(ShsmKey *k, const char *s);             /* a str part; NULL is None */
void shsm_key_repr(ShsmKey *k, AvpStr repr);               /* any other part: repr(_freeze(value)) */
void shsm_key_none(ShsmKey *k);                            /* None */
void shsm_key_int(ShsmKey *k, i64 v);                      /* an int */
void shsm_key_str_list(ShsmKey *k, const AvpStr *v, size_t n); /* a list or tuple of str */
/* The key: sha256 hex (64 characters) from the arena; ends the hash. */
char *shsm_key_hex(AvpArena *a, ShsmKey *k);

/* repr(tuple of str): "('a', 'b')", "('a',)", "()" */
AvpStr shsm_repr_str_tuple(AvpArena *a, const AvpStr *v, size_t n);
/* repr(tuple of (str, str) pairs), as _freeze gives a list of 2-tuples: "(('a', 'b'), ('c', 'd'))" */
AvpStr shsm_repr_str_pairs(AvpArena *a, const AvpStr *firsts, const AvpStr *seconds, size_t n);

/* cache.file_digest(path): sha256 hex of a file's contents; raises open()'s OSError (FileNotFoundError "[Errno 2]
 * No such file or directory: '...'") as SHSM_EXC_FILE_NOT_FOUND / SHSM_EXC_OS. */
char *shsm_file_digest(AvpArena *a, const char *path);

/* ---- the cache ---- */
typedef struct {
    const char *root;
    i64 hits, misses, stores; /* this object's calls */
} ShsmCache;
/* cache.default_root(): $SHSM_CACHE_DIR, else %LOCALAPPDATA%\shsm-cache, else ~/.cache/shsm-cache. */
const char *shsm_cache_default_root(AvpArena *a);
/* Cache(root): root NULL is the default root. Used by one thread at a time. */
ShsmCache *shsm_cache_new(const char *root);
const char *shsm_cache_path(AvpArena *a, const ShsmCache *c, const char *key); /* <root>/<key[:2]>/<key> */
/* get: true with the payload in *out (from a); false on a miss (a damaged entry is removed and counts as a miss). A
 * hit touches the entry's times (recency). */
bool shsm_cache_get(AvpArena *a, ShsmCache *c, const char *key, AvpStr *out);
/* put / store: write atomically (a ".tmp-" file renamed into place); return the entry path. OS errors raise. */
const char *shsm_cache_put(ShsmCache *c, const char *key, const void *data, size_t n);
const char *shsm_cache_store(ShsmCache *c, const char *key, const char *src_path);
/* restore: write the entry to dst_path atomically (creating its directory): true on a hit, false on a miss. OS
 * errors raise. */
bool shsm_cache_restore(ShsmCache *c, const char *key, const char *dst_path);
bool shsm_cache_contains(ShsmCache *c, const char *key);
bool shsm_cache_remove(ShsmCache *c, const char *key);
