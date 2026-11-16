/* Shared fixtures of the gate's unit tests (the helpers at the top of tools/test_gate.py): make_root, a scripted
 * runner answering by the first argv substring (Scripted, git_runner), temporary roots. */
#pragma once
#include "avp/test.h"
#include "gate.h"

#define TA avp_arena()

static const char *tf_tmpdir(void) { return avp_mkdtemp(TA, "gate_test_", "", NULL); }

static void tf_write(const char *path, const char *text) {
    AvpStr d = avp_path_dirname(path);
    if (d.n) avp_makedirs(avp_cz(TA, d), true);
    AVP_ASSERT(avp_write_file(path, text, strlen(text)));
}

static void tf_write_n(const char *path, const void *p, size_t n) {
    AvpStr d = avp_path_dirname(path);
    if (d.n) avp_makedirs(avp_cz(TA, d), true);
    AVP_ASSERT(avp_write_file(path, p, n));
}

static void tf_append(const char *path, const char *text) { AVP_ASSERT(avp_append_file(path, text, strlen(text))); }

static const char *tf_join(const char *root, const char *rel) { return avp_path_join(TA, root, rel); }

/* make_root(tmp, dol=b"DOL!" * 64, contract=None, yml_hash=None) -> sha1 of the DOL. */
static const char *tf_make_root(const char *tmp, const char *contract, const char *yml_hash) {
    AvpStr dol = avp_str_repeat(TA, AVP_S("DOL!"), 64);
    const char *sha = avp_sha1_hex(TA, dol.p, dol.n);
    tf_write(tf_join(tmp, "config/R5WEA4/build.sha1"),
             avp_sprintf(TA, "%s  build/R5WEA4/main.dol\n", contract ? contract : sha));
    tf_write(tf_join(tmp, "config/R5WEA4/config.yml"),
             avp_sprintf(TA, "object_base: orig/R5WEA4\nobject: sys/main.dol\nhash: %s\n",
                         yml_hash ? yml_hash : (contract ? contract : sha)));
    tf_write_n(tf_join(tmp, "orig/R5WEA4/sys/main.dol"), dol.p, dol.n);
    tf_write_n(tf_join(tmp, "build/R5WEA4/main.dol"), dol.p, dol.n);
    return sha;
}

static GateOpts tf_opts(void) { return (GateOpts){0}; }

/* ---- the scripted runner: the first entry whose key occurs in " ".join(argv) answers; else exit 0, no output */
typedef struct TfEntry {
    const char *key;
    i64 rc;
    const char *out;
    void (*fn)(void *ctx, const char *cwd); /* side effect before answering (Scripted's callable result) */
    void *ctx;
    size_t out_n; /* the length of out when it holds NULs (0: strlen) */
} TfEntry;

typedef struct TfRunner {
    const TfEntry *entries;
    size_t n;
    AvpStrVec calls;
} TfRunner;

static GateRun tf_run(void *ctx, AvpArena *a, const char *const *argv, size_t argc, const char *cwd, const AvpEnv *env,
                      double timeout) {
    (void)env;
    (void)timeout;
    TfRunner *r = ctx;
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < argc; i++) {
        if (i) avp_buf_putc(&b, ' ');
        avp_buf_putz(&b, argv[i]);
    }
    AvpStr text = avp_buf_str(&b);
    if (r) avp_vec_push(&r->calls, text);
    GateRun res = {0, false, AVP_S(""), AVP_S(""), 0.01};
    for (size_t i = 0; r && i < r->n; i++) {
        if (!avp_str_contains(text, avp_str(r->entries[i].key))) continue;
        if (r->entries[i].fn) r->entries[i].fn(r->entries[i].ctx, cwd);
        res.returncode = r->entries[i].rc;
        const char *o = r->entries[i].out ? r->entries[i].out : "";
        res.out = r->entries[i].out_n ? avp_strn(o, r->entries[i].out_n) : avp_str(o);
        return res;
    }
    return res;
}

/* The rules step's checker and scanner as command lines through tf_run ("classes scan", "check_rules --json ..."). */
static GateRulesTools tf_tools(TfRunner *r) {
    GateRunnerTools *rt = AVP_NEW(TA, GateRunnerTools);
    *rt = (GateRunnerTools){tf_run, r, NULL};
    return gate_rules_tools_via_runner(rt);
}

static bool tf_called(const TfRunner *r, const char *key) {
    for (size_t i = 0; i < r->calls.n; i++)
        if (avp_str_contains(r->calls.v[i], avp_str(key))) return true;
    return false;
}

/* git_runner: rev-parse answers abc123 */
static const TfEntry TF_GIT[] = {{"rev-parse", 0, "abc123\n", NULL, NULL}};
static TfRunner tf_git_runner(void) { return (TfRunner){TF_GIT, 1, {0}}; }

static bool tf_contains(const char *hay, const char *needle) { return hay && strstr(hay, needle) != NULL; }
static bool tf_starts(const char *s, const char *prefix) { return s && strncmp(s, prefix, strlen(prefix)) == 0; }

static const GateStep *tf_step(const Gate *g, const char *name) {
    for (size_t i = 0; i < GATE_NSTEPS; i++)
        if (strcmp(g->steps[i].name, name) == 0) return &g->steps[i];
    return NULL;
}

/* Runs fn and returns the exception code it raised (0 when none); msg receives the message. */
#define TF_RAISES(code_var, msg_var, stmt)                                                                         \
    do {                                                                                                           \
        AvpTry tf_t_;                                                                                              \
        if (AVP_TRY(&tf_t_)) {                                                                                     \
            stmt;                                                                                                  \
            avp_try_end(&tf_t_);                                                                                   \
            code_var = 0;                                                                                          \
        } else {                                                                                                   \
            code_var = tf_t_.code;                                                                                 \
            msg_var = avp_strdup(TA, tf_t_.msg);                                                                   \
        }                                                                                                          \
    } while (0)
