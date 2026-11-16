/* classes_scan.h: the scan engine of classes.exe (tools/classes.py Program, Analysis and Analysis.to_json).
 *
 * classes_scan parses the asm tree and the symbols file, runs the analysis and returns the classes.json
 * document {"inputs", "stats", "methods", "classes"} with `inputs` (the caller's stamp object) as its first
 * member, exactly as Analysis.to_json(stamp) builds it. The data regions come from splits_path (NULL or a
 * missing file: no regions, as run_scan does). Failures raise SHSM_EXC_* through shsm_raise. */
#ifndef CLASSES_SCAN_H
#define CLASSES_SCAN_H

#include "avp/avp.h"

typedef struct ClassesScanOpts {
    const char *asm_dir;      /* --asm */
    const char *symbols_path; /* --symbols */
    const char *splits_path;  /* --splits; NULL when the file does not exist */
    int base_rule;            /* CLASSES_BASE_RULE_SLOTS (0, the reference's slot-overlap rule) or _CTOR (1) */
    const char **note;        /* out (may be NULL): the note on listings of an earlier split the scan skipped, or NULL */
} ClassesScanOpts;

AvpJson *classes_scan(AvpArena *a, const ClassesScanOpts *opts, AvpJson *inputs);

#endif
