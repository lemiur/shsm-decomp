/* decomp_store: decomp.py's Paths, load_json/save_json, symbols_stamp, the index (load_index, merge_results,
 * stale_entries) and the read-only listing lookup (listing() without export) offered in decomp_listing.h. */
#include "decomp.h"

#include <string.h>

DecompPaths *decomp_paths(AvpArena *a, const ShsmRepo *r) {
    DecompPaths *p = AVP_NEW(a, DecompPaths);
    p->repo = r;
    p->root = avp_join(a, r->root, "build", "ghidra");
    p->state = avp_join(a, p->root, "state.json");
    p->project = avp_join(a, p->root, "project");
    p->input = avp_join(a, p->root, "input");
    p->symbols_tsv = avp_join(a, p->input, "symbols.tsv");
    p->applied_tsv = avp_join(a, p->input, "applied_symbols.tsv");
    p->decomp = avp_join(a, p->root, "decomp");
    p->index = avp_join(a, p->decomp, "index.json");
    p->logs = avp_join(a, p->root, "logs");
    return p;
}

bool decomp_project_exists(const DecompPaths *p) {
    AvpArena *s = avp_scratch();
    AvpArenaMark m = avp_arena_mark(s);
    bool ok = avp_isfile(avp_join(s, p->project, DECOMP_PROJECT_NAME ".gpr"));
    avp_arena_reset(s, m);
    return ok;
}

AvpJson *dc_load_json(AvpArena *a, const char *path, AvpJson *dflt) {
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TM_UTF8, &text)) return dflt; /* OSError, or a decode error (a ValueError) */
    AvpJsonError err = {0};
    AvpJson *v = avp_json_parse(a, text, &err);
    return v ? v : dflt;
}

void dc_save_json(const char *path, const AvpJson *data) {
    AvpArena *a = avp_arena();
    AvpStr dir = avp_path_dirname(path);
    if (dir.n && !avp_makedirs(avp_cz(a, dir), true)) shsm_raise_io();
    AvpJsonDumpOpts o = AVP_JSON_INDENT(1);
    o.sort_keys = true;
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_json_dump_buf(&b, data, &o);
    avp_buf_putc(&b, '\n');
    const char *tmp = avp_sprintf(a, "%s.tmp", path);
    if (!avp_write_file(tmp, b.p, b.n)) shsm_raise_io();
    if (!avp_replace(tmp, path)) shsm_raise_io();
}

const char *decomp_symbols_stamp(AvpArena *a, const char *symbols_txt) {
    AvpBytes data = {0};
    if (!avp_read_all(a, symbols_txt, &data)) shsm_raise_io();
    u8 d[20];
    avp_sha1(data.v, data.n, d);
    return avp_hex(a, d, 6);
}

AvpStr dc_pystr(AvpArena *a, const AvpJson *v) {
    if (!v || v->kind == AVP_JSON_NULL) return AVP_S("None");
    switch (v->kind) {
    case AVP_JSON_BOOL: return v->b ? AVP_S("True") : AVP_S("False");
    case AVP_JSON_INT: return v->big ? v->u.str : avp_pyfmt(a, "%d", v->u.i);
    case AVP_JSON_FLOAT: return avp_pyfmt(a, "%v", v->u.f);
    case AVP_JSON_STRING: return v->u.str;
    default: {
        /* str() of a list or dict: Python's repr, approximated by the JSON text (cannot occur in a store the tool
         * wrote) */
        return avp_json_dumps(a, v, NULL);
    }
    }
}

bool dc_json_ne_str(const AvpJson *obj, const char *key, const char *s) {
    const AvpJson *v = obj && avp_json_is_obj(obj) ? avp_json_getz(obj, key) : NULL;
    if (!v || v->kind != AVP_JSON_STRING) return true;
    return !avp_str_eqz(v->u.str, s);
}

/* int(text, base) for the bases used here: Python accepts surrounding whitespace, a sign, '_' between digits and
 * (base 16) a 0x prefix. */
static i64 py_int(AvpStr text, int base) {
    AvpStr t = avp_str_strip(text);
    bool neg = false;
    size_t i = 0;
    if (i < t.n && (t.p[i] == '+' || t.p[i] == '-')) neg = t.p[i++] == '-';
    if (base == 16 && i + 1 < t.n && t.p[i] == '0' && (t.p[i + 1] == 'x' || t.p[i + 1] == 'X')) {
        i += 2;
        if (i < t.n && t.p[i] == '_') i++;
    }
    u64 v = 0;
    bool any = false, prev_us = false;
    for (; i < t.n; i++) {
        char c = t.p[i];
        int d;
        if (c == '_' && any && !prev_us) {
            prev_us = true;
            continue;
        }
        if (c >= '0' && c <= '9') d = c - '0';
        else if (base == 16 && c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (base == 16 && c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else goto bad;
        v = v * (u64)base + (u64)d;
        any = true;
        prev_us = false;
    }
    if (!any || prev_us) goto bad;
    return neg ? -(i64)v : (i64)v;
bad:
    shsm_raise(SHSM_EXC_VALUE, "invalid literal for int() with base %d: %s", base,
               avp_pyfmt(avp_arena(), "%R", &text).p);
}

i64 dc_int16(AvpStr text) { return py_int(text, 16); }
i64 dc_int10(AvpStr text) { return py_int(text, 10); }

AvpJson *decomp_load_index(AvpArena *a, const DecompPaths *p) {
    AvpJson *d = avp_json_obj(a);
    avp_json_set(a, d, AVP_S("functions"), avp_json_obj(a));
    return dc_load_json(a, p->index, d);
}

AvpJson *decomp_index_entry(const AvpJson *index, u64 address) {
    if (!index || !avp_json_is_obj(index)) return NULL;
    AvpJson *funcs = avp_json_getz(index, "functions");
    if (!funcs || !avp_json_is_obj(funcs)) return NULL;
    char key[16];
    snprintf(key, sizeof key, "%08llX", (unsigned long long)address);
    return avp_json_getz(funcs, key);
}

static AvpJson *json_get_obj(AvpArena *a, AvpJson *obj, const char *key) {
    AvpJson *v = avp_json_getz(obj, key);
    if (!v) v = avp_json_set(a, obj, avp_str(key), avp_json_obj(a));
    return v;
}

void dc_merge_results(AvpArena *a, AvpJson *index, const char *results_tsv, const AvpJson *stamp,
                      const AvpJson *ghidra_version, i64 *ok, i64 *failed) {
    *ok = *failed = 0;
    AvpStr text;
    if (!avp_read_text(a, results_tsv, AVP_TM_UTF8, &text)) shsm_raise_io();
    AvpStrVec lines = avp_str_splitlines(text, false);
    AvpJson *funcs = json_get_obj(a, index, "functions"); /* index.setdefault("functions", {}) */
    for (size_t i = 1; i < lines.n; i++) {
        AvpStrVec c = avp_str_split(lines.v[i], AVP_S("\t"), -1);
        if (c.n < 6) {
            avp_vec_free(&c);
            continue;
        }
        AvpJson *e = avp_json_obj(a);
        avp_json_set(a, e, AVP_S("name"), avp_json_str(a, c.v[1]));
        avp_json_set(a, e, AVP_S("file"), avp_json_str(a, c.v[2]));
        avp_json_set(a, e, AVP_S("status"), avp_json_str(a, c.v[3]));
        avp_json_set(a, e, AVP_S("reason"), avp_json_str(a, c.v[4]));
        AvpJson *refs = avp_json_arr(a);
        AvpStrVec rs = avp_str_split(c.v[5], AVP_S(","), -1);
        for (size_t j = 0; j < rs.n; j++)
            if (rs.v[j].n) avp_json_arr_push(a, refs, avp_json_str(a, rs.v[j]));
        avp_vec_free(&rs);
        avp_json_set(a, e, AVP_S("refs"), refs);
        avp_json_set(a, e, AVP_S("stamp"), (AvpJson *)stamp);
        avp_json_set(a, e, AVP_S("ghidra"), ghidra_version ? (AvpJson *)ghidra_version : avp_json_null());
        avp_json_set(a, funcs, c.v[0], e);
        if (avp_str_eqz(c.v[3], "ok")) (*ok)++;
        else (*failed)++;
        avp_vec_free(&c);
    }
    avp_vec_free(&lines);
}

AvpStrVec dc_stale_entries(AvpArena *a, const AvpJson *index, const char *stamp) {
    AvpStrVec out = {0};
    AvpJson *funcs = avp_json_getz(index, "functions");
    for (size_t i = 0; funcs && i < avp_json_len(funcs); i++) {
        const AvpJsonMember *m = avp_json_member(funcs, i);
        if (dc_json_ne_str(m->val, "stamp", stamp)) avp_vec_push(&out, avp_str_dup(a, m->key));
    }
    avp_sort_strs(out.v, out.n);
    return out;
}

const char *decomp_entry_file(AvpArena *a, const DecompPaths *p, const AvpJson *entry) {
    const AvpJson *f = entry && avp_json_is_obj(entry) ? avp_json_getz(entry, "file") : NULL;
    if (!f || f->kind != AVP_JSON_STRING) return NULL;
    return avp_path_join(a, p->decomp, avp_cz(a, f->u.str));
}

/* e is None or not os.path.isfile(os.path.join(paths.decomp, e.get("file") or "-")) */
bool dc_entry_has_file(AvpArena *a, const DecompPaths *p, const AvpJson *e) {
    if (!e) return false;
    const AvpJson *f = avp_json_is_obj(e) ? avp_json_getz(e, "file") : NULL;
    const char *name = f && avp_json_truthy(f) && f->kind == AVP_JSON_STRING ? avp_cz(a, f->u.str) : "-";
    return avp_isfile(avp_path_join(a, p->decomp, name));
}

DecompListing decomp_listing_at(AvpArena *a, const ShsmRepo *r, u64 address) {
    DecompPaths *p = decomp_paths(a, r);
    DecompListing out = {0};
    AvpJson *e = decomp_index_entry(decomp_load_index(a, p), address);
    out.entry = e;
    if (!dc_entry_has_file(a, p, e)) return out;
    if (!avp_read_text(a, decomp_entry_file(a, p, e), AVP_TM_UTF8, &out.text)) shsm_raise_io();
    out.found = true;
    out.stale = dc_json_ne_str(e, "stamp", decomp_symbols_stamp(a, r->symbols_txt));
    return out;
}
