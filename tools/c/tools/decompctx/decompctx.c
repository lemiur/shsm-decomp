#include "decompctx.h"

#include <stdio.h>

#include "shsm_locale.h"
#include "shsm_text.h"

void decompctx_init(DecompctxState *st, AvpArena *a, const char *root_dir) {
    memset(st, 0, sizeof *st);
    st->a = a;
    st->root_dir = root_dir;
    st->defines = avp_set_new();
    st->script = "decompctx.exe";
}

AvpStr decompctx_prelude(AvpArena *a, const AvpStrVec *defs) {
    if (defs->n == 0) return AVP_S("");
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, "/* decompctx prelude */\n");
    for (size_t i = 0; i < defs->n; i++) {
        AvpStr name, val;
        if (avp_str_partition(defs->v[i], AVP_S("="), &name, &val))
            avp_buf_printf(&b, "#define %.*s %.*s\n", AVP_SF(name), AVP_SF(val));
        else
            avp_buf_printf(&b, "#define %.*s\n", AVP_SF(name));
    }
    avp_buf_putz(&b, "/* end decompctx prelude */\n\n");
    return avp_buf_str(&b);
}

AvpStr decompctx_sanitize_path(AvpArena *a, AvpStr path) {
    AvpStr t = avp_str_replace(a, path, AVP_S("\\"), AVP_S("/"), -1);
    return avp_str_replace(a, t, AVP_S(" "), AVP_S("\\ "), -1);
}

/* True when s[i..] starts with the ASCII word w; *end after it. */
static bool word_at(AvpStr s, size_t i, const char *w, size_t *end) {
    size_t n = strlen(w);
    if (i + n > s.n || memcmp(s.p + i, w, n) != 0) return false;
    *end = i + n;
    return true;
}

/* ^#\s*include\s*[<"](.+?)[>"] */
bool decompctx_match_include(AvpStr s, AvpStr *out) {
    size_t i;
    if (s.n == 0 || s.p[0] != '#') return false;
    if (!word_at(s, shsm_skip_ws(s, 1), "include", &i)) return false;
    i = shsm_skip_ws(s, i);
    if (i >= s.n || (s.p[i] != '<' && s.p[i] != '"')) return false;
    size_t start = i + 1;
    if (start >= s.n) return false; /* (.+?) needs a character */
    for (size_t k = start + 1; k < s.n; k++) {
        if (s.p[k] == '>' || s.p[k] == '"') {
            *out = avp_strn(s.p + start, k - start);
            return true;
        }
    }
    return false;
}

/* ^#\s*ifndef\s+(.*)$ */
bool decompctx_match_guard(AvpStr s, AvpStr *out) {
    size_t i;
    if (s.n == 0 || s.p[0] != '#') return false;
    if (!word_at(s, shsm_skip_ws(s, 1), "ifndef", &i)) return false;
    size_t j = shsm_skip_ws(s, i);
    if (j == i) return false;
    *out = avp_strn(s.p + j, s.n - j);
    return true;
}

/* ^#\s*pragma\s+once$ */
bool decompctx_match_once(AvpStr s) {
    size_t i;
    if (s.n == 0 || s.p[0] != '#') return false;
    if (!word_at(s, shsm_skip_ws(s, 1), "pragma", &i)) return false;
    size_t j = shsm_skip_ws(s, i);
    if (j == i) return false;
    size_t e;
    return word_at(s, j, "once", &e) && e == s.n;
}

static bool is_excluded(const DecompctxState *st, AvpStr name) {
    char *cz = avp_cz(st->a, name);
    for (size_t i = 0; i < st->exclude_globs.n; i++)
        if (avp_fnmatch(cz, avp_cz(st->a, st->exclude_globs.v[i]))) return true;
    return false;
}

static void fail(DecompctxState *st, const char *cls, const char *text) {
    st->failed = true;
    st->err_class = cls;
    st->err_text = text;
}

static void fail_io(DecompctxState *st) {
    fail(st, avp_io_err_class(), avp_cz(st->a, avp_io_err_str(st->a)));
}

static bool import_h_file(DecompctxState *st, AvpStr name, AvpStr r_path, AvpBuf *out) {
    char *nz = avp_cz(st->a, name);
    char *rel_path = avp_join(st->a, st->root_dir, avp_cz(st->a, r_path), nz);
    if (avp_exists(rel_path)) return decompctx_import_c_file(st, rel_path, out);
    for (size_t i = 0; i < st->include_dirs.n; i++) {
        char *inc_path = avp_path_join(st->a, avp_cz(st->a, st->include_dirs.v[i]), nz);
        if (avp_exists(inc_path)) return decompctx_import_c_file(st, inc_path, out);
    }
    printf("Failed to locate %s\n", nz);
    return true;
}

bool decompctx_process_text(DecompctxState *st, const char *in_file, AvpStr text, AvpBuf *out) {
    AvpFileLines it;
    AvpStr raw;
    size_t idx = 0;
    avp_file_lines_init(&it, text, AVP_NL_DEFAULT);
    AvpStr dir = avp_path_dirname(in_file);
    AvpBuf mine;
    avp_buf_init(&mine, st->a);
    for (; avp_file_lines_next(&it, &raw); idx++) {
        AvpStr line = avp_str_strip(raw);
        if (idx == 0) {
            AvpStr g;
            if (decompctx_match_guard(line, &g)) {
                if (avp_set_has(st->defines, g)) break;
                avp_set_add(st->defines, g);
            } else if (decompctx_match_once(line)) {
                AvpStr key = avp_str(in_file);
                if (avp_set_has(st->defines, key)) break;
                avp_set_add(st->defines, key);
            }
            printf("Processing file %s\n", in_file);
        }
        AvpStr inc;
        if (decompctx_match_include(line, &inc) && !avp_str_endswith(inc, AVP_S(".s"))) {
            bool excluded = is_excluded(st, inc);
            avp_buf_printf(&mine, "/* \"%s\" line %zu \"%.*s\" */\n", in_file, idx, AVP_SF(inc));
            if (excluded)
                avp_buf_putz(&mine, "/* Skipped excluded file */\n");
            else if (!import_h_file(st, inc, dir, &mine))
                return false;
            avp_buf_printf(&mine, "/* end \"%.*s\" */\n", AVP_SF(inc));
        } else {
            avp_buf_put(&mine, raw);
        }
    }
    avp_buf_put(out, avp_buf_str(&mine));
    return true;
}

bool decompctx_import_c_file(DecompctxState *st, const char *path, AvpBuf *out) {
    char *in_file;
    if (!avp_path_relpath(st->a, path, st->root_dir, &in_file)) {
        fail(st, "ValueError", avp_cz(st->a, avp_str(avp_io_err()->msg)));
        return false;
    }
    avp_vec_push(&st->deps, avp_str(in_file));
    if (st->depth >= DECOMPCTX_MAX_DEPTH) {
        st->fatal = true;
        fail(st, "RecursionError", "maximum recursion depth exceeded");
        return false;
    }
    AvpStr text;
    st->depth++;
    st->failed = false;
    bool ok = false;
    if (avp_read_text(st->a, in_file, AVP_TM_UTF8, &text)) {
        ok = decompctx_process_text(st, in_file, text, out);
    } else {
        fail_io(st);
    }
    if (!ok && !st->fatal) {
        /* except Exception: open(in_file), the locale's encoding */
        st->failed = false;
        if (shsm_locale_read_text(st->a, in_file, &text))
            ok = decompctx_process_text(st, in_file, text, out);
        else
            fail_io(st);
    }
    st->depth--;
    return ok;
}
