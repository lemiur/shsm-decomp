/* query.py's shared part: Ctx, the DOL reader, resolution, describe_sym / sym_line, summarize, row_note and the
 * formatters. */
#include "query.h"

#include <math.h>
#include <stdarg.h>

/* ---- the retail DOL ---- */

static u32 be32(const unsigned char *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }

QueryDol *query_dol_load(AvpArena *a, const char *path) {
    AvpBytes bytes = {0};
    if (!avp_read_all(a, path, &bytes)) return NULL;
    QueryDol *d = AVP_NEW(a, QueryDol);
    d->data = (AvpStr){(const char *)bytes.v, bytes.n};
    if (d->data.n < 0xE0) shsm_raise(SHSM_EXC_OS, "unpack_from requires a buffer of at least 224 bytes");
    const unsigned char *h = (const unsigned char *)d->data.p;
    for (int i = 0; i < 18; i++) {
        u32 off = be32(h + 4 * i), addr = be32(h + 72 + 4 * i), size = be32(h + 144 + 4 * i);
        if (size) d->sections[d->nsections++] = (QueryDolSection){addr, size, off, i < 7};
    }
    d->bss = be32(h + 216);
    d->bss_size = be32(h + 220);
    return d;
}

AvpStr query_dol_read(AvpArena *a, const QueryDol *d, i64 address, i64 size) {
    for (size_t i = 0; i < d->nsections; i++) {
        const QueryDolSection *s = &d->sections[i];
        if (s->address <= address && address + size <= (i64)s->address + s->size) {
            i64 lo = (i64)s->offset + address - s->address, hi = lo + size;
            if (lo > (i64)d->data.n) lo = (i64)d->data.n;
            if (hi > (i64)d->data.n) hi = (i64)d->data.n;
            return (AvpStr){d->data.p + lo, (size_t)(hi > lo ? hi - lo : 0)};
        }
    }
    if (d->bss <= address && address + size <= (i64)d->bss + d->bss_size)
        return (AvpStr){avp_alloc(a, (size_t)size + 1), (size_t)size};
    return (AvpStr){NULL, 0};
}

/* ---- context ---- */

QueryCtx *query_ctx_for_root(AvpArena *a, const char *root) {
    QueryCtx *c = AVP_NEW(a, QueryCtx);
    c->a = a;
    c->root = root;
    c->repo = shsm_repo_find(root);
    c->symcache = avp_map_new();
    c->ref_targets = avp_imap_new();
    c->items = avp_map_new();
    return c;
}

void query_ctx_close(QueryCtx *ctx) {
    if (ctx->ix_) shsm_asmindex_close(ctx->ix_);
    ctx->ix_ = NULL;
}

ShsmAsmIndex *query_ix(QueryCtx *ctx) {
    if (!ctx->ix_) {
        AvpTry t;
        if (AVP_TRY(&t)) {
            ctx->ix_ = shsm_asmindex_open(&(ShsmAsmIndexOpts){.repo = ctx->repo, .readonly = true});
            avp_try_end(&t);
        } else {
            if (t.code == SHSM_EXC_FILE_NOT_FOUND) shsm_raise(SHSM_EXC_USAGE, "%s", t.msg);
            avp_throw(t.code, "%s", t.msg);
        }
    }
    return ctx->ix_;
}

const ShsmUnits *query_units(QueryCtx *ctx) {
    if (!ctx->units_loaded) {
        ctx->units_loaded = true;
        AvpTry t;
        if (AVP_TRY(&t)) {
            ctx->units_ = shsm_load_units(ctx->repo);
            avp_try_end(&t);
        } else if (!(shsm_exc_is_os(t.code) || shsm_exc_is_value(t.code))) {
            avp_throw(t.code, "%s", t.msg);
        }
        if (!ctx->units_) {
            ShsmUnits *e = AVP_NEW(ctx->a, ShsmUnits);
            e->index = avp_map_new();
            ctx->units_ = e;
        }
    }
    return ctx->units_;
}

const ShsmReport *query_report(QueryCtx *ctx) {
    if (!ctx->report_loaded) {
        ctx->report_loaded = true;
        AvpTry t;
        if (AVP_TRY(&t)) {
            ctx->report_ = shsm_load_report(ctx->repo);
            avp_try_end(&t);
        } else {
            if (!(shsm_exc_is_os(t.code) || shsm_exc_is_value(t.code))) avp_throw(t.code, "%s", t.msg);
            ctx->report_ = NULL;
        }
    }
    return ctx->report_;
}

ShsmSymbolTable *query_syms(QueryCtx *ctx) {
    if (!ctx->syms_) ctx->syms_ = shsm_load_symbols(ctx->repo, true);
    return ctx->syms_;
}

const QueryDol *query_dol(QueryCtx *ctx) {
    if (!ctx->dol_loaded) {
        ctx->dol_loaded = true;
        ctx->dol_ = query_dol_load(ctx->a, ctx->repo->retail_dol);
    }
    return ctx->dol_;
}

const ShsmSymRow *query_sym(QueryCtx *ctx, AvpStr name) {
    if (!name.p) return NULL;
    AvpVal *v = avp_map_get(ctx->symcache, name);
    if (v) return v->p;
    ShsmSymRow *row = NULL;
    if (name.n) {
        ShsmSymRow tmp;
        if (shsm_asmindex_symbol(ctx->a, query_ix(ctx), name, &tmp)) {
            row = AVP_NEW(ctx->a, ShsmSymRow);
            *row = tmp;
        }
    }
    avp_map_put(ctx->symcache, avp_str_dup(ctx->a, name), NULL)->p = row;
    return row;
}

i64 query_base(QueryCtx *ctx, AvpStr name) {
    const ShsmSymRow *s = query_sym(ctx, name);
    return s ? s->address : SHSM_NONE;
}

static AvpStr json_s(const AvpJson *v) { return (v && avp_json_is_str(v)) ? v->u.str : (AvpStr){NULL, 0}; }
static i64 json_i(const AvpJson *v) { return (v && avp_json_is_int(v)) ? v->u.i : SHSM_NONE; }

AvpStr query_locate(QueryCtx *ctx, i64 address, i64 *sym_addr, AvpStr *unit) {
    if (!ctx->spans_loaded) {
        ctx->spans_loaded = true;
        AvpJson *rows = shsm_asmindex_sql(ctx->a, query_ix(ctx),
                                          "SELECT address, size, name, unit FROM symbols WHERE kind != 'label' AND "
                                          "size > 0 ORDER BY address, size DESC",
                                          NULL);
        ctx->nspans = avp_json_len(rows);
        ctx->spans = AVP_NEWN(ctx->a, QuerySpan, ctx->nspans + 1);
        for (size_t i = 0; i < ctx->nspans; i++) {
            AvpJson *r = avp_json_at(rows, i);
            ctx->spans[i] = (QuerySpan){json_i(avp_json_at(r, 0)), json_i(avp_json_at(r, 1)),
                                        json_s(avp_json_at(r, 2)), json_s(avp_json_at(r, 3))};
        }
    }
    /* bisect_right(addrs, address) - 1 */
    size_t lo = 0, hi = ctx->nspans;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (address < ctx->spans[mid].address) hi = mid;
        else lo = mid + 1;
    }
    i64 i = (i64)lo - 1;
    i64 stop = i - 64 > -1 ? i - 64 : -1;
    for (i64 k = i; k > stop; k--) {
        const QuerySpan *s = &ctx->spans[k];
        if (s->address <= address && address < s->address + s->size) {
            *sym_addr = s->address;
            *unit = (avp_pyfmt(ctx->a, "main/%S", &s->unit));
            return s->name;
        }
    }
    *sym_addr = SHSM_NONE;
    *unit = i >= 0 ? (avp_pyfmt(ctx->a, "main/%S", &ctx->spans[i].unit)) : (AvpStr){NULL, 0};
    return (AvpStr){NULL, 0};
}

const char *query_status(QueryCtx *ctx, AvpStr unit) {
    if (!unit.p || !unit.n) return "?";
    if (!ctx->status) {
        ctx->status = avp_map_new();
        AvpStr text = {0};
        bool ok = avp_read_text(ctx->a, ctx->repo->configure_py, AVP_TM_UTF8, &text);
        if (!ok && shsm_io_exc_code() == SHSM_EXC_VALUE) shsm_raise_io();
        if (ok) {
            ShsmConfStatus *st = NULL;
            size_t n = shsm_parse_configure_status(ctx->a, text, &st);
            for (size_t i = 0; i < n; i++) avp_map_put(ctx->status, avp_str(st[i].path), NULL)->p = (void *)st[i].status;
        }
    }
    AvpStr path = avp_str_startswith(unit, AVP_S("main/")) ? avp_str_slice(unit, 5, AVP_END) : unit;
    AvpVal *v = avp_map_get(ctx->status, path);
    if (v) return v->p;
    return avp_str_startswith(path, AVP_S("auto_")) ? "Asm" : "?";
}

static bool is_upper_hex(char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'); }

bool query_percent(QueryCtx *ctx, AvpStr name, AvpStr unit, double *pct) {
    const ShsmReport *rp = query_report(ctx);
    if (!rp || !name.p || !name.n) return false;
    const char *n = avp_cz(ctx->a, name), *u = (unit.p && unit.n) ? avp_cz(ctx->a, unit) : NULL;
    const ShsmFunctionReport *f = u ? shsm_report_function(rp, n, u) : NULL;
    if (!f) f = shsm_report_function(rp, n, NULL);
    if (!f && name.n >= 9 && name.p[name.n - 9] == '_') {
        bool hex = true;
        for (size_t i = name.n - 8; i < name.n; i++) hex = hex && is_upper_hex(name.p[i]);
        if (hex) {
            const char *g = avp_cz(ctx->a, avp_str_slice(name, 0, (i64)name.n - 9));
            f = u ? shsm_report_function(rp, g, u) : shsm_report_function(rp, g, NULL);
        }
    }
    if (!f) return false;
    *pct = f->fuzzy;
    return true;
}

/* ---- small helpers ---- */

AvpStr query_pct(AvpArena *a, bool has, double p) {
    return has ? (avp_pyfmt(a, "%5.1f%%", p)) : AVP_S("  -   ");
}

const char *query_mark(const char *status) { return strcmp(status, "Matching") == 0 ? "M" : "."; }

#define PLACEHOLDER_PAT "(?:^|::|__)(?:fn|lbl|unk|jumptable|gap|dtor|sinit)_[0-9A-Fa-f]{6,8}|^@\\d+$"

bool query_is_placeholder(AvpArena *a, AvpStr name) {
    if (!name.p || !name.n) return false;
    AvpStr base = query_mangle_base_name(a, name);
    AvpStr plain = base.p ? base : name;
    ShsmRe *re = shsm_re(PLACEHOLDER_PAT, 0);
    ShsmReMatch m;
    if (shsm_re_search(re, plain, 0, AVP_END, &m)) return true;
    i64 cut = avp_str_find(name, AVP_S("__"), 0, AVP_END);
    AvpStr head = cut < 0 ? name : avp_str_slice(name, 0, cut);
    return shsm_re_search(re, head, 0, AVP_END, &m);
}

AvpStr query_demangled(AvpArena *a, AvpStr name) {
    if (!name.p || !name.n) return (AvpStr){NULL, 0};
    const char *d = shsm_demangle(a, avp_cz(a, name));
    if (!d || !*d || avp_str_eqz(name, d)) return (AvpStr){NULL, 0};
    return avp_str(d);
}

i64 query_parse_addr(AvpStr text) {
    ShsmReMatch m;
    if (!shsm_re_match(shsm_re("^(?:0x)?(8[0-9A-Fa-f]{7})$", 0), avp_str_strip(text), 0, AVP_END, &m))
        return SHSM_NONE;
    u64 v = 0;
    avp_parse_uint(shsm_re_group(&m, 1), 16, &v);
    return (i64)v;
}

i64 query_parse_int(AvpArena *a, AvpStr text) {
    AvpStr t = avp_str_replace(a, avp_str_strip(text), AVP_S("_"), AVP_S(""), -1);
    bool neg = avp_str_startswith(t, AVP_S("-"));
    while (t.n && (t.p[0] == '+' || t.p[0] == '-')) t = avp_str_slice(t, 1, AVP_END);
    AvpStr low = avp_str_lower(a, avp_str_slice(t, 0, 2));
    i64 v = 0;
    bool ok;
    if (avp_str_eqz(low, "0d")) ok = avp_parse_int(avp_str_slice(t, 2, AVP_END), 10, &v, NULL);
    else ok = avp_parse_int(avp_str_eqz(low, "0x") ? avp_str_slice(t, 2, AVP_END) : t, 16, &v, NULL);
    if (!ok) shsm_raise(SHSM_EXC_USAGE, "not a number: %s", avp_cz(a, text));
    return neg ? -v : v;
}

AvpStr query_cstr(AvpArena *a, AvpStr text, i64 width) {
    AvpBuf b = {0};
    const char *p = text.p, *end = text.p + text.n;
    while (p < end) {
        const char *q = p;
        u32 c = avp_u8_next(&p, end);
        if (c == '\\') avp_buf_putz(&b, "\\\\");
        else if (c == '"') avp_buf_putz(&b, "\\\"");
        else if (c == '\n') avp_buf_putz(&b, "\\n");
        else if (c == '\t') avp_buf_putz(&b, "\\t");
        else if (c == '\r') avp_buf_putz(&b, "\\r");
        else if (c < 0x20 || c == 0x7F) avp_buf_printf(&b, "\\x%02X", (unsigned)c);
        else avp_buf_put(&b, (AvpStr){q, (size_t)(p - q)});
    }
    AvpStr s = avp_str_dup(a, avp_buf_str(&b));
    avp_buf_free(&b);
    if (width > 0 && (i64)avp_u8_len(s) > width) {
        AvpStr cut = avp_u8_slice(s, 0, width - 3);
        s = avp_pyfmt(a, "%S...", &cut);
    }
    return (avp_pyfmt(a, "\"%S\"", &s));
}

static AvpStr frepr(AvpArena *a, double v) {
    char buf[32];
    size_t n = avp_float_repr(v, buf);
    return avp_str_dup(a, (AvpStr){buf, n});
}

AvpStr query_fmt_float(AvpArena *a, bool has, double v, bool single) {
    if (!has) return AVP_S("0");
    if (isinf(v) || isnan(v)) return frepr(a, v);
    AvpStr t = frepr(a, v);
    if (single) {
        float fv = (float)v;
        for (int prec = 1; prec < 10; prec++) {
            double c = 0;
            avp_parse_float((avp_pyfmt(a, "%.*g", prec, v)), &c);
            float fc = (float)c;
            /* A short decimal just beyond FLT_MAX (3.402824e+38 for 3.4028235e+38) rounds to infinity: that
             * precision cannot name the constant, so try the next one. */
            if (isinf(fc) && !isinf(c)) continue;
            if (memcmp(&fc, &fv, 4) == 0) {
                t = frepr(a, c);
                break;
            }
        }
    }
    bool has_e = avp_str_find(t, AVP_S("e"), 0, AVP_END) >= 0;
    if (avp_str_endswith(t, AVP_S(".0")) && !has_e && t.n > 12) {
        double f = 0;
        avp_parse_float(t, &f);
        t = (avp_pyfmt(a, "%g", f));
    }
    if (avp_str_find(t, AVP_S("e"), 0, AVP_END) < 0 && avp_str_find(t, AVP_S("."), 0, AVP_END) < 0 &&
        avp_str_find(t, AVP_S("n"), 0, AVP_END) < 0)
        t = (avp_pyfmt(a, "%S.0", &t));
    return single ? (avp_pyfmt(a, "%Sf", &t)) : t;
}

AvpStr query_hex8(AvpArena *a, i64 v) { return (avp_pyfmt(a, "0x%08X", v)); }

AvpJson *query_sql(QueryCtx *ctx, const char *sql, size_t nparams, ...) {
    AvpJson *params = avp_json_arr(ctx->a);
    va_list ap;
    va_start(ap, nparams);
    for (size_t i = 0; i < nparams; i++) avp_json_arr_push(ctx->a, params, va_arg(ap, AvpJson *));
    va_end(ap);
    return shsm_asmindex_sql(ctx->a, query_ix(ctx), sql, params);
}

/* ---- resolution ---- */

static AvpStr like(AvpArena *a, AvpStr text) {
    AvpStr t = avp_str_replace(a, text, AVP_S("\\"), AVP_S("\\\\"), -1);
    t = avp_str_replace(a, t, AVP_S("%"), AVP_S("\\%"), -1);
    t = avp_str_replace(a, t, AVP_S("_"), AVP_S("\\_"), -1);
    return (avp_pyfmt(a, "%%%S%%", &t));
}

static const ShsmSymRow *first_of_kind(QueryCtx *ctx, AvpStr name, const char *kind) {
    ShsmSymRows rows = shsm_asmindex_symbols_named(ctx->a, query_ix(ctx), name);
    for (size_t i = 0; i < rows.n; i++)
        if (avp_str_eqz(rows.v[i].kind, kind)) {
            ShsmSymRow *r = AVP_NEW(ctx->a, ShsmSymRow);
            *r = rows.v[i];
            return r;
        }
    return NULL;
}

const ShsmSymRow *query_resolve(QueryCtx *ctx, AvpStr text, const char *kind) {
    AvpArena *a = ctx->a;
    ShsmAsmIndex *ix = query_ix(ctx);
    i64 addr = query_parse_addr(text);
    if (addr != SHSM_NONE) {
        ShsmSymRows at = shsm_asmindex_at(a, ix, addr);
        ShsmSymRows cands = {0};
        for (size_t i = 0; i < at.n; i++)
            if (!avp_str_eqz(at.v[i].kind, "label")) avp_vec_push(&cands, at.v[i]);
        if (!cands.n) {
            ShsmSymRow c;
            if (shsm_asmindex_containing(a, ix, addr, &c)) avp_vec_push(&cands, c);
        }
        for (size_t i = 0; i < cands.n; i++)
            if (!kind || avp_str_eqz(cands.v[i].kind, kind)) {
                ShsmSymRow *r = AVP_NEW(a, ShsmSymRow);
                *r = cands.v[i];
                avp_vec_free(&cands);
                return r;
            }
        avp_vec_free(&cands);
        if (avp_str_startswith(avp_str_lower(a, text), AVP_S("0x")))
            shsm_raise(SHSM_EXC_USAGE, "no %s at 0x%08llX", kind ? kind : "symbol", (long long)addr);
    }
    ShsmSymRow s;
    bool found = shsm_asmindex_symbol(a, ix, text, &s);
    if (found && (!kind || avp_str_eqz(s.kind, kind))) {
        ShsmSymRow *r = AVP_NEW(a, ShsmSymRow);
        *r = s;
        return r;
    }
    if (found && kind) {
        const ShsmSymRow *o = first_of_kind(ctx, text, kind);
        if (o) return o;
        shsm_raise(SHSM_EXC_USAGE, "%s is a %s, not a %s", avp_cz(a, text), avp_cz(a, s.kind), kind);
    }
    const char *kw = kind ? " AND kind = ?" : "";
    AvpJson *jt = avp_json_str(a, text), *jk = kind ? avp_json_strz(a, kind) : NULL;
    AvpJson *rows = kind ? query_sql(ctx, avp_sprintf(a, "SELECT DISTINCT name FROM symbols WHERE (demangled = ? OR "
                                                         "substr(demangled, 1, instr(demangled, '(') - 1) = ?)%s", kw),
                                     3, jt, jt, jk)
                         : query_sql(ctx, "SELECT DISTINCT name FROM symbols WHERE (demangled = ? OR "
                                          "substr(demangled, 1, instr(demangled, '(') - 1) = ?)",
                                     2, jt, jt);
    AvpStrVec names = {0};
    for (size_t i = 0; i < avp_json_len(rows); i++) avp_vec_push(&names, json_s(avp_json_at(avp_json_at(rows, i), 0)));
    if (!names.n) {
        AvpJson *jl = avp_json_str(a, like(a, text));
        const char *q = avp_sprintf(a, "SELECT DISTINCT name FROM symbols WHERE (name LIKE ? ESCAPE '\\' OR "
                                       "demangled LIKE ? ESCAPE '\\')%s ORDER BY address LIMIT 60", kw);
        rows = kind ? query_sql(ctx, q, 3, jl, jl, jk) : query_sql(ctx, q, 2, jl, jl);
        for (size_t i = 0; i < avp_json_len(rows); i++)
            avp_vec_push(&names, json_s(avp_json_at(avp_json_at(rows, i), 0)));
        if (names.n > 1) {
            AvpStr esc = shsm_re_escape(a, text);
            ShsmRe *word = shsm_re_try_n((avp_pyfmt(a, "(?<![A-Za-z0-9_])%S(?![A-Za-z0-9])", &esc)), 0);
            AvpStrVec whole = {0};
            for (size_t i = 0; i < names.n; i++) {
                ShsmReMatch m;
                AvpStr dm = query_demangled(a, names.v[i]);
                if (shsm_re_search(word, names.v[i], 0, AVP_END, &m) ||
                    shsm_re_search(word, dm.p ? dm : AVP_S(""), 0, AVP_END, &m))
                    avp_vec_push(&whole, names.v[i]);
            }
            if (whole.n) {
                avp_vec_free(&names);
                names = whole;
            } else avp_vec_free(&whole);
        }
    }
    if (names.n == 1) {
        if (!kind) return query_sym(ctx, names.v[0]);
        const ShsmSymRow *r = first_of_kind(ctx, names.v[0], kind);
        if (!r) shsm_raise(SHSM_EXC_OS, "list index out of range"); /* IndexError in the Python */
        return r;
    }
    if (!names.n) shsm_raise(SHSM_EXC_USAGE, "no %s matches %s", kind ? kind : "symbol", avp_cz(a, avp_repr_str(a, text)));
    AvpBuf b = {0};
    for (size_t i = 0; i < names.n && i < 8; i++) {
        if (i) avp_buf_putz(&b, ", ");
        avp_buf_put(&b, names.v[i]);
    }
    if (names.n > 8) avp_buf_printf(&b, " (+%d more)", (int)(names.n - 8));
    AvpStr shown = avp_str_dup(a, avp_buf_str(&b));
    avp_buf_free(&b);
    shsm_raise(SHSM_EXC_USAGE, "%s is ambiguous: %s", avp_repr_str(a, text).p ? avp_cz(a, avp_repr_str(a, text)) : "",
               avp_cz(a, shown));
}

/* ---- describe_sym / sym_line ---- */

QuerySymDesc query_describe_sym(QueryCtx *ctx, AvpStr name, i64 addr) {
    AvpArena *a = ctx->a;
    const ShsmSymRow *s = (name.p && name.n) ? query_sym(ctx, name) : NULL;
    AvpStr unit = {NULL, 0};
    if (s) unit = avp_str(shsm_symrow_unit_name(a, s));
    else if (addr != SHSM_NONE) unit = avp_str(shsm_asmindex_unit_of(a, query_ix(ctx), addr));
    if (!s && addr != SHSM_NONE && !unit.n) unit = (AvpStr){NULL, 0};
    QuerySymDesc d = {0};
    d.name = name;
    d.demangled = (name.p && name.n) ? query_demangled(a, name) : (AvpStr){NULL, 0};
    d.address = s ? s->address : addr;
    d.kind = s ? s->kind : (AvpStr){NULL, 0};
    d.unit = unit;
    d.status = query_status(ctx, unit);
    d.placeholder = query_is_placeholder(a, name);
    if (s && avp_str_eqz(s->kind, "function") && strcmp(d.status, "Asm") != 0 && strcmp(d.status, "?") != 0)
        d.has_percent = query_percent(ctx, name, unit, &d.percent);
    return d;
}

AvpJson *query_desc_json(AvpArena *a, const QuerySymDesc *d) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("name"), shsm_json_opt_str(a, d->name));
    avp_json_set(a, o, AVP_S("demangled"), shsm_json_opt_str(a, d->demangled));
    avp_json_set(a, o, AVP_S("address"), shsm_json_opt_int(a, d->address));
    avp_json_set(a, o, AVP_S("kind"), shsm_json_opt_str(a, d->kind));
    avp_json_set(a, o, AVP_S("unit"), shsm_json_opt_str(a, d->unit));
    avp_json_set(a, o, AVP_S("status"), avp_json_strz(a, d->status));
    avp_json_set(a, o, AVP_S("placeholder"), avp_json_bool(d->placeholder));
    avp_json_set(a, o, AVP_S("percent"), d->has_percent ? avp_json_float(a, d->percent) : avp_json_null());
    return o;
}

AvpStr query_sym_line(AvpArena *a, const QuerySymDesc *d, AvpStr extra) {
    AvpStr name = (d->name.p && d->name.n)
                      ? d->name
                      : (avp_pyfmt(a, "<data @0x%08X>", d->address == SHSM_NONE ? 0 : d->address));
    AvpStr dm = d->demangled.p && d->demangled.n ? (avp_pyfmt(a, "  {%S}", &d->demangled)) : AVP_S("");
    AvpStr p = AVP_S("");
    if (d->has_percent) {
        AvpStr ps = avp_str_strip(query_pct(a, true, d->percent));
        p = (avp_pyfmt(a, "  %S", &ps));
    }
    AvpStr unit = d->unit.p && d->unit.n ? d->unit : AVP_S("?");
    return (avp_pyfmt(a, "%s%s %S%S%S  %S %s%S", query_mark(d->status), d->placeholder ? "?" : " ", &extra,
                             &name, &dm, &unit, d->status, &p));
}

/* ---- summarize / row_note ---- */

static bool sym_digits_after(AvpStr sym, const char *prefix, i64 *n) {
    AvpStr pre = avp_str(prefix);
    if (!sym.p || !avp_str_startswith(sym, pre) || sym.n == pre.n) return false;
    for (size_t i = pre.n; i < sym.n; i++)
        if (sym.p[i] < '0' || sym.p[i] > '9') return false;
    bool ovf = false;
    return avp_parse_int(avp_str_slice(sym, (i64)pre.n, AVP_END), 10, n, &ovf) || ovf;
}

static void add_from(i64 *minreg, i64 from) {
    if (from < 32 && (*minreg < 0 || from < *minreg)) *minreg = from;
}

QuerySummary query_summarize(const ShsmAsmRow *rows, size_t n) {
    QuerySummary s = {0};
    s.min_gpr = s.min_fpr = -1;
    s.instructions = (i64)n;
    ShsmRe *frame_re = shsm_re("r1, -(0x[0-9A-Fa-f]+|\\d+)\\(r1\\)", 0);
    ShsmRe *stmw_re = shsm_re("r(\\d+), ", 0);
    ShsmRe *stw_re = shsm_re("r(1[4-9]|2\\d|3[01]), .*\\(r1\\)$", 0);
    ShsmRe *stfd_re = shsm_re("f(1[4-9]|2\\d|3[01]), .*\\(r1\\)", 0);
    static const char *const fpfx[] = {"f", "lfs", "lfd", "stfs", "stfd", "psq_", "ps_", "lfsx", "lfdx"};
    for (size_t i = 0; i < n; i++) {
        const ShsmAsmRow *r = &rows[i];
        AvpStr m = r->mnemonic, ops = r->operands;
        bool rel24 = r->reloc && strcmp(r->reloc, "rel24") == 0;
        if (avp_str_eqz(m, "bl") || avp_str_eqz(m, "bctrl") || avp_str_eqz(m, "blrl") ||
            (rel24 && avp_str_startswith(m, AVP_S("b")) && avp_str_endswith(m, AVP_S("l")))) {
            s.calls++;
            i64 k;
            if (sym_digits_after(r->sym, "_savegpr_", &k)) add_from(&s.min_gpr, k);
            if (sym_digits_after(r->sym, "_savefpr_", &k)) add_from(&s.min_fpr, k);
            continue;
        }
        if (avp_str_startswith(m, AVP_S("b")) && !avp_str_startswith(m, AVP_S("blr"))) {
            s.branches++;
            if (rel24 && r->sym.p && r->sym.n) s.tail_calls++;
        }
        ShsmReMatch mm;
        if (avp_str_eqz(m, "stwu") && avp_str_startswith(ops, AVP_S("r1, -"))) {
            if (shsm_re_match(frame_re, ops, 0, AVP_END, &mm) && !s.frame) {
                AvpStr g = shsm_re_group(&mm, 1);
                i64 v = 0;
                avp_parse_int(g, 0, &v, NULL);
                s.frame = v;
            }
        } else if (avp_str_eqz(m, "stmw")) {
            if (shsm_re_match(stmw_re, ops, 0, AVP_END, &mm)) {
                i64 v = 0;
                avp_parse_int(shsm_re_group(&mm, 1), 10, &v, NULL);
                add_from(&s.min_gpr, v);
            }
        } else if (avp_str_eqz(m, "stw") && shsm_re_match(stw_re, ops, 0, AVP_END, &mm)) {
            i64 v = 0;
            avp_parse_int(shsm_re_group(&mm, 1), 10, &v, NULL);
            add_from(&s.min_gpr, v);
        } else if ((avp_str_eqz(m, "stfd") || avp_str_eqz(m, "psq_st")) &&
                   shsm_re_match(stfd_re, ops, 0, AVP_END, &mm)) {
            i64 v = 0;
            avp_parse_int(shsm_re_group(&mm, 1), 10, &v, NULL);
            add_from(&s.min_fpr, v);
        }
        for (size_t k = 0; k < sizeof fpfx / sizeof *fpfx; k++)
            if (avp_str_startswith(m, avp_str(fpfx[k]))) s.floats = true;
    }
    return s;
}

AvpJson *query_summary_json(AvpArena *a, const QuerySummary *s) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("instructions"), avp_json_int(a, s->instructions));
    avp_json_set(a, o, AVP_S("calls"), avp_json_int(a, s->calls));
    avp_json_set(a, o, AVP_S("tail_calls"), avp_json_int(a, s->tail_calls));
    avp_json_set(a, o, AVP_S("branches"), avp_json_int(a, s->branches));
    avp_json_set(a, o, AVP_S("frame"), avp_json_int(a, s->frame));
    avp_json_set(a, o, AVP_S("saved_gprs"),
                 s->min_gpr >= 0 ? avp_json_str(a, avp_pyfmt(a, "r%d-r31", s->min_gpr)) : avp_json_null());
    avp_json_set(a, o, AVP_S("saved_fprs"),
                 s->min_fpr >= 0 ? avp_json_str(a, avp_pyfmt(a, "f%d-f31", s->min_fpr)) : avp_json_null());
    avp_json_set(a, o, AVP_S("float"), avp_json_bool(s->floats));
    return o;
}

i64 query_resolved_target(QueryCtx *ctx, i64 address) {
    AvpVal *v = avp_imap_get(ctx->ref_targets, (u64)address);
    if (!v) {
        ShsmRefs refs = shsm_asmindex_refs_from(ctx->a, query_ix(ctx), shsm_target_addr(address), NULL, 0);
        for (size_t i = 0; i < refs.n; i++) {
            bool created = false;
            AvpVal *e = avp_imap_put(ctx->ref_targets, (u64)refs.v[i].from_addr, &created);
            if (created) e->i = refs.v[i].to_addr;
        }
        bool created = false;
        v = avp_imap_put(ctx->ref_targets, (u64)address, &created);
        if (created) v->i = SHSM_NONE;
    }
    return v->i;
}

AvpStr query_row_note(QueryCtx *ctx, const ShsmAsmRow *r) {
    AvpArena *a = ctx->a;
    if (!r->sym.p || !r->sym.n) return AVP_S("");
    if (r->reloc && strcmp(r->reloc, "rel24") == 0) {
        AvpStr d = query_demangled(a, r->sym);
        return d.p ? d : AVP_S("");
    }
    if (r->reloc && strcmp(r->reloc, "ha") == 0) return AVP_S("");
    i64 t = query_resolved_target(ctx, r->address);
    if (t == SHSM_NONE) return AVP_S("");
    AvpJson *rows = query_sql(ctx, "SELECT kind, value, text FROM floats WHERE address = ?", 1, avp_json_int(a, t));
    if (avp_json_len(rows)) {
        AvpJson *row = avp_json_at(rows, 0), *val = avp_json_at(row, 1);
        bool has = val && avp_json_is_num(val);
        double v = has ? (avp_json_is_int(val) ? (double)val->u.i : val->u.f) : 0;
        AvpStr f = query_fmt_float(a, has, v, avp_str_eqz(json_s(avp_json_at(row, 0)), "float"));
        return (avp_pyfmt(a, "= %S", &f));
    }
    rows = query_sql(ctx, "SELECT text FROM strings WHERE address = ?", 1, avp_json_int(a, t));
    if (avp_json_len(rows)) {
        AvpStr c = query_cstr(a, json_s(avp_json_at(avp_json_at(rows, 0), 0)), 60);
        return (avp_pyfmt(a, "= %S", &c));
    }
    return AVP_S("");
}

AvpStr query_unit_header(QueryCtx *ctx, const ShsmSymRow *s) {
    AvpArena *a = ctx->a;
    const char *un = shsm_symrow_unit_name(a, s);
    const ShsmUnit *u = shsm_units_lookup(query_units(ctx), un);
    const char *bits[3] = {query_status(ctx, avp_str(un)), u && u->library ? u->library : "",
                           u && u->source ? u->source : ""};
    AvpBuf b = {0};
    bool first = true;
    for (int i = 0; i < 3; i++) {
        if (!*bits[i]) continue;
        if (!first) avp_buf_putz(&b, ", ");
        avp_buf_putz(&b, bits[i]);
        first = false;
    }
    AvpStr inner = avp_str_dup(a, avp_buf_str(&b));
    avp_buf_free(&b);
    return (avp_pyfmt(a, "%s (%S)", un, &inner));
}

/* ---- data items, units ---- */

const QueryItems *query_block_items(QueryCtx *ctx, AvpStr name) {
    AvpVal *v = avp_map_get(ctx->items, name);
    if (v) return v->p;
    QueryItems *it = AVP_NEW(ctx->a, QueryItems);
    it->by_addr = avp_imap_new();
    const ShsmSymRow *s = query_sym(ctx, name);
    if (s && avp_str_eqz(s->kind, "object")) {
        ShsmAsmFile *af = NULL;
        it->block = shsm_asmindex_read(query_ix(ctx), s, &af); /* the file stays alive for the run */
        if (it->block)
            for (size_t i = 0; i < it->block->items.n; i++)
                avp_imap_put(it->by_addr, (u64)it->block->items.v[i].address, NULL)->p = &it->block->items.v[i];
    }
    avp_map_put(ctx->items, avp_str_dup(ctx->a, name), NULL)->p = it;
    return it;
}

const ShsmAsmItem *query_item_at(const QueryItems *items, i64 address) {
    AvpVal *v = avp_imap_get(items->by_addr, (u64)address);
    return v ? v->p : NULL;
}

AvpStr query_ptr_text(QueryCtx *ctx, AvpStr sym, i64 off) {
    if (!sym.p || !sym.n) return AVP_S("NULL");
    const ShsmSymRow *s = query_sym(ctx, sym);
    bool isfn = s && avp_str_eqz(s->kind, "function");
    AvpStr base = isfn ? sym : avp_pyfmt(ctx->a, "&%S", &sym);
    if (off && off != SHSM_NONE) base = avp_pyfmt(ctx->a, "(u8*)%S + 0x%X", &base, off);
    return base;
}

AvpStr query_val_str(AvpArena *a, const ShsmAsmVal *v) {
    switch (v->t) {
    case SHSM_VAL_INT: return avp_pyfmt(a, "%d", v->i);
    case SHSM_VAL_FLOAT: return frepr(a, v->f);
    case SHSM_VAL_STR: return v->s;
    default: return AVP_S("None");
    }
}

static i64 val_int_or0(const ShsmAsmVal *v) { return v->t == SHSM_VAL_INT ? v->i : 0; }

AvpStr query_item_text(QueryCtx *ctx, const ShsmAsmItem *it) {
    AvpArena *a = ctx->a;
    const char *k = it->kind;
    const ShsmAsmVal *v = &it->value;
    bool num = v->t == SHSM_VAL_INT || v->t == SHSM_VAL_FLOAT;
    double f = v->t == SHSM_VAL_INT ? (double)v->i : v->f;
    if (!strcmp(k, "float")) return query_fmt_float(a, num, f, true);
    if (!strcmp(k, "double")) return query_fmt_float(a, num, f, false);
    if (!strcmp(k, "ptr")) return query_ptr_text(ctx, it->sym, it->offset);
    if (!strcmp(k, "string") || !strcmp(k, "string16")) {
        AvpStr c = query_cstr(a, v->t == SHSM_VAL_STR ? v->s : AVP_S(""), 0);
        return !strcmp(k, "string16") ? avp_pyfmt(a, "L%S", &c) : c;
    }
    if (!strcmp(k, "rel")) return v->t == SHSM_VAL_INT ? avp_pyfmt(a, ".L_%08X", v->i) : query_val_str(a, v);
    if (!strcmp(k, "4byte")) return avp_pyfmt(a, "0x%08X", val_int_or0(v));
    if (!strcmp(k, "2byte")) return avp_pyfmt(a, "0x%04X", val_int_or0(v));
    if (!strcmp(k, "byte")) return avp_pyfmt(a, "0x%02X", val_int_or0(v));
    return query_val_str(a, v);
}

const ShsmUnit *query_find_unit(QueryCtx *ctx, AvpStr text) {
    AvpArena *a = ctx->a;
    const ShsmUnit *u = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        u = shsm_get_unit(ctx->repo, avp_cz(a, text));
        avp_try_end(&t);
    } else if (!shsm_exc_is_key(t.code)) {
        avp_throw(t.code, "%s", t.msg);
    }
    if (u) return u;
    AvpStr tt = avp_str_replace(a, text, AVP_S("\\"), AVP_S("/"), -1);
    AvpStr suffix = avp_pyfmt(a, "/%S", &tt);
    const ShsmUnits *us = query_units(ctx);
    const ShsmUnit *hit = NULL;
    size_t n = 0;
    for (size_t i = 0; i < us->n; i++) {
        const ShsmUnit *x = us->v[i];
        if (avp_str_endswith(avp_str(x->name), suffix) || avp_str_eqz(tt, x->path ? x->path : "")) {
            if (!n) hit = x;
            n++;
        }
    }
    return n == 1 ? hit : NULL;
}
