/* The MIPS decoder of witness.py (the subset used): REG, the opcode tables, decode, format_insn. */
#include "witness_ps2.h"

#include <stdio.h>
#include <string.h>

const char *const WT_REG[32] = {"zero", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2",
                                "t3",   "t4", "t5", "t6", "t7", "s0", "s1", "s2", "s3", "s4", "s5",
                                "s6",   "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"};

static const char *const KIND_NAMES[] = {"nop",  "call", "callr", "jump",  "jumpr", "ret",  "branch",
                                         "lui",  "imm",  "load",  "store", "mtc1",  "alu",  "other"};

const char *wt_kind_name(WtKind k) { return KIND_NAMES[k]; }

/* _SPECIAL (by function field) */
static const char *const SPECIAL[64] = {
    [0] = "sll",     [2] = "srl",    [3] = "sra",     [4] = "sllv",    [6] = "srlv",    [7] = "srav",
    [8] = "jr",      [9] = "jalr",   [10] = "movz",   [11] = "movn",   [12] = "syscall", [13] = "break",
    [15] = "sync",   [16] = "mfhi",  [17] = "mthi",   [18] = "mflo",   [19] = "mtlo",   [20] = "dsllv",
    [22] = "dsrlv",  [23] = "dsrav", [24] = "mult",   [25] = "multu",  [26] = "div",    [27] = "divu",
    [32] = "add",    [33] = "addu",  [34] = "sub",    [35] = "subu",   [36] = "and",    [37] = "or",
    [38] = "xor",    [39] = "nor",   [40] = "mfsa",   [41] = "mtsa",   [42] = "slt",    [43] = "sltu",
    [44] = "dadd",   [45] = "daddu", [46] = "dsub",   [47] = "dsubu",  [56] = "dsll",   [58] = "dsrl",
    [59] = "dsra",   [60] = "dsll32", [62] = "dsrl32", [63] = "dsra32"};
/* _IMM, _LOADS, _STORES, _BRANCH, _OTHER_OPS (by opcode) */
static const char *const IMM[64] = {[8] = "addi",  [9] = "addiu", [10] = "slti",  [11] = "sltiu", [12] = "andi",
                                    [13] = "ori",  [14] = "xori", [15] = "lui",   [24] = "daddi", [25] = "daddiu"};
static const char *const LOADS[64] = {[32] = "lb",  [33] = "lh",  [34] = "lwl", [35] = "lw",   [36] = "lbu",
                                      [37] = "lhu", [38] = "lwr", [39] = "lwu", [26] = "ldl",  [27] = "ldr",
                                      [55] = "ld",  [30] = "lq",  [49] = "lwc1", [54] = "lqc2"};
static const char *const STORES[64] = {[40] = "sb",  [41] = "sh", [42] = "swl", [43] = "sw",   [44] = "sdl", [45] = "sdr",
                                       [46] = "swr", [63] = "sd", [31] = "sq",  [57] = "swc1", [62] = "sqc2"};
static const char *const BRANCH[64] = {[4] = "beq",   [5] = "bne",   [6] = "blez",   [7] = "bgtz",
                                       [20] = "beql", [21] = "bnel", [22] = "blezl", [23] = "bgtzl"};
static const char *const REGIMM[32] = {[0] = "bltz",    [1] = "bgez",    [2] = "bltzl",    [3] = "bgezl",
                                       [16] = "bltzal", [17] = "bgezal", [18] = "bltzall", [19] = "bgezall"};
static const char *const COP1_S[64] = {[0] = "add.s",   [1] = "sub.s",   [2] = "mul.s",   [3] = "div.s",
                                       [4] = "sqrt.s",  [5] = "abs.s",   [6] = "mov.s",   [7] = "neg.s",
                                       [22] = "rsqrt.s", [24] = "adda.s", [26] = "mula.s", [28] = "madd.s",
                                       [29] = "msub.s", [36] = "cvt.w.s", [40] = "max.s", [41] = "min.s",
                                       [48] = "c.f.s",  [50] = "c.eq.s", [52] = "c.lt.s", [54] = "c.le.s"};
static const char *const OTHER_OPS[64] = {[16] = "cop0", [18] = "cop2", [28] = "mmi", [47] = "cache", [51] = "pref"};

static bool is_shift(u32 f) { return f == 0 || f == 2 || f == 3 || f == 56 || f == 58 || f == 59 || f == 60 || f == 62 || f == 63; }
static bool is_no_dst(u32 f) { return f == 8 || f == 12 || f == 13 || f == 15 || f == 17 || f == 19 || f == 41; }
static bool is_fp_mem(u32 op) { return op == 49 || op == 54 || op == 57 || op == 62; }

static void set(WtInsn *o, const char *mn, WtKind kind) {
    o->mn = mn;
    o->kind = (u8)kind;
}

void wt_decode(u32 word, u32 pc, WtInsn *o) {
    memset(o, 0, sizeof *o);
    o->pc = pc;
    o->word = word;
    o->dst = -1;
    u32 op = word >> 26, rs = (word >> 21) & 31, rt = (word >> 16) & 31, rd = (word >> 11) & 31;
    u32 imm = word & 0xFFFF;
    i32 simm = (imm & 0x8000) ? (i32)imm - 0x10000 : (i32)imm;
    u32 btarget = pc + 4 + ((u32)simm << 2); /* (pc + 4 + (simm << 2)) & 0xFFFFFFFF */
    if (word == 0) {
        set(o, "nop", WT_K_NOP);
        return;
    }
    if (op == 0) {
        u32 f = word & 63;
        const char *mn = SPECIAL[f] ? SPECIAL[f] : "special";
        if (f == 8) {
            set(o, "jr", rs == WT_RA ? WT_K_RET : WT_K_JUMPR);
            o->rs = (u8)rs;
            return;
        }
        if (f == 9) {
            set(o, "jalr", WT_K_CALLR);
            o->rs = (u8)rs;
            o->rd = (u8)rd;
            o->dst = (i8)rd;
            return;
        }
        o->rs = (u8)rs;
        o->rt = (u8)rt;
        o->rd = (u8)rd;
        o->dst = (is_no_dst(f) || rd == 0) ? -1 : (i8)rd;
        if ((f == 33 || f == 45 || f == 37) && rt == 0) {
            set(o, "move", WT_K_ALU);
            return;
        }
        set(o, mn, WT_K_ALU);
        o->sa = is_shift(f) ? (u8)((word >> 6) & 31) : 0;
        return;
    }
    if (op == 2 || op == 3) {
        o->target = ((pc + 4) & 0xF0000000u) | ((word & 0x3FFFFFFu) << 2);
        set(o, op == 3 ? "jal" : "j", op == 3 ? WT_K_CALL : WT_K_JUMP);
        if (op == 3) o->dst = WT_RA;
        return;
    }
    if (BRANCH[op]) {
        set(o, (op == 4 && rs == 0 && rt == 0) ? "b" : BRANCH[op], WT_K_BRANCH);
        o->rs = (u8)rs;
        o->rt = (u8)rt;
        o->imm = simm;
        o->target = btarget;
        o->likely = op >= 20;
        return;
    }
    if (op == 1) {
        o->rs = (u8)rs;
        o->rt = (u8)rt;
        if (REGIMM[rt]) {
            set(o, REGIMM[rt], WT_K_BRANCH);
            o->imm = simm;
            o->target = btarget;
            o->likely = (rt & 2) != 0;
            if (rt & 16) o->dst = WT_RA;
            return;
        }
        set(o, "regimm", WT_K_OTHER);
        return;
    }
    if (IMM[op]) {
        o->rt = (u8)rt;
        o->dst = rt ? (i8)rt : -1;
        if (op == 15) {
            set(o, "lui", WT_K_LUI);
            o->imm = (i32)imm;
            return;
        }
        set(o, IMM[op], WT_K_IMM);
        o->rs = (u8)rs;
        o->imm = (op >= 12 && op <= 15) ? (i32)imm : simm;
        return;
    }
    if (LOADS[op]) {
        bool fp = is_fp_mem(op);
        set(o, LOADS[op], WT_K_LOAD);
        o->rs = (u8)rs;
        o->rt = (u8)rt;
        o->imm = simm;
        o->dst = (fp || rt == 0) ? -1 : (i8)rt;
        o->fp = fp;
        return;
    }
    if (STORES[op]) {
        set(o, STORES[op], WT_K_STORE);
        o->rs = (u8)rs;
        o->rt = (u8)rt;
        o->imm = simm;
        o->fp = is_fp_mem(op);
        return;
    }
    if (op == 17) {
        if (rs == 4) {
            set(o, "mtc1", WT_K_MTC1);
            o->rt = (u8)rt;
            o->rd = (u8)rd;
            return;
        }
        if (rs == 0) {
            set(o, "mfc1", WT_K_ALU);
            o->rt = (u8)rt;
            o->rd = (u8)rd;
            o->dst = rt ? (i8)rt : -1;
            return;
        }
        if (rs == 8) {
            static const char *const BC1[4] = {"bc1f", "bc1t", "bc1fl", "bc1tl"};
            set(o, BC1[rt & 3], WT_K_BRANCH);
            o->imm = simm;
            o->target = btarget;
            o->likely = (rt & 2) != 0;
            return;
        }
        if (rs == 16) {
            set(o, COP1_S[word & 63] ? COP1_S[word & 63] : "cop1.s", WT_K_OTHER);
            o->rt = (u8)rt;
            o->rd = (u8)rd;
            o->sa = (u8)((word >> 6) & 31);
            return;
        }
        if (rs == 20 && (word & 63) == 32) {
            set(o, "cvt.s.w", WT_K_OTHER);
            o->rd = (u8)rd;
            return;
        }
        set(o, rs == 2 ? "cfc1" : rs == 6 ? "ctc1" : "cop1", WT_K_OTHER);
        o->rt = (u8)rt;
        o->rd = (u8)rd;
        o->dst = (rs == 2 && rt) ? (i8)rt : -1;
        return;
    }
    set(o, OTHER_OPS[op] ? OTHER_OPS[op] : ".word", WT_K_OTHER);
}

bool wt_insn_is_cti(const WtInsn *i) {
    return i->kind == WT_K_CALL || i->kind == WT_K_CALLR || i->kind == WT_K_JUMP || i->kind == WT_K_JUMPR ||
           i->kind == WT_K_RET || i->kind == WT_K_BRANCH;
}

static bool ends_with(const char *s, const char *suf) {
    size_t n = strlen(s), m = strlen(suf);
    return n >= m && memcmp(s + n - m, suf, m) == 0;
}

static bool in_list(const char *s, const char *const *list) {
    for (; *list; list++)
        if (strcmp(s, *list) == 0) return true;
    return false;
}

char *wt_format_insn(const WtInsn *i, char buf[WT_INSN_TEXT]) {
    const char *const *r = WT_REG;
    const char *mn = i->mn;
    WtKind k = (WtKind)i->kind;
    size_t cap = WT_INSN_TEXT;
    switch (k) {
    case WT_K_NOP: snprintf(buf, cap, "nop"); return buf;
    case WT_K_CALL:
    case WT_K_JUMP: snprintf(buf, cap, "%s 0x%X", mn, i->target); return buf;
    case WT_K_RET:
    case WT_K_JUMPR: snprintf(buf, cap, "jr %s", r[i->rs]); return buf;
    case WT_K_CALLR:
        if (i->rd == WT_RA)
            snprintf(buf, cap, "jalr %s", r[i->rs]);
        else
            snprintf(buf, cap, "jalr %s, %s", r[i->rd], r[i->rs]);
        return buf;
    case WT_K_BRANCH: {
        u32 op = i->word >> 26;
        if (strcmp(mn, "b") == 0 || strncmp(mn, "bc1", 3) == 0)
            snprintf(buf, cap, "%s 0x%X", mn, i->target);
        else if (op == 4 || op == 5 || op == 20 || op == 21)
            snprintf(buf, cap, "%s %s, %s, 0x%X", mn, r[i->rs], r[i->rt], i->target);
        else
            snprintf(buf, cap, "%s %s, 0x%X", mn, r[i->rs], i->target);
        return buf;
    }
    case WT_K_LUI: snprintf(buf, cap, "lui %s, 0x%X", r[i->rt], (u32)i->imm); return buf;
    case WT_K_IMM:
        if (strcmp(mn, "ori") == 0 || strcmp(mn, "andi") == 0 || strcmp(mn, "xori") == 0)
            snprintf(buf, cap, "%s %s, %s, 0x%x", mn, r[i->rt], r[i->rs], (u32)i->imm); /* hex(): non-negative */
        else
            snprintf(buf, cap, "%s %s, %s, %d", mn, r[i->rt], r[i->rs], i->imm);
        return buf;
    case WT_K_LOAD:
    case WT_K_STORE: {
        char reg[8];
        if (ends_with(mn, "c1"))
            snprintf(reg, sizeof reg, "$f%d", i->rt);
        else if (ends_with(mn, "c2"))
            snprintf(reg, sizeof reg, "vf%d", i->rt);
        else
            snprintf(reg, sizeof reg, "%s", r[i->rt]);
        snprintf(buf, cap, "%s %s, %d(%s)", mn, reg, i->imm, r[i->rs]);
        return buf;
    }
    case WT_K_MTC1: snprintf(buf, cap, "mtc1 %s, $f%d", r[i->rt], i->rd); return buf;
    default: break;
    }
    if (strcmp(mn, "move") == 0) {
        snprintf(buf, cap, "move %s, %s", r[i->rd], r[i->rs]);
        return buf;
    }
    if (strcmp(mn, "mfc1") == 0) {
        snprintf(buf, cap, "mfc1 %s, $f%d", r[i->rt], i->rd);
        return buf;
    }
    if (k == WT_K_ALU) {
        static const char *const SH[] = {"sll", "srl", "sra", "dsll", "dsrl", "dsra", "dsll32", "dsrl32", "dsra32", NULL};
        static const char *const HL[] = {"mfhi", "mflo", NULL};
        static const char *const TO[] = {"mthi", "mtlo", NULL};
        if (i->sa || in_list(mn, SH))
            snprintf(buf, cap, "%s %s, %s, %d", mn, r[i->rd], r[i->rt], i->sa);
        else if (in_list(mn, HL))
            snprintf(buf, cap, "%s %s", mn, r[i->rd]);
        else if (in_list(mn, TO))
            snprintf(buf, cap, "%s %s", mn, r[i->rs]);
        else
            snprintf(buf, cap, "%s %s, %s, %s", mn, r[i->rd], r[i->rs], r[i->rt]);
        return buf;
    }
    if (ends_with(mn, ".s") && strcmp(mn, "cop1.s") != 0) {
        snprintf(buf, cap, "%s $f%d, $f%d, $f%d", mn, i->sa, i->rd, i->rt);
        return buf;
    }
    if (strcmp(mn, ".word") != 0)
        snprintf(buf, cap, "%s", mn);
    else
        snprintf(buf, cap, ".word 0x%08X", i->word);
    return buf;
}
