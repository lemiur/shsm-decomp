/* flagsweep: a port of tools/flagsweep.py (its docstring is the spec). Compiles one unit once per variant (compiler
 * version x flag alternatives), compares every function with the retail listing through the shared comparison
 * library (cc_codecmp) and prints a matrix and a ranking, or the --json document.
 *
 * Flags are (name, arg) pairs; arg.p == NULL is the Python's None. Strings live in the arena given. Errors the Python
 * raised as SystemExit(msg) end the process through avp_fatal. cc.unit_flags, permute.find_source and shlex come from
 * tool_permute (permute.h). */
#pragma once
#include "avp/avp.h"
#include "cc_codecmp.h"

typedef struct {
    AvpStr name, arg; /* arg.p == NULL: a flag without an argument */
} FsFlag;
typedef AVP_VEC(FsFlag) FsFlags;

/* ---- flag handling ---------------------------------------------------------------------------------------------- */
FsFlags fs_parse_flags(const AvpStr *tokens, size_t n);
FsFlags fs_parse_spec(AvpArena *a, AvpStr text); /* avp_fatal on a quoting error (shlex's ValueError) */
AvpStrVec fs_flag_tokens(const FsFlags *flags);
AvpStr fs_flag_str(AvpArena *a, FsFlag f);           /* str(Flag) */
AvpStr fs_family(AvpArena *a, FsFlag f);
/* apply_axis(flags, alternative, families) */
FsFlags fs_apply_axis(AvpArena *a, const FsFlags *flags, const FsFlags *alt, const AvpStrVec *families);

typedef struct {
    FsFlags *alts;   /* alternatives */
    AvpStr *labels;
    size_t n;
    AvpStrVec families;
} FsAxis;
FsAxis fs_parse_axis(AvpArena *a, AvpStr text); /* avp_fatal "--vary %r names no flag" */
/* resolve_versions(text, compilers) */
AvpStrVec fs_resolve_versions(AvpArena *a, AvpStr text, const char *compilers);

typedef struct {
    AvpStr id, label, version;
    FsFlags flags;
    AvpStrVec choices;
    bool current;
    /* filled by the compile */
    bool compiled;
    AvpMap *fns;        /* name -> CcFn* */
    AvpStr error;       /* .p == NULL: None */
    bool has_seconds;
    double seconds;
} FsVariant;
typedef AVP_VEC(FsVariant) FsVariants;

/* build_variants(base_version, base_flags, versions, axes, sets, drops, include_base, max_variants) */
FsVariants fs_build_variants(AvpArena *a, AvpStr base_version, const FsFlags *base_flags, const AvpStrVec *versions,
                             const AvpStrVec *axes, const AvpStrVec *sets, const AvpStrVec *drops, bool include_base,
                             i64 max_variants);

/* ---- comparison ------------------------------------------------------------------------------------------------- */
/* norm_row(line): cc.norm plus the compiler-generated names (the --no-targets normalisation) */
AvpStr fs_norm_row(AvpArena *a, AvpStr line);
bool fs_wanted(AvpStr name, const AvpStrVec *symbols);

typedef struct {
    bool exact;
    size_t diff, rows, targets, unresolved;
    const char *status; /* ok | diff | target | missing */
} FsScore;
/* score_variant(retail, compiled, ctx, targets): one FsScore per retail function, in retail order. */
FsScore *fs_score_variant(AvpArena *a, const AvpMap *retail, const AvpMap *compiled, CcContext *ctx, bool targets);

/* first_error(text) */
AvpStr fs_first_error(AvpArena *a, AvpStr text);
/* cell(f) */
AvpStr fs_cell(AvpArena *a, const FsScore *f);

/* ---- results ---------------------------------------------------------------------------------------------------- */
typedef struct {
    const FsVariant *v;
    bool ok;
    i64 exact, diff, targets, unresolved; /* None (compile failed) when !ok */
    FsScore *fns;                          /* parallel to the result's functions */
} FsEntry;

typedef struct {
    AvpStrVec functions; /* the universe, retail order */
    size_t retail_functions;
    FsEntry *entries;
    size_t nentries;
    size_t *ranking;     /* entry indices, best first */
    double seconds;
} FsResult;

/* rank_entries */
void fs_rank(FsResult *res);
/* format_report(res, header, show_flags, width) */
AvpStr fs_format_report(AvpArena *a, const FsResult *res, AvpStr header, bool show_flags, i64 width);

int flagsweep_main(int argc, char **argv);
