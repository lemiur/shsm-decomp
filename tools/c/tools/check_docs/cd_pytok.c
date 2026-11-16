/* check_docs: the subset of Python's tokenizer that check_docs.tab_indented_line needs.
 *
 * tab_indented_line(data) ran tokenize.tokenize over the file's bytes and returned the line of the first INDENT token
 * whose text holds a tab, or None when there was none or the tokenizer raised first (TokenError, SyntaxError,
 * TabError, IndentationError, UnicodeDecodeError). CPython 3.14's tokenize module is its C tokenizer
 * (Parser/lexer/lexer.c) in "extra tokens" mode, reading the file one line at a time. This file follows that code for
 * everything that decides an INDENT or an error before one:
 *   - encoding: detect_encoding (BOM, a coding cookie on line 1, or on line 2 after a blank or comment line); each
 *     line is decoded when the tokenizer first needs it, so an undecodable line (or one holding a NUL byte) is an
 *     error only once it is reached;
 *   - indentation: col with tabs of 8, altcol with tabs of 1, form feed resets both, a backslash continuation inside
 *     the indentation (the first one fixes the column), blank and comment-only lines ignored, nothing inside brackets;
 *     the TabError and "unindent does not match" checks; the INDENT text is the current physical line's leading
 *     whitespace;
 *   - tokens that can fail: strings with every prefix (incompatible prefixes are an error), single- and
 *     triple-quoted, backslash escapes and continuations; f- and t-strings with nested replacement fields, format
 *     specs and reused quotes (PEP 701), single '}' errors; numbers (hex, octal, binary, underscores, exponents);
 *     a backslash not followed by a newline; ASCII control characters outside strings and comments.
 * What is not reproduced is in DEVIATIONS.md (D-check_docs-4). */
#include "check_docs.h"

#define MAXINDENT 100

typedef struct {
    const u8 *p;
    size_t n, i;
    i64 line;      /* line of position i */
    size_t line_start;
    size_t read_upto;  /* end of the lines read so far */
    i64 lines_read;
    int enc;           /* 0 utf-8, 1 latin-1 (every byte decodes), 2 ascii */
    bool err;
    i64 found;
    int level;         /* bracket depth of the plain code */
    int indent;
    int indstack[MAXINDENT], altstack[MAXINDENT];
} Tok;

/* ---- reading lines --------------------------------------------------------------------------------------------- */
static size_t utf8_seq_ok(const u8 *p, size_t n) {
    u8 c = p[0];
    if (c < 0x80) return 1;
    size_t len;
    u8 lo = 0x80, hi = 0xBF;
    if (c >= 0xC2 && c <= 0xDF) len = 2;
    else if (c == 0xE0) len = 3, lo = 0xA0;
    else if (c == 0xED) len = 3, hi = 0x9F;
    else if (c >= 0xE1 && c <= 0xEF) len = 3;
    else if (c == 0xF0) len = 4, lo = 0x90;
    else if (c == 0xF4) len = 4, hi = 0x8F;
    else if (c >= 0xF1 && c <= 0xF3) len = 4;
    else return 0;
    if (n < len) return 0;
    if (p[1] < lo || p[1] > hi) return 0;
    for (size_t k = 2; k < len; k++)
        if (p[k] < 0x80 || p[k] > 0xBF) return 0;
    return len;
}

static bool line_decodes(const Tok *t, size_t a, size_t b) {
    for (size_t k = a; k < b;) {
        if (t->p[k] == 0) return false;
        if (t->enc == 1) {
            k++;
        } else if (t->enc == 2) {
            if (t->p[k] >= 0x80) return false;
            k++;
        } else {
            size_t len = utf8_seq_ok(t->p + k, b - k);
            if (!len) return false;
            k += len;
        }
    }
    return true;
}

/* Reads lines until position j is covered (the tokenizer's tok_nextc at the end of its buffer). */
static void ensure(Tok *t, size_t j) {
    while (!t->err && t->read_upto <= j && t->read_upto < t->n) {
        size_t a = t->read_upto, b = a;
        while (b < t->n && t->p[b] != '\n') b++;
        if (b < t->n) b++;
        if (!line_decodes(t, a, b)) t->err = true;
        t->read_upto = b;
        t->lines_read++;
    }
}

/* the byte at i + off, or -1 at the end of the file or on an error */
static int peek(Tok *t, size_t off) {
    size_t j = t->i + off;
    if (j >= t->n) return -1;
    ensure(t, j);
    return t->err ? -1 : t->p[j];
}

static void adv(Tok *t) {
    if (t->i >= t->n) return;
    if (t->p[t->i] == '\n') {
        t->line++;
        t->line_start = t->i + 1;
    }
    t->i++;
}

static bool fail(Tok *t) {
    t->err = true;
    return false;
}

/* ---- tokens ---------------------------------------------------------------------------------------------------- */
static bool is_digit(int c) { return c >= '0' && c <= '9'; }
static bool is_ident_start(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0x80; }
static bool is_ident(int c) { return is_ident_start(c) || is_digit(c); }
static bool is_hex(int c) { return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

static bool scan_atom(Tok *t);
static bool scan_fexpr(Tok *t, int q, bool triple, bool raw);

/* "\n" or "\r\n" at the current position: consumes it */
static bool take_newline(Tok *t) {
    if (peek(t, 0) == '\r' && peek(t, 1) == '\n') adv(t);
    if (peek(t, 0) == '\n') {
        adv(t);
        return true;
    }
    return false;
}

/* a backslash at the current position: a line continuation (tok_continuation_line) */
static bool continuation(Tok *t) {
    adv(t);
    if (!take_newline(t)) return fail(t);
    if (peek(t, 0) < 0) return fail(t);
    return true;
}

static bool closing(Tok *t, int q, bool triple) {
    if (peek(t, 0) != q) return false;
    if (!triple) return true;
    return peek(t, 1) == q && peek(t, 2) == q;
}

static void take_closing(Tok *t, bool triple) {
    adv(t);
    if (triple) {
        adv(t);
        adv(t);
    }
}

/* the body of a plain string; the opening quote(s) consumed */
static bool scan_string(Tok *t, int q, bool triple) {
    for (;;) {
        int c = peek(t, 0);
        if (c < 0) return fail(t);
        if (closing(t, q, triple)) {
            take_closing(t, triple);
            return true;
        }
        if (c == '\n' && !triple) return fail(t);
        if (c == '\\') {
            adv(t);
            if (peek(t, 0) < 0) return fail(t);
        }
        adv(t);
    }
}

/* a format spec after ':'; ends with the '}' that closes the replacement field */
static bool scan_fspec(Tok *t, int q, bool triple, bool raw) {
    for (;;) {
        int c = peek(t, 0);
        if (c < 0) return fail(t);
        if (closing(t, q, triple)) return fail(t);
        if (c == '\n' && !triple) return fail(t);
        if (c == '{') {
            adv(t);
            if (!scan_fexpr(t, q, triple, raw)) return false;
            continue;
        }
        if (c == '}') {
            adv(t);
            return true;
        }
        if (c == '\\' && !raw) {
            adv(t);
            if (peek(t, 0) < 0) return fail(t);
            if (peek(t, 0) == '{' || peek(t, 0) == '}') continue;
        }
        adv(t);
    }
}

/* a replacement field's expression after '{' */
static bool scan_fexpr(Tok *t, int q, bool triple, bool raw) {
    int depth = 0;
    for (;;) {
        int c = peek(t, 0);
        if (c < 0) return fail(t);
        if (c == ' ' || c == '\t' || c == '\f' || c == '\r' || c == '\n') {
            adv(t);
        } else if (c == '#') {
            while (peek(t, 0) >= 0 && peek(t, 0) != '\n') adv(t);
        } else if (c == '\\') {
            if (!continuation(t)) return false;
        } else if (depth == 0 && c == '}') {
            adv(t);
            return true;
        } else if (depth == 0 && c == ':') {
            adv(t);
            return scan_fspec(t, q, triple, raw);
        } else if (c == '(' || c == '[' || c == '{') {
            depth++;
            adv(t);
        } else if (c == ')' || c == ']' || c == '}') {
            if (depth > 0) depth--;
            adv(t);
        } else if (!scan_atom(t)) {
            return false;
        }
    }
}

/* the body of an f- or t-string; the opening quote(s) consumed */
static bool scan_fstring(Tok *t, int q, bool triple, bool raw) {
    for (;;) {
        int c = peek(t, 0);
        if (c < 0) return fail(t);
        if (closing(t, q, triple)) {
            take_closing(t, triple);
            return true;
        }
        if (c == '\n' && !triple) return fail(t);
        if (c == '{') {
            if (peek(t, 1) == '{') {
                adv(t);
                adv(t);
                continue;
            }
            adv(t);
            if (!scan_fexpr(t, q, triple, raw)) return false;
            continue;
        }
        if (c == '}') {
            if (peek(t, 1) == '}') {
                adv(t);
                adv(t);
                continue;
            }
            return fail(t);
        }
        if (c == '\\') {
            int d = peek(t, 1);
            if (d < 0) return fail(t);
            if (!raw && d == 'N' && peek(t, 2) == '{') {
                adv(t);
                adv(t);
                adv(t);
                while (peek(t, 0) >= 0 && peek(t, 0) != '}' && peek(t, 0) != '\n') adv(t);
                if (peek(t, 0) != '}') return fail(t);
                adv(t);
                continue;
            }
            if (d == '{' || d == '}') {
                adv(t);
                continue;
            }
            adv(t);
        }
        adv(t);
    }
}

/* the opening quote of a string at the current position */
static bool scan_quoted(Tok *t, bool fstr, bool raw) {
    int q = peek(t, 0);
    bool triple = peek(t, 1) == q && peek(t, 2) == q;
    take_closing(t, triple);
    return fstr ? scan_fstring(t, q, triple, raw) : scan_string(t, q, triple);
}

/* digits with single underscores between them (tok_decimal_tail); the current byte is a digit */
static bool decimal_tail(Tok *t) {
    for (;;) {
        while (is_digit(peek(t, 0))) adv(t);
        if (peek(t, 0) != '_') return true;
        adv(t);
        if (!is_digit(peek(t, 0))) return fail(t);
    }
}

static bool radix_digits(Tok *t, int radix) {
    int c = peek(t, 0);
    do {
        if (c == '_') {
            adv(t);
            c = peek(t, 0);
        }
        bool ok = radix == 16 ? is_hex(c) : (c >= '0' && c < '0' + radix);
        if (!ok) return fail(t);
        do {
            adv(t);
            c = peek(t, 0);
        } while (radix == 16 ? is_hex(c) : (c >= '0' && c < '0' + radix));
    } while (c == '_');
    if (radix != 16 && is_digit(c)) return fail(t); /* invalid digit in an octal or binary literal */
    return true;
}

static bool number_end(Tok *t, bool fraction_now);

static bool scan_number(Tok *t) {
    int c = peek(t, 0);
    if (c == '.') {
        adv(t);
        return number_end(t, true);
    }
    if (c == '0') {
        adv(t);
        c = peek(t, 0);
        if (c == 'x' || c == 'X') {
            adv(t);
            return radix_digits(t, 16);
        }
        if (c == 'o' || c == 'O') {
            adv(t);
            return radix_digits(t, 8);
        }
        if (c == 'b' || c == 'B') {
            adv(t);
            return radix_digits(t, 2);
        }
        for (;;) {
            if (c == '_') {
                adv(t);
                c = peek(t, 0);
                if (!is_digit(c)) return fail(t);
            }
            if (c != '0') break;
            adv(t);
            c = peek(t, 0);
        }
        if (is_digit(c) && !decimal_tail(t)) return false;
        return number_end(t, false);
    }
    if (!decimal_tail(t)) return false;
    return number_end(t, false);
}

/* after the integer part: '.', fraction, exponent, imaginary suffix */
static bool number_end(Tok *t, bool fraction_now) {
    int c = peek(t, 0);
    if (!fraction_now && c == '.') {
        adv(t);
        fraction_now = true;
    }
    if (fraction_now) {
        if (is_digit(peek(t, 0)) && !decimal_tail(t)) return false;
    }
    c = peek(t, 0);
    if (c == 'e' || c == 'E') {
        size_t at = t->i;
        adv(t);
        c = peek(t, 0);
        if (c == '+' || c == '-') {
            adv(t);
            if (!is_digit(peek(t, 0))) return fail(t);
        } else if (!is_digit(c)) {
            t->i = at; /* "1e": the number ends before the e */
            return true;
        }
        if (!decimal_tail(t)) return false;
        c = peek(t, 0);
    }
    if (c == 'j' || c == 'J') adv(t);
    return true;
}

/* an identifier, or a string with its prefix */
static bool scan_name(Tok *t) {
    bool b = false, r = false, u = false, f = false, tt = false;
    for (;;) {
        int c = peek(t, 0);
        if (!b && (c == 'b' || c == 'B')) b = true;
        else if (!u && (c == 'u' || c == 'U')) u = true;
        else if (!r && (c == 'r' || c == 'R')) r = true;
        else if (!f && (c == 'f' || c == 'F')) f = true;
        else if (!tt && (c == 't' || c == 'T')) tt = true;
        else break;
        adv(t);
        c = peek(t, 0);
        if (c == '"' || c == '\'') {
            if (u && (b || r || f || tt)) return fail(t);
            if (b && (f || tt)) return fail(t);
            if (f && tt) return fail(t);
            return scan_quoted(t, f || tt, r);
        }
    }
    while (is_ident(peek(t, 0))) adv(t);
    return true;
}

/* one token that is not whitespace, a newline, a comment, a continuation or a bracket */
static bool scan_atom(Tok *t) {
    int c = peek(t, 0);
    if (c < 0) return !t->err;
    if (c == '"' || c == '\'') return scan_quoted(t, false, false);
    if (is_ident_start(c)) return scan_name(t);
    if (is_digit(c) || (c == '.' && is_digit(peek(t, 1)))) return scan_number(t);
    if ((c < 0x20 && c != '\t' && c != '\n' && c != '\f' && c != '\r') || c == 0x7F) return fail(t);
    adv(t);
    return true;
}

/* ---- lines and indentation ------------------------------------------------------------------------------------- */
/* At the start of a line: the indentation, then INDENT / DEDENT bookkeeping. False to stop (an error, or the INDENT
 * with a tab was found). */
static bool line_start(Tok *t) {
    int col = 0, altcol = 0, cont_col = 0;
    for (;;) {
        int c = peek(t, 0);
        if (c == ' ') {
            col++, altcol++;
        } else if (c == '\t') {
            col = (col / 8 + 1) * 8;
            altcol = altcol + 1;
        } else if (c == '\f') {
            col = altcol = 0;
        } else if (c == '\\') {
            cont_col = cont_col ? cont_col : col;
            if (!continuation(t)) return false;
            continue;
        } else {
            break;
        }
        adv(t);
    }
    if (t->err) return false;
    int c = peek(t, 0);
    if (t->err) return false;
    bool blank = c == '#' || c == '\n' || c == '\r' || c < 0;
    if (blank || t->level != 0) return true;
    if (cont_col) col = altcol = cont_col;
    if (col == t->indstack[t->indent]) {
        if (altcol != t->altstack[t->indent]) return fail(t);
    } else if (col > t->indstack[t->indent]) {
        if (t->indent + 1 >= MAXINDENT) return fail(t);
        if (altcol <= t->altstack[t->indent]) return fail(t);
        t->indent++;
        t->indstack[t->indent] = col;
        t->altstack[t->indent] = altcol;
        if (memchr(t->p + t->line_start, '\t', t->i - t->line_start)) {
            t->found = t->line;
            return false;
        }
    } else {
        while (t->indent > 0 && col < t->indstack[t->indent]) t->indent--;
        if (col != t->indstack[t->indent]) return fail(t);
        if (altcol != t->altstack[t->indent]) return fail(t);
    }
    return true;
}

static void scan_file(Tok *t) {
    bool atbol = true;
    for (;;) {
        if (atbol) {
            atbol = false;
            if (!line_start(t)) return;
        }
        int c = peek(t, 0);
        if (c < 0) return;
        if (c == ' ' || c == '\t' || c == '\f' || c == '\r') {
            adv(t);
        } else if (c == '\n') {
            adv(t);
            atbol = true;
        } else if (c == '#') {
            while (peek(t, 0) >= 0 && peek(t, 0) != '\n') adv(t);
        } else if (c == '\\') {
            if (!continuation(t)) return;
        } else if (c == '(' || c == '[' || c == '{') {
            t->level++;
            adv(t);
        } else if (c == ')' || c == ']' || c == '}') {
            if (t->level > 0) t->level--;
            adv(t);
        } else if (!scan_atom(t)) {
            return;
        }
        if (t->err) return;
    }
}

/* ---- detect_encoding ------------------------------------------------------------------------------------------- */
static size_t line_end(const u8 *p, size_t n, size_t a) {
    size_t b = a;
    while (b < n && p[b] != '\n') b++;
    return b < n ? b + 1 : b;
}

/* cookie_re = ^[ \t\f]*#.*?coding[:=][ \t]*([-\w.]+) (ASCII): the name, or an empty view */
static AvpStr find_cookie_name(const u8 *p, size_t a, size_t b) {
    size_t k = a;
    while (k < b && (p[k] == ' ' || p[k] == '\t' || p[k] == '\f')) k++;
    if (k >= b || p[k] != '#') return avp_strn(NULL, 0);
    for (size_t s = k + 1; s + 7 <= b; s++) {
        if (p[s] == '\n' || p[s] == '\r') break;
        if (memcmp(p + s, "coding", 6) != 0 || (p[s + 6] != ':' && p[s + 6] != '=')) continue;
        size_t e = s + 7;
        while (e < b && (p[e] == ' ' || p[e] == '\t')) e++;
        size_t st = e;
        while (e < b && (p[e] == '-' || p[e] == '.' || p[e] == '_' || (p[e] >= '0' && p[e] <= '9') ||
                         ((p[e] | 0x20) >= 'a' && (p[e] | 0x20) <= 'z')))
            e++;
        if (e > st) return avp_strn((const char *)p + st, e - st);
    }
    return avp_strn(NULL, 0);
}

static bool blank_line(const u8 *p, size_t a, size_t b) {
    size_t k = a;
    while (k < b && (p[k] == ' ' || p[k] == '\t' || p[k] == '\f')) k++;
    return k >= b || p[k] == '#' || p[k] == '\r' || p[k] == '\n';
}

/* -1: SyntaxError (undecodable cookie line, unknown or BOM-conflicting encoding); else the Tok.enc value */
static int classify_encoding(AvpStr name, bool bom) {
    char low[16] = {0};
    size_t n = name.n < 12 ? name.n : 12;
    for (size_t k = 0; k < n; k++) {
        char c = name.p[k];
        low[k] = c == '_' ? '-' : (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
    }
    int enc = -1;
    if (!strcmp(low, "utf-8") || !strncmp(low, "utf-8-", 6) || !strcmp(low, "utf8") || !strcmp(low, "u8") ||
        !strcmp(low, "utf"))
        enc = 0;
    else if (!strcmp(low, "latin-1") || !strcmp(low, "iso-8859-1") || !strcmp(low, "iso-latin-1") ||
             !strncmp(low, "latin-1-", 8) || !strncmp(low, "iso-8859-1-", 11) || !strncmp(low, "iso-latin-1-", 12) ||
             !strcmp(low, "latin1") || !strcmp(low, "l1") || !strcmp(low, "iso8859-1") || !strcmp(low, "cp819"))
        enc = 1;
    else if (!strcmp(low, "ascii") || !strcmp(low, "us-ascii") || !strcmp(low, "646"))
        enc = 2;
    else
        enc = 0; /* another codec: decoded as UTF-8 here (D-check_docs-4) */
    if (bom && enc != 0) return -1;
    return enc;
}

i64 cd_tab_indented_line(AvpStr data) {
    Tok t;
    memset(&t, 0, sizeof t);
    t.p = (const u8 *)data.p;
    t.n = data.n;
    t.line = 1;
    bool bom = t.n >= 3 && memcmp(t.p, "\xEF\xBB\xBF", 3) == 0;
    if (bom) t.i = t.line_start = t.read_upto = 3;
    /* detect_encoding: the first line, and the second when the first is blank or a comment */
    size_t a1 = t.i, b1 = line_end(t.p, t.n, a1);
    if (a1 < t.n) {
        Tok probe = t;
        probe.enc = 0;
        if (!line_decodes(&probe, a1, b1) && memchr(t.p + a1, 0, b1 - a1) == NULL) return 0;
        AvpStr name = find_cookie_name(t.p, a1, b1);
        if (!name.n && blank_line(t.p, a1, b1) && b1 < t.n) {
            size_t b2 = line_end(t.p, t.n, b1);
            if (!line_decodes(&probe, b1, b2) && memchr(t.p + b1, 0, b2 - b1) == NULL) return 0;
            name = find_cookie_name(t.p, b1, b2);
        }
        if (name.n) {
            int enc = classify_encoding(name, bom);
            if (enc < 0) return 0;
            t.enc = enc;
        }
    }
    scan_file(&t);
    return t.found;
}
