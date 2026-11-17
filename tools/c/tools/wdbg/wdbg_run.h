/* One compile under the debugger: the part of wdbg.exe's single-run path that other tools reuse (mwtrace runs its
 * generated specs through it). The compiler process is created suspended inside a job object with
 * JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE, so it never outlives the tool that runs it: the job handle is closed when the
 * run ends, and Windows closes it when the tool dies. */
#pragma once
#include "wdbg_debug.h"

typedef struct {
    const char *exe;          /* tools/compilers/<build>/mwcceppc.exe (forward slashes, as wdbg.exe builds it) */
    const char *build;        /* "Wii/1.0a": the spec's build line and addr() use it */
    const char *const *argv;  /* the whole command line, argv[0] the compiler */
    size_t argc;
    const char *cwd;          /* the compiler's working directory */
    const char *outdir;       /* exists; log.txt and the spec's snapshots go here */
    const char *spec_path;    /* named in error messages */
    AvpStr spec_text;         /* the spec (SPEC.md) */
    const char *output_path;  /* NULL: the compiler writes to this process's console; else its stdout and stderr go
                               * to this file (created anew) */
} WdbgRun;

/* Writes <outdir>/log.txt (its first line "compiler: <build> (tools/compilers/<build>/mwcceppc.exe)"), parses and
 * loads the spec, runs the compiler to its exit. false with *cls ("SyntaxError", "AssertionError", ...) and *msg set
 * when the run could not start: the message of a spec load error already carries its class ("BuildError: ..."),
 * then *cls is NULL. *exit_code is the compiler's exit code (-1 when it is unknown, as after a timeout). */
bool wdbg_run(AvpArena *a, const WdbgRun *r, const char **cls, const char **msg, i64 *exit_code);
