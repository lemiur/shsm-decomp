/* analysis2_test.c: classes_analysis part 2 (tools/classes.py lines 1430-1934) on synthetic listings: names,
 * mangled-only classes, members, fields and conflicts, sizes, propagation into a base, layout and the to_json
 * document. The expected values are the Python's output for the same listings. */
#include "avp/test.h"
#include "classes_analysis.h"

static AvpStr S(const char *z) { return avp_str(z); }

#define ROW(addr, text) "/* " addr " 00000000  00 00 00 00 */\t" text "\n"

/* CBase (ctor reading 0x4, writing 0x8), CDer : CBase (ctor calling CBase's ctor; a 1-byte store and a 2-byte load
 * at 0x10, a word load at 0x4 and a half-word store at 0x6 that overlaps it, a float at 0x14; its table's slot 3 is
 * a CPlain member), fn_new (new CDer, 0x20 bytes), lbl_80400040 (installed by the factory fn_make, registered with
 * the string "CThing" by __sinit_T_cpp), __vt__12unk_80400060 (no installer: shape only), CPlain (no vtable). */
static const char *ASM_T =
    ".section .text\n"
    ".fn __ct__5CBaseFv, global\n"
    ROW("80001000", "lis r4, __vt__5CBase@ha")
    ROW("80001004", "addi r4, r4, __vt__5CBase@l")
    ROW("80001008", "stw r4, 0x0(r3)")
    ROW("8000100C", "lwz r5, 0x4(r3)")
    ROW("80001010", "stw r5, 0x8(r3)")
    ROW("80001014", "blr")
    ".endfn __ct__5CBaseFv\n"
    ".fn fn_a, global\n"
    ROW("80001030", "blr")
    ".endfn fn_a\n"
    ".fn fn_b, global\n"
    ROW("80001034", "blr")
    ".endfn fn_b\n"
    ".fn __ct__4CDerFv, global\n"
    ROW("80001100", "mr r31, r3")
    ROW("80001104", "bl __ct__5CBaseFv")
    ROW("80001108", "lis r4, __vt__4CDer@ha")
    ROW("8000110C", "addi r4, r4, __vt__4CDer@l")
    ROW("80001110", "stw r4, 0x0(r31)")
    ROW("80001114", "li r0, 0x1")
    ROW("80001118", "stb r0, 0x10(r31)")
    ROW("8000111C", "lhz r6, 0x10(r31)")
    ROW("80001120", "lwz r7, 0x4(r31)")
    ROW("80001124", "sth r7, 0x6(r31)")
    ROW("80001128", "lfs f1, 0x14(r31)")
    ROW("8000112C", "mr r3, r31")
    ROW("80001130", "blr")
    ".endfn __ct__4CDerFv\n"
    ".fn fn_new, global\n"
    ROW("80001200", "li r3, 0x20")
    ROW("80001204", "bl __nw__FUl")
    ROW("80001208", "lis r4, __vt__4CDer@ha")
    ROW("8000120C", "addi r4, r4, __vt__4CDer@l")
    ROW("80001210", "stw r4, 0x0(r3)")
    ROW("80001214", "blr")
    ".endfn fn_new\n"
    ".fn fn_make, global\n"
    ROW("80001300", "li r3, 0x18")
    ROW("80001304", "bl __nw__FUl")
    ROW("80001308", "lis r4, lbl_80400040@ha")
    ROW("8000130C", "addi r4, r4, lbl_80400040@l")
    ROW("80001310", "stw r4, 0x0(r3)")
    ROW("80001314", "lwz r5, 0xC(r3)")
    ROW("80001318", "blr")
    ".endfn fn_make\n"
    ".fn __sinit_T_cpp, global\n"
    ROW("80001400", "lis r3, lbl_80400100@ha")
    ROW("80001404", "addi r3, r3, lbl_80400100@l")
    ROW("80001408", "lis r4, fn_make@ha")
    ROW("8000140C", "addi r4, r4, fn_make@l")
    ROW("80001410", "li r5, 0x18")
    ROW("80001414", "bl RegisterClass")
    ROW("80001418", "blr")
    ".endfn __sinit_T_cpp\n"
    ".fn Get__6CPlainFv, global\n"
    ROW("80001500", "lwz r3, 0x4(r3)")
    ROW("80001504", "blr")
    ".endfn Get__6CPlainFv\n"
    ".fn Set__6CPlainFi, global\n"
    ROW("80001508", "stw r4, 0x4(r3)")
    ROW("8000150C", "blr")
    ".endfn Set__6CPlainFi\n"
    ".fn __ct__6CPlainFv, global\n"
    ROW("80001510", "blr")
    ".endfn __ct__6CPlainFv\n"
    ".fn RegisterClass, global\n"
    ROW("80001600", "blr")
    ".endfn RegisterClass\n"
    ".section .data\n"
    "# .data:0x0 | 0x80400000 | size: 0x10\n"
    ".obj __vt__5CBase, global\n"
    "\t.4byte 0\n\t.4byte 0\n\t.4byte fn_a\n\t.4byte fn_b\n"
    ".endobj __vt__5CBase\n"
    "# .data:0x10 | 0x80400010 | size: 0x10\n"
    ".obj __vt__4CDer, global\n"
    "\t.4byte 0\n\t.4byte 0\n\t.4byte fn_a\n\t.4byte Get__6CPlainFv\n"
    ".endobj __vt__4CDer\n"
    "# .data:0x40 | 0x80400040 | size: 0xC\n"
    ".obj lbl_80400040, global\n"
    "\t.4byte 0\n\t.4byte 0\n\t.4byte fn_b\n"
    ".endobj lbl_80400040\n"
    "# .data:0x60 | 0x80400060 | size: 0xC\n"
    ".obj __vt__12unk_80400060, global\n"
    "\t.4byte 0\n\t.4byte 0\n\t.4byte fn_a\n"
    ".endobj __vt__12unk_80400060\n"
    "# .data:0x100 | 0x80400100 | size: 0x8\n"
    ".obj lbl_80400100, global\n"
    "\t.string \"CThing\"\n"
    ".endobj lbl_80400100\n";

/* Two unnamed tables: one installed by fn_ctor, one never installed (placeholder names). */
static const char *ASM_P =
    ".section .text\n"
    ".fn fn_a, global\n" ROW("80002000", "blr") ".endfn fn_a\n"
    ".fn fn_ctor, global\n"
    ROW("80002010", "lis r4, lbl_80410010@ha")
    ROW("80002014", "addi r4, r4, lbl_80410010@l")
    ROW("80002018", "stw r4, 0x0(r3)")
    ROW("8000201C", "blr")
    ".endfn fn_ctor\n"
    ".section .data\n"
    "# .data:0x0 | 0x80410000 | size: 0xC\n"
    ".obj lbl_80410000, global\n\t.4byte 0\n\t.4byte 0\n\t.4byte fn_a\n.endobj lbl_80410000\n"
    "# .data:0x10 | 0x80410010 | size: 0xC\n"
    ".obj lbl_80410010, global\n\t.4byte 0\n\t.4byte 0\n\t.4byte fn_a\n.endobj lbl_80410010\n";

static ClassesAnalysis *an_of(AvpArena *a, const char *text) {
    AvpStr unit = S("game/T"), t = S(text);
    ClassesProgram *p = classes_program_from_texts(a, &unit, &t, 1, S(""));
    return classes_analysis_new(a, p, NULL);
}
static ClassesRec *rec(ClassesAnalysis *an, const char *key) {
    AvpVal *v = avp_map_get(an->classes, S(key));
    return v ? v->p : NULL;
}
static AvpStr dumps(AvpArena *a, const AvpJson *v) { return avp_json_dumps(a, v, &AVP_JSON_COMPACT); }
static AvpJson *cls_json(AvpJson *doc, const char *key) {
    AvpJson *cl = avp_json_getz(doc, "classes");
    for (size_t i = 0; i < avp_json_len(cl); i++)
        if (avp_str_eqz(avp_json_get_str(avp_json_at(cl, i), "key", S("")), key)) return avp_json_at(cl, i);
    return NULL;
}

AVP_TEST(names) {
    ClassesAnalysis *an = an_of(avp_arena(), ASM_T);
    REQUIRE(an->recs.n == 5);
    ClassesRec *der = rec(an, "vt:__vt__4CDer");
    REQUIRE(der != NULL);
    CHECK_STR(der->name, "CDer");
    CHECK_STR(der->name_source, "vt-symbol");
    REQUIRE(der->cands.n == 3); /* vt-symbol, mangled-ctor, the slot's CPlain */
    CHECK_STR(der->cands.v[1].source, "mangled-ctor");
    CHECK_STR(der->cands.v[2].name, "CPlain");
    CHECK_STR(der->cands.v[2].source, "mangled-slot");
    ClassesRec *th = rec(an, "vt:lbl_80400040");
    REQUIRE(th != NULL);
    CHECK_STR(th->name, "CThing");
    CHECK_STR(th->name_source, "registry-string");
    AvpVal *f = avp_map_get(an->factories, S("fn_make"));
    REQUIRE(f != NULL);
    CHECK_STR(*(AvpStr *)f->p, "lbl_80400040");
    AvpVal *r = avp_map_get(an->registry, S("lbl_80400040"));
    REQUIRE(r != NULL);
    const ClassesRegList *rl = r->p;
    REQUIRE(rl->n == 1);
    CHECK_STR(rl->v[0].sinit, "__sinit_T_cpp");
    CHECK(rl->v[0].has_size && rl->v[0].size == 0x18);
    CHECK_STR(rec(an, "vt:__vt__12unk_80400060")->name, "unk_80400060");

    ClassesAnalysis *p = an_of(avp_arena(), ASM_P);
    CHECK_STR(rec(p, "vt:lbl_80410000")->name, "unk_80410000");
    CHECK_STR(rec(p, "vt:lbl_80410000")->name_source, "placeholder (vtable address)");
    CHECK_STR(rec(p, "vt:lbl_80410010")->name, "unk_80002010");
    CHECK_STR(rec(p, "vt:lbl_80410010")->name_source, "placeholder (constructor address)");
}

AVP_TEST(mangled_only_and_members) {
    ClassesAnalysis *an = an_of(avp_arena(), ASM_T);
    ClassesRec *pl = rec(an, "name:CPlain");
    REQUIRE(pl != NULL);
    CHECK(pl->vt == NULL);
    CHECK_STR(pl->name_source, "mangled");
    REQUIRE(pl->members.n == 3);
    CHECK_STR(pl->members.v[0].name, "Get__6CPlainFv");
    CHECK_STR(pl->members.v[2].role, "mangled");
    CHECK_STR(pl->unit, "game/T");
    ClassesRec *der = rec(an, "vt:__vt__4CDer");
    /* slots first (a slot target that is a function), then ctors, then factories */
    REQUIRE(der->members.n == 3);
    CHECK_STR(der->members.v[0].name, "Get__6CPlainFv");
    CHECK_STR(der->members.v[0].role, "slot");
    CHECK_STR(der->members.v[1].role, "ctor");
    CHECK_STR(der->members.v[2].name, "fn_new");
    CHECK_STR(der->members.v[2].role, "factory");
}

AVP_TEST(fields_sizes_propagation_layout) {
    AvpArena *a = avp_arena();
    ClassesAnalysis *an = an_of(a, ASM_T);
    ClassesRec *der = rec(an, "vt:__vt__4CDer"), *base = rec(an, "vt:__vt__5CBase");
    CHECK(der->has_size && der->size == 0x20);
    CHECK_STR(der->size_source, "new in fn_new");
    CHECK(!base->has_size && base->has_size_est && base->size_est == 12);
    CHECK(rec(an, "vt:lbl_80400040")->has_size);
    CHECK_STR(rec(an, "vt:lbl_80400040")->size_source, "new in fn_make");
    /* CDer's base CBase: its range ends at CBase's estimate (0xC), recorded on the base entry */
    REQUIRE(der->bases.n == 1);
    CHECK(der->bases.v[0].rec == base);
    CHECK(der->bases.v[0].has_size_est && der->bases.v[0].size_est == 0xC);
    CHECK(der->has_base_end && der->base_end == 0xC);
    CHECK(der->own_vptrs.n == 0);
    REQUIRE(base->derived.n == 1);
    CHECK_STR(base->derived.v[0], "vt:__vt__4CDer");
    /* CDer's accesses inside CBase's range are added to CBase, with "via" */
    CHECK_STR(dumps(a, base->fields_json),
              "[{\"offset\":\"0x0\",\"width\":4,\"kind\":\"int\",\"type\":\"vptr\",\"loads\":0,\"stores\":3,"
              "\"functions\":3,\"seen_in_derived\":[\"CDer\"]},"
              "{\"offset\":\"0x4\",\"width\":4,\"kind\":\"int\",\"type\":\"u32\",\"loads\":3,\"stores\":0,"
              "\"functions\":3,\"seen_in_derived\":[\"CDer\"]},"
              "{\"offset\":\"0x6\",\"width\":2,\"kind\":\"int\",\"type\":\"u16\",\"loads\":0,\"stores\":1,"
              "\"functions\":1,\"seen_in_derived\":[\"CDer\"],\"conflict\":[\"overlaps field at 0x4\"]},"
              "{\"offset\":\"0x8\",\"width\":4,\"kind\":\"int\",\"type\":\"u32\",\"loads\":0,\"stores\":1,"
              "\"functions\":1}]");
    /* width conflicts before overlaps in field_conflicts; the most common use (first on a tie) gives the type */
    CHECK_STR(dumps(a, der->field_conflicts), "[\"0x10\",\"0x6\"]");
    AvpJson *f10 = avp_json_at(der->fields_json, 3);
    CHECK_STR(dumps(a, f10), "{\"offset\":\"0x10\",\"width\":1,\"kind\":\"int\",\"type\":\"u8\",\"loads\":1,"
                             "\"stores\":1,\"functions\":1,\"conflict\":[\"1-byte int x1\",\"2-byte int x1\"]}");
    CHECK_STR(avp_json_get_str(avp_json_at(der->fields_json, 4), "type", S("")), "f32");
    CHECK_STR(avp_json_get_str(avp_json_at(der->fields_json, 0), "inherited_from", S("")), "CBase");
    ClassesField fl = {0};
    CHECK(strcmp(classes_field_ctype(1, S("int"), &fl, a), "u8") == 0);
    fl.sign = true;
    CHECK(strcmp(classes_field_ctype(2, S("int"), &fl, a), "s16") == 0);
    fl.pointer = true;
    CHECK(strcmp(classes_field_ctype(4, S("int"), &fl, a), "void*") == 0);
    CHECK(strcmp(classes_field_ctype(8, S("paired"), &fl, a), "f32[2]") == 0);
    CHECK(strcmp(classes_field_ctype(8, S("double"), &fl, a), "f64") == 0);
    CHECK(strcmp(classes_field_ctype(12, S("int"), &fl, a), "u8[12]") == 0);
}

AVP_TEST(to_json_shape) {
    AvpArena *a = avp_arena();
    ClassesAnalysis *an = an_of(a, ASM_T);
    AvpJson *in = avp_json_obj(a);
    avp_json_set(a, in, S("asm"), avp_json_strz(a, "x"));
    AvpJson *doc = classes_analysis_to_json(an, in);
    REQUIRE(avp_json_len(doc) == 4);
    CHECK_STR(avp_json_member(doc, 0)->key, "inputs");
    CHECK_STR(avp_json_member(doc, 1)->key, "stats");
    CHECK_STR(avp_json_member(doc, 2)->key, "methods");
    CHECK_STR(avp_json_member(doc, 3)->key, "classes");
    CHECK(avp_json_len(classes_analysis_to_json(an, avp_json_obj(a))) == 3); /* empty inputs: no member */
    AvpJson *st = avp_json_getz(doc, "stats");
    CHECK_STR(dumps(a, st),
              "{\"vtables\":4,\"vtables_installed\":3,\"vtables_mixed_site\":0,\"vtables_shape_only\":1,"
              "\"multi_group_vtables\":0,\"classes\":5,\"classes_with_vtable\":4,\"classes_mangled_only\":1,"
              "\"vtable_class_name_sources\":{\"vt-symbol\":3,\"registry-string\":1},\"named_vtable_classes\":3,"
              "\"unk_vtable_classes\":1,\"classes_with_bases\":2,\"base_edges\":2,"
              "\"base_evidence\":{\"slot-overlap\":1,\"ctor-call\":1},\"name_collisions\":[],\"name_conflicts\":1,"
              "\"classes_with_field_conflicts\":2,\"member_objects\":0,"
              "\"vtable_classes_by_region\":{\"other\":{\"named\":3,\"unk\":1}}}");
    /* classes: vtable classes by address, then the rest by name */
    AvpJson *cl = avp_json_getz(doc, "classes");
    REQUIRE(avp_json_len(cl) == 5);
    CHECK_STR(avp_json_get_str(avp_json_at(cl, 0), "key", S("")), "vt:__vt__5CBase");
    CHECK_STR(avp_json_get_str(avp_json_at(cl, 2), "key", S("")), "vt:lbl_80400040");
    CHECK_STR(avp_json_get_str(avp_json_at(cl, 4), "key", S("")), "name:CPlain");
    const char *vt_keys[] = {"key", "name", "name_source", "name_candidates", "vtable_symbol", "members", "unit",
                             "vtable", "ctors", "dtors", "factories", "other_installers", "registry", "bases", "size",
                             "size_source", "vptrs", "member_objects", "fields", "field_conflicts", "base_end",
                             "own_vptrs", "derived"};
    AvpJson *der = cls_json(doc, "vt:__vt__4CDer");
    REQUIRE(avp_json_len(der) == sizeof vt_keys / sizeof vt_keys[0]);
    for (size_t i = 0; i < sizeof vt_keys / sizeof vt_keys[0]; i++) CHECK_STR(avp_json_member(der, i)->key, vt_keys[i]);
    const char *plain_keys[] = {"key", "name", "name_source", "name_candidates", "vtable_symbol", "members", "unit",
                                "vtable", "ctors", "dtors", "factories", "other_installers", "registry", "bases",
                                "vptrs", "member_objects", "fields", "field_conflicts", "base_end", "own_vptrs",
                                "size_estimate", "derived"};
    AvpJson *pl = cls_json(doc, "name:CPlain");
    REQUIRE(avp_json_len(pl) == sizeof plain_keys / sizeof plain_keys[0]);
    for (size_t i = 0; i < sizeof plain_keys / sizeof plain_keys[0]; i++)
        CHECK_STR(avp_json_member(pl, i)->key, plain_keys[i]);
    CHECK_STR(dumps(a, avp_json_getz(der, "bases")),
              "[{\"class\":\"CBase\",\"key\":\"vt:__vt__5CBase\",\"offset\":\"0x0\",\"vtable_word\":0,"
              "\"primary\":true,\"evidence\":[\"ctor-call __ct__5CBaseFv\"],\"size_estimate\":\"0xC\"}]");
    CHECK_STR(dumps(a, avp_json_getz(cls_json(doc, "vt:lbl_80400040"), "registry")),
              "[{\"string\":\"CThing\",\"sinit\":\"__sinit_T_cpp\",\"factory\":\"fn_make\",\"size\":\"0x18\"}]");
    CHECK_STR(dumps(a, avp_json_getz(der, "members")),
              "[{\"name\":\"__ct__4CDerFv\",\"address\":\"0x80001100\",\"role\":\"ctor\"},"
              "{\"name\":\"fn_new\",\"address\":\"0x80001200\",\"role\":\"factory\"},"
              "{\"name\":\"Get__6CPlainFv\",\"address\":\"0x80001500\",\"role\":\"slot\"}]");
    AvpJson *vt = avp_json_getz(der, "vtable");
    CHECK_STR(dumps(a, avp_json_getz(vt, "groups")), "[{\"addend\":\"0x0\",\"this_offset\":\"0x0\"}]");
    CHECK_STR(dumps(a, avp_json_at(avp_json_getz(vt, "words"), 3)),
              "{\"index\":3,\"offset\":\"0xC\",\"target\":\"Get__6CPlainFv\",\"status\":\"override\","
              "\"base\":\"CBase\",\"base_target\":\"fn_b\",\"base_index\":3,\"group\":\"complete\","
              "\"target_address\":\"0x80001500\"}");
    CHECK_STR(avp_json_get_str(vt, "size", S("")), "0x10");
}

AVP_TEST_MAIN("classes_analysis2")
