/* shsm_report: the objdiff report reader on a fixture. */
#include "shsm_report.h"
#include "shsm_test_util.h"

static const char REPORT[] =
    "{\"measures\": {\"fuzzy_match_percent\": 1.5}, \"units\": ["
    "{\"name\": \"main/a\", \"measures\": {\"fuzzy_match_percent\": 50.5}, \"metadata\": {\"complete\": true, "
    "\"source_path\": \"src/a.c\"}, \"functions\": ["
    "{\"name\": \"f\", \"size\": \"16\", \"fuzzy_match_percent\": 100.0, \"metadata\": {\"virtual_address\": "
    "\"2147483648\", \"demangled_name\": \"f()\"}},"
    "{\"name\": \"g\", \"size\": \"8\", \"address\": \"16\"},"
    "{\"name\": \"f\", \"size\": \"4\"}]},"
    "{\"name\": \"main/b\", \"functions\": [{\"name\": \"f\", \"size\": 1, \"fuzzy_match_percent\": 0}, "
    "{\"name\": \"h\", \"size\": \"2\"}]}]}";

AVP_TEST(report_fixture) {
    const char *d = fx_dir("report fixture");
    const char *p = fx_write(d, "report.json", REPORT);
    ShsmReport *rep = shsm_read_report(p);
    REQUIRE(rep->nunits == 2);
    const ShsmUnitReport *a = shsm_report_unit(rep, "a.c");
    REQUIRE(a);
    CHECK(a->complete);
    CHECK_EQ_F64(a->fuzzy, 50.5);
    CHECK_CZ(a->source_path, "src/a.c");
    CHECK_EQ_U64(a->nfunctions, 3);
    const ShsmFunctionReport *f = shsm_unit_report_function(a, "f");
    REQUIRE(f);
    CHECK_EQ_I64(f->size, 16);
    CHECK(f->scored && f->has_address && f->address == 0x80000000u);
    CHECK_CZ(f->demangled, "f()");
    const ShsmFunctionReport *g = shsm_report_function(rep, "g", NULL);
    REQUIRE(g);
    CHECK(!g->scored && g->fuzzy == 0.0 && !g->has_address && g->has_offset && g->offset == 16);
    CHECK_CZ(shsm_report_function(rep, "f", NULL)->unit, "main/a");
    const ShsmFunctionReport *bf = shsm_report_function(rep, "f", "main/b");
    REQUIRE(bf);
    CHECK(bf->scored && bf->fuzzy == 0.0 && bf->size == 1);
    CHECK_CZ(shsm_report_function(rep, "h", NULL)->unit, "main/b");
    CHECK(shsm_report_unit(rep, "c") == NULL);
    ShsmRepo r = {0};
    r.report_json = avp_join(avp_arena(), d, "missing.json");
    char msg[1024];
    snprintf(msg, sizeof msg, "no report at %s (build it with ninja first)", r.report_json);
    CHECK_RAISES(shsm_load_report(&r), SHSM_EXC_FILE_NOT_FOUND, msg);
}

AVP_TEST_MAIN("shsm_report")
