/* eq_side: the instruction model of tools/eqcheck.py (L533-1399): the listing tables (_REG, _MEM, _CRBIT, _CC,
 * _BITS, LOADS, STORES, FBIN, FMA, FUN), _int, callee_arity, State, BlockRun and Side (the symbolic execution of one
 * basic block of one function); plus the exception discipline every eqcheck module shares (Unsupported, NotProven
 * and Verdict, L81-87 and L1607-1610) and Prover.entry_state (L1626-1631), which the tests need too.
 *
 * ---- Exceptions ------------------------------------------------------------------------------------------------
 * The Python raises Unsupported, NotProven and Verdict through the engine and catches them at a few places. Here
 * they are thrown with avp_throw (avp/base.h) through eq_raise / eq_raise_verdict, and caught with AVP_TRY:
 *     AvpTry tr;
 *     if (AVP_TRY(&tr)) { ... eq_side_run_block(...) ...; avp_try_end(&tr); }
 *     else { const EqExc *e = eq_exc_caught(&tr); if (e->code != EQ_EXC_NOTPROVEN) eq_reraise(e); ... }
 * eq_exc_caught gives the full exception (avp_throw's own message buffer is cut at 1023 bytes; the record keeps the
 * whole text, the Python's str(e)). The record is per thread and overwritten by the next raise: keep one with
 * eq_exc_save. Exceptions the Python leaves to Python itself (KeyError on a register name the state lacks, IndexError
 * on a missing operand, ValueError from int() or tuple unpacking, TypeError) are thrown with the same mechanism and
 * the SHSM_EXC_KEY / SHSM_EXC_VALUE / SHSM_EXC_TYPE codes of shsm_cli.h, or EQ_EXC_INDEX; the Python's prover lets
 * them escape (scan reports "<Class>: <str(e)>" for the unit), so callers should too (DEVIATIONS D-eqcheck-2).
 * Throws do not unwind: everything between AVP_TRY and the throw is allocated in arenas (states, runs, nodes).
 *
 * ---- State ------------------------------------------------------------------------------------------------------
 * The Python's State: `r` a dict {location: term}, `slots` a dict {(r1 offset, width): term}, mem, stores (list),
 * frame (term or None), ncall, dead (a set of (offset, width)). Here:
 *   r[EQ_NREG_LOCS]  the REG_LOCS keys, by REG_LOCS index (eq_loc_index); NULL = key absent (an entry state has all
 *                    99). The dict is created in REG_LOCS order, so its iteration order is this array's order.
 *   xr               any other key the instructions wrote (a register operand outside REG_LOCS, e.g. "cr32" from a
 *                    compare into cr8), in dict insertion order; the Prover reads REG_LOCS only.
 *   slots            in dict insertion order; assignment to an existing key keeps its position, pop removes (the
 *                    Python's dict semantics; eq_state_slot_set / eq_state_slot_pop).
 *   dead             a set: no meaningful order (the Python sorts it where order reaches output); no duplicates.
 * eq_state_copy is State.copy (L598-602): a deep copy of the arrays (the terms are shared, as in the Python).
 * Every State lives in the arena it was made or copied in.
 *
 * ---- Side ---------------------------------------------------------------------------------------------------------
 * Side(name, fn, resolve, terms, live) (L616-625). The relocation resolver is a callback row -> address term, NULL
 * for the Python's None (an unresolved relocation; addr_of then raises NotProven). Node allocation order is the
 * Python's everywhere (see eq_terms.h): the resolver is called exactly where the Python calls resolve(row).
 * Side.unresolved (never appended to) and BlockRun.notes (never appended to) are not ported.
 * Memory: a Side allocates its graph, operand cache, states, runs and effect texts in the Terms' arena (t->a), which
 * must not be the thread's scratch arena (eq_side uses avp_scratch() under marks for temporaries). One Side is used
 * by one thread. Sides that share one EqTerms (the Prover's retail and compiled sides) share its node ids.
 *
 * Effects (BlockRun.effects): the Python's tuples ("io", node, text), ("st", node, None) and
 * ("call", node, desc, declared): kind, n, text (NULL for "st"; the row text for a call) and, for a call, the set of
 * declared registers as a mask over EQ_ARG_REGS (eq_live.h: every declared register is r3..r10 or f1..f8). */
#pragma once
#include "avp/avp.h"
#include "cc_listing.h"
#include "eq_live.h"
#include "eq_terms.h"
#include "wall_cfg.h"
#include "wall_rows.h"

/* ---- exceptions (L81-87, L1607-1610) ------------------------------------------------------------------------------ */
enum {
    EQ_EXC_UNSUPPORTED = 128, /* Unsupported: an instruction or construct the model does not cover */
    EQ_EXC_NOTPROVEN,         /* NotProven */
    EQ_EXC_VERDICT,           /* Verdict(kind, reason, block=None, terms=None) */
    EQ_EXC_INDEX,             /* IndexError (a missing operand) */
};
typedef struct {
    int code;        /* EQ_EXC_* or an SHSM_EXC_* / AVP_ERR_* code */
    const char *msg; /* args[0]: the reason (str(e) except for a KeyError, whose str is the repr of msg) */
    /* Verdict only: */
    const char *vkind; /* "DIFFERENT" / "NOT PROVEN" (a static string) */
    int block;         /* -1 = None */
    bool has_terms;    /* terms is not None */
    EqArg terms[2];
} EqExc;
/* raise <class>(message): code EQ_EXC_UNSUPPORTED, EQ_EXC_NOTPROVEN, EQ_EXC_INDEX or SHSM_EXC_KEY/VALUE/TYPE. */
AVP_NORETURN void eq_raise(int code, AVP_PRINTF_FMT const char *fmt, ...);
/* raise Verdict(kind, reason, block, terms): block -1 for None, terms NULL for None (else two arguments). */
AVP_NORETURN void eq_raise_verdict(const char *kind, int block, const EqArg *terms, AVP_PRINTF_FMT const char *fmt,
                                   ...);
/* raise e (a caught exception again, unchanged). */
AVP_NORETURN void eq_reraise(const EqExc *e);
/* The exception an AVP_TRY caught: the record eq_raise filled (full text), or, for a throw from elsewhere (avp_throw /
 * shsm_raise), one made from tr->code and tr->msg. Valid until the thread's next raise. */
const EqExc *eq_exc_caught(const AvpTry *tr);
/* A copy of e in arena a (a Python `first = e` kept across later raises). */
EqExc *eq_exc_save(AvpArena *a, const EqExc *e);
/* type(e).__name__: "Unsupported", "NotProven", "Verdict", "IndexError", or shsm_exc_class(code). */
const char *eq_exc_class(int code);
/* str(e): the repr of msg for a KeyError, else msg. In arena a. */
const char *eq_exc_str(AvpArena *a, const EqExc *e);

/* ---- small helpers ------------------------------------------------------------------------------------------------ */
/* REG_LOCS.index(name), or -1 (exact spelling: "r3", not "r03"). */
int eq_loc_index(AvpStr name);
/* _int(tok) (L556-557): int(tok, 0). Raises ValueError "invalid literal for int() with base 0: <repr>". Values
 * beyond 64 bits wrap (DEVIATIONS D-eqcheck-3). */
i64 eq_py_int0(AvpStr tok);
/* int(tok) (base 10), as exec uses for "cr7"[2:] and "r29"[1:]. Raises ValueError as int(). */
i64 eq_py_int10(AvpStr tok);
/* repr(s) of a str (Python's quoting and escapes, ASCII). In arena a. */
const char *eq_py_repr(AvpArena *a, AvpStr s);

/* ---- callee_arity (L560-589) ----------------------------------------------------------------------------------------- */
typedef enum {
    EQ_ARITY_UNKNOWN = 0, /* None: every argument register */
    EQ_ARITY_STACK,       /* "stack": arguments passed on the stack */
    EQ_ARITY_REGS,        /* (ng, nf) */
} EqArityKind;
typedef struct {
    int kind; /* EqArityKind */
    int ng, nf;
} EqArity;
/* callee_arity(name): name NULL or "" is None; a name mangle.parse rejects (or raises on) is None. */
EqArity eq_callee_arity(const char *name);

/* Side.arity_map: {target node id: arity} (the Prover's share_arities fills it; a value may be None). */
typedef struct EqArityMap {
    AvpArena *a;
    int *ids;
    EqArity *vals;
    size_t n, cap;
} EqArityMap;
EqArityMap *eq_arity_map_new(AvpArena *a);
/* amap.get(id): false when id is not a key. */
bool eq_arity_map_get(const EqArityMap *m, int id, EqArity *out);
/* amap[id] = v (an existing key keeps its position). */
void eq_arity_map_set(EqArityMap *m, int id, EqArity v);

/* ---- State (L592-602) ------------------------------------------------------------------------------------------------ */
typedef struct {
    i64 off, w;
} EqSlotKey;
typedef struct {
    i64 off, w;
    EqNode *v;
} EqSlot;
typedef struct {
    const char *name;
    EqNode *v;
} EqXReg;
typedef struct EqState {
    EqNode *r[EQ_NREG_LOCS];
    EqXReg *xr;
    size_t nxr, xrcap;
    EqSlot *slots;
    size_t nslots, slotcap;
    EqNode *mem;
    EqNode **stores;
    size_t nstores, storecap;
    EqNode *frame; /* NULL = None */
    i64 ncall;
    EqSlotKey *dead;
    size_t ndead, deadcap;
} EqState;
/* State(): every field empty (no register keys, mem NULL). */
EqState *eq_state_new(AvpArena *a);
/* State.copy(). */
EqState *eq_state_copy(AvpArena *a, const EqState *s);
/* Prover.entry_state() (L1626-1631): r = {loc: t.entry(loc) for loc in REG_LOCS} (entries made in REG_LOCS order),
 * then mem = leaf("E", "mem"); no slots, stores or dead words, frame None, ncall 0. In t's arena. */
EqState *eq_state_entry(EqTerms *t);
/* st.r.get(name) (NULL when absent) and st.r[name] = v. */
EqNode *eq_state_reg(const EqState *s, AvpStr name);
void eq_state_set_reg(AvpArena *a, EqState *s, AvpStr name, EqNode *v);
/* st.r[name] (raises KeyError when absent). */
EqNode *eq_state_reg_req(const EqState *s, AvpStr name);
/* slots.get((off, w)) (NULL when absent), slots[(off, w)] = v, slots.pop((off, w)) (NULL when absent). */
EqNode *eq_state_slot_get(const EqState *s, i64 off, i64 w);
void eq_state_slot_set(AvpArena *a, EqState *s, i64 off, i64 w, EqNode *v);
EqNode *eq_state_slot_pop(EqState *s, i64 off, i64 w);
/* stores.append(n). */
void eq_state_store_push(AvpArena *a, EqState *s, EqNode *n);
/* dead.add((off, w)) and (off, w) in dead. */
void eq_state_dead_add(AvpArena *a, EqState *s, i64 off, i64 w);
bool eq_state_dead_has(const EqState *s, i64 off, i64 w);

/* ---- BlockRun (L605-610) --------------------------------------------------------------------------------------------- */
typedef enum { EQ_EFF_IO, EQ_EFF_ST, EQ_EFF_CALL } EqEffKind;
typedef struct {
    int kind;         /* EqEffKind */
    EqNode *n;        /* the ioload / st / call node */
    const char *text; /* io: "load 4 from ..." / "store 4 to ..."; call: the row text (desc); st: NULL */
    u32 declared;     /* call: the declared registers (mask over EQ_ARG_REGS) */
} EqEffect;
typedef struct {
    EqState *state;
    EqEffect *effects;
    size_t neffects, effcap;
    EqNode *cond;      /* NULL = None */
    EqNode *exit_tail; /* exit = ("tail", call node); NULL = None */
    WallKind kind;     /* the block's kind */
} EqBlockRun;

/* ---- Side (L613-1399) ------------------------------------------------------------------------------------------------ */
/* resolve(row): the address term of the row's relocation target, NULL for None. May allocate nodes in t. */
typedef EqNode *(*EqResolveFn)(void *ctx, EqTerms *t, const CcRow *row);

/* An immediate operand (imm, L647-661): ('n', value) or a relocation half (kind, address term). */
typedef struct {
    bool is_n;
    i64 n;             /* is_n */
    const char *kind;  /* else: "ha", "h", "l", "sda21" or any other text after the '@' */
    EqNode *a;
} EqImm;

/* Side.split(addr) (L707-716): (None, k) for a constant, (args[1:], k) for a lin, ((addr, 1), 0) otherwise. */
typedef struct {
    bool none;          /* base is None */
    int n;              /* the base tuple's length */
    const EqArg *items; /* the base tuple (points into the lin node's args, or at pair) */
    EqArg pair[2];      /* (addr, 1) storage: use eq_split_item, not items, after copying the struct */
    bool is_pair;
    i64 off;
} EqSplit;
EqSplit eq_split(EqNode *addr);
EqArg eq_split_item(const EqSplit *s, int k);
/* b1 == b2 of two split bases (tuples compared element by element, None equal only to None). */
bool eq_split_base_eq(const EqSplit *a, const EqSplit *b);
/* is_io(addr) (L728-730): a constant >= 0xC0000000. */
bool eq_is_io(const EqNode *addr);

typedef struct EqSide {
    const char *name; /* "retail" / "compiled" (the unknowns' side tag) */
    const CcFn *fn;
    const CcRow *rows;
    size_t nrows;
    EqResolveFn resolve;
    void *resolve_ctx;
    EqTerms *t;
    EqLive *live;          /* NULL = None */
    EqArityMap *arity_map; /* NULL = None */
    WallCfg *cfg;
    bool escaped;
    bool wants_escape;
    /* private */
    AvpArena *a; /* t->a */
    const char ***ops;
    size_t *nops;
    i64 ucount;
} EqSide;

/* Side(name, fn, resolve, terms, live) (L616-625): builds the graph (wall.Cfg); arity_map None, flags false. */
EqSide *eq_side_new(const char *name, const CcFn *fn, EqResolveFn resolve, void *resolve_ctx, EqTerms *t, EqLive *live);
/* fresh(tag) (L627-629): unknown(name, "<tag>#<n>") with a per-side counter. */
EqNode *eq_side_fresh(EqSide *s, const char *tag);
/* operands(i) (L632-639): the row's operands (text after the mnemonic split at ",", stripped), cached. */
const char **eq_side_operands(EqSide *s, size_t i, size_t *n);
/* addr_of(i) (L641-645): resolve(row) or NotProven "unresolved relocation at <text>". */
EqNode *eq_side_addr_of(EqSide *s, size_t i);
/* imm, imm_node, hi_node, ea (L647-694). ea returns the address and its base register name (*base). */
EqImm eq_side_imm(EqSide *s, size_t i, AvpStr tok, bool is_signed);
EqNode *eq_side_imm_node(EqSide *s, EqImm im, bool is_signed);
EqNode *eq_side_hi_node(EqSide *s, EqImm im);
EqNode *eq_side_ea(EqSide *s, EqState *st, size_t i, AvpStr tok, const char **base);
/* stack_off(addr) (L697-705): false for None. */
bool eq_side_stack_off(const EqSide *s, const EqNode *addr, i64 *off);
/* may_alias(a1, w1, a2, w2) (L718-726). */
bool eq_side_may_alias(const EqSide *s, EqNode *a1, i64 w1, EqNode *a2, i64 w2);
/* ext(v, srcw, k, w), cat(pieces) (L732-759). */
EqNode *eq_side_ext(EqSide *s, EqNode *v, i64 srcw, i64 k, i64 w);
typedef struct {
    EqNode *v;
    i64 w;
} EqPiece;
EqNode *eq_side_cat(EqSide *s, const EqPiece *pieces, size_t n);
/* read_slot, default_slot, write_slot (L761-805). read_slot raises NotProven. */
EqNode *eq_side_read_slot(EqSide *s, EqState *st, i64 off, i64 w);
EqNode *eq_side_default_slot(EqSide *s, EqState *st, i64 off, i64 w);
void eq_side_write_slot(EqSide *s, EqState *st, i64 off, i64 w, EqNode *val);
/* load, store (L807-843); run receives the io / st effects. */
EqNode *eq_side_load(EqSide *s, EqState *st, EqBlockRun *run, EqNode *addr, i64 w);
void eq_side_store(EqSide *s, EqState *st, EqBlockRun *run, EqNode *addr, i64 w, EqNode *val, bool backchain);
/* mem_now(st) (L845-848): mem, or node("mem", [mem] + stores sorted by id). */
EqNode *eq_side_mem_now(EqSide *s, const EqState *st);
/* set_cmp, set_cr0, crbit (L851-867). crbit raises Unsupported "condition bit <repr>". */
void eq_side_set_cmp(EqSide *s, EqState *st, i64 f, EqNode *lt, EqNode *gt, EqNode *eq, EqNode *so);
void eq_side_set_cr0(EqSide *s, EqState *st, EqNode *v);
i64 eq_side_crbit(AvpArena *a, AvpStr tok);
/* frame_image, is_save, sp_off (L870-888). */
EqNode *eq_side_frame_image(EqSide *s, const EqState *st);
bool eq_side_is_save(const EqNode *v);
i64 eq_side_sp_off(EqSide *s, const EqState *st);
/* call(st, run, target, name, desc, tail) (L890-939): the call node. name NULL = None. */
EqNode *eq_side_call(EqSide *s, EqState *st, EqBlockRun *run, EqNode *target, const char *name, const char *desc,
                     bool tail);
/* helper(st, run, kind, regs, n) (L941-953): kind "save"/"rest", regs "gpr"/"fpr". */
void eq_side_helper(EqSide *s, EqState *st, EqBlockRun *run, AvpStr kind, AvpStr regs, i64 n);
/* run_block(bid, state) (L956-983): executes block bid on a copy of state (state itself is not changed). The run
 * and its state are allocated in the side's arena. Raises Unsupported / NotProven (and the Python-level errors). */
EqBlockRun *eq_side_run_block(EqSide *s, int bid, const EqState *state);
/* target(i), target_name(i) (L985-993): leaf("self") for a self call, else addr_of(i); the row's symbol or NULL. */
EqNode *eq_side_target(EqSide *s, size_t i);
const char *eq_side_target_name(const EqSide *s, size_t i);
/* branch_cond, exec_call, exec, memop, psq (L995-1398), on row i. */
EqNode *eq_side_branch_cond(EqSide *s, EqState *st, size_t i);
void eq_side_exec_call(EqSide *s, EqState *st, EqBlockRun *run, size_t i);
void eq_side_exec(EqSide *s, EqState *st, EqBlockRun *run, size_t i);
