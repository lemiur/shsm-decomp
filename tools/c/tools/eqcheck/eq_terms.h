/* eq_terms: the term language of tools/eqcheck.py (L63-530): the constants (MASK, ASSUMPTIONS, SINGLE_OPS, SP_PROP,
 * CALLEE_SAVED, REG_LOCS, VOLATILE), _wmask / _s16 / _s32, Node, Terms (hash-consing table and simplifier) and
 * show(); plus mask_of (L550-553), which the Side and the tests use.
 *
 * Nodes. A Node is (op, args) with derived flags; Terms hash-conses them, so two terms are equal exactly when they
 * are the same pointer (the Python's `is`). Node ids are allocation order in the Terms (len(tab) at creation): T is
 * 0, F 1, sp0 2, then every new node or leaf in the order the Python creates it. The simplifier's canonical operand
 * order (sorted by id) and show()'s text depend on that order, so every function here creates nodes in exactly the
 * Python's order (including nodes the Python builds and then discards, e.g. lin's "los" probe, eq's negation,
 * band's bnot probes). CALLERS MUST DO THE SAME: Python evaluates the constructor calls of one expression left to
 * right (`t.add(t.leaf("sym", n), t.c(off))` makes the leaf, then the constant), C leaves argument order
 * unspecified, so a ported expression with two or more allocating calls is sequenced through locals.
 *
 * Arguments (EqArg) are tagged: a node, an int (Python int; i64 here, every value the engine stores fits), a str
 * (NUL-terminated; copied into the Terms' arena when a new node is made) or a bool. The hash-consing key follows
 * the Python's `(op,) + tuple(a.id if Node else ("=", a))`: a node argument by identity, an int and a bool by
 * numeric value (True == 1 in a Python dict key, so leaf(op, True) and leaf(op, 1) are the same node: the first
 * one made keeps its tag), a str by content. node() and leaf() share the one table (as in the Python).
 *
 * Ops are small ints (EqOp): the known names below have fixed ids, any other name (an instruction mnemonic passed
 * to eq_op, "ret"/"clob", ...) is interned process-wide by eq_op_id (thread-safe; ids are stable for the process).
 * n->opname is the name. Compare ops by id: `n->op == EQ_OP_C`, or `n->op == eq_op_id("psq1")`.
 *
 * Integer arithmetic: values are 32-bit words (u32 in nodes' "c" args and lin coefficients, stored as i64 >= 0).
 * The Python reduces with `& MASK`; every entry point here takes i64 and reduces the same way (two's complement,
 * so a negative i64 gives the Python's `v & MASK`). Shift counts follow the Python (`&= 31` / `&= 63` where it
 * masks). nz is "may be nonzero" bits (<= MASK).
 *
 * Errors: none. The functions here never raise (the Python's Terms raises nothing either); the two preconditions
 * the Python leaves to Python's own errors (sra n >= 0, sext bits in 1..62) are asserted (DEVIATIONS D-eqcheck-1).
 *
 * Memory: an EqTerms allocates its nodes, argument arrays and strings in the arena given to eq_terms_new (they
 * live as long as that arena); the hash table is heap memory freed by eq_terms_free. One EqTerms is used by one
 * thread (the Python's one Terms per proof). show() allocates its text in the arena passed to it. */
#pragma once
#include "avp/avp.h"

#define EQ_MASK 0xFFFFFFFFu

/* ASSUMPTIONS (L66-80): the six assumption sentences of an EQUIVALENT (proved) verdict, in order. */
#define EQ_NASSUMPTIONS 6
extern const char *const EQ_ASSUMPTIONS[EQ_NASSUMPTIONS];

/* CALLEE_SAVED (L104-105): r1 r2 r13 r14..r31 f14..f31 cr8..cr19 lr (52 names, the Python's list order). */
#define EQ_NCALLEE_SAVED 52
extern const char *const EQ_CALLEE_SAVED[EQ_NCALLEE_SAVED];
/* REG_LOCS (L106-107): r0..r31 f0..f31 cr0..cr31 ca ctr lr (99 names). */
#define EQ_NREG_LOCS 99
extern const char *const EQ_REG_LOCS[EQ_NREG_LOCS];
/* VOLATILE (L108-109): r0 r5..r12 f0 f2..f13 cr0..cr7 cr20..cr31 ca ctr lr (45 names). */
#define EQ_NVOLATILE 45
extern const char *const EQ_VOLATILE[EQ_NVOLATILE];

/* _wmask(w) (L112-113): MASK for w >= 4, else the low 8*w bits. */
static inline u32 eq_wmask(i64 w) { return w >= 4 ? EQ_MASK : (u32)((1ull << (8 * w)) - 1); }
/* _s16(v), _s32(v) (L116-123): the low 16 / 32 bits as a signed value. */
static inline i64 eq_s16(i64 v) {
    v &= 0xFFFF;
    return (v & 0x8000) ? v - 0x10000 : v;
}
static inline i64 eq_s32(i64 v) {
    v &= 0xFFFFFFFFll;
    return (v & 0x80000000ll) ? v - 0x100000000ll : v;
}
/* mask_of(mb, me) (L550-553): the rlwinm mask from bit mb to bit me (0 = MSB), wrapping when mb > me. */
u32 eq_mask_of(i64 mb, i64 me);

/* ---- ops ---------------------------------------------------------------------------------------------------- */
typedef enum {
    EQ_OP_C, EQ_OP_E, EQ_OP_BOOL, EQ_OP_SYM, EQ_OP_U, EQ_OP_BV, EQ_OP_CSYM, EQ_OP_SELF, EQ_OP_NOCR6,
    EQ_OP_LIN, EQ_OP_MUL, EQ_OP_AND, EQ_OP_OR, EQ_OP_XOR, EQ_OP_SHR, EQ_OP_SRA, EQ_OP_ROTL, EQ_OP_SEXT, EQ_OP_CRW,
    EQ_OP_B2I, EQ_OP_BITOF, EQ_OP_BNOT, EQ_OP_BAND, EQ_OP_BOR, EQ_OP_BXOR, EQ_OP_LTS, EQ_OP_LTU, EQ_OP_EQZ,
    EQ_OP_LOS, EQ_OP_LOU, EQ_OP_HA, EQ_OP_HI, EQ_OP_LD, EQ_OP_MV, EQ_OP_MEM, EQ_OP_FIMG, EQ_OP_ARGS, EQ_OP_EXT,
    EQ_OP_CAT, EQ_OP_FA, EQ_OP_IOLOAD, EQ_OP_ST, EQ_OP_CALL, EQ_OP_RET, EQ_OP_CLOB, EQ_OP_MEMAFTER, EQ_OP_IND,
    EQ_OP_CADD, EQ_OP_CADDE, EQ_OP_CASRA, EQ_OP_CASRAW, EQ_OP_FLT, EQ_OP_FEQ, EQ_OP_FUN, EQ_OP_FSEL,
    EQ_OP_S2D, EQ_OP_D2S, EQ_OP_FRSP, EQ_OP_FADDS, EQ_OP_FSUBS, EQ_OP_FMULS, EQ_OP_FDIVS, EQ_OP_FMADDS,
    EQ_OP_FMSUBS, EQ_OP_FNMADDS, EQ_OP_FNMSUBS, EQ_OP_FRES, EQ_OP_PSQ, EQ_OP_PSQ1, EQ_OP_PSQB, EQ_OP_PSQB1,
    EQ_OP_FNEG, EQ_OP_FABS, EQ_OP_CNTLZW, EQ_OP_ROTLV,
    EQ_OP__KNOWN /* first id given to an interned name */
} EqOp;
/* The op id of a name (interned on first use; the known names give their enum value). */
int eq_op_id(const char *name);
/* The name of an op id. */
const char *eq_op_name(int op);
/* SINGLE_OPS (L100-101): s2d frsp fadds fsubs fmuls fdivs fmadds fmsubs fnmadds fnmsubs fres psq1. */
bool eq_is_single_op(int op);
/* SP_PROP (L102): lin and or xor shr sra rotl mul (ops whose node inherits a child's sp flag). */
bool eq_is_sp_prop(int op);

/* ---- nodes ------------------------------------------------------------------------------------------------- */
typedef struct EqNode EqNode;
typedef enum { EQ_ARG_NODE, EQ_ARG_INT, EQ_ARG_STR, EQ_ARG_BOOL } EqArgKind;
typedef struct {
    int kind;      /* EqArgKind */
    i64 i;         /* EQ_ARG_INT value; EQ_ARG_BOOL 0 or 1 */
    EqNode *n;     /* EQ_ARG_NODE */
    const char *s; /* EQ_ARG_STR */
} EqArg;
static inline EqArg eq_an(EqNode *n) { return (EqArg){EQ_ARG_NODE, 0, n, NULL}; }
static inline EqArg eq_ai(i64 i) { return (EqArg){EQ_ARG_INT, i, NULL, NULL}; }
static inline EqArg eq_as(const char *s) { return (EqArg){EQ_ARG_STR, 0, NULL, s}; }
static inline EqArg eq_ab(bool b) { return (EqArg){EQ_ARG_BOOL, b ? 1 : 0, NULL, NULL}; }

/* Node (L90-95 and Terms.node/leaf). args[0..nargs) as made; flags as the Python computes them:
 * node(): glob = every node argument glob (true without node arguments), local = any node argument local,
 * sp = op in SP_PROP and any node argument sp; leaf(): the flags given. */
struct EqNode {
    int op;
    const char *opname;
    int nargs;
    EqArg *args;
    int id;
    u32 nz;
    bool glob, local, sp;
    u64 hash; /* of the hash-consing key */
};

/* diff_path's per-argument test (L1610-1630): `x is not y` for a node x, `x != y` otherwise. So nodes compare by
 * pointer, ints and bools by value (True == 1), strs by content, and a node never equals a non-node. */
bool eq_arg_differs(EqArg x, EqArg y);

typedef struct EqTerms EqTerms;
struct EqTerms {
    AvpArena *a;
    EqNode *T, *F, *sp0; /* leaf("bool", True), leaf("bool", False), leaf("E", "r1", sp=True) */
    /* private */
    EqNode **slots;
    size_t cap, len;
};

/* Terms() (L126-132): an empty table with T, F and sp0 made (ids 0, 1, 2). */
EqTerms *eq_terms_new(AvpArena *a);
void eq_terms_free(EqTerms *t);
/* len(tab): the number of nodes made (the next node's id). */
static inline size_t eq_terms_len(const EqTerms *t) { return t->len; }

/* node(op, args, nz=MASK) (L135-147). */
EqNode *eq_node(EqTerms *t, int op, const EqArg *args, int nargs, u32 nz);
/* node() with node arguments only. */
EqNode *eq_node_n(EqTerms *t, int op, EqNode *const *kids, int nkids, u32 nz);
static inline EqNode *eq_node1(EqTerms *t, int op, EqNode *x, u32 nz) { return eq_node_n(t, op, &x, 1, nz); }
/* leaf(op, *args, glob=True, local=False, sp=False, nz=MASK) (L149-158). */
EqNode *eq_leaf(EqTerms *t, int op, const EqArg *args, int nargs, bool glob, bool local, bool sp, u32 nz);
/* leaf(op, *args) with the default flags. */
static inline EqNode *eq_leaf_d(EqTerms *t, int op, const EqArg *args, int nargs) {
    return eq_leaf(t, op, args, nargs, true, false, false, EQ_MASK);
}
/* c(v) (L160-162): the constant v & MASK (nz = the value). */
EqNode *eq_c(EqTerms *t, i64 v);
/* entry(loc) (L164-168): r1 is sp0; else leaf("E", loc), nz 1 for "ca" and "cr*". */
EqNode *eq_entry(EqTerms *t, const char *loc);
/* unknown(side, tag) (L170-171): leaf("u", side, tag, glob=False, local=True). */
EqNode *eq_unknown(EqTerms *t, const char *side, const char *tag);
/* sym(name, off=0) (L173-174): add(leaf("sym", name), c(off)). */
EqNode *eq_sym(EqTerms *t, const char *name, i64 off);

/* ---- integer arithmetic (L177-394) ---------------------------------------------------------------------------- */
typedef struct {
    EqNode *t;
    i64 cf;
} EqTerm;
/* lin(k, terms) (L177-223): k + sum(cf * t), flattened, constants folded, relocation halves folded, as one "lin"
 * node (args k, t1, v1, t2, v2 ... in id order), a constant, or a lone term. */
EqNode *eq_lin(EqTerms *t, i64 k, const EqTerm *terms, size_t n);
/* add(*xs), sub(a, b), mulc(a, c), mul(a, b) (L225-238). */
EqNode *eq_addv(EqTerms *t, EqNode *const *xs, size_t n);
EqNode *eq_add(EqTerms *t, EqNode *a, EqNode *b);
EqNode *eq_add3(EqTerms *t, EqNode *a, EqNode *b, EqNode *c);
EqNode *eq_sub(EqTerms *t, EqNode *a, EqNode *b);
EqNode *eq_mulc(EqTerms *t, EqNode *a, i64 c);
EqNode *eq_mul(EqTerms *t, EqNode *a, EqNode *b);
/* and_(*xs), and_mask(x, m) (L240-273). */
EqNode *eq_andv(EqTerms *t, EqNode *const *xs, size_t n);
EqNode *eq_and(EqTerms *t, EqNode *a, EqNode *b);
EqNode *eq_and_mask(EqTerms *t, EqNode *x, i64 m);
/* or_(*xs), xor_(*xs), not_(x) (L275-322). */
EqNode *eq_orv(EqTerms *t, EqNode *const *xs, size_t n);
EqNode *eq_or(EqTerms *t, EqNode *a, EqNode *b);
EqNode *eq_xorv(EqTerms *t, EqNode *const *xs, size_t n);
EqNode *eq_xor(EqTerms *t, EqNode *a, EqNode *b);
EqNode *eq_not(EqTerms *t, EqNode *x);
/* shl(x, n), shr(x, n), sra(x, n) (L324-352). */
EqNode *eq_shl(EqTerms *t, EqNode *x, i64 n);
EqNode *eq_shr(EqTerms *t, EqNode *x, i64 n);
EqNode *eq_sra(EqTerms *t, EqNode *x, i64 n);
/* sext(x, bits) (L354-365). */
EqNode *eq_sext(EqTerms *t, EqNode *x, i64 bits);
/* rot_mask(x, s, m) (L367-385): rlwinm, rotl(x, s) & m. */
EqNode *eq_rot_mask(EqTerms *t, EqNode *x, i64 s, i64 m);
/* op(name, *args, comm=False, nz=MASK) (L387-390): node(name, args), args sorted by id when comm. */
EqNode *eq_op(EqTerms *t, int op, EqNode *const *args, int nargs, bool comm, u32 nz);

/* ---- booleans (L393-489) ----------------------------------------------------------------------------------- */
EqNode *eq_b2i(EqTerms *t, EqNode *b);
/* bit_of(x, i): CR bit i (0 = MSB) of a word moved into the CR. */
EqNode *eq_bit_of(EqTerms *t, EqNode *x, int i);
EqNode *eq_bnot(EqTerms *t, EqNode *b);
EqNode *eq_bandv(EqTerms *t, EqNode *const *xs, size_t n);
EqNode *eq_band(EqTerms *t, EqNode *a, EqNode *b);
EqNode *eq_borv(EqTerms *t, EqNode *const *xs, size_t n);
EqNode *eq_bor(EqTerms *t, EqNode *a, EqNode *b);
EqNode *eq_bxor(EqTerms *t, EqNode *a, EqNode *b);
/* lt(signed, a, b), eq(a, b) (L472-489). */
EqNode *eq_lt(EqTerms *t, bool is_signed, EqNode *a, EqNode *b);
EqNode *eq_eq(EqTerms *t, EqNode *a, EqNode *b);

/* ---- show (L492-528) --------------------------------------------------------------------------------------- */
#define EQ_SHOW_DEPTH 6
/* show(n, depth=6): readable text of a term, abbreviated below depth. Allocated in a. */
char *eq_show(AvpArena *a, const EqNode *n, int depth);
/* show(x) of an argument value (a node: as eq_show; an int: hex when abs > 9; a bool "True"/"False"; a str
 * itself). */
char *eq_show_arg(AvpArena *a, EqArg x, int depth);
/* Python's "%s" % x of an argument (str(x): an int in decimal, a node as eq_show at the default depth). */
char *eq_str_arg(AvpArena *a, EqArg x);
