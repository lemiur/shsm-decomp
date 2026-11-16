/* unit_cmd: the subcommands of tools/unit.py (new, check, park, flip) callable in-process. See API.md. */
#pragma once
#include "unit_stub.h"

typedef struct {
    const char *path;
    const char *text;   /* --text START-END, NULL for a data-only unit */
    const char *lib;    /* --lib, NULL: inferred */
    const char *data;   /* "auto" or "none" */
    bool confirmed, allow_no_asm, dry_run, regen;
    const char *name_evidence; /* --name-evidence: replaces the "unit name unconfirmed" comment; NULL: none */
    AvpStrVec include_data, exclude_data; /* command-line order, repeats kept */
} UnitNewArgs;

typedef struct {
    const char *path;
    bool dry_run, ignore_untracked;
    const char *gate_cmd, *rules_cmd; /* NULL: not given */
} UnitFlipArgs;

/* Each prints through unit_out and returns the exit code; a ToolError raises UNIT_TOOL_ERROR. */
int unit_cmd_new(UnitProject *p, const UnitNewArgs *args);
/* resplit: re-split an existing NonMatching unit to a new --text range and data set (args as for new; confirmed,
 * name_evidence and lib are unused). Rewrites its splits.txt entry in place; refuses a Matching unit. */
int unit_cmd_resplit(UnitProject *p, const UnitNewArgs *args);
int unit_cmd_check(UnitProject *p, const char *path);
int unit_cmd_park(UnitProject *p, const char *path);
int unit_cmd_flip(UnitProject *p, const UnitFlipArgs *args);

/* regen(root): runs tools/bin/ninja.exe build.ninja in the checkout, returns its exit code (2 when it is missing). */
int unit_regen(AvpArena *a, const char *root);
/* remove_stale_listings(root): deletes the auto_*.s listings the split record no longer names (removed may be NULL). */
void unit_remove_stale_listings(AvpArena *a, const char *root, AvpStrVec *removed);
/* gate_pending_ok(rc, text, path): gate.exe's GATE OK-PENDING line for this flip, or NULL. */
const char *unit_gate_pending_ok(AvpArena *a, int rc, AvpStr text, const char *path);
/* The C tools flip runs, beside unit.exe: unit_tool_exe(a, "gate.exe" | "check_rules.exe"). The unit tests replace
 * the gate with a stub through the environment variable UNIT_GATE_ENV (the program's full path). */
#define UNIT_GATE_ENV "SHSM_UNIT_GATE_EXE"
const char *unit_tool_exe(AvpArena *a, const char *name);

/* The tool's main (argv as AVP_MAIN gives it). */
int unit_main(int argc, char **argv);
