/* unit_build_source and unit_decls: the stubs `new` writes for functions the mangler qualifies with one name. A name
 * the headers declare as a namespace puts the stub in a `namespace X { }` block; a class some header defines gets a
 * member definition; any other name keeps the "signature not expanded" comment form. */
#include "avp/test.h"
#include "unit_stub.h"
#include "../../../tests/shsm_test_util.h" /* fx_write */

#include <string.h>

static AvpArena *A(void) { return avp_arena(); }

static UnitSym sym(const char *name, i64 addr) {
    return (UnitSym){name, ".text", "function", "global", "", addr, 0x10};
}

static const char *build(const char *path, const UnitSym *syms, size_t n, const UnitDecls *decls) {
    AvpArena *a = A();
    UnitSymPtrVec funcs = {0};
    for (size_t i = 0; i < n; i++) avp_vec_push(&funcs, &syms[i]);
    AvpStrVec note = {0};
    const char *header = NULL;
    return unit_build_source(a, path, &funcs, true, NULL, &note, NULL, NULL, NULL, decls, &header);
}

static bool has(const char *text, const char *needle) { return strstr(text, needle) != NULL; }

AVP_TEST(decls_read_namespaces_and_defined_classes) {
    const AvpStr headers[] = {
        AVP_S("// namespace Commented { }\n/* class Hidden { */\nnamespace FMOD {\nclass Plain;\nclass SystemI : public Base {\n};\n}\n"),
        AVP_S("#define X class Macro {\nstruct Pod\n{\n int a;\n};\nenum class Mode { A };\nclass Holder { friend class Friend; };\n"
              "template <class T> struct Box final : Base {};\nnamespace alias = FMOD;\nnamespace {\n}\n"),
    };
    UnitDecls *d = unit_decls_from_texts(A(), headers, 2);
    CHECK(unit_decls_has_namespace(d, "FMOD"));
    CHECK(!unit_decls_has_namespace(d, "Commented"));
    CHECK(!unit_decls_has_namespace(d, "alias"));
    CHECK(unit_decls_has_class(d, "SystemI"));
    CHECK(unit_decls_has_class(d, "Pod"));
    CHECK(unit_decls_has_class(d, "Holder"));
    CHECK(unit_decls_has_class(d, "Box"));
    CHECK(!unit_decls_has_class(d, "Plain"));  /* a forward declaration */
    CHECK(!unit_decls_has_class(d, "Hidden")); /* in a comment */
    CHECK(!unit_decls_has_class(d, "Macro"));  /* in a preprocessor line */
    CHECK(!unit_decls_has_class(d, "Mode"));   /* enum class */
    CHECK(!unit_decls_has_class(d, "Friend"));
    CHECK(!unit_decls_has_namespace(NULL, "FMOD"));
}

AVP_TEST(namespace_function_goes_in_a_namespace_block) {
    const AvpStr headers[] = {AVP_S("namespace FMOD {\nvoid init_mparams();\n}\n")};
    UnitDecls *d = unit_decls_from_texts(A(), headers, 1);
    UnitSym syms[] = {sym("init_mparams__4FMODFv", 0x80377680), sym("next_one__4FMODFi", 0x80377700)};
    const char *src = build("fmod/dlmalloc.cpp", syms, 2, d);
    CHECK(has(src, "namespace FMOD {\n\nvoid init_mparams() {\n    // TODO\n} // 0x80377680\n\n"
                   "void next_one( int ) {\n    // TODO\n} // 0x80377700\n\n} // namespace FMOD\n"));
    CHECK(!has(src, "FMOD::init_mparams() {"));
    /* one block for the two consecutive functions */
    const char *first = strstr(src, "namespace FMOD {");
    CHECK(first != NULL && strstr(first + 1, "namespace FMOD {") == NULL);
}

AVP_TEST(declared_class_keeps_its_member_definition) {
    const AvpStr headers[] = {AVP_S("class Pool {\n public:\n  void Reset();\n};\n")};
    UnitDecls *d = unit_decls_from_texts(A(), headers, 1);
    UnitSym syms[] = {sym("Reset__4PoolFv", 0x80000100)};
    const char *src = build("misc/pool.cpp", syms, 1, d);
    CHECK(has(src, "void Pool::Reset() {\n    // TODO\n} // 0x80000100\n"));
    CHECK(!has(src, "namespace"));
}

AVP_TEST(undeclared_class_keeps_the_comment_form) {
    UnitSym syms[] = {sym("Reset__4PoolFv", 0x80000100), sym("__ct__4PoolFv", 0x80000110)};
    const char *src = build("misc/pool.cpp", syms, 2, NULL);
    CHECK(has(src, "// TODO: signature not expanded or needs a complete type: Reset__4PoolFv\n// void Pool::Reset() { } // 0x80000100\n"));
    CHECK(has(src, "// TODO: signature not expanded or needs a complete type: __ct__4PoolFv\n"));
    CHECK(!has(src, "\nvoid Pool::Reset() {"));
    /* a namespace is never a constructor's class: no namespace block for it either */
    const AvpStr headers[] = {AVP_S("namespace Pool {\n}\n")};
    UnitSym ct[] = {sym("__ct__4PoolFv", 0x80000110)};
    src = build("misc/pool.cpp", ct, 1, unit_decls_from_texts(A(), headers, 1));
    CHECK(has(src, "signature not expanded"));
    CHECK(!has(src, "namespace Pool {"));
}

AVP_TEST(a_non_namespace_function_between_blocks_closes_the_block) {
    const AvpStr headers[] = {AVP_S("namespace FMOD { }\nnamespace Other { }\n")};
    UnitDecls *d = unit_decls_from_texts(A(), headers, 1);
    UnitSym syms[] = {sym("a__4FMODFv", 0x80000100), sym("free_fn__Fv", 0x80000110), sym("b__5OtherFv", 0x80000120)};
    const char *src = build("fmod/x.cpp", syms, 3, d);
    const char *a_at = strstr(src, "namespace FMOD {"), *close = strstr(src, "} // namespace FMOD"),
               *free_at = strstr(src, "void free_fn() {"), *other = strstr(src, "namespace Other {");
    REQUIRE(a_at && close && free_at && other);
    CHECK(a_at < close && close < free_at && free_at < other);
    CHECK(strstr(src, "} // namespace Other") != NULL);
}

/* The header that defines a class, as the path an #include takes; the smallest path when two define it. */
AVP_TEST(decls_scan_names_the_header_of_a_class) {
    AvpArena *a = A();
    const char *dir = avp_path_abspath(a, avp_join(a, "build", "tmp", "cpilot", "B", "unit decls"));
    if (avp_isdir(dir)) AVP_ASSERT(avp_rmtree(dir, AVP_RMTREE_CLEAR_READONLY));
    fx_write(dir, "fmod/fmod_channel_real.h", "namespace FMOD {\nclass ChannelReal {\n};\n}\n");
    fx_write(dir, "fmod/b.hpp", "class ChannelReal {};\nclass Twice { int x; };\n");
    fx_write(dir, "fmod/a.h", "class Twice {};\nclass Fwd;\n");
    UnitDecls *d = unit_decls_scan(a, dir);
    CHECK_CZ(unit_decls_class_header(d, "ChannelReal"), "fmod/b.hpp");
    CHECK_CZ(unit_decls_class_header(d, "Twice"), "fmod/a.h");
    CHECK(unit_decls_class_header(d, "Fwd") == NULL);
    CHECK(unit_decls_class_header(NULL, "Twice") == NULL);
    UnitDecls *t = unit_decls_from_texts(a, (AvpStr[]){avp_str("class T {};")}, 1);
    CHECK(unit_decls_has_class(t, "T") && unit_decls_class_header(t, "T") == NULL); /* bare texts name no header */
    AVP_ASSERT(avp_rmtree(dir, AVP_RMTREE_CLEAR_READONLY));
}

AVP_TEST_MAIN("unit")
