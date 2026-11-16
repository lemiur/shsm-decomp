/* tools/ninja_syntax.py in C (ninja_syntax.h). Lengths and positions are counted in code points, as Python's str. */
#include "ninja_syntax.h"

#include <string.h>

#include "avp/vec.h"

NinjaPaths ninja_none(void) {
    NinjaPaths p = {0};
    return p;
}

NinjaPaths ninja_one(AvpArena *a, const char *s) {
    NinjaPaths p = {0};
    if (!s || !*s) return p;
    p.v = avp_alloc(a, sizeof *p.v);
    p.v[0] = s;
    p.n = 1;
    p.given = true;
    return p;
}

NinjaPaths ninja_list(AvpArena *a, size_t n, const char *const *items) {
    NinjaPaths p = {0};
    p.given = n > 0;
    p.v = avp_alloc(a, (n ? n : 1) * sizeof *p.v);
    for (size_t i = 0; i < n; i++)
        if (items[i] && *items[i]) p.v[p.n++] = items[i];
    return p;
}

const char *ninja_join(AvpArena *a, NinjaPaths p) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < p.n; i++) {
        if (i) avp_buf_putc(&b, ' ');
        avp_buf_putz(&b, p.v[i]);
    }
    return avp_buf_cz(&b);
}

const char *ninja_escape_path(AvpArena *a, const char *word) {
    AvpStr s = avp_str(word);
    s = avp_str_replace(a, s, AVP_S("$ "), AVP_S("$$ "), -1);
    s = avp_str_replace(a, s, AVP_S(" "), AVP_S("$ "), -1);
    s = avp_str_replace(a, s, AVP_S(":"), AVP_S("$:"), -1);
    return avp_cz(a, s);
}

const char *ninja_escape(AvpArena *a, const char *s) {
    return avp_cz(a, avp_str_replace(a, avp_str(s), AVP_S("$"), AVP_S("$$"), -1));
}

void ninja_writer_init(NinjaWriter *w, AvpArena *a, AvpBuf *out) {
    w->a = a;
    w->out = out;
    w->width = 78;
}

void ninja_newline(NinjaWriter *w) { avp_buf_putc(w->out, '\n'); }

/* ---- textwrap.wrap ---- */

typedef AVP_VEC(u32) CpVec;

static CpVec decode(const char *text) {
    CpVec v = {0};
    const char *p = text, *end = text + strlen(text);
    while (p < end) avp_vec_push(&v, avp_u8_next(&p, end));
    return v;
}

static void put_cps(AvpBuf *b, const u32 *cp, size_t n) {
    for (size_t i = 0; i < n; i++) avp_buf_putcp(b, cp[i]);
}

typedef struct {
    size_t start, len;
    bool ws;
} Chunk;

AvpStrVec ninja_textwrap(AvpArena *a, const char *text, size_t width) {
    AvpStrVec lines = {0};
    /* _munge_whitespace: expandtabs(), then each of \t\n\v\f\r to a space */
    AvpStr expanded = avp_str_expandtabs(a, avp_str(text), 8);
    CpVec cp = decode(avp_cz(a, expanded));
    for (size_t i = 0; i < cp.n; i++)
        if (cp.v[i] == '\t' || cp.v[i] == '\n' || cp.v[i] == '\v' || cp.v[i] == '\f' || cp.v[i] == '\r') cp.v[i] = ' ';
    /* _split: runs of spaces and runs of the rest */
    AVP_VEC(Chunk) chunks = {0};
    for (size_t i = 0; i < cp.n;) {
        bool ws = cp.v[i] == ' ';
        size_t j = i;
        while (j < cp.n && (cp.v[j] == ' ') == ws) j++;
        Chunk c = {i, j - i, ws};
        avp_vec_push(&chunks, c);
        i = j;
    }
    /* _wrap_chunks over the chunks (the Python pops from the end of the reversed list: next = chunks[k]) */
    size_t k = 0;
    while (k < chunks.n) {
        AVP_VEC(Chunk) cur = {0};
        size_t cur_len = 0;
        if (chunks.v[k].ws && lines.n) k++;
        while (k < chunks.n) {
            size_t l = chunks.v[k].len;
            if (cur_len + l <= width) {
                avp_vec_push(&cur, chunks.v[k]);
                k++;
                cur_len += l;
            } else {
                break;
            }
        }
        if (k < chunks.n && chunks.v[k].len > width) {
            /* _handle_long_word, break_long_words=False */
            if (cur.n == 0) {
                avp_vec_push(&cur, chunks.v[k]);
                k++;
            }
        }
        if (cur.n && cur.v[cur.n - 1].ws) cur.n--;
        if (cur.n) {
            AvpBuf b;
            avp_buf_init(&b, a);
            for (size_t i = 0; i < cur.n; i++) put_cps(&b, cp.v + cur.v[i].start, cur.v[i].len);
            avp_vec_push(&lines, avp_buf_str(&b));
        }
        avp_vec_free(&cur);
    }
    avp_vec_free(&chunks);
    avp_vec_free(&cp);
    return lines;
}

void ninja_comment(NinjaWriter *w, const char *text) {
    AvpStrVec lines = ninja_textwrap(w->a, text, (size_t)(w->width - 2));
    for (size_t i = 0; i < lines.n; i++) {
        avp_buf_putz(w->out, "# ");
        avp_buf_put(w->out, lines.v[i]);
        avp_buf_putc(w->out, '\n');
    }
    avp_vec_free(&lines);
}

/* ---- _line ---- */

static i64 count_dollars_before(const CpVec *s, i64 i) {
    i64 count = 0, idx = i - 1;
    while (idx > 0 && s->v[idx] == '$') {
        count++;
        idx--;
    }
    return count;
}

/* str.rfind(" ", 0, end) */
static i64 rfind_space(const CpVec *s, i64 end) {
    i64 n = (i64)s->n;
    if (end < 0) end += n;
    if (end < 0) end = 0;
    if (end > n) end = n;
    for (i64 i = end - 1; i >= 0; i--)
        if (s->v[i] == ' ') return i;
    return -1;
}

/* str.find(" ", start) */
static i64 find_space(const CpVec *s, i64 start) {
    i64 n = (i64)s->n;
    if (start < 0) start += n;
    if (start < 0) start = 0;
    for (i64 i = start; i < n; i++)
        if (s->v[i] == ' ') return i;
    return -1;
}

static void line(NinjaWriter *w, const char *text_z, int indent) {
    CpVec text = decode(text_z);
    i64 lead = 2 * (i64)indent;
    while (lead + (i64)text.n > w->width) {
        i64 available = w->width - lead - 2;
        i64 space = available;
        for (;;) {
            space = rfind_space(&text, space);
            if (space < 0 || count_dollars_before(&text, space) % 2 == 0) break;
        }
        if (space < 0) {
            space = available - 1;
            for (;;) {
                space = find_space(&text, space + 1);
                if (space < 0 || count_dollars_before(&text, space) % 2 == 0) break;
            }
        }
        if (space < 0) break;
        for (i64 i = 0; i < lead; i++) avp_buf_putc(w->out, ' ');
        put_cps(w->out, text.v, (size_t)space);
        avp_buf_putz(w->out, " $\n");
        memmove(text.v, text.v + space + 1, (text.n - (size_t)space - 1) * sizeof(u32));
        text.n -= (size_t)space + 1;
        lead = 2 * (i64)(indent + 2);
    }
    for (i64 i = 0; i < lead; i++) avp_buf_putc(w->out, ' ');
    put_cps(w->out, text.v, text.n);
    avp_buf_putc(w->out, '\n');
    avp_vec_free(&text);
}

void ninja_variable(NinjaWriter *w, const char *key, const char *value, int indent) {
    line(w, avp_sprintf(w->a, "%s = %s", key, value ? value : ""), indent);
}

void ninja_pool(NinjaWriter *w, const char *name, int depth) {
    line(w, avp_sprintf(w->a, "pool %s", name), 0);
    ninja_variable(w, "depth", avp_sprintf(w->a, "%d", depth), 1);
}

static bool set(const char *s) { return s && *s; }

void ninja_rule(NinjaWriter *w, const NinjaRule *r) {
    line(w, avp_sprintf(w->a, "rule %s", r->name), 0);
    ninja_variable(w, "command", r->command, 1);
    if (set(r->description)) ninja_variable(w, "description", r->description, 1);
    if (set(r->depfile)) ninja_variable(w, "depfile", r->depfile, 1);
    if (r->generator) ninja_variable(w, "generator", "1", 1);
    if (set(r->pool)) ninja_variable(w, "pool", r->pool, 1);
    if (r->restat) ninja_variable(w, "restat", "1", 1);
    if (set(r->rspfile)) ninja_variable(w, "rspfile", r->rspfile, 1);
    if (set(r->rspfile_content)) ninja_variable(w, "rspfile_content", r->rspfile_content, 1);
    if (set(r->deps)) ninja_variable(w, "deps", r->deps, 1);
}

static void put_escaped(NinjaWriter *w, AvpBuf *b, NinjaPaths p) {
    for (size_t i = 0; i < p.n; i++) {
        avp_buf_putc(b, ' ');
        avp_buf_putz(b, ninja_escape_path(w->a, p.v[i]));
    }
}

void ninja_build(NinjaWriter *w, const NinjaBuild *bd) {
    AvpBuf b;
    avp_buf_init(&b, w->a);
    avp_buf_putz(&b, "build");
    put_escaped(w, &b, bd->outputs);
    if (bd->implicit_outputs.given) {
        avp_buf_putz(&b, " |");
        put_escaped(w, &b, bd->implicit_outputs);
    }
    avp_buf_putz(&b, ": ");
    avp_buf_putz(&b, bd->rule);
    put_escaped(w, &b, bd->inputs);
    if (bd->implicit.given) {
        avp_buf_putz(&b, " |");
        put_escaped(w, &b, bd->implicit);
    }
    if (bd->order_only.given) {
        avp_buf_putz(&b, " ||");
        put_escaped(w, &b, bd->order_only);
    }
    /* "build %s: %s" % (" ".join(out_outputs), ...): with no outputs the Python writes "build : rule" */
    const char *text = avp_buf_cz(&b);
    if (bd->outputs.n == 0 && !bd->implicit_outputs.given) text = avp_sprintf(w->a, "build %s", text + 5);
    line(w, text, 0);
    if (bd->pool) line(w, avp_sprintf(w->a, "  pool = %s", bd->pool), 0);
    if (bd->dyndep) line(w, avp_sprintf(w->a, "  dyndep = %s", bd->dyndep), 0);
    for (size_t i = 0; i < bd->nvars; i++) ninja_variable(w, bd->vars[i].key, bd->vars[i].val, 1);
}

void ninja_include(NinjaWriter *w, const char *path) { line(w, avp_sprintf(w->a, "include %s", path), 0); }

void ninja_subninja(NinjaWriter *w, const char *path) { line(w, avp_sprintf(w->a, "subninja %s", path), 0); }

void ninja_default(NinjaWriter *w, NinjaPaths paths) {
    line(w, avp_sprintf(w->a, "default %s", ninja_join(w->a, paths)), 0);
}
