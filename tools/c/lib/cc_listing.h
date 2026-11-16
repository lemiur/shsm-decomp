/* dtk listings -> rows (tools/codecmp.py: Ref, Row, parse_row, decode_branch, FnCode, parse_listing, load_asm,
 * disasm_object, load_object). The same parser reads the retail unit listing (build/<ver>/asm/<unit>.s) and the
 * listing `dtk elf disasm` writes for a compiled object; on the compiled side each row is tied to its ELF
 * relocation. The Python's regular expressions are hand-coded matchers with the same results (ASCII whitespace
 * plus \x1c-\x1f stands for Python's \s; listings are ASCII).
 *
 * Every string is an AvpStr view that is also NUL-terminated. All memory comes from the arena given. */
#pragma once
#include "cc_elf.h"

/* A symbolic operand of a row: symbol name (quotes removed), constant offset, relocation kind ("ha", "l",
 * "sda21", ... or "rel24" for a branch/call target). */
typedef struct {
    AvpStr name;
    i64 off;
    AvpStr kind;
} CcRef;

typedef struct {
    i64 addr;        /* listing address (retail: virtual address; compiled: section offset) */
    bool has_word;
    u32 word;        /* the instruction word (when the listing's byte column holds 4 bytes) */
    AvpStr op;       /* first word of text */
    AvpStr text;     /* the instruction text, whitespace runs collapsed to one space */
    AvpStr norm;     /* normalised text: the symbol -> SYM (kind kept), local labels -> L */
    bool has_ref;
    CcRef ref;       /* the symbolic operand */
    bool has_label;
    i64 label;       /* address of the .L_ branch target (or a decoded local branch) */
    i64 bidx;        /* row index of that label inside the function, -1 when none */
    const CcReloc *reloc; /* the ELF relocation of the row (compiled side), NULL when none */
    bool stray;      /* a symbol-looking name the operand parser did not understand */
} CcRow;

typedef struct CcFn {
    AvpStr name;
    CcRow *rows;
    size_t nrows;
    const CcElf *elf; /* compiled side only */
    bool has_shndx;
    u32 shndx;        /* the function symbol's section (compiled side, when the ELF has the function) */
    i64 start, end;   /* start: the symbol value, else the first row's address; end: last row + 4 */
} CcFn;

/* parse_row(addr, text): a row from an instruction's text (has_word false, reloc NULL). */
CcRow cc_parse_row(AvpArena *a, i64 addr, AvpStr text);
/* decode_branch(word, addr): target of a b/bl/bc word, false for absolute branches and other instructions. */
bool cc_decode_branch(u32 word, i64 addr, i64 *target);
/* FnCode(name, rows, elf, shndx, start): makes local branches local and sets bidx. rows is taken over. has_start
 * false is the Python's start=None. */
CcFn *cc_fn_new(AvpArena *a, AvpStr name, CcRow *rows, size_t nrows, const CcElf *elf, bool has_shndx, u32 shndx,
                bool has_start, i64 start);
/* FnCode.from_norms(name, norms): a target-less function from normalised row strings. */
CcFn *cc_fn_from_norms(AvpArena *a, AvpStr name, const AvpStr *norms, size_t n);
/* [r.norm for r in fn.rows] into out (n = fn->nrows). */
void cc_fn_norms(const CcFn *fn, AvpStr *out);

/* parse_listing(text, elf): {function name: FnCode} as an AvpMap name -> CcFn* (val.p), in listing order (a
 * repeated name keeps its first position and the last block). elf NULL for a retail listing. */
AvpMap *cc_parse_listing(AvpArena *a, AvpStr text, const CcElf *elf);
/* load_asm(path): the retail listing read as UTF-8. NULL with *err (Python's str(OSError)) when unreadable. */
AvpMap *cc_load_asm(AvpArena *a, const char *path, AvpStr *err);
/* disasm_object + load_object(obj, dtk): runs `<dtk> elf disasm <obj> <obj>.s` (dtk NULL: <root>/tools/bin/dtk.exe
 * with root = avp_root()), reads the ELF and the listing. NULL with *err on failure (the Python raised). */
AvpMap *cc_load_object(AvpArena *a, const char *obj, const char *dtk, AvpStr *err);

/* The lookups the matchers use, exposed for the tests. */
bool cc_is_space(u8 c); /* Python's \s on one byte (ASCII + \x1c-\x1f) */
