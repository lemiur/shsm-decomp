/* decl_test.c: classes decl's model (classes_decl.h) on synthetic vtables: slot naming from retail symbols, the
 * pairing, the sibling vtable order and placeholders; vtable ownership from a sibling __vt__ (including a table
 * classes.json attributes to the wrong class); overloads, pure virtuals, overrides against inherited slots and the
 * placement of the vtable pointer among the members. */
#include "avp/test.h"
#include "classes_decl.h"

#include <string.h>

#define W(off, retail, addr, pair) ((CdWord){(off), (retail), (addr), (pair)})

static bool has(AvpStr text, const char *needle) { return avp_str_find(text, avp_str(needle), 0, (i64)text.n) >= 0; }
static i64 pos(AvpStr text, const char *needle) { return avp_str_find(text, avp_str(needle), 0, (i64)text.n); }

AVP_TEST(slot_names_by_source) {
    AvpArena *a = avp_arena();
    static const char *const sib_slots[] = {"release__Q24FMOD6PluginFv", "alloc__Q24FMOD4DSPIFi",
                                            "reset__Q24FMOD4DSPIFv", NULL};
    CdSibVt sib = {"__vt__Q24FMOD4DSPI", "FMOD::DSPI", (const char **)sib_slots, 4, false};
    CdWord w[] = {
        W(0x8, "release__Q24FMOD6PluginFv", 0x80001000, "release__Q24FMOD6PluginFv"), /* retail symbol */
        W(0xC, "fn_80001100", 0x80001100, "alloc__Q24FMOD4DSPIFi"),                     /* pairing */
        W(0x10, "fn_80001200", 0x80001200, NULL),                                        /* sibling vtable order */
        W(0x14, NULL, 0, NULL),                                                          /* pure */
    };
    CdName n[4];
    cd_name_slots(a, w, 4, &sib, n);
    CHECK_EQ_I64(n[0].src, CD_SRC_RETAIL);
    CHECK_EQ_I64(n[1].src, CD_SRC_PAIR);
    CHECK_CZ(n[1].name, "alloc__Q24FMOD4DSPIFi");
    CHECK_EQ_I64(n[2].src, CD_SRC_SIBVT);
    CHECK_CZ(n[2].name, "reset__Q24FMOD4DSPIFv");
    CHECK(n[3].pure && !n[3].name);
    /* without the sibling table the third slot keeps its placeholder */
    cd_name_slots(a, w, 4, NULL, n);
    CHECK_EQ_I64(n[2].src, CD_SRC_NONE);
    CHECK(!n[2].name);
    /* a pairing that disagrees with the sibling table's order is reported */
    {
        static const char *const other[] = {"release__Q24FMOD6PluginFv", "free__Q24FMOD4DSPIFi", "x__Q24FMOD4DSPIFv", NULL};
        CdSibVt s2 = {"__vt__Q24FMOD4DSPI", "FMOD::DSPI", (const char **)other, 4, false};
        cd_name_slots(a, w, 4, &s2, n);
        CHECK_CZ(n[1].other, "free__Q24FMOD4DSPIFi");
    }
}

AVP_TEST(vtable_ownership) {
    AvpArena *a = avp_arena();
    /* the retail table classes.json calls DSPI: its slots are those of Gormiti's DSPResampler, not DSPI's */
    static const char *const dspi[] = {"release__Q24FMOD6PluginFv", "execute__Q24FMOD4DSPIFv", "reset__Q24FMOD4DSPIFv"};
    static const char *const res[] = {"release__Q24FMOD6PluginFv", "execute__Q24FMOD12DSPResamplerFv",
                                      "reset__Q24FMOD4DSPIFv"};
    static const char *const plugin[] = {"release__Q24FMOD6PluginFv"};
    CdSibVt c[] = {{"__vt__Q24FMOD4DSPI", "FMOD::DSPI", (const char **)dspi, 3, false},
                   {"__vt__Q24FMOD12DSPResampler", "FMOD::DSPResampler", (const char **)res, 3, false},
                   {"__vt__Q24FMOD6Plugin", "FMOD::Plugin", (const char **)plugin, 1, false}};
    CdWord w[] = {W(0x8, "release__Q24FMOD6PluginFv", 1, NULL), W(0xC, "fn_80002000", 2, "execute__Q24FMOD12DSPResamplerFv"),
                  W(0x10, "reset__Q24FMOD4DSPIFv", 3, NULL)};
    CdVtMatch m = cd_match_vtable(a, w, 3, c, 3, "FMOD::DSPI");
    CHECK_EQ_I64(m.index, 1);
    CHECK_EQ_I64(m.used, 3);
    /* a shape-only retail table one null word longer than the sibling's: Plugin, truncated */
    CdWord p[] = {W(0x8, "release__Q24FMOD6PluginFv", 1, NULL), W(0xC, NULL, 0, NULL)};
    m = cd_match_vtable(a, p, 2, c, 3, NULL);
    CHECK_EQ_I64(m.index, 2);
    CHECK_EQ_I64(m.used, 1);
    /* two identical sibling tables: ambiguous unless the preferred name decides */
    CdSibVt twin[] = {c[0], {"__vt__Q24FMOD5Other", "FMOD::Other", (const char **)dspi, 3, false}};
    CdWord d[] = {W(0x8, "release__Q24FMOD6PluginFv", 1, NULL), W(0xC, "execute__Q24FMOD4DSPIFv", 2, NULL),
                  W(0x10, "reset__Q24FMOD4DSPIFv", 3, NULL)};
    m = cd_match_vtable(a, d, 3, twin, 2, NULL);
    CHECK_EQ_I64(m.index, -1);
    CHECK_EQ_I64(m.ntied, 2);
    m = cd_match_vtable(a, d, 3, twin, 2, "FMOD::Other");
    CHECK_EQ_I64(m.index, 1);
    /* a contradicting slot rules a table out */
    CdWord bad[] = {W(0x8, "release__Q24FMOD6PluginFv", 1, NULL), W(0xC, "other__Q24FMOD4DSPIFv", 2, NULL),
                    W(0x10, "reset__Q24FMOD4DSPIFv", 3, NULL)};
    m = cd_match_vtable(a, bad, 3, c, 2, NULL);
    CHECK_EQ_I64(m.index, -1);
}

AVP_TEST(kinds_override_inherited_new) {
    AvpArena *a = avp_arena();
    CdName base[] = {{"__dt__4BaseFv", CD_SRC_RETAIL, false, NULL}, {"fn_A__4BaseFv", CD_SRC_RETAIL, false, NULL},
                     {NULL, 0, true, NULL}};
    CdName mine[] = {{"__dt__7DerivedFv", CD_SRC_RETAIL, false, NULL}, {"fn_A__4BaseFv", CD_SRC_RETAIL, false, NULL},
                     {"pure__7DerivedFv", CD_SRC_RETAIL, false, NULL}, {"fn_A__7DerivedFb", CD_SRC_RETAIL, false, NULL}};
    int k[4];
    cd_classify(a, "Derived", mine, 4, 3, base, 3, k);
    CHECK_EQ_I64(k[0], CD_KIND_OVERRIDE);
    CHECK_EQ_I64(k[1], CD_KIND_INHERITED);
    CHECK_EQ_I64(k[2], CD_KIND_OVERRIDE); /* implements the base's pure slot */
    CHECK_EQ_I64(k[3], CD_KIND_NEW);      /* the overload of fn_A is a new slot */
    /* no base table: a slot named for the class overrides, one named for the base is inherited */
    cd_classify(a, "Derived", mine, 4, 2, NULL, 0, k);
    CHECK_EQ_I64(k[0], CD_KIND_OVERRIDE);
    CHECK_EQ_I64(k[1], CD_KIND_INHERITED);
    CHECK_EQ_I64(k[2], CD_KIND_NEW);
    /* the base count from the names: the last slot named for an ancestor */
    const char *anc[] = {"Base"};
    CHECK_EQ_I64((i64)cd_infer_base_count(a, mine, 4, anc, 1), 2);
}

AVP_TEST(method_declarations) {
    AvpArena *a = avp_arena();
    CdSig s = {"FMOD_RESULT", (const char *[]){"system"}, 1};
    CHECK_CZ(cd_method_decl(a, "DSPI", "FMOD", "getSystemObject__Q24FMOD4DSPIFPPQ24FMOD6System", &s, false, "x", NULL),
             "virtual FMOD_RESULT getSystemObject( System **system )");
    CHECK_CZ(cd_method_decl(a, "DSPI", "FMOD", "release__Q24FMOD4DSPIFv", NULL, false, "x", "FMOD_RESULT"),
             "virtual /* return type unconfirmed */ FMOD_RESULT release()");
    CHECK_CZ(cd_method_decl(a, "DSPI", "FMOD", "__dt__Q24FMOD4DSPIFv", NULL, false, "x", NULL), "virtual ~DSPI()");
    CdSig c = {"int", NULL, 0};
    CHECK_CZ(cd_method_decl(a, "S", "", "get__1SCFi", &c, true, "x", NULL), "virtual int get( int ) const = 0");
    CHECK_CZ(cd_method_decl(a, "S", "", NULL, NULL, true, "fn_80400010", "void"),
             "virtual /* return type unconfirmed */ void fn_80400010() = 0");
}

/* A class with no Gormiti layout lists the fields retail reads as field_0x members, marked as drafted. */
AVP_TEST(render_drafted_fields) {
    AvpArena *a = avp_arena();
    CdWord w[] = {W(0x8, "reallyClose__Q24FMOD8NullFileFv", 0x80001000, NULL)};
    CdName n[1];
    cd_name_slots(a, w, 1, NULL, n);
    int k[1] = {CD_KIND_OVERRIDE};
    CdMember dm[] = {{"unsigned int field_0x194", 0x194, 4, "0x194, u32: retail 2 loads, 4 stores"}};
    CdClass c = {0};
    c.cls = "FMOD::NullFile";
    c.vtable_symbol = "__vt__Q24FMOD8NullFile";
    c.vtable_address = 0x8044FA90;
    c.owner_source = "test";
    c.w = w;
    c.names = n;
    c.kind = k;
    c.n = 1;
    c.base_cls = "FMOD::File";
    c.drafted = dm;
    c.ndrafted = 1;
    AvpBuf out;
    avp_buf_init(&out, a);
    cd_render(a, &c, &out);
    AvpStr t = avp_buf_str(&out);
    CHECK(has(t, "1 member drafted from the retail field accesses (classes.json), named field_0x<offset>"));
    CHECK(has(t, "unsigned int field_0x194;"));
    CHECK(has(t, "// 0x194, u32: retail 2 loads, 4 stores"));
    CHECK(pos(t, "field_0x194;") > pos(t, "virtual "));
    c.ndrafted = 0;
    avp_buf_init(&out, a);
    cd_render(a, &c, &out);
    CHECK(has(avp_buf_str(&out), "(members not drafted)"));
}

/* Overloads keep their slot order; members before the class's own vptr come before the first virtual. */
AVP_TEST(render_vptr_placement_and_overloads) {
    AvpArena *a = avp_arena();
    CdWord w[] = {W(0x8, "release__Q24FMOD6PluginFv", 0x80001000, NULL), W(0xC, "release__Q24FMOD6PluginFb", 0x80001010, NULL)};
    CdName n[2];
    cd_name_slots(a, w, 2, NULL, n);
    int k[2] = {CD_KIND_NEW, CD_KIND_NEW};
    CdSig sg[2] = {{"FMOD_RESULT", NULL, 0}, {"FMOD_RESULT", (const char *[]){"freethis"}, 1}};
    CdMember m[] = {{"SystemI *mSystem", 0x10, 4}, {"Global *mGlobal", 0x14, 4}, {"int mAfter", 0x1C, 4}};
    CdBase b[] = {{"SortedLinkedListNode", "public", 0, 0x10, false}};
    CdClass c = {0};
    c.cls = "FMOD::Plugin";
    c.vtable_symbol = "lbl_8044CA98";
    c.vtable_address = 0x8044CA98;
    c.owner_source = "test";
    c.w = w;
    c.names = n;
    c.kind = k;
    c.sigs = sg;
    c.n = 2;
    c.layout = (CdLayout){true, "class", 0x20, b, 1, m, 3, 1};
    c.own_vptr = true;
    c.vptr_off = 0x18;
    CHECK_EQ_I64((i64)cd_members_before_vptr(&c), 2);
    AvpBuf out;
    avp_buf_init(&out, a);
    cd_render(a, &c, &out);
    AvpStr t = avp_buf_str(&out);
    CHECK(has(t, "namespace FMOD {"));
    CHECK(has(t, "class Plugin : public SortedLinkedListNode {"));
    i64 glob = pos(t, "Global *mGlobal;"), first = pos(t, "virtual FMOD_RESULT release();"),
        second = pos(t, "virtual FMOD_RESULT release( bool freethis );"), after = pos(t, "int mAfter;");
    CHECK(glob >= 0 && first > glob && second > first && after > second);
    CHECK(has(t, "// vtable pointer at 0x18"));
    CHECK(has(t, "}; // size 0x20 (Gormiti DWARF)"));
    /* inherited slots are not declared; an override is marked */
    k[0] = CD_KIND_INHERITED;
    k[1] = CD_KIND_OVERRIDE;
    c.own_vptr = false;
    avp_buf_init(&out, a);
    cd_render(a, &c, &out);
    t = avp_buf_str(&out);
    CHECK(!has(t, "release();"));
    CHECK(has(t, "slot 0x0C, override"));
}

/* lbl_80451F70's shape: EventProjectI's table directly followed by its interface EventProject's (header pair, pure
 * slots as null words, the destructor) */
AVP_TEST(two_tables_in_one_object) {
    AvpArena *a = avp_arena();
    static const char *const impl[] = {"release__Q24FMOD13EventProjectIFv", "getInfo__Q24FMOD13EventProjectIFPi",
                                       "__dt__Q24FMOD13EventProjectIFv"};
    static const char *const iface[] = {NULL, NULL, "__dt__Q24FMOD12EventProjectFv"};
    CdSibVt sv[2] = {{"__vt__Q24FMOD13EventProjectI", "FMOD::EventProjectI", (const char **)impl, 3, false},
                     {"__vt__Q24FMOD12EventProject", "FMOD::EventProject", (const char **)iface, 3, false}};
    CdWord w[] = {
        W(0x8, "release__Q24FMOD13EventProjectIFv", 0x80001000, NULL),
        W(0xC, "getInfo__Q24FMOD13EventProjectIFPi", 0x80001100, NULL),
        W(0x10, "__dt__Q24FMOD13EventProjectIFv", 0x80001200, NULL),
        W(0x14, NULL, 0, NULL), W(0x18, NULL, 0, NULL), /* the second table's header */
        W(0x1C, NULL, 0, NULL), W(0x20, NULL, 0, NULL),
        W(0x24, "fn_80001300", 0x80001300, "__dt__Q24FMOD12EventProjectFv"),
    };
    CHECK_EQ_I64((i64)cd_split_point(a, w, 8, sv, 2), 3);
    /* the whole object matches no table; its prefix matches the implementation's */
    CHECK(cd_match_vtable(a, w, 8, sv, 2, NULL).index < 0);
    CHECK_EQ_I64(cd_match_vtable(a, w, 3, sv, 2, NULL).index, 0);
    CHECK_EQ_I64(cd_match_vtable(a, w + 5, 3, sv, 2, NULL).index, 1);
    /* trailing nulls only: padding, one table */
    CdWord p[] = {w[0], w[1], w[2], W(0x14, NULL, 0, NULL), W(0x18, NULL, 0, NULL), W(0x1C, NULL, 0, NULL)};
    CHECK_EQ_I64((i64)cd_split_point(a, p, 6, sv, 2), 0);
}

AVP_TEST(unqualified_names) {
    AvpArena *a = avp_arena();
    static const char *const names[] = {"FMOD::EventI", "FMOD::EventGroup", "Audio::EventGroup", "FMOD::EventI",
                                        NULL, "EventGroupI"};
    AvpStrVec one = cd_short_matches(a, "EventI", names, 6);
    REQUIRE(one.n == 1);
    CHECK(avp_str_eqz(one.v[0], "FMOD::EventI"));
    AvpStrVec two = cd_short_matches(a, "EventGroup", names, 6);
    CHECK(two.n == 2);
    CHECK(cd_short_matches(a, "EventGroupI", names, 6).n == 0); /* already unqualified: nothing to resolve */
    avp_vec_free(&one);
    avp_vec_free(&two);
}

AVP_TEST(callback_const_from_mangled_user) {
    AvpArena *a = avp_arena();
    AvpMap *cb = avp_map_new();
    cd_scan_callbacks(a, cb, "FMOD::SystemI::setFileSystem(FMOD_RESULT (*)(const char*, int, unsigned int*, void**,"
                             " void**), FMOD_RESULT (*)(void*, void*), int)");
    cd_scan_callbacks(a, cb, "X::f(FMOD_RESULT (*)(const void*, void*))"); /* a second spelling of (void*, void*) */
    const char *fixed = cd_callback_consts(a, cb, "FMOD_RESULT (*mOpenCallback)(char *, int, unsigned int *, void **, void **)");
    REQUIRE(fixed != NULL);
    CHECK_CZ(fixed, "FMOD_RESULT (*mOpenCallback)(const char *, int, unsigned int *, void **, void **)");
    CHECK(cd_callback_consts(a, cb, "FMOD_RESULT (*mCloseCallback)(void *, void *)") == NULL); /* ambiguous */
    CHECK(cd_callback_consts(a, cb, "int mCount") == NULL);
    avp_map_free(cb);
}

AVP_TEST_MAIN("classes_decl")
