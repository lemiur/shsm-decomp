/* shsm_links: junction / symbolic link detection, target reading, creation, link-only removal and the link walk, on
 * fixture folders (paths with spaces) under build/tmp/cpilot/B. Symbolic-link cases are skipped, with a note, when
 * the process may not create symbolic links. */
#include "avp/hash.h"
#include "shsm_links.h"
#include "shsm_test_util.h"

#include <errno.h>

/* ---- helpers ---------------------------------------------------------------------------------------------------- */

static const char *J(const char *a, const char *b) { return avp_join(avp_arena(), a, b); }

static bool same_path(const char *x, const char *y) {
    AvpArena *a = avp_arena();
    return strcmp(avp_path_normcase(a, avp_path_normpath(a, x)), avp_path_normcase(a, avp_path_normpath(a, y))) == 0;
}

/* A text description of everything under dir, links listed but not entered: folders, files with size and sha256,
 * links with kind and target. Independent of the code under test (scandir attributes only). */
static const char *snapshot(const char *dir, const char *rel) {
    AvpArena *a = avp_arena();
    AvpDirEntries it = {0};
    AVP_ASSERT(avp_scandir(a, dir, &it));
    const char *acc = "";
    for (size_t i = 0; i < it.n; i++) {
        const AvpDirEntry *e = &it.v[i];
        const char *r = avp_sprintf(a, "%s/%.*s", rel, AVP_SF(e->name));
        if (e->attrs & AVP_FILE_ATTRIBUTE_REPARSE_POINT) {
            char *t = NULL;
            AVP_ASSERT(avp_readlink(a, e->path.p, &t));
            acc = avp_sprintf(a, "%sL %s %s\n", acc, r, t);
        } else if (e->attrs & AVP_FILE_ATTRIBUTE_DIRECTORY) {
            acc = avp_sprintf(a, "%sD %s\n%s", acc, r, snapshot(e->path.p, r));
        } else {
            acc = avp_sprintf(a, "%sF %s %lld %s\n", acc, r, (long long)e->size,
                              avp_file_digest_hex(a, AVP_HASH_SHA256, e->path.p));
        }
    }
    avp_vec_free(&it);
    return acc;
}

/* base/tgt: a.txt, empty/, sub/deep/b.bin (every byte value), sub/c.txt. Returns the tgt path. */
static const char *make_target(const char *base) {
    const char *tgt = J(base, "tgt");
    fx_write(tgt, "a.txt", "alpha\r\nbeta\n");
    fx_write(tgt, "sub/c.txt", "gamma");
    AVP_ASSERT(avp_makedirs(J(tgt, "empty"), true));
    u8 bytes[256];
    for (int i = 0; i < 256; i++) bytes[i] = (u8)i;
    const char *b = J(tgt, "sub/deep");
    AVP_ASSERT(avp_makedirs(b, true));
    AVP_ASSERT(avp_write_file(J(b, "b.bin"), bytes, sizeof bytes));
    return tgt;
}

static bool symlinks_allowed(const char *base) {
    const char *probe = J(base, "probe-symlink");
    if (!avp_symlink(J(base, "tgt"), probe, true)) return false;
    AVP_ASSERT(shsm_link_remove(probe));
    return true;
}

static const char *kind_text(const char *p) { return shsm_link_kind_name(shsm_link_kind(p)); }

static void check_target(const char *link, const char *want) {
    char *t = shsm_link_target(avp_arena(), link);
    CHECK_MSG(t != NULL, "no target for %s", link);
    if (t) CHECK_MSG(same_path(t, want), "target of %s is %s, expected %s", link, t, want);
}

/* ---- detection, targets, creation ------------------------------------------------------------------------------- */

AVP_TEST(links_kinds_and_targets) {
    const char *base = fx_dir("links kinds");
    const char *tgt = make_target(base);
    const char *jn = J(base, "jn");
    CHECK(shsm_link_create_junction(jn, tgt));
    CHECK_CZ(kind_text(jn), "junction");
    check_target(jn, tgt);
    CHECK(avp_isdir(jn) && avp_exists(J(jn, "sub/c.txt")));

    /* ordinary things and missing paths are not links */
    CHECK_CZ(kind_text(tgt), "none");
    CHECK_CZ(kind_text(J(tgt, "a.txt")), "none");
    CHECK_CZ(kind_text(J(base, "missing")), "none");
    ShsmLinkKind k = SHSM_LINK_JUNCTION;
    CHECK(!shsm_link_stat(J(base, "missing"), &k) && k == SHSM_LINK_NONE);
    CHECK(shsm_link_stat(tgt, &k) && k == SHSM_LINK_NONE);
    CHECK(shsm_link_target(avp_arena(), tgt) == NULL && avp_io_err()->winerror == 4390);
    CHECK(shsm_link_target(avp_arena(), J(base, "missing")) == NULL);

    /* a relative target is made absolute; the link's missing parent folders are created */
    char *rel = NULL;
    CHECK(avp_path_relpath(avp_arena(), tgt, NULL, &rel));
    const char *jr = J(base, "x y/z/jrel");
    CHECK(shsm_link_create_junction(jr, rel));
    CHECK_CZ(kind_text(jr), "junction");
    check_target(jr, tgt);

    /* an existing path (folder, file, live or dangling link) is refused and left alone */
    const char *before = snapshot(tgt, "");
    CHECK(!shsm_link_create_junction(tgt, base) && avp_io_is(EEXIST));
    CHECK(!shsm_link_create_junction(J(tgt, "a.txt"), base) && avp_io_is(EEXIST));
    CHECK(!shsm_link_create_junction(jn, base) && avp_io_is(EEXIST));
    check_target(jn, tgt);
    CHECK_CZ(snapshot(tgt, ""), before);
}

AVP_TEST(links_symlink_kinds_and_targets) {
    const char *base = fx_dir("links symlink kinds");
    const char *tgt = make_target(base);
    if (!symlinks_allowed(base)) {
        printf("skipped: this process cannot create symbolic links (privilege / developer mode)\n");
        return;
    }
    const char *sd = J(base, "sd"), *sf = J(base, "sf"), *rd = J(base, "rd");
    CHECK(shsm_link_create_symlink(sd, tgt, true));
    CHECK(shsm_link_create_symlink(sf, J(tgt, "a.txt"), false));
    CHECK(shsm_link_create_symlink(rd, "tgt", true)); /* relative: stored as given */
    CHECK_CZ(kind_text(sd), "symlink-dir");
    CHECK_CZ(kind_text(sf), "symlink-file");
    CHECK_CZ(kind_text(rd), "symlink-dir");
    check_target(sd, tgt);
    check_target(sf, J(tgt, "a.txt"));
    CHECK_CZ(shsm_link_target(avp_arena(), rd), "tgt");
    CHECK(avp_exists(J(rd, "a.txt")));
    CHECK(!shsm_link_create_symlink(sd, tgt, true) && avp_io_is(EEXIST));
}

/* ---- walk -------------------------------------------------------------------------------------------------------- */

typedef struct {
    const char *rel;
    const char *kind;
} Want;

static void check_walk(const char *root, const Want *want, size_t n, const char *tgt_of_all) {
    ShsmLinkVec found = {0};
    CHECK(shsm_links_walk(avp_arena(), root, &found));
    CHECK_EQ_U64(found.n, n);
    for (size_t i = 0; i < n && i < found.n; i++) {
        CHECK_MSG(strcmp(found.v[i].rel, want[i].rel) == 0, "entry %zu: %s, expected %s", i, found.v[i].rel,
                  want[i].rel);
        CHECK_CZ(shsm_link_kind_name(found.v[i].kind), want[i].kind);
        CHECK_MSG(found.v[i].target != NULL, "entry %zu has no target", i);
        if (found.v[i].target && tgt_of_all) CHECK(same_path(found.v[i].target, tgt_of_all));
    }
    avp_vec_free(&found);
}

AVP_TEST(links_walk_does_not_descend) {
    const char *base = fx_dir("links walk");
    const char *tgt = make_target(base);
    /* junction to the target; a cycle (a junction inside the target back to the target); a nested ordinary folder
     * holding a junction; a dangling junction; a plain file and folder in between */
    CHECK(shsm_link_create_junction(J(base, "jn"), tgt));
    CHECK(shsm_link_create_junction(J(tgt, "sub/back"), tgt));
    CHECK(shsm_link_create_junction(J(base, "plain/inner/jn2"), tgt));
    fx_write(base, "plain/file.txt", "x");
    const char *gone = J(base, "gone");
    AVP_ASSERT(avp_makedirs(gone, true));
    CHECK(shsm_link_create_junction(J(base, "dj"), gone));
    AVP_ASSERT(avp_rmtree(gone, 0));
    CHECK_CZ(kind_text(J(base, "dj")), "junction");
    CHECK(!avp_exists(J(base, "dj")) && avp_lexists(J(base, "dj")));

    Want w[] = {{"dj", "junction"}, {"jn", "junction"}, {"plain/inner/jn2", "junction"}, {"tgt/sub/back", "junction"}};
    ShsmLinkVec found = {0};
    CHECK(shsm_links_walk(avp_arena(), base, &found));
    CHECK_EQ_U64(found.n, 4);
    for (size_t i = 0; i < 4 && i < found.n; i++) {
        CHECK_MSG(strcmp(found.v[i].rel, w[i].rel) == 0, "entry %zu: %s, expected %s", i, found.v[i].rel, w[i].rel);
        CHECK_CZ(shsm_link_kind_name(found.v[i].kind), w[i].kind);
    }
    if (found.n == 4) {
        CHECK(same_path(found.v[0].target, gone)); /* the dangling one still names its vanished target */
        CHECK(same_path(found.v[1].target, tgt));
        CHECK(same_path(found.v[2].target, tgt));
        CHECK(same_path(found.v[3].target, tgt));
    }
    avp_vec_free(&found);

    /* the target itself: only its inner cycle link, found as an ordinary folder's child */
    Want inner[] = {{"sub/back", "junction"}};
    check_walk(tgt, inner, 1, tgt);
    /* a root that is a link is the one entry "." and is not entered */
    Want root[] = {{".", "junction"}};
    check_walk(J(base, "jn"), root, 1, tgt);
    /* a folder without links, and a root that does not exist */
    check_walk(J(tgt, "empty"), NULL, 0, NULL);
    ShsmLinkVec none = {0};
    CHECK(!shsm_links_walk(avp_arena(), J(base, "missing"), &none) && none.n == 0);
}

AVP_TEST(links_walk_orders_by_name_per_folder) {
    const char *base = fx_dir("links walk order");
    const char *tgt = make_target(base);
    /* "a" and "a-c": a path sort puts a-c first ('-' < '\'), the per-folder walk lists a's contents first */
    CHECK(shsm_link_create_junction(J(base, "a/z"), tgt));
    CHECK(shsm_link_create_junction(J(base, "a-c"), tgt));
    CHECK(shsm_link_create_junction(J(base, "B"), tgt));
    CHECK(shsm_link_create_junction(J(base, "a/b"), tgt));
    Want w[] = {{"B", "junction"}, {"a/b", "junction"}, {"a/z", "junction"}, {"a-c", "junction"}};
    check_walk(base, w, 4, NULL);
}

AVP_TEST(links_walk_symlinks) {
    const char *base = fx_dir("links walk symlinks");
    const char *tgt = make_target(base);
    if (!symlinks_allowed(base)) {
        printf("skipped: this process cannot create symbolic links (privilege / developer mode)\n");
        return;
    }
    CHECK(shsm_link_create_junction(J(base, "jn"), tgt));
    CHECK(shsm_link_create_symlink(J(base, "sd"), tgt, true));
    CHECK(shsm_link_create_symlink(J(base, "sf"), J(tgt, "a.txt"), false));
    CHECK(shsm_link_create_symlink(J(base, "plain/sd2"), tgt, true));
    CHECK(shsm_link_create_symlink(J(tgt, "sub/loop"), tgt, true)); /* a symbolic-link cycle inside the target */
    Want w[] = {{"jn", "junction"},       {"plain/sd2", "symlink-dir"}, {"sd", "symlink-dir"},
                {"sf", "symlink-file"},   {"tgt/sub/loop", "symlink-dir"}};
    ShsmLinkVec found = {0};
    CHECK(shsm_links_walk(avp_arena(), base, &found));
    CHECK_EQ_U64(found.n, 5);
    for (size_t i = 0; i < 5 && i < found.n; i++) {
        CHECK_MSG(strcmp(found.v[i].rel, w[i].rel) == 0, "entry %zu: %s, expected %s", i, found.v[i].rel, w[i].rel);
        CHECK_CZ(shsm_link_kind_name(found.v[i].kind), w[i].kind);
        CHECK(found.v[i].target != NULL);
    }
    if (found.n == 5) CHECK(same_path(found.v[3].target, J(tgt, "a.txt")));
    avp_vec_free(&found);
}

/* ---- removal ------------------------------------------------------------------------------------------------------ */

AVP_TEST(links_remove_leaves_target_intact) {
    const char *base = fx_dir("links remove");
    const char *tgt = make_target(base);
    CHECK(shsm_link_create_junction(J(base, "jn"), tgt));
    CHECK(shsm_link_create_junction(J(base, "plain/inner/jn2"), tgt));
    CHECK(shsm_link_create_junction(J(tgt, "sub/back"), tgt)); /* inside the target: part of the snapshot */
    const char *before = snapshot(tgt, "");
    CHECK(strstr(before, "L /sub/back") != NULL && strstr(before, "F /sub/deep/b.bin 256 ") != NULL);

    CHECK(shsm_link_remove(J(base, "jn")));
    CHECK(!avp_lexists(J(base, "jn")));
    CHECK(avp_isdir(tgt));
    CHECK_CZ(snapshot(tgt, ""), before);
    CHECK(shsm_link_remove(J(base, "plain/inner/jn2")));
    CHECK(avp_isdir(J(base, "plain/inner")));
    CHECK_CZ(snapshot(tgt, ""), before);

    /* the cycle link inside the target: removing it removes only itself, not the target it points to */
    const char *back = J(tgt, "sub/back");
    CHECK(shsm_link_remove(back));
    CHECK(!avp_lexists(back));
    CHECK(avp_isdir(tgt) && avp_isfile(J(tgt, "sub/deep/b.bin")));
    CHECK(strstr(snapshot(tgt, ""), "back") == NULL);
    /* a second removal finds nothing and deletes nothing */
    CHECK(!shsm_link_remove(J(base, "jn")) && avp_io_is(ENOENT));
}

AVP_TEST(links_remove_refuses_non_links) {
    const char *base = fx_dir("links remove refuse");
    const char *tgt = make_target(base);
    CHECK(shsm_link_create_junction(J(base, "jn"), tgt));
    const char *all = snapshot(base, "");
    /* an ordinary non-empty folder, an empty folder, a file, a missing path, a path through a junction */
    CHECK(!shsm_link_remove(tgt) && avp_io_err()->winerror == 4390);
    CHECK(!shsm_link_remove(J(tgt, "empty")) && avp_io_err()->winerror == 4390);
    CHECK(!shsm_link_remove(J(tgt, "a.txt")) && avp_io_err()->winerror == 4390);
    CHECK(!shsm_link_remove(J(base, "missing")) && avp_io_is(ENOENT));
    CHECK(!shsm_link_remove(J(base, "jn/sub")) && avp_io_err()->winerror == 4390); /* inside a link, not the link */
    CHECK(!shsm_link_remove(J(base, "jn/a.txt")) && avp_io_err()->winerror == 4390);
    CHECK_CZ(snapshot(base, ""), all);
    CHECK(avp_isfile(J(tgt, "a.txt")) && avp_isdir(J(tgt, "empty")) && avp_isfile(J(tgt, "sub/deep/b.bin")));
    CHECK_CZ(kind_text(J(base, "jn")), "junction");
}

AVP_TEST(links_remove_dangling_junction) {
    const char *base = fx_dir("links remove dangling");
    const char *gone = J(base, "gone");
    fx_write(gone, "inner/file.txt", "data");
    const char *dj = J(base, "dj");
    CHECK(shsm_link_create_junction(dj, gone));
    AVP_ASSERT(avp_rmtree(gone, 0));
    CHECK(!avp_exists(dj) && avp_lexists(dj));
    CHECK_CZ(kind_text(dj), "junction");
    check_target(dj, gone);
    CHECK(shsm_link_remove(dj));
    CHECK(!avp_lexists(dj));
    CHECK(avp_isdir(base));
}

AVP_TEST(links_remove_symlinks) {
    const char *base = fx_dir("links remove symlinks");
    const char *tgt = make_target(base);
    if (!symlinks_allowed(base)) {
        printf("skipped: this process cannot create symbolic links (privilege / developer mode)\n");
        return;
    }
    const char *sd = J(base, "sd"), *sf = J(base, "sf"), *rd = J(base, "rd"), *dsf = J(base, "dsf");
    CHECK(shsm_link_create_symlink(sd, tgt, true));
    CHECK(shsm_link_create_symlink(sf, J(tgt, "a.txt"), false));
    CHECK(shsm_link_create_symlink(rd, "tgt", true));
    CHECK(shsm_link_create_symlink(dsf, J(base, "no such file"), false)); /* dangling file link */
    const char *before = snapshot(tgt, "");
    CHECK(shsm_link_remove(sd));
    CHECK(shsm_link_remove(sf));
    CHECK(shsm_link_remove(rd));
    CHECK(shsm_link_remove(dsf));
    CHECK(!avp_lexists(sd) && !avp_lexists(sf) && !avp_lexists(rd) && !avp_lexists(dsf));
    CHECK_CZ(snapshot(tgt, ""), before);
    CHECK(avp_isfile(J(tgt, "a.txt")));
}

AVP_TEST_MAIN("shsm_links")
