/* draft_grade.c: the pure parts (cli.first_error, the rejects table's parse and text, basis_of, local_copies on the
 * checkout's drive). Expected values are the Python's (tools.common.cli.first_error, draft.load_rejects /
 * save_rejects, draft.basis_of on the same inputs). grade, reject, rejects and packet are proven by the differential
 * cases and the real-tree comparison (DEVIATIONS.md). */
#include "avp/test.h"
#include "draft.h"

#include <string.h>

AVP_TEST(first_error) {
    AvpArena *a = avp_arena();
    AvpStr out = AVP_S("### mwcceppc.exe Compiler:\n"
                       "#    File: build\\tmp\\hand\\pad_broken.cpp\n"
                       "# --------------------------------------------\n"
                       "#     336:     lbl_80529148 = on +;\n"
                       "#   Error:                         ^\n"
                       "#   (10141) expression syntax error\n"
                       "#   Too many errors printed, aborting program\n");
    /* cli.first_error(out) == 'pad_broken.cpp:336: (10141) expression syntax error' */
    AvpStr e = draft_first_error(a, out);
    CHECK_CZ(avp_cz(a, e), "pad_broken.cpp:336: (10141) expression syntax error");
    AvpStr warn = AVP_S("### mwcceppc.exe Usage Warning:\n#   The option '-multibyte' is deprecated\n");
    CHECK(draft_first_error(a, warn).p == NULL);
    CHECK(draft_first_error(a, AVP_S("")).p == NULL);
    /* a message without a file: the tool's name; a "Linker Error" heading is not an Error block */
    AvpStr tool = AVP_S("### mwcceppc.exe Error:\n#   undefined: 'foo'\n");
    CHECK_CZ(avp_cz(a, draft_first_error(a, tool)), "mwcceppc.exe: undefined: 'foo'");
    CHECK(draft_first_error(a, AVP_S("### mwldeppc.exe Linker Error:\n#   undefined: 'foo'\n")).p == NULL);
}

AVP_TEST(rejects_text) {
    AvpArena *a = avp_arena();
    AvpStr text = AVP_S("symbol\tapproach\treason\tbasis\n"
                        "fn_1\ttwin\tbad\tabc\n"
                        "\n"
                        "   \n"
                        "only\n"
                        "a\tb\tc\td\te\n");
    DraftRejects rows = draft_parse_rejects(a, text);
    CHECK_EQ_I64((i64)rows.n, 3);
    CHECK_CZ(rows.v[0].symbol, "fn_1");
    CHECK_CZ(rows.v[0].basis, "abc");
    CHECK_CZ(rows.v[1].symbol, "only");
    CHECK_CZ(rows.v[1].approach, "");
    CHECK_CZ(rows.v[1].basis, "");
    CHECK_CZ(rows.v[2].basis, "d");
    /* save_rejects: tabs and newlines in a field become spaces; header first, final newline */
    DraftRejects w = {0};
    avp_vec_push(&w, ((DraftReject){"s", "twin", "a\tb\nc", "", false}));
    CHECK_CZ(avp_cz(a, draft_format_rejects(a, &w)), "symbol\tapproach\treason\tbasis\ns\ttwin\ta b c\t\n");
    DraftRejects none = {0};
    CHECK_CZ(avp_cz(a, draft_format_rejects(a, &none)), "symbol\tapproach\treason\tbasis\n");
    /* only the header: no rows */
    CHECK_EQ_I64((i64)draft_parse_rejects(a, AVP_S("symbol\tapproach\treason\tbasis\n")).n, 0);
}

AVP_TEST(basis_of) {
    AvpArena *a = avp_arena();
    ShsmRepo r = {0};
    r.root = avp_path_abspath(a, "build/tmp/draft_grade_test");
    CHECK(avp_makedirs(r.root, true));
    const char *src = avp_path_join(a, r.root, "x.cpp");
    CHECK(avp_write_file(src, "int x;\n", 7));
    TwinsFn f1 = {0}, f2 = {0}, f3 = {0};
    f1.name = "b_fn";
    f1.source = "x.cpp";
    f2.name = "a_fn";
    f2.source = "missing.cpp";
    f3.name = "c_fn";
    TwinsFn *fns[] = {&f1, &f2, &f3};
    /* h = sha1(); for t in sorted by name: update(name); update(file bytes) when readable
     * -> sha1(b"a_fn" b"b_fn" b"int x;\n" b"c_fn").hexdigest()[:12] */
    char *want = avp_sha1_hex(a, "a_fnb_fnint x;\nc_fn", 19);
    want[12] = 0;
    CHECK_CZ(draft_basis_of(a, &r, fns, 3), want);
    /* no twin: sha1(b"").hexdigest()[:12] */
    CHECK_CZ(draft_basis_of(a, &r, fns, 0), "da39a3ee5e6b");
}

AVP_TEST(local_copies_same_drive) {
    AvpArena *a = avp_arena();
    ShsmRepo r = {0};
    r.root = avp_path_abspath(a, ".");
    const char *paths[] = {avp_path_abspath(a, "src/x.cpp"), "include/y.h"};
    const char **out = draft_local_copies(a, &r, paths, 2);
    CHECK(out[0] == paths[0]);
    CHECK(out[1] == paths[1]);
}

AVP_TEST(clone_origin_dirs) {
    AvpArena *a = avp_arena();
    const char *base = avp_path_abspath(a, avp_join(a, "build", "tmp", "cpilot", "B", "draft clone dirs"));
    if (avp_isdir(base)) avp_rmtree(base, AVP_RMTREE_CLEAR_READONLY);
    const char *repo_inc = avp_join(a, base, "repo", "include"), *clone = avp_join(a, base, "draft", "include");
    AVP_ASSERT(avp_makedirs(avp_join(a, repo_inc, "fmod"), true));
    AVP_ASSERT(avp_makedirs(avp_join(a, repo_inc, "game"), true));
    AVP_ASSERT(avp_makedirs(avp_join(a, clone, "fmod"), true));
    AVP_ASSERT(avp_makedirs(avp_join(a, clone, "nowhere"), true));
    AvpStrVec roots = {0};
    avp_vec_push(&roots, avp_str(clone));
    const char *hdrs[] = {
        avp_join(a, clone, "fmod", "fmod_channelgroupi.h"), avp_join(a, clone, "fmod", "fmod_other.h"),
        avp_join(a, clone, "top.h"),                          avp_join(a, clone, "nowhere", "x.h"),
        avp_join(a, base, "elsewhere", "y.h"),
    };
    AvpStrVec dirs = draft_clone_origin_dirs(a, repo_inc, &roots, hdrs, 5);
    /* the original folder of a clone, once; none for a header directly under the root, one the repository lacks, or one
     * outside the draft's include folder */
    REQUIRE(dirs.n == 1);
    CHECK_CZ(avp_cz(a, dirs.v[0]), avp_join(a, repo_inc, "fmod"));
}

AVP_TEST_MAIN("draft_grade")
