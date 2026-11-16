/* The lever catalogue as data, for the differential check of generation against the Python (diff/gen_check.py).
 *
 * With LEVERS_GEN_LIST=<file of "path<TAB>symbol" lines> and LEVERS_GEN_OUT=<output file> in the environment, writes one
 * JSON line per function: {file, symbol, error, forms, cands: [[family, lever, line, sha1, desc, span, fake]]}, the
 * same as gen_check.py writes from tools/levers.py. Without them the test does nothing (CTest runs it that way). */
#include <stdlib.h>
#include <string.h>

#include "avp/avp.h"
#include "avp/test.h"
#include "levers_search.h"

static AvpJson *counter_json(AvpArena *a, const LvCounter *c) {
    AvpJson *o = avp_json_obj(a);
    /* sorted keys */
    const char *keys[64];
    size_t n = 0;
    for (size_t i = 0; i < c->n && n < 64; i++) keys[n++] = c->v[i].k;
    for (size_t i = 1; i < n; i++)
        for (size_t j = i; j > 0 && strcmp(keys[j - 1], keys[j]) > 0; j--) {
            const char *t = keys[j];
            keys[j] = keys[j - 1];
            keys[j - 1] = t;
        }
    for (size_t i = 0; i < n; i++) avp_json_set(a, o, avp_str(keys[i]), avp_json_int(a, lv_counter_get(c, keys[i])));
    return o;
}

static void dump_one(AvpArena *a, AvpBuf *out, const char *path, const char *symbol) {
    AvpBytes b = {0};
    AvpJson *row = avp_json_obj(a);
    avp_json_set(a, row, AVP_S("file"), avp_json_strz(a, path));
    avp_json_set(a, row, AVP_S("symbol"), avp_json_strz(a, symbol));
    if (!avp_read_all(a, path, &b)) {
        avp_json_set(a, row, AVP_S("error"), avp_json_strz(a, "OSError"));
    } else {
        LvS text = {(const char *)b.v, b.n};
        AvpTry t;
        if (AVP_TRY(&t)) {
            LvCtx *x = lv_ctx_new(a, text, symbol);
            LvCounter forms = lv_fake_forms(a, lv_slice(x, x->span0, x->span1), symbol);
            LvCandidates gen = lv_generate(x, NULL, 0);
            AvpJson *cands = avp_json_arr(a);
            for (size_t i = 0; i < gen.n; i++) {
                LvCandidate *cd = &gen.v[i];
                AvpJson *c = avp_json_arr(a);
                avp_json_arr_push(a, c, avp_json_strz(a, cd->family));
                avp_json_arr_push(a, c, avp_json_strz(a, cd->lever));
                avp_json_arr_push(a, c, avp_json_int(a, cd->line));
                avp_json_arr_push(a, c, avp_json_strz(a, lv_sha(a, cd->text)));
                avp_json_arr_push(a, c, avp_json_strz(a, lv_utf8(a, cd->desc)));
                AvpJson *sp = avp_json_arr(a);
                avp_json_arr_push(a, sp, avp_json_int(a, cd->span0));
                avp_json_arr_push(a, sp, avp_json_int(a, cd->span1));
                avp_json_arr_push(a, c, sp);
                LvS fn = {cd->text.p + cd->span0, (size_t)(cd->span1 - cd->span0)};
                AvpCzVec why = lv_fake_reasons(a, &forms, fn, symbol);
                AvpJson *w = avp_json_arr(a);
                for (size_t k = 0; k < why.n; k++) avp_json_arr_push(a, w, avp_json_strz(a, why.v[k]));
                avp_json_arr_push(a, c, w);
                avp_json_arr_push(a, cands, c);
            }
            avp_json_set(a, row, AVP_S("error"), avp_json_null());
            avp_json_set(a, row, AVP_S("forms"), counter_json(a, &forms));
            avp_json_set(a, row, AVP_S("cands"), cands);
            avp_try_end(&t);
        } else {
            const char *name = lv_exc_name(t.code);
            avp_json_set(a, row, AVP_S("error"), avp_json_strz(a, name ? name : "other"));
        }
    }
    AvpJsonDumpOpts o = {0};
    avp_json_dump_buf(out, row, &o);
    avp_buf_putc(out, '\n');
}

AVP_TEST(gen_dump) {
    const char *list = getenv("LEVERS_GEN_LIST"), *outp = getenv("LEVERS_GEN_OUT");
    if (!list || !outp) return;
    AvpArena *a = avp_arena();
    AvpStr text;
    REQUIRE(avp_read_text(a, list, AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_STRICT, AVP_NL_DEFAULT), &text));
    AvpBuf out;
    avp_buf_init(&out, a);
    AvpStrVec lines = avp_str_splitlines(text, false);
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr before, after;
        if (!avp_str_partition(lines.v[i], AVP_S("\t"), &before, &after)) continue;
        dump_one(a, &out, avp_cz(a, before), avp_cz(a, after));
    }
    REQUIRE(avp_write_file(outp, out.p, out.n));
}

AVP_TEST_MAIN("levers_gen")
