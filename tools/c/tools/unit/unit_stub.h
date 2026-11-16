/* unit_stub: what `new` writes (tools/unit.py lines 1234-1473): the source and header stubs, the splits.txt entry
 * and its place, and the configure.py Object line. See API.md. */
#pragma once
#include "unit_infer.h"

/* header_guard(path): GAME_CFOO_H. */
const char *unit_header_guard(AvpArena *a, const char *path);

/* The names the headers of include/ declare: namespaces, and classes (class, struct, union) that are defined. */
typedef struct UnitDecls UnitDecls;
UnitDecls *unit_decls_scan(AvpArena *a, const char *include_dir); /* a folder that is not there: nothing declared */
UnitDecls *unit_decls_from_texts(AvpArena *a, const AvpStr *texts, size_t n);
bool unit_decls_has_namespace(const UnitDecls *x, const char *name); /* x NULL: false */
bool unit_decls_has_class(const UnitDecls *x, const char *name);
const char *unit_decls_class_header(const UnitDecls *x, const char *name); /* include-relative path of its header, or NULL */

/* build_source(path, funcs, ...): the source text; *header the header text, or NULL (only game/climax C++ units
 * get one). factory_names / vtable_names / sinit_names are sets of function names (NULL: empty). decls (NULL: nothing
 * declared) decides, for a unit without a generated header, whether the one name a function is qualified with is a
 * namespace (the stub goes in a `namespace X { }` block) or a declared class (a member definition); an undeclared
 * class keeps the "signature not expanded" comment form. */
const char *unit_build_source(AvpArena *a, const char *path, const UnitSymPtrVec *funcs, bool confirmed,
                              const char *name_evidence, const AvpStrVec *data_note, AvpSet *factory_names,
                              AvpSet *vtable_names, AvpSet *sinit_names, const UnitDecls *decls, const char **header);

/* true when path ends in .c, .cpp, .cp or .s (the unit source extensions `new` accepts). A `.s` path gets an
 * assembly stub (header NULL); name_evidence (NULL: none) replaces the "unit name unconfirmed" comment. */
bool unit_has_source_ext(const char *path);

/* split_entry_text(path, ranges): "path:\n\t.text       start:0x... end:0x...\n..." in SPLIT_SECTIONS order. */
const char *unit_split_entry_text(AvpArena *a, const char *path, const UnitRanges *ranges);

/* infer_lib(cfg, path, forced): the library index (raises UNIT_TOOL_ERROR). */
size_t unit_infer_lib(AvpArena *a, const UnitConfig *cfg, const char *path, const char *forced);

/* insert_object(cfg, splits, lib, path, start): configure.py text with Object(NonMatching, path) inserted. */
AvpStr unit_insert_object(AvpArena *a, const UnitConfig *cfg, const UnitSplits *splits, size_t lib, const char *path,
                          i64 start);

/* data_reader(idx, splits, includes): the split whose code reads most of the objects, or NULL. */
const UnitSplit *unit_data_reader(const UnitIndex *x, const UnitSplits *splits, const AvpStrVec *includes);

/* data_only_position(splits, ranges, reader): true and the line index to insert before (false: at the end); *why
 * a warning or NULL. */
bool unit_data_only_position(const UnitSplits *splits, const UnitRanges *ranges, const UnitSplit *reader, size_t *line,
                             const char **why);

/* The splits.txt text with the entry inserted before line `pos` (has_pos false: appended). */
AvpStr unit_splits_with_entry(AvpArena *a, AvpStr stext, const char *entry, bool has_pos, size_t pos);
