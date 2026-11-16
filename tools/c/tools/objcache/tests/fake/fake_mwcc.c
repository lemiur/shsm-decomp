/* objcache_fake_mwcc.exe: the fake compiler of tests/wrapper_test.c (test-only; built into the tests output folder).
 *
 *     objcache_fake_mwcc.exe <bin/mwcceppc.fake> [-i DIR]... [-c SRC] [-o OUTDIR] [other words]...
 *
 * The first argument is the compiler's identity file (the "mwcceppc" word objcache keys on; its bytes stand for the
 * compiler's version). The run follows `#include "name"` lines through the includer's directory and then each -i
 * directory, writes OUTDIR/<stem>.o (depending on its arguments and every byte it read) and OUTDIR/<stem>.d in MWCC's
 * -MMD format (absolute header paths), and appends the source to calls.log beside the identity file, so a test can
 * tell a hit from a miss. A file containing `#error` fails (exit 1); one containing `WARN` prints a warning. */
#include "avp/avp.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    AvpArena *a;
    AvpStrVec inc, read;
    AvpBuf body;
    bool warn;
} Fake;

static bool contains(AvpStr hay, const char *needle) { return avp_str_contains(hay, avp_str(needle)); }

static void visit(Fake *f, const char *path) {
    AvpBytes b = {0};
    if (!avp_read_all(f->a, path, &b)) {
        printf("error: cannot read %s\n", path);
        avp_exit(1);
    }
    AvpStr data = {(const char *)b.v, b.n};
    if (contains(data, "#error")) {
        printf("error: %s\n", path);
        avp_exit(1);
    }
    if (contains(data, "WARN")) f->warn = true;
    avp_buf_put(&f->body, data);
    AvpStrVec lines = avp_str_splitlines(data, false);
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr line = lines.v[i];
        if (!avp_str_startswith(line, AVP_S("#include \""))) continue;
        AvpStr rest = avp_str_slice(line, 10, line.n);
        const char *q = memchr(rest.p, '"', rest.n);
        if (!q) continue;
        const char *name = avp_cz(f->a, (AvpStr){rest.p, (size_t)(q - rest.p)});
        const char *found = NULL;
        const char *first = avp_cz(f->a, avp_path_dirname(path));
        for (size_t k = 0; k <= f->inc.n && !found; k++) {
            const char *d = k == 0 ? first : avp_cz(f->a, f->inc.v[k - 1]);
            const char *p = avp_path_join(f->a, d, name);
            if (avp_isfile(p)) found = p;
        }
        if (!found) {
            printf("error: cannot find %s\n", name);
            avp_exit(1);
        }
        AvpStr ap = avp_str(avp_path_abspath(f->a, found));
        bool seen = false;
        for (size_t k = 0; k < f->read.n && !seen; k++) seen = avp_str_eq(f->read.v[k], ap);
        if (!seen) {
            avp_vec_push(&f->read, ap);
            visit(f, found);
        }
    }
}

static int run(int argc, char **argv) {
    AvpArena *a = avp_arena();
    if (argc < 2) {
        fprintf(stderr, "usage: objcache_fake_mwcc <identity file> args...\n");
        return 2;
    }
    Fake f = {0};
    f.a = a;
    avp_buf_init(&f.body, a);
    const char *src = NULL, *out = NULL;
    for (int i = 2; i < argc;) {
        const char *w = argv[i];
        if ((!strcmp(w, "-i") || !strcmp(w, "-c") || !strcmp(w, "-o")) && i + 1 < argc) {
            if (!strcmp(w, "-i")) avp_vec_push(&f.inc, avp_str(argv[i + 1]));
            else if (!strcmp(w, "-c")) src = argv[i + 1];
            else out = argv[i + 1];
            i += 2;
            continue;
        }
        i++;
    }
    if (!src || !out) {
        fprintf(stderr, "objcache_fake_mwcc: -c and -o are required\n");
        return 2;
    }
    const char *bin = avp_cz(a, avp_path_dirname(avp_path_abspath(a, argv[1])));
    AvpStr call = avp_str(avp_sprintf(a, "%s\n", src));
    if (!avp_append_file(avp_path_join(a, bin, "calls.log"), call.p, call.n)) return 3;
    visit(&f, src);
    if (f.warn) printf("warning: in %s\n", avp_path_abspath(a, src));
    AvpStr base = avp_path_basename(src);
    const char *dot = NULL;
    for (size_t i = base.n; i > 0; i--)
        if (base.p[i - 1] == '.') {
            dot = base.p + i - 1;
            break;
        }
    const char *stem = avp_cz(a, dot && dot != base.p ? (AvpStr){base.p, (size_t)(dot - base.p)} : base);
    if (!avp_makedirs(out, true)) return 3;
    AvpBuf obj;
    avp_buf_init(&obj, a);
    avp_buf_putz(&obj, "FAKEOBJ");
    avp_buf_putc(&obj, '\0');
    avp_buf_put(&obj, base);
    avp_buf_putc(&obj, '\0');
    bool firstw = true;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], out)) continue;
        if (!firstw) avp_buf_putc(&obj, ' ');
        avp_buf_putz(&obj, argv[i]);
        firstw = false;
    }
    avp_buf_putc(&obj, '\0');
    u8 digest[32];
    avp_sha256(f.body.p, f.body.n, digest);
    avp_buf_put(&obj, (AvpStr){(const char *)digest, 32});
    if (!avp_write_file(avp_path_join(a, out, avp_sprintf(a, "%s.o", stem)), obj.p, obj.n)) return 3;
    AvpBuf dep;
    avp_buf_init(&dep, a);
    avp_buf_printf(&dep, "%s\\%s.o: %s", out, stem, src);
    for (size_t i = 0; i < f.read.n; i++) {
        avp_buf_putz(&dep, " \\\r\n\t");
        avp_buf_put(&dep, avp_str_replace(a, f.read.v[i], AVP_S(" "), AVP_S("\\ "), -1));
    }
    avp_buf_putz(&dep, " \r\n");
    if (!avp_write_file(avp_path_join(a, out, avp_sprintf(a, "%s.d", stem)), dep.p, dep.n)) return 3;
    return 0;
}

AVP_MAIN(run, "objcache_fake_mwcc")
