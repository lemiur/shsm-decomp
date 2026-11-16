/* sibling: the reports (pair summary, of, names, units, stats). */
#include "sibling.h"
#include "sibling_lookup.h"

#include "avp/sort.h"
#include "shsm_mangle.h"
#include "shsm_symbols.h"
#include "twins.h"

#include <stdlib.h>
#include <string.h>

const char *sibling_name_agreement(AvpArena *a, const char *shsm, const char *sib) {
    if (!shsm || !sib) return "differ";
    if (strcmp(shsm, sib) == 0) return "same";
    const char *q1 = shsm_qualified(a, shsm), *q2 = shsm_qualified(a, sib);
    if (q1 && q2 && strcmp(q1, q2) == 0) return "same-name";
    return "differ";
}

static bool lib_ok(AvpArena *a, const SibFn *f, const char *lib) {
    if (!lib) return true;
    TwinsFn t = {0};
    t.lib = f->lib;
    return twins_lib_matches(a, &t, lib);
}

static bool tier_ok(const SibArgs *args, int tier) { return tier && (args->tiers & (1u << tier)); }

static const char *owner(AvpArena *a, const SibFn *g) {
    if (!g->object) return "(no map object)";
    return g->lib && *g->lib ? avp_sprintf(a, "%s %s", g->lib, g->object) : g->object;
}

static const char *demangled(AvpArena *a, const char *name) {
    const char *d = name ? shsm_demangle_or_name(a, name) : NULL;
    return d ? d : (name ? name : "?");
}

/* ---- pair summary and stats ---- */

typedef struct {
    const char *lib;
    i64 fns, bytes, n[SIB_NTIERS], b[SIB_NTIERS];
} LibRow;

static LibRow *lib_rows(AvpArena *a, const SibFnVec *s, const SibArgs *args, size_t *count) {
    AvpMap *m = avp_map_new();
    AVP_VEC(LibRow *) rows = {0};
    for (size_t i = 0; i < s->n; i++) {
        const SibFn *f = &s->v[i];
        if (!lib_ok(a, f, args->lib)) continue;
        bool created;
        AvpVal *v = avp_map_put(m, avp_str(f->lib ? f->lib : "unknown"), &created);
        if (created) {
            LibRow *r = avp_alloc(a, sizeof *r);
            r->lib = f->lib ? f->lib : "unknown";
            v->p = r;
            avp_vec_push(&rows, r);
        }
        LibRow *r = v->p;
        r->fns++;
        r->bytes += f->size;
        r->n[f->tier]++;
        r->b[f->tier] += f->size;
    }
    avp_map_free(m);
    *count = rows.n;
    LibRow *out = avp_alloc(a, (rows.n + 1) * sizeof *out);
    for (size_t k = 0; k < rows.n; k++) {
        out[k] = *rows.v[k];
        out[rows.n].fns += rows.v[k]->fns;
        out[rows.n].bytes += rows.v[k]->bytes;
        for (int t = 0; t < SIB_NTIERS; t++) out[rows.n].n[t] += rows.v[k]->n[t], out[rows.n].b[t] += rows.v[k]->b[t];
    }
    out[rows.n].lib = "total";
    avp_vec_free(&rows);
    return out;
}

static void print_lib_rows(AvpArena *a, const LibRow *rows, size_t n, ShsmOutput *out, AvpJson *arr) {
    shsm_output_linez(out, "library            functions      bytes |    exact  exact-seq      calls   seq-size |"
                           "  unpaired");
    for (size_t k = 0; k <= n; k++) {
        const LibRow *r = &rows[k];
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_printf(&b, "%-18s %9lld %10lld |", r->lib, (long long)r->fns, (long long)r->bytes);
        for (int t = 1; t < SIB_NTIERS; t++) avp_buf_printf(&b, " %10lld", (long long)r->n[t]);
        avp_buf_printf(&b, " | %9lld", (long long)r->n[0]);
        shsm_output_linez(out, avp_buf_cz(&b));
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("lib"), avp_json_strz(a, r->lib));
        avp_json_set(a, o, AVP_S("functions"), avp_json_int(a, r->fns));
        avp_json_set(a, o, AVP_S("bytes"), avp_json_int(a, r->bytes));
        for (int t = 0; t < SIB_NTIERS; t++) {
            AvpJson *x = avp_json_obj(a);
            avp_json_set(a, x, AVP_S("functions"), avp_json_int(a, r->n[t]));
            avp_json_set(a, x, AVP_S("bytes"), avp_json_int(a, r->b[t]));
            avp_json_set(a, o, avp_str(t ? SIBLING_TIERS[t] : "unpaired"), x);
        }
        avp_json_arr_push(a, arr, o);
    }
}

int sibling_cmd_pair_report(Sibling *sb, const SibFnVec *s, const SibFnVec *g, const SibPairCounts *c,
                            const SibArgs *args, ShsmOutput *out) {
    AvpArena *a = avp_arena();
    shsm_output_linez(out, avp_sprintf(a, "%s: %zu retail functions, %zu sibling functions; paired exact %lld,"
                                          " exact-seq %lld, calls %lld, seq-size %lld (%lld rounds, %lld undone by contradicting callees)",
                                       sb->name, s->n, g->n, (long long)c->exact, (long long)c->exact_seq,
                                       (long long)c->calls, (long long)c->seq_size, (long long)c->rounds, (long long)c->rejected));
    size_t n;
    LibRow *rows = lib_rows(a, s, args, &n);
    AvpJson *arr = avp_json_arr(a);
    print_lib_rows(a, rows, n, out, arr);
    shsm_output_data(out, "libraries", arr);
    return 0;
}

/* ---- of ---- */
/* A function of v by name, by demangled name, or (by_address, retail side) by address: 0x8032C1E0, 8032C1E0 or
 * fn_8032C1E0 (any address inside the function). */
i64 sibling_resolve(AvpArena *a, const SibFnVec *v, const char *text, bool by_address) {
    if (!text) return -1;
    for (size_t i = 0; i < v->n; i++)
        if (v->v[i].name && strcmp(v->v[i].name, text) == 0) return (i64)i;
    for (size_t i = 0; i < v->n; i++)
        if (v->v[i].name && strcmp(demangled(a, v->v[i].name), text) == 0) return (i64)i;
    if (!by_address) return -1;
    const char *hex = text;
    bool placeholder = false;
    if (strncmp(hex, "0x", 2) == 0 || strncmp(hex, "0X", 2) == 0) hex += 2;
    else if (strncmp(hex, "fn_", 3) == 0) hex += 3, placeholder = true;
    if (strspn(hex, "0123456789abcdefABCDEF") != 8 || !(hex[8] == 0 || (placeholder && hex[8] == '_'))) return -1;
    i64 addr = (i64)strtoull(hex, NULL, 16);
    for (size_t i = 0; i < v->n; i++)
        if (addr >= v->v[i].address && addr < v->v[i].address + v->v[i].size) return (i64)i;
    return -1;
}



/* The sibling functions that carry the retail function's own (mangled) name. A placeholder name has no counterpart by
 * name. The first is returned; *count tells whether the name is ambiguous. */
i64 sibling_same_named(const SibFnVec *g, const char *name, size_t *count) {
    i64 first = -1;
    size_t n = 0;
    if (name && !sibling_is_placeholder(name))
        for (size_t i = 0; i < g->n; i++)
            if (g->v[i].name && strcmp(g->v[i].name, name) == 0) {
                if (first < 0) first = (i64)i;
                n++;
            }
    if (count) *count = n;
    return first;
}

const char *sibling_same_named_note(AvpArena *a, const SibFn *f, const SibFnVec *g) {
    size_t n;
    i64 j = sibling_same_named(g, f->name, &n);
    if (j < 0) return NULL;
    if (n > 1)
        return avp_sprintf(a, "no exact pair; %zu same-named sibling functions exist (the first at 0x%08llX, size 0x%llX"
                              " vs 0x%llX): `sibling.exe fn '%s' --sibling`",
                           n, (long long)g->v[j].address, (long long)g->v[j].size, (long long)f->size, f->name);
    return avp_sprintf(a, "no exact pair; same-named sibling function exists (size 0x%llX vs 0x%llX, %s):"
                          " `sibling.exe fn '%s' --sibling`",
                       (long long)g->v[j].size, (long long)f->size, owner(a, &g->v[j]), f->name);
}

static AvpJson *fn_json(AvpArena *a, const SibFn *f, bool sib) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("address"), avp_json_strz(a, avp_sprintf(a, "0x%08llX", (long long)f->address)));
    avp_json_set(a, o, AVP_S("name"), f->name ? avp_json_strz(a, f->name) : avp_json_null());
    avp_json_set(a, o, AVP_S("demangled"), avp_json_strz(a, demangled(a, f->name)));
    avp_json_set(a, o, AVP_S("size"), avp_json_int(a, f->size));
    avp_json_set(a, o, AVP_S("lib"), f->lib ? avp_json_strz(a, f->lib) : avp_json_null());
    avp_json_set(a, o, avp_str(sib ? "object" : "unit"), f->object ? avp_json_strz(a, f->object) : avp_json_null());
    if (sib) avp_json_set(a, o, AVP_S("order"), avp_json_int(a, f->order));
    avp_json_set(a, o, AVP_S("tier"), avp_json_strz(a, f->tier ? SIBLING_TIERS[f->tier] : "unpaired"));
    return o;
}

int sibling_cmd_of(Sibling *sb, const SibFnVec *s, const SibFnVec *g, const SibArgs *args, ShsmOutput *out) {
    (void)sb;
    AvpArena *a = avp_arena();
    i64 i = sibling_resolve(a, s, args->target, true);
    if (i < 0) {
        /* a sibling name: its retail counterpart, or the sibling function alone when it has none */
        i64 j = sibling_resolve(a, g, args->target, false);
        if (j < 0)
            shsm_raise(SHSM_EXC_USAGE, "no retail function named or containing %s, and no sibling function of that name",
                       args->target);
        const SibFn *p = &g->v[j];
        if (p->peer < 0) {
            shsm_output_linez(out, avp_sprintf(a, "sibling %s at 0x%08llX size 0x%llX, %s #%lld", p->name,
                                               (long long)p->address, (long long)p->size, owner(a, p),
                                               (long long)p->order));
            shsm_output_linez(out, "  no retail counterpart");
            shsm_output_data(out, "function", avp_json_null());
            shsm_output_data(out, "counterpart", fn_json(a, p, true));
            return 1;
        }
        i = p->peer;
    }

    const SibFn *f = &s->v[i];
    shsm_output_linez(out, avp_sprintf(a, "%s 0x%08llX size 0x%llX (%s, %s)", f->name, (long long)f->address,
                                       (long long)f->size, f->lib ? f->lib : "unknown",
                                       f->object ? f->object : "?"));
    AvpJson *data = fn_json(a, f, false);
    shsm_output_data(out, "function", data);
    if (f->peer < 0) {
        shsm_output_linez(out, "  no counterpart");
        shsm_output_data(out, "counterpart", avp_json_null());
        size_t nsame;
        i64 same = sibling_same_named(g, f->name, &nsame);
        const char *note = sibling_same_named_note(a, f, g);
        if (note) shsm_output_linez(out, avp_sprintf(a, "  %s", note));
        shsm_output_data(out, "same_named", same >= 0 && nsame == 1 ? fn_json(a, &g->v[same], true) : avp_json_null());
        return 1;
    }
    const SibFn *p = &g->v[f->peer];
    shsm_output_linez(out, avp_sprintf(a, "  %s -> %s", SIBLING_TIERS[f->tier], demangled(a, p->name)));
    shsm_output_linez(out, avp_sprintf(a, "     %s at 0x%08llX size 0x%llX, %s #%lld", p->name, (long long)p->address,
                                       (long long)p->size, owner(a, p), (long long)p->order));
    shsm_output_linez(out, avp_sprintf(a, "     names: %s", sibling_name_agreement(a, f->name, p->name)));
    shsm_output_data(out, "counterpart", fn_json(a, p, true));
    AvpJson *nb = avp_json_arr(a);
    if (p->object) {
        size_t lo = (size_t)f->peer, hi = (size_t)f->peer;
        while (lo > 0 && g->v[lo - 1].object && strcmp(g->v[lo - 1].object, p->object) == 0) lo--;
        while (hi + 1 < g->n && g->v[hi + 1].object && strcmp(g->v[hi + 1].object, p->object) == 0) hi++;
        size_t from = lo, to = hi;
        if (hi - lo > 8) {
            from = (size_t)f->peer > lo + 4 ? (size_t)f->peer - 4 : lo;
            to = (size_t)f->peer + 4 < hi ? (size_t)f->peer + 4 : hi;
        }
        shsm_output_linez(out, avp_sprintf(a, "  object %s: %zu functions (showing #%lld..#%lld)", owner(a, p),
                                           hi - lo + 1, (long long)g->v[from].order, (long long)g->v[to].order));
        for (size_t j = from; j <= to; j++) {
            const SibFn *n = &g->v[j];
            const char *peer = n->peer >= 0 ? avp_sprintf(a, "0x%08llX %s (%s)", (long long)s->v[n->peer].address,
                                                          s->v[n->peer].name, SIBLING_TIERS[n->tier])
                                            : "unpaired";
            shsm_output_linez(out, avp_sprintf(a, "  %s #%-3lld %-50s %s", j == (size_t)f->peer ? ">" : " ",
                                               (long long)n->order, demangled(a, n->name), peer));
            AvpJson *o = fn_json(a, n, true);
            if (n->peer >= 0) avp_json_set(a, o, AVP_S("retail"), fn_json(a, &s->v[n->peer], false));
            avp_json_arr_push(a, nb, o);
        }
    }
    shsm_output_data(out, "neighbours", nb);
    return 0;
}

/* ---- names ---- */

int sibling_cmd_names(Sibling *sb, const SibFnVec *s, const SibFnVec *g, const SibArgs *args, ShsmOutput *out) {
    (void)sb;
    AvpArena *a = avp_arena();
    AvpMap *used = avp_map_new();
    for (size_t i = 0; i < s->n; i++)
        if (s->v[i].name) avp_map_put(used, avp_str(s->v[i].name), NULL)->i = 1;
    i64 counts[4] = {0}; /* placeholder, same, other, taken */
    AvpJson *arr = avp_json_arr(a);
    AvpStrVec lines = {0};
    for (size_t i = 0; i < s->n; i++) {
        const SibFn *f = &s->v[i];
        if (f->peer < 0 || !tier_ok(args, f->tier) || !lib_ok(a, f, args->lib)) continue;
        const SibFn *p = &g->v[f->peer];
        const char *status;
        const char *agree = sibling_name_agreement(a, f->name, p->name);
        if (strcmp(agree, "same") == 0) status = "same", counts[1]++;
        else if (sibling_is_placeholder(f->name)) {
            if (avp_map_has(used, avp_str(p->name))) status = "taken", counts[3]++;
            else status = "placeholder", counts[0]++;
        } else status = strcmp(agree, "same-name") == 0 ? "other-signature" : "conflict", counts[2]++;
        if (strcmp(status, "same") == 0) continue;
        avp_vec_push(&lines, avp_str(avp_sprintf(a, "0x%08llX %-15s %-9s %s -> %s", (long long)f->address, status,
                                                 SIBLING_TIERS[f->tier], f->name, p->name)));
        if (strcmp(status, "placeholder") == 0) {
            const char *thunk = sibling_thunk_note(a, p->name);
            if (thunk) /* the compiler's thunk has no source spelling: not renamed */
                avp_vec_push(&lines, avp_str(avp_sprintf(a, "    not renamed: %s", thunk)));
            else
                avp_vec_push(&lines, avp_str(avp_sprintf(a, "    build/tools/names.exe rename %s %s%s", f->name, p->name,
                                                         sibling_is_ctor_dtor(a, p->name) ? " --symbols-only" : "")));
        }
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("address"), avp_json_strz(a, avp_sprintf(a, "0x%08llX", (long long)f->address)));
        avp_json_set(a, o, AVP_S("status"), avp_json_strz(a, status));
        avp_json_set(a, o, AVP_S("tier"), avp_json_strz(a, SIBLING_TIERS[f->tier]));
        avp_json_set(a, o, AVP_S("current"), avp_json_strz(a, f->name));
        avp_json_set(a, o, AVP_S("proposed"), avp_json_strz(a, p->name));
        avp_json_set(a, o, AVP_S("demangled"), avp_json_strz(a, demangled(a, p->name)));
        avp_json_set(a, o, AVP_S("object"), avp_json_strz(a, owner(a, p)));
        avp_json_arr_push(a, arr, o);
    }
    shsm_output_linez(out, avp_sprintf(a, "proposed renames: %lld placeholder, %lld taken (the sibling name is already"
                                          " a retail symbol), %lld already the same, %lld different real name"
                                          " (conflict or other signature); nothing applied",
                                       (long long)counts[0], (long long)counts[3], (long long)counts[1],
                                       (long long)counts[2]));
    for (size_t k = 0; k < lines.n; k++) shsm_output_line(out, lines.v[k]);
    shsm_output_data(out, "renames", arr);
    avp_vec_free(&lines);
    avp_map_free(used);
    return 0;
}

/* ---- units ---- */

typedef struct {
    const char *lib, *object;
    AVP_VEC(size_t) members; /* retail indices, address order */
    i64 start, end;          /* retail span */
    i64 inside_unpaired, inside_other;
    bool ordered;
    int split_agreement; /* 0 none, 1 exact, 2 inside one unit, 3 crosses units */
    const char *split_unit;
} ObjSpan;
typedef AVP_VEC(ObjSpan *) ObjSpanVec;

static const char *suggest_path(AvpArena *a, const ObjSpan *o, const SibFn *first) {
    const char *obj = o->object;
    const char *b1 = strrchr(obj, '/'), *b2 = strrchr(obj, '\\');
    const char *base = b1 > b2 ? b1 + 1 : (b2 ? b2 + 1 : obj);
    const char *dot = strrchr(base, '.');
    AvpStr stem = avp_strn(base, dot ? (size_t)(dot - base) : strlen(base));
    const char *dir;
    if (o->lib && *o->lib) {
        size_t n = strlen(o->lib);
        dir = avp_cz(a, avp_strn(o->lib, n > 2 && strcmp(o->lib + n - 2, ".a") == 0 ? n - 2 : n));
    } else {
        dir = first->lib ? first->lib : "unknown";
    }
    return avp_sprintf(a, "%s/%s", dir, avp_cz(a, stem));
}

static int span_cmp(const void *x, const void *y) {
    const ObjSpan *a = *(ObjSpan *const *)x, *b = *(ObjSpan *const *)y;
    return a->start < b->start ? -1 : a->start > b->start;
}

/* Objects with paired members (selected tiers); their spans, ordering and agreement with the split units. */
static ObjSpanVec object_spans(AvpArena *a, Sibling *sb, const SibFnVec *s, const SibFnVec *g,
                                        const SibArgs *args) {
    ObjSpanVec objs = {0};
    AvpMap *m = avp_map_new();
    for (size_t i = 0; i < s->n; i++) {
        const SibFn *f = &s->v[i];
        if (f->peer < 0 || !tier_ok(args, f->tier)) continue;
        const SibFn *p = &g->v[f->peer];
        if (!p->object) continue;
        bool created;
        AvpVal *v = avp_map_put(m, avp_str(avp_sprintf(a, "%s|%s", p->lib ? p->lib : "", p->object)), &created);
        if (created) {
            ObjSpan *o = avp_alloc(a, sizeof *o);
            o->lib = p->lib ? p->lib : "";
            o->object = p->object;
            v->p = o;
            avp_vec_push(&objs, o);
        }
        avp_vec_push(&((ObjSpan *)v->p)->members, i);
    }
    avp_map_free(m);
    const ShsmSplits *sp = shsm_load_splits(sb->repo);
    for (size_t k = 0; k < objs.n; k++) {
        ObjSpan *o = objs.v[k];
        size_t i0 = o->members.v[0], i1 = o->members.v[o->members.n - 1];
        o->start = s->v[i0].address;
        o->end = s->v[i1].address + s->v[i1].size;
        o->ordered = true;
        for (size_t x = 1; x < o->members.n; x++)
            if (s->v[o->members.v[x]].peer < s->v[o->members.v[x - 1]].peer) o->ordered = false;
        for (size_t i = i0; i <= i1; i++) {
            const SibFn *f = &s->v[i];
            if (f->peer < 0 || !tier_ok(args, f->tier)) o->inside_unpaired++;
            else {
                const SibFn *p = &g->v[f->peer];
                if (!p->object || strcmp(p->object, o->object) != 0 || strcmp(p->lib ? p->lib : "", o->lib) != 0)
                    o->inside_other++;
            }
        }
        const ShsmSpan *u0 = shsm_splits_range_of(sp, (u64)o->start), *u1 = shsm_splits_range_of(sp, (u64)o->end - 1);
        if (u0 && u1 && u0->unit && u1->unit && strcmp(u0->unit, u1->unit) == 0 && !strncmp(u0->section, ".text", 5)) {
            o->split_unit = u0->unit;
            o->split_agreement = (u64)o->start == u0->start && (u64)o->end == u0->end ? 1 : 2;
        } else if (u0 || u1) {
            o->split_agreement = 3;
            o->split_unit = u0 ? u0->unit : u1->unit;
        }
    }
    qsort(objs.v, objs.n, sizeof *objs.v, span_cmp);
    return objs;
}

static bool span_overlaps(const ObjSpanVec *objs, size_t k) {
    const ObjSpan *o = objs->v[k];
    return (k > 0 && objs->v[k - 1]->end > o->start) || (k + 1 < objs->n && objs->v[k + 1]->start < o->end);
}

/* clean: ordered like the sibling's object, no other object's function inside, no overlap with a neighbour span */
static bool span_clean(const ObjSpanVec *objs, size_t k) {
    const ObjSpan *o = objs->v[k];
    return o->ordered && !o->inside_other && !span_overlaps(objs, k);
}

bool *sibling_clean_flags(AvpArena *a, Sibling *sb, const SibFnVec *s, const SibFnVec *g, const SibArgs *args) {
    ObjSpanVec objs = object_spans(a, sb, s, g, args);
    bool *flags = avp_alloc(a, (s->n + 1) * sizeof *flags);
    for (size_t k = 0; k < objs.n; k++)
        if (span_clean(&objs, k))
            for (size_t x = 0; x < objs.v[k]->members.n; x++) flags[objs.v[k]->members.v[x]] = true;
    return flags;
}

static SibProposal *span_proposals(AvpArena *a, Sibling *sb, const SibFnVec *s, const SibFnVec *g,
                                   const SibArgs *args, const ObjSpanVec *objs, bool with_splits);

SibLookupSpan *sibling_span_list(AvpArena *a, Sibling *sb, const SibFnVec *s, const SibFnVec *g, const SibArgs *args,
                                 size_t *n) {
    ObjSpanVec objs = object_spans(a, sb, s, g, args);
    SibLookupSpan *out = avp_alloc(a, (objs.n + 1) * sizeof *out);
    SibProposal *props = objs.n ? span_proposals(a, sb, s, g, args, &objs, true) : NULL;
    for (size_t k = 0; k < objs.n; k++) {
        out[k].proposed = props[k].proposed;
        out[k].prop_start = props[k].start;
        out[k].prop_end = props[k].end;
        out[k].prop_pad_end = props[k].pad_end;
        const ObjSpan *o = objs.v[k];
        out[k].lib = o->lib;
        out[k].object = o->object;
        out[k].start = o->start;
        out[k].end = o->end;
        out[k].paired = o->members.n;
        out[k].clean = span_clean(&objs, k);
        AvpStrVec unused = sibling_unused_of(a, sb, o->lib, o->object);
        out[k].unused = (i64)unused.n;
        avp_vec_free(&unused);
    }
    *n = objs.n;
    return out;
}

/* The proposed unit ranges of the spans (sibling_units.c); with_splits false leaves the split units out of the
 * evidence (the ground-truth measure of stats). */
static SibProposal *span_proposals(AvpArena *a, Sibling *sb, const SibFnVec *s, const SibFnVec *g,
                                   const SibArgs *args, const ObjSpanVec *objs, bool with_splits) {
    SibSpanRef *refs = avp_alloc(a, (objs->n + 1) * sizeof *refs);
    for (size_t k = 0; k < objs->n; k++) {
        const ObjSpan *o = objs->v[k];
        refs[k] = (SibSpanRef){o->lib, o->object, o->members.v[0], o->members.v[o->members.n - 1],
                               span_clean(objs, k)};
    }
    SibMapSymVec unused = sibling_unused_all(a, sb);
    SibProposal *p = sibling_propose_units(a, s, g, &unused, with_splits ? shsm_load_splits(sb->repo) : NULL, refs,
                                           objs->n, args->tiers);
    avp_vec_free(&unused);
    return p;
}

static AvpJson *gap_json(AvpArena *a, const SibGapFnVec *v) {
    AvpJson *arr = avp_json_arr(a);
    for (size_t x = 0; x < v->n; x++) {
        const SibGapFn *f = &v->v[x];
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("address"), avp_json_strz(a, avp_sprintf(a, "0x%08llX", (long long)f->address)));
        avp_json_set(a, o, AVP_S("size"), avp_json_int(a, f->size));
        avp_json_set(a, o, AVP_S("name"), f->name ? avp_json_strz(a, f->name) : avp_json_null());
        avp_json_set(a, o, AVP_S("taken"), avp_json_bool(f->mine));
        avp_json_set(a, o, AVP_S("status"), avp_json_strz(a, f->assigned ? "assigned" : f->candidate ? "candidate" : "ambiguous"));
        avp_json_set(a, o, AVP_S("why"), f->why ? avp_json_strz(a, f->why) : avp_json_null());
        avp_json_arr_push(a, arr, o);
    }
    return arr;
}

static void gap_lines(AvpArena *a, const SibGapFnVec *v, AvpStrVec *lines) {
    for (size_t x = 0; x < v->n; x++) {
        const SibGapFn *f = &v->v[x];
        avp_vec_push(lines, avp_str(avp_sprintf(a, "      %s 0x%08llX size 0x%-5llX %-34s %s%s%s", f->mine ? "+" : "?",
                                                (long long)f->address, (long long)f->size, f->name ? f->name : "?",
                                                f->assigned ? "assigned (" : "", f->why ? f->why : "",
                                                f->assigned ? ")" : f->candidate ? "; left out" : (f->mine ? "; taken" : "; left out"))));
    }
}

int sibling_cmd_units(Sibling *sb, const SibFnVec *s, const SibFnVec *g, const SibArgs *args, ShsmOutput *out) {
    AvpArena *a = avp_arena();
    ObjSpanVec objs = object_spans(a, sb, s, g, args);
    SibProposal *props = span_proposals(a, sb, s, g, args, &objs, !args->no_splits);
    const ShsmSplits *sp = shsm_load_splits(sb->repo);
    AvpJson *arr = avp_json_arr(a);
    AvpStrVec lines = {0};
    i64 clean = 0, clean_fns = 0, shown = 0, agree[4] = {0};
    for (size_t k = 0; k < objs.n; k++) {
        ObjSpan *o = objs.v[k];
        const SibFn *first = &s->v[o->members.v[0]];
        if (!lib_ok(a, first, args->lib)) continue;
        shown++;
        bool overlaps = span_overlaps(&objs, k);
        bool is_clean = span_clean(&objs, k);
        if (is_clean) clean++, clean_fns += (i64)o->members.n;
        agree[o->split_agreement]++;
        AvpStrVec unused = sibling_unused_of(a, sb, o->lib, o->object);
        const char *path = suggest_path(a, o, first);
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_printf(&b, "0x%08llX-0x%08llX %-34s %zu paired, %lld unpaired inside", (long long)o->start,
                       (long long)o->end, path, o->members.n, (long long)o->inside_unpaired);
        if (o->inside_other) avp_buf_printf(&b, ", %lld of other objects (interleaved)", (long long)o->inside_other);
        avp_buf_printf(&b, ", %s%s", o->ordered ? "ordered" : "ORDER DIFFERS", overlaps ? ", OVERLAPS" : "");
        static const char *const ag[] = {"no split unit", "= split unit", "inside split unit", "crosses split units"};
        avp_buf_printf(&b, "; %s%s%s", ag[o->split_agreement], o->split_unit ? " " : "",
                       o->split_unit ? o->split_unit : "");
        avp_buf_printf(&b, " [%s%s%s]", o->lib, *o->lib ? " " : "", o->object);
        avp_vec_push(&lines, avp_str(avp_buf_cz(&b)));
        if (unused.n) {
            AvpBuf u;
            avp_buf_init(&u, a);
            avp_buf_printf(&u, "    UNUSED in the sibling (%zu):", unused.n);
            for (size_t x = 0; x < unused.n && x < 6; x++) {
                avp_buf_putc(&u, ' ');
                avp_buf_put(&u, unused.v[x]);
            }
            if (unused.n > 6) avp_buf_printf(&u, " ...");
            avp_vec_push(&lines, avp_str(avp_buf_cz(&u)));
        }
        const SibProposal *pr = &props[k];
        const char *unit_line = NULL;
        if (!pr->proposed) {
            avp_vec_push(&lines, AVP_S("    proposed unit range: none (the span is not clean)"));
        } else {
            size_t amb = 0, taken = 0, cand = 0;
            const SibGapFnVec *both[2] = {&pr->before, &pr->after};
            for (int w = 0; w < 2; w++)
                for (size_t x = 0; x < both[w]->n; x++) amb += !both[w]->v[x].assigned && !both[w]->v[x].candidate, cand += both[w]->v[x].candidate, taken += both[w]->v[x].mine;
            avp_vec_push(&lines, avp_str(avp_sprintf(a, "    proposed unit range 0x%08llX-0x%08llX (%zu gap functions"
                                                        " taken, %zu ambiguous, %zu candidate%s)%s",
                                                     (long long)pr->start, (long long)pr->end, taken, amb, cand, cand == 1 ? "" : "s",
                                                     pr->start == o->start && pr->end == o->end ? " = the span" : "")));
            gap_lines(a, &pr->before, &lines);
            gap_lines(a, &pr->after, &lines);
            const ShsmSpan *u = shsm_splits_range_of(sp, (u64)pr->start);
            if (pr->pad_end != pr->end)
                avp_vec_push(&lines, avp_str(avp_sprintf(a, "    alignment padding to 0x%08llX follows: end the unit"
                                                            " there if the neighbours' splits include theirs",
                                                         (long long)pr->pad_end)));
            if (u && u->unit && (u64)pr->start == u->start && ((u64)pr->end == u->end || (u64)pr->pad_end == u->end))
                avp_vec_push(&lines, avp_str(avp_sprintf(a, "    = split unit %s", u->unit)));
            else {
                unit_line = avp_sprintf(a, "build/tools/unit.exe new %s --text 0x%08llX-0x%08llX", path,
                                        (long long)pr->start, (long long)pr->end);
                avp_vec_push(&lines, avp_str(avp_sprintf(a, "    %s%s", unit_line,
                                                         o->split_agreement ? "   (a split unit already covers part)"
                                                                            : "")));
            }
        }
        AvpJson *j = avp_json_obj(a);
        AvpJson *pj = avp_json_obj(a);
        avp_json_set(a, pj, AVP_S("proposed"), avp_json_bool(pr->proposed));
        avp_json_set(a, pj, AVP_S("start"), avp_json_strz(a, avp_sprintf(a, "0x%08llX", (long long)pr->start)));
        avp_json_set(a, pj, AVP_S("end"), avp_json_strz(a, avp_sprintf(a, "0x%08llX", (long long)pr->end)));
        avp_json_set(a, pj, AVP_S("before"), gap_json(a, &pr->before));
        avp_json_set(a, pj, AVP_S("after"), gap_json(a, &pr->after));
        avp_json_set(a, pj, AVP_S("unit_new"), unit_line ? avp_json_strz(a, unit_line) : avp_json_null());
        avp_json_set(a, j, AVP_S("proposal"), pj);
        avp_json_set(a, j, AVP_S("start"), avp_json_strz(a, avp_sprintf(a, "0x%08llX", (long long)o->start)));
        avp_json_set(a, j, AVP_S("end"), avp_json_strz(a, avp_sprintf(a, "0x%08llX", (long long)o->end)));
        avp_json_set(a, j, AVP_S("path"), avp_json_strz(a, path));
        avp_json_set(a, j, AVP_S("lib"), avp_json_strz(a, o->lib));
        avp_json_set(a, j, AVP_S("object"), avp_json_strz(a, o->object));
        avp_json_set(a, j, AVP_S("paired"), avp_json_int(a, (i64)o->members.n));
        avp_json_set(a, j, AVP_S("unpaired_inside"), avp_json_int(a, o->inside_unpaired));
        avp_json_set(a, j, AVP_S("other_inside"), avp_json_int(a, o->inside_other));
        avp_json_set(a, j, AVP_S("ordered"), avp_json_bool(o->ordered));
        avp_json_set(a, j, AVP_S("overlaps"), avp_json_bool(overlaps));
        avp_json_set(a, j, AVP_S("split_agreement"), avp_json_strz(a, ag[o->split_agreement]));
        avp_json_set(a, j, AVP_S("split_unit"), o->split_unit ? avp_json_strz(a, o->split_unit) : avp_json_null());
        AvpJson *uj = avp_json_arr(a);
        for (size_t x = 0; x < unused.n; x++) avp_json_arr_push(a, uj, avp_json_str(a, unused.v[x]));
        avp_json_set(a, j, AVP_S("unused"), uj);
        avp_json_arr_push(a, arr, j);
        avp_vec_free(&unused);
    }
    shsm_output_linez(out, avp_sprintf(a, "%lld sibling objects with paired functions: %lld clean (ordered, no other"
                                          " object inside, no overlap) covering %lld paired functions; split units:"
                                          " %lld equal, %lld inside one, %lld crossing, %lld outside any",
                                       (long long)shown, (long long)clean, (long long)clean_fns, (long long)agree[1],
                                       (long long)agree[2], (long long)agree[3], (long long)agree[0]));
    for (size_t k = 0; k < lines.n; k++) shsm_output_line(out, lines.v[k]);
    shsm_output_data(out, "objects", arr);
    avp_vec_free(&lines);
    return 0;
}

/* ---- stats ---- */

int sibling_cmd_stats(Sibling *sb, const SibFnVec *s, const SibFnVec *g, const SibArgs *args, ShsmOutput *out) {
    AvpArena *a = avp_arena();
    size_t n;
    LibRow *rows = lib_rows(a, s, args, &n);
    AvpJson *libs = avp_json_arr(a);
    shsm_output_linez(out, avp_sprintf(a, "pairing with %s (%zu sibling functions)", sb->name, g->n));
    print_lib_rows(a, rows, n, out, libs);
    shsm_output_data(out, "libraries", libs);
    /* precision on retail functions that carry a real name */
    i64 named[SIB_NTIERS] = {0}, same[SIB_NTIERS] = {0}, sig[SIB_NTIERS] = {0};
    AvpStrVec dis = {0};
    AvpJson *disj = avp_json_arr(a);
    for (int t = 1; t < SIB_NTIERS; t++)
        for (size_t i = 0; i < s->n; i++) {
            const SibFn *f = &s->v[i];
            if (f->tier != t || sibling_is_placeholder(f->name) || !lib_ok(a, f, args->lib)) continue;
            const SibFn *p = &g->v[f->peer];
            const char *ag = sibling_name_agreement(a, f->name, p->name);
            named[t]++;
            if (strcmp(ag, "same") == 0) same[t]++;
            else if (strcmp(ag, "same-name") == 0) sig[t]++;
            else {
                avp_vec_push(&dis, avp_str(avp_sprintf(a, "  %-9s 0x%08llX %-10s %s != %s [%s]", SIBLING_TIERS[t],
                                                       (long long)f->address, f->lib, f->name, p->name,
                                                       owner(a, p))));
                AvpJson *o = avp_json_obj(a);
                avp_json_set(a, o, AVP_S("tier"), avp_json_strz(a, SIBLING_TIERS[t]));
                avp_json_set(a, o, AVP_S("retail"), fn_json(a, f, false));
                avp_json_set(a, o, AVP_S("sibling"), fn_json(a, p, true));
                avp_json_arr_push(a, disj, o);
            }
        }
    shsm_output_linez(out, "precision on named retail functions (same = identical name; same-name = same qualified"
                           " name, other signature):");
    AvpJson *prec = avp_json_obj(a);
    for (int t = 1; t < SIB_NTIERS; t++) {
        double p = named[t] ? 100.0 * (double)(same[t] + sig[t]) / (double)named[t] : 0.0;
        double ps = named[t] ? 100.0 * (double)same[t] / (double)named[t] : 0.0;
        shsm_output_linez(out, avp_sprintf(a, "  %-9s named %6lld  same %6lld  same-name %4lld  differ %5lld  "
                                              "precision %.2f%% (strict %.2f%%)%s",
                                           SIBLING_TIERS[t], (long long)named[t], (long long)same[t],
                                           (long long)sig[t], (long long)(named[t] - same[t] - sig[t]), p, ps,
                                           named[t] && p < 98.0 ? "  BELOW 98%" : ""));
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("named"), avp_json_int(a, named[t]));
        avp_json_set(a, o, AVP_S("same"), avp_json_int(a, same[t]));
        avp_json_set(a, o, AVP_S("same_name"), avp_json_int(a, sig[t]));
        avp_json_set(a, o, AVP_S("differ"), avp_json_int(a, named[t] - same[t] - sig[t]));
        avp_json_set(a, prec, avp_str(SIBLING_TIERS[t]), o);
    }
    shsm_output_data(out, "precision", prec);
    /* unit boundaries against the split units (tiers selected) */
    ObjSpanVec objs = object_spans(a, sb, s, g, args);
    i64 agree[4] = {0};
    for (size_t k = 0; k < objs.n; k++)
        if (lib_ok(a, &s->v[objs.v[k]->members.v[0]], args->lib)) agree[objs.v[k]->split_agreement]++;
    shsm_output_linez(out, avp_sprintf(a, "unit spans vs split units: %lld equal, %lld inside one unit, %lld crossing"
                                          " units, %lld in unsplit code",
                                       (long long)agree[1], (long long)agree[2], (long long)agree[3],
                                       (long long)agree[0]));
    AvpJson *uj = avp_json_obj(a);
    avp_json_set(a, uj, AVP_S("equal"), avp_json_int(a, agree[1]));
    avp_json_set(a, uj, AVP_S("inside"), avp_json_int(a, agree[2]));
    avp_json_set(a, uj, AVP_S("crossing"), avp_json_int(a, agree[3]));
    avp_json_set(a, uj, AVP_S("unsplit"), avp_json_int(a, agree[0]));
    shsm_output_data(out, "units", uj);
    /* the proposed unit ranges on ground truth: split .text units that hold one clean span and no paired function of
     * another object, the proposals computed without the split units as evidence */
    SibProposal *gt = span_proposals(a, sb, s, g, args, &objs, false);
    const ShsmSplits *sp = shsm_load_splits(sb->repo);
    i64 ntruth = 0, equal = 0, padded = 0, wide[2] = {0}, narrow[2] = {0};
    AvpStrVec miss = {0};
    for (size_t k = 0; k < objs.n; k++) {
        const ObjSpan *o = objs.v[k];
        if (!lib_ok(a, &s->v[o->members.v[0]], args->lib) || !span_clean(&objs, k)) continue;
        if (o->split_agreement != 1 && o->split_agreement != 2) continue;
        const ShsmSpan *u = shsm_splits_range_of(sp, (u64)o->start);
        if (!u || !u->unit) continue;
        bool pure = true;
        size_t i0 = o->members.v[0], i1 = o->members.v[o->members.n - 1];
        while (i0 > 0 && (u64)s->v[i0 - 1].address >= u->start) i0--;
        while (i1 + 1 < s->n && (u64)s->v[i1 + 1].address < u->end) i1++;
        for (size_t i = i0; i <= i1 && pure; i++) {
            const SibFn *f = &s->v[i];
            if (f->peer < 0 || !tier_ok(args, f->tier)) continue;
            const SibFn *p = &g->v[f->peer];
            if (!p->object || strcmp(p->object, o->object) || strcmp(p->lib ? p->lib : "", o->lib)) pure = false;
        }
        if (!pure) continue;
        ntruth++;
        i64 ps = gt[k].start, pe = gt[k].end;
        if ((u64)ps == u->start && (u64)pe == u->end) {
            equal++;
            continue;
        }
        if ((u64)ps == u->start && (u64)gt[k].pad_end == u->end) {
            padded++;
            continue;
        }
        if ((u64)ps < u->start) wide[0]++;
        if ((u64)ps > u->start) narrow[0]++;
        if ((u64)pe > u->end) wide[1]++;
        if ((u64)pe < u->end) narrow[1]++;
        avp_vec_push(&miss, avp_str(avp_sprintf(a, "  0x%08llX-0x%08llX proposed, split 0x%08llX-0x%08llX %s [%s%s%s]",
                                                (long long)ps, (long long)pe, (unsigned long long)u->start,
                                                (unsigned long long)u->end, u->unit, o->lib, *o->lib ? " " : "",
                                                o->object)));
    }
    shsm_output_linez(out, avp_sprintf(a, "proposed unit ranges vs split units holding one sibling object (split units"
                                          " not used as evidence): %lld of %lld equal, %lld more up to the trailing"
                                          " alignment padding; misses: start too early %lld, too late %lld; end too"
                                          " late %lld, too early %lld",
                                       (long long)equal, (long long)ntruth, (long long)padded, (long long)wide[0],
                                       (long long)narrow[0], (long long)wide[1], (long long)narrow[1]));
    AvpJson *gj = avp_json_obj(a);
    avp_json_set(a, gj, AVP_S("units"), avp_json_int(a, ntruth));
    avp_json_set(a, gj, AVP_S("equal"), avp_json_int(a, equal));
    avp_json_set(a, gj, AVP_S("equal_with_padding"), avp_json_int(a, padded));
    AvpJson *mj = avp_json_arr(a);
    for (size_t k = 0; k < miss.n; k++) avp_json_arr_push(a, mj, avp_json_str(a, miss.v[k]));
    avp_json_set(a, gj, AVP_S("misses"), mj);
    shsm_output_data(out, "proposed_ranges", gj);
    if (args->all)
        for (size_t k = 0; k < miss.n; k++) shsm_output_line(out, miss.v[k]);
    avp_vec_free(&miss);
    shsm_output_linez(out, avp_sprintf(a, "disagreements on named functions (%zu):", dis.n));
    for (size_t k = 0; k < dis.n; k++) shsm_output_line(out, dis.v[k]);
    shsm_output_data(out, "disagreements", disj);
    avp_vec_free(&dis);
    return 0;
}
