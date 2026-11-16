/* slot: paths, links, repository, health and locks of tools/slot.py. */
#include "slot.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const SlotShared SLOT_SHARED[SLOT_NSHARED] = {
    {"tools/bin", true, "bin"},
    {"tools/compilers", true, "compilers"},
    {"orig/R5WEA4/sys", true, "sys"},
    {"orig/R5WEA4/files", false, "files"},
    {"tools/src", false, "src"},
    {"tools/venv", false, "venv"},
    {"tools/permuter", false, "permuter"},
    {"orig/_iso", false, "iso"},         /* the PS2 executable witness.exe reads */
    {"orig/_ref", false, "ref"},         /* the sibling binaries sibling.exe indexes */
    {"build/binutils", false, "binutils", true}, /* powerpc-eabi-objdump and the rest of the downloaded binutils */
};

const char *const SLOT_REQUIRED_FILES[SLOT_NREQUIRED] = {
    "tools/bin/dtk.exe",
    "tools/bin/objdiff-cli.exe",
    "tools/bin/ninja.exe",
    "tools/compilers/Wii/1.0/mwcceppc.exe",
    "tools/compilers/Wii/1.0a/mwcceppc.exe",
    "tools/compilers/GC/3.0a5/mwcceppc.exe",
    "orig/R5WEA4/sys/main.dol",
};

/* ---- errors ---------------------------------------------------------------------------------------------------- */

static char *g_msg;

void slot_fail(int code, const char *fmt, ...) {
    AvpBuf b;
    avp_buf_init(&b, avp_arena());
    va_list ap;
    va_start(ap, fmt);
    avp_buf_vprintf(&b, fmt, ap);
    va_end(ap);
    g_msg = avp_buf_cz(&b);
    avp_throw(code, "%.900s", g_msg);
}

const char *slot_error_message(void) { return g_msg ? g_msg : ""; }

/* ---- paths and links ------------------------------------------------------------------------------------------- */

const char *slot_norm(AvpArena *a, const char *path) { return shsm_git_path_key(a, path); }

bool slot_same(AvpArena *a, const char *x, const char *y) { return strcmp(slot_norm(a, x), slot_norm(a, y)) == 0; }

bool slot_is_link(const char *path) { return shsm_link_kind(path) != SHSM_LINK_NONE; }

const char *slot_link_target(AvpArena *a, const char *path) {
    char *t = shsm_link_target(a, path);
    return t ? slot_norm(a, t) : NULL;
}

void slot_remove_link(const char *path) {
    AvpStat st;
    if (!avp_lstat(path, &st)) shsm_raise_io();
    if (!slot_is_link(path)) slot_fail(SLOT_REFUSAL, "not a link: %s", path);
    if (!shsm_link_remove(path)) shsm_raise_io();
}

AvpStrVec slot_scan_links(AvpArena *a, const char *root) {
    AvpStrVec out = {0};
    if (slot_is_link(root)) {
        avp_vec_push(&out, avp_str(avp_strdup(a, root)));
        return out;
    }
    ShsmLinkVec v = {0};
    if (!shsm_links_walk(a, root, &v)) return out; /* scan_links: an unlistable root gives [] */
    for (size_t i = 0; i < v.n; i++) {
        char *rel = avp_strdup(a, v.v[i].rel);
        for (char *p = rel; *p; p++)
            if (*p == '/') *p = '\\';
        avp_vec_push(&out, avp_str(avp_path_join(a, root, rel)));
    }
    avp_vec_free(&v);
    avp_sort_strs(out.v, out.n);
    return out;
}

/* ---- git ------------------------------------------------------------------------------------------------------- */

/* slot.git(cwd, *args): `git -c core.quotepath=off -C cwd ...`, text mode, never raises for an exit code. */
static ShsmGitRun git(AvpArena *a, const char *cwd, const char *const *args, size_t n) {
    ShsmGitOpts o = {.quotepath_off = true};
    return shsm_git_run(a, cwd, args, n, &o);
}
#define GIT(a, cwd, ...) git((a), (cwd), SHSM_GIT_ARGS_(__VA_ARGS__))

/* (err or out).strip().splitlines() of a failed run. */
static AvpStrVec failure_lines(const ShsmGitRun *r) { return avp_str_splitlines(shsm_git_failure_text(r), false); }

/* git(..., check=True): ToolError "git <args>: [<repr of the last line>]". */
static void git_check(AvpArena *a, const ShsmGitRun *r, const char *const *args, size_t n) {
    if (shsm_git_ok(r)) return;
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, "git ");
    for (size_t i = 0; i < n; i++) {
        if (i) avp_buf_putc(&b, ' ');
        avp_buf_putz(&b, args[i]);
    }
    avp_buf_putz(&b, ": [");
    AvpStrVec lines = failure_lines(r);
    if (lines.n) avp_buf_put(&b, avp_repr_str(a, lines.v[lines.n - 1]));
    avp_buf_putc(&b, ']');
    slot_fail(SLOT_TOOL_ERROR, "%s", avp_buf_cz(&b));
}

/* (err or out).strip().splitlines()[-1] for a refusal message (an IndexError in the Python when there is none). */
static const char *last_failure_line(AvpArena *a, const ShsmGitRun *r) {
    AvpStrVec lines = failure_lines(r);
    if (!lines.n) slot_fail(SLOT_UNCAUGHT, "IndexError: list index out of range");
    return avp_cz(a, lines.v[lines.n - 1]);
}

static bool is_digits(const char *s) {
    if (!*s) return false;
    for (; *s; s++)
        if (*s < '0' || *s > '9') return false;
    return true;
}

/* ---- repository ------------------------------------------------------------------------------------------------ */

SlotRepo *slot_repo_open(AvpArena *a, const char *repo, const char *base) {
    ShsmGitRun r = GIT(a, repo, "rev-parse", "--show-toplevel", "--git-common-dir");
    if (!shsm_git_ok(&r)) slot_fail(SLOT_TOOL_ERROR, "not a git repository: %s", repo);
    AvpStrVec lines = avp_str_splitlines(r.out, false);
    if (lines.n < 2) slot_fail(SLOT_UNCAUGHT, "ValueError: not enough values to unpack (expected 2, got %zu)", lines.n);
    SlotRepo *s = AVP_NEW(a, SlotRepo);
    s->cwd = avp_cz(a, lines.v[0]);
    /* slot.py joins the common dir with the top-level (not the folder git ran in): kept for the same lock folder */
    s->common = slot_norm(a, avp_path_join(a, s->cwd, avp_cz(a, lines.v[1])));
    s->base = base;
    static const char *const wl[] = {"worktree", "list", "--porcelain"};
    ShsmGitRun w = git(a, s->cwd, wl, 3);
    git_check(a, &w, wl, 3);
    ShsmGitWorktreeVec v = {0};
    shsm_git_parse_worktrees(a, w.out, &v);
    if (!v.n) slot_fail(SLOT_TOOL_ERROR, "git worktree list returned nothing");
    s->trees = AVP_NEWN(a, SlotTree, v.n);
    s->n = v.n;
    for (size_t i = 0; i < v.n; i++) {
        SlotTree *t = &s->trees[i];
        t->path = v.v[i].npath;
        t->head = v.v[i].head ? v.v[i].head : "";
        t->branch = shsm_git_branch_short(v.v[i].branch);
        t->locked = v.v[i].locked;
        t->prunable = v.v[i].prunable;
    }
    avp_vec_free(&v);
    s->main = s->trees[0].path;
    return s;
}

SlotTree slot_repo_find(AvpArena *a, const SlotRepo *r, const char *arg) {
    AvpStrVec cands = {0};
    bool digits = is_digits(arg);
    if (digits) avp_vec_push(&cands, avp_str(slot_norm(a, avp_path_join(a, r->base, avp_sprintf(a, "slot%s", arg)))));
    avp_vec_push(&cands, avp_str(slot_norm(a, arg)));
    const char *carg = avp_path_normcase(a, arg);
    for (size_t i = 0; i < r->n; i++) {
        const char *bn = avp_path_normcase(a, avp_cz(a, avp_path_basename(r->trees[i].path)));
        if (strcmp(bn, carg) == 0) avp_vec_insert(&cands, 0, avp_str(slot_norm(a, r->trees[i].path)));
    }
    for (size_t c = 0; c < cands.n; c++)
        for (size_t i = 0; i < r->n; i++)
            if (avp_str_eqz(cands.v[c], slot_norm(a, r->trees[i].path))) {
                avp_vec_free(&cands);
                return r->trees[i];
            }
    avp_vec_free(&cands);
    SlotTree t = {0};
    t.path = digits ? avp_path_abspath(a, avp_path_join(a, r->base, avp_sprintf(a, "slot%s", arg)))
                    : avp_path_abspath(a, arg);
    t.head = "";
    t.unknown = true;
    return t;
}

const char *slot_lock_path(AvpArena *a, const SlotRepo *r, const char *path) {
    const char *name = avp_sprintf(a, "%s.json", avp_cz(a, avp_path_basename(path)));
    return avp_join(a, r->common, SLOT_LOCK_DIR, name);
}

/* ---- health ---------------------------------------------------------------------------------------------------- */

void slot_link_states(AvpArena *a, const char *slot, const char *main, SlotLinkState out[SLOT_NSHARED]) {
    for (size_t i = 0; i < SLOT_NSHARED; i++) {
        const SlotShared *sh = &SLOT_SHARED[i];
        const char *src = avp_path_join(a, main, sh->rel);
        const char *dst = avp_path_join(a, slot, sh->rel);
        bool have_src = avp_isdir(src);
        SlotLinkState *s = &out[i];
        s->rel = sh->rel;
        s->required = sh->required;
        s->detail = "";
        if (slot_is_link(dst)) {
            const char *tgt = slot_link_target(a, dst);
            if (!tgt || !avp_exists(dst)) {
                s->state = "broken";
                s->detail = tgt ? tgt : "?";
            } else if (strcmp(tgt, slot_norm(a, src)) != 0) {
                s->state = "wrong";
                s->detail = tgt;
            } else {
                s->state = "ok";
            }
        } else if (avp_exists(dst)) {
            s->state = ((have_src || sh->required) && !sh->copy_ok) ? "copy" : "local";
        } else if (have_src || sh->required) {
            s->state = "missing";
        } else {
            s->state = "n/a";
        }
    }
}

SlotHealth slot_health(AvpArena *a, const char *slot, const char *main) {
    SlotHealth h = {0};
    slot_link_states(a, slot, main, h.links);
    for (size_t i = 0; i < SLOT_NSHARED; i++) {
        const SlotLinkState *s = &h.links[i];
        const char *st = s->state;
        if (!strcmp(st, "copy") || !strcmp(st, "wrong") || !strcmp(st, "broken") || (!strcmp(st, "missing") && s->required))
            avp_vec_push(&h.problems, avp_str(avp_sprintf(a, "%s %s%s%s", s->rel, st, *s->detail ? " -> " : "",
                                                          s->detail)));
    }
    for (size_t i = 0; i < SLOT_NREQUIRED; i++)
        if (!avp_isfile(avp_path_join(a, slot, SLOT_REQUIRED_FILES[i]))) {
            avp_vec_push(&h.missing_files, avp_str(SLOT_REQUIRED_FILES[i]));
            avp_vec_push(&h.problems, avp_str(avp_sprintf(a, "unreachable %s", SLOT_REQUIRED_FILES[i])));
        }
    const char *shared[SLOT_NSHARED];
    for (size_t i = 0; i < SLOT_NSHARED; i++) shared[i] = slot_norm(a, avp_path_join(a, slot, SLOT_SHARED[i].rel));
    AvpStrVec links = slot_scan_links(a, slot);
    for (size_t i = 0; i < links.n; i++) {
        const char *k = slot_norm(a, links.v[i].p);
        bool is_shared = false;
        for (size_t j = 0; j < SLOT_NSHARED && !is_shared; j++) is_shared = strcmp(k, shared[j]) == 0;
        if (!is_shared) avp_vec_push(&h.extra_links, links.v[i]);
    }
    avp_vec_free(&links);
    return h;
}

/* ---- Python values --------------------------------------------------------------------------------------------- */

static void py_repr(AvpBuf *b, const AvpJson *v);

static void py_scalar(AvpBuf *b, const AvpJson *v, bool repr) {
    char num[32];
    if (!v || v->kind == AVP_JSON_NULL) avp_buf_putz(b, "None");
    else if (v->kind == AVP_JSON_BOOL) avp_buf_putz(b, v->b ? "True" : "False");
    else if (v->kind == AVP_JSON_INT) {
        if (v->big) avp_buf_put(b, v->u.str);
        else avp_buf_printf(b, "%lld", (long long)v->u.i);
    } else if (v->kind == AVP_JSON_FLOAT) {
        avp_float_repr(v->u.f, num);
        avp_buf_putz(b, num);
    } else if (v->kind == AVP_JSON_STRING) {
        if (repr) avp_buf_repr_str(b, v->u.str);
        else avp_buf_put(b, v->u.str);
    } else py_repr(b, v);
}

static void py_repr(AvpBuf *b, const AvpJson *v) {
    if (v && v->kind == AVP_JSON_ARRAY) {
        avp_buf_putc(b, '[');
        for (size_t i = 0; i < v->u.arr.n; i++) {
            if (i) avp_buf_putz(b, ", ");
            py_scalar(b, v->u.arr.v[i], true);
        }
        avp_buf_putc(b, ']');
    } else if (v && v->kind == AVP_JSON_OBJECT) {
        avp_buf_putc(b, '{');
        for (size_t i = 0; i < v->u.obj.n; i++) {
            if (i) avp_buf_putz(b, ", ");
            avp_buf_repr_str(b, v->u.obj.v[i].key);
            avp_buf_putz(b, ": ");
            py_scalar(b, v->u.obj.v[i].val, true);
        }
        avp_buf_putc(b, '}');
    } else {
        py_scalar(b, v, true);
    }
}

const char *slot_py_str(AvpArena *a, const AvpJson *v) {
    AvpBuf b;
    avp_buf_init(&b, a);
    py_scalar(&b, v, false);
    return avp_buf_cz(&b);
}

static const char *py_type(const AvpJson *v) {
    if (!v || v->kind == AVP_JSON_NULL) return "NoneType";
    switch (v->kind) {
    case AVP_JSON_BOOL: return "bool";
    case AVP_JSON_INT: return "int";
    case AVP_JSON_FLOAT: return "float";
    case AVP_JSON_STRING: return "str";
    case AVP_JSON_ARRAY: return "list";
    default: return "dict";
    }
}

/* float(v) (absent: dflt); what Python could not convert is an uncaught TypeError / ValueError. */
static double py_float(const AvpJson *lock, const char *key, double dflt) {
    const AvpJson *v = avp_json_getz(lock, key);
    if (!v) return dflt;
    double f;
    switch (v->kind) {
    case AVP_JSON_BOOL: return v->b ? 1.0 : 0.0;
    case AVP_JSON_INT:
    case AVP_JSON_FLOAT: return avp_json_f64(v, 0.0);
    case AVP_JSON_STRING:
        if (avp_parse_float(v->u.str, &f)) return f;
        slot_fail(SLOT_UNCAUGHT, "ValueError: could not convert string to float: %s", avp_cz(avp_arena(),
                  avp_repr_str(avp_arena(), v->u.str)));
    default:
        slot_fail(SLOT_UNCAUGHT, "TypeError: float() argument must be a string or a real number, not '%s'", py_type(v));
    }
}

/* ---- locks ----------------------------------------------------------------------------------------------------- */

static AvpJson *corrupt_lock(AvpArena *a, const char *lp) {
    AvpJson *d = avp_json_obj(a);
    avp_json_set(a, d, AVP_S("owner"), avp_json_strz(a, "?"));
    avp_json_set(a, d, AVP_S("corrupt"), avp_json_bool(true));
    avp_json_set(a, d, AVP_S("epoch"), avp_json_int(a, 0));
    avp_json_set(a, d, AVP_S("ttl_hours"), avp_json_int(a, 0));
    avp_json_set(a, d, AVP_S("file"), avp_json_strz(a, lp));
    return d;
}

AvpJson *slot_read_lock(AvpArena *a, const SlotRepo *r, const char *path) {
    const char *lp = slot_lock_path(a, r, path);
    AvpBytes data = {0};
    if (!avp_read_all(a, lp, &data)) {
        if (avp_io_is(ENOENT)) return NULL;
        return corrupt_lock(a, lp);
    }
    AvpJsonError err;
    AvpJson *d = avp_json_parse(a, (AvpStr){(const char *)data.v, data.n}, &err);
    if (!d) return corrupt_lock(a, lp);
    if (d->kind != AVP_JSON_OBJECT) {
        if (d->kind == AVP_JSON_ARRAY) slot_fail(SLOT_UNCAUGHT, "TypeError: list indices must be integers or slices, not str");
        slot_fail(SLOT_UNCAUGHT, "TypeError: '%s' object does not support item assignment", py_type(d));
    }
    avp_json_set(a, d, AVP_S("file"), avp_json_strz(a, lp));
    return d;
}

double slot_lock_age_h(const AvpJson *lock, double now) { return (now - py_float(lock, "epoch", 0.0)) / 3600.0; }

bool slot_lock_is_stale(const AvpJson *lock, double now) {
    if (avp_json_truthy(avp_json_getz(lock, "corrupt"))) return true;
    double age = slot_lock_age_h(lock, now);
    return age > py_float(lock, "ttl_hours", SLOT_DEFAULT_TTL_HOURS);
}

const char *slot_lock_text(AvpArena *a, const AvpJson *lock, double now) {
    if (!lock) return "-";
    const char *owner = slot_py_str(a, avp_json_getz(lock, "owner"));
    const char *lane = "";
    const AvpJson *lv = avp_json_getz(lock, "lane");
    if (lv && avp_json_truthy(lv)) {
        if (lv->kind != AVP_JSON_STRING)
            slot_fail(SLOT_UNCAUGHT, "TypeError: can only concatenate str (not \"%s\") to str", py_type(lv));
        lane = avp_sprintf(a, "/%s", avp_cz(a, lv->u.str));
    }
    AvpStr age = avp_pyfmt(a, "%.1f", slot_lock_age_h(lock, now));
    return avp_sprintf(a, "%s%s %.*sh%s", owner, lane, AVP_SF(age), slot_lock_is_stale(lock, now) ? " STALE" : "");
}

bool slot_lock_owner_is(const AvpJson *lock, const char *owner) {
    const AvpJson *v = avp_json_getz(lock, "owner");
    if (!owner) return !v || v->kind == AVP_JSON_NULL;
    return v && v->kind == AVP_JSON_STRING && avp_str_eqz(v->u.str, owner);
}
