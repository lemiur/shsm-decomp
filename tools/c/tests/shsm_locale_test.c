/* shsm_locale: reading with the default code page, encoding for stdout, the UTF-8 file writer. The cp1252 checks are
 * skipped on a machine with another ANSI code page. */
#include "avp/test.h"
#include "shsm_locale.h"

static const char *tmp_path(const char *name) {
    avp_makedirs("build/tmp/shsm_locale_test", true);
    return avp_path_join(avp_arena(), "build/tmp/shsm_locale_test", name);
}

static bool put(const char *name, const char *bytes, size_t n) { return avp_write_file(tmp_path(name), bytes, n); }

AVP_TEST(read_decodes_cp1252_and_translates_newlines) {
    if (shsm_locale_codepage() != 1252) return;
    REQUIRE(put("a.txt", "caf\xe9 \x80 \x85\r\nx\ry\n", 14));
    AvpStr text;
    REQUIRE(shsm_locale_read_text(avp_arena(), tmp_path("a.txt"), &text));
    CHECK_STR(text, "caf\xc3\xa9 \xe2\x82\xac \xe2\x80\xa6\nx\ny\n");
}

AVP_TEST(read_fails_on_the_undefined_cp1252_bytes) {
    if (shsm_locale_codepage() != 1252) return;
    static const char bad[] = {'\x81', '\x8d', '\x8f', '\x90', '\x9d'};
    for (size_t i = 0; i < sizeof bad; i++) {
        char data[3] = {'a', bad[i], 'b'};
        REQUIRE(put("b.txt", data, 3));
        AvpStr text;
        CHECK(!shsm_locale_read_text(avp_arena(), tmp_path("b.txt"), &text));
        CHECK_CZ(avp_io_err_class(), "UnicodeDecodeError");
    }
}

AVP_TEST(read_of_a_missing_file_is_file_not_found) {
    AvpStr text;
    CHECK(!shsm_locale_read_text(avp_arena(), tmp_path("no_such_file.txt"), &text));
    CHECK_CZ(avp_io_err_class(), "FileNotFoundError");
}

AVP_TEST(encode_for_stdout_is_strict) {
    if (shsm_locale_codepage() != 1252) return;
    AvpStr out;
    REQUIRE(shsm_locale_encode(avp_arena(), AVP_S("caf\xc3\xa9 \xe2\x82\xac \xe2\x80\xa6"), &out));
    CHECK_STR(out, "caf\xe9 \x80 \x85");
    CHECK(!shsm_locale_encode(avp_arena(), AVP_S("\xe4\xb8\xad"), &out));
    CHECK_CZ(avp_io_err_class(), "UnicodeEncodeError");
    /* U+0081 is a code point cp1252 does not define */
    CHECK(!shsm_locale_encode(avp_arena(), AVP_S("\xc2\x81"), &out));
    /* a lone surrogate (generalised UTF-8 form) */
    CHECK(!shsm_locale_encode(avp_arena(), AVP_S("\xed\xa0\x80"), &out));
}

AVP_TEST(utf8_writer_uses_crlf_and_refuses_surrogates) {
    const char *p = tmp_path("w.txt");
    REQUIRE(shsm_write_utf8_text(p, AVP_S("a\n\xc3\xa9\n")));
    AvpBytes raw = {0};
    REQUIRE(avp_read_all(avp_arena(), p, &raw));
    CHECK_EQ_U64(raw.n, 7);
    CHECK_MEM(raw.v, "a\r\n\xc3\xa9\r\n", 7);
    CHECK(!shsm_write_utf8_text(p, AVP_S("x\xed\xa0\x80")));
    CHECK_CZ(avp_io_err_class(), "UnicodeEncodeError");
    AvpBytes after = {0};
    REQUIRE(avp_read_all(avp_arena(), p, &after));
    CHECK_EQ_U64(after.n, 0);     /* open(p, "w") already emptied it */
}

AVP_TEST_MAIN("shsm_locale")
