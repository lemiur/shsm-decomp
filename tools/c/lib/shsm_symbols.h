/* config/<ver>/symbols.txt and splits.txt (tools/common/symbols.py). */
#pragma once
#include "avp/map.h"
#include "avp/str.h"
#include "shsm_repo.h"

typedef struct {
    AvpStr key, value;
} ShsmAttr;

/* One symbols.txt line. Strings are NUL-terminated. */
typedef struct {
    const char *name, *section;
    u64 address;
    bool has_size;      /* size is None when false */
    u64 size;
    const char *type;   /* "function", "object", "label" or NULL */
    const char *scope;  /* "global", "local", "weak" or NULL */
    bool has_align;
    i64 align;
    const char *data;   /* "4byte", "string" ... or NULL */
    bool hidden;
    const ShsmAttr *attrs; /* every key:value token as written (later duplicates win in Python's dict: use attr()) */
    size_t nattrs;
    const AvpStr *flags;   /* bare tokens other than "hidden" */
    size_t nflags;
    size_t lineno;      /* 1-based */
    const char *raw;    /* the line as written (no line terminator) */
} ShsmSymbol;

static inline u64 shsm_symbol_end(const ShsmSymbol *s) { return s->address + (s->has_size ? s->size : 0); }
/* Symbol.attrs.get(key): the last value of key, or a NULL view. */
AvpStr shsm_symbol_attr(const ShsmSymbol *s, const char *key);

/* parse_symbol_line: true with *out for a symbol line; false for a blank, comment or unparsable line. A bad
 * size:/align: number raises SHSM_EXC_VALUE as int() did. */
bool shsm_parse_symbol_line(AvpArena *a, AvpStr line, size_t lineno, ShsmSymbol *out);

typedef struct ShsmSplits ShsmSplits;

typedef struct ShsmSymbolTable {
    ShsmSymbol *symbols;   /* file order */
    size_t n;
    const ShsmSplits *splits; /* NULL when not attached */
    /* internal */
    ShsmSymbol **sorted;   /* by (address, -size), stable */
    u64 *addrs;
    u64 max_size;
    AvpMap *by_name;       /* name -> AvpPtrVec * of its symbols, file order */
} ShsmSymbolTable;

/* parse_symbols(text, splits). */
ShsmSymbolTable *shsm_parse_symbols(AvpArena *a, AvpStr text, const ShsmSplits *splits);
/* load_symbols(repo): symbols.txt with splits.txt attached when it exists (OS errors raised as Python did). */
ShsmSymbolTable *shsm_load_symbols(const ShsmRepo *r, bool with_splits);
/* by_name: the first symbol called name, or NULL. */
const ShsmSymbol *shsm_sym_by_name(const ShsmSymbolTable *t, AvpStr name);
/* all_by_name: every symbol called name (file order); returns the count, fills out (heap vector). */
size_t shsm_sym_all_by_name(const ShsmSymbolTable *t, AvpStr name, AvpPtrVec *out);
/* at(address, section): symbols starting at address (section NULL: any), functions and objects before labels,
 * larger first; first_at is its first or NULL. */
size_t shsm_sym_at(const ShsmSymbolTable *t, u64 address, const char *section, AvpPtrVec *out);
const ShsmSymbol *shsm_sym_first_at(const ShsmSymbolTable *t, u64 address, const char *section);
/* containing(address): the innermost sized symbol holding address, or NULL. */
const ShsmSymbol *shsm_sym_containing(const ShsmSymbolTable *t, u64 address);
/* describe(address): "name", "name+0xN" or "0x8XXXXXXX". */
const char *shsm_sym_describe(AvpArena *a, const ShsmSymbolTable *t, u64 address);
/* in_section(section) / functions(): address order (pointers into t->symbols). */
size_t shsm_sym_in_section(const ShsmSymbolTable *t, const char *section, AvpPtrVec *out);
size_t shsm_sym_functions(const ShsmSymbolTable *t, AvpPtrVec *out);
/* in_range(start, end): start <= address < end, address order. Returns a view into the sorted array. */
ShsmSymbol *const *shsm_sym_in_range(const ShsmSymbolTable *t, u64 start, u64 end, size_t *count);
/* unit_of(address): the unit path holding address via the attached splits, or NULL. */
const char *shsm_sym_unit_of(const ShsmSymbolTable *t, u64 address);

/* ---- splits.txt ---- */
typedef struct {
    const char *section;
    u64 start, end;
} ShsmSplitRange;

typedef struct {
    const char *path;         /* unit path, no extension ("game/PadRumble") */
    const char *file;         /* the file name as written ("game/PadRumble.cpp") */
    ShsmSplitRange *ranges;   /* {section: (start, end)} in dict order (a repeated section keeps its first place) */
    size_t nranges, cap;
    ShsmAttr *attrs;          /* the header's extra key:value tokens (dict: later duplicates replace in place) */
    size_t nattrs, attrs_cap;
} ShsmSplitUnit;

typedef struct {
    const char *name;
    ShsmAttr *attrs;
    size_t nattrs, attrs_cap;
} ShsmSplitSection;

typedef struct {
    u64 start, end;
    const char *unit, *section;
} ShsmSpan;

struct ShsmSplits {
    ShsmSplitSection *sections; /* the Sections: block, dict order */
    size_t nsections;
    ShsmSplitUnit *units;       /* dict order (a repeated unit header keeps the first place and is emptied) */
    size_t nunits;
    AvpMap *unit_index;         /* path -> index into units */
    ShsmSpan *spans;            /* every (start, end, unit, section), sorted as Python sorts the tuples */
    size_t nspans;
};

/* parse_splits(text). A bad start:/end: number raises SHSM_EXC_VALUE. */
ShsmSplits *shsm_parse_splits(AvpArena *a, AvpStr text);
/* load_splits(repo). */
ShsmSplits *shsm_load_splits(const ShsmRepo *r);
/* units[unit_path(unit)], or NULL ('main/game/PadRumble', 'game/PadRumble.cpp' ... accepted). */
const ShsmSplitUnit *shsm_splits_unit(const ShsmSplits *s, const char *unit);
/* range_of(address): the span holding address, or NULL; unit_of is its unit or NULL. */
const ShsmSpan *shsm_splits_range_of(const ShsmSplits *s, u64 address);
const char *shsm_splits_unit_of(const ShsmSplits *s, u64 address);
