/* pe_disasm: the pieces of the port on a small synthetic PE (one executable section with two functions, a call, a
 * string immediate and padding). The tool as a whole is checked by the differential harness (diff/cases.txt) over
 * every compiler under tools/compilers. There is no Python test of pe_disasm.py to port. */
#include <string.h>

#include "avp/test.h"
#include "pe_disasm.h"

static void put32(u8 *p, u32 v) {
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
    p[2] = (u8)(v >> 16);
    p[3] = (u8)(v >> 24);
}

/* image base 0x400000, .text at 0x401000 (file 0x200, 0x40 bytes):
 *   401000 push ebp; mov ebp,esp; call 401010; pop ebp; ret; int3 x6
 *   401010 push 0x401030; call 401010; add esp,4; ret; int3 x18
 *   401030 "hello world\0" */
static const u8 *image(size_t *n) {
    static u8 d[0x240];
    memset(d, 0, sizeof d);
    d[0] = 'M';
    d[1] = 'Z';
    put32(d + 0x3C, 0x40);
    memcpy(d + 0x40, "PE\0\0", 4);
    d[0x44] = 0x4c;
    d[0x45] = 0x01;
    d[0x46] = 1;    /* one section */
    d[0x54] = 0xE0; /* optional header size */
    put32(d + 0x58 + 28, 0x400000);
    u8 *s = d + 0x58 + 0xE0;
    memcpy(s, ".text", 5);
    put32(s + 8, 0x40);
    put32(s + 12, 0x1000);
    put32(s + 16, 0x40);
    put32(s + 20, 0x200);
    put32(s + 36, 0x60000020);
    static const u8 code[] = {0x55, 0x8b, 0xec, 0xe8, 0x08, 0, 0, 0, 0x5d, 0xc3, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
                              0x68, 0x30, 0x10, 0x40, 0x00, 0xe8, 0xf6, 0xff, 0xff, 0xff, 0x83, 0xc4, 0x04, 0xc3, 0xcc, 0xcc};
    memcpy(d + 0x200, code, sizeof code);
    memset(d + 0x220, 0xcc, 0x10);
    memcpy(d + 0x230, "hello world", 11);
    *n = sizeof d;
    return d;
}

AVP_TEST(sections_and_reads) {
    size_t n;
    const u8 *d = image(&n);
    PeDisPe *pe = pe_disasm_parse(avp_arena(), d, n);
    CHECK_EQ_U64(pe->nsecs, 1);
    CHECK_CZ(pe->secs[0].name, ".text");
    CHECK_EQ_I64(pe->lo, 0x401000);
    CHECK_EQ_I64(pe->hi, 0x401040);
    u64 off;
    REQUIRE(pe_disasm_off(pe, 0x401030, &off));
    CHECK_EQ_U64(off, 0x230);
    CHECK(!pe_disasm_off(pe, 0x401040, &off));
    CHECK_STR(pe_disasm_cstr(pe, 0x401030), "hello world");
    CHECK(pe_disasm_cstr(pe, 0x401000).p == NULL); /* not printable */
    CHECK(pe_disasm_cstr(pe, 0x401038).p == NULL); /* "rld": shorter than 4 */
}

AVP_TEST(index_calls_refs_starts) {
    size_t n;
    const u8 *d = image(&n);
    AvpArena *a = avp_arena();
    PeDisPe *pe = pe_disasm_parse(a, d, n);
    PeDisIndex *idx = pe_disasm_build_index(a, pe, 1.5);
    REQUIRE(idx->ncalls >= 2);
    CHECK_EQ_I64(idx->calls[0].a, 0x401003);
    CHECK_EQ_I64(idx->calls[0].v, 0x401010);
    CHECK_EQ_I64(idx->calls[1].a, 0x401015);
    CHECK_EQ_I64(idx->calls[1].v, 0x401010);
    bool ref = false;
    for (size_t i = 0; i < idx->nrefs; i++) ref |= idx->refs[i].a == 0x401010 && idx->refs[i].v == 0x401030;
    CHECK(ref);
    REQUIRE(idx->nstarts >= 2);
    CHECK_EQ_I64(idx->starts[0], 0x401000);
    CHECK_EQ_I64(idx->starts[1], 0x401010);
    i64 f;
    REQUIRE(pe_disasm_func_of(idx, 0x401015, &f));
    CHECK_EQ_I64(f, 0x401010);
    CHECK(!pe_disasm_func_of(idx, 0x400fff, &f));
}

AVP_TEST(func_extent_stops_at_the_last_ret) {
    size_t n;
    const u8 *d = image(&n);
    PeDisPe *pe = pe_disasm_parse(avp_arena(), d, n);
    csh h = pe_disasm_md();
    PeDisInsns v = pe_disasm_func_extent(h, pe, 0x401000, 4000);
    REQUIRE(v.n == 5);
    CHECK_CZ(v.v[0]->mnemonic, "push");
    CHECK_CZ(v.v[1]->op_str, "ebp, esp");
    CHECK_CZ(v.v[4]->mnemonic, "ret");
    i64 vals[8];
    CHECK_EQ_U64(pe_disasm_operand_values(v.v[2], vals), 0); /* a call's target is not an operand value */
    pe_disasm_free_extent(&v);
    v = pe_disasm_func_extent(h, pe, 0x401000, 2);
    CHECK_EQ_U64(v.n, 2);
    pe_disasm_free_extent(&v);
    v = pe_disasm_func_extent(h, pe, 0x401010, 4000);
    REQUIRE(v.n == 4);
    CHECK_EQ_U64(pe_disasm_operand_values(v.v[0], vals), 1);
    CHECK_EQ_I64(vals[0], 0x401030);
    pe_disasm_free_extent(&v);
    cs_close(&h);
}

AVP_TEST(strings_and_names) {
    size_t n;
    const u8 *d = image(&n);
    AvpArena *a = avp_arena();
    PeDisPe *pe = pe_disasm_parse(a, d, n);
    AvpBuf out;
    avp_buf_init(&out, a);
    pe_disasm_cmd_strings(a, pe, AVP_S("w.r"), &out);
    CHECK_STR(avp_buf_str(&out), "0x00401030  hello world\n");
    CHECK_STR(pe_disasm_fname(a, NULL, 0x401010), "func_401010");
    CHECK_STR(pe_disasm_fname(a, NULL, -1), "func_-1");
}

AVP_TEST(no_sections_is_an_error) {
    size_t n;
    const u8 *d = image(&n);
    u8 copy[0x240];
    memcpy(copy, d, n);
    copy[0x46] = 0;
    AvpTry t;
    if (AVP_TRY(&t)) {
        pe_disasm_parse(avp_arena(), copy, n);
        avp_try_end(&t);
        CHECK_MSG(false, "no exception");
    } else {
        CHECK_CZ(t.msg, "ValueError: min() iterable argument is empty");
    }
}

AVP_TEST_MAIN("pe_disasm")
