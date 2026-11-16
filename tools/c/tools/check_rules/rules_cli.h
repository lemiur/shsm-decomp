/* rules_cli: check_rules.py's command line below argparse (L4625-4834): the file selection (collect_files,
 * units_to_files, changed_files), the text report (summarize and main's text block) and the --json document.
 * main.c holds the parser and the order of main(). rules_check.md "Command line" documents it. */
#pragma once
#include "rules_check.h"

/* collect_files(root, paths): sources and headers of each directory (walked in sorted order), each plain file as
 * given, as root-relative "/" paths. false with *err = UsageError's text for a path that does not exist, or
 * *err = NULL when os.path.relpath would raise ValueError (another drive). */
bool rl_collect_files(AvpArena *a, const char *root, const AvpStrVec *paths, AvpStrVec *out, const char **err);
/* rl_asm_unit: the unit (`main/x`, `x` or `x.s`) is an assembly source unit, root/src/x.s; rl_run's --units leaves it
 * out (the checker reads no `.s` file). */
bool rl_asm_unit(AvpArena *a, const char *root, AvpStr name);
/* units_to_files(root, names, sources): the src/ keys of source_paths (load_sources order) of each unit name. */
bool rl_units_to_files(AvpArena *a, const AvpStrVec *names, const AvpStrVec *source_paths, AvpStrVec *out,
                       const char **err);
/* changed_files(root, rev): `git diff --name-only --diff-filter=ACMR rev --` then the untracked files, filtered.
 * Runs git in root through shsm_git with -z (D-check_rules-12: a path with a space stays whole). */
bool rl_changed_files(AvpArena *a, const char *root, AvpStr rev, AvpStrVec *out, const char **err);

/* The findings main() shows: those of the --tier set (tiers.p NULL: all), sorted by (path, line, rule), stable. */
typedef RL_VEC(const RlFinding *) RlFindingPtrs;
RlFindingPtrs rl_shown_findings(RlChecker *ck, AvpStr tiers);
/* summarize(ck, out): the summary lines appended to *out. */
void rl_summarize(RlChecker *ck, AvpStrVec *out, bool all_functions);
/* main()'s text report: every line, joined with "\n", plus print()'s final newline. */
AvpStr rl_report_text(RlChecker *ck, AvpStr tiers, bool no_summary, bool all_functions);
/* main()'s --json document: json.dumps(doc, indent=2) + "\n". */
void rl_report_json(RlChecker *ck, AvpBuf *b, AvpStr root, AvpStr tiers);

/* main() below argparse: one run of the checker. The tools that run the checker in-process (match --rules,
 * pre_commit, ledger, status, gate) fill the options the Python's command line would carry; NULL or empty fields are
 * the Python's defaults (no paths: src/). root is required. */
typedef struct {
    const char *root;
    AvpStrVec paths;       /* positional paths */
    AvpStrVec units;       /* --units */
    AvpStr changed;        /* --changed REV ({0}: none) */
    const char *configure; /* --configure (NULL: <root>/configure.py) */
    const char *allow;     /* --allow (NULL: <root>/docs/exceptions.md) */
    const char *classes;   /* --classes (NULL: <root>/build/R5WEA4/classes.json) */
    bool json, no_summary, functions;
    AvpStr tiers; /* --tier ({0}: all) */
} RlRunOpts;
/* The exit status the Python's main() gives (0, 1, 2); *out gets what it prints on stdout and *err what it prints on
 * stderr (a usage line, or a traceback's last line), each appended. */
int rl_run(AvpArena *a, const RlRunOpts *o, AvpBuf *out, AvpBuf *err);

/* ledger.run_checker(root), in-process: `check_rules --json --root root` (the command ledger.py, status.py, pick.py
 * and the gate run) read as ledger.py reads it. Returns the document; NULL with *why the LedgerError text when the
 * run exits with neither 0 nor 1 ("rule checker failed (exit N): <stderr stripped, first 300 characters>") or prints
 * no JSON ("rule checker printed no JSON: <json error>"). *rc gets the exit status when rc is not NULL. */
AvpJson *rl_checker_json(AvpArena *a, const char *root, int *rc, const char **why);
/* The same reading of a finished run's exit status, stdout and stderr (for callers that substitute the run). */
AvpJson *rl_checker_json_of(AvpArena *a, int rc, AvpStr out, AvpStr err, const char **why);
