/* Unit tests of tools/flagsweep.py's logic (ported from tools/test_flagsweep.py; 32 tests).
 *
 * Not ported as a unit test:
 *   - FlagTests.test_glue_values: glue_values is part of flagsweep_main (a block inside it, not a function); the
 *     differential harness (diff/cases.txt) runs the tool with spaced "--vary VALUE" arguments.
 *   - RealCompileTest compiles through the static compile_variant/sweep of flagsweep_run.c. The three tests are ported
 *     to what the public API allows: the compiler is run here (avp_run), the object is read with cc_load_object, and
 *     fs_build_variants / fs_score_variant / fs_rank / fs_format_report are driven by hand as sweep() drives them. They
 *     are skipped (pass without checking) when tools/compilers/Wii/1.0 or tools/bin/dtk.exe is absent.
 * Difference: the Python scores a variant with one dict of norms; the C scores CcFn values (cc_fn_from_norms). */
#include <stdlib.h>
#include "avp/avp.h"
#include "avp/test.h"
#include "flagsweep.h"
#include "permute.h"

#define N(a) (sizeof(a) / sizeof((a)[0]))

/* ---- synthetic ELF objects (copied from tools/c/tools/codecmp/tests/codecmp_test.c) ------------------------------ */

typedef struct {
    const char *name;
    u32 type;
    const u8 *data;
    size_t n; /* for SHT_NOBITS the size only */
} TSec;
typedef struct {
    const char *name;
    u32 value, size;
    u8 bind, type;
    u16 shndx;
} TSym;
typedef struct {
    u32 sec, off, type, sym;
    i32 addend;
} TRel;

static void put32(AvpBuf *b, u32 v) {
    char c[4] = {(char)(v >> 24), (char)(v >> 16), (char)(v >> 8), (char)v};
    avp_buf_putn(b, c, 4);
}
static void put16(AvpBuf *b, u16 v) {
    char c[2] = {(char)(v >> 8), (char)v};
    avp_buf_putn(b, c, 2);
}

/* A 32-bit big-endian relocatable ELF: sections 1..nsec as given, then one .rela.<name> per section that has
 * relocations (in section order), .symtab, .strtab, .shstrtab. Symbol indices are 1..nsyms (0 is the null symbol). */
static AvpStr build_elf(const TSec *secs, size_t nsec, const TSym *syms, size_t nsyms, const TRel *rels, size_t nrels) {
    AvpArena *a = avp_arena();
    AvpBuf strtab = {0}, symtab = {0}, shstr = {0}, body = {0}, hdrs = {0};
    avp_buf_putc(&strtab, 0);
    for (int k = 0; k < 16; k++) avp_buf_putc(&symtab, 0);
    for (size_t k = 0; k < nsyms; k++) {
        put32(&symtab, (u32)strtab.n);
        avp_buf_putn(&strtab, syms[k].name, strlen(syms[k].name) + 1);
        put32(&symtab, syms[k].value);
        put32(&symtab, syms[k].size);
        avp_buf_putc(&symtab, (char)((syms[k].bind << 4) | syms[k].type));
        avp_buf_putc(&symtab, 0);
        put16(&symtab, syms[k].shndx);
    }
    /* section list: user sections, rela sections, symtab, strtab, shstrtab */
    size_t nrela = 0;
    bool has[32] = {0};
    for (size_t k = 0; k < nrels; k++) has[rels[k].sec] = true;
    for (size_t s = 1; s <= nsec; s++) nrela += has[s];
    u32 symtab_idx = (u32)(1 + nsec + nrela);
    AvpBuf relabuf[32];
    memset(relabuf, 0, sizeof relabuf);
    avp_buf_putc(&shstr, 0);
    u32 pos = 52;
    for (int k = 0; k < 40; k++) avp_buf_putc(&hdrs, 0); /* the null section header */
    typedef struct {
        u32 name, type, off, size, link, info, ent;
    } H;
    H h[64];
    size_t nh = 0;
    for (size_t s = 0; s < nsec; s++) {
        h[nh].name = (u32)shstr.n;
        avp_buf_putn(&shstr, secs[s].name, strlen(secs[s].name) + 1);
        h[nh].type = secs[s].type;
        h[nh].off = pos;
        h[nh].size = (u32)secs[s].n;
        h[nh].link = h[nh].info = h[nh].ent = 0;
        if (secs[s].type != 8) {
            avp_buf_putn(&body, secs[s].data, secs[s].n);
            pos += (u32)secs[s].n;
        }
        nh++;
    }
    for (size_t s = 1; s <= nsec; s++) {
        if (!has[s]) continue;
        for (size_t k = 0; k < nrels; k++) {
            if (rels[k].sec != s) continue;
            put32(&relabuf[s], rels[k].off);
            put32(&relabuf[s], (rels[k].sym << 8) | rels[k].type);
            put32(&relabuf[s], (u32)rels[k].addend);
        }
        char *nm = avp_sprintf(a, ".rela%s", secs[s - 1].name);
        h[nh].name = (u32)shstr.n;
        avp_buf_putn(&shstr, nm, strlen(nm) + 1);
        h[nh].type = 4;
        h[nh].off = pos;
        h[nh].size = (u32)relabuf[s].n;
        h[nh].link = symtab_idx;
        h[nh].info = (u32)s;
        h[nh].ent = 12;
        avp_buf_putn(&body, relabuf[s].p, relabuf[s].n);
        pos += (u32)relabuf[s].n;
        nh++;
    }
    const struct {
        const char *name;
        u32 type;
        AvpBuf *b;
        u32 link, info, ent;
    } tail[3] = {{".symtab", 2, &symtab, symtab_idx + 1, 1, 16},
                 {".strtab", 3, &strtab, 0, 0, 0},
                 {".shstrtab", 3, &shstr, 0, 0, 0}};
    for (int t = 0; t < 3; t++) {
        h[nh].name = (u32)shstr.n;
        avp_buf_putn(&shstr, tail[t].name, strlen(tail[t].name) + 1);
        h[nh].type = tail[t].type;
        h[nh].off = pos;
        h[nh].link = tail[t].link;
        h[nh].info = tail[t].info;
        h[nh].ent = tail[t].ent;
        h[nh].size = (u32)tail[t].b->n; /* .shstrtab: fixed below (its own name is the last thing added) */
        if (t < 2) {
            avp_buf_putn(&body, tail[t].b->p, tail[t].b->n);
            pos += (u32)tail[t].b->n;
        }
        nh++;
    }
    /* .shstrtab's bytes are complete now */
    h[nh - 1].off = pos;
    h[nh - 1].size = (u32)shstr.n;
    avp_buf_putn(&body, shstr.p, shstr.n);
    pos += (u32)shstr.n;
    for (size_t k = 0; k < nh; k++) {
        put32(&hdrs, h[k].name);
        put32(&hdrs, h[k].type);
        put32(&hdrs, 0);
        put32(&hdrs, 0);
        put32(&hdrs, h[k].off);
        put32(&hdrs, h[k].size);
        put32(&hdrs, h[k].link);
        put32(&hdrs, h[k].info);
        put32(&hdrs, 4);
        put32(&hdrs, h[k].ent);
    }
    AvpBuf f = {0};
    for (int k = 0; k < 52; k++) avp_buf_putc(&f, 0);
    avp_buf_putn(&f, body.p, body.n);
    u32 shoff = (u32)f.n;
    avp_buf_putn(&f, hdrs.p, hdrs.n);
    u8 *p = (u8 *)f.p;
    memcpy(p, "\x7f" "ELF\x01\x02\x01", 7);
    p[17] = 1;  /* ET_REL */
    p[19] = 20; /* EM_PPC */
    p[32] = (u8)(shoff >> 24), p[33] = (u8)(shoff >> 16), p[34] = (u8)(shoff >> 8), p[35] = (u8)shoff;
    p[47] = 40;                /* shentsize */
    p[49] = (u8)(nh + 1);      /* shnum (the null header included) */
    p[51] = (u8)nh;            /* shstrndx: the last section */
    return avp_strn(f.p, f.n);
}

/* ---- listings ---------------------------------------------------------------------------------------------------- */

typedef struct {
    const char *word; /* eight hex digits */
    const char *text;
} TI;

/* A dtk-style listing block of one function; the addresses start at base and advance by four. */
static AvpStr listing(const char *name, i64 base, const TI *ins, size_t n) {
    AvpBuf b = {0};
    avp_buf_printf(&b, ".fn %s, global\n", name);
    for (size_t k = 0; k < n; k++) {
        i64 addr = base + 4 * (i64)k;
        const char *w = ins[k].word;
        avp_buf_printf(&b, "/* %08X %08X  %c%c %c%c %c%c %c%c */\t%s\n", (u32)addr, (u32)(addr + 0x40), w[0], w[1],
                       w[2], w[3], w[4], w[5], w[6], w[7], ins[k].text);
    }
    avp_buf_printf(&b, ".endfn %s\n", name);
    return avp_strn(b.p, b.n);
}

static const CcFn *one(const AvpMap *fns, const char *name) {
    AvpVal *v = avp_map_get(fns, avp_str(name));
    AVP_ASSERT(v);
    return v->p;
}

static CcFn *retail_fn(const char *name, const TI *ins, size_t n) {
    AvpArena *a = avp_arena();
    AvpMap *m = cc_parse_listing(a, listing(name, 0x80001000, ins, n), NULL);
    return (CcFn *)one(m, name);
}

typedef struct {
    const TI *ins;
    size_t nins;
    const TSym *syms; /* after the function's own symbol, which is index 1 */
    size_t nsyms;
    const TRel *rels;
    size_t nrels;
    const u8 *data; /* the .data section (section 2); NULL: sixteen zero bytes */
    size_t ndata;
    const TSec *extra; /* sections 3... */
    size_t nextra;
} CSpec;

/* compiled_fn: a compiled function placed at .text offset 0, with .data as section 2. */
static CcFn *compiled_fn(const char *name, const CSpec *s) {
    AvpArena *a = avp_arena();
    static const u8 zero16[16] = {0};
    size_t n = 4 * s->nins;
    u8 *text = avp_alloc(a, n ? n : 1);
    memset(text, 0, n);
    TSec secs[8];
    AVP_ASSERT(s->nextra + 2 <= N(secs));
    secs[0] = (TSec){".text", 1, text, n};
    secs[1] = (TSec){".data", 1, s->data ? s->data : zero16, s->data ? s->ndata : 16};
    for (size_t k = 0; k < s->nextra; k++) secs[2 + k] = s->extra[k];
    TSym syms[16];
    AVP_ASSERT(s->nsyms + 1 <= N(syms));
    syms[0] = (TSym){name, 0, (u32)n, 1, CC_STT_FUNC, 1};
    for (size_t k = 0; k < s->nsyms; k++) syms[1 + k] = s->syms[k];
    AvpStr bytes = build_elf(secs, 2 + s->nextra, syms, 1 + s->nsyms, s->rels, s->nrels);
    const char *why;
    CcElf *elf = cc_elf_parse(a, bytes.p, bytes.n, &why);
    AVP_ASSERT(elf);
    AvpMap *m = cc_parse_listing(a, listing(name, 0, s->ins, s->nins), elf);
    return (CcFn *)one(m, name);
}

#define INS(arr) .ins = (arr), .nins = N(arr)
#define SYMS(arr) .syms = (arr), .nsyms = N(arr)
#define RELS(arr) .rels = (arr), .nrels = N(arr)

/* ---- helpers ----------------------------------------------------------------------------------------------------- */

static AvpStrVec vec_of(const char *const *s, size_t n) {
    AvpStrVec v = {0};
    for (size_t k = 0; k < n; k++) avp_vec_push(&v, avp_str(s[k]));
    return v;
}
#define VEC(...) vec_of((const char *const[]){__VA_ARGS__}, N(((const char *const[]){__VA_ARGS__})))

static FsFlags spec(const char *text) { return fs_parse_spec(avp_arena(), avp_str(text)); }

static FsFlags base_flags(void) {
    return spec("-nodefaults -proc gekko -fp hardware -O4,p -inline auto -pragma \"cats off\" -fp_contract on "
                "-str reuse -O3 -fp_contract off -func_align 4 -ipa file -lang=c++");
}

static AvpStrVec toks(const FsFlags *f) { return fs_flag_tokens(f); }

static long tok_index(const AvpStrVec *t, const char *s) {
    for (size_t k = 0; k < t->n; k++)
        if (avp_str_eqz(t->v[k], s)) return (long)k;
    return -1;
}

static bool toks_equal(const AvpStrVec *a, const AvpStrVec *b) {
    if (a->n != b->n) return false;
    for (size_t k = 0; k < a->n; k++)
        if (!avp_str_eq(a->v[k], b->v[k])) return false;
    return true;
}

static bool flag_is(FsFlag f, const char *name, const char *arg) {
    return avp_str_eqz(f.name, name) && (!arg ? f.arg.p == NULL : (f.arg.p && avp_str_eqz(f.arg, arg)));
}

/* Flag(name, arg) in flags; arg NULL is Flag(name). */
static bool flag_in(const FsFlags *f, const char *name, const char *arg) {
    for (size_t k = 0; k < f->n; k++)
        if (flag_is(f->v[k], name, arg)) return true;
    return false;
}

static AvpStr fam(const char *name, const char *arg) {
    FsFlag f = {avp_str(name), arg ? avp_str(arg) : (AvpStr){0}};
    return fs_family(avp_arena(), f);
}

/* An avp_throw (the Python's SystemExit) in fn: its message, else NULL. */
typedef void (*Thunk)(void *);
static const char *thrown(Thunk fn, void *arg) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        fn(arg);
        avp_try_end(&t);
        return NULL;
    }
    return avp_sprintf(avp_arena(), "%s", t.msg);
}

static bool has(AvpStr s, const char *needle) { return strstr(avp_cz(avp_arena(), s), needle) != NULL; }
static bool hasz(const char *s, const char *needle) { return s && strstr(s, needle) != NULL; }

/* ---- FlagTests --------------------------------------------------------------------------------------------------- */

AVP_TEST(parse_groups_arguments) {
    FsFlags flags = base_flags();
    CHECK(flag_in(&flags, "-fp_contract", "off"));
    CHECK(flag_in(&flags, "-pragma", "cats off"));
    CHECK(flag_in(&flags, "-nodefaults", NULL));
    CHECK(flag_in(&flags, "-lang=c++", NULL));
    AvpStrVec t = toks(&flags);
    REQUIRE(t.n >= 3);
    CHECK_STR(t.v[0], "-nodefaults");
    CHECK_STR(t.v[1], "-proc");
    CHECK_STR(t.v[2], "gekko");
}

AVP_TEST(dash_value_is_not_swallowed) {
    AvpStr in[] = {AVP_S("-ipa"), AVP_S("-O3")};
    FsFlags f = fs_parse_flags(in, 2);
    REQUIRE(f.n == 2);
    CHECK(flag_is(f.v[0], "-ipa", NULL));
    CHECK(flag_is(f.v[1], "-O3", NULL));
}

AVP_TEST(families) {
    CHECK(avp_str_eq(fam("-O4,p", NULL), fam("-O3", NULL)));
    CHECK(avp_str_eq(fam("-fp_contract", "on"), fam("-fp_contract", "off")));
    CHECK(!avp_str_eq(fam("-fp_contract", "on"), fam("-fp", "hardware")));
    CHECK(!avp_str_eq(fam("-pragma", "a"), fam("-pragma", "b")));
    CHECK(avp_str_eq(fam("-lang=c", NULL), fam("-lang=c++", NULL)));
}

AVP_TEST(replace_changes_only_the_last_occurrence) {
    AvpArena *a = avp_arena();
    FsFlags base = base_flags(), alt = spec("-O4,s");
    AvpStrVec fams = VEC("-O");
    FsFlags out = fs_apply_axis(a, &base, &alt, &fams);
    AvpStrVec t = toks(&out);
    long n44 = 0;
    for (size_t k = 0; k < t.n; k++) n44 += avp_str_eqz(t.v[k], "-O4,p");
    CHECK_EQ_I64(n44, 1); /* the earlier one stays: later options override */
    CHECK_EQ_I64(tok_index(&t, "-O3"), -1);
    long i = tok_index(&t, "-O4,s");
    REQUIRE(i >= 0 && (size_t)i + 2 < t.n);
    CHECK_STR(t.v[i + 1], "-fp_contract"); /* in place, not appended */
    CHECK_STR(t.v[i + 2], "off");
}

AVP_TEST(replace_with_identical_flag_is_identity) {
    AvpArena *a = avp_arena();
    FsFlags base = base_flags(), alt = spec("-O3");
    AvpStrVec fams = VEC("-O");
    FsFlags out = fs_apply_axis(a, &base, &alt, &fams);
    AvpStrVec t1 = toks(&out), t2 = toks(&base);
    CHECK(toks_equal(&t1, &t2));
}

AVP_TEST(absent_alternative_removes_every_occurrence) {
    AvpArena *a = avp_arena();
    FsFlags base = base_flags(), none = {0};
    AvpStrVec fams = {0};
    avp_vec_push(&fams, fam("-fp_contract", "on"));
    FsFlags out = fs_apply_axis(a, &base, &none, &fams);
    AvpStrVec t = toks(&out);
    CHECK_EQ_I64(tok_index(&t, "-fp_contract"), -1);
    CHECK_EQ_U64(out.n, base.n - 2);
}

AVP_TEST(absent_flag_is_appended) {
    AvpArena *a = avp_arena();
    FsFlags base = base_flags(), alt = spec("-schedule off");
    AvpStrVec fams = VEC("-schedule");
    FsFlags out = fs_apply_axis(a, &base, &alt, &fams);
    AvpStrVec t = toks(&out);
    REQUIRE(t.n >= 2);
    CHECK_STR(t.v[t.n - 2], "-schedule");
    CHECK_STR(t.v[t.n - 1], "off");
}

static void parse_axis_thunk(void *p) { (void)fs_parse_axis(avp_arena(), avp_str((const char *)p)); }

AVP_TEST(parse_axis) {
    AvpArena *a = avp_arena();
    FsAxis ax = fs_parse_axis(a, AVP_S("|-ipa file"));
    REQUIRE(ax.n == 2);
    CHECK_EQ_U64(ax.alts[0].n, 0);
    REQUIRE(ax.alts[1].n == 1);
    CHECK(flag_is(ax.alts[1].v[0], "-ipa", "file"));
    REQUIRE(ax.families.n == 1);
    CHECK_STR(ax.families.v[0], "-ipa");
    CHECK_STR(ax.labels[0], "no -ipa");
    CHECK_STR(ax.labels[1], "-ipa file");
    ax = fs_parse_axis(a, AVP_S("-O4,p|-O3|-O4,s"));
    REQUIRE(ax.n == 3);
    REQUIRE(ax.families.n == 1);
    CHECK_STR(ax.families.v[0], "-O");
    CHECK_STR(ax.labels[0], "-O4,p");
    CHECK_STR(ax.labels[1], "-O3");
    CHECK_STR(ax.labels[2], "-O4,s");
    CHECK(thrown(parse_axis_thunk, (void *)"|") != NULL);
}

/* ---- VariantTests ------------------------------------------------------------------------------------------------ */

static FsVariants build(const AvpStrVec *versions, const AvpStrVec *axes, const AvpStrVec *sets, const AvpStrVec *drops,
                        bool include_base, i64 max) {
    FsFlags base = base_flags();
    return fs_build_variants(avp_arena(), AVP_S("Wii/1.0"), &base, versions, axes, sets, drops, include_base, max);
}

AVP_TEST(product_and_current_mark) {
    AvpStrVec versions = VEC("Wii/1.0", "Wii/1.1"), axes = VEC("-O3|-O4,p", "-fp_contract on|-fp_contract off");
    FsVariants vs = build(&versions, &axes, NULL, NULL, true, 64);
    CHECK_EQ_U64(vs.n, 8);
    size_t ncur = 0, cur = 0;
    for (size_t k = 0; k < vs.n; k++)
        if (vs.v[k].current) ncur++, cur = k;
    CHECK_EQ_U64(ncur, 1);
    CHECK_STR(vs.v[cur].version, "Wii/1.0");
    REQUIRE(vs.v[cur].choices.n == 2);
    CHECK_STR(vs.v[cur].choices.v[0], "-O3");
    CHECK_STR(vs.v[cur].choices.v[1], "-fp_contract off");
    for (size_t k = 0; k < vs.n; k++)
        CHECK_MSG(avp_str_eqz(vs.v[k].id, avp_sprintf(avp_arena(), "v%zu", k)), "id of variant %zu", k);
    CHECK_STR(vs.v[0].label, "Wii/1.0 | -O3 | -fp_contract on");
}

AVP_TEST(base_is_added_when_not_in_the_product) {
    AvpStrVec axes = VEC("-O4,p|-O4,s");
    FsVariants vs = build(NULL, &axes, NULL, NULL, true, 64);
    REQUIRE(vs.n == 3);
    CHECK(vs.v[2].current);
    REQUIRE(vs.v[2].choices.n == 1);
    CHECK_STR(vs.v[2].choices.v[0], "(unit as is)");
    FsFlags base = base_flags();
    AvpStrVec t1 = toks(&vs.v[2].flags), t2 = toks(&base);
    CHECK(toks_equal(&t1, &t2));
    vs = build(NULL, &axes, NULL, NULL, false, 64);
    CHECK_EQ_U64(vs.n, 2);
    for (size_t k = 0; k < vs.n; k++) CHECK(!vs.v[k].current);
}

AVP_TEST(versions_only) {
    AvpStrVec versions = VEC("Wii/1.0", "GC/3.0a5.2");
    FsVariants vs = build(&versions, NULL, NULL, NULL, true, 64);
    REQUIRE(vs.n == 2);
    CHECK(vs.v[0].current);
    CHECK(!vs.v[1].current);
}

AVP_TEST(no_axes_gives_the_unit_itself) {
    FsVariants vs = build(NULL, NULL, NULL, NULL, true, 64);
    REQUIRE(vs.n == 1);
    CHECK(vs.v[0].current);
}

typedef struct {
    const AvpStrVec *versions, *axes;
    i64 max;
} BuildArg;
static void build_thunk(void *p) {
    BuildArg *b = p;
    (void)build(b->versions, b->axes, NULL, NULL, true, b->max);
}

AVP_TEST(cap) {
    AvpStrVec axes = VEC("-O3|-O4,p|-O4,s", "-fp_contract on|-fp_contract off", "|-ipa file", "-inline auto|-inline off");
    CHECK_EQ_U64(build(NULL, &axes, NULL, NULL, true, 64).n, 24);
    BuildArg lo = {NULL, &axes, 23};
    CHECK(hasz(thrown(build_thunk, &lo), "24 variants exceed --max 23"));
    AvpStrVec versions = VEC("Wii/1.0", "Wii/1.1");
    BuildArg ver = {&versions, &axes, 40}; /* versions count toward the cap */
    CHECK(thrown(build_thunk, &ver) != NULL);
}

AVP_TEST(set_and_drop_apply_to_every_variant) {
    AvpStrVec axes = VEC("-O3|-O4,p"), sets = VEC("-use_lmw_stmw off"), drops = VEC("-ipa");
    FsVariants vs = build(NULL, &axes, &sets, &drops, true, 64);
    REQUIRE(vs.n == 3);
    CHECK(vs.v[2].current); /* the unit as is stays in as the reference */
    for (size_t k = 0; k < 2; k++) {
        AvpStrVec t = toks(&vs.v[k].flags);
        long i = tok_index(&t, "-use_lmw_stmw");
        REQUIRE(i >= 0 && (size_t)i + 1 < t.n);
        CHECK_STR(t.v[i + 1], "off");
        CHECK_EQ_I64(tok_index(&t, "-ipa"), -1);
        CHECK(!vs.v[k].current); /* set/drop move every variant away from the unit */
    }
}

typedef struct {
    const char *dir, *text;
} VerArg;
static void versions_thunk(void *p) {
    VerArg *v = p;
    (void)fs_resolve_versions(avp_arena(), avp_str(v->text), v->dir);
}

AVP_TEST(resolve_versions) {
    AvpArena *a = avp_arena();
    char *d = avp_mkdtemp(a, "flagsweep_ct", "", NULL);
    REQUIRE(d != NULL);
    const char *names[] = {"Wii\\1.0", "Wii\\1.1", "GC\\3.0a5.2", "GC\\empty"};
    for (size_t k = 0; k < N(names); k++) {
        const char *dir = avp_sprintf(a, "%s\\%s", d, names[k]);
        AVP_ASSERT(avp_makedirs(dir, true));
        if (k != 3) AVP_ASSERT(avp_write_text(avp_sprintf(a, "%s\\mwcceppc.exe", dir), AVP_S(""), AVP_TM_UTF8));
    }
    AvpStrVec r = fs_resolve_versions(a, AVP_S("Wii/1.0,1.1,Wii/1.0"), d);
    REQUIRE(r.n == 2);
    CHECK_STR(r.v[0], "Wii/1.0");
    CHECK_STR(r.v[1], "Wii/1.1");
    r = fs_resolve_versions(a, AVP_S("all"), d);
    REQUIRE(r.n == 3);
    CHECK_STR(r.v[0], "Wii/1.0");
    CHECK_STR(r.v[1], "Wii/1.1");
    CHECK_STR(r.v[2], "GC/3.0a5.2");
    VerArg bad = {d, "Wii/9.9"};
    CHECK(thrown(versions_thunk, &bad) != NULL);
    avp_tempdir_cleanup(d);
}

/* ---- CompareTests ------------------------------------------------------------------------------------------------ */

static const char ASM[] =
    ".fn alpha, global\n"
    "/* 80000000 00000000  38 60 00 01 */\tli r3, 1\n"
    ".L_80000004:\n"
    "/* 80000004 00000004  48 00 00 08 */\tb .L_8000000C\n"
    "/* 80000008 00000008  4B FF FF F5 */\tbl fn_80001234\n"
    ".L_8000000C:\n"
    "/* 8000000C 0000000C  4E 80 00 20 */\tblr\n"
    ".endfn alpha\n"
    ".fn fn_80002000__Fv, global\n"
    "/* 80002000 00000000  4E 80 00 20 */\tblr\n"
    ".endfn fn_80002000__Fv\n";

static AvpStr nr(const char *line) { return fs_norm_row(avp_arena(), avp_str(line)); }

AVP_TEST(norm_row) {
    CHECK_STR(nr("/* 80000000 00000000  38 60 00 01 */\tli r3, 1"), "li r3, 1");
    CHECK(avp_str_eq(nr("\tlfs f0, \"@150\"@sda21(r0)"), nr("\tlfs f0, lbl_805290AC@sda21(r0)")));
    CHECK(avp_str_eq(nr("\taddi r30, r30, ...bss.0@l"), nr("\taddi r30, r30, lbl_80477538@l")));
    CHECK(avp_str_eq(nr("\tlis r3, jumptable_803D8780@ha"), nr("\tlis r3, @123@ha")));
    CHECK(avp_str_eq(nr("\tb .L_8000000C"), nr("\tb .L_00000010")));
    CHECK_STR(nr(".L_80000004:"), "");
    CHECK(!avp_str_eq(nr("\tli r3, 1"), nr("\tli r3, 2")));
}

AVP_TEST(parse_asm_functions) {
    AvpMap *fns = cc_parse_listing(avp_arena(), avp_str(ASM), NULL);
    AvpMapIter it = {0};
    size_t n = 0;
    const char *want[] = {"alpha", "fn_80002000__Fv"};
    while (avp_map_next(fns, &it)) {
        REQUIRE(n < 2);
        CHECK_STR(it.key, want[n]);
        n++;
    }
    CHECK_EQ_U64(n, 2);
    const CcFn *alpha = one(fns, "alpha");
    REQUIRE(alpha->nrows == 4);
    CHECK_STR(alpha->rows[0].norm, "li r3, 1");
    CHECK_STR(alpha->rows[1].norm, "b L");
    CHECK_STR(alpha->rows[2].norm, "bl SYM");
    CHECK_STR(alpha->rows[3].norm, "blr");
}

static size_t edit_of(const char *want, const char *got) {
    AvpStrVec w = avp_str_split(avp_str(want), AVP_S(""), -1), g = avp_str_split(avp_str(got), AVP_S(""), -1);
    return cc_edit_count(w.v, w.n, g.v, g.n);
}

AVP_TEST(edit_rows) {
    CHECK_EQ_U64(edit_of("a b c d", "a b c d"), 0);
    CHECK_EQ_U64(edit_of("a b c d", "a X c d"), 1);
    CHECK_EQ_U64(edit_of("a b c d", "a b I c d"), 1); /* insertion shifts nothing */
    CHECK_EQ_U64(edit_of("a b c d", "a d"), 2);
    CHECK_EQ_U64(edit_of("a b c d", ""), 4);
}

/* names[k] with the space-separated norms rows[k], as a map name -> CcFn* */
static AvpMap *fn_map(const char *const *names, const char *const *rows, size_t n) {
    AvpArena *a = avp_arena();
    AvpMap *m = avp_map_new();
    for (size_t k = 0; k < n; k++) {
        AvpStrVec r = avp_str_split(avp_str(rows[k]), AVP_S(""), -1);
        avp_map_put(m, avp_str(names[k]), NULL)->p = cc_fn_from_norms(a, avp_str(names[k]), r.v, r.n);
    }
    return m;
}

static const char *retail_name(const char *cname, const AvpMap *retail) {
    AvpStr r = cc_retail_name_for(avp_str(cname), retail);
    return r.p ? avp_cz(avp_arena(), r) : NULL;
}

AVP_TEST(retail_name_for) {
    const char *names[] = {"alpha", "fn_80002000__Fv", "fn_80003000"}, *rows[] = {"", "", ""};
    AvpMap *retail = fn_map(names, rows, 3);
    CHECK_CZ(retail_name("alpha", retail), "alpha");
    CHECK_CZ(retail_name("fn_80002000__Fv", retail), "fn_80002000__Fv");
    CHECK_CZ(retail_name("fn_80002000__FRi", retail), "fn_80002000__Fv");
    CHECK_CZ(retail_name("fn_80003000__8unk_RampFv", retail), "fn_80003000");
    CHECK(retail_name("beta", retail) == NULL);
}

AVP_TEST(score_variant) {
    const char *rn[] = {"a", "b", "c"}, *rr[] = {"x y", "x", "p q r"};
    const char *cn[] = {"a", "b"}, *cr[] = {"x y", "z"};
    AvpMap *retail = fn_map(rn, rr, 3), *comp = fn_map(cn, cr, 2);
    FsScore *res = fs_score_variant(avp_arena(), retail, comp, NULL, true);
    CHECK(res[0].exact);
    CHECK_EQ_U64(res[0].diff, 0);
    CHECK_EQ_U64(res[0].rows, 2);
    CHECK_EQ_U64(res[0].targets, 0);
    CHECK_EQ_U64(res[0].unresolved, 0);
    CHECK_CZ(res[0].status, "ok");
    CHECK_CZ(res[1].status, "diff");
    CHECK_EQ_U64(res[1].diff, 1);
    CHECK(!res[2].exact);
    CHECK_EQ_U64(res[2].diff, 3);
    CHECK_EQ_U64(res[2].rows, 3);
    CHECK_EQ_U64(res[2].targets, 0);
    CHECK_EQ_U64(res[2].unresolved, 0);
    CHECK_CZ(res[2].status, "missing");
}

/* ---- the wrong-callee fixtures (tools/test_codecmp.py: retail_fn, compiled_fn, fake_project) ----------------------- */

static const char *g_root;
static int g_project_no;

static void remove_root(void) {
    if (g_root) avp_tempdir_cleanup(g_root);
}

static const char *SYMBOLS =
    "OSDisableInterrupts = .text:0x80001000; // type:function size:0x14 scope:global\n"
    "OSEnableInterrupts = .text:0x80001020; // type:function size:0x14 scope:global\n"
    "fn_80002000__Fv = .text:0x80002000; // type:function size:0x10\n"
    "Callback = .text:0x80003000; // type:function size:0xC scope:local\n"
    "Callback = .text:0x80004000; // type:function size:0xC scope:local\n"
    "lbl_80500010 = .sbss:0x80500010; // type:object size:0x4 scope:global\n"
    "lbl_80500014 = .sbss:0x80500014; // type:object size:0x4 scope:global\n";

/* This test's own scratch folder, build/tmp/flagsweep/_ctest_<pid> (removed at exit). */
static const char *scratch(void) {
    if (!g_root) {
        g_root = avp_sprintf(avp_arena(), "%s\\build\\tmp\\flagsweep\\_ctest_%u", avp_root(), avp_getpid());
        AVP_ASSERT(avp_makedirs(g_root, true));
        atexit(remove_root);
    }
    return g_root;
}

/* fake_project(symbols): a root folder holding config/R5WEA4/symbols.txt and splits.txt. */
static CcProject *new_project(const char *symbols) {
    AvpArena *a = avp_arena();
    char *root = avp_sprintf(a, "%s\\p%d", scratch(), ++g_project_no);
    AVP_ASSERT(avp_makedirs(avp_sprintf(a, "%s\\config\\R5WEA4", root), true));
    AVP_ASSERT(avp_write_text(avp_sprintf(a, "%s\\config\\R5WEA4\\symbols.txt", root), avp_str(symbols), AVP_TM_UTF8));
    AVP_ASSERT(avp_write_text(avp_sprintf(a, "%s\\config\\R5WEA4\\splits.txt", root),
                              AVP_S("Sections:\n\t.text type:code align:8\n\n"), AVP_TM_UTF8));
    return cc_project_new(root);
}

typedef struct {
    AvpMap *retail, *good, *bad;
    CcContext *ctx;
} WrongCallee;

static AvpMap *single(const char *name, CcFn *fn) {
    AvpMap *m = avp_map_new();
    avp_map_put(m, avp_str(name), NULL)->p = fn;
    return m;
}

/* Retail calls fn_80001000, the compiled function fn_80002000: same text, and both names are the kind the old
 * normalisation collapses. */
static WrongCallee wrong_callee(void) {
    WrongCallee w;
    const TI ri[] = {{"48001001", "bl fn_80001000"}, {"4E800020", "blr"}};
    const TI gi[] = {{"48000001", "bl fn_80001000"}, {"4E800020", "blr"}};
    const TI bi[] = {{"48000001", "bl fn_80002000"}, {"4E800020", "blr"}};
    const TSym gs[] = {{"fn_80001000", 0, 0, 1, 0, 0}}, bs[] = {{"fn_80002000", 0, 0, 1, 0, 0}};
    const TRel rels[] = {{1, 0, CC_R_REL24, 2, 0}};
    w.retail = single("f", retail_fn("f", ri, N(ri)));
    w.good = single("f", compiled_fn("f", &(CSpec){INS(gi), SYMS(gs), RELS(rels)}));
    w.bad = single("f", compiled_fn("f", &(CSpec){INS(bi), SYMS(bs), RELS(rels)}));
    w.ctx = cc_context_new(new_project(SYMBOLS), AVP_S("game/Foo"));
    return w;
}

AVP_TEST(score_variant_flags_a_wrong_callee) {
    WrongCallee w = wrong_callee();
    FsScore *ok = fs_score_variant(avp_arena(), w.retail, w.good, w.ctx, true);
    CHECK(ok[0].exact);
    CHECK_CZ(ok[0].status, "ok");
    CHECK_EQ_U64(ok[0].targets, 0);
    FsScore *res = fs_score_variant(avp_arena(), w.retail, w.bad, w.ctx, true);
    CHECK(!res[0].exact);
    CHECK_CZ(res[0].status, "target");
    CHECK_EQ_U64(res[0].diff, 0);
    CHECK_EQ_U64(res[0].targets, 1);
    /* the old comparison (and a missing context) cannot see it */
    CHECK(fs_score_variant(avp_arena(), w.retail, w.bad, w.ctx, false)[0].exact);
    CHECK(fs_score_variant(avp_arena(), w.retail, w.bad, NULL, true)[0].exact);
}

static AvpStr cell_of(bool exact, size_t diff, size_t targets, size_t unresolved, const char *status) {
    FsScore f = {exact, diff, 5, targets, unresolved, status};
    return fs_cell(avp_arena(), &f);
}

AVP_TEST(cell) {
    CHECK_STR(cell_of(true, 0, 0, 0, "ok"), "ok");
    CHECK_STR(cell_of(true, 0, 0, 2, "ok"), "ok?");
    CHECK_STR(cell_of(false, 12, 0, 0, "diff"), "12");
    CHECK_STR(cell_of(false, 0, 3, 0, "target"), "3t");
    CHECK_STR(cell_of(false, 12, 3, 0, "diff"), "12+3t");
    CHECK_STR(cell_of(false, 5, 0, 0, "missing"), "--");
    /* the Python's last case passes a dict without target keys; a C FsScore always has them (0) */
    CHECK_STR(cell_of(false, 2, 0, 0, "diff"), "2");
}

AVP_TEST(wanted) {
    AvpStrVec none = {0}, a = VEC("fn_80002000"), b = VEC("foo");
    CHECK(fs_wanted(AVP_S("anything"), &none));
    CHECK(fs_wanted(AVP_S("fn_80002000__Fv"), &a));
    CHECK(fs_wanted(AVP_S("foo__Fv"), &b));
    CHECK(!fs_wanted(AVP_S("bar"), &b));
}

/* ---- ErrorSummaryTests ------------------------------------------------------------------------------------------- */

AVP_TEST(first_error) {
    AvpArena *a = avp_arena();
    CHECK_STR(fs_first_error(a, AVP_S("### mwcceppc.exe Usage Error:\n#   Unknown option 'ipa'\n\nErrors caused tool to abort.\n")),
              "mwcceppc.exe Usage Error: Unknown option 'ipa'");
    CHECK_STR(fs_first_error(a, AVP_S("Error: bad\nmore")), "Error: bad");
    CHECK_STR(fs_first_error(a, AVP_S("")), "");
}

/* ---- ReportTests ------------------------------------------------------------------------------------------------- */

static FsVariant mk_variant(const char *id, const char *label, bool current) {
    FsVariant v = {0};
    v.id = avp_str(id);
    v.label = avp_str(label);
    v.version = AVP_S("Wii/1.0");
    v.current = current;
    v.flags = spec("-O3");
    v.compiled = true;
    v.has_seconds = true;
    v.seconds = 0.1;
    return v;
}

static FsScore *scores(const FsScore *src, size_t n) {
    FsScore *p = avp_alloc(avp_arena(), n * sizeof *p);
    memcpy(p, src, n * sizeof *p);
    return p;
}

static void sum_entry(FsEntry *e, size_t n) {
    for (size_t k = 0; k < n; k++) {
        e->exact += e->fns[k].exact;
        e->diff += (i64)e->fns[k].diff;
        e->targets += (i64)e->fns[k].targets;
        e->unresolved += (i64)e->fns[k].unresolved;
    }
}

static const AvpStr *line_with(const AvpStrVec *lines, const char *prefix) {
    for (size_t k = 0; k < lines->n; k++)
        if (avp_str_startswith(lines->v[k], avp_str(prefix))) return &lines->v[k];
    return NULL;
}

/* The words of a matrix row after the function name. */
static AvpStrVec row_cells(const AvpStr *line) {
    AvpStrVec w = line ? avp_str_split(*line, AVP_S(""), -1) : (AvpStrVec){0};
    AvpStrVec out = {0};
    for (size_t k = 1; k < w.n; k++) avp_vec_push(&out, w.v[k]);
    return out;
}

static bool cells_are(const AvpStrVec *c, const char *a, const char *b, const char *d) {
    const char *want[3] = {a, b, d};
    size_t n = d ? 3 : 2;
    if (c->n != n) return false;
    for (size_t k = 0; k < n; k++)
        if (!avp_str_eqz(c->v[k], want[k])) return false;
    return true;
}

AVP_TEST(format) {
    AvpArena *a = avp_arena();
    static const FsScore ok = {true, 0, 5, 0, 0, "ok"}, bad = {false, 7, 5, 0, 0, "diff"},
                         gone = {false, 5, 5, 0, 0, "missing"};
    FsVariant vs[3] = {mk_variant("v0", "Wii/1.0 | -O3", true), mk_variant("v1", "Wii/1.0 | -O4,p", false),
                       mk_variant("v2", "Wii/1.0 | -bogus", false)};
    vs[2].compiled = false;
    vs[2].error = AVP_S("bad option");
    vs[2].has_seconds = false;
    FsEntry en[3] = {{0}};
    const FsScore s0[2] = {ok, ok}, s1[2] = {bad, gone};
    en[0] = (FsEntry){.v = &vs[0], .ok = true, .fns = scores(s0, 2)};
    en[1] = (FsEntry){.v = &vs[1], .ok = true, .fns = scores(s1, 2)};
    en[2] = (FsEntry){.v = &vs[2]};
    sum_entry(&en[0], 2);
    sum_entry(&en[1], 2);
    FsResult res = {{0}};
    avp_vec_push(&res.functions, AVP_S("f1"));
    avp_vec_push(&res.functions, AVP_S("f2"));
    res.retail_functions = 3;
    res.seconds = 1.2;
    res.entries = en;
    res.nentries = 3;
    size_t rank[3] = {0, 1, 2};
    res.ranking = rank;
    AvpStr text = fs_format_report(a, &res, AVP_S("hdr"), true, 140);
    AvpStrVec lines = avp_str_splitlines(text, false);
    REQUIRE(lines.n > 0);
    CHECK_STR(lines.v[0], "hdr");
    CHECK(has(text, "v0*"));
    CHECK(has(text, "compile error: bad option"));
    AvpStrVec c = row_cells(line_with(&lines, "f1 "));
    CHECK(cells_are(&c, "ok", "7", "ERR"));
    c = row_cells(line_with(&lines, "f2 "));
    CHECK(cells_are(&c, "ok", "--", "ERR"));
    AvpStr r[3] = {{0}};
    const char *pfx[3] = {" 1 ", " 2 ", " 3 "};
    for (size_t k = 0; k < 3; k++) {
        const AvpStr *l = line_with(&lines, pfx[k]);
        REQUIRE(l != NULL);
        r[k] = *l;
    }
    CHECK(has(r[0], "2/2"));
    CHECK(has(r[0], "[current]"));
    CHECK(has(r[2], "[compile failed]"));
}

AVP_TEST(target_mismatches_show_in_matrix_and_ranking) {
    AvpArena *a = avp_arena();
    WrongCallee w = wrong_callee();
    FsVariant ok_v = mk_variant("v0", "Wii/1.0 | good", false), bad_v = mk_variant("v1", "Wii/1.0 | bad", false);
    FsEntry en[2] = {{0}};
    en[0] = (FsEntry){.v = &bad_v, .ok = true, .fns = fs_score_variant(a, w.retail, w.bad, w.ctx, true)};
    en[1] = (FsEntry){.v = &ok_v, .ok = true, .fns = fs_score_variant(a, w.retail, w.good, w.ctx, true)};
    sum_entry(&en[0], 1);
    sum_entry(&en[1], 1);
    FsResult res = {{0}};
    avp_vec_push(&res.functions, AVP_S("f"));
    res.retail_functions = 1;
    res.entries = en;
    res.nentries = 2;
    size_t rank[2] = {1, 0}; /* ["v0", "v1"] */
    res.ranking = rank;
    AvpStr text = fs_format_report(a, &res, AVP_S(""), false, 140);
    AvpStrVec lines = avp_str_splitlines(text, false);
    AvpStrVec c = row_cells(line_with(&lines, "f "));
    CHECK(cells_are(&c, "1t", "ok", NULL));
    const AvpStr *r1 = line_with(&lines, " 1 "), *r2 = line_with(&lines, " 2 ");
    REQUIRE(r1 && r2);
    CHECK(has(*r1, "1/1"));
    CHECK(has(*r2, "0/1"));
}

AVP_TEST(sweep_ranks_a_target_mismatch_below_an_exact_variant) {
    /* The Python builds the fixture too but ranks plain dicts; rank_entries is fs_rank here. */
    FsVariant v1 = mk_variant("v1", "", false), v2 = mk_variant("v2", "", false), v3 = mk_variant("v3", "", false),
              v4 = mk_variant("v4", "", true);
    FsEntry en[4] = {{.v = &v1, .ok = true, .exact = 0, .diff = 0, .targets = 1},
                     {.v = &v2, .ok = true, .exact = 1, .diff = 0, .targets = 0},
                     {.v = &v3, .ok = false},
                     {.v = &v4, .ok = true, .exact = 0, .diff = 2, .targets = 0}};
    FsResult res = {{0}};
    res.entries = en;
    res.nentries = 4;
    fs_rank(&res);
    CHECK_EQ_U64(res.ranking[0], 1); /* v2 */
    CHECK_EQ_U64(res.ranking[1], 0); /* v1 */
    CHECK_EQ_U64(res.ranking[2], 3); /* v4 */
    CHECK_EQ_U64(res.ranking[3], 2); /* v3 */
}

AVP_TEST(wraps_wide_matrices) {
    static const FsScore ok = {true, 0, 5, 0, 0, "ok"};
    FsVariant vs[3] = {mk_variant("v0", "a", true), mk_variant("v1", "b", false), mk_variant("v2", "c", false)};
    FsEntry en[3] = {{0}};
    const FsScore s0[1] = {ok};
    for (size_t k = 0; k < 3; k++) {
        en[k] = (FsEntry){.v = &vs[k], .ok = true, .fns = scores(s0, 1)};
        sum_entry(&en[k], 1);
    }
    FsResult res = {{0}};
    avp_vec_push(&res.functions, AVP_S("f1"));
    res.retail_functions = 1;
    res.entries = en;
    res.nentries = 3;
    size_t rank[3] = {0, 1, 2};
    res.ranking = rank;
    AvpStr text = fs_format_report(avp_arena(), &res, AVP_S(""), false, 18);
    AvpStrVec lines = avp_str_splitlines(text, false);
    size_t n = 0;
    for (size_t k = 0; k < lines.n; k++) n += avp_str_startswith(lines.v[k], AVP_S("function"));
    CHECK(n > 1);
}

/* ---- RealCompileTest --------------------------------------------------------------------------------------------- */

static bool have_real_tools(void) {
    AvpArena *a = avp_arena();
    return avp_exists(avp_sprintf(a, "%s\\tools\\compilers\\Wii\\1.0\\mwcceppc.exe", avp_root())) &&
           avp_exists(avp_sprintf(a, "%s\\tools\\bin\\dtk.exe", avp_root()));
}

/* compile_variant: the compiler on src with flags into dir\<stem>.o, then the object read with dtk. NULL with *error
 * (the compiler's first error line) when it fails. */
static AvpMap *compile_c(const char *src, const FsFlags *flags, const char *dir, AvpStr *error) {
    AvpArena *a = avp_arena();
    AVP_ASSERT(avp_makedirs(dir, true));
    AvpStr stem, ext;
    avp_path_splitext(avp_cz(a, avp_path_basename(src)), &stem, &ext);
    const char *obj = avp_sprintf(a, "%s\\%.*s.o", dir, AVP_SF(stem));
    const char *wrap = avp_sprintf(a, "%s\\build\\tools\\sjiswrap.exe", avp_root());
    AvpStrVec t = fs_flag_tokens(flags);
    const char **cmd = avp_alloc(a, (t.n + 7) * sizeof *cmd);
    size_t n = 0;
    if (avp_exists(wrap)) cmd[n++] = wrap;
    cmd[n++] = avp_sprintf(a, "%s\\tools\\compilers\\Wii\\1.0\\mwcceppc.exe", avp_root());
    for (size_t k = 0; k < t.n; k++) cmd[n++] = avp_cz(a, t.v[k]);
    cmd[n++] = "-c";
    cmd[n++] = src;
    cmd[n++] = "-o";
    cmd[n++] = obj;
    cmd[n] = NULL;
    AvpProcResult r = avp_run(&(AvpProcSpec){.argv = cmd, .cwd = avp_root(), .out = AVP_IO_CAPTURE,
                                             .err = AVP_IO_CAPTURE, .timeout = 600, .arena = a});
    if (r.status != AVP_PROC_OK || r.returncode || !avp_exists(obj)) {
        *error = fs_first_error(a, permute_universal_newlines(a, avp_str_concat(a, r.out, r.err)));
        if (!error->n) *error = AVP_S("compile failed");
        return NULL;
    }
    AvpStr err = {0};
    AvpMap *fns = cc_load_object(a, obj, NULL, &err);
    if (!fns) *error = err;
    return fns;
}

static const char *write_src(const char *dir, const char *name, const char *text) {
    AvpArena *a = avp_arena();
    AVP_ASSERT(avp_makedirs(dir, true));
    const char *path = avp_sprintf(a, "%s\\%s", dir, name);
    AVP_ASSERT(avp_write_text(path, avp_str(text), AVP_TM_UTF8));
    return path;
}

AVP_TEST(fp_contract_axis) {
    if (!have_real_tools()) return;
    AvpArena *a = avp_arena();
    const char *src = write_src(avp_sprintf(a, "%s\\fp", scratch()), "madd.c",
                                "float madd(float a, float b, float c) { return a * b + c; }\n"
                                "int twice(int x) { return x + x; }\n");
    FsFlags flags = spec("-nodefaults -proc gekko -fp hardware -O4,p -fp_contract on");
    AvpStr error = {0};
    /* "retail" = the unit as configured, so the current variant must score exact */
    AvpMap *retail = compile_c(src, &flags, avp_sprintf(a, "%s\\fp\\ref", scratch()), &error);
    REQUIRE(retail != NULL);
    CHECK(avp_map_get(retail, AVP_S("madd")) != NULL);
    AvpStrVec axes = VEC("-fp_contract on|-fp_contract off|-bogusflag");
    FsVariants vs = fs_build_variants(a, AVP_S("Wii/1.0"), &flags, NULL, &axes, NULL, NULL, true, 64);
    REQUIRE(vs.n == 3);
    FsEntry *en = avp_alloc(a, vs.n * sizeof *en);
    FsScore **sc = avp_alloc(a, vs.n * sizeof *sc);
    CcProject *proj = cc_project_new(avp_root());
    for (size_t k = 0; k < vs.n; k++) {
        AvpMap *fns = compile_c(src, &vs.v[k].flags, avp_sprintf(a, "%s\\fp\\%.*s", scratch(), AVP_SF(vs.v[k].id)),
                                &error);
        sc[k] = fns ? fs_score_variant(a, retail, fns, cc_context_new(proj, AVP_S("")), true) : NULL;
        en[k].v = &vs.v[k];
    }
    /* the universe: retail functions some successful variant compiled (sweep()) */
    AvpStrVec fnames = {0};
    size_t pick[8], npick = 0, idx = 0;
    AvpMapIter it = {0};
    while (avp_map_next(retail, &it)) {
        bool any = false;
        for (size_t k = 0; k < vs.n; k++) any |= sc[k] && strcmp(sc[k][idx].status, "missing") != 0;
        if (any) {
            avp_vec_push(&fnames, it.key);
            pick[npick++] = idx;
        }
        idx++;
    }
    for (size_t k = 0; k < vs.n; k++) {
        if (!sc[k]) continue;
        en[k].ok = true;
        en[k].fns = avp_alloc(a, (npick + 1) * sizeof *en[k].fns);
        for (size_t f = 0; f < npick; f++) en[k].fns[f] = sc[k][pick[f]];
        sum_entry(&en[k], npick);
    }
    FsResult res = {{0}};
    res.functions = fnames;
    res.retail_functions = avp_map_len(retail);
    res.entries = en;
    res.nentries = vs.n;
    fs_rank(&res);
    size_t on = vs.n, off = vs.n;
    for (size_t k = 0; k < vs.n; k++) {
        if (avp_str_eqz(vs.v[k].label, "Wii/1.0 | -fp_contract on")) on = k;
        if (avp_str_eqz(vs.v[k].label, "Wii/1.0 | -fp_contract off")) off = k;
    }
    REQUIRE(on < vs.n && off < vs.n);
    CHECK(vs.v[on].current);
    CHECK(en[on].ok);
    CHECK_EQ_I64(en[on].exact, 2);
    CHECK_EQ_I64(en[on].diff, 0);
    CHECK_EQ_I64(en[off].exact, 1); /* twice() is unaffected */
    REQUIRE(npick == 2);
    CHECK(!en[off].fns[0].exact); /* fmadds becomes fmuls + fadds (madd is the first retail function) */
    CHECK(en[off].diff > 0);
    CHECK_EQ_U64(res.ranking[0], on);
    REQUIRE(fnames.n == 2);
    CHECK_STR(fnames.v[0], "madd");
    CHECK_STR(fnames.v[1], "twice");
    CHECK(has(fs_format_report(a, &res, AVP_S(""), false, 140), "v0"));
    CHECK(!en[2].ok); /* the bogus-flag variant did not compile */
}

AVP_TEST(wrong_callee_through_the_real_compiler_and_elf) {
    if (!have_real_tools()) return;
    AvpArena *a = avp_arena();
    FsFlags flags = spec("-nodefaults -proc gekko -O4,p");
    const char *dir = avp_sprintf(a, "%s\\tgt", scratch());
    AvpMap *comp[2] = {0};
    const char *tags[2] = {"a", "b"}, *callee[2] = {"fn_80001000", "fn_80002000"};
    for (size_t k = 0; k < 2; k++) {
        const char *text = avp_sprintf(a, "void foo(void); void bar(void);\nvoid go(int n) { if (n) %s(); }\n", callee[k]);
        const char *src = write_src(dir, avp_sprintf(a, "%s.c", tags[k]), text);
        AvpStr error = {0};
        comp[k] = compile_c(src, &flags, avp_sprintf(a, "%s\\%s", dir, tags[k]), &error);
        CHECK_MSG(comp[k] != NULL, "compile %s: %.*s", tags[k], AVP_SF(error));
    }
    REQUIRE(comp[0] && comp[1]);
    AvpStr lst;
    REQUIRE(avp_read_text(a, avp_sprintf(a, "%s\\a\\a.o.s", dir), AVP_TM_UTF8, &lst));
    AvpMap *retail = cc_parse_listing(
        a, avp_str_replace(a, lst, AVP_S("48 00 00 00 */"), AVP_S("48 00 10 00 */"), -1), NULL);
    CcContext *ctx = cc_context_new(new_project("foo = .text:0x80001000; // type:function size:0x4\n"
                                                "bar = .text:0x80002000; // type:function size:0x4\n"),
                                    AVP_S("game/Foo"));
    FsScore *same = fs_score_variant(a, retail, comp[0], ctx, true);
    CHECK(same[0].exact);
    CHECK_EQ_U64(same[0].rows, 4);
    FsScore *other = fs_score_variant(a, retail, comp[1], ctx, true);
    CHECK_EQ_U64(other[0].diff, 0);
    CHECK_EQ_U64(other[0].targets, 1);
    CHECK_CZ(other[0].status, "target");
    CHECK(fs_score_variant(a, retail, comp[1], ctx, false)[0].exact);
}

AVP_TEST(compile_failure_is_reported) {
    if (!have_real_tools()) return;
    AvpArena *a = avp_arena();
    const char *src = write_src(avp_sprintf(a, "%s\\fail", scratch()), "bad.c", "int broken( { return 1; }\n");
    FsFlags flags = spec("-nodefaults -proc gekko -O4,p");
    AvpStr error = {0};
    AvpMap *fns = compile_c(src, &flags, avp_sprintf(a, "%s\\fail\\o", scratch()), &error);
    CHECK(fns == NULL);
    CHECK(error.n > 0);
    /* sweep() records the failure on the variant and the report names it */
    FsVariants vs = fs_build_variants(a, AVP_S("Wii/1.0"), &flags, NULL, NULL, NULL, NULL, true, 64);
    REQUIRE(vs.n == 1);
    vs.v[0].error = error;
    FsEntry en[1] = {{.v = &vs.v[0]}};
    FsResult res = {{0}};
    res.retail_functions = 1;
    res.entries = en;
    res.nentries = 1;
    fs_rank(&res);
    CHECK(has(fs_format_report(a, &res, AVP_S(""), false, 140), "compile failed"));
}

AVP_TEST_MAIN("flagsweep")
