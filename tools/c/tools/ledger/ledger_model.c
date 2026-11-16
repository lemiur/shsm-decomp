/* ledger: the exceptions record and the model (parse_exceptions, build_model of tools/ledger.py). */
#include "ledger.h"
#include <string.h>

static AvpStr pair_key(AvpArena *a, AvpStr u, AvpStr f) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_put(&b, u);
    avp_buf_putc(&b, '\0');
    avp_buf_put(&b, f);
    return avp_buf_str(&b);
}

AvpStr ledger_py_str(AvpArena *a, const AvpJson *v) {
    if (!v || v->kind == AVP_JSON_NULL) return AVP_S("None");
    switch (v->kind) {
    case AVP_JSON_BOOL: return v->b ? AVP_S("True") : AVP_S("False");
    case AVP_JSON_STRING: return v->u.str;
    case AVP_JSON_FLOAT: return avp_repr_f64(a, v->u.f);
    default: return avp_json_dumps(a, v, NULL);
    }
}

i64 ledger_py_int(const AvpJson *v) {
    if (!v) return 0;
    if (v->kind == AVP_JSON_INT) return v->u.i;
    if (v->kind == AVP_JSON_FLOAT) return (i64)v->u.f;
    if (v->kind == AVP_JSON_BOOL) return v->b ? 1 : 0;
    if (v->kind == AVP_JSON_STRING) {
        i64 out = 0;
        bool overflow = false;
        if (avp_parse_int(v->u.str, 10, &out, &overflow)) return out;
        avp_throw(AVP_ERR_FORMAT, "ValueError: invalid literal for int() with base 10: '%.*s'", AVP_SF(v->u.str));
    }
    avp_throw(AVP_ERR_FORMAT, "TypeError: int() argument must be a string, a bytes-like object or a real number");
}

/* f.get(key, "") as a string view ("" when absent or not a string). */
static AvpStr jstr(const AvpJson *o, const char *key) {
    const AvpJson *v = avp_json_is_obj(o) ? avp_json_getz(o, key) : NULL;
    return avp_json_is_str(v) ? v->u.str : AVP_S("");
}
static const AvpJson *jarr(const AvpJson *o, const char *key) {
    const AvpJson *v = avp_json_is_obj(o) ? avp_json_getz(o, key) : NULL;
    return avp_json_is_arr(v) ? v : NULL;
}
static size_t jlen(const AvpJson *arr) { return arr ? avp_json_len(arr) : 0; }
/* (x.get("metadata") or {}) */
static const AvpJson *jmeta(const AvpJson *o) {
    const AvpJson *v = avp_json_is_obj(o) ? avp_json_getz(o, "metadata") : NULL;
    return avp_json_truthy(v) ? v : NULL;
}

LedgerExcRows ledger_parse_exceptions(AvpArena *a, AvpStr text) {
    LedgerExcRows rows = {0};
    AvpStr section = AVP_S("");
    AvpStrVec lines = avp_str_split(text, AVP_S("\n"), -1);
    for (size_t li = 0; li < lines.n; li++) {
        AvpStr ln = lines.v[li];
        if (avp_str_startswith(ln, AVP_S("#"))) {
            section = avp_str_strip(avp_str_lstrip_chars(ln, AVP_S("#")));
            continue;
        }
        if (!avp_str_startswith(ln, AVP_S("|"))) continue;
        AvpStr rest = avp_str_strip(avp_str_replace(a, ln, AVP_S("|"), AVP_S(""), -1));
        bool sep = true;
        for (size_t i = 0; i < rest.n && sep; i++) sep = rest.p[i] == '-' || rest.p[i] == ':' || rest.p[i] == ' ';
        if (sep) continue;
        AvpStrVec cells = ledger_table_cells(a, ln);
        if (cells.n && (avp_str_eqz(cells.v[0], "unit") || avp_str_eqz(cells.v[0], "function") ||
                        avp_str_eqz(cells.v[0], "location")))
            continue;
        const char *table;
        if (avp_str_startswith(section, AVP_S("MATCH-PENDING"))) table = "MATCH-PENDING";
        else if (avp_str_startswith(section, AVP_S("ORIGINAL-ASM"))) table = "ORIGINAL-ASM";
        else if (avp_str_startswith(section, AVP_S("Listed inline-asm helpers"))) table = "helpers";
        else if (avp_str_startswith(section, AVP_S("Review rulings"))) table = "rulings";
        else continue;
        LedgerExcRow *r = AVP_NEW(a, LedgerExcRow);
        r->table = table;
        r->line = (i64)li + 1;
        r->cells = cells;
        r->ticks = AVP_NEWN(a, AvpStrVec, cells.n ? cells.n : 1);
        for (size_t i = 0; i < cells.n; i++) r->ticks[i] = ledger_ticks(cells.v[i]);
        if (strcmp(table, "rulings") != 0 && cells.n >= 2) {
            for (size_t i = 0; i < r->ticks[0].n; i++) avp_vec_push(&r->units, ledger_unit_key(a, r->ticks[0].v[i]));
            for (size_t i = 0; i < r->ticks[1].n; i++) avp_vec_push(&r->functions, r->ticks[1].v[i]);
        } else if (strcmp(table, "rulings") == 0 && cells.n) {
            for (size_t i = 0; i < r->ticks[0].n; i++) avp_vec_push(&r->functions, r->ticks[0].v[i]);
        }
        avp_vec_push(&rows, r);
    }
    avp_vec_free(&lines);
    return rows;
}

i64 ledger_unit_first_addr(const LedgerUnit *u) {
    if (!u->fns.n) return 0;
    i64 m = u->fns.v[0]->addr;
    for (size_t i = 1; i < u->fns.n; i++)
        if (u->fns.v[i]->addr < m) m = u->fns.v[i]->addr;
    return m;
}

/* `^pad_\d{2}_[0-9A-Fa-f]{8}_[A-Za-z0-9]+$`: dtk's generated filler function name */
static bool dtk_pad_name(AvpStr s) {
    if (s.n < 4 + 3 + 9 + 1 || memcmp(s.p, "pad_", 4) != 0) return false;
    size_t i = 4;
    for (int k = 0; k < 2; k++, i++)
        if (s.p[i] < '0' || s.p[i] > '9') return false;
    if (s.p[i++] != '_') return false;
    for (int k = 0; k < 8; k++, i++) {
        char c = s.p[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    if (s.p[i++] != '_' || i >= s.n) return false;
    for (; i < s.n; i++) {
        char c = s.p[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) return false;
    }
    return true;
}

static void problem(AvpArena *a, LedgerModel *m, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    AvpStr s = avp_pyvfmt(a, fmt, ap);
    va_end(ap);
    avp_vec_push(&m->problems, s);
}

/* unconfirmed_in_source: the first 12 lines (universal newlines) of the unit's source. */
static bool unconfirmed_in_source(AvpArena *a, const char *root, AvpStr rel) {
    if (!rel.n) return false;
    char *p = avp_path_join(a, root, avp_cz(a, rel));
    if (!avp_isfile(p)) return false;
    AvpStr text;
    if (!avp_read_text(a, p, AVP_TM_UTF8_REPLACE, &text)) return false;
    size_t i = 0;
    for (int k = 0; k < 12 && i < text.n; k++) {
        while (i < text.n && text.p[i] != '\n') i++;
        if (i < text.n) i++;
    }
    return ledger_unconfirmed_text(avp_strn(text.p, i));
}

LedgerModel *ledger_build_model(AvpArena *a, const AvpJson *report, const LedgerCfgVec *configure,
                                const LedgerExcRows *exc, const AvpJson *checker, const char *root, AvpMap *symbols) {
    LedgerModel *m = AVP_NEW(a, LedgerModel);
    m->units = avp_map_new();
    AvpMap *cfg = avp_map_new();
    for (size_t i = 0; i < configure->n; i++) {
        bool c;
        avp_map_put(cfg, configure->v[i].unit, &c)->p = (void *)&configure->v[i];
    }
    const AvpJson *meas = avp_json_getz(report, "measures");
    if (!avp_json_is_obj(meas)) meas = NULL;
    const AvpJson *runits = jarr(report, "units");

    /* checker facts, keyed by (unit, name) */
    AvpMap *chk_status = avp_map_new();
    AvpSet *asm_fn = avp_set_new(), *asm_block = avp_set_new();
    const AvpJson *cfns = jarr(checker, "functions");
    for (size_t i = 0; i < jlen(cfns); i++) {
        const AvpJson *f = avp_json_at(cfns, i);
        if (!avp_str_startswith(jstr(f, "path"), AVP_S("src/"))) continue;
        AvpStr key = pair_key(a, ledger_unit_key(a, jstr(f, "path")), jstr(f, "name"));
        bool c;
        avp_map_put(chk_status, key, &c)->p = (void *)f;
        if (avp_str_eqz(jstr(f, "status"), "ORIGINAL-ASM")) avp_set_add(asm_fn, key);
    }
    for (int pass = 0; pass < 2; pass++) {
        const AvpJson *list = jarr(checker, pass ? "findings" : "allowed");
        for (size_t i = 0; i < jlen(list); i++) {
            const AvpJson *f = avp_json_at(list, i);
            if (!avp_str_eqz(jstr(f, "rule"), "R18") || !avp_str_startswith(jstr(f, "path"), AVP_S("src/")) ||
                !avp_json_truthy(avp_json_getz(f, "function")))
                continue;
            AvpStr key = pair_key(a, ledger_unit_key(a, jstr(f, "path")), jstr(f, "function"));
            if (!avp_str_eqz(jstr(f, "status"), "allowed")) continue;
            if (avp_str_contains(jstr(f, "message"), AVP_S("written in assembly"))) avp_set_add(asm_fn, key);
            else avp_set_add(asm_block, key);
        }
    }

    AvpSet *asm_table = avp_set_new(), *pend_table = avp_set_new();
    AvpMap *rules_for = avp_map_new();
    for (size_t i = 0; i < exc->n; i++) {
        const LedgerExcRow *r = exc->v[i];
        bool asm = strcmp(r->table, "ORIGINAL-ASM") == 0, pend = strcmp(r->table, "MATCH-PENDING") == 0;
        if (!asm && !pend) continue;
        for (size_t u = 0; u < r->units.n; u++)
            for (size_t f = 0; f < r->functions.n; f++)
                avp_set_add(asm ? asm_table : pend_table, pair_key(a, r->units.v[u], r->functions.v[f]));
    }
    for (size_t i = 0; i < exc->n; i++) {
        const LedgerExcRow *r = exc->v[i];
        if (strcmp(r->table, "MATCH-PENDING") != 0 || r->cells.n < 3) continue;
        AvpStrVec *rl = AVP_NEW(a, AvpStrVec);
        *rl = ledger_rule_ids(r->cells.v[2]);
        for (size_t u = 0; u < r->units.n; u++)
            for (size_t f = 0; f < r->functions.n; f++) {
                bool c;
                avp_map_put(rules_for, pair_key(a, r->units.v[u], r->functions.v[f]), &c)->p = rl;
            }
    }

    i64 gross_n = 0, gross_b = 0;
    for (size_t i = 0; i < jlen(runits); i++) {
        const AvpJson *fl = jarr(avp_json_at(runits, i), "functions");
        for (size_t k = 0; k < jlen(fl); k++) {
            gross_n++;
            gross_b += ledger_py_int(avp_json_getz(avp_json_at(fl, k), "size"));
        }
    }
    const AvpJson *mv;
    if (meas && (mv = avp_json_getz(meas, "total_functions")) && ledger_py_int(mv) != gross_n) {
        AvpStr t = ledger_py_str(a, mv);
        problem(a, m, "report: measures.total_functions %S != %d functions listed", &t, gross_n);
    }
    if (meas && (mv = avp_json_getz(meas, "total_code")) && ledger_py_int(mv) != gross_b) {
        AvpStr t = ledger_py_str(a, mv);
        problem(a, m, "report: measures.total_code %S != %d bytes listed", &t, gross_b);
    }

    AvpSet *seen_checker = avp_set_new();
    for (size_t i = 0; i < jlen(runits); i++) {
        const AvpJson *ru = avp_json_at(runits, i);
        const AvpJson *md = jmeta(ru);
        if (md && avp_json_truthy(avp_json_getz(md, "auto_generated"))) continue;
        AvpStr rname = jstr(ru, "name");
        AvpStr name = avp_str_startswith(rname, AVP_S("main/")) ? avp_str_slice(rname, 5, (i64)rname.n) : rname;
        LedgerUnit *u = AVP_NEW(a, LedgerUnit);
        u->name = name;
        AvpVal *cv = avp_map_get(cfg, name);
        u->status = cv ? ((const LedgerCfg *)cv->p)->status : (AvpStr){NULL, 0};
        u->path = md ? jstr(md, "source_path") : AVP_S("");
        /* an assembly source unit is assembly throughout: every function in it is ORIGINAL-ASM (no R18 hit; the
         * rule checker does not read `.s` files) */
        bool asm_unit = (cv && ledger_is_asm_source(((const LedgerCfg *)cv->p)->path)) || ledger_is_asm_source(u->path);
        const AvpJson *cats = md ? avp_json_getz(md, "progress_categories") : NULL;
        bool game = false;
        for (size_t k = 0; avp_json_is_arr(cats) && k < avp_json_len(cats); k++)
            if (avp_json_is_str(avp_json_at(cats, k)) && avp_str_eqz(avp_json_at(cats, k)->u.str, "game")) game = true;
        u->section = game ? "game" : "sdk";
        bool complete = md && avp_json_truthy(avp_json_getz(md, "complete"));
        u->linked = u->status.p && avp_str_eqz(u->status, "Matching");
        if (!u->status.p) problem(a, m, "%S: report unit has no Object() entry in configure.py", &name);
        if (u->linked != complete) {
            AvpStr st = u->status.p ? u->status : AVP_S("None");
            problem(a, m, "%S: configure.py says %S but the report says complete=%s", &name, &st,
                    complete ? "True" : "False");
        }
        u->unconfirmed = unconfirmed_in_source(a, root, u->path);
        const AvpJson *rfs = jarr(ru, "functions");
        for (size_t k = 0; k < jlen(rfs); k++) {
            const AvpJson *rf = avp_json_at(rfs, k);
            const AvpJson *fmd = jmeta(rf);
            LedgerFn *f = AVP_NEW(a, LedgerFn);
            f->unit = name;
            f->name = jstr(rf, "name");
            AvpStr dm = fmd ? jstr(fmd, "demangled_name") : AVP_S("");
            if (dm.n) {
                i64 paren = avp_str_findc(dm, '(');
                f->alias = paren >= 0 ? avp_strn(dm.p, (size_t)paren) : dm;
            } else f->alias = f->name;
            const AvpJson *va = fmd ? avp_json_getz(fmd, "virtual_address") : NULL;
            f->addr = va ? ledger_py_int(va) : 0;
            f->size = ledger_py_int(avp_json_getz(rf, "size"));
            const AvpJson *pct = avp_json_getz(rf, "fuzzy_match_percent");
            f->pct = (pct && pct->kind != AVP_JSON_NULL) ? pct : NULL;
            f->section = u->section;
            f->linked = u->linked;
            f->kind = "matched";
            AvpStr names[2] = {pair_key(a, name, f->name), pair_key(a, name, f->alias)};
            f->asm_fn = asm_unit || avp_set_has(asm_fn, names[0]) || avp_set_has(asm_fn, names[1]);
            f->asm_block = avp_set_has(asm_block, names[0]) || avp_set_has(asm_block, names[1]);
            f->in_asm_table = avp_set_has(asm_table, names[0]) || avp_set_has(asm_table, names[1]);
            const AvpJson *cf = NULL;
            for (int n = 0; n < 2; n++) {
                AvpVal *v = avp_map_get(chk_status, names[n]);
                if (v) {
                    if (!cf) cf = (const AvpJson *)v->p;
                    avp_set_add(seen_checker, names[n]);
                }
            }
            if (cf && avp_str_eqz(jstr(cf, "status"), "MATCH-PENDING")) {
                f->pending = true;
                const AvpJson *mr = avp_json_getz(cf, "match_rules");
                for (size_t q = 0; avp_json_is_arr(mr) && q < avp_json_len(mr); q++)
                    avp_vec_push(&f->rules, avp_json_at(mr, q)->u.str);
            }
            if (avp_set_has(pend_table, names[0]) || avp_set_has(pend_table, names[1])) {
                f->pending = true;
                if (!f->rules.n)
                    for (int n = 0; n < 2; n++) {
                        AvpVal *v = avp_map_get(rules_for, names[n]);
                        if (v && ((AvpStrVec *)v->p)->n) f->rules = *(AvpStrVec *)v->p;
                    }
            }
            if (f->linked) {
                if (!f->pct || avp_json_f64(f->pct, 0.0) < 100.0) {
                    AvpStr ps = ledger_py_str(a, f->pct);
                    problem(a, m, "%S::%S: linked but scored %S", &name, &f->name, &ps);
                }
                if (f->in_asm_table || f->asm_fn) f->kind = "ORIGINAL-ASM";
                else if (f->pending) f->kind = "MATCH-PENDING";
                else f->kind = "matched";
            }
            /* dtk's generated pad_NN_XXXXXXXX_<section> over an assembly source unit's label-only bytes has no
             * symbols.txt entry (the TRK vector table; D-ledger-asm-1) */
            if (symbols && !(asm_unit && dtk_pad_name(f->name))) {
                AvpVal *sv = avp_map_get(symbols, f->name);
                if (!sv) problem(a, m, "%S::%S: not in symbols.txt", &name, &f->name);
                else {
                    const LedgerSymVec *sy = (const LedgerSymVec *)sv->p;
                    bool ok = false;
                    for (size_t q = 0; q < sy->n && !ok; q++)
                        ok = sy->v[q].addr == f->addr && (!sy->v[q].has_size || sy->v[q].size == f->size);
                    if (!ok)
                        problem(a, m, "%S::%S: report address/size 0x%08X/%d differs from symbols.txt", &name, &f->name,
                                f->addr, f->size);
                }
            }
            avp_vec_push(&u->fns, f);
        }
        bool created;
        avp_map_put(m->units, name, &created)->p = u;
    }

    /* a checker MATCH-PENDING in a Matching unit that maps to no row would be silently dropped */
    AvpMapIter it = {0};
    while (avp_map_next(chk_status, &it)) {
        const AvpJson *cf = (const AvpJson *)it.val->p;
        AvpStr uname, fname;
        avp_str_partition(it.key, AVP_S("\0"), &uname, &fname);
        AvpVal *uv = avp_map_get(m->units, uname);
        const LedgerUnit *u = uv ? (const LedgerUnit *)uv->p : NULL;
        if (avp_str_eqz(jstr(cf, "status"), "MATCH-PENDING") && u && u->linked && !avp_set_has(seen_checker, it.key))
            problem(a, m, "%S::%S: checker MATCH-PENDING in a Matching unit maps to no ledger row", &uname, &fname);
    }

    LedgerNumbers *n = &m->numbers;
    n->gross_n = gross_n;
    n->gross_b = gross_b;
    it = (AvpMapIter){0};
    while (avp_map_next(m->units, &it)) {
        const LedgerUnit *u = (const LedgerUnit *)it.val->p;
        if (!u->linked) continue;
        for (size_t k = 0; k < u->fns.n; k++) {
            const LedgerFn *f = u->fns.v[k];
            n->linked_n++;
            n->linked_b += f->size;
            if (strcmp(f->kind, "ORIGINAL-ASM") == 0) {
                n->asm_n++;
                n->asm_b += f->size;
            } else if (strcmp(f->kind, "MATCH-PENDING") == 0) {
                n->pend_n++;
                n->pend_b += f->size;
            }
        }
    }
    n->total_n = gross_n - n->asm_n;
    n->total_b = gross_b - n->asm_b;
    n->bytes_n = n->linked_n - n->asm_n;
    n->bytes_b = n->linked_b - n->asm_b;
    n->rules_n = n->bytes_n - n->pend_n;
    n->rules_b = n->bytes_b - n->pend_b;
    n->units_total = (i64)jlen(runits);
    for (size_t i = 0; i < jlen(runits); i++) {
        const AvpJson *md = jmeta(avp_json_at(runits, i));
        if (md && avp_json_truthy(avp_json_getz(md, "complete"))) n->units_complete++;
    }
    if (meas && (mv = avp_json_getz(meas, "complete_functions")) && ledger_py_int(mv) != n->linked_n)
        problem(a, m, "report: complete_functions mismatch");
    if (meas && (mv = avp_json_getz(meas, "complete_code")) && ledger_py_int(mv) != n->linked_b) {
        AvpStr t = ledger_py_str(a, mv);
        problem(a, m, "report: measures.complete_code %S != linked bytes %d", &t, n->linked_b);
    }
    if (meas && (mv = avp_json_getz(meas, "complete_units")) && ledger_py_int(mv) != n->units_complete) {
        AvpStr t = ledger_py_str(a, mv);
        problem(a, m, "report: measures.complete_units %S != %d", &t, n->units_complete);
    }
    if (meas && (mv = avp_json_getz(meas, "total_units")) && ledger_py_int(mv) != n->units_total) {
        AvpStr t = ledger_py_str(a, mv);
        problem(a, m, "report: measures.total_units %S != %d", &t, n->units_total);
    }
    i64 n_matching = 0, n_linked_src = 0;
    for (size_t i = 0; i < configure->n; i++)
        if (avp_str_eqz(configure->v[i].status, "Matching")) n_matching++;
    it = (AvpMapIter){0};
    while (avp_map_next(m->units, &it))
        if (((const LedgerUnit *)it.val->p)->linked) n_linked_src++;
    if (n_matching != n_linked_src)
        problem(a, m, "configure.py lists %d Matching units but %d report source units are linked", n_matching,
                n_linked_src);
    return m;
}
