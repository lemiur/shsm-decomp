/* tools/test_unit.py DemangleTest ported to C (the C API of unit_demangle).
 *
 * Mapping (test_unit.py -> here):
 *   DemangleTest.test_member                  -> member
 *   DemangleTest.test_ctor_dtor               -> ctor_dtor
 *   DemangleTest.test_free_and_basic_types    -> free_and_basic_types
 *   DemangleTest.test_const_method_and_backrefs -> const_method_and_backrefs
 *   DemangleTest.test_function_pointer        -> function_pointer
 *   DemangleTest.test_generated_and_plain     -> generated_and_plain
 *   DemangleTest.test_nested_is_complex       -> nested_is_complex
 *   DemangleTest.test_vtable_is_data          -> vtable_is_data */
#include "avp/test.h"
#include "unit.h"

#include <string.h>

static AvpArena *A(void) { return avp_arena(); }

/* assertEqual(d.args, [...]) */
static void check_args(const UnitDemangled *d, const char *const *want, size_t n, int line) {
    if (!d->has_args) {
        avp_test__fail(__FILE__, line, "args is None");
        return;
    }
    if (d->args.n != n) {
        avp_test__fail(__FILE__, line, "args has %zu entries, expected %zu", d->args.n, n);
        return;
    }
    for (size_t i = 0; i < n; i++)
        if (!avp_str_eqz(d->args.v[i], want[i]))
            avp_test__fail(__FILE__, line, "args[%zu] == \"%s\": got \"%.*s\"", i, want[i], AVP_SF(d->args.v[i]));
}
#define CHECK_ARGS(d, ...)                                                                          \
    do {                                                                                            \
        static const char *const want_[] = {__VA_ARGS__};                                           \
        check_args(&(d), want_, sizeof want_ / sizeof *want_, __LINE__);                            \
    } while (0)

AVP_TEST(member) {
    UnitDemangled d = unit_demangle(A(), "fn_800C2F54__19CAmbientTextMessageFRC19unk_AttributePacket");
    CHECK_CZ(unit_dm_qualified(A(), &d), "CAmbientTextMessage::fn_800C2F54");
    CHECK_ARGS(d, "const unk_AttributePacket&");
    CHECK(!d.is_const);
}

AVP_TEST(ctor_dtor) {
    UnitDemangled d = unit_demangle(A(), "__ct__8CCounterFv");
    CHECK(d.is_ctor);
    CHECK_CZ(unit_dm_qualified(A(), &d), "CCounter::CCounter");
    CHECK(d.has_args);
    CHECK_EQ_U64(d.args.n, 0);
    d = unit_demangle(A(), "__dt__8CCounterFv");
    CHECK_CZ(unit_dm_qualified(A(), &d), "CCounter::~CCounter");
}

AVP_TEST(free_and_basic_types) {
    UnitDemangled d = unit_demangle(A(), "fn_800C770C__Fi");
    CHECK_CZ(d.cls, "");
    CHECK_ARGS(d, "int");
    d = unit_demangle(A(), "fn_80006860__8unk_RampFfff");
    CHECK_ARGS(d, "float", "float", "float");
    d = unit_demangle(A(), "f__1AFUiUlUsUcPCcRfPPv");
    CHECK_ARGS(d, "unsigned int", "u32", "u16", "u8", "const char*", "float&", "void**");
}

AVP_TEST(const_method_and_backrefs) {
    UnitDemangled d = unit_demangle(A(), "g__3FooCFPcT1");
    CHECK(d.is_const);
    CHECK_ARGS(d, "char*", "char*");
    d = unit_demangle(A(), "h__3FooFiN21");
    CHECK_ARGS(d, "int", "int", "int");
}

AVP_TEST(function_pointer) {
    UnitDemangled d =
        unit_demangle(A(), "__ct__12unk_801A8F6CFPCcPFRC19unk_AttributePacket_P17CAttributeHandlerUl");
    CHECK_ARGS(d, "const char*", "CAttributeHandler* (*)(const unk_AttributePacket&)", "u32");
}

AVP_TEST(generated_and_plain) {
    UnitDemangled g = unit_demangle(A(), "__sinit_\\PadRumble_cpp");
    CHECK(unit_dm_generated(&g));
    UnitDemangled t = unit_demangle(A(), "@36@__dt__19CAmbientTextMessageFv");
    CHECK(unit_dm_generated(&t));
    CHECK_CZ(t.thunk, "@36@");
    UnitDemangled p = unit_demangle(A(), "OSInit");
    CHECK(!p.mangled);
    CHECK_CZ(p.base, "OSInit");
    UnitDemangled q = unit_demangle(A(), "fn_80077904");
    CHECK(!q.mangled);
}

AVP_TEST(nested_is_complex) {
    UnitDemangled d = unit_demangle(A(), "f__Q23Foo3BarFv");
    CHECK_CZ(d.cls, "Foo::Bar");
    CHECK(d.complex);
}

AVP_TEST(vtable_is_data) {
    UnitDemangled d = unit_demangle(A(), "__vt__19CAmbientTextMessage");
    CHECK(!d.mangled);
}

AVP_TEST_MAIN("unit")
