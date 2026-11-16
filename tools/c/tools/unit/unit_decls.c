/* unit_decls: which names the headers of include/ declare as a namespace or define as a class. `new` writes a stub
 * for a function the mangler qualifies with one name (`init_mparams__4FMODFv`); the name is a namespace (the stub
 * goes inside `namespace FMOD { }`) or a class (a member definition needs the class declared), and only the headers
 * can say which. */
#include "unit_stub.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

struct UnitDecls {
    AvpSet *namespaces, *classes;
    AvpMap *class_header;  /* class name -> include-relative path of the first header that defines it (AvpStr *) */
    const char *cur;       /* the header being scanned (NULL: scanning a bare text) */
};

static bool ident_start(char c) { return isalpha((unsigned char)c) || c == '_'; }
static bool ident_char(char c) { return isalnum((unsigned char)c) || c == '_'; }

static size_t skip_space(const char *s, size_t i, size_t n) {
    while (i < n && isspace((unsigned char)s[i])) i++;
    return i;
}

/* One header's text: `namespace X {` names a namespace; `class X {`, `class X : base {` (also struct and union) a class.
 * A forward declaration (`class X;`), `friend class X;`, `enum class X`, comments, strings and preprocessor lines are
 * not definitions. */
static void add_text(AvpArena *a, UnitDecls *x, AvpStr t) {
    const char *s = t.p;
    size_t n = t.n, i = 0;
    char prev[16] = "";
    while (i < n) {
        char c = s[i];
        if (c == '/' && i + 1 < n && s[i + 1] == '/') {
            while (i < n && s[i] != '\n') i++;
        } else if (c == '/' && i + 1 < n && s[i + 1] == '*') {
            i += 2;
            while (i + 1 < n && !(s[i] == '*' && s[i + 1] == '/')) i++;
            i += 2;
        } else if (c == '"' || c == '\'') {
            i++;
            while (i < n && s[i] != c && s[i] != '\n') i += s[i] == '\\' ? 2 : 1;
            i++;
        } else if (c == '#') {
            while (i < n && s[i] != '\n') i += s[i] == '\\' && i + 1 < n ? 2 : 1;
        } else if (ident_start(c)) {
            size_t b = i;
            while (i < n && ident_char(s[i])) i++;
            size_t wl = i - b;
            bool is_ns = wl == 9 && !memcmp(s + b, "namespace", 9);
            bool is_cls = (wl == 5 && !memcmp(s + b, "class", 5)) || (wl == 6 && !memcmp(s + b, "struct", 6)) ||
                          (wl == 5 && !memcmp(s + b, "union", 5));
            if ((is_ns || is_cls) && strcmp(prev, "enum") != 0 && strcmp(prev, "friend") != 0) {
                size_t j = skip_space(s, i, n);
                if (j < n && ident_start(s[j])) {
                    size_t nb = j;
                    while (j < n && ident_char(s[j])) j++;
                    size_t ne = j;
                    j = skip_space(s, j, n);
                    if (is_cls && j + 5 <= n && !memcmp(s + j, "final", 5) && !(j + 5 < n && ident_char(s[j + 5])))
                        j = skip_space(s, j + 5, n); /* class X final : ... */
                    bool opens = j < n && s[j] == '{';
                    bool based = is_cls && j < n && s[j] == ':' && !(j + 1 < n && s[j + 1] == ':');
                    if (opens || based) {
                        AvpStr nm = avp_str_dup(a, (AvpStr){s + nb, ne - nb});
                        avp_set_add(is_ns ? x->namespaces : x->classes, nm);
                        if (!is_ns && x->cur) { /* the smallest path wins: the walk order must not decide */
                            AvpVal *old = avp_map_get(x->class_header, nm);
                            if (!old || strcmp(x->cur, ((AvpStr *)old->p)->p) < 0) {
                                AvpStr *h = AVP_NEW(a, AvpStr);
                                *h = avp_str(avp_strdup(a, x->cur));
                                avp_map_put(x->class_header, nm, NULL)->p = h;
                            }
                        }
                    }
                }
            }
            snprintf(prev, sizeof prev, "%.*s", (int)(wl < sizeof prev - 1 ? wl : sizeof prev - 1), s + b);
        } else {
            i++;
        }
    }
}

UnitDecls *unit_decls_from_texts(AvpArena *a, const AvpStr *texts, size_t n) {
    UnitDecls *x = AVP_NEW(a, UnitDecls);
    x->namespaces = avp_set_new();
    x->classes = avp_set_new();
    x->class_header = avp_map_new();
    for (size_t i = 0; i < n; i++) add_text(a, x, texts[i]);
    return x;
}

static bool header_name(const char *name) {
    static const char *const exts[] = {".h", ".hpp", ".hh", ".inl"};
    size_t l = strlen(name);
    for (size_t k = 0; k < sizeof exts / sizeof *exts; k++) {
        size_t e = strlen(exts[k]);
        if (l > e && !strcmp(name + l - e, exts[k])) return true;
    }
    return false;
}

UnitDecls *unit_decls_scan(AvpArena *a, const char *include_dir) {
    UnitDecls *x = unit_decls_from_texts(a, NULL, 0);
    AvpWalk *w = avp_isdir(include_dir) ? avp_walk_open(include_dir, true) : NULL;
    if (!w) return x;
    AvpWalkItem it = {0};
    while (avp_walk_next(w, &it)) {
        const char *dir = avp_cz(a, it.dirpath);
        for (size_t i = 0; i < it.files.n; i++) {
            const char *name = avp_cz(a, it.files.v[i]);
            AvpBytes b = {0};
            if (!header_name(name) || !avp_read_all(a, avp_path_join(a, dir, name), &b)) continue;
            char *rel = NULL;
            if (!avp_path_relpath(a, avp_path_join(a, dir, name), include_dir, &rel)) continue;
            for (char *c = rel; *c; c++)
                if (*c == '\\') *c = '/';
            x->cur = rel;
            add_text(a, x, (AvpStr){(const char *)b.v, b.n});
            x->cur = NULL;
        }
    }
    avp_walk_close(w);
    return x;
}

bool unit_decls_has_namespace(const UnitDecls *x, const char *name) { return x && avp_set_has(x->namespaces, avp_str(name)); }
bool unit_decls_has_class(const UnitDecls *x, const char *name) { return x && avp_set_has(x->classes, avp_str(name)); }

/* The include-relative path (`fmod/fmod_channel_realmanual3d.h`) of the header that defines the class; NULL when none
 * does, or when the declarations came from bare texts. */
const char *unit_decls_class_header(const UnitDecls *x, const char *name) {
    AvpVal *v = x ? avp_map_get(x->class_header, avp_str(name)) : NULL;
    return v ? ((AvpStr *)v->p)->p : NULL;
}
