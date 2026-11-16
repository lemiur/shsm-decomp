/* project: the value model (pv.h) and the classes of pv_project.c. The pathlib tables were produced by CPython 3.14.3
 * (`python -c "from pathlib import PureWindowsPath as P; ..."`: str(P(s)), .parent, .name, .suffix, .stem,
 * .as_posix(), .is_absolute(), repr(Path(s)), P(a) / b, .with_suffix(x)) and are hard-coded here. */
#include <string.h>

#include "avp/test.h"
#include "project.h"

typedef struct {
    const char *in, *norm, *parent, *name, *suffix, *stem, *posix;
    int absolute;
    const char *repr;
} PathCase;
typedef struct {
    const char *base, *other, *joined;
} JoinCase;
typedef struct {
    const char *path, *suffix, *result;
} SuffixCase;

/* Generated from CPython 3.14.3 pathlib.PureWindowsPath (str, parent, name, suffix, stem, as_posix, is_absolute,
 * repr of WindowsPath; a / b; with_suffix, NULL where it raises ValueError). */
static const PathCase k_paths[] = {
    {"", ".", ".", "", "", "", ".", 0, "WindowsPath('.')"},
    {".", ".", ".", "", "", "", ".", 0, "WindowsPath('.')"},
    {"..", "..", ".", "..", "", "..", "..", 0, "WindowsPath('..')"},
    {"a", "a", ".", "a", "", "a", "a", 0, "WindowsPath('a')"},
    {"a/b", "a\\b", "a", "b", "", "b", "a/b", 0, "WindowsPath('a/b')"},
    {"a\\b\\", "a\\b", "a", "b", "", "b", "a/b", 0, "WindowsPath('a/b')"},
    {"./a/./b/", "a\\b", "a", "b", "", "b", "a/b", 0, "WindowsPath('a/b')"},
    {"a//b", "a\\b", "a", "b", "", "b", "a/b", 0, "WindowsPath('a/b')"},
    {"/a", "\\a", "\\", "a", "", "a", "/a", 0, "WindowsPath('/a')"},
    {"\\a\\b", "\\a\\b", "\\a", "b", "", "b", "/a/b", 0, "WindowsPath('/a/b')"},
    {"C:", "C:", "C:", "", "", "", "C:", 0, "WindowsPath('C:')"},
    {"C:a", "C:a", "C:", "a", "", "a", "C:a", 0, "WindowsPath('C:a')"},
    {"c:/a/b", "c:\\a\\b", "c:\\a", "b", "", "b", "c:/a/b", 1, "WindowsPath('c:/a/b')"},
    {"C:\\", "C:\\", "C:\\", "", "", "", "C:/", 1, "WindowsPath('C:/')"},
    {"C:/", "C:\\", "C:\\", "", "", "", "C:/", 1, "WindowsPath('C:/')"},
    {"//server/share", "\\\\server\\share\\", "\\\\server\\share\\", "", "", "", "//server/share/", 1, "WindowsPath('//server/share/')"},
    {"//server/share/x/y", "\\\\server\\share\\x\\y", "\\\\server\\share\\x", "y", "", "y", "//server/share/x/y", 1, "WindowsPath('//server/share/x/y')"},
    {"\\\\server\\share\\", "\\\\server\\share\\", "\\\\server\\share\\", "", "", "", "//server/share/", 1, "WindowsPath('//server/share/')"},
    {"//server", "\\\\server", "\\\\server", "", "", "", "//server", 1, "WindowsPath('//server')"},
    {"//?/C:/x", "\\\\?\\C:\\x", "\\\\?\\C:\\", "x", "", "x", "//?/C:/x", 1, "WindowsPath('//?/C:/x')"},
    {"//?/UNC/srv/sh/x", "\\\\?\\UNC\\srv\\sh\\x", "\\\\?\\UNC\\srv\\sh\\", "x", "", "x", "//?/UNC/srv/sh/x", 1, "WindowsPath('//?/UNC/srv/sh/x')"},
    {"//./dev/x", "\\\\.\\dev\\x", "\\\\.\\dev\\", "x", "", "x", "//./dev/x", 1, "WindowsPath('//./dev/x')"},
    {"///x", "\\\\\\x", "\\\\\\x", "", "", "", "///x", 1, "WindowsPath('///x')"},
    {"a/c:b", "a\\c:b", "a", "c:b", "", "c:b", "a/c:b", 0, "WindowsPath('a/c:b')"},
    {"./c:b", ".\\c:b", ".", "c:b", "", "c:b", "./c:b", 0, "WindowsPath('./c:b')"},
    {"..\\..\\x", "..\\..\\x", "..\\..", "x", "", "x", "../../x", 0, "WindowsPath('../../x')"},
    {"build/R5WEA4/src/a.o", "build\\R5WEA4\\src\\a.o", "build\\R5WEA4\\src", "a.o", ".o", "a", "build/R5WEA4/src/a.o", 0, "WindowsPath('build/R5WEA4/src/a.o')"},
    {"x/.hidden", "x\\.hidden", "x", ".hidden", "", ".hidden", "x/.hidden", 0, "WindowsPath('x/.hidden')"},
    {"x/a.", "x\\a.", "x", "a.", ".", "a", "x/a.", 0, "WindowsPath('x/a.')"},
    {"x/..a", "x\\..a", "x", "..a", "", "..a", "x/..a", 0, "WindowsPath('x/..a')"},
    {"x/a.b.c", "x\\a.b.c", "x", "a.b.c", ".c", "a.b", "x/a.b.c", 0, "WindowsPath('x/a.b.c')"},
    {"x/...", "x\\...", "x", "...", "", "...", "x/...", 0, "WindowsPath('x/...')"},
    {"x/a..b", "x\\a..b", "x", "a..b", ".b", "a.", "x/a..b", 0, "WindowsPath('x/a..b')"},
    {"tools/bin/dtk.exe", "tools\\bin\\dtk.exe", "tools\\bin", "dtk.exe", ".exe", "dtk", "tools/bin/dtk.exe", 0, "WindowsPath('tools/bin/dtk.exe')"},
    {"X:/Game Project Checkout Dir/configure.py", "X:\\Game Project Checkout Dir\\configure.py", "X:\\Game Project Checkout Dir", "configure.py", ".py", "configure", "X:/Game Project Checkout Dir/configure.py", 1, "WindowsPath('X:/Game Project Checkout Dir/configure.py')"},
    {"D:x/y", "D:x\\y", "D:x", "y", "", "y", "D:x/y", 0, "WindowsPath('D:x/y')"},
    {"x/.", "x", ".", "x", "", "x", "x", 0, "WindowsPath('x')"},
    {"x/..", "x\\..", "x", "..", "", "..", "x/..", 0, "WindowsPath('x/..')"},
    {".a", ".a", ".", ".a", "", ".a", ".a", 0, "WindowsPath('.a')"},
    {"//?/UNC/srv", "\\\\?\\UNC\\srv", "\\\\?\\UNC\\srv", "", "", "", "//?/UNC/srv", 1, "WindowsPath('//?/UNC/srv')"},
    {"\\\\?\\unc\\srv\\sh", "\\\\?\\unc\\srv\\sh\\", "\\\\?\\unc\\srv\\sh\\", "", "", "", "//?/unc/srv/sh/", 1, "WindowsPath('//?/unc/srv/sh/')"},
    {"//srv/sh/", "\\\\srv\\sh\\", "\\\\srv\\sh\\", "", "", "", "//srv/sh/", 1, "WindowsPath('//srv/sh/')"},
    {"a/b/", "a\\b", "a", "b", "", "b", "a/b", 0, "WindowsPath('a/b')"},
};
static const JoinCase k_joins[] = {
    {"a", "b", "a\\b"},
    {"a", "/b", "\\b"},
    {"C:\\a", "b", "C:\\a\\b"},
    {"C:\\a", "D:b", "D:b"},
    {"C:\\a", "c:b", "c:\\a\\b"},
    {"C:\\a", "\\b", "C:\\b"},
    {"C:a", "\\b", "C:\\b"},
    {"\\\\srv\\sh\\", "x", "\\\\srv\\sh\\x"},
    {"\\\\srv\\sh\\x", "D:\\y", "D:\\y"},
    {".", "x", "x"},
    {".", "c:x", "c:x"},
    {"a", ".", "a"},
    {"a", "", "a"},
    {"build", "R5WEA4/src/x.o", "build\\R5WEA4\\src\\x.o"},
    {"a\\b", "..\\c", "a\\b\\..\\c"},
    {"C:", "x", "C:x"},
    {"a", "//srv/sh", "\\\\srv\\sh\\"},
    {"a", "C:", "C:"},
    {"\\\\srv\\sh\\", "x", "\\\\srv\\sh\\x"},
    {"C:\\a", "C:", "C:\\a"},
    {"C:\\a", "D:", "D:"},
};
static const SuffixCase k_suffixes[] = {
    {"a\\b.c", ".o", "a\\b.o"},
    {"a\\b", ".s", "a\\b.s"},
    {"a\\b.c", "", "a\\b"},
    {"a\\b.tar.gz", ".x", "a\\b.tar.x"},
    {"x\\.hidden", ".o", "x\\.hidden.o"},
    {"x\\a.", ".o", "x\\a.o"},
    {"x\\...", ".o", "x\\....o"},
    {"a\\b.c", ".ctx", "a\\b.ctx"},
    {".", ".o", NULL},
    {"a", "o", NULL},
    {"C:\\", ".o", NULL},
    {"x\\..", ".o", "x\\...o"},
};

#define COUNT(x) (sizeof(x) / sizeof((x)[0]))

AVP_TEST(path_parts_match_pathlib) {
    AvpArena *a = avp_arena();
    for (size_t i = 0; i < COUNT(k_paths); i++) {
        const PathCase *c = &k_paths[i];
        AvpStr n = pv_path_norm(a, avp_str(c->in));
        CHECK_MSG(avp_str_eqz(n, c->norm), "norm(%s): %.*s", c->in, AVP_SF(n));
        /* the accessors take the str() form, and give the same on the raw input */
        CHECK_MSG(avp_str_eqz(pv_path_parent(a, n), c->parent), "parent(%s)", c->in);
        CHECK_MSG(avp_str_eqz(pv_path_parent(a, avp_str(c->in)), c->parent), "parent raw(%s)", c->in);
        CHECK_MSG(avp_str_eqz(pv_path_name(n), c->name), "name(%s)", c->in);
        CHECK_MSG(avp_str_eqz(pv_path_suffix(n), c->suffix), "suffix(%s)", c->in);
        CHECK_MSG(avp_str_eqz(pv_path_stem(n), c->stem), "stem(%s)", c->in);
        CHECK_MSG(avp_str_eqz(pv_path_as_posix(a, n), c->posix), "as_posix(%s)", c->in);
        CHECK_MSG(pv_path_is_absolute(n) == (c->absolute != 0), "is_absolute(%s)", c->in);
        Pv *p = pv_path(a, avp_str(c->in));
        CHECK_MSG(avp_str_eqz(pv_repr(a, p), c->repr), "repr(%s)", c->in);
        CHECK_MSG(avp_str_eqz(pv_to_str(a, p), c->norm), "str(%s)", c->in);
    }
}

AVP_TEST(path_join_matches_pathlib) {
    AvpArena *a = avp_arena();
    for (size_t i = 0; i < COUNT(k_joins); i++) {
        const JoinCase *c = &k_joins[i];
        AvpStr j = pv_path_join(a, avp_str(c->base), avp_str(c->other));
        CHECK_MSG(avp_str_eqz(j, c->joined), "%s / %s: %.*s", c->base, c->other, AVP_SF(j));
    }
}

AVP_TEST(path_with_suffix_matches_pathlib) {
    AvpArena *a = avp_arena();
    for (size_t i = 0; i < COUNT(k_suffixes); i++) {
        const SuffixCase *c = &k_suffixes[i];
        AvpStr r = pv_path_with_suffix(a, avp_str(c->path), avp_str(c->suffix));
        if (!c->result) {
            CHECK_MSG(r.p == NULL, "with_suffix(%s, %s) should fail", c->path, c->suffix);
        } else {
            CHECK_MSG(r.p && avp_str_eqz(r, c->result), "with_suffix(%s, %s): %.*s", c->path, c->suffix, AVP_SF(r));
        }
    }
}

AVP_TEST(path_equality_is_case_insensitive) {
    AvpArena *a = avp_arena();
    CHECK(pv_eq(pv_path(a, AVP_S("A/b")), pv_path(a, AVP_S("a\\B"))));
    CHECK(!pv_eq(pv_path(a, AVP_S("a")), pv_strz(a, "a"))); /* P("a") == "a" is False */
}

AVP_TEST(values_follow_python) {
    AvpArena *a = avp_arena();
    CHECK(pv_none() == pv_none());
    CHECK(pv_bool(true) == pv_bool(true));
    CHECK(pv_eq(pv_bool(true), pv_int(a, 1)));
    CHECK(!pv_eq(pv_strz(a, "1"), pv_int(a, 1)));
    CHECK(!pv_truthy(pv_none()) && !pv_truthy(pv_strz(a, "")) && !pv_truthy(pv_list(a)) && !pv_truthy(pv_int(a, 0)));
    CHECK(pv_truthy(pv_path(a, AVP_S(""))) && pv_truthy(pv_strz(a, "x")));
    Pv *l = pv_list(a);
    pv_list_push(a, l, pv_strz(a, "-O4,p"));
    pv_list_push(a, l, pv_none());
    pv_list_push(a, l, pv_bool(false));
    pv_list_push(a, l, pv_int(a, -3));
    pv_list_push(a, l, pv_strz(a, "it's"));
    CHECK_STR(pv_repr(a, l), "['-O4,p', None, False, -3, \"it's\"]");
    CHECK_STR(pv_to_str(a, l), "['-O4,p', None, False, -3, \"it's\"]");
    pv_list_extend(a, l, l); /* l.extend(l) doubles the list once */
    CHECK_EQ_U64(pv_len(l), 10);
    Pv *c = pv_list_copy(a, l);
    pv_list_push(a, c, pv_none());
    CHECK_EQ_U64(pv_len(l), 10);
    CHECK(!pv_eq(l, c));
    Pv *t = pv_tuple(a);
    pv_list_push(a, t, pv_strz(a, "x"));
    CHECK_STR(pv_repr(a, t), "('x',)");
    CHECK(!pv_eq(t, l));
    Pv *d = pv_dict(a);
    pv_dict_set(a, d, AVP_S("b"), pv_int(a, 1));
    pv_dict_set(a, d, AVP_S("a"), pv_path(a, AVP_S("x/y")));
    pv_dict_set(a, d, AVP_S("b"), pv_int(a, 2));
    CHECK_STR(pv_repr(a, d), "{'b': 2, 'a': WindowsPath('x/y')}");
    Pv *d2 = pv_dict_copy(a, d);
    pv_dict_set(a, d2, AVP_S("c"), pv_none());
    CHECK_EQ_U64(pv_len(d), 2);
    CHECK(pv_dict_get(d, AVP_S("c")) == NULL);
    CHECK(pv_dict_get(d2, AVP_S("a")) == pv_dict_get(d, AVP_S("a")));
    CHECK_EQ_U64(pv_len(pv_strz(a, "h\xc3\xa9")), 2);
    CHECK_STR(avp_str(pv_kind_name(pv_path(a, AVP_S("x")))), "WindowsPath");
    CHECK_STR(avp_str(pv_kind_name(pv_none())), "NoneType");
}

/* The attribute names of vars(ProjectConfig()) in CPython's order. */
static const char *const k_config_attrs[] = {
    "build_dir", "src_dir", "tools_dir", "asm_dir", "binutils_tag", "binutils_path", "dtk_tag", "dtk_path",
    "compilers_tag", "compilers_path", "wibo_tag", "wrapper", "sjiswrap_tag", "sjiswrap_path", "ninja_path",
    "objdiff_tag", "objdiff_path", "non_matching", "build_rels", "check_sha_path", "config_path", "generate_map",
    "asflags", "ldflags", "libs", "precompiled_headers", "linker_version", "version", "warn_missing_config",
    "warn_missing_source", "rel_strip_partial", "rel_empty_file", "shift_jis", "reconfig_deps", "custom_build_rules",
    "custom_build_steps", "generate_compile_commands", "extra_clang_flags", "scratch_preset_id", "link_order_callback",
    "context_exclude_globs", "context_defines", "objcache", "progress", "progress_modules", "progress_each_module",
    "progress_categories", "print_progress_categories", "progress_report_args", "progress_use_fancy",
    "progress_code_fancy_frac", "progress_code_fancy_item", "progress_data_fancy_frac", "progress_data_fancy_item",
};

AVP_TEST(project_config_defaults) {
    AvpArena *a = avp_arena();
    Pv *c = project_new_config(a);
    REQUIRE(c->kind == PV_INSTANCE && pv_isinstance(c, "ProjectConfig"));
    PvDict *d = c->u.inst->attrs;
    REQUIRE(d->n == COUNT(k_config_attrs));
    for (size_t i = 0; i < d->n; i++) CHECK_MSG(avp_str_eqz(d->keys[i], k_config_attrs[i]), "attr %zu", i);
    /* repr of every value, as vars(ProjectConfig()) gives them */
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < d->n; i++) {
        avp_buf_put(&b, pv_repr(a, d->vals[i]));
        avp_buf_putc(&b, ' ');
    }
    CHECK_STR(avp_buf_str(&b),
              "WindowsPath('build') WindowsPath('src') WindowsPath('tools') WindowsPath('asm') None None None None "
              "None None None None None None None None None False True None None False None None None None None "
              "None False False True None True None None None True [] None None [] [] False True True False [] True "
              "None False 0 '' 0 '' ");
    CHECK(project_is_windows());
}

AVP_TEST(object_options_follow_init) {
    AvpArena *a = avp_arena();
    Pv *opts = pv_dict(a);
    Pv *cflags = pv_list(a);
    pv_list_push(a, cflags, pv_strz(a, "x"));
    pv_dict_set(a, opts, AVP_S("cflags"), cflags);
    pv_dict_set(a, opts, AVP_S("foo"), pv_int(a, 1));
    Pv *o = project_new_object(a, true, AVP_S("a/b.c"), opts);
    REQUIRE(pv_isinstance(o, "Object"));
    AvpBuf b;
    avp_buf_init(&b, a);
    PvDict *d = o->u.inst->attrs;
    for (size_t i = 0; i < d->n; i++) {
        avp_buf_put(&b, d->keys[i]);
        avp_buf_putc(&b, '=');
        avp_buf_put(&b, pv_repr(a, d->vals[i]));
        avp_buf_putc(&b, ';');
    }
    /* vars(Object(True, 'a/b.c', cflags=['x'], foo=1)) */
    CHECK_STR(avp_buf_str(&b),
              "name='a/b.c';completed=True;options={'add_to_all': None, 'asflags': None, 'asm_dir': None, 'cflags': "
              "['x'], 'extab_padding': None, 'extra_asflags': [], 'extra_cflags': [], 'extra_clang_flags': [], 'lib': "
              "None, 'mw_version': None, 'progress_category': None, 'scratch_preset_id': None, 'shift_jis': None, "
              "'source': 'a/b.c', 'src_dir': None, 'foo': 1};src_path=None;asm_path=None;src_obj_path=None;"
              "asm_obj_path=None;ctx_path=None;");
    CHECK(pv_dict_get(pv_getattr(o, "options"), AVP_S("cflags")) == cflags); /* shared, not copied */
    Pv *o2 = project_new_object(a, false, AVP_S("x"), NULL);
    CHECK(pv_getattr(o2, "completed") == pv_bool(false));
    Pv *pc = project_new_progress_category(a, AVP_S("sdk"), AVP_S("SDK"));
    CHECK_STR(pv_repr(a, pv_getattr(pc, "id")), "'sdk'");
    CHECK_STR(pv_repr(a, pv_getattr(pc, "name")), "'SDK'");
}

AVP_TEST_MAIN("project")
