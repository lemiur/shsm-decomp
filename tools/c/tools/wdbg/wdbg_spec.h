/* The wdbg spec language (SPEC.md): a parser and interpreter for the breakpoint specs of wdbg.exe, the C port of
 * tools/mwcc_trace/wdbg.py. A Python spec is a Python file whose handlers run arbitrary Python; wdbg.exe runs this
 * small language instead (D-wdbg-1). The interpreter knows nothing about the debugger: memory, the log, snapshots
 * and compiler symbol names go through a WdbgHost, so the tests drive it with a fake. */
#pragma once
#include "avp/avp.h"

/* The registers a handler sees, the fields of the Python's WCTX structure (a WOW64_CONTEXT), in its order. */
enum { WDBG_NREGS = 23 };
extern const char *const wdbg_reg_names[WDBG_NREGS];

typedef enum { WDBG_INT, WDBG_STR, WDBG_BYTES } WdbgKind;
typedef struct {
    WdbgKind k;
    i64 i;   /* WDBG_INT */
    AvpStr s; /* WDBG_STR (UTF-8) and WDBG_BYTES */
} WdbgVal;

/* What a spec reaches outside itself. Every callback is required. */
typedef struct WdbgHost {
    void *ctx;
    /* ReadProcessMemory: the bytes read (0 when the address is unreadable). */
    size_t (*read)(void *ctx, u32 addr, void *buf, size_t n);
    /* WriteProcessMemory and FlushInstructionCache; failures are ignored, as the Python's. */
    void (*write)(void *ctx, u32 addr, const void *p, size_t n);
    /* One log line (print(text, file=log) and a flush): text without the line end. */
    void (*out)(void *ctx, AvpStr text);
    /* D.snap(name, lo, hi): returns false with a "Class: message" in *err when the file cannot be written. */
    bool (*snap)(void *ctx, AvpStr name, i64 lo, i64 hi, const char **err);
    /* D.sym(addr): the compiler name of addr, or hex(addr). */
    AvpStr (*sym)(void *ctx, AvpArena *a, i64 addr);
    /* The compiler build the run uses (the unit's mw_version, "Wii/1.0a"); NULL when unknown. */
    const char *build;
    /* The address of the compiler function called name (an exact NB11 name) in that build: false when the build has
     * no such function or its table cannot be loaded. NULL: no table. */
    bool (*lookup)(void *ctx, AvpArena *a, AvpStr name, i64 *addr);
} WdbgHost;

typedef struct WdbgSpec WdbgSpec;

/* Parses spec text. NULL on a syntax error, with "<path>:<line>: <message>" in *err. */
WdbgSpec *wdbg_spec_parse(AvpArena *a, const char *path, AvpStr text, const char **err);

/* Runs the spec's top-level statements in order (runpy.run_path) and registers its breakpoints. false on an
 * error with "Class: message" in *err (the Python's uncaught exception). */
bool wdbg_spec_load(WdbgSpec *s, const WdbgHost *host, const char **err);

/* The breakpoint addresses, in the order of their first `bp` (a repeated address keeps its first position and its
 * last body, as a dict literal). */
size_t wdbg_spec_bp_count(const WdbgSpec *s);
u32 wdbg_spec_bp_addr(const WdbgSpec *s, size_t i);
bool wdbg_spec_has_bp(const WdbgSpec *s, u32 addr);

/* One breakpoint hit at addr: D.hits[addr] += 1, then the handler with the given registers (pointers into the
 * debugger's thread context; a handler may assign them). A handler error writes "HANDLER ERR <Class>: <message>"
 * through host->out and returns false. */
bool wdbg_spec_hit(WdbgSpec *s, u32 addr, u32 *const regs[WDBG_NREGS]);

/* D.hits.get(addr, 0). */
i64 wdbg_spec_hits(const WdbgSpec *s, u32 addr);

/* Python's `fmt % args` for the value kinds of the language: false with "TypeError: ..." / "ValueError: ..." in
 * *err. Exposed for the tests. */
bool wdbg_format(AvpArena *a, AvpStr fmt, const WdbgVal *args, size_t n, AvpStr *out, const char **err);

/* str(v) and repr(v) of a value. */
AvpStr wdbg_val_str(AvpArena *a, WdbgVal v);
AvpStr wdbg_val_repr(AvpArena *a, WdbgVal v);
