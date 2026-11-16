/* CodeWarrior C++ name mangling (tools/common/mangle.py: mangle, mangle_type, mangle_scope, mangle_template_name,
 * _split_scope), statement for statement. ValueError becomes SHSM_EXC_VALUE with the Python message. The Python
 * recurses once per template nesting level (mangle_type -> mangle_scope -> mangle_template_name -> mangle_type); a
 * nesting deeper than MAX_NESTING raises SHSM_EXC_RECURSION as the interpreter's stack limit would (shsm_mangle.md). */
#include "avp/fmt.h"
#include "shsm_cli.h"
#include "shsm_mangle.h"
#include "shsm_mangle_internal.h"
#include "shsm_text.h"

#define MAX_NESTING 248 /* CPython 3.14: mangle_type("A<A<...int...>>") fails from 249 levels, called at top level */

static AvpStr mtype(AvpArena *a, AvpStr text, int depth);

/* len(s): code points (UTF-8 continuation bytes do not count) */
static size_t cp_count(AvpStr s) {
    size_t n = 0;
    for (size_t i = 0; i < s.n; i++)
        if (((unsigned char)s.p[i] & 0xC0) != 0x80) n++;
    return n;
}

static AvpStr slice(AvpStr s, size_t i, size_t j) { return avp_str_slice(s, (i64)i, (i64)j); }

/* re.sub(r"\s+", " ", text.strip()) */
static AvpStr normalize_ws(AvpArena *a, AvpStr text) {
    AvpStr t = avp_str_strip(text);
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < t.n;) {
        if (shsm_ws_len(t, i)) {
            avp_buf_putc(&b, ' ');
            i = shsm_skip_ws(t, i);
        } else {
            avp_buf_putc(&b, t.p[i++]);
        }
    }
    return b.n ? avp_buf_str(&b) : AVP_S("");
}

/* _split_scope: cut at "::" outside <...> */
static AvpStr *split_scope(AvpArena *a, AvpStr text, size_t *np) {
    AvpStr *out = AVP_NEWN(a, AvpStr, text.n / 2 + 2);
    size_t n = 0, start = 0, i = 0;
    int depth = 0;
    while (i < text.n) {
        char ch = text.p[i];
        if (ch == '<') depth++;
        else if (ch == '>') depth--;
        if (depth == 0 && ch == ':' && i + 1 < text.n && text.p[i + 1] == ':') {
            out[n++] = slice(text, start, i);
            i += 2;
            start = i;
            continue;
        }
        i++;
    }
    out[n++] = slice(text, start, text.n);
    *np = n;
    return out;
}

/* scope.split("::") */
static AvpStr *split_colons(AvpArena *a, AvpStr text, size_t *np) {
    AvpStr *out = AVP_NEWN(a, AvpStr, text.n / 2 + 2);
    size_t n = 0, start = 0;
    for (size_t i = 0; i + 1 < text.n;) {
        if (text.p[i] == ':' && text.p[i + 1] == ':') {
            out[n++] = slice(text, start, i);
            i += 2;
            start = i;
        } else {
            i++;
        }
    }
    out[n++] = slice(text, start, text.n);
    *np = n;
    return out;
}

/* _split_targs into the arena (the shared splitter allocates on the heap) */
static AvpStr *targs(AvpArena *a, AvpStr s, size_t *n) {
    AvpStrVec v = shsm_mangle_split_targs(s);
    AvpStr *out = AVP_NEWN(a, AvpStr, v.n + 1);
    memcpy(out, v.v, v.n * sizeof(AvpStr));
    *n = v.n;
    avp_vec_free(&v);
    return out;
}

static AvpStr mtemplate(AvpArena *a, AvpStr name, int depth) {
    i64 i = avp_str_findc(name, '<');
    if (i < 0 || !avp_str_endswith(name, AVP_S(">"))) return name;
    size_t n;
    AvpStr *args = targs(a, avp_str_slice(name, i + 1, (i64)name.n - 1), &n);
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_put(&b, avp_str_slice(name, 0, i));
    avp_buf_putc(&b, '<');
    for (size_t k = 0; k < n; k++) {
        AvpStr arg = avp_str_strip(args[k]);
        if (k) avp_buf_putc(&b, ',');
        avp_buf_put(&b, shsm_mangle_is_int_literal(arg) ? arg : mtype(a, arg, depth + 1));
    }
    avp_buf_putc(&b, '>');
    return avp_buf_str(&b);
}

static AvpStr mscope(AvpArena *a, const AvpStr *in, size_t n, int depth) {
    if (depth > MAX_NESTING) shsm_raise(SHSM_EXC_RECURSION, "maximum recursion depth exceeded");
    AvpStr *parts = AVP_NEWN(a, AvpStr, n + 1);
    size_t m = 0;
    for (size_t i = 0; i < n; i++)
        if (in[i].n) parts[m++] = mtemplate(a, in[i], depth);
    if (!m) return AVP_S("");
    AvpBuf b;
    avp_buf_init(&b, a);
    if (m == 1) {
        avp_buf_printf(&b, "%zu", cp_count(parts[0]));
        avp_buf_put(&b, parts[0]);
        return avp_buf_str(&b);
    }
    if (m > 9) shsm_raise(SHSM_EXC_VALUE, "scope too deep");
    avp_buf_printf(&b, "Q%zu", m);
    for (size_t i = 0; i < m; i++) {
        avp_buf_printf(&b, "%zu", cp_count(parts[i]));
        avp_buf_put(&b, parts[i]);
    }
    return avp_buf_str(&b);
}

static const char *builtin_code(AvpStr w) {
    static const char *const rev[][2] = {{"void", "v"},   {"bool", "b"},   {"char", "c"},         {"short", "s"},
                                         {"int", "i"},    {"long", "l"},   {"long long", "x"},    {"float", "f"},
                                         {"double", "d"}, {"long double", "r"}, {"wchar_t", "w"}, {"...", "e"}};
    for (size_t i = 0; i < AVP_ARRAY_LEN(rev); i++)
        if (avp_str_eqz(w, rev[i][0])) return rev[i][1];
    return NULL;
}

static bool is_cv(AvpStr w) { return avp_str_eqz(w, "const") || avp_str_eqz(w, "volatile"); }

/* re.findall(r"[^\s<]+(?:<.*>)?|\S+", s): the tokens, in order */
static AvpStr *base_tokens(AvpArena *a, AvpStr s, size_t *np) {
    AvpStr *out = AVP_NEWN(a, AvpStr, s.n + 1);
    size_t n = 0, i = 0;
    while (i < s.n) {
        if (shsm_ws_len(s, i)) {
            i = shsm_skip_ws(s, i);
            continue;
        }
        size_t end = i;
        if (s.p[i] != '<') {
            while (end < s.n && !shsm_ws_len(s, end) && s.p[end] != '<') end++;
            if (end < s.n && s.p[end] == '<') { /* (?:<.*>)? : up to the last '>' that follows */
                for (size_t k = s.n; k > end + 1; k--)
                    if (s.p[k - 1] == '>') {
                        end = k;
                        break;
                    }
            }
        } else {
            while (end < s.n && !shsm_ws_len(s, end)) end++;
        }
        out[n++] = slice(s, i, end);
        i = end;
    }
    *np = n;
    return out;
}

static AvpStr mtype(AvpArena *a, AvpStr text, int depth) {
    if (depth > MAX_NESTING) shsm_raise(SHSM_EXC_RECURSION, "maximum recursion depth exceeded");
    AvpStr t = normalize_ws(a, text);
    if (avp_str_findc(t, '(') >= 0 || avp_str_findc(t, '[') >= 0)
        shsm_raise(SHSM_EXC_VALUE, "unsupported type for mangling: %.*s", AVP_SF(text));
    /* split at the first '*' or '&' outside <...>: base (with its cv) | declarator chain */
    int dp = 0;
    size_t k = t.n;
    for (size_t j = 0; j < t.n; j++) {
        char ch = t.p[j];
        dp += (ch == '<') - (ch == '>');
        if (dp == 0 && (ch == '*' || ch == '&')) {
            k = j;
            break;
        }
    }
    size_t nbase;
    AvpStr *base_words = base_tokens(a, avp_str_strip(slice(t, 0, k)), &nbase);
    /* re.findall(r"\*|&|const|volatile", t[k:]) */
    AvpStr rest = slice(t, k, t.n);
    AvpStr *toks = AVP_NEWN(a, AvpStr, rest.n + 1);
    size_t ntok = 0;
    for (size_t i = 0; i < rest.n;) {
        AvpStr r = slice(rest, i, rest.n);
        size_t l = (rest.p[i] == '*' || rest.p[i] == '&') ? 1
                   : avp_str_startswith(r, AVP_S("const"))    ? 5
                   : avp_str_startswith(r, AVP_S("volatile")) ? 8
                                                              : 0;
        if (!l) {
            i++;
            continue;
        }
        toks[ntok++] = slice(rest, i, i + l);
        i += l;
    }
    AvpBuf cvb;
    avp_buf_init(&cvb, a);
    AvpStr *words = AVP_NEWN(a, AvpStr, nbase + 1);
    size_t nw = 0;
    for (size_t i = 0; i < nbase; i++) {
        if (is_cv(base_words[i])) avp_buf_putc(&cvb, avp_str_eqz(base_words[i], "const") ? 'C' : 'V');
        else words[nw++] = base_words[i];
    }
    const char *sign = "";
    size_t w0 = 0;
    if (nw && (avp_str_eqz(words[0], "unsigned") || avp_str_eqz(words[0], "signed"))) {
        sign = avp_str_eqz(words[0], "unsigned") ? "U" : "S";
        w0 = 1;
        if (nw == 1) { /* words[1:] or ["int"] */
            words[0] = AVP_S("int");
            w0 = 0;
        }
    }
    AvpStr w = avp_str_join(a, AVP_S(" "), words + w0, nw - w0);
    AvpBuf core;
    avp_buf_init(&core, a);
    avp_buf_put(&core, avp_buf_str(&cvb));
    const char *bc = builtin_code(w);
    if (bc) {
        avp_buf_putz(&core, sign);
        avp_buf_putz(&core, bc);
    } else if (*sign) {
        shsm_raise(SHSM_EXC_VALUE, "signed/unsigned on a non-builtin: %.*s", AVP_SF(text));
    } else {
        size_t np;
        AvpStr *parts = split_scope(a, w, &np);
        avp_buf_put(&core, mscope(a, parts, np, depth));
    }
    /* declarators apply inside-out: 'const char* const' = C P C c */
    AvpStr out = core.n ? avp_buf_str(&core) : AVP_S("");
    for (size_t i = 0; i < ntok;) {
        AvpStr d = toks[i++];
        AvpBuf q;
        avp_buf_init(&q, a);
        while (i < ntok && is_cv(toks[i])) avp_buf_putc(&q, avp_str_eqz(toks[i++], "const") ? 'C' : 'V');
        avp_buf_putc(&q, avp_str_eqz(d, "*") ? 'P' : 'R');
        avp_buf_put(&q, out);
        out = avp_buf_str(&q);
    }
    return out;
}

/* ---- public ---- */

const char **shsm_split_scope(AvpArena *a, const char *text, size_t *n) {
    size_t np;
    AvpStr *parts = split_scope(a, avp_str(text), &np);
    const char **out = AVP_NEWN(a, const char *, np + 1);
    for (size_t i = 0; i < np; i++) out[i] = avp_cz(a, parts[i]);
    if (n) *n = np;
    return out;
}

const char *shsm_mangle_template_name(AvpArena *a, const char *name) {
    return avp_cz(a, mtemplate(a, avp_str(name), 0));
}

const char *shsm_mangle_scope(AvpArena *a, const char *scope) {
    size_t np;
    AvpStr *parts = split_scope(a, avp_str(scope), &np);
    return avp_cz(a, mscope(a, parts, np, 0));
}

const char *shsm_mangle_scope_parts(AvpArena *a, const char *const *parts, size_t n) {
    AvpStr *p = AVP_NEWN(a, AvpStr, n + 1);
    for (size_t i = 0; i < n; i++) p[i] = avp_str(parts[i]);
    return avp_cz(a, mscope(a, p, n, 0));
}

const char *shsm_mangle_type(AvpArena *a, const char *text) { return avp_cz(a, mtype(a, avp_str(text), 0)); }

static const char *op_code(AvpStr op) {
    for (size_t i = 0; i < SHSM_MANGLE_NOPS; i++)
        if (avp_str_eqz(op, SHSM_MANGLE_OPS[i][1])) return SHSM_MANGLE_OPS[i][0];
    return NULL;
}

static AvpStr mangle_impl(AvpArena *a, AvpStr name, const AvpStr *scope, size_t nscope, const char *const *params,
                          size_t nparams, bool is_const, bool is_volatile) {
    AvpStr base = name;
    bool special = false;
    if (nscope) {
        AvpStr head, rest;
        avp_str_partition(scope[nscope - 1], AVP_S("<"), &head, &rest);
        if (avp_str_eq(name, head)) {
            base = AVP_S("__ct");
            special = true;
        } else if (name.n == head.n + 1 && name.p[0] == '~' && memcmp(name.p + 1, head.p, head.n) == 0) {
            base = AVP_S("__dt");
            special = true;
        }
    }
    if (!special && avp_str_startswith(name, AVP_S("operator"))) {
        AvpStr tail = slice(name, 8, name.n);
        const char *code = op_code(name);
        if (!code) code = op_code(avp_str_concat(a, AVP_S("operator"), avp_str_strip(tail)));
        if (code) base = avp_str_concat(a, AVP_S("__"), avp_str(code));
        else base = avp_str_concat(a, AVP_S("__op"), mtype(a, tail, 0));
    }
    AvpBuf pb;
    avp_buf_init(&pb, a);
    for (size_t i = 0; i < nparams; i++) avp_buf_put(&pb, mtype(a, avp_str(params[i]), 0));
    AvpStr ps = pb.n ? avp_buf_str(&pb) : AVP_S("v");
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_put(&b, base);
    avp_buf_put(&b, AVP_S("__"));
    avp_buf_put(&b, mscope(a, scope, nscope, 0));
    if (is_const) avp_buf_putc(&b, 'C');
    if (is_volatile) avp_buf_putc(&b, 'V');
    avp_buf_putc(&b, 'F');
    avp_buf_put(&b, ps);
    return avp_buf_str(&b);
}

const char *shsm_mangle(AvpArena *a, const char *name, const char *scope, const char *const *params, size_t nparams,
                        bool is_const, bool is_volatile) {
    size_t n = 0;
    AvpStr *parts = scope ? split_colons(a, avp_str(scope), &n) : NULL;
    return avp_cz(a, mangle_impl(a, avp_str(name), parts, n, params, nparams, is_const, is_volatile));
}

const char *shsm_mangle_parts(AvpArena *a, const char *name, const char *const *scope, size_t nscope,
                              const char *const *params, size_t nparams, bool is_const, bool is_volatile) {
    AvpStr *parts = AVP_NEWN(a, AvpStr, nscope + 1);
    for (size_t i = 0; i < nscope; i++) parts[i] = avp_str(scope[i]);
    return avp_cz(a, mangle_impl(a, avp_str(name), parts, nscope, params, nparams, is_const, is_volatile));
}
