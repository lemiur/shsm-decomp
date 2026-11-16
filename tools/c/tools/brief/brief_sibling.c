/* brief: the sibling (Gormiti) counterpart of a function and the sibling object spans of a range. The data is read
 * from the sibling database when `sibling.exe pair` has built it; without one nothing is printed and nothing fails. */
#include "brief.h"
#include "sibling_lookup.h"

static AvpArena *A(void) { return avp_arena(); }

static SibLookup *lookup(BriefWorld *W) {
    static const BriefWorld *opened_for;
    static SibLookup *lk;
    if (opened_for != W) {
        opened_for = W;
        lk = sibling_lookup_open(A(), W->repo);
    }
    return lk;
}

AvpJson *brief_sibling_fn(BriefWorld *W, i64 address, const char *name, i64 size) {
    AvpArena *a = A();
    SibLookup *lk = lookup(W);
    SibLookupFn f;
    if (!lk) return avp_json_null();
    if (!sibling_lookup_fn(lk, address, &f)) {
        SibLookupSame same;
        if (!sibling_lookup_same_named(lk, name, &same)) return avp_json_null();
        AvpJson *d = avp_json_obj(a);
        brief_set(d, "same_named", brief_jb(true));
        brief_set(d, "name", brief_jz(same.name));
        brief_set(d, "demangled", brief_jz(same.demangled));
        brief_set(d, "lib", brief_jz(same.lib));
        brief_set(d, "object", brief_jz(same.object));
        brief_set(d, "order", avp_json_int(a, same.order));
        brief_set(d, "size", avp_json_int(a, same.size));
        brief_set(d, "retail_size", brief_ji(size));
        brief_set(d, "count", avp_json_int(a, (i64)same.count));
        return d;
    }
    AvpJson *d = avp_json_obj(a);
    brief_set(d, "name", brief_jz(f.name));
    brief_set(d, "demangled", brief_jz(f.demangled));
    brief_set(d, "tier", brief_jz(f.tier));
    brief_set(d, "lib", brief_jz(f.lib));
    brief_set(d, "object", brief_jz(f.object));
    brief_set(d, "order", avp_json_int(a, f.order));
    return d;
}

AvpJson *brief_sibling_range(BriefWorld *W, i64 start, i64 end) {
    AvpArena *a = A();
    SibLookup *lk = lookup(W);
    if (!lk) return avp_json_null();
    SibLookupSpanVec v = sibling_lookup_range(lk, start, end);
    if (!v.n) return avp_json_null();
    AvpJson *arr = avp_json_arr(a);
    for (size_t k = 0; k < v.n; k++) {
        const SibLookupSpan *sp = &v.v[k];
        bool equals_proposal = sp->proposed && sp->prop_start == start && (sp->prop_end == end || sp->prop_pad_end == end);
        const char *rel = sp->start == start && sp->end == end ? "equals"
                          : equals_proposal                    ? "equals-proposed"
                          : sp->start >= start && sp->end <= end ? "inside"
                          : sp->start <= start && sp->end >= end ? "covers"
                                                                 : "partly";
        AvpJson *o = avp_json_obj(a);
        brief_set(o, "lib", brief_jz(sp->lib));
        brief_set(o, "object", brief_jz(sp->object));
        brief_set(o, "start", avp_json_int(a, sp->start));
        brief_set(o, "end", avp_json_int(a, sp->end));
        brief_set(o, "paired", avp_json_int(a, (i64)sp->paired));
        brief_set(o, "clean", brief_jb(sp->clean));
        brief_set(o, "unused", avp_json_int(a, sp->unused));
        brief_set(o, "relation", brief_jz(rel));
        brief_set(o, "proposed", brief_jb(sp->proposed));
        brief_set(o, "prop_start", avp_json_int(a, sp->prop_start));
        brief_set(o, "prop_end", avp_json_int(a, sp->prop_end));
        brief_set(o, "proposal_is_span", brief_jb(sp->proposed && sp->prop_start == sp->start && sp->prop_end == sp->end));
        avp_json_arr_push(a, arr, o);
    }
    avp_vec_free(&v);
    return arr;
}

void brief_sibling_fn_lines(BriefLines *L, const AvpJson *d) {
    if (!brief_truthy(d)) return;
    const char *lib = brief_py_strz(avp_json_getz(d, "lib"));
    if (brief_truthy(avp_json_getz(d, "same_named"))) {
        i64 count = avp_json_get_i64(d, "count", 1);
        const char *name = brief_py_strz(avp_json_getz(d, "name"));
        brief_addf(L, "  sibling (Gormiti): no exact pair; %s (size 0x%X vs 0x%X, %s%s%s #%d):"
                      " build/tools/sibling.exe fn '%s' --sibling",
                   count > 1 ? avp_cz(A(), avp_pyfmt(A(), "%d same-named functions exist, the first", count))
                             : "same-named function exists",
                   avp_json_get_i64(d, "size", 0), avp_json_get_i64(d, "retail_size", 0), lib, *lib ? " " : "",
                   brief_py_strz(avp_json_getz(d, "object")), avp_json_get_i64(d, "order", 0), name);
        return;
    }
    brief_addf(L, "  sibling (Gormiti): %s  %s, %s%s%s #%d  (build/tools/sibling.exe of)",
               brief_py_strz(avp_json_getz(d, "demangled")), brief_py_strz(avp_json_getz(d, "tier")),
               lib, *lib ? " " : "", brief_py_strz(avp_json_getz(d, "object")),
               avp_json_get_i64(d, "order", 0));
}

void brief_sibling_range_lines(BriefLines *L, const AvpJson *d) {
    if (!brief_truthy(d)) return;
    size_t n = avp_json_len(d);
    brief_addf(L, "  sibling link map (Gormiti): %d object span(s) overlap this range", (i64)n);
    for (size_t k = 0; k < n && k < 3; k++) {
        const AvpJson *o = avp_json_at(d, k);
        const char *lib = brief_py_strz(avp_json_getz(o, "lib"));
        AvpStr rel = brief_jstr(o, "relation");
        bool equals = avp_str_eqz(rel, "equals"), clean = brief_truthy(avp_json_getz(o, "clean"));
        bool proposed = brief_truthy(avp_json_getz(o, "proposed"));
        bool same = brief_truthy(avp_json_getz(o, "proposal_is_span"));
        /* `sibling.exe units` proposes a unit range for a clean span: the range of the unit is it, or only the span
         * of the paired functions (then the proposal is printed) */
        const char *what = avp_str_eqz(rel, "equals-proposed") ? "= proposed unit range (sibling.exe units)"
                           : equals && clean && proposed && same ? "= clean span = proposed unit range (sibling.exe units)"
                           : equals && clean && proposed
                               ? avp_cz(A(), avp_pyfmt(A(), "= clean span; proposed unit range %s-%s (sibling.exe units)",
                                                       brief_hex(avp_json_get_i64(o, "prop_start", 0)),
                                                       brief_hex(avp_json_get_i64(o, "prop_end", 0))))
                           : equals && clean ? "= clean span"
                           : equals        ? "= span, not clean"
                           : avp_str_eqz(rel, "inside") ? "inside the range"
                           : avp_str_eqz(rel, "covers") ? "covers the range and more"
                                                        : "partly in the range";
        brief_addf(L, "    %s%s%s  %s-%s  %d paired, %s, %s, %d UNUSED", lib, *lib ? " " : "",
                   brief_py_strz(avp_json_getz(o, "object")),
                   brief_hex(avp_json_get_i64(o, "start", 0)), brief_hex(avp_json_get_i64(o, "end", 0)),
                   avp_json_get_i64(o, "paired", 0), clean ? "clean" : "not clean", what,
                   avp_json_get_i64(o, "unused", 0));
    }
    brief_more(L, n > 3 ? n - 3 : 0, "more object spans");
}
