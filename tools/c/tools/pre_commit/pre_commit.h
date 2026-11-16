/* pre_commit: the pre-commit hook (a port of python tools/hooks/pre_commit.py). Refuses staged game files, build
 * output, toolchain binaries and binary blobs (exit 1), then prints the rule checker's findings as a warning. */
#pragma once
#include "avp/avp.h"

#define PRE_COMMIT_MB (1024 * 1024)
#define PRE_COMMIT_MAX_RULE_LINES 30
/* The longest ls-files command line sent in one piece (a Windows command line holds 32767 characters). */
#define PRE_COMMIT_CMDLINE_MAX 30000

typedef struct PreCommitEntry {
    const char *path;   /* posix path as git prints it */
    i64 size;
    bool binary;        /* a NUL in the first 8000 bytes of the staged blob */
} PreCommitEntry;
typedef AVP_VEC(PreCommitEntry) PreCommitEntries;

typedef struct PreCommitRefusal {
    const char *path, *reason;
} PreCommitRefusal;
typedef AVP_VEC(PreCommitRefusal) PreCommitRefusals;

/* decide(entries): every refused entry with its reason; empty means the commit may go ahead. */
PreCommitRefusals pre_commit_decide(AvpArena *a, const PreCommitEntry *entries, size_t n);

/* The staged names split into ls-files batches: [start, end) index pairs. One batch when the whole command line fits
 * in PRE_COMMIT_CMDLINE_MAX (the Python's single call), else consecutive batches that each fit. */
typedef struct PreCommitBatch {
    size_t start, end;
} PreCommitBatch;
typedef AVP_VEC(PreCommitBatch) PreCommitBatches;
PreCommitBatches pre_commit_batches(const AvpStr *names, size_t n, size_t limit);

/* staged_entries() in the current folder (the top level). Raises SHSM_EXC_USAGE "git <args>: <stderr>" when git
 * fails (the Python's RuntimeError), SHSM_EXC_OS when git cannot start. */
PreCommitEntries pre_commit_staged_entries(AvpArena *a);

/* main(argv): the exit code. */
int pre_commit_main(int argc, char **argv);
