/* transform_dep: the pure pieces of the port against values tools/transform_dep.py gives (import_d_file over decoded
 * text, the wine prefix directory). The tool as a whole (file reading in the locale encoding, the CRLF output file,
 * tracebacks) is checked by the differential harness (diff/cases.txt). */
#include "avp/test.h"
#include "transform_dep.h"

#define DEVICES "C:\\wine_probe_nonexistent\\dosdevices"

static AvpStr run(const char *text) { return transform_dep_text(avp_arena(), avp_str(text), DEVICES); }

AVP_TEST(first_line_gets_forward_slashes) {
    CHECK_STR(run("a\\b.o: c\\d.c \\\n"), "a/b.o: c/d.c \\\n");
    CHECK_STR(run("a\\b\n"), "a/b\n");
    CHECK_STR(run("a \\ b"), "a / b");
    CHECK_STR(run("\\\n"), "/\n");
    CHECK_STR(run(""), "");
}

AVP_TEST(z_drive_paths_drop_the_drive) {
    CHECK_STR(run("a\\b.o: c\\d.c \\\n\tZ:\\usr\\include\\a.h \\\n\tz:/x\\y\n"),
              "a/b.o: c/d.c \\\n\t/usr/include/a.h \\\n\t/x/y\n");
    CHECK_STR(run("x \\\n\tZ:\\\n"), "x \\\n\t/\n");
    /* only the first two characters go, whatever they are */
    CHECK_STR(run("x:y \\\n\tz\n\tZ:\n"), "x:y \\\n\t\n\t\n");
}

AVP_TEST(other_drives_are_resolved_as_ntpath_realpath) {
    /* an existing path comes back with the case the file system has, and an upper-case drive */
    CHECK_STR(run("a \\\n  \tc:\\windows\\system32 \n"), "a \\\n\tC:\\Windows\\System32\n");
    /* a missing one is only normalised */
    CHECK_STR(run("a \\\n\tE:\\no\\such\\dir\\..\\x.h\n"), "a \\\n\tE:\\no\\such\\x.h\n");
    CHECK_STR(run("a\n\tC:/no/such/./x.h \\\n"), "a\n\tC:\\no\\such\\x.h \\\n");
}

AVP_TEST(relative_paths_join_onto_the_dosdevices_directory) {
    CHECK_STR(run("a \\\n\tinclude\\a.h\n"), "a \\\n\t" DEVICES "\\include\\a.h\n");
    /* a path that starts with a backslash is rooted on the prefix's drive, not on the directory */
    CHECK_STR(run("a \\\n\t\\x\\a.h\n"), "a \\\n\tC:\\x\\a.h\n");
}

AVP_TEST(continuation_marker_is_kept_only_with_its_space) {
    CHECK_STR(run("a \\\n\tE:\\x\\a.h \\"), "a \\\n\tE:\\x\\a.h \n");
    CHECK_STR(run("a \\\n\tE:\\x\\a.h\\\n"), "a \\\n\tE:\\x\\a.h\n");   /* no marker: realpath drops the trailing separator */
    CHECK_STR(run("a \\\n\\ \\\n"), "a \\\n\tC:\\ \\\n");
}

AVP_TEST(python_whitespace_is_stripped) {
    CHECK_STR(run("a \\\n\xc2\xa0" "E:\\x\xc2\xa0\n"), "a \\\n\tE:\\x\n");        /* U+00A0 */
    CHECK_STR(run("a \\\n\xe3\x80\x80" "E:\\x\xe3\x80\x80 \\\n"), "a \\\n\tE:\\x\xe3\x80\x80 \\\n");    /* U+3000: stripped in front, kept before the marker */
    CHECK_STR(run("a \\\n\x0b" "E:\\x\x0c\n"), "a \\\n\tE:\\x\n");
}

AVP_TEST(first_letter_is_lowered_as_unicode) {
    /* U+00C4 is lowered to U+00E4 and joined onto the directory; the rest of the path keeps its case */
    CHECK_STR(run("a \\\n\t\xc3\x84:\\x\n"), "a \\\n\t\xc3\xa4:\\x\n");
}

static void expect_index_error(const char *text) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        run(text);
        avp_try_end(&t);
        CHECK_MSG(false, "no exception for %s", text);
    } else {
        CHECK_CZ(t.msg, "IndexError: string index out of range");
    }
}

AVP_TEST(empty_path_line_is_an_index_error) {
    expect_index_error("a\n\n");
    expect_index_error("a \\\n   \n");
    expect_index_error("a \\\n \\\n");
    expect_index_error("a \\\n   \\\n");
    expect_index_error("a \\\n\xc2\xa0\n");
}

AVP_TEST(line_ends_are_the_decoded_text_ones) {
    /* the file reader has already turned \r\n and \r into \n; the writer adds the \r back */
    CHECK_STR(run("a\\b \\\n\tZ:\\x \\\n\tZ:\\y\n"), "a/b \\\n\t/x \\\n\t/y\n");
    CHECK_STR(run("a\\b \\\n\tZ:\\x \\\n\tZ:\\y"), "a/b \\\n\t/x \\\n\t/y\n");
}

AVP_TEST(winedevices_follows_wineprefix_then_home) {
    AvpArena *a = avp_arena();
    char *saved_home = avp_getenv(a, "HOME");
    char *saved_prefix = avp_getenv(a, "WINEPREFIX");
    avp_setenv("HOME", "C:\\home\\u");
    avp_unsetenv("WINEPREFIX");
    CHECK_CZ(transform_dep_winedevices(a), "C:\\home\\u\\.wine\\dosdevices");
    avp_setenv("WINEPREFIX", "D:\\prefix");
    CHECK_CZ(transform_dep_winedevices(a), "D:\\prefix\\dosdevices");
    avp_setenv("WINEPREFIX", "rel/prefix");
    CHECK_CZ(transform_dep_winedevices(a), "rel/prefix\\dosdevices");
    avp_unsetenv("WINEPREFIX");
    avp_unsetenv("HOME");
    CHECK(transform_dep_winedevices(a) == NULL);
    avp_setenv("WINEPREFIX", "D:\\prefix");
    CHECK_CZ(transform_dep_winedevices(a), "D:\\prefix\\dosdevices");   /* HOME is not needed then */
    if (saved_home) avp_setenv("HOME", saved_home);
    if (saved_prefix) avp_setenv("WINEPREFIX", saved_prefix);
    else avp_unsetenv("WINEPREFIX");
}

AVP_TEST_MAIN("transform_dep_transform")
