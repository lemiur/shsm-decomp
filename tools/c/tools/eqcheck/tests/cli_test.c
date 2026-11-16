/* Unit tests of eqcheck's command line (eq_cli.c): _ret_regs and _reason_key. The expected values are the Python's
 * (tools/eqcheck.py) on the same inputs. */
#include "avp/test.h"
#include "eq_cli.h"
#include "shsm_cli.h"

static AvpStr reason_key(AvpArena *a, const char *json) {
    AvpJsonError err;
    AvpJson *f = avp_json_parse(a, avp_str(json), &err);
    AVP_ASSERT(f);
    return eq_reason_key(a, f);
}

AVP_TEST(reason_key) {
    AvpArena *a = avp_arena();
    CHECK_STR(reason_key(a, "{\"unmodelled\": \"bctr  at row 4\", \"reason\": \"x\"}"), "unmodelled: bctr");
    CHECK_STR(reason_key(a, "{\"unmodelled\": null, \"reason\": \"block 12: call 1 (bl fn_80028D0C__FiPCce): "
                            "argument r7 at 0x8052A574 [retail v25.r7 | compiled v25.r7]\"}"),
              "block N: call N (bl fn_80028D0C__FiPCce): argument r7 at N");
    CHECK_STR(reason_key(a, "{\"unmodelled\": \"\", \"reason\": \"a frame address is stored to memory ((-0x10 + in.r1 "
                            "+ and((ld4[0x80000000]*-1), 31)))\"}"),
              "a frame address is stored to memory ((-N + in.r1 + and((ld4[N]*-N), N)))");
    char longr[160];
    memset(longr, 'x', 120);
    memcpy(longr + 120, "1", 2);
    AvpStr k = reason_key(a, avp_sprintf(a, "{\"reason\": \"%s\"}", longr));
    CHECK_EQ_U64(k.n, 90);
    CHECK_STR(reason_key(a, "{\"reason\": \"ab12 12ab 3.5 _7 r7\"}"), "ab12 12ab N.N _7 r7");
}

AVP_TEST(ret_regs) {
    AvpArena *a = avp_arena();
    CHECK_EQ_I64(eq_ret_regs(a, NULL).n, -1);
    CHECK_EQ_I64(eq_ret_regs(a, "").n, -1);
    CHECK_EQ_I64(eq_ret_regs(a, "auto").n, -1);
    CHECK_EQ_I64(eq_ret_regs(a, "void").n, 0);
    CHECK_EQ_I64(eq_ret_regs(a, ",").n, 0);
    EqRet r = eq_ret_regs(a, " r3 , f1 ");
    REQUIRE(r.n == 2);
    CHECK_STR(avp_str(r.names[0]), "r3");
    CHECK_STR(avp_str(r.names[1]), "f1");
    CHECK_EQ_I64(eq_ret_regs(a, "r3,r3").n, 2);
    AvpTry t;
    if (AVP_TRY(&t)) {
        eq_ret_regs(a, "R3");
        avp_try_end(&t);
        CHECK(false);
    } else {
        CHECK_EQ_I64(t.code, SHSM_EXC_USAGE);
        CHECK_STR(avp_str(t.msg), "--ret: unknown register R3");
    }
}

AVP_TEST_MAIN("eqcheck")
