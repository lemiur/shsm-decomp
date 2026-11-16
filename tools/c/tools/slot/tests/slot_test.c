/* slot: tools/test_slot.py ported to C, plus the lock helpers.
 *
 * Every test builds a throwaway git repository (gitignored tool folders, real junctions) under
 * build/tmp/cpilot/B/slot <name>/ and runs build/tools/slot.exe on it with --repo/--base; the real repository and
 * slots are never touched. Links are removed link-only before a fixture folder is deleted.
 *
 * Mapping (tools/test_slot.py -> here):
 *   TestNewCheckRemove.test_new_check_list               -> new_check_list
 *   TestNewCheckRemove.test_check_flags_copy_and_broken  -> check_flags_copy
 *   TestNewCheckRemove.test_remove_keeps_targets         -> remove_keeps_targets
 *   TestNewCheckRemove.test_remove_refuses_dirty_and_unmerged -> remove_refuses_dirty_and_unmerged
 *   TestNewCheckRemove.test_remove_link_only             -> remove_link_only
 *   TestNewCheckRemove.test_reset                        -> reset
 *   TestLocks.test_contention                            -> locks_contention
 *   TestLocks.test_stale                                 -> locks_stale
 *   TestConflicts.test_pair                              -> conflicts_pair
 * New: lock_interop_python (a lock written by either tool is honoured by the other), lock_text_values,
 * scan_links_sorted. */
#include "avp/proc.h"
#include "../../../tests/shsm_test_util.h" /* fx_write (tests/ is not on a tool test's include path) */
#include "slot.h"

#include <string.h>

typedef struct {
    const char *tmp, *main, *base;
} Fx;

static AvpArena *A(void) { return avp_arena(); }

static void unlink_all(const char *dir) {
    if (!avp_isdir(dir)) return;
    AvpStrVec links = slot_scan_links(A(), dir);
    for (size_t i = 0; i < links.n; i++) AVP_ASSERT(shsm_link_remove(links.v[i].p));
}

static void wipe(const char *dir) {
    unlink_all(dir);
    if (avp_isdir(dir)) AVP_ASSERT(avp_rmtree(dir, AVP_RMTREE_CLEAR_READONLY));
}

static void sh(const char *cwd, const char *const *args, size_t n) { shsm_git_check(A(), cwd, args, n, NULL); }
#define SH(cwd, ...) sh((cwd), SHSM_GIT_ARGS_(__VA_ARGS__))

static const char *J(const char *x, const char *y) { return avp_path_join(A(), x, y); }

static Fx setup(const char *name) {
    AvpArena *a = A();
    avp_setenv("GIT_AUTHOR_NAME", "t");
    avp_setenv("GIT_AUTHOR_EMAIL", "t@example.invalid");
    avp_setenv("GIT_COMMITTER_NAME", "t");
    avp_setenv("GIT_COMMITTER_EMAIL", "t@example.invalid");
    Fx f;
    f.tmp = avp_path_abspath(a, avp_join(a, "build", "tmp", "cpilot", "B", avp_sprintf(a, "slot %s", name)));
    wipe(f.tmp);
    f.main = J(f.tmp, "main");
    f.base = J(f.tmp, "wt");
    AVP_ASSERT(avp_makedirs(f.main, true));
    SH(f.main, "init", "-q", "-b", "main");
    SH(f.main, "config", "commit.gpgsign", "false");
    SH(f.main, "config", "core.autocrlf", "false");
    fx_write(f.main, ".gitignore",
             "tools/bin/\ntools/compilers/\ntools/src/\ntools/venv/\ntools/permuter/\nbuild/\n"
             "orig/R5WEA4/*\n!orig/R5WEA4/.gitkeep\norig/_iso/\norig/_ref/\n");
    fx_write(f.main, "orig/R5WEA4/.gitkeep", "");
    fx_write(f.main, "src/a.c", "int a;\n");
    SH(f.main, "add", "-A");
    SH(f.main, "commit", "-q", "-m", "init");
    for (size_t i = 0; i < SLOT_NREQUIRED; i++) fx_write(f.main, SLOT_REQUIRED_FILES[i], "payload");
    fx_write(f.main, "tools/compilers/Wii/1.0/lib/deep/nested.txt", "deep");
    return f;
}

static void teardown(const Fx *f) { wipe(f->tmp); }

typedef struct {
    i64 rc;
    const char *out; /* stdout + stderr, as the Python test captured both */
} Res;

static const char *g_cwd; /* working directory of the next run; NULL keeps the test's */

static Res run_argv(const char *const *head, size_t nhead, const Fx *f, const char *const *args, size_t n) {
    AvpArena *a = A();
    const char **argv = AVP_NEWN(a, const char *, nhead + 4 + n);
    size_t k = 0;
    for (size_t i = 0; i < nhead; i++) argv[k++] = head[i];
    argv[k++] = "--repo";
    argv[k++] = f->main;
    argv[k++] = "--base";
    argv[k++] = f->base;
    for (size_t i = 0; i < n; i++) argv[k++] = args[i];
    ShsmRun r = shsm_run(a, argv, k, g_cwd, 300);
    Res res = {r.returncode, avp_cz(a, shsm_run_output(a, &r))};
    return res;
}

static Res run_c(const Fx *f, const char *const *args, size_t n) {
    const char *exe = avp_path_abspath(A(), avp_join(A(), avp_exe_dir(), "..", "slot.exe"));
    return run_argv(&exe, 1, f, args, n);
}
#define SLOT(f, ...) run_c((f), SHSM_GIT_ARGS_(__VA_ARGS__))

static bool has(const char *text, const char *needle) { return strstr(text, needle) != NULL; }

/* path\0content lines of every file under main/tools plus the retail DOL (target_files + read). */
static const char *snapshot(const Fx *f) {
    AvpArena *a = A();
    AvpStrVec files = {0};
    AvpWalk *w = avp_walk_open(J(f->main, "tools"), true);
    AvpWalkItem it = {0};
    while (avp_walk_next(w, &it))
        for (size_t i = 0; i < it.files.n; i++)
            avp_vec_push(&files, avp_str(avp_path_join(a, avp_cz(a, it.dirpath), avp_cz(a, it.files.v[i]))));
    avp_walk_close(w);
    avp_vec_push(&files, avp_str(J(f->main, "orig/R5WEA4/sys/main.dol")));
    avp_sort_strs(files.v, files.n);
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < files.n; i++) {
        AvpStr text;
        AVP_ASSERT(avp_read_text(a, files.v[i].p, AVP_TM_UTF8, &text));
        avp_buf_printf(&b, "%s=%.*s\n", files.v[i].p, AVP_SF(text));
    }
    return avp_buf_cz(&b);
}

static const char *read_text(const char *p) {
    AvpStr t;
    AVP_ASSERT(avp_read_text(A(), p, AVP_TM_UTF8, &t));
    return t.p;
}

/* ---- TestNewCheckRemove ---------------------------------------------------------------------------------------- */

AVP_TEST(new_check_list) {
    Fx f = setup("new check list");
    Res r = SLOT(&f, "new", "1");
    CHECK_MSG(r.rc == 0, "%s", r.out);
    const char *p = J(f.base, "slot1");
    CHECK(slot_is_link(J(p, "tools/bin")));
    CHECK(avp_isfile(J(p, "tools/compilers/Wii/1.0/lib/deep/nested.txt")));
    r = SLOT(&f, "check", "1");
    CHECK_MSG(r.rc == 0, "%s", r.out);
    CHECK(has(r.out, "slot1: OK"));
    r = SLOT(&f, "list");
    CHECK(has(r.out, "bin=ok"));
    CHECK(has(r.out, "slot1"));
    teardown(&f);
}

/* `slotN` means <base>/slotN as `N` does (not a path under the current directory), and a path inside the checkout is
 * refused whether given absolute or relative to a current directory inside it. */
AVP_TEST(new_slot_name_and_refuse_inside_checkout) {
    Fx f = setup("new inside");
    g_cwd = f.main; /* where `slot4` used to resolve to a nested worktree */
    Res r = SLOT(&f, "new", "slot4");
    g_cwd = NULL;
    CHECK_MSG(r.rc == 0, "%s", r.out);
    CHECK(avp_isdir(J(f.base, "slot4")) && !avp_lexists(J(f.main, "slot4")));
    CHECK(slot_is_link(J(J(f.base, "slot4"), "tools/bin")));
    const char *inside[] = {J(f.main, "slot9"), J(f.main, "sub/dir/s"), J(f.main, "tools/bin/x"), f.main};
    for (size_t i = 0; i < 4; i++) {
        r = SLOT(&f, "new", inside[i]);
        CHECK_MSG(r.rc != 0 && has(r.out, "REFUSED: a slot may not sit inside the checkout"), "%s: %s", inside[i], r.out);
    }
    g_cwd = f.main;
    r = SLOT(&f, "new", "nested/s");
    g_cwd = NULL;
    CHECK_MSG(r.rc != 0 && has(r.out, "REFUSED: a slot may not sit inside the checkout"), "%s", r.out);
    CHECK(!avp_lexists(J(f.main, "nested")) && !avp_lexists(J(f.main, "slot9")) && !avp_lexists(J(f.main, "sub")));
    ShsmGitRun st = SHSM_GIT_CHECK(A(), f.main, "worktree", "list", "--porcelain");
    const char *wl = avp_cz(A(), st.out);
    CHECK(!has(wl, "slot9") && !has(wl, "nested"));
    /* a path outside the checkout, spelled as a path, is still accepted */
    r = SLOT(&f, "new", J(f.tmp, "elsewhere"));
    CHECK_MSG(r.rc == 0, "%s", r.out);
    teardown(&f);
}

AVP_TEST(shared_inputs_orig_iso_orig_ref_and_binutils) {
    Fx f = setup("shared extras");
    fx_write(f.main, "orig/_iso/ps2.elf", "elf");
    fx_write(f.main, "orig/_ref/gormiti_wii/a.elf", "elf");
    fx_write(f.main, "build/binutils/powerpc-eabi-objdump.exe", "objdump");
    Res r = SLOT(&f, "new", "1");
    CHECK_MSG(r.rc == 0, "%s", r.out);
    const char *p = J(f.base, "slot1");
    CHECK(slot_is_link(J(p, "orig/_iso")) && slot_is_link(J(p, "orig/_ref")) && slot_is_link(J(p, "build/binutils")));
    CHECK_CZ(read_text(J(p, "orig/_ref/gormiti_wii/a.elf")), "elf");
    CHECK_CZ(read_text(J(p, "build/binutils/powerpc-eabi-objdump.exe")), "objdump");
    r = SLOT(&f, "check", "1");
    CHECK_MSG(r.rc == 0 && has(r.out, "slot1: OK"), "%s", r.out);
    r = SLOT(&f, "list");
    CHECK_MSG(has(r.out, "iso=ok") && has(r.out, "ref=ok") && has(r.out, "binutils=ok"), "%s", r.out);
    /* a slot made before the links existed gets them from `link`; remove takes them down without touching the targets */
    slot_remove_link(J(p, "orig/_iso"));
    r = SLOT(&f, "link", "1");
    CHECK_MSG(r.rc == 0 && has(r.out, "orig/_iso"), "%s", r.out);
    CHECK(slot_is_link(J(p, "orig/_iso")));
    /* check --fix does the same for an old slot, as part of the check, and leaves what is there alone */
    slot_remove_link(J(p, "orig/_iso"));
    slot_remove_link(J(p, "orig/_ref"));
    r = SLOT(&f, "check", "1");
    CHECK_MSG(!has(r.out, "links created"), "%s", r.out);
    CHECK(!avp_exists(J(p, "orig/_iso")));
    r = SLOT(&f, "check", "1", "--fix");
    CHECK_MSG(r.rc == 0 && has(r.out, "links created: orig/_iso, orig/_ref") && has(r.out, "slot1: OK"), "%s", r.out);
    CHECK(slot_is_link(J(p, "orig/_iso")) && slot_is_link(J(p, "orig/_ref")));
    CHECK_CZ(read_text(J(p, "orig/_iso/ps2.elf")), "elf");
    r = SLOT(&f, "check", "1", "--fix");
    CHECK_MSG(r.rc == 0 && !has(r.out, "links created"), "%s", r.out);
    r = SLOT(&f, "remove", "1");
    CHECK_MSG(r.rc == 0, "%s", r.out);
    CHECK(!avp_exists(p));
    CHECK_CZ(read_text(J(f.main, "orig/_iso/ps2.elf")), "elf");
    CHECK_CZ(read_text(J(f.main, "orig/_ref/gormiti_wii/a.elf")), "elf");
    CHECK_CZ(read_text(J(f.main, "build/binutils/powerpc-eabi-objdump.exe")), "objdump");
    teardown(&f);
}

AVP_TEST(check_flags_copy) {
    Fx f = setup("check copy");
    SLOT(&f, "new", "2");
    const char *p = J(f.base, "slot2");
    slot_remove_link(J(p, "tools/bin"));
    CHECK(avp_copytree(J(f.main, "tools/bin"), J(p, "tools/bin"), NULL));
    Res r = SLOT(&f, "check", "2");
    CHECK_EQ_I64(r.rc, 1);
    CHECK_MSG(has(r.out, "tools/bin copy"), "%s", r.out);
    teardown(&f);
}

AVP_TEST(remove_keeps_targets) {
    Fx f = setup("remove keeps");
    const char *before = snapshot(&f);
    ShsmGitRun wl0 = SHSM_GIT_CHECK(A(), f.main, "worktree", "list");
    SLOT(&f, "new", "1");
    const char *p = J(f.base, "slot1");
    const char *extra = J(f.tmp, "outside");
    fx_write(extra, "a/b/c.txt", "keep");
    CHECK(shsm_link_create_junction(J(p, "build/deep/link"), extra));
    fx_write(p, "build/R5WEA4/x.o", "o");
    Res r = SLOT(&f, "remove", "1");
    CHECK_MSG(r.rc == 0, "%s", r.out);
    CHECK(!avp_exists(p));
    CHECK_CZ(snapshot(&f), before);
    CHECK_CZ(read_text(J(extra, "a/b/c.txt")), "keep");
    ShsmGitRun wl = SHSM_GIT_CHECK(A(), f.main, "worktree", "list");
    CHECK_MSG(wl.out.n == wl0.out.n && memcmp(wl.out.p, wl0.out.p, wl.out.n) == 0, "worktree list changed: %s vs %s", wl.out.p, wl0.out.p);
    teardown(&f);
}

AVP_TEST(remove_refuses_dirty_and_unmerged) {
    Fx f = setup("remove refuses");
    SLOT(&f, "new", "1", "--branch", "lane/x");
    const char *p = J(f.base, "slot1");
    fx_write(p, "src/a.c", "int b;\n");
    Res r = SLOT(&f, "remove", "1");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(has(r.out, "dirty"));
    CHECK(slot_is_link(J(p, "tools/bin"))); /* nothing unlinked on refusal */
    SH(p, "commit", "-q", "-am", "lane work");
    r = SLOT(&f, "remove", "1");
    CHECK_EQ_I64(r.rc, 1);
    CHECK(has(r.out, "not in main"));
    r = SLOT(&f, "remove", "1", "--force");
    CHECK_MSG(r.rc == 0, "%s", r.out);
    CHECK(avp_isfile(J(f.main, "tools/bin/dtk.exe")));
    teardown(&f);
}

AVP_TEST(remove_link_only) {
    Fx f = setup("remove link only");
    const char *tgt = J(f.tmp, "t");
    fx_write(tgt, "n/f.txt", "1");
    const char *link = J(f.tmp, "l");
    CHECK(shsm_link_create_junction(link, tgt));
    slot_remove_link(link);
    CHECK(!avp_lexists(link));
    CHECK(avp_isfile(J(tgt, "n/f.txt")));
    AvpTry t;
    if (AVP_TRY(&t)) {
        slot_remove_link(tgt); /* a real directory is never removed */
        avp_try_end(&t);
        CHECK_MSG(false, "no refusal");
    } else {
        CHECK_EQ_I64(t.code, SLOT_REFUSAL);
        CHECK(avp_isfile(J(tgt, "n/f.txt")));
    }
    teardown(&f);
}

AVP_TEST(reset) {
    Fx f = setup("reset");
    SLOT(&f, "new", "1", "--branch", "lane/r");
    const char *p = J(f.base, "slot1");
    fx_write(p, "src/b.c", "int c;\n");
    SH(p, "add", "-A");
    SH(p, "commit", "-q", "-m", "w");
    Res r = SLOT(&f, "reset", "1");
    CHECK_EQ_I64(r.rc, 1);
    SH(f.main, "merge", "-q", "--ff-only", "lane/r");
    r = SLOT(&f, "reset", "1");
    CHECK_MSG(r.rc == 0, "%s", r.out);
    CHECK_CZ(shsm_git_rev_parse(A(), p, "HEAD"), shsm_git_rev_parse(A(), f.main, "main"));
    CHECK_CZ(shsm_git_branch(A(), p), "HEAD");
    teardown(&f);
}

/* ---- TestLocks ------------------------------------------------------------------------------------------------- */

AVP_TEST(locks_contention) {
    Fx f = setup("locks contention");
    SLOT(&f, "new", "1");
    CHECK_EQ_I64(SLOT(&f, "lock", "1", "--owner", "a", "--lane", "lane/a").rc, 0);
    Res r = SLOT(&f, "lock", "1", "--owner", "b");
    CHECK_EQ_I64(r.rc, 1);
    CHECK_MSG(has(r.out, "held by a/lane/a"), "%s", r.out);
    CHECK_EQ_I64(SLOT(&f, "lock", "1", "--owner", "a").rc, 0); /* refresh */
    r = SLOT(&f, "who");
    CHECK_MSG(has(r.out, "slot1: a"), "%s", r.out);
    CHECK_EQ_I64(SLOT(&f, "unlock", "1", "--owner", "b").rc, 1);
    CHECK_EQ_I64(SLOT(&f, "remove", "1").rc, 1); /* locked */
    CHECK_EQ_I64(SLOT(&f, "unlock", "1").rc, 0);
    CHECK_EQ_I64(SLOT(&f, "lock", "1", "--owner", "b").rc, 0);
    teardown(&f);
}

static const char *lock_file(const Fx *f) {
    AvpArena *a = A();
    SlotRepo *repo = slot_repo_open(a, f->main, f->base);
    return slot_lock_path(a, repo, J(f->base, "slot1"));
}

AVP_TEST(locks_stale) {
    Fx f = setup("locks stale");
    SLOT(&f, "new", "1");
    SLOT(&f, "lock", "1", "--owner", "a", "--ttl", "1");
    AvpArena *a = A();
    const char *lp = lock_file(&f);
    AvpJson *d = avp_json_parse(a, avp_str(read_text(lp)), NULL);
    REQUIRE(d);
    avp_json_set(a, d, AVP_S("epoch"), avp_json_float(a, avp_time() - 2 * 3600));
    AvpStr text = avp_json_dumps(a, d, NULL);
    CHECK(avp_write_text(lp, text, AVP_TM_UTF8));
    Res r = SLOT(&f, "who");
    CHECK_MSG(has(r.out, "STALE"), "%s", r.out);
    CHECK_EQ_I64(SLOT(&f, "lock", "1", "--owner", "b").rc, 1);
    CHECK_EQ_I64(SLOT(&f, "lock", "1", "--owner", "b", "--force").rc, 0);
    teardown(&f);
}

/* ---- TestConflicts --------------------------------------------------------------------------------------------- */

AVP_TEST(conflicts_pair) {
    Fx f = setup("conflicts");
    static const char *const names[] = {"lane/a", "lane/b", "lane/c"};
    static const char *const texts[] = {"int a = 1;\n", "int a = 2;\n", NULL};
    for (size_t i = 0; i < 3; i++) {
        SH(f.main, "checkout", "-q", "-b", names[i], "main");
        if (texts[i]) fx_write(f.main, "src/a.c", texts[i]);
        else fx_write(f.main, "src/c.c", "int c;\n");
        SH(f.main, "add", "-A");
        SH(f.main, "commit", "-q", "-m", names[i]);
        SH(f.main, "checkout", "-q", "main");
    }
    Res r = SLOT(&f, "conflicts");
    CHECK_MSG(r.rc == 1, "%s", r.out);
    CHECK(has(r.out, "lane/a: clean onto main"));
    CHECK(has(r.out, "lane/a x lane/b: 1 shared, CONFLICT: src/a.c"));
    CHECK(!has(r.out, "lane/a x lane/c"));
    r = SLOT(&f, "conflicts", "lane/a", "lane/c");
    CHECK_MSG(r.rc == 0, "%s", r.out);
    ShsmGitRun st = SHSM_GIT_CHECK(A(), f.main, "status", "--porcelain");
    CHECK_EQ_I64((i64)st.out.n, 0);
    teardown(&f);
}

/* ---- pure helpers ---------------------------------------------------------------------------------------------- */

static AvpJson *J1(const char *text) {
    AvpJson *d = avp_json_parse(A(), avp_str(text), NULL);
    AVP_ASSERT(d);
    return d;
}

AVP_TEST(lock_text_values) {
    AvpArena *a = A();
    double now = 1000.0 * 3600.0;
    CHECK_CZ(slot_lock_text(a, NULL, now), "-");
    CHECK_CZ(slot_lock_text(a, J1("{\"owner\": \"a\", \"lane\": \"l\", \"epoch\": 3600000.0, \"ttl_hours\": 12.0}"), now),
             "a/l 0.0h");
    CHECK_CZ(slot_lock_text(a, J1("{\"owner\": \"a\", \"lane\": null, \"epoch\": 3592800, \"ttl_hours\": 1.0}"), now),
             "a 2.0h STALE");
    CHECK_CZ(slot_lock_text(a, J1("{\"owner\": \"?\", \"corrupt\": true, \"epoch\": 0, \"ttl_hours\": 0}"), now),
             "? 1000.0h STALE");
    CHECK_CZ(slot_lock_text(a, J1("{\"epoch\": \"3594600\"}"), now), "None 1.5h"); /* float("..."), ttl 12 */
    CHECK(slot_lock_owner_is(J1("{}"), NULL));
    CHECK(slot_lock_owner_is(J1("{\"owner\": null}"), NULL));
    CHECK(!slot_lock_owner_is(J1("{\"owner\": \"a\"}"), NULL));
    CHECK(slot_lock_owner_is(J1("{\"owner\": \"a\"}"), "a"));
    CHECK(!slot_lock_owner_is(J1("{\"owner\": 1}"), "1"));
    CHECK_CZ(slot_py_str(a, J1("1.0")), "1.0");
    CHECK_CZ(slot_py_str(a, J1("true")), "True");
    CHECK_CZ(slot_py_str(a, J1("[1, \"x\", null]")), "[1, 'x', None]");
    CHECK_CZ(slot_py_str(a, NULL), "None");
}

AVP_TEST(scan_links_sorted) {
    Fx f = setup("scan sorted");
    const char *root = J(f.tmp, "root");
    const char *tgt = J(f.tmp, "tgt");
    fx_write(tgt, "x.txt", "x");
    CHECK(shsm_link_create_junction(J(root, "a/z"), tgt));
    CHECK(shsm_link_create_junction(J(root, "a-c"), tgt));
    CHECK(shsm_link_create_junction(J(root, "a/b/j"), tgt));
    AvpStrVec v = slot_scan_links(A(), root);
    REQUIRE(v.n == 3);
    /* sorted() over whole paths: "-" (0x2D) sorts before "\" (0x5C) */
    CHECK_CZ(v.v[0].p, J(root, "a-c"));
    CHECK_CZ(v.v[1].p, J(root, "a\\b\\j"));
    CHECK_CZ(v.v[2].p, J(root, "a\\z"));
    teardown(&f);
}

AVP_TEST_MAIN("slot")
