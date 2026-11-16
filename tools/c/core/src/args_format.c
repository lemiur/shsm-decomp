/* argparse's HelpFormatter (usage and help text), textwrap.wrap and the repr helpers, for args.h. The code follows
 * CPython 3.14.8's argparse.py and textwrap.py function by function (Python's names are in the comments); all widths
 * count code points, as Python's len() does. Colour is never produced. */
#include "args_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

const char avp_args_suppress[] = "==SUPPRESS==";

/* ---- small helpers --------------------------------------------------------------------------------------------- */
size_t argp_cplen(AvpStr s) {
    size_t n = 0;
    for (size_t i = 0; i < s.n; i++)
        if (((u8)s.p[i] & 0xC0) != 0x80) n++;
    return n;
}
size_t argp_cplen_z(const char *cz) { return argp_cplen(avp_str(cz)); }

static void put_spaces(AvpBuf *b, size_t n) {
    for (size_t i = 0; i < n; i++) avp_buf_putc(b, ' ');
}

static AvpStr arena_str(AvpArena *a, AvpStr s) { return avp_str_dup(a, s); }

/* ---- repr -------------------------------------------------------------------------------------------------------- */
/* str.isprintable() for one code point, approximated: the C0/C1 controls, NBSP, the soft hyphen, the Unicode space
 * separators, the format characters and the private-use and surrogate ranges are not printable. */
static bool cp_printable(u32 c) {
    if (c < 0x20 || c == 0x7f) return false;
    if (c < 0x7f) return true;
    if (c <= 0xA0 || c == 0xAD) return false;
    if (c >= 0x0600 && c <= 0x0605) return false;
    if (c == 0x061C || c == 0x06DD || c == 0x070F || c == 0x180E || c == 0x1680) return false;
    if (c >= 0x2000 && c <= 0x200F) return false;
    if (c >= 0x2028 && c <= 0x202F) return false;
    if (c >= 0x205F && c <= 0x206F) return false;
    if (c == 0x3000 || c == 0xFEFF) return false;
    if (c >= 0xFFF9 && c <= 0xFFFB) return false;
    if (c >= 0xD800 && c <= 0xF8FF) return false;
    if (c >= 0xF0000) return false;
    return true;
}

void argp_repr(AvpBuf *b, AvpStr s) {
    char quote = '\'';
    if (avp_str_findc(s, '\'') >= 0 && avp_str_findc(s, '"') < 0) quote = '"';
    avp_buf_putc(b, quote);
    const char *p = s.p, *end = s.p + s.n;
    while (p < end) {
        u32 c = avp_u8_next(&p, end);
        if (c == (u32)quote || c == '\\') {
            avp_buf_putc(b, '\\');
            avp_buf_putc(b, (char)c);
        } else if (c == '\t') avp_buf_putz(b, "\\t");
        else if (c == '\n') avp_buf_putz(b, "\\n");
        else if (c == '\r') avp_buf_putz(b, "\\r");
        else if (c < 0x100 && !cp_printable(c)) avp_buf_printf(b, "\\x%02x", c);
        else if (!cp_printable(c)) {
            if (c < 0x10000) avp_buf_printf(b, "\\u%04x", c);
            else avp_buf_printf(b, "\\U%08x", c);
        } else avp_buf_putcp(b, c);
    }
    avp_buf_putc(b, quote);
}

/* repr(float): the shortest digits that read back the same, in Python's layout. */
void argp_float_repr(AvpBuf *b, double x) {
    if (isnan(x)) {
        avp_buf_putz(b, "nan");
        return;
    }
    if (isinf(x)) {
        avp_buf_putz(b, x < 0 ? "-inf" : "inf");
        return;
    }
    char e[40];
    int prec;
    for (prec = 0; prec < 17; prec++) {
        snprintf(e, sizeof e, "%.*e", prec, x);
        if (strtod(e, NULL) == x) break;
    }
    if (prec == 17) snprintf(e, sizeof e, "%.16e", x);
    /* e = [-]d[.ddd]e[+-]XX */
    const char *q = e;
    if (*q == '-') {
        avp_buf_putc(b, '-');
        q++;
    }
    char digits[24];
    int nd = 0;
    digits[nd++] = *q++;
    if (*q == '.') {
        q++;
        while (*q != 'e') digits[nd++] = *q++;
    }
    q++; /* 'e' */
    int exp10 = atoi(q);
    while (nd > 1 && digits[nd - 1] == '0') nd--;
    int decpt = exp10 + 1;
    if (x == 0) {
        avp_buf_putz(b, "0.0");
        return;
    }
    if (decpt > -4 && decpt <= 16) {
        if (decpt <= 0) {
            avp_buf_putz(b, "0.");
            for (int i = 0; i < -decpt; i++) avp_buf_putc(b, '0');
            avp_buf_putn(b, digits, (size_t)nd);
        } else if (decpt >= nd) {
            avp_buf_putn(b, digits, (size_t)nd);
            for (int i = nd; i < decpt; i++) avp_buf_putc(b, '0');
            avp_buf_putz(b, ".0");
        } else {
            avp_buf_putn(b, digits, (size_t)decpt);
            avp_buf_putc(b, '.');
            avp_buf_putn(b, digits + decpt, (size_t)(nd - decpt));
        }
    } else {
        avp_buf_putc(b, digits[0]);
        if (nd > 1) {
            avp_buf_putc(b, '.');
            avp_buf_putn(b, digits + 1, (size_t)(nd - 1));
        }
        avp_buf_printf(b, "e%c%02d", exp10 < 0 ? '-' : '+', exp10 < 0 ? -exp10 : exp10);
    }
}

/* ---- the width ---------------------------------------------------------------------------------------------------- */
int avp_args_width(void) {
    int columns = 0;
    const char *env = getenv("COLUMNS");
    if (env) {
        i64 v;
        if (avp_parse_int(avp_str(env), 10, &v, NULL) && v > 0 && v < 100000000) columns = (int)v;
    }
    if (columns <= 0) {
        CONSOLE_SCREEN_BUFFER_INFO info;
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (h != INVALID_HANDLE_VALUE && h != NULL && GetConsoleScreenBufferInfo(h, &info))
            columns = info.srWindow.Right - info.srWindow.Left + 1;
        if (columns <= 0) columns = 80; /* the fallback of shutil.get_terminal_size */
    }
    return columns - 2;
}

/* ---- textwrap ---------------------------------------------------------------------------------------------------- */
typedef struct {
    const u32 *p;
    size_t n;
} Chunk;
typedef AVP_VEC(Chunk) ChunkVec;
typedef AVP_VEC(u32) CpVec;

static bool tw_ws(u32 c) { return c == ' ' || c == '\t' || c == '\n' || c == 0x0b || c == 0x0c || c == '\r'; }

static bool cp_class(u32 c, bool (*pred)(AvpStr)) {
    char buf[4];
    size_t n = avp_u8_put(buf, c);
    return pred(avp_strn(buf, n));
}
/* \w */
static bool tw_word(u32 c) {
    if (c < 0x80) return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    return cp_class(c, avp_str_isalnum);
}
/* [^\d\W]: a word character that is not a decimal digit */
static bool tw_letter(u32 c) {
    if (c < 0x80) return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    return cp_class(c, avp_str_isalnum) && !cp_class(c, avp_str_isdecimal);
}
/* [\w!"'&.,?] */
static bool tw_wp(u32 c) { return tw_word(c) || c == '!' || c == '"' || c == '\'' || c == '&' || c == '.' || c == ',' || c == '?'; }

static size_t dash_run(const u32 *t, size_t n, size_t e) {
    size_t r = 0;
    while (e + r < n && t[e + r] == '-') r++;
    return r;
}

/* TextWrapper._split with break_on_hyphens: the chunks wordsep_re.split() gives (empty strings dropped). The regex
 * alternatives are tried in order at each position: whitespace run, em-dash between words, then a lazily grown
 * word that stops at a hyphenated-word break, the end of the word, or an em-dash. */
static void tw_split(const u32 *t, size_t n, ChunkVec *out) {
    size_t p = 0;
    while (p < n) {
        size_t end = 0;
        if (tw_ws(t[p])) {
            end = p;
            while (end < n && tw_ws(t[end])) end++;
        } else {
            size_t run = dash_run(t, n, p);
            if (p > 0 && tw_wp(t[p - 1]) && run >= 2 && p + run < n && tw_word(t[p + run])) {
                end = p + run;
            } else {
                size_t e = p + 1;
                for (;;) {
                    if (e < n && t[e] == '-') {
                        bool lb = (e >= 2 && tw_letter(t[e - 2]) && tw_letter(t[e - 1])) ||
                                  (e >= 3 && tw_letter(t[e - 3]) && t[e - 2] == '-' && tw_letter(t[e - 1]));
                        if (lb) {
                            size_t q = e + 1;
                            bool la = false;
                            if (q < n && tw_letter(t[q])) {
                                if (q + 1 < n && tw_letter(t[q + 1])) la = true;
                                else if (q + 1 < n && t[q + 1] == '-' && q + 2 < n && tw_letter(t[q + 2])) la = true;
                            }
                            if (la) {
                                end = e + 1;
                                break;
                            }
                        }
                    }
                    if (e == n || tw_ws(t[e])) {
                        end = e;
                        break;
                    }
                    if (tw_wp(t[e - 1])) {
                        size_t r2 = dash_run(t, n, e);
                        if (r2 >= 2 && e + r2 < n && tw_word(t[e + r2])) {
                            end = e;
                            break;
                        }
                    }
                    e++;
                }
            }
        }
        Chunk c = {t + p, end - p};
        avp_vec_push(out, c);
        p = end;
    }
}

static bool chunk_blank(Chunk c) {
    for (size_t i = 0; i < c.n; i++)
        if (!avp_cp_isspace(c.p[i])) return false;
    return true;
}

static void put_cps(AvpBuf *b, const u32 *p, size_t n) {
    for (size_t i = 0; i < n; i++) avp_buf_putcp(b, p[i]);
}

/* textwrap.wrap(text, width, initial_indent, subsequent_indent) with the default options: the lines, allocated from a. The
 * text must already have its whitespace replaced by spaces (argparse collapses it first). */
static AvpStrVec tw_wrap(AvpArena *a, AvpStr text, int width, AvpStr initial_indent, AvpStr subsequent_indent) {
    AvpStrVec lines = {0};
    CpVec cps = {0};
    const char *p = text.p, *end = text.p + text.n;
    while (p < end) avp_vec_push(&cps, avp_u8_next(&p, end));
    ChunkVec chunks = {0};
    tw_split(cps.v, cps.n, &chunks);
    size_t ci = 0;
    ChunkVec cur = {0};
    size_t ni = argp_cplen(initial_indent), ns = argp_cplen(subsequent_indent);
    while (ci < chunks.n) {
        cur.n = 0;
        size_t cur_len = 0;
        AvpStr indent = lines.n ? subsequent_indent : initial_indent;
        i64 w = (i64)width - (i64)(lines.n ? ns : ni);
        if (chunk_blank(chunks.v[ci]) && lines.n) ci++;
        while (ci < chunks.n) {
            size_t l = chunks.v[ci].n;
            if ((i64)(cur_len + l) <= w) {
                avp_vec_push(&cur, chunks.v[ci]);
                ci++;
                cur_len += l;
            } else break;
        }
        if (ci < chunks.n && (i64)chunks.v[ci].n > w) {
            /* TextWrapper._handle_long_word */
            i64 space_left = w < 1 ? 1 : w - (i64)cur_len;
            if (space_left > 0) {
                size_t endc = (size_t)space_left;
                Chunk ch = chunks.v[ci];
                if (ch.n > (size_t)space_left) {
                    i64 hyphen = -1;
                    for (i64 k = space_left - 1; k >= 0; k--)
                        if (ch.p[k] == '-') {
                            hyphen = k;
                            break;
                        }
                    if (hyphen > 0) {
                        bool other = false;
                        for (i64 k = 0; k < hyphen; k++)
                            if (ch.p[k] != '-') other = true;
                        if (other) endc = (size_t)hyphen + 1;
                    }
                }
                Chunk head = {ch.p, endc};
                avp_vec_push(&cur, head);
                chunks.v[ci].p += endc;
                chunks.v[ci].n -= endc;
            } else if (!cur.n) {
                avp_vec_push(&cur, chunks.v[ci]);
                ci++;
            }
            cur_len = 0;
            for (size_t k = 0; k < cur.n; k++) cur_len += cur.v[k].n;
        }
        if (cur.n && chunk_blank(cur.v[cur.n - 1])) {
            cur_len -= cur.v[cur.n - 1].n;
            cur.n--;
        }
        if (cur.n) {
            AvpBuf line;
            avp_buf_init(&line, NULL);
            avp_buf_put(&line, indent);
            for (size_t k = 0; k < cur.n; k++) put_cps(&line, cur.v[k].p, cur.v[k].n);
            avp_vec_push(&lines, arena_str(a, avp_buf_str(&line)));
            avp_buf_free(&line);
        }
        (void)cur_len;
    }
    avp_vec_free(&cps);
    avp_vec_free(&chunks);
    avp_vec_free(&cur);
    return lines;
}

/* re.sub(r'\s+', ' ', text, flags=re.ASCII).strip() */
static AvpStr collapse_ws(AvpArena *a, AvpStr text) {
    AvpBuf b;
    avp_buf_init(&b, NULL);
    bool in_ws = false;
    for (size_t i = 0; i < text.n; i++) {
        char c = text.p[i];
        bool ws = c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
        if (ws) {
            if (!in_ws) avp_buf_putc(&b, ' ');
            in_ws = true;
        } else {
            avp_buf_putc(&b, c);
            in_ws = false;
        }
    }
    AvpStr r = arena_str(a, avp_str_strip(avp_buf_str(&b)));
    avp_buf_free(&b);
    return r;
}

/* ---- the formatter ----------------------------------------------------------------------------------------------- */
enum { F_USAGE, F_TEXT, F_ACTION, F_SECTION };
typedef struct FSec FSec;
typedef struct {
    int kind;
    const AvpArgAction *const *acts; /* F_USAGE */
    size_t nacts;
    AvpArgMutex *const *groups;
    size_t ngroups;
    const char *usage;
    const char *prefix;
    const char *text;                /* F_TEXT */
    const AvpArgAction *act;         /* F_ACTION */
    FSec *sec;                       /* F_SECTION */
} FItem;
struct FSec {
    FSec *parent;
    const char *heading;
    AVP_VEC(FItem) items;
};
typedef struct {
    AvpArgParser *p;
    AvpArena *a;
    const char *prog;
    int width;
    int max_help_position;
    int indent_inc;
    int cur_indent;
    int action_max_length;
    FSec root;
    FSec *cur;
} Fmt;

static void fmt_init(Fmt *f, AvpArgParser *p, const char *prog) {
    memset(f, 0, sizeof *f);
    f->p = p;
    f->a = p->a;
    f->prog = prog;
    f->width = avp_args_width();
    f->indent_inc = 2;
    int hp = f->width - 20;
    if (hp < f->indent_inc * 2) hp = f->indent_inc * 2;
    f->max_help_position = hp < 24 ? hp : 24;
    f->cur = &f->root;
}
static void fmt_free(FSec *s) {
    for (size_t i = 0; i < s->items.n; i++)
        if (s->items.v[i].kind == F_SECTION) {
            fmt_free(s->items.v[i].sec);
            avp_xfree(s->items.v[i].sec);
        }
    avp_vec_free(&s->items);
}

static void add_item(Fmt *f, FItem it) { avp_vec_push(&f->cur->items, it); }

static void start_section(Fmt *f, const char *heading) {
    f->cur_indent += f->indent_inc;
    FSec *s = (FSec *)avp_xcalloc(1, sizeof *s);
    s->parent = f->cur;
    s->heading = heading;
    FItem it = {0};
    it.kind = F_SECTION;
    it.sec = s;
    add_item(f, it);
    f->cur = s;
}
static void end_section(Fmt *f) {
    f->cur = f->cur->parent;
    f->cur_indent -= f->indent_inc;
}

static void add_text(Fmt *f, const char *text) {
    if (!text) return;
    FItem it = {0};
    it.kind = F_TEXT;
    it.text = text;
    add_item(f, it);
}

static void add_usage(Fmt *f, const char *usage, const AvpArgAction *const *acts, size_t nacts,
                      AvpArgMutex *const *groups, size_t ngroups, const char *prefix) {
    FItem it = {0};
    it.kind = F_USAGE;
    it.usage = usage;
    it.acts = acts;
    it.nacts = nacts;
    it.groups = groups;
    it.ngroups = ngroups;
    it.prefix = prefix;
    add_item(f, it);
}

/* _get_default_metavar_for_optional / _for_positional */
static const char *default_metavar(Fmt *f, const AvpArgAction *act, bool optional) {
    if (f->p->formatter == AVP_FORMAT_METAVAR_TYPE) {
        if (act->type_name) return act->type_name;
        return act->type == AVP_T_INT || act->type == AVP_T_INT0 ? "int" : act->type == AVP_T_FLOAT ? "float" : "str";
    }
    if (!act->dest) return "";
    if (!optional) return act->dest;
    AvpStr up = avp_str_upper(f->a, avp_str(act->dest));
    return avp_cz(f->a, up);
}

/* _metavar_formatter(action, default)(tuple_size): returns the strings (a tuple metavar as it is). */
static size_t metavar_formatter(Fmt *f, const AvpArgAction *act, const char *dflt, size_t size, const char **out) {
    if (act->metavars) {
        for (size_t i = 0; i < act->nmetavars; i++) out[i] = act->metavars[i];
        return act->nmetavars;
    }
    const char *one;
    if (act->metavar) one = act->metavar;
    else if (act->kind == ARGK_SUBPARSERS || act->choices) {
        AvpBuf b;
        avp_buf_init(&b, f->a);
        avp_buf_putc(&b, '{');
        if (act->kind == ARGK_SUBPARSERS) {
            for (size_t i = 0; i < act->sub->names.n; i++) {
                if (i) avp_buf_putc(&b, ',');
                avp_buf_putz(&b, act->sub->names.v[i]);
            }
        } else {
            for (size_t i = 0; i < act->nchoices; i++) {
                if (i) avp_buf_putc(&b, ',');
                avp_buf_putz(&b, act->choices[i]);
            }
        }
        avp_buf_putc(&b, '}');
        one = avp_buf_cz(&b);
    } else one = dflt;
    for (size_t i = 0; i < size; i++) out[i] = one;
    return size;
}

/* _format_args */
static const char *format_args(Fmt *f, const AvpArgAction *act, const char *dflt) {
    const char *m[64];
    AvpBuf b;
    avp_buf_init(&b, f->a);
    int nargs = act->nargs;
    if (nargs == ARGN_NONE) {
        metavar_formatter(f, act, dflt, 1, m);
        avp_buf_putz(&b, m[0]);
    } else if (nargs == AVP_NARGS_OPT) {
        metavar_formatter(f, act, dflt, 1, m);
        avp_buf_printf(&b, "[%s]", m[0]);
    } else if (nargs == AVP_NARGS_STAR) {
        size_t k = metavar_formatter(f, act, dflt, 1, m);
        if (k == 2) avp_buf_printf(&b, "[%s [%s ...]]", m[0], m[1]);
        else avp_buf_printf(&b, "[%s ...]", m[0]);
    } else if (nargs == AVP_NARGS_PLUS) {
        metavar_formatter(f, act, dflt, 2, m);
        avp_buf_printf(&b, "%s [%s ...]", m[0], m[1]);
    } else if (nargs == AVP_NARGS_REMAINDER) {
        avp_buf_putz(&b, "...");
    } else if (nargs == AVP_NARGS_PARSER) {
        metavar_formatter(f, act, dflt, 1, m);
        avp_buf_printf(&b, "%s ...", m[0]);
    } else if (nargs == AVP_NARGS_SUPPRESS) {
        /* '' */
    } else {
        AVP_ASSERT(nargs >= 0 && nargs < 64);
        metavar_formatter(f, act, dflt, (size_t)nargs, m);
        for (int i = 0; i < nargs; i++) {
            if (i) avp_buf_putc(&b, ' ');
            avp_buf_putz(&b, m[i]);
        }
    }
    return avp_buf_cz(&b);
}

/* _format_action_invocation */
static const char *format_invocation(Fmt *f, const AvpArgAction *act) {
    AvpBuf b;
    avp_buf_init(&b, f->a);
    if (!act->nopts) {
        const char *m[64];
        size_t k = metavar_formatter(f, act, default_metavar(f, act, false), 1, m);
        for (size_t i = 0; i < k; i++) {
            if (i) avp_buf_putc(&b, ' ');
            avp_buf_putz(&b, m[i]);
        }
    } else if (act->nargs == 0) {
        for (size_t i = 0; i < act->nopts; i++) {
            if (i) avp_buf_putz(&b, ", ");
            avp_buf_putz(&b, act->opts[i]);
        }
    } else {
        const char *args = format_args(f, act, default_metavar(f, act, true));
        for (size_t i = 0; i < act->nopts; i++) {
            if (i) avp_buf_putz(&b, ", ");
            avp_buf_putz(&b, act->opts[i]);
        }
        avp_buf_putc(&b, ' ');
        avp_buf_putz(&b, args);
    }
    return avp_buf_cz(&b);
}

/* Action.format_usage(): option_strings[0], or all of them with " | " for BooleanOptionalAction. */
static const char *action_format_usage(Fmt *f, const AvpArgAction *act) {
    if (act->kind != AVP_ARG_BOOLEAN_OPTIONAL) return act->opts[0];
    AvpBuf b;
    avp_buf_init(&b, f->a);
    for (size_t i = 0; i < act->nopts; i++) {
        if (i) avp_buf_putz(&b, " | ");
        avp_buf_putz(&b, act->opts[i]);
    }
    return avp_buf_cz(&b);
}

static const ActionVec *subactions(const AvpArgAction *act) {
    return act->kind == ARGK_SUBPARSERS ? &act->sub->pseudo : NULL;
}

static void add_argument(Fmt *f, const AvpArgAction *act) {
    if (act->help_suppress) return;
    size_t len = argp_cplen_z(format_invocation(f, act)) + (size_t)f->cur_indent;
    size_t best = len;
    const ActionVec *subs = subactions(act);
    if (subs) {
        f->cur_indent += f->indent_inc;
        for (size_t i = 0; i < subs->n; i++) {
            size_t l = argp_cplen_z(format_invocation(f, subs->v[i])) + (size_t)f->cur_indent;
            if (l > best) best = l;
        }
        f->cur_indent -= f->indent_inc;
    }
    if ((int)best > f->action_max_length) f->action_max_length = (int)best;
    FItem it = {0};
    it.kind = F_ACTION;
    it.act = act;
    add_item(f, it);
}

/* ---- %-formatting of help texts ---------------------------------------------------------------------------------- */
static void put_default_text(AvpBuf *b, AvpArgDefault d, bool as_repr, const AvpArgAction *act, bool is_default) {
    switch (d.kind) {
    case AVP_DK_STR:
        if (as_repr) argp_repr(b, avp_str(d.s));
        else avp_buf_putz(b, d.s);
        break;
    case AVP_DK_INT: avp_buf_printf(b, "%lld", (long long)d.i); break;
    case AVP_DK_FLOAT: argp_float_repr(b, d.f); break;
    case AVP_DK_BOOL: avp_buf_putz(b, d.i ? "True" : "False"); break;
    default:
        if (is_default && act->has_dflt_list) {
            avp_buf_putc(b, '[');
            for (size_t i = 0; i < act->ndflt_list; i++) {
                if (i) avp_buf_putz(b, ", ");
                argp_repr(b, avp_str(act->dflt_list[i]));
            }
            avp_buf_putc(b, ']');
        } else avp_buf_putz(b, "None");
        break;
    }
}

/* One `%(name)s` / `%(name)r` / `%(name)d`: false when the name is unknown. */
static bool put_param(Fmt *f, AvpBuf *b, const AvpArgAction *act, AvpStr name, char conv) {
    bool r = conv == 'r';
    if (avp_str_eqz(name, "prog")) {
        if (r) argp_repr(b, avp_str(f->prog));
        else avp_buf_putz(b, f->prog);
    } else if (avp_str_eqz(name, "default")) {
        if (act->dflt.kind == AVP_DK_SUPPRESS) return false;
        put_default_text(b, act->dflt, r, act, true);
    } else if (avp_str_eqz(name, "const")) {
        put_default_text(b, act->cnst, r, act, false);
    } else if (avp_str_eqz(name, "dest")) {
        if (!act->dest) return false;
        if (r) argp_repr(b, avp_str(act->dest));
        else avp_buf_putz(b, act->dest);
    } else if (avp_str_eqz(name, "required")) {
        avp_buf_putz(b, act->required ? "True" : "False");
    } else if (avp_str_eqz(name, "type")) {
        avp_buf_putz(b, act->type_name ? act->type_name : "None");
    } else if (avp_str_eqz(name, "metavar")) {
        if (act->metavars) {
            avp_buf_putc(b, '(');
            for (size_t i = 0; i < act->nmetavars; i++) {
                if (i) avp_buf_putz(b, ", ");
                argp_repr(b, avp_str(act->metavars[i]));
            }
            if (act->nmetavars == 1) avp_buf_putc(b, ',');
            avp_buf_putc(b, ')');
        } else if (act->metavar) {
            if (r) argp_repr(b, avp_str(act->metavar));
            else avp_buf_putz(b, act->metavar);
        } else avp_buf_putz(b, "None");
    } else if (avp_str_eqz(name, "choices")) {
        if (act->kind == ARGK_SUBPARSERS) {
            for (size_t i = 0; i < act->sub->names.n; i++) {
                if (i) avp_buf_putz(b, ", ");
                avp_buf_putz(b, act->sub->names.v[i]);
            }
        } else if (act->choices) {
            for (size_t i = 0; i < act->nchoices; i++) {
                if (i) avp_buf_putz(b, ", ");
                avp_buf_putz(b, act->choices[i]);
            }
        } else avp_buf_putz(b, "None");
    } else if (avp_str_eqz(name, "nargs")) {
        int n = act->nargs;
        if (n == ARGN_NONE) avp_buf_putz(b, "None");
        else if (n == AVP_NARGS_OPT) avp_buf_putz(b, r ? "'?'" : "?");
        else if (n == AVP_NARGS_STAR) avp_buf_putz(b, r ? "'*'" : "*");
        else if (n == AVP_NARGS_PLUS) avp_buf_putz(b, r ? "'+'" : "+");
        else if (n == AVP_NARGS_REMAINDER) avp_buf_putz(b, r ? "'...'" : "...");
        else if (n == AVP_NARGS_PARSER) avp_buf_putz(b, r ? "'A...'" : "A...");
        else if (n >= 0) avp_buf_printf(b, "%d", n);
        else return false;
    } else if (avp_str_eqz(name, "help")) {
        if (act->help) avp_buf_putz(b, act->help);
        else avp_buf_putz(b, "None");
    } else if (avp_str_eqz(name, "version")) {
        if (act->version) avp_buf_putz(b, act->version);
        else avp_buf_putz(b, "None");
    } else if (avp_str_eqz(name, "option_strings")) {
        avp_buf_putc(b, '[');
        for (size_t i = 0; i < act->nopts; i++) {
            if (i) avp_buf_putz(b, ", ");
            argp_repr(b, avp_str(act->opts[i]));
        }
        avp_buf_putc(b, ']');
    } else if (avp_str_eqz(name, "deprecated")) {
        avp_buf_putz(b, "False");
    } else return false;
    return true;
}

/* help_string % params (_expand_help). */
static AvpStr expand_percent(Fmt *f, const AvpArgAction *act, const char *help) {
    AvpBuf b;
    avp_buf_init(&b, f->a);
    for (const char *p = help; *p; p++) {
        if (*p != '%') {
            avp_buf_putc(&b, *p);
            continue;
        }
        p++;
        if (*p == '%') {
            avp_buf_putc(&b, '%');
            continue;
        }
        if (*p != '(') avp_panic(__FILE__, __LINE__, "argparse help: unsupported format in \"%s\"", help);
        const char *close = strchr(p, ')');
        if (!close) avp_panic(__FILE__, __LINE__, "argparse help: unterminated %%( in \"%s\"", help);
        AvpStr name = avp_strn(p + 1, (size_t)(close - p - 1));
        char conv = close[1];
        if (conv != 's' && conv != 'r' && conv != 'd' && conv != 'i')
            avp_panic(__FILE__, __LINE__, "argparse help: unsupported conversion in \"%s\"", help);
        if (!put_param(f, &b, act, name, conv))
            avp_panic(__FILE__, __LINE__, "argparse help: KeyError %.*s in \"%s\"", AVP_SF(name), help);
        p = close + 1;
    }
    return avp_buf_str(&b);
}

/* HelpFormatter._get_help_string (ArgumentDefaultsHelpFormatter adds the default). */
static const char *get_help_string(Fmt *f, const AvpArgAction *act) {
    const char *help = act->help ? act->help : "";
    if (f->p->formatter == AVP_FORMAT_ARGUMENT_DEFAULTS) {
        if (!strstr(help, "%(default)") && act->dflt.kind != AVP_DK_SUPPRESS && !act->required) {
            if (act->nopts || act->nargs == AVP_NARGS_OPT || act->nargs == AVP_NARGS_STAR)
                return avp_sprintf(f->a, "%s (default: %%(default)s)", help);
        }
    }
    return help;
}

static AvpStr expand_help(Fmt *f, const AvpArgAction *act) {
    const char *h = get_help_string(f, act);
    if (!strchr(h, '%')) return avp_str(h);
    return expand_percent(f, act, h);
}

/* HelpFormatter._split_lines (RawTextHelpFormatter: text.splitlines()) */
static AvpStrVec split_lines(Fmt *f, AvpStr text, int width) {
    if (f->p->formatter == AVP_FORMAT_RAW_TEXT) {
        AvpStrVec v = avp_str_splitlines(text, false);
        AvpStrVec out = {0};
        for (size_t i = 0; i < v.n; i++) avp_vec_push(&out, arena_str(f->a, v.v[i]));
        avp_vec_free(&v);
        return out;
    }
    AvpStr t = collapse_ws(f->a, text);
    return tw_wrap(f->a, t, width, AVP_S(""), AVP_S(""));
}

/* HelpFormatter._fill_text (RawDescription/RawText: indent + line for every line, line ends kept) */
static AvpStr fill_text(Fmt *f, AvpStr text, int width, AvpStr indent) {
    AvpBuf b;
    avp_buf_init(&b, f->a);
    if (f->p->formatter == AVP_FORMAT_RAW_DESCRIPTION || f->p->formatter == AVP_FORMAT_RAW_TEXT) {
        AvpStrVec v = avp_str_splitlines(text, true);
        for (size_t i = 0; i < v.n; i++) {
            avp_buf_put(&b, indent);
            avp_buf_put(&b, v.v[i]);
        }
        avp_vec_free(&v);
        return avp_buf_str(&b);
    }
    AvpStr t = collapse_ws(f->a, text);
    AvpStrVec lines = tw_wrap(f->a, t, width, indent, indent);
    for (size_t i = 0; i < lines.n; i++) {
        if (i) avp_buf_putc(&b, '\n');
        avp_buf_put(&b, lines.v[i]);
    }
    avp_vec_free(&lines);
    return avp_buf_str(&b);
}

AvpStr argp_percent_prog(AvpArena *a, const char *s, const char *prog) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (const char *p = s; *p; p++) {
        if (*p != '%') {
            avp_buf_putc(&b, *p);
        } else if (p[1] == '%') {
            avp_buf_putc(&b, '%');
            p++;
        } else if (strncmp(p, "%(prog)s", 8) == 0) {
            avp_buf_putz(&b, prog);
            p += 7;
        } else avp_panic(__FILE__, __LINE__, "argparse: unsupported % format in \"%s\"", s);
    }
    return avp_buf_str(&b);
}

/* ---- usage ------------------------------------------------------------------------------------------------------- */
typedef struct {
    bool group_required; /* meaningful when n > 1 or in a group */
    bool has_group;
    AVP_VEC(const AvpArgAction *) acts;
} UsageGroup;

static bool in_actions(const AvpArgAction *const *list, size_t n, const AvpArgAction *a) {
    for (size_t i = 0; i < n; i++)
        if (list[i] == a) return true;
    return false;
}

/* action_groups.pop(action): true when it was still present; *out receives its group (may be NULL). */
static bool pop_action(bool *present, AvpArgMutex *const *grp, size_t idx, AvpArgMutex **out) {
    if (!present[idx]) return false;
    present[idx] = false;
    *out = grp[idx];
    return true;
}

/* _get_actions_usage_parts: returns the parts, *pos_start the index where the positionals begin. */
static AvpStrVec usage_parts(Fmt *f, const AvpArgAction *const *all, size_t nall, AvpArgMutex *const *groups,
                             size_t ngroups, size_t *pos_start) {
    /* actions = [action for action in actions if action.help is not SUPPRESS] */
    AVP_VEC(const AvpArgAction *) acts = {0};
    for (size_t i = 0; i < nall; i++)
        if (!all[i]->help_suppress) avp_vec_push(&acts, all[i]);
    /* action_groups = dict.fromkeys(actions); each action maps to the last group holding it. present[i] says it is
     * still in the dict (not popped). */
    AvpArgMutex **grp = (AvpArgMutex **)avp_xcalloc(acts.n + 1, sizeof(AvpArgMutex *));
    bool *present = (bool *)avp_xcalloc(acts.n + 1, sizeof(bool));
    for (size_t i = 0; i < acts.n; i++) present[i] = true;
    for (size_t g = 0; g < ngroups; g++)
        for (size_t k = 0; k < groups[g]->actions.n; k++)
            for (size_t i = 0; i < acts.n; i++)
                if (acts.v[i] == groups[g]->actions.v[k]) grp[i] = groups[g];

    typedef struct {
        AvpArgMutex *group;
        AVP_VEC(const AvpArgAction *) items;
    } Entry;
    AVP_VEC(Entry) optionals = {0}, positionals = {0};

    for (size_t i = 0; i < acts.n; i++) {
        if (acts.v[i]->nopts) continue;
        AvpArgMutex *group = NULL;
        (void)pop_action(present, grp, i, &group);
        Entry e = {0};
        if (group) {
            e.group = group;
            for (size_t k = 0; k < group->actions.n; k++) {
                const AvpArgAction *a2 = group->actions.v[k];
                if (!a2->nopts) continue;
                for (size_t j = 0; j < acts.n; j++) {
                    if (acts.v[j] != a2) continue;
                    AvpArgMutex *g2 = NULL;
                    if (pop_action(present, grp, j, &g2) && g2) avp_vec_push(&e.items, a2);
                }
            }
            avp_vec_push(&e.items, acts.v[i]);
        } else avp_vec_push(&e.items, acts.v[i]);
        avp_vec_push(&positionals, e);
    }
    for (size_t i = 0; i < acts.n; i++) {
        if (!acts.v[i]->nopts || !present[i]) continue;
        AvpArgMutex *group = NULL;
        (void)pop_action(present, grp, i, &group);
        Entry e = {0};
        avp_vec_push(&e.items, acts.v[i]);
        if (group) {
            e.group = group;
            for (size_t k = 0; k < group->actions.n; k++) {
                const AvpArgAction *a2 = group->actions.v[k];
                if (!a2->nopts) continue;
                for (size_t j = 0; j < acts.n; j++) {
                    if (acts.v[j] != a2) continue;
                    AvpArgMutex *g2 = NULL;
                    if (pop_action(present, grp, j, &g2) && g2) avp_vec_push(&e.items, a2);
                }
            }
        }
        avp_vec_push(&optionals, e);
    }

    AvpStrVec parts = {0};
    size_t pstart = (size_t)-1;
    size_t total = optionals.n + positionals.n;
    for (size_t gi = 0; gi < total; gi++) {
        Entry *e = gi < optionals.n ? &optionals.v[gi] : &positionals.v[gi - optionals.n];
        size_t start = parts.n;
        if (gi == optionals.n) pstart = start;
        bool in_group = e->items.n > 1;
        bool required = e->group && e->group->required;
        for (size_t k = 0; k < e->items.n; k++) {
            const AvpArgAction *act = e->items.v[k];
            AvpBuf part;
            avp_buf_init(&part, f->a);
            if (!act->nopts) {
                const char *dflt = default_metavar(f, act, false);
                const char *s = format_args(f, act, dflt);
                size_t n = strlen(s);
                if (in_group && n >= 2 && s[0] == '[' && s[n - 1] == ']') avp_buf_putn(&part, s + 1, n - 2);
                else avp_buf_putz(&part, s);
            } else {
                const char *option_string = act->opts[0];
                if (act->nargs == 0) {
                    avp_buf_putz(&part, action_format_usage(f, act));
                } else {
                    const char *dflt = default_metavar(f, act, true);
                    const char *args = format_args(f, act, dflt);
                    avp_buf_printf(&part, "%s %s", option_string, args);
                }
                if (!(act->required || required || in_group)) {
                    AvpBuf wrapped;
                    avp_buf_init(&wrapped, f->a);
                    avp_buf_putc(&wrapped, '[');
                    avp_buf_put(&wrapped, avp_buf_str(&part));
                    avp_buf_putc(&wrapped, ']');
                    part = wrapped;
                }
            }
            avp_vec_push(&parts, avp_buf_str(&part));
        }
        if (in_group) {
            AvpBuf first;
            avp_buf_init(&first, f->a);
            avp_buf_putc(&first, required ? '(' : '[');
            avp_buf_put(&first, parts.v[start]);
            parts.v[start] = avp_buf_str(&first);
            for (size_t i = start; i + 1 < parts.n; i++) {
                AvpBuf t;
                avp_buf_init(&t, f->a);
                avp_buf_put(&t, parts.v[i]);
                avp_buf_putz(&t, " |");
                parts.v[i] = avp_buf_str(&t);
            }
            AvpBuf last;
            avp_buf_init(&last, f->a);
            avp_buf_put(&last, parts.v[parts.n - 1]);
            avp_buf_putc(&last, required ? ')' : ']');
            parts.v[parts.n - 1] = avp_buf_str(&last);
        }
    }
    if (pstart == (size_t)-1) pstart = parts.n;
    *pos_start = pstart;

    for (size_t i = 0; i < optionals.n; i++) avp_vec_free(&optionals.v[i].items);
    for (size_t i = 0; i < positionals.n; i++) avp_vec_free(&positionals.v[i].items);
    avp_vec_free(&optionals);
    avp_vec_free(&positionals);
    avp_vec_free(&acts);
    avp_xfree(grp);
    avp_xfree(present);
    return parts;
}

/* get_lines() of _format_usage: wraps the parts into lines of at most text_width. */
static AvpStrVec usage_get_lines(Fmt *f, const AvpStr *parts, size_t nparts, AvpStr indent, const char *prefix,
                                 size_t text_width) {
    AvpStrVec lines = {0};
    size_t indent_length = argp_cplen(indent);
    i64 line_len = prefix ? (i64)argp_cplen_z(prefix) - 1 : (i64)indent_length - 1;
    AVP_VEC(AvpStr) line = {0};
    for (size_t i = 0; i < nparts; i++) {
        i64 part_len = (i64)argp_cplen(parts[i]);
        if (line_len + 1 + part_len > (i64)text_width && line.n) {
            AvpBuf b;
            avp_buf_init(&b, f->a);
            avp_buf_put(&b, indent);
            avp_buf_put(&b, avp_str_join(f->a, AVP_S(" "), line.v, line.n));
            avp_vec_push(&lines, avp_buf_str(&b));
            line.n = 0;
            line_len = (i64)indent_length - 1;
        }
        avp_vec_push(&line, parts[i]);
        line_len += part_len + 1;
    }
    if (line.n) {
        AvpBuf b;
        avp_buf_init(&b, f->a);
        avp_buf_put(&b, indent);
        avp_buf_put(&b, avp_str_join(f->a, AVP_S(" "), line.v, line.n));
        avp_vec_push(&lines, avp_buf_str(&b));
    }
    if (prefix && lines.n) lines.v[0] = avp_str_slice(lines.v[0], (i64)indent_length, AVP_END);
    avp_vec_free(&line);
    return lines;
}

/* HelpFormatter._format_usage */
static void format_usage_item(Fmt *f, AvpBuf *out, const FItem *it) {
    const char *prefix = it->prefix ? it->prefix : "usage: ";
    AvpStr usage;
    AvpBuf ub;
    avp_buf_init(&ub, f->a);
    if (it->usage) {
        usage = argp_percent_prog(f->a, it->usage, f->prog);
    } else if (!it->nacts) {
        usage = avp_str(f->prog);
    } else {
        AvpStr prog = avp_str(f->prog);
        size_t pos_start;
        AvpStrVec parts = usage_parts(f, it->acts, it->nacts, it->groups, it->ngroups, &pos_start);
        AVP_VEC(AvpStr) all = {0};
        avp_vec_push(&all, prog);
        for (size_t i = 0; i < parts.n; i++)
            if (parts.v[i].n) avp_vec_push(&all, parts.v[i]);
        usage = avp_str_join(f->a, AVP_S(" "), all.v, all.n);
        avp_vec_free(&all);
        size_t text_width = (size_t)(f->width - f->cur_indent);
        size_t prefix_len = argp_cplen_z(prefix);
        if (prefix_len + argp_cplen(usage) > text_width) {
            const AvpStr *opt_parts = parts.v;
            size_t n_opt = pos_start;
            const AvpStr *pos_parts = parts.v + pos_start;
            size_t n_pos = parts.n - pos_start;
            size_t prog_len = argp_cplen(prog);
            AvpStrVec lines = {0};
            if ((double)(prefix_len + prog_len) <= 0.75 * (double)text_width) {
                AvpBuf ib;
                avp_buf_init(&ib, f->a);
                put_spaces(&ib, prefix_len + prog_len + 1);
                AvpStr indent = avp_buf_str(&ib);
                if (n_opt) {
                    AVP_VEC(AvpStr) first = {0};
                    avp_vec_push(&first, prog);
                    for (size_t i = 0; i < n_opt; i++) avp_vec_push(&first, opt_parts[i]);
                    AvpStrVec l1 = usage_get_lines(f, first.v, first.n, indent, prefix, text_width);
                    for (size_t i = 0; i < l1.n; i++) avp_vec_push(&lines, l1.v[i]);
                    AvpStrVec l2 = usage_get_lines(f, pos_parts, n_pos, indent, NULL, text_width);
                    for (size_t i = 0; i < l2.n; i++) avp_vec_push(&lines, l2.v[i]);
                    avp_vec_free(&first);
                    avp_vec_free(&l1);
                    avp_vec_free(&l2);
                } else if (n_pos) {
                    AVP_VEC(AvpStr) first = {0};
                    avp_vec_push(&first, prog);
                    for (size_t i = 0; i < n_pos; i++) avp_vec_push(&first, pos_parts[i]);
                    AvpStrVec l1 = usage_get_lines(f, first.v, first.n, indent, prefix, text_width);
                    for (size_t i = 0; i < l1.n; i++) avp_vec_push(&lines, l1.v[i]);
                    avp_vec_free(&first);
                    avp_vec_free(&l1);
                } else avp_vec_push(&lines, prog);
            } else {
                AvpBuf ib;
                avp_buf_init(&ib, f->a);
                put_spaces(&ib, prefix_len);
                AvpStr indent = avp_buf_str(&ib);
                AvpStrVec l = usage_get_lines(f, parts.v, parts.n, indent, NULL, text_width);
                AvpStrVec body = {0};
                if (l.n > 1) {
                    AvpStrVec l1 = usage_get_lines(f, opt_parts, n_opt, indent, NULL, text_width);
                    AvpStrVec l2 = usage_get_lines(f, pos_parts, n_pos, indent, NULL, text_width);
                    for (size_t i = 0; i < l1.n; i++) avp_vec_push(&body, l1.v[i]);
                    for (size_t i = 0; i < l2.n; i++) avp_vec_push(&body, l2.v[i]);
                    avp_vec_free(&l1);
                    avp_vec_free(&l2);
                } else {
                    for (size_t i = 0; i < l.n; i++) avp_vec_push(&body, l.v[i]);
                }
                avp_vec_push(&lines, prog);
                for (size_t i = 0; i < body.n; i++) avp_vec_push(&lines, body.v[i]);
                avp_vec_free(&l);
                avp_vec_free(&body);
            }
            usage = avp_str_join(f->a, AVP_S("\n"), lines.v, lines.n);
            avp_vec_free(&lines);
        }
        avp_vec_free(&parts);
        /* usage = usage.removeprefix(prog); usage = f"{prog}{usage}" */
        usage = avp_str_removeprefix(usage, prog);
        AvpBuf nb;
        avp_buf_init(&nb, f->a);
        avp_buf_put(&nb, prog);
        avp_buf_put(&nb, usage);
        usage = avp_buf_str(&nb);
    }
    avp_buf_putz(out, prefix);
    avp_buf_put(out, usage);
    avp_buf_putz(out, "\n\n");
}

/* ---- text and actions -------------------------------------------------------------------------------------------- */
static void format_text_item(Fmt *f, AvpBuf *out, const char *text) {
    AvpStr t = avp_str(text);
    if (strstr(text, "%(prog)")) t = argp_percent_prog(f->a, text, f->prog);
    int text_width = f->width - f->cur_indent;
    if (text_width < 11) text_width = 11;
    AvpBuf ib;
    avp_buf_init(&ib, f->a);
    put_spaces(&ib, (size_t)f->cur_indent);
    AvpStr filled = fill_text(f, t, text_width, avp_buf_str(&ib));
    avp_buf_put(out, filled);
    avp_buf_putz(out, "\n\n");
}

/* '%*s%-*s  ' style padding: the string left-justified to width code points */
static void put_ljust(AvpBuf *b, const char *s, size_t width) {
    avp_buf_putz(b, s);
    size_t n = argp_cplen_z(s);
    if (n < width) put_spaces(b, width - n);
}

static void format_action_item(Fmt *f, AvpBuf *out, const AvpArgAction *act) {
    int help_position = f->action_max_length + 2;
    if (help_position > f->max_help_position) help_position = f->max_help_position;
    int help_width = f->width - help_position;
    if (help_width < 11) help_width = 11;
    int action_width = help_position - f->cur_indent - 2;
    const char *header = format_invocation(f, act);
    size_t header_len = argp_cplen_z(header);
    AvpBuf parts;
    avp_buf_init(&parts, f->a);
    int indent_first = 0;
    bool header_has_nl;
    if (!act->help || !act->help[0]) {
        put_spaces(&parts, (size_t)f->cur_indent);
        avp_buf_putz(&parts, header);
        avp_buf_putc(&parts, '\n');
        header_has_nl = true;
    } else if ((i64)header_len <= action_width) {
        put_spaces(&parts, (size_t)f->cur_indent);
        put_ljust(&parts, header, (size_t)(action_width > 0 ? action_width : 0));
        avp_buf_putz(&parts, "  ");
        indent_first = 0;
        header_has_nl = false;
    } else {
        put_spaces(&parts, (size_t)f->cur_indent);
        avp_buf_putz(&parts, header);
        avp_buf_putc(&parts, '\n');
        indent_first = help_position;
        header_has_nl = true;
    }
    if (act->help && avp_str_strip(avp_str(act->help)).n) {
        AvpStr help_text = expand_help(f, act);
        if (help_text.n) {
            AvpStrVec hl = split_lines(f, help_text, help_width);
            if (hl.n) {
                put_spaces(&parts, (size_t)indent_first);
                avp_buf_put(&parts, hl.v[0]);
                avp_buf_putc(&parts, '\n');
                for (size_t i = 1; i < hl.n; i++) {
                    put_spaces(&parts, (size_t)help_position);
                    avp_buf_put(&parts, hl.v[i]);
                    avp_buf_putc(&parts, '\n');
                }
            }
            avp_vec_free(&hl);
        }
    } else if (!header_has_nl) {
        avp_buf_putc(&parts, '\n');
    }
    avp_buf_put(out, avp_buf_str(&parts));
    const ActionVec *subs = subactions(act);
    if (subs) {
        f->cur_indent += f->indent_inc;
        for (size_t i = 0; i < subs->n; i++) format_action_item(f, out, subs->v[i]);
        f->cur_indent -= f->indent_inc;
    }
}

static void format_section(Fmt *f, FSec *s, AvpBuf *out);

static void format_item(Fmt *f, AvpBuf *out, const FItem *it) {
    switch (it->kind) {
    case F_USAGE: format_usage_item(f, out, it); break;
    case F_TEXT: format_text_item(f, out, it->text); break;
    case F_ACTION: format_action_item(f, out, it->act); break;
    default: format_section(f, it->sec, out); break;
    }
}

/* _Section.format_help */
static void format_section(Fmt *f, FSec *s, AvpBuf *out) {
    if (s->parent) f->cur_indent += f->indent_inc;
    AvpBuf items;
    avp_buf_init(&items, f->a);
    for (size_t i = 0; i < s->items.n; i++) format_item(f, &items, &s->items.v[i]);
    if (s->parent) f->cur_indent -= f->indent_inc;
    if (!items.n) return;
    if (s->parent == NULL) {
        avp_buf_put(out, avp_buf_str(&items));
        return;
    }
    avp_buf_putc(out, '\n');
    if (s->heading) {
        put_spaces(out, (size_t)f->cur_indent);
        avp_buf_putz(out, s->heading);
        avp_buf_putz(out, ":\n");
    }
    avp_buf_put(out, avp_buf_str(&items));
    avp_buf_putc(out, '\n');
}

/* HelpFormatter.format_help */
static AvpStr fmt_format_help(Fmt *f) {
    AvpBuf raw;
    avp_buf_init(&raw, f->a);
    format_section(f, &f->root, &raw);
    AvpStr h = avp_buf_str(&raw);
    if (!h.n) return h;
    /* re.sub(r'\n\n\n+', '\n\n') then strip('\n') + '\n' */
    AvpBuf out;
    avp_buf_init(&out, f->a);
    for (size_t i = 0; i < h.n;) {
        if (h.p[i] == '\n') {
            size_t j = i;
            while (j < h.n && h.p[j] == '\n') j++;
            size_t run = j - i;
            if (run >= 3) run = 2;
            for (size_t k = 0; k < run; k++) avp_buf_putc(&out, '\n');
            i = j;
        } else avp_buf_putc(&out, h.p[i++]);
    }
    AvpStr s = avp_buf_str(&out);
    size_t a0 = 0, b0 = s.n;
    while (a0 < b0 && s.p[a0] == '\n') a0++;
    while (b0 > a0 && s.p[b0 - 1] == '\n') b0--;
    AvpBuf fin;
    avp_buf_init(&fin, f->a);
    avp_buf_putn(&fin, s.p + a0, b0 - a0);
    avp_buf_putc(&fin, '\n');
    return avp_buf_str(&fin);
}

/* ---- the parser's views ------------------------------------------------------------------------------------------ */
AvpStr argp_format_usage(AvpArgParser *p) {
    Fmt f;
    fmt_init(&f, p, p->prog);
    add_usage(&f, p->usage, (const AvpArgAction *const *)p->actions.v, p->actions.n, p->mutexes.v, p->mutexes.n, NULL);
    AvpStr r = fmt_format_help(&f);
    fmt_free(&f.root);
    return r;
}

AvpStr argp_format_help(AvpArgParser *p) {
    Fmt f;
    fmt_init(&f, p, p->prog);
    add_usage(&f, p->usage, (const AvpArgAction *const *)p->actions.v, p->actions.n, p->mutexes.v, p->mutexes.n, NULL);
    add_text(&f, p->description);
    for (size_t i = 0; i < p->sections.n; i++) {
        AvpArgSection *s = p->sections.v[i];
        start_section(&f, s->title);
        add_text(&f, s->description);
        for (size_t k = 0; k < s->actions.n; k++) add_argument(&f, s->actions.v[k]);
        end_section(&f);
    }
    add_text(&f, p->epilog);
    AvpStr r = fmt_format_help(&f);
    fmt_free(&f.root);
    return r;
}

const char *argp_subparsers_prog(AvpArgParser *p) {
    Fmt f;
    fmt_init(&f, p, p->prog);
    AVP_VEC(const AvpArgAction *) pos = {0};
    for (size_t i = 0; i < p->actions.n; i++)
        if (!p->actions.v[i]->nopts) avp_vec_push(&pos, p->actions.v[i]);
    add_usage(&f, NULL, pos.v, pos.n, p->mutexes.v, p->mutexes.n, "");
    AvpStr r = fmt_format_help(&f);
    fmt_free(&f.root);
    avp_vec_free(&pos);
    /* format_help().strip() */
    return avp_cz(p->a, avp_str_strip(r));
}

AvpStr argp_format_version(AvpArgParser *p, const char *version) {
    Fmt f;
    fmt_init(&f, p, p->prog);
    add_text(&f, version);
    AvpStr r = fmt_format_help(&f);
    fmt_free(&f.root);
    return r;
}

/* _get_action_name */
const char *argp_action_name(AvpArena *a, const AvpArgAction *act) {
    if (!act) return NULL;
    if (act->nopts) {
        AvpBuf b;
        avp_buf_init(&b, a);
        for (size_t i = 0; i < act->nopts; i++) {
            if (i) avp_buf_putc(&b, '/');
            avp_buf_putz(&b, act->opts[i]);
        }
        return avp_buf_cz(&b);
    }
    if (act->metavars) {
        AvpBuf b;
        avp_buf_init(&b, a);
        if ((act->nargs == AVP_NARGS_STAR && act->nmetavars == 2) || act->nargs == AVP_NARGS_PLUS) {
            avp_buf_printf(&b, "%s[, %s]", act->metavars[0], act->metavars[1]);
        } else {
            for (size_t i = 0; i < act->nmetavars; i++) {
                if (i) avp_buf_putz(&b, ", ");
                avp_buf_putz(&b, act->metavars[i]);
            }
        }
        return avp_buf_cz(&b);
    }
    if (act->metavar) return act->metavar;
    if (act->dest) return act->dest;
    if (act->kind == ARGK_SUBPARSERS && act->sub->names.n) {
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_putc(&b, '{');
        for (size_t i = 0; i < act->sub->names.n; i++) {
            if (i) avp_buf_putc(&b, ',');
            avp_buf_putz(&b, act->sub->names.v[i]);
        }
        avp_buf_putc(&b, '}');
        return avp_buf_cz(&b);
    }
    if (act->choices && act->nchoices) {
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_putc(&b, '{');
        for (size_t i = 0; i < act->nchoices; i++) {
            if (i) avp_buf_putc(&b, ',');
            avp_buf_putz(&b, act->choices[i]);
        }
        avp_buf_putc(&b, '}');
        return avp_buf_cz(&b);
    }
    return NULL;
}
