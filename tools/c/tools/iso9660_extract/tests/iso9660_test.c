/* Unit tests of the identifier rules of iso9660_extract (the rest is proven by the differential harness). */
#include "avp/test.h"
#include "iso9660_extract.h"

static AvpStr clean(const char *s, size_t n) { return iso9660_clean_name(avp_arena(), (const u8 *)s, n); }

AVP_TEST(version_suffix_is_dropped) {
    CHECK_STR(clean("SYSTEM.CNF;1", 12), "SYSTEM.CNF");
    CHECK_STR(clean("A.BIN;123", 9), "A.BIN");
    CHECK_STR(clean("NOVER", 5), "NOVER");
    CHECK_STR(clean("X;", 2), "X;");
    CHECK_STR(clean(";1", 2), ""); /* re.sub(r";\d+$", "", ";1") */
}

AVP_TEST(dot_entries) {
    CHECK_STR(clean("\0", 1), ".");
    CHECK_STR(clean("\1", 1), "..");
    CHECK_EQ_U64(clean("\2", 1).n, 1);
}

AVP_TEST(latin1_becomes_utf8) {
    CHECK_STR(clean("caf\xe9;1", 6), "caf\xc3\xa9");
}

AVP_TEST_MAIN("iso9660_extract")
