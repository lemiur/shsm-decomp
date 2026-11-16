/* Small text helpers (Python's \s, shlex.split). */
#include "shsm_text.h"

size_t shsm_ws_len(AvpStr s, size_t i) {
    if (i >= s.n) return 0;
    unsigned char c = (unsigned char)s.p[i];
    if (c < 0x80) return avp_cp_isspace(c) ? 1 : 0;
    const char *p = s.p + i;
    u32 cp = avp_u8_next(&p, s.p + s.n);
    return avp_cp_isspace(cp) ? (size_t)(p - (s.p + i)) : 0;
}

size_t shsm_skip_ws(AvpStr s, size_t i) {
    size_t k;
    while ((k = shsm_ws_len(s, i)) != 0) i += k;
    return i;
}

/* shlex.shlex(s, posix=True), whitespace_split=True, commenters='': read_token's state machine. */
bool shsm_shlex_split(AvpArena *a, AvpStr s, AvpStrVec *out, const char **err) {
    size_t i = 0;
    for (;;) {
        AvpBuf tok;
        avp_buf_init(&tok, a);
        bool quoted = false;
        char state = ' ', escapedstate = ' ';
        bool eof = false;
        for (;;) {
            if (i >= s.n) {
                eof = true;
                if (state == '\'' || state == '"') {
                    *err = "No closing quotation";
                    return false;
                }
                if (state == '\\') {
                    *err = "No escaped character";
                    return false;
                }
                break;
            }
            char c = s.p[i++];
            bool ws = c == ' ' || c == '\t' || c == '\r' || c == '\n';
            if (state == ' ') {
                if (ws) {
                    if (tok.n || quoted) break;
                    continue;
                }
                if (c == '\\') {
                    escapedstate = 'a';
                    state = '\\';
                } else if (c == '\'' || c == '"') {
                    state = c;
                } else {
                    avp_buf_putc(&tok, c);
                    state = 'a';
                }
            } else if (state == '\'' || state == '"') {
                quoted = true;
                if (c == state) {
                    state = 'a';
                } else if (c == '\\' && state == '"') {
                    escapedstate = state;
                    state = '\\';
                } else {
                    avp_buf_putc(&tok, c);
                }
            } else if (state == '\\') {
                if ((escapedstate == '\'' || escapedstate == '"') && c != '\\' && c != escapedstate)
                    avp_buf_putc(&tok, '\\');
                avp_buf_putc(&tok, c);
                state = escapedstate;
            } else { /* 'a' */
                if (ws) {
                    state = ' ';
                    if (tok.n || quoted) break;
                    continue;
                }
                if (c == '\'' || c == '"') {
                    state = c;
                } else if (c == '\\') {
                    escapedstate = 'a';
                    state = '\\';
                } else {
                    avp_buf_putc(&tok, c);
                }
            }
        }
        if (tok.n || quoted) avp_vec_push(out, avp_str(avp_buf_cz(&tok)));
        if (eof) break;
    }
    return true;
}
