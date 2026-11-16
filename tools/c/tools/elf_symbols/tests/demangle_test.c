/* Unit tests of the demangler and the string mining of elf_symbols. */
#include "avp/test.h"
#include "elf_symbols.h"

static const char *dem(const char *sym) {
    AvpStr out;
    if (!elf_demangle_v2(avp_arena(), avp_str(sym), &out)) return NULL;
    return avp_cz(avp_arena(), out);
}

AVP_TEST(self_test_cases) {
    CHECK(strcmp(dem("ProcessStream__14CStreamHandlerPC7RwChunk"), "CStreamHandler::ProcessStream(RwChunk const *)") == 0);
    CHECK(strcmp(dem("__14CStreamHandler"), "CStreamHandler::CStreamHandler(void)") == 0);
    CHECK(strcmp(dem("_$_14CStreamHandler"), "CStreamHandler::~CStreamHandler()") == 0);
    CHECK(strcmp(dem("foo__Fiif"), "foo(int, int, float)") == 0);
    CHECK(strcmp(dem("Get__Q23RWS7CEntity"), "RWS::CEntity::Get(void)") == 0);
    CHECK(strcmp(dem("Run__C6CThing"), "CThing::Run(void) const") == 0);
}

AVP_TEST(special_names) {
    CHECK(strcmp(dem("__vt_14CStreamHandler"), "vtable for CStreamHandler") == 0);
    CHECK(strcmp(dem("__ti4CFoo"), "typeinfo for CFoo") == 0);
    CHECK(strcmp(dem("__pl__6CThingRC6CThing"), "CThing::operator+(CThing const &)") == 0);
}

AVP_TEST(failures_give_none) {
    CHECK(dem("main") == NULL);
    CHECK(dem("Tmpl__t4List1i") == NULL); /* templates are not supported */
    CHECK(dem("__Q0") == NULL);
}

AVP_TEST(scheme_detection) {
    AvpStr v[12];
    for (int i = 0; i < 12; i++) v[i] = avp_str("a__Fi");
    CHECK(strcmp(elf_detect_scheme(v, 12), "gnu-v2") == 0);
    CHECK(strcmp(elf_detect_scheme(v, 10), "none") == 0);
    for (int i = 0; i < 12; i++) v[i] = avp_str("_ZN1a1bEv");
    CHECK(strcmp(elf_detect_scheme(v, 12), "itanium") == 0);
}

AVP_TEST(mining) {
    static const char data[] = "xx\0c:/a/b.c\0abc\0Multiplayer::Msg::msgFoo\0CBikeCamera\0CAB12\0no.h\0";
    AvpStrVec paths = {0}, quals = {0}, cnames = {0};
    elf_mine_strings(avp_arena(), avp_strn(data, sizeof data - 1), &paths, &quals, &cnames);
    CHECK_EQ_U64(paths.n, 1);
    CHECK_STR(paths.v[0], "c:/a/b.c");
    CHECK_EQ_U64(quals.n, 1);
    CHECK_STR(quals.v[0], "Multiplayer::Msg::msgFoo");
    CHECK_EQ_U64(cnames.n, 1);
    CHECK_STR(cnames.v[0], "CBikeCamera");
}

AVP_TEST_MAIN("elf_symbols")
