/* The debugger half of wdbg.exe: the Windows debug-API loop of tools/mwcc_trace/wdbg.py over a 32-bit mwcceppc.exe,
 * and the WdbgHost a spec runs against (memory, log, snapshots, compiler symbols). */
#pragma once
#include "wdbg_spec.h"

typedef struct WdbgDebug {
    void *process;      /* HANDLE of the debuggee (CreateProcessW's) */
    const char *outdir; /* as given on the command line: the log and the snapshots go under it */
    void *log;          /* AvpFile * of outdir/log.txt */
    void *syms;         /* the compiler's NB11 names, loaded on the first sym() */
    bool syms_tried;
    const char *exe;    /* the compiler being run (tools/compilers/<build>/mwcceppc.exe), whose names sym() and addr() read */
    const char *build;  /* its build, "Wii/1.0a" */
    bool exited;        /* the loop saw the exit-process event */
    u32 exit_code;      /* its exit code */
} WdbgDebug;

/* The host callbacks over d. */
WdbgHost wdbg_debug_host(WdbgDebug *d);

/* D.out(text): one line to the log, flushed. */
void wdbg_debug_out(WdbgDebug *d, AvpStr text);

/* The event loop until the debuggee's exit or a 120 s wait without an event ('timeout' in the log). */
void wdbg_debug_loop(WdbgDebug *d, WdbgSpec *spec);
