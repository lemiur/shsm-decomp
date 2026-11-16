/* Port of tools/test_gatecheck.py LayoutTests (the tests that do not run the command line; those are in cli_test.c)
 * and AlignTests' two alignment tests.
 *
 * Python test -> C test
 *   LayoutTests.test_parse_asm                              -> parse_asm
 *   LayoutTests.test_clean_layout                           -> clean_layout
 *   LayoutTests.test_order_difference                       -> order_difference
 *   LayoutTests.test_size_difference                        -> size_difference
 *   LayoutTests.test_content_difference                     -> content_difference
 *   LayoutTests.test_content_difference_in_table            -> content_difference_in_table
 *   LayoutTests.test_named_object_renamed_is_a_note         -> named_object_renamed_is_a_note
 *   LayoutTests.test_missing_and_extra                      -> missing_and_extra
 *   LayoutTests.test_total_size                             -> total_size
 *   LayoutTests.test_deadstrip_of_unreferenced_data         -> deadstrip_of_unreferenced_data
 *   LayoutTests.test_deadstrip_realigns_a_later_double      -> deadstrip_realigns_a_later_double
 *   LayoutTests.test_deadstrip_does_not_excuse_unexplained_padding -> deadstrip_does_not_excuse_unexplained_padding
 *   LayoutTests.test_weak_duplicate_is_extra_without_dedupe -> weak_duplicate_is_extra_without_dedupe
 *   LayoutTests.test_weak_duplicate_is_deduplicated         -> weak_duplicate_is_deduplicated
 *   LayoutTests.test_dedupe_does_not_hide_other_extras      -> dedupe_does_not_hide_other_extras
 *   LayoutTests.test_dedupe_ignores_names_the_unit_defines  -> dedupe_ignores_names_the_unit_defines
 *   LayoutTests.test_weak_dedupe_index                      -> weak_dedupe_index
 *   LayoutTests.test_coarse_retail_object                   -> coarse_retail_object
 *   LayoutTests.test_coarse_content_difference              -> coarse_content_difference
 *   LayoutTests.test_bss_order                              -> bss_order
 *   LayoutTests.test_loose_ctors_section                    -> loose_ctors_section
 *   LayoutTests.test_code_size_matches_with_dead_strip_and_padding -> code_size_matches_with_dead_strip_and_padding
 *   LayoutTests.test_code_size_difference_is_a_layout_failure      -> code_size_difference_is_a_layout_failure
 *   LayoutTests.test_directive_widths                       -> directive_widths
 *   LayoutTests.test_anon_names                             -> anon_names
 *   LayoutTests.test_gap_names_are_dtks_only                -> gap_names_are_dtks_only
 *   LayoutTests.test_alignment_check                        -> alignment_check
 *   AlignTests.test_lcs_pairs_by_name_and_size              -> lcs_pairs_by_name_and_size
 *   AlignTests.test_lcs_detects_swap                        -> lcs_detects_swap
 * Command-line tests (test_cli_layout_all_and_single, test_cli_layout_refuses_empty_work_and_misused_overrides,
 * test_cli_layout_reports_a_code_size_difference, AlignTests.test_cli_requires_unit): cli_test.c. */
#include "gc_fixtures.h"

static GcLayoutResult *layout_default(void) {
    FxBuilt o = fx_built_defaults();
    return fx_layout(fx_built(&o), fx_retail_asm(), fx_retail_blob(), NULL);
}

AVP_TEST(parse_asm) {
    AvpArena *a = avp_arena();
    ShsmDol *d = fx_dol_parse(fx_dol1(7, FX_START, fx_retail_blob()));
    GcAsmUnit *au = gc_parse_asm_data(a, avp_str(fx_retail_asm()), d);
    GcAsmSection *sec = gc_asm_section(au, ".data");
    REQUIRE(sec != NULL);
    CHECK(sec->has_start);
    CHECK_EQ_I64(sec->start, FX_START);
    CHECK_EQ_I64(sec->end, FX_START + 0x30);
    const char *names[] = {"lbl_80300000", "gap_07_80300003_data", "lbl_80300004",
                           "gap_07_80300007_data", "Table", "gap_07_80300018_data"};
    REQUIRE(sec->objs.n == 6);
    for (size_t i = 0; i < 6; i++) CHECK_CZ(sec->objs.v[i]->name, names[i]);
    const GcObj *table = sec->objs.v[4];
    CHECK_EQ_I64(table->off, 8);
    CHECK_EQ_I64(table->size, 16);
    REQUIRE(table->relocs.n == 1);
    CHECK_EQ_I64(table->relocs.v[0].off, 4);
    CHECK_EQ_I64(sec->objs.v[0]->dlen, 3);
    CHECK_MEM(sec->objs.v[0]->data, "ab\0", 3);
    CHECK(sec->objs.v[1]->gap);
    CHECK_EQ_U64(sec->warnings.n, 0);
}

AVP_TEST(clean_layout) {
    /* built has a different compiler name for the strings and a relocation to @1; the table word at +4 is relocated
     * in both */
    fx_check_no_differences(layout_default());
}

AVP_TEST(order_difference) {
    FxBuilt o = fx_built_defaults();
    o.order[0] = 1; o.order[1] = 0; o.order[2] = 2;
    GcLayoutResult *res = fx_layout(fx_built(&o), fx_retail_asm(), fx_retail_blob(), NULL);
    CHECK(fx_has(res, "ORDER", true) || fx_has(res, "OFFSET", true) || fx_has(res, "CONTENT", true));
}

AVP_TEST(size_difference) {
    FxBuilt o = fx_built_defaults();
    o.table_size = 20;
    GcLayoutResult *res = fx_layout(fx_built(&o), fx_retail_asm(), fx_retail_blob(), NULL);
    CHECK(fx_has(res, "SIZE", true));
}

AVP_TEST(content_difference) {
    FxBuilt o = fx_built_defaults();
    o.s1 = AVP_S("cX\0");
    GcLayoutResult *res = fx_layout(fx_built(&o), fx_retail_asm(), fx_retail_blob(), NULL);
    CHECK(fx_has(res, "CONTENT", true));
}

AVP_TEST(content_difference_in_table) {
    FxBuilt o = fx_built_defaults();
    o.w[0] = 2;
    GcLayoutResult *res = fx_layout(fx_built(&o), fx_retail_asm(), fx_retail_blob(), NULL);
    CHECK(fx_has(res, "CONTENT", true));
}

AVP_TEST(named_object_renamed_is_a_note) {
    const char *asm_text = fx_replace(fx_retail_asm(), "Table", "gTable");
    FxBuilt o = fx_built_defaults();
    GcLayoutResult *res = fx_layout(fx_built(&o), asm_text, fx_retail_blob(), NULL);
    fx_check_no_differences(res);
    CHECK(fx_has(res, "RENAME", false));
}

AVP_TEST(missing_and_extra) {
    /* retail has an additional named object that the built object lacks */
    const FxAsmObj objs[] = {
        {"lbl_80300000", 0, 3, FX_BODY(".string \"ab\"")},
        {"gap_07_80300003_data", 3, 1, FX_BODY(".byte 0x00")},
        {"lbl_80300004", 4, 3, FX_BODY(".string \"cd\"")},
        {"gap_07_80300007_data", 7, 1, FX_BODY(".byte 0x00")},
        {"Table", 8, 16,
         FX_BODY(".4byte 0x00000001", ".4byte lbl_80300000", ".4byte 0x00000003", ".2byte 0x0004", ".2byte 0x0005")},
        {"Extra", 0x18, 4, FX_BODY(".4byte 0x00000007")},
        {"gap_07_8030001C_data", 0x1C, 0x14, FX_BODY(".skip 0x14")},
    };
    const char *asm_text = fx_data_asm(".data", FX_START, FX_START + 0x30, objs, FX_N(objs));
    char *blob = (char *)fx_copy(fx_retail_blob()).p;
    blob[0x18] = 0; blob[0x19] = 0; blob[0x1A] = 0; blob[0x1B] = 7;
    FxBuilt o = fx_built_defaults();
    GcLayoutResult *res = fx_layout(fx_built(&o), asm_text, avp_strn(blob, 0x30), NULL);
    CHECK(fx_has(res, "MISSING", true));
}

AVP_TEST(total_size) {
    const char *asm_text = fx_replace(fx_retail_asm(), "0x80300000..0x80300030 | size: 0x30",
                                      "0x80300000..0x80300080 | size: 0x80");
    FxBuilt o = fx_built_defaults();
    GcLayoutResult *res = fx_layout(fx_built(&o), asm_text, fx_cat(fx_retail_blob(), fx_fill(0, 0x50)), NULL);
    CHECK(fx_has(res, "TOTAL", true));
}

AVP_TEST(deadstrip_of_unreferenced_data) {
    /* @2 is not referenced by anything and has no retail counterpart name: dropped by the link, but the retail
     * layout must then not contain it either */
    const FxAsmObj objs[] = {
        {"lbl_80300000", 0, 3, FX_BODY(".string \"ab\"")},
        {"gap_07_80300003_data", 3, 1, FX_BODY(".byte 0x00")},
        {"Table", 4, 16,
         FX_BODY(".4byte 0x00000001", ".4byte lbl_80300000", ".4byte 0x00000003", ".2byte 0x0004", ".2byte 0x0005")},
        {"gap_07_80300014_data", 0x14, 0xC, FX_BODY(".skip 0xC")},
    };
    const char *asm_text = fx_data_asm(".data", FX_START, FX_START + 0x20, objs, FX_N(objs));
    AvpBuf b;
    avp_buf_init(&b, avp_arena());
    avp_buf_putn(&b, "ab\0\0", 4);
    fx_be32(&b, 1);
    fx_be32(&b, FX_START);
    fx_be32(&b, 3);
    fx_be16(&b, 4);
    fx_be16(&b, 5);
    while (b.n < 0x20) avp_buf_putc(&b, 0);
    FxBuilt o = fx_built_defaults();
    o.ref_second = false;
    GcLayoutResult *res = fx_layout(fx_built(&o), asm_text, avp_buf_str(&b), NULL);
    fx_check_no_differences(res);
    CHECK(fx_has(res, "DEADSTRIP", false));
}

AVP_TEST(deadstrip_realigns_a_later_double) {
    /* a dead 4-byte object sits before a 4-byte object and an 8-aligned double: the link closes the gap, then pads
     * before the double again, so built minus retail goes +0, +4, +0 with no real difference */
    const FxAsmObj objs[] = {
        {"A", 0, 4, FX_BODY(".4byte 0x00000001")},
        {"X", 4, 4, FX_BODY(".4byte 0x00000002")},
        {"B", 8, 4, FX_BODY(".4byte 0x00000003")},
        {"gap_07_8030000C_data", 0xC, 4, FX_BODY(".skip 4")},
        {"D", 0x10, 8, FX_BODY(".4byte 0x00000004", ".4byte 0x00000005")},
    };
    const char *asm_text = fx_data_asm(".data", FX_START, FX_START + 0x18, objs, FX_N(objs));
    AvpBuf r, bd;
    avp_buf_init(&r, avp_arena());
    avp_buf_init(&bd, avp_arena());
    const u32 rw[6] = {1, 2, 3, 0, 4, 5}, bw[6] = {1, 2, 9, 3, 4, 5};
    for (int i = 0; i < 6; i++) {
        fx_be32(&r, rw[i]);
        fx_be32(&bd, bw[i]);
    }
    FxElfSym syms[7] = {{"x.c", 0, 0, GC_STT_FILE, 0, GC_SHN_ABS}, {"A", 0, 4, GC_STT_OBJECT, 0, 1},
                        {"X", 4, 4, GC_STT_OBJECT, 0, 1},           {"@2", 8, 4, GC_STT_OBJECT, 0, 1},
                        {"B", 12, 4, GC_STT_OBJECT, 0, 1},          {"D", 16, 8, GC_STT_OBJECT, 0, 1},
                        {"func", 0, 16, GC_STT_FUNC, 1, 2}};
    FxElfRel rels[4] = {{2, 0, 2, 4, 0}, {2, 4, 3, 4, 0}, {2, 8, 5, 4, 0}, {2, 12, 6, 4, 0}};
    FxElfSec secs[2] = {{".data", 1, 3, 8, avp_buf_str(&bd), 0, 0},
                        {".text", 1, 6, 4, AVP_S("`\0\0\0`\0\0\0`\0\0\0`\0\0\0"), 0, 0}};
    GcLayoutResult *res = fx_layout(fx_elf(secs, 2, syms, 7, rels, 4), asm_text, avp_buf_str(&r), NULL);
    fx_check_no_differences(res);
    CHECK(fx_has(res, "DEADSTRIP", false));
}

AVP_TEST(deadstrip_does_not_excuse_unexplained_padding) {
    /* the same +0, +4, +0 delta pattern, but C is a word at +0xC: no alignment the built C can have puts it 4 bytes
     * after B once the dead @2 is dropped (the linked C sits at +8), so the retail padding before C is a real
     * difference */
    const FxAsmObj objs[] = {
        {"A", 0, 4, FX_BODY(".4byte 0x00000001")},
        {"B", 4, 4, FX_BODY(".4byte 0x00000003")},
        {"gap_07_80300008_data", 8, 4, FX_BODY(".skip 4")},
        {"C", 0xC, 4, FX_BODY(".4byte 0x00000004")},
    };
    const char *asm_text = fx_data_asm(".data", FX_START, FX_START + 0x10, objs, FX_N(objs));
    AvpBuf r, bd;
    avp_buf_init(&r, avp_arena());
    avp_buf_init(&bd, avp_arena());
    const u32 rw[4] = {1, 3, 0, 4}, bw[4] = {1, 9, 3, 4};
    for (int i = 0; i < 4; i++) {
        fx_be32(&r, rw[i]);
        fx_be32(&bd, bw[i]);
    }
    FxElfSym syms[6] = {{"x.c", 0, 0, GC_STT_FILE, 0, GC_SHN_ABS}, {"A", 0, 4, GC_STT_OBJECT, 0, 1},
                        {"@2", 4, 4, GC_STT_OBJECT, 0, 1},          {"B", 8, 4, GC_STT_OBJECT, 0, 1},
                        {"C", 12, 4, GC_STT_OBJECT, 0, 1},          {"func", 0, 12, GC_STT_FUNC, 1, 2}};
    FxElfRel rels[3] = {{2, 0, 2, 4, 0}, {2, 4, 4, 4, 0}, {2, 8, 5, 4, 0}};
    FxElfSec secs[2] = {{".data", 1, 3, 8, avp_buf_str(&bd), 0, 0},
                        {".text", 1, 6, 4, AVP_S("`\0\0\0`\0\0\0`\0\0\0"), 0, 0}};
    GcLayoutResult *res = fx_layout(fx_elf(secs, 2, syms, 6, rels, 3), asm_text, avp_buf_str(&r), NULL);
    CHECK(fx_has(res, "OFFSET", true));
    CHECK(fx_has(res, "DEADSTRIP", false));
}

/* ---- weak objects the linker deduplicates ---- */
static const char *weak_asm(void) {
    /* retail .data: Table (its word at +4 points at the RTTI record, which another unit holds) */
    const FxAsmObj objs[] = {
        {"Table", 0, 16,
         FX_BODY(".4byte 0x00000001", ".4byte __RTTI__X", ".4byte 0x00000003", ".4byte 0x00000004")},
        {"gap_07_80300010_data", 0x10, 0x10, FX_BODY(".skip 0x10")},
    };
    return fx_data_asm(".data", FX_START, FX_START + 0x20, objs, FX_N(objs));
}
static AvpStr weak_blob(void) {
    AvpBuf b;
    avp_buf_init(&b, avp_arena());
    fx_be32(&b, 1);
    fx_be32(&b, 0x80400000u);
    fx_be32(&b, 3);
    fx_be32(&b, 4);
    while (b.n < 0x20) avp_buf_putc(&b, 0);
    return avp_buf_str(&b);
}
/* Table, a locally compiled copy of __RTTI__X (global symbol, points at the string @3) and @3. */
static AvpStr weak_built(void) {
    AvpBuf b;
    avp_buf_init(&b, avp_arena());
    fx_be32(&b, 1);
    fx_be32(&b, 0);
    fx_be32(&b, 3);
    fx_be32(&b, 4);
    for (int i = 0; i < 8; i++) avp_buf_putc(&b, 0);
    avp_buf_putn(&b, "name\0\0\0\0", 8);
    FxElfSym syms[4] = {{"x.cpp", 0, 0, GC_STT_FILE, 0, GC_SHN_ABS},
                        {"Table", 0, 16, GC_STT_OBJECT, 1, 1},
                        {"__RTTI__X", 16, 8, GC_STT_OBJECT, 1, 1},
                        {"@3", 24, 5, GC_STT_OBJECT, 0, 1}};
    FxElfRel rels[2] = {{1, 4, 3, 1, 0}, {1, 16, 4, 1, 0}};
    FxElfSec sec = {".data", 1, 3, 8, avp_buf_str(&b), 0, 0};
    return fx_elf(&sec, 1, syms, 4, rels, 2);
}
static GcDedupeVec dedupe_of(const char *name, const char *unit) {
    GcDedupeVec v = {0};
    avp_vec_push(&v, ((GcDedupe){name, unit}));
    return v;
}

AVP_TEST(weak_duplicate_is_extra_without_dedupe) {
    GcLayoutResult *res = fx_layout(weak_built(), weak_asm(), weak_blob(), NULL);
    CHECK(fx_has(res, "EXTRA", true));
}

AVP_TEST(weak_duplicate_is_deduplicated) {
    GcDedupeVec dd = dedupe_of("__RTTI__X", "game/Other.cpp");
    GcLayoutResult *res = fx_layout(weak_built(), weak_asm(), weak_blob(), &dd);
    fx_check_no_differences(res);
    REQUIRE(fx_count(res, "DEDUP", false) + fx_count(res, "DEDUP", true) == 1);
    const GcFinding *f = fx_find(res, "DEDUP");
    REQUIRE(f != NULL);
    CHECK(strstr(f->text, "retail copy in game/Other.cpp") != NULL);
    CHECK(strstr(f->text, "__RTTI__X") != NULL);
    CHECK(strstr(f->text, "@3") != NULL); /* the type-name string only the record references goes with it */
    CHECK(!fx_has(res, "DEADSTRIP", false));
}

AVP_TEST(dedupe_does_not_hide_other_extras) {
    /* a name that is not the duplicate (or not in the index at all) stays an EXTRA */
    GcDedupeVec dd = dedupe_of("Unrelated", "game/Other.cpp");
    GcLayoutResult *res = fx_layout(weak_built(), weak_asm(), weak_blob(), &dd);
    CHECK(fx_has(res, "EXTRA", true));
    CHECK(!fx_has(res, "DEDUP", false));
}

AVP_TEST(dedupe_ignores_names_the_unit_defines) {
    /* the retail asm of this unit defines the object: the index entry is the unit's own copy */
    const FxAsmObj objs[] = {
        {"Table", 0, 16,
         FX_BODY(".4byte 0x00000001", ".4byte __RTTI__X", ".4byte 0x00000003", ".4byte 0x00000004")},
        {"__RTTI__X", 0x10, 8, FX_BODY(".4byte 0x00000000", ".4byte 0x00000000")},
        {"lbl_80300018", 0x18, 8, FX_BODY(".string \"name\"")},
    };
    const char *asm_text = fx_data_asm(".data", FX_START, FX_START + 0x20, objs, FX_N(objs));
    GcDedupeVec dd = dedupe_of("__RTTI__X", "game/Other.cpp");
    GcLayoutResult *res = fx_layout(weak_built(), asm_text, weak_blob(), &dd);
    CHECK(!fx_has(res, "DEDUP", false));
    CHECK(!fx_has(res, "EXTRA", true));
}

static bool has_name(const GcWeakVec *v, const char *name) {
    for (size_t i = 0; i < v->n; i++)
        if (strcmp(v->v[i].name, name) == 0) return true;
    return false;
}
static bool dd_has(const GcDedupeVec *v, const char *name) {
    for (size_t i = 0; i < v->n; i++)
        if (strcmp(v->v[i].name, name) == 0) return true;
    return false;
}
static const char *dd_get(const GcDedupeVec *v, const char *name) {
    for (size_t i = 0; i < v->n; i++)
        if (strcmp(v->v[i].name, name) == 0) return v->v[i].unit;
    return NULL;
}

AVP_TEST(weak_dedupe_index) {
    AvpArena *a = avp_arena();
    GcSplitVec splits = gc_parse_splits(a, AVP_S("game/A.cpp:\n\t.data       start:0x80300000 end:0x80300020\n"
                                                 "game/B.cpp:\n\t.data       start:0x80300020 end:0x80300040\n"));
    GcSymTxtVec symbols = gc_parse_symbols_txt(
        a, AVP_S("inA = .data:0x80300008; // type:object size:0x8 scope:weak\n"
                 "inB = .data:0x80300028; // type:object size:0x8 scope:weak\n"
                 "unsplit = .data:0x80300100; // type:object size:0x8 scope:weak\n"
                 "plain = .data:0x80300030; // type:object size:0x8 scope:global\n"));
    GcWeakVec index = gc_weak_dedupe_index(a, &symbols, &splits);
    CHECK_EQ_U64(index.n, 3);
    CHECK(has_name(&index, "inA") && has_name(&index, "inB") && has_name(&index, "unsplit"));
    CHECK(!has_name(&index, "plain"));
    GcDedupeVec got = gc_dedupe_for_unit(a, &index, "main/game/A.cpp");
    CHECK_EQ_U64(got.n, 2);
    CHECK(dd_has(&got, "inB") && dd_has(&got, "unsplit") && !dd_has(&got, "inA"));
    CHECK_CZ(dd_get(&got, "inB"), "game/B.cpp");
    CHECK(strstr(dd_get(&got, "unsplit"), "0x80300100") != NULL);
    GcDedupeVec got2 = gc_dedupe_for_unit(a, &index, "game/B");
    CHECK_EQ_U64(got2.n, 2);
    CHECK(dd_has(&got2, "inA") && dd_has(&got2, "unsplit") && !dd_has(&got2, "inB"));
}

static const char *coarse_asm(void) {
    /* dtk made one object where the compiler emitted two strings and a table */
    const FxAsmObj objs[] = {
        {"lbl_80300000", 0, 0x18,
         FX_BODY(".string \"ab\"", ".byte 0x00", ".string \"cd\"", ".byte 0x00", ".4byte 0x00000001",
                 ".4byte lbl_80300000", ".4byte 0x00000003", ".2byte 0x0004", ".2byte 0x0005")},
        {"gap_07_80300018_data", 0x18, 0x18, FX_BODY(".skip 0x18")},
    };
    return fx_data_asm(".data", FX_START, FX_START + 0x30, objs, FX_N(objs));
}

AVP_TEST(coarse_retail_object) {
    FxBuilt o = fx_built_defaults();
    GcLayoutResult *res = fx_layout(fx_built(&o), coarse_asm(), fx_retail_blob(), NULL);
    fx_check_no_differences(res);
    CHECK(fx_has(res, "COARSE", false));
}

AVP_TEST(coarse_content_difference) {
    FxBuilt o = fx_built_defaults();
    o.s1 = AVP_S("zz\0");
    GcLayoutResult *res = fx_layout(fx_built(&o), coarse_asm(), fx_retail_blob(), NULL);
    CHECK(fx_has(res, "CONTENT", true));
}

static GcElf *bss_elf(const char *const *names, const int *sizes, int n) {
    FxElfSym syms[4];
    size_t ns = 0;
    syms[ns++] = (FxElfSym){"x.c", 0, 0, GC_STT_FILE, 0, GC_SHN_ABS};
    u32 off = 0;
    for (int i = 0; i < n; i++) {
        syms[ns++] = (FxElfSym){names[i], off, (u32)sizes[i], GC_STT_OBJECT, 0, 1};
        off += (u32)sizes[i];
    }
    FxElfSec sec = {".bss", GC_SHT_NOBITS, 3, 8, {0}, off, 0};
    return fx_elf_parse(fx_elf(&sec, 1, syms, ns, NULL, 0));
}

AVP_TEST(bss_order) {
    AvpArena *a = avp_arena();
    const FxAsmObj objs[] = {{"A", 0, 0x10, FX_BODY(".skip 0x10")},
                             {"B", 0x10, 0x20, FX_BODY(".skip 0x20")},
                             {"gap_08_80400030_bss", 0x30, 0x10, FX_BODY(".skip 0x10")}};
    const char *asm_text = fx_data_asm(".bss", 0x80400000u, 0x80400040u, objs, FX_N(objs));
    ShsmDol *d = fx_dol_parse(fx_dol1(7, FX_START, fx_fill(0, 16)));
    GcAsmUnit *au = gc_parse_asm_data(a, avp_str(asm_text), d);
    const char *ab[] = {"A", "B"};
    int abs_[] = {0x10, 0x20};
    GcLayoutResult *ok = fx_result_new();
    gc_compare_unit(a, ok, au, gc_built_sections(a, bss_elf(ab, abs_, 2), au->names, NULL), NULL);
    fx_check_no_differences(ok);
    const char *ba[] = {"B", "A"};
    int bas[] = {0x20, 0x10};
    GcLayoutResult *bad = fx_result_new();
    gc_compare_unit(a, bad, au, gc_built_sections(a, bss_elf(ba, bas, 2), au->names, NULL), NULL);
    CHECK(fx_has(bad, "ORDER", true) || fx_has(bad, "OFFSET", true));
}

/* Two zero-filled counters of one size, defined in the wrong order (MWCC puts the later definition first): equal in
 * size and content, they differ only in which name sits at which address. */
AVP_TEST(sbss_counters_in_the_wrong_order) {
    AvpArena *a = avp_arena();
    const FxAsmObj objs[] = {{"lbl_80400000", 0, 4, FX_BODY(".skip 4")}, {"lbl_80400004", 4, 4, FX_BODY(".skip 4")}};
    const char *asm_text = fx_data_asm(".bss", 0x80400000u, 0x80400008u, objs, FX_N(objs));
    ShsmDol *d = fx_dol_parse(fx_dol1(7, FX_START, fx_fill(0, 16)));
    GcAsmUnit *au = gc_parse_asm_data(a, avp_str(asm_text), d);
    const char *good[] = {"lbl_80400000", "lbl_80400004"};
    int sz[] = {4, 4};
    GcLayoutResult *ok = fx_result_new();
    gc_compare_unit(a, ok, au, gc_built_sections(a, bss_elf(good, sz, 2), au->names, NULL), NULL);
    fx_check_no_differences(ok);
    const char *swapped[] = {"lbl_80400004", "lbl_80400000"};
    GcLayoutResult *bad = fx_result_new();
    gc_compare_unit(a, bad, au, gc_built_sections(a, bss_elf(swapped, sz, 2), au->names, NULL), NULL);
    CHECK(fx_has(bad, "ORDER", true));
}

AVP_TEST(loose_ctors_section) {
    AvpArena *a = avp_arena();
    const char *asm_text = "\n# 0x80300000..0x80300004 | size: 0x4\n.section .ctors, \"a\"\n.balign 4\n\t.4byte __sinit_x_cpp\n";
    ShsmDol *d = fx_dol_parse(fx_dol1(7, FX_START, AVP_S("\x80\x01\x02\x03")));
    GcAsmUnit *au = gc_parse_asm_data(a, avp_str(asm_text), d);
    GcAsmSection *cs = gc_asm_section(au, ".ctors");
    REQUIRE(cs != NULL && cs->objs.n >= 1);
    const GcObj *o = cs->objs.v[0];
    CHECK_EQ_I64(o->size, 4);
    REQUIRE(o->relocs.n == 1);
    CHECK_EQ_I64(o->relocs.v[0].off, 0);
    FxElfSec secs[2] = {{".ctors", 1, 2, 4, fx_fill(0, 4), 0, 0}, {".text", 1, 6, 4, fx_fill(0, 4), 0, 0}};
    FxElfSym syms[1] = {{"__sinit_x_cpp", 0, 4, GC_STT_FUNC, 0, 2}};
    FxElfRel rels[1] = {{1, 0, 1, 1, 0}};
    GcElf *elf = fx_elf_parse(fx_elf(secs, 2, syms, 1, rels, 1));
    GcLayoutResult *res = fx_result_new();
    gc_compare_unit(a, res, au, gc_built_sections(a, elf, au->names, NULL), NULL);
    fx_check_no_differences(res);
}

/* ---- code sizes ---- */
static GcElf *code_elf(const char *const *names, const int *offs, const int *sizes, int n, int align) {
    FxElfSym syms[4];
    int size = 0;
    for (int i = 0; i < n; i++) {
        syms[i] = (FxElfSym){names[i], (u32)offs[i], (u32)sizes[i], GC_STT_FUNC, 1, 1};
        if (offs[i] + sizes[i] > size) size = offs[i] + sizes[i];
    }
    FxElfSec sec = {".text", 1, 6, (u32)align, fx_fill(0, (size_t)size), 0, 0};
    return fx_elf_parse(fx_elf(&sec, 1, syms, (size_t)n, NULL, 0));
}
/* the findings that are differences */
static GcLayoutResult *code_findings(const char *asm_text, GcElf *elf) {
    AvpArena *a = avp_arena();
    GcLayoutResult *res = fx_result_new();
    gc_compare_code_sizes(a, res, gc_parse_asm_data(a, avp_str(asm_text), NULL), elf, NULL, NULL);
    return res;
}

AVP_TEST(code_size_matches_with_dead_strip_and_padding) {
    /* A (0x10) and B (0xC) are retail functions; C is dead-stripped; retail pads B to 16 bytes */
    const char *n[] = {"A", "C", "B"};
    int o[] = {0, 0x10, 0x20}, s[] = {0x10, 0x8, 0xC};
    GcElf *elf = code_elf(n, o, s, 3, 16);
    FxFn f1[] = {{"A", 0x10}, {"B", 0x10}};
    fx_check_no_differences(code_findings(fx_code_asm(0x80006000u, 0x80006020u, f1, 2), elf));
    /* a split that ends at the last instruction is fine too */
    FxFn f2[] = {{"A", 0x10}, {"B", 0xC}};
    fx_check_no_differences(code_findings(fx_code_asm(0x80006000u, 0x8000601Cu, f2, 2), elf));
    /* dtk's gap_ padding functions are not built functions */
    FxFn f3[] = {{"A", 0x10}, {"B", 0xC}, {"gap_03_8000601C_text", 4}};
    fx_check_no_differences(code_findings(fx_code_asm(0x80006000u, 0x80006020u, f3, 3), elf));
}

AVP_TEST(code_size_difference_is_a_layout_failure) {
    const char *n[] = {"A", "B"};
    int o[] = {0, 0x10}, s[] = {0x10, 0x10};
    GcElf *elf = code_elf(n, o, s, 2, 4);
    FxFn f1[] = {{"A", 0x10}, {"B", 0xC}};
    GcLayoutResult *r = code_findings(fx_code_asm(0x80006000u, 0x8000601Cu, f1, 2), elf);
    REQUIRE(fx_count(r, NULL, true) == 1);
    const GcFinding *f = fx_find(r, "TOTAL");
    REQUIRE(f != NULL && f->difference);
    CHECK_CZ(f->section, ".text");
    CHECK(strstr(f->text, "retail split 0x1C, built 0x20") != NULL);
    /* a retail function the object does not define (an unfinished unit) makes the built code short */
    FxFn f2[] = {{"A", 0x10}, {"B", 0x10}, {"D", 0x10}};
    r = code_findings(fx_code_asm(0x80006000u, 0x80006030u, f2, 3), elf);
    REQUIRE(fx_count(r, NULL, true) == 1);
    f = fx_find(r, "TOTAL");
    REQUIRE(f != NULL);
    CHECK(strstr(f->text, "retail split 0x30, built 0x20") != NULL);
    /* and .init is checked the same way */
    FxFn f3[] = {{"A", 0x10}};
    const char *asm_text = fx_replace(fx_code_asm(0x80004000u, 0x80004010u, f3, 1), "\n.text\n", "\n.section .init, \"ax\"\n");
    FxElfSec sec = {".init", 1, 6, 4, fx_fill(0, 0x14), 0, 0};
    FxElfSym sym = {"A", 0, 0x14, GC_STT_FUNC, 1, 1};
    GcElf *init = fx_elf_parse(fx_elf(&sec, 1, &sym, 1, NULL, 0));
    r = code_findings(asm_text, init);
    REQUIRE(fx_count(r, NULL, true) == 1);
    f = fx_find(r, "TOTAL");
    REQUIRE(f != NULL);
    CHECK_CZ(f->section, ".init");
}

AVP_TEST(function_order_difference_is_a_layout_failure) {
    /* -inline deferred emits the functions last to first: the sizes agree, the order does not */
    const char *rev[] = {"C", "B", "A"};
    int o[] = {0, 0x10, 0x20}, s[] = {0x10, 0x10, 0x10};
    FxFn fns[] = {{"A", 0x10}, {"B", 0x10}, {"C", 0x10}};
    const char *asm_text = fx_code_asm(0x80006000u, 0x80006030u, fns, 3);
    GcLayoutResult *r = code_findings(asm_text, code_elf(rev, o, s, 3, 4));
    REQUIRE(fx_count(r, NULL, true) == 1);
    const GcFinding *f = fx_find(r, "FN-ORDER");
    REQUIRE(f != NULL && f->difference);
    CHECK_CZ(f->section, ".text");
    CHECK(strstr(f->text, "first out-of-order function A (retail position 1 of 3, built position 3)") != NULL);
    /* one function moved: the first one whose place differs is named */
    const char *mv[] = {"A", "C", "B"};
    r = code_findings(asm_text, code_elf(mv, o, s, 3, 4));
    f = fx_find(r, "FN-ORDER");
    REQUIRE(f != NULL);
    CHECK(strstr(f->text, "first out-of-order function B (retail position 2 of 3, built position 3)") != NULL);
    /* the same order, with a dead-stripped function between, is fine */
    const char *same[] = {"A", "X", "B", "C"};
    int o4[] = {0, 0x10, 0x20, 0x30}, s4[] = {0x10, 0x8, 0x10, 0x10};
    fx_check_no_differences(code_findings(fx_code_asm(0x80006000u, 0x80006030u, fns, 3), code_elf(same, o4, s4, 4, 4)));
}

/* D-gatecheck-post-2: rwsdk batextur's .sdata. dtk sizes each 1-byte "" literal with its padding (4 bytes); the built
 * object has two 1-byte literals 4 bytes apart (and the zero-size section symbol at +0). The first retail object is
 * not "covered" by both built literals: each pairs with its own literal. */
AVP_TEST(padded_literals_are_not_one_coarse_object) {
    AvpArena *a = avp_arena();
    const FxAsmObj objs[] = {{"lbl_80300000", 0, 4, FX_BODY(".4byte 0x00000000")},
                             {"lbl_80300004", 4, 4, FX_BODY(".4byte 0x00000000")}};
    const char *asm_text = fx_data_asm(".sdata", FX_START, FX_START + 8, objs, FX_N(objs));
    ShsmDol *d = fx_dol_parse(fx_dol1(7, FX_START, fx_fill(0, 8)));
    GcAsmUnit *au = gc_parse_asm_data(a, avp_str(asm_text), d);
    /* the literals are read by a function (else they would be dead-stripped) */
    FxElfSec secs[2] = {{".sdata", 1, 3, 8, fx_fill(0, 8), 0, 0}, {".text", 1, 6, 4, fx_fill(0, 8), 0, 0}};
    FxElfSym syms[4] = {{"", 0, 0, GC_STT_SECTION, 0, 1}, {"@1795", 0, 1, GC_STT_OBJECT, 0, 1},
                        {"@1885", 4, 1, GC_STT_OBJECT, 0, 1}, {"Reader", 0, 8, GC_STT_FUNC, 1, 2}};
    FxElfRel rels[2] = {{2, 0, 2, 1, 0}, {2, 4, 3, 1, 0}};
    GcElf *elf = fx_elf_parse(fx_elf(secs, 2, syms, 4, rels, 2));
    avp_set_add(au->names, AVP_S("Reader")); /* a retail function: the linker keeps it and what it reads */
    GcLayoutResult *res = fx_result_new();
    gc_compare_unit(a, res, au, gc_built_sections(a, elf, au->names, NULL), NULL);
    for (size_t i = 0; i < res->findings.n; i++)
        CHECK_MSG(!res->findings.v[i].difference && strcmp(res->findings.v[i].kind, "COARSE") != 0, "%s %s",
                  res->findings.v[i].kind, res->findings.v[i].text);
}

/* ---- label-only code sections (assembly source units, user's ruling 2026-11-14; DEVIATIONS.md D-gatecheck-asm-1) ---- */
/* retail .init 0x80004000..0x80004010 covered by dtk's pad_ function; built .init 0x10 bytes with only a label and a
 * branch relocation at +4 (the built word is the unrelocated `bl 0`, retail's the resolved branch) */
static GcLayoutResult *whole_section(AvpStr built, u32 split_end, bool with_retail) {
    AvpArena *a = avp_arena();
    FxFn f[] = {{"pad_00_80004000_init", (int)(split_end - 0x80004000u)}};
    const char *asm_text = fx_replace(fx_code_asm(0x80004000u, split_end, f, 1), "\n.text\n", "\n.section .init, \"ax\"\n");
    FxElfSec sec = {".init", 1, 6, 4, built, 0, 0};
    FxElfSym syms[2] = {{"gTRKInterruptVectorTable", 0, 0, 0, 1, 1}, {"__TRK_reset", 0, 0, 0, 1, 0}};
    FxElfRel rel = {1, 4, 2, 10, 0};
    GcElf *elf = fx_elf_parse(fx_elf(&sec, 1, syms, 2, &rel, 1));
    AvpStr retail_bytes = avp_strn("\x60\x00\x00\x00\x4B\xFF\xFF\xFD\x7C\x60\x00\xA6\x4C\x00\x00\x64", 16);
    ShsmDol *d = with_retail ? fx_dol_parse(fx_dol1(0, 0x80004000u, retail_bytes)) : NULL;
    GcLayoutResult *res = fx_result_new();
    gc_compare_code_sizes(a, res, gc_parse_asm_data(a, avp_str(asm_text), NULL), elf, NULL, d);
    return res;
}
static AvpStr built_init(void) {
    return avp_strn("\x60\x00\x00\x00\x48\x00\x00\x01\x7C\x60\x00\xA6\x4C\x00\x00\x64", 16);
}

AVP_TEST(label_only_section_is_one_whole_object) {
    GcLayoutResult *r = whole_section(built_init(), 0x80004010u, true);
    fx_check_no_differences(r);
    AvpVal *v = avp_map_get(r->sections, AVP_S(".init"));
    REQUIRE(v != NULL);
    CHECK_MSG(strstr((const char *)v->p, "built 0x10 (0 functions kept, 0 dropped, 1 whole section)") != NULL, "%s",
              (const char *)v->p);
    REQUIRE(r->notes.n == 1);
    CHECK(avp_str_contains(r->notes.v[0], AVP_S("matches retail at 0x80004000: 0x10 bytes, 1 relocated words masked")));
    /* without a retail DOL the section is sized only */
    r = whole_section(built_init(), 0x80004010u, false);
    fx_check_no_differences(r);
    CHECK_EQ_U64(r->notes.n, 0);
}

AVP_TEST(label_only_section_content_and_size_differences) {
    AvpStr b = built_init();
    char *c = avp_memdup(avp_arena(), b.p, b.n);
    c[9] = 0x61; /* `mfmsr r3` becomes another register: not a relocated word */
    GcLayoutResult *r = whole_section(avp_strn(c, b.n), 0x80004010u, true);
    REQUIRE(fx_count(r, NULL, true) == 1);
    const GcFinding *f = fx_find(r, "CONTENT");
    REQUIRE(f != NULL && f->difference);
    CHECK_CZ(f->section, ".init");
    CHECK(strstr(f->text, "first difference at +0x9 (0x80004009)") != NULL);
    /* a retail split longer than the assembled section is a size difference */
    r = whole_section(built_init(), 0x80004020u, true);
    f = fx_find(r, "TOTAL");
    REQUIRE(f != NULL && f->difference);
    CHECK(strstr(f->text, "retail split 0x20, built 0x10") != NULL);
    CHECK(strstr(f->text, "1 whole section") != NULL);
}

/* ---- small functions ---- */
AVP_TEST(directive_widths) {
    AvpArena *a = avp_arena();
    GcRelocVec rel = {0};
    CHECK_EQ_I64(gc_advance_directive(a, AVP_S(".string \"a\\n\\001b\""), 0, &rel), 5);
    CHECK_EQ_I64(gc_advance_directive(a, AVP_S(".byte 0x00, 0x01, 0x02"), 0, &rel), 3);
    CHECK_EQ_I64(gc_advance_directive(a, AVP_S(".double 1.5"), 4, &rel), 12);
    CHECK_EQ_I64(gc_advance_directive(a, AVP_S(".4byte lbl_80123456"), 8, &rel), 12);
    REQUIRE(rel.n == 1);
    CHECK_EQ_I64(rel.v[0].off, 8);
    CHECK_CZ(rel.v[0].target, "lbl_80123456");
    CHECK_CZ(rel.v[0].addend, "");
    CHECK_EQ_I64(gc_advance_directive(a, AVP_S(".4byte sym+0x10"), 12, &rel), 16);
    REQUIRE(rel.n == 2);
    CHECK_EQ_I64(rel.v[1].off, 12);
    CHECK_CZ(rel.v[1].target, "sym");
    CHECK_CZ(rel.v[1].addend, "+0x10");
    CHECK_EQ_I64(gc_advance_directive(a, AVP_S(".skip 0x20"), 0, &rel), 0x20);
}

AVP_TEST(anon_names) {
    const char *anon[] = {"lbl_803DEE80", "gap_07_803DEEA6_data", "@980", "jumptable_80123456", "buf$123"};
    for (size_t i = 0; i < FX_N(anon); i++) CHECK_MSG(gc_is_anon(anon[i]), "%s", anon[i]);
    const char *named[] = {"StmEhInBuf", "__vt__3Foo", "OSInit", "gap_cb", "gap_api", "gap_btm_cback0", "gap_x"};
    for (size_t i = 0; i < FX_N(named); i++) CHECK_MSG(!gc_is_anon(named[i]), "%s", named[i]);
}

AVP_TEST(gap_names_are_dtks_only) {
    /* BTE defines a real .bss object gap_cb; only dtk's gap_<idx>_<addr>_<section> names are fillers */
    const char *gaps[] = {"gap_00_800062B4_init", "gap_03_80006C7C_text", "gap_07_803DEEA6_data",
                          "gap_08_804EA220_bss", "gap_12_805A0000_sbss2"};
    for (size_t i = 0; i < FX_N(gaps); i++) CHECK_MSG(gc_is_gap_name(gaps[i]), "%s", gaps[i]);
    const char *other[] = {"gap_cb", "gap_conn_init", "gap_07_data", "gap_07_803DEEA6", "gap_x"};
    for (size_t i = 0; i < FX_N(other); i++) CHECK_MSG(!gc_is_gap_name(other[i]), "%s", other[i]);
    const FxAsmObj objs[] = {{"gap_cb", 0, 0x10, FX_BODY(".skip 0x10")},
                             {"gap_08_80400010_bss", 0x10, 0x30, FX_BODY(".skip 0x30")}};
    const char *asm_text = fx_data_asm(".bss", 0x80400000u, 0x80400040u, objs, FX_N(objs));
    GcAsmUnit *au = gc_parse_asm_data(avp_arena(), avp_str(asm_text), NULL);
    GcAsmSection *sec = gc_asm_section(au, ".bss");
    REQUIRE(sec != NULL && sec->objs.n == 2);
    CHECK_CZ(sec->objs.v[0]->name, "gap_cb");
    CHECK(!sec->objs.v[0]->gap);
    CHECK_CZ(sec->objs.v[1]->name, "gap_08_80400010_bss");
    CHECK(sec->objs.v[1]->gap);
}

AVP_TEST(alignment_check) {
    /* same objects, but the retail section starts at +8: align 8 still divides, no ALIGN */
    const char *asm_text = fx_replace(fx_retail_asm(), "0x80300000..0x80300030", "0x80300008..0x80300038");
    FxBuilt o = fx_built_defaults();
    GcLayoutResult *res = fx_layout(fx_built(&o), asm_text, fx_retail_blob(), NULL);
    CHECK(!fx_has(res, "ALIGN", true));
}

/* ---- AlignTests ---- */
static GcObj **objs_of(const char *const *names, const int *sizes, int n) {
    GcObj **v = AVP_NEWN(avp_arena(), GcObj *, n);
    int off = 0;
    for (int i = 0; i < n; i++) {
        v[i] = gc_obj_new(avp_arena(), names[i], off, sizes[i], NULL, 0, false, false);
        off += sizes[i];
    }
    return v;
}

AVP_TEST(lcs_pairs_by_name_and_size) {
    const char *rn[] = {"A", "lbl_1", "B"}, *bn[] = {"A", "@5", "B"};
    int sz[] = {4, 8, 4};
    GcPairVec p = gc_align_objects(objs_of(rn, sz, 3), 3, objs_of(bn, sz, 3), 3);
    REQUIRE(p.n == 3);
    for (int i = 0; i < 3; i++) {
        CHECK_EQ_I64(p.v[i].i, i);
        CHECK_EQ_I64(p.v[i].j, i);
    }
}

AVP_TEST(lcs_detects_swap) {
    const char *rn[] = {"A", "B", "C"}, *bn[] = {"B", "A", "C"};
    int sz[] = {4, 4, 4};
    GcPairVec p = gc_align_objects(objs_of(rn, sz, 3), 3, objs_of(bn, sz, 3), 3);
    CHECK_EQ_U64(p.n, 2);
}

/* A run of equal-looking objects (the same __FILE__ literal six times) on the built side; on the retail side the first
 * three are one coarse object and one lone literal follows it: the lone retail copy is paired with the built copy at
 * its own offset, not with the first copy of the run. */
AVP_TEST(lcs_pairs_a_run_of_equal_objects_by_offset) {
    AvpArena *a = avp_arena();
    GcObj *R[] = {gc_obj_new(a, "lbl_8044E3B8", 0x00, 0x3C, NULL, 0, false, false),
                  gc_obj_new(a, "lbl_8044E3F4", 0x3C, 0x11, NULL, 0, false, false),
                  gc_obj_new(a, "lbl_8044E408", 0x50, 0x74, NULL, 0, false, false),
                  gc_obj_new(a, "lbl_8044E47C", 0xC4, 0x11, NULL, 0, false, false)};
    GcObj *B[10];
    for (int k = 0; k < 6; k++)
        B[k] = gc_obj_new(a, avp_sprintf(a, "@%d", 2198 + k), 0x14 * k, 0x11, NULL, 0, false, false);
    B[6] = gc_obj_new(a, "@2293", 0x78, 0x11, NULL, 0, false, false);
    B[7] = gc_obj_new(a, "@2294", 0x8C, 0x21, NULL, 0, false, false);
    B[8] = gc_obj_new(a, "@2295", 0xB0, 0x11, NULL, 0, false, false);
    B[9] = gc_obj_new(a, "@2301", 0xC4, 0x11, NULL, 0, false, false);
    GcPairVec p = gc_align_objects(R, 4, B, 10);
    REQUIRE(p.n == 2);
    CHECK_EQ_I64(p.v[0].i, 1);
    CHECK_EQ_I64(p.v[0].j, 3); /* built +0x3C, not the first literal at +0 */
    CHECK_EQ_I64(p.v[1].i, 3);
    CHECK_EQ_I64(p.v[1].j, 9);
}

AVP_TEST_MAIN("gatecheck")
