/* The input stamp and the scan driver's checks on synthetic asm trees: input_stamp, stamp_key, stale_reason,
 * check_inputs, the scanner digest, the content key, _current and the classes.json text. */
#include "avp/test.h"
#include "classes.h"
#include "shsm_cli.h"

#include <string.h>

typedef struct {
    char *dir, *asm_dir, *symbols, *splits;
} Tree;

static void put(const char *path, const char *text) { AVP_ASSERT(avp_write_file(path, text, strlen(text))); }

static Tree tree(void) {
    AvpArena *a = avp_arena();
    Tree t;
    t.dir = avp_mkdtemp(a, "classes_stamp", NULL, NULL);
    AVP_ASSERT(t.dir != NULL);
    t.asm_dir = avp_path_join(a, t.dir, "asm");
    AVP_ASSERT(avp_makedirs(avp_path_join(a, t.asm_dir, "game"), true));
    put(avp_path_join(a, t.asm_dir, "a.s"), ".fn a\n.endfn a\n");
    put(avp_join(a, t.asm_dir, "game", "b.s"), ".fn b, global\n.endfn b\n");
    put(avp_join(a, t.asm_dir, "game", "notes.txt"), "not asm\n");
    t.symbols = avp_path_join(a, t.dir, "symbols.txt");
    put(t.symbols, "a = .text:0x80000000; // type:function size:0x4\n");
    t.splits = avp_path_join(a, t.dir, "splits.txt");
    put(t.splits, "game/b.cpp:\n\t.text start:0x80000004 end:0x80000008\n");
    return t;
}

static ClassesDb *db_with(AvpJson *inputs) {
    AvpArena *a = avp_arena();
    AvpJson *doc = avp_json_obj(a);
    if (inputs) avp_json_set(a, doc, AVP_S("inputs"), inputs);
    avp_json_set(a, doc, AVP_S("classes"), avp_json_arr(a));
    return classes_db_from_json(a, doc);
}

AVP_TEST(input_stamp) {
    AvpArena *a = avp_arena();
    Tree t = tree();
    AvpJson *s = classes_input_stamp(a, t.asm_dir, t.symbols);
    AvpJson *as = avp_json_getz(s, "asm");
    CHECK_EQ_I64(avp_json_get_i64(as, "files", -1), 2);
    CHECK_EQ_I64(avp_json_get_i64(as, "bytes", -1), (i64)(strlen(".fn a\n.endfn a\n") + strlen(".fn b, global\n.endfn b\n")));
    CHECK(avp_json_get_i64(as, "mtime_ns", 0) > 0);
    CHECK_STR(avp_json_get_str(as, "dir", AVP_S("")), avp_path_abspath(a, t.asm_dir));
    AvpJson *sy = avp_json_getz(s, "symbols");
    CHECK_STR(avp_json_get_str(sy, "path", AVP_S("")), avp_path_abspath(a, t.symbols));
    /* member order as the Python dict */
    CHECK_STR(avp_json_member(s, 0)->key, "asm");
    CHECK_STR(avp_json_member(as, 3)->key, "mtime_ns");
    /* a missing asm directory counts nothing; a missing symbols file raises */
    AvpJson *e = classes_input_stamp(a, avp_path_join(a, t.dir, "none"), t.symbols);
    CHECK_EQ_I64(avp_json_get_i64(avp_json_getz(e, "asm"), "files", -1), 0);
    AvpTry tr;
    if (AVP_TRY(&tr)) {
        classes_input_stamp(a, t.asm_dir, avp_path_join(a, t.dir, "none.txt"));
        avp_try_end(&tr);
        CHECK_MSG(false, "no exception");
    } else {
        CHECK(tr.code == SHSM_EXC_FILE_NOT_FOUND);
    }
    avp_tempdir_cleanup(t.dir);
}

AVP_TEST(stamp_key) {
    AvpArena *a = avp_arena();
    ClassesStampKey k1, k2;
    CHECK(!classes_stamp_key(NULL, &k1));
    CHECK(!classes_stamp_key(avp_json_arr(a), &k1));
    CHECK(classes_stamp_key(avp_json_obj(a), &k1)); /* {}: a tuple of Nones */
    CHECK(k1.v[0] == NULL);
    AvpJson *x = avp_json_parse(a, AVP_S("{\"asm\": {\"files\": 2, \"bytes\": 10, \"mtime_ns\": 5},"
                                         " \"symbols\": {\"bytes\": 3, \"mtime_ns\": 7}, \"scanner\": \"s\"}"), NULL);
    AvpJson *y = avp_json_parse(a, AVP_S("{\"asm\": {\"files\": 2.0, \"bytes\": 10, \"mtime_ns\": 5, \"dir\": \"d\"},"
                                         " \"symbols\": {\"bytes\": 3, \"mtime_ns\": 7}}"), NULL);
    AvpJson *z = avp_json_parse(a, AVP_S("{\"asm\": {\"files\": 2, \"bytes\": 10, \"mtime_ns\": 5},"
                                         " \"symbols\": {\"bytes\": 3, \"mtime_ns\": null}}"), NULL);
    REQUIRE(classes_stamp_key(x, &k1) && classes_stamp_key(y, &k2));
    CHECK(classes_stamp_key_eq(&k1, &k2)); /* 2 == 2.0, paths and the scanner not compared */
    REQUIRE(classes_stamp_key(z, &k2));
    CHECK(!classes_stamp_key_eq(&k1, &k2));
}

AVP_TEST(stale_reason) {
    AvpArena *a = avp_arena();
    Tree t = tree();
    CHECK_CZ(classes_stale_reason(a, db_with(NULL), t.asm_dir, t.symbols),
             "it records no input stamp (written by an older classes.exe)");
    AvpJson *s = classes_input_stamp(a, t.asm_dir, t.symbols);
    CHECK(classes_stale_reason(a, db_with(s), t.asm_dir, t.symbols) == NULL);
    put(t.symbols, "changed = .text:0x80000000; // type:function size:0x4\n");
    CHECK_CZ(classes_stale_reason(a, db_with(s), t.asm_dir, t.symbols), "the symbols file changed since the scan");
    put(avp_path_join(a, t.asm_dir, "c.s"), "x\n");
    CHECK_CZ(classes_stale_reason(a, db_with(s), t.asm_dir, t.symbols),
             "the asm tree (3 .s files, 40 bytes) and the symbols file changed since the scan");
    s = classes_input_stamp(a, t.asm_dir, t.symbols);
    CHECK(classes_stale_reason(a, db_with(s), t.asm_dir, t.symbols) == NULL);
    const char *why = classes_stale_reason(a, db_with(s), t.asm_dir, avp_path_join(a, t.dir, "none.txt"));
    CHECK_MSG(why && strncmp(why, "its inputs cannot be read ([WinError 2] ", 40) == 0, "%s", why ? why : "(null)");
    avp_tempdir_cleanup(t.dir);
}

AVP_TEST(staleness_verdicts) {
    /* classes_staleness: the comparison stale_reason words, for callers with their own wording (check_rules.exe) */
    AvpArena *a = avp_arena();
    Tree t = tree();
    ClassesStaleness s = classes_staleness(a, NULL, t.asm_dir, t.symbols);
    CHECK(s.verdict == CLASSES_STAMP_NONE);
    AvpJson *stamp = classes_input_stamp(a, t.asm_dir, t.symbols);
    /* a stamp with a scanner value (either producer, either base rule) is judged by the five values alone */
    avp_json_set(a, stamp, AVP_S("scanner"), avp_json_strz(a, "abc+base-rule=ctor"));
    s = classes_staleness(a, stamp, t.asm_dir, t.symbols);
    CHECK(s.verdict == CLASSES_STAMP_CURRENT && !s.asm_changed && !s.symbols_changed);
    put(t.symbols, "changed = .text:0x80000000; // type:function size:0x4\n");
    s = classes_staleness(a, stamp, t.asm_dir, t.symbols);
    CHECK(s.verdict == CLASSES_STAMP_CHANGED && !s.asm_changed && s.symbols_changed);
    put(avp_path_join(a, t.asm_dir, "c.s"), "x\n");
    s = classes_staleness(a, stamp, t.asm_dir, t.symbols);
    CHECK(s.verdict == CLASSES_STAMP_CHANGED && s.asm_changed && s.symbols_changed);
    s = classes_staleness(a, stamp, t.asm_dir, avp_path_join(a, t.dir, "none.txt"));
    CHECK(s.verdict == CLASSES_STAMP_UNREADABLE && s.error && strncmp(s.error, "[WinError 2] ", 13) == 0);
    avp_tempdir_cleanup(t.dir);
}

AVP_TEST(default_base_rule) {
    AvpArena *a = avp_arena();
    avp_unsetenv("SHSM_CLASSES_BASE_RULE");
    CHECK(classes_default_base_rule(a) == CLASSES_BASE_RULE_CTOR); /* the owner's ruling, 2026-11-13 */
    AVP_ASSERT(avp_setenv("SHSM_CLASSES_BASE_RULE", "slots"));
    CHECK(classes_default_base_rule(a) == CLASSES_BASE_RULE_SLOTS);
    AVP_ASSERT(avp_setenv("SHSM_CLASSES_BASE_RULE", "ctor"));
    CHECK(classes_default_base_rule(a) == CLASSES_BASE_RULE_CTOR);
    AVP_ASSERT(avp_setenv("SHSM_CLASSES_BASE_RULE", "other"));
    CHECK(classes_default_base_rule(a) == CLASSES_BASE_RULE_CTOR);
    avp_unsetenv("SHSM_CLASSES_BASE_RULE");
}

static const char *input_error(const char *asm_dir, const char *symbols) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        classes_check_inputs(asm_dir, symbols);
        avp_try_end(&t);
        return NULL;
    }
    CHECK(t.code == SHSM_EXC_USAGE);
    return avp_strdup(avp_arena(), t.msg ? t.msg : "");
}

AVP_TEST(check_inputs) {
    AvpArena *a = avp_arena();
    Tree t = tree();
    CHECK(input_error(t.asm_dir, t.symbols) == NULL);
    char *none = avp_path_join(a, t.dir, "none");
    CHECK_CZ(input_error(none, t.symbols),
             avp_sprintf(a, "asm directory not found: %s (run ninja to split the DOL, or pass --asm)", none));
    char *empty = avp_path_join(a, t.dir, "empty");
    AVP_ASSERT(avp_makedirs(empty, true));
    put(avp_path_join(a, empty, "x.txt"), "");
    CHECK_CZ(input_error(empty, t.symbols), avp_sprintf(a, "no .s files under the asm directory: %s", empty));
    CHECK_CZ(input_error(t.asm_dir, none), avp_sprintf(a, "symbols file not found: %s (pass --symbols)", none));
    avp_tempdir_cleanup(t.dir);
}

AVP_TEST(scanner_digest_and_content_key) {
    AvpArena *a = avp_arena();
    Tree t = tree();
    char *d1 = classes_scanner_digest(a, t.splits);
    CHECK(strlen(d1) == 64);
    CHECK_CZ(classes_scanner_digest(a, t.splits), d1);
    char *d0 = classes_scanner_digest(a, NULL);
    CHECK(strcmp(d0, d1) != 0);
    CHECK_CZ(classes_scanner_digest(a, avp_path_join(a, t.dir, "none.txt")), d0); /* a missing splits file: none */
    char *k1 = classes_content_key(a, t.asm_dir, t.symbols, d1);
    CHECK(strlen(k1) == 64);
    CHECK_CZ(classes_content_key(a, t.asm_dir, t.symbols, d1), k1);
    CHECK(strcmp(classes_content_key(a, t.asm_dir, t.symbols, d0), k1) != 0);
    put(avp_path_join(a, t.asm_dir, "a.s"), ".fn a\n.endfn A\n"); /* same size, other bytes */
    char *k2 = classes_content_key(a, t.asm_dir, t.symbols, d1);
    CHECK(strcmp(k2, k1) != 0);
    /* the key depends on the relative paths, not where the tree lives */
    char *moved = avp_path_join(a, t.dir, "asm2");
    REQUIRE(avp_rename(t.asm_dir, moved));
    CHECK_CZ(classes_content_key(a, moved, t.symbols, d1), k2);
    avp_tempdir_cleanup(t.dir);
}

AVP_TEST(current_and_dump) {
    AvpArena *a = avp_arena();
    Tree t = tree();
    AvpJson *stamp = classes_input_stamp(a, t.asm_dir, t.symbols);
    avp_json_set(a, stamp, AVP_S("scanner"), avp_json_strz(a, "abc"));
    AvpJson *doc = avp_json_obj(a);
    avp_json_set(a, doc, AVP_S("inputs"), stamp);
    avp_json_set(a, doc, AVP_S("stats"), avp_json_parse(a, AVP_S("{\"n\": 1, \"s\": \"\\u00e9\"}"), NULL));
    avp_json_set(a, doc, AVP_S("classes"), avp_json_arr(a));
    AvpStr text = classes_dump_file_text(a, doc);
    CHECK(text.n > 2 && text.p[text.n - 1] == '}');                  /* no final newline */
    CHECK(avp_str_find(text, AVP_S("{\r\n \"inputs\": {\r\n  \"asm\""), 0, (i64)text.n) == 0); /* indent=1, CRLF */
    CHECK(avp_str_find(text, AVP_S("\"\\u00e9\""), 0, (i64)text.n) >= 0); /* ensure_ascii */
    CHECK(avp_str_find(text, AVP_S("\"classes\": []"), 0, (i64)text.n) >= 0);
    char *out = avp_path_join(a, t.dir, "classes.json");
    CHECK(classes_current(a, out, stamp, "abc") == NULL); /* no file */
    REQUIRE(avp_write_file(out, text.p, text.n));
    CHECK(classes_current(a, out, stamp, "abc") != NULL);
    CHECK(classes_current(a, out, stamp, "abd") == NULL); /* another scanner */
    AvpJson *other = classes_input_stamp(a, t.asm_dir, t.symbols);
    avp_json_set(a, avp_json_getz(other, "symbols"), AVP_S("bytes"), avp_json_int(a, 1));
    CHECK(classes_current(a, out, other, "abc") == NULL); /* another stamp */
    avp_json_set(a, doc, AVP_S("stats"), avp_json_arr(a));
    text = classes_dump_file_text(a, doc);
    REQUIRE(avp_write_file(out, text.p, text.n));
    CHECK(classes_current(a, out, stamp, "abc") == NULL); /* stats not a dict */
    REQUIRE(avp_write_file(out, "{", 1));
    CHECK(classes_current(a, out, stamp, "abc") == NULL); /* not JSON */
    avp_tempdir_cleanup(t.dir);
}

AVP_TEST(default_paths) {
    AvpArena *a = avp_arena();
    ClassesPaths p = classes_default_paths(a, "X:\\repo");
    CHECK_CZ(p.asm_dir, "X:\\repo\\build\\R5WEA4\\asm");
    CHECK_CZ(p.symbols, "X:\\repo\\config\\R5WEA4\\symbols.txt");
    CHECK_CZ(p.out, "X:\\repo\\build\\R5WEA4\\classes.json");
    CHECK_CZ(p.splits, "X:\\repo\\config\\R5WEA4\\splits.txt");
}

AVP_TEST_MAIN("classes_stamp")
