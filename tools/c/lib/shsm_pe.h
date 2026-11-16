/* shsm_pe: Win32 PE headers and the Metrowerks CodeView NB11 reader of tools/mwcc_trace/symbols.py (SymbolTable,
 * load, parse, _strip, version_of). lib/shsm_pe.md documents the module. */
#pragma once
#include "avp/avp.h"

/* Exceptions (avp_throw codes; the message is the Python's str(e)):
 *   SHSM_PE_EXC_NO_DEBUG_INFO  symbols.NoDebugInfo ("not a PE file", "no PE debug directory", ...)
 *   SHSM_PE_EXC_STRUCT         struct.error (a read past the end of the file or blob)
 *   SHSM_PE_EXC_INDEX          IndexError (a name length byte past the end, a segment number with no section)
 * A file that cannot be read throws AVP_ERR_IO with str(OSError) ("[Errno 2] No such file or directory: 'x'"). */
enum {
    SHSM_PE_EXC_NO_DEBUG_INFO = 0x5045, /* "PE" */
    SHSM_PE_EXC_STRUCT,
    SHSM_PE_EXC_INDEX
};
/* "NoDebugInfo", "struct.error", "IndexError" for the codes above, "OSError" for AVP_ERR_IO, else "Exception". */
const char *shsm_pe_exc_class(int code);

/* ---- PE headers ------------------------------------------------------------------------------------------------ */
typedef struct {
    char name[9];   /* the 8 name bytes up to the first NUL (pe_disasm: rstrip(b"\0"), latin-1) */
    u32 vsize, va;  /* VirtualSize, VirtualAddress (an RVA) */
    u32 rsize, raw; /* SizeOfRawData, PointerToRawData */
    u32 ch;         /* Characteristics */
} ShsmPeSection;

typedef struct {
    u32 pe;                  /* e_lfanew */
    bool pe_sig;             /* the four bytes at pe are "PE\0\0" */
    u16 nsec, optsz;
    u32 opt;                 /* file offset of the optional header */
    u32 base;                /* ImageBase */
    u32 ndd;                 /* NumberOfRvaAndSizes (read only with SHSM_PE_READ_NDD) */
    u32 dbg_rva, dbg_size;   /* data directory 6 (debug), 0/0 when ndd <= 6 */
    ShsmPeSection *secs;
    size_t nsecs;
} ShsmPeHeaders;

/* Which fields to read, in the order and with the bounds the two Python readers use (a short file raises
 * struct.error at the first field that does not fit):
 *   SHSM_PE_SYMBOLS   symbols._pe_sections: e_lfanew, signature (NoDebugInfo "not a PE file"), nsec, optsz, base,
 *                     ndd, 24 bytes per section header, debug directory;
 *   SHSM_PE_DISASM    pe_disasm.PE: e_lfanew, nsec, optsz, base, 40 bytes per section header (no signature check). */
typedef enum { SHSM_PE_SYMBOLS, SHSM_PE_DISASM } ShsmPeRead;
void shsm_pe_headers(AvpArena *a, const u8 *d, size_t n, ShsmPeRead how, ShsmPeHeaders *out);

/* struct.unpack_from bounds: throws SHSM_PE_EXC_STRUCT with CPython's text when off + size > n. */
u32 shsm_pe_u32(const u8 *d, size_t n, u64 off);
u16 shsm_pe_u16(const u8 *d, size_t n, u64 off);

/* ---- NB11 symbol table ------------------------------------------------------------------------------------------ */
typedef struct {
    i64 va;
    i64 size;      /* exact (procedure record) or the distance to the next named symbol, capped at the module run */
    AvpStr name;   /* decoded latin-1 -> UTF-8, without x86 decoration (_strip) */
    AvpStr module; /* the .obj name, "?" when no module range holds va */
} ShsmPeFunc;
typedef struct {
    i64 lo, hi;
    AvpStr module;
} ShsmPeRange;
typedef struct {
    i64 va;
    AvpStr name;
} ShsmPeDatum;

typedef struct ShsmPeSymbols {
    u32 base;
    ShsmPeFunc *funcs; /* sorted by va */
    size_t nfuncs;
    ShsmPeRange *ranges; /* sorted (lo, hi, module) */
    size_t nranges;
    ShsmPeDatum *data; /* public symbols outside .text, sorted (va, name) */
    size_t ndata;
} ShsmPeSymbols;

/* symbols.parse(d) / symbols.load(path). Throws as described above. */
ShsmPeSymbols *shsm_pe_symbols_parse(AvpArena *a, const u8 *d, size_t n);
ShsmPeSymbols *shsm_pe_symbols_load(AvpArena *a, const char *exe);

/* SymbolTable.lookup: the function whose start is the greatest <= va, NULL before the first. */
const ShsmPeFunc *shsm_pe_symbols_lookup(const ShsmPeSymbols *t, i64 va);
/* SymbolTable.find: the functions whose lowered name equals lower(text), else those containing it (table order). */
size_t shsm_pe_symbols_find(AvpArena *a, const ShsmPeSymbols *t, AvpStr text, const ShsmPeFunc ***out);
/* SymbolTable.module_at: the module of the first range holding va; .p == NULL for None. */
AvpStr shsm_pe_symbols_module_at(const ShsmPeSymbols *t, i64 va);
/* SymbolTable.describe: "name (M.obj)", "name+0x10 (M.obj)", "static in M.obj ..." or the empty AvpStr for None. */
AvpStr shsm_pe_symbols_describe(AvpArena *a, const ShsmPeSymbols *t, i64 va);
/* SymbolTable.describe_data: the name of the public data symbol exactly at va, or the empty AvpStr for None. */
AvpStr shsm_pe_symbols_describe_data(const ShsmPeSymbols *t, i64 va);

/* symbols._strip: drop a cdecl leading '_' and a stdcall '@N' suffix ('?' names unchanged). A view into name. */
AvpStr shsm_pe_strip(AvpStr name);
/* symbols.version_of(path) for the checkout root `root`: the directory below <root>\tools\compilers with separators
 * turned into '-' ("Wii-1.0"), else the parent folder's name, else "compiler". */
char *shsm_pe_version_of(AvpArena *a, const char *root, const char *path);
