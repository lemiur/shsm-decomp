/* draft.py's World (lazy shared state: repo, asm index, twins model, symbols, units; info, unit_of with the plan
 * memo of memo_plan_model) and tools/brief.py's infer_profile / profile_of (only what draft needs: brief.World's
 * unit table, splits and libraries). */
#include "draft.h"
#include "shsm_libraries.h"
#include "shsm_re.h"

#include <stdlib.h>
#include <string.h>

/* ---- World ---------------------------------------------------------------------------------------------------- */

DraftWorld *draft_world_new(const char *root) {
    AvpArena *a = avp_arena();
    DraftWorld *W = AVP_NEW(a, DraftWorld);
    memset(W, 0, sizeof *W);
    W->a = a;
    W->repo = shsm_repo_find(root);
    W->plans = avp_map_new();
    return W;
}

void draft_world_close(DraftWorld *W) {
    if (!W) return;
    if (W->tw_) twins_close(W->tw_);
    if (W->ix_) shsm_asmindex_close(W->ix_);
    W->tw_ = NULL;
    W->ix_ = NULL;
}

ShsmAsmIndex *draft_world_ix(DraftWorld *W) {
    if (!W->ix_) W->ix_ = shsm_asmindex_open(&(ShsmAsmIndexOpts){.repo = W->repo, .readonly = true});
    return W->ix_;
}

Twins *draft_world_tw(DraftWorld *W) {
    if (!W->tw_) W->tw_ = twins_load(W->repo->root, false, 0);
    return W->tw_;
}

const ShsmSymbolTable *draft_world_st(DraftWorld *W) {
    if (!W->st_) W->st_ = shsm_load_symbols(W->repo, true);
    return W->st_;
}

const ShsmUnits *draft_world_units(DraftWorld *W) {
    if (!W->units_) W->units_ = shsm_load_units(W->repo);
    return W->units_;
}

DraftInfo draft_world_info(void *ctx, AvpStr sym, i64 off) {
    DraftWorld *W = ctx;
    DraftInfo out = {DRAFT_NONE, {NULL, 0}};
    ShsmSymRow row;
    if (!shsm_asmindex_symbol(W->a, draft_world_ix(W), sym, &row)) return out;
    out.address = row.address;
    if (!avp_str_eqz(row.kind, "object")) return out;
    /* the Python's `except Exception: pass` around the object read */
    AvpTry t;
    if (AVP_TRY(&t)) {
        ShsmAsmBlock *b = shsm_asmindex_object(W->a, draft_world_ix(W), shsm_target_addr(row.address), &(ShsmAsmFile *){NULL});
        if (b)
            for (size_t i = 0; i < b->items.n; i++) {
                const ShsmAsmItem *it = &b->items.v[i];
                if (it->address == row.address + off && it->kind && !strcmp(it->kind, "string"))
                    out.string = it->value.t == SHSM_VAL_STR ? it->value.s : AVP_S("");
            }
        avp_try_end(&t);
    }
    return out;
}

/* ---- the plan memo (memo_plan_model) ------------------------------------------------------------------------- */

/* One UpState per (root, cache stamp) for the process, as the Python's patched load_cached keeps them. */
static AvpMap *g_states;

static UpState *memo_state(DraftWorld *W) {
    ShsmAsmIndex *ix = draft_world_ix(W);
    AvpArena *a = avp_arena();
    AvpStr stamp = up_cache_stamp(a, W->repo, ix);
    AvpBuf key;
    avp_buf_init(&key, a);
    avp_buf_putz(&key, W->repo->root);
    avp_buf_put(&key, AVP_S("\n"));
    avp_buf_put(&key, stamp);
    if (!g_states) g_states = avp_map_new();
    AvpVal *v = avp_map_get(g_states, avp_buf_str(&key));
    if (v) return v->p;
    UpState *s = up_load_cached(a, W->repo, ix, true);
    if (!s) return NULL;
    avp_map_put(g_states, avp_buf_str(&key), NULL)->p = s;
    return s;
}

UpPlanDoc *draft_world_plan_for(DraftWorld *W, i64 start, i64 end) {
    UpPlanDoc *d = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        UpState *s = memo_state(W);
        if (s) {
            up_warn_classes(s->m);
            AvpArena *a = W->a;
            d = AVP_NEW(a, UpPlanDoc);
            memset(d, 0, sizeof *d);
            d->start = start;
            d->end = end;
            d->accuracy = s->calib;
            d->header = avp_cz(a, up_accuracy_header(a, s->calib));
            d->units = up_plan(a, s->m, start, end, s->calib, &s->ev, up_min_p("medium"), s->lab);
            d->class_note = s->m->class_note;
        }
        avp_try_end(&t);
    } else {
        d = NULL; /* the Python's `except Exception: self._plans[key] = None` */
    }
    return d;
}

/* ---- unit_of ---------------------------------------------------------------------------------------------------- */

DraftUnitOf draft_world_unit_of(DraftWorld *W, i64 address) {
    AvpArena *a = W->a;
    DraftUnitOf r = {.path = NULL, .kind = "none"};
    const ShsmSymbolTable *st = draft_world_st(W);
    const ShsmSpan *sp = st->splits ? shsm_splits_range_of(st->splits, (u64)address) : NULL;
    if (sp) {
        const char *u = draft_plain_unit(a, sp->unit);
        if (strncmp(u, "auto_", 5) != 0) {
            r.path = u;
            r.kind = "unit";
            r.has_range = true;
            r.start = (i64)sp->start;
            r.end = (i64)sp->end;
            return r;
        }
    }
    ShsmAsmIndex *ix = draft_world_ix(W);
    ShsmSymRow row;
    if (shsm_asmindex_symbol_at(a, ix, address, &row) && row.unit.p && row.unit.n) {
        ShsmSymRows fns = shsm_asmindex_unit_symbols(a, ix, avp_cz(a, row.unit), "function");
        if (!fns.n) shsm_raise(SHSM_EXC_VALUE, "min() iterable argument is empty");
        i64 lo = fns.v[0].address, hi = INT64_MIN;
        for (size_t i = 0; i < fns.n; i++) {
            i64 sz = fns.v[i].size == SHSM_NONE ? 0 : fns.v[i].size;
            if (fns.v[i].address < lo) lo = fns.v[i].address;
            if (fns.v[i].address + sz > hi) hi = fns.v[i].address + sz;
        }
        char kbuf[48];
        snprintf(kbuf, sizeof kbuf, "%lld-%lld", (long long)lo, (long long)hi);
        AvpVal *pv = avp_map_get(W->plans, avp_str(kbuf));
        UpPlanDoc *plan;
        if (pv) {
            plan = pv->p;
        } else {
            plan = draft_world_plan_for(W, lo, hi);
            avp_map_put(W->plans, avp_str(kbuf), NULL)->p = plan;
        }
        if (plan)
            for (size_t i = 0; i < plan->units.n; i++) {
                const UpUnit *u = &plan->units.v[i];
                if (!(u->start <= address && address < u->end)) continue;
                const char *path = NULL;
                ShsmReMatch m;
                AvpStr cmd = avp_str(u->command);
                if (shsm_re_search(shsm_re("unit\\.(?:py|exe) new (\\S+)", 0), cmd, 0, AVP_END, &m)) {
                    AvpStr g = shsm_re_group(&m, 1);
                    if (!avp_str_eqz(g, "<path>")) path = avp_cz(a, g);
                }
                r.path = path;
                r.kind = "plan";
                r.has_conf = u->has_conf;
                r.conf = u->confidence;
                r.end_edge = !u->has_conf;
                r.start_edge = i == 0;
                if (i > 0 && plan->units.v[i - 1].has_conf) {
                    r.has_start_conf = true;
                    r.start_conf = plan->units.v[i - 1].confidence;
                }
                r.has_range = true;
                r.start = u->start;
                r.end = u->end;
                return r;
            }
        r.kind = "asm";
        r.has_range = true;
        r.start = lo;
        r.end = hi;
        return r;
    }
    return r;
}

/* ---- brief.infer_profile ----------------------------------------------------------------------------------------- */

AvpStrVec draft_base_words(AvpArena *a, const AvpStrVec *cflags) {
    (void)a;
    AvpStrVec out = {0};
    i64 last = -1;
    for (size_t i = 0; i < cflags->n; i++)
        if (avp_str_startswith(cflags->v[i], AVP_S("-D"))) last = (i64)i;
    for (size_t i = (size_t)(last + 1); i < cflags->n; i++)
        if (!avp_str_startswith(cflags->v[i], AVP_S("-lang"))) avp_vec_push(&out, cflags->v[i]);
    return out;
}

/* brief.World.library(address): libraries.tsv's library, None outside every row or when the table fails to load. */
static const char *library_at(AvpArena *a, const ShsmRepo *r, i64 address) {
    const char *lib = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        lib = shsm_libraries_lookup(a, shsm_load_libraries(r), address);
        avp_try_end(&t);
    }
    return lib;
}

static AvpStr group_of(const char *lib) {
    AvpStr s = avp_str(lib), before, after;
    if (avp_str_partition(s, AVP_S("/"), &before, &after)) return before;
    return s;
}

bool draft_infer_profile(AvpArena *a, const ShsmRepo *r, i64 address, const char *lib, bool lib_given,
                         DraftProfile *out) {
    if (!lib_given) lib = library_at(a, r, address);
    const ShsmSplits *sp = shsm_load_splits(r);
    const ShsmUnits *us = shsm_load_units(r);
    AvpStr group = group_of(lib);
    const ShsmUnit *best = NULL;
    int best_rank = 0;
    i64 best_dist = 0;
    for (size_t i = 0; i < sp->nunits; i++) {
        const ShsmSplitUnit *su = &sp->units[i];
        const ShsmSplitRange *t = NULL;
        for (size_t k = 0; k < su->nranges; k++)
            if (!strcmp(su->ranges[k].section, ".text")) {
                t = &su->ranges[k];
                break;
            }
        const ShsmUnit *u = shsm_units_lookup(us, shsm_unit_name(a, su->path));
        if (u && !(shsm_unit_has_source(u) && u->source[0])) u = NULL; /* brief.World.source_unit */
        if (!t || !u || !u->has_cflags || !u->cflags.n) continue;
        const char *ulib = library_at(a, r, (i64)t->start);
        int rank = (ulib && lib ? !strcmp(ulib, lib) : ulib == lib) ? 0
                   : avp_str_eq(group_of(ulib), group)            ? 1
                                                                  : 2;
        i64 dist = llabs((long long)((i64)t->start - address));
        if (!best || rank < best_rank || (rank == best_rank && dist < best_dist)) {
            best = u;
            best_rank = rank;
            best_dist = dist;
        }
    }
    if (!best) return false;
    static const char *const MATCH[] = {"same library", "same group", "different library"};
    memset(out, 0, sizeof *out);
    out->unit = best->name;
    out->library = best->library;
    out->compiler = best->mw_version;
    out->extra = draft_base_words(a, &best->cflags);
    for (size_t i = 0; i < best->cflags.n; i++)
        if (avp_str_startswith(best->cflags.v[i], AVP_S("-lang="))) {
            AvpStr before, after;
            avp_str_partition(best->cflags.v[i], AVP_S("="), &before, &after);
            out->lang = avp_cz(a, after);
            break;
        }
    out->match = MATCH[best_rank];
    out->distance = best_dist;
    return true;
}

AvpJson *draft_profile_json(AvpArena *a, const DraftProfile *p) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("unit"), avp_json_strz(a, p->unit));
    avp_json_set(a, o, AVP_S("library"), p->library ? avp_json_strz(a, p->library) : avp_json_null());
    avp_json_set(a, o, AVP_S("compiler"), p->compiler ? avp_json_strz(a, p->compiler) : avp_json_null());
    AvpJson *ex = avp_json_arr(a);
    for (size_t i = 0; i < p->extra.n; i++) avp_json_arr_push(a, ex, avp_json_str(a, p->extra.v[i]));
    avp_json_set(a, o, AVP_S("extra"), ex);
    avp_json_set(a, o, AVP_S("lang"), p->lang ? avp_json_strz(a, p->lang) : avp_json_null());
    avp_json_set(a, o, AVP_S("match"), avp_json_strz(a, p->match));
    avp_json_set(a, o, AVP_S("distance"), avp_json_int(a, p->distance));
    return o;
}

const UnitDecls *draft_world_decls(DraftWorld *W) {
    if (!W->decls_) W->decls_ = unit_decls_scan(W->a, W->repo->include_dir);
    return W->decls_;
}
