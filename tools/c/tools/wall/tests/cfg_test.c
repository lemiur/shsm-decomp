/* Tests for wall_rows / wall_cfg: tools/test_wall.py TestControlFlow.test_cfg and TestRegister.test_abstraction,
 * plus the row helpers, graphs, summaries and match_cfgs results checked against the Python (values printed by
 * tools/wall.py on the same inputs). */
#include "avp/test.h"
#include "wall_cfg.h"
#include "wall_rows.h"
#include "wall_testrows.h"

static AvpStr dump(AvpJson *v) { return avp_json_dumps(avp_arena(), v, NULL); }

AVP_TEST(test_cfg) {
    AvpArena *a = avp_arena();
    CcFn *f = wall_test_fn(a, "cmpwi r3, 0x0\nbeq .L_4\nsubi r3, r3, 0x1\nb .L_0\nblr");
    WallCfg *g = wall_cfg_of_fn(a, f);
    REQUIRE(g->nblocks == 3);
    REQUIRE(g->nback_edges == 1);
    CHECK_EQ_I64(g->back_edges[0].u, 1);
    CHECK_EQ_I64(g->back_edges[0].v, 0);
    REQUIRE(g->blocks[0].nsucc == 2);
    CHECK_EQ_I64(g->blocks[0].succ[0], 2);
    CHECK_EQ_I64(g->blocks[0].succ[1], 1);
}

AVP_TEST(test_abstraction) {
    AvpArena *a = avp_arena();
    CHECK_STR(wall_abstract_regs(a, AVP_S("lwz r31, 0x8(r1)")), "lwz R, 0x8(r1)");
    CHECK_STR(wall_abstract_values(a, wall_abstract_regs(a, AVP_S("addi r3, r4, 0x10"))), "addi R, R, #");
    AvpStr *regs;
    size_t n = wall_regs_of(a, AVP_S("fmadds f1, f2, f3, f4"), &regs);
    REQUIRE(n == 4);
    CHECK_STR(regs[0], "f1");
    CHECK_STR(regs[3], "f4");
}

static void check_abs(const char *in, const char *regs, const char *vals, const char *regs_of) {
    AvpArena *a = avp_arena();
    AvpStr r = wall_abstract_regs(a, avp_str(in));
    CHECK_MSG(avp_str_eqz(r, regs), "abstract_regs(%s) = %.*s", in, AVP_SF(r));
    AvpStr v = wall_abstract_values(a, r);
    CHECK_MSG(avp_str_eqz(v, vals), "abstract_values(%s) = %.*s", in, AVP_SF(v));
    AvpStr *rv;
    size_t n = wall_regs_of(a, avp_str(in), &rv);
    AvpStr j = avp_str_join(a, AVP_S(","), rv, n);
    CHECK_MSG(avp_str_eqz(j, regs_of), "regs_of(%s) = %.*s", in, AVP_SF(j));
}

AVP_TEST(abstraction_cases) {
    check_abs("lwz r31, 0x8(r1)", "lwz R, 0x8(r1)", "lwz R, #(r1)", "r31");
    check_abs("stw r0, -0x4(r1)", "stw R, -0x4(r1)", "stw R, #(r1)", "r0");
    check_abs("cror cr1, cr2, cr3", "cror CR, CR, CR", "cror CR, CR, CR", "cr1,cr2,cr3");
    check_abs("lwz r3, lbl_80001000@sda21(r0)", "lwz R, lbl_80001000@sda21(R)", "lwz R, lbl_80001000@sda21(R)", "r3");
    check_abs("addi r3, r3, -12", "addi R, R, -12", "addi R, R, #", "r3,r3");
    check_abs("li r3,0x1g", "li R,0x1g", "li R,0x1g", "r3");
    check_abs("bl fn_80001000", "bl fn_80001000", "bl fn_80001000", "");
    check_abs("rlwinm r3, r4, 0, 16, 31", "rlwinm R, R, 0, 16, 31", "rlwinm R, R, #, #, #", "r3,r4");
    check_abs("lwz r3, r13x", "lwz R, r13x", "lwz R, r13x", "r3");
    check_abs("r3-4 #5 @6 x-7", "R-4 #5 @6 x-7", "R-# #5 @6 x-#", "r3");
}

AVP_TEST(save_helper_and_callee_saved) {
    AvpStr o;
    CHECK(wall_save_helper(AVP_S("_savegpr_24"), &o) && avp_str_eqz(o, "_savegpr"));
    CHECK(wall_save_helper(AVP_S("_restfpr_14"), &o) && avp_str_eqz(o, "_restfpr"));
    CHECK(wall_save_helper(AVP_S("_savegpr_24\n"), &o) && avp_str_eqz(o, "_savegpr"));
    CHECK(!wall_save_helper(AVP_S("_savegpr"), &o));
    CHECK(!wall_save_helper(AVP_S("_savegprx_1"), &o));
    CHECK(!wall_save_helper(AVP_S("_savegpr_"), &o));
    CHECK(!wall_save_helper(AVP_S(""), &o));
    CHECK(wall_callee_saved(AVP_S("r14")));
    CHECK(!wall_callee_saved(AVP_S("r13")));
    CHECK(wall_callee_saved(AVP_S("f31")));
    CHECK(!wall_callee_saved(AVP_S("cr2")));
    CHECK(!wall_callee_saved(AVP_S("r3")));
    CHECK(!wall_callee_saved(AVP_S("f0")));
    CHECK(wall_callee_saved(AVP_S("r99999999999999999999")));
    CHECK(!wall_callee_saved(AVP_S("R14")));
    CHECK(!wall_callee_saved(AVP_S("r14 ")));
}

AVP_TEST(helper_names) {
    const char *yes[] = {"_savegpr", "_savegpr_24", "__ptmf_scall", "__destroy_new_array", "__destroy_new_array2",
                         "__div2u", "__rtX", "__copy\n", "__dl__FPv"};
    const char *no[] = {"__ptmf_", "__div2x", "__rt", "foo"};
    for (size_t i = 0; i < sizeof yes / sizeof yes[0]; i++)
        CHECK_MSG(wall_is_helper(avp_str(yes[i])), "%s", yes[i]);
    for (size_t i = 0; i < sizeof no / sizeof no[0]; i++)
        CHECK_MSG(!wall_is_helper(avp_str(no[i])), "%s", no[i]);
    CHECK(wall_is_signed_pair(AVP_S("cmpw"), AVP_S("cmplw")));
    CHECK(wall_is_signed_pair(AVP_S("cmplwi"), AVP_S("cmpwi")));
    CHECK(!wall_is_signed_pair(AVP_S("cmpw"), AVP_S("cmpw")));
    CHECK(!wall_is_signed_pair(AVP_S("cmpw"), AVP_S("cmplwi")));
    CHECK(wall_is_compare(AVP_S("fcmpu")) && !wall_is_compare(AVP_S("cmpx")));
    CHECK_EQ_I64(wall_class_index(AVP_S("other")), 3);
    CHECK_EQ_I64(wall_class_index(AVP_S("nope")), -1);
}

AVP_TEST(r1_slots) {
    AvpStr s = AVP_S("stmw r27, -0x14(r1) lwz r1, 8(r1) x0x(r1)");
    size_t st, en, pos = 0;
    AvpStr num;
    REQUIRE(wall_r1_slot_find(s, pos, &st, &en, &num));
    CHECK_STR(num, "-0x14");
    REQUIRE(wall_r1_slot_find(s, en, &st, &en, &num));
    CHECK_STR(num, "8");
    CHECK(!wall_r1_slot_find(s, en, &st, &en, &num));
    CHECK(wall_uses_r1(AVP_S("lwz r3, 0x8(r1)")));
    CHECK(!wall_uses_r1(AVP_S("lwz r3, 0x8(r12)")));
}

AVP_TEST(lis_keep) {
    AvpArena *a = avp_arena();
    i64 s1[] = {3, 1, 2, 5, 4, 0, 6};
    bool *k = wall_lis_keep(a, s1, 7);
    bool e1[] = {false, true, true, false, true, false, true};
    for (int i = 0; i < 7; i++)
        CHECK_MSG(k[i] == e1[i], "index %d", i);
    i64 s2[] = {2, 2, 2};
    k = wall_lis_keep(a, s2, 3);
    CHECK(!k[0] && !k[1] && k[2]);
    i64 s3[] = {5, 4, 3};
    k = wall_lis_keep(a, s3, 3);
    CHECK(!k[0] && !k[1] && k[2]);
    (void)wall_lis_keep(a, NULL, 0);
}

static AvpStr kinds_of(CcFn *f) {
    AvpArena *a = avp_arena();
    AvpStr *v = AVP_NEWN(a, AvpStr, f->nrows);
    for (size_t i = 0; i < f->nrows; i++) {
        const char *k = wall_kind_name(wall_row_kind(&f->rows[i]));
        v[i] = avp_str(k ? k : "None");
    }
    return avp_str_join(a, AVP_S(","), v, f->nrows);
}

static AvpStr blocks_of(WallCfg *g) {
    AvpArena *a = avp_arena();
    AvpJson *arr = avp_json_arr(a);
    for (size_t i = 0; i < g->nblocks; i++)
        avp_json_arr_push(a, arr, wall_block_to_dict(a, &g->blocks[i]));
    return dump(arr);
}

static AvpStr loops_of(WallCfg *g) {
    AvpArena *a = avp_arena();
    AvpJson *arr = avp_json_arr(a);
    for (size_t i = 0; i < g->nloops; i++)
        avp_json_arr_push(a, arr, wall_loop_to_dict(a, &g->loops[i]));
    AvpJson *preds = avp_json_arr(a);
    for (size_t i = 0; i < g->nblocks; i++) {
        AvpJson *p = avp_json_arr(a);
        for (size_t j = 0; j < g->blocks[i].npreds; j++)
            avp_json_arr_push(a, p, avp_json_int(a, g->blocks[i].preds[j]));
        avp_json_arr_push(a, preds, p);
    }
    AvpJson *edges = avp_json_arr(a);
    for (size_t i = 0; i < g->nback_edges; i++) {
        AvpJson *e = avp_json_arr(a);
        avp_json_arr_push(a, e, avp_json_int(a, g->back_edges[i].u));
        avp_json_arr_push(a, e, avp_json_int(a, g->back_edges[i].v));
        avp_json_arr_push(a, edges, e);
    }
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("loops"), arr);
    avp_json_set(a, o, AVP_S("preds"), preds);
    avp_json_set(a, o, AVP_S("back"), edges);
    avp_json_set(a, o, AVP_S("switch"), avp_json_bool(g->approx_switch));
    avp_json_set(a, o, AVP_S("reach"), avp_json_int(a, (i64)g->nreachable));
    return dump(o);
}

typedef struct {
    const char *src, *kinds, *blocks, *rest, *summary;
} CfgCase;

static const CfgCase CASES[] = {
    {"cmpwi r3, 0x0\nbeq .L_4\nsubi r3, r3, 0x1\nb .L_0\nblr", "None,cond,None,goto,ret",
     "[{\"id\": 0, \"rows\": [0, 2], \"kind\": \"cond\", \"succ\": [2, 1]}, {\"id\": 1, \"rows\": [2, 4], \"kind\": "
     "\"goto\", \"succ\": [0]}, {\"id\": 2, \"rows\": [4, 5], \"kind\": \"ret\", \"succ\": []}]",
     "{\"loops\": [{\"header\": 0, \"blocks\": [0, 1], \"kind\": \"top-test\"}], \"preds\": [[1], [0], [0]], "
     "\"back\": [[1, 0]], \"switch\": false, \"reach\": 3}",
     "{\"blocks\": 3, \"edges\": 3, \"back_edges\": 1, \"loops\": {\"top-test\": 1}, \"exits\": 1, \"returns\": 1, "
     "\"switches\": 0, \"conds\": 1}"},
    {"li r4, 0x0\nb .L_4\naddi r4, r4, 0x1\nsubi r3, r3, 0x1\ncmpwi r3, 0x0\nbne .L_2\nblr",
     "None,goto,None,None,None,cond,ret",
     "[{\"id\": 0, \"rows\": [0, 2], \"kind\": \"goto\", \"succ\": [2]}, {\"id\": 1, \"rows\": [2, 4], \"kind\": "
     "\"fall\", \"succ\": [2]}, {\"id\": 2, \"rows\": [4, 6], \"kind\": \"cond\", \"succ\": [1, 3]}, {\"id\": 3, "
     "\"rows\": [6, 7], \"kind\": \"ret\", \"succ\": []}]",
     "{\"loops\": [{\"header\": 2, \"blocks\": [1, 2], \"kind\": \"bottom-test\"}], \"preds\": [[], [2], [0, 1], "
     "[2]], \"back\": [[1, 2]], \"switch\": false, \"reach\": 4}",
     "{\"blocks\": 4, \"edges\": 4, \"back_edges\": 1, \"loops\": {\"bottom-test\": 1}, \"exits\": 1, \"returns\": "
     "1, \"switches\": 0, \"conds\": 1}"},
    {"cmpwi r3, 0x0\nbeqlr\nbctr\nli r3, 0x1\nblr\nli r3, 0x2\nb fn_80001000\n",
     "None,condret,switch,None,ret,None,tail",
     "[{\"id\": 0, \"rows\": [0, 2], \"kind\": \"condret\", \"succ\": [1]}, {\"id\": 1, \"rows\": [2, 3], \"kind\": "
     "\"switch\", \"succ\": [2, 3]}, {\"id\": 2, \"rows\": [3, 5], \"kind\": \"ret\", \"succ\": []}, {\"id\": 3, "
     "\"rows\": [5, 7], \"kind\": \"tail\", \"succ\": []}]",
     "{\"loops\": [], \"preds\": [[], [0], [1], [1]], \"back\": [], \"switch\": true, \"reach\": 4}",
     "{\"blocks\": 4, \"edges\": 3, \"back_edges\": 0, \"loops\": {}, \"exits\": 3, \"returns\": 2, \"switches\": 1, "
     "\"conds\": 0}"},
    {"mtctr r3\nlbl:\nbdnz .L_1\nbl foo\nbeq bar\nb .L_1", "None,None,cond,call,condtail,goto",
     "[{\"id\": 0, \"rows\": [0, 1], \"kind\": \"fall\", \"succ\": [1]}, {\"id\": 1, \"rows\": [1, 3], \"kind\": "
     "\"cond\", \"succ\": [1, 2]}, {\"id\": 2, \"rows\": [3, 5], \"kind\": \"condtail\", \"succ\": [3]}, {\"id\": 3, "
     "\"rows\": [5, 6], \"kind\": \"goto\", \"succ\": [1]}]",
     "{\"loops\": [{\"header\": 1, \"blocks\": [1, 2, 3], \"kind\": \"no-exit\"}], \"preds\": [[], [0, 1, 3], [1], "
     "[2]], \"back\": [[1, 1], [3, 1]], \"switch\": false, \"reach\": 4}",
     "{\"blocks\": 4, \"edges\": 5, \"back_edges\": 2, \"loops\": {\"no-exit\": 1}, \"exits\": 1, \"returns\": 0, "
     "\"switches\": 0, \"conds\": 1}"},
};

AVP_TEST(cfg_cases) {
    AvpArena *a = avp_arena();
    for (size_t i = 0; i < sizeof CASES / sizeof CASES[0]; i++) {
        const CfgCase *c = &CASES[i];
        CcFn *f = wall_test_fn(a, c->src);
        AvpStr k = kinds_of(f);
        CHECK_MSG(avp_str_eqz(k, c->kinds), "case %zu kinds %.*s", i, AVP_SF(k));
        WallCfg *g = wall_cfg_of_fn(a, f);
        AvpStr b = blocks_of(g);
        CHECK_MSG(avp_str_eqz(b, c->blocks), "case %zu blocks %.*s", i, AVP_SF(b));
        AvpStr r = loops_of(g);
        CHECK_MSG(avp_str_eqz(r, c->rest), "case %zu rest %.*s", i, AVP_SF(r));
        AvpStr s = dump(wall_cfg_summary(a, g));
        CHECK_MSG(avp_str_eqz(s, c->summary), "case %zu summary %.*s", i, AVP_SF(s));
    }
}

/* the sim of eqcheck.pairings / classify: sum((Counter(norms of a) & Counter(norms of b)).values()) */
typedef struct {
    const WallCfg *ga, *gb;
} SimCtx;

static i64 sim_norms(void *ctx, int a, int b) {
    const SimCtx *c = ctx;
    const WallBlock *x = &c->ga->blocks[a], *y = &c->gb->blocks[b];
    size_t ny = y->end - y->start;
    bool used[64] = {0};
    i64 n = 0;
    for (size_t i = x->start; i < x->end; i++)
        for (size_t j = 0; j < ny && j < 64; j++)
            if (!used[j] && avp_str_eq(c->ga->rows[i].norm, c->gb->rows[y->start + j].norm)) {
                used[j] = true;
                n++;
                break;
            }
    return n;
}

static AvpStr iso_str(const WallIso *m) {
    AvpArena *a = avp_arena();
    if (m->status == WALL_ISO_BUDGET)
        return AVP_S("budget");
    if (m->status == WALL_ISO_NONE)
        return AVP_S("None");
    AvpJson *sw = avp_json_arr(a);
    for (size_t i = 0; i < m->nswaps; i++)
        avp_json_arr_push(a, sw, avp_json_int(a, m->swaps[i]));
    return avp_str_concat(a, dump(wall_iso_mapping_json(a, m)), avp_str_concat(a, AVP_S(" "), dump(sw)));
}

AVP_TEST(match_cfgs) {
    AvpArena *a = avp_arena();
    WallCfg *ga = wall_cfg_of_fn(a, wall_test_fn(a, "cmpwi r3, 0x0\nbeq .L_4\nli r3, 0x1\nblr\nli r3, 0x2\nblr"));
    WallCfg *gb = wall_cfg_of_fn(a, wall_test_fn(a, "cmpwi r3, 0x0\nbne .L_4\nli r3, 0x2\nblr\nli r3, 0x1\nblr"));
    WallIso m = wall_match_cfgs(a, ga, gb, WALL_MATCH_BUDGET, NULL, NULL);
    CHECK_STR(iso_str(&m), "{\"0\": 0, \"2\": 2, \"1\": 1} []");
    SimCtx sc = {ga, gb};
    WallIso ms = wall_match_cfgs(a, ga, gb, WALL_MATCH_BUDGET, sim_norms, &sc);
    CHECK_STR(iso_str(&ms), "{\"0\": 0, \"2\": 1, \"1\": 2} [0]");
    CHECK(!wall_iso_mapping_equal(&m, &ms));
    CHECK(wall_iso_mapping_equal(&m, &m));
    WallIso b1 = wall_match_cfgs(a, ga, gb, 1, NULL, NULL);
    CHECK_STR(iso_str(&b1), "budget");
    WallIso b2 = wall_match_cfgs(a, ga, gb, 2, NULL, NULL);
    CHECK_STR(iso_str(&b2), "budget");
    WallIso n1 = wall_match_cfgs(a, ga, wall_cfg_of_fn(a, wall_test_fn(a, "blr")), WALL_MATCH_BUDGET, NULL, NULL);
    CHECK_STR(iso_str(&n1), "None");
    WallCfg *e = wall_cfg_new(a, NULL, 0);
    WallIso e2 = wall_match_cfgs(a, e, e, WALL_MATCH_BUDGET, NULL, NULL);
    CHECK_STR(iso_str(&e2), "{} []");
    WallIso e1 = wall_match_cfgs(a, e, ga, WALL_MATCH_BUDGET, NULL, NULL);
    CHECK_STR(iso_str(&e1), "None");

    WallCfg *c1 = wall_cfg_of_fn(a, wall_test_fn(a, "cmpwi r3, 0x0\nbeq .L_3\nbl foo\ncmpwi r4, 0x0\nbeq .L_6\nbl bar\nblr"));
    WallCfg *c2 = wall_cfg_of_fn(
        a, wall_test_fn(a, "cmpwi r3, 0x0\nbne .L_3\nb .L_6\nbl foo\ncmpwi r4, 0x0\nbeq .L_7\nbl bar\nblr"));
    WallIso s1 = wall_match_cfgs(a, c1, c1, WALL_MATCH_BUDGET, NULL, NULL);
    CHECK_STR(iso_str(&s1), "{\"0\": 0, \"2\": 2, \"4\": 4, \"3\": 3, \"1\": 1} []");
    WallIso s2 = wall_match_cfgs(a, c1, c2, WALL_MATCH_BUDGET, NULL, NULL);
    CHECK_STR(iso_str(&s2), "None");
}

#include "cfg_fuzz_cases.h"

AVP_TEST(fuzz_cfg) {
    AvpArena *a = avp_arena();
    for (size_t i = 0; i < sizeof FUZZ_CFG / sizeof FUZZ_CFG[0]; i++) {
        WallCfg *g = wall_cfg_of_fn(a, wall_test_fn(a, FUZZ_CFG[i][0]));
        AvpStr b = blocks_of(g);
        CHECK_MSG(avp_str_eqz(b, FUZZ_CFG[i][1]), "cfg %zu blocks %.*s", i, AVP_SF(b));
        AvpJson *loops = avp_json_arr(a);
        for (size_t k = 0; k < g->nloops; k++)
            avp_json_arr_push(a, loops, wall_loop_to_dict(a, &g->loops[k]));
        AvpStr l = dump(loops);
        CHECK_MSG(avp_str_eqz(l, FUZZ_CFG[i][2]), "cfg %zu loops %.*s", i, AVP_SF(l));
        AvpStr s = dump(wall_cfg_summary(a, g));
        CHECK_MSG(avp_str_eqz(s, FUZZ_CFG[i][3]), "cfg %zu summary %.*s", i, AVP_SF(s));
    }
}

static void fuzz_iso(const char *const (*cases)[5], size_t n, i64 small_budget) {
    AvpArena *a = avp_arena();
    for (size_t i = 0; i < n; i++) {
        WallCfg *ga = wall_cfg_of_fn(a, wall_test_fn(a, cases[i][0]));
        WallCfg *gb = wall_cfg_of_fn(a, wall_test_fn(a, cases[i][1]));
        WallIso m = wall_match_cfgs(a, ga, gb, WALL_MATCH_BUDGET, NULL, NULL);
        AvpStr s = iso_str(&m);
        CHECK_MSG(avp_str_eqz(s, cases[i][2]), "pair %zu plain %.*s", i, AVP_SF(s));
        SimCtx sc = {ga, gb};
        m = wall_match_cfgs(a, ga, gb, WALL_MATCH_BUDGET, sim_norms, &sc);
        s = iso_str(&m);
        CHECK_MSG(avp_str_eqz(s, cases[i][3]), "pair %zu sim %.*s", i, AVP_SF(s));
        m = wall_match_cfgs(a, ga, gb, small_budget, NULL, NULL);
        s = iso_str(&m);
        CHECK_MSG(avp_str_eqz(s, cases[i][4]), "pair %zu budget %.*s", i, AVP_SF(s));
    }
}

AVP_TEST(fuzz_match_random) { fuzz_iso(FUZZ_ISO, sizeof FUZZ_ISO / sizeof FUZZ_ISO[0], 3); }
AVP_TEST(fuzz_match_inverted) { fuzz_iso(FUZZ_ISO2, sizeof FUZZ_ISO2 / sizeof FUZZ_ISO2[0], 6); }

AVP_TEST_MAIN("wall_cfg")
