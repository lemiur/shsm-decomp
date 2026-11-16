/* shsm_is_placeholder_body: exactly the stub `unit.exe new` writes, in the game form and the library form. */
#include "shsm_placeholder.h"
#include "avp/test.h"

AVP_TEST(generated_stub_is_a_placeholder) {
    CHECK(shsm_is_placeholder_body(AVP_S("void fn_802C32E0() {\n" SHSM_PLACEHOLDER_BODY_LINE "\n}")));
    CHECK(shsm_is_placeholder_body(AVP_S("void fn_802C32E0() {\r\n" SHSM_PLACEHOLDER_BODY_LINE "\r\n} // 0x802C32E0")));
    CHECK(shsm_is_placeholder_body(AVP_S("void A::f(int x) {\n" SHSM_PLACEHOLDER_BODY_LINE "\n}")));
}

AVP_TEST(written_code_is_not_a_placeholder) {
    CHECK(!shsm_is_placeholder_body(AVP_S("void f() {\n}")));
    CHECK(!shsm_is_placeholder_body(AVP_S("void f() {\n    // TODO\n    g();\n}")));
    CHECK(!shsm_is_placeholder_body(AVP_S("void f() {\n    g();\n    // TODO\n}")));
    CHECK(!shsm_is_placeholder_body(AVP_S("void f() {\n    // TODO: later\n}")));
    CHECK(!shsm_is_placeholder_body(AVP_S("void f();")));
    CHECK(!shsm_is_placeholder_body(AVP_S("")));
}

AVP_TEST_MAIN("shsm_placeholder")
