/* baserule_test.c: base rule 1 (scan --base-rule ctor, classes_baserule.c) on synthetic listings, against the
 * reference rule on the same listings. The rule-0 expectations are the Python's output for these listings
 * (Analysis(Program.from_texts(...)).bases). */
#include "avp/test.h"
#include "classes_analysis.h"

static AvpStr S(const char *z) { return avp_str(z); }

/* `.fn name` with one row per instruction, 4 bytes apart from addr */
static void fn(AvpBuf *b, const char *name, unsigned addr, const char *const *rows, size_t n) {
    avp_buf_putz(b, avp_sprintf(b->a, ".fn %s, global\n", name));
    for (size_t i = 0; i < n; i++)
        avp_buf_putz(b, avp_sprintf(b->a, "/* %08X 00000000  00 00 00 00 */\t%s\n", addr + 4u * (unsigned)i, rows[i]));
    avp_buf_putz(b, avp_sprintf(b->a, ".endfn %s\n", name));
}
#define FN(b, name, addr, ...)                                                                                       \
    do {                                                                                                             \
        static const char *const rows_[] = {__VA_ARGS__};                                                            \
        fn(b, name, addr, rows_, sizeof rows_ / sizeof rows_[0]);                                                    \
    } while (0)

static void obj(AvpBuf *b, const char *name, unsigned addr, const char *const *words, size_t n) {
    avp_buf_putz(b, avp_sprintf(b->a, "# .data:0x0 | 0x%08X | size: 0x%X\n.obj %s, global\n", addr,
                                4u * (unsigned)n, name));
    for (size_t i = 0; i < n; i++) avp_buf_putz(b, avp_sprintf(b->a, "\t.4byte %s\n", words[i]));
    avp_buf_putz(b, avp_sprintf(b->a, ".endobj %s\n", name));
}
#define OBJ(b, name, addr, ...)                                                                                      \
    do {                                                                                                             \
        static const char *const words_[] = {__VA_ARGS__};                                                           \
        obj(b, name, addr, words_, sizeof words_ / sizeof words_[0]);                                                \
    } while (0)

/* A base at offset 0: CDer's constructor calls CBase's, then installs its own table over CBase's vptr. */
static AvpStr asm_zero(AvpArena *a) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, ".section .text\n");
    FN(&b, "__ct__5CBaseFv", 0x80001000u, "lis r4, __vt__5CBase@ha", "addi r4, r4, __vt__5CBase@l",
       "stw r4, 0x0(r3)", "blr");
    FN(&b, "fn_a", 0x80001030u, "blr");
    FN(&b, "fn_b", 0x80001034u, "blr");
    FN(&b, "__ct__4CDerFv", 0x80001100u, "mr r31, r3", "bl __ct__5CBaseFv", "lis r4, __vt__4CDer@ha",
       "addi r4, r4, __vt__4CDer@l", "stw r4, 0x0(r31)", "mr r3, r31", "blr");
    avp_buf_putz(&b, ".section .data\n");
    OBJ(&b, "__vt__5CBase", 0x80400000u, "0", "0", "fn_a", "fn_b");
    OBJ(&b, "__vt__4CDer", 0x80400010u, "0", "0", "fn_a", "fn_b");
    return avp_buf_str(&b);
}

/* A subobject at this+0x34 built by fn_sub, which installs its own table B (lbl_80400000) at +0x5C and a member's
 * table M (lbl_80400010) at +0x4 (as fn_80039E40 under CGhostPhotoTrigger). The outer class D (lbl_80400100):
 * fn_outer calls fn_sub on this+0x34, then installs D at 0 and (overwrite) D+0x10 at this+0x90, the subobject's
 * vptr; its destructor fn_dd calls fn_bd (B's destructor) on this+0x34 with r4 = dd_r4. With secondary, D's table
 * has the group for the subobject (words 4-7: B's image, slot 6 a thunk adjusting this by -0x34). */
static AvpStr asm_sub(AvpArena *a, const char *dd_r4, bool overwrite, bool secondary) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, ".section .text\n");
    FN(&b, "__dl__FPv", 0x80000F00u, "blr");
    FN(&b, "fn_sub", 0x80001000u, "lis r4, lbl_80400000@ha", "addi r4, r4, lbl_80400000@l",
       "lis r5, lbl_80400010@ha", "addi r5, r5, lbl_80400010@l", "stw r4, 0x5c(r3)", "stw r5, 0x4(r3)", "blr");
    FN(&b, "fn_bd", 0x80001040u, "cmpwi r3, 0x0", "mr r31, r4", "bl __dl__FPv", "blr");
    FN(&b, "fn_b1", 0x80001060u, "blr");
    FN(&b, "fn_m1", 0x80001064u, "blr");
    FN(&b, "fn_d1", 0x80001068u, "blr");
    FN(&b, "fn_th", 0x80001080u, "subi r3, r3, 0x34", "b fn_dd");
    const char *dd[] = {"cmpwi r3, 0x0", "mr r31, r4",  "mr r30, r3",      "addi r3, r30, 0x34",
                        NULL,            "bl fn_bd",    "mr r3, r30",      "bl __dl__FPv", "blr"};
    dd[4] = avp_sprintf(a, "li r4, %s", dd_r4);
    fn(&b, "fn_dd", 0x800010A0u, dd, 9);
    const char *outer[] = {"mr r31, r3", "addi r3, r31, 0x34", "bl fn_sub", "lis r3, lbl_80400100@ha",
                           "addi r3, r3, lbl_80400100@l", "addi r0, r3, 0x10", "stw r3, 0x0(r31)",
                           "stw r0, 0x90(r31)", "mr r3, r31", "blr"};
    if (overwrite) {
        fn(&b, "fn_outer", 0x80001100u, outer, 10);
    } else {
        const char *no[] = {outer[0], outer[1], outer[2], outer[3], outer[4], outer[5], outer[6], outer[8], outer[9]};
        fn(&b, "fn_outer", 0x80001100u, no, 9);
    }
    avp_buf_putz(&b, ".section .data\n");
    OBJ(&b, "lbl_80400000", 0x80400000u, "0", "0", "fn_bd", "fn_b1");
    OBJ(&b, "lbl_80400010", 0x80400010u, "0", "0", "fn_m1");
    if (secondary)
        OBJ(&b, "lbl_80400100", 0x80400100u, "0", "0", "fn_dd", "fn_d1", "0", "0", "fn_th", "fn_b1");
    else
        OBJ(&b, "lbl_80400100", 0x80400100u, "0", "0", "fn_dd", "fn_d1");
    return avp_buf_str(&b);
}

static ClassesAnalysis *an_of(AvpArena *a, AvpStr text, int rule) {
    AvpStr unit = S("game/T");
    ClassesProgram *p = classes_program_from_texts(a, &unit, &text, 1, S(""));
    return classes_analysis_new_rule(a, p, NULL, rule);
}
static const ClassesBaseList *bases(ClassesAnalysis *an, const char *vt) {
    ClassesVtable *r = classes_an_vt(an, S(vt));
    return r ? &r->bases : NULL;
}
static bool has_ev(const ClassesBase *b, const char *ev) {
    for (size_t i = 0; i < b->evidence.n; i++)
        if (avp_str_eqz(b->evidence.v[i], ev)) return true;
    return false;
}

AVP_TEST(base_at_offset_zero) {
    AvpArena *a = avp_arena();
    for (int rule = 0; rule < 2; rule++) {
        const ClassesBaseList *bl = bases(an_of(a, asm_zero(a), rule), "__vt__4CDer");
        REQUIRE(bl != NULL && bl->n == 1);
        const ClassesBase *b = bl->v[0];
        CHECK_STR(b->base, "__vt__5CBase");
        CHECK(!b->base_is_name && b->has_offset && b->offset == 0 && b->has_word && b->word == 0);
        CHECK(has_ev(b, "ctor-call __ct__5CBaseFv"));
        CHECK(has_ev(b, "vptr-overwrite this+0x0 (__ct__5CBaseFv installs __vt__5CBase there)") == (rule == 1));
    }
}

AVP_TEST(base_at_nonzero_offset) {
    AvpArena *a = avp_arena();
    ClassesAnalysis *an = an_of(a, asm_sub(a, "0x0", true, true), 1);
    const ClassesBaseList *bl = bases(an, "lbl_80400100");
    REQUIRE(bl != NULL && bl->n == 1);
    const ClassesBase *b = bl->v[0];
    CHECK_STR(b->base, "lbl_80400000"); /* the table fn_sub installs at the overwritten vptr, not its owner M */
    CHECK(b->has_offset && b->offset == 0x34 && b->has_word && b->word == 4);
    REQUIRE(b->evidence.n == 4);
    CHECK_STR(b->evidence.v[0], "ctor-call fn_sub");
    CHECK_STR(b->evidence.v[1], "vptr-overwrite this+0x90 (fn_sub installs lbl_80400000 there)");
    CHECK_STR(b->evidence.v[2], "dtor-call fn_bd (r4=0)");
    CHECK_STR(b->evidence.v[3], "thunks this-0x34 (1 slots)");
    /* part 2: a secondary base (word 4) */
    AvpVal *v = avp_map_get(an->classes, S("vt:lbl_80400100"));
    REQUIRE(v != NULL);
    const ClassesRec *rec = v->p;
    REQUIRE(rec->bases.n == 1);
    CHECK(rec->bases.v[0].has_offset && rec->bases.v[0].offset == 0x34 && rec->bases.v[0].word == 4);
}

AVP_TEST(reference_rule_on_nonzero_offset) {
    /* the reference's result (the Python's): the callee's owner M does not image, B's complete_off is normalised to
     * 0, and slot overlap places B at the vptr's offset 0x90 */
    AvpArena *a = avp_arena();
    ClassesAnalysis *an = an_of(a, asm_sub(a, "0x0", true, true), 0);
    const ClassesBaseList *bl = bases(an, "lbl_80400100");
    REQUIRE(bl != NULL && bl->n == 1);
    const ClassesBase *b = bl->v[0];
    CHECK_STR(b->base, "lbl_80400000");
    CHECK(b->has_offset && b->offset == 0x90 && b->has_word && b->word == 4);
    REQUIRE(b->evidence.n == 1);
    CHECK_STR(b->evidence.v[0], "slot-overlap (1 slots)");
    CHECK(classes_an_vt(an, S("lbl_80400100"))->verdicts.n == 0);
}

AVP_TEST(member_object_same_call) {
    AvpArena *a = avp_arena();
    for (int rule = 0; rule < 2; rule++) {
        ClassesAnalysis *an = an_of(a, asm_sub(a, "-0x1", false, false), rule);
        ClassesVtable *r = classes_an_vt(an, S("lbl_80400100"));
        REQUIRE(r != NULL);
        CHECK(r->bases.n == 0);
        if (rule == 0) {
            CHECK(r->verdicts.n == 0 && r->member_objects.n == 0);
            continue;
        }
        REQUIRE(r->verdicts.n == 2);
        CHECK(r->verdicts.v[0].off == 0x34 && r->verdicts.v[0].tier == CLASSES_BR_VPTR);
        CHECK_STR(r->verdicts.v[0].ev, "ctor-call fn_sub (vptr not overwritten)");
        CHECK(r->verdicts.v[1].off == 0x34 && r->verdicts.v[1].tier == CLASSES_BR_DTOR);
        CHECK_STR(r->verdicts.v[1].ev, "dtor-call fn_bd (r4=-1)");
        REQUIRE(r->member_objects.n == 2); /* fn_sub's tables at this+0x34+p */
        CHECK(r->member_objects.v[0].off == 0x90);
        CHECK_STR(r->member_objects.v[0].vt, "lbl_80400000");
        CHECK(r->member_objects.v[1].off == 0x38);
        CHECK_STR(r->member_objects.v[1].vt, "lbl_80400010");
    }
}

AVP_TEST(conflict_keeps_vptr_evidence) {
    /* overwritten vptr (a base) but the destructor passes r4 = -1 (a member): the stronger evidence decides */
    AvpArena *a = avp_arena();
    const ClassesBaseList *bl = bases(an_of(a, asm_sub(a, "-0x1", true, true), 1), "lbl_80400100");
    REQUIRE(bl != NULL && bl->n == 1);
    CHECK_STR(bl->v[0]->base, "lbl_80400000");
    CHECK(bl->v[0]->offset == 0x34);
    CHECK(has_ev(bl->v[0], "conflict: dtor-call fn_bd (r4=-1)"));
    CHECK(!has_ev(bl->v[0], "dtor-call fn_bd (r4=0)"));
}

/* A dtk-split base table B (lbl_80400000) that runs one null word past its two slots, to the next object's
 * alignment (as lbl_803E2548). D's constructor calls fn_bct and installs D (four words) over B's vptr: B at word 0.
 * X's constructor calls fn_bct on this+0x10 and installs X+0xC there: B's group at word 3, followed by X's own slot
 * fn_x2 at word 7 (as unk_8003C454's own slots after its 0x1BC group). */
static AvpStr asm_pad(AvpArena *a) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, ".section .text\n");
    FN(&b, "fn_bct", 0x80001000u, "lis r4, lbl_80400000@ha", "addi r4, r4, lbl_80400000@l", "stw r4, 0x0(r3)", "blr");
    FN(&b, "fn_b1", 0x80001010u, "blr");
    FN(&b, "fn_b2", 0x80001014u, "blr");
    FN(&b, "fn_d2", 0x80001018u, "blr");
    FN(&b, "fn_x1", 0x8000101Cu, "blr");
    FN(&b, "fn_x2", 0x80001020u, "blr");
    FN(&b, "fn_dct", 0x80001100u, "mr r31, r3", "bl fn_bct", "lis r4, lbl_80400100@ha",
       "addi r4, r4, lbl_80400100@l", "stw r4, 0x0(r31)", "mr r3, r31", "blr");
    FN(&b, "fn_xct", 0x80001200u, "mr r31, r3", "addi r3, r31, 0x10", "bl fn_bct", "lis r3, lbl_80400200@ha",
       "addi r3, r3, lbl_80400200@l", "addi r0, r3, 0xc", "stw r3, 0x0(r31)", "stw r0, 0x10(r31)", "mr r3, r31",
       "blr");
    avp_buf_putz(&b, ".section .data\n");
    OBJ(&b, "lbl_80400000", 0x80400000u, "0", "0", "fn_b1", "fn_b2", "0");
    OBJ(&b, "lbl_80400100", 0x80400100u, "0", "0", "fn_b1", "fn_d2");
    OBJ(&b, "lbl_80400200", 0x80400200u, "0", "0", "fn_x1", "0", "0", "fn_b1", "fn_b2", "fn_x2");
    return avp_buf_str(&b);
}

AVP_TEST(trailing_padding_of_a_base_table) {
    AvpArena *a = avp_arena();
    ClassesAnalysis *an = an_of(a, asm_pad(a), 1);
    CHECK_EQ_U64(classes_an_vt(an, S("lbl_80400000"))->words->n, 4); /* D ends after B's second slot */
    const ClassesBaseList *bl = bases(an, "lbl_80400100");
    REQUIRE(bl != NULL && bl->n == 1);
    CHECK_STR(bl->v[0]->base, "lbl_80400000");
    CHECK(bl->v[0]->offset == 0 && bl->v[0]->word == 0);
    bl = bases(an, "lbl_80400200");
    REQUIRE(bl != NULL && bl->n == 1);
    CHECK_STR(bl->v[0]->base, "lbl_80400000");
    CHECK(bl->v[0]->offset == 0x10 && bl->v[0]->word == 3);
    ClassesSpans sp = classes_an_spans(an, classes_an_vt(an, S("lbl_80400200")), 0);
    REQUIRE(sp.n >= 1);
    CHECK(sp.v[0].start == 3 && sp.v[0].end == 7); /* fn_x2 is X's own slot, not B's padding */
    /* the reference rule keeps the dtk extent */
    ClassesAnalysis *ref = an_of(a, asm_pad(a), 0);
    CHECK_EQ_U64(classes_an_vt(ref, S("lbl_80400000"))->words->n, 5);
}

/* G (lbl_80400000, installed by fn_gct) and P (lbl_80400100: G's first slot, its own override fn_p2 and new slot
 * fn_p3; installed nowhere, its constructor always inlined); C's constructor calls fn_gct and installs C, whose
 * table repeats P's slots: C derives from P, which the vptr evidence cannot see (AreaTriggerBox,
 * AreaTriggerBoxLogic). */
static AvpStr asm_inlined_parent(AvpArena *a) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, ".section .text\n");
    FN(&b, "fn_gct", 0x80001000u, "lis r4, lbl_80400000@ha", "addi r4, r4, lbl_80400000@l", "stw r4, 0x0(r3)", "blr");
    FN(&b, "fn_g1", 0x80001010u, "blr");
    FN(&b, "fn_g2", 0x80001014u, "blr");
    FN(&b, "fn_p2", 0x80001018u, "blr");
    FN(&b, "fn_p3", 0x8000101Cu, "blr");
    FN(&b, "fn_c4", 0x80001020u, "blr");
    FN(&b, "fn_cct", 0x80001100u, "mr r31, r3", "bl fn_gct", "lis r4, lbl_80400200@ha",
       "addi r4, r4, lbl_80400200@l", "stw r4, 0x0(r31)", "mr r3, r31", "blr");
    avp_buf_putz(&b, ".section .data\n");
    OBJ(&b, "lbl_80400000", 0x80400000u, "0", "0", "fn_g1", "fn_g2");
    OBJ(&b, "lbl_80400100", 0x80400100u, "0", "0", "fn_g1", "fn_p2", "fn_p3");
    OBJ(&b, "lbl_80400200", 0x80400200u, "0", "0", "fn_g1", "fn_p2", "fn_p3", "fn_c4");
    return avp_buf_str(&b);
}

AVP_TEST(inlined_parent_without_vptr_store) {
    AvpArena *a = avp_arena();
    ClassesAnalysis *an = an_of(a, asm_inlined_parent(a), 1);
    const ClassesBaseList *bl = bases(an, "lbl_80400100");
    REQUIRE(bl != NULL && bl->n == 1);
    CHECK_STR(bl->v[0]->base, "lbl_80400000"); /* P's base, by slot overlap */
    bl = bases(an, "lbl_80400200");
    REQUIRE(bl != NULL && bl->n == 1);
    const ClassesBase *b = bl->v[0];
    CHECK_STR(b->base, "lbl_80400100");
    CHECK(b->offset == 0 && b->word == 0);
    CHECK(has_ev(b, "ctor-call fn_gct"));
    CHECK(has_ev(b, "inlined-parent (grandparent vt:lbl_80400000 reached directly)"));
}

/* ---- the command line: ctor is scan's default (owner's ruling, 2026-11-13), slots on request ------------------- */
typedef struct {
    char *dir, *asm_dir, *syms, *out, *exe;
} Cli;

static Cli cli_setup(void) {
    AvpArena *a = avp_arena();
    Cli c;
    c.dir = avp_mkdtemp(a, "classes_baserule", NULL, NULL);
    AVP_ASSERT(c.dir != NULL);
    c.asm_dir = avp_path_join(a, c.dir, "asm");
    AVP_ASSERT(avp_makedirs(avp_path_join(a, c.asm_dir, "game"), true));
    AvpStr text = asm_sub(a, "0x0", true, true);
    AVP_ASSERT(avp_write_file(avp_join(a, c.asm_dir, "game", "T.s"), text.p, text.n));
    c.syms = avp_path_join(a, c.dir, "symbols.txt");
    AVP_ASSERT(avp_write_file(c.syms, "", 0));
    c.out = avp_path_join(a, c.dir, "classes.json");
    c.exe = avp_path_abspath(a, avp_path_join(a, avp_exe_dir(), "..\\classes.exe"));
    AVP_ASSERT(avp_exists(c.exe));
    AVP_ASSERT(avp_setenv("SHSM_CACHE_DIR", avp_path_join(a, c.dir, "cache"))); /* a private cache */
    avp_unsetenv("SHSM_CLASSES_CACHE");
    avp_unsetenv("SHSM_CLASSES_BASE_RULE");
    return c;
}

/* classes.exe --asm --symbols --out --splits <none> <cmd> [extra]: the last stdout line, or NULL on a failure */
static AvpStr cli_run(const Cli *c, const char *cmd, const char *extra, const char *extra2) {
    AvpArena *a = avp_arena();
    const char *argv[] = {c->exe, "--asm", c->asm_dir, "--symbols", c->syms, "--out", c->out, "--splits",
                          avp_path_join(a, c->dir, "no_splits.txt"), cmd, extra, extra2, NULL};
    AvpProcResult r = avp_run(&(AvpProcSpec){.argv = argv, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE, .text = true,
                                             .timeout = 120, .arena = a});
    CHECK_MSG(r.status == AVP_PROC_OK && r.returncode == 0, "classes.exe %s failed: %.*s", cmd, AVP_SF(r.err));
    AvpStrVec lines = avp_str_splitlines(avp_str_strip(r.out), false);
    return lines.n ? lines.v[lines.n - 1] : S("");
}

/* the written classes.json: the offset of lbl_80400100's base and its inputs.scanner */
static const char *cli_base_offset(const Cli *c, AvpStr *scanner) {
    AvpArena *a = avp_arena();
    AvpBytes raw = {0};
    AVP_ASSERT(avp_read_all(a, c->out, &raw));
    AvpJson *db = avp_json_parse(a, avp_strn((const char *)raw.v, raw.n), &(AvpJsonError){0});
    AVP_ASSERT(db != NULL);
    *scanner = avp_json_getz(avp_json_getz(db, "inputs"), "scanner")->u.str;
    const AvpJson *cl = avp_json_getz(db, "classes");
    for (size_t i = 0; i < cl->u.arr.n; i++) {
        const AvpJson *x = cl->u.arr.v[i];
        if (!avp_str_eqz(avp_json_getz(x, "key")->u.str, "vt:lbl_80400100")) continue;
        const AvpJson *b = avp_json_getz(x, "bases");
        if (b->u.arr.n != 1) return "(not one base)";
        return avp_cz(a, avp_json_getz(b->u.arr.v[0], "offset")->u.str);
    }
    return "(no class)";
}

AVP_TEST(cli_default_is_ctor) {
    Cli c = cli_setup();
    AvpStr sc;
    /* no option: the ctor rule (base at the subobject, 0x34), its own scanner value */
    CHECK(!avp_str_contains(cli_run(&c, "scan", NULL, NULL), S("(")));
    CHECK_CZ(cli_base_offset(&c, &sc), "0x34");
    CHECK(avp_str_endswith(sc, S("+base-rule=ctor")));
    CHECK(avp_str_contains(cli_run(&c, "scan", NULL, NULL), S("kept")));
    /* --base-rule slots: the Python's rule (slot overlap at the vptr, 0x90); the ctor file is not current for it */
    CHECK(!avp_str_contains(cli_run(&c, "scan", "--base-rule", "slots"), S("(")));
    CHECK_CZ(cli_base_offset(&c, &sc), "0x90");
    CHECK(!avp_str_contains(sc, S("+base-rule")));
    CHECK(avp_str_contains(cli_run(&c, "scan", "--base-rule", "slots"), S("kept")));
    /* each rule has its own cache entry */
    CHECK(avp_str_contains(cli_run(&c, "scan", NULL, NULL), S("restored from the cache")));
    CHECK_CZ(cli_base_offset(&c, &sc), "0x34");
    /* SHSM_CLASSES_BASE_RULE=slots changes the default; an explicit option still wins */
    AVP_ASSERT(avp_setenv("SHSM_CLASSES_BASE_RULE", "slots"));
    CHECK(avp_str_contains(cli_run(&c, "scan", NULL, NULL), S("restored from the cache")));
    CHECK_CZ(cli_base_offset(&c, &sc), "0x90");
    cli_run(&c, "scan", "--base-rule", "ctor");
    CHECK_CZ(cli_base_offset(&c, &sc), "0x34");
    /* a reader's quiet scan of a missing classes.json follows the default too */
    AVP_ASSERT(avp_remove(c.out));
    cli_run(&c, "tree", NULL, NULL);
    CHECK_CZ(cli_base_offset(&c, &sc), "0x90");
    avp_unsetenv("SHSM_CLASSES_BASE_RULE");
    AVP_ASSERT(avp_remove(c.out));
    cli_run(&c, "tree", NULL, NULL);
    CHECK_CZ(cli_base_offset(&c, &sc), "0x34");
    avp_unsetenv("SHSM_CACHE_DIR");
    avp_tempdir_cleanup(c.dir);
}

AVP_TEST_MAIN("classes_baserule")
