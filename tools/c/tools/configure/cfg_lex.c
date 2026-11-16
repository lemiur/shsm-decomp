/* configure.exe: Python's tokenizer for the configure.py subset (cfg.h). */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "cfg.h"

static const char *g_script = "configure.py";

void cfg_set_script_name(const char *name) { g_script = name; }

void cfg_fail(int line, const char *fmt, ...) {
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    fflush(stdout);
    fprintf(stderr, "configure: error: %s:%d: %s\n", g_script, line, msg);
    avp_exit(2);
}

static bool is_ident_start(unsigned char c) { return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0x80; }
static bool is_ident(unsigned char c) { return is_ident_start(c) || (c >= '0' && c <= '9'); }

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

AvpStr cfg_unescape(AvpArena *a, AvpStr s, int line) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < s.n; i++) {
        char c = s.p[i];
        if (c == '\n') line++;
        if (c != '\\' || i + 1 >= s.n) {
            avp_buf_putc(&b, c);
            continue;
        }
        char e = s.p[++i];
        switch (e) {
        case '\n': line++; break;
        case '\\': avp_buf_putc(&b, '\\'); break;
        case '\'': avp_buf_putc(&b, '\''); break;
        case '"': avp_buf_putc(&b, '"'); break;
        case 'a': avp_buf_putc(&b, '\a'); break;
        case 'b': avp_buf_putc(&b, '\b'); break;
        case 'f': avp_buf_putc(&b, '\f'); break;
        case 'n': avp_buf_putc(&b, '\n'); break;
        case 'r': avp_buf_putc(&b, '\r'); break;
        case 't': avp_buf_putc(&b, '\t'); break;
        case 'v': avp_buf_putc(&b, '\v'); break;
        case 'x':
        case 'u':
        case 'U': {
            int digits = e == 'x' ? 2 : e == 'u' ? 4 : 8;
            uint32_t cp = 0;
            for (int k = 0; k < digits; k++) {
                int h = i + 1 + (size_t)k < s.n ? hexval(s.p[i + 1 + (size_t)k]) : -1;
                if (h < 0) cfg_fail(line, "truncated \\%c escape in a string", e);
                cp = cp * 16 + (uint32_t)h;
            }
            i += (size_t)digits;
            avp_buf_putcp(&b, cp);
            break;
        }
        case 'N': cfg_fail(line, "\\N{...} escapes are not supported");
        default:
            if (e >= '0' && e <= '7') {
                uint32_t cp = (uint32_t)(e - '0');
                for (int k = 0; k < 2 && i + 1 < s.n && s.p[i + 1] >= '0' && s.p[i + 1] <= '7'; k++)
                    cp = cp * 8 + (uint32_t)(s.p[++i] - '0');
                avp_buf_putcp(&b, cp);
            } else {
                avp_buf_putc(&b, '\\'); /* an unknown escape keeps its backslash */
                avp_buf_putc(&b, e);
            }
        }
    }
    return avp_buf_str(&b);
}

static const char *const k_ops[] = {"**=", "//=", ">>=", "<<=", "...", "->", ":=", "**", "//", "==", "!=", "<=", ">=",
                                    "<<", ">>", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "@=", NULL};

static void push(CfgToks *t, CfgTokKind kind, AvpStr text, int line) {
    CfgTok tok = {0};
    tok.kind = kind;
    tok.text = text;
    tok.line = line;
    avp_vec_push(t, tok);
}

CfgToks cfg_lex(AvpArena *a, AvpStr src, int first_line, bool expr_only) {
    CfgToks t = {0};
    AVP_VEC(int) indents = {0};
    avp_vec_push(&indents, 0);
    int line = first_line, depth = expr_only ? 1 : 0;
    size_t i = 0, n = src.n;
    const char *p = src.p;
    bool at_line_start = !expr_only;
    while (i < n) {
        if (at_line_start) {
            /* measure the indentation; blank and comment-only lines are skipped */
            int col = 0;
            size_t j = i;
            while (j < n && (p[j] == ' ' || p[j] == '\t' || p[j] == '\f')) {
                col = p[j] == '\t' ? (col / 8 + 1) * 8 : p[j] == ' ' ? col + 1 : 0;
                j++;
            }
            if (j >= n) {
                i = j;
                break;
            }
            if (p[j] == '#' || p[j] == '\n' || (p[j] == '\\' && j + 1 < n && p[j + 1] == '\n')) {
                if (p[j] == '\\') { /* a continuation on an empty line joins the next one */
                    i = j + 2;
                    line++;
                    continue;
                }
                while (j < n && p[j] != '\n') j++;
                i = j < n ? j + 1 : j;
                line++;
                continue;
            }
            i = j;
            at_line_start = false;
            if (col > avp_vec_last(&indents)) {
                avp_vec_push(&indents, col);
                push(&t, CFG_T_INDENT, AVP_S(""), line);
            } else {
                while (col < avp_vec_last(&indents)) {
                    (void)avp_vec_pop(&indents);
                    push(&t, CFG_T_DEDENT, AVP_S(""), line);
                }
                if (col != avp_vec_last(&indents)) cfg_fail(line, "unindent does not match any outer indentation level");
            }
        }
        char c = p[i];
        if (c == ' ' || c == '\t' || c == '\f' || c == '\r') {
            i++;
            continue;
        }
        if (c == '#') {
            while (i < n && p[i] != '\n') i++;
            continue;
        }
        if (c == '\\' && i + 1 < n && p[i + 1] == '\n') {
            i += 2;
            line++;
            continue;
        }
        if (c == '\n') {
            i++;
            if (depth > 0) {
                line++;
                continue;
            }
            push(&t, CFG_T_NEWLINE, AVP_S(""), line);
            line++;
            at_line_start = true;
            continue;
        }
        /* strings, with an optional prefix */
        size_t pre = 0;
        while (pre < 2 && i + pre < n && strchr("rRbBfFuU", p[i + pre]) && p[i + pre] != 0) pre++;
        if (i + pre < n && (p[i + pre] == '\'' || p[i + pre] == '"') &&
            (pre == 0 || !(i + pre < n && is_ident((unsigned char)p[i + pre]))) ) {
            bool raw = false, fs = false, bytes = false;
            for (size_t k = 0; k < pre; k++) {
                char pc = p[i + k];
                if (pc == 'r' || pc == 'R') raw = true;
                else if (pc == 'f' || pc == 'F') fs = true;
                else if (pc == 'b' || pc == 'B') bytes = true;
            }
            if (bytes) cfg_fail(line, "bytes literals are not supported");
            char q = p[i + pre];
            size_t j = i + pre;
            bool triple = j + 2 < n && p[j + 1] == q && p[j + 2] == q;
            size_t qlen = triple ? 3 : 1;
            size_t start = j + qlen, k = start;
            int start_line = line;
            for (;;) {
                if (k >= n) cfg_fail(start_line, "unterminated string literal");
                if (p[k] == '\\' && k + 1 < n) {
                    if (p[k + 1] == '\n') line++;
                    k += 2;
                    continue;
                }
                if (p[k] == '\n') {
                    if (!triple) cfg_fail(start_line, "unterminated string literal");
                    line++;
                }
                if (p[k] == q && (!triple || (k + 2 < n && p[k + 1] == q && p[k + 2] == q))) break;
                k++;
            }
            AvpStr body = avp_strn(p + start, k - start);
            CfgTok tok = {0};
            tok.kind = CFG_T_STR;
            tok.line = start_line;
            tok.fstr = fs;
            tok.raw = raw;
            tok.text = fs || raw ? body : cfg_unescape(a, body, start_line);
            avp_vec_push(&t, tok);
            i = k + qlen;
            continue;
        }
        if (is_ident_start((unsigned char)c)) {
            size_t j = i;
            while (j < n && is_ident((unsigned char)p[j])) j++;
            push(&t, CFG_T_NAME, avp_strn(p + i, j - i), line);
            i = j;
            continue;
        }
        if (c >= '0' && c <= '9') {
            size_t j = i;
            int64_t v = 0;
            int base = 10;
            if (c == '0' && j + 1 < n && (p[j + 1] == 'x' || p[j + 1] == 'X')) base = 16, j += 2;
            else if (c == '0' && j + 1 < n && (p[j + 1] == 'o' || p[j + 1] == 'O')) base = 8, j += 2;
            else if (c == '0' && j + 1 < n && (p[j + 1] == 'b' || p[j + 1] == 'B')) base = 2, j += 2;
            for (; j < n; j++) {
                if (p[j] == '_') continue;
                int d = hexval(p[j]);
                if (d < 0 || d >= base) break;
                v = v * base + d;
            }
            if (j < n && (p[j] == '.' || p[j] == 'e' || p[j] == 'E' || p[j] == 'j' || p[j] == 'J' || is_ident_start((unsigned char)p[j])))
                cfg_fail(line, "float, complex and malformed number literals are not supported");
            CfgTok tok = {0};
            tok.kind = CFG_T_INT;
            tok.text = avp_strn(p + i, j - i);
            tok.ival = v;
            tok.line = line;
            avp_vec_push(&t, tok);
            i = j;
            continue;
        }
        if (c == '.' && i + 1 < n && p[i + 1] >= '0' && p[i + 1] <= '9') cfg_fail(line, "float literals are not supported");
        size_t oplen = 1;
        for (const char *const *op = k_ops; *op; op++) {
            size_t l = strlen(*op);
            if (i + l <= n && memcmp(p + i, *op, l) == 0) {
                oplen = l;
                break;
            }
        }
        if (oplen == 1 && !strchr("()[]{},:;.@=+-*/%<>~&|^!", c)) cfg_fail(line, "unexpected character '%c'", c);
        if (oplen == 1 && c == '!') cfg_fail(line, "unexpected character '!'");
        if (oplen == 1) {
            if (c == '(' || c == '[' || c == '{') depth++;
            if ((c == ')' || c == ']' || c == '}') && depth > (expr_only ? 1 : 0)) depth--;
        }
        push(&t, CFG_T_OP, avp_strn(p + i, oplen), line);
        i += oplen;
    }
    if (!expr_only) {
        if (t.n && t.v[t.n - 1].kind != CFG_T_NEWLINE && t.v[t.n - 1].kind != CFG_T_DEDENT)
            push(&t, CFG_T_NEWLINE, AVP_S(""), line);
        while (indents.n > 1) {
            (void)avp_vec_pop(&indents);
            push(&t, CFG_T_DEDENT, AVP_S(""), line);
        }
    }
    push(&t, CFG_T_EOF, AVP_S(""), line);
    avp_vec_free(&indents);
    return t;
}
