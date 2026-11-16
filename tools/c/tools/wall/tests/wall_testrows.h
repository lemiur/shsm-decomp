/* Test helper for the wall tests: the fn() helper of tools/test_wall.py (L23-30). One instruction per line; ".L_<n>"
 * names the row index n (rewritten to ".L_%08X" of BASE + 4n), each row parsed at BASE + 4i with cc_parse_row and
 * the rows made a CcFn (FnCode(name, rows): local branches resolved, bidx set). Header-only; every allocation is in
 * the arena given. */
#pragma once
#include "avp/avp.h"
#include "cc_listing.h"

#define WALL_TEST_BASE 0x80000000LL

static inline bool wall_test__word(u8 c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0x80;
}

/* re.sub(r"\.L_(\d+)\b", ".L_%08X" % (BASE + 4 * int(n)), line) */
static inline AvpStr wall_test__labels(AvpArena *a, AvpStr line) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putn(&b, "", 0);
    size_t i = 0;
    while (i < line.n) {
        if (i + 3 <= line.n && memcmp(line.p + i, ".L_", 3) == 0) {
            size_t j = i + 3;
            u64 v = 0;
            while (j < line.n && line.p[j] >= '0' && line.p[j] <= '9')
                v = v * 10 + (u64)(line.p[j++] - '0');
            if (j > i + 3 && !(j < line.n && wall_test__word((u8)line.p[j]))) {
                avp_buf_printf(&b, ".L_%08llX", (unsigned long long)(WALL_TEST_BASE + 4 * (i64)v));
                i = j;
                continue;
            }
        }
        avp_buf_putc(&b, line.p[i]);
        i++;
    }
    return avp_strn(avp_buf_cz(&b), b.n);
}

/* fn(text, name): text.strip().splitlines(), each line stripped (empty inner lines kept as rows, as the Python). */
static inline CcFn *wall_test_fn_named(AvpArena *a, const char *text, const char *name) {
    AvpStr t = avp_str_strip(avp_str(text));
    AVP_VEC(CcRow) rows = {0};
    size_t i = 0;
    while (t.n && i <= t.n) {
        size_t j = i;
        while (j < t.n && t.p[j] != '\n' && t.p[j] != '\r')
            j++;
        AvpStr line = avp_str_strip(avp_strn(t.p + i, j - i));
        line = wall_test__labels(a, line);
        CcRow r = cc_parse_row(a, WALL_TEST_BASE + 4 * (i64)rows.n, line);
        avp_vec_push(&rows, r);
        if (j >= t.n)
            break;
        i = j + ((t.p[j] == '\r' && j + 1 < t.n && t.p[j + 1] == '\n') ? 2 : 1);
    }
    CcRow *arr = AVP_NEWN(a, CcRow, rows.n);
    if (rows.n)
        memcpy(arr, rows.v, rows.n * sizeof *arr);
    size_t n = rows.n;
    avp_vec_free(&rows);
    return cc_fn_new(a, avp_str(name), arr, n, NULL, false, 0, false, 0);
}

static inline CcFn *wall_test_fn(AvpArena *a, const char *text) { return wall_test_fn_named(a, text, "f"); }
