/* tools/test_mcp_server.py TestOverPipes and TestHelp: build/tools/mcp_server.exe driven through pipes by a scripted
 * client (one JSON-RPC message per line), on the dolphin tests' fixture checkout. Each test starts its own server. */
#include "../../dolphin/tests/dolphin_fixture.h"
#include "mcp_server.h"

typedef struct {
    AvpProc *p;
    i64 next_id;
} Client;

static const char *server_exe(void) { return avp_join(avp_arena(), avp_exe_dir(), "..", "mcp_server.exe"); }

static Client client_open(const char *root) {
    const char *argv[] = {server_exe(), "--root", root, NULL};
    AvpProcResult fail;
    Client c = {avp_proc_start(&(AvpProcSpec){.argv = argv, .in = AVP_IO_CAPTURE, .out = AVP_IO_CAPTURE,
                                              .err = AVP_IO_NULL},
                               &fail),
                0};
    AVP_ASSERT(c.p);
    return c;
}

static void client_send_raw(Client *c, const char *line) {
    AVP_ASSERT(avp_proc_write(c->p, line, strlen(line)));
    AVP_ASSERT(avp_proc_write(c->p, "\n", 1));
}

static AvpJson *client_read(Client *c) {
    AvpStr line;
    if (!avp_proc_readline(c->p, 1, &line)) {
        avp_test__fail(__FILE__, __LINE__, "server closed stdout");
        return avp_json_obj(avp_arena());
    }
    AvpJsonError err;
    AvpJson *j = avp_json_parse(avp_arena(), avp_str_strip(line), &err);
    AVP_ASSERT(j);
    return j;
}

/* request(method, params): params is JSON text or NULL */
static AvpJson *client_request(Client *c, const char *method, const char *params) {
    c->next_id++;
    AvpArena *a = avp_arena();
    const char *msg = params ? avp_sprintf(a, "{\"jsonrpc\": \"2.0\", \"id\": %lld, \"method\": \"%s\", \"params\": %s}",
                                           (long long)c->next_id, method, params)
                             : avp_sprintf(a, "{\"jsonrpc\": \"2.0\", \"id\": %lld, \"method\": \"%s\"}",
                                           (long long)c->next_id, method);
    client_send_raw(c, msg);
    AvpJson *r = client_read(c);
    CHECK_EQ_I64(avp_json_get_i64(r, "id", -1), c->next_id);
    return r;
}

static void client_close(Client *c) {
    avp_proc_close_stdin(c->p);
    i64 code = -1;
    CHECK(avp_proc_wait(c->p, 10, &code));
    CHECK_EQ_I64(code, 0);
    avp_proc_close(c->p);
}

static AvpJson *result(AvpJson *r) { return avp_json_getz(r, "result"); }
static bool is_error(AvpJson *r) { return avp_json_get_bool(result(r), "isError", false); }
static AvpStr text_of(AvpJson *r) {
    return avp_json_get_str(avp_json_at(avp_json_getz(result(r), "content"), 0), "text", AVP_S(""));
}
static bool contains(AvpStr s, const char *part) { return avp_str_find(s, avp_str(part), 0, AVP_END) >= 0; }
static i64 error_code(AvpJson *r) { return avp_json_get_i64(avp_json_getz(r, "error"), "code", 0); }

static bool in_required(AvpJson *schema, const char *name) {
    AvpJson *req = avp_json_getz(schema, "required");
    for (size_t i = 0; i < avp_json_len(req); i++)
        if (avp_str_eqz(avp_json_str_or(avp_json_at(req, i), AVP_S("")), name)) return true;
    return false;
}

AVP_TEST(over_pipes_1_handshake) {
    Client c = client_open(fx_root("mcp-pipes"));
    AvpJson *r = client_request(&c, "initialize",
                                "{\"protocolVersion\": \"2024-11-05\", \"capabilities\": {}, "
                                "\"clientInfo\": {\"name\": \"test\", \"version\": \"0\"}}");
    AvpJson *res = result(r);
    CHECK_STR(avp_json_get_str(res, "protocolVersion", AVP_S("")), "2024-11-05");
    CHECK(avp_json_getz(avp_json_getz(res, "capabilities"), "tools") != NULL);
    CHECK_STR(avp_json_get_str(avp_json_getz(res, "serverInfo"), "name", AVP_S("")), "shsm-dolphin");
    client_send_raw(&c, "{\"jsonrpc\": \"2.0\", \"method\": \"notifications/initialized\"}");
    r = client_request(&c, "ping", NULL);
    CHECK(avp_json_is_obj(result(r)) && avp_json_len(result(r)) == 0);
    r = client_request(&c, "initialize", "{\"protocolVersion\": \"1999-01-01\", \"capabilities\": {}}");
    CHECK_STR(avp_json_get_str(result(r), "protocolVersion", AVP_S("")), "2024-11-05");
    client_close(&c);
}

AVP_TEST(over_pipes_2_list) {
    Client c = client_open(fx_root("mcp-pipes"));
    AvpJson *tools = avp_json_getz(result(client_request(&c, "tools/list", NULL)), "tools");
    CHECK_EQ_U64(avp_json_len(tools), dolphin_nops);
    AvpJson *mem = NULL, *write = NULL, *quit = NULL;
    for (size_t i = 0; i < avp_json_len(tools) && i < dolphin_nops; i++) {
        AvpJson *t = avp_json_at(tools, i);
        AvpStr name = avp_json_get_str(t, "name", AVP_S(""));
        CHECK_STR(name, avp_sprintf(avp_arena(), "dolphin_%s", dolphin_ops[i].name));
        AvpJson *schema = avp_json_getz(t, "inputSchema");
        if (avp_str_eqz(name, "dolphin_mem")) mem = schema;
        if (avp_str_eqz(name, "dolphin_write")) write = schema;
        if (avp_str_eqz(name, "dolphin_quit")) quit = schema;
    }
    REQUIRE(mem && write && quit);
    CHECK_STR(avp_json_get_str(mem, "type", AVP_S("")), "object");
    AvpJson *req = avp_json_getz(mem, "required");
    CHECK(avp_json_len(req) == 1 && avp_str_eqz(avp_json_str_or(avp_json_at(req, 0), AVP_S("")), "address"));
    CHECK(in_required(write, "confirm"));
    CHECK(in_required(quit, "confirm"));
    client_close(&c);
}

AVP_TEST(over_pipes_3_call) {
    Client c = client_open(fx_root("mcp-pipes"));
    AvpJson *r = client_request(&c, "tools/call", "{\"name\": \"dolphin_status\", \"arguments\": {}}");
    CHECK(!is_error(r));
    CHECK(contains(text_of(r), "no session"));
    r = client_request(&c, "tools/call", "{\"name\": \"dolphin_status\", \"arguments\": {\"json\": true}}");
    AvpJsonError err;
    AvpJson *doc = avp_json_parse(avp_arena(), text_of(r), &err);
    REQUIRE(doc);
    CHECK(avp_json_has(doc, AVP_S("session")) && avp_json_is_null(avp_json_getz(doc, "session")));
    r = client_request(&c, "tools/call", "{\"name\": \"dolphin_mem\", \"arguments\": {\"address\": \"helper\"}}");
    CHECK(is_error(r));
    CHECK(contains(text_of(r), "no Dolphin session"));
    client_close(&c);
}

AVP_TEST(over_pipes_4_unknown_tool_and_method) {
    Client c = client_open(fx_root("mcp-pipes"));
    AvpJson *r = client_request(&c, "tools/call", "{\"name\": \"dolphin_nope\", \"arguments\": {}}");
    CHECK_EQ_I64(error_code(r), -32602);
    r = client_request(&c, "resources/list", NULL);
    CHECK_EQ_I64(error_code(r), -32601);
    client_send_raw(&c, "\"not json\"");
    client_send_raw(&c, "{oops");
    CHECK_EQ_I64(error_code(client_read(&c)), -32600);
    CHECK_EQ_I64(error_code(client_read(&c)), -32700);
    client_close(&c);
}

AVP_TEST(over_pipes_5_write_without_confirmation) {
    static const char *const cases[][2] = {
        {"dolphin_write", "{\"address\": \"0x80000000\", \"value\": \"1\"}"},
        {"dolphin_write", "{\"address\": \"0x80000000\", \"value\": \"1\", \"confirm\": false}"},
        {"dolphin_quit", "{}"},
        {"dolphin_undo", "{}"},
    };
    Client c = client_open(fx_root("mcp-pipes"));
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
        AvpJson *r = client_request(&c, "tools/call",
                                    avp_sprintf(avp_arena(), "{\"name\": \"%s\", \"arguments\": %s}", cases[i][0],
                                                cases[i][1]));
        CHECK_MSG(is_error(r), "%s", cases[i][0]);
        CHECK(contains(text_of(r), "confirm"));
    }
    client_close(&c);
}

AVP_TEST(over_pipes_6_bad_arguments) {
    Client c = client_open(fx_root("mcp-pipes"));
    AvpJson *r = client_request(&c, "tools/call",
                                "{\"name\": \"dolphin_mem\", \"arguments\": {\"address\": \"x\", \"length\": \"big\"}}");
    CHECK(is_error(r));
    r = client_request(&c, "tools/call", "{\"name\": \"dolphin_mem\", \"arguments\": {}}");
    CHECK(contains(text_of(r), "missing"));
    r = client_request(&c, "tools/call",
                       "{\"name\": \"dolphin_mem\", \"arguments\": {\"address\": \"helper\", \"zz\": 1}}");
    CHECK(contains(text_of(r), "unknown argument"));
    client_close(&c);
}

AVP_TEST(help_lists_the_tools_and_exits_0) {
    const char *argv[] = {server_exe(), "--help", NULL};
    AvpProcResult r = avp_run(&(AvpProcSpec){.argv = argv, .in = AVP_IO_NULL, .out = AVP_IO_CAPTURE,
                                             .err = AVP_IO_CAPTURE, .text = true, .timeout = 60});
    CHECK_EQ_I64(r.returncode, 0);
    for (size_t i = 0; i < dolphin_nops; i++)
        CHECK_MSG(contains(r.out, avp_sprintf(avp_arena(), "dolphin_%s", dolphin_ops[i].name)), "%s", dolphin_ops[i].name);
    CHECK(contains(r.out, "mcp.example.json"));
}

AVP_TEST_MAIN("mcp_server_pipes")
