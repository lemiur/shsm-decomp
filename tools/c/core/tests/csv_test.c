/* Tests of csv.h against CPython 3.14.8: csv_corpus.h holds thousands of generated cases (csv.reader in the three
 * file modes and over a list of lines, csv.DictReader, csv.writer, csv.DictWriter, line splitting and
 * line.split("\t")); its header comment has the generator and the layout of each case. The hand-written tests
 * below cover what the corpus does not (limits, the API around it); their expected values came from the
 * snippets in their comments. */
#include "avp/avp.h"
#include "avp/csv.h"
#include "avp/test.h"
#include "csv_corpus.h"
#include <stdlib.h>

#define SEP_ITEM '\x1e'
#define SEP_FIELD '\x1f'
#define SEP_KV '\x1d'
#define SEP_LIST '\x1c'

static AvpCsvDialect dialect_for(int id) {
    AvpCsvDialect d = avp_csv_excel;
    switch (id) {
    case 0: break;
    case 1: d.delimiter = '\t'; break;
    case 2: d.delimiter = '\t'; d.strict = true; break;
    case 3: d.skipinitialspace = true; break;
    case 4: d.escapechar = '\\'; break;
    case 5: d.doublequote = false; d.escapechar = '\\'; break;
    case 6: d.quoting = AVP_CSV_NONE; break;
    case 7: d.quoting = AVP_CSV_NONE; d.escapechar = '\\'; break;
    case 8: d.strict = true; break;
    case 9: d.quotechar = '\''; d.delimiter = ';'; break;
    case 10: d.delimiter = ' '; d.skipinitialspace = true; break;
    case 11: d.quoting = AVP_CSV_ALL; d.strict = true; d.escapechar = '\\'; break;
    case 12: d.doublequote = false; break;
    case 13: d.lineterminator = AVP_S("\n"); break;
    case 14: d.lineterminator = AVP_S("|"); break;
    case 15: d.lineterminator = AVP_S(""); break;
    case 16: d.quoting = AVP_CSV_NONNUMERIC; break;
    case 17: d.quoting = AVP_CSV_STRINGS; break;
    case 18: d.quoting = AVP_CSV_NOTNULL; break;
    case 19: d.quotechar = -1; break;
    case 20: d.quoting = AVP_CSV_ALL; d.lineterminator = AVP_S("\n"); break;
    case 21: d.delimiter = ';'; d.escapechar = '\\'; d.doublequote = false; d.lineterminator = AVP_S("\r"); break;
    case 22: d.delimiter = ' '; d.skipinitialspace = true; d.quoting = AVP_CSV_NONE; d.escapechar = '\\'; break;
    case 23: d.delimiter = ' '; d.skipinitialspace = true; d.quoting = AVP_CSV_NOTNULL; break;
    default: AVP_ASSERT(false);
    }
    return d;
}

static void put_item_sep(AvpBuf *b, bool *first) {
    if (!*first) avp_buf_putc(b, SEP_ITEM);
    *first = false;
}

/* The expected-output encoding of the generator, built from what the C reader gives. */
static void run_reader(AvpBuf *out, const AvpCsvCase *c) {
    AvpCsvDialect d = dialect_for(c->dialect);
    AvpCsvReader rd;
    AvpStrVec lines = {0};
    AvpStr text = avp_strn(c->in, c->in_n);
    if (c->mode == 3) {
        /* each line is 'L' + the line, the lines are joined by U+00FF (C3 BF); no lines is the empty input */
        const char *p = text.p, *end = text.p + text.n;
        while (p < end) {
            const char *q = p + 1;
            while (q + 1 < end && !((unsigned char)q[0] == 0xC3 && (unsigned char)q[1] == 0xBF)) q++;
            if (q + 1 >= end) q = end;
            avp_vec_push(&lines, avp_strn(p + 1, (size_t)(q - p) - 1));
            p = q + 2;
        }
        avp_csv_reader_init_lines(&rd, &d, lines.v, lines.n);
    } else {
        avp_csv_reader_init(&rd, &d, text, (AvpCsvLineMode)c->mode);
    }
    bool first = true;
    for (;;) {
        int st = avp_csv_next(&rd);
        if (st == 0) break;
        put_item_sep(out, &first);
        if (st < 0) {
            avp_buf_printf(out, "E%lld%c", (long long)rd.line_num, SEP_FIELD);
            avp_buf_put(out, rd.error);
            continue;
        }
        avp_buf_printf(out, "R%lld", (long long)rd.line_num);
        for (size_t i = 0; i < rd.row.n; i++) {
            avp_buf_putc(out, SEP_FIELD);
            avp_buf_put(out, rd.row.v[i]);
        }
    }
    avp_csv_reader_free(&rd);
    avp_vec_free(&lines);
}

static void run_writer(AvpBuf *out, const AvpCsvCase *c) {
    AvpCsvDialect d = dialect_for(c->dialect);
    const char *p = c->in, *end = c->in + c->in_n;
    bool first = true;
    /* rows are separated by 0x1E, fields by 0x1F; an empty input is a single empty row */
    for (;;) {
        const char *rend = p;
        while (rend < end && *rend != SEP_ITEM) rend++;
        AvpBuf row = {0};
        AvpCsvWriter w;
        avp_csvw_init(&w, &row, &d);
        avp_csvw_begin(&w);
        if (rend > p) {
            const char *f = p;
            for (;;) {
                const char *fend = f;
                while (fend < rend && *fend != SEP_FIELD) fend++;
                AvpStr v = avp_strn(f + 1, (size_t)(fend - f) - 1);
                switch (*f) {
                case 's': avp_csvw_str(&w, v); break;
                case 'n': avp_csvw_none(&w); break;
                case 'b': avp_csvw_bool(&w, v.p[0] == '1'); break;
                case 'i':
                    if (v.n <= 18) avp_csvw_i64(&w, strtoll(avp_cz(avp_scratch(), v), NULL, 10));
                    else avp_csvw_number_text(&w, v);
                    break;
                case 'f': avp_csvw_f64(&w, strtod(avp_cz(avp_scratch(), v), NULL)); break;
                default: AVP_ASSERT(false);
                }
                if (fend >= rend) break;
                f = fend + 1;
            }
        }
        bool ok = avp_csvw_end(&w);
        put_item_sep(out, &first);
        if (ok) {
            avp_buf_putc(out, 'W');
            avp_buf_put(out, avp_buf_str(&row));
        } else {
            avp_buf_putc(out, 'E');
            avp_buf_put(out, w.error);
        }
        avp_buf_free(&row);
        if (rend >= end) break;
        p = rend + 1;
    }
}

static void put_dict_record(AvpBuf *out, const AvpCsvDictReader *dr) {
    avp_buf_printf(out, "D%lld", (long long)dr->line_num);
    for (size_t i = 0; i < dr->names.n; i++) {
        AvpStr n = dr->names.v[i];
        bool seen = false;
        for (size_t j = 0; j < i; j++) seen = seen || avp_str_eq(dr->names.v[j], n);
        if (seen) continue; /* a repeated name keeps its first position */
        AvpStr v = avp_csv_dict_get(dr, n);
        avp_buf_putc(out, SEP_FIELD);
        avp_buf_putc(out, 'S');
        avp_buf_put(out, n);
        avp_buf_putc(out, SEP_KV);
        if (!v.p) avp_buf_putc(out, 'N');
        else {
            avp_buf_putc(out, 'S');
            avp_buf_put(out, v);
        }
    }
    if (dr->rest_present) {
        avp_buf_putc(out, SEP_FIELD);
        if (dr->restkey.p) {
            avp_buf_putc(out, 'S');
            avp_buf_put(out, dr->restkey);
        } else {
            avp_buf_putc(out, 'N');
        }
        avp_buf_putc(out, SEP_KV);
        avp_buf_putc(out, 'L');
        for (size_t i = 0; i < dr->rest.n; i++) {
            if (i) avp_buf_putc(out, SEP_LIST);
            avp_buf_put(out, dr->rest.v[i]);
        }
    }
}

static void run_dictreader(AvpBuf *out, const AvpCsvCase *c) {
    AvpCsvDialect d = dialect_for(c->dialect);
    AvpCsvReader rd;
    avp_csv_reader_init(&rd, &d, avp_strn(c->in, c->in_n), AVP_CSV_UNIVERSAL);
    AvpCsvDictReader dr;
    avp_csv_dictreader_init(&dr, avp_arena(), &rd);
    if (c->aux & 1) dr.restkey = AVP_S("R");
    if (c->aux & 2) dr.restval = AVP_S("V");
    bool first = true;
    int st;
    const AvpStrVec *names = avp_csv_dictreader_fieldnames(&dr, &st);
    put_item_sep(out, &first);
    if (names) {
        avp_buf_putc(out, 'F');
        for (size_t i = 0; i < names->n; i++) {
            if (i) avp_buf_putc(out, SEP_FIELD);
            avp_buf_put(out, names->v[i]);
        }
    } else {
        avp_buf_putz(out, "F-");
        if (st < 0) {
            put_item_sep(out, &first);
            avp_buf_printf(out, "E%lld%c", (long long)dr.rd.line_num, SEP_FIELD);
            avp_buf_put(out, dr.rd.error);
        }
    }
    for (;;) {
        st = avp_csv_dict_next(&dr);
        if (st == 0) break;
        put_item_sep(out, &first);
        if (st < 0) {
            avp_buf_printf(out, "E%lld%c", (long long)dr.rd.line_num, SEP_FIELD);
            avp_buf_put(out, dr.rd.error);
            continue;
        }
        put_dict_record(out, &dr);
    }
    avp_csv_dictreader_free(&dr);
}

static void run_dictwriter(AvpBuf *out, const AvpCsvCase *c) {
    AvpCsvDialect d = avp_csv_excel;
    d.delimiter = ';';
    d.lineterminator = AVP_S("\n");
    AvpCsvWriter w;
    avp_csvw_init(&w, out, &d);
    /* input: fieldnames (0x1F), then per row 0x1E 'D' key 0x1D value (0x1F ...) */
    const char *p = c->in, *end = c->in + c->in_n;
    AvpStrVec names = {0};
    const char *q = p;
    for (;;) {
        const char *f = q;
        while (q < end && *q != SEP_FIELD && *q != SEP_ITEM) q++;
        avp_vec_push(&names, avp_strn(f, (size_t)(q - f)));
        if (q >= end || *q == SEP_ITEM) break;
        q++;
    }
    CHECK(avp_csvw_row(&w, names.v, names.n));
    AvpStr restval = (c->aux & 1) ? AVP_S("V") : AVP_S("");
    AvpStr *vals = (AvpStr *)avp_xcalloc(names.n, sizeof(AvpStr));
    while (q < end) {
        q++; /* the 0x1E */
        q++; /* the 'D' */
        const char *rend = q;
        while (rend < end && *rend != SEP_ITEM) rend++;
        memset(vals, 0, names.n * sizeof(AvpStr));
        while (q < rend) {
            const char *kv = q;
            while (kv < rend && *kv != SEP_KV) kv++;
            AvpStr key = avp_strn(q, (size_t)(kv - q));
            const char *ve = kv + 1;
            while (ve < rend && *ve != SEP_FIELD) ve++;
            AvpStr val = avp_strn(kv + 1, (size_t)(ve - kv - 1));
            for (size_t i = 0; i < names.n; i++)
                if (avp_str_eq(names.v[i], key)) vals[i] = val;
            q = ve < rend ? ve + 1 : rend;
        }
        CHECK(avp_csvw_dict_row(&w, names.n, vals, restval));
        q = rend;
    }
    avp_xfree(vals);
    avp_vec_free(&names);
}

static void run_lines(AvpBuf *out, const AvpCsvCase *c) {
    AvpLines it;
    avp_lines_init(&it, avp_strn(c->in, c->in_n), (AvpLineMode)c->mode);
    AvpStr line;
    AvpStrVec f = {0};
    bool first = true;
    while (c->aux ? avp_lines_next_keep(&it, &line) : avp_lines_next(&it, &line)) {
        put_item_sep(out, &first);
        avp_buf_putc(out, 'L');
        avp_split_char(line, '\t', c->aux2, &f);
        for (size_t i = 0; i < f.n; i++) {
            if (i) avp_buf_putc(out, SEP_FIELD);
            avp_buf_put(out, f.v[i]);
        }
        if (c->aux2 < 0) { /* the two other spellings of line.split("\t") agree */
            AvpStrVec g = {0};
            avp_tsv_split(line, &g);
            AvpStr fixed[64];
            size_t n = avp_tsv_split_into(line, fixed, 64);
            CHECK_EQ_U64(g.n, f.n);
            CHECK_EQ_U64(n, f.n);
            for (size_t i = 0; i < g.n && i < 64; i++) CHECK(avp_str_eq(g.v[i], f.v[i]) && avp_str_eq(fixed[i], f.v[i]));
            avp_vec_free(&g);
        }
    }
    avp_vec_free(&f);
}

AVP_TEST(corpus_against_cpython) {
    int failures = 0;
    size_t per_kind[5] = {0};
    for (size_t i = 0; i < AVP_ARRAY_LEN(avp_csv_cases); i++) {
        const AvpCsvCase *c = &avp_csv_cases[i];
        AvpBuf got = {0};
        switch (c->kind) {
        case 0: run_reader(&got, c); break;
        case 1: run_writer(&got, c); break;
        case 2: run_dictreader(&got, c); break;
        case 3: run_dictwriter(&got, c); break;
        case 4: run_lines(&got, c); break;
        default: AVP_ASSERT(false);
        }
        per_kind[c->kind]++;
        if (!(got.n == c->exp_n && (got.n == 0 || memcmp(got.p, c->exp, c->exp_n) == 0))) {
            if (failures++ < 8)
                avp_test__fail(__FILE__, __LINE__, "case %zu (kind %d mode %d dialect %d aux %d,%d)\n   input    %.*s\n   expected %.*s\n   got      %.*s", i,
                               c->kind, c->mode, c->dialect, c->aux, c->aux2, (int)AVP_MIN(c->in_n, 160), c->in, (int)AVP_MIN(c->exp_n, 240), c->exp,
                               (int)AVP_MIN(got.n, 240), got.p ? got.p : "");
        }
        avp_buf_free(&got);
    }
    CHECK_EQ_I64(failures, 0);
    for (int k = 0; k < 5; k++) CHECK(per_kind[k] > 100);
}

/* ---- the limits ---- */
AVP_TEST(field_size_limit) {
    /* r = list(csv.reader([text])) for 131072 x's, 131073 x's, 66000 two-byte characters, and a quoted 140000-char field:
     * the limit (csv.field_size_limit() = 131072) counts characters */
    AvpBuf b = {0};
    AvpCsvReader rd;
    for (int i = 0; i < 131072; i++) avp_buf_putc(&b, 'x');
    avp_buf_putc(&b, '\n');
    avp_csv_reader_init(&rd, &avp_csv_excel, avp_buf_str(&b), AVP_CSV_RAW);
    CHECK_EQ_I64(avp_csv_next(&rd), 1);
    CHECK_EQ_U64(rd.row.v[0].n, 131072);
    avp_csv_reader_free(&rd);
    avp_buf_clear(&b);
    for (int i = 0; i < 131073; i++) avp_buf_putc(&b, 'x');
    avp_buf_putc(&b, '\n');
    avp_buf_putz(&b, "ok\n");
    avp_csv_reader_init(&rd, &avp_csv_excel, avp_buf_str(&b), AVP_CSV_RAW);
    CHECK_EQ_I64(avp_csv_next(&rd), -1);
    CHECK_STR(rd.error, "field larger than field limit (131072)");
    CHECK_EQ_I64(avp_csv_next(&rd), 1); /* the iterator goes on with the next line */
    CHECK_STR(rd.row.v[0], "ok");
    avp_csv_reader_free(&rd);
    avp_buf_clear(&b);
    for (int i = 0; i < 66000; i++) avp_buf_putz(&b, "\303\251");
    avp_buf_putz(&b, ",a\n");
    avp_csv_reader_init(&rd, &avp_csv_excel, avp_buf_str(&b), AVP_CSV_RAW);
    CHECK_EQ_I64(avp_csv_next(&rd), 1);
    CHECK_EQ_U64(rd.row.n, 2);
    CHECK_EQ_U64(rd.row.v[0].n, 132000);
    avp_csv_reader_free(&rd);
    avp_buf_clear(&b);
    avp_buf_putz(&b, "a,\"");
    for (int i = 0; i < 140000; i++) avp_buf_putc(&b, 'y');
    avp_buf_putz(&b, "\"\n");
    avp_csv_reader_init(&rd, &avp_csv_excel, avp_buf_str(&b), AVP_CSV_RAW);
    CHECK_EQ_I64(avp_csv_next(&rd), -1);
    CHECK_STR(rd.error, "field larger than field limit (131072)");
    avp_csv_reader_free(&rd);
    avp_buf_free(&b);
}

/* ---- the API around the corpus ---- */
AVP_TEST(views_and_kept_rows) {
    /* fields that are one run of the text are views; quoted/escaped ones are built; with a keep arena rows survive */
    AvpStr text = AVP_S("plain,\"quo\"\"ted\",x\n1,2\n");
    AvpCsvReader rd;
    avp_csv_reader_init(&rd, &avp_csv_excel, text, AVP_CSV_UNIVERSAL);
    CHECK_EQ_I64(avp_csv_next(&rd), 1);
    CHECK(rd.row.v[0].p == text.p);
    CHECK_STR(rd.row.v[1], "quo\"ted");
    CHECK(rd.row.v[1].p < text.p || rd.row.v[1].p >= text.p + text.n);
    AvpArena *a = avp_arena_new(0);
    avp_csv_reader_free(&rd);
    avp_csv_reader_init(&rd, &avp_csv_excel, text, AVP_CSV_UNIVERSAL);
    avp_csv_reader_keep(&rd, a);
    CHECK_EQ_I64(avp_csv_next(&rd), 1);
    AvpStr kept = rd.row.v[1];
    CHECK_EQ_I64(avp_csv_next(&rd), 1);
    CHECK_STR(kept, "quo\"ted");
    CHECK_EQ_I64(rd.line_num, 2);
    CHECK_EQ_I64(avp_csv_next(&rd), 0);
    avp_csv_reader_free(&rd);
    avp_arena_free(a);
}

AVP_TEST(dictreader_api) {
    /* csv.DictReader(io.StringIO(text), delimiter="\t") with a row that is short, one that is long, and blank lines */
    AvpStr text = AVP_S("unit\tfunction\tstatus\nu1\tf1\tMATCH\n\nu2\tf2\n\nu3\tf3\tX\textra1\textra2\n");
    AvpCsvReader rd;
    avp_csv_reader_init(&rd, &avp_csv_excel_tab, text, AVP_CSV_UNIVERSAL);
    AvpCsvDictReader dr;
    avp_csv_dictreader_init(&dr, NULL, &rd);
    int st;
    const AvpStrVec *names = avp_csv_dictreader_fieldnames(&dr, &st);
    REQUIRE(names != NULL);
    CHECK_EQ_U64(names->n, 3);
    CHECK_EQ_I64(avp_csv_dict_next(&dr), 1);
    CHECK_STR(avp_csv_dict_getz(&dr, "unit"), "u1");
    CHECK_STR(avp_csv_dict_getz(&dr, "status"), "MATCH");
    CHECK(avp_csv_dict_getz(&dr, "nope").p == NULL);
    CHECK(avp_csv_dict_has(&dr, AVP_S("unit")) && !avp_csv_dict_has(&dr, AVP_S("nope")));
    CHECK_EQ_I64(avp_csv_dict_next(&dr), 1);
    CHECK_STR(avp_csv_dict_getz(&dr, "function"), "f2");
    CHECK(avp_csv_dict_getz(&dr, "status").p == NULL); /* restval=None */
    dr.restval = AVP_S("");
    CHECK_EQ_I64(avp_csv_dict_next(&dr), 1);
    CHECK_STR(avp_csv_dict_getz(&dr, "status"), "X");
    CHECK(dr.rest_present);
    CHECK_EQ_U64(dr.rest.n, 2);
    CHECK_STR(dr.rest.v[1], "extra2");
    CHECK_EQ_I64(avp_csv_dict_next(&dr), 0);
    avp_csv_dictreader_free(&dr);
}

AVP_TEST(writer_api) {
    /* csv.DictWriter(f, fieldnames=COLUMNS, delimiter="\t", lineterminator="\n"): writeheader(), writerows(rows)
     * with a value that needs quoting, and csv.writer(f) rows with numbers */
    AvpBuf b = {0};
    AvpCsvDialect d = avp_csv_excel_tab;
    d.lineterminator = AVP_S("\n");
    AvpCsvWriter w;
    avp_csvw_init(&w, &b, &d);
    AvpStr hdr[3] = {AVP_S("unit"), AVP_S("function"), AVP_S("note")};
    CHECK(avp_csvw_row(&w, hdr, 3));
    AvpStr vals[3] = {AVP_S("u1"), {NULL, 0}, AVP_S("has \"quote\"\tand tab")};
    CHECK(avp_csvw_dict_row(&w, 3, vals, AVP_S("")));
    CHECK_STR(avp_buf_str(&b), "unit\tfunction\tnote\nu1\t\t\"has \"\"quote\"\"\tand tab\"\n");
    avp_buf_free(&b);
    avp_csvw_init(&w, &b, &avp_csv_excel);
    avp_csvw_begin(&w);
    avp_csvw_str(&w, AVP_S("a,b"));
    avp_csvw_i64(&w, -12);
    avp_csvw_f64(&w, 0.1);
    avp_csvw_f64(&w, 1e22);
    avp_csvw_none(&w);
    avp_csvw_bool(&w, true);
    CHECK(avp_csvw_end(&w));
    /* the single empty field row is "" and a failing row leaves the buffer unchanged */
    CHECK(avp_csvw_row(&w, &AVP_S(""), 1));
    CHECK_STR(avp_buf_str(&b), "\"a,b\",-12,0.1,1e+22,,True\r\n\"\"\r\n");
    AvpCsvDialect none = avp_csv_excel;
    none.quoting = AVP_CSV_NONE;
    AvpBuf b2 = {0};
    AvpCsvWriter w2;
    avp_csvw_init(&w2, &b2, &none);
    AvpStr bad[1] = {AVP_S("a,b")};
    CHECK(!avp_csvw_row(&w2, bad, 1));
    CHECK_STR(w2.error, "need to escape, but no escapechar set");
    CHECK_EQ_U64(b2.n, 0);
    AvpStr empty[1] = {AVP_S("")};
    CHECK(!avp_csvw_row(&w2, empty, 1));
    CHECK_STR(w2.error, "single empty field record must be quoted");
    avp_buf_free(&b2);
    avp_buf_free(&b);
}

AVP_TEST(bom_and_lines) {
    CHECK_STR(avp_csv_strip_bom(AVP_S("\357\273\277a,b")), "a,b");
    CHECK_STR(avp_csv_strip_bom(AVP_S("a,b")), "a,b");
    /* "a\tb\t\tc".split("\t"), "".split("\t") and the maxsplit forms */
    AvpStrVec v = {0};
    CHECK_EQ_U64(avp_tsv_split(AVP_S("a\tb\t\tc"), &v), 4);
    CHECK_STR(v.v[2], "");
    CHECK_EQ_U64(avp_tsv_split(AVP_S(""), &v), 1);
    CHECK_EQ_U64(avp_tsv_split(AVP_S("\t"), &v), 2);
    CHECK_EQ_U64(avp_split_char(AVP_S("a\tb\tc"), '\t', 1, &v), 2);
    CHECK_STR(v.v[1], "b\tc");
    avp_vec_free(&v);
    /* a mapped ledger is walked without copies: the lines are views of the text */
    AvpStr text = AVP_S("h1\th2\r\nx\ty\r\n\r\nlast");
    AvpLines it;
    AvpStr line;
    avp_lines_init(&it, text, AVP_LINES_UNIVERSAL);
    CHECK(avp_lines_next(&it, &line) && line.p == text.p);
    CHECK_STR(line, "h1\th2");
    CHECK(avp_lines_next(&it, &line));
    CHECK(avp_lines_next(&it, &line));
    CHECK_STR(line, "");
    CHECK(avp_lines_next(&it, &line));
    CHECK_STR(line, "last");
    CHECK(!avp_lines_next(&it, &line));
}

AVP_TEST_MAIN("csv")
