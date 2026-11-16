/* The address-to-library table config/<ver>/libraries.tsv (tools/common/libraries.py): rows (start, end exclusive,
 * library, note) sorted, lookup by address, group of a library name, problems. Documented in shsm_libraries.md. */
#pragma once
#include "avp/str.h"
#include "shsm_repo.h"

typedef struct {
    i64 start, end;
    AvpStr library, note;
} ShsmLibRow;

typedef struct {
    AVP_VEC(ShsmLibRow) rows; /* sorted as Python's sorted(rows): start, end, library, note */
    const char *path;         /* NULL when parsed from text without a path */
} ShsmLibraries;

/* Libraries.parse(text, path): raises SHSM_EXC_VALUE "<path or libraries>:<line>: expected start, end, library,
 * note" for a short row and "...: bad address" for an address int(x, 16) rejects. */
ShsmLibraries *shsm_libraries_parse(AvpArena *a, AvpStr text, const char *path);
/* Libraries.load(repo): the checkout's table, empty when the file is absent (not cached). */
ShsmLibraries *shsm_libraries_load(AvpArena *a, const ShsmRepo *r);
/* load_libraries(repo): parsed once per file stamp (size, mtime_ns) in the process (thread's long-lived arena). */
const ShsmLibraries *shsm_load_libraries(const ShsmRepo *r);
/* row_of(address) / lookup(address): NULL outside every row (address SHSM_NONE-like INT64_MIN gives NULL). */
const ShsmLibRow *shsm_libraries_row_of(const ShsmLibraries *l, i64 address);
const char *shsm_libraries_lookup(AvpArena *a, const ShsmLibraries *l, i64 address);
/* library_of(address, repo) */
const char *shsm_library_of(AvpArena *a, i64 address, const ShsmRepo *r);
/* names(): each library once, in first-row order. */
AvpStrVec shsm_libraries_names(const ShsmLibraries *l);
/* problems(start, end): overlaps, empty rows and (with have_range) gaps, as Python's text lines. */
AvpStrVec shsm_libraries_problems(AvpArena *a, const ShsmLibraries *l, bool have_range, i64 start, i64 end);
/* group_of(lib): 'sdk/os' -> 'sdk'; NULL or "" -> 'unknown'. */
AvpStr shsm_library_group_of(const char *lib);
