/* pre_commit: the port of tools/test_hooks.py. The decision function on made-up staged-file lists; one test stages
 * files in a throwaway git repository under build/tmp/pre_commit/ and runs the built hook there. Plus the ls-files
 * batching (D-pre_commit-1) and the C-first shim tools/hooks/pre-commit-c, installed in a throwaway repository.
 *
 * Map of tools/test_hooks.py (5 tests) to the C tests:
 *   TestDecide.test_allowed                  -> allowed
 *   TestDecide.test_refused_paths            -> refused_paths
 *   TestDecide.test_refused_types_and_blobs  -> refused_types_and_blobs
 *   TestDecide.test_text_over_limit_allowed  -> text_over_limit_allowed
 *   TestHookInRepo.test_index                -> index (the built pre_commit.exe in the repository, not main() in-process)
 * C only: batches (D-pre_commit-1), c_shim (tools/hooks/pre-commit-c with and without build/tools/pre_commit.exe). */
#include "avp/test.h"
#include "pre_commit.h"
#include "shsm_git.h"

#include <string.h>

#define A avp_arena()
#define MB PRE_COMMIT_MB
#define NARR(x) (sizeof(x) / sizeof((x)[0]))

static bool refused(const PreCommitRefusals *r, const char *path) {
    for (size_t i = 0; i < r->n; i++)
        if (!strcmp(r->v[i].path, path)) return true;
    return false;
}

AVP_TEST(allowed) {
    static const PreCommitEntry e[] = {
        {"src/game/PadRumble.cpp", 4000, false}, {"orig/INVENTORY.md", 100, false}, {"orig/R5WEA4/.gitkeep", 0, false},
        {"ref/ghostrider_ps2/classes.txt", 2 * MB, false}, {"docs/progress.md", 50000, false},
        {"tools/bin_notes.py", 10, false}, {"config/R5WEA4/small.bin", 1000, true}};
    CHECK_EQ_I64((i64)pre_commit_decide(A, e, NARR(e)).n, 0);
}

AVP_TEST(refused_paths) {
    static const PreCommitEntry e[] = {
        {"orig/R5WEA4/sys/main.dol", 4 * MB, true}, {"orig/notes.txt", 10, false}, {"build/R5WEA4/report.json", 10, false},
        {"tools/compilers/Wii/1.0/mwcceppc.exe", 10, true}, {"tools/bin/dtk.exe", 10, true},
        {"tools/src/decomp-toolkit/README.md", 10, false}, {"Build/x.txt", 1, false}};
    PreCommitRefusals r = pre_commit_decide(A, e, NARR(e));
    CHECK_EQ_I64((i64)r.n, (i64)NARR(e));
    for (size_t i = 0; i < NARR(e); i++) CHECK_MSG(refused(&r, e[i].path), "%s", e[i].path);
}

AVP_TEST(refused_types_and_blobs) {
    static const PreCommitEntry e[] = {
        {"a/x.DOL", 10, true}, {"x.rel", 10, true}, {"y.elf", 10, true}, {"src/z.o", 10, true}, {"disc.rvz", 10, true},
        {"disc.iso", 10, true}, {"data/big.pak", 2 * MB, true}, {"ref/ghostrider_ps2/tiny.bin", 10, true}};
    PreCommitRefusals r = pre_commit_decide(A, e, NARR(e));
    CHECK_EQ_I64((i64)r.n, (i64)NARR(e));
    for (size_t i = 0; i < NARR(e); i++) CHECK_MSG(refused(&r, e[i].path), "%s", e[i].path);
    CHECK_CZ(r.v[6].reason, "binary blob of 2.0 MB");
    CHECK_CZ(r.v[0].reason, "binary image/object type");
}

AVP_TEST(text_over_limit_allowed) {
    static const PreCommitEntry e[] = {{"docs/huge.md", 3 * MB, false}};
    CHECK_EQ_I64((i64)pre_commit_decide(A, e, 1).n, 0);
}

AVP_TEST(batches) {
    AvpStr names[100];
    for (int i = 0; i < 100; i++) names[i] = AVP_S("src/a_name_of_twenty.c"); /* 22 + 3 per name */
    PreCommitBatches one = pre_commit_batches(names, 100, 30000);
    REQUIRE(one.n == 1);
    CHECK(one.v[0].start == 0 && one.v[0].end == 100);
    PreCommitBatches many = pre_commit_batches(names, 100, 48 + 25 * 10);
    REQUIRE(many.n == 10);
    for (size_t i = 0; i < many.n; i++) CHECK(many.v[i].start == i * 10 && many.v[i].end == i * 10 + 10);
    CHECK_EQ_I64((i64)pre_commit_batches(names, 0, 30000).n, 1);
}

/* ---- TestHookInRepo -------------------------------------------------------------------------------------------- */
static const char *repo_dir(void) {
    const char *d = avp_path_abspath(A, avp_join(A, "build", "tmp", "pre_commit", "hook test"));
    if (avp_isdir(d)) AVP_ASSERT(avp_rmtree(d, AVP_RMTREE_CLEAR_READONLY));
    AVP_ASSERT(avp_makedirs(d, true));
    SHSM_GIT_CHECK(A, d, "init", "-q", "-b", "main");
    return d;
}

static void stage(const char *repo, const char *rel, const void *data, size_t n) {
    char *p = avp_path_join(A, repo, rel);
    AVP_ASSERT(avp_makedirs(avp_cz(A, avp_path_dirname(p)), true));
    AVP_ASSERT(avp_write_file(p, data, n));
    SHSM_GIT_CHECK(A, repo, "add", "-f", rel);
}

static i64 run_hook(const char *repo, AvpStr *out) {
    const char *exe = avp_path_join(A, avp_cz(A, avp_path_dirname(avp_exe_dir())), "pre_commit.exe");
    AvpProcResult r = avp_run(&(AvpProcSpec){.argv = (const char *const[]){exe, "--no-rules", NULL}, .cwd = repo,
                                             .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE, .text = true, .timeout = 120,
                                             .arena = A});
    AVP_ASSERT(r.status == AVP_PROC_OK);
    *out = r.out;
    return r.returncode;
}

AVP_TEST(index) {
    const char *r = repo_dir();
    AvpStr out;
    stage(r, "src/a.c", "int a;\n", 7);
    CHECK_EQ_I64(run_hook(r, &out), 0);
    stage(r, "ref/blob.dat", "\0\1\2", 3);
    stage(r, "build/x.json", "{}", 2);
    CHECK_EQ_I64(run_hook(r, &out), 1);
    CHECK(avp_str_contains(out, AVP_S("ref/blob.dat")));
    CHECK(avp_str_contains(out, AVP_S("build/x.json")));
    CHECK(!avp_str_contains(out, AVP_S("src/a.c")));
}

/* tools/hooks/pre-commit-c as the repository's pre-commit hook: with build/tools/pre_commit.exe present it runs the exe
 * (the only thing that can refuse); without it the shim skips with a notice. */
AVP_TEST(c_shim) {
    const char *r = repo_dir();
    static const char *const cfg[][2] = {{"user.name", "Test"}, {"user.email", "t@example.invalid"},
                                         {"commit.gpgsign", "false"}, {"core.autocrlf", "false"}};
    for (size_t i = 0; i < NARR(cfg); i++) SHSM_GIT_CHECK(A, r, "config", cfg[i][0], cfg[i][1]);
    const char *hook = avp_path_join(A, r, ".git/hooks/pre-commit");
    AVP_ASSERT(avp_makedirs(avp_cz(A, avp_path_dirname(hook)), true));
    REQUIRE(avp_copyfile("tools/hooks/pre-commit-c", hook));
    const char *exe = avp_path_join(A, r, "build/tools/pre_commit.exe");
    AVP_ASSERT(avp_makedirs(avp_cz(A, avp_path_dirname(exe)), true));
    REQUIRE(avp_copyfile(avp_path_join(A, avp_cz(A, avp_path_dirname(avp_exe_dir())), "pre_commit.exe"), exe));

    stage(r, "build/x.txt", "x\n", 2);
    ShsmGitRun c = SHSM_GIT(A, r, "commit", "-q", "-m", "refused");
    CHECK_MSG(c.started && c.rc == 1, "rc %lld", (long long)c.rc);
    AvpStr said = avp_str_concat(A, c.out, c.err); /* git sends a hook's stdout to its stderr */
    CHECK_MSG(avp_str_contains(said, AVP_S("build/x.txt: build/ is never committed")), "%.*s", AVP_SF(said));
    SHSM_GIT_CHECK(A, r, "rm", "-q", "--cached", "build/x.txt");
    stage(r, "a.txt", "a\n", 2);
    c = SHSM_GIT(A, r, "commit", "-q", "-m", "allowed");
    CHECK_MSG(shsm_git_ok(&c), "%.*s", AVP_SF(c.err));

    AVP_ASSERT(avp_remove(exe)); /* no exe: the shim skips with a notice */
    stage(r, "build/x.txt", "x\n", 2);
    c = SHSM_GIT(A, r, "commit", "-q", "-m", "skipped");
    CHECK_MSG(shsm_git_ok(&c), "%.*s", AVP_SF(c.err));
    CHECK_MSG(avp_str_contains(c.err, AVP_S("build/tools/pre_commit.exe not built (run tools/build_c.cmd); skipped")),
              "%.*s", AVP_SF(c.err));
}

AVP_TEST_MAIN("pre_commit")
