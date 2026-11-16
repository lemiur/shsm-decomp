/* gate_regress: score regressions (see gate_regress.h for what each function ports). */
#include "gate.h"
#include "shsm_cli.h"
#include "shsm_re.h"
#include "sh_priv.h"
#include "shsm_placeholder.h"

#define A_ a

/* ---- falls ----------------------------------------------------------------------------------------------------- */
void gate_falls_put(GateFalls *fs, GateFall f) {
    if (!fs->index) fs->index = avp_map_new();
    bool created = false;
    AvpVal *slot = avp_map_put(fs->index, avp_str(f.key), &created);
    if (!created) {
        fs->v[slot->i] = f;
        return;
    }
    slot->i = (i64)fs->n;
    avp_vec_push(fs, f);
}

const GateFall *gate_falls_get(const GateFalls *fs, const char *key) {
    if (!fs->index) return NULL;
    AvpVal *v = avp_map_get(fs->index, avp_str(key));
    return v ? &fs->v[v->i] : NULL;
}

const char *gate_tuple_key(AvpArena *a, const char *unit, const char *name) {
    AvpStr u = avp_repr_str(a, avp_str(unit));
    AvpStr n = name ? avp_repr_str(a, avp_str(name)) : AVP_S("None");
    return avp_sprintf(a, "(%.*s, %.*s)", AVP_SF(u), AVP_SF(n));
}

const char *gate_addr_key(AvpArena *a, i64 addr) { return avp_sprintf(a, "%lld", (long long)addr); }

/* ---- compare_reports ------------------------------------------------------------------------------------------- */
static const ShsmUnitReport *unit_exact(const ShsmReport *rep, AvpStr name) {
    AvpVal *v = avp_map_get(rep->index, name);
    return v ? rep->units[v->i] : NULL;
}

static const ShsmFunctionReport *fn_first(const ShsmUnitReport *u, AvpStr name) {
    if (!u) return NULL;
    AvpVal *v = avp_map_get(u->first_by_name, name);
    return v ? &u->functions[v->i] : NULL;
}

static void cmp_unit(GateCmpRows *out, const char *un, const ShsmUnitReport *ou, const ShsmUnitReport *nu,
                     bool only_changed) {
    for (int side = 0; side < 2; side++) {
        const ShsmUnitReport *src = side == 0 ? ou : nu;
        if (!src) continue;
        AvpMapIter it = {0};
        while (avp_map_next(src->first_by_name, &it)) {
            if (side == 1 && fn_first(ou, it.key)) continue; /* already listed from the old side */
            const ShsmFunctionReport *of = fn_first(ou, it.key), *nf = fn_first(nu, it.key);
            GateCmpRow r = {.unit = un, .fn = src->functions[it.val->i].name};
            r.has_old = of != NULL;
            r.old = of ? of->fuzzy : 0.0;
            r.has_new = nf != NULL;
            r.new_ = nf ? nf->fuzzy : 0.0;
            bool same = r.has_old == r.has_new && (!r.has_old || r.old == r.new_);
            if (!only_changed || !same) avp_vec_push(out, r);
        }
    }
}

GateCmpRows gate_compare_reports(const ShsmReport *old, const ShsmReport *new_, bool only_changed) {
    GateCmpRows out = {0};
    for (size_t i = 0; i < old->nunits; i++) {
        const ShsmUnitReport *ou = old->units[i];
        cmp_unit(&out, ou->name, ou, unit_exact(new_, avp_str(ou->name)), only_changed);
    }
    for (size_t i = 0; i < new_->nunits; i++) {
        const ShsmUnitReport *nu = new_->units[i];
        if (unit_exact(old, avp_str(nu->name))) continue;
        cmp_unit(&out, nu->name, NULL, nu, only_changed);
    }
    return out;
}

/* ---- fallen_functions ------------------------------------------------------------------------------------------ */
void gate_fallen_functions(AvpArena *a, const ShsmReport *base, const ShsmReport *fresh, const AvpSet *units,
                           GateFalls *out) {
    AvpIMap *best_new = avp_imap_new();
    AvpMap *fresh_by_name = avp_map_new();
    for (size_t i = 0; i < fresh->nunits; i++) {
        const ShsmUnitReport *u = fresh->units[i];
        AvpMapIter it = {0};
        while (avp_map_next(u->first_by_name, &it)) {
            const ShsmFunctionReport *f = &u->functions[it.val->i];
            if (f->has_address) {
                bool created = false;
                AvpVal *v = avp_imap_put(best_new, f->address, &created);
                double prev = created ? -1.0 : v->f;
                v->f = f->fuzzy > prev ? f->fuzzy : prev;
            }
            bool created = false;
            avp_map_put(fresh_by_name, avp_str(gate_tuple_key(a, u->name, f->name)), &created)->f = f->fuzzy;
        }
    }
    GateCmpRows rows = gate_compare_reports(base, fresh, true);
    for (size_t i = 0; i < rows.n; i++) {
        const GateCmpRow *r = &rows.v[i];
        if (!r->has_old || (units && !avp_set_has(units, avp_str(r->unit)))) continue;
        const ShsmFunctionReport *bf = fn_first(unit_exact(base, avp_str(r->unit)), avp_str(r->fn));
        bool has_a = bf && bf->has_address;
        bool has_got = false;
        double got = 0.0;
        AvpVal *v = has_a ? avp_imap_get(best_new, bf->address)
                          : avp_map_get(fresh_by_name, avp_str(gate_tuple_key(a, r->unit, r->fn)));
        if (v) {
            has_got = true;
            got = v->f;
        }
        if (has_got && got < 0) has_got = false;
        if ((!has_got && r->old > GATE_EPS) || (has_got && got < r->old - GATE_EPS)) {
            GateFall f = {.has_addr = has_a, .addr = has_a ? (i64)bf->address : 0, .unit = r->unit, .name = r->fn,
                          .old = r->old, .has_new = has_got, .new_ = got};
            f.key = has_a ? gate_addr_key(a, f.addr) : gate_tuple_key(a, r->unit, r->fn);
            gate_falls_put(out, f);
        }
    }
    avp_vec_free(&rows);
}

const char *gate_format_fall(AvpArena *a, const GateFall *f) {
    const char *nw = f->has_new ? avp_cz(a, avp_pyfmt(a, "%.2f%%", f->new_)) : "gone";
    return avp_cz(a, avp_pyfmt(a, "    %-44s %6.2f%% -> %-7s %s [%s]", f->name, f->old, nw, f->unit,
                               f->label ? f->label : ""));
}

/* ---- merge_lane.py --------------------------------------------------------------------------------------------- */
bool gate_addr_int(const AvpJson *addr, i64 *out) {
    if (!addr || addr->kind == AVP_JSON_NULL) return false;
    switch (addr->kind) {
    case AVP_JSON_BOOL: *out = addr->b ? 1 : 0; return true;
    case AVP_JSON_INT:
        if (addr->big) return false;
        *out = addr->u.i;
        return true;
    case AVP_JSON_FLOAT:
        if (addr->u.f != addr->u.f || addr->u.f > 9.2e18 || addr->u.f < -9.2e18) return false;
        *out = (i64)addr->u.f;
        return true;
    case AVP_JSON_STRING: {
        bool overflow = false;
        return avp_parse_int(addr->u.str, 10, out, &overflow) && !overflow;
    }
    default: return false;
    }
}

static const char *str_or_null(AvpArena *a, const AvpJson *v) {
    if (!v || v->kind == AVP_JSON_NULL) return NULL;
    if (v->kind == AVP_JSON_STRING) return avp_cz(a, v->u.str);
    return gate_json_pystr(a, v);
}

/* float(x) of a JSON value; TypeError for null, as float(None). */
static double py_float(const AvpJson *v) {
    if (!v || v->kind == AVP_JSON_NULL)
        shsm_raise(SHSM_EXC_TYPE, "float() argument must be a string or a real number, not 'NoneType'");
    if (v->kind == AVP_JSON_STRING) {
        double d = 0;
        if (!avp_parse_float(v->u.str, &d))
            shsm_raise(SHSM_EXC_VALUE, "could not convert string to float: %.*s",
                       AVP_SF(avp_repr_str(avp_arena(), v->u.str)));
        return d;
    }
    if (v->kind == AVP_JSON_BOOL) return v->b ? 1.0 : 0.0;
    return avp_json_f64(v, 0.0);
}

/* The func_scores key (unit, addr if addr is not None else name) as a string. */
static AvpStr score_key(AvpArena *a, const char *unit, const AvpJson *addr, const char *name) {
    AvpStr k;
    if (addr && addr->kind != AVP_JSON_NULL)
        k = avp_json_dumps(a, addr, NULL);
    else
        k = avp_str(name ? avp_sprintf(a, "N%s", name) : "\x02None");
    return avp_str(avp_sprintf(a, "%s\x01%.*s", unit, AVP_SF(k)));
}

GateScores gate_func_scores(AvpArena *a, const AvpJson *cj) {
    GateScores res = {0};
    AvpMap *index = avp_map_new();
    const AvpJson *units = avp_json_getz(cj, "units");
    size_t nu = avp_json_is_arr(units) ? avp_json_len(units) : 0;
    for (size_t i = 0; i < nu; i++) {
        const AvpJson *u = avp_json_at(units, i);
        const char *unit = avp_cz(a, avp_json_get_str(u, "name", AVP_S("")));
        const AvpJson *fs = avp_json_getz(u, "functions");
        size_t nf = avp_json_is_arr(fs) ? avp_json_len(fs) : 0;
        for (size_t j = 0; j < nf; j++) {
            const AvpJson *f = avp_json_at(fs, j);
            const AvpJson *md = avp_json_getz(f, "metadata");
            const AvpJson *addr = avp_json_is_obj(md) ? avp_json_getz(md, "virtual_address") : NULL;
            if (addr && addr->kind == AVP_JSON_NULL) addr = NULL;
            const char *fname = str_or_null(a, avp_json_getz(f, "name"));
            bool created = false;
            AvpVal *slot = avp_map_put(index, score_key(a, unit, addr, fname), &created);
            if (created) {
                slot->i = (i64)res.n;
                GateScore e = {.unit = unit, .addr = addr, .key_name = fname};
                avp_vec_push(&res, e);
            }
            GateScore *e = &res.v[slot->i];
            if (avp_json_has(f, AVP_S("from"))) {
                e->from = py_float(avp_json_get_or(avp_json_getz(f, "from"), AVP_S("fuzzy_match_percent"),
                                                   avp_json_float(a, 0.0)));
                e->has_from = true;
                e->onto_name = fname;
            }
            if (avp_json_has(f, AVP_S("to"))) {
                e->to = py_float(avp_json_get_or(avp_json_getz(f, "to"), AVP_S("fuzzy_match_percent"),
                                                 avp_json_float(a, 0.0)));
                e->has_to = true;
                e->name = fname;
            }
        }
    }
    avp_map_free(index);
    return res;
}

typedef struct {
    const char *k0, *k1;
    size_t i;
} SortItem;

static int cmp_items(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const SortItem *p = x, *q = y;
    int c = strcmp(p->k0, q->k0);
    return c ? c : strcmp(p->k1, q->k1);
}

GateScores gate_merge_by_symbol(AvpArena *a, const GateScores *scores) {
    SortItem *items = AVP_NEWN(a, SortItem, scores->n + 1);
    for (size_t i = 0; i < scores->n; i++) {
        const GateScore *e = &scores->v[i];
        const char *k1;
        if (e->addr) k1 = e->addr->kind == AVP_JSON_STRING ? avp_cz(a, e->addr->u.str) : gate_json_pystr(a, e->addr);
        else k1 = e->key_name ? e->key_name : "None";
        items[i] = (SortItem){e->unit, k1, i};
    }
    avp_sort(items, scores->n, sizeof(SortItem), cmp_items, NULL, false);
    GateScores merged = {0};
    AvpMap *index = avp_map_new();
    for (size_t i = 0; i < scores->n; i++) {
        const GateScore *e = &scores->v[items[i].i];
        i64 ai = 0;
        bool has_a = gate_addr_int(e->addr, &ai);
        AvpStr key = has_a ? avp_str(avp_sprintf(a, "addr\x01%lld", (long long)ai))
                           : avp_str(avp_sprintf(a, "name\x01%s\x01%s", e->unit,
                                                 e->name ? e->name : (e->onto_name ? e->onto_name : "\x02None")));
        bool created = false;
        AvpVal *slot = avp_map_put(index, key, &created);
        if (created) {
            slot->i = (i64)merged.n;
            GateScore m = {.unit = e->unit, .addr = e->addr, .has_addr_int = has_a, .addr_int = ai};
            avp_vec_push(&merged, m);
        }
        GateScore *m = &merged.v[slot->i];
        if (e->has_from && (!m->has_from || e->from > m->from)) {
            m->from = e->from;
            m->has_from = true;
            m->onto_name = e->onto_name;
            m->unit = e->unit;
        }
        if (e->has_to && (!m->has_to || e->to > m->to)) {
            m->to = e->to;
            m->has_to = true;
            m->name = e->name;
            m->unit_lane = e->unit;
        }
    }
    for (size_t i = 0; i < merged.n; i++)
        if (!merged.v[i].has_from && merged.v[i].unit_lane) merged.v[i].unit = merged.v[i].unit_lane;
    avp_map_free(index);
    return merged;
}

GateUnitIndex gate_all_unit_index(AvpArena *a, const AvpJson *report) {
    GateUnitIndex ix = {avp_imap_new(), avp_map_new()};
    const AvpJson *units = avp_json_getz(report, "units");
    size_t nu = avp_json_is_arr(units) ? avp_json_len(units) : 0;
    for (size_t i = 0; i < nu; i++) {
        const AvpJson *u = avp_json_at(units, i);
        const char *unit = avp_cz(a, avp_json_get_str(u, "name", AVP_S("")));
        const AvpJson *fs = avp_json_getz(u, "functions");
        size_t nf = avp_json_is_arr(fs) ? avp_json_len(fs) : 0;
        for (size_t j = 0; j < nf; j++) {
            const AvpJson *f = avp_json_at(fs, j);
            const AvpJson *sv = avp_json_getz(f, "fuzzy_match_percent");
            double score = avp_json_truthy(sv) ? py_float(sv) : 0.0;
            GateHit *hit = AVP_NEW(a, GateHit);
            *hit = (GateHit){unit, str_or_null(a, avp_json_getz(f, "name")), score};
            const AvpJson *md = avp_json_getz(f, "metadata");
            i64 ad = 0;
            if (avp_json_is_obj(md) && gate_addr_int(avp_json_getz(md, "virtual_address"), &ad)) {
                bool created = false;
                AvpVal *v = avp_imap_put(ix.by_addr, (u64)ad, &created);
                if (created || score > ((GateHit *)v->p)->score) v->p = hit;
            }
            bool created = false;
            AvpVal *v = avp_map_put(ix.by_name, avp_str(gate_tuple_key(a, unit, hit->name)), &created);
            if (created || score > ((GateHit *)v->p)->score) v->p = hit;
        }
    }
    return ix;
}

const GateHit *gate_unit_index_addr(const GateUnitIndex *ix, i64 addr) {
    AvpVal *v = avp_imap_get(ix->by_addr, (u64)addr);
    return v ? v->p : NULL;
}

const GateHit *gate_unit_index_name(AvpArena *a, const GateUnitIndex *ix, const char *unit, const char *name) {
    AvpVal *v = avp_map_get(ix->by_name, avp_str(gate_tuple_key(a, unit, name)));
    return v ? v->p : NULL;
}

GateChangeRows gate_parse_changes_text(AvpArena *a, AvpStr text) {
    GateChangeRows rows = {0};
    ShsmRe *re = shsm_re("^\\s*(.+?) \\| (\\w+)\\s* \\| \\s*(-?[0-9.]+)% -> \\s*(-?[0-9.]+)%\\s*$", 0);
    AvpStrVec lines = avp_str_splitlines(text, false);
    for (size_t i = 0; i < lines.n; i++) {
        ShsmReMatch m;
        if (!shsm_re_match(re, lines.v[i], 0, AVP_END, &m)) continue;
        double f = 0, t = 0;
        if (!avp_parse_float(shsm_re_group(&m, 3), &f) || !avp_parse_float(shsm_re_group(&m, 4), &t))
            shsm_raise(SHSM_EXC_VALUE, "could not convert string to float");
        GateChangeRow r = {avp_cz(a, avp_str_strip(shsm_re_group(&m, 1))), avp_cz(a, shsm_re_group(&m, 2)), f, t};
        avp_vec_push(&rows, r);
    }
    avp_vec_free(&lines);
    return rows;
}

bool gate_is_unit_row(const GateChangeRow *row, const AvpSet *unit_names) {
    static const char *const keys[] = {"matched_code", "matched_data", "complete_code", "complete_data"};
    for (size_t i = 0; i < 4; i++)
        if (strcmp(row->key, keys[i]) == 0) return true;
    return strcmp(row->name, "Total") == 0 || (unit_names && avp_set_has(unit_names, avp_str(row->name)));
}

i64 gate_json_function_regressions(const AvpJson *cj) {
    i64 n = 0;
    const AvpJson *units = avp_json_getz(cj, "units");
    size_t nu = avp_json_is_arr(units) ? avp_json_len(units) : 0;
    for (size_t i = 0; i < nu; i++) {
        const AvpJson *fs = avp_json_getz(avp_json_at(units, i), "functions");
        size_t nf = avp_json_is_arr(fs) ? avp_json_len(fs) : 0;
        for (size_t j = 0; j < nf; j++) {
            const AvpJson *f = avp_json_at(fs, j);
            const AvpJson *fr = avp_json_getz(f, "from"), *to = avp_json_getz(f, "to");
            double x = avp_json_truthy(fr) ? avp_json_get_f64(fr, "fuzzy_match_percent", 0.0) : 0.0;
            double y = avp_json_truthy(to) ? avp_json_get_f64(to, "fuzzy_match_percent", 0.0) : 0.0;
            if (x > y) n++;
        }
    }
    return n;
}

GateSrcUnits gate_source_units(AvpArena *a, const AvpJson *report) {
    GateSrcUnits out = {0};
    ShsmRe *autore = shsm_re("^main/auto_\\d+_", 0);
    const AvpJson *units = avp_json_getz(report, "units");
    size_t nu = avp_json_is_arr(units) ? avp_json_len(units) : 0;
    for (size_t i = 0; i < nu; i++) {
        const AvpJson *u = avp_json_at(units, i);
        const AvpJson *md = avp_json_getz(u, "metadata");
        AvpStr name = avp_json_get_str(u, "name", AVP_S(""));
        ShsmReMatch m;
        if ((avp_json_is_obj(md) && avp_json_truthy(avp_json_getz(md, "auto_generated"))) ||
            shsm_re_match(autore, name, 0, AVP_END, &m))
            continue;
        const AvpJson *fs = avp_json_getz(u, "functions");
        size_t nf = avp_json_is_arr(fs) ? avp_json_len(fs) : 0;
        GateSrcUnit su = {avp_cz(a, name), "", AVP_NEWN(a, GateSrcFunc, nf + 1), 0};
        for (size_t j = 0; j < nf; j++) {
            const AvpJson *f = avp_json_at(fs, j);
            const AvpJson *fm = avp_json_getz(f, "metadata");
            i64 addr = 0;
            if (!avp_json_is_obj(fm) || !gate_addr_int(avp_json_getz(fm, "virtual_address"), &addr)) continue;
            const AvpJson *sv = avp_json_getz(f, "fuzzy_match_percent");
            const AvpJson *sz = avp_json_getz(f, "size");
            i64 size = 0;
            if (avp_json_truthy(sz) && !gate_addr_int(sz, &size)) size = 0;
            su.funcs[su.nfuncs++] = (GateSrcFunc){str_or_null(a, avp_json_getz(f, "name")), addr,
                                                  sv && sv->kind != AVP_JSON_NULL ? py_float(sv) : 0.0, size};
        }
        if (avp_json_is_obj(md)) {
            const AvpJson *sp = avp_json_getz(md, "source_path");
            if (avp_json_truthy(sp)) su.source_path = str_or_null(a, sp);
        }
        avp_vec_push(&out, su);
    }
    return out;
}

GateReportIndex gate_report_index(AvpArena *a, const AvpJson *report) {
    GateReportIndex ix = {avp_map_new(), avp_map_new()};
    GateSrcUnits su = gate_source_units(a, report);
    for (size_t i = 0; i < su.n; i++) {
        for (size_t j = 0; j < su.v[i].nfuncs; j++) {
            const GateSrcFunc *f = &su.v[i].funcs[j];
            GateReportEntry *e = AVP_NEW(a, GateReportEntry);
            *e = (GateReportEntry){su.v[i].unit, su.v[i].source_path, f->symbol, f->score, f->addr};
            bool created = false;
            avp_map_put(ix.by_addr, avp_str(gate_tuple_key(a, su.v[i].unit, gate_addr_key(a, f->addr))), &created)
                ->p = e;
            avp_map_put(ix.by_name, avp_str(gate_tuple_key(a, su.v[i].unit, f->symbol)), &created)->p = e;
        }
    }
    avp_vec_free(&su);
    return ix;
}

/* ---- score_history.py ------------------------------------------------------------------------------------------ */
static const char *const OPS[][2] = {
    {"__as", "operator="},   {"__eq", "operator=="},      {"__ne", "operator!="},      {"__vc", "operator[]"},
    {"__nw", "operator new"}, {"__dl", "operator delete"}, {"__nwa", "operator new[]"}, {"__dla", "operator delete[]"},
    {"__pl", "operator+"},   {"__mi", "operator-"},       {"__ml", "operator*"},       {"__dv", "operator/"},
    {"__apl", "operator+="}, {"__ami", "operator-="},     {"__aml", "operator*="},     {"__adv", "operator/="},
    {"__lt", "operator<"},   {"__gt", "operator>"},       {"__le", "operator<="},      {"__ge", "operator>="},
    {"__cl", "operator()"},  {"__rf", "operator->"},      {"__nt", "operator!"},       {"__ls", "operator<<"},
    {"__rs", "operator>>"},  {"__pp", "operator++"},      {"__mm", "operator--"},      {"__md", "operator%"},
    {"__ad", "operator&"},   {"__or", "operator|"},       {"__er", "operator^"},       {"__aa", "operator&&"},
    {"__oo", "operator||"},
};

static bool is_ascii_digit(char c) { return c >= '0' && c <= '9'; }

/* _parse_len_name(s, pos): the name and the position after it. `\d+` is Unicode in Python; mangled names are
 * ASCII. */
static bool parse_len_name(AvpStr s, size_t pos, AvpStr *name, size_t *end) {
    size_t i = pos;
    while (i < s.n && is_ascii_digit(s.p[i])) i++;
    if (i == pos) return false;
    i64 n = 0;
    bool overflow = false;
    if (!avp_parse_int(avp_strn(s.p + pos, i - pos), 10, &n, &overflow) || overflow) return false;
    if (n <= 0 || i + (size_t)n > s.n) return false;
    *name = avp_strn(s.p + i, (size_t)n);
    *end = i + (size_t)n;
    return true;
}

static bool parse_qualifier(AvpStr rest, AvpStrVec *quals) {
    size_t pos = 0;
    if (rest.n && rest.p[0] == 'F') return true;
    if (rest.n > 2 && rest.p[0] == 'Q' && is_ascii_digit(rest.p[1])) {
        int n = rest.p[1] - '0';
        pos = 2;
        for (int k = 0; k < n; k++) {
            AvpStr nm;
            if (!parse_len_name(rest, pos, &nm, &pos)) return false;
            avp_vec_push(quals, nm);
        }
    } else {
        AvpStr nm;
        if (!parse_len_name(rest, 0, &nm, &pos)) return false;
        avp_vec_push(quals, nm);
    }
    if (pos < rest.n && rest.p[pos] == 'C') pos++;
    return pos < rest.n && rest.p[pos] == 'F';
}

AvpStrVec gate_sh_demangle(AvpArena *a, const char *sym_cz, bool *sinit) {
    AvpStrVec cands = {0};
    *sinit = false;
    AvpStr sym = avp_str(sym_cz);
    ShsmReMatch m;
    if (shsm_re_match(shsm_re("^@\\d+@(.+)$", 0), sym, 0, AVP_END, &m)) sym = shsm_re_group(&m, 1);
    if (avp_str_startswith(sym, AVP_S("__sinit_"))) {
        *sinit = true;
        return cands;
    }
    i64 start = 1;
    for (;;) {
        i64 i = avp_str_find(sym, AVP_S("__"), start, AVP_END);
        if (i < 0) break;
        start = i + 1;
        AvpStr base = avp_strn(sym.p, (size_t)i);
        AvpStr rest = avp_str_slice(sym, i + 2, AVP_END);
        AvpStrVec quals = {0};
        if (!parse_qualifier(rest, &quals)) {
            avp_vec_free(&quals);
            continue;
        }
        AvpStr name = base;
        if (avp_str_eqz(base, "__ct") && quals.n) {
            name = quals.v[quals.n - 1];
        } else if (avp_str_eqz(base, "__dt") && quals.n) {
            name = avp_str_concat(a, AVP_S("~"), quals.v[quals.n - 1]);
        } else {
            for (size_t k = 0; k < sizeof OPS / sizeof OPS[0]; k++)
                if (avp_str_eqz(base, OPS[k][0])) name = avp_str(OPS[k][1]);
        }
        AvpBuf b;
        avp_buf_init(&b, a);
        for (size_t k = 0; k < quals.n; k++) {
            avp_buf_put(&b, quals.v[k]);
            avp_buf_putz(&b, "::");
        }
        avp_buf_put(&b, name);
        avp_vec_push(&cands, avp_buf_str(&b));
        if (quals.n > 1)
            avp_vec_push(&cands, avp_str(avp_sprintf(a, "%.*s::%.*s", AVP_SF(quals.v[quals.n - 1]), AVP_SF(name))));
        avp_vec_free(&quals);
        break;
    }
    avp_vec_push(&cands, avp_str_dup(a, sym));
    return cands;
}

const char *gate_guess_path(AvpArena *a, const char *unit, GateExistsFn exists, void *ctx) {
    static const char *const exts[] = {".cpp", ".c", ".cp", ".cc", ".s"}; /* .s: assembly source units */
    const char *base = strncmp(unit, "main/", 5) == 0 ? unit + 5 : unit;
    for (size_t i = 0; i < sizeof exts / sizeof exts[0]; i++) {
        const char *p = avp_sprintf(a, "src/%s%s", base, exts[i]);
        if (exists(ctx, p)) return p;
    }
    return "";
}

GateFileIndex *gate_file_index_new(AvpArena *a, const AvpJson *data) {
    GateFileIndex *fi = AVP_NEW(a, GateFileIndex);
    fi->data = data;
    fi->by_addr = avp_imap_new();
    fi->by_name = avp_map_new();
    fi->unit_hash = str_or_null(a, avp_json_getz(data, "unit_hash"));
    fi->rest_hash = str_or_null(a, avp_json_getz(data, "rest_hash"));
    const AvpJson *funcs = avp_json_getz(data, "funcs");
    size_t n = avp_json_is_arr(funcs) ? avp_json_len(funcs) : 0;
    for (size_t i = 0; i < n; i++) {
        AvpJson *f = avp_json_at(funcs, i);
        i64 ad = 0;
        const AvpJson *av = avp_json_getz(f, "addr");
        if (av && av->kind == AVP_JSON_INT && !av->big) {
            ad = av->u.i;
            bool created = false;
            AvpVal *v = avp_imap_put(fi->by_addr, (u64)ad, &created);
            if (created) v->p = f;
        }
        bool created = false;
        AvpVal *v = avp_map_put(fi->by_name, avp_json_get_str(f, "name", AVP_S("")), &created);
        if (created) v->p = AVP_NEW(a, AvpPtrVec);
        avp_vec_push((AvpPtrVec *)v->p, f);
    }
    return fi;
}

const AvpJson *gate_file_index_lookup(AvpArena *a, const GateFileIndex *fi, const char *symbol, bool has_addr,
                                      i64 addr, const char **basis) {
    if (has_addr) {
        AvpVal *v = avp_imap_get(fi->by_addr, (u64)addr);
        if (v) {
            *basis = "addr";
            return v->p;
        }
    }
    bool sinit = false;
    AvpStrVec dm = gate_sh_demangle(a, symbol, &sinit);
    if (sinit) {
        *basis = "sinit";
        return NULL;
    }
    for (size_t i = 0; i < dm.n; i++) {
        AvpVal *v = avp_map_get(fi->by_name, dm.v[i]);
        AvpPtrVec *lst = v ? v->p : NULL;
        if (lst && lst->n == 1) {
            avp_vec_free(&dm);
            *basis = "name";
            return lst->v[0];
        }
    }
    avp_vec_free(&dm);
    *basis = "unit";
    return NULL;
}

/* ---- index_source ---------------------------------------------------------------------------------------------- */
AvpJson *gate_index_linked(GateIndexer *ix, AvpArena *a, const char *path, AvpStr text) {
    (void)ix;
    (void)path;
    ShIndex *si = sh_index_source(a, text);
    AvpJsonError err;
    AvpJson *doc = avp_json_parse(a, sh_index_to_json(a, si), &err);
    if (!doc) avp_throw(AVP_ERR_FORMAT, "index_source: %s", avp_cz(a, avp_json_error_str(a, &err)));
    return doc;
}

GateFileIndex *gate_source_side_index(AvpArena *a, GateSourceSide *s, const char *path) {
    if (!s->cache) s->cache = avp_map_new();
    AvpVal *v = avp_map_get(s->cache, avp_str(path));
    if (v) return v->p;
    AvpStr text;
    GateFileIndex *fi = NULL;
    if (s->read(s->read_ctx, a, path, &text)) fi = gate_file_index_new(a, s->ix->index(s->ix, a, path, text));
    bool created = false;
    avp_map_put(s->cache, avp_str(path), &created)->p = fi;
    return fi;
}

/* ---- classify_decrease ----------------------------------------------------------------------------------------- */
static bool streq(const char *x, const char *y) { return (x && y) ? strcmp(x, y) == 0 : x == y; }

bool gate_classify_decrease(AvpArena *a, const GateDecrease *e, GateSourceSide *lane, GateSourceSide *onto,
                            GateExistsFn exists_lane, void *exists_ctx, bool headers_changed, const char **label) {
    const char *lp = (e->path_lane && *e->path_lane) ? e->path_lane : gate_guess_path(a, e->unit, exists_lane, exists_ctx);
    const char *op = (e->path_onto && *e->path_onto) ? e->path_onto : lp;
    if (!*lp) {
        *label = "source file not found in the lane";
        return true;
    }
    GateFileIndex *li = gate_source_side_index(a, lane, lp);
    GateFileIndex *oi = gate_source_side_index(a, onto, op);
    if (!li) {
        *label = avp_sprintf(a, "source file %s missing in the lane", lp);
        return true;
    }
    if (!oi) {
        *label = avp_sprintf(a, "source file %s is new in the lane", lp);
        return true;
    }
    const char *lb = NULL, *ob = NULL;
    const AvpJson *lf = gate_file_index_lookup(a, li, e->name ? e->name : "", e->has_addr, e->addr, &lb);
    const char *oname = (e->onto_name && *e->onto_name) ? e->onto_name : ((e->name && *e->name) ? e->name : "");
    const AvpJson *of = gate_file_index_lookup(a, oi, oname, e->has_addr, e->addr, &ob);
    const char *lab;
    if (lf && of) {
        const char *lh = str_or_null(a, avp_json_getz(lf, "hash")), *oh = str_or_null(a, avp_json_getz(of, "hash"));
        if (!streq(lh, oh)) {
            *label = avp_sprintf(a, "function source changed (%s)", lb);
            return true;
        }
        lab = avp_sprintf(a, "function source identical (%s)", lb);
    } else {
        bool same;
        if (strcmp(lb, "sinit") == 0 || strcmp(ob, "sinit") == 0)
            same = streq(li->rest_hash, oi->rest_hash);
        else
            same = streq(li->unit_hash, oi->unit_hash);
        if (!same) {
            *label = "function not located in source, file changed";
            return true;
        }
        lab = "function not located, file unchanged";
    }
    if (headers_changed)
        lab = avp_sprintf(a, "%s; include/ or configure.py changed", lab);
    else if (!streq(li->unit_hash, oi->unit_hash))
        lab = avp_sprintf(a, "%s; rest of the file changed", lab);
    *label = lab;
    return false;
}

/* ---- classify_falls -------------------------------------------------------------------------------------------- */
typedef struct {
    const char *tree;
    GateGitFn git;
    void *git_ctx;
    const char *rev;
} ClassifyCtx;

static bool read_tree(void *ctx, AvpArena *a, const char *p, AvpStr *text) {
    ClassifyCtx *c = ctx;
    return avp_read_text(a, avp_path_join(a, c->tree, p), AVP_TM_UTF8_REPLACE, text);
}

static bool read_rev(void *ctx, AvpArena *a, const char *p, AvpStr *text) {
    ClassifyCtx *c = ctx;
    const char *spec = avp_sprintf(a, "%s:%s", c->rev, p);
    GateRun r = c->git(c->git_ctx, a, (const char *const[]){"show", spec}, 2);
    if (!gate_run_ok(&r)) return false;
    *text = r.out;
    return true;
}

static bool exists_tree(void *ctx, const char *p) {
    ClassifyCtx *c = ctx;
    return avp_exists(avp_path_join(avp_arena(), c->tree, p));
}

typedef struct {
    const GateFall *f;
} FallRef;

static int cmp_fall_key(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return strcmp(((const FallRef *)x)->f->key, ((const FallRef *)y)->f->key);
}

void gate_classify_falls(AvpArena *a, const char *tree, GateGitFn git, void *git_ctx, const char *rev,
                         const ShsmReport *report, const GateFalls *fallen, GateIndexer *ix, GateFalls *edited,
                         GateFalls *unchanged, GateFalls *placeholder) {
    if (!rev || !*rev) {
        for (size_t i = 0; i < fallen->n; i++) {
            GateFall f = fallen->v[i];
            f.label = "baseline commit unknown";
            gate_falls_put(edited, f);
        }
        return;
    }
    ClassifyCtx c = {tree, git, git_ctx, rev};
    GateRun d = git(git_ctx, a, (const char *const[]){"diff", "--name-only", rev}, 3);
    bool headers = false;
    AvpStrVec dl = avp_str_splitlines(d.out, false);
    for (size_t i = 0; i < dl.n; i++)
        if (avp_str_startswith(dl.v[i], AVP_S("include/")) || avp_str_eqz(dl.v[i], "configure.py")) headers = true;
    avp_vec_free(&dl);
    GateSourceSide lane = {read_tree, &c, ix, NULL}, onto = {read_rev, &c, ix, NULL};
    /* fresh_by_addr.setdefault(f.address, (u.name, f.name)): the None address is a key too */
    AvpIMap *by_addr = avp_imap_new();
    const ShsmFunctionReport *none_first = NULL;
    for (size_t i = 0; report && i < report->nunits; i++) {
        const ShsmUnitReport *u = report->units[i];
        AvpMapIter it = {0};
        while (avp_map_next(u->first_by_name, &it)) {
            const ShsmFunctionReport *f = &u->functions[it.val->i];
            if (!f->has_address) {
                if (!none_first) none_first = f;
                continue;
            }
            bool created = false;
            AvpVal *v = avp_imap_put(by_addr, f->address, &created);
            if (created) v->p = (void *)f;
        }
    }
    FallRef *refs = AVP_NEWN(a, FallRef, fallen->n + 1);
    for (size_t i = 0; i < fallen->n; i++) refs[i].f = &fallen->v[i];
    avp_sort(refs, fallen->n, sizeof(FallRef), cmp_fall_key, NULL, false);
    for (size_t i = 0; i < fallen->n; i++) {
        GateFall f = *refs[i].f;
        const ShsmFunctionReport *hit = NULL;
        if (f.has_addr) {
            AvpVal *v = avp_imap_get(by_addr, (u64)f.addr);
            hit = v ? v->p : NULL;
        } else {
            hit = none_first;
        }
        GateDecrease e = {.unit = hit ? hit->unit : f.unit,
                          .name = f.has_new ? (hit ? hit->name : f.name) : NULL,
                          .onto_name = f.name,
                          .has_addr = f.has_addr,
                          .addr = f.addr,
                          .path_lane = "",
                          .path_onto = ""};
        bool is_edit = true;
        const char *label = NULL;
        AvpTry t;
        if (AVP_TRY(&t)) {
            if (placeholder &&
                gate_placeholder_removed(a, &e, &lane, &onto, exists_tree, &c)) {
                avp_try_end(&t);
                f.label = "placeholder removed";
                gate_falls_put(placeholder, f);
                continue;
            }
            is_edit = gate_classify_decrease(a, &e, &lane, &onto, exists_tree, &c, headers, &label);
            avp_try_end(&t);
        } else {
            is_edit = true;
            label = avp_sprintf(a, "not classified: %.*s", AVP_SF(shsm_exc_str(a, t.code, t.msg)));
        }
        if (!f.has_new) {
            is_edit = true;
            label = "function missing from the fresh report";
        }
        f.label = label;
        gate_falls_put(is_edit ? edited : unchanged, f);
    }
}

/* ---- a function that left its unit by a resplit ---------------------------------------------------------------- */
GateResplit *gate_resplit_new(AvpArena *a, const AvpStr *lane_splits, const AvpStr *base_splits,
                              const AvpStr *lane_symbols) {
    if (!lane_splits || !base_splits || !lane_symbols) return NULL;
    GateResplit *rs = AVP_NEW(a, GateResplit);
    rs->lane = shsm_parse_splits(a, *lane_splits);
    rs->base = shsm_parse_splits(a, *base_splits);
    rs->syms = shsm_parse_symbols(a, *lane_symbols, NULL);
    return rs;
}

static const ShsmSplitRange *range_holding(const ShsmSplitUnit *u, u64 addr) {
    for (size_t i = 0; u && i < u->nranges; i++)
        if (addr >= u->ranges[i].start && addr < u->ranges[i].end) return &u->ranges[i];
    return NULL;
}

static const char *range_text(AvpArena *a, const ShsmSplitRange *r) {
    return avp_sprintf(a, "%s 0x%08llX-0x%08llX", r->section, (unsigned long long)r->start, (unsigned long long)r->end);
}

bool gate_resplit_left(AvpArena *a, const GateResplit *rs, const char *unit, const char *name, bool has_addr, i64 addr,
                       double from, GateLeftUnit *out) {
    if (!rs || from >= 99.995) return false;
    u64 at;
    const ShsmSymbol *sym = name ? shsm_sym_by_name(rs->syms, avp_str(name)) : NULL;
    if (sym) at = sym->address;
    else if (has_addr) at = (u64)addr;
    else return false;
    const ShsmSplitUnit *bu = shsm_splits_unit(rs->base, unit);
    const ShsmSplitRange *was = range_holding(bu, at);
    if (!was) return false;
    const ShsmSplitUnit *lu = shsm_splits_unit(rs->lane, unit);
    if (range_holding(lu, at)) return false;
    const ShsmSplitRange *now = NULL;
    for (size_t i = 0; lu && i < lu->nranges; i++)
        if (!strcmp(lu->ranges[i].section, was->section)) now = &lu->ranges[i];
    out->old_range = range_text(a, was);
    out->new_range = now ? avp_sprintf(a, "0x%08llX-0x%08llX", (unsigned long long)now->start, (unsigned long long)now->end)
                         : (lu ? "no range" : "unit removed");
    return true;
}

const char *gate_left_unit_line(AvpArena *a, const char *name, double from, bool has_to, double to, const char *unit,
                                const GateLeftUnit *l) {
    const char *t = has_to ? avp_cz(a, avp_pyfmt(a, "%.2f%%", to)) : "gone";
    return avp_cz(a, avp_pyfmt(a, "    %-44s %6.2f%% -> %-7s %s  [%s -> %s]", name ? name : "?", from, t, unit,
                               l->old_range, l->new_range));
}

/* ---- a placeholder that the lane removed ----------------------------------------------------------------------- */
bool gate_placeholder_removed(AvpArena *a, const GateDecrease *e, GateSourceSide *lane, GateSourceSide *onto,
                              GateExistsFn exists_lane, void *exists_ctx) {
    const char *lp = (e->path_lane && *e->path_lane)  ? e->path_lane
                     : (e->path_onto && *e->path_onto) ? e->path_onto
                                                       : gate_guess_path(a, e->unit, exists_lane, exists_ctx);
    const char *op = (e->path_onto && *e->path_onto) ? e->path_onto : lp;
    if (!*lp) return false;
    GateFileIndex *li = gate_source_side_index(a, lane, lp);
    GateFileIndex *oi = gate_source_side_index(a, onto, op);
    if (!li || !oi) return false;
    const char *lb = NULL, *ob = NULL;
    if (gate_file_index_lookup(a, li, e->name ? e->name : "", e->has_addr, e->addr, &lb)) return false;
    const char *oname = (e->onto_name && *e->onto_name) ? e->onto_name : ((e->name && *e->name) ? e->name : "");
    const AvpJson *of = gate_file_index_lookup(a, oi, oname, e->has_addr, e->addr, &ob);
    if (!of || strcmp(ob, "addr") != 0 && strcmp(ob, "name") != 0) return false;
    AvpStr text;
    if (!onto->read(onto->read_ctx, a, op, &text)) return false;
    i64 first = avp_json_get_i64(of, "line", 0), last = avp_json_get_i64(of, "end", 0);
    if (first < 1 || last < first) return false;
    AvpStrVec lines = avp_str_splitlines(text, false);
    if ((size_t)last > lines.n) return false;
    AvpBuf b;
    avp_buf_init(&b, a);
    for (i64 i = first; i <= last; i++) {
        avp_buf_put(&b, lines.v[i - 1]);
        avp_buf_putc(&b, '\n');
    }
    return shsm_is_placeholder_body(avp_buf_str(&b));
}
