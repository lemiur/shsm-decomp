/* status: a port of python tools/status.py (where the project stands: per-library progress, per-unit status,
 * changes between reports, an SVG chart). The world model, the library filter and pct come from tool_pick; the
 * headline model (configure.py, docs/exceptions.md, the rule checker) from tool_ledger. Rows and items are JSON
 * objects with the Python's dict keys in the Python's order (they are what --json prints). */
#pragma once
#include "avp/avp.h"
#include "ledger.h"
#include "pick.h"

#define STATUS_NKINDS 4
extern const char *const STATUS_KINDS[STATUS_NKINDS]; /* matched, exact, progress, unstarted */

/* ---- per-library table ---- */
typedef struct {
    const char *library;
    i64 functions, bytes;
    i64 n[STATUS_NKINDS], b[STATUS_NKINDS]; /* per kind: functions, bytes */
    /* ORIGINAL-ASM, linked, listed apart as the ledger does (docs/exceptions.md rows and `.s` units): not counted in
     * n[0]/b[0]; functions and bytes include them */
    i64 asm_n, asm_b;
} StatusRow;
typedef AVP_VEC(StatusRow *) StatusRows;

int status_kind_of(const PickFn *f); /* index into STATUS_KINDS */
/* library_table(world, lib, detail): the rows (sorted) and the total row. lib NULL or "": every library. */
void status_library_table(AvpArena *a, PickWorld *w, const char *lib, bool detail, StatusRows *rows,
                          StatusRow **total);
AvpJson *status_row_json(AvpArena *a, const StatusRow *r); /* the row dict */
double status_pc(i64 a, i64 b);
AvpStrVec status_fmt_table(AvpArena *a, const StatusRows *rows, const StatusRow *total);

/* ---- headline ---- */
/* ledger_block(root): {'rules': [...], 'bytes': ..., 'asm': ..., 'linked': ..., 'units': ...} (first-seen key
 * order) from docs/progress.md's numbers block, or NULL (None) without the file or the block. */
AvpJson *status_ledger_block(AvpArena *a, const char *root);

typedef struct {
    i64 rules[4], bytes[4], asm_[2], linked[2], units[2]; /* computed (tools/ledger.py's model) */
    AvpJson *ledger;                                      /* ledger_block or {} */
    i64 progress[7]; /* matched_code, total_code, matched_functions, total_functions, complete_code, complete_units,
                        total_units */
    AvpStrVec problems;
    const char *note; /* NULL: None */
} StatusHeadline;

/* The rule checker runner: tests replace it; NULL restores the default, which runs the rule checker in-process
 * (`check_rules --json --root <root>`, tool_check_rules). */
typedef struct {
    int returncode;
    AvpStr out, err;
} StatusCheckerRun;
typedef StatusCheckerRun (*StatusCheckerRunner)(void *ctx, AvpArena *a, const char *root);
void status_set_checker_runner(StatusCheckerRunner fn, void *ctx);

/* headline(world, report_path, checker_json, no_checker) */
StatusHeadline *status_headline(AvpArena *a, PickWorld *w, const char *report_path, const char *checker_json,
                                bool no_checker);
AvpJson *status_headline_json(AvpArena *a, const StatusHeadline *h);
AvpStrVec status_fmt_headline(AvpArena *a, const StatusHeadline *h);

/* ---- --units ---- */
/* unit_rows(world, lib, pending): pending maps unit path -> count (NULL: none). Rows are the Python's dicts. */
typedef AVP_VEC(AvpJson *) StatusJsonVec;
StatusJsonVec status_unit_rows(AvpArena *a, PickWorld *w, const char *lib, AvpMap *pending);
AvpStr status_fmt_unit(AvpArena *a, const AvpJson *r);
AvpMap *status_pending_by_unit(AvpArena *a, PickWorld *w);

/* ---- --since ---- */
/* load_old_report(world, ref): a report file, else `git show <ref>:<baseline>` (SHSM_EXC_USAGE when neither). */
ShsmReport *status_load_old_report(AvpArena *a, PickWorld *w, const char *ref, const char **name);
/* since(world, old, lib): {gained, lost, improved, regressed, added, removed, flipped_on, flipped_off} */
AvpJson *status_since(AvpArena *a, PickWorld *w, const ShsmReport *old, const char *lib);
AvpStrVec status_fmt_since(AvpArena *a, const AvpJson *res, const char *old_name);

/* ---- --svg ---- */
extern const char *const STATUS_SVG_COLORS[STATUS_NKINDS];
extern const char *const STATUS_SVG_TEXT_COLORS[3]; /* SVG_TEXT, SVG_MUTED, SVG_NOTE */
double status_text_w(AvpStr text, double size, bool bold);
AvpStr status_human(AvpArena *a, i64 n);
/* render_svg(rows, total, headline_text): headline_text NULL for the default */
AvpStr status_render_svg(AvpArena *a, const StatusRows *rows, const StatusRow *total, const char *headline_text);

/* ---- front end ---- */
int status_main(int argc, char **argv);
