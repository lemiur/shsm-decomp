/* The pieces every mwtrace subcommand shares (inline now; regalloc later): a unit resolved to one compile (its
 * source or a scratch text, its compiler build and flags, flag words added or dropped), that compile run under the
 * debugger with a generated spec (wdbg_run), and the calls of the object it wrote. */
#pragma once
#include "avp/avp.h"
#include "mwt_build.h"
#include "shsm_repo.h"
#include "shsm_units.h"

typedef struct {
    const ShsmRepo *repo;
    const ShsmUnit *unit;
    const char *build;     /* "Wii/1.0a" (the unit's mw_version with '/') */
    const MwtBuild *table; /* its address table */
    const char *source;    /* the file compiled: absolute */
    bool scratch;          /* source came from --src */
    AvpStrVec flags;       /* the compiler arguments used (the unit's, with the words added and dropped) */
    AvpStrVec added;       /* each --flag as given */
    AvpStrVec dropped;     /* each --drop-flag as given */
    const char *outdir;    /* absolute; the compile's log, spec and object go here */
    bool quiet;            /* the compiler's console output goes to <outdir>/compiler.txt (mwt_set_quiet) */
} MwtJob;

/* Resolves unit ("main/x/y" or "x/y"), checks that mwtrace has addresses for its compiler build and that the exe has
 * their hash, and applies the flag changes: each add is shlex-split and appended; each drop is shlex-split and every
 * run of those words in the flags is removed (an error when there is none). outdir is created. false with *why. */
bool mwt_job_init(AvpArena *a, MwtJob *j, const char *unit, const char *src, const AvpStrVec *adds,
                  const AvpStrVec *drops, const char *outdir, const char **why);

/* Every job initialised after mwt_set_quiet(true) sends the compiler's stdout and stderr to <outdir>/compiler.txt
 * instead of this process's console (a tool that runs many traces; mwtrace.exe itself leaves it off). */
void mwt_set_quiet(bool quiet);

/* Writes spec_text to <outdir>/<spec_name> and compiles the job's source into <outdir>/o.o under the debugger with
 * it. *log is the log text (<outdir>/log.txt), *secs the wall time of the run, *exit_code the compiler's. */
bool mwt_job_run(AvpArena *a, const MwtJob *j, AvpStr spec_text, const char *spec_name, AvpStr *log, double *secs,
                 i64 *exit_code, const char **why);

/* ---- the object's calls ---- */
typedef struct {
    u32 offset;  /* of the branch, from the function's start */
    AvpStr target; /* the symbol it reaches (a section symbol resolved to the function at its addend) */
} MwtCall;
typedef AVP_VEC(MwtCall) MwtCalls;

typedef struct {
    AvpStr name; /* the function symbol */
    MwtCalls calls; /* R_PPC_REL24 relocations inside it, by offset */
} MwtObjFunc;
typedef AVP_VEC(MwtObjFunc) MwtObjFuncs;

/* The defined STT_FUNC symbols of the object and the REL24 branches inside each. false with *why. */
bool mwt_obj_calls(AvpArena *a, const char *path, MwtObjFuncs *out, const char **why);
/* The function of that name, or NULL. */
const MwtObjFunc *mwt_obj_find(const MwtObjFuncs *fs, AvpStr name);
