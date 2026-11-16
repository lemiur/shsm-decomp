/* check_rules: the stage-3 modules: the class model and classes.json helpers (rules_classes.h), the R13 slot check,
 * the cross-file rules, RULE-EXCEPTION handling, the allow list and the function records (Checker.run). The
 * whole-run proof is the differential harness (diff/dump_final.py). */
#include "avp/test.h"
#include "rules_check.h"
#include "rules_classes.h"

#include "classes.h"

/* Checker.run over in-memory sources (paths[0] scanned), unit status Matching for paths[0]; classes: JSON text or
 * NULL. */
static RlChecker *run3(const char *const *paths, const char *const *texts, size_t n, const char *classes,
                       const char *rulings) {
    AvpArena *a = avp_arena();
    AvpStrVec P = {0}, T = {0};
    for (size_t i = 0; i < n; i++) {
        avp_vec_push(&P, avp_str(paths[i]));
        avp_vec_push(&T, avp_str(texts[i]));
    }
    RlConfig cfg = {0};
    cfg.unit_status = avp_map_new();
    AvpStr *v = AVP_NEW(a, AvpStr);
    *v = AVP_S("Matching");
    avp_map_put(cfg.unit_status, avp_str(paths[0] + 4), NULL)->p = v;
    if (rulings) rl_allow_from_markdown(a, &cfg, avp_str(rulings), AVP_S("docs/exceptions.md"));
    RlChecker *ck = rl_checker_from(a, &P, &T, &cfg);
    avp_vec_push(&ck->scan, avp_str(paths[0]));
    if (classes) {
        AvpJsonError err;
        ck->classes_data = avp_json_parse(a, avp_str(classes), &err);
    }
    rl_checker_run(ck);
    return ck;
}

static int count(RlChecker *ck, const char *rule, const char *tier, const char *status, const char *needle) {
    int n = 0;
    for (size_t i = 0; i < ck->findings.n; i++) {
        const RlFinding *f = &ck->findings.v[i];
        if (strcmp(f->rule, rule) || (tier && strcmp(f->tier, tier)) || (status && strcmp(f->status, status)))
            continue;
        if (needle && !avp_str_contains(f->msg, avp_str(needle))) continue;
        n++;
    }
    return n;
}

AVP_TEST(demangle) {
    AvpArena *a = avp_arena();
    AvpStr cls, name;
    rl_demangle_member(a, AVP_S("fn_8009CB1C__14CBaseBehaviourFR7unk_Msg"), &cls, &name);
    CHECK_STR(cls, "CBaseBehaviour");
    CHECK_STR(name, "fn_8009CB1C");
    rl_demangle_member(a, AVP_S("__dt__Q23RWS6CThingFv"), &cls, &name);
    CHECK_STR(cls, "RWS::CThing");
    CHECK_STR(name, "~");
    rl_demangle_member(a, AVP_S("@36@fn_1__4BaseFv"), &cls, &name);
    CHECK_STR(cls, "Base");
    rl_demangle_member(a, AVP_S("fn_80001234"), &cls, &name);
    CHECK(cls.p == NULL);
    CHECK_STR(name, "fn_80001234");
    rl_demangle_member(a, (AvpStr){0}, &cls, &name);
    CHECK(cls.p == NULL && name.p == NULL);
    CHECK_STR(rl_mangled_params(a, AVP_S("draw__Q310homebutton3gui9InterfaceFv")), "v");
    CHECK_STR(rl_mangled_params(a, AVP_S("get__4BaseCFi")), "i");
    CHECK(rl_mangled_params(a, AVP_S("fn_80001234")).p == NULL);
}

AVP_TEST(template_name) {
    CHECK_STR(rl_template_name(AVP_S("Foo<int>")), "Foo");
    CHECK_STR(rl_template_name(AVP_S("ut::List<T<a>>")), "ut::List");
    CHECK_STR(rl_template_name(AVP_S("Foo <int>")), "Foo ");
    CHECK(rl_template_name(AVP_S("Foo")).p == NULL);
    CHECK(rl_template_name(AVP_S("<int>")).p == NULL);
}

AVP_TEST(cpp_classes) {
    RlParsed *P = rl_parse(avp_arena(), AVP_S("src/t.cpp"),
                           AVP_S("namespace N { class A : public virtual B, C<int> {\npublic:\n"
                                 "  virtual ~A();\n  virtual void f(int x, const char* s = 0) const = 0;\n"
                                 "  static void g();\n  A();\n  int v;\n  struct In { void h(); };\n}; }\n"),
                           NULL);
    const RlSrcClasses *cs = rl_cpp_classes(P);
    REQUIRE(cs->n == 2);
    const RlSrcClass *c = &cs->v[0];
    CHECK_STR(c->name, "N::A");
    CHECK(c->virtual_bases);
    REQUIRE(c->bases.n == 2);
    CHECK_STR(c->bases.v[1], "C<int>");
    REQUIRE(c->methods.n == 4);
    CHECK_STR(c->methods.v[0].name, "~");
    CHECK(c->methods.v[0].virtual_);
    const RlSrcMethod *f = &c->methods.v[1];
    CHECK(f->pure && f->sig.const_ && f->sig.np == 2);
    CHECK_STR(f->sig.params[0], "int");
    CHECK_STR(f->sig.params[1], "const char *");
    CHECK(c->methods.v[2].static_);
    CHECK(c->methods.v[3].ctor);
    CHECK_STR(cs->v[1].name, "N::A::In");
}

static const char *const BASE =
    "class Base {\npublic:\n    virtual ~Base();\n    virtual void fn_A();\n    int x;\n};\n"
    "class Derived : public Base {\npublic:\n    virtual void fn_B();\n};\n";

static const char *vt_doc(const char *third) {
    return avp_sprintf(
        avp_arena(),
        "{\"classes\": ["
        "{\"name\": \"Base\", \"vtable\": {\"symbol\": \"lbl_80400000\", \"address\": \"0x80400000\", "
        "\"group_evidence\": \"stores\", \"words\": [{\"index\": 0, \"offset\": \"0x0\", \"status\": \"header\"}, "
        "{\"index\": 1, \"offset\": \"0x4\", \"status\": \"header\"}, "
        "{\"index\": 2, \"offset\": \"0x8\", \"status\": \"new\", \"target\": \"__dt__4BaseFv\", \"group\": \"complete\"}, "
        "{\"index\": 3, \"offset\": \"0xc\", \"status\": \"new\", \"target\": \"fn_A__4BaseFv\", \"group\": \"complete\"}]}, "
        "\"ctors\": [], \"dtors\": [], \"factories\": [], \"members\": []},"
        "{\"name\": \"Derived\", \"vtable\": {\"symbol\": \"lbl_80400100\", \"address\": \"0x80400100\", "
        "\"group_evidence\": \"stores\", \"words\": [{\"index\": 0, \"offset\": \"0x0\", \"status\": \"header\"}, "
        "{\"index\": 1, \"offset\": \"0x4\", \"status\": \"header\"}, "
        "{\"index\": 2, \"offset\": \"0x8\", \"status\": \"override\", \"target\": \"__dt__7DerivedFv\", \"group\": \"complete\"}, "
        "{\"index\": 3, \"offset\": \"0xc\", \"status\": \"inherited\", \"target\": \"fn_A__4BaseFv\", \"group\": \"complete\"}%s]}, "
        "\"ctors\": [], \"dtors\": [], \"factories\": [], \"members\": []}]}",
        third);
}

AVP_TEST(r13_slots) {
    const char *paths[] = {"src/t.cpp"}, *texts[] = {BASE};
    /* the new virtual has its slot: clean */
    RlChecker *ck = run3(paths, texts, 1,
                         vt_doc(", {\"index\": 4, \"offset\": \"0x10\", \"status\": \"new\", \"target\": "
                                "\"fn_B__7DerivedFv\", \"group\": \"complete\"}"),
                         NULL);
    CHECK(!ck->error);
    CHECK_EQ_I64(count(ck, "R13", NULL, NULL, NULL), 0); /* the implicit destructor overrides the primary slot */
    CHECK_EQ_I64(ck->r13.matched.n, 2);
    /* no slot for it: an added virtual, blocking */
    ck = run3(paths, texts, 1, vt_doc(""), NULL);
    CHECK_EQ_I64(count(ck, "R13", "match", NULL, "new virtual fn_B"), 1);
    CHECK_EQ_I64(ck->r13.added, 1);
    CHECK_EQ_I64(rl_exit_code(ck), 1);
    /* no classes.json: the note */
    ck = run3(paths, texts, 1, NULL, NULL);
    CHECK_STR(ck->r13.note, "R13 slot checks skipped: no classes.json given");
}

/* A same-named virtual with another parameter count hides the base's, it does not override it: Derived::fn_A(bool)
 * is a new slot after the inherited fn_A() (FMOD::DSPI::release(bool) after Plugin::release()). */
AVP_TEST(r13_hidden_overload_is_a_new_slot) {
    static const char *const SRC =
        "class Base {\npublic:\n    virtual ~Base();\n    virtual void fn_A();\n    int x;\n};\n"
        "class Derived : public Base {\npublic:\n    virtual void fn_A( bool b );\n};\n";
    const char *paths[] = {"src/t.cpp"}, *texts[] = {SRC};
    RlChecker *ck = run3(paths, texts, 1,
                         vt_doc(", {\"index\": 4, \"offset\": \"0x10\", \"status\": \"new\", \"target\": "
                                "\"fn_A__7DerivedFb\", \"group\": \"complete\"}"),
                         NULL);
    CHECK(!ck->error);
    CHECK_EQ_I64(count(ck, "R13", NULL, NULL, NULL), 0);
}

AVP_TEST(load_classes_notes) {
    AvpArena *a = avp_arena();
    AvpStr note;
    CHECK(rl_load_classes(a, "build/tmp/no/such/classes.json", &note) == NULL);
    CHECK(avp_str_contains(note, AVP_S("not found (run build/tools/classes.exe scan)")));
}

/* classes.json at dir/classes.json with the given inputs (NULL: none) and scanner value */
static const char *write_classes(const char *dir, AvpJson *inputs, const char *scanner) {
    AvpArena *a = avp_arena();
    AvpJson *db = avp_json_obj(a);
    if (inputs) {
        avp_json_set(a, inputs, AVP_S("scanner"), avp_json_strz(a, scanner));
        avp_json_set(a, db, AVP_S("inputs"), inputs);
    }
    avp_json_set(a, db, AVP_S("classes"), avp_json_arr(a));
    const char *path = avp_path_join(a, dir, "classes.json");
    AvpStr text = avp_json_dumps(a, db, NULL);
    AVP_ASSERT(avp_write_file(path, text.p, text.n));
    return path;
}

AVP_TEST(load_classes_stamp_by_c_reader) {
    /* the input stamp decides (classes_staleness), never the scanner value: a file from classes.exe under either
     * base rule, or from tools/classes.py, is current while its inputs are unchanged */
    AvpArena *a = avp_arena();
    char *dir = avp_mkdtemp(a, "rules_stamp", NULL, NULL);
    AVP_ASSERT(dir != NULL);
    char *asm_dir = avp_path_join(a, dir, "asm"), *sym = avp_path_join(a, dir, "symbols.txt");
    AVP_ASSERT(avp_makedirs(asm_dir, true));
    AVP_ASSERT(avp_write_file(avp_path_join(a, asm_dir, "a.s"), ".fn a\n", 6));
    AVP_ASSERT(avp_write_file(sym, "a = .text:0x80000000;\n", 22));
    AvpStr note;
    static const char *const SCANNERS[] = {"0123abcd+base-rule=ctor", "0123abcd", "python-digest"};
    for (size_t i = 0; i < 3; i++) {
        const char *p = write_classes(dir, classes_input_stamp(a, asm_dir, sym), SCANNERS[i]);
        CHECK_MSG(rl_load_classes(a, p, &note) != NULL, "%s: %.*s", SCANNERS[i], AVP_SF(note));
        CHECK(note.n == 0);
    }
    const char *p = write_classes(dir, classes_input_stamp(a, asm_dir, sym), SCANNERS[0]);
    AVP_ASSERT(avp_write_file(sym, "b = .text:0x80000000;\n", 22)); /* same size, mtime may not move: */
    AvpStat st;
    AVP_ASSERT(avp_stat(sym, &st));
    AVP_ASSERT(avp_utime_ns(sym, st.atime_ns, st.mtime_ns + 2000000000));
    CHECK(rl_load_classes(a, p, &note) == NULL);
    CHECK(avp_str_contains(note, AVP_S("is stale: the symbols file changed since the scan")));
    AVP_ASSERT(avp_write_file(avp_path_join(a, asm_dir, "b.s"), "\n", 1));
    CHECK(rl_load_classes(a, p, &note) == NULL);
    CHECK(avp_str_contains(note, AVP_S("is stale: the asm tree and the symbols file changed since the scan")));
    p = write_classes(dir, NULL, NULL);
    CHECK(rl_load_classes(a, p, &note) == NULL);
    CHECK(avp_str_contains(note, AVP_S("it records no input stamp")));
    p = write_classes(dir, classes_input_stamp(a, avp_path_join(a, dir, "gone"), sym), SCANNERS[0]);
    CHECK(rl_load_classes(a, p, &note) == NULL);
    CHECK(avp_str_contains(note, AVP_S("are gone")));
    avp_tempdir_cleanup(dir);
}

AVP_TEST(cross_file) {
    const char *paths[] = {"src/a.c", "src/b.c", "include/x.h", "include/y.h"};
    const char *texts[] = {"#include \"x.h\"\n#include \"y.h\"\n", "int g;\n",
                           "struct S { int a; int b; };\nextern int g;\n", "struct S { int a; int b; };\nextern char g;\n"};
    RlChecker *ck = run3(paths, texts, 4, NULL, NULL);
    CHECK_EQ_I64(count(ck, "R1", "match", NULL, "type 'S' is defined in more than one file (an identical copy"), 2);
    CHECK_EQ_I64(count(ck, "R2", "match", NULL, "global 'g' is declared with different types (char vs int)"), 2);
}

AVP_TEST(exceptions_and_allow) {
    const char *paths[] = {"src/t.c"};
    const char *texts[] = {"/* f */\nvoid f(void) {\n    goto out; // RULE-EXCEPTION: R14, the retail branch order needs it\nout:;\n}\n"
                           "/* g */\nvoid g(void) {\n    goto out; // RULE-EXCEPTION: R99, x\nout:;\n}\n"};
    RlChecker *ck = run3(paths, texts, 1, NULL, NULL);
    REQUIRE(ck->exceptions.n == 2);
    CHECK(ck->exceptions.v[0].valid);
    CHECK_EQ_I64(ck->exceptions.v[0].suppressed, 1);
    CHECK_STR(ck->exceptions.v[1].problem, "unknown rule R99");
    CHECK_EQ_I64(count(ck, "R14", NULL, "excepted", NULL), 1);
    CHECK_EQ_I64(count(ck, "R0", "free", NULL, "unknown rule R99"), 1);
    /* a review ruling allows g's goto */
    ck = run3(paths, texts, 1, NULL, "## Review rulings\n\n| function | rule |\n|---|---|\n| `g` | R14 |\n");
    CHECK_EQ_I64(count(ck, "R14", NULL, "allowed", NULL), 1);
    REQUIRE(ck->functions.n == 2);
    CHECK_STR(ck->functions.v[0].name, "f");
    CHECK(!strcmp(ck->functions.v[0].status, "ok"));
    REQUIRE(ck->findings.v[0].nunits == 1);
    CHECK_STR(ck->findings.v[0].units[0], "src/t.c");
}

AVP_TEST_MAIN("check_rules_run")
