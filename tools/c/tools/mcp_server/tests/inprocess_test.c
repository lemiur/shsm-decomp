/* tools/test_mcp_server.py TestInProcess: tool calls in process against the fake GDB stub (lib/shsm_gdbfake.h),
 * through the dolphin tests' fixture checkout. */
#include "../../dolphin/tests/dolphin_fixture.h"
#include "mcp_server.h"

static DolphinEnv *env_with_target(void *ctx, const char *root) {
    DolphinEnv *env = dolphin_env_new(root);
    env->target = (ShsmGdbTarget *)ctx;
    return env;
}

static AvpJson *call(McpServer *s, const char *name, const char *args) {
    AvpArena *a = avp_arena();
    AvpJsonError err;
    AvpJson *j = avp_json_parse(a, avp_str(args), &err);
    AVP_ASSERT(j);
    AvpJson *r = mcp_call_tool(s, a, avp_str(name), j);
    AVP_ASSERT(r);
    return r;
}

static bool is_error(const AvpJson *r) { return avp_json_get_bool(r, "isError", false); }
static AvpStr text_of(const AvpJson *r) {
    return avp_json_get_str(avp_json_at(avp_json_getz(r, "content"), 0), "text", AVP_S(""));
}

AVP_TEST(calls_against_a_fake_stub) {
    const char *root = fx_root("mcp-inprocess");
    ShsmGdbFake *stub = shsm_gdbfake_new();
    fx_be32(stub->mem + 0x1010, 0x2C030000);
    FxLive live = fx_live(root, stub);
    McpServer s = {.root = root, .env_factory = env_with_target, .env_ctx = &live.target};

    AvpJson *res = call(&s, "dolphin_mem", "{\"address\": \"helper\", \"length\": 4, \"view\": \"u32\"}");
    CHECK(!is_error(res));
    CHECK(avp_str_find(text_of(res), AVP_S("2C030000"), 0, AVP_END) >= 0);

    res = call(&s, "dolphin_write", "{\"address\": \"0x80000200\", \"value\": \"0x01020304\", \"confirm\": true}");
    CHECK_MSG(!is_error(res), "%.*s", AVP_SF(text_of(res)));
    static const u8 written[4] = {1, 2, 3, 4}, zero[4] = {0};
    CHECK_MEM(stub->mem + 0x200, written, 4);

    res = call(&s, "dolphin_undo", "{\"confirm\": true}");
    CHECK_MEM(stub->mem + 0x200, zero, 4);

    res = call(&s, "dolphin_break", "{\"functions\": [\"helper\"]}");
    CHECK_EQ_U64(shsm_gdbfake_bp_count(stub), 1);
    CHECK_EQ_U64(stub->bps.n ? stub->bps.v[0] : 0, 0x80001010u);

    fx_live_free(&live);
}

AVP_TEST_MAIN("mcp_server_inprocess")
