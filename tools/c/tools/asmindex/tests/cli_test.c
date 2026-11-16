/* asmindex.exe on a synthetic checkout: TestStaleListings.test_index_command_prints_the_listings_note of
 * tools/common/test_common.py (the first run parses, the second finds nothing new; both print the listings note),
 * plus the missing-asm error. */
#include "avp/fs.h"
#include "shsm_asm.h"
#include "../../../tests/shsm_test_util.h"

#include "../../../tests/shsm_asm_fixture.h"

static const char *STALE_S = "# 0x80001010..0x80001018 | size: 0x8\n.text\n.balign 4\n\n"
                             "# .text:0x0 | 0x80001010 | size: 0x8\n.fn helper, global\n"
                             "/* 80001010 00000000  4E 80 00 20 */\tblr\n"
                             "/* 80001014 00000004  4E 80 00 20 */\tblr\n.endfn helper\n";

static const char *make_checkout(const char *name, bool with_asm) {
    const char *root = fx_dir(name);
    fx_write(root, "configure.py", "# synthetic\n");
    fx_write(root, "tools/.keep", "");
    fx_write(root, "config/TESTV1/config.yml", "object_base: orig/TESTV1\nobject: sys/main.dol\n");
    if (with_asm) {
        fx_write(root, "build/TESTV1/asm/game/Alpha.s", ALPHA_S);
        fx_write(root, "build/TESTV1/asm/game/Beta.s", BETA_S);
        fx_write(root, "build/TESTV1/asm/auto_03_80001010_text.s", STALE_S);
        fx_write(root, "build/TESTV1/config.json",
                 "{\"version\": \"1.8.4\", \"units\": [{\"object\": \"o\", \"name\": \"game/Alpha.cpp\"}, "
                 "{\"object\": \"o\", \"name\": \"game/Beta.c\"}], \"modules\": []}");
    }
    return root;
}

static ShsmRun run_exe(const char *root) {
    AvpArena *a = avp_arena();
    const char *exe = avp_path_join(a, avp_cz(a, avp_path_dirname(avp_exe_dir())), "asmindex.exe");
    const char *argv[] = {exe, "--root", root, "-j", "1"};
    return shsm_run(a, argv, 5, NULL, 120);
}

AVP_TEST(index_command_prints_the_listings_note) {
    const char *root = make_checkout("asmindex cli", true);
    AvpArena *a = avp_arena();
    ShsmListings *l = shsm_asm_listings(a, avp_join(a, root, "build", "TESTV1", "asm"));
    REQUIRE(l->note);
    const char *want = avp_sprintf(a, "note: %s", l->note);
    for (int i = 0; i < 2; i++) {
        ShsmRun r = run_exe(root);
        CHECK_EQ_I64(r.returncode, 0);
        AvpStrVec lines = avp_str_splitlines(r.out, false);
        size_t notes = 0;
        for (size_t k = 0; k < lines.n; k++)
            if (avp_str_startswith(lines.v[k], AVP_S("note: "))) {
                notes++;
                CHECK_STR(lines.v[k], want);
            }
        CHECK_EQ_U64(notes, 1);
        CHECK(strstr(avp_cz(a, r.out), i == 0 ? "2 parsed" : "0 parsed") != NULL);
    }
}

AVP_TEST(missing_asm_is_an_error) {
    const char *root = make_checkout("asmindex cli noasm", false);
    ShsmRun r = run_exe(root);
    CHECK_EQ_I64(r.returncode, 2);
    CHECK(strstr(avp_cz(avp_arena(), r.err), "asmindex: error: no retail asm at ") != NULL);
}

AVP_TEST_MAIN("asmindex")
