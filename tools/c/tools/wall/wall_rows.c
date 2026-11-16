/* wall_rows: the row helpers of tools/wall.py (see wall_rows.h). */
#include "wall_rows.h"

const char *const WALL_CLASS_ORDER[WALL_NCLASSES] = {WALL_REFERENT, WALL_CALL, WALL_CONTROL, WALL_OTHER, WALL_REGISTER};

const char WALL_ORDER_NOTE[] =
    "Verdict order: referent, call set, control flow, other, register/schedule; 'other' (a differing "
    "immediate or offset) ranks above register/schedule, unlike the project's four-class order, because "
    "it is a source difference rather than an allocator decision.";

int wall_class_index(AvpStr cls) {
    for (int i = 0; i < WALL_NCLASSES; i++)
        if (avp_str_eqz(cls, WALL_CLASS_ORDER[i]))
            return i;
    return -1;
}

/* ---- character classes ---- */

static bool is_word(u8 c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0x80;
}
static bool is_digit(u8 c) { return c >= '0' && c <= '9'; }
static bool is_hex(u8 c) { return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
static bool word_at(AvpStr s, size_t i) { return i < s.n && is_word((u8)s.p[i]); }
/* \b at position i */
static bool boundary(AvpStr s, size_t i) { return (i > 0 && is_word((u8)s.p[i - 1])) != word_at(s, i); }

/* _REG = \b(cr|r|f)(\d+)\b at position i: the match end, the class prefix length (1 or 2), or 0 for none. */
static size_t reg_at(AvpStr s, size_t i, size_t *plen) {
    if (!boundary(s, i))
        return 0;
    size_t p;
    if (i + 1 < s.n && s.p[i] == 'c' && s.p[i + 1] == 'r')
        p = 2;
    else if (i < s.n && (s.p[i] == 'r' || s.p[i] == 'f'))
        p = 1;
    else
        return 0;
    size_t j = i + p;
    while (j < s.n && is_digit((u8)s.p[j]))
        j++;
    if (j == i + p || word_at(s, j)) /* \d+ then \b: the longest digit run must end the word */
        return 0;
    *plen = p;
    return j;
}

static bool is_fixed(AvpStr m) {
    return avp_str_eqz(m, "r1") || avp_str_eqz(m, "r2") || avp_str_eqz(m, "r13");
}

AvpStr wall_abstract_regs(AvpArena *a, AvpStr norm) {
    char *out = avp_alloc_noz(a, norm.n + 1);
    size_t o = 0, i = 0;
    while (i < norm.n) {
        size_t plen, e = reg_at(norm, i, &plen);
        if (e) {
            AvpStr m = avp_strn(norm.p + i, e - i);
            if (is_fixed(m)) {
                memcpy(out + o, m.p, m.n);
                o += m.n;
            } else if (plen == 2) {
                out[o++] = 'C';
                out[o++] = 'R';
            } else {
                out[o++] = norm.p[i] == 'r' ? 'R' : 'F';
            }
            i = e;
        } else {
            out[o++] = norm.p[i++];
        }
    }
    out[o] = 0;
    return avp_strn(out, o);
}

/* -?(?:0x[0-9a-fA-F]+|\d+)\b with the number starting at i (no sign): the match end, or 0. */
static size_t number_at(AvpStr s, size_t i) {
    if (i + 1 < s.n && s.p[i] == '0' && s.p[i + 1] == 'x') {
        size_t j = i + 2;
        while (j < s.n && is_hex((u8)s.p[j]))
            j++;
        if (j > i + 2 && !word_at(s, j))
            return j;
    }
    size_t j = i;
    while (j < s.n && is_digit((u8)s.p[j]))
        j++;
    if (j > i && !word_at(s, j))
        return j;
    return 0;
}

/* _NUM at position i: the match end, or 0. */
static size_t num_at(AvpStr s, size_t i) {
    if (i > 0) {
        u8 c = (u8)s.p[i - 1];
        if (is_word(c) || c == '@' || c == '#')
            return 0;
    }
    if (i < s.n && s.p[i] == '-')
        return number_at(s, i + 1); /* without the sign the number would start at '-': no match */
    return number_at(s, i);
}

AvpStr wall_abstract_values(AvpArena *a, AvpStr abstract) {
    char *out = avp_alloc_noz(a, abstract.n + 1);
    size_t o = 0, i = 0;
    while (i < abstract.n) {
        size_t e = num_at(abstract, i);
        if (e) {
            out[o++] = '#';
            i = e;
        } else {
            out[o++] = abstract.p[i++];
        }
    }
    out[o] = 0;
    return avp_strn(out, o);
}

size_t wall_regs_of(AvpArena *a, AvpStr norm, AvpStr **out) {
    AvpStr s = avp_str_replace(a, norm, AVP_S("@sda21(r0)"), AVP_S("@sda21"), -1);
    size_t n = 0, cap = 0;
    for (size_t i = 0; i < s.n;) {
        size_t plen, e = reg_at(s, i, &plen);
        if (e) {
            cap++;
            i = e;
        } else {
            i++;
        }
    }
    AvpStr *v = AVP_NEWN(a, AvpStr, cap);
    for (size_t i = 0; i < s.n;) {
        size_t plen, e = reg_at(s, i, &plen);
        if (e) {
            AvpStr m = avp_strn(s.p + i, e - i);
            if (!is_fixed(m))
                v[n++] = avp_str_dup(a, m);
            i = e;
        } else {
            i++;
        }
    }
    *out = v;
    return n;
}

/* Python's `$` in a match(): the end, or just before one trailing "\n". */
static AvpStr dollar_trim(AvpStr s) {
    return (s.n && s.p[s.n - 1] == '\n') ? avp_strn(s.p, s.n - 1) : s;
}

bool wall_save_helper(AvpStr name, AvpStr *out) {
    /* ^_(?:save|rest)[gf]pr_(\d+)$ : the digits end at the end or before a final "\n" */
    AvpStr s = dollar_trim(name);
    if (s.n < 10 || s.p[0] != '_')
        return false;
    if (memcmp(s.p + 1, "save", 4) != 0 && memcmp(s.p + 1, "rest", 4) != 0)
        return false;
    if ((s.p[5] != 'g' && s.p[5] != 'f') || memcmp(s.p + 6, "pr_", 3) != 0)
        return false;
    for (size_t i = 9; i < s.n; i++)
        if (!is_digit((u8)s.p[i]))
            return false;
    *out = avp_strn(name.p, 8); /* name[:m.start(1) - 1] */
    return true;
}

bool wall_callee_saved(AvpStr reg) {
    size_t p;
    if (reg.n >= 2 && reg.p[0] == 'c' && reg.p[1] == 'r')
        p = 2;
    else if (reg.n >= 1 && (reg.p[0] == 'r' || reg.p[0] == 'f'))
        p = 1;
    else
        return false;
    if (reg.n == p)
        return false;
    for (size_t i = p; i < reg.n; i++)
        if (!is_digit((u8)reg.p[i]))
            return false;
    if (p == 2)
        return false;
    /* int(digits) >= 14, for any length */
    size_t i = p;
    while (i < reg.n - 1 && reg.p[i] == '0')
        i++;
    size_t len = reg.n - i;
    if (len > 2)
        return true;
    int v = 0;
    for (; i < reg.n; i++)
        v = v * 10 + (reg.p[i] - '0');
    return v >= 14;
}

/* rest is all word characters (min: at least that many) */
static bool word_tail(AvpStr s, size_t from, size_t min) {
    if (s.n < from + min)
        return false;
    for (size_t i = from; i < s.n; i++)
        if (!is_word((u8)s.p[i]))
            return false;
    return true;
}

static bool digits_tail(AvpStr s, size_t from) {
    if (s.n <= from)
        return false;
    for (size_t i = from; i < s.n; i++)
        if (!is_digit((u8)s.p[i]))
            return false;
    return true;
}

bool wall_is_helper(AvpStr name) {
    AvpStr s = dollar_trim(name);
    static const char *const saverest[] = {"_savegpr", "_restgpr", "_savefpr", "_restfpr"};
    for (size_t k = 0; k < 4; k++) {
        AvpStr p = avp_str(saverest[k]);
        if (avp_str_startswith(s, p)) {
            if (s.n == p.n)
                return true;
            if (s.p[p.n] == '_' && digits_tail(s, p.n + 1))
                return true;
        }
    }
    static const char *const exact[] = {"__save_fpr", "__restore_fpr", "__save_gpr", "__restore_gpr",
                                        "__construct_array", "__construct_new_array", "__destroy_arr",
                                        "__dl__FPv", "__dla__FPv", "__nw__FUl", "__nwa__FUl",
                                        "__div2i", "__div2u", "__mod2i", "__mod2u", "__shl2i", "__shr2i", "__shr2u",
                                        "__register_global_object", "__copy", "__va_arg", "__dynamic_cast"};
    for (size_t k = 0; k < sizeof exact / sizeof exact[0]; k++)
        if (avp_str_eqz(s, exact[k]))
            return true;
    static const struct {
        const char *prefix;
        size_t min;
    } tails[] = {{"__ptmf_", 1}, {"__destroy_new_array", 0}, {"__cvt_", 1}, {"__init_cpp_exceptions", 0}, {"__rt", 1}};
    for (size_t k = 0; k < sizeof tails / sizeof tails[0]; k++) {
        AvpStr p = avp_str(tails[k].prefix);
        if (avp_str_startswith(s, p) && word_tail(s, p.n, tails[k].min))
            return true;
    }
    return false;
}

bool wall_is_compare(AvpStr op) {
    static const char *const cmps[] = {"cmpw", "cmplw", "cmpwi", "cmplwi", "fcmpu", "fcmpo", "cmp", "cmpl", "cmpi", "cmpli"};
    for (size_t k = 0; k < sizeof cmps / sizeof cmps[0]; k++)
        if (avp_str_eqz(op, cmps[k]))
            return true;
    return false;
}

bool wall_is_signed_pair(AvpStr a, AvpStr b) {
    return (avp_str_eqz(a, "cmpw") && avp_str_eqz(b, "cmplw")) || (avp_str_eqz(a, "cmplw") && avp_str_eqz(b, "cmpw")) ||
           (avp_str_eqz(a, "cmpwi") && avp_str_eqz(b, "cmplwi")) || (avp_str_eqz(a, "cmplwi") && avp_str_eqz(b, "cmpwi"));
}

bool wall_uses_r1(AvpStr s) {
    for (size_t i = 0; i + 2 <= s.n; i++)
        if (s.p[i] == 'r' && s.p[i + 1] == '1' && boundary(s, i) && !word_at(s, i + 2))
            return true;
    return false;
}

/* (?:0x[0-9a-fA-F]+|\d+)\(r1\) with the number at i: the match end, or 0. */
static size_t slot_number_at(AvpStr s, size_t i) {
    AvpStr tail = AVP_S("(r1)");
    if (i + 1 < s.n && s.p[i] == '0' && s.p[i + 1] == 'x') {
        size_t j = i + 2;
        while (j < s.n && is_hex((u8)s.p[j]))
            j++;
        if (j > i + 2 && avp_str_startswith(avp_strn(s.p + j, s.n - j), tail))
            return j;
    }
    size_t j = i;
    while (j < s.n && is_digit((u8)s.p[j]))
        j++;
    if (j > i && avp_str_startswith(avp_strn(s.p + j, s.n - j), tail))
        return j;
    return 0;
}

bool wall_r1_slot_find(AvpStr s, size_t pos, size_t *start, size_t *end, AvpStr *num) {
    for (size_t i = pos; i < s.n; i++) {
        size_t j = s.p[i] == '-' ? slot_number_at(s, i + 1) : slot_number_at(s, i);
        if (j) {
            *start = i;
            *end = j + 4;
            *num = avp_strn(s.p + i, j - i);
            return true;
        }
    }
    return false;
}

/* ---- row kinds ---- */

const char *wall_kind_name(WallKind k) {
    switch (k) {
    case WALL_K_COND: return "cond";
    case WALL_K_GOTO: return "goto";
    case WALL_K_FALL: return "fall";
    case WALL_K_RET: return "ret";
    case WALL_K_TAIL: return "tail";
    case WALL_K_CONDRET: return "condret";
    case WALL_K_CONDTAIL: return "condtail";
    case WALL_K_SWITCH: return "switch";
    case WALL_K_CALL: return "call";
    default: return NULL;
    }
}

bool wall_kind_is_terminator(WallKind k) {
    return k == WALL_K_COND || k == WALL_K_GOTO || k == WALL_K_RET || k == WALL_K_TAIL || k == WALL_K_CONDRET ||
           k == WALL_K_CONDTAIL || k == WALL_K_SWITCH;
}

bool wall_is_cond_branch(AvpStr op) {
    static const char *const cc[] = {"eq", "ne", "lt", "gt", "le", "ge", "ns", "so", "nl", "ng", "un", "nu",
                                     "dnz", "dz", "dnzt", "dnzf", "dzt", "dzf", "c"};
    AvpStr s = dollar_trim(op);
    if (s.n < 2 || s.p[0] != 'b')
        return false;
    AvpStr rest = avp_strn(s.p + 1, s.n - 1);
    for (size_t k = 0; k < sizeof cc / sizeof cc[0]; k++)
        if (avp_str_eqz(rest, cc[k]))
            return true;
    return false;
}

WallKind wall_row_kind(const CcRow *row) {
    AvpStr op = avp_str_rstrip_chars(row->op, AVP_S("+-"));
    if (!avp_str_startswith(op, AVP_S("b")))
        return WALL_K_NONE;
    if (avp_str_eqz(op, "blr"))
        return WALL_K_RET;
    if (avp_str_eqz(op, "bctr") ||
        (avp_str_endswith(op, AVP_S("ctr")) && !avp_str_eqz(op, "bctrl") && !avp_str_endswith(op, AVP_S("ctrl"))))
        return WALL_K_SWITCH;
    if (avp_str_eqz(op, "bl") || avp_str_eqz(op, "bla") || avp_str_eqz(op, "bctrl") || avp_str_eqz(op, "blrl") ||
        avp_str_endswith(op, AVP_S("ctrl")))
        return WALL_K_CALL;
    if (avp_str_endswith(op, AVP_S("lr")) && op.n > 3)
        return !avp_str_endswith(op, AVP_S("lrl")) ? WALL_K_CONDRET : WALL_K_CALL;
    bool local = row->has_label;
    if (local && row->bidx >= 0) {
        if (avp_str_eqz(op, "b"))
            return WALL_K_GOTO;
        if (wall_is_cond_branch(op))
            return WALL_K_COND;
    }
    if (row->has_ref || local) {
        if (avp_str_eqz(op, "b"))
            return WALL_K_TAIL;
        if (avp_str_endswith(op, AVP_S("l")) && !avp_str_eqz(op, "bl"))
            return WALL_K_CALL;
        if (wall_is_cond_branch(op))
            return WALL_K_CONDTAIL;
    }
    return WALL_K_NONE;
}

bool wall_is_self_call(const CcRow *row) {
    return avp_str_eqz(avp_str_rstrip_chars(row->op, AVP_S("+-")), "bl") && !row->has_ref && row->has_label &&
           row->bidx == 0;
}
