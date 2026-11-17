/* solver_case: --case files. A case names a unit and a function, the expectations a run must meet, and literal
 * find/replace pairs applied to the scratch copy only (the unit's own source is never written). The format is in
 * solver.h (sv_case_parse). */
#include "solver.h"

static bool fail(AvpArena *a, const char **err, const char *path, size_t line, const char *msg) {
    *err = avp_sprintf(a, "%s:%zu: %s", path ? path : "<case>", line, msg);
    return false;
}

/* A heredoc after `find <<TAG`: the lines up to the line equal to TAG, joined with "\n". *i is the index of the
 * opening line; on return the index of the terminator. */
static bool heredoc(AvpArena *a, const AvpStrVec *lines, size_t *i, AvpStr tag, AvpStr *out) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t k = *i + 1; k < lines->n; k++) {
        AvpStr l = lines->v[k];
        if (l.n && l.p[l.n - 1] == '\r') l.n--;
        if (avp_str_eq(l, tag)) {
            *out = avp_str_dup(a, avp_buf_str(&b));
            *i = k;
            return true;
        }
        if (k > *i + 1) avp_buf_putc(&b, '\n');
        avp_buf_put(&b, l);
    }
    return false;
}

bool sv_case_parse(AvpArena *a, AvpStr text, const char *path, SvCase *out, const char **err) {
    *out = (SvCase){0};
    out->path = path;
    *err = NULL;
    AvpStrVec lines = avp_str_split(text, AVP_S("\n"), -1);
    bool want_replace = false;
    size_t find_line = 0;
    AvpStr pending = {0};
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr raw = lines.v[i];
        if (raw.n && raw.p[raw.n - 1] == '\r') raw.n--;
        AvpStr l = avp_str_strip(raw);
        if (!l.n || l.p[0] == '#') continue;
        AvpStr key = l, rest = {0};
        i64 sp = -1;
        for (size_t k = 0; k < l.n; k++)
            if (l.p[k] == ' ' || l.p[k] == '\t' || l.p[k] == ':') {
                sp = (i64)k;
                break;
            }
        if (sp >= 0) {
            key = avp_strn(l.p, (size_t)sp);
            rest = avp_str_strip(avp_str_slice(l, sp + 1, (i64)l.n));
        }
        bool colon = sp >= 0 && l.p[sp] == ':';
        if (avp_str_eqz(key, "unit") && !colon) {
            out->unit = avp_cz(a, rest);
        } else if (avp_str_eqz(key, "function") && !colon) {
            out->function = avp_cz(a, rest);
        } else if (avp_str_eqz(key, "expect") && !colon) {
            AvpStrVec w = avp_str_split(rest, AVP_S(""), 1);
            if (w.n != 2) {
                avp_vec_free(&w);
                return fail(a, err, path, i + 1, "expect takes a key and a value");
            }
            SvExpect e = {avp_cz(a, w.v[0]), avp_cz(a, avp_str_strip(w.v[1])), i + 1};
            avp_vec_free(&w);
            if (strcmp(e.key, "class") && strcmp(e.key, "verdict") && strcmp(e.key, "verdict-not") &&
                strcmp(e.key, "callee") && strcmp(e.key, "winner") && strcmp(e.key, "contains") &&
                strcmp(e.key, "not-contains") && strcmp(e.key, "candidates"))
                return fail(a, err, path, i + 1,
                            "expect key must be class, verdict, verdict-not, callee, winner, contains, not-contains or "
                            "candidates");
            avp_vec_push(&out->expects, e);
        } else if (avp_str_eqz(key, "find") || avp_str_eqz(key, "replace")) {
            bool is_find = avp_str_eqz(key, "find");
            if (is_find == want_replace)
                return fail(a, err, path, i + 1, is_find ? "find before the previous find's replace"
                                                         : "replace without a find");
            AvpStr value;
            size_t open_line = i + 1; /* the find line itself, before a heredoc moves i */
            if (colon) {
                /* one-line form: the text after "find:" with one leading space removed */
                AvpStr after = avp_str_slice(raw, (i64)(avp_str_find(raw, AVP_S(":"), 0, (i64)raw.n) + 1), (i64)raw.n);
                if (after.n && after.p[0] == ' ') after = avp_str_slice(after, 1, (i64)after.n);
                value = avp_str_dup(a, after);
            } else if (avp_str_startswith(rest, AVP_S("<<")) && rest.n > 2) {
                AvpStr tag = avp_str_strip(avp_str_slice(rest, 2, (i64)rest.n));
                if (!heredoc(a, &lines, &i, tag, &value))
                    return fail(a, err, path, i + 1, avp_sprintf(a, "heredoc not closed by a line '%.*s'", AVP_SF(tag)));
            } else {
                return fail(a, err, path, i + 1, "find/replace take `<<TAG` (a heredoc) or `:` (one line)");
            }
            if (is_find) {
                if (!value.n) return fail(a, err, path, i + 1, "empty find text");
                pending = value;
                find_line = open_line;
                want_replace = true;
            } else {
                avp_vec_push(&out->edits, ((SvEdit){pending, value, find_line}));
                want_replace = false;
            }
        } else {
            return fail(a, err, path, i + 1, avp_sprintf(a, "unknown key '%.*s'", AVP_SF(key)));
        }
    }
    avp_vec_free(&lines);
    if (want_replace) return fail(a, err, path, find_line, "find without a replace");
    if (!out->unit || !*out->unit) return fail(a, err, path, 0, "no unit line");
    if (!out->function || !*out->function) return fail(a, err, path, 0, "no function line");
    return true;
}

static AvpStr crlf(AvpArena *a, AvpStr s) {
    AvpStr t = avp_str_replace(a, s, AVP_S("\r\n"), AVP_S("\n"), -1);
    return avp_str_replace(a, t, AVP_S("\n"), AVP_S("\r\n"), -1);
}

bool sv_case_apply(AvpArena *a, const SvCase *c, AvpStr text, AvpStr *out, const char **err) {
    *err = NULL;
    bool is_crlf = avp_str_contains(text, AVP_S("\r\n"));
    AvpStr cur = text;
    for (size_t i = 0; i < c->edits.n; i++) {
        const SvEdit *e = &c->edits.v[i];
        AvpStr find = is_crlf ? crlf(a, e->find) : e->find, repl = is_crlf ? crlf(a, e->replace) : e->replace;
        size_t n = avp_str_count(cur, find);
        if (n != 1) {
            *err = avp_sprintf(a, "%s:%zu: the find text occurs %zu times in the source (it must occur exactly once)",
                               c->path ? c->path : "<case>", e->line, n);
            return false;
        }
        cur = avp_str_replace(a, cur, find, repl, 1);
    }
    *out = cur;
    return true;
}
