/* objcache.exe: the shared object cache for the MWCC compile step (tools/objcache.py). `run -- <command>` is the hot
 * path (no argument parser is built); stats, prune, clear, verify and prehash are the maintenance subcommands. */
#include "objcache.h"
#include "shsm_cache.h"
#include "shsm_cli.h"
#include "shsm_repo.h"

#include <string.h>
#include <windows.h>

static const char *const SUB_NAMES[] = {"run", "stats", "prune", "clear", "verify", "prehash"};
static const char *const SUB_HELP[] = {
    "run -- <compile command>: the wrapper in front of the compiler (configure.py --objcache)",
    "entries, size, hits and misses (this checkout's current build/ and all builds)",
    "drop least recently used entries",
    "delete every object cache entry",
    "recompile cached entries fresh and compare bytes; exit 1 (and the bad entry deleted) on a difference",
    "hash every source, header and compiler file once into this checkout's hash memo, so that the wrappers' hits "
    "only look hashes up (the gate and the ninja build run it before compiling)",
};

static const char EPILOG[] =
    "Usage:\n"
    "    build/tools/objcache.exe run -- <compile command>     the wrapper the ninja MWCC rules put in front of\n"
    "                                                          the compiler (configure.py --objcache)\n"
    "    build/tools/objcache.exe stats [--root DIR] [--json]  entries, size, hits and misses (this checkout's\n"
    "                                                          current build/ and all builds)\n"
    "    build/tools/objcache.exe prune [--max-size GB]        drop least recently used entries (default 2 GB)\n"
    "    build/tools/objcache.exe clear                        delete every object cache entry\n"
    "    build/tools/objcache.exe prehash [--compilers DIR] [--stamp FILE]\n"
    "                                                          hash every source, header and compiler file into\n"
    "                                                          this checkout's hash memo (the build's first step)\n"
    "    build/tools/objcache.exe verify [--sample N | --all] [-j N] [--root DIR]\n"
    "                                                          recompile cached entries fresh and compare bytes;\n"
    "                                                          exit 1 (and the bad entry deleted) on a difference\n"
    "\n"
    "Environment: SHSM_OBJCACHE=0 bypasses the cache entirely (the command runs unchanged, nothing is read or\n"
    "written); SHSM_CACHE_DIR moves the shared cache root (entries live in <root>/objcache, default\n"
    "%LOCALAPPDATA%\\shsm-cache\\objcache, shared by every worktree on the machine).";

static AvpArgParser *build_parser(void) {
    AvpArgParser *ap = avp_args_parser(
        NULL, &(AvpArgParserDesc){.prog = "objcache.exe",
                                  .description = "Shared object cache for the MWCC compile step (ccache's \"direct "
                                                 "mode\").",
                                  .epilog = EPILOG, .formatter = AVP_FORMAT_RAW_DESCRIPTION});
    AvpArgSubparsers *sub = avp_args_subparsers(ap, &(AvpArgSubparsersDesc){.dest = "cmd"});
    avp_args_subparser(sub, "run",
                       &(AvpArgParserDesc){.help = SUB_HELP[0], .description = SUB_HELP[0],
                                           .usage = "objcache.exe run [-h] -- <compile command>"});
    for (size_t i = 1; i < 6; i++)
        avp_args_subparser(sub, SUB_NAMES[i], &(AvpArgParserDesc){.help = SUB_HELP[i], .description = SUB_HELP[i]});
    return ap;
}

static AvpArgParser *sub_parser(size_t i) {
    AvpArena *a = avp_arena();
    const char *name = SUB_NAMES[i];
    AvpArgParser *ap = shsm_cli_parser(avp_sprintf(a, "objcache.exe %s", name),
                                       avp_sprintf(a, "objcache %s: %s", name, SUB_HELP[i]), true, true, false);
    if (!strcmp(name, "prune")) {
        avp_arg(ap, &(AvpArgDesc){.names = "--max-size", .type = AVP_T_FLOAT, .dflt = AVP_DEF_FLOAT(OC_DEFAULT_MAX_GB),
                                  .metavar = "GB", .help = "target total size (default 2)"});
    } else if (!strcmp(name, "verify")) {
        AvpArgMutex *g = avp_args_mutex(ap, false);
        avp_arg_mutex(g, &(AvpArgDesc){.names = "--sample", .type = AVP_T_INT, .dflt = AVP_DEF_INT(20), .metavar = "N",
                                       .help = "check N random entries (default 20)"});
        avp_arg_mutex(g, &(AvpArgDesc){.names = "--all", .action = AVP_ARG_STORE_TRUE, .help = "check every entry"});
        avp_arg(ap, &(AvpArgDesc){.names = "-j --jobs", .type = AVP_T_INT, .dflt = AVP_DEF_NONE,
                                  .help = "parallel compiles (default: CPUs)"});
    } else if (!strcmp(name, "prehash")) {
        avp_arg(ap, &(AvpArgDesc){.names = "--compilers", .dflt = AVP_DEF_NONE,
                                  .help = "compilers directory (default: <root>/tools/compilers)"});
        avp_arg(ap, &(AvpArgDesc){.names = "--stamp", .dflt = AVP_DEF_NONE,
                                  .help = "touch this file when done (the ninja edge's output)"});
    }
    return ap;
}

static const char *root_arg(AvpArena *a, AvpArgs *args) {
    AvpStr r = avp_args_str(args, "root");
    return r.p ? avp_cz(a, r) : NULL;
}

static AvpJson *counts_json(AvpArena *a, const i64 *c) {
    if (!c) return avp_json_null();
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("hits"), avp_json_int(a, c[0]));
    avp_json_set(a, o, AVP_S("misses_stored"), avp_json_int(a, c[1]));
    avp_json_set(a, o, AVP_S("misses_not_stored"), avp_json_int(a, c[2]));
    avp_json_set(a, o, AVP_S("uncacheable"), avp_json_int(a, c[3]));
    return o;
}

static const i64 *count_log(AvpArena *a, const char *path) {
    AvpBytes b = {0};
    if (!avp_read_all(a, path, &b)) return NULL;
    i64 *c = AVP_NEWN(a, i64, 4);
    for (size_t i = 0; i < b.n; i++) {
        switch (b.v[i]) {
        case 'h': c[0]++; break;
        case 'm': c[1]++; break;
        case 'n': c[2]++; break;
        case 'u': c[3]++; break;
        default: break;
        }
    }
    return c;
}

static int cmd_stats(void *ctx, AvpArgs *args) {
    (void)ctx;
    AvpArena *a = avp_arena();
    const char *cache = oc_cache_dir(a);
    OcEntries es = oc_entries(a, cache, true);
    i64 objects = 0, manifests = 0, size = 0;
    for (size_t i = 0; i < es.n; i++) {
        objects += es.v[i].kind == OC_KIND_RESULT;
        manifests += es.v[i].kind == OC_KIND_MANIFEST;
        size += es.v[i].size;
    }
    const char *root = shsm_repo_find(root_arg(a, args))->root;
    const i64 *last = count_log(a, avp_path_joinv(a, 3, (const char *const[]){root, "build", OC_STATS_LOG}));
    const i64 *total = count_log(a, avp_path_join(a, cache, OC_STATS_LOG));
    ShsmOutput *out = shsm_output_open(args, -1, -1, -1);
    shsm_output_line(out, avp_pyfmt(a, "cache:    %s", cache));
    shsm_output_line(out, avp_pyfmt(a, "entries:  %d objects, %d manifests, %.1f MB", objects, manifests,
                                    (double)size / 1e6));
    const char *labels[2] = {"this build:", "all builds:"};
    const i64 *cs[2] = {last, total};
    for (int i = 0; i < 2; i++) {
        const i64 *c = cs[i];
        if (!c)
            shsm_output_line(out, avp_pyfmt(a, "%-10s no compiles recorded", labels[i]));
        else
            shsm_output_line(out, avp_pyfmt(a, "%-10s %d hits, %d misses (%d stored, %d not stored), %d uncacheable",
                                            labels[i], c[0], c[1] + c[2], c[1], c[2], c[3]));
    }
    shsm_output_data(out, "cache", avp_json_strz(a, cache));
    shsm_output_data(out, "objects", avp_json_int(a, objects));
    shsm_output_data(out, "manifests", avp_json_int(a, manifests));
    shsm_output_data(out, "bytes", avp_json_int(a, size));
    shsm_output_data(out, "this_build", counts_json(a, last));
    shsm_output_data(out, "all_builds", counts_json(a, total));
    shsm_output_close(out);
    avp_vec_free(&es);
    return SHSM_EXIT_OK;
}

static int cmd_prune(void *ctx, AvpArgs *args) {
    (void)ctx;
    AvpArena *a = avp_arena();
    i64 removed, freed;
    double gb = avp_args_float(args, "max_size");
    oc_prune(a, oc_cache_dir(a), (i64)(gb * (double)(1LL << 30)), &removed, &freed);
    ShsmOutput *out = shsm_output_open(args, -1, -1, -1);
    shsm_output_line(out, avp_pyfmt(a, "removed %d entries, %.1f MB", removed, (double)freed / 1e6));
    shsm_output_data(out, "removed", avp_json_int(a, removed));
    shsm_output_data(out, "freed", avp_json_int(a, freed));
    shsm_output_close(out);
    return SHSM_EXIT_OK;
}

static int cmd_clear(void *ctx, AvpArgs *args) {
    (void)ctx;
    AvpArena *a = avp_arena();
    OcEntries es = oc_entries(a, oc_cache_dir(a), false);
    i64 n = 0;
    for (size_t i = 0; i < es.n; i++) n += avp_remove(es.v[i].path);
    ShsmOutput *out = shsm_output_open(args, -1, -1, -1);
    shsm_output_line(out, avp_pyfmt(a, "removed %d entries", n));
    shsm_output_data(out, "removed", avp_json_int(a, n));
    shsm_output_close(out);
    avp_vec_free(&es);
    return SHSM_EXIT_OK;
}

static const char *key16(AvpArena *a, const char *k);

typedef struct {
    const char *root, *cache, *scratch;
    const char **keys;
    const char **results;
} VerifyCtx;

static int verify_one(void *ctx, size_t i) {
    VerifyCtx *v = ctx;
    AvpArena *a = avp_arena_new(0);
    v->results[i] = oc_verify_entry(a, v->root, v->cache, v->keys[i], v->scratch); /* a string literal */
    avp_arena_free(a);
    return 0;
}

static int cmd_verify(void *ctx, AvpArgs *args) {
    (void)ctx;
    AvpArena *a = avp_arena();
    const char *root = shsm_repo_find(root_arg(a, args))->root;
    const char *cache = oc_cache_dir(a);
    OcEntries es = oc_entries(a, cache, true);
    AvpPtrVec keys = {0};
    for (size_t i = 0; i < es.n; i++)
        if (es.v[i].c_result) avp_vec_push(&keys, (void *)es.v[i].key);
    i64 sample = avp_args_int(args, "sample");
    if (!avp_args_bool(args, "all") && sample >= 0 && (size_t)sample < keys.n) {
        /* random.sample: a partial Fisher-Yates shuffle (D-objcache-4) */
        LARGE_INTEGER pc;
        QueryPerformanceCounter(&pc);
        u64 x = (u64)pc.QuadPart | 1;
        for (size_t i = 0; i < (size_t)sample; i++) {
            x ^= x << 13, x ^= x >> 7, x ^= x << 17;
            size_t j = i + (size_t)(x % (keys.n - i));
            void *t = keys.v[i];
            keys.v[i] = keys.v[j];
            keys.v[j] = t;
        }
        keys.n = (size_t)sample;
    }
    char *scratch = avp_path_joinv(a, 3, (const char *const[]){root, "build", "objcache-verify"});
    avp_makedirs(scratch, true);
    VerifyCtx v = {root, cache, scratch, (const char **)keys.v, AVP_NEWN(a, const char *, keys.n + 1)};
    i64 jobs = avp_args_is_none(args, "jobs") ? 0 : avp_args_int(args, "jobs");
    if (keys.n) avp_pool_for(NULL, jobs > 0 ? (int)jobs : 0, verify_one, &v, keys.n);
    avp_rmtree(scratch, AVP_RMTREE_IGNORE_ERRORS);
    ShsmCache *c = shsm_cache_new(cache);
    i64 ok = 0, stale = 0;
    AvpStrVec badlines = {0};
    AvpJson *bad = avp_json_arr(a);
    for (size_t i = 0; i < keys.n; i++) {
        const char *r = v.results[i];
        if (!strncmp(r, "bad", 3)) {
            shsm_cache_remove(c, v.keys[i]);
            const char *k16 = key16(a, v.keys[i]);
            AvpStr line = avp_pyfmt(a, "%s %s (entry deleted)", k16, r);
            avp_vec_push(&badlines, line);
            AvpJson *o = avp_json_obj(a);
            avp_json_set(a, o, AVP_S("key"), avp_json_strz(a, v.keys[i]));
            avp_json_set(a, o, AVP_S("reason"), avp_json_strz(a, r));
            avp_json_arr_push(a, bad, o);
        }
        ok += !strcmp(r, "ok");
        stale += !strcmp(r, "stale");
    }
    ShsmOutput *out = shsm_output_open(args, -1, -1, -1);
    AvpStrVec capped = shsm_cap_lines(a, badlines.v, badlines.n, 40, false, "use --all");
    shsm_output_lines(out, capped.v, capped.n);
    shsm_output_line(out, avp_pyfmt(a, "verified %d entries: %d identical, %d bad, %d stale (not this checkout's files)",
                                    (i64)keys.n, ok, (i64)badlines.n, stale));
    shsm_output_data(out, "checked", avp_json_int(a, (i64)keys.n));
    shsm_output_data(out, "ok", avp_json_int(a, ok));
    shsm_output_data(out, "bad", bad);
    shsm_output_data(out, "stale", avp_json_int(a, stale));
    shsm_output_close(out);
    int rc = badlines.n ? SHSM_EXIT_FINDING : SHSM_EXIT_OK;
    avp_vec_free(&badlines);
    avp_vec_free(&keys);
    avp_vec_free(&es);
    return rc;
}

static int cmd_prehash(void *ctx, AvpArgs *args) {
    (void)ctx;
    AvpArena *a = avp_arena();
    const char *r = root_arg(a, args);
    const char *root = avp_path_abspath(a, r && *r ? r : avp_getcwd(a));
    AvpStr comp = avp_args_str(args, "compilers"), stamp = avp_args_str(args, "stamp");
    double t = avp_time();
    OcPrehash ph;
    oc_prehash(a, root, comp.p && comp.n ? avp_cz(a, comp) : NULL, NULL, 0, 0, &ph);
    if (stamp.p && stamp.n) {
        char *sp = avp_cz(a, stamp);
        avp_makedirs(avp_cz(a, avp_path_dirname(avp_path_abspath(a, sp))), true);
        avp_write_atomic(sp, "", 0);
    }
    ShsmOutput *out = shsm_output_open(args, -1, -1, -1);
    shsm_output_line(out, avp_pyfmt(a, "hash memo for %s: %d files, %d hashed, %d unchanged (%.2fs)", root, ph.files,
                                    ph.hashed, ph.kept, avp_time() - t));
    shsm_output_data(out, "root", avp_json_strz(a, root));
    shsm_output_data(out, "files", avp_json_int(a, ph.files));
    shsm_output_data(out, "hashed", avp_json_int(a, ph.hashed));
    shsm_output_data(out, "kept", avp_json_int(a, ph.kept));
    shsm_output_data(out, "memo", avp_json_strz(a, oc_memo_path(a, root)));
    shsm_output_close(out);
    return SHSM_EXIT_OK;
}

static const char *key16(AvpArena *a, const char *k) {
    AvpStr s = {k, 16};
    return avp_cz(a, s);
}

static bool is_help(const char *s) {
    return !strcmp(s, "-h") || !strcmp(s, "--help");
}

static int run(int argc, char **argv) {
    AvpArena *a = avp_arena();
    if (argc > 1 && !strcmp(argv[1], "run")) { /* the hot path: no parser */
        int i = 2;
        if (i < argc && is_help(argv[i])) {
            char *hv[] = {argv[0], "run", "--help"};
            avp_args_parse(build_parser(), 3, hv); /* prints the help and exits 0 */
        }
        if (i < argc && !strcmp(argv[i], "--")) i++;
        return oc_run_wrapped(a, (const char *const *)argv + i, (size_t)(argc - i), NULL, NULL, NULL);
    }
    if (argc < 2 || is_help(argv[1])) {
        avp_args_print_help(build_parser());
        return SHSM_EXIT_OK;
    }
    static const ShsmMainFn handlers[] = {NULL, cmd_stats, cmd_prune, cmd_clear, cmd_verify, cmd_prehash};
    for (size_t i = 1; i < 6; i++)
        if (!strcmp(argv[1], SUB_NAMES[i])) {
            char **sv = argv + 1; /* argv[1] stands in as the program name */
            return shsm_cli_main(sub_parser(i), argc - 1, sv, handlers[i], NULL);
        }
    AvpStr rep = avp_repr_str(a, avp_str(argv[1]));
    fprintf(stderr, "objcache.exe: error: unknown subcommand %.*s (choose from run, stats, prune, clear, verify, prehash;"
                    " --help for usage)\n", (int)rep.n, rep.p);
    return SHSM_EXIT_ERROR;
}

AVP_MAIN(run, "objcache")
