#include "transform_dep.h"

char *transform_dep_winedevices(AvpArena *a) {
    char *prefix = avp_getenv(a, "WINEPREFIX");
    if (!prefix) {
        char *home = avp_getenv(a, "HOME");
        if (!home) return NULL;
        prefix = avp_path_join(a, home, ".wine");
    }
    return avp_path_join(a, prefix, "dosdevices");
}

/* path[0].lower() + path[1:] */
static AvpStr lower_first(AvpArena *a, AvpStr path) {
    const char *p = path.p, *end = path.p + path.n;
    avp_u8_next(&p, end);
    AvpStr first = avp_strn(path.p, (size_t)(p - path.p));
    AvpStr rest = avp_strn(p, (size_t)(end - p));
    return avp_str_concat(a, avp_str_lower(a, first), rest);
}

AvpStr transform_dep_text(AvpArena *a, AvpStr text, const char *winedevices) {
    AvpBuf out;
    avp_buf_init(&out, a);
    AvpFileLines it;
    avp_file_lines_init(&it, text, AVP_NL_DEFAULT);
    AvpStr line;
    for (size_t idx = 0; avp_file_lines_next(&it, &line); idx++) {
        bool continued = avp_str_endswith(line, AVP_S(" \\\n"));
        if (idx == 0) {
            if (continued) {
                AvpStr head = avp_str_slice(line, 0, -3);
                avp_buf_put(&out, avp_str_replace(a, head, AVP_S("\\"), AVP_S("/"), -1));
                avp_buf_putz(&out, " \\\n");
            } else {
                avp_buf_put(&out, avp_str_replace(a, line, AVP_S("\\"), AVP_S("/"), -1));
            }
            continue;
        }
        const char *suffix = "";
        AvpStr path;
        if (continued) {
            suffix = " \\";
            path = avp_u8_slice(avp_str_lstrip(line), 0, -3);
        } else {
            path = avp_str_strip(line);
        }
        if (path.n == 0) avp_throw(AVP_ERR_OTHER, "IndexError: string index out of range");
        path = lower_first(a, path);
        if (path.p[0] == 'z') {
            /* shortcut for z: */
            path = avp_str_replace(a, avp_u8_slice(path, 2, AVP_END), AVP_S("\\"), AVP_S("/"), -1);
        } else {
            /* use $WINEPREFIX/dosdevices to resolve the path */
            AvpStr unix_like = avp_str_replace(a, path, AVP_S("\\"), AVP_S("/"), -1);
            char *joined = avp_path_join(a, winedevices, avp_cz(a, unix_like));
            path = avp_str(avp_path_realpath(a, joined));
        }
        avp_buf_putc(&out, '\t');
        avp_buf_put(&out, path);
        avp_buf_putz(&out, suffix);
        avp_buf_putc(&out, '\n');
    }
    return avp_buf_str(&out);
}
