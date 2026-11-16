/* The classes of tools/project.py that configure.py instantiates (project.h): Object.__init__,
 * ProgressCategory.__init__ and ProjectConfig.__init__, every attribute in __init__'s order with its default. */
#include "project.h"

Pv *project_new_object(AvpArena *a, bool completed, AvpStr name, const Pv *options) {
    Pv *o = pv_instance(a, "Object");
    pv_setattr(a, o, "name", pv_str(a, name));
    pv_setattr(a, o, "completed", pv_bool(completed));
    Pv *opts = pv_dict(a);
    pv_dict_set(a, opts, AVP_S("add_to_all"), pv_none());
    pv_dict_set(a, opts, AVP_S("asflags"), pv_none());
    pv_dict_set(a, opts, AVP_S("asm_dir"), pv_none());
    pv_dict_set(a, opts, AVP_S("cflags"), pv_none());
    pv_dict_set(a, opts, AVP_S("extab_padding"), pv_none());
    pv_dict_set(a, opts, AVP_S("extra_asflags"), pv_list(a));
    pv_dict_set(a, opts, AVP_S("extra_cflags"), pv_list(a));
    pv_dict_set(a, opts, AVP_S("extra_clang_flags"), pv_list(a));
    pv_dict_set(a, opts, AVP_S("lib"), pv_none());
    pv_dict_set(a, opts, AVP_S("mw_version"), pv_none());
    pv_dict_set(a, opts, AVP_S("progress_category"), pv_none());
    pv_dict_set(a, opts, AVP_S("scratch_preset_id"), pv_none());
    pv_dict_set(a, opts, AVP_S("shift_jis"), pv_none());
    pv_dict_set(a, opts, AVP_S("source"), pv_str(a, name));
    pv_dict_set(a, opts, AVP_S("src_dir"), pv_none());
    if (options) { /* self.options.update(options) */
        for (size_t i = 0; i < options->u.dict->n; i++)
            pv_dict_set(a, opts, options->u.dict->keys[i], options->u.dict->vals[i]);
    }
    pv_setattr(a, o, "options", opts);
    pv_setattr(a, o, "src_path", pv_none());
    pv_setattr(a, o, "asm_path", pv_none());
    pv_setattr(a, o, "src_obj_path", pv_none());
    pv_setattr(a, o, "asm_obj_path", pv_none());
    pv_setattr(a, o, "ctx_path", pv_none());
    return o;
}

Pv *project_new_progress_category(AvpArena *a, AvpStr id, AvpStr name) {
    Pv *c = pv_instance(a, "ProgressCategory");
    pv_setattr(a, c, "id", pv_str(a, id));
    pv_setattr(a, c, "name", pv_str(a, name));
    return c;
}

Pv *project_new_config(AvpArena *a) {
    static const char *const none_attrs1[] = {
        "binutils_tag", "binutils_path", "dtk_tag", "dtk_path", "compilers_tag", "compilers_path", "wibo_tag",
        "wrapper", "sjiswrap_tag", "sjiswrap_path", "ninja_path", "objdiff_tag", "objdiff_path",
    };
    Pv *c = pv_instance(a, "ProjectConfig");
    /* Paths */
    pv_setattr(a, c, "build_dir", pv_path(a, AVP_S("build")));
    pv_setattr(a, c, "src_dir", pv_path(a, AVP_S("src")));
    pv_setattr(a, c, "tools_dir", pv_path(a, AVP_S("tools")));
    pv_setattr(a, c, "asm_dir", pv_path(a, AVP_S("asm")));
    /* Tooling */
    for (size_t i = 0; i < sizeof none_attrs1 / sizeof none_attrs1[0]; i++) pv_setattr(a, c, none_attrs1[i], pv_none());
    /* Project config */
    pv_setattr(a, c, "non_matching", pv_bool(false));
    pv_setattr(a, c, "build_rels", pv_bool(true));
    pv_setattr(a, c, "check_sha_path", pv_none());
    pv_setattr(a, c, "config_path", pv_none());
    pv_setattr(a, c, "generate_map", pv_bool(false));
    pv_setattr(a, c, "asflags", pv_none());
    pv_setattr(a, c, "ldflags", pv_none());
    pv_setattr(a, c, "libs", pv_none());
    pv_setattr(a, c, "precompiled_headers", pv_none());
    pv_setattr(a, c, "linker_version", pv_none());
    pv_setattr(a, c, "version", pv_none());
    pv_setattr(a, c, "warn_missing_config", pv_bool(false));
    pv_setattr(a, c, "warn_missing_source", pv_bool(false));
    pv_setattr(a, c, "rel_strip_partial", pv_bool(true));
    pv_setattr(a, c, "rel_empty_file", pv_none());
    pv_setattr(a, c, "shift_jis", pv_bool(true));
    pv_setattr(a, c, "reconfig_deps", pv_none());
    pv_setattr(a, c, "custom_build_rules", pv_none());
    pv_setattr(a, c, "custom_build_steps", pv_none());
    pv_setattr(a, c, "generate_compile_commands", pv_bool(true));
    pv_setattr(a, c, "extra_clang_flags", pv_list(a));
    pv_setattr(a, c, "scratch_preset_id", pv_none());
    pv_setattr(a, c, "link_order_callback", pv_none());
    pv_setattr(a, c, "context_exclude_globs", pv_list(a));
    pv_setattr(a, c, "context_defines", pv_list(a));
    /* Shared object cache */
    pv_setattr(a, c, "objcache", pv_bool(false));
    /* Progress output and report.json config */
    pv_setattr(a, c, "progress", pv_bool(true));
    pv_setattr(a, c, "progress_modules", pv_bool(true));
    pv_setattr(a, c, "progress_each_module", pv_bool(false));
    pv_setattr(a, c, "progress_categories", pv_list(a));
    pv_setattr(a, c, "print_progress_categories", pv_bool(true));
    pv_setattr(a, c, "progress_report_args", pv_none());
    /* Progress fancy printing */
    pv_setattr(a, c, "progress_use_fancy", pv_bool(false));
    pv_setattr(a, c, "progress_code_fancy_frac", pv_int(a, 0));
    pv_setattr(a, c, "progress_code_fancy_item", pv_str(a, AVP_S("")));
    pv_setattr(a, c, "progress_data_fancy_frac", pv_int(a, 0));
    pv_setattr(a, c, "progress_data_fancy_item", pv_str(a, AVP_S("")));
    return c;
}

bool project_is_windows(void) { return true; }
