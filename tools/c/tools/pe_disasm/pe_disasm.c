/* pe_disasm: the logic of tools/mwcc_trace/pe_disasm.py. See pe_disasm.h. */
#include "pe_disasm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shsm_re.h"

const char pe_disasm_doc[] =
    "Tiny static PE (Win32 x86) explorer for mwcceppc.exe: strings, xrefs, per-function disassembly, call graph.\n"
    "\n"
    "Usage (all VAs hex, e.g. 0x4a1b20):\n"
    "  pe_disasm.exe strings <exe> <regex>        ASCII strings matching regex: VA and text\n"
    "  pe_disasm.exe xref    <exe> <va>           code sites whose operands (imm32/disp32) equal VA, with containing "
    "function start\n"
    "  pe_disasm.exe func    <exe> <va> [--max N] linear disassembly of one function (extent found by following "
    "jumps);\n"
    "                                             call targets and string immediates are annotated\n"
    "  pe_disasm.exe callers <exe> <va>           direct E8 call sites targeting VA (with containing function)\n"
    "  pe_disasm.exe callees <exe> <va>           direct E8 call targets inside the function at VA (with counts)\n"
    "\n"
    "Function names come from the compiler's own NB11 CodeView info (mwcc_symbols.exe) when the exe has it;\n"
    "otherwise (and for unnamed statics) functions print as func_<va>.\n"
    "\n"
    "The first query linearly disassembles .text once and caches calls/refs/function starts in\n"
    "build/tmp/pe/<exe>.idx.bin (rebuilt if the exe's mtime changes).\n"
    "Function starts are heuristic: `push ebp; mov ebp,esp` prologues, 16-aligned code after ret/jmp/padding,\n"
    "and every direct call target.\n";

/* ---- PE ---------------------------------------------------------------------------------------------------------- */

PeDisPe *pe_disasm_parse(AvpArena *a, const u8 *d, size_t n) {
    ShsmPeHeaders h;
    shsm_pe_headers(a, d, n, SHSM_PE_DISASM, &h);
    PeDisPe *pe = AVP_NEW(a, PeDisPe);
    pe->data = d;
    pe->n = n;
    pe->base = h.base;
    pe->nsecs = h.nsecs;
    pe->secs = AVP_NEWN(a, PeDisSec, h.nsecs ? h.nsecs : 1);
    for (size_t i = 0; i < h.nsecs; i++) {
        const ShsmPeSection *s = &h.secs[i];
        PeDisSec *o = &pe->secs[i];
        memcpy(o->name, s->name, sizeof o->name);
        o->va = (i64)h.base + s->va;
        o->size = s->vsize ? (s->vsize < s->rsize ? s->vsize : s->rsize) : s->rsize;
        o->raw = s->raw;
        o->ch = s->ch;
    }
    if (!h.nsecs) avp_throw(AVP_ERR_OTHER, "ValueError: min() iterable argument is empty");
    pe->lo = pe->secs[0].va;
    pe->hi = pe->secs[0].va + pe->secs[0].size;
    for (size_t i = 1; i < pe->nsecs; i++) {
        if (pe->secs[i].va < pe->lo) pe->lo = pe->secs[i].va;
        if (pe->secs[i].va + pe->secs[i].size > pe->hi) pe->hi = pe->secs[i].va + pe->secs[i].size;
    }
    return pe;
}

PeDisPe *pe_disasm_open(AvpArena *a, const char *path) {
    AvpBytes b = {0};
    if (!avp_read_all(a, path, &b)) {
        AvpStr msg = avp_io_err_str(a);
        avp_throw(AVP_ERR_IO, "%s: %.*s", avp_io_err_class(), AVP_SF(msg));
    }
    return pe_disasm_parse(a, b.v, b.n);
}

bool pe_disasm_off(const PeDisPe *pe, i64 va, u64 *off) {
    for (size_t i = 0; i < pe->nsecs; i++) {
        const PeDisSec *s = &pe->secs[i];
        if (s->va <= va && va < s->va + s->size) {
            *off = (u64)s->raw + (u64)(va - s->va);
            return true;
        }
    }
    return false;
}

bool pe_disasm_read(const PeDisPe *pe, i64 va, size_t n, AvpStr *out) {
    u64 o;
    if (!pe_disasm_off(pe, va, &o)) return false;
    if (o >= pe->n) {
        *out = (AvpStr){(const char *)pe->data, 0};
        return true;
    }
    size_t k = o + n > pe->n ? (size_t)(pe->n - o) : n;
    *out = (AvpStr){(const char *)pe->data + o, k};
    return true;
}

AvpStr pe_disasm_cstr(const PeDisPe *pe, i64 va) {
    AvpStr b;
    if (!pe_disasm_read(pe, va, 96, &b) || !b.n) return (AvpStr){0};
    const char *z = memchr(b.p, 0, b.n);
    if (!z || z - b.p < 4) return (AvpStr){0};
    AvpStr s = {b.p, (size_t)(z - b.p)};
    for (size_t i = 0; i < s.n; i++) {
        u8 c = (u8)s.p[i];
        if (!((c >= 0x20 && c < 0x7F) || c == 9 || c == 10)) return (AvpStr){0};
    }
    return s;
}

/* ---- capstone ---------------------------------------------------------------------------------------------------- */

csh pe_disasm_md(void) {
    csh h;
    if (cs_open(CS_ARCH_X86, CS_MODE_32, &h) != CS_ERR_OK) avp_fatal("capstone: cs_open failed");
    cs_option(h, CS_OPT_DETAIL, CS_OPT_ON);
    return h;
}

static bool is_call_imm(const cs_insn *ins) {
    return ins->id == X86_INS_CALL && ins->detail->x86.op_count > 0 && ins->detail->x86.operands[0].type == X86_OP_IMM;
}

size_t pe_disasm_operand_values(const cs_insn *ins, i64 out[8]) {
    size_t n = 0;
    const cs_x86 *x = &ins->detail->x86;
    for (unsigned i = 0; i < x->op_count && i < 8; i++) {
        const cs_x86_op *op = &x->operands[i];
        if (op->type == X86_OP_IMM && ins->mnemonic[0] != 'j' && ins->id != X86_INS_CALL)
            out[n++] = (i64)((u64)op->imm & 0xFFFFFFFFu);
        else if (op->type == X86_OP_MEM && op->mem.base != X86_REG_EBP && op->mem.base != X86_REG_ESP)
            out[n++] = (i64)((u64)op->mem.disp & 0xFFFFFFFFu);
    }
    return n;
}

static bool mn_is(const cs_insn *ins, const char *m) { return strcmp(ins->mnemonic, m) == 0; }

/* ---- the index --------------------------------------------------------------------------------------------------- */

static int cmp_i64(const void *x, const void *y, void *ctx) {
    (void)ctx;
    i64 p = *(const i64 *)x, q = *(const i64 *)y;
    return p < q ? -1 : p > q;
}

PeDisIndex *pe_disasm_build_index(AvpArena *a, const PeDisPe *pe, double mtime) {
    csh h = pe_disasm_md();
    cs_insn *ins = cs_malloc(h);
    AVP_VEC(PeDisPair) calls = {0};
    AVP_VEC(PeDisPair) refs = {0};
    AvpI64Vec prologs = {0};
    for (size_t s = 0; s < pe->nsecs; s++) {
        const PeDisSec *sec = &pe->secs[s];
        if (!(sec->ch & 0x20000000)) continue;
        /* code = data[raw:raw + sz] */
        size_t lo = sec->raw < pe->n ? sec->raw : pe->n;
        size_t hi = (u64)sec->raw + (u64)(sec->size > 0 ? sec->size : 0) < pe->n ? (size_t)(sec->raw + sec->size) : pe->n;
        const u8 *code = pe->data + lo;
        size_t len = hi > lo ? hi - lo : 0;
        size_t pos = 0;
        while (pos < len) {
            size_t end = pos;
            bool term = true; /* the previous instruction ended a function or block (ret/jmp/padding) */
            const u8 *p = code + pos;
            size_t left = len - pos;
            u64 addr = (u64)(sec->va + (i64)pos);
            while (cs_disasm_iter(h, &p, &left, &addr, ins)) {
                i64 at = (i64)ins->address;
                if (term && at % 16 == 0 && !mn_is(ins, "int3") && !mn_is(ins, "nop")) avp_vec_push(&prologs, at);
                term = mn_is(ins, "ret") || mn_is(ins, "retn") || mn_is(ins, "jmp") || mn_is(ins, "int3") ||
                       mn_is(ins, "nop");
                end = (size_t)(at + ins->size - sec->va);
                /* ins.bytes[:3] == b"\x55\x8b\xec": as the Python, never true (push ebp is a one-byte instruction) */
                if (ins->size >= 3 && memcmp(ins->bytes, "\x55\x8b\xec", 3) == 0) avp_vec_push(&prologs, at);
                if (is_call_imm(ins))
                    avp_vec_push(&calls, ((PeDisPair){at, (i64)((u64)ins->detail->x86.operands[0].imm & 0xFFFFFFFFu)}));
                i64 vals[8];
                size_t nv = pe_disasm_operand_values(ins, vals);
                for (size_t k = 0; k < nv; k++)
                    if (pe->lo <= vals[k] && vals[k] < pe->hi) avp_vec_push(&refs, ((PeDisPair){at, vals[k]}));
            }
            pos = end + 1; /* skip one undecodable byte and resync */
        }
    }
    cs_free(ins, 1);
    cs_close(&h);
    for (size_t i = 0; i < calls.n; i++)
        if (pe->lo <= calls.v[i].v && calls.v[i].v < pe->hi) avp_vec_push(&prologs, calls.v[i].v);
    avp_sort(prologs.v, prologs.n, sizeof *prologs.v, cmp_i64, NULL, false);
    PeDisIndex *idx = AVP_NEW(a, PeDisIndex);
    idx->mtime = mtime;
    idx->starts = AVP_NEWN(a, i64, prologs.n ? prologs.n : 1);
    for (size_t i = 0; i < prologs.n; i++)
        if (!idx->nstarts || idx->starts[idx->nstarts - 1] != prologs.v[i]) idx->starts[idx->nstarts++] = prologs.v[i];
    idx->calls = AVP_NEWN(a, PeDisPair, calls.n ? calls.n : 1);
    if (calls.n) memcpy(idx->calls, calls.v, calls.n * sizeof *calls.v);
    idx->ncalls = calls.n;
    idx->refs = AVP_NEWN(a, PeDisPair, refs.n ? refs.n : 1);
    if (refs.n) memcpy(idx->refs, refs.v, refs.n * sizeof *refs.v);
    idx->nrefs = refs.n;
    avp_vec_free(&calls);
    avp_vec_free(&refs);
    avp_vec_free(&prologs);
    return idx;
}

#define IDX_KIND "pe_disasm_idx"
#define IDX_SCHEMA 1u

static void put_pairs(AvpCacheWriter *w, const PeDisPair *v, size_t n) {
    avp_cw_count(w, n);
    for (size_t i = 0; i < n; i++) {
        avp_cw_i64(w, v[i].a);
        avp_cw_i64(w, v[i].v);
    }
}

static PeDisPair *get_pairs(AvpArena *a, AvpCacheReader *r, size_t *n) {
    *n = avp_cr_count(r, 16);
    PeDisPair *v = AVP_NEWN(a, PeDisPair, *n ? *n : 1);
    for (size_t i = 0; i < *n; i++) {
        v[i].a = avp_cr_i64(r);
        v[i].v = avp_cr_i64(r);
    }
    return v;
}

PeDisIndex *pe_disasm_index(AvpArena *a, const PeDisPe *pe, const char *path, const char *cache_dir) {
    double mt = 0;
    if (!avp_getmtime(path, &mt)) {
        AvpStr msg = avp_io_err_str(a);
        avp_throw(AVP_ERR_IO, "%s: %.*s", avp_io_err_class(), AVP_SF(msg));
    }
    char *cp = NULL;
    if (cache_dir) {
        AvpStr base = avp_path_basename(path);
        cp = avp_path_join(a, cache_dir, avp_sprintf(a, "%.*s.idx.bin", AVP_SF(base)));
        AvpCache c;
        if (avp_cache_open(&c, cp, IDX_KIND, IDX_SCHEMA, &mt, sizeof mt)) {
            PeDisIndex *idx = AVP_NEW(a, PeDisIndex);
            idx->mtime = mt;
            idx->calls = get_pairs(a, &c.r, &idx->ncalls);
            idx->refs = get_pairs(a, &c.r, &idx->nrefs);
            idx->nstarts = avp_cr_count(&c.r, 8);
            idx->starts = AVP_NEWN(a, i64, idx->nstarts ? idx->nstarts : 1);
            for (size_t i = 0; i < idx->nstarts; i++) idx->starts[i] = avp_cr_i64(&c.r);
            bool ok = avp_cr_ok(&c.r) && !avp_cr_remaining(&c.r);
            avp_cache_close(&c);
            if (ok) return idx;
        }
    }
    PeDisIndex *idx = pe_disasm_build_index(a, pe, mt);
    if (cp) {
        AvpCacheWriter w;
        avp_cw_init(&w);
        put_pairs(&w, idx->calls, idx->ncalls);
        put_pairs(&w, idx->refs, idx->nrefs);
        avp_cw_count(&w, idx->nstarts);
        for (size_t i = 0; i < idx->nstarts; i++) avp_cw_i64(&w, idx->starts[i]);
        if (avp_makedirs(cache_dir, true)) avp_cache_write_w(cp, IDX_KIND, IDX_SCHEMA, &mt, sizeof mt, &w);
        avp_cw_free(&w);
    }
    return idx;
}

bool pe_disasm_func_of(const PeDisIndex *idx, i64 va, i64 *start) {
    size_t lo = 0, hi = idx->nstarts;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (va < idx->starts[mid]) hi = mid;
        else lo = mid + 1;
    }
    if (!lo) return false;
    *start = idx->starts[lo - 1];
    return true;
}

/* ---- func_extent ------------------------------------------------------------------------------------------------- */

PeDisInsns pe_disasm_func_extent(csh h, const PeDisPe *pe, i64 start, i64 maxn) {
    PeDisInsns out = {0};
    i64 maxj = start, addr = start;
    while ((i64)out.n < maxn) {
        AvpStr b;
        if (!pe_disasm_read(pe, addr, 16, &b) || !b.n) break;
        cs_insn *ins = cs_malloc(h);
        const u8 *p = (const u8 *)b.p;
        size_t left = b.n;
        u64 at = (u64)addr;
        if (!cs_disasm_iter(h, &p, &left, &at, ins)) {
            cs_free(ins, 1);
            avp_vec_push(&out, (cs_insn *)NULL);
            addr += 1;
            continue;
        }
        avp_vec_push(&out, ins);
        addr += ins->size;
        const cs_x86 *x = &ins->detail->x86;
        bool op0_imm = x->op_count > 0 && x->operands[0].type == X86_OP_IMM;
        if (ins->mnemonic[0] == 'j' && op0_imm) {
            i64 t = (i64)((u64)x->operands[0].imm & 0xFFFFFFFFu);
            if (t > addr && t - start < 0x8000 && t > maxj) maxj = t;
        }
        if ((mn_is(ins, "ret") || mn_is(ins, "retn")) && addr > maxj) break;
        if (mn_is(ins, "jmp") && addr > maxj && !op0_imm) break;
    }
    return out;
}

void pe_disasm_free_extent(PeDisInsns *v) {
    for (size_t i = 0; i < v->n; i++)
        if (v->v[i]) cs_free(v->v[i], 1);
    avp_vec_free(v);
}

/* ---- commands ---------------------------------------------------------------------------------------------------- */

void pe_disasm_cmd_strings(AvpArena *a, const PeDisPe *pe, AvpStr rx, AvpBuf *out) {
    ShsmRe *r = shsm_re_try_n(rx, SHSM_RE_BYTES);
    for (size_t s = 0; s < pe->nsecs; s++) {
        const PeDisSec *sec = &pe->secs[s];
        size_t lo = sec->raw < pe->n ? sec->raw : pe->n;
        size_t hi = (u64)sec->raw + (u64)(sec->size > 0 ? sec->size : 0) < pe->n ? (size_t)(sec->raw + sec->size) : pe->n;
        const u8 *d = pe->data;
        /* re.finditer(rb"[\x20-\x7e\t\n]{4,}", chunk) */
        size_t i = lo;
        while (i < hi) {
            if (!((d[i] >= 0x20 && d[i] <= 0x7e) || d[i] == '\t' || d[i] == '\n')) {
                i++;
                continue;
            }
            size_t j = i;
            while (j < hi && ((d[j] >= 0x20 && d[j] <= 0x7e) || d[j] == '\t' || d[j] == '\n')) j++;
            if (j - i >= 4) {
                AvpStr run = {(const char *)d + i, j - i};
                ShsmReMatch m;
                if (shsm_re_search(r, run, 0, AVP_END, &m)) {
                    avp_buf_printf(out, "0x%08llx  ", (unsigned long long)(sec->va + (i64)(i - lo)));
                    for (size_t k = 0; k < run.n; k++) {
                        if (run.p[k] == '\n') avp_buf_put(out, AVP_S("\\n"));
                        else avp_buf_putc(out, run.p[k]);
                    }
                    avp_buf_putc(out, '\n');
                }
            }
            i = j;
        }
    }
    (void)a;
}

/* '%x' % v of a Python int */
static char *hexs(AvpArena *a, i64 v) {
    if (v < 0) return avp_sprintf(a, "-%llx", (unsigned long long)(0 - (u64)v));
    return avp_sprintf(a, "%llx", (unsigned long long)v);
}

AvpStr pe_disasm_fname(AvpArena *a, const ShsmPeSymbols *syms, i64 va) {
    const ShsmPeFunc *f = syms ? shsm_pe_symbols_lookup(syms, va) : NULL;
    if (f && f->va == va) return f->name;
    return avp_str(avp_sprintf(a, "func_%s", hexs(a, va)));
}

static void annotate(AvpArena *a, const PeDisPe *pe, const cs_insn *ins, AvpBuf *out) {
    i64 vals[8];
    size_t nv = pe_disasm_operand_values(ins, vals), notes = 0;
    for (size_t k = 0; k < nv; k++) {
        if (!(pe->lo <= vals[k] && vals[k] < pe->hi)) continue;
        avp_buf_put(out, notes++ ? AVP_S(", ") : AVP_S("  ; "));
        AvpStr s = pe_disasm_cstr(pe, vals[k]);
        if (s.p) avp_buf_printf(out, "\"%.*s\"", AVP_SF(s));
        else avp_buf_printf(out, "data 0x%s", hexs(a, vals[k]));
    }
}

/* ---- main -------------------------------------------------------------------------------------------------------- */

static void flush_buf(AvpBuf *b) {
    AvpStr s = avp_buf_str(b);
    fwrite(s.p, 1, s.n, stdout);
    avp_buf_clear(b);
}

/* An unexpected internal error: one line on stderr, exit 1. */
static int traceback(const char *text) {
    fflush(stdout);
    fprintf(stderr, "pe_disasm.exe: error: %s\n", text);
    return 1;
}

static int usage(void) {
    fputs(pe_disasm_doc, stdout);
    fputc('\n', stdout);
    return 1;
}

static const ShsmPeSymbols *load_symbols(AvpArena *a, const char *exe) {
    AvpTry t;
    const ShsmPeSymbols *s = NULL;
    if (AVP_TRY(&t)) {
        s = shsm_pe_symbols_load(a, exe);
        avp_try_end(&t);
    } else {
        s = NULL;
    }
    return s;
}

static int run_cmd(AvpArena *a, int argc, char **argv, AvpBuf *out) {
    const char *cmd = argv[1], *exe = argv[2], *arg = argv[3];
    PeDisPe *pe = pe_disasm_open(a, exe);
    if (strcmp(cmd, "strings") == 0) {
        pe_disasm_cmd_strings(a, pe, avp_str(arg), out);
        return 0;
    }
    const char *root = avp_path_abspath(a, avp_root());
    PeDisIndex *idx = pe_disasm_index(a, pe, exe, avp_join(a, root, "build", "tmp", "pe"));
    i64 va;
    bool overflow = false;
    if (!avp_parse_int(avp_str(arg), 16, &va, &overflow))
        avp_throw(AVP_ERR_OTHER, "%s",
                  overflow ? "OverflowError: value beyond 64 bits"
                           : avp_sprintf(a, "ValueError: invalid literal for int() with base 16: %s",
                                         avp_repr_str(a, avp_str(arg)).p));
    const ShsmPeSymbols *syms = load_symbols(a, exe);
    if (strcmp(cmd, "xref") == 0 || strcmp(cmd, "callers") == 0) {
        bool xref = cmd[0] == 'x';
        const PeDisPair *v = xref ? idx->refs : idx->calls;
        size_t n = xref ? idx->nrefs : idx->ncalls;
        for (size_t i = 0; i < n; i++) {
            if (v[i].v != va) continue;
            i64 f = 0;
            if (!pe_disasm_func_of(idx, v[i].a, &f)) f = 0;
            avp_buf_printf(out, "0x%08llx  in %.*s\n", (unsigned long long)v[i].a, AVP_SF(pe_disasm_fname(a, syms, f)));
        }
    } else if (strcmp(cmd, "func") == 0) {
        i64 mx = 4000;
        for (int i = 0; i < argc; i++) {
            if (strcmp(argv[i], "--max") != 0) continue;
            if (i + 1 >= argc) avp_throw(AVP_ERR_OTHER, "IndexError: list index out of range");
            if (!avp_parse_int(avp_str(argv[i + 1]), 10, &mx, &overflow))
                avp_throw(AVP_ERR_OTHER, "%s",
                          overflow ? "OverflowError: value beyond 64 bits"
                                   : avp_sprintf(a, "ValueError: invalid literal for int() with base 10: %s",
                                                 avp_repr_str(a, avp_str(argv[i + 1])).p));
            break;
        }
        csh h = pe_disasm_md();
        PeDisInsns ins = pe_disasm_func_extent(h, pe, va, mx);
        for (size_t i = 0; i < ins.n; i++) {
            const cs_insn *x = ins.v[i];
            if (!x) {
                avp_buf_put(out, AVP_S("  ?? (bad byte)\n"));
                continue;
            }
            avp_buf_printf(out, "%08llx  %-6s %s", (unsigned long long)x->address, x->mnemonic, x->op_str);
            annotate(a, pe, x, out);
            if (is_call_imm(x))
                avp_buf_printf(out, "  ; call %.*s",
                               AVP_SF(pe_disasm_fname(a, syms, (i64)((u64)x->detail->x86.operands[0].imm & 0xFFFFFFFFu))));
            avp_buf_putc(out, '\n');
        }
        pe_disasm_free_extent(&ins);
        cs_close(&h);
    } else if (strcmp(cmd, "callees") == 0) {
        csh h = pe_disasm_md();
        PeDisInsns ins = pe_disasm_func_extent(h, pe, va, 4000);
        const cs_insn *first = NULL, *last = NULL;
        for (size_t i = 0; i < ins.n; i++)
            if (ins.v[i]) {
                if (!first) first = ins.v[i];
                last = ins.v[i];
            }
        if (!first) {
            pe_disasm_free_extent(&ins);
            cs_close(&h);
            avp_throw(AVP_ERR_OTHER, "IndexError: list index out of range");
        }
        i64 lo = (i64)first->address, hi = (i64)last->address + last->size;
        pe_disasm_free_extent(&ins);
        cs_close(&h);
        AvpIMap *cnt = avp_imap_new(); /* target -> count; printed sorted by target */
        for (size_t i = 0; i < idx->ncalls; i++)
            if (lo <= idx->calls[i].a && idx->calls[i].a < hi) avp_imap_put(cnt, (u64)idx->calls[i].v, NULL)->i += 1;
        AvpI64Vec ts = {0};
        AvpIMapIter it = {0};
        while (avp_imap_next(cnt, &it)) avp_vec_push(&ts, (i64)it.key);
        avp_sort(ts.v, ts.n, sizeof *ts.v, cmp_i64, NULL, false);
        for (size_t i = 0; i < ts.n; i++) {
            i64 c = avp_imap_get(cnt, (u64)ts.v[i])->i;
            avp_buf_printf(out, "0x%08llx  x%lld", (unsigned long long)ts.v[i], (long long)c);
            AvpStr nm = pe_disasm_fname(a, syms, ts.v[i]);
            if (syms && !avp_str_startswith(nm, AVP_S("func_"))) avp_buf_printf(out, "  %.*s", AVP_SF(nm));
            avp_buf_putc(out, '\n');
        }
        avp_vec_free(&ts);
        avp_imap_free(cnt);
    } else {
        flush_buf(out);
        return usage();
    }
    return 0;
}

int pe_disasm_main(int argc, char **argv) {
    if (argc < 4) return usage();
    AvpArena *a = avp_arena();
    AvpBuf out;
    avp_buf_init(&out, a);
    AvpTry t;
    int rc;
    if (AVP_TRY(&t)) {
        rc = run_cmd(a, argc, argv, &out);
        avp_try_end(&t);
    } else {
        flush_buf(&out);
        if (t.code == AVP_ERR_REGEX) return traceback(avp_sprintf(a, "re.PatternError: %s", t.msg));
        if (t.code == SHSM_PE_EXC_STRUCT || t.code == SHSM_PE_EXC_INDEX)
            return traceback(avp_sprintf(a, "%s: %s", shsm_pe_exc_class(t.code), t.msg));
        return traceback(t.msg);
    }
    flush_buf(&out);
    return rc;
}
