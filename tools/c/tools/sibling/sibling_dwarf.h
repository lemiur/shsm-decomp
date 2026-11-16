/* sibling: a DWARF 2 reader for the sibling's ELF (32-bit big-endian, version 2, 4-byte addresses), as MWCC emits it.
 *
 * What MWCC (Gormiti, CW for Wii 1.1) emits, and so what this reader is built and tested for: one shared abbreviation
 * table; forms addr, block, block2, data1, data2, data4, flag, ref_addr, string, udata (every other DWARF 2 form is
 * sized too); DW_AT_location and DW_AT_frame_base as data4 offsets into .debug_loc; lexical blocks whose pc bounds
 * use the ref_addr form; DW_AT_sibling values that do not always point at the next sibling (never followed); type
 * references across compile units (ref_addr is section-absolute); unqualified class names (no namespaces) and no
 * member-function declarations inside classes (the defining subprogram carries "Class::Method(args)").
 * A form the reader cannot size stops that compile unit with a note; an unknown tag or attribute is kept or skipped.
 * Evidence only, never gate input. */
#pragma once
#include "avp/arena.h"
#include "avp/str.h"
#include "avp/vec.h"

/* ---- tags, attributes, forms (DWARF 2) ---- */
enum {
    SD_TAG_array_type = 0x01, SD_TAG_class_type = 0x02, SD_TAG_enumeration_type = 0x04,
    SD_TAG_formal_parameter = 0x05, SD_TAG_lexical_block = 0x0b, SD_TAG_member = 0x0d, SD_TAG_pointer_type = 0x0f,
    SD_TAG_reference_type = 0x10, SD_TAG_compile_unit = 0x11, SD_TAG_structure_type = 0x13,
    SD_TAG_subroutine_type = 0x15, SD_TAG_typedef = 0x16, SD_TAG_union_type = 0x17,
    SD_TAG_unspecified_parameters = 0x18, SD_TAG_inheritance = 0x1c, SD_TAG_inlined_subroutine = 0x1d,
    SD_TAG_ptr_to_member_type = 0x1f, SD_TAG_subrange_type = 0x21, SD_TAG_base_type = 0x24,
    SD_TAG_const_type = 0x26, SD_TAG_enumerator = 0x28, SD_TAG_subprogram = 0x2e, SD_TAG_variable = 0x34,
    SD_TAG_volatile_type = 0x35,
};
enum {
    SD_AT_sibling = 0x01, SD_AT_location = 0x02, SD_AT_name = 0x03, SD_AT_byte_size = 0x0b, SD_AT_bit_offset = 0x0c,
    SD_AT_bit_size = 0x0d, SD_AT_stmt_list = 0x10, SD_AT_low_pc = 0x11, SD_AT_high_pc = 0x12, SD_AT_language = 0x13,
    SD_AT_comp_dir = 0x1b, SD_AT_const_value = 0x1c, SD_AT_upper_bound = 0x2f, SD_AT_abstract_origin = 0x31,
    SD_AT_accessibility = 0x32, SD_AT_containing_type = 0x1d, SD_AT_data_member_location = 0x38,
    SD_AT_decl_file = 0x3a, SD_AT_decl_line = 0x3b, SD_AT_declaration = 0x3c, SD_AT_encoding = 0x3e,
    SD_AT_external = 0x3f, SD_AT_frame_base = 0x40, SD_AT_type = 0x49, SD_AT_virtuality = 0x4c,
    SD_AT_vtable_elem_location = 0x4d, SD_AT_producer = 0x25,
};
enum {
    SD_FORM_addr = 0x01, SD_FORM_block2 = 0x03, SD_FORM_block4 = 0x04, SD_FORM_data2 = 0x05, SD_FORM_data4 = 0x06,
    SD_FORM_data8 = 0x07, SD_FORM_string = 0x08, SD_FORM_block = 0x09, SD_FORM_block1 = 0x0a, SD_FORM_data1 = 0x0b,
    SD_FORM_flag = 0x0c, SD_FORM_sdata = 0x0d, SD_FORM_strp = 0x0e, SD_FORM_udata = 0x0f, SD_FORM_ref_addr = 0x10,
    SD_FORM_ref1 = 0x11, SD_FORM_ref2 = 0x12, SD_FORM_ref4 = 0x13, SD_FORM_ref8 = 0x14, SD_FORM_ref_udata = 0x15,
    SD_FORM_indirect = 0x16,
};
const char *sd_tag_name(unsigned tag); /* "class_type"; NULL when unknown */

/* ---- the input ---- */
typedef struct {
    const u8 *p;
    size_t n;
} SdSec;
typedef struct {
    SdSec abbrev, info, line, loc;
} SdSections;
/* The .debug_* sections of a big-endian ELF32; false when the file is not one or has no .debug_info. */
bool sd_elf_sections(AvpStr elf, SdSections *out);

/* ---- the parsed tree ---- */
#define SD_NONE 0xFFFFFFFFu
enum {
    SD_F_CHILDREN = 1, SD_F_EXTERNAL = 2, SD_F_DECLARATION = 4, SD_F_LOW = 8, SD_F_HIGH = 16, SD_F_LOC = 32,
    SD_F_LOCLIST = 64, SD_F_FRAME = 128, SD_F_CONST = 256, SD_F_UPPER = 512, SD_F_BITS = 1024, SD_F_MEMBER_LOC = 2048,
    SD_F_SIZE = 4096,
};
typedef struct {
    u32 off;           /* offset in .debug_info */
    u32 next;          /* index of the DIE after this one's subtree */
    u32 parent;        /* index of the parent DIE, SD_NONE for a compile unit */
    u32 cu;            /* index into SdInfo.cus */
    u16 tag, flags;
    u8 depth, accessibility, virtuality, encoding;
    const char *name;  /* NULL when absent */
    u32 type;          /* .debug_info offset of DW_AT_type, 0 when absent */
    u32 containing_type;
    u32 byte_size, member_loc;
    u16 bit_size, bit_offset, decl_line, decl_file;
    u32 low, high;     /* pc range (lexical blocks too) */
    u32 loc;           /* .debug_loc offset (SD_F_LOCLIST) */
    const u8 *loc_expr; /* an inline location expression (SD_F_LOC without SD_F_LOCLIST) */
    u32 loc_n;
    u32 frame_base;    /* .debug_loc offset of DW_AT_frame_base */
    i64 const_value, upper_bound;
} SdDie;
typedef struct {
    u32 off, end;      /* header offset and end of the unit in .debug_info */
    u32 die;           /* index of the unit DIE (SD_NONE when the unit is unreadable) */
    const char *name, *producer, *comp_dir;
    u32 low, high, stmt_list;
    bool has_pc, has_stmt;
    u32 language;
} SdCu;
typedef struct {
    SdSections sec;
    SdDie *dies;
    size_t ndies;
    SdCu *cus;
    size_t ncus;
    AvpStrVec notes; /* what the reader could not read, one line each */
} SdInfo;
SdInfo *sd_parse(AvpArena *a, const SdSections *sec);
/* The DIE at a .debug_info offset; NULL when none starts there. */
const SdDie *sd_die_at(const SdInfo *d, u32 off);
static inline u32 sd_index(const SdInfo *d, const SdDie *x) { return (u32)(x - d->dies); }

/* ---- types ---- */
/* The size of a type in bytes (typedefs followed; pointers 4); 0 when unknown. */
u32 sd_type_size(const SdInfo *d, u32 type);
/* A C declaration of `inner` (a declarator, "" for an abstract one) with type `type` (0: void). */
const char *sd_decl(AvpArena *a, const SdInfo *d, u32 type, const char *inner);
static inline const char *sd_type_name(AvpArena *a, const SdInfo *d, u32 type) { return sd_decl(a, d, type, ""); }
bool sd_is_aggregate(unsigned tag);    /* class, struct, union */
bool sd_is_named_type(unsigned tag);   /* class, struct, union, enum, typedef */

/* ---- locations ---- */
/* One location expression as text ("r31", "fb+0x38", "@0x8074F1C0"). */
const char *sd_expr_text(AvpArena *a, const u8 *p, size_t n);
typedef struct {
    u32 begin, end; /* absolute addresses; begin 0 and end SD_NONE: everywhere */
    const char *expr;
} SdLocRange;
typedef AVP_VEC(SdLocRange) SdLocVec;
/* The location list at a .debug_loc offset, its ranges made absolute with the unit's base address. */
SdLocVec sd_loclist(AvpArena *a, const SdInfo *d, u32 off, u32 base);
/* A DIE's location in one line: an inline expression, a single range's expression, or "expr [lo-hi], ...". */
const char *sd_location_text(AvpArena *a, const SdInfo *d, const SdDie *x, u32 (*map)(void *, u32), void *ctx);

/* ---- a function's parameters and locals ---- */
/* The number of formal parameters a subprogram records. */
size_t sd_param_count(const SdInfo *d, const SdDie *fn);
/* The names of the variables of a subprogram, nested lexical blocks included, in DIE order (parameters excluded). */
AvpStrVec sd_local_names(AvpArena *a, const SdInfo *d, const SdDie *fn);
/* The names of `want` that `have` lacks (a multiset difference: two `_result` against one leaves one). */
AvpStrVec sd_names_missing(const AvpStrVec *have, const AvpStrVec *want);

/* ---- line tables ---- */
typedef struct {
    u32 address, line, file;
    bool end_sequence;
} SdLineRow;
typedef AVP_VEC(SdLineRow) SdLineRowVec;
typedef struct {
    const char **files; /* 1-based: files[0] unused; directory joined in */
    size_t nfiles;
    SdLineRowVec rows;
    u8 min_inst;
    bool ok;
    const char *error;
} SdLines;
SdLines sd_lines(AvpArena *a, const SdInfo *d, u32 stmt_list);
const char *sd_file_name(const SdLines *l, u32 file); /* "?" when out of range */

/* ---- the subcommands types, fn and cu (sibling_dwarf_cmd.c) ---- */
#include "sibling.h"
/* Register the three subparsers; `shared` adds the options every sibling subcommand takes. */
void sibling_dwarf_add_parsers(AvpArgSubparsers *sub, void (*shared)(AvpArgParser *));
bool sibling_dwarf_is_cmd(AvpStr cmd);
int sibling_dwarf_run(Sibling *sb, AvpStr cmd, AvpArgs *args, ShsmOutput *out);
