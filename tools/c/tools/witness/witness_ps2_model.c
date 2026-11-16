/* The PS2 side of witness.py's model: decode_text, find_gp, _jump_table, recover_functions, analyse_function,
 * _const, data_runs, build_ps2. */
#include "witness_ps2.h"

#include "shsm_cli.h"

#include <stdlib.h>
#include <string.h>

/* decode_text(elf): {address: Insn} over the text sections. Stored per text section (in elf.sections order); a
 * lookup takes the last section that decoded the address, as the later dict assignment would. */
typedef struct Code {
    const WtSection *s;
    WtInsn *insns;
    size_t n;
} Code;
typedef struct Text {
    Code *v;
    size_t n;
    i64 words; /* len(insns) */
} Text;

static const WtInsn *insn_at(const Text *t, i64 addr) {
    for (size_t k = t->n; k-- > 0;) {
        const Code *c = &t->v[k];
        i64 d = addr - c->s->addr;
        if (d >= 0 && !(d & 3) && (u64)(d >> 2) < c->n) return &c->insns[d >> 2];
    }
    return NULL;
}

static const WtInsn *insn_req(const Text *t, i64 addr) {
    const WtInsn *i = insn_at(t, addr);
    if (!i) shsm_raise(SHSM_EXC_KEY, "%lld", (long long)addr); /* insns[a]: KeyError */
    return i;
}

static bool is_kind(const WtSection *s, const char *k) { return strcmp(s->kind, k) == 0; }

static void decode_text(const WtElf *e, Text *t) {
    memset(t, 0, sizeof *t);
    t->v = avp_xcalloc(e->sections.n + 1, sizeof(Code));
    bool overlap = false;
    i64 prev_end = INT64_MIN;
    for (size_t k = 0; k < e->sections.n; k++) {
        const WtSection *s = &e->sections.v[k];
        if (!is_kind(s, "text")) continue;
        Code *c = &t->v[t->n++];
        c->s = s;
        c->n = s->size >= 4 ? (size_t)((s->size - 4) / 4 + 1) : 0; /* range(0, size - 3, 4) */
        c->insns = avp_xmalloc((c->n ? c->n : 1) * sizeof(WtInsn));
        if (c->n && (s->offset < 0 || (u64)s->offset + 4 * (c->n - 1) + 4 > e->size))
            shsm_raise(SHSM_EXC_VALUE, "unpack_from: section %s: offset %lld is outside the file", s->name,
                       (long long)(s->offset + 4 * (i64)c->n - 4));
        const u8 *d = e->data + s->offset;
        for (size_t j = 0; j < c->n; j++) {
            const u8 *p = d + 4 * j;
            u32 w = (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;
            wt_decode(w, (u32)(s->addr + 4 * (i64)j), &c->insns[j]);
        }
        if (c->n && s->addr < prev_end) overlap = true;
        i64 end = s->addr + 4 * (i64)c->n;
        if (c->n && end > prev_end) prev_end = end;
        t->words += (i64)c->n;
    }
    if (overlap) { /* distinct addresses only */
        AvpISet *seen = avp_iset_new();
        t->words = 0;
        for (size_t k = 0; k < t->n; k++)
            for (size_t j = 0; j < t->v[k].n; j++)
                if (avp_iset_add(seen, (u64)(t->v[k].s->addr + 4 * (i64)j))) t->words++;
        avp_iset_free(seen);
    }
}

/* A register file standing for a dict {reg: value} (only membership and values are used). */
typedef struct Regs {
    bool has[32];
    u32 val[32];
} Regs;

static bool mn_in2(const char *mn, const char *a, const char *b) { return strcmp(mn, a) == 0 || strcmp(mn, b) == 0; }

/* find_gp(elf, insns) */
static bool find_gp(const WtElf *e, const Text *t, u32 *gp) {
    Regs v = {0};
    for (int k = 0; k < 512; k++) {
        const WtInsn *i = insn_at(t, e->entry + 4 * (i64)k);
        if (!i) break;
        if (i->kind == WT_K_LUI) {
            v.has[i->rt] = true;
            v.val[i->rt] = (u32)i->imm << 16;
        } else if (i->kind == WT_K_IMM && v.has[i->rs] &&
                   (mn_in2(i->mn, "addiu", "daddiu") || strcmp(i->mn, "ori") == 0)) {
            u32 x = strcmp(i->mn, "ori") == 0 ? (v.val[i->rs] | (u32)i->imm) : v.val[i->rs] + (u32)i->imm;
            v.has[i->rt] = true;
            v.val[i->rt] = x;
        } else if (strcmp(i->mn, "move") == 0 && v.has[i->rs]) {
            v.has[i->rd] = true;
            v.val[i->rd] = v.val[i->rs];
        } else if (i->dst >= 0) {
            v.has[i->dst] = false;
        }
        if (v.has[WT_GP] && (i->dst == WT_GP || (i->rt == WT_GP && (i->kind == WT_K_LUI || i->kind == WT_K_IMM))) &&
            i->kind != WT_K_LUI) {
            *gp = v.val[WT_GP];
            return true;
        }
    }
    return false;
}

/* _jump_table(elf, insns, pc, lo, hi) */
static void jump_table(const WtElf *e, const Text *t, i64 pc, i64 lo, i64 hi, AvpI64Vec *out) {
    Regs regs = {0};
    bool has_table = false;
    i64 table = 0;
    out->n = 0;
    for (i64 a = (lo > pc - 64 ? lo : pc - 64); a < pc; a += 4) {
        const WtInsn *i = insn_at(t, a);
        if (!i) continue;
        if (i->kind == WT_K_LUI) {
            regs.has[i->rt] = true;
            regs.val[i->rt] = (u32)i->imm << 16;
        } else if ((i->kind == WT_K_IMM || i->kind == WT_K_LOAD) && regs.has[i->rs] &&
                   (mn_in2(i->mn, "addiu", "daddiu") || strcmp(i->mn, "lw") == 0)) {
            i64 addr = (i64)(u32)(regs.val[i->rs] + (u32)i->imm);
            const WtSection *s = wt_elf_section_of(e, addr);
            if (s && is_kind(s, "data")) {
                table = addr;
                has_table = true;
            }
        }
    }
    if (!has_table) return;
    for (int k = 0; k < 1024; k++) {
        u32 w;
        if (!wt_elf_word(e, table + 4 * (i64)k, &w) || (w & 3) || !(lo <= (i64)w && (i64)w < hi)) break;
        avp_vec_push(out, (i64)w);
    }
}

typedef struct Span {
    i64 start, end;
} Span;
typedef AVP_VEC(Span) Spans;

static bool in_text(const WtElf *e, i64 a) {
    for (size_t k = 0; k < e->sections.n; k++) {
        const WtSection *s = &e->sections.v[k];
        if (is_kind(s, "text") && s->addr <= a && a < s->addr + s->size) return true;
    }
    return false;
}

/* recover_functions(elf, insns) */
static void recover_functions(const WtElf *e, const Text *t, Spans *fns) {
    AvpISet *seeds = avp_iset_new();
    avp_iset_add(seeds, (u64)e->entry);
    for (size_t k = 0; k < t->n; k++)
        for (size_t j = 0; j < t->v[k].n; j++) {
            const WtInsn *i = &t->v[k].insns[j];
            if (i->kind == WT_K_CALL && insn_at(t, i->pc) == i && in_text(e, i->target))
                avp_iset_add(seeds, i->target);
        }
    for (size_t k = 0; k < e->sections.n; k++) {
        const WtSection *s = &e->sections.v[k];
        if (!is_kind(s, "data")) continue;
        for (i64 o = 0; o < s->size - 3; o += 4) {
            if (s->offset + o < 0 || (u64)(s->offset + o) + 4 > e->size)
                shsm_raise(SHSM_EXC_VALUE, "unpack_from: section %s: offset %lld is outside the file", s->name,
                           (long long)(s->offset + o));
            const u8 *p = e->data + s->offset + o;
            u32 w = (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;
            if (w & 3) continue;
            const WtInsn *i = insn_at(t, w);
            if (i && i->kind == WT_K_IMM && mn_in2(i->mn, "addiu", "daddiu") && i->rt == WT_SP && i->rs == WT_SP &&
                i->imm < 0)
                avp_iset_add(seeds, w);
        }
    }
    AvpI64Vec tab = {0};
    for (size_t k = 0; k < t->n; k++) {
        const WtSection *s = t->v[k].s;
        i64 a = s->addr, end_all = s->addr + s->size;
        while (a < end_all) {
            while (a < end_all && insn_req(t, a)->word == 0 && !avp_iset_has(seeds, (u64)a)) a += 4;
            if (a >= end_all) break;
            i64 start = a, maxt = a, p = a, end = end_all;
            while (p < end_all) {
                if (p > start && avp_iset_has(seeds, (u64)p)) {
                    end = p;
                    break;
                }
                const WtInsn *i = insn_req(t, p);
                WtKind kd = (WtKind)i->kind;
                i64 tg = i->target;
                if ((kd == WT_K_BRANCH || kd == WT_K_JUMP) && start <= tg && tg < end_all) {
                    if (tg > maxt && (kd == WT_K_BRANCH || !avp_iset_has(seeds, (u64)tg))) maxt = tg;
                } else if (kd == WT_K_JUMPR) {
                    i64 hi = start + 0x40000 < end_all ? start + 0x40000 : end_all;
                    jump_table(e, t, p, start, hi, &tab);
                    for (size_t m = 0; m < tab.n; m++)
                        if (tab.v[m] > maxt) maxt = tab.v[m];
                }
                bool out_or_seed = tg < start || avp_iset_has(seeds, (u64)tg);
                if (kd == WT_K_RET || (kd == WT_K_JUMP && out_or_seed) ||
                    (kd == WT_K_BRANCH && strcmp(i->mn, "b") == 0 && out_or_seed)) {
                    if (p + 4 >= maxt) {
                        end = p + 8 < end_all ? p + 8 : end_all;
                        break;
                    }
                }
                p += 4;
            }
            avp_vec_push(fns, ((Span){start, end}));
            a = end;
        }
    }
    avp_vec_free(&tab);
    avp_iset_free(seeds);
}

/* _const(f, v) */
static void add_const(WtFn *f, i64 v) {
    v &= 0xFFFFFFFF;
    i64 s = (v & 0x80000000) ? v - ((i64)1 << 32) : v;
    if ((s < 0 ? -s : s) >= 16) avp_pyintset_add(f->ints, v);
}

static bool callee_saved(int reg) { return (reg >= 16 && reg < 24) || reg == WT_GP || reg == WT_SP || reg == WT_FP; }

/* analyse_function(elf, insns, start, end, gp, starts) */
static WtFn *analyse_function(AvpArena *a, const WtElf *e, const Text *t, i64 start, i64 end, bool has_gp, u32 gp,
                              const AvpISet *starts) {
    WtFn *f = wt_fn_new(start);
    f->size = end - start;
    Regs v = {0};
    bool pending = false;
    i64 nonzero = 0;
    for (i64 pc = start; pc < end; pc += 4) {
        const WtInsn *i = insn_req(t, pc);
        WtKind k = (WtKind)i->kind;
        i64 tg = i->target;
        if (k != WT_K_NOP) nonzero++;
        if (k == WT_K_CALL || k == WT_K_CALLR) {
            avp_vec_push(&f->calls, k == WT_K_CALL ? tg : WT_NONE);
            avp_vec_push(&f->sites, pc);
        } else if (k == WT_K_JUMP && avp_iset_has(starts, (u64)tg) && !(start <= tg && tg < end)) {
            avp_vec_push(&f->calls, tg);
            avp_vec_push(&f->sites, pc);
        } else if (k == WT_K_BRANCH && tg <= pc && start <= tg) {
            f->back++;
        }
        if (k == WT_K_IMM && i->rt == WT_SP && i->rs == WT_SP && i->imm < 0 && !f->frame) f->frame = -(i64)i->imm;
        bool has_addr = false;
        i64 addr = 0;
        if (k == WT_K_LUI) {
            v.has[i->rt] = true;
            v.val[i->rt] = (u32)i->imm << 16;
        } else if (k == WT_K_IMM) {
            bool has_v = false;
            i64 val = 0;
            const char *mn = i->mn;
            if (mn_in2(mn, "addiu", "daddiu") || mn_in2(mn, "addi", "daddi")) {
                if (i->rs == 0) {
                    has_v = true;
                    val = (i64)(u32)i->imm;
                } else if (v.has[i->rs]) {
                    has_v = has_addr = true;
                    val = addr = (i64)(u32)(v.val[i->rs] + (u32)i->imm);
                } else if (i->rs == WT_GP && has_gp) {
                    has_addr = true;
                    addr = (i64)(u32)(gp + (u32)i->imm);
                }
            } else if (strcmp(mn, "ori") == 0) {
                if (i->rs == 0) {
                    has_v = true;
                    val = i->imm;
                } else if (v.has[i->rs]) {
                    has_v = has_addr = true;
                    val = addr = (i64)(v.val[i->rs] | (u32)i->imm);
                }
            } else if (mn_in2(mn, "slti", "sltiu")) {
                add_const(f, (i64)i->imm & 0xFFFFFFFF);
            }
            if (has_v && (!has_addr || !wt_elf_section_of(e, addr))) {
                add_const(f, val);
                has_addr = false;
            }
            if (i->dst >= 0) {
                if (has_v) {
                    v.has[i->dst] = true;
                    v.val[i->dst] = (u32)val;
                } else {
                    v.has[i->dst] = false;
                }
            }
        } else if (k == WT_K_LOAD || k == WT_K_STORE) {
            if (v.has[i->rs]) {
                has_addr = true;
                addr = (i64)(u32)(v.val[i->rs] + (u32)i->imm);
            } else if (i->rs == WT_GP && has_gp) {
                has_addr = true;
                addr = (i64)(u32)(gp + (u32)i->imm);
            } else if (i->rs != WT_SP && i->rs != WT_FP && i->rs != WT_GP && 0 <= i->imm && i->imm < 0x10000) {
                avp_pyintset_add(f->offs, i->imm);
            }
            if (has_addr && strcmp(i->mn, "lwc1") == 0) {
                u32 w;
                if (wt_elf_float_at(e, addr, &w)) avp_pyintset_add(f->floats, w);
                has_addr = false;
            }
            if (i->dst >= 0) v.has[i->dst] = false;
        } else if (k == WT_K_MTC1) {
            if (i->rt == 0)
                avp_pyintset_add(f->floats, 0);
            else if (v.has[i->rt])
                avp_pyintset_add(f->floats, v.val[i->rt]);
        } else if (i->dst >= 0) {
            v.has[i->dst] = false;
        }
        if (has_addr && k == WT_K_IMM) {
            const WtSection *s = wt_elf_section_of(e, addr);
            if (s && is_kind(s, "data")) {
                const char *text = wt_elf_cstring(a, e, addr);
                if (text && wt_pystr_len(text) >= 2) avp_set_add(f->strs, avp_str(text));
            }
        }
        if (pending) {
            for (int r = 0; r < 32; r++)
                if (!callee_saved(r)) v.has[r] = false;
            pending = false;
        }
        if (k == WT_K_CALL || k == WT_K_CALLR) pending = true;
    }
    f->n = nonzero;
    return f;
}

/* data_runs(elf, fn_starts, min_len=3) */
static void flush(WtRun *cur, WtRuns *runs) {
    size_t real = 0;
    for (size_t k = 0; k < cur->n; k++)
        if (cur->v[k].kind != 'o') real++;
    if (real >= 3) {
        WtRun copy = {0};
        avp_vec_reserve(&copy, cur->n);
        memcpy(copy.v, cur->v, cur->n * sizeof *cur->v);
        copy.n = cur->n;
        avp_vec_push(runs, copy);
    }
    cur->n = 0;
}

static void data_runs(AvpArena *a, const WtElf *e, const AvpISet *fn_starts, WtRuns *runs) {
    WtRun cur = {0};
    for (size_t k = 0; k < e->sections.n; k++) {
        const WtSection *s = &e->sections.v[k];
        if (!is_kind(s, "data")) continue;
        bool after_fn = false;
        for (i64 o = 0; o < s->size - 3; o += 4) {
            if (s->offset + o < 0 || (u64)(s->offset + o) + 4 > e->size)
                shsm_raise(SHSM_EXC_VALUE, "unpack_from: section %s: offset %lld is outside the file", s->name,
                           (long long)(s->offset + o));
            const u8 *p = e->data + s->offset + o;
            u32 w = (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;
            if (avp_iset_has(fn_starts, w)) {
                avp_vec_push(&cur, ((WtRunItem){'f', (i64)w, NULL}));
                after_fn = true;
                continue;
            }
            if (after_fn && w < 0x10000) { /* {code pointer, this-adjustment}: skip the second word */
                after_fn = false;
                continue;
            }
            after_fn = false;
            const WtSection *sec = w ? wt_elf_section_of(e, w) : NULL;
            if (!sec) {
                flush(&cur, runs);
                continue;
            }
            const char *text = is_kind(sec, "data") ? wt_elf_cstring(a, e, w) : NULL;
            if (text && *text)
                avp_vec_push(&cur, ((WtRunItem){'s', 0, text}));
            else
                avp_vec_push(&cur, ((WtRunItem){'o', 0, NULL}));
        }
        flush(&cur, runs);
    }
    avp_vec_free(&cur);
}

static void build_ps2(AvpArena *a, const WtElf *e, const i64 *given, size_t ngiven, WtSide *fns, WtRuns *runs,
                      WtFacts *facts) {
    Text t;
    decode_text(e, &t);
    u32 gp = 0;
    bool has_gp = find_gp(e, &t, &gp);
    Spans spans = {0};
    if (given) {
        for (size_t k = 0; k < ngiven; k++)
            if (given[2 * k + 1] > given[2 * k] && insn_at(&t, given[2 * k]) && insn_at(&t, given[2 * k + 1] - 4))
                avp_vec_push(&spans, ((Span){given[2 * k], given[2 * k + 1]}));
    } else recover_functions(e, &t, &spans);
    AvpISet *starts = avp_iset_new();
    for (size_t k = 0; k < spans.n; k++) avp_iset_add(starts, (u64)spans.v[k].start);
    wt_side_init(fns);
    for (size_t k = 0; k < spans.n; k++)
        wt_side_put(fns, analyse_function(a, e, &t, spans.v[k].start, spans.v[k].end, has_gp, gp, starts));
    memset(runs, 0, sizeof *runs);
    data_runs(a, e, starts, runs);
    memset(facts, 0, sizeof *facts);
    facts->machine = e->machine;
    facts->entry = e->entry;
    facts->gp = has_gp ? (i64)gp : WT_NONE;
    facts->words = t.words;
    for (size_t k = 0; k < e->sections.n; k++) avp_vec_push(&facts->sections, e->sections.v[k]);
    for (size_t k = 0; k < t.n; k++) free(t.v[k].insns);
    free(t.v);
    avp_vec_free(&spans);
    avp_iset_free(starts);
}

void wt_build_ps2_elf(AvpArena *a, const WtElf *e, WtSide *fns, WtRuns *runs, WtFacts *facts) {
    build_ps2(a, e, NULL, 0, fns, runs, facts);
}

void wt_build_ps2_spans(AvpArena *a, const WtElf *e, const i64 *spans, size_t n, WtSide *fns, WtRuns *runs,
                        WtFacts *facts) {
    build_ps2(a, e, spans, n, fns, runs, facts);
}

void wt_build_ps2(AvpArena *a, const u8 *data, size_t size, WtSide *fns, WtRuns *runs, WtFacts *facts) {
    wt_build_ps2_elf(a, wt_elf_new(a, data, size), fns, runs, facts);
}
