/* The dolphin tests' fixture checkout: tools/common/_fixture.py make_checkout (the files the dolphin tool reads)
 * plus test_dolphin.py's classes.json and write_dol, built at run time under build/tmp/dolphin-tests/<name>. */
#pragma once
#include "avp/avp.h"
#include "avp/test.h"
#include "dolphin.h"
#include "shsm_cli.h"
#include "shsm_gdbfake.h"

#include <string.h>

#define FX_VTABLE 0x80002100u
#define FX_OBJ 0x80008000u

static const char FX_CONFIGURE[] =
    "\nDEFAULT_VERSION = 0\nVERSIONS = [\n    \"TESTV1\",  # 0: test\n]\n\nconfig.libs = [\n    {\n        \"lib\": \"game\",\n"
    "        \"objects\": [\n            Object(Matching, \"game/Alpha.cpp\"),\n            Object(\n                NonMatching,\n"
    "                \"game/Beta.c\",\n                extra_cflags=[\"-O0\"],\n            ),\n"
    "            Object(Equivalent, \"game/Gamma.c\"),\n        ],\n    },\n]\n";

static const char FX_SYMBOLS[] =
    "fn_80001000__Fv = .text:0x80001000; // type:function size:0x10 scope:global\n"
    "helper = .text:0x80001010; // type:function size:0x8 scope:local align:4\n"
    "fn_80001018__8unk_RampFf = .text:0x80001018; // type:function size:0xC\n"
    "jumper = .text:0x80001100; // type:function size:0x8 scope:global\n"
    "lbl_80002000 = .data:0x80002000; // type:object size:0x6 scope:global data:string\n"
    "lbl_80002008 = .data:0x80002008; // type:object size:0xC scope:global\n"
    "lbl_80003000 = .sdata2:0x80003000; // type:object size:0x4 scope:local data:float\n"
    "lbl_80003008 = .sdata2:0x80003008; // type:object size:0x8 scope:local data:double hidden\n"
    "start_label = .text:0x80001000; // type:label scope:global\n";

static const char FX_SPLITS[] =
    "Sections:\n\t.text       type:code align:8\n\t.data       type:data align:32\n\n"
    "game/Alpha.cpp:\n\t.text       start:0x80001000 end:0x80001024\n\t.data       start:0x80002000 end:0x80002014\n"
    "\t.sdata2     start:0x80003000 end:0x80003010\n\n"
    "game/Beta.c:\n\t.text       start:0x80001100 end:0x80001108\n";

static const char FX_ALPHA_S[] =
    ".include \"macros.inc\"\n.file \"Alpha.cpp\"\n\n# 0x80001000..0x80001024 | size: 0x24\n.text\n.balign 4\n\n"
    "# .text:0x0 | 0x80001000 | size: 0x10\n# fn_80001000()\n.fn fn_80001000__Fv, global\n.sym start_label, global\n"
    "/* 80001000 00000100  3C 60 80 00 */\tlis r3, lbl_80002000@ha\n"
    "/* 80001004 00000104  38 63 20 00 */\taddi r3, r3, lbl_80002000@l\n"
    "/* 80001008 00000108  C0 22 80 00 */\tlfs f1, lbl_80003000@sda21(r0)\n"
    "/* 8000100C 0000010C  48 00 00 05 */\tbl helper\n.endfn fn_80001000__Fv\n\n"
    "# .text:0x10 | 0x80001010 | size: 0x8\n.fn helper, local\n/* 80001010 00000110  2C 03 00 00 */\tcmpwi r3, 0x0\n"
    ".L_80001014:\n/* 80001014 00000114  41 82 FF FC */\tbeq .L_80001014\n.endfn helper\n\n"
    "# .text:0x18 | 0x80001018 | size: 0xC\n# unk_Ramp::fn_80001018(float)\n.fn fn_80001018__8unk_RampFf, global\n"
    "/* 80001018 00000118  4B FF FF F9 */\tbl helper\n/* 8000101C 0000011C  C8 22 80 08 */\tlfd f1, lbl_80003008@sda21(r0)\n"
    "/* 80001020 00000120  4B FF FF E0 */\tb fn_80001000__Fv\n.endfn fn_80001018__8unk_RampFf\n\n"
    "# 0x80002000..0x80002014 | size: 0x14\n.data\n.balign 8\n\n# .data:0x0 | 0x80002000 | size: 0x6\n"
    ".obj lbl_80002000, global\n\t.string \"a\\\"b\\n\"\n.endobj lbl_80002000\n\n"
    "# .data:0x8 | 0x80002008 | size: 0xC\n.obj lbl_80002008, global\n\t.4byte fn_80001000__Fv\n"
    "\t.4byte lbl_80002000+0x2\n\t.2byte 0x1234\n\t.byte 0x01, 0x02\n.endobj lbl_80002008\n\n"
    "# 0x80003000..0x80003010 | size: 0x10\n.section .sdata2, \"a\"\n.balign 8\n\n"
    "# .sdata2:0x0 | 0x80003000 | size: 0x4\n.obj lbl_80003000, local\n\t.float 1.5\n.endobj lbl_80003000\n\n"
    "# .sdata2:0x8 | 0x80003008 | size: 0x8\n.obj lbl_80003008, local\n.hidden lbl_80003008\n\t.double 0.25\n"
    ".endobj lbl_80003008\n";

static const char FX_BETA_S[] =
    ".include \"macros.inc\"\n.file \"Beta.c\"\n\n# 0x80001100..0x80001108 | size: 0x8\n.text\n.balign 4\n\n"
    "# .text:0x0 | 0x80001100 | size: 0x8\n.fn jumper, global\n/* 80001100 00000200  4B FF FF 01 */\tbl fn_80001000__Fv\n"
    "/* 80001104 00000204  4B FF FF 14 */\tb helper\n.endfn jumper\n\n"
    "# 0x80004000..0x80004004 | size: 0x4\n.section .ctors, \"a\"\n.balign 4\n\t.4byte jumper\n";

static const char FX_CLASSES[] =
    "{\"classes\": [{\"key\": \"vt:lbl_80002100\", \"name\": \"unk_Thing\", \"name_source\": \"test\", "
    "\"vtable_symbol\": \"lbl_80002100\", \"vtable\": {\"address\": \"0x80002100\", \"groups\": [{\"addend\": \"0x0\"}]}, "
    "\"fields\": [{\"offset\": \"0x0\", \"width\": 4, \"kind\": \"int\", \"type\": \"vptr\"}, "
    "{\"offset\": \"0x4\", \"width\": 4, \"kind\": \"float\", \"type\": \"f32\"}, "
    "{\"offset\": \"0x8\", \"width\": 4, \"kind\": \"int\", \"type\": \"void*\"}], \"size_estimate\": 12}]}";

static inline void fx_write_bytes(const char *root, const char *rel, const void *p, size_t n) {
    AvpArena *a = avp_arena();
    const char *path = avp_path_join(a, root, rel);
    avp_makedirs(avp_cz(a, avp_path_dirname(path)), true);
    AVP_ASSERT(avp_write_file(path, p, n));
}
static inline void fx_put(const char *root, const char *rel, const char *text) { fx_write_bytes(root, rel, text, strlen(text)); }

/* fixture_root(): make_checkout plus classes.json */
static inline const char *fx_root(const char *name) {
    AvpArena *a = avp_arena();
    const char *d = avp_path_abspath(a, avp_join(a, "build", "tmp", "dolphin-tests", name));
    if (avp_isdir(d)) avp_rmtree(d, 0);
    AVP_ASSERT(avp_makedirs(d, true));
    fx_put(d, "configure.py", FX_CONFIGURE);
    fx_put(d, "tools/.keep", "");
    fx_put(d, "config/TESTV1/config.yml", "object_base: orig/TESTV1\nobject: sys/main.dol\n");
    fx_put(d, "config/TESTV1/symbols.txt", FX_SYMBOLS);
    fx_put(d, "config/TESTV1/splits.txt", FX_SPLITS);
    fx_put(d, "build/TESTV1/asm/game/Alpha.s", FX_ALPHA_S);
    fx_put(d, "build/TESTV1/asm/game/Beta.s", FX_BETA_S);
    fx_put(d, "build/TESTV1/classes.json", FX_CLASSES);
    return d;
}

static inline void fx_be32(u8 *p, u32 v) {
    p[0] = (u8)(v >> 24), p[1] = (u8)(v >> 16), p[2] = (u8)(v >> 8), p[3] = (u8)v;
}

/* write_dol(root, image): a one-section DOL, .text at 0x80001000 */
static inline void fx_write_dol(const char *root, const u8 *image, size_t n) {
    u8 *buf = (u8 *)avp_alloc(avp_arena(), 0x100 + n);
    fx_be32(buf, 0x100);
    fx_be32(buf + 0x48, 0x80001000u);
    fx_be32(buf + 0x90, (u32)n);
    memcpy(buf + 0x100, image, n);
    fx_write_bytes(root, "orig/TESTV1/sys/main.dol", buf, 0x100 + n);
}

/* env_with(stub): a connected client (halt_reason done) injected into a fresh Env */
typedef struct {
    ShsmGdbFake *stub;
    ShsmGdbClient *client;
    ShsmGdbTarget target;
    DolphinEnv *env;
} FxLive;

static inline FxLive fx_live(const char *root, ShsmGdbFake *stub) {
    FxLive l = {stub, NULL, {0}, NULL};
    shsm_gdbfake_start(stub);
    l.client = shsm_gdb_connect("127.0.0.1", stub->port, 2.0, 2.0);
    shsm_gdb_halt_reason(avp_arena(), l.client);
    l.target = shsm_gdb_target_direct(l.client);
    l.env = dolphin_env_new(root);
    l.env->target = &l.target;
    return l;
}

static inline void fx_live_free(FxLive *l) {
    dolphin_env_close(l->env);
    shsm_gdb_client_free(l->client);
    shsm_gdbfake_free(l->stub);
}

/* Expects stmt to raise exc (any message containing `part`, or any message when NULL). */
#define FX_RAISES(stmt, exc, part)                                                                              \
    do {                                                                                                       \
        AvpTry t_;                                                                                             \
        if (AVP_TRY(&t_)) {                                                                                    \
            stmt;                                                                                              \
            avp_try_end(&t_);                                                                                  \
            avp_test__fail(__FILE__, __LINE__, "%s did not raise", #stmt);                                     \
        } else if (t_.code != (exc) || ((part) && !strstr(t_.msg, (part) ? (part) : ""))) {                    \
            avp_test__fail(__FILE__, __LINE__, "%s raised %d: %s", #stmt, t_.code, t_.msg);                    \
        }                                                                                                      \
    } while (0)

/* call an op with JSON args given as text */
static inline DolphinResult *fx_call(DolphinEnv *env, const char *op, const char *json) {
    AvpJsonError err;
    AvpJson *args = json ? avp_json_parse(avp_arena(), avp_str(json), &err) : NULL;
    AVP_ASSERT(!json || args);
    return dolphin_call(env, op, args);
}
