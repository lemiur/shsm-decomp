/* shsm_gdb: framing, the client against the fake stub (shsm_gdbfake), the broker and its client
 * (tools/test_dolphin.py TestFraming, TestClient, TestBroker). */
#include "avp/clock.h"
#include "avp/fmt.h"
#include "avp/pool.h"
#include "avp/test.h"
#include "shsm_cli.h"
#include "shsm_gdb.h"
#include "shsm_gdbfake.h"

#include <stdio.h>
#include <stdlib.h>

static AvpStr cs_str(AvpArena *a, const char *p) { return avp_str(avp_sprintf(a, "%02x", shsm_gdb_checksum(p, strlen(p)))); }

/* catches one exception: its code, 0 when none */
#define CATCH(code_var, stmt)                                                                                         \
    do {                                                                                                              \
        AvpTry t_;                                                                                                    \
        code_var = 0;                                                                                                 \
        if (AVP_TRY(&t_)) {                                                                                           \
            stmt;                                                                                                     \
            avp_try_end(&t_);                                                                                         \
        } else {                                                                                                      \
            code_var = t_.code;                                                                                       \
            snprintf(g_msg, sizeof g_msg, "%s", t_.msg);                                                              \
        }                                                                                                             \
    } while (0)
static char g_msg[1024];

static ShsmGdbClient *connect_fake(ShsmGdbFake *f, double timeout) {
    shsm_gdbfake_start(f);
    return shsm_gdb_connect("127.0.0.1", f->port, timeout, 2.0);
}

AVP_TEST(frame_and_checksum) {
    AvpArena *a = avp_arena();
    CHECK_STR(shsm_gdb_frame(a, AVP_S("g")), "$g#67");
    AvpStr f = shsm_gdb_frame(a, AVP_S("m80000000,4"));
    CHECK(avp_str_eq(f, avp_str(avp_sprintf(a, "$m80000000,4#%s", cs_str(a, "m80000000,4").p))));
    f = shsm_gdb_frame(a, AVP_S("a#b"));
    CHECK(avp_str_eq(f, avp_str(avp_sprintf(a, "$a}\x03" "b#%s", cs_str(a, "a}\x03" "b").p))));
}

AVP_TEST(unescape_and_rle) {
    AvpArena *a = avp_arena();
    CHECK_STR(shsm_gdb_unescape(a, AVP_S("a}\x03" "b")), "a#b");
    CHECK_STR(shsm_gdb_unescape(a, AVP_S("0* ")), "0000"); /* ' ' = 32: three more copies */
}

AVP_TEST(stop_replies) {
    AvpArena *a = avp_arena();
    ShsmGdbStop *st = shsm_gdb_parse_stop(a, AVP_S("T0540:80211350;01:805400b0;"));
    CHECK_EQ_I64(st->signal, 5);
    CHECK(st->has_pc && st->pc == 0x80211350);
    CHECK(st->has_sp && st->sp == 0x805400B0);
    CHECK(shsm_gdb_is_stop_reply(AVP_S("T05")));
    CHECK(!shsm_gdb_is_stop_reply(AVP_S("OK")));
    CHECK(!shsm_gdb_is_stop_reply(AVP_S("80211350")));
    /* the JSON form, as json.dumps(parse_stop(...)) */
    AvpJsonDumpOpts o = {0};
    CHECK_STR(avp_json_dumps(a, shsm_gdb_stop_to_json(a, st), &o),
              "{\"kind\": \"T\", \"signal\": 5, \"regs\": {\"40\": 2149651280, \"01\": 2152988848}, \"pc\": 2149651280, "
              "\"sp\": 2152988848, \"raw\": \"T0540:80211350;01:805400b0;\"}");
    ShsmGdbStop *back = shsm_gdb_stop_from_json(a, shsm_gdb_stop_to_json(a, st));
    CHECK_STR(avp_json_dumps(a, shsm_gdb_stop_to_json(a, back), &o),
              "{\"kind\": \"T\", \"signal\": 5, \"regs\": {\"40\": 2149651280, \"01\": 2152988848}, \"pc\": 2149651280, "
              "\"sp\": 2152988848, \"raw\": \"T0540:80211350;01:805400b0;\"}");
    st = shsm_gdb_parse_stop(a, AVP_S("S02"));
    CHECK_STR(avp_json_dumps(a, shsm_gdb_stop_to_json(a, st), &o),
              "{\"kind\": \"S\", \"signal\": 2, \"regs\": {}, \"pc\": null, \"sp\": null, \"raw\": \"S02\"}");
    int code;
    CATCH(code, shsm_gdb_check_error(AVP_S("m80000000,4"), AVP_S("E01")));
    CHECK_EQ_I64(code, SHSM_EXC_GDB);
    CHECK_CZ(g_msg, "stub refused m80000000: E01");
}

AVP_TEST(registers_memory_breakpoints) {
    AvpArena *a = avp_arena();
    ShsmGdbFake *f = shsm_gdbfake_new();
    f->gpr[3] = 0x12345678;
    memcpy(f->mem + 0x10, "\xde\xad\xbe\xef", 4);
    ShsmGdbClient *c = connect_fake(f, 2.0);
    ShsmGdbStop *st = shsm_gdb_halt_reason(a, c);
    CHECK(st->pc == 0x80001000);
    u64 r[32];
    CHECK_EQ_U64(shsm_gdb_read_gprs(c, r), 32);
    CHECK_EQ_U64(r[3], 0x12345678);
    CHECK_EQ_U64(shsm_gdb_read_register(c, SHSM_GDB_REG_LR), 0x80001104);
    ShsmBytes m = shsm_gdb_read_memory(a, c, 0x80000010, 4);
    CHECK(m.n == 4 && !memcmp(m.p, "\xde\xad\xbe\xef", 4));
    u8 big[256 * 12]; /* crosses MEM_CHUNK */
    for (size_t i = 0; i < sizeof big; i++) big[i] = (u8)i;
    shsm_gdb_write_memory(c, 0x80004000, big, sizeof big);
    m = shsm_gdb_read_memory(a, c, 0x80004000, sizeof big);
    CHECK(m.n == sizeof big && !memcmp(m.p, big, sizeof big));
    int code;
    CATCH(code, shsm_gdb_read_memory(a, c, 0x90000000, 4));
    CHECK_EQ_I64(code, SHSM_EXC_GDB);
    CHECK_CZ(g_msg, "stub refused m90000000: E01");
    shsm_gdb_insert_breakpoint(c, 0x80001018, 0, 4);
    shsm_gdb_insert_breakpoint(c, 0x80002000, 2, 4);
    ShsmGdbBreakpoint *bps;
    CHECK_EQ_U64(shsm_gdb_breakpoints(a, c, &bps), 2);
    shsm_gdb_clear_breakpoints(c);
    CHECK_EQ_U64(shsm_gdb_breakpoints(a, c, &bps), 0);
    CHECK_EQ_U64(shsm_gdbfake_bp_count(f), 0);
    shsm_gdb_client_free(c);
    shsm_gdbfake_free(f);
}

AVP_TEST(bad_checksum_is_retried) {
    ShsmGdbFake *f = shsm_gdbfake_new();
    f->corrupt = 1;
    f->gpr[5] = 7;
    ShsmGdbClient *c = connect_fake(f, 2.0);
    u64 r[32];
    shsm_gdb_read_gprs(c, r);
    CHECK_EQ_U64(r[5], 7);
    CHECK_EQ_I64(shsm_gdb_bad_checksums(c), 1);
    shsm_gdb_client_free(c);
    shsm_gdbfake_free(f);
}

AVP_TEST(nak_resends) {
    ShsmGdbFake *f = shsm_gdbfake_new();
    f->nak = 1;
    ShsmGdbClient *c = connect_fake(f, 2.0);
    CHECK_EQ_U64(shsm_gdb_read_register(c, SHSM_GDB_REG_PC), 0x80001000);
    CHECK_EQ_U64(shsm_gdbfake_received_count(f, "p40"), 1);
    shsm_gdb_client_free(c);
    shsm_gdbfake_free(f);
}

AVP_TEST(stalled_reply_times_out) {
    AvpArena *a = avp_arena();
    ShsmGdbFake *f = shsm_gdbfake_new();
    f->stall_on = "m";
    ShsmGdbClient *c = connect_fake(f, 0.5);
    double t0 = avp_monotonic();
    int code;
    CATCH(code, shsm_gdb_read_memory(a, c, 0x80000000, 4));
    CHECK_EQ_I64(code, SHSM_EXC_GDB_TIMEOUT);
    CHECK_CZ(g_msg, "timed out waiting for the GDB stub");
    CHECK(avp_monotonic() - t0 < 3);
    shsm_gdb_client_free(c);
    shsm_gdbfake_free(f);
}

AVP_TEST(dropped_connection) {
    ShsmGdbFake *f = shsm_gdbfake_new();
    f->drop_on = "g";
    ShsmGdbClient *c = connect_fake(f, 2.0);
    u64 r[32];
    int code;
    CATCH(code, shsm_gdb_read_gprs(c, r));
    CHECK_EQ_I64(code, SHSM_EXC_GDB_DISCONNECTED);
    CHECK(shsm_exc_is_gdb(code));
    shsm_gdb_client_free(c);
    shsm_gdbfake_free(f);
}

AVP_TEST(connect_refused) {
    int port = shsm_net_free_port();
    int code;
    CATCH(code, shsm_gdb_connect("127.0.0.1", port, SHSM_GDB_DEFAULT_TIMEOUT, 0.3));
    CHECK_EQ_I64(code, SHSM_EXC_GDB_DISCONNECTED);
    char want[200];
    snprintf(want, sizeof want,
             "cannot connect to the GDB stub at 127.0.0.1:%d: [WinError 10061] No connection could be made because the "
             "target machine actively refused it",
             port);
    CHECK_CZ(g_msg, want);
}

AVP_TEST(resume_steps_off_a_breakpoint) {
    AvpArena *a = avp_arena();
    ShsmGdbFake *f = shsm_gdbfake_new();
    ShsmGdbFakeStep prog[2] = {{.pc = 0x80001018}, {.pc = 0x80001018, .ngpr = 1, .gpr = {{3, 2}}}};
    f->program = prog;
    f->nprogram = 2;
    ShsmGdbClient *c = connect_fake(f, 2.0);
    shsm_gdb_halt_reason(a, c);
    shsm_gdb_insert_breakpoint(c, 0x80001018, 0, 4);
    shsm_gdb_resume(c, SHSM_GDB_NONE);
    ShsmGdbStop *st = shsm_gdb_wait_stop(a, c, 2);
    REQUIRE(st);
    CHECK(st->pc == 0x80001018);
    shsm_gdb_resume(c, SHSM_GDB_NONE); /* Dolphin would stop here again without the step-off */
    st = shsm_gdb_wait_stop(a, c, 2);
    REQUIRE(st);
    CHECK(st->pc == 0x80001018);
    CHECK(shsm_gdbfake_received_count(f, "s") >= 1);
    u64 r[32];
    shsm_gdb_read_gprs(c, r);
    CHECK_EQ_U64(r[3], 2);
    shsm_gdb_client_free(c);
    shsm_gdbfake_free(f);
}

AVP_TEST(step) {
    AvpArena *a = avp_arena();
    ShsmGdbFake *f = shsm_gdbfake_new();
    ShsmGdbClient *c = connect_fake(f, 2.0);
    shsm_gdb_halt_reason(a, c);
    ShsmGdbStop *st = shsm_gdb_step(a, c, 3, SHSM_GDB_NONE);
    REQUIRE(st);
    CHECK(st->pc == 0x8000100C);
    shsm_gdb_client_free(c);
    shsm_gdbfake_free(f);
}

AVP_TEST(interrupt_drains_extra_packets) {
    AvpArena *a = avp_arena();
    ShsmGdbFake *f = shsm_gdbfake_new();
    ShsmGdbClient *c = connect_fake(f, 2.0);
    shsm_gdb_halt_reason(a, c);
    shsm_gdb_resume(c, SHSM_GDB_NONE);
    CHECK(shsm_gdb_wait_stop(a, c, 0.2) == NULL);
    ShsmGdbStop *st = shsm_gdb_interrupt(a, c, SHSM_GDB_NONE);
    REQUIRE(st);
    CHECK_EQ_I64(st->signal, 2);
    CHECK(!shsm_gdb_state(a, c).running);
    CHECK_EQ_U64(shsm_gdb_read_register(c, SHSM_GDB_REG_PC), 0x80001000);
    shsm_gdb_client_free(c);
    shsm_gdbfake_free(f);
}

/* ---- broker ---- */

static void serve_thread(void *arg) {
    ShsmBroker *b = arg;
    AvpTry t;
    if (AVP_TRY(&t)) {
        shsm_broker_serve(b);
        avp_try_end(&t);
    }
}

AVP_TEST(broker_roundtrip_and_token) {
    AvpArena *a = avp_arena();
    ShsmGdbFake *f = shsm_gdbfake_new();
    memcpy(f->mem, "\x01\x02\x03\x04", 4);
    ShsmGdbClient *gdb = connect_fake(f, 2.0);
    shsm_gdb_halt_reason(a, gdb);
    ShsmBroker *b = shsm_broker_new(gdb, "tok", NULL, 0, NULL, NULL, NULL);
    AvpThread *th = avp_thread_start(serve_thread, b, 0);
    ShsmBrokerClient *bc = shsm_broker_client_new(shsm_broker_port(b), "tok", NULL, 60.0);
    ShsmGdbTarget t = shsm_gdb_target_broker(bc);
    ShsmBytes m = shsm_target_read_memory(a, &t, 0x80000000, 4);
    CHECK(m.n == 4 && !memcmp(m.p, "\x01\x02\x03\x04", 4));
    ShsmGdbState s = shsm_target_state(a, &t);
    CHECK(!s.running);
    REQUIRE(s.stop);
    CHECK(s.stop->pc == 0x80001000);
    int code;
    CATCH(code, shsm_target_read_memory(a, &t, 0x90000000, 4));
    CHECK_EQ_I64(code, SHSM_EXC_GDB);
    CHECK_CZ(g_msg, "stub refused m90000000: E01");
    /* the wire format: one json.dumps line per request */
    AvpJson *args = avp_json_arr(a);
    CATCH(code, shsm_broker_call(a, bc, "nope", args, 0));
    CHECK_EQ_I64(code, SHSM_EXC_GDB);
    CHECK_CZ(g_msg, "unknown method 'nope'");
    avp_json_arr_push(a, args, avp_json_int(a, 1));
    CATCH(code, shsm_broker_call(a, bc, "state", args, 0));
    CHECK_CZ(g_msg, "GdbClient.state() takes 1 positional argument but 2 were given");
    CATCH(code, shsm_broker_call(a, bc, "read_memory", args, 0));
    CHECK_CZ(g_msg, "GdbClient.read_memory() missing 1 required positional argument: 'length'");
    shsm_target_insert_breakpoint(&t, 0x80001018, 0, 4);
    AvpJsonDumpOpts o = {0};
    CHECK_STR(avp_json_dumps(a, shsm_target_call(a, &t, "breakpoints", NULL), &o),
              "[{\"type\": 0, \"address\": 2147487768, \"length\": 4}]");
    shsm_target_clear_breakpoints(&t);
    shsm_broker_client_free(bc);
    ShsmBrokerClient *bad = shsm_broker_client_new(shsm_broker_port(b), "wrong", NULL, 60.0);
    ShsmGdbTarget tb = shsm_gdb_target_broker(bad);
    CATCH(code, shsm_target_state(a, &tb));
    CHECK_EQ_I64(code, SHSM_EXC_GDB);
    CHECK_CZ(g_msg, "bad token");
    shsm_broker_client_free(bad);
    bc = shsm_broker_client_new(shsm_broker_port(b), "tok", NULL, 60.0);
    shsm_broker_client_shutdown(a, bc);
    shsm_broker_client_free(bc);
    avp_thread_join(th);
    CHECK(shsm_broker_stopping(b));
    shsm_broker_free(b);
    shsm_gdb_client_free(gdb);
    shsm_gdbfake_free(f);
}

AVP_TEST(broker_handle_line) {
    AvpArena *a = avp_arena();
    ShsmGdbFake *f = shsm_gdbfake_new();
    ShsmGdbClient *gdb = connect_fake(f, 2.0);
    ShsmBroker *b = shsm_broker_new(gdb, "tok", NULL, 0, NULL, NULL, NULL);
    CHECK_STR(shsm_broker_handle_line(a, b, AVP_S("not json\n")),
              "{\"ok\": false, \"error\": \"bad request\", \"kind\": \"usage\"}\n");
    CHECK_STR(shsm_broker_handle_line(a, b, AVP_S("{\"token\": \"x\"}\n")),
              "{\"ok\": false, \"error\": \"bad token\", \"kind\": \"auth\"}\n");
    CHECK_STR(shsm_broker_handle_line(a, b, AVP_S("{\"token\": \"tok\", \"method\": \"ping\"}")),
              "{\"ok\": true, \"result\": true}\n");
    CHECK_STR(shsm_broker_handle_line(a, b, AVP_S("{\"token\": \"tok\", \"method\": [1, null, {\"a\": 1.5}]}")),
              "{\"ok\": false, \"error\": \"unknown method [1, None, {'a': 1.5}]\", \"kind\": \"usage\"}\n");
    CHECK_STR(shsm_broker_handle_line(a, b, AVP_S("{\"token\": \"tok\", \"method\": \"read_register\", \"args\": [64]}")),
              "{\"ok\": true, \"result\": 2147487744}\n");
    CHECK_STR(shsm_broker_handle_line(a, b, AVP_S("{\"token\": \"tok\", \"method\": \"read_memory\", \"args\": [2147483648, 2]}")),
              "{\"ok\": true, \"result\": {\"__bytes__\": \"0000\"}}\n");
    CHECK_STR(shsm_broker_handle_line(a, b, AVP_S("{\"token\": \"tok\", \"method\": \"write_memory\", "
                                                  "\"args\": [2147483648, {\"__bytes__\": \"zz\"}]}")),
              "{\"ok\": false, \"error\": \"non-hexadecimal number found in fromhex() arg at position 0\", \"kind\": "
              "\"error\"}\n");
    shsm_broker_free(b);
    shsm_gdb_client_free(gdb);
    shsm_gdbfake_free(f);
}

AVP_TEST(direct_target_and_free_port) {
    AvpArena *a = avp_arena();
    ShsmGdbFake *f = shsm_gdbfake_new();
    ShsmGdbClient *c = connect_fake(f, 2.0);
    ShsmGdbTarget t = shsm_gdb_target_direct(c);
    shsm_target_halt_reason(a, &t);
    AvpJsonDumpOpts o = {0};
    CHECK_STR(avp_json_dumps(a, shsm_target_call(a, &t, "state", NULL), &o),
              "{\"running\": false, \"exited\": false, \"stop\": {\"kind\": \"T\", \"signal\": 5, \"regs\": {\"40\": "
              "2147487744, \"01\": 2147545088}, \"pc\": 2147487744, \"sp\": 2147545088, \"raw\": "
              "\"T0540:80001000;01:8000f000;\"}, \"breakpoints\": [], \"bad_checksums\": 0}");
    i64 nums[2] = {SHSM_GDB_REG_PC, SHSM_GDB_REG_LR};
    u64 out[2];
    shsm_target_read_registers(&t, nums, 2, out);
    CHECK_EQ_U64(out[0], 0x80001000);
    CHECK_EQ_U64(out[1], 0x80001104);
    CHECK(shsm_net_free_port() > 0);
    shsm_gdb_client_free(c);
    shsm_gdbfake_free(f);
}

/* ---- interoperability with the Python broker and client (run by hand, see shsm_gdb.md) ----
 * SHSM_GDB_INTEROP_STUB=<port>: a C broker on a GDB stub at that port (token "tok"); prints "PORT <n>" and serves
 * until a client sends shutdown. SHSM_GDB_INTEROP_BROKER=<port>: a C client against a broker (token "tok"); prints
 * each result as one JSON line. Without either variable the test does nothing. */
AVP_TEST(interop_by_hand) {
    AvpArena *a = avp_arena();
    const char *stub = getenv("SHSM_GDB_INTEROP_STUB"), *broker = getenv("SHSM_GDB_INTEROP_BROKER");
    AvpJsonDumpOpts o = {0};
    if (stub) {
        ShsmGdbClient *gdb = shsm_gdb_connect("127.0.0.1", atoi(stub), 2.0, 2.0);
        ShsmBroker *b = shsm_broker_new(gdb, "tok", NULL, 0, NULL, NULL, NULL);
        printf("PORT %d\n", shsm_broker_port(b));
        fflush(stdout);
        shsm_broker_serve(b);
        shsm_broker_free(b);
        shsm_gdb_client_free(gdb);
    }
    if (broker) {
        ShsmBrokerClient *bc = shsm_broker_client_new(atoi(broker), "tok", NULL, 60.0);
        ShsmGdbTarget t = shsm_gdb_target_broker(bc);
        printf("%s\n", avp_json_dumps(a, shsm_gdb_stop_to_json(a, shsm_target_halt_reason(a, &t)), &o).p);
        ShsmBytes m = shsm_target_read_memory(a, &t, 0x80000000, 4);
        printf("%s\n", avp_json_dumps(a, shsm_gdb_json_from_bytes(a, m.p, m.n), &o).p);
        shsm_target_write_memory(&t, 0x80000100, "\xca\xfe", 2);
        m = shsm_target_read_memory(a, &t, 0x80000100, 2);
        printf("%s\n", avp_json_dumps(a, shsm_gdb_json_from_bytes(a, m.p, m.n), &o).p);
        shsm_target_insert_breakpoint(&t, 0x80001018, 0, 4);
        printf("%s\n", avp_json_dumps(a, shsm_target_call(a, &t, "state", NULL), &o).p);
        shsm_target_clear_breakpoints(&t);
        printf("%s\n", avp_json_dumps(a, shsm_gdb_stop_to_json(a, shsm_target_step(a, &t, 2, SHSM_GDB_NONE)), &o).p);
        int code;
        CATCH(code, shsm_target_read_memory(a, &t, 0x90000000, 4));
        printf("error %d %s\n", code, g_msg);
        shsm_broker_client_shutdown(a, bc);
        shsm_broker_client_free(bc);
        fflush(stdout);
    }
}

AVP_TEST_MAIN("shsm_gdb")
