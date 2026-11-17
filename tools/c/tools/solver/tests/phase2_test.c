/* Pure unit tests of Phase 2 on synthetic inputs: the `dtk dwarf dump` text parser, the sibling-JSON locals reader
 * (nested blocks), the locals comparison (missing / extra / order / scope / type), the declaration-order
 * prediction, the candidate filter, and that a probe can never produce MATCH. */
#include "avp/test.h"
#include "solver.h"

static const char DUMP[] =
    "// total size: 0x10\n"
    "class MemBlockHeader {\n"
    "    // Members\n"
    "public:\n"
    "    int mSize; // offset 0x0, size 0x4\n"
    "};\n"
    "\n"
    "// Range: 0x4C8 -> 0x718\n"
    "// this: r25\n"
    "void MemPool::set(int blockoffset /* r26 */, int value /* r27 */, int numblocks /* r28 */) {\n"
    "    // Local variables\n"
    "    int byteoffset; // r31\n"
    "    int bitoffset; // r30\n"
    "    int count; // r0\n"
    "    int blocksleft; // r29\n"
    "}\n"
    "\n"
    "// Range: 0x93C -> 0xBE4\n"
    "static struct RwStream * SkinGeometryRead(struct RwStream * stream /* r25 */) {\n"
    "    // Local variables\n"
    "    struct RpSkin * skin; // r31\n"
    "    unsigned int skinInfo; // r1+0xC\n"
    "    unsigned char usedBoneList[256]; // r1+0x10\n"
    "    void (* cb)(int); // r28\n"
    "    static int counter; // fb+0x0\n"
    "\n"
    "    // References\n"
    "    // -> void * RwEngineInstance;\n"
    "}\n"
    "\n"
    "// Range: 0xEA4 -> 0xEAC\n"
    "static int SkinAtomicGetSize() {}\n";

AVP_TEST(dtk_dump_parser) {
    AvpArena *a = avp_arena();
    SvDwFns fns = sv_dtk_dump_parse(a, avp_strn(DUMP, sizeof DUMP - 1));
    REQUIRE(fns.n == 3);
    const SvDwFn *f = &fns.v[0];
    CHECK_CZ(f->name, "MemPool::set");
    CHECK_EQ_I64(f->lo, 0x4C8);
    CHECK_EQ_I64(f->hi, 0x718);
    CHECK_CZ(f->this_loc, "r25");
    REQUIRE(f->params.n == 3);
    CHECK_CZ(f->params.v[0].name, "blockoffset");
    CHECK_CZ(f->params.v[0].loc, "r26");
    CHECK_CZ(f->params.v[2].name, "numblocks");
    REQUIRE(f->locals.n == 4);
    CHECK_CZ(f->locals.v[0].name, "byteoffset");
    CHECK_CZ(f->locals.v[0].loc, "r31");
    CHECK_CZ(f->locals.v[0].type, "int");
    CHECK_CZ(f->locals.v[3].name, "blocksleft");
    const SvDwFn *g = &fns.v[1];
    CHECK_CZ(g->name, "SkinGeometryRead");
    REQUIRE(g->params.n == 1);
    CHECK_CZ(g->params.v[0].name, "stream");
    CHECK_CZ(g->params.v[0].type, "struct RwStream *");
    REQUIRE(g->locals.n == 4); /* the static local is no register variable */
    CHECK_CZ(g->locals.v[1].loc, "r1+0xC");
    CHECK_CZ(g->locals.v[2].name, "usedBoneList");
    CHECK_CZ(g->locals.v[3].name, "cb");
    CHECK_CZ(g->locals.v[3].loc, "r28");
    CHECK(fns.v[2].locals.n == 0 && fns.v[2].params.n == 0);
    /* lookups: by Range start, else by the last name component */
    CHECK(sv_dtk_find(&fns, 0x93C, NULL) == g);
    CHECK(sv_dtk_find(&fns, -1, "set") == f);
    CHECK(sv_dtk_find(&fns, 0x1234, "SkinGeometryRead") == g);
    CHECK(sv_dtk_find(&fns, -1, "nothing") == NULL);
    CHECK(sv_vars_find(&f->locals, "count") == &f->locals.v[2]);
    /* registers */
    CHECK_EQ_I64(sv_reg_num("r31"), 31);
    CHECK_EQ_I64(sv_reg_num("f30"), 130);
    CHECK_EQ_I64(sv_reg_num("r1+0xC"), -1);
    CHECK_EQ_I64(sv_reg_num("r32"), -1);
    CHECK(sv_reg_callee_saved(14) && sv_reg_callee_saved(131) && !sv_reg_callee_saved(13) && !sv_reg_callee_saved(0));
}

static const char SIB[] =
    "{\"function\": {\"name\": \"WorldSyncCamera\", \"tier\": \"exact\", \"offsets_reliable\": true,\n"
    " \"locals_only_in_debug\": [\"isAtomic\"],\n"
    " \"parameters\": [{\"name\": \"world\", \"type\": \"RpWorld *\", \"location\": \"r25\", \"line\": 10}],\n"
    " \"locals\": [\n"
    "  {\"name\": \"spaStack\", \"type\": \"RpSector *[64]\", \"location\": \"fb+0x50\", \"line\": 11},\n"
    "  {\"name\": \"spSect\", \"type\": \"RpSector *\", \"location\": \"r26\", \"line\": 12},\n"
    "  {\"block\": \"802DA390-802DA6B0\", \"locals\": [\n"
    "    {\"block\": \"802DA390-802DA390\", \"locals\": [\n"
    "      {\"name\": \"base\", \"type\": \"RwV3d *\", \"location\": \"r6\", \"line\": 20}]},\n"
    "    {\"name\": \"i\", \"type\": \"int\", \"location\": \"r5\", \"line\": 19}]}\n"
    " ]}}\n";

AVP_TEST(sibling_json_reader) {
    AvpArena *a = avp_arena();
    SvSibFn f;
    REQUIRE(sv_sibling_parse(a, avp_strn(SIB, sizeof SIB - 1), &f));
    CHECK_CZ(f.name, "WorldSyncCamera");
    CHECK_CZ(f.tier, "exact");
    CHECK(f.offsets_reliable);
    REQUIRE(f.params.n == 1);
    CHECK(f.params.v[0].is_param);
    REQUIRE(f.locals.n == 4);
    CHECK_CZ(f.locals.v[0].name, "spaStack");
    CHECK_EQ_I64(f.locals.v[0].depth, 0);
    CHECK_CZ(f.locals.v[2].name, "base");
    CHECK_EQ_I64(f.locals.v[2].depth, 2);
    CHECK_CZ(f.locals.v[2].block, "802DA390-802DA390");
    CHECK_CZ(f.locals.v[3].name, "i");
    CHECK_EQ_I64(f.locals.v[3].depth, 1);
    CHECK_CZ(f.locals.v[3].block, "802DA390-802DA6B0");
    CHECK_EQ_I64(f.locals.v[3].line, 19);
    REQUIRE(f.only_in_debug.n == 1);
    CHECK_CZ(f.only_in_debug.v[0], "isAtomic");
    SvSibFn e;
    CHECK(!sv_sibling_parse(a, AVP_S("{}"), &e));
    CHECK(!sv_sibling_parse(a, AVP_S("not json"), &e));
}

static SvVar var(const char *name, const char *type, i64 line, int depth, const char *block) {
    SvVar v = {0};
    v.name = name;
    v.type = type;
    v.line = line;
    v.depth = depth;
    v.block = block;
    return v;
}

static bool has(const SvLcFindings *f, SvLcKind k, const char *name) {
    for (size_t i = 0; i < f->n; i++)
        if (f->v[i].kind == k && !strcmp(f->v[i].name, name)) return true;
    return false;
}

AVP_TEST(locals_comparison) {
    AvpArena *a = avp_arena();
    SvVars ours = {0}, fin = {0}, dbg = {0};
    /* ours: a b c at function scope, x in a block, extra only in ours */
    avp_vec_push(&ours, var("a", "int", 1, 0, NULL));
    avp_vec_push(&ours, var("b", "RwUInt8 *", 2, 0, NULL));
    avp_vec_push(&ours, var("c", "int", 3, 0, NULL));
    avp_vec_push(&ours, var("x", "int", 5, 1, "blk"));
    avp_vec_push(&ours, var("extra", "int", 6, 0, NULL));
    for (size_t i = 0; i < ours.n; i++) ours.v[i].index = (i64)i;
    /* the DWARF: c before b (order), b typed void * (type), x at function scope (scope), m missing; Debug adds h */
    avp_vec_push(&fin, var("a", "int", 10, 0, NULL));
    avp_vec_push(&fin, var("c", "int", 11, 0, NULL));
    avp_vec_push(&fin, var("b", "void *", 12, 0, NULL));
    avp_vec_push(&fin, var("x", "int", 13, 0, NULL));
    avp_vec_push(&fin, var("m", "unsigned int", 14, 0, NULL));
    avp_vec_push(&dbg, var("h", "unsigned char *", 20, 1, "802A0000-802A0100"));
    SvLcFindings f = sv_locals_compare(a, &ours, &fin, &dbg, NULL);
    CHECK(has(&f, SV_LC_MISSING, "m"));
    CHECK(has(&f, SV_LC_MISSING, "h")); /* the Debug build's local counts */
    CHECK(has(&f, SV_LC_EXTRA, "extra"));
    CHECK(!has(&f, SV_LC_EXTRA, "a"));
    CHECK(has(&f, SV_LC_ORDER, "c"));
    CHECK(!has(&f, SV_LC_ORDER, "a"));
    CHECK(has(&f, SV_LC_SCOPE, "x"));
    CHECK(has(&f, SV_LC_TYPE, "b"));
    CHECK(!has(&f, SV_LC_TYPE, "a"));
    for (size_t i = 0; i < f.n; i++)
        if (f.v[i].kind == SV_LC_ORDER) CHECK_CZ(f.v[i].other, "b");
    /* our dtk types settle a typedef: no type finding when our resolved type is the DWARF's */
    SvVars types = {0};
    avp_vec_push(&types, var("b", "void *", 0, 0, NULL));
    SvLcFindings g = sv_locals_compare(a, &ours, &fin, &dbg, &types);
    CHECK(!has(&g, SV_LC_TYPE, "b"));
    /* type spelling: keywords and spaces do not count */
    CHECK(sv_types_same("struct RpSkin *", "RpSkin*"));
    CHECK(!sv_types_same("const RwFrustumPlane *", "RwFrustumPlane *"));
    /* no Debug build read: extra says so */
    SvLcFindings h = sv_locals_compare(a, &ours, &fin, NULL, NULL);
    bool said = false;
    for (size_t i = 0; i < h.n; i++)
        if (h.v[i].kind == SV_LC_EXTRA) said = said || strstr(h.v[i].text, "Debug not read") != NULL;
    CHECK(said);
}

AVP_TEST(declaration_order_prediction) {
    AvpArena *a = avp_arena();
    const char *why = NULL;
    const char *names[] = {"byteoffset", "bitoffset", "count", "blocksleft"};
    int regs[] = {31, 30, 0, 29};
    CHECK_EQ_I64(sv_order_model(a, names, regs, 4, &why), 1);
    int bad[] = {30, 31, 0, 29};
    CHECK_EQ_I64(sv_order_model(a, names, bad, 4, &why), 0);
    CHECK(why && strstr(why, "byteoffset") && strstr(why, "bitoffset"));
    int none[] = {0, 3, 4, -1};
    CHECK_EQ_I64(sv_order_model(a, names, none, 4, &why), -1);
    /* floating-point registers are their own class */
    const char *fn[] = {"x", "y", "i"};
    int fr[] = {131, 130, 31};
    CHECK_EQ_I64(sv_order_model(a, fn, fr, 3, &why), 1);
    /* the implied order: callee-saved only, highest register first */
    int retail[] = {30, 31, 3, 29};
    AvpCzVec o = sv_implied_order(a, names, retail, 4);
    REQUIRE(o.n == 3);
    CHECK_CZ(o.v[0], "bitoffset");
    CHECK_CZ(o.v[1], "byteoffset");
    CHECK_CZ(o.v[2], "blocksleft");
}

static const char ORIG[] = "void f(int *p) {\n    int a;\n    a = p[0] + p[1];\n    g(a);\n}\n";

static bool rejected(const char *cand, const char *reason) {
    AvpArena *a = avp_arena();
    AvpCzVec why = sv_candidate_rejects(a, avp_strn(ORIG, sizeof ORIG - 1), avp_str(cand), "f");
    bool hit = false;
    for (size_t i = 0; i < why.n; i++) hit = hit || !strcmp(why.v[i], reason);
    return hit;
}

AVP_TEST(candidate_filter) {
    AvpArena *a = avp_arena();
    CHECK(rejected("void f(int *p) {\n    volatile int a;\n    a = p[0] + p[1];\n    g(a);\n}\n", "volatile"));
    CHECK(rejected("void f(int *p) {\n    int a;\n    a = (int)p[0] + p[1];\n    g(a);\n}\n", "cast"));
    CHECK(rejected("void f(int *p) {\n#pragma optimization_level 1\n    int a;\n    a = p[0] + p[1];\n    g(a);\n}\n",
                   "pragma"));
    CHECK(rejected("void f(int *p) {\n    int a;\n    asm { nop }\n    a = p[0] + p[1];\n    g(a);\n}\n", "asm"));
    CHECK(rejected("void f(int *p) {\n    /* RULE-EXCEPTION: R30, order */\n    int a;\n    a = p[0] + p[1];\n"
                   "    g(a);\n}\n",
                   "RULE-EXCEPTION"));
    CHECK(rejected("void f(int *p) {\n    int a;\n    a = (*(const __typeof__(p) *)&p)[0] + p[1];\n    g(a);\n}\n",
                   "__typeof__"));
    /* a plain respelling passes */
    AvpCzVec ok = sv_candidate_rejects(a, avp_strn(ORIG, sizeof ORIG - 1),
                                       AVP_S("void f(int *p) {\n    int a;\n    a = p[1] + p[0];\n    g(a);\n}\n"), "f");
    CHECK_EQ_U64(ok.n, 0);
}

AVP_TEST(probe_never_matches) {
    AvpArena *a = avp_arena();
    /* a closing probe alone: NO-FORM-WITHIN-RULES, never MATCH */
    SvRecord *r = sv_record_new(a, "main/u", "f", SV_REGISTER);
    r->phase_built = true;
    SvProbe p = {0};
    p.lever = "decl-order: declare `j` before `i`";
    p.closes = true;
    avp_vec_push(&r->probes, p);
    CHECK(sv_assign_verdict(r) == SV_NO_FORM);
    /* a closing probe that opens another function: still NO-FORM-WITHIN-RULES */
    avp_vec_push(&r->probes.v[0].broken, "g");
    CHECK(sv_assign_verdict(r) == SV_NO_FORM);
    /* with an improving clean candidate: CLOSER from the candidate, not MATCH from the probe */
    SvCandidate c = {0};
    c.form = "reuse-local";
    c.compiled = true;
    c.rows_before = 5;
    c.rows_after = 3;
    avp_vec_push(&r->candidates, c);
    CHECK(sv_assign_verdict(r) == SV_CLOSER);
    /* an exact candidate that breaks another function: NO-FORM-WITHIN-RULES ("closes A, opens B") */
    SvRecord *s = sv_record_new(a, "main/u", "f", SV_REGISTER);
    SvCandidate d = {0};
    d.form = "dwarf-order";
    d.compiled = true;
    d.exact = true;
    d.rows_before = 5;
    d.rows_after = 0;
    avp_vec_push(&d.broken, "SkinAtomicRead");
    avp_vec_push(&s->candidates, d);
    CHECK(sv_assign_verdict(s) == SV_NO_FORM);
    /* an exact candidate with a new [match] finding: not MATCH */
    SvRecord *t = sv_record_new(a, "main/u", "f", SV_REGISTER);
    SvCandidate e = d;
    e.broken = (AvpCzVec){0};
    avp_vec_push(&e.rules, "f: 1 new [match] finding(s)");
    avp_vec_push(&t->candidates, e);
    CHECK(sv_assign_verdict(t) != SV_MATCH);
}

AVP_TEST(apply_edits) {
    AvpArena *a = avp_arena();
    LvEdit ed[2] = {{6, 7, AVP_S("X")}, {0, 0, AVP_S(">")}};
    LvS out;
    REQUIRE(sv_apply_edits(a, AVP_S("hello world"), ed, 2, &out));
    CHECK(avp_str_eqz(out, ">hello Xorld"));
    LvEdit bad[2] = {{0, 5, AVP_S("")}, {3, 4, AVP_S("")}};
    CHECK(!sv_apply_edits(a, AVP_S("hello world"), bad, 2, &out));
    CHECK_EQ_I64(sv_line_of(AVP_S("a\nb\nc"), 4), 3);
}

/* A type that differs only by a typedef name on our side is kept as evidence ("typedef (not compared)"), not a type
 * finding: ours is one identifier that is no builtin spelling, the DWARF spells a struct or a pointer. */
AVP_TEST(typedef_name_is_no_type_finding) {
    AvpArena *a = avp_arena();
    SvVars ours = {0}, fin = {0};
    avp_vec_push(&ours, var("atomic", "RpAtomicPtr", 1, 0, NULL)); /* a typedef name; the DWARF: a pointer */
    avp_vec_push(&ours, var("frame", "RwFramePtr", 2, 0, NULL));   /* the same; the DWARF: a struct spelling */
    avp_vec_push(&ours, var("count", "int", 3, 0, NULL));          /* builtin: a real difference */
    avp_vec_push(&ours, var("bytes", "RwUInt8 *", 4, 0, NULL));    /* not one identifier: a real difference */
    avp_vec_push(&fin, var("atomic", "struct RpAtomic *", 1, 0, NULL));
    avp_vec_push(&fin, var("frame", "struct RwFrame", 2, 0, NULL));
    avp_vec_push(&fin, var("count", "struct Count *", 3, 0, NULL));
    avp_vec_push(&fin, var("bytes", "void *", 4, 0, NULL));
    SvLcFindings f = sv_locals_compare(a, &ours, &fin, NULL, NULL);
    CHECK(has(&f, SV_LC_TYPEDEF, "atomic"));
    CHECK(has(&f, SV_LC_TYPEDEF, "frame"));
    CHECK(!has(&f, SV_LC_TYPE, "atomic"));
    CHECK(!has(&f, SV_LC_TYPE, "frame"));
    CHECK(has(&f, SV_LC_TYPE, "count"));
    CHECK(has(&f, SV_LC_TYPE, "bytes"));
    for (size_t i = 0; i < f.n; i++)
        if (f.v[i].kind == SV_LC_TYPEDEF) CHECK(strstr(f.v[i].text, "typedef (not compared)"));
    CHECK_CZ(SV_LC_KEYS[SV_LC_TYPEDEF], "dwarf-typedef");
}

AVP_TEST_MAIN("solver_phase2")
