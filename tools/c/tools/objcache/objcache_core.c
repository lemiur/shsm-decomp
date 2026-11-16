/* objcache: keys, the command line, the hash memo, the dependency file and the include names (tools/objcache.py,
 * "The hash memo", "The command line", "The dependency file", include_names). */
#include "objcache.h"
#include "shsm_cache.h"

#include <string.h>
#include <windows.h>

volatile long oc_files_hashed;
void (*oc_on_hash)(const char *path);

/* ================================================================================================= environment */

const char *oc_default_root(AvpArena *a) {
    return shsm_cache_default_root(a);
}

const char *oc_cache_dir(AvpArena *a) {
    return avp_path_join(a, oc_default_root(a), "objcache");
}

bool oc_enabled_value(const char *value) {
    if (!value) return true;
    AvpStr s = avp_str_strip(avp_str(value));
    if (s.n > 5) return true;
    char low[8] = {0};
    for (size_t i = 0; i < s.n; i++) {
        char c = s.p[i];
        low[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    return strcmp(low, "0") && strcmp(low, "off") && strcmp(low, "no") && strcmp(low, "false");
}

OcEnv oc_env_current(AvpArena *a) {
    AvpPtrVec v = {0};
    wchar_t *block = GetEnvironmentStringsW();
    if (block) {
        for (const wchar_t *p = block; *p; p += wcslen(p) + 1) {
            if (*p == L'=') continue; /* the per-drive "=C:" entries os.environ leaves out */
            AvpStr s = avp_from_w(a, p, -1);
            avp_vec_push(&v, (void *)avp_cz(a, s));
        }
        FreeEnvironmentStringsW(block);
    }
    const char **vars = avp_alloc(a, (v.n + 1) * sizeof(char *));
    for (size_t i = 0; i < v.n; i++) vars[i] = v.v[i];
    OcEnv e = {vars, v.n};
    avp_vec_free(&v);
    return e;
}

static bool ieq_prefix(const char *var, const char *name, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char x = var[i], y = name[i];
        if (x >= 'a' && x <= 'z') x = (char)(x - 32);
        if (y >= 'a' && y <= 'z') y = (char)(y - 32);
        if (x != y || !x) return false;
    }
    return true;
}

const char *oc_env_get(const OcEnv *e, const char *name) {
    size_t n = strlen(name);
    for (size_t i = e->n; i-- > 0;) { /* the last setting wins, as a dict update */
        const char *v = e->vars[i];
        if (ieq_prefix(v, name, n) && v[n] == '=') return v + n + 1;
    }
    return NULL;
}

/* ================================================================================================= helpers */

char *oc_norm(AvpArena *a, const char *p) {
    return avp_path_normcase(a, avp_path_normpath(a, p));
}

char *oc_sha_hex(AvpArena *a, const void *p, size_t n) {
    return avp_sha256_hex(a, p, n);
}

char *oc_file_sha(AvpArena *a, const char *path) {
    AvpBytes b = {0};
    if (!avp_read_all(a, path, &b)) return NULL;
    InterlockedIncrement(&oc_files_hashed);
    if (oc_on_hash) oc_on_hash(path);
    return oc_sha_hex(a, b.v, b.n);
}

static AvpStr rstrip_seps(const char *s) {
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\\' || s[n - 1] == '/')) n--;
    return (AvpStr){s, n};
}

char *oc_join_dep(AvpArena *a, const char *cwd, char kind, const char *p) {
    if (kind != 'r') return avp_strdup(a, p);
    AvpStr c = rstrip_seps(cwd);
    return avp_sprintf(a, "%.*s\\%s", (int)c.n, c.p, p);
}

static bool is_ascii(const char *s) {
    for (; *s; s++)
        if ((unsigned char)*s >= 0x80) return false;
    return true;
}

static bool ends_with_ci(const char *s, const char *suffix) {
    size_t n = strlen(s), m = strlen(suffix);
    return n >= m && _stricmp(s + n - m, suffix) == 0;
}

/* ================================================================================================= hash memo */

struct OcMemo {
    AvpMap *map; /* normcase absolute path -> OcMemoEntry* */
};

const char *oc_memo_path(AvpArena *a, const char *cwd) {
    ShsmKey k;
    shsm_key_init(&k);
    shsm_key_strz(&k, OC_FORMAT);
    shsm_key_strz(&k, "memo");
    shsm_key_strz(&k, oc_norm(a, avp_path_abspath(a, cwd)));
    char *hex = shsm_key_hex(a, &k);
    hex[32] = 0;
    return avp_path_joinv(a, 3, (const char *const[]){oc_cache_dir(a), "memo", hex});
}

#define MEMO_SEED 0x4F434D454D4F3121ull

OcMemo *oc_memo_load(AvpArena *a, const char *cwd) {
    OcMemo *m = AVP_NEW(a, OcMemo);
    m->map = avp_map_new();
    AvpBytes b = {0};
    size_t ml = sizeof(OC_MEMO_MAGIC) - 1;
    if (!avp_read_all(a, oc_memo_path(a, cwd), &b) || b.n < ml + 8 || memcmp(b.v, OC_MEMO_MAGIC, ml) != 0) return m;
    u64 want;
    memcpy(&want, b.v + ml, 8);
    const u8 *payload = b.v + ml + 8;
    size_t pn = b.n - ml - 8;
    if (avp_hash64(payload, pn, MEMO_SEED) != want) return m; /* damaged: every file is hashed afresh */
    AvpCacheReader r;
    avp_cr_init(&r, payload, pn);
    u32 n = avp_cr_count(&r, 4 + 8 * 4 + 4);
    for (u32 i = 0; i < n && avp_cr_ok(&r); i++) {
        AvpStr path = avp_cr_str(&r);
        OcMemoEntry *e = AVP_NEW(a, OcMemoEntry);
        e->ino = avp_cr_u64(&r);
        e->size = avp_cr_i64(&r);
        e->mtime_ns = avp_cr_i64(&r);
        e->hashed_ns = avp_cr_i64(&r);
        e->sha = avp_cz(a, avp_cr_str(&r));
        if (!avp_cr_ok(&r)) break;
        avp_map_put(m->map, (AvpStr){avp_cz(a, path), path.n}, NULL)->p = e;
    }
    if (!avp_cr_ok(&r) || avp_cr_remaining(&r)) avp_map_clear(m->map);
    return m;
}

size_t oc_memo_count(const OcMemo *m) {
    return avp_map_len(m->map);
}

const OcMemoEntry *oc_memo_find(const OcMemo *m, const char *key) {
    AvpVal *v = avp_map_get(m->map, avp_str(key));
    return v ? v->p : NULL;
}

bool oc_memo_entry_holds(const OcMemoEntry *e, u64 ino, i64 size, i64 mtime_ns) {
    return e && e->ino == ino && e->size == size && e->mtime_ns == mtime_ns && e->hashed_ns - e->mtime_ns >= OC_RACY_NS;
}

char *oc_memo_sha(AvpArena *a, OcMemo *m, const char *path) {
    if (m && avp_map_len(m->map)) {
        AvpStat st;
        if (avp_stat(path, &st)) {
            const OcMemoEntry *e = oc_memo_find(m, oc_norm(a, avp_path_abspath(a, path)));
            if (e && oc_memo_entry_holds(e, st.ino, st.size, st.mtime_ns)) return avp_strdup(a, e->sha);
        }
    }
    return oc_file_sha(a, path);
}

typedef struct {
    const char *key, *path;
    OcMemoEntry e;
    bool ok;
} PrehashItem;

static int prehash_one(void *ctx, size_t index) {
    PrehashItem *it = (PrehashItem *)ctx + index;
    AvpArena *a = avp_arena_new(0);
    AvpStat st, st2;
    if (avp_stat(it->path, &st)) {
        i64 t = avp_time_ns();
        char *sha = oc_file_sha(a, it->path);
        if (sha && avp_stat(it->path, &st2) && st.ino == st2.ino && st.size == st2.size && st.mtime_ns == st2.mtime_ns) {
            it->e = (OcMemoEntry){st.ino, st.size, st.mtime_ns, t, NULL};
            if (oc_memo_entry_holds(&it->e, st.ino, st.size, st.mtime_ns)) {
                memcpy((char *)it->key + strlen(it->key) + 1, sha, 65); /* room reserved by the caller */
                it->e.sha = it->key + strlen(it->key) + 1;
                it->ok = true;
            }
        }
    }
    avp_arena_free(a);
    return 0;
}

static void walk_files(AvpArena *a, const char *top, AvpPtrVec *out, bool exe_dll_only) {
    AvpWalk *w = avp_walk_open(top, true);
    if (!w) return;
    AvpWalkItem it;
    while (avp_walk_next(w, &it)) {
        char *dp = avp_cz(a, it.dirpath);
        for (size_t i = 0; i < it.files.n; i++) {
            char *f = avp_cz(a, it.files.v[i]);
            if (exe_dll_only && !ends_with_ci(f, ".exe") && !ends_with_ci(f, ".dll")) continue;
            avp_vec_push(out, (void *)avp_path_join(a, dp, f));
        }
    }
    avp_walk_close(w);
}

bool oc_prehash(AvpArena *a, const char *root, const char *compilers, const char *const *tops, size_t ntops, int jobs,
                OcPrehash *out) {
    static const char *const default_tops[] = {"include", "src"};
    if (!tops) {
        tops = default_tops;
        ntops = 2;
    }
    OcMemo *old = oc_memo_load(a, root);
    AvpPtrVec paths = {0};
    for (size_t i = 0; i < ntops; i++) walk_files(a, avp_path_join(a, root, tops[i]), &paths, false);
    char *bt = avp_path_joinv(a, 3, (const char *const[]){root, "build", "tools"});
    if (avp_isdir(bt)) {
        AvpStrVec names = {0};
        if (avp_listdir(a, bt, &names))
            for (size_t i = 0; i < names.n; i++) {
                char *f = avp_cz(a, names.v[i]);
                if (ends_with_ci(f, ".exe")) avp_vec_push(&paths, (void *)avp_path_join(a, bt, f));
            }
        avp_vec_free(&names);
    }
    walk_files(a, compilers ? compilers : avp_path_joinv(a, 3, (const char *const[]){root, "tools", "compilers"}),
               &paths, true);

    AvpMap *keep = avp_map_new(); /* key -> entry (kept or new), in path order */
    AVP_VEC(PrehashItem) todo = {0};
    for (size_t i = 0; i < paths.n; i++) {
        const char *p = paths.v[i];
        AvpStat st;
        if (!avp_stat(p, &st)) continue;
        char *k = oc_norm(a, avp_path_abspath(a, p));
        const OcMemoEntry *e = oc_memo_find(old, k);
        if (e && oc_memo_entry_holds(e, st.ino, st.size, st.mtime_ns)) {
            avp_map_put(keep, avp_str(k), NULL)->p = (void *)e;
        } else {
            size_t kl = strlen(k);
            char *kk = avp_alloc(a, kl + 1 + 65); /* the key, then room for the sha */
            memcpy(kk, k, kl + 1);
            avp_vec_push(&todo, ((PrehashItem){kk, p, {0}, false}));
        }
    }
    i64 kept = (i64)avp_map_len(keep);
    if (todo.n) avp_pool_for(NULL, jobs > 0 ? jobs : (avp_cpu_count() < 16 ? avp_cpu_count() : 16), prehash_one,
                             todo.v, todo.n);
    for (size_t i = 0; i < todo.n; i++)
        if (todo.v[i].ok) {
            OcMemoEntry *e = AVP_NEW(a, OcMemoEntry);
            *e = todo.v[i].e;
            avp_map_put(keep, avp_str(todo.v[i].key), NULL)->p = e;
        }

    AvpCacheWriter w;
    avp_cw_init(&w);
    avp_cw_count(&w, avp_map_len(keep));
    AvpMapIter mi = {0};
    while (avp_map_next(keep, &mi)) {
        const OcMemoEntry *e = mi.val->p;
        avp_cw_str(&w, mi.key);
        avp_cw_u64(&w, e->ino);
        avp_cw_i64(&w, e->size);
        avp_cw_i64(&w, e->mtime_ns);
        avp_cw_i64(&w, e->hashed_ns);
        avp_cw_cz(&w, e->sha);
    }
    AvpBuf file = {0};
    avp_buf_putz(&file, OC_MEMO_MAGIC);
    u64 h = avp_hash64(w.b.v, w.b.n, MEMO_SEED);
    avp_buf_putn(&file, &h, 8);
    avp_buf_putn(&file, w.b.v, w.b.n);
    avp_cw_free(&w);

    const char *path = oc_memo_path(a, root);
    avp_makedirs(avp_cz(a, avp_path_dirname(path)), true);
    char *tmp = avp_sprintf(a, "%s.%lu.tmp", path, (unsigned long)GetCurrentProcessId());
    bool ok = avp_write_atomic(tmp, file.p, file.n);
    avp_buf_free(&file);
    if (ok) {
        bool moved = false;
        for (int attempt = 0; attempt < 20 && !moved; attempt++) { /* a wrapper reading the old memo blocks it */
            moved = avp_replace(tmp, path);
            if (!moved) Sleep(50);
        }
        if (!moved) avp_remove(tmp);
    }
    out->files = kept + (i64)todo.n;
    out->hashed = (i64)todo.n;
    out->kept = kept;
    avp_vec_free(&todo);
    avp_vec_free(&paths);
    avp_map_free(keep);
    avp_map_free(old->map);
    return true;
}

/* ================================================================================================= command line */

static const char *const SOURCE_EXTS[] = {".c", ".cpp", ".cp", ".cc", ".cxx"};
static const char *const UNSUPPORTED[] = {"-precompile", "-E", "-EP", "-P", "-M", "-make", "-preprocess", "-S",
                                          "-ir", "-dis", "-disassemble", "-prefix"};

static bool is_source(const char *a) {
    if (a[0] == '-') return false;
    for (size_t i = 0; i < sizeof SOURCE_EXTS / sizeof *SOURCE_EXTS; i++)
        if (ends_with_ci(a, SOURCE_EXTS[i])) return true;
    return false;
}

static bool lower_startswith(AvpStr s, const char *prefix) {
    size_t n = strlen(prefix);
    if (s.n < n) return false;
    for (size_t i = 0; i < n; i++) {
        char c = s.p[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        if (c != prefix[i]) return false;
    }
    return true;
}

bool oc_command_parse(AvpArena *a, const char *const *argv, size_t argc, const char *cwd, OcCommand *c,
                      const char **why) {
    memset(c, 0, sizeof *c);
    c->argv = argv;
    c->argc = argc;
    c->cwd = cwd;
    size_t ci = argc;
    for (size_t i = 0; i < argc; i++)
        if (lower_startswith(avp_path_basename(argv[i]), "mwcceppc")) {
            ci = i;
            break;
        }
    if (ci == argc) return *why = "no mwcceppc in the command", false;
    c->ci = ci;
    const char *const *args = argv + ci + 1;
    size_t n = argc - ci - 1;
    size_t nc = 0, no = 0, ic = 0, io = 0;
    bool mmd = false;
    for (size_t i = 0; i < n; i++) {
        const char *x = args[i];
        if (x[0] == '@') return *why = avp_sprintf(a, "unsupported argument %s", x), false;
        for (size_t u = 0; u < sizeof UNSUPPORTED / sizeof *UNSUPPORTED; u++)
            if (!strcmp(x, UNSUPPORTED[u])) return *why = avp_sprintf(a, "unsupported argument %s", x), false;
        if (!strcmp(x, "-c") && nc++ == 0) ic = i;
        if (!strcmp(x, "-o") && no++ == 0) io = i;
        if (!strcmp(x, "-MMD") || !strcmp(x, "-MD")) mmd = true;
    }
    if (nc != 1 || no != 1) return *why = "needs exactly one -c and one -o", false;
    if (!mmd) return *why = "no -MMD/-MD dependency file", false;
    if (ic + 1 >= n || io + 1 >= n) return *why = "list index out of range", false;
    c->src = args[ic + 1];
    c->out = args[io + 1];
    c->oi = ci + 1 + io + 1;
    size_t nsrc = 0;
    bool only = true;
    for (size_t i = 0; i < n; i++)
        if (is_source(args[i])) {
            nsrc++;
            if (strcmp(args[i], c->src)) only = false;
        }
    if (nsrc != 1 || !only) return *why = "needs exactly one source file", false;
    if (ends_with_ci(c->out, ".o")) return *why = "-o names a file, not a directory", false;
    if (avp_path_isabs(c->out) || strcmp(avp_path_normpath(a, c->out), c->out) || !strncmp(c->out, "..", 2))
        return *why = "-o is not a normalised relative directory", false;
    for (size_t i = 0; i < argc; i++)
        if (!is_ascii(argv[i])) return *why = "non-ASCII path or argument", false;
    if (!is_ascii(cwd)) return *why = "non-ASCII path or argument", false;
    AvpStr root, ext;
    avp_path_splitext(avp_cz(a, avp_path_basename(c->src)), &root, &ext);
    c->obj = avp_path_join(a, c->out, avp_sprintf(a, "%.*s.o", (int)root.n, root.p));
    c->dep = avp_path_join(a, c->out, avp_sprintf(a, "%.*s.d", (int)root.n, root.p));
    for (size_t i = 0; i < n;) {
        const char *x = args[i];
        if ((!strcmp(x, "-i") || !strcmp(x, "-I") || !strcmp(x, "-I+")) && i + 1 < n) {
            avp_vec_push(&c->include_dirs, avp_str(args[i + 1]));
            i += 2;
            continue;
        }
        size_t l = strlen(x);
        if (!strncmp(x, "-I+", 3) && l > 3)
            avp_vec_push(&c->include_dirs, avp_str(x + 3));
        else if (!strncmp(x, "-I", 2) && l > 2 && strcmp(x, "-I-"))
            avp_vec_push(&c->include_dirs, avp_str(x + 2));
        i++;
    }
    return true;
}

char *oc_cmd_path(AvpArena *a, const OcCommand *c, const char *p) {
    return avp_path_join(a, c->cwd, p);
}

static int cmp_lower(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return _stricmp(((const AvpStr *)x)->p, ((const AvpStr *)y)->p);
}

const char *oc_compiler_identity(AvpArena *a, const OcCommand *c, OcMemo *memo, const char **why) {
    ShsmKey k;
    shsm_key_init(&k);
    shsm_key_strz(&k, "compiler-identity");
    for (size_t i = 0; i < c->ci; i++) {
        char *p = oc_cmd_path(a, c, c->argv[i]);
        if (avp_isfile(p)) {
            char *sha = memo ? oc_memo_sha(a, memo, p) : oc_file_sha(a, p);
            if (!sha) return *why = avp_sprintf(a, "cannot read %s", p), NULL;
            shsm_key_strz(&k, "file");
            shsm_key_strz(&k, sha);
        } else {
            shsm_key_strz(&k, "word");
            shsm_key_strz(&k, c->argv[i]);
        }
    }
    char *exe = oc_cmd_path(a, c, c->argv[c->ci]);
    if (!avp_isfile(exe)) return *why = avp_sprintf(a, "compiler not found: %s", exe), NULL;
    char *sha = memo ? oc_memo_sha(a, memo, exe) : oc_file_sha(a, exe);
    if (!sha) return *why = avp_sprintf(a, "cannot read %s", exe), NULL;
    shsm_key_strz(&k, "compiler");
    shsm_key_strz(&k, sha);
    char *d = avp_cz(a, avp_path_dirname(exe));
    AvpStrVec names = {0};
    if (!avp_listdir(a, d, &names)) return *why = avp_sprintf(a, "cannot list %s", d), NULL;
    for (size_t i = 0; i < names.n; i++) names.v[i] = avp_str(avp_cz(a, names.v[i]));
    avp_sort(names.v, names.n, sizeof *names.v, cmp_lower, NULL, false);
    for (size_t i = 0; i < names.n; i++) {
        const char *nm = names.v[i].p;
        if (!ends_with_ci(nm, ".dll")) continue;
        char *dp = avp_path_join(a, d, nm);
        char *ds = memo ? oc_memo_sha(a, memo, dp) : oc_file_sha(a, dp);
        if (!ds) return *why = avp_sprintf(a, "cannot read %s", dp), NULL;
        char *low = avp_strdup(a, nm);
        for (char *q = low; *q; q++)
            if (*q >= 'A' && *q <= 'Z') *q = (char)(*q + 32);
        shsm_key_strz(&k, "dll");
        shsm_key_strz(&k, low);
        shsm_key_strz(&k, ds);
    }
    avp_vec_free(&names);
    return shsm_key_hex(a, &k);
}

typedef struct {
    const char *name, *value;
} EnvPair;

static int cmp_pair(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const EnvPair *p = x, *q = y;
    int r = strcmp(p->name, q->name);
    return r ? r : strcmp(p->value, q->value);
}

const char *oc_manifest_key(AvpArena *a, const OcCommand *c, const char *compiler_id, AvpStr source,
                            const OcEnv *env) {
    ShsmKey k;
    shsm_key_init(&k);
    shsm_key_strz(&k, OC_FORMAT);
    shsm_key_strz(&k, "manifest");
    shsm_key_strz(&k, compiler_id);
    shsm_key_int(&k, (i64)(c->argc - c->ci - 1));
    for (size_t i = c->ci + 1; i < c->argc; i++) shsm_key_strz(&k, i == c->oi ? OC_OUT : c->argv[i]);
    AVP_VEC(EnvPair) mw = {0};
    for (size_t i = 0; i < env->n; i++) {
        const char *v = env->vars[i], *eq = strchr(v, '=');
        if (!eq || eq == v) continue;
        char *name = avp_cz(a, (AvpStr){v, (size_t)(eq - v)});
        for (char *q = name; *q; q++)
            if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
        if (strncmp(name, "MW", 2)) continue;
        avp_vec_push(&mw, ((EnvPair){name, eq + 1}));
    }
    avp_sort(mw.v, mw.n, sizeof *mw.v, cmp_pair, NULL, false);
    shsm_key_int(&k, (i64)mw.n);
    for (size_t i = 0; i < mw.n; i++) {
        shsm_key_strz(&k, mw.v[i].name);
        shsm_key_strz(&k, mw.v[i].value);
    }
    avp_vec_free(&mw);
    shsm_key_strz(&k, c->src);
    shsm_key_bytes(&k, source.p, source.n);
    return shsm_key_hex(a, &k);
}

/* ================================================================================================= dependency file */

AvpStr oc_render_depfile(AvpArena *a, const char *target, const char *src, const char *const *deps, size_t n) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_printf(&b, "%s: %s", target, src);
    for (size_t i = 0; i < n; i++) {
        avp_buf_putz(&b, " \\\r\n\t");
        for (const char *p = deps[i]; *p; p++) {
            if (*p == ' ') avp_buf_putc(&b, '\\'); /* MWCC escapes a space as '\ ' */
            avp_buf_putc(&b, *p);
        }
    }
    avp_buf_putz(&b, " \r\n");
    return avp_buf_str(&b);
}

bool oc_parse_depfile(AvpArena *a, AvpStr data, const char *src, const char **target, AvpStrVec *deps,
                      const char **why) {
    for (size_t i = 0; i < data.n; i++)
        if ((unsigned char)data.p[i] >= 0x80) return *why = "non-ASCII dependency file", false;
    if (data.n < 3 || memcmp(data.p + data.n - 3, " \r\n", 3)) return *why = "unexpected dependency file ending", false;
    AvpStr body = {data.p, data.n - 3};
    AvpStrVec items = avp_str_split(body, AVP_S(" \\\r\n"), -1);
    AvpStr head = items.v[0];
    char *tail = avp_sprintf(a, ": %s", src);
    size_t tl = strlen(tail);
    if (head.n < tl || memcmp(head.p + head.n - tl, tail, tl)) {
        avp_vec_free(&items);
        return *why = "unexpected dependency file target line", false;
    }
    *target = avp_cz(a, (AvpStr){head.p, head.n - tl});
    deps->n = 0;
    for (size_t i = 1; i < items.n; i++) {
        AvpStr it = items.v[i];
        if (!it.n || it.p[0] != '\t' || memchr(it.p, '\r', it.n) || memchr(it.p, '\n', it.n)) {
            avp_vec_free(&items);
            return *why = "unexpected dependency file line", false;
        }
        AvpBuf b;
        avp_buf_init(&b, a);
        for (size_t j = 1; j < it.n; j++) {
            if (it.p[j] == '\\' && j + 1 < it.n && it.p[j + 1] == ' ') continue;
            avp_buf_putc(&b, it.p[j]);
        }
        avp_vec_push(deps, avp_str(avp_buf_cz(&b)));
    }
    avp_vec_free(&items);
    const char **dz = avp_alloc(a, (deps->n + 1) * sizeof(char *));
    for (size_t i = 0; i < deps->n; i++) dz[i] = deps->v[i].p;
    AvpStr again = oc_render_depfile(a, *target, src, dz, deps->n);
    if (!avp_str_eq(again, data)) return *why = "dependency file does not round-trip", false;
    return true;
}

/* ================================================================================================= include names */

static bool is_word(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

/* INCLUDE_PATTERN rb'^[ \t]*#[ \t]*include[ \t]*([<"])([^>"\r\n]*)[>"]' and ANY_INCLUDE_PATTERN
 * rb"^[ \t]*#[ \t]*include\b", both re.M (a line starts at 0 and after every '\n'). */
bool oc_include_names(AvpArena *a, AvpStr data, AvpSet *names, const char **why) {
    const char *p = data.p, *end = data.p + data.n;
    size_t found = 0, any = 0;
    for (const char *ls = p; ls <= end;) {
        const char *q = ls;
        while (q < end && (*q == ' ' || *q == '\t')) q++;
        if (q < end && *q == '#') {
            q++;
            while (q < end && (*q == ' ' || *q == '\t')) q++;
            if (end - q >= 7 && !memcmp(q, "include", 7)) {
                q += 7;
                if (q == end || !is_word(*q)) any++;
                while (q < end && (*q == ' ' || *q == '\t')) q++;
                if (q < end && (*q == '<' || *q == '"')) {
                    const char *s = ++q;
                    while (q < end && *q != '>' && *q != '"' && *q != '\r' && *q != '\n') q++;
                    if (q < end && (*q == '>' || *q == '"')) {
                        found++;
                        for (const char *x = s; x < q; x++)
                            if ((unsigned char)*x >= 0x80) return *why = "non-ASCII #include name", false;
                        char *nm = avp_cz(a, (AvpStr){s, (size_t)(q - s)});
                        for (char *x = nm; *x; x++)
                            if (*x == '/') *x = '\\';
                        avp_set_add(names, avp_str(nm));
                    }
                }
            }
        }
        const char *nl = memchr(ls, '\n', (size_t)(end - ls));
        if (!nl) break;
        ls = nl + 1;
    }
    if (found != any) return *why = "computed #include", false;
    return true;
}
