/* draft_promote.c: tools/test_draft.py's Promote cases with the same fake promoter (dirty tree, tool calls that
 * record their argv and fail on request, rename, undo) and the same graded draft. The real steps are proven by the
 * differential cases (refusals, --dry-run) and the real-tree dry runs (DEVIATIONS.md). */
#include "avp/test.h"
#include "draft.h"

#include <string.h>

static const char DRAFT_SRC[] = "// 0x80003000\nvoid CBar::fn_801A73A8( int x ) {\n}\n";

/* FakePromoter */
typedef struct {
    const char *root;
    AvpStrVec dirty;
    const char *fail; /* the tool that fails (argv[0]), NULL: none */
    AvpStrVec calls;  /* each call's argv joined by spaces; renames as "rename OLD NEW" */
    bool written;     /* the undo case: source and header added to the dirty list on the first ninja run */
    bool undo_case;
    bool weak;       /* unit_of: a plan unit whose cuts are only medium, not an edge and a high cut */
    bool configured; /* unit_of: an already-configured split */
    bool edges;      /* unit_of: the plan's only unit, the asm block's edges on both sides */
    const char *unit_source;
} Fake;

static Fake *fake_of(DraftPromoter *p) { return p->ctx; }

static AvpStrVec fake_dirty(DraftPromoter *p) {
    AvpStrVec out = {0};
    Fake *f = fake_of(p);
    for (size_t i = 0; i < f->dirty.n; i++) avp_vec_push(&out, f->dirty.v[i]);
    return out;
}

static ShsmRun fake_tool(DraftPromoter *p, const char *const *argv, size_t n) {
    AvpArena *a = avp_arena();
    Fake *f = fake_of(p);
    AvpBuf b;
    avp_buf_init(&b, a);
    bool dry = false;
    for (size_t i = 0; i < n; i++) {
        if (i) avp_buf_putz(&b, " ");
        avp_buf_putz(&b, argv[i]);
        dry = dry || !strcmp(argv[i], "--dry-run");
    }
    avp_vec_push(&f->calls, avp_buf_str(&b));
    if (!strcmp(argv[0], "unit") && !dry) {
        avp_vec_push(&f->dirty, AVP_S("configure.py"));
        avp_vec_push(&f->dirty, AVP_S("config/R5WEA4/splits.txt"));
    }
    bool ok = !f->fail || strcmp(argv[0], f->fail) != 0;
    ShsmRun r = {0};
    r.returncode = ok ? 0 : 1;
    r.out = AVP_S("");
    r.err = ok ? AVP_S("") : avp_str(avp_sprintf(a, "%s: it failed", argv[0]));
    if (f->undo_case && !strcmp(argv[0], "ninja") && !f->written) {
        f->written = true; /* the source and header written before the regeneration runs */
        avp_vec_push(&f->dirty, AVP_S("src/game/CBar.cpp"));
        avp_vec_push(&f->dirty, AVP_S("include/game/CBar.h"));
    }
    return r;
}

static void fake_rename(DraftPromoter *p, const char *old, const char *new_) {
    Fake *f = fake_of(p);
    avp_vec_push(&f->calls, avp_str(avp_sprintf(avp_arena(), "rename %s %s", old, new_)));
    avp_vec_push(&f->dirty, AVP_S("config/R5WEA4/symbols.txt"));
}

static bool has(const AvpStrVec *v, AvpStr s) {
    for (size_t i = 0; i < v->n; i++)
        if (avp_str_eq(v->v[i], s)) return true;
    return false;
}

static AvpStrVec fake_undo(DraftPromoter *p, const AvpStrVec *before) {
    AvpArena *a = avp_arena();
    Fake *f = fake_of(p);
    AvpStrVec changed = {0}, keep = {0};
    for (size_t i = 0; i < f->dirty.n; i++) {
        if (has(before, f->dirty.v[i])) {
            avp_vec_push(&keep, f->dirty.v[i]);
            continue;
        }
        avp_vec_push(&changed, f->dirty.v[i]);
        const char *full = avp_path_join(a, f->root, avp_cz(a, f->dirty.v[i]));
        if (avp_isfile(full) &&
            (avp_str_startswith(f->dirty.v[i], AVP_S("src/")) || avp_str_startswith(f->dirty.v[i], AVP_S("include/"))))
            avp_remove(full);
    }
    f->dirty = keep;
    return changed;
}

static DraftUnitOf fake_unit_of(DraftPromoter *p, i64 address) {
    (void)address;
    Fake *f = fake_of(p);
    /* default: a high cut on both sides */
    DraftUnitOf u = {.path = NULL, .kind = "plan", .has_conf = true, .conf = 0.95, .has_range = true};
    u.start = 0x80003000;
    u.end = 0x80003010;
    u.has_start_conf = true;
    u.start_conf = 0.95;
    if (f->weak) {
        u.start_conf = 0.7;
        u.conf = 0.65;
    }
    if (f->edges) { /* the plan's only unit: the block's edges, no cut */
        u.has_start_conf = u.has_conf = false;
        u.start_edge = u.end_edge = true;
    }
    if (f->configured) u.kind = "unit";
    return u;
}

static const char *fake_unit_source(DraftPromoter *p, const char *unit_name) {
    (void)unit_name;
    return fake_of(p)->unit_source; /* the Python test patches units.get_unit (KeyError, or a unit) */
}

/* good(verdict, blocking) */
typedef struct {
    const char *verdict;
    bool blocking;
    int extra; /* functions besides the one graded */
} Good;

static DraftGrade *good(void *ctx, const char *range, const char *source) {
    (void)range;
    (void)source;
    AvpArena *a = avp_arena();
    Good *g = ctx;
    DraftGrade *d = AVP_NEW(a, DraftGrade);
    d->verdict = g->verdict;
    d->blocking = g->blocking;
    avp_vec_push(&d->lines, avp_str(avp_sprintf(a, "GRADE %s", g->verdict)));
    DraftGradeFn fn = {0};
    fn.retail = "fn_80003000";
    fn.compiled = "fn_801A73A8__4CBarFi";
    fn.marked = true;
    fn.status = "EXACT";
    avp_vec_push(&d->functions, fn);
    for (int i = 0; i < g->extra; i++) {
        DraftGradeFn x = {0};
        x.retail = "fn_80003008";
        x.compiled = "fn_80003008";
        x.status = "EXACT";
        avp_vec_push(&d->functions, x);
    }
    return d;
}

typedef struct {
    ShsmRepo repo;
    DraftWorld W;
    const char *src, *hdr;
    Fake fake;
    DraftPromoter pr;
} Setup;

static Setup *setup(const char *name) {
    AvpArena *a = avp_arena();
    Setup *s = AVP_NEW(a, Setup);
    const char *tmp = avp_path_abspath(a, avp_sprintf(a, "build/tmp/draft_promote_test/%s", name));
    avp_rmtree(tmp, AVP_RMTREE_IGNORE_ERRORS);
    avp_makedirs(tmp, true);
    s->repo.root = tmp;
    s->repo.version = "R5WEA4";
    s->repo.src_dir = avp_path_join(a, tmp, "src");
    s->repo.include_dir = avp_path_join(a, tmp, "include");
    s->W.a = a;
    s->W.repo = &s->repo;
    s->src = avp_path_join(a, tmp, "draft.cpp");
    avp_write_text(s->src, avp_str(DRAFT_SRC), AVP_TM_UTF8);
    s->hdr = avp_path_join(a, tmp, "draft.h");
    avp_write_text(s->hdr, AVP_S("class CBar {};\n"), AVP_TM_UTF8);
    s->fake.root = tmp;
    s->pr.repo = &s->repo;
    s->pr.W = &s->W;
    s->pr.ctx = &s->fake;
    s->pr.dirty = fake_dirty;
    s->pr.tool = fake_tool;
    s->pr.rename = fake_rename;
    s->pr.undo = fake_undo;
    s->pr.unit_of = fake_unit_of;
    s->pr.unit_source = fake_unit_source;
    return s;
}

static bool run_promote(Setup *s, Good *g, bool dry_run, AvpStrVec *lines) {
    return draft_promote(&s->W, "game/CBar.cpp", s->src, s->hdr, false, 0, 0, dry_run, false, &s->pr, good, g, lines);
}

static bool contains(AvpStr s, const char *sub) { return avp_str_find(s, avp_str(sub), 0, AVP_END) >= 0; }

static bool any_line(const AvpStrVec *v, const char *sub) {
    for (size_t i = 0; i < v->n; i++)
        if (contains(v->v[i], sub)) return true;
    return false;
}

AVP_TEST(refuses_dirty_tree) {
    Setup *s = setup("dirty");
    avp_vec_push(&s->fake.dirty, AVP_S("src/x.cpp"));
    AvpStrVec lines = {0};
    Good g = {"MATCH", false, 0};
    CHECK(!run_promote(s, &g, false, &lines));
    CHECK(contains(lines.v[0], "not clean"));
    CHECK_STR(lines.v[0], "REFUSED: the tree is not clean (1 path(s), first src/x.cpp)");
}

AVP_TEST(refuses_partial) {
    Setup *s = setup("partial");
    AvpStrVec lines = {0};
    Good g = {"PARTIAL", false, 0};
    CHECK(!run_promote(s, &g, false, &lines));
    CHECK_STR(lines.v[lines.n - 1], "REFUSED: the draft does not grade MATCH without findings over 80003000-80003010");
    CHECK_STR(lines.v[0], "  GRADE PARTIAL");
}

AVP_TEST(refuses_blocking) {
    Setup *s = setup("blocking");
    AvpStrVec lines = {0};
    Good g = {"MATCH", true, 0};
    CHECK(!run_promote(s, &g, false, &lines));
    CHECK(contains(lines.v[lines.n - 1], "REFUSED"));
}

AVP_TEST(refuses_existing_unit) {
    Setup *s = setup("existing");
    AvpArena *a = avp_arena();
    avp_makedirs(avp_join(a, s->repo.root, "src", "game"), true);
    avp_write_text(avp_join(a, s->repo.root, "src", "game", "CBar.cpp"), avp_str(DRAFT_SRC), AVP_TM_UTF8);
    s->fake.unit_source = "src/game/CBar.cpp";
    AvpStrVec lines = {0};
    Good g = {"MATCH", false, 0};
    CHECK(!run_promote(s, &g, false, &lines));
    CHECK_STR(lines.v[lines.n - 1], "REFUSED: main/game/CBar exists and is not a stub");
}

AVP_TEST(dry_run_writes_nothing) {
    Setup *s = setup("dry");
    AvpArena *a = avp_arena();
    AvpStrVec lines = {0};
    Good g = {"MATCH", false, 0};
    CHECK(run_promote(s, &g, true, &lines));
    CHECK(!avp_exists(avp_join(a, s->repo.root, "src", "game", "CBar.cpp")));
    CHECK(any_line(&s->fake.calls, "--dry-run"));
    CHECK_STR(s->fake.calls.v[0], "unit new game/CBar.cpp --text 80003000-80003010 --dry-run");
    CHECK(any_line(&lines, "would rename fn_80003000 -> fn_801A73A8__4CBarFi"));
    CHECK_STR(lines.v[lines.n - 1],
              "dry run: would unit.exe new game/CBar.cpp --text 80003000-80003010; rename fn_80003000 -> "
              "fn_801A73A8__4CBarFi (symbols.txt); write src/game/CBar.cpp; write include/game/CBar.h; ninja "
              "build.ninja (dtk split, configure.exe); match.exe main/game/CBar; gatecheck.exe layout main/game/CBar; "
              "check_rules.exe --units main/game/CBar");
}

AVP_TEST(success) {
    Setup *s = setup("success");
    AvpArena *a = avp_arena();
    AvpStrVec lines = {0};
    Good g = {"MATCH", false, 0};
    CHECK(run_promote(s, &g, false, &lines));
    CHECK(avp_exists(avp_join(a, s->repo.root, "src", "game", "CBar.cpp")));
    CHECK(avp_exists(avp_join(a, s->repo.root, "include", "game", "CBar.h")));
    CHECK(contains(lines.v[lines.n - 1], "unit.exe flip game/CBar.cpp"));
    CHECK(has(&s->fake.calls, AVP_S("rename fn_80003000 fn_801A73A8__4CBarFi")));
    CHECK(!any_line(&s->fake.calls, "neutral")); /* a new header: no other unit includes it */
    CHECK(any_line(&lines, "neutral: not needed (a new header only this unit includes)"));
    static const char *const CALLS[] = {"unit new game/CBar.cpp --text 80003000-80003010",
                                        "rename fn_80003000 fn_801A73A8__4CBarFi",
                                        "ninja build.ninja",
                                        "match main/game/CBar",
                                        "ninja build/R5WEA4/src/game/CBar.o",
                                        "gatecheck layout main/game/CBar",
                                        "check_rules --units main/game/CBar"};
    CHECK_EQ_U64(s->fake.calls.n, 7);
    for (size_t i = 0; i < 7 && i < s->fake.calls.n; i++) CHECK_STR(s->fake.calls.v[i], CALLS[i]);
}

AVP_TEST(undo_on_failed_check) {
    Setup *s = setup("undo");
    AvpArena *a = avp_arena();
    s->fake.fail = "gatecheck";
    s->fake.undo_case = true;
    AvpStrVec lines = {0};
    Good g = {"MATCH", false, 0};
    CHECK(!run_promote(s, &g, false, &lines));
    bool failed_at = false;
    for (size_t i = 0; i < lines.n; i++)
        failed_at = failed_at || avp_str_startswith(lines.v[i], AVP_S("FAILED at gatecheck layout"));
    CHECK(failed_at);
    CHECK(any_line(&lines, "FAILED at gatecheck layout: gatecheck: it failed"));
    CHECK(!avp_exists(avp_join(a, s->repo.root, "src", "game", "CBar.cpp")));
    CHECK_EQ_U64(s->fake.dirty.n, 0);
    CHECK_STR(lines.v[lines.n - 1], "undone: 5 path(s) restored (config/R5WEA4/splits.txt, config/R5WEA4/symbols.txt, "
                                    "configure.py, include/game/CBar.h, src/game/CBar.cpp)");
}

AVP_TEST(meta_range_and_refusals) {
    /* no address comment: the draft's meta.json unit_range, else a refusal */
    Setup *s = setup("meta");
    AvpArena *a = avp_arena();
    avp_write_text(s->src, AVP_S("void f( void ) {\n}\n"), AVP_TM_UTF8);
    AvpStrVec lines = {0};
    Good g = {"PARTIAL", false, 0};
    CHECK(!run_promote(s, &g, false, &lines));
    CHECK_STR(lines.v[lines.n - 1], "REFUSED: no // 0x address comment in the draft and no --text");
    avp_write_text(avp_path_join(a, s->repo.root, "meta.json"),
                   AVP_S("{\"unit_range\": [\"0x80004000\", \"0x80004020\"]}"), AVP_TM_UTF8);
    lines = (AvpStrVec){0};
    CHECK(!run_promote(s, &g, false, &lines));
    CHECK_STR(lines.v[lines.n - 1], "REFUSED: the draft does not grade MATCH without findings over 80004000-80004020");
}

/* the one-function guard: a derived range of one function needs an edge or a high cut on both sides */
AVP_TEST(refuses_small_unit_without_high_cuts) {
    Setup *s = setup("small");
    s->fake.weak = true;
    AvpStrVec lines = {0};
    Good g = {"MATCH", false, 0};
    CHECK(!run_promote(s, &g, true, &lines));
    CHECK(avp_str_startswith(lines.v[lines.n - 1],
                             AVP_S("REFUSED: 80003000-80003010 holds only one function (fn_80003000) and is not a "
                                   "configured split; unit_plan confidence at its start: medium, at its end: medium "
                                   "(high needed on both).")));
    CHECK(contains(lines.v[lines.n - 1], "Pass --allow-small-unit"));
    CHECK(!any_line(&s->fake.calls, "unit new"));
    /* a block edge is no cut */
    s = setup("small_edges");
    s->fake.edges = true;
    lines = (AvpStrVec){0};
    CHECK(!run_promote(s, &g, true, &lines));
    CHECK(contains(lines.v[lines.n - 1], "at its start: block edge, at its end: block edge"));
}

AVP_TEST(small_unit_override_and_exemptions) {
    AvpStrVec lines = {0};
    Good g = {"MATCH", false, 0};
    /* the override */
    Setup *s = setup("small_allow");
    s->fake.weak = true;
    CHECK(draft_promote(&s->W, "game/CBar.cpp", s->src, s->hdr, false, 0, 0, true, true, &s->pr, good, &g, &lines));
    /* an explicit --text range is the caller's own evidence */
    s = setup("small_text");
    s->fake.weak = true;
    lines = (AvpStrVec){0};
    CHECK(draft_promote(&s->W, "game/CBar.cpp", s->src, s->hdr, true, 0x80003000, 0x80003010, true, false, &s->pr, good,
                        &g, &lines));
    /* more than one function: not a one-function unit */
    s = setup("small_many");
    s->fake.weak = true;
    Good two = {"MATCH", false, 1};
    lines = (AvpStrVec){0};
    CHECK(run_promote(s, &two, true, &lines));
    /* an already-configured split */
    s = setup("small_configured");
    s->fake.weak = true;
    s->fake.configured = true;
    lines = (AvpStrVec){0};
    CHECK(run_promote(s, &g, true, &lines));
    /* the default fake (a high cut on both sides) passes without the override */
    s = setup("small_high");
    lines = (AvpStrVec){0};
    CHECK(run_promote(s, &g, true, &lines));
}

/* Promoter.rename's names.py argv: --local for a name that exists only as scope:local copies, --symbols-only for a
 * static initialiser (a generated name has no source spelling to edit). */
AVP_TEST(rename_argv) {
    AvpArena *a = avp_arena();
    Setup *s = setup("rename_argv");
    s->repo.symbols_txt = avp_path_join(a, s->repo.root, "symbols.txt");
    avp_write_text(s->repo.symbols_txt,
                   AVP_S("displayHeight = .sbss:0x80530000; // type:object size:0x4 scope:local\n"
                         "fn_801A73A8__4CBarFi = .text:0x801A73A8; // type:function size:0x10 scope:global\n"),
                   AVP_TM_UTF8);
    draft_promoter_rename(&s->pr, "fn_80003000", "fn_801A73A8__4CBarFi");
    draft_promoter_rename(&s->pr, "lbl_80530004", "displayHeight");
    draft_promoter_rename(&s->pr, "fn_80003100", "__sinit_\\CBar_cpp");
    REQUIRE(s->fake.calls.n == 3);
    const char *root = s->repo.root;
    CHECK(avp_str_eq(s->fake.calls.v[0],
                     avp_str(avp_sprintf(a, "names rename fn_80003000 fn_801A73A8__4CBarFi --apply --root %s", root))));
    CHECK(avp_str_eq(s->fake.calls.v[1],
                     avp_str(avp_sprintf(a, "names rename lbl_80530004 displayHeight --apply --root %s --local", root))));
    CHECK(avp_str_eq(s->fake.calls.v[2],
                     avp_str(avp_sprintf(a, "names rename fn_80003100 __sinit_\\CBar_cpp --apply --root %s --symbols-only",
                                         root))));
}

AVP_TEST_MAIN("draft_promote")
