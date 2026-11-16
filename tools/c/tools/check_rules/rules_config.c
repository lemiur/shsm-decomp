/* rules_config: what check_rules' main() reads besides the sources: configure.py's unit table (parse_units) and
 * include directories (parse_include_dirs, _py_tokens, _match_bracket), and the asm allow lists of
 * docs/exceptions.md (Allow.from_markdown, _strip_main). check_rules.py L1891-1934, L4422-4617, L4752-4770. */
#include "rules_check.h"
#include "shsm_text.h"

/* ---- parse_units ---- */
void rl_parse_units(AvpArena *a, RlConfig *cfg, AvpStr text) {
    if (!cfg->unit_status) cfg->unit_status = avp_map_new();
    AvpBuf b;
    avp_buf_init(&b, a);
    AvpStrVec lines = avp_str_split(text, AVP_S("\n"), -1);
    bool first = true;
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr s = avp_str_lstrip(lines.v[i]);
        if (s.n && s.p[0] == '#') continue;
        if (!first) avp_buf_putc(&b, '\n');
        avp_buf_put(&b, lines.v[i]);
        first = false;
    }
    avp_vec_free(&lines);
    AvpStr joined = avp_strn(b.p, b.n);
    const ShsmRe *re = shsm_re("Object\\(\\s*(Matching|NonMatching|Equivalent)\\s*,\\s*\"([^\"]+)\"", 0);
    ShsmReIter it;
    ShsmReMatch m;
    shsm_re_iter_init(&it, re, joined, 0, AVP_END);
    while (shsm_re_iter_next(&it, &m)) {
        AvpStr *v = AVP_NEW(a, AvpStr);
        *v = shsm_re_group(&m, 1);
        avp_map_put(cfg->unit_status, shsm_re_group(&m, 2), NULL)->p = v;
    }
}

/* ---- _py_tokens ---- */
typedef enum { PT_STR, PT_NAME, PT_OP } PtKind;
typedef struct {
    PtKind k;
    AvpStr v;
} PyTok;
typedef RL_VEC(PyTok) PyToks;

static bool py_alpha(unsigned char c) { return (c | 32) >= 'a' && (c | 32) <= 'z'; }
static bool py_alnum(unsigned char c) { return py_alpha(c) || (c >= '0' && c <= '9'); }

static PyToks py_tokens(AvpArena *a, AvpStr text) {
    PyToks toks = {0};
    size_t i = 0, n = text.n;
    const char *s = text.p;
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        if (c == '#') {
            while (i < n && s[i] != '\n') i++;
        } else if (c == '"' || c == '\'') {
            size_t ql = (i + 3 <= n && s[i + 1] == (char)c && s[i + 2] == (char)c) ? 3 : 1;
            size_t j = i + ql;
            AvpBuf buf;
            avp_buf_init(&buf, a);
            while (j < n && !(j + ql <= n && memcmp(s + j, s + i, ql) == 0)) {
                if (s[j] == '\\' && j + 1 < n) j++;
                avp_buf_putc(&buf, s[j]);
                j++;
            }
            PyTok *t = RL_PUSH(a, &toks);
            t->k = PT_STR;
            t->v = avp_strn(buf.p, buf.n);
            i = j + ql;
        } else if (py_alpha(c) || c == '_') {
            size_t j = i;
            while (j < n && (py_alnum((unsigned char)s[j]) || s[j] == '_')) j++;
            AvpStr w = avp_strn(s + i, j - i);
            static const char *const prefixes[] = {"f", "r", "b", "u", "rb", "br", "fr", "rf", NULL};
            AvpStr low = avp_str_lower(a, w);
            if (rl_sin(low, prefixes) && j < n && (s[j] == '"' || s[j] == '\'')) {
                i = j;
                continue;
            }
            PyTok *t = RL_PUSH(a, &toks);
            t->k = PT_NAME;
            t->v = w;
            i = j;
        } else if (shsm_ws_len(text, i)) {
            i += shsm_ws_len(text, i);
        } else {
            size_t cl = 1; /* one code point */
            if (c >= 0x80) {
                const char *p = s + i;
                avp_u8_next(&p, s + n);
                cl = (size_t)(p - (s + i));
            }
            PyTok *t = RL_PUSH(a, &toks);
            t->k = PT_OP;
            t->v = avp_strn(s + i, cl);
            i += cl;
        }
    }
    return toks;
}

static bool pt_is(const PyTok *t, PtKind k, const char *v) { return t->k == k && avp_str_eqz(t->v, v); }
static bool pt_op_in(const PyTok *t, const char *set) {
    return t->k == PT_OP && t->v.n == 1 && strchr(set, t->v.p[0]) != NULL;
}

static size_t match_bracket(const PyToks *toks, size_t i) {
    i64 depth = 0;
    for (size_t j = i; j < toks->n; j++) {
        if (toks->v[j].k == PT_OP) {
            if (pt_op_in(&toks->v[j], "([{")) depth++;
            else if (pt_op_in(&toks->v[j], ")]}")) {
                if (--depth == 0) return j;
            }
        }
    }
    return toks->n - 1;
}

typedef struct {
    bool spread;
    AvpStr v;
} FlagItem;
typedef RL_VEC(FlagItem) FlagItems;

static FlagItems list_items(AvpArena *a, const PyToks *toks, size_t i) {
    FlagItems out = {0};
    size_t end = match_bracket(toks, i);
    size_t j = i + 1;
    while (j < end) {
        const PyTok *t = &toks->v[j];
        if (pt_op_in(t, "([{")) {
            j = match_bracket(toks, j) + 1;
            continue;
        }
        if (t->k == PT_STR) {
            FlagItem *f = RL_PUSH(a, &out);
            f->v = t->v;
        } else if (pt_is(t, PT_OP, "*") && j + 1 < end && toks->v[j + 1].k == PT_NAME) {
            FlagItem *f = RL_PUSH(a, &out);
            f->spread = true;
            f->v = toks->v[j + 1].v;
            j++;
        }
        j++;
    }
    return out;
}

static FlagItems value_items(AvpArena *a, const PyToks *toks, size_t j) {
    FlagItems out = {0};
    if (j >= toks->n) return out;
    if (pt_is(&toks->v[j], PT_OP, "[")) return list_items(a, toks, j);
    if (toks->v[j].k == PT_NAME) {
        FlagItem *f = RL_PUSH(a, &out);
        f->spread = true;
        f->v = toks->v[j].v;
    }
    return out;
}

static bool vec_has(const AvpStrVec *v, AvpStr s) {
    for (size_t i = 0; i < v->n; i++)
        if (avp_str_eq(v->v[i], s)) return true;
    return false;
}

typedef struct {
    AvpArena *a;
    AvpMap *lists; /* name -> FlagItems* */
} Resolver;

static void resolve(Resolver *R, const FlagItems *items, AvpStrVec *seen, AvpStrVec *dirs) {
    const ShsmRe *re = shsm_re("-i\\s+(\\S+)\\s*$", 0);
    for (size_t i = 0; i < items->n; i++) {
        const FlagItem *it = &items->v[i];
        if (!it->spread) {
            ShsmReMatch m;
            if (shsm_re_match(re, it->v, 0, AVP_END, &m)) {
                AvpStr d = avp_str_replace(R->a, shsm_re_group(&m, 1), AVP_S("\\"), AVP_S("/"), -1);
                if (!avp_str_contains(d, AVP_S("{")) && !avp_str_startswith(d, AVP_S("build/")) && !vec_has(dirs, d))
                    avp_vec_push(dirs, d);
            }
        } else {
            AvpVal *l = avp_map_get(R->lists, it->v);
            if (l && !vec_has(seen, it->v)) {
                avp_vec_push(seen, it->v);
                AvpStrVec sub = {0};
                resolve(R, (const FlagItems *)l->p, seen, &sub);
                seen->n--;
                for (size_t k = 0; k < sub.n; k++)
                    if (!vec_has(dirs, sub.v[k])) avp_vec_push(dirs, sub.v[k]);
                avp_vec_free(&sub);
            }
        }
    }
}

void rl_parse_include_dirs(AvpArena *a, RlConfig *cfg, AvpStr text) {
    if (!cfg->include_map) cfg->include_map = avp_map_new();
    PyToks T = py_tokens(a, text);
    const PyToks *toks = &T;
    Resolver R = {a, avp_map_new()};
    AvpMap *helpers = avp_map_new(); /* name -> FlagItems* */
    i64 depth = 0;
    i64 libs_open = -1;
    for (size_t i = 0; i < T.n; i++) {
        const PyTok *t = &T.v[i];
        if (pt_op_in(t, "([{")) depth++;
        else if (pt_op_in(t, ")]}")) depth--;
        if (depth != 0 || t->k != PT_NAME) continue;
        if (i + 2 < T.n && pt_is(&T.v[i + 1], PT_OP, "=") && pt_is(&T.v[i + 2], PT_OP, "[")) {
            if (i > 0 && pt_is(&T.v[i - 1], PT_OP, ".")) {
                if (avp_str_eqz(t->v, "libs")) libs_open = (i64)i + 2;
            } else {
                FlagItems *li = AVP_NEW(a, FlagItems);
                *li = list_items(a, toks, i + 2);
                avp_map_put(R.lists, t->v, NULL)->p = li;
            }
        } else if (avp_str_eqz(t->v, "def") && i + 1 < T.n && T.v[i + 1].k == PT_NAME) {
            size_t j = i + 2;
            while (j < T.n && !pt_is(&T.v[j], PT_NAME, "return") && !pt_is(&T.v[j], PT_NAME, "def")) j++;
            if (j + 1 < T.n && pt_is(&T.v[j], PT_NAME, "return") && pt_is(&T.v[j + 1], PT_OP, "{")) {
                size_t end = match_bracket(toks, j + 1);
                for (size_t k = j + 2; k < end; k++) {
                    if (pt_is(&T.v[k], PT_STR, "cflags") && pt_is(&T.v[k + 1], PT_OP, ":")) {
                        FlagItems *fi = AVP_NEW(a, FlagItems);
                        *fi = value_items(a, toks, k + 2);
                        avp_map_put(helpers, T.v[i + 1].v, NULL)->p = fi;
                        break;
                    }
                }
            }
        }
    }
    if (libs_open >= 0) {
        size_t libs_end = match_bracket(toks, (size_t)libs_open);
        size_t j = (size_t)libs_open + 1;
        while (j < libs_end) {
            size_t start = j;
            while (j < libs_end && !pt_op_in(&T.v[j], "({")) j++;
            if (j >= libs_end) break;
            size_t end = match_bracket(toks, j);
            FlagItems flags = {0};
            if (pt_is(&T.v[j], PT_OP, "(") && j > start && T.v[j - 1].k == PT_NAME) {
                AvpVal *h = avp_map_get(helpers, T.v[j - 1].v);
                if (h) flags = *(FlagItems *)h->p;
            } else {
                for (size_t m = j; m < end; m++) {
                    if (pt_is(&T.v[m], PT_STR, "cflags") && pt_is(&T.v[m + 1], PT_OP, ":")) {
                        flags = value_items(a, toks, m + 2);
                        break;
                    }
                }
            }
            AvpStrVec seen = {0}, base = {0};
            resolve(&R, &flags, &seen, &base);
            for (size_t m = j; m < end; m++) {
                if (pt_is(&T.v[m], PT_NAME, "Object") && pt_is(&T.v[m + 1], PT_OP, "(")) {
                    size_t oend = match_bracket(toks, m + 1);
                    AvpStr path = {0};
                    for (size_t q = m + 2; q < oend; q++)
                        if (T.v[q].k == PT_STR) {
                            path = T.v[q].v;
                            break;
                        }
                    FlagItems extra = {0};
                    for (size_t q = m + 2; q < oend; q++) {
                        if (pt_is(&T.v[q], PT_NAME, "extra_cflags") && pt_is(&T.v[q + 1], PT_OP, "=")) {
                            extra = value_items(a, toks, q + 2);
                            break;
                        }
                    }
                    if (path.p && path.n) {
                        AvpStrVec *dirs = AVP_NEW(a, AvpStrVec);
                        for (size_t q = 0; q < base.n; q++) avp_vec_push(dirs, base.v[q]);
                        AvpStrVec ex = {0};
                        resolve(&R, &extra, &seen, &ex);
                        for (size_t q = 0; q < ex.n; q++)
                            if (!vec_has(dirs, ex.v[q])) avp_vec_push(dirs, ex.v[q]);
                        avp_vec_free(&ex);
                        bool created;
                        AvpVal *slot = avp_map_put(cfg->include_map, path, &created);
                        slot->p = dirs; /* out[path] = dirs: a later Object of the same path wins */
                    }
                    m = oend;
                }
            }
            avp_vec_free(&seen);
            avp_vec_free(&base);
            j = end + 1;
        }
    }
    avp_map_free(helpers);
    avp_map_free(R.lists);
}

/* ---- Allow.from_markdown ---- */
static AvpStr strip_main(AvpStr u) { return avp_str_startswith(u, AVP_S("main/")) ? avp_str_slice(u, 5, u.n) : u; }

static AvpStr pair_key(AvpArena *a, AvpStr unit, AvpStr fn) { return rl_fmt(a, "%.*s\x01%.*s", AVP_SF(unit), AVP_SF(fn)); }

void rl_allow_from_markdown(AvpArena *a, RlConfig *cfg, AvpStr text, AvpStr source) {
    if (!cfg->original_asm) cfg->original_asm = avp_set_new();
    if (!cfg->asm_helpers) cfg->asm_helpers = avp_set_new();
    if (!cfg->rulings) cfg->rulings = avp_set_new();
    if (!cfg->pending) cfg->pending = avp_set_new();
    cfg->allow_source = source;
    const ShsmRe *rnum = shsm_re("R\\d+", 0);
    const ShsmRe *tick = shsm_re("`([^`]+)`", 0);
    const ShsmRe *call = shsm_re("(\\w+)\\s*\\(", 0);
    AvpStr section = {0};
    AvpStrVec lines = avp_str_split(text, AVP_S("\n"), -1);
    for (size_t li = 0; li < lines.n; li++) {
        AvpStr line = lines.v[li];
        if (avp_str_startswith(line, AVP_S("#"))) {
            section = avp_str_strip(avp_str_lstrip_chars(line, AVP_S("#")));
            continue;
        }
        if (!avp_str_startswith(line, AVP_S("|"))) continue;
        AvpStr rest = avp_str_strip(avp_str_replace(a, line, AVP_S("|"), AVP_S(""), -1));
        bool only = true;
        for (size_t k = 0; k < rest.n; k++)
            if (rest.p[k] != '-' && rest.p[k] != ':' && rest.p[k] != ' ') only = false;
        if (only) continue;
        AvpStrVec cells = avp_str_split(avp_str_strip_chars(avp_str_strip(line), AVP_S("|")), AVP_S("|"), -1);
        for (size_t k = 0; k < cells.n; k++) cells.v[k] = avp_str_strip(cells.v[k]);
        /* ticks of the first three cells (the only ones read) */
        AvpStrVec ticks[3] = {{0}, {0}, {0}};
        for (size_t k = 0; k < cells.n && k < 3; k++) {
            ShsmReIter it;
            ShsmReMatch m;
            shsm_re_iter_init(&it, tick, cells.v[k], 0, AVP_END);
            while (shsm_re_iter_next(&it, &m)) avp_vec_push(&ticks[k], shsm_re_group(&m, 1));
        }
        if ((avp_str_startswith(section, AVP_S("ORIGINAL-ASM")) ||
             avp_str_startswith(section, AVP_S("Header-level asm inlines"))) &&
            cells.n >= 2 && ticks[0].n && ticks[1].n) {
            /* a header-level asm inline is listed by its header path (`include/game/VectorPS.h`) in the table
             * "Header-level asm inlines (ORIGINAL-ASM)" (not a configure.py unit, so the ledger does not read it):
             * keyed as rl_unit_key keys the header, without the extension */
            AvpStr u = strip_main(ticks[0].v[0]);
            if (avp_str_endswith(u, AVP_S(".h"))) u = avp_str_slice(u, 0, u.n - 2);
            else if (avp_str_endswith(u, AVP_S(".hpp"))) u = avp_str_slice(u, 0, u.n - 4);
            for (size_t k = 0; k < ticks[1].n; k++)
                avp_set_add(cfg->original_asm, pair_key(a, u, ticks[1].v[k]));
        } else if (avp_str_startswith(section, AVP_S("Listed inline-asm helpers")) && cells.n >= 3 && ticks[0].n &&
                   ticks[1].n) {
            avp_set_add(cfg->asm_helpers, pair_key(a, strip_main(ticks[0].v[0]), ticks[1].v[0]));
            ShsmReMatch m;
            if (ticks[2].n && shsm_re_search(call, ticks[2].v[0], 0, AVP_END, &m))
                avp_set_add(cfg->asm_helpers, pair_key(a, strip_main(ticks[0].v[0]), shsm_re_group(&m, 1)));
        }
        else if (avp_str_startswith(section, AVP_S("Review rulings")) && cells.n >= 2) {
            ShsmReIter it;
            ShsmReMatch m;
            shsm_re_iter_init(&it, rnum, cells.v[1], 0, AVP_END);
            AvpStrVec rules = {0};
            while (shsm_re_iter_next(&it, &m)) avp_vec_push(&rules, shsm_re_group(&m, 0));
            for (size_t f = 0; f < ticks[0].n; f++)
                for (size_t r = 0; r < rules.n; r++) avp_set_add(cfg->rulings, pair_key(a, rules.v[r], ticks[0].v[f]));
            avp_vec_free(&rules);
        } else if (avp_str_startswith(section, AVP_S("MATCH-PENDING")) && cells.n && ticks[0].n) {
            for (size_t f = 0; f < ticks[0].n; f++) avp_set_add(cfg->pending, ticks[0].v[f]);
        }
        for (int k = 0; k < 3; k++) avp_vec_free(&ticks[k]);
        avp_vec_free(&cells);
    }
    avp_vec_free(&lines);
}

static bool read_file(AvpArena *a, const char *path, AvpStr *out) {
    if (!avp_isfile(path)) return false;
    return rl_read_source(a, path, out);
}

void rl_config_load(AvpArena *a, RlConfig *cfg, const char *root, const char *configure, const char *allow) {
    memset(cfg, 0, sizeof *cfg);
    cfg->unit_status = avp_map_new();
    cfg->include_map = avp_map_new();
    cfg->original_asm = avp_set_new();
    cfg->asm_helpers = avp_set_new();
    cfg->rulings = avp_set_new();
    cfg->pending = avp_set_new();
    const char *cpath = configure ? configure : avp_path_join(a, root, "configure.py");
    AvpStr text;
    if (read_file(a, cpath, &text)) {
        rl_parse_units(a, cfg, text);
        rl_parse_include_dirs(a, cfg, text);
    }
    const char *ex = allow ? allow : avp_path_join(a, avp_path_join(a, root, "docs"), "exceptions.md");
    if (read_file(a, ex, &text)) {
        char *rel = NULL;
        if (!avp_path_relpath(a, ex, root, &rel)) rel = (char *)ex;
        rl_allow_from_markdown(a, cfg, text, avp_str_replace(a, avp_str(rel), AVP_S("\\"), AVP_S("/"), -1));
    }
}
