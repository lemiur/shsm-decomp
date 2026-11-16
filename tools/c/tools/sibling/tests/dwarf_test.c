/* sibling: the DWARF 2 reader on hand-built blobs (abbrev, info, loc, line, and a small ELF around them). */
#include "avp/test.h"
#include "sibling_dwarf.h"

#include <string.h>

typedef struct {
    u8 b[4096];
    size_t n;
} Blob;

static void p8(Blob *x, u32 v) { x->b[x->n++] = (u8)v; }
static void p16(Blob *x, u32 v) { p8(x, v >> 8); p8(x, v); }
static void p32(Blob *x, u32 v) { p16(x, v >> 16); p16(x, v); }
static void puleb(Blob *x, u64 v) {
    do {
        u8 c = (u8)(v & 0x7F);
        v >>= 7;
        p8(x, v ? c | 0x80 : c);
    } while (v);
}
static void psleb(Blob *x, i64 v) {
    for (;;) {
        u8 c = (u8)(v & 0x7F);
        v >>= 7;
        bool done = (v == 0 && !(c & 0x40)) || (v == -1 && (c & 0x40));
        p8(x, done ? c : c | 0x80);
        if (done) return;
    }
}
static void pstr(Blob *x, const char *s) {
    size_t n = strlen(s) + 1;
    memcpy(x->b + x->n, s, n);
    x->n += n;
}
static size_t slot32(Blob *x) { size_t at = x->n; p32(x, 0); return at; }
static void patch32(Blob *x, size_t at, u32 v) {
    x->b[at] = (u8)(v >> 24); x->b[at + 1] = (u8)(v >> 16); x->b[at + 2] = (u8)(v >> 8); x->b[at + 3] = (u8)v;
}

/* abbreviation: code, tag, children, then (attribute, form) pairs ending in 0, 0 */
static void abbrev(Blob *x, u32 code, u32 tag, bool children, const u32 *pairs) {
    puleb(x, code);
    puleb(x, tag);
    p8(x, children);
    for (; pairs[0] || pairs[1]; pairs += 2) { puleb(x, pairs[0]); puleb(x, pairs[1]); }
    p8(x, 0);
    p8(x, 0);
}

enum { A_CU = 1, A_BASE, A_STRUCT, A_MEMBER, A_BITS, A_INHERIT, A_SUB, A_PARAM, A_BLOCK, A_VAR, A_PTR, A_ODD, A_ARRAY,
       A_RANGE, A_BADCU };

static Blob ab, info, loc, line;
static u32 OFF_INT, OFF_BASE, OFF_DERIVED, OFF_PTR, OFF_ARR, OFF_SUB, OFF_CU2, OFF_CU3;

static void build(void) {
    memset(&ab, 0, sizeof ab), memset(&info, 0, sizeof info), memset(&loc, 0, sizeof loc), memset(&line, 0, sizeof line);
    abbrev(&ab, A_CU, SD_TAG_compile_unit, true,
           (const u32[]){SD_AT_name, SD_FORM_string, SD_AT_low_pc, SD_FORM_addr, SD_AT_high_pc, SD_FORM_addr,
                         SD_AT_stmt_list, SD_FORM_data4, SD_AT_producer, SD_FORM_string, 0, 0});
    abbrev(&ab, A_BASE, SD_TAG_base_type, false,
           (const u32[]){SD_AT_name, SD_FORM_string, SD_AT_byte_size, SD_FORM_data4, SD_AT_encoding, SD_FORM_data1, 0, 0});
    abbrev(&ab, A_STRUCT, SD_TAG_structure_type, true,
           (const u32[]){SD_AT_name, SD_FORM_string, SD_AT_byte_size, SD_FORM_data4, 0, 0});
    abbrev(&ab, A_MEMBER, SD_TAG_member, false,
           (const u32[]){SD_AT_name, SD_FORM_string, SD_AT_type, SD_FORM_ref_addr, SD_AT_data_member_location, SD_FORM_block2, 0, 0});
    abbrev(&ab, A_BITS, SD_TAG_member, false,
           (const u32[]){SD_AT_name, SD_FORM_string, SD_AT_type, SD_FORM_ref_addr, SD_AT_data_member_location, SD_FORM_block2,
                         SD_AT_byte_size, SD_FORM_data4, SD_AT_bit_size, SD_FORM_udata, SD_AT_bit_offset, SD_FORM_udata, 0, 0});
    abbrev(&ab, A_INHERIT, SD_TAG_inheritance, false,
           (const u32[]){SD_AT_type, SD_FORM_ref_addr, SD_AT_data_member_location, SD_FORM_block2, SD_AT_accessibility,
                         SD_FORM_data1, SD_AT_virtuality, SD_FORM_data1, 0, 0});
    abbrev(&ab, A_SUB, SD_TAG_subprogram, true,
           (const u32[]){SD_AT_name, SD_FORM_string, SD_AT_low_pc, SD_FORM_addr, SD_AT_high_pc, SD_FORM_addr, SD_AT_type,
                         SD_FORM_ref_addr, SD_AT_external, SD_FORM_flag, SD_AT_frame_base, SD_FORM_data4, SD_AT_decl_line,
                         SD_FORM_data2, SD_AT_decl_file, SD_FORM_data2, SD_AT_sibling, SD_FORM_ref_addr, 0, 0});
    abbrev(&ab, A_PARAM, SD_TAG_formal_parameter, false,
           (const u32[]){SD_AT_name, SD_FORM_string, SD_AT_type, SD_FORM_ref_addr, SD_AT_location, SD_FORM_data4, 0, 0});
    abbrev(&ab, A_BLOCK, SD_TAG_lexical_block, true,
           (const u32[]){SD_AT_low_pc, SD_FORM_ref_addr, SD_AT_high_pc, SD_FORM_ref_addr, 0, 0});
    abbrev(&ab, A_VAR, SD_TAG_variable, false,
           (const u32[]){SD_AT_name, SD_FORM_string, SD_AT_type, SD_FORM_ref_addr, SD_AT_location, SD_FORM_block1, 0, 0});
    abbrev(&ab, A_PTR, SD_TAG_pointer_type, false, (const u32[]){SD_AT_type, SD_FORM_ref_addr, 0, 0});
    abbrev(&ab, A_ODD, 0x4109, false, (const u32[]){0x2001, SD_FORM_data1, SD_AT_name, SD_FORM_string, 0, 0});
    abbrev(&ab, A_ARRAY, SD_TAG_array_type, true, (const u32[]){SD_AT_type, SD_FORM_ref_addr, 0, 0});
    abbrev(&ab, A_RANGE, SD_TAG_subrange_type, false, (const u32[]){SD_AT_upper_bound, SD_FORM_data2, 0, 0});
    abbrev(&ab, A_BADCU, SD_TAG_compile_unit, true, (const u32[]){SD_AT_name, SD_FORM_string, 0x2002, 0x30, 0, 0});
    p8(&ab, 0);

    /* .debug_loc: frame base r1; this r3 then r31; n in f1 everywhere */
    p32(&loc, 0x0), p32(&loc, 0x40), p16(&loc, 1), p8(&loc, 0x51), p32(&loc, 0), p32(&loc, 0); /* 0: 19 bytes */
    p32(&loc, 0x0), p32(&loc, 0x10), p16(&loc, 1), p8(&loc, 0x53);                             /* 19 */
    p32(&loc, 0x10), p32(&loc, 0x40), p16(&loc, 1), p8(&loc, 0x6f), p32(&loc, 0), p32(&loc, 0);
    p32(&loc, 0x0), p32(&loc, 0xFFFFFFFF), p16(&loc, 2), p8(&loc, 0x90), p8(&loc, 33), p32(&loc, 0), p32(&loc, 0); /* 49 */

    /* unit 1 */
    size_t len = slot32(&info);
    p16(&info, 2), p32(&info, 0), p8(&info, 4);
    puleb(&info, A_CU), pstr(&info, "test.cpp"), p32(&info, 0x80001000), p32(&info, 0x80001100), p32(&info, 0),
        pstr(&info, "MW EABI PPC C-Compiler");
    OFF_INT = (u32)info.n;
    puleb(&info, A_BASE), pstr(&info, "int"), p32(&info, 4), p8(&info, 5);
    OFF_BASE = (u32)info.n;
    puleb(&info, A_STRUCT), pstr(&info, "Base"), p32(&info, 4);
    puleb(&info, A_MEMBER), pstr(&info, "b"), p32(&info, OFF_INT), p16(&info, 2), p8(&info, 0x23), p8(&info, 0);
    p8(&info, 0);
    OFF_DERIVED = (u32)info.n;
    puleb(&info, A_STRUCT), pstr(&info, "Derived"), p32(&info, 0x10);
    puleb(&info, A_INHERIT), p32(&info, OFF_BASE), p16(&info, 2), p8(&info, 0x23), p8(&info, 0), p8(&info, 1), p8(&info, 0);
    puleb(&info, A_MEMBER), pstr(&info, "x"), p32(&info, OFF_INT), p16(&info, 2), p8(&info, 0x23), p8(&info, 8);
    puleb(&info, A_BITS), pstr(&info, "lo"), p32(&info, OFF_INT), p16(&info, 2), p8(&info, 0x23), p8(&info, 0xC), p32(&info, 4),
        puleb(&info, 3), puleb(&info, 0);
    puleb(&info, A_BITS), pstr(&info, "hi"), p32(&info, OFF_INT), p16(&info, 2), p8(&info, 0x23), p8(&info, 0xC), p32(&info, 4),
        puleb(&info, 5), puleb(&info, 3);
    p8(&info, 0);
    OFF_PTR = (u32)info.n;
    puleb(&info, A_PTR), p32(&info, OFF_DERIVED);
    OFF_ARR = (u32)info.n;
    puleb(&info, A_ARRAY), p32(&info, OFF_INT);
    puleb(&info, A_RANGE), p16(&info, 3);
    p8(&info, 0);
    puleb(&info, A_ODD), p8(&info, 7), pstr(&info, "odd");
    OFF_SUB = (u32)info.n;
    puleb(&info, A_SUB), pstr(&info, "Derived::Get(int)"), p32(&info, 0x80001000), p32(&info, 0x80001040), p32(&info, OFF_INT),
        p8(&info, 1), p32(&info, 0), p16(&info, 10), p16(&info, 1), p32(&info, 0x7FFF); /* a sibling pointing nowhere */
    puleb(&info, A_PARAM), pstr(&info, "this"), p32(&info, OFF_PTR), p32(&info, 19);
    puleb(&info, A_PARAM), pstr(&info, "n"), p32(&info, OFF_INT), p32(&info, 49);
    puleb(&info, A_BLOCK), p32(&info, 0x80001010), p32(&info, 0x80001030);
    puleb(&info, A_VAR), pstr(&info, "tmp"), p32(&info, OFF_ARR), p8(&info, 2), p8(&info, 0x91), psleb(&info, -8);
    p8(&info, 0);
    p8(&info, 0);
    p8(&info, 0);
    patch32(&info, len, (u32)(info.n - len - 4));
    /* unit 2: a form the reader cannot size */
    OFF_CU2 = (u32)info.n;
    len = slot32(&info);
    p16(&info, 2), p32(&info, 0), p8(&info, 4);
    puleb(&info, A_BADCU), pstr(&info, "bad.c"), p8(&info, 0x55), p8(&info, 0x55);
    p8(&info, 0);
    patch32(&info, len, (u32)(info.n - len - 4));
    /* unit 3: read after the bad one */
    OFF_CU3 = (u32)info.n;
    len = slot32(&info);
    p16(&info, 2), p32(&info, 0), p8(&info, 4);
    puleb(&info, A_CU), pstr(&info, "third.c"), p32(&info, 0x80002000), p32(&info, 0x80002010), p32(&info, 0), pstr(&info, "MW");
    puleb(&info, A_BASE), pstr(&info, "char"), p32(&info, 1), p8(&info, 8);
    p8(&info, 0);
    patch32(&info, len, (u32)(info.n - len - 4));

    /* .debug_line */
    len = slot32(&line);
    p16(&line, 2);
    size_t hl = slot32(&line);
    size_t hstart = line.n;
    p8(&line, 4), p8(&line, 1), p8(&line, (u8)-5), p8(&line, 14), p8(&line, 10);
    const u8 std[9] = {0, 1, 1, 1, 1, 0, 0, 0, 1};
    for (int k = 0; k < 9; k++) p8(&line, std[k]);
    pstr(&line, "C:\\src\\"), p8(&line, 0);
    pstr(&line, "test.cpp"), puleb(&line, 1), puleb(&line, 0), puleb(&line, 0);
    pstr(&line, "inl.h"), puleb(&line, 0), puleb(&line, 0), puleb(&line, 0);
    p8(&line, 0);
    patch32(&line, hl, (u32)(line.n - hstart));
    p8(&line, 0), puleb(&line, 5), p8(&line, 2), p32(&line, 0x80001000); /* set_address */
    p8(&line, 3), psleb(&line, 9);                                       /* advance_line -> 10 */
    p8(&line, 1);                                                        /* copy */
    p8(&line, 30);                                                       /* special: +4, +1 */
    p8(&line, 4), puleb(&line, 2);                                       /* set_file 2 */
    p8(&line, 2), puleb(&line, 2);                                       /* advance_pc 2*4 */
    p8(&line, 8);                                                        /* const_add_pc 17*4 */
    p8(&line, 1);
    p8(&line, 9), p16(&line, 0x10);                                      /* fixed_advance_pc */
    p8(&line, 3), psleb(&line, -3);
    p8(&line, 1);
    p8(&line, 0), puleb(&line, 1), p8(&line, 1);                         /* end_sequence */
    patch32(&line, len, (u32)(line.n - len - 4));
}

static SdInfo *parsed(AvpArena *a) {
    build();
    SdSections s = {{ab.b, ab.n}, {info.b, info.n}, {line.b, line.n}, {loc.b, loc.n}};
    return sd_parse(a, &s);
}

AVP_TEST(dwarf_units_and_notes) {
    AvpArena *a = avp_arena();
    SdInfo *d = parsed(a);
    REQUIRE(d->ncus == 3);
    CHECK_CZ(d->cus[0].name, "test.cpp");
    CHECK_CZ(d->cus[0].producer, "MW EABI PPC C-Compiler");
    CHECK(d->cus[0].has_pc && d->cus[0].low == 0x80001000 && d->cus[0].has_stmt);
    CHECK_CZ(d->cus[2].name, "third.c"); /* the unit after the unreadable one is read */
    const SdDie *c = sd_die_at(d, OFF_CU3 + 11 + 1 + 8 + 4 + 4 + 4 + 3);
    REQUIRE(c);
    CHECK_CZ(c->name, "char");
    bool form_note = false, tag_note = false;
    for (size_t k = 0; k < d->notes.n; k++) {
        form_note |= avp_str_contains(d->notes.v[k], AVP_S("form 0x30"));
        tag_note |= avp_str_contains(d->notes.v[k], AVP_S("unknown tag 0x4109"));
    }
    CHECK(form_note);
    CHECK(tag_note);
    const SdDie *odd = NULL;
    for (size_t k = 0; k < d->ndies; k++)
        if (d->dies[k].tag == 0x4109) odd = &d->dies[k];
    REQUIRE(odd);
    CHECK_CZ(odd->name, "odd"); /* the unknown attribute before it was sized and skipped */
}

AVP_TEST(dwarf_struct_base_bitfield) {
    AvpArena *a = avp_arena();
    SdInfo *d = parsed(a);
    const SdDie *x = sd_die_at(d, OFF_DERIVED);
    REQUIRE(x);
    CHECK(x->tag == SD_TAG_structure_type && x->byte_size == 0x10);
    CHECK(sd_type_size(d, OFF_DERIVED) == 0x10);
    u32 i = sd_index(d, x), nkids = 0;
    const SdDie *inh = NULL, *hi = NULL;
    for (u32 k = i + 1; k < x->next; k++) {
        if (d->dies[k].parent != i) continue;
        nkids++;
        if (d->dies[k].tag == SD_TAG_inheritance) inh = &d->dies[k];
        if (d->dies[k].name && strcmp(d->dies[k].name, "hi") == 0) hi = &d->dies[k];
    }
    CHECK(nkids == 4);
    REQUIRE(inh && hi);
    CHECK(inh->type == OFF_BASE && inh->member_loc == 0 && inh->accessibility == 1);
    CHECK(hi->member_loc == 0xC && hi->bit_size == 5 && hi->bit_offset == 3 && hi->byte_size == 4);
    CHECK_CZ(sd_decl(a, d, OFF_PTR, "p"), "Derived *p");
    CHECK_CZ(sd_decl(a, d, OFF_ARR, "tmp"), "int tmp[4]");
    CHECK(sd_type_size(d, OFF_ARR) == 16);
    CHECK_CZ(sd_decl(a, d, 0, ""), "void");
}

AVP_TEST(dwarf_subprogram_locals) {
    AvpArena *a = avp_arena();
    SdInfo *d = parsed(a);
    const SdDie *f = sd_die_at(d, OFF_SUB);
    REQUIRE(f);
    CHECK(f->low == 0x80001000 && f->high == 0x80001040 && f->decl_line == 10 && (f->flags & SD_F_EXTERNAL));
    /* the bogus DW_AT_sibling is ignored: the subtree ends where the children end */
    const SdDie *after = f->next < d->ndies ? &d->dies[f->next] : NULL;
    CHECK(after == NULL || after->depth <= f->depth);
    const SdDie *this_ = NULL, *n = NULL, *blk = NULL, *tmp = NULL;
    for (u32 k = sd_index(d, f) + 1; k < f->next; k++) {
        const SdDie *x = &d->dies[k];
        if (x->name && strcmp(x->name, "this") == 0) this_ = x;
        if (x->name && strcmp(x->name, "n") == 0) n = x;
        if (x->tag == SD_TAG_lexical_block) blk = x;
        if (x->name && strcmp(x->name, "tmp") == 0) tmp = x;
    }
    REQUIRE(this_ && n && blk && tmp);
    CHECK(blk->low == 0x80001010 && blk->high == 0x80001030 && tmp->parent == sd_index(d, blk));
    CHECK_CZ(sd_location_text(a, d, this_, NULL, NULL), "r3 [80001000-80001010), r31 [80001010-80001040)");
    CHECK_CZ(sd_location_text(a, d, n, NULL, NULL), "f1");
    CHECK_CZ(sd_location_text(a, d, tmp, NULL, NULL), "fb-0x8");
    SdLocVec fb = sd_loclist(a, d, f->frame_base, 0x80001000);
    REQUIRE(fb.n == 1);
    CHECK_CZ(fb.v[0].expr, "r1");
    CHECK(fb.v[0].begin == 0x80001000 && fb.v[0].end == 0x80001040);
    avp_vec_free(&fb);
}

AVP_TEST(dwarf_param_and_local_names) {
    AvpArena *a = avp_arena();
    SdInfo *d = parsed(a);
    const SdDie *f = sd_die_at(d, OFF_SUB);
    REQUIRE(f);
    CHECK(sd_param_count(d, f) == 2); /* this, n */
    AvpStrVec locals = sd_local_names(a, d, f);
    REQUIRE(locals.n >= 1);
    bool has_tmp = false;
    for (size_t k = 0; k < locals.n; k++) has_tmp |= avp_str_eqz(locals.v[k], "tmp");
    CHECK(has_tmp); /* a local of a nested block is listed */
    /* the Debug build records more: a multiset difference, one name per missing occurrence */
    AvpStrVec debug = {0}, none = {0};
    for (size_t k = 0; k < locals.n; k++) avp_vec_push(&debug, locals.v[k]);
    avp_vec_push(&debug, avp_str("pool"));
    avp_vec_push(&debug, avp_str("_result"));
    avp_vec_push(&debug, avp_str("_result"));
    AvpStrVec have = {0};
    for (size_t k = 0; k < locals.n; k++) avp_vec_push(&have, locals.v[k]);
    avp_vec_push(&have, avp_str("_result"));
    AvpStrVec lost = sd_names_missing(&have, &debug);
    REQUIRE(lost.n == 2);
    CHECK(avp_str_eqz(lost.v[0], "pool") && avp_str_eqz(lost.v[1], "_result"));
    AvpStrVec all = sd_names_missing(&none, &have);
    CHECK(all.n == have.n);
    CHECK(sd_names_missing(&debug, &have).n == 0);
    avp_vec_free(&locals);
    avp_vec_free(&debug);
    avp_vec_free(&have);
    avp_vec_free(&lost);
    avp_vec_free(&all);
}

AVP_TEST(dwarf_line_program) {
    AvpArena *a = avp_arena();
    SdInfo *d = parsed(a);
    SdLines l = sd_lines(a, d, 0);
    REQUIRE(l.ok);
    CHECK(l.min_inst == 4);
    CHECK_CZ(sd_file_name(&l, 1), "C:\\src\\test.cpp");
    CHECK_CZ(sd_file_name(&l, 2), "inl.h");
    CHECK_CZ(sd_file_name(&l, 9), "?");
    REQUIRE(l.rows.n == 5);
    CHECK(l.rows.v[0].address == 0x80001000 && l.rows.v[0].line == 10 && l.rows.v[0].file == 1);
    CHECK(l.rows.v[1].address == 0x80001004 && l.rows.v[1].line == 11);
    CHECK(l.rows.v[2].address == 0x80001050 && l.rows.v[2].line == 11 && l.rows.v[2].file == 2);
    CHECK(l.rows.v[3].address == 0x80001060 && l.rows.v[3].line == 8);
    CHECK(l.rows.v[4].end_sequence);
    SdLines bad = sd_lines(a, d, 0x9999);
    CHECK(!bad.ok && bad.error);
}

AVP_TEST(dwarf_expressions) {
    AvpArena *a = avp_arena();
    const u8 addr[] = {0x03, 0x80, 0x74, 0xF1, 0xC0};
    CHECK_CZ(sd_expr_text(a, addr, sizeof addr), "@0x8074F1C0");
    const u8 breg[] = {0x71, 0x10};
    CHECK_CZ(sd_expr_text(a, breg, sizeof breg), "[r1+0x10]");
    const u8 unknown[] = {0xE0, 0x01};
    CHECK_CZ(sd_expr_text(a, unknown, sizeof unknown), "op0xe0?");
    const u8 cut[] = {0x03, 0x80};
    CHECK(avp_str_contains(avp_str(sd_expr_text(a, cut, sizeof cut)), AVP_S("truncated")));
}

AVP_TEST(dwarf_elf_sections) {
    build();
    static u8 elf[8192];
    memset(elf, 0, sizeof elf);
    const char shstr[] = "\0.shstrtab\0.debug_abbrev\0.debug_info\0";
    size_t pos = 52;
    size_t str_at = pos;
    memcpy(elf + pos, shstr, sizeof shstr);
    pos += sizeof shstr;
    size_t ab_at = pos;
    memcpy(elf + pos, ab.b, ab.n);
    pos += ab.n;
    size_t in_at = pos;
    memcpy(elf + pos, info.b, info.n);
    pos += info.n;
    pos = (pos + 3) & ~(size_t)3;
    size_t sh = pos;
    memcpy(elf, "\x7f" "ELF\x01\x02\x01", 7);
    u32 fields[][3] = {{0, 0, 0}, {1, (u32)str_at, sizeof shstr}, {11, (u32)ab_at, (u32)ab.n}, {25, (u32)in_at, (u32)info.n}};
    for (int k = 0; k < 4; k++) {
        u8 *e = elf + sh + 40 * (size_t)k;
        Blob t = {0};
        p32(&t, fields[k][0]), p32(&t, 1), p32(&t, 0), p32(&t, 0), p32(&t, fields[k][1]), p32(&t, fields[k][2]);
        memcpy(e, t.b, t.n);
    }
    Blob hd = {0};
    p32(&hd, (u32)sh);
    memcpy(elf + 32, hd.b, 4);
    elf[46] = 0, elf[47] = 40, elf[48] = 0, elf[49] = 4, elf[50] = 0, elf[51] = 1;
    SdSections s;
    REQUIRE(sd_elf_sections(avp_strn((const char *)elf, sh + 160), &s));
    CHECK(s.info.n == info.n && s.abbrev.n == ab.n && s.line.n == 0);
    CHECK(!sd_elf_sections(AVP_S("not an elf, not an elf, not an elf, not an elf, not an elf"), &s));
}

AVP_TEST_MAIN("sibling_dwarf")
