/* tools/test_unit.py ParsingTest ported to C (the C API: configure.py, splits.txt, config.yml, ranges).
 *
 * Mapping (test_unit.py -> here):
 *   ParsingTest.test_configure_parse_and_status    -> configure_parse (the objects, libraries and statuses; the
 *                                                     status rewrite of set_status is static in the C port and is
 *                                                     tested through `park` in flip_test.c)
 *   ParsingTest.test_commented_object_is_ignored   -> commented_object_is_ignored
 *   ParsingTest.test_insert_object_orders_by_address -> insert_object_orders_by_address
 *   ParsingTest.test_splits_roundtrip              -> splits_roundtrip
 *   ParsingTest.test_force_active                  -> force_active
 *   ParsingTest.test_parse_range                   -> parse_range */
#include "fixture.h"

static AvpArena *A(void) { return avp_arena(); }

static AvpStrVec paths_of(AvpStr text) {
    UnitConfig *cfg = unit_parse_configure(A(), text);
    AvpStrVec v = {0};
    for (size_t i = 0; i < cfg->objects.n; i++) avp_vec_push(&v, avp_str(cfg->objects.v[i].path));
    return v;
}

static void check_paths(AvpStrVec got, const char *const *want, size_t n, int line) {
    if (got.n != n) {
        avp_test__fail(__FILE__, line, "%zu objects, expected %zu", got.n, n);
        return;
    }
    for (size_t i = 0; i < n; i++)
        if (!avp_str_eqz(got.v[i], want[i]))
            avp_test__fail(__FILE__, line, "object %zu: expected \"%s\", got \"%.*s\"", i, want[i], AVP_SF(got.v[i]));
}
#define CHECK_PATHS(text, ...)                                                                      \
    do {                                                                                            \
        static const char *const want_[] = {__VA_ARGS__};                                           \
        check_paths(paths_of(text), want_, sizeof want_ / sizeof *want_, __LINE__);                 \
    } while (0)

AVP_TEST(configure_parse) {
    UnitConfig *cfg = unit_parse_configure(A(), avp_str(FX_CONFIGURE));
    CHECK_PATHS(avp_str(FX_CONFIGURE), "Runtime/a.c", "game/Other.cpp", "game/After.cpp");
    REQUIRE(cfg->libs.n == 2);
    CHECK_CZ(cfg->libs.v[0].name, "Runtime");
    CHECK_CZ(cfg->libs.v[1].name, "game");
    const UnitObj *after = unit_config_find(cfg, "game/After.cpp");
    REQUIRE(after != NULL);
    CHECK_CZ(after->status, "NonMatching");
    const UnitObj *other = unit_config_find(cfg, "game/Other.cpp");
    REQUIRE(other != NULL);
    CHECK_CZ(other->status, "Matching");
    /* the status token sits where set_status will rewrite it */
    CHECK_MSG(after->status_end - after->status_start == strlen("NonMatching"), "status span %zu",
              after->status_end - after->status_start);
    CHECK(!strncmp(cfg->text.p + after->status_start, "NonMatching", strlen("NonMatching")));
}

AVP_TEST(commented_object_is_ignored) {
    AvpStr text = fx_replace(avp_str(FX_CONFIGURE), "            Object(Matching, \"game/Other.cpp\"),",
                             "            # Object(Matching, \"game/Ghost.cpp\"),\n"
                             "            Object(Matching, \"game/Other.cpp\"),");
    UnitConfig *cfg = unit_parse_configure(A(), text);
    CHECK(unit_config_find(cfg, "game/Ghost.cpp") == NULL);
}

AVP_TEST(insert_object_orders_by_address) {
    AvpStr text = avp_str(FX_CONFIGURE);
    UnitConfig *cfg = unit_parse_configure(A(), text);
    UnitSplits splits = unit_load_splits(A(), avp_str(FX_SPLITS));
    /* between Other (0x80000000) and After (0x80010040) */
    AvpStr nw = unit_insert_object(A(), cfg, &splits, 1, "game/Mid.cpp", 0x80010000);
    CHECK_PATHS(nw, "Runtime/a.c", "game/Other.cpp", "game/Mid.cpp", "game/After.cpp");
    CHECK_IN(nw, "            # a comment that belongs to the next object\n");
    CHECK_BEFORE(nw, "game/Mid.cpp", "a comment that belongs");
    /* after the last (a multi-line Object statement) */
    nw = unit_insert_object(A(), cfg, &splits, 1, "game/Last.cpp", 0x80030000);
    AvpStrVec last = paths_of(nw);
    REQUIRE(last.n > 0);
    CHECK_STR(last.v[last.n - 1], "game/Last.cpp");
    CHECK_IN(nw, "],\n            ),\n            Object(NonMatching, \"game/Last.cpp\"),\n");
    /* before the first */
    nw = unit_insert_object(A(), cfg, &splits, 1, "game/First.cpp", 0x7FFF0000);
    AvpStrVec first = paths_of(nw);
    REQUIRE(first.n > 1);
    CHECK_STR(first.v[1], "game/First.cpp");
}

AVP_TEST(splits_roundtrip) {
    UnitSplits sp = unit_load_splits(A(), avp_str(FX_SPLITS));
    REQUIRE(sp.n == 2);
    CHECK_CZ(sp.v[0].path, "game/Other.cpp");
    CHECK_CZ(sp.v[1].path, "game/After.cpp");
    const UnitRange *ctors = unit_ranges_get(&sp.v[0].ranges, ".ctors");
    REQUIRE(ctors != NULL);
    CHECK_EQ_U64(ctors->lo, 0x803D8360);
    CHECK_EQ_U64(ctors->hi, 0x803D8364);
    UnitRanges rng = {0};
    unit_ranges_set(&rng, ".text", 0x10, 0x20);
    unit_ranges_set(&rng, ".sdata2", 0x30, 0x40);
    const char *entry = unit_split_entry_text(A(), "game/X.cpp", &rng);
    CHECK_CZ(entry, "game/X.cpp:\n\t.text       start:0x00000010 end:0x00000020\n"
                    "\t.sdata2     start:0x00000030 end:0x00000040\n");
}

AVP_TEST(force_active) {
    Repo r = repo_new(false);
    AvpStrVec v = unit_load_force_active(A(), avp_join(A(), r.root, "config/R5WEA4/config.yml"));
    REQUIRE(v.n == 2);
    CHECK_STR(v.v[0], "__init_cpp_exceptions_reference");
    CHECK_STR(v.v[1], "lbl_80500FFF");
    repo_cleanup(&r);
}

static bool range_raises(const char *text) {
    AvpTry t;
    i64 lo = 0, hi = 0;
    if (AVP_TRY(&t)) {
        unit_parse_range(A(), text, &lo, &hi);
        avp_try_end(&t);
        return false;
    }
    return t.code == UNIT_TOOL_ERROR;
}

AVP_TEST(parse_range) {
    i64 lo = 0, hi = 0;
    CHECK(unit_parse_range(A(), "0x10-0x20", &lo, &hi));
    CHECK_EQ_I64(lo, 0x10);
    CHECK_EQ_I64(hi, 0x20);
    CHECK(unit_parse_range(A(), "10:20", &lo, &hi));
    CHECK_EQ_I64(lo, 0x10);
    CHECK_EQ_I64(hi, 0x20);
    CHECK(range_raises("20-10"));
}

UNIT_TEST_MAIN("unit")
