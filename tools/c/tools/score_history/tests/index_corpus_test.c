/* The source index over a corpus, for diff/index_corpus.py: with SH_INDEX_DUMP=<file list> in the environment it
 * writes, per listed file, one line `<path>\t<json>` (the cached index dict plus the {hash: text} map of
 * index_source) to SH_INDEX_OUT. Without it, the test indexes one small file and checks the result is stable. */
#include "sh_priv.h"

#include "avp/test.h"

#include <stdio.h>
#include <string.h>

static AvpStr dump_one(AvpArena *a, AvpStr bytes) {
    ShIndex *ix = sh_index_source(a, sh_decode_utf8(a, bytes));
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, "{\"index\": ");
    avp_buf_put(&b, sh_index_to_json(a, ix));
    avp_buf_putz(&b, ", \"texts\": {");
    for (size_t i = 0; i < ix->ntexts; i++) {
        if (i) avp_buf_putz(&b, ", ");
        avp_json_put_string(&b, ix->text_hash[i], false);
        avp_buf_putz(&b, ": ");
        avp_json_put_string(&b, ix->text[i], false);
    }
    avp_buf_putz(&b, "}}");
    return avp_buf_str(&b);
}

AVP_TEST(index_corpus) {
    AvpArena *a = avp_arena();
    const char *list = avp_getenv_or(a, "SH_INDEX_DUMP", NULL);
    const char *outp = avp_getenv_or(a, "SH_INDEX_OUT", NULL);
    if (!list || !outp) {
        AvpStr x = dump_one(a, AVP_S("int f(void) { return 1; } // 0x80000004\n"));
        CHECK(avp_str_eq(x, dump_one(a, AVP_S("int f(void) {\n  return 1;\n} // 0x80000004\n"))) == false);
        CHECK(avp_str_contains(x, AVP_S("\"addr\": 2147483652")));
        return;
    }
    AvpStrVec files = {0};
    REQUIRE(avp_read_lines(a, list, (AvpTextMode){0}, &files));
    FILE *f = fopen(outp, "wb");
    REQUIRE(f != NULL);
    for (size_t i = 0; i < files.n; i++) {
        AvpStr p = avp_str_rstrip(files.v[i]);
        if (!p.n) continue;
        AvpArenaMark mk = avp_arena_mark(a);
        AvpBytes data = {0};
        char *pz = avp_cz(a, p);
        if (!avp_read_all(a, pz, &data)) continue;
        AvpTry t;
        AvpStr line;
        if (AVP_TRY(&t)) {
            line = dump_one(a, avp_strn((const char *)data.v, data.n));
            avp_try_end(&t);
        } else {
            line = avp_str(avp_sprintf(a, "{\"error\": \"%s\"}", t.code == SH_EXC_CRASH ? "crash" : "other"));
        }
        fprintf(f, "%s\t%.*s\n", pz, AVP_SF(line));
        avp_arena_reset(a, mk);
    }
    fclose(f);
}

AVP_TEST_MAIN("score_history")
