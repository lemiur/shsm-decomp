/* Parser of decomp-toolkit's retail asm listings (tools/common/asm.py): build/<ver>/asm/**.s files into functions with
 * instruction rows, data objects with typed items, references, strings, floats and section runs; and the listings
 * the last dtk split names (asm.listings). Documented in shsm_asm.md.
 *
 * Python None: integer fields hold SHSM_NONE, string fields have p == NULL. Strings are views into the parsed
 * file's arena (ShsmAsmFile.arena), valid until shsm_asm_free. */
#pragma once
#include "avp/arena.h"
#include "avp/json.h"
#include "avp/map.h"
#include "avp/str.h"
#include "avp/vec.h"

#define SHSM_NONE INT64_MIN

/* Reference kinds (asm.CALL ...): static strings, compare with strcmp. */
extern const char SHSM_REF_CALL[], SHSM_REF_BRANCH[], SHSM_REF_HA[], SHSM_REF_LO[], SHSM_REF_HI[],
    SHSM_REF_SDA21[], SHSM_REF_PTR[], SHSM_REF_JUMPTABLE[];

typedef enum { SHSM_VAL_NONE, SHSM_VAL_INT, SHSM_VAL_FLOAT, SHSM_VAL_STR } ShsmValType;
typedef struct {
    ShsmValType t;
    i64 i;
    double f;
    AvpStr s;
} ShsmAsmVal;

/* asm.Row: word SHSM_NONE unless 8 hex digits; reloc NULL or "ha" "l" "h" "sda21" "sda2" "sdarel" "got" "rel24";
 * label SHSM_NONE or the branch target. */
typedef struct {
    i64 address, word;
    AvpStr bytes, mnemonic, operands, sym;
    i64 sym_off;
    const char *reloc;
    i64 label;
} ShsmAsmRow;

/* asm.Item: kind "4byte" "2byte" "byte" "float" "double" "string" "string16" "skip" "ptr" "rel"; sym p NULL when
 * Python's sym is None (then offset is 0 and not reported). */
typedef struct {
    i64 address;
    const char *kind;
    ShsmAsmVal value;
    AvpStr sym;
    i64 offset, size;
} ShsmAsmItem;

typedef struct {
    i64 address;
    AvpStr name; /* ".L_80001014" */
} ShsmAsmLabel;

/* asm.Block: kind "function" "object" "label". rows is filled unless the file was parsed with SHSM_ASM_NO_ROWS;
 * nrows counts the rows either way. labels keep dict order (first insertion's position, last value). */
typedef struct {
    const char *kind;
    AvpStr name, scope, section, demangled;
    i64 address, size;
    bool hidden;
    i64 line_start, line_end, byte_start, byte_end;
    AVP_VEC(ShsmAsmRow) rows;
    size_t nrows;
    AVP_VEC(ShsmAsmLabel) labels;
    AVP_VEC(ShsmAsmItem) items;
} ShsmAsmBlock;

/* asm.Ref. owner_addr, to_base, to_addr SHSM_NONE when unknown; from_sym and section p NULL for None. */
typedef struct {
    AvpStr from_sym;
    i64 from_addr;
    AvpStr to_sym;
    i64 to_off;
    const char *kind;
    i64 owner_addr, to_base, to_addr;
    AvpStr section;
} ShsmAsmRef;

typedef struct {
    AvpStr owner; /* p NULL: loose data */
    i64 address;
    AvpStr text;  /* UTF-8 (may hold NUL bytes) */
    const char *kind;
} ShsmAsmString;

typedef struct {
    AvpStr owner;
    i64 address;
    const char *kind;
    bool has_value; /* false: Python's None (float() failed) */
    double value;
    AvpStr text;
} ShsmAsmFloat;

typedef struct {
    AvpStr name;
    i64 start, end;
} ShsmAsmSection;

/* asm.AsmFile; blocks in Python's order (a .sym label before its enclosing block). */
typedef struct {
    const char *path, *unit;
    AvpArena *arena;
    AVP_VEC(ShsmAsmBlock *) blocks;
    AVP_VEC(ShsmAsmRef) refs;
    AVP_VEC(ShsmAsmString) strings;
    AVP_VEC(ShsmAsmFloat) floats;
    AVP_VEC(ShsmAsmSection) sections;
} ShsmAsmFile;

enum { SHSM_ASM_NO_ROWS = 1 }; /* count instruction rows without keeping them (the index needs the count only) */

/* asm.parse_text(text, path, unit): text is copied. path/unit may be NULL (""). */
ShsmAsmFile *shsm_asm_parse_text(AvpStr text, const char *path, const char *unit, unsigned flags);
/* asm.parse_file(path, unit): raises the fs.h failure (shsm_raise_io) when the file cannot be read. */
ShsmAsmFile *shsm_asm_parse_file(const char *path, const char *unit, unsigned flags);
void shsm_asm_free(ShsmAsmFile *af);

/* af.function(name) / af.object(name) / af.block(name): the first such block, or NULL. */
ShsmAsmBlock *shsm_asm_function(const ShsmAsmFile *af, AvpStr name);
ShsmAsmBlock *shsm_asm_object(const ShsmAsmFile *af, AvpStr name);
ShsmAsmBlock *shsm_asm_block(const ShsmAsmFile *af, AvpStr name);

/* asm.read_block(path, byte_start, byte_end, section): the first non-label block of the fragment (else the first
 * block), with section filled in when the fragment has none; NULL when the fragment has no block. The block lives
 * in *out_file (free it with shsm_asm_free). */
ShsmAsmBlock *shsm_asm_read_block(const char *path, i64 byte_start, i64 byte_end, AvpStr section,
                                  ShsmAsmFile **out_file);

/* Helpers, exposed for tests. */
AvpStr shsm_asm_unquote(AvpArena *a, AvpStr name);
AvpStr shsm_asm_decode_string(AvpArena *a, AvpStr lit);          /* bytes of a GNU as literal body */
AvpStr shsm_asm_bytes_to_text(AvpArena *a, AvpStr bytes);        /* UTF-8, else cp932, else Latin-1 -> UTF-8 */
void shsm_asm_parse_operands(AvpArena *a, ShsmAsmRow *row);      /* fills sym, sym_off, reloc, label */

/* to_dict() of each type as JSON (Row.to_dict, Item.to_dict, Block.to_dict(rows)); a ref as the list
 * [from_sym, from_addr, to_sym, to_off, kind, owner_addr, to_base, to_addr, section]. */
AvpJson *shsm_asm_row_json(AvpArena *a, const ShsmAsmRow *r);
AvpJson *shsm_asm_item_json(AvpArena *a, const ShsmAsmItem *it);
AvpJson *shsm_asm_block_json(AvpArena *a, const ShsmAsmBlock *b, bool rows);
AvpJson *shsm_asm_ref_json(AvpArena *a, const ShsmAsmRef *r);
AvpJson *shsm_json_opt_int(AvpArena *a, i64 v);  /* SHSM_NONE -> null */
AvpJson *shsm_json_opt_str(AvpArena *a, AvpStr s); /* p NULL -> null */

/* ---- which listings are current ---- */
/* asm.listing_rel(unit): 'game/PadRumble.cpp' -> 'game/PadRumble.s'. */
const char *shsm_asm_listing_rel(AvpArena *a, const char *unit);

/* asm.Listings: files (rel -> absolute path, sorted by rel), stale (sorted rel paths), record (NULL when absent or
 * unusable), note (NULL when none). */
typedef struct {
    AvpStrVec rels, paths; /* parallel */
    AvpStrVec stale;
    const char *record;
    const char *note;
} ShsmListings;
/* asm.split_record(asm_dir): the record's path; *named (a set of rel paths) NULL when unusable. */
const char *shsm_asm_split_record(AvpArena *a, const char *asm_dir, AvpSet **named);
const char *shsm_asm_stale_note(AvpArena *a, const AvpStrVec *stale, const char *record, size_t limit);
ShsmListings *shsm_asm_listings(AvpArena *a, const char *asm_dir);
