/* ledger: the commands and the argument parser (load_inputs, cmd_progress, cmd_exceptions_check, main of
 * tools/ledger.py). Errors: a LedgerError is "ledger: error: <msg>" and exit 2; an exception the Python did not
 * catch (a missing or malformed input file) is its traceback's last line and exit 1 (DEVIATIONS.md D-ledger-3). */
#include "ledger.h"
#include "rules_cli.h"
#include "shsm_cli.h"
#include "shsm_repo.h"
#include <string.h>

#define DEFAULT_REPORT "build/R5WEA4/report.json"
#define BASELINE_REPORT "config/R5WEA4/baseline_report.json"
#define SYMBOLS "config/R5WEA4/symbols.txt"
#define DESCRIPTION "Regenerate the generated parts of docs/progress.md and check docs/exceptions.md."

enum { LEDGER_ERROR = 200, PY_ERROR = 201 };

typedef struct {
    const char *cmd;
    const char *root, *report, *checker_json, *ledger, *out, *exceptions; /* NULL: None (or "") */
    bool check, diff, add_markers, force, strict;
    i64 diff_lines;
} Args;

typedef struct {
    AvpArena *a;
    AvpBuf *out, *err;
} Io;

/* An exception the Python leaves uncaught: "<Class>: <str(e)>" (the traceback's last line). */
AVP_NORETURN static void py_io_error(AvpArena *a) {
    AvpStr s = avp_io_err_str(a);
    avp_throw(PY_ERROR, "%s: %.*s", avp_io_err_class(), AVP_SF(s));
}

static AvpStr read_raw(AvpArena *a, const char *path) {
    AvpStr t;
    if (!avp_read_text(a, path, AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_REPLACE, AVP_NL_EMPTY), &t)) py_io_error(a);
    return t;
}

/* read_text: (text with \n endings, eol) */
static AvpStr read_text(AvpArena *a, const char *path, const char **eol) {
    AvpStr raw = read_raw(a, path);
    bool crlf = avp_str_contains(raw, AVP_S("\r\n"));
    if (eol) *eol = crlf ? "\r\n" : "\n";
    return crlf ? avp_str_replace(a, raw, AVP_S("\r\n"), AVP_S("\n"), -1) : raw;
}

static AvpJson *json_text(AvpArena *a, AvpStr text) {
    AvpJsonError e;
    AvpJson *doc = avp_json_parse(a, text, &e);
    if (!doc) {
        AvpStr m = avp_json_error_str(a, &e);
        const char *cls = e.kind == AVP_JSON_E_DECODE      ? "json.decoder.JSONDecodeError"
                          : e.kind == AVP_JSON_E_RECURSION ? "RecursionError"
                          : e.kind == AVP_JSON_E_UNICODE   ? "UnicodeDecodeError"
                                                           : "ValueError";
        avp_throw(PY_ERROR, "%s: %.*s", cls, AVP_SF(m));
    }
    return doc;
}

static AvpJson *load_json(AvpArena *a, const char *path) {
    AvpStr t;
    if (!avp_read_text(a, path, AVP_TM_UTF8, &t)) py_io_error(a);
    return json_text(a, t);
}

/* run_checker: the rule checker, run in-process (tool_check_rules). */
static AvpJson *run_checker(AvpArena *a, const char *root, const char *json_path) {
    if (json_path) return load_json(a, json_path);
    const char *why = NULL;
    AvpJson *doc = rl_checker_json(a, root, NULL, &why);
    if (!doc) avp_throw(LEDGER_ERROR, "%s", why);
    return doc;
}

typedef struct {
    const char *root, *report_path;
    AvpJson *report;
    LedgerCfgVec configure;
    LedgerExcRows exc_rows;
    AvpJson *checker;
    AvpMap *symbols; /* NULL: None */
} Inputs;

static const char *pick_report(AvpArena *a, const char *root, const char *arg) {
    if (arg) return arg;
    const char *cands[2] = {DEFAULT_REPORT, BASELINE_REPORT};
    for (int i = 0; i < 2; i++) {
        char *p = avp_path_join(a, root, cands[i]);
        if (avp_isfile(p)) return p;
    }
    avp_throw(LEDGER_ERROR, "no objdiff report (looked for %s and %s)", DEFAULT_REPORT, BASELINE_REPORT);
}

static Inputs load_inputs(AvpArena *a, const Args *args) {
    Inputs in = {0};
    in.root = avp_path_abspath(a, args->root);
    in.report_path = pick_report(a, in.root, args->report);
    in.report = load_json(a, in.report_path);
    char *cfg_path = avp_path_join(a, in.root, "configure.py");
    if (!avp_isfile(cfg_path)) avp_throw(LEDGER_ERROR, "configure.py not found under %s", in.root);
    in.configure = ledger_parse_configure(a, read_text(a, cfg_path, NULL));
    const char *exc_path = args->exceptions ? args->exceptions : avp_join(a, in.root, "docs", "exceptions.md");
    AvpStr exc_text = avp_isfile(exc_path) ? read_text(a, exc_path, NULL) : AVP_S("");
    in.exc_rows = ledger_parse_exceptions(a, exc_text);
    in.checker = run_checker(a, in.root, args->checker_json);
    char *sym_path = avp_path_join(a, in.root, SYMBOLS);
    if (avp_isfile(sym_path)) in.symbols = ledger_parse_symbols(a, read_text(a, sym_path, NULL));
    return in;
}

static char *relpath(AvpArena *a, const char *p, const char *start) {
    char *out = NULL;
    if (!avp_path_relpath(a, p, start, &out)) py_io_error(a);
    return out;
}

static int cmd_progress(Io *io, const Args *args) {
    AvpArena *a = io->a;
    AvpBuf *err = io->err;
    Inputs in = load_inputs(a, args);
    LedgerModel *model = ledger_build_model(a, in.report, &in.configure, &in.exc_rows, in.checker, in.root, in.symbols);
    const char *ledger = args->ledger ? args->ledger : avp_join(a, in.root, "docs", "progress.md");
    const char *eol;
    AvpStr text = read_text(a, ledger, &eol);
    AvpStrVec warnings = {0};
    const char *how[4] = {0};
    i64 old_count = 0, new_count = 0;
    AvpStr fresh = ledger_generate(a, text, model, &warnings, args->add_markers, how, &old_count, &new_count);
    avp_buf_pyfmt(err, "ledger: report %s\n", relpath(a, in.report_path, in.root));
    const LedgerNumbers *n = &model->numbers;
    avp_buf_pyfmt(err,
                  "ledger: matched under the rules %d/%d functions, %d/%d bytes; bytes matched %d, %d; ORIGINAL-ASM %d, "
                  "%d; linked %d, %d; Matching units %d/%d\n",
                  n->rules_n, n->total_n, n->rules_b, n->total_b, n->bytes_n, n->bytes_b, n->asm_n, n->asm_b,
                  n->linked_n, n->linked_b, n->units_complete, n->units_total);
    /* sorted(how.items()): game, gross, numbers, status */
    avp_buf_pyfmt(err, "ledger: rows %d -> %d; blocks game=%s, gross=%s, numbers=%s, status=%s\n", old_count,
                  new_count, how[1], how[3], how[2], how[0]);
    static const char *const names[4] = {"status", "game", "numbers", "gross"};
    for (int i = 0; i < 4; i++)
        if (!how[i])
            avp_buf_pyfmt(err, "ledger: warning: block '%s' not found in %s (no markers, no recognisable table)\n",
                          names[i], ledger);
    for (size_t i = 0; i < warnings.n; i++) avp_buf_pyfmt(err, "ledger: warning: %S\n", &warnings.v[i]);
    for (size_t i = 0; i < model->problems.n; i++) avp_buf_pyfmt(err, "ledger: PROBLEM: %S\n", &model->problems.v[i]);
    if (model->problems.n && !args->force) {
        avp_buf_putz(err, "ledger: refusing to write with problems (--force overrides)\n");
        return 1;
    }
    if (args->check) {
        if (avp_str_eq(fresh, text)) {
            avp_buf_pyfmt(err, "ledger: %s is up to date\n", ledger);
            return 0;
        }
        avp_buf_pyfmt(err, "ledger: %s differs from the generated ledger\n", ledger);
        if (args->diff) {
            AvpStrVec x = avp_str_split(text, AVP_S("\n"), -1), y = avp_str_split(fresh, AVP_S("\n"), -1);
            AvpUdiff o = AVP_UDIFF_DEFAULT;
            o.fromfile = AVP_S("disk");
            o.tofile = AVP_S("generated");
            o.n = 0;
            o.lineterm = AVP_S("");
            AvpStrVec d = avp_unified_diff_lines(a, x.v, x.n, y.v, y.n, &o);
            i64 len = (i64)d.n, k = args->diff_lines;
            i64 upto = k >= 0 ? (k < len ? k : len) : (len + k > 0 ? len + k : 0); /* d[:k] */
            for (i64 i = 0; i < upto; i++) {
                avp_buf_put(io->out, avp_u8_slice(d.v[i], 0, 260));
                avp_buf_putc(io->out, '\n');
            }
            if (len > k) avp_buf_pyfmt(io->out, "... %d more diff lines\n", len - k);
        }
        return 1;
    }
    if (args->out && strcmp(args->out, "-") == 0) {
        avp_buf_put(io->out, fresh);
        return 0;
    }
    const char *out = args->out ? args->out : ledger;
    if (args->out) {
        AvpStr dir = avp_path_dirname(avp_path_abspath(a, out));
        if (!avp_makedirs(avp_cz(a, dir), true)) py_io_error(a);
    }
    AvpStr body = strcmp(eol, "\n") == 0 ? fresh : avp_str_replace(a, fresh, AVP_S("\n"), avp_str(eol), -1);
    if (!avp_write_text(out, body, AVP_TM_UTF8_NL_EMPTY)) py_io_error(a);
    avp_buf_pyfmt(err, "ledger: wrote %s\n", out);
    return 0;
}

/* ---- exceptions-check -------------------------------------------------------------------------------------- */

typedef struct {
    AvpArena *a;
    AvpStr exc_rel;
    AvpStrVec findings, notes;
} Check;

static void add(Check *c, i64 line, bool note, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    AvpStr msg = avp_pyvfmt(c->a, fmt, ap);
    va_end(ap);
    avp_vec_push(note ? &c->notes : &c->findings,
                 avp_pyfmt(c->a, "%S:%d: %s%S", &c->exc_rel, line, note ? "note: " : "", &msg));
}

static AvpStr pair(AvpArena *a, AvpStr u, AvpStr f) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_put(&b, u);
    avp_buf_putc(&b, '\0');
    avp_buf_put(&b, f);
    return avp_buf_str(&b);
}

static AvpStr jstr(const AvpJson *o, const char *key) {
    const AvpJson *v = avp_json_is_obj(o) ? avp_json_getz(o, key) : NULL;
    return avp_json_is_str(v) ? v->u.str : AVP_S("");
}
static const AvpJson *jarr(const AvpJson *o, const char *key) {
    const AvpJson *v = avp_json_is_obj(o) ? avp_json_getz(o, key) : NULL;
    return avp_json_is_arr(v) ? v : NULL;
}
static size_t jlen(const AvpJson *arr) { return arr ? avp_json_len(arr) : 0; }

/* source_words: {relative path: set of identifiers} for src/ and include/, plus their union. */
static AvpMap *source_words(AvpArena *a, const char *root, AvpSet *all) {
    AvpMap *words = avp_map_new();
    static const char *const tops[2] = {"src", "include"};
    static const char *const exts[] = {".c", ".cc", ".cp", ".cpp", ".h", ".hpp", ".s"};
    for (int t = 0; t < 2; t++) {
        AvpWalk *w = avp_walk_open(avp_path_join(a, root, tops[t]), true);
        if (!w) continue;
        AvpWalkItem item = {0};
        while (avp_walk_next(w, &item)) {
            char *dp = avp_cz(a, item.dirpath);
            for (size_t i = 0; i < item.files.n; i++) {
                AvpStr low = avp_str_lower(a, item.files.v[i]);
                bool ok = false;
                for (size_t k = 0; k < sizeof exts / sizeof exts[0] && !ok; k++)
                    ok = avp_str_endswith(low, avp_str(exts[k]));
                if (!ok) continue;
                char *full = avp_path_join(a, dp, avp_cz(a, item.files.v[i]));
                AvpStr rel = avp_str_replace(a, avp_str(relpath(a, full, root)), AVP_S("\\"), AVP_S("/"), -1);
                AvpStr text;
                if (!avp_read_text(a, full, AVP_TM_UTF8_REPLACE, &text)) py_io_error(a);
                AvpSet *s = avp_set_new();
                ledger_source_words(text, s);
                bool c;
                avp_map_put(words, rel, &c)->p = s;
                size_t pos = 0;
                AvpStr m;
                while (avp_set_next(s, &pos, &m)) avp_set_add(all, m);
            }
        }
        avp_walk_close(w);
    }
    return words;
}

typedef struct {
    AvpArena *a;
    LedgerModel *model;
    AvpMap *cfg;   /* unit -> LedgerCfg* (the last entry) */
    AvpMap *words; /* rel -> AvpSet* */
} Ctx;

/* unit_fn_names: name -> LedgerFn* (names[f.name] = f; names.setdefault(f.alias, f)) */
static AvpMap *unit_fn_names(Ctx *c, AvpStr unit) {
    AvpMap *names = avp_map_new();
    AvpVal *uv = avp_map_get(c->model->units, unit);
    if (uv) {
        const LedgerUnit *u = (const LedgerUnit *)uv->p;
        for (size_t i = 0; i < u->fns.n; i++) {
            bool cr;
            avp_map_put(names, u->fns.v[i]->name, &cr)->p = u->fns.v[i];
            AvpVal *v = avp_map_put(names, u->fns.v[i]->alias, &cr);
            if (cr) v->p = u->fns.v[i];
        }
    }
    return names;
}

static bool exists_in_unit(Ctx *c, AvpStr unit, AvpStr fn) {
    AvpMap *names = unit_fn_names(c, unit);
    bool found = avp_map_has(names, fn);
    avp_map_free(names);
    if (found) return true;
    AvpVal *cv = avp_map_get(c->cfg, unit);
    if (cv) {
        AvpStr rel = avp_str_concat(c->a, AVP_S("src/"), ((const LedgerCfg *)cv->p)->path);
        AvpVal *wv = avp_map_get(c->words, rel);
        if (wv && avp_set_has((AvpSet *)wv->p, fn)) return true;
    }
    return false;
}

static AvpStr last_component(AvpStr fn) {
    i64 k = avp_str_rfind(fn, AVP_S("::"), 0, (i64)fn.n);
    return k >= 0 ? avp_str_slice(fn, k + 2, (i64)fn.n) : fn;
}

static bool vec_has(const AvpStrVec *v, AvpStr s) {
    for (size_t i = 0; i < v->n; i++)
        if (avp_str_eq(v->v[i], s)) return true;
    return false;
}

static int cmd_exceptions_check(Io *io, const Args *args) {
    AvpArena *a = io->a;
    Inputs in = load_inputs(a, args);
    Check ck = {a, AVP_S("docs/exceptions.md"), {0}, {0}};
    if (args->exceptions)
        ck.exc_rel = avp_str_replace(a, avp_str(relpath(a, args->exceptions, in.root)), AVP_S("\\"), AVP_S("/"), -1);
    LedgerModel *model = ledger_build_model(a, in.report, &in.configure, &in.exc_rows, in.checker, in.root, NULL);
    Ctx c = {a, model, avp_map_new(), NULL};
    for (size_t i = 0; i < in.configure.n; i++) {
        bool cr;
        avp_map_put(c.cfg, in.configure.v[i].unit, &cr)->p = &in.configure.v[i];
    }
    AvpSet *all_words = avp_set_new();
    c.words = source_words(a, in.root, all_words);
    AvpSet *missing = avp_set_new();
    i64 line_pend = 1, line_asm = 1;
    bool have_pend = false, have_asm = false;
    for (size_t i = 0; i < in.exc_rows.n; i++) {
        const LedgerExcRow *r = in.exc_rows.v[i];
        if (!have_pend && strcmp(r->table, "MATCH-PENDING") == 0) have_pend = true, line_pend = r->line;
        if (!have_asm && strcmp(r->table, "ORIGINAL-ASM") == 0) have_asm = true, line_asm = r->line;
    }
#define SYM_OR_WORD(fn)                                                                        \
    ((in.symbols && avp_map_has(in.symbols, (fn))) || avp_set_has(all_words, (fn)) ||          \
     avp_set_has(all_words, last_component(fn)))

    for (size_t i = 0; i < in.exc_rows.n; i++) {
        const LedgerExcRow *r = in.exc_rows.v[i];
        for (size_t k = 0; k < r->units.n; k++) {
            AvpStr u = r->units.v[k];
            AvpVal *cv = avp_map_get(c.cfg, u);
            if (!cv) add(&ck, r->line, false, "%s table: unit `%S` is not in configure.py (renamed or removed?)", r->table, &u);
            else {
                AvpStr rel = avp_str_concat(a, AVP_S("src/"), ((const LedgerCfg *)cv->p)->path);
                if (!avp_isfile(avp_path_join(a, in.root, avp_cz(a, rel))))
                    add(&ck, r->line, false, "%s table: unit `%S` has an Object() entry but %S does not exist", r->table,
                        &u, &rel);
            }
        }
        for (size_t k = 0; k < r->functions.n; k++) {
            AvpStr fn = r->functions.v[k];
            bool found = false;
            if (r->units.n) {
                AvpStrVec known = {0};
                for (size_t q = 0; q < r->units.n; q++)
                    if (avp_map_has(c.cfg, r->units.v[q])) avp_vec_push(&known, r->units.v[q]);
                if (!known.n) continue; /* already reported as a unit finding */
                for (size_t q = 0; q < known.n && !found; q++) found = exists_in_unit(&c, known.v[q], fn);
                if (!found && SYM_OR_WORD(fn)) {
                    if (strcmp(r->table, "MATCH-PENDING") != 0) {
                        AvpBuf b;
                        avp_buf_init(&b, a);
                        for (size_t q = 0; q < known.n; q++) {
                            if (q) avp_buf_putz(&b, ", ");
                            avp_buf_pyfmt(&b, "`%S`", &known.v[q]);
                        }
                        AvpStr ks = avp_buf_str(&b);
                        add(&ck, r->line, true,
                            "%s table: `%S` is not in the source of %S (found elsewhere in src/include)", r->table, &fn,
                            &ks);
                    }
                    found = true;
                }
                avp_vec_free(&known);
            } else {
                found = SYM_OR_WORD(fn);
            }
            if (!found) {
                avp_set_add(missing, avp_pyfmt(a, "%d\x01%S", r->line, &fn));
                add(&ck, r->line, false, "%s table: `%S` is not a symbol, a report function or an identifier of the source",
                    r->table, &fn);
            }
        }
    }

    /* ORIGINAL-ASM: the body must still be assembly in source, and counted rows need a report row */
    AvpSet *asm_any = avp_set_new();
    const AvpJson *cfns = jarr(in.checker, "functions");
    for (size_t i = 0; i < jlen(cfns); i++) {
        const AvpJson *f = avp_json_at(cfns, i);
        if (avp_str_startswith(jstr(f, "path"), AVP_S("src/")) && avp_str_eqz(jstr(f, "status"), "ORIGINAL-ASM"))
            avp_set_add(asm_any, pair(a, ledger_unit_key(a, jstr(f, "path")), jstr(f, "name")));
    }
    for (int pass = 0; pass < 2; pass++) {
        const AvpJson *list = jarr(in.checker, pass ? "findings" : "allowed");
        for (size_t i = 0; i < jlen(list); i++) {
            const AvpJson *f = avp_json_at(list, i);
            if (avp_str_eqz(jstr(f, "rule"), "R18") && avp_str_startswith(jstr(f, "path"), AVP_S("src/")) &&
                avp_json_truthy(avp_json_getz(f, "function")))
                avp_set_add(asm_any, pair(a, ledger_unit_key(a, jstr(f, "path")), jstr(f, "function")));
        }
    }
    AvpSet *listed_asm = avp_set_new();
    for (size_t i = 0; i < in.exc_rows.n; i++) {
        const LedgerExcRow *r = in.exc_rows.v[i];
        if (strcmp(r->table, "ORIGINAL-ASM") != 0) continue;
        LedgerFnVec fns = {0};
        size_t known_units = 0;
        for (size_t k = 0; k < r->units.n; k++) {
            AvpStr u = r->units.v[k];
            AvpVal *cv = avp_map_get(c.cfg, u);
            if (!cv) continue;
            known_units++;
            AvpMap *names = unit_fn_names(&c, u);
            for (size_t q = 0; q < r->functions.n; q++) {
                AvpStr fn = r->functions.v[q];
                avp_set_add(listed_asm, pair(a, u, fn));
                if (avp_set_has(missing, avp_pyfmt(a, "%d\x01%S", r->line, &fn))) continue;
                AvpVal *fv = avp_map_get(names, fn);
                const LedgerFn *f = fv ? (const LedgerFn *)fv->p : NULL;
                /* an assembly source unit (`.s`) is assembly throughout; the rule checker does not read it */
                bool whole_asm = ledger_is_asm_source(((const LedgerCfg *)cv->p)->path);
                if (!whole_asm && !avp_set_has(asm_any, pair(a, u, fn)) &&
                    !avp_set_has(asm_any, pair(a, u, f ? f->alias : fn)))
                    add(&ck, r->line, false,
                        "ORIGINAL-ASM table: `%S` of `%S` has no assembly body in source any more (the checker finds no "
                        "R18 hit for it)",
                        &fn, &u);
                if (!f) {
                    if (avp_str_eqz(((const LedgerCfg *)cv->p)->status, "Matching"))
                        add(&ck, r->line, false,
                            "ORIGINAL-ASM table: `%S` of Matching unit `%S` is not in the objdiff report (no retail "
                            "address: nothing to subtract from the totals)",
                            &fn, &u);
                } else avp_vec_push(&fns, (LedgerFn *)f);
            }
            avp_map_free(names);
        }
        AvpStr nf, nb;
        if (r->cells.n > 1 && ledger_count_note(r->cells.v[1], &nf, &nb) && fns.n &&
            fns.n == r->functions.n * (known_units > 1 ? known_units : 1)) {
            i64 sum = 0;
            for (size_t q = 0; q < fns.n; q++) sum += fns.v[q]->size;
            i64 want_n = 0, want_b = 0;
            bool ov;
            avp_parse_int(nf, 10, &want_n, &ov);
            avp_parse_int(nb, 10, &want_b, &ov);
            if ((i64)fns.n != want_n || sum != want_b)
                add(&ck, r->line, false,
                    "ORIGINAL-ASM table: row says %S functions, %S bytes but the report has %d functions, %d bytes", &nf,
                    &nb, (i64)fns.n, sum);
        }
        avp_vec_free(&fns);
    }
    /* a Matching assembly source unit (`.s`) is ORIGINAL-ASM throughout: each of its functions must be listed */
    for (size_t i = 0; i < in.configure.n; i++) {
        const LedgerCfg *cf = &in.configure.v[i];
        if (!ledger_is_asm_source(cf->path) || !avp_str_eqz(cf->status, "Matching")) continue;
        AvpVal *uv = avp_map_get(c.model->units, cf->unit);
        const LedgerUnit *u = uv ? (const LedgerUnit *)uv->p : NULL;
        for (size_t k = 0; u && k < u->fns.n; k++) {
            const LedgerFn *f = u->fns.v[k];
            if (!avp_set_has(listed_asm, pair(a, cf->unit, f->name)) &&
                !avp_set_has(listed_asm, pair(a, cf->unit, f->alias)))
                add(&ck, line_asm, false,
                    "`%S` of `%S` is in an assembly source unit (ORIGINAL-ASM throughout) and is not listed in the "
                    "ORIGINAL-ASM table",
                    &f->name, &cf->unit);
        }
    }

    /* MATCH-PENDING: every checker MATCH-PENDING function in a Matching unit must be listed */
    AvpSet *listed_pend = avp_set_new();
    for (size_t i = 0; i < in.exc_rows.n; i++) {
        const LedgerExcRow *r = in.exc_rows.v[i];
        if (strcmp(r->table, "MATCH-PENDING") != 0) continue;
        for (size_t k = 0; k < r->units.n; k++)
            for (size_t q = 0; q < r->functions.n; q++) {
                avp_set_add(listed_pend, pair(a, r->units.v[k], r->functions.v[q]));
                avp_set_add(listed_pend, pair(a, r->units.v[k], last_component(r->functions.v[q])));
            }
        AvpStr nf, nb;
        if (r->cells.n > 1 && ledger_count_note(r->cells.v[1], &nf, &nb)) {
            i64 cnt = 0, sum = 0;
            for (size_t k = 0; k < r->units.n; k++) {
                AvpMap *names = unit_fn_names(&c, r->units.v[k]);
                for (size_t q = 0; q < r->functions.n; q++) {
                    AvpVal *fv = avp_map_get(names, r->functions.v[q]);
                    if (fv) {
                        cnt++;
                        sum += ((const LedgerFn *)fv->p)->size;
                    }
                }
                avp_map_free(names);
            }
            i64 want_n = 0, want_b = 0;
            bool ov;
            avp_parse_int(nf, 10, &want_n, &ov);
            avp_parse_int(nb, 10, &want_b, &ov);
            if (cnt && (cnt != want_n || sum != want_b))
                add(&ck, r->line, false,
                    "MATCH-PENDING table: row says %S functions, %S bytes but the report has %d functions, %d bytes",
                    &nf, &nb, cnt, sum);
        }
    }
    for (size_t i = 0; i < jlen(cfns); i++) {
        const AvpJson *f = avp_json_at(cfns, i);
        if (!avp_str_eqz(jstr(f, "status"), "MATCH-PENDING") || !avp_str_startswith(jstr(f, "path"), AVP_S("src/")))
            continue;
        AvpStr u = ledger_unit_key(a, jstr(f, "path"));
        AvpVal *cv = avp_map_get(c.cfg, u);
        if (!cv || !avp_str_eqz(((const LedgerCfg *)cv->p)->status, "Matching")) continue;
        AvpStr name = jstr(f, "name");
        if (!avp_set_has(listed_pend, pair(a, u, name)) && !avp_set_has(listed_pend, pair(a, u, last_component(name))))
            add(&ck, line_pend, false,
                "checker reports `%S` MATCH-PENDING in Matching unit `%S` but the MATCH-PENDING table does not list it",
                &name, &u);
    }
    /* linked pending rows the ledger would count */
    for (size_t i = 0; i < model->problems.n; i++)
        if (avp_str_contains(model->problems.v[i], AVP_S("maps to no ledger row")))
            add(&ck, line_pend, false, "%S", &model->problems.v[i]);

    /* MATCH-PENDING deviation text naming a function of a unit the row does not list */
    AvpMap *by_name = avp_map_new(); /* name -> AvpSet* of units */
    AvpMapIter it = {0};
    while (avp_map_next(model->units, &it)) {
        const LedgerUnit *u = (const LedgerUnit *)it.val->p;
        for (size_t k = 0; k < u->fns.n; k++) {
            bool cr;
            AvpVal *v = avp_map_put(by_name, u->fns.v[k]->name, &cr);
            if (cr) v->p = avp_set_new();
            avp_set_add((AvpSet *)v->p, u->name);
        }
    }
    for (size_t i = 0; i < in.exc_rows.n; i++) {
        const LedgerExcRow *r = in.exc_rows.v[i];
        if (strcmp(r->table, "MATCH-PENDING") != 0) continue;
        for (size_t ci = 2; ci < r->cells.n; ci++)
            for (size_t k = 0; k < r->ticks[ci].n; k++) {
                AvpStr tok = r->ticks[ci].v[k];
                AvpVal *ov = avp_map_get(by_name, tok);
                if (!ov) continue;
                AvpSet *owners = (AvpSet *)ov->p;
                bool shared = false;
                for (size_t q = 0; q < r->units.n && !shared; q++) shared = avp_set_has(owners, r->units.v[q]);
                if (shared || vec_has(&r->functions, tok)) continue;
                AvpStrVec sorted = avp_set_sorted(owners);
                AvpBuf b;
                avp_buf_init(&b, a);
                for (size_t q = 0; q < sorted.n; q++) {
                    if (q) avp_buf_putz(&b, ", ");
                    avp_buf_pyfmt(&b, "`%S`", &sorted.v[q]);
                }
                avp_vec_free(&sorted);
                AvpStr os = avp_buf_str(&b);
                add(&ck, r->line, true,
                    "MATCH-PENDING table: the text names `%S` (a function of %S), which the row's unit and function "
                    "cells do not list",
                    &tok, &os);
            }
    }

    /* ORIGINAL-ASM by source marker only (listing in exceptions.md is the reviewer's record) */
    AvpSet *helpers = avp_set_new();
    for (size_t i = 0; i < in.exc_rows.n; i++) {
        const LedgerExcRow *r = in.exc_rows.v[i];
        if (strcmp(r->table, "helpers") != 0) continue;
        for (size_t k = 0; k < r->units.n; k++)
            for (size_t q = 0; q < r->functions.n; q++) avp_set_add(helpers, pair(a, r->units.v[k], r->functions.v[q]));
    }
    const AvpJson *allowed = jarr(in.checker, "allowed");
    for (size_t i = 0; i < jlen(allowed); i++) {
        const AvpJson *f = avp_json_at(allowed, i);
        if (avp_str_eqz(jstr(f, "rule"), "R18") && avp_str_contains(jstr(f, "message"), AVP_S("written in assembly")) &&
            avp_str_startswith(jstr(f, "path"), AVP_S("src/")) &&
            avp_str_contains(jstr(f, "reason"), AVP_S("marker in the source"))) {
            AvpStr u = ledger_unit_key(a, jstr(f, "path"));
            AvpStr fn = jstr(f, "function");
            if (!avp_set_has(listed_asm, pair(a, u, fn)) && !avp_set_has(helpers, pair(a, u, fn)))
                add(&ck, line_asm, true,
                    "`%S` of `%S` is assembly by source marker only and is not listed in any exceptions.md table", &fn,
                    &u);
        }
    }
#undef SYM_OR_WORD

    size_t n_findings = ck.findings.n;
    for (size_t i = 0; i < ck.findings.n; i++) avp_buf_pyfmt(io->out, "%S\n", &ck.findings.v[i]);
    for (size_t i = 0; i < ck.notes.n; i++) avp_buf_pyfmt(io->out, "%S\n", &ck.notes.v[i]);
    avp_buf_pyfmt(io->err, "exceptions-check: %d finding(s), %d note(s)\n", n_findings, ck.notes.n);
    return (n_findings || (args->strict && ck.notes.n)) ? 1 : 0;
}

/* ---- main --------------------------------------------------------------------------------------------------- */

static void common_args(AvpArgParser *p) {
    avp_arg(p, &(AvpArgDesc){.names = "--root", .dflt = AVP_DEF_NONE, .help = "repository root (default: parent of tools/)"});
    avp_arg(p, &(AvpArgDesc){.names = "--report", .dflt = AVP_DEF_NONE,
                             .help = "objdiff report (default build/R5WEA4/report.json, else the baseline)"});
    avp_arg(p, &(AvpArgDesc){.names = "--checker-json", .dflt = AVP_DEF_NONE,
                             .help = "saved output of check_rules.exe --json (default: run it)"});
}

static AvpArgParser *build_parser(AvpArena *a) {
    AvpArgParser *ap = avp_args_parser(a, &(AvpArgParserDesc){.prog = "ledger.exe", .description = DESCRIPTION});
    AvpArgSubparsers *sub = avp_args_subparsers(ap, &(AvpArgSubparsersDesc){.dest = "cmd", .required = true});
    AvpArgParser *p = avp_args_subparser(sub, "progress",
                                         &(AvpArgParserDesc){.help = "regenerate the generated parts of docs/progress.md"});
    common_args(p);
    avp_arg(p, &(AvpArgDesc){.names = "--ledger", .dflt = AVP_DEF_NONE, .help = "ledger to read (default docs/progress.md)"});
    avp_arg(p, &(AvpArgDesc){.names = "--out", .dflt = AVP_DEF_NONE, .help = "write here instead of in place ('-' for stdout)"});
    avp_arg(p, &(AvpArgDesc){.names = "--check", .action = AVP_ARG_STORE_TRUE,
                             .help = "write nothing; exit 1 if the ledger differs from the generated one"});
    avp_arg(p, &(AvpArgDesc){.names = "--diff", .action = AVP_ARG_STORE_TRUE, .help = "with --check, print a unified diff"});
    avp_arg(p, &(AvpArgDesc){.names = "--diff-lines", .type = AVP_T_INT, .dflt = AVP_DEF_INT(60)});
    avp_arg(p, &(AvpArgDesc){.names = "--add-markers", .action = AVP_ARG_STORE_TRUE,
                             .help = "wrap each located block in ledger markers (once)"});
    avp_arg(p, &(AvpArgDesc){.names = "--force", .action = AVP_ARG_STORE_TRUE,
                             .help = "write even when cross-checks report problems"});
    AvpArgParser *e = avp_args_subparser(sub, "exceptions-check",
                                         &(AvpArgParserDesc){.help = "consistency checks on docs/exceptions.md"});
    common_args(e);
    avp_arg(e, &(AvpArgDesc){.names = "--exceptions", .dflt = AVP_DEF_NONE,
                             .help = "exceptions record (default docs/exceptions.md)"});
    avp_arg(e, &(AvpArgDesc){.names = "--strict", .action = AVP_ARG_STORE_TRUE, .help = "notes count as findings"});
    return ap;
}

/* An option's value; "" counts as not given (the Python tests the truth of each). */
static const char *opt(AvpArena *a, const AvpArgs *r, const char *dest) {
    if (!avp_args_has(r, dest) || avp_args_is_none(r, dest)) return NULL;
    AvpStr s = avp_args_str(r, dest);
    return s.n ? avp_cz(a, s) : NULL;
}

/* The default --root: the checkout holding the current directory (Python: the checkout of ledger.py; D-ledger-1). */
static const char *default_root(AvpArena *a) {
    AvpTry t;
    const char *r = NULL;
    if (AVP_TRY(&t)) {
        r = shsm_find_root(NULL);
        avp_try_end(&t);
    } else {
        r = avp_path_abspath(a, avp_join(a, avp_exe_dir(), "..", ".."));
    }
    return r;
}

int ledger_main(int argc, char **argv, AvpBuf *out, AvpBuf *err) {
    AvpArena *a = avp_arena();
    AvpArgParser *ap = build_parser(a);
    AvpStrVec list = {0};
    for (int i = 1; i < argc; i++) avp_vec_push(&list, avp_str(argv[i]));
    AvpArgsExit ex = {0};
    AvpArgs *r = avp_args_try_parse(ap, list.v, list.n, false, &ex);
    avp_vec_free(&list);
    if (!r) {
        avp_buf_put(out, ex.out);
        avp_buf_put(err, ex.err);
        return ex.code;
    }
    Args args = {0};
    args.cmd = opt(a, r, "cmd");
    const char *root = NULL;
    if (avp_args_has(r, "root") && !avp_args_is_none(r, "root")) root = avp_cz(a, avp_args_str(r, "root"));
    args.root = root ? root : default_root(a);
    args.report = opt(a, r, "report");
    args.checker_json = opt(a, r, "checker_json");
    bool progress = args.cmd && strcmp(args.cmd, "progress") == 0;
    if (progress) {
        args.ledger = opt(a, r, "ledger");
        args.out = opt(a, r, "out");
        args.check = avp_args_bool(r, "check");
        args.diff = avp_args_bool(r, "diff");
        args.diff_lines = avp_args_int(r, "diff_lines");
        args.add_markers = avp_args_bool(r, "add_markers");
        args.force = avp_args_bool(r, "force");
    } else {
        args.exceptions = opt(a, r, "exceptions");
        args.strict = avp_args_bool(r, "strict");
    }
    Io io = {a, out, err};
    AvpTry t;
    int code;
    if (AVP_TRY(&t)) {
        code = progress ? cmd_progress(&io, &args) : cmd_exceptions_check(&io, &args);
        avp_try_end(&t);
    } else if (t.code == LEDGER_ERROR) {
        avp_buf_pyfmt(err, "ledger: error: %s\n", t.msg);
        code = 2;
    } else {
        /* the Python ends with a traceback; its last line */
        if (t.code == PY_ERROR) avp_buf_pyfmt(err, "%s\n", t.msg);
        else avp_buf_pyfmt(err, "Exception: %s\n", t.msg);
        code = 1;
    }
    return code;
}
