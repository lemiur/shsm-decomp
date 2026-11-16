/* tools/test_unit.py NewBssTest and NewInferenceTest ported to C: the data inference of `unit.exe new` on the
 * .bss and weak-RTTI fixtures.
 *
 * Mapping (test_unit.py -> here):
 *   NewBssTest.test_sinit_defined_objects_give_a_bss_range               -> bss_sinit_defined_objects_give_a_range
 *   NewBssTest.test_shared_object_without_anchor_is_reported_with_the_flag -> bss_shared_object_without_anchor
 *   NewInferenceTest.test_weak_rtti_after_the_unit_is_taken              -> weak_rtti_after_the_unit_is_taken
 *   NewInferenceTest.test_no_asm_blocks_is_an_error_unless_allowed       -> no_asm_blocks_is_an_error_unless_allowed
 *   NewInferenceTest.test_non_game_mangled_stubs_carry_address_comments  -> non_game_mangled_stubs_carry_address_comments */
#include "fixture.h"

static const char *SPLITS_REL = "config/R5WEA4/splits.txt";

static Repo bss_repo(void) {
    Repo r = repo_new(false);
    add_bss_fixture(r.root);
    return r;
}

static Repo weak_repo(void) {
    Repo r = repo_new(false);
    add_weak_fixture(r.root);
    return r;
}

AVP_TEST(bss_sinit_defined_objects_give_a_range) {
    Repo r = bss_repo();
    Cli c = RUN(r, "new", "game/BssTest.cpp", "--text", "0x80030000-0x80030040", "--dry-run");
    CHECK_RC(c, 0);
    CHECK_IN(c.out, ".bss 0x80700000-0x80700034");
    static const char *const names[] = {"lbl_80700000", "lbl_80700008", "lbl_80700010"}; /* store / ctor / base reg */
    for (size_t i = 0; i < 3; i++)
        CHECK_IN(c.out, avp_sprintf(fx_a(), "%s: defined by __sinit_\\BssTest_cpp (stored to or constructed there)",
                                    names[i]));
    /* an address that is only copied does not make the unit its owner; a shared object is never guessed */
    CHECK_IN(c.out, "lbl_80700034: shared with fn_80040000, outside the hull (ADJACENT, ambiguous): NOT taken");
    CHECK_NOT_IN(c.out, "lbl_80700040"); /* not referenced by this unit */
    c = RUN(r, "new", "game/BssTest.cpp", "--text", "0x80030000-0x80030040");
    CHECK_RC(c, 0);
    CHECK_IN(fx_read(r.root, SPLITS_REL), "game/BssTest.cpp:\n\t.text       start:0x80030000 end:0x80030040\n"
                                          "\t.bss        start:0x80700000 end:0x80700034\n");
    repo_cleanup(&r);
}

AVP_TEST(bss_shared_object_without_anchor) {
    Repo r = bss_repo();
    Cli c = RUN(r, "new", "game/BssRead.cpp", "--text", "0x80050000-0x80050010", "--dry-run");
    CHECK_RC(c, 0);
    CHECK_IN(c.out, ".bss: no range inferred");
    CHECK_IN(c.out, "lbl_80700040: shared with fn_80040000; no private object of .bss anchors a range: NOT taken "
                    "(--include-data lbl_80700040 if this unit defines it)");
    c = RUN(r, "new", "game/BssRead.cpp", "--text", "0x80050000-0x80050010", "--include-data", "lbl_80700040");
    CHECK_RC(c, 0);
    CHECK_IN(fx_read(r.root, SPLITS_REL), ".bss        start:0x80700040 end:0x80700044");
    repo_cleanup(&r);
}

AVP_TEST(weak_rtti_after_the_unit_is_taken) {
    Repo r = weak_repo();
    Cli c = RUN(r, "new", "game/WkA.cpp", "--text", "0x80060000-0x80060010", "--dry-run");
    CHECK_RC(c, 0);
    CHECK_IN(c.out, ".data 0x80710000-0x80710030");
    CHECK_IN(c.out, "__RTTI__4CWkB: weak object reached from this unit's data, contiguous after it: taken");
    CHECK_IN(c.out, "lbl_80710020: referenced only by weak objects");
    CHECK_NOT_IN(c.out, "NOT taken");
    /* the vtable the unit's code reads is taken by default, with the reason; the objects next to the range say why
     * they are left out and which --include-data would take them */
    CHECK_IN(c.out, "__vt__4CWkA: vtable read only by this unit's code: taken by default");
    CHECK_IN(c.out, "__vt__4CWkB: neighbour after the range, read by fn_80070000, not by this unit's code: not taken "
                    "(--include-data __vt__4CWkB takes it)");
    /* once that unit holds them, the later unit leaves them alone */
    CHECK_EQ_I64(RUN(r, "new", "game/WkA.cpp", "--text", "0x80060000-0x80060010").rc, 0);
    c = RUN(r, "new", "game/WkB.cpp", "--text", "0x80070000-0x80070010", "--dry-run");
    CHECK_RC(c, 0);
    CHECK_IN(c.out, ".data 0x80710030-0x80710038");
    CHECK_IN(c.out, "__RTTI__4CWkB (.data 0x80710028, size 0x8): already in split game/WkA.cpp");
    repo_cleanup(&r);
}

AVP_TEST(no_asm_blocks_is_an_error_unless_allowed) {
    Repo r = weak_repo();
    AvpStr b_splits = fx_read(r.root, SPLITS_REL), b_cfg = fx_read(r.root, "configure.py");
    Cli c = RUN(r, "new", "game/NoAsm.cpp", "--text", "0x80090000-0x80090010");
    CHECK_RC(c, 1);
    CHECK_IN(c.out, "no asm blocks for this .text range");
    CHECK_IN(c.out, "--allow-no-asm");
    CHECK_BYTES_EQ(b_splits, fx_read(r.root, SPLITS_REL));
    CHECK_BYTES_EQ(b_cfg, fx_read(r.root, "configure.py"));
    CHECK(!fx_exists(r.root, "src/game/NoAsm.cpp"));
    c = RUN(r, "new", "game/NoAsm.cpp", "--text", "0x80090000-0x80090010", "--allow-no-asm");
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "[WARN] no asm blocks");
    CHECK_IN(fx_read(r.root, SPLITS_REL), "game/NoAsm.cpp:\n\t.text       start:0x80090000 end:0x80090010\n");
    repo_cleanup(&r);
}

AVP_TEST(non_game_mangled_stubs_carry_address_comments) {
    Repo r = weak_repo();
    Cli c = RUN(r, "new", "Runtime/foo.cpp", "--text", "0x80010000-0x80010040", "--lib", "Runtime");
    CHECK_RC(c, 0);
    AvpStr src = fx_read_lf(r.root, "src/Runtime/foo.cpp");
    static const char *const addrs[] = {"0x80010000", "0x80010010", "0x80010020"};
    for (size_t i = 0; i < 3; i++) CHECK_IN(src, avp_sprintf(fx_a(), "} // %s", addrs[i]));
    c = RUN(r, "check", "Runtime/foo.cpp");
    CHECK_NOT_IN(c.out, "[ERROR]");
    CHECK_NOT_IN(c.out, "no `// 0x8XXXXXXX` address comments");
    CHECK_NOT_IN(c.out, "functions have no address comment");
    CHECK_IN(c.out, "3 address comments match symbols.txt");
    repo_cleanup(&r);
}

/* A table of the unit's own function addresses that is not named __vt__ is a vtable too: taken by default, also when
 * another unit's code reads it. */
AVP_TEST(table_of_the_units_function_addresses_is_taken) {
    Repo r = repo_new(false);
    Cli c = RUN(r, "new", "game/CFooTest.cpp", "--text", "0x80010000-0x80010040", "--dry-run");
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "lbl_803E0000: table of this unit's function addresses (a vtable): taken by default");
    CHECK_IN(c.out, ".data 0x803E0000-0x803E002C");
    repo_cleanup(&r);
}

UNIT_TEST_MAIN("unit")
