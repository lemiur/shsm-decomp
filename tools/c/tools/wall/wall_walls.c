/* build/walls.json (wall.py walls_path, load_walls; match.py inputs_staleness) and the parked-function notes of a
 * source (wall.py find_notes, note_classes, NOTE, CLASS_WORDS, _base_name). */
#include "wall.h"

#include "match.h"
#include "shsm_cache.h"
#include "shsm_re.h"

/* ---- walls.json ------------------------------------------------------------------------------------------------ */

const char *wall_walls_path(AvpArena *a, const ShsmRepo *r) { return avp_join(a, r->root, "build", "walls.json"); }

static AvpStr slashes(AvpArena *a, const char *p) { return avp_str_replace(a, avp_str(p), AVP_S("\\"), AVP_S("/"), -1); }

/* file_digest(path) if it exists else None, as the JSON value the record holds */
static AvpJson *digest_or_none(AvpArena *a, const char *path) {
    return avp_exists(path) ? avp_json_strz(a, shsm_file_digest(a, path)) : avp_json_null();
}

AvpStr wall_inputs_staleness(AvpArena *a, const ShsmRepo *r, const char *unit, const AvpJson *inp) {
    AvpJson one = {.kind = AVP_JSON_INT, .u.i = MATCH_STATE_INPUTS_VERSION};
    if (!avp_json_truthy(inp) || !avp_json_is_obj(inp) || !wall_py_eq(avp_json_getz(inp, "version"), &one))
        return AVP_S("no inputs recorded (a --file or --built run, or an older tool)");
    const ShsmUnit *u = shsm_get_unit(r, unit);
    const AvpJson *src = avp_json_getz(inp, "source");
    if (!u->source || !(avp_json_is_str(src) && avp_str_eq(src->u.str, slashes(a, u->source)))) {
        AvpStr s = wall_py_str(a, src);
        return avp_pyfmt(a, "recorded for %S, not the unit's source", &s);
    }
    const AvpJson *flags = avp_json_getz(inp, "flags");
    if (!(avp_json_is_str(flags) && avp_str_eqz(flags->u.str, match_flags_key(a, u)))) return AVP_S("compiler or flags changed");
    const AvpJson *files = avp_json_getz(inp, "files");
    for (size_t i = 0; avp_json_is_obj(files) && i < files->u.obj.n; i++) {
        AvpStr path = files->u.obj.v[i].key;
        const AvpJson *digest = files->u.obj.v[i].val;
        const char *pz = avp_cz(a, path);
        const char *full = avp_path_isabs(pz) ? pz : avp_path_join(a, r->root, pz);
        AvpTry t;
        bool same = false;
        if (AVP_TRY(&t)) {
            const char *d = shsm_file_digest(a, full);
            same = avp_json_is_str(digest) && avp_str_eqz(digest->u.str, d);
            avp_try_end(&t);
        } else {
            if (!shsm_exc_is_os(t.code)) avp_throw(t.code, "%s", t.msg);
            return avp_pyfmt(a, "%S is gone", &path);
        }
        if (!same) return avp_pyfmt(a, "%S changed", &path);
    }
    const char *asm_path = shsm_repo_asm_path(a, r, u->name);
    if (!wall_py_eq(avp_json_getz(inp, "retail"), digest_or_none(a, asm_path))) return AVP_S("retail listing changed");
    if (!wall_py_eq(avp_json_getz(inp, "symbols"), digest_or_none(a, r->symbols_txt))) return AVP_S("symbols.txt changed");
    return AVP_S("");
}

AvpJson *wall_load_walls(AvpArena *a, const ShsmRepo *r, bool include_stale) {
    AvpStr text;
    if (!avp_read_text(a, wall_walls_path(a, r), AVP_TM_UTF8, &text)) {
        avp_io_clear();
        return NULL; /* OSError, or a UnicodeDecodeError (a ValueError) */
    }
    AvpJsonError jerr;
    AvpJson *doc = avp_json_parse(a, text, &jerr);
    if (!doc || !avp_json_is_obj(doc)) return NULL;
    const AvpJson *inputs = avp_json_getz(doc, "inputs");
    if (!avp_json_truthy(inputs)) inputs = NULL;
    const AvpJson *units = avp_json_getz(doc, "units");
    size_t nunits = avp_json_is_arr(units) ? avp_json_len(units) : 0;
    AvpJson *stale = avp_json_obj(a);
    for (size_t i = 0; i < nunits; i++) {
        const AvpJson *uv = avp_json_at(units, i);
        AvpStr un = wall_py_str(a, uv);
        const AvpJson *inp = (inputs && avp_json_is_str(uv)) ? avp_json_get(inputs, uv->u.str) : NULL;
        AvpStr why;
        AvpTry t;
        if (AVP_TRY(&t)) {
            why = wall_inputs_staleness(a, r, avp_cz(a, un), inp);
            avp_try_end(&t);
        } else {
            if (!shsm_exc_is_key(t.code)) avp_throw(t.code, "%s", t.msg);
            why = AVP_S("unit no longer exists");
        }
        if (why.n) avp_json_set(a, stale, un, avp_json_str(a, why));
    }
    avp_json_set(a, doc, AVP_S("stale"), stale);
    if (!include_stale) {
        AvpJson *keep = avp_json_arr(a);
        for (size_t i = 0; i < nunits; i++) {
            AvpJson *uv = avp_json_at(units, i);
            if (!(avp_json_is_str(uv) && avp_json_has(stale, uv->u.str))) avp_json_arr_push(a, keep, uv);
        }
        avp_json_set(a, doc, AVP_S("units"), keep);
        const AvpJson *fns = avp_json_getz(doc, "functions");
        AvpJson *fkeep = avp_json_arr(a);
        for (size_t i = 0; avp_json_is_arr(fns) && i < avp_json_len(fns); i++) {
            AvpJson *f = avp_json_at(fns, i);
            const AvpJson *fu = avp_json_is_obj(f) ? avp_json_getz(f, "unit") : NULL;
            if (!(avp_json_is_str(fu) && avp_json_has(stale, fu->u.str))) avp_json_arr_push(a, fkeep, f);
        }
        avp_json_set(a, doc, AVP_S("functions"), fkeep);
    }
    return doc;
}

/* ---- notes ----------------------------------------------------------------------------------------------------- */

#define NOTE_RE                                                                                                       \
    "(?:\\bNonMatching:|MATCH-PENDING|\\bNot exact\\b|\\bNot yet matched\\b|\\bParked:|\\bdoes not match\\b|"        \
    "\\bnot matching\\b|\\bwall:)"
#define ADDR_ABOVE_RE "^\\s*//\\s*0x(8[0-9A-Fa-f]{7})\\s*$"
#define ADDR_BRACE_RE "^\\}\\s*//\\s*0x(8[0-9A-Fa-f]{7})\\s*$"
#define RULE_NOTE_RE "MATCH-PENDING|\\bR\\d+\\b"

static const struct {
    const char *cls, *re;
} CLASS_WORDS[] = {
    {WALL_REFERENT, "referent|wrong symbol|placeholder|symbols\\.txt"},
    {WALL_CALL, "inlin|\\bcalls?\\b|out of line"},
    {WALL_CONTROL, "branch|\\bbeq\\b|\\bbne\\b|\\bble\\b|\\bblt\\b|entry jump|control flow|switch|early return|loop rotat"},
    {WALL_OTHER, "constant|immediate|offset"},
    {WALL_REGISTER, "regist|regalloc|reload|schedul|hoist|earlier|numbering|stack slot|\\bCSE\\b|temporar|swap"},
};

AvpStr wall_base_name(AvpStr name) {
    i64 k = avp_str_find(name, AVP_S("__"), 1, AVP_END);
    return k > 0 ? avp_strn(name.p, (size_t)k) : name;
}

size_t wall_note_classes(AvpStr text, const char *out[5]) {
    size_t n = 0;
    for (size_t i = 0; i < AVP_ARRAY_LEN(CLASS_WORDS); i++)
        if (shsm_re_search(shsm_re(CLASS_WORDS[i].re, SHSM_RE_I), text, 0, AVP_END, NULL)) out[n++] = CLASS_WORDS[i].cls;
    return n;
}

bool wall_is_rule_note(AvpStr text) { return shsm_re_search(shsm_re(RULE_NOTE_RE, 0), text, 0, AVP_END, NULL); }

i64 wall_note_keyword(AvpStr text) {
    ShsmReMatch m;
    return shsm_re_search(shsm_re(NOTE_RE, SHSM_RE_I), text, 0, AVP_END, &m) ? shsm_re_start(&m, 0) : -1;
}

static bool addr_match(const char *re, AvpStr line, u64 *addr) {
    ShsmReMatch m;
    if (!shsm_re_match(shsm_re(re, 0), line, 0, AVP_END, &m)) return false;
    if (addr) {
        AvpStr g = shsm_re_group(&m, 1);
        u64 v = 0;
        for (size_t i = 0; i < g.n; i++) {
            char c = g.p[i];
            v = v * 16 + (u64)(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10);
        }
        *addr = v;
    }
    return true;
}

static bool starts_comment(AvpStr s, bool with_block) {
    return avp_str_startswith(s, AVP_S("//")) || (with_block && avp_str_startswith(s, AVP_S("/*"))) ||
           avp_str_startswith(s, AVP_S("*"));
}

/* s.lstrip("/*").strip() */
static AvpStr strip_markers(AvpStr s) { return avp_str_strip(avp_str_lstrip_chars(s, AVP_S("/*"))); }

typedef struct {
    size_t k;
    u64 addr;
    bool above;
} Mark;

size_t wall_find_notes(AvpArena *a, const char *path, WallNote **out) {
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TM_UTF8_REPLACE, &text)) shsm_raise_io();
    AvpStrVec lines = avp_str_splitlines(text, false);
    size_t n = lines.n;
    Mark *marks = AVP_NEWN(a, Mark, n + 1);
    size_t nmarks = 0;
    for (size_t k = 0; k < n; k++) {
        u64 addr;
        if (addr_match(ADDR_ABOVE_RE, lines.v[k], &addr)) marks[nmarks++] = (Mark){k, addr, true};
        else if (addr_match(ADDR_BRACE_RE, lines.v[k], &addr)) marks[nmarks++] = (Mark){k, addr, false};
    }
    /* owner: line -> address of the function whose header / body holds it (index n: one past the end) */
    bool *has_owner = AVP_NEWN(a, bool, n + 2);
    u64 *owner = AVP_NEWN(a, u64, n + 2);
    i64 prev_end = -1;
    for (size_t i = 0; i < nmarks; i++) {
        size_t k = marks[i].k;
        i64 end;
        if (marks[i].above) {
            size_t j = k + 1;
            i64 depth = 0;
            bool started = false;
            while (j < n) {
                depth += (i64)avp_str_count(lines.v[j], AVP_S("{")) - (i64)avp_str_count(lines.v[j], AVP_S("}"));
                started = started || avp_str_find(lines.v[j], AVP_S("{"), 0, AVP_END) >= 0;
                if (started && depth <= 0) break;
                j++;
            }
            end = (i64)j;
        } else {
            end = (i64)k;
        }
        for (i64 x = prev_end + 1; x <= end; x++) {
            has_owner[x] = true;
            owner[x] = marks[i].addr;
        }
        prev_end = end;
    }
    /* in_block[k]: line k starts inside a block comment */
    bool *in_block = AVP_NEWN(a, bool, n + 1);
    bool inside = false;
    for (size_t k = 0; k < n; k++) {
        in_block[k] = inside;
        AvpStr ln = lines.v[k];
        i64 pos = 0;
        for (;;) {
            if (inside) {
                i64 e = avp_str_find(ln, AVP_S("*/"), pos, AVP_END);
                if (e < 0) break;
                inside = false;
                pos = e + 2;
            } else {
                i64 s_ = avp_str_find(ln, AVP_S("/*"), pos, AVP_END), c_ = avp_str_find(ln, AVP_S("//"), pos, AVP_END);
                if (s_ < 0 || (0 <= c_ && c_ < s_)) break;
                inside = true;
                pos = s_ + 2;
            }
        }
    }
    size_t header_end = n;
    for (size_t k = 0; k < n; k++) {
        AvpStr s = avp_str_strip(lines.v[k]);
        if (s.n == 0 || in_block[k] || starts_comment(s, true)) continue;
        header_end = k;
        break;
    }
    WallNote *notes = AVP_NEWN(a, WallNote, n + 1);
    size_t nn = 0;
    for (size_t k = 0; k < n; k++) {
        AvpStr ln = lines.v[k];
        i64 ms = wall_note_keyword(ln);
        if (ms < 0 || avp_str_find(ln, AVP_S("ORIGINAL-ASM"), 0, AVP_END) >= 0 ||
            avp_str_find(ln, AVP_S("RULE-EXCEPTION"), 0, AVP_END) >= 0)
            continue;
        AvpStr st = avp_str_strip(ln);
        if (!(in_block[k] || starts_comment(st, true) || avp_str_find(ln, AVP_S("//"), 0, ms) >= 0 || k < header_end))
            continue; /* code (an assert message), not a comment */
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_put(&b, strip_markers(st));
        for (size_t j = k + 1; j < n && j < k + 6; j++) {
            AvpStr s = avp_str_strip(lines.v[j]);
            if (!(avp_str_startswith(s, AVP_S("//")) || avp_str_startswith(s, AVP_S("*"))) ||
                addr_match(ADDR_ABOVE_RE, lines.v[j], NULL) || avp_str_strip_chars(s, AVP_S("/* =")).n == 0)
                break;
            avp_buf_putc(&b, ' ');
            avp_buf_put(&b, strip_markers(s));
        }
        WallNote *w = &notes[nn++];
        w->line = (i64)k + 1;
        w->has_addr = k >= header_end && has_owner[k];
        w->addr = w->has_addr ? owner[k] : 0;
        w->text = avp_buf_str(&b);
    }
    avp_vec_free(&lines);
    *out = notes;
    return nn;
}
