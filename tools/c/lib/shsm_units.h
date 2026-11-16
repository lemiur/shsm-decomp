/* Translation units (tools/common/units.py): objdiff.json + configure.py statuses + build.ninja compile commands,
 * the compile argv, compiling into a private directory. */
#pragma once
#include "avp/json.h"
#include "avp/str.h"
#include "shsm_cli.h"
#include "shsm_repo.h"
#include <string.h>

/* One translation unit (units.Unit). Strings NULL where Python had None. */
typedef struct {
    const char *name;        /* "main/game/PadRumble" */
    const char *path;        /* "game/PadRumble" */
    const char *source;      /* "src/game/PadRumble.cpp" or NULL */
    const char *status;      /* Matching / NonMatching / Equivalent / MatchingFor / Asm */
    const char *library;     /* build.ninja library name or NULL */
    bool linked, complete, auto_generated;
    AvpStrVec progress_categories;
    const char *target_obj, *base_obj; /* absolute (normpath) or NULL */
    const char *mw_version;  /* "Wii/1.0" or NULL */
    bool has_cflags;         /* cflags is None when false */
    AvpStrVec cflags;        /* compiler arguments as build.ninja passes them (shlex-split) */
    const char *rule;        /* "mwcc_sjis" ... or NULL */
    AvpJson *objdiff;        /* the raw objdiff.json entry, NULL for {} (configure.py-only units) */
} ShsmUnit;

static inline bool shsm_unit_has_source(const ShsmUnit *u) { return u->source != NULL; }
/* An assembly source unit (`src/x.s`, user's ruling 2026-11-14): assembled by binutils as, never compiled by MWCC, so
 * the compile-and-compare tools (match, wall, eqcheck, levers, neutral) leave it out of their batches. */
static inline bool shsm_unit_is_asm(const ShsmUnit *u) {
    size_t n = u->source ? strlen(u->source) : 0;
    return n >= 2 && u->source[n - 2] == '.' && (u->source[n - 1] == 's' || u->source[n - 1] == 'S');
}

typedef struct {
    ShsmUnit **v;            /* objdiff.json order, then configure.py-only units in configure.py order */
    size_t n, cap;
    void *index;             /* AvpMap: name -> index (internal) */
} ShsmUnits;

/* ---- parsers (exposed for tests) ---- */
typedef struct {
    const char *path, *status, *source; /* unit path, status (MatchingFor(...) -> MatchingFor), source as written */
} ShsmConfStatus;
/* parse_configure_status(text): one entry per Object(...) in text order (a repeated path: the last status wins in
 * Python's dict, at the first position; this list keeps that dict order). */
size_t shsm_parse_configure_status(AvpArena *a, AvpStr text, ShsmConfStatus **out);

typedef struct {
    const char *path;            /* unit path */
    const char *source, *library, *object, *rule, *mw_version; /* mw_version NULL when absent */
    bool linked, has_cflags;
    AvpStrVec cflags;
} ShsmNinjaCompile;
/* join_ninja_lines(text): `$`-newline continuations joined (re.sub(r" ?\$\r?\n\s*", " ", text)). */
AvpStr shsm_join_ninja_lines(AvpArena *a, AvpStr text);
/* parse_ninja_compiles(text): every mwcc* build edge after a "# <source>: <lib> (linked True|False)" comment, in dict
 * order (a repeated unit path keeps its first place, last value). A bad cflags quoting raises SHSM_EXC_VALUE. */
size_t shsm_parse_ninja_compiles(AvpArena *a, AvpStr text, ShsmNinjaCompile **out);

/* ---- the unit table ---- */
/* load_units(repo): every unit (parsed once per process and cached until objdiff.json, configure.py or build.ninja
 * change size or mtime). Raises what reading or parsing them raised in Python. */
const ShsmUnits *shsm_load_units(const ShsmRepo *r);
/* units.get(unit_name(unit)) without the errors of get_unit: NULL when unknown. */
const ShsmUnit *shsm_units_lookup(const ShsmUnits *us, const char *unit);
/* get_unit(unit): raises SHSM_EXC_KEY "unknown unit <unit> (not in objdiff.json, configure.py or splits.txt)", or
 * SHSM_EXC_UNIT_NOT_BUILT when splits.txt or configure.py names the unit but build.ninja does not. */
const ShsmUnit *shsm_get_unit(const ShsmRepo *r, const char *unit);

#define SHSM_REGEN_COMMAND "tools/bin/ninja.exe build.ninja"

/* ---- compiling ---- */
/* object_for(source, out_dir): <abspath(out_dir)>/<source stem>.o; depfile_for: the .d beside it. */
const char *shsm_object_for(AvpArena *a, const char *source, const char *out_dir);
const char *shsm_depfile_for(AvpArena *a, const char *source, const char *out_dir);

typedef struct {
    const char *const *extra_flags; size_t nextra; /* appended after the unit's cflags */
    const char *const *drop_flags; size_t ndrop;   /* exact words removed from the unit's cflags */
    const char *mw_version;                        /* override, NULL: the unit's */
    int sjis;                                      /* -1: as the unit's rule says (and the wrapper exists); 0/1 */
    bool depfile;                                  /* -MMD and the output directory after -o */
} ShsmCompileOpts;
/* compile_argv(unit, source, out, ...): the exact argv build.ninja would run (cwd = the repository root). Raises
 * SHSM_EXC_VALUE when the unit has no cflags ("unit X has no compile command in build.ninja (asm unit, or regenerate
 * it with `...`)") or, with depfile, when out is a file not named <source stem>.o. opts may be NULL. */
AvpStrVec shsm_compile_argv(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u, const char *source, const char *out,
                            const ShsmCompileOpts *opts);
/* compile_source(unit, source, out, ...): makes the output directory, runs compile_argv with cwd = root. */
ShsmRun shsm_compile_source(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u, const char *source, const char *out,
                            const ShsmCompileOpts *opts, double timeout);
