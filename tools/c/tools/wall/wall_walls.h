/* wall_walls: the parts of tools/wall.py other tools read (the briefing tool, the lever search).
 *
 *   build/walls.json, the scan's record (wall.py walls_path, load_walls, WALLS_VERSION), with the staleness test
 *   of tools/match.py inputs_staleness (a private port: the C match port has no inputs_staleness);
 *   the parked-function notes in a unit's source comments (wall.py find_notes, note_classes, NOTE, CLASS_WORDS,
 *   _base_name).
 *
 * Python exceptions are raised as shsm_raise does (SHSM_EXC_* codes, shsm_cli.h). */
#pragma once
#include "avp/avp.h"
#include "shsm_repo.h"

#define WALLS_VERSION 2 /* wall.py WALLS_VERSION: the "version" member of build/walls.json */

/* walls_path(repo): <root>\build\walls.json. */
const char *wall_walls_path(AvpArena *a, const ShsmRepo *r);

/* match.inputs_staleness(repo, unit, inp): why a match.state_inputs() record (a JSON object, or NULL / null for
 * None) no longer describes `unit`; "" when it still does. Raises what shsm_get_unit raises for an unknown unit
 * (SHSM_EXC_KEY / SHSM_EXC_UNIT_NOT_BUILT: the KeyError load_walls catches). */
AvpStr wall_inputs_staleness(AvpArena *a, const ShsmRepo *r, const char *unit, const AvpJson *inp);

/* load_walls(repo, include_stale): build/walls.json with only its current units: the document as written
 * ({version, seconds, built, units [names], inputs {unit: state_inputs}, counts, proved, prove_all, errors,
 * call_summary, functions [records with "unit"]}) plus "stale" {unit: reason}. A unit is current when its source,
 * every file of its dependency file, its flags, its retail listing and symbols.txt still have the digests the scan
 * recorded (wall_inputs_staleness; an unknown unit is stale with "unit no longer exists"); a --built scan records
 * no inputs, so all its units are stale. Without include_stale "units" and "functions" keep only the current units'
 * entries (members replaced in place; "stale", then "units", then "functions" appended when absent, as the Python's
 * dict assignments do). NULL when the file is absent, unreadable, not UTF-8, not JSON or not an object. */
AvpJson *wall_load_walls(AvpArena *a, const ShsmRepo *r, bool include_stale);

/* ---- notes ----------------------------------------------------------------------------------------------------- */

/* One parked-function note of a source (find_notes' tuple). */
typedef struct {
    i64 line;      /* 1-based line of the note's keyword */
    bool has_addr; /* false: None (a file-header note, or no function region holds it) */
    u64 addr;      /* the address comment of the function whose header or body holds the note */
    AvpStr text;   /* the note line and up to five continuation comment lines, comment markers stripped */
} WallNote;

/* find_notes(path): the notes of a source file in line order (a comment line holding NonMatching:, MATCH-PENDING,
 * Not exact, Not yet matched, Parked:, does not match, not matching or wall:, case-insensitive, and not
 * ORIGINAL-ASM / RULE-EXCEPTION). The file is read as UTF-8 with errors="replace"; a read failure raises the
 * OSError open() raised (shsm_raise_io). Returns the count; *out is allocated from a. */
size_t wall_find_notes(AvpArena *a, const char *path, WallNote **out);

/* note_classes(text): the wall classes a note's words name, in CLASS_WORDS order ("referent", "call set",
 * "control flow", "other", "register/schedule"). The strings are static. */
size_t wall_note_classes(AvpStr text, const char *out[5]);

/* _RULE_NOTE.search(text): the note speaks of a rule (MATCH-PENDING or an R<n> rule number). */
bool wall_is_rule_note(AvpStr text);

/* NOTE.search(text): the byte offset of the first note keyword, or -1. */
i64 wall_note_keyword(AvpStr text);

/* _base_name(name): the source name of a symbol ('fn_801A73A8' for 'fn_801A73A8__12PlaneTriggerF...', a name
 * without "__" after its first character as is). A view into name. */
AvpStr wall_base_name(AvpStr name);
