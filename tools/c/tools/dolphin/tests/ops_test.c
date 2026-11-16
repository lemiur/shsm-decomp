/* tools/test_dolphin.py TestSymbols and TestOps against the fake GDB stub (shsm_gdbfake) and a fixture checkout. */
#include "dolphin_fixture.h"

/* ---- TestSymbols ---- */

AVP_TEST(resolve) {
    const char *root = fx_root("resolve");
    DolphinEnv *env = dolphin_env_new(root);
    const ShsmSymbol *s = NULL;
    CHECK_EQ_U64(dolphin_resolve(env, AVP_S("helper"), NULL, NULL), 0x80001010u);
    CHECK_EQ_U64(dolphin_resolve(env, AVP_S("unk_Ramp::fn_80001018(float)"), NULL, NULL), 0x80001018u);
    CHECK_EQ_U64(dolphin_resolve(env, AVP_S("unk_Ramp::fn_80001018"), NULL, NULL), 0x80001018u);
    CHECK_EQ_U64(dolphin_resolve(env, AVP_S("0x80001014"), NULL, &s), 0x80001014u);
    REQUIRE(s != NULL);
    CHECK_CZ(s->name, "helper");
    FX_RAISES(dolphin_resolve(env, AVP_S("nothing_here"), NULL, NULL), SHSM_EXC_USAGE, "no symbol matches");
    FX_RAISES(dolphin_resolve(env, AVP_S("helper"), "object", NULL), SHSM_EXC_USAGE, "helper is a function, not a object");
    dolphin_env_close(env);
}

AVP_TEST(default_signature) {
    AvpArena *a = avp_arena();
    DolphinSig s = dolphin_default_signature(a, "fn_80001018__8unk_RampFf");
    REQUIRE(!s.none && s.n == 2);
    CHECK_CZ(s.v[0].name, "this");
    CHECK_CZ(s.v[0].type, "obj");
    CHECK_CZ(s.v[1].name, "p1");
    CHECK_CZ(s.v[1].type, "float");
    s = dolphin_default_signature(a, "fn_800C770C__FPCci");
    REQUIRE(!s.none && s.n == 2);
    CHECK_CZ(s.v[0].name, "p1");
    CHECK_CZ(s.v[0].type, "str");
    CHECK_CZ(s.v[1].name, "p2");
    CHECK_CZ(s.v[1].type, "int");
    CHECK(dolphin_default_signature(a, "memcpy").none);
    s = dolphin_parse_signature(a, AVP_S("this:ptr, v:float"));
    REQUIRE(s.n == 2);
    CHECK_CZ(s.v[0].name, "this");
    CHECK_CZ(s.v[0].type, "ptr");
    CHECK_CZ(s.v[1].name, "v");
    CHECK_CZ(s.v[1].type, "float");
    FX_RAISES(dolphin_parse_signature(a, AVP_S("x:nope")), SHSM_EXC_USAGE, "unknown argument type 'nope'");
}

AVP_TEST(user_dir_guard) {
    const char *root = fx_root("guard");
    DolphinEnv *env = dolphin_env_new(root);
    AvpArena *a = avp_arena();
    dolphin_guard_user_dir(env, avp_path_join(a, env->dir, "user"));
    FX_RAISES(dolphin_guard_user_dir(env, avp_path_join(a, root, "elsewhere")), SHSM_EXC_USAGE, "outside build/dolphin");
}

/* ---- TestOps ---- */

static const char *field(const DolphinResult *r, size_t i, const char *key) {
    AvpJson *f = avp_json_at(avp_json_getz(r->data, "fields"), i);
    return avp_cz(avp_arena(), avp_json_get_str(f, key, AVP_S("")));
}

AVP_TEST(mem_and_compare_dol) {
    const char *root = fx_root("mem");
    ShsmGdbFake *stub = shsm_gdbfake_new();
    u8 image[16];
    for (int i = 0; i < 16; i++) stub->mem[0x1000 + i] = (u8)i, image[i] = (u8)i;
    FxLive l = fx_live(root, stub);
    fx_write_dol(root, image, 16);
    DolphinResult *r = fx_call(l.env, "mem", "{\"address\": \"fn_80001000__Fv\", \"length\": 16, \"view\": \"u8\", "
                                              "\"compare_dol\": true}");
    CHECK_EQ_I64(r->code, 0);
    CHECK(avp_json_get_bool(r->data, "dol_equal", false));
    /* stub.mem[0x1003] = 0xFF: change it through the protocol (the stub thread owns its memory) */
    u8 ff = 0xFF;
    shsm_gdb_write_memory(l.client, 0x80001003, &ff, 1);
    DolphinEnv *env2 = dolphin_env_new(root);
    env2->target = &l.target;
    r = fx_call(env2, "mem", "{\"address\": \"0x80001000\", \"length\": 16, \"view\": \"u32\", \"compare_dol\": true}");
    CHECK_EQ_I64(r->code, SHSM_EXIT_FINDING);
    CHECK(strstr(dolphin_result_text(avp_arena(), r, 200).p, "first +0x3") != NULL);
    dolphin_env_close(env2);
    fx_live_free(&l);
}

AVP_TEST(read_typed) {
    const char *root = fx_root("read");
    ShsmGdbFake *stub = shsm_gdbfake_new();
    static const u8 obj[12] = {0x80, 0x00, 0x10, 0x00, 0x80, 0x00, 0x20, 0x02, 0x12, 0x34, 1, 2};
    memcpy(stub->mem + 0x2008, obj, 12);
    float f = 2.5f;
    u32 bits;
    memcpy(&bits, &f, 4);
    fx_be32(stub->mem + 0x3000, bits);
    FxLive l = fx_live(root, stub);
    DolphinResult *r = fx_call(l.env, "read", "{\"symbol\": \"lbl_80002008\"}");
    CHECK_CZ(field(r, 0, "kind"), "ptr");
    CHECK_CZ(field(r, 1, "kind"), "ptr");
    CHECK_CZ(field(r, 2, "kind"), "u16");
    CHECK(strstr(field(r, 0, "value"), "fn_80001000__Fv") != NULL);
    r = fx_call(l.env, "read", "{\"symbol\": \"lbl_80003000\"}");
    CHECK_CZ(field(r, 0, "value"), "2.5");
    fx_live_free(&l);
}

static bool mem_is(ShsmGdbClient *c, u32 addr, const u8 *want, size_t n) {
    ShsmBytes b = shsm_gdb_read_memory(avp_arena(), c, addr, (i64)n);
    return b.n == n && memcmp(b.p, want, n) == 0;
}

AVP_TEST(write_journal_undo) {
    const char *root = fx_root("write");
    FxLive l = fx_live(root, shsm_gdbfake_new());
    FX_RAISES(fx_call(l.env, "write", "{\"address\": \"0x80000100\", \"value\": \"0x11223344\", \"view\": \"u32\"}"),
              SHSM_EXC_USAGE, "add --confirm");
    CHECK(!avp_exists(l.env->journal_path));
    DolphinResult *r = fx_call(l.env, "write", "{\"address\": \"0x80000100\", \"value\": \"0x11223344\", \"view\": "
                                               "\"u32\", \"confirm\": true}");
    CHECK_EQ_I64(r->code, 0);
    CHECK(mem_is(l.client, 0x80000100, (const u8 *)"\x11\x22\x33\x44", 4));
    fx_call(l.env, "write", "{\"address\": \"0x80000104\", \"value\": \"1.5\", \"view\": \"float\", \"confirm\": true}");
    AvpJson *j = dolphin_journal_entries(l.env);
    REQUIRE(avp_json_len(j) == 2);
    CHECK_EQ_I64(avp_json_get_i64(avp_json_at(j, 0), "id", 0), 1);
    CHECK_EQ_I64(avp_json_get_i64(avp_json_at(j, 1), "id", 0), 2);
    FX_RAISES(fx_call(l.env, "undo", NULL), SHSM_EXC_USAGE, "add --confirm");
    fx_call(l.env, "undo", "{\"confirm\": true}");
    CHECK(mem_is(l.client, 0x80000104, (const u8 *)"\0\0\0\0", 4));
    u8 x = 0x99;
    shsm_gdb_write_memory(l.client, 0x80000100, &x, 1); /* memory changed since write #1 */
    FX_RAISES(fx_call(l.env, "undo", "{\"confirm\": true}"), SHSM_EXC_USAGE, "--force");
    fx_call(l.env, "undo", "{\"confirm\": true, \"force\": true}");
    CHECK(mem_is(l.client, 0x80000100, (const u8 *)"\0\0\0\0", 4));
    CHECK_EQ_I64(fx_call(l.env, "undo", "{\"confirm\": true}")->code, SHSM_EXIT_FINDING);
    fx_live_free(&l);
}

static void put_obj(ShsmGdbFake *stub) { /* struct.pack(">IfI", VTABLE, 3.5, 0x80001010) at OBJ */
    float f = 3.5f;
    u32 bits;
    memcpy(&bits, &f, 4);
    u8 *p = stub->mem + (FX_OBJ - SHSM_GDBFAKE_MEM_BASE);
    fx_be32(p, FX_VTABLE);
    fx_be32(p + 4, bits);
    fx_be32(p + 8, 0x80001010u);
}

static AvpStr arg_value(const DolphinResult *r, size_t hit, size_t i) {
    AvpJson *h = avp_json_at(avp_json_getz(r->data, "hits"), hit);
    return avp_json_get_str(avp_json_at(avp_json_getz(h, "args"), i), "value", AVP_S(""));
}

AVP_TEST(calls_identifies_the_class_of_this) {
    const char *root = fx_root("calls");
    static ShsmGdbFakeStep prog[2];
    memset(prog, 0, sizeof prog);
    prog[0].pc = 0x80001018u, prog[0].ngpr = 1, prog[0].gpr[0].index = 3, prog[0].gpr[0].value = FX_OBJ;
    prog[0].nfpr = 1, prog[0].fpr[0].index = 1, prog[0].fpr[0].value = 1.25, prog[0].has_lr = true, prog[0].lr = 0x80001104u;
    prog[1] = prog[0];
    prog[1].gpr[0].value = FX_OBJ + 0x100, prog[1].fpr[0].value = -2.0;
    ShsmGdbFake *stub = shsm_gdbfake_new();
    stub->program = prog, stub->nprogram = 2;
    put_obj(stub);
    FxLive l = fx_live(root, stub);
    DolphinResult *r = fx_call(l.env, "calls", "{\"function\": \"unk_Ramp::fn_80001018\", \"count\": 2, \"timeout\": 3}");
    AvpJson *hits = avp_json_getz(r->data, "hits");
    REQUIRE(avp_json_len(hits) == 2);
    AvpJson *args = avp_json_getz(avp_json_at(hits, 0), "args");
    REQUIRE(avp_json_len(args) == 2);
    CHECK_STR(avp_json_get_str(avp_json_at(args, 0), "name", AVP_S("")), "this");
    CHECK_STR(avp_json_get_str(avp_json_at(args, 1), "name", AVP_S("")), "p1");
    CHECK(avp_str_find(arg_value(r, 0, 0), AVP_S("<unk_Thing>"), 0, AVP_END) >= 0);
    CHECK_STR(arg_value(r, 0, 1), "1.25");
    CHECK(avp_str_find(arg_value(r, 1, 0), AVP_S("unk_Thing"), 0, AVP_END) < 0);
    CHECK_STR(avp_json_get_str(avp_json_at(hits, 0), "caller", AVP_S("")), "jumper");
    CHECK_EQ_U64(shsm_gdbfake_bp_count(stub), 0);
    r = fx_call(l.env, "calls", "{\"function\": \"fn_80001018__8unk_RampFf\", \"count\": 1, \"timeout\": 0.3, "
                                "\"args\": \"self:ptr, x:float\"}");
    CHECK_EQ_I64(r->code, SHSM_EXIT_FINDING);
    fx_live_free(&l);
}

AVP_TEST(object) {
    const char *root = fx_root("object");
    ShsmGdbFake *stub = shsm_gdbfake_new();
    put_obj(stub);
    FxLive l = fx_live(root, stub);
    DolphinResult *r = fx_call(l.env, "object", "{\"address\": \"0x80008000\"}");
    CHECK_STR(avp_json_get_str(r->data, "class", AVP_S("")), "unk_Thing");
    REQUIRE(avp_json_len(avp_json_getz(r->data, "fields")) == 3);
    CHECK_CZ(field(r, 1, "value"), "3.5");
    CHECK_CZ(field(r, 2, "value"), "0x80001010 <helper>");
    static const u8 zero[4] = {0};
    shsm_gdb_write_memory(l.client, FX_OBJ, zero, 4);
    r = fx_call(l.env, "object", "{\"address\": \"0x80008000\"}");
    CHECK(avp_json_is_null(avp_json_getz(r->data, "class")));
    r = fx_call(l.env, "object", "{\"address\": \"0x80008000\", \"cls\": \"unk_Thing\"}");
    CHECK_STR(avp_json_get_str(r->data, "class", AVP_S("")), "unk_Thing");
    fx_live_free(&l);
}

AVP_TEST(where_bt_regs) {
    const char *root = fx_root("where");
    ShsmGdbFake *stub = shsm_gdbfake_new();
    stub->pc = 0x80001018u;
    stub->lr = 0x80001104u;
    fx_be32(stub->mem + 0xF000, 0x8000F100u); /* r1 -> 0x8000F000 -> back 0x8000F100 (LR slot +4 = 0x80001024) */
    fx_be32(stub->mem + 0xF104, 0x80001024u);
    FxLive l = fx_live(root, stub);
    DolphinResult *w = fx_call(l.env, "where", NULL);
    CHECK_STR(avp_json_get_str(w->data, "function", AVP_S("")), "fn_80001018__8unk_RampFf");
    DolphinResult *bt = fx_call(l.env, "bt", NULL);
    AvpJson *frames = avp_json_getz(bt->data, "frames");
    REQUIRE(avp_json_len(frames) == 3);
    CHECK_EQ_I64(avp_json_get_i64(avp_json_at(frames, 0), "pc", 0), 0x80001018);
    CHECK_EQ_I64(avp_json_get_i64(avp_json_at(frames, 1), "pc", 0), 0x80001104);
    CHECK_EQ_I64(avp_json_get_i64(avp_json_at(frames, 2), "pc", 0), 0x80001024);
    DolphinResult *r = fx_call(l.env, "regs", "{\"fpr\": true}");
    CHECK_EQ_I64(avp_json_get_i64(avp_json_getz(r->data, "special"), "pc", 0), 0x80001018);
    CHECK_EQ_U64(avp_json_len(avp_json_getz(r->data, "fpr")), 32);
    fx_live_free(&l);
}

AVP_TEST(trace_counts) {
    const char *root = fx_root("trace");
    static ShsmGdbFakeStep prog[3];
    memset(prog, 0, sizeof prog);
    prog[0].pc = 0x80001000u, prog[1].pc = 0x80001010u, prog[2].pc = 0x80001000u;
    ShsmGdbFake *stub = shsm_gdbfake_new();
    stub->program = prog, stub->nprogram = 3;
    stub->pc = 0x80000F00u;
    FxLive l = fx_live(root, stub);
    DolphinResult *r = fx_call(l.env, "trace", "{\"functions\": [\"fn_80001000__Fv\", \"helper\", \"jumper\"], "
                                               "\"seconds\": 1.0}");
    AvpJson *c = avp_json_getz(r->data, "counts");
    REQUIRE(avp_json_len(c) == 3);
    for (size_t i = 0; i < 3; i++) {
        AvpStr fn = avp_json_get_str(avp_json_at(c, i), "function", AVP_S(""));
        i64 hits = avp_json_get_i64(avp_json_at(c, i), "hits", -1);
        if (avp_str_eqz(fn, "fn_80001000__Fv")) CHECK_EQ_I64(hits, 2);
        else if (avp_str_eqz(fn, "helper")) CHECK_EQ_I64(hits, 1);
        else CHECK(avp_str_eqz(fn, "jumper") && hits == 0);
    }
    CHECK_EQ_U64(shsm_gdbfake_bp_count(stub), 0);
    fx_live_free(&l);
}

AVP_TEST(break_and_clear) {
    const char *root = fx_root("break");
    ShsmGdbFake *stub = shsm_gdbfake_new();
    FxLive l = fx_live(root, stub);
    fx_call(l.env, "break", "{\"functions\": [\"helper\", \"0x80001100\"]}");
    CHECK_EQ_U64(shsm_gdbfake_bp_count(stub), 2);
    CHECK(shsm_gdbfake_received_count(stub, "Z0,80001010,4") == 1);
    CHECK(shsm_gdbfake_received_count(stub, "Z0,80001100,4") == 1);
    fx_call(l.env, "break", "{\"functions\": [\"lbl_80002000\"], \"watch\": \"w\"}");
    CHECK(shsm_gdbfake_received_count(stub, "Z2,80002000,6") == 1);
    CHECK_EQ_I64(avp_json_get_i64(fx_call(l.env, "clear", NULL)->data, "cleared", -1), 3);
    fx_live_free(&l);
}

AVP_TEST_MAIN("dolphin")
