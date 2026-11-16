/* git, once: see shsm_git.h. The runner is the CreateProcess call of avp_run; the parsers are written against the
 * output formats of git 2.50 (documented per function in shsm_git.md).
 *
 * The process-spawn shape (argv array, "cannot run git" on a missing executable) follows that project's lib/worktree.c
 * (another project of the author's, commit 49e811503d7ba5c9cff92160557df057c50a8ab3: git(args, cwd) and list_worktrees). */
#include "shsm_git.h"

#include "avp/fs.h"
#include "avp/proc.h"
#include "shsm_cli.h"

#include <string.h>

#define NARGS(arr) (sizeof(arr) / sizeof((arr)[0]))

/* ---- the runner ------------------------------------------------------------------------------------------------ */

static const char **make_argv(AvpArena *a, const char *checkout, bool quotepath_off, const char *const *args, size_t n) {
    const char **argv = AVP_NEWN(a, const char *, n + 6);
    size_t k = 0;
    argv[k++] = "git";
    if (quotepath_off) {
        argv[k++] = "-c";
        argv[k++] = "core.quotepath=off";
    }
    if (checkout) {
        argv[k++] = "-C";
        argv[k++] = checkout;
    }
    for (size_t i = 0; i < n; i++) argv[k++] = args[i];
    argv[k] = NULL;
    return argv;
}

/* The inherited environment plus the "NAME=VALUE" additions (names upper-cased as os.environ does on Windows). */
static AvpEnv *make_env(AvpArena *a, const char *const *add) {
    AvpEnv *e = avp_env_current(a);
    for (size_t i = 0; add[i]; i++) {
        const char *eq = strchr(add[i], '=');
        size_t nlen = eq ? (size_t)(eq - add[i]) : strlen(add[i]);
        char *name = avp_strdup(a, add[i]);
        name[nlen] = 0;
        for (size_t j = 0; j < nlen; j++)
            if (name[j] >= 'a' && name[j] <= 'z') name[j] = (char)(name[j] - 'a' + 'A');
        avp_env_set(e, name, eq ? eq + 1 : "");
    }
    return e;
}

ShsmGitRun shsm_git_run(AvpArena *a, const char *checkout, const char *const *args, size_t n, const ShsmGitOpts *opts) {
    ShsmGitOpts o = {0};
    if (opts) o = *opts;
    AvpProcSpec spec = {0};
    spec.argv = make_argv(a, checkout, o.quotepath_off, args, n);
    spec.in = o.input ? AVP_IO_CAPTURE : AVP_IO_NULL;
    spec.input = o.input;
    spec.input_n = o.input_n;
    spec.out = AVP_IO_CAPTURE;
    spec.err = AVP_IO_CAPTURE;
    spec.text = !o.binary;
    spec.errors = AVP_ERRORS_REPLACE;
    spec.timeout = o.timeout > 0 ? o.timeout : 0;
    spec.env = o.env ? make_env(a, o.env) : NULL;
    spec.arena = a;
    AvpProcResult r = avp_run(&spec);
    ShsmGitRun out = {0};
    out.out = AVP_S("");
    out.err = AVP_S("");
    if (r.status == AVP_PROC_START_FAILED) {
        const char *m = r.message ? r.message : "cannot start";
        if (!o.soft_start) shsm_raise(SHSM_EXC_USAGE, "cannot run git: %s", m);
        out.rc = -1;
        out.err = avp_str(avp_strdup(a, m));
        return out;
    }
    out.started = true;
    if (r.out.p) out.out = r.out;
    if (r.err.p) out.err = r.err;
    if (r.status == AVP_PROC_TIMEOUT) {
        out.timed_out = true;
        out.rc = -1;
    } else {
        out.rc = r.returncode;
    }
    return out;
}

AvpStr shsm_git_failure_text(const ShsmGitRun *r) {
    if (r->timed_out) return AVP_S("timed out");
    return avp_str_strip(r->err.n ? r->err : r->out);
}

void shsm_git_require(AvpArena *a, const ShsmGitRun *r, const char *const *args, size_t n) {
    if (shsm_git_ok(r)) return;
    AvpBuf cmd;
    avp_buf_init(&cmd, a);
    for (size_t i = 0; i < n; i++) {
        if (i) avp_buf_putc(&cmd, ' ');
        avp_buf_putz(&cmd, args[i]);
    }
    AvpStr why = shsm_git_failure_text(r);
    shsm_raise(SHSM_EXC_USAGE, "git %s failed: %.*s", avp_buf_cz(&cmd), AVP_SF(why));
}

ShsmGitRun shsm_git_check(AvpArena *a, const char *checkout, const char *const *args, size_t n, const ShsmGitOpts *opts) {
    ShsmGitRun r = shsm_git_run(a, checkout, args, n, opts);
    shsm_git_require(a, &r, args, n);
    return r;
}

bool shsm_git_available(AvpArena *a) { return avp_which(a, "git", NULL, NULL) != NULL; }

/* ---- internals ------------------------------------------------------------------------------------------------- */

/* A query: quotepath off, text or binary. */
static ShsmGitRun gq(AvpArena *a, const char *co, bool binary, const char *const *args, size_t n) {
    ShsmGitOpts o = {0};
    o.quotepath_off = true;
    o.binary = binary;
    return shsm_git_run(a, co, args, n, &o);
}

/* The result of a list query: true when ok; else the stripped failure text in *err, or a raise when err is NULL. */
static bool finish(AvpArena *a, const ShsmGitRun *r, const char *const *args, size_t n, AvpStr *err) {
    if (shsm_git_ok(r)) return true;
    if (err) {
        *err = shsm_git_failure_text(r);
        return false;
    }
    shsm_git_require(a, r, args, n);
    return false;
}

/* head[0..nh) then tail[0..nt) as one argument array. */
static const char **cat_args(AvpArena *a, const char *const *head, size_t nh, const char *const *tail, size_t nt,
                             size_t *n) {
    const char **v = AVP_NEWN(a, const char *, nh + nt + 1);
    for (size_t i = 0; i < nh; i++) v[i] = head[i];
    for (size_t i = 0; i < nt; i++) v[nh + i] = tail[i];
    *n = nh + nt;
    return v;
}

/* The first line of a successful query's stdout, stripped, as a C string; NULL on failure or an empty answer. */
static char *first_line(AvpArena *a, const ShsmGitRun *r) {
    if (!shsm_git_ok(r)) return NULL;
    AvpStr s = avp_str_strip(r->out);
    i64 nl = avp_str_findc(s, '\n');
    if (nl >= 0) s = avp_str_strip(avp_strn(s.p, (size_t)nl));
    return s.n ? avp_cz(a, s) : NULL;
}

AvpStrVec shsm_git_split_lines(AvpStr text) {
    AvpStrVec v = {0};
    size_t st = 0;
    for (size_t i = 0; i < text.n; i++) {
        if (text.p[i] == '\n') {
            avp_vec_push(&v, avp_strn(text.p + st, i - st));
            st = i + 1;
        }
    }
    if (st < text.n) avp_vec_push(&v, avp_strn(text.p + st, text.n - st));
    return v;
}

AvpStrVec shsm_git_split_nul(AvpStr data) {
    AvpStrVec v = {0};
    size_t st = 0;
    for (size_t i = 0; i < data.n; i++) {
        if (data.p[i] == 0) {
            avp_vec_push(&v, avp_strn(data.p + st, i - st));
            st = i + 1;
        }
    }
    if (st < data.n) avp_vec_push(&v, avp_strn(data.p + st, data.n - st));
    return v;
}

/* Every non-empty stripped line of a text answer. */
static void push_stripped_lines(AvpStr text, AvpStrVec *out) {
    AvpStrVec lines = shsm_git_split_lines(text);
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr s = avp_str_strip(lines.v[i]);
        if (s.n) avp_vec_push(out, s);
    }
    avp_vec_free(&lines);
}

/* The folder relative answers of git (--git-common-dir, --git-path) are relative to. */
static char *base_dir(AvpArena *a, const char *checkout) { return avp_path_abspath(a, checkout ? checkout : "."); }

static char *resolve(AvpArena *a, const char *checkout, const char *p) {
    if (avp_path_isabs(p)) return avp_path_normpath(a, p);
    return avp_path_normpath(a, avp_path_join(a, base_dir(a, checkout), p));
}

/* ---- status ---------------------------------------------------------------------------------------------------- */

void shsm_git_parse_status(AvpStr z, ShsmGitStatusVec *out) {
    AvpStrVec f = shsm_git_split_nul(z);
    for (size_t i = 0; i < f.n; i++) {
        AvpStr rec = f.v[i];
        if (rec.n < 4) continue; /* "XY " and at least one path byte */
        ShsmGitStatusEntry e = {0};
        e.xy[0] = rec.p[0];
        e.xy[1] = rec.p[1];
        e.path = avp_strn(rec.p + 3, rec.n - 3);
        if ((e.xy[0] == 'R' || e.xy[0] == 'C' || e.xy[1] == 'R' || e.xy[1] == 'C') && i + 1 < f.n) {
            e.orig_path = f.v[++i];
            e.has_orig = true;
        }
        avp_vec_push(out, e);
    }
    avp_vec_free(&f);
}

bool shsm_git_status(AvpArena *a, const char *checkout, bool untracked_all, ShsmGitStatusVec *out, AvpStr *err) {
    const char *with_all[] = {"status", "--porcelain=v1", "-z", "--untracked-files=all"};
    const char *plain[] = {"status", "--porcelain=v1", "-z"};
    const char *const *args = untracked_all ? with_all : plain;
    size_t n = untracked_all ? NARGS(with_all) : NARGS(plain);
    ShsmGitRun r = gq(a, checkout, true, args, n);
    if (!finish(a, &r, args, n, err)) return false;
    avp_vec_clear(out);
    shsm_git_parse_status(r.out, out);
    return true;
}

bool shsm_git_dirty(AvpArena *a, const char *checkout) {
    const char *args[] = {"status", "--porcelain"};
    ShsmGitRun r = gq(a, checkout, false, args, NARGS(args));
    if (!shsm_git_ok(&r)) shsm_raise(SHSM_EXC_USAGE, "git status failed in %s", checkout ? checkout : ".");
    return avp_str_strip(r.out).n > 0;
}

/* ---- rev-parse and refs ---------------------------------------------------------------------------------------- */

char *shsm_git_rev_parse(AvpArena *a, const char *checkout, const char *rev) {
    const char *spec = avp_sprintf(a, "%s^{commit}", rev);
    const char *args[] = {"rev-parse", "--verify", "-q", spec};
    ShsmGitRun r = gq(a, checkout, false, args, NARGS(args));
    return first_line(a, &r);
}

char *shsm_git_rev_parse_object(AvpArena *a, const char *checkout, const char *rev) {
    const char *args[] = {"rev-parse", "--verify", "-q", rev};
    ShsmGitRun r = gq(a, checkout, false, args, NARGS(args));
    return first_line(a, &r);
}

char *shsm_git_object_id(AvpArena *a, const char *checkout, const char *rev, const char *path) {
    return shsm_git_rev_parse_object(a, checkout, avp_sprintf(a, "%s:%s", rev, path));
}

char *shsm_git_branch(AvpArena *a, const char *checkout) {
    const char *args[] = {"rev-parse", "--abbrev-ref", "HEAD"};
    ShsmGitRun r = gq(a, checkout, false, args, NARGS(args));
    return first_line(a, &r);
}

char *shsm_git_short_head(AvpArena *a, const char *checkout) {
    const char *args[] = {"rev-parse", "--short", "HEAD"};
    ShsmGitRun r = gq(a, checkout, false, args, NARGS(args));
    return first_line(a, &r);
}

bool shsm_git_ref_exists(AvpArena *a, const char *checkout, const char *ref) {
    const char *args[] = {"rev-parse", "--verify", "--quiet", ref};
    ShsmGitRun r = gq(a, checkout, false, args, NARGS(args));
    return shsm_git_ok(&r);
}

bool shsm_git_branch_exists(AvpArena *a, const char *checkout, const char *branch) {
    return shsm_git_ref_exists(a, checkout, avp_sprintf(a, "refs/heads/%s", branch));
}

bool shsm_git_roots(AvpArena *a, const char *checkout, ShsmGitRoots *out) {
    const char *args[] = {"rev-parse", "--show-toplevel", "--git-common-dir"};
    ShsmGitRun r = gq(a, checkout, false, args, NARGS(args));
    if (!shsm_git_ok(&r)) return false;
    AvpStrVec lines = shsm_git_split_lines(r.out);
    bool ok = lines.n >= 2;
    if (ok) {
        out->top = avp_cz(a, avp_str_strip(lines.v[0]));
        out->common = resolve(a, checkout, avp_cz(a, avp_str_strip(lines.v[1])));
    }
    avp_vec_free(&lines);
    return ok;
}

char *shsm_git_toplevel(AvpArena *a, const char *checkout) {
    const char *args[] = {"rev-parse", "--show-toplevel"};
    ShsmGitRun r = gq(a, checkout, false, args, NARGS(args));
    return first_line(a, &r);
}

char *shsm_git_common_dir(AvpArena *a, const char *checkout) {
    const char *args[] = {"rev-parse", "--git-common-dir"};
    ShsmGitRun r = gq(a, checkout, false, args, NARGS(args));
    char *p = first_line(a, &r);
    return p ? resolve(a, checkout, p) : NULL;
}

char *shsm_git_git_path(AvpArena *a, const char *checkout, const char *name) {
    const char *args[] = {"rev-parse", "--git-path", name};
    ShsmGitRun r = gq(a, checkout, false, args, NARGS(args));
    char *p = first_line(a, &r);
    return p ? resolve(a, checkout, p) : NULL;
}

/* ---- ancestry -------------------------------------------------------------------------------------------------- */

char *shsm_git_merge_base(AvpArena *a, const char *checkout, const char *x, const char *y) {
    const char *args[] = {"merge-base", x, y};
    ShsmGitRun r = gq(a, checkout, false, args, NARGS(args));
    return first_line(a, &r);
}

int shsm_git_is_ancestor(AvpArena *a, const char *checkout, const char *anc, const char *desc) {
    const char *args[] = {"merge-base", "--is-ancestor", anc, desc};
    ShsmGitRun r = gq(a, checkout, false, args, NARGS(args));
    if (!r.started || r.timed_out) return -1;
    if (r.rc == 0) return 0;
    if (r.rc == 1) return 1;
    return -1;
}

bool shsm_git_rev_count(AvpArena *a, const char *checkout, const char *range, i64 *count) {
    const char *args[] = {"rev-list", "--count", range};
    ShsmGitRun r = gq(a, checkout, false, args, NARGS(args));
    if (!shsm_git_ok(&r)) return false;
    return avp_parse_int(avp_str_strip(r.out), 10, count, NULL);
}

/* ---- diff ------------------------------------------------------------------------------------------------------ */

bool shsm_git_diff_names(AvpArena *a, const char *checkout, const char *const *args, size_t n, AvpStrVec *out,
                         AvpStr *err) {
    const char *head[] = {"diff", "--name-only", "-z"};
    size_t total;
    const char **argv = cat_args(a, head, NARGS(head), args, n, &total);
    ShsmGitRun r = gq(a, checkout, true, argv, total);
    if (!finish(a, &r, argv, total, err)) return false;
    avp_vec_clear(out);
    AvpStrVec f = shsm_git_split_nul(r.out);
    for (size_t i = 0; i < f.n; i++)
        if (f.v[i].n) avp_vec_push(out, f.v[i]);
    avp_vec_free(&f);
    return true;
}

bool shsm_git_diff_text(AvpArena *a, const char *checkout, const char *const *args, size_t n, AvpStr *out, AvpStr *err) {
    const char *head[] = {"diff"};
    size_t total;
    const char **argv = cat_args(a, head, NARGS(head), args, n, &total);
    ShsmGitRun r = gq(a, checkout, false, argv, total);
    if (!finish(a, &r, argv, total, err)) return false;
    *out = r.out;
    return true;
}

bool shsm_git_diff_tree_names(AvpArena *a, const char *checkout, const char *sha, AvpStrVec *out, AvpStr *err) {
    const char *args[] = {"diff-tree", "--no-commit-id", "--name-only", "-r", "-z", sha};
    ShsmGitRun r = gq(a, checkout, true, args, NARGS(args));
    if (!finish(a, &r, args, NARGS(args), err)) return false;
    avp_vec_clear(out);
    AvpStrVec f = shsm_git_split_nul(r.out);
    for (size_t i = 0; i < f.n; i++)
        if (f.v[i].n) avp_vec_push(out, f.v[i]);
    avp_vec_free(&f);
    return true;
}

/* ---- objects --------------------------------------------------------------------------------------------------- */

bool shsm_git_blob(AvpArena *a, const char *checkout, const char *spec, AvpStr *data) {
    const char *args[] = {"cat-file", "blob", spec};
    ShsmGitRun r = gq(a, checkout, true, args, NARGS(args));
    if (!shsm_git_ok(&r)) return false;
    *data = r.out;
    return true;
}

bool shsm_git_show_blob(AvpArena *a, const char *checkout, const char *rev, const char *path, AvpStr *data) {
    return shsm_git_blob(a, checkout, avp_sprintf(a, "%s:%s", rev, path), data);
}

bool shsm_git_blob_head(AvpArena *a, const char *checkout, const char *spec, size_t n, AvpStr *data) {
    const char *args[] = {"cat-file", "blob", spec};
    AvpProcSpec ps = {0};
    ps.argv = make_argv(a, checkout, true, args, NARGS(args));
    ps.in = AVP_IO_NULL;
    ps.out = AVP_IO_CAPTURE;
    ps.err = AVP_IO_NULL;
    ps.arena = a;
    AvpProcResult fail = {0};
    AvpProc *p = avp_proc_start(&ps, &fail);
    if (!p) {
        const char *m = fail.message ? fail.message : "cannot start";
        shsm_raise(SHSM_EXC_USAGE, "cannot run git: %s", m);
    }
    char *buf = (char *)avp_alloc(a, n + 1);
    size_t got = n ? avp_proc_read_exact(p, 1, buf, n) : 0;
    bool found;
    if (got == n && n > 0) {
        found = true; /* more may follow: enough to answer, stop reading */
    } else {
        i64 rc = -1;
        found = avp_proc_wait(p, -1, &rc) && rc == 0;
    }
    avp_proc_close(p); /* kills a git still writing */
    if (!found) return false;
    *data = avp_strn(buf, got);
    return true;
}

bool shsm_git_object_exists(AvpArena *a, const char *checkout, const char *spec) {
    const char *args[] = {"cat-file", "-e", spec};
    ShsmGitRun r = gq(a, checkout, false, args, NARGS(args));
    return shsm_git_ok(&r);
}

bool shsm_git_object_size(AvpArena *a, const char *checkout, const char *spec, i64 *size) {
    const char *args[] = {"cat-file", "-s", spec};
    ShsmGitRun r = gq(a, checkout, false, args, NARGS(args));
    if (!shsm_git_ok(&r)) return false;
    return avp_parse_int(avp_str_strip(r.out), 10, size, NULL);
}

bool shsm_git_cat_batch(AvpArena *a, const char *checkout, const char *const *names, size_t n, ShsmGitObjectVec *out,
                        AvpStr *err) {
    avp_vec_clear(out);
    if (n == 0) return true;
    AvpBuf in;
    avp_buf_init(&in, a);
    for (size_t i = 0; i < n; i++) {
        avp_buf_putz(&in, names[i]);
        avp_buf_putc(&in, '\n');
    }
    const char *args[] = {"cat-file", "--batch"};
    ShsmGitOpts o = {0};
    o.quotepath_off = true;
    o.binary = true;
    o.input = avp_buf_cz(&in);
    o.input_n = in.n;
    ShsmGitRun r = shsm_git_run(a, checkout, args, NARGS(args), &o);
    if (!finish(a, &r, args, NARGS(args), err)) return false;
    size_t pos = 0;
    for (size_t i = 0; i < n; i++) {
        ShsmGitObject ob = {0};
        ob.type = AVP_S("");
        ob.data = AVP_S("");
        if (pos >= r.out.n) {
            avp_vec_push(out, ob);
            continue;
        }
        AvpStr rest = avp_strn(r.out.p + pos, r.out.n - pos);
        i64 nl = avp_str_findc(rest, '\n');
        if (nl < 0) nl = (i64)rest.n;
        AvpStr header = avp_strn(rest.p, (size_t)nl);
        pos += (size_t)nl + 1;
        AvpStrVec parts = avp_str_rsplit(header, AVP_S(" "), 2);
        i64 size = 0;
        if (parts.n == 3 && avp_parse_int(parts.v[2], 10, &size, NULL) && size >= 0 &&
            pos + (size_t)size <= r.out.n) {
            ob.found = true;
            ob.type = parts.v[1];
            ob.data = avp_strn(r.out.p + pos, (size_t)size);
            pos += (size_t)size + 1; /* the newline after the contents */
        }
        avp_vec_free(&parts);
        avp_vec_push(out, ob);
    }
    return true;
}

/* ---- index and trees ------------------------------------------------------------------------------------------- */

bool shsm_git_ls_files(AvpArena *a, const char *checkout, const char *const *args, size_t n, AvpStrVec *out,
                       AvpStr *err) {
    const char *head[] = {"ls-files", "-z"};
    size_t total;
    const char **argv = cat_args(a, head, NARGS(head), args, n, &total);
    ShsmGitRun r = gq(a, checkout, true, argv, total);
    if (!finish(a, &r, argv, total, err)) return false;
    avp_vec_clear(out);
    AvpStrVec f = shsm_git_split_nul(r.out);
    for (size_t i = 0; i < f.n; i++)
        if (f.v[i].n) avp_vec_push(out, f.v[i]);
    avp_vec_free(&f);
    return true;
}

/* "<A> <B> <C>\t<path>" records of -z output: the three fields and the path; false for a malformed record. */
static bool split_meta(AvpStr rec, AvpStrVec *meta, AvpStr *path) {
    i64 tab = avp_str_findc(rec, '\t');
    if (tab < 0) return false;
    *meta = avp_str_split(avp_strn(rec.p, (size_t)tab), AVP_S(""), -1);
    *path = avp_strn(rec.p + tab + 1, rec.n - (size_t)tab - 1);
    if (meta->n != 3) {
        avp_vec_free(meta);
        return false;
    }
    return true;
}

bool shsm_git_ls_files_stage(AvpArena *a, const char *checkout, const char *const *args, size_t n,
                             ShsmGitStageVec *out, AvpStr *err) {
    const char *head[] = {"ls-files", "-s", "-z"};
    size_t total;
    const char **argv = cat_args(a, head, NARGS(head), args, n, &total);
    ShsmGitRun r = gq(a, checkout, true, argv, total);
    if (!finish(a, &r, argv, total, err)) return false;
    avp_vec_clear(out);
    AvpStrVec f = shsm_git_split_nul(r.out);
    for (size_t i = 0; i < f.n; i++) {
        AvpStrVec meta = {0};
        AvpStr path;
        u64 mode = 0;
        i64 stage = 0;
        if (!split_meta(f.v[i], &meta, &path)) continue;
        if (avp_parse_uint(meta.v[0], 8, &mode) && avp_parse_int(meta.v[2], 10, &stage, NULL)) {
            ShsmGitStageEntry e = {(u32)mode, (int)stage, meta.v[1], path};
            avp_vec_push(out, e);
        }
        avp_vec_free(&meta);
    }
    avp_vec_free(&f);
    return true;
}

bool shsm_git_ls_tree(AvpArena *a, const char *checkout, const char *rev, const char *const *paths, size_t npaths,
                      ShsmGitTreeVec *out, AvpStr *err) {
    const char *head[] = {"ls-tree", "-r", "-z", rev};
    const char *dd[] = {"--"};
    size_t n1;
    const char **v1 = cat_args(a, head, NARGS(head), npaths ? dd : NULL, npaths ? 1 : 0, &n1);
    size_t total;
    const char **argv = cat_args(a, v1, n1, paths, npaths, &total);
    ShsmGitRun r = gq(a, checkout, true, argv, total);
    if (!finish(a, &r, argv, total, err)) return false;
    avp_vec_clear(out);
    AvpStrVec f = shsm_git_split_nul(r.out);
    for (size_t i = 0; i < f.n; i++) {
        AvpStrVec meta = {0};
        AvpStr path;
        u64 mode = 0;
        if (!split_meta(f.v[i], &meta, &path)) continue;
        if (avp_parse_uint(meta.v[0], 8, &mode)) {
            ShsmGitTreeEntry e = {(u32)mode, meta.v[1], meta.v[2], path};
            avp_vec_push(out, e);
        }
        avp_vec_free(&meta);
    }
    avp_vec_free(&f);
    return true;
}

bool shsm_git_check_ignore(AvpArena *a, const char *checkout, const char *const *paths, size_t n, AvpStrVec *ignored,
                           AvpStr *err) {
    avp_vec_clear(ignored);
    if (n == 0) return true;
    AvpBuf in;
    avp_buf_init(&in, a);
    for (size_t i = 0; i < n; i++) {
        avp_buf_putz(&in, paths[i]);
        avp_buf_putc(&in, '\0');
    }
    const char *args[] = {"check-ignore", "-z", "--stdin"};
    ShsmGitOpts o = {0};
    o.quotepath_off = true;
    o.binary = true;
    o.input = in.p;
    o.input_n = in.n;
    ShsmGitRun r = shsm_git_run(a, checkout, args, NARGS(args), &o);
    if (r.started && !r.timed_out && r.rc == 1) return true; /* none of them is ignored */
    if (!finish(a, &r, args, NARGS(args), err)) return false;
    AvpStrVec f = shsm_git_split_nul(r.out);
    for (size_t i = 0; i < f.n; i++)
        if (f.v[i].n) avp_vec_push(ignored, f.v[i]);
    avp_vec_free(&f);
    return true;
}

/* ---- worktrees ------------------------------------------------------------------------------------------------- */

static char *dup_value(AvpArena *a, AvpStr line, size_t skip) {
    return avp_cz(a, avp_strn(line.p + skip, line.n - skip));
}

void shsm_git_parse_worktrees(AvpArena *a, AvpStr text, ShsmGitWorktreeVec *out) {
    AvpStrVec lines = shsm_git_split_lines(text);
    bool have = false;
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr ln = lines.v[i];
        if (avp_str_startswith(ln, AVP_S("worktree "))) {
            ShsmGitWorktree w = {0};
            w.path = dup_value(a, ln, 9);
            w.npath = avp_path_normpath(a, w.path);
            w.head = "";
            avp_vec_push(out, w);
            have = true;
            continue;
        }
        if (!have) continue;
        ShsmGitWorktree *cur = &out->v[out->n - 1]; /* re-taken per line: pushing a record moves the vector */
        if (avp_str_startswith(ln, AVP_S("HEAD "))) {
            cur->head = dup_value(a, ln, 5);
        } else if (avp_str_startswith(ln, AVP_S("branch "))) {
            cur->branch = dup_value(a, ln, 7);
        } else if (avp_str_eq(ln, AVP_S("bare"))) {
            cur->bare = true;
        } else if (avp_str_eq(ln, AVP_S("detached"))) {
            cur->detached = true;
        } else if (avp_str_startswith(ln, AVP_S("locked"))) {
            cur->locked = true;
            if (ln.n > 7) cur->locked_reason = dup_value(a, ln, 7);
        } else if (avp_str_startswith(ln, AVP_S("prunable"))) {
            cur->prunable = true;
            if (ln.n > 9) cur->prunable_reason = dup_value(a, ln, 9);
        }
    }
    avp_vec_free(&lines);
}

bool shsm_git_worktrees(AvpArena *a, const char *checkout, ShsmGitWorktreeVec *out, AvpStr *err) {
    const char *args[] = {"worktree", "list", "--porcelain"};
    ShsmGitRun r = gq(a, checkout, false, args, NARGS(args));
    if (!finish(a, &r, args, NARGS(args), err)) return false;
    avp_vec_clear(out);
    shsm_git_parse_worktrees(a, r.out, out);
    return true;
}

const char *shsm_git_branch_short(const char *ref) {
    if (!ref) return NULL;
    return strncmp(ref, "refs/heads/", 11) == 0 ? ref + 11 : ref;
}

char *shsm_git_path_key(AvpArena *a, const char *path) {
    const char *p = path;
    char *pre_unc = NULL;
    if (strncmp(p, "\\\\?\\UNC\\", 8) == 0) {
        pre_unc = avp_sprintf(a, "\\\\%s", p + 8);
        p = pre_unc;
    } else if (strncmp(p, "\\\\?\\", 4) == 0 || strncmp(p, "\\??\\", 4) == 0) {
        p += 4;
    }
    return avp_path_normcase(a, avp_path_normpath(a, avp_path_abspath(a, p)));
}

/* ---- merge-tree ------------------------------------------------------------------------------------------------ */

bool shsm_git_merge_tree(AvpArena *a, const char *checkout, const char *x, const char *y, ShsmGitMergeTree *out,
                         AvpStr *err) {
    const char *args[] = {"merge-tree", "--write-tree", "--name-only", "--no-messages", "-z", x, y};
    ShsmGitRun r = gq(a, checkout, true, args, NARGS(args));
    if (r.started && !r.timed_out && (r.rc == 0 || r.rc == 1)) {
        memset(out, 0, sizeof *out);
        out->clean = r.rc == 0;
        AvpStrVec f = shsm_git_split_nul(r.out);
        if (f.n) out->tree = avp_cz(a, avp_str_strip(f.v[0]));
        for (size_t i = 1; i < f.n; i++)
            if (f.v[i].n) avp_vec_push(&out->conflicts, f.v[i]);
        avp_vec_free(&f);
        if (out->tree) return true;
        if (err) {
            *err = AVP_S("merge-tree printed no tree id");
            return false;
        }
        shsm_raise(SHSM_EXC_USAGE, "git merge-tree %s %s failed: no tree id in the output", x, y);
    }
    return finish(a, &r, args, NARGS(args), err);
}

/* ---- log ------------------------------------------------------------------------------------------------------- */

char *shsm_git_last_commit_for_path(AvpArena *a, const char *checkout, const char *path) {
    const char *args[] = {"log", "-1", "--format=%H", "--", path};
    ShsmGitRun r = gq(a, checkout, false, args, NARGS(args));
    return first_line(a, &r);
}

bool shsm_git_log_path_shas(AvpArena *a, const char *checkout, const char *path, bool first_parent, AvpStrVec *out,
                            AvpStr *err) {
    const char *with_fp[] = {"log", "--format=%H", "--reverse", "--first-parent", "--", path};
    const char *plain[] = {"log", "--format=%H", "--reverse", "--", path};
    const char *const *args = first_parent ? with_fp : plain;
    size_t n = first_parent ? NARGS(with_fp) : NARGS(plain);
    ShsmGitRun r = gq(a, checkout, false, args, n);
    if (!finish(a, &r, args, n, err)) return false;
    avp_vec_clear(out);
    push_stripped_lines(r.out, out);
    return true;
}

char *shsm_git_subject(AvpArena *a, const char *checkout, const char *rev) {
    const char *args[] = {"log", "-1", "--format=%s", rev};
    ShsmGitRun r = gq(a, checkout, false, args, NARGS(args));
    if (!shsm_git_ok(&r)) return NULL;
    return avp_cz(a, avp_str_strip(r.out));
}

bool shsm_git_commit_info(AvpArena *a, const char *checkout, const char *rev, i64 *commit_time, char **subject) {
    const char *args[] = {"show", "-s", "--format=%ct%x09%s", rev};
    ShsmGitRun r = gq(a, checkout, false, args, NARGS(args));
    if (!shsm_git_ok(&r)) return false;
    AvpStr before, after;
    avp_str_partition(avp_str_strip(r.out), AVP_S("\t"), &before, &after);
    i64 t;
    if (!avp_parse_int(before, 10, &t, NULL)) return false;
    *commit_time = t;
    *subject = avp_cz(a, after);
    return true;
}

bool shsm_git_branches_no_merged(AvpArena *a, const char *checkout, const char *onto, AvpStrVec *out, AvpStr *err) {
    const char *args[] = {"branch", "--no-merged", onto, "--format=%(refname:short)"};
    ShsmGitRun r = gq(a, checkout, false, args, NARGS(args));
    if (!finish(a, &r, args, NARGS(args), err)) return false;
    avp_vec_clear(out);
    push_stripped_lines(r.out, out);
    return true;
}
