/* draft_world.c / draft_twin.c: the pure parts (brief._base_words, the profile dict, draft_paths). Expected values
 * are the Python's (brief._base_words, json.dumps of profile_of's dict, draft.draft_paths on the same inputs). The
 * twin command itself is proven by the differential cases and the real-tree comparison (DEVIATIONS.md). */
#include "avp/test.h"
#include "draft.h"

#include <string.h>

static AvpStrVec words(const char *const *w) {
    AvpStrVec v = {0};
    for (size_t i = 0; w[i]; i++) avp_vec_push(&v, avp_str(w[i]));
    return v;
}

static const char *joined(AvpStrVec v) {
    return avp_cz(avp_arena(), avp_str_join(avp_arena(), AVP_S(" "), v.v, v.n));
}

AVP_TEST(base_words) {
    AvpArena *a = avp_arena();
    static const char *const W1[] = {"-nodefaults", "-DX=1", "-proc", "gekko", "-DY", "-O4,p", "-lang=c++", "-ipa",
                                     "file", NULL};
    static const char *const W2[] = {"-O4", "-lang=c", NULL};
    static const char *const W3[] = {NULL};
    AvpStrVec v1 = words(W1), v2 = words(W2), v3 = words(W3);
    /* ['-O4,p', '-ipa', 'file'], ['-O4'], [] */
    CHECK_CZ(joined(draft_base_words(a, &v1)), "-O4,p -ipa file");
    CHECK_CZ(joined(draft_base_words(a, &v2)), "-O4");
    CHECK_EQ_I64((i64)draft_base_words(a, &v3).n, 0);
}

AVP_TEST(profile_json) {
    AvpArena *a = avp_arena();
    DraftProfile p = {0};
    p.unit = "main/game/PadRumble";
    p.compiler = "Wii/1.0";
    avp_vec_push(&p.extra, avp_str("-O4"));
    p.match = "same group";
    p.distance = 16;
    AvpJsonDumpOpts o = {0};
    CHECK_CZ(avp_cz(a, avp_json_dumps(a, draft_profile_json(a, &p), &o)),
             "{\"unit\": \"main/game/PadRumble\", \"library\": null, \"compiler\": \"Wii/1.0\", \"extra\": [\"-O4\"], "
             "\"lang\": null, \"match\": \"same group\", \"distance\": 16}");
}

AVP_TEST(paths) {
    AvpArena *a = avp_arena();
    ShsmRepo *r = shsm_repo_find(NULL);
    char *rel;
    /* draft_paths(repo, None, "__sinit_\\X_cpp"): build\draft\__sinit__X_cpp under the root */
    REQUIRE(avp_path_relpath(a, draft_paths(a, r, NULL, "__sinit_\\X_cpp"), r->root, &rel));
    CHECK_CZ(rel, "build\\draft\\__sinit__X_cpp");
    REQUIRE(avp_path_relpath(a, draft_paths(a, r, "", "fn_80001000"), r->root, &rel));
    CHECK_CZ(rel, "build\\draft\\fn_80001000");
    CHECK_CZ(draft_paths(a, r, "Z:\\o\\p/q", "t"), "Z:\\o\\p\\q");
}

/* draft_first_twin_draft: the twins in order, a twin without a definition passes to the next */
static DraftTwinMeta MADE;
static int attempts;

static DraftTwinMeta *attempt_by_name(void *ctx, TwinsFn *tw) {
    const char *good = ctx;
    attempts++;
    if (!strcmp(tw->name, "io")) avp_throw(AVP_ERR_IO, "disk gone");
    if (good && !strcmp(tw->name, good)) return &MADE;
    avp_throw(SHSM_EXC_FINDING, "the twin %s has no definition", tw->name);
}

AVP_TEST(first_twin_draft) {
    TwinsFn a = {0}, b = {0}, c = {0};
    a.name = "twin_a";
    b.name = "twin_b";
    c.name = "twin_c";
    TwinsFnVec v = {0};
    avp_vec_push(&v, &a);
    avp_vec_push(&v, &b);
    avp_vec_push(&v, &c);
    AvpTry t;
    /* the second twin is the first with a definition: the third is not tried */
    attempts = 0;
    CHECK(draft_first_twin_draft(&v, attempt_by_name, "twin_b") == &MADE);
    CHECK_EQ_I64(attempts, 2);
    /* none has one: the first twin's reason, and how many were tried */
    attempts = 0;
    if (AVP_TRY(&t)) {
        draft_first_twin_draft(&v, attempt_by_name, NULL);
        avp_try_end(&t);
        CHECK_MSG(false, "expected a failure");
    } else {
        CHECK_EQ_I64(t.code, SHSM_EXC_FINDING);
        CHECK_CZ(t.msg, "the twin twin_a has no definition (3 matched twins tried, none could be cloned)");
    }
    CHECK_EQ_I64(attempts, 3);
    /* one twin: its own message, unchanged */
    TwinsFnVec one = {0};
    avp_vec_push(&one, &a);
    if (AVP_TRY(&t)) {
        draft_first_twin_draft(&one, attempt_by_name, NULL);
        avp_try_end(&t);
        CHECK_MSG(false, "expected a failure");
    } else {
        CHECK_CZ(t.msg, "the twin twin_a has no definition");
    }
    /* an error that is not about the twin stops the loop */
    TwinsFn io = {0};
    io.name = "io";
    TwinsFnVec w = {0};
    avp_vec_push(&w, &io);
    avp_vec_push(&w, &b);
    attempts = 0;
    if (AVP_TRY(&t)) {
        draft_first_twin_draft(&w, attempt_by_name, "twin_b");
        avp_try_end(&t);
        CHECK_MSG(false, "expected a failure");
    } else {
        CHECK_EQ_I64(t.code, AVP_ERR_IO);
    }
    CHECK_EQ_I64(attempts, 1);
}

/* draft_twin_level: ref when the relocation targets agree too, else exact */
AVP_TEST(twin_level) {
    TwinsFn f = {0}, same = {0}, other = {0};
    f.fp[TWINS_EXACT] = same.fp[TWINS_EXACT] = other.fp[TWINS_EXACT] = "aa";
    f.fp[TWINS_REF] = same.fp[TWINS_REF] = "bb";
    other.fp[TWINS_REF] = "cc";
    CHECK_CZ(draft_twin_level(&f, &same), "ref");
    CHECK_CZ(draft_twin_level(&f, &other), "exact");
}

AVP_TEST_MAIN("draft")
