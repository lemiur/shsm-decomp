/* args.h against CPython 3.14.8's argparse: the corpus in args_corpus.h (generated, see its comment for the Python
 * that produced it) holds the parser definitions of the project's tools (dumped from the real scripts) plus
 * synthetic definitions for the features the tools do not use, with argument vectors and the exact results, exit
 * codes, stdout and error messages argparse gives, and format_usage()/format_help() texts at several widths. The
 * parsers are rebuilt here from the recorded definitions through the C API. */
#include "avp/args.h"
#include "avp/avp.h"
#include "avp/test.h"
#include "args_corpus.h"

#include "../src/args_internal.h"

#include <math.h>
#include <stdlib.h>

static size_t g_pos; /* the cursor in args_corpus */
static const char *cur(void) { return g_pos < ARGS_CORPUS_LEN ? args_corpus[g_pos] : ""; }
static bool at(const char *tag) { return strcmp(cur(), tag) == 0; }
/* if the current entry starts with tag, returns the rest and advances */
static const char *take(const char *tag) {
    size_t n = strlen(tag);
    if (strncmp(cur(), tag, n) != 0) return NULL;
    return args_corpus[g_pos++] + n;
}

/* The corpus comes from CPython 3.14.8, which quotes the choices of an "invalid choice" message (repr); the reference
 * interpreter of the tools, CPython 3.14.3, joins them with str (args_errors_test.c checks that form against it).
 * Turns "(choose from 'a', 'b')" into "(choose from a, b)"; the project's choices hold no quotes. */
static const char *msg_for_reference(const char *msg) {
    const char *at = msg ? strstr(msg, " (choose from ") : NULL;
    if (!at) return msg;
    AvpBuf b;
    avp_buf_init(&b, avp_arena());
    avp_buf_put(&b, (AvpStr){msg, (size_t)(at - msg)});
    for (const char *q = at; *q; q++)
        if (*q != '\'') avp_buf_putc(&b, *q);
    return avp_buf_cz(&b);
}

static int g_failures;
static size_t g_vectors, g_texts_checked, g_parsers;
static void fail(const char *what, size_t entry) {
    if (g_failures++ < 20) fprintf(stderr, "args corpus entry %zu: %s\n", entry, what);
    avp_test__failed_checks++;
}

static char *dupc(const char *s) { return avp_strdup(avp_arena(), s); }

/* ---- loading definitions ---------------------------------------------------------------------------------------- */
typedef struct {
    AvpArgParser *p;
    char *path;
    size_t pid;
} Loaded;
static AVP_VEC(Loaded) g_loaded;
static AVP_VEC(AvpArgParser *) g_top;
typedef AVP_VEC(const char *) DestVec;
static AVP_VEC(DestVec) g_dests; /* per top-level parser: every dest its arguments declare */

static void add_dest(size_t pid, const char *dest) {
    while (g_dests.n <= pid) avp_vec_push_zero(&g_dests);
    DestVec *dv = &g_dests.v[pid];
    for (size_t i = 0; i < dv->n; i++)
        if (strcmp(dv->v[i], dest) == 0) return;
    avp_vec_push(dv, dest);
}

/* "<sep>a<sep>b": a NULL-terminated array of copies; "" is the empty list */
static const char **parse_joined(const char *s, size_t *count) {
    AvpCzVec v = {0};
    if (*s) {
        char sep = *s++;
        for (;;) {
            const char *q = strchr(s, sep);
            if (!q) {
                avp_vec_push(&v, dupc(s));
                break;
            }
            avp_vec_push(&v, avp_cz(avp_arena(), avp_strn(s, (size_t)(q - s))));
            s = q + 1;
        }
    }
    if (count) *count = v.n;
    avp_vec_push(&v, NULL);
    return (const char **)v.v;
}

static AvpArgDefault parse_default(const char *s) {
    switch (s[0]) {
    case 'N': return AVP_DEF_NONE;
    case 'X': return AVP_DEF_SUPPRESS;
    case 'B': return AVP_DEF_BOOL(s[1] == '1');
    case 'I': return AVP_DEF_INT(strtoll(s + 1, NULL, 10));
    case 'F': {
        u64 u = strtoull(s + 1, NULL, 16);
        double d;
        memcpy(&d, &u, sizeof d);
        return AVP_DEF_FLOAT(d);
    }
    default: return AVP_DEF_STR(dupc(s + 1));
    }
}

static const char *natural_dest(const char *names) {
    /* the first long option (else the first option), without its dashes, '-' to '_' */
    AvpStrVec v = avp_str_split(avp_str(names), AVP_S(""), -1);
    AvpStr pick = v.v[0];
    for (size_t i = 0; i < v.n; i++)
        if (v.v[i].n > 1 && v.v[i].p[1] == '-') {
            pick = v.v[i];
            break;
        }
    size_t k = 0;
    while (k < pick.n && pick.p[k] == '-') k++;
    char *d = avp_cz(avp_arena(), avp_strn(pick.p + k, pick.n - k));
    for (char *c = d; *c; c++)
        if (*c == '-') *c = '_';
    avp_vec_free(&v);
    return d;
}

static void load_block(AvpArgParser *p, size_t pid, const char *path);

static void read_parser_fields(AvpArgParserDesc *d, const char **prog) {
    const char *v;
    for (;;) {
        if ((v = take("pr:"))) *prog = dupc(v);
        else if ((v = take("ds:"))) d->description = dupc(v);
        else if ((v = take("ep:"))) d->epilog = dupc(v);
        else if ((v = take("fm:"))) d->formatter = (AvpArgFormatter)atoi(v);
        else if (take("nh:")) d->no_help = true;
        else if (take("na:")) d->no_abbrev = true;
        else break;
    }
}

static void load_arg(AvpArgParser *p, AvpArgMutex **mx, size_t pid) {
    AvpArgDesc d;
    memset(&d, 0, sizeof d);
    int mutex = -1;
    const char *v;
    bool explicit_dest = false;
    while (!at("E")) {
        if ((v = take("n:"))) d.names = dupc(v);
        else if ((v = take("k:"))) d.action = (AvpArgActionKind)atoi(v);
        else if ((v = take("ty:"))) d.type = (AvpArgType)atoi(v);
        else if ((v = take("tn:"))) d.type_name = dupc(v);
        else if ((v = take("ng:"))) d.nargs = atoi(v);
        else if ((v = take("dt:"))) {
            d.dest = dupc(v);
            explicit_dest = true;
        } else if ((v = take("mv:"))) d.metavar = dupc(v);
        else if ((v = take("mt:"))) d.metavars = parse_joined(v, NULL);
        else if ((v = take("ch:"))) d.choices = parse_joined(v, NULL);
        else if ((v = take("df:"))) d.dflt = parse_default(v);
        else if ((v = take("dl:"))) d.dflt_list = parse_joined(v, NULL);
        else if ((v = take("cn:"))) d.cnst = parse_default(v);
        else if (take("rq:")) d.required = true;
        else if (take("hs:")) d.help = AVP_ARG_SUPPRESS;
        else if ((v = take("hp:"))) d.help = dupc(v);
        else if ((v = take("mx:"))) mutex = atoi(v);
        else if ((v = take("vs:"))) d.version = dupc(v);
        else {
            fail("unknown argument field", g_pos);
            g_pos++;
        }
    }
    g_pos++; /* E */
    if (d.action != AVP_ARG_HELP && d.action != AVP_ARG_VERSION) {
        if (explicit_dest) add_dest(pid, d.dest);
        else if (d.names[0] == '-') add_dest(pid, natural_dest(d.names));
        else add_dest(pid, d.names);
    }
    if (mutex >= 0) avp_arg_mutex(mx[mutex], &d);
    else avp_arg(p, &d);
}

static void load_subparsers(AvpArgParser *p, size_t pid, const char *path) {
    g_pos++; /* SB */
    AvpArgSubparsersDesc sd;
    memset(&sd, 0, sizeof sd);
    const char *v;
    for (;;) {
        if ((v = take("sd:"))) {
            sd.dest = dupc(v);
            add_dest(pid, sd.dest);
        } else if (take("sr:")) sd.required = true;
        else if ((v = take("sm:"))) sd.metavar = dupc(v);
        else if ((v = take("sh:"))) sd.help = dupc(v);
        else break;
    }
    AvpArgSubparsers *subs = avp_args_subparsers(p, &sd);
    while ((v = take("SP:"))) {
        const char *name = dupc(v);
        AvpArgParserDesc d;
        memset(&d, 0, sizeof d);
        if ((v = take("sa:"))) d.aliases = parse_joined(v, NULL);
        if ((v = take("sp:"))) d.help = dupc(v);
        const char *want_prog = NULL;
        read_parser_fields(&d, &want_prog);
        AvpArgParser *sub = avp_args_subparser(subs, name, &d);
        if (strcmp(avp_args_prog(sub), want_prog) != 0) {
            fprintf(stderr, "sub-parser prog: got \"%s\", want \"%s\"\n", avp_args_prog(sub), want_prog);
            fail("sub-parser prog", g_pos);
        }
        char *sub_path = path[0] ? avp_sprintf(avp_arena(), "%s/%s", path, name) : dupc(name);
        load_block(sub, pid, sub_path);
        if (!take("SPE")) fail("SPE expected", g_pos);
    }
    if (!take("SBE")) fail("SBE expected", g_pos);
}

static void load_block(AvpArgParser *p, size_t pid, const char *path) {
    Loaded l = {p, dupc(path), pid};
    avp_vec_push(&g_loaded, l);
    g_parsers++;
    AvpArgMutex *mx[16];
    size_t nmx = 0;
    const char *v;
    for (;;) {
        if ((v = take("MX:"))) {
            AVP_ASSERT(nmx < 16);
            mx[nmx++] = avp_args_mutex(p, v[0] == '1');
        } else if (at("A")) {
            g_pos++;
            load_arg(p, mx, pid);
        } else if (at("SB")) {
            load_subparsers(p, pid, path);
        } else break;
    }
}

static AvpArgParser *find_parser(size_t pid, const char *path) {
    for (size_t i = 0; i < g_loaded.n; i++)
        if (g_loaded.v[i].pid == pid && strcmp(g_loaded.v[i].path, path) == 0) return g_loaded.v[i].p;
    return NULL;
}

static void load_parser(void) {
    g_pos++; /* P */
    AvpArgParserDesc d;
    memset(&d, 0, sizeof d);
    const char *prog = NULL;
    read_parser_fields(&d, &prog);
    d.prog = prog;
    AvpArgParser *p = avp_args_parser(NULL, &d);
    size_t pid = g_top.n;
    avp_vec_push(&g_top, p);
    load_block(p, pid, "");
    if (!take("PE")) fail("PE expected", g_pos);
}

/* ---- texts ------------------------------------------------------------------------------------------------------ */
typedef struct {
    size_t pid;
    char *path;
    int width;
    const char *usage;
} UsageRec;
static AVP_VEC(UsageRec) g_usages;

static void set_columns(int w) {
    char b[16];
    snprintf(b, sizeof b, "%d", w);
    _putenv_s("COLUMNS", b);
}

static void check_texts(void) {
    size_t entry = g_pos;
    size_t pid;
    int width;
    char path[256];
    const char *line = take("T ");
    char *endp;
    pid = (size_t)strtoull(line, &endp, 10);
    const char *last = strrchr(line, ' ');
    width = atoi(last + 1);
    size_t plen = (size_t)(last - (endp + 1));
    memcpy(path, endp + 1, plen);
    path[plen] = 0;
    const char *tu = take("tu:");
    const char *th = take("th:");
    AvpArgParser *p = find_parser(pid, path);
    if (!p || !tu) {
        fail("T record without parser", entry);
        return;
    }
    set_columns(width);
    AvpStr u = avp_args_format_usage(p);
    if (!avp_str_eqz(u, tu)) {
        fail("format_usage", entry);
        if (g_failures < 4) fprintf(stderr, "--- got:\n%.*s--- want:\n%s---\n", AVP_SF(u), tu);
    }
    if (th) {
        AvpStr h = avp_args_format_help(p);
        if (!avp_str_eqz(h, th)) {
            fail("format_help", entry);
            if (g_failures < 4) fprintf(stderr, "--- got:\n%.*s--- want:\n%s---\n", AVP_SF(h), th);
        }
    }
    UsageRec r = {pid, dupc(path), width, tu};
    avp_vec_push(&g_usages, r);
    g_texts_checked++;
}

/* ---- vectors ---------------------------------------------------------------------------------------------------- */
static void canon(AvpBuf *b, const AvpArgValue *v) {
    switch (v->kind) {
    case AVP_V_NONE: avp_buf_putc(b, 'N'); break;
    case AVP_V_BOOL: avp_buf_printf(b, "B%d", v->i ? 1 : 0); break;
    case AVP_V_INT: avp_buf_printf(b, "I%lld", (long long)v->i); break;
    case AVP_V_FLOAT: {
        u64 u;
        memcpy(&u, &v->f, sizeof u);
        avp_buf_printf(b, "F%016llx", (unsigned long long)u);
        break;
    }
    case AVP_V_STR:
        avp_buf_printf(b, "S%zu:", v->s.n);
        avp_buf_put(b, v->s);
        break;
    default:
        avp_buf_printf(b, "L%zu:", v->n);
        for (size_t i = 0; i < v->n; i++) canon(b, &v->items[i]);
        break;
    }
}

static const UsageRec *find_usage(size_t pid, const char *path, int width) {
    for (size_t i = 0; i < g_usages.n; i++)
        if (g_usages.v[i].pid == pid && g_usages.v[i].width == width && strcmp(g_usages.v[i].path, path) == 0)
            return &g_usages.v[i];
    return NULL;
}

static void run_vector(void) {
    size_t entry = g_pos;
    size_t pid, n;
    int width, known;
    sscanf(take("V "), "%zu %d %d %zu", &pid, &width, &known, &n);
    AvpStr *args = (AvpStr *)avp_alloc(avp_arena(), (n + 1) * sizeof(AvpStr));
    for (size_t i = 0; i < n; i++) {
        const char *w = take("w:");
        args[i] = avp_str(dupc(w ? w : ""));
    }
    AvpArgParser *top = g_top.v[pid];
    set_columns(width);
    AvpArgsExit ex = {0};
    AvpArgs *r = avp_args_try_parse(top, args, n, known != 0, &ex);
    g_vectors++;
    const char *v;
    if ((v = take("ok "))) {
        size_t nns = (size_t)atoi(v);
        if (!r) {
            fail("parse failed where Python succeeded", entry);
            if (g_failures < 5) fprintf(stderr, "got exit %d, out: %.*s err: %.*s\n", ex.code, AVP_SF(ex.out), AVP_SF(ex.err));
        }
        AvpStrVec seen = {0};
        for (size_t i = 0; i < nns; i++) {
            const char *e = take("ns:");
            const char *eq = strchr(e, '=');
            char dest[128];
            size_t dl = (size_t)(eq - e);
            memcpy(dest, e, dl);
            dest[dl] = 0;
            avp_vec_push(&seen, avp_strn(e, dl));
            if (!r) continue;
            const AvpArgValue *val = avp_args_value(r, dest);
            if (!val) {
                fail("missing attribute", entry);
                if (g_failures < 5) fprintf(stderr, "  dest %s\n", dest);
                continue;
            }
            AvpBuf b;
            avp_buf_init(&b, NULL);
            canon(&b, val);
            if (strcmp(avp_buf_cz(&b), eq + 1) != 0) {
                fail("value", entry);
                if (g_failures < 8) fprintf(stderr, "  dest %s: got %s want %s\n", dest, avp_buf_cz(&b), eq + 1);
            }
            avp_buf_free(&b);
        }
        const char *ex_canon = take("ex:");
        if (r) {
            /* declared dests that Python did not set must be absent */
            DestVec *dv = pid < g_dests.n ? &g_dests.v[pid] : NULL;
            for (size_t i = 0; dv && i < dv->n; i++) {
                bool expected = false;
                for (size_t k = 0; k < seen.n; k++)
                    if (avp_str_eqz(seen.v[k], dv->v[i])) expected = true;
                if (!expected && avp_args_has(r, dv->v[i])) {
                    fail("unexpected attribute", entry);
                    if (g_failures < 8) fprintf(stderr, "  dest %s\n", dv->v[i]);
                }
            }
            AvpArgValue extras = {0};
            extras.kind = AVP_V_LIST;
            AvpStrVec ev = avp_args_extras(r);
            AvpArgValue *items = (AvpArgValue *)avp_alloc(avp_arena(), (ev.n + 1) * sizeof(AvpArgValue));
            for (size_t i = 0; i < ev.n; i++) {
                items[i].kind = AVP_V_STR;
                items[i].s = ev.v[i];
            }
            extras.items = items;
            extras.n = ev.n;
            AvpBuf b;
            avp_buf_init(&b, NULL);
            canon(&b, &extras);
            if (strcmp(avp_buf_cz(&b), ex_canon) != 0) {
                fail("extras", entry);
                if (g_failures < 8) fprintf(stderr, "  got %s want %s\n", avp_buf_cz(&b), ex_canon);
            }
            avp_buf_free(&b);
        }
        avp_vec_free(&seen);
    } else if ((v = take("x "))) {
        int code;
        char path[256] = "";
        if (sscanf(v, "%d %255s", &code, path) < 1) fail("bad x record", entry);
        const char *so = take("so:");
        const char *msg = code == 2 ? msg_for_reference(take("msg:")) : NULL;
        if (r) {
            fail("parse succeeded where Python exited", entry);
        } else {
            if (ex.code != code) fail("exit code", entry);
            if (!avp_str_eqz(ex.out, so ? so : "")) {
                fail("stdout", entry);
                if (g_failures < 6) fprintf(stderr, "--- got:\n%.*s--- want:\n%s---\n", AVP_SF(ex.out), so ? so : "");
            }
            if (code == 2) {
                const UsageRec *u = find_usage(pid, path, width);
                AvpArgParser *ep = find_parser(pid, path);
                if (!u || !ep) fail("no usage record for the error", entry);
                else {
                    AvpBuf want;
                    avp_buf_init(&want, NULL);
                    avp_buf_putz(&want, u->usage);
                    avp_buf_printf(&want, "%s: error: %s\n", avp_args_prog(ep), msg ? msg : "");
                    if (!avp_str_eqz(ex.err, avp_buf_cz(&want))) {
                        fail("stderr", entry);
                        if (g_failures < 6) fprintf(stderr, "--- got:\n%.*s--- want:\n%s---\n", AVP_SF(ex.err), avp_buf_cz(&want));
                    }
                    avp_buf_free(&want);
                }
            } else if (ex.err.n) fail("unexpected stderr", entry);
        }
    } else fail("bad vector result", entry);
    while (!at("VE") && g_pos < ARGS_CORPUS_LEN) g_pos++;
    g_pos++;
}

AVP_TEST(corpus_against_cpython) {
    g_pos = 0;
    g_failures = 0;
    while (g_pos < ARGS_CORPUS_LEN) {
        if (at("P")) load_parser();
        else if (cur()[0] == 'T' && cur()[1] == ' ') check_texts();
        else if (cur()[0] == 'V' && cur()[1] == ' ') run_vector();
        else {
            fail("unexpected corpus entry", g_pos);
            g_pos++;
        }
    }
    CHECK(g_parsers > 100);
    CHECK(g_vectors > 1500);
    CHECK(g_texts_checked > 300);
    printf("args corpus: %zu parsers (with sub-parsers), %zu vectors, %zu usage/help texts, %d failures\n", g_parsers,
           g_vectors, g_texts_checked, g_failures);
}

/* ---- API behaviour not in the corpus ---------------------------------------------------------------------------- */
AVP_TEST(accessors_and_given) {
    set_columns(80);
    AvpArgParser *p = avp_args_parser(NULL, &(AvpArgParserDesc){.prog = "demo.py"});
    avp_arg(p, &(AvpArgDesc){.names = "-j --jobs", .type = AVP_T_INT, .dflt = AVP_DEF_INT(4), .metavar = "N"});
    avp_arg(p, &(AvpArgDesc){.names = "--name"});
    avp_arg(p, &(AvpArgDesc){.names = "--ratio", .type = AVP_T_FLOAT, .dflt = AVP_DEF_FLOAT(0.5)});
    avp_arg(p, &(AvpArgDesc){.names = "-q", .action = AVP_ARG_STORE_TRUE});
    avp_arg(p, &(AvpArgDesc){.names = "-v", .action = AVP_ARG_COUNT});
    avp_arg(p, &(AvpArgDesc){.names = "units", .nargs = AVP_NARGS_STAR});
    AvpStr argv[] = {AVP_S("-j8"), AVP_S("-qvv"), AVP_S("a"), AVP_S("b")};
    AvpArgsExit ex;
    AvpArgs *r = avp_args_try_parse(p, argv, 4, false, &ex);
    if (!r) fprintf(stderr, "exit %d: %.*s%.*s", ex.code, AVP_SF(ex.out), AVP_SF(ex.err));
    REQUIRE(r != NULL);
    CHECK_EQ_I64(avp_args_int(r, "jobs"), 8);
    CHECK(avp_args_given(r, "jobs"));
    CHECK(avp_args_is_none(r, "name"));
    CHECK(!avp_args_given(r, "name"));
    CHECK_EQ_F64(avp_args_float(r, "ratio"), 0.5);
    CHECK(!avp_args_given(r, "ratio"));
    CHECK(avp_args_bool(r, "q"));
    CHECK_EQ_I64(avp_args_int(r, "v"), 2);
    AvpStrVec units = avp_args_strs(r, "units");
    REQUIRE(units.n == 2);
    CHECK_STR(units.v[0], "a");
    CHECK_STR(units.v[1], "b");
    avp_vec_free(&units);
    CHECK(avp_args_str(r, "name").p == NULL);
}

AVP_TEST(error_text_and_exit) {
    set_columns(80);
    AvpArgParser *p = avp_args_parser(NULL, &(AvpArgParserDesc){.prog = "demo.py"});
    avp_arg(p, &(AvpArgDesc){.names = "--build", .choices = (const char *const[]){"dx9", "dx11", NULL}});
    AvpStr argv[] = {AVP_S("--build"), AVP_S("dx10")};
    AvpArgsExit ex;
    AvpArgs *r = avp_args_try_parse(p, argv, 2, false, &ex);
    CHECK(r == NULL);
    CHECK_EQ_I64(ex.code, 2);
    CHECK_STR(ex.err, "usage: demo.py [-h] [--build {dx9,dx11}]\n"
                      "demo.py: error: argument --build: invalid choice: 'dx10' (choose from dx9, dx11)\n");
    AvpStr h[] = {AVP_S("-h")};
    r = avp_args_try_parse(p, h, 1, false, &ex);
    CHECK(r == NULL);
    CHECK_EQ_I64(ex.code, 0);
    CHECK_STR(ex.out, "usage: demo.py [-h] [--build {dx9,dx11}]\n\noptions:\n  -h, --help          show this help message and exit\n  --build {dx9,dx11}\n");
    CHECK_STR(avp_args_error_text(p, AVP_S("custom")), "usage: demo.py [-h] [--build {dx9,dx11}]\ndemo.py: error: custom\n");
}

static bool even_conv(void *ctx, AvpArena *a, AvpStr in, AvpArgValue *out, AvpStr *msg) {
    AVP_UNUSED(ctx);
    AVP_UNUSED(a);
    i64 v;
    if (!avp_parse_int(in, 10, &v, NULL)) return false;
    if (v % 2) {
        *msg = AVP_S("must be even");
        return false;
    }
    out->kind = AVP_V_INT;
    out->i = v;
    return true;
}

AVP_TEST(custom_type_converter) {
    set_columns(80);
    AvpArgParser *p = avp_args_parser(NULL, &(AvpArgParserDesc){.prog = "demo.py"});
    avp_arg(p, &(AvpArgDesc){.names = "--n", .type = AVP_T_CUSTOM, .type_name = "even", .conv = even_conv});
    AvpArgsExit ex;
    AvpStr ok[] = {AVP_S("--n"), AVP_S("4")};
    AvpArgs *r = avp_args_try_parse(p, ok, 2, false, &ex);
    REQUIRE(r != NULL);
    CHECK_EQ_I64(avp_args_int(r, "n"), 4);
    AvpStr odd[] = {AVP_S("--n=3")};
    CHECK(avp_args_try_parse(p, odd, 1, false, &ex) == NULL);
    CHECK_STR(ex.err, "usage: demo.py [-h] [--n N]\ndemo.py: error: argument --n: must be even\n");
    AvpStr bad[] = {AVP_S("--n"), AVP_S("x")};
    CHECK(avp_args_try_parse(p, bad, 2, false, &ex) == NULL);
    CHECK_STR(ex.err, "usage: demo.py [-h] [--n N]\ndemo.py: error: argument --n: invalid even value: 'x'\n");
}

AVP_TEST(width_from_columns) {
    set_columns(100);
    CHECK_EQ_I64(avp_args_width(), 98); /* shutil.get_terminal_size().columns - 2 */
    _putenv_s("COLUMNS", " 60 ");
    CHECK_EQ_I64(avp_args_width(), 58); /* int(" 60 ") is 60 */
    set_columns(80);
    CHECK_EQ_I64(avp_args_width(), 78);
}

AVP_TEST(huge_int_is_rejected) {
    /* Python's int is unbounded; this port's is 64-bit, so a value that does not fit is "invalid int value" */
    set_columns(80);
    AvpArgParser *p = avp_args_parser(NULL, &(AvpArgParserDesc){.prog = "demo.py"});
    avp_arg(p, &(AvpArgDesc){.names = "--n", .type = AVP_T_INT});
    AvpStr big[] = {AVP_S("--n"), AVP_S("99999999999999999999")};
    AvpArgsExit ex;
    CHECK(avp_args_try_parse(p, big, 2, false, &ex) == NULL);
    CHECK_STR(ex.err, "usage: demo.py [-h] [--n N]\ndemo.py: error: argument --n: invalid int value: '99999999999999999999'\n");
}

/* python -c "print(repr(x))" for each value (the %(default)s of a float default in help texts) */
AVP_TEST(float_repr) {
    static const struct {
        double x;
        const char *repr;
    } cases[] = {
    {0.0, "0.0"},
    {-0.0, "-0.0"},
    {1.0, "1.0"},
    {0.1, "0.1"},
    {0.25, "0.25"},
    {1e+16, "1e+16"},
    {1000000000000000.0, "1000000000000000.0"},
    {123456789012345.6, "123456789012345.6"},
    {1e-05, "1e-05"},
    {0.0001, "0.0001"},
    {1.234e-05, "1.234e-05"},
    {5e-324, "5e-324"},
    {1.7976931348623157e+308, "1.7976931348623157e+308"},
    {0.3333333333333333, "0.3333333333333333"},
    {100.0, "100.0"},
    {2.5e-07, "2.5e-07"},
    {1.2345678901234567e+19, "1.2345678901234567e+19"},
    {0.5, "0.5"},
    {-2.5, "-2.5"},
    {1e+22, "1e+22"},
    {1e+23, "1e+23"},
    {1e+23, "1e+23"},
    {INFINITY, "inf"},
    {-INFINITY, "-inf"},
    {3.141592653589793, "3.141592653589793"},
    {9007199254740992.0, "9007199254740992.0"},
    {1234567890123456.0, "1234567890123456.0"},
    {1.2345678901234568e+16, "1.2345678901234568e+16"},
    {0.001, "0.001"},
    {1.5e+300, "1.5e+300"},
    {4.35, "4.35"},
    {0.3, "0.3"}
    };
    for (size_t i = 0; i < AVP_ARRAY_LEN(cases); i++) {
        AvpBuf b;
        avp_buf_init(&b, NULL);
        argp_float_repr(&b, cases[i].x);
        CHECK_CZ(avp_buf_cz(&b), cases[i].repr);
        avp_buf_free(&b);
    }
}

AVP_TEST_MAIN("args")
