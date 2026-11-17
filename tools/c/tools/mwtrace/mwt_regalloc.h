/* mwtrace regalloc: what the register allocator decided for one function of one compile, and by which numbers.
 *
 * The spec MWT_REGALLOC_SPEC (mwt_regalloc_spec.c) breaks in colorinstructions' pipeline (Coloring.obj,
 * InterferenceGraph.obj): per register class and per colouring attempt, the graph's virtual nodes once it is complete
 * (their variable, coalescing root, flags, degree), the coalescing merges and refusals, simplify's pushes (sweep,
 * optimistic choice with its candidates' spill cost and degree), select's pops (the registers the neighbours leave
 * free, the nonvolatile register added when none is, the colour picked, a spill). mwt_regalloc_parse turns the log
 * into passes; mwt_regalloc_check holds the two self-checks (against the -sym on object's DWARF and its prologue)
 * and the comparison of two traces (--against). docs/compiler_regalloc.md names every address and field. */
#pragma once
#include "avp/avp.h"
#include "cc_dwdump.h"
#include "mwt_run.h"

extern const char *const MWT_REGALLOC_SPEC;

/* The register classes of the back end (the class byte coloring_class). */
enum { MWT_RC_SPR = 0, MWT_RC_CR = 1, MWT_RC_VR = 2, MWT_RC_FPR = 3, MWT_RC_GPR = 4, MWT_RC_N = 5 };

/* How simplify removed a node: in a sweep with degree < K (low), as the optimistic choice when every remaining
 * node had degree >= K (optimistic), or in a sweep after an optimistic choice (after). */
typedef enum { MWT_PUSH_NONE = 0, MWT_PUSH_LOW, MWT_PUSH_OPTIMISTIC, MWT_PUSH_AFTER } MwtPushHow;

typedef AVP_VEC(int) MwtInts;

typedef struct {
    int index;           /* the virtual register number (node index) */
    int alias;           /* its coalescing root (union-find; the node itself when not coalesced) */
    u32 flags;           /* node +0x14 when the graph is complete */
    int degree;          /* node +0x16 when the graph is complete */
    u64 obj;             /* node +0x04: the Object it stands for (0: a temporary) */
    bool inherited;      /* obj was copied from a node coalesced into this one (it has none of its own) */
    AvpStr name;         /* the Object's name, "" for a temporary */
    int kind;            /* 'p' parameter, 'l' local, 'o' another object, 't' temporary */
    AvpStr orig;         /* an inlined callee's local or parameter object (renamed @NNN): its name in the callee */
    AvpStr inl_from;     /* that callee's name */
    bool inl_param;      /* a parameter object of the callee (else a local) */
    AvpStr key;          /* the name two traces are matched by: name, or callee:orig#k for an inlined object */
    u64 remat;           /* node +0x08: its single defining instruction when it can be rematerialised, else 0 */
    i64 def_at;          /* the instruction (numbered in block order at graph build) of its first definition, -1 */
    AvpStr def_op;       /* that instruction's opcode name */
    int ndefs;
    i64 cost;            /* node +0x0c at select (estimatespillcosts' sum), meaningful when the pass estimated */
    int push_pos;        /* position in simplify's push order, -1 not pushed */
    MwtPushHow push_how;
    int push_sweep;      /* the sweep (1-based, counted over the whole simplify) */
    int push_degree;     /* its degree (+0x16) when it was pushed */
    int select_pos;      /* position in select's pop order, -1 not popped */
    u32 free_mask;       /* the class's registers its neighbours left free at its first try (before a nonvolatile
                          * register was added) */
    bool free_seen;
    int nv_added;        /* the nonvolatile register select added before colouring it, -1 none */
    int colour;          /* the register select picked (-1 none) */
    bool rewritten;      /* the operand rewrite gave its Object's VarInfo the colour ([Object +0x40] +9 set); a variable
                          * whose node it skips keeps the virtual register number, which the -sym on DWARF records */
    bool spilled;
    MwtInts phys;        /* the physical registers it interferes with (edges to nodes 0..nreal-1) */
    MwtInts members;     /* the nodes coalesced into it (alias == this, other than itself) */
    int final_reg;       /* the register it ends in: its root's colour, the root itself when the root is physical;
                          * -1 spilled or none */
} MwtRaNode;
typedef AVP_VEC(MwtRaNode) MwtRaNodes;

typedef struct {
    u64 obj;
    AvpStr name;
    bool is_param;
    int vflags;          /* VarInfo +0x0e: 0x2 in a register, 0x4 a register pair */
    int vclass;          /* VarInfo +0x0f */
    int reg, reg2;       /* VarInfo +0x10, +0x12: its virtual register (and the pair's second) */
} MwtRaVar;
typedef AVP_VEC(MwtRaVar) MwtRaVars;

typedef struct {
    int lo, hi;          /* the two roots of a copy */
    AvpStr why;          /* "" merged (alias[hi] = lo); "interferes", "restriction", "set" refused */
} MwtRaMerge;
typedef AVP_VEC(MwtRaMerge) MwtRaMerges;

typedef struct {
    int index;
    i64 cost;
    int degree;
    u32 flags;
} MwtRaCand;
typedef AVP_VEC(MwtRaCand) MwtRaCands;

typedef struct {
    int chosen;          /* the node pushed optimistically */
    MwtRaCands cands;    /* the high-degree list it was chosen from, in the order compared */
} MwtRaChoice;
typedef AVP_VEC(MwtRaChoice) MwtRaChoices;

typedef struct {
    int cls, iter;       /* register class; colouring attempt (0 first; > 0 after spill code) */
    int nreal, used;     /* physical registers of the class (nodes 0..nreal-1); nodes in all */
    int K;               /* available_registers(class): simplify's degree bound */
    bool vle;            /* the VLE byte: simplify's stack is re-sorted by node +0x1a when set (GPR only) */
    u32 vol, avail;      /* select's start mask (volatile registers) and the class's allocatable mask */
    bool estimated;      /* estimatespillcosts ran (simplify blocked) */
    int sweeps;
    MwtRaNodes nodes;    /* the virtual nodes nreal..used-1, by index - nreal */
    MwtRaVars vars;      /* the parameters and locals of this class (63d930's walk) */
    MwtRaMerges merges;
    MwtRaChoices choices;
    MwtInts push;        /* simplify's push order */
    MwtInts select;      /* select's pop order (each node once) */
    MwtInts nv;          /* the nonvolatile registers select added, in order */
    MwtInts spills;
    int ok;              /* select's answer: 1 coloured, 0 spill, -1 not seen */
} MwtRaPass;
typedef AVP_VEC(MwtRaPass) MwtRaPasses;

typedef struct {
    u64 obj;
    AvpStr name;         /* the Object's name */
    bool traced;         /* the spec traced it (the --function filter) */
    MwtRaPasses passes;
} MwtRaFunc;
typedef AVP_VEC(MwtRaFunc) MwtRaFuncs;

typedef struct {
    AvpStr build;
    MwtRaFuncs funcs;
    AvpIMap *links;      /* obj -> AvpStr* link name */
    AvpStrVec errors;
} MwtRaTrace;

/* Parses a trace log (the line forms are listed in mwt_regalloc_parse.c). Derived fields (members, final_reg,
 * push and select positions, kinds) are filled. */
void mwt_ra_parse(AvpArena *a, AvpStr log, MwtRaTrace *t);
/* The link name of a function (its LN line), else its source name. */
AvpStr mwt_ra_link(const MwtRaTrace *t, const MwtRaFunc *f);
/* The traced function of that link or source name; NULL with *why (none, or several). */
const MwtRaFunc *mwt_ra_find(AvpArena *a, const MwtRaTrace *t, const char *sym, const char **why);
/* The last pass of a class (the attempt that coloured), NULL when the class was not allocated. */
const MwtRaPass *mwt_ra_final(const MwtRaFunc *f, int cls);
/* The source-level name of a mangled symbol, the name its Object carries ("set__Q24FMOD7MemPoolFiii" -> "set",
 * "__ct__Q24FMOD4TestFv" -> "__ct", a C name unchanged). */
AvpStr mwt_ra_source_name(AvpStr sym);
/* A register of a class as text: "r31", "f1", "cr2", "v3", "spr5". */
const char *mwt_ra_reg(AvpArena *a, int cls, int reg);
/* A mask of a class's registers as text: "r0 r3-r12", "none". */
const char *mwt_ra_mask(AvpArena *a, u32 m, int cls);
/* A node's label: its variable's name, or "temp" with its first definition. */
AvpStr mwt_ra_label(AvpArena *a, const MwtRaNode *n);

/* ---- self-check ---- */
typedef struct {
    AvpStr what;         /* "dwarf", "prologue", "code" */
    AvpStr why;          /* the first disagreement in words */
} MwtRaGap;
typedef AVP_VEC(MwtRaGap) MwtRaGaps;

typedef struct {
    MwtRaGaps gaps;
    AvpStrVec checked;   /* "name r31" for each variable compared */
    AvpStrVec uncovered; /* variables the DWARF check could not cover, with the reason */
    u32 trace_nv[MWT_RC_N], saved_nv[MWT_RC_N]; /* nonvolatile registers: assigned by the trace, saved by the code */
    bool prologue_checked;
} MwtRaCheck;

/* One variable of the -sym on object's DWARF for the function: name and location ("r31", "f30", "r1+0x8"). */
typedef struct {
    AvpStr name;
    AvpStr loc;
    bool is_param;
} MwtDwVar;
typedef AVP_VEC(MwtDwVar) MwtDwVars;

/* The saved nonvolatile registers of a function's code from its rows' texts ("stw r31, 0x1c(r1)", "stmw r27,
 * 0x8(r1)", "stfd f31, 0x28(r1)", "psq_st f31, 0x28(r1), 0, qr0", "bl _savegpr_25"): GPR and FPR masks. A register
 * the code sets from r1 ("mr r31, r1": the frame pointer) is left out: the allocator does not assign it. */
void mwt_ra_saved_regs(const AvpStr *rows, size_t n, u32 *gpr, u32 *fpr);
/* The two self-checks: (a) every register variable of the trace against the DWARF (have_dwarf false: not run, the
 * variables listed as uncovered); (b) the nonvolatile GPR and FPR sets against the saved masks (have_saved false:
 * not run). Gaps go to c->gaps. */
void mwt_ra_check(AvpArena *a, const MwtRaFunc *f, const MwtDwVars *dw, bool have_dwarf, u32 saved_gpr, u32 saved_fpr,
                  bool have_saved, MwtRaCheck *c);

/* ---- comparison of two traces (--against) ---- */
/* Plain-fact lines: per class, the first position where simplify's or select's order differs (with both nodes'
 * numbers), the nodes only one side has, and every named variable whose register differs. */
AvpStrVec mwt_ra_diff(AvpArena *a, const MwtRaFunc *A, const MwtRaFunc *B);

/* ---- one traced compile (the subcommand's work, reusable by other tools) ---- */
typedef struct {
    MwtJob job;
    MwtRaTrace trace;
    const MwtRaFunc *fn;
    MwtRaCheck check;    /* check.gaps empty: the trace equals the object */
    double secs;         /* the -sym on compile under the debugger */
    double total_secs;   /* everything the run did */
    const char *error;   /* the run could not be made (exit 2) */
} MwtRaRun;
/* Compiles the unit (or src) with -sym on added under the debugger with MWT_REGALLOC_SPEC for one function, parses
 * the log and runs the self-checks: the DWARF and prologue checks always; with code_check also a second compile
 * without -sym on whose code must equal it. false with r->error when the run could not be made. */
bool mwt_ra_run(AvpArena *a, const char *unit, const char *src, const AvpStrVec *adds, const AvpStrVec *drops,
                const char *outdir, const char *sym, bool code_check, MwtRaRun *r);

/* Every function of one compile at once: the debugger's cost is the whole unit's whatever the spec logs, so a tool
 * that needs several functions of one text traces them in one compile (the spec's `only` empty), then checks each. */
typedef struct {
    MwtJob job;
    MwtRaTrace trace;    /* every function the compile allocated registers for */
    double secs, total_secs;
    AvpMap *om;          /* the -sym on object's functions (cc_load_object), NULL when unreadable */
    CcDwFns fns;         /* its DWARF dump, parsed */
    bool have_fns;
} MwtRaUnit;
/* The -sym on compile of the unit (or src) with the spec logging every function, its object and DWARF dump. false
 * with *why. */
bool mwt_ra_unit_run(AvpArena *a, const char *unit, const char *src, const AvpStrVec *adds, const AvpStrVec *drops,
                     const char *outdir, MwtRaUnit *u, const char **why);
/* One function of a unit trace with its self-checks (DWARF, prologue, completeness; no second compile). false with
 * r->error when the trace has no such function. */
bool mwt_ra_unit_fn(AvpArena *a, const MwtRaUnit *u, const char *sym, MwtRaRun *r);

/* ---- reports ---- */
AvpStr mwt_ra_text(AvpArena *a, const MwtRaTrace *t, const MwtRaFunc *f, const MwtRaCheck *c);
AvpJson *mwt_ra_json(AvpArena *a, const MwtRaTrace *t, const MwtRaFunc *f, const MwtRaCheck *c);

/* The subcommand: mwtrace.exe regalloc <unit> --function SYM [options]. */
int mwt_regalloc_main(AvpArena *a, int argc, char **argv);
