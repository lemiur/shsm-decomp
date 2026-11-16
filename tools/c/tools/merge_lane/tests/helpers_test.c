/* TestHelpers and TestToolResolution of tools/test_merge_lane.py.
 *   TestHelpers.test_parse_objects_ignores_comments          -> parse_objects_ignores_comments
 *   TestHelpers.test_unit_of                                 -> unit_of
 *   TestHelpers.test_classify_conflict                       -> classify_conflict
 *   TestHelpers.test_func_scores_pairs_a_rename_by_address   -> func_scores_pairs_a_rename_by_address
 *       (merge_lane.py's func_scores is gate_regress's gate_func_scores in the port)
 *   TestHelpers.test_merge_by_symbol_joins_units_and_takes_the_best -> merge_by_symbol_joins_units_and_takes_the_best
 *       (gate_merge_by_symbol)
 *   TestToolResolution.test_local_copy_wins                  -> local_copy_wins
 *       differs: shutil.which is not patched; PATH is set to a folder holding a ninja.exe for the call
 *   TestToolResolution.test_falls_back_to_path               -> falls_back_to_path
 *       differs: as above; the result is compared after normcase(realpath()) (avp_which builds the path itself)
 *   TestToolResolution.test_missing_everywhere_fails_with_both_places_named -> missing_everywhere_fails_with_both_places_named
 *       differs: PATH is set to an empty folder instead of patching which; the Ctx is an Ml built by hand
 *   TestToolResolution.test_default_commands_do_not_hard_code_the_binary -> default_commands_do_not_hard_code_the_binary
 *       differs: the port has no DEFAULT_COMMANDS argv table (ninja is resolved in ml_default_cmd); checked through
 *       behaviour instead: run in two checkouts without a ninja, ninja and ninja_changes each name that checkout's own
 *       tools/bin/ninja.exe, never a fixed path */
#include "ml_test_fixtures.h"

AVP_TEST(parse_objects_ignores_comments) {
    const char *t = "    Object(Matching, \"x/y.c\"),\n    # Object(Matching, \"z.c\"),\n    Object(NonMatching, \"w.cpp\"),\n";
    AvpMap *m = ml_parse_objects(TA, avp_str(t));
    REQUIRE(m);
    CHECK_EQ_U64(avp_map_len(m), 2);
    AvpVal *v1 = avp_map_get(m, AVP_S("x/y.c")), *v2 = avp_map_get(m, AVP_S("w.cpp"));
    REQUIRE(v1 && v2);
    CHECK_CZ((const char *)v1->p, "Matching");
    CHECK_CZ((const char *)v2->p, "NonMatching");
}

AVP_TEST(unit_of) { CHECK_CZ(ml_unit_of(TA, AVP_S("game/PadRumble.cpp")), "main/game/PadRumble"); }

AVP_TEST(classify_conflict) {
    CHECK_CZ(ml_classify_conflict(AVP_S("config/R5WEA4/baseline_report.json")), "generated");
    CHECK_CZ(ml_classify_conflict(AVP_S("docs/progress.md")), "ledger");
    CHECK_CZ(ml_classify_conflict(AVP_S("docs/exceptions.md")), "exceptions");
    CHECK_CZ(ml_classify_conflict(AVP_S("src/game/X.cpp")), "manual");
    CHECK_CZ(ml_classify_conflict(AVP_S("include/X.h")), "manual");
}

static bool addr_is(const GateScore *e, const char *want) {
    if (!want) return e->addr == NULL || avp_json_is_null(e->addr);
    return e->addr && avp_json_is_str(e->addr) && avp_str_eqz(e->addr->u.str, want);
}

static const GateScore *score_at(const GateScores *s, const char *unit, const char *addr) {
    for (size_t i = 0; i < s->n; i++)
        if (!strcmp(s->v[i].unit, unit) && addr_is(&s->v[i], addr)) return &s->v[i];
    return NULL;
}

AVP_TEST(func_scores_pairs_a_rename_by_address) {
    AvpJson *doc = tx_json(
        "{\"units\": [{\"name\": \"main/a\", \"functions\": ["
        "{\"name\": \"fn_old\", \"from\": {\"fuzzy_match_percent\": 90.0}, \"metadata\": {\"virtual_address\": \"100\"}},"
        "{\"name\": \"fn_new\", \"to\": {\"fuzzy_match_percent\": 95.0}, \"metadata\": {\"virtual_address\": \"100\"}},"
        "{\"name\": \"fn_z\", \"from\": {\"fuzzy_match_percent\": 50.0}, \"to\": {}, "
        "\"metadata\": {\"virtual_address\": \"200\"}}]}]}");
    GateScores s = gate_func_scores(TA, doc);
    const GateScore *e = score_at(&s, "main/a", "100");
    REQUIRE(e);
    CHECK_CZ(e->onto_name, "fn_old");
    CHECK_CZ(e->name, "fn_new");
    CHECK(e->has_from && e->from == 90.0);
    CHECK(e->has_to && e->to == 95.0);
    const GateScore *z = score_at(&s, "main/a", "200");
    REQUIRE(z);
    CHECK(z->has_to && z->to == 0.0); /* missing fuzzy field means 0 like changes_fmt */
}

static const GateScore *merged_at(const GateScores *m, bool has_addr, i64 addr, const char *unit) {
    for (size_t i = 0; i < m->n; i++) {
        const GateScore *e = &m->v[i];
        if (e->has_addr_int != has_addr || strcmp(e->unit, unit)) continue;
        if (!has_addr || e->addr_int == addr) return e;
    }
    return NULL;
}

AVP_TEST(merge_by_symbol_joins_units_and_takes_the_best) {
    AvpJson *doc = tx_json(
        "{\"units\": ["
        "{\"name\": \"main/auto_old\", \"functions\": ["
        "{\"name\": \"fn\", \"from\": {\"fuzzy_match_percent\": 80.0}, \"metadata\": {\"virtual_address\": \"100\"}},"
        "{\"name\": \"lbl\", \"from\": {\"fuzzy_match_percent\": 10.0}, \"metadata\": {\"virtual_address\": \"300\"}}]},"
        "{\"name\": \"main/auto_new\", \"functions\": ["
        "{\"name\": \"fn\", \"to\": {\"fuzzy_match_percent\": 85.0}, \"metadata\": {\"virtual_address\": \"100\"}}]},"
        "{\"name\": \"main/b\", \"functions\": ["
        "{\"name\": \"lbl\", \"from\": {\"fuzzy_match_percent\": 20.0}, \"to\": {\"fuzzy_match_percent\": 20.0}}]}]}");
    GateScores scores = gate_func_scores(TA, doc);
    GateScores m = gate_merge_by_symbol(TA, &scores);
    const GateScore *e = merged_at(&m, true, 100, "main/auto_old");
    REQUIRE(e);
    CHECK(e->has_from && e->from == 80.0);
    CHECK(e->has_to && e->to == 85.0);
    CHECK_CZ(e->unit_lane, "main/auto_new");
    /* no address: keyed by (unit, name), so same-named locals in different units stay apart */
    CHECK_EQ_U64(m.n, 3);
    const GateScore *b = merged_at(&m, false, 0, "main/b");
    REQUIRE(b);
    CHECK(b->has_from && b->from == 20.0);
    const GateScore *l = merged_at(&m, true, 300, "main/auto_old");
    REQUIRE(l);
    CHECK(!l->has_to);
}

/* ---- TestToolResolution ----------------------------------------------------------------------------------------- */
static void touch(const char *path) { mlfx_write(avp_cz(TA, avp_path_dirname(path)), avp_cz(TA, avp_path_basename(path)), AVP_S("")); }

/* PATH set to dir for one call, then restored. */
typedef struct PathSave {
    const char *path;
} PathSave;
static PathSave path_set(const char *dir) {
    PathSave s = {avp_getenv(TA, "PATH")};
    avp_setenv("PATH", dir);
    return s;
}
static void path_restore(PathSave s) {
    if (s.path) avp_setenv("PATH", s.path);
    else avp_unsetenv("PATH");
}

AVP_TEST(local_copy_wins) {
    const char *tmp = avp_mkdtemp(TA, "merge_lane_tool_", "", NULL);
    const char *local = avp_path_joinv(TA, 4, (const char *const[]){tmp, "tools", "bin", "ninja.exe"});
    touch(local);
    const char *elsewhere = avp_path_join(TA, tmp, "elsewhere");
    touch(avp_path_join(TA, elsewhere, "ninja.exe"));
    PathSave ps = path_set(elsewhere);
    const char *r = ml_resolve_tool(TA, tmp, "ninja");
    path_restore(ps);
    TX_EQ_CZ(r, local);
    avp_rmtree(tmp, AVP_RMTREE_CLEAR_READONLY | AVP_RMTREE_IGNORE_ERRORS);
}

AVP_TEST(falls_back_to_path) {
    const char *tmp = avp_mkdtemp(TA, "merge_lane_tool_", "", NULL);
    const char *somewhere = avp_path_join(TA, tmp, "some where");
    const char *exe = avp_path_join(TA, somewhere, "ninja.exe");
    touch(exe);
    PathSave ps = path_set(somewhere);
    const char *r = ml_resolve_tool(TA, tmp, "ninja");
    path_restore(ps);
    CHECK_MSG(r && ml_norm_eq(TA, r, exe), "resolve_tool: %s, want %s", r ? r : "(null)", exe);
    avp_rmtree(tmp, AVP_RMTREE_CLEAR_READONLY | AVP_RMTREE_IGNORE_ERRORS);
}

static Ml *ctx_for(const char *tmp) {
    /* Ctx(parse_args(["--slot", tmp, "--branch", "b", "--log-dir", tmp/logs])) */
    Ml *ml = AVP_NEW(TA, Ml);
    ml->a = TA;
    ml->slot = tmp, ml->branch = "b", ml->onto = "main", ml->repo = tmp;
    ml->log_dir = avp_path_join(TA, tmp, "logs");
    ml->passed = avp_set_new();
    return ml;
}

AVP_TEST(missing_everywhere_fails_with_both_places_named) {
    const char *tmp = avp_mkdtemp(TA, "merge_lane_tool_", "", NULL);
    const char *empty = avp_path_join(TA, tmp, "empty");
    avp_makedirs(empty, true);
    Ml *ml = ctx_for(tmp);
    PathSave ps = path_set(empty);
    AvpStr text = AVP_S("");
    i64 rc = ml_run_cmd(ml, "ninja_changes", "x.log", NULL, tmp, &text);
    path_restore(ps);
    CHECK_EQ_I64(rc, 127);
    CHECK_MSG(tx_has(text, "ninja not found"), "text: %.*s", AVP_SF(text));
    const char *want = avp_path_joinv(TA, 4, (const char *const[]){tmp, "tools", "bin", "ninja.exe"});
    CHECK_MSG(tx_has(text, want), "\"%s\" not in: %.*s", want, AVP_SF(text));
    avp_rmtree(tmp, AVP_RMTREE_CLEAR_READONLY | AVP_RMTREE_IGNORE_ERRORS);
}

AVP_TEST(default_commands_do_not_hard_code_the_binary) {
    const char *tmp = avp_mkdtemp(TA, "merge_lane_tool_", "", NULL);
    const char *empty = avp_path_join(TA, tmp, "empty");
    avp_makedirs(empty, true);
    const char *keys[2] = {"ninja", "ninja_changes"};
    const char *roots[2] = {avp_path_join(TA, tmp, "one"), avp_path_join(TA, tmp, "two")};
    PathSave ps = path_set(empty);
    for (int r = 0; r < 2; r++) {
        avp_makedirs(roots[r], true);
        Ml *ml = ctx_for(roots[r]);
        for (int k = 0; k < 2; k++) {
            AvpStr text = AVP_S("");
            i64 rc = ml_run_cmd(ml, keys[k], "x.log", NULL, roots[r], &text);
            CHECK_EQ_I64(rc, 127);
            const char *own = avp_path_joinv(TA, 4, (const char *const[]){roots[r], "tools", "bin", "ninja.exe"});
            const char *other = avp_path_joinv(TA, 4, (const char *const[]){roots[1 - r], "tools", "bin", "ninja.exe"});
            CHECK_MSG(tx_has(text, own) && !tx_has(text, other), "%s in %s: %.*s", keys[k], roots[r], AVP_SF(text));
        }
    }
    path_restore(ps);
    avp_rmtree(tmp, AVP_RMTREE_CLEAR_READONLY | AVP_RMTREE_IGNORE_ERRORS);
}

AVP_TEST_MAIN("merge_lane_helpers")
