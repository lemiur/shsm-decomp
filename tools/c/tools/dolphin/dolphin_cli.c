/* dolphin.py's command line: build_parser, run, run_broker. */
#include "dolphin_priv.h"

#include <windows.h>
#include <bcrypt.h>

static const char *const VIEWS_MEM[] = {"u8", "u16", "u32", "float", "str", "ptr", NULL};
static const char *const VIEWS_WRITE[] = {"u8", "u16", "u32", "float", "bytes", NULL};
static const char *const WATCH[] = {"r", "w", "a", NULL};

static bool limit_conv(void *ctx, AvpArena *a, AvpStr in, AvpArgValue *out, AvpStr *msg) {
    (void)ctx;
    i64 n = 0;
    if (!shsm_limit_type(a, in, &n, msg)) return false;
    out->kind = AVP_V_INT;
    out->i = n;
    return true;
}

/* cli.parser("") as a parent: -h (the subparser's own), --root, --json, --all, --limit, --debug */
static AvpArgParser *add(AvpArgSubparsers *sub, const char *name) {
    const char *doc = dolphin_op(name)->doc;
    AvpArgParser *p = avp_args_subparser(sub, name, &(AvpArgParserDesc){.help = doc, .description = doc});
    avp_arg(p, &(AvpArgDesc){.names = "--root", .metavar = "DIR",
                             .help = "checkout to work on (default: the one around the cwd)"});
    avp_arg(p, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE,
                             .help = "print one JSON document instead of text"});
    avp_arg(p, &(AvpArgDesc){.names = "--all", .action = AVP_ARG_STORE_TRUE, .help = "do not cap the output"});
    avp_arg(p, &(AvpArgDesc){.names = "--limit", .type = AVP_T_CUSTOM, .conv = limit_conv, .type_name = "limit_type",
                             .dflt = AVP_DEF_INT(SHSM_DEFAULT_LIMIT), .metavar = "N",
                             .help = "cap text output at N lines (default 40; 0 or 'all': no cap)"});
    avp_arg(p, &(AvpArgDesc){.names = "--debug", .action = AVP_ARG_STORE_TRUE, .help = "print tracebacks"});
    return p;
}

static AvpArgParser *build_parser(void) {
    AvpArgParser *ap = shsm_cli_parser(DOLPHIN_PROG,
                                       "Observe the running game in Dolphin through its GDB remote stub: memory, "
                                       "registers, breakpoints, call logs.",
                                       false, false, false);
    AvpBuf mv;
    avp_buf_init(&mv, avp_arena());
    avp_buf_putc(&mv, '{');
    for (size_t i = 0; i < dolphin_nops; i++) avp_buf_printf(&mv, "%s%s", i ? "," : "", dolphin_ops[i].name);
    avp_buf_putc(&mv, '}');
    AvpArgSubparsers *sub =
        avp_args_subparsers(ap, &(AvpArgSubparsersDesc){.dest = "cmd", .required = true,
                                                        .metavar = avp_cz(avp_arena(), avp_buf_str(&mv))});
    AvpArgParser *p = add(sub, "launch");
    avp_arg(p, &(AvpArgDesc){.names = "--wait-boot", .type = AVP_T_FLOAT, .dflt = AVP_DEF_FLOAT(120.0),
                             .help = "seconds to wait for the stub (default 120)"});
    avp_arg(p, &(AvpArgDesc){.names = "--dolphin", .help = "path to Dolphin.exe (remembered in build/dolphin/dolphin.json)"});
    avp_arg(p, &(AvpArgDesc){.names = "--disc", .help = "disc image (default: the one in the repository root)"});
    add(sub, "quit");
    add(sub, "status");
    p = add(sub, "regs");
    avp_arg(p, &(AvpArgDesc){.names = "--fpr", .action = AVP_ARG_STORE_TRUE, .help = "also show f0..f31"});
    p = add(sub, "mem");
    avp_arg(p, &(AvpArgDesc){.names = "address"});
    avp_arg(p, &(AvpArgDesc){.names = "--len", .dest = "length", .type = AVP_T_INT0, .dflt = AVP_DEF_INT(64)});
    avp_arg(p, &(AvpArgDesc){.names = "--as", .dest = "view", .choices = VIEWS_MEM, .dflt = AVP_DEF_STR("u8")});
    avp_arg(p, &(AvpArgDesc){.names = "--compare-dol", .action = AVP_ARG_STORE_TRUE, .help = "compare with the retail DOL"});
    p = add(sub, "read");
    avp_arg(p, &(AvpArgDesc){.names = "symbol"});
    p = add(sub, "write");
    avp_arg(p, &(AvpArgDesc){.names = "address"});
    avp_arg(p, &(AvpArgDesc){.names = "value"});
    avp_arg(p, &(AvpArgDesc){.names = "--as", .dest = "view", .choices = VIEWS_WRITE, .dflt = AVP_DEF_STR("u32")});
    avp_arg(p, &(AvpArgDesc){.names = "--confirm", .action = AVP_ARG_STORE_TRUE,
                             .help = "required: the write changes the running game"});
    p = add(sub, "undo");
    avp_arg(p, &(AvpArgDesc){.names = "--confirm", .action = AVP_ARG_STORE_TRUE, .help = "required"});
    avp_arg(p, &(AvpArgDesc){.names = "--force", .action = AVP_ARG_STORE_TRUE});
    p = add(sub, "break");
    avp_arg(p, &(AvpArgDesc){.names = "functions", .nargs = AVP_NARGS_PLUS});
    avp_arg(p, &(AvpArgDesc){.names = "--watch", .choices = WATCH});
    avp_arg(p, &(AvpArgDesc){.names = "--len", .dest = "length", .type = AVP_T_INT0, .dflt = AVP_DEF_INT(4)});
    add(sub, "clear");
    p = add(sub, "cont");
    avp_arg(p, &(AvpArgDesc){.names = "--timeout", .type = AVP_T_FLOAT, .dflt = AVP_DEF_FLOAT(10.0)});
    add(sub, "pause");
    p = add(sub, "step");
    avp_arg(p, &(AvpArgDesc){.names = "count", .nargs = AVP_NARGS_OPT, .type = AVP_T_INT, .dflt = AVP_DEF_INT(1)});
    add(sub, "where");
    p = add(sub, "bt");
    avp_arg(p, &(AvpArgDesc){.names = "--depth", .type = AVP_T_INT, .dflt = AVP_DEF_INT(24)});
    p = add(sub, "calls");
    avp_arg(p, &(AvpArgDesc){.names = "function"});
    avp_arg(p, &(AvpArgDesc){.names = "--count", .type = AVP_T_INT, .dflt = AVP_DEF_INT(5)});
    avp_arg(p, &(AvpArgDesc){.names = "--timeout", .type = AVP_T_FLOAT, .dflt = AVP_DEF_FLOAT(30.0)});
    avp_arg(p, &(AvpArgDesc){.names = "--args", .help = "'this:obj, value:float' (types: ptr, int, u32, s32, hex, bool, "
                                                        "u8, u16, s16, char, float, double, str, i64, obj)"});
    p = add(sub, "object");
    avp_arg(p, &(AvpArgDesc){.names = "address"});
    avp_arg(p, &(AvpArgDesc){.names = "--class", .dest = "cls"});
    avp_arg(p, &(AvpArgDesc){.names = "--size", .type = AVP_T_INT0, .dflt = AVP_DEF_NONE});
    p = add(sub, "trace");
    avp_arg(p, &(AvpArgDesc){.names = "functions", .nargs = AVP_NARGS_PLUS});
    avp_arg(p, &(AvpArgDesc){.names = "--seconds", .type = AVP_T_FLOAT, .dflt = AVP_DEF_FLOAT(5.0)});
    p = avp_args_subparser(sub, "_broker",
                           &(AvpArgParserDesc){.description = "(internal) hold the stub's one connection for the session"});
    avp_arg(p, &(AvpArgDesc){.names = "--root"});
    avp_arg(p, &(AvpArgDesc){.names = "--gdb-port", .type = AVP_T_INT, .required = true});
    avp_arg(p, &(AvpArgDesc){.names = "--wait", .type = AVP_T_FLOAT, .dflt = AVP_DEF_FLOAT(120.0)});
    avp_arg(p, &(AvpArgDesc){.names = "--debug", .action = AVP_ARG_STORE_TRUE});
    return ap;
}

/* op_kwargs: the parsed options as the op's JSON arguments (None values left out: the op default is None too) */
static AvpJson *kwargs(AvpArena *a, const AvpArgs *args, const char *cmd) {
    AvpJson *kw = avp_json_obj(a);
    const DolphinOp *op = dolphin_op(cmd);
    for (const DolphinParam *p = op->params; p->name; p++) {
        const AvpArgValue *v = avp_args_value(args, p->name);
        if (!v || v->kind == AVP_V_NONE) continue;
        AvpJson *j = NULL;
        switch (v->kind) {
        case AVP_V_STR: j = avp_json_str(a, v->s); break;
        case AVP_V_INT: j = avp_json_int(a, v->i); break;
        case AVP_V_FLOAT: j = avp_json_float(a, v->f); break;
        case AVP_V_BOOL: j = avp_json_bool(v->i != 0); break;
        case AVP_V_LIST:
            j = avp_json_arr(a);
            for (size_t i = 0; i < v->n; i++) avp_json_arr_push(a, j, avp_json_str(a, v->items[i].s));
            break;
        default: break;
        }
        if (j) avp_json_set(a, kw, avp_str(p->name), j);
    }
    if (!strcmp(cmd, "write") || !strcmp(cmd, "undo"))
        DJ_SET(a, kw, "confirm", avp_json_bool(avp_args_bool(args, "confirm")));
    return kw;
}

/* ---- _broker ---- */

typedef struct {
    i64 pid;
    AvpJson *created;
} BrokerCtx;

static void broker_log(void *ctx, const char *msg) {
    (void)ctx;
    AvpStr now;
    avp_strftime_now(avp_scratch(), "%H:%M:%S", &now);
    fprintf(stderr, "%.*s %s\n", AVP_SF(now), msg);
    fflush(stderr);
}

static bool broker_alive(void *ctx) {
    BrokerCtx *c = ctx;
    i64 created = 0;
    if (!dolphin_proc_info(avp_scratch(), c->pid, NULL, &created)) return false;
    return avp_json_is_int(c->created) && avp_json_i64(c->created, 0) == created;
}

int dolphin_run_broker(const char *root, int gdb_port, double wait) {
    DolphinEnv *env = dolphin_env_new(root);
    AvpArena *a = env->a;
    const char *ready = avp_join(a, env->dir, "broker.json");
    AvpJson *s = dolphin_session(env);
    if (!s) s = avp_json_obj(a);
    BrokerCtx ctx = {avp_json_get_i64(s, "dolphin_pid", 0), avp_json_getz(s, "dolphin_created")};
    ShsmGdbClient *gdb = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        gdb = shsm_gdb_connect("127.0.0.1", gdb_port, SHSM_GDB_DEFAULT_TIMEOUT, wait);
        shsm_gdb_halt_reason(a, gdb);
        avp_try_end(&t);
    } else {
        if (!shsm_exc_is_gdb(t.code)) avp_throw(t.code, "%s", t.msg);
        AvpJson *r = avp_json_obj(a);
        DJ_SET(a, r, "ready", avp_json_bool(false));
        DJ_SET(a, r, "error", DJ_STR(a, t.msg));
        dolphin_write_json_atomic(ready, r);
        broker_log(NULL, t.msg);
        return 2;
    }
    AvpJson *tok = avp_json_getz(s, "token");
    const char *token;
    if (avp_json_truthy(tok)) token = avp_cz(a, avp_json_str_or(tok, AVP_S("")));
    else {
        u8 b[16];
        BCryptGenRandom(NULL, b, sizeof b, 2 /* BCRYPT_USE_SYSTEM_PREFERRED_RNG */);
        token = dolphin_hex(a, b, sizeof b);
    }
    ShsmBroker *broker = shsm_broker_new(gdb, token, NULL, 0, broker_alive, broker_log, &ctx);
    AvpJson *r = avp_json_obj(a);
    DJ_SET(a, r, "ready", avp_json_bool(true));
    DJ_SET(a, r, "port", DJ_INT(a, shsm_broker_port(broker)));
    DJ_SET(a, r, "token", DJ_STR(a, token));
    DJ_SET(a, r, "pid", DJ_INT(a, GetCurrentProcessId()));
    dolphin_write_json_atomic(ready, r);
    broker_log(NULL, avp_sprintf(a, "connected to the stub on %d; serving on %d", gdb_port, shsm_broker_port(broker)));
    shsm_broker_serve(broker);
    broker_log(NULL, "broker exits");
    shsm_broker_free(broker);
    shsm_gdb_client_free(gdb);
    return 0;
}

/* ---- run ---- */

static int run(void *ctx, AvpArgs *args) {
    (void)ctx;
    AvpArena *a = avp_arena();
    const char *cmd = avp_cz(a, avp_args_str(args, "cmd"));
    AvpStr root = avp_args_str(args, "root");
    const char *rootz = root.p ? avp_cz(a, root) : NULL;
    if (!strcmp(cmd, "_broker")) return dolphin_run_broker(rootz, (int)avp_args_int(args, "gdb_port"), avp_args_float(args, "wait"));
    DolphinEnv *env = dolphin_env_new(rootz);
    DolphinResult *res = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        res = dolphin_call(env, cmd, kwargs(a, args, cmd));
        avp_try_end(&t);
    }
    dolphin_env_close(env);
    if (t.code) avp_throw(t.code, "%s", t.msg);
    ShsmOutput *out = shsm_output_open(args, -1, -1, -1);
    shsm_output_lines(out, res->lines.v, res->lines.n);
    for (size_t i = 0; i < avp_json_len(res->data); i++) {
        const AvpJsonMember *m = avp_json_member(res->data, i);
        shsm_output_data(out, avp_cz(a, m->key), m->val);
    }
    shsm_output_close(out);
    return res->code;
}

int dolphin_main(int argc, char **argv) { return shsm_cli_main(build_parser(), argc, argv, run, NULL); }
