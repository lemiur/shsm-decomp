/* query.py addr (with section_of) and unit. */
#include "query_b.h"

static AvpStr js(const AvpJson *v) { return (v && avp_json_is_str(v)) ? v->u.str : (AvpStr){NULL, 0}; }
static i64 ji(const AvpJson *v) { return (v && avp_json_is_int(v)) ? v->u.i : SHSM_NONE; }
static const AvpJson *cell(const AvpJson *rows, size_t r, size_t c) { return avp_json_at(avp_json_at(rows, r), c); }
static i64 or0(i64 v) { return v == SHSM_NONE ? 0 : v; }
static AvpStr opt(const char *z) { return z ? avp_str(z) : (AvpStr){NULL, 0}; }

/* ---- addr ---- */

ShsmSymbolTable *query_syms_split(QueryCtx *ctx) { return query_syms(ctx); }

AvpStr query_section_of(QueryCtx *ctx, i64 address) {
    AvpJson *ja = avp_json_int(ctx->a, address);
    AvpJson *rows = query_sql(ctx, "SELECT section FROM symbols WHERE address <= ? AND address + size > ? AND kind "
                                   "!= 'label' LIMIT 1", 2, ja, ja);
    if (avp_json_len(rows)) return js(cell(rows, 0, 0));
    rows = query_sql(ctx, "SELECT section FROM symbols WHERE address <= ? AND kind != 'label' ORDER BY address DESC "
                          "LIMIT 1", 1, ja);
    return avp_json_len(rows) ? js(cell(rows, 0, 0)) : (AvpStr){NULL, 0};
}

/* "  {demangled}" or "". */
static AvpStr dm_suffix(AvpArena *a, AvpStr name) {
    AvpStr d = query_demangled(a, name);
    return d.p ? avp_pyfmt(a, "  {%S}", &d) : AVP_S("");
}

int query_cmd_addr(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out) {
    AvpArena *a = ctx->a;
    AvpStr text = avp_args_str(args, "address");
    i64 addr = query_parse_addr(text);
    if (addr == SHSM_NONE) addr = query_parse_int(a, text);
    ShsmAsmIndex *ix = query_ix(ctx);
    ShsmSymRow srow;
    const ShsmSymRow *s = shsm_asmindex_containing(a, ix, addr, &srow) ? &srow : NULL;
    AvpStr unit = s ? avp_str(shsm_symrow_unit_name(a, s)) : opt(shsm_asmindex_unit_of(a, ix, addr));
    if (!unit.p) {
        const char *su = addr >= 0 ? shsm_sym_unit_of(query_syms_split(ctx), (u64)addr) : NULL;
        if (su && *su) unit = avp_pyfmt(a, "main/%s", su);
    }
    AvpStr sec = s ? s->section : query_section_of(ctx, addr);
    const char *src = addr >= 0 ? shsm_sym_unit_of(query_syms_split(ctx), (u64)addr) : NULL;
    const char *status = query_status(ctx, unit);
    AvpJson *doc = avp_json_obj(a);
    avp_json_set(a, doc, AVP_S("address"), avp_json_int(a, addr));
    avp_json_set(a, doc, AVP_S("section"), shsm_json_opt_str(a, sec));
    avp_json_set(a, doc, AVP_S("unit"), shsm_json_opt_str(a, unit));
    avp_json_set(a, doc, AVP_S("status"), avp_json_strz(a, status));
    avp_json_set(a, doc, AVP_S("source_unit"), src ? avp_json_strz(a, src) : avp_json_null());
    avp_json_set(a, doc, AVP_S("symbol"), s ? avp_json_str(a, s->name) : avp_json_null());
    avp_json_set(a, doc, AVP_S("offset"), s ? avp_json_int(a, addr - s->address) : avp_json_null());
    AvpStr sect = sec.p && sec.n ? sec : AVP_S("?"), ut = unit.p && unit.n ? unit : AVP_S("?");
    shsm_output_line(out, avp_pyfmt(a, "0x%08X  %S  %S (%s)%s", addr, &sect, &ut, status,
                                    src && *src ? "" : "  [dtk auto unit, no source]"));
    if (s) {
        AvpStr dm = dm_suffix(a, s->name);
        shsm_output_line(out, avp_pyfmt(a, "  in %S+0x%X  (%S 0x%08X size 0x%X)%S", &s->name, addr - s->address,
                                        &s->kind, s->address, or0(s->size), &dm));
        ShsmAsmFile *af = NULL;
        ShsmAsmBlock *b = shsm_asmindex_read(ix, s, &af);
        if (b && avp_str_eqz(s->kind, "function")) {
            for (size_t i = 0; i < b->rows.n; i++) {
                const ShsmAsmRow *r = &b->rows.v[i];
                if (r->address != addr) continue;
                shsm_output_line(out, avp_pyfmt(a, "  %08X  %-8S %S", r->address, &r->mnemonic, &r->operands));
                AvpStr t = r->operands.n ? avp_pyfmt(a, "%S %S", &r->mnemonic, &r->operands) : r->mnemonic;
                avp_json_set(a, doc, AVP_S("instruction"), avp_json_str(a, t));
            }
        } else if (b) {
            for (size_t i = 0; i < b->items.n; i++) {
                const ShsmAsmItem *it = &b->items.v[i];
                i64 sz = or0(it->size);
                if (it->address <= addr && addr < it->address + (sz > 1 ? sz : 1)) {
                    AvpStr t = query_item_text(ctx, it);
                    shsm_output_line(out, avp_pyfmt(a, "  item at +0x%X: %s %S", it->address - s->address, it->kind,
                                                    &t));
                    avp_json_set(a, doc, AVP_S("item"), shsm_asm_item_json(a, it));
                    break;
                }
            }
        }
        if (af) shsm_asm_free(af);
    } else {
        AvpJson *ja = avp_json_int(a, addr);
        AvpJson *before = query_sql(ctx, "SELECT name, address, size FROM symbols WHERE address <= ? AND kind != "
                                         "'label' ORDER BY address DESC LIMIT 1", 1, ja);
        AvpJson *after = query_sql(ctx, "SELECT name, address FROM symbols WHERE address > ? ORDER BY address LIMIT 1",
                                   1, ja);
        if (avp_json_len(before)) {
            AvpStr n = js(cell(before, 0, 0));
            i64 b0 = ji(cell(before, 0, 1)), sz = or0(ji(cell(before, 0, 2)));
            shsm_output_line(out, avp_pyfmt(a, "  after %S (0x%08X, ends 0x%08X)", &n, b0, b0 + sz));
        }
        if (avp_json_len(after)) {
            AvpStr n = js(cell(after, 0, 0));
            shsm_output_line(out, avp_pyfmt(a, "  before %S (0x%08X)", &n, ji(cell(after, 0, 1))));
        }
        avp_json_set(a, doc, AVP_S("before"), avp_json_len(before) ? avp_json_str(a, js(cell(before, 0, 0)))
                                                                   : avp_json_null());
        avp_json_set(a, doc, AVP_S("after"), avp_json_len(after) ? avp_json_str(a, js(cell(after, 0, 0)))
                                                                 : avp_json_null());
    }
    for (size_t i = 0; i < doc->u.obj.n; i++) {
        const AvpJsonMember *m = avp_json_member(doc, i);
        shsm_output_data(out, avp_cz(a, m->key), m->val);
    }
    return SHSM_EXIT_OK;
}

/* ---- unit ---- */

/* Measures' _num(v, 0) for an integer field: 0 when absent, int(v) of a number or numeric string. */
static i64 measure(const AvpJson *measures, const char *key) {
    const AvpJson *v = measures ? avp_json_getz(measures, key) : NULL;
    if (!v || avp_json_is_null(v)) return 0;
    if (avp_json_is_int(v)) return v->u.i;
    if (avp_json_is_str(v)) {
        i64 n = 0;
        if (avp_parse_int(avp_str_strip(v->u.str), 10, &n, NULL)) return n;
        double f = 0;
        if (avp_parse_float(avp_str_strip(v->u.str), &f)) return (i64)f;
        return 0;
    }
    if (avp_json_is_num(v)) return (i64)v->u.f;
    return 0;
}

typedef struct {
    AvpStr name;
    i64 start, end;
} Sec;
typedef AVP_VEC(Sec) Secs;

int query_cmd_unit(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out) {
    AvpArena *a = ctx->a;
    AvpStr text = avp_args_str(args, "unit");
    const ShsmUnit *u = query_find_unit(ctx, text);
    if (!u) {
        AvpStr t = avp_str_lower(a, avp_str_replace(a, text, AVP_S("\\"), AVP_S("/"), -1));
        const ShsmUnits *us = query_units(ctx);
        AVP_VEC(const ShsmUnit *) hits = {0};
        for (size_t i = 0; i < us->n; i++)
            if (avp_str_contains(avp_str_lower(a, avp_str(us->v[i]->name)), t)) avp_vec_push(&hits, us->v[i]);
        if (hits.n != 1) {
            AvpBuf b = {0};
            if (hits.n) {
                avp_buf_putz(&b, ": candidates ");
                for (size_t i = 0; i < hits.n && i < 8; i++) {
                    if (i) avp_buf_putz(&b, ", ");
                    avp_buf_putz(&b, hits.v[i]->name);
                }
            }
            AvpStr tail = avp_str_dup(a, avp_buf_str(&b));
            avp_buf_free(&b);
            avp_vec_free(&hits);
            shsm_raise(SHSM_EXC_USAGE, "%s", avp_cz(a, avp_pyfmt(a, "no unit %R%S", &text, &tail)));
        }
        u = hits.v[0];
        avp_vec_free(&hits);
    }
    Secs secs = {0};
    const ShsmSplits *splits = shsm_load_splits(ctx->repo);
    AvpVal *ui = avp_map_get(splits->unit_index, avp_str(u->path));
    if (ui) {
        const ShsmSplitUnit *su = &splits->units[ui->i];
        for (size_t i = 0; i < su->nranges; i++)
            avp_vec_push(&secs, ((Sec){avp_str(su->ranges[i].section), (i64)su->ranges[i].start,
                                       (i64)su->ranges[i].end}));
    }
    if (!secs.n) {
        AvpJson *rows = query_sql(ctx, "SELECT section, MIN(address), MAX(address + size) FROM symbols WHERE unit = ? "
                                       "AND kind != 'label' GROUP BY section ORDER BY MIN(address)",
                                  1, avp_json_strz(a, u->path));
        for (size_t i = 0; i < avp_json_len(rows); i++) {
            AvpStr name = js(cell(rows, i, 0));
            bool dup = false;
            for (size_t k = 0; k < secs.n; k++)
                if (avp_str_eq(secs.v[k].name, name)) {
                    secs.v[k].start = ji(cell(rows, i, 1)), secs.v[k].end = ji(cell(rows, i, 2)), dup = true;
                }
            if (!dup) avp_vec_push(&secs, ((Sec){name, ji(cell(rows, i, 1)), ji(cell(rows, i, 2))}));
        }
    }
    const ShsmReport *rp = query_report(ctx);
    const ShsmUnitReport *ur = rp ? shsm_report_unit(rp, u->name) : NULL;
    shsm_output_line(out, avp_pyfmt(a, "%s  %s  library %s", u->name, u->status, u->library && *u->library ? u->library : "-"));
    if (u->source && *u->source) shsm_output_line(out, avp_pyfmt(a, "  source  %s", u->source));
    AvpStr flags = avp_str_join(a, AVP_S(" "), u->cflags.v, u->cflags.n);
    if (u->mw_version && *u->mw_version)
        shsm_output_line(out, avp_pyfmt(a, "  cc      %s %S", u->mw_version, &flags));
    for (size_t i = 0; i < secs.n; i++)
        shsm_output_line(out, avp_pyfmt(a, "  %-10S 0x%08X..0x%08X (0x%X)", &secs.v[i].name, secs.v[i].start,
                                        secs.v[i].end, secs.v[i].end - secs.v[i].start));
    if (ur)
        shsm_output_line(out, avp_pyfmt(a, "  report  fuzzy %.1f%%, %d/%d functions exact", ur->fuzzy,
                                        measure(ur->measures, "matched_functions"),
                                        measure(ur->measures, "total_functions")));
    ShsmSymRows fns = shsm_asmindex_unit_symbols(a, query_ix(ctx), u->name, "function");
    AvpJson *rows = avp_json_arr(a);
    shsm_output_line(out, avp_pyfmt(a, "functions (%d):", fns.n));
    AvpStr uname = avp_str(u->name);
    for (size_t i = 0; i < fns.n; i++) {
        const ShsmSymRow *f = &fns.v[i];
        double p = 0;
        bool has = query_percent(ctx, f->name, uname, &p);
        AvpStr dm = query_demangled(a, f->name);
        AvpJson *r = avp_json_obj(a);
        avp_json_set(a, r, AVP_S("name"), avp_json_str(a, f->name));
        avp_json_set(a, r, AVP_S("address"), shsm_json_opt_int(a, f->address));
        avp_json_set(a, r, AVP_S("size"), shsm_json_opt_int(a, f->size));
        avp_json_set(a, r, AVP_S("percent"), has ? avp_json_float(a, p) : avp_json_null());
        avp_json_set(a, r, AVP_S("demangled"), shsm_json_opt_str(a, dm));
        avp_json_arr_push(a, rows, r);
        AvpStr ps = query_pct(a, has, p), dms = dm_suffix(a, f->name);
        shsm_output_line(out, avp_pyfmt(a, "  %08X %5X %S %S%S", f->address, or0(f->size), &ps, &f->name, &dms));
    }
    AvpJson *doc = avp_json_obj(a);
    avp_json_set(a, doc, AVP_S("unit"), avp_json_strz(a, u->name));
    avp_json_set(a, doc, AVP_S("status"), avp_json_strz(a, u->status));
    avp_json_set(a, doc, AVP_S("library"), u->library ? avp_json_strz(a, u->library) : avp_json_null());
    avp_json_set(a, doc, AVP_S("source"), u->source ? avp_json_strz(a, u->source) : avp_json_null());
    avp_json_set(a, doc, AVP_S("compiler"), u->mw_version ? avp_json_strz(a, u->mw_version) : avp_json_null());
    AvpJson *cf = avp_json_null();
    if (u->has_cflags) {
        cf = avp_json_arr(a);
        for (size_t i = 0; i < u->cflags.n; i++) avp_json_arr_push(a, cf, avp_json_str(a, u->cflags.v[i]));
    }
    avp_json_set(a, doc, AVP_S("cflags"), cf);
    AvpJson *js_secs = avp_json_obj(a);
    for (size_t i = 0; i < secs.n; i++) {
        AvpJson *pair = avp_json_arr(a);
        avp_json_arr_push(a, pair, shsm_json_opt_int(a, secs.v[i].start));
        avp_json_arr_push(a, pair, shsm_json_opt_int(a, secs.v[i].end));
        avp_json_set(a, js_secs, secs.v[i].name.p ? secs.v[i].name : AVP_S("null"), pair);
    }
    avp_json_set(a, doc, AVP_S("sections"), js_secs);
    avp_json_set(a, doc, AVP_S("functions"), rows);
    if (avp_args_bool(args, "data")) {
        ShsmSymRows objs = shsm_asmindex_unit_symbols(a, query_ix(ctx), u->name, "object");
        shsm_output_line(out, avp_pyfmt(a, "data (%d):", objs.n));
        AvpJson *data = avp_json_arr(a);
        for (size_t i = 0; i < objs.n; i++) {
            const ShsmSymRow *o = &objs.v[i];
            AvpJson *r = avp_json_obj(a);
            avp_json_set(a, r, AVP_S("name"), avp_json_str(a, o->name));
            avp_json_set(a, r, AVP_S("address"), shsm_json_opt_int(a, o->address));
            avp_json_set(a, r, AVP_S("size"), shsm_json_opt_int(a, o->size));
            avp_json_set(a, r, AVP_S("section"), shsm_json_opt_str(a, o->section));
            avp_json_arr_push(a, data, r);
            AvpStr sec = o->section.p ? o->section : AVP_S("None");
            shsm_output_line(out, avp_pyfmt(a, "  %08X %5X %-8S %S", o->address, or0(o->size), &sec, &o->name));
        }
        avp_json_set(a, doc, AVP_S("data"), data);
    }
    avp_vec_free(&secs);
    for (size_t i = 0; i < doc->u.obj.n; i++) {
        const AvpJsonMember *m = avp_json_member(doc, i);
        shsm_output_data(out, avp_cz(a, m->key), m->val);
    }
    return SHSM_EXIT_OK;
}
