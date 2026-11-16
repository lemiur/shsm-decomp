/* CLI conventions (tools/common/cli.py). */
#include "shsm_cli.h"

#include "avp/clock.h"
#include "avp/fmt.h"
#include "avp/fs.h"
#include "avp/pool.h"
#include "avp/proc.h"

#include <stdio.h>

/* ---- exceptions ---- */

void shsm_raise(int exc, const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    avp_throw(exc, "%s", buf);
}

int shsm_io_exc_code(void) {
    const AvpIoErr *e = avp_io_err();
    if (e->kind == AVP_IOE_OS)
        return strcmp(avp_io_err_class(), "FileNotFoundError") == 0 ? SHSM_EXC_FILE_NOT_FOUND : SHSM_EXC_OS;
    if (e->kind == AVP_IOE_DECODE || e->kind == AVP_IOE_ENCODE || e->kind == AVP_IOE_VALUE) return SHSM_EXC_VALUE;
    return SHSM_EXC_OS;
}

void shsm_raise_io(void) {
    int exc = shsm_io_exc_code();
    AvpStr s = avp_io_err_str(avp_arena());
    shsm_raise(exc, "%.*s", AVP_SF(s));
}

const char *shsm_exc_class(int exc) {
    switch (exc) {
    case SHSM_EXC_USAGE: return "UsageError";
    case SHSM_EXC_FINDING: return "Finding";
    case SHSM_EXC_KEY: return "KeyError";
    case SHSM_EXC_UNIT_NOT_BUILT: return "UnitNotBuilt";
    case SHSM_EXC_VALUE: return "ValueError";
    case SHSM_EXC_TYPE: return "TypeError";
    case SHSM_EXC_OS: return "OSError";
    case SHSM_EXC_FILE_NOT_FOUND: return "FileNotFoundError";
    case SHSM_EXC_RECURSION: return "RecursionError";
    case AVP_ERR_IO: return "OSError";
    case AVP_ERR_FORMAT: return "ValueError";
    case AVP_ERR_PROC: return "OSError";
    default: return "Exception";
    }
}

bool shsm_exc_is_key(int exc) { return exc == SHSM_EXC_KEY || exc == SHSM_EXC_UNIT_NOT_BUILT; }
bool shsm_exc_is_os(int exc) {
    return exc == SHSM_EXC_OS || exc == SHSM_EXC_FILE_NOT_FOUND || exc == AVP_ERR_IO || exc == AVP_ERR_PROC;
}
bool shsm_exc_is_value(int exc) { return exc == SHSM_EXC_VALUE || exc == AVP_ERR_FORMAT; }

AvpStr shsm_exc_str(AvpArena *a, int exc, const char *msg) {
    if (exc == SHSM_EXC_KEY) return avp_repr_str(a, avp_str(msg));
    return avp_str_dup(a, avp_str(msg));
}

AvpStr shsm_exc_main_text(AvpArena *a, int exc, const char *msg) {
    AvpStr m = shsm_exc_is_key(exc) ? avp_str(msg) : shsm_exc_str(a, exc, msg);
    if (m.n == 0) return avp_str(shsm_exc_class(exc));
    AvpStrVec lines = avp_str_splitlines(m, false);
    AvpStr first = lines.n ? lines.v[0] : AVP_S("");
    first = avp_str_dup(a, first);
    avp_vec_free(&lines);
    return first;
}

/* ---- parser ---- */

static bool limit_conv(void *ctx, AvpArena *a, AvpStr in, AvpArgValue *out, AvpStr *msg) {
    (void)ctx;
    i64 n = 0;
    if (!shsm_limit_type(a, in, &n, msg)) return false;
    out->kind = AVP_V_INT;
    out->i = n;
    return true;
}

bool shsm_limit_type(AvpArena *a, AvpStr text, i64 *n, AvpStr *msg) {
    AvpStr t = avp_str_lower(a, avp_str_strip(text));
    if (avp_str_eqz(t, "all") || avp_str_eqz(t, "none") || avp_str_eqz(t, "unlimited") || avp_str_eqz(t, "inf")) {
        *n = 0;
        return true;
    }
    i64 v = 0;
    bool overflow = false;
    if (!avp_parse_int(t, 10, &v, &overflow)) {
        if (!overflow) {
            AvpStr r = avp_repr_str(a, text);
            *msg = avp_str(avp_sprintf(a, "expected a line count or 'all', got %.*s", AVP_SF(r)));
            return false;
        }
        /* Python's int is unbounded: a huge count is simply no practical cap */
        v = avp_str_startswith(avp_str_lstrip(t), AVP_S("-")) ? -1 : INT64_MAX;
    }
    if (v < 0) {
        *msg = AVP_S("a line count cannot be negative");
        return false;
    }
    *n = v;
    return true;
}

AvpArgParser *shsm_cli_parser(const char *prog, const char *description, bool root, bool json_switch, bool limit) {
    AvpArgParser *p = avp_args_parser(NULL, &(AvpArgParserDesc){.prog = prog, .description = description});
    if (root)
        avp_arg(p, &(AvpArgDesc){.names = "--root", .metavar = "DIR",
                                 .help = "checkout to work on (default: the one around the cwd)"});
    if (json_switch)
        avp_arg(p, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE,
                                 .help = "print one JSON document instead of text"});
    if (limit) {
        avp_arg(p, &(AvpArgDesc){.names = "--all", .action = AVP_ARG_STORE_TRUE, .help = "do not cap the output"});
        avp_arg(p, &(AvpArgDesc){.names = "--limit", .type = AVP_T_CUSTOM, .conv = limit_conv,
                                 .type_name = "limit_type", .dflt = AVP_DEF_INT(SHSM_DEFAULT_LIMIT), .metavar = "N",
                                 .help = "cap text output at N lines (default 40; 0 or 'all': no cap)"});
    }
    avp_arg(p, &(AvpArgDesc){.names = "--debug", .action = AVP_ARG_STORE_TRUE, .help = "print tracebacks"});
    return p;
}

/* ---- output ---- */

static ShsmOutput *g_open_outputs;

ShsmOutput *shsm_output_open(const AvpArgs *args, i64 limit, int show_all, int as_json) {
    ShsmOutput *o = AVP_NEW(avp_arena(), ShsmOutput);
    o->as_json = as_json >= 0 ? as_json != 0 : (args && avp_args_has(args, "json") && avp_args_bool(args, "json"));
    o->show_all = show_all >= 0 ? show_all != 0 : (args && avp_args_has(args, "all") && avp_args_bool(args, "all"));
    if (limit >= 0) o->limit = limit;
    else o->limit = (args && avp_args_has(args, "limit")) ? avp_args_int(args, "limit") : SHSM_DEFAULT_LIMIT;
    o->payload = avp_json_obj(avp_arena());
    o->prev_open = g_open_outputs;
    g_open_outputs = o;
    return o;
}

void shsm_output_line(ShsmOutput *o, AvpStr text) {
    if (o->as_json) return;
    AvpStrVec parts = avp_str_split(text, AVP_S("\n"), -1);
    for (size_t i = 0; i < parts.n; i++) {
        if (o->show_all || o->limit == 0 || o->count < o->limit) {
            fwrite(parts.v[i].p, 1, parts.v[i].n, stdout);
            fputc('\n', stdout);
            o->count++;
        } else {
            o->dropped++;
        }
    }
    avp_vec_free(&parts);
}

void shsm_output_linez(ShsmOutput *o, const char *text) { shsm_output_line(o, avp_str(text)); }

void shsm_output_lines(ShsmOutput *o, const AvpStr *lines, size_t n) {
    for (size_t i = 0; i < n; i++) shsm_output_line(o, lines[i]);
}

void shsm_output_data(ShsmOutput *o, const char *key, AvpJson *val) {
    avp_json_set(avp_arena(), o->payload, avp_str(key), val);
}

AvpJsonDumpOpts shsm_json_opts(void) { return AVP_JSON_INDENT(1); }

void shsm_output_close(ShsmOutput *o) {
    if (o->closed) return;
    o->closed = true;
    /* unlink from the open list */
    for (ShsmOutput **pp = &g_open_outputs; *pp; pp = &(*pp)->prev_open) {
        if (*pp == o) {
            *pp = o->prev_open;
            break;
        }
    }
    if (o->as_json) {
        AvpJsonDumpOpts opts = shsm_json_opts();
        AvpBuf b = {0};
        avp_json_dump_buf(&b, o->payload, &opts);
        fwrite(b.p, 1, b.n, stdout);
        fputc('\n', stdout);
        avp_buf_free(&b);
    } else if (o->dropped) {
        printf("... %lld more lines, use --all\n", (long long)o->dropped);
    }
}

AvpStrVec shsm_cap_lines(AvpArena *a, const AvpStr *lines, size_t n, i64 limit, bool show_all, const char *hint) {
    AvpStrVec out = {0};
    if (show_all || limit <= 0 || (i64)n <= limit) {
        avp_vec_extend(&out, lines, n);
        return out;
    }
    avp_vec_extend(&out, lines, (size_t)limit);
    avp_vec_push(&out, avp_str(avp_sprintf(a, "... %lld more lines, %s", (long long)((i64)n - limit), hint)));
    return out;
}

/* ---- main ---- */

int shsm_cli_main(AvpArgParser *ap, int argc, char **argv, ShsmMainFn fn, void *ctx) {
    AvpArgs *args = avp_args_parse(ap, argc, argv);
    const char *prog = avp_args_prog(ap);
    AvpTry t;
    if (AVP_TRY(&t)) {
        int rc = fn(ctx, args);
        avp_try_end(&t);
        return rc;
    }
    while (g_open_outputs) shsm_output_close(g_open_outputs);
    fflush(stdout);
    if (t.code == SHSM_EXC_FINDING) {
        fprintf(stderr, "%s: %s\n", prog, t.msg);
        return SHSM_EXIT_FINDING;
    }
    AvpStr text = shsm_exc_main_text(avp_arena(), t.code, t.msg);
    fprintf(stderr, "%s: error: %.*s\n", prog, AVP_SF(text));
    return SHSM_EXIT_ERROR;
}

/* ---- subprocesses ---- */

ShsmRun shsm_run(AvpArena *a, const char *const *argv, size_t argc, const char *cwd, double timeout) {
    const char **av = AVP_NEWN(a, const char *, argc + 1);
    for (size_t i = 0; i < argc; i++) av[i] = argv[i];
    AvpProcSpec spec = {0};
    spec.argv = av;
    spec.cwd = cwd;
    spec.out = AVP_IO_CAPTURE;
    spec.err = AVP_IO_CAPTURE;
    spec.text = true;
    spec.errors = AVP_ERRORS_REPLACE;
    spec.timeout = timeout > 0 ? timeout : 0;
    spec.arena = a;
    double t0 = avp_perf_counter();
    AvpProcResult r = avp_run(&spec);
    ShsmRun out = {0};
    out.seconds = avp_perf_counter() - t0;
    if (r.status == AVP_PROC_START_FAILED) {
        const char *m = r.message ? r.message : "cannot start";
        if (r.winerror == 2 || r.winerror == 3) shsm_raise(SHSM_EXC_USAGE, "cannot run %s: %s", argc ? argv[0] : "", m);
        shsm_raise(SHSM_EXC_OS, "%s", m);
    }
    out.out = r.out.p ? r.out : AVP_S("");
    out.err = r.err.p ? r.err : AVP_S("");
    if (r.status == AVP_PROC_TIMEOUT) {
        out.timed_out = true;
        out.returncode = -1;
    } else {
        out.returncode = r.returncode;
    }
    return out;
}

AvpStr shsm_run_output(AvpArena *a, const ShsmRun *r) { return avp_str_concat(a, r->out, r->err); }

void shsm_run_check(AvpArena *a, const ShsmRun *r, const char *argv0) {
    if (shsm_run_ok(r)) return;
    AvpStr all = avp_str_strip(shsm_run_output(a, r));
    AvpStrVec lines = avp_str_splitlines(all, false);
    AvpStr first = lines.n ? lines.v[0] : AVP_S("");
    AvpStr base = avp_path_basename(argv0);
    char why[64];
    if (r->timed_out) snprintf(why, sizeof why, "timed out");
    else snprintf(why, sizeof why, "exit %lld", (long long)r->returncode);
    char *msg = avp_sprintf(a, "%.*s failed (%s): %.*s", AVP_SF(base), why, AVP_SF(first));
    avp_vec_free(&lines);
    shsm_raise(SHSM_EXC_USAGE, "%s", msg);
}

int shsm_default_jobs(void) {
    int n = avp_cpu_count();
    return n > 1 ? n : 1;
}
