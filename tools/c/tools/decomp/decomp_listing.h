/* decomp_listing.h: the read-only side of tools/decomp.py for other tools (the briefing tool's gather_listing):
 * where the Ghidra store lives, its index, the symbols.txt stamp and one function's stored listing. Link
 * `tool_decomp` (shsm_add_tool(... LIBS tool_decomp)) and include this header; nothing here starts Ghidra.
 *
 * Python counterparts: decomp.NOTICE, decomp.Paths, decomp.load_index, decomp.symbols_stamp and
 * decomp.listing(repo, address) (export=False). Exceptions are raised as the Python raised them (shsm_cli.h codes):
 * an unreadable symbols.txt is SHSM_EXC_FILE_NOT_FOUND / SHSM_EXC_OS, a listing file that vanished between the
 * index check and the read likewise. */
#pragma once
#include "avp/json.h"
#include "avp/str.h"
#include "shsm_repo.h"

/* decomp.NOTICE: the first line `show` prints; every listing reader repeats it. */
#define DECOMP_NOTICE "// Ghidra listing: a reading aid, not source. Never paste it into src/ (AGENTS.md, Policies)."
#define DECOMP_PROJECT_NAME "shsm"

/* decomp.Paths: everything the tool writes, under build/ghidra/ (os.path.join forms, backslashes). */
typedef struct DecompPaths {
    const ShsmRepo *repo;
    const char *root;        /* build/ghidra */
    const char *state;       /* build/ghidra/state.json */
    const char *project;     /* build/ghidra/project */
    const char *input;       /* build/ghidra/input */
    const char *symbols_tsv; /* input/symbols.tsv */
    const char *applied_tsv; /* input/applied_symbols.tsv */
    const char *decomp;      /* build/ghidra/decomp */
    const char *index;       /* decomp/index.json */
    const char *logs;        /* build/ghidra/logs */
} DecompPaths;

DecompPaths *decomp_paths(AvpArena *a, const ShsmRepo *r);
/* Paths.project_exists(): project/shsm.gpr is a file. */
bool decomp_project_exists(const DecompPaths *p);
/* load_index(paths): index.json as loaded (an object), or {"functions": {}} when it is missing or unreadable. */
AvpJson *decomp_load_index(AvpArena *a, const DecompPaths *p);
/* index["functions"].get("%08X" % address): the entry object, or NULL. */
AvpJson *decomp_index_entry(const AvpJson *index, u64 address);
/* symbols_stamp(path): sha1(file)[:12]. Raises the OSError class of a failed read. */
const char *decomp_symbols_stamp(AvpArena *a, const char *symbols_txt);

/* listing(repo, address): the function's stored listing without exporting.
 * found false: no index entry, or its file is missing (entry then is the entry or NULL; stale false).
 * found true: text (the file, universal newlines), entry (its index object), stale (made from an older symbols.txt). */
typedef struct {
    bool found;
    AvpStr text;
    AvpJson *entry;
    bool stale;
} DecompListing;
DecompListing decomp_listing_at(AvpArena *a, const ShsmRepo *r, u64 address);
/* The absolute path of an entry's listing file (os.path.join(paths.decomp, e["file"])), NULL when e has no file. */
const char *decomp_entry_file(AvpArena *a, const DecompPaths *p, const AvpJson *entry);
