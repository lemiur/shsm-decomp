/* lib/shsm_asm on synthetic listings: the cases of tools/common/test_common.py TestAsmParser and the listings part of
 * TestStaleListings. */
#include "shsm_asm.h"
#include "shsm_asm_fixture.h"
#include "shsm_test_util.h"

static ShsmAsmFile *alpha(void) {
    static ShsmAsmFile *af;
    if (!af) af = shsm_asm_parse_text(avp_str(ALPHA_S), "Alpha.s", "game/Alpha", 0);
    return af;
}

#define CHECK_SV(s, lit) CHECK_MSG((s).p && avp_str_eqz((s), (lit)), "%.*s != %s", AVP_SF(s), (lit))

AVP_TEST(functions) {
    ShsmAsmFile *af = alpha();
    ShsmAsmBlock *f = shsm_asm_function(af, AVP_S("fn_80001000__Fv"));
    REQUIRE(f);
    CHECK_EQ_I64(f->address, 0x80001000);
    CHECK_EQ_I64(f->size, 0x10);
    CHECK_SV(f->section, ".text");
    CHECK_SV(f->scope, "global");
    CHECK_SV(f->demangled, "fn_80001000()");
    REQUIRE(f->rows.n == 4);
    const char *mn[] = {"lis", "addi", "lfs", "bl"};
    for (int i = 0; i < 4; i++) CHECK_SV(f->rows.v[i].mnemonic, mn[i]);
    CHECK_SV(f->rows.v[0].sym, "lbl_80002000");
    CHECK_CZ(f->rows.v[0].reloc, "ha");
    CHECK_SV(f->rows.v[2].sym, "lbl_80003000");
    CHECK_CZ(f->rows.v[2].reloc, "sda21");
    CHECK_SV(f->rows.v[3].sym, "helper");
    CHECK_CZ(f->rows.v[3].reloc, "rel24");
    CHECK_EQ_I64(f->rows.v[0].word, 0x3C608000);
    ShsmAsmBlock *h = shsm_asm_function(af, AVP_S("helper"));
    REQUIRE(h && h->labels.n == 1);
    CHECK_EQ_I64(h->labels.v[0].address, 0x80001014);
    CHECK_SV(h->labels.v[0].name, ".L_80001014");
    CHECK_EQ_I64(h->rows.v[1].label, 0x80001014);
    CHECK(h->demangled.p == NULL);
}

AVP_TEST(objects_and_items) {
    ShsmAsmFile *af = alpha();
    ShsmAsmBlock *o = shsm_asm_object(af, AVP_S("lbl_80002008"));
    REQUIRE(o && o->items.n == 5);
    const char *kinds[] = {"ptr", "ptr", "2byte", "byte", "byte"};
    i64 addrs[] = {0x80002008, 0x8000200C, 0x80002010, 0x80002012, 0x80002013};
    for (int i = 0; i < 5; i++) {
        CHECK_CZ(o->items.v[i].kind, kinds[i]);
        CHECK_EQ_I64(o->items.v[i].address, addrs[i]);
    }
    CHECK_SV(o->items.v[1].sym, "lbl_80002000");
    CHECK_EQ_I64(o->items.v[1].offset, 2);
    ShsmAsmBlock *s = shsm_asm_object(af, AVP_S("lbl_80002000"));
    CHECK(s->items.v[0].value.t == SHSM_VAL_STR);
    CHECK_SV(s->items.v[0].value.s, "a\"b\n");
    CHECK(shsm_asm_object(af, AVP_S("lbl_80003008"))->hidden);
    CHECK_SV(shsm_asm_object(af, AVP_S("lbl_80003000"))->section, ".sdata2");
}

static bool has_ref(const ShsmAsmFile *af, const char *from, const char *to, i64 off, const char *kind) {
    for (size_t i = 0; i < af->refs.n; i++) {
        const ShsmAsmRef *r = &af->refs.v[i];
        if (((!from && !r->from_sym.p) || (from && r->from_sym.p && avp_str_eqz(r->from_sym, from))) &&
            avp_str_eqz(r->to_sym, to) && r->to_off == off && strcmp(r->kind, kind) == 0)
            return true;
    }
    return false;
}

AVP_TEST(refs_strings_floats) {
    ShsmAsmFile *af = alpha();
    CHECK(has_ref(af, "fn_80001000__Fv", "helper", 0, "call"));
    CHECK(has_ref(af, "fn_80001018__8unk_RampFf", "fn_80001000__Fv", 0, "branch"));
    CHECK(has_ref(af, "lbl_80002008", "lbl_80002000", 2, "ptr"));
    REQUIRE(af->strings.n == 1);
    CHECK_SV(af->strings.v[0].owner, "lbl_80002000");
    CHECK_EQ_I64(af->strings.v[0].address, 0x80002000);
    CHECK_SV(af->strings.v[0].text, "a\"b\n");
    CHECK_CZ(af->strings.v[0].kind, "string");
    REQUIRE(af->floats.n == 2);
    CHECK_SV(af->floats.v[0].owner, "lbl_80003000");
    CHECK_EQ_F64(af->floats.v[0].value, 1.5);
    CHECK_CZ(af->floats.v[1].kind, "double");
    CHECK_EQ_F64(af->floats.v[1].value, 0.25);
    size_t nlabels = 0;
    for (size_t i = 0; i < af->blocks.n; i++)
        if (strcmp(af->blocks.v[i]->kind, "label") == 0) {
            nlabels++;
            CHECK_SV(af->blocks.v[i]->name, "start_label");
        }
    CHECK_EQ_U64(nlabels, 1);
}

AVP_TEST(loose_data_and_unclosed_block) {
    ShsmAsmFile *af = shsm_asm_parse_text(avp_str(BETA_S), NULL, NULL, 0);
    size_t n = 0;
    for (size_t i = 0; i < af->refs.n; i++)
        if (strcmp(af->refs.v[i].kind, "ptr") == 0) {
            n++;
            CHECK(af->refs.v[i].from_sym.p == NULL);
            CHECK_EQ_I64(af->refs.v[i].from_addr, 0x80004000);
            CHECK_SV(af->refs.v[i].to_sym, "jumper");
        }
    CHECK_EQ_U64(n, 1);
    shsm_asm_free(af);
    af = shsm_asm_parse_text(AVP_S("# .bss:0x0 | 0x80006000 | size: 0x10\n.obj tail, global\n\t.skip 0x10\n"), NULL,
                             NULL, 0);
    REQUIRE(af->blocks.n == 1);
    CHECK_SV(af->blocks.v[0]->name, "tail");
    CHECK_EQ_I64(af->blocks.v[0]->items.v[0].value.i, 0x10);
    shsm_asm_free(af);
}

AVP_TEST(quoted_names_and_jumptable) {
    ShsmAsmFile *af = shsm_asm_parse_text(
        AVP_S("# .text:0x0 | 0x80001000 | size: 0x4\n.fn \"@36@fn_X__3FooFv\", global\n"
              "/* 80001000 00000000  48 00 00 01 */\tbl \"Get__24@unnamed@a_cpp@Fi\"\n.endfn \"@36@fn_X__3FooFv\"\n"
              "# .data:0x0 | 0x80002000 | size: 0x4\n.obj jumptable_80002000, local\n"
              "\t.rel fn_80001000, .L_80001004\n.endobj jumptable_80002000\n"),
        NULL, NULL, 0);
    REQUIRE(af->blocks.n == 2);
    CHECK_SV(af->blocks.v[0]->name, "@36@fn_X__3FooFv");
    CHECK_SV(af->blocks.v[0]->rows.v[0].sym, "Get__24@unnamed@a_cpp@Fi");
    ShsmAsmItem *it = &af->blocks.v[1]->items.v[0];
    CHECK_CZ(it->kind, "rel");
    CHECK_SV(it->sym, "fn_80001000");
    CHECK_EQ_I64(it->value.i, 0x80001004);
    CHECK_CZ(af->refs.v[af->refs.n - 1].kind, "jumptable");
    shsm_asm_free(af);
}

AVP_TEST(string_decoding) {
    AvpStr b = shsm_asm_decode_string(avp_arena(), AVP_S("HID\\000\\000x\\\\y\\tz\\x41"));
    CHECK_EQ_U64(b.n, 11);
    CHECK_MEM(b.p, "HID\0\0x\\y\tzA", 11);
}

AVP_TEST(parser_classifies_rel_into_data) {
    ShsmAsmFile *af = shsm_asm_parse_text(avp_str(TWO_S), NULL, NULL, 0);
    ShsmAsmBlock *ptrs = shsm_asm_object(af, AVP_S("ptrs"));
    REQUIRE(ptrs && ptrs->items.n == 2);
    CHECK_CZ(ptrs->items.v[0].kind, "ptr");
    CHECK_SV(ptrs->items.v[0].sym, "bigBuf");
    CHECK_EQ_I64(ptrs->items.v[0].offset, 4);
    CHECK_CZ(ptrs->items.v[1].kind, "rel");
    CHECK_EQ_I64(ptrs->items.v[1].offset, 0);
    const ShsmAsmRef *r[2];
    size_t n = 0;
    for (size_t i = 0; i < af->refs.n && n < 2; i++)
        if (af->refs.v[i].from_sym.p && avp_str_eqz(af->refs.v[i].from_sym, "ptrs")) r[n++] = &af->refs.v[i];
    REQUIRE(n == 2);
    CHECK_SV(r[0]->to_sym, "bigBuf");
    CHECK_CZ(r[0]->kind, "ptr");
    CHECK_EQ_I64(r[0]->to_off, 4);
    CHECK_EQ_I64(r[0]->to_base, 0x80009000);
    CHECK_EQ_I64(r[0]->to_addr, 0x80009004);
    CHECK_CZ(r[1]->kind, "jumptable");
    CHECK_EQ_I64(r[1]->to_base, 0x80001100);
    CHECK_EQ_I64(r[1]->to_addr, 0x80001104);
    REQUIRE(af->sections.n >= 2);
    CHECK_SV(af->sections.v[0].name, ".text");
    CHECK_EQ_I64(af->sections.v[0].start, 0x80001100);
    CHECK_EQ_I64(af->sections.v[0].end, 0x80001108);
    CHECK_SV(af->sections.v[1].name, ".sdata");
    shsm_asm_free(af);
}

AVP_TEST(operand_forms) {
    AvpArena *a = avp_arena();
    struct {
        const char *mn, *ops, *sym, *reloc;
        i64 off, label;
    } cases[] = {
        {"lwz", "r3, dup@sda21(r13)", "dup", "sda21", 0, SHSM_NONE},
        {"addi", "r3, r3, x+0x10@l", "x", "l", 16, SHSM_NONE},
        {"lwz", "r3, \"a b\"-8@sda2(r2)", "a b", "sda2", -8, SHSM_NONE},
        {"b", "foo+0x8", "foo", "rel24", 8, SHSM_NONE},
        {"bne", "cr1, .L_80001234", NULL, NULL, 0, 0x80001234},
        {"bctr", "", NULL, NULL, 0, SHSM_NONE},
        {"blr", "lr", NULL, NULL, 0, SHSM_NONE},
        {"bdnz", "-0x10", NULL, NULL, 0, SHSM_NONE},
        {"lis", "r3, 0x10@hi", "x10", "h", 0, SHSM_NONE}, /* the regex's own reading: 'x10' then @h */
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
        ShsmAsmRow r = {0, SHSM_NONE, AVP_S(""), avp_str(cases[i].mn), avp_str(cases[i].ops), {NULL, 0}, 0, NULL,
                        SHSM_NONE};
        shsm_asm_parse_operands(a, &r);
        if (cases[i].sym) CHECK_SV(r.sym, cases[i].sym);
        else CHECK_MSG(r.sym.p == NULL, "case %zu: sym %.*s", i, AVP_SF(r.sym));
        if (cases[i].reloc) CHECK_CZ(r.reloc, cases[i].reloc);
        else CHECK(r.reloc == NULL);
        CHECK_EQ_I64(r.sym_off, cases[i].off);
        CHECK_EQ_I64(r.label, cases[i].label);
    }
}

AVP_TEST(text_of_string_bytes) {
    AvpArena *a = avp_arena();
    AvpStr t = shsm_asm_bytes_to_text(a, AVP_S("\x82\xa0")); /* cp932 hiragana a */
    CHECK_EQ_U64(t.n, 3);
    CHECK_MEM(t.p, "\xe3\x81\x82", 3);
    t = shsm_asm_bytes_to_text(a, AVP_S("\xff\xfe")); /* Python's cp932: 0xFF -> U+F8F3, 0xFE -> U+F8F2 */
    CHECK_EQ_U64(t.n, 6);
    CHECK_MEM(t.p, "\xef\xa3\xb3\xef\xa3\xb2", 6);
    t = shsm_asm_bytes_to_text(a, AVP_S("\x81\x20")); /* a lead byte without a trail byte: Latin-1 */
    CHECK_EQ_U64(t.n, 3);
    CHECK_MEM(t.p, "\xc2\x81\x20", 3);
}

AVP_TEST(listing_rel) {
    AvpArena *a = avp_arena();
    CHECK_CZ(shsm_asm_listing_rel(a, "game/PadRumble.cpp"), "game/PadRumble.s");
    CHECK_CZ(shsm_asm_listing_rel(a, "auto_03_80006C7C_text"), "auto_03_80006C7C_text.s");
    CHECK_CZ(shsm_asm_listing_rel(a, "Runtime.PPCEABI.H\\__init_cpp_exceptions.cpp"),
              "Runtime.PPCEABI.H/__init_cpp_exceptions.s");
}

static const char *STALE_S = "# 0x80001010..0x80001018 | size: 0x8\n.text\n.balign 4\n\n"
                             "# .text:0x0 | 0x80001010 | size: 0x8\n.fn helper, global\n"
                             "/* 80001010 00000000  4E 80 00 20 */\tblr\n"
                             "/* 80001014 00000004  4E 80 00 20 */\tblr\n.endfn helper\n";

AVP_TEST(listings_and_split_record) {
    const char *root = fx_dir("asm listings");
    const char *asm_dir = avp_join(avp_arena(), root, "build", "V", "asm");
    fx_write(asm_dir, "game/Alpha.s", ALPHA_S);
    fx_write(asm_dir, "game/Beta.s", BETA_S);
    fx_write(asm_dir, "auto_03_80001010_text.s", STALE_S);
    AvpArena *a = avp_arena();
    ShsmListings *l = shsm_asm_listings(a, asm_dir);
    CHECK_EQ_U64(l->rels.n, 3);
    CHECK(l->record == NULL);
    CHECK(l->note && strstr(l->note, "no usable split record"));
    const char *dir = avp_join(a, root, "build", "V");
    fx_write(dir, "config.json",
             "{\"version\": \"1.8.4\", \"units\": [{\"object\": \"o\", \"name\": \"game/Alpha.cpp\"}, "
             "{\"object\": \"o\", \"name\": \"game/Beta.c\"}], \"modules\": []}");
    l = shsm_asm_listings(a, asm_dir);
    REQUIRE(l->rels.n == 2);
    CHECK_SV(l->rels.v[0], "game/Alpha.s");
    CHECK_SV(l->rels.v[1], "game/Beta.s");
    REQUIRE(l->stale.n == 1);
    CHECK_SV(l->stale.v[0], "auto_03_80001010_text.s");
    CHECK(l->record && strstr(l->record, "config.json"));
    CHECK(l->note && strstr(l->note, "1 stale asm listing from an earlier split"));
    CHECK(l->note && strstr(l->note, "auto_03_80001010_text.s"));
    fx_write(dir, "config.json", "{\"units\": [], \"modules\": [{\"name\": \"rel\"}]}");
    CHECK(shsm_asm_listings(a, asm_dir)->record == NULL);
}

AVP_TEST_MAIN("shsm_asm")
