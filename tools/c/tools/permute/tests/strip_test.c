/* Tests of permute_strip (tools/test_permute.py: SymbolNames, StripCxx; plus the C stripper and strip_extern_c). */
#include "avp/test.h"
#include "permute_strip.h"

static const char UNIT[] =
    "namespace game {\n"
    "class Counter {\n"
    "public:\n"
    "    Counter(int start) : mCount(start) {}\n"
    "    virtual ~Counter() {}\n"
    "    inline int Get() const { return mCount; }\n"
    "    void Add(int n);\n"
    "    int Twice(int n) const;\n"
    "    int Total() const;\n"
    "    static int Clamp(int v);\n"
    "private:\n"
    "    int mCount;\n"
    "    int mLimit;\n"
    "};\n"
    "}\n"
    "\n"
    "static int Square(int v) { return v * v; }\n"
    "int Helper(int v) { return v + 1; }\n"
    "int Unrelated(int v) { return v - 1; }\n"
    "\n"
    "namespace game {\n"
    "int Counter::Clamp(int v) { return v > 100 ? 100 : v; }\n"
    "\n"
    "void Counter::Add(int n) {\n"
    "    mCount = Clamp(mCount + Square(n));\n"
    "}\n"
    "\n"
    "int Counter::Twice(int n) const {\n"
    "    return Helper(n) * 2 + Get();\n"
    "}\n"
    "\n"
    "int Counter::Total() const {\n"
    "    return Unrelated(mCount);\n"
    "}\n"
    "}\n";

static AvpStr name_of(const char *sym)
{
    AvpStr out = {0};
    if (!permute_source_name(avp_arena(), avp_str(sym), &out)) return AVP_S("<None>");
    return out;
}

AVP_TEST(member_function)
{
    CHECK_STR(name_of("DrawFrame__Q36nw4hbm3lyt6WindowFRCQ36nw4hbm4math4VEC2RCQ46nw4hbm3lyt6Window5FrameRCQ36nw4hbm3l"
                      "yt15WindowFrameSizeUc"),
              "DrawFrame");
}

AVP_TEST(ctor_dtor_operator)
{
    CHECK_STR(name_of("__ct__Q36nw4hbm3lyt6WindowFPCQ46nw4hbm3lyt3res6WindowRCQ36nw4hbm3lyt11ResBlockSet"), "Window");
    CHECK_STR(name_of("__dt__Q36nw4hbm3lyt6WindowFv"), "~Window");
    CHECK_STR(name_of("__as__Q24game7CounterFRCQ24game7Counter"), "operator");
    CHECK_STR(name_of("__ct__8CFooTestFv"), "CFooTest");
}

AVP_TEST(free_function_and_placeholders)
{
    CHECK_STR(name_of("fn_800C770C__Fi"), "fn_800C770C");
    CHECK_STR(name_of("fn_800067CC__8unk_RampFf"), "fn_800067CC");
}

AVP_TEST(c_linkage_is_not_mangled)
{
    const char *syms[] = {"__copy_longs_rev_unaligned", "OSInit", "__OSInitAlarm", "fn_80006860", "_savegpr_14",
                          "__sinit_\\PadRumble_cpp"};
    for (size_t i = 0; i < 6; i++) {
        CHECK_STR(name_of(syms[i]), "<None>");
        CHECK(!permute_is_mangled(avp_str(syms[i])));
    }
}

/* D-permute-2: a thunk is written as its target's definition (the Python returned None or "@36@"). */
AVP_TEST(thunk_names_are_their_targets)
{
    CHECK_STR(name_of("@36@__dt__10FMVTriggerFv"), "~FMVTrigger");
    CHECK_STR(name_of("@36@fn_8009CB1C__14CBaseBehaviourFR7unk_Msg"), "fn_8009CB1C");
    CHECK(permute_is_mangled(AVP_S("@36@__dt__10FMVTriggerFv")));
}

AVP_TEST(dir_name_is_bounded_and_stable)
{
    AvpArena *a = avp_arena();
    AvpStr lng = AVP_S("DrawFrame__Q36nw4hbm3lyt6WindowFRCQ36nw4hbm4math4VEC2RCQ46nw4hbm3lyt6Window5FrameRCQ36nw4hbm3l"
                       "yt15WindowFrameSizeUc");
    AvpStr d = permute_dir_name(a, lng);
    CHECK(d.n <= 64);
    CHECK(avp_str_eq(d, permute_dir_name(a, lng)));
    CHECK(avp_str_startswith(d, avp_strn(lng.p, 48)));
    CHECK_STR(permute_dir_name(a, AVP_S("OSInit")), "OSInit");
}

static AvpStr strip(const char *src, const char *keep, bool callees_too, size_t *kept, AvpStrVec *callees)
{
    AvpStr k = avp_str(keep);
    return permute_strip_other_fns_cxx(avp_arena(), avp_str(src), &k, 1, callees_too, kept, callees);
}

#define IN(t, lit) CHECK_MSG(avp_str_contains((t), AVP_S(lit)), "missing: %s", lit)
#define NOT_IN(t, lit) CHECK_MSG(!avp_str_contains((t), AVP_S(lit)), "present: %s", lit)

AVP_TEST(free_function_becomes_declaration_and_member_is_dropped)
{
    size_t kept;
    AvpStrVec callees;
    AvpStr t = strip(UNIT, "Add", false, &kept, &callees);
    CHECK_EQ_I64(kept, 1);
    CHECK_EQ_I64(callees.n, 0);
    IN(t, "mCount = Clamp(mCount + Square(n));");
    NOT_IN(t, "return v - 1;");
    IN(t, "int Unrelated(int v);");
    NOT_IN(t, "Counter::Twice");
    NOT_IN(t, "Counter::Total");
    IN(t, "inline int Get() const { return mCount; }");
    IN(t, "Counter(int start) : mCount(start) {}");
    IN(t, "static int Clamp(int v);");
}

AVP_TEST(callees_stay_transitively)
{
    size_t kept;
    AvpStrVec callees;
    AvpStr t = strip(UNIT, "Add", true, &kept, &callees);
    REQUIRE(callees.n == 2);
    CHECK_STR(callees.v[0], "Clamp");
    CHECK_STR(callees.v[1], "Square");
    IN(t, "static int Square(int v) { return v * v; }");
    IN(t, "int Counter::Clamp(int v) { return v > 100 ? 100 : v; }");
    NOT_IN(t, "return v + 1;");
    strip(UNIT, "Twice", true, &kept, &callees);
    REQUIRE(callees.n == 1);
    CHECK_STR(callees.v[0], "Helper");
}

AVP_TEST(namespace_and_extern_c_are_entered)
{
    size_t kept;
    AvpStrVec callees;
    AvpStr t = strip("extern \"C\" {\nint cf(int a) { return a; }\nint cg(int a) { return cf(a) + 1; }\n}\n", "cg",
                     false, &kept, &callees);
    CHECK_EQ_I64(kept, 1);
    IN(t, "int cf(int a);");
    IN(t, "return cf(a) + 1;");
}

AVP_TEST(ctor_initialiser_list_is_not_in_the_declaration)
{
    const char *src = "struct B { B(int); int x; };\nstruct A : B { A(int a); void F(); };\nA::A(int a) : B(a) { x = 1; "
                      "}\nvoid A::F() { x = 2; }\nint g() { return 3; }\n";
    size_t kept;
    AvpStrVec callees;
    AvpStr t = strip(src, "F", false, &kept, &callees);
    CHECK_EQ_I64(kept, 1);
    NOT_IN(t, ": B(a)");
    IN(t, "x = 2;");
    IN(t, "int g();");
    t = strip(src, "A", false, &kept, &callees);
    CHECK_EQ_I64(kept, 1);
    IN(t, ": B(a) { x = 1; }");
}

AVP_TEST(templates_data_and_pragmas_stay)
{
    const char *src = "#pragma push\ntemplate <class T> T Max(T a, T b) { return a > b ? a : b; }\n"
                      "int table[3] = { 1, 2, 3 };\n#pragma dont_inline on\nint other(int a) { return a; }\n#pragma pop\n"
                      "int target(int a) { return Max(a, 1); }\n";
    size_t kept;
    AvpStrVec callees;
    AvpStr t = strip(src, "target", false, &kept, &callees);
    CHECK_EQ_I64(kept, 1);
    IN(t, "return a > b ? a : b;");
    IN(t, "int table[3] = { 1, 2, 3 };");
    IN(t, "#pragma dont_inline on");
    IN(t, "#pragma pop");
    IN(t, "int other(int a);");
}

AVP_TEST(string_with_braces_does_not_confuse_the_scan)
{
    size_t kept;
    AvpStrVec callees;
    AvpStr t = strip("int f() { return puts(\"}{\"); }\nint g() { return 1; }\n", "g", false, &kept, &callees);
    CHECK_EQ_I64(kept, 1);
    IN(t, "int f();");
    IN(t, "int g() { return 1; }");
}

AVP_TEST(operator_definitions)
{
    size_t kept;
    AvpStrVec callees;
    AvpStr t = strip("struct V { int x; V& operator=(const V& o); bool operator==(const V& o) const; };\nV& "
                     "V::operator=(const V& o) { x = o.x; return *this; }\nbool V::operator==(const V& o) const { "
                     "return x == o.x; }\nint z() { return 0; }\n",
                     "operator", false, &kept, &callees);
    CHECK_EQ_I64(kept, 2);
    IN(t, "x = o.x;");
    IN(t, "int z();");
}

AVP_TEST(fn_name_and_decl_of)
{
    AvpArena *a = avp_arena();
    AvpStr q, n;
    size_t p;
    REQUIRE(permute_fn_name(a, AVP_S("int game :: Counter::Twice(int n) const"), &q, &n, &p));
    CHECK_STR(q, "game::Counter::");
    CHECK_STR(n, "Twice");
    CHECK_EQ_I64(p, 26);
    REQUIRE(permute_fn_name(a, AVP_S("T<int>::~T()"), &q, &n, &p));
    CHECK_STR(q, "T<int>::");
    CHECK_STR(n, "~T");
    REQUIRE(permute_fn_name(a, AVP_S("bool V::operator == (const V& o) const"), &q, &n, &p));
    CHECK_STR(n, "operator");
    REQUIRE(permute_fn_name(a, AVP_S("X::operator int*()"), &q, &n, &p));
    CHECK_STR(n, "operator");
    CHECK(!permute_fn_name(a, AVP_S("struct A : B"), &q, &n, &p));
    CHECK_STR(permute_decl_of(a, AVP_S("A::A(int a) : B(a)"), 4), "A::A(int a);");
    CHECK_STR(permute_decl_of(a, AVP_S("int f(int (*g)(void)) const"), 5), "int f(int (*g)(void)) const;");
}

AVP_TEST(strip_c_bodies)
{
    AvpArena *a = avp_arena();
    AvpStr out;
    REQUIRE(permute_strip_other_fns(a, AVP_S("int f(int a)\n{\n  return a;\n}\n#x\nstatic void g(void) { if (1) { } }\n"),
                                    AVP_S("g"), &out));
    CHECK_STR(out, "int f(int a);\n#x\nstatic void g(void) { if (1) { } }\n");
    REQUIRE(permute_strip_other_fns(a, AVP_S("int f(int a) { return a; }\n"), AVP_S("f"), &out));
    CHECK_STR(out, "int f(int a) { return a; }\n");
    CHECK(!permute_strip_other_fns(a, AVP_S("int f(int a) { return a;\n"), AVP_S("f"), &out));
}

AVP_TEST(strip_extern_c)
{
    AvpArena *a = avp_arena();
    AvpStr out;
    REQUIRE(permute_strip_extern_c(a, AVP_S("extern \"C\" {\nint f(void);\nextern \"C\" { int g; }\n}\nextern \"C\" int h;\n"),
                                   &out));
    CHECK_STR(out, "\nint f(void);\n int g; \n\nextern int h;\n");
}

AVP_TEST_MAIN("permute_strip")
