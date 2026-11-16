/* pick.py: the library filters, the reference table, pct, the World model and report.stale_note. */
#include "pick.h"

#include <string.h>

/* REFERENCES: public reference projects per library, from docs/references.md (the "reference first" rule). */
static const PickReference REFERENCES[] = {
    {"sdk", "yes", "RVL SDK"},
    {"sdk/bte", "yes", "Bluetooth BTE stack"},
    {"sdk/hbm", "yes", "Home Button Menu (HBM) and nw4hbm"},
    {"sdk/thp", "yes", "THP"},
    {"sdk/NdevExi2AD", "yes", "NdevExi2AD"},
    {"sdk/kpad", "yes", "KPAD, WPAD, WUD, USB, TPL"},
    {"sdk/wpad", "yes", "KPAD, WPAD, WUD, USB, TPL"},
    {"sdk/wud", "yes", "KPAD, WPAD, WUD, USB, TPL"},
    {"sdk/usb", "yes", "KPAD, WPAD, WUD, USB, TPL"},
    {"sdk/tpl", "yes", "KPAD, WPAD, WUD, USB, TPL"},
    {"MSL_C", "yes", "MSL C, Metrowerks runtime, MetroTRK"},
    {"Runtime.PPCEABI.H", "yes", "MSL C, Metrowerks runtime, MetroTRK"},
    {"MetroTRK", "yes", "MSL C, Metrowerks runtime, MetroTRK"},
    {"crt", "yes", "MSL C, Metrowerks runtime, MetroTRK"},
    {"zlib", "inflate", "zlib 1.2.3"},
    {"libjpeg", "no", "IJG libjpeg 6b"},
    {"renderware", "partial", "RenderWare Graphics (3.7) and RW Physics"},
    {"rwphysics", "no", "RenderWare Graphics (3.7) and RW Physics"},
    {"fmod", "no", "FMOD Ex 4.x and the event system"},
    {"game", "no", "No public decomp found"},
    {"climax", "no", "No public decomp found"},
    {"unknown", "no", NULL},
};

const PickReference *pick_references(size_t *n) {
    *n = sizeof REFERENCES / sizeof REFERENCES[0];
    return REFERENCES;
}

static const PickReference *ref_get(AvpStr lib) {
    for (size_t i = 0; i < sizeof REFERENCES / sizeof REFERENCES[0]; i++)
        if (avp_str_eq(lib, avp_str(REFERENCES[i].library))) return &REFERENCES[i];
    return NULL;
}

const char *pick_reference_flag(const char *lib, const char **section) {
    if (!lib || !*lib) lib = "unknown";
    const PickReference *r = ref_get(avp_str(lib));
    if (!r) r = ref_get(shsm_library_group_of(lib));
    if (section) *section = r ? r->section : NULL;
    return r ? r->flag : "?";
}

bool pick_lib_matches(AvpArena *a, const char *lib, const char *query, const char *unit_library) {
    if (!query || !*query) return true;
    AvpStr q = avp_str_strip_chars(avp_str_lower(a, avp_str(query)), AVP_S("/"));
    AvpStr name = avp_str_lower(a, avp_str(lib && *lib ? lib : "unknown"));
    if (avp_str_eq(name, q)) return true;
    AvpStr qs = avp_pyfmt(a, "%S/", &q);
    if (avp_str_startswith(name, qs)) return true;
    AvpStrVec parts = avp_str_split(name, AVP_S("/"), -1);
    bool hit = false;
    for (size_t i = 0; i < parts.n && !hit; i++) hit = avp_str_eq(parts.v[i], q);
    avp_vec_free(&parts);
    if (hit) return true;
    return unit_library && *unit_library && avp_str_eq(avp_str_lower(a, avp_str(unit_library)), q);
}

bool pick_unit_matches(AvpArena *a, const char *path, const char *query) {
    if (!query || !*query) return true;
    const char *q = (strchr(query, '/') || strncmp(query, "main", 4) == 0) ? shsm_unit_path(a, query) : query;
    return strcmp(path, q) == 0 || strncmp(path, q, strlen(q)) == 0 || strstr(path, query) != NULL;
}

const char *pick_pct(AvpArena *a, bool has, double p) {
    if (!has) return "-";
    if (p >= 100.0) return "100%";
    double r = avp_py_round(p, 1);
    if (r > 99.9) r = 99.9; /* min(round(p, 1), 99.9) */
    return avp_cz(a, avp_pyfmt(a, "%.1f%%", r));
}

/* ---- World ---- */

static int fn_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const PickFn *f = *(PickFn *const *)x, *g = *(PickFn *const *)y;
    i64 fa = f->has_address ? f->address : 0, ga = g->has_address ? g->address : 0;
    if (fa != ga) return fa < ga ? -1 : 1;
    return strcmp(f->name, g->name);
}

PickWorld *pick_world_new(AvpArena *a, const char *root, const char *report_path) {
    PickWorld *w = AVP_NEW(a, PickWorld);
    w->repo = shsm_repo_find(root);
    if (report_path) {
        w->report = shsm_read_report(report_path);
        w->report_path = report_path;
    } else {
        w->report = shsm_load_report(w->repo);
        w->report_path = w->repo->report_json;
    }
    AvpTry t;
    if (AVP_TRY(&t)) {
        w->unit_table = shsm_load_units(w->repo);
        avp_try_end(&t);
    } else {
        if (!shsm_exc_is_os(t.code) && !shsm_exc_is_value(t.code)) avp_throw(t.code, "%s", t.msg);
        w->unit_table = NULL;
    }
    if (AVP_TRY(&t)) {
        w->libraries = shsm_load_libraries(w->repo);
        avp_try_end(&t);
    } else {
        if (shsm_exc_is_value(t.code)) shsm_raise(SHSM_EXC_USAGE, "%s", t.msg);
        avp_throw(t.code, "%s", t.msg);
    }
    w->by_unit = avp_map_new();
    w->unit_status = avp_map_new();
    w->unit_library = avp_map_new();
    for (size_t i = 0; i < w->report->nunits; i++) {
        const ShsmUnitReport *ur = w->report->units[i];
        const char *path = shsm_unit_path(a, ur->name);
        const ShsmUnit *tu = w->unit_table ? shsm_units_lookup(w->unit_table, ur->name) : NULL;
        const char *status;
        if (ur->auto_generated) status = "Asm";
        else if (tu && strcmp(tu->status, "Asm") != 0) status = tu->status;
        else status = ur->complete ? "Matching" : "NonMatching";
        AvpStr key = avp_str(path);
        avp_map_put(w->unit_status, key, NULL)->p = (void *)status;
        const char *ulib = tu ? tu->library : NULL;
        avp_map_put(w->unit_library, key, NULL)->p = (void *)ulib;
        bool created = false;
        AvpVal *slot = avp_map_put(w->by_unit, key, &created);
        if (created) slot->p = AVP_NEW(a, PickFnVec);
        PickFnVec *lst = slot->p;
        for (size_t k = 0; k < ur->nfunctions; k++) {
            const ShsmFunctionReport *fr = &ur->functions[k];
            PickFn *f = AVP_NEW(a, PickFn);
            f->name = fr->name;
            f->unit = path;
            f->status = status;
            f->linked = ur->complete;
            f->has_address = fr->has_address;
            f->address = (i64)fr->address;
            f->size = fr->size;
            f->fuzzy = fr->fuzzy;
            f->scored = fr->scored;
            const char *lib = fr->has_address ? shsm_libraries_lookup(a, w->libraries, (i64)fr->address) : NULL;
            f->library = lib ? lib : ulib ? ulib : "unknown";
            f->unit_library = ulib;
            f->demangled = fr->demangled;
            avp_vec_push(lst, f);
            avp_vec_push(&w->fns, f);
        }
    }
    avp_sort(w->fns.v, w->fns.n, sizeof(PickFn *), fn_cmp, NULL, false);
    return w;
}

const PickFnVec *pick_world_unit_fns(const PickWorld *w, const char *path) {
    AvpVal *v = avp_map_get(w->by_unit, avp_str(path));
    return v ? v->p : NULL;
}

const char *pick_world_unit_status(const PickWorld *w, const char *path) {
    AvpVal *v = avp_map_get(w->unit_status, avp_str(path));
    return v ? v->p : NULL;
}

const char *pick_world_unit_library(const PickWorld *w, const char *path) {
    AvpVal *v = avp_map_get(w->unit_library, avp_str(path));
    return v ? v->p : NULL;
}

PickFn *pick_world_by_address(PickWorld *w, i64 address) {
    if (!w->by_addr) {
        w->by_addr = avp_imap_new();
        for (size_t i = 0; i < w->fns.n; i++)
            if (w->fns.v[i]->has_address) avp_imap_put(w->by_addr, (u64)w->fns.v[i]->address, NULL)->p = w->fns.v[i];
    }
    AvpVal *v = avp_imap_get(w->by_addr, (u64)address);
    return v ? v->p : NULL;
}

bool pick_world_matched(const PickFn *f) { return f->linked && f->fuzzy >= 100.0; }

const char *pick_world_unit_lib(const PickWorld *w, const char *path) {
    const PickFnVec *fl = pick_world_unit_fns(w, path);
    if (fl && fl->n) {
        const PickFn *best = fl->v[0];
        for (size_t i = 1; i < fl->n; i++) {
            i64 x = fl->v[i]->has_address ? fl->v[i]->address : 0;
            i64 b = best->has_address ? best->address : 0;
            if (x < b) best = fl->v[i];
        }
        return best->library;
    }
    const char *ul = pick_world_unit_library(w, path);
    return ul ? ul : "unknown";
}

const char *pick_stale_note(AvpArena *a, const ShsmRepo *repo, const char *report_path, bool can_pass_report) {
    double rep_t = 0;
    if (!avp_getmtime(report_path, &rep_t)) return NULL;
    if (strcmp(avp_path_abspath(a, report_path), avp_path_abspath(a, repo->baseline_report_json)) == 0) return NULL;
    AvpStrVec newer = {0};
    const char *paths[2] = {repo->configure_py, repo->symbols_txt};
    for (int i = 0; i < 2; i++) {
        double t = 0;
        if (avp_getmtime(paths[i], &t) && t > rep_t) avp_vec_push(&newer, avp_path_basename(paths[i]));
    }
    if (!newer.n) return NULL;
    AvpBuf ways = {0};
    avp_buf_putz(&ways, "rebuild (tools/bin/ninja.exe)");
    if (can_pass_report) avp_buf_pyfmt(&ways, ", or pass --report %s", shsm_repo_rel(a, repo, repo->baseline_report_json));
    AvpStr joined = avp_str_join(a, AVP_S(" and "), newer.v, newer.n);
    AvpStr out = avp_pyfmt(a, "note: %s is older than %S, so these numbers predate the last change; %s",
                           shsm_repo_rel(a, repo, report_path), &joined, avp_buf_cz(&ways));
    avp_buf_free(&ways);
    avp_vec_free(&newer);
    return avp_cz(a, out);
}

/* ---- options ---- */

void pick_args_default(PickArgs *o) {
    memset(o, 0, sizeof *o);
    o->threshold = 90.0;
    o->tiny = 16;
    o->limit = SHSM_DEFAULT_LIMIT;
}

bool pick_keep(AvpArena *a, const PickFn *f, const PickArgs *o) {
    if (o->has_min_size && f->size < o->min_size) return false;
    if (o->has_max_size && f->size > o->max_size) return false;
    return pick_lib_matches(a, f->library, o->lib, f->unit_library) && pick_unit_matches(a, f->unit, o->unit);
}
