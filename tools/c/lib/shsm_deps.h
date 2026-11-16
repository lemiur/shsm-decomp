/* Header dependencies of the built objects, as ninja recorded them (`ninja -t deps`), and whether an object is older
 * than one of its inputs. Used by gatecheck layout (a verdict read from a stale object) and match --changed (the units
 * a header edit touched). */
#pragma once
#include "avp/map.h"
#include "avp/str.h"
#include "avp/vec.h"

typedef struct ShsmDeps ShsmDeps;

/* `<root>/tools/bin/ninja.exe -t deps` in root: object path as ninja prints it (build/R5WEA4/src/game/X.o) -> its
 * inputs. Without ninja, or when it fails, the table is empty (no header is then known). */
ShsmDeps *shsm_deps_load(AvpArena *a, const char *root);

/* The first input of obj_rel that is newer than `than` (seconds since 1970, as avp_getmtime): its source file
 * (src_rel, root-relative; NULL: none), each extra path (absolute or root-relative; NULL entries skipped) and each
 * header ninja recorded for obj_rel. Returns the path as recorded; NULL when none is newer. */
const char *shsm_deps_newer_input(AvpArena *a, const ShsmDeps *d, const char *root, const char *obj_rel,
                                  const char *src_rel, const char *const *extra, size_t nextra, double than);
