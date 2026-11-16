/* avpcore csv: lines of a mapped file and the line.split("\t") fast path of the project's ledgers. */
#include "avp/csv.h"

void avp_lines_init(AvpLines *it, AvpStr text, AvpLineMode mode) {
    it->p = text.p;
    it->end = text.p + text.n;
    it->mode = (int)mode;
    it->done = false;
}

/* The end of the line starting at p: *term_len is the length of its ending (0 at the end of the text). */
static const char *find_end(const AvpLines *it, const char *p, size_t *term_len) {
    const char *end = it->end;
    switch (it->mode) {
    case AVP_LINES_LF:
    case AVP_LINES_SPLIT_LF: {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        if (!nl) {
            *term_len = 0;
            return end;
        }
        *term_len = 1;
        return nl;
    }
    case AVP_LINES_UNIVERSAL: {
        for (; p < end; p++) {
            if (*p == '\n') {
                *term_len = 1;
                return p;
            }
            if (*p == '\r') {
                *term_len = (p + 1 < end && p[1] == '\n') ? 2 : 1;
                return p;
            }
        }
        *term_len = 0;
        return end;
    }
    default: { /* str.splitlines(): \n \r \r\n \v \f \x1c \x1d \x1e U+0085 U+2028 U+2029 */
        for (; p < end; p++) {
            unsigned char c = (unsigned char)*p;
            if (c == '\n' || c == 0x0B || c == 0x0C || (c >= 0x1C && c <= 0x1E)) {
                *term_len = 1;
                return p;
            }
            if (c == '\r') {
                *term_len = (p + 1 < end && p[1] == '\n') ? 2 : 1;
                return p;
            }
            if (c == 0xC2 && p + 1 < end && (unsigned char)p[1] == 0x85) {
                *term_len = 2;
                return p;
            }
            if (c == 0xE2 && p + 2 < end && (unsigned char)p[1] == 0x80 && ((unsigned char)p[2] == 0xA8 || (unsigned char)p[2] == 0xA9)) {
                *term_len = 3;
                return p;
            }
        }
        *term_len = 0;
        return end;
    }
    }
}

static bool next(AvpLines *it, AvpStr *line, bool keep) {
    if (it->done) return false;
    if (it->mode == AVP_LINES_SPLIT_LF) {
        size_t t;
        const char *e = find_end(it, it->p, &t);
        *line = avp_strn(it->p, (size_t)(e - it->p) + (keep ? t : 0));
        if (t == 0) it->done = true;
        else it->p = e + t;
        return true;
    }
    if (it->p >= it->end) {
        it->done = true;
        return false;
    }
    size_t t;
    const char *e = find_end(it, it->p, &t);
    *line = avp_strn(it->p, (size_t)(e - it->p) + (keep ? t : 0));
    it->p = e + t;
    return true;
}

bool avp_lines_next(AvpLines *it, AvpStr *line) { return next(it, line, false); }
bool avp_lines_next_keep(AvpLines *it, AvpStr *line) { return next(it, line, true); }

size_t avp_split_char(AvpStr line, char sep, int maxsplit, AvpStrVec *out) {
    avp_vec_clear(out);
    const char *p = line.p, *end = line.p + line.n;
    int splits = 0;
    for (;;) {
        const char *q = (maxsplit >= 0 && splits >= maxsplit) ? NULL : (const char *)memchr(p, sep, (size_t)(end - p));
        if (!q) {
            avp_vec_push(out, avp_strn(p, (size_t)(end - p)));
            break;
        }
        avp_vec_push(out, avp_strn(p, (size_t)(q - p)));
        p = q + 1;
        splits++;
    }
    return out->n;
}

size_t avp_tsv_split(AvpStr line, AvpStrVec *out) { return avp_split_char(line, '\t', -1, out); }

size_t avp_tsv_split_into(AvpStr line, AvpStr *out, size_t cap) {
    const char *p = line.p, *end = line.p + line.n;
    size_t n = 0;
    for (;;) {
        const char *q = end > p ? (const char *)memchr(p, '\t', (size_t)(end - p)) : NULL;
        const char *e = q ? q : end;
        if (n < cap) out[n] = avp_strn(p, (size_t)(e - p));
        n++;
        if (!q) break;
        p = q + 1;
    }
    return n;
}
