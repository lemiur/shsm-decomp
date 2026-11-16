/* rules_check_tokens: the whole-file token checks of check_rules.py: check_tokens (L2311), check_identifier and
 * flush_identifiers (R7, L2329-2356), the R4 pass (_skip_operand, offset macros, _pointer_names, _operand_start,
 * _pointer_term, check_offset_macro_uses, check_r4: L2396-2687), check_volatile with _is_delay_counter and
 * _volatile_kind (R17, L2690-2813), check_casts and _local_type (R19, R26, L2816-2884). */
#include "rules_check.h"

#define TEQ RL_TEQ
static bool is_lit(const RlTok *t) { return t->k == RL_NUM || t->k == RL_STR || t->k == RL_CHR; }
static bool is_operand(const RlTok *t) { return t->k == RL_ID || is_lit(t); }

/* ---- R7 identifiers ---- */
static const struct {
    const char *re, *label;
} PLACEHOLDERS[] = {
    {"fn_[0-9A-F]{8}", "address-derived function name"},
    {"lbl_[0-9A-F]{8}", "address-derived label/object name"},
    {"unk_[0-9A-Fa-f]+", "unknown-member placeholder"},
    {"unk_[A-Z][A-Za-z0-9]*", "invented-role class placeholder (unk_<Role>)"},
    {"field_0x[0-9A-Fa-f]+", "offset-derived field name"},
    {"(?:dtor|sinit|jumptable|gap)_[0-9A-Fa-f]{6,8}", "dtk-generated name"},
    {"stringBase\\d+", "dtk-generated string base"},
    {"(?:FUN|DAT|LAB|PTR|SUB)_[0-9A-Fa-f]{4,8}", "disassembler placeholder"},
    {"sub_[0-9A-Fa-f]{4,8}", "disassembler placeholder"},
    {"(?:local|param)_\\w+", "disassembler placeholder"},
    {"[a-z]{0,2}Var\\d+|uStack\\w*", "disassembler placeholder"},
    {"(?:temp|tmp|ptr|result|var|v)\\d+", "numbered generic name"},
    {"var_\\w+", "m2c-style variable name"},
    {"temp_\\w+", "m2c-style temporary name"},
    {"phi_\\w+|saved_reg\\w*", "m2c-style artefact"},
    {"sp(?:0x)?[0-9A-F]{1,4}", "m2c-style stack slot"},
    {"M2C_\\w+|MIPS2C_\\w+", "m2c artefact"},
};
#define N_PLACEHOLDERS (sizeof PLACEHOLDERS / sizeof PLACEHOLDERS[0])

static const ShsmRe *any_placeholder(void) {
    static const ShsmRe *re;
    if (!re) {
        AvpBuf b;
        avp_buf_init(&b, avp_arena());
        for (size_t i = 0; i < N_PLACEHOLDERS; i++) {
            if (i) avp_buf_putc(&b, '|');
            avp_buf_printf(&b, "(?:%s)", PLACEHOLDERS[i].re);
        }
        avp_buf_putc(&b, 0);
        re = shsm_re(b.p, 0);
    }
    return re;
}

void rl_check_identifier(RlChecker *ck, RlParsed *P, const RlTok *t) {
    AvpStr name = t->t;
    AvpStr msg = {0};
    if (shsm_re_fullmatch(any_placeholder(), name, 0, AVP_END, NULL)) {
        for (size_t i = 0; i < N_PLACEHOLDERS; i++) {
            if (shsm_re_fullmatch(shsm_re(PLACEHOLDERS[i].re, 0), name, 0, AVP_END, NULL)) {
                msg = rl_fmt(ck->a, "placeholder name '%.*s' (%s): open naming work", AVP_SF(name), PLACEHOLDERS[i].label);
                break;
            }
        }
    }
    if (!msg.p && name.n >= 6) {
        bool digit = false;
        for (size_t i = 0; i < name.n; i++) digit |= name.p[i] >= '0' && name.p[i] <= '9';
        if (digit && shsm_re_fullmatch(shsm_re("[A-Za-z_]*?_?(?=[0-9a-fA-F]*[0-9])[0-9a-fA-F]{6,8}", 0), name, 0, AVP_END,
                                       NULL))
            msg = rl_fmt(ck->a, "address-like name '%.*s': open naming work", AVP_SF(name));
    }
    if (!msg.p) return;
    i64 fi = rl_func_at(P, t->line);
    AvpStr fname = fi >= 0 ? P->p.funcs.v[fi].name : AVP_S("");
    AvpArena *s = avp_scratch();
    AvpArenaMark mk = avp_arena_mark(s);
    AvpStr key = rl_fmt(s, "%.*s\x01%.*s", AVP_SF(fname), AVP_SF(name));
    bool created;
    AvpVal *v = avp_map_put(ck->ph, key, &created);
    avp_arena_reset(s, mk);
    if (created) {
        v->i = (i64)ck->ph_v.n;
        RlIdent *id = RL_PUSH(ck->a, &ck->ph_v);
        id->line = t->line;
        id->count = 1;
        id->msg = msg;
    } else {
        ck->ph_v.v[v->i].count++;
    }
}

void rl_flush_identifiers(RlChecker *ck, RlParsed *P) {
    for (size_t i = 0; i < ck->ph_v.n; i++) {
        RlIdent *id = &ck->ph_v.v[i];
        AvpStr msg = id->msg;
        if (id->count > 1) msg = rl_fmt(ck->a, "%.*s [%d uses]", AVP_SF(msg), id->count);
        rl_add(ck, P, id->line, "R7", "checked", msg, (AvpStr){0});
    }
}

/* ---- R4 ---- */
static size_t skip_operand(const RlTok *toks, size_t n, const i32 *match, size_t p) {
    static const char *const PRE[] = {"&", "*", "-", "!", "~", "+", NULL};
    size_t q = p;
    while (q < n && rl_tin(&toks[q], PRE)) q++;
    if (q >= n) return p;
    if (TEQ(&toks[q], "(") && match[q] >= 0) q = (size_t)match[q] + 1;
    else if (is_operand(&toks[q])) q++;
    else return p;
    while (q < n) {
        const RlTok *x = &toks[q];
        if ((TEQ(x, "->") || TEQ(x, ".") || TEQ(x, "::")) && q + 1 < n && toks[q + 1].k == RL_ID) q += 2;
        else if ((TEQ(x, "[") || TEQ(x, "(")) && match[q] >= 0) q = (size_t)match[q] + 1;
        else break;
    }
    return q;
}

static bool is_operand_end(const RlTok *t) {
    return (is_operand(t) && !TEQ(t, "return") && !TEQ(t, "case")) || TEQ(t, ")") || TEQ(t, "]");
}

static const char *const NOT_CALLEE[] = {"return", "if", "while", "for", "switch", "case", "else", "do", "sizeof", NULL};
static const char *const NOT_DECL[] = {"return", "else", "case", "goto", "delete", "new", "sizeof", "throw",
                                       "typedef", "if", "while", "for", "switch", "do", NULL};
static const char *const OFFSET_ADDR_INTS[] = {"u32", "U32", "uint32_t", "uintptr_t", "intptr_t", "uptr", NULL};

static AvpSet *pointer_names(RlChecker *ck, RlParsed *P, const RlFunc *f) {
    AvpStr key = rl_fmt(ck->a, "%d\x01%.*s", f->line, AVP_SF(f->name));
    AvpVal *got = avp_map_get(ck->ptr_names, key);
    if (got) return got->p;
    AvpSet *names = avp_set_new();
    for (size_t i = 0; i < f->params.n; i++)
        if (f->params.v[i].name.n && avp_str_contains(f->params.v[i].type, AVP_S("*")))
            avp_set_add(names, f->params.v[i].name);
    const RlTok *toks = P->toks.v;
    i64 n = (i64)P->toks.n;
    i64 hi = f->body_end > 0 ? f->body_end : n;
    i64 lo = (f->body > 0 ? f->body : 0) + 1;
    i64 top = hi < n - 3 ? hi : n - 3;
    static const char *const AFTER[] = {"=", ";", ",", "[", ")", NULL};
    for (i64 j = lo; j < top; j++) {
        if (!TEQ(&toks[j], "*") || toks[j + 1].k != RL_ID || !rl_tin(&toks[j + 2], AFTER)) continue;
        i64 s = j - 1;
        bool ok = toks[s].k == RL_ID || TEQ(&toks[s], "*");
        while (ok && s >= 0 && (toks[s].k == RL_ID || TEQ(&toks[s], "*") || TEQ(&toks[s], ",") || TEQ(&toks[s], "::"))) {
            if (rl_tin(&toks[s], NOT_DECL)) {
                ok = false;
                break;
            }
            s--;
        }
        if (!ok || s < 0 || j - 1 == s) continue;
        const RlTok *b = &toks[s];
        if (TEQ(b, ";") || TEQ(b, "{") || TEQ(b, "}") || (TEQ(b, "(") && s > 0 && TEQ(&toks[s - 1], "for")))
            avp_set_add(names, toks[j + 1].t);
    }
    avp_map_put(ck->ptr_names, key, NULL)->p = names;
    return names;
}

/* _operand_start: -1 for None. rev is match (pair_brackets maps both ways). */
static i64 operand_start(const RlTok *toks, size_t n, const i32 *match, i64 end) {
    i64 j = end;
    for (;;) {
        const RlTok *t = &toks[j];
        if (TEQ(t, ")") || TEQ(t, "]")) {
            i64 o = match[j];
            if (o < 0) return -1;
            j = o;
            if (TEQ(t, "]")) {
                if (j == 0) return -1;
                j--;
                continue;
            }
            if (j > 0) {
                const RlTok *p = &toks[j - 1];
                if (p->k == RL_ID && !rl_tin(p, NOT_CALLEE)) {
                    j--;
                    continue;
                }
                if ((TEQ(p, ")") || TEQ(p, "]")) &&
                    !(TEQ(p, ")") && match[j - 1] >= 0 && rl_cast_info(toks, n, match, (size_t)match[j - 1], false) >= 0)) {
                    j--;
                    continue;
                }
            }
            return j;
        }
        if (is_operand(t)) {
            if (j >= 2 && (TEQ(&toks[j - 1], ".") || TEQ(&toks[j - 1], "->") || TEQ(&toks[j - 1], "::"))) {
                j -= 2;
                continue;
            }
            return j;
        }
        return -1;
    }
}

static bool unary_at(const RlTok *toks, i64 x) { return x >= 0 && (x == 0 || !is_operand_end(&toks[x - 1])); }

static bool pointer_term(const RlTok *toks, size_t n, const i32 *match, i64 s, i64 e, AvpSet *ptrs) {
    if (s > 0 && TEQ(&toks[s - 1], ")")) {
        i64 o = match[s - 1];
        i32 c = o >= 0 ? rl_cast_info(toks, n, match, (size_t)o, false) : -1;
        if (c >= 0) {
            static const char *const SKIP[] = {"const", "volatile", "unsigned", "signed", NULL};
            bool any = false, all = true;
            for (i64 k = o + 1; k < c; k++) {
                if (toks[k].k != RL_ID || rl_tin(&toks[k], SKIP)) continue;
                any = true;
                if (!rl_tin(&toks[k], OFFSET_ADDR_INTS)) all = false;
            }
            if (o > 0 && TEQ(&toks[o - 1], "*") && unary_at(toks, o - 1)) return false;
            return TEQ(&toks[c - 1], "*") || (any && all);
        }
    }
    if (s > 0 && TEQ(&toks[s - 1], "&") && unary_at(toks, s - 1)) return true;
    if (s > 0 && TEQ(&toks[s - 1], "*") && unary_at(toks, s - 1)) return false;
    if (s == e && toks[s].k == RL_ID) return TEQ(&toks[s], "this") || (ptrs && avp_set_has(ptrs, toks[s].t));
    return false;
}

static void check_offset_macro_uses(RlChecker *ck, RlParsed *P, const RlTok *toks, size_t n, const i32 *match,
                                    bool in_macro, const bool *skip) {
    AvpSet *names = ck->offset_names;
    if (!names || !avp_set_len(names)) return;
    static const char *const NOT_AFTER[] = {"*", "/", "%", "(", "[", ".", "->", "++", "--", NULL};
    static const char *const SIGNS[] = {"+", "-", "+=", "-=", NULL};
    static const char *const PREFIX[] = {"&", "*", "-", "+", "~", "!", NULL};
    for (size_t k = 0; k < n; k++) {
        const RlTok *t = &toks[k];
        if (t->k != RL_ID || k < 2 || skip[k] || !avp_set_has(names, t->t)) continue;
        if (k + 1 < n && rl_tin(&toks[k + 1], NOT_AFTER)) continue;
        const RlTok *sign = &toks[k - 1];
        if (!rl_tin(sign, SIGNS) || !is_operand_end(&toks[k - 2])) continue;
        AvpSet *ptrs = NULL;
        if (!in_macro) {
            i64 fi = rl_func_at(P, t->line);
            if (fi >= 0) ptrs = pointer_names(ck, P, &P->p.funcs.v[fi]);
        }
        i64 e = (i64)k - 2;
        bool hit = false;
        if (TEQ(sign, "+=") || TEQ(sign, "-=")) {
            i64 s = operand_start(toks, n, match, e);
            hit = s >= 0 && s == e && toks[s].k == RL_ID && ptrs && avp_set_has(ptrs, toks[s].t);
        } else {
            bool after_minus = false;
            while (e >= 0) {
                i64 s = operand_start(toks, n, match, e);
                if (s < 0) break;
                if (!after_minus && pointer_term(toks, n, match, s, e, ptrs)) {
                    hit = true;
                    break;
                }
                i64 ps = s;
                while (ps > 0) {
                    if (rl_tin(&toks[ps - 1], PREFIX) && (ps - 1 == 0 || !is_operand_end(&toks[ps - 2]))) ps--;
                    else if (TEQ(&toks[ps - 1], ")") && match[ps - 1] >= 0 &&
                             rl_cast_info(toks, n, match, (size_t)match[ps - 1], false) >= 0)
                        ps = match[ps - 1];
                    else break;
                }
                if (ps >= 2 && (TEQ(&toks[ps - 1], "+") || TEQ(&toks[ps - 1], "-")) && is_operand_end(&toks[ps - 2])) {
                    after_minus = TEQ(&toks[ps - 1], "-");
                    e = ps - 2;
                } else {
                    break;
                }
            }
        }
        if (!hit || rl_has_note(P, t->line, rl_re_byteptr_note(), true)) continue;
        rl_add(ck, P, t->line, "R4", "match",
               rl_fmt(ck->a, "offset macro '%.*s' offsets a pointer: use typed members (offsetof only in field tables)",
                      AVP_SF(t->t)),
               (AvpStr){0});
    }
}

static const char *const BYTE_POINTEE[] = {"char", "u8", "s8", "BYTE", "byte", "uchar", "uint8_t", "int8_t",
                                           "uint8", "int8", "void", "U8", "S8", NULL};
static const char *const ADDR_INTS[] = {"u32", "s32", "int", "unsigned", "long", "uintptr_t", "intptr_t",
                                        "size_t", "uptr", "ptrdiff_t", "U32", "uint32_t", NULL};

static bool offset_const(AvpStr s) { /* OFFSET_CONST.fullmatch: [A-Z_][A-Z0-9_]+ */
    if (s.n < 2) return false;
    if (!((s.p[0] >= 'A' && s.p[0] <= 'Z') || s.p[0] == '_')) return false;
    for (size_t i = 1; i < s.n; i++)
        if (!((s.p[i] >= 'A' && s.p[i] <= 'Z') || (s.p[i] >= '0' && s.p[i] <= '9') || s.p[i] == '_')) return false;
    return true;
}

void rl_check_r4(RlChecker *ck, RlParsed *P, const RlTok *toks, size_t n, const i32 *match, bool in_macro) {
    AvpArena *a = ck->a;
    bool *named_seen = avp_alloc(a, n + 1);
    static const char *const NOT_ID[] = {"const", "volatile", "struct", "union", "unsigned", "signed", NULL};
    for (size_t i = 0; i < n; i++) {
        if (!TEQ(&toks[i], "(")) continue;
        i32 ci = rl_cast_info(toks, n, match, i, false);
        if (ci < 0) continue;
        size_t c = (size_t)ci;
        /* ids, stars over inner = toks[i+1 .. c) */
        size_t nid = 0, stars = 0;
        const RlTok *last_id = NULL;
        bool has_unsigned = false, addr_all = true, addr_any = false;
        for (size_t k = i + 1; k < c; k++) {
            const RlTok *x = &toks[k];
            if (TEQ(x, "*")) stars++;
            if (TEQ(x, "unsigned")) has_unsigned = true;
            if (x->k == RL_ID && !rl_tin(x, NOT_ID)) {
                nid++;
                last_id = x;
            }
            if (x->k == RL_ID && !TEQ(x, "const") && !TEQ(x, "volatile")) {
                addr_any = true;
                if (!rl_tin(x, ADDR_INTS) && !TEQ(x, "unsigned") && !TEQ(x, "signed")) addr_all = false;
            }
        }
        (void)addr_any;
        bool is_byte_ptr = stars == 1 && nid >= 1 && rl_tin(last_id, BYTE_POINTEE) && TEQ(&toks[c - 1], "*");
        if (stars == 1 && has_unsigned && nid && TEQ(last_id, "char")) is_byte_ptr = true;
        bool is_addr_int = stars == 0 && nid && addr_all;
        if (!(is_byte_ptr || is_addr_int)) continue;
        size_t op_end = skip_operand(toks, n, match, c + 1);
        if (op_end == c + 1 || op_end >= n) continue;
        const RlTok *sign = &toks[op_end];
        if (!TEQ(sign, "+") && !TEQ(sign, "-")) continue;
        size_t q = op_end + 1;
        if (q >= n) continue;
        const RlTok *off = &toks[q];
        bool named = false, fixed;
        if (off->k == RL_NUM) {
            fixed = true;
        } else if (off->k == RL_ID && offset_const(off->t) &&
                   !(q + 1 < n && (TEQ(&toks[q + 1], "(") || TEQ(&toks[q + 1], "[") || TEQ(&toks[q + 1], ".") ||
                                   TEQ(&toks[q + 1], "->")))) {
            fixed = named = true;
        } else {
            fixed = false;
        }
        if (!fixed) continue;
        if (named && shsm_re_search(shsm_re("(?i)header|hdr", 0), off->t, 0, AVP_END, NULL)) continue;
        if (i == 0 || !TEQ(&toks[i - 1], "(")) continue;
        size_t g_open = i - 1;
        i64 g_close = match[g_open];
        if (g_close < 0 || g_close < (i64)q) continue;
        bool deref = false, cast_ok = false;
        if (g_open >= 1 && TEQ(&toks[g_open - 1], ")")) {
            i64 co = match[g_open - 1];
            if (co >= 0) {
                i32 oc = rl_cast_info(toks, n, match, (size_t)co, false);
                bool star = false;
                for (i64 k = co + 1; oc >= 0 && k < oc; k++) star |= TEQ(&toks[k], "*");
                if (oc >= 0 && star) {
                    cast_ok = true;
                    if (co >= 1 && TEQ(&toks[co - 1], "*") &&
                        ((co < 2 || (toks[co - 2].k != RL_ID && toks[co - 2].k != RL_NUM && !TEQ(&toks[co - 2], ")") &&
                                     !TEQ(&toks[co - 2], "]"))) ||
                         (co >= 2 && (TEQ(&toks[co - 2], "return") || TEQ(&toks[co - 2], "="))))) {
                        deref = true;
                    } else if (co >= 1 && TEQ(&toks[co - 1], "(")) {
                        i64 wrap_close = match[co - 1];
                        if (wrap_close >= 0 && wrap_close == g_close + 1 && wrap_close + 1 < (i64)n &&
                            (TEQ(&toks[wrap_close + 1], "->") || TEQ(&toks[wrap_close + 1], "[") ||
                             TEQ(&toks[wrap_close + 1], ".")))
                            deref = true;
                    }
                }
            }
        }
        if (!cast_ok) continue;
        i32 line = toks[i].line;
        bool on_this = c + 1 < n && TEQ(&toks[c + 1], "this");
        const char *where = on_this ? "this" : "a byte pointer";
        if (named) named_seen[q] = true;
        if (deref) {
            if (rl_has_note(P, line, rl_re_byteptr_note(), true)) continue;
            rl_add(ck, P, line, "R4", "match",
                   rl_fmt(a, "fixed-offset access: %s '%.*s %.*s' dereferenced through a cast; use a typed member", where,
                          AVP_SF(sign->t), AVP_SF(off->t)),
                   (AvpStr){0});
        } else if (off->k == RL_NUM) {
            bool void_outer = false;
            if (g_open >= 1 && TEQ(&toks[g_open - 1], ")")) {
                i64 co2 = match[g_open - 1];
                if (co2 >= 0 && (i64)g_open - 1 - (co2 + 1) == 2 && TEQ(&toks[co2 + 1], "void") && TEQ(&toks[co2 + 2], "*"))
                    void_outer = true;
            }
            AvpStr digits = avp_str_rstrip_chars(off->t, AVP_S("uUlL"));
            i64 v;
            bool overflow = false, big;
            if (avp_parse_int(digits, 0, &v, &overflow)) big = v >= 0x10000;
            else big = overflow;
            if (rl_has_note(P, line, rl_re_byteptr_note(), true) || void_outer || big) continue;
            rl_add(ck, P, line, "R4", "advisory",
                   rl_fmt(a, "pointer formed from %s plus the literal offset %.*s; check it is not a struct member", where,
                          AVP_SF(off->t)),
                   (AvpStr){0});
        }
    }
    check_offset_macro_uses(ck, P, toks, n, match, in_macro, named_seen);
    /* *(prim*)this */
    static const char *const SCALAR_EXTRA[] = {"*", "const", "volatile", NULL};
    for (size_t i = 0; i + 3 < n; i++) {
        if (!TEQ(&toks[i], "*") || !TEQ(&toks[i + 1], "(") || match[i + 1] < 0) continue;
        size_t c = (size_t)match[i + 1];
        if (!(c + 1 < n && TEQ(&toks[c + 1], "this"))) continue;
        if (c <= i + 2 || !TEQ(&toks[c - 1], "*")) continue;
        size_t nstar = 0;
        bool ok = true;
        for (size_t k = i + 2; k < c; k++) {
            if (TEQ(&toks[k], "*")) nstar++;
            if (!rl_tin(&toks[k], RL_SCALAR_WORDS) && !rl_tin(&toks[k], SCALAR_EXTRA)) ok = false;
        }
        if (!ok || nstar != 1) continue;
        const RlTok *prev = i ? &toks[i - 1] : NULL;
        if (!prev || (prev->k != RL_ID && prev->k != RL_NUM) || TEQ(prev, "return"))
            rl_add(ck, P, toks[i].line, "R4", "match", AVP_S("'this' read as a scalar through a cast: use a typed member"),
                   (AvpStr){0});
    }
}

/* ---- volatile (R17) ---- */
static bool is_delay_counter(const RlTok *toks, size_t n, const i32 *match, const RlFunc *fn, size_t i) {
    size_t j = i + 1;
    static const char *const STOP[] = {";", "=", ",", "[", "(", "{", NULL};
    while ((i64)j < fn->body_end && !rl_tin(&toks[j], STOP)) j++;
    if ((i64)j >= fn->body_end || !TEQ(&toks[j], ";") || toks[j - 1].k != RL_ID || j - 1 == i) return false;
    size_t decl = j - 1;
    AvpStr name = toks[decl].t;
    if (rl_volatile_type(name) || avp_str_eqz(name, "volatile") || avp_str_eqz(name, "const")) return false;
    i64 umin = -1, umax = -1;
    for (i64 k = fn->body + 1; k < fn->body_end; k++)
        if ((size_t)k != decl && toks[k].k == RL_ID && avp_str_eq(toks[k].t, name)) {
            if (umin < 0) umin = k;
            umax = k;
        }
    if (umin < 0) return false;
    for (i64 f_i = fn->body + 1; f_i < fn->body_end; f_i++) {
        if (!TEQ(&toks[f_i], "for") || !TEQ(&toks[f_i + 1], "(")) continue;
        i64 lp = f_i + 1, rp = match[lp];
        if (rp < 0 || !(lp < umin && umax < rp)) continue;
        i64 b = rp + 1;
        if (b >= (i64)n) return false;
        if (!(TEQ(&toks[b], ";") || (TEQ(&toks[b], "{") && match[b] == b + 1))) return false;
        /* the header's three clauses at top-level ';' */
        i64 cl[4][2];
        int nc = 0;
        i64 depth = 0, start = lp + 1;
        for (i64 k = lp + 1; k < rp; k++) {
            const RlTok *x = &toks[k];
            if (TEQ(x, "(") || TEQ(x, "[") || TEQ(x, "{")) depth++;
            else if (TEQ(x, ")") || TEQ(x, "]") || TEQ(x, "}")) depth--;
            if (TEQ(x, ";") && depth == 0) {
                if (nc >= 2) return false; /* a fourth clause */
                cl[nc][0] = start;
                cl[nc][1] = k;
                nc++;
                start = k + 1;
            }
        }
        if (nc != 2) return false;
        cl[2][0] = start;
        cl[2][1] = rp;
        i64 i0 = cl[0][0], i1 = cl[0][1];
        if (i1 - i0 < 3 || !avp_str_eq(toks[i0].t, name) || !TEQ(&toks[i0 + 1], "=")) return false;
        for (i64 k = i0 + 2; k < i1; k++)
            if (avp_str_eq(toks[k].t, name)) return false;
        bool has = false;
        static const char *const ASG[] = {"=", "++", "--", "+=", "-=", NULL};
        for (i64 k = cl[1][0]; k < cl[1][1]; k++) {
            if (avp_str_eq(toks[k].t, name)) has = true;
            if (rl_tin(&toks[k], ASG)) return false;
        }
        if (!has) return false;
        i64 s0 = cl[2][0], s1 = cl[2][1];
        if (s1 - s0 != 2) return false;
        if (!((avp_str_eq(toks[s0].t, name) && TEQ(&toks[s0 + 1], "++")) ||
              (TEQ(&toks[s0], "++") && avp_str_eq(toks[s0 + 1].t, name))))
            return false;
        return true;
    }
    return false;
}

/* A function-like macro some definition of which (in the file's include closure) expands to nothing: the SDK's
 * release `#define ASSERTMSG1(exp, msg, param1) ((void)0)`; also an empty body and `(void)0`. */
static bool release_noop_macro(RlChecker *ck, RlParsed *P, AvpStr name) {
    const AvpStrVec *cl = rl_checker_closure(ck, P->path);
    for (size_t qi = 0; qi < cl->n; qi++) {
        RlParsed *Q = rl_checker_parse(ck, cl->v[qi]);
        for (size_t di = 0; di < Q->directives.n; di++) {
            const RlDirective *d = &Q->directives.v[di];
            const RlTok *toks = d->toks.v;
            size_t n = d->toks.n;
            if (!avp_str_eqz(d->name, "define") || n < 3 || toks[0].k != RL_ID || !avp_str_eq(toks[0].t, name) ||
                !TEQ(&toks[1], "(") || toks[1].line != toks[0].line)
                continue;
            size_t k = 2;
            while (k < n && !TEQ(&toks[k], ")")) k++;
            if (k >= n) continue;
            char body[32];
            size_t bl = 0;
            bool fits = true;
            for (size_t q = k + 1; q < n && fits; q++) {
                if (bl + toks[q].t.n >= sizeof body) fits = false;
                else {
                    memcpy(body + bl, toks[q].t.p, toks[q].t.n);
                    bl += toks[q].t.n;
                }
            }
            if (!fits) continue;
            body[bl] = 0;
            if (!strcmp(body, "") || !strcmp(body, "((void)0)") || !strcmp(body, "(void)0")) return true;
        }
    }
    return false;
}

/* The second volatile-local shape R17 accepts (user's ruling, 2026-11-15): a scalar declared without an initialiser,
 * assigned exactly once by a statement `v = <expr>;` whose right side does not read it, and read only inside the
 * arguments of macros that compile to nothing in release (tp's `volatile u32 mail` in __DSP_boot_task, read only by
 * ASSERTMSG1). At least one such read. */
static bool is_assert_only(RlChecker *ck, RlParsed *P, const RlFunc *fn, size_t i) {
    const RlTok *toks = P->toks.v;
    size_t j = i + 1;
    static const char *const STOP[] = {";", "=", ",", "[", "(", "{", NULL};
    while ((i64)j < fn->body_end && !rl_tin(&toks[j], STOP)) j++;
    if ((i64)j >= fn->body_end || !TEQ(&toks[j], ";") || toks[j - 1].k != RL_ID || j - 1 == i) return false;
    size_t decl = j - 1;
    AvpStr name = toks[decl].t;
    if (rl_volatile_type(name) || avp_str_eqz(name, "volatile") || avp_str_eqz(name, "const")) return false;
    for (size_t q = i; q < decl; q++)
        if (TEQ(&toks[q], "*")) return false; /* a scalar, not a pointer */
    int stores = 0, reads = 0;
    for (i64 k = fn->body + 1; k < fn->body_end; k++) {
        if ((size_t)k == decl || toks[k].k != RL_ID || !avp_str_eq(toks[k].t, name)) continue;
        const RlTok *pv = &toks[k - 1];
        if ((TEQ(pv, ";") || TEQ(pv, "{") || TEQ(pv, "}")) && TEQ(&toks[k + 1], "=")) {
            i64 e = k + 2;
            while (e < fn->body_end && !TEQ(&toks[e], ";")) {
                if (avp_str_eq(toks[e].t, name)) return false;
                if ((TEQ(&toks[e], "(") || TEQ(&toks[e], "[")) && P->match[e] > e) e = P->match[e];
                e++;
            }
            stores++;
            k = e;
            continue;
        }
        /* a read: some enclosing '(' must open the arguments of a release no-op macro */
        bool in_assert = false;
        i64 depth = 0;
        for (i64 q = k - 1; q > fn->body && !in_assert; q--) {
            if (TEQ(&toks[q], ")") || TEQ(&toks[q], "]")) depth++;
            else if (TEQ(&toks[q], "(") || TEQ(&toks[q], "[")) {
                if (depth > 0) depth--;
                else if (TEQ(&toks[q], "(") && P->match[q] > k && toks[q - 1].k == RL_ID &&
                         release_noop_macro(ck, P, toks[q - 1].t))
                    in_assert = true;
            } else if (depth == 0 && (TEQ(&toks[q], ";") || TEQ(&toks[q], "{") || TEQ(&toks[q], "}")))
                break;
        }
        if (!in_assert) return false;
        reads++;
    }
    return stores == 1 && reads > 0;
}

static const char *volatile_kind(const RlTok *toks, size_t n, size_t i, size_t stmt_start) {
    if (i > 0 && TEQ(&toks[i - 1], "(")) return "cast";
    i64 depth = 0;
    for (i64 k = (i64)i - 1; k >= (i64)stmt_start; k--) {
        if (TEQ(&toks[k], ")")) depth++;
        else if (TEQ(&toks[k], "(")) {
            if (depth == 0) return "cast";
            depth--;
        }
    }
    static const char *const STOP[] = {";", "=", ",", "[", "{", ")", NULL};
    bool saw_star = false, before_star = false;
    for (size_t j = i + 1; j < n && !rl_tin(&toks[j], STOP); j++)
        if (TEQ(&toks[j], "*")) saw_star = true;
    for (size_t k = stmt_start; k < i; k++)
        if (TEQ(&toks[k], "*")) before_star = true;
    if (saw_star && !before_star) return "pointer";
    return "variable";
}

static void check_volatile(RlChecker *ck, RlParsed *P) {
    const RlTok *toks = P->toks.v;
    size_t n = P->toks.n;
    const RlFuncs *funcs = &P->p.funcs;
    for (size_t i = 0; i < n; i++) {
        const RlTok *t = &toks[i];
        if (t->k != RL_ID) continue;
        if (!(TEQ(t, "volatile") || TEQ(t, "__volatile__") || rl_volatile_type(t->t))) continue;
        size_t s = i;
        while (s > 0 && !TEQ(&toks[s - 1], ";") && !TEQ(&toks[s - 1], "{") && !TEQ(&toks[s - 1], "}")) s--;
        if (TEQ(&toks[s], "typedef")) continue;
        if (t->t.p[0] == 'v' && rl_volatile_type(t->t) && i > 0 &&
            (TEQ(&toks[i - 1], ".") || TEQ(&toks[i - 1], "->") || TEQ(&toks[i - 1], "::")))
            continue;
        if (i > 0 && (TEQ(&toks[i - 1], "asm") || TEQ(&toks[i - 1], "__asm") || TEQ(&toks[i - 1], "__asm__"))) continue;
        const RlFunc *fn = NULL;
        bool in_params = false;
        for (size_t k = 0; k < funcs->n; k++) {
            const RlFunc *f = &funcs->v[k];
            if (!fn && f->body < (i64)i && (i64)i < f->body_end) fn = f;
            if (f->lparen < (i64)i && (i64)i < f->rparen) in_params = true;
        }
        const char *kind = volatile_kind(toks, n, i, s);
        bool has_note = rl_has_note(P, t->line, rl_re_volatile_note(), true);
        AvpStr name = rl_volatile_decl_name(toks, n, i);
        if (name.p && name.n && avp_set_has(ck->noted_volatile, name)) has_note = true;
        if (fn && !strcmp(kind, "variable") && !in_params) {
            if (rl_has_note(P, t->line, rl_re_delay_note(), true) && is_delay_counter(toks, n, P->match, fn, i)) continue;
            if (rl_has_note(P, t->line, rl_re_assert_note(), true) && is_assert_only(ck, P, fn, i)) continue;
            rl_add(ck, P, t->line, "R17", "match",
                   AVP_S("volatile local variable: a volatile carrier, not source (only an SDK delay-loop counter with a "
                         "'// volatile: delay loop, <reference>' note passes)"),
                   (AvpStr){0});
            continue;
        }
        if (!has_note)
            rl_add(ck, P, t->line, "R17", "match",
                   rl_fmt(ck->a, "volatile %s without a '// volatile: <evidence>' note", kind), (AvpStr){0});
    }
}

/* ---- casts (R19, R26) ---- */
static AvpStr inner_text(AvpArena *a, const RlTok *toks, size_t i, size_t c) {
    return rl_strip_ws(a, rl_ptext(a, toks + i + 1, c - i - 1));
}

static i64 enclosing_body(const RlParsed *P, size_t i) {
    for (size_t k = 0; k < P->p.funcs.n; k++)
        if (P->p.funcs.v[k].body < (i64)i && (i64)i < P->p.funcs.v[k].body_end) return (i64)k;
    return -1;
}

static AvpStr local_type(RlChecker *ck, RlParsed *P, const RlFunc *fn, AvpStr name) {
    AvpArena *s = avp_scratch();
    AvpArenaMark mk = avp_arena_mark(s);
    RlRange *st;
    size_t ns = rl_statements(P->toks.v, P->match, fn->body + 1, fn->body_end, &st);
    AvpStr res = {0};
    bool done = false;
    for (size_t k = 0; k < ns && !done; k++) {
        RlDecl *d;
        size_t nd = rl_parse_decl(s, P->toks.v + st[k].lo, (size_t)(st[k].hi - st[k].lo), &d);
        for (size_t q = 0; q < nd; q++) {
            if (avp_str_eq(d[q].name, name)) {
                if (d[q].type.n) res = rl_strip_ws(ck->a, avp_strn(avp_cz(ck->a, d[q].type), d[q].type.n));
                done = true;
                break;
            }
        }
    }
    avp_arena_reset(s, mk);
    return res;
}

static void check_casts(RlChecker *ck, RlParsed *P) {
    const RlTok *toks = P->toks.v;
    size_t n = P->toks.n;
    const i32 *match = P->match;
    AvpArena *a = ck->a;
    static const char *const NOT_AFTER[] = {"->", ".", "[", "(", "++", "--", NULL};
    for (size_t i = 0; i < n; i++) {
        if (!TEQ(&toks[i], "(")) continue;
        i32 ci = rl_cast_info(toks, n, match, i, false);
        if (ci < 0) continue;
        size_t c = (size_t)ci;
        if (c + 1 < n && TEQ(&toks[c + 1], "(")) {
            i32 c2 = rl_cast_info(toks, n, match, c + 1, true);
            if (c2 >= 0) {
                AvpStr sa = inner_text(a, toks, i, c), sb = inner_text(a, toks, c + 1, (size_t)c2);
                if (avp_str_contains(sa, AVP_S("*")) && avp_str_contains(sb, AVP_S("*")))
                    rl_add(ck, P, toks[i].line, "R19", "checked",
                           rl_fmt(a, "cast chain (%.*s)(%.*s): retype the source so one cast, or none, is needed",
                                  AVP_SF(sa), AVP_SF(sb)),
                           (AvpStr){0});
            }
        }
        if (c + 2 < n && toks[c + 1].k == RL_ID && !rl_tin(&toks[c + 2], NOT_AFTER)) {
            i64 fi = enclosing_body(P, i);
            if (fi >= 0) {
                const RlFunc *fn = &P->p.funcs.v[fi];
                AvpStr nm = toks[c + 1].t;
                AvpStr ty = {0};
                for (size_t q = fn->params.n; q-- > 0;)
                    if (fn->params.v[q].name.n && avp_str_eq(fn->params.v[q].name, nm)) {
                        ty = rl_strip_ws(a, fn->params.v[q].type);
                        if (!ty.p) ty = AVP_S("");
                        break;
                    }
                AvpStr cast_raw = inner_text(a, toks, i, c);
                AvpStr cast_ty = avp_str_replace(a, cast_raw, AVP_S("const"), AVP_S(""), -1);
                if (!ty.p) ty = local_type(ck, P, fn, nm);
                if (ty.p && ty.n && avp_str_eq(avp_str_replace(a, ty, AVP_S("const"), AVP_S(""), -1), cast_ty) &&
                    avp_str_contains(ty, AVP_S("const")) == avp_str_contains(cast_raw, AVP_S("const")))
                    rl_add(ck, P, toks[i].line, "R19", avp_str_contains(cast_ty, AVP_S("*")) ? "checked" : "free",
                           rl_fmt(a, "cast of '%.*s' to its own type (%.*s)", AVP_SF(nm), AVP_SF(cast_ty)), (AvpStr){0});
            }
        }
        if (c + 3 < n && toks[c + 1].k == RL_ID && TEQ(&toks[c + 2], ")") &&
            (TEQ(&toks[c + 3], "->") || TEQ(&toks[c + 3], "[")) && i > 0 && TEQ(&toks[i - 1], "(")) {
            i64 fi = enclosing_body(P, i);
            if (fi >= 0) {
                const RlFunc *fn = &P->p.funcs.v[fi];
                for (size_t q = 0; q < fn->params.n; q++) {
                    const RlParam *p = &fn->params.v[q];
                    if (avp_str_eq(p->name, toks[c + 1].t) &&
                        avp_str_eqz(rl_strip_ws(a, avp_str_replace(a, p->type, AVP_S("const"), AVP_S(""), -1)), "void*"))
                        rl_add(ck, P, toks[i].line, "R26", "checked",
                               rl_fmt(a, "void* parameter '%.*s' cast at the point of use: assign it to a typed local",
                                      AVP_SF(p->name)),
                               (AvpStr){0});
                }
            }
        }
    }
}

/* ---- check_tokens (L2311) ---- */
void rl_check_tokens(RlChecker *ck, RlParsed *P) {
    const RlTok *toks = P->toks.v;
    size_t n = P->toks.n;
    for (size_t i = 0; i < n; i++)
        if (toks[i].k == RL_ID) rl_check_identifier(ck, P, &toks[i]);
    rl_check_r4(ck, P, toks, n, P->match, false);
    check_volatile(ck, P);
    if (P->is_cpp) {
        static const char *const CPP11[] = {"nullptr", "constexpr", "noexcept", "static_assert", "decltype",
                                            "thread_local", "alignas", "char16_t", "char32_t", NULL};
        for (size_t i = 0; i < n; i++) {
            const RlTok *t = &toks[i];
            if (t->k == RL_ID && rl_tin(t, CPP11))
                rl_add(ck, P, t->line, "R22", "free",
                       rl_fmt(ck->a, "'%.*s' is not C++98 (the project dialect)", AVP_SF(t->t)), (AvpStr){0});
            else if (TEQ(t, "enum") && i + 1 < n && (TEQ(&toks[i + 1], "class") || TEQ(&toks[i + 1], "struct")))
                rl_add(ck, P, t->line, "R22", "free", rl_fmt(ck->a, "'enum %.*s' is not C++98", AVP_SF(toks[i + 1].t)),
                       (AvpStr){0});
        }
    }
    check_casts(ck, P);
}
