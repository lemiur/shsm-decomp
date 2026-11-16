/* Unit tests of the command-line half of wall (notes, walls.json helpers, proofs, first_error). The
 * expected values are the Python's (tools/wall.py, tools/common/cli.py) on the same inputs. */
#include "avp/test.h"
#include "wall.h"

static const char NOTES_FIXTURE[] =
    "// Parked: header note about fn_80001000, a register swap.\n"
    "#include \"x.h\"\n"
    "\n"
    "// 0x80001000\n"
    "void fn_80001000(void) {\n"
    "    /* Not exact: the compiler hoists\n"
    "     * the load (stack slot). */\n"
    "    int x = 1; // wall: branch order\n"
    "}\n"
    "\n"
    "void fn_80001100(void) {\n"
    "    assert(\"does not match\"); /* ok */\n"
    "} // 0x80001100\n"
    "// MATCH-PENDING: R17 volatile carrier in fn_80001100.\n"
    "// ORIGINAL-ASM does not match\n";

AVP_TEST(find_notes_fixture) {
    AvpArena *a = avp_arena();
    char *dir = avp_mkdtemp(a, "wall_test_", NULL, NULL);
    REQUIRE(dir);
    const char *path = avp_path_join(a, dir, "fixture.c");
    REQUIRE(avp_write_file(path, NOTES_FIXTURE, sizeof NOTES_FIXTURE - 1));
    WallNote *n;
    size_t count = wall_find_notes(a, path, &n);
    REQUIRE(count == 4);
    CHECK_EQ_I64(n[0].line, 1);
    CHECK(!n[0].has_addr);
    CHECK_STR(n[0].text, "Parked: header note about fn_80001000, a register swap.");
    CHECK_EQ_I64(n[1].line, 6);
    CHECK(n[1].has_addr);
    CHECK_EQ_U64(n[1].addr, 0x80001000u);
    CHECK_STR(n[1].text, "Not exact: the compiler hoists the load (stack slot). */");
    CHECK_EQ_I64(n[2].line, 8);
    CHECK_EQ_U64(n[2].addr, 0x80001000u);
    CHECK_STR(n[2].text, "int x = 1; // wall: branch order");
    CHECK_EQ_I64(n[3].line, 14);
    CHECK(!n[3].has_addr);
    CHECK_STR(n[3].text, "MATCH-PENDING: R17 volatile carrier in fn_80001100. ORIGINAL-ASM does not match");
    const char *cls[5];
    CHECK(wall_note_classes(n[0].text, cls) == 1 && strcmp(cls[0], WALL_REGISTER) == 0);
    CHECK(wall_note_classes(n[2].text, cls) == 1 && strcmp(cls[0], WALL_CONTROL) == 0);
    CHECK(wall_note_classes(n[3].text, cls) == 0);
    CHECK(wall_is_rule_note(n[3].text));
    CHECK(!wall_is_rule_note(n[0].text));
    avp_rmtree(dir, 0);
}

AVP_TEST(note_classes_order) {
    const char *cls[5];
    size_t k = wall_note_classes(AVP_S("Swap of a CONSTANT, inlined call, wrong symbol, switch"), cls);
    REQUIRE(k == 5);
    CHECK(strcmp(cls[0], WALL_REFERENT) == 0 && strcmp(cls[1], WALL_CALL) == 0 && strcmp(cls[2], WALL_CONTROL) == 0 &&
          strcmp(cls[3], WALL_OTHER) == 0 && strcmp(cls[4], WALL_REGISTER) == 0);
    CHECK_EQ_I64(wall_note_keyword(AVP_S("x // NOT EXACT here")), 5);
    CHECK_EQ_I64(wall_note_keyword(AVP_S("nonexact")), -1);
}

AVP_TEST(base_name) {
    CHECK_STR(wall_base_name(AVP_S("fn_801A73A8__12PlaneTriggerFv")), "fn_801A73A8");
    CHECK_STR(wall_base_name(AVP_S("__EXI2Imm")), "__EXI2Imm");
    CHECK_STR(wall_base_name(AVP_S("a__b")), "a");
}

AVP_TEST(first_error) {
    AvpArena *a = avp_arena();
    AvpStr out = AVP_S("### mwcceppc.exe Usage Warning:\n"
                       "#   The option -multibyte is deprecated\n"
                       "### mwcceppc.exe Compiler:\n"
                       "#    File: src\\game\\Foo.cpp\n"
                       "# ---------------------------\n"
                       "#      12:   int x = y;\n"
                       "#   Error:                ^\n"
                       "#   (10139) undefined identifier 'y'\n");
    CHECK_STR(wall_first_error(a, out), "Foo.cpp:12: (10139) undefined identifier 'y'");
    CHECK_STR(wall_first_error(a, AVP_S("### mwcceppc.exe Linker:\n#   Error:\n#   undefined: foo\n")),
              "mwcceppc.exe: undefined: foo");
    CHECK(wall_first_error(a, AVP_S("plain output\n")).p == NULL);
}

static AvpJson *obj1(AvpArena *a, const char *k, AvpJson *v) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, avp_str(k), v);
    return o;
}

AVP_TEST(wants_and_apply_proof) {
    AvpArena *a = avp_arena();
    AvpJson *d = obj1(a, "verdict", avp_json_strz(a, "EQUIVALENT-LOOKING: x"));
    CHECK(wall_wants_proof(d, false));
    avp_json_set(a, d, AVP_S("exact"), avp_json_bool(true));
    CHECK(!wall_wants_proof(d, true));
    CHECK(!wall_wants_proof(obj1(a, "verdict", avp_json_strz(a, "WALL other: x")), false));
    CHECK(wall_wants_proof(obj1(a, "verdict", avp_json_strz(a, "WALL other: x")), true));

    AvpJsonDumpOpts o = AVP_JSON_COMPACT;
    d = obj1(a, "verdict", avp_json_strz(a, "EQUIVALENT-LOOKING: rows agree"));
    AvpJsonError e;
    AvpJson *p = avp_json_parse(a, AVP_S("{\"verdict\": \"EQUIVALENT\", \"reason\": \"\", \"line\": \"x\"}"), &e);
    wall_apply_proof(a, d, p);
    CHECK_STR(avp_json_dumps(a, d, &o),
              "{\"verdict\":\"EQUIVALENT (proved; eqcheck)\",\"proof\":{\"verdict\":\"EQUIVALENT\",\"reason\":\"\","
              "\"line\":\"x\"},\"looking\":\"EQUIVALENT-LOOKING: rows agree\"}");
    CHECK(wall_proof_line(a, d).p == NULL);

    d = obj1(a, "verdict", avp_json_strz(a, "WALL other: q"));
    p = avp_json_parse(a, AVP_S("{\"verdict\": \"NOT PROVEN\", \"reason\": \"\", \"line\": \"NOT PROVEN\"}"), &e);
    wall_apply_proof(a, d, p);
    CHECK_STR(avp_json_dumps(a, d, &o),
              "{\"verdict\":\"WALL other: q\",\"proof\":{\"verdict\":\"NOT PROVEN\",\"reason\":\"\",\"line\":\"NOT "
              "PROVEN\"}}");
    CHECK_STR(wall_proof_line(a, d), "eqcheck: NOT PROVEN");
}

AVP_TEST(py_values) {
    AvpArena *a = avp_arena();
    AvpJsonError e;
    AvpJson *v = avp_json_parse(a, AVP_S("[1, 2.0, \"a'b\", null, true, {\"k\": [0.1]}]"), &e);
    CHECK_STR(wall_py_repr(a, v), "[1, 2.0, \"a'b\", None, True, {'k': [0.1]}]");
    CHECK_STR(wall_py_str(a, avp_json_at(v, 2)), "a'b");
    CHECK_STR(wall_py_hex(a, -16), "-0x10");
    CHECK(wall_py_eq(avp_json_at(v, 0), avp_json_at(v, 4)));
}

AVP_TEST_MAIN("wall")
