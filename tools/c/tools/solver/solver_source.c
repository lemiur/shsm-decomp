/* solver_source: reading a source text with levers' tokenizer and definition finder: whether the unit defines a
 * function, and which names a function's body (and the bodies of the same-text functions it names, transitively)
 * mention. Phase 0 reads the preprocessed text for mentions, so a call written through a macro (FMOD_Memory_Free)
 * or a header inline is seen. Levers interns token texts in one process-wide table: call from one thread. */
#include "solver.h"

SvSource *sv_source_parse(AvpArena *a, AvpStr text) {
    SvSource *s = AVP_NEW(a, SvSource);
    s->text = text;
    AvpTry t;
    if (AVP_TRY(&t)) {
        s->toks = lv_tokenize(a, text);
        s->code = lv_code_tokens(a, s->toks);
        s->m = lv_match_brackets(a, s->code);
        s->defs = lv_scan_definitions(a, s->code, s->m);
        s->ok = true;
        avp_try_end(&t);
    } else {
        s->ok = false;
        s->error = avp_strdup(a, t.msg ? t.msg : "parse error");
    }
    return s;
}

bool sv_source_find(AvpArena *a, const SvSource *s, const char *symbol, LvFnDef *out) {
    if (!s || !s->ok) return false;
    AvpTry t;
    bool found = false;
    if (AVP_TRY(&t)) {
        *out = lv_find_definition(a, s->code, s->m, symbol);
        found = true;
        avp_try_end(&t);
    } else {
        found = false;
    }
    return found;
}

i64 sv_source_line(const SvSource *s, i32 tok) {
    if (!s || !s->ok || tok < 0 || tok >= s->code.n) return 0;
    i64 line = 1;
    i32 end = s->code.v[tok].s;
    for (i32 i = 0; i < end && (size_t)i < s->text.n; i++)
        if (s->text.p[i] == '\n') line++;
    return line;
}

const char *sv_source_name(AvpArena *a, const char *symbol) {
    if (!symbol || !*symbol || symbol[0] == '<') return NULL;
    if (!strncmp(symbol, "__nwa__", 7) || !strncmp(symbol, "__nw__", 6)) return "new";
    if (!strncmp(symbol, "__dla__", 7) || !strncmp(symbol, "__dl__", 6)) return "delete";
    LvTarget tg = lv_symbol_target(a, symbol);
    if (!strncmp(symbol, "__ct__", 6) || !strncmp(symbol, "__dt__", 6)) {
        if (!tg.cls) return symbol;
        size_t n = strcspn(tg.cls, "<");
        return avp_cz(a, avp_strn(tg.cls, n));
    }
    return tg.name ? tg.name : symbol;
}

SvMentions sv_source_mentions(AvpArena *a, const SvSource *s, const LvFnDef *fn) {
    SvMentions out = {0};
    out.names = avp_map_new();
    if (!s || !s->ok || !fn) return out;
    /* definitions by name */
    AvpMap *by_name = avp_map_new();
    for (size_t i = 0; i < s->defs.n; i++) {
        bool created = false;
        AvpVal *v = avp_map_put(by_name, avp_str(s->defs.v[i].name), &created);
        AvpPtrVec *list = v->p;
        if (created || !list) {
            list = AVP_NEW(a, AvpPtrVec);
            v->p = list;
        }
        avp_vec_push(list, &s->defs.v[i]);
    }
    AvpPtrVec queue = {0};
    AvpPtrVec owners = {0};
    AvpSet *visited = avp_set_new();
    avp_vec_push(&queue, (void *)fn);
    avp_vec_push(&owners, (void *)"");
    avp_set_add(visited, avp_str(avp_sprintf(a, "%d", fn->lbrace)));
    for (size_t q = 0; q < queue.n; q++) {
        const LvFnDef *d = queue.v[q];
        const char *owner = owners.v[q];
        for (i32 i = d->lbrace + 1; i < d->rbrace && i < s->code.n; i++) {
            const LvTok *t = &s->code.v[i];
            if (t->k != LV_ID) continue;
            bool created = false;
            AvpVal *v = avp_map_put(out.names, avp_str(t->t), &created);
            if (created) v->p = (void *)owner;
            AvpVal *defs = avp_map_get(by_name, avp_str(t->t));
            if (!defs || !defs->p) continue;
            const AvpPtrVec *list = defs->p;
            for (size_t k = 0; k < list->n; k++) {
                const LvFnDef *h = list->v[k];
                if (!avp_set_add(visited, avp_str(avp_sprintf(a, "%d", h->lbrace)))) continue;
                avp_vec_push(&queue, (void *)h);
                avp_vec_push(&owners, (void *)h->name);
            }
        }
    }
    avp_vec_free(&queue);
    avp_vec_free(&owners);
    avp_set_free(visited);
    out.ok = true;
    return out;
}
