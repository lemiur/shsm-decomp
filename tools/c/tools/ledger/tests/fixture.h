/* The synthetic inputs of tools/test_ledger.py (report, configure.py, symbols, progress.md, exceptions.md, checker
 * output) in a temporary root under build/tmp/, and a runner that calls ledger_main in process. */
#pragma once
#include "avp/test.h"
#include "ledger.h"
#include <string.h>

static const char PROGRESS[] =
    "# Progress ledger\n"
    "\n"
    "Hand-written intro.\n"
    "\n"
    "## Numbers\n"
    "\n"
    "| measure | functions | code bytes |\n"
    "|---|---|---|\n"
    "| stale | 1 / 2 | 3 / 4 |\n"
    "\n"
    "Totals exclude the `ORIGINAL-ASM` functions (1 functions and 2 code bytes in all, counting the dtk pad blocks).\n"
    "Hand-written prose stays.\n"
    "\n"
    "## Function status\n"
    "\n"
    "Status values: hand-written.\n"
    "\n"
    "| unit | function | address | size | status | naming |\n"
    "|---|---|---|---|---|---|\n"
    "| `lib/Beta` | `BetaA` | 0x80003000 | 20 | parked, 90.0% | renamed from `fn_80003000`: provenance note |\n"
    "| `lib/Alpha` | `AlphaOne` | 0x80002000 | 16 | matched |  |\n"
    "| `lib/Alpha` | `AlphaPend` | 0x80002018 | 12 | MATCH-PENDING (R17: hand-written reason) |  |\n"
    "\n"
    "Prose between the tables.\n"
    "\n"
    "## Game code\n"
    "\n"
    "Hand-written game intro.\n"
    "\n"
    "| unit | function | address | size | status | naming |\n"
    "|---|---|---|---|---|---|\n"
    "| `game/Delta` | `fn_80005000__5DeltaFv` | 0x80005000 | 24 | parked, 40.0% | placeholder |\n"
    "\n"
    "Tail prose.\n";

static const char EXCEPTIONS[] =
    "# Exceptions record\n"
    "\n"
    "## MATCH-PENDING\n"
    "\n"
    "| unit | function | rule | deviation |\n"
    "|---|---|---|---|\n"
    "| `lib/Alpha` | `AlphaPend` (1 function, 12 bytes) | R17 (steering) | steering text |\n"
    "\n"
    "## RULE-EXCEPTION\n"
    "\n"
    "| location | rule | deviation |\n"
    "|---|---|---|\n"
    "| `x` | R6 | text |\n"
    "\n"
    "## ORIGINAL-ASM\n"
    "\n"
    "| unit | function | evidence |\n"
    "|---|---|---|\n"
    "| `lib/Alpha` | `AlphaAsm` (1 function, 8 bytes) | nofralloc |\n"
    "\n"
    "## Listed inline-asm helpers (R18 original-helper exception; counted)\n"
    "\n"
    "| unit | function | helper | evidence |\n"
    "|---|---|---|---|\n"
    "| `lib/Alpha` | `AlphaOne` | `static inline void H(void)` | text |\n"
    "\n"
    "## Review rulings (counted)\n"
    "\n"
    "| function | rule | ruling |\n"
    "|---|---|---|\n"
    "| `AlphaOne` | R4 | text |\n"
    "\n"
    "## Open items that do not block a match\n"
    "\n"
    "Prose.\n";

static const char CONFIGURE[] = "config.libs = [\n"
                                "    {\"lib\": \"lib\", \"objects\": [\n"
                                "        # Object(Matching, \"lib/Commented.c\"),\n"
                                "        Object(Matching, \"lib/Alpha.c\"),\n"
                                "        Object(NonMatching, \"lib/Beta.c\"),\n"
                                "        Object(Matching, \"lib/Epsilon.c\"),\n"
                                "    ]},\n"
                                "    GameLib(\"game\", [\n"
                                "        Object(\n"
                                "            Matching,\n"
                                "            \"game/Gamma.cpp\",\n"
                                "        ),\n"
                                "        Object(NonMatching, \"game/Delta.cpp\"),\n"
                                "    ]),\n"
                                "]\n";

typedef struct {
    const char *name;
    i64 addr, size;
    double pct; /* < 0: none */
    const char *demangled;
} FxFn;

static const FxFn ALPHA[] = {{"AlphaOne", 0x80002000, 16, 100.0, NULL},
                             {"AlphaAsm", 0x80002010, 8, 100.0, NULL},
                             {"AlphaPend", 0x80002018, 12, 100.0, NULL}};
static const FxFn BETA[] = {{"BetaA", 0x80003000, 20, 96.449, NULL},
                            {"fn_80003014", 0x80003014, 4, -1, NULL},
                            {"BetaAsm", 0x80003018, 8, 100.0, NULL},
                            {"BetaNoScoreAsm", 0x80003020, 4, -1, NULL}};
static const FxFn GAMMA[] = {{"fn_80004000__5GammaFv", 0x80004000, 12, 100.0, "Gamma::fn_80004000()"},
                             {"__sinit_\\Gamma_cpp", 0x8000400C, 20, 100.0, NULL},
                             {"@36@fn_80004000__5GammaFv", 0x80004020, 8, 100.0, NULL}};
static const FxFn DELTA[] = {{"fn_80005000__5DeltaFv", 0x80005000, 24, 50.0, NULL}};
static const FxFn AUTO[] = {{"pad_03_80001000_text", 0x80001000, 8, -1, NULL}};
#define FX_N(x) (sizeof(x) / sizeof((x)[0]))

static AvpJson *fx_fn(const FxFn *f) {
    AvpArena *a = avp_arena();
    AvpJson *d = avp_json_obj(a), *md = avp_json_obj(a);
    avp_json_set(a, d, AVP_S("name"), avp_json_strz(a, f->name));
    avp_json_set(a, d, AVP_S("size"), avp_json_str(a, avp_pyfmt(a, "%d", f->size)));
    avp_json_set(a, md, AVP_S("virtual_address"), avp_json_str(a, avp_pyfmt(a, "%d", f->addr)));
    avp_json_set(a, d, AVP_S("metadata"), md);
    if (f->pct >= 0) avp_json_set(a, d, AVP_S("fuzzy_match_percent"), avp_json_float(a, f->pct));
    if (f->demangled) avp_json_set(a, md, AVP_S("demangled_name"), avp_json_strz(a, f->demangled));
    return d;
}

static AvpJson *fx_unit(const char *name, const FxFn *fns, size_t n, bool complete, const char *src, const char *cat,
                        bool autogen) {
    AvpArena *a = avp_arena();
    AvpJson *m = avp_json_obj(a);
    avp_json_set(a, m, AVP_S("complete"), avp_json_bool(complete));
    avp_json_set(a, m, AVP_S("module_name"), avp_json_strz(a, "main"));
    avp_json_set(a, m, AVP_S("module_id"), avp_json_int(a, 0));
    avp_json_set(a, m, AVP_S("auto_generated"), avp_json_bool(autogen));
    if (src) avp_json_set(a, m, AVP_S("source_path"), avp_json_strz(a, src));
    if (cat) {
        AvpJson *c = avp_json_arr(a);
        avp_json_arr_push(a, c, avp_json_strz(a, cat));
        avp_json_set(a, m, AVP_S("progress_categories"), c);
    }
    AvpJson *u = avp_json_obj(a);
    avp_json_set(a, u, AVP_S("name"), avp_json_str(a, avp_pyfmt(a, "main/%s", name)));
    avp_json_set(a, u, AVP_S("metadata"), m);
    if (n) {
        AvpJson *fl = avp_json_arr(a);
        i64 total = 0;
        for (size_t i = 0; i < n; i++) {
            avp_json_arr_push(a, fl, fx_fn(&fns[i]));
            total += fns[i].size;
        }
        avp_json_set(a, u, AVP_S("functions"), fl);
        AvpJson *ms = avp_json_obj(a);
        avp_json_set(a, ms, AVP_S("total_functions"), avp_json_int(a, (i64)n));
        avp_json_set(a, ms, AVP_S("total_code"), avp_json_str(a, avp_pyfmt(a, "%d", total)));
        avp_json_set(a, u, AVP_S("measures"), ms);
    }
    return u;
}

static i64 fx_sum(const FxFn *f, size_t n) {
    i64 s = 0;
    for (size_t i = 0; i < n; i++) s += f[i].size;
    return s;
}

static AvpJson *make_report(void) {
    AvpArena *a = avp_arena();
    AvpJson *units = avp_json_arr(a);
    avp_json_arr_push(a, units, fx_unit("auto_00_80001000_text", AUTO, FX_N(AUTO), false, NULL, NULL, true));
    avp_json_arr_push(a, units, fx_unit("lib/Alpha", ALPHA, FX_N(ALPHA), true, "src/lib/Alpha.c", "sdk", false));
    avp_json_arr_push(a, units, fx_unit("lib/Beta", BETA, FX_N(BETA), false, "src/lib/Beta.c", "sdk", false));
    avp_json_arr_push(a, units, fx_unit("lib/Epsilon", NULL, 0, true, "src/lib/Epsilon.c", "sdk", false));
    avp_json_arr_push(a, units, fx_unit("game/Gamma", GAMMA, FX_N(GAMMA), true, "src/game/Gamma.cpp", "game", false));
    avp_json_arr_push(a, units, fx_unit("game/Delta", DELTA, FX_N(DELTA), false, "src/game/Delta.cpp", "game", false));
    i64 nall = (i64)(FX_N(AUTO) + FX_N(ALPHA) + FX_N(BETA) + FX_N(GAMMA) + FX_N(DELTA));
    i64 ball = fx_sum(AUTO, FX_N(AUTO)) + fx_sum(ALPHA, FX_N(ALPHA)) + fx_sum(BETA, FX_N(BETA)) +
               fx_sum(GAMMA, FX_N(GAMMA)) + fx_sum(DELTA, FX_N(DELTA));
    i64 blinked = fx_sum(ALPHA, FX_N(ALPHA)) + fx_sum(GAMMA, FX_N(GAMMA));
    AvpJson *ms = avp_json_obj(a);
    avp_json_set(a, ms, AVP_S("total_functions"), avp_json_int(a, nall));
    avp_json_set(a, ms, AVP_S("total_code"), avp_json_str(a, avp_pyfmt(a, "%d", ball)));
    avp_json_set(a, ms, AVP_S("complete_code"), avp_json_str(a, avp_pyfmt(a, "%d", blinked)));
    avp_json_set(a, ms, AVP_S("complete_units"), avp_json_int(a, 3));
    avp_json_set(a, ms, AVP_S("total_units"), avp_json_int(a, 6));
    AvpJson *r = avp_json_obj(a);
    avp_json_set(a, r, AVP_S("measures"), ms);
    avp_json_set(a, r, AVP_S("units"), units);
    avp_json_set(a, r, AVP_S("version"), avp_json_int(a, 2));
    return r;
}

static AvpJson *fx_rec(const char *const *kv) {
    AvpArena *a = avp_arena();
    AvpJson *o = avp_json_obj(a);
    for (size_t i = 0; kv[i]; i += 2) {
        if (strcmp(kv[i], "match_rules") == 0) {
            AvpJson *l = avp_json_arr(a);
            if (*kv[i + 1]) avp_json_arr_push(a, l, avp_json_strz(a, kv[i + 1]));
            avp_json_set(a, o, AVP_S("match_rules"), l);
        } else avp_json_set(a, o, avp_str(kv[i]), avp_json_strz(a, kv[i + 1]));
    }
    return o;
}

/* checker_json(): functions, allowed, findings, exceptions */
static AvpJson *checker_json(void) {
    AvpArena *a = avp_arena();
    static const char *const f0[] = {"path", "src/lib/Alpha.c", "name", "AlphaOne", "status", "ok", "match_rules", "",
                                     "unit_status", "Matching", NULL};
    static const char *const f1[] = {"path", "src/lib/Alpha.c", "name", "AlphaAsm", "status", "ORIGINAL-ASM",
                                     "match_rules", "", "unit_status", "Matching", NULL};
    static const char *const f2[] = {"path", "src/lib/Beta.c", "name", "BetaA", "status", "MATCH-PENDING",
                                     "match_rules", "R17", "unit_status", "NonMatching", NULL};
    static const char *const f3[] = {"path", "src/game/Gamma.cpp", "name", "Gamma::fn_80004000", "status", "ok",
                                     "match_rules", "", "unit_status", "Matching", NULL};
    static const char *const a0[] = {"path", "src/lib/Alpha.c", "function", "AlphaAsm", "rule", "R18", "status",
                                     "allowed", "message", "assembly body: function 'AlphaAsm' is written in assembly",
                                     "reason", "ORIGINAL-ASM listed in docs/exceptions.md", NULL};
    static const char *const a1[] = {"path", "src/lib/Beta.c", "function", "BetaAsm", "rule", "R18", "status",
                                     "allowed", "message", "assembly body: function 'BetaAsm' is written in assembly",
                                     "reason", "marker", NULL};
    static const char *const a2[] = {"path", "src/lib/Beta.c", "function", "BetaNoScoreAsm", "rule", "R18", "status",
                                     "allowed", "message",
                                     "assembly body: function 'BetaNoScoreAsm' is written in assembly", "reason", "marker",
                                     NULL};
    AvpJson *funcs = avp_json_arr(a), *allowed = avp_json_arr(a);
    avp_json_arr_push(a, funcs, fx_rec(f0));
    avp_json_arr_push(a, funcs, fx_rec(f1));
    avp_json_arr_push(a, funcs, fx_rec(f2));
    avp_json_arr_push(a, funcs, fx_rec(f3));
    avp_json_arr_push(a, allowed, fx_rec(a0));
    avp_json_arr_push(a, allowed, fx_rec(a1));
    avp_json_arr_push(a, allowed, fx_rec(a2));
    AvpJson *c = avp_json_obj(a);
    avp_json_set(a, c, AVP_S("functions"), funcs);
    avp_json_set(a, c, AVP_S("allowed"), allowed);
    avp_json_set(a, c, AVP_S("findings"), avp_json_arr(a));
    avp_json_set(a, c, AVP_S("exceptions"), avp_json_arr(a));
    return c;
}

static void symbols_add(AvpBuf *b, const FxFn *f, size_t n) {
    for (size_t i = 0; i < n; i++)
        avp_buf_pyfmt(b, "%s = .text:0x%08X; // type:function size:0x%X scope:global\n", f[i].name, f[i].addr,
                      f[i].size);
}

typedef struct {
    char *root;
    char *report, *checker;
} Fixture;

static char *fx_path(Fixture *fx, const char *rel) { return avp_path_join(avp_arena(), fx->root, rel); }

static char *fx_write_eol(Fixture *fx, const char *rel, AvpStr text, const char *eol) {
    AvpArena *a = avp_arena();
    char *p = fx_path(fx, rel);
    AvpStr dir = avp_path_dirname(p);
    avp_makedirs(avp_cz(a, dir), true);
    AvpStr body = strcmp(eol, "\n") == 0 ? text : avp_str_replace(a, text, AVP_S("\n"), avp_str(eol), -1);
    avp_write_file(p, body.p, body.n);
    return p;
}
static char *fx_write(Fixture *fx, const char *rel, const char *text) {
    return fx_write_eol(fx, rel, avp_str(text), "\n");
}
static char *fx_write_json(Fixture *fx, const char *rel, const AvpJson *doc) {
    AvpStr s = avp_json_dumps(avp_arena(), doc, NULL);
    return fx_write_eol(fx, rel, s, "\n");
}
static AvpStr fx_read(Fixture *fx, const char *rel) {
    AvpBytes b = {0};
    avp_read_all(avp_arena(), fx_path(fx, rel), &b);
    return avp_strn((const char *)b.v, b.n);
}

static Fixture fx_new(void) {
    AvpArena *a = avp_arena();
    Fixture fx = {0};
    avp_makedirs("build/tmp", true);
    fx.root = avp_path_abspath(a, avp_mkdtemp(a, "ledger_test_", "", "build/tmp"));
    fx_write(&fx, "configure.py", CONFIGURE);
    AvpBuf sb;
    avp_buf_init(&sb, a);
    symbols_add(&sb, AUTO, FX_N(AUTO));
    symbols_add(&sb, ALPHA, FX_N(ALPHA));
    symbols_add(&sb, BETA, FX_N(BETA));
    symbols_add(&sb, GAMMA, FX_N(GAMMA));
    symbols_add(&sb, DELTA, FX_N(DELTA));
    fx_write(&fx, "config/R5WEA4/symbols.txt", avp_buf_cz(&sb));
    fx_write(&fx, "docs/progress.md", PROGRESS);
    fx_write(&fx, "docs/exceptions.md", EXCEPTIONS);
    fx_write(&fx, "src/lib/Alpha.c", "// alpha\nint AlphaOne(void);\nvoid AlphaAsm(void);\nvoid AlphaPend(void);\n");
    fx_write(&fx, "src/lib/Beta.c", "// Unit name unconfirmed: invented for the test.\nvoid BetaA(void);\n");
    fx_write(&fx, "src/lib/Epsilon.c", "// data only\n");
    fx_write(&fx, "src/game/Gamma.cpp", "// unit name unconfirmed: no string names the file\nvoid fn_80004000(void);\n");
    fx_write(&fx, "src/game/Delta.cpp", "// game unit with a known name\n");
    fx_write(&fx, "include/h.h", "");
    fx.report = fx_write_json(&fx, "report.json", make_report());
    fx.checker = fx_write_json(&fx, "checker.json", checker_json());
    return fx;
}

static void fx_done(Fixture *fx) { avp_rmtree(fx->root, 0); }

typedef struct {
    int code;
    AvpStr out, err;
} FxRun;

static FxRun fx_runv(const char *const *args) {
    AvpArena *a = avp_arena();
    char *argv[40];
    int argc = 0;
    argv[argc++] = "ledger";
    for (size_t i = 0; args[i]; i++) argv[argc++] = (char *)args[i];
    AvpBuf out, err;
    avp_buf_init(&out, a);
    avp_buf_init(&err, a);
    FxRun r;
    r.code = ledger_main(argc, argv, &out, &err);
    r.out = avp_buf_str(&out);
    r.err = avp_buf_str(&err);
    return r;
}

/* fx.progress(*extra) / fx.exc(*extra) */
static FxRun fx_cmd(Fixture *fx, const char *cmd, const char *const *extra) {
    const char *args[40];
    size_t n = 0;
    args[n++] = cmd;
    args[n++] = "--root";
    args[n++] = fx->root;
    args[n++] = "--report";
    args[n++] = fx->report;
    args[n++] = "--checker-json";
    args[n++] = fx->checker;
    for (size_t i = 0; extra && extra[i]; i++) args[n++] = extra[i];
    args[n] = NULL;
    return fx_runv(args);
}
#define FX_ARGS(...) ((const char *const[]){__VA_ARGS__, NULL})
#define FX_PROGRESS(fx, ...) fx_cmd((fx), "progress", FX_ARGS(__VA_ARGS__))
#define FX_PROGRESS0(fx) fx_cmd((fx), "progress", NULL)
#define FX_EXC(fx, ...) fx_cmd((fx), "exceptions-check", FX_ARGS(__VA_ARGS__))
#define FX_EXC0(fx) fx_cmd((fx), "exceptions-check", NULL)

static bool has(AvpStr hay, const char *needle) { return avp_str_contains(hay, avp_str(needle)); }
static AvpStr replace(AvpStr s, const char *old, const char *new_) {
    return avp_str_replace(avp_arena(), s, avp_str(old), avp_str(new_), -1);
}
