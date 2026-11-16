/* dtk listings -> rows (tools/codecmp.py: parse_row, decode_branch, FnCode, parse_listing). Each matcher below
 * names the Python regular expression it implements; the comments say why greedy matching without backtracking
 * gives the same result. */
#include "cc_listing.h"

bool cc_is_space(u8 c) { return c == ' ' || (c >= '\t' && c <= '\r') || (c >= 0x1c && c <= 0x1f); }
static bool is_hex(u8 c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
static bool is_uhex(u8 c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'); }
static bool is_digit(u8 c) { return c >= '0' && c <= '9'; }
/* Python's \w: ASCII letters, digits, '_' and (approximately) every non-ASCII byte. */
static bool is_word(u8 c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || is_digit(c) || c == '_' || c >= 0x80; }
static int hexval(u8 c) { return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; }

static AvpStr mk(AvpArena *a, const char *p, size_t n) {
    char *s = avp_alloc_noz(a, n + 1);
    if (n) memcpy(s, p, n);
    s[n] = 0;
    return avp_strn(s, n);
}

static AvpStr sstrip(AvpStr s) {
    while (s.n && cc_is_space((u8)s.p[0])) s.p++, s.n--;
    while (s.n && cc_is_space((u8)s.p[s.n - 1])) s.n--;
    return s;
}

/* re.sub(r"\s+", " ", text.strip()) */
static AvpStr collapse(AvpArena *a, AvpStr t) {
    t = sstrip(t);
    char *out = avp_alloc_noz(a, t.n + 1);
    size_t k = 0;
    for (size_t i = 0; i < t.n;) {
        if (cc_is_space((u8)t.p[i])) {
            while (i < t.n && cc_is_space((u8)t.p[i])) i++;
            out[k++] = ' ';
        } else {
            out[k++] = t.p[i++];
        }
    }
    out[k] = 0;
    return avp_strn(out, k);
}

/* int("[+-]0x<hex>", 16) over s[i:j] */
static i64 parse_signed_hex(AvpStr s, size_t i, size_t j) {
    bool neg = s.p[i] == '-';
    i64 v = 0;
    for (size_t k = i + 3; k < j; k++) v = v * 16 + hexval((u8)s.p[k]);
    return neg ? -v : v;
}

/* (?P<off>[+-]0x[0-9A-Fa-f]+) at p: the end of the match or 0 when none (greedy hex run). */
static size_t match_off(AvpStr s, size_t p) {
    if (p + 3 < s.n && (s.p[p] == '+' || s.p[p] == '-') && s.p[p + 1] == '0' && s.p[p + 2] == 'x' &&
        is_hex((u8)s.p[p + 3])) {
        size_t e = p + 3;
        while (e < s.n && is_hex((u8)s.p[e])) e++;
        return e;
    }
    return 0;
}

/* _SYMTXT = (?:"[^"]+"|[^\s,()+\-"@]+) at p: the end of the name or 0. A shorter unquoted run is never followed by
 * '+', '-' or '@' (those are outside the class), so the maximal run is the only candidate. */
static bool is_namec(u8 c) {
    return !cc_is_space(c) && c != ',' && c != '(' && c != ')' && c != '+' && c != '-' && c != '"' && c != '@';
}
static size_t match_quoted(AvpStr s, size_t p) {
    if (p >= s.n || s.p[p] != '"' || p + 1 >= s.n || s.p[p + 1] == '"') return 0;
    for (size_t k = p + 1; k < s.n; k++)
        if (s.p[k] == '"') return k + 1;
    return 0;
}
static size_t match_unquoted(AvpStr s, size_t p) {
    size_t e = p;
    while (e < s.n && is_namec((u8)s.p[e])) e++;
    return e > p ? e : 0;
}

static AvpStr unquote(AvpArena *a, AvpStr s) {
    if (s.n && s.p[0] == '"') return mk(a, s.p + 1, s.n >= 2 ? s.n - 2 : 0);
    return mk(a, s.p, s.n);
}

typedef struct {
    size_t start, end, name_s, name_e, kind_s, kind_e;
    bool has_off;
    i64 off;
} RelocM;

/* The tail (off)?@(kind) after a name ending at p. */
static bool reloc_tail(AvpStr s, size_t p, RelocM *m) {
    size_t q = match_off(s, p);
    size_t at = q ? q : p;
    if (q && !(at < s.n && s.p[at] == '@')) at = p; /* backtrack: no offset */
    if (!(at < s.n && s.p[at] == '@')) return false;
    size_t k = at + 1;
    if (!(k < s.n && ((s.p[k] >= 'A' && s.p[k] <= 'Z') || (s.p[k] >= 'a' && s.p[k] <= 'z')))) return false;
    size_t ke = k + 1;
    while (ke < s.n && ((s.p[ke] >= 'A' && s.p[ke] <= 'Z') || (s.p[ke] >= 'a' && s.p[ke] <= 'z') || is_digit((u8)s.p[ke]))) ke++;
    m->has_off = at != p;
    m->off = m->has_off ? parse_signed_hex(s, p, at) : 0;
    m->kind_s = k;
    m->kind_e = ke;
    m->end = ke;
    return true;
}

/* _RELOC.search(args) */
static bool search_reloc(AvpStr s, RelocM *m) {
    for (size_t p = 0; p < s.n; p++) {
        size_t e = match_quoted(s, p);
        if (e && reloc_tail(s, e, m)) {
            m->start = m->name_s = p;
            m->name_e = e;
            return true;
        }
        e = match_unquoted(s, p);
        if (e && reloc_tail(s, e, m)) {
            m->start = m->name_s = p;
            m->name_e = e;
            return true;
        }
    }
    return false;
}

/* _BRANCH_SYM.match(last): ^(name)(off)?$ */
static bool match_branch_sym(AvpStr s, size_t *name_e, bool *has_off, i64 *off) {
    size_t e = match_quoted(s, 0);
    for (int alt = 0; alt < 2; alt++) {
        if (alt == 1) e = match_unquoted(s, 0);
        if (!e) continue;
        if (e == s.n) {
            *name_e = e, *has_off = false, *off = 0;
            return true;
        }
        size_t q = match_off(s, e);
        if (q == s.n) {
            *name_e = e, *has_off = true, *off = parse_signed_hex(s, e, q);
            return true;
        }
    }
    return false;
}

/* _BRANCH_OP.match(op) */
static bool is_branch_op(AvpStr op) {
    static const char *const ends[] = {"", "l", "a", "la"};
    static const char *const conds[] = {"eq", "ne", "lt", "gt", "le", "ge", "ns", "so", "nl", "ng",
                                        "dnz", "dz", "dnzt", "dnzf", "dzt", "dzf"};
    if (!op.n || op.p[0] != 'b') return false;
    AvpStr r = avp_strn(op.p + 1, op.n - 1);
    for (size_t k = 0; k < 4; k++)
        if (avp_str_eqz(r, ends[k])) return true;
    for (size_t c = 0; c < AVP_ARRAY_LEN(conds); c++) {
        size_t cl = strlen(conds[c]);
        if (r.n < cl || memcmp(r.p, conds[c], cl) != 0) continue;
        AvpStr t = avp_strn(r.p + cl, r.n - cl);
        if (t.n && (t.p[0] == '+' || t.p[0] == '-')) t.p++, t.n--;
        for (size_t k = 0; k < 4; k++)
            if (avp_str_eqz(t, ends[k])) return true;
    }
    if (r.n >= 1 && r.p[0] == 'c') {
        for (size_t k = 1; k < r.n; k++)
            if (r.p[k] != 'l' && r.p[k] != 'a') return false;
        return true;
    }
    return false;
}

/* _LABEL.sub("L", s) */
static AvpStr label_sub(AvpArena *a, AvpStr s) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_reserve(&b, s.n + 1);
    size_t i = 0;
    while (i < s.n) {
        if (i + 3 < s.n && s.p[i] == '.' && s.p[i + 1] == 'L' && s.p[i + 2] == '_' && is_hex((u8)s.p[i + 3])) {
            size_t e = i + 3;
            while (e < s.n && is_hex((u8)s.p[e])) e++;
            avp_buf_putc(&b, 'L');
            i = e;
        } else {
            avp_buf_putc(&b, s.p[i++]);
        }
    }
    return avp_strn(avp_buf_cz(&b), b.n);
}

/* bool(_STRAY.search(s)) */
static bool stray_search(AvpStr s) {
    static const char *const pre[] = {"lbl_", "fn_", "jumptable_", "dtor_", "sinit_"};
    for (size_t i = 0; i < s.n; i++) {
        u8 c = (u8)s.p[i];
        if (c == '@') {
            if (i + 1 < s.n && is_digit((u8)s.p[i + 1])) return true;
            if (s.n - i >= 11 && memcmp(s.p + i, "@stringBase", 11) == 0) return true;
        }
        if (c == '.' && i + 3 < s.n && s.p[i + 1] == '.' && s.p[i + 2] == '.' && is_word((u8)s.p[i + 3])) {
            size_t e = i + 3;
            while (e < s.n && is_word((u8)s.p[e])) e++;
            /* \w+ is maximal (it cannot contain '.'); then '.' and a digit */
            if (e + 1 < s.n && s.p[e] == '.' && is_digit((u8)s.p[e + 1])) return true;
        }
        if (i == 0 || !is_word((u8)s.p[i - 1])) {
            for (size_t k = 0; k < AVP_ARRAY_LEN(pre); k++) {
                size_t pl = strlen(pre[k]);
                if (s.n - i < pl + 8 || memcmp(s.p + i, pre[k], pl) != 0) continue;
                bool ok = true;
                for (size_t h = 0; h < 8; h++) ok = ok && is_uhex((u8)s.p[i + pl + h]);
                if (ok) return true;
            }
        }
    }
    return false;
}

static i64 rfind(AvpStr hay, AvpStr needle) { return avp_str_rfind(hay, needle, 0, AVP_END); }

CcRow cc_parse_row(AvpArena *a, i64 addr, AvpStr text0) {
    CcRow row;
    memset(&row, 0, sizeof row);
    row.addr = addr;
    row.bidx = -1;
    AvpStr text = collapse(a, text0);
    row.text = text;
    AvpStr op = text, args = avp_strn(text.p + text.n, 0);
    i64 sp = avp_str_findc(text, ' ');
    if (sp >= 0) {
        op = avp_strn(text.p, (size_t)sp);
        args = avp_strn(text.p + sp + 1, text.n - (size_t)sp - 1);
    }
    row.op = mk(a, op.p, op.n);
    AvpBuf nb;
    avp_buf_init(&nb, a);
    AvpStr norm = text;
    RelocM m;
    if (search_reloc(args, &m)) {
        row.has_ref = true;
        row.ref.name = unquote(a, avp_strn(args.p + m.name_s, m.name_e - m.name_s));
        row.ref.off = m.off;
        row.ref.kind = mk(a, args.p + m.kind_s, m.kind_e - m.kind_s);
        avp_buf_put(&nb, op);
        avp_buf_putc(&nb, ' ');
        avp_buf_putn(&nb, args.p, m.start);
        avp_buf_putz(&nb, "SYM@");
        avp_buf_put(&nb, row.ref.kind);
        avp_buf_putn(&nb, args.p + m.end, args.n - m.end);
        norm = avp_strn(avp_buf_cz(&nb), nb.n);
    } else if (is_branch_op(op) && args.n) {
        AvpStr before, last;
        if (!avp_str_rpartition(args, AVP_S(","), &before, &last)) last = args;
        last = sstrip(last);
        bool label = last.n > 3 && memcmp(last.p, ".L_", 3) == 0;
        for (size_t k = 3; label && k < last.n; k++) label = is_hex((u8)last.p[k]);
        if (label) {
            u64 v = 0;
            for (size_t k = 3; k < last.n; k++) v = v * 16 + (u64)hexval((u8)last.p[k]);
            row.has_label = true;
            row.label = (i64)v;
            avp_buf_put(&nb, op);
            avp_buf_putc(&nb, ' ');
            avp_buf_putn(&nb, args.p, (size_t)rfind(args, last));
            avp_buf_putc(&nb, 'L');
            norm = avp_strn(avp_buf_cz(&nb), nb.n);
        } else {
            size_t ne;
            bool has_off;
            i64 off;
            bool hexlit = last.n > 2 && last.p[0] == '0' && last.p[1] == 'x';
            for (size_t k = 2; hexlit && k < last.n; k++) hexlit = is_hex((u8)last.p[k]);
            if (match_branch_sym(last, &ne, &has_off, &off) && !hexlit) {
                row.has_ref = true;
                row.ref.name = unquote(a, avp_strn(last.p, ne));
                row.ref.off = off;
                row.ref.kind = AVP_S("rel24");
                avp_buf_put(&nb, op);
                avp_buf_putc(&nb, ' ');
                avp_buf_putn(&nb, args.p, (size_t)rfind(args, last));
                avp_buf_putz(&nb, "SYM");
                norm = avp_strn(avp_buf_cz(&nb), nb.n);
            }
        }
    }
    norm = sstrip(label_sub(a, norm));
    row.norm = mk(a, norm.p, norm.n);
    row.stray = stray_search(row.norm);
    return row;
}

bool cc_decode_branch(u32 word, i64 addr, i64 *target) {
    u32 op = word >> 26;
    i64 disp;
    if (op == 18) {
        disp = word & 0x03FFFFFC;
        if (disp & 0x02000000) disp -= 0x04000000;
    } else if (op == 16) {
        disp = word & 0xFFFC;
        if (disp & 0x8000) disp -= 0x10000;
    } else {
        return false;
    }
    if (word & 2) return false;
    *target = addr + disp;
    return true;
}

/* re.sub(r"\s+\S+$", " L", norm) */
static AvpStr last_token_to_label(AvpArena *a, AvpStr s) {
    if (!s.n || cc_is_space((u8)s.p[s.n - 1])) return s;
    size_t k = s.n;
    while (k > 0 && !cc_is_space((u8)s.p[k - 1])) k--;
    if (k == 0) return s;
    size_t ws = k - 1;
    while (ws > 0 && cc_is_space((u8)s.p[ws - 1])) ws--;
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putn(&b, s.p, ws);
    avp_buf_putz(&b, " L");
    return avp_strn(avp_buf_cz(&b), b.n);
}

CcFn *cc_fn_new(AvpArena *a, AvpStr name, CcRow *rows, size_t nrows, const CcElf *elf, bool has_shndx, u32 shndx,
                bool has_start, i64 start) {
    CcFn *f = AVP_NEW(a, CcFn);
    f->name = name;
    f->rows = rows;
    f->nrows = nrows;
    f->elf = elf;
    f->has_shndx = has_shndx;
    f->shndx = shndx;
    f->start = nrows && !has_start ? rows[0].addr : (has_start ? start : 0);
    f->end = nrows ? rows[nrows - 1].addr + 4 : f->start;
    AvpIMap *by_addr = avp_imap_new_cap(nrows);
    for (size_t i = 0; i < nrows; i++) {
        bool created;
        AvpVal *v = avp_imap_put(by_addr, (u64)rows[i].addr, &created);
        if (created) v->i = (i64)i;
    }
    for (size_t i = 0; i < nrows; i++) {
        CcRow *r = &rows[i];
        if (!r->has_label && r->has_ref && avp_str_eqz(r->ref.kind, "rel24")) {
            bool ok = false;
            i64 tgt = 0;
            if (!r->reloc) {
                ok = r->has_word && cc_decode_branch(r->word, r->addr, &tgt);
            } else if (elf && has_shndx && (r->reloc->type == CC_R_REL24 || r->reloc->type == CC_R_REL14) &&
                       r->reloc->sym < elf->nsymbols) {
                const CcSym *sym = &elf->symbols[r->reloc->sym];
                if (sym->shndx == shndx) {
                    ok = true;
                    tgt = (i64)sym->value + r->reloc->addend;
                }
            }
            if (ok && f->start <= tgt && tgt < f->end) {
                r->has_label = true;
                r->label = tgt;
                r->has_ref = false;
                r->norm = last_token_to_label(a, r->norm);
            }
        }
        if (r->has_label) {
            AvpVal *v = avp_imap_get(by_addr, (u64)r->label);
            r->bidx = v ? v->i : -1;
        }
    }
    avp_imap_free(by_addr);
    return f;
}

CcFn *cc_fn_from_norms(AvpArena *a, AvpStr name, const AvpStr *norms, size_t n) {
    CcRow *rows = AVP_NEWN(a, CcRow, n ? n : 1);
    for (size_t i = 0; i < n; i++) {
        CcRow *r = &rows[i];
        r->addr = (i64)i * 4;
        r->text = r->norm = mk(a, norms[i].p, norms[i].n);
        AvpStrVec parts = avp_str_split(r->text, AVP_S(""), 1);
        r->op = parts.n ? mk(a, parts.v[0].p, parts.v[0].n) : AVP_S("");
        avp_vec_free(&parts);
        r->bidx = -1;
    }
    return cc_fn_new(a, mk(a, name.p, name.n), rows, n, NULL, false, 0, false, 0);
}

void cc_fn_norms(const CcFn *fn, AvpStr *out) {
    for (size_t i = 0; i < fn->nrows; i++) out[i] = fn->rows[i].norm;
}

/* _FN.match(line): ^\.fn\s+("[^"]+"|[^\s,]+)\s*, -> the raw group */
static bool match_fn(AvpStr line, AvpStr *g) {
    if (line.n < 4 || memcmp(line.p, ".fn", 3) != 0 || !cc_is_space((u8)line.p[3])) return false;
    size_t p = 3;
    while (p < line.n && cc_is_space((u8)line.p[p])) p++;
    for (int alt = 0; alt < 2; alt++) {
        size_t e = 0;
        if (alt == 0) {
            e = match_quoted(line, p);
        } else {
            e = p;
            while (e < line.n && !cc_is_space((u8)line.p[e]) && line.p[e] != ',') e++;
            if (e == p) e = 0;
        }
        if (!e) continue;
        size_t k = e;
        while (k < line.n && cc_is_space((u8)line.p[k])) k++;
        if (k < line.n && line.p[k] == ',') {
            *g = avp_strn(line.p + p, e - p);
            return true;
        }
    }
    return false;
}

/* _ROW.match(line): ^/\*\s+([hex]{8})\s+[hex]{8}\s+((?:[hex]{2}\s*)+)\*\/\s*(.*?)\s*$ */
static bool match_row(AvpStr s, i64 *addr, AvpStr *bytes, AvpStr *body) {
    size_t p = 0;
    if (s.n < 2 || s.p[0] != '/' || s.p[1] != '*') return false;
    p = 2;
    for (int field = 0; field < 2; field++) {
        size_t w = p;
        while (p < s.n && cc_is_space((u8)s.p[p])) p++;
        if (p == w) return false;
        for (int k = 0; k < 8; k++, p++)
            if (p >= s.n || !is_hex((u8)s.p[p])) return false;
        if (field == 0) {
            i64 v = 0;
            for (size_t k = p - 8; k < p; k++) v = v * 16 + hexval((u8)s.p[k]);
            *addr = v;
        }
    }
    size_t w = p;
    while (p < s.n && cc_is_space((u8)s.p[p])) p++;
    if (p == w) return false;
    size_t bs = p;
    int pairs = 0;
    while (p + 1 < s.n && is_hex((u8)s.p[p]) && is_hex((u8)s.p[p + 1])) {
        p += 2;
        pairs++;
        while (p < s.n && cc_is_space((u8)s.p[p])) p++;
    }
    if (!pairs || p + 1 >= s.n || s.p[p] != '*' || s.p[p + 1] != '/') return false;
    *bytes = avp_strn(s.p + bs, p - bs);
    *body = sstrip(avp_strn(s.p + p + 2, s.n - p - 2));
    return true;
}

typedef AVP_VEC(CcRow) RowVec;

AvpMap *cc_parse_listing(AvpArena *a, AvpStr text, const CcElf *elf) {
    AvpMap *fns = avp_map_new();
    AvpStrVec lines = avp_str_splitlines(text, false);
    bool in_fn = false;
    AvpStr cur = {0};
    RowVec rows = {0};
    for (size_t li = 0; li < lines.n; li++) {
        AvpStr line = lines.v[li];
        if (!in_fn) {
            AvpStr g;
            if (match_fn(line, &g)) {
                cur = unquote(a, g);
                in_fn = true;
                rows.n = 0;
            }
            continue;
        }
        if (line.n >= 6 && memcmp(line.p, ".endfn", 6) == 0) {
            bool has_shndx = false, has_start = false;
            u32 shndx = 0;
            i64 start = 0;
            CcRow *arr = AVP_NEWN(a, CcRow, rows.n ? rows.n : 1);
            if (rows.n) memcpy(arr, rows.v, rows.n * sizeof(CcRow));
            if (elf) {
                const CcSym *sym = cc_elf_function(elf, cur);
                if (sym) {
                    has_shndx = has_start = true;
                    shndx = sym->shndx;
                    start = sym->value;
                    for (size_t k = 0; k < rows.n; k++) arr[k].reloc = cc_elf_reloc_at(elf, shndx, arr[k].addr);
                }
            }
            avp_map_put(fns, cur, NULL)->p = cc_fn_new(a, cur, arr, rows.n, elf, has_shndx, shndx, has_start, start);
            in_fn = false;
            continue;
        }
        i64 addr;
        AvpStr bytes, body;
        if (!match_row(line, &addr, &bytes, &body)) continue;
        if (!body.n || body.p[0] == '.' || body.p[0] == '#') continue;
        CcRow row = cc_parse_row(a, addr, body);
        /* word = int(bytes.replace(" ", ""), 16) when that is 8 characters */
        char hx[16];
        size_t hn = 0;
        bool hex_ok = true;
        for (size_t k = 0; k < bytes.n; k++) {
            if (bytes.p[k] == ' ') continue;
            if (hn < sizeof hx) hx[hn] = bytes.p[k];
            hn++;
        }
        if (hn == 8) {
            u32 wv = 0;
            for (size_t k = 0; k < 8; k++) {
                hex_ok = hex_ok && is_hex((u8)hx[k]);
                wv = wv * 16 + (u32)(is_hex((u8)hx[k]) ? hexval((u8)hx[k]) : 0);
            }
            row.has_word = hex_ok;
            row.word = wv;
        }
        avp_vec_push(&rows, row);
    }
    avp_vec_free(&rows);
    avp_vec_free(&lines);
    return fns;
}

AvpMap *cc_load_asm(AvpArena *a, const char *path, AvpStr *err) {
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TM_UTF8, &text)) {
        if (err) *err = avp_io_err_str(a);
        return NULL;
    }
    return cc_parse_listing(a, text, NULL);
}

AvpMap *cc_load_object(AvpArena *a, const char *obj, const char *dtk, AvpStr *err) {
    char *lst = avp_sprintf(a, "%s.s", obj);
    if (!dtk) dtk = avp_sprintf(a, "%s/tools/bin/dtk.exe", avp_root());
    const char *argv[] = {dtk, "elf", "disasm", obj, lst, NULL};
    AvpProcSpec spec = {0};
    spec.argv = argv;
    spec.out = AVP_IO_CAPTURE;
    spec.err = AVP_IO_CAPTURE;
    spec.check = true;
    spec.arena = a;
    AvpProcResult r = avp_run(&spec);
    if (r.status != AVP_PROC_OK) {
        if (err)
            *err = avp_str(r.message ? avp_strdup(a, r.message)
                                     : avp_sprintf(a, "Command '%s' returned non-zero exit status %lld.", dtk,
                                                   (long long)r.returncode));
        return NULL;
    }
    AvpBytes data = {0};
    if (!avp_read_all(a, obj, &data)) {
        if (err) *err = avp_io_err_str(a);
        return NULL;
    }
    const char *why;
    CcElf *elf = cc_elf_parse(a, data.v, data.n, &why);
    if (!elf) {
        if (err) *err = avp_str(why);
        return NULL;
    }
    AvpStr text;
    if (!avp_read_text(a, lst, AVP_TM_UTF8, &text)) {
        if (err) *err = avp_io_err_str(a);
        return NULL;
    }
    return cc_parse_listing(a, text, elf);
}
