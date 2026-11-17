/* mwtrace sched: what the instruction scheduler decided for one function of one compile, and by which numbers.
 *
 * The spec MWT_SCHED_SPEC (mwt_sched_spec.c) breaks in Scheduler.obj: per pass (scheduleinstructions' argument: 0
 * before register allocation, 1 after) and per scheduled block, the nodes as the dependence builder makes them (opcode,
 * latency, register operands), every dependence edge with the call site that added it (its kind) and every may_alias
 * answer, the nodes' height, slack and predecessor count once the block is built, then each selection: the can-issue
 * answers, the incumbent, the successor counts compared and the candidates taken, each issue with its successors'
 * updates, the end of each cycle. It also logs every instruction assemblefunction emits (the object's order) and the
 * register allocator's instruction walk (regalloc's numbers). mwt_sched_parse turns the log into blocks;
 * mwt_sched_check holds the replay, the self-checks, the --against comparison and the pair query.
 * docs/compiler_regalloc.md ("Scheduler") names every address and field. */
#pragma once
#include "avp/avp.h"
#include "mwt_run.h"

extern const char *const MWT_SCHED_SPEC;

typedef AVP_VEC(int) MwtSchedInts;

/* An edge's kind: the call site of the edge adder (0x5bc0b0 in Wii/1.0) that made it. One edge can carry several. */
typedef enum {
    MWT_DEP_RAW = 0,     /* the earlier node defines a register a later node uses (latency: the earlier's) */
    MWT_DEP_WAW,         /* both define it (latency 0 on gekko) */
    MWT_DEP_WAR,         /* the earlier uses a register the later defines (latency 0 on gekko) */
    MWT_DEP_MEM_LS,      /* a load before a store that may_alias says may overlap */
    MWT_DEP_MEM_SL,      /* a store before a load, may_alias 1 */
    MWT_DEP_MEM_SS,      /* a store before a store, may_alias 1 */
    MWT_DEP_ORDERED,     /* two instructions with pcode flag 0x80, kept in order */
    MWT_DEP_SERIAL,      /* the earlier serialises: an edge to every later instruction */
    MWT_DEP_TO_SERIAL,   /* the later serialises */
    MWT_DEP_BRANCH,      /* a node without successors before the block's branch */
    MWT_DEP_OTHER,       /* a call site the trace does not know (its return address in site) */
    MWT_DEP_N
} MwtDepKind;

typedef struct {
    int from, to;        /* node indices: from is earlier in the input order */
    int latency;         /* the edge's latency once the block is built (the greatest asked) */
    u32 kinds;           /* 1 << MwtDepKind */
    AvpStr regs;         /* the registers of its register kinds ("r33 f1"), "" none */
    u32 site;            /* the return address of an MWT_DEP_OTHER edge */
} MwtSchedEdge;
typedef AVP_VEC(MwtSchedEdge) MwtSchedEdges;

/* One memory pair the dependence builder asked about: the precheck (0x5bbcc0, only when [SC_G_PRE] is set) and
 * may_alias. kind 'l' load before store, 's' store before load, 'S' store before store. */
typedef struct {
    int from, to;
    char kind;
    int precheck;        /* -1 not asked; nonzero: no dependence, may_alias not asked */
    int alias;           /* -1 not asked; may_alias's answer */
} MwtSchedMem;
typedef AVP_VEC(MwtSchedMem) MwtSchedMems;

typedef struct {
    int index;           /* position in the block's input order (0-based) */
    u64 ptr, pcode;      /* the scheduler's node, the instruction (stable across passes) */
    AvpStr op;           /* opcode name, as regalloc prints it */
    u32 pflags, pflags2; /* pcode +0x0c, +0x10 */
    AvpStr opnds;        /* register operands: "r33= r32" (= defined, plain used, =+ both) */
    int latency;         /* node +0x10 (the machine model's latency) */
    int height;          /* node +0x16 once the block is built: the longest latency path to the block's end */
    int slack;           /* node +0x14: the block's greatest height - height; critical at cycle c when slack <= c */
    int npreds;          /* node +0x18 once built: its predecessors */
    int prio;            /* the opcode priority byte (pass 1's key 4; smaller wins) */
    int ra;              /* regalloc's instruction number (the first allocation walk after pass 1), -1 not seen */
    int out;             /* its position in the block's output order, -1 not issued */
    int cycle, slot;     /* when it issued, -1 */
    int fpos;            /* its position among the function's emitted instructions, -1 not emitted */
    MwtSchedInts succ, pred; /* its edges (indices into the block's edges) */
} MwtSchedNode;
typedef AVP_VEC(MwtSchedNode) MwtSchedNodes;

/* The keys of the selection (static at 0x5bb7e0), in the order it tries them. */
typedef enum {
    MWT_KEY_NONE = 0,
    MWT_KEY_CRITICAL,    /* critical (slack <= cycle) beats non-critical */
    MWT_KEY_SUCCESSORS,  /* more successors whose only remaining predecessor is this node */
    MWT_KEY_HEIGHT,      /* greater height */
    MWT_KEY_PRIORITY,    /* pass 1 (coloring set) only: the smaller opcode priority byte */
    MWT_KEY_ORDER        /* none differs: the incumbent (earlier in program order) stays */
} MwtSchedKey;

/* One comparison of a candidate with the incumbent. */
typedef struct {
    int cand, inc;
    int cand_succ, inc_succ; /* the successor counts, -1 not compared */
    bool taken;          /* the candidate became the incumbent */
    MwtSchedKey key;     /* the key that decided (from the logged numbers; checked against taken) */
} MwtSchedCmp;
typedef AVP_VEC(MwtSchedCmp) MwtSchedCmps;

typedef struct {
    int cycle, slot;
    MwtSchedInts ready;  /* the nodes the selection found ready and issuable, in list (program) order */
    MwtSchedInts busy;   /* ready by their dependences, but the machine model's can-issue said 0 */
    int incumbent;       /* the first ready node, -1 none */
    int chosen;          /* the node issued, -1 none (the cycle ends) */
    MwtSchedCmps cmps;
    MwtSchedInts upd;    /* the issue's successor updates as logged: triples (successor, its remaining predecessors
                          * after the decrement, issue cycle + edge latency) */
} MwtSchedSel;
typedef AVP_VEC(MwtSchedSel) MwtSchedSels;

typedef struct {
    int pass;            /* 1 before register allocation, 2 after */
    int seq;             /* position among the pass's scheduled blocks (0-based) */
    bool prio_key;       /* key 4 applies (coloring set) */
    u64 ptr;
    int index;           /* block +0x1c */
    int count;           /* instructions */
    u32 flags;           /* block +0x2a */
    int maxh;            /* the greatest height */
    int cycles;          /* cycles used */
    MwtSchedNodes nodes;
    MwtSchedEdges edges;
    MwtSchedMems mems;
    MwtSchedSels sels;
    MwtSchedInts order;  /* output order (node indices) */
} MwtSchedBlock;
typedef AVP_VEC(MwtSchedBlock) MwtSchedBlocks;

typedef struct {
    u64 ptr;
    int index, count;
    u32 flags;
    int sched;           /* the scheduled block's seq, -1 not scheduled */
} MwtSchedVisit;
typedef AVP_VEC(MwtSchedVisit) MwtSchedVisits;

typedef struct {
    int pass;            /* 1, 2 */
    int arg;             /* scheduleinstructions' argument */
    int coloring;        /* [SC_G_COLORING] */
    int mode, proc, x112, pre; /* the schedule mode byte, the processor byte, [G_X112], the precheck flag */
    int width, m4, m8;   /* machine model +0 (issue width), +4, +8 */
    MwtSchedVisits visits;
    MwtSchedBlocks blocks;
} MwtSchedPass;
typedef AVP_VEC(MwtSchedPass) MwtSchedPasses;

typedef struct {
    u64 pcode;
    AvpStr op;
    u64 block;
} MwtSchedEmit;
typedef AVP_VEC(MwtSchedEmit) MwtSchedEmits;

typedef struct {
    u64 obj;
    AvpStr name;
    bool traced;
    MwtSchedPasses passes;
    MwtSchedEmits emitted; /* assemblefunction's instructions, in the object's order */
    AvpIMap *ra;         /* pcode -> regalloc's instruction number (.i) */
} MwtSchedFunc;
typedef AVP_VEC(MwtSchedFunc) MwtSchedFuncs;

typedef struct {
    AvpStr build;
    MwtSchedFuncs funcs;
    AvpIMap *links;      /* obj -> AvpStr* link name */
    AvpStrVec errors;    /* the debugger's own lines and lines the parser could not place */
} MwtSchedTrace;

/* Parses a trace log (the forms are listed in mwt_sched_parse.c). Derived fields (edges' latencies, node issue
 * cycles and output positions, regalloc numbers, emitted positions, comparison keys) are filled. */
void mwt_sched_parse(AvpArena *a, AvpStr log, MwtSchedTrace *t);
/* The traced function of that link or source name; NULL with *why. */
const MwtSchedFunc *mwt_sched_find(AvpArena *a, const MwtSchedTrace *t, const char *sym, const char **why);
AvpStr mwt_sched_link(const MwtSchedTrace *t, const MwtSchedFunc *f);
/* The pass of that number (1, 2), NULL when the function had none. */
const MwtSchedPass *mwt_sched_pass(const MwtSchedFunc *f, int pass);
const char *mwt_sched_dep_name(MwtDepKind k);
const char *mwt_sched_key_name(MwtSchedKey k);
/* The comparison the selection makes of candidate c with incumbent i at cycle: the key that decides and whether c
 * wins. cs, is: their successor counts. */
MwtSchedKey mwt_sched_compare(const MwtSchedBlock *b, int cycle, int c, int i, int cs, int is, bool *c_wins);
/* The key's numbers as words: "height 5 > 4", "priority 2 < 4", "critical (slack 0 <= cycle 1) over slack 3". */
const char *mwt_sched_key_words(AvpArena *a, const MwtSchedBlock *b, int cycle, MwtSchedKey k, int win, int lose,
                                int ws, int ls);
/* A node's label: "#3 LWZ r33= r32". */
const char *mwt_sched_node_label(AvpArena *a, const MwtSchedNode *n);

/* ---- self-check ---- */
typedef struct {
    AvpStr stage;        /* "trace" (the trace is incomplete or contradicts itself) or "after-scheduler" (a later
                          * pass changed the scheduled order) */
    int pass, block;     /* the block's pass and seq, -1 none */
    int pos;             /* the first differing position, -1 none */
    AvpStr why;
} MwtSchedGap;
typedef AVP_VEC(MwtSchedGap) MwtSchedGaps;

typedef struct {
    MwtSchedGaps gaps;
    AvpStrVec notes;     /* what later passes did that the check accounted for (instructions added, deleted) */
    int emitted;         /* instructions assemblefunction emitted */
    int rows;            /* the object's rows (-1 no object) */
    int ops_checked, regs_checked;
} MwtSchedCheck;

/* The replay: every selection's ready set (by the trace's edges, earliest cycles and the can-issue answers) against
 * the logged one, the successor counts, every comparison's key against the logged take, the output order against the
 * issues. Gaps (stage "trace") go to c. */
void mwt_sched_check_trace(AvpArena *a, const MwtSchedFunc *f, MwtSchedCheck *c);
/* The emitted instructions against the object's rows (texts as listed: "lwz r3, 0x4(r4)", opcode by opcode, the
 * registers of pass 2's nodes where they are explicit), then each pass-2 block's output order against the emitted
 * order. rows NULL: the object comparison is a gap. */
void mwt_sched_check_object(AvpArena *a, const MwtSchedFunc *f, const AvpStr *rows, size_t nrows, MwtSchedCheck *c);
/* Whether the opcode name the compiler emitted can be the object's mnemonic ("ADDI" and "li", "RLWINM" and "slwi"). */
bool mwt_sched_op_matches(AvpStr op, AvpStr row);

/* ---- the pair query ---- */
typedef struct {
    int pass, block;     /* the block (pass, seq) */
    int a, b;            /* the two nodes */
    int first, second;   /* the one issued first, the other */
    int cycle, slot;     /* the selection that issued first */
    int sel;             /* its index in the block's sels */
    bool both_ready;     /* second was ready and issuable at that selection */
    MwtSchedKey key;     /* both ready: the key that decides between them (first as the winner) */
    AvpStr key_words;
    AvpStr cmp_words;    /* both ready: the comparison the selection actually made of second (with whom, which key) */
    AvpStr held;         /* not both ready: why second was not (the edge from an unissued predecessor with its kind,
                          * the earliest cycle an issued predecessor's latency gives, or can-issue 0) */
} MwtSchedPair;

/* The (pass 2) block and node of the function's emitted instruction at fpos. false when fpos is not a scheduled
 * node of that pass. */
bool mwt_sched_locate(const MwtSchedFunc *f, int pass, int fpos, int *block, int *node);
/* The pair query: in block (pass, seq), nodes a and b. false with *why (no such block or node, a node not issued). */
bool mwt_sched_pair(AvpArena *a, const MwtSchedFunc *f, int pass, int block, int na, int nb, MwtSchedPair *out,
                    const char **why);
AvpStr mwt_sched_pair_text(AvpArena *a, const MwtSchedFunc *f, const MwtSchedPair *p);
AvpJson *mwt_sched_pair_json(AvpArena *a, const MwtSchedFunc *f, const MwtSchedPair *p);

/* ---- comparison of two traces (--against) ---- */
/* Plain-fact lines: the first pass, block and selection where the two traces issue a different node (nodes are
 * matched by opcode and occurrence in the block), with both decisions and their keys, and the nodes only one side's
 * block has. Empty when the schedules agree. */
AvpStrVec mwt_sched_diff(AvpArena *a, const MwtSchedFunc *A, const MwtSchedFunc *B);

/* ---- reports ---- */
AvpStr mwt_sched_text(AvpArena *a, const MwtSchedTrace *t, const MwtSchedFunc *f, const MwtSchedCheck *c);
AvpJson *mwt_sched_json(AvpArena *a, const MwtSchedTrace *t, const MwtSchedFunc *f, const MwtSchedCheck *c);

/* ---- one traced compile (the subcommand's work, reusable by other tools) ---- */
typedef struct {
    MwtJob job;
    MwtSchedTrace trace;
    const MwtSchedFunc *fn;
    MwtSchedCheck check; /* check.gaps empty: the trace replays and equals the object */
    double secs;         /* the compile under the debugger */
    double total_secs;   /* everything the run did */
    const char *error;   /* the run could not be made */
} MwtSchedRun;
/* Compiles the unit (or src) under the debugger with MWT_SCHED_SPEC for one function, parses the log and runs the
 * self-checks (the replay, the object's rows). false with r->error when the run could not be made. */
bool mwt_sched_run(AvpArena *a, const char *unit, const char *src, const AvpStrVec *adds, const AvpStrVec *drops,
                   const char *outdir, const char *sym, MwtSchedRun *r);

/* Every function of one compile at once (the spec's `only` empty), as mwt_ra_unit_run does for regalloc. */
typedef struct {
    MwtJob job;
    MwtSchedTrace trace; /* every function the compile scheduled */
    double secs, total_secs;
    AvpMap *om;          /* the compile's object (cc_load_object), NULL when unreadable */
} MwtSchedUnit;
bool mwt_sched_unit_run(AvpArena *a, const char *unit, const char *src, const AvpStrVec *adds, const AvpStrVec *drops,
                        const char *outdir, MwtSchedUnit *u, const char **why);
/* One function of a unit trace with its self-checks. false with r->error when the trace has no such function. */
bool mwt_sched_unit_fn(AvpArena *a, const MwtSchedUnit *u, const char *sym, MwtSchedRun *r);

/* The subcommand: mwtrace.exe sched <unit> --function SYM [options]. */
int mwt_sched_main(AvpArena *a, int argc, char **argv);
