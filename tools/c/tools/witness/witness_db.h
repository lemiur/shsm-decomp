/* witness_db: the derived database build/index/witness.sqlite (tools/witness.py _stamp, _TABLES, build, save,
 * load, and Witness.__init__). db_path and lookup are declared in witness.h.
 *
 * The file is shared with the Python tool: the same tables, the same stamp text (so that either language reads the
 * other's file without a rebuild) and the same meta JSON (json.dumps(meta)). */
#ifndef WITNESS_DB_H
#define WITNESS_DB_H

#include "witness_map.h"

struct sqlite3;
struct ShsmAsmIndex;

extern const char WT_TABLES_SQL[]; /* _TABLES */

/* _stamp(r, ix, exe): sha1 hex of "SCHEMA|exe path|member|size|mtime_ns|%r of the files rows". */
const char *wt_stamp(AvpArena *a, struct ShsmAsmIndex *ix, WtExe exe);

/* build(r, ix, exe, jobs): every table's contents and the meta dict. */
typedef struct WtBuilt {
    AvpJson *meta;       /* the meta dict (key order of the Python) */
    WtSide wii, ps2;
    WtRuns wruns, pruns;
    WtCorr *c;           /* the correspondence after run(): c->pairs */
    WtInls inl;          /* c.inlined_rows() */
    WtValidation val;
} WtBuilt;
void wt_build(AvpArena *a, const ShsmRepo *r, struct ShsmAsmIndex *ix, WtExe exe, int jobs, WtBuilt *out);

/* save(con, stamp, meta, ...): replaces every table's rows in one transaction. */
void wt_save(struct sqlite3 *con, const char *stamp, const WtBuilt *b);

/* load(con): the sides, runs, pairs and rev (from the pairs table), c->inl (rows whose evidence is not "call set"),
 * every inl row, and the meta dict. */
typedef struct WtLoaded {
    WtSide wii, ps2;
    WtRuns wruns, pruns;
    WtCorr *c;
    WtInls inl;
    AvpJson *meta;
} WtLoaded;
void wt_load(AvpArena *a, struct sqlite3 *con, WtLoaded *out);

/* Witness(root, exe, rebuild, jobs): finds the checkout and the executable, opens the asm index read-only, builds
 * and saves the database when the stamp changed (or rebuild), then loads it. */
typedef struct WtWitness {
    AvpArena *arena;
    const ShsmRepo *repo;
    WtExe exe;
    struct ShsmAsmIndex *ix;
    WtLoaded m;              /* m.c is self.c; m.wii / m.ps2 are self.wii / self.ps2; m.inl is self.inl */
    bool has_built;          /* self.built is not None */
    double built;            /* round(seconds, 1) */
    double load_seconds;     /* round(seconds, 2) */
    AvpI64Vec ps2_order;     /* sorted(self.ps2) */
    AvpIMap *ps2_rank;       /* address -> index into ps2_order (AvpVal.i) */
} WtWitness;
WtWitness *wt_witness_open(const char *root, const char *exe, bool rebuild, int jobs);
void wt_witness_close(WtWitness *w);

#endif
