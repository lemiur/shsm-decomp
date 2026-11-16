/* pytests_core_test.c: the Python unit tests of tools/test_classes.py that need no command line (ParseTest,
 * SameAddressTest, VtableTest, CtorBaseTest, MultiTest and RttiVtableTest), one C test per Python test with the
 * same name, the same synthetic listings and the same assertions, through the C API. The helpers fn / obj /
 * string_obj / lis_addi / build_single / build_multi / build_rtti_texts are the Python ones.
 *
 * Python's `C.Analysis(prog)` is classes_analysis_new(a, prog, NULL), `an.to_json()` classes_analysis_to_json(an,
 * NULL), `C.render_header` classes_header over a ClassesDb decoded from the document. Not ported 1:1: none. */
#include "avp/test.h"
#include "classes_analysis.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static AvpStr S(const char *z) { return avp_str(z); }
#define SP(...) avp_sprintf(avp_arena(), __VA_ARGS__)

/* ---- the synthetic listing builders (test_classes.py lines 23-154) -------------------------------------------- */

static i64 g_addr = 0x80010000; /* _addr: one counter for the whole process, as in the Python module */

static void fn_begin(AvpBuf *b, const char *name, const char *demangled) {
    bool q = strchr(name, '@') || strchr(name, '\\') || strchr(name, ',');
    if (demangled) avp_buf_printf(b, "# %s\n", demangled);
    avp_buf_printf(b, ".fn %s%s%s, global\n", q ? "\"" : "", name, q ? "\"" : "");
}
/* One instruction; a line starting with "." (a label) is copied without an address. */
static void I(AvpBuf *b, const char *ins) {
    if (ins[0] == '.') {
        avp_buf_printf(b, "%s\n", ins);
        return;
    }
    unsigned a = (unsigned)g_addr;
    g_addr += 4;
    avp_buf_printf(b, "/* %08X %08X  00 00 00 00 */\t%s\n", a, a - 0x80000000u, ins);
}
static void fn_end(AvpBuf *b, const char *name) { avp_buf_printf(b, ".endfn %s\n\n", name); }
/* fn(name, body) for a body of fixed strings. */
#define FN(b, name, dem, ...)                                                    \
    do {                                                                         \
        const char *body_[] = {__VA_ARGS__};                                     \
        fn_begin((b), (name), (dem));                                            \
        for (size_t i_ = 0; i_ < sizeof body_ / sizeof *body_; i_++) I((b), body_[i_]); \
        fn_end((b), (name));                                                     \
    } while (0)

static void lis_addi(AvpBuf *b, const char *reg, const char *sym) {
    I(b, SP("lis %s, %s@ha", reg, sym));
    I(b, SP("addi %s, %s, %s@l", reg, reg, sym));
}

/* obj(name, addr, words): an int word is written 0x%08X, a name as it is. */
static void obj_n(AvpBuf *b, const char *name, i64 addr, const char *const *w, size_t n) {
    avp_buf_printf(b, "# .data:0x0 | 0x%08X | size: 0x%X\n.obj %s, global\n", (unsigned)addr, (unsigned)(4 * n), name);
    for (size_t i = 0; i < n; i++) {
        if (isdigit((unsigned char)w[i][0])) avp_buf_printf(b, "\t.4byte 0x%08X\n", (unsigned)strtoul(w[i], NULL, 0));
        else avp_buf_printf(b, "\t.4byte %s\n", w[i]);
    }
    avp_buf_printf(b, ".endobj %s\n\n", name);
}
#define OBJ(b, name, addr, ...)                                                  \
    do {                                                                         \
        const char *w_[] = {__VA_ARGS__};                                        \
        obj_n((b), (name), (addr), w_, sizeof w_ / sizeof *w_);                  \
    } while (0)

static void string_obj(AvpBuf *b, const char *name, i64 addr, const char *text) {
    avp_buf_printf(b, "# .data:0x0 | 0x%08X | size: 0x%X\n.obj %s, global\n\t.string \"%s\"\n.endobj %s\n\n",
                   (unsigned)addr, (unsigned)(strlen(text) + 1), name, text, name);
}

static ClassesProgram *prog_of(const char *unit_name, AvpStr text) {
    AvpStr unit = S(unit_name);
    return classes_program_from_texts(avp_arena(), &unit, &text, 1, S(""));
}

/* CBase (vtable lbl_80300000) and CDerived : CBase (vtable lbl_80300010). */
static void single_text(AvpBuf *tp) {
    AvpBuf t = *tp;
    avp_buf_putz(&t, ".text\n");
    fn_begin(&t, "__ct__5CBaseFv", "CBase::CBase()");
    lis_addi(&t, "r4", "lbl_80300000");
    I(&t, "stw r4, 0x0(r3)"); I(&t, "li r0, 0x0"); I(&t, "stw r0, 0x4(r3)"); I(&t, "blr");
    fn_end(&t, "__ct__5CBaseFv");
    fn_begin(&t, "__dt__5CBaseFv", "CBase::~CBase()");
    I(&t, "cmpwi r3, 0x0"); I(&t, "mr r30, r3");
    lis_addi(&t, "r4", "lbl_80300000");
    I(&t, "stw r4, 0x0(r30)"); I(&t, "bl __dl__FPv"); I(&t, "mr r3, r30"); I(&t, "blr");
    fn_end(&t, "__dt__5CBaseFv");
    FN(&t, "fn_80010100__5CBaseFv", "CBase::fn_80010100()", "lwz r3, 0x4(r3)", "blr");
    /* derived constructor: base call, then its own vptr, then fields */
    fn_begin(&t, "fn_80020000", NULL);
    I(&t, "mflr r0"); I(&t, "mr r31, r3"); I(&t, "bl __ct__5CBaseFv");
    lis_addi(&t, "r4", "lbl_80300010");
    I(&t, "stw r4, 0x0(r31)"); I(&t, "lfs f0, lbl_80400000@sda21(r0)"); I(&t, "stfs f0, 0x8(r31)");
    I(&t, "li r0, 0x1"); I(&t, "sth r0, 0xc(r31)"); I(&t, "mr r3, r31"); I(&t, "blr");
    fn_end(&t, "fn_80020000");
    fn_begin(&t, "fn_80020100", NULL);
    I(&t, "cmpwi r3, 0x0"); I(&t, "mr r31, r3"); I(&t, "beq .L_x");
    lis_addi(&t, "r4", "lbl_80300010");
    I(&t, "stw r4, 0x0(r31)"); I(&t, "mr r3, r31"); I(&t, "li r4, 0x0"); I(&t, "bl __dt__5CBaseFv");
    I(&t, "mr r3, r31"); I(&t, "bl __dl__FPv");
    I(&t, ".L_x:"); I(&t, "mr r3, r31"); I(&t, "blr");
    fn_end(&t, "fn_80020100");
    /* an override reading fields: signed byte, signed half, float, and a pointer */
    FN(&t, "fn_80020200", NULL, "lbz r0, 0x10(r3)", "extsb r0, r0", "lha r4, 0xc(r3)", "lfs f1, 0x8(r3)",
       "lwz r5, 0x14(r3)", "lwz r6, 0x0(r5)", "blr");
    /* a second override that disagrees about 0x8 (an int) -> conflict */
    FN(&t, "fn_80020300", NULL, "lwz r0, 0x8(r3)", "blr");
    /* factory and registry call */
    FN(&t, "fn_80020400", NULL, "li r3, 0x18", "li r4, 0x2", "bl __nw__FUlii", "cmpwi r3, 0x0", "beq .L_y",
       "bl fn_80020000", ".L_y:", "blr");
    fn_begin(&t, "__sinit_\\Derived_cpp", NULL);
    lis_addi(&t, "r4", "lbl_80300100");
    lis_addi(&t, "r5", "fn_80020400");
    I(&t, "li r6, 0x18"); I(&t, "bl __ct__12unk_801A8F6CFv"); I(&t, "blr");
    fn_end(&t, "__sinit_\\Derived_cpp");
    avp_buf_putz(&t, ".data\n");
    OBJ(&t, "lbl_80300000", 0x80300000, "0", "0", "__dt__5CBaseFv", "fn_80010100__5CBaseFv");
    OBJ(&t, "lbl_80300010", 0x80300010, "0", "0", "fn_80020100", "fn_80020200", "fn_80020300");
    string_obj(&t, "lbl_80300100", 0x80300100, "CDerived");
    *tp = t;
}
static ClassesProgram *build_single(void) {
    AvpBuf t;
    avp_buf_init(&t, avp_arena());
    single_text(&t);
    return prog_of("game/test", avp_buf_str(&t));
}

/* stores(vt, reg): the five instructions of a constructor/destructor vptr block with three groups. */
static void stores(AvpBuf *b, const char *vt, const char *reg) {
    lis_addi(b, "r5", vt);
    I(b, "addi r4, r5, 0x8"); I(b, "addi r0, r5, 0x38");
    I(b, SP("stw r5, 0x2c(%s)", reg)); I(b, SP("stw r4, 0x0(%s)", reg)); I(b, SP("stw r0, 0x24(%s)", reg));
}

/* CHit : CBeh (vptr at 0x2C, groups +0x8 -> this+0, +0x38 -> this+0x24), MWCC layout. */
static ClassesProgram *build_multi(void) {
    AvpArena *a = avp_arena();
    AvpBuf t;
    avp_buf_init(&t, a);
    avp_buf_putz(&t, ".text\n");
    const char *beh[24], *hit[24];
    size_t nb = 0, nh = 0;
    beh[nb++] = "0"; beh[nb++] = "0"; beh[nb++] = "0"; beh[nb++] = "0"; beh[nb++] = "__dt__4CBehFv";
    hit[nh++] = "0"; hit[nh++] = "0"; hit[nh++] = "0"; hit[nh++] = "0"; hit[nh++] = "__dt__4CHitFv";
    for (int i = 0; i < 9; i++) {
        beh[nb++] = SP("fn_A%d", i);
        hit[nh++] = SP("fn_A%d", i);
    }
    beh[nb++] = "0"; beh[nb++] = "0"; beh[nb++] = "fn_TH1"; beh[nb++] = "fn_TH2"; beh[nb++] = "fn_M__4CBehFv";
    beh[nb++] = "__dt__4CBehFv";
    hit[nh++] = "0"; hit[nh++] = "0"; hit[nh++] = "@36@__dt__4CHitFv"; hit[nh++] = "fn_TH2";
    hit[nh++] = "fn_M__4CBehFv"; hit[nh++] = "__dt__4CHitFv"; hit[nh++] = "fn_N__4CHitFv";
    hit[nh++] = "0x43486974"; hit[nh++] = "0"; /* the derived vtable is followed by a string dtk did not split off */
    for (int i = 0; i < 9; i++) FN(&t, SP("fn_A%d", i), NULL, "blr");
    FN(&t, "fn_TH1", NULL, "subi r3, r3, 0x24", "b __dt__4CBehFv");
    FN(&t, "fn_TH2", NULL, "subi r3, r3, 0x24", "b fn_M__4CBehFv");
    FN(&t, "@36@__dt__4CHitFv", NULL, "subi r3, r3, 0x24", "b __dt__4CHitFv");
    FN(&t, "fn_M__4CBehFv", "CBeh::fn_M()", "lwz r3, 0x30(r3)", "blr");
    FN(&t, "fn_N__4CHitFv", "CHit::fn_N()", "lfs f1, 0x34(r3)", "blr");
    fn_begin(&t, "__ct__4CBehFv", "CBeh::CBeh()");
    stores(&t, "lbl_80310000", "r3");
    I(&t, "blr");
    fn_end(&t, "__ct__4CBehFv");
    fn_begin(&t, "__dt__4CBehFv", "CBeh::~CBeh()");
    I(&t, "cmpwi r3, 0x0"); I(&t, "mr r30, r3");
    stores(&t, "lbl_80310000", "r30");
    I(&t, "addi r3, r30, 0x24"); I(&t, "li r4, 0x0"); I(&t, "bl __dt__4CEvtFv"); I(&t, "bl __dl__FPv"); I(&t, "blr");
    fn_end(&t, "__dt__4CBehFv");
    FN(&t, "__dt__4CHitFv", "CHit::~CHit()", "cmpwi r3, 0x0", "mr r30, r3", "mr r3, r30", "li r4, 0x0",
       "bl __dt__4CBehFv", "bl __dl__FPv", "blr");
    fn_begin(&t, "fn_80030000", NULL);
    I(&t, "li r3, 0x40"); I(&t, "bl __nw__FUlii"); I(&t, "mr r30, r3"); I(&t, "bl __ct__4CBehFv");
    stores(&t, "lbl_80310100", "r30");
    I(&t, "li r0, 0x0"); I(&t, "stw r0, 0x34(r30)"); I(&t, "blr");
    fn_end(&t, "fn_80030000");
    avp_buf_putz(&t, ".data\n");
    obj_n(&t, "lbl_80310000", 0x80310000, beh, nb);
    obj_n(&t, "lbl_80310100", 0x80310100, hit, nh);
    return prog_of("game/multi", avp_buf_str(&t));
}

/* nw4hbm::lyt::Bounding : nw4hbm::lyt::Pane, built with -RTTI on: every table group starts with (address of the
 * class's __RTTI__ object, offset). Bounding owns a member (ut::Color) it constructs after setting its vptr and
 * destroys in its destructor, which makes two unordered evidence strings. */
#define PANE "Q36nw4hbm3lyt4Pane"
#define BND "Q36nw4hbm3lyt8Bounding"
#define COLOR "Q36nw4hbm2ut5Color"
static AvpStr build_rtti_text(void) {
    AvpArena *a = avp_arena();
    AvpBuf t;
    avp_buf_init(&t, a);
    avp_buf_putz(&t, ".text\n");
    fn_begin(&t, "__ct__" PANE "Fv", NULL);
    lis_addi(&t, "r4", "__vt__" PANE);
    I(&t, "stw r4, 0x0(r3)"); I(&t, "blr");
    fn_end(&t, "__ct__" PANE "Fv");
    fn_begin(&t, "__dt__" PANE "Fv", NULL);
    I(&t, "cmpwi r3, 0x0"); I(&t, "mr r30, r3");
    lis_addi(&t, "r4", "__vt__" PANE);
    I(&t, "stw r4, 0x0(r30)"); I(&t, "bl __dl__FPv"); I(&t, "mr r3, r30"); I(&t, "blr");
    fn_end(&t, "__dt__" PANE "Fv");
    fn_begin(&t, "GetRuntimeTypeInfo__" PANE "CFv", NULL);
    lis_addi(&t, "r3", "__RTTI__" PANE);
    I(&t, "blr");
    fn_end(&t, "GetRuntimeTypeInfo__" PANE "CFv");
    fn_begin(&t, "__ct__" BND "Fv", NULL);
    I(&t, "mflr r0"); I(&t, "mr r31, r3"); I(&t, "bl __ct__" PANE "Fv");
    lis_addi(&t, "r4", "__vt__" BND);
    I(&t, "stw r4, 0x0(r31)"); I(&t, "addi r3, r31, 0x10"); I(&t, "bl __ct__" COLOR "Fv");
    I(&t, "mr r3, r31"); I(&t, "blr");
    fn_end(&t, "__ct__" BND "Fv");
    fn_begin(&t, "__dt__" BND "Fv", NULL);
    I(&t, "cmpwi r3, 0x0"); I(&t, "mr r30, r3");
    lis_addi(&t, "r4", "__vt__" BND);
    I(&t, "stw r4, 0x0(r30)"); I(&t, "addi r3, r30, 0x10"); I(&t, "li r4, 0x0"); I(&t, "bl __dt__" COLOR "Fv");
    I(&t, "mr r3, r30"); I(&t, "li r4, 0x0"); I(&t, "bl __dt__" PANE "Fv"); I(&t, "bl __dl__FPv"); I(&t, "blr");
    fn_end(&t, "__dt__" BND "Fv");
    fn_begin(&t, "GetRuntimeTypeInfo__" BND "CFv", NULL);
    lis_addi(&t, "r3", "__RTTI__" BND);
    I(&t, "blr");
    fn_end(&t, "GetRuntimeTypeInfo__" BND "CFv");
    avp_buf_putz(&t, ".data\n");
    OBJ(&t, "__vt__" PANE, 0x80320000, "__RTTI__" PANE, "0", "__dt__" PANE "Fv", "GetRuntimeTypeInfo__" PANE "CFv");
    OBJ(&t, "__RTTI__" PANE, 0x80320018, "0", "0");
    OBJ(&t, "__vt__" BND, 0x80320020, "__RTTI__" BND, "0", "__dt__" BND "Fv", "GetRuntimeTypeInfo__" BND "CFv");
    OBJ(&t, "__RTTI__" BND, 0x80320030, "0", "0");
    return avp_buf_str(&t);
}
static ClassesProgram *build_rtti(void) { return prog_of("revolution/hbm/lyt", build_rtti_text()); }

/* ---- checking helpers ------------------------------------------------------------------------------------------ */

static const char *sz(AvpStr s) { return CLASSES_NONE(s) ? "None" : avp_cz(avp_arena(), s); }

static ClassesWords *words_of(AvpMap *m, const char *name) {
    AvpVal *v = avp_map_get(m, S(name));
    return v ? (ClassesWords *)v->p : NULL;
}
static void words_eq(const ClassesWords *w, const char *const *exp, size_t n, int line) {
    if (!w) {
        avp_test__fail(__FILE__, line, "vtable missing");
        return;
    }
    if (w->n != n) avp_test__fail(__FILE__, line, "word count %zu != %zu", w->n, n);
    for (size_t i = 0; i < n && i < w->n; i++) {
        if (!exp[i]) CHECK_MSG(CLASSES_NONE(w->v[i]), "word %zu should be None (line %d)", i, line);
        else CHECK_MSG(avp_str_eqz(w->v[i], exp[i]), "word %zu: got %s, want %s (line %d)", i, sz(w->v[i]), exp[i], line);
    }
}
#define WORDS_EQ(w, ...)                                                         \
    do {                                                                         \
        const char *e_[] = {__VA_ARGS__};                                        \
        words_eq((w), e_, sizeof e_ / sizeof *e_, __LINE__);                     \
    } while (0)

static void keys_eq(AvpMap *m, const char *k0, const char *k1) { /* sorted(m) == [k0, k1] */
    AvpStrVec k = avp_map_keys(m);
    REQUIRE(k.n == 2);
    avp_sort_strs(k.v, k.n);
    CHECK_STR(k.v[0], k0);
    CHECK_STR(k.v[1], k1);
}

static void group_is(const ClassesAnalysis *an, const char *vt, i64 a0, i64 a8, i64 a38) {
    const ClassesVtable *v = classes_an_vt(an, S(vt));
    REQUIRE(v != NULL);
    CHECK_EQ_U64(v->group.n, 3);
    const i64 keys[3] = {0, 8, 0x38}, want[3] = {a0, a8, a38};
    for (int i = 0; i < 3; i++) {
        const ClassesGroupEnt *e = classes_group_get(&v->group, keys[i]);
        REQUIRE(e != NULL && e->has_o);
        CHECK_EQ_I64(e->o, want[i]);
    }
}

static AvpJson *cls_named(AvpJson *doc, const char *name) {
    AvpJson *cl = avp_json_getz(doc, "classes");
    for (size_t i = 0; i < avp_json_len(cl); i++)
        if (avp_str_eqz(avp_json_get_str(avp_json_at(cl, i), "name", S("")), name)) return avp_json_at(cl, i);
    return NULL;
}
static AvpJson *with_key(AvpJson *arr, const char *key, const char *val) { /* {x[key]: x}[val] */
    for (size_t i = 0; i < avp_json_len(arr); i++)
        if (avp_str_eqz(avp_json_get_str(avp_json_at(arr, i), key, S("")), val)) return avp_json_at(arr, i);
    return NULL;
}
static bool any_contains(AvpJson *arr, const char *sub) { /* the evidence list joined with spaces contains sub */
    for (size_t i = 0; i < avp_json_len(arr); i++)
        if (avp_str_contains(avp_json_str_or(avp_json_at(arr, i), S("")), S(sub))) return true;
    return false;
}
static void class_list_is(AvpJson *arr, const char *key, const char *want) { /* [x[key] for x in arr] == [want] */
    REQUIRE(avp_json_len(arr) == 1);
    CHECK_STR(avp_json_get_str(avp_json_at(arr, 0), key, S("")), want);
}

/* ---- ParseTest ------------------------------------------------------------------------------------------------- */

static void mc(const char *name, const char *want) {
    AvpStr r = classes_mangled_class(avp_arena(), S(name));
    if (want) CHECK_MSG(avp_str_eqz(r, want), "%s: got %s, want %s", name, sz(r), want);
    else CHECK_MSG(CLASSES_NONE(r), "%s: want None", name);
}

AVP_TEST(test_mangled_class) {
    mc("fn_8009CB1C__17CHitEffectManagerFR7unk_Msg", "CHitEffectManager");
    mc("__ct__14CBaseBehaviourFv", "CBaseBehaviour");
    mc("@36@__dt__17CHitEffectManagerFv", "CHitEffectManager");
    mc("__vt__17CHitEffectManager", "CHitEffectManager");
    mc("fn_1__Q23RWS10CBehaviourFv", "RWS::CBehaviour");
    mc("__vt__Q36nw4hbm3lyt8Bounding", "nw4hbm::lyt::Bounding");
    mc("DrawSelf__Q36nw4hbm3lyt8BoundingFRCQ36nw4hbm3lyt8DrawInfo", "nw4hbm::lyt::Bounding");
    mc("__dt__Q46nw4hbm3lyt6detail8PaneBaseFv", "nw4hbm::lyt::detail::PaneBase");
    mc("fn_800C770C__Fi", NULL);
    mc("__nw__FUlii", NULL);
}

AVP_TEST(test_thunk) {
    ClassesProgram *p = build_multi();
    const ClassesInfo *inf = classes_info(p, S("fn_TH1"));
    REQUIRE(inf != NULL && inf->has_thunk);
    CHECK_EQ_I64(inf->thunk_adjust, 0x24);
    CHECK_STR(inf->thunk_target, "__dt__4CBehFv");
    i64 adj = -1;
    AvpStr t = classes_resolve_thunk(p, S("@36@__dt__4CHitFv"), &adj);
    CHECK_STR(t, "__dt__4CHitFv");
    CHECK_EQ_I64(adj, 0x24);
}

AVP_TEST(test_thunk_to_template_instance) {
    AvpArena *a = avp_arena();
    /* a quoted symbol with a comma is one operand: the thunk target is the whole template instance */
    AvpStrVec o = classes_split_operands(a, S("r3, r3, 0x18"));
    REQUIRE(o.n == 3);
    CHECK_STR(o.v[0], "r3");
    CHECK_STR(o.v[1], "r3");
    CHECK_STR(o.v[2], "0x18");
    o = classes_split_operands(a, S("\"f__1AFP4List<B,0>\""));
    REQUIRE(o.n == 1);
    CHECK_STR(o.v[0], "\"f__1AFP4List<B,0>\"");
    const char *tgt = "f__Q36nw4hbm3lyt6detailFPQ36nw4hbm2ut40LinkList<Q36nw4hbm3lyt13AnimationLink,0>P1B";
    AvpBuf t;
    avp_buf_init(&t, a);
    avp_buf_putz(&t, ".text\n");
    fn_begin(&t, "fn_TH3", NULL);
    I(&t, "subi r3, r3, 0x18"); I(&t, SP("b \"%s\"", tgt));
    fn_end(&t, "fn_TH3");
    ClassesProgram *p = prog_of("game/test", avp_buf_str(&t));
    const ClassesInfo *inf = classes_info(p, S("fn_TH3"));
    REQUIRE(inf != NULL && inf->has_thunk);
    CHECK_EQ_I64(inf->thunk_adjust, 0x18);
    CHECK_STR(inf->thunk_target, tgt);
}

/* ---- SameAddressTest ------------------------------------------------------------------------------------------- */

AVP_TEST(test_cstring_with_objects_sharing_an_address) {
    /* A zero-size label and objects at one address must not make the scan compare Obj instances. */
    AvpBuf t;
    avp_buf_init(&t, avp_arena());
    avp_buf_putz(&t, ".data\n");
    avp_buf_putz(&t, "# .data:0x0 | 0x80320000 | size: 0x0\n.obj lbl_alias, global\n.endobj lbl_alias\n\n");
    string_obj(&t, "lbl_80320000", 0x80320000, "Hello");
    string_obj(&t, "lbl_other", 0x80320000, "Hello");
    ClassesProgram *p = prog_of("game/same", avp_buf_str(&t));
    CHECK_STR(classes_cstring(p, S("lbl_80320000"), 0), "Hello");
    CHECK_STR(classes_cstring(p, S("lbl_alias"), 0), "Hello");
    CHECK_STR(classes_cstring(p, S("lbl_80320000"), 1), "ello");
}

/* ---- VtableTest ------------------------------------------------------------------------------------------------ */

AVP_TEST(test_vtable_shape) {
    ClassesProgram *p = build_single();
    AvpMap *vts = classes_find_vtables(avp_arena(), p, NULL);
    keys_eq(vts, "lbl_80300000", "lbl_80300010");
    WORDS_EQ(words_of(vts, "lbl_80300000"), NULL, NULL, "__dt__5CBaseFv", "fn_80010100__5CBaseFv");
}

AVP_TEST(test_vtable_cut_before_string) {
    ClassesProgram *p = build_multi();
    AvpMap *vts = classes_find_vtables(avp_arena(), p, NULL);
    ClassesWords *w = words_of(vts, "lbl_80310100");
    REQUIRE(w != NULL);
    CHECK_EQ_U64(w->n, 21);
}

AVP_TEST(test_groups_from_stores) {
    ClassesAnalysis *an = classes_analysis_new(avp_arena(), build_multi(), NULL);
    group_is(an, "lbl_80310000", 0x2C, 0x0, 0x24);
    group_is(an, "lbl_80310100", 0x2C, 0x0, 0x24);
}

/* ---- CtorBaseTest (setUp: Analysis(build_single()) and its to_json) -------------------------------------------- */

typedef struct {
    ClassesAnalysis *an;
    AvpJson *db;
} Single;
static Single single_setup(void) {
    Single s;
    s.an = classes_analysis_new(avp_arena(), build_single(), NULL);
    s.db = classes_analysis_to_json(s.an, NULL);
    return s;
}

AVP_TEST(test_ctor_store_detected) {
    Single s = single_setup();
    CHECK_STR(classes_an_vt_of_ctor(s.an, S("__ct__5CBaseFv"), CLASSES_KIND_BIT(CLASSES_K_CTOR)), "lbl_80300000");
    CHECK_STR(classes_an_vt_of_ctor(s.an, S("fn_80020000"), CLASSES_KIND_BIT(CLASSES_K_CTOR)), "lbl_80300010");
}

AVP_TEST(test_base_from_ctor_and_dtor_calls) {
    Single s = single_setup();
    AvpJson *d = cls_named(s.db, "CDerived");
    REQUIRE(d != NULL);
    AvpJson *bases = avp_json_getz(d, "bases");
    REQUIRE(avp_json_len(bases) == 1);
    AvpJson *b = avp_json_at(bases, 0);
    CHECK_STR(avp_json_get_str(b, "class", S("")), "CBase");
    CHECK_STR(avp_json_get_str(b, "offset", S("")), "0x0");
    CHECK(avp_json_get_bool(b, "primary", false));
    AvpJson *ev = avp_json_getz(b, "evidence");
    CHECK(any_contains(ev, "ctor-call"));
    CHECK(any_contains(ev, "dtor-call"));
}

AVP_TEST(test_names) {
    Single s = single_setup();
    CHECK_STR(avp_json_get_str(cls_named(s.db, "CBase"), "name_source", S("")), "mangled-ctor");
    AvpJson *d = cls_named(s.db, "CDerived");
    REQUIRE(d != NULL);
    CHECK_STR(avp_json_get_str(d, "name_source", S("")), "registry-string");
    CHECK_STR(avp_json_get_str(d, "size", S("")), "0x18");
    class_list_is(avp_json_getz(d, "factories"), "name", "fn_80020400");
}

AVP_TEST(test_slot_status) {
    Single s = single_setup();
    AvpJson *words = avp_json_getz(avp_json_getz(cls_named(s.db, "CDerived"), "vtable"), "words");
    const char *want[5] = {"header", "header", "override", "override", "new"};
    REQUIRE(avp_json_len(words) == 5);
    for (size_t i = 0; i < 5; i++) CHECK_STR(avp_json_get_str(avp_json_at(words, i), "status", S("")), want[i]);
    CHECK_STR(avp_json_get_str(avp_json_at(words, 2), "base_target", S("")), "__dt__5CBaseFv");
}

AVP_TEST(test_fields_merged) {
    Single s = single_setup();
    AvpJson *f = avp_json_getz(cls_named(s.db, "CDerived"), "fields");
    AvpJson *f0 = with_key(f, "offset", "0x0"), *fc = with_key(f, "offset", "0xC"), *f10 = with_key(f, "offset", "0x10"),
            *f14 = with_key(f, "offset", "0x14"), *f8 = with_key(f, "offset", "0x8");
    REQUIRE(f0 && fc && f10 && f14 && f8);
    CHECK_STR(avp_json_get_str(f0, "type", S("")), "vptr");
    CHECK_STR(avp_json_get_str(f0, "inherited_from", S("")), "CBase");
    CHECK_STR(avp_json_get_str(fc, "type", S("")), "s16");
    CHECK_STR(avp_json_get_str(f10, "type", S("")), "s8");
    CHECK_STR(avp_json_get_str(f14, "type", S("")), "void*");
    CHECK(avp_json_has(f8, S("conflict")));                        /* float in one function, int in another */
    CHECK_STR(avp_json_get_str(f8, "type", S("")), "f32");        /* the majority use */
}

/* classes_analysis.h and classes.h (the document reader) declare the same type names, so this test cannot decode the
 * document in process: it writes build_single's listing as a one-unit asm tree and runs classes.exe scan and
 * header on it (the reader the Python calls as C.render_header). */
AVP_TEST(test_header) {
    AvpArena *a = avp_arena();
    char *dir = avp_mkdtemp(a, "classes_pytests_hdr", NULL, NULL);
    REQUIRE(dir != NULL);
    char *asm_dir = avp_path_join(a, dir, "asm");
    REQUIRE(avp_makedirs(avp_path_join(a, asm_dir, "game"), true));
    AvpBuf t;
    avp_buf_init(&t, a);
    single_text(&t);
    AvpStr text = avp_buf_str(&t);
    REQUIRE(avp_write_file(avp_join(a, asm_dir, "game", "test.s"), text.p, text.n));
    char *syms = avp_path_join(a, dir, "symbols.txt");
    REQUIRE(avp_write_file(syms, "", 0));
    char *out = avp_path_join(a, dir, "classes.json");
    char *exe = avp_path_abspath(a, avp_path_join(a, avp_exe_dir(), "..\\classes.exe"));
    REQUIRE(avp_exists(exe));
    REQUIRE(avp_setenv("SHSM_CACHE_DIR", avp_path_join(a, dir, "cache")));
    const char *scan_cmd[] = {exe, "--asm", asm_dir, "--symbols", syms, "--out", out, "--splits", "none.txt", "scan", NULL};
    AvpProcResult r = avp_run(&(AvpProcSpec){.argv = scan_cmd, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE,
                                             .text = true, .timeout = 300, .arena = a});
    REQUIRE(r.status == AVP_PROC_OK && r.returncode == 0);
    const char *hdr_cmd[] = {exe, "--asm", asm_dir, "--symbols", syms, "--out", out, "--splits", "none.txt", "header",
                             "CDerived", NULL};
    r = avp_run(&(AvpProcSpec){.argv = hdr_cmd, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE, .text = true,
                               .timeout = 300, .arena = a});
    REQUIRE(r.status == AVP_PROC_OK && r.returncode == 0);
    AvpStr h = r.out;
    CHECK(avp_str_contains(h, S("class CDerived : public CBase {")));
    CHECK(avp_str_contains(h, S("virtual         ~CDerived();   // override")));
    CHECK(avp_str_contains(h, S("virtual void    fn_80020300();   // signature unknown")));
    CHECK(avp_str_contains(h, S("field_0x8;")));
    avp_tempdir_cleanup(dir);
}

/* ---- MultiTest ------------------------------------------------------------------------------------------------- */

static AvpJson *multi_setup(void) {
    return classes_analysis_to_json(classes_analysis_new(avp_arena(), build_multi(), NULL), NULL);
}

AVP_TEST(test_hierarchy) {
    AvpJson *db = multi_setup();
    AvpJson *hit = cls_named(db, "CHit");
    REQUIRE(hit != NULL);
    AvpJson *bases = avp_json_getz(hit, "bases");
    class_list_is(bases, "class", "CBeh");
    CHECK(avp_json_get_bool(avp_json_at(bases, 0), "primary", false));
    CHECK_STR(avp_json_get_str(hit, "size", S("")), "0x40");
}

AVP_TEST(test_slots) {
    AvpJson *db = multi_setup();
    AvpJson *words = avp_json_getz(avp_json_getz(cls_named(db, "CHit"), "vtable"), "words");
    AvpJson *w40 = with_key(words, "offset", "0x40"), *w44 = with_key(words, "offset", "0x44"),
            *w50 = with_key(words, "offset", "0x50"), *w10 = with_key(words, "offset", "0x10");
    REQUIRE(w40 && w44 && w50 && w10);
    CHECK_STR(avp_json_get_str(w40, "status", S("")), "override");
    CHECK_STR(avp_json_get_str(avp_json_getz(w40, "thunk"), "target", S("")), "__dt__4CHitFv");
    CHECK_STR(avp_json_get_str(w44, "status", S("")), "inherited");
    CHECK_STR(avp_json_get_str(w50, "status", S("")), "new");
    CHECK_STR(avp_json_get_str(w50, "group", S("")), "complete");
    CHECK_STR(avp_json_get_str(w10, "group", S("")), "embedded");
}

AVP_TEST(test_root_embedded_groups) {
    /* CBeh has no known bases: its embedded groups are guessed (next group / thunk run) */
    AvpJson *db = multi_setup();
    AvpJson *words = avp_json_getz(avp_json_getz(cls_named(db, "CBeh"), "vtable"), "words");
    AvpJson *w40 = with_key(words, "offset", "0x40"), *w44 = with_key(words, "offset", "0x44"),
            *w48 = with_key(words, "offset", "0x48");
    REQUIRE(w40 && w44 && w48);
    CHECK_STR(avp_json_get_str(w40, "status", S("")), "unresolved");
    CHECK_STR(avp_json_get_str(w44, "status", S("")), "unresolved");
    CHECK_STR(avp_json_get_str(w48, "status", S("")), "new"); /* first own slot after the thunk run */
}

AVP_TEST(test_field_after_base) {
    AvpJson *db = multi_setup();
    AvpJson *f = with_key(avp_json_getz(cls_named(db, "CHit"), "fields"), "offset", "0x34");
    REQUIRE(f != NULL);
    CHECK(!avp_json_has(f, S("inherited_from")));
    CHECK(avp_json_has(f, S("conflict"))); /* int store in the factory, float load in fn_N */
}

/* ---- RttiVtableTest -------------------------------------------------------------------------------------------- */

#define PANE_VT "__vt__" PANE
#define BND_VT "__vt__" BND

AVP_TEST(test_rtti_table_shape) {
    AvpArena *a = avp_arena();
    AvpMap *rtti = avp_map_new();
    AvpMap *vts = classes_find_vtables(a, build_rtti(), rtti);
    keys_eq(vts, PANE_VT, BND_VT);
    /* the RTTI word is a header word (None) and the table keeps its two header words */
    WORDS_EQ(words_of(vts, BND_VT), NULL, NULL, "__dt__" BND "Fv", "GetRuntimeTypeInfo__" BND "CFv");
    CHECK_EQ_U64(words_of(vts, PANE_VT)->n, 4);
    AvpVal *r = avp_map_get(rtti, S(BND_VT));
    REQUIRE(r != NULL);
    const ClassesRttiList *l = r->p;
    REQUIRE(l->n == 1);
    CHECK_EQ_I64(l->v[0].k, 0);
    CHECK_STR(l->v[0].sym, "__RTTI__" BND);
    CHECK_EQ_I64(l->v[0].off, 0);
}

/* the unit of test_trailing_null_slot_of_named_vtable: a table named `name` with a trailing zero word */
static ClassesProgram *trailing_null_prog(const char *name) {
    AvpBuf t;
    avp_buf_init(&t, avp_arena());
    avp_buf_putz(&t, ".text\n");
    FN(&t, "fn_A__1AFv", NULL, "blr");
    avp_buf_putz(&t, ".data\n");
    OBJ(&t, name, 0x80340000, "__RTTI__1A", "0", "fn_A__1AFv", "0");
    OBJ(&t, "__RTTI__1A", 0x80340010, "0", "0");
    return prog_of("revolution/hbm/x", avp_buf_str(&t));
}

AVP_TEST(test_trailing_null_slot_of_named_vtable) {
    /* a named __vt__ symbol has its extent from the symbols file: a trailing zero inside it is the null slot of a
     * pure virtual (homebutton::gui::Component's contain); an unsized dtk split (lbl_) -RTTI on table still drops
     * a trailing zero as padding to the next object */
    AvpArena *a = avp_arena();
    AvpMap *v1 = classes_find_vtables(a, trailing_null_prog("__vt__1A"), avp_map_new());
    WORDS_EQ(words_of(v1, "__vt__1A"), NULL, NULL, "fn_A__1AFv", NULL);
    AvpMap *v2 = classes_find_vtables(a, trailing_null_prog("lbl_80340000"), avp_map_new());
    WORDS_EQ(words_of(v2, "lbl_80340000"), NULL, NULL, "fn_A__1AFv");
}

AVP_TEST(test_base_with_trailing_null_fits_shorter_derived) {
    /* the base symbol's extent includes a trailing zero (padding or a pure slot: indistinguishable); the derived
     * table one word shorter still holds its image (ArcResourceAccessor : ResourceAccessor) */
    AvpArena *a = avp_arena();
    AvpStr text = build_rtti_text();
    AvpStr old = S("\t.4byte GetRuntimeTypeInfo__" PANE "CFv\n.endobj __vt__" PANE);
    AvpStr rep = S("\t.4byte GetRuntimeTypeInfo__" PANE "CFv\n\t.4byte 0x00000000\n.endobj __vt__" PANE);
    CHECK(avp_str_contains(text, old));
    text = avp_str_replace(a, text, old, rep, -1);
    ClassesAnalysis *an = classes_analysis_new(a, prog_of("revolution/hbm/lyt", text), NULL);
    const ClassesVtable *pv = classes_an_vt(an, S(PANE_VT));
    REQUIRE(pv != NULL);
    CHECK_EQ_U64(pv->words->n, 5);
    AvpJson *b = cls_named(classes_analysis_to_json(an, NULL), "nw4hbm::lyt::Bounding");
    REQUIRE(b != NULL);
    class_list_is(avp_json_getz(b, "bases"), "class", "nw4hbm::lyt::Pane");
}

AVP_TEST(test_quoted_template_symbol_is_one_word) {
    /* a quoted mangled name with a comma inside its template arguments is one .4byte word
     * (PaneManager's walkInChildren__...LinkList<Q36nw4hbm3lyt4Pane,4>) */
    AvpArena *a = avp_arena();
    const char *sym = "walk__1AFRQ26nw4hbm8LinkList<Q26nw4hbm4Pane,4>";
    AvpBuf t;
    avp_buf_init(&t, a);
    avp_buf_putz(&t, ".text\n");
    FN(&t, sym, NULL, "blr");
    FN(&t, "fn_B__1AFv", NULL, "blr");
    avp_buf_putz(&t, ".data\n");
    avp_buf_printf(&t, "# .data:0x0 | 0x80350000 | size: 0x10\n.obj __vt__1A, global\n    .4byte 0x00000000\n"
                       "    .4byte 0x00000000\n    .4byte \"%s\"\n    .4byte fn_B__1AFv\n.endobj __vt__1A\n", sym);
    ClassesProgram *p = prog_of("revolution/hbm/y", avp_buf_str(&t));
    const ClassesObj *o = classes_obj(p, S("__vt__1A"));
    REQUIRE(o != NULL && o->has_data);
    CHECK_EQ_U64(o->data.n, 16);
    AvpMap *vts = classes_find_vtables(a, p, avp_map_new());
    WORDS_EQ(words_of(vts, "__vt__1A"), NULL, NULL, sym, "fn_B__1AFv");
}

AVP_TEST(test_rtti_off_table_unchanged) {
    /* a zero header still works, and a trailing zero word of a table without a type descriptor stays part of the
     * table */
    AvpArena *a = avp_arena();
    AvpBuf t;
    avp_buf_init(&t, a);
    avp_buf_putz(&t, ".text\n");
    FN(&t, "fn_A", NULL, "blr");
    avp_buf_putz(&t, ".data\n");
    OBJ(&t, "lbl_80330000", 0x80330000, "0", "0", "fn_A", "0");
    ClassesProgram *p = prog_of("game/x", avp_buf_str(&t));
    AvpMap *rtti = avp_map_new();
    AvpMap *vts = classes_find_vtables(a, p, rtti);
    CHECK_EQ_U64(avp_map_len(vts), 1);
    WORDS_EQ(words_of(vts, "lbl_80330000"), NULL, NULL, "fn_A", NULL);
    CHECK_EQ_U64(avp_map_len(rtti), 0);
}

AVP_TEST(test_classes) {
    AvpJson *db = classes_analysis_to_json(classes_analysis_new(avp_arena(), build_rtti(), NULL), NULL);
    AvpJson *b = cls_named(db, "nw4hbm::lyt::Bounding");
    REQUIRE(b != NULL);
    CHECK_STR(avp_json_get_str(b, "name_source", S("")), "vt-symbol");
    class_list_is(avp_json_getz(b, "bases"), "class", "nw4hbm::lyt::Pane");
    AvpJson *vt = avp_json_getz(b, "vtable");
    CHECK_STR(avp_json_get_str(vt, "size", S("")), "0x10");
    AvpJson *words = avp_json_getz(vt, "words");
    const char *want[4] = {"header", "header", "override", "override"};
    REQUIRE(avp_json_len(words) == 4);
    for (size_t i = 0; i < 4; i++) CHECK_STR(avp_json_get_str(avp_json_at(words, i), "status", S("")), want[i]);
    CHECK_STR(avp_json_get_str(avp_json_at(words, 0), "rtti", S("")), "__RTTI__" BND);
    CHECK(!avp_json_has(avp_json_at(words, 1), S("rtti")));
    AvpJson *pw = avp_json_getz(avp_json_getz(cls_named(db, "nw4hbm::lyt::Pane"), "vtable"), "words");
    CHECK_STR(avp_json_get_str(avp_json_at(pw, 0), "rtti", S("")), "__RTTI__" PANE);
    AvpJson *mo = avp_json_getz(b, "member_objects");
    class_list_is(mo, "class", "nw4hbm::ut::Color");
    CHECK_EQ_U64(avp_json_len(avp_json_getz(avp_json_at(mo, 0), "evidence")), 2);
}

AVP_TEST_MAIN("classes_pytests_core")
