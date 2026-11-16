/* The pure parts of tools/test_brief.py: _clip, _pct, parse_sections, the page budget (layout's shrink order,
 * --full), work_order, _base_words and profile_line, and the formatters on fixed data (twins too small, the listing
 * caps, the start lines, the shape line). */
#include "avp/test.h"
#include "brief.h"

#include <string.h>

static AvpJson *jparse(const char *text) {
    AvpJson *j = avp_json_parse(avp_arena(), avp_str(text), NULL);
    AVP_ASSERT(j);
    return j;
}

static AvpStr joined(BriefLines L) { return avp_str_join(avp_arena(), AVP_S("\n"), L.v, L.n); }

static bool contains(AvpStr hay, const char *needle) { return avp_str_find(hay, avp_str(needle), 0, AVP_END) >= 0; }

AVP_TEST(clip_and_pct) {
    CHECK_CZ(avp_cz(avp_arena(), brief_clip(AVP_S("  a \n b\tc  "), 110)), "a b c");
    CHECK_CZ(avp_cz(avp_arena(), brief_clip(AVP_S("abcdefghij"), 8)), "abcde...");
    CHECK_CZ(avp_cz(avp_arena(), brief_clip(AVP_S("\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9"), 4)), "\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9");
    CHECK_CZ(brief_pct(NULL), "-");
    CHECK_CZ(brief_pct(avp_json_float(avp_arena(), 99.96)), "100.0%");
    CHECK_CZ(brief_pct(avp_json_float(avp_arena(), 100.0)), "100%");
    CHECK_CZ(brief_pct(avp_json_int(avp_arena(), 42)), "42.0%");
    CHECK_CZ(brief_hex(0x800067CC), "0x800067CC");
}

AVP_TEST(parse_sections) {
    static const char *const allowed[] = {"identity", "status", "twins", "context", "shape", "listing", "start"};
    CHECK(brief_parse_sections(NULL, allowed, 7) == NULL);
    CHECK(brief_parse_sections("", allowed, 7) == NULL);
    AvpStrVec *s = brief_parse_sections(" Where ,asm,start-here,stands", allowed, 7);
    REQUIRE(s && s->n == 4);
    CHECK(avp_str_eqz(s->v[0], "status"));
    CHECK(avp_str_eqz(s->v[1], "listing"));
    CHECK(avp_str_eqz(s->v[2], "start"));
    CHECK(avp_str_eqz(s->v[3], "status"));
    AvpTry t;
    if (AVP_TRY(&t)) {
        brief_parse_sections("bogus", allowed, 7);
        avp_try_end(&t);
        CHECK_MSG(false, "no error for an unknown section");
    } else {
        CHECK(t.code == SHSM_EXC_USAGE);
        CHECK_CZ(t.msg, "unknown section 'bogus' (one of identity, status, twins, context, shape, listing, start)");
    }
}

AVP_TEST(work_order) {
    AvpJson *items = jparse(
        "[{\"name\": \"top\", \"callees\": [\"mid\", \"leaf\"], \"twin\": null, \"size\": 8, \"address\": 1},"
        " {\"name\": \"mid\", \"callees\": [\"leaf\"], \"twin\": null, \"size\": 8, \"address\": 2},"
        " {\"name\": \"leaf\", \"callees\": [], \"twin\": null, \"size\": 64, \"address\": 3},"
        " {\"name\": \"small\", \"callees\": [], \"twin\": null, \"size\": 4, \"address\": 4},"
        " {\"name\": \"twinned\", \"callees\": [\"ext\"], \"twin\": {\"name\": \"t\"}, \"size\": 99, \"address\": 5},"
        " {\"name\": \"a\", \"callees\": [\"b\"], \"twin\": null, \"size\": 8, \"address\": 6},"
        " {\"name\": \"b\", \"callees\": [\"a\"], \"twin\": null, \"size\": 8, \"address\": 7}]");
    AvpJson *order = brief_work_order(items);
    REQUIRE(avp_json_len(order) == 7);
    const char *expect[] = {"twinned", "small", "leaf", "mid", "top", "a", "b"};
    for (size_t i = 0; i < 7; i++)
        CHECK(avp_str_eqz(avp_json_getz(avp_json_at(order, i), "name")->u.str, expect[i]));
}

AVP_TEST(base_words_and_profile_line) {
    AvpStrVec cf = {0};
    const char *w[] = {"-O4,p", "-i", "include", "-DNDEBUG=1", "-O3", "-ipa", "file", "-lang=c++"};
    for (size_t i = 0; i < 8; i++) avp_vec_push(&cf, avp_str(w[i]));
    AvpJson *bw = brief_base_words(&cf);
    REQUIRE(avp_json_len(bw) == 3);
    CHECK(avp_str_eqz(avp_json_at(bw, 0)->u.str, "-O3"));
    CHECK(avp_str_eqz(avp_json_at(bw, 2)->u.str, "file"));
    AvpJson *p = jparse("{\"unit\": \"main/game/Bar\", \"compiler\": \"Wii/1.0\", \"extra\": [\"-O3\"], \"lang\": "
                        "\"c++\", \"match\": \"same library\", \"library\": \"game\", \"distance\": 4}");
    CHECK_CZ(brief_profile_line(p, "game/G1"),
              "expected profile (inferred from game/Bar, nearest same library source unit): Wii/1.0 + -O3 (-lang=c++)");
    brief_set(p, "match", avp_json_strz(avp_arena(), "different library"));
    CHECK(contains(avp_str(brief_profile_line(p, "fmod")), "unknown, no source unit of fmod"));
    CHECK_CZ(brief_profile_line(NULL, "fmod"), "expected profile: unknown (no source unit to infer from)");
    avp_vec_free(&cf);
}

static BriefSection *section(const char *name, AvpJson *data, BriefFmt fmt, int levels) {
    BriefSection *s = AVP_NEW(avp_arena(), BriefSection);
    s->name = name;
    s->data = data;
    s->fmt = fmt;
    s->levels = levels;
    s->first = BRIEF_LISTING_LINES;
    return s;
}

static AvpJson *listing_data(int n) {
    AvpArena *a = avp_arena();
    AvpJson *d = avp_json_obj(a), *lines = avp_json_arr(a);
    for (int i = 0; i < n; i++) avp_json_arr_push(a, lines, avp_json_str(a, avp_pyfmt(a, "line %d", i)));
    brief_set(d, "kind", avp_json_strz(a, "ghidra"));
    brief_set(d, "lines", lines);
    brief_set(d, "note", avp_json_strz(a, "// Ghidra listing: a reading aid"));
    brief_set(d, "file", avp_json_strz(a, "build/ghidra/decomp/x.c"));
    brief_set(d, "stale", avp_json_bool(false));
    return d;
}

AVP_TEST(listing_caps) {
    BriefSection *s = section("listing", listing_data(80), brief_fmt_listing, 6);
    BriefLines L = brief_fmt_listing(s, 0);
    REQUIRE(L.n == 42);
    CHECK(avp_str_eqz(L.v[0], "// Ghidra listing: a reading aid"));
    CHECK(avp_str_eqz(L.v[41], "... 40 more lines (build/ghidra/decomp/x.c, or --full)"));
    CHECK(brief_fmt_listing(s, 5).n == 2);
    CHECK(brief_fmt_listing(s, 4).n == 10);
    CHECK(brief_fmt_listing(s, BRIEF_FULL).n == 81);
}

AVP_TEST(layout_shrinks_listing_first) {
    AvpArena *a = avp_arena();
    AvpJson *ident = avp_json_arr(a), *start = avp_json_arr(a);
    avp_json_arr_push(a, ident, avp_json_strz(a, "fn_x"));
    avp_json_arr_push(a, start, avp_json_strz(a, "  build/tools/draft.exe twin fn_x"));
    BriefSections secs = {0};
    secs.v[secs.n++] = section("identity", ident, brief_fmt_list, 1);
    secs.v[secs.n++] = section("listing", listing_data(80), brief_fmt_listing, 6);
    secs.v[secs.n++] = section("start", start, brief_fmt_list, 1);
    int levels[8];
    brief_layout(&secs, 1000000, false, levels);
    CHECK(levels[0] == 0 && levels[1] == 0 && levels[2] == 0);
    brief_layout(&secs, 30, false, levels);
    CHECK(levels[1] == 2); /* 20 listing lines + note + footer + 2 + 2 + 1 = 27 */
    brief_layout(&secs, 5, false, levels);
    CHECK(levels[1] == 5 && levels[0] == 0 && levels[2] == 0);
    brief_layout(&secs, 5, true, levels);
    CHECK(levels[1] == BRIEF_FULL);
    BriefLines page = brief_render(&secs, levels);
    CHECK(avp_str_eqz(page.v[0], "fn_x"));
    CHECK(avp_str_eqz(page.v[1], "## Listing"));
    CHECK(contains(joined(page), "line 79"));
    CHECK(contains(joined(page), "## Start here"));
}

AVP_TEST(twins_too_small_and_shape) {
    BriefSection *s = section("twins", jparse("{\"too_small\": true, \"name\": \"x\"}"), brief_fmt_twins, 3);
    BriefLines L = brief_fmt_twins(s, 0);
    REQUIRE(L.n == 1);
    CHECK(avp_str_eqz(L.v[0], "  under 16 bytes: twins carry no information here"));
    BriefSection *sh = section("shape",
                               jparse("{\"instructions\": 37, \"calls\": 0, \"tail_calls\": 1, \"branches\": 9, "
                                      "\"frame\": 0, \"saved_gprs\": \"r31\", \"saved_fprs\": null, \"float\": true, "
                                      "\"loops\": 2, \"jump_tables\": 1}"),
                               brief_fmt_shape, 1);
    L = brief_fmt_shape(sh, 0);
    REQUIRE(L.n == 1);
    CHECK_CZ(avp_cz(avp_arena(), L.v[0]), "  37 instructions, no frame, saves r31, 0 calls (1 tail), 2 loop(s), 1 jump "
                                           "table(s), float code");
}

AVP_TEST(start_names_and_fragments_lines) {
    AvpJson *d = jparse("{\"state\": \"unstarted\", \"commands\": [], \"profile\": null, \"plan\": null, "
                        "\"read_first\": null, \"errors\": [], \"symbol\": \"fn_800CA138\", \"library\": \"game/G4\", "
                        "\"names\": [[\"lbl_80529B18\", \"FMVTrigger\", \"interned name\"]], "
                        "\"fragments\": [[\"L113\", 32, 7, 82]]}");
    BriefSection *s = section("start", d, brief_fmt_start, 1);
    AvpStr text = joined(brief_fmt_start(s, 0));
    CHECK(contains(text, "known names (build/tools/hashes.exe vars): lbl_80529B18 = \"FMVTrigger\" (interned name)"));
    CHECK(contains(text, "fragments (build/tools/fragments.exe in fn_800CA138): L113 +0x20 7 rows x82 fns"));
    CHECK(contains(text, "expected profile: unknown (no source unit to infer from)"));
    brief_set(d, "names", avp_json_arr(avp_arena()));
    brief_set(d, "fragments", avp_json_arr(avp_arena()));
    text = joined(brief_fmt_start(s, 0));
    CHECK(!contains(text, "known names"));
    CHECK(!contains(text, "fragments"));
}

AVP_TEST(context_caps_class_list) {
    AvpJson *d = jparse("{\"classes\": [], \"callers\": [], \"callees\": []}");
    AvpJson *cls = avp_json_getz(d, "classes");
    for (int i = 0; i < 20; i++) {
        AvpJson *c = jparse("{\"class\": \"CThing\", \"slots\": [[9, \"0x24\"]], \"roles\": [\"slot\"], "
                            "\"vtable\": \"lbl_803E0000\", \"name_source\": null, \"header\": null}");
        avp_json_arr_push(avp_arena(), cls, c);
    }
    BriefSection *s = section("context", d, brief_fmt_context, 5);
    BriefLines capped = brief_fmt_context(s, 0);
    BriefLines all = brief_fmt_context(s, -1);
    CHECK(contains(joined(capped), "... 14 more classes sharing the vtable slot (--full lists them all)"));
    CHECK(capped.n < 12);
    CHECK(!contains(joined(all), "more classes"));
    CHECK(all.n >= 21);
}

AVP_TEST(sibling_lines) {
    BriefLines L = {0};
    brief_sibling_fn_lines(&L, avp_json_null()); /* no database: nothing printed */
    brief_sibling_range_lines(&L, avp_json_null());
    CHECK(L.n == 0);
    brief_sibling_fn_lines(&L, jparse("{\"name\": \"threadFunc__Q24FMOD11AsyncThreadFv\", \"demangled\": "
                                      "\"FMOD::AsyncThread::threadFunc()\", \"tier\": \"exact\", \"lib\": \"fmodex.a\","
                                      " \"object\": \"fmod_async.obj\", \"order\": 8}"));
    REQUIRE(L.n == 1);
    CHECK(contains(L.v[0], "sibling (Gormiti): FMOD::AsyncThread::threadFunc()  exact, fmodex.a fmod_async.obj #8"));
    BriefLines R = {0};
    brief_sibling_range_lines(
        &R, jparse("[{\"lib\": \"fmodex.a\", \"object\": \"fmod.obj\", \"start\": 2150808148, \"end\": 2150809004,"
                   " \"paired\": 6, \"clean\": true, \"unused\": 324, \"relation\": \"equals\"},"
                   " {\"lib\": \"\", \"object\": \"x.obj\", \"start\": 1, \"end\": 2, \"paired\": 1, \"clean\": false,"
                   " \"unused\": 0, \"relation\": \"partly\"}]"));
    REQUIRE(R.n == 3);
    CHECK(contains(R.v[0], "2 object span(s) overlap this range"));
    CHECK(contains(R.v[1], "fmodex.a fmod.obj  0x8032BA54-0x8032BDAC  6 paired, clean, = clean span, 324 UNUSED"));
    CHECK(contains(R.v[2], "x.obj  0x00000001-0x00000002  1 paired, not clean, partly in the range, 0 UNUSED"));
}

AVP_TEST(sibling_same_named_line) {
    BriefLines L = {0};
    brief_sibling_fn_lines(&L, jparse("{\"same_named\": true, \"name\": \"load__Q24FMOD11EventGroupIFPPv\", \"lib\": "
                                      "\"fmod_event.a\", \"object\": \"fmod_eventgroupi.obj\", \"order\": 21, \"size\": "
                                      "4688, \"retail_size\": 4360, \"count\": 1}"));
    REQUIRE(L.n == 1);
    CHECK(contains(L.v[0], "no exact pair; same-named function exists (size 0x1250 vs 0x1108, fmod_event.a "
                           "fmod_eventgroupi.obj #21): build/tools/sibling.exe fn 'load__Q24FMOD11EventGroupIFPPv' --sibling"));
    BriefLines M = {0};
    brief_sibling_fn_lines(&M, jparse("{\"same_named\": true, \"name\": \"f__Fv\", \"lib\": \"\", \"object\": \"a.obj\","
                                      " \"order\": 1, \"size\": 16, \"retail_size\": 20, \"count\": 3}"));
    REQUIRE(M.n == 1);
    CHECK(contains(M.v[0], "3 same-named functions exist, the first (size 0x10 vs 0x14, a.obj #1)"));
}

AVP_TEST(sibling_range_proposal_lines) {
    BriefLines R = {0};
    /* the range is the proposed unit range; the paired span is smaller */
    brief_sibling_range_lines(
        &R, jparse("[{\"lib\": \"\", \"object\": \"a.obj\", \"start\": 4096, \"end\": 4352, \"paired\": 4, \"clean\": true,"
                   " \"unused\": 0, \"relation\": \"equals-proposed\", \"proposed\": true, \"prop_start\": 4096,"
                   " \"prop_end\": 4608, \"proposal_is_span\": false}]"));
    REQUIRE(R.n == 2);
    CHECK(contains(R.v[1], "= proposed unit range (sibling.exe units)"));
    BriefLines S = {0};
    /* the range is only the paired span: the proposal is printed */
    brief_sibling_range_lines(
        &S, jparse("[{\"lib\": \"\", \"object\": \"a.obj\", \"start\": 4096, \"end\": 4352, \"paired\": 4, \"clean\": true,"
                   " \"unused\": 0, \"relation\": \"equals\", \"proposed\": true, \"prop_start\": 4096,"
                   " \"prop_end\": 4608, \"proposal_is_span\": false}]"));
    REQUIRE(S.n == 2);
    CHECK(contains(S.v[1], "= clean span; proposed unit range 0x00001000-0x00001200 (sibling.exe units)"));
    BriefLines T = {0};
    brief_sibling_range_lines(
        &T, jparse("[{\"lib\": \"\", \"object\": \"a.obj\", \"start\": 4096, \"end\": 4352, \"paired\": 4, \"clean\": true,"
                   " \"unused\": 0, \"relation\": \"equals\", \"proposed\": true, \"prop_start\": 4096,"
                   " \"prop_end\": 4352, \"proposal_is_span\": true}]"));
    REQUIRE(T.n == 2);
    CHECK(contains(T.v[1], "= clean span = proposed unit range (sibling.exe units)"));
}

AVP_TEST(unavailable_section) {
    BriefSection *s = section("context", NULL, brief_fmt_context, 5);
    s->unavailable = "no build/index/asm.sqlite";
    BriefLines L = brief_section_lines(s, 0);
    REQUIRE(L.n == 1);
    CHECK(avp_str_eqz(L.v[0], "unavailable: no build/index/asm.sqlite"));
    AvpJson *j = brief_section_json(s, 0);
    CHECK(avp_json_is_null(avp_json_getz(j, "data")));
    CHECK(avp_str_eqz(avp_json_getz(j, "unavailable")->u.str, "no build/index/asm.sqlite"));
}

AVP_TEST_MAIN("brief")
