/* shsm_mangle: demangle(), parse() and the encoder (mangle, mangle_type, mangle_scope ...) against recorded values,
 * and parse + mangle round trip of every symbol of config/<ver>/symbols.txt. */
#include "shsm_mangle.h"
#include "shsm_repo.h"
#include "shsm_symbols.h"
#include "shsm_test_util.h"

static const char *const CASES[][2] = {
    {"fn_800067CC__8unk_RampFf", "unk_Ramp::fn_800067CC(float)"},
    {"memcpy", NULL},
    {"__ct__3FooFv", "Foo::Foo()"},
    {"__dt__Q23foo3BarFv", "foo::Bar::~Bar()"},
    {"GetLineHeight__Q36nw4hbm2ut17TextWriterBase<c>CFv", "nw4hbm::ut::TextWriterBase<char>::GetLineHeight() const"},
    {"__as__3FooFRC3Foo", "Foo::operator=(const Foo&)"},
    {"__vt__13CPointTrigger", "CPointTrigger::__vtable"},
    {"@LOCAL@func__Fv@x", "func()::x"},
    {"@36@foo__3BarFv", "Bar::@36@foo()"},
    {"__opi__3FooFv", "Foo::operator int()"},
    {"f__FPFi_v", "f(void (*)(int))"},
    {"g__FA4_i", "g(int[4])"},
    {"h__FM3FooFP3Fooi_v", "h(void (Foo::*)(int))"},
    {"k__FPCc", "k(const char*)"},
    {"m__FRCPCVc", "m(const volatile char* const&)"},
    {"x__FUiSc", "x(unsigned int, signed char)"},
    {"__nw__FUl", "operator new(unsigned long)"},
    {"tmpl<i,3>__Fv", "tmpl<int, 3>()"},
    {"c__3FooFv_i", "int Foo::c()"},
    {"n__FUz", NULL},
    {"q__Q23abFv", NULL},
    {"a__3Foo<>Fv", NULL},
    {"a__6Foo<>Fv", NULL},
    {"_x__Fv", "_x()"},
    {"__x", NULL},
    {"f__Fe", "f(...)"},
    {"f__Fv_PFv_v", "void (*)() f()"},
    {"f__FPA3_i", "f(int(*)[3])"},
    {"f__3FooCFv", "Foo::f() const"},
    {"x__Q2", NULL},
};

AVP_TEST(demangle_cases) {
    for (size_t i = 0; i < AVP_ARRAY_LEN(CASES); i++) {
        const char *got = shsm_demangle(avp_arena(), CASES[i][0]);
        if (!CASES[i][1]) CHECK_MSG(got == NULL, "%s: expected None, got %s", CASES[i][0], got);
        else CHECK_MSG(got && strcmp(got, CASES[i][1]) == 0, "%s: got %s", CASES[i][0], got ? got : "None");
    }
}

AVP_TEST(demangle_errors_escape) {
    CHECK_RAISES(shsm_demangle(avp_arena(), "x__FP"), SHSM_EXC_RECURSION, "maximum recursion depth exceeded");
    CHECK_RAISES(shsm_demangle(avp_arena(), "x__FC"), SHSM_EXC_RECURSION, "maximum recursion depth exceeded");
    CHECK_RAISES(shsm_demangle(avp_arena(), "x__Qx3abcFv"), SHSM_EXC_VALUE,
                 "invalid literal for int() with base 10: 'x'");
}

AVP_TEST(qualified) {
    CHECK_CZ(shsm_qualified(avp_arena(), "fn_800067CC__8unk_RampFf"), "unk_Ramp::fn_800067CC");
    CHECK_CZ(shsm_qualified(avp_arena(), "memcpy"), "memcpy");
    CHECK_CZ(shsm_qualified(avp_arena(), "f__8A<PFi_v>Fv"), "A<void (*)(int)>::f");
    CHECK_CZ(shsm_demangle_or_name(avp_arena(), "memcpy"), "memcpy");
}

/* ---- the Python unit tests (tools/common/test_common.py, TestMangle), ported ---- */

/* Ported from tools/common/test_common.py TestMangle: CASES, NOT_MANGLED, MANGLE. */
static const char *const PY_CASES[][2] = {
    {"fn_800067A0__Fv", "fn_800067A0()"},
    {"fn_800067CC__8unk_RampFf", "unk_Ramp::fn_800067CC(float)"},
    {"fn_80006860__8unk_RampFfff", "unk_Ramp::fn_80006860(float, float, float)"},
    {"__ct__12unk_801A92A8Fv", "unk_801A92A8::unk_801A92A8()"},
    {"__dt__12CColdTriggerFv", "CColdTrigger::~CColdTrigger()"},
    {"fn_8009CB1C__12CColdTriggerFR7unk_Msg", "CColdTrigger::fn_8009CB1C(unk_Msg&)"},
    {"fn_801A73A8__14CBaseBehaviourFRC19unk_AttributePacket", "CBaseBehaviour::fn_801A73A8(const unk_AttributePacket&)"},
    {"fn_8009CD20__14CBaseBehaviourCFv", "CBaseBehaviour::fn_8009CD20() const"},
    {"fn_800E4148__17unk_HUDControllerFiPCciif", "unk_HUDController::fn_800E4148(int, const char*, int, int, float)"},
    {"__nw__FUlii", "operator new(unsigned long, int, int)"},
    {"__nwa__FUlii", "operator new[](unsigned long, int, int)"},
    {"__dl__FPv", "operator delete(void*)"},
    {"__dla__FPv", "operator delete[](void*)"},
    {"HBMFreeMem__FPv", "HBMFreeMem(void*)"},
    {"SetVolumeAllSeq__10homebuttonFf", "homebutton::SetVolumeAllSeq(float)"},
    {"__dt__Q210homebutton9RemoteSpkFv", "homebutton::RemoteSpk::~RemoteSpk()"},
    {"init__Q310homebutton3gui7ManagerFv", "homebutton::gui::Manager::init()"},
    {"update__Q310homebutton3gui7ManagerFiPC10KPADStatusffPv", "homebutton::gui::Manager::update(int, const KPADStatus*, float, float, void*)"},
    {"GetEncoding__Q46nw4hbm2ut6detail11ResFontBaseCFv", "nw4hbm::ut::detail::ResFontBase::GetEncoding() const"},
    {"Printf__Q36nw4hbm2ut17TextWriterBase<w>FPCwe", "nw4hbm::ut::TextWriterBase<wchar_t>::Printf(const wchar_t*, ...)"},
    {"CalcFormatStringRect__Q36nw4hbm2ut17TextWriterBase<c>CFPQ36nw4hbm2ut4RectPCce", "nw4hbm::ut::TextWriterBase<char>::CalcFormatStringRect(nw4hbm::ut::Rect*, const char*, ...) const"},
    {"__ct__Q36nw4hbm2ut17TextWriterBase<c>Fv", "nw4hbm::ut::TextWriterBase<char>::TextWriterBase()"},
    {"SetTagProcessor__Q36nw4hbm2ut17TextWriterBase<c>FPQ36nw4hbm2ut19TagProcessorBase<c>", "nw4hbm::ut::TextWriterBase<char>::SetTagProcessor(nw4hbm::ut::TagProcessorBase<char>*)"},
    {"SetGradationMode__Q36nw4hbm2ut10CharWriterFQ46nw4hbm2ut10CharWriter13GradationMode", "nw4hbm::ut::CharWriter::SetGradationMode(nw4hbm::ut::CharWriter::GradationMode)"},
    {"Insert__Q46nw4hbm2ut6detail12LinkListImplFQ56nw4hbm2ut6detail12LinkListImpl8IteratorPQ36nw4hbm2ut12LinkListNode", "nw4hbm::ut::detail::LinkListImpl::Insert(nw4hbm::ut::detail::LinkListImpl::Iterator, nw4hbm::ut::LinkListNode*)"},
    {"SetIndTexMtx__26@unnamed@lyt_material_cpp@F14_GXIndTexMtxIDPA3_Cf", "@unnamed@lyt_material_cpp@::SetIndTexMtx(_GXIndTexMtxID, const float(*)[3])"},
    {"CalcTextureMtx__26@unnamed@lyt_material_cpp@FPQ36nw4hbm4math5MTX34RCQ36nw4hbm3lyt6TexSRT", "@unnamed@lyt_material_cpp@::CalcTextureMtx(nw4hbm::math::MTX34*, const nw4hbm::lyt::TexSRT&)"},
    {"fn_802D1A58__FlUlPFPvll_PvPFPvll_PvPFPvPCvll_Pv", "fn_802D1A58(long, unsigned long, void* (*)(void*, long, long), void* (*)(void*, long, long), void* (*)(void*, const void*, long, long))"},
    {"__ct__12unk_801A8F6CFPCcPFRC19unk_AttributePacket_P17CAttributeHandlerUl", "unk_801A8F6C::unk_801A8F6C(const char*, CAttributeHandler* (*)(const unk_AttributePacket&), unsigned long)"},
    {"fn_801B05C0__FP19unk_ResourceManagerPCcP19unk_SpawnerResourceiPFUlUlUlUlUlPv_vPv", "fn_801B05C0(unk_ResourceManager*, const char*, unk_SpawnerResource*, int, void (*)(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, void*), void*)"},
    {"@36@fn_8009CB1C__15CFlickerTriggerFR7unk_Msg", "CFlickerTrigger::@36@fn_8009CB1C(unk_Msg&)"},
    {"@68@fn_801A73A8__13CPointTriggerFRC19unk_AttributePacket", "CPointTrigger::@68@fn_801A73A8(const unk_AttributePacket&)"},
    {"@LOCAL@WPADControlDpd__FlUlPFll_v@cfg1", "WPADControlDpd(long, unsigned long, void (*)(long, long))::cfg1"},
    {"__vt__13CPointTrigger", "CPointTrigger::__vtable"},
    {"__vt__Q36nw4hbm2ut19TagProcessorBase<w>", "nw4hbm::ut::TagProcessorBase<wchar_t>::__vtable"},
    {"CalcStringRectImpl<w>__25@unnamed@lyt_textBox_cpp@FPQ36nw4hbm2ut4RectPQ36nw4hbm2ut17TextWriterBase<w>PCwif_v", "void @unnamed@lyt_textBox_cpp@::CalcStringRectImpl<wchar_t>(nw4hbm::ut::Rect*, nw4hbm::ut::TextWriterBase<wchar_t>*, const wchar_t*, int, float)"},
    {"__HBMSEQInitTracks__FP15_HBMSEQSEQUENCEPUci", "__HBMSEQInitTracks(_HBMSEQSEQUENCE*, unsigned char*, int)"},
    {"fn_8000BF24__12unk_8000A040Fif", "unk_8000A040::fn_8000BF24(int, float)"},
    {"VPrintf__Q36nw4hbm2ut17TextWriterBase<c>FPCcP16__va_list_struct", "nw4hbm::ut::TextWriterBase<char>::VPrintf(const char*, __va_list_struct*)"},
    {"__as__3FooFRC3Foo", "Foo::operator=(const Foo&)"},
    {"__opb__3FooCFv", "Foo::operator bool() const"},
    {"f__FPCPc", "f(char* const*)"},
    {"f__FPCc", "f(const char*)"},
    {"g__FM3FooFPCvi_v", "g(void (Foo::*)(int))"},
    {"h__FRA4_i", "h(int(&)[4])"},
    {"k__FUxSc", "k(unsigned long long, signed char)"},
};
static const char *const PY_NOT_MANGLED[] = {"memcpy", "__sinit_\\PadRumble_cpp", "lbl_80001000", "@stringBase0", "__start", "_savegpr_14", "@etb_800066E0"};

typedef struct {
    const char *name, *scope;
    const char *params[4];
    size_t nparams;
    bool is_const;
    const char *want;
} MangleCase;
static const MangleCase PY_MANGLE[] = {
    {"fn_800067CC", "unk_Ramp", {"float"}, 1, false, "fn_800067CC__8unk_RampFf"},
    {"fn_800067A0", NULL, {NULL}, 0, false, "fn_800067A0__Fv"},
    {"unk_801A92A8", "unk_801A92A8", {NULL}, 0, false, "__ct__12unk_801A92A8Fv"},
    {"~CColdTrigger", "CColdTrigger", {NULL}, 0, false, "__dt__12CColdTriggerFv"},
    {"fn_8009CD20", "CBaseBehaviour", {NULL}, 0, true, "fn_8009CD20__14CBaseBehaviourCFv"},
    {"fn_801A73A8", "CBaseBehaviour", {"const unk_AttributePacket&"}, 1, false, "fn_801A73A8__14CBaseBehaviourFRC19unk_AttributePacket"},
    {"init", "homebutton::gui::Manager", {NULL}, 0, false, "init__Q310homebutton3gui7ManagerFv"},
    {"SetTagProcessor", "nw4hbm::ut::TextWriterBase<char>", {"nw4hbm::ut::TagProcessorBase<char>*"}, 1, false, "SetTagProcessor__Q36nw4hbm2ut17TextWriterBase<c>FPQ36nw4hbm2ut19TagProcessorBase<c>"},
    {"operator new", NULL, {"unsigned long", "int", "int"}, 3, false, "__nw__FUlii"},
    {"operator=", "Foo", {"const Foo&"}, 1, false, "__as__3FooFRC3Foo"},
    {"f", NULL, {"char* const*", "const char*", "..."}, 3, false, "f__FPCPcPCce"},
};

static bool streq(const char *a, const char *b) { return a && b && strcmp(a, b) == 0; }

AVP_TEST(python_demangle_table) {
    AvpArena *a = avp_arena();
    for (size_t i = 0; i < AVP_ARRAY_LEN(PY_CASES); i++) {
        const char *got = shsm_demangle(a, PY_CASES[i][0]);
        CHECK_MSG(streq(got, PY_CASES[i][1]), "%s: got %s", PY_CASES[i][0], got ? got : "None");
        ShsmDemangled *d = shsm_mangle_parse(a, PY_CASES[i][0]);
        CHECK_MSG(d && streq(shsm_demangled_text(a, d), PY_CASES[i][1]), "%s: parse text", PY_CASES[i][0]);
    }
    for (size_t i = 0; i < AVP_ARRAY_LEN(PY_NOT_MANGLED); i++) {
        CHECK_MSG(shsm_demangle(a, PY_NOT_MANGLED[i]) == NULL, "%s demangles", PY_NOT_MANGLED[i]);
        CHECK(shsm_mangle_parse(a, PY_NOT_MANGLED[i]) == NULL);
    }
}

AVP_TEST(parse_fields) {
    AvpArena *a = avp_arena();
    ShsmDemangled *d = shsm_mangle_parse(a, "GetLineHeight__Q36nw4hbm2ut17TextWriterBase<c>CFv");
    REQUIRE(d);
    CHECK_EQ_U64(d->nscope, 3);
    CHECK_CZ(d->scope[0], "nw4hbm");
    CHECK_CZ(d->scope[1], "ut");
    CHECK_CZ(d->scope[2], "TextWriterBase<char>");
    CHECK_CZ(d->name, "GetLineHeight");
    CHECK_CZ(d->raw_name, "GetLineHeight");
    CHECK_CZ(d->kind, "function");
    CHECK(!d->is_data && d->is_const && !d->is_volatile);
    CHECK_EQ_U64(d->nparams, 0);
    CHECK(d->ret == NULL && d->local == NULL);
    CHECK_CZ(d->thunk, "");

    d = shsm_mangle_parse(a, "__dt__12CColdTriggerFv");
    REQUIRE(d);
    CHECK_CZ(d->name, "~CColdTrigger");
    CHECK_CZ(d->raw_name, "__dt");

    d = shsm_mangle_parse(a, "__vt__13CPointTrigger");
    REQUIRE(d);
    CHECK_CZ(d->kind, "data");
    CHECK(d->is_data);
    CHECK_CZ(d->name, "__vtable");
    CHECK_CZ(d->raw_name, "__vt");
    CHECK_EQ_U64(d->nscope, 1);

    d = shsm_mangle_parse(a, "@36@fn_8009CB1C__15CFlickerTriggerFR7unk_Msg");
    REQUIRE(d);
    CHECK_CZ(d->thunk, "@36@");
    CHECK_CZ(d->name, "fn_8009CB1C");
    CHECK_EQ_U64(d->nparams, 1);
    CHECK_CZ(d->params[0], "unk_Msg&");
    CHECK_CZ(shsm_demangled_text(a, d), "CFlickerTrigger::@36@fn_8009CB1C(unk_Msg&)");

    d = shsm_mangle_parse(a, "@LOCAL@WPADControlDpd__FlUlPFll_v@cfg1");
    REQUIRE(d);
    CHECK_CZ(d->kind, "data");
    CHECK_CZ(d->local, "WPADControlDpd(long, unsigned long, void (*)(long, long))");
    CHECK_CZ(d->name, "cfg1");
    CHECK_CZ(d->raw_name, "");
    CHECK_EQ_U64(d->nscope, 0);

    d = shsm_mangle_parse(a, "f__Fv_PFv_v");
    REQUIRE(d);
    CHECK(d->ret != NULL);
    CHECK_CZ(shsm_demangled_text(a, d), "void (*)() f()");

    d = shsm_mangle_parse(a, "x__3FooCVFv");
    REQUIRE(d);
    CHECK(d->is_const && d->is_volatile);
    CHECK_CZ(shsm_demangled_text(a, d), "Foo::x() const volatile");

    d = shsm_mangle_parse(a, "__opi__3FooFv");
    REQUIRE(d);
    CHECK_CZ(d->name, "operator int");
    CHECK_CZ(d->raw_name, "__opi");

    CHECK_RAISES(shsm_mangle_parse(a, "x__FP"), SHSM_EXC_RECURSION, "maximum recursion depth exceeded");
    CHECK_RAISES(shsm_mangle_parse(a, "x__Qx3abcFv"), SHSM_EXC_VALUE, "invalid literal for int() with base 10: 'x'");
}

AVP_TEST(parse_to_json) {
    AvpArena *a = avp_arena();
    ShsmDemangled *d = shsm_mangle_parse(a, "GetLineHeight__Q36nw4hbm2ut17TextWriterBase<c>CFv");
    REQUIRE(d);
    AvpJsonDumpOpts o = AVP_JSON_COMPACT;
    CHECK_CZ(avp_json_dumps(a, shsm_demangled_to_json(a, d), &o).p,
             "{\"kind\":\"function\",\"scope\":[\"nw4hbm\",\"ut\",\"TextWriterBase<char>\"],\"name\":\"GetLineHeight\","
             "\"raw_name\":\"GetLineHeight\",\"params\":[],\"const\":true,\"volatile\":false,\"ret\":null,"
             "\"thunk\":\"\",\"local\":null,\"text\":\"nw4hbm::ut::TextWriterBase<char>::GetLineHeight() const\"}");
}

AVP_TEST(python_mangle_table) {
    AvpArena *a = avp_arena();
    for (size_t i = 0; i < AVP_ARRAY_LEN(PY_MANGLE); i++) {
        const MangleCase *c = &PY_MANGLE[i];
        const char *got = shsm_mangle(a, c->name, c->scope, c->params, c->nparams, c->is_const, false);
        CHECK_MSG(streq(got, c->want), "%s: got %s, want %s", c->name, got, c->want);
        CHECK_MSG(shsm_demangle(a, c->want) != NULL, "%s does not demangle", c->want);
    }
}

typedef struct {
    const char *text, *want;
} TextCase;

AVP_TEST(mangle_type_cases) {
    static const TextCase T[] = {
        {"int", "i"}, {"unsigned", "Ui"}, {"signed", "Si"}, {"unsigned long", "Ul"}, {"signed char", "Sc"},
        {"unsigned long long", "Ux"}, {"long long", "x"}, {"long double", "r"}, {"wchar_t", "w"}, {"...", "e"},
        {"const char*", "PCc"}, {"char* const", "CPc"}, {"const char* const", "CPCc"}, {"const volatile char*", "PCVc"},
        {"volatile const int", "VCi"}, {"unk_Msg&", "R7unk_Msg"}, {"const A::B&", "RCQ21A1B"},
        {"A::B<C::D>*", "PQ21A9B<Q21C1D>"}, {"char **", "PPc"}, {"char * const * volatile", "VPCPc"},
        {"constant", "8constant"}, {"const", "C"}, {"volatile", "V"}, {"", ""}, {"  ", ""}, {"unsigned*", "PUi"},
        {"Foo<int, 3>*", "P8Foo<i,3>"}, {"Foo<Bar<wchar_t>,-3>", "15Foo<6Bar<w>,-3>"}, {"::A", "1A"},
        {"a::b::c::d::e::f::g::h::i", "Q91a1b1c1d1e1f1g1h1i"}, {"A const*", "PC1A"}, {"A<x> const &", "RC5A<1x>"},
        {"  const   char   *  ", "PCc"}, {"char\t*", "Pc"}, {"char *constant", "CPc"}, {"int&&", "RRi"},
        {"const const int", "CCi"}, {"A<B<C<int> > >", "12A<8B<4C<i>>>"}, {"A< int >", "4A<i>"},
        {"A<const char*>", "6A<PCc>"}, {"\xc3\xa9", "1\xc3\xa9"}, {"\xc2\xa0int", "i"},
        {"std::pair<int, A::B<c>>&", "RQ23std18pair<i,Q21A5B<1c>>"},
        /* the Python reads 'A<B>::C' as the two words 'A<B>' and '::C' and joins them with a space: kept */
        {"A<B>::C", "Q25A<B> 1C"}, {"a::b<c::d>::e", "Q31a8b<c::d> 1e"},
    };
    AvpArena *a = avp_arena();
    for (size_t i = 0; i < AVP_ARRAY_LEN(T); i++) {
        const char *got = shsm_mangle_type(a, T[i].text);
        CHECK_MSG(streq(got, T[i].want), "'%s': got %s, want %s", T[i].text, got, T[i].want);
    }
}

AVP_TEST(mangle_errors) {
    AvpArena *a = avp_arena();
    const char *unsigned_foo[] = {"unsigned Foo"};
    const char *array[] = {"int[3]"};
    CHECK_RAISES(shsm_mangle_type(a, "void (*)(int)"), SHSM_EXC_VALUE, "unsupported type for mangling: void (*)(int)");
    CHECK_RAISES(shsm_mangle_type(a, " int[3] "), SHSM_EXC_VALUE, "unsupported type for mangling:  int[3] ");
    CHECK_RAISES(shsm_mangle_type(a, "unsigned Foo"), SHSM_EXC_VALUE, "signed/unsigned on a non-builtin: unsigned Foo");
    CHECK_RAISES(shsm_mangle_type(a, "a::b::c::d::e::f::g::h::i::j"), SHSM_EXC_VALUE, "scope too deep");
    CHECK_RAISES(shsm_mangle_type(a, "A<int[2]>"), SHSM_EXC_VALUE, "unsupported type for mangling: A<int[2]>");
    CHECK_RAISES(shsm_mangle_template_name(a, "A<int[2]>"), SHSM_EXC_VALUE, "unsupported type for mangling: int[2]");
    CHECK_RAISES(shsm_mangle(a, "f", NULL, unsigned_foo, 1, false, false), SHSM_EXC_VALUE,
                 "signed/unsigned on a non-builtin: unsigned Foo");
    /* the operator's type is mangled before the parameters */
    CHECK_RAISES(shsm_mangle(a, "operator void (*)(int)", NULL, array, 1, false, false), SHSM_EXC_VALUE,
                 "unsupported type for mangling:  void (*)(int)");
    CHECK_RAISES(shsm_mangle_scope(a, "a::b::c::d::e::f::g::h::i::j"), SHSM_EXC_VALUE, "scope too deep");
    /* the Python's recursion limit, as a depth: 300 template levels do not fit */
    AvpBuf deep = {0};
    for (int i = 0; i < 300; i++) avp_buf_putz(&deep, "A<");
    avp_buf_putz(&deep, "int");
    for (int i = 0; i < 300; i++) avp_buf_putc(&deep, '>');
    CHECK_RAISES(shsm_mangle_type(a, avp_buf_cz(&deep)), SHSM_EXC_RECURSION, "maximum recursion depth exceeded");
    avp_buf_free(&deep);
}

AVP_TEST(mangle_scope_and_template) {
    AvpArena *a = avp_arena();
    CHECK_CZ(shsm_mangle_scope(a, "A::B"), "Q21A1B");
    CHECK_CZ(shsm_mangle_scope(a, "A"), "1A");
    CHECK_CZ(shsm_mangle_scope(a, ""), "");
    CHECK_CZ(shsm_mangle_scope(a, "::"), "");
    CHECK_CZ(shsm_mangle_scope(a, "A<B::C>::D"), "Q29A<Q21B1C>1D");
    CHECK_CZ(shsm_mangle_scope(a, "A::::B"), "Q21A1B");
    CHECK_CZ(shsm_mangle_scope(a, ":::"), "1:");
    CHECK_CZ(shsm_mangle_scope(a, "\xc3\xa9::b"), "Q21\xc3\xa9"
                                                  "1b");
    const char *p1[] = {"nw4hbm", "ut", "TextWriterBase<wchar_t>"};
    CHECK_CZ(shsm_mangle_scope_parts(a, p1, 3), "Q36nw4hbm2ut17TextWriterBase<w>");
    CHECK_CZ(shsm_mangle_scope_parts(a, p1, 0), "");
    const char *p2[] = {"", "A", ""};
    CHECK_CZ(shsm_mangle_scope_parts(a, p2, 3), "1A");
    CHECK_CZ(shsm_mangle_template_name(a, "TextWriterBase<wchar_t>"), "TextWriterBase<w>");
    CHECK_CZ(shsm_mangle_template_name(a, "A<1,2>"), "A<1,2>");
    CHECK_CZ(shsm_mangle_template_name(a, "A<-3>"), "A<-3>");
    CHECK_CZ(shsm_mangle_template_name(a, "A<B<int>, C>"), "A<4B<i>,1C>");
    CHECK_CZ(shsm_mangle_template_name(a, "A< int , long >"), "A<i,l>");
    CHECK_CZ(shsm_mangle_template_name(a, "A<int"), "A<int");
    CHECK_CZ(shsm_mangle_template_name(a, "x"), "x");
    size_t n;
    const char **s = shsm_split_scope(a, "A::B<C::D>::E", &n);
    REQUIRE(n == 3);
    CHECK_CZ(s[0], "A");
    CHECK_CZ(s[1], "B<C::D>");
    CHECK_CZ(s[2], "E");
    s = shsm_split_scope(a, "", &n);
    CHECK(n == 1 && s[0][0] == 0);
    s = shsm_split_scope(a, ":::", &n);
    CHECK(n == 2 && s[0][0] == 0 && streq(s[1], ":"));
    CHECK_CZ(shsm_demangle_template_name(a, "TextWriterBase<w>"), "TextWriterBase<wchar_t>");
    CHECK_CZ(shsm_demangle_template_name(a, "A<Q23foo3Bar,i>"), "A<foo::Bar, int>");
    CHECK_CZ(shsm_demangle_template_name(a, "A<3>"), "A<3>");
    CHECK_CZ(shsm_demangle_template_name(a, "plain"), "plain");
    CHECK_RAISES(shsm_demangle_template_name(a, "A<Qx>"), SHSM_EXC_VALUE,
                 "invalid literal for int() with base 10: 'x'");
    CHECK_RAISES(shsm_demangle_template_name(a, "A<>"), SHSM_EXC_RECURSION, "maximum recursion depth exceeded");
}

AVP_TEST(mangle_special_names) {
    AvpArena *a = avp_arena();
    const char *none[] = {NULL};
    const char *cref[] = {"const Foo&"};
    const char *pv[] = {"void*"};
    const char *vd[] = {"void"};
    const char *empty[] = {""};
    const char *in[] = {"int"};
    CHECK_CZ(shsm_mangle(a, "TextWriterBase", "nw4hbm::ut::TextWriterBase<char>", none, 0, false, false),
             "__ct__Q36nw4hbm2ut17TextWriterBase<c>Fv");
    CHECK_CZ(shsm_mangle(a, "~TextWriterBase", "nw4hbm::ut::TextWriterBase<char>", none, 0, false, false),
             "__dt__Q36nw4hbm2ut17TextWriterBase<c>Fv");
    CHECK_CZ(shsm_mangle(a, "operator delete[]", NULL, pv, 1, false, false), "__dla__FPv");
    CHECK_CZ(shsm_mangle(a, "operator =", "Foo", cref, 1, false, false), "__as__3FooFRC3Foo");
    CHECK_CZ(shsm_mangle(a, "operator bool", "Foo", none, 0, true, false), "__opb__3FooCFv");
    CHECK_CZ(shsm_mangle(a, "operator const char*", "Foo", none, 0, true, false), "__opPCc__3FooCFv");
    CHECK_CZ(shsm_mangle(a, "f", NULL, vd, 1, false, false), "f__Fv");
    CHECK_CZ(shsm_mangle(a, "f", NULL, empty, 1, false, false), "f__Fv");
    CHECK_CZ(shsm_mangle(a, "f", "A", none, 0, true, true), "f__1ACVFv");
    /* scope "" is the one-part list [""], which names the constructor of nothing; NULL is None */
    CHECK_CZ(shsm_mangle(a, "", "", none, 0, false, false), "__ct__Fv");
    CHECK_CZ(shsm_mangle(a, "", NULL, none, 0, false, false), "__Fv");
    const char *ab[] = {"A", "B"};
    CHECK_CZ(shsm_mangle_parts(a, "f", ab, 2, in, 1, false, false), "f__Q21A1BFi");
    CHECK_CZ(shsm_mangle_parts(a, "B", ab, 2, none, 0, false, false), "__ct__Q21A1BFv");
    CHECK_CZ(shsm_mangle_parts(a, "f", ab, 0, none, 0, false, false), "f__Fv");
    /* mangle() cuts a scope string at every '::', also inside <...> (mangle_scope would not): kept */
    CHECK_CZ(shsm_mangle(a, "f", "A<B::C>::D", none, 0, false, false), "f__Q33A<B2C>1DFv");
    CHECK_CZ(shsm_mangle_scope(a, "A<B::C>::D"), "Q29A<Q21B1C>1D");
}

/* ---- every symbol of symbols.txt ---- */

/* Every symbol of symbols.txt: parse, and for an encodable function mangle(d.name, d.scope, d.params, ...), the round
 * trip. The round trip must give the symbol back, except for array and function-pointer parameters (unsupported type
 * for mangling), a return type and a template function's base name, which the mangler documents it cannot encode. */
AVP_TEST(symbols_roundtrip) {
    if (!avp_exists("configure.py") || !avp_isdir("config")) {
        printf("skipped: not in the repository\n");
        return;
    }
    ShsmRepo *r = shsm_repo_find(NULL);
    ShsmSymbolTable *st = shsm_load_symbols(r, false);
    size_t functions = 0, parsed = 0, encodable = 0, exact = 0, unsupported = 0, retname = 0, enc_total = 0;
    for (size_t i = 0; i < st->n; i++) {
        const ShsmSymbol *s = &st->symbols[i];
        bool fn = s->type && streq(s->type, "function");
        functions += fn;
        AvpArena *a = avp_scratch();
        AvpArenaMark mark = avp_arena_mark(a);
        ShsmDemangled *pd = shsm_mangle_parse(a, s->name);
        parsed += pd != NULL;
        bool encodes = pd && !pd->is_data && !*pd->thunk && !pd->local;
        if (encodes) {
            const char *m = NULL;
            const char *err = "";
            AvpTry t;
            if (AVP_TRY(&t)) {
                m = shsm_mangle_parts(a, pd->name, (const char *const *)pd->scope, pd->nscope,
                                      (const char *const *)pd->params, pd->nparams, pd->is_const, pd->is_volatile);
                avp_try_end(&t);
            } else {
                err = avp_strdup(a, t.msg);
            }
            enc_total++;
            if (fn) {
                encodable++;
                if (m && streq(m, s->name)) exact++;
                else if (!m && strstr(err, "unsupported type for mangling")) unsupported++;
                else if (m && (pd->ret || strchr(pd->name, '<'))) retname++;
                else CHECK_MSG(false, "%s: does not round-trip: %s", s->name, m ? m : err);
            }
        }
        avp_arena_reset(a, mark);
    }
    printf("%zu symbols (%zu functions, %zu parse as a mangled name); %zu symbols re-encode; of %zu functions, round "
           "trip exact %zu, unsupported parameter type %zu, return type or template base name %zu\n",
           st->n, functions, parsed, enc_total, encodable, exact, unsupported, retname);
    CHECK_EQ_U64(exact + unsupported + retname, encodable);
}

AVP_TEST_MAIN("shsm_mangle")
