/* query.exe: tools/query.py, questions about the retail binary and the project answered from the asm index.
 *
 * The importable part of query.py, for the tools that link tool_query (brief and others): the context (Ctx,
 * Ctx.for_root), symbol resolution (resolve), describe_sym / sym_line, the retail DOL reader, summarize, row_note,
 * the value formatters (cstr, _fmt_float) and the notes search (doc_notes and its helpers, query_notes.h). Every
 * subcommand is query_cmd_<name>(ctx, args, out), as cmd_<name>.
 *
 * None: integers SHSM_NONE, strings p == NULL / NULL pointers. Errors are raised as the Python raised them
 * (cli.UsageError is SHSM_EXC_USAGE). Allocations come from ctx->a (the thread's long-lived arena). */
#pragma once
#include "avp/avp.h"
#include "shsm_asmindex.h"
#include "shsm_cli.h"
#include "shsm_mangle.h"
#include "shsm_re.h"
#include "shsm_repo.h"
#include "shsm_report.h"
#include "shsm_symbols.h"
#include "shsm_units.h"

/* ---- the retail DOL (query.Dol) ---- */
typedef struct {
    u32 address, size, offset;
    bool text;
} QueryDolSection;
typedef struct {
    AvpStr data;
    QueryDolSection sections[18];
    size_t nsections;
    u32 bss, bss_size;
} QueryDol;
/* Dol.load(path): NULL when the file cannot be read. */
QueryDol *query_dol_load(AvpArena *a, const char *path);
/* Dol.read(address, size): bytes at [address, address+size), zeros inside .bss; p NULL when unmapped. */
AvpStr query_dol_read(AvpArena *a, const QueryDol *d, i64 address, i64 size);

/* ---- context (query.Ctx) ---- */
typedef struct {
    i64 address, size;
    AvpStr name, unit;
} QuerySpan;

typedef struct QueryCtx {
    AvpArena *a;
    const ShsmRepo *repo;
    const char *root;           /* --root as given (NULL: the cwd's checkout) */
    ShsmAsmIndex *ix_;          /* lazily opened: use query_ix() */
    const ShsmUnits *units_;    bool units_loaded;
    ShsmReport *report_;        bool report_loaded;
    ShsmSymbolTable *syms_;
    QueryDol *dol_;             bool dol_loaded;
    AvpMap *symcache;           /* name -> ShsmSymRow * (NULL: None) */
    QuerySpan *spans;           size_t nspans; bool spans_loaded;
    AvpMap *status;             /* unit path -> status (configure.py) */
    AvpIMap *ref_targets;       /* from_addr -> to_addr (SHSM_NONE: None) */
    AvpMap *items;              /* object name -> (QueryItems *) */
    void *extra[8];             /* per-command caches (classes.json ...) */
} QueryCtx;

/* Ctx(args) / Ctx.for_root(root): root NULL means the checkout around the cwd. Repo.find raises as the Python. */
QueryCtx *query_ctx_for_root(AvpArena *a, const char *root);
void query_ctx_close(QueryCtx *ctx);
ShsmAsmIndex *query_ix(QueryCtx *ctx);            /* AsmIndex.open_readonly; FileNotFoundError -> UsageError */
const ShsmUnits *query_units(QueryCtx *ctx);      /* load_units, or an empty table on OSError/ValueError */
const ShsmReport *query_report(QueryCtx *ctx);    /* load_report, or NULL on OSError/ValueError */
ShsmSymbolTable *query_syms(QueryCtx *ctx);       /* load_symbols(repo), splits attached */
const QueryDol *query_dol(QueryCtx *ctx);         /* Dol.load(repo.retail_dol), NULL when unreadable */

/* ctx.sym(name): the SymRow of an index name (cached), or NULL (also for an empty or NULL name). */
const ShsmSymRow *query_sym(QueryCtx *ctx, AvpStr name);
/* ctx.base(name): the symbol's address, or SHSM_NONE. */
i64 query_base(QueryCtx *ctx, AvpStr name);
/* ctx.locate(address): (symbol, its address, unit name) of the function/object holding address; between symbols
 * name.p NULL and *sym_addr SHSM_NONE, *unit the previous symbol's unit name or p NULL. */
AvpStr query_locate(QueryCtx *ctx, i64 address, i64 *sym_addr, AvpStr *unit);
/* ctx.status(unit): 'Matching', 'NonMatching', 'Asm' ... for 'main/x' or 'x', or '?'. */
const char *query_status(QueryCtx *ctx, AvpStr unit);
/* ctx.percent(name, unit): objdiff's fuzzy percent, false when None. unit p NULL: any unit. */
bool query_percent(QueryCtx *ctx, AvpStr name, AvpStr unit, double *pct);

/* ---- small helpers ---- */
AvpStr query_pct(AvpArena *a, bool has, double p);  /* pct(p): "  -   " or "%5.1f%%" */
const char *query_mark(const char *status);        /* "M" for Matching, else "." */
bool query_is_placeholder(AvpArena *a, AvpStr name);
AvpStr query_demangled(AvpArena *a, AvpStr name);   /* demangle(name) when it differs from name, else p NULL */
i64 query_parse_addr(AvpStr text);                  /* ADDR_RE on text.strip(): the address, or SHSM_NONE */
i64 query_parse_int(AvpArena *a, AvpStr text);      /* parse_int: hex, -0x8, 0d100; UsageError "not a number: x" */
AvpStr query_mangle_base_name(AvpArena *a, AvpStr name); /* mangle.parse(name).name, p NULL: None */
/* cstr(text, width): a C string literal of text; width 0: no cut. */
AvpStr query_cstr(AvpArena *a, AvpStr text, i64 width);
/* _fmt_float(v, single): the shortest C literal that reads back as the same float/double; has false: "0". */
AvpStr query_fmt_float(AvpArena *a, bool has, double v, bool single);
AvpStr query_hex8(AvpArena *a, i64 v);              /* "0x%08X" */

/* ---- resolution ---- */
/* resolve(ctx, text, kind): kind NULL for any. Raises SHSM_EXC_USAGE as the Python. */
const ShsmSymRow *query_resolve(QueryCtx *ctx, AvpStr text, const char *kind);

/* describe_sym(ctx, name, addr): name p NULL for None; addr SHSM_NONE for None. */
typedef struct {
    AvpStr name, demangled, unit, kind;  /* p NULL: None */
    i64 address;                         /* SHSM_NONE: None */
    const char *status;
    bool placeholder;
    bool has_percent;
    double percent;
} QuerySymDesc;
QuerySymDesc query_describe_sym(QueryCtx *ctx, AvpStr name, i64 addr);
/* The dict as JSON: name, demangled, address, kind, unit, status, placeholder, percent (Python key order). */
AvpJson *query_desc_json(AvpArena *a, const QuerySymDesc *d);
/* sym_line(d, extra). */
AvpStr query_sym_line(AvpArena *a, const QuerySymDesc *d, AvpStr extra);

/* ---- fn ---- */
typedef struct {
    i64 instructions, calls, tail_calls, branches, frame;
    i64 min_gpr, min_fpr;  /* -1: None */
    bool floats;
} QuerySummary;
QuerySummary query_summarize(const ShsmAsmRow *rows, size_t n);
AvpJson *query_summary_json(AvpArena *a, const QuerySummary *s);
/* resolved_target(ctx, address): the index's to_addr of the reference made at address, or SHSM_NONE. */
i64 query_resolved_target(QueryCtx *ctx, i64 address);
/* row_note(ctx, r): "" or the callee's demangled name, "= <float>" or "= <string>". */
AvpStr query_row_note(QueryCtx *ctx, const ShsmAsmRow *r);
/* unit_header(ctx, s): "main/x (status, library, source)". */
AvpStr query_unit_header(QueryCtx *ctx, const ShsmSymRow *s);
/* An index SQL query with params given as JSON values; rows as a JSON array of arrays. */
AvpJson *query_sql(QueryCtx *ctx, const char *sql, size_t nparams, ...);

/* ---- data items, units (shared by data, addr, vtable, strings, unit) ---- */
/* block_items(ctx, name): the items of a data object by address (cached; empty unless name is an object). */
typedef struct {
    ShsmAsmBlock *block;   /* NULL when not an object */
    AvpIMap *by_addr;      /* address -> ShsmAsmItem * (the last item at an address, as the dict) */
} QueryItems;
const QueryItems *query_block_items(QueryCtx *ctx, AvpStr name);
const ShsmAsmItem *query_item_at(const QueryItems *items, i64 address); /* NULL: absent */
/* _ptr_text(ctx, sym, off) and _item_text(ctx, it): an item as C initialiser text. */
AvpStr query_ptr_text(QueryCtx *ctx, AvpStr sym, i64 off);
AvpStr query_item_text(QueryCtx *ctx, const ShsmAsmItem *it);
/* str(value) of an item value (None, int, float repr, str). */
AvpStr query_val_str(AvpArena *a, const ShsmAsmVal *v);
/* find_unit(ctx, text): the unit text names exactly (any spelling), or NULL. */
const ShsmUnit *query_find_unit(QueryCtx *ctx, AvpStr text);

/* ---- subcommands: cmd_<name>(ctx, args, out) -> exit code ---- */
int query_cmd_fn(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out);
int query_cmd_callers(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out);
int query_cmd_callees(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out);
int query_cmd_xref(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out);
int query_cmd_strings(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out);
int query_cmd_find_string(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out);
int query_cmd_find_float(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out);
int query_cmd_find_const(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out);
int query_cmd_data(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out);
int query_cmd_addr(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out);
int query_cmd_unit(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out);
int query_cmd_vtable(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out);
int query_cmd_notes(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out);

/* query.main(argv): the CLI (build_parser + run). */
int query_main(int argc, char **argv);
