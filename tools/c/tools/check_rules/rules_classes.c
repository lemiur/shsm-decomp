/* rules_classes: the C++ class model and classes.json (check_rules.py L1366-1853). See rules_classes.h. */
#include "rules_classes.h"
#include "rules_check.h"

#include "classes.h"

#define TEQ(tok, lit) RL_TEQ(tok, lit)
static const AvpStr NONE = {0};

/* ---- mangled names (L1376-1424) ---- */
bool rl_demangle_parts(AvpArena *a, AvpStr sym, AvpStr *cls, AvpStr *name, AvpStr *rest) {
    (void)a;
    static const ShsmRe *thunk, *member;
    if (!thunk) {
        thunk = shsm_re("@\\d+@(.*)", 0);
        member = shsm_re("(__dt|__ct|[A-Za-z_]\\w*?)__(?=Q\\d|\\d)", 0);
    }
    ShsmReMatch m;
    if (shsm_re_match(thunk, sym, 0, AVP_END, &m)) sym = shsm_re_group(&m, 1);
    *cls = NONE;
    *rest = NONE;
    *name = sym;
    if (!shsm_re_match(member, sym, 0, AVP_END, &m)) return false;
    AvpStr nm = shsm_re_group(&m, 1);
    AvpStr r = avp_str_slice(sym, shsm_re_end(&m, 0), AVP_END);
    int count = 1;
    if (r.n && r.p[0] == 'Q') {
        /* the lookahead guarantees a digit (ASCII in every symbol: D-check_rules-4) */
        count = r.p[1] - '0';
        r = avp_str_lstrip_chars(avp_str_slice(r, 2, AVP_END), AVP_S("_"));
    }
    if (count <= 0) return false; /* Q0: the Python raises IndexError (D-check_rules-4) */
    AvpStr parts[10];
    for (int k = 0; k < count; k++) {
        size_t d = 0;
        u64 ln = 0;
        while (d < r.n && r.p[d] >= '0' && r.p[d] <= '9') {
            if (ln < (1u << 30)) ln = ln * 10 + (u64)(r.p[d] - '0');
            d++;
        }
        if (!d) return false;
        size_t end = d + ln < r.n ? d + (size_t)ln : r.n;
        parts[k] = (AvpStr){r.p + d, end - d};
        r = (AvpStr){r.p + end, r.n - end};
    }
    if (parts[count - 1].n == 0) return false;
    *cls = avp_str_join(a ? a : avp_arena(), AVP_S("::"), parts, (size_t)count);
    *name = avp_str_eqz(nm, "__dt") ? AVP_S("~") : nm;
    *rest = r.p ? r : AVP_S("");
    return true;
}

void rl_demangle_member(AvpArena *a, AvpStr sym, AvpStr *cls, AvpStr *name) {
    if (!sym.p || !sym.n) {
        *cls = *name = NONE;
        return;
    }
    AvpStr rest;
    rl_demangle_parts(a, sym, cls, name, &rest);
}

AvpStr rl_mangled_params(AvpArena *a, AvpStr sym) {
    if (!sym.p || !sym.n) return NONE;
    AvpStr cls, name, rest;
    if (!rl_demangle_parts(a, sym, &cls, &name, &rest) || !cls.n) return NONE;
    if (rest.n && rest.p[0] == 'C') rest = avp_str_slice(rest, 1, AVP_END);
    if (rest.n && rest.p[0] == 'F') return avp_str_slice(rest, 1, AVP_END);
    return NONE;
}

AvpStr rl_template_name(AvpStr name) {
    name = avp_str_strip(name);
    if (!name.n || name.p[name.n - 1] != '>') return NONE;
    int depth = 0;
    for (size_t k = name.n; k-- > 0;) {
        if (name.p[k] == '>') {
            depth++;
        } else if (name.p[k] == '<') {
            depth--;
            if (depth == 0) return k ? (AvpStr){name.p, k} : NONE;
        }
    }
    return NONE;
}

/* ---- source classes (L1427-1647) ---- */
bool rl_sig_eq(const RlSig *x, const RlSig *y) {
    if (x->const_ != y->const_ || x->np != y->np) return false;
    for (u32 i = 0; i < x->np; i++)
        if (!avp_str_eq(x->params[i], y->params[i])) return false;
    return true;
}

static const char *const PARAM_TYPE_WORDS[] = {"void",   "char",     "short",    "int",   "long",
                                               "float",  "double",   "bool",     "signed", "unsigned",
                                               "const",  "volatile", "wchar_t", NULL};
static const char *const ATTR_SPECIFIERS[] = {"__declspec", "__attribute__", "ATTRIBUTE_ALIGN", NULL};
static const char *const ATTR_PREV[] = {"__declspec", "__attribute__", "ATTRIBUTE_ALIGN", "__attribute",
                                        "alignas",    "sizeof",        NULL};

static bool is_ident(AvpStr s) {
    static const ShsmRe *re;
    if (!re) re = shsm_re("[A-Za-z_]\\w*", 0);
    ShsmReMatch m;
    return shsm_re_fullmatch(re, s, 0, AVP_END, &m);
}

/* _param_types(toks, match, lp, rp) */
static RlSig param_types(AvpArena *a, const RlTok *toks, i32 lp, i32 rp) {
    RL_VEC(AvpStr) out = {0};
    AvpStr words[512];
    size_t nw = 0;
    int depth = 0;
    for (i32 i = lp + 1; i <= rp; i++) {
        bool sentinel = i == rp;
        AvpStr t = sentinel ? AVP_S(",") : toks[i].t;
        if (!sentinel) {
            if (avp_str_eqz(t, "(") || avp_str_eqz(t, "[") || avp_str_eqz(t, "<")) depth++;
            else if (avp_str_eqz(t, ")") || avp_str_eqz(t, "]") || avp_str_eqz(t, ">")) depth--;
        }
        if (avp_str_eqz(t, ",") && depth == 0) {
            size_t n = nw;
            for (size_t k = 0; k < n; k++)
                if (avp_str_eqz(words[k], "=")) {
                    n = k;
                    break;
                }
            bool array = false;
            for (size_t k = 0; k < n; k++)
                if (avp_str_eqz(words[k], "[")) {
                    array = true;
                    n = k;
                    break;
                }
            if (n >= 2 && is_ident(words[n - 1]) && !rl_sin(words[n - 1], PARAM_TYPE_WORDS) &&
                !avp_str_eqz(words[n - 2], "::"))
                n--;
            if (array) words[n++] = AVP_S("*");
            if (n) *RL_PUSH(a, &out) = avp_str_join(a, AVP_S(" "), words, n);
            nw = 0;
        } else if (nw < sizeof words / sizeof words[0]) {
            words[nw++] = t;
        }
    }
    if (out.n == 1 && avp_str_eqz(out.v[0], "void")) out.n = 0;
    return (RlSig){out.v, (u32)out.n, false};
}

static i32 mget(const i32 *match, i32 k) {
    return match[k] >= 0 ? match[k] : k;
}

/* _member_function(toks, match, a, b, short): false for None. */
static bool member_function(AvpArena *ar, const RlTok *toks, const i32 *match, i32 a, i32 b, AvpStr short_,
                            RlSrcMethod *out) {
    static const char *const SKIP[] = {"friend", "typedef", "using", "template", "enum", NULL};
    if (a >= b || rl_tin(&toks[a], SKIP)) return false;
    i32 k = a, head_lo = a, head_hi = a, lp = 0, rp = 0, name_line = 0;
    AvpStr name = {0};
    bool found = false;
    while (k < b) {
        const RlTok *x = &toks[k];
        if (TEQ(x, "=")) return false;
        if (TEQ(x, "[") && match[k] >= 0) {
            k = match[k] + 1;
            continue;
        }
        if (TEQ(x, "operator")) {
            i32 p = k + 1;
            if (p + 1 < b && TEQ(&toks[p], "(") && TEQ(&toks[p + 1], ")")) p += 2;
            while (p < b && !TEQ(&toks[p], "(")) p++;
            if (p >= b) return false;
            AvpBuf nb;
            avp_buf_init(&nb, ar);
            avp_buf_put(&nb, AVP_S("operator"));
            for (i32 q = k + 1; q < p; q++) avp_buf_put(&nb, toks[q].t);
            name = avp_buf_str(&nb);
            head_lo = a;
            head_hi = k;
            name_line = x->line;
            lp = p;
            rp = mget(match, p);
            found = true;
            break;
        }
        if (TEQ(x, "(")) {
            const RlTok *prev = k > a ? &toks[k - 1] : NULL;
            if (prev && rl_tin(prev, ATTR_PREV)) {
                k = mget(match, k) + 1;
                continue;
            }
            if (!prev || prev->k != RL_ID || k + 1 >= b || TEQ(&toks[k + 1], "*") || TEQ(&toks[k + 1], "&"))
                return false;
            name = prev->t;
            if (k - 2 >= a && TEQ(&toks[k - 2], "~")) name = AVP_S("~");
            head_lo = a;
            head_hi = k - 1;
            name_line = prev->line;
            lp = k;
            rp = mget(match, k);
            found = true;
            break;
        }
        k++;
    }
    if (!found) return false;
    bool pure = false;
    for (i32 q = rp + 1; q + 1 < b; q++)
        if (TEQ(&toks[q], "=") && TEQ(&toks[q + 1], "0")) {
            pure = true;
            break;
        }
    memset(out, 0, sizeof *out);
    if (rp > lp) out->sig = param_types(ar, toks, lp, rp);
    out->sig.const_ = rp + 1 < b && TEQ(&toks[rp + 1], "const");
    out->name = name;
    out->line = name_line;
    for (i32 q = head_lo; q < head_hi; q++) {
        if (TEQ(&toks[q], "virtual")) out->virtual_ = true;
        if (TEQ(&toks[q], "static")) out->static_ = true;
    }
    out->pure = pure;
    out->ctor = avp_str_eq(name, short_);
    return true;
}

/* _class_members(toks, match, lo, hi, short) */
static void class_members(AvpArena *ar, const RlTok *toks, const i32 *match, i32 lo, i32 hi, RlSrcClass *c) {
    i32 i = lo, start = lo;
    bool nested = false;
    RlSrcMethod m;
    while (i < hi) {
        const RlTok *t = &toks[i];
        if (i == start && (TEQ(t, "public") || TEQ(t, "private") || TEQ(t, "protected")) && i + 1 < hi &&
            TEQ(&toks[i + 1], ":")) {
            i += 2;
            start = i;
            continue;
        }
        if ((TEQ(t, "(") || TEQ(t, "[")) && match[i] >= 0) {
            i = match[i] + 1;
            continue;
        }
        if (TEQ(t, "{") && match[i] >= 0) {
            i32 end = match[i];
            if (!nested && member_function(ar, toks, match, start, i, c->short_, &m)) {
                *RL_PUSH(ar, &c->methods) = m;
                i = end + 1;
                if (i < hi && TEQ(&toks[i], ";")) i++;
                start = i;
                continue;
            }
            nested = true;
            i = end + 1;
            continue;
        }
        if (TEQ(t, ";")) {
            if (!nested && member_function(ar, toks, match, start, i, c->short_, &m)) *RL_PUSH(ar, &c->methods) = m;
            i++;
            start = i;
            nested = false;
            continue;
        }
        i++;
    }
}

typedef struct {
    i32 open, close;
    AvpStr name;
} Scope;
typedef struct {
    i32 i, k;
} Found;

static RlSrcClasses *parse_cpp_classes(RlParsed *P) {
    AvpArena *a = P->a;
    const RlTok *toks = P->toks.v;
    const i32 *match = P->match;
    i32 n = (i32)P->toks.n;
    RL_VEC(Scope) scopes = {0};
    RL_VEC(Found) at = {0}; /* the keyword and body indices of each found class */
    RlSrcClasses *found = AVP_NEW(a, RlSrcClasses);
    for (i32 i = 0; i < n; i++) {
        const RlTok *t = &toks[i];
        if (t->k != RL_ID) continue;
        if (TEQ(t, "namespace") && i + 2 < n && toks[i + 1].k == RL_ID && TEQ(&toks[i + 2], "{") &&
            match[i + 2] >= 0) {
            *RL_PUSH(a, &scopes) = (Scope){i + 2, match[i + 2], toks[i + 1].t};
            continue;
        }
        if (!TEQ(t, "class") && !TEQ(t, "struct")) continue;
        AvpStr prev = i ? toks[i - 1].t : AVP_S("");
        if (avp_str_eqz(prev, "<") || avp_str_eqz(prev, ",") || avp_str_eqz(prev, "enum") ||
            avp_str_eqz(prev, "friend") || avp_str_eqz(prev, "typename"))
            continue;
        i32 j = i + 1;
        while (j + 1 < n && rl_tin(&toks[j], ATTR_SPECIFIERS) && TEQ(&toks[j + 1], "(")) j = mget(match, j + 1) + 1;
        if (j >= n || toks[j].k != RL_ID) continue;
        i32 k = j + 1, bases_lo = -1;
        if (k < n && TEQ(&toks[k], ":")) {
            bases_lo = k + 1;
            while (k < n && !TEQ(&toks[k], "{") && !TEQ(&toks[k], ";")) k++;
        }
        if (k >= n || !TEQ(&toks[k], "{") || match[k] < 0) continue;
        RlSrcClass *c = RL_PUSH(a, found);
        c->name = c->short_ = toks[j].t;
        c->path = P->path;
        c->line = t->line;
        c->template_ = avp_str_eqz(prev, ">");
        if (bases_lo >= 0) {
            AvpStr words[256];
            size_t nw = 0;
            int depth = 0;
            for (i32 q = bases_lo; q <= k; q++) {
                bool sentinel = q == k;
                AvpStr x = sentinel ? AVP_S(",") : toks[q].t;
                if (!sentinel) {
                    if (avp_str_eqz(x, "<")) depth++;
                    else if (avp_str_eqz(x, ">")) depth--;
                }
                if (avp_str_eqz(x, ",") && depth == 0) {
                    size_t keep = 0;
                    for (size_t w = 0; w < nw; w++) {
                        if (avp_str_eqz(words[w], "virtual")) c->virtual_bases = true;
                        if (avp_str_eqz(words[w], "public") || avp_str_eqz(words[w], "private") ||
                            avp_str_eqz(words[w], "protected") || avp_str_eqz(words[w], "virtual"))
                            continue;
                        words[keep++] = words[w];
                    }
                    if (keep)
                        *RL_PUSH(a, &c->bases) =
                            avp_str_lstrip_chars(avp_str_join(a, AVP_S(""), words, keep), AVP_S(":"));
                    nw = 0;
                } else if (nw < sizeof words / sizeof words[0]) {
                    words[nw++] = x;
                }
            }
        }
        *RL_PUSH(a, &scopes) = (Scope){k, match[k], c->short_};
        *RL_PUSH(a, &at) = (Found){i, k};
    }
    for (size_t f = 0; f < found->n; f++) {
        RlSrcClass *c = &found->v[f];
        i32 i = at.v[f].i, k = at.v[f].k;
        Scope outer[64];
        AvpStr names[64];
        size_t nn = 0;
        for (size_t s = 0; s < scopes.n; s++)
            if (scopes.v[s].open < i && i < scopes.v[s].close && nn < 63) {
                /* sorted by the opening index (insertion sort; stable like sorted()) */
                size_t q = nn++;
                while (q > 0 && outer[q - 1].open > scopes.v[s].open) {
                    outer[q] = outer[q - 1];
                    q--;
                }
                outer[q] = scopes.v[s];
            }
        for (size_t q = 0; q < nn; q++) names[q] = outer[q].name;
        if (nn) {
            names[nn++] = c->short_;
            c->name = avp_str_join(a, AVP_S("::"), names, nn);
        }
        class_members(a, toks, match, k + 1, match[k], c);
    }
    return found;
}

const RlSrcClasses *rl_cpp_classes(RlParsed *P) {
    if (!P->cpp_classes_memo) P->cpp_classes_memo = parse_cpp_classes(P);
    return (const RlSrcClasses *)P->cpp_classes_memo;
}

AvpStr rl_class_display(AvpArena *a, const RlSrcClass *c, AvpStr m) {
    if (avp_str_eqz(m, "~")) return avp_str_concat(a, AVP_S("~"), c->short_);
    return m;
}

/* ---- JSON helpers ---- */
AvpStr rl_json_strv(const AvpJson *v) {
    if (!v || v->kind != AVP_JSON_STRING) return NONE;
    return v->u.str.p ? v->u.str : AVP_S("");
}

AvpStr rl_py_str(AvpArena *a, const AvpJson *v) {
    if (!v || v->kind == AVP_JSON_NULL) return AVP_S("None");
    switch (v->kind) {
    case AVP_JSON_STRING: return rl_json_strv(v);
    case AVP_JSON_BOOL: return v->b ? AVP_S("True") : AVP_S("False");
    case AVP_JSON_INT: return v->big ? v->u.str : avp_str(avp_sprintf(a, "%lld", (long long)v->u.i));
    case AVP_JSON_FLOAT: {
        char buf[32];
        size_t n = avp_float_repr(v->u.f, buf);
        return avp_str_dup(a, (AvpStr){buf, n});
    }
    default: /* a container: never in classes.json where it is printed (D-check_rules-5) */
        return avp_json_dumps(a, v, &AVP_JSON_COMPACT);
    }
}

bool rl_int16(const AvpJson *v, i64 *out, const char **error) {
    if (!v || v->kind != AVP_JSON_STRING) {
        *error = "TypeError";
        return false;
    }
    AvpStr s = avp_str_strip(rl_json_strv(v));
    size_t i = 0;
    bool neg = false;
    if (i < s.n && (s.p[i] == '+' || s.p[i] == '-')) neg = s.p[i++] == '-';
    if (i + 1 < s.n && s.p[i] == '0' && (s.p[i + 1] == 'x' || s.p[i + 1] == 'X')) {
        i += 2;
        if (i < s.n && s.p[i] == '_') i++;
    }
    u64 val = 0;
    size_t digits = 0;
    bool last_us = true;
    for (; i < s.n; i++) {
        char ch = s.p[i];
        int d;
        if (ch >= '0' && ch <= '9') d = ch - '0';
        else if (ch >= 'a' && ch <= 'f') d = ch - 'a' + 10;
        else if (ch >= 'A' && ch <= 'F') d = ch - 'A' + 10;
        else if (ch == '_' && !last_us) {
            last_us = true;
            continue;
        } else {
            *error = "ValueError";
            return false;
        }
        val = val * 16 + (u64)d;
        digits++;
        last_us = false;
    }
    if (!digits || last_us) {
        *error = "ValueError";
        return false;
    }
    *out = neg ? -(i64)val : (i64)val;
    return true;
}

/* ---- load_classes (L1650-1709) ---- */
/* classes_stale: the Python checker's wording around the verdict of the C reader (classes.h classes_staleness, the
 * comparison classes.exe's readers make), so a classes.json from either scanner, under either base rule, is current
 * when its input stamp matches (DEVIATIONS.md D-check_rules-11). */
static AvpStr classes_stale(AvpArena *a, const AvpJson *data) {
    const AvpJson *rec = avp_json_getz(data, "inputs");
    const AvpJson *as = rec && rec->kind == AVP_JSON_OBJECT ? avp_json_getz(rec, "asm") : NULL;
    const AvpJson *sy = rec && rec->kind == AVP_JSON_OBJECT ? avp_json_getz(rec, "symbols") : NULL;
    const AvpJson *dirv = as && as->kind == AVP_JSON_OBJECT ? avp_json_getz(as, "dir") : NULL;
    const AvpJson *symv = sy && sy->kind == AVP_JSON_OBJECT ? avp_json_getz(sy, "path") : NULL;
    if (!rec || rec->kind != AVP_JSON_OBJECT || !avp_json_truthy(dirv) || !avp_json_truthy(symv))
        return AVP_S("it records no input stamp (written by an older classes.exe)");
    AvpStr dir = rl_json_strv(dirv), sym = rl_json_strv(symv);
    if (!avp_isdir(avp_cz(a, dir)) || !avp_isfile(avp_cz(a, sym)))
        return rl_fmt(a, "its inputs (%.*s, %.*s) are gone", AVP_SF(dir), AVP_SF(sym));
    ClassesStaleness st = classes_staleness(a, rec, avp_cz(a, dir), avp_cz(a, sym));
    switch (st.verdict) {
    case CLASSES_STAMP_CURRENT: return NONE;
    case CLASSES_STAMP_UNREADABLE: return rl_fmt(a, "its inputs cannot be read (%s)", st.error);
    case CLASSES_STAMP_NONE: return AVP_S("it records no input stamp (written by an older classes.exe)");
    default: break;
    }
    if (st.asm_changed && st.symbols_changed) return AVP_S("the asm tree and the symbols file changed since the scan");
    return st.symbols_changed ? AVP_S("the symbols file changed since the scan")
                              : AVP_S("the asm tree changed since the scan");
}

AvpJson *rl_load_classes(AvpArena *a, const char *path, AvpStr *note) {
    *note = NONE;
    if (!avp_isfile(path)) {
        *note = rl_fmt(a, "R13 slot checks skipped: %s not found (run build/tools/classes.exe scan)", path);
        return NULL;
    }
    AvpBytes bytes = {0};
    if (!avp_read_all(a, path, &bytes)) {
        *note = rl_fmt(a, "R13 slot checks skipped: %s is unreadable (%.*s)", path, AVP_SF(avp_io_err_str(a)));
        return NULL;
    }
    AvpJsonError err;
    AvpJson *data = avp_json_parse(a, (AvpStr){(const char *)bytes.v, bytes.n}, &err);
    if (!data) {
        AvpStr why = err.kind == AVP_JSON_E_UNICODE ? AVP_S("invalid UTF-8") : avp_json_error_str(a, &err);
        *note = rl_fmt(a, "R13 slot checks skipped: %s is unreadable (%.*s)", path, AVP_SF(why));
        return NULL;
    }
    if (data->kind != AVP_JSON_OBJECT || !avp_json_is_arr(avp_json_getz(data, "classes"))) {
        *note = rl_fmt(a, "R13 slot checks skipped: %s has no class list", path);
        return NULL;
    }
    AvpStr why = classes_stale(a, data);
    if (why.p) {
        *note = rl_fmt(a, "R13 slot checks skipped: %s is stale: %.*s (run build/tools/classes.exe scan)", path,
                       AVP_SF(why));
        return NULL;
    }
    return data;
}

/* ---- RetailIndex (L1729-1805) ---- */
typedef RL_VEC(const AvpJson *) Recs;
struct RlRetailIndex {
    AvpArena *a;
    AvpMap *by_name; /* name (or "\x01None") -> Recs* */
    AvpStrVec name_keys; /* by_name keys in insertion order (None as {NULL, 0}) */
    AvpMap *by_vt;   /* symbol -> const AvpJson* (last) */
    AvpIMap *by_addr;
    AvpMap *by_mangled, *ctor_mangled;
};

static Recs *recs_of(AvpArena *a, AvpMap *m, AvpStr key, bool create) {
    AvpVal *v = create ? avp_map_put(m, key, NULL) : avp_map_get(m, key);
    if (!v) return NULL;
    if (!v->p) v->p = AVP_NEW(a, Recs);
    return v->p;
}

static AvpStr name_key(AvpStr name) {
    return name.p ? name : AVP_S("\x01None");
}

RlRetailIndex *rl_retail_index(AvpArena *a, const AvpJson *data, const char **error) {
    RlRetailIndex *ri = AVP_NEW(a, RlRetailIndex);
    ri->a = a;
    ri->by_name = avp_map_new();
    ri->by_vt = avp_map_new();
    ri->by_addr = avp_imap_new();
    ri->by_mangled = avp_map_new();
    ri->ctor_mangled = avp_map_new();
    const AvpJson *classes = avp_json_getz(data, "classes");
    size_t nc = avp_json_len(classes);
    static const char *const roles[] = {"ctors", "dtors", "factories"};
    for (size_t i = 0; i < nc; i++) {
        const AvpJson *c = avp_json_at(classes, i);
        AvpStr nm = rl_json_strv(avp_json_getz(c, "name"));
        AvpStr key = name_key(nm);
        bool created = false;
        AvpVal *v = avp_map_put(ri->by_name, key, &created);
        if (created) {
            v->p = AVP_NEW(a, Recs);
            avp_vec_push(&ri->name_keys, nm);
        }
        *RL_PUSH(a, (Recs *)v->p) = c;
        const AvpJson *vt = avp_json_getz(c, "vtable");
        if (!avp_json_truthy(vt)) continue;
        AvpStr sym = rl_json_strv(avp_json_getz(vt, "symbol"));
        if (sym.n) avp_map_put(ri->by_vt, sym, NULL)->p = (void *)c;
        RL_VEC(const AvpJson *) addrs = {0};
        *RL_PUSH(a, &addrs) = avp_json_getz(vt, "address");
        for (int r = 0; r < 3; r++) {
            const AvpJson *lst = avp_json_getz(c, roles[r]);
            for (size_t k = 0; k < avp_json_len(lst) && avp_json_is_arr(lst); k++) {
                const AvpJson *f = avp_json_at(lst, k);
                *RL_PUSH(a, &addrs) = avp_json_getz(f, "address");
                AvpStr cls, mn;
                rl_demangle_member(a, rl_json_strv(avp_json_getz(f, "name")), &cls, &mn);
                if (cls.n) {
                    *RL_PUSH(a, recs_of(a, ri->by_mangled, cls, true)) = c;
                    if (r != 2) *RL_PUSH(a, recs_of(a, ri->ctor_mangled, cls, true)) = c;
                }
            }
        }
        const AvpJson *mem = avp_json_getz(c, "members");
        for (size_t k = 0; k < avp_json_len(mem) && avp_json_is_arr(mem); k++) {
            AvpStr cls, mn;
            rl_demangle_member(a, rl_json_strv(avp_json_getz(avp_json_at(mem, k), "name")), &cls, &mn);
            if (cls.n) *RL_PUSH(a, recs_of(a, ri->by_mangled, cls, true)) = c;
        }
        for (size_t k = 0; k < addrs.n; k++) {
            if (!avp_json_truthy(addrs.v[k])) continue;
            i64 ad;
            if (!rl_int16(addrs.v[k], &ad, error)) return NULL;
            AvpVal *av = avp_imap_put(ri->by_addr, (u64)ad, NULL);
            if (!av->p) av->p = AVP_NEW(a, Recs);
            *RL_PUSH(a, (Recs *)av->p) = c;
        }
    }
    return ri;
}

static int name_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const AvpJson *a = *(const AvpJson *const *)x, *b = *(const AvpJson *const *)y;
    return avp_str_cmp(rl_json_strv(avp_json_getz(a, "name")), rl_json_strv(avp_json_getz(b, "name")));
}

const AvpJson **rl_retail_instances(RlRetailIndex *ri, const RlSrcClass *sc, size_t *count) {
    RL_VEC(const AvpJson *) out = {0};
    for (size_t i = 0; i < ri->name_keys.n; i++) {
        AvpStr nm = ri->name_keys.v[i];
        AvpStr t = rl_template_name(nm.p ? nm : AVP_S(""));
        if (!t.p || !avp_str_eq(t, sc->name)) continue;
        Recs *r = recs_of(ri->a, ri->by_name, name_key(nm), false);
        for (size_t k = 0; k < r->n; k++)
            if (avp_json_truthy(avp_json_getz(r->v[k], "vtable"))) *RL_PUSH(ri->a, &out) = r->v[k];
    }
    if (out.n > 1) avp_sort(out.v, out.n, sizeof *out.v, name_cmp, NULL, false);
    *count = out.n;
    return out.v;
}

/* _one(lst) over the records with a vtable: 0 none, 1 one (*one), 2 several (distinct in *many). */
static int one(AvpArena *a, const Recs *lst, bool need_vtable, const AvpJson **one_, Recs *many) {
    Recs uniq = {0};
    for (size_t i = 0; lst && i < lst->n; i++) {
        if (need_vtable && !avp_json_truthy(avp_json_getz(lst->v[i], "vtable"))) continue;
        bool seen = false;
        for (size_t k = 0; k < uniq.n; k++)
            if (uniq.v[k] == lst->v[i]) seen = true;
        if (!seen) *RL_PUSH(a, &uniq) = lst->v[i];
    }
    if (uniq.n == 1) *one_ = uniq.v[0];
    if (many) *many = uniq;
    return uniq.n == 0 ? 0 : uniq.n == 1 ? 1 : 2;
}

static size_t code_points(AvpStr s) {
    size_t n = 0;
    for (size_t i = 0; i < s.n; i++)
        if (((u8)s.p[i] & 0xC0) != 0x80) n++;
    return n;
}

const AvpJson *rl_retail_find(RlRetailIndex *ri, const RlSrcClass *sc, AvpStr *how) {
    AvpArena *a = ri->a;
    AvpStr name = sc->name;
    static const ShsmRe *ph_re;
    if (!ph_re) ph_re = shsm_re("unk_([0-9A-Fa-f]{8})", 0);
    ShsmReMatch m;
    bool ph = shsm_re_fullmatch(ph_re, name, 0, AVP_END, &m);
    const AvpJson *r = NULL;
    if (ph) {
        AvpStr hex = shsm_re_group(&m, 1);
        u64 ad = 0;
        for (size_t i = 0; i < hex.n; i++) {
            char ch = hex.p[i];
            ad = ad * 16 + (u64)(ch <= '9' ? ch - '0' : (ch | 0x20) - 'a' + 10);
        }
        AvpVal *v = avp_imap_get(ri->by_addr, ad);
        if (one(a, v ? v->p : NULL, false, &r, NULL) == 1) {
            AvpStr up = avp_str_dup(a, hex);
            for (size_t i = 0; i < up.n; i++)
                if (up.p[i] >= 'a' && up.p[i] <= 'f') ((char *)up.p)[i] = (char)(up.p[i] - 32);
            *how = rl_fmt(a, "address 0x%.*s", AVP_SF(up));
            return r;
        }
    } else {
        if (one(a, recs_of(a, ri->by_name, name_key(name), false), true, &r, NULL) == 1) {
            *how = AVP_S("name");
            return r;
        }
        AvpBuf b;
        avp_buf_init(&b, a);
        AvpStrVec parts = avp_str_split(name, AVP_S("::"), -1);
        if (parts.n > 1) avp_buf_printf(&b, "Q%zu", parts.n);
        for (size_t i = 0; i < parts.n; i++) {
            avp_buf_printf(&b, "%zu", code_points(parts.v[i]));
            avp_buf_put(&b, parts.v[i]);
        }
        avp_vec_free(&parts);
        AvpVal *v = avp_map_get(ri->by_vt, avp_str_concat(a, AVP_S("__vt__"), avp_buf_str(&b)));
        if (v && avp_json_truthy(v->p)) {
            *how = AVP_S("vtable symbol");
            return v->p;
        }
    }
    AvpMap *tables[2] = {ri->ctor_mangled, ri->by_mangled};
    const char *hows[2] = {"constructor/destructor", "member functions"};
    for (int t = 0; t < 2; t++) {
        Recs many;
        int k = one(a, recs_of(a, tables[t], name, false), true, &r, &many);
        if (k == 1) {
            *how = avp_str(hows[t]);
            return r;
        }
        if (k == 2) {
            AvpStrVec names = {0};
            for (size_t i = 0; i < many.n; i++) {
                const AvpJson *nv = avp_json_getz(many.v[i], "name");
                avp_vec_push(&names, nv ? rl_json_strv(nv) : AVP_S("?"));
            }
            avp_sort_strs(names.v, names.n);
            *how = rl_fmt(a, "several retail vtables fit (%.*s)",
                          AVP_SF(avp_str_join(a, AVP_S(", "), names.v, names.n)));
            avp_vec_free(&names);
            return NULL;
        }
    }
    if (ph) {
        Recs *lst = recs_of(a, ri->by_name, name_key(name), false);
        if (lst && lst->n && one(a, lst, true, &r, NULL) == 1) {
            *how = AVP_S("name");
            return r;
        }
    }
    *how = AVP_S("no retail vtable found by name, vtable symbol, address or member functions");
    return NULL;
}

/* ---- vtable words (L1808-1852) ---- */
AvpStr rl_slot_implementation(AvpArena *a, const AvpJson *w) {
    AvpStr own = rl_json_strv(avp_json_getz(w, "target"));
    const AvpJson *th = avp_json_getz(w, "thunk");
    AvpStr tgt = avp_json_truthy(th) ? rl_json_strv(avp_json_getz(th, "target")) : NONE;
    if (!tgt.n) return own;
    AvpStr own_cls, nm, tcls;
    rl_demangle_member(a, own, &own_cls, &nm);
    if (own_cls.n && !avp_str_startswith(own, AVP_S("@"))) return own;
    rl_demangle_member(a, tgt, &tcls, &nm);
    if (own_cls.n && !tcls.n) return own;
    return tgt;
}

RlRetailSlot *rl_retail_complete_slots(AvpArena *a, const AvpJson *rc, size_t *count) {
    const AvpJson *words = avp_json_getz(avp_json_getz(rc, "vtable"), "words");
    RL_VEC(RlRetailSlot) out = {0};
    for (size_t i = 0; i < avp_json_len(words) && avp_json_is_arr(words); i++) {
        const AvpJson *w = avp_json_at(words, i);
        const AvpJson *grp = avp_json_getz(w, "group"), *stv = avp_json_getz(w, "status");
        AvpStr st = rl_json_strv(stv);
        if (!avp_str_eqz(rl_json_strv(grp), "complete") || avp_str_eqz(st, "header")) continue;
        RlRetailSlot *e = RL_PUSH(a, &out);
        e->index = avp_json_getz(w, "index");
        e->offset = avp_json_getz(w, "offset");
        const AvpJson *tv = avp_json_getz(w, "target");
        e->target = rl_json_strv(tv);
        bool tnone = !tv || tv->kind == AVP_JSON_NULL;
        if (avp_str_eqz(st, "null") || (tnone && !avp_str_eqz(st, "unresolved")) ||
            avp_str_eqz(e->target, "__pure_virtual")) {
            e->kind = RL_SLOT_NULL;
        } else if (avp_str_eqz(st, "unresolved")) {
            e->kind = RL_SLOT_UNRESOLVED;
        } else {
            AvpStr tgt = rl_slot_implementation(a, w);
            rl_demangle_member(a, tgt, &e->cls, &e->name);
            e->params = rl_mangled_params(a, tgt);
            e->kind = e->cls.n ? RL_SLOT_NAMED : RL_SLOT_UNNAMED;
        }
    }
    *count = out.n;
    return out.v;
}
