/* query.py vtable (with load_classes); mangle.mangle_scope is query_mangle_scope (query_mangle.c). */
#include "query_b.h"

/* ---- classes.json ---- */

#define CLASSES_SLOT 7 /* ctx->extra[] slot of the loaded list */
static const AvpJson CLASSES_NONE = {0};

const AvpJson *query_load_classes(QueryCtx *ctx) {
    if (ctx->extra[CLASSES_SLOT]) {
        const AvpJson *c = ctx->extra[CLASSES_SLOT];
        return c == &CLASSES_NONE ? NULL : c;
    }
    AvpArena *a = ctx->a;
    const char *p = avp_path_join(a, ctx->repo->version_build_dir, "classes.json");
    AvpStr text = {0};
    const AvpJson *out = NULL;
    if (avp_read_text(a, p, AVP_TM_UTF8, &text)) {
        AvpJsonError err = {0};
        AvpJson *doc = avp_json_parse(a, text, &err);
        if (doc) {
            if (!avp_json_is_obj(doc)) shsm_raise(SHSM_EXC_TYPE, "'%s' object has no attribute 'get'",
                                                  avp_json_is_arr(doc) ? "list" : "object");
            const AvpJson *c = avp_json_getz(doc, "classes");
            out = c ? c : avp_json_arr(a);
        }
    }
    ctx->extra[CLASSES_SLOT] = out ? (void *)out : (void *)&CLASSES_NONE;
    return out;
}

/* ---- vtable ---- */

/* str(v) of a JSON value as Python prints it with %s (None for a missing value). */
static AvpStr py_str(AvpArena *a, const AvpJson *v) {
    if (!v || avp_json_is_null(v)) return AVP_S("None");
    if (avp_json_is_str(v)) return v->u.str;
    if (v->kind == AVP_JSON_BOOL) return v->b ? AVP_S("True") : AVP_S("False");
    if (avp_json_is_int(v)) return v->big ? v->u.str : avp_pyfmt(a, "%d", v->u.i);
    if (avp_json_is_num(v)) return avp_pyfmt(a, "%v", v->u.f);
    return avp_json_dumps(a, v, NULL);
}

static size_t jlen(const AvpJson *v) { return v && avp_json_is_arr(v) ? avp_json_len(v) : 0; }

static bool has_ptr_items(QueryCtx *ctx, const ShsmSymRow *s) {
    ShsmAsmFile *af = NULL;
    ShsmAsmBlock *b = shsm_asmindex_read(query_ix(ctx), s, &af);
    bool any = false;
    for (size_t i = 0; b && i < b->items.n; i++) any = any || !strcmp(b->items.v[i].kind, "ptr");
    if (af) shsm_asm_free(af);
    return any;
}

int query_cmd_vtable(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out) {
    AvpArena *a = ctx->a;
    AvpStr t = avp_args_str(args, "target");
    const AvpJson *classes = NULL;
    bool loaded = false;
    const ShsmSymRow *s = NULL;
    AvpTry tr;
    if (AVP_TRY(&tr)) {
        s = query_resolve(ctx, t, "object");
        avp_try_end(&tr);
    } else {
        if (tr.code != SHSM_EXC_USAGE) avp_throw(tr.code, "%s", tr.msg);
        s = NULL;
    }
    const AvpJson *cls = NULL;
    if (!s || !(avp_str_startswith(s->name, AVP_S("__vt__")) || has_ptr_items(ctx, s))) {
        AvpStr vt = {NULL, 0};
        AvpStrVec parts = avp_str_split(t, AVP_S("::"), -1);
        if (AVP_TRY(&tr)) {
            AvpStr sc = query_mangle_scope(a, parts.v, parts.n);
            vt = avp_pyfmt(a, "__vt__%S", &sc);
            avp_try_end(&tr);
        } else {
            if (!shsm_exc_is_value(tr.code) && tr.code != SHSM_EXC_TYPE) avp_throw(tr.code, "%s", tr.msg);
        }
        avp_vec_free(&parts);
        s = vt.p ? query_sym(ctx, vt) : NULL;
        if (!s) {
            classes = query_load_classes(ctx);
            loaded = true;
            for (size_t i = 0; i < jlen(classes); i++) {
                const AvpJson *c = avp_json_at(classes, i);
                if (!avp_json_is_obj(c)) continue;
                const AvpJson *nm = avp_json_getz(c, "name"), *vs = avp_json_getz(c, "vtable_symbol");
                if (nm && avp_json_is_str(nm) && avp_str_eq(nm->u.str, t) && vs && avp_json_truthy(vs)) {
                    cls = c;
                    s = avp_json_is_str(vs) ? query_sym(ctx, vs->u.str) : NULL;
                    break;
                }
            }
        }
        if (!s)
            shsm_raise(SHSM_EXC_USAGE, "%s", avp_cz(a, avp_pyfmt(a, "no vtable for %R (not a __vt__ symbol, a class "
                                                                    "with one, or a classes.json name)", &t)));
    }
    if (!cls) {
        if (!loaded || !classes) classes = query_load_classes(ctx);
        for (size_t i = 0; i < jlen(classes); i++) {
            const AvpJson *c = avp_json_at(classes, i);
            const AvpJson *vs = avp_json_is_obj(c) ? avp_json_getz(c, "vtable_symbol") : NULL;
            if (vs && avp_json_is_str(vs) && avp_str_eq(vs->u.str, s->name)) {
                cls = c;
                break;
            }
        }
    }
    ShsmAsmFile *af = NULL;
    ShsmAsmBlock *b = shsm_asmindex_read(query_ix(ctx), s, &af);
    AvpStr unit = avp_str(shsm_symrow_unit_name(a, s));
    AvpStr sec = s->section.p ? s->section : AVP_S("None");
    shsm_output_line(out, avp_pyfmt(a, "%S  %S 0x%08X size 0x%X  %S %s", &s->name, &sec, s->address,
                                    s->size == SHSM_NONE ? 0 : s->size, &unit, query_status(ctx, unit)));
    AvpIMap *words = avp_imap_new();
    if (cls) {
        const AvpJson *bases = avp_json_getz(cls, "bases");
        AvpBuf bb = {0};
        for (size_t i = 0; i < jlen(bases); i++) {
            const AvpJson *x = avp_json_at(bases, i);
            if (i) avp_buf_putz(&bb, ", ");
            avp_buf_put(&bb, py_str(a, avp_json_is_obj(x) ? avp_json_getz(x, "class") : NULL));
            avp_buf_putz(&bb, "@");
            avp_buf_put(&bb, py_str(a, avp_json_is_obj(x) ? avp_json_getz(x, "offset") : NULL));
        }
        AvpStr bs = avp_str_dup(a, avp_buf_str(&bb));
        avp_buf_free(&bb);
        AvpStr tail = bs.n ? avp_pyfmt(a, "  bases: %S", &bs) : AVP_S("");
        AvpStr nm = py_str(a, avp_json_getz(cls, "name")), src = py_str(a, avp_json_getz(cls, "name_source"));
        shsm_output_line(out, avp_pyfmt(a, "  class %S (name from %S)%S", &nm, &src, &tail));
        const AvpJson *vtab = avp_json_getz(cls, "vtable");
        const AvpJson *ws = vtab && avp_json_is_obj(vtab) ? avp_json_getz(vtab, "words") : NULL;
        for (size_t i = 0; i < jlen(ws); i++) {
            const AvpJson *w = avp_json_at(ws, i);
            const AvpJson *ix = avp_json_is_obj(w) ? avp_json_getz(w, "index") : NULL;
            if (ix && avp_json_is_int(ix)) avp_imap_put(words, (u64)ix->u.i, NULL)->p = (void *)w;
        }
    } else if (!classes) {
        shsm_output_line(out, avp_pyfmt(a, "  (no build/%s/classes.json: run build/tools/classes.exe scan for slot "
                                           "annotations)", ctx->repo->version));
    }
    AvpJson *slots = avp_json_arr(a);
    for (size_t i = 0; b && i < b->items.n; i++) {
        const ShsmAsmItem *it = &b->items.v[i];
        i64 off = it->address - s->address;
        AvpVal *wv = avp_imap_get(words, (u64)i);
        const AvpJson *w = wv ? wv->p : NULL;
        bool wtrue = w && avp_json_truthy(w);
        AvpStr text;
        AvpJson *target = avp_json_null(), *value;
        bool ptr = !strcmp(it->kind, "ptr");
        if (ptr) {
            QuerySymDesc d = query_describe_sym(ctx, it->sym, SHSM_NONE);
            text = query_sym_line(a, &d, AVP_S(""));
            target = query_desc_json(a, &d);
            value = avp_json_null();
        } else {
            AvpStr itx = query_item_text(ctx, it);
            text = avp_pyfmt(a, "   %S", &itx);
            value = avp_json_getz(shsm_asm_item_json(a, it), "value");
        }
        AvpStr ann = AVP_S("");
        if (wtrue) {
            const AvpJson *base = avp_json_getz(w, "base");
            AvpStr st = py_str(a, avp_json_getz(w, "status"));
            AvpStr bstr = py_str(a, base);
            AvpStr from = base && avp_json_truthy(base) ? avp_pyfmt(a, " from %S", &bstr) : AVP_S("");
            ann = avp_pyfmt(a, "  [%S%S]", &st, &from);
        }
        shsm_output_line(out, avp_pyfmt(a, "  [%2d] +0x%-3X %S%S", i, off, &text, &ann));
        AvpJson *slot = avp_json_obj(a);
        avp_json_set(a, slot, AVP_S("index"), avp_json_int(a, (i64)i));
        avp_json_set(a, slot, AVP_S("offset"), avp_json_int(a, off));
        avp_json_set(a, slot, AVP_S("kind"), avp_json_strz(a, it->kind));
        avp_json_set(a, slot, AVP_S("target"), target);
        avp_json_set(a, slot, AVP_S("value"), value ? value : avp_json_null());
        avp_json_set(a, slot, AVP_S("classes"), wtrue ? (AvpJson *)w : avp_json_null());
        avp_json_arr_push(a, slots, slot);
    }
    avp_imap_free(words);
    shsm_output_data(out, "vtable", shsm_symrow_json(a, s));
    AvpJson *cj = avp_json_null();
    if (cls) {
        cj = avp_json_obj(a);
        static const char *const keys[] = {"name", "name_source", "bases", "unit"};
        for (size_t i = 0; i < 4; i++) {
            AvpJson *v = avp_json_getz(cls, keys[i]);
            avp_json_set(a, cj, avp_str(keys[i]), v ? v : avp_json_null());
        }
    }
    shsm_output_data(out, "class_", cj);
    shsm_output_data(out, "slots", slots);
    if (af) shsm_asm_free(af);
    return SHSM_EXIT_OK;
}
