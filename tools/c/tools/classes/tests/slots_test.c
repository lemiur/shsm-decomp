/* Slot labels (classes_slot_label): the class that introduces a slot is the topmost base whose table has the index,
 * the class whose function the table holds is the first one from the asked class up that does not merely inherit
 * the word, pure slots are null entries, and the name to declare keeps the introducing slot's plain name with the
 * declaring class's mangling. Synthetic classes.json: Alpha introduces slots 2 (A), 3 (pure) and 4 (B); Beta
 * overrides 3 and 4; Gamma inherits 2 and 3, overrides 4 under another plain name, holds a new slot 5 that its
 * secondary base Mixin also has (an override in the embedded group, word 6); Delta inherits from a base whose table
 * the document does not have. */
#include "avp/test.h"
#include "classes.h"

#include <string.h>

#define HDR "{\"index\": 0, \"offset\": \"0x0\", \"status\": \"header\"}, {\"index\": 1, \"offset\": \"0x4\", \"status\": \"header\"}"
#define CLS(name, words)                                                                                         \
    "{\"key\": \"" name "\", \"name\": \"" name "\", \"name_source\": \"x\", \"vtable\": {\"symbol\": \"__vt__" name \
    "\", \"address\": \"0x80001000\", \"size\": \"0x20\", \"groups\": [], \"group_evidence\": \"s\","              \
    " \"group_conflicts\": [], \"words\": [" HDR ", " words "]}, \"bases\": [], \"derived\": [], \"fields\": []}"

static const char DOC[] =
    "{\"classes\": ["
    CLS("Alpha",
        "{\"index\": 2, \"offset\": \"0x8\", \"target\": \"fn_A__5AlphaFv\", \"status\": \"new\", \"group\": \"complete\"},"
        "{\"index\": 3, \"offset\": \"0xC\", \"target\": null, \"status\": \"null\", \"group\": \"complete\"},"
        "{\"index\": 4, \"offset\": \"0x10\", \"target\": \"fn_B__5AlphaFv\", \"status\": \"new\", \"group\": \"complete\"}")
    ","
    CLS("Beta",
        "{\"index\": 2, \"offset\": \"0x8\", \"target\": \"fn_A__5AlphaFv\", \"status\": \"inherited\", \"group\": \"complete\","
        " \"base\": \"Alpha\", \"base_index\": 2},"
        "{\"index\": 3, \"offset\": \"0xC\", \"target\": \"fn_Z__4BetaFv\", \"status\": \"override\", \"group\": \"complete\","
        " \"base\": \"Alpha\", \"base_target\": null, \"base_index\": 3},"
        "{\"index\": 4, \"offset\": \"0x10\", \"target\": \"fn_B__4BetaFv\", \"status\": \"override\", \"group\": \"complete\","
        " \"base\": \"Alpha\", \"base_target\": \"fn_B__5AlphaFv\", \"base_index\": 4}")
    ","
    CLS("Gamma",
        "{\"index\": 2, \"offset\": \"0x8\", \"target\": \"fn_A__5AlphaFv\", \"status\": \"inherited\", \"group\": \"complete\","
        " \"base\": \"Beta\", \"base_index\": 2},"
        "{\"index\": 3, \"offset\": \"0xC\", \"target\": \"fn_Z__4BetaFv\", \"status\": \"inherited\", \"group\": \"complete\","
        " \"base\": \"Beta\", \"base_index\": 3},"
        "{\"index\": 4, \"offset\": \"0x10\", \"target\": \"fn_Y__5GammaFv\", \"status\": \"override\", \"group\": \"complete\","
        " \"base\": \"Beta\", \"base_target\": \"fn_B__4BetaFv\", \"base_index\": 4},"
        "{\"index\": 5, \"offset\": \"0x14\", \"target\": \"fn_X__5GammaFv\", \"status\": \"new\", \"group\": \"complete\"},"
        "{\"index\": 6, \"offset\": \"0x18\", \"target\": \"@8@fn_X__5GammaFv\", \"status\": \"override\","
        " \"group\": \"embedded\", \"base\": \"Mixin\", \"base_target\": \"fn_X__5MixinFv\", \"base_index\": 2,"
        " \"thunk\": {\"adjust\": 8, \"target\": \"fn_X__5GammaFv\"}}")
    ","
    CLS("Mixin",
        "{\"index\": 2, \"offset\": \"0x8\", \"target\": \"fn_X__5MixinFv\", \"status\": \"new\", \"group\": \"complete\"}")
    ","
    CLS("Delta",
        "{\"index\": 2, \"offset\": \"0x8\", \"target\": \"fn_Q__6NowhereFv\", \"status\": \"inherited\","
        " \"group\": \"complete\", \"base\": \"Nowhere\", \"base_index\": 2}")
    "]}";

static ClassesDb *db(void) {
    AvpArena *a = avp_arena();
    return classes_db_from_json(a, avp_json_parse(a, avp_str(DOC), NULL));
}

static const ClassesWord *word(const ClassesDb *d, const char *cls, i64 index, const ClassesClass **c) {
    *c = classes_by_name(d, cls);
    return *c ? classes_word_at(*c, index) : NULL;
}

static bool label(const ClassesDb *d, const char *cls, i64 index, ClassesSlotLabel *l, const ClassesWord **w) {
    const ClassesClass *c;
    *w = word(d, cls, index, &c);
    if (!*w) return false;
    return classes_slot_label(avp_arena(), d, c, *w, l);
}

AVP_TEST(inherited_slot_names_the_class_that_defines_it) {
    ClassesDb *d = db();
    ClassesSlotLabel l;
    const ClassesWord *w;
    REQUIRE(label(d, "Gamma", 2, &l, &w));
    CHECK_CZ(l.introducer, "Alpha");
    CHECK_CZ(l.holder, "Alpha"); /* not Beta: `<- Beta` is only the next table up */
    CHECK(l.inherited && l.declare == NULL && !l.pure_intro && !l.topmost_known);
    CHECK_CZ(classes_slot_label_text(avp_arena(), w, &l), "introduced by Alpha; held by Alpha; inherited unchanged");
}

AVP_TEST(pure_slot_overridden_two_levels_up) {
    ClassesDb *d = db();
    ClassesSlotLabel l;
    const ClassesWord *w;
    REQUIRE(label(d, "Gamma", 3, &l, &w));
    CHECK_CZ(l.introducer, "Alpha");
    CHECK(l.pure_intro);
    CHECK_CZ(l.holder, "Beta"); /* Beta's override, inherited unchanged by Gamma */
    CHECK_CZ(classes_slot_label_text(avp_arena(), w, &l), "introduced by Alpha (pure); held by Beta; inherited unchanged");
    REQUIRE(label(d, "Alpha", 3, &l, &w)); /* the null entry itself */
    CHECK_CZ(l.introducer, "Alpha");
    CHECK(l.pure_intro && l.pure_here && l.holder == NULL);
    CHECK_CZ(classes_slot_label_text(avp_arena(), w, &l), "introduced by Alpha (pure); null entry in this table");
}

AVP_TEST(override_keeps_the_introducing_plain_name) {
    ClassesDb *d = db();
    ClassesSlotLabel l;
    const ClassesWord *w;
    REQUIRE(label(d, "Gamma", 4, &l, &w));
    CHECK_CZ(l.introducer, "Alpha"); /* through Beta's override */
    CHECK_CZ(l.holder, "Gamma");
    CHECK_CZ(l.intro_target, "fn_B__5AlphaFv");
    CHECK_CZ(l.declare, "fn_B__5GammaFv"); /* the table's symbol says fn_Y__5GammaFv */
    CHECK_CZ(classes_slot_label_text(avp_arena(), w, &l), "introduced by Alpha; held by Gamma; declare fn_B__5GammaFv");
    REQUIRE(label(d, "Beta", 3, &l, &w)); /* an override of a pure slot declares the table's own name */
    CHECK(l.pure_intro);
    CHECK_CZ(l.declare, "fn_Z__4BetaFv");
    CHECK_CZ(classes_slot_label_text(avp_arena(), w, &l), "introduced by Alpha (pure); held by Beta");
}

AVP_TEST(new_slot_held_by_a_secondary_base_is_that_bases_slot) {
    ClassesDb *d = db();
    ClassesSlotLabel l;
    const ClassesWord *w;
    REQUIRE(label(d, "Gamma", 5, &l, &w));
    CHECK_CZ(l.introducer, "Mixin");
    CHECK_CZ(l.holder, "Gamma");
    CHECK_CZ(l.via_offset, "0x18");
    CHECK_CZ(classes_slot_label_text(avp_arena(), w, &l),
             "introduced by Mixin; held by Gamma; also held at 0x18 by the table of a secondary base");
    REQUIRE(label(d, "Gamma", 6, &l, &w)); /* the thunked word itself */
    CHECK_CZ(l.introducer, "Mixin");
    CHECK_CZ(l.holder, "Gamma");
    REQUIRE(label(d, "Alpha", 2, &l, &w)); /* a plain new slot is introduced here */
    CHECK_CZ(l.introducer, "Alpha");
    CHECK_CZ(l.holder, "Alpha");
    CHECK(l.via_offset == NULL);
}

AVP_TEST(chain_ends_at_a_base_without_a_table) {
    ClassesDb *d = db();
    ClassesSlotLabel l;
    const ClassesWord *w;
    REQUIRE(label(d, "Delta", 2, &l, &w));
    CHECK_CZ(l.introducer, "Nowhere");
    CHECK(l.topmost_known);
    CHECK_CZ(l.holder, "Nowhere");
    CHECK(strstr(classes_slot_label_text(avp_arena(), w, &l), "introduced by Nowhere or above") != NULL);
}

AVP_TEST(header_words_have_no_label_and_the_legend_is_one_line) {
    ClassesDb *d = db();
    ClassesSlotLabel l;
    const ClassesWord *w;
    CHECK(!label(d, "Gamma", 0, &l, &w));
    const char *g = classes_slot_legend();
    CHECK(strchr(g, '\n') == NULL && strstr(g, "introduced by") && strstr(g, "held by") && strstr(g, "declare") && strstr(g, "pure"));
}

AVP_TEST_MAIN("classes_slots")
