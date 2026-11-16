/* brief.py: the unit briefing (--unit): header, functions, classes, includes, notes, start. */
#include "brief.h"
#include "shsm_re.h"
#include "twins.h"
#include "wall_walls.h"

#include <string.h>

static AvpArena *A(void) { return avp_arena(); }
#define J(o, k) avp_json_getz((o), (k))

void brief_ref_lines(BriefLines *L, const AvpJson *d, bool with_none);
void brief_docs_lines(BriefLines *L, const AvpJson *docs, i64 n, i64 clip_text, const char *more);
AvpJson *brief_docs_notes(BriefWorld *W, AvpStr term, bool expand);
AvpJson *brief_profile_of(BriefWorld *W, i64 address, const char *lib);
bool brief_is_helper(AvpStr n);

typedef struct {
    AvpJson *head;
    const ShsmUnit *u;
    ShsmSymRows fns;
    AvpJson *fdata;
} UnitCtx;

static bool has_source(const ShsmUnit *u) { return u && u->source; }

/* int(text, 16) of a classes.json address ("0x8000ABCD"). */
static i64 hex16(const AvpJson *v) {
    AvpStr t = avp_str_strip(brief_py_str(v));
    if (avp_str_startswith(t, AVP_S("0x")) || avp_str_startswith(t, AVP_S("0X"))) t = avp_str_slice(t, 2, AVP_END);
    i64 r = 0;
    if (!t.n) shsm_raise(SHSM_EXC_VALUE, "invalid literal for int() with base 16: %s", avp_cz(A(), brief_py_str(v)));
    for (size_t i = 0; i < t.n; i++) {
        char ch = t.p[i];
        int d = ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;
        if (d < 0) shsm_raise(SHSM_EXC_VALUE, "invalid literal for int() with base 16: %s", avp_cz(A(), brief_py_str(v)));
        r = r * 16 + d;
    }
    return r;
}

/* unit_functions(W, target): (title, unit or NULL, start, end, functions). */
static const char *unit_functions(BriefWorld *W, const char *target, const ShsmUnit **u_out, i64 *start, i64 *end,
                                  ShsmSymRows *fns) {
    AvpArena *a = A();
    ShsmAsmIndex *ix;
    ShsmReMatch m;
    AvpStr t = avp_str_strip(avp_str(target));
    ShsmRe *rx = shsm_re("(?:0x)?([0-9A-Fa-f]{1,8})\\s*-\\s*(?:0x)?([0-9A-Fa-f]{1,8})", 0);
    *fns = (ShsmSymRows){0};
    *u_out = NULL;
    if (shsm_re_fullmatch(rx, t, 0, (i64)t.n, &m)) {
        AvpStr g1 = shsm_re_group(&m, 1), g2 = shsm_re_group(&m, 2);
        i64 s = 0, e = 0;
        for (size_t i = 0; i < g1.n; i++) s = s * 16 + (g1.p[i] <= '9' ? g1.p[i] - '0' : (g1.p[i] | 0x20) - 'a' + 10);
        for (size_t i = 0; i < g2.n; i++) e = e * 16 + (g2.p[i] <= '9' ? g2.p[i] - '0' : (g2.p[i] | 0x20) - 'a' + 10);
        if (e <= s) shsm_raise(SHSM_EXC_USAGE, "empty range %s", target);
        ix = brief_ix(W);
        AvpJson *params = brief_arr_of(2, avp_json_int(a, s), avp_json_int(a, e));
        AvpJson *rows = shsm_asmindex_sql(
            a, ix, "SELECT name FROM symbols WHERE kind = 'function' AND address >= ? AND address < ? ORDER BY address",
            params);
        for (size_t i = 0; i < avp_json_len(rows); i++) {
            ShsmSymRow r;
            if (shsm_asmindex_symbol(a, ix, avp_json_at(avp_json_at(rows, i), 0)->u.str, &r)) avp_vec_push(fns, r);
        }
        *start = s, *end = e;
        return avp_sprintf(a, "%08llX-%08llX", (unsigned long long)s, (unsigned long long)e);
    }
    const ShsmUnit *u = NULL;
    AvpTry tr;
    if (AVP_TRY(&tr)) {
        u = shsm_get_unit(W->repo, target);
        avp_try_end(&tr);
    } else {
        if (!shsm_exc_is_key(tr.code)) avp_throw(tr.code, "%s", tr.msg);
        shsm_raise(SHSM_EXC_USAGE, "%s",
                   avp_cz(a, avp_pyfmt(a, "no unit %r (give a unit path or START-END)", target)));
    }
    ix = brief_ix(W);
    ShsmSymRows all = shsm_asmindex_unit_symbols(a, ix, u->path, "function");
    for (size_t i = 0; i < all.n; i++)
        if (!avp_str_startswith(all.v[i].name, AVP_S("gap_"))) avp_vec_push(fns, all.v[i]);
    if (!fns->n) shsm_raise(SHSM_EXC_USAGE, "unit %s has no functions in the asm index", u->name);
    *u_out = u;
    *start = fns->v[0].address;
    const ShsmSymRow *last = &fns->v[fns->n - 1];
    *end = last->address + (last->size == SHSM_NONE ? 0 : last->size);
    return u->name;
}

static AvpJson *matched_twin(BriefWorld *W, const ShsmSymRow *f) {
    AvpArena *a = A();
    if ((f->size == SHSM_NONE ? 0 : f->size) < BRIEF_TWIN_MIN_SIZE) return avp_json_null();
    const char *fp[3];
    if (!brief_twins_fps(W, f->address, fp)) return avp_json_null();
    for (int lv = 0; lv < 3; lv++) {
        BriefTwinRows rows = brief_twins_rows(W, lv, fp[lv], false, f->address);
        for (size_t i = 0; i < rows.n; i++) {
            const char *un = rows.v[i].unit && *rows.v[i].unit ? avp_sprintf(a, "main/%s", rows.v[i].unit) : rows.v[i].unit;
            if (!brief_matched(W, rows.v[i].address, un)) continue;
            AvpJson *o = avp_json_obj(a);
            brief_set(o, "level", avp_json_strz(a, TWINS_LEVELS[lv]));
            brief_set(o, "name", brief_jz(rows.v[i].name));
            brief_set(o, "source", avp_json_strz(a, brief_source_unit(W, un)->source));
            avp_vec_free(&rows);
            return o;
        }
        avp_vec_free(&rows);
    }
    return avp_json_null();
}

static AvpJson *gather_unit_functions(BriefWorld *W, void *vc) {
    UnitCtx *c = vc;
    AvpArena *a = A();
    const ShsmUnit *u = c->u;
    AvpMap *walls = avp_map_new();
    if (has_source(u))
        for (size_t i = 0; i < c->fns.n; i++) {
            const AvpJson *e = brief_wall_entry(W, u->name, c->fns.v[i].name);
            if (e && !brief_truthy(J(e, "exact")))
                avp_map_put(walls, c->fns.v[i].name, NULL)->p =
                    brief_truthy(J(e, "wall")) ? (void *)J(e, "wall") : (void *)avp_json_strz(a, "?");
        }
    AvpSet *names = avp_set_new();
    for (size_t i = 0; i < c->fns.n; i++) avp_set_add(names, c->fns.v[i].name);
    ShsmAsmIndex *ix = brief_ix(W);
    AvpJson *todo_in = avp_json_arr(a), *exact = avp_json_arr(a);
    for (size_t i = 0; i < c->fns.n; i++) {
        const ShsmSymRow *f = &c->fns.v[i];
        ShsmCallees ce = shsm_asmindex_callees(a, ix, shsm_target_name(f->name));
        AvpJson *callees = avp_json_arr(a), *internal = avp_json_arr(a);
        for (size_t k = 0; k < ce.n; k++) {
            if (brief_is_helper(ce.v[k].name)) continue;
            avp_json_arr_push(a, callees, avp_json_str(a, ce.v[k].name));
            if (avp_set_has(names, ce.v[k].name)) avp_json_arr_push(a, internal, avp_json_str(a, ce.v[k].name));
        }
        BriefScore sc = {0};
        if (has_source(u)) sc = brief_score(W, f);
        ShsmStrRows strs = shsm_asmindex_strings_of(a, ix, shsm_target_name(f->name), false);
        AvpJson *js = avp_json_arr(a);
        for (size_t k = 0; k < strs.n && k < 2; k++) avp_json_arr_push(a, js, avp_json_str(a, strs.v[k].text));
        AvpJson *it = avp_json_obj(a);
        brief_set(it, "name", avp_json_str(a, f->name));
        brief_set(it, "address", avp_json_int(a, f->address));
        brief_set(it, "size", brief_ji(f->size));
        brief_set(it, "percent", sc.percent ? sc.percent : avp_json_null());
        brief_set(it, "exact", brief_jb(sc.exact));
        brief_set(it, "twin", sc.exact ? avp_json_null() : matched_twin(W, f));
        AvpVal *wv = avp_map_get(walls, f->name);
        brief_set(it, "wall", wv ? (AvpJson *)wv->p : avp_json_null());
        brief_set(it, "callees", callees);
        brief_set(it, "internal_callees", internal);
        brief_set(it, "strings", js);
        if (sc.exact)
            avp_json_arr_push(a, exact, avp_json_str(a, f->name));
        else
            avp_json_arr_push(a, todo_in, it);
    }
    AvpJson *d = avp_json_obj(a);
    brief_set(d, "order", brief_work_order(todo_in));
    brief_set(d, "exact", exact);
    brief_set(d, "total", avp_json_int(a, (i64)c->fns.n));
    avp_map_free(walls);
    avp_set_free(names);
    return d;
}

BriefLines brief_fmt_unit_functions(const BriefSection *sec, int level) {
    AvpArena *a = A();
    const AvpJson *d = sec->data, *order = J(d, "order");
    BriefLines L = {0};
    brief_addf(&L, "  %d function(s), %d exact; %d to do", avp_json_get_i64(d, "total", 0), avp_json_len(J(d, "exact")),
               avp_json_len(order));
    static const i64 caps[] = {25, 15, 8, 4};
    size_t k, shown = brief_cap(avp_json_len(order), brief_lvl(level, caps, 4), &k);
    for (size_t i = 0; i < shown; i++) {
        const AvpJson *it = avp_json_at(order, i);
        AvpStrVec bits = {0};
        avp_vec_push(&bits, avp_pyfmt(a, "%08X %5X %s", avp_json_get_i64(it, "address", 0),
                                      avp_json_get_i64(it, "size", 0), brief_py_strz(J(it, "name"))));
        if (!avp_json_is_null(J(it, "percent"))) avp_vec_push(&bits, avp_str(brief_pct(J(it, "percent"))));
        const AvpJson *tw = J(it, "twin");
        if (brief_truthy(tw))
            avp_vec_push(&bits, avp_pyfmt(a, "twin %s (%s)", brief_py_strz(J(tw, "name")), brief_py_strz(J(tw, "source"))));
        if (brief_truthy(J(it, "wall"))) avp_vec_push(&bits, avp_pyfmt(a, "wall: %s", brief_py_strz(J(it, "wall"))));
        if (!avp_json_len(J(it, "callees"))) avp_vec_push(&bits, AVP_S("leaf"));
        const AvpJson *strs = J(it, "strings");
        if (avp_json_len(strs)) {
            AvpStr c = brief_clip(avp_json_at(strs, 0)->u.str, 30);
            avp_vec_push(&bits, avp_pyfmt(a, "\"%S\"", &c));
        }
        AvpStr j = avp_str_join(a, AVP_S("  "), bits.v, bits.n);
        brief_addf(&L, "  %S", &j);
        avp_vec_free(&bits);
    }
    brief_more(&L, k, "more functions");
    return L;
}

BriefLines brief_fmt_unit_header(const BriefSection *sec, int level) {
    (void)level;
    AvpArena *a = A();
    const AvpJson *d = sec->data;
    BriefLines L = {0};
    const AvpJson *st = J(d, "status");
    const char *status = avp_json_is_null(st) ? "unstarted range"
                         : avp_json_is_str(st) && avp_str_eqz(st->u.str, "Asm") ? "dtk asm unit (unstarted)"
                                                                                : brief_py_strz(st);
    const AvpJson *lib = J(d, "library");
    brief_addf(&L, "%s  %s  library %s  %s..%s  %d function(s), 0x%X bytes", brief_py_strz(J(d, "target")), status,
               brief_truthy(lib) ? brief_py_strz(lib) : "?", brief_hex(avp_json_get_i64(d, "start", 0)),
               brief_hex(avp_json_get_i64(d, "end", 0)), avp_json_get_i64(d, "functions", 0),
               avp_json_get_i64(d, "bytes", 0));
    brief_ref_lines(&L, d, false);
    brief_sibling_range_lines(&L, J(d, "sibling"));
    if (brief_truthy(J(d, "source"))) {
        const AvpJson *cf = J(d, "cflags");
        AvpStrVec w = {0};
        for (size_t i = 0; brief_truthy(cf) && i < avp_json_len(cf); i++) avp_vec_push(&w, brief_py_str(avp_json_at(cf, i)));
        AvpStr j = avp_str_join(a, AVP_S(" "), w.v, w.n);
        brief_addf(&L, "  source %s; %s + %s", brief_py_strz(J(d, "source")), brief_py_strz(J(d, "compiler")),
                   j.n ? avp_cz(a, j) : "base cflags");
        avp_vec_free(&w);
        /* sorted by start (stable) */
        const AvpJson *secs = J(d, "sections");
        size_t n = avp_json_len(secs);
        AVP_VEC(size_t) idx = {0};
        for (size_t i = 0; i < n; i++) {
            size_t pos = idx.n;
            i64 s0 = avp_json_i64(avp_json_at(avp_json_member(secs, i)->val, 0), 0);
            while (pos > 0 && avp_json_i64(avp_json_at(avp_json_member(secs, idx.v[pos - 1])->val, 0), 0) > s0) pos--;
            avp_vec_push(&idx, 0);
            memmove(&idx.v[pos + 1], &idx.v[pos], (idx.n - 1 - pos) * sizeof(size_t));
            idx.v[pos] = i;
        }
        for (size_t i = 0; i < idx.n; i++) {
            const AvpJsonMember *m = avp_json_member(secs, idx.v[i]);
            brief_addf(&L, "    %-8S %s-%s", &m->key, brief_hex(avp_json_i64(avp_json_at(m->val, 0), 0)),
                       brief_hex(avp_json_i64(avp_json_at(m->val, 1), 0)));
        }
        avp_vec_free(&idx);
    } else {
        brief_addf(&L, "  %s", brief_profile_line(J(d, "profile"), brief_truthy(lib) ? brief_py_strz(lib) : NULL));
    }
    return L;
}

/* ---- classes ---- */

typedef AVP_VEC(i64) I64s;
static void put_code(AvpIMap *out, I64s *order, i64 addr, AvpJson *name) {
    bool created;
    AvpVal *v = avp_imap_put(out, (u64)addr, &created);
    if (created) avp_vec_push(order, addr);
    v->p = name;
}

static int cmp_i64(const void *x, const void *y, void *ctx) {
    (void)ctx;
    i64 a = *(const i64 *)x, b = *(const i64 *)y;
    return a < b ? -1 : a > b;
}

static AvpJson *outside_class_code(BriefWorld *W, const AvpJson *c, i64 start, i64 end) {
    AvpArena *a = A();
    ShsmAsmIndex *ix = brief_ix(W);
    AvpIMap *code = avp_imap_new();
    I64s order = {0};
    const AvpJson *members = J(c, "members");
    for (size_t i = 0; brief_truthy(members) && i < avp_json_len(members); i++) {
        const AvpJson *m = avp_json_at(members, i);
        if (brief_truthy(J(m, "address"))) put_code(code, &order, hex16(J(m, "address")), J(m, "name") ? J(m, "name") : avp_json_null());
    }
    const AvpJson *vt = J(c, "vtable"), *words = brief_truthy(vt) ? J(vt, "words") : NULL;
    for (size_t i = 0; words && i < avp_json_len(words); i++) {
        const AvpJson *w = avp_json_at(words, i);
        AvpStr st = brief_jstr(w, "status");
        if (brief_truthy(J(w, "target_address")) && st.p && avp_str_eqz(st, "override"))
            put_code(code, &order, hex16(J(w, "target_address")), J(w, "target") ? J(w, "target") : avp_json_null());
    }
    const AvpJson *reg = J(c, "registry");
    for (size_t i = 0; brief_truthy(reg) && i < avp_json_len(reg); i++) {
        const AvpJson *r = avp_json_at(reg, i);
        const char *keys[] = {"sinit", "factory"};
        for (int k = 0; k < 2; k++) {
            const AvpJson *v = J(r, keys[k]);
            if (!brief_truthy(v)) continue;
            ShsmSymRow row;
            if (shsm_asmindex_symbol(a, ix, brief_py_str(v), &row) && row.address != SHSM_NONE)
                put_code(code, &order, row.address, (AvpJson *)v);
        }
    }
    /* found: address -> (address, size, name) */
    AvpIMap *found = avp_imap_new();
    AVP_VEC(i64) faddr = {0};
    AvpJson *recs = avp_json_obj(a);
    (void)recs;
    AVP_VEC(i64) fsize = {0};
    AVP_VEC(AvpJson *) fname = {0};
    for (size_t i = 0; i < order.n; i++) {
        i64 ad = order.v[i];
        if (!((ad < start && start - ad <= BRIEF_NEAR_CLASS_CODE) || (ad >= end && ad - end <= BRIEF_NEAR_CLASS_CODE)))
            continue;
        ShsmSymRows row = shsm_asmindex_at(a, ix, ad);
        bool created;
        AvpVal *v = avp_imap_put(found, (u64)ad, &created);
        if (created) {
            v->i = (i64)faddr.n;
            avp_vec_push(&faddr, ad);
            avp_vec_push(&fsize, 0);
            avp_vec_push(&fname, NULL);
        }
        fsize.v[v->i] = row.n ? row.v[0].size : SHSM_NONE;
        fname.v[v->i] = (AvpJson *)avp_imap_get(code, (u64)ad)->p;
    }
    AvpJson *out = avp_json_arr(a);
    if (faddr.n) {
        i64 tail = 0;
        for (size_t i = 0; i < faddr.n; i++) {
            i64 t = faddr.v[i] + (fsize.v[i] == SHSM_NONE || !fsize.v[i] ? 4 : fsize.v[i]);
            if (i == 0 || t > tail) tail = t;
        }
        for (;;) {
            ShsmSymRows row = shsm_asmindex_at(a, ix, tail);
            if (!row.n || !avp_str_startswith(row.v[0].name, AVP_S("@")) || row.v[0].size == SHSM_NONE || !row.v[0].size)
                break;
            bool created;
            AvpVal *v = avp_imap_put(found, (u64)tail, &created);
            if (created) {
                v->i = (i64)faddr.n;
                avp_vec_push(&faddr, tail);
                avp_vec_push(&fsize, row.v[0].size);
                avp_vec_push(&fname, avp_json_str(a, row.v[0].name));
            }
            tail += row.v[0].size;
        }
        AVP_VEC(i64) sorted = {0};
        for (size_t i = 0; i < faddr.n; i++) avp_vec_push(&sorted, faddr.v[i]);
        avp_sort(sorted.v, sorted.n, sizeof(i64), cmp_i64, NULL, false);
        for (size_t i = 0; i < sorted.n; i++) {
            i64 k = avp_imap_get(found, (u64)sorted.v[i])->i;
            AvpJson *o = avp_json_obj(a);
            brief_set(o, "address", avp_json_int(a, faddr.v[k]));
            brief_set(o, "size", brief_ji(fsize.v[k]));
            brief_set(o, "name", fname.v[k] ? fname.v[k] : avp_json_null());
            avp_json_arr_push(a, out, o);
        }
        avp_vec_free(&sorted);
    }
    avp_vec_free(&faddr);
    avp_vec_free(&fsize);
    avp_vec_free(&fname);
    avp_vec_free(&order);
    avp_imap_free(found);
    avp_imap_free(code);
    return out;
}

static AvpJson *gather_unit_classes(BriefWorld *W, void *vc) {
    UnitCtx *c = vc;
    AvpArena *a = A();
    i64 start = avp_json_get_i64(c->head, "start", 0), end = avp_json_get_i64(c->head, "end", 0);
    AvpJson *out = avp_json_arr(a);
    const AvpJson *cls = J(brief_classes(W), "classes");
    for (size_t ci = 0; cls && ci < avp_json_len(cls); ci++) {
        const AvpJson *cl = avp_json_at(cls, ci);
        const AvpJson *vt = J(cl, "vtable"), *words = brief_truthy(vt) ? J(vt, "words") : NULL;
        size_t hits = 0;
        for (size_t i = 0; words && i < avp_json_len(words); i++) {
            const AvpJson *w = avp_json_at(words, i);
            if (!brief_truthy(J(w, "target_address"))) continue;
            i64 ta = hex16(J(w, "target_address"));
            if (start <= ta && ta < end) hits++;
        }
        if (!hits) continue;
        size_t of = 0;
        for (size_t i = 0; i < avp_json_len(words); i++) of += brief_truthy(J(avp_json_at(words, i), "target"));
        AvpJson *d = avp_json_obj(a);
        brief_set(d, "class", J(cl, "name") ? J(cl, "name") : avp_json_null());
        brief_set(d, "vtable", J(cl, "vtable_symbol") ? J(cl, "vtable_symbol") : avp_json_null());
        brief_set(d, "slots", avp_json_int(a, (i64)hits));
        brief_set(d, "of", avp_json_int(a, (i64)of));
        AvpJson *outside = outside_class_code(W, cl, start, end);
        if (avp_json_len(outside)) {
            i64 lo = avp_json_get_i64(avp_json_at(outside, 0), "address", 0);
            if (start < lo) lo = start;
            i64 hi = end;
            for (size_t i = 0; i < avp_json_len(outside); i++) {
                const AvpJson *o = avp_json_at(outside, i);
                i64 sz = avp_json_get_i64(o, "size", 0);
                i64 t = avp_json_get_i64(o, "address", 0) + (sz ? sz : 4);
                if (t > hi) hi = t;
            }
            brief_set(d, "outside", outside);
            brief_set(d, "widen", avp_json_str(a, avp_pyfmt(a, "%08X-%08X", lo, hi)));
        }
        avp_json_arr_push(a, out, d);
    }
    return avp_json_len(out) ? out : NULL;
}

BriefLines brief_fmt_unit_classes(const BriefSection *sec, int level) {
    AvpArena *a = A();
    const AvpJson *d = sec->data;
    BriefLines L = {0};
    static const i64 caps[] = {6, 3, 1}, ocaps[] = {4, 3, 2};
    size_t k, shown = brief_cap(avp_json_len(d), brief_lvl(level, caps, 3), &k);
    for (size_t i = 0; i < shown; i++) {
        const AvpJson *c = avp_json_at(d, i);
        brief_addf(&L, "  %s (vtable %s): %d of %d slots here", brief_py_strz(J(c, "class")),
                   brief_py_strz(J(c, "vtable")), avp_json_get_i64(c, "slots", 0), avp_json_get_i64(c, "of", 0));
        const AvpJson *outside = J(c, "outside");
        if (brief_truthy(outside)) {
            size_t hidden, few = brief_cap(avp_json_len(outside), brief_lvl(level, ocaps, 3), &hidden);
            AvpStrVec parts = {0};
            for (size_t j = 0; j < few; j++) {
                const AvpJson *o = avp_json_at(outside, j);
                avp_vec_push(&parts, avp_pyfmt(a, "%08X %s", avp_json_get_i64(o, "address", 0), brief_py_strz(J(o, "name"))));
            }
            AvpStr pj = avp_str_join(a, AVP_S(", "), parts.v, parts.n);
            brief_addf(&L, "    also outside the range: %S", &pj);
            if (hidden) brief_addf(&L, "      ... %d more outside the range", hidden);
            brief_addf(&L, "    widen to %s", brief_py_strz(J(c, "widen")));
            avp_vec_free(&parts);
        }
    }
    brief_more(&L, k, "more classes");
    return L;
}

/* ---- includes ---- */

static int cmp_str(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return avp_str_cmp(*(const AvpStr *)x, *(const AvpStr *)y);
}

static AvpJson *gather_unit_includes(BriefWorld *W, void *vc) {
    UnitCtx *c = vc;
    AvpArena *a = A();
    AvpSet *names = avp_set_new();
    for (size_t i = 0; i < c->fns.n; i++) avp_set_add(names, c->fns.v[i].name);
    AvpMap *heads = avp_map_new(); /* header -> AvpSet * */
    AvpStrVec nohead = {0};
    ShsmAsmIndex *ix = brief_ix(W);
    const AvpJson *order = J(c->fdata, "order");
    for (size_t i = 0; i < avp_json_len(order); i++) {
        const AvpJson *cs = J(avp_json_at(order, i), "callees");
        for (size_t k = 0; k < avp_json_len(cs); k++) {
            AvpStr cn = avp_json_at(cs, k)->u.str;
            if (avp_set_has(names, cn)) continue;
            ShsmSymRow r;
            if (!shsm_asmindex_symbol(a, ix, cn, &r) || !brief_matched(W, r.address, brief_unit_name_of(&r))) continue;
            const char *h = brief_header_of(W, avp_cz(a, cn));
            if (h) {
                bool created;
                AvpVal *v = avp_map_put(heads, avp_str(h), &created);
                if (created) v->p = avp_set_new();
                avp_set_add((AvpSet *)v->p, cn);
            } else {
                bool dup = false;
                for (size_t j = 0; j < nohead.n && !dup; j++) dup = avp_str_eq(nohead.v[j], cn);
                if (!dup) avp_vec_push(&nohead, cn);
            }
        }
    }
    if (!avp_map_len(heads) && !nohead.n) return NULL;
    AvpStrVec hk = {0};
    AvpMapIter it = {0};
    while (avp_map_next(heads, &it)) avp_vec_push(&hk, it.key);
    avp_sort(hk.v, hk.n, sizeof(AvpStr), cmp_str, NULL, false);
    AvpJson *hj = avp_json_obj(a);
    for (size_t i = 0; i < hk.n; i++) {
        AvpStrVec v = avp_set_sorted((AvpSet *)avp_map_get(heads, hk.v[i])->p);
        AvpJson *arr = avp_json_arr(a);
        for (size_t k = 0; k < v.n; k++) avp_json_arr_push(a, arr, avp_json_str(a, v.v[k]));
        avp_json_set(a, hj, hk.v[i], arr);
    }
    AvpJson *un = avp_json_arr(a);
    for (size_t i = 0; i < nohead.n; i++) avp_json_arr_push(a, un, avp_json_str(a, nohead.v[i]));
    AvpJson *d = avp_json_obj(a);
    brief_set(d, "headers", hj);
    brief_set(d, "undeclared", un);
    avp_vec_free(&hk);
    avp_vec_free(&nohead);
    return d;
}

static AvpStr join_arr(const AvpJson *arr, AvpStr sep) {
    AvpStrVec w = {0};
    for (size_t i = 0; i < avp_json_len(arr); i++) avp_vec_push(&w, brief_py_str(avp_json_at(arr, i)));
    AvpStr s = avp_str_join(A(), sep, w.v, w.n);
    avp_vec_free(&w);
    return s;
}

BriefLines brief_fmt_unit_includes(const BriefSection *sec, int level) {
    const AvpJson *d = sec->data, *heads = J(d, "headers");
    BriefLines L = {0};
    static const i64 caps[] = {10, 5, 2};
    size_t k, shown = brief_cap(avp_json_len(heads), brief_lvl(level, caps, 3), &k);
    for (size_t i = 0; i < shown; i++) {
        const AvpJsonMember *m = avp_json_member(heads, i);
        AvpStr c = brief_clip(join_arr(m->val, AVP_S(", ")), 70);
        brief_addf(&L, "  #include <%S>  // %S", &m->key, &c);
    }
    brief_more(&L, k, "more headers");
    const AvpJson *un = J(d, "undeclared");
    if (avp_json_len(un)) {
        AvpStr c = brief_clip(join_arr(un, AVP_S(", ")), 100);
        brief_addf(&L, "  matched but in no header: %S", &c);
    }
    return L;
}

/* ---- notes ---- */

static AvpJson *gather_unit_notes(BriefWorld *W, void *vc) {
    UnitCtx *c = vc;
    AvpArena *a = A();
    const ShsmUnit *u = c->u;
    AvpJson *d = avp_json_obj(a), *banner = avp_json_arr(a);
    brief_set(d, "banner", banner);
    brief_set(d, "docs", avp_json_arr(a));
    if (has_source(u)) {
        WallNote *notes;
        size_t n = wall_find_notes(a, avp_path_join(a, W->root, u->source), &notes);
        for (size_t i = 0; i < n; i++)
            avp_json_arr_push(a, banner, brief_arr_of(4, avp_json_strz(a, u->source), avp_json_int(a, notes[i].line),
                                                      avp_json_str(a, notes[i].text),
                                                      notes[i].has_addr ? avp_json_int(a, (i64)notes[i].addr)
                                                                        : avp_json_null()));
    }
    AvpStr term = u ? avp_str(u->path) : brief_jstr(c->head, "target");
    brief_set(d, "docs", brief_docs_notes(W, term, false));
    return avp_json_len(banner) || avp_json_len(J(d, "docs")) ? d : NULL;
}

BriefLines brief_fmt_unit_notes(const BriefSection *sec, int level) {
    AvpArena *a = A();
    const AvpJson *d = sec->data;
    BriefLines L = {0};
    static const i64 caps[] = {6, 3, 1};
    i64 n = brief_lvl(level, caps, 3);
    const AvpJson *banner = J(d, "banner");
    size_t k, shown = brief_cap(avp_json_len(banner), n, &k);
    for (size_t i = 0; i < shown; i++) {
        const AvpJson *b = avp_json_at(banner, i), *ad = avp_json_at(b, 3);
        AvpStr at = brief_truthy(ad) ? avp_pyfmt(a, " (%s)", brief_hex(avp_json_i64(ad, 0))) : AVP_S("");
        AvpStr t = brief_clip(avp_json_at(b, 2)->u.str, 90);
        brief_addf(&L, "  %s:%d%S: %S", brief_py_strz(avp_json_at(b, 0)), avp_json_i64(avp_json_at(b, 1), 0), &at, &t);
    }
    brief_more(&L, k, "more source notes (build/tools/wall.exe notes --unit ...)");
    brief_docs_lines(&L, J(d, "docs"), n, 80, "more docs passages");
    return L;
}

static AvpJson *gather_unit_start(BriefWorld *W, void *vc) {
    (void)W;
    UnitCtx *c = vc;
    AvpArena *a = A();
    AvpJson *L = avp_json_arr(a);
    const ShsmUnit *u = c->u;
    if (has_source(u)) {
        avp_json_arr_push(a, L, avp_json_str(a, avp_pyfmt(a, "  build/tools/match.exe %s", u->path)));
        avp_json_arr_push(a, L, avp_json_str(a, avp_pyfmt(a, "  build/tools/wall.exe scan --unit %s", u->path)));
        return L;
    }
    i64 s = avp_json_get_i64(c->head, "start", 0), e = avp_json_get_i64(c->head, "end", 0);
    avp_json_arr_push(a, L, avp_json_str(a, avp_pyfmt(a, "  build/tools/unit_plan.exe %08X-%08X", s, e)));
    if (!u || (u->status && strcmp(u->status, "Asm") == 0))
        avp_json_arr_push(a, L, avp_json_str(a, avp_pyfmt(a, "  build/tools/unit.exe new <path> --text 0x%08X-0x%08X "
                                                             "--dry-run   # after the plan agrees",
                                                          s, e)));
    return L;
}

/* ---- the unit briefing ---- */

static const char *const UNIT_SECTIONS[] = {"header", "functions", "classes", "includes", "notes", "start"};

const char *brief_unit_briefing(BriefWorld *W, const BriefOpts *o, BriefSections *out, BriefTimings *tm) {
    AvpArena *a = A();
    if (!W->ctx) W->ctx = query_ctx_for_root(a, W->root);
    double t0 = avp_time();
    UnitCtx *c = AVP_NEW(a, UnitCtx);
    i64 start, end;
    const char *title = unit_functions(W, o->unit, &c->u, &start, &end, &c->fns);
    const ShsmUnit *u = c->u;
    const char *lib = brief_library(W, start);
    const char *ref_section = NULL;
    const char *flag = brief_reference(lib, &ref_section);
    i64 bytes = 0;
    for (size_t i = 0; i < c->fns.n; i++) bytes += c->fns.v[i].size == SHSM_NONE ? 0 : c->fns.v[i].size;
    AvpJson *head = avp_json_obj(a);
    brief_set(head, "target", avp_json_strz(a, title));
    brief_set(head, "unit", brief_jz(u ? u->name : NULL));
    brief_set(head, "status", brief_jz(u ? u->status : NULL));
    brief_set(head, "source", brief_jz(has_source(u) ? u->source : NULL));
    brief_set(head, "library", brief_jz(lib));
    brief_set(head, "reference", brief_jz(flag));
    brief_set(head, "reference_section", brief_jz(ref_section));
    brief_set(head, "start", avp_json_int(a, start));
    brief_set(head, "end", avp_json_int(a, end));
    brief_set(head, "functions", avp_json_int(a, (i64)c->fns.n));
    brief_set(head, "bytes", avp_json_int(a, bytes));
    brief_set(head, "compiler", avp_json_null());
    brief_set(head, "cflags", avp_json_null());
    brief_set(head, "profile", avp_json_null());
    brief_set(head, "sections", avp_json_obj(a));
    brief_set(head, "sibling", brief_sibling_range(W, start, end));
    if (has_source(u)) {
        brief_set(head, "compiler", brief_jz(u->mw_version));
        brief_set(head, "cflags", brief_base_words(&u->cflags));
        AvpJson *secs = avp_json_obj(a);
        AvpTry t;
        if (AVP_TRY(&t)) {
            const ShsmSplitUnit *su = shsm_splits_unit(brief_splits(W), u->path);
            for (size_t i = 0; su && i < su->nranges; i++)
                avp_json_set(a, secs, avp_str(su->ranges[i].section),
                             brief_arr_of(2, avp_json_int(a, (i64)su->ranges[i].start),
                                          avp_json_int(a, (i64)su->ranges[i].end)));
            avp_try_end(&t);
        } else {
            secs = avp_json_obj(a);
        }
        brief_set(head, "sections", secs);
    } else {
        brief_set(head, "profile", brief_profile_of(W, start, lib));
    }
    c->head = head;
    double th = avp_time() - t0;
    brief_timing(tm, "header", th);
    BriefSection *secs[6] = {0};
    BriefSection *hs = AVP_NEW(a, BriefSection);
    hs->name = "header";
    hs->data = head;
    hs->fmt = brief_fmt_unit_header;
    hs->levels = 1;
    hs->seconds = th;
    secs[0] = hs;
    BriefSection *fs = brief_build_section(W, "functions", gather_unit_functions, c, brief_fmt_unit_functions, 4);
    secs[1] = fs;
    secs[2] = brief_build_section(W, "classes", gather_unit_classes, c, brief_fmt_unit_classes, 3);
    bool have_includes = false;
    if (fs && fs->data) {
        c->fdata = fs->data;
        secs[3] = brief_build_section(W, "includes", gather_unit_includes, c, brief_fmt_unit_includes, 3);
        have_includes = true;
    }
    secs[4] = brief_build_section(W, "notes", gather_unit_notes, c, brief_fmt_unit_notes, 3);
    secs[5] = brief_build_section(W, "start", gather_unit_start, c, brief_fmt_list, 1);
    for (size_t i = 0; i < 6; i++)
        if (secs[i] && (i != 3 || have_includes)) brief_timing(tm, UNIT_SECTIONS[i], secs[i]->seconds);
    out->n = 0;
    for (size_t i = 0; i < 6; i++)
        if (brief_wants(o->sections, UNIT_SECTIONS[i]) && secs[i]) out->v[out->n++] = secs[i];
    return title;
}
