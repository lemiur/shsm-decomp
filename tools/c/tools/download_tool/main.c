/* download_tool.exe: front end of tools/c/tools/download_tool (a port of python tools/download_tool.py).
 *
 * Test seam (C side only): when SHSM_DOWNLOAD_TOOL_MIRROR is set to "http://host:port", the request goes to that server
 * with the path of the real URL, and the system proxy is not used. The progress line still names the real URL. */
#include <stdio.h>
#include <stdlib.h>

#include "download_tool.h"
#include "dt_http.h"
#include "shsm_locale.h"

#define SCRIPT "download_tool.exe"

typedef struct {
    const char *output;
    bool is_zip;
    AvpBytes body;  /* a zip response, read whole */
    AvpFile *file;  /* any other response */
    bool failed;
} Download;

static bool begin(void *ctx) {
    Download *d = ctx;
    if (d->is_zip) return true;
    d->file = avp_fopen(d->output, "wb", AVP_TM_UTF8);
    if (!d->file) {
        d->failed = true;
        return false;
    }
    return true;
}

static bool data(void *ctx, const void *p, size_t n) {
    Download *d = ctx;
    if (d->is_zip) {
        avp_vec_reserve(&d->body, d->body.n + n);
        memcpy(d->body.v + d->body.n, p, n);
        d->body.n += n;
        return true;
    }
    if (!avp_fwrite(d->file, p, n)) {
        d->failed = true;
        return false;
    }
    return true;
}

/* The URL the request really goes to: the real one, or the test mirror with the real URL's path. */
static const char *fetch_url(AvpArena *a, const char *url, bool *no_proxy) {
    const char *mirror = getenv("SHSM_DOWNLOAD_TOOL_MIRROR");
    *no_proxy = false;
    if (!mirror || !*mirror) return url;
    const char *p = strstr(url, "://");
    p = p ? strchr(p + 3, '/') : NULL;
    *no_proxy = true;
    return avp_sprintf(a, "%s%s", mirror, p ? p : "/");
}

static int run(int argc, char **argv) {
    AvpArena *a = avp_arena();
    AvpArgParser *p = avp_args_parser(a, &(AvpArgParserDesc){.prog = "download_tool.exe"});
    avp_arg(p, &(AvpArgDesc){.names = "tool", .help = "Tool name"});
    avp_arg(p, &(AvpArgDesc){.names = "output", .type = AVP_T_IDENT, .type_name = "Path", .help = "output file path"});
    avp_arg(p, &(AvpArgDesc){.names = "--tag", .required = true, .help = "GitHub tag"});
    AvpArgs *args = avp_args_parse(p, argc, argv);

    const char *tool = avp_cz(a, avp_args_str(args, "tool"));
    const char *tag = avp_cz(a, avp_args_str(args, "tag"));
    char *url = dt_tool_url(a, tool, tag, "windows", dt_machine(a));
    if (!url) shsm_py_traceback(SCRIPT, "KeyError", "%s", avp_cz(a, avp_repr_str(a, avp_str(tool))));
    const char *output = avp_pl_str(a, avp_cz(a, avp_args_str(args, "output")));

    printf("Downloading %s to %s\n", url, output);

    Download d;
    memset(&d, 0, sizeof d);
    d.output = output;
    d.is_zip = strlen(url) >= 4 && strcmp(url + strlen(url) - 4, ".zip") == 0;
    bool no_proxy;
    const char *target = fetch_url(a, url, &no_proxy);
    DtHttpResult r;
    DtSink sink = {begin, data, &d};
    if (!dt_http_get(target, no_proxy, &sink, &r)) {
        if (d.failed) {
            if (d.file) avp_fclose(d.file);
            shsm_py_traceback_io(SCRIPT);
        }
        if (r.status)
            shsm_py_traceback(SCRIPT, "urllib.error.HTTPError", "HTTP Error %d: %s", r.status, r.reason);
        shsm_py_traceback(SCRIPT, "urllib.error.URLError", "<urlopen error %s>", r.error);
    }
    if (d.is_zip) {
        DtErr err;
        if (!dt_zip_extract_all(a, d.body.v, d.body.n, output, &err)) shsm_py_traceback(SCRIPT, err.cls, "%s", err.text);
        /* make all files executable */
        AvpWalk *w = avp_walk_open(output, true);
        if (w) {
            AvpWalkItem item;
            memset(&item, 0, sizeof item);
            while (avp_walk_next(w, &item)) {
                for (size_t i = 0; i < item.files.n; i++) {
                    char *f = avp_path_join(a, avp_cz(a, item.dirpath), avp_cz(a, item.files.v[i]));
                    if (!avp_chmod(f, 0755)) shsm_py_traceback_io(SCRIPT);
                }
            }
            avp_walk_close(w);
        }
        /* output.touch(mode=0o755): update the directory's mtime, or create the file when nothing was extracted */
        if (!avp_utime_now(output)) {
            AvpFile *f = avp_fopen(output, "wb", AVP_TM_UTF8);
            if (!f) shsm_py_traceback_io(SCRIPT);
            avp_fclose(f);
        }
    } else {
        if (!avp_fclose(d.file)) shsm_py_traceback_io(SCRIPT);
        if (!avp_chmod(output, 0755)) shsm_py_traceback_io(SCRIPT);
    }
    return 0;
}

AVP_MAIN(run, "download_tool")
