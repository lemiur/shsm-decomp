/* rules_check: the Checker's plumbing (check_rules.py L2030-2310): source index, parse memo, include resolution
 * and closures, unit status, add(), run()'s prelude (_set_header_languages, _volatile_notes, offset_macro_names),
 * check_file, Finding.text/to_dict, and the statement and declaration helpers the check modules share
 * (statements L2887, parse_decl L2914, _cast_info L2359). */
#include "rules_check.h"
#include "shsm_text.h"
#include <stdarg.h>
#include <stdio.h>

const char *const RL_SCALAR_WORDS[] = {
    "int", "char", "short", "long", "float", "double", "unsigned", "signed", "bool", "void", "u8", "u16", "u32",
    "u64", "s8", "s16", "s32", "s64", "f32", "f64", "size_t", "BOOL", "uint8_t", "uint16_t", "uint32_t",
    "uint64_t", "int8_t", "int16_t", "int32_t", "int64_t", "vu8", "vu16", "vu32", "vu64", "vs8", "vs16", "vs32",
    "vs64", "vf32", "vf64", "ptrdiff_t", "uintptr_t", "intptr_t", "wchar_t", NULL};
const char *const RL_DECL_KEYWORDS[] = {"static", "register", "const", "volatile", "struct", "union", "enum",
                                        "class", "extern", "auto", "inline", "typename", "mutable", "virtual",
                                        "unsigned", "signed", NULL};
const char *const RL_STMT_KEYWORDS[] = {"return", "goto", "case", "default", "else", "if", "while", "for",
                                        "do", "switch", "break", "continue", "delete", "throw", "typedef", "new",
                                        "sizeof", "using", "asm", "__asm", "template", "namespace", "try",
                                        "catch", NULL};

bool rl_volatile_type(AvpStr s) {
    if (s.n < 3 || s.p[0] != 'v') return false;
    AvpStr rest = avp_str_slice(s, 2, s.n);
    if (s.p[1] == 'u' || s.p[1] == 's')
        return avp_str_eqz(rest, "8") || avp_str_eqz(rest, "16") || avp_str_eqz(rest, "32") || avp_str_eqz(rest, "64");
    if (s.p[1] == 'f') return avp_str_eqz(rest, "32") || avp_str_eqz(rest, "64");
    return false;
}

AvpStr rl_fmt(AvpArena *a, const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *p = avp_alloc(a, (size_t)n + 1);
    vsnprintf(p, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return avp_strn(p, (size_t)n);
}

AvpStr rl_strip_ws(AvpArena *a, AvpStr s) {
    size_t i = 0;
    for (; i < s.n; i++)
        if (shsm_ws_len(s, i)) break;
    if (i == s.n) return s;
    char *p = avp_alloc(a, s.n + 1);
    size_t o = 0;
    for (i = 0; i < s.n;) {
        size_t w = shsm_ws_len(s, i);
        if (w) {
            i += w;
            continue;
        }
        p[o++] = s.p[i++];
    }
    return avp_strn(p, o);
}

const char *rl_tier_of(const char *rule) {
    static const char *const tiers[] = {
        "free",  "match",   "match",   "free",     "match", "evidence", "evidence", "checked", "free",  "free",
        "checked", "free",  "evidence", "match",   "checked", "checked", "checked", "match",  "match", "checked",
        "evidence", "free", "free",     "free",    "free",  "checked",  "checked",  "free",    "checked", "free",
        "checked", "free",  "free",     "free",    "checked", "free",   "free",     "checked"};
    if (!rule || rule[0] != 'R' || !rule[1]) return NULL;
    int v = 0;
    for (const char *p = rule + 1; *p; p++) {
        if (*p < '0' || *p > '9' || v > 100) return NULL;
        v = v * 10 + (*p - '0');
    }
    if ((rule[1] == '0' && rule[2]) || v > 37) return NULL;
    return tiers[v];
}

/* ---- sources and parses ---- */
static const char *const SRC_EXTS[] = {".c", ".cc", ".cp", ".cpp", ".cxx", NULL};
static const char *const HDR_EXTS[] = {".h", ".hh", ".hpp", ".hxx", ".inc", NULL};
static const char *const CPP_EXTS[] = {".cc", ".cp", ".cpp", ".cxx", ".hh", ".hpp", ".hxx", NULL};

static bool ends_ci(AvpArena *a, AvpStr path, const char *const *exts) {
    AvpStr low = avp_str_lower(a, path);
    for (const char *const *e = exts; *e; e++)
        if (avp_str_endswith(low, avp_str(*e))) return true;
    return false;
}

static void add_source(RlChecker *ck, AvpStr path, AvpStr text) {
    avp_map_put(ck->index, path, NULL)->i = (i64)ck->paths.n;
    avp_vec_push(&ck->paths, path);
    avp_vec_push(&ck->texts, text);
}

static RlChecker *finish(RlChecker *ck, const RlConfig *cfg);

RlChecker *rl_checker_new(AvpArena *a, const char *root, const AvpStrVec *extra, const RlConfig *cfg) {
    RlChecker *ck = AVP_NEW(a, RlChecker);
    ck->a = a;
    ck->index = avp_map_new();
    RlSources *S = rl_load_sources(a, root);
    for (size_t i = 0; i < S->paths.n; i++) add_source(ck, S->paths.v[i], S->texts.v[i]);
    for (size_t i = 0; extra && i < extra->n; i++) {
        if (avp_map_has(ck->index, extra->v[i])) continue;
        AvpStr text;
        if (!rl_source_text(a, S, root, extra->v[i], &text)) return NULL;
        add_source(ck, extra->v[i], text);
    }
    return finish(ck, cfg);
}

RlChecker *rl_checker_from(AvpArena *a, const AvpStrVec *paths, const AvpStrVec *texts, const RlConfig *cfg) {
    RlChecker *ck = AVP_NEW(a, RlChecker);
    ck->a = a;
    ck->index = avp_map_new();
    for (size_t i = 0; i < paths->n; i++) {
        AvpVal *v = avp_map_get(ck->index, paths->v[i]);
        if (v) ck->texts.v[v->i] = texts->v[i]; /* a dict: a repeated key keeps its place, takes the new text */
        else add_source(ck, paths->v[i], texts->v[i]);
    }
    return finish(ck, cfg);
}

static RlChecker *finish(RlChecker *ck, const RlConfig *cfg) {
    AvpArena *a = ck->a;
    static const RlConfig none = {0};
    if (!cfg) cfg = &none;
    ck->storage = avp_map_new();
    for (size_t i = 0; i < ck->texts.n; i++) rl_scan_storage_macros(ck->storage, ck->texts.v[i]);
    ck->parsed = avp_alloc(a, sizeof(RlParsed *) * (ck->paths.n + 1));
    ck->arenas = avp_alloc(a, sizeof(AvpArena *) * (ck->paths.n + 1));
    ck->closure_memo = avp_alloc(a, sizeof(AvpStrVec *) * (ck->paths.n + 1));
    ck->cfg = *cfg;
    if (!ck->cfg.unit_status) ck->cfg.unit_status = avp_map_new();
    if (!ck->cfg.include_map) ck->cfg.include_map = avp_map_new();
    if (!ck->cfg.original_asm) ck->cfg.original_asm = avp_set_new();
    if (!ck->cfg.asm_helpers) ck->cfg.asm_helpers = avp_set_new();
    avp_vec_push(&ck->include_dirs, AVP_S("include"));
    return ck;
}

static i64 index_of(RlChecker *ck, AvpStr path) {
    AvpVal *v = avp_map_get(ck->index, path);
    return v ? v->i : -1;
}

RlParsed *rl_checker_parse(RlChecker *ck, AvpStr path) {
    i64 i = index_of(ck, path);
    if (i < 0) return NULL;
    if (!ck->parsed[i]) {
        ck->arenas[i] = avp_arena_new(0);
        ck->parsed[i] = rl_parse(ck->arenas[i], ck->paths.v[i], ck->texts.v[i], ck->storage);
        if (!ck->parsed[i]->ok) ck->failed = true;
    }
    return ck->parsed[i];
}

/* ---- include resolution (L2115-2181) ---- */
static void tu_include_dirs(RlChecker *ck, AvpStr tu, bool library_dir, AvpStrVec *out) {
    if (tu.p && avp_str_startswith(tu, AVP_S("src/"))) {
        AvpVal *v = avp_map_get(ck->cfg.include_map, avp_str_slice(tu, 4, tu.n));
        if (v) {
            const AvpStrVec *d = v->p;
            for (size_t i = 0; i < d->n; i++) avp_vec_push(out, d->v[i]);
            return;
        }
    }
    for (size_t i = 0; i < ck->include_dirs.n; i++) avp_vec_push(out, ck->include_dirs.v[i]);
    AvpStr t = tu.p ? avp_str_replace(ck->a, tu, AVP_S("\\"), AVP_S("/"), -1) : AVP_S("");
    AvpStrVec parts = avp_str_split(t, AVP_S("/"), -1);
    if (library_dir && parts.n > 2 && avp_str_eqz(parts.v[0], "src")) {
        for (size_t i = 0; i < ck->include_dirs.n; i++)
            avp_vec_push(out, rl_fmt(ck->a, "%.*s/%.*s", AVP_SF(ck->include_dirs.v[i]), AVP_SF(parts.v[1])));
    }
    avp_vec_free(&parts);
}

static const AvpStrVec *every_library_dir(RlChecker *ck) {
    if (!ck->every_lib) {
        ck->every_lib = AVP_NEW(ck->a, AvpStrVec);
        AvpMapIter it = {0};
        while (avp_map_next(ck->cfg.include_map, &it)) {
            const AvpStrVec *d = it.val->p;
            for (size_t i = 0; i < d->n; i++) {
                bool seen = false;
                for (size_t k = 0; k < ck->every_lib->n && !seen; k++) seen = avp_str_eq(ck->every_lib->v[k], d->v[i]);
                if (!seen) avp_vec_push(ck->every_lib, d->v[i]);
            }
        }
    }
    return ck->every_lib;
}

static AvpStr resolve_include(RlChecker *ck, AvpStr from, AvpStr name, bool library_dir, AvpStr tu) {
    AvpArena *s = avp_scratch();
    AvpArenaMark mk = avp_arena_mark(s);
    if (!tu.p && avp_str_startswith(from, AVP_S("src/")) && ends_ci(s, from, SRC_EXTS)) tu = from;
    AvpStrVec cands = {0};
    AvpStr d = avp_path_dirname(avp_cz(s, from));
    avp_vec_push(&cands, d.n ? rl_fmt(s, "%.*s/%.*s", AVP_SF(d), AVP_SF(name)) : name);
    AvpStrVec dirs = {0};
    tu_include_dirs(ck, tu, library_dir, &dirs);
    for (size_t i = 0; i < dirs.n; i++) avp_vec_push(&cands, rl_fmt(s, "%.*s/%.*s", AVP_SF(dirs.v[i]), AVP_SF(name)));
    for (size_t i = 0; i < ck->include_dirs.n; i++)
        avp_vec_push(&cands, rl_fmt(s, "%.*s/%.*s", AVP_SF(ck->include_dirs.v[i]), AVP_SF(name)));
    if (!tu.p) {
        const AvpStrVec *lib = every_library_dir(ck);
        for (size_t i = 0; i < lib->n; i++) avp_vec_push(&cands, rl_fmt(s, "%.*s/%.*s", AVP_SF(lib->v[i]), AVP_SF(name)));
    }
    avp_vec_push(&cands, rl_fmt(s, "src/%.*s", AVP_SF(name)));
    AvpStr found = {0};
    for (size_t i = 0; i < cands.n && !found.p; i++) {
        char *np = avp_path_normpath(s, avp_cz(s, cands.v[i]));
        AvpStr c = avp_str_replace(s, avp_str(np), AVP_S("\\"), AVP_S("/"), -1);
        i64 ix = index_of(ck, c);
        if (ix >= 0) found = ck->paths.v[ix];
    }
    avp_vec_free(&cands);
    avp_vec_free(&dirs);
    avp_arena_reset(s, mk);
    return found;
}

static void closure_rec(RlChecker *ck, AvpStr path, AvpSet *seen, AvpStr tu) {
    if (avp_set_has(seen, path)) return;
    avp_set_add(seen, path);
    if (!tu.p && avp_str_startswith(path, AVP_S("src/")) && ends_ci(ck->a, path, SRC_EXTS)) tu = path;
    RlParsed *P = rl_checker_parse(ck, path);
    for (size_t i = 0; i < P->includes.n; i++) {
        AvpStr r = resolve_include(ck, path, P->includes.v[i], true, tu);
        if (r.p) closure_rec(ck, r, seen, tu);
    }
}

const AvpStrVec *rl_checker_closure(RlChecker *ck, AvpStr path) {
    i64 i = index_of(ck, path);
    AVP_ASSERT(i >= 0);
    if (!ck->closure_memo[i]) {
        AvpSet *seen = avp_set_new();
        closure_rec(ck, ck->paths.v[i], seen, (AvpStr){0});
        AvpStrVec sorted = avp_set_sorted(seen);
        AvpStrVec *v = AVP_NEW(ck->a, AvpStrVec);
        for (size_t k = 0; k < sorted.n; k++) avp_vec_push(v, ck->paths.v[index_of(ck, sorted.v[k])]);
        avp_vec_free(&sorted);
        avp_set_free(seen);
        ck->closure_memo[i] = v;
    }
    return ck->closure_memo[i];
}

AvpStr rl_checker_status_of(RlChecker *ck, AvpStr path) {
    AvpVal *v = NULL;
    if (avp_str_startswith(path, AVP_S("src/"))) v = avp_map_get(ck->cfg.unit_status, avp_str_slice(path, 4, path.n));
    if (!v) v = avp_map_get(ck->cfg.unit_status, path);
    return v ? *(AvpStr *)v->p : AVP_S("Unlisted");
}

/* ---- run()'s prelude ---- */
void rl_checker_prepare(RlChecker *ck) {
    /* _set_header_languages */
    AvpMap *only_cpp = avp_map_new();
    for (size_t i = 0; i < ck->paths.n; i++) {
        AvpStr path = ck->paths.v[i];
        if (!avp_str_startswith(path, AVP_S("src/")) || !ends_ci(ck->a, path, SRC_EXTS)) continue;
        bool cpp = ends_ci(ck->a, path, CPP_EXTS);
        const AvpStrVec *cl = rl_checker_closure(ck, path);
        for (size_t k = 0; k < cl->n; k++) {
            AvpStr h = cl->v[k];
            if (avp_str_eq(h, path) || !ends_ci(ck->a, h, HDR_EXTS)) continue;
            bool created;
            AvpVal *v = avp_map_put(only_cpp, h, &created);
            v->i = (created ? 1 : v->i) && cpp;
        }
    }
    for (size_t i = 0; i < ck->paths.n; i++) {
        AvpStr path = ck->paths.v[i];
        if (!ends_ci(ck->a, path, HDR_EXTS) || ends_ci(ck->a, path, CPP_EXTS)) continue;
        RlParsed *P = rl_checker_parse(ck, path);
        bool v = rl_cpp_syntax(P);
        if (!v) {
            AvpVal *o = avp_map_get(only_cpp, path);
            v = o ? o->i != 0
                  : (avp_str_startswith(path, AVP_S("include/game/")) || avp_str_startswith(path, AVP_S("include/climax/")));
        }
        P->is_cpp = v;
    }
    avp_map_free(only_cpp);
    /* _volatile_notes */
    ck->noted_volatile = avp_set_new();
    for (size_t i = 0; i < ck->paths.n; i++) {
        AvpSet *s = rl_volatile_noted(rl_checker_parse(ck, ck->paths.v[i]));
        size_t pos = 0;
        AvpStr m;
        while (avp_set_next(s, &pos, &m)) avp_set_add(ck->noted_volatile, m);
    }
    /* offset_macro_names (memoized on first use in the Python; computed here, it reads only parses) */
    ck->offset_names = avp_set_new();
    for (size_t i = 0; i < ck->paths.n; i++) {
        AvpSet *s = rl_offset_macros(rl_checker_parse(ck, ck->paths.v[i]));
        size_t pos = 0;
        AvpStr m;
        while (avp_set_next(s, &pos, &m)) avp_set_add(ck->offset_names, m);
    }
}

/* ---- findings ---- */
i64 rl_add(RlChecker *ck, RlParsed *P, i32 line, const char *rule, const char *tier, AvpStr msg, AvpStr func) {
    i64 fi = rl_func_at(P, line);
    const RlFunc *fn = fi >= 0 ? &P->p.funcs.v[fi] : NULL;
    if (!func.p) func = fn ? fn->name : (AvpStr){0};
    if (!strcmp(rule, "R17") && fn && rl_no_copy_in_retail(P, fn)) return -1;
    RlFinding *f = RL_PUSH(ck->a, &ck->findings);
    f->path = P->path;
    f->line = line;
    f->rule = rule;
    f->tier = tier;
    f->msg = msg;
    f->func = func;
    f->status = "violation";
    f->reason = AVP_S("");
    return (i64)(ck->findings.n - 1);
}

AvpStr rl_finding_text(AvpArena *a, const RlFinding *f) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_printf(&b, "%.*s:%d: %s [%s] %.*s", AVP_SF(f->path), f->line, f->rule, f->tier, AVP_SF(f->msg));
    if (f->func.p && f->func.n) avp_buf_printf(&b, " (in %.*s)", AVP_SF(f->func));
    if (!strcmp(f->status, "excepted")) avp_buf_printf(&b, " -- RULE-EXCEPTION: %.*s", AVP_SF(f->reason));
    else if (!strcmp(f->status, "allowed")) avp_buf_printf(&b, " -- allowed: %.*s", AVP_SF(f->reason));
    return avp_strn(b.p, b.n);
}

void rl_finding_json(AvpJsonW *w, const RlFinding *f) {
    avp_jsonw_begin_object(w);
    avp_jsonw_kv_str(w, "path", f->path);
    avp_jsonw_key(w, AVP_S("line"));
    avp_jsonw_i64(w, f->line);
    avp_jsonw_kv_str(w, "rule", avp_str(f->rule));
    avp_jsonw_kv_str(w, "tier", avp_str(f->tier));
    avp_jsonw_kv_str(w, "message", f->msg);
    avp_jsonw_key(w, AVP_S("function"));
    if (f->func.p) avp_jsonw_str(w, f->func);
    else avp_jsonw_null(w);
    avp_jsonw_kv_str(w, "status", avp_str(f->status));
    avp_jsonw_kv_str(w, "reason", f->reason);
    avp_jsonw_end_object(w);
}

void rl_finding_json_full(AvpJsonW *w, const RlFinding *f) {
    avp_jsonw_begin_object(w);
    avp_jsonw_kv_str(w, "path", f->path);
    avp_jsonw_key(w, AVP_S("line"));
    avp_jsonw_i64(w, f->line);
    avp_jsonw_kv_str(w, "rule", avp_str(f->rule));
    avp_jsonw_kv_str(w, "tier", avp_str(f->tier));
    avp_jsonw_kv_str(w, "message", f->msg);
    avp_jsonw_key(w, AVP_S("function"));
    if (f->func.p) avp_jsonw_str(w, f->func);
    else avp_jsonw_null(w);
    avp_jsonw_kv_str(w, "status", avp_str(f->status));
    avp_jsonw_kv_str(w, "reason", f->reason);
    avp_jsonw_key(w, AVP_S("units"));
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < f->nunits; i++) avp_jsonw_str(w, f->units[i]);
    avp_jsonw_end_array(w);
    avp_jsonw_end_object(w);
}

/* ---- check_file (L2298) ---- */
void rl_check_file(RlChecker *ck, RlParsed *P) {
    ck->P = P;
    if (ck->ph) avp_map_free(ck->ph);
    ck->ph = avp_map_new();
    ck->ph_v.n = 0;
    if (ck->ptr_names) avp_map_free(ck->ptr_names);
    ck->ptr_names = avp_map_new();
    if (ck->macro_bodies) avp_map_free(ck->macro_bodies);
    ck->macro_bodies = NULL;
    rl_check_tokens(ck, P);
    rl_check_directives(ck, P);
    rl_flush_identifiers(ck, P);
    rl_check_comments(ck, P);
    rl_check_asm(ck, P);
    rl_check_declarations(ck, P);
    for (size_t i = 0; i < P->p.funcs.n; i++) rl_check_function(ck, P, &P->p.funcs.v[i]);
    rl_check_function_meta(ck, P);
}

/* ---- shared helpers ---- */
size_t rl_statements(const RlTok *toks, const i32 *match, i32 lo, i32 hi, RlRange **out) {
    RL_VEC(RlRange) v = {0};
    AvpArena *s = avp_scratch();
    i32 start = lo, i = lo;
    while (i < hi) {
        const RlTok *t = &toks[i];
        if ((RL_TEQ(t, "(") || RL_TEQ(t, "[")) && match[i] >= 0) {
            i = match[i] + 1;
            continue;
        }
        if (RL_TEQ(t, "{") && i > start && RL_TEQ(&toks[i - 1], "=") && match[i] >= 0) {
            i = match[i] + 1;
            continue;
        }
        if (RL_TEQ(t, ";") || RL_TEQ(t, "{") || RL_TEQ(t, "}")) {
            if (i > start) *RL_PUSH(s, &v) = (RlRange){start, i};
            start = ++i;
            continue;
        }
        i++;
    }
    if (i > start) *RL_PUSH(s, &v) = (RlRange){start, i};
    *out = v.v;
    return v.n;
}

static const char *const QUALS[] = {"static", "register", "const", "volatile", "extern", "auto", "mutable", NULL};

size_t rl_parse_decl(AvpArena *a, const RlTok *st, size_t n, RlDecl **out) {
    size_t k = 0;
    size_t q0 = 0;
    while (k < n && st[k].k == RL_ID && rl_tin(&st[k], QUALS)) k++;
    size_t nq = k;
    if (k >= n || st[k].k != RL_ID || rl_tin(&st[k], RL_STMT_KEYWORDS)) return 0;
    size_t w0 = k;
    RL_VEC(size_t) words = {0};
    AvpArena *s = avp_scratch();
    while (k < n && (st[k].k == RL_ID || RL_TEQ(&st[k], "::"))) {
        if (RL_TEQ(&st[k], "::")) {
            k++;
            continue;
        }
        if (rl_tin(&st[k], RL_STMT_KEYWORDS)) return 0;
        *RL_PUSH(s, &words) = k;
        k++;
    }
    (void)w0;
    size_t ptrs = 0;
    while (k < n && (RL_TEQ(&st[k], "*") || RL_TEQ(&st[k], "&") ||
                     (st[k].k == RL_ID && (RL_TEQ(&st[k], "const") || RL_TEQ(&st[k], "volatile"))))) {
        if (RL_TEQ(&st[k], "*") || RL_TEQ(&st[k], "&")) ptrs++;
        k++;
    }
    i64 name = -1;
    size_t ntype = words.n;
    if (ptrs > 0) {
        if (k < n && st[k].k == RL_ID && words.n >= 1) name = (i64)k++;
    } else if (words.n >= 2 && !rl_tin(&st[words.v[words.n - 1]], RL_DECL_KEYWORDS)) {
        name = (i64)words.v[words.n - 1];
        ntype = words.n - 1;
    }
    if (name < 0) return 0;
    if (k < n && !(RL_TEQ(&st[k], "=") || RL_TEQ(&st[k], "[") || RL_TEQ(&st[k], ",") || RL_TEQ(&st[k], "(") ||
                   RL_TEQ(&st[k], ":") || RL_TEQ(&st[k], ";"))) {
        if (!(words.n && (size_t)name == words.v[words.n - 1])) return 0;
    }
    /* the shared type prefix: quals + type words */
    AvpBuf b;
    avp_buf_init(&b, a);
    bool any = false;
    for (size_t i = q0; i < nq; i++) {
        if (any) avp_buf_putc(&b, ' ');
        avp_buf_put(&b, st[i].t);
        any = true;
    }
    for (size_t i = 0; i < ntype; i++) {
        if (any) avp_buf_putc(&b, ' ');
        avp_buf_put(&b, st[words.v[i]].t);
        any = true;
    }
    AvpStr prefix = avp_strn(b.p, b.n);
    RL_VEC(RlDecl) res = {0};
#define STARS(cnt)                                                                  \
    do {                                                                            \
        AvpBuf tb;                                                                  \
        avp_buf_init(&tb, a);                                                       \
        avp_buf_put(&tb, prefix);                                                   \
        bool an = prefix.n > 0 || any;                                              \
        for (size_t z = 0; z < (cnt); z++) {                                        \
            if (an) avp_buf_putc(&tb, ' ');                                         \
            avp_buf_putc(&tb, '*');                                                 \
            an = true;                                                              \
        }                                                                           \
        d->type = avp_strn(tb.p, tb.n);                                             \
    } while (0)
    RlDecl *d = RL_PUSH(s, &res);
    d->name = st[name].t;
    d->tok = (i32)name;
    STARS(ptrs);
    i64 depth = 0;
    for (size_t m = k; m < n; m++) {
        const RlTok *x = &st[m];
        if (x->t.n == 1 && strchr("([{", x->t.p[0])) depth++;
        else if (x->t.n == 1 && strchr(")]}", x->t.p[0])) depth--;
        else if (RL_TEQ(x, ",") && depth == 0) {
            size_t q = m + 1, p2 = 0;
            while (q < n && (RL_TEQ(&st[q], "*") || RL_TEQ(&st[q], "&"))) {
                p2++;
                q++;
            }
            if (q < n && st[q].k == RL_ID) {
                d = RL_PUSH(s, &res);
                d->name = st[q].t;
                d->tok = (i32)q;
                STARS(p2);
            }
        }
    }
#undef STARS
    *out = res.v;
    return res.n;
}

static const char *const CAST_PREV_OK[] = {"return", "case", "else", "do", "throw", "new", "delete", "and", "or", NULL};

i32 rl_cast_info(const RlTok *toks, size_t n, const i32 *match, size_t i, bool chained) {
    if (!RL_TEQ(&toks[i], "(") || match[i] < 0) return -1;
    size_t c = (size_t)match[i];
    if (c <= i + 1) return -1;
    if (i > 0) {
        const RlTok *p = &toks[i - 1];
        if (p->k == RL_ID && !rl_tin(p, CAST_PREV_OK)) return -1;
        if (((RL_TEQ(p, ")") || RL_TEQ(p, "]")) && !chained) || p->k == RL_NUM || p->k == RL_STR || p->k == RL_CHR)
            return -1;
    }
    if (c + 1 >= n) return -1;
    const RlTok *nx = &toks[c + 1];
    static const char *const NEXT_OK[] = {"(", "*", "&", "-", "+", "~", "!", "++", "--", NULL};
    if (!(nx->k == RL_ID || nx->k == RL_NUM || nx->k == RL_STR || nx->k == RL_CHR || rl_tin(nx, NEXT_OK))) return -1;
    bool ids = false;
    for (size_t k = i + 1; k < c; k++) {
        const RlTok *x = &toks[k];
        if (x->k != RL_ID && x->k != RL_OP) return -1;
    }
    for (size_t k = i + 1; k < c; k++) {
        const RlTok *x = &toks[k];
        if (!(x->k == RL_ID || RL_TEQ(x, "*") || RL_TEQ(x, "&") || RL_TEQ(x, "::"))) return -1;
        if (x->k == RL_ID) ids = true;
    }
    if (!ids) return -1;
    const RlTok *last = &toks[c - 1];
    if (RL_TEQ(last, "*") || RL_TEQ(last, "&")) return (i32)c;
    static const char *const EXTRA[] = {"const", "volatile", "struct", "union", "enum", NULL};
    for (size_t k = i + 1; k < c; k++)
        if (toks[k].k == RL_ID && !rl_tin(&toks[k], RL_SCALAR_WORDS) && !rl_tin(&toks[k], EXTRA)) return -1;
    return (i32)c;
}

/* ---- exported for the stage-3 phases ---- */
AvpStr rl_resolve_include(RlChecker *ck, AvpStr from, AvpStr name, bool library_dir, AvpStr tu) {
    return resolve_include(ck, from, name, library_dir, tu);
}

bool rl_is_source_path(AvpArena *a, AvpStr path) {
    return ends_ci(a, path, SRC_EXTS);
}
