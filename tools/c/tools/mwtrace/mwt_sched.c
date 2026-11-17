/* mwt_sched: the `sched` subcommand (mwt_sched.h): the run, the text and JSON reports, the --against comparison and
 * the --pair query. */
#include "mwt_sched.h"

#include <stdlib.h>

#include "cc_compare.h"
#include "cc_listing.h"
#include "mwt_regalloc.h"

static void set(AvpArena *a, AvpJson *o, const char *k, AvpJson *v) { avp_json_set(a, o, avp_str(k), v); }
static AvpJson *jint(AvpArena *a, i64 v) { return avp_json_int(a, v); }
static AvpJson *jstr(AvpArena *a, AvpStr s) { return avp_json_str(a, s); }

static AvpJson *jints(AvpArena *a, const MwtSchedInts *v) {
    AvpJson *r = avp_json_arr(a);
    for (size_t i = 0; i < v->n; i++) avp_json_arr_push(a, r, jint(a, v->v[i]));
    return r;
}

static AvpJson *jstrs(AvpArena *a, const AvpStrVec *v) {
    AvpJson *r = avp_json_arr(a);
    for (size_t i = 0; i < v->n; i++) avp_json_arr_push(a, r, jstr(a, v->v[i]));
    return r;
}

static const char *kinds_text(AvpArena *a, u32 kinds) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (int k = 0; k < MWT_DEP_N; k++)
        if (kinds & (1u << k)) avp_buf_printf(&b, "%s%s", b.n ? "+" : "", mwt_sched_dep_name((MwtDepKind)k));
    return avp_cz(a, avp_buf_str(&b));
}

static const char *ints_text(AvpArena *a, const MwtSchedInts *v) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < v->n; i++) avp_buf_printf(&b, "%s#%d", i ? " " : "", v->v[i]);
    if (!v->n) avp_buf_putz(&b, "none");
    return avp_cz(a, avp_buf_str(&b));
}

/* "issued #17 LWZ over #19 by height 5 > 4, over #22 by program order (...)" */
static void sel_line(AvpArena *a, AvpBuf *o, const MwtSchedBlock *b, const MwtSchedSel *s) {
    avp_buf_printf(o, "    cycle %d slot %d: ", s->cycle, s->slot);
    if (s->chosen < 0) avp_buf_putz(o, "nothing issued (the cycle ends)");
    else avp_buf_printf(o, "issued %s", mwt_sched_node_label(a, &b->nodes.v[s->chosen]));
    for (size_t i = 0; i < s->ready.n; i++) {
        int y = s->ready.v[i];
        if (y == s->chosen) continue;
        for (size_t k = 0; k < s->cmps.n; k++) {
            const MwtSchedCmp *m = &s->cmps.v[k];
            int win = m->taken ? m->cand : m->inc, lose = m->taken ? m->inc : m->cand;
            if (lose != y) continue;
            int ws = m->taken ? m->cand_succ : m->inc_succ, ls = m->taken ? m->inc_succ : m->cand_succ;
            const char *kw = mwt_sched_key_words(a, b, s->cycle, m->key, win, lose, ws, ls);
            if (win == s->chosen) avp_buf_printf(o, "; over #%d by %s", y, kw);
            else avp_buf_printf(o, "; #%d lost to #%d by %s", y, win, kw);
        }
    }
    avp_buf_printf(o, "; ready: %s", ints_text(a, &s->ready));
    if (s->busy.n) avp_buf_printf(o, "; can-issue 0: %s", ints_text(a, &s->busy));
    avp_buf_putc(o, '\n');
}

static void block_text(AvpArena *a, AvpBuf *o, const MwtSchedBlock *b) {
    avp_buf_printf(o, "  block %d (index %d, %d instructions, flags %#x, greatest height %d, %d cycles)\n", b->seq, b->index,
                   b->count, b->flags, b->maxh, b->cycles);
    avp_buf_printf(o, "    %-4s %-8s %-24s %3s %6s %5s %5s %4s %5s %4s %5s %4s\n", "node", "op", "registers", "lat", "height",
                   "slack", "preds", "prio", "ra", "out", "cycle", "emit");
    for (size_t i = 0; i < b->nodes.n; i++) {
        const MwtSchedNode *n = &b->nodes.v[i];
        AvpStrVec rw = avp_str_split(n->opnds, AVP_S(" "), -1);
        AvpStr regs = n->opnds.n && rw.n > 4 ? avp_str(avp_sprintf(a, "(%zu registers)", rw.n)) : n->opnds;
        avp_buf_printf(o, "    #%-3d %-8.*s %-24.*s %3d %6d %5d %5d %4d %5s %4d %5d %4s\n", n->index, AVP_SF(n->op),
                       AVP_SF(regs), n->latency, n->height, n->slack, n->npreds, n->prio,
                       n->ra >= 0 ? avp_sprintf(a, "#%d", n->ra) : "-", n->out, n->cycle,
                       n->fpos >= 0 ? avp_sprintf(a, "@%d", n->fpos) : "-");
    }
    if (b->edges.n) avp_buf_putz(o, "    edges:\n");
    for (size_t i = 0; i < b->edges.n; i++) {
        const MwtSchedEdge *e = &b->edges.v[i];
        avp_buf_printf(o, "      #%d -> #%d %s", e->from, e->to, kinds_text(a, e->kinds));
        if (e->regs.n) avp_buf_printf(o, " %.*s", AVP_SF(e->regs));
        if (e->kinds & (1u << MWT_DEP_OTHER)) avp_buf_printf(o, " (site %#x)", e->site);
        avp_buf_printf(o, ", latency %d\n", e->latency);
    }
    if (b->mems.n) avp_buf_putz(o, "    memory pairs:\n");
    for (size_t i = 0; i < b->mems.n; i++) {
        const MwtSchedMem *m = &b->mems.v[i];
        const char *k = m->kind == 'l' ? "load before store" : m->kind == 's' ? "store before load" : "store before store";
        avp_buf_printf(o, "      #%d -> #%d %s:", m->from, m->to, k);
        if (m->precheck >= 0) avp_buf_printf(o, " precheck %d", m->precheck);
        if (m->alias >= 0) avp_buf_printf(o, " may_alias %d%s", m->alias, m->alias ? " (edge)" : " (no edge)");
        avp_buf_putc(o, '\n');
    }
    for (size_t i = 0; i < b->sels.n; i++) sel_line(a, o, b, &b->sels.v[i]);
    avp_buf_printf(o, "    output: %s\n", ints_text(a, &b->order));
}

AvpStr mwt_sched_text(AvpArena *a, const MwtSchedTrace *t, const MwtSchedFunc *f, const MwtSchedCheck *c) {
    AvpBuf o;
    avp_buf_init(&o, a);
    avp_buf_printf(&o, "function %.*s (%.*s), compiler %.*s\n", AVP_SF(mwt_sched_link(t, f)), AVP_SF(f->name),
                   AVP_SF(t->build));
    for (size_t pi = 0; pi < f->passes.n; pi++) {
        const MwtSchedPass *p = &f->passes.v[pi];
        avp_buf_printf(&o, "\npass %d (%s; argument %d, coloring %d: key 4 %s), schedule mode %d, processor %d, machine "
                           "width %d (+4 %d, +8 %d)%s%s\n",
                       p->pass, p->pass == 1 ? "before register allocation" : "after register allocation", p->arg,
                       p->coloring, p->coloring ? "on" : "off", p->mode, p->proc, p->width, p->m4, p->m8,
                       p->x112 ? ", [G_X112] set" : "", p->pre ? ", memory precheck on" : "");
        int skipped = 0;
        for (size_t i = 0; i < p->visits.n; i++)
            if (p->visits.v[i].sched < 0) skipped++;
        avp_buf_printf(&o, "  %zu blocks visited, %zu scheduled", p->visits.n, p->blocks.n);
        if (skipped) {
            avp_buf_putz(&o, "; not scheduled:");
            for (size_t i = 0; i < p->visits.n; i++)
                if (p->visits.v[i].sched < 0)
                    avp_buf_printf(&o, " index %d (%d instructions, flags %#x)", p->visits.v[i].index, p->visits.v[i].count,
                                   p->visits.v[i].flags);
        }
        avp_buf_putc(&o, '\n');
        for (size_t bi = 0; bi < p->blocks.n; bi++) block_text(a, &o, &p->blocks.v[bi]);
    }
    if (c) {
        avp_buf_printf(&o, "\nself-check: %d instructions emitted, object rows %d, %d opcodes and %d registers compared\n",
                       c->emitted, c->rows, c->ops_checked, c->regs_checked);
        for (size_t i = 0; i < c->notes.n; i++) avp_buf_printf(&o, "  accounted: %.*s\n", AVP_SF(c->notes.v[i]));
        for (size_t i = 0; i < c->gaps.n; i++) {
            const MwtSchedGap *g = &c->gaps.v[i];
            avp_buf_printf(&o, "  TRACE-GAP %.*s", AVP_SF(g->stage));
            if (g->pass >= 0) avp_buf_printf(&o, " pass %d", g->pass);
            if (g->block >= 0) avp_buf_printf(&o, " block %d", g->block);
            if (g->pos >= 0) avp_buf_printf(&o, " position %d", g->pos);
            avp_buf_printf(&o, ": %.*s\n", AVP_SF(g->why));
        }
        if (!c->gaps.n) avp_buf_putz(&o, "  ok: every selection replays from the trace's own edges, and pass 2's output "
                                         "order is the object's\n");
    }
    for (size_t i = 0; i < t->errors.n; i++) avp_buf_printf(&o, "  log: %.*s\n", AVP_SF(t->errors.v[i]));
    return avp_buf_str(&o);
}

static AvpJson *block_json(AvpArena *a, const MwtSchedBlock *b) {
    AvpJson *o = avp_json_obj(a);
    set(a, o, "seq", jint(a, b->seq));
    set(a, o, "index", jint(a, b->index));
    set(a, o, "count", jint(a, b->count));
    set(a, o, "flags", jint(a, b->flags));
    set(a, o, "greatest_height", jint(a, b->maxh));
    set(a, o, "cycles", jint(a, b->cycles));
    set(a, o, "priority_key", avp_json_bool(b->prio_key));
    AvpJson *ns = avp_json_arr(a);
    for (size_t i = 0; i < b->nodes.n; i++) {
        const MwtSchedNode *n = &b->nodes.v[i];
        AvpJson *j = avp_json_obj(a);
        set(a, j, "index", jint(a, n->index));
        set(a, j, "op", jstr(a, n->op));
        set(a, j, "registers", jstr(a, n->opnds));
        set(a, j, "pcode", jint(a, (i64)n->pcode));
        set(a, j, "pcode_flags", jint(a, n->pflags));
        set(a, j, "pcode_flags2", jint(a, n->pflags2));
        set(a, j, "latency", jint(a, n->latency));
        set(a, j, "height", jint(a, n->height));
        set(a, j, "slack", jint(a, n->slack));
        set(a, j, "preds", jint(a, n->npreds));
        set(a, j, "priority", jint(a, n->prio));
        set(a, j, "ra", jint(a, n->ra));
        set(a, j, "out", jint(a, n->out));
        set(a, j, "cycle", jint(a, n->cycle));
        set(a, j, "slot", jint(a, n->slot));
        set(a, j, "emitted", jint(a, n->fpos));
        avp_json_arr_push(a, ns, j);
    }
    set(a, o, "nodes", ns);
    AvpJson *es = avp_json_arr(a);
    for (size_t i = 0; i < b->edges.n; i++) {
        const MwtSchedEdge *e = &b->edges.v[i];
        AvpJson *j = avp_json_obj(a);
        set(a, j, "from", jint(a, e->from));
        set(a, j, "to", jint(a, e->to));
        set(a, j, "latency", jint(a, e->latency));
        AvpJson *ks = avp_json_arr(a);
        for (int k = 0; k < MWT_DEP_N; k++)
            if (e->kinds & (1u << k)) avp_json_arr_push(a, ks, avp_json_strz(a, mwt_sched_dep_name((MwtDepKind)k)));
        set(a, j, "kinds", ks);
        set(a, j, "registers", jstr(a, e->regs));
        if (e->site) set(a, j, "site", jint(a, e->site));
        avp_json_arr_push(a, es, j);
    }
    set(a, o, "edges", es);
    AvpJson *ms = avp_json_arr(a);
    for (size_t i = 0; i < b->mems.n; i++) {
        const MwtSchedMem *m = &b->mems.v[i];
        AvpJson *j = avp_json_obj(a);
        set(a, j, "from", jint(a, m->from));
        set(a, j, "to", jint(a, m->to));
        set(a, j, "kind", avp_json_strz(a, m->kind == 'l' ? "load-store" : m->kind == 's' ? "store-load" : "store-store"));
        set(a, j, "precheck", jint(a, m->precheck));
        set(a, j, "may_alias", jint(a, m->alias));
        avp_json_arr_push(a, ms, j);
    }
    set(a, o, "memory", ms);
    AvpJson *ss = avp_json_arr(a);
    for (size_t i = 0; i < b->sels.n; i++) {
        const MwtSchedSel *s = &b->sels.v[i];
        AvpJson *j = avp_json_obj(a);
        set(a, j, "cycle", jint(a, s->cycle));
        set(a, j, "slot", jint(a, s->slot));
        set(a, j, "ready", jints(a, &s->ready));
        set(a, j, "busy", jints(a, &s->busy));
        set(a, j, "incumbent", jint(a, s->incumbent));
        set(a, j, "chosen", jint(a, s->chosen));
        AvpJson *cs = avp_json_arr(a);
        for (size_t k = 0; k < s->cmps.n; k++) {
            const MwtSchedCmp *m = &s->cmps.v[k];
            AvpJson *c = avp_json_obj(a);
            set(a, c, "candidate", jint(a, m->cand));
            set(a, c, "incumbent", jint(a, m->inc));
            set(a, c, "candidate_successors", jint(a, m->cand_succ));
            set(a, c, "incumbent_successors", jint(a, m->inc_succ));
            set(a, c, "taken", avp_json_bool(m->taken));
            set(a, c, "key", avp_json_strz(a, mwt_sched_key_name(m->key)));
            avp_json_arr_push(a, cs, c);
        }
        set(a, j, "comparisons", cs);
        avp_json_arr_push(a, ss, j);
    }
    set(a, o, "selections", ss);
    set(a, o, "order", jints(a, &b->order));
    return o;
}

AvpJson *mwt_sched_json(AvpArena *a, const MwtSchedTrace *t, const MwtSchedFunc *f, const MwtSchedCheck *c) {
    AvpJson *o = avp_json_obj(a);
    set(a, o, "link", jstr(a, mwt_sched_link(t, f)));
    set(a, o, "name", jstr(a, f->name));
    set(a, o, "build", jstr(a, t->build));
    AvpJson *ps = avp_json_arr(a);
    for (size_t pi = 0; pi < f->passes.n; pi++) {
        const MwtSchedPass *p = &f->passes.v[pi];
        AvpJson *j = avp_json_obj(a);
        set(a, j, "pass", jint(a, p->pass));
        set(a, j, "argument", jint(a, p->arg));
        set(a, j, "coloring", jint(a, p->coloring));
        set(a, j, "mode", jint(a, p->mode));
        set(a, j, "processor", jint(a, p->proc));
        set(a, j, "width", jint(a, p->width));
        set(a, j, "machine_4", jint(a, p->m4));
        set(a, j, "machine_8", jint(a, p->m8));
        set(a, j, "x112", jint(a, p->x112));
        set(a, j, "precheck", jint(a, p->pre));
        AvpJson *vs = avp_json_arr(a);
        for (size_t i = 0; i < p->visits.n; i++) {
            AvpJson *v = avp_json_obj(a);
            set(a, v, "index", jint(a, p->visits.v[i].index));
            set(a, v, "count", jint(a, p->visits.v[i].count));
            set(a, v, "flags", jint(a, p->visits.v[i].flags));
            set(a, v, "scheduled", jint(a, p->visits.v[i].sched));
            avp_json_arr_push(a, vs, v);
        }
        set(a, j, "visits", vs);
        AvpJson *bs = avp_json_arr(a);
        for (size_t bi = 0; bi < p->blocks.n; bi++) avp_json_arr_push(a, bs, block_json(a, &p->blocks.v[bi]));
        set(a, j, "blocks", bs);
        avp_json_arr_push(a, ps, j);
    }
    set(a, o, "passes", ps);
    AvpJson *em = avp_json_arr(a);
    for (size_t i = 0; i < f->emitted.n; i++) avp_json_arr_push(a, em, jstr(a, f->emitted.v[i].op));
    set(a, o, "emitted", em);
    if (c) {
        AvpJson *cj = avp_json_obj(a);
        AvpJson *gs = avp_json_arr(a);
        for (size_t i = 0; i < c->gaps.n; i++) {
            const MwtSchedGap *g = &c->gaps.v[i];
            AvpJson *gj = avp_json_obj(a);
            set(a, gj, "stage", jstr(a, g->stage));
            set(a, gj, "pass", jint(a, g->pass));
            set(a, gj, "block", jint(a, g->block));
            set(a, gj, "position", jint(a, g->pos));
            set(a, gj, "why", jstr(a, g->why));
            avp_json_arr_push(a, gs, gj);
        }
        set(a, cj, "gaps", gs);
        set(a, cj, "accounted", jstrs(a, &c->notes));
        set(a, cj, "emitted", jint(a, c->emitted));
        set(a, cj, "rows", jint(a, c->rows));
        set(a, cj, "opcodes_compared", jint(a, c->ops_checked));
        set(a, cj, "registers_compared", jint(a, c->regs_checked));
        set(a, o, "check", cj);
    }
    set(a, o, "log_errors", jstrs(a, &t->errors));
    return o;
}

/* ---- the run ---- */
static bool sched_compile(AvpArena *a, const char *unit, const char *src, const AvpStrVec *adds, const AvpStrVec *drops,
                          const char *outdir, AvpStr only, MwtJob *job, MwtSchedTrace *trace, double *secs,
                          const char **why) {
    if (!mwt_job_init(a, job, unit, src, adds, drops, outdir, why)) return false;
    AvpStr body;
    if (!mwt_build_expand(a, job->table, avp_str(MWT_SCHED_SPEC), &body, why)) return false;
    AvpStr spec = avp_str(avp_sprintf(a, "only = \"%.*s\"\n%.*s", AVP_SF(only), AVP_SF(body)));
    AvpStr log;
    i64 code = -1;
    if (!mwt_job_run(a, job, spec, "sched.wdbg", &log, secs, &code, why)) return false;
    if (code != 0) {
        *why = avp_sprintf(a, "the compiler exited with %lld (log: %s/log.txt)", (long long)code, job->outdir);
        return false;
    }
    mwt_sched_parse(a, log, trace);
    return true;
}

/* The self-checks of one traced function against the object's rows (om: the compile's object, NULL unreadable). */
static void sched_fn_checks(AvpArena *a, MwtSchedRun *r, AvpMap *om, const char *sym) {
    mwt_sched_check_trace(a, r->fn, &r->check);
    AvpStr link = avp_str(sym);
    AvpVal *v = om ? avp_map_get(om, link) : NULL;
    if (!v && om) {
        link = mwt_sched_link(&r->trace, r->fn);
        v = avp_map_get(om, link);
    }
    const CcFn *of = v ? v->p : NULL;
    AvpStr *rows = NULL;
    if (of) {
        rows = AVP_NEWN(a, AvpStr, of->nrows ? of->nrows : 1);
        for (size_t i = 0; i < of->nrows; i++) rows[i] = of->rows[i].text;
    }
    mwt_sched_check_object(a, r->fn, rows, of ? of->nrows : 0, &r->check);
    for (size_t i = 0; i < r->trace.errors.n; i++) {
        MwtSchedGap g = {AVP_S("trace"), -1, -1, -1,
                         avp_str(avp_sprintf(a, "the log has %.*s", AVP_SF(r->trace.errors.v[i])))};
        avp_vec_push(&r->check.gaps, g);
    }
}

static AvpMap *load_object(AvpArena *a, const MwtJob *job, const char *outdir) {
    CcRepo cr = {job->repo->root, job->repo->version};
    AvpStr err = {0};
    return cc_load_object(a, avp_path_join(a, outdir, "o.o"), cc_repo_dtk(a, &cr), &err);
}

bool mwt_sched_run(AvpArena *a, const char *unit, const char *src, const AvpStrVec *adds, const AvpStrVec *drops,
                   const char *outdir, const char *sym, MwtSchedRun *r) {
    memset(r, 0, sizeof *r);
    double t0 = avp_perf_counter();
    const char *why = NULL;
    if (!sched_compile(a, unit, src, adds, drops, outdir, mwt_ra_source_name(avp_str(sym)), &r->job, &r->trace, &r->secs,
                       &why)) {
        r->error = why;
        return false;
    }
    r->fn = mwt_sched_find(a, &r->trace, sym, &why);
    if (!r->fn) {
        r->error = why;
        return false;
    }
    sched_fn_checks(a, r, load_object(a, &r->job, outdir), sym);
    r->total_secs = avp_perf_counter() - t0;
    return true;
}

bool mwt_sched_unit_run(AvpArena *a, const char *unit, const char *src, const AvpStrVec *adds, const AvpStrVec *drops,
                        const char *outdir, MwtSchedUnit *u, const char **why) {
    memset(u, 0, sizeof *u);
    double t0 = avp_perf_counter();
    if (!sched_compile(a, unit, src, adds, drops, outdir, AVP_S(""), &u->job, &u->trace, &u->secs, why)) return false;
    u->om = load_object(a, &u->job, outdir);
    u->total_secs = avp_perf_counter() - t0;
    return true;
}

bool mwt_sched_unit_fn(AvpArena *a, const MwtSchedUnit *u, const char *sym, MwtSchedRun *r) {
    memset(r, 0, sizeof *r);
    r->job = u->job;
    r->trace = u->trace;
    r->secs = u->secs;
    const char *why = NULL;
    r->fn = mwt_sched_find(a, &r->trace, sym, &why);
    if (!r->fn) {
        r->error = why;
        return false;
    }
    sched_fn_checks(a, r, u->om, sym);
    return true;
}

static bool trace_one(AvpArena *a, const char *unit, const char *src, const AvpStrVec *adds, const AvpStrVec *drops,
                      const char *outdir, const char *sym, MwtSchedRun *r) {
    return mwt_sched_run(a, unit, src, adds, drops, outdir, sym, r);
}

/* "#12" or "12": a node index; "@40": an emitted position (the object's row) */
static bool parse_ref(const char *s, bool *at, int *v) {
    *at = s[0] == '@';
    const char *d = s + (s[0] == '@' || s[0] == '#');
    if (!*d) return false;
    char *end;
    long x = strtol(d, &end, 10);
    if (*end || x < 0) return false;
    *v = (int)x;
    return true;
}

static const char *usage(void) {
    return "usage: mwtrace.exe sched <unit> --function SYM [--src FILE] [--against FILE2] [--pair A,B [--block K]\n"
           "                             [--pass P]] [--flag \"<words>\"]... [--drop-flag W]... [--json] [--out DIR]\n"
           "\n"
           "Compiles the unit (or --src FILE with the unit's flags) under the debugger and prints what the instruction\n"
           "scheduler decided for one function: per pass (1 before register allocation, 2 after) and scheduled block, the\n"
           "nodes in input order (opcode, registers, latency, height, slack, predecessors, opcode priority, regalloc's\n"
           "instruction number, output position, issue cycle, emitted position), the dependence edges with their kind\n"
           "(raw, waw, war, mem-*: may_alias said 1, ordered, serial, to-serial, branch) and latency, every memory pair\n"
           "may_alias was asked about, then each selection: the node issued, the key that decided against every other\n"
           "ready node (critical, successors, height, priority, order), the ready list, and the block's output order.\n"
           "Self-check: every selection replayed from the trace's own edges, the emitted instructions against the\n"
           "object's rows, each pass-2 block's output order against the emitted order; a disagreement is TRACE-GAP\n"
           "(exit 1) with its stage (trace, after-scheduler).\n"
           "--against FILE2 traces a second text and prints the first pass, block and selection where the issued node\n"
           "differs, with both decisions. --pair A,B (node indices #N of --block K in --pass P, default 2; or @N, the\n"
           "function's emitted instruction N = the object's row N) prints the selection that issued one while the other\n"
           "was ready and the key that decided, or the edge that held the other. Output: <out>/sched.txt, sched.json\n"
           "(default out: build/mwtrace/<unit path>/sched/<SYM>; with --against, <out>/a and <out>/b).";
}

int mwt_sched_main(AvpArena *a, int argc, char **argv) {
    const char *unit = NULL, *sym = NULL, *src = NULL, *against = NULL, *out = NULL, *pair = NULL;
    int block = -1, pass = 2;
    bool json = false;
    AvpStrVec adds = {0}, drops = {0};
    for (int i = 0; i < argc; i++) {
        const char *s = argv[i];
        if (!strcmp(s, "--help") || !strcmp(s, "-h")) {
            puts(usage());
            return 0;
        } else if (!strcmp(s, "--json")) {
            json = true;
        } else if ((!strcmp(s, "--function") || !strcmp(s, "--flag") || !strcmp(s, "--drop-flag") || !strcmp(s, "--src") ||
                    !strcmp(s, "--against") || !strcmp(s, "--out") || !strcmp(s, "--pair") || !strcmp(s, "--block") ||
                    !strcmp(s, "--pass")) &&
                   i + 1 < argc) {
            const char *v = argv[++i];
            if (!strcmp(s, "--function")) sym = v;
            else if (!strcmp(s, "--flag")) avp_vec_push(&adds, avp_str(v));
            else if (!strcmp(s, "--drop-flag")) avp_vec_push(&drops, avp_str(v));
            else if (!strcmp(s, "--src")) src = v;
            else if (!strcmp(s, "--against")) against = v;
            else if (!strcmp(s, "--pair")) pair = v;
            else if (!strcmp(s, "--block")) block = atoi(v);
            else if (!strcmp(s, "--pass")) pass = atoi(v);
            else out = v;
        } else if (s[0] == '-' || unit) {
            fprintf(stderr, "mwtrace.exe sched: unexpected argument %s\n%s\n", s, usage());
            return 2;
        } else {
            unit = s;
        }
    }
    if (!unit || !sym) {
        fprintf(stderr, "%s\n", usage());
        return 2;
    }
    bool at1 = false, at2 = false;
    int r1 = -1, r2 = -1;
    if (pair) {
        const char *comma = strchr(pair, ',');
        if (!comma || !parse_ref(avp_cz(a, avp_strn(pair, (size_t)(comma - pair))), &at1, &r1) ||
            !parse_ref(comma + 1, &at2, &r2) || ((!at1 || !at2) && block < 0) || (pass != 1 && pass != 2)) {
            fprintf(stderr, "mwtrace.exe sched: --pair takes A,B: #N node indices (with --block K) or @N emitted "
                            "positions; --pass 1 or 2\n");
            return 2;
        }
    }
    const ShsmRepo *repo = shsm_repo_find(NULL);
    const char *path = strncmp(unit, "main/", 5) == 0 ? unit + 5 : unit;
    const char *outdir = out ? avp_path_abspath(a, out) : avp_join(a, repo->root, "build", "mwtrace", path, "sched", sym);
    const char *build_dir = avp_path_abspath(a, avp_path_join(a, repo->root, "build"));
    if (strncmp(avp_path_normcase(a, outdir), avp_path_normcase(a, build_dir), strlen(build_dir)) != 0) {
        fprintf(stderr, "mwtrace.exe sched: --out must be under %s\n", build_dir);
        return 2;
    }
    MwtSchedRun A, B;
    const char *dir_a = against ? avp_path_join(a, outdir, "a") : outdir;
    avp_makedirs(dir_a, true);
    if (!trace_one(a, unit, src, &adds, &drops, dir_a, sym, &A)) {
        fprintf(stderr, "mwtrace.exe sched: %s\n", A.error);
        return 2;
    }
    AvpBuf tb;
    avp_buf_init(&tb, a);
    avp_buf_printf(&tb, "mwtrace sched %s %s: %.2f s%s\n  source: %s\n", A.job.unit->name, sym, A.secs,
                   against ? " (A)" : "", A.job.source);
    avp_buf_put(&tb, mwt_sched_text(a, &A.trace, A.fn, &A.check));
    AvpJson *js = mwt_sched_json(a, &A.trace, A.fn, &A.check);
    set(a, js, "unit", avp_json_strz(a, A.job.unit->name));
    set(a, js, "function", avp_json_strz(a, sym));
    set(a, js, "source", avp_json_strz(a, A.job.source));
    set(a, js, "seconds", avp_json_float(a, avp_clock_round(A.secs, 2)));
    bool gaps = A.check.gaps.n > 0;
    int rc = 0;
    if (pair) {
        int b1 = block, b2 = block, n1 = r1, n2 = r2;
        bool ok = true;
        if (at1 && !mwt_sched_locate(A.fn, pass, r1, &b1, &n1)) ok = false;
        if (at2 && !mwt_sched_locate(A.fn, pass, r2, &b2, &n2)) ok = false;
        MwtSchedPair pr;
        const char *why = "the two instructions are not nodes of one block of that pass";
        if (ok && b1 == b2 && mwt_sched_pair(a, A.fn, pass, b1, n1, n2, &pr, &why)) {
            avp_buf_putz(&tb, "\npair:\n");
            avp_buf_put(&tb, mwt_sched_pair_text(a, A.fn, &pr));
            set(a, js, "pair", mwt_sched_pair_json(a, A.fn, &pr));
        } else {
            avp_buf_printf(&tb, "\npair: %s\n", why);
            set(a, js, "pair", avp_json_strz(a, why));
            rc = 2;
        }
    }
    if (against) {
        const char *dir_b = avp_path_join(a, outdir, "b");
        avp_makedirs(dir_b, true);
        if (!trace_one(a, unit, against, &adds, &drops, dir_b, sym, &B)) {
            fprintf(stderr, "mwtrace.exe sched: %s (--against)\n", B.error);
            return 2;
        }
        gaps = gaps || B.check.gaps.n > 0;
        avp_buf_printf(&tb, "\nB: %s: %.2f s\n", B.job.source, B.secs);
        avp_buf_put(&tb, mwt_sched_text(a, &B.trace, B.fn, &B.check));
        AvpStrVec d = mwt_sched_diff(a, A.fn, B.fn);
        avp_buf_putz(&tb, "\ndifferences (A: the first source, B: --against):\n");
        for (size_t i = 0; i < d.n; i++) avp_buf_printf(&tb, "  %.*s\n", AVP_SF(d.v[i]));
        AvpJson *root = avp_json_obj(a);
        AvpJson *jb = mwt_sched_json(a, &B.trace, B.fn, &B.check);
        set(a, jb, "source", avp_json_strz(a, B.job.source));
        set(a, jb, "seconds", avp_json_float(a, avp_clock_round(B.secs, 2)));
        set(a, root, "a", js);
        set(a, root, "b", jb);
        set(a, root, "differences", jstrs(a, &d));
        js = root;
    }
    AvpStr all = avp_buf_str(&tb);
    AvpJsonDumpOpts o = AVP_JSON_INDENT(1);
    AvpStr jtext = avp_json_dumps(a, js, &o);
    avp_write_file(avp_path_join(a, outdir, "sched.txt"), all.p, all.n);
    avp_write_file(avp_path_join(a, outdir, "sched.json"), jtext.p, jtext.n);
    if (json) printf("%.*s\n", AVP_SF(jtext));
    else fwrite(all.p, 1, all.n, stdout);
    if (rc) return rc;
    return gaps ? 1 : 0;
}
