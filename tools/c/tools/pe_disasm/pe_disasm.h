/* pe_disasm: a port of tools/mwcc_trace/pe_disasm.py (a static explorer of a Win32 x86 PE: strings, xrefs,
 * per-function disassembly, call graph), on capstone 5.0.7 (third_party/capstone) and lib/shsm_pe. */
#pragma once
#include <capstone/capstone.h>

#include "avp/avp.h"
#include "shsm_pe.h"

/* pe_disasm.PE: the file and its sections. */
typedef struct {
    char name[9];
    i64 va;   /* image base + VirtualAddress */
    i64 size; /* min(VirtualSize, SizeOfRawData) when VirtualSize, else SizeOfRawData */
    u32 raw;  /* PointerToRawData */
    u32 ch;   /* Characteristics */
} PeDisSec;

typedef struct {
    const u8 *data;
    size_t n;
    u32 base;
    PeDisSec *secs;
    size_t nsecs;
    i64 lo, hi; /* min section va, max section end */
} PeDisPe;

/* PE(path): throws AVP_ERR_IO (OSError text), SHSM_PE_EXC_STRUCT, or AVP_ERR_OTHER "ValueError: ..." for a PE
 * with no sections (min() of an empty sequence). */
PeDisPe *pe_disasm_open(AvpArena *a, const char *path);
PeDisPe *pe_disasm_parse(AvpArena *a, const u8 *d, size_t n);
/* PE.off: the file offset of va, false for None. */
bool pe_disasm_off(const PeDisPe *pe, i64 va, u64 *off);
/* PE.read(va, n): the clamped bytes (len 0 when va is in a section but past the file); false for None. */
bool pe_disasm_read(const PeDisPe *pe, i64 va, size_t n, AvpStr *out);
/* PE.cstr(va): a printable NUL-terminated string of at least 4 characters within 96 bytes, else .p == NULL. */
AvpStr pe_disasm_cstr(const PeDisPe *pe, i64 va);

/* The index (calls, refs and function starts of every executable section). */
typedef struct {
    i64 a, v;
} PeDisPair;
typedef struct {
    double mtime;
    PeDisPair *calls;
    size_t ncalls;
    PeDisPair *refs;
    size_t nrefs;
    i64 *starts;
    size_t nstarts;
} PeDisIndex;

/* build_index(pe, path) without the cache, and with it (cache_dir/<basename>.idx.bin, reused while the exe's
 * mtime is the one stored). cache_dir NULL: no cache. */
PeDisIndex *pe_disasm_build_index(AvpArena *a, const PeDisPe *pe, double mtime);
PeDisIndex *pe_disasm_index(AvpArena *a, const PeDisPe *pe, const char *path, const char *cache_dir);
/* func_of: the greatest start <= va; false for None. */
bool pe_disasm_func_of(const PeDisIndex *idx, i64 va, i64 *start);

/* capstone handle in 32-bit x86 mode with details on (md()) */
csh pe_disasm_md(void);
/* func_extent(pe, start, maxn): decoded instructions, NULL entries for undecodable bytes. Each cs_insn is owned by
 * the caller (free with pe_disasm_free_extent). */
typedef AVP_VEC(cs_insn *) PeDisInsns;
PeDisInsns pe_disasm_func_extent(csh h, const PeDisPe *pe, i64 start, i64 maxn);
void pe_disasm_free_extent(PeDisInsns *v);

/* operand_values(ins): imm32 / disp32 values of non-branch operands, into out (returns the count, at most 8). */
size_t pe_disasm_operand_values(const cs_insn *ins, i64 out[8]);

/* The commands: output appended to out. Throws as the Python raises (see main.c). */
void pe_disasm_cmd_strings(AvpArena *a, const PeDisPe *pe, AvpStr rx, AvpBuf *out);
/* fname(va): the NB11 name when a function starts exactly at va, else "func_<hex>". syms may be NULL. */
AvpStr pe_disasm_fname(AvpArena *a, const ShsmPeSymbols *syms, i64 va);

/* The whole tool (argv with the program name first); returns the exit code. */
int pe_disasm_main(int argc, char **argv);

/* The Python module docstring (printed for a short command line or an unknown command). */
extern const char pe_disasm_doc[];
