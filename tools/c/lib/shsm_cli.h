/* CLI conventions shared by the tools (tools/common/cli.py): exit codes, the Python exceptions the shared modules
 * raise (as avp_throw codes), the main() wrapper, capped output with the same footers, --json, subprocess runs.
 *
 * Exceptions. The shared modules raise what tools/common raised, through avp_throw (base.h) with one of the
 * SHSM_EXC_* codes and the exception's args[0] as the message. A caller that caught an exception in Python wraps the
 * call in AVP_TRY and tests the class with shsm_exc_is_*; str(e) is shsm_exc_str. Uncaught, shsm_cli_main turns it
 * into the one-line error and exit code 2 (1 for SHSM_EXC_FINDING), as cli.main did. */
#pragma once
#include "avp/args.h"
#include "avp/json.h"
#include "avp/str.h"

enum { SHSM_EXIT_OK = 0, SHSM_EXIT_FINDING = 1, SHSM_EXIT_ERROR = 2 };
#define SHSM_DEFAULT_LIMIT 40 /* cli.DEFAULT_LIMIT */

/* ---- exceptions ---------------------------------------------------------------------------------------------- */
enum {
    SHSM_EXC_USAGE = 64,      /* cli.UsageError */
    SHSM_EXC_FINDING,         /* cli.Finding */
    SHSM_EXC_KEY,             /* KeyError (msg is args[0]; str(e) is its repr) */
    SHSM_EXC_UNIT_NOT_BUILT,  /* units.UnitNotBuilt, a KeyError whose str(e) is the message */
    SHSM_EXC_VALUE,           /* ValueError (also UnicodeDecodeError and mangle's int() failures) */
    SHSM_EXC_TYPE,            /* TypeError */
    SHSM_EXC_OS,              /* OSError (not FileNotFoundError) */
    SHSM_EXC_FILE_NOT_FOUND,  /* FileNotFoundError (an OSError) */
    SHSM_EXC_RECURSION        /* RecursionError */
};
/* raise <class>(message) */
AVP_NORETURN void shsm_raise(int exc, AVP_PRINTF_FMT const char *fmt, ...);
/* Re-raises the calling thread's last fs.h failure (avp_io_err) as Python did: FileNotFoundError / OSError /
 * ValueError (decode errors, relpath), with str(e) as the message. */
AVP_NORETURN void shsm_raise_io(void);
int shsm_io_exc_code(void); /* the SHSM_EXC_* class shsm_raise_io would raise */
/* type(e).__name__ of an exception code (core AVP_ERR_* codes too). */
const char *shsm_exc_class(int exc);
bool shsm_exc_is_key(int exc);   /* except KeyError (KEY, UNIT_NOT_BUILT) */
bool shsm_exc_is_os(int exc);    /* except OSError (OS, FILE_NOT_FOUND, core AVP_ERR_IO / AVP_ERR_PROC) */
bool shsm_exc_is_value(int exc); /* except ValueError (VALUE, core AVP_ERR_FORMAT) */
/* str(e): the repr of the message for a KeyError, else the message. */
AvpStr shsm_exc_str(AvpArena *a, int exc, const char *msg);
/* The text cli.main prints after "<prog>: error: ": the first line of args[0] for a KeyError, else of str(e),
 * else the class name. */
AvpStr shsm_exc_main_text(AvpArena *a, int exc, const char *msg);

/* ---- argument parser --------------------------------------------------------------------------------------------
 * cli.parser(description): argparse with --root DIR, --json, --all, --limit N (cli.limit_type: 0, 'all', 'none',
 * 'unlimited', 'inf' mean no cap, i.e. 0) and --debug, with cli.py's help texts. prog: Python's prog ("match.py"). */
AvpArgParser *shsm_cli_parser(const char *prog, const char *description, bool root, bool json_switch, bool limit);
/* cli.limit_type on one argument: true with *n, or false with *msg (the ArgumentTypeError text). */
bool shsm_limit_type(AvpArena *a, AvpStr text, i64 *n, AvpStr *msg);

/* ---- main ------------------------------------------------------------------------------------------------------
 * cli.main(func, ap, argv): parses argv (argparse exits 2 on errors, 0 on --help), calls fn and returns its exit
 * code. An exception from fn closes every ShsmOutput still open (as their `with` blocks did, so --json still prints
 * its document), then prints "<prog>: <msg>" and gives 1 for SHSM_EXC_FINDING, else "<prog>: error: <text>" and 2.
 * --debug prints no traceback (there is none). */
typedef int (*ShsmMainFn)(void *ctx, AvpArgs *args);
int shsm_cli_main(AvpArgParser *ap, int argc, char **argv, ShsmMainFn fn, void *ctx);

/* ---- capped output ---------------------------------------------------------------------------------------------- */
/* cli.cap_lines: lines cut to limit, plus "... N more lines, <hint>" when cut. limit 0 or show_all: all (a copy). */
AvpStrVec shsm_cap_lines(AvpArena *a, const AvpStr *lines, size_t n, i64 limit, bool show_all, const char *hint);

/* cli.Output: capped text lines on stdout plus a --json collector. Text is suppressed under --json. Outputs are
 * allocated from the thread's long-lived arena (they outlive a throw out of the function that opened them) and are
 * used from the main thread only. */
typedef struct ShsmOutput {
    bool as_json, show_all;
    i64 limit;            /* 0: no cap (Python's None) */
    i64 count, dropped;
    AvpJson *payload;     /* the document close() prints under --json (an object) */
    bool closed;
    struct ShsmOutput *prev_open;
} ShsmOutput;
/* Output(args, limit=..., show_all=..., as_json=...): limit < 0 means "from args" (args.limit, else 40), else the
 * given value (0 = no cap); show_all/as_json < 0 mean from args.all / args.json, else false/true. args may be
 * NULL. */
ShsmOutput *shsm_output_open(const AvpArgs *args, i64 limit, int show_all, int as_json);
void shsm_output_line(ShsmOutput *o, AvpStr text); /* one print (split at "\n"; dropped and counted past the cap) */
void shsm_output_linez(ShsmOutput *o, const char *text);
void shsm_output_lines(ShsmOutput *o, const AvpStr *lines, size_t n);
void shsm_output_data(ShsmOutput *o, const char *key, AvpJson *val); /* payload[key] = val */
/* Prints json.dumps(payload, indent=1) under --json, else "... N more lines, use --all" when lines were dropped.
 * Idempotent. */
void shsm_output_close(ShsmOutput *o);
/* The --json document's dump options: json.dumps(x, indent=1) (ensure_ascii, separators "," and ": "). */
AvpJsonDumpOpts shsm_json_opts(void);

/* ---- subprocesses (cli.run) ------------------------------------------------------------------------------------- */
typedef struct {
    i64 returncode;   /* -1 on a timeout (Python's None) */
    bool timed_out;
    AvpStr out, err;  /* text, utf-8 with errors="replace", newlines translated */
    double seconds;
} ShsmRun;
/* cli.run(argv, cwd=cwd, timeout=timeout): no shell, captured text output; timeout <= 0: none. A program that
 * cannot be found raises SHSM_EXC_USAGE "cannot run <argv0>: [WinError 2] ..."; another start failure raises
 * SHSM_EXC_OS with the Windows message. Never raises for a non-zero exit. */
ShsmRun shsm_run(AvpArena *a, const char *const *argv, size_t argc, const char *cwd, double timeout);
static inline bool shsm_run_ok(const ShsmRun *r) { return !r->timed_out && r->returncode == 0; }
AvpStr shsm_run_output(AvpArena *a, const ShsmRun *r); /* stdout + stderr */
/* RunResult.check(): raises SHSM_EXC_USAGE "<exe base name> failed (exit N|timed out): <first output line>". */
void shsm_run_check(AvpArena *a, const ShsmRun *r, const char *argv0);

/* cli.default_jobs(): os.cpu_count(), at least 1. */
int shsm_default_jobs(void);
