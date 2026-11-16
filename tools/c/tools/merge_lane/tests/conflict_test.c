/* ml_conflict_report: the hunks of a rebase conflict in configure.py or splits.txt, and the union of both sides' units. */
#include "ml_test_fixtures.h"

static Ml *ml_for_conflicts(void) {
    Ml *ml = avp_alloc(avp_arena(), sizeof *ml);
    ml->a = avp_arena();
    return ml;
}

static const char CONFIGURE[] =
    "libs = [\n"
    "<<<<<<< main\n"
    "    Object(Matching, \"a/x.c\"),\n"
    "    Object(NonMatching, \"a/y.c\"),\n"
    "=======\n"
    "    Object(NonMatching, \"a/x.c\"),\n"
    "    Object(NonMatching, \"a/z.c\"),\n"
    ">>>>>>> lane\n"
    "]\n";

static bool report_has(const AvpStrVec *r, const char *needle) {
    for (size_t i = 0; i < r->n; i++)
        if (avp_str_contains(r->v[i], avp_str(needle))) return true;
    return false;
}

AVP_TEST(configure_union_keeps_both_sides_and_takes_the_lane_status) {
    AvpStrVec report = {0};
    AvpStr resolved = {0};
    REQUIRE(ml_conflict_report(ml_for_conflicts(), "configure.py", avp_str(CONFIGURE), &report, &resolved));
    CHECK_STR(resolved,
              "libs = [\n"
              "    Object(NonMatching, \"a/x.c\"),\n"
              "    Object(NonMatching, \"a/y.c\"),\n"
              "    Object(NonMatching, \"a/z.c\"),\n"
              "]\n");
    CHECK(report_has(&report, "hunk 1 at line 2: main 2 line(s), lane 2 line(s)"));
    CHECK(report_has(&report, "keeps main's 2 unit(s), adds the lane's 1, takes the lane's version for 1 named by both (a/x.c)"));
}

AVP_TEST(configure_with_other_code_in_a_hunk_is_not_resolved) {
    AvpStrVec report = {0};
    AvpStr resolved = {0};
    const char *t = "<<<<<<< main\n    Object(Matching, \"a/x.c\"),\n    cflags = x,\n=======\n    Object(Matching, \"a/y.c\"),\n>>>>>>> lane\n";
    CHECK(!ml_conflict_report(ml_for_conflicts(), "configure.py", avp_str(t), &report, &resolved));
    CHECK(report_has(&report, "no union offered"));
    CHECK(resolved.n == 0);
    /* a multi-line Object( is not a single-line entry */
    t = "<<<<<<< main\n    Object(\n        Matching, \"a/x.c\"),\n=======\n    Object(Matching, \"a/y.c\"),\n>>>>>>> lane\n";
    CHECK(!ml_conflict_report(ml_for_conflicts(), "configure.py", avp_str(t), &report, &resolved));
}

AVP_TEST(splits_union_keeps_blocks_of_both_sides_and_the_lane_block_for_a_shared_unit) {
    const char *t =
        "Sections:\n\t.text type:code align:8\n\n"
        "<<<<<<< main\n"
        "a/x.cpp:\n\t.text start:0x1 end:0x2\n\n"
        "b/main_only.cpp:\n\t.text start:0x10 end:0x20\n\n"
        "=======\n"
        "a/x.cpp:\n\t.text start:0x1 end:0x3\n\n"
        "c/lane_only.cpp:\n\t.text start:0x30 end:0x40\n\n"
        ">>>>>>> lane\n"
        "d/after.cpp:\n\t.text start:0x50 end:0x60\n";
    AvpStrVec report = {0};
    AvpStr resolved = {0};
    REQUIRE(ml_conflict_report(ml_for_conflicts(), "config/R5WEA4/splits.txt", avp_str(t), &report, &resolved));
    CHECK_STR(resolved,
              "Sections:\n\t.text type:code align:8\n\n"
              "a/x.cpp:\n\t.text start:0x1 end:0x3\n\n"
              "b/main_only.cpp:\n\t.text start:0x10 end:0x20\n\n"
              "c/lane_only.cpp:\n\t.text start:0x30 end:0x40\n\n"
              "d/after.cpp:\n\t.text start:0x50 end:0x60\n");
    CHECK(report_has(&report, "takes the lane's version for 1 named by both (a/x.cpp)"));
    /* a hunk that starts inside a block is left to the caller */
    t = "<<<<<<< main\n\t.text start:0x1 end:0x2\n=======\n\t.text start:0x1 end:0x3\n>>>>>>> lane\n";
    CHECK(!ml_conflict_report(ml_for_conflicts(), "config/R5WEA4/splits.txt", avp_str(t), &report, &resolved));
}

AVP_TEST(crlf_files_keep_their_line_ends) {
    AvpStrVec report = {0};
    AvpStr resolved = {0};
    const char *t = "<<<<<<< main\r\nObject(Matching, \"a/x.c\"),\r\n=======\r\nObject(Matching, \"a/y.c\"),\r\n>>>>>>> lane\r\nend\r\n";
    REQUIRE(ml_conflict_report(ml_for_conflicts(), "configure.py", avp_str(t), &report, &resolved));
    CHECK_STR(resolved, "Object(Matching, \"a/x.c\"),\r\nObject(Matching, \"a/y.c\"),\r\nend\r\n");
}

AVP_TEST(other_files_are_not_handled) {
    AvpStrVec report = {0};
    AvpStr resolved = {0};
    CHECK(!ml_conflict_report(ml_for_conflicts(), "src/a.cpp", avp_str(CONFIGURE), &report, &resolved));
    CHECK(report.n == 0);
}

AVP_TEST_MAIN("merge_lane_conflict")
