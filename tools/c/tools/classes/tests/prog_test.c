/* prog_test.c: classes_prog (tools/classes.py lines 182-923) on synthetic listings.
 *
 * With CLASSES_PROG_DUMP=<file> in the environment the test `dump_real_tree` parses the real tree
 * (build/R5WEA4/asm, config/R5WEA4/symbols.txt, config/R5WEA4/splits.txt) and writes the canonical dump the proof
 * script compares with the Python's (every fact of Program, FuncInfo, the vtable finders and parse_data_regions);
 * without it the test does nothing. */
#include "avp/test.h"
#include "classes_prog.h"

#include <stdlib.h>

static AvpStr S(const char *z) { return avp_str(z); }

/* ---- parsing -------------------------------------------------------------------------------------------------- */

static const char *ASM1 =
    ".include \"macros.inc\"\n"
    ".section .text, \"ax\"  # 0x80006C7C - 0x80006D00\n"
    "\n"
    "# CFoo::CFoo()\n"
    ".fn __ct__4CFooFv, global\n"
    "/* 80006C7C 00000000  3C 80 80 3F */\tlis r4, __vt__4CFoo@ha\n"
    "/* 80006C80 00000004  38 84 8B 78 */\taddi r4, r4, __vt__4CFoo@l\n"
    "/* 80006C84 00000008  90 83 00 00 */\tstw r4, 0x0(r3)\n"
    "/* 80006C88 0000000C  88 A3 00 04 */\tlbz r5, 0x4(r3)\n"
    "/* 80006C8C 00000010  7C A5 07 74 */\textsb r5, r5\n"
    "/* 80006C90 00000014  80 C3 00 08 */\tlwz r6, 0x8(r3)\n"
    "/* 80006C94 00000018  C0 26 00 10 */\tlfs f1, 0x10(r6)\n"
    "/* 80006C98 0000001C  4E 80 00 20 */\tblr \n"
    ".endfn __ct__4CFooFv\n"
    "\n"
    "# not a signature\n"
    ".fn \"@8@fn_1__4CFooFv\", local\n"
    "/* 80006C9C 00000020  38 63 FF F8 */\tsubi r3, r3, 0x8\n"
    "/* 80006CA0 00000024  48 00 00 04 */\tb fn_1__4CFooFv\n"
    ".endfn \"@8@fn_1__4CFooFv\"\n"
    "\n"
    ".fn fn_1__4CFooFv, global\n"
    "/* 80006CA4 00000028  38 60 00 10 */\tli r3, 0x10\n"
    "/* 80006CA8 0000002C  48 00 00 01 */\tbl __nw__FUl\n"
    "/* 80006CAC 00000030  48 00 00 01 */\tbl __ct__4CFooFv\n"
    "/* 80006CB0 00000034  90 03 00 0C */\tstw r0, 0xC(r3)\n"
    "/* 80006CB4 00000038  4E 80 00 20 */\tblr\n"
    ".endfn fn_1__4CFooFv\n"
    "\n"
    ".section .data, \"wa\"  # 0x803E8B70 - 0x803E8BA0\n"
    "\n"
    "# .data:0x0 | 0x803E8B70 | size: 0x8\n"
    ".obj lbl_803E8B70, local\n"
    "\t.string \"ab\\x41\\101\\n\"\n"
    "\t.byte 0x00, -1, \"x\"\n"
    ".endobj lbl_803E8B70\n"
    "\n"
    "# .data:0x8 | 0x803E8B78 | size: 0x14\n"
    ".obj __vt__4CFoo, global\n"
    "\t.4byte 0x00000000\n"
    "\t.4byte 0\n"
    "\t.4byte fn_1__4CFooFv\n"
    "\t.4byte \"@8@fn_1__4CFooFv\", 0\n"
    ".endobj __vt__4CFoo\n"
    "\n"
    ".obj lbl_noloc, local\n"
    "\t.4byte lbl_803E8B70+0x4\n"
    "\t.float 1.0\n"
    ".endobj lbl_noloc\n"
    ".obj lbl_open, local\n"
    "\t.4byte 1\n";

static ClassesProgram *prog1(AvpArena *a) {
    AvpStr unit = S("game/Foo"), text = S(ASM1);
    return classes_program_from_texts(a, &unit, &text, 1,
                                      S("__ct__4CFooFv = .text:0x80006C7C; // type:function size:0x20\n"
                                        "x = .data:0x803E8B70; // size:0x8 type:object scope:local\n"
                                        "fn_ext = .text:0x80001000; // type:function\n"
                                        "x = .bss:0x80000010; // type: label\n"));
}

AVP_TEST(parse_functions) {
    AvpArena *a = avp_arena();
    ClassesProgram *p = prog1(a);
    CHECK_EQ_U64(avp_map_len(p->funcs), 3);
    ClassesFunc *f = classes_func(p, S("__ct__4CFooFv"));
    REQUIRE(f != NULL);
    CHECK(f->has_addr);
    CHECK_EQ_I64(f->addr, 0x80006C7C);
    CHECK_STR(f->unit, "game/Foo");
    CHECK_STR(f->section, ".text");
    CHECK_STR(f->demangled, "CFoo::CFoo()");
    CHECK_EQ_U64(f->insns.n, 8);
    CHECK_STR(f->insns.v[0].mn, "lis");
    CHECK_EQ_U64(f->insns.v[0].nops, 2);
    CHECK_STR(f->insns.v[0].ops[1], "__vt__4CFoo@ha");
    CHECK_EQ_U64(f->insns.v[7].nops, 0);
    ClassesFunc *t = classes_func(p, S("@8@fn_1__4CFooFv"));
    REQUIRE(t != NULL);
    CHECK(CLASSES_NONE(t->demangled)); /* "# not a signature" has no '(' */
    CHECK_STR(classes_method_text(f), "CFoo()");
    CHECK(CLASSES_NONE(classes_method_text(t)));
}

AVP_TEST(parse_objects) {
    AvpArena *a = avp_arena();
    ClassesProgram *p = prog1(a);
    CHECK_EQ_U64(avp_map_len(p->objs), 3); /* lbl_open has no .endobj */
    ClassesObj *s = classes_obj(p, S("lbl_803E8B70"));
    REQUIRE(s != NULL);
    CHECK(s->has_loc);
    CHECK_EQ_I64(s->addr, 0x803E8B70);
    CHECK_EQ_I64(s->size, 8);
    CHECK_STR(s->string, "ab\\x41\\101\\n");
    CHECK(!s->pure_words);
    REQUIRE(s->has_data);
    CHECK_EQ_U64(s->data.n, 9);
    CHECK_MEM(s->data.v, "abAA\n\0\0\xff\0", 9);
    ClassesObj *v = classes_obj(p, S("__vt__4CFoo"));
    REQUIRE(v != NULL);
    CHECK(v->pure_words);
    CHECK_EQ_U64(v->items.n, 5);
    CHECK_STR(v->items.v[3].name, "@8@fn_1__4CFooFv");
    CHECK_EQ_I64(v->items.v[4].v, 0);
    ClassesObj *n = classes_obj(p, S("lbl_noloc"));
    REQUIRE(n != NULL);
    CHECK(n->has_loc); /* the location comment is never reset */
    CHECK_EQ_I64(n->addr, 0x803E8B78);
    CHECK(!n->has_data);
    CHECK_EQ_I64(n->items.v[0].v, 4);
    CHECK(CLASSES_NONE(classes_cstring(p, S("lbl_803E8B70"), 0))); /* "abAA\n": not printable */
}

/* A listing the last split's record (config.json beside the asm directory) does not name is left by an earlier split:
 * from_dir skips it and says how many; without a record every listing is read. */
static const char *ASM_ONE_FN(AvpArena *a, const char *name, unsigned addr) {
    return avp_sprintf(a, ".section .text, \"ax\"  # 0x%08X - 0x%08X\n\n.fn %s, global\n"
                          "/* %08X 00000000  4E 80 00 20 */\tblr\n.endfn %s\n",
                       addr, addr + 4, name, addr, name);
}

AVP_TEST(from_dir_skips_listings_of_an_earlier_split) {
    AvpArena *a = avp_arena();
    char *dir = avp_mkdtemp(a, "classes_prog", NULL, NULL);
    REQUIRE(dir != NULL);
    char *asm_dir = avp_path_join(a, dir, "asm");
    REQUIRE(avp_makedirs(avp_path_join(a, asm_dir, "game"), true));
    REQUIRE(avp_makedirs(avp_path_join(a, asm_dir, "old"), true));
    const char *cur = ASM_ONE_FN(a, "fn_current", 0x80001000), *old = ASM_ONE_FN(a, "fn_stale", 0x80002000);
    REQUIRE(avp_write_file(avp_path_join(a, avp_path_join(a, asm_dir, "game"), "Foo.s"), cur, strlen(cur)));
    REQUIRE(avp_write_file(avp_path_join(a, avp_path_join(a, asm_dir, "old"), "Bar.s"), old, strlen(old)));
    /* no record: both are read, nothing is reported */
    ClassesProgram *p = classes_program_from_dir(a, asm_dir, NULL);
    CHECK(classes_func(p, S("fn_current")) != NULL && classes_func(p, S("fn_stale")) != NULL);
    CHECK(p->note == NULL);
    const char *rec = "{\"units\": [{\"name\": \"game/Foo.cpp\"}]}";
    REQUIRE(avp_write_file(avp_path_join(a, dir, "config.json"), rec, strlen(rec)));
    p = classes_program_from_dir(a, asm_dir, NULL);
    CHECK(classes_func(p, S("fn_current")) != NULL);
    CHECK(classes_func(p, S("fn_stale")) == NULL);
    REQUIRE(p->note != NULL);
    CHECK(strstr(p->note, "1 stale asm listing") != NULL && strstr(p->note, "old/Bar.s") != NULL);
    avp_tempdir_cleanup(dir);
}

AVP_TEST(parse_symbols) {
    AvpArena *a = avp_arena();
    ClassesProgram *p = prog1(a);
    CHECK_EQ_U64(avp_map_len(p->symbols), 3);
    AvpStrVec keys = avp_map_keys(p->symbols);
    CHECK_STR(keys.v[1], "x"); /* a duplicate keeps its first position ... */
    ClassesSymbol *x = classes_symbol(p, S("x"));
    CHECK_STR(x->section, ".bss"); /* ... and takes the last value */
    CHECK(CLASSES_NONE(x->type));  /* "type: label": no word right after the colon */
    CHECK(classes_is_func(p, S("fn_ext")));
    CHECK(!classes_is_func(p, S("x")));
    i64 ad = 0;
    CHECK(classes_faddr(p, S("fn_ext"), &ad));
    CHECK_EQ_I64(ad, 0x80001000);
    CHECK(!classes_faddr(p, S("nothing"), &ad));
}

AVP_TEST(analyze) {
    AvpArena *a = avp_arena();
    ClassesProgram *p = prog1(a);
    ClassesInfo *ct = classes_info(p, S("__ct__4CFooFv"));
    CHECK(ct->returns_this);
    REQUIRE(ct->stores.n == 1);
    CHECK_STR(ct->stores.v[0].tok, "a3");
    CHECK_STR(ct->stores.v[0].sym, "__vt__4CFoo");
    CHECK_EQ_I64(ct->stores.v[0].addend, 0);
    REQUIRE(ct->access.n == 3); /* 0x10(r6): r6 holds a loaded word, not an object */
    CHECK_EQ_I64(ct->access.v[1].off, 4);
    CHECK_EQ_I64(ct->access.v[1].sign, 1);   /* lbz then extsb */
    CHECK(ct->access.v[2].pointer);          /* lwz r6 then 0x10(r6) */
    CHECK_STR(ct->access.v[0].rw, "store");
    ClassesInfo *th = classes_info(p, S("@8@fn_1__4CFooFv"));
    CHECK(th->has_thunk);
    CHECK_EQ_I64(th->thunk_adjust, 8);
    i64 adj;
    CHECK_STR(classes_resolve_thunk(p, S("@8@fn_1__4CFooFv"), &adj), "fn_1__4CFooFv");
    CHECK_EQ_I64(adj, 8);
    ClassesInfo *f1 = classes_info(p, S("fn_1__4CFooFv"));
    REQUIRE(f1->calls.n == 2);
    CHECK_EQ_I64(f1->calls.v[0].args[0].kind, CLASSES_V_IMM);
    REQUIRE(f1->nw.n == 1);
    CHECK_STR(f1->nw.v[0].tok, "ret@80006CA8");
    CHECK_EQ_I64(f1->nw.v[0].size, 16);
    /* second pass: the constructor returns its r3, so r3 keeps the __nw__ result */
    CHECK_STR(f1->access.v[0].tok, "ret@80006CA8");
    CHECK_EQ_I64(f1->access.v[0].off, 12);
    CHECK(!f1->returns_this);
}

AVP_TEST(analyze_registers) {
    AvpArena *a = avp_arena();
    AvpStr unit = S("u"), text = S(".fn f, global\n"
                                   "/* 80000000 00000000  00 00 00 00 */\tmr r31, r3\n"
                                   "/* 80000004 00000004  00 00 00 00 */\taddi r30, r31, 0x10\n"
                                   "/* 80000008 00000008  00 00 00 00 */\tlis r4, 0x1\n"
                                   "/* 8000000C 0000000C  00 00 00 00 */\tsubi r4, r4, 1\n"
                                   "/* 80000010 00000010  00 00 00 00 */\tli r5, x@sda21\n"
                                   "/* 80000014 00000014  00 00 00 00 */\taddi r31, r31, 4\n"
                                   "/* 80000018 00000018  00 00 00 00 */\tbl _savegpr_25\n"
                                   "/* 8000001C 0000001C  00 00 00 00 */\tbl g\n"
                                   "/* 80000020 00000020  00 00 00 00 */\tstfd f1, -0x8(r30)\n"
                                   "/* 80000024 00000024  00 00 00 00 */\tb .L_1\n"
                                   "/* 80000028 00000028  00 00 00 00 */\tb h\n"
                                   ".endfn f\n");
    ClassesProgram *p = classes_program_from_texts(a, &unit, &text, 1, S(""));
    ClassesInfo *inf = classes_info(p, S("f"));
    REQUIRE(inf->calls.n == 2);
    const ClassesCall *c = &inf->calls.v[0];
    CHECK_STR(c->target, "g");
    CHECK_EQ_I64(c->args[0].kind, CLASSES_V_OBJ); /* r3 */
    CHECK_EQ_I64(c->args[1].kind, CLASSES_V_IMM); /* r4 = 0x10000 - 1 */
    CHECK_EQ_I64(c->args[1].v, 0xFFFF);
    CHECK_EQ_I64(c->args[2].kind, CLASSES_V_SDA);
    CHECK_STR(c->args[2].name, "x");
    CHECK(inf->calls.v[1].tail);
    REQUIRE(inf->access.n == 1);
    CHECK_EQ_I64(inf->access.v[0].off, 8); /* r30 = a3 + 0x10 survives the call (r30 is saved) */
    CHECK_STR(inf->access.v[0].rw, "store");
    CHECK_EQ_I64(inf->access.v[0].width, 8);
    CHECK_EQ_I64(inf->access.v[0].sign, -1);
}

AVP_TEST(mangled_names) {
    AvpArena *a = avp_arena();
    CHECK_STR(classes_mangled_class(a, S("fn_800067CC__8unk_RampFf")), "unk_Ramp");
    CHECK_STR(classes_mangled_class(a, S("__vt__Q36nw4hbm3lyt8Bounding")), "nw4hbm::lyt::Bounding");
    CHECK(CLASSES_NONE(classes_mangled_class(a, S("__vt__4CFooX"))));
    CHECK_STR(classes_mangled_class(a, S("@12@fn__4CBarCFv")), "CBar");
    CHECK_STR(classes_mangled_class(a, S("__ct__4CFooFv")), "CFoo");
    CHECK(CLASSES_NONE(classes_mangled_class(a, S("fn_800C770C__Fi"))));
    CHECK(CLASSES_NONE(classes_mangled_class(a, S("__0Fv"))));
    CHECK_STR(classes_vt_symbol_class(a, S("__vt__4CFoo")), "CFoo");
    CHECK(CLASSES_NONE(classes_vt_symbol_class(a, S("fn__4CFooFv"))));
    AvpStr cls, rem;
    CHECK(!classes_parse_qual(a, S("Q20Fv"), &cls, &rem)); /* the second part has no length */
    CHECK(classes_parse_qual(a, S("Q10Fv"), &cls, &rem));
    CHECK_STR(cls, "");
    CHECK_STR(rem, "Fv");
}

AVP_TEST(helpers) {
    AvpArena *a = avp_arena();
    AvpStrVec ops = classes_split_operands(a, S("r3, \"f<A,0>\" ,  x"));
    REQUIRE(ops.n == 3);
    CHECK_STR(ops.v[1], "\"f<A,0>\"");
    CHECK_STR(ops.v[2], "x");
    CHECK_STR(classes_unquote(S("\"")), "\"");
    i64 v;
    CHECK(classes_parse_int(S(" -0x10 "), &v));
    CHECK_EQ_I64(v, -16);
    CHECK(classes_parse_int(S("--5"), &v));
    CHECK_EQ_I64(v, 5);
    CHECK(classes_parse_int(S("1_0"), &v));
    CHECK_EQ_I64(v, 10);
    CHECK(!classes_parse_int(S("x@l"), &v));
    CHECK_STR(classes_hx(a, 0), "0x0");
    CHECK_STR(classes_hx(a, -0x10), "-0x10");
    CHECK_STR(classes_hx(a, 0x803E8B78), "0x803E8B78");
    ClassesRegions r = classes_parse_data_regions(a, S("Sections:\n\t.data type:data\n"
                                                       "game/B.cpp:\n\t.text start:0x1 end:0x2\n"
                                                       "\t.data start:0x80400000 end:0x80400010\n"
                                                       "climax/A.cpp:\n\t.data       start:0x80300000 end:0x80300008\n"));
    REQUIRE(r.n == 2);
    CHECK_STR(r.v[0].library, "climax");
    CHECK_EQ_I64(r.v[1].start, 0x80400000);
    CHECK_STR(classes_region_of_library(S("climax")), "climax-core");
    CHECK(CLASSES_NONE(classes_region_of_library(S("MSL_C"))));
}

/* ---- vtables -------------------------------------------------------------------------------------------------- */

AVP_TEST(vtables) {
    AvpArena *a = avp_arena();
    ClassesProgram *p = prog1(a);
    AvpMap *rtti = avp_map_new();
    AvpMap *vt = classes_find_vtables(a, p, rtti);
    REQUIRE(avp_map_len(vt) == 1);
    ClassesWords *w = avp_map_get(vt, S("__vt__4CFoo"))->p;
    REQUIRE(w->n == 5); /* a whole table: the trailing zero stays (a null slot) */
    CHECK(CLASSES_NONE(w->v[0]));
    CHECK_STR(w->v[2], "fn_1__4CFooFv");
    CHECK_EQ_U64(avp_map_len(rtti), 0);
}

static const char *ASM_EMB = ".section .data\n"
                             "# .data:0x0 | 0x80400000 | size: 0x20\n"
                             ".obj lbl_80400000, local\n"
                             "\t.4byte 0x12345678\n"
                             "\t.4byte 0x0\n"
                             "\t.4byte __RTTI__4CBaz\n"
                             "\t.4byte 0\n"
                             "\t.4byte m1\n"
                             "\t.4byte 0\n"
                             "\t.4byte m2\n"
                             "\t.4byte __RTTI__4CBaz\n"
                             ".endobj lbl_80400000\n"
                             ".section .text\n"
                             ".fn ctor, global\n"
                             "/* 80001000 00000000  00 00 00 00 */\tlis r4, lbl_80400000@ha\n"
                             "/* 80001004 00000004  00 00 00 00 */\taddi r4, r4, lbl_80400000@l\n"
                             "/* 80001008 00000008  00 00 00 00 */\taddi r5, r4, 0x8\n"
                             "/* 8000100C 0000000C  00 00 00 00 */\tstw r5, 0x0(r3)\n"
                             "/* 80001010 00000010  00 00 00 00 */\taddi r5, r4, 0x10\n"
                             "/* 80001014 00000014  00 00 00 00 */\tstw r5, 0x4(r3)\n"
                             "/* 80001018 00000018  00 00 00 00 */\tblr\n"
                             ".endfn ctor\n"
                             ".fn m1, global\n.endfn m1\n";

AVP_TEST(embedded_vtables) {
    AvpArena *a = avp_arena();
    AvpStr unit = S("u"), text = S(ASM_EMB);
    ClassesProgram *p = classes_program_from_texts(a, &unit, &text, 1, S("m2 = .text:0x80001100; // type:function\n"));
    AvpMap *vt = classes_find_vtables(a, p, NULL);
    CHECK_EQ_U64(avp_map_len(vt), 0); /* the object starts with a non-zero word */
    ClassesEmbeddedList e = classes_find_embedded_vtables(a, p, vt);
    REQUIRE(e.n == 1);
    CHECK_STR(e.v[0].sym, "lbl_80400000");
    CHECK_EQ_I64(e.v[0].base, 8);
    REQUIRE(e.v[0].words.n == 5); /* the trailing RTTI header belongs to the next table */
    CHECK_STR(e.v[0].words.v[2], "m1");
    CHECK(CLASSES_NONE(e.v[0].words.v[3]));
    CHECK_STR(e.v[0].words.v[4], "m2");
    REQUIRE(e.v[0].has_rtti);
    REQUIRE(e.v[0].rtti.n == 1);
    CHECK_EQ_I64(e.v[0].rtti.v[0].k, 0);
    CHECK_STR(e.v[0].rtti.v[0].sym, "__RTTI__4CBaz");
}

AVP_TEST(vtable_prefix_shapes) {
    AvpArena *a = avp_arena();
    ClassesProgram *p = prog1(a);
    ClassesItem z = {CLASSES_IT_INT, {0}, 0}, f = {CLASSES_IT_SYM, S("fn_ext"), 0}, d = {CLASSES_IT_SYM, S("x"), 0};
    ClassesWords w;
    ClassesItem only_zero[] = {z, z, z, z};
    CHECK(!classes_vtable_prefix(a, p, only_zero, 4, true, NULL, false, &w)); /* no function */
    ClassesItem tail_zero[] = {z, z, f, z};
    REQUIRE(classes_vtable_prefix(a, p, tail_zero, 4, true, NULL, false, &w));
    CHECK_EQ_U64(w.n, 4); /* whole, nothing cut: the zero stays */
    REQUIRE(classes_vtable_prefix(a, p, tail_zero, 4, false, NULL, false, &w));
    CHECK_EQ_U64(w.n, 3); /* not pure words: trailing zeros dropped */
    ClassesItem cut[] = {z, z, f, z, z, d};
    REQUIRE(classes_vtable_prefix(a, p, cut, 6, true, NULL, true, &w));
    CHECK_EQ_U64(w.n, 3); /* cut at a data symbol: keep_zero only for a whole table */
}

/* ---- the real-tree dump (proof against the Python) ------------------------------------------------------------ */

static void put_s(AvpBuf *b, AvpStr s) {
    if (CLASSES_NONE(s)) avp_buf_putz(b, "None");
    else avp_buf_put(b, s);
}
static void put_t(AvpBuf *b, int t) { avp_buf_putz(b, t < 0 ? "None" : t ? "True" : "False"); }
static void put_v(AvpBuf *b, const ClassesVal *v) {
    switch (v->kind) {
    case CLASSES_V_OBJ: avp_buf_printf(b, "obj:%.*s:%lld", AVP_SF(v->name), (long long)v->v); break;
    case CLASSES_V_SYM: avp_buf_printf(b, "sym:%.*s:%lld", AVP_SF(v->name), (long long)v->v); break;
    case CLASSES_V_IMM: avp_buf_printf(b, "imm:%lld", (long long)v->v); break;
    case CLASSES_V_HA: avp_buf_printf(b, "ha:%.*s", AVP_SF(v->name)); break;
    case CLASSES_V_SDA: avp_buf_printf(b, "sda:%.*s", AVP_SF(v->name)); break;
    default: avp_buf_putz(b, "None");
    }
}
static void put_words(AvpBuf *b, const ClassesWords *w) {
    for (size_t i = 0; i < w->n; i++) {
        if (i) avp_buf_putc(b, ' ');
        put_s(b, w->v[i]);
    }
}
static void put_rtti(AvpBuf *b, const ClassesRttiList *r) {
    for (size_t i = 0; i < r->n; i++)
        avp_buf_printf(b, "%s%lld=%.*s:%lld", i ? " " : "", (long long)r->v[i].k, AVP_SF(r->v[i].sym),
                       (long long)r->v[i].off);
}

static void dump(const char *out) {
    AvpArena *a = avp_arena();
    double t0 = avp_perf_counter();
    ClassesProgram *p = classes_program_from_dir(a, "build/R5WEA4/asm", "config/R5WEA4/symbols.txt");
    double t1 = avp_perf_counter();
    AvpMap *rtti = avp_map_new();
    AvpMap *vt = classes_find_vtables(a, p, rtti);
    ClassesEmbeddedList emb = classes_find_embedded_vtables(a, p, vt);
    double t2 = avp_perf_counter();
    AvpStr splits;
    REQUIRE(avp_read_text(a, "config/R5WEA4/splits.txt", AVP_TM_UTF8, &splits));
    ClassesRegions regions = classes_parse_data_regions(a, splits);
    AvpBuf b = {0};
    AvpMapIter it = {0};
    while (avp_map_next(p->funcs, &it)) {
        ClassesFunc *f = it.val->p;
        avp_buf_printf(&b, "FUNC %.*s|", AVP_SF(it.key));
        if (f->has_addr) avp_buf_printf(&b, "%lld", (long long)f->addr);
        else avp_buf_putz(&b, "None");
        avp_buf_printf(&b, "|%.*s|", AVP_SF(f->unit));
        put_s(&b, f->section);
        avp_buf_putc(&b, '|');
        put_s(&b, f->demangled);
        avp_buf_putc(&b, '\n');
        for (size_t i = 0; i < f->insns.n; i++) {
            const ClassesInsn *in = &f->insns.v[i];
            avp_buf_printf(&b, " I %lld %.*s %u ", (long long)in->addr, AVP_SF(in->mn), in->nops);
            for (u32 k = 0; k < in->nops; k++) {
                if (k) avp_buf_putc(&b, '\x1f');
                avp_buf_put(&b, in->ops[k]);
            }
            avp_buf_putc(&b, '\n');
        }
        avp_buf_putz(&b, " MC ");
        put_s(&b, classes_mangled_class(a, it.key));
        avp_buf_putz(&b, " MT ");
        put_s(&b, classes_method_text(f));
        avp_buf_putc(&b, '\n');
    }
    memset(&it, 0, sizeof it);
    while (avp_map_next(p->objs, &it)) {
        ClassesObj *o = it.val->p;
        avp_buf_printf(&b, "OBJ %.*s|", AVP_SF(it.key));
        if (o->has_loc) avp_buf_printf(&b, "%lld|%lld", (long long)o->addr, (long long)o->size);
        else avp_buf_putz(&b, "None|None");
        avp_buf_printf(&b, "|%.*s|", AVP_SF(o->unit));
        put_s(&b, o->section);
        avp_buf_putc(&b, '|');
        put_t(&b, o->pure_words);
        avp_buf_putc(&b, '|');
        put_s(&b, o->string);
        avp_buf_putc(&b, '|');
        if (!o->has_data) avp_buf_putz(&b, "None");
        else
            for (size_t i = 0; i < o->data.n; i++) avp_buf_printf(&b, "%02x", o->data.v[i]);
        avp_buf_putc(&b, '\n');
        for (size_t i = 0; i < o->items.n; i++) {
            const ClassesItem *x = &o->items.v[i];
            if (x->kind == CLASSES_IT_INT) avp_buf_printf(&b, " IT int:%lld\n", (long long)x->v);
            else avp_buf_printf(&b, " IT sym:%.*s:%lld\n", AVP_SF(x->name), (long long)x->v);
        }
        avp_buf_putz(&b, " CS ");
        put_s(&b, classes_cstring(p, it.key, 0));
        avp_buf_putc(&b, ' ');
        put_s(&b, classes_cstring(p, it.key, 4));
        avp_buf_putz(&b, " VC ");
        put_s(&b, classes_vt_symbol_class(a, it.key));
        avp_buf_putc(&b, '\n');
    }
    memset(&it, 0, sizeof it);
    while (avp_map_next(p->symbols, &it)) {
        ClassesSymbol *s = it.val->p;
        avp_buf_printf(&b, "SYM %.*s|%.*s|%lld|", AVP_SF(it.key), AVP_SF(s->section), (long long)s->addr);
        put_s(&b, s->type);
        avp_buf_putc(&b, '\n');
    }
    AvpIMapIter ii = {0};
    while (avp_imap_next(p->by_addr, &ii))
        avp_buf_printf(&b, "BA %lld %.*s\n", (long long)ii.key, AVP_SF(((ClassesFunc *)ii.val->p)->name));
    memset(&it, 0, sizeof it);
    while (avp_map_next(p->info, &it)) {
        ClassesInfo *inf = it.val->p;
        avp_buf_printf(&b, "INFO %.*s ", AVP_SF(it.key));
        if (inf->has_thunk) avp_buf_printf(&b, "%lld:%.*s", (long long)inf->thunk_adjust, AVP_SF(inf->thunk_target));
        else avp_buf_putz(&b, "None");
        avp_buf_putc(&b, ' ');
        put_t(&b, inf->calls_dl);
        avp_buf_putc(&b, ' ');
        put_t(&b, inf->returns_this);
        avp_buf_putc(&b, '\n');
        for (size_t i = 0; i < inf->calls.n; i++) {
            const ClassesCall *c = &inf->calls.v[i];
            avp_buf_printf(&b, " C %lld %lld %.*s ", (long long)c->index, (long long)c->addr, AVP_SF(c->target));
            put_t(&b, c->tail);
            avp_buf_putc(&b, ' ');
            bool first = true;
            for (int r = 0; r < 8; r++) {
                if (c->args[r].kind == CLASSES_V_NONE) continue;
                avp_buf_printf(&b, "%s%d=", first ? "" : " ", r + 3);
                put_v(&b, &c->args[r]);
                first = false;
            }
            avp_buf_putc(&b, '\n');
        }
        for (size_t i = 0; i < inf->stores.n; i++) {
            const ClassesStore *s = &inf->stores.v[i];
            avp_buf_printf(&b, " S %lld %.*s %lld %.*s %lld\n", (long long)s->index, AVP_SF(s->tok), (long long)s->off,
                           AVP_SF(s->sym), (long long)s->addend);
        }
        for (size_t i = 0; i < inf->access.n; i++) {
            const ClassesAccess *x = &inf->access.v[i];
            avp_buf_printf(&b, " A %lld %d %.*s %.*s ", (long long)x->off, x->width, AVP_SF(x->kind), AVP_SF(x->rw));
            put_t(&b, x->sign);
            avp_buf_putc(&b, ' ');
            put_t(&b, x->pointer);
            avp_buf_printf(&b, " %.*s\n", AVP_SF(x->tok));
        }
        for (size_t i = 0; i < inf->nw.n; i++)
            avp_buf_printf(&b, " N %.*s %lld\n", AVP_SF(inf->nw.v[i].tok), (long long)inf->nw.v[i].size);
        i64 adj, fa;
        AvpStr tgt = classes_resolve_thunk(p, it.key, &adj);
        avp_buf_printf(&b, " RT %.*s %lld FA ", AVP_SF(tgt), (long long)adj);
        if (classes_faddr(p, it.key, &fa)) avp_buf_printf(&b, "%lld", (long long)fa);
        else avp_buf_putz(&b, "None");
        avp_buf_putz(&b, " IF ");
        put_t(&b, classes_is_func(p, it.key));
        avp_buf_putc(&b, '\n');
    }
    memset(&it, 0, sizeof it);
    while (avp_map_next(vt, &it)) {
        avp_buf_printf(&b, "VT %.*s ", AVP_SF(it.key));
        put_words(&b, it.val->p);
        avp_buf_putc(&b, '\n');
    }
    memset(&it, 0, sizeof it);
    while (avp_map_next(rtti, &it)) {
        avp_buf_printf(&b, "VTR %.*s ", AVP_SF(it.key));
        put_rtti(&b, it.val->p);
        avp_buf_putc(&b, '\n');
    }
    for (size_t i = 0; i < emb.n; i++) {
        avp_buf_printf(&b, "EMB %.*s %lld ", AVP_SF(emb.v[i].sym), (long long)emb.v[i].base);
        put_words(&b, &emb.v[i].words);
        avp_buf_putc(&b, '\n');
    }
    for (size_t i = 0; i < emb.n; i++) {
        if (!emb.v[i].has_rtti) continue;
        avp_buf_printf(&b, "EMBR %.*s %lld ", AVP_SF(emb.v[i].sym), (long long)emb.v[i].base);
        put_rtti(&b, &emb.v[i].rtti);
        avp_buf_putc(&b, '\n');
    }
    for (size_t i = 0; i < regions.n; i++)
        avp_buf_printf(&b, "REG %lld %lld %.*s\n", (long long)regions.v[i].start, (long long)regions.v[i].end,
                       AVP_SF(regions.v[i].library));
    avp_buf_putz(&b, "HX None\n");
    static const i64 hxv[] = {0, 5, -5, 0x803E8B78, -0x10};
    for (int i = 0; i < 5; i++) {
        avp_buf_putz(&b, "HX ");
        avp_buf_put(&b, classes_hx(a, hxv[i]));
        avp_buf_putc(&b, '\n');
    }
    FILE *fp = fopen(out, "wb");
    REQUIRE(fp != NULL);
    fwrite(b.p, 1, b.n, fp);
    fclose(fp);
    fprintf(stderr, "parse+analyze %.2fs vtables %.2fs\n", t1 - t0, t2 - t1);
}

AVP_TEST(dump_real_tree) {
    const char *out = getenv("CLASSES_PROG_DUMP");
    if (out && *out) dump(out);
}

AVP_TEST_MAIN("classes_prog")
