/* Port of tools/test_neutral.py.
 *
 * Python test -> C test
 *   Pure.test_parse_depfile                              -> parse_depfile
 *   Pure.test_remap_argv                                 -> remap_argv
 *   Pure.test_key_argv_is_location_free                  -> key_argv_is_location_free
 *   Pure.test_rel_dep_keeps_case                         -> rel_dep_keeps_case
 *   Pure.test_reach_with_deps                            -> reach_with_deps
 *   Pure.test_reach_fallback_by_include_dirs             -> reach_fallback_by_include_dirs
 *   Pure.test_diff_models                                -> diff_models
 *   RealObject.test_object_model_identical_copy          -> object_model_identical_copy
 *   RealObject.test_local_relocation_targets_are_section_relative -> local_relocation_targets_are_section_relative
 * Added: first_error (cli.first_error, which the port carries), safe_name. */
#include <stdlib.h>

#include "avp/test.h"
#include "neutral.h"

static AvpStrVec strs(const char *const *v, size_t n) {
    AvpStrVec out = {0};
    for (size_t i = 0; i < n; i++) avp_vec_push(&out, avp_str(v[i]));
    return out;
}

AVP_TEST(parse_depfile) {
    AvpArena *a = avp_arena();
    char *tmp = avp_mkdtemp(a, "tmp", NULL, NULL);
    REQUIRE(tmp);
    const char *p = avp_path_join(a, tmp, "x.d");
    const char *t1 = "build\\x.o: src\\game\\X.cpp \\\n\tX:\\w s\\include\\game\\X.h \\\n\tX:\\w s\\include\\new\n";
    REQUIRE(avp_write_file(p, t1, strlen(t1)));
    AvpStrVec got = neutral_parse_depfile(a, p);
    REQUIRE(got.n == 3);
    CHECK(avp_str_eqz(got.v[0], "src\\game\\X.cpp"));
    CHECK(avp_str_eqz(got.v[1], "X:\\w s\\include\\game\\X.h"));
    CHECK(avp_str_eqz(got.v[2], "X:\\w s\\include\\new"));
    /* MWCC's own form: a space in a path is written '\ ' (a checkout path with spaces) */
    const char *t2 = "build\\x.o: src\\game\\X.cpp \\\r\n\tX:\\Game\\ Folder\\include\\game\\X.h \r\n";
    REQUIRE(avp_write_file(p, t2, strlen(t2)));
    got = neutral_parse_depfile(a, p);
    REQUIRE(got.n == 2);
    CHECK(avp_str_eqz(got.v[0], "src\\game\\X.cpp"));
    CHECK(avp_str_eqz(got.v[1], "X:\\Game Folder\\include\\game\\X.h"));
    avp_rmtree(tmp, 0);
}

AVP_TEST(remap_argv) {
    AvpArena *a = avp_arena();
    const char *root = "X:/w/slot", *old = "X:/w/slot/build/neutral/tree-abc";
    const char *v[] = {"mwcc.exe", "-i", "include", "-i", "build/R5WEA4/include", "-i", "include/libjpeg", "-c",
                       "X:/w/slot/src/game/X.cpp", "-o", "out"};
    AvpStrVec argv = strs(v, 11);
    AvpStrVec got = neutral_remap_argv(a, &argv, root, old, "src/game/X.cpp");
    CHECK(avp_str_eqz(got.v[2], avp_path_join(a, old, "include")));
    CHECK(avp_str_eqz(got.v[4], "build/R5WEA4/include")); /* generated, not in git: unchanged */
    CHECK(avp_str_eqz(got.v[6], avp_path_join(a, old, "include/libjpeg")));
    CHECK(avp_str_eqz(got.v[8], avp_path_join(a, old, "src/game/X.cpp")));
    CHECK(avp_str_eqz(argv.v[2], "include")); /* the input is not changed */
}

AVP_TEST(key_argv_is_location_free) {
    AvpArena *a = avp_arena();
    const char *va[] = {"X:/w/slot/build/neutral/tree-1/include", "X:/w/slot/build/neutral/old/u/x.o",
                        "X:/w/slot/tools/compilers/Wii/1.0/mwcceppc.exe"};
    const char *vb[] = {"F:/o/build/neutral/tree-1/include", "F:/o/build/neutral/old/u/x.o",
                        "F:/o/tools/compilers/Wii/1.0/mwcceppc.exe"};
    AvpStrVec xa = strs(va, 3), xb = strs(vb, 3);
    AvpStrVec ka = neutral_key_argv(a, &xa, "X:/w/slot", "X:/w/slot/build/neutral/tree-1", "X:/w/slot/build/neutral/old/u");
    AvpStrVec kb = neutral_key_argv(a, &xb, "F:/o", "F:/o/build/neutral/tree-1", "F:/o/build/neutral/old/u");
    REQUIRE(ka.n == 3 && kb.n == 3);
    for (size_t i = 0; i < 3; i++) CHECK(avp_str_eq(ka.v[i], kb.v[i]));
    CHECK(avp_str_eqz(ka.v[0], "<OLD>/include"));
    CHECK(avp_str_eqz(ka.v[1], "<OUT>/x.o"));
    CHECK(avp_str_eqz(ka.v[2], "<ROOT>/tools/compilers/Wii/1.0/mwcceppc.exe"));
}

AVP_TEST(rel_dep_keeps_case) {
    AvpArena *a = avp_arena();
    CHECK_CZ(neutral_rel_dep(a, "X:/w/slot/build/neutral/tree-1/include/game/Vec3.h", "X:/w/slot",
                             "X:/w/slot/build/neutral/tree-1"),
             "include/game/Vec3.h");
    CHECK_CZ(neutral_rel_dep(a, "X:/w/slot/build/R5WEA4/include/macros.inc", "X:/w/slot", "X:/w/slot/build/neutral/tree-1"),
             "build/R5WEA4/include/macros.inc");
}

static ShsmUnit *mkunit(AvpArena *a, const char *name, const char *n, const char *const *flags, size_t nflags) {
    ShsmUnit *u = AVP_NEW(a, ShsmUnit);
    u->name = name;
    u->path = name + 5;
    u->source = avp_sprintf(a, "src/game/%s.cpp", n);
    u->has_cflags = true;
    u->cflags = strs(flags, nflags);
    u->base_obj = avp_sprintf(a, "X:/w/slot/build/R5WEA4/src/game/%s.o", n);
    return u;
}

static bool has_key(const AvpMap *m, const char *k) { return avp_map_get(m, avp_str(k)) != NULL; }

AVP_TEST(reach_with_deps) {
    AvpArena *a = avp_arena();
    const char *fl[] = {"-i", "include"};
    ShsmUnit *units[2] = {mkunit(a, "main/game/A", "A", fl, 2), mkunit(a, "main/game/B", "B", fl, 2)};
    AvpMap *deps = avp_map_new();
    bool cr = false;
    AvpStrVec *d = AVP_NEW(a, AvpStrVec);
    avp_vec_push(d, AVP_S("src/game/A.cpp"));
    avp_vec_push(d, AVP_S("include/game/V.h"));
    avp_map_put(deps, AVP_S("build/R5WEA4/src/game/A.o"), &cr)->p = d;
    AvpStr f1[] = {AVP_S("include/game/V.h")};
    const char *note = NULL;
    AvpMap *got = neutral_reach(a, units, 2, f1, 1, deps, "X:/w/slot", &note);
    CHECK(avp_map_len(got) == 1 && has_key(got, "main/game/A"));
    CHECK(strstr(note, "ninja") != NULL);
    AvpStr f2[] = {AVP_S("src/game/B.cpp")};
    got = neutral_reach(a, units, 2, f2, 1, deps, "X:/w/slot", &note);
    CHECK(avp_map_len(got) == 1 && has_key(got, "main/game/B"));
}

AVP_TEST(reach_fallback_by_include_dirs) {
    AvpArena *a = avp_arena();
    const char *fa[] = {"-i", "include", "-i", "include/libjpeg"};
    const char *fb[] = {"-i", "include/game_only"};
    ShsmUnit *units[2] = {mkunit(a, "main/jpeg/A", "A", fa, 4), mkunit(a, "main/game/B", "B", fb, 2)};
    AvpStr f[] = {AVP_S("include/libjpeg/jconfig.h")};
    const char *note = NULL;
    AvpMap *got = neutral_reach(a, units, 2, f, 1, NULL, "X:/w/slot", &note);
    CHECK(avp_map_len(got) == 1 && has_key(got, "main/jpeg/A"));
    CHECK(strstr(note, "no ninja dependency records") != NULL);
}

static NeutralReloc rel(i64 off, i64 type, const char *name, i64 value) {
    NeutralReloc r = {off, type, {name, value, false, 0}};
    return r;
}

static void add_fn(AvpArena *a, NeutralModel *m, const char *name, const char *body, const NeutralReloc *rl, size_t n) {
    NeutralFunc *f = AVP_NEW(a, NeutralFunc);
    f->body = avp_str(body);
    for (size_t i = 0; i < n; i++) avp_vec_push(&f->relocs, rl[i]);
    bool cr = false;
    avp_map_put(m->functions, avp_str(name), &cr)->p = f;
}

static void add_data(AvpArena *a, NeutralModel *m, const char *name, i64 size, const char *bytes, const NeutralReloc *rl,
                     size_t n) {
    NeutralData *d = AVP_NEW(a, NeutralData);
    d->size = size;
    d->bytes = avp_str(bytes);
    for (size_t i = 0; i < n; i++) avp_vec_push(&d->relocs, rl[i]);
    bool cr = false;
    avp_map_put(m->data, avp_str(name), &cr)->p = d;
}

AVP_TEST(diff_models) {
    AvpArena *a = avp_arena();
    NeutralReloc rl[1] = {rel(0, 10, "memcpy", 0)};
    NeutralReloc rx[1] = {rel(0, 1, "x", 0)};
    NeutralModel *old = neutral_model_new(a), *new_ = neutral_model_new(a);
    add_fn(a, old, "f", "\x01", rl, 1);
    add_fn(a, old, "g", "\x02", NULL, 0);
    add_fn(a, old, "gone", "", NULL, 0);
    add_data(a, old, ".data", 4, "abcd", NULL, 0);
    add_data(a, old, ".bss", 8, "", NULL, 0);
    add_fn(a, new_, "f", "\x01", rl, 1);
    add_fn(a, new_, "g", "\x03", NULL, 0);
    add_fn(a, new_, "new", "", NULL, 0);
    add_data(a, new_, ".data", 4, "abcd", rx, 1);
    add_data(a, new_, ".bss", 12, "", NULL, 0);
    NeutralChangeVec funcs = {0}, data = {0};
    neutral_diff_models(old, new_, &funcs, &data);
    REQUIRE(funcs.n == 3);
    CHECK_CZ(funcs.v[0].name, "g");
    CHECK_CZ(funcs.v[0].kind, "bytes");
    CHECK_CZ(funcs.v[1].name, "gone");
    CHECK_CZ(funcs.v[1].kind, "removed");
    CHECK_CZ(funcs.v[2].name, "new");
    CHECK_CZ(funcs.v[2].kind, "added");
    REQUIRE(data.n == 2);
    CHECK_CZ(data.v[0].name, ".bss");
    CHECK_CZ(data.v[0].kind, "size");
    CHECK_CZ(data.v[1].name, ".data");
    CHECK_CZ(data.v[1].kind, "relocations");
    NeutralChangeVec f2 = {0}, d2 = {0};
    neutral_diff_models(old, old, &f2, &d2);
    CHECK(f2.n == 0 && d2.n == 0);
}

AVP_TEST(first_error) {
    AvpArena *a = avp_arena();
    const char *out = "### mwcceppc.exe Usage Warning:\n#   The option -multibyte is deprecated\n"
                      "### mwcceppc.exe Compiler:\n#    File: X:\\w\\src\\game\\X.cpp\n# ---------\n"
                      "#      12:   int x = y;\n#   Error:   ^\n#   (10181) undefined identifier 'y'\n";
    AvpStr e = neutral_first_error(a, avp_str(out));
    CHECK(avp_str_eqz(e, "X.cpp:12: (10181) undefined identifier 'y'"));
    CHECK(neutral_first_error(a, AVP_S("plain failure\n")).p == NULL);
}

AVP_TEST(safe_name) {
    AvpArena *a = avp_arena();
    CHECK_CZ(neutral_safe_name(a, "game/Pad Rumble+x.cpp"), "game_Pad_Rumble_x.cpp");
    CHECK_CZ(neutral_safe_name(a, "a\xc3\xa9" "b"), "a_b");
}

AVP_TEST(prepare_dirs_old_exists_for_dotted_name) {
    AvpArena *a = avp_arena();
    const char *base = avp_join(a, avp_path_abspath(a, "build"), "neutral_prepare_test");
    if (avp_isdir(base)) avp_rmtree(base, 0);
    const char *safe = neutral_safe_name(a, "Runtime.PPCEABI.H/__mem");
    CHECK_CZ(safe, "Runtime.PPCEABI.H___mem");
    const char *nd = avp_join(a, base, "new", safe), *od = avp_join(a, base, "old", safe);
    CHECK(avp_makedirs(nd, true));
    neutral_prepare_dirs(nd, od);
    CHECK(!avp_isdir(nd));
    CHECK(avp_isdir(od));
    avp_rmtree(base, 0);
}

/* ---- RealObject: a compiled object of the checkout's build ------------------------------------------------------- */

static const char *real_object(AvpArena *a) {
    if (getenv("SHSM_NO_REAL_TREE") && *getenv("SHSM_NO_REAL_TREE")) return NULL;
    const char *p = avp_path_abspath(a, "build/R5WEA4/src/game/PadRumble.o");
    return avp_isfile(p) ? p : NULL;
}

AVP_TEST(object_model_identical_copy) {
    AvpArena *a = avp_arena();
    const char *obj = real_object(a);
    if (!obj) {
        printf("skipped: no build/R5WEA4/src/game/PadRumble.o\n");
        return;
    }
    NeutralModel *m = neutral_object_model(a, obj);
    CHECK(avp_map_len(m->functions) > 0);
    char *tmp = avp_mkdtemp(a, "tmp", NULL, NULL);
    REQUIRE(tmp);
    const char *cp = avp_path_join(a, tmp, "x.o");
    REQUIRE(avp_copy2(obj, cp));
    NeutralChangeVec f = {0}, d = {0};
    neutral_diff_models(m, neutral_object_model(a, cp), &f, &d);
    CHECK(f.n == 0 && d.n == 0);
    avp_rmtree(tmp, 0);
}

AVP_TEST(local_relocation_targets_are_section_relative) {
    AvpArena *a = avp_arena();
    const char *obj = real_object(a);
    if (!obj) {
        printf("skipped: no build/R5WEA4/src/game/PadRumble.o\n");
        return;
    }
    NeutralModel *m = neutral_object_model(a, obj);
    AvpMapIter it = {0};
    size_t n = 0;
    while (avp_map_next(m->functions, &it)) {
        const NeutralFunc *f = it.val->p;
        for (size_t i = 0; i < f->relocs.n; i++, n++) CHECK(f->relocs.v[i].target.name[0] != '@');
    }
    CHECK(n > 0);
}

AVP_TEST_MAIN("neutral")
