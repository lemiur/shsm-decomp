/* query.py data (struct_period, _data_as). */
#include <math.h>
#include <string.h>
#include "query_a.h"

static const char *const CTYPE[][2] = {{"4byte", "u32"}, {"2byte", "u16"}, {"byte", "u8"},
                                       {"float", "float"}, {"double", "double"}, {"ptr", "void*"}};

static const char *ctype_or_null(const char *kind) {
    for (size_t i = 0; i < sizeof CTYPE / sizeof *CTYPE; i++)
        if (!strcmp(CTYPE[i][0], kind)) return CTYPE[i][1];
    return NULL;
}

const char *query_ctype(const char *kind) {
    const char *t = ctype_or_null(kind);
    return t ? t : kind;
}

/* (kind, size) of an item for struct_period: a 4byte with a symbol counts as a ptr. */
static const char *period_kind(const ShsmAsmItem *it) {
    return strcmp(it->kind, "4byte") || !it->sym.p || !it->sym.n ? it->kind : "ptr";
}

int query_struct_period(const ShsmAsmItem *items, size_t n) {
    size_t top = n / 2 < 16 ? n / 2 : 16;
    for (size_t p = 2; p <= top; p++) {
        if (n % p) continue;
        bool distinct = false;
        for (size_t i = 1; i < p && !distinct; i++)
            if (strcmp(period_kind(&items[i]), period_kind(&items[0]))) distinct = true;
        if (!distinct) continue;
        bool same = true;
        for (size_t i = 0; i < n && same; i++)
            same = !strcmp(period_kind(&items[i]), period_kind(&items[i % p])) && items[i].size == items[i % p].size;
        if (same) return (int)p;
    }
    return 0;
}

static bool is_kind(const ShsmAsmItem *it, const char *k) { return !strcmp(it->kind, k); }

static i64 skip_value(const ShsmAsmItem *it) { return it->value.t == SHSM_VAL_INT ? it->value.i : 0; }

static int cmp_cz(const void *x, const void *y, void *c) {
    (void)c;
    return strcmp(*(const char *const *)x, *(const char *const *)y);
}

/* _data_as(ctx, s, view, out, doc) */
static int data_as(QueryCtx *ctx, const ShsmSymRow *s, const char *view, ShsmOutput *out, AvpJson *doc) {
    AvpArena *a = ctx->a;
    i64 size = s->size == SHSM_NONE ? 0 : s->size;
    const QueryDol *dol = query_dol(ctx);
    AvpStr data = dol ? query_dol_read(a, dol, s->address, size) : (AvpStr){NULL, 0};
    if (!data.p) shsm_raise(SHSM_EXC_USAGE, "cannot read 0x%llX bytes at 0x%08llX from the retail DOL",
                            (long long)size, (long long)s->address);
    bool is_ptr = !strcmp(view, "ptr"), is_float = !strcmp(view, "float");
    i64 width = !strcmp(view, "u8") ? 1 : !strcmp(view, "u16") ? 2 : 4;
    i64 n = size / width;
    const char *ctype = is_ptr ? "void*" : view;
    const unsigned char *p = (const unsigned char *)data.p;
    shsm_output_line(out, avp_pyfmt(a, "// forced view %s over the retail bytes", view));
    AvpStrVec lines = {0};
    avp_vec_push(&lines, avp_pyfmt(a, "%s %S[%d] = {", ctype, &s->name, n));
    u32 *vals = AVP_NEWN(a, u32, (size_t)n + 1);
    for (i64 i = 0; i < n; i++) {
        const unsigned char *q = p + i * width;
        vals[i] = width == 1 ? q[0] : width == 2 ? (u32)(q[0] << 8 | q[1])
                                                 : (u32)q[0] << 24 | (u32)q[1] << 16 | (u32)q[2] << 8 | q[3];
    }
    if (is_ptr) {
        for (i64 i = 0; i < n; i++) {
            u32 v = vals[i];
            AvpStr t;
            if (v == 0) t = AVP_S("NULL");
            else if (v >= 0x80000000u && v < 0x81800000u) t = avp_str(shsm_sym_describe(a, query_syms(ctx), v));
            else t = avp_pyfmt(a, "0x%08X", v);
            if (avp_str_startswith(t, AVP_S("0x")) && v) t = avp_pyfmt(a, "(void*)%S", &t);
            AvpStr cell = avp_pyfmt(a, "%S,", &t);
            avp_vec_push(&lines, avp_pyfmt(a, "    %-40S // [%d] +0x%X", &cell, i, i * 4));
        }
    } else {
        i64 per = width == 1 ? 16 : width == 2 ? 8 : 4;
        for (i64 i = 0; i < n; i += per) {
            AvpStrVec txt = {0};
            for (i64 k = i; k < n && k < i + per; k++) {
                if (is_float) {
                    float f;
                    memcpy(&f, &vals[k], 4);
                    avp_vec_push(&txt, query_fmt_float(a, true, (double)f, true));
                } else {
                    avp_vec_push(&txt, avp_pyfmt(a, "0x%0*X", width * 2, vals[k]));
                }
            }
            AvpStr row = avp_str_join(a, AVP_S(", "), txt.v, txt.n);
            avp_vec_push(&lines, avp_pyfmt(a, "    /* +0x%02X */ %S,", i * width, &row));
            avp_vec_free(&txt);
        }
    }
    avp_vec_push(&lines, AVP_S("};"));
    if (size % width) avp_vec_push(&lines, avp_pyfmt(a, "// + 0x%X trailing bytes not shown", size % width));
    shsm_output_lines(out, lines.v, lines.n);
    avp_json_set(a, doc, AVP_S("view"), avp_json_strz(a, view));
    avp_json_set(a, doc, AVP_S("draft"), avp_json_str(a, avp_str_join(a, AVP_S("\n"), lines.v, lines.n)));
    avp_vec_free(&lines);
    return SHSM_EXIT_OK;
}

static void put_doc(AvpArena *a, ShsmOutput *out, AvpJson *doc) {
    for (size_t i = 0; i < avp_json_len(doc); i++) {
        const AvpJsonMember *m = avp_json_member(doc, i);
        shsm_output_data(out, avp_cz(a, m->key), m->val);
    }
}

int query_cmd_data(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out) {
    AvpArena *a = ctx->a;
    const ShsmSymRow *s = query_resolve(ctx, avp_args_str(args, "sym"), NULL);
    ShsmAsmFile *af = NULL;
    ShsmAsmBlock *b = avp_str_eqz(s->kind, "object") ? shsm_asmindex_read(query_ix(ctx), s, &af) : NULL;
    if (!b) {
        AvpStr kind = s->kind.p ? s->kind : AVP_S("None");
        shsm_raise(SHSM_EXC_USAGE, "%s is a %s, not a data object", avp_cz(a, s->name), avp_cz(a, kind));
    }
    i64 size = s->size == SHSM_NONE ? 0 : s->size;
    AvpStr section = s->section.p ? s->section : AVP_S("None");
    AvpStr unit = avp_str(shsm_symrow_unit_name(a, s));
    const char *status = query_status(ctx, unit);
    shsm_output_line(out, avp_pyfmt(a, "// %S  %S 0x%08X size 0x%X  %S %s", &s->name, &section, s->address, size,
                                    &unit, status));
    AvpStr dm = query_demangled(a, s->name);
    if (dm.p) shsm_output_line(out, avp_pyfmt(a, "// %S", &dm));
    const ShsmAsmItem *items = b->items.v;
    size_t n = b->items.n;
    AvpJson *doc = avp_json_obj(a);
    avp_json_set(a, doc, AVP_S("symbol"), shsm_symrow_json(a, s));
    AvpJson *jitems = avp_json_arr(a);
    for (size_t i = 0; i < n; i++) avp_json_arr_push(a, jitems, shsm_asm_item_json(a, &items[i]));
    avp_json_set(a, doc, AVP_S("items"), jitems);
    AvpStr as_view = avp_args_str(args, "as_view");
    if (as_view.p) {
        int rc = data_as(ctx, s, avp_cz(a, as_view), out, doc);
        put_doc(a, out, doc);
        if (af) shsm_asm_free(af);
        return rc;
    }
    /* kinds (without skips) and their set */
    AVP_VEC(const char *) kinds = {0};
    AVP_VEC(const char *) kset = {0};
    bool has_skip = false;
    for (size_t i = 0; i < n; i++) {
        if (is_kind(&items[i], "skip")) {
            has_skip = true;
            continue;
        }
        avp_vec_push(&kinds, items[i].kind);
        bool have = false;
        for (size_t k = 0; k < kset.n && !have; k++) have = !strcmp(kset.v[k], items[i].kind);
        if (!have) avp_vec_push(&kset, items[i].kind);
    }
    bool vt = avp_str_startswith(s->name, AVP_S("__vt__"));
    bool kset_ptr = false, kset_ptr4 = true;
    for (size_t k = 0; k < kset.n; k++) {
        if (!strcmp(kset.v[k], "ptr")) kset_ptr = true;
        else if (strcmp(kset.v[k], "4byte")) kset_ptr4 = false;
    }
    AvpStrVec lines = {0};
    const char *view;
    int period = 0;
    if (!kinds.n) {
        avp_vec_push(&lines, avp_pyfmt(a, "u8 %S[0x%X];  // no initialiser (%S, zero-filled)", &s->name, size, &section));
        view = "zero";
    } else if (kset.n == 1 && !strcmp(kset.v[0], "rel")) {
        view = "jump table";
        AvpStr first = items[0].sym.p && items[0].sym.n ? items[0].sym : AVP_S("?");
        avp_vec_push(&lines, avp_pyfmt(a, "// jump table of %S, one case per slot", &first));
        avp_vec_push(&lines, avp_pyfmt(a, "void* %S[%d] = {", &s->name, n));
        for (size_t i = 0; i < n; i++) {
            AvpStr t = query_item_text(ctx, &items[i]);
            AvpStr cell = avp_pyfmt(a, "%S,", &t);
            avp_vec_push(&lines, avp_pyfmt(a, "    %-14S // [%d] +0x%X", &cell, i, items[i].address - s->address));
        }
        avp_vec_push(&lines, AVP_S("};"));
    } else if (kinds.n == 1 && !strcmp(kinds.v[0], "ptr") && n == 1) {
        view = "pointer";
        const ShsmAsmItem *it = &items[0];
        ShsmStrRows txt = shsm_asmindex_strings_of(a, query_ix(ctx), shsm_target_sym(s), false);
        AvpStr t = query_item_text(ctx, it);
        if (txt.n) {
            AvpStr lit = query_cstr(a, txt.v[0].text, 0);
            avp_vec_push(&lines, avp_pyfmt(a, "const char* %S = %S;", &s->name, &lit));
            avp_vec_push(&lines, avp_pyfmt(a, "// %S", &t));
        } else {
            AvpStr idm = query_demangled(a, it->sym);
            AvpStr note = idm.p ? avp_pyfmt(a, "  // %S", &idm) : AVP_S("");
            avp_vec_push(&lines, avp_pyfmt(a, "void* %S = %S;%S", &s->name, &t, &note));
        }
    } else if (!vt && !has_skip && (period = query_struct_period(items, n)) != 0) {
        int p = period;
        view = avp_cz(a, avp_pyfmt(a, "array of %d-field structs", p));
        AvpStr *types = AVP_NEWN(a, AvpStr, (size_t)p);
        for (int i = 0; i < p; i++) types[i] = avp_str(query_ctype(items[i].kind));
        AvpStr tj = avp_str_join(a, AVP_S(", "), types, (size_t)p);
        avp_vec_push(&lines, avp_pyfmt(a, "struct /* guess: %S */ %S[%d] = {", &tj, &s->name, n / (size_t)p));
        AvpStr *cells = AVP_NEWN(a, AvpStr, (size_t)p);
        for (size_t i = 0; i < n; i += (size_t)p) {
            for (int k = 0; k < p; k++) cells[k] = query_item_text(ctx, &items[i + (size_t)k]);
            AvpStr row = avp_str_join(a, AVP_S(", "), cells, (size_t)p);
            avp_vec_push(&lines, avp_pyfmt(a, "    { %S },%s// [%d] +0x%X", &row, " ", i / (size_t)p,
                                           items[i].address - s->address));
        }
        avp_vec_push(&lines, AVP_S("};"));
    } else if ((kset_ptr && kset_ptr4) || vt) {
        view = vt ? "vtable" : "pointer table";
        avp_vec_push(&lines, avp_pyfmt(a, "void* %S[%d] = {", &s->name, n));
        for (size_t i = 0; i < n; i++) {
            const ShsmAsmItem *it = &items[i];
            AvpStr note = AVP_S("");
            if (is_kind(it, "ptr")) {
                AvpStr idm = query_demangled(a, it->sym);
                if (idm.p) note = avp_pyfmt(a, "  %S", &idm);
            } else if (vt && i < 2) {
                note = i == 0 ? AVP_S("  header (RTTI, this offset)") : AVP_S("  header");
            }
            AvpStr t = query_item_text(ctx, it);
            AvpStr cell = avp_pyfmt(a, "%S,", &t);
            avp_vec_push(&lines,
                         avp_pyfmt(a, "    %-40S // [%d] +0x%X%S", &cell, i, it->address - s->address, &note));
        }
        avp_vec_push(&lines, AVP_S("};"));
    } else if (kset.n == 1 && (!strcmp(kset.v[0], "string") || !strcmp(kset.v[0], "string16")) && kinds.n == 1) {
        view = "string";
        size_t at = 0;
        while (at < n && strcmp(items[at].kind, kinds.v[0])) at++;
        AvpStr t = query_item_text(ctx, &items[at]);
        avp_vec_push(&lines, avp_pyfmt(a, "%s %S[] = %S;", !strcmp(kinds.v[0], "string16") ? "wchar_t" : "char",
                                       &s->name, &t));
    } else if (kset.n == 1 && ctype_or_null(kset.v[0])) {
        const char *k = kinds.v[0];
        const char *ct = ctype_or_null(k);
        view = avp_cz(a, avp_pyfmt(a, "array of %s", ct));
        if (kinds.n == 1) {
            size_t at = 0;
            while (at < n && is_kind(&items[at], "skip")) at++;
            AvpStr t = query_item_text(ctx, &items[at]);
            avp_vec_push(&lines, avp_pyfmt(a, "%s %S = %S;", ct, &s->name, &t));
        } else {
            size_t per = !strcmp(k, "byte") ? 16 : !strcmp(k, "2byte") ? 8 : 4;
            avp_vec_push(&lines, avp_pyfmt(a, "%s %S[%d] = {", ct, &s->name, kinds.n));
            AvpStrVec row = {0};
            for (size_t i = 0; i < n; i++) {
                const ShsmAsmItem *it = &items[i];
                if (is_kind(it, "skip")) {
                    if (row.n) {
                        AvpStr r = avp_str_join(a, AVP_S(" "), row.v, row.n);
                        avp_vec_push(&lines, avp_pyfmt(a, "    %S", &r));
                        row.n = 0;
                    }
                    avp_vec_push(&lines, avp_pyfmt(a, "    /* pad 0x%X */", skip_value(it)));
                    continue;
                }
                if (!row.n) avp_vec_push(&row, avp_pyfmt(a, "/* +0x%02X */", it->address - s->address));
                AvpStr t = query_item_text(ctx, it);
                avp_vec_push(&row, avp_pyfmt(a, "%S,", &t));
                if (row.n > per) {
                    AvpStr r = avp_str_join(a, AVP_S(" "), row.v, row.n);
                    avp_vec_push(&lines, avp_pyfmt(a, "    %S", &r));
                    row.n = 0;
                }
            }
            if (row.n) {
                AvpStr r = avp_str_join(a, AVP_S(" "), row.v, row.n);
                avp_vec_push(&lines, avp_pyfmt(a, "    %S", &r));
            }
            avp_vec_free(&row);
            avp_vec_push(&lines, AVP_S("};"));
        }
    } else {
        view = "mixed (struct)";
        avp_vec_push(&lines, avp_pyfmt(a, "struct /* guess */ %S = {", &s->name));
        for (size_t i = 0; i < n; i++) {
            const ShsmAsmItem *it = &items[i];
            i64 off = it->address - s->address;
            if (is_kind(it, "skip")) {
                avp_vec_push(&lines, avp_pyfmt(a, "    /* pad 0x%X */                              // +0x%X",
                                               skip_value(it), off));
                continue;
            }
            AvpStr t = query_item_text(ctx, it);
            AvpStr cell = avp_pyfmt(a, "%S,", &t);
            avp_vec_push(&lines, avp_pyfmt(a, "    %-40S // +0x%X %s", &cell, off, query_ctype(it->kind)));
        }
        avp_vec_push(&lines, AVP_S("};"));
    }
    i64 end = s->address;
    for (size_t i = 0; i < n; i++) {
        i64 e = items[i].address + (items[i].size == SHSM_NONE ? 0 : items[i].size);
        if (i == 0 || e > end) end = e;
    }
    if (n && end < s->address + size)
        avp_vec_push(&lines, avp_pyfmt(a, "// + 0x%X bytes of trailing padding", s->address + size - end));
    avp_sort(kset.v, kset.n, sizeof *kset.v, cmp_cz, NULL, false);
    AvpBuf kb = {0};
    for (size_t k = 0; k < kset.n; k++) {
        if (k) avp_buf_putz(&kb, ", ");
        avp_buf_putz(&kb, kset.v[k]);
    }
    AvpStr ks = kset.n ? avp_str_dup(a, avp_buf_str(&kb)) : AVP_S("none");
    avp_buf_free(&kb);
    shsm_output_line(out, avp_pyfmt(a, "// the type is a guess from the asm item directives (%S): view %s", &ks, view));
    shsm_output_lines(out, lines.v, lines.n);
    avp_json_set(a, doc, AVP_S("view"), avp_json_strz(a, view));
    avp_json_set(a, doc, AVP_S("draft"), avp_json_str(a, avp_str_join(a, AVP_S("\n"), lines.v, lines.n)));
    put_doc(a, out, doc);
    avp_vec_free(&lines);
    avp_vec_free(&kinds);
    avp_vec_free(&kset);
    if (af) shsm_asm_free(af);
    return SHSM_EXIT_OK;
}
