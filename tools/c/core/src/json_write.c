/* avpcore json writer: json.dumps / json.dump byte for byte (the escapes, separators, indent, sort_keys and float
 * rules of Lib/json/encoder.py), as a tree walk and as a streaming writer. */
#include "json_internal.h"
#include "avp/arena.h"
#include "avp/fmt.h"
#include <math.h>
#include <stdlib.h>

/* ---- low-level output: writes straight into the AvpBuf's memory ---- */
static char *room(AvpBuf *b, size_t n) {
    avp_buf_reserve(b, n + 1);
    return b->p + b->n;
}

static void commit(AvpBuf *b, size_t n) {
    b->n += n;
    b->p[b->n] = 0;
}

static void put(AvpBuf *b, const char *s, size_t n) {
    if (!n) return;
    memcpy(room(b, n), s, n);
    commit(b, n);
}

static const char HEX[] = "0123456789abcdef";

static size_t put_u_escape(char *d, unsigned v) {
    d[0] = '\\';
    d[1] = 'u';
    d[2] = HEX[(v >> 12) & 15];
    d[3] = HEX[(v >> 8) & 15];
    d[4] = HEX[(v >> 4) & 15];
    d[5] = HEX[v & 15];
    return 6;
}

/* Decodes one code point of generalized UTF-8 (surrogates allowed) at p; invalid bytes give U+FFFD, one byte. */
static u32 next_cp(const unsigned char **pp, const unsigned char *end) {
    const unsigned char *p = *pp;
    unsigned c = *p;
    if (c < 0x80) {
        *pp = p + 1;
        return c;
    }
    if (c >= 0xC2 && c <= 0xDF && p + 1 < end && (p[1] & 0xC0) == 0x80) {
        *pp = p + 2;
        return ((c & 0x1F) << 6) | (p[1] & 0x3F);
    }
    if (c >= 0xE0 && c <= 0xEF && p + 2 < end && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        u32 cp = ((c & 0x0F) << 12) | ((u32)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        if (cp >= 0x800) {
            *pp = p + 3;
            return cp;
        }
    }
    if (c >= 0xF0 && c <= 0xF4 && p + 3 < end && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
        u32 cp = ((c & 0x07) << 18) | ((u32)(p[1] & 0x3F) << 12) | ((u32)(p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        if (cp >= 0x10000 && cp <= 0x10FFFF) {
            *pp = p + 4;
            return cp;
        }
    }
    *pp = p + 1;
    return 0xFFFD;
}

/* One piece of json.dumps(s): the escapes of py_encode_basestring_ascii (or py_encode_basestring when raw_unicode).
 * The piece must not end inside a UTF-8 sequence. */
static void put_string_piece(AvpBuf *out, const unsigned char *p, const unsigned char *end, bool raw_unicode) {
    /* worst case: every byte becomes a 6-byte \u00XX escape, a 4-byte code point two escapes (12) */
    char *d = room(out, (size_t)(end - p) * 6);
    char *start = d;
    while (p < end) {
        unsigned c = *p;
        if (c >= 0x20 && c != '"' && c != '\\' && c < 0x7F) {
            const unsigned char *q = p + 1;
            while (q < end && *q >= 0x20 && *q != '"' && *q != '\\' && *q < 0x7F) q++;
            memcpy(d, p, (size_t)(q - p));
            d += q - p;
            p = q;
            continue;
        }
        if (c < 0x80) {
            p++;
            switch (c) {
            case '"': *d++ = '\\'; *d++ = '"'; break;
            case '\\': *d++ = '\\'; *d++ = '\\'; break;
            case '\n': *d++ = '\\'; *d++ = 'n'; break;
            case '\r': *d++ = '\\'; *d++ = 'r'; break;
            case '\t': *d++ = '\\'; *d++ = 't'; break;
            case '\b': *d++ = '\\'; *d++ = 'b'; break;
            case '\f': *d++ = '\\'; *d++ = 'f'; break;
            case 0x7F:
                if (raw_unicode) *d++ = (char)c;
                else d += put_u_escape(d, c);
                break;
            default: d += put_u_escape(d, c); break;
            }
            continue;
        }
        if (raw_unicode) {
            /* non-ASCII bytes pass through unchanged (valid UTF-8, lone surrogates in their 3-byte form) */
            const unsigned char *q = p + 1;
            while (q < end && *q >= 0x80) q++;
            memcpy(d, p, (size_t)(q - p));
            d += q - p;
            p = q;
            continue;
        }
        u32 cp = next_cp(&p, end);
        if (cp >= 0x10000) {
            cp -= 0x10000;
            d += put_u_escape(d, 0xD800 | (cp >> 10));
            d += put_u_escape(d, 0xDC00 | (cp & 0x3FF));
        } else {
            d += put_u_escape(d, cp);
        }
    }
    commit(out, (size_t)(d - start));
}

/* json.dumps(s) (ensure_ascii) or json.dumps(s, ensure_ascii=False), quotes included. */
void avp_json_put_string(AvpBuf *out, AvpStr s, bool raw_unicode) {
    const unsigned char *p = (const unsigned char *)s.p, *end = p + s.n;
    put(out, "\"", 1);
    while (p < end) {
        const unsigned char *q = (size_t)(end - p) > 4096 ? p + 4096 : end;
        while (q < end && (*q & 0xC0) == 0x80) q++;
        put_string_piece(out, p, q, raw_unicode);
        p = q;
    }
    put(out, "\"", 1);
}

static void put_i64(AvpBuf *b, i64 v) {
    char tmp[24];
    char *e = tmp + sizeof tmp, *p = e;
    u64 u = v < 0 ? 0u - (u64)v : (u64)v;
    do {
        *--p = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    if (v < 0) *--p = '-';
    put(b, p, (size_t)(e - p));
}

static void put_u64(AvpBuf *b, u64 u) {
    char tmp[24];
    char *e = tmp + sizeof tmp, *p = e;
    do {
        *--p = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    put(b, p, (size_t)(e - p));
}

/* ---- the streaming writer ---- */
enum { ST_ARRAY = 1, ST_OBJECT = 2, ST_HAS_ITEM = 0x80, ST_KEY_PENDING = 0x40, ST_KIND = 0x3F };

void avp_jsonw_init(AvpJsonW *w, AvpBuf *out, const AvpJsonDumpOpts *opts) {
    memset(w, 0, sizeof *w);
    w->out = out;
    if (opts) w->o = *opts;
    if (w->o.custom_separators) {
        w->item_sep = w->o.item_sep;
        w->key_sep = w->o.key_sep;
    } else {
        w->item_sep = w->o.indented ? AVP_S(",") : AVP_S(", ");
        w->key_sep = AVP_S(": ");
    }
    if (w->o.indented) {
        if (w->o.indent.p) {
            w->indent_unit = w->o.indent;
        } else {
            /* ' ' * width: a static run of spaces long enough for any sane width, else built in the buffer on use */
            w->indent_unit = (AvpStr){NULL, w->o.indent_width > 0 ? (size_t)w->o.indent_width : 0};
        }
    }
    w->stack = w->inline_stack;
    w->stack_cap = sizeof w->inline_stack;
}

void avp_jsonw_set_spill(AvpJsonW *w, size_t threshold, AvpJsonSpillFn fn, void *ctx) {
    w->spill_threshold = threshold;
    w->spill = fn;
    w->spill_ctx = ctx;
}

static void maybe_spill(AvpJsonW *w) {
    if (w->spill && w->out->n >= w->spill_threshold) {
        if (!w->spill(w->spill_ctx, w->out->p, w->out->n)) w->failed = true;
        avp_buf_clear(w->out);
    }
}

bool avp_jsonw_finish(AvpJsonW *w) {
    if (w->spill && w->out->n) {
        if (!w->spill(w->spill_ctx, w->out->p, w->out->n)) w->failed = true;
        avp_buf_clear(w->out);
    }
    if (w->stack != w->inline_stack) avp_xfree(w->stack);
    w->stack = w->inline_stack;
    w->stack_cap = sizeof w->inline_stack;
    return !w->failed;
}

static void put_newline_indent(AvpJsonW *w, size_t levels) {
    AvpBuf *b = w->out;
    size_t unit = w->indent_unit.n;
    char *d = room(b, 1 + unit * levels);
    *d++ = '\n';
    if (w->indent_unit.p) {
        for (size_t i = 0; i < levels; i++) {
            memcpy(d, w->indent_unit.p, unit);
            d += unit;
        }
    } else {
        memset(d, ' ', unit * levels);
        d += unit * levels;
    }
    commit(b, (size_t)(d - (b->p + b->n)));
}

/* Called before every value: the separator and indentation of an array item, or checks that a key is pending. */
static bool before_value(AvpJsonW *w) {
    if (w->failed) return false;
    if (w->depth == 0) return true;
    u8 *top = &w->stack[w->depth - 1];
    if ((*top & ST_KIND) == ST_OBJECT) {
        AVP_ASSERT(*top & ST_KEY_PENDING);
        *top &= (u8)~ST_KEY_PENDING;
        return true;
    }
    if (*top & ST_HAS_ITEM) put(w->out, w->item_sep.p, w->item_sep.n);
    if (w->o.indented) put_newline_indent(w, w->depth);
    *top |= ST_HAS_ITEM;
    return true;
}

static void after_value(AvpJsonW *w) { maybe_spill(w); }

static void push(AvpJsonW *w, u8 kind) {
    if (w->depth == w->stack_cap) {
        size_t cap = w->stack_cap * 2;
        u8 *n = (u8 *)avp_xmalloc(cap);
        memcpy(n, w->stack, w->depth);
        if (w->stack != w->inline_stack) avp_xfree(w->stack);
        w->stack = n;
        w->stack_cap = cap;
    }
    w->stack[w->depth++] = kind;
}

static void begin(AvpJsonW *w, u8 kind, char open) {
    if (!before_value(w)) return;
    put(w->out, &open, 1);
    push(w, kind);
}

static void end(AvpJsonW *w, u8 kind, char close) {
    if (w->failed) return;
    AVP_ASSERT(w->depth > 0 && (w->stack[w->depth - 1] & ST_KIND) == kind && !(w->stack[w->depth - 1] & ST_KEY_PENDING));
    u8 st = w->stack[--w->depth];
    if ((st & ST_HAS_ITEM) && w->o.indented) put_newline_indent(w, w->depth);
    put(w->out, &close, 1);
    after_value(w);
}

void avp_jsonw_begin_object(AvpJsonW *w) { begin(w, ST_OBJECT, '{'); }
void avp_jsonw_end_object(AvpJsonW *w) { end(w, ST_OBJECT, '}'); }
void avp_jsonw_begin_array(AvpJsonW *w) { begin(w, ST_ARRAY, '['); }
void avp_jsonw_end_array(AvpJsonW *w) { end(w, ST_ARRAY, ']'); }

void avp_jsonw_key(AvpJsonW *w, AvpStr key) {
    if (w->failed) return;
    AVP_ASSERT(w->depth > 0);
    u8 *top = &w->stack[w->depth - 1];
    AVP_ASSERT((*top & ST_KIND) == ST_OBJECT && !(*top & ST_KEY_PENDING));
    if (*top & ST_HAS_ITEM) put(w->out, w->item_sep.p, w->item_sep.n);
    if (w->o.indented) put_newline_indent(w, w->depth);
    *top |= ST_HAS_ITEM | ST_KEY_PENDING;
    avp_json_put_string(w->out, key, w->o.raw_unicode);
    put(w->out, w->key_sep.p, w->key_sep.n);
}

void avp_jsonw_null(AvpJsonW *w) {
    if (!before_value(w)) return;
    put(w->out, "null", 4);
    after_value(w);
}

void avp_jsonw_bool(AvpJsonW *w, bool b) {
    if (!before_value(w)) return;
    if (b) put(w->out, "true", 4);
    else put(w->out, "false", 5);
    after_value(w);
}

void avp_jsonw_i64(AvpJsonW *w, i64 v) {
    if (!before_value(w)) return;
    put_i64(w->out, v);
    after_value(w);
}

void avp_jsonw_u64(AvpJsonW *w, u64 v) {
    if (!before_value(w)) return;
    put_u64(w->out, v);
    after_value(w);
}

void avp_jsonw_int_digits(AvpJsonW *w, AvpStr digits) {
    if (!before_value(w)) return;
    i64 v;
    if (avp_json__int_fits(digits, &v)) put_i64(w->out, v); /* "-0" is 0 */
    else put(w->out, digits.p, digits.n);
    after_value(w);
}

void avp_jsonw_f64(AvpJsonW *w, double v) {
    if (!before_value(w)) return;
    if (isnan(v) || isinf(v)) {
        if (w->o.reject_nan) {
            w->failed = true;
            if (w->o.err) {
                w->o.err->kind = AVP_JSON_E_VALUE;
                w->o.err->msg = isnan(v) ? AVP_S("Out of range float values are not JSON compliant: nan")
                                         : v > 0 ? AVP_S("Out of range float values are not JSON compliant: inf")
                                                 : AVP_S("Out of range float values are not JSON compliant: -inf");
                w->o.err->lineno = w->o.err->colno = w->o.err->pos = 0;
            }
            return;
        }
        if (isnan(v)) put(w->out, "NaN", 3);
        else if (v > 0) put(w->out, "Infinity", 8);
        else put(w->out, "-Infinity", 9);
    } else {
        char buf[32];
        size_t n = avp_float_repr(v, buf);
        put(w->out, buf, n);
    }
    after_value(w);
}

void avp_jsonw_str(AvpJsonW *w, AvpStr s) {
    if (!before_value(w)) return;
    avp_json_put_string(w->out, s, w->o.raw_unicode);
    after_value(w);
}

void avp_jsonw_strz(AvpJsonW *w, const char *cz) { avp_jsonw_str(w, avp_str(cz)); }

void avp_jsonw_kv_str(AvpJsonW *w, const char *key, AvpStr s) {
    avp_jsonw_key(w, avp_str(key));
    avp_jsonw_str(w, s);
}
void avp_jsonw_kv_i64(AvpJsonW *w, const char *key, i64 v) {
    avp_jsonw_key(w, avp_str(key));
    avp_jsonw_i64(w, v);
}
void avp_jsonw_kv_f64(AvpJsonW *w, const char *key, double v) {
    avp_jsonw_key(w, avp_str(key));
    avp_jsonw_f64(w, v);
}
void avp_jsonw_kv_bool(AvpJsonW *w, const char *key, bool b) {
    avp_jsonw_key(w, avp_str(key));
    avp_jsonw_bool(w, b);
}

/* ---- trees ---- */
typedef struct {
    const AvpJson *node;
    size_t i;
    const AvpJsonMember **order; /* sorted members of an object when sort_keys (heap), else NULL */
} TFrame;

static int cmp_member(const void *x, const void *y) {
    const AvpJsonMember *a = *(const AvpJsonMember *const *)x, *b = *(const AvpJsonMember *const *)y;
    size_t n = a->key.n < b->key.n ? a->key.n : b->key.n;
    int c = n ? memcmp(a->key.p, b->key.p, n) : 0; /* UTF-8 byte order is code point order */
    if (c) return c;
    return a->key.n < b->key.n ? -1 : a->key.n > b->key.n;
}

static void write_scalar(AvpJsonW *w, const AvpJson *v) {
    switch (v->kind) {
    case AVP_JSON_NULL: avp_jsonw_null(w); break;
    case AVP_JSON_BOOL: avp_jsonw_bool(w, v->b); break;
    case AVP_JSON_INT:
        if (v->big) avp_jsonw_int_digits(w, v->u.str);
        else avp_jsonw_i64(w, v->u.i);
        break;
    case AVP_JSON_FLOAT: avp_jsonw_f64(w, v->u.f); break;
    case AVP_JSON_STRING: avp_jsonw_str(w, v->u.str); break;
    default: AVP_ASSERT(false);
    }
}

void avp_jsonw_tree(AvpJsonW *w, const AvpJson *root) {
    AVP_VEC(TFrame) st = {0};
    const AvpJson *v = root ? root : avp_json_null();
    for (;;) {
        /* write v: scalars at once, containers open and push */
        if (v->kind == AVP_JSON_ARRAY || v->kind == AVP_JSON_OBJECT) {
            bool obj = v->kind == AVP_JSON_OBJECT;
            if (obj) avp_jsonw_begin_object(w);
            else avp_jsonw_begin_array(w);
            TFrame *f = avp_vec_push_zero(&st);
            f->node = v;
            if (obj && w->o.sort_keys && v->u.obj.n > 1) {
                f->order = (const AvpJsonMember **)avp_xmalloc(v->u.obj.n * sizeof(AvpJsonMember *));
                for (size_t i = 0; i < v->u.obj.n; i++) f->order[i] = &v->u.obj.v[i];
                qsort(f->order, v->u.obj.n, sizeof(AvpJsonMember *), cmp_member);
            }
        } else {
            write_scalar(w, v);
        }
        /* find the next value to write, closing finished containers */
        for (;;) {
            if (!st.n) {
                avp_vec_free(&st);
                return;
            }
            TFrame *f = &st.v[st.n - 1];
            const AvpJson *n = f->node;
            bool obj = n->kind == AVP_JSON_OBJECT;
            size_t count = obj ? n->u.obj.n : n->u.arr.n;
            if (f->i == count) {
                if (obj) avp_jsonw_end_object(w);
                else avp_jsonw_end_array(w);
                avp_xfree(f->order);
                st.n--;
                continue;
            }
            if (obj) {
                const AvpJsonMember *m = f->order ? f->order[f->i] : &n->u.obj.v[f->i];
                avp_jsonw_key(w, m->key);
                v = m->val;
            } else {
                v = n->u.arr.v[f->i];
            }
            f->i++;
            break;
        }
        if (w->failed) {
            for (size_t i = 0; i < st.n; i++) avp_xfree(st.v[i].order);
            avp_vec_free(&st);
            return;
        }
    }
}

/* ---- one-shot dumps ---- */
bool avp_json_dump_buf(AvpBuf *out, const AvpJson *v, const AvpJsonDumpOpts *opts) {
    AvpJsonW w;
    avp_jsonw_init(&w, out, opts);
    avp_jsonw_tree(&w, v);
    return avp_jsonw_finish(&w);
}

AvpStr avp_json_dumps(AvpArena *a, const AvpJson *v, const AvpJsonDumpOpts *opts) {
    AvpBuf b = {0};
    AvpStr r = {NULL, 0};
    if (avp_json_dump_buf(&b, v, opts)) r = avp_str_dup(a, avp_buf_str(&b));
    avp_buf_free(&b);
    return r;
}

static bool spill_to_file(void *ctx, const char *p, size_t n) { return fwrite(p, 1, n, (FILE *)ctx) == n; }

bool avp_json_dump_file(FILE *f, const AvpJson *v, const AvpJsonDumpOpts *opts) {
    AvpBuf b = {0};
    AvpJsonW w;
    avp_jsonw_init(&w, &b, opts);
    avp_jsonw_set_spill(&w, 1 << 16, spill_to_file, f);
    avp_jsonw_tree(&w, v);
    bool ok = avp_jsonw_finish(&w);
    avp_buf_free(&b);
    return ok;
}
