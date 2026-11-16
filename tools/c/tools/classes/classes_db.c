/* classes_db.c: decoding classes.json into the typed view of classes.h, the lookups of find_class, and the input
 * stamp (classes.py input_stamp, stamp_key, stale_reason). */
#include "classes.h"
#include "shsm_cli.h"

#include <string.h>

/* ---- decoding helpers ----------------------------------------------------------------------------------------- */
static AvpJson *req(const AvpJson *obj, const char *key) {
    if (!avp_json_is_obj(obj)) shsm_raise(SHSM_EXC_TYPE, "'%s' object is not subscriptable", "NoneType");
    AvpJson *v = avp_json_getz(obj, key);
    if (!v) shsm_raise(SHSM_EXC_KEY, "%s", key);
    return v;
}

/* A string member (or JSON null / absent: NULL). */
static const char *str_of(AvpArena *a, const AvpJson *v, const char *key) {
    if (!v || v->kind == AVP_JSON_NULL) return NULL;
    if (v->kind != AVP_JSON_STRING) shsm_raise(SHSM_EXC_TYPE, "%s: expected a string", key);
    return avp_cz(a, v->u.str);
}
static const char *req_str(AvpArena *a, const AvpJson *obj, const char *key) { return str_of(a, req(obj, key), key); }
static const char *opt_str(AvpArena *a, const AvpJson *obj, const char *key) {
    return str_of(a, avp_json_getz(obj, key), key);
}
static i64 int_of(const AvpJson *v, const char *key) {
    if (!v || (v->kind != AVP_JSON_INT && v->kind != AVP_JSON_BOOL) || v->big)
        shsm_raise(SHSM_EXC_TYPE, "%s: expected an integer", key);
    return v->kind == AVP_JSON_BOOL ? (i64)v->b : v->u.i;
}
static const AvpJson *arr_of(const AvpJson *v, const char *key) {
    if (!v || v->kind == AVP_JSON_NULL) return NULL;
    if (v->kind != AVP_JSON_ARRAY) shsm_raise(SHSM_EXC_TYPE, "%s: expected a list", key);
    return v;
}
static ClassesStrs strs_of(AvpArena *a, const AvpJson *v, const char *key) {
    ClassesStrs s = {0};
    const AvpJson *arr = arr_of(v, key);
    if (!arr) return s;
    s.n = avp_json_len(arr);
    s.v = AVP_NEWN(a, const char *, s.n ? s.n : 1);
    for (size_t i = 0; i < s.n; i++) s.v[i] = str_of(a, avp_json_at(arr, i), key);
    return s;
}

static ClassesFn *fns_of(AvpArena *a, const AvpJson *v, const char *key, size_t *n) {
    const AvpJson *arr = arr_of(v, key);
    *n = arr ? avp_json_len(arr) : 0;
    ClassesFn *out = AVP_NEWN(a, ClassesFn, *n ? *n : 1);
    for (size_t i = 0; i < *n; i++) {
        const AvpJson *f = avp_json_at(arr, i);
        out[i].name = req_str(a, f, "name");
        out[i].address = req_str(a, f, "address");
    }
    return out;
}

static void decode_vtable(AvpArena *a, const AvpJson *j, ClassesVtable *vt) {
    vt->symbol = req_str(a, j, "symbol");
    vt->address = req_str(a, j, "address");
    vt->size = opt_str(a, j, "size");
    vt->group_evidence = opt_str(a, j, "group_evidence");
    const AvpJson *groups = arr_of(avp_json_getz(j, "groups"), "groups");
    vt->n_groups = groups ? avp_json_len(groups) : 0;
    vt->groups = AVP_NEWN(a, ClassesGroup, vt->n_groups ? vt->n_groups : 1);
    for (size_t i = 0; i < vt->n_groups; i++) {
        const AvpJson *g = avp_json_at(groups, i);
        vt->groups[i].addend = req_str(a, g, "addend");
        vt->groups[i].this_offset = opt_str(a, g, "this_offset");
    }
    vt->group_conflicts = strs_of(a, avp_json_getz(j, "group_conflicts"), "group_conflicts");
    const AvpJson *words = arr_of(req(j, "words"), "words");
    vt->n_words = words ? avp_json_len(words) : 0;
    vt->words = AVP_NEWN(a, ClassesWord, vt->n_words ? vt->n_words : 1);
    for (size_t i = 0; i < vt->n_words; i++) {
        AvpJson *w = avp_json_at(words, i);
        ClassesWord *o = &vt->words[i];
        o->json = w;
        o->index = int_of(req(w, "index"), "index");
        o->offset = req_str(a, w, "offset");
        o->status = req_str(a, w, "status");
        o->group = opt_str(a, w, "group");
        o->target = opt_str(a, w, "target");
        o->target_address = opt_str(a, w, "target_address");
        o->base = opt_str(a, w, "base");
        const AvpJson *bt = avp_json_getz(w, "base_target");
        o->has_base_target = bt != NULL;
        o->base_target = str_of(a, bt, "base_target");
        const AvpJson *bi = avp_json_getz(w, "base_index");
        o->has_base_index = bi != NULL && bi->kind != AVP_JSON_NULL;
        if (o->has_base_index) o->base_index = int_of(bi, "base_index");
        const AvpJson *th = avp_json_getz(w, "thunk");
        o->has_thunk = th != NULL;
        if (th) {
            o->thunk_adjust = int_of(req(th, "adjust"), "adjust");
            o->thunk_target = opt_str(a, th, "target");
        }
        o->rtti = opt_str(a, w, "rtti");
        o->rtti_offset = opt_str(a, w, "rtti_offset");
    }
}

static void decode_class(AvpArena *a, AvpJson *j, ClassesClass *c) {
    c->json = j;
    c->key = req_str(a, j, "key");
    c->name = req_str(a, j, "name");
    c->name_source = req_str(a, j, "name_source");
    c->unit = opt_str(a, j, "unit");
    const AvpJson *nc = arr_of(avp_json_getz(j, "name_candidates"), "name_candidates");
    c->n_name_candidates = nc ? avp_json_len(nc) : 0;
    c->name_candidates = AVP_NEWN(a, ClassesNameCand, c->n_name_candidates ? c->n_name_candidates : 1);
    for (size_t i = 0; i < c->n_name_candidates; i++) {
        const AvpJson *x = avp_json_at(nc, i);
        c->name_candidates[i].name = req_str(a, x, "name");
        c->name_candidates[i].source = opt_str(a, x, "source");
    }
    c->vtable_symbol = opt_str(a, j, "vtable_symbol");
    const AvpJson *vt = avp_json_getz(j, "vtable");
    if (vt && vt->kind != AVP_JSON_NULL) {
        if (vt->kind != AVP_JSON_OBJECT) shsm_raise(SHSM_EXC_TYPE, "vtable: expected an object");
        c->vtable = AVP_NEW(a, ClassesVtable);
        decode_vtable(a, vt, c->vtable);
    }
    c->ctors = fns_of(a, avp_json_getz(j, "ctors"), "ctors", &c->n_ctors);
    c->dtors = fns_of(a, avp_json_getz(j, "dtors"), "dtors", &c->n_dtors);
    c->factories = fns_of(a, avp_json_getz(j, "factories"), "factories", &c->n_factories);
    c->other_installers = fns_of(a, avp_json_getz(j, "other_installers"), "other_installers", &c->n_other_installers);
    const AvpJson *reg = arr_of(avp_json_getz(j, "registry"), "registry");
    c->n_registry = reg ? avp_json_len(reg) : 0;
    c->registry = AVP_NEWN(a, ClassesRegistry, c->n_registry ? c->n_registry : 1);
    for (size_t i = 0; i < c->n_registry; i++) {
        const AvpJson *r = avp_json_at(reg, i);
        c->registry[i].string = opt_str(a, r, "string");
        c->registry[i].sinit = opt_str(a, r, "sinit");
        c->registry[i].factory = opt_str(a, r, "factory");
        c->registry[i].size = opt_str(a, r, "size");
    }
    const AvpJson *mem = arr_of(avp_json_getz(j, "members"), "members");
    c->n_members = mem ? avp_json_len(mem) : 0;
    c->members = AVP_NEWN(a, ClassesMember, c->n_members ? c->n_members : 1);
    for (size_t i = 0; i < c->n_members; i++) {
        const AvpJson *m = avp_json_at(mem, i);
        c->members[i].name = opt_str(a, m, "name");
        c->members[i].address = opt_str(a, m, "address");
        c->members[i].role = opt_str(a, m, "role");
    }
    const AvpJson *bases = arr_of(avp_json_getz(j, "bases"), "bases");
    c->n_bases = bases ? avp_json_len(bases) : 0;
    c->bases = AVP_NEWN(a, ClassesBase, c->n_bases ? c->n_bases : 1);
    for (size_t i = 0; i < c->n_bases; i++) {
        const AvpJson *b = avp_json_at(bases, i);
        ClassesBase *o = &c->bases[i];
        o->cls = req_str(a, b, "class");
        o->key = req_str(a, b, "key");
        o->offset = opt_str(a, b, "offset");
        o->primary = avp_json_truthy(avp_json_getz(b, "primary"));
        const AvpJson *vw = avp_json_getz(b, "vtable_word");
        o->has_vtable_word = vw && vw->kind != AVP_JSON_NULL;
        if (o->has_vtable_word) o->vtable_word = int_of(vw, "vtable_word");
        o->size_estimate = opt_str(a, b, "size_estimate");
        o->evidence = strs_of(a, avp_json_getz(b, "evidence"), "evidence");
    }
    c->derived = strs_of(a, avp_json_getz(j, "derived"), "derived");
    c->vptrs = strs_of(a, avp_json_getz(j, "vptrs"), "vptrs");
    c->own_vptrs = strs_of(a, avp_json_getz(j, "own_vptrs"), "own_vptrs");
    c->field_conflicts = strs_of(a, avp_json_getz(j, "field_conflicts"), "field_conflicts");
    const AvpJson *mo = arr_of(avp_json_getz(j, "member_objects"), "member_objects");
    c->n_member_objects = mo ? avp_json_len(mo) : 0;
    c->member_objects = AVP_NEWN(a, ClassesMemberObject, c->n_member_objects ? c->n_member_objects : 1);
    for (size_t i = 0; i < c->n_member_objects; i++) {
        const AvpJson *m = avp_json_at(mo, i);
        c->member_objects[i].offset = req_str(a, m, "offset");
        c->member_objects[i].cls = req_str(a, m, "class");
        c->member_objects[i].evidence = strs_of(a, avp_json_getz(m, "evidence"), "evidence");
    }
    const AvpJson *fields = arr_of(avp_json_getz(j, "fields"), "fields");
    c->n_fields = fields ? avp_json_len(fields) : 0;
    c->fields = AVP_NEWN(a, ClassesField, c->n_fields ? c->n_fields : 1);
    for (size_t i = 0; i < c->n_fields; i++) {
        const AvpJson *f = avp_json_at(fields, i);
        ClassesField *o = &c->fields[i];
        o->offset = req_str(a, f, "offset");
        o->width = int_of(req(f, "width"), "width");
        o->kind = req_str(a, f, "kind");
        o->type = req_str(a, f, "type");
        o->loads = int_of(req(f, "loads"), "loads");
        o->stores = int_of(req(f, "stores"), "stores");
        o->functions = int_of(req(f, "functions"), "functions");
        o->inherited_from = opt_str(a, f, "inherited_from");
        o->seen_in_derived = strs_of(a, avp_json_getz(f, "seen_in_derived"), "seen_in_derived");
        o->conflict = strs_of(a, avp_json_getz(f, "conflict"), "conflict");
    }
    c->base_end = opt_str(a, j, "base_end");
    c->size = opt_str(a, j, "size");
    c->size_source = opt_str(a, j, "size_source");
    const AvpJson *se = avp_json_getz(j, "size_estimate");
    c->has_size_estimate = se && se->kind != AVP_JSON_NULL;
    if (c->has_size_estimate) c->size_estimate = int_of(se, "size_estimate");
}

ClassesDb *classes_db_from_json(AvpArena *a, AvpJson *doc) {
    if (!avp_json_is_obj(doc)) shsm_raise(SHSM_EXC_TYPE, "classes.json: expected an object");
    ClassesDb *db = AVP_NEW(a, ClassesDb);
    db->doc = doc;
    db->inputs = avp_json_getz(doc, "inputs");
    db->stats = avp_json_getz(doc, "stats");
    db->methods = avp_json_getz(doc, "methods");
    const AvpJson *cl = arr_of(req(doc, "classes"), "classes");
    db->n = cl ? avp_json_len(cl) : 0;
    db->classes = AVP_NEWN(a, ClassesClass, db->n ? db->n : 1);
    db->by_key = avp_map_new_cap(db->n);
    for (size_t i = 0; i < db->n; i++) {
        decode_class(a, avp_json_at(cl, i), &db->classes[i]);
        avp_map_put(db->by_key, avp_str(db->classes[i].key), NULL)->i = (i64)i + 1;
    }
    return db;
}

ClassesDb *classes_db_load(AvpArena *a, const char *path) {
    AvpBytes raw = {0};
    if (!avp_read_all(a, path, &raw)) shsm_raise_io();
    AvpJsonError err = {0};
    AvpJson *doc = avp_json_parse(a, avp_strn((const char *)raw.v, raw.n), &err);
    if (!doc) shsm_raise(SHSM_EXC_VALUE, "%s", avp_cz(a, avp_json_error_str(a, &err)));
    return classes_db_from_json(a, doc);
}

/* ---- lookups -------------------------------------------------------------------------------------------------- */
static bool eqz(const char *x, const char *y) { return x && y && strcmp(x, y) == 0; }

const ClassesClass *classes_by_key(const ClassesDb *db, const char *key) {
    AvpVal *v = key ? avp_map_get(db->by_key, avp_str(key)) : NULL;
    return v ? &db->classes[v->i - 1] : NULL;
}

const ClassesClass *classes_by_name(const ClassesDb *db, const char *name) {
    for (size_t i = 0; i < db->n; i++)
        if (eqz(db->classes[i].name, name)) return &db->classes[i];
    return NULL;
}

const ClassesClass *classes_by_vtable_symbol(const ClassesDb *db, const char *symbol) {
    for (size_t i = 0; i < db->n; i++)
        if (db->classes[i].vtable && eqz(db->classes[i].vtable->symbol, symbol)) return &db->classes[i];
    return NULL;
}

static const ClassesClass *by_address_text(const ClassesDb *db, const char *addr) {
    for (size_t i = 0; i < db->n; i++) {
        const ClassesClass *c = &db->classes[i];
        if (c->vtable && eqz(c->vtable->address, addr)) return c;
        for (size_t k = 0; k < c->n_ctors; k++)
            if (eqz(c->ctors[k].address, addr)) return c;
        for (size_t k = 0; k < c->n_dtors; k++)
            if (eqz(c->dtors[k].address, addr)) return c;
        for (size_t k = 0; k < c->n_factories; k++)
            if (eqz(c->factories[k].address, addr)) return c;
    }
    return NULL;
}

const ClassesClass *classes_by_address(const ClassesDb *db, u32 addr) {
    char buf[16];
    snprintf(buf, sizeof buf, "0x%X", addr);
    return by_address_text(db, buf);
}

static bool is_hex(char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

/* re.match(r"^(?:0x)?([0-9A-Fa-f]{8})$", s): eight hex digits, `$` also before a final newline */
static bool eight_hex(const char *s, u32 *out) {
    size_t n = strlen(s);
    if (n == 9 && s[8] == 0x0A) n = 8;
    if (n != 8) return false;
    u32 v = 0;
    for (size_t i = 0; i < 8; i++) {
        if (!is_hex(s[i])) return false;
        v = v * 16 + (u32)(s[i] <= '9' ? s[i] - '0' : (s[i] | 0x20) - 'a' + 10);
    }
    *out = v;
    return true;
}

const ClassesClass *classes_find(const ClassesDb *db, const char *q) {
    for (size_t i = 0; i < db->n; i++) {
        const ClassesClass *c = &db->classes[i];
        if (eqz(c->name, q) || eqz(c->key, q) || (c->vtable && eqz(c->vtable->symbol, q))) return c;
    }
    u32 addr = 0;
    if ((q[0] == '0' && q[1] == 'x' && eight_hex(q + 2, &addr)) || eight_hex(q, &addr)) {
        const ClassesClass *c = classes_by_address(db, addr);
        if (c) return c;
    }
    for (size_t i = 0; i < db->n; i++) {
        const ClassesClass *c = &db->classes[i];
        for (size_t k = 0; k < c->n_name_candidates; k++)
            if (eqz(c->name_candidates[k].name, q)) return c;
    }
    return NULL;
}

const char *classes_method(const ClassesDb *db, const char *function) {
    if (!function || !avp_json_is_obj(db->methods)) return NULL;
    AvpJson *v = avp_json_getz(db->methods, function);
    if (!v || v->kind == AVP_JSON_NULL) return NULL;
    if (v->kind != AVP_JSON_STRING) return NULL;
    return avp_cz(avp_arena(), v->u.str);
}

bool classes_hex(const char *s, i64 *out) {
    if (!s) return false;
    return avp_parse_int(avp_str(s), 16, out, NULL);
}

/* ---- paths and the input stamp ------------------------------------------------------------------------------- */
ClassesPaths classes_default_paths(AvpArena *a, const char *root) {
    ClassesPaths p;
    p.asm_dir = avp_join(a, root, "build", "R5WEA4", "asm");
    p.symbols = avp_join(a, root, "config", "R5WEA4", "symbols.txt");
    p.out = avp_join(a, root, "build", "R5WEA4", "classes.json");
    p.splits = avp_join(a, root, "config", "R5WEA4", "splits.txt");
    return p;
}

static bool ends_s(AvpStr fn) { return fn.n >= 2 && fn.p[fn.n - 2] == '.' && fn.p[fn.n - 1] == 's'; }

AvpJson *classes_input_stamp(AvpArena *a, const char *asm_dir, const char *symbols) {
    i64 files = 0, size = 0, newest = 0;
    AvpWalk *w = avp_walk_open(asm_dir, true);
    if (w) {
        AvpWalkItem it = {0};
        while (avp_walk_next(w, &it)) {
            for (size_t i = 0; i < it.files.n; i++) {
                if (!ends_s(it.files.v[i])) continue;
                AvpArenaMark mk = avp_arena_mark(avp_scratch());
                char *full = avp_path_join(avp_scratch(), avp_cz(avp_scratch(), it.dirpath),
                                           avp_cz(avp_scratch(), it.files.v[i]));
                AvpStat st;
                if (!avp_stat(full, &st)) {
                    avp_walk_close(w);
                    shsm_raise_io();
                }
                avp_arena_reset(avp_scratch(), mk);
                files++;
                size += st.size;
                if (st.mtime_ns > newest) newest = st.mtime_ns;
            }
        }
        avp_walk_close(w);
    }
    AvpStat st;
    if (!avp_stat(symbols, &st)) shsm_raise_io();
    AvpJson *o = avp_json_obj(a);
    AvpJson *as = avp_json_obj(a);
    avp_json_set(a, as, AVP_S("dir"), avp_json_strz(a, avp_path_abspath(a, asm_dir)));
    avp_json_set(a, as, AVP_S("files"), avp_json_int(a, files));
    avp_json_set(a, as, AVP_S("bytes"), avp_json_int(a, size));
    avp_json_set(a, as, AVP_S("mtime_ns"), avp_json_int(a, newest));
    AvpJson *sy = avp_json_obj(a);
    avp_json_set(a, sy, AVP_S("path"), avp_json_strz(a, avp_path_abspath(a, symbols)));
    avp_json_set(a, sy, AVP_S("bytes"), avp_json_int(a, st.size));
    avp_json_set(a, sy, AVP_S("mtime_ns"), avp_json_int(a, st.mtime_ns));
    avp_json_set(a, o, AVP_S("asm"), as);
    avp_json_set(a, o, AVP_S("symbols"), sy);
    return o;
}

bool classes_stamp_key(const AvpJson *stamp, ClassesStampKey *out) {
    memset(out, 0, sizeof *out);
    if (!avp_json_is_obj(stamp)) return false;
    /* stamp.get("asm") or {}: a falsy value is an empty dict; a truthy non-dict would be an AttributeError */
    const AvpJson *as = avp_json_getz(stamp, "asm");
    const AvpJson *sy = avp_json_getz(stamp, "symbols");
    if (avp_json_truthy(as) && !avp_json_is_obj(as)) shsm_raise(SHSM_EXC_TYPE, "'asm' of the stamp is not a dict");
    if (avp_json_truthy(sy) && !avp_json_is_obj(sy))
        shsm_raise(SHSM_EXC_TYPE, "'symbols' of the stamp is not a dict");
    if (avp_json_is_obj(as)) {
        out->v[0] = avp_json_getz(as, "files");
        out->v[1] = avp_json_getz(as, "bytes");
        out->v[2] = avp_json_getz(as, "mtime_ns");
    }
    if (avp_json_is_obj(sy)) {
        out->v[3] = avp_json_getz(sy, "bytes");
        out->v[4] = avp_json_getz(sy, "mtime_ns");
    }
    return true;
}

/* Python == on json.load values (None == None, 1 == 1.0 == True, dicts by content). */
static bool is_none(const AvpJson *v) { return !v || v->kind == AVP_JSON_NULL; }
static bool is_number(const AvpJson *v) {
    return v && (v->kind == AVP_JSON_INT || v->kind == AVP_JSON_FLOAT || v->kind == AVP_JSON_BOOL);
}
static bool py_eq(const AvpJson *x, const AvpJson *y) {
    if (is_none(x) || is_none(y)) return is_none(x) && is_none(y);
    if (is_number(x) && is_number(y)) {
        if (x->kind == AVP_JSON_INT && x->big) return y->kind == AVP_JSON_INT && y->big && avp_str_eq(x->u.str, y->u.str);
        if (y->kind == AVP_JSON_INT && y->big) return false;
        if (x->kind == AVP_JSON_FLOAT || y->kind == AVP_JSON_FLOAT) {
            double fx = x->kind == AVP_JSON_FLOAT ? x->u.f : (double)(x->kind == AVP_JSON_BOOL ? x->b : x->u.i);
            double fy = y->kind == AVP_JSON_FLOAT ? y->u.f : (double)(y->kind == AVP_JSON_BOOL ? y->b : y->u.i);
            return fx == fy;
        }
        i64 ix = x->kind == AVP_JSON_BOOL ? x->b : x->u.i;
        i64 iy = y->kind == AVP_JSON_BOOL ? y->b : y->u.i;
        return ix == iy;
    }
    if (x->kind != y->kind) return false;
    if (x->kind == AVP_JSON_STRING) return avp_str_eq(x->u.str, y->u.str);
    if (x->kind == AVP_JSON_ARRAY) {
        if (x->u.arr.n != y->u.arr.n) return false;
        for (size_t i = 0; i < x->u.arr.n; i++)
            if (!py_eq(x->u.arr.v[i], y->u.arr.v[i])) return false;
        return true;
    }
    if (x->u.obj.n != y->u.obj.n) return false;
    for (size_t i = 0; i < x->u.obj.n; i++) {
        const AvpJsonMember *m = avp_json_member(x, i);
        const AvpJson *o = avp_json_get(y, m->key);
        if (!o || !py_eq(m->val, o)) return false;
    }
    return true;
}

bool classes_stamp_key_eq(const ClassesStampKey *x, const ClassesStampKey *y) {
    for (int i = 0; i < 5; i++)
        if (!py_eq(x->v[i], y->v[i])) return false;
    return true;
}

static bool key_range_eq(const ClassesStampKey *x, const ClassesStampKey *y, int from, int to) {
    for (int i = from; i < to; i++)
        if (!py_eq(x->v[i], y->v[i])) return false;
    return true;
}

/* str() of a stamp component (an int as it is written) */
static const char *value_str(AvpArena *a, const AvpJson *v) {
    if (is_none(v)) return "None";
    if (v->kind == AVP_JSON_STRING) return avp_cz(a, v->u.str);
    return avp_cz(a, avp_json_dumps(a, v, NULL));
}

ClassesStaleness classes_staleness(AvpArena *a, const AvpJson *inputs, const char *asm_dir, const char *symbols) {
    ClassesStaleness s;
    memset(&s, 0, sizeof s);
    if (!classes_stamp_key(inputs, &s.rec)) {
        s.verdict = CLASSES_STAMP_NONE;
        return s;
    }
    AvpJson *stamp = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        stamp = classes_input_stamp(a, asm_dir, symbols);
        avp_try_end(&t);
    } else {
        if (!shsm_exc_is_os(t.code)) avp_throw(t.code, "%s", t.msg ? t.msg : "");
        s.verdict = CLASSES_STAMP_UNREADABLE;
        s.error = avp_cz(a, shsm_exc_str(a, t.code, t.msg));
        return s;
    }
    classes_stamp_key(stamp, &s.cur);
    s.asm_changed = !key_range_eq(&s.cur, &s.rec, 0, 3);
    s.symbols_changed = !key_range_eq(&s.cur, &s.rec, 3, 5);
    s.verdict = s.asm_changed || s.symbols_changed ? CLASSES_STAMP_CHANGED : CLASSES_STAMP_CURRENT;
    return s;
}

const char *classes_stale_reason(AvpArena *a, const ClassesDb *db, const char *asm_dir, const char *symbols) {
    return classes_stale_reason_inputs(a, db->inputs, asm_dir, symbols);
}

const char *classes_stale_reason_inputs(AvpArena *a, const AvpJson *inputs, const char *asm_dir, const char *symbols) {
    ClassesStaleness s = classes_staleness(a, inputs, asm_dir, symbols);
    if (s.verdict == CLASSES_STAMP_NONE) return "it records no input stamp (written by an older classes.exe)";
    if (s.verdict == CLASSES_STAMP_UNREADABLE) return avp_sprintf(a, "its inputs cannot be read (%s)", s.error);
    if (s.verdict == CLASSES_STAMP_CURRENT) return NULL;
    AvpBuf b;
    avp_buf_init(&b, a);
    if (s.asm_changed)
        avp_buf_printf(&b, "the asm tree (%s .s files, %s bytes)", value_str(a, s.cur.v[0]), value_str(a, s.cur.v[1]));
    if (s.symbols_changed) {
        if (b.n) avp_buf_putz(&b, " and ");
        avp_buf_putz(&b, "the symbols file");
    }
    avp_buf_putz(&b, " changed since the scan");
    return avp_buf_cz(&b);
}
