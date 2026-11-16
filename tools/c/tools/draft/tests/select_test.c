/* draft_select.c: group_by_class and the select cache, the cases of tools/test_draft.py's ByClass and SelectCache
 * (the same classes, queue, sweep verdicts and expected groups). select, select_classes and the commands are proven
 * by the differential cases and the real-tree comparison (DEVIATIONS.md). */
#include "avp/test.h"
#include "draft.h"

#include <string.h>

/* klass(key, name, source, members, registry, size) of test_draft.py */
static AvpJson *klass(AvpArena *a, const char *key, const char *name, const char *source, const i64 *addrs,
                      const char *const *roles, size_t n, const i64 *registry, size_t nreg, const char *size) {
    AvpJson *c = avp_json_obj(a);
    avp_json_set(a, c, AVP_S("key"), avp_json_strz(a, key));
    avp_json_set(a, c, AVP_S("name"), avp_json_strz(a, name));
    avp_json_set(a, c, AVP_S("name_source"), avp_json_strz(a, source));
    avp_json_set(a, c, AVP_S("vtable_symbol"), avp_json_strz(a, key + 3));
    avp_json_set(a, c, AVP_S("size"), size ? avp_json_strz(a, size) : avp_json_null());
    AvpJson *m = avp_json_arr(a), *r = avp_json_arr(a);
    for (size_t i = 0; i < n; i++) {
        AvpJson *x = avp_json_obj(a);
        avp_json_set(a, x, AVP_S("name"), avp_json_strz(a, avp_sprintf(a, "fn_%08llX", (unsigned long long)addrs[i])));
        avp_json_set(a, x, AVP_S("address"), avp_json_strz(a, avp_sprintf(a, "0x%08llX", (unsigned long long)addrs[i])));
        avp_json_set(a, x, AVP_S("role"), avp_json_strz(a, roles[i]));
        avp_json_arr_push(a, m, x);
    }
    for (size_t i = 0; i < nreg; i++) {
        AvpJson *x = avp_json_obj(a);
        avp_json_set(a, x, AVP_S("string"), avp_json_strz(a, name));
        avp_json_set(a, x, AVP_S("sinit"), avp_json_strz(a, avp_sprintf(a, "fn_%08llX", (unsigned long long)registry[i])));
        avp_json_set(a, x, AVP_S("factory"), avp_json_null());
        avp_json_arr_push(a, r, x);
    }
    avp_json_set(a, c, AVP_S("members"), m);
    avp_json_set(a, c, AVP_S("registry"), r);
    return c;
}

static AvpJson *classes(AvpArena *a) {
    AvpJson *cs = avp_json_arr(a);
    /* CThing: two slots, a factory and its registry static initialiser */
    static const i64 A1[] = {0x80001000, 0x80001040, 0x80001080}, R1[] = {0x800010C0};
    static const char *const P1[] = {"slot", "slot", "factory"};
    avp_json_arr_push(a, cs, klass(a, "vt:lbl_803E0000", "CThing", "registry-string", A1, P1, 3, R1, 1, "0x40"));
    /* a placeholder class whose function reads an interned name; it also lists CThing's slot */
    static const i64 A2[] = {0x80002000, 0x80002040, 0x80001040};
    static const char *const P2[] = {"dtor", "slot", "slot"};
    avp_json_arr_push(a, cs, klass(a, "vt:lbl_803E0100", "unk_803E0100", "placeholder (vtable address)", A2, P2, 3,
                                   NULL, 0, NULL));
    /* a placeholder class with no interned name */
    static const i64 A3[] = {0x80003000, 0x80003040};
    static const char *const P3[] = {"slot", "slot"};
    avp_json_arr_push(a, cs, klass(a, "vt:lbl_803E0200", "unk_803E0200", "placeholder (vtable address)", A3, P3, 2,
                                   NULL, 0, NULL));
    return cs;
}

static AvpJson *queued(AvpArena *a, const i64 *addrs, size_t n) {
    AvpJson *q = avp_json_arr(a);
    for (size_t i = 0; i < n; i++) {
        AvpJson *e = avp_json_obj(a);
        avp_json_set(a, e, AVP_S("symbol"), avp_json_strz(a, avp_sprintf(a, "fn_%08llX", (unsigned long long)addrs[i])));
        avp_json_set(a, e, AVP_S("address"), avp_json_strz(a, avp_sprintf(a, "0x%08llX", (unsigned long long)addrs[i])));
        avp_json_arr_push(a, q, e);
    }
    return q;
}

static const AvpJson *by_class(const AvpJson *groups, const char *name) {
    for (size_t i = 0; i < avp_json_len(groups); i++) {
        const AvpJson *g = avp_json_at(groups, i);
        if (avp_str_eqz(avp_json_get_str(g, "class", AVP_S("")), name)) return g;
    }
    return NULL;
}

static const char *symbols_of(AvpArena *a, const AvpJson *g) {
    AvpJson *s = avp_json_getz(g, "symbols");
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < avp_json_len(s); i++) {
        if (i) avp_buf_putz(&b, " ");
        avp_buf_put(&b, avp_json_str_or(avp_json_at(s, i), AVP_S("")));
    }
    return avp_cz(a, avp_buf_str(&b));
}

AVP_TEST(groups) {
    AvpArena *a = avp_arena();
    static const i64 FNS[] = {0x80001000, 0x80001040, 0x80001080, 0x800010C0, 0x800010F8,
                              0x80002000, 0x80002040, 0x80003000, 0x80003040, 0x80009000};
    TwinsFn *fi = AVP_NEWN(a, TwinsFn, 10);
    AvpIMap *fn_info = avp_imap_new();
    for (int i = 0; i < 10; i++) {
        fi[i].address = FNS[i];
        fi[i].size = 0x40;
        fi[i].matched = FNS[i] == 0x80003040; /* matched: not counted */
        avp_imap_put(fn_info, (u64)FNS[i], NULL)->p = &fi[i];
    }
    static const i64 Q[] = {0x80001000, 0x80001040, 0x800010C0, 0x800010F8, 0x80002040, 0x80003000, 0x80009000};
    AvpMap *sweep = avp_map_new();
    avp_map_put(sweep, AVP_S("fn_80001000"), NULL)->p = avp_json_strz(a, "MATCH");
    avp_map_put(sweep, AVP_S("fn_80001040"), NULL)->p = avp_json_strz(a, "PARTIAL");
    avp_map_put(sweep, AVP_S("fn_800010C0"), NULL)->p = avp_json_strz(a, "MATCH");
    AvpIMap *interned = avp_imap_new();
    avp_imap_put(interned, 0x80002040, NULL)->p = "CLamp";
    AvpJson *g = draft_group_by_class(a, queued(a, Q, 7), classes(a), interned, sweep, fn_info, NULL, NULL);
    /* the classless function 0x38 bytes after the static initialiser joins CThing; the shared slot is CThing's (its
     * factory is nearer than the placeholder's destructor) */
    const AvpJson *thing = by_class(g, "CThing");
    CHECK(thing != NULL);
    CHECK_CZ(symbols_of(a, thing), "fn_80001000 fn_80001040 fn_800010C0 fn_800010F8");
    CHECK_EQ_I64(avp_json_get_i64(thing, "queued", -1), 4);
    CHECK_EQ_I64(avp_json_get_i64(thing, "functions", -1), 5);
    CHECK_EQ_I64(avp_json_get_i64(thing, "sweep_match", -1), 2);
    CHECK_CZ(avp_cz(a, avp_json_get_str(thing, "object_size", AVP_S(""))), "0x40");
    CHECK_EQ_I64(avp_json_get_i64(thing, "code_size", -1), 5 * 0x40);
    const AvpJson *lamp = by_class(g, "CLamp");
    CHECK(lamp != NULL);
    CHECK_CZ(avp_cz(a, avp_json_get_str(lamp, "how", AVP_S(""))), "interned name");
    CHECK_EQ_I64(avp_json_get_i64(lamp, "queued", -1), 1);
    CHECK_EQ_I64(avp_json_get_i64(lamp, "functions", -1), 2);
    const AvpJson *vt = by_class(g, "lbl_803E0200");
    CHECK(vt != NULL);
    CHECK_CZ(avp_cz(a, avp_json_get_str(vt, "how", AVP_S(""))), "vtable symbol");
    CHECK_EQ_I64(avp_json_get_i64(vt, "functions", -1), 1);
    /* coverage order, the classless function last */
    CHECK_EQ_I64((i64)avp_json_len(g), 4);
    static const char *const ORDER[] = {"lbl_803E0200", "CThing", "CLamp", "(no class) fn_80009000"};
    for (size_t i = 0; i < 4 && i < avp_json_len(g); i++)
        CHECK_CZ(avp_cz(a, avp_json_get_str(avp_json_at(g, i), "class", AVP_S(""))), ORDER[i]);
    /* the dict's keys, in the Python's order */
    const char *keys[] = {"class", "how", "vtable", "object_size", "queued", "sweep_match", "symbols", "functions",
                          "code_size", "first"};
    for (size_t i = 0; i < 10; i++) CHECK_STR(avp_json_member(thing, i)->key, keys[i]);
    CHECK_CZ(avp_cz(a, avp_json_get_str(thing, "first", AVP_S(""))), "0x80001000");
}

AVP_TEST(no_sweep) {
    AvpArena *a = avp_arena();
    static const i64 Q[] = {0x80001000};
    AvpJson *g = draft_group_by_class(a, queued(a, Q, 1), classes(a), NULL, NULL, NULL, NULL, NULL);
    CHECK(avp_json_is_null(avp_json_getz(avp_json_at(g, 0), "sweep_match")));
}

/* compute() of the Python test: {"n": number of calls so far} */
static i64 g_calls;
static AvpJson *select_test_compute(void *ctx) {
    AvpArena *a = *(AvpArena **)ctx;
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("n"), avp_json_int(a, ++g_calls));
    return o;
}

AVP_TEST(cache_hit_and_invalidation) {
    AvpArena *a = avp_arena();
    ShsmRepo r = {0};
    r.root = avp_path_abspath(a, "build/tmp/draft_select_test");
    avp_rmtree(r.root, AVP_RMTREE_IGNORE_ERRORS);
    CHECK(avp_makedirs(r.root, true));
    const char *inp = avp_path_join(a, r.root, "symbols.txt");
    CHECK(avp_write_text(inp, AVP_S("a"), AVP_TM_UTF8));
    AvpStrVec inputs = {0};
    avp_vec_push(&inputs, avp_str(inp));
    AvpArena *ctx = a;
    AvpJson *(*compute)(void *) = select_test_compute;
    bool hit = true;
    AvpJson *v = draft_cached(a, &r, "k", compute, &ctx, &inputs, &hit);
    CHECK(!hit);
    CHECK_EQ_I64(avp_json_get_i64(v, "n", -1), 1);
    v = draft_cached(a, &r, "k", compute, &ctx, &inputs, &hit);
    CHECK(hit);
    CHECK_EQ_I64(avp_json_get_i64(v, "n", -1), 1);
    CHECK(avp_write_text(inp, AVP_S("ab"), AVP_TM_UTF8));
    v = draft_cached(a, &r, "k", compute, &ctx, &inputs, &hit);
    CHECK(!hit);
    CHECK_EQ_I64(avp_json_get_i64(v, "n", -1), 2);
    /* the file is json.dump(d): compact, the default separators, stamp first */
    AvpStr text;
    CHECK(avp_read_text(a, avp_join(a, r.root, "build", "draft", "select_cache.json"), AVP_TM_UTF8, &text));
    CHECK(avp_str_startswith(text, AVP_S("{\"stamp\": \"")));
    CHECK(avp_str_endswith(text, AVP_S("\", \"entries\": {\"k\": {\"n\": 2}}}")));
    avp_rmtree(r.root, AVP_RMTREE_IGNORE_ERRORS);
}

AVP_TEST(file_stamp) {
    /* hashlib.sha1(repr([("<normcase(abspath)>", None)]).encode()) for a missing file */
    AvpArena *a = avp_arena();
    const char *p = "Z:\\No Such\\File.txt";
    const char *s = draft_file_stamp(a, &p, 1);
    AvpStr repr = AVP_S("[('z:\\\\no such\\\\file.txt', None)]");
    CHECK_CZ(s, avp_sha1_hex(a, repr.p, repr.n));
}

AVP_TEST_MAIN("draft_select")
