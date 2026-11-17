/* solver_apply: --apply N. A run saves, for every function that has candidates or probes, <out>/apply/<function>.json
 * (the unit's source path and sha1 as the run read it, and one entry per candidate and probe with what the verdict
 * rules look at) and the whole unit text of each candidate (<out>/apply/<function>/<N>.txt). --apply reads that
 * back and writes candidate N into the unit's source file, after refusing everything the verdict rules would not
 * count: a probe, a candidate that is not exact or closer, one with broken functions or rule findings, a run that
 * edited the text (--case), and a source file that is no longer the one the run read. It writes that one file; it
 * never commits and never touches another path. */
#include "solver.h"

#include <stdarg.h>

/* ---- numbering ------------------------------------------------------------------------------------------------- */

void sv_number_records(SvRecords *recs) {
    AvpMap *count = avp_map_new();
    for (size_t i = 0; i < recs->n; i++) {
        SvRecord *r = recs->v[i];
        AvpVal *v = avp_map_put(count, avp_str(r->function), NULL);
        for (size_t k = 0; k < r->candidates.n; k++) r->candidates.v[k].number = (int)++v->i;
    }
    for (size_t i = 0; i < recs->n; i++) {
        SvRecord *r = recs->v[i];
        AvpVal *v = avp_map_put(count, avp_str(r->function), NULL);
        for (size_t k = 0; k < r->probes.n; k++) r->probes.v[k].number = (int)++v->i;
    }
    avp_map_free(count);
}

/* ---- the saved run --------------------------------------------------------------------------------------------- */

/* A function name as a file name (mangled names hold only letters, digits and _ @ $ ? in practice). */
static const char *safe_name(AvpArena *a, const char *fn) {
    char *s = avp_strdup(a, fn);
    for (char *p = s; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')) *p = '_';
    return s;
}

static AvpJson *jlist(AvpArena *a, const AvpCzVec *v) {
    AvpJson *l = avp_json_arr(a);
    for (size_t i = 0; i < v->n; i++) avp_json_arr_push(a, l, avp_json_strz(a, v->v[i]));
    return l;
}

bool sv_apply_save(AvpArena *a, const char *outdir, const SvRecords *recs) {
    const char *dir = avp_path_join(a, outdir, "apply");
    if (!avp_makedirs(dir, true)) return false;
    AvpSet *done = avp_set_new();
    bool ok = true;
    for (size_t i = 0; i < recs->n && ok; i++) {
        const SvRecord *first = recs->v[i];
        if (!avp_set_add(done, avp_str(first->function))) continue;
        const char *safe = safe_name(a, first->function);
        const char *jpath = avp_path_join(a, dir, avp_sprintf(a, "%s.json", safe)), *tdir = avp_path_join(a, dir, safe);
        size_t ncand = 0, nprobe = 0;
        for (size_t j = i; j < recs->n; j++)
            if (!strcmp(recs->v[j]->function, first->function)) {
                ncand += recs->v[j]->candidates.n;
                nprobe += recs->v[j]->probes.n;
            }
        if (avp_isdir(tdir)) avp_rmtree(tdir, 1); /* an earlier run of the function */
        if (avp_exists(jpath)) avp_remove(jpath);
        if (!ncand && !nprobe) continue;
        AvpJson *doc = avp_json_obj(a), *cands = avp_json_arr(a), *probes = avp_json_arr(a);
        avp_json_set(a, doc, AVP_S("function"), avp_json_strz(a, first->function));
        avp_json_set(a, doc, AVP_S("unit"), avp_json_strz(a, first->unit ? first->unit : ""));
        avp_json_set(a, doc, AVP_S("source"), avp_json_strz(a, first->source ? first->source : ""));
        avp_json_set(a, doc, AVP_S("source_sha1"), avp_json_strz(a, first->source_sha1 ? first->source_sha1 : ""));
        avp_json_set(a, doc, AVP_S("from_case"), avp_json_bool(first->from_case));
        if (ncand && !avp_makedirs(tdir, true)) ok = false;
        for (size_t j = i; j < recs->n && ok; j++) {
            const SvRecord *r = recs->v[j];
            if (strcmp(r->function, first->function)) continue;
            for (size_t k = 0; k < r->candidates.n && ok; k++) {
                const SvCandidate *c = &r->candidates.v[k];
                AvpJson *x = avp_json_obj(a);
                avp_json_set(a, x, AVP_S("number"), avp_json_int(a, c->number));
                avp_json_set(a, x, AVP_S("form"), avp_json_strz(a, c->form ? c->form : ""));
                avp_json_set(a, x, AVP_S("description"), avp_json_strz(a, c->desc ? c->desc : ""));
                avp_json_set(a, x, AVP_S("compiled"), avp_json_bool(c->compiled));
                avp_json_set(a, x, AVP_S("exact"), avp_json_bool(c->exact));
                avp_json_set(a, x, AVP_S("rows_before"), avp_json_int(a, c->rows_before));
                avp_json_set(a, x, AVP_S("rows_after"), avp_json_int(a, c->rows_after));
                avp_json_set(a, x, AVP_S("broken"), jlist(a, &c->broken));
                avp_json_set(a, x, AVP_S("rules"), jlist(a, &c->rules));
                if (c->text.n) {
                    const char *rel = avp_sprintf(a, "%s/%d.txt", safe, c->number);
                    avp_json_set(a, x, AVP_S("file"), avp_json_strz(a, rel));
                    if (!avp_write_file(avp_path_join(a, dir, rel), c->text.p, c->text.n)) ok = false;
                }
                avp_json_arr_push(a, cands, x);
            }
            for (size_t k = 0; k < r->probes.n; k++) {
                AvpJson *x = avp_json_obj(a);
                avp_json_set(a, x, AVP_S("number"), avp_json_int(a, r->probes.v[k].number));
                avp_json_set(a, x, AVP_S("lever"), avp_json_strz(a, r->probes.v[k].lever ? r->probes.v[k].lever : ""));
                avp_json_arr_push(a, probes, x);
            }
        }
        avp_json_set(a, doc, AVP_S("candidates"), cands);
        avp_json_set(a, doc, AVP_S("probes"), probes);
        AvpJsonDumpOpts jo = shsm_json_opts();
        AvpStr d = avp_json_dumps(a, doc, &jo);
        if (!avp_write_file(jpath, d.p, d.n)) ok = false;
    }
    avp_set_free(done);
    return ok;
}

/* ---- apply ----------------------------------------------------------------------------------------------------- */

static SvApplied refuse(AvpArena *a, const char *fmt, ...) {
    SvApplied r = {0};
    va_list ap;
    va_start(ap, fmt);
    r.error = avp_vsprintf(a, fmt, ap);
    va_end(ap);
    return r;
}

SvApplied sv_apply(AvpArena *a, const char *root, const char *outdir, const char *function, i64 n) {
    const char *dir = avp_path_join(a, outdir, "apply");
    const char *safe = safe_name(a, function);
    const char *jpath = avp_path_join(a, dir, avp_sprintf(a, "%s.json", safe));
    AvpBytes jb = {0};
    if (!avp_read_all(a, jpath, &jb))
        return refuse(a, "no recorded run of %s in %s (run solver.exe on it first; a run records the candidates it "
                         "compiled)",
                      function, dir);
    AvpJsonError err = {0};
    AvpJson *doc = avp_json_parse(a, avp_strn((const char *)jb.v, jb.n), &err);
    if (!doc) return refuse(a, "%s is not readable", jpath);
    const char *unit = avp_cz(a, avp_json_get_str(doc, "unit", AVP_S("")));
    const char *source = avp_cz(a, avp_json_get_str(doc, "source", AVP_S("")));
    const char *sha = avp_cz(a, avp_json_get_str(doc, "source_sha1", AVP_S("")));
    const AvpJson *cands = avp_json_getz(doc, "candidates"), *probes = avp_json_getz(doc, "probes");
    const AvpJson *c = NULL;
    for (size_t i = 0; cands && i < avp_json_len(cands); i++)
        if (avp_json_get_i64(avp_json_at(cands, i), "number", -1) == n) c = avp_json_at(cands, i);
    if (!c) {
        for (size_t i = 0; probes && i < avp_json_len(probes); i++)
            if (avp_json_get_i64(avp_json_at(probes, i), "number", -1) == n)
                return refuse(a, "#%lld is a probe (%.*s): a lever compiled only to state the condition, never applied",
                              (long long)n, AVP_SF(avp_json_get_str(avp_json_at(probes, i), "lever", AVP_S(""))));
        return refuse(a, "%s has no candidate #%lld in its last run (it recorded %zu candidate(s), %zu probe(s))",
                      function, (long long)n, cands ? avp_json_len(cands) : 0, probes ? avp_json_len(probes) : 0);
    }
    SvApplied r = {0};
    r.unit = unit;
    r.source = source;
    r.form = avp_cz(a, avp_json_get_str(c, "form", AVP_S("")));
    r.desc = avp_cz(a, avp_json_get_str(c, "description", AVP_S("")));
    r.rows_before = avp_json_get_i64(c, "rows_before", -1);
    r.rows_after = avp_json_get_i64(c, "rows_after", -1);
    r.exact = avp_json_get_bool(c, "exact", false);
    if (!avp_json_get_bool(c, "compiled", false))
        return refuse(a, "candidate #%lld did not compile and score: not applied", (long long)n);
    if (!r.exact && !(r.rows_after >= 0 && r.rows_after < r.rows_before))
        return refuse(a, "candidate #%lld is neither exact nor closer (rows %lld -> %lld): not applied", (long long)n,
                      (long long)r.rows_before, (long long)r.rows_after);
    const AvpJson *broken = avp_json_getz(c, "broken"), *rules = avp_json_getz(c, "rules");
    if (broken && avp_json_len(broken))
        return refuse(a, "candidate #%lld breaks %.*s (it closes this function at another's cost): not applied",
                      (long long)n, AVP_SF(avp_json_str_or(avp_json_at(broken, 0), AVP_S(""))));
    if (rules && avp_json_len(rules))
        return refuse(a, "candidate #%lld has a rule finding (%.*s): not applied", (long long)n,
                      AVP_SF(avp_json_str_or(avp_json_at(rules, 0), AVP_S(""))));
    if (avp_json_get_bool(doc, "from_case", false))
        return refuse(a, "the run of %s edited the scratch text (--case): its candidates do not describe the real "
                         "source and are never applied",
                      function);
    if (!*source) return refuse(a, "the recorded run names no source file");
    const char *spath = avp_path_join(a, root, source);
    AvpBytes sb = {0};
    if (!avp_read_all(a, spath, &sb)) return refuse(a, "cannot read %s", spath);
    const char *now = avp_sha1_hex(a, sb.v, sb.n);
    if (strcmp(now, sha))
        return refuse(a, "%s is not the file the run read (sha1 %s, the run's %s): changed since, rerun solver.exe",
                      source, now, sha);
    AvpStr file = avp_json_get_str(c, "file", AVP_S(""));
    AvpBytes tb = {0};
    if (!file.n || !avp_read_all(a, avp_path_join(a, dir, avp_cz(a, file)), &tb) || !tb.n)
        return refuse(a, "the text of candidate #%lld was not recorded", (long long)n);
    if (!avp_write_file(spath, tb.v, tb.n)) return refuse(a, "cannot write %s", spath);
    r.ok = true;
    return r;
}
