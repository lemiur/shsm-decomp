/* check_docs: the documentation describes the tools and files that exist (a port of python tools/check_docs.py).
 *
 * Five checks, each adding to one CdReport: verbs (the "Project adapter" table of docs/tools.md against each build/tools
 * executable's --help),
 * paths (repository paths written in the docs exist), pins (the Toolchain list of docs/toolchain.md against doctor's pin table),
 * exceptions (ledger exceptions-check folded in) and text (hygiene of the tracked text files). The pieces the tests
 * replace are parameters: the help runner, the pin table, the exceptions runner and the file list. */
#pragma once
#include "avp/avp.h"
#include "doctor.h"

/* Where the documents the verbs and pins checks read live: the verb table (docs/tools.md) and the Toolchain list
 * (docs/toolchain.md), both moved out of AGENTS.md. The DOL hash stays in AGENTS.md ("Byte gate"). */
#define CD_VERBS_DOC "docs/tools.md"
#define CD_PINS_DOC "docs/toolchain.md"
#define CD_GUIDE_DOC "AGENTS.md"

/* ---- report (check_docs.Report) -------------------------------------------------------------------------------- */
typedef struct {
    const char *check;   /* upper case: "VERBS", "PATHS", ... */
    const char *file;
    i64 line;
    const char *message;
    bool note;
} CdItem;
typedef AVP_VEC(CdItem) CdItems;
typedef struct {
    AvpArena *a;
    CdItems items;
} CdReport;

void cd_report_init(CdReport *rep, AvpArena *a);
void cd_report_add(CdReport *rep, const char *check, const char *file, i64 line, bool note, const char *fmt, ...);
/* Report.text(item): "file:line: CHECK [note: ]message". */
char *cd_item_text(AvpArena *a, const CdItem *it);

extern const char *const CD_CHECKS[]; /* verbs, paths, pins, exceptions, text; NULL-terminated */

/* ---- shared readers -------------------------------------------------------------------------------------------- */
/* read_lines(path): the lines of a text file (utf-8, errors="replace", universal newlines), terminators removed.
 * Raises the OSError as Python would. */
AvpStrVec cd_read_lines(AvpArena *a, const char *path);
/* doc_files(root): AGENTS.md, tools/rules/README.md, tools/c/README.md (those that are files), then
 * docs/*.md sorted. Root-relative, forward slashes. */
AvpStrVec cd_doc_files(AvpArena *a, const char *root);
AvpStrVec cd_split_cells(AvpArena *a, AvpStr line);
AvpStrVec cd_code_spans(AvpArena *a, AvpStr text);

/* ---- verbs ----------------------------------------------------------------------------------------------------- */
typedef struct {
    i64 lineno;
    AvpStr verb, cell;
} CdRow;
typedef AVP_VEC(CdRow) CdRows;
CdRows cd_adapter_rows(AvpArena *a, const char *root);

/* What a command of the table runs. Each segment of a code span (split at &&, ; and |) is one command at most. */
typedef enum {
    CD_INV_TOOL,   /* build/tools/<name>.exe: a project tool, its --help checked */
    CD_INV_BIN,    /* tools/bin/<tool>[.exe]: a pinned binary (ninja, objdiff-cli, dtk), its --help checked */
    CD_INV_OTHER,  /* tools/build_c.cmd, ctest, cmake, git, cp, rm: accepted, not checked */
    CD_INV_PYTHON, /* python tools/x.py, python -m tools.x, python configure.py, tools/x.py: a finding */
} CdInvKind;
typedef struct {
    CdInvKind kind;
    AvpStr program; /* build/tools/match.exe, tools/bin/ninja (no .exe), git, python tools/x.py */
    AvpStr args;    /* the text after the program */
} CdInvocation;
typedef AVP_VEC(CdInvocation) CdInvocations;
CdInvocations cd_parse_invocations(AvpArena *a, AvpStr span);
AvpStrVec cd_command_tokens(AvpArena *a, AvpStr text);
bool cd_is_flag(AvpStr tok);
bool cd_is_word(AvpStr tok);

/* The project tool `name` ("ledger.exe"): exe_dir/name when it is a file (exe_dir NULL: the folder of this
 * executable, so an isolated build such as build/tools-B checks its own tools), else root/build/tools/name, else
 * NULL. */
const char *cd_tool_exe(AvpArena *a, const char *exe_dir, const char *root, const char *name);
#define CD_BUILD_HINT "build the tools with tools/build_c.cmd"

/* A help runner: the help text of argv run in root, or NULL when it cannot be read (run_help). Called from worker
 * threads: it must be thread safe and return memory that outlives the call (heap or a long-lived arena). */
typedef char *(*CdHelpRunner)(void *ctx, const char *const *argv, size_t argc, const char *root);
typedef struct {
    CdHelpRunner run;
    void *ctx;
    const char *exe_dir; /* where the build/tools/<name>.exe commands are looked up first (cd_tool_exe) */
    int jobs;            /* help commands run at once (at least 1) */
} CdHelpEnv;
/* The real runner: the command with stdin from NUL, 120 s, stdout + stderr as text (utf-8, errors="replace"). */
char *cd_run_help(void *ctx, const char *const *argv, size_t argc, const char *root);
void cd_check_verbs(const char *root, CdReport *rep, const CdHelpEnv *env);

/* ---- paths ----------------------------------------------------------------------------------------------------- */
void cd_check_paths(const char *root, CdReport *rep);
/* clean_path / is_placeholder / path_tokens, exposed for the tests. NULL: not a repository path. */
const char *cd_clean_path(AvpArena *a, AvpStr token);
bool cd_is_placeholder(AvpStr seg);
AvpStrVec cd_path_tokens(AvpArena *a, AvpStr span);

/* ---- pins ------------------------------------------------------------------------------------------------------ */
typedef struct {
    const DoctorPin *pins;
    size_t npins;
    const char *const *compilers_required; /* NULL-terminated, may be NULL */
    const char *const *compilers_optional;
} CdPins;
CdPins cd_doctor_pins(void); /* the C doctor's table (doctor.PINS, COMPILERS_REQUIRED, COMPILERS_OPTIONAL) */
typedef struct {
    i64 lineno;
    AvpStr text;
} CdEntry;
typedef AVP_VEC(CdEntry) CdEntries;
CdEntries cd_toolchain_entries(AvpArena *a, const char *root);
void cd_check_pins(const char *root, CdReport *rep, const CdPins *pins);

/* ---- exceptions ------------------------------------------------------------------------------------------------ */
/* An exceptions runner: runs argv in root; *rc and *out (stdout + stderr) on success; false with *err (the OSError
 * text) when it could not run. */
typedef bool (*CdExcRunner)(void *ctx, const char *const *argv, size_t argc, const char *root, i64 *rc, AvpStr *out,
                            const char **err);
bool cd_run_exceptions(void *ctx, const char *const *argv, size_t argc, const char *root, i64 *rc, AvpStr *out,
                       const char **err);
/* check_exceptions: runs `ledger.exe exceptions-check` (cd_tool_exe with exe_dir) and folds its lines in; returns
 * the exit code (-1: could not run, or no ledger.exe). runner NULL: cd_run_exceptions. */
i64 cd_check_exceptions(const char *root, CdReport *rep, const char *exe_dir, CdExcRunner runner, void *ctx);

/* ---- text ------------------------------------------------------------------------------------------------------ */
typedef struct {
    bool insert_final_newline, trim_final_newlines;
} CdFinalNewline;
CdFinalNewline cd_vscode_rules(const char *root);
void cd_check_text_file(CdReport *rep, const char *rel, AvpStr data, CdFinalNewline rules);
/* files NULL: git ls-files -z in root (a failure raises SHSM_EXC_USAGE "git ls-files failed: ..."). */
void cd_check_text(const char *root, CdReport *rep, const AvpStrVec *files);
/* tab_indented_line: the line of the first INDENT token whose text holds a tab, or 0 (Python's None) when there is
 * none or the tokenizer stops with an error first (cd_pytok.c). */
i64 cd_tab_indented_line(AvpStr data);
/* bytes.decode("cp932") succeeds (Python's codec, not the Windows code page's exact table). */
bool cd_cp932_valid(AvpStr data);

/* ---- front end ------------------------------------------------------------------------------------------------- */
int check_docs_main(int argc, char **argv);
