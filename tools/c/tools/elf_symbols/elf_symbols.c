/* A port of tools/scripts/elf_symbols.py: ELF32 little-endian MIPS section/symbol dumper (sections.txt, symbols.txt,
 * source_files.txt, classes.txt), the string-mining fallback and the self-test of the demangler.
 *
 * Names and strings are latin-1 bytes until they are written (the Python decodes with latin-1). */
#include "elf_symbols.h"

#include <stdlib.h>
#include <string.h>

#include "shsm_re.h"

/* print(__doc__) */
#include "usage.inc"

#define TRACEBACK(kind, what) avp_fatal("elf_symbols.exe: error: %s: %s", (kind), (what))

typedef struct {
    u32 idx, nameoff, type, flags, addr, off, size, link, info, entsz;
    AvpStr name;
} Section;

typedef struct {
    AvpStr d;
    Section *sec;
    size_t nsec;
} Elf;

typedef struct {
    u32 val, size;
    const char *typ, *bind;
    AvpStr sec, name;
    char typ_buf[16], bind_buf[16];
} Sym;

static u32 rd32(AvpStr d, size_t off) {
    if (off + 4 > d.n) TRACEBACK("struct.error", "unpack_from requires a buffer of at least 4 bytes");
    const u8 *p = (const u8 *)d.p + off;
    return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;
}
static u32 rd16(AvpStr d, size_t off) {
    if (off + 2 > d.n) TRACEBACK("struct.error", "unpack_from requires a buffer of at least 2 bytes");
    const u8 *p = (const u8 *)d.p + off;
    return (u32)p[0] | (u32)p[1] << 8;
}

/* Elf.cstr(off): d.index(b'\0', off) raises ValueError when there is no terminator */
static AvpStr cstr(const Elf *e, size_t off) {
    if (off > e->d.n) TRACEBACK("ValueError", "subsection not found");
    const char *z = memchr(e->d.p + off, 0, e->d.n - off);
    if (!z) TRACEBACK("ValueError", "subsection not found");
    return avp_strn(e->d.p + off, (size_t)(z - (e->d.p + off)));
}

static Elf elf_open(AvpArena *a, const char *path) {
    AvpBytes b = {0};
    if (!avp_read_all(a, path, &b)) TRACEBACK("FileNotFoundError", path);
    Elf e = {{(const char *)b.v, b.n}, NULL, 0};
    AvpStr d = e.d;
    if (d.n < 4 || memcmp(d.p, "\x7f" "ELF", 4) != 0 || d.n < 6 || d.p[4] != 1 || d.p[5] != 1)
        avp_fatal("not an ELF32 little-endian file");
    /* struct.unpack_from('<HHIIIIIHHHHHH', d, 16) */
    u32 shoff = rd32(d, 16 + 2 + 2 + 4 + 4 + 4);
    u32 shentsize = rd16(d, 16 + 2 + 2 + 4 + 4 + 4 + 4 + 4 + 2 + 2 + 2);
    u32 shnum = rd16(d, 16 + 2 + 2 + 4 + 4 + 4 + 4 + 4 + 2 + 2 + 2 + 2);
    u32 shstrndx = rd16(d, 16 + 2 + 2 + 4 + 4 + 4 + 4 + 4 + 2 + 2 + 2 + 2 + 2);
    e.nsec = shnum;
    e.sec = avp_xcalloc(shnum + 1, sizeof(Section));
    for (u32 i = 0; i < shnum; i++) {
        size_t o = (size_t)shoff + (size_t)i * shentsize;
        Section *s = &e.sec[i];
        s->idx = i;
        s->nameoff = rd32(d, o);
        s->type = rd32(d, o + 4);
        s->flags = rd32(d, o + 8);
        s->addr = rd32(d, o + 12);
        s->off = rd32(d, o + 16);
        s->size = rd32(d, o + 20);
        s->link = rd32(d, o + 24);
        s->info = rd32(d, o + 28);
        rd32(d, o + 32); /* align */
        s->entsz = rd32(d, o + 36);
    }
    if (shnum) {
        if (shstrndx >= shnum) TRACEBACK("IndexError", "list index out of range");
        size_t so = e.sec[shstrndx].off;
        for (u32 i = 0; i < shnum; i++) e.sec[i].name = cstr(&e, so + e.sec[i].nameoff);
    }
    return e;
}

static const Section *elf_section(const Elf *e, const char *name) {
    for (size_t i = 0; i < e->nsec; i++)
        if (avp_str_eqz(e->sec[i].name, name)) return &e->sec[i];
    return NULL;
}

static const char *stt_name(unsigned v, char *buf) {
    static const char *names[] = {"NOTYPE", "OBJECT", "FUNC", "SECTION", "FILE", "COMMON", "TLS"};
    if (v < 7) return names[v];
    snprintf(buf, 16, "%u", v);
    return buf;
}
static const char *stb_name(unsigned v, char *buf) {
    static const char *names[] = {"LOCAL", "GLOBAL", "WEAK"};
    if (v < 3) return names[v];
    snprintf(buf, 16, "%u", v);
    return buf;
}

typedef AVP_VEC(Sym) SymVec;

static SymVec elf_symbols(AvpArena *a, const Elf *e) {
    SymVec out = {0};
    const Section *st = elf_section(e, ".symtab");
    if (!st) return out;
    if (st->link >= e->nsec) TRACEBACK("IndexError", "list index out of range");
    size_t strtab = e->sec[st->link].off;
    for (u32 i = 0; i < st->size / 16; i++) {
        size_t o = (size_t)st->off + (size_t)i * 16;
        u32 nm = rd32(e->d, o), val = rd32(e->d, o + 4), size = rd32(e->d, o + 8);
        if (o + 16 > e->d.n) TRACEBACK("struct.error", "unpack_from requires a buffer of at least 16 bytes");
        unsigned info = (u8)e->d.p[o + 12];
        unsigned shndx = rd16(e->d, o + 14);
        Sym *s = avp_vec_push_zero(&out);
        if (shndx == 0) s->sec = avp_str("UND");
        else if (shndx == 0xfff1) s->sec = avp_str("ABS");
        else if (shndx == 0xfff2) s->sec = avp_str("COM");
        else if (shndx < e->nsec) s->sec = e->sec[shndx].name;
        else s->sec = avp_str_dup(a, avp_str((snprintf(s->typ_buf, 16, "%u", shndx), s->typ_buf)));
        s->val = val;
        s->size = size;
        s->typ = stt_name(info & 15, s->typ_buf);
        s->bind = stb_name(info >> 4, s->bind_buf);
        s->name = nm ? cstr(e, strtab + nm) : avp_str("");
    }
    return out;
}

/* ---- string mining ------------------------------------------------------------------------------------------- */
static void put_unique(AvpMap *seen, AvpStrVec *out, AvpStr s) {
    bool created = false;
    avp_map_put(seen, s, &created);
    if (created) avp_vec_push(out, s);
}

void elf_mine_strings(AvpArena *a, AvpStr data, AvpStrVec *paths, AvpStrVec *quals, AvpStrVec *cnames) {
    (void)a;
    /* PATH_RE: the class holds a backslash-tilde pair that Python reads as a plain `~` */
    const ShsmRe *run_re = shsm_re("[\\x20-\\x7e]{4,}", SHSM_RE_BYTES);
    const ShsmRe *path_re = shsm_re("[A-Za-z0-9_./:\\~+-]*[A-Za-z0-9_]+\\.(?:c|cpp|cc|cxx|h|hpp|s|vsm|dsm|vcl)\\b", SHSM_RE_BYTES);
    const ShsmRe *qual_re = shsm_re("\\b[A-Za-z_][A-Za-z0-9_]*(?:::~?[A-Za-z_][A-Za-z0-9_]*)+", SHSM_RE_BYTES);
    const ShsmRe *cname_re = shsm_re("C[A-Z][A-Za-z0-9]{4,}", SHSM_RE_BYTES);
    const ShsmRe *lower3_re = shsm_re("[a-z]{3}", SHSM_RE_BYTES);
    AvpMap *seen_p = avp_map_new(), *seen_q = avp_map_new(), *seen_c = avp_map_new();
    AvpStrVec runs = {0};
    ShsmReIter it;
    ShsmReMatch m;
    shsm_re_iter_init(&it, run_re, data, 0, AVP_END);
    while (shsm_re_iter_next(&it, &m)) avp_vec_push(&runs, shsm_re_group(&m, 0));
    for (size_t k = 0; k < runs.n; k++) {
        AvpStr r = runs.v[k];
        shsm_re_iter_init(&it, path_re, r, 0, AVP_END);
        while (shsm_re_iter_next(&it, &m)) {
            AvpStr p = shsm_re_group(&m, 0);
            /* `'/' in p or BS in p`; the class has no backslash, and `.replace(BS, '/')` changes nothing */
            if (avp_str_findc(p, '/') >= 0 || avp_str_findc(p, '\\') >= 0) put_unique(seen_p, paths, p);
        }
        shsm_re_iter_init(&it, qual_re, r, 0, AVP_END);
        while (shsm_re_iter_next(&it, &m)) {
            AvpStr q = shsm_re_group(&m, 0);
            AvpStrVec parts = avp_str_split(q, avp_str("::"), -1);
            bool ok = true; /* `not any(x.isdigit())` holds: every part starts with a letter, `_` or `~` */
            for (size_t j = 0; j < parts.n; j++)
                if (parts.v[j].n < 3) ok = false;
            avp_vec_free(&parts);
            if (ok) put_unique(seen_q, quals, q);
        }
    }
    for (size_t k = 0; k < runs.n; k++) {
        AvpStr t = runs.v[k];
        if (shsm_re_fullmatch(cname_re, t, 0, AVP_END, &m) && shsm_re_search(lower3_re, t, 0, AVP_END, &m))
            put_unique(seen_c, cnames, t);
    }
    avp_vec_free(&runs);
    avp_map_free(seen_p);
    avp_map_free(seen_q);
    avp_map_free(seen_c);
}

/* ---- classes ------------------------------------------------------------------------------------------------ */
typedef struct {
    AvpStr q, m;
} Pair;

static int cmp_str(const void *x, const void *y) { return avp_str_cmp(*(const AvpStr *)x, *(const AvpStr *)y); }
static int cmp_pair(const void *x, const void *y) {
    const Pair *p = x, *q = y;
    int c = avp_str_cmp(p->q, q->q);
    return c ? c : avp_str_cmp(p->m, q->m);
}

/* build_classes: {'A::B': {members}}, as sorted, de-duplicated pairs */
static size_t build_classes(const AvpStrVec *names, Pair **out) {
    Pair *v = avp_xcalloc(names->n + 1, sizeof(Pair));
    size_t n = 0;
    for (size_t k = 0; k < names->n; k++) {
        AvpStr q = names->v[k];
        i64 at = avp_str_rfind(q, avp_str("::"), 0, (i64)q.n);
        if (at < 0) continue; /* fewer than two parts */
        v[n].q = avp_strn(q.p, (size_t)at);
        v[n].m = avp_strn(q.p + at + 2, q.n - (size_t)at - 2);
        n++;
    }
    qsort(v, n, sizeof(Pair), cmp_pair);
    size_t w = 0;
    for (size_t k = 0; k < n; k++)
        if (w == 0 || cmp_pair(&v[w - 1], &v[k]) != 0) v[w++] = v[k];
    *out = v;
    return w;
}

/* ---- output -------------------------------------------------------------------------------------------------- */
/* a latin-1 text as UTF-8 */
static AvpStr u8s(AvpArena *a, AvpStr latin1) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < latin1.n; i++) avp_buf_putcp(&b, (u8)latin1.p[i]);
    return avp_buf_str(&b);
}

static void write_lines(AvpArena *a, const char *dir, const char *name, AvpBuf *b) {
    const char *path = avp_join(a, dir, name);
    if (!avp_write_text(path, avp_buf_str(b), AVP_TEXT(AVP_ENC_MBCS, AVP_ERR_STRICT, AVP_NL_DEFAULT)))
        TRACEBACK("OSError", path);
}

/* self_test(): the number of mismatches, each printed */
static int self_test(AvpArena *a) {
    static const char *cases[][2] = {
        {"ProcessStream__14CStreamHandlerPC7RwChunk", "CStreamHandler::ProcessStream(RwChunk const *)"},
        {"__14CStreamHandler", "CStreamHandler::CStreamHandler(void)"},
        {"_$_14CStreamHandler", "CStreamHandler::~CStreamHandler()"},
        {"foo__Fiif", "foo(int, int, float)"},
        {"Get__Q23RWS7CEntity", "RWS::CEntity::Get(void)"},
        {"Run__C6CThing", "CThing::Run(void) const"},
    };
    int bad = 0;
    for (size_t k = 0; k < sizeof cases / sizeof *cases; k++) {
        AvpStr r;
        bool ok = elf_demangle_v2(a, avp_str(cases[k][0]), &r);
        if (!ok || !avp_str_eqz(r, cases[k][1])) {
            if (ok) printf("self-test MISMATCH %s -> '%s' (want '%s')\n", cases[k][0], avp_cz(a, r), cases[k][1]);
            else printf("self-test MISMATCH %s -> None (want '%s')\n", cases[k][0], cases[k][1]);
            bad++;
        }
    }
    return bad;
}

int elf_symbols_main(int argc, char **argv) {
    if (argc != 3) {
        fputs(USAGE, stdout);
        return 1;
    }
    AvpArena *a = avp_arena();
    int bad = self_test(a);
    Elf elf = elf_open(a, argv[1]);
    const char *out = argv[2];
    if (!avp_makedirs(out, true)) TRACEBACK("OSError", out);

    /* sections / debug report */
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, "# idx name type flags addr offset size link info entsize\n");
    for (size_t i = 0; i < elf.nsec; i++) {
        const Section *s = &elf.sec[i];
        avp_buf_printf(&b, "%u %s 0x%x 0x%x 0x%08x 0x%x 0x%x %u %u %u\n", s->idx,
                       s->name.n ? avp_cz(a, u8s(a, s->name)) : "-", s->type, s->flags, s->addr, s->off, s->size,
                       s->link, s->info, s->entsz);
    }
    AvpStrVec dbg = {0};
    for (size_t i = 0; i < elf.nsec; i++) {
        AvpStr n = elf.sec[i].name;
        if (avp_str_eqz(n, ".mdebug") || avp_str_eqz(n, ".line") || avp_str_eqz(n, ".stab") ||
            avp_str_eqz(n, ".stabstr") || avp_str_eqz(n, ".pdr") || avp_str_startswith(n, avp_str(".debug")) ||
            avp_str_startswith(n, avp_str(".mdebug")))
            avp_vec_push(&dbg, n);
    }
    avp_buf_putz(&b, "# debug-info sections present: ");
    if (dbg.n) avp_buf_put(&b, u8s(a, avp_str_join(a, avp_str(", "), dbg.v, dbg.n)));
    else avp_buf_putz(&b, "NONE");
    avp_buf_putc(&b, '\n');
    bool has_symtab = elf_section(&elf, ".symtab") != NULL;
    avp_buf_printf(&b, "# .symtab present: %s\n", has_symtab ? "True" : "False");
    write_lines(a, out, "sections.txt", &b);

    SymVec syms = elf_symbols(a, &elf);
    AvpStrVec names = {0};
    for (size_t i = 0; i < syms.n; i++)
        if (syms.v[i].name.n) avp_vec_push(&names, syms.v[i].name);
    const char *scheme = elf_detect_scheme(names.v, names.n);
    bool v2 = strcmp(scheme, "gnu-v2") == 0;
    size_t dem_ok = 0, nfunc = 0, nobj = 0;
    AvpStrVec qual_from_syms = {0}, files = {0};
    avp_buf_init(&b, a);
    avp_buf_putz(&b, "# address size type bind section raw_name demangled_name\n");
    avp_buf_printf(&b, "# .symtab present: %s; symbols: %zu; mangling scheme: %s\n", has_symtab ? "True" : "False", syms.n,
                   scheme);
    for (size_t i = 0; i < syms.n; i++) {
        const Sym *s = &syms.v[i];
        if (strcmp(s->typ, "FILE") == 0) avp_vec_push(&files, s->name);
        AvpStr dem = avp_str("-");
        if (v2 && s->name.n) {
            AvpStr r;
            if (elf_demangle_v2(a, s->name, &r) && r.n) {
                dem = r;
                dem_ok++;
                /* re.sub(r'\(.*$', '', r) */
                i64 par = avp_str_findc(r, '(');
                avp_vec_push(&qual_from_syms, par >= 0 ? avp_strn(r.p, (size_t)par) : r);
            }
        }
        nfunc += strcmp(s->typ, "FUNC") == 0;
        nobj += strcmp(s->typ, "OBJECT") == 0;
        avp_buf_printf(&b, "0x%08x %u %s %s %s %s %s\n", s->val, s->size, s->typ, s->bind, avp_cz(a, u8s(a, s->sec)),
                       s->name.n ? avp_cz(a, u8s(a, s->name)) : "-", avp_cz(a, u8s(a, dem)));
    }
    write_lines(a, out, "symbols.txt", &b);

    /* source files */
    AvpStrVec paths = {0}, quals = {0}, cnames = {0};
    elf_mine_strings(a, elf.d, &paths, &quals, &cnames);
    avp_buf_init(&b, a);
    if (files.n) {
        avp_buf_putz(&b, "# STT_FILE entries from .symtab\n");
        for (size_t i = 0; i < files.n; i++) avp_buf_printf(&b, "%s\n", avp_cz(a, u8s(a, files.v[i])));
    }
    if (!has_symtab) {
        avp_buf_putz(&b, "# no .symtab: source paths below are string literals found in the image (heuristic)\n");
        for (size_t i = 0; i < paths.n; i++) avp_buf_printf(&b, "%s\n", avp_cz(a, u8s(a, paths.v[i])));
    }
    if (b.n == 0) avp_buf_putc(&b, '\n'); /* '\n'.join([]) + '\n' */
    write_lines(a, out, "source_files.txt", &b);

    /* classes */
    Pair *pairs;
    size_t npairs;
    const char *src;
    if (qual_from_syms.n) {
        src = "demangled .symtab";
        npairs = build_classes(&qual_from_syms, &pairs);
    } else {
        src = "strings of the form A::B::member found in the image (heuristic; ELF is stripped)";
        npairs = build_classes(&quals, &pairs);
    }
    avp_buf_init(&b, a);
    avp_buf_printf(&b, "# source: %s\n# qualifier<TAB>member_count<TAB>members(up to 8)\n", src);
    size_t nclasses = 0;
    for (size_t i = 0; i < npairs;) {
        size_t j = i;
        while (j < npairs && avp_str_eq(pairs[j].q, pairs[i].q)) j++;
        avp_buf_printf(&b, "%s\t%zu\t", avp_cz(a, u8s(a, pairs[i].q)), j - i);
        for (size_t k = i; k < j && k < i + 8; k++) {
            if (k > i) avp_buf_putc(&b, ',');
            avp_buf_put(&b, u8s(a, pairs[k].m));
        }
        avp_buf_putc(&b, '\n');
        nclasses++;
        i = j;
    }
    if (!qual_from_syms.n) {
        avp_buf_printf(&b, "# --- class-name-like literals (C[A-Z]... exact strings; likely class factory / reflection names), %zu ---\n",
                       cnames.n);
        qsort(cnames.v, cnames.n, sizeof(AvpStr), cmp_str);
        for (size_t i = 0; i < cnames.n; i++) avp_buf_printf(&b, "%s\n", avp_cz(a, u8s(a, cnames.v[i])));
    }
    write_lines(a, out, "classes.txt", &b);

    printf("sections: %zu; .symtab: %s; debug sections: ", elf.nsec, has_symtab ? "True" : "False");
    if (dbg.n) {
        printf("[");
        for (size_t i = 0; i < dbg.n; i++) printf("%s'%s'", i ? ", " : "", avp_cz(a, u8s(a, dbg.v[i])));
        printf("]\n");
    } else {
        printf("none\n");
    }
    printf("symbols: %zu (FUNC %zu, OBJECT %zu); scheme: %s; demangled: %zu; STT_FILE: %zu\n", syms.n, nfunc, nobj, scheme,
           dem_ok, files.n);
    printf("string-mined source paths: %zu; qualified names: %zu; qualifiers: %zu\n", paths.n, quals.n, nclasses);
    if (bad) printf("WARNING: %d demangler self-test failures\n", bad);
    return 0;
}
