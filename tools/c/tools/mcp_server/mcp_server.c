/* tools/mcp/server.py in C: tool table, argument coercion, call_tool, handle, serve. See mcp_server.h. */
#include "mcp_server.h"

#include "avp/avp.h"
#include "shsm_cli.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static const char *const PROTOCOLS[] = {"2024-11-05", "2025-03-26", "2025-06-18"};

/* ---- Python's str() and repr() of JSON values ---- */

static AvpStr py_repr(AvpArena *a, const AvpJson *v) {
    switch (v->kind) {
    case AVP_JSON_NULL: return AVP_S("None");
    case AVP_JSON_BOOL: return avp_repr_bool(v->b);
    case AVP_JSON_INT: return v->big ? v->u.str : avp_repr_i64(a, v->u.i);
    case AVP_JSON_FLOAT: return avp_repr_f64(a, v->u.f);
    case AVP_JSON_STRING: return avp_repr_str(a, v->u.str);
    case AVP_JSON_ARRAY: {
        AvpStr *items = AVP_NEWN(a, AvpStr, v->u.arr.n + 1);
        for (size_t i = 0; i < v->u.arr.n; i++) items[i] = py_repr(a, v->u.arr.v[i]);
        return avp_repr_list(a, items, v->u.arr.n);
    }
    default: {
        size_t n = v->u.obj.n;
        AvpStr *keys = AVP_NEWN(a, AvpStr, n + 1), *vals = AVP_NEWN(a, AvpStr, n + 1);
        for (size_t i = 0; i < n; i++) {
            keys[i] = avp_repr_str(a, v->u.obj.v[i].key);
            vals[i] = py_repr(a, v->u.obj.v[i].val);
        }
        return avp_repr_dict(a, keys, vals, n);
    }
    }
}

AvpStr mcp_py_str(AvpArena *a, const AvpJson *v) {
    return v->kind == AVP_JSON_STRING ? v->u.str : py_repr(a, v);
}

/* type(value).__name__ */
static const char *py_type(const AvpJson *v) {
    static const char *const names[] = {"NoneType", "bool", "int", "float", "str", "list", "dict"};
    return names[v->kind];
}

const char *mcp_exc_class(int exc) {
    switch (exc) {
    case SHSM_EXC_GDB: return "GdbError";
    case SHSM_EXC_GDB_TIMEOUT: return "GdbTimeout";
    case SHSM_EXC_GDB_DISCONNECTED: return "GdbDisconnected";
    case MCP_EXC_ATTRIBUTE: return "AttributeError";
    case MCP_EXC_OVERFLOW: return "OverflowError";
    case MCP_EXC_UNICODE_DECODE: return "UnicodeDecodeError";
    default: return shsm_exc_class(exc);
    }
}

/* except cli.UsageError: GdbError and its subclasses are UsageErrors */
static bool is_usage(int exc) {
    return exc == SHSM_EXC_USAGE || exc == SHSM_EXC_GDB || exc == SHSM_EXC_GDB_TIMEOUT || exc == SHSM_EXC_GDB_DISCONNECTED;
}

/* ---- the tool table ---- */

static AvpJson *str_json(AvpArena *a, const char *cz) { return avp_json_strz(a, cz); }

static AvpJson *typed(AvpArena *a, const char *type, const char *description) {
    AvpJson *s = avp_json_obj(a);
    avp_json_set(a, s, AVP_S("type"), str_json(a, type));
    avp_json_set(a, s, AVP_S("description"), str_json(a, description));
    return s;
}

AvpJson *mcp_input_schema(AvpArena *a, const DolphinOp *op) {
    AvpJson *props = avp_json_obj(a), *required = avp_json_arr(a);
    for (const DolphinParam *p = op->params; p->name; p++) {
        AvpJson *s = typed(a, p->type, p->description);
        if (!strcmp(p->type, "array")) {
            AvpJson *items = avp_json_obj(a);
            avp_json_set(a, items, AVP_S("type"), str_json(a, "string"));
            avp_json_set(a, s, AVP_S("items"), items);
        }
        avp_json_set(a, props, avp_str(p->name), s);
        if (p->required) avp_json_arr_push(a, required, str_json(a, p->name));
    }
    if (op->destructive) {
        avp_json_set(a, props, AVP_S("confirm"),
                     typed(a, "boolean", "must be true: this changes the running game or ends the session"));
        avp_json_arr_push(a, required, str_json(a, "confirm"));
    }
    avp_json_set(a, props, AVP_S("json"), typed(a, "boolean", "return the JSON document instead of text"));
    AvpJson *schema = avp_json_obj(a);
    avp_json_set(a, schema, AVP_S("type"), str_json(a, "object"));
    avp_json_set(a, schema, AVP_S("properties"), props);
    avp_json_set(a, schema, AVP_S("additionalProperties"), avp_json_bool(false));
    if (avp_json_len(required)) avp_json_set(a, schema, AVP_S("required"), required);
    return schema;
}

static const char *tool_description(AvpArena *a, const DolphinOp *op) {
    return op->destructive ? avp_sprintf(a, "%s (destructive: requires confirm: true)", op->doc) : op->doc;
}

AvpJson *mcp_list_tools(AvpArena *a) {
    AvpJson *out = avp_json_arr(a);
    for (size_t i = 0; i < dolphin_nops; i++) {
        const DolphinOp *op = &dolphin_ops[i];
        AvpJson *t = avp_json_obj(a);
        avp_json_set(a, t, AVP_S("name"), avp_json_str(a, avp_str(avp_sprintf(a, "dolphin_%s", op->name))));
        avp_json_set(a, t, AVP_S("description"), str_json(a, tool_description(a, op)));
        avp_json_set(a, t, AVP_S("inputSchema"), mcp_input_schema(a, op));
        avp_json_arr_push(a, out, t);
    }
    return out;
}

/* ---- tools/call ---- */

static AvpJson *coerce(AvpArena *a, const AvpJson *v, const char *type, AvpStr name) {
    if (!strcmp(type, "integer")) {
        if (v->kind == AVP_JSON_INT) return (AvpJson *)v;
        if (v->kind != AVP_JSON_FLOAT) shsm_raise(SHSM_EXC_USAGE, "argument %.*s must be an integer", AVP_SF(name));
        double f = v->u.f;
        if (isnan(f)) shsm_raise(SHSM_EXC_VALUE, "cannot convert float NaN to integer");
        if (isinf(f)) avp_throw(MCP_EXC_OVERFLOW, "cannot convert float infinity to integer");
        if (trunc(f) != f) shsm_raise(SHSM_EXC_USAGE, "argument %.*s must be an integer", AVP_SF(name));
        if (fabs(f) < 9.2e18) return avp_json_int(a, (i64)f);
        return avp_json_int_digits(a, avp_str(avp_sprintf(a, "%.0f", f))); /* int(1e19): exact digits */
    }
    if (!strcmp(type, "number")) {
        if (v->kind == AVP_JSON_FLOAT) return (AvpJson *)v;
        if (v->kind != AVP_JSON_INT) shsm_raise(SHSM_EXC_USAGE, "argument %.*s must be a number", AVP_SF(name));
        if (!v->big) return avp_json_float(a, (double)v->u.i);
        double f = strtod(avp_cz(a, v->u.str), NULL);
        if (isinf(f)) avp_throw(MCP_EXC_OVERFLOW, "int too large to convert to float");
        return avp_json_float(a, f);
    }
    if (!strcmp(type, "boolean")) {
        if (v->kind != AVP_JSON_BOOL) shsm_raise(SHSM_EXC_USAGE, "argument %.*s must be true or false", AVP_SF(name));
        return (AvpJson *)v;
    }
    if (!strcmp(type, "array")) {
        bool ok = v->kind == AVP_JSON_ARRAY;
        for (size_t i = 0; ok && i < v->u.arr.n; i++) ok = v->u.arr.v[i]->kind == AVP_JSON_STRING;
        if (!ok) shsm_raise(SHSM_EXC_USAGE, "argument %.*s must be an array of strings", AVP_SF(name));
        return (AvpJson *)v;
    }
    if (v->kind != AVP_JSON_STRING) shsm_raise(SHSM_EXC_USAGE, "argument %.*s must be a string", AVP_SF(name));
    return (AvpJson *)v;
}

AvpJson *mcp_text_result(AvpArena *a, AvpStr text, bool error) {
    AvpJson *item = avp_json_obj(a);
    avp_json_set(a, item, AVP_S("type"), str_json(a, "text"));
    avp_json_set(a, item, AVP_S("text"), avp_json_str(a, text));
    AvpJson *content = avp_json_arr(a);
    avp_json_arr_push(a, content, item);
    AvpJson *r = avp_json_obj(a);
    avp_json_set(a, r, AVP_S("content"), content);
    avp_json_set(a, r, AVP_S("isError"), avp_json_bool(error));
    return r;
}

/* a + b + c without C-string formatting (a JSON string may hold U+0000) */
static AvpStr cat3(AvpArena *a, AvpStr x, AvpStr y, AvpStr z) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_put(&b, x);
    avp_buf_put(&b, y);
    avp_buf_put(&b, z);
    return avp_buf_str(&b);
}

static const DolphinParam *find_param(const DolphinOp *op, AvpStr k) {
    for (const DolphinParam *p = op->params; p->name; p++)
        if (avp_str_eqz(k, p->name)) return p;
    return NULL;
}

/* an unexpected internal error: one line on stderr */
static void print_exc(int exc, const char *msg) {
    fprintf(stderr, "mcp_server.exe: error: %s: %s\n", mcp_exc_class(exc), msg);
    fflush(stderr);
}

AvpJson *mcp_call_tool(McpServer *s, AvpArena *a, AvpStr name, const AvpJson *arguments) {
    AvpStr prefix = AVP_S("dolphin_");
    if (!avp_str_startswith(name, prefix) || memchr(name.p, 0, name.n)) return NULL;
    const DolphinOp *op = dolphin_op(avp_cz(a, avp_strn(name.p + prefix.n, name.n - prefix.n)));
    if (!op) return NULL;
    const AvpJson *args = avp_json_is_obj(arguments) ? arguments : NULL;
    AvpJson *json_arg = avp_json_getz(args, "json"), *confirm = avp_json_getz(args, "confirm");
    bool as_json = json_arg && json_arg->kind == AVP_JSON_BOOL && json_arg->b;
    const char *namez = avp_cz(a, name);
    DolphinEnv *volatile env = NULL;
    DolphinResult *res = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        if (op->destructive && !(confirm && confirm->kind == AVP_JSON_BOOL && confirm->b))
            shsm_raise(SHSM_EXC_USAGE, "refused: %s is destructive; call it again with \"confirm\": true", namez);
        AvpJson *kwargs = avp_json_obj(a);
        for (size_t i = 0, n = args ? avp_json_len(args) : 0; i < n; i++) {
            const AvpJsonMember *m = avp_json_member(args, i);
            if (avp_str_eqz(m->key, "json") || avp_str_eqz(m->key, "confirm")) continue;
            const DolphinParam *p = find_param(op, m->key);
            if (!p) shsm_raise(SHSM_EXC_USAGE, "unknown argument %s for %s", avp_repr_str(a, m->key).p, namez);
            avp_json_set(a, kwargs, m->key, coerce(a, m->val, p->type, m->key));
        }
        for (const DolphinParam *p = op->params; p->name; p++)
            if (p->required && !avp_json_getz(kwargs, p->name))
                shsm_raise(SHSM_EXC_USAGE, "missing argument %s for %s", avp_repr_str(a, avp_str(p->name)).p, namez);
        if (!strcmp(op->name, "write") || !strcmp(op->name, "undo")) /* the ops with a confirm parameter */
            avp_json_set(a, kwargs, AVP_S("confirm"), avp_json_bool(true));
        env = s->env_factory ? s->env_factory(s->env_ctx, s->root) : dolphin_env_new(s->root);
        res = dolphin_call(env, op->name, kwargs);
        DolphinEnv *e = env;
        env = NULL;
        dolphin_env_close(e);
        avp_try_end(&t);
    } else {
        if (env) {
            DolphinEnv *e = env;
            env = NULL;
            AvpTry t2;
            if (AVP_TRY(&t2)) {
                dolphin_env_close(e);
                avp_try_end(&t2);
            }
        }
        if (is_usage(t.code)) {
            AvpStr m = avp_str(t.msg);
            if (!m.n) return mcp_text_result(a, avp_str(mcp_exc_class(t.code)), true);
            AvpStrVec lines = avp_str_splitlines(m, false);
            AvpStr first = avp_str_dup(a, lines.n ? lines.v[0] : AVP_S(""));
            avp_vec_free(&lines);
            return mcp_text_result(a, cat3(a, AVP_S("error: "), first, AVP_S("")), true);
        }
        print_exc(t.code, t.msg);
        AvpStr str = shsm_exc_str(a, t.code, t.msg);
        return mcp_text_result(a, cat3(a, AVP_S("error: "), avp_str(avp_sprintf(a, "%s: ", mcp_exc_class(t.code))), str),
                               true);
    }
    if (as_json) return mcp_text_result(a, avp_json_dumps(a, res->data, NULL), false);
    AvpStr text = dolphin_result_text(a, res, MCP_TEXT_LIMIT);
    if (!text.n) text = AVP_S("(no output)");
    if (res->code == SHSM_EXIT_FINDING) text = cat3(a, text, AVP_S("\n(finding: exit status 1)"), AVP_S(""));
    return mcp_text_result(a, text, false);
}

/* ---- handle ---- */

static AvpJson *response(AvpArena *a, const AvpJson *id, const char *key, AvpJson *value) {
    AvpJson *r = avp_json_obj(a);
    avp_json_set(a, r, AVP_S("jsonrpc"), str_json(a, "2.0"));
    avp_json_set(a, r, AVP_S("id"), id ? (AvpJson *)id : avp_json_null());
    avp_json_set(a, r, avp_str(key), value);
    return r;
}

static AvpJson *result_response(AvpArena *a, const AvpJson *id, AvpJson *result) {
    return response(a, id, "result", result);
}

static AvpJson *error_response(AvpArena *a, const AvpJson *id, int code, AvpStr message) {
    AvpJson *e = avp_json_obj(a);
    avp_json_set(a, e, AVP_S("code"), avp_json_int(a, code));
    avp_json_set(a, e, AVP_S("message"), avp_json_str(a, message));
    return response(a, id, "error", e);
}

/* params.get(key) on `msg.get("params") or {}`: AttributeError when params is truthy and not a dict */
static AvpJson *param_get(AvpArena *a, const AvpJson *params, const char *key) {
    if (!avp_json_truthy(params)) return NULL;
    if (params->kind != AVP_JSON_OBJECT)
        avp_throw(MCP_EXC_ATTRIBUTE, "'%s' object has no attribute 'get'", py_type(params));
    (void)a;
    return avp_json_getz(params, key);
}

AvpJson *mcp_handle(McpServer *s, AvpArena *a, const AvpJson *msg) {
    AvpJson *jsonrpc = avp_json_getz(msg, "jsonrpc");
    if (!avp_json_is_obj(msg) || !(jsonrpc && jsonrpc->kind == AVP_JSON_STRING && avp_str_eqz(jsonrpc->u.str, "2.0")) ||
        !avp_json_has(msg, AVP_S("method")))
        return error_response(a, avp_json_getz(msg, "id"), MCP_INVALID_REQUEST, AVP_S("invalid request"));
    const AvpJson *method = avp_json_getz(msg, "method");
    bool is_str = method->kind == AVP_JSON_STRING;
    const AvpJson *params = avp_json_getz(msg, "params");
    if (!avp_json_has(msg, AVP_S("id"))) {
        if (is_str && avp_str_eqz(method->u.str, "notifications/initialized")) s->initialized = true;
        return NULL;
    }
    const AvpJson *id = avp_json_getz(msg, "id");
    AvpStr m = is_str ? method->u.str : AVP_S("");
    if (is_str && avp_str_eqz(m, "initialize")) {
        AvpJson *asked = param_get(a, params, "protocolVersion");
        const char *version = MCP_DEFAULT_PROTOCOL;
        for (size_t i = 0; asked && asked->kind == AVP_JSON_STRING && i < sizeof PROTOCOLS / sizeof *PROTOCOLS; i++)
            if (avp_str_eqz(asked->u.str, PROTOCOLS[i])) version = PROTOCOLS[i];
        AvpJson *tools = avp_json_obj(a), *caps = avp_json_obj(a), *info = avp_json_obj(a), *r = avp_json_obj(a);
        avp_json_set(a, tools, AVP_S("listChanged"), avp_json_bool(false));
        avp_json_set(a, caps, AVP_S("tools"), tools);
        avp_json_set(a, info, AVP_S("name"), str_json(a, MCP_SERVER_NAME));
        avp_json_set(a, info, AVP_S("version"), str_json(a, MCP_SERVER_VERSION));
        avp_json_set(a, r, AVP_S("protocolVersion"), str_json(a, version));
        avp_json_set(a, r, AVP_S("capabilities"), caps);
        avp_json_set(a, r, AVP_S("serverInfo"), info);
        return result_response(a, id, r);
    }
    if (is_str && avp_str_eqz(m, "ping")) return result_response(a, id, avp_json_obj(a));
    if (is_str && avp_str_eqz(m, "tools/list")) {
        AvpJson *r = avp_json_obj(a);
        avp_json_set(a, r, AVP_S("tools"), mcp_list_tools(a));
        return result_response(a, id, r);
    }
    if (is_str && avp_str_eqz(m, "tools/call")) {
        AvpJson *name = param_get(a, params, "name");
        if (!name || name->kind != AVP_JSON_STRING)
            return error_response(a, id, MCP_INVALID_PARAMS, AVP_S("tools/call needs a tool name"));
        AvpJson *arguments = param_get(a, params, "arguments");
        if (avp_json_truthy(arguments) && arguments->kind != AVP_JSON_OBJECT)
            return error_response(a, id, MCP_INVALID_PARAMS, AVP_S("arguments must be an object"));
        AvpJson *r = mcp_call_tool(s, a, name->u.str, avp_json_truthy(arguments) ? arguments : NULL);
        if (!r)
            return error_response(a, id, MCP_INVALID_PARAMS,
                                  cat3(a, AVP_S("Unknown tool: "), name->u.str, AVP_S("")));
        return result_response(a, id, r);
    }
    return error_response(a, id, MCP_METHOD_NOT_FOUND,
                          cat3(a, AVP_S("method not found: "), mcp_py_str(a, method), AVP_S("")));
}

/* ---- serve ---- */

typedef struct {
    McpServer *s;
    FILE *out;
    int code; /* -1 serving; else the exit code */
} Serve;

static void write_response(Serve *sv, AvpArena *a, const AvpJson *resp) {
    AvpStr text = avp_json_dumps(a, resp, &AVP_JSON_COMPACT);
    fwrite(text.p, 1, text.n, sv->out);
    fputc('\n', sv->out);
    fflush(sv->out);
}

/* an unexpected internal error: one line on stderr, exit status 1 */
static void crash(Serve *sv, int exc, const char *msg) {
    print_exc(exc, msg);
    sv->code = 1;
}

static void serve_line(Serve *sv, AvpStr raw) {
    AvpArena *a = avp_arena();
    AvpStr line = avp_str_strip(raw);
    if (!line.n) return;
    AvpJsonError err;
    AvpJson *msg = avp_json_parse(a, line, &err);
    if (!msg) {
        if (err.kind == AVP_JSON_E_RECURSION) { /* RecursionError is not a ValueError */
            crash(sv, SHSM_EXC_RECURSION, avp_json_error_str(a, &err).p);
            return;
        }
        write_response(sv, a, error_response(a, NULL, MCP_PARSE_ERROR, AVP_S("parse error")));
        return;
    }
    AvpTry t;
    if (msg->kind == AVP_JSON_ARRAY) {
        AvpJson *resps = avp_json_arr(a);
        if (AVP_TRY(&t)) {
            for (size_t i = 0; i < msg->u.arr.n; i++) {
                AvpJson *r = mcp_handle(sv->s, a, msg->u.arr.v[i]);
                if (r) avp_json_arr_push(a, resps, r);
            }
            avp_try_end(&t);
        } else {
            crash(sv, t.code, t.msg);
            return;
        }
        if (avp_json_len(resps)) write_response(sv, a, resps);
        return;
    }
    AvpJson *resp;
    if (AVP_TRY(&t)) {
        resp = mcp_handle(sv->s, a, msg);
        avp_try_end(&t);
    } else {
        print_exc(t.code, t.msg);
        resp = error_response(a, avp_json_getz(msg, "id"), MCP_INTERNAL_ERROR, shsm_exc_str(a, t.code, t.msg));
    }
    if (resp) write_response(sv, a, resp);
}

/* Length of the valid UTF-8 prefix of p[0..n) (strict, as Python's decoder); *bad set when the rest is not a
 * truncated sequence (an error now rather than at the next chunk). */
static size_t utf8_prefix(const u8 *p, size_t n, bool *bad, size_t *bad_at) {
    size_t i = 0;
    *bad = false;
    while (i < n) {
        u8 c = p[i];
        size_t len;
        u8 lo = 0x80, hi = 0xBF;
        if (c < 0x80) { i++; continue; }
        if (c >= 0xC2 && c <= 0xDF) len = 2;
        else if (c >= 0xE0 && c <= 0xEF) { len = 3; if (c == 0xE0) lo = 0xA0; if (c == 0xED) hi = 0x9F; }
        else if (c >= 0xF0 && c <= 0xF4) { len = 4; if (c == 0xF0) lo = 0x90; if (c == 0xF4) hi = 0x8F; }
        else { *bad = true; *bad_at = i; return i; }
        for (size_t k = 1; k < len; k++) {
            if (i + k >= n) return i; /* truncated: wait for more */
            u8 d = p[i + k];
            if (k == 1 ? (d < lo || d > hi) : (d < 0x80 || d > 0xBF)) { *bad = true; *bad_at = i; return i; }
        }
        i += len;
    }
    return i;
}

int mcp_serve(McpServer *s, void *in_handle, FILE *out) {
    Serve sv = {s, out, -1};
    AvpArena *arena = avp_arena();
    /* A long-lived server: what one message allocates from the thread's arena (the dolphin Env, symbols, results)
     * goes when its answer is written. The checkout's ShsmRepo is cached in that arena on first use, so it is found
     * once here, before the first mark. */
    AvpTry t;
    if (AVP_TRY(&t)) {
        shsm_repo_find(s->root);
        avp_try_end(&t);
    }
    AvpBuf line = {0};
    bool pending_cr = false;
    static u8 chunk[8192 + 4]; /* TextIOWrapper's chunk size */
    size_t carry = 0;          /* bytes of a truncated UTF-8 sequence kept for the next chunk */
    for (;;) {
        DWORD got = 0;
        bool eof = !ReadFile((HANDLE)in_handle, chunk + carry, 8192, &got, NULL) || got == 0;
        size_t n = carry + got;
        bool bad = false;
        size_t bad_at = 0;
        size_t ok = utf8_prefix(chunk, n, &bad, &bad_at);
        if (bad || (eof && ok < n)) {
            const char *why = bad ? "invalid start byte" : "unexpected end of data";
            crash(&sv, MCP_EXC_UNICODE_DECODE,
                  avp_sprintf(arena, "'utf-8' codec can't decode byte 0x%02x in position %zu: %s",
                              chunk[bad ? bad_at : ok], bad ? bad_at : ok, why));
            break;
        }
        for (size_t i = 0; i < ok && sv.code < 0; i++) {
            char ch = (char)chunk[i];
            if (pending_cr) {
                pending_cr = false;
                if (ch == '\n') continue; /* \r\n is one line break */
            }
            if (ch == '\n' || ch == '\r') {
                AvpArenaMark mark = avp_arena_mark(arena);
                serve_line(&sv, avp_buf_str(&line));
                avp_arena_reset(arena, mark);
                avp_buf_clear(&line);
                pending_cr = ch == '\r';
            } else avp_buf_putc(&line, ch);
        }
        if (sv.code >= 0) break;
        carry = n - ok;
        memmove(chunk, chunk + ok, carry);
        if (eof) break;
    }
    if (sv.code < 0 && line.n) {
        AvpArenaMark mark = avp_arena_mark(arena);
        serve_line(&sv, avp_buf_str(&line));
        avp_arena_reset(arena, mark);
    }
    avp_buf_free(&line);
    return sv.code < 0 ? 0 : sv.code;
}
