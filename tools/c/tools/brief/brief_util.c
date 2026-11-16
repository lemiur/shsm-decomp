/* brief.py: _clip, _pct, _hex, _cap, _more, lvl, the Section object, layout, render, parse_sections, work_order,
 * _base_words and profile_line. */
#include "brief.h"

#include <stdarg.h>
#include <string.h>

static AvpArena *A(void) { return avp_arena(); }

void brief_add(BriefLines *L, AvpStr s) { avp_vec_push(L, s); }
void brief_addz(BriefLines *L, const char *s) { avp_vec_push(L, avp_str_dup(A(), avp_str(s))); }

void brief_addf_(BriefLines *L, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    AvpBuf b;
    avp_buf_init(&b, A());
    avp_buf_pyvfmt(&b, fmt, ap);
    va_end(ap);
    avp_vec_push(L, avp_buf_str(&b));
}

AvpStr brief_clip(AvpStr text, i64 n) {
    AvpStrVec words = avp_str_split(text, (AvpStr){0}, -1);
    AvpStr t = avp_str_join(A(), AVP_S(" "), words.v, words.n);
    avp_vec_free(&words);
    if ((i64)avp_u8_len(t) <= n) return t;
    return avp_str_concat(A(), avp_u8_slice(t, 0, n - 3), AVP_S("..."));
}

const char *brief_clipz(const char *text, i64 n) { return avp_cz(A(), brief_clip(avp_str(text), n)); }

const char *brief_pct(const AvpJson *p) {
    if (avp_json_is_null(p)) return "-";
    double v = avp_json_f64(p, 0.0);
    if (v < 100) return avp_cz(A(), avp_pyfmt(A(), "%.1f%%", v));
    return "100%";
}

const char *brief_hex(i64 v) { return avp_cz(A(), avp_pyfmt(A(), "0x%08X", v)); }

AvpStr brief_py_str(const AvpJson *v) {
    if (avp_json_is_null(v)) return AVP_S("None");
    switch (v->kind) {
    case AVP_JSON_BOOL: return v->b ? AVP_S("True") : AVP_S("False");
    case AVP_JSON_INT: return v->big ? v->u.str : avp_pyfmt(A(), "%d", v->u.i);
    case AVP_JSON_FLOAT: return avp_pyfmt(A(), "%v", v->u.f);
    case AVP_JSON_STRING: return v->u.str;
    default: {
        AvpJsonDumpOpts o = {0};
        return avp_json_dumps(A(), v, &o); /* not reached for the values brief prints */
    }
    }
}

const char *brief_py_strz(const AvpJson *v) { return avp_cz(A(), brief_py_str(v)); }

bool brief_truthy(const AvpJson *v) { return !avp_json_is_null(v) && avp_json_truthy(v); }

AvpStr brief_jstr(const AvpJson *obj, const char *key) {
    const AvpJson *v = obj ? avp_json_getz(obj, key) : NULL;
    if (!v || !avp_json_is_str(v)) return (AvpStr){0};
    return v->u.str;
}

i64 brief_lvl(int level, const i64 *caps, size_t n) {
    if (level < 0) return -1;
    return caps[(size_t)level < n ? (size_t)level : n - 1];
}

size_t brief_cap(size_t len, i64 n, size_t *hidden) {
    if (n < 0 || len <= (size_t)n) {
        *hidden = 0;
        return len;
    }
    *hidden = len - (size_t)n;
    return (size_t)n;
}

void brief_more(BriefLines *L, size_t k, const char *what) {
    if (k > 0) brief_addf(L, "  ... %d %s", k, what);
}

AvpJson *brief_jz(const char *s) { return s ? avp_json_strz(A(), s) : avp_json_null(); }
AvpJson *brief_ji(i64 v) { return v == SHSM_NONE ? avp_json_null() : avp_json_int(A(), v); }
AvpJson *brief_jb(bool b) { return avp_json_bool(b); }
void brief_set(AvpJson *obj, const char *key, AvpJson *v) { avp_json_set(A(), obj, avp_str(key), v); }

AvpJson *brief_arr_of(size_t n, ...) {
    AvpJson *arr = avp_json_arr(A());
    va_list ap;
    va_start(ap, n);
    for (size_t i = 0; i < n; i++) avp_json_arr_push(A(), arr, va_arg(ap, AvpJson *));
    va_end(ap);
    return arr;
}

/* ---- sections ---- */

BriefLines brief_section_lines(const BriefSection *s, int level) {
    BriefLines L = {0};
    if (s->unavailable) {
        brief_addf(&L, "unavailable: %s", s->unavailable);
        return L;
    }
    if (!s->fmt) return L;
    return s->fmt(s, level);
}

AvpJson *brief_section_json(const BriefSection *s, int level) {
    AvpArena *a = A();
    AvpJson *o = avp_json_obj(a), *lines = avp_json_arr(a);
    BriefLines L = brief_section_lines(s, level);
    for (size_t i = 0; i < L.n; i++) avp_json_arr_push(a, lines, avp_json_str(a, L.v[i]));
    brief_set(o, "lines", lines);
    brief_set(o, "data", s->data ? s->data : avp_json_null());
    brief_set(o, "unavailable", brief_jz(s->unavailable));
    brief_set(o, "seconds", avp_json_float(a, avp_py_round(s->seconds, 3)));
    return o;
}

/* SHRINK: (section, deepest level at this stage; -1 = its last) */
static const struct {
    const char *name;
    int deepest;
} SHRINK[] = {{"listing", 4}, {"context", -1}, {"functions", -1}, {"twins", -1}, {"includes", -1},
              {"classes", -1}, {"status", -1}, {"notes", -1}, {"listing", -1}};

static i64 page_total(const BriefSections *secs, const int *levels) {
    i64 t = 0;
    for (size_t i = 0; i < secs->n; i++) t += 1 + (i64)brief_section_lines(secs->v[i], levels[i]).n;
    return t;
}

void brief_layout(const BriefSections *secs, i64 budget, bool full, int *levels) {
    for (size_t i = 0; i < secs->n; i++) levels[i] = full ? BRIEF_FULL : 0;
    if (full) return;
    while (page_total(secs, levels) > budget) {
        bool moved = false;
        for (size_t k = 0; k < sizeof SHRINK / sizeof SHRINK[0] && !moved; k++) {
            for (size_t i = 0; i < secs->n; i++) {
                const BriefSection *s = secs->v[i];
                if (strcmp(s->name, SHRINK[k].name) != 0 || s->unavailable) continue;
                int stop = SHRINK[k].deepest < 0 ? s->levels - 1
                                                 : (SHRINK[k].deepest < s->levels - 1 ? SHRINK[k].deepest
                                                                                      : s->levels - 1);
                if (levels[i] < stop) {
                    levels[i]++;
                    moved = true;
                }
                break;
            }
        }
        if (!moved) break;
    }
}

const char *brief_title(const char *name) {
    static const char *const T[][2] = {{"identity", "Identity"}, {"status", "Where it stands"}, {"twins", "Twins"},
                                       {"context", "Context"}, {"shape", "Shape"}, {"listing", "Listing"},
                                       {"start", "Start here"}, {"header", "Unit"},
                                       {"functions", "Functions (suggested order)"}, {"classes", "Classes"},
                                       {"includes", "Includes"}, {"notes", "Notes"}};
    for (size_t i = 0; i < sizeof T / sizeof T[0]; i++)
        if (strcmp(T[i][0], name) == 0) return T[i][1];
    return name;
}

BriefLines brief_render(const BriefSections *secs, const int *levels) {
    BriefLines L = {0};
    for (size_t i = 0; i < secs->n; i++) {
        BriefLines body = brief_section_lines(secs->v[i], levels[i]);
        if (!body.n) continue;
        const char *k = secs->v[i]->name;
        if (strcmp(k, "identity") != 0 && strcmp(k, "header") != 0) brief_addf(&L, "## %s", brief_title(k));
        for (size_t j = 0; j < body.n; j++) brief_add(&L, body.v[j]);
    }
    return L;
}

AvpStrVec *brief_parse_sections(const char *text, const char *const *allowed, size_t nallowed) {
    if (!text || !*text) return NULL;
    AvpArena *a = A();
    AvpStrVec *out = AVP_NEW(a, AvpStrVec);
    AvpStrVec parts = avp_str_split(avp_str(text), AVP_S(","), -1);
    static const char *const alias[][2] = {
        {"where", "status"}, {"stands", "status"}, {"asm", "listing"}, {"start-here", "start"}};
    for (size_t i = 0; i < parts.n; i++) {
        AvpStr w = avp_str_lower(a, avp_str_strip(parts.v[i]));
        for (size_t k = 0; k < sizeof alias / sizeof alias[0]; k++)
            if (avp_str_eqz(w, alias[k][0])) w = avp_str(alias[k][1]);
        bool ok = false;
        for (size_t k = 0; k < nallowed; k++) ok = ok || avp_str_eqz(w, allowed[k]);
        if (!ok) {
            AvpStrVec al = {0};
            for (size_t k = 0; k < nallowed; k++) avp_vec_push(&al, avp_str(allowed[k]));
            AvpStr joined = avp_str_join(a, AVP_S(", "), al.v, al.n);
            shsm_raise(SHSM_EXC_USAGE, "%s",
                       avp_cz(a, avp_pyfmt(a, "unknown section %R (one of %S)", &w, &joined)));
        }
        avp_vec_push(out, w);
    }
    avp_vec_free(&parts);
    return out;
}

bool brief_wants(const AvpStrVec *want, const char *name) {
    if (!want) return true;
    for (size_t i = 0; i < want->n; i++)
        if (avp_str_eqz(want->v[i], name)) return true;
    return false;
}

/* ---- work order ---- */

typedef struct {
    const AvpJson *it;
    AvpStr name;
    AvpStrVec deps;
    bool done, remaining;
} WoNode;

static int wo_key_cmp(const AvpJson *x, const AvpJson *y) {
    bool tx = avp_json_is_null(avp_json_getz(x, "twin")), ty = avp_json_is_null(avp_json_getz(y, "twin"));
    if (tx != ty) return tx ? 1 : -1;
    bool cx = avp_json_len(avp_json_getz(x, "callees")) > 0, cy = avp_json_len(avp_json_getz(y, "callees")) > 0;
    if (cx != cy) return cx ? 1 : -1;
    i64 sx = avp_json_get_i64(x, "size", 0), sy = avp_json_get_i64(y, "size", 0);
    if (sx != sy) return sx < sy ? -1 : 1;
    i64 ax = avp_json_get_i64(x, "address", 0), ay = avp_json_get_i64(y, "address", 0);
    return ax < ay ? -1 : ax > ay;
}

AvpJson *brief_work_order(const AvpJson *items) {
    AvpArena *a = A();
    size_t n = avp_json_len(items);
    AvpMap *by = avp_map_new(); /* name -> node index (the last item of a name, at the first place) */
    AVP_VEC(WoNode) nodes = {0};
    for (size_t i = 0; i < n; i++) {
        const AvpJson *it = avp_json_at(items, i);
        AvpStr name = brief_jstr(it, "name");
        bool created;
        AvpVal *v = avp_map_put(by, name, &created);
        if (created) {
            v->i = (i64)nodes.n;
            avp_vec_push(&nodes, ((WoNode){.it = it, .name = name, .remaining = true}));
        } else {
            nodes.v[v->i].it = it;
        }
    }
    for (size_t i = 0; i < nodes.n; i++) {
        const AvpJson *cs = avp_json_getz(nodes.v[i].it, "callees");
        for (size_t k = 0; k < avp_json_len(cs); k++) {
            AvpStr c = avp_json_at(cs, k)->u.str;
            if (avp_map_get(by, c) && !avp_str_eq(c, nodes.v[i].name)) avp_vec_push(&nodes.v[i].deps, c);
        }
    }
    AvpJson *out = avp_json_arr(a);
    for (size_t round = 0; round < nodes.n; round++) {
        WoNode *best = NULL, *best_any = NULL;
        for (size_t i = 0; i < nodes.n; i++) {
            WoNode *nd = &nodes.v[i];
            if (!nd->remaining) continue;
            if (!best_any || wo_key_cmp(nd->it, best_any->it) < 0) best_any = nd;
            bool ready = true;
            for (size_t k = 0; k < nd->deps.n && ready; k++) ready = nodes.v[avp_map_get(by, nd->deps.v[k])->i].done;
            if (ready && (!best || wo_key_cmp(nd->it, best->it) < 0)) best = nd;
        }
        WoNode *pick = best ? best : best_any;
        avp_json_arr_push(a, out, (AvpJson *)pick->it);
        pick->done = true;
        pick->remaining = false;
    }
    for (size_t i = 0; i < nodes.n; i++) avp_vec_free(&nodes.v[i].deps);
    avp_vec_free(&nodes);
    avp_map_free(by);
    return out;
}

/* ---- the compile profile ---- */

AvpJson *brief_base_words(const AvpStrVec *cflags) {
    AvpArena *a = A();
    i64 last = -1;
    for (size_t i = 0; i < cflags->n; i++)
        if (avp_str_startswith(cflags->v[i], AVP_S("-D"))) last = (i64)i;
    AvpJson *out = avp_json_arr(a);
    for (size_t i = (size_t)(last + 1); i < cflags->n; i++)
        if (!avp_str_startswith(cflags->v[i], AVP_S("-lang"))) avp_json_arr_push(a, out, avp_json_str(a, cflags->v[i]));
    return out;
}

static AvpStr join_words(const AvpJson *arr) {
    AvpStrVec w = {0};
    for (size_t i = 0; i < avp_json_len(arr); i++) avp_vec_push(&w, brief_py_str(avp_json_at(arr, i)));
    AvpStr s = avp_str_join(A(), AVP_S(" "), w.v, w.n);
    avp_vec_free(&w);
    return s;
}

const char *brief_profile_line(const AvpJson *p, const char *lib) {
    AvpArena *a = A();
    if (!brief_truthy(p)) return "expected profile: unknown (no source unit to infer from)";
    AvpStr extra = join_words(avp_json_getz(p, "extra"));
    AvpStr lang = brief_truthy(avp_json_getz(p, "lang"))
                      ? avp_pyfmt(a, " (-lang=%s)", brief_py_strz(avp_json_getz(p, "lang")))
                      : AVP_S("");
    AvpStr flags = avp_pyfmt(a, "%s + %s%S", brief_py_strz(avp_json_getz(p, "compiler")),
                             extra.n ? avp_cz(a, extra) : "base cflags", &lang);
    const char *unit = shsm_unit_path(a, brief_py_strz(avp_json_getz(p, "unit")));
    AvpStr match = brief_jstr(p, "match");
    if (avp_str_eqz(match, "different library"))
        return avp_cz(a, avp_pyfmt(a,
                                   "expected profile: unknown, no source unit of %s yet (nearest source unit %s is "
                                   "another library: %S; middleware may use another compiler, docs/toolchain.md)",
                                   lib && *lib ? lib : "this library", unit, &flags));
    return avp_cz(a, avp_pyfmt(a, "expected profile (inferred from %s, nearest %S source unit): %S", unit, &match,
                               &flags));
}

/* ---- timings ---- */

void brief_timing(BriefTimings *t, const char *key, double v) {
    for (size_t i = 0; i < t->n; i++)
        if (strcmp(t->keys[i], key) == 0) {
            t->vals[i] = v;
            return;
        }
    if (t->n < sizeof t->keys / sizeof t->keys[0]) {
        t->keys[t->n] = key;
        t->vals[t->n++] = v;
    }
}

BriefLines brief_fmt_list(const BriefSection *s, int level) {
    (void)level;
    BriefLines L = {0};
    for (size_t i = 0; i < avp_json_len(s->data); i++) brief_add(&L, avp_json_at(s->data, i)->u.str);
    return L;
}
