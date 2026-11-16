/* project.py: generate_objdiff_config (objdiff.json) and generate_compile_commands (compile_commands.json). */
#include <stdio.h>
#include <string.h>

#include "project_int.h"

/* json.dump(..., indent=2) through text-mode files: CRLF line ends, ASCII escapes */
static void write_json(AvpArena *a, const char *path, const AvpJson *doc) {
    AvpJsonDumpOpts opts = AVP_JSON_INDENT(2);
    proj_write_text(a, path, avp_json_dumps(a, doc, &opts));
}

static const char *unix_path(AvpArena *a, const char *p) {
    return avp_cz(a, avp_str_replace(a, avp_str(p), AVP_S("\\"), AVP_S("/"), -1));
}

/* A configure.py value as JSON, paths through objdiff's default=unix_path */
static AvpJson *pv_json(AvpArena *a, const Pv *v) {
    switch (v->kind) {
    case PV_NONE: return avp_json_null();
    case PV_BOOL: return avp_json_bool(v->u.b);
    case PV_INT: return avp_json_int(a, v->u.i);
    case PV_STR: return avp_json_str(a, v->u.s);
    case PV_PATH: return avp_json_strz(a, unix_path(a, avp_cz(a, v->u.s)));
    case PV_LIST:
    case PV_TUPLE: {
        AvpJson *arr = avp_json_arr(a);
        for (size_t i = 0; i < v->u.list->n; i++) avp_json_arr_push(a, arr, pv_json(a, v->u.list->v[i]));
        return arr;
    }
    case PV_DICT: {
        AvpJson *obj = avp_json_obj(a);
        for (size_t i = 0; i < v->u.dict->n; i++) avp_json_set(a, obj, v->u.dict->keys[i], pv_json(a, v->u.dict->vals[i]));
        return obj;
    }
    default: proj_exit("TypeError: Object of type %s is not JSON serializable", pv_kind_name(v));
    }
}

/* cleandict: None values dropped from every dict (not from lists) */
static AvpJson *cleandict(AvpArena *a, AvpJson *d) {
    if (avp_json_is_obj(d)) {
        AvpJson *r = avp_json_obj(a);
        for (size_t i = 0; i < avp_json_len(d); i++) {
            const AvpJsonMember *m = avp_json_member(d, i);
            if (!avp_json_is_null(m->val)) avp_json_set(a, r, m->key, cleandict(a, m->val));
        }
        return r;
    }
    if (avp_json_is_arr(d)) {
        AvpJson *r = avp_json_arr(a);
        for (size_t i = 0; i < avp_json_len(d); i++) avp_json_arr_push(a, r, cleandict(a, avp_json_at(d, i)));
        return r;
    }
    return d;
}

static const char *const COMPILER_MAP[][2] = {
    {"GC/1.0", "mwcc_233_144"},         {"GC/1.1", "mwcc_233_159"},       {"GC/1.1p1", "mwcc_233_159p1"},
    {"GC/1.2.5", "mwcc_233_163"},       {"GC/1.2.5e", "mwcc_233_163e"},   {"GC/1.2.5n", "mwcc_233_163n"},
    {"GC/1.3", "mwcc_242_53"},          {"GC/1.3.2", "mwcc_242_81"},      {"GC/1.3.2r", "mwcc_242_81r"},
    {"GC/2.0", "mwcc_247_92"},          {"GC/2.0p1", "mwcc_247_92p1"},    {"GC/2.5", "mwcc_247_105"},
    {"GC/2.6", "mwcc_247_107"},         {"GC/2.7", "mwcc_247_108"},       {"GC/3.0a3", "mwcc_41_51213"},
    {"GC/3.0a3.2", "mwcc_41_60126"},    {"GC/3.0a3.3", "mwcc_41_60209"},  {"GC/3.0a3.4", "mwcc_42_60308"},
    {"GC/3.0a5", "mwcc_42_60422"},      {"GC/3.0a5.2", "mwcc_41_60831"},  {"GC/3.0", "mwcc_41_60831"},
    {"Wii/1.0RC1", "mwcc_42_140"},      {"Wii/0x4201_127", "mwcc_42_142"}, {"Wii/1.0a", "mwcc_42_142"},
    {"Wii/1.0", "mwcc_43_145"},         {"Wii/1.1", "mwcc_43_151"},       {"Wii/1.3", "mwcc_43_172"},
    {"Wii/1.5", "mwcc_43_188"},         {"Wii/1.6", "mwcc_43_202"},       {"Wii/1.7", "mwcc_43_213"},
};

static const char *compiler_map(const Pv *mw) {
    if (mw->kind != PV_STR) return NULL;
    for (size_t i = 0; i < sizeof COMPILER_MAP / sizeof COMPILER_MAP[0]; i++)
        if (avp_str_eqz(mw->u.s, COMPILER_MAP[i][0])) return COMPILER_MAP[i][1];
    return NULL;
}

static bool keep_flag(const char *f) {
    return !proj_startswith(f, "-i ") && !proj_startswith(f, "-i-") && !proj_startswith(f, "-I ") &&
           !proj_startswith(f, "-I+") && !proj_startswith(f, "-I-");
}

static void flags_into(AvpArena *a, PStrs *out, const Pv *flags) {
    if (!flags || (flags->kind != PV_LIST && flags->kind != PV_TUPLE))
        proj_exit("TypeError: cflags must be a list, not %s", flags ? pv_kind_name(flags) : "NoneType");
    for (size_t i = 0; i < flags->u.list->n; i++) avp_vec_push(out, proj_pv_str(a, flags->u.list->v[i]));
}

typedef struct {
    PCtx *c;
    AvpJson *units;
    AvpMap *existing; /* name -> AvpJson* unit */
} Objdiff;

static void objdiff_add_unit(Objdiff *od, AvpJson *build_obj, AvpStr module_name, AvpJson *progress_categories) {
    PCtx *c = od->c;
    AvpArena *a = c->a;
    AvpJson *obj_path = avp_json_get(build_obj, AVP_S("object"));
    AvpStr obj_name = avp_json_get_str(build_obj, "name", AVP_S(""));
    const char *base_object = proj_with_suffix(a, proj_path(a, avp_cz(a, obj_name)), "");
    const char *name = unix_path(a, proj_join(a, proj_path(a, avp_cz(a, module_name)), base_object));
    AvpJson *unit = avp_json_obj(a);
    avp_json_set(a, unit, AVP_S("name"), avp_json_strz(a, name));
    avp_json_set(a, unit, AVP_S("target_path"), obj_path ? obj_path : avp_json_null());
    avp_json_set(a, unit, AVP_S("base_path"), avp_json_null());
    avp_json_set(a, unit, AVP_S("scratch"), avp_json_null());
    AvpJson *meta = avp_json_obj(a);
    avp_json_set(a, meta, AVP_S("complete"), avp_json_null());
    avp_json_set(a, meta, AVP_S("reverse_fn_order"), avp_json_null());
    avp_json_set(a, meta, AVP_S("source_path"), avp_json_null());
    avp_json_set(a, meta, AVP_S("progress_categories"), progress_categories);
    AvpJson *autogen = avp_json_get(build_obj, AVP_S("autogenerated"));
    avp_json_set(a, meta, AVP_S("auto_generated"), autogen ? autogen : avp_json_null());
    avp_json_set(a, unit, AVP_S("metadata"), meta);
    avp_json_set(a, unit, AVP_S("symbol_mappings"), avp_json_null());

    AvpVal *existing = avp_map_get(od->existing, avp_str(name));
    if (existing) {
        AvpJson *sm = avp_json_is_obj(existing->p) ? avp_json_get(existing->p, AVP_S("symbol_mappings")) : NULL;
        avp_json_set(a, unit, AVP_S("symbol_mappings"), sm ? sm : avp_json_null());
    }

    PObj *obj = proj_object(c, obj_name);
    if (!obj) {
        avp_json_arr_push(a, od->units, unit);
        return;
    }
    bool src_exists = obj->src_path && avp_exists(obj->src_path);
    if (src_exists) {
        avp_json_set(a, unit, AVP_S("base_path"), avp_json_strz(a, unix_path(a, obj->src_obj_path)));
        avp_json_set(a, meta, AVP_S("source_path"), avp_json_strz(a, unix_path(a, obj->src_path)));
    }
    PStrs both = {0}, all_cflags = {0};
    flags_into(a, &both, pv_dict_get(obj->options, AVP_S("cflags")));
    flags_into(a, &both, pv_dict_get(obj->options, AVP_S("extra_cflags")));
    for (size_t i = 0; i < both.n; i++)
        if (keep_flag(both.v[i])) avp_vec_push(&all_cflags, both.v[i]);
    bool reverse_fn_order = false;
    for (size_t i = 0; i < all_cflags.n; i++) {
        if (!proj_startswith(all_cflags.v[i], "-inline ")) continue;
        AvpStrVec words = avp_str_split(avp_str(all_cflags.v[i]), AVP_S(" "), -1);
        AvpStrVec values = avp_str_split(words.v[1], AVP_S(","), -1);
        for (size_t k = 0; k < values.n; k++) {
            if (avp_str_eqz(values.v[k], "deferred")) reverse_fn_order = true;
            else if (avp_str_eqz(values.v[k], "nodeferred")) reverse_fn_order = false;
        }
    }
    Pv *mw = pv_dict_get(obj->options, AVP_S("mw_version"));
    const char *compiler = compiler_map(mw);
    if (!compiler) {
        printf("Missing scratch compiler mapping for %s\n", proj_pv_str(a, mw));
    } else {
        AvpBuf flags;
        avp_buf_init(&flags, a);
        for (size_t i = 0; i < all_cflags.n; i++) {
            if (i) avp_buf_putc(&flags, ' ');
            avp_buf_putz(&flags, all_cflags.v[i]);
        }
        AvpJson *scratch = avp_json_obj(a);
        avp_json_set(a, scratch, AVP_S("platform"), avp_json_strz(a, "gc_wii"));
        avp_json_set(a, scratch, AVP_S("compiler"), avp_json_strz(a, compiler));
        avp_json_set(a, scratch, AVP_S("c_flags"), avp_json_strz(a, avp_buf_cz(&flags)));
        avp_json_set(a, scratch, AVP_S("preset_id"), pv_json(a, pv_dict_get(obj->options, AVP_S("scratch_preset_id"))));
        if (src_exists) {
            avp_json_set(a, scratch, AVP_S("ctx_path"), avp_json_strz(a, unix_path(a, obj->ctx_path)));
            avp_json_set(a, scratch, AVP_S("build_ctx"), avp_json_bool(true));
        }
        avp_json_set(a, unit, AVP_S("scratch"), scratch);
    }
    Pv *cat = pv_dict_get(obj->options, AVP_S("progress_category"));
    if (cat->kind == PV_LIST) {
        for (size_t i = 0; i < cat->u.list->n; i++) avp_json_arr_push(a, progress_categories, pv_json(a, cat->u.list->v[i]));
    } else if (cat->kind != PV_NONE) {
        avp_json_arr_push(a, progress_categories, pv_json(a, cat));
    }
    avp_json_set(a, meta, AVP_S("complete"), src_exists ? avp_json_bool(obj->completed) : avp_json_null());
    avp_json_set(a, meta, AVP_S("reverse_fn_order"), avp_json_bool(reverse_fn_order));
    avp_json_set(a, meta, AVP_S("progress_categories"), progress_categories);
    avp_json_arr_push(a, od->units, unit);
}

static void add_category(AvpArena *a, AvpJson *cats, AvpJson *id, AvpJson *name) {
    AvpJson *c = avp_json_obj(a);
    avp_json_set(a, c, AVP_S("id"), id);
    avp_json_set(a, c, AVP_S("name"), name);
    avp_json_arr_push(a, cats, c);
}

void proj_generate_objdiff_config(PCtx *c) {
    AvpArena *a = c->a;
    AvpJson *bc = c->build_config;
    if (!bc) return;
    Objdiff od = {c, NULL, avp_map_new()};
    if (avp_isfile("objdiff.json")) {
        AvpJson *existing = proj_load_json(a, "objdiff.json");
        AvpJson *units = avp_json_get(existing, AVP_S("units"));
        if (!units) proj_exit("KeyError: 'units'");
        for (size_t i = 0; i < avp_json_len(units); i++) {
            AvpJson *u = avp_json_at(units, i);
            AvpJson *nm = avp_json_get(u, AVP_S("name"));
            if (!nm) proj_exit("KeyError: 'name'");
            avp_map_put(od.existing, avp_json_str_or(nm, AVP_S("")), NULL)->p = u;
        }
    }
    const char *ninja = "ninja";
    Pv *ninja_path = proj_attr(c, "ninja_path");
    if (pv_truthy(ninja_path)) {
        const char *p = proj_pv_path(a, ninja_path);
        ninja = pv_path_is_absolute(avp_str(p)) ? p : proj_join(a, avp_getcwd(a), p);
    }
    AvpJson *doc = avp_json_obj(a);
    avp_json_set(a, doc, AVP_S("min_version"), avp_json_strz(a, "2.0.0-beta.5"));
    avp_json_set(a, doc, AVP_S("custom_make"), avp_json_strz(a, ninja));
    avp_json_set(a, doc, AVP_S("build_target"), avp_json_bool(false));
    static const char *const patterns[] = {"*.c",   "*.cc",  "*.cp",  "*.cpp", "*.cxx", "*.c++", "*.h",
                                           "*.hh",  "*.hp",  "*.hpp", "*.hxx", "*.h++", "*.pch", "*.pch++",
                                           "*.inc", "*.py",  "*.yml", "*.txt", "*.json"};
    AvpJson *wp = avp_json_arr(a);
    for (size_t i = 0; i < sizeof patterns / sizeof patterns[0]; i++) avp_json_arr_push(a, wp, avp_json_strz(a, patterns[i]));
    avp_json_set(a, doc, AVP_S("watch_patterns"), wp);
    od.units = avp_json_arr(a);
    avp_json_set(a, doc, AVP_S("units"), od.units);
    AvpJson *cats = avp_json_arr(a);
    avp_json_set(a, doc, AVP_S("progress_categories"), cats);

    AvpJson *units = avp_json_get(bc, AVP_S("units"));
    AvpJson *modules = avp_json_get(bc, AVP_S("modules"));
    size_t nmodules = avp_json_len(modules);
    for (size_t i = 0; i < avp_json_len(units); i++) {
        AvpJson *pcs = avp_json_arr(a);
        if (nmodules > 0) avp_json_arr_push(a, pcs, avp_json_strz(a, "dol"));
        objdiff_add_unit(&od, avp_json_at(units, i), avp_json_get_str(bc, "name", AVP_S("")), pcs);
    }
    bool progress_modules = pv_truthy(proj_attr(c, "progress_modules"));
    bool each_module = pv_truthy(proj_attr(c, "progress_each_module"));
    for (size_t m = 0; m < nmodules; m++) {
        AvpJson *module = avp_json_at(modules, m);
        AvpJson *munits = avp_json_get(module, AVP_S("units"));
        for (size_t i = 0; i < avp_json_len(munits); i++) {
            AvpJson *pcs = avp_json_arr(a);
            if (progress_modules) avp_json_arr_push(a, pcs, avp_json_strz(a, "modules"));
            if (each_module) avp_json_arr_push(a, pcs, avp_json_get(module, AVP_S("name")));
            objdiff_add_unit(&od, avp_json_at(munits, i), avp_json_get_str(module, "name", AVP_S("")), pcs);
        }
    }
    if (nmodules > 0) {
        add_category(a, cats, avp_json_strz(a, "dol"), avp_json_strz(a, "DOL"));
        if (progress_modules) add_category(a, cats, avp_json_strz(a, "modules"), avp_json_strz(a, "Modules"));
        if (each_module)
            for (size_t m = 0; m < nmodules; m++) {
                AvpJson *nm = avp_json_get(avp_json_at(modules, m), AVP_S("name"));
                add_category(a, cats, nm, nm);
            }
    }
    Pv *pcats = proj_attr(c, "progress_categories");
    for (size_t i = 0; i < pv_len(pcats) && pcats->kind != PV_DICT; i++) {
        Pv *pc = pcats->u.list->v[i];
        add_category(a, cats, pv_json(a, pv_getattr(pc, "id")), pv_json(a, pv_getattr(pc, "name")));
    }
    write_json(a, "objdiff.json", cleandict(a, doc));
}

/* ---- compile_commands.json ---- */

static const char *const REPLACE_PREFIX[][2] = {{"-i ", "-I"}, {"-I ", "-I"}, {"-I+", "-I"},
                                                {"-d ", "-D"}, {"-D ", "-D"}, {"-D+", "-D"}};

typedef struct {
    const char *prefix;
    const char *options[4][3]; /* option, replacement 1, replacement 2 (NULL when one) */
} ReplaceOptions;

static const ReplaceOptions REPLACE_OPTIONS[] = {
    {"-Cpp_exceptions", {{"off", "-fno-cxx-exceptions", NULL}, {"on", "-fcxx-exceptions", NULL}}},
    {"-RTTI", {{"off", "-fno-rtti", NULL}, {"on", "-frtti", NULL}}},
    {"-lang",
     {{"c", "--language=c", "--std=c99"},
      {"c99", "--language=c", "--std=c99"},
      {"c++", "--language=c++", "--std=c++98"},
      {"cplus", "--language=c++", "--std=c++98"}}},
    {"-enum", {{"min", "-fshort-enums", NULL}, {"int", "-fno-short-enums", NULL}}},
    {"-common", {{"off", "-fno-common", NULL}, {"on", "-fcommon", NULL}}},
};

static void append_cflags(AvpArena *a, AvpJson *out, const Pv *flags) {
    PStrs fl = {0};
    flags_into(a, &fl, flags);
    for (size_t i = 0; i < fl.n; i++) {
        const char *flag = fl.v[i];
        if (!strcmp(flag, "-I-") || !strcmp(flag, "-i-") || proj_startswith(flag, "-ir ")) continue;
        bool done = false;
        for (size_t k = 0; k < sizeof REPLACE_PREFIX / sizeof REPLACE_PREFIX[0] && !done; k++) {
            if (proj_startswith(flag, REPLACE_PREFIX[k][0])) {
                avp_json_arr_push(a, out, avp_json_strz(a, avp_sprintf(a, "%s%s", REPLACE_PREFIX[k][1], flag + strlen(REPLACE_PREFIX[k][0]))));
                done = true;
            }
        }
        for (size_t k = 0; k < sizeof REPLACE_OPTIONS / sizeof REPLACE_OPTIONS[0] && !done; k++) {
            const ReplaceOptions *ro = &REPLACE_OPTIONS[k];
            if (!proj_startswith(flag, ro->prefix)) continue;
            AvpStr option = avp_str_lstrip(avp_str_removeprefix(avp_str(flag + strlen(ro->prefix)), AVP_S("=")));
            for (size_t m = 0; m < 4 && ro->options[m][0]; m++) {
                if (avp_str_eqz(option, ro->options[m][0])) {
                    for (size_t r = 1; r < 3 && ro->options[m][r]; r++) avp_json_arr_push(a, out, avp_json_strz(a, ro->options[m][r]));
                    break;
                }
            }
            done = true;
        }
        if (done) continue;
        if (proj_startswith(flag, "-I") || proj_startswith(flag, "-D")) avp_json_arr_push(a, out, avp_json_strz(a, flag));
    }
}

/* default_format for a Path: o.resolve().as_posix() */
static AvpJson *resolved(AvpArena *a, const char *p) {
    return avp_json_strz(a, unix_path(a, proj_path(a, avp_path_realpath(a, p))));
}

void proj_generate_compile_commands(PCtx *c) {
    AvpArena *a = c->a;
    AvpJson *bc = c->build_config;
    if (!bc || !pv_truthy(proj_attr(c, "generate_compile_commands"))) return;
    AvpJson *doc = avp_json_arr(a);
    const char *cwd = proj_path(a, avp_getcwd(a));
    AVP_VEC(AvpJson *) lists = {0};
    avp_vec_push(&lists, avp_json_get(bc, AVP_S("units")));
    AvpJson *modules = avp_json_get(bc, AVP_S("modules"));
    for (size_t m = 0; m < avp_json_len(modules); m++) avp_vec_push(&lists, avp_json_get(avp_json_at(modules, m), AVP_S("units")));
    for (size_t l = 0; l < lists.n; l++) {
        for (size_t i = 0; i < avp_json_len(lists.v[l]); i++) {
            PObj *obj = proj_object(c, avp_json_get_str(avp_json_at(lists.v[l], i), "name", AVP_S("")));
            if (!obj || !obj->src_path || !obj->src_obj_path) continue;
            if (!proj_file_is_c(obj->src_path) && !proj_file_is_cpp(obj->src_path)) continue;
            AvpJson *cflags = avp_json_arr(a);
            append_cflags(a, cflags, pv_dict_get(obj->options, AVP_S("cflags")));
            append_cflags(a, cflags, pv_dict_get(obj->options, AVP_S("extra_cflags")));
            Pv *extra = proj_attr(c, "extra_clang_flags");
            for (size_t k = 0; k < pv_len(extra); k++) avp_json_arr_push(a, cflags, pv_json(a, extra->u.list->v[k]));
            Pv *oextra = pv_dict_get(obj->options, AVP_S("extra_clang_flags"));
            for (size_t k = 0; k < pv_len(oextra); k++) avp_json_arr_push(a, cflags, pv_json(a, oextra->u.list->v[k]));
            AvpJson *unit = avp_json_obj(a);
            avp_json_set(a, unit, AVP_S("directory"), resolved(a, cwd));
            avp_json_set(a, unit, AVP_S("file"), resolved(a, obj->src_path));
            avp_json_set(a, unit, AVP_S("output"), resolved(a, obj->src_obj_path));
            AvpJson *args = avp_json_arr(a);
            static const char *const head[] = {"clang", "-nostdinc", "-fno-builtin", "--target=powerpc-eabi"};
            for (size_t k = 0; k < 4; k++) avp_json_arr_push(a, args, avp_json_strz(a, head[k]));
            for (size_t k = 0; k < avp_json_len(cflags); k++) avp_json_arr_push(a, args, avp_json_at(cflags, k));
            avp_json_arr_push(a, args, avp_json_strz(a, "-c"));
            avp_json_arr_push(a, args, resolved(a, obj->src_path));
            avp_json_arr_push(a, args, avp_json_strz(a, "-o"));
            avp_json_arr_push(a, args, resolved(a, obj->src_obj_path));
            avp_json_set(a, unit, AVP_S("arguments"), args);
            avp_json_arr_push(a, doc, unit);
        }
    }
    write_json(a, "compile_commands.json", doc);
}
