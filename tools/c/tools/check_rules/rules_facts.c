/* rules_facts: the per-file facts dump of a parse (check_rules --dump-facts), the C side of
 * diff/dump_facts.py. The JSON shape is documented in rules_lex.md ("Facts dump"). */
#include "rules_parse.h"

static void w_toks(AvpJsonW *w, const RlTok *t, size_t n) {
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < n; i++) {
        avp_jsonw_begin_array(w);
        avp_jsonw_strz(w, rl_kind_names[t[i].k]);
        avp_jsonw_str(w, t[i].t);
        avp_jsonw_i64(w, t[i].line);
        avp_jsonw_end_array(w);
    }
    avp_jsonw_end_array(w);
}

static void w_name_line(AvpJsonW *w, AvpStr name, i32 line) {
    avp_jsonw_begin_array(w);
    avp_jsonw_str(w, name);
    avp_jsonw_i64(w, line);
    avp_jsonw_end_array(w);
}

static void w_set_sorted(AvpJsonW *w, const AvpSet *s) {
    AvpStrVec v = avp_set_sorted(s);
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < v.n; i++) avp_jsonw_str(w, v.v[i]);
    avp_jsonw_end_array(w);
    avp_vec_free(&v);
}

static int cmp_i32(const void *x, const void *y, void *ctx) {
    (void)ctx;
    i32 a = *(const i32 *)x, b = *(const i32 *)y;
    return a < b ? -1 : a > b;
}

void rl_dump_facts(AvpJsonW *w, RlParsed *P) {
    avp_jsonw_begin_object(w);
    avp_jsonw_kv_str(w, "path", P->path);
    if (!P->ok) {
        avp_jsonw_kv_str(w, "error", avp_str("AttributeError"));
        avp_jsonw_end_object(w);
        return;
    }
    avp_jsonw_kv_bool(w, "is_header", P->is_header);
    avp_jsonw_kv_bool(w, "is_cpp", P->is_cpp);

    /* storage_macros: dict(argument) updated with the file's own, as sorted items */
    {
        AvpMap *m = avp_map_new();
        AvpMapIter it = {0};
        if (P->storage_global)
            while (avp_map_next(P->storage_global, &it)) avp_map_put(m, it.key, NULL)->i = it.val->i;
        it = (AvpMapIter){0};
        while (avp_map_next(P->storage_own, &it)) avp_map_put(m, it.key, NULL)->i = it.val->i;
        AvpStrVec keys = avp_map_keys(m);
        avp_sort_strs(keys.v, keys.n);
        avp_jsonw_key(w, AVP_S("storage_macros"));
        avp_jsonw_begin_array(w);
        for (size_t i = 0; i < keys.n; i++) {
            avp_jsonw_begin_array(w);
            avp_jsonw_str(w, keys.v[i]);
            avp_jsonw_strz(w, avp_map_get(m, keys.v[i])->i == RL_STORAGE_STATIC ? "static" : "extern");
            avp_jsonw_end_array(w);
        }
        avp_jsonw_end_array(w);
        avp_vec_free(&keys);
        avp_map_free(m);
    }
    avp_jsonw_kv_i64(w, "nlines", (i64)P->lines.n);

    avp_jsonw_key(w, AVP_S("toks"));
    w_toks(w, P->toks.v, P->toks.n);
    avp_jsonw_key(w, AVP_S("match"));
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < P->toks.n; i++) {
        if (P->match[i] < 0) continue;
        avp_jsonw_begin_array(w);
        avp_jsonw_i64(w, (i64)i);
        avp_jsonw_i64(w, P->match[i]);
        avp_jsonw_end_array(w);
    }
    avp_jsonw_end_array(w);

    avp_jsonw_key(w, AVP_S("comments"));
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < P->comments.n; i++) {
        const RlComment *c = &P->comments.v[i];
        avp_jsonw_begin_array(w);
        avp_jsonw_i64(w, c->line);
        avp_jsonw_i64(w, c->end_line);
        avp_jsonw_str(w, c->text);
        avp_jsonw_bool(w, c->trailing);
        avp_jsonw_end_array(w);
    }
    avp_jsonw_end_array(w);

    avp_jsonw_key(w, AVP_S("directives"));
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < P->directives.n; i++) {
        const RlDirective *d = &P->directives.v[i];
        avp_jsonw_begin_array(w);
        avp_jsonw_i64(w, d->line);
        avp_jsonw_i64(w, d->end_line);
        avp_jsonw_str(w, d->name);
        avp_jsonw_str(w, d->rest);
        w_toks(w, d->toks.v, d->toks.n);
        avp_jsonw_end_array(w);
    }
    avp_jsonw_end_array(w);

    avp_jsonw_key(w, AVP_S("includes"));
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < P->includes.n; i++) avp_jsonw_str(w, P->includes.v[i]);
    avp_jsonw_end_array(w);

    avp_jsonw_key(w, AVP_S("asm_sites"));
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < P->asm_sites.n; i++) {
        avp_jsonw_begin_array(w);
        avp_jsonw_i64(w, P->asm_sites.v[i].line);
        avp_jsonw_str(w, P->asm_sites.v[i].name);
        avp_jsonw_str(w, P->asm_sites.v[i].kind);
        avp_jsonw_end_array(w);
    }
    avp_jsonw_end_array(w);
    avp_jsonw_key(w, AVP_S("masks"));
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < P->masks.n; i++) {
        avp_jsonw_begin_array(w);
        avp_jsonw_i64(w, P->masks.v[i].lo);
        avp_jsonw_i64(w, P->masks.v[i].hi);
        avp_jsonw_end_array(w);
    }
    avp_jsonw_end_array(w);
    avp_jsonw_key(w, AVP_S("asm_ids"));
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < P->asm_ids.n; i++) {
        avp_jsonw_begin_array(w);
        avp_jsonw_i64(w, P->asm_ids.v[i].tok);
        avp_jsonw_begin_array(w);
        for (size_t k = 0; k < P->asm_ids.v[i].ids.n; k++) avp_jsonw_str(w, P->asm_ids.v[i].ids.v[k]);
        avp_jsonw_end_array(w);
        avp_jsonw_end_array(w);
    }
    avp_jsonw_end_array(w);

    avp_jsonw_key(w, AVP_S("funcs"));
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < P->p.funcs.n; i++) {
        const RlFunc *f = &P->p.funcs.v[i];
        avp_jsonw_begin_object(w);
        avp_jsonw_kv_str(w, "name", f->name);
        avp_jsonw_kv_i64(w, "line", f->line);
        avp_jsonw_kv_i64(w, "end_line", f->end_line);
        avp_jsonw_kv_i64(w, "head", f->head);
        avp_jsonw_kv_i64(w, "lparen", f->lparen);
        avp_jsonw_kv_i64(w, "rparen", f->rparen);
        avp_jsonw_kv_i64(w, "body", f->body);
        avp_jsonw_kv_i64(w, "body_end", f->body_end);
        avp_jsonw_kv_bool(w, "is_asm", f->is_asm);
        avp_jsonw_kv_bool(w, "is_static", f->is_static);
        avp_jsonw_key(w, AVP_S("params"));
        avp_jsonw_begin_array(w);
        for (size_t k = 0; k < f->params.n; k++) {
            avp_jsonw_begin_array(w);
            avp_jsonw_str(w, f->params.v[k].name);
            avp_jsonw_str(w, f->params.v[k].type);
            avp_jsonw_bool(w, f->params.v[k].reg);
            avp_jsonw_end_array(w);
        }
        avp_jsonw_end_array(w);
        avp_jsonw_kv_str(w, "in_class", f->in_class);
        avp_jsonw_kv_bool(w, "virtual", f->virtual_);
        avp_jsonw_kv_bool(w, "template", f->template_);
        avp_jsonw_kv_bool(w, "anon_ns", f->anon_ns);
        avp_jsonw_key(w, AVP_S("addr"));
        if (f->has_addr) avp_jsonw_i64(w, f->addr);
        else avp_jsonw_null(w);
        avp_jsonw_kv_str(w, "status", avp_str("ok"));
        avp_jsonw_end_object(w);
    }
    avp_jsonw_end_array(w);

    avp_jsonw_key(w, AVP_S("aggs"));
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < P->p.aggs.n; i++) {
        const RlAgg *g = &P->p.aggs.v[i];
        avp_jsonw_begin_object(w);
        avp_jsonw_kv_str(w, "kind", g->kind);
        avp_jsonw_kv_str(w, "name", g->name);
        avp_jsonw_kv_i64(w, "line", g->line);
        avp_jsonw_key(w, AVP_S("members"));
        avp_jsonw_begin_array(w);
        for (size_t k = 0; k < g->members.n; k++) avp_jsonw_str(w, g->members.v[k]);
        avp_jsonw_end_array(w);
        avp_jsonw_key(w, AVP_S("member_decls"));
        avp_jsonw_begin_array(w);
        for (size_t k = 0; k < g->member_decls.n; k++) {
            avp_jsonw_begin_array(w);
            avp_jsonw_str(w, g->member_decls.v[k].type);
            avp_jsonw_str(w, g->member_decls.v[k].name);
            avp_jsonw_i64(w, g->member_decls.v[k].line);
            avp_jsonw_end_array(w);
        }
        avp_jsonw_end_array(w);
        avp_jsonw_key(w, AVP_S("first_hidden"));
        if (g->first_hidden < 0) avp_jsonw_null(w);
        else avp_jsonw_i64(w, g->first_hidden);
        avp_jsonw_kv_str(w, "body_text", g->body_text);
        avp_jsonw_kv_i64(w, "keyword_line", g->keyword_line);
        avp_jsonw_end_object(w);
    }
    avp_jsonw_end_array(w);

    avp_jsonw_key(w, AVP_S("vars"));
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < P->p.vars.n; i++) {
        const RlVar *v = &P->p.vars.v[i];
        avp_jsonw_begin_object(w);
        avp_jsonw_kv_str(w, "name", v->name);
        avp_jsonw_kv_str(w, "type_text", v->type_text);
        avp_jsonw_kv_i64(w, "line", v->line);
        avp_jsonw_kv_bool(w, "is_extern", v->is_extern);
        avp_jsonw_kv_bool(w, "is_static", v->is_static);
        avp_jsonw_kv_bool(w, "is_const", v->is_const);
        avp_jsonw_kv_bool(w, "has_init", v->has_init);
        avp_jsonw_kv_bool(w, "addr_bound", v->addr_bound);
        avp_jsonw_kv_str(w, "ns", v->ns);
        avp_jsonw_kv_bool(w, "in_function", v->in_function);
        avp_jsonw_end_object(w);
    }
    avp_jsonw_end_array(w);

    avp_jsonw_key(w, AVP_S("fwds"));
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < P->p.fwds.n; i++) {
        avp_jsonw_begin_array(w);
        avp_jsonw_str(w, P->p.fwds.v[i].kind);
        avp_jsonw_str(w, P->p.fwds.v[i].name);
        avp_jsonw_i64(w, P->p.fwds.v[i].line);
        avp_jsonw_end_array(w);
    }
    avp_jsonw_end_array(w);

    avp_jsonw_key(w, AVP_S("protos"));
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < P->p.protos.n; i++) {
        const RlProto *p = &P->p.protos.v[i];
        avp_jsonw_begin_object(w);
        avp_jsonw_kv_str(w, "name", p->name);
        avp_jsonw_kv_i64(w, "line", p->line);
        avp_jsonw_kv_bool(w, "is_extern", p->is_extern);
        avp_jsonw_kv_bool(w, "empty_params", p->empty_params);
        avp_jsonw_end_object(w);
    }
    avp_jsonw_end_array(w);

    avp_jsonw_key(w, AVP_S("typedefs"));
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < P->p.typedefs.n; i++) w_name_line(w, P->p.typedefs.v[i].name, P->p.typedefs.v[i].line);
    avp_jsonw_end_array(w);
    avp_jsonw_key(w, AVP_S("virtuals"));
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < P->p.virtuals.n; i++) w_name_line(w, P->p.virtuals.v[i].name, P->p.virtuals.v[i].line);
    avp_jsonw_end_array(w);

    avp_jsonw_key(w, AVP_S("func_names"));
    w_set_sorted(w, P->func_names);

    avp_jsonw_key(w, AVP_S("comments_by_line"));
    avp_jsonw_begin_array(w);
    for (i32 ln = 0; ln < P->cbl_lines; ln++) {
        size_t n;
        const i32 *ix = rl_comments_on(P, ln, &n);
        if (!n) continue;
        avp_jsonw_begin_array(w);
        avp_jsonw_i64(w, ln);
        avp_jsonw_begin_array(w);
        for (size_t k = 0; k < n; k++) avp_jsonw_i64(w, ix[k]);
        avp_jsonw_end_array(w);
        avp_jsonw_end_array(w);
    }
    avp_jsonw_end_array(w);

    avp_jsonw_kv_bool(w, "cpp_syntax", rl_cpp_syntax(P));
    avp_jsonw_key(w, AVP_S("offset_macros"));
    w_set_sorted(w, rl_offset_macros(P));
    avp_jsonw_key(w, AVP_S("volatile_noted"));
    w_set_sorted(w, rl_volatile_noted(P));

    /* func_at at every line where its answer can change: 0, each f.line and each f.end_line + 1 */
    {
        size_t nf = P->p.funcs.n, nc = 0;
        i32 *cand = AVP_NEWN(P->a, i32, 2 * nf + 1);
        cand[nc++] = 0;
        for (size_t i = 0; i < nf; i++) {
            cand[nc++] = P->p.funcs.v[i].line;
            cand[nc++] = P->p.funcs.v[i].end_line + 1;
        }
        avp_sort(cand, nc, sizeof *cand, cmp_i32, NULL, false);
        avp_jsonw_key(w, AVP_S("func_at"));
        avp_jsonw_begin_array(w);
        for (size_t i = 0; i < nc; i++) {
            if (i && cand[i] == cand[i - 1]) continue;
            avp_jsonw_begin_array(w);
            avp_jsonw_i64(w, cand[i]);
            avp_jsonw_i64(w, rl_func_at(P, cand[i]));
            avp_jsonw_end_array(w);
        }
        avp_jsonw_end_array(w);
    }
    avp_jsonw_key(w, AVP_S("no_copy_in_retail"));
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < P->p.funcs.n; i++) avp_jsonw_bool(w, rl_no_copy_in_retail(P, &P->p.funcs.v[i]));
    avp_jsonw_end_array(w);

    avp_jsonw_key(w, AVP_S("notes"));
    avp_jsonw_begin_object(w);
    {
        static const char *const KEYS[] = {"volatile", "delay", "byteptr", "pad", "exception"};
        const ShsmRe *res[] = {rl_re_volatile_note(), rl_re_delay_note(), rl_re_byteptr_note(), rl_re_pad_note(),
                               rl_re_exception()};
        for (size_t r = 0; r < 5; r++) {
            avp_jsonw_key(w, avp_str(KEYS[r]));
            avp_jsonw_begin_array(w);
            for (i32 ln = 1; ln <= (i32)P->lines.n; ln++)
                if (rl_has_note(P, ln, res[r], true)) avp_jsonw_i64(w, ln);
            avp_jsonw_end_array(w);
        }
    }
    avp_jsonw_end_object(w);
    avp_jsonw_end_object(w);
}
