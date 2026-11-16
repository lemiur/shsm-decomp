/* rules_sources: load_sources(root) of tools/rules/check_rules.py (line 4674) and the project-wide storage-class
 * macros Checker.parse passes to every Parsed (lines 2063-2066). */
#include "rules_sources.h"

static bool source_name(AvpStr f) {
    static const char *const EXTS[] = {".c", ".cc", ".cp", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inc", NULL};
    for (const char *const *e = EXTS; *e; e++) {
        size_t l = strlen(*e);
        if (f.n < l) continue;
        bool eq = true;
        for (size_t q = 0; q < l && eq; q++) {
            char c = f.p[f.n - l + q];
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            eq = c == (*e)[q];
        }
        if (eq) return true;
    }
    return false;
}

RlSources *rl_load_sources(AvpArena *a, const char *root) {
    RlSources *S = AVP_NEW(a, RlSources);
    S->index = avp_map_new();
    static const char *const TOPS[] = {"src", "include"};
    for (size_t t = 0; t < 2; t++) {
        char *base = avp_path_join(a, root, TOPS[t]);
        if (!avp_isdir(base)) continue;
        AvpWalk *w = avp_walk_open(base, true);
        AvpWalkItem item = {0};
        while (w && avp_walk_next(w, &item)) {
            for (size_t k = 0; k < item.files.n; k++) {
                AvpStr f = item.files.v[k];
                if (!source_name(f)) continue;
                char *full = avp_path_join(a, avp_cz(a, item.dirpath), avp_cz(a, f));
                char *rel = NULL;
                if (!avp_path_relpath(a, full, root, &rel)) continue;
                for (char *p = rel; *p; p++)
                    if (*p == '\\') *p = '/';
                AvpStr text;
                if (!rl_read_source(a, full, &text)) continue;
                bool created;
                AvpVal *v = avp_map_put(S->index, avp_str(rel), &created);
                if (created) {
                    v->i = (i64)S->paths.n;
                    avp_vec_push(&S->paths, avp_str(rel));
                    avp_vec_push(&S->texts, text);
                } else {
                    S->texts.v[v->i] = text;
                }
            }
        }
        if (w) avp_walk_close(w);
    }
    return S;
}

AvpMap *rl_project_storage_macros(const RlSources *S) {
    AvpMap *m = avp_map_new();
    for (size_t i = 0; i < S->texts.n; i++) {
        shsm_re_assume_valid(S->texts.v[i]);
        rl_scan_storage_macros(m, S->texts.v[i]);
    }
    return m;
}

bool rl_source_text(AvpArena *a, const RlSources *S, const char *root, AvpStr rel, AvpStr *out) {
    AvpVal *v = avp_map_get(S->index, rel);
    if (v) {
        *out = S->texts.v[v->i];
        return true;
    }
    return rl_read_source(a, avp_path_join(a, root, avp_cz(a, rel)), out);
}
