/* objdiff report files (tools/common/report.py): build/<ver>/report.json and config/<ver>/baseline_report.json.
 * Ported: the units and functions with their scores and lookups. Not ported: Measures as objects (the raw JSON is
 * kept), categories, sections, compare_reports. match.py itself reads no report; this is for the tools after it. */
#pragma once
#include "avp/json.h"
#include "avp/map.h"
#include "shsm_repo.h"

typedef struct {
    const char *name, *unit;  /* unit: the unit name */
    i64 size;                 /* bytes (_num: int of a number or digit string) */
    double fuzzy;             /* match percent, 0.0 when absent */
    bool scored;              /* false when the report gives no percentage */
    bool has_address;         /* metadata.virtual_address present */
    u64 address;
    bool has_offset;          /* "address" (section offset) present */
    i64 offset;
    const char *demangled;    /* metadata.demangled_name or NULL */
    AvpJson *metadata;        /* raw ({} when absent) */
} ShsmFunctionReport;

typedef struct {
    const char *name;         /* "main/game/PadRumble" */
    double fuzzy;             /* measures.fuzzy_match_percent (0.0 when absent) */
    bool complete, auto_generated;
    const char *source_path;  /* or NULL */
    ShsmFunctionReport *functions; /* every entry in report order (repeated names included) */
    size_t nfunctions;
    AvpMap *first_by_name;    /* name -> index of its first entry */
    AvpJson *measures, *metadata; /* raw */
} ShsmUnitReport;

typedef struct {
    const char *path;
    ShsmUnitReport **units;   /* report order (a repeated unit name: the last entry, at the first place) */
    size_t nunits;
    AvpMap *index;            /* unit name -> index */
    AvpMap *fn_first;         /* function name -> ShsmFunctionReport * (first unit that has it) */
    AvpJson *measures;
} ShsmReport;

/* read_report(path): raises the OSError / ValueError reading it raised. */
ShsmReport *shsm_read_report(const char *path);
/* load_report(repo): build/<ver>/report.json; SHSM_EXC_FILE_NOT_FOUND "no report at <path> (build it with ninja
 * first)" when absent. */
ShsmReport *shsm_load_report(const ShsmRepo *r);
/* load_baseline(repo): config/<ver>/baseline_report.json. */
ShsmReport *shsm_load_baseline(const ShsmRepo *r);
/* Report.unit(unit): the UnitReport for 'main/x', 'x' or 'x.c', or NULL. */
const ShsmUnitReport *shsm_report_unit(const ShsmReport *rep, const char *unit);
/* Report.function(name, unit): by symbol name within unit (unit NULL: the first unit that has it), or NULL. */
const ShsmFunctionReport *shsm_report_function(const ShsmReport *rep, const char *name, const char *unit);
/* UnitReport.functions.get(name): the first entry of name in the unit, or NULL. */
const ShsmFunctionReport *shsm_unit_report_function(const ShsmUnitReport *u, const char *name);
