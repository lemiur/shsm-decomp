/* ledger: a port of python tools/ledger.py (regenerate the generated blocks of docs/progress.md, check
 * docs/exceptions.md). The pieces mirror the Python's functions; the hand scanners stand in for its regular
 * expressions (DEVIATIONS.md, "Consolidation"). */
#pragma once
#include "avp/avp.h"

/* ---- scanners (ledger_scan.c) ---- */
AvpStr ledger_unit_key(AvpArena *a, AvpStr path);                  /* unit_key */
bool ledger_is_asm_source(AvpStr path); /* an assembly source unit (`.s`): every function in it is ORIGINAL-ASM */
typedef struct {
    AvpStr status, unit, path;
} LedgerCfg;
typedef AVP_VEC(LedgerCfg) LedgerCfgVec;
LedgerCfgVec ledger_parse_configure(AvpArena *a, AvpStr text); /* parse_configure */
typedef struct {
    AvpStr section;
    i64 addr;
    bool has_size;
    i64 size;
    AvpStr attrs;
} LedgerSym;
typedef AVP_VEC(LedgerSym) LedgerSymVec;
AvpMap *ledger_parse_symbols(AvpArena *a, AvpStr text); /* name -> LedgerSymVec* (parse_symbols) */
AvpStrVec ledger_table_cells(AvpArena *a, AvpStr line);   /* table_cells */
AvpStrVec ledger_ticks(AvpStr cell);                      /* ticks: re.findall(r"`([^`]+)`") */
bool ledger_is_placeholder(AvpStr name);                  /* PLACEHOLDER_RE.search */
bool ledger_thunk_target(AvpStr name, AvpStr *target);    /* THUNK_RE.match */
bool ledger_unconfirmed_text(AvpStr text);                /* UNCONFIRMED_RE.search */
/* parse_status_cell: base, and note (note->p == NULL for None). */
void ledger_parse_status_cell(AvpStr cell, AvpStr *base, AvpStr *note);
/* ROW_RE.match: the six groups. */
bool ledger_match_row(AvpStr line, AvpStr g[6]);
/* re.search(r"\((\d+) functions?, (\d+) bytes\)", s) */
bool ledger_count_note(AvpStr s, AvpStr *nfn, AvpStr *nbytes);
/* re.findall(r"R\d+", s) */
AvpStrVec ledger_rule_ids(AvpStr s);
/* re.search(r"\d+ functions and \d+ code bytes(?= in all)", s): the match span */
bool ledger_find_gross(AvpStr s, size_t *start, size_t *end);
/* re.findall(r"[A-Za-z_@\\][\w\\@]*", text) into the set */
void ledger_source_words(AvpStr text, AvpSet *out);

/* ---- exceptions record (ledger_model.c) ---- */
typedef struct {
    const char *table; /* "MATCH-PENDING", "ORIGINAL-ASM", "helpers", "rulings" */
    i64 line;
    AvpStrVec cells;
    AvpStrVec *ticks; /* one vector per cell */
    AvpStrVec units, functions;
} LedgerExcRow;
typedef AVP_VEC(LedgerExcRow *) LedgerExcRows;
LedgerExcRows ledger_parse_exceptions(AvpArena *a, AvpStr text);

/* ---- model ---- */
typedef struct {
    AvpStr unit, name, alias;
    i64 addr, size;
    const AvpJson *pct; /* NULL: None */
    const char *section;
    bool linked;
    const char *kind; /* matched | ORIGINAL-ASM | MATCH-PENDING */
    AvpStrVec rules;
    bool asm_fn, asm_block, pending, in_asm_table;
} LedgerFn;
typedef AVP_VEC(LedgerFn *) LedgerFnVec;
typedef struct {
    AvpStr name;
    const char *section;
    bool linked;
    AvpStr status; /* p == NULL: None */
    AvpStr path;
    bool unconfirmed;
    LedgerFnVec fns;
} LedgerUnit;
typedef AVP_VEC(LedgerUnit *) LedgerUnitVec;
typedef struct {
    i64 gross_n, gross_b, asm_n, asm_b, total_n, total_b, linked_n, linked_b, pend_n, pend_b, bytes_n, bytes_b,
        rules_n, rules_b, units_total, units_complete;
} LedgerNumbers;
typedef struct {
    AvpMap *units; /* name -> LedgerUnit*, report order */
    LedgerNumbers numbers;
    AvpStrVec problems;
} LedgerModel;
i64 ledger_unit_first_addr(const LedgerUnit *u);
/* build_model; symbols NULL: None. */
LedgerModel *ledger_build_model(AvpArena *a, const AvpJson *report, const LedgerCfgVec *configure,
                                const LedgerExcRows *exc, const AvpJson *checker, const char *root, AvpMap *symbols);
/* str() of a JSON value as Python's %s prints it (None, True, digits, float repr, the string). */
AvpStr ledger_py_str(AvpArena *a, const AvpJson *v);
/* int() of a JSON value (an int or a str of digits). */
i64 ledger_py_int(const AvpJson *v);

/* ---- rendering (ledger_render.c) ---- */
AvpStr ledger_fmt_pct(AvpArena *a, double p);
AvpStrVec ledger_auto_naming(AvpStr name);
const char *ledger_note_class(AvpStr note); /* "pending", "asm", "helper" or NULL */
AvpStr ledger_status_cell(AvpArena *a, const LedgerFn *f, AvpStr old_base, AvpStr old_note);
LedgerUnitVec ledger_order_units(AvpArena *a, const LedgerUnitVec *units, const AvpStrVec *old_order);
/* generate: how[4] in the order status, game, numbers, gross ("marker", "located", "located+marked" or NULL). */
AvpStr ledger_generate(AvpArena *a, AvpStr text, const LedgerModel *m, AvpStrVec *warnings, bool add_markers,
                       const char *how[4], i64 *old_count, i64 *new_count);

/* ---- front end (ledger_cli.c) ---- */
/* main(argv): the exit code; stdout and stderr text go to out and err. */
int ledger_main(int argc, char **argv, AvpBuf *out, AvpBuf *err);
