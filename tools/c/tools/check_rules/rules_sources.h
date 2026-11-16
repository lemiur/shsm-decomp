/* rules_sources: the source set check_rules reads (load_sources) and the storage-class macros it passes to every
 * parse. See rules_lex.md. */
#pragma once
#include "rules_parse.h"

typedef struct {
    AvpStrVec paths; /* repository-relative, forward slashes, os.walk order (src/ then include/) */
    AvpStrVec texts; /* decoded as rl_read_source */
    AvpMap *index;   /* path -> index */
} RlSources;

/* load_sources(root): every .c .cc .cp .cpp .cxx .h .hh .hpp .hxx .inc file under root/src and root/include. */
RlSources *rl_load_sources(AvpArena *a, const char *root);
/* Checker.parse's storage_macros: scan_storage_macros of every source text, in order (later definitions win). */
AvpMap *rl_project_storage_macros(const RlSources *S);
/* The text of rel: from S when loaded, else read from root/rel (main's `sources[p] = fh.read()`). */
bool rl_source_text(AvpArena *a, const RlSources *S, const char *root, AvpStr rel, AvpStr *out);
