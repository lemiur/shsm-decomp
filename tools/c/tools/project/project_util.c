/* project.py's helpers: shell_join / cmd_path / chain, file kinds, check_path_case, the path and value glue. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "project_int.h"

void proj_exit(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *msg = avp_vsprintf(avp_arena(), fmt, ap);
    va_end(ap);
    fflush(stdout);
    fprintf(stderr, "%s\n", msg);
    avp_exit(1);
}

Pv *proj_attr(const PCtx *c, const char *name) {
    Pv *v = pv_getattr(c->config, name);
    if (!v) proj_exit("AttributeError: 'ProjectConfig' object has no attribute '%s'", name);
    return v;
}

const char *proj_path(AvpArena *a, const char *s) { return avp_cz(a, pv_path_norm(a, avp_str(s))); }

const char *proj_join(AvpArena *a, const char *base, const char *other) {
    return avp_cz(a, pv_path_join(a, avp_str(base), avp_str(other)));
}

const char *proj_pv_str(AvpArena *a, const Pv *v) { return avp_cz(a, pv_to_str(a, v)); }

const char *proj_pv_path(AvpArena *a, const Pv *v) {
    if (!v || v->kind == PV_NONE) return NULL;
    if (v->kind != PV_STR && v->kind != PV_PATH)
        proj_exit("TypeError: argument should be a str or an os.PathLike object, not '%s'", pv_kind_name(v));
    return proj_path(a, avp_cz(a, v->u.s));
}

const char *proj_with_suffix(AvpArena *a, const char *p, const char *suffix) {
    AvpStr r = pv_path_with_suffix(a, avp_str(p), avp_str(suffix));
    if (!r.p) proj_exit("ValueError: %s has an empty name", p);
    return avp_cz(a, r);
}

const char *proj_list2cmdline(AvpArena *a, size_t n, const char *const *args) {
    AvpBuf r;
    avp_buf_init(&r, a);
    for (size_t i = 0; i < n; i++) {
        const char *arg = args[i];
        size_t bs = 0;
        if (r.n) avp_buf_putc(&r, ' ');
        bool needquote = strchr(arg, ' ') || strchr(arg, '\t') || !*arg;
        if (needquote) avp_buf_putc(&r, '"');
        for (const char *p = arg; *p; p++) {
            if (*p == '\\') {
                bs++;
            } else if (*p == '"') {
                for (size_t k = 0; k < bs * 2; k++) avp_buf_putc(&r, '\\');
                bs = 0;
                avp_buf_putz(&r, "\\\"");
            } else {
                for (; bs; bs--) avp_buf_putc(&r, '\\');
                avp_buf_putc(&r, *p);
            }
        }
        for (size_t k = 0; k < bs; k++) avp_buf_putc(&r, '\\');
        if (needquote) {
            for (size_t k = 0; k < bs; k++) avp_buf_putc(&r, '\\');
            avp_buf_putc(&r, '"');
        }
    }
    return avp_buf_cz(&r);
}

const char *proj_cmd_path(AvpArena *a, const char *path) {
    AvpStr s = avp_str(path);
    bool space = false;
    for (const char *p = s.p, *end = s.p + s.n; p < end;)
        if (avp_cp_isspace(avp_u8_next(&p, end))) space = true;
    if (!space) return path;
    return proj_list2cmdline(a, 1, &path);
}

const char *proj_chain(AvpArena *a, const char *command) { return avp_sprintf(a, "cmd /c \"%s\"", command); }

const char *proj_relpath(AvpArena *a, const char *path) {
    char *out = NULL;
    if (!avp_path_relpath(a, avp_path_abspath(a, path), NULL, &out))
        proj_exit("ValueError: %s", avp_cz(a, avp_io_err_str(a)));
    return proj_path(a, out);
}

static bool suffix_in(const char *p, const char *const *list, size_t n) {
    AvpArena *a = avp_arena();
    AvpStr suf = avp_str_lower(a, pv_path_suffix(avp_str(p)));
    for (size_t i = 0; i < n; i++)
        if (avp_str_eqz(suf, list[i])) return true;
    return false;
}

bool proj_file_is_asm(const char *p) { return suffix_in(p, (const char *const[]){".s"}, 1); }
bool proj_file_is_c(const char *p) { return suffix_in(p, (const char *const[]){".c"}, 1); }
bool proj_file_is_cpp(const char *p) {
    return suffix_in(p, (const char *const[]){".cc", ".cp", ".cpp", ".cxx", ".pch++"}, 5);
}

bool proj_path_eq(const char *x, const char *y) {
    AvpArena *a = avp_arena();
    return avp_str_eq(avp_str_lower(a, avp_str(x)), avp_str_lower(a, avp_str(y)));
}

bool proj_startswith(const char *s, const char *prefix) { return strncmp(s, prefix, strlen(prefix)) == 0; }

const char *proj_flags_str(AvpArena *a, const Pv *flags) {
    if (!flags || flags->kind == PV_NONE) return "";
    if (flags->kind != PV_LIST && flags->kind != PV_TUPLE)
        proj_exit("TypeError: can only join an iterable of str, not %s", pv_kind_name(flags));
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < flags->u.list->n; i++) {
        const Pv *f = flags->u.list->v[i];
        if (f->kind != PV_STR) proj_exit("TypeError: sequence item %zu: expected str instance, %s found", i, pv_kind_name(f));
        if (i) avp_buf_putc(&b, ' ');
        avp_buf_put(&b, f->u.s);
    }
    return avp_buf_cz(&b);
}

/* _listdir_cache: str(Path) lower-cased -> AvpStrVec* (module-global in the Python) */
static AvpMap *g_listdir_cache;

void proj_check_path_case(AvpArena *a, const char *path) {
    if (!g_listdir_cache) g_listdir_cache = avp_map_new();
    AvpStrVec parts = avp_pl_parts(a, path);
    const char *curr;
    size_t start;
    if (pv_path_is_absolute(avp_str(path))) {
        curr = proj_path(a, avp_cz(a, parts.v[0]));
        start = 1;
    } else {
        curr = ".";
        start = 0;
    }
    for (size_t i = start; i < parts.n; i++) {
        AvpStr key = avp_str_lower(a, avp_str(curr));
        AvpVal *hit = avp_map_get(g_listdir_cache, key);
        AvpStrVec *entries;
        if (hit) {
            entries = hit->p;
        } else {
            entries = avp_alloc(a, sizeof *entries);
            if (!avp_listdir(a, curr, entries)) proj_exit("Cannot access: %s", curr);
            avp_map_put(g_listdir_cache, key, NULL)->p = entries;
        }
        AvpStr want = avp_str_lower(a, parts.v[i]);
        bool found = false;
        for (size_t k = 0; k < entries->n; k++) {
            if (avp_str_eq(avp_str_lower(a, entries->v[k]), want)) {
                curr = proj_join(a, curr, avp_cz(a, entries->v[k]));
                found = true;
                break;
            }
        }
        if (!found) proj_exit("Cannot resolve: %s", path);
    }
    if (!proj_path_eq(path, curr)) printf("\xe2\x9a\xa0\xef\xb8\x8f  Case mismatch: expected=%s actual=%s\n", path, curr);
}

PObj *proj_object(const PCtx *c, AvpStr name) {
    AvpVal *v = avp_map_get(c->objects, name);
    return v ? v->p : NULL;
}

NinjaPaths proj_pv_ninja(AvpArena *a, const Pv *v) {
    if (!v || v->kind == PV_NONE) return ninja_none();
    if (v->kind == PV_STR || v->kind == PV_PATH) return ninja_one(a, avp_cz(a, v->u.s));
    if (v->kind == PV_LIST || v->kind == PV_TUPLE) {
        size_t n = v->u.list->n;
        const char **items = avp_alloc(a, (n ? n : 1) * sizeof *items);
        for (size_t i = 0; i < n; i++) {
            const Pv *x = v->u.list->v[i];
            items[i] = pv_truthy(x) ? proj_pv_str(a, x) : NULL;
        }
        return ninja_list(a, n, items);
    }
    proj_exit("TypeError: a ninja path or a list of paths expected, not %s", pv_kind_name(v));
}

AvpJson *proj_load_json(AvpArena *a, const char *path) {
    AvpBytes bytes = {0};
    if (!avp_read_all(a, path, &bytes)) proj_exit("%s", avp_cz(a, avp_io_err_str(a)));
    AvpJsonError err = {0};
    AvpJson *j = avp_json_parse(a, avp_strn((const char *)bytes.v, bytes.n), &err);
    if (!j) proj_exit("json.decoder.JSONDecodeError: %s", avp_cz(a, avp_json_error_str(a, &err)));
    return j;
}

void proj_write_text(AvpArena *a, const char *path, AvpStr text) {
    (void)a;
    if (!avp_write_text(path, text, AVP_TM_UTF8)) proj_exit("%s", avp_cz(avp_arena(), avp_io_err_str(avp_arena())));
}
