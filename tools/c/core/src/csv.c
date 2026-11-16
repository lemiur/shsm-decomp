/* avpcore csv: Python's csv.reader / csv.DictReader / csv.writer / csv.DictWriter (CPython 3.14's _csv.c and
 * csv.py) over UTF-8 text. The TSV helpers are in csv_tsv.c. */
#include "avp/csv.h"
#include "avp/arena.h"
#include <stdio.h>

#include "avp/fmt.h"

const AvpCsvDialect avp_csv_excel = {',', '"', -1, true, false, false, AVP_CSV_MINIMAL, {"\r\n", 2}};
const AvpCsvDialect avp_csv_excel_tab = {'\t', '"', -1, true, false, false, AVP_CSV_MINIMAL, {"\r\n", 2}};
const AvpCsvDialect avp_csv_unix = {',', '"', -1, true, false, false, AVP_CSV_ALL, {"\n", 1}};

AvpStr avp_csv_strip_bom(AvpStr text) {
    if (text.n >= 3 && memcmp(text.p, "\xEF\xBB\xBF", 3) == 0) return avp_strn(text.p + 3, text.n - 3);
    return text;
}

/* A dialect without a quote character cannot quote (Python's default for quotechar=None). */
static AvpCsvDialect normalize(const AvpCsvDialect *d) {
    AvpCsvDialect n = *d;
    AVP_ASSERT(n.delimiter >= 0 && n.delimiter < 128);
    AVP_ASSERT(n.quotechar < 128 && n.escapechar < 128);
    if (n.quotechar < 0) n.quoting = AVP_CSV_NONE;
    return n;
}

/* ---- the reader: CPython's parse_process_char ---- */
enum {
    START_RECORD,
    START_FIELD,
    ESCAPED_CHAR,
    IN_FIELD,
    IN_QUOTED_FIELD,
    ESCAPE_IN_QUOTED_FIELD,
    QUOTE_IN_QUOTED_FIELD,
    EAT_CRNL,
    AFTER_ESCAPED_CRNL
};
#define EOL (-2) /* the end of a line, as CPython's EOL: never a byte value and never "no such character" (-1) */

void avp_csv_reader_init(AvpCsvReader *rd, const AvpCsvDialect *d, AvpStr text, AvpCsvLineMode mode) {
    memset(rd, 0, sizeof *rd);
    rd->d = normalize(d);
    rd->text = text;
    rd->mode = (int)mode;
    rd->field_limit = 131072;
    const AvpCsvDialect *x = &rd->d;
    u8 *f = rd->special[0], *q = rd->special[1];
    f['\n'] = f['\r'] = 1;
    f[x->delimiter] = 1;
    if (x->escapechar >= 0) f[x->escapechar] = q[x->escapechar] = 1;
    if (x->quoting != AVP_CSV_NONE && x->quotechar >= 0) q[x->quotechar] = 1;
}

void avp_csv_reader_init_lines(AvpCsvReader *rd, const AvpCsvDialect *d, const AvpStr *lines, size_t n) {
    avp_csv_reader_init(rd, d, avp_strn("", 0), AVP_CSV_RAW);
    rd->mode = -1;
    rd->lines = lines;
    rd->nlines = n;
}

void avp_csv_reader_keep(AvpCsvReader *rd, AvpArena *a) { rd->keep = a; }

void avp_csv_reader_free(AvpCsvReader *rd) {
    avp_vec_free(&rd->row);
    avp_buf_free(&rd->cur);
    if (rd->own) avp_arena_free(rd->own);
    rd->own = NULL;
}

static bool reader_fail(AvpCsvReader *rd, const char *msg) {
    rd->error = avp_str(msg);
    return false;
}

static size_t field_len(const AvpCsvReader *rd) { return rd->in_cur ? rd->cur.n : rd->view_n; }

static bool check_limit(AvpCsvReader *rd) {
    size_t n = field_len(rd);
    if (n <= rd->field_limit) return true;
    /* the limit counts characters, not bytes */
    const char *p = rd->in_cur ? rd->cur.p : rd->view_p;
    size_t cps = 0;
    for (size_t i = 0; i < n; i++) cps += ((unsigned char)p[i] & 0xC0) != 0x80;
    if (cps <= rd->field_limit) return true;
    return reader_fail(rd, "field larger than field limit (131072)");
}

/* Appends n bytes of the text itself to the current field: a view while the field is one contiguous run. */
static bool add_run(AvpCsvReader *rd, const char *p, size_t n) {
    if (!rd->in_cur) {
        if (rd->view_n == 0) {
            rd->view_p = p;
            rd->view_n = n;
            return n <= rd->field_limit || check_limit(rd);
        }
        if (rd->view_p + rd->view_n == p) {
            rd->view_n += n;
            return rd->view_n <= rd->field_limit || check_limit(rd);
        }
        avp_buf_clear(&rd->cur);
        avp_buf_putn(&rd->cur, rd->view_p, rd->view_n);
        rd->in_cur = true;
    }
    avp_buf_putn(&rd->cur, p, n);
    return rd->cur.n <= rd->field_limit || check_limit(rd);
}

static bool add_synth(AvpCsvReader *rd, char c) {
    if (!rd->in_cur) {
        avp_buf_clear(&rd->cur);
        if (rd->view_n) avp_buf_putn(&rd->cur, rd->view_p, rd->view_n);
        rd->in_cur = true;
    }
    avp_buf_putc(&rd->cur, c);
    return rd->cur.n <= rd->field_limit || check_limit(rd);
}

static bool add_char(AvpCsvReader *rd, int c, const char *src) {
    if (src && c >= 0) return add_run(rd, src, 1);
    return add_synth(rd, (char)c);
}

static void save_field(AvpCsvReader *rd) {
    AvpStr f;
    if (rd->in_cur) {
        AvpArena *a = rd->keep;
        if (!a) {
            if (!rd->own) {
                rd->own = avp_arena_new(1 << 16);
                rd->own_mark = avp_arena_mark(rd->own);
            }
            a = rd->own;
        }
        f = avp_str_dup(a, avp_buf_str(&rd->cur));
    } else {
        f = (AvpStr){rd->view_n ? rd->view_p : "", rd->view_n};
    }
    avp_vec_push(&rd->row, f);
    rd->view_p = NULL;
    rd->view_n = 0;
    rd->in_cur = false;
}

/* One character (c < 0 never; EOL is the end of the line). src is the character's address in the text, or NULL. */
static bool step(AvpCsvReader *rd, int c, const char *src) {
    const AvpCsvDialect *d = &rd->d;
    bool can_quote = d->quoting != AVP_CSV_NONE;
    switch (rd->state) {
    case START_RECORD:
        if (c == EOL) break;
        if (c == '\n' || c == '\r') {
            rd->state = EAT_CRNL;
            break;
        }
        rd->state = START_FIELD;
        /* fall through: a normal character starts a field */
    case START_FIELD:
        if (c == '\n' || c == '\r' || c == EOL) {
            save_field(rd);
            rd->state = c == EOL ? START_RECORD : EAT_CRNL;
        } else if (c == d->quotechar && can_quote) {
            rd->state = IN_QUOTED_FIELD;
        } else if (c == d->escapechar) {
            rd->state = ESCAPED_CHAR;
        } else if (c == ' ' && d->skipinitialspace) {
            /* ignore a space at the start of a field */
        } else if (c == d->delimiter) {
            save_field(rd);
        } else {
            if (!add_char(rd, c, src)) return false;
            rd->state = IN_FIELD;
        }
        break;
    case ESCAPED_CHAR:
        if (c == '\n' || c == '\r') {
            if (!add_char(rd, c, src)) return false;
            rd->state = AFTER_ESCAPED_CRNL;
            break;
        }
        if (c == EOL) {
            c = '\n';
            src = NULL;
        }
        if (!add_char(rd, c, src)) return false;
        rd->state = IN_FIELD;
        break;
    case AFTER_ESCAPED_CRNL:
        if (c == EOL) break;
        /* fall through */
    case IN_FIELD:
        if (c == '\n' || c == '\r' || c == EOL) {
            save_field(rd);
            rd->state = c == EOL ? START_RECORD : EAT_CRNL;
        } else if (c == d->escapechar) {
            rd->state = ESCAPED_CHAR;
        } else if (c == d->delimiter) {
            save_field(rd);
            rd->state = START_FIELD;
        } else {
            /* the state is left as it is: after an escaped newline (AFTER_ESCAPED_CRNL) it stays, as in CPython */
            if (!add_char(rd, c, src)) return false;
        }
        break;
    case IN_QUOTED_FIELD:
        if (c == EOL) {
            /* the line break of a field that goes on in the next line is not part of the field */
        } else if (c == d->escapechar) {
            rd->state = ESCAPE_IN_QUOTED_FIELD;
        } else if (c == d->quotechar && can_quote) {
            rd->state = d->doublequote ? QUOTE_IN_QUOTED_FIELD : IN_FIELD;
        } else {
            if (!add_char(rd, c, src)) return false;
        }
        break;
    case ESCAPE_IN_QUOTED_FIELD:
        if (c == EOL) {
            c = '\n';
            src = NULL;
        }
        if (!add_char(rd, c, src)) return false;
        rd->state = IN_QUOTED_FIELD;
        break;
    case QUOTE_IN_QUOTED_FIELD:
        if (can_quote && c == d->quotechar) {
            if (!add_char(rd, c, src)) return false;
            rd->state = IN_QUOTED_FIELD;
        } else if (c == d->delimiter) {
            save_field(rd);
            rd->state = START_FIELD;
        } else if (c == '\n' || c == '\r' || c == EOL) {
            save_field(rd);
            rd->state = c == EOL ? START_RECORD : EAT_CRNL;
        } else if (!d->strict) {
            if (!add_char(rd, c, src)) return false;
            rd->state = IN_FIELD;
        } else {
            snprintf(rd->errbuf, sizeof rd->errbuf, "'%c' expected after '%c'", d->delimiter, d->quotechar);
            rd->error = avp_str(rd->errbuf);
            return false;
        }
        break;
    case EAT_CRNL:
        if (c == '\n' || c == '\r') {
            /* eaten */
        } else if (c == EOL) {
            rd->state = START_RECORD;
        } else {
            return reader_fail(rd, "new-line character seen in unquoted field - do you need to open the file with newline=''?");
        }
        break;
    }
    return true;
}

/* The characters p[0..e) of the text. Runs of ordinary characters in the two scanning states are taken at once. */
static bool feed(AvpCsvReader *rd, const char *p, const char *e) {
    while (p < e) {
        if (rd->state == IN_FIELD || rd->state == IN_QUOTED_FIELD) {
            const u8 *sp = rd->special[rd->state == IN_QUOTED_FIELD];
            const char *q = p;
            while (q < e && !sp[(u8)*q]) q++;
            if (q != p) {
                if (!add_run(rd, p, (size_t)(q - p))) return false;
                p = q;
                if (p == e) break;
            }
        }
        if (!step(rd, (u8)*p, p)) return false;
        p++;
    }
    return true;
}

/* The next line of the input: its characters [*bp, *be) and whether a "\n" the text did not have in that form
 * (universal newlines) ends it. false at the end of the input. */
static bool next_line(AvpCsvReader *rd, const char **bp, const char **be, bool *synth_nl) {
    *synth_nl = false;
    if (rd->mode < 0) {
        if (rd->next_line >= rd->nlines) return false;
        AvpStr l = rd->lines[rd->next_line++];
        *bp = l.p;
        *be = l.p + l.n;
        return true;
    }
    const char *p = rd->text.p + rd->pos, *end = rd->text.p + rd->text.n;
    if (p >= end) return false;
    *bp = p;
    const char *q = p;
    if (rd->mode == AVP_CSV_LF) {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        q = nl ? nl + 1 : end;
        *be = q;
    } else {
        while (q < end && *q != '\n' && *q != '\r') q++;
        if (q < end) {
            const char *after = q + 1;
            if (*q == '\r' && after < end && *after == '\n') after++;
            if (rd->mode == AVP_CSV_RAW) {
                *be = after;
            } else {
                *be = q;
                *synth_nl = true;
            }
            q = after;
        } else {
            *be = q;
        }
    }
    rd->pos = (size_t)(q - rd->text.p);
    return true;
}

int avp_csv_next(AvpCsvReader *rd) {
    rd->row.n = 0;
    rd->error = (AvpStr){NULL, 0};
    rd->state = START_RECORD;
    rd->view_p = NULL;
    rd->view_n = 0;
    rd->in_cur = false;
    if (rd->own) avp_arena_reset(rd->own, rd->own_mark);
    do {
        const char *bp, *be;
        bool synth_nl;
        if (!next_line(rd, &bp, &be, &synth_nl)) {
            /* the end of the input: a field in progress is the last row (or "unexpected end of data") */
            if (field_len(rd) != 0 || rd->state == IN_QUOTED_FIELD) {
                if (rd->d.strict) {
                    reader_fail(rd, "unexpected end of data");
                    return -1;
                }
                save_field(rd);
                return 1;
            }
            return 0;
        }
        rd->line_num++;
        if (!feed(rd, bp, be)) return -1;
        if (synth_nl && !step(rd, '\n', NULL)) return -1;
        if (!step(rd, EOL, NULL)) return -1;
    } while (rd->state != START_RECORD);
    return 1;
}

/* ---- DictReader ---- */
static u32 name_hash(AvpStr s) {
    u32 h = 2166136261u;
    for (size_t i = 0; i < s.n; i++) h = (h ^ (u8)s.p[i]) * 16777619u;
    return h;
}

/* The slot of name in the index: the one holding it, or the empty slot where it goes. */
static u32 *index_slot(u32 *tab, u32 cap, const AvpStrVec *names, AvpStr name) {
    u32 mask = cap - 1, i = name_hash(name) & mask;
    while (tab[i] && !avp_str_eq(names->v[tab[i] - 1], name)) i = (i + 1) & mask;
    return &tab[i];
}

void avp_csv_dictreader_init(AvpCsvDictReader *dr, AvpArena *a, const AvpCsvReader *rd) {
    memset(dr, 0, sizeof *dr);
    dr->rd = *rd;
    dr->a = a ? a : avp_arena();
}

void avp_csv_dictreader_free(AvpCsvDictReader *dr) {
    avp_csv_reader_free(&dr->rd);
    avp_vec_free(&dr->names);
    avp_vec_free(&dr->cells);
    avp_vec_free(&dr->rest);
    avp_xfree(dr->index);
    dr->index = NULL;
}

const AvpStrVec *avp_csv_dictreader_fieldnames(AvpCsvDictReader *dr, int *status) {
    int st = 1;
    if (!dr->have_names) {
        st = avp_csv_next(&dr->rd);
        if (st > 0) {
            dr->have_names = true;
            dr->index_cap = 16;
            while (dr->index_cap < dr->rd.row.n * 2) dr->index_cap <<= 1;
            dr->index = (u32 *)avp_xcalloc(dr->index_cap, sizeof(u32));
            for (size_t i = 0; i < dr->rd.row.n; i++) {
                AvpStr n = avp_str_dup(dr->a, dr->rd.row.v[i]);
                avp_vec_push(&dr->names, n);
                /* a repeated name: the last position wins, as dict(zip(fieldnames, row)) */
                u32 *slot = index_slot(dr->index, dr->index_cap, &dr->names, n);
                *slot = (u32)i + 1;
            }
        }
        dr->line_num = dr->rd.line_num;
    }
    if (status) *status = st;
    return dr->have_names ? &dr->names : NULL;
}

int avp_csv_dict_next(AvpCsvDictReader *dr) {
    if (!dr->have_names) {
        int st;
        avp_csv_dictreader_fieldnames(dr, &st);
        if (st <= 0) return st;
    }
    int st = avp_csv_next(&dr->rd);
    if (st <= 0) return st;
    dr->line_num = dr->rd.line_num;
    while (dr->rd.row.n == 0) { /* a blank line is the empty list: skipped */
        st = avp_csv_next(&dr->rd);
        if (st <= 0) return st;
    }
    dr->line_num = dr->rd.line_num; /* csv.py's self.fieldnames getter, read after the loop, refreshes it */
    size_t lf = dr->names.n, lr = dr->rd.row.n;
    avp_vec_clear(&dr->cells);
    avp_vec_clear(&dr->rest);
    for (size_t i = 0; i < lf; i++) avp_vec_push(&dr->cells, i < lr ? dr->rd.row.v[i] : dr->restval);
    dr->rest_present = lr > lf;
    for (size_t i = lf; i < lr; i++) avp_vec_push(&dr->rest, dr->rd.row.v[i]);
    return 1;
}

AvpStr avp_csv_dict_get(const AvpCsvDictReader *dr, AvpStr name) {
    if (!dr->index || dr->cells.n != dr->names.n) return (AvpStr){NULL, 0};
    u32 idx = *index_slot(dr->index, dr->index_cap, &dr->names, name);
    if (!idx) return (AvpStr){NULL, 0};
    return dr->cells.v[idx - 1];
}

AvpStr avp_csv_dict_getz(const AvpCsvDictReader *dr, const char *name) { return avp_csv_dict_get(dr, avp_str(name)); }

bool avp_csv_dict_has(const AvpCsvDictReader *dr, AvpStr name) {
    if (dr->index && *index_slot(dr->index, dr->index_cap, &dr->names, name)) return true;
    return dr->rest_present && dr->restkey.p && avp_str_eq(dr->restkey, name);
}

/* ---- the writer: CPython's join_append / csv_writerow ---- */
void avp_csvw_init(AvpCsvWriter *w, AvpBuf *out, const AvpCsvDialect *d) {
    memset(w, 0, sizeof *w);
    w->d = normalize(d);
    w->out = out;
}

void avp_csvw_begin(AvpCsvWriter *w) {
    w->row_start = w->out->n;
    w->nfields = 0;
    w->failed = false;
    w->error = (AvpStr){NULL, 0};
    w->last_none = false;
}

static void writer_fail(AvpCsvWriter *w, const char *msg) {
    w->failed = true;
    w->error = avp_str(msg);
}

typedef enum { FK_STR, FK_NONE, FK_NUM } FieldKind;

static bool needs_special(const AvpCsvDialect *d, unsigned char c) {
    if (c == d->delimiter || c == '\n' || c == '\r') return true;
    if ((int)c == d->escapechar || (int)c == d->quotechar) return true;
    for (size_t i = 0; i < d->lineterminator.n; i++)
        if ((unsigned char)d->lineterminator.p[i] == c) return true;
    return false;
}

static void put_field(AvpCsvWriter *w, AvpStr s, FieldKind kind) {
    if (w->failed) return;
    const AvpCsvDialect *d = &w->d;
    AvpBuf *out = w->out;
    bool quoted;
    switch (d->quoting) {
    case AVP_CSV_NONNUMERIC: quoted = kind != FK_NUM; break;
    case AVP_CSV_ALL: quoted = true; break;
    case AVP_CSV_STRINGS: quoted = kind == FK_STR; break;
    case AVP_CSV_NOTNULL: quoted = kind != FK_NONE; break;
    default: quoted = false; break;
    }
    /* an empty field would vanish when read back with skipinitialspace and a space delimiter: it is quoted */
    if (s.n == 0 && d->delimiter == ' ' && d->skipinitialspace) {
        if (d->quoting == AVP_CSV_NONE || (kind == FK_NONE && (d->quoting == AVP_CSV_STRINGS || d->quoting == AVP_CSV_NOTNULL))) {
            writer_fail(w, "empty field must be quoted if delimiter is a space and skipinitialspace is true");
            return;
        }
        quoted = true;
    }
    /* first pass: does the field need quoting, and can it be written at all? */
    size_t extra = 0;
    if (kind != FK_NONE) {
        for (size_t i = 0; i < s.n; i++) {
            unsigned char c = (unsigned char)s.p[i];
            if (!needs_special(d, c)) continue;
            bool want_escape = false;
            if (d->quoting == AVP_CSV_NONE) {
                want_escape = true;
            } else {
                if ((int)c == d->quotechar) {
                    if (d->doublequote) extra++;
                    else want_escape = true;
                } else if ((int)c == d->escapechar) {
                    want_escape = true;
                }
                if (!want_escape) quoted = true;
            }
            if (want_escape) {
                if (d->escapechar < 0) {
                    writer_fail(w, "need to escape, but no escapechar set");
                    return;
                }
                extra++;
            }
        }
    }
    size_t n = (kind == FK_NONE ? 0 : s.n);
    avp_buf_reserve(out, n + extra + 4);
    if (w->nfields > 0) avp_buf_putc(out, (char)d->delimiter);
    if (quoted) avp_buf_putc(out, (char)d->quotechar);
    if (kind == FK_NONE) {
        /* nothing */
    } else if (!extra && !quoted) {
        avp_buf_putn(out, s.p, s.n);
    } else {
        for (size_t i = 0; i < s.n; i++) {
            unsigned char c = (unsigned char)s.p[i];
            if (needs_special(d, c)) {
                bool want_escape = false;
                if (d->quoting == AVP_CSV_NONE) {
                    want_escape = true;
                } else if ((int)c == d->quotechar) {
                    if (d->doublequote) avp_buf_putc(out, (char)d->quotechar);
                    else want_escape = true;
                } else if ((int)c == d->escapechar) {
                    want_escape = true;
                }
                if (want_escape) avp_buf_putc(out, (char)d->escapechar);
            }
            avp_buf_putc(out, (char)c);
        }
    }
    if (quoted) avp_buf_putc(out, (char)d->quotechar);
    w->nfields++;
    w->last_none = kind == FK_NONE;
}

void avp_csvw_str(AvpCsvWriter *w, AvpStr s) { put_field(w, s, FK_STR); }
void avp_csvw_none(AvpCsvWriter *w) { put_field(w, avp_strn("", 0), FK_NONE); }
void avp_csvw_number_text(AvpCsvWriter *w, AvpStr s) { put_field(w, s, FK_NUM); }
void avp_csvw_bool(AvpCsvWriter *w, bool v) { put_field(w, v ? AVP_S("True") : AVP_S("False"), FK_NUM); }

void avp_csvw_i64(AvpCsvWriter *w, i64 v) {
    char tmp[24];
    int n = snprintf(tmp, sizeof tmp, "%lld", (long long)v);
    put_field(w, avp_strn(tmp, (size_t)n), FK_NUM);
}

void avp_csvw_f64(AvpCsvWriter *w, double v) {
    char tmp[32];
    size_t n = avp_float_repr(v, tmp);
    put_field(w, avp_strn(tmp, n), FK_NUM);
}

bool avp_csvw_end(AvpCsvWriter *w) {
    AvpBuf *out = w->out;
    const AvpCsvDialect *d = &w->d;
    if (!w->failed && w->nfields > 0 && out->n == w->row_start) {
        /* a record that is a single empty field would read back as an empty line: it is written as "" */
        if (d->quoting == AVP_CSV_NONE ||
            (w->last_none && (d->quoting == AVP_CSV_STRINGS || d->quoting == AVP_CSV_NOTNULL))) {
            writer_fail(w, "single empty field record must be quoted");
        } else {
            avp_buf_putc(out, (char)d->quotechar);
            avp_buf_putc(out, (char)d->quotechar);
        }
    }
    if (w->failed) {
        out->n = w->row_start;
        if (out->p) out->p[out->n] = 0;
        return false;
    }
    avp_buf_put(out, d->lineterminator);
    return true;
}

bool avp_csvw_row(AvpCsvWriter *w, const AvpStr *fields, size_t n) {
    avp_csvw_begin(w);
    for (size_t i = 0; i < n; i++) avp_csvw_str(w, fields[i]);
    return avp_csvw_end(w);
}

bool avp_csvw_dict_row(AvpCsvWriter *w, size_t nfields, const AvpStr *values, AvpStr restval) {
    avp_csvw_begin(w);
    for (size_t i = 0; i < nfields; i++) {
        AvpStr v = values[i].p ? values[i] : restval;
        if (v.p) avp_csvw_str(w, v);
        else avp_csvw_none(w);
    }
    return avp_csvw_end(w);
}
