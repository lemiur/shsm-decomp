/* repr() of str and of containers built from item reprs (Objects/unicodeobject.c unicode_repr, listobject.c,
 * tupleobject.c, dictobject.c, setobject.c). */
#include "avp/fmt.h"

void avp_buf_repr_str(AvpBuf *b, AvpStr s) {
    bool squote = false, dquote = false;
    for (size_t i = 0; i < s.n; i++) {
        if (s.p[i] == '\'') squote = true;
        else if (s.p[i] == '"') dquote = true;
    }
    /* Single quotes unless the text has a ' and no ": then double quotes. */
    char quote = (squote && !dquote) ? '"' : '\'';
    avp_buf_putc(b, quote);
    const char *p = s.p, *end = s.p + s.n;
    static const char hex[] = "0123456789abcdef";
    while (p < end) {
        const char *at = p;
        u32 cp = avp_u8_next(&p, end);
        if (cp == (u32)quote || cp == '\\') {
            avp_buf_putc(b, '\\');
            avp_buf_putc(b, (char)cp);
        } else if (cp == '\t') {
            avp_buf_putn(b, "\\t", 2);
        } else if (cp == '\n') {
            avp_buf_putn(b, "\\n", 2);
        } else if (cp == '\r') {
            avp_buf_putn(b, "\\r", 2);
        } else if (cp < 0x20 || cp == 0x7f) {
            char e[4] = {'\\', 'x', hex[cp >> 4], hex[cp & 15]};
            avp_buf_putn(b, e, 4);
        } else if (cp < 0x7f) {
            avp_buf_putc(b, (char)cp);
        } else if (avp_cp_isprintable(cp)) {
            avp_buf_putn(b, at, (size_t)(p - at));
        } else {
            char e[10];
            size_t n = 0;
            e[n++] = '\\';
            int digits;
            if (cp <= 0xff) {
                e[n++] = 'x';
                digits = 2;
            } else if (cp <= 0xffff) {
                e[n++] = 'u';
                digits = 4;
            } else {
                e[n++] = 'U';
                digits = 8;
            }
            for (int sh = (digits - 1) * 4; sh >= 0; sh -= 4) e[n++] = hex[(cp >> sh) & 15];
            avp_buf_putn(b, e, n);
        }
    }
    avp_buf_putc(b, quote);
}

static AvpStr finish(AvpBuf *b) {
    char *z = avp_buf_cz(b);
    return avp_strn(z, b->n);
}

AvpStr avp_repr_str(AvpArena *a, AvpStr s) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_repr_str(&b, s);
    return finish(&b);
}

AvpStr avp_repr_i64(AvpArena *a, i64 v) {
    char d[64];
    size_t n = avp_u64_digits(d, v < 0 ? 0 - (u64)v : (u64)v, 10, false);
    AvpBuf b;
    avp_buf_init(&b, a);
    if (v < 0) avp_buf_putc(&b, '-');
    avp_buf_putn(&b, d, n);
    return finish(&b);
}

AvpStr avp_repr_bool(bool v) { return v ? AVP_S("True") : AVP_S("False"); }

static AvpStr seq(AvpArena *a, char open, char close, const AvpStr *items, size_t n, bool one_comma) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putc(&b, open);
    for (size_t i = 0; i < n; i++) {
        if (i) avp_buf_putn(&b, ", ", 2);
        avp_buf_put(&b, items[i]);
    }
    if (n == 1 && one_comma) avp_buf_putc(&b, ',');
    avp_buf_putc(&b, close);
    return finish(&b);
}

AvpStr avp_repr_list(AvpArena *a, const AvpStr *items, size_t n) { return seq(a, '[', ']', items, n, false); }
AvpStr avp_repr_tuple(AvpArena *a, const AvpStr *items, size_t n) { return seq(a, '(', ')', items, n, true); }

AvpStr avp_repr_set(AvpArena *a, const AvpStr *items, size_t n) {
    if (n == 0) return avp_str_dup(a, AVP_S("set()"));
    return seq(a, '{', '}', items, n, false);
}

AvpStr avp_repr_dict(AvpArena *a, const AvpStr *keys, const AvpStr *vals, size_t n) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putc(&b, '{');
    for (size_t i = 0; i < n; i++) {
        if (i) avp_buf_putn(&b, ", ", 2);
        avp_buf_put(&b, keys[i]);
        avp_buf_putn(&b, ": ", 2);
        avp_buf_put(&b, vals[i]);
    }
    avp_buf_putc(&b, '}');
    return finish(&b);
}
