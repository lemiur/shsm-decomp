/* solver_record: the record of one stop decision, the verdict rules, the cause table, and the three output forms
 * (records.json, records.tsv / causes.tsv, the readable block). */
#include "solver.h"

#include <stdarg.h>
#include <stdlib.h>

const char *const SV_CLASS_NAMES[SV_NCLASS] = {"not-started", "source-call",  "inline",   "flag",
                                               "referent",    "control-flow", "register", "schedule", "other"};
const char *const SV_VERDICT_NAMES[SV_NVERDICT] = {"MATCH",          "CLOSER",      "NO-FORM-WITHIN-RULES",
                                                   "CONDITION-ONLY", "MODEL-GAP",   "SKIPPED"};

SvRecord *sv_record_new(AvpArena *a, const char *unit, const char *function, SvClass cls) {
    SvRecord *r = AVP_NEW(a, SvRecord);
    r->unit = unit;
    r->function = function;
    r->cls = cls;
    r->site_addr = -1;
    r->site_row = -1;
    r->rows_retail = r->rows_compiled = r->rows_differing = -1;
    r->verdict = SV_CONDITION_ONLY;
    return r;
}

void sv_note(AvpArena *a, SvRecord *rec, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *s = avp_vsprintf(a, fmt, ap);
    va_end(ap);
    avp_vec_push(&rec->notes, s);
}

void sv_evidence(AvpArena *a, SvRecord *rec, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *s = avp_vsprintf(a, fmt, ap);
    va_end(ap);
    for (size_t i = 0; i < rec->evidence.n; i++)
        if (!strcmp(rec->evidence.v[i], s)) return; /* wall repeats a reading per call site */
    avp_vec_push(&rec->evidence, s);
}

/* ---- verdicts -------------------------------------------------------------------------------------------------- */

SvVerdict sv_assign_verdict(SvRecord *rec) {
    SvVerdict v = SV_CONDITION_ONLY;
    bool match = false, closer = false, closes_by_breaking = false, probe_hit = false;
    for (size_t i = 0; i < rec->candidates.n; i++) {
        const SvCandidate *c = &rec->candidates.v[i];
        if (!c->compiled) continue;
        bool clean = !c->broken.n && !c->rules.n;
        if (c->exact && clean) match = true;
        else if (clean && c->rows_before >= 0 && c->rows_after >= 0 && c->rows_after < c->rows_before) closer = true;
        if (c->exact && c->broken.n) closes_by_breaking = true;
    }
    for (size_t i = 0; i < rec->probes.n; i++)
        if (!rec->probes.v[i].error && (rec->probes.v[i].closes || rec->probes.v[i].improves)) probe_hit = true;
    if (rec->cls == SV_NOT_STARTED)
        v = SV_SKIPPED;
    else if (rec->control_failed || rec->model_gap)
        v = SV_MODEL_GAP;
    else if (match)
        v = SV_MATCH;
    else if (closer)
        v = SV_CLOSER;
    else if (probe_hit || rec->rule_exception || closes_by_breaking || (rec->surviving_source && rec->phase_built))
        v = SV_NO_FORM;
    rec->verdict = v;
    return v;
}

/* ---- causes ---------------------------------------------------------------------------------------------------- */

SvCauses sv_group_causes(AvpArena *a, const SvRecords *recs) {
    SvCauses out = {0};
    AvpMap *idx = avp_map_new();
    for (size_t i = 0; i < recs->n; i++) {
        const SvRecord *r = recs->v[i];
        const char *cause = r->cause ? r->cause : SV_CLASS_NAMES[r->cls];
        bool created = false;
        AvpVal *v = avp_map_put(idx, avp_str(cause), &created);
        if (created) {
            v->i = (i64)out.n;
            SvCause c = {0};
            c.cause = cause;
            avp_vec_push(&out, c);
        }
        SvCause *c = &out.v[v->i];
        c->count++;
        c->verdicts[r->verdict]++;
        bool seen = false;
        for (size_t k = 0; k < c->functions.n && !seen; k++) seen = strcmp(c->functions.v[k], r->function) == 0;
        if (!seen) avp_vec_push(&c->functions, (char *)r->function);
    }
    /* most frequent first, stable */
    for (size_t i = 1; i < out.n; i++)
        for (size_t j = i; j > 0 && out.v[j - 1].count < out.v[j].count; j--) {
            SvCause t = out.v[j];
            out.v[j] = out.v[j - 1];
            out.v[j - 1] = t;
        }
    AVP_UNUSED(a);
    return out;
}

/* ---- JSON ------------------------------------------------------------------------------------------------------ */

static AvpJson *jz(AvpArena *a, const char *s) { return s ? avp_json_strz(a, s) : avp_json_null(); }

static AvpJson *jlist(AvpArena *a, const AvpCzVec *v) {
    AvpJson *l = avp_json_arr(a);
    for (size_t i = 0; i < v->n; i++) avp_json_arr_push(a, l, avp_json_strz(a, v->v[i]));
    return l;
}

static const char *joined_notes(AvpArena *a, const SvRecord *r) {
    if (!r->notes.n) return NULL;
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < r->notes.n; i++) {
        if (i) avp_buf_putz(&b, "; ");
        avp_buf_putz(&b, r->notes.v[i]);
    }
    return b.p;
}

AvpJson *sv_record_json(AvpArena *a, const SvRecord *r) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("function"), jz(a, r->function));
    avp_json_set(a, o, AVP_S("unit"), jz(a, r->unit));
    avp_json_set(a, o, AVP_S("callee"), jz(a, r->callee));
    avp_json_set(a, o, AVP_S("percent"), r->has_percent ? avp_json_float(a, r->percent) : avp_json_null());
    AvpJson *rows = avp_json_obj(a);
    avp_json_set(a, rows, AVP_S("retail"), r->rows_retail >= 0 ? avp_json_int(a, r->rows_retail) : avp_json_null());
    avp_json_set(a, rows, AVP_S("compiled"),
                 r->rows_compiled >= 0 ? avp_json_int(a, r->rows_compiled) : avp_json_null());
    avp_json_set(a, rows, AVP_S("differing"),
                 r->rows_differing >= 0 ? avp_json_int(a, r->rows_differing) : avp_json_null());
    avp_json_set(a, o, AVP_S("rows"), rows);
    avp_json_set(a, o, AVP_S("class"), avp_json_strz(a, SV_CLASS_NAMES[r->cls]));
    AvpJson *w = avp_json_obj(a);
    avp_json_set(a, w, AVP_S("class"), jz(a, r->wall));
    avp_json_set(a, w, AVP_S("classes"), jlist(a, &r->wall_classes));
    avp_json_set(a, w, AVP_S("verdict"), jz(a, r->wall_verdict));
    avp_json_set(a, o, AVP_S("wall"), w);
    AvpJson *site = avp_json_obj(a);
    avp_json_set(a, site, AVP_S("retail_address"),
                 r->site_addr >= 0 ? avp_json_strz(a, avp_sprintf(a, "0x%08llX", (unsigned long long)r->site_addr))
                                   : avp_json_null());
    avp_json_set(a, site, AVP_S("compiled_row"), r->site_row >= 0 ? avp_json_int(a, r->site_row) : avp_json_null());
    avp_json_set(a, o, AVP_S("site"), site);
    avp_json_set(a, o, AVP_S("ours"), jz(a, r->ours));
    avp_json_set(a, o, AVP_S("retail"), jz(a, r->retail));
    avp_json_set(a, o, AVP_S("condition"), jz(a, r->condition));
    avp_json_set(a, o, AVP_S("trace"), r->trace ? r->trace : avp_json_null());
    avp_json_set(a, o, AVP_S("evidence"), jlist(a, &r->evidence));
    AvpJson *cands = avp_json_arr(a);
    for (size_t i = 0; i < r->candidates.n; i++) {
        const SvCandidate *c = &r->candidates.v[i];
        AvpJson *x = avp_json_obj(a);
        avp_json_set(a, x, AVP_S("number"), avp_json_int(a, c->number));
        avp_json_set(a, x, AVP_S("form"), jz(a, c->form));
        avp_json_set(a, x, AVP_S("unevidenced_declaration_order"), avp_json_bool(sv_candidate_unevidenced(c)));
        avp_json_set(a, x, AVP_S("description"), jz(a, c->desc));
        avp_json_set(a, x, AVP_S("line"), avp_json_int(a, c->line));
        avp_json_set(a, x, AVP_S("evidence"), jz(a, c->evidence));
        avp_json_set(a, x, AVP_S("compiled"), avp_json_bool(c->compiled));
        avp_json_set(a, x, AVP_S("rows_before"), avp_json_int(a, c->rows_before));
        avp_json_set(a, x, AVP_S("rows_after"), avp_json_int(a, c->rows_after));
        avp_json_set(a, x, AVP_S("exact"), avp_json_bool(c->exact));
        avp_json_set(a, x, AVP_S("broken"), jlist(a, &c->broken));
        avp_json_set(a, x, AVP_S("rules"), jlist(a, &c->rules));
        avp_json_arr_push(a, cands, x);
    }
    avp_json_set(a, o, AVP_S("candidates"), cands);
    AvpJson *probes = avp_json_arr(a);
    for (size_t i = 0; i < r->probes.n; i++) {
        const SvProbe *p = &r->probes.v[i];
        AvpJson *x = avp_json_obj(a);
        avp_json_set(a, x, AVP_S("number"), avp_json_int(a, p->number));
        avp_json_set(a, x, AVP_S("lever"), jz(a, p->lever));
        avp_json_set(a, x, AVP_S("result"), jz(a, p->result));
        avp_json_set(a, x, AVP_S("closes"), avp_json_bool(p->closes));
        avp_json_set(a, x, AVP_S("improves"), avp_json_bool(p->improves));
        avp_json_set(a, x, AVP_S("no_effect"), avp_json_bool(p->no_effect));
        avp_json_set(a, x, AVP_S("lever_ruled"), avp_json_bool(p->lever_ruled));
        avp_json_arr_push(a, probes, x);
    }
    avp_json_set(a, o, AVP_S("probes"), probes);
    avp_json_set(a, o, AVP_S("verdict"), avp_json_strz(a, SV_VERDICT_NAMES[r->verdict]));
    avp_json_set(a, o, AVP_S("cause"), jz(a, r->cause ? r->cause : SV_CLASS_NAMES[r->cls]));
    avp_json_set(a, o, AVP_S("note"), jz(a, joined_notes(a, r)));
    avp_json_set(a, o, AVP_S("seconds"), avp_json_float(a, avp_py_round(r->seconds, 2)));
    avp_json_set(a, o, AVP_S("ran_candidates"), avp_json_bool(r->ran_candidates));
    avp_json_set(a, o, AVP_S("budget_hit"), avp_json_bool(r->budget_hit));
    return o;
}

/* ---- TSV ------------------------------------------------------------------------------------------------------- */

static void tsv_field(AvpBuf *b, const char *s) {
    for (const char *p = s ? s : ""; *p; p++) avp_buf_putc(b, (*p == '\t' || *p == '\n' || *p == '\r') ? ' ' : *p);
}

AvpStr sv_records_tsv(AvpArena *a, const SvRecords *recs) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, "function\tunit\tcallee\tclass\tverdict\tpercent\trows_differing\tsite\tours\tretail\tcondition\t"
                     "cause\tnote\tseconds\ttrace\n");
    for (size_t i = 0; i < recs->n; i++) {
        const SvRecord *r = recs->v[i];
        const char *f[15] = {
            r->function,
            r->unit,
            r->callee,
            SV_CLASS_NAMES[r->cls],
            SV_VERDICT_NAMES[r->verdict],
            r->has_percent ? avp_sprintf(a, "%.2f", r->percent) : "",
            r->rows_differing >= 0 ? avp_sprintf(a, "%lld", (long long)r->rows_differing) : "",
            r->site_addr >= 0 ? avp_sprintf(a, "0x%08llX", (unsigned long long)r->site_addr) : "",
            r->ours,
            r->retail,
            r->condition,
            r->cause ? r->cause : SV_CLASS_NAMES[r->cls],
            joined_notes(a, r),
            avp_sprintf(a, "%.2f", r->seconds),
            r->trace_condition,
        };
        for (int k = 0; k < 15; k++) {
            if (k) avp_buf_putc(&b, '\t');
            tsv_field(&b, f[k]);
        }
        avp_buf_putc(&b, '\n');
    }
    return avp_buf_str(&b);
}

AvpStr sv_causes_tsv(AvpArena *a, const SvCauses *causes) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, "cause\tcount\tfunctions\tverdicts\n");
    for (size_t i = 0; i < causes->n; i++) {
        tsv_field(&b, causes->v[i].cause);
        avp_buf_printf(&b, "\t%zu\t", causes->v[i].count);
        for (size_t k = 0; k < causes->v[i].functions.n; k++) {
            if (k) avp_buf_putz(&b, ", ");
            tsv_field(&b, causes->v[i].functions.v[k]);
        }
        avp_buf_putc(&b, '\t');
        bool any = false;
        for (int v = 0; v < SV_NVERDICT; v++)
            if (causes->v[i].verdicts[v]) {
                avp_buf_printf(&b, "%s%s %zu", any ? ", " : "", SV_VERDICT_NAMES[v], causes->v[i].verdicts[v]);
                any = true;
            }
        avp_buf_putc(&b, '\n');
    }
    return avp_buf_str(&b);
}

/* ---- the readable block ---------------------------------------------------------------------------------------- */

static void line(AvpArena *a, AvpStrVec *out, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *s = avp_vsprintf(a, fmt, ap);
    va_end(ap);
    avp_vec_push(out, avp_str(s));
}

#define SV_UNEVIDENCED "declaration order unevidenced (no sibling DWARF)"

bool sv_candidate_unevidenced(const SvCandidate *c) {
    return (c->form && !strcmp(c->form, "decl")) || (c->desc && strstr(c->desc, SV_UNEVIDENCED));
}

/* The marker a decl-family candidate carries in every row that names it (its description has it already). */
static const char *cand_marker(const SvCandidate *c) {
    return sv_candidate_unevidenced(c) && !(c->desc && strstr(c->desc, SV_UNEVIDENCED)) ? SV_UNEVIDENCED ": " : "";
}

void sv_record_lines(AvpArena *a, const SvRecord *r, AvpStrVec *out) {
    line(a, out, "%s  [%s] %s  (%s)", r->function, SV_CLASS_NAMES[r->cls], SV_VERDICT_NAMES[r->verdict], r->unit);
    AvpBuf b;
    avp_buf_init(&b, a);
    if (r->callee) avp_buf_printf(&b, "%s %s", r->cls == SV_FLAG ? "flag" : "callee", r->callee);
    if (r->site_addr >= 0)
        avp_buf_printf(&b, "%ssite 0x%08llX", b.n ? "; " : "", (unsigned long long)r->site_addr);
    if (r->site_row >= 0) avp_buf_printf(&b, " (compiled row %lld)", (long long)r->site_row);
    if (r->rows_differing >= 0)
        avp_buf_printf(&b, "%srows %lld differ of %lld", b.n ? "; " : "", (long long)r->rows_differing,
                       (long long)r->rows_retail);
    if (r->has_percent) avp_buf_printf(&b, "%s%.2f%%", b.n ? "; " : "", r->percent);
    if (r->ran_candidates)
        avp_buf_printf(&b, "%s%.1f s%s", b.n ? "; " : "", r->seconds, r->budget_hit ? " (budget reached)" : "");
    if (b.n) line(a, out, "  %s", b.p);
    if (r->wall) line(a, out, "  wall: %s", r->wall_verdict ? r->wall_verdict : r->wall);
    if (r->ours) line(a, out, "  ours: %s", r->ours);
    if (r->retail) line(a, out, "  retail: %s", r->retail);
    if (r->condition) line(a, out, "  condition: %s", r->condition);
    if (r->trace_condition && (!r->condition || strcmp(r->trace_condition, r->condition)))
        line(a, out, "  trace: %s", r->trace_condition);
    for (size_t i = 0; i < r->evidence.n; i++) line(a, out, "  evidence: %s", r->evidence.v[i]);
    for (size_t i = 0; i < r->candidates.n; i++) {
        const SvCandidate *c = &r->candidates.v[i];
        line(a, out, "  candidate #%d %s: %s%s (rows %lld -> %lld%s%s%s)", c->number, c->form, cand_marker(c),
             c->desc ? c->desc : "", (long long)c->rows_before, (long long)c->rows_after, c->exact ? ", exact" : "",
             c->rules.n ? "; rule finding: " : "", c->rules.n ? c->rules.v[0] : "");
    }
    for (size_t i = 0; i < r->probes.n; i++)
        line(a, out, "  probe #%d %s: %s", r->probes.v[i].number, r->probes.v[i].lever,
             r->probes.v[i].result ? r->probes.v[i].result : "");
    for (size_t i = 0; i < r->notes.n; i++) line(a, out, "  note: %s", r->notes.v[i]);
    if (r->cause) line(a, out, "  cause: %s", r->cause);
}

/* ---- timing ---------------------------------------------------------------------------------------------------- */

static int cmp_double(const void *x, const void *y) {
    double a = *(const double *)x, b = *(const double *)y;
    return a < b ? -1 : a > b;
}

SvTiming sv_timing(const SvRecords *recs) {
    SvTiming t = {0};
    t.stops = recs->n;
    double *v = recs->n ? calloc(recs->n, sizeof(double)) : NULL;
    for (size_t i = 0; i < recs->n; i++) {
        const SvRecord *r = recs->v[i];
        if (r->budget_hit) t.budget_hit++;
        if (!r->ran_candidates) continue;
        v[t.solved++] = r->seconds;
        t.total_solved += r->seconds;
        if (r->seconds > t.max) t.max = r->seconds;
    }
    if (t.solved) {
        qsort(v, t.solved, sizeof(double), cmp_double);
        t.median = t.solved % 2 ? v[t.solved / 2] : (v[t.solved / 2 - 1] + v[t.solved / 2]) / 2;
    }
    free(v);
    return t;
}

/* ---- the two summary tables ------------------------------------------------------------------------------------- */

/* The first n bytes of s, with "..." when cut. */
static const char *clip(AvpArena *a, const char *s, size_t n) {
    if (!s) return "";
    if (strlen(s) <= n) return s;
    return avp_sprintf(a, "%.*s...", (int)(n - 3), s);
}

/* The description without the unevidenced marker prefix (the table shows the marker itself). */
static const char *desc_body(const SvCandidate *c) {
    const char *d = c->desc ? c->desc : "";
    const char *m = strstr(d, SV_UNEVIDENCED);
    if (!m) return d;
    m += strlen(SV_UNEVIDENCED);
    if (*m == ':') m++;
    while (*m == ' ') m++;
    return m;
}

/* The best candidate (clean exact, closes-by-breaking, clean closer by fewest rows) or probe of a record, in a few
 * words. */
static const char *best_text(AvpArena *a, const SvRecord *r) {
    const SvCandidate *best = NULL;
    int rank = 9;
    for (size_t i = 0; i < r->candidates.n; i++) {
        const SvCandidate *c = &r->candidates.v[i];
        if (!c->compiled) continue;
        bool clean = !c->broken.n && !c->rules.n;
        int k = c->exact && clean ? 0 : c->exact ? 1 : clean && c->rows_after >= 0 && c->rows_after < c->rows_before ? 2 : 9;
        if (k < rank || (k == rank && k == 2 && c->rows_after < best->rows_after)) {
            best = c;
            rank = k;
        }
    }
    if (best) {
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_printf(&b, "#%d %s", best->number, best->form);
        if (sv_candidate_unevidenced(best)) avp_buf_putz(&b, " [" SV_UNEVIDENCED "]");
        avp_buf_printf(&b, ": %s (rows %lld -> %lld%s)", clip(a, desc_body(best), 70), (long long)best->rows_before,
                       (long long)best->rows_after, best->exact ? ", exact" : "");
        if (best->broken.n) avp_buf_printf(&b, " [breaks %s]", best->broken.v[0]);
        if (best->rules.n) avp_buf_printf(&b, " [rule finding]");
        return b.p;
    }
    const SvProbe *pb = NULL;
    for (size_t i = 0; i < r->probes.n; i++) {
        const SvProbe *p = &r->probes.v[i];
        if (p->error) continue;
        if (!pb || (p->closes && !pb->closes) || (!p->closes && !pb->closes && p->improves && !pb->improves)) pb = p;
    }
    if (pb && (pb->closes || pb->improves))
        return avp_sprintf(a, "probe #%d %s: %s", pb->number, clip(a, pb->lever, 50), clip(a, pb->result, 70));
    return "-";
}


static void put_table(AvpArena *a, AvpStrVec *out, const char *const *head, size_t ncol, const AvpStrVec *cells,
                      size_t nrows) {
    size_t *w = AVP_NEWN(a, size_t, ncol);
    for (size_t c = 0; c < ncol; c++) w[c] = strlen(head[c]);
    for (size_t r = 0; r < nrows; r++)
        for (size_t c = 0; c < ncol; c++) {
            size_t n = cells[r].v[c].n;
            if (n > w[c]) w[c] = n;
        }
    for (size_t pass = 0; pass < nrows + 2; pass++) {
        AvpBuf b;
        avp_buf_init(&b, a);
        for (size_t c = 0; c < ncol; c++) {
            if (c) avp_buf_putz(&b, "  ");
            const char *txt = pass == 0 ? head[c] : pass == 1 ? "" : avp_cz(a, cells[pass - 2].v[c]);
            size_t n = strlen(txt);
            if (pass == 1) {
                for (size_t k = 0; k < w[c]; k++) avp_buf_putc(&b, '-');
                continue;
            }
            avp_buf_putz(&b, txt);
            if (c + 1 < ncol)
                for (size_t k = n; k < w[c]; k++) avp_buf_putc(&b, ' ');
        }
        avp_vec_push(out, avp_buf_str(&b));
    }
}

void sv_tables(AvpArena *a, const SvRecords *recs, const SvCauses *causes, AvpStrVec *out) {
    static const char *const H1[] = {"function", "unit", "class", "verdict", "rows", "best candidate or probe"};
    avp_vec_push(out, avp_str(avp_sprintf(a, "table 1: one row per record (%zu)", recs->n)));
    AvpStrVec *c1 = AVP_NEWN(a, AvpStrVec, recs->n + 1);
    for (size_t i = 0; i < recs->n; i++) {
        const SvRecord *r = recs->v[i];
        const char *rows = r->rows_differing >= 0 && r->rows_retail >= 0
                               ? avp_sprintf(a, "%lld/%lld", (long long)r->rows_differing, (long long)r->rows_retail)
                               : "-";
        avp_vec_push(&c1[i], avp_str(r->function));
        avp_vec_push(&c1[i], avp_str(r->unit));
        avp_vec_push(&c1[i], avp_str(SV_CLASS_NAMES[r->cls]));
        avp_vec_push(&c1[i], avp_str(SV_VERDICT_NAMES[r->verdict]));
        avp_vec_push(&c1[i], avp_str(rows));
        avp_vec_push(&c1[i], avp_str(best_text(a, r)));
    }
    put_table(a, out, H1, 6, c1, recs->n);
    avp_vec_push(out, AVP_S(""));
    static const char *const H2[] = {"cause", "count", "verdicts", "functions"};
    avp_vec_push(out, avp_str(avp_sprintf(a, "table 2: causes (%zu)", causes->n)));
    AvpStrVec *c2 = AVP_NEWN(a, AvpStrVec, causes->n + 1);
    for (size_t i = 0; i < causes->n; i++) {
        const SvCause *c = &causes->v[i];
        AvpBuf vb, fb;
        avp_buf_init(&vb, a);
        avp_buf_init(&fb, a);
        for (int k = 0; k < SV_NVERDICT; k++)
            if (c->verdicts[k]) avp_buf_printf(&vb, "%s%s %zu", vb.n ? ", " : "", SV_VERDICT_NAMES[k], c->verdicts[k]);
        for (size_t k = 0; k < c->functions.n && k < 5; k++)
            avp_buf_printf(&fb, "%s%s", k ? ", " : "", c->functions.v[k]);
        if (c->functions.n > 5) avp_buf_printf(&fb, " (+%zu more)", c->functions.n - 5);
        avp_vec_push(&c2[i], avp_str(c->cause));
        avp_vec_push(&c2[i], avp_str(avp_sprintf(a, "%zu", c->count)));
        avp_vec_push(&c2[i], avp_str(vb.p ? vb.p : ""));
        avp_vec_push(&c2[i], avp_str(fb.p ? fb.p : ""));
    }
    put_table(a, out, H2, 4, c2, causes->n);
}
