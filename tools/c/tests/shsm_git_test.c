/* shsm_git: every function of lib/shsm_git.h against temporary repositories (fixture folders with spaces in their
 * names). The shared "main" repository is built once and only read afterwards; tests that change a repository use
 * their own. */
#include "avp/proc.h"
#include "shsm_git.h"
#include "shsm_test_util.h"

#include <string.h>

#define G(repo, ...) SHSM_GIT_CHECK(avp_arena(), (repo), __VA_ARGS__)
#define NARR(arr) (sizeof(arr) / sizeof((arr)[0]))

/* fx_dir(), after removing a repository of an earlier run (git objects are read-only files). */
static const char *gdir(const char *name) {
    AvpArena *a = avp_arena();
    const char *d = avp_path_abspath(a, avp_join(a, "build", "tmp", "cpilot", "B", name));
    if (avp_isdir(d)) AVP_ASSERT(avp_rmtree(d, AVP_RMTREE_CLEAR_READONLY));
    return fx_dir(name);
}

static bool is_sha(const char *s) {
    if (!s || strlen(s) != 40) return false;
    for (const char *p = s; *p; p++)
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f'))) return false;
    return true;
}

static bool vec_has(const AvpStrVec *v, const char *s) {
    for (size_t i = 0; i < v->n; i++)
        if (avp_str_eqz(v->v[i], s)) return true;
    return false;
}

static void commit_all(const char *repo, const char *msg) {
    G(repo, "add", "-A");
    G(repo, "commit", "-q", "-m", msg);
}

static const char *init_repo(const char *fixture) {
    const char *d = gdir(fixture);
    G(d, "init", "-q", "-b", "main");
    G(d, "config", "user.name", "Test User");
    G(d, "config", "user.email", "test@example.invalid");
    G(d, "config", "commit.gpgsign", "false");
    G(d, "config", "core.autocrlf", "false");
    G(d, "config", "core.safecrlf", "false");
    return d;
}

/* The shared repository: C1 -> C2 (rename, new files, a non-ASCII name) -> C3 on main; topic and clash start at C2. */
static const char *g_repo;
static char *g_c1, *g_c2, *g_c3, *g_topic, *g_clash;
#define NON_ASCII "caf\xc3\xa9.txt"

static const char *main_repo(void) {
    if (g_repo) return g_repo;
    AvpArena *a = avp_arena();
    const char *r = init_repo("git main repo");
    fx_write(r, "a.txt", "one\n");
    fx_write(r, "dir/b.txt", "bee\n");
    commit_all(r, "first commit");
    g_c1 = shsm_git_rev_parse(a, r, "HEAD");
    G(r, "mv", "dir/b.txt", "dir/c.txt");
    fx_write(r, "a.txt", "two\n");
    fx_write(r, "new file.txt", "x\n");
    fx_write(r, NON_ASCII, "cafe\n");
    commit_all(r, "second: rename");
    g_c2 = shsm_git_rev_parse(a, r, "HEAD");
    G(r, "branch", "topic");
    G(r, "branch", "clash");
    G(r, "checkout", "-q", "topic");
    fx_write(r, "t.txt", "t\n");
    commit_all(r, "topic work");
    g_topic = shsm_git_rev_parse(a, r, "HEAD");
    G(r, "checkout", "-q", "clash");
    fx_write(r, "a.txt", "clash\n");
    commit_all(r, "clash work");
    g_clash = shsm_git_rev_parse(a, r, "HEAD");
    G(r, "checkout", "-q", "main");
    fx_write(r, "a.txt", "main\n");
    commit_all(r, "third on main");
    g_c3 = shsm_git_rev_parse(a, r, "HEAD");
    g_repo = r;
    return r;
}

/* ---- the runner ------------------------------------------------------------------------------------------------ */

AVP_TEST(run_basics) {
    AvpArena *a = avp_arena();
    CHECK(shsm_git_available(a));
    ShsmGitRun v = SHSM_GIT(a, NULL, "--version");
    CHECK(shsm_git_ok(&v) && v.started && !v.timed_out && v.rc == 0);
    CHECK(avp_str_startswith(v.out, AVP_S("git version ")));
    CHECK_EQ_U64(v.err.n, 0);
    CHECK(v.err.p != NULL);

    ShsmGitRun bad = SHSM_GIT(a, NULL, "no-such-subcommand");
    CHECK(!shsm_git_ok(&bad) && bad.started && bad.rc != 0 && bad.err.n > 0);
    CHECK(shsm_git_failure_text(&bad).n > 0);
    /* shsm_git_check: the message is "git <args> failed: <stderr>" */
    AvpTry t;
    if (AVP_TRY(&t)) {
        SHSM_GIT_CHECK(a, NULL, "no-such-subcommand", "--flag");
        avp_try_end(&t);
        CHECK_MSG(false, "shsm_git_check did not raise");
    } else {
        CHECK_EQ_I64(t.code, SHSM_EXC_USAGE);
        CHECK_MSG(strncmp(t.msg, "git no-such-subcommand --flag failed: ", 38) == 0, "message: %s", t.msg);
    }

    /* stdin bytes */
    ShsmGitOpts o = {0};
    o.input = "hello\n";
    o.input_n = 6;
    ShsmGitRun h = SHSM_GIT_OPTS(a, NULL, &o, "hash-object", "--stdin");
    CHECK(shsm_git_ok(&h));
    CHECK_STR(avp_str_strip(h.out), "ce013625030ba8dba906f756967f9e9ca394464a");
    /* environment additions (names upper-cased) */
    const char *env[] = {"git_author_name=Env Tester", "GIT_AUTHOR_EMAIL=e@x.invalid", NULL};
    ShsmGitOpts oe = {0};
    oe.env = env;
    ShsmGitRun id = SHSM_GIT_OPTS(a, NULL, &oe, "var", "GIT_AUTHOR_IDENT");
    CHECK(shsm_git_ok(&id));
    CHECK(avp_str_startswith(id.out, AVP_S("Env Tester <e@x.invalid> ")));
    /* a timeout kills the run */
    ShsmGitOpts ot = {0};
    ot.timeout = 0.5;
    ShsmGitRun slow = SHSM_GIT_OPTS(a, NULL, &ot, "-c", "alias.slow=!sleep 20", "slow");
    CHECK(slow.timed_out && slow.rc == -1 && !shsm_git_ok(&slow));
    CHECK_CZ(avp_cz(a, shsm_git_failure_text(&slow)), "timed out");
    /* -C: runs in the given folder */
    const char *r = main_repo();
    ShsmGitRun top = SHSM_GIT(a, r, "rev-parse", "--is-inside-work-tree");
    CHECK_STR(avp_str_strip(top.out), "true");
}

AVP_TEST(run_missing_git) {
    AvpArena *a = avp_arena();
    /* An empty PATH hides git from CreateProcess's search (the repository root has no git.exe). */
    char *saved = avp_getenv(a, "PATH");
    const char *git_path = avp_which(a, "git", NULL, NULL);
    if (!saved || !git_path) return;
    avp_setenv("PATH", "C:\\no-such-folder");
    ShsmGitOpts soft = {0};
    soft.soft_start = true;
    const char *args[] = {"--version"};
    ShsmGitRun r = shsm_git_run(a, NULL, args, 1, &soft);
    int code = 0;
    char msg[512] = "";
    AvpTry t;
    if (AVP_TRY(&t)) {
        shsm_git_run(a, NULL, args, 1, NULL);
        avp_try_end(&t);
    } else {
        code = t.code;
        snprintf(msg, sizeof msg, "%s", t.msg);
    }
    avp_setenv("PATH", saved);
    CHECK(!r.started && r.rc == -1 && !shsm_git_ok(&r) && r.err.n > 0);
    if (r.started) return; /* git was found anyway (found next to this executable): nothing more to check */
    CHECK_EQ_I64(code, SHSM_EXC_USAGE);
    CHECK_MSG(strncmp(msg, "cannot run git: ", 16) == 0, "message: %s", msg);
}

AVP_TEST(quotepath) {
    AvpArena *a = avp_arena();
    const char *r = main_repo();
    ShsmGitRun quoted = SHSM_GIT(a, r, "ls-files");
    CHECK(avp_str_contains(quoted.out, AVP_S("\"caf\\303\\251.txt\"")));
    ShsmGitOpts o = {0};
    o.quotepath_off = true;
    ShsmGitRun raw = SHSM_GIT_OPTS(a, r, &o, "ls-files");
    CHECK(avp_str_contains(raw.out, AVP_S("\ncaf\xc3\xa9.txt\n")));
}

AVP_TEST(split_helpers) {
    AvpStrVec l = shsm_git_split_lines(AVP_S("a\n\nb c\nd"));
    CHECK_EQ_U64(l.n, 4);
    CHECK_STR(l.v[1], "");
    CHECK_STR(l.v[2], "b c");
    CHECK_STR(l.v[3], "d");
    AvpStrVec l2 = shsm_git_split_lines(AVP_S("x\n"));
    CHECK_EQ_U64(l2.n, 1);
    AvpStrVec l3 = shsm_git_split_lines(AVP_S(""));
    CHECK_EQ_U64(l3.n, 0);
    AvpStrVec z = shsm_git_split_nul(avp_strn("a\0\0b c\0", 7));
    CHECK_EQ_U64(z.n, 3);
    CHECK_STR(z.v[1], "");
    CHECK_STR(z.v[2], "b c");
    AvpStrVec z2 = shsm_git_split_nul(avp_strn("q", 1));
    CHECK_EQ_U64(z2.n, 1);
    avp_vec_free(&l);
    avp_vec_free(&l2);
    avp_vec_free(&l3);
    avp_vec_free(&z);
    avp_vec_free(&z2);
}

/* ---- status, dirty --------------------------------------------------------------------------------------------- */

AVP_TEST(parse_status_synthetic) {
    static const char z[] = "R  new name\0old name\0 M x\0?? y z\0C  c\0o\0UU m\0";
    ShsmGitStatusVec v = {0};
    shsm_git_parse_status(avp_strn(z, sizeof z - 1), &v);
    REQUIRE(v.n == 5);
    CHECK_CZ(v.v[0].xy, "R ");
    CHECK_STR(v.v[0].path, "new name");
    CHECK(v.v[0].has_orig);
    CHECK_STR(v.v[0].orig_path, "old name");
    CHECK_CZ(v.v[1].xy, " M");
    CHECK(!v.v[1].has_orig);
    CHECK_STR(v.v[2].path, "y z");
    CHECK_CZ(v.v[3].xy, "C ");
    CHECK_STR(v.v[3].orig_path, "o");
    CHECK_CZ(v.v[4].xy, "UU");
    avp_vec_free(&v);
}

AVP_TEST(status_dirty_and_diff_cached) {
    AvpArena *a = avp_arena();
    const char *r = init_repo("git status repo");
    fx_write(r, "a.txt", "one\n");
    fx_write(r, "dir/b.txt", "bee\n");
    commit_all(r, "first");
    CHECK(!shsm_git_dirty(a, r));
    ShsmGitStatusVec st = {0};
    AvpStr err = {0};
    CHECK(shsm_git_status(a, r, true, &st, &err));
    CHECK_EQ_U64(st.n, 0);

    G(r, "mv", "dir/b.txt", "dir/c.txt");
    fx_write(r, "a.txt", "two\n");
    fx_write(r, "new file.txt", "x\n");
    fx_write(r, "sub/deep/u.txt", "u\n");
    fx_write(r, NON_ASCII, "cafe\n");
    CHECK(shsm_git_dirty(a, r));
    CHECK(shsm_git_status(a, r, true, &st, &err));
    CHECK_EQ_U64(st.n, 5);
    int seen = 0;
    for (size_t i = 0; i < st.n; i++) {
        const ShsmGitStatusEntry *e = &st.v[i];
        if (avp_str_eqz(e->path, "dir/c.txt")) {
            CHECK_CZ(e->xy, "R ");
            CHECK(e->has_orig && avp_str_eqz(e->orig_path, "dir/b.txt"));
            seen |= 1;
        } else if (avp_str_eqz(e->path, "a.txt")) {
            CHECK_CZ(e->xy, " M");
            CHECK(!e->has_orig);
            seen |= 2;
        } else if (avp_str_eqz(e->path, "new file.txt")) {
            CHECK_CZ(e->xy, "??");
            seen |= 4;
        } else if (avp_str_eqz(e->path, "sub/deep/u.txt")) {
            CHECK_CZ(e->xy, "??");
            seen |= 8;
        } else if (avp_str_eqz(e->path, NON_ASCII)) {
            seen |= 16;
        }
    }
    CHECK_EQ_I64(seen, 31);
    /* without -uall an untracked folder is one entry "sub/" */
    CHECK(shsm_git_status(a, r, false, &st, &err));
    CHECK_EQ_U64(st.n, 5);
    bool folder = false;
    for (size_t i = 0; i < st.n; i++) folder |= avp_str_eqz(st.v[i].path, "sub/");
    CHECK(folder);

    /* diff --cached with a filter (pre_commit.staged_entries) */
    AvpStrVec names = {0};
    G(r, "add", "-A");
    const char *cached[] = {"--cached", "--diff-filter=ACMRT"};
    CHECK(shsm_git_diff_names(a, r, cached, 2, &names, &err));
    CHECK_EQ_U64(names.n, 5);
    CHECK(vec_has(&names, "dir/c.txt") && vec_has(&names, "a.txt") && vec_has(&names, "new file.txt") &&
          vec_has(&names, "sub/deep/u.txt") && vec_has(&names, NON_ASCII));
    /* ls-files -s -z over those names: modes, stage, sha, the gitlink constant */
    ShsmGitStageVec stage = {0};
    const char *just[] = {"--", "new file.txt", "dir/c.txt"};
    CHECK(shsm_git_ls_files_stage(a, r, just, 3, &stage, &err));
    REQUIRE(stage.n == 2);
    CHECK_EQ_U64(stage.v[0].mode, 0100644);
    CHECK_EQ_I64(stage.v[0].stage, 0);
    CHECK_STR(stage.v[0].path, "dir/c.txt");
    CHECK_STR(stage.v[1].path, "new file.txt");
    CHECK_EQ_U64(stage.v[1].sha.n, 40);
    char *sub = shsm_git_rev_parse(a, r, "HEAD");
    G(r, "update-index", "--add", "--cacheinfo", avp_sprintf(a, "160000,%s,gitlink dir", sub));
    const char *glink[] = {"--", "gitlink dir"};
    CHECK(shsm_git_ls_files_stage(a, r, glink, 2, &stage, &err));
    REQUIRE(stage.n == 1);
    CHECK_EQ_U64(stage.v[0].mode, SHSM_GIT_MODE_SUBMODULE);
    avp_vec_free(&st);
    avp_vec_free(&names);
    avp_vec_free(&stage);
}

AVP_TEST(failures_report_or_raise) {
    AvpArena *a = avp_arena();
    const char *gone = avp_join(a, gdir("git failures"), "no such folder");
    ShsmGitStatusVec st = {0};
    AvpStr err = {0};
    CHECK(!shsm_git_status(a, gone, true, &st, &err));
    CHECK(err.n > 0);
    CHECK_RAISES(shsm_git_status(a, gone, true, &st, NULL), SHSM_EXC_USAGE, NULL);
    char want[1024];
    snprintf(want, sizeof want, "git status failed in %s", gone);
    CHECK_RAISES(shsm_git_dirty(a, gone), SHSM_EXC_USAGE, want);
    CHECK(shsm_git_rev_parse(a, gone, "HEAD") == NULL);
    CHECK(shsm_git_branch(a, gone) == NULL);
    CHECK(shsm_git_toplevel(a, gone) == NULL);
    CHECK(shsm_git_common_dir(a, gone) == NULL);
    CHECK(shsm_git_git_path(a, gone, "hooks") == NULL);
    ShsmGitRoots roots = {0};
    CHECK(!shsm_git_roots(a, gone, &roots));
    CHECK(!shsm_git_ref_exists(a, gone, "refs/heads/main"));
    CHECK(shsm_git_merge_base(a, gone, "a", "b") == NULL);
    CHECK_EQ_I64(shsm_git_is_ancestor(a, gone, "a", "b"), -1);
    ShsmGitWorktreeVec w = {0};
    CHECK(!shsm_git_worktrees(a, gone, &w, &err));
}

/* ---- rev-parse and refs ---------------------------------------------------------------------------------------- */

AVP_TEST(rev_parse_family) {
    AvpArena *a = avp_arena();
    const char *r = main_repo();
    char *head = shsm_git_rev_parse(a, r, "HEAD");
    CHECK(is_sha(head));
    CHECK_CZ(head, g_c3);
    CHECK_CZ(shsm_git_rev_parse(a, r, "main"), g_c3);
    CHECK_CZ(shsm_git_rev_parse(a, r, "topic"), g_topic);
    CHECK_CZ(shsm_git_rev_parse(a, r, "HEAD~1"), g_c2);
    CHECK(shsm_git_rev_parse(a, r, "no-such-rev") == NULL);
    /* a blob is not a commit; the object form takes any object and REV:PATH */
    char *blob = shsm_git_object_id(a, r, "main", "a.txt");
    CHECK(is_sha(blob));
    CHECK(shsm_git_rev_parse(a, r, blob) == NULL);
    CHECK_CZ(shsm_git_rev_parse_object(a, r, blob), blob);
    CHECK_CZ(shsm_git_rev_parse_object(a, r, "main:a.txt"), blob);
    CHECK(shsm_git_object_id(a, r, "main", "no/such/file") == NULL);
    CHECK(shsm_git_object_id(a, r, "no-such-rev", "a.txt") == NULL);
    /* the blob id is the hash of the content */
    ShsmGitOpts o = {0};
    o.input = "main\n";
    o.input_n = 5;
    ShsmGitRun h = SHSM_GIT_OPTS(a, r, &o, "hash-object", "--stdin");
    CHECK_STR(avp_str_strip(h.out), blob);

    CHECK_CZ(shsm_git_branch(a, r), "main");
    char *sh = shsm_git_short_head(a, r);
    CHECK(sh && strlen(sh) >= 7 && strncmp(sh, g_c3, strlen(sh)) == 0);

    CHECK(shsm_git_ref_exists(a, r, "refs/heads/topic"));
    CHECK(!shsm_git_ref_exists(a, r, "refs/heads/nope"));
    CHECK(shsm_git_branch_exists(a, r, "clash"));
    CHECK(!shsm_git_branch_exists(a, r, "nope"));
    CHECK(!shsm_git_branch_exists(a, r, "c1")); /* a sha is not a branch */
}

AVP_TEST(roots_and_paths) {
    AvpArena *a = avp_arena();
    const char *r = main_repo();
    ShsmGitRoots roots = {0};
    REQUIRE(shsm_git_roots(a, r, &roots));
    CHECK_CZ(shsm_git_path_key(a, roots.top), shsm_git_path_key(a, r));
    char *want_common = avp_path_normpath(a, avp_join(a, r, ".git"));
    CHECK_CZ(shsm_git_path_key(a, roots.common), shsm_git_path_key(a, want_common));
    CHECK(avp_path_isabs(roots.common));
    CHECK(strchr(roots.common, '/') == NULL);
    CHECK_CZ(shsm_git_path_key(a, shsm_git_toplevel(a, r)), shsm_git_path_key(a, r));
    CHECK_CZ(shsm_git_path_key(a, shsm_git_common_dir(a, r)), shsm_git_path_key(a, want_common));
    /* from a sub-folder: the top-level is the same, the common dir is still correct (git prints it relative) */
    const char *sub = avp_join(a, r, "dir");
    ShsmGitRoots sr = {0};
    REQUIRE(shsm_git_roots(a, sub, &sr));
    CHECK_CZ(shsm_git_path_key(a, sr.top), shsm_git_path_key(a, r));
    CHECK_CZ(shsm_git_path_key(a, sr.common), shsm_git_path_key(a, want_common));
    CHECK_CZ(shsm_git_path_key(a, shsm_git_common_dir(a, sub)), shsm_git_path_key(a, want_common));
    /* --git-path: absolute and normalised, existing or not */
    char *hooks = shsm_git_git_path(a, r, "hooks");
    REQUIRE(hooks);
    CHECK(avp_path_isabs(hooks));
    CHECK_CZ(shsm_git_path_key(a, hooks), shsm_git_path_key(a, avp_join(a, want_common, "hooks")));
    char *mh = shsm_git_git_path(a, sub, "MERGE_HEAD");
    REQUIRE(mh);
    CHECK_CZ(shsm_git_path_key(a, mh), shsm_git_path_key(a, avp_join(a, want_common, "MERGE_HEAD")));
    CHECK(!avp_exists(mh));
    /* path_key: separators, case, trailing dot, extended prefixes */
    CHECK_CZ(shsm_git_path_key(a, "C:/Some Dir/Sub"), shsm_git_path_key(a, "c:\\some dir\\SUB\\."));
    CHECK_CZ(shsm_git_path_key(a, "\\\\?\\C:\\Some Dir\\Sub"), shsm_git_path_key(a, "C:\\Some Dir\\Sub"));
    CHECK_CZ(shsm_git_path_key(a, "\\\\?\\UNC\\host\\share\\x"), shsm_git_path_key(a, "\\\\host\\share\\x"));
}

AVP_TEST(ancestry) {
    AvpArena *a = avp_arena();
    const char *r = main_repo();
    CHECK_CZ(shsm_git_merge_base(a, r, "topic", "clash"), g_c2);
    CHECK_CZ(shsm_git_merge_base(a, r, "main", "topic"), g_c2);
    CHECK_CZ(shsm_git_merge_base(a, r, "HEAD", "main"), g_c3);
    CHECK(shsm_git_merge_base(a, r, "main", "no-such-rev") == NULL);
    CHECK_EQ_I64(shsm_git_is_ancestor(a, r, g_c1, "main"), 0);
    CHECK_EQ_I64(shsm_git_is_ancestor(a, r, "main", "main"), 0);
    CHECK_EQ_I64(shsm_git_is_ancestor(a, r, "main", g_c1), 1);
    CHECK_EQ_I64(shsm_git_is_ancestor(a, r, "topic", "clash"), 1);
    CHECK_EQ_I64(shsm_git_is_ancestor(a, r, "no-such-rev", "main"), -1);
    i64 n = -1;
    CHECK(shsm_git_rev_count(a, r, avp_sprintf(a, "%s..main", g_c2), &n));
    CHECK_EQ_I64(n, 1);
    CHECK(shsm_git_rev_count(a, r, avp_sprintf(a, "%s..topic", g_c1), &n));
    CHECK_EQ_I64(n, 2);
    CHECK(shsm_git_rev_count(a, r, "main..main", &n));
    CHECK_EQ_I64(n, 0);
    CHECK(!shsm_git_rev_count(a, r, "no-such-rev..main", &n));
}

/* ---- diff ------------------------------------------------------------------------------------------------------ */

AVP_TEST(diff_family) {
    AvpArena *a = avp_arena();
    const char *r = main_repo();
    AvpStrVec names = {0};
    AvpStr err = {0};
    /* A B */
    const char *two[] = {g_c1, g_c2};
    CHECK(shsm_git_diff_names(a, r, two, 2, &names, &err));
    CHECK_EQ_U64(names.n, 4);
    CHECK(vec_has(&names, "a.txt") && vec_has(&names, "dir/c.txt") && vec_has(&names, "new file.txt") &&
          vec_has(&names, NON_ASCII));
    /* A...B */
    const char *tri[] = {avp_sprintf(a, "%s...%s", "main", "topic")};
    CHECK(shsm_git_diff_names(a, r, tri, 1, &names, &err));
    CHECK_EQ_U64(names.n, 1);
    CHECK_STR(names.v[0], "t.txt");
    /* single REV against the work tree: clean, nothing */
    const char *one[] = {"HEAD"};
    CHECK(shsm_git_diff_names(a, r, one, 1, &names, &err));
    CHECK_EQ_U64(names.n, 0);
    /* failure: reported, or raised */
    const char *bad[] = {"no-such-rev"};
    CHECK(!shsm_git_diff_names(a, r, bad, 1, &names, &err));
    CHECK(err.n > 0);
    CHECK_RAISES(shsm_git_diff_names(a, r, bad, 1, &names, NULL), SHSM_EXC_USAGE, NULL);

    AvpStr text = {0};
    const char *stat[] = {g_c1, g_c2, "--stat"};
    CHECK(shsm_git_diff_text(a, r, stat, 3, &text, &err));
    CHECK(avp_str_contains(text, AVP_S("a.txt")) && avp_str_contains(text, AVP_S("files changed")));
    const char *patch[] = {g_c2, "main", "--", "a.txt"};
    CHECK(shsm_git_diff_text(a, r, patch, 4, &text, &err));
    CHECK(avp_str_contains(text, AVP_S("-two\n+main\n")));
    CHECK(!shsm_git_diff_text(a, r, bad, 1, &text, &err));

    CHECK(shsm_git_diff_tree_names(a, r, g_c2, &names, &err));
    CHECK(vec_has(&names, "dir/b.txt") && vec_has(&names, "dir/c.txt") && vec_has(&names, "a.txt"));
    CHECK(vec_has(&names, NON_ASCII));
    CHECK(!shsm_git_diff_tree_names(a, r, "no-such-rev", &names, &err));
    avp_vec_free(&names);
}

/* ---- objects --------------------------------------------------------------------------------------------------- */

AVP_TEST(blob_family) {
    AvpArena *a = avp_arena();
    const char *r = main_repo();
    AvpStr d = {0};
    CHECK(shsm_git_blob(a, r, "main:a.txt", &d));
    CHECK_STR(d, "main\n");
    CHECK(shsm_git_show_blob(a, r, g_c1, "a.txt", &d));
    CHECK_STR(d, "one\n");
    CHECK(shsm_git_show_blob(a, r, "main", "new file.txt", &d));
    CHECK_STR(d, "x\n");
    CHECK(shsm_git_show_blob(a, r, "main", NON_ASCII, &d));
    CHECK_STR(d, "cafe\n");
    CHECK(shsm_git_blob(a, r, shsm_git_object_id(a, r, "topic", "t.txt"), &d));
    CHECK_STR(d, "t\n");
    CHECK(!shsm_git_blob(a, r, "main:no-such-file", &d));
    CHECK(!shsm_git_show_blob(a, r, "main", "t.txt", &d)); /* only on topic */
    CHECK(!shsm_git_blob(a, r, "main:dir", &d));            /* a tree is not a blob */

    CHECK(shsm_git_blob_head(a, r, "main:a.txt", 2, &d));
    CHECK_STR(d, "ma");
    CHECK(shsm_git_blob_head(a, r, "main:a.txt", 100, &d)); /* shorter than n: the whole blob */
    CHECK_STR(d, "main\n");
    CHECK(shsm_git_blob_head(a, r, "main:a.txt", 5, &d)); /* exactly n */
    CHECK_STR(d, "main\n");
    CHECK(!shsm_git_blob_head(a, r, "main:no-such-file", 10, &d));

    CHECK(shsm_git_object_exists(a, r, "main:a.txt"));
    CHECK(shsm_git_object_exists(a, r, "main:dir"));
    CHECK(!shsm_git_object_exists(a, r, "main:t.txt"));
    CHECK(shsm_git_object_exists(a, r, g_c3));
    i64 size = -1;
    CHECK(shsm_git_object_size(a, r, "main:a.txt", &size));
    CHECK_EQ_I64(size, 5);
    CHECK(shsm_git_object_size(a, r, "main:new file.txt", &size));
    CHECK_EQ_I64(size, 2);
    CHECK(!shsm_git_object_size(a, r, "main:no-such-file", &size));
}

AVP_TEST(cat_batch) {
    AvpArena *a = avp_arena();
    const char *r = main_repo();
    AvpStr err = {0};
    ShsmGitObjectVec objs = {0};
    char *blob = shsm_git_object_id(a, r, "main", "a.txt");
    const char *names[] = {blob, "main:no such file", "main:new file.txt", "main:dir", "0123456789012345678901234567890123456789",
                           "main:a.txt"};
    CHECK(shsm_git_cat_batch(a, r, names, 6, &objs, &err));
    REQUIRE(objs.n == 6);
    CHECK(objs.v[0].found);
    CHECK_STR(objs.v[0].type, "blob");
    CHECK_STR(objs.v[0].data, "main\n");
    CHECK(!objs.v[1].found);
    CHECK_EQ_U64(objs.v[1].data.n, 0);
    CHECK(objs.v[2].found);
    CHECK_STR(objs.v[2].data, "x\n");
    CHECK(objs.v[3].found);
    CHECK_STR(objs.v[3].type, "tree");
    CHECK(!objs.v[4].found);
    CHECK(objs.v[5].found);
    CHECK_STR(objs.v[5].data, "main\n");
    CHECK(shsm_git_cat_batch(a, r, names, 0, &objs, &err));
    CHECK_EQ_U64(objs.n, 0);
    avp_vec_free(&objs);
}

AVP_TEST(big_blob_head_and_ignore) {
    AvpArena *a = avp_arena();
    const char *r = init_repo("git big repo");
    size_t big = 3u * 1024 * 1024;
    char *text = (char *)avp_alloc(a, big + 1);
    memset(text, 'x', big);
    fx_write(r, "big file.bin", text);
    fx_write(r, ".gitignore", "*.log\nbuild/\n");
    fx_write(r, "keep.txt", "k\n");
    commit_all(r, "big");
    AvpStr d = {0};
    CHECK(shsm_git_blob_head(a, r, "HEAD:big file.bin", 100, &d));
    CHECK_EQ_U64(d.n, 100);
    CHECK(memcmp(d.p, text, 100) == 0);
    i64 size = 0;
    CHECK(shsm_git_object_size(a, r, "HEAD:big file.bin", &size));
    CHECK_EQ_I64(size, (i64)big);
    CHECK(shsm_git_blob(a, r, "HEAD:big file.bin", &d));
    CHECK_EQ_U64(d.n, big);
    CHECK(shsm_git_blob_head(a, r, "HEAD:keep.txt", 0, &d) && d.n == 0);

    /* check-ignore: input order, names with spaces, exit 1 means none */
    AvpStrVec ig = {0};
    AvpStr err = {0};
    const char *paths[] = {"a.log", "keep.txt", "dir with space/c.log", "build/x.o"};
    CHECK(shsm_git_check_ignore(a, r, paths, 4, &ig, &err));
    CHECK_EQ_U64(ig.n, 3);
    CHECK_STR(ig.v[0], "a.log");
    CHECK_STR(ig.v[1], "dir with space/c.log");
    CHECK_STR(ig.v[2], "build/x.o");
    const char *none[] = {"keep.txt", "other.c"};
    CHECK(shsm_git_check_ignore(a, r, none, 2, &ig, &err));
    CHECK_EQ_U64(ig.n, 0);
    CHECK(shsm_git_check_ignore(a, r, none, 0, &ig, &err));
    CHECK(!shsm_git_check_ignore(a, avp_join(a, r, "gone"), paths, 4, &ig, &err));
    CHECK(err.n > 0);

    /* ls-files: tracked, others, cached+others with ignore rules */
    fx_write(r, "u ntracked.txt", "u\n");
    fx_write(r, "ignored.log", "i\n");
    AvpStrVec names = {0};
    CHECK(shsm_git_ls_files(a, r, NULL, 0, &names, &err));
    CHECK_EQ_U64(names.n, 3);
    CHECK(vec_has(&names, ".gitignore") && vec_has(&names, "keep.txt") && vec_has(&names, "big file.bin"));
    const char *others[] = {"--others", "--exclude-standard"};
    CHECK(shsm_git_ls_files(a, r, others, 2, &names, &err));
    CHECK_EQ_U64(names.n, 1);
    CHECK_STR(names.v[0], "u ntracked.txt");
    const char *both[] = {"--cached", "--others", "--exclude-standard"};
    CHECK(shsm_git_ls_files(a, r, both, 3, &names, &err));
    CHECK_EQ_U64(names.n, 4);
    CHECK(!vec_has(&names, "ignored.log"));
    avp_vec_free(&ig);
    avp_vec_free(&names);
}

/* ---- trees ----------------------------------------------------------------------------------------------------- */

AVP_TEST(ls_files_and_trees) {
    AvpArena *a = avp_arena();
    const char *r = main_repo();
    AvpStr err = {0};
    AvpStrVec names = {0};
    CHECK(shsm_git_ls_files(a, r, NULL, 0, &names, &err));
    CHECK_EQ_U64(names.n, 4);
    CHECK_STR(names.v[0], "a.txt");
    CHECK_STR(names.v[1], NON_ASCII);
    CHECK_STR(names.v[2], "dir/c.txt");
    CHECK_STR(names.v[3], "new file.txt");
    const char *under[] = {"--", "dir"};
    CHECK(shsm_git_ls_files(a, r, under, 2, &names, &err));
    CHECK_EQ_U64(names.n, 1);
    const char *badopt[] = {"--no-such-option"};
    CHECK(!shsm_git_ls_files(a, r, badopt, 1, &names, &err));
    CHECK(err.n > 0);

    ShsmGitStageVec st = {0};
    CHECK(shsm_git_ls_files_stage(a, r, NULL, 0, &st, &err));
    REQUIRE(st.n == 4);
    for (size_t i = 0; i < st.n; i++) {
        CHECK_EQ_U64(st.v[i].mode, 0100644);
        CHECK_EQ_I64(st.v[i].stage, 0);
        char *want = shsm_git_object_id(a, r, "HEAD", avp_cz(a, st.v[i].path));
        CHECK_MSG(want && avp_str_eqz(st.v[i].sha, want), "sha of %.*s", AVP_SF(st.v[i].path));
    }

    ShsmGitTreeVec tr = {0};
    CHECK(shsm_git_ls_tree(a, r, "main", NULL, 0, &tr, &err));
    CHECK_EQ_U64(tr.n, 4);
    const char *dirs[] = {"dir"};
    CHECK(shsm_git_ls_tree(a, r, g_c1, dirs, 1, &tr, &err));
    REQUIRE(tr.n == 1);
    CHECK_STR(tr.v[0].path, "dir/b.txt");
    CHECK_STR(tr.v[0].type, "blob");
    CHECK_EQ_U64(tr.v[0].mode, 0100644);
    CHECK_STR(tr.v[0].sha, shsm_git_object_id(a, r, g_c1, "dir/b.txt"));
    const char *two[] = {"dir", "a.txt"};
    CHECK(shsm_git_ls_tree(a, r, "main", two, 2, &tr, &err));
    CHECK_EQ_U64(tr.n, 2);
    CHECK(!shsm_git_ls_tree(a, r, "no-such-rev", NULL, 0, &tr, &err));
    CHECK(err.n > 0);
    avp_vec_free(&names);
    avp_vec_free(&st);
    avp_vec_free(&tr);
}

/* ---- worktrees ------------------------------------------------------------------------------------------------- */

AVP_TEST(parse_worktrees_synthetic) {
    AvpArena *a = avp_arena();
    AvpStr text = AVP_S(
        "worktree C:/repo\nHEAD 1111111111111111111111111111111111111111\nbranch refs/heads/main\n\n"
        "worktree C:/repo wt/bare one\nbare\n\n"
        "worktree C:/x/det\nHEAD 2222222222222222222222222222222222222222\ndetached\nlocked because of a reason\n"
        "prunable gitdir file points to non-existent location\n\n"
        "worktree C:/x/lk\nHEAD 3333333333333333333333333333333333333333\nbranch refs/heads/feat/x\nlocked\n");
    ShsmGitWorktreeVec w = {0};
    shsm_git_parse_worktrees(a, text, &w);
    REQUIRE(w.n == 4);
    CHECK_CZ(w.v[0].path, "C:/repo");
    CHECK_CZ(w.v[0].npath, "C:\\repo");
    CHECK_CZ(w.v[0].head, "1111111111111111111111111111111111111111");
    CHECK_CZ(w.v[0].branch, "refs/heads/main");
    CHECK_CZ(shsm_git_branch_short(w.v[0].branch), "main");
    CHECK(!w.v[0].bare && !w.v[0].detached && !w.v[0].locked && !w.v[0].prunable);
    CHECK_CZ(w.v[1].path, "C:/repo wt/bare one");
    CHECK(w.v[1].bare && w.v[1].branch == NULL && w.v[1].head[0] == 0);
    CHECK(w.v[2].detached && w.v[2].locked && w.v[2].prunable && w.v[2].branch == NULL);
    CHECK_CZ(w.v[2].locked_reason, "because of a reason");
    CHECK_CZ(w.v[2].prunable_reason, "gitdir file points to non-existent location");
    CHECK(w.v[3].locked && w.v[3].locked_reason == NULL);
    CHECK_CZ(shsm_git_branch_short(w.v[3].branch), "feat/x");
    CHECK(shsm_git_branch_short(NULL) == NULL);
    CHECK_CZ(shsm_git_branch_short("v1"), "v1");
    avp_vec_free(&w);
}

AVP_TEST(worktrees_real) {
    AvpArena *a = avp_arena();
    const char *r = main_repo();
    const char *wt1 = gdir("git wt one");
    const char *wt2 = gdir("git wt two");
    G(r, "worktree", "add", "-q", "-b", "wtb", wt1, "topic");
    G(r, "worktree", "add", "-q", "--detach", wt2, g_c1);
    G(r, "worktree", "lock", "--reason", "testing", wt2);

    ShsmGitWorktreeVec w = {0};
    AvpStr err = {0};
    CHECK(shsm_git_worktrees(a, r, &w, &err));
    REQUIRE(w.n == 3);
    CHECK_CZ(shsm_git_path_key(a, w.v[0].npath), shsm_git_path_key(a, r));
    CHECK_CZ(shsm_git_branch_short(w.v[0].branch), "main");
    CHECK_CZ(w.v[0].head, g_c3);
    CHECK(strchr(w.v[0].npath, '/') == NULL);
    int found = 0;
    for (size_t i = 1; i < w.n; i++) {
        char *key = shsm_git_path_key(a, w.v[i].npath);
        if (strcmp(key, shsm_git_path_key(a, wt1)) == 0) {
            CHECK_CZ(w.v[i].branch, "refs/heads/wtb");
            CHECK_CZ(w.v[i].head, g_topic);
            CHECK(!w.v[i].detached && !w.v[i].locked);
            found |= 1;
        } else if (strcmp(key, shsm_git_path_key(a, wt2)) == 0) {
            CHECK(w.v[i].branch == NULL && w.v[i].detached && w.v[i].locked);
            CHECK_CZ(w.v[i].head, g_c1);
            CHECK_CZ(w.v[i].locked_reason, "testing");
            found |= 2;
        }
    }
    CHECK_EQ_I64(found, 3);
    /* the same list from inside a linked work tree, whose own branch and top-level differ */
    ShsmGitWorktreeVec w1 = {0};
    CHECK(shsm_git_worktrees(a, wt1, &w1, &err));
    CHECK_EQ_U64(w1.n, 3);
    CHECK_CZ(shsm_git_path_key(a, w1.v[0].npath), shsm_git_path_key(a, r));
    CHECK_CZ(shsm_git_branch(a, wt1), "wtb");
    ShsmGitRoots roots = {0};
    REQUIRE(shsm_git_roots(a, wt1, &roots));
    CHECK_CZ(shsm_git_path_key(a, roots.top), shsm_git_path_key(a, wt1));
    CHECK_CZ(shsm_git_path_key(a, roots.common), shsm_git_path_key(a, avp_join(a, r, ".git")));
    CHECK(shsm_git_git_path(a, wt1, "MERGE_HEAD") != NULL);
    CHECK(!shsm_git_dirty(a, wt1));

    /* clean up the work trees (inside the temporary repository only) */
    G(r, "worktree", "unlock", wt2);
    G(r, "worktree", "remove", "--force", wt2);
    G(r, "worktree", "remove", "--force", wt1);
    G(r, "branch", "-D", "wtb");
    CHECK(shsm_git_worktrees(a, r, &w, &err));
    CHECK_EQ_U64(w.n, 1);
    CHECK(!shsm_git_branch_exists(a, r, "wtb"));
    avp_vec_free(&w);
    avp_vec_free(&w1);
}

/* ---- merge-tree ------------------------------------------------------------------------------------------------ */

AVP_TEST(merge_tree_results) {
    AvpArena *a = avp_arena();
    const char *r = main_repo();
    AvpStr err = {0};
    ShsmGitMergeTree m = {0};
    CHECK(shsm_git_merge_tree(a, r, "main", "topic", &m, &err));
    CHECK(m.clean && is_sha(m.tree) && m.conflicts.n == 0);
    ShsmGitMergeTree c = {0};
    CHECK(shsm_git_merge_tree(a, r, "main", "clash", &c, &err));
    CHECK(!c.clean && is_sha(c.tree));
    REQUIRE(c.conflicts.n == 1);
    CHECK_STR(c.conflicts.v[0], "a.txt");
    /* the merged tree holds both sides' non-conflicting changes */
    CHECK(shsm_git_object_exists(a, r, avp_sprintf(a, "%s:t.txt", m.tree)));
    CHECK(shsm_git_object_exists(a, r, avp_sprintf(a, "%s:new file.txt", m.tree)));
    ShsmGitMergeTree bad = {0};
    CHECK(!shsm_git_merge_tree(a, r, "main", "no-such-rev", &bad, &err));
    CHECK(err.n > 0);
    CHECK_RAISES(shsm_git_merge_tree(a, r, "main", "no-such-rev", &bad, NULL), SHSM_EXC_USAGE, NULL);
    avp_vec_free(&m.conflicts);
    avp_vec_free(&c.conflicts);
}

/* ---- log ------------------------------------------------------------------------------------------------------- */

AVP_TEST(log_family) {
    AvpArena *a = avp_arena();
    const char *r = main_repo();
    AvpStr err = {0};
    CHECK_CZ(shsm_git_last_commit_for_path(a, r, "a.txt"), g_c3);
    CHECK_CZ(shsm_git_last_commit_for_path(a, r, "dir/c.txt"), g_c2);
    CHECK_CZ(shsm_git_last_commit_for_path(a, r, "dir/b.txt"), g_c2); /* its deletion */
    CHECK(shsm_git_last_commit_for_path(a, r, "no/such/path") == NULL);

    AvpStrVec shas = {0};
    CHECK(shsm_git_log_path_shas(a, r, "a.txt", false, &shas, &err));
    REQUIRE(shas.n == 3);
    CHECK_STR(shas.v[0], g_c1);
    CHECK_STR(shas.v[1], g_c2);
    CHECK_STR(shas.v[2], g_c3);
    CHECK(shsm_git_log_path_shas(a, r, "a.txt", true, &shas, &err));
    CHECK_EQ_U64(shas.n, 3);
    CHECK(shsm_git_log_path_shas(a, r, "no/such/path", false, &shas, &err));
    CHECK_EQ_U64(shas.n, 0);

    CHECK_CZ(shsm_git_subject(a, r, g_c3), "third on main");
    CHECK_CZ(shsm_git_subject(a, r, "topic"), "topic work");
    CHECK(shsm_git_subject(a, r, "no-such-rev") == NULL);

    i64 t = 0;
    char *subj = NULL;
    CHECK(shsm_git_commit_info(a, r, "clash", &t, &subj));
    CHECK(t > 1500000000);
    CHECK_CZ(subj, "clash work");
    CHECK(!shsm_git_commit_info(a, r, "no-such-rev", &t, &subj));

    AvpStrVec br = {0};
    CHECK(shsm_git_branches_no_merged(a, r, "main", &br, &err));
    CHECK_EQ_U64(br.n, 2);
    CHECK(vec_has(&br, "topic") && vec_has(&br, "clash"));
    CHECK(shsm_git_branches_no_merged(a, r, "topic", &br, &err));
    CHECK(vec_has(&br, "main") && vec_has(&br, "clash") && !vec_has(&br, "topic"));
    CHECK(!shsm_git_branches_no_merged(a, r, "no-such-rev", &br, &err));
    CHECK(err.n > 0);
    avp_vec_free(&shas);
    avp_vec_free(&br);
}

AVP_TEST_MAIN("shsm_git")
