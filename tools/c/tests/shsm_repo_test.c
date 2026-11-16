/* shsm_repo: shsm_repo_main_root and shsm_repo_shared_path (reference inputs that live only in the main checkout)
 * against a throwaway repository with a linked work tree. */
#include "avp/proc.h"
#include "shsm_git.h"
#include "shsm_repo.h"
#include "shsm_test_util.h"

#define G(repo, ...) SHSM_GIT_CHECK(avp_arena(), (repo), __VA_ARGS__)

static const char *g_main, *g_wt;

static void fixture(void) {
    if (g_main) return;
    AvpArena *a = avp_arena();
    const char *main = avp_path_abspath(a, avp_join(a, "build", "tmp", "cpilot", "B", "repo shared main"));
    const char *wt = avp_path_abspath(a, avp_join(a, "build", "tmp", "cpilot", "B", "repo shared wt"));
    if (avp_isdir(wt)) AVP_ASSERT(avp_rmtree(wt, AVP_RMTREE_CLEAR_READONLY));
    if (avp_isdir(main)) AVP_ASSERT(avp_rmtree(main, AVP_RMTREE_CLEAR_READONLY));
    main = fx_dir("repo shared main");
    G(main, "init", "-q", "-b", "main");
    G(main, "config", "user.name", "Test User");
    G(main, "config", "user.email", "test@example.invalid");
    G(main, "config", "commit.gpgsign", "false");
    fx_write(main, "a.txt", "one\n");
    G(main, "add", "-A");
    G(main, "commit", "-q", "-m", "first");
    G(main, "worktree", "add", "-q", "-b", "lane", wt);
    fx_write(main, "orig/_ref/gormiti_wii/Exe/GormitiFinal.elf", "elf");
    fx_write(main, "orig/_iso/SLUS_000.00", "exe");
    fx_write(wt, "orig/_iso/own.iso", "iso");
    g_main = main;
    g_wt = wt;
}

static bool same_path(const char *x, const char *y) {
    AvpArena *a = avp_arena();
    return strcmp(shsm_git_path_key(a, x), shsm_git_path_key(a, y)) == 0;
}

AVP_TEST(main_root_of_a_work_tree) {
    fixture();
    AvpArena *a = avp_arena();
    ShsmRepo lane = {.root = g_wt}, main = {.root = g_main};
    CHECK(same_path(shsm_repo_main_root(a, &lane), g_main));
    CHECK(same_path(shsm_repo_main_root(a, &main), g_main));
}

AVP_TEST(shared_path_falls_back_to_the_main_checkout) {
    fixture();
    AvpArena *a = avp_arena();
    ShsmRepo lane = {.root = g_wt};
    /* absent in the lane, present in main: main's file */
    const char *elf = shsm_repo_shared_path(a, &lane, "orig/_ref/gormiti_wii/Exe/GormitiFinal.elf");
    CHECK(same_path(elf, avp_path_join(a, g_main, "orig/_ref/gormiti_wii/Exe/GormitiFinal.elf")));
    CHECK(avp_isfile(elf));
    /* present in the lane: the lane's own */
    const char *iso = shsm_repo_shared_path(a, &lane, "orig/_iso");
    CHECK(same_path(iso, avp_path_join(a, g_wt, "orig/_iso")));
    /* present nowhere: the lane path, so the caller's error names it */
    const char *none = shsm_repo_shared_path(a, &lane, "orig/_ref/none.elf");
    CHECK(same_path(none, avp_path_join(a, g_wt, "orig/_ref/none.elf")));
    /* the main checkout itself never falls back to itself */
    ShsmRepo main = {.root = g_main};
    CHECK(same_path(shsm_repo_shared_path(a, &main, "orig/_ref/none.elf"), avp_path_join(a, g_main, "orig/_ref/none.elf")));
}

AVP_TEST_MAIN("shsm_repo")
