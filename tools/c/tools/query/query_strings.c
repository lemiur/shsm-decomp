/* query.py strings and find-string (strings_used, _string_users). */
#include "query_a.h"

static AvpStr none_str(AvpStr s) { return s.p ? s : AVP_S("None"); }

/* ---- strings ---- */

QueryStrUses query_strings_used(QueryCtx *ctx, ShsmTarget target) {
    AvpArena *a = ctx->a;
    ShsmAsmIndex *ix = query_ix(ctx);
    ShsmStrRows direct = shsm_asmindex_strings_of(a, ix, target, false);
    AvpIMap *seen = avp_imap_new();
    QueryStrUses out = {0};
    for (size_t i = 0; i < direct.n; i++) {
        avp_imap_put(seen, (u64)direct.v[i].address, NULL);
        avp_vec_push(&out, ((QueryStrUse){direct.v[i].owner, direct.v[i].address, direct.v[i].text, {NULL, 0}}));
    }
    ShsmStrRows all = shsm_asmindex_strings_of(a, ix, target, true);
    AvpMap *objects = NULL;
    for (size_t i = 0; i < all.n; i++) {
        const ShsmStrRow *x = &all.v[i];
        if (avp_imap_has(seen, (u64)x->address)) continue;
        if (!objects) {
            objects = avp_map_new();
            ShsmDataRefs dt = shsm_asmindex_data_targets(a, ix, target);
            for (size_t k = 0; k < dt.n; k++) avp_map_put(objects, dt.v[k].sym, NULL);
        }
        static const char *const ptr_kinds[] = {"ptr"};
        ShsmRefs rs = shsm_asmindex_refs_at(a, ix, x->address, ptr_kinds, 1);
        AvpStr via = AVP_S("?");
        for (size_t k = 0; k < rs.n; k++)
            if (rs.v[k].from_sym.p && avp_map_has(objects, rs.v[k].from_sym)) {
                if (rs.v[k].from_sym.n) via = rs.v[k].from_sym; /* `via or "?"` */
                break;
            }
        avp_vec_push(&out, ((QueryStrUse){x->owner, x->address, x->text, via}));
    }
    if (objects) avp_map_free(objects);
    avp_imap_free(seen);
    return out;
}

int query_cmd_strings(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out) {
    AvpArena *a = ctx->a;
    AvpStr target = avp_args_str(args, "target");
    const ShsmUnit *u = query_find_unit(ctx, target);
    AVP_VEC(const ShsmSymRow *) blocks = {0};
    AvpStr title;
    if (u) {
        ShsmSymRows rows = shsm_asmindex_unit_symbols(a, query_ix(ctx), u->name, NULL);
        for (size_t i = 0; i < rows.n; i++)
            if (avp_str_eqz(rows.v[i].kind, "function")) avp_vec_push(&blocks, &rows.v[i]);
        title = avp_pyfmt(a, "%s: %d functions", u->name, blocks.n);
    } else {
        const ShsmSymRow *s = query_resolve(ctx, target, NULL);
        avp_vec_push(&blocks, s);
        QuerySymDesc d = query_describe_sym(ctx, s->name, SHSM_NONE);
        title = query_sym_line(a, &d, AVP_S(""));
    }
    shsm_output_line(out, title);
    AvpJson *found = avp_json_arr(a);
    for (size_t i = 0; i < blocks.n; i++) {
        const ShsmSymRow *b = blocks.v[i];
        QueryStrUses us = query_strings_used(ctx, shsm_target_sym(b));
        for (size_t k = 0; k < us.n; k++) {
            const QueryStrUse *x = &us.v[k];
            AvpJson *j = avp_json_obj(a);
            avp_json_set(a, j, AVP_S("function"), avp_json_str(a, b->name));
            avp_json_set(a, j, AVP_S("object"), shsm_json_opt_str(a, x->owner));
            avp_json_set(a, j, AVP_S("address"), avp_json_int(a, x->address));
            avp_json_set(a, j, AVP_S("text"), avp_json_str(a, x->text));
            avp_json_set(a, j, AVP_S("via"), shsm_json_opt_str(a, x->via));
            avp_json_arr_push(a, found, j);
            AvpStr owner = none_str(x->owner), lit = query_cstr(a, x->text, 90);
            AvpStr via = x->via.p ? avp_pyfmt(a, "  via %S", &x->via) : AVP_S("");
            AvpStr from = blocks.n > 1 ? avp_pyfmt(a, "   <- %S", &b->name) : AVP_S("");
            shsm_output_line(out, avp_pyfmt(a, "  %08X %-14S %S%S%S", x->address, &owner, &lit, &via, &from));
        }
        avp_vec_free(&us);
    }
    if (!avp_json_len(found)) shsm_output_linez(out, "  (no string literals addressed directly)");
    shsm_output_data(out, "strings", found);
    avp_vec_free(&blocks);
    return SHSM_EXIT_OK;
}

/* ---- find-string ---- */

static void add_user(QueryUsers *users, AvpStr key, AvpStr kind) {
    for (size_t i = 0; i < users->n; i++)
        if (avp_str_eq(users->v[i].name, key)) return;
    avp_vec_push(users, ((QueryUser){key, kind}));
}

QueryUsers query_string_users(QueryCtx *ctx, i64 addr) {
    AvpArena *a = ctx->a;
    ShsmAsmIndex *ix = query_ix(ctx);
    QueryUsers users = {0};
    ShsmRefs rs = shsm_asmindex_refs_at(a, ix, addr, NULL, 0);
    if (rs.n) {
        for (size_t i = 0; i < rs.n; i++) {
            const ShsmAsmRef *r = &rs.v[i];
            AvpStr key = r->from_sym.p && r->from_sym.n ? r->from_sym : avp_pyfmt(a, "@%08X", r->from_addr);
            add_user(&users, key, avp_str(r->kind));
        }
        return users;
    }
    /* a pooled constant: code addresses the pool's base and adds the offset in the load */
    ShsmSymRow sr;
    if (!shsm_asmindex_containing(a, ix, addr, &sr) || !avp_str_eqz(sr.kind, "object") || sr.address == SHSM_NONE ||
        sr.address == addr)
        return users;
    rs = shsm_asmindex_refs_at(a, ix, sr.address, NULL, 0);
    for (size_t i = 0; i < rs.n; i++) {
        const ShsmAsmRef *r = &rs.v[i];
        if (!strcmp(r->kind, "jumptable")) continue;
        AvpStr key = r->from_sym.p && r->from_sym.n ? r->from_sym : avp_pyfmt(a, "@%08X", r->from_addr);
        add_user(&users, key, avp_pyfmt(a, "%s (pool)", r->kind));
    }
    return users;
}

/* ", ".join(items[:k]) + " +N" when there are more. */
static AvpStr join_capped(AvpArena *a, const AvpStr *items, size_t n, size_t k) {
    AvpStr s = avp_str_join(a, AVP_S(", "), items, n < k ? n : k);
    if (n > k) s = avp_pyfmt(a, "%S +%d", &s, n - k);
    return s;
}

typedef struct {
    ShsmRe *rx;
    AvpStr needle; /* lower-cased under -i */
    bool icase;
} Matcher;

static bool matches(AvpArena *a, const Matcher *m, AvpStr text) {
    if (m->rx) {
        ShsmReMatch rm;
        return shsm_re_search(m->rx, text, 0, (i64)text.n, &rm);
    }
    if (m->icase) return avp_str_contains(avp_str_lower(a, text), m->needle);
    return avp_str_contains(text, m->needle);
}

int query_cmd_find_string(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out) {
    AvpArena *a = ctx->a;
    AvpStr text = avp_args_str(args, "text");
    bool icase = avp_args_bool(args, "ignore_case");
    Matcher m = {NULL, text, icase};
    if (avp_args_bool(args, "regex")) {
        char err[512];
        if (!shsm_re_compile(text, icase ? SHSM_RE_I : 0, &m.rx, err, sizeof err))
            shsm_raise(SHSM_EXC_USAGE, "bad regex: %s", err);
    } else if (icase) {
        m.needle = avp_str_lower(a, text);
    }
    AvpJson *rows = query_sql(ctx, "SELECT owner, address, text FROM strings ORDER BY address", 0);
    AvpJson *hits = avp_json_arr(a);
    size_t nrows = avp_json_len(rows);
    for (size_t i = 0; i < nrows; i++) {
        AvpJson *r = avp_json_at(rows, i);
        AvpStr t = avp_json_str_or(avp_json_at(r, 2), AVP_S(""));
        if (!matches(a, &m, t)) continue;
        AvpJson *jo = avp_json_at(r, 0);
        i64 addr = avp_json_at(r, 1)->u.i;
        QueryUsers users = query_string_users(ctx, addr);
        AvpJson *h = avp_json_obj(a);
        avp_json_set(a, h, AVP_S("object"), jo);
        avp_json_set(a, h, AVP_S("address"), avp_json_int(a, addr));
        avp_json_set(a, h, AVP_S("text"), avp_json_str(a, t));
        const char *unit = shsm_asmindex_unit_of(a, query_ix(ctx), addr);
        avp_json_set(a, h, AVP_S("unit"), unit ? avp_json_strz(a, unit) : avp_json_null());
        AvpJson *ju = avp_json_arr(a);
        avp_json_set(a, h, AVP_S("users"), ju);
        AvpStr *shown = AVP_NEWN(a, AvpStr, users.n + 1);
        for (size_t k = 0; k < users.n; k++) {
            AvpJson *x = avp_json_obj(a);
            avp_json_set(a, x, AVP_S("name"), avp_json_str(a, users.v[k].name));
            avp_json_set(a, x, AVP_S("kind"), avp_json_str(a, users.v[k].kind));
            avp_json_arr_push(a, ju, x);
        }
        for (size_t k = 0; k < users.n; k++) {
            AvpStr un = users.v[k].name;
            const ShsmSymRow *s = query_sym(ctx, un);
            if (s && avp_str_eqz(s->kind, "object")) {
                ShsmRefs rs = shsm_asmindex_refs_to(a, query_ix(ctx), shsm_target_sym(s), NULL, 0);
                AvpMap *set = avp_map_new();
                AvpStrVec up = {0};
                for (size_t j = 0; j < rs.n; j++) {
                    AvpStr f = rs.v[j].from_sym.p && rs.v[j].from_sym.n ? rs.v[j].from_sym
                                                                        : avp_pyfmt(a, "@%08X", rs.v[j].from_addr);
                    bool created = false;
                    avp_map_put(set, f, &created);
                    if (created) avp_vec_push(&up, f);
                }
                avp_sort_strs(up.v, up.n);
                AvpJson *jup = avp_json_arr(a);
                for (size_t j = 0; j < up.n; j++) avp_json_arr_push(a, jup, avp_json_str(a, up.v[j]));
                avp_json_set(a, avp_json_at(ju, k), AVP_S("users"), jup);
                AvpStr list = join_capped(a, up.v, up.n, 3);
                un = avp_pyfmt(a, "%S (<- %S)", &un, &list);
                avp_vec_free(&up);
                avp_map_free(set);
            }
            shown[k] = un;
        }
        avp_json_arr_push(a, hits, h);
        AvpStr us = join_capped(a, shown, users.n, 4);
        if (!us.n) us = AVP_S("(no direct reference)");
        AvpStr lit = query_cstr(a, t, 70);
        shsm_output_line(out, avp_pyfmt(a, "%08X %S  <- %S", addr, &lit, &us));
        avp_vec_free(&users);
    }
    shsm_output_data(out, "strings", hits);
    if (!avp_json_len(hits)) {
        shsm_output_linez(out, "no string matches");
        return SHSM_EXIT_FINDING;
    }
    return SHSM_EXIT_OK;
}
