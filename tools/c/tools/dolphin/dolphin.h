/* tools/dolphin.py in C: observe the running game in Dolphin through its GDB remote stub.
 *
 * The library part (static library tool_dolphin) holds every operation of the Python's OPS table, so the command
 * line (main.c, dolphin_cli.c) and the MCP server (tools/c/tools/mcp_server, LIBS tool_dolphin) share them.
 *
 *     DolphinEnv *env = dolphin_env_new(root);              // root NULL: the checkout around the cwd
 *     env->target = &my_target;                             // optional: an injected ShsmGdbTarget (tests)
 *     AvpJson *args = avp_json_obj(a);                      // the op's parameters by their Python names
 *     avp_json_set(a, args, AVP_S("address"), avp_json_strz(a, "lbl_80002008"));
 *     DolphinResult *r = dolphin_call(env, "mem", args);    // raises SHSM_EXC_USAGE / SHSM_EXC_GDB* like Python
 *     ... r->lines (text), r->data (the JSON document), r->code (exit code 0/1) ...
 *     dolphin_env_close(env);                               // closes the broker connection
 *
 * dolphin_call fills absent parameters with the Python defaults. "confirm" (write, undo) is not in the OPS params
 * (the Python's run() adds it for the command line): pass {"confirm": true} in args to perform the write; without it
 * write/undo raise the Python's "refused: ... add --confirm" usage error. An MCP server decides confirmation itself
 * (the op table's `destructive` flag) and passes confirm accordingly. --json is the caller's choice of rendering:
 * print r->data with shsm_json_opts() (json.dumps(indent=1)) or r->lines capped by dolphin_result_text.
 *
 * Errors are avp_throw codes (SHSM_EXC_USAGE, SHSM_EXC_GDB, _TIMEOUT, _DISCONNECTED; all exit 2 through
 * shsm_cli_main); catch them with AVP_TRY. Allocation is from env->a (the thread's long-lived arena). */
#pragma once
#include "avp/json.h"
#include "avp/str.h"
#include "avp/vec.h"
#include "shsm_gdb.h"
#include "shsm_repo.h"
#include "shsm_symbols.h"

#define DOLPHIN_PROG "dolphin.exe"

/* ---- Result ---- */
typedef struct {
    AvpStrVec lines;
    AvpJson *data; /* an object (Python's `data or {}`) */
    int code;      /* SHSM_EXIT_OK / SHSM_EXIT_FINDING */
} DolphinResult;

/* Result.text(limit): the lines capped with cli.cap_lines (limit 0: all), joined with "\n". */
AvpStr dolphin_result_text(AvpArena *a, const DolphinResult *r, i64 limit);

/* ---- Env ---- */
typedef struct DolphinEnv {
    AvpArena *a;
    const ShsmRepo *repo;
    const char *dir, *session_path, *journal_path, *settings_path, *user_dir;
    ShsmGdbTarget *target; /* injected target (tests); NULL: the session broker */
    /* private */
    ShsmGdbTarget own;
    ShsmBrokerClient *bc;
    struct DolphinSyms *syms;
    struct DolphinClasses *classes;
    int dol_state; /* 0 not loaded, 1 loaded, -1 missing */
    AvpBytes dol;
    int ix_state;  /* 0 not opened, 1 open, -1 unavailable (Python's ix None) */
    struct ShsmAsmIndex *ix;
} DolphinEnv;

DolphinEnv *dolphin_env_new(const char *root);
void dolphin_env_close(DolphinEnv *env);
AvpJson *dolphin_session(DolphinEnv *env);      /* session.json, or NULL */
AvpJson *dolphin_session_live(DolphinEnv *env); /* the session when its Dolphin is the recorded process, else NULL */
ShsmGdbTarget *dolphin_target(DolphinEnv *env); /* the injected target or the broker (raises without a session) */

/* ---- the op table (OPS, in Python order) ---- */
typedef struct {
    const char *name, *type, *description; /* type: "number" "string" "boolean" "integer" "array" */
    bool required;
} DolphinParam;
typedef struct {
    const char *name, *doc;
    bool destructive;
    const DolphinParam *params; /* NULL-name terminated */
} DolphinOp;
extern const DolphinOp dolphin_ops[];
extern const size_t dolphin_nops;
const DolphinOp *dolphin_op(const char *name);

/* Runs the op `name` with JSON arguments (an object; NULL: none). Unknown op: SHSM_EXC_USAGE. */
DolphinResult *dolphin_call(DolphinEnv *env, const char *name, const AvpJson *args);

/* ---- pieces exposed for the tests and the CLI ---- */
typedef struct {
    const char *name, *type;
} DolphinSigArg;
typedef struct {
    DolphinSigArg *v;
    size_t n;
    bool none; /* None (default_signature of a name that does not demangle) */
} DolphinSig;
DolphinSig dolphin_default_signature(AvpArena *a, const char *name); /* name NULL: None */
DolphinSig dolphin_parse_signature(AvpArena *a, AvpStr text);        /* raises SHSM_EXC_USAGE */
void dolphin_guard_user_dir(DolphinEnv *env, const char *user);       /* raises SHSM_EXC_USAGE */
/* (address, symbol or NULL); kind NULL, "function" or "object" */
u64 dolphin_resolve(DolphinEnv *env, AvpStr text, const char *kind, const ShsmSymbol **sym);
AvpJson *dolphin_journal_entries(DolphinEnv *env); /* array of the write entries, each with "undone" */
void dolphin_write_json_atomic(const char *path, const AvpJson *data);
AvpJson *dolphin_read_json(AvpArena *a, const char *path); /* NULL on OSError / ValueError */

/* proc_info: false when the process is not running; exe and the creation FILETIME (as Python's int) */
bool dolphin_proc_info(AvpArena *a, i64 pid, const char **exe, i64 *created);
bool dolphin_terminate(i64 pid, double wait);
/* spawn_detached: a process that outlives this one (DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP, out of the job
 * when allowed); stdout and stderr appended to log_path (NULL: nul). Returns the pid (raises SHSM_EXC_OS). */
i64 dolphin_spawn_detached(const char *const *argv, size_t argc, const char *log_path, const char *cwd, void **handle);

/* internal _broker subcommand */
int dolphin_run_broker(const char *root, int gdb_port, double wait);
/* the command line (dolphin_cli.c) */
int dolphin_main(int argc, char **argv);
