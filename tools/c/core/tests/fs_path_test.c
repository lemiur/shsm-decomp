/* os.path (ntpath), pathlib.PureWindowsPath and fnmatch against CPython 3.14 results (fs_path_corpus.h). */
#include "avp/avp.h"
#include "avp/fs.h"
#include "avp/test.h"

#include "fs_path_corpus.h"

#include <stdio.h>

static int g_fail;

static bool view_eq(AvpStr v, const char *z) { return v.n == strlen(z) && memcmp(v.p, z, v.n) == 0; }

#define EXPECT_STR(what, input, got, want)                                                                         \
    do {                                                                                                           \
        const char *g_ = (got), *w_ = (want);                                                                      \
        if (strcmp(g_, w_) != 0 && g_fail++ < 20) avp_test__fail(__FILE__, __LINE__, "%s(%s): got [%s] want [%s]", what, input, g_, w_); \
    } while (0)

#define EXPECT_VIEW(what, input, got, want)                                                                        \
    do {                                                                                                           \
        AvpStr gv_ = (got);                                                                                        \
        const char *w_ = (want);                                                                                   \
        if (!view_eq(gv_, w_) && g_fail++ < 20)                                                                    \
            avp_test__fail(__FILE__, __LINE__, "%s(%s): got [%.*s] want [%s]", what, input, (int)gv_.n, gv_.p, w_); \
    } while (0)

static void in_windows(void) {
    /* abspath/relpath expectations were produced with the current folder C:\Windows */
    CHECK(avp_chdir("C:\\Windows"));
}

AVP_TEST(path_join) {
    AvpArena *a = avp_arena_new(0);
    g_fail = 0;
    for (size_t i = 0; i < AVP_ARRAY_LEN(k_join); i++) {
        const char *parts[3] = {k_join[i].a, k_join[i].b, k_join[i].c};
        char *r = avp_path_joinv(a, (size_t)k_join[i].k, parts);
        EXPECT_STR("join", k_join[i].a, r, k_join[i].out);
        if (g_fail >= 20) break;
    }
    CHECK(g_fail == 0);
    CHECK_CZ(avp_join(a, "C:\\x", "y", "..\\z"), "C:\\x\\y\\..\\z");
    avp_arena_free(a);
}

AVP_TEST(path_single_functions) {
    AvpArena *a = avp_arena_new(0);
    g_fail = 0;
    for (size_t i = 0; i < AVP_ARRAY_LEN(k_single); i++) {
        const char *p = k_single[i].p;
        EXPECT_STR("normpath", p, avp_path_normpath(a, p), k_single[i].normpath);
        EXPECT_STR("normcase", p, avp_path_normcase(a, p), k_single[i].normcase);
        if (avp_path_isabs(p) != (k_single[i].isabs != 0) && g_fail++ < 20) avp_test__fail(__FILE__, __LINE__, "isabs(%s)", p);
        AvpStr d, r, t, h, tl, ro, ex;
        avp_path_splitroot(p, &d, &r, &t);
        EXPECT_VIEW("splitroot.drive", p, d, k_single[i].drive);
        EXPECT_VIEW("splitroot.root", p, r, k_single[i].root);
        EXPECT_VIEW("splitroot.tail", p, t, k_single[i].tail);
        avp_path_splitdrive(p, &d, &r);
        {
            char *want = avp_sprintf(a, "%s%s", k_single[i].root, k_single[i].tail);
            EXPECT_VIEW("splitdrive.rest", p, r, want);
        }
        avp_path_split(p, &h, &tl);
        EXPECT_VIEW("split.head", p, h, k_single[i].head);
        EXPECT_VIEW("split.tail", p, tl, k_single[i].tail2);
        EXPECT_VIEW("basename", p, avp_path_basename(p), k_single[i].tail2);
        EXPECT_VIEW("dirname", p, avp_path_dirname(p), k_single[i].head);
        avp_path_splitext(p, &ro, &ex);
        EXPECT_VIEW("splitext.root", p, ro, k_single[i].sroot);
        EXPECT_VIEW("splitext.ext", p, ex, k_single[i].ext);
        if (g_fail >= 20) break;
    }
    CHECK(g_fail == 0);
    avp_arena_free(a);
}

AVP_TEST(path_abspath) {
    AvpArena *a = avp_arena_new(0);
    in_windows();
    g_fail = 0;
    for (size_t i = 0; i < AVP_ARRAY_LEN(k_abspath); i++) {
        EXPECT_STR("abspath", k_abspath[i].p, avp_path_abspath(a, k_abspath[i].p), k_abspath[i].out);
        if (g_fail >= 20) break;
    }
    CHECK(g_fail == 0);
    avp_arena_free(a);
}

AVP_TEST(path_relpath) {
    AvpArena *a = avp_arena_new(0);
    in_windows();
    g_fail = 0;
    for (size_t i = 0; i < AVP_ARRAY_LEN(k_relpath); i++) {
        char *out = NULL;
        bool ok = avp_path_relpath(a, k_relpath[i].p, k_relpath[i].start, &out);
        if (ok != (k_relpath[i].ok != 0)) {
            if (g_fail++ < 20) avp_test__fail(__FILE__, __LINE__, "relpath(%s, %s): ok=%d want %d", k_relpath[i].p, k_relpath[i].start ? k_relpath[i].start : "(none)", ok, k_relpath[i].ok);
            continue;
        }
        if (ok) EXPECT_STR("relpath", k_relpath[i].p, out, k_relpath[i].out);
        else EXPECT_STR("relpath-error", k_relpath[i].p, avp_io_err_str(a).p, k_relpath[i].out);
        if (g_fail >= 20) break;
    }
    CHECK(g_fail == 0);
    avp_arena_free(a);
}

AVP_TEST(path_commonpath) {
    AvpArena *a = avp_arena_new(0);
    g_fail = 0;
    for (size_t i = 0; i < AVP_ARRAY_LEN(k_commonpath); i++) {
        const char *ps[3] = {k_commonpath[i].a, k_commonpath[i].b, k_commonpath[i].c};
        char *out = NULL;
        bool ok = avp_path_commonpath(a, ps, (size_t)k_commonpath[i].k, &out);
        if (ok != (k_commonpath[i].ok != 0)) {
            if (g_fail++ < 20) avp_test__fail(__FILE__, __LINE__, "commonpath(%s,...): ok=%d want %d", ps[0] ? ps[0] : "", ok, k_commonpath[i].ok);
            continue;
        }
        if (ok) EXPECT_STR("commonpath", ps[0] ? ps[0] : "", out, k_commonpath[i].out);
        else EXPECT_STR("commonpath-error", ps[0] ? ps[0] : "", avp_io_err_str(a).p, k_commonpath[i].out);
        if (g_fail >= 20) break;
    }
    CHECK(g_fail == 0);
    avp_arena_free(a);
}

AVP_TEST(path_expanduser) {
    AvpArena *a = avp_arena_new(0);
    /* ntpath.expanduser with USERPROFILE=C:\Users\bob and USERNAME=bob (python: os.environ set the same way) */
    avp_setenv("USERPROFILE", "C:\\Users\\bob");
    avp_setenv("USERNAME", "bob");
    CHECK_CZ(avp_path_expanduser(a, "~"), "C:\\Users\\bob");
    CHECK_CZ(avp_path_expanduser(a, "~\\x\\y"), "C:\\Users\\bob\\x\\y");
    CHECK_CZ(avp_path_expanduser(a, "~/x"), "C:\\Users\\bob/x");
    CHECK_CZ(avp_path_expanduser(a, "~bob/x"), "C:\\Users\\bob/x");
    CHECK_CZ(avp_path_expanduser(a, "~alice\\x"), "C:\\Users\\alice\\x");
    CHECK_CZ(avp_path_expanduser(a, "x~"), "x~");
    avp_arena_free(a);
}

AVP_TEST(pathlib_pure) {
    AvpArena *a = avp_arena_new(0);
    g_fail = 0;
    for (size_t i = 0; i < AVP_ARRAY_LEN(k_pl); i++) {
        const char *p = k_pl[i].p;
        char *s = avp_pl_str(a, p);
        EXPECT_STR("pl.str", p, s, k_pl[i].str);
        EXPECT_VIEW("pl.name", s, avp_pl_name(s), k_pl[i].name);
        EXPECT_VIEW("pl.suffix", s, avp_pl_suffix(s), k_pl[i].suffix);
        EXPECT_VIEW("pl.stem", s, avp_pl_stem(s), k_pl[i].stem);
        EXPECT_STR("pl.parent", p, avp_pl_parent(a, p), k_pl[i].parent);
        AvpStrVec parts = avp_pl_parts(a, p);
        char *j = (char *)avp_alloc(a, 4096);
        size_t jn = 0;
        for (size_t k = 0; k < parts.n; k++) {
            if (k) j[jn++] = '\x1f';
            memcpy(j + jn, parts.v[k].p, parts.v[k].n);
            jn += parts.v[k].n;
        }
        EXPECT_STR("pl.parts", p, j, k_pl[i].parts);
        avp_vec_free(&parts);
        if (g_fail >= 20) break;
    }
    CHECK(g_fail == 0);
    avp_arena_free(a);
}

AVP_TEST(pathlib_with) {
    AvpArena *a = avp_arena_new(0);
    g_fail = 0;
    for (size_t i = 0; i < AVP_ARRAY_LEN(k_pl_with); i++) {
        char *r1 = avp_pl_with_suffix(a, k_pl_with[i].p, k_pl_with[i].suffix);
        if ((r1 != NULL) != (k_pl_with[i].ok1 != 0)) {
            if (g_fail++ < 20) avp_test__fail(__FILE__, __LINE__, "with_suffix(%s,%s) ok=%d want %d", k_pl_with[i].p, k_pl_with[i].suffix, r1 != NULL, k_pl_with[i].ok1);
        } else if (r1) EXPECT_STR("with_suffix", k_pl_with[i].p, r1, k_pl_with[i].out1);
        char *r2 = avp_pl_with_name(a, k_pl_with[i].p, k_pl_with[i].name);
        if ((r2 != NULL) != (k_pl_with[i].ok2 != 0)) {
            if (g_fail++ < 20) avp_test__fail(__FILE__, __LINE__, "with_name(%s,%s) ok=%d want %d", k_pl_with[i].p, k_pl_with[i].name, r2 != NULL, k_pl_with[i].ok2);
        } else if (r2) EXPECT_STR("with_name", k_pl_with[i].p, r2, k_pl_with[i].out2);
        if (g_fail >= 20) break;
    }
    CHECK(g_fail == 0);
    avp_arena_free(a);
}

AVP_TEST(pathlib_relative_and_order) {
    AvpArena *a = avp_arena_new(0);
    g_fail = 0;
    for (size_t i = 0; i < AVP_ARRAY_LEN(k_pl_rel); i++) {
        char *out = NULL;
        bool ok = avp_pl_relative_to(a, k_pl_rel[i].p, k_pl_rel[i].base, &out);
        if (ok != (k_pl_rel[i].ok != 0)) {
            if (g_fail++ < 20) avp_test__fail(__FILE__, __LINE__, "relative_to(%s,%s) ok=%d want %d", k_pl_rel[i].p, k_pl_rel[i].base, ok, k_pl_rel[i].ok);
        } else if (ok) EXPECT_STR("relative_to", k_pl_rel[i].p, out, k_pl_rel[i].out);
        if (avp_pl_is_relative_to(k_pl_rel[i].p, k_pl_rel[i].base) != (k_pl_rel[i].is_rel != 0) && g_fail++ < 20)
            avp_test__fail(__FILE__, __LINE__, "is_relative_to(%s,%s)", k_pl_rel[i].p, k_pl_rel[i].base);
        if (avp_pl_cmp(k_pl_rel[i].p, k_pl_rel[i].base) != k_pl_rel[i].cmp && g_fail++ < 20)
            avp_test__fail(__FILE__, __LINE__, "cmp(%s,%s) want %d", k_pl_rel[i].p, k_pl_rel[i].base, k_pl_rel[i].cmp);
        if (g_fail >= 20) break;
    }
    CHECK(g_fail == 0);
    avp_arena_free(a);
}

AVP_TEST(fnmatch_corpus) {
    g_fail = 0;
    for (size_t i = 0; i < AVP_ARRAY_LEN(k_fnmatch); i++) {
        bool nc = avp_fnmatch(k_fnmatch[i].name, k_fnmatch[i].pat), cs = avp_fnmatchcase(k_fnmatch[i].name, k_fnmatch[i].pat);
        if (nc != (k_fnmatch[i].nc != 0) && g_fail++ < 20)
            avp_test__fail(__FILE__, __LINE__, "fnmatch(%s, %s) = %d want %d", k_fnmatch[i].name, k_fnmatch[i].pat, nc, k_fnmatch[i].nc);
        if (cs != (k_fnmatch[i].cs != 0) && g_fail++ < 20)
            avp_test__fail(__FILE__, __LINE__, "fnmatchcase(%s, %s) = %d want %d", k_fnmatch[i].name, k_fnmatch[i].pat, cs, k_fnmatch[i].cs);
        if (g_fail >= 20) break;
    }
    CHECK(g_fail == 0);
}

AVP_TEST_MAIN("fs_path")
