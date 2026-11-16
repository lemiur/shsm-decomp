/* shsm_pe: the port of tools/mwcc_trace/test_symbols.py (known Wii/1.0 compiler addresses; skipped without the
 * compiler or under SHSM_NO_REAL_TREE), plus the pure pieces (_strip, version_of, the error paths of parse). Run
 * from the repository root. */
#include <stdlib.h>
#include <string.h>

#include "avp/test.h"
#include "shsm_pe.h"

#define EXE "tools/compilers/Wii/1.0/mwcceppc.exe"

static const ShsmPeSymbols *table(void) {
    static const ShsmPeSymbols *t;
    static bool tried;
    if (!tried) {
        tried = true;
        const char *nr = getenv("SHSM_NO_REAL_TREE");
        if ((nr && *nr && strcmp(nr, "0") != 0) || !avp_isfile(EXE)) {
            printf("skipped: SHSM_NO_REAL_TREE is set or " EXE " is absent\n");
            return NULL;
        }
        t = shsm_pe_symbols_load(avp_arena(), EXE);
    }
    return t;
}

AVP_TEST(known_functions) {
    const ShsmPeSymbols *t = table();
    if (!t) return;
    AvpArena *a = avp_arena();
    static const struct {
        i64 va;
        const char *name, *module;
    } known[] = {{0x5b88f0, "colorinstructions", "Coloring.obj"},
                 {0x5bb420, "scheduleinstructions", "Scheduler.obj"},
                 {0x63d720, "buildinterferencegraph", "InterferenceGraph.obj"},
                 {0x63e9d0, "insertspillcode", "Spill.obj"},
                 {0x592bf0, "pickcolor", "RegisterInfo.obj"}};
    for (size_t i = 0; i < sizeof known / sizeof *known; i++) {
        const ShsmPeFunc *f = shsm_pe_symbols_lookup(t, known[i].va);
        REQUIRE(f);
        CHECK_EQ_I64(f->va, known[i].va);
        CHECK_STR(f->name, known[i].name);
        CHECK_STR(f->module, known[i].module);
        CHECK_STR(shsm_pe_symbols_describe(a, t, known[i].va), avp_sprintf(a, "%s (%s)", known[i].name, known[i].module));
        const ShsmPeFunc **hits;
        REQUIRE(shsm_pe_symbols_find(a, t, avp_str(known[i].name), &hits) >= 1);
        CHECK_EQ_I64(hits[0]->va, known[i].va);
    }
}

AVP_TEST(offset_and_static) {
    const ShsmPeSymbols *t = table();
    if (!t) return;
    AvpArena *a = avp_arena();
    CHECK_STR(shsm_pe_symbols_describe(a, t, 0x5b88f4), "colorinstructions+0x4 (Coloring.obj)");
    CHECK(avp_str_startswith(shsm_pe_symbols_describe(a, t, 0x5b8e90), AVP_S("static in Coloring.obj")));
}

AVP_TEST(count) {
    const ShsmPeSymbols *t = table();
    if (!t) return;
    CHECK_EQ_U64(t->nfuncs, 3680);
}

AVP_TEST(no_debug_info_fails_clearly) {
    /* b'MZ' + b'\0' * 0x3E + b'\x80\0\0\0' + b'\0' * 0x40 + b'junk' */
    u8 d[2 + 0x3E + 4 + 0x40 + 4] = {'M', 'Z'};
    memcpy(d + 0x40, "\x80\0\0\0", 4);
    memcpy(d + 0x84, "junk", 4);
    AvpTry t;
    if (AVP_TRY(&t)) {
        shsm_pe_symbols_parse(avp_arena(), d, sizeof d);
        avp_try_end(&t);
        CHECK_MSG(false, "no exception");
    } else {
        CHECK_EQ_I64(t.code, SHSM_PE_EXC_NO_DEBUG_INFO);
        CHECK_CZ(t.msg, "not a PE file");
    }
}

AVP_TEST(short_file_is_a_struct_error) {
    u8 d[16] = {'M', 'Z'};
    AvpTry t;
    if (AVP_TRY(&t)) {
        shsm_pe_symbols_parse(avp_arena(), d, sizeof d);
        avp_try_end(&t);
        CHECK_MSG(false, "no exception");
    } else {
        CHECK_EQ_I64(t.code, SHSM_PE_EXC_STRUCT);
        CHECK_CZ(t.msg, "unpack_from requires a buffer of at least 64 bytes for unpacking 4 bytes at offset 60 "
                        "(actual buffer size is 16)");
    }
}

AVP_TEST(missing_file_is_an_os_error) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        shsm_pe_symbols_load(avp_arena(), "build/tmp/no such file.exe");
        avp_try_end(&t);
        CHECK_MSG(false, "no exception");
    } else {
        CHECK_EQ_I64(t.code, AVP_ERR_IO);
        CHECK_CZ(t.msg, "[Errno 2] No such file or directory: 'build/tmp/no such file.exe'");
    }
}

AVP_TEST(strip_undoes_x86_decoration) {
    CHECK_STR(shsm_pe_strip(AVP_S("_main")), "main");
    CHECK_STR(shsm_pe_strip(AVP_S("_WinMain@16")), "WinMain");
    CHECK_STR(shsm_pe_strip(AVP_S("f@x@12")), "f@x");
    CHECK_STR(shsm_pe_strip(AVP_S("a@1@2")), "a@1");
    CHECK_STR(shsm_pe_strip(AVP_S("?f@@YAXXZ")), "?f@@YAXXZ");
    CHECK_STR(shsm_pe_strip(AVP_S("__x@")), "_x@");
    CHECK_STR(shsm_pe_strip(AVP_S("@12")), "");
    CHECK_STR(shsm_pe_strip(AVP_S("a@12\n")), "a");
    CHECK_STR(shsm_pe_strip(AVP_S("a\nb@12")), "a\nb@12");
    CHECK_STR(shsm_pe_strip(AVP_S("_")), "");
}

AVP_TEST(version_of_names_the_compiler_directory) {
    AvpArena *a = avp_arena();
    CHECK_CZ(shsm_pe_version_of(a, "X:\\r", "X:\\r\\tools\\compilers\\Wii\\1.0\\mwcceppc.exe"), "Wii-1.0");
    CHECK_CZ(shsm_pe_version_of(a, "X:\\r", "X:\\r\\tools\\compilers\\mwcceppc.exe"), ".");
    CHECK_CZ(shsm_pe_version_of(a, "X:\\r", "X:\\other\\GC\\mwcceppc.exe"), "GC");
    CHECK_CZ(shsm_pe_version_of(a, "X:\\r", "X:\\mwcceppc.exe"), "compiler");
}

AVP_TEST_MAIN("shsm_pe")
