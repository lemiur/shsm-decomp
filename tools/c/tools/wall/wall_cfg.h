/* wall_cfg: control-flow graphs of tools/wall.py (L166-371): Block, Cfg (basic blocks, back edges, natural loops,
 * summary), _iso_kind, match_cfgs (bounded isomorphism of two graphs) and _lis_keep.
 *
 * A public API: tools/eqcheck.py imports wall.Cfg, wall.match_cfgs, wall.row_kind (and the kind constants) and
 * wall.is_self_call; its C port links this header and wall_rows.h. Self-contained: include it alone.
 *
 * Memory: everything a function here returns lives in the arena passed to it and stays valid for that arena's
 * life (nothing is freed separately); temporaries use the thread's scratch arena under a mark (a may itself be the
 * scratch arena: the temporaries are then left to the caller's reset). A WallCfg keeps a pointer to the rows it was built from: they must outlive it.
 * Block and row indices are int / size_t; "None" in the Python is -1 (or a false flag) here. */
#pragma once
#include "avp/avp.h"
#include "cc_listing.h"
#include "wall_rows.h"

/* Block (wall.py L166-173). */
typedef struct {
    int id;         /* index in WallCfg.blocks (address order) */
    size_t start;   /* first row */
    size_t end;     /* one past the last row */
    WallKind kind;  /* the last row's kind when it is a terminator, else WALL_K_FALL */
    int *succ;      /* successors, the Python's order: cond = [taken, fall-through], goto = [target], fall/condret/
                     * condtail = [next], switch = every orphan block (approximation), ret/tail = [] */
    size_t nsucc;
    int *preds;     /* predecessors: for each block in id order, for each of its succ (duplicates kept) */
    size_t npreds;
} WallBlock;

/* One natural loop (the dicts of Cfg.loops): header block, its body blocks ascending, and where the exit test
 * sits: "top-test", "bottom-test", "middle-test" or "no-exit" (a static string). */
typedef struct {
    int header;
    int *blocks;
    size_t nblocks;
    const char *kind;
} WallLoop;

typedef struct {
    int u, v; /* back edge u -> v (v is a loop header), the Python's tuple (u, v) */
} WallEdge;

/* Cfg (wall.py L176-279). */
typedef struct WallCfg {
    const CcRow *rows;     /* not owned */
    size_t nrows;
    WallBlock *blocks;
    size_t nblocks;
    int *block_of;         /* [nrows]: the block of each row */
    bool approx_switch;    /* a switch block's successors were approximated */
    WallEdge *back_edges;  /* in DFS discovery order (iterative DFS from block 0, successors in order) */
    size_t nback_edges;
    WallLoop *loops;       /* by header ascending */
    size_t nloops;
    bool *reachable;       /* [nblocks]: reached from block 0 (the Python's set Cfg.reachable) */
    size_t nreachable;     /* len(Cfg.reachable); 0 for a graph without blocks (the Python leaves it unset) */
} WallCfg;

/* Cfg(rows): cuts rows into basic blocks and computes successors, predecessors, back edges, reachability and loops.
 * rows may be NULL when nrows is 0. Allocates from a. */
WallCfg *wall_cfg_new(AvpArena *a, const CcRow *rows, size_t nrows);
/* Cfg(fn.rows). */
WallCfg *wall_cfg_of_fn(AvpArena *a, const CcFn *fn);
/* Cfg.summary(): {"blocks", "edges", "back_edges", "loops": {kind: count, first-seen order}, "exits", "returns",
 * "switches", "conds"} as an AvpJson object in arena a. */
AvpJson *wall_cfg_summary(AvpArena *a, const WallCfg *g);
/* Block.to_dict(): {"id", "rows": [start, end], "kind", "succ"}. */
AvpJson *wall_block_to_dict(AvpArena *a, const WallBlock *b);
/* One Cfg.loops entry as its dict {"header", "blocks", "kind"}. */
AvpJson *wall_loop_to_dict(AvpArena *a, const WallLoop *lp);

/* _iso_kind(k): fall counts as goto when graphs are compared. */
WallKind wall_iso_kind(WallKind k);

/* match_cfgs (wall.py L290-352). */
#define WALL_MATCH_BUDGET 50000
/* sim(a, b): a content similarity of retail block a and compiled block b (the Python's sim callable; its callers
 * return Counter sums, so an integer). Called four times per two-way conditional block, in the Python's order:
 * sim(sa0, sb1), sim(sa1, sb0), sim(sa0, sb0), sim(sa1, sb1). */
typedef i64 (*WallSimFn)(void *ctx, int a, int b);

typedef enum {
    WALL_ISO_NONE = 0, /* the Python's None: no isomorphism */
    WALL_ISO_MAPPING,  /* the tuple (mapping, swapped) */
    WALL_ISO_BUDGET,   /* the string 'budget': the bounded search gave up */
} WallIsoStatus;

typedef struct {
    WallIsoStatus status;
    /* WALL_ISO_MAPPING only (zeroed otherwise): */
    int *map;        /* [nmap_a]: retail block -> compiled block, -1 where unmapped (mapping.get(a)) */
    size_t nmap_a;   /* ga->nblocks */
    int *order;      /* the mapping dict's keys in insertion order (the DFS order); norder == len(mapping) */
    size_t norder;
    int *swaps;      /* the swapped list: retail conditional blocks paired crosswise (inverted conditions) */
    size_t nswaps;
} WallIso;

/* match_cfgs(ga, gb, budget, sim): isomorphism of two graphs from their entries. A conditional block may pair its
 * successors crosswise; the straight pairing is tried first unless sim (when non-NULL) prefers the crossed one.
 * budget is the number of block pairings allowed in total (never restored on backtracking; the Python default is
 * WALL_MATCH_BUDGET). Both graphs empty gives a MAPPING with no entries; one empty, NONE. The Python recurses once
 * per conditional block (recursion limit raised to 20000); this port keeps an explicit stack, so it has no depth
 * limit (where the Python would raise RecursionError, beyond ~20000 nested conditionals, this returns a result).
 * The result's arrays are in arena a; working memory is heap-allocated and freed. */
WallIso wall_match_cfgs(AvpArena *a, const WallCfg *ga, const WallCfg *gb, i64 budget, WallSimFn sim, void *ctx);
/* m1[0] == m2[0] for two MAPPING results: the mapping dicts are equal (order ignored). False unless both are
 * MAPPING. */
bool wall_iso_mapping_equal(const WallIso *m1, const WallIso *m2);
/* The mapping as a JSON object {str(a): b} in insertion order (json.dumps of the dict). */
AvpJson *wall_iso_mapping_json(AvpArena *a, const WallIso *m);

/* _lis_keep(seq) (wall.py L355-371): the indices of one longest strictly increasing subsequence (bisect_left
 * patience sorting, the same tie-breaks). Returns a bool array [n] in arena a: keep[i] is `i in keep`. */
bool *wall_lis_keep(AvpArena *a, const i64 *seq, size_t n);
