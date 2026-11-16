/* witness_ps2: the PS2 executable (tools/witness.py, "the executable", "MIPS decoder", "PS2 model").
 *
 * ISO 9660 and SYSTEM.CNF (iso_files, read_iso_exe), the ELF reader (Elf, Section, cstring_at), the MIPS subset
 * decoder (decode, Insn, format_insn), and the PS2 side of the model (decode_text, find_gp, _jump_table,
 * recover_functions, analyse_function, _const, data_runs, build_ps2). find_exe, load_exe and build_ps2 are declared
 * in witness.h. */
#ifndef WITNESS_PS2_H
#define WITNESS_PS2_H

#include "witness.h"

/* ---- MIPS decoder ---- */
enum { WT_GP = 28, WT_SP = 29, WT_FP = 30, WT_RA = 31 };
extern const char *const WT_REG[32];

typedef enum WtKind {
    WT_K_NOP, WT_K_CALL, WT_K_CALLR, WT_K_JUMP, WT_K_JUMPR, WT_K_RET, WT_K_BRANCH,
    WT_K_LUI, WT_K_IMM, WT_K_LOAD, WT_K_STORE, WT_K_MTC1, WT_K_ALU, WT_K_OTHER
} WtKind;
const char *wt_kind_name(WtKind k); /* Insn.kind: "nop", "call", "callr", ... */

/* Insn. `target` is meaningful for call, jump and branch only (None elsewhere); `dst` is -1 for None. */
typedef struct WtInsn {
    u32 pc, word, target;
    i32 imm;
    const char *mn; /* a static string */
    u8 kind, rs, rt, rd, sa;
    i8 dst;
    bool fp, likely;
} WtInsn;

void wt_decode(u32 word, u32 pc, WtInsn *out);
bool wt_insn_is_cti(const WtInsn *i);
/* format_insn(i) into buf (64 bytes always suffice); returns buf. */
#define WT_INSN_TEXT 64
char *wt_format_insn(const WtInsn *i, char buf[WT_INSN_TEXT]);

/* ---- ELF ---- */
typedef struct WtElf {
    const u8 *data;
    size_t size;
    i64 machine, entry;
    AVP_VEC(WtSection) sections; /* sorted by address (stable) */
} WtElf;

/* Elf(data): raises SHSM_EXC_USAGE "not a 32-bit little-endian ELF". `data` must outlive the WtElf. */
WtElf *wt_elf_new(AvpArena *a, const u8 *data, size_t size);
const WtSection *wt_elf_section_of(const WtElf *e, i64 addr); /* NULL = None */
bool wt_elf_word(const WtElf *e, i64 addr, u32 *out);         /* false = None */
bool wt_elf_float_at(const WtElf *e, i64 addr, u32 *out);     /* false = None */
/* Elf.cstring(addr, maxlen=512): UTF-8 text in `a`, NULL = None. */
const char *wt_elf_cstring(AvpArena *a, const WtElf *e, i64 addr);
/* cstring_at(data, off, lo, hi, maxlen): UTF-8 text in `a` (latin-1 bytes converted), NULL = None. Also the Wii
 * Dol's reader. */
const char *wt_cstring_at(AvpArena *a, const u8 *data, size_t size, i64 off, i64 lo, i64 hi, i64 maxlen);
/* len() of a Python str given as UTF-8 (code points). */
size_t wt_pystr_len(const char *s);

/* ---- PS2 model ---- */
/* build_ps2(elf): the same as wt_build_ps2 on an already parsed ELF. */
void wt_build_ps2_elf(AvpArena *a, const WtElf *e, WtSide *fns, WtRuns *runs, WtFacts *facts);
/* The same with the function spans given (a symbol table's: spans[2k] start, spans[2k+1] end, in address order)
 * instead of recovered; spans outside the decoded text are dropped. Used by sibling for a symbolised MIPS ELF. */
void wt_build_ps2_spans(AvpArena *a, const WtElf *e, const i64 *spans, size_t n, WtSide *fns, WtRuns *runs,
                        WtFacts *facts);

#endif
