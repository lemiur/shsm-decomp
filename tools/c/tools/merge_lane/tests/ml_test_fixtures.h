/* Shared helpers of merge_lane's unit tests (tools/test_merge_lane.py's LaneTest: setUp, tearDown, run_tool,
 * summary_rows, and the patched Stubs.ninja some TestChanges tests use). The fixture itself (the throwaway repository
 * and the stub commands) is ml_stubs.c; this header adds the per-test case around it: a fresh temporary folder
 * holding a folder with a space in its name ("repo dir"), the stubs, the run and its captured output. */
#pragma once
#include "avp/test.h"
#include "ml_stubs.h"
#include <stdarg.h>
#include <string.h>

#define TA avp_arena()

/* str(A_ADDR), str(B_ADDR) */
#define A_ADDR_S "2147483904"
#define B_ADDR_S "2147484160"

typedef struct TxCase {
    AvpArena *a;
    const char *tmp; /* the mkdtemp folder, removed by tx_end */
    MlFixture fx;
    MlStubs s;
    const char *ninja_report; /* non-NULL: the ninja stub then writes this report (a patched Stubs.ninja) */
    int rc;
    AvpStr text;
} TxCase;

/* LaneTest.setUp */
static inline TxCase *tx_begin(void) {
    AvpArena *a = TA;
    TxCase *c = AVP_NEW(a, TxCase);
    c->a = a;
    c->tmp = avp_mkdtemp(a, "merge_lane_test_", "", NULL);
    c->fx = mlfx_setup(a, avp_path_join(a, c->tmp, "repo dir"));
    mlst_init(&c->s, a, &c->fx);
    return c;
}

/* LaneTest.tearDown */
static inline void tx_end(TxCase *c) { avp_rmtree(c->tmp, AVP_RMTREE_CLEAR_READONLY | AVP_RMTREE_IGNORE_ERRORS); }

/* ---- hooks forwarding to the stubs (with the ninja report patched) ---------------------------------------------- */
static inline bool tx_hook_cmd(void *h, Ml *ml, const char *key, const MlExtra *extra, i64 *rc, AvpStr *text) {
    TxCase *c = h;
    MlHooks in = mlst_hooks(&c->s, NULL);
    bool r = in.command(in.ctx, ml, key, extra, rc, text);
    if (r && c->ninja_report && !strcmp(key, "ninja")) mlfx_write(ml->slot, ML_REPORT, avp_str(c->ninja_report));
    return r;
}
static inline bool tx_hook_git(void *h, Ml *ml, const char *const *args, size_t n, const char *cwd, MlGit *res) {
    TxCase *c = h;
    MlHooks in = mlst_hooks(&c->s, NULL);
    return in.git(in.ctx, ml, args, n, cwd, res);
}
static inline AvpJson *tx_hook_index(void *h, AvpArena *a, const char *path, AvpStr text) {
    TxCase *c = h;
    MlHooks in = mlst_hooks(&c->s, NULL);
    return in.index(in.ctx, a, path, text);
}
static inline void tx_hook_before(void *h, Ml *ml, const char *step) {
    TxCase *c = h;
    MlHooks in = mlst_hooks(&c->s, NULL);
    in.before_step(in.ctx, ml, step);
}

/* ml.main(argv, commands=stubs.commands()) with stdout captured; args NULL-terminated, after the program name. */
static inline int tx_main_argv(TxCase *c, const char *const *args) {
    size_t n = 0;
    while (args[n]) n++;
    char **argv = AVP_NEWN(c->a, char *, n + 2);
    argv[0] = "merge_lane";
    for (size_t i = 0; i < n; i++) argv[i + 1] = (char *)args[i];
    argv[n + 1] = NULL;
    AvpBuf out;
    avp_buf_init(&out, c->a);
    MlHooks h = mlst_hooks(&c->s, &out);
    if (c->ninja_report) {
        MlHooks in = h;
        h = (MlHooks){.ctx = c, .command = tx_hook_cmd, .git = tx_hook_git, .index = in.index ? tx_hook_index : NULL,
                      .before_step = tx_hook_before, .out = &out};
    }
    c->rc = ml_main((int)n + 1, argv, &h);
    c->text = avp_buf_str(&out);
    return c->rc;
}

/* The same with a NULL-terminated variadic list. */
static inline int tx_main(TxCase *c, ...) {
    AvpStrVec v = {0};
    va_list ap;
    va_start(ap, c);
    for (const char *s; (s = va_arg(ap, const char *)) != NULL;) avp_vec_push(&v, avp_str(s));
    va_end(ap);
    const char **args = AVP_NEWN(c->a, const char *, v.n + 1);
    for (size_t i = 0; i < v.n; i++) args[i] = v.v[i].p;
    args[v.n] = NULL;
    return tx_main_argv(c, args);
}

/* run_tool(*extra, branch=branch or "lane/x", onto=onto or "main"); extra NULL-terminated. Without a patched ninja
 * report it is ml_stubs' mlfx_run. */
static inline int tx_run(TxCase *c, const char *branch, const char *onto, ...) {
    AvpStrVec v = {0};
    va_list ap;
    va_start(ap, onto);
    for (const char *s; (s = va_arg(ap, const char *)) != NULL;) avp_vec_push(&v, avp_str(s));
    va_end(ap);
    const char **extra = AVP_NEWN(c->a, const char *, v.n + 12);
    for (size_t i = 0; i < v.n; i++) extra[i] = v.v[i].p;
    if (!c->ninja_report) {
        c->rc = mlfx_run(&c->fx, &c->s, branch, onto, extra, v.n, &c->text);
        return c->rc;
    }
    const char **args = AVP_NEWN(c->a, const char *, v.n + 12);
    const char *fixed[] = {"--slot", c->fx.slot, "--branch", branch ? branch : "lane/x", "--onto", onto ? onto : "main",
                           "--repo", c->fx.main, "--log-dir", c->fx.logs};
    size_t k = 0;
    for (size_t i = 0; i < 10; i++) args[k++] = fixed[i];
    for (size_t i = 0; i < v.n; i++) args[k++] = extra[i];
    args[k] = NULL;
    return tx_main_argv(c, args);
}

/* ---- assertions ------------------------------------------------------------------------------------------------- */
static inline bool tx_has(AvpStr hay, const char *needle) { return avp_str_find(hay, avp_str(needle), 0, AVP_END) >= 0; }

/* summary_rows(text)[step], or "(missing)" where the Python would raise KeyError. */
static inline const char *tx_row(AvpStr text, const char *step) {
    static const char *const order[] = {"preflight", "rebase", "gate", "changes", "rules", "layout", "ledger", "merge"};
    const char *found = "(missing)";
    bool in_sum = false;
    AvpStrVec lines = avp_str_splitlines(text, false);
    for (size_t i = 0; i < lines.n; i++) {
        if (avp_str_startswith(lines.v[i], AVP_S("== summary =="))) {
            in_sum = true;
            continue;
        }
        AvpStrVec parts = avp_str_split(lines.v[i], AVP_S(""), -1);
        if (!in_sum || parts.n < 2) continue;
        bool known = false;
        for (size_t k = 0; k < sizeof order / sizeof order[0]; k++) known |= avp_str_eqz(parts.v[0], order[k]);
        if (!known || !avp_str_eqz(parts.v[0], step)) continue;
        if (avp_str_eqz(parts.v[1], "PASS") || avp_str_eqz(parts.v[1], "FAIL") || avp_str_eqz(parts.v[1], "SKIP"))
            found = avp_cz(TA, parts.v[1]);
    }
    return found;
}

/* stubs.calls == [list] (NULL-terminated). */
static inline bool tx_calls_eq(const MlStubs *s, ...) {
    va_list ap;
    va_start(ap, s);
    size_t i = 0;
    bool ok = true;
    for (const char *k; (k = va_arg(ap, const char *)) != NULL; i++)
        if (i >= s->calls.n || !avp_str_eqz(s->calls.v[i], k)) ok = false;
    va_end(ap);
    return ok && i == s->calls.n;
}

static inline const char *tx_calls_str(const MlStubs *s) {
    return avp_cz(TA, avp_str_join(TA, AVP_S(" "), s->calls.v, s->calls.n));
}

/* text.split(sep)[1] */
static inline AvpStr tx_split1(AvpStr text, const char *sep) {
    AvpStrVec parts = avp_str_split(text, avp_str(sep), -1);
    return parts.n > 1 ? parts.v[1] : AVP_S("");
}

static inline AvpJson *tx_json(const char *text) {
    AvpJsonError e;
    AvpJson *j = avp_json_parse(TA, avp_str(text), &e);
    AVP_ASSERT(j);
    return j;
}

static inline AvpStr tx_read(const char *path) {
    AvpStr t = AVP_S("");
    if (!ml_read_text(TA, path, &t)) return AVP_S("<unreadable>");
    return t;
}

static inline AvpStr tx_replace(const char *s, const char *old, const char *new_) {
    return avp_str_replace(TA, avp_str(s), avp_str(old), avp_str(new_), -1);
}

/* A_SRC.replace("x + 1", "x + 2"): fn_A edited */
static inline AvpStr tx_a_edited(void) { return tx_replace(MLFX_A_SRC, "x + 1", "x + 2"); }
/* A_SRC + "// lane\n" */
static inline AvpStr tx_a_lane(void) { return avp_str(avp_sprintf(TA, "%s// lane\n", MLFX_A_SRC)); }

#define TX_LANE(c, rel, text) mlfx_lane_commit(&(c)->fx, (rel), (text), NULL, NULL, true)
#define TX_MAIN(c, rel, text) mlfx_main_commit(&(c)->fx, (rel), (text), NULL)
#define TX_GIT(c, cwd, ...) MLFX_GIT((c)->a, (cwd), __VA_ARGS__)

#define TX_RC(c, want)                                                                                          \
    CHECK_MSG((c)->rc == (want), "rc %d != %d; output:\n%.*s", (c)->rc, (int)(want), AVP_SF((c)->text))
#define TX_IN(c, needle) \
    CHECK_MSG(tx_has((c)->text, (needle)), "\"%s\" not in the output:\n%.*s", (needle), AVP_SF((c)->text))
#define TX_NOT_IN(c, needle) \
    CHECK_MSG(!tx_has((c)->text, (needle)), "\"%s\" in the output:\n%.*s", (needle), AVP_SF((c)->text))
#define TX_ROW(c, step, want)                                                                                    \
    CHECK_MSG(!strcmp(tx_row((c)->text, (step)), (want)), "summary %s: %s != %s", (step), tx_row((c)->text, (step)), \
              (want))
#define TX_CALLS(c, ...) \
    CHECK_MSG(tx_calls_eq(&(c)->s, __VA_ARGS__, NULL), "calls: [%s]", tx_calls_str(&(c)->s))
#define TX_NO_CALLS(c) CHECK_MSG((c)->s.calls.n == 0, "calls: [%s]", tx_calls_str(&(c)->s))
#define TX_EQ_CZ(got, want)                                                                                      \
    do {                                                                                                         \
        const char *tx_g_ = (got), *tx_w_ = (want);                                                              \
        CHECK_MSG(tx_g_ && tx_w_ && !strcmp(tx_g_, tx_w_), "%s == %s: \"%s\" != \"%s\"", #got, #want,             \
                  tx_g_ ? tx_g_ : "(null)", tx_w_ ? tx_w_ : "(null)");                                           \
    } while (0)
