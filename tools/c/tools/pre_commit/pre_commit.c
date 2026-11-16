/* pre_commit: the hook's logic and command line (hooks/pre_commit.py). */
#include "pre_commit.h"
#include "rules_cli.h"
#include "shsm_cli.h"
#include "shsm_git.h"

#include <stdio.h>
#include <string.h>

#define A avp_arena()
#define FMT(...) ((char *)avp_pyfmt(A, __VA_ARGS__).p)

static const char *const blocked_dirs[] = {"build/", "tools/compilers/", "tools/bin/", "tools/src/"};
static const char *const blocked_exts[] = {".dol", ".rel", ".elf", ".o", ".rvz", ".iso", ".wbfs", ".gcm"};
static const char *const source_exts[] = {".c", ".cpp", ".cc", ".h", ".hpp", ".inc"};
#define NARR(x) (sizeof(x) / sizeof((x)[0]))

static bool ends_with_any(AvpStr s, const char *const *exts, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (avp_str_endswith(s, avp_str(exts[i]))) return true;
    return false;
}

PreCommitRefusals pre_commit_decide(AvpArena *a, const PreCommitEntry *entries, size_t n) {
    PreCommitRefusals bad = {0};
    for (size_t i = 0; i < n; i++) {
        const PreCommitEntry *e = &entries[i];
        AvpStr path = avp_str(e->path);
        AvpStr low = avp_str_lower(a, path);
        AvpStr before, name;
        if (!avp_str_rpartition(low, AVP_S("/"), &before, &name)) name = low;
        if (avp_str_startswith(low, AVP_S("orig/"))) {
            if (!avp_str_eqz(path, "orig/INVENTORY.md") && !avp_str_eqz(name, ".gitkeep"))
                avp_vec_push(&bad, ((PreCommitRefusal){e->path, "original game files are never committed"}));
            continue;
        }
        const char *hit = NULL;
        for (size_t k = 0; k < NARR(blocked_dirs) && !hit; k++)
            if (avp_str_startswith(low, avp_str(blocked_dirs[k]))) hit = blocked_dirs[k];
        if (hit)
            avp_vec_push(&bad, ((PreCommitRefusal){e->path, avp_pyfmt(a, "%s is never committed", hit).p}));
        else if (ends_with_any(low, blocked_exts, NARR(blocked_exts)))
            avp_vec_push(&bad, ((PreCommitRefusal){e->path, "binary image/object type"}));
        else if (e->binary && avp_str_startswith(low, AVP_S("ref/")))
            avp_vec_push(&bad, ((PreCommitRefusal){e->path, "binary file under ref/ (only text outputs are committed)"}));
        else if (e->binary && e->size > PRE_COMMIT_MB)
            avp_vec_push(&bad, ((PreCommitRefusal){e->path, avp_pyfmt(a, "binary blob of %.1f MB",
                                                                        (double)e->size / PRE_COMMIT_MB).p}));
    }
    return bad;
}

/* git(*args): `git -c core.quotepath=off ARGS` in the current folder, stdout decoded (utf-8, errors="replace", no
 * newline translation). Raises the Python's RuntimeError text as SHSM_EXC_USAGE. */
static AvpStr git(const char *const *args, size_t n) {
    ShsmGitOpts o = {.quotepath_off = true, .binary = true, .soft_start = true};
    ShsmGitRun r = shsm_git_run(A, NULL, args, n, &o);
    if (!r.started) shsm_raise(SHSM_EXC_OS, "[WinError 2] The system cannot find the file specified");
    AvpStr out;
    if (!avp_decode_text(A, r.out, AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_REPLACE, AVP_NL_LF), &out)) out = r.out;
    if (r.rc != 0) {
        AvpStr err;
        if (!avp_decode_text(A, r.err, AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_REPLACE, AVP_NL_LF), &err)) err = r.err;
        AvpBuf b;
        avp_buf_init(&b, A);
        for (size_t i = 0; i < n; i++) {
            if (i) avp_buf_putc(&b, ' ');
            avp_buf_putz(&b, args[i]);
        }
        AvpStr e = avp_str_strip(err);
        shsm_raise(SHSM_EXC_USAGE, "git %s: %.*s", avp_buf_cz(&b), AVP_SF(e));
    }
    return out;
}
#define GIT(...) git((const char *const[]){__VA_ARGS__}, sizeof((const char *const[]){__VA_ARGS__}) / sizeof(char *))

PreCommitBatches pre_commit_batches(const AvpStr *names, size_t n, size_t limit) {
    PreCommitBatches out = {0};
    /* "git -c core.quotepath=off ls-files -s -z --" and, per name, a space, the name and its quotes */
    size_t base = 48, total = base;
    for (size_t i = 0; i < n; i++) total += names[i].n + 3;
    if (total <= limit || n == 0) {
        avp_vec_push(&out, ((PreCommitBatch){0, n}));
        return out;
    }
    size_t start = 0, len = base;
    for (size_t i = 0; i < n; i++) {
        size_t add = names[i].n + 3;
        if (i > start && len + add > limit) {
            avp_vec_push(&out, ((PreCommitBatch){start, i}));
            start = i;
            len = base;
        }
        len += add;
    }
    avp_vec_push(&out, ((PreCommitBatch){start, n}));
    return out;
}

PreCommitEntries pre_commit_staged_entries(AvpArena *a) {
    PreCommitEntries out = {0};
    AvpStr z = GIT("diff", "--cached", "--name-only", "--diff-filter=ACMRT", "-z");
    AvpStrVec all = avp_str_split(z, AVP_S("\0"), -1);
    AvpStrVec names = {0};
    for (size_t i = 0; i < all.n; i++)
        if (all.v[i].n) avp_vec_push(&names, all.v[i]);
    if (!names.n) return out;
    PreCommitBatches batches = pre_commit_batches(names.v, names.n, PRE_COMMIT_CMDLINE_MAX);
    for (size_t b = 0; b < batches.n; b++) {
        size_t cnt = batches.v[b].end - batches.v[b].start;
        const char **args = AVP_NEWN(A, const char *, cnt + 4);
        args[0] = "ls-files", args[1] = "-s", args[2] = "-z", args[3] = "--";
        for (size_t i = 0; i < cnt; i++) args[4 + i] = avp_cz(A, names.v[batches.v[b].start + i]);
        AvpStrVec recs = avp_str_split(git(args, cnt + 4), AVP_S("\0"), -1);
        for (size_t i = 0; i < recs.n; i++) {
            AvpStr ln = recs.v[i];
            if (!ln.n) continue;
            AvpStr meta, path;
            if (!avp_str_partition(ln, AVP_S("\t"), &meta, &path)) shsm_raise(SHSM_EXC_VALUE, "not enough values to unpack (expected 2, got 1)");
            AvpStrVec f = avp_str_split(meta, (AvpStr){0}, -1);
            if (f.n != 3) shsm_raise(SHSM_EXC_VALUE, "not enough values to unpack (expected 3, got %zu)", f.n);
            if (avp_str_eqz(f.v[0], "160000")) continue; /* submodule */
            const char *sha = avp_cz(A, f.v[1]);
            AvpStr sz = avp_str_strip(GIT("cat-file", "-s", sha));
            i64 size = 0;
            if (!avp_parse_int(sz, 10, &size, NULL)) shsm_raise(SHSM_EXC_VALUE, "invalid literal for int() with base 10: %.*s", AVP_SF(sz));
            AvpStr head = {0};
            if (!shsm_git_blob_head(A, NULL, sha, 8000, &head)) head = AVP_S("");
            bool binary = memchr(head.p, 0, head.n) != NULL;
            avp_vec_push(&out, ((PreCommitEntry){avp_cz(a, path), size, binary}));
        }
    }
    return out;
}

/* rule_warnings(paths): the rule checker's findings, never blocking (the C checker in-process: D-pre_commit-2). */
static void rule_warnings(const PreCommitEntries *entries) {
    AvpPtrVec src = {0};
    for (size_t i = 0; i < entries->n; i++) {
        AvpStr p = avp_str(entries->v[i].path);
        if (!avp_str_startswith(p, AVP_S("src/")) && !avp_str_startswith(p, AVP_S("include/"))) continue;
        if (!ends_with_any(avp_str_lower(A, p), source_exts, NARR(source_exts))) continue;
        if (!avp_isfile(entries->v[i].path)) continue;
        avp_vec_push(&src, (void *)entries->v[i].path);
    }
    if (!src.n) return;
    /* check_rules <paths>, run in-process (tool_check_rules); its root is the checkout (cwd is the toplevel) */
    RlRunOpts o = {0};
    o.root = avp_path_abspath(A, ".");
    for (size_t i = 0; i < src.n; i++) avp_vec_push(&o.paths, avp_str(src.v[i]));
    AvpBuf out, err;
    avp_buf_init(&out, A);
    avp_buf_init(&err, A);
    int rc = 1;
    AvpTry t;
    if (AVP_TRY(&t)) {
        rc = rl_run(A, &o, &out, &err);
        avp_try_end(&t);
    } else { /* an internal error of the checker, reported with its findings */
        avp_buf_printf(&err, "pre_commit.exe: error: %s: %s\n", shsm_exc_class(t.code), t.msg);
    }
    AvpStrVec all = avp_str_splitlines(avp_str_concat(A, avp_buf_str(&out), avp_buf_str(&err)), false);
    AvpStrVec lines = {0};
    for (size_t i = 0; i < all.n; i++)
        if (avp_str_strip(all.v[i]).n) avp_vec_push(&lines, all.v[i]);
    if (rc != 0 && lines.n) {
        fputs("pre-commit: warning: rule checker findings (not blocking):\n", stdout);
        for (size_t i = 0; i < lines.n && i < PRE_COMMIT_MAX_RULE_LINES; i++) avp_pyprintf("  %S\n", &lines.v[i]);
        if (lines.n > PRE_COMMIT_MAX_RULE_LINES)
            avp_pyprintf("  ... %d more lines\n", (i64)(lines.n - PRE_COMMIT_MAX_RULE_LINES));
    }
}

int pre_commit_main(int argc, char **argv) {
    AvpArgParser *ap =
        avp_args_parser(NULL, &(AvpArgParserDesc){.prog = "pre_commit.exe", .description = "pre-commit hook (see module docstring)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--no-rules", .action = AVP_ARG_STORE_TRUE});
    AvpArgs *args = avp_args_parse(ap, argc, argv);
    PreCommitEntries entries = {0};
    AvpTry t;
    if (AVP_TRY(&t)) {
        AvpStr top = avp_str_strip(GIT("rev-parse", "--show-toplevel"));
        if (!avp_chdir(avp_cz(A, top))) shsm_raise_io();
        entries = pre_commit_staged_entries(A);
        avp_try_end(&t);
    } else {
        fflush(stdout);
        if (!shsm_exc_is_os(t.code) && t.code != SHSM_EXC_USAGE) { /* not caught by the Python: a traceback, exit 1 */
            fprintf(stderr, "%s: %s\n", shsm_exc_class(t.code), t.msg);
            return 1;
        }
        fprintf(stderr, "pre-commit: error: %s\n", t.msg);
        return 2;
    }
    PreCommitRefusals bad = pre_commit_decide(A, entries.v, entries.n);
    if (bad.n) {
        avp_pyprintf("pre-commit: refused %d staged file(s):\n", (i64)bad.n);
        for (size_t i = 0; i < bad.n && i < 40; i++) avp_pyprintf("  %s: %s\n", bad.v[i].path, bad.v[i].reason);
        if (bad.n > 40) avp_pyprintf("  ... %d more\n", (i64)(bad.n - 40));
        fputs("unstage them with `git restore --staged <path>`\n", stdout);
        return 1;
    }
    if (!avp_args_bool(args, "no_rules")) rule_warnings(&entries);
    return 0;
}
