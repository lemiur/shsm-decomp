/* Text codecs and newline handling of open() against CPython 3.14 (fs_text_corpus.h). */
#include "avp/avp.h"
#include "avp/fs.h"
#include "avp/csv.h"
#include "avp/json.h"
#include "avp/fmt.h"
#include "avp/proc.h"
#include "avp/pool.h"
#include "avp/win.h"
#include "avp/test.h"

#include "fs_text_corpus.h"

static int g_fail;

static bool same(AvpStr s, const char *z, size_t n) { return s.n == n && memcmp(s.p, z, n) == 0; }

static void check_decode(const char *name, AvpEncoding enc, const AvpDecRow *rows, size_t count) {
    AvpArena *a = avp_arena_new(0);
    g_fail = 0;
    for (size_t i = 0; i < count && g_fail < 10; i++) {
        AvpStr out;
        AvpStr bytes = {rows[i].b, rows[i].n};
        bool ok = avp_decode_text(a, bytes, AVP_TEXT(enc, AVP_ERR_REPLACE, AVP_NL_LF), &out);
        if (!ok || !same(out, rows[i].replace, rows[i].repn)) {
            g_fail++;
            avp_test__fail(__FILE__, __LINE__, "%s replace case %zu (%zu bytes): got [%.*s] want [%s]", name, i, rows[i].n, (int)out.n, ok ? out.p : "", rows[i].replace);
        }
        ok = avp_decode_text(a, bytes, AVP_TEXT(enc, AVP_ERR_STRICT, AVP_NL_LF), &out);
        if (ok != (rows[i].strict_ok != 0)) {
            g_fail++;
            avp_test__fail(__FILE__, __LINE__, "%s strict case %zu: ok=%d want %d", name, i, ok, rows[i].strict_ok);
        } else if (ok && !same(out, rows[i].strict, rows[i].strn)) {
            g_fail++;
            avp_test__fail(__FILE__, __LINE__, "%s strict case %zu: text differs", name, i);
        } else if (!ok) {
            AvpStr msg = avp_io_err_str(a);
            if (avp_io_err()->kind != AVP_IOE_DECODE || !same(msg, rows[i].strict, rows[i].strn)) {
                g_fail++;
                avp_test__fail(__FILE__, __LINE__, "%s strict case %zu: error [%.*s] want [%s]", name, i, (int)msg.n, msg.p, rows[i].strict);
            }
        }
    }
    CHECK(g_fail == 0);
    avp_arena_free(a);
}

AVP_TEST(decode_utf8) { check_decode("utf-8", AVP_ENC_UTF8, k_dec_utf_8, AVP_ARRAY_LEN(k_dec_utf_8)); }
AVP_TEST(decode_utf8_sig) { check_decode("utf-8-sig", AVP_ENC_UTF8_SIG, k_dec_utf_8_sig, AVP_ARRAY_LEN(k_dec_utf_8_sig)); }
AVP_TEST(decode_ascii) { check_decode("ascii", AVP_ENC_ASCII, k_dec_ascii, AVP_ARRAY_LEN(k_dec_ascii)); }
AVP_TEST(decode_latin1) { check_decode("latin-1", AVP_ENC_LATIN1, k_dec_latin_1, AVP_ARRAY_LEN(k_dec_latin_1)); }

AVP_TEST(newline_modes) {
    AvpArena *a = avp_arena_new(0);
    g_fail = 0;
    for (size_t i = 0; i < AVP_ARRAY_LEN(k_nl) && g_fail < 10; i++) {
        AvpTextMode m = AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_STRICT, (AvpNewline)k_nl[i].nl);
        AvpStr data = {k_nl[i].data, 0};
        /* the corpus strings have no NUL: strlen is the length */
        data.n = strlen(k_nl[i].data);
        AvpStr text;
        bool ok = avp_decode_text(a, data, m, &text);
        if (!ok || !same(text, k_nl[i].read, strlen(k_nl[i].read))) {
            g_fail++;
            avp_test__fail(__FILE__, __LINE__, "newline case %zu (mode %d): read [%.*s] want [%s]", i, k_nl[i].nl, (int)text.n, text.p, k_nl[i].read);
            continue;
        }
        /* lines, joined with 0x1f */
        AvpFileLines it;
        AvpStr line;
        char *joined = (char *)avp_alloc(a, text.n * 2 + 64);
        size_t jn = 0, count = 0;
        avp_file_lines_init(&it, text, m.newline);
        while (avp_file_lines_next(&it, &line)) {
            if (count++) joined[jn++] = '\x1f';
            memcpy(joined + jn, line.p, line.n);
            jn += line.n;
        }
        if ((int)count != k_nl[i].nlines || strcmp(joined, k_nl[i].lines) != 0) {
            g_fail++;
            avp_test__fail(__FILE__, __LINE__, "newline case %zu (mode %d): %zu lines [%s] want %d [%s]", i, k_nl[i].nl, count, joined, k_nl[i].nlines, k_nl[i].lines);
        }
        /* writing: the text as written to a file */
        AvpStr enc;
        AvpStr orig = {k_nl[i].data, data.n};
        ok = avp_encode_text(a, orig, m, &enc);
        if (!ok || !same(enc, k_nl[i].written, strlen(k_nl[i].written))) {
            g_fail++;
            avp_test__fail(__FILE__, __LINE__, "newline case %zu (mode %d): written [%.*s] want [%s]", i, k_nl[i].nl, (int)enc.n, enc.p, k_nl[i].written);
        }
    }
    CHECK(g_fail == 0);
    avp_arena_free(a);
}

AVP_TEST(encode_latin1_ascii) {
    AvpArena *a = avp_arena_new(0);
    g_fail = 0;
    for (size_t i = 0; i < AVP_ARRAY_LEN(k_enc) && g_fail < 10; i++) {
        AvpStr text = {k_enc[i].text, strlen(k_enc[i].text)}, out;
        bool ok = avp_encode_text(a, text, AVP_TEXT((AvpEncoding)k_enc[i].enc, AVP_ERR_STRICT, AVP_NL_LF), &out);
        if (ok != (k_enc[i].ok != 0)) {
            g_fail++;
            avp_test__fail(__FILE__, __LINE__, "encode case %zu: ok=%d want %d", i, ok, k_enc[i].ok);
        } else if (ok && !same(out, k_enc[i].out, k_enc[i].outn)) {
            g_fail++;
            avp_test__fail(__FILE__, __LINE__, "encode case %zu: bytes differ", i);
        } else if (!ok) {
            AvpStr msg = avp_io_err_str(a);
            if (avp_io_err()->kind != AVP_IOE_ENCODE || !same(msg, k_enc[i].out, k_enc[i].outn)) {
                g_fail++;
                avp_test__fail(__FILE__, __LINE__, "encode case %zu: error [%.*s] want [%s]", i, (int)msg.n, msg.p, k_enc[i].out);
            }
        }
        ok = avp_encode_text(a, text, AVP_TEXT((AvpEncoding)k_enc[i].enc, AVP_ERR_REPLACE, AVP_NL_LF), &out);
        if (!ok || !same(out, k_enc[i].replace, k_enc[i].repn)) {
            g_fail++;
            avp_test__fail(__FILE__, __LINE__, "encode case %zu: replace differs", i);
        }
    }
    CHECK(g_fail == 0);
    avp_arena_free(a);
}

AVP_TEST(utf8_sig_bom_is_written_once) {
    AvpArena *a = avp_arena_new(0);
    AvpStr out;
    CHECK(avp_encode_text(a, (AvpStr){"x", 1}, AVP_TM_UTF8_SIG, &out));
    CHECK(same(out, "\xEF\xBB\xBF" "x\r\n" + 0, 4) || same(out, "\xEF\xBB\xBFx", 4));
    avp_arena_free(a);
}

AVP_TEST_MAIN("fs_text")
