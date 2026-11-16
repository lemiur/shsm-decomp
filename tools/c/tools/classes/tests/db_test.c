/* The classes.json reader: decoding, find_class and the other lookups, and the decode errors. */
#include "avp/test.h"
#include "classes.h"
#include "shsm_cli.h"

#include <string.h>

static const char DOC[] =
    "{\"inputs\": {}, \"stats\": {}, \"methods\": {\"f__1AFv\": \"f()\", \"n\": null},"
    " \"classes\": ["
    "{\"key\": \"vt:0x80001000\", \"name\": \"A\", \"name_source\": \"vt-symbol\", \"unit\": \"game/A\","
    " \"name_candidates\": [{\"name\": \"A\", \"source\": \"vt-symbol\"}],"
    " \"vtable\": {\"symbol\": \"__vt__1A\", \"address\": \"0x80001000\", \"size\": \"0xC\","
    "  \"groups\": [{\"addend\": \"0x0\", \"this_offset\": null}], \"group_evidence\": \"stores\", \"group_conflicts\": [],"
    "  \"words\": [{\"index\": 0, \"offset\": \"0x0\", \"status\": \"header\"},"
    "             {\"index\": 1, \"offset\": \"0x4\", \"status\": \"header\", \"rtti\": \"__RTTI__1A\"},"
    "             {\"index\": 2, \"offset\": \"0x8\", \"target\": \"@4@f__1AFv\", \"status\": \"override\","
    "              \"group\": \"embedded\", \"base\": \"B\", \"base_target\": null, \"base_index\": 2,"
    "              \"thunk\": {\"adjust\": 4, \"target\": \"f__1AFv\"}}]},"
    " \"ctors\": [{\"name\": \"__ct__1AFv\", \"address\": \"0x80002000\"}],"
    " \"dtors\": [{\"name\": \"__dt__1AFv\", \"address\": \"0x80002100\"}],"
    " \"factories\": [{\"name\": \"fn_80002200\", \"address\": \"0x80002200\"}],"
    " \"bases\": [{\"class\": \"B\", \"key\": \"B\", \"offset\": null, \"primary\": true, \"vtable_word\": null,"
    "  \"evidence\": [\"dtor-call\"]}],"
    " \"derived\": [], \"own_vptrs\": [\"0x0\"],"
    " \"member_objects\": [{\"offset\": \"0x8\", \"class\": \"C\", \"evidence\": [\"e1\", \"e2\"]}],"
    " \"fields\": [{\"offset\": \"0x4\", \"width\": 2, \"kind\": \"int\", \"type\": \"s16\", \"loads\": 1,"
    "  \"stores\": 0, \"functions\": 1, \"conflict\": [\"x\"]}],"
    " \"size\": \"0x10\", \"size_source\": \"new\", \"size_estimate\": 16, \"base_end\": \"0x4\"},"
    "{\"key\": \"B\", \"name\": \"B\", \"name_source\": \"mangled-dtor\", \"vtable\": null,"
    " \"name_candidates\": [{\"name\": \"Bee\", \"source\": \"registry-string\"}], \"bases\": [], \"derived\": [],"
    " \"fields\": []},"
    "{\"key\": \"B\", \"name\": \"__vt__1A\", \"name_source\": \"x\", \"vtable\": null, \"bases\": [], \"derived\": [],"
    " \"fields\": []}"
    "]}";

static ClassesDb *db(void) {
    AvpArena *a = avp_arena();
    return classes_db_from_json(a, avp_json_parse(a, avp_str(DOC), NULL));
}

AVP_TEST(decode) {
    ClassesDb *d = db();
    REQUIRE(d->n == 3);
    const ClassesClass *c = &d->classes[0];
    CHECK_CZ(c->name, "A");
    CHECK_CZ(c->unit, "game/A");
    REQUIRE(c->vtable != NULL);
    CHECK_CZ(c->vtable->symbol, "__vt__1A");
    CHECK(c->vtable->n_groups == 1 && c->vtable->groups[0].this_offset == NULL);
    REQUIRE(c->vtable->n_words == 3);
    CHECK(c->vtable->words[0].group == NULL && c->vtable->words[0].target == NULL);
    CHECK_CZ(c->vtable->words[1].rtti, "__RTTI__1A");
    const ClassesWord *w = &c->vtable->words[2];
    CHECK(w->has_thunk && w->thunk_adjust == 4);
    CHECK_CZ(w->thunk_target, "f__1AFv");
    CHECK(w->has_base_target && w->base_target == NULL);
    CHECK(w->has_base_index && w->base_index == 2);
    CHECK(c->n_ctors == 1 && c->n_dtors == 1 && c->n_factories == 1 && c->n_other_installers == 0);
    REQUIRE(c->n_bases == 1);
    CHECK(c->bases[0].offset == NULL && c->bases[0].primary && !c->bases[0].has_vtable_word);
    CHECK(c->bases[0].evidence.n == 1);
    CHECK(c->n_member_objects == 1 && c->member_objects[0].evidence.n == 2);
    REQUIRE(c->n_fields == 1);
    CHECK(c->fields[0].width == 2 && c->fields[0].conflict.n == 1 && c->fields[0].inherited_from == NULL);
    CHECK_CZ(c->size, "0x10");
    CHECK(c->has_size_estimate && c->size_estimate == 16);
    CHECK(d->classes[1].vtable == NULL && d->classes[1].size == NULL && !d->classes[1].has_size_estimate);
}

AVP_TEST(find_class) {
    ClassesDb *d = db();
    const ClassesClass *a = &d->classes[0], *b = &d->classes[1], *v = &d->classes[2];
    CHECK(classes_find(d, "A") == a);
    CHECK(classes_find(d, "vt:0x80001000") == a);
    CHECK(classes_find(d, "B") == b);            /* name and key: the first class */
    CHECK(classes_find(d, "__vt__1A") == a);     /* a vtable symbol before a later class's name */
    (void)v;
    CHECK(classes_find(d, "0x80001000") == a);   /* vtable address */
    CHECK(classes_find(d, "80001000") == a);
    CHECK(classes_find(d, "0x80002000") == a);   /* constructor */
    CHECK(classes_find(d, "80002100") == a);     /* destructor */
    CHECK(classes_find(d, "0x80002200") == a);   /* factory */
    CHECK(classes_find(d, "80002200\n") == a);   /* `$` matches before a final newline */
    CHECK(classes_find(d, "0x8000200") == NULL); /* seven digits */
    CHECK(classes_find(d, "0X80001000") == NULL);
    CHECK(classes_find(d, "Bee") == b);          /* a name candidate */
    CHECK(classes_find(d, "nothing") == NULL);
}

AVP_TEST(lookups) {
    ClassesDb *d = db();
    CHECK(classes_by_key(d, "B") == &d->classes[2]); /* {x["key"]: x}: the last wins */
    CHECK(classes_by_key(d, "nothing") == NULL);
    CHECK(classes_by_name(d, "B") == &d->classes[1]);
    CHECK(classes_by_vtable_symbol(d, "__vt__1A") == &d->classes[0]);
    CHECK(classes_by_address(d, 0x80002100u) == &d->classes[0]);
    CHECK(classes_by_address(d, 0x80009999u) == NULL);
    CHECK_CZ(classes_method(d, "f__1AFv"), "f()");
    CHECK(classes_method(d, "n") == NULL);
    CHECK(classes_method(d, "missing") == NULL);
    CHECK(classes_method(d, NULL) == NULL);
    i64 v = 0;
    CHECK(classes_hex("0x2C", &v) && v == 0x2C);
    CHECK(classes_hex("2c", &v) && v == 0x2C);
    CHECK(!classes_hex("zz", &v));
    CHECK(!classes_hex(NULL, &v));
}

static int decode_error(const char *text) {
    AvpArena *a = avp_arena();
    AvpTry t;
    if (AVP_TRY(&t)) {
        classes_db_from_json(a, avp_json_parse(a, avp_str(text), NULL));
        avp_try_end(&t);
        return 0;
    }
    return t.code;
}

AVP_TEST(decode_errors) {
    CHECK(decode_error("[]") == SHSM_EXC_TYPE);
    CHECK(decode_error("{}") == SHSM_EXC_KEY);
    CHECK(decode_error("{\"classes\": [{\"name\": \"x\"}]}") == SHSM_EXC_KEY);
    CHECK(decode_error("{\"classes\": [{\"key\": 1, \"name\": \"x\", \"name_source\": \"y\"}]}") == SHSM_EXC_TYPE);
    CHECK(decode_error("{\"classes\": []}") == 0);
}

AVP_TEST(load_file) {
    AvpArena *a = avp_arena();
    char *dir = avp_mkdtemp(a, "classes_db", NULL, NULL);
    REQUIRE(dir != NULL);
    char *p = avp_path_join(a, dir, "classes.json");
    REQUIRE(avp_write_file(p, DOC, strlen(DOC)));
    ClassesDb *d = classes_db_load(a, p);
    CHECK(d->n == 3);
    REQUIRE(avp_write_file(p, "{", 1));
    AvpTry t;
    if (AVP_TRY(&t)) {
        classes_db_load(a, p);
        avp_try_end(&t);
        CHECK_MSG(false, "a bad file loaded");
    } else {
        CHECK(t.code == SHSM_EXC_VALUE);
        CHECK(t.msg && strstr(t.msg, "Expecting property name enclosed in double quotes") != NULL);
    }
    if (AVP_TRY(&t)) {
        classes_db_load(a, avp_path_join(a, dir, "missing.json"));
        avp_try_end(&t);
        CHECK_MSG(false, "a missing file loaded");
    } else {
        CHECK(t.code == SHSM_EXC_FILE_NOT_FOUND);
    }
    avp_tempdir_cleanup(dir);
}

AVP_TEST_MAIN("classes_db")
