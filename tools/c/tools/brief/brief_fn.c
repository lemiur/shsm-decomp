/* brief.py: the function briefing (identity, status, twins, context, shape, listing, start) and build_section. */
#include "brief.h"
#include "decomp_listing.h"
#include "fragments.h"
#include "hashes.h"
#include "query_notes.h"
#include "shsm_mangle.h"
#include "shsm_re.h"
#include "twins.h"
#include "unit_plan.h"
#include "wall_walls.h"
#include "witness_lookup.h"

#include <math.h>
#include <string.h>

static AvpArena *A(void) { return avp_arena(); }
#define J(o, k) avp_json_getz((o), (k))

/* ---- build_section ---- */

BriefSection *brief_build_section(BriefWorld *W, const char *name, BriefGather gather, void *ctx, BriefFmt fmt,
                                  int levels) {
    AvpArena *a = A();
    double t0 = avp_time();
    BriefSection *s = AVP_NEW(a, BriefSection);
    s->name = name;
    s->fmt = fmt;
    s->levels = levels;
    AvpTry t;
    if (AVP_TRY(&t)) {
        s->data = gather(W, ctx);
        avp_try_end(&t);
    } else {
        AvpStr msg = shsm_exc_str(a, t.code, t.msg);
        if (msg.n) {
            AvpStrVec parts = avp_str_splitlines(msg, false);
            msg = parts.n ? parts.v[0] : AVP_S("");
        } else {
            msg = avp_str(shsm_exc_class(t.code));
        }
        s->unavailable = avp_cz(a, brief_clip(msg, 140));
        s->seconds = avp_time() - t0;
        return s;
    }
    s->seconds = avp_time() - t0;
    return s->data ? s : NULL;
}

/* str(e) or type(e).__name__ of a caught exception, clipped. */
static const char *exc_text(const AvpTry *t, i64 n, bool or_class) {
    AvpStr msg = shsm_exc_str(A(), t->code, t->msg);
    if (!msg.n && or_class) msg = avp_str(shsm_exc_class(t->code));
    return avp_cz(A(), brief_clip(msg, n));
}

typedef struct {
    const ShsmSymRow *s;
    const char *unit_name;
    const BriefOpts *o;
    bool wall_run;
    AvpJson *plan; /* {range, run} or {range: None, error} */
    AvpJson *ident, *twins;
} FnCtx;

/* ---- identity ---- */

static AvpJson *gather_identity(BriefWorld *W, void *vc) {
    FnCtx *c = vc;
    AvpArena *a = A();
    const ShsmSymRow *s = c->s;
    const ShsmUnit *u = brief_unit(W, c->unit_name);
    const char *lib = brief_library(W, s->address);
    const char *ref_section = NULL;
    const char *flag = brief_reference(lib, &ref_section);
    BriefScore sc = brief_score(W, s);
    const char *dem = s->demangled.p && s->demangled.n ? avp_cz(a, s->demangled)
                                                       : shsm_demangle(a, avp_cz(a, s->name));
    AvpJson *d = avp_json_obj(a);
    brief_set(d, "name", avp_json_str(a, s->name));
    brief_set(d, "demangled", brief_jz(dem));
    brief_set(d, "address", avp_json_int(a, s->address));
    brief_set(d, "size", brief_ji(s->size));
    brief_set(d, "section", s->section.p ? avp_json_str(a, s->section) : avp_json_null());
    brief_set(d, "library", brief_jz(lib));
    brief_set(d, "configure_library", brief_jz(u ? u->library : NULL));
    brief_set(d, "reference", brief_jz(flag));
    brief_set(d, "reference_section", brief_jz(ref_section));
    brief_set(d, "unit", brief_jz(c->unit_name));
    brief_set(d, "status", brief_jz(u ? u->status : query_status(W->ctx, avp_str(c->unit_name))));
    brief_set(d, "source", brief_jz(u && u->source ? u->source : NULL));
    brief_set(d, "percent", sc.percent ? sc.percent : avp_json_null());
    brief_set(d, "score_origin", brief_jz(sc.origin));
    brief_set(d, "exact", brief_jb(sc.exact));
    i64 line = u && u->source && *u->source ? brief_source_line(W, u->source, s->address, avp_cz(a, s->name))
                                            : SHSM_NONE;
    brief_set(d, "source_line", brief_ji(line));
    brief_set(d, "sibling", brief_sibling_fn(W, s->address, avp_cz(a, s->name), s->size));
    return d;
}

static const char *REF_ADVICE(AvpStr ref) {
    if (avp_str_eqz(ref, "yes")) return ": start from that project's code (reference first)";
    if (avp_str_eqz(ref, "partial")) return ": covers part of the library; check it for this function first";
    if (avp_str_eqz(ref, "inflate")) return ": covers the inflate side only";
    return "";
}

void brief_ref_lines(BriefLines *L, const AvpJson *d, bool with_none);
void brief_ref_lines(BriefLines *L, const AvpJson *d, bool with_none) {
    AvpStr ref = brief_jstr(d, "reference");
    if (ref.p && (avp_str_eqz(ref, "yes") || avp_str_eqz(ref, "partial") || avp_str_eqz(ref, "inflate"))) {
        brief_addf(L, "  public reference: %S (docs/references.md, \"%s\")%s", &ref,
                   brief_py_strz(J(d, "reference_section")), REF_ADVICE(ref));
    } else if (with_none && ref.p && avp_str_eqz(ref, "no")) {
        const AvpJson *rs = J(d, "reference_section");
        brief_addf(L, "  public reference: none (docs/references.md%s)",
                   brief_truthy(rs) ? avp_cz(A(), avp_pyfmt(A(), ", \"%s\"", brief_py_strz(rs))) : "");
    }
}

BriefLines brief_fmt_identity(const BriefSection *sec, int level) {
    (void)level;
    AvpArena *a = A();
    const AvpJson *d = sec->data;
    BriefLines L = {0};
    AvpStr name = brief_jstr(d, "name");
    const AvpJson *dem = J(d, "demangled");
    if (brief_truthy(dem) && !avp_str_eq(dem->u.str, name))
        brief_addf(&L, "%S  {%S}", &name, &dem->u.str);
    else
        brief_add(&L, name);
    const AvpJson *cl = J(d, "configure_library"), *lib = J(d, "library");
    bool show_cl = brief_truthy(cl) && !(avp_json_is_str(lib) && avp_str_eq(cl->u.str, lib->u.str));
    brief_addf(&L, "  %s size 0x%X %s  library %s%s", brief_hex(avp_json_get_i64(d, "address", 0)),
               avp_json_get_i64(d, "size", 0), brief_py_strz(J(d, "section")),
               brief_truthy(lib) ? brief_py_strz(lib) : "?",
               show_cl ? avp_cz(a, avp_pyfmt(a, " (configure.py %s)", brief_py_strz(cl))) : "");
    brief_ref_lines(&L, d, true);
    brief_sibling_fn_lines(&L, J(d, "sibling"));
    if (brief_truthy(J(d, "source"))) {
        const AvpJson *sl = J(d, "source_line");
        AvpStr loc = brief_truthy(sl) ? avp_pyfmt(a, "%s:%d", brief_py_strz(J(d, "source")), avp_json_i64(sl, 0))
                                      : brief_py_str(J(d, "source"));
        brief_addf(&L, "  unit %s  %s  %S", brief_py_strz(J(d, "unit")), brief_py_strz(J(d, "status")), &loc);
        const AvpJson *so = J(d, "score_origin");
        brief_addf(&L, "  score %s (%s)", brief_truthy(J(d, "exact")) ? "exact" : brief_pct(J(d, "percent")),
                   brief_truthy(so) ? brief_py_strz(so) : "?");
    } else {
        brief_addf(&L, "  unit %s (dtk asm unit: not started)", brief_py_strz(J(d, "unit")));
    }
    return L;
}

/* ---- where it stands ---- */

/* start_wall: whether wall.py would be run (compiled, not exact, no current walls.json record). */
static bool wall_wanted(BriefWorld *W, const ShsmSymRow *s, const char *unit_name, bool no_compile) {
    const ShsmUnit *u = brief_source_unit(W, unit_name);
    if (!u || no_compile) return false;
    BriefScore sc = brief_score(W, s);
    return !(sc.exact || brief_wall_entry(W, unit_name, s->name));
}

static const char *last_line(AvpStr text) {
    AvpStrVec parts = avp_str_splitlines(avp_str_strip(text), false);
    if (!parts.n) shsm_raise(SHSM_EXC_VALUE, "list index out of range"); /* IndexError */
    return avp_cz(A(), brief_clip(parts.v[parts.n - 1], 120));
}

/* The wall.py run, as the C port: build/tools/wall.exe <unit> <symbol> --json --root <root>. */
static void run_wall(BriefWorld *W, const ShsmUnit *u, const ShsmSymRow *s, AvpJson *d) {
    AvpArena *a = A();
    AvpTry t;
    if (AVP_TRY(&t)) {
        const char *exe = avp_path_join(a, avp_exe_dir(), "wall.exe");
        const char *argv[] = {exe, u->name, avp_cz(a, s->name), "--json", "--root", W->root, NULL};
        AvpProcSpec spec = {.argv = argv, .cwd = W->root, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE,
                            .text = true, .errors = AVP_ERRORS_STRICT, .timeout = 300};
        AvpProcResult r = avp_run(&spec);
        if (r.status == AVP_PROC_START_FAILED)
            shsm_raise(SHSM_EXC_FILE_NOT_FOUND, "%s", r.message ? r.message : "cannot start wall.exe");
        if (r.status == AVP_PROC_TIMEOUT) {
            AvpBuf b;
            avp_buf_init(&b, a);
            for (size_t i = 0; argv[i]; i++) avp_buf_pyfmt(&b, "%s%r", i ? ", " : "[", argv[i]);
            shsm_raise(SHSM_EXC_OS, "Command '%s]' timed out after 300 seconds", avp_buf_cz(&b));
        }
        AvpJson *j = NULL;
        if (avp_str_startswith(avp_str_strip(r.out), AVP_S("{"))) {
            AvpJsonError err;
            j = avp_json_parse(a, r.out, &err);
            if (!j) shsm_raise(SHSM_EXC_VALUE, "%s", avp_cz(a, avp_json_error_str(a, &err)));
        }
        if (brief_truthy(j)) {
            brief_set(d, "wall", brief_truthy(J(j, "exact")) ? avp_json_strz(a, "EXACT")
                                                             : (J(j, "verdict") ? J(j, "verdict") : avp_json_null()));
            brief_set(d, "wall_origin", avp_json_strz(a, "wall.exe"));
            const AvpJson *p = J(j, "proof");
            if (brief_truthy(p)) {
                const AvpJson *v = brief_truthy(J(p, "line")) ? J(p, "line") : J(p, "verdict");
                brief_set(d, "proof", brief_arr_of(2, v ? (AvpJson *)v : avp_json_null(), avp_json_strz(a, "wall.exe")));
            }
        } else {
            AvpStr txt = r.err.n ? r.err : r.out.n ? r.out : AVP_S("no output");
            brief_set(d, "wall_error", avp_json_strz(a, last_line(txt)));
        }
        avp_try_end(&t);
    } else {
        brief_set(d, "wall_error", avp_json_strz(a, exc_text(&t, 120, true)));
    }
}

static AvpJson *docs_notes(BriefWorld *W, AvpStr term, bool expand) {
    AvpArena *a = A();
    static const char *const LEDGER[] = {"progress.md"};
    QueryNotesOpts o = {.docs_only = true, .exclude = LEDGER, .nexclude = 1, .no_expand = !expand};
    QueryNoteHits hits = query_doc_notes(W->ctx, term, &o, NULL);
    AvpJson *out = avp_json_arr(a);
    for (size_t i = 0; i < hits.n; i++) {
        const QueryNoteHit *h = &hits.v[i];
        avp_json_arr_push(a, out,
                          brief_arr_of(4, avp_json_str(a, h->file), avp_json_int(a, h->line),
                                       h->heading.p ? avp_json_str(a, h->heading) : avp_json_null(),
                                       avp_json_str(a, h->text)));
    }
    avp_vec_free(&hits);
    return out;
}
AvpJson *brief_docs_notes(BriefWorld *W, AvpStr term, bool expand);
AvpJson *brief_docs_notes(BriefWorld *W, AvpStr term, bool expand) { return docs_notes(W, term, expand); }

static AvpJson *gather_status(BriefWorld *W, void *vc) {
    FnCtx *c = vc;
    AvpArena *a = A();
    const ShsmSymRow *s = c->s;
    AvpJson *d = avp_json_obj(a);
    brief_set(d, "wall", avp_json_null());
    brief_set(d, "wall_origin", avp_json_null());
    brief_set(d, "banner", avp_json_arr(a));
    brief_set(d, "docs", avp_json_arr(a));
    const ShsmUnit *u = brief_source_unit(W, c->unit_name);
    BriefScore sc = brief_score(W, s);
    if (u && !sc.exact) {
        const AvpJson *e = brief_wall_entry(W, c->unit_name, s->name);
        if (e) {
            brief_set(d, "wall", J(e, "verdict") ? J(e, "verdict") : avp_json_null());
            brief_set(d, "wall_origin", avp_json_strz(a, "build/walls.json"));
            const AvpJson *p = J(e, "proof");
            if (brief_truthy(p)) {
                const AvpJson *v = brief_truthy(J(p, "line")) ? J(p, "line") : J(p, "verdict");
                brief_set(d, "proof",
                          brief_arr_of(2, v ? (AvpJson *)v : avp_json_null(), avp_json_strz(a, "build/walls.json")));
            }
        } else if (c->wall_run) {
            run_wall(W, u, s, d);
        } else {
            brief_set(d, "wall_error",
                      avp_json_strz(a, "not classified (--no-compile, and build/walls.json has no current record)"));
        }
        if (!J(d, "proof")) {
            const AvpJson *q = brief_eqcheck_entry(W, c->unit_name, s->name);
            if (q && brief_truthy(J(q, "line")))
                brief_set(d, "proof", brief_arr_of(2, J(q, "line"), avp_json_strz(a, "build/eqcheck.json")));
        }
        const AvpJson *wv = J(d, "wall");
        if (J(d, "proof") && brief_truthy(wv) && avp_json_is_str(wv) &&
            avp_str_startswith(wv->u.str, AVP_S("EQUIVALENT (proved")))
            avp_json_del(d, AVP_S("proof"));
    }
    if (u) {
        AvpTry t;
        if (AVP_TRY(&t)) {
            WallNote *notes;
            size_t n = wall_find_notes(a, avp_path_join(a, W->root, u->source), &notes);
            AvpJson *banner = J(d, "banner");
            for (size_t i = 0; i < n; i++)
                if (notes[i].has_addr && (i64)notes[i].addr == s->address)
                    avp_json_arr_push(a, banner, brief_arr_of(3, avp_json_strz(a, u->source),
                                                              avp_json_int(a, notes[i].line),
                                                              avp_json_str(a, notes[i].text)));
            avp_try_end(&t);
        } else {
            brief_set(d, "banner_error", avp_json_strz(a, exc_text(&t, 100, false)));
        }
    }
    brief_set(d, "docs", docs_notes(W, s->name, true));
    if (!(brief_truthy(J(d, "wall")) || brief_truthy(J(d, "wall_error")) || J(d, "proof") ||
          avp_json_len(J(d, "banner")) || avp_json_len(J(d, "docs"))))
        return NULL;
    return d;
}

static AvpStr first_nonblank(AvpStr text) {
    AvpStrVec parts = avp_str_split(text, AVP_S("\n"), -1);
    AvpStr r = AVP_S("");
    for (size_t i = 0; i < parts.n; i++)
        if (avp_str_strip(parts.v[i]).n) {
            r = parts.v[i];
            break;
        }
    avp_vec_free(&parts);
    return r;
}

/* docs passages: "  file:line [heading]: first line" */
void brief_docs_lines(BriefLines *L, const AvpJson *docs, i64 n, i64 clip_text, const char *more);
void brief_docs_lines(BriefLines *L, const AvpJson *docs, i64 n, i64 clip_text, const char *more) {
    AvpArena *a = A();
    size_t k, shown = brief_cap(avp_json_len(docs), n, &k);
    for (size_t i = 0; i < shown; i++) {
        const AvpJson *h = avp_json_at(docs, i);
        const AvpJson *head = avp_json_at(h, 2);
        AvpStr first = first_nonblank(avp_json_at(h, 3)->u.str);
        AvpStr hc = brief_truthy(head) ? brief_clip(head->u.str, 40) : AVP_S("");
        AvpStr hd = brief_truthy(head) ? avp_pyfmt(a, " [%S]", &hc) : AVP_S("");
        AvpStr ft = brief_clip(first, clip_text);
        brief_addf(L, "  %s:%d%S: %S", brief_py_strz(avp_json_at(h, 0)), avp_json_i64(avp_json_at(h, 1), 0), &hd,
                   &ft);
    }
    brief_more(L, k, more);
}

BriefLines brief_fmt_status(const BriefSection *sec, int level) {
    AvpArena *a = A();
    const AvpJson *d = sec->data;
    BriefLines L = {0};
    if (brief_truthy(J(d, "wall"))) {
        AvpStr w = brief_clip(brief_py_str(J(d, "wall")), 150);
        brief_addf(&L, "  wall (%s): %S", brief_py_strz(J(d, "wall_origin")), &w);
    } else if (brief_truthy(J(d, "wall_error"))) {
        brief_addf(&L, "  wall: unavailable: %s", brief_py_strz(J(d, "wall_error")));
    }
    const AvpJson *p = J(d, "proof");
    if (p) {
        AvpStr t = brief_clip(brief_py_str(avp_json_at(p, 0)), 150);
        brief_addf(&L, "  eqcheck (%s): %S", brief_py_strz(avp_json_at(p, 1)), &t);
    }
    static const i64 caps[] = {4, 3, 2, 1};
    i64 n = brief_lvl(level, caps, 4);
    const AvpJson *banner = J(d, "banner");
    size_t k, shown = brief_cap(avp_json_len(banner), n, &k);
    for (size_t i = 0; i < shown; i++) {
        const AvpJson *b = avp_json_at(banner, i);
        AvpStr t = brief_clip(avp_json_at(b, 2)->u.str, 100);
        brief_addf(&L, "  %s:%d: %S", brief_py_strz(avp_json_at(b, 0)), avp_json_i64(avp_json_at(b, 1), 0), &t);
    }
    brief_more(&L, k, "more source notes");
    AvpStr term = J(d, "term") ? brief_py_str(J(d, "term")) : AVP_S("<symbol>");
    brief_docs_lines(&L, J(d, "docs"), n, 90,
                     avp_cz(a, avp_pyfmt(a, "more docs passages (build/tools/query.exe notes %S)", &term)));
    return L;
}

/* ---- twins ---- */

static AvpJson *twin_record(BriefWorld *W, const BriefTwinRow *r) {
    AvpArena *a = A();
    const char *un = r->unit && *r->unit && strncmp(r->unit, "main/", 5) != 0 ? avp_sprintf(a, "main/%s", r->unit)
                                                                             : r->unit;
    bool m = brief_matched(W, r->address, un);
    AvpJson *rec = avp_json_obj(a);
    brief_set(rec, "address", avp_json_int(a, r->address));
    brief_set(rec, "name", brief_jz(r->name));
    brief_set(rec, "unit", brief_jz(un));
    brief_set(rec, "size", brief_ji(r->size));
    brief_set(rec, "matched", brief_jb(m));
    brief_set(rec, "library", brief_jz(brief_library(W, r->address)));
    if (m) {
        const ShsmUnit *u = brief_source_unit(W, un);
        brief_set(rec, "source", avp_json_strz(a, u->source));
        brief_set(rec, "status", brief_jz(u->status));
        brief_set(rec, "line", brief_ji(brief_source_line(W, u->source, r->address, r->name ? r->name : "None")));
    }
    return rec;
}

static AvpJson *gather_twins(BriefWorld *W, void *vc) {
    FnCtx *c = vc;
    AvpArena *a = A();
    const ShsmSymRow *s = c->s;
    AvpJson *d = avp_json_obj(a);
    if ((s->size == SHSM_NONE ? 0 : s->size) < BRIEF_TWIN_MIN_SIZE) {
        brief_set(d, "too_small", avp_json_bool(true));
        return d;
    }
    const char *fp[3];
    if (!brief_twins_fps(W, s->address, fp))
        shsm_raise(SHSM_EXC_VALUE, "%.*s is not in build/index/twins.sqlite", AVP_SF(s->name));
    AvpJson *levels = avp_json_obj(a);
    AvpISet *seen = avp_iset_new();
    avp_iset_add(seen, (u64)s->address);
    bool any = false;
    for (int lv = 0; lv < 3; lv++) {
        AvpJson *recs = avp_json_arr(a);
        BriefTwinRows rows = brief_twins_rows(W, lv, fp[lv], true, 0);
        for (size_t i = 0; i < rows.n; i++)
            if (avp_iset_add(seen, (u64)rows.v[i].address)) avp_json_arr_push(a, recs, twin_record(W, &rows.v[i]));
        avp_vec_free(&rows);
        any = any || avp_json_len(recs);
        brief_set(levels, TWINS_LEVELS[lv], recs);
    }
    if (!any) return NULL;
    brief_set(d, "too_small", avp_json_bool(false));
    brief_set(d, "levels", levels);
    brief_set(d, "library", brief_jz(brief_library(W, s->address)));
    return d;
}

static const char *TWIN_GLOSS[] = {"same code, other referents", "identical", "same shape, other offsets"};

BriefLines brief_fmt_twins(const BriefSection *sec, int level) {
    AvpArena *a = A();
    const AvpJson *d = sec->data;
    BriefLines L = {0};
    if (brief_truthy(J(d, "too_small"))) {
        brief_addf(&L, "  under %d bytes: twins carry no information here", BRIEF_TWIN_MIN_SIZE);
        return L;
    }
    const AvpJson *levels = J(d, "levels");
    AVP_VEC(const AvpJson *) matched = {0};
    AVP_VEC(int) mlv = {0};
    for (int lv = 0; lv < 3; lv++) {
        const AvpJson *recs = J(levels, TWINS_LEVELS[lv]);
        for (size_t i = 0; i < avp_json_len(recs); i++)
            if (brief_truthy(J(avp_json_at(recs, i), "matched"))) {
                avp_vec_push(&matched, avp_json_at(recs, i));
                avp_vec_push(&mlv, lv);
            }
    }
    static const i64 caps[] = {4, 2, 1};
    size_t k, shown = brief_cap(matched.n, brief_lvl(level, caps, 3), &k);
    const AvpJson *dlib = J(d, "library");
    for (size_t i = 0; i < shown; i++) {
        const AvpJson *r = matched.v[i], *rl = J(r, "library");
        bool other = brief_truthy(rl) && brief_truthy(dlib) && !avp_str_eq(rl->u.str, dlib->u.str);
        const AvpJson *line = J(r, "line");
        AvpStr ln = brief_truthy(line) ? avp_pyfmt(a, ":%d", avp_json_i64(line, 0)) : AVP_S("");
        AvpStr lib = other ? avp_pyfmt(a, ", library %s", brief_py_strz(rl)) : AVP_S("");
        brief_addf(&L, "  matched %s twin (%s): %s  read %s%S (%s%S)", TWINS_LEVELS[mlv.v[i]], TWIN_GLOSS[mlv.v[i]],
                   brief_py_strz(J(r, "name")), brief_py_strz(J(r, "source")), &ln, brief_py_strz(J(r, "status")),
                   &lib);
    }
    brief_more(&L, k, "more matched twins");
    AvpStrVec fam = {0};
    for (int lv = 0; lv < 3; lv++) {
        const AvpJson *recs = J(levels, TWINS_LEVELS[lv]);
        size_t n = avp_json_len(recs), un = 0;
        if (!n) continue;
        for (size_t i = 0; i < n; i++) un += !brief_truthy(J(avp_json_at(recs, i), "matched"));
        avp_vec_push(&fam, avp_pyfmt(a, "%s %d (%d unmatched)", TWINS_LEVELS[lv], n, un));
    }
    if (fam.n) {
        AvpStr j = avp_str_join(a, AVP_S(", "), fam.v, fam.n);
        AvpStr nm = J(d, "name") ? brief_py_str(J(d, "name")) : AVP_S("<symbol>");
        brief_addf(&L, "  other copies: %S  (build/tools/twins.exe of %S)", &j, &nm);
    }
    avp_vec_free(&matched);
    avp_vec_free(&mlv);
    avp_vec_free(&fam);
    return L;
}

/* ---- context ---- */

static AvpStr lower_str_of(const AvpJson *v, bool missing_empty) {
    if (!v) return missing_empty ? AVP_S("") : AVP_S("none");
    return avp_str_lower(A(), brief_py_str(v));
}

static int cmp_str(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return avp_str_cmp(*(const AvpStr *)x, *(const AvpStr *)y);
}

static AvpJson *class_roles(BriefWorld *W, i64 address) {
    AvpArena *a = A();
    AvpStr tag = avp_str_lower(a, avp_str(brief_hex(address)));
    AvpJson *out = avp_json_arr(a);
    const AvpJson *cls = J(brief_classes(W), "classes");
    for (size_t ci = 0; cls && ci < avp_json_len(cls); ci++) {
        const AvpJson *c = avp_json_at(cls, ci);
        AvpStrVec roles = {0};
        bool any_role = false;
        const AvpJson *members = J(c, "members");
        for (size_t i = 0; members && i < avp_json_len(members); i++) {
            const AvpJson *m = avp_json_at(members, i);
            if (!avp_str_eq(lower_str_of(J(m, "address"), true), tag)) continue;
            any_role = true;
            const AvpJson *r = J(m, "role");
            if (brief_truthy(r)) {
                AvpStr rs = brief_py_str(r);
                bool dup = false;
                for (size_t k = 0; k < roles.n; k++) dup = dup || avp_str_eq(roles.v[k], rs);
                if (!dup) avp_vec_push(&roles, rs);
            }
        }
        const AvpJson *vt = J(c, "vtable");
        const AvpJson *words = brief_truthy(vt) ? J(vt, "words") : NULL;
        AvpJson *slots = avp_json_arr(a), *status = avp_json_arr(a);
        for (size_t i = 0; words && i < avp_json_len(words); i++) {
            const AvpJson *w = avp_json_at(words, i);
            if (!avp_str_eq(lower_str_of(J(w, "target_address"), true), tag)) continue;
            avp_json_arr_push(a, slots, brief_arr_of(2, J(w, "index") ? J(w, "index") : avp_json_null(),
                                                     J(w, "offset") ? J(w, "offset") : avp_json_null()));
            avp_json_arr_push(a, status, J(w, "status") ? J(w, "status") : avp_json_null());
        }
        if (!any_role && !avp_json_len(slots)) {
            avp_vec_free(&roles);
            continue;
        }
        avp_sort(roles.v, roles.n, sizeof(AvpStr), cmp_str, NULL, false);
        AvpJson *o = avp_json_obj(a), *jr = avp_json_arr(a);
        for (size_t k = 0; k < roles.n; k++) avp_json_arr_push(a, jr, avp_json_str(a, roles.v[k]));
        const AvpJson *name = J(c, "name");
        brief_set(o, "class", name ? (AvpJson *)name : avp_json_null());
        brief_set(o, "name_source", J(c, "name_source") ? J(c, "name_source") : avp_json_null());
        brief_set(o, "roles", jr);
        brief_set(o, "vtable", J(c, "vtable_symbol") ? J(c, "vtable_symbol") : avp_json_null());
        brief_set(o, "slots", slots);
        brief_set(o, "status", status);
        const char *header = NULL;
        if (brief_truthy(name)) {
            AvpTry t;
            if (AVP_TRY(&t)) {
                const char *mg = shsm_mangle(a, "x", brief_py_strz(name), NULL, 0, false, false);
                header = brief_header_of(W, mg);
                avp_try_end(&t);
            } else {
                header = NULL;
            }
        }
        brief_set(o, "header", brief_jz(header));
        avp_json_arr_push(a, out, o);
        avp_vec_free(&roles);
    }
    return out;
}

static void data_value(BriefWorld *W, AvpStr sym, const char **desc, const char **val) {
    AvpArena *a = A();
    *desc = *val = NULL;
    const ShsmSymRow *r = query_sym(W->ctx, sym);
    if (!r) return;
    bool has_size = r->size != SHSM_NONE && r->size != 0;
    AvpStr sec = r->section.p && r->section.n ? r->section : AVP_S("?");
    AvpStr bytes = has_size ? avp_pyfmt(a, "%d bytes", r->size) : AVP_S("");
    *desc = avp_cz(a, avp_str_strip(avp_pyfmt(a, "%S %S", &sec, &bytes)));
    if (r->section.p && (avp_str_eqz(r->section, ".bss") || avp_str_eqz(r->section, ".sbss") ||
                         avp_str_eqz(r->section, ".sbss2"))) {
        *val = "zero-initialised";
        return;
    }
    const QueryDol *dol = has_size && r->size <= 8 ? query_dol(W->ctx) : NULL;
    if (!dol) return;
    AvpStr b = query_dol_read(a, dol, r->address, r->size);
    if (!b.p || !b.n) return;
    u64 v = 0;
    for (size_t i = 0; i < b.n; i++) v = v << 8 | (u8)b.p[i];
    if (r->size == 4 && v >= 0x80000000u && v < 0x81800000u) {
        *val = avp_sprintf(a, "&%s", shsm_sym_describe(a, query_syms(W->ctx), v));
        return;
    }
    if (r->size == 4 && r->section.p && (avp_str_eqz(r->section, ".sdata2") || avp_str_eqz(r->section, ".rodata"))) {
        u32 w = (u32)v;
        float f;
        memcpy(&f, &w, 4);
        double fd = f;
        if (fd == fd && 1e-6 < fabs(fd) && fabs(fd) < 1e9) {
            char rep[32];
            avp_float_repr(fd, rep);
            *val = avp_cz(a, avp_pyfmt(a, "0x%08X (%sf)", (i64)v, rep));
            return;
        }
    }
    AvpBuf hx;
    avp_buf_init(&hx, a);
    avp_buf_putz(&hx, "0x");
    for (size_t i = 0; i < b.n; i++) avp_buf_printf(&hx, "%02X", (u8)b.p[i]);
    *val = avp_buf_cz(&hx);
}

static bool is_helper(AvpStr n) {
    static const char *const P[] = {"_savegpr_", "_restgpr_", "_savefpr_", "_restfpr_", "__save", "__rest"};
    for (size_t i = 0; i < 6; i++)
        if (avp_str_startswith(n, avp_str(P[i]))) return true;
    return false;
}
bool brief_is_helper(AvpStr n);
bool brief_is_helper(AvpStr n) { return is_helper(n); }

static AvpJson *gather_context(BriefWorld *W, void *vc) {
    FnCtx *c = vc;
    AvpArena *a = A();
    const ShsmSymRow *s = c->s;
    AvpJson *d = avp_json_obj(a);
    const char *keys[] = {"classes", "callers", "callees", "strings", "floats", "data", "errors"};
    for (size_t i = 0; i < 7; i++) brief_set(d, keys[i], avp_json_arr(a));
    brief_set(d, "symbol", avp_json_str(a, s->name));
    AvpTry t;
    if (AVP_TRY(&t)) {
        brief_set(d, "classes", class_roles(W, s->address));
        avp_try_end(&t);
    } else {
        avp_json_arr_push(a, J(d, "errors"),
                          avp_json_str(a, avp_str_concat(a, AVP_S("classes: "),
                                                         brief_clip(shsm_exc_str(a, t.code, t.msg), 90))));
    }
    ShsmAsmIndex *ix = brief_ix(W);
    ShsmCallers callers = shsm_asmindex_callers(a, ix, shsm_target_name(s->name), true);
    AvpMap *groups = avp_map_new();
    AVP_VEC(AvpJson *) gl = {0};
    AVP_VEC(AvpSet *) kinds = {0};
    for (size_t i = 0; i < callers.n; i++) {
        const ShsmCaller *cr = &callers.v[i];
        AvpStr key = cr->name.p && cr->name.n ? cr->name : avp_pyfmt(a, "@%08X", cr->site);
        bool created;
        AvpVal *v = avp_map_put(groups, key, &created);
        if (created) {
            AvpJson *g = avp_json_obj(a);
            brief_set(g, "name", cr->name.p ? avp_json_str(a, cr->name) : avp_json_null());
            brief_set(g, "address", avp_json_int(a, cr->site));
            brief_set(g, "kinds", avp_json_arr(a));
            brief_set(g, "sites", avp_json_int(a, 0));
            v->i = (i64)gl.n;
            avp_vec_push(&gl, g);
            avp_vec_push(&kinds, avp_set_new());
        }
        AvpJson *g = gl.v[v->i];
        avp_set_add(kinds.v[v->i], avp_str(cr->kind));
        J(g, "sites")->u.i += 1;
    }
    AvpJson *jcallers = J(d, "callers");
    for (size_t i = 0; i < gl.n; i++) {
        AvpJson *g = gl.v[i];
        AvpStr gname = brief_jstr(g, "name");
        ShsmSymRow r;
        bool has_r = gname.p && gname.n && shsm_asmindex_symbol(a, ix, gname, &r);
        const char *unit = has_r ? brief_unit_name_of(&r)
                                 : shsm_asmindex_unit_of(a, ix, avp_json_get_i64(g, "address", 0));
        brief_set(g, "unit", brief_jz(unit));
        brief_set(g, "matched", brief_jb(has_r && brief_matched(W, r.address, brief_unit_name_of(&r))));
        AvpStrVec ks = avp_set_sorted(kinds.v[i]);
        AvpJson *jk = J(g, "kinds");
        for (size_t k = 0; k < ks.n; k++) avp_json_arr_push(a, jk, avp_json_str(a, ks.v[k]));
        avp_json_arr_push(a, jcallers, g);
    }
    ShsmCallees callees = shsm_asmindex_callees(a, ix, shsm_target_name(s->name));
    AvpJson *jcallees = J(d, "callees");
    for (size_t i = 0; i < callees.n; i++) {
        const ShsmCallee *ce = &callees.v[i];
        if (is_helper(ce->name)) continue;
        ShsmSymRow r;
        bool has_r = shsm_asmindex_symbol(a, ix, ce->name, &r);
        const char *un = has_r ? brief_unit_name_of(&r) : NULL;
        bool m = has_r && brief_matched(W, r.address, un);
        AvpJson *o = avp_json_obj(a);
        brief_set(o, "name", avp_json_str(a, ce->name));
        brief_set(o, "kind", brief_jz(ce->kind));
        brief_set(o, "count", avp_json_int(a, ce->count));
        brief_set(o, "unit", brief_jz(un));
        brief_set(o, "matched", brief_jb(m));
        brief_set(o, "header", brief_jz(m ? brief_header_of(W, avp_cz(a, ce->name)) : NULL));
        brief_set(o, "source", brief_jz(m ? brief_source_unit(W, un)->source : NULL));
        avp_json_arr_push(a, jcallees, o);
    }
    WtLookup wl;
    AvpJson *wt = avp_json_null();
    if (AVP_TRY(&t)) {
        if (wt_lookup(a, W->repo, s->address, 3, &wl)) {
            wt = avp_json_obj(a);
            brief_set(wt, "ps2", brief_ji(wl.ps2 == WT_NONE ? SHSM_NONE : wl.ps2));
            brief_set(wt, "tier", brief_jz(wl.tier));
            brief_set(wt, "evidence", brief_jz(wl.evidence));
            AvpJson *hs = avp_json_arr(a);
            for (size_t i = 0; i < wl.helpers.n; i++) {
                AvpJson *h = avp_json_obj(a);
                brief_set(h, "helper", avp_json_int(a, wl.helpers.v[i].helper));
                brief_set(h, "size", brief_ji(wl.helpers.v[i].size == WT_NONE ? SHSM_NONE : wl.helpers.v[i].size));
                brief_set(h, "wii_copy", brief_jz(wl.helpers.v[i].wii_copy));
                avp_json_arr_push(a, hs, h);
            }
            brief_set(wt, "helpers", hs);
            brief_set(wt, "more", avp_json_int(a, wl.more));
            avp_vec_free(&wl.helpers);
        }
        avp_try_end(&t);
    }
    brief_set(d, "witness", wt);
    AvpSet *skip = avp_set_new();
    ShsmStrRows strs = shsm_asmindex_strings_of(a, ix, shsm_target_name(s->name), false);
    AvpJson *jstrs = avp_json_arr(a);
    for (size_t i = 0; i < strs.n; i++) {
        AvpJson *o = avp_json_obj(a);
        brief_set(o, "object", strs.v[i].owner.p ? avp_json_str(a, strs.v[i].owner) : avp_json_null());
        brief_set(o, "address", avp_json_int(a, strs.v[i].address));
        brief_set(o, "text", avp_json_str(a, strs.v[i].text));
        avp_json_arr_push(a, jstrs, o);
        if (strs.v[i].owner.p) avp_set_add(skip, strs.v[i].owner);
    }
    brief_set(d, "strings", jstrs);
    ShsmFloatRows fls = shsm_asmindex_floats_of(a, ix, shsm_target_name(s->name));
    AvpJson *jfl = avp_json_arr(a);
    for (size_t i = 0; i < fls.n; i++) {
        AvpJson *o = avp_json_obj(a);
        brief_set(o, "object", fls.v[i].owner.p ? avp_json_str(a, fls.v[i].owner) : avp_json_null());
        brief_set(o, "address", avp_json_int(a, fls.v[i].address));
        brief_set(o, "kind", fls.v[i].kind.p ? avp_json_str(a, fls.v[i].kind) : avp_json_null());
        brief_set(o, "value", fls.v[i].has_value ? avp_json_float(a, fls.v[i].value) : avp_json_null());
        avp_json_arr_push(a, jfl, o);
        if (fls.v[i].owner.p) avp_set_add(skip, fls.v[i].owner);
    }
    brief_set(d, "floats", jfl);
    ShsmDataRefs refs = shsm_asmindex_data_refs(a, ix, shsm_target_name(s->name));
    AvpJson *jd = J(d, "data");
    for (size_t i = 0; i < refs.n; i++) {
        AvpStr sym = refs.v[i].sym;
        if (avp_set_has(skip, sym) || avp_str_eq(sym, s->name)) continue;
        const char *desc, *val;
        data_value(W, sym, &desc, &val);
        AvpJson *o = avp_json_obj(a);
        brief_set(o, "symbol", avp_json_str(a, sym));
        brief_set(o, "offset", brief_ji(refs.v[i].offset));
        brief_set(o, "desc", brief_jz(desc));
        brief_set(o, "value", brief_jz(val));
        avp_json_arr_push(a, jd, o);
    }
    for (size_t i = 0; i < kinds.n; i++) avp_set_free(kinds.v[i]);
    avp_vec_free(&kinds);
    avp_vec_free(&gl);
    avp_map_free(groups);
    avp_set_free(skip);
    return d;
}

static AvpStr fmt_float(const AvpJson *v) {
    if (!v || !avp_json_is_num(v)) return brief_py_str(v);
    double f = avp_json_f64(v, 0.0);
    if (fabs(f) < 1e7) {
        char rep[32];
        avp_float_repr(f, rep);
        return avp_str_dup(A(), avp_str(rep));
    }
    return avp_pyfmt(A(), "%g", f);
}

static const char *role_text(AvpStr r) {
    if (avp_str_eqz(r, "slot")) return "virtual";
    if (avp_str_eqz(r, "mangled")) return "member";
    if (avp_str_eqz(r, "ctor")) return "constructor";
    if (avp_str_eqz(r, "dtor")) return "destructor";
    return avp_cz(A(), r);
}

static int cmp_unmatched_first(const void *x, const void *y, void *ctx) {
    (void)ctx;
    bool mx = brief_truthy(J(*(const AvpJson *const *)x, "matched"));
    bool my = brief_truthy(J(*(const AvpJson *const *)y, "matched"));
    return (int)!mx - (int)!my;
}

typedef AVP_VEC(const AvpJson *) JsonPtrs;
static JsonPtrs sorted_matched_first(const AvpJson *arr) {
    JsonPtrs v = {0};
    for (size_t i = 0; i < avp_json_len(arr); i++) avp_vec_push(&v, avp_json_at(arr, i));
    avp_sort(v.v, v.n, sizeof(v.v[0]), cmp_unmatched_first, NULL, false);
    return v;
}

static const char *unit_path_or_q(const AvpJson *u) {
    return brief_truthy(u) ? shsm_unit_path(A(), brief_py_strz(u)) : "?";
}

BriefLines brief_fmt_context(const BriefSection *sec, int level) {
    AvpArena *a = A();
    const AvpJson *d = sec->data;
    BriefLines L = {0};
    static const i64 caps[5][5] = {
        {5, 8, 6, 6, 6}, {3, 5, 4, 4, 4}, {2, 3, 2, 2, 3}, {1, 2, 1, 1, 2}, {0, 0, 0, 0, 1}};
    i64 nc = SHSM_NONE, ne = SHSM_NONE, ns = SHSM_NONE, nd = SHSM_NONE, ncls = SHSM_NONE;
    if (level >= 0) {
        const i64 *cp = caps[level < 5 ? level : 4];
        nc = cp[0], ne = cp[1], ns = cp[2], nd = cp[3], ncls = cp[4];
    }
    const AvpJson *cls = J(d, "classes");
    size_t cls_hidden, cls_shown = brief_cap(avp_json_len(cls), ncls, &cls_hidden);
    for (size_t i = 0; i < cls_shown; i++) {
        const AvpJson *c = avp_json_at(cls, i);
        AvpStrVec sl = {0}, rl = {0};
        const AvpJson *slots = J(c, "slots");
        for (size_t k = 0; k < avp_json_len(slots); k++) {
            const AvpJson *p = avp_json_at(slots, k);
            avp_vec_push(&sl, avp_pyfmt(a, "slot %s at %s", brief_py_strz(avp_json_at(p, 0)),
                                        brief_py_strz(avp_json_at(p, 1))));
        }
        const AvpJson *roles = J(c, "roles");
        for (size_t k = 0; k < avp_json_len(roles); k++)
            avp_vec_push(&rl, avp_str(role_text(avp_json_at(roles, k)->u.str)));
        AvpStr slot = avp_str_join(a, AVP_S(", "), sl.v, sl.n);
        AvpStr rt = avp_str_join(a, AVP_S(", "), rl.v, rl.n);
        if (!rt.n) rt = AVP_S("vtable target");
        AvpStr nsrc = brief_jstr(c, "name_source");
        AvpStr vt = slot.n ? avp_pyfmt(a, "; vtable %s %S", brief_py_strz(J(c, "vtable")), &slot) : AVP_S("");
        AvpStr hd = brief_truthy(J(c, "header")) ? avp_pyfmt(a, "; declared in <%s>", brief_py_strz(J(c, "header")))
                                                 : AVP_S("");
        const char *cname = brief_py_strz(J(c, "class"));
        bool placeholder = nsrc.p && avp_str_startswith(nsrc, AVP_S("placeholder"));
        brief_addf(&L, "  class %s%s: %S%S%S  (build/tools/classes.exe show %s)", cname,
                   placeholder ? " [placeholder]" : "", &rt, &vt, &hd, cname);
        avp_vec_free(&sl);
        avp_vec_free(&rl);
    }
    brief_more(&L, cls_hidden, "more classes sharing the vtable slot (--full lists them all)");
    const AvpJson *callers = J(d, "callers");
    if (!avp_json_len(callers)) {
        brief_addz(&L, "  callers: none (no call, branch or pointer names it)");
    } else {
        size_t m = 0;
        for (size_t i = 0; i < avp_json_len(callers); i++) m += brief_truthy(J(avp_json_at(callers, i), "matched"));
        brief_addf(&L, "  callers: %d (%d matched)", avp_json_len(callers), m);
        JsonPtrs v = sorted_matched_first(callers);
        size_t k, shown = brief_cap(v.n, nc, &k);
        for (size_t i = 0; i < shown; i++) {
            const AvpJson *g = v.v[i], *kinds = J(g, "kinds");
            AvpStr name = brief_truthy(J(g, "name")) ? brief_py_str(J(g, "name"))
                                                     : avp_pyfmt(a, "@%08X", avp_json_get_i64(g, "address", 0));
            bool only_call = avp_json_len(kinds) == 1 && avp_str_eqz(avp_json_at(kinds, 0)->u.str, "call");
            AvpStrVec ks = {0};
            for (size_t j = 0; j < avp_json_len(kinds); j++) avp_vec_push(&ks, avp_json_at(kinds, j)->u.str);
            AvpStr kj = avp_str_join(a, AVP_S(","), ks.v, ks.n);
            AvpStr kt = only_call ? AVP_S("") : avp_pyfmt(a, "  (%S)", &kj);
            brief_addf(&L, "    %s %S  %s%S", brief_truthy(J(g, "matched")) ? "*" : " ", &name,
                       unit_path_or_q(J(g, "unit")), &kt);
            avp_vec_free(&ks);
        }
        brief_more(&L, nc != 0 ? k : 0, "more callers");
        avp_vec_free(&v);
    }
    const AvpJson *callees = J(d, "callees");
    if (avp_json_len(callees)) {
        size_t m = 0;
        for (size_t i = 0; i < avp_json_len(callees); i++) m += brief_truthy(J(avp_json_at(callees, i), "matched"));
        brief_addf(&L, "  callees: %d (%d matched; * = matched)", avp_json_len(callees), m);
        JsonPtrs v = sorted_matched_first(callees);
        size_t k, shown = brief_cap(v.n, ne, &k);
        for (size_t i = 0; i < shown; i++) {
            const AvpJson *c = v.v[i];
            bool matched = brief_truthy(J(c, "matched"));
            AvpStr where;
            if (brief_truthy(J(c, "header")))
                where = avp_pyfmt(a, "#include <%s>", brief_py_strz(J(c, "header")));
            else if (matched)
                where = avp_pyfmt(a, "no header declares it; defined in %s", brief_py_strz(J(c, "source")));
            else
                where = avp_str(unit_path_or_q(J(c, "unit")));
            i64 count = avp_json_get_i64(c, "count", 0);
            AvpStr cnt = count > 1 ? avp_pyfmt(a, " x%d", count) : AVP_S("");
            brief_addf(&L, "    %s %s%S  %S", matched ? "*" : " ", brief_py_strz(J(c, "name")), &cnt, &where);
        }
        brief_more(&L, ne != 0 ? k : 0, "more callees");
        avp_vec_free(&v);
    }
    const AvpJson *wt = J(d, "witness");
    if (brief_truthy(wt)) {
        AvpStrVec parts = {0};
        const AvpJson *ps2 = J(wt, "ps2");
        avp_vec_push(&parts, avp_pyfmt(a, "PS2 counterpart %s",
                                       avp_json_is_null(ps2)
                                           ? "not mapped"
                                           : avp_cz(a, avp_pyfmt(a, "0x%06X (%s)", avp_json_i64(ps2, 0),
                                                                 brief_py_strz(J(wt, "tier"))))));
        const AvpJson *hs = J(wt, "helpers");
        if (brief_truthy(hs)) {
            AvpStrVec hl = {0};
            for (size_t i = 0; i < avp_json_len(hs); i++) {
                const AvpJson *h = avp_json_at(hs, i);
                AvpStr sz = brief_truthy(J(h, "size")) ? avp_pyfmt(a, " 0x%X bytes", avp_json_get_i64(h, "size", 0))
                                                       : AVP_S("");
                AvpStr wc = brief_truthy(J(h, "wii_copy")) ? avp_pyfmt(a, " = %s", brief_py_strz(J(h, "wii_copy")))
                                                           : AVP_S("");
                avp_vec_push(&hl, avp_pyfmt(a, "0x%06X%S%S", avp_json_get_i64(h, "helper", 0), &sz, &wc));
            }
            AvpStr hj = avp_str_join(a, AVP_S(", "), hl.v, hl.n);
            i64 more = avp_json_get_i64(wt, "more", 0);
            AvpStr mo = more ? avp_pyfmt(a, " +%d", more) : AVP_S("");
            avp_vec_push(&parts, avp_pyfmt(a, "inlined here, out of line on PS2: %S%S", &hj, &mo));
            avp_vec_free(&hl);
        }
        AvpStr pj = avp_str_join(a, AVP_S("; "), parts.v, parts.n);
        AvpStr sym = J(d, "symbol") ? brief_py_str(J(d, "symbol")) : AVP_S("<fn>");
        brief_addf(&L, "  %S  (build/tools/witness.exe of %S; advisory)", &pj, &sym);
        avp_vec_free(&parts);
    }
    const AvpJson *strs = J(d, "strings");
    if (avp_json_len(strs)) {
        AvpMap *uniq = avp_map_new();
        for (size_t i = 0; i < avp_json_len(strs); i++) {
            bool created;
            avp_map_put(uniq, avp_json_at(strs, i) ? brief_jstr(avp_json_at(strs, i), "text") : AVP_S(""), &created)
                ->i += 1;
        }
        size_t k, shown = brief_cap(avp_map_len(uniq), ns, &k);
        AvpStrVec parts = {0};
        AvpMapIter it = {0};
        for (size_t i = 0; i < shown && avp_map_next(uniq, &it); i++) {
            AvpStr t = brief_clip(avp_str_replace(a, it.key, AVP_S("\n"), AVP_S("\\n"), -1), 50);
            AvpStr x = it.val->i > 1 ? avp_pyfmt(a, " x%d", it.val->i) : AVP_S("");
            avp_vec_push(&parts, avp_pyfmt(a, "\"%S\"%S", &t, &x));
        }
        AvpStr j = avp_str_join(a, AVP_S("; "), parts.v, parts.n);
        AvpStr more = k ? avp_pyfmt(a, " (+%d)", k) : AVP_S("");
        brief_addf(&L, "  strings: %S%S", &j, &more);
        avp_vec_free(&parts);
        avp_map_free(uniq);
    }
    const AvpJson *fls = J(d, "floats");
    if (avp_json_len(fls)) {
        size_t k, shown = brief_cap(avp_json_len(fls), ns, &k);
        AvpStrVec parts = {0};
        for (size_t i = 0; i < shown; i++) {
            const AvpJson *x = avp_json_at(fls, i);
            AvpStr fv = fmt_float(J(x, "value"));
            avp_vec_push(&parts, avp_pyfmt(a, "%s=%S", brief_py_strz(J(x, "object")), &fv));
        }
        AvpStr j = avp_str_join(a, AVP_S(", "), parts.v, parts.n);
        AvpStr more = k ? avp_pyfmt(a, " (+%d)", k) : AVP_S("");
        brief_addf(&L, "  floats: %S%S", &j, &more);
        avp_vec_free(&parts);
    }
    const AvpJson *data = J(d, "data");
    if (avp_json_len(data) && nd == 0) {
        brief_addf(&L, "  data symbols: %d", avp_json_len(data));
    } else if (avp_json_len(data)) {
        size_t k, shown = brief_cap(avp_json_len(data), nd, &k);
        for (size_t i = 0; i < shown; i++) {
            const AvpJson *x = avp_json_at(data, i);
            i64 off = avp_json_get_i64(x, "offset", 0);
            AvpStr o = off ? avp_pyfmt(a, "+0x%X", off) : AVP_S("");
            AvpStr v = brief_truthy(J(x, "value")) ? avp_pyfmt(a, " = %s", brief_py_strz(J(x, "value"))) : AVP_S("");
            const AvpJson *desc = J(x, "desc");
            brief_addf(&L, "    data %s%S  %s%S", brief_py_strz(J(x, "symbol")), &o,
                       brief_truthy(desc) ? brief_py_strz(desc) : "", &v);
        }
        brief_more(&L, k, "more data symbols");
    }
    const AvpJson *errs = J(d, "errors");
    for (size_t i = 0; i < avp_json_len(errs); i++)
        brief_addf(&L, "  unavailable: %s", brief_py_strz(avp_json_at(errs, i)));
    return L;
}

/* ---- shape ---- */

static AvpJson *gather_shape(BriefWorld *W, void *vc) {
    FnCtx *c = vc;
    AvpArena *a = A();
    const ShsmSymRow *s = c->s;
    ShsmAsmIndex *ix = brief_ix(W);
    ShsmAsmFile *af = NULL;
    ShsmAsmBlock *b = shsm_asmindex_function(a, ix, shsm_target_addr(s->address), &af);
    if (!b) shsm_raise(SHSM_EXC_TYPE, "'NoneType' object has no attribute 'rows'");
    QuerySummary qs = query_summarize(b->rows.v, b->rows.n);
    AvpJson *summ = query_summary_json(a, &qs);
    const char *regs[] = {"saved_gprs", "saved_fprs"};
    for (int k = 0; k < 2; k++) {
        const AvpJson *v = J(summ, regs[k]);
        if (!brief_truthy(v) || !avp_json_is_str(v)) continue;
        AvpStrVec parts = avp_str_split(v->u.str, AVP_S("-"), -1);
        if (avp_str_eq(parts.v[0], parts.v[parts.n - 1])) brief_set(summ, regs[k], avp_json_str(a, parts.v[0]));
        avp_vec_free(&parts);
    }
    i64 back = 0, bctr = 0;
    for (size_t i = 0; i < b->rows.n; i++) {
        const ShsmAsmRow *r = &b->rows.v[i];
        if (r->label != SHSM_NONE && r->label <= r->address && avp_str_startswith(r->mnemonic, AVP_S("b"))) back++;
        if (avp_str_eqz(r->mnemonic, "bctr")) bctr++;
    }
    brief_set(summ, "loops", avp_json_int(a, back));
    brief_set(summ, "jump_tables", avp_json_int(a, bctr));
    AvpTry t;
    if (AVP_TRY(&t)) {
        i64 jt = 0;
        if (bctr) {
            static const char *const kinds[] = {"jumptable"};
            ShsmRefs refs = shsm_asmindex_refs_to(a, ix, shsm_target_name(s->name), kinds, 1);
            if (refs.n) jt = bctr;
        }
        brief_set(summ, "jump_tables", avp_json_int(a, jt > 0 ? jt : 0));
        avp_try_end(&t);
    }
    return summ;
}

BriefLines brief_fmt_shape(const BriefSection *sec, int level) {
    (void)level;
    AvpArena *a = A();
    const AvpJson *d = sec->data;
    BriefLines L = {0};
    AvpStrVec bits = {0};
    avp_vec_push(&bits, avp_pyfmt(a, "%d instructions", avp_json_get_i64(d, "instructions", 0)));
    i64 frame = avp_json_get_i64(d, "frame", 0);
    avp_vec_push(&bits, frame ? avp_pyfmt(a, "frame 0x%X", frame) : AVP_S("no frame"));
    const AvpJson *g = J(d, "saved_gprs"), *f = J(d, "saved_fprs");
    if (brief_truthy(g) || brief_truthy(f)) {
        AvpStrVec r = {0};
        if (brief_truthy(g)) avp_vec_push(&r, brief_py_str(g));
        if (brief_truthy(f)) avp_vec_push(&r, brief_py_str(f));
        AvpStr j = avp_str_join(a, AVP_S(", "), r.v, r.n);
        avp_vec_push(&bits, avp_pyfmt(a, "saves %S", &j));
        avp_vec_free(&r);
    } else {
        avp_vec_push(&bits, AVP_S("no saved registers"));
    }
    i64 tail = avp_json_get_i64(d, "tail_calls", 0);
    AvpStr tl = tail ? avp_pyfmt(a, " (%d tail)", tail) : AVP_S("");
    avp_vec_push(&bits, avp_pyfmt(a, "%d calls%S", avp_json_get_i64(d, "calls", 0), &tl));
    i64 loops = avp_json_get_i64(d, "loops", 0);
    avp_vec_push(&bits, loops ? avp_pyfmt(a, "%d loop(s)", loops) : AVP_S("no loops"));
    i64 jt = avp_json_get_i64(d, "jump_tables", 0);
    if (jt) avp_vec_push(&bits, avp_pyfmt(a, "%d jump table(s)", jt));
    avp_vec_push(&bits, brief_truthy(J(d, "float")) ? AVP_S("float code") : AVP_S("no float code"));
    AvpStr j = avp_str_join(a, AVP_S(", "), bits.v, bits.n);
    brief_addf(&L, "  %S", &j);
    avp_vec_free(&bits);
    return L;
}

/* ---- listing ---- */

static AvpJson *asm_rows(BriefWorld *W, const ShsmSymRow *s) {
    AvpArena *a = A();
    ShsmAsmFile *af = NULL;
    ShsmAsmBlock *b = shsm_asmindex_function(a, brief_ix(W), shsm_target_addr(s->address), &af);
    if (!b) shsm_raise(SHSM_EXC_TYPE, "'NoneType' object has no attribute 'rows'");
    AvpIMap *labels = avp_imap_new();
    for (size_t i = 0; i < b->labels.n; i++) avp_imap_put(labels, (u64)b->labels.v[i].address, NULL)->i = (i64)i;
    AvpJson *out = avp_json_arr(a);
    for (size_t i = 0; i < b->rows.n; i++) {
        const ShsmAsmRow *r = &b->rows.v[i];
        AvpVal *lv = avp_imap_get(labels, (u64)r->address);
        if (lv && r->address != s->address)
            avp_json_arr_push(a, out, avp_json_str(a, avp_pyfmt(a, "%S:", &b->labels.v[lv->i].name)));
        AvpStr note = query_row_note(W->ctx, r);
        AvpStr line = avp_pyfmt(a, "  %08X  %-8S %S", r->address, &r->mnemonic, &r->operands);
        avp_json_arr_push(a, out, avp_json_str(a, note.n ? avp_pyfmt(a, "%-58S ; %S", &line, &note) : line));
    }
    avp_imap_free(labels);
    return out;
}

static AvpJson *gather_listing(BriefWorld *W, void *vc) {
    FnCtx *c = vc;
    AvpArena *a = A();
    const ShsmSymRow *s = c->s;
    AvpJson *d = avp_json_obj(a);
    brief_set(d, "kind", avp_json_null());
    brief_set(d, "lines", avp_json_arr(a));
    brief_set(d, "note", avp_json_null());
    brief_set(d, "file", avp_json_null());
    brief_set(d, "stale", avp_json_bool(false));
    if (!c->o->asm_) {
        DecompPaths *paths = decomp_paths(a, W->repo);
        DecompListing lst = {0};
        AvpTry t;
        if (AVP_TRY(&t)) {
            lst = decomp_listing_at(a, W->repo, (u64)s->address);
            avp_try_end(&t);
        } else {
            lst = (DecompListing){0};
        }
        const AvpJson *e = lst.entry;
        AvpStr status = brief_jstr(e, "status");
        bool ok = status.p && avp_str_eqz(status, "ok");
        if (lst.found && brief_truthy(e) && ok) {
            AvpStrVec text = avp_str_split(avp_str_rstrip_chars(lst.text, AVP_S("\n")), AVP_S("\n"), -1);
            size_t i = 0;
            while (i < text.n && avp_str_startswith(text.v[i], AVP_S("// ")) &&
                   avp_str_find(text.v[i], AVP_S(":"), 0, AVP_END) >= 0)
                i++;
            while (i < text.n && !avp_str_strip(text.v[i]).n) i++;
            AvpJson *lines = avp_json_arr(a);
            for (; i < text.n; i++) avp_json_arr_push(a, lines, avp_json_str(a, text.v[i]));
            brief_set(d, "kind", avp_json_strz(a, "ghidra"));
            brief_set(d, "lines", lines);
            brief_set(d, "note", avp_json_strz(a, DECOMP_NOTICE));
            brief_set(d, "file", avp_json_strz(a, shsm_repo_rel(a, W->repo, decomp_entry_file(a, paths, e))));
            brief_set(d, "stale", avp_json_bool(lst.stale));
            return d;
        }
        if (brief_truthy(e) && !ok) {
            const AvpJson *reason = J(e, "reason");
            brief_set(d, "note", avp_json_str(a, avp_pyfmt(a, "Ghidra could not decompile it (%s); retail assembly "
                                                              "instead",
                                                           brief_truthy(reason) ? brief_py_strz(reason) : "?")));
        } else if (decomp_project_exists(paths)) {
            brief_set(d, "note", avp_json_str(a, avp_pyfmt(a, "no listing yet (build/tools/decomp.exe show %S); retail "
                                                              "assembly instead",
                                                           &s->name)));
        } else {
            brief_set(d, "note",
                      avp_json_strz(a, "no Ghidra project (build/tools/decomp.exe setup); retail assembly instead"));
        }
    }
    brief_set(d, "kind", avp_json_strz(a, "asm"));
    brief_set(d, "lines", asm_rows(W, s));
    brief_set(d, "file", avp_json_str(a, avp_pyfmt(a, "build/tools/query.exe fn %S", &s->name)));
    return d;
}

BriefLines brief_fmt_listing(const BriefSection *sec, int level) {
    const AvpJson *d = sec->data;
    BriefLines L = {0};
    if (brief_truthy(J(d, "note"))) brief_add(&L, brief_py_str(J(d, "note")));
    if (brief_truthy(J(d, "stale")))
        brief_addz(&L, "// made from an older symbols.txt: names may be stale (build/tools/decomp.exe refresh)");
    i64 first = sec->first;
    i64 caps[] = {first, avp_floordiv_i64(first * 3, 4), avp_floordiv_i64(first, 2), 12, 8, 0};
    i64 n = level < 0 ? SHSM_NONE : caps[level < 6 ? level : 5];
    const AvpJson *lines = J(d, "lines");
    size_t len = avp_json_len(lines);
    size_t shown = len;
    i64 k = 0;
    if (n != SHSM_NONE && (i64)len > n) {
        shown = n >= 0 ? (size_t)n : (size_t)((i64)len + n > 0 ? (i64)len + n : 0);
        k = (i64)len - n;
    }
    for (size_t i = 0; i < shown; i++) brief_add(&L, avp_json_at(lines, i)->u.str);
    if (k) brief_addf(&L, "... %d more lines (%s, or --full)", k, brief_py_strz(J(d, "file")));
    return L;
}

/* ---- start here ---- */

AvpJson *brief_profile_of(BriefWorld *W, i64 address, const char *lib);
AvpJson *brief_profile_of(BriefWorld *W, i64 address, const char *lib) {
    AvpArena *a = A();
    AvpStr group = avp_str(lib ? lib : "");
    AvpStr before, after;
    avp_str_partition(group, AVP_S("/"), &before, &after);
    group = before;
    const ShsmSplits *sp = brief_splits(W);
    const ShsmUnit *best = NULL;
    int best_rank = 0;
    i64 best_dist = 0;
    for (size_t i = 0; i < sp->nunits; i++) {
        const ShsmSplitUnit *su = &sp->units[i];
        const ShsmSplitRange *t = NULL;
        for (size_t k = 0; k < su->nranges && !t; k++)
            if (strcmp(su->ranges[k].section, ".text") == 0) t = &su->ranges[k];
        if (!t) continue;
        const ShsmUnit *u = brief_source_unit(W, shsm_unit_name(a, su->path));
        if (!u || !u->has_cflags || !u->cflags.n) continue;
        const char *ulib = brief_library(W, (i64)t->start);
        int rank;
        if (ulib && lib ? strcmp(ulib, lib) == 0 : (!ulib && !lib)) {
            rank = 0;
        } else {
            AvpStr ug = avp_str(ulib ? ulib : ""), ub, ua;
            avp_str_partition(ug, AVP_S("/"), &ub, &ua);
            rank = avp_str_eq(ub, group) ? 1 : 2;
        }
        i64 dist = (i64)t->start - address;
        if (dist < 0) dist = -dist;
        if (!best || rank < best_rank || (rank == best_rank && dist < best_dist)) {
            best = u;
            best_rank = rank;
            best_dist = dist;
        }
    }
    if (!best) return avp_json_null();
    const char *lang = NULL;
    for (size_t i = 0; i < best->cflags.n && !lang; i++)
        if (avp_str_startswith(best->cflags.v[i], AVP_S("-lang="))) lang = avp_cz(a, avp_str_slice(best->cflags.v[i], 6, AVP_END));
    static const char *const MATCH[] = {"same library", "same group", "different library"};
    AvpJson *p = avp_json_obj(a);
    brief_set(p, "unit", avp_json_strz(a, best->name));
    brief_set(p, "library", brief_jz(best->library));
    brief_set(p, "compiler", brief_jz(best->mw_version));
    brief_set(p, "extra", brief_base_words(&best->cflags));
    brief_set(p, "lang", brief_jz(lang));
    brief_set(p, "match", avp_json_strz(a, MATCH[best_rank]));
    brief_set(p, "distance", avp_json_int(a, best_dist));
    return p;
}

/* neighbourhood(W, s): [start, end) of the window of asm functions around s. */
static void neighbourhood(BriefWorld *W, const ShsmSymRow *s, i64 *start, i64 *end) {
    AvpArena *a = A();
    const char *lib = brief_library(W, s->address);
    AvpJson *params = avp_json_arr(a);
    avp_json_arr_push(a, params, s->unit.p ? avp_json_str(a, s->unit) : avp_json_null());
    AvpJson *rows = shsm_asmindex_sql(
        a, brief_ix(W), "SELECT address, size FROM symbols WHERE kind = 'function' AND unit = ? ORDER BY address",
        params);
    AVP_VEC(i64) addr = {0};
    AVP_VEC(i64) size = {0};
    for (size_t i = 0; i < avp_json_len(rows); i++) {
        const AvpJson *r = avp_json_at(rows, i);
        i64 ad = avp_json_i64(avp_json_at(r, 0), 0);
        const char *l = brief_library(W, ad);
        if (!(l && lib ? strcmp(l, lib) == 0 : (!l && !lib))) continue;
        avp_vec_push(&addr, ad);
        avp_vec_push(&size, avp_json_is_null(avp_json_at(r, 1)) ? 0 : avp_json_i64(avp_json_at(r, 1), 0));
    }
    if (!addr.n) {
        avp_vec_push(&addr, s->address);
        avp_vec_push(&size, s->size == SHSM_NONE || !s->size ? 4 : s->size);
    }
    i64 k = 0;
    for (size_t i = 0; i < addr.n; i++)
        if (addr.v[i] == s->address) {
            k = (i64)i;
            break;
        }
    i64 half = BRIEF_PLAN_WINDOW / 2;
    i64 lo = (k / half) * half - half / 2;
    if (lo < 0) lo = 0;
    i64 hi = ((i64)addr.n < lo + BRIEF_PLAN_WINDOW ? (i64)addr.n : lo + BRIEF_PLAN_WINDOW) - 1;
    *start = addr.v[lo];
    *end = addr.v[hi] + (size.v[hi] ? size.v[hi] : 4);
    avp_vec_free(&addr);
    avp_vec_free(&size);
}

static AvpJson *known_names(BriefWorld *W, const ShsmSymRow *s, const char *unit_name) {
    AvpArena *a = A();
    AvpJson *out = avp_json_arr(a);
    HashesVarNames *rows = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        ShsmDataRefs dt = shsm_asmindex_data_targets(a, brief_ix(W), shsm_target_sym(s));
        AVP_VEC(i64) refs = {0};
        for (size_t i = 0; i < dt.n; i++)
            if (dt.v[i].address != SHSM_NONE) avp_vec_push(&refs, dt.v[i].address);
        rows = hashes_var_names(a, W->repo, unit_name, refs.v, refs.n, NULL, 0);
        avp_vec_free(&refs);
        avp_try_end(&t);
    } else {
        return out;
    }
    AvpSet *seen = avp_set_new();
    for (size_t i = 0; rows && i < rows->n; i++) {
        const HashesVarName *r = &rows->v[i];
        AvpStr var = r->var_off ? avp_pyfmt(a, "%s+0x%X", r->var, r->var_off) : avp_str(r->var);
        AvpStr key = avp_pyfmt(a, "%S\x1f%s\x1f%s", &var, r->text, r->kind);
        if (!avp_set_add(seen, key)) continue;
        avp_json_arr_push(a, out, brief_arr_of(3, avp_json_str(a, var), brief_jz(r->text), brief_jz(r->kind)));
    }
    avp_set_free(seen);
    return out;
}

static AvpJson *known_fragments(BriefWorld *W, const ShsmSymRow *s) {
    AvpArena *a = A();
    AvpJson *out = avp_json_arr(a);
    FragmentsInFunction rows = {0};
    bool ok = false;
    AvpTry t;
    if (AVP_TRY(&t)) {
        ok = fragments_in_function(a, W->root, s->address, 4, NULL, &rows);
        avp_try_end(&t);
    }
    if (!ok) return out;
    for (size_t i = 0; i < rows.n; i++) {
        avp_json_arr_push(a, out, brief_arr_of(4, avp_json_strz(a, rows.v[i].id), avp_json_int(a, rows.v[i].offset),
                                               avp_json_int(a, rows.v[i].length), avp_json_int(a, rows.v[i].nfuncs)));
        avp_vec_free(&rows.v[i].pattern);
    }
    avp_vec_free(&rows);
    return out;
}

static AvpJson *gather_start(BriefWorld *W, void *vc) {
    FnCtx *c = vc;
    AvpArena *a = A();
    const ShsmSymRow *s = c->s;
    AvpJson *d = avp_json_obj(a);
    brief_set(d, "state", avp_json_null());
    AvpJson *commands = avp_json_arr(a), *errors = avp_json_arr(a);
    brief_set(d, "commands", commands);
    brief_set(d, "profile", avp_json_null());
    brief_set(d, "plan", avp_json_null());
    brief_set(d, "read_first", avp_json_null());
    brief_set(d, "errors", errors);
    brief_set(d, "symbol", avp_json_str(a, s->name));
    const AvpJson *tw = c->twins;
    bool twins_ok = brief_truthy(tw) && !brief_truthy(J(tw, "too_small"));
    if (twins_ok) {
        for (int lv = 0; lv < 3 && avp_json_is_null(J(d, "read_first")); lv++) {
            const AvpJson *recs = J(J(tw, "levels"), TWINS_LEVELS[lv]);
            for (size_t i = 0; i < avp_json_len(recs); i++) {
                const AvpJson *r = avp_json_at(recs, i);
                if (!brief_truthy(J(r, "matched"))) continue;
                const AvpJson *line = J(r, "line");
                AvpStr ln = brief_truthy(line) ? avp_pyfmt(a, ":%d", avp_json_i64(line, 0)) : AVP_S("");
                brief_set(d, "read_first", avp_json_str(a, avp_pyfmt(a, "%s%S (%s)", brief_py_strz(J(r, "source")), &ln,
                                                                     brief_py_strz(J(r, "name")))));
                break;
            }
        }
    }
    const ShsmUnit *u = brief_source_unit(W, c->unit_name);
    bool exact = brief_truthy(J(c->ident, "exact"));
    if (u) {
        brief_set(d, "state", avp_json_strz(a, exact ? "exact" : "compiled"));
        if (exact) {
            avp_json_arr_push(a, commands,
                              avp_json_str(a, avp_pyfmt(a, "build/tools/match.exe %s %S --rules", u->path, &s->name)));
        } else {
            avp_json_arr_push(a, commands,
                              avp_json_str(a, avp_pyfmt(a, "build/tools/match.exe %s --diff %S", u->path, &s->name)));
            avp_json_arr_push(a, commands,
                              avp_json_str(a, avp_pyfmt(a, "build/tools/wall.exe %s %S", u->path, &s->name)));
        }
        return d;
    }
    brief_set(d, "state", avp_json_strz(a, "unstarted"));
    if (twins_ok) {
        const AvpJson *ex = J(J(tw, "levels"), "exact");
        bool any = false;
        for (size_t i = 0; i < avp_json_len(ex); i++) any = any || brief_truthy(J(avp_json_at(ex, i), "matched"));
        if (any) avp_json_arr_push(a, commands, avp_json_str(a, avp_pyfmt(a, "build/tools/draft.exe twin %S", &s->name)));
    }
    const AvpJson *lib = J(c->ident, "library");
    brief_set(d, "library", lib ? (AvpJson *)lib : avp_json_null());
    const char *libz = brief_truthy(lib) || (lib && avp_json_is_str(lib)) ? brief_py_strz(lib) : NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        brief_set(d, "profile", brief_profile_of(W, s->address, libz));
        avp_try_end(&t);
    } else {
        avp_json_arr_push(a, errors, avp_json_str(a, avp_str_concat(a, AVP_S("profile: "),
                                                                    brief_clip(shsm_exc_str(a, t.code, t.msg), 90))));
    }
    const AvpJson *plan = c->plan;
    if (plan && brief_truthy(J(plan, "error"))) {
        avp_json_arr_push(a, errors,
                          avp_json_str(a, avp_pyfmt(a, "unit plan: %s", brief_py_strz(J(plan, "error")))));
    } else if (plan) {
        const AvpJson *rng = J(plan, "range");
        i64 start = avp_json_i64(avp_json_at(rng, 0), 0), end = avp_json_i64(avp_json_at(rng, 1), 0);
        brief_set(d, "neighbourhood", brief_arr_of(2, avp_json_int(a, start), avp_json_int(a, end)));
        if (AVP_TRY(&t)) {
            UpPlanDoc *doc = up_plan_for(a, start, end, W->repo, "low", brief_ix(W),
                                         brief_truthy(J(plan, "run")), false);
            for (size_t i = 0; doc && i < doc->units.n; i++) {
                const UpUnit *pu = &doc->units.v[i];
                if (!(pu->start <= s->address && s->address < pu->end)) continue;
                AvpJson *full = up_unit_json(a, pu), *pd = avp_json_obj(a);
                const char *keys[] = {"start", "end", "functions", "confidence", "end_tags", "names", "command"};
                for (size_t k = 0; k < 7; k++) brief_set(pd, keys[k], J(full, keys[k]) ? J(full, keys[k]) : avp_json_null());
                brief_set(d, "plan", pd);
                break;
            }
            avp_try_end(&t);
        } else {
            avp_json_arr_push(a, errors, avp_json_str(a, avp_str_concat(a, AVP_S("unit plan: "),
                                                                        brief_clip(shsm_exc_str(a, t.code, t.msg), 90))));
        }
        avp_json_arr_push(a, commands,
                          avp_json_str(a, avp_pyfmt(a, "build/tools/unit_plan.exe %08X-%08X", start, end)));
        const AvpJson *pd = J(d, "plan");
        if (brief_truthy(pd)) {
            const AvpJson *cmd = J(pd, "command");
            if (brief_truthy(cmd) && !avp_str_startswith(brief_py_str(cmd), AVP_S("#")))
                avp_json_arr_push(a, commands, (AvpJson *)cmd);
        }
    }
    brief_set(d, "names", known_names(W, s, c->unit_name));
    brief_set(d, "fragments", known_fragments(W, s));
    return d;
}

BriefLines brief_fmt_start(const BriefSection *sec, int level) {
    (void)level;
    AvpArena *a = A();
    const AvpJson *d = sec->data;
    BriefLines L = {0};
    if (brief_truthy(J(d, "read_first")))
        brief_addf(&L, "  read the matched twin first: %s", brief_py_strz(J(d, "read_first")));
    const AvpJson *p = J(d, "plan");
    if (brief_truthy(p)) {
        const AvpJson *conf = J(p, "confidence"), *tags = J(p, "end_tags"), *names = J(p, "names");
        AvpStrVec tl = {0};
        for (size_t i = 0; brief_truthy(tags) && i < avp_json_len(tags); i++)
            avp_vec_push(&tl, brief_py_str(avp_json_at(tags, i)));
        AvpStr tj = avp_str_join(a, AVP_S(","), tl.v, tl.n);
        if (!tj.n) tj = AVP_S("(range end)");
        AvpStr ct = avp_json_is_null(conf) ? AVP_S("")
                                           : avp_pyfmt(a, " (precision %.0f%%)", avp_json_f64(conf, 0.0) * 100);
        AvpStr nt = AVP_S("");
        if (brief_truthy(names)) {
            AvpStrVec nl = {0};
            for (size_t i = 0; i < avp_json_len(names) && i < 3; i++) avp_vec_push(&nl, brief_py_str(avp_json_at(names, i)));
            AvpStr nj = avp_str_join(a, AVP_S(", "), nl.v, nl.n);
            nt = avp_pyfmt(a, ", names %S", &nj);
            avp_vec_free(&nl);
        }
        brief_addf(&L, "  proposed unit %08X-%08X: %d function(s), end %S%S%S", avp_json_get_i64(p, "start", 0),
                   avp_json_get_i64(p, "end", 0), avp_json_get_i64(p, "functions", 0), &tj, &ct, &nt);
        avp_vec_free(&tl);
    }
    const AvpJson *cmds = J(d, "commands");
    for (size_t i = 0; i < avp_json_len(cmds); i++) brief_addf(&L, "  %s", brief_py_strz(avp_json_at(cmds, i)));
    AvpStr state = brief_jstr(d, "state");
    if (state.p && avp_str_eqz(state, "unstarted")) {
        const AvpJson *lib = J(d, "library");
        brief_addf(&L, "  %s", brief_profile_line(J(d, "profile"), brief_truthy(lib) ? brief_py_strz(lib) : NULL));
    }
    const AvpJson *names = J(d, "names");
    if (brief_truthy(names)) {
        AvpStrVec nl = {0};
        size_t n = avp_json_len(names);
        for (size_t i = 0; i < n && i < 4; i++) {
            const AvpJson *x = avp_json_at(names, i);
            avp_vec_push(&nl, avp_pyfmt(a, "%s = \"%s\" (%s)", brief_py_strz(avp_json_at(x, 0)),
                                        brief_py_strz(avp_json_at(x, 1)), brief_py_strz(avp_json_at(x, 2))));
        }
        AvpStr j = avp_str_join(a, AVP_S(", "), nl.v, nl.n);
        if (n > 4) j = avp_str_concat(a, j, avp_pyfmt(a, ", +%d", n - 4));
        AvpStr cj = brief_clip(j, 150);
        brief_addf(&L, "  known names (build/tools/hashes.exe vars): %S", &cj);
        avp_vec_free(&nl);
    }
    const AvpJson *frs = J(d, "fragments");
    if (brief_truthy(frs)) {
        AvpStrVec fl = {0};
        size_t n = avp_json_len(frs);
        for (size_t i = 0; i < n && i < 4; i++) {
            const AvpJson *x = avp_json_at(frs, i);
            avp_vec_push(&fl, avp_pyfmt(a, "%s +0x%X %d rows x%d fns", brief_py_strz(avp_json_at(x, 0)),
                                        avp_json_i64(avp_json_at(x, 1), 0), avp_json_i64(avp_json_at(x, 2), 0),
                                        avp_json_i64(avp_json_at(x, 3), 0)));
        }
        AvpStr j = avp_str_join(a, AVP_S(", "), fl.v, fl.n);
        if (n > 4) j = avp_str_concat(a, j, avp_pyfmt(a, ", +%d", n - 4));
        AvpStr cj = brief_clip(j, 150);
        AvpStr sym = J(d, "symbol") ? brief_py_str(J(d, "symbol")) : AVP_S("<fn>");
        brief_addf(&L, "  inlined-helper fragments (build/tools/fragments.exe in %S): %S", &sym, &cj);
        avp_vec_free(&fl);
    }
    const AvpJson *errs = J(d, "errors");
    for (size_t i = 0; i < avp_json_len(errs); i++)
        brief_addf(&L, "  unavailable: %s", brief_py_strz(avp_json_at(errs, i)));
    return L;
}

/* ---- the function briefing ---- */

static const char *const SECTIONS[] = {"identity", "status", "twins", "context", "shape", "listing", "start"};

const char *brief_function(BriefWorld *W, const BriefOpts *o, BriefSections *out, BriefTimings *tm) {
    AvpArena *a = A();
    double t0 = avp_time();
    W->ctx = W->ctx ? W->ctx : query_ctx_for_root(a, W->root);
    const ShsmSymRow *s = query_resolve(W->ctx, avp_str(o->target), "function");
    brief_timing(tm, "resolve", avp_time() - t0);
    const AvpStrVec *want = o->sections;
    FnCtx *c = AVP_NEW(a, FnCtx);
    c->s = s;
    c->o = o;
    c->unit_name = brief_unit_name_of(s);
    if (brief_wants(want, "status")) c->wall_run = wall_wanted(W, s, c->unit_name, o->no_compile);
    if (brief_wants(want, "start") && !brief_source_unit(W, c->unit_name)) {
        AvpJson *plan = avp_json_obj(a);
        AvpTry t;
        if (AVP_TRY(&t)) {
            i64 start, end;
            neighbourhood(W, s, &start, &end);
            brief_set(plan, "range", brief_arr_of(2, avp_json_int(a, start), avp_json_int(a, end)));
            brief_set(plan, "run", avp_json_bool(!o->no_compile));
            avp_try_end(&t);
        } else {
            brief_set(plan, "range", avp_json_null());
            brief_set(plan, "error", avp_json_strz(a, exc_text(&t, 90, true)));
        }
        c->plan = plan;
    }
    BriefSection *secs[7] = {0};
    BriefSection *ident = brief_build_section(W, "identity", gather_identity, c, brief_fmt_identity, 1);
    secs[0] = ident;
    c->ident = ident && ident->data ? ident->data : NULL;
    if (!c->ident) {
        c->ident = avp_json_obj(a);
        brief_set(c->ident, "exact", avp_json_bool(false));
        brief_set(c->ident, "library", avp_json_null());
    }
    BriefSection *twins = NULL;
    if (brief_wants(want, "twins") || brief_wants(want, "start")) {
        twins = brief_build_section(W, "twins", gather_twins, c, brief_fmt_twins, 3);
        if (twins && twins->data) brief_set(twins->data, "name", avp_json_str(a, s->name));
    }
    c->twins = twins ? twins->data : NULL;
    bool have[7] = {true};
    if (brief_wants(want, "status")) {
        BriefSection *st = brief_build_section(W, "status", gather_status, c, brief_fmt_status, 4);
        if (st && st->data) brief_set(st->data, "term", avp_json_str(a, s->name));
        secs[1] = st, have[1] = true;
    }
    if (brief_wants(want, "twins")) secs[2] = twins, have[2] = true;
    if (brief_wants(want, "context"))
        secs[3] = brief_build_section(W, "context", gather_context, c, brief_fmt_context, 5), have[3] = true;
    if (brief_wants(want, "shape"))
        secs[4] = brief_build_section(W, "shape", gather_shape, c, brief_fmt_shape, 1), have[4] = true;
    if (brief_wants(want, "listing") && !o->no_listing) {
        secs[5] = brief_build_section(W, "listing", gather_listing, c, brief_fmt_listing, 6), have[5] = true;
        if (secs[5]) secs[5]->first = o->listing_lines;
    }
    if (brief_wants(want, "start"))
        secs[6] = brief_build_section(W, "start", gather_start, c, brief_fmt_start, 1), have[6] = true;
    for (size_t i = 0; i < 7; i++)
        if (have[i] && secs[i]) brief_timing(tm, SECTIONS[i], secs[i]->seconds);
    out->n = 0;
    for (size_t i = 0; i < 7; i++)
        if (brief_wants(want, SECTIONS[i]) && secs[i]) out->v[out->n++] = secs[i];
    return avp_cz(a, s->name);
}
