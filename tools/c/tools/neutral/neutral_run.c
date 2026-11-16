/* neutral: git, the old tree, compiling both sides, judging the changed symbols, and the command (tools/neutral.py
 * git .. run). */
#include "cc_compare.h"
#include "gate.h"
#include "gatecheck.h"
#include "neutral.h"
#include "shsm_archive.h"
#include "shsm_cache.h"
#include "shsm_git.h"

/* ---- git and files ----------------------------------------------------------------------------------------------- */

/* neutral.git(root, *args): stdout, or UsageError "git <first two args> failed: <stderr>" */
static AvpStr git(AvpArena *a, const char *root, const char *const *args, size_t n, bool binary) {
    ShsmGitOpts o = {0};
    o.binary = binary;
    ShsmGitRun r = shsm_git_run(a, root, args, n, &o);
    if (!shsm_git_ok(&r)) {
        AvpStr err = r.err;
        if (binary) {
            /* the Python decodes stderr as UTF-8 with errors="replace" */
            err = avp_str(avp_cz(a, err));
        }
        shsm_raise(SHSM_EXC_USAGE, "git %s%s%s failed: %.*s", args[0], n > 1 ? " " : "", n > 1 ? args[1] : "",
                   AVP_SF(avp_str_strip(err)));
    }
    return r.out;
}

static AvpStrVec nonblank_lines(AvpStr text) {
    AvpStrVec raw = avp_str_splitlines(text, false), out = {0};
    for (size_t i = 0; i < raw.n; i++) {
        AvpStr s = avp_str_strip(raw.v[i]);
        if (s.n) avp_vec_push(&out, s);
    }
    avp_vec_free(&raw);
    return out;
}

/* changed_files(root, rev, paths): sorted and unique */
static AvpStrVec changed_files(AvpArena *a, const char *root, const char *rev, const AvpStrVec *paths) {
    AvpSet *set = avp_set_new();
    if (paths->n) {
        for (size_t i = 0; i < paths->n; i++) {
            char *rel = NULL;
            const char *abs = avp_path_abspath(a, avp_cz(a, paths->v[i]));
            if (!avp_path_relpath(a, abs, root, &rel)) shsm_raise_io();
            avp_set_add(set, avp_str_replace(a, avp_str(rel), AVP_S("\\"), AVP_S("/"), -1));
        }
    } else {
        const char *d[] = {"diff", "--name-only", rev};
        AvpStrVec l = nonblank_lines(git(a, root, d, 3, false));
        for (size_t i = 0; i < l.n; i++) avp_set_add(set, l.v[i]);
        avp_vec_free(&l);
        const char *u[] = {"ls-files", "--others", "--exclude-standard"};
        l = nonblank_lines(git(a, root, u, 3, false));
        for (size_t i = 0; i < l.n; i++) avp_set_add(set, l.v[i]);
        avp_vec_free(&l);
    }
    AvpStrVec sorted = avp_set_sorted(set), out = {0};
    for (size_t i = 0; i < sorted.n; i++) avp_vec_push(&out, avp_str_dup(a, sorted.v[i]));
    avp_vec_free(&sorted);
    avp_set_free(set);
    return out;
}

/* rev_blobs(root, rev): {path: blob id} of include/ and src/ at rev */
static AvpMap *rev_blobs(AvpArena *a, const char *root, const char *rev) {
    const char *args[] = {"ls-tree", "-r", rev, "--", "include", "src"};
    AvpStrVec lines = avp_str_splitlines(git(a, root, args, 6, false), false);
    AvpMap *out = avp_map_new();
    bool cr = false;
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr meta, path;
        if (!avp_str_partition(lines.v[i], AVP_S("\t"), &meta, &path)) path = AVP_S("");
        AvpStrVec parts = avp_str_split(meta, (AvpStr){0}, -1);
        if (parts.n == 3 && avp_str_eqz(parts.v[1], "blob")) avp_map_put(out, path, &cr)->p = avp_cz(a, parts.v[2]);
        avp_vec_free(&parts);
    }
    avp_vec_free(&lines);
    return out;
}

/* materialise_rev(root, rev_sha, base): build/neutral/tree-<sha16>/{include,src} at rev */
static const char *materialise_rev(AvpArena *a, const char *root, const char *rev_sha, const char *base) {
    const char *d = avp_path_join(a, base, avp_sprintf(a, "tree-%.16s", rev_sha));
    const char *mark = avp_path_join(a, d, ".complete");
    if (avp_isfile(mark)) return d;
    if (avp_isdir(d) && !avp_rmtree(d, 0)) shsm_raise_io();
    const char *args[] = {"archive", "--format=tar", rev_sha, "--", "include", "src"};
    AvpStr tar = git(a, root, args, 6, true);
    size_t written = 0;
    const char *err = NULL;
    if (!shsm_tar_extract_files(tar.p, tar.n, d, NULL, NULL, &written, &err)) shsm_raise(SHSM_EXC_OS, "%s", err);
    if (!avp_write_file(mark, rev_sha, strlen(rev_sha))) shsm_raise_io();
    return d;
}

static char *sha1_file(AvpArena *a, const char *path) {
    AvpBytes b = {0};
    if (!avp_read_all(a, path, &b)) shsm_raise_io();
    return avp_sha1_hex(a, b.v, b.n);
}

/* ---- dependencies ------------------------------------------------------------------------------------------------- */

static AvpMap *ninja_deps(AvpArena *a, const char *root) {
    if (!avp_isfile(avp_path_join(a, root, ".ninja_deps"))) return NULL;
    const char *argv[] = {avp_join(a, root, "tools", "bin", "ninja.exe"), "-t", "deps"};
    ShsmRun r = shsm_run(a, argv, 3, root, 0);
    if (r.timed_out || r.returncode != 0) return NULL;
    return gate_parse_ninja_deps(a, r.out, root);
}

/* ---- compiling ---------------------------------------------------------------------------------------------------- */

typedef struct {
    const char *root, *rev, *base, *old_tree;
    const ShsmRepo *repo;
    ShsmCache *cache; /* NULL: --no-cache */
    AvpMutex cache_lock;
    const AvpMap *blobs;
} Ctx;

typedef struct {
    const char *name, *reason;
    const ShsmUnit *u;
    const char *old_obj, *new_obj, *error;
    bool cache_hit;
    AvpArena *a;
    int exc; /* an exception the compile raised (re-raised in order on the main thread) */
    char excmsg[1024];
    bool loaded[2];
    const AvpMap *compiled[2];
} Job;

static const char **cargv(AvpArena *a, const AvpStrVec *v) {
    const char **av = AVP_NEWN(a, const char *, v->n + 1);
    for (size_t i = 0; i < v->n; i++) av[i] = avp_cz(a, v->v[i]);
    return av;
}

static ShsmRun run_compile(AvpArena *a, const AvpStrVec *argv, const char *root, const char *outdir) {
    if (!avp_makedirs(outdir, true)) shsm_raise_io();
    return shsm_run(a, cargv(a, argv), argv->n, root, 300);
}

static const char *compile_failure(AvpArena *a, const ShsmRun *r) {
    AvpStr out = shsm_run_output(a, r);
    AvpStr fe = neutral_first_error(a, out);
    if (fe.p) return avp_cz(a, fe);
    AvpStrVec l = avp_str_splitlines(avp_str_strip(out), false);
    const char *s = l.n ? avp_cz(a, l.v[l.n - 1]) : "?";
    avp_vec_free(&l);
    return s;
}

/* read_id(path): the blob id at REV (blob), or ("file", sha1) outside git (file); false: None */
typedef struct {
    const char *blob, *file;
} ReadId;

static bool read_id(AvpArena *a, const Ctx *c, AvpStr path, ReadId *out) {
    *out = (ReadId){0};
    AvpStr before, after;
    AvpStr first = avp_str_partition(path, AVP_S("/"), &before, &after) ? before : path;
    if (avp_str_eqz(first, "include") || avp_str_eqz(first, "src")) {
        AvpVal *v = avp_map_get(c->blobs, path);
        if (!v) return false;
        out->blob = v->p;
        return true;
    }
    const char *pz = avp_cz(a, path);
    const char *p = avp_path_isabs(pz) ? pz : avp_path_join(a, c->root, pz);
    if (!avp_isfile(p)) return false;
    out->file = sha1_file(a, p);
    return true;
}

/* repr(_freeze(ids)): (('path', 'blob'), ('path', ('file', 'sha1')), ...) */
static AvpStr ids_repr(AvpArena *a, const AvpStrVec *reads, const ReadId *ids) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putc(&b, '(');
    for (size_t i = 0; i < reads->n; i++) {
        if (i) avp_buf_putz(&b, ", ");
        avp_buf_putc(&b, '(');
        avp_buf_put(&b, avp_repr_str(a, reads->v[i]));
        avp_buf_putz(&b, ", ");
        if (ids[i].blob) {
            avp_buf_put(&b, avp_repr_str(a, avp_str(ids[i].blob)));
        } else {
            avp_buf_putz(&b, "('file', ");
            avp_buf_put(&b, avp_repr_str(a, avp_str(ids[i].file)));
            avp_buf_putc(&b, ')');
        }
        avp_buf_putc(&b, ')');
    }
    if (reads->n == 1) avp_buf_putc(&b, ',');
    avp_buf_putc(&b, ')');
    return avp_buf_str(&b);
}

/* the ids of every read, false when one is None */
static bool all_ids(AvpArena *a, const Ctx *c, const AvpStrVec *reads, ReadId **ids) {
    *ids = AVP_NEWN(a, ReadId, reads->n + 1);
    bool ok = true;
    for (size_t i = 0; i < reads->n; i++)
        if (!read_id(a, c, reads->v[i], &(*ids)[i])) ok = false;
    return ok;
}

static char *obj_key(AvpArena *a, const AvpStrVec *kargv, const AvpStrVec *reads, const ReadId *ids) {
    ShsmKey k;
    shsm_key_init(&k);
    shsm_key_strz(&k, "neutral-obj");
    shsm_key_str_list(&k, kargv->v, kargv->n);
    shsm_key_repr(&k, ids_repr(a, reads, ids));
    return shsm_key_hex(a, &k);
}

static void compile_pair(Job *j, Ctx *c) {
    AvpArena *a = j->a;
    const ShsmUnit *u = j->u;
    const char *src_rel = avp_cz(a, avp_str_replace(a, avp_str(u->source), AVP_S("\\"), AVP_S("/"), -1));
    const char *safe = neutral_safe_name(a, u->path);
    const char *new_dir = avp_join(a, c->base, "new", safe);
    const char *old_dir = avp_join(a, c->base, "old", safe);
    neutral_prepare_dirs(new_dir, old_dir);
    /* new side */
    const char *new_obj = shsm_object_for(a, src_rel, new_dir);
    AvpStrVec argv = shsm_compile_argv(a, c->repo, u, avp_path_join(a, c->root, src_rel), new_obj, NULL);
    ShsmRun r = run_compile(a, &argv, c->root, new_dir);
    if (!shsm_run_ok(&r)) {
        j->error = avp_sprintf(a, "working tree does not compile: %s", compile_failure(a, &r));
        return;
    }
    j->new_obj = new_obj;
    /* old side */
    if (!avp_map_get(c->blobs, avp_str(src_rel))) {
        j->error = avp_sprintf(a, "source %s does not exist at %.12s", src_rel, c->rev);
        return;
    }
    ShsmCompileOpts opts = {0};
    opts.sjis = -1;
    opts.depfile = true;
    argv = shsm_compile_argv(a, c->repo, u, avp_path_join(a, c->root, src_rel), old_dir, &opts);
    argv = neutral_remap_argv(a, &argv, c->root, c->old_tree, src_rel);
    AvpStrVec kargv = neutral_key_argv(a, &argv, c->root, c->old_tree, old_dir);
    const char *old_obj = shsm_object_for(a, src_rel, old_dir);
    char *dkey = NULL;
    if (c->cache) {
        ShsmKey k;
        shsm_key_init(&k);
        shsm_key_strz(&k, "neutral-deps");
        shsm_key_strz(&k, u->name);
        shsm_key_str_list(&k, kargv.v, kargv.n);
        dkey = shsm_key_hex(a, &k);
        AvpStr raw = {0};
        avp_mutex_lock(&c->cache_lock);
        bool hit = shsm_cache_get(a, c->cache, dkey, &raw);
        avp_mutex_unlock(&c->cache_lock);
        if (hit) {
            AvpJsonError jerr = {0};
            AvpJson *doc = avp_json_parse(a, raw, &jerr);
            if (!doc || !avp_json_is_arr(doc)) shsm_raise(SHSM_EXC_VALUE, "cached dependency list is not a JSON list");
            AvpStrVec reads = {0};
            for (size_t i = 0; i < avp_json_len(doc); i++) avp_vec_push(&reads, avp_json_at(doc, i)->u.str);
            ReadId *ids = NULL;
            if (all_ids(a, c, &reads, &ids)) {
                char *okey = obj_key(a, &kargv, &reads, ids);
                avp_mutex_lock(&c->cache_lock);
                bool restored = false;
                AvpTry t;
                if (AVP_TRY(&t)) {
                    restored = shsm_cache_restore(c->cache, okey, old_obj);
                    avp_try_end(&t);
                } else {
                    avp_mutex_unlock(&c->cache_lock);
                    avp_throw(t.code, "%s", t.msg);
                }
                avp_mutex_unlock(&c->cache_lock);
                if (restored) {
                    j->old_obj = old_obj;
                    j->cache_hit = true;
                    return;
                }
            }
        }
    }
    r = run_compile(a, &argv, c->root, old_dir);
    if (!shsm_run_ok(&r)) {
        j->error = avp_sprintf(a, "REV %.12s does not compile: %s", c->rev, compile_failure(a, &r));
        return;
    }
    j->old_obj = old_obj;
    if (c->cache) {
        const char *dfile = shsm_depfile_for(a, src_rel, old_dir);
        if (avp_isfile(dfile)) {
            AvpStrVec deps = neutral_parse_depfile(a, dfile);
            AvpSet *set = avp_set_new();
            for (size_t i = 0; i < deps.n; i++)
                avp_set_add(set, avp_str(neutral_rel_dep(a, avp_cz(a, deps.v[i]), c->root, c->old_tree)));
            avp_set_add(set, avp_str(src_rel));
            AvpStrVec sorted = avp_set_sorted(set), reads = {0};
            for (size_t i = 0; i < sorted.n; i++) avp_vec_push(&reads, avp_str_dup(a, sorted.v[i]));
            avp_vec_free(&sorted);
            avp_set_free(set);
            ReadId *ids = NULL;
            if (all_ids(a, c, &reads, &ids)) {
                AvpJson *arr = avp_json_arr(a);
                for (size_t i = 0; i < reads.n; i++) avp_json_arr_push(a, arr, avp_json_str(a, reads.v[i]));
                AvpJsonDumpOpts dopts = {0};
                AvpStr payload = avp_json_dumps(a, arr, &dopts);
                char *okey = obj_key(a, &kargv, &reads, ids);
                avp_mutex_lock(&c->cache_lock);
                AvpTry t;
                if (AVP_TRY(&t)) {
                    shsm_cache_put(c->cache, dkey, payload.p, payload.n);
                    shsm_cache_store(c->cache, okey, old_obj);
                    avp_try_end(&t);
                } else {
                    avp_mutex_unlock(&c->cache_lock);
                    avp_throw(t.code, "%s", t.msg);
                }
                avp_mutex_unlock(&c->cache_lock);
            }
        }
    }
}

static int compile_worker(void *ctx, size_t index, const void *item, void *result) {
    AVP_UNUSED(index);
    AVP_UNUSED(result);
    Job *j = *(Job *const *)item;
    AvpTry t;
    if (AVP_TRY(&t)) {
        compile_pair(j, ctx);
        avp_try_end(&t);
    } else {
        j->exc = t.code ? t.code : SHSM_EXC_OS;
        snprintf(j->excmsg, sizeof j->excmsg, "%s", t.msg);
    }
    return 0;
}

/* ---- judging ------------------------------------------------------------------------------------------------------ */

static void raise_cc(const CcError *e) {
    int code = e->kind == CC_ERR_USAGE ? SHSM_EXC_USAGE : e->kind == CC_ERR_VALUE ? SHSM_EXC_VALUE : SHSM_EXC_OS;
    shsm_raise(code, "%.*s", AVP_SF(e->msg));
}

/* compare.compare_function on one side: true with (exact, edit), false for a KeyError */
static bool compare_side(AvpArena *a, const CcRepo *repo, Job *j, int side, const char *name, bool *exact,
                         size_t *edit) {
    const char *obj = side ? j->new_obj : j->old_obj;
    if (!j->loaded[side]) {
        /* one disassembly per object; on a failure the call below repeats the load and reports it as the Python */
        j->loaded[side] = true;
        CcError e = {0};
        j->compiled[side] = cc_load_compiled(a, repo, obj, NULL, NULL, NULL, &e);
    }
    CcError err = {0};
    CcCompareResult *r = cc_compare_function_unit(a, repo, avp_str(j->u->name), obj, avp_str(name), AVP_S(""), true,
                                                  j->compiled[side], NULL, &err);
    if (!r) {
        if (err.kind == CC_ERR_KEY) return false;
        raise_cc(&err);
    }
    *exact = r->exact;
    *edit = r->edit;
    return true;
}

static const char *judge_function(AvpArena *a, const CcRepo *repo, Job *j, const char *name) {
    bool ea = false, eb = false;
    size_t da = 0, db = 0;
    bool ha = compare_side(a, repo, j, 0, name, &ea, &da);
    bool hb = compare_side(a, repo, j, 1, name, &eb, &db);
    if (!ha && !hb) return "no retail counterpart";
    if (!ha || !hb) return !ha ? "improvement" : "regression";
    if (ea != eb) return eb ? "improvement" : "regression";
    if (da != db) return db < da ? "improvement" : "regression";
    return avp_sprintf(a, "same distance (edit %zu)", db);
}

/* `gatecheck.py --root ROOT layout <unit path> --object OBJ`, by its exit code: 0 ok, 1 DIFFERENT, else error */
static const char *layout_status(const char *root, const char *unit_path, const char *obj) {
    AvpArena *a = avp_arena_new(0);
    const char *res = "ok";
    AvpTry t;
    if (AVP_TRY(&t)) {
        const ShsmDol *retail = shsm_dol_load(a, avp_path_join(a, root, "orig/R5WEA4/sys/main.dol"));
        const char *cfg = avp_path_join(a, root, "configure.py");
        AvpMap *statuses = avp_exists(cfg) ? gc_parse_configure(a, gc_read_text(a, cfg)) : avp_map_new();
        const char *key = gc_strip_ext(a, gc_unit_key(a, unit_path));
        const char *path = key;
        AvpMapIter it = {0};
        while (avp_map_next(statuses, &it)) {
            const char *p = avp_cz(a, it.key);
            if (strcmp(gc_strip_ext(a, p), key) == 0) {
                path = p;
                break;
            }
        }
        const char *asm_ = avp_join(a, root, "build/R5WEA4/asm", avp_sprintf(a, "%s.s", key));
        if (!avp_exists(obj) || !avp_exists(asm_)) {
            res = "error";
        } else {
            GcWeakVec dedupe = {0};
            const char *sp = avp_path_join(a, root, "config/R5WEA4/splits.txt");
            const char *yp = avp_path_join(a, root, "config/R5WEA4/symbols.txt");
            if (avp_exists(sp) && avp_exists(yp)) {
                GcSymTxtVec syms = gc_parse_symbols_txt(a, gc_read_text(a, yp));
                GcSplitVec splits = gc_parse_splits(a, gc_read_text(a, sp));
                dedupe = gc_weak_dedupe_index(a, &syms, &splits);
            }
            AvpVal *v = avp_map_get(statuses, avp_str(path));
            GcLayoutResult *lr =
                gc_check_layout(a, path, v ? v->p : "unlisted", obj, asm_, retail, NULL, &dedupe);
            size_t nd = gc_layout_ndiffs(lr);
            res = nd ? "DIFFERENT" : (lr->errors.n ? "error" : "ok");
        }
        avp_try_end(&t);
    } else {
        /* a GateError exits 2; any other exception is a traceback with exit status 1 */
        res = gc_is_gate(t.code) ? "error" : "DIFFERENT";
    }
    avp_arena_free(a);
    return res;
}

static const char *judge_layout(AvpArena *a, const char *root, const Job *j) {
    const char *x = layout_status(root, j->u->path, j->old_obj);
    const char *y = layout_status(root, j->u->path, j->new_obj);
    if (strcmp(x, y) == 0) return avp_sprintf(a, "layout %s on both sides", y);
    return avp_sprintf(a, "%s (layout %s -> %s)", strcmp(y, "ok") == 0 ? "improvement" : "regression", x, y);
}

/* ---- main --------------------------------------------------------------------------------------------------------- */

typedef struct {
    const char *unit, *symbol, *kind, *verdict;
} Changed;

static int cmp_job(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return strcmp((*(Job *const *)x)->name, (*(Job *const *)y)->name);
}

int neutral_run(const NeutralOpts *o, const AvpArgs *args) {
    AvpArena *a = avp_arena();
    const char *root = o->root ? shsm_find_root(o->root)
                               : avp_path_abspath(a, avp_join(a, avp_exe_dir(), "..", ".."));
    const ShsmRepo *rp = shsm_repo_new(root, NULL);
    root = rp->root;
    double t0 = avp_time();
    const char *spec = avp_sprintf(a, "%s^{commit}", o->since ? o->since : "HEAD");
    const char *rv[] = {"rev-parse", "--verify", spec};
    const char *rev = avp_cz(a, avp_str_strip(git(a, root, rv, 3, false)));
    AvpStrVec files = changed_files(a, root, rev, &o->paths);
    if (!files.n) shsm_raise(SHSM_EXC_USAGE, "no changed file against %.12s: nothing to check", rev);
    if (!o->paths.n)
        for (size_t i = 0; i < files.n; i++)
            if (avp_str_eqz(files.v[i], "configure.py"))
                shsm_raise(SHSM_EXC_USAGE, "configure.py changed: both sides would use the current flags (out of scope)");
    const ShsmUnits *units = shsm_load_units(rp);
    AvpMap *deps = ninja_deps(a, root);
    const char *note = NULL;
    AvpMap *reached = neutral_reach(a, units->v, units->n, files.v, files.n, deps, root, &note);
    if (!avp_map_len(reached))
        shsm_raise(SHSM_EXC_USAGE, "no unit is reached by the %zu changed file(s) (%s): nothing to compile", files.n,
                   note);
    const char *base = avp_path_join(a, rp->build_dir, "neutral");
    if (!avp_makedirs(base, true)) shsm_raise_io();
    Ctx c = {0};
    c.root = root;
    c.repo = rp;
    c.rev = rev;
    c.base = base;
    c.cache = o->no_cache ? NULL : shsm_cache_new(NULL);
    c.blobs = rev_blobs(a, root, rev);
    c.old_tree = materialise_rev(a, root, rev, base);
    AVP_VEC(Job *) jobs = {0};
    AvpMapIter it = {0};
    while (avp_map_next(reached, &it)) {
        Job *j = AVP_NEW(a, Job);
        j->name = avp_cz(a, it.key);
        j->reason = it.val->p;
        j->u = shsm_units_lookup(units, j->name);
        j->a = avp_arena_new(0);
        avp_vec_push(&jobs, j);
    }
    avp_sort(jobs.v, jobs.n, sizeof *jobs.v, cmp_job, NULL, false);
    int nj = o->jobs > 0 ? o->jobs : shsm_default_jobs();
    if (nj <= 1 || jobs.n <= 1) nj = 1;
    else if ((size_t)nj > jobs.n) nj = (int)jobs.n;
    avp_pool_map(NULL, nj, compile_worker, &c, jobs.v, jobs.n, sizeof *jobs.v, NULL, 0);
    for (size_t i = 0; i < jobs.n; i++)
        if (jobs.v[i]->exc) avp_throw(jobs.v[i]->exc, "%s", jobs.v[i]->excmsg);
    CcRepo repo = {root, rp->version};
    AVP_VEC(Changed) changed = {0};
    AVP_VEC(Job *) errors = {0};
    i64 nfun = 0;
    for (size_t i = 0; i < jobs.n; i++) {
        Job *j = jobs.v[i];
        if (j->error) {
            avp_vec_push(&errors, j);
            continue;
        }
        NeutralModel *old = neutral_object_model(j->a, j->old_obj), *new_ = neutral_object_model(j->a, j->new_obj);
        nfun += (i64)avp_map_len(new_->functions);
        NeutralChangeVec fchg = {0}, dchg = {0};
        neutral_diff_models(old, new_, &fchg, &dchg);
        for (size_t k = 0; k < fchg.n; k++)
            avp_vec_push(&changed, ((Changed){j->name, fchg.v[k].name, fchg.v[k].kind,
                                              judge_function(j->a, &repo, j, fchg.v[k].name)}));
        for (size_t k = 0; k < dchg.n; k++)
            avp_vec_push(&changed, ((Changed){j->name, dchg.v[k].name, dchg.v[k].kind, judge_layout(a, root, j)}));
        avp_vec_free(&fchg);
        avp_vec_free(&dchg);
    }
    i64 hits = 0;
    for (size_t i = 0; i < jobs.n; i++) hits += jobs.v[i]->cache_hit;
    ShsmOutput *out = shsm_output_open(args, -1, -1, -1);
    shsm_output_line(out, avp_pyfmt(a, "%d changed file(s) against %.12s; %d unit(s) reached (%s); old side: %d cache hit(s)",
                                    files.n, rev, jobs.n, note, hits));
    for (size_t i = 0; i < errors.n; i++)
        shsm_output_line(out, avp_pyfmt(a, "ERROR  %-48s %s", errors.v[i]->name, errors.v[i]->error));
    for (size_t i = 0; i < changed.n; i++)
        shsm_output_line(out, avp_pyfmt(a, "%-40s %-48s %-11s %s", changed.v[i].unit, changed.v[i].symbol,
                                        changed.v[i].kind, changed.v[i].verdict));
    /* out.data(rev=, files=, units=, changed=, errors=, functions=, cache_hits=, seconds=) */
    AvpJson *jf = avp_json_arr(a), *ju = avp_json_arr(a), *jc = avp_json_arr(a), *je = avp_json_arr(a);
    for (size_t i = 0; i < files.n; i++) avp_json_arr_push(a, jf, avp_json_str(a, files.v[i]));
    for (size_t i = 0; i < jobs.n; i++) avp_json_arr_push(a, ju, avp_json_strz(a, jobs.v[i]->name));
    for (size_t i = 0; i < changed.n; i++) {
        AvpJson *d = avp_json_obj(a);
        avp_json_set(a, d, AVP_S("unit"), avp_json_strz(a, changed.v[i].unit));
        avp_json_set(a, d, AVP_S("symbol"), avp_json_strz(a, changed.v[i].symbol));
        avp_json_set(a, d, AVP_S("kind"), avp_json_strz(a, changed.v[i].kind));
        avp_json_set(a, d, AVP_S("verdict"), avp_json_strz(a, changed.v[i].verdict));
        avp_json_arr_push(a, jc, d);
    }
    for (size_t i = 0; i < errors.n; i++) {
        AvpJson *d = avp_json_obj(a);
        avp_json_set(a, d, AVP_S("unit"), avp_json_strz(a, errors.v[i]->name));
        avp_json_set(a, d, AVP_S("error"), avp_json_strz(a, errors.v[i]->error));
        avp_json_arr_push(a, je, d);
    }
    shsm_output_data(out, "rev", avp_json_strz(a, rev));
    shsm_output_data(out, "files", jf);
    shsm_output_data(out, "units", ju);
    shsm_output_data(out, "changed", jc);
    shsm_output_data(out, "errors", je);
    shsm_output_data(out, "functions", avp_json_int(a, nfun));
    shsm_output_data(out, "cache_hits", avp_json_int(a, hits));
    shsm_output_data(out, "seconds", avp_json_float(a, avp_py_round(avp_time() - t0, 1)));
    shsm_output_close(out);
    int rc = SHSM_EXIT_OK;
    if (errors.n || changed.n) {
        i64 imp = 0, reg = 0;
        AvpSet *us = avp_set_new();
        for (size_t i = 0; i < changed.n; i++) {
            imp += strncmp(changed.v[i].verdict, "improvement", 11) == 0;
            reg += strncmp(changed.v[i].verdict, "regression", 10) == 0;
            avp_set_add(us, avp_str(changed.v[i].unit));
        }
        AvpStr line = avp_pyfmt(a,
                                "NOT NEUTRAL: %d unit(s), %d symbol(s) changed (%d improvement(s), %d regression(s)), "
                                "%d error(s) (%.0fs)",
                                avp_set_len(us), changed.n, imp, reg, errors.n, avp_time() - t0);
        avp_set_free(us);
        printf("%.*s\n", AVP_SF(line));
        rc = SHSM_EXIT_FINDING;
    } else {
        AvpStr line = avp_pyfmt(a, "NEUTRAL (%d units, %d functions) (%.0fs)", jobs.n, nfun, avp_time() - t0);
        printf("%.*s\n", AVP_SF(line));
    }
    for (size_t i = 0; i < jobs.n; i++) avp_arena_free(jobs.v[i]->a);
    return rc;
}
