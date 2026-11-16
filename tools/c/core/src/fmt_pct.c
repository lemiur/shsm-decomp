/* Python's % operator for C arguments (see the argument convention in fmt.h). A port of the field handling of
 * CPython 3.14's Objects/unicodeobject.c (unicode_format_arg_*): numbers get sign, prefix, zero extension to the
 * precision and then the width, with the 0 flag filling after the sign and the prefix and the - flag overriding it;
 * strings pad with spaces and count code points. */
#include "avp/fmt.h"
#include <limits.h>
#include <stdarg.h>

#ifndef _M_X64
#error "fmt_pct.c reads the widened variadic arguments as 64-bit values (x64 only)"
#endif

typedef struct {
    bool ljust, sign, blank, alt, zero;
    i64 width, prec; /* -1: absent */
} Pct;

static void pad(AvpBuf *b, char c, i64 n) {
    for (; n > 0; n--) avp_buf_putc(b, c);
}

/* A number: sign char (0 for none), prefix, then the body. extra_zeros is the precision zero extension of an integer. */
static void put_number(AvpBuf *b, const Pct *s, char sign, const char *prefix, const char *body, size_t n_body,
                       i64 extra_zeros) {
    size_t n_prefix = prefix ? strlen(prefix) : 0;
    i64 len = (sign ? 1 : 0) + (i64)n_prefix + extra_zeros + (i64)n_body;
    i64 fill = s->width > len ? s->width - len : 0;
    if (s->ljust) {
        if (sign) avp_buf_putc(b, sign);
        if (n_prefix) avp_buf_putn(b, prefix, n_prefix);
        pad(b, '0', extra_zeros);
        avp_buf_putn(b, body, n_body);
        pad(b, ' ', fill);
    } else if (s->zero) {
        if (sign) avp_buf_putc(b, sign);
        if (n_prefix) avp_buf_putn(b, prefix, n_prefix);
        pad(b, '0', fill + extra_zeros);
        avp_buf_putn(b, body, n_body);
    } else {
        pad(b, ' ', fill);
        if (sign) avp_buf_putc(b, sign);
        if (n_prefix) avp_buf_putn(b, prefix, n_prefix);
        pad(b, '0', extra_zeros);
        avp_buf_putn(b, body, n_body);
    }
}

/* Text: precision cuts to that many code points, width pads with spaces, counted in code points. */
static void put_text(AvpBuf *b, const Pct *s, AvpStr t) {
    if (s->prec >= 0) t = avp_u8_slice(t, 0, s->prec);
    i64 len = (i64)avp_u8_len(t);
    i64 fill = s->width > len ? s->width - len : 0;
    if (!s->ljust) pad(b, ' ', fill);
    avp_buf_put(b, t);
    if (s->ljust) pad(b, ' ', fill);
}

static char sign_char(const Pct *s, bool neg) {
    if (neg) return '-';
    if (s->sign) return '+';
    if (s->blank) return ' ';
    return 0;
}

static void put_integer(AvpBuf *b, const Pct *s, char conv, bool neg, u64 mag) {
    int base = 10;
    const char *prefix = NULL;
    switch (conv) {
    case 'x': base = 16; prefix = "0x"; break;
    case 'X': base = 16; prefix = "0X"; break;
    case 'o': base = 8; prefix = "0o"; break;
    default: break;
    }
    if (!s->alt) prefix = NULL;
    char digits[64];
    size_t n = avp_u64_digits(digits, mag, base, conv == 'X');
    i64 extra = s->prec > (i64)n ? s->prec - (i64)n : 0;
    put_number(b, s, sign_char(s, neg), prefix, digits, n, extra);
}

static void put_float(AvpBuf *b, const Pct *s, char conv, double x) {
    AvpBuf t = {0}; /* a heap builder: the output builder b may grow while t is read */
    int prec = s->prec < 0 ? 6 : (s->prec > INT_MAX ? INT_MAX : (int)s->prec);
    avp_float_to_string(&t, x, conv, prec, s->alt ? AVP_DTSF_ALT : 0);
    const char *z = avp_buf_cz(&t);
    bool neg = z[0] == '-';
    put_number(b, s, sign_char(s, neg), NULL, z + neg, t.n - (neg ? 1u : 0u), 0);
    avp_buf_free(&t);
}

static i64 read_int(const char **p) {
    i64 v = 0;
    while (**p >= '0' && **p <= '9') {
        if (v > (INT64_MAX - 9) / 10) avp_panic(__FILE__, __LINE__, "pyfmt: width or precision too large");
        v = v * 10 + (**p - '0');
        (*p)++;
    }
    return v;
}

void avp_buf_pyvfmt(AvpBuf *b, const char *fmt, va_list ap) {
    const char *p = fmt;
    while (*p) {
        if (*p != '%') {
            const char *q = strchr(p, '%');
            size_t n = q ? (size_t)(q - p) : strlen(p);
            avp_buf_putn(b, p, n);
            p += n;
            continue;
        }
        p++;
        Pct s = {false, false, false, false, false, -1, -1};
        for (;; p++) {
            if (*p == '-') s.ljust = true;
            else if (*p == '+') s.sign = true;
            else if (*p == ' ') s.blank = true;
            else if (*p == '#') s.alt = true;
            else if (*p == '0') s.zero = true;
            else break;
        }
        if (*p == '*') {
            long long w = va_arg(ap, long long);
            if (w < 0) {
                s.ljust = true;
                w = -w;
            }
            s.width = w;
            p++;
        } else if (*p >= '0' && *p <= '9') {
            s.width = read_int(&p);
        }
        if (*p == '.') {
            p++;
            if (*p == '*') {
                long long v = va_arg(ap, long long);
                s.prec = v < 0 ? 0 : v;
                p++;
            } else {
                s.prec = read_int(&p);
            }
        }
        bool unsigned_arg = false;
        for (;; p++) {
            if (*p == 'h' || *p == 'l' || *p == 'L') continue;
            if (*p == 'U') {
                unsigned_arg = true;
                continue;
            }
            break;
        }
        char conv = *p;
        if (!conv) avp_panic(__FILE__, __LINE__, "pyfmt: incomplete format in \"%s\"", fmt);
        p++;
        if (s.ljust) s.zero = false;
        switch (conv) {
        case '%':
            avp_buf_putc(b, '%');
            break;
        case 'd': case 'i': case 'u': case 'x': case 'X': case 'o': {
            bool neg = false;
            u64 mag;
            if (unsigned_arg || conv == 'u') {
                mag = va_arg(ap, unsigned long long);
            } else {
                long long v = va_arg(ap, long long);
                neg = v < 0;
                mag = neg ? 0 - (u64)v : (u64)v;
            }
            put_integer(b, &s, conv == 'i' || conv == 'u' ? 'd' : conv, neg, mag);
            break;
        }
        case 'c': {
            long long v = va_arg(ap, long long);
            if (v < 0 || v > 0x10ffff) avp_panic(__FILE__, __LINE__, "pyfmt: %%c arg not in range(0x110000)");
            char tmp[4];
            size_t n = avp_u8_put(tmp, (u32)v);
            Pct t = s;
            t.prec = -1;
            put_text(b, &t, avp_strn(tmp, n));
            break;
        }
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
            put_float(b, &s, conv, va_arg(ap, double));
            break;
        case 'v': {
            char tmp[32];
            size_t n = avp_float_repr(va_arg(ap, double), tmp);
            put_text(b, &s, avp_strn(tmp, n));
            break;
        }
        case 's': {
            const char *z = va_arg(ap, const char *);
            put_text(b, &s, z ? avp_str(z) : AVP_S("None"));
            break;
        }
        case 'S': {
            const AvpStr *t = va_arg(ap, const AvpStr *);
            put_text(b, &s, *t);
            break;
        }
        case 'r': case 'R': {
            AvpStr t;
            if (conv == 'r') {
                const char *z = va_arg(ap, const char *);
                t = z ? avp_str(z) : AVP_S("");
            } else {
                t = *va_arg(ap, const AvpStr *);
            }
            AvpBuf rb = {0};
            avp_buf_repr_str(&rb, t);
            put_text(b, &s, avp_buf_str(&rb));
            avp_buf_free(&rb);
            break;
        }
        default:
            avp_panic(__FILE__, __LINE__, "pyfmt: unsupported conversion '%c' in \"%s\"", conv, fmt);
        }
    }
}

AvpStr avp_pyvfmt(AvpArena *a, const char *fmt, va_list ap) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_pyvfmt(&b, fmt, ap);
    char *z = avp_buf_cz(&b);
    return avp_strn(z, b.n);
}

void avp_buf_pyfmt_(AvpBuf *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    avp_buf_pyvfmt(b, fmt, ap);
    va_end(ap);
}

AvpStr avp_pyfmt_(AvpArena *a, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    AvpStr r = avp_pyvfmt(a, fmt, ap);
    va_end(ap);
    return r;
}

static size_t write_out(FILE *f, const char *fmt, va_list ap) {
    AvpArena *sc = avp_scratch();
    AvpArenaMark mark = avp_arena_mark(sc);
    AvpBuf b;
    avp_buf_init(&b, sc);
    avp_buf_pyvfmt(&b, fmt, ap);
    size_t n = b.n;
    if (n) fwrite(b.p, 1, n, f);
    avp_arena_reset(sc, mark);
    return n;
}

size_t avp_pyprintf_(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    size_t n = write_out(stdout, fmt, ap);
    va_end(ap);
    return n;
}

size_t avp_pyfprintf_(FILE *f, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    size_t n = write_out(f, fmt, ap);
    va_end(ap);
    return n;
}
