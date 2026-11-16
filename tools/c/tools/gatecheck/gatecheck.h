/* gatecheck: gate diagnostics (a port of python tools/gatecheck.py). `attribute` names the symbols behind a DOL hash
 * mismatch; `layout` compares a unit's built object with the retail split (data order, sizes, offsets, contents and
 * code-section sizes). The files follow the Python's sections: gc_elf.c (ELF32 big-endian reader), gc_meta.c
 * (splits.txt, symbols.txt, configure.py, SymbolMap, UnitMap), gc_attribute.c (attribute), gc_asm.c (retail asm
 * side of layout), gc_built.c (built object side), gc_compare.c (comparison), gc_cli.c (commands and parser). */
#pragma once
#include "avp/avp.h"
#include "shsm_cli.h"
#include "shsm_dol.h"

/* ---- exceptions (avp_throw codes; the Python classes they stand for) ---------------------------------------------- */
enum {
    GC_EXC_GATE = 120, /* GateError (shsm_dol's SHSM_EXC_VALUE / SHSM_EXC_OS are GateErrors too) */
    GC_EXC_STRUCT,     /* struct.error */
    GC_EXC_VALUE,      /* ValueError */
    GC_EXC_INDEX,      /* IndexError */
    GC_EXC_CRASH       /* an exception the Python does not catch (AttributeError, ...): a traceback, exit 1 */
};
AVP_NORETURN void gc_raise(int exc, AVP_PRINTF_FMT const char *fmt, ...);
bool gc_is_gate(int code); /* except GateError */

/* "0x%0*X" % (width, v) */
char *gc_hx(AvpArena *a, i64 v, int width);
/* Python "%X" % v (a minus sign for negatives) */
char *gc_hex(AvpArena *a, i64 v);
/* s[:n] in code points */
AvpStr gc_cp_prefix(AvpStr s, size_t n);

/* ---- ELF32 big-endian (gatecheck.Elf) ---------------------------------------------------------------------------- */
enum { GC_SHT_SYMTAB = 2, GC_SHT_RELA = 4, GC_SHT_NOBITS = 8, GC_SHT_REL = 9 };
enum { GC_STT_NOTYPE, GC_STT_OBJECT, GC_STT_FUNC, GC_STT_SECTION, GC_STT_FILE };
#define GC_SHN_ABS 0xFFF1

typedef struct {
    i64 index, name_off, type, flags, addr, off, size, link, info, align, entsize;
    const char *name; /* latin-1 decoded (UTF-8) */
} GcSec;
typedef struct {
    i64 index;
    const char *name;
    i64 value, size, bind, type, shndx;
} GcSym;
typedef struct {
    i64 offset, sym, type, addend;
} GcRel;
typedef AVP_VEC(GcRel) GcRelVec;
typedef struct {
    const u8 *data;
    size_t n;
    GcSec *secs;
    size_t nsecs;
    GcSym *syms;
    size_t nsyms;
    bool syms_loaded;
} GcElf;

GcElf *gc_elf_parse(AvpArena *a, const u8 *data, size_t n); /* raises GATE / STRUCT / VALUE as Elf() */
GcElf *gc_elf_load(AvpArena *a, const char *path);          /* Elf.load */
void gc_elf_symbols(AvpArena *a, GcElf *e);                  /* Elf.symbols (lazy); raises as the Python */
AvpStr gc_elf_section_data(const GcElf *e, const GcSec *s);  /* the slice data[off:off+size] (clamped) */
void gc_elf_relocs_of(const GcElf *e, const GcSec *s, GcRelVec *out);
void gc_elf_relocs_for(const GcElf *e, i64 sec_index, GcRelVec *out);

/* ---- metadata ---------------------------------------------------------------------------------------------------- */
typedef struct {
    i64 start, end;
    const char *section, *unit;
} GcSplit;
typedef AVP_VEC(GcSplit) GcSplitVec;
typedef struct {
    i64 addr, size;
    const char *name, *section, *type, *scope;
} GcSymTxt;
typedef AVP_VEC(GcSymTxt) GcSymTxtVec;

GcSplitVec gc_parse_splits(AvpArena *a, AvpStr text);
GcSymTxtVec gc_parse_symbols_txt(AvpArena *a, AvpStr text);
AvpMap *gc_parse_configure(AvpArena *a, AvpStr text); /* path -> status (val.p: const char *) */
AvpStr gc_read_text(AvpArena *a, const char *path);   /* raises GATE "cannot read <path>: <err>" */

typedef struct {
    i64 addr, size;
    const char *name;
} GcEntry;
typedef struct {
    GcEntry *e;
    i64 *addrs;
    size_t n;
} GcSymbolMap;
typedef struct {
    const char *name;
    i64 addr, size;
    bool inside;
} GcHit;
GcSymbolMap *gc_symbol_map(AvpArena *a, GcEntry *entries, size_t n); /* sorts (addr, -size), stable */
bool gc_symbol_lookup(const GcSymbolMap *m, i64 addr, GcHit *out);
bool gc_symbol_next_start(const GcSymbolMap *m, i64 addr, i64 *out);
GcSymbolMap *gc_elf_symbol_map(AvpArena *a, GcElf *e);

typedef struct {
    i64 addr, end;
    const char *name;
} GcElfRange;
typedef struct {
    GcSplit *ranges;
    i64 *starts;
    size_t n;
    AvpMap *statuses;
    GcElfRange *elf_sections;
    size_t nelf;
} GcUnitMap;
GcUnitMap *gc_unit_map(AvpArena *a, const GcSplitVec *splits, AvpMap *statuses);
const char *gc_unit_section_name(const GcUnitMap *u, i64 addr); /* NULL: None */
/* (unit, section) or false; *index is the range */
bool gc_unit_lookup(const GcUnitMap *u, i64 addr, const char **unit, const char **section, size_t *index);
const char *gc_unit_status(const GcUnitMap *u, const char *unit);

/* ---- attribute --------------------------------------------------------------------------------------------------- */
typedef struct {
    i64 start, end, nbytes;
} GcRun;
typedef AVP_VEC(GcRun) GcRunVec;
typedef struct {
    i64 addr;
    u8 r[4], b[4];
} GcWord;
typedef struct {
    const char *unit, *section, *sym;
    i64 sym_addr, sym_size, nbytes, first, last_end, words_seen;
    bool has_first;
    AVP_VEC(GcWord) words;
    const char *retail_names[3];
    int nretail;
} GcGroup;
typedef struct {
    const char *section;
    i64 addr;
    const char *unit, *sym;
    i64 saddr;
    const char *rname;
    i64 shift; /* 0: None */
} GcFirst;
typedef struct {
    AvpStrVec header;
    GcRunVec runs, only_retail, only_built;
    AVP_VEC(GcGroup *) groups;
    AVP_VEC(GcFirst) first; /* first_per_section in insertion order */
    i64 total_bytes;
} GcAttribution;

AvpStrVec gc_header_differences(AvpArena *a, const ShsmDol *retail, const ShsmDol *built);
void gc_byte_runs(const u8 *x, const u8 *y, i64 n, i64 base, GcRunVec *out); /* (start, end) runs */
GcRunVec gc_merge_runs(const GcRunVec *runs, i64 gap);
void gc_diff_dols(const ShsmDol *retail, const ShsmDol *built, GcRunVec *runs, GcRunVec *only_r, GcRunVec *only_b);
const char *gc_classify_word(AvpArena *a, u32 r, u32 b);
bool gc_shift_hint(AvpArena *a, const ShsmDol *retail, const ShsmDol *built, i64 pos, i64 *out);
GcAttribution *gc_attribute(AvpArena *a, const ShsmDol *retail, const ShsmDol *built, const GcUnitMap *units,
                            const GcSymbolMap *built_syms, const GcSymbolMap *retail_syms, i64 words);
AvpStrVec gc_format_attribution(AvpArena *a, const GcAttribution *res, const GcUnitMap *units, i64 limit_symbols,
                                i64 limit_units, bool all_symbols, const char *built_path, const char *retail_path);
const char *gc_elf_dol_mismatch(AvpArena *a, const GcElf *e, const ShsmDol *dol); /* NULL: belongs */

/* ---- layout: objects ----------------------------------------------------------------------------------------------- */
typedef struct {
    i64 off;
    const char *target, *addend;
} GcReloc;
typedef AVP_VEC(GcReloc) GcRelocVec; /* a dict: sorted by off, unique (gc_relocs_dict) */
void gc_relocs_dict(GcRelocVec *v);  /* dict(list): the last value of each key wins */
const GcReloc *gc_relocs_get(const GcRelocVec *v, i64 off);

typedef struct GcObj {
    const char *name;
    i64 off, size;
    const u8 *data; /* NULL: None */
    i64 dlen;
    GcRelocVec relocs;
    bool gap, synthetic, anon, dead, after_dead, has_addr;
    const char *dedup;
    i64 addr;
    const u8 *mk; /* masked() (computed on first use) */
} GcObj;
typedef AVP_VEC(GcObj *) GcObjVec;
GcObj *gc_obj_new(AvpArena *a, const char *name, i64 off, i64 size, const u8 *data, i64 dlen, bool gap,
                  bool synthetic);
static inline i64 gc_obj_end(const GcObj *o) { return o->off + o->size; }

bool gc_is_anon(const char *name);
bool gc_is_gap_name(const char *name);
AvpStr gc_base_section(AvpStr name);
bool gc_is_data_section(const char *name);
bool gc_is_nobits_section(const char *name);
bool gc_is_code_section(const char *name);
bool gc_is_root_section(const char *name);
extern const char *const gc_data_sections[11];

typedef struct {
    const char *name;
    bool has_start, has_end;
    i64 start, end;
    GcObjVec objs;
    AvpStrVec warnings;
    i64 loose_pos;
    GcRelocVec loose_relocs; /* a list (not deduplicated) */
} GcAsmSection;
typedef struct {
    const char *name;
    bool has_start;
    i64 start, end;
    AvpStrVec fns;
} GcAsmCode;
typedef struct {
    AVP_VEC(GcAsmSection *) sections; /* insertion order */
    AVP_VEC(GcAsmCode *) code;
    AvpSet *names;
} GcAsmUnit;
GcAsmSection *gc_asm_section(const GcAsmUnit *u, const char *name);
GcAsmCode *gc_asm_code(const GcAsmUnit *u, const char *name);

i64 gc_unescape_asm_string(AvpStr s);
i64 gc_advance_directive(AvpArena *a, AvpStr line, i64 pos, GcRelocVec *relocs);
GcAsmUnit *gc_parse_asm_data(AvpArena *a, AvpStr text, const ShsmDol *retail);

typedef struct {
    const char *name;
    GcObjVec objs;
    i64 size, align;
    GcRelocVec relocs;
    const u8 *data; /* NULL for nobits */
    i64 dlen;
    bool nobits;
} GcBuiltSection;
typedef struct {
    AVP_VEC(GcBuiltSection *) v;
} GcBuiltSections;
GcBuiltSection *gc_built_get(const GcBuiltSections *s, const char *name);

typedef struct {
    const char *name, *unit;
} GcDedupe; /* name -> label */
typedef AVP_VEC(GcDedupe) GcDedupeVec;

/* live_symbols: a flag per symbol index, set for the symbols the linker keeps (dropped may be NULL) */
u8 *gc_live_symbols(AvpArena *a, GcElf *e, const AvpSet *kept, const AvpSet *dropped);
GcBuiltSections *gc_built_sections(AvpArena *a, GcElf *e, const AvpSet *kept, const GcDedupeVec *dedupe);

/* ---- layout: comparison ---------------------------------------------------------------------------------------- */
typedef struct {
    const char *kind, *section, *text;
    bool difference;
} GcFinding;
typedef AVP_VEC(GcFinding) GcFindingVec;

typedef struct {
    i64 i, j;
} GcPair;
typedef AVP_VEC(GcPair) GcPairVec;
int gc_pair_score(const GcObj *r, const GcObj *b);
bool gc_content_equal(const GcObj *r, const GcObj *b);
GcPairVec gc_align_objects(GcObj *const *R, size_t n, GcObj *const *B, size_t m);
void gc_compare_section(AvpArena *a, GcFindingVec *F, const char *secname, GcObj *const *R, size_t nr, bool has_rspan,
                        i64 rspan, bool has_rstart, i64 rstart, GcObj *const *B, size_t nb, i64 bsize, i64 balign,
                        const u8 *rbytes, i64 rlen, const u8 *bbytes, i64 blen, const GcRelocVec *rrelocs,
                        const GcRelocVec *brelocs);

typedef struct {
    const char *unit, *status;
    GcFindingVec findings;
    AvpStrVec notes, errors;
    AvpMap *sections; /* name -> summary line (val.p) */
} GcLayoutResult;
size_t gc_layout_ndiffs(const GcLayoutResult *r);

const char *gc_unit_key(AvpArena *a, const char *name);
const char *gc_strip_ext(AvpArena *a, const char *path);
void gc_compare_unit(AvpArena *a, GcLayoutResult *res, const GcAsmUnit *u, const GcBuiltSections *b,
                     const char *only_section);
/* retail may be NULL: a code section with no sized symbol is then sized but its bytes are not compared */
void gc_compare_code_sizes(AvpArena *a, GcLayoutResult *res, const GcAsmUnit *u, GcElf *e, const char *only_section,
                           const ShsmDol *retail);

typedef struct {
    const char *name, *unit; /* unit NULL: unsplit */
    i64 addr;
} GcWeak;
typedef AVP_VEC(GcWeak) GcWeakVec; /* insertion order, first of each name */
GcWeakVec gc_weak_dedupe_index(AvpArena *a, const GcSymTxtVec *symbols, const GcSplitVec *splits);
GcDedupeVec gc_dedupe_for_unit(AvpArena *a, const GcWeakVec *index, const char *unit);
GcLayoutResult *gc_check_layout(AvpArena *a, const char *unit, const char *status, const char *obj_path,
                                const char *asm_path, const ShsmDol *retail, const char *only_section,
                                const GcWeakVec *dedupe);
AvpStrVec gc_format_layout(AvpArena *a, const GcLayoutResult *res, bool verbose);

/* ---- layout: stale objects ------------------------------------------------------------------------------------ */
/* Exit status of `gatecheck layout` when a built object is older than its source, an included header, splits.txt or
 * symbols.txt (the verdict would describe the previous build). */
#define GC_EXIT_STALE 3
typedef struct GcStaleCheck GcStaleCheck;
GcStaleCheck *gc_stale_open(AvpArena *a, const char *root, const char *splits, const char *symbols);
/* The first input of the object (root-relative obj_rel, e.g. build/R5WEA4/src/game/X.o; src_rel the unit's source,
 * e.g. src/game/X.cpp) that is newer than obj_path, as a path to print; NULL when the object is current. */
const char *gc_stale_newer_input(AvpArena *a, GcStaleCheck *c, const char *obj_rel, const char *obj_path,
                                 const char *src_rel);

/* ---- commands -------------------------------------------------------------------------------------------------- */
int gc_main(int argc, char **argv);
/* `gatecheck layout <unit> [--object OBJ]` run in-process (match --data): the exit status the command gives (0 layout
 * matches, 1 differs, 2 error) and the lines it prints, stdout's in *out and stderr's in *err (without newlines). The
 * retail DOL, configure.py, splits and symbols are read under root, as the command's defaults. */
int gc_layout_lines(AvpArena *a, const char *root, const char *unit, const char *object, AvpStrVec *out,
                    AvpStrVec *err);
