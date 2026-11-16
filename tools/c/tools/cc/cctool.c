/* cc.exe logic: the functions of python tools/mwcc_trace/cc.py. The Python's regular expressions are matched by
 * hand; each function names the pattern it reproduces. */
#include "cctool.h"
#include "shsm_text.h"
#include "shsm_units.h"

/* Python's \s on one byte (listings and build.ninja are ASCII). */
static bool is_ws(u8 c) { return cc_is_space(c); }
static bool is_hexu(u8 c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'); }
/* \w on one byte: ASCII letters, digits, underscore; a non-ASCII byte belongs to a letter in these texts. */
static bool is_word(u8 c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c >= 0x80;
}

/* ---- unit_flags --------------------------------------------------------------------------------------------- */

/* re.search(r"\n\s*<key> = (pattern)", block): the value after the first match. whole: (.*) to the end of the
 * line; else (\S+). */
static bool ninja_var(AvpStr block, const char *key, bool whole, AvpStr *out) {
    AvpStr k = avp_str(key);
    for (size_t i = 0; i < block.n; i++) {
        if (block.p[i] != '\n') continue;
        size_t j = i + 1;
        while (j < block.n && is_ws((u8)block.p[j])) j++;
        if (j + k.n + 3 > block.n || memcmp(block.p + j, k.p, k.n) != 0 || memcmp(block.p + j + k.n, " = ", 3) != 0)
            continue;
        size_t s = j + k.n + 3, e = s;
        if (whole) {
            while (e < block.n && block.p[e] != '\n') e++;
        } else {
            while (e < block.n && !is_ws((u8)block.p[e])) e++;
            if (e == s) continue;
        }
        *out = avp_strn(block.p + s, e - s);
        return true;
    }
    return false;
}

int cctool_unit_flags(AvpArena *a, AvpStr ninja, const char *unit, AvpStr *ver, AvpStrVec *cflags, const char **err) {
    AvpStr text = shsm_join_ninja_lines(a, ninja);
    AvpStr bs = avp_str_replace(a, avp_str(unit), AVP_S("/"), AVP_S("\\"), -1);
    AvpStr key = avp_str(avp_sprintf(a, "src\\%.*s.", AVP_SF(bs)));
    AvpStrVec blocks = avp_str_split(text, AVP_S("\nbuild "), -1);
    for (size_t b = 0; b < blocks.n; b++) {
        AvpStr block = blocks.v[b];
        i64 nl = avp_str_findc(block, '\n');
        AvpStr head = nl < 0 ? block : avp_strn(block.p, (size_t)nl);
        if (!avp_str_contains(head, AVP_S(": mwcc")) || !avp_str_contains(head, key)) continue;
        AvpStr v, cf;
        if (!ninja_var(block, "mw_version", false, &v) || !ninja_var(block, "cflags", true, &cf)) {
            avp_vec_free(&blocks);
            return 2;
        }
        /* re.sub(r"\s+", " ", cf) */
        AvpBuf nb;
        avp_buf_init(&nb, a);
        for (size_t i = 0; i < cf.n;) {
            if (is_ws((u8)cf.p[i])) {
                while (i < cf.n && is_ws((u8)cf.p[i])) i++;
                avp_buf_putc(&nb, ' ');
            } else {
                avp_buf_putc(&nb, cf.p[i++]);
            }
        }
        *ver = avp_str_replace(a, v, AVP_S("\\"), AVP_S("/"), -1);
        avp_vec_free(&blocks);
        AvpStrVec words = {0};
        if (!shsm_shlex_split(a, avp_buf_str(&nb), &words, err)) return 3;
        *cflags = words;
        return 0;
    }
    avp_vec_free(&blocks);
    return 1;
}

/* ---- listing ------------------------------------------------------------------------------------------------ */

size_t cctool_prefix_len(AvpStr s) {
    /* ^/\* [0-9A-F]+ [0-9A-F]+  [0-9A-F ]+ \*\/ */
    size_t i = 0;
    if (s.n < 3 || memcmp(s.p, "/* ", 3) != 0) return 0;
    i = 3;
    for (int field = 0; field < 2; field++) {
        size_t st = i;
        while (i < s.n && is_hexu((u8)s.p[i])) i++;
        if (i == st) return 0;
        size_t need = field == 0 ? 1 : 2;
        for (size_t k = 0; k < need; k++, i++)
            if (i >= s.n || s.p[i] != ' ') return 0;
    }
    size_t st = i;
    while (i < s.n && (is_hexu((u8)s.p[i]) || s.p[i] == ' ')) i++;
    /* the greedy run backs off to its last space, which must be followed by "*\/" */
    if (i - st < 2 || s.p[i - 1] != ' ' || i + 1 >= s.n || s.p[i] != '*' || s.p[i + 1] != '/') return 0;
    return i + 2;
}

/* re.match(r"\.fn (\S+),", line) with group(1).rstrip(","). */
static bool fn_header(AvpStr line, AvpStr *name) {
    if (line.n < 4 || memcmp(line.p, ".fn ", 4) != 0) return false;
    size_t s = 4, e = s;
    while (e < line.n && !is_ws((u8)line.p[e])) e++;
    /* \S+ backs off to the last comma of the run (at least one character before it) */
    size_t k = e;
    while (k > s + 1 && line.p[k - 1] != ',') k--;
    if (k <= s + 1 || line.p[k - 1] != ',') return false;
    AvpStr g = avp_strn(line.p + s, k - 1 - s);
    *name = avp_str_rstrip_chars(g, AVP_S(","));
    return true;
}

/* The lines of a text file as `for line in f` gives them (newline translation already done). */
static AvpStrVec file_lines(AvpStr text) {
    AvpStrVec out = {0};
    size_t s = 0;
    for (size_t i = 0; i < text.n; i++) {
        if (text.p[i] == '\n') {
            avp_vec_push(&out, avp_strn(text.p + s, i + 1 - s));
            s = i + 1;
        }
    }
    if (s < text.n) avp_vec_push(&out, avp_strn(text.p + s, text.n - s));
    return out;
}

AvpStr cctool_filter_listing(AvpArena *a, AvpStr listing, const AvpSet *symbols, bool raw) {
    bool all = !symbols || avp_set_len(symbols) == 0;
    AvpStrVec lines = file_lines(listing);
    AvpStrVec out = {0};
    AvpStr cur = {0};
    bool in_fn = false;
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr line = lines.v[i], name;
        if (fn_header(line, &name)) {
            cur = name;
            in_fn = true;
            if (all || avp_set_has(symbols, cur)) avp_vec_push(&out, avp_pyfmt(a, "%S:", &cur));
            continue;
        }
        if (avp_str_startswith(line, AVP_S(".endfn"))) {
            in_fn = false;
            continue;
        }
        if (in_fn && (all || avp_set_has(symbols, cur))) {
            line = avp_str_rstrip(line);
            if (!raw) {
                size_t p = cctool_prefix_len(line);
                if (p) {
                    while (p < line.n && is_ws((u8)line.p[p])) p++;
                    line = avp_str_concat(a, AVP_S("    "), avp_strn(line.p + p, line.n - p));
                }
            }
            avp_vec_push(&out, line);
        }
    }
    AvpStr r = avp_str_join(a, AVP_S("\n"), out.v, out.n);
    avp_vec_free(&lines);
    avp_vec_free(&out);
    return r;
}

/* ---- the --no-targets comparison ---------------------------------------------------------------------------- */

AvpStr cctool_norm(AvpArena *a, AvpStr line) {
    size_t p = cctool_prefix_len(line);
    line = avp_str_strip(avp_strn(line.p + p, line.n - p));
    if (!line.n || line.p[0] == '.' || line.p[0] == '#') return AVP_S("");
    /* \.L_[0-9A-F]+ -> L */
    AvpBuf b1;
    avp_buf_init(&b1, a);
    for (size_t i = 0; i < line.n;) {
        if (i + 3 < line.n && memcmp(line.p + i, ".L_", 3) == 0 && is_hexu((u8)line.p[i + 3])) {
            i += 3;
            while (i < line.n && is_hexu((u8)line.p[i])) i++;
            avp_buf_putc(&b1, 'L');
        } else {
            avp_buf_putc(&b1, line.p[i++]);
        }
    }
    AvpStr s1 = avp_buf_str(&b1);
    /* \b(lbl|fn)_[0-9A-F]+ -> SYM */
    AvpBuf b2;
    avp_buf_init(&b2, a);
    for (size_t i = 0; i < s1.n;) {
        size_t w = 0;
        if (i == 0 || !is_word((u8)s1.p[i - 1])) {
            if (i + 4 <= s1.n && memcmp(s1.p + i, "lbl_", 4) == 0) w = 4;
            else if (i + 3 <= s1.n && memcmp(s1.p + i, "fn_", 3) == 0) w = 3;
        }
        if (w && i + w < s1.n && is_hexu((u8)s1.p[i + w])) {
            i += w;
            while (i < s1.n && is_hexu((u8)s1.p[i])) i++;
            avp_buf_putz(&b2, "SYM");
        } else {
            avp_buf_putc(&b2, s1.p[i++]);
        }
    }
    AvpStr s2 = avp_buf_str(&b2);
    /* \s+ -> " " */
    AvpBuf b3;
    avp_buf_init(&b3, a);
    for (size_t i = 0; i < s2.n;) {
        if (is_ws((u8)s2.p[i])) {
            while (i < s2.n && is_ws((u8)s2.p[i])) i++;
            avp_buf_putc(&b3, ' ');
        } else {
            avp_buf_putc(&b3, s2.p[i++]);
        }
    }
    return avp_str_dup(a, avp_buf_str(&b3));
}

AvpMap *cctool_split_fns(AvpArena *a, AvpStr text) {
    AvpMap *fns = avp_map_new();
    AvpStrVec *cur = NULL;
    AvpStrVec lines = avp_str_splitlines(text, false);
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr line = lines.v[i];
        if (avp_str_endswith(line, AVP_S(":")) && !avp_str_startswith(line, AVP_S(" ")) &&
            !avp_str_startswith(line, AVP_S("."))) {
            cur = AVP_NEW(a, AvpStrVec);
            avp_map_put(fns, avp_strn(line.p, line.n - 1), NULL)->p = cur;
        } else if (cur) {
            avp_vec_push(cur, line);
        }
    }
    avp_vec_free(&lines);
    return fns;
}

/* The position after the first "^.fn NAME," line's text (before its newline), or -1. */
static i64 retail_fn_line_end(AvpStr retail, AvpStr name) {
    for (size_t i = 0; i < retail.n;) {
        size_t e = i;
        while (e < retail.n && retail.p[e] != '\n') e++;
        AvpStr line = avp_strn(retail.p + i, e - i);
        if (line.n >= 4 + name.n + 1 && memcmp(line.p, ".fn ", 4) == 0 && memcmp(line.p + 4, name.p, name.n) == 0 &&
            line.p[4 + name.n] == ',')
            return (i64)e;
        i = e + 1;
    }
    return -1;
}

bool cctool_retail_has(AvpStr retail, AvpStr name) { return retail_fn_line_end(retail, name) >= 0; }

bool cctool_retail_block(AvpStr retail, AvpStr name, AvpStr *body) {
    i64 st = retail_fn_line_end(retail, name);
    if (st < 0) return false;
    for (size_t q = (size_t)st; q < retail.n; q++) {
        if (retail.p[q] == '\n' && q + 1 + 6 <= retail.n && memcmp(retail.p + q + 1, ".endfn", 6) == 0) {
            *body = avp_strn(retail.p + st, q + 1 - (size_t)st);
            return true;
        }
    }
    return false;
}

AvpStr cctool_positional(AvpArena *a, AvpStr fn, const AvpStrVec *body, AvpStr retail_body) {
    AvpStrVec want = {0}, got = {0};
    AvpStrVec rl = avp_str_splitlines(retail_body, false);
    for (size_t i = 0; i < rl.n; i++) {
        AvpStr n = cctool_norm(a, rl.v[i]);
        if (n.n) avp_vec_push(&want, n);
    }
    for (size_t i = 0; i < body->n; i++) {
        AvpStr n = cctool_norm(a, body->v[i]);
        if (n.n) avp_vec_push(&got, n);
    }
    size_t lo = want.n < got.n ? want.n : got.n, hi = want.n < got.n ? got.n : want.n;
    AvpBuf b;
    avp_buf_init(&b, a);
    size_t nbad = 0;
    for (size_t i = 0; i < lo; i++)
        if (!avp_str_eq(want.v[i], got.v[i])) nbad++;
    nbad += hi - lo;
    avp_buf_pyfmt(&b, "%S: %d/%d rows differ", &fn, nbad, hi);
    size_t shown = 0;
    for (size_t i = 0; i < hi && shown < 40; i++) {
        if (i < lo) {
            if (avp_str_eq(want.v[i], got.v[i])) continue;
            avp_buf_pyfmt(&b, "\n  %3d %-32S | %S", i, &want.v[i], &got.v[i]);
        } else {
            avp_buf_pyfmt(&b, "\n  %3d %-32s | %s", i, "", "");
        }
        shown++;
    }
    avp_vec_free(&rl);
    avp_vec_free(&want);
    avp_vec_free(&got);
    return avp_buf_str(&b);
}

/* ---- the target comparison ---------------------------------------------------------------------------------- */

AvpStr cctool_describe_result(AvpArena *a, AvpStr name, const CcFnResult *r, bool show_unresolved, size_t limit) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_pyfmt(&b, "%S: %d/%d rows differ", &name, r->npositional, r->rows);
    if (r->targets.n) avp_buf_pyfmt(&b, ", %d target mismatches", r->targets.n);
    if (r->unresolved.n) avp_buf_pyfmt(&b, ", %d unresolved targets", r->unresolved.n);
    for (size_t i = 0; i < r->npositional && i < limit; i++) {
        const CcPositional *p = &r->positional[i];
        avp_buf_pyfmt(&b, "\n  %3d %-32S | %S", p->index, &p->retail, &p->compiled);
    }
    for (size_t i = 0; i < r->targets.n && i < limit; i++) {
        const CcTargetDiff *t = &r->targets.v[i];
        avp_buf_pyfmt(&b, "\n  %3d %-32S | %-32S [target]\n        -> %S", t->i, &t->retail, &t->compiled, &t->detail);
    }
    if (show_unresolved) {
        for (size_t i = 0; i < r->unresolved.n && i < limit; i++) {
            const CcTargetDiff *t = &r->unresolved.v[i];
            avp_buf_pyfmt(&b, "\n  %3d %-32S | %-32S [unresolved]\n        -> %S", t->i, &t->retail, &t->compiled,
                          &t->detail);
        }
    }
    return avp_buf_str(&b);
}

AvpStr cctool_fn_prefix(AvpStr name) {
    if (name.n < 11 || memcmp(name.p, "fn_", 3) != 0) return AVP_S("");
    for (size_t i = 3; i < 11; i++)
        if (!is_hexu((u8)name.p[i])) return AVP_S("");
    return avp_strn(name.p, 11);
}

/* The --cmp text over the aligned comparison match.exe uses (cc_align): the heading counts rows as match.exe does
 * (replace + delete + insert), then the differing pairs; a one-instruction shift is one insert or delete, not a run of
 * replaced rows. */
AvpStr cctool_describe_aligned(AvpArena *a, AvpStr name, const CcCompareResult *r, bool show_unresolved, size_t limit) {
    AvpBuf b;
    avp_buf_init(&b, a);
    const CcCounts *c = &r->counts;
    size_t rows = c->replace + c->delete_ + c->insert;
    size_t total = c->retail_rows > c->compiled_rows ? c->retail_rows : c->compiled_rows;
    avp_buf_pyfmt(&b, "%S: %d/%d rows differ (aligned: %d replaced, %d only in retail, %d only in compiled; retail %d / "
                      "compiled %d rows)",
                  &name, (i64)rows, (i64)total, (i64)c->replace, (i64)c->delete_, (i64)c->insert, (i64)c->retail_rows,
                  (i64)c->compiled_rows);
    if (c->target_mismatches) avp_buf_pyfmt(&b, ", %d target mismatches", (i64)c->target_mismatches);
    if (c->unresolved) avp_buf_pyfmt(&b, ", %d unresolved targets", (i64)c->unresolved);
    size_t shown = 0;
    for (size_t i = 0; i < r->npairs; i++) {
        const CcPair *p = &r->pairs[i];
        if (!cc_pair_differs(p)) continue;
        if (shown++ >= limit) continue;
        const char *m = p->status == CC_REPLACE ? "!" : p->status == CC_DELETE ? "-" : p->status == CC_INSERT ? "+" : "T";
        AvpStr rt = p->retail ? p->retail->text : AVP_S(""), ct = p->compiled ? p->compiled->text : AVP_S("");
        i64 idx = p->retail ? (i64)p->retail->index : (i64)p->compiled->index;
        avp_buf_pyfmt(&b, "\n  %s %3d %-32S | %S", m, idx, &rt, &ct);
        if (p->target && strcmp(p->target, "mismatch") == 0 && p->detail.p && p->detail.n)
            avp_buf_pyfmt(&b, "\n        -> %S", &p->detail);
    }
    if (shown > limit)
        avp_buf_pyfmt(&b, "\n  ... %d more differing rows (cut at %d)", (i64)(shown - limit), (i64)limit);
    if (show_unresolved && r->unresolved) {
        for (size_t i = 0; i < r->unresolved->n && i < limit; i++) {
            const CcTargetDiff *t = &r->unresolved->v[i];
            avp_buf_pyfmt(&b, "\n  %3d %-32S | %-32S [unresolved]\n        -> %S", t->i, &t->retail, &t->compiled,
                          &t->detail);
        }
    }
    return avp_buf_str(&b);
}
