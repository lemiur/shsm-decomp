/* fragments.py row normalisation and rendering: frame_flags, norm_row, _letters, rename_rows, _fmt_varies, render. */
#include "frag.h"

#include "avp/fmt.h"
#include "shsm_re.h"

#include <stdlib.h>
#include <string.h>

const char *const FRAG_LEVELS[FRAG_NLEVELS] = {"strict", "loose"};
const char FRAG_LEVEL_PREFIX[FRAG_NLEVELS] = {'S', 'L'};

int frag_level_index(const char *name) {
    for (int i = 0; i < FRAG_NLEVELS; i++)
        if (name && strcmp(name, FRAG_LEVELS[i]) == 0) return i;
    return -1;
}

static const char REGTOK[] = "\\b([rf])(\\d{1,2})\\b";

/* REG_ID[name] for r0..r31 / f0..f31, else -1 */
static int reg_id(AvpStr name) {
    if (name.n < 2 || name.n > 3 || (name.p[0] != 'r' && name.p[0] != 'f')) return -1;
    int v = 0;
    for (size_t i = 1; i < name.n; i++) {
        if (name.p[i] < '0' || name.p[i] > '9') return -1;
        v = v * 10 + (name.p[i] - '0');
    }
    if (name.n == 3 && name.p[1] == '0') return -1; /* "r05" is not a key of REG_ID */
    if (v > 31) return -1;
    return (name.p[0] == 'r' ? 0 : 32) + v;
}

static bool preserved(AvpStr name) {
    return avp_str_eqz(name, "r1") || avp_str_eqz(name, "r2") || avp_str_eqz(name, "r13");
}

/* _callee_saved(reg): fullmatch ([rf])(\d{1,2}) and int >= 14 */
static bool callee_saved(AvpStr reg) {
    if (reg.n < 2 || reg.n > 3 || (reg.p[0] != 'r' && reg.p[0] != 'f')) return false;
    int v = 0;
    for (size_t i = 1; i < reg.n; i++) {
        if (reg.p[i] < '0' || reg.p[i] > '9') return false;
        v = v * 10 + (reg.p[i] - '0');
    }
    return v >= 14;
}

static bool mn_in(AvpStr m, const char *const *set) {
    for (; *set; set++)
        if (avp_str_eqz(m, *set)) return true;
    return false;
}

void frag_frame_flags(const ShsmAsmRow *rows, size_t n, bool *out) {
    static const char *const always[] = {"mflr", "mtlr", "stmw", "lmw", "blr", ".4byte", NULL};
    static const char *const frame_ls[] = {"stw", "lwz", "stfd", "lfd", "psq_st", "psq_l", "stfs", "lfs", NULL};
    char buf[512];
    for (size_t i = 0; i < n; i++) {
        const ShsmAsmRow *r = &rows[i];
        AvpStr m = r->mnemonic;
        /* (r.operands or "").replace(" ", "") */
        size_t k = 0;
        AvpStr raw = r->operands.p ? r->operands : AVP_S("");
        char *ob = raw.n < sizeof buf ? buf : avp_xmalloc(raw.n + 1);
        for (size_t j = 0; j < raw.n; j++)
            if (raw.p[j] != ' ') ob[k++] = raw.p[j];
        AvpStr ops = avp_strn(ob, k);
        bool f = false;
        if (mn_in(m, always)) {
            f = true;
        } else if (avp_str_eqz(m, "stwu") && avp_str_startswith(ops, AVP_S("r1,"))) {
            f = true;
        } else if (avp_str_eqz(m, "addi") &&
                   (avp_str_startswith(ops, AVP_S("r1,r1,")) || avp_str_startswith(ops, AVP_S("r11,r1,")))) {
            f = true;
        } else if (avp_str_eqz(m, "lwz") && avp_str_startswith(ops, AVP_S("r1,"))) {
            f = true;
        } else if ((avp_str_eqz(m, "bl") || avp_str_eqz(m, "b")) && r->sym.p && r->sym.n &&
                   shsm_re_match(shsm_re("_(?:save|rest)(?:gpr|fpr)", 0), r->sym, 0, AVP_END, NULL)) {
            f = true;
        } else if (mn_in(m, frame_ls) && avp_str_find(ops, AVP_S("(r1)"), 0, AVP_END) >= 0) {
            i64 c = avp_str_find(ops, AVP_S(","), 0, AVP_END);
            AvpStr reg = c >= 0 ? avp_strn(ops.p, (size_t)c) : ops;
            if (callee_saved(reg) && !avp_str_eqz(m, "stfs") && !avp_str_eqz(m, "lfs")) {
                f = true;
            } else if (avp_str_eqz(reg, "r0") && (avp_str_eqz(m, "stw") || avp_str_eqz(m, "lwz"))) {
                size_t lo = i >= 3 ? i - 3 : 0, hi = i + 4 < n ? i + 4 : n;
                for (size_t j = lo; j < hi; j++)
                    if (avp_str_eqz(rows[j].mnemonic, "mflr") || avp_str_eqz(rows[j].mnemonic, "mtlr")) f = true;
            }
        }
        if (ob != buf) free(ob);
        out[i] = f;
    }
}

/* _sym_token: the operand text with the referenced symbol (quoted or bare) replaced by a marker \x00. */
static AvpStr sym_token(AvpArena *a, AvpStr ops, AvpStr sym) {
    AvpStr esc = avp_str_replace(a, sym, AVP_S("\\"), AVP_S("\\\\"), -1);
    esc = avp_str_replace(a, esc, AVP_S("\""), AVP_S("\\\""), -1);
    AvpStr quoted = avp_pyfmt(a, "\"%S\"", &esc);
    AvpStr spelled[2] = {quoted, sym};
    for (int k = 0; k < 2; k++) {
        i64 i = avp_str_find(ops, spelled[k], 0, AVP_END);
        if (i >= 0) {
            AvpBuf b;
            avp_buf_init(&b, a);
            avp_buf_putn(&b, ops.p, (size_t)i);
            avp_buf_putc(&b, '\0');
            avp_buf_putn(&b, ops.p + i + spelled[k].n, ops.n - (size_t)i - spelled[k].n);
            return avp_buf_str(&b);
        }
    }
    return ops;
}

typedef struct {
    FragNorm *out;
} RegCtx;

static void rep_reg(void *ctx, const ShsmReMatch *m, AvpBuf *b) {
    RegCtx *c = ctx;
    AvpStr name = shsm_re_group(m, 0);
    if (preserved(name)) {
        avp_buf_put(b, name);
        return;
    }
    if (c->out->nregs < (int)(sizeof c->out->regs / sizeof c->out->regs[0])) c->out->regs[c->out->nregs] = reg_id(name);
    c->out->nregs++;
    avp_buf_putc(b, '%');
    avp_buf_put(b, shsm_re_group(m, 1));
}

static AvpStr cat_mn(AvpArena *a, AvpStr m, AvpStr ops) { return avp_pyfmt(a, "%S %S", &m, &ops); }

void frag_norm_row(AvpArena *a, const ShsmAsmRow *row, i64 fn_start, AvpStr fn_name, FragNorm *out) {
    static const AvpStr NUL = {"\0", 1}, MARK = {"\x01", 1};
    memset(out, 0, sizeof *out);
    AvpStr m = row->mnemonic;
    AvpStr ops = row->operands.p ? row->operands : AVP_S("");
    i64 label = row->label;
    bool has_fn = fn_name.p && fn_name.n;
    bool is_rel24 = row->reloc && strcmp(row->reloc, "rel24") == 0;
    if (label == SHSM_NONE && has_fn && row->sym.p && avp_str_eq(row->sym, fn_name) && is_rel24) label = fn_start;
    AvpStr rel = AVP_S(""), lrel = AVP_S("");
    AvpStr sym = row->sym.p && row->sym.n ? row->sym : (AvpStr){NULL, 0};
    if (label != SHSM_NONE) {
        i64 dist = avp_floordiv_i64(label - row->address, 4);
        rel = avp_pyfmt(a, "@%+d", dist);
        lrel = dist > 0 ? AVP_S("@fwd") : AVP_S("@back");
        const ShsmRe *lab = shsm_re("\\.L_[0-9A-Fa-f]{8}", 0);
        if (shsm_re_search(lab, ops, 0, AVP_END, NULL)) ops = shsm_re_sub(a, lab, MARK, ops, 0);
        else if (has_fn) ops = avp_str_replace(a, sym_token(a, ops, fn_name), NUL, MARK, -1);
        sym = (AvpStr){NULL, 0};
    }
    ops = shsm_re_sub(a, shsm_re("@sda21\\((?:r0|r2|r13)\\)", 0), AVP_S("@sda21"), ops, 0);
    AvpStr marked = sym.p ? sym_token(a, ops, sym) : ops;
    RegCtx rc = {out};
    AvpStr shaped = shsm_re_sub_fn(a, shsm_re(REGTOK, 0), rep_reg, &rc, marked, 0, NULL);
    if (out->nregs > (int)(sizeof out->regs / sizeof out->regs[0]))
        avp_panic(__FILE__, __LINE__, "fragments: a row with %d registers", out->nregs);
    AvpStr symt = sym.p ? sym : AVP_S("");
    out->strict = avp_str_replace(a, cat_mn(a, m, avp_str_replace(a, shaped, NUL, symt, -1)), MARK, rel, -1);
    AvpStr lsym = sym.p && is_rel24 ? sym : avp_pyfmt(a, "<%s>", row->reloc ? row->reloc : "sym");
    AvpStr num = shsm_re_sub(a, shsm_re("(?<![\\w@.$%])-?(?:0x[0-9A-Fa-f]+|\\d+)\\b", 0), AVP_S("#"), shaped, 0);
    out->loose = avp_str_replace(a, cat_mn(a, m, avp_str_replace(a, num, NUL, lsym, -1)), MARK, lrel, -1);
    out->disp =
        avp_str_rstrip(avp_str_replace(a, cat_mn(a, m, avp_str_replace(a, marked, NUL, symt, -1)), MARK, rel, -1));
}

/* ---- rendering ---- */

static AvpStr letters(AvpArena *a, i64 i) {
    char buf[16];
    int n = 0;
    i += 1;
    while (i) {
        i64 r = (i - 1) % 26;
        i = (i - 1) / 26;
        buf[n++] = (char)('A' + r);
    }
    char *s = avp_alloc(a, (size_t)n + 1);
    for (int k = 0; k < n; k++) s[k] = buf[n - 1 - k];
    return avp_strn(s, (size_t)n);
}

typedef struct {
    AvpStr order[64]; /* register id -> new name (p NULL: not seen) */
} RenameCtx;

static void rep_rename(void *ctx, const ShsmReMatch *m, AvpBuf *b) {
    RenameCtx *c = ctx;
    AvpStr name = shsm_re_group(m, 0);
    int id = reg_id(name);
    if (preserved(name) || id < 0 || !c->order[id].p) {
        avp_buf_put(b, name);
        return;
    }
    avp_buf_put(b, c->order[id]);
}

FragRenamed *frag_rename_rows(AvpArena *a, const FragRow *rows, size_t n) {
    RenameCtx c;
    memset(&c, 0, sizeof c);
    int percls[2] = {0, 0};
    FragRenamed *out = avp_alloc(a, (n ? n : 1) * sizeof *out);
    for (size_t i = 0; i < n; i++) {
        for (int k = 0; k < rows[i].nregs; k++) {
            i32 r = rows[i].regs[k];
            if (r < 0 || r >= 64 || c.order[r].p) continue;
            int cls = r < 32 ? 0 : 1;
            AvpStr l = letters(a, percls[cls]++);
            c.order[r] = avp_pyfmt(a, "%s%S", cls ? "f" : "r", &l);
        }
        AvpStr mn, ops;
        if (!avp_str_partition(rows[i].disp, AVP_S(" "), &mn, &ops)) {
            mn = rows[i].disp;
            ops = AVP_S("");
        }
        out[i].mn = mn;
        out[i].ops = shsm_re_sub_fn(a, shsm_re(REGTOK, 0), rep_rename, &c, ops, 0, NULL);
    }
    return out;
}

/* _fmt_varies(values, cap=3): Counter.most_common order (count, then first appearance) */
static AvpStr fmt_varies(AvpArena *a, const AvpStr *vals, size_t n) {
    AvpMap *m = avp_map_new();
    AVP_VEC(AvpStr) keys = {0};
    AvpI64Vec counts = {0};
    for (size_t i = 0; i < n; i++) {
        bool created;
        AvpVal *v = avp_map_put(m, vals[i], &created);
        if (created) {
            v->i = (i64)keys.n;
            avp_vec_push(&keys, vals[i]);
            avp_vec_push(&counts, 0);
        }
        counts.v[v->i]++;
    }
    /* stable sort by count, descending */
    size_t k = keys.n;
    size_t *idx = avp_xmalloc((k ? k : 1) * sizeof *idx);
    for (size_t i = 0; i < k; i++) idx[i] = i;
    for (size_t i = 1; i < k; i++) { /* insertion sort: few distinct values in practice */
        size_t x = idx[i];
        size_t j = i;
        while (j > 0 && counts.v[idx[j - 1]] < counts.v[x]) {
            idx[j] = idx[j - 1];
            j--;
        }
        idx[j] = x;
    }
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, "<varies: ");
    for (size_t i = 0; i < k && i < 3; i++) {
        if (i) avp_buf_putc(&b, '|');
        avp_buf_put(&b, keys.v[idx[i]]);
    }
    if (k > 3) avp_buf_pyfmt(&b, "|+%d", (i64)(k - 3));
    avp_buf_putc(&b, '>');
    free(idx);
    avp_vec_free(&keys);
    avp_vec_free(&counts);
    avp_map_free(m);
    return avp_buf_str(&b);
}

/* _ATOM.findall(text) */
static AvpStrVec atoms_of(AvpStr text) {
    AvpStrVec out = {0};
    const ShsmRe *re = shsm_re("<varies[^>]*>|[^,()\\s]+|[,()]|\\s+", 0);
    ShsmReIter it;
    ShsmReMatch m;
    shsm_re_iter_init(&it, re, text, 0, AVP_END);
    while (shsm_re_iter_next(&it, &m)) avp_vec_push(&out, shsm_re_group(&m, 0));
    return out;
}

static bool all_same(const AvpStr *v, size_t n) {
    for (size_t i = 1; i < n; i++)
        if (!avp_str_eq(v[i], v[0])) return false;
    return true;
}

FragRender frag_render(AvpArena *a, const FragRow *rows, size_t nocc, size_t len) {
    FragRender r;
    memset(&r, 0, sizeof r);
    if (!nocc) return r;
    FragRenamed **ren = avp_alloc(a, nocc * sizeof *ren);
    for (size_t o = 0; o < nocc; o++) ren[o] = frag_rename_rows(a, rows + o * len, len);
    r.values = avp_xcalloc(nocc, sizeof *r.values);
    AvpStrVec *atoms = avp_xcalloc(nocc, sizeof *atoms);
    AvpStr *vs = avp_xcalloc(nocc, sizeof *vs);
    for (size_t j = 0; j < len; j++) {
        AvpStr mn = ren[0][j].mn;
        bool differ = false;
        for (size_t o = 0; o < nocc; o++) {
            atoms[o] = atoms_of(ren[o][j].ops);
            if (atoms[o].n != atoms[0].n) differ = true;
        }
        if (differ) {
            for (size_t o = 0; o < nocc; o++) vs[o] = ren[o][j].ops;
            if (all_same(vs, nocc)) {
                avp_vec_push(&r.lines, avp_str_rstrip(avp_pyfmt(a, "%S %S", &mn, &vs[0])));
            } else {
                AvpStr fv = fmt_varies(a, vs, nocc);
                avp_vec_push(&r.lines, avp_pyfmt(a, "%S %S", &mn, &fv));
                avp_vec_push(&r.cols, ((FragCol){(i32)j, 0, avp_pyfmt(a, "+%d %S", (i64)j, &mn)}));
                for (size_t o = 0; o < nocc; o++) avp_vec_push(&r.values[o], vs[o]);
            }
        } else {
            AvpBuf b;
            avp_buf_init(&b, a);
            for (size_t x = 0; x < atoms[0].n; x++) {
                for (size_t o = 0; o < nocc; o++) vs[o] = atoms[o].v[x];
                if (all_same(vs, nocc)) {
                    avp_buf_put(&b, vs[0]);
                } else {
                    avp_buf_put(&b, fmt_varies(a, vs, nocc));
                    avp_vec_push(&r.cols, ((FragCol){(i32)j, (i32)x, avp_pyfmt(a, "+%d %S", (i64)j, &mn)}));
                    for (size_t o = 0; o < nocc; o++) avp_vec_push(&r.values[o], vs[o]);
                }
            }
            AvpStr parts = avp_buf_str(&b);
            avp_vec_push(&r.lines, avp_str_rstrip(avp_pyfmt(a, "%S %S", &mn, &parts)));
        }
        for (size_t o = 0; o < nocc; o++) avp_vec_free(&atoms[o]);
    }
    free(vs);
    free(atoms);
    return r;
}

void frag_render_free(FragRender *r, size_t nocc) {
    avp_vec_free(&r->lines);
    avp_vec_free(&r->cols);
    if (r->values) {
        for (size_t o = 0; o < nocc; o++) avp_vec_free(&r->values[o]);
        free(r->values);
    }
    r->values = NULL;
}
