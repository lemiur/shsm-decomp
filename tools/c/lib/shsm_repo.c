/* Repository model (tools/common/repo.py). */
#include "shsm_repo.h"

#include "avp/base.h"
#include "avp/fs.h"
#include "avp/map.h"
#include "avp/sort.h"
#include "shsm_cli.h"
#include "shsm_git.h"
#include "shsm_text.h"

#include <stdio.h>
#include <windows.h>

static bool is_root(AvpArena *a, const char *p) {
    return avp_isfile(avp_join(a, p, "configure.py")) && avp_isdir(avp_join(a, p, "config")) &&
           avp_isdir(avp_join(a, p, "tools"));
}

const char *shsm_find_root(const char *start) {
    AvpArena *a = avp_arena();
    bool explicit_start = start != NULL;
    const char *base = (start && *start) ? start : avp_getcwd(a);
    char *p = avp_path_abspath(a, base);
    for (;;) {
        if (is_root(a, p)) return p;
        AvpStr parent = avp_path_dirname(p);
        if (avp_str_eqz(parent, p)) break;
        p = avp_cz(a, parent);
    }
    if (explicit_start)
        shsm_raise(SHSM_EXC_FILE_NOT_FOUND, "not inside a checkout (no configure.py/config/tools): %s", start);
    /* the checkout of the executable: <root>/build/tools/<tool>.exe (Python: this package's own checkout) */
    char *own = avp_path_abspath(a, avp_join(a, avp_exe_dir(), "..", ".."));
    if (is_root(a, own)) return own;
    shsm_raise(SHSM_EXC_FILE_NOT_FOUND, "cannot find the repository root from %s", avp_getcwd(a));
}

/* A Python list literal of plain string literals (ast.literal_eval of what read_version passes it); false when the
 * text is anything else (the Python then fell back to config/). */
static bool parse_str_list(AvpArena *a, AvpStr t, AvpStrVec *out) {
    size_t i = 0, n = t.n;
    const char *s = t.p;
#define SKIP()                                                              \
    for (;;) {                                                              \
        i = shsm_skip_ws(t, i);                                             \
        if (i < n && s[i] == '#') {                                         \
            while (i < n && s[i] != '\n') i++;                              \
            continue;                                                       \
        }                                                                   \
        break;                                                              \
    }
    SKIP();
    if (i >= n || s[i] != '[') return false;
    i++;
    bool need_comma = false;
    for (;;) {
        SKIP();
        if (i >= n) return false;
        if (s[i] == ']') {
            i++;
            break;
        }
        if (s[i] == ',') {
            if (!need_comma) return false;
            need_comma = false;
            i++;
            continue;
        }
        if (need_comma) return false;
        AvpBuf b;
        avp_buf_init(&b, a);
        bool any = false;
        while (i < n && (s[i] == '"' || s[i] == '\'')) { /* adjacent literals concatenate */
            char q = s[i++];
            any = true;
            for (;;) {
                if (i >= n || s[i] == '\n') return false;
                char c = s[i++];
                if (c == q) break;
                if (c == '\\') {
                    if (i >= n) return false;
                    char e = s[i++];
                    switch (e) {
                    case '\\': avp_buf_putc(&b, '\\'); break;
                    case '\'': avp_buf_putc(&b, '\''); break;
                    case '"': avp_buf_putc(&b, '"'); break;
                    case 'n': avp_buf_putc(&b, '\n'); break;
                    case 't': avp_buf_putc(&b, '\t'); break;
                    default: return false;
                    }
                    continue;
                }
                avp_buf_putc(&b, c);
            }
            size_t j = i;
            i = shsm_skip_ws(t, i);
            if (!(i < n && (s[i] == '"' || s[i] == '\''))) i = j;
        }
        if (!any) return false;
        avp_vec_push(out, avp_str(avp_buf_cz(&b)));
        need_comma = true;
    }
    SKIP();
#undef SKIP
    return i == n;
}

/* re.search(r"^NAME\s*=\s*", text, re.M): the offset after the match, or -1. */
static i64 find_assignment(AvpStr text, const char *name) {
    AvpStr nm = avp_str(name);
    for (size_t ls = 0; ls <= text.n;) {
        AvpStr line = avp_str_slice(text, (i64)ls, AVP_END);
        if (avp_str_startswith(line, nm)) {
            size_t i = shsm_skip_ws(text, ls + nm.n);
            if (i < text.n && text.p[i] == '=') return (i64)shsm_skip_ws(text, i + 1);
        }
        i64 nl = avp_str_find(text, AVP_S("\n"), (i64)ls, AVP_END);
        if (nl < 0) break;
        ls = (size_t)nl + 1;
    }
    return -1;
}

static const char *version_from_configure(AvpArena *a, const char *root) {
    AvpStr text;
    if (!avp_read_text(a, avp_join(a, root, "configure.py"), AVP_TM_UTF8, &text)) return NULL;
    i64 v = find_assignment(text, "VERSIONS");
    /* (\[.*?\]) with re.S: from the "[" to the first "]" */
    if (v < 0 || (size_t)v >= text.n || text.p[v] != '[') return NULL;
    i64 close = avp_str_find(text, AVP_S("]"), v, AVP_END);
    if (close < 0) return NULL;
    AvpStrVec versions = {0};
    if (!parse_str_list(a, avp_str_slice(text, v, close + 1), &versions)) return NULL;
    i64 idx = 0;
    i64 d = find_assignment(text, "DEFAULT_VERSION");
    if (d >= 0) {
        size_t e = (size_t)d;
        while (e < text.n && text.p[e] >= '0' && text.p[e] <= '9') e++;
        if (e > (size_t)d) {
            if (!avp_parse_int(avp_str_slice(text, d, (i64)e), 10, &idx, NULL)) return NULL;
        }
    }
    if (idx < 0 || (size_t)idx >= versions.n) return NULL;
    return versions.v[idx].p;
}

const char *shsm_read_version(const char *root) {
    AvpArena *a = avp_arena();
    const char *v = version_from_configure(a, root);
    if (v) return v;
    const char *cfg = avp_join(a, root, "config");
    AvpStrVec names = {0};
    if (!avp_listdir(a, cfg, &names)) shsm_raise_io();
    avp_sort_strs(names.v, names.n);
    const char *found = NULL;
    size_t count = 0;
    for (size_t i = 0; i < names.n; i++) {
        if (avp_isfile(avp_join(a, cfg, names.v[i].p, "config.yml"))) {
            found = names.v[i].p;
            count++;
        }
    }
    if (count == 1) return found;
    shsm_raise(SHSM_EXC_VALUE, "cannot determine the version name in %s", root);
}

/* read_config_keys(config_yml, ("object_base", "object")) */
static void read_config_keys(AvpArena *a, const char *config_yml, const char **object_base, const char **object) {
    *object_base = *object = NULL;
    AvpStr text;
    if (!avp_read_text(a, config_yml, AVP_TM_UTF8, &text)) {
        if (avp_io_err()->kind == AVP_IOE_OS) return;
        shsm_raise_io();
    }
    AvpFileLines it;
    avp_file_lines_init(&it, text, AVP_NL_DEFAULT);
    AvpStr line;
    while (avp_file_lines_next(&it, &line)) {
        if (!line.n || line.p[0] == ' ' || line.p[0] == '\t' || line.p[0] == '#' || line.p[0] == '-') continue;
        AvpStr k, v;
        if (!avp_str_partition(line, AVP_S(":"), &k, &v)) continue;
        k = avp_str_strip(k);
        bool is_base = avp_str_eqz(k, "object_base"), is_obj = avp_str_eqz(k, "object");
        if (!is_base && !is_obj) continue;
        AvpStr before, after;
        avp_str_partition(v, AVP_S(" #"), &before, &after);
        v = avp_str_strip_chars(avp_str_strip(before), AVP_S("'\""));
        if (!v.n) continue;
        if (is_base) *object_base = avp_cz(a, v);
        else *object = avp_cz(a, v);
    }
}

const char *shsm_retail_dol_path(const char *root, const char *config_yml) {
    AvpArena *a = avp_arena();
    const char *base, *obj;
    read_config_keys(a, config_yml, &base, &obj);
    if (!obj) return NULL;
    const char *j = base ? avp_join(a, root, base, obj) : avp_join(a, root, obj);
    return avp_path_normpath(a, j);
}

ShsmRepo *shsm_repo_new(const char *root, const char *version) {
    AvpArena *a = avp_arena();
    ShsmRepo *r = AVP_NEW(a, ShsmRepo);
    r->root = avp_path_abspath(a, root);
    r->version = version ? avp_strdup(a, version) : shsm_read_version(r->root);
    const char *R = r->root, *V = r->version;
    r->config_dir = avp_join(a, R, "config", V);
    r->symbols_txt = avp_join(a, R, "config", V, "symbols.txt");
    r->splits_txt = avp_join(a, R, "config", V, "splits.txt");
    r->config_yml = avp_join(a, R, "config", V, "config.yml");
    r->build_sha1 = avp_join(a, R, "config", V, "build.sha1");
    r->baseline_report_json = avp_join(a, R, "config", V, "baseline_report.json");
    r->configure_py = avp_join(a, R, "configure.py");
    r->objdiff_json = avp_join(a, R, "objdiff.json");
    r->build_ninja = avp_join(a, R, "build.ninja");
    r->build_dir = avp_join(a, R, "build");
    r->version_build_dir = avp_join(a, R, "build", V);
    r->asm_dir = avp_join(a, R, "build", V, "asm");
    r->obj_dir = avp_join(a, R, "build", V, "obj");
    r->src_obj_dir = avp_join(a, R, "build", V, "src");
    r->report_json = avp_join(a, R, "build", V, "report.json");
    r->main_dol = avp_join(a, R, "build", V, "main.dol");
    r->retail_dol = shsm_retail_dol_path(R, r->config_yml);
    if (!r->retail_dol) r->retail_dol = avp_join(a, R, "orig", V, "sys", "main.dol");
    r->libraries_tsv = avp_join(a, R, "config", V, "libraries.tsv");
    r->score_history_db = avp_join(a, R, "build", "score_history.sqlite");
    r->index_dir = avp_join(a, R, "build", "index");
    r->tools_dir = avp_join(a, R, "tools");
    r->bin_dir = avp_join(a, R, "tools", "bin");
    r->compilers_dir = avp_join(a, R, "tools", "compilers");
    r->sjiswrap = avp_join(a, R, "build", "tools", "sjiswrap.exe");
    r->src_dir = avp_join(a, R, "src");
    r->include_dir = avp_join(a, R, "include");
    return r;
}

static SRWLOCK g_repo_lock = SRWLOCK_INIT;
static AvpMap *g_repos;

ShsmRepo *shsm_repo_find(const char *root) {
    const char *r = shsm_find_root(root);
    AvpStr key = avp_str(avp_path_normcase(avp_arena(), r));
    AcquireSRWLockExclusive(&g_repo_lock);
    if (!g_repos) g_repos = avp_map_new();
    AvpVal *v = avp_map_get(g_repos, key);
    ShsmRepo *repo = v ? (ShsmRepo *)v->p : NULL;
    ReleaseSRWLockExclusive(&g_repo_lock);
    if (repo) return repo;
    repo = shsm_repo_new(r, NULL); /* may raise: outside the lock */
    AcquireSRWLockExclusive(&g_repo_lock);
    bool created = false;
    AvpVal *slot = avp_map_put(g_repos, key, &created);
    if (created) slot->p = repo;
    else repo = (ShsmRepo *)slot->p;
    ReleaseSRWLockExclusive(&g_repo_lock);
    return repo;
}

const char *shsm_repo_tool(AvpArena *a, const ShsmRepo *r, const char *name) {
    AvpStr n = avp_str(name);
    if (avp_str_endswith(n, AVP_S(".exe"))) return avp_join(a, r->bin_dir, name);
    return avp_join(a, r->bin_dir, avp_sprintf(a, "%s.exe", name));
}

const char *shsm_repo_compiler(AvpArena *a, const ShsmRepo *r, const char *mw_version, const char *exe) {
    if (!mw_version) shsm_raise(SHSM_EXC_TYPE, "expected string or bytes-like object, got 'NoneType'");
    AvpStr v = avp_str(mw_version);
    size_t count = 1;
    for (size_t i = 0; i < v.n; i++)
        if (v.p[i] == '/' || v.p[i] == '\\') count++;
    const char **parts = AVP_NEWN(a, const char *, count + 2);
    size_t k = 0, start = 0;
    parts[k++] = r->compilers_dir;
    for (size_t i = 0; i <= v.n; i++) {
        if (i == v.n || v.p[i] == '/' || v.p[i] == '\\') {
            parts[k++] = avp_cz(a, avp_str_slice(v, (i64)start, (i64)i));
            start = i + 1;
        }
    }
    parts[k++] = exe ? exe : "mwcceppc.exe";
    return avp_path_joinv(a, k, parts);
}

const char *shsm_repo_asm_path(AvpArena *a, const ShsmRepo *r, const char *unit) {
    AvpStr p = avp_str(shsm_unit_path(a, unit));
    AvpStrVec parts = avp_str_split(p, AVP_S("/"), -1);
    const char **v = AVP_NEWN(a, const char *, parts.n + 1);
    v[0] = r->asm_dir;
    for (size_t i = 0; i < parts.n; i++) v[i + 1] = avp_cz(a, parts.v[i]);
    const char *j = avp_path_joinv(a, parts.n + 1, v);
    avp_vec_free(&parts);
    return avp_sprintf(a, "%s.s", j);
}

const char *shsm_repo_rel(AvpArena *a, const ShsmRepo *r, const char *path) {
    char *out = NULL;
    if (!avp_path_relpath(a, path, r->root, &out)) shsm_raise_io();
    for (char *c = out; *c; c++)
        if (*c == '\\') *c = '/';
    return out;
}

const char *shsm_unit_path(AvpArena *a, const char *unit) {
    char *u = avp_strdup(a, unit);
    for (char *c = u; *c; c++)
        if (*c == '\\') *c = '/';
    if (strncmp(u, "main/", 5) == 0) u += 5;
    AvpStr root, ext;
    avp_path_splitext(u, &root, &ext);
    static const char *const exts[] = {".c", ".cpp", ".cp", ".s", ".o", ".C", ".cc"};
    for (size_t i = 0; i < AVP_ARRAY_LEN(exts); i++)
        if (avp_str_eqz(ext, exts[i])) return avp_cz(a, root);
    return u;
}

const char *shsm_unit_name(AvpArena *a, const char *unit) {
    return avp_sprintf(a, "main/%s", shsm_unit_path(a, unit));
}

const char *shsm_repo_main_root(AvpArena *a, const ShsmRepo *r) {
    char *common = shsm_git_common_dir(a, r->root);
    if (!common) return NULL;
    AvpStr base = avp_path_basename(common);
    if (!avp_str_eqz(base, ".git")) return NULL; /* a bare repository: no main checkout */
    return avp_cz(a, avp_path_dirname(common));
}

const char *shsm_repo_shared_path(AvpArena *a, const ShsmRepo *r, const char *rel) {
    const char *own = avp_path_join(a, r->root, rel);
    if (avp_exists(own)) return own;
    const char *main_root = shsm_repo_main_root(a, r);
    if (!main_root) return own;
    if (strcmp(shsm_git_path_key(a, main_root), shsm_git_path_key(a, r->root)) == 0) return own; /* this is the main checkout */
    const char *there = avp_path_join(a, main_root, rel);
    if (!avp_exists(there)) return own;
    static bool noted;
    if (!noted) {
        noted = true;
        fprintf(stderr, "note: %s is not under %s; inputs come from the main checkout %s\n", rel, r->root, main_root);
    }
    return there;
}
