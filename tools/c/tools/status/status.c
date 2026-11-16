/* status.py: library_table, fmt_table, the headline (ledger_block, ledger_numbers, headline, fmt_headline), unit_rows,
 * fmt_unit, load_old_report, since, fmt_since, render_svg and human. */
#include "status.h"
#include "rules_cli.h"
#include "shsm_re.h"

#include <string.h>

const char *const STATUS_KINDS[STATUS_NKINDS] = {"matched", "exact", "progress", "unstarted"};
static const char *const GROUP_ORDER[] = {"game",     "climax",  "sdk",  "MSL_C",      "Runtime.PPCEABI.H",
                                          "MetroTRK", "crt",     "libjpeg", "zlib",    "renderware",
                                          "rwphysics", "fmod",   "unknown"};
#define NGROUPS (sizeof GROUP_ORDER / sizeof GROUP_ORDER[0])

/* ---------------------------------------------------------------------------------------------------------------
 * per-library table
 * ------------------------------------------------------------------------------------------------------------ */

int status_kind_of(const PickFn *f) {
    if (f->status && strcmp(f->status, "Asm") == 0) return 3;
    if (f->fuzzy >= 100.0) return f->linked ? 0 : 1;
    return 2;
}

static StatusRow *empty_row(AvpArena *a, const char *name) {
    StatusRow *r = AVP_NEW(a, StatusRow);
    r->library = name;
    return r;
}

static size_t group_index(const char *lib) {
    AvpStr g = shsm_library_group_of(lib);
    for (size_t i = 0; i < NGROUPS; i++)
        if (avp_str_eqz(g, GROUP_ORDER[i])) return i;
    return NGROUPS;
}

static int row_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const StatusRow *a = *(StatusRow *const *)x, *b = *(StatusRow *const *)y;
    size_t ia = group_index(a->library), ib = group_index(b->library);
    if (ia != ib) return ia < ib ? -1 : 1;
    return avp_str_cmp(avp_str(a->library), avp_str(b->library));
}

static bool truthy(const char *s) { return s && *s; }

static AvpStr ledger_read_text(AvpArena *a, const char *path);

/* The ledger's ORIGINAL-ASM sources: the rows of docs/exceptions.md's ORIGINAL-ASM table (unit, function) and every
 * function of an assembly source unit (`.s`). units: names of the `.s` units; pairs: "unit\0function". */
typedef struct {
    AvpSet *units, *pairs;
} AsmSources;

static AsmSources asm_sources(AvpArena *a, const char *root, const char *configure_py) {
    AsmSources s = {avp_set_new(), avp_set_new()};
    if (configure_py && avp_isfile(configure_py)) {
        LedgerCfgVec cfg = ledger_parse_configure(a, ledger_read_text(a, configure_py));
        for (size_t i = 0; i < cfg.n; i++)
            if (ledger_is_asm_source(cfg.v[i].path)) avp_set_add(s.units, cfg.v[i].unit);
    }
    const char *exc = avp_join(a, root, "docs", "exceptions.md");
    if (!avp_isfile(exc)) return s;
    LedgerExcRows rows = ledger_parse_exceptions(a, ledger_read_text(a, exc));
    for (size_t i = 0; i < rows.n; i++) {
        const LedgerExcRow *r = rows.v[i];
        if (strcmp(r->table, "ORIGINAL-ASM") != 0) continue;
        for (size_t u = 0; u < r->units.n; u++)
            for (size_t f = 0; f < r->functions.n; f++) {
                AvpBuf b = {.a = a};
                avp_buf_put(&b, r->units.v[u]);
                avp_buf_putc(&b, '\0');
                avp_buf_put(&b, r->functions.v[f]);
                avp_set_add(s.pairs, avp_buf_str(&b));
            }
    }
    return s;
}

static bool pair_in(AvpArena *a, AvpSet *pairs, const char *unit, AvpStr fn) {
    AvpBuf b = {.a = a};
    avp_buf_putz(&b, unit);
    avp_buf_putc(&b, '\0');
    avp_buf_put(&b, fn);
    return avp_set_has(pairs, avp_buf_str(&b));
}

/* The ledger's test (ledger_model.c): linked, and the unit is a `.s` unit or the (unit, name or alias) pair is in the
 * ORIGINAL-ASM table; the alias is the demangled name up to its `(`. */
static bool is_original_asm(AvpArena *a, const AsmSources *s, const PickFn *f) {
    if (!f->linked || !f->unit) return false;
    if (avp_set_has(s->units, avp_str(f->unit))) return true;
    if (pair_in(a, s->pairs, f->unit, avp_str(f->name))) return true;
    if (f->demangled && *f->demangled) {
        AvpStr dm = avp_str(f->demangled);
        i64 paren = avp_str_findc(dm, '(');
        if (pair_in(a, s->pairs, f->unit, paren >= 0 ? avp_strn(dm.p, (size_t)paren) : dm)) return true;
    }
    return false;
}

void status_library_table(AvpArena *a, PickWorld *w, const char *lib, bool detail, StatusRows *rows,
                          StatusRow **total) {
    AsmSources asrc = asm_sources(a, w->repo->root, w->repo->configure_py);
    AvpMap *by = avp_map_new();
    StatusRows out = {0};
    StatusRow *tot = empty_row(a, "total");
    for (size_t i = 0; i < w->fns.n; i++) {
        const PickFn *f = w->fns.v[i];
        if (!pick_lib_matches(a, f->library, lib, f->unit_library)) continue;
        const char *key = (detail || truthy(lib)) ? (f->library ? f->library : "unknown")
                                                  : avp_cz(a, shsm_library_group_of(f->library));
        bool created = false;
        AvpVal *slot = avp_map_put(by, avp_str(key), &created);
        if (created) {
            slot->p = empty_row(a, key);
            avp_vec_push(&out, (StatusRow *)slot->p);
        }
        StatusRow *r = slot->p;
        int k = status_kind_of(f);
        bool is_asm = is_original_asm(a, &asrc, f);
        StatusRow *both[2] = {r, tot};
        for (int j = 0; j < 2; j++) {
            both[j]->functions += 1;
            both[j]->bytes += f->size;
            if (is_asm) {
                both[j]->asm_n += 1;
                both[j]->asm_b += f->size;
            } else {
                both[j]->n[k] += 1;
                both[j]->b[k] += f->size;
            }
        }
    }
    avp_map_free(by);
    avp_sort(out.v, out.n, sizeof out.v[0], row_cmp, NULL, false);
    *rows = out;
    *total = tot;
}

AvpJson *status_row_json(AvpArena *a, const StatusRow *r) {
    AvpJson *d = avp_json_obj(a);
    avp_json_set(a, d, AVP_S("library"), avp_json_strz(a, r->library));
    avp_json_set(a, d, AVP_S("functions"), avp_json_int(a, r->functions));
    avp_json_set(a, d, AVP_S("bytes"), avp_json_int(a, r->bytes));
    for (int k = 0; k < STATUS_NKINDS; k++) {
        avp_json_set(a, d, avp_str(STATUS_KINDS[k]), avp_json_int(a, r->n[k]));
        avp_json_set(a, d, avp_pyfmt(a, "%s_bytes", STATUS_KINDS[k]), avp_json_int(a, r->b[k]));
    }
    avp_json_set(a, d, AVP_S("original_asm"), avp_json_int(a, r->asm_n));
    avp_json_set(a, d, AVP_S("original_asm_bytes"), avp_json_int(a, r->asm_b));
    return d;
}

double status_pc(i64 a, i64 b) { return b ? 100.0 * (double)a / (double)b : 0.0; }

static AvpStr head_slice(AvpStr s, i64 n) { return avp_u8_slice(s, 0, n); }

AvpStrVec status_fmt_table(AvpArena *a, const StatusRows *rows, const StatusRow *total) {
    AvpStrVec lines = {0};
    AvpStr head = avp_pyfmt(a, "%-20s %6s %6s %5s %5s %6s %5s %6s | %9s %9s %8s %8s %9s %9s %6s", "library", "fns",
                            "match", "exact", "prog", "unst", "asm", "fn%", "bytes", "matched", "exact", "progress",
                            "unstarted", "asm", "byte%");
    size_t hl = avp_u8_len(head);
    AvpBuf dash = {.a = a};
    for (size_t i = 0; i < hl; i++) avp_buf_putc(&dash, '-');
    AvpStr rule = avp_buf_str(&dash);
    avp_vec_push(&lines, head);
    avp_vec_push(&lines, rule);
    for (size_t i = 0; i <= rows->n; i++) {
        const StatusRow *r = i < rows->n ? rows->v[i] : total;
        if (r == total) avp_vec_push(&lines, rule);
        AvpStr name = head_slice(avp_str(r->library), 20);
        avp_vec_push(&lines, avp_pyfmt(a, "%-20S %6d %6d %5d %5d %6d %5d %5.1f%% | %9d %9d %8d %8d %9d %9d %5.1f%%", &name,
                                       r->functions, r->n[0], r->n[1], r->n[2], r->n[3], r->asm_n,
                                       status_pc(r->n[0], r->functions), r->bytes, r->b[0], r->b[1], r->b[2], r->b[3], r->asm_b,
                                       status_pc(r->b[0], r->bytes)));
    }
    return lines;
}

/* ---------------------------------------------------------------------------------------------------------------
 * headline
 * ------------------------------------------------------------------------------------------------------------ */

static i64 py_int_str(AvpStr s) {
    i64 v = 0;
    avp_parse_int(s, 10, &v, NULL);
    return v;
}

AvpJson *status_ledger_block(AvpArena *a, const char *root) {
    const char *path = avp_join(a, root, "docs", "progress.md");
    if (!avp_isfile(path)) return NULL;
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TM_UTF8_REPLACE, &text)) shsm_raise_io();
    ShsmReMatch m;
    if (!shsm_re_search(shsm_re("<!-- ledger:begin numbers -->(.*?)<!-- ledger:end numbers -->", SHSM_RE_S), text, 0,
                        AVP_END, &m))
        return NULL;
    const ShsmRe *row = shsm_re("^\\|\\s*(.+?)\\s*\\|\\s*([\\d ]+(?:/\\s*\\d+)?)\\s*\\|\\s*([\\d ]*(?:/\\s*\\d+)?)\\s*\\|", 0);
    const ShsmRe *digits = shsm_re("\\d+", 0);
    AvpJson *out = avp_json_obj(a);
    AvpStrVec lines = avp_str_splitlines(shsm_re_group(&m, 1), false);
    for (size_t i = 0; i < lines.n; i++) {
        ShsmReMatch mm;
        if (!shsm_re_match(row, avp_str_strip(lines.v[i]), 0, AVP_END, &mm)) continue;
        AvpStr label = shsm_re_group(&mm, 1);
        AvpStr g2 = shsm_re_group(&mm, 2), g3 = shsm_re_group(&mm, 3);
        AvpStr joined = avp_pyfmt(a, "%S %S", &g2, &g3);
        ShsmReFindall fa = shsm_re_findall(digits, joined, 0, AVP_END);
        const char *key = NULL;
        size_t keep = 0;
        if (avp_str_contains(label, AVP_S("matched under the rules")))
            key = "rules", keep = 4;
        else if (avp_str_startswith(label, AVP_S("bytes matched")))
            key = "bytes", keep = 4;
        else if (avp_str_startswith(label, AVP_S("`ORIGINAL-ASM`")))
            key = "asm", keep = 2;
        else if (avp_str_startswith(label, AVP_S("linked from source")))
            key = "linked", keep = 2;
        else if (avp_str_contains(label, AVP_S("Matching` units")))
            key = "units", keep = 2;
        if (key) {
            AvpJson *arr = avp_json_arr(a);
            for (size_t k = 0; k < fa.count && k < keep; k++)
                avp_json_arr_push(a, arr, avp_json_int(a, py_int_str(shsm_re_findall_at(&fa, k, 0))));
            avp_json_set(a, out, avp_str(key), arr);
        }
        shsm_re_findall_free(&fa);
    }
    return out;
}

/* ---- the inputs of tools/ledger.py's model (ledger.read_text, load_json, run_checker) ---- */

enum { STATUS_LEDGER_ERROR = 300 };

static AvpStr ledger_read_text(AvpArena *a, const char *path) {
    AvpStr raw;
    if (!avp_read_text(a, path, AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_REPLACE, AVP_NL_EMPTY), &raw)) shsm_raise_io();
    return avp_str_contains(raw, AVP_S("\r\n")) ? avp_str_replace(a, raw, AVP_S("\r\n"), AVP_S("\n"), -1) : raw;
}

static AvpJson *json_or_raise(AvpArena *a, AvpStr text) {
    AvpJsonError e;
    AvpJson *doc = avp_json_parse(a, text, &e);
    if (!doc) {
        AvpStr m = avp_json_error_str(a, &e);
        shsm_raise(SHSM_EXC_VALUE, "%.*s", AVP_SF(m));
    }
    return doc;
}

static AvpJson *ledger_load_json(AvpArena *a, const char *path) {
    AvpStr t;
    if (!avp_read_text(a, path, AVP_TM_UTF8, &t)) shsm_raise_io();
    return json_or_raise(a, t);
}

/* The default runner: the rule checker in-process (tool_check_rules), `check_rules --json --root <root>`. */
static StatusCheckerRun default_runner(void *ctx, AvpArena *a, const char *root) {
    (void)ctx;
    RlRunOpts o = {0};
    o.root = root;
    o.json = true;
    AvpBuf out, err;
    avp_buf_init(&out, a);
    avp_buf_init(&err, a);
    StatusCheckerRun r = {0};
    r.returncode = rl_run(a, &o, &out, &err);
    r.out = avp_buf_str(&out);
    r.err = avp_buf_str(&err);
    return r;
}

static StatusCheckerRunner checker_runner = default_runner;
static void *checker_ctx;

void status_set_checker_runner(StatusCheckerRunner fn, void *ctx) {
    checker_runner = fn ? fn : default_runner;
    checker_ctx = fn ? ctx : NULL;
}

/* ledger.run_checker, read as rl_checker_json_of reads it. LedgerError is STATUS_LEDGER_ERROR. */
static AvpJson *run_checker(AvpArena *a, const char *root, const char *json_path) {
    if (json_path && *json_path) return ledger_load_json(a, json_path);
    StatusCheckerRun p = checker_runner(checker_ctx, a, root);
    const char *why = NULL;
    AvpJson *doc = rl_checker_json_of(a, p.returncode, p.out, p.err, &why);
    if (!doc) avp_throw(STATUS_LEDGER_ERROR, "%s", why);
    return doc;
}

/* type(x).__name__ of a JSON value */
static const char *py_type_name(const AvpJson *v) {
    switch (v ? v->kind : AVP_JSON_NULL) {
    case AVP_JSON_BOOL: return "bool";
    case AVP_JSON_INT: return "int";
    case AVP_JSON_FLOAT: return "float";
    case AVP_JSON_STRING: return "str";
    case AVP_JSON_ARRAY: return "list";
    case AVP_JSON_OBJECT: return "dict";
    default: return "NoneType";
    }
}

/* int(_num(v, 0)) of a report measure */
static i64 measure_int(const AvpJson *meas, const char *key) {
    const AvpJson *v = meas && avp_json_is_obj(meas) ? avp_json_getz(meas, key) : NULL;
    if (!v || v->kind == AVP_JSON_NULL) return 0;
    if (v->kind == AVP_JSON_BOOL) return v->b ? 1 : 0;
    if (v->kind == AVP_JSON_INT) return v->u.i;
    if (v->kind == AVP_JSON_FLOAT) return (i64)v->u.f;
    if (v->kind == AVP_JSON_STRING) {
        i64 i = 0;
        if (avp_parse_int(v->u.str, 10, &i, NULL)) return i;
        double f = 0;
        if (avp_parse_float(v->u.str, &f)) return (i64)f;
    }
    return 0;
}

StatusHeadline *status_headline(AvpArena *a, PickWorld *w, const char *report_path, const char *checker_json,
                                bool no_checker) {
    StatusHeadline *h = AVP_NEW(a, StatusHeadline);
    const char *root = w->repo->root;
    /* ledger_numbers */
    AvpJson *rep = ledger_load_json(a, report_path);
    LedgerCfgVec configure = ledger_parse_configure(a, ledger_read_text(a, w->repo->configure_py));
    const char *exc = avp_join(a, root, "docs", "exceptions.md");
    LedgerExcRows exc_rows = {0};
    if (avp_isfile(exc)) exc_rows = ledger_parse_exceptions(a, ledger_read_text(a, exc));
    AvpJson *checker = NULL;
    if (no_checker) {
        checker = avp_json_obj(a);
        h->note = "without the rule checker: ORIGINAL-ASM and MATCH-PENDING from docs/exceptions.md only";
    } else {
        AvpTry t;
        if (AVP_TRY(&t)) {
            checker = run_checker(a, root, checker_json);
            avp_try_end(&t);
        } else {
            if (t.code != STATUS_LEDGER_ERROR) avp_throw(t.code, "%s", t.msg);
            checker = avp_json_obj(a);
            h->note = avp_cz(a, avp_pyfmt(a, "rule checker unavailable (%s): docs/exceptions.md only", t.msg));
        }
    }
    if (!avp_json_is_obj(rep))
        shsm_raise(SHSM_EXC_TYPE, "'%s' object has no attribute 'get'", py_type_name(rep));
    if (!avp_json_is_obj(checker))
        shsm_raise(SHSM_EXC_TYPE, "'%s' object has no attribute 'get'", py_type_name(checker));
    LedgerModel *model = ledger_build_model(a, rep, &configure, &exc_rows, checker, root, NULL);
    const LedgerNumbers *n = &model->numbers;
    h->problems = model->problems;
    /* headline */
    AvpJson *led = status_ledger_block(a, root);
    h->ledger = led ? led : avp_json_obj(a);
    i64 rules[4] = {n->rules_n, n->total_n, n->rules_b, n->total_b};
    i64 bytes[4] = {n->bytes_n, n->total_n, n->bytes_b, n->total_b};
    memcpy(h->rules, rules, sizeof rules);
    memcpy(h->bytes, bytes, sizeof bytes);
    h->asm_[0] = n->asm_n, h->asm_[1] = n->asm_b;
    h->linked[0] = n->linked_n, h->linked[1] = n->linked_b;
    h->units[0] = n->units_complete, h->units[1] = n->units_total;
    static const char *const keys[7] = {"matched_code",  "total_code",     "matched_functions", "total_functions",
                                        "complete_code", "complete_units", "total_units"};
    for (int i = 0; i < 7; i++) h->progress[i] = measure_int(w->report->measures, keys[i]);
    return h;
}

static AvpJson *int_arr(AvpArena *a, const i64 *v, size_t n) {
    AvpJson *arr = avp_json_arr(a);
    for (size_t i = 0; i < n; i++) avp_json_arr_push(a, arr, avp_json_int(a, v[i]));
    return arr;
}

AvpJson *status_headline_json(AvpArena *a, const StatusHeadline *h) {
    AvpJson *d = avp_json_obj(a), *c = avp_json_obj(a), *p = avp_json_obj(a), *probs = avp_json_arr(a);
    avp_json_set(a, c, AVP_S("rules"), int_arr(a, h->rules, 4));
    avp_json_set(a, c, AVP_S("bytes"), int_arr(a, h->bytes, 4));
    avp_json_set(a, c, AVP_S("asm"), int_arr(a, h->asm_, 2));
    avp_json_set(a, c, AVP_S("linked"), int_arr(a, h->linked, 2));
    avp_json_set(a, c, AVP_S("units"), int_arr(a, h->units, 2));
    static const char *const keys[7] = {"matched_code",  "total_code",     "matched_functions", "total_functions",
                                        "complete_code", "complete_units", "total_units"};
    for (int i = 0; i < 7; i++) avp_json_set(a, p, avp_str(keys[i]), avp_json_int(a, h->progress[i]));
    for (size_t i = 0; i < h->problems.n; i++) avp_json_arr_push(a, probs, avp_json_str(a, h->problems.v[i]));
    avp_json_set(a, d, AVP_S("computed"), c);
    avp_json_set(a, d, AVP_S("ledger"), h->ledger);
    avp_json_set(a, d, AVP_S("configure_progress"), p);
    avp_json_set(a, d, AVP_S("problems"), probs);
    avp_json_set(a, d, AVP_S("note"), h->note ? avp_json_strz(a, h->note) : avp_json_null());
    return d;
}

static AvpStr four(AvpArena *a, const i64 *t) {
    return avp_pyfmt(a, "%d / %d fns, %d / %d bytes (%.2f%%)", t[0], t[1], t[2], t[3], status_pc(t[2], t[3]));
}

/* cmp(key, t): the docs/progress.md suffix */
static AvpStr cmp_ledger(AvpArena *a, const StatusHeadline *h, const char *key, const i64 *t, size_t n) {
    const AvpJson *old = avp_json_getz(h->ledger, key);
    if (!old) return AVP_S("");
    bool same = avp_json_len(old) == n;
    for (size_t i = 0; same && i < n; i++) same = avp_json_i64(avp_json_at(old, i), 0) == t[i];
    if (same) return AVP_S("   [docs/progress.md: same]");
    AvpBuf b = {.a = a};
    avp_buf_putz(&b, "   [docs/progress.md: ");
    for (size_t i = 0; i < avp_json_len(old); i++) {
        if (i) avp_buf_putz(&b, " / ");
        avp_buf_pyfmt(&b, "%d", avp_json_i64(avp_json_at(old, i), 0));
    }
    avp_buf_putz(&b, "]");
    return avp_buf_str(&b);
}

AvpStrVec status_fmt_headline(AvpArena *a, const StatusHeadline *h) {
    AvpStrVec lines = {0};
    AvpStr f = four(a, h->rules), c = cmp_ledger(a, h, "rules", h->rules, 4);
    avp_vec_push(&lines, avp_pyfmt(a, "matched under the rules (headline): %S%S", &f, &c));
    f = four(a, h->bytes), c = cmp_ledger(a, h, "bytes", h->bytes, 4);
    avp_vec_push(&lines, avp_pyfmt(a, "bytes matched (linked, excl. ORIGINAL-ASM): %S%S", &f, &c));
    c = cmp_ledger(a, h, "asm", h->asm_, 2);
    avp_vec_push(&lines, avp_pyfmt(a, "ORIGINAL-ASM (linked, apart): %d fns, %d bytes%S", h->asm_[0], h->asm_[1], &c));
    c = cmp_ledger(a, h, "linked", h->linked, 2);
    avp_vec_push(&lines, avp_pyfmt(a, "linked from source: %d fns, %d bytes; Matching units %d / %d%S", h->linked[0],
                                   h->linked[1], h->units[0], h->units[1], &c));
    const i64 *p = h->progress;
    avp_vec_push(&lines, avp_pyfmt(a,
                                   "report measures (configure.py progress): matched %d / %d bytes (%d / %d fns, "
                                   "unlinked 100%% included), linked %d bytes, %d / %d units",
                                   p[0], p[1], p[2], p[3], p[4], p[5], p[6]));
    if (h->note && *h->note) avp_vec_push(&lines, avp_pyfmt(a, "note: %s", h->note));
    for (size_t i = 0; i < h->problems.n && i < 5; i++)
        avp_vec_push(&lines, avp_pyfmt(a, "ledger problem: %S", &h->problems.v[i]));
    if (h->problems.n > 5)
        avp_vec_push(&lines, avp_pyfmt(a, "... %d more ledger problems (build/tools/ledger.exe progress --check)",
                                       (i64)(h->problems.n - 5)));
    return lines;
}

/* ---------------------------------------------------------------------------------------------------------------
 * --units
 * ------------------------------------------------------------------------------------------------------------ */

typedef struct {
    i64 address;
    AvpStr unit;
    AvpJson *row;
} UnitKey;

static int unit_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const UnitKey *a = x, *b = y;
    if (a->address != b->address) return a->address < b->address ? -1 : 1;
    return avp_str_cmp(a->unit, b->unit);
}

StatusJsonVec status_unit_rows(AvpArena *a, PickWorld *w, const char *lib, AvpMap *pending) {
    AVP_VEC(UnitKey) keys = {0};
    AvpMapIter it = {0};
    while (avp_map_next(w->by_unit, &it)) {
        const char *path = avp_cz(a, it.key);
        const PickFnVec *fl = it.val->p;
        const char *status = pick_world_unit_status(w, path);
        if (status && strcmp(status, "Asm") == 0) continue;
        const char *ulib = pick_world_unit_lib(w, path);
        if (!pick_lib_matches(a, ulib, lib, pick_world_unit_library(w, path))) continue;
        i64 nexact = 0, bytes = 0, exact_bytes = 0, addr = 0;
        bool first = true;
        for (size_t i = 0; i < fl->n; i++) {
            const PickFn *f = fl->v[i];
            bytes += f->size;
            if (f->fuzzy >= 100.0) nexact++, exact_bytes += f->size;
            i64 fa = f->has_address ? f->address : 0;
            if (first || fa < addr) addr = fa;
            first = false;
        }
        AvpJson *flags = avp_json_arr(a);
        const AvpVal *pv = pending ? avp_map_get(pending, it.key) : NULL;
        if (pv && pv->i) avp_json_arr_push(a, flags, avp_json_str(a, avp_pyfmt(a, "MATCH-PENDING x%d", pv->i)));
        if ((!status || strcmp(status, "Matching") != 0) && fl->n && nexact == (i64)fl->n)
            avp_json_arr_push(a, flags, avp_json_strz(a, "layout unverified"));
        AvpJson *d = avp_json_obj(a);
        avp_json_set(a, d, AVP_S("unit"), avp_json_strz(a, path));
        avp_json_set(a, d, AVP_S("status"), avp_json_strz(a, status));
        avp_json_set(a, d, AVP_S("library"), avp_json_strz(a, ulib));
        avp_json_set(a, d, AVP_S("functions"), avp_json_int(a, (i64)fl->n));
        avp_json_set(a, d, AVP_S("exact"), avp_json_int(a, nexact));
        avp_json_set(a, d, AVP_S("bytes"), avp_json_int(a, bytes));
        avp_json_set(a, d, AVP_S("exact_bytes"), avp_json_int(a, exact_bytes));
        avp_json_set(a, d, AVP_S("flags"), flags);
        avp_json_set(a, d, AVP_S("address"), avp_json_int(a, addr));
        UnitKey k = {addr ? addr : 0xFFFFFFFF, avp_str(path), d};
        avp_vec_push(&keys, k);
    }
    avp_sort(keys.v, keys.n, sizeof keys.v[0], unit_cmp, NULL, false);
    StatusJsonVec out = {0};
    for (size_t i = 0; i < keys.n; i++) avp_vec_push(&out, keys.v[i].row);
    avp_vec_free(&keys);
    return out;
}

AvpStr status_fmt_unit(AvpArena *a, const AvpJson *r) {
    AvpStr unit = head_slice(avp_json_get_str(r, "unit", AVP_S("")), 48);
    AvpStr status = avp_json_get_str(r, "status", AVP_S(""));
    AvpStr lib = head_slice(avp_json_get_str(r, "library", AVP_S("")), 14);
    const AvpJson *flags = avp_json_getz(r, "flags");
    AvpBuf b = {.a = a};
    for (size_t i = 0; i < avp_json_len(flags); i++) {
        if (i) avp_buf_putz(&b, ", ");
        avp_buf_put(&b, avp_json_str_or(avp_json_at(flags, i), AVP_S("")));
    }
    AvpStr fl = avp_buf_str(&b);
    return avp_pyfmt(a, "%-48S %-11S %-14S %4d/%-4d fns %7d/%-7d bytes  %S", &unit, &status, &lib,
                     avp_json_get_i64(r, "exact", 0), avp_json_get_i64(r, "functions", 0),
                     avp_json_get_i64(r, "exact_bytes", 0), avp_json_get_i64(r, "bytes", 0), &fl);
}

AvpMap *status_pending_by_unit(AvpArena *a, PickWorld *w) {
    AvpMap *counts = avp_map_new();
    PickItems recs = pick_pending_records(a, w, false);
    for (size_t i = 0; i < recs.n; i++) {
        bool created;
        avp_map_put(counts, avp_json_get_str(recs.v[i], "unit", AVP_S("")), &created)->i += 1;
    }
    return counts;
}

/* ---------------------------------------------------------------------------------------------------------------
 * --since
 * ------------------------------------------------------------------------------------------------------------ */

/* read_report(path): Report(json.load(f)) calls data.get first, an AttributeError for a document that is not an
 * object (shsm_read_report accepts one). */
static ShsmReport *read_report(AvpArena *a, const char *path) {
    ShsmReport *rep = shsm_read_report(path);
    AvpStr t;
    if (avp_read_text(a, path, AVP_TM_UTF8, &t) && !avp_str_startswith(avp_str_strip(t), AVP_S("{"))) {
        const AvpJson *doc = avp_json_parse(a, t, NULL);
        if (doc && !avp_json_is_obj(doc))
            shsm_raise(SHSM_EXC_TYPE, "'%s' object has no attribute 'get'", py_type_name(doc));
    }
    return rep;
}

ShsmReport *status_load_old_report(AvpArena *a, PickWorld *w, const char *ref, const char **name) {
    if (avp_isfile(ref)) {
        *name = ref;
        return read_report(a, ref);
    }
    const char *rel = shsm_repo_rel(a, w->repo, w->repo->baseline_report_json);
    const char *spec = avp_cz(a, avp_pyfmt(a, "%s:%s", ref, rel));
    const char *argv[] = {"git", "show", spec};
    ShsmRun r = shsm_run(a, argv, 3, w->repo->root, 0);
    if (!shsm_run_ok(&r)) shsm_raise(SHSM_EXC_USAGE, "%s is neither a report file nor a git rev with %s", ref, rel);
    char *tmp = NULL;
    AvpFile *f = avp_mkstemp(a, "tmp", ".json", NULL, &tmp);
    if (!f) shsm_raise_io();
    bool ok = avp_fwrite(f, r.out.p, r.out.n);
    ok = avp_fclose(f) && ok;
    ShsmReport *rep = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        if (!ok) shsm_raise_io();
        rep = read_report(a, tmp);
        avp_try_end(&t);
    } else {
        avp_remove(tmp);
        avp_throw(t.code, "%s", t.msg);
    }
    avp_remove(tmp);
    *name = spec;
    return rep;
}

static AvpJson *opt_float(AvpArena *a, bool has, double v) { return has ? avp_json_float(a, v) : avp_json_null(); }

/* the unique function names of a unit report in order (UnitReport.functions keys) */
static bool first_entry(const ShsmUnitReport *u, size_t i) {
    const AvpVal *v = avp_map_get(u->first_by_name, avp_str(u->functions[i].name));
    return v && (size_t)v->i == i;
}

static const ShsmUnitReport *unit_by_name(const ShsmReport *r, AvpStr name) {
    const AvpVal *v = avp_map_get(r->index, name);
    return v ? r->units[v->i] : NULL;
}

AvpJson *status_since(AvpArena *a, PickWorld *w, const ShsmReport *old, const char *lib) {
    const ShsmReport *new = w->report;
    AvpMap *lib_of = avp_map_new();
    for (size_t i = 0; i < w->fns.n; i++) {
        const PickFn *f = w->fns.v[i];
        bool created;
        avp_map_put(lib_of, avp_pyfmt(a, "%s\n%s", f->unit, f->name), &created)->p = (void *)f->library;
    }
    static const char *const KEYS[8] = {"gained",  "lost",    "improved",   "regressed",
                                        "added",   "removed", "flipped_on", "flipped_off"};
    AvpJson *res = avp_json_obj(a);
    AvpJson *L[8];
    for (int i = 0; i < 8; i++) avp_json_set(a, res, avp_str(KEYS[i]), L[i] = avp_json_arr(a));
    /* compare_reports(old, new) */
    AvpStrVec names = {0};
    for (size_t i = 0; i < old->nunits; i++) avp_vec_push(&names, avp_str(old->units[i]->name));
    for (size_t i = 0; i < new->nunits; i++)
        if (!avp_map_has(old->index, avp_str(new->units[i]->name))) avp_vec_push(&names, avp_str(new->units[i]->name));
    for (size_t ui = 0; ui < names.n; ui++) {
        const ShsmUnitReport *ou = unit_by_name(old, names.v[ui]), *nu = unit_by_name(new, names.v[ui]);
        AvpStrVec fns = {0};
        if (ou)
            for (size_t i = 0; i < ou->nfunctions; i++)
                if (first_entry(ou, i)) avp_vec_push(&fns, avp_str(ou->functions[i].name));
        if (nu)
            for (size_t i = 0; i < nu->nfunctions; i++)
                if (first_entry(nu, i) && !(ou && avp_map_has(ou->first_by_name, avp_str(nu->functions[i].name))))
                    avp_vec_push(&fns, avp_str(nu->functions[i].name));
        const char *path = shsm_unit_path(a, avp_cz(a, names.v[ui]));
        for (size_t fi = 0; fi < fns.n; fi++) {
            const char *fn = avp_cz(a, fns.v[fi]);
            const ShsmFunctionReport *fa = ou ? shsm_unit_report_function(ou, fn) : NULL;
            const ShsmFunctionReport *fb = nu ? shsm_unit_report_function(nu, fn) : NULL;
            double x = fa ? fa->fuzzy : 0, y = fb ? fb->fuzzy : 0;
            if (fa && fb && x == y) continue;
            if (truthy(lib)) {
                const AvpVal *lv = avp_map_get(lib_of, avp_pyfmt(a, "%s\n%s", path, fn));
                if (!pick_lib_matches(a, lv ? lv->p : NULL, lib, pick_world_unit_library(w, path))) continue;
            }
            AvpJson *item = avp_json_obj(a);
            avp_json_set(a, item, AVP_S("unit"), avp_json_strz(a, path));
            avp_json_set(a, item, AVP_S("symbol"), avp_json_strz(a, fn));
            avp_json_set(a, item, AVP_S("old"), opt_float(a, fa != NULL, x));
            avp_json_set(a, item, AVP_S("new"), opt_float(a, fb != NULL, y));
            if (!fa) {
                avp_json_arr_push(a, L[4], item);
                if (fb && y >= 100.0) avp_json_arr_push(a, L[0], item);
            } else if (!fb) {
                avp_json_arr_push(a, L[5], item);
                if (x >= 100.0) avp_json_arr_push(a, L[1], item);
            } else if (y >= 100.0 && 100.0 > x) {
                avp_json_arr_push(a, L[0], item);
            } else if (x >= 100.0 && 100.0 > y) {
                avp_json_arr_push(a, L[1], item);
            } else if (y > x) {
                avp_json_arr_push(a, L[2], item);
            } else {
                avp_json_arr_push(a, L[3], item);
            }
        }
        avp_vec_free(&fns);
    }
    /* flipped units */
    AvpStrVec on = {0}, off = {0};
    for (size_t ui = 0; ui < names.n; ui++) {
        const ShsmUnitReport *ou = unit_by_name(old, names.v[ui]), *nu = unit_by_name(new, names.v[ui]);
        bool oc = ou && ou->complete, nc = nu && nu->complete;
        if (oc != nc) avp_vec_push(nc ? &on : &off, avp_str(shsm_unit_path(a, avp_cz(a, names.v[ui]))));
    }
    avp_sort_strs(on.v, on.n);
    avp_sort_strs(off.v, off.n);
    for (size_t i = 0; i < on.n; i++) avp_json_arr_push(a, L[6], avp_json_str(a, on.v[i]));
    for (size_t i = 0; i < off.n; i++) avp_json_arr_push(a, L[7], avp_json_str(a, off.v[i]));
    avp_vec_free(&names);
    avp_map_free(lib_of);
    return res;
}

static AvpStr pct_of(AvpArena *a, const AvpJson *v) {
    if (!v || v->kind == AVP_JSON_NULL) return AVP_S("-");
    return avp_str(pick_pct(a, true, avp_json_f64(v, 0)));
}

AvpStrVec status_fmt_since(AvpArena *a, const AvpJson *res, const char *old_name) {
    AvpStrVec lines = {0};
#define NL(k) (i64) avp_json_len(avp_json_getz(res, k))
    avp_vec_push(&lines, avp_pyfmt(a,
                                   "since %s: %d gained, %d lost, %d improved, %d regressed, %d added, %d removed; "
                                   "units flipped to Matching %d, away %d",
                                   old_name, NL("gained"), NL("lost"), NL("improved"), NL("regressed"), NL("added"),
                                   NL("removed"), NL("flipped_on"), NL("flipped_off")));
#undef NL
    const AvpJson *on = avp_json_getz(res, "flipped_on"), *off = avp_json_getz(res, "flipped_off");
    for (size_t i = 0; i < avp_json_len(on); i++) {
        AvpStr u = avp_json_str_or(avp_json_at(on, i), AVP_S(""));
        avp_vec_push(&lines, avp_pyfmt(a, "  flipped  %S", &u));
    }
    for (size_t i = 0; i < avp_json_len(off); i++) {
        AvpStr u = avp_json_str_or(avp_json_at(off, i), AVP_S(""));
        avp_vec_push(&lines, avp_pyfmt(a, "  UNFLIPPED %S", &u));
    }
    static const char *const keys[4][2] = {
        {"lost", "LOST"}, {"regressed", "regressed"}, {"gained", "gained"}, {"improved", "improved"}};
    for (int k = 0; k < 4; k++) {
        const AvpJson *list = avp_json_getz(res, keys[k][0]);
        for (size_t i = 0; i < avp_json_len(list); i++) {
            const AvpJson *it = avp_json_at(list, i);
            AvpStr unit = head_slice(avp_json_get_str(it, "unit", AVP_S("")), 40);
            AvpStr sym = head_slice(avp_json_get_str(it, "symbol", AVP_S("")), 48);
            AvpStr o = pct_of(a, avp_json_getz(it, "old")), n = pct_of(a, avp_json_getz(it, "new"));
            avp_vec_push(&lines, avp_pyfmt(a, "  %-9s %-40S %-48S %6S -> %S", keys[k][1], &unit, &sym, &o, &n));
        }
    }
    return lines;
}

/* ---------------------------------------------------------------------------------------------------------------
 * --svg
 * ------------------------------------------------------------------------------------------------------------ */

/* Every bar colour keeps 3:1 against the white panel (WCAG non-text contrast) and every text colour 4.5:1;
 * adjacent segments are told apart by a white gap, not by their hue alone. */
const char *const STATUS_SVG_COLORS[STATUS_NKINDS] = {"#2b8a3e", "#5c940d", "#d9480f", "#868e96"};
static const char *const SVG_LABELS[STATUS_NKINDS] = {"matched (linked)", "exact, unlinked", "in progress",
                                                      "unstarted"};
#define SVG_TEXT "#212529"
#define SVG_MUTED "#495057"
#define SVG_NOTE "#6c757d"
const char *const STATUS_SVG_TEXT_COLORS[3] = {SVG_TEXT, SVG_MUTED, SVG_NOTE};
#define SVG_CHAR_EM 0.62 /* a generous average glyph advance (sans-serif), for laying text out without fonts */

double status_text_w(AvpStr text, double size, bool bold) {
    return (double)avp_u8_len(text) * size * SVG_CHAR_EM * (bold ? 1.06 : 1.0);
}

AvpStr status_human(AvpArena *a, i64 n) {
    if (n >= (i64)1 << 20) return avp_pyfmt(a, "%.1f MiB", (double)n / (double)(1 << 20));
    if (n >= (i64)1 << 10) return avp_pyfmt(a, "%.0f KiB", (double)n / 1024.0);
    return avp_pyfmt(a, "%d B", n);
}

/* xml.sax.saxutils.escape */
static AvpStr esc(AvpArena *a, AvpStr s) {
    AvpBuf b = {.a = a};
    for (size_t i = 0; i < s.n; i++) {
        char c = s.p[i];
        if (c == '&')
            avp_buf_putz(&b, "&amp;");
        else if (c == '<')
            avp_buf_putz(&b, "&lt;");
        else if (c == '>')
            avp_buf_putz(&b, "&gt;");
        else
            avp_buf_putc(&b, c);
    }
    return avp_buf_str(&b);
}

AvpStr status_render_svg(AvpArena *a, const StatusRows *rows, const StatusRow *total, const char *headline_text) {
    static const char title[] = "Progress by library (code bytes)";
    const i64 label_w = 150, bar_w = 520, pad = 16, row_h = 22;
    const i64 label_chars = (i64)(150.0 / (12 * SVG_CHAR_EM)) - 1;
    AvpStr head = headline_text ? avp_str(headline_text)
                                : avp_pyfmt(a, "%.2f%% of code bytes matched (%d / %d bytes)",
                                            status_pc(total->b[0], total->bytes), total->b[0], total->bytes);
    AvpStr big, detail;
    if (!avp_str_partition(head, AVP_S(" ("), &big, &detail)) big = head, detail = AVP_S("");
    if (avp_str_endswith(detail, AVP_S(")"))) detail.n -= 1;
    const i64 top = 104;
    i64 width = pad * 2 + label_w + bar_w + 120;
    i64 height = top + row_h * (i64)(rows->n + 1) + 8 + 40;
    AvpBuf o = {.a = a};
    avp_buf_pyfmt(&o,
                  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\" "
                  "font-family=\"system-ui, -apple-system, Segoe UI, Helvetica, Arial, sans-serif\" font-size=\"12\">",
                  width, height, width, height);
    AvpStr t = esc(a, avp_str(title));
    avp_buf_pyfmt(&o, "\n<title>%S</title>", &t);
    avp_buf_pyfmt(&o, "\n<rect x=\"0.5\" y=\"0.5\" width=\"%d\" height=\"%d\" rx=\"8\" fill=\"#ffffff\" stroke=\"#ced4da\"/>",
                  width - 1, height - 1);
    AvpStr e = esc(a, big);
    avp_buf_pyfmt(&o, "\n<text x=\"%d\" y=\"32\" font-size=\"20\" font-weight=\"700\" fill=\"%s\">%S</text>", pad,
                  SVG_TEXT, &e);
    if (detail.n) {
        e = esc(a, detail);
        avp_buf_pyfmt(&o, "\n<text x=\"%d\" y=\"52\" fill=\"%s\">%S</text>", pad, SVG_MUTED, &e);
    }
    avp_buf_pyfmt(&o, "\n<text x=\"%d\" y=\"74\" fill=\"%s\" font-weight=\"700\">%S</text>", pad, SVG_MUTED, &t);
    i64 x = pad;
    for (int k = 0; k < STATUS_NKINDS; k++) {
        avp_buf_pyfmt(&o, "\n<rect x=\"%d\" y=\"84\" width=\"10\" height=\"10\" fill=\"%s\"/>", x, STATUS_SVG_COLORS[k]);
        avp_buf_pyfmt(&o, "\n<text x=\"%d\" y=\"93\" fill=\"%s\" font-size=\"11\">%s</text>", x + 14, SVG_MUTED,
                      SVG_LABELS[k]);
        x += 14 + (i64)status_text_w(avp_str(SVG_LABELS[k]), 11, false) + 16;
    }
    for (size_t i = 0; i <= rows->n; i++) {
        const StatusRow *r = i < rows->n ? rows->v[i] : total;
        bool is_total = r == total;
        i64 y = top + (i64)i * row_h + (is_total ? 8 : 0);
        const char *bold = is_total ? " font-weight=\"700\"" : "";
        AvpStr lib = avp_str(r->library);
        AvpStr name = lib;
        if ((i64)avp_u8_len(lib) > label_chars) {
            AvpBuf nb = {.a = a};
            avp_buf_put(&nb, avp_u8_slice(lib, 0, label_chars - 1));
            avp_buf_putcp(&nb, 0x2026);
            name = avp_buf_str(&nb);
        }
        e = esc(a, name);
        avp_buf_pyfmt(&o, "\n<text x=\"%d\" y=\"%d\" fill=\"%s\"%s>%S</text>", pad, y + 14, SVG_TEXT, bold, &e);
        double bx = (double)(pad + label_w);
        avp_buf_pyfmt(&o, "\n<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"16\" fill=\"#f1f3f5\"/>", (i64)bx, y + 2,
                      bar_w);
        AvpStr elib = esc(a, lib);
        for (int k = 0; k < STATUS_NKINDS; k++) {
            double wd = r->bytes ? (double)(bar_w * r->b[k]) / (double)r->bytes : 0;
            if (wd > 0) {
                avp_buf_pyfmt(&o,
                              "\n<rect x=\"%.2f\" y=\"%d\" width=\"%.2f\" height=\"16\" fill=\"%s\" stroke=\"#ffffff\" "
                              "stroke-width=\"1\"><title>%S %s: %d bytes</title></rect>",
                              bx, y + 2, wd, STATUS_SVG_COLORS[k], &elib, SVG_LABELS[k], r->b[k]);
                bx += wd;
            }
        }
        AvpStr hm = status_human(a, r->bytes);
        avp_buf_pyfmt(&o, "\n<text x=\"%d\" y=\"%d\" fill=\"%s\"%s>%.1f%%  %S</text>", pad + label_w + bar_w + 8, y + 14,
                      SVG_TEXT, bold, status_pc(r->b[0], r->bytes), &hm);
    }
    avp_buf_pyfmt(&o,
                  "\n<text x=\"%d\" y=\"%d\" fill=\"%s\" font-size=\"10\">bars: share of each library's code bytes; "
                  "libraries by address (config/libraries.tsv)</text>",
                  pad, height - 14, SVG_NOTE);
    avp_buf_putz(&o, "\n</svg>\n");
    return avp_buf_str(&o);
}
