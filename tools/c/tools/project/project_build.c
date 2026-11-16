/* project.py: Object.resolve, ProjectConfig.validate / objects / out_path / compilers, load_build_config,
 * generate_build and generate_build_ninja. Statement order follows the Python, so the output order does. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "project_int.h"

/* ---- ProjectConfig ---- */

void proj_validate(PCtx *c) {
    static const char *const required[] = {"build_dir", "src_dir", "tools_dir", "check_sha_path", "config_path",
                                           "ldflags", "linker_version", "libs", "version"};
    for (size_t i = 0; i < sizeof required / sizeof required[0]; i++)
        if (proj_attr(c, required[i])->kind == PV_NONE) proj_exit("ProjectConfig.%s missing", required[i]);
}

const char *proj_out_path(const PCtx *c) {
    return proj_join(c->a, proj_pv_path(c->a, proj_attr(c, "build_dir")), proj_pv_str(c->a, proj_attr(c, "version")));
}

static const char *cfg_path(const PCtx *c, const char *name) { return proj_pv_path(c->a, proj_attr(c, name)); }

static const char *compilers_dir(const PCtx *c) {
    if (pv_truthy(proj_attr(c, "compilers_path"))) return cfg_path(c, "compilers_path");
    if (pv_truthy(proj_attr(c, "compilers_tag"))) return proj_join(c->a, cfg_path(c, "build_dir"), "compilers");
    proj_exit("ProjectConfig.compilers_tag missing");
}

/* ---- Object.resolve and ProjectConfig.objects ---- */

static void set_default(AvpArena *a, Pv *opts, const char *key, Pv *value) {
    Pv *v = pv_dict_get(opts, avp_str(key));
    if (!v || v->kind == PV_NONE) pv_dict_set(a, opts, avp_str(key), value);
}

static void check_category(const PCtx *c, const Pv *category) {
    Pv *cats = proj_attr(c, "progress_categories");
    if (cats->kind == PV_LIST || cats->kind == PV_TUPLE)
        for (size_t i = 0; i < cats->u.list->n; i++) {
            Pv *id = pv_getattr(cats->u.list->v[i], "id");
            if (id && pv_eq(category, id)) return;
        }
    proj_exit("Progress category '%s' missing from config.progress_categories", proj_pv_str(c->a, category));
}

static PObj *resolve(const PCtx *c, Pv *self, Pv *lib) {
    AvpArena *a = c->a;
    Pv *name = pv_getattr(self, "name");
    bool completed = pv_truthy(pv_getattr(self, "completed"));
    Pv *obj = project_new_object(a, completed, name->u.s, lib);
    Pv *opts = pv_getattr(obj, "options");
    Pv *own = pv_getattr(self, "options");
    for (size_t i = 0; i < own->u.dict->n; i++) {
        AvpStr key = own->u.dict->keys[i];
        Pv *value = own->u.dict->vals[i];
        if (value->kind != PV_NONE || !pv_dict_get(opts, key)) pv_dict_set(a, opts, key, value);
    }
    set_default(a, opts, "add_to_all", pv_bool(true));
    set_default(a, opts, "asflags", proj_attr(c, "asflags"));
    set_default(a, opts, "asm_dir", proj_attr(c, "asm_dir"));
    set_default(a, opts, "extab_padding", pv_none());
    set_default(a, opts, "mw_version", proj_attr(c, "linker_version"));
    set_default(a, opts, "scratch_preset_id", proj_attr(c, "scratch_preset_id"));
    set_default(a, opts, "shift_jis", proj_attr(c, "shift_jis"));
    set_default(a, opts, "src_dir", proj_attr(c, "src_dir"));

    Pv *pc = pv_dict_get(opts, AVP_S("progress_category"));
    if (pc->kind == PV_LIST) {
        for (size_t i = 0; i < pc->u.list->n; i++) check_category(c, pc->u.list->v[i]);
    } else if (pc->kind != PV_NONE) {
        check_category(c, pc);
    }

    PObj *o = avp_alloc(a, sizeof *o);
    o->inst = obj;
    o->options = opts;
    o->name = avp_cz(a, name->u.s);
    o->completed = completed;
    const char *build_dir = proj_out_path(c);
    const char *source = proj_pv_path(a, pv_dict_get(opts, AVP_S("source")));
    o->src_path = proj_join(a, proj_pv_path(a, pv_dict_get(opts, AVP_S("src_dir"))), source);
    Pv *asm_dir = pv_dict_get(opts, AVP_S("asm_dir"));
    if (asm_dir->kind != PV_NONE) o->asm_path = proj_with_suffix(a, proj_join(a, proj_pv_path(a, asm_dir), source), ".s");
    const char *base_name = proj_with_suffix(a, proj_path(a, o->name), "");
    o->src_obj_path = proj_join(a, proj_join(a, build_dir, "src"), avp_sprintf(a, "%s.o", base_name));
    o->asm_obj_path = proj_join(a, proj_join(a, build_dir, "mod"), avp_sprintf(a, "%s.o", base_name));
    o->ctx_path = proj_join(a, proj_join(a, build_dir, "src"), avp_sprintf(a, "%s.ctx", base_name));
    return o;
}

void proj_resolve_objects(PCtx *c) {
    c->objects = avp_map_new();
    Pv *libs = proj_attr(c, "libs");
    if (libs->kind != PV_LIST && libs->kind != PV_TUPLE) return; /* `self.libs or {}` */
    for (size_t i = 0; i < libs->u.list->n; i++) {
        Pv *lib = libs->u.list->v[i];
        Pv *objects = pv_dict_get(lib, AVP_S("objects"));
        if (!objects) proj_exit("KeyError: 'objects'");
        for (size_t k = 0; k < objects->u.list->n; k++) {
            Pv *obj = objects->u.list->v[k];
            AvpStr name = pv_getattr(obj, "name")->u.s;
            if (avp_map_has(c->objects, name)) proj_exit("Duplicate object name %.*s", AVP_SF(name));
            avp_map_put(c->objects, name, NULL)->p = resolve(c, obj, lib);
        }
    }
}

/* ---- load_build_config ---- */

static bool versiontuple_lt(AvpArena *a, AvpStr x, AvpStr y) {
    AvpStrVec xs = avp_str_split(x, AVP_S("."), -1), ys = avp_str_split(y, AVP_S("."), -1);
    for (size_t i = 0;; i++) {
        if (i >= xs.n || i >= ys.n) return xs.n < ys.n;
        i64 xi, yi;
        bool ov;
        if (!avp_parse_int(avp_str_strip(xs.v[i]), 10, &xi, &ov))
            proj_exit("ValueError: invalid literal for int() with base 10: %s", avp_cz(a, avp_repr_str(a, xs.v[i])));
        if (!avp_parse_int(avp_str_strip(ys.v[i]), 10, &yi, &ov))
            proj_exit("ValueError: invalid literal for int() with base 10: %s", avp_cz(a, avp_repr_str(a, ys.v[i])));
        if (xi != yi) return xi < yi;
    }
}

void proj_load_build_config(PCtx *c, const char *path) {
    AvpArena *a = c->a;
    c->build_config = NULL;
    if (!avp_isfile(path)) return;
    AvpJson *bc = proj_load_json(a, path);
    AvpJson *version = avp_json_is_obj(bc) ? avp_json_get(bc, AVP_S("version")) : NULL;
    if (!version || avp_json_is_null(version)) {
        printf("Invalid config.json, regenerating...\n");
        avp_remove(path);
        return;
    }
    const char *dtk_tag = proj_pv_str(a, proj_attr(c, "dtk_tag"));
    AvpStr dtk_version = avp_str_slice(avp_str(dtk_tag), 1, AVP_END);
    if (!avp_json_is_str(version)) proj_exit("AttributeError: config.json version is not a str");
    if (versiontuple_lt(a, avp_json_str_or(version, AVP_S("")), dtk_version)) {
        printf("Outdated config.json, regenerating...\n");
        avp_remove(path);
        return;
    }
    if (pv_truthy(proj_attr(c, "link_order_callback")))
        proj_exit("configure: error: config.link_order_callback is not supported by the C port (DEVIATIONS.md)");
    c->build_config = bc;
}

/* ---- generate_build_ninja ---- */

typedef struct {
    AvpStr name;
    i64 module_id;
    const char *ldscript;
    AvpJson *entry;
    PStrs inputs;
} LinkStep;

typedef struct {
    PCtx *c;
    AvpArena *a;
    NinjaWriter *n;
    const char *build_path;
    const char *dtk, *decompctx;
    PStrs mwcc_implicit, mwcc_sjis_implicit, mwcc_pch_implicit, mwcc_pch_sjis_implicit, mwcc_extab_implicit,
        mwcc_sjis_extab_implicit, mwld_implicit, gnu_as_implicit;
    PStrs used_compiler_versions; /* a set in the Python; insertion order here (DEVIATIONS.md) */
    PStrs source_inputs;
    AvpSet *source_added; /* lower-cased paths: Path equality on Windows */
} Gen;

/* A C tool next to configure.exe: its absolute path, and that path quoted (for a command line). */
static const char *ctool_path(const PCtx *c, const char *exe) { return avp_sprintf(c->a, "%s\\%s", c->env->ctools, exe); }
static const char *ctool_cmd(const PCtx *c, const char *exe) { return avp_sprintf(c->a, "\"%s\"", ctool_path(c, exe)); }

static NinjaPaths strs(AvpArena *a, const PStrs *v) { return ninja_list(a, v->n, v->v); }

static PStrs strs_of(size_t n, const char *const *items) {
    PStrs v = {0};
    for (size_t i = 0; i < n; i++) avp_vec_push(&v, items[i]);
    return v;
}
#define STRS(...) strs_of(sizeof((const char *[]){__VA_ARGS__}) / sizeof(const char *), (const char *[]){__VA_ARGS__})

static PStrs strs_plus(const PStrs *base, const char *extra) {
    PStrs v = strs_of(base->n, base->v);
    avp_vec_push(&v, extra);
    return v;
}

static const char *map_path(AvpArena *a, const char *path) {
    return proj_join(a, avp_cz(a, pv_path_parent(a, avp_str(path))), avp_sprintf(a, "%s.MAP", avp_cz(a, pv_path_name(avp_str(path)))));
}

static const char *link_output(Gen *g, const LinkStep *s) {
    if (s->module_id == 0) return proj_join(g->a, g->build_path, avp_sprintf(g->a, "%.*s.dol", AVP_SF(s->name)));
    return proj_join(g->a, proj_join(g->a, g->build_path, avp_cz(g->a, s->name)), avp_sprintf(g->a, "%.*s.rel", AVP_SF(s->name)));
}

static const char *link_partial_output(Gen *g, const LinkStep *s) {
    if (s->module_id == 0) return proj_join(g->a, g->build_path, avp_sprintf(g->a, "%.*s.elf", AVP_SF(s->name)));
    return proj_join(g->a, proj_join(g->a, g->build_path, avp_cz(g->a, s->name)), avp_sprintf(g->a, "%.*s.plf", AVP_SF(s->name)));
}

static LinkStep *link_step_new(Gen *g, AvpJson *module) {
    LinkStep *s = avp_alloc(g->a, sizeof *s);
    s->name = avp_json_get_str(module, "name", AVP_S(""));
    s->module_id = avp_json_get_i64(module, "module_id", 0);
    s->ldscript = proj_path(g->a, avp_cz(g->a, avp_json_get_str(module, "ldscript", AVP_S(""))));
    s->entry = avp_json_get(module, AVP_S("entry"));
    return s;
}

static void link_step_write(Gen *g, LinkStep *s) {
    AvpArena *a = g->a;
    NinjaWriter *n = g->n;
    bool map = pv_truthy(proj_attr(g->c, "generate_map"));
    ninja_comment(n, avp_sprintf(a, "Link %.*s", AVP_SF(s->name)));
    if (s->module_id == 0) {
        const char *elf_path = proj_join(a, g->build_path, avp_sprintf(a, "%.*s.elf", AVP_SF(s->name)));
        const char *elf_ldflags = avp_sprintf(a, "$ldflags -lcf %s", s->ldscript);
        const char *elf_map = NULL;
        if (map) {
            elf_map = map_path(a, elf_path);
            elf_ldflags = avp_sprintf(a, "%s -map %s", elf_ldflags, elf_map);
        }
        PStrs implicit = STRS(s->ldscript);
        for (size_t i = 0; i < g->mwld_implicit.n; i++) avp_vec_push(&implicit, g->mwld_implicit.v[i]);
        NinjaVar vars[] = {{"ldflags", elf_ldflags}};
        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, elf_path), .rule = "link", .inputs = strs(a, &s->inputs),
                                     .implicit = strs(a, &implicit), .implicit_outputs = ninja_one(a, elf_map),
                                     .vars = vars, .nvars = 1, .order_only = ninja_one(a, "post-compile")});
    } else {
        const char *dir = proj_join(a, g->build_path, avp_cz(a, s->name));
        const char *preplf_path = proj_join(a, dir, avp_sprintf(a, "%.*s.preplf", AVP_SF(s->name)));
        const char *plf_path = proj_join(a, dir, avp_sprintf(a, "%.*s.plf", AVP_SF(s->name)));
        const char *preplf_ldflags = "$ldflags -sdata 0 -sdata2 0 -r";
        const char *plf_ldflags = avp_sprintf(a, "$ldflags -sdata 0 -sdata2 0 -r1 -lcf %s", s->ldscript);
        if (avp_json_truthy(s->entry)) {
            plf_ldflags = avp_sprintf(a, "%s -m %s", plf_ldflags, avp_cz(a, avp_json_str_or(s->entry, AVP_S(""))));
            if (pv_truthy(proj_attr(g->c, "rel_strip_partial"))) plf_ldflags = avp_sprintf(a, "%s -strip_partial", plf_ldflags);
        }
        const char *preplf_map = NULL, *plf_map = NULL;
        if (map) {
            preplf_map = map_path(a, preplf_path);
            preplf_ldflags = avp_sprintf(a, "%s -map %s", preplf_ldflags, preplf_map);
            plf_map = map_path(a, plf_path);
            plf_ldflags = avp_sprintf(a, "%s -map %s", plf_ldflags, plf_map);
        }
        NinjaVar v1[] = {{"ldflags", preplf_ldflags}};
        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, preplf_path), .rule = "link", .inputs = strs(a, &s->inputs),
                                     .implicit = strs(a, &g->mwld_implicit), .implicit_outputs = ninja_one(a, preplf_map),
                                     .vars = v1, .nvars = 1, .order_only = ninja_one(a, "post-compile")});
        PStrs implicit = STRS(s->ldscript, preplf_path);
        for (size_t i = 0; i < g->mwld_implicit.n; i++) avp_vec_push(&implicit, g->mwld_implicit.v[i]);
        NinjaVar v2[] = {{"ldflags", plf_ldflags}};
        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, plf_path), .rule = "link", .inputs = strs(a, &s->inputs),
                                     .implicit = strs(a, &implicit), .implicit_outputs = ninja_one(a, plf_map),
                                     .vars = v2, .nvars = 1, .order_only = ninja_one(a, "post-compile")});
    }
    ninja_newline(n);
}

static bool source_add(Gen *g, const char *path) {
    return avp_set_add(g->source_added, avp_str_lower(g->a, avp_str(path)));
}

static bool any_lang(const Pv *flags) {
    if (!flags || (flags->kind != PV_LIST && flags->kind != PV_TUPLE)) return false;
    for (size_t i = 0; i < flags->u.list->n; i++) {
        const Pv *f = flags->u.list->v[i];
        if (f->kind == PV_STR && avp_str_startswith(f->u.s, AVP_S("-lang"))) return true;
    }
    return false;
}

static void list_insert0(AvpArena *a, Pv *list, Pv *item) {
    pv_list_push(a, list, item);
    PvList *l = list->u.list;
    memmove(l->v + 1, l->v, (l->n - 1) * sizeof *l->v);
    l->v[0] = item;
}

static Pv *list_concat(AvpArena *a, const Pv *x, const Pv *y) {
    if (!x || (x->kind != PV_LIST)) proj_exit("TypeError: unsupported operand type(s) for +: '%s' and 'list'", x ? pv_kind_name(x) : "NoneType");
    if (!y || (y->kind != PV_LIST)) proj_exit("TypeError: can only concatenate list (not \"%s\") to list", y ? pv_kind_name(y) : "NoneType");
    Pv *r = pv_list_copy(a, x);
    pv_list_extend(a, r, y);
    return r;
}

static const char *extab_hex(AvpArena *a, const Pv *padding) {
    AvpBuf b;
    avp_buf_init(&b, a);
    if (padding->kind != PV_LIST && padding->kind != PV_TUPLE) proj_exit("TypeError: extab_padding must be a list of ints");
    for (size_t i = 0; i < padding->u.list->n; i++) avp_buf_printf(&b, "%02llx", (unsigned long long)padding->u.list->v[i]->u.i);
    return avp_buf_cz(&b);
}

static const char *opt_str(Gen *g, PObj *obj, const char *key) {
    return proj_pv_str(g->a, pv_dict_get(obj->options, avp_str(key)));
}

static const char *c_build(Gen *g, PObj *obj, const char *src_path) {
    AvpArena *a = g->a;
    NinjaWriter *n = g->n;
    if (!obj->src_obj_path || !source_add(g, obj->src_obj_path)) return obj->src_obj_path;
    Pv *cflags = pv_dict_get(obj->options, AVP_S("cflags"));
    Pv *extra_cflags = pv_dict_get(obj->options, AVP_S("extra_cflags"));
    if (!any_lang(cflags) && !any_lang(extra_cflags)) {
        extra_cflags = pv_list_copy(a, extra_cflags);
        pv_dict_set(a, obj->options, AVP_S("extra_cflags"), extra_cflags);
        list_insert0(a, extra_cflags, pv_strz(a, proj_file_is_cpp(src_path) ? "-lang=c++" : "-lang=c"));
    }
    Pv *all_cflags = list_concat(a, cflags, extra_cflags);
    const char *cflags_str = proj_flags_str(a, all_cflags);
    const char *mw_version = opt_str(g, obj, "mw_version");
    bool seen = false;
    for (size_t i = 0; i < g->used_compiler_versions.n; i++)
        if (!strcmp(g->used_compiler_versions.v[i], mw_version)) seen = true;
    if (!seen) avp_vec_push(&g->used_compiler_versions, mw_version);

    const char *lib_name = opt_str(g, obj, "lib");
    const char *rule = "mwcc";
    PStrs *implicit = &g->mwcc_implicit;
    NinjaVar vars[5] = {
        {"mw_version", proj_path(a, mw_version)},
        {"cflags", cflags_str},
        {"basedir", avp_cz(a, avp_path_dirname(obj->src_obj_path))},
        {"basefile", proj_with_suffix(a, obj->src_obj_path, "")},
    };
    size_t nvars = 4;
    bool sjis = pv_truthy(pv_dict_get(obj->options, AVP_S("shift_jis")));
    Pv *padding = pv_dict_get(obj->options, AVP_S("extab_padding"));
    if (sjis && padding->kind != PV_NONE) {
        rule = "mwcc_sjis_extab";
        implicit = &g->mwcc_sjis_extab_implicit;
        vars[nvars++] = (NinjaVar){"extab_padding", extab_hex(a, padding)};
    } else if (sjis) {
        rule = "mwcc_sjis";
        implicit = &g->mwcc_sjis_implicit;
    } else if (padding->kind != PV_NONE) {
        rule = "mwcc_extab";
        implicit = &g->mwcc_extab_implicit;
        vars[nvars++] = (NinjaVar){"extab_padding", extab_hex(a, padding)};
    }
    ninja_comment(n, avp_sprintf(a, "%s: %s (linked %s)", obj->name, lib_name, obj->completed ? "True" : "False"));
    ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, obj->src_obj_path), .rule = rule, .inputs = ninja_one(a, src_path),
                                 .vars = vars, .nvars = nvars, .implicit = strs(a, implicit),
                                 .order_only = ninja_one(a, "pre-compile")});

    if (obj->ctx_path) {
        AvpBuf inc, exc, def;
        avp_buf_init(&inc, a);
        avp_buf_init(&exc, a);
        avp_buf_init(&def, a);
        for (size_t i = 0; i < all_cflags->u.list->n; i++) {
            const char *flag = proj_pv_str(a, all_cflags->u.list->v[i]);
            if (proj_startswith(flag, "-i ") || proj_startswith(flag, "-I ") || proj_startswith(flag, "-I+")) {
                if (inc.n) avp_buf_putc(&inc, ' ');
                avp_buf_printf(&inc, "-I %s", avp_cz(a, avp_u8_slice(avp_str(flag), 3, AVP_END)));
            }
        }
        Pv *globs = proj_attr(g->c, "context_exclude_globs"), *defines = proj_attr(g->c, "context_defines");
        for (size_t i = 0; i < pv_len(globs); i++) {
            if (exc.n) avp_buf_putc(&exc, ' ');
            avp_buf_printf(&exc, "-x %s", proj_pv_str(a, globs->u.list->v[i]));
        }
        for (size_t i = 0; i < pv_len(defines); i++) {
            if (def.n) avp_buf_putc(&def, ' ');
            avp_buf_printf(&def, "-D %s", proj_pv_str(a, defines->u.list->v[i]));
        }
        NinjaVar cv[] = {{"includes", avp_buf_cz(&inc)}, {"excludes", avp_buf_cz(&exc)}, {"defines", avp_buf_cz(&def)}};
        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, obj->ctx_path), .rule = "decompctx",
                                     .inputs = ninja_one(a, src_path), .implicit = ninja_one(a, g->decompctx),
                                     .vars = cv, .nvars = 3});
    }
    ninja_newline(n);
    if (pv_truthy(pv_dict_get(obj->options, AVP_S("add_to_all")))) avp_vec_push(&g->source_inputs, obj->src_obj_path);
    return obj->src_obj_path;
}

static const char *asm_build(Gen *g, PObj *obj, const char *src_path, const char *obj_path) {
    AvpArena *a = g->a;
    Pv *asflags = pv_dict_get(obj->options, AVP_S("asflags"));
    if (asflags->kind == PV_NONE) proj_exit("ProjectConfig.asflags missing");
    const char *asflags_str = proj_flags_str(a, asflags);
    Pv *extra = pv_dict_get(obj->options, AVP_S("extra_asflags"));
    if (pv_len(extra) > 0) asflags_str = avp_sprintf(a, "%s %s", asflags_str, proj_flags_str(a, extra));
    if (!obj_path || !source_add(g, obj_path)) return obj_path;
    ninja_comment(g->n, avp_sprintf(a, "%s: %s (linked %s)", obj->name, opt_str(g, obj, "lib"), obj->completed ? "True" : "False"));
    NinjaVar vars[] = {{"asflags", asflags_str}};
    ninja_build(g->n, &(NinjaBuild){.outputs = ninja_one(a, obj_path), .rule = "as", .inputs = ninja_one(a, src_path),
                                    .vars = vars, .nvars = 1, .implicit = strs(a, &g->gnu_as_implicit),
                                    .order_only = ninja_one(a, "pre-compile")});
    ninja_newline(g->n);
    if (pv_truthy(pv_dict_get(obj->options, AVP_S("add_to_all")))) avp_vec_push(&g->source_inputs, obj_path);
    return obj_path;
}

static void add_unit(Gen *g, AvpJson *build_obj, LinkStep *step) {
    AvpArena *a = g->a;
    AvpJson *obj_path_j = avp_json_get(build_obj, AVP_S("object"));
    const char *obj_path = avp_json_is_null(obj_path_j) ? NULL : avp_cz(a, avp_json_str_or(obj_path_j, AVP_S("")));
    AvpStr obj_name = avp_json_get_str(build_obj, "name", AVP_S(""));
    PObj *obj = proj_object(g->c, obj_name);
    if (!obj) {
        if (pv_truthy(proj_attr(g->c, "warn_missing_config")) && !avp_json_truthy(avp_json_get(build_obj, AVP_S("autogenerated"))))
            printf("Missing configuration for %.*s\n", AVP_SF(obj_name));
        if (obj_path) avp_vec_push(&step->inputs, proj_path(a, obj_path));
        return;
    }
    bool link_built_obj = obj->completed;
    const char *built_obj_path = NULL;
    if (obj->src_path && avp_exists(obj->src_path)) {
        proj_check_path_case(a, obj->src_path);
        if (proj_file_is_c(obj->src_path) || proj_file_is_cpp(obj->src_path)) built_obj_path = c_build(g, obj, obj->src_path);
        else if (proj_file_is_asm(obj->src_path)) built_obj_path = asm_build(g, obj, obj->src_path, obj->src_obj_path);
        else proj_exit("Unknown source file type %s", obj->src_path);
    } else {
        if (pv_truthy(proj_attr(g->c, "warn_missing_source")) || obj->completed)
            printf("Missing source file %s\n", obj->src_path ? obj->src_path : "None");
        link_built_obj = false;
    }
    if (!link_built_obj && obj->asm_path && avp_exists(obj->asm_path)) {
        proj_check_path_case(a, obj->asm_path);
        link_built_obj = true;
        built_obj_path = asm_build(g, obj, obj->asm_path, obj->asm_obj_path);
    }
    if (link_built_obj && built_obj_path) avp_vec_push(&step->inputs, built_obj_path);
    else if (obj_path) avp_vec_push(&step->inputs, proj_path(a, obj_path));
}

static const char *pv_get_str(AvpArena *a, const Pv *d, const char *key) {
    Pv *v = pv_dict_get(d, avp_str(key));
    return v && pv_truthy(v) ? proj_pv_str(a, v) : NULL;
}

static void write_custom_rules(Gen *g) {
    AvpArena *a = g->a;
    Pv *rules = proj_attr(g->c, "custom_build_rules");
    if (pv_len(rules) > 0) ninja_comment(g->n, "Custom project build rules (pre/post-processing)");
    if (rules->kind != PV_LIST && rules->kind != PV_TUPLE) return;
    for (size_t i = 0; i < rules->u.list->n; i++) {
        Pv *r = rules->u.list->v[i];
        Pv *name = pv_dict_get(r, AVP_S("name")), *command = pv_dict_get(r, AVP_S("command"));
        ninja_rule(g->n, &(NinjaRule){.name = name ? proj_pv_str(a, name) : "None",
                                      .command = command ? proj_pv_str(a, command) : "None",
                                      .description = pv_get_str(a, r, "description"),
                                      .depfile = pv_get_str(a, r, "depfile"),
                                      .generator = pv_truthy(pv_dict_get(r, AVP_S("generator")) ? pv_dict_get(r, AVP_S("generator")) : pv_bool(false)),
                                      .pool = pv_get_str(a, r, "pool"),
                                      .restat = pv_truthy(pv_dict_get(r, AVP_S("restat")) ? pv_dict_get(r, AVP_S("restat")) : pv_bool(false)),
                                      .rspfile = pv_get_str(a, r, "rspfile"),
                                      .rspfile_content = pv_get_str(a, r, "rspfile_content"),
                                      .deps = pv_get_str(a, r, "deps")});
        ninja_newline(g->n);
    }
}

static NinjaPaths pv_paths_of(AvpArena *a, const Pv *d, const char *key) { return proj_pv_ninja(a, pv_dict_get(d, avp_str(key))); }

static void write_custom_step(Gen *g, const char *step, const char *prev_step, const PStrs *extra_inputs) {
    AvpArena *a = g->a;
    PStrs implicit = {0};
    Pv *steps = proj_attr(g->c, "custom_build_steps");
    Pv *list = pv_truthy(steps) && steps->kind == PV_DICT ? pv_dict_get(steps, avp_str(step)) : NULL;
    if (list) {
        ninja_comment(g->n, avp_sprintf(a, "Custom build steps (%s)", step));
        for (size_t i = 0; i < pv_len(list); i++) {
            Pv *cs = list->u.list->v[i];
            Pv *outputs = pv_dict_get(cs, AVP_S("outputs"));
            if (outputs && outputs->kind == PV_LIST) {
                for (size_t k = 0; k < outputs->u.list->n; k++)
                    avp_vec_push(&implicit, pv_truthy(outputs->u.list->v[k]) ? proj_pv_str(a, outputs->u.list->v[k]) : NULL);
            } else {
                avp_vec_push(&implicit, outputs && pv_truthy(outputs) ? proj_pv_str(a, outputs) : NULL);
            }
            AVP_VEC(NinjaVar) vars = {0};
            Pv *vv = pv_dict_get(cs, AVP_S("variables"));
            if (vv && vv->kind == PV_DICT) {
                for (size_t k = 0; k < vv->u.dict->n; k++) {
                    NinjaVar nv = {avp_cz(a, vv->u.dict->keys[k]), ninja_join(a, proj_pv_ninja(a, vv->u.dict->vals[k]))};
                    avp_vec_push(&vars, nv);
                }
            } else if (vv && (vv->kind == PV_LIST || vv->kind == PV_TUPLE)) {
                for (size_t k = 0; k < vv->u.list->n; k++) {
                    Pv *kv = vv->u.list->v[k];
                    NinjaVar nv = {proj_pv_str(a, kv->u.list->v[0]), ninja_join(a, proj_pv_ninja(a, kv->u.list->v[1]))};
                    avp_vec_push(&vars, nv);
                }
            }
            Pv *pool = pv_dict_get(cs, AVP_S("pool")), *dyndep = pv_dict_get(cs, AVP_S("dyndep"));
            ninja_build(g->n, &(NinjaBuild){.outputs = proj_pv_ninja(a, outputs), .rule = pv_get_str(a, cs, "rule"),
                                            .inputs = pv_paths_of(a, cs, "inputs"), .implicit = pv_paths_of(a, cs, "implicit"),
                                            .order_only = pv_paths_of(a, cs, "order_only"), .vars = vars.v, .nvars = vars.n,
                                            .implicit_outputs = pv_paths_of(a, cs, "implicit_outputs"),
                                            .pool = pool && pool->kind != PV_NONE ? proj_pv_str(a, pool) : NULL,
                                            .dyndep = dyndep && dyndep->kind != PV_NONE ? (pv_truthy(dyndep) ? proj_pv_str(a, dyndep) : "") : NULL});
            ninja_newline(g->n);
        }
    }
    ninja_build(g->n, &(NinjaBuild){.outputs = ninja_one(a, step), .rule = "phony", .inputs = strs(a, &implicit),
                                    .order_only = ninja_one(a, prev_step),
                                    .implicit = extra_inputs ? strs(a, extra_inputs) : ninja_none()});
}

static int cmp_cstr(const void *x, const void *y) {
    return avp_str_cmp(avp_str(*(const char *const *)x), avp_str(*(const char *const *)y));
}

typedef struct {
    const char *dirpath;
    PStrs files;
} WalkDir;

static int cmp_walkdir(const void *x, const void *y) {
    return avp_str_cmp(avp_str(((const WalkDir *)x)->dirpath), avp_str(((const WalkDir *)y)->dirpath));
}

/* for dirpath, _dirs, files in sorted(os.walk(top)): Path(dirpath) / f for f in sorted(files) */
static void walk_sorted(AvpArena *a, const char *top, PStrs *out) {
    AVP_VEC(WalkDir) dirs = {0};
    AvpWalk *w = avp_walk_open(top, true);
    AvpWalkItem item = {0};
    while (w && avp_walk_next(w, &item)) {
        WalkDir d = {avp_cz(a, item.dirpath), {0}};
        for (size_t i = 0; i < item.files.n; i++) avp_vec_push(&d.files, avp_cz(a, item.files.v[i]));
        avp_vec_push(&dirs, d);
    }
    if (w) avp_walk_close(w);
    if (dirs.n) qsort(dirs.v, dirs.n, sizeof *dirs.v, cmp_walkdir);
    for (size_t i = 0; i < dirs.n; i++) {
        if (dirs.v[i].files.n) qsort(dirs.v[i].files.v, dirs.v[i].files.n, sizeof(const char *), cmp_cstr);
        for (size_t k = 0; k < dirs.v[i].files.n; k++) avp_vec_push(out, proj_join(a, dirs.v[i].dirpath, dirs.v[i].files.v[k]));
    }
}

static bool json_str_in(AvpJson *list, AvpStr s) {
    for (size_t i = 0; i < avp_json_len(list); i++)
        if (avp_str_eq(avp_json_str_or(avp_json_at(list, i), AVP_S("")), s)) return true;
    return false;
}

void proj_generate_build_ninja(PCtx *c) {
    AvpArena *a = c->a;
    const ProjectEnv *env = c->env;
    AvpBuf out;
    avp_buf_init(&out, a);
    NinjaWriter nw;
    ninja_writer_init(&nw, a, &out);
    NinjaWriter *n = &nw;
    Gen g0 = {0}, *g = &g0;
    g->c = c;
    g->a = a;
    g->n = n;
    g->source_added = avp_set_new();

    ninja_variable(n, "ninja_required_version", "1.3", 0);
    ninja_newline(n);

    const char *configure_script = proj_relpath(a, env->script);
    /* configure.exe's own options before the script's arguments: --script when it is not ./configure.py */
    const char *configure_own =
        strcmp(configure_script, "configure.py") ? avp_sprintf(a, " --script %s", proj_cmd_path(a, configure_script)) : "";
    ninja_comment(n, "The arguments passed to configure.py, for rerunning it.");
    ninja_variable(n, "configure_args", ninja_escape(a, proj_list2cmdline(a, env->argc, env->argv)), 0);
    ninja_newline(n);

    ninja_comment(n, "Variables");
    ninja_variable(n, "ldflags", proj_flags_str(a, proj_attr(c, "ldflags")), 0);
    if (proj_attr(c, "linker_version")->kind == PV_NONE) proj_exit("ProjectConfig.linker_version missing");
    ninja_variable(n, "mw_version", cfg_path(c, "linker_version"), 0);
    ninja_variable(n, "objdiff_report_args", proj_flags_str(a, proj_attr(c, "progress_report_args")), 0);
    ninja_newline(n);

    ninja_comment(n, "Tooling");
    const char *build_dir = cfg_path(c, "build_dir");
    const char *build_path = proj_out_path(c);
    g->build_path = build_path;
    const char *report_path = proj_join(a, build_path, "report.json");
    const char *build_tools_path = proj_join(a, build_dir, "tools");
    const char *download_tool = ctool_path(c, "download_tool.exe");
    const char *download_cmd = ctool_cmd(c, "download_tool.exe");
    ninja_rule(n, &(NinjaRule){.name = "download_tool", .command = avp_sprintf(a, "%s $tool $out --tag $tag", download_cmd),
                               .description = "TOOL $out"});

    g->decompctx = ctool_path(c, "decompctx.exe");
    const char *decompctx_cmd = ctool_cmd(c, "decompctx.exe");
    ninja_rule(n, &(NinjaRule){.name = "decompctx",
                               .command = avp_sprintf(a, "%s $in -o $out -d $out.d $includes $excludes $defines", decompctx_cmd),
                               .description = "CTX $in", .depfile = "$out.d", .deps = "gcc"});

    bool cargo_rule_written = false;
#define WRITE_CARGO_RULE()                                                                                          \
    do {                                                                                                            \
        if (!cargo_rule_written) {                                                                                  \
            ninja_pool(n, "cargo", 1);                                                                              \
            ninja_rule(n, &(NinjaRule){.name = "cargo",                                                             \
                                       .command = "cargo build --release --manifest-path $in --bin $bin --target-dir $target", \
                                       .description = "CARGO $bin", .pool = "cargo",                                 \
                                       .depfile = proj_path(a, "$target/release/$bin.d"), .deps = "gcc"});           \
            cargo_rule_written = true;                                                                              \
        }                                                                                                           \
    } while (0)

    /* dtk and objdiff: a file path, a cargo checkout, or a download */
    const char *tools_out[2];
    static const char *const tool_attr[2] = {"dtk", "objdiff"};
    static const char *const tool_bin[2] = {"dtk", "objdiff-cli"};
    for (int t = 0; t < 2; t++) {
        const char *path = cfg_path(c, avp_sprintf(a, "%s_path", tool_attr[t]));
        Pv *tag = proj_attr(c, avp_sprintf(a, "%s_tag", tool_attr[t]));
        if (path && avp_isfile(path)) {
            tools_out[t] = path;
        } else if (path) {
            tools_out[t] = proj_join(a, proj_join(a, build_tools_path, "release"), avp_sprintf(a, "%s.exe", tool_bin[t]));
            WRITE_CARGO_RULE();
            NinjaVar vars[] = {{"bin", tool_bin[t]}, {"target", build_tools_path}};
            ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, tools_out[t]), .rule = "cargo",
                                         .inputs = ninja_one(a, proj_join(a, path, "Cargo.toml")),
                                         .implicit = ninja_one(a, proj_join(a, path, "Cargo.lock")), .vars = vars, .nvars = 2});
        } else if (pv_truthy(tag)) {
            tools_out[t] = proj_join(a, build_tools_path, avp_sprintf(a, "%s.exe", tool_bin[t]));
            NinjaVar vars[] = {{"tool", tool_bin[t]}, {"tag", proj_pv_str(a, tag)}};
            ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, tools_out[t]), .rule = "download_tool",
                                         .implicit = ninja_one(a, download_tool), .vars = vars, .nvars = 2});
        } else {
            proj_exit("ProjectConfig.%s_tag missing", tool_attr[t]);
        }
    }
    const char *dtk = tools_out[0], *objdiff = tools_out[1];
    g->dtk = dtk;

    const char *sjiswrap;
    if (pv_truthy(proj_attr(c, "sjiswrap_path"))) {
        sjiswrap = cfg_path(c, "sjiswrap_path");
    } else if (pv_truthy(proj_attr(c, "sjiswrap_tag"))) {
        sjiswrap = proj_join(a, build_tools_path, "sjiswrap.exe");
        NinjaVar vars[] = {{"tool", "sjiswrap"}, {"tag", proj_pv_str(a, proj_attr(c, "sjiswrap_tag"))}};
        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, sjiswrap), .rule = "download_tool",
                                     .implicit = ninja_one(a, download_tool), .vars = vars, .nvars = 2});
    } else {
        proj_exit("ProjectConfig.sjiswrap_tag missing");
    }

    /* compiler_wrapper(): config.wrapper (use_wibo() is false on Windows) */
    Pv *wrapper_v = proj_attr(c, "wrapper");
    const char *wrapper = proj_pv_path(a, wrapper_v);
    const char *wrapper_implicit = wrapper && avp_exists(wrapper) ? wrapper : NULL;
    const char *wrapper_cmd = wrapper && pv_truthy(wrapper_v) ? avp_sprintf(a, "%s ", proj_cmd_path(a, wrapper)) : "";

    const char *compilers = compilers_dir(c);
    const char *compilers_implicit = NULL;
    if (proj_attr(c, "compilers_path")->kind == PV_NONE && proj_attr(c, "compilers_tag")->kind != PV_NONE) {
        compilers_implicit = compilers;
        NinjaVar vars[] = {{"tool", "compilers"}, {"tag", proj_pv_str(a, proj_attr(c, "compilers_tag"))}};
        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, compilers), .rule = "download_tool",
                                     .implicit = ninja_one(a, download_tool), .vars = vars, .nvars = 2});
    }

    const char *binutils_implicit = NULL, *binutils;
    if (pv_truthy(proj_attr(c, "binutils_path"))) {
        binutils = cfg_path(c, "binutils_path");
    } else if (pv_truthy(proj_attr(c, "binutils_tag"))) {
        binutils = proj_join(a, build_dir, "binutils");
        binutils_implicit = binutils;
        NinjaVar vars[] = {{"tool", "binutils"}, {"tag", proj_pv_str(a, proj_attr(c, "binutils_tag"))}};
        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, binutils), .rule = "download_tool",
                                     .implicit = ninja_one(a, download_tool), .vars = vars, .nvars = 2});
    } else {
        proj_exit("ProjectConfig.binutils_tag missing");
    }
    ninja_newline(n);

    ninja_comment(n, "Download all tools");
    ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, "tools"), .rule = "phony",
                                 .inputs = NINJA_LIST(a, dtk, sjiswrap, wrapper, compilers, binutils, objdiff)});
    ninja_newline(n);

    const char *compiler_path = proj_join(a, compilers, "$mw_version");

    const char *objcache_cmd = "";
    PStrs objcache_prehash = {0};
    if (pv_truthy(proj_attr(c, "objcache"))) {
        objcache_cmd = avp_sprintf(a, "%s run -- ", ctool_cmd(c, "objcache.exe"));
        const char *prehash_cmd = ctool_cmd(c, "objcache.exe");
        const char *prehash_stamp = proj_join(a, build_dir, "objcache-prehash.stamp");
        PStrs prehash_inputs = {0};
        walk_sorted(a, "include", &prehash_inputs);
        walk_sorted(a, "src", &prehash_inputs);
        ninja_rule(n, &(NinjaRule){.name = "objcache_prehash",
                                   .command = avp_sprintf(a, "%s prehash --compilers %s --stamp $out", prehash_cmd, proj_cmd_path(a, compilers)),
                                   .description = "OBJCACHE prehash"});
        if (compilers_implicit) avp_vec_push(&prehash_inputs, compilers_implicit);
        avp_vec_push(&prehash_inputs, sjiswrap);
        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, prehash_stamp), .rule = "objcache_prehash",
                                     .implicit = strs(a, &prehash_inputs)});
        ninja_newline(n);
        avp_vec_push(&objcache_prehash, prehash_stamp);
    }

    const char *mwcc = proj_join(a, compiler_path, "mwcceppc.exe");
    const char *mwcc_cmd = avp_sprintf(a, "%s%s%s $cflags -MMD -c $in -o $basedir", objcache_cmd, wrapper_cmd, proj_cmd_path(a, mwcc));
    g->mwcc_implicit = STRS(compilers_implicit ? compilers_implicit : mwcc, wrapper_implicit);
    const char *mwcc_sjis_cmd = avp_sprintf(a, "%s%s%s %s $cflags -MMD -c $in -o $basedir", objcache_cmd, wrapper_cmd,
                                            proj_cmd_path(a, sjiswrap), proj_cmd_path(a, mwcc));
    g->mwcc_sjis_implicit = strs_plus(&g->mwcc_implicit, sjiswrap);
    const char *mwcc_pch_cmd = avp_sprintf(a, "%s%s $cflags -MMD -c $in -o $basedir -precompile $basefilestem.mch", wrapper_cmd, proj_cmd_path(a, mwcc));
    g->mwcc_pch_implicit = strs_of(g->mwcc_implicit.n, g->mwcc_implicit.v);
    const char *mwcc_pch_sjis_cmd = avp_sprintf(a, "%s%s %s $cflags -MMD -c $in -o $basedir -precompile $basefilestem.mch",
                                                wrapper_cmd, proj_cmd_path(a, sjiswrap), proj_cmd_path(a, mwcc));
    g->mwcc_pch_sjis_implicit = strs_plus(&g->mwcc_implicit, sjiswrap);
    const char *mwcc_extab_cmd = proj_chain(a, avp_sprintf(a, "%s && %s extab clean --padding \"$extab_padding\" $out $out", mwcc_cmd, proj_cmd_path(a, dtk)));
    g->mwcc_extab_implicit = strs_plus(&g->mwcc_implicit, dtk);
    const char *mwcc_sjis_extab_cmd = proj_chain(a, avp_sprintf(a, "%s && %s extab clean --padding \"$extab_padding\" $out $out", mwcc_sjis_cmd, proj_cmd_path(a, dtk)));
    g->mwcc_sjis_extab_implicit = strs_plus(&g->mwcc_sjis_implicit, dtk);
    const char *mwld = proj_join(a, compiler_path, "mwldeppc.exe");
    const char *mwld_cmd = avp_sprintf(a, "%s%s $ldflags -o $out @$out.rsp", wrapper_cmd, proj_cmd_path(a, mwld));
    g->mwld_implicit = STRS(compilers_implicit ? compilers_implicit : mwld, wrapper_implicit);
    const char *gnu_as = proj_join(a, binutils, "powerpc-eabi-as.exe");
    const char *gnu_as_cmd = proj_chain(a, avp_sprintf(a, "%s $asflags -o $out $in && %s elf fixup $out $out", proj_cmd_path(a, gnu_as), proj_cmd_path(a, dtk)));
    g->gnu_as_implicit = STRS(binutils_implicit ? binutils_implicit : gnu_as, dtk, proj_join(a, proj_join(a, build_path, "include"), "macros.inc"));

    ninja_comment(n, "Link ELF file");
    ninja_rule(n, &(NinjaRule){.name = "link", .command = mwld_cmd, .description = "LINK $out", .rspfile = "$out.rsp",
                               .rspfile_content = "$in_newline"});
    ninja_newline(n);
    ninja_comment(n, "Generate DOL");
    ninja_rule(n, &(NinjaRule){.name = "elf2dol", .command = avp_sprintf(a, "%s elf2dol $in $out", proj_cmd_path(a, dtk)), .description = "DOL $out"});
    ninja_newline(n);
    ninja_comment(n, "MWCC build");
    ninja_rule(n, &(NinjaRule){.name = "mwcc", .command = mwcc_cmd, .description = "MWCC $out", .depfile = "$basefile.d", .deps = "gcc"});
    ninja_newline(n);
    ninja_comment(n, "MWCC build (with UTF-8 to Shift JIS wrapper)");
    ninja_rule(n, &(NinjaRule){.name = "mwcc_sjis", .command = mwcc_sjis_cmd, .description = "MWCC $out", .depfile = "$basefile.d", .deps = "gcc"});
    ninja_newline(n);
    ninja_comment(n, "MWCC build (with extab post-processing)");
    ninja_rule(n, &(NinjaRule){.name = "mwcc_extab", .command = mwcc_extab_cmd, .description = "MWCC $out", .depfile = "$basefile.d", .deps = "gcc"});
    ninja_newline(n);
    ninja_comment(n, "MWCC build (with UTF-8 to Shift JIS wrapper and extab post-processing)");
    ninja_rule(n, &(NinjaRule){.name = "mwcc_sjis_extab", .command = mwcc_sjis_extab_cmd, .description = "MWCC $out", .depfile = "$basefile.d", .deps = "gcc"});
    ninja_comment(n, "Assemble asm");
    ninja_rule(n, &(NinjaRule){.name = "as", .command = gnu_as_cmd, .description = "AS $out"});
    ninja_newline(n);
    ninja_comment(n, "Build precompiled header");
    ninja_rule(n, &(NinjaRule){.name = "mwcc_pch", .command = mwcc_pch_cmd, .description = "PCH $out", .depfile = "$basefile.d", .deps = "gcc"});
    ninja_newline(n);
    ninja_comment(n, "Build precompiled header (with UTF-8 to Shift JIS wrapper)");
    ninja_rule(n, &(NinjaRule){.name = "mwcc_pch_sjis", .command = mwcc_pch_sjis_cmd, .description = "PCH $out", .depfile = "$basefile.d", .deps = "gcc"});
    ninja_newline(n);

    write_custom_rules(g);

    /* pre-compile: the precompiled headers and the object cache's prehash stamp */
    Pv *pchs = proj_attr(c, "precompiled_headers");
    PStrs pre_inputs = {0};
    for (size_t i = 0; i < pv_len(pchs) && pchs->kind != PV_DICT; i++) {
        Pv *pch = pchs->u.list->v[i];
        const char *mch = proj_with_suffix(a, proj_pv_path(a, pv_dict_get(pch, AVP_S("source"))), ".mch");
        avp_vec_push(&pre_inputs, avp_path_abspath(a, proj_join(a, proj_join(a, build_path, "include"), mch)));
    }
    for (size_t i = 0; i < objcache_prehash.n; i++) avp_vec_push(&pre_inputs, objcache_prehash.v[i]);
    write_custom_step(g, "pre-compile", NULL, &pre_inputs);

    ninja_comment(n, "Source files");

    PStrs link_outputs = {0};
    const char *ok_path = NULL;
    AvpJson *bc = c->build_config;
    if (bc) {
        AVP_VEC(LinkStep *) link_steps = {0};
        for (size_t i = 0; i < pv_len(pchs) && pchs->kind != PV_DICT; i++) {
            Pv *pch = pchs->u.list->v[i];
            const char *src_path_rel = proj_pv_path(a, pv_dict_get(pch, AVP_S("source")));
            const char *pch_out_name = proj_with_suffix(a, src_path_rel, ".mch");
            const char *pch_out_abs_path = proj_path(a, avp_path_abspath(a, proj_join(a, proj_join(a, build_path, "include"), pch_out_name)));
            Pv *cflags = pv_dict_get(pch, AVP_S("cflags"));
            if (!any_lang(cflags)) list_insert0(a, cflags, pv_strz(a, proj_file_is_cpp(src_path_rel) ? "-lang=c++" : "-lang=c"));
            const char *cflags_str = proj_flags_str(a, cflags);
            Pv *sj = pv_dict_get(pch, AVP_S("shift_jis"));
            bool shift_jis = pv_truthy(sj ? sj : proj_attr(c, "shift_jis"));
            ninja_comment(n, avp_sprintf(a, "Precompiled header %s", pch_out_name));
            NinjaVar vars[] = {
                {"mw_version", proj_pv_path(a, pv_dict_get(pch, AVP_S("mw_version")))},
                {"cflags", cflags_str},
                {"basedir", avp_cz(a, avp_path_dirname(pch_out_abs_path))},
                {"basefile", proj_with_suffix(a, pch_out_abs_path, "")},
                {"basefilestem", avp_cz(a, pv_path_stem(avp_str(pch_out_abs_path)))},
            };
            ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, pch_out_abs_path), .rule = shift_jis ? "mwcc_pch_sjis" : "mwcc_pch",
                                         .inputs = ninja_one(a, avp_sprintf(a, "include/%s", src_path_rel)), .vars = vars, .nvars = 5,
                                         .implicit = strs(a, shift_jis ? &g->mwcc_pch_sjis_implicit : &g->mwcc_pch_implicit)});
            ninja_newline(n);
        }

        LinkStep *dol = link_step_new(g, bc);
        AvpJson *units = avp_json_get(bc, AVP_S("units"));
        for (size_t i = 0; i < avp_json_len(units); i++) add_unit(g, avp_json_at(units, i), dol);
        avp_vec_push(&link_steps, dol);

        AvpJson *modules = avp_json_get(bc, AVP_S("modules"));
        if (pv_truthy(proj_attr(c, "build_rels"))) {
            for (size_t m = 0; m < avp_json_len(modules); m++) {
                AvpJson *module = avp_json_at(modules, m);
                LinkStep *s = link_step_new(g, module);
                AvpJson *munits = avp_json_get(module, AVP_S("units"));
                for (size_t i = 0; i < avp_json_len(munits); i++) add_unit(g, avp_json_at(munits, i), s);
                if (s->inputs.n == 0) {
                    Pv *empty = proj_attr(c, "rel_empty_file");
                    if (empty->kind == PV_NONE) proj_exit("ProjectConfig.rel_empty_file missing");
                    AvpJson *u = avp_json_obj(a);
                    avp_json_set(a, u, AVP_S("object"), avp_json_null());
                    avp_json_set(a, u, AVP_S("name"), avp_json_str(a, avp_str(proj_pv_str(a, empty))));
                    avp_json_set(a, u, AVP_S("autogenerated"), avp_json_bool(true));
                    add_unit(g, u, s);
                }
                avp_vec_push(&link_steps, s);
            }
        }
        ninja_newline(n);

        bool have_compilers_path = pv_truthy(proj_attr(c, "compilers_path"));
        for (size_t i = 0; i < g->used_compiler_versions.n; i++) {
            const char *mw_path = proj_join(a, proj_join(a, compilers, g->used_compiler_versions.v[i]), "mwcceppc.exe");
            if (have_compilers_path && !avp_exists(mw_path)) proj_exit("Compiler %s does not exist", mw_path);
        }
        const char *ld_path = proj_join(a, proj_join(a, compilers, proj_pv_str(a, proj_attr(c, "linker_version"))), "mwldeppc.exe");
        if (have_compilers_path && !avp_exists(ld_path)) proj_exit("Linker %s does not exist", ld_path);

        write_custom_step(g, "post-compile", "pre-compile", NULL);

        for (size_t i = 0; i < link_steps.n; i++) {
            link_step_write(g, link_steps.v[i]);
            avp_vec_push(&link_outputs, link_output(g, link_steps.v[i]));
        }
        ninja_newline(n);

        write_custom_step(g, "post-link", "post-compile", NULL);

        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, link_output(g, link_steps.v[0])), .rule = "elf2dol",
                                     .inputs = ninja_one(a, link_partial_output(g, link_steps.v[0])),
                                     .implicit = ninja_one(a, dtk), .order_only = ninja_one(a, "post-link")});

        ninja_comment(n, "Generate REL(s)");
        AvpJson *links = avp_json_get(bc, AVP_S("links"));
        const char *flags = avp_json_len(links) > 1 ? "-w -q" : "-w";
        ninja_rule(n, &(NinjaRule){.name = "makerel",
                                   .command = avp_sprintf(a, "%s rel make %s -c $config $names @$rspfile", proj_cmd_path(a, dtk), flags),
                                   .description = "REL", .rspfile = "$rspfile", .rspfile_content = "$in_newline"});
        AvpStrVec generated_rels = {0};
        const char *config_path = cfg_path(c, "config_path");
        for (size_t idx = 0; idx < avp_json_len(links); idx++) {
            AvpJson *link_modules = avp_json_get(avp_json_at(links, idx), AVP_S("modules"));
            AVP_VEC(LinkStep *) local = {0};
            for (size_t i = 0; i < link_steps.n; i++)
                if (json_str_in(link_modules, link_steps.v[i]->name)) avp_vec_push(&local, link_steps.v[i]);
            for (size_t i = 1; i < local.n; i++) /* stable sort by module_id */
                for (size_t k = i; k > 0 && local.v[k - 1]->module_id > local.v[k]->module_id; k--) {
                    LinkStep *t = local.v[k];
                    local.v[k] = local.v[k - 1];
                    local.v[k - 1] = t;
                }
            PStrs rel_outputs = {0}, partials = {0};
            AvpBuf names;
            avp_buf_init(&names, a);
            size_t first_new = generated_rels.n;
            for (size_t i = 0; i < local.n; i++) {
                LinkStep *s = local.v[i];
                bool done = false;
                for (size_t k = 0; k < first_new; k++)
                    if (avp_str_eq(generated_rels.v[k], s->name)) done = true;
                if (s->module_id != 0 && !done) avp_vec_push(&rel_outputs, link_output(g, s));
            }
            if (rel_outputs.n == 0) continue;
            for (size_t i = 0; i < local.n; i++) {
                LinkStep *s = local.v[i];
                bool done = false;
                for (size_t k = 0; k < first_new; k++)
                    if (avp_str_eq(generated_rels.v[k], s->name)) done = true;
                if (s->module_id != 0 && !done) avp_vec_push(&generated_rels, s->name);
                avp_vec_push(&partials, link_partial_output(g, s));
                if (i) avp_buf_putc(&names, ' ');
                avp_buf_printf(&names, "-n %.*s", AVP_SF(s->name));
            }
            NinjaVar vars[] = {{"config", config_path},
                               {"rspfile", proj_join(a, proj_out_path(c), avp_sprintf(a, "rel%zu.rsp", idx))},
                               {"names", avp_buf_cz(&names)}};
            ninja_build(n, &(NinjaBuild){.outputs = strs(a, &rel_outputs), .rule = "makerel", .inputs = strs(a, &partials),
                                         .implicit = NINJA_LIST(a, dtk, config_path), .vars = vars, .nvars = 3,
                                         .order_only = ninja_one(a, "post-link")});
            ninja_newline(n);
        }

        write_custom_step(g, "post-build", "post-link", NULL);

        ninja_comment(n, "Build all source files");
        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, "all_source"), .rule = "phony", .inputs = strs(a, &g->source_inputs)});
        ninja_newline(n);

        ninja_comment(n, "Check hash");
        ok_path = proj_join(a, build_path, "ok");
        const char *quiet = link_steps.n > 3 ? "-q " : "";
        ninja_rule(n, &(NinjaRule){.name = "check", .command = avp_sprintf(a, "%s shasum %s -c $in -o $out", proj_cmd_path(a, dtk), quiet),
                                   .description = "CHECK $in"});
        PStrs check_implicit = STRS(dtk);
        for (size_t i = 0; i < link_outputs.n; i++) avp_vec_push(&check_implicit, link_outputs.v[i]);
        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, ok_path), .rule = "check",
                                     .inputs = ninja_one(a, cfg_path(c, "check_sha_path")),
                                     .implicit = strs(a, &check_implicit), .order_only = ninja_one(a, "post-build")});
        ninja_newline(n);

        ninja_comment(n, "Calculate progress");
        const char *progress_cmd = avp_sprintf(a, "%s%s $configure_args progress", ctool_cmd(c, "configure.exe"), configure_own);
        const char *progress_lib = ctool_path(c, "configure.exe");
        ninja_rule(n, &(NinjaRule){.name = "progress", .command = progress_cmd, .description = "PROGRESS"});
        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, "progress"), .rule = "progress",
                                     .implicit = NINJA_LIST(a, ok_path, configure_script, progress_lib, report_path),
                                     .order_only = ninja_one(a, "post-build")});

        ninja_comment(n, "Generate progress report");
        ninja_rule(n, &(NinjaRule){.name = "report",
                                   .command = avp_sprintf(a, "%s report generate $objdiff_report_args -o $out", proj_cmd_path(a, objdiff)),
                                   .description = "REPORT"});
        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, report_path), .rule = "report",
                                     .implicit = NINJA_LIST(a, objdiff, "objdiff.json", "all_source"),
                                     .order_only = ninja_one(a, "post-build")});

        ninja_comment(n, "Phony edge that will always be considered dirty by ninja.");
        ninja_comment(n, "This can be used as an implicit to a target that should always be rerun, ignoring file modified times.");
        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, "always"), .rule = "phony"});
        ninja_newline(n);

        const char *report_baseline_path = proj_join(a, build_path, "baseline.json");
        const char *report_changes_path = proj_join(a, build_path, "report_changes.json");
        const char *regressions_md = proj_join(a, build_path, "regressions.md");
        const char *changes_fmt = ctool_path(c, "changes_fmt.exe");
        const char *changes_cmd = ctool_cmd(c, "changes_fmt.exe");
        ninja_comment(n, "Create a baseline progress report for later match regression testing");
        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, report_baseline_path), .rule = "report",
                                     .implicit = NINJA_LIST(a, objdiff, "all_source", "always"),
                                     .order_only = ninja_one(a, "post-build")});
        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, "baseline"), .rule = "phony", .inputs = ninja_one(a, report_baseline_path)});
        ninja_comment(n, "Check for any match regressions against the baseline");
        ninja_comment(n, "Will fail if no baseline has been created");
        ninja_rule(n, &(NinjaRule){.name = "report_changes",
                                   .command = avp_sprintf(a, "%s report changes --format json-pretty %s $in -o $out", proj_cmd_path(a, objdiff),
                                                          proj_cmd_path(a, report_baseline_path)),
                                   .description = "CHANGES"});
        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, report_changes_path), .rule = "report_changes",
                                     .inputs = ninja_one(a, report_path), .implicit = NINJA_LIST(a, objdiff, "always")});
        ninja_rule(n, &(NinjaRule){.name = "changes_fmt", .command = avp_sprintf(a, "%s $args $in", changes_cmd), .description = "CHANGESFMT"});
        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, "changes"), .rule = "changes_fmt",
                                     .inputs = ninja_one(a, report_changes_path), .implicit = ninja_one(a, changes_fmt)});
        NinjaVar all_var[] = {{"args", "--all"}};
        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, "changes_all"), .rule = "changes_fmt",
                                     .inputs = ninja_one(a, report_changes_path), .implicit = ninja_one(a, changes_fmt),
                                     .vars = all_var, .nvars = 1});
        ninja_rule(n, &(NinjaRule){.name = "changes_md", .command = avp_sprintf(a, "%s $in -o $out", changes_cmd), .description = "CHANGESFMT $out"});
        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, regressions_md), .rule = "changes_md",
                                     .inputs = ninja_one(a, report_changes_path), .implicit = ninja_one(a, changes_fmt)});
        ninja_newline(n);

        const char *dol_elf_path = link_partial_output(g, link_steps.v[0]);
        ninja_comment(n, "Check for mismatching symbols");
        ninja_rule(n, &(NinjaRule){.name = "dol_diff", .command = avp_sprintf(a, "%s -L error dol diff $in", proj_cmd_path(a, dtk)),
                                   .description = avp_sprintf(a, "DIFF %s", dol_elf_path)});
        ninja_build(n, &(NinjaBuild){.inputs = NINJA_LIST(a, config_path, dol_elf_path), .outputs = ninja_one(a, "dol_diff"), .rule = "dol_diff"});
        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, "diff"), .rule = "phony", .inputs = ninja_one(a, "dol_diff")});
        ninja_newline(n);
        ninja_comment(n, "Apply symbols from linked ELF");
        ninja_rule(n, &(NinjaRule){.name = "dol_apply", .command = avp_sprintf(a, "%s dol apply $in", proj_cmd_path(a, dtk)),
                                   .description = avp_sprintf(a, "APPLY %s", dol_elf_path)});
        ninja_build(n, &(NinjaBuild){.inputs = NINJA_LIST(a, config_path, dol_elf_path), .outputs = ninja_one(a, "dol_apply"),
                                     .rule = "dol_apply", .implicit = NINJA_LIST(a, ok_path)});
        ninja_build(n, &(NinjaBuild){.outputs = ninja_one(a, "apply"), .rule = "phony", .inputs = ninja_one(a, "dol_apply")});
        ninja_newline(n);
    }

    const char *build_config_path = proj_join(a, build_path, "config.json");
    ninja_comment(n, "Split DOL into relocatable objects");
    ninja_rule(n, &(NinjaRule){.name = "split", .command = avp_sprintf(a, "%s dol split $in $out_dir", proj_cmd_path(a, dtk)),
                               .description = "SPLIT $in", .depfile = "$out_dir/dep", .deps = "gcc"});
    NinjaVar split_vars[] = {{"out_dir", build_path}};
    ninja_build(n, &(NinjaBuild){.inputs = ninja_one(a, cfg_path(c, "config_path")), .outputs = ninja_one(a, build_config_path),
                                 .rule = "split", .implicit = ninja_one(a, dtk), .vars = split_vars, .nvars = 1});
    ninja_newline(n);

    ninja_comment(n, "Reconfigure on change");
    const char *configure_cmd = avp_sprintf(a, "%s%s $configure_args", ctool_cmd(c, "configure.exe"), configure_own);
    PStrs reconfig = STRS(build_config_path, configure_script, ctool_path(c, "configure.exe"));
    ninja_rule(n, &(NinjaRule){.name = "configure", .command = configure_cmd, .generator = true,
                               .description = avp_sprintf(a, "RUN %s", configure_script)});
    Pv *deps = proj_attr(c, "reconfig_deps");
    for (size_t i = 0; i < pv_len(deps) && deps->kind != PV_DICT; i++)
        avp_vec_push(&reconfig, pv_truthy(deps->u.list->v[i]) ? proj_pv_str(a, deps->u.list->v[i]) : NULL);
    ninja_build(n, &(NinjaBuild){.outputs = NINJA_LIST(a, "build.ninja", "objdiff.json"), .rule = "configure",
                                 .implicit = strs(a, &reconfig)});
    ninja_newline(n);

    ninja_comment(n, "Default rule");
    if (bc) {
        if (pv_truthy(proj_attr(c, "non_matching"))) ninja_default(n, strs(a, &link_outputs));
        else if (pv_truthy(proj_attr(c, "progress"))) ninja_default(n, ninja_one(a, "progress"));
        else ninja_default(n, ninja_one(a, ok_path));
    } else {
        ninja_default(n, ninja_one(a, build_config_path));
    }
    proj_write_text(a, "build.ninja", avp_buf_str(&out));
}

/* ---- generate_build ---- */

int project_generate_build(AvpArena *a, Pv *config, const ProjectEnv *env) {
    PCtx c = {a, config, env, NULL, NULL};
    proj_validate(&c);
    proj_resolve_objects(&c);
    proj_load_build_config(&c, proj_join(a, proj_out_path(&c), "config.json"));
    proj_generate_build_ninja(&c);
    proj_generate_objdiff_config(&c);
    proj_generate_compile_commands(&c);
    return 0;
}
