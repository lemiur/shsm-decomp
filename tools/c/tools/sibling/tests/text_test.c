/* sibling: the link-map parser and the dtk config transforms, on synthetic text. */
#include "avp/test.h"
#include "sibling.h"

#include <string.h>

static const char MAP[] =
    ".init section layout\n"
    "  00000000 000044 80004000 00000100  4 __init_hardware \tos.a __start.o\n"
    "\n"
    ".text section layout\n"
    "  Starting        Virtual  File\n"
    "  address  Size   address  offset\n"
    "  ---------------------------------\n"
    "  00000000 00da90 80008140 00004320  1 .text \tAiDebug.obj \n"
    "  00000000 000044 80008140 00004320  4 FindBone__15StateTransitionFP16RpHAnimHierarchyi \tAiDebug.obj \n"
    "  00000044 000004 80008184 00004364  4 Print__2AIFPQ210Characters19CCharacterBehaviour \tAiDebug.obj \n"
    "  UNUSED   000060 ........ ........    FMOD_Memory_GetStats fmodex.a fmod_memory.obj\n"
    "  UNUSED   000004 ........ ........    jpeg_abort_decompress jdapimin.obj \n"
    "  00346bc0 0003e0 8034ed00 0034aee0  4 RtQuatRotate \tRtQuat.a rtquat.obj\n"
    "  00346fa0 000134 8034f0e0 0034b2c0  4 RtQuatConvertFromMatrix \tRtQuat.a rtquat.obj\n"
    "  00348a66 00011b 00348a66 00916aad  1 .dwarf.RtQuatRotate \tRtQuat.a rtquat.obj\n"
    "  002a8e44 000000 802b0f84 002ad164    TRKAccessFile (entry of .text) \tTRK.a D:\\Data\\trk\\targsupp.o\n"
    "  002a8e48 000010 802b0f88 002ad168  4 __ct__Q23std11vector<int>Fv \tMSL_C++.PPCEABI.bare.H.a X:\\Products\\My Dir\\ios.o\n"
    "  00349408 000008 80351550 0034d730 16 *fill*\n"
    "\n"
    ".ctors section layout\n"
    "  00000000 000004 80505464 00501644  4 __sinit_\\AiDebug_cpp \tAiDebug.obj \n";

AVP_TEST(map_text_layout) {
    AvpArena *a = avp_arena();
    SibMap *m = sibling_parse_map(a, avp_str(MAP));
    REQUIRE(m->text.n == 5);
    CHECK_CZ(m->text.v[0].name, "FindBone__15StateTransitionFP16RpHAnimHierarchyi");
    CHECK_CZ(m->text.v[0].lib, "");
    CHECK_CZ(m->text.v[0].object, "AiDebug.obj");
    CHECK(m->text.v[0].address == 0x80008140 && m->text.v[0].size == 0x44 && m->text.v[0].order == 0);
    CHECK(m->text.v[1].order == 1);
    CHECK_CZ(m->text.v[2].name, "RtQuatRotate");
    CHECK_CZ(m->text.v[2].lib, "RtQuat.a");
    CHECK_CZ(m->text.v[2].object, "rtquat.obj");
    CHECK(m->text.v[2].size == 0x3E0 && m->text.v[2].order == 0 && m->text.v[3].order == 1);
    /* a template name and an object path with a space */
    CHECK_CZ(m->text.v[4].name, "__ct__Q23std11vector<int>Fv");
    CHECK_CZ(m->text.v[4].lib, "MSL_C++.PPCEABI.bare.H.a");
    CHECK_CZ(m->text.v[4].object, "X:\\Products\\My Dir\\ios.o");
}

AVP_TEST(map_unused) {
    AvpArena *a = avp_arena();
    SibMap *m = sibling_parse_map(a, avp_str(MAP));
    REQUIRE(m->unused.n == 2);
    CHECK_CZ(m->unused.v[0].name, "FMOD_Memory_GetStats");
    CHECK_CZ(m->unused.v[0].lib, "fmodex.a");
    CHECK_CZ(m->unused.v[0].object, "fmod_memory.obj");
    CHECK(m->unused.v[0].size == 0x60);
    CHECK_CZ(m->unused.v[1].lib, "");
    CHECK_CZ(m->unused.v[1].object, "jdapimin.obj");
}

static const char *DATA_MAP =
    ".text section layout\n"
    "  Starting        Virtual\n"
    "  address  Size   address\n"
    "  -----------------------\n"
    "  00000000 000008 80003000 00001000  4 helper__Fv \tfmodex.a a.obj\n"
    "\n"
    ".sbss section layout\n"
    "  Starting        Virtual\n"
    "  address  Size   address\n"
    "  -----------------------\n"
    "  00005e98 000002 80755058 005c7098  2 _RwDlTokenCurrent \tRwCore.a dltoken.obj\n"
    "  00189fc0 000058 8074efc0 0074b1c0  1 .sbss \tfmod_event.a fmod_soundbank.obj\n"
    "  00189fc0 00002c 8074efc0 0074b1c0  4 gQueue__Q24FMOD9SoundBank \tfmod_event.a fmod_soundbank.obj\n"
    "  00189fec 000010 8074efec 0074b1ec  4 other \tmain.obj\n"
    "\n"
    ".debug_info section layout\n"
    "  003eeb28 000027 003eeb28 009bcb6f  1 .dwarf.gQueue__Q24FMOD9SoundBank \tfmod_event.a fmod_soundbank.obj\n";

AVP_TEST(map_data_symbols) {
    AvpArena *a = avp_arena();
    SibMapData d;
    CHECK(sibling_map_find(a, avp_str(DATA_MAP), "_RwDlTokenCurrent", 0, &d));
    CHECK_CZ(d.section, ".sbss");
    CHECK(d.address == 0x80755058 && d.size == 2 && d.align == 2);
    CHECK_CZ(d.lib, "RwCore.a");
    CHECK_CZ(d.object, "dltoken.obj");
    /* an address inside a symbol; the object's own ".sbss" line is not a symbol */
    CHECK(sibling_map_find(a, avp_str(DATA_MAP), NULL, 0x8074EFD0, &d));
    CHECK_CZ(d.name, "gQueue__Q24FMOD9SoundBank");
    CHECK(d.address == 0x8074EFC0 && d.size == 0x2C);
    CHECK(sibling_map_find(a, avp_str(DATA_MAP), "other", 0, &d));
    CHECK_CZ(d.lib, "");
    CHECK_CZ(d.object, "main.obj");
    CHECK(!sibling_map_find(a, avp_str(DATA_MAP), "helper__Fv", 0, &d));  /* .text is not data */
    CHECK(!sibling_map_find(a, avp_str(DATA_MAP), "gQueue__Q24FMOD9SoundBank", 0x1, &d) == false); /* by name wins */
    CHECK(!sibling_map_find(a, avp_str(DATA_MAP), NULL, 0x80000001, &d));
}

AVP_TEST(split_owner) {
    AvpArena *a = avp_arena();
    const char *lib, *obj;
    sibling_split_owner(a, avp_str(" a.a b c.o "), &lib, &obj);
    CHECK_CZ(lib, "a.a");
    CHECK_CZ(obj, "b c.o");
    sibling_split_owner(a, avp_str("x.obj"), &lib, &obj);
    CHECK_CZ(lib, "");
    CHECK_CZ(obj, "x.obj");
}

AVP_TEST(fix_splits) {
    AvpArena *a = avp_arena();
    const char *in = "Sections:\n"
                     "\t.init       type:code align:32\n"
                     "\t.text       type:code align:32\n"
                     "\n"
                     "mainloop.cpp:\n"
                     "\t.text       start:0x80008140 end:0x80008200\n"
                     "\t.data       start:0x80519B40 end:0x8051A6E8\n"
                     "\t.sdata2     start:0x807595C0 end:0x807595C0\n"
                     "\n"
                     "mainloop.c:\n"
                     "\t.text       start:0x80008200 end:0x80008200\n"
                     "\n"
                     "D:\\Data\\trk\\targsupp.s:\n"
                     "\t.text       start:0x80008200 end:0x80009000\n";
    AvpStr out = sibling_fix_splits(a, avp_str(in), 0x80008F00);
    const char *o = avp_cz(a, out);
    CHECK(strstr(o, "Sections:\n\t.init       type:code align:32\n\t.text       type:code align:32\n") != NULL);
    CHECK(strstr(o, "mainloop.cpp:\n\t.text       start:0x80008140 end:0x80008200\n") != NULL);
    CHECK(strstr(o, ".data") == NULL);
    CHECK(strstr(o, "mainloop_1.c:\n") != NULL);  /* collides with mainloop.cpp in dtk's obj/ */
    CHECK(strstr(o, "start:0x80008200 end:0x80008200") == NULL); /* empty range dropped */
    CHECK(strstr(o, "D_/Data/trk/targsupp.s:\n\t.text       start:0x80008200 end:0x80008F00\n") != NULL);
}

AVP_TEST(fix_symbols) {
    AvpArena *a = avp_arena();
    const char *in = "memset = .init:0x80005FE8; // type:function size:0x30 scope:global\n"
                     "gTRKInterruptVectorTable = .init:0x80004000; // type:label scope:global\n"
                     "...rodata.0 = .rodata:0x80513E30; // type:label scope:local\n"
                     "@17009 = .data:0x80519B40; // type:object size:0xD scope:local\n"
                     "_eti_init_info = .rodata:0x80505400; // type:object size:0x20 scope:global\n"
                     "Foo = .text:0x80008140; // type:function size:0x44 scope:global\n";
    const char *o = avp_cz(a, sibling_fix_symbols(a, avp_str(in)));
    CHECK(strstr(o, "memset = .init:0x80005FE8;") != NULL);
    CHECK(strstr(o, "gTRKInterruptVectorTable") == NULL);
    CHECK(strstr(o, "...rodata.0") == NULL);
    CHECK(strstr(o, "@17009_a80519B40 = .data:0x80519B40;") != NULL);
    CHECK(strstr(o, "_eti_init_info = .rodata:0x80505400;") != NULL);
    CHECK(strstr(o, "Foo = .text:0x80008140;") != NULL);
}

AVP_TEST(placeholders) {
    CHECK(sibling_is_placeholder("fn_80006860__8unk_RampFfff"));
    CHECK(sibling_is_placeholder("__ct__12unk_801A92A8Fv"));
    CHECK(sibling_is_placeholder("lbl_80001000"));
    CHECK(!sibling_is_placeholder("RtQuatRotate"));
    CHECK(!sibling_is_placeholder("__sinit_\\PadRumble_cpp"));
    AvpArena *a = avp_arena();
    CHECK_CZ(sibling_name_agreement(a, "RtQuatRotate", "RtQuatRotate"), "same");
    CHECK_CZ(sibling_name_agreement(a, "TRKNubMainLoop", "TRK_NubMainLoop"), "differ");
}

AVP_TEST(initialiser_names) {
    CHECK(sibling_is_initialiser("__sinit_\\PadRumble_cpp"));
    CHECK(sibling_is_initialiser("sinit_80001234_text"));
    CHECK(!sibling_is_initialiser("fn_80001234"));
    CHECK(!sibling_is_initialiser("sinfo__Fv"));
    CHECK(!sibling_is_initialiser(NULL));
}

AVP_TEST(elf_text_end_rejects_garbage) {
    CHECK(sibling_elf_text_end(avp_str("not an elf at all, just text that is long enough........")) == 0);
}

AVP_TEST_MAIN("sibling")
