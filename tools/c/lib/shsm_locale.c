#include "shsm_locale.h"
#include <windows.h>

/* cp1252 0x80..0x9F; 0 marks the five undefined bytes. */
static const u16 cp1252_high[32] = {
    0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0,      0x017D, 0,
    0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0,      0x017E, 0x0178};

unsigned shsm_locale_codepage(void) {
    const char *u = avp_getenv(avp_scratch(), "PYTHONUTF8");
    if (u && strcmp(u, "1") == 0) return 65001;
    return GetACP();
}

static bool has_surrogate(AvpStr s) {
    for (size_t i = 0; i + 1 < s.n; i++)
        if ((u8)s.p[i] == 0xED && ((u8)s.p[i + 1] & 0xE0) == 0xA0) return true;
    return false;
}

static bool decode_cp1252(AvpArena *a, AvpStr bytes, AvpStr *utf8) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < bytes.n; i++) {
        u8 c = (u8)bytes.p[i];
        u32 cp = c;
        if (c >= 0x80 && c < 0xA0) {
            cp = cp1252_high[c - 0x80];
            if (!cp) {
                avp_io_fail_msg(AVP_IOE_DECODE, 0, "'charmap' codec can't decode byte 0x%02x in position %zu: character maps to <undefined>", c, i);
                return false;
            }
        }
        avp_buf_putcp(&b, cp);
    }
    *utf8 = avp_buf_str(&b);
    return true;
}

bool shsm_locale_read_text(AvpArena *a, const char *path, AvpStr *out) {
    AvpBytes raw = {0};
    if (!avp_read_all(a, path, &raw)) return false;
    AvpStr bytes = {(const char *)raw.v, raw.n};
    unsigned cp = shsm_locale_codepage();
    if (cp == 65001) return avp_decode_text(a, bytes, AVP_TM_UTF8, out);
    if (cp == 1252) {
        AvpStr u;
        if (!decode_cp1252(a, bytes, &u)) return false;
        return avp_decode_text(a, u, AVP_TM_UTF8, out);
    }
    return avp_decode_text(a, bytes, AVP_TEXT(AVP_ENC_MBCS, AVP_ERR_STRICT, AVP_NL_DEFAULT), out);
}

static void fail_encode(unsigned cpv, size_t pos, const char *codec) {
    avp_io_fail_msg(AVP_IOE_ENCODE, 0, "'%s' codec can't encode character '\\u%04x' in position %zu: %s", codec, cpv, pos,
                    strcmp(codec, "utf-8") == 0 ? "surrogates not allowed" : "character maps to <undefined>");
}

bool shsm_locale_encode(AvpArena *a, AvpStr text, AvpStr *out) {
    unsigned cp = shsm_locale_codepage();
    if (cp == 65001) {
        if (has_surrogate(text)) {
            fail_encode(0xD800, 0, "utf-8");
            return false;
        }
        *out = text;
        return true;
    }
    AvpBuf b;
    avp_buf_init(&b, a);
    const char *p = text.p, *end = text.p + text.n;
    size_t pos = 0;
    if (cp == 1252) {
        while (p < end) {
            u32 c = avp_u8_next(&p, end);
            int byte = -1;
            if (c < 0x80 || (c >= 0xA0 && c <= 0xFF)) byte = (int)c;
            else
                for (int k = 0; k < 32; k++)
                    if (cp1252_high[k] == c) byte = 0x80 + k;
            if (byte < 0) {
                fail_encode(c, pos, "charmap");
                return false;
            }
            avp_buf_putc(&b, (char)byte);
            pos++;
        }
        *out = avp_buf_str(&b);
        return true;
    }
    if (has_surrogate(text)) {
        fail_encode(0xD800, 0, "charmap");
        return false;
    }
    wchar_t *w = avp_to_w(a, text);
    int wl = (int)wcslen(w);
    BOOL used = FALSE;
    int bl = wl ? WideCharToMultiByte(cp, WC_NO_BEST_FIT_CHARS, w, wl, NULL, 0, NULL, &used) : 0;
    char *buf = AVP_NEWN(a, char, (size_t)bl + 1);
    if (bl) WideCharToMultiByte(cp, WC_NO_BEST_FIT_CHARS, w, wl, buf, bl, NULL, &used);
    if (used) {
        fail_encode(0xFFFD, 0, "charmap");
        return false;
    }
    *out = avp_strn(buf, (size_t)bl);
    return true;
}

bool shsm_write_utf8_text(const char *path, AvpStr text) {
    if (has_surrogate(text)) {
        /* open(path, "w") has created (or emptied) the file when the write fails */
        if (!avp_write_file(path, "", 0)) return false;
        fail_encode(0xD800, 0, "utf-8");
        return false;
    }
    return avp_write_text(path, text, AVP_TM_UTF8);
}

void shsm_py_traceback(const char *script, const char *cls, const char *fmt, ...) {
    fflush(stdout);
    fprintf(stderr, "%s: error: ", script);
    va_list ap;
    va_start(ap, fmt);
    char text[1024];
    vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    fprintf(stderr, "%s%s%s\n", cls, text[0] ? ": " : "", text);
    avp_exit(1);
}

void shsm_py_traceback_io(const char *script) {
    const char *cls = avp_io_err_class();
    AvpStr s = avp_io_err_str(avp_scratch());
    shsm_py_traceback(script, cls, "%.*s", AVP_SF(s));
}
