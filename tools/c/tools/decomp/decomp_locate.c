/* decomp_locate: locating Ghidra (decomp.py read_properties, headless_script, resolve_install, install_info,
 * find_language, locate_ghidra, java_major, parse_java_version). */
#include "decomp.h"
#include "shsm_re.h"

#include <string.h>

AvpMap *dc_read_properties(AvpArena *a, const char *path) {
    AvpMap *props = avp_map_new();
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TM_UTF8_REPLACE, &text)) return props; /* except OSError: pass */
    AvpFileLines it;
    avp_file_lines_init(&it, text, AVP_NL_DEFAULT);
    AvpStr line;
    while (avp_file_lines_next(&it, &line)) {
        line = avp_str_strip(line);
        if (!line.n || line.p[0] == '#') continue;
        AvpStr k, v;
        if (!avp_str_partition(line, AVP_S("="), &k, &v)) continue;
        avp_map_put(props, avp_str_strip(k), NULL)->p = avp_cz(a, avp_str_strip(v));
    }
    return props;
}

static const char *prop(const AvpMap *props, const char *key, const char *dflt) {
    AvpVal *v = avp_map_get(props, avp_str(key));
    return v ? (const char *)v->p : dflt;
}

const char *dc_headless_script(AvpArena *a, const char *install) {
    return avp_join(a, install, "support", "analyzeHeadless.bat");
}

static bool install_ok(AvpArena *a, const char *p) {
    return avp_isfile(avp_join(a, p, "Ghidra", "application.properties")) && avp_isfile(dc_headless_script(a, p));
}

static AvpStrVec sorted_listdir(AvpArena *a, const char *dir) {
    AvpStrVec names = {0};
    if (!avp_listdir(a, dir, &names)) shsm_raise_io();
    avp_sort_strs(names.v, names.n);
    return names;
}

const char *dc_resolve_install(AvpArena *a, const char *path) {
    if (!path || !*path || !avp_isdir(path)) return NULL;
    if (install_ok(a, path)) return avp_path_abspath(a, path);
    AvpStrVec names = sorted_listdir(a, path);
    const char *only = NULL;
    size_t found = 0;
    for (size_t i = 0; i < names.n; i++) {
        const char *s = avp_path_join(a, path, avp_cz(a, names.v[i]));
        if (avp_isdir(s) && install_ok(a, s)) {
            if (!found) only = s;
            found++;
        }
    }
    avp_vec_free(&names);
    return found == 1 ? avp_path_abspath(a, only) : NULL;
}

const char *dc_find_language(AvpArena *a, const char *install, const char *settings_dir, const char *language_id) {
    const char *roots[3] = {avp_join(a, install, "Ghidra", "Processors"), avp_join(a, install, "Ghidra", "Extensions"),
                            avp_join(a, settings_dir, "Extensions")};
    AvpStr needle = avp_pyfmt(a, "id=\"%s\"", language_id);
    for (int r = 0; r < 3; r++) {
        if (!avp_isdir(roots[r])) continue;
        AvpStrVec mods = sorted_listdir(a, roots[r]);
        for (size_t i = 0; i < mods.n; i++) {
            const char *lang = avp_join(a, roots[r], avp_cz(a, mods.v[i]), "data", "languages");
            if (!avp_isdir(lang)) continue;
            AvpStrVec names = sorted_listdir(a, lang);
            for (size_t j = 0; j < names.n; j++) {
                if (!avp_str_endswith(names.v[j], AVP_S(".ldefs"))) continue;
                const char *p = avp_path_join(a, lang, avp_cz(a, names.v[j]));
                AvpStr text;
                if (!avp_read_text(a, p, AVP_TM_UTF8_REPLACE, &text)) continue;
                if (avp_str_contains(text, needle)) return p;
            }
            avp_vec_free(&names);
        }
        avp_vec_free(&mods);
    }
    return NULL;
}

DcInstall dc_install_info(AvpArena *a, const char *install, const char *appdata) {
    AvpMap *props = dc_read_properties(a, avp_join(a, install, "Ghidra", "application.properties"));
    DcInstall info = {0};
    info.install = install;
    info.version = prop(props, "application.version", "?");
    info.release = prop(props, "application.release.name", "PUBLIC");
    const char *jm = prop(props, "application.java.min", "0");
    info.java_min = *jm ? dc_int10(avp_str(jm)) : 0; /* int(props.get(...) or 0) */
    if (!appdata) appdata = avp_getenv_or(a, "APPDATA", avp_path_expanduser(a, "~"));
    info.settings_dir =
        avp_join(a, appdata, "ghidra", avp_pyfmt(a, "ghidra_%s_%s", info.version, info.release).p);
    info.language = dc_find_language(a, install, info.settings_dir, DC_GEKKO_LANGUAGE) ? DC_GEKKO_LANGUAGE
                                                                                       : DC_DEFAULT_LANGUAGE;
    avp_map_free(props);
    return info;
}

void dc_default_candidates(AvpArena *a, const char *const **out, size_t *n) {
    const char **c = AVP_NEWN(a, const char *, 5);
    c[0] = "E:\\Downloads\\ghidra_11.4_PUBLIC_20250620";
    c[1] = "E:\\Downloads\\ghidra_12.0.1_PUBLIC_20260114";
    c[2] = "C:\\ghidra";
    c[3] = "C:\\Program Files\\ghidra";
    c[4] = avp_path_expanduser(a, "~\\ghidra");
    *out = c;
    *n = 5;
}

DcInstall dc_locate_ghidra(AvpArena *a, const char *arg, const char *env_dir, const char *const *candidates,
                           size_t ncandidates, const char *appdata, const char **how) {
    const char *hows[2] = {"--ghidra", "GHIDRA_INSTALL_DIR"};
    const char *paths[2] = {arg, env_dir};
    for (int i = 0; i < 2; i++) {
        if (!paths[i] || !*paths[i]) continue;
        const char *inst = dc_resolve_install(a, paths[i]);
        if (!inst)
            shsm_raise(SHSM_EXC_USAGE,
                       "%s %s is not a Ghidra install (no Ghidra/application.properties and support/analyzeHeadless)",
                       hows[i], paths[i]);
        *how = hows[i];
        return dc_install_info(a, inst, appdata);
    }
    if (!candidates) dc_default_candidates(a, &candidates, &ncandidates);
    DcInstall first = {0};
    bool any = false;
    for (size_t i = 0; i < ncandidates; i++) {
        const char *inst = dc_resolve_install(a, candidates[i]);
        if (!inst) continue;
        DcInstall info = dc_install_info(a, inst, appdata);
        if (!any) first = info;
        any = true;
        if (strcmp(info.language, DC_GEKKO_LANGUAGE) == 0) {
            /* found[] is built in full before the scan in Python: every later candidate is still read, which
             * changes nothing observable */
            for (size_t j = i + 1; j < ncandidates; j++) {
                const char *later = dc_resolve_install(a, candidates[j]);
                if (later) dc_install_info(a, later, appdata);
            }
            *how = "candidate with the Gekko/Broadway language";
            return info;
        }
    }
    if (any) {
        *how = "first candidate found";
        return first;
    }
    shsm_raise(SHSM_EXC_USAGE, "no Ghidra install found: pass --ghidra DIR or set GHIDRA_INSTALL_DIR");
}

i64 dc_parse_java_version(AvpStr text) {
    ShsmReMatch m;
    if (!shsm_re_search(shsm_re("version \"(\\d+)(?:\\.(\\d+))?", 0), text, 0, AVP_END, &m)) return -1;
    i64 major = dc_int10(shsm_re_group(&m, 1));
    if (major == 1 && shsm_re_matched(&m, 2) && shsm_re_group(&m, 2).n) return dc_int10(shsm_re_group(&m, 2));
    return major;
}

i64 dc_java_major(void) {
    AvpArena *a = avp_arena();
    const char *argv[] = {"java", "-version", NULL};
    AvpProcSpec spec = {.argv = argv, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE, .text = true,
                        .errors = AVP_ERRORS_REPLACE, .timeout = 60, .arena = a};
    AvpProcResult r = avp_run(&spec);
    if (r.status == AVP_PROC_START_FAILED || r.status == AVP_PROC_TIMEOUT) return -1;
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_put(&b, r.err);
    avp_buf_put(&b, r.out);
    return dc_parse_java_version(avp_buf_str(&b));
}
