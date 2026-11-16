/* permute_cc: cc.unit_flags, permute.find_source and the standard-library pieces (see permute.h). unit_flags is the cc
 * tool's (cctool_unit_flags, tool_cc): cc.py's own reading of build.ninja (the first `mwcc` edge whose head names
 * src\<unit>.), which differs from the unit table's. */
#include "permute.h"

#include <ctype.h>

#include "cctool.h"
#include "shsm_units.h"

const char *permute_repo(void)
{
    static char *repo;
    if (!repo) {
        AvpStr r = avp_str_replace(avp_arena(), avp_str(avp_root()), AVP_S("/"), AVP_S("\\"), -1);
        repo = avp_cz(avp_arena(), r);
    }
    return repo;
}

const char *permute_join(AvpArena *a, const char *first, const char *second) { return avp_path_join(a, first, second); }

AvpStr permute_universal_newlines(AvpArena *a, AvpStr s)
{
    if (!memchr(s.p, '\r', s.n)) return s;
    char *out = avp_alloc(a, s.n + 1);
    size_t n = 0;
    for (size_t i = 0; i < s.n; i++) {
        if (s.p[i] == '\r') {
            out[n++] = '\n';
            if (i + 1 < s.n && s.p[i + 1] == '\n') i++;
        } else {
            out[n++] = s.p[i];
        }
    }
    out[n] = 0;
    return avp_strn(out, n);
}

static AvpStr read_text_or_die(AvpArena *a, const char *path)
{
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TM_UTF8, &text)) avp_fatal("%s", avp_io_err_str(a).p);
    return text;
}

void permute_unit_flags(AvpArena *a, const char *root, AvpStr unit, const char **version, AvpStrVec *cflags)
{
    AvpStr ninja = permute_universal_newlines(a, read_text_or_die(a, permute_join(a, root, "build.ninja")));
    AvpStr ver;
    const char *err = NULL;
    switch (cctool_unit_flags(a, ninja, avp_cz(a, unit), &ver, cflags, &err)) {
    case 0: *version = avp_cz(a, ver); return;
    case 1: avp_fatal("unit not found in build.ninja: %.*s", AVP_SF(unit));
    case 2: avp_fatal("AttributeError: 'NoneType' object has no attribute 'group'");
    default: avp_fatal("ValueError: %s", err);
    }
}

const char *permute_find_source(AvpArena *a, const char *root, AvpStr unit)
{
    static const char *const exts[] = {".c", ".cp", ".cpp"};
    const char *src = permute_join(a, root, "src");
    for (size_t k = 0; k < 3; k++) {
        const char *p = permute_join(a, src, avp_sprintf(a, "%.*s%s", AVP_SF(unit), exts[k]));
        if (avp_exists(p)) return p;
    }
    avp_fatal("no source for unit %.*s", AVP_SF(unit));
}

AvpStr permute_shlex_quote(AvpArena *a, AvpStr s)
{
    if (!s.n) return AVP_S("''");
    bool safe = true;
    for (size_t i = 0; i < s.n && safe; i++) {
        u8 c = (u8)s.p[i];
        safe = (c < 0x80 && (isalnum(c) || c == '_')) || (c && strchr("@%+=:,./-", c));
    }
    if (safe) return s;
    return avp_str(avp_sprintf(a, "'%s'", avp_cz(a, avp_str_replace(a, s, AVP_S("'"), AVP_S("'\"'\"'"), -1))));
}

AvpStr permute_shlex_join(AvpArena *a, const AvpStr *words, size_t n)
{
    AvpStr *q = avp_alloc(a, (n + 1) * sizeof *q);
    for (size_t i = 0; i < n; i++) q[i] = permute_shlex_quote(a, words[i]);
    return avp_str_join(a, AVP_S(" "), q, n);
}

bool permute_run_capture(AvpArena *a, const char *const *argv, const char *cwd, i64 *rc, AvpStr *out, AvpStr *err,
                         const char **msg)
{
    AvpProcResult r = avp_run(&(AvpProcSpec){.argv = argv, .cwd = cwd, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE,
                                             .arena = a});
    if (r.returncode < 0 && r.message) {
        *msg = r.message;
        return false;
    }
    *rc = r.returncode;
    *out = permute_universal_newlines(a, r.out);
    *err = permute_universal_newlines(a, r.err);
    return true;
}
