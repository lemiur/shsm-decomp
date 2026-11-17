/* mwtrace regalloc: one function's register allocation traced under the debugger, checked against the object of the
 * same compile, reported as text and JSON; with --against, the same function traced in a second source text and the
 * two traces compared (mwt_regalloc.h). The traced compile adds -sym on (its DWARF is the self-check's variable map);
 * a second compile without it, under the debugger with no breakpoints, must give the same code for the function. */
#include "mwt_regalloc.h"

#include "cc_compare.h"
#include "cc_dwdump.h"
#include "cc_listing.h"

static const char *cls_name(int cls) {
    switch (cls) {
    case MWT_RC_GPR: return "GPR";
    case MWT_RC_FPR: return "FPR";
    case MWT_RC_CR: return "CR";
    case MWT_RC_VR: return "VR";
    default: return "SPR";
    }
}

static const char *how_name(MwtPushHow h) {
    return h == MWT_PUSH_LOW ? "low" : h == MWT_PUSH_OPTIMISTIC ? "optimistic" : h == MWT_PUSH_AFTER ? "after" : "-";
}

static const MwtRaNode *pass_node(const MwtRaPass *p, int idx) {
    if (!p || idx < p->nreal || idx >= p->used) return NULL;
    return &p->nodes.v[idx - p->nreal];
}

static u32 phys_mask(const MwtRaNode *n) {
    u32 m = 0;
    for (size_t i = 0; i < n->phys.n; i++)
        if (n->phys.v[i] >= 0 && n->phys.v[i] < 32) m |= 1u << n->phys.v[i];
    return m;
}

/* Interferes with every volatile register of the class (what a call's clobbers give): derived from the edges. */
static bool across_call(const MwtRaPass *p, const MwtRaNode *n) { return p->vol && (phys_mask(n) & p->vol) == p->vol; }

static const char *kind_name(int k) {
    return k == 'p' ? "parameter" : k == 'l' ? "local" : k == 'o' ? "object" : "temp";
}

/* ---- text ------------------------------------------------------------------------------------------------------ */

static void pass_text(AvpArena *a, AvpBuf *b, const MwtRaPass *p, bool final) {
    avp_buf_printf(b, "%s (class %d), attempt %d%s: physical nodes 0-%d, virtual nodes %d-%d, K %d; volatile %s; "
                      "allocatable %s; %s\n",
                   cls_name(p->cls), p->cls, p->iter, final ? "" : " (spilled)", p->nreal - 1, p->nreal, p->used - 1,
                   p->K, mwt_ra_mask(a, p->vol, p->cls), mwt_ra_mask(a, p->avail, p->cls),
                   p->ok == 1 ? "coloured" : p->ok == 0 ? "spill" : "select not seen");
    if (p->vle) avp_buf_putz(b, "  VLE on: simplify's stack was re-sorted by node +0x1a (machine_specific_sort_graph)\n");
    if (!p->nodes.n) return;
    avp_buf_printf(b, "  %-4s %-5s %-30s %-6s %-6s %-22s %-18s %-5s %s\n", "sel", "node", "variable", "degree",
                   "cost", "pushed", "free", "+nv", "register");
    for (size_t i = 0; i < p->select.n; i++) {
        const MwtRaNode *n = pass_node(p, p->select.v[i]);
        if (!n) continue;
        AvpStr lab = mwt_ra_label(a, n);
        const char *pushed = n->push_pos >= 0 ? avp_sprintf(a, "#%d %s s%d d%d", n->push_pos + 1, how_name(n->push_how),
                                                            n->push_sweep, n->push_degree)
                                              : "-";
        avp_buf_printf(b, "  %-4zu %-5d %-30.*s %-6d %-6s %-22s %-18s %-5s %s", i + 1, n->index, AVP_SF(lab), n->degree,
                       p->estimated ? avp_sprintf(a, "%lld", (long long)n->cost) : "-", pushed,
                       mwt_ra_mask(a, n->free_mask, p->cls), n->nv_added >= 0 ? mwt_ra_reg(a, p->cls, n->nv_added) : "-",
                       n->spilled ? "spilled" : mwt_ra_reg(a, p->cls, n->final_reg));
        if (n->kind == 'p' || n->kind == 'l') avp_buf_printf(b, "  (%s)", kind_name(n->kind));
        if (across_call(p, n)) avp_buf_putz(b, "  across a call");
        if (n->members.n) {
            avp_buf_putz(b, "  coalesced:");
            for (size_t k = 0; k < n->members.n; k++) {
                const MwtRaNode *m = pass_node(p, n->members.v[k]);
                avp_buf_printf(b, " %d", n->members.v[k]);
                if (m && (m->kind == 'p' || m->kind == 'l')) avp_buf_printf(b, " %.*s", AVP_SF(m->name));
            }
        }
        avp_buf_putc(b, '\n');
    }
    /* nodes select did not pop: coalesced into another node or a physical register, or never in the stack */
    for (size_t i = 0; i < p->nodes.n; i++) {
        const MwtRaNode *n = &p->nodes.v[i];
        if (n->select_pos >= 0) continue;
        avp_buf_printf(b, "  -    %-5d %-30.*s %-6d", n->index, AVP_SF(mwt_ra_label(a, n)), n->degree);
        if (n->alias != n->index) {
            int r = n->alias;
            while (r >= p->nreal && pass_node(p, r) && pass_node(p, r)->alias != r) r = pass_node(p, r)->alias;
            if (r < p->nreal) avp_buf_printf(b, " coalesced into %s (physical)", mwt_ra_reg(a, p->cls, r));
            else avp_buf_printf(b, " coalesced into node %d", r);
        } else {
            avp_buf_printf(b, " not popped (flags %#x)", n->flags);
        }
        avp_buf_printf(b, " -> %s\n", n->spilled ? "spilled" : mwt_ra_reg(a, p->cls, n->final_reg));
    }
    avp_buf_putz(b, "  simplify order:");
    int sw = -1;
    for (size_t i = 0; i < p->push.n; i++) {
        const MwtRaNode *n = pass_node(p, p->push.v[i]);
        if (!n) continue;
        if (n->push_sweep != sw) {
            sw = n->push_sweep;
            avp_buf_printf(b, " [sweep %d]", sw);
        }
        avp_buf_printf(b, " %d%s", n->index, n->push_how == MWT_PUSH_OPTIMISTIC ? "(optimistic)" : "");
    }
    avp_buf_putc(b, '\n');
    for (size_t i = 0; i < p->choices.n; i++) {
        const MwtRaChoice *c = &p->choices.v[i];
        avp_buf_printf(b, "  optimistic choice: node %d, the smallest spill cost / degree among", c->chosen);
        for (size_t k = 0; k < c->cands.n; k++) {
            const MwtRaCand *x = &c->cands.v[k];
            avp_buf_printf(b, " %d (%lld/%d", x->index, (long long)x->cost, x->degree);
            if (x->flags & 0x80) avp_buf_putz(b, " flag 0x80: FLT_MAX");
            else if (x->flags & 0x400) avp_buf_putz(b, " flag 0x400: 1.7e38");
            avp_buf_putc(b, ')');
        }
        avp_buf_putc(b, '\n');
    }
    if (p->merges.n) {
        avp_buf_putz(b, "  coalescing:");
        for (size_t i = 0; i < p->merges.n; i++) {
            const MwtRaMerge *m = &p->merges.v[i];
            if (m->why.n) avp_buf_printf(b, " %d/%d refused (%.*s);", m->lo, m->hi, AVP_SF(m->why));
            else avp_buf_printf(b, " %d<-%d;", m->lo, m->hi);
        }
        avp_buf_putc(b, '\n');
    }
    if (p->nv.n) {
        avp_buf_putz(b, "  nonvolatile registers added:");
        for (size_t i = 0; i < p->nv.n; i++) avp_buf_printf(b, " %s", mwt_ra_reg(a, p->cls, p->nv.v[i]));
        avp_buf_putc(b, '\n');
    }
    if (p->spills.n) {
        avp_buf_putz(b, "  spilled:");
        for (size_t i = 0; i < p->spills.n; i++) avp_buf_printf(b, " %d", p->spills.v[i]);
        avp_buf_putc(b, '\n');
    }
    bool any = false;
    for (size_t i = 0; i < p->vars.n; i++) {
        const MwtRaVar *v = &p->vars.v[i];
        if (v->vclass != p->cls) continue;
        if (!any) avp_buf_putz(b, "  variables (the parameter and locals lists, VarInfo):");
        any = true;
        if (!(v->vflags & 2)) avp_buf_printf(b, " %.*s memory;", AVP_SF(v->name));
        else {
            const MwtRaNode *n = pass_node(p, v->reg);
            avp_buf_printf(b, " %.*s node %d %s;", AVP_SF(v->name), v->reg,
                           n ? (n->spilled ? "spilled" : mwt_ra_reg(a, p->cls, n->final_reg)) : mwt_ra_reg(a, p->cls, v->reg));
        }
    }
    if (any) avp_buf_putc(b, '\n');
}

static void check_text(AvpArena *a, AvpBuf *b, const MwtRaCheck *c) {
    if (!c) return;
    if (!c->gaps.n) avp_buf_putz(b, "self-check: ok");
    else avp_buf_printf(b, "TRACE-GAP (%.*s): %.*s", AVP_SF(c->gaps.v[0].what), AVP_SF(c->gaps.v[0].why));
    avp_buf_putc(b, '\n');
    for (size_t i = 1; i < c->gaps.n; i++)
        avp_buf_printf(b, "  also (%.*s): %.*s\n", AVP_SF(c->gaps.v[i].what), AVP_SF(c->gaps.v[i].why));
    avp_buf_printf(b, "  DWARF: %zu variables agree:", c->checked.n);
    for (size_t i = 0; i < c->checked.n; i++) avp_buf_printf(b, " %.*s;", AVP_SF(c->checked.v[i]));
    avp_buf_putc(b, '\n');
    if (c->prologue_checked)
        avp_buf_printf(b, "  prologue: saved %s %s; assigned %s %s\n", mwt_ra_mask(a, c->saved_nv[MWT_RC_GPR], MWT_RC_GPR),
                       mwt_ra_mask(a, c->saved_nv[MWT_RC_FPR], MWT_RC_FPR), mwt_ra_mask(a, c->trace_nv[MWT_RC_GPR], MWT_RC_GPR),
                       mwt_ra_mask(a, c->trace_nv[MWT_RC_FPR], MWT_RC_FPR));
    if (c->uncovered.n) {
        avp_buf_putz(b, "  not covered:");
        for (size_t i = 0; i < c->uncovered.n; i++) avp_buf_printf(b, " %.*s;", AVP_SF(c->uncovered.v[i]));
        avp_buf_putc(b, '\n');
    }
    avp_buf_putz(b, "  not covered by the checks: classes other than GPR and FPR, temporaries' registers (no DWARF)\n");
}

AvpStr mwt_ra_text(AvpArena *a, const MwtRaTrace *t, const MwtRaFunc *f, const MwtRaCheck *c) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_printf(&b, "%.*s (%.*s)\n", AVP_SF(mwt_ra_link(t, f)), AVP_SF(t->build));
    for (size_t i = 0; i < f->passes.n; i++) {
        const MwtRaPass *p = &f->passes.v[i];
        pass_text(a, &b, p, mwt_ra_final(f, p->cls) == p);
    }
    check_text(a, &b, c);
    return avp_buf_str(&b);
}

/* ---- JSON ------------------------------------------------------------------------------------------------------ */

static void set(AvpArena *a, AvpJson *o, const char *k, AvpJson *v) { avp_json_set(a, o, avp_str(k), v); }
static AvpJson *jint(AvpArena *a, i64 v) { return avp_json_int(a, v); }
static AvpJson *jstrz(AvpArena *a, const char *s) { return avp_json_strz(a, s); }
static AvpJson *jints(AvpArena *a, const MwtInts *v) {
    AvpJson *r = avp_json_arr(a);
    for (size_t i = 0; i < v->n; i++) avp_json_arr_push(a, r, jint(a, v->v[i]));
    return r;
}
static AvpJson *jstrs(AvpArena *a, const AvpStrVec *v) {
    AvpJson *r = avp_json_arr(a);
    for (size_t i = 0; i < v->n; i++) avp_json_arr_push(a, r, avp_json_str(a, v->v[i]));
    return r;
}

static AvpJson *pass_json(AvpArena *a, const MwtRaPass *p, bool final) {
    AvpJson *o = avp_json_obj(a);
    set(a, o, "class", jstrz(a, cls_name(p->cls)));
    set(a, o, "class_number", jint(a, p->cls));
    set(a, o, "attempt", jint(a, p->iter));
    set(a, o, "final", avp_json_bool(final));
    set(a, o, "physical", jint(a, p->nreal));
    set(a, o, "nodes_used", jint(a, p->used));
    set(a, o, "K", jint(a, p->K));
    set(a, o, "vle", avp_json_bool(p->vle));
    set(a, o, "volatile", jstrz(a, mwt_ra_mask(a, p->vol, p->cls)));
    set(a, o, "allocatable", jstrz(a, mwt_ra_mask(a, p->avail, p->cls)));
    set(a, o, "spill_costs_estimated", avp_json_bool(p->estimated));
    set(a, o, "coloured", p->ok < 0 ? avp_json_null() : avp_json_bool(p->ok == 1));
    set(a, o, "sweeps", jint(a, p->sweeps));
    set(a, o, "simplify_order", jints(a, &p->push));
    set(a, o, "select_order", jints(a, &p->select));
    AvpJson *nv = avp_json_arr(a);
    for (size_t i = 0; i < p->nv.n; i++) avp_json_arr_push(a, nv, jstrz(a, mwt_ra_reg(a, p->cls, p->nv.v[i])));
    set(a, o, "nonvolatile_added", nv);
    set(a, o, "spilled", jints(a, &p->spills));
    AvpJson *nodes = avp_json_arr(a);
    for (size_t i = 0; i < p->nodes.n; i++) {
        const MwtRaNode *n = &p->nodes.v[i];
        AvpJson *x = avp_json_obj(a);
        set(a, x, "node", jint(a, n->index));
        set(a, x, "label", avp_json_str(a, mwt_ra_label(a, n)));
        set(a, x, "kind", jstrz(a, kind_name(n->kind)));
        set(a, x, "name", n->kind == 't' ? avp_json_null() : avp_json_str(a, n->name));
        set(a, x, "object", n->obj ? jstrz(a, avp_sprintf(a, "%#llx", (unsigned long long)n->obj)) : avp_json_null());
        set(a, x, "object_inherited", avp_json_bool(n->inherited));
        set(a, x, "alias", jint(a, n->alias));
        set(a, x, "flags", jint(a, n->flags));
        set(a, x, "degree", jint(a, n->degree));
        set(a, x, "spill_cost", p->estimated ? jint(a, n->cost) : avp_json_null());
        set(a, x, "rematerialisable", avp_json_bool(n->remat != 0));
        set(a, x, "first_def", n->def_at >= 0 ? jint(a, n->def_at) : avp_json_null());
        set(a, x, "first_def_op", n->def_at >= 0 ? avp_json_str(a, n->def_op) : avp_json_null());
        set(a, x, "defs", jint(a, n->ndefs));
        set(a, x, "simplify_pos", n->push_pos >= 0 ? jint(a, n->push_pos + 1) : avp_json_null());
        set(a, x, "push", jstrz(a, how_name(n->push_how)));
        set(a, x, "push_sweep", jint(a, n->push_sweep));
        set(a, x, "push_degree", jint(a, n->push_degree));
        set(a, x, "select_pos", n->select_pos >= 0 ? jint(a, n->select_pos + 1) : avp_json_null());
        set(a, x, "free", jstrz(a, mwt_ra_mask(a, n->free_mask, p->cls)));
        set(a, x, "nonvolatile_added", n->nv_added >= 0 ? jstrz(a, mwt_ra_reg(a, p->cls, n->nv_added)) : avp_json_null());
        set(a, x, "physical_neighbours", jstrz(a, mwt_ra_mask(a, phys_mask(n), p->cls)));
        set(a, x, "across_call", avp_json_bool(across_call(p, n)));
        set(a, x, "coalesced", jints(a, &n->members));
        set(a, x, "spilled", avp_json_bool(n->spilled));
        set(a, x, "register", n->spilled || n->final_reg < 0 ? avp_json_null() : jstrz(a, mwt_ra_reg(a, p->cls, n->final_reg)));
        avp_json_arr_push(a, nodes, x);
    }
    set(a, o, "nodes", nodes);
    AvpJson *ch = avp_json_arr(a);
    for (size_t i = 0; i < p->choices.n; i++) {
        AvpJson *c = avp_json_obj(a);
        set(a, c, "chosen", jint(a, p->choices.v[i].chosen));
        AvpJson *cs = avp_json_arr(a);
        for (size_t k = 0; k < p->choices.v[i].cands.n; k++) {
            const MwtRaCand *x = &p->choices.v[i].cands.v[k];
            AvpJson *e = avp_json_obj(a);
            set(a, e, "node", jint(a, x->index));
            set(a, e, "spill_cost", jint(a, x->cost));
            set(a, e, "degree", jint(a, x->degree));
            set(a, e, "flags", jint(a, x->flags));
            avp_json_arr_push(a, cs, e);
        }
        set(a, c, "candidates", cs);
        avp_json_arr_push(a, ch, c);
    }
    set(a, o, "optimistic_choices", ch);
    AvpJson *ms = avp_json_arr(a);
    for (size_t i = 0; i < p->merges.n; i++) {
        AvpJson *m = avp_json_obj(a);
        set(a, m, "lo", jint(a, p->merges.v[i].lo));
        set(a, m, "hi", jint(a, p->merges.v[i].hi));
        set(a, m, "refused", p->merges.v[i].why.n ? avp_json_str(a, p->merges.v[i].why) : avp_json_null());
        avp_json_arr_push(a, ms, m);
    }
    set(a, o, "coalescing", ms);
    AvpJson *vs = avp_json_arr(a);
    for (size_t i = 0; i < p->vars.n; i++) {
        const MwtRaVar *v = &p->vars.v[i];
        if (v->vclass != p->cls) continue;
        const MwtRaNode *n = (v->vflags & 2) ? pass_node(p, v->reg) : NULL;
        AvpJson *x = avp_json_obj(a);
        set(a, x, "name", avp_json_str(a, v->name));
        set(a, x, "parameter", avp_json_bool(v->is_param));
        set(a, x, "in_register", avp_json_bool((v->vflags & 2) != 0));
        set(a, x, "node", (v->vflags & 2) ? jint(a, v->reg) : avp_json_null());
        set(a, x, "register", n && !n->spilled && n->final_reg >= 0 ? jstrz(a, mwt_ra_reg(a, p->cls, n->final_reg)) : avp_json_null());
        avp_json_arr_push(a, vs, x);
    }
    set(a, o, "variables", vs);
    return o;
}

static AvpJson *check_json(AvpArena *a, const MwtRaCheck *c) {
    AvpJson *o = avp_json_obj(a);
    set(a, o, "ok", avp_json_bool(!c->gaps.n));
    AvpJson *g = avp_json_arr(a);
    for (size_t i = 0; i < c->gaps.n; i++) {
        AvpJson *x = avp_json_obj(a);
        set(a, x, "check", avp_json_str(a, c->gaps.v[i].what));
        set(a, x, "why", avp_json_str(a, c->gaps.v[i].why));
        avp_json_arr_push(a, g, x);
    }
    set(a, o, "gaps", g);
    set(a, o, "dwarf_agree", jstrs(a, &c->checked));
    set(a, o, "not_covered", jstrs(a, &c->uncovered));
    set(a, o, "prologue_checked", avp_json_bool(c->prologue_checked));
    set(a, o, "saved_gpr", jstrz(a, mwt_ra_mask(a, c->saved_nv[MWT_RC_GPR], MWT_RC_GPR)));
    set(a, o, "saved_fpr", jstrz(a, mwt_ra_mask(a, c->saved_nv[MWT_RC_FPR], MWT_RC_FPR)));
    set(a, o, "assigned_gpr", jstrz(a, mwt_ra_mask(a, c->trace_nv[MWT_RC_GPR], MWT_RC_GPR)));
    set(a, o, "assigned_fpr", jstrz(a, mwt_ra_mask(a, c->trace_nv[MWT_RC_FPR], MWT_RC_FPR)));
    return o;
}

AvpJson *mwt_ra_json(AvpArena *a, const MwtRaTrace *t, const MwtRaFunc *f, const MwtRaCheck *c) {
    AvpJson *o = avp_json_obj(a);
    set(a, o, "function", avp_json_str(a, mwt_ra_link(t, f)));
    set(a, o, "source_name", avp_json_str(a, f->name));
    set(a, o, "compiler", avp_json_str(a, t->build));
    AvpJson *ps = avp_json_arr(a);
    for (size_t i = 0; i < f->passes.n; i++)
        avp_json_arr_push(a, ps, pass_json(a, &f->passes.v[i], mwt_ra_final(f, f->passes.v[i].cls) == &f->passes.v[i]));
    set(a, o, "passes", ps);
    if (c) set(a, o, "self_check", check_json(a, c));
    return o;
}

/* ---- one traced compile ---------------------------------------------------------------------------------------- */

typedef MwtRaRun RaRun;

static void add_gap(MwtRaCheck *c, const char *what, const char *why) {
    MwtRaGap g = {avp_str(what), avp_str(why)};
    avp_vec_push(&c->gaps, g);
}

static const CcFn *obj_fn(AvpMap *m, AvpStr name) {
    if (!m) return NULL;
    AvpVal *v = avp_map_get(m, name);
    return v ? v->p : NULL;
}

/* The link name of sym in the object (the trace may know the function by its source name only). */
static AvpStr link_in(AvpMap *om, const MwtRaTrace *t, const MwtRaFunc *fn, const char *sym) {
    AvpStr link = avp_str(sym);
    if (!obj_fn(om, link)) link = mwt_ra_link(t, fn);
    return link;
}

/* The per-function self-checks of r->fn: (c) the code check (code_check: against pm, the plain compile's functions),
 * (a) the DWARF (fns: the -sym on object's dump, NULL when none was made), (b) the prologue, the trace's own
 * completeness. The gaps' order is mwtrace regalloc's. */
static void fn_checks(AvpArena *a, RaRun *r, AvpMap *om, AvpMap *pm, bool code_check, bool have_plain, const char *plain_why,
                      const CcDwFns *fns, const char *sym) {
    AvpStr link = link_in(om, &r->trace, r->fn, sym);
    const CcFn *of = obj_fn(om, link), *pf = obj_fn(pm, link);
    /* (c) the -sym on code equals the plain code (not run without code_check: the caller checks it its own way) */
    if (!code_check) {
        if (!of) add_gap(&r->check, "code", avp_sprintf(a, "%.*s is not in the -sym on object", AVP_SF(link)));
    } else if (!have_plain) {
        add_gap(&r->check, "code", avp_sprintf(a, "the compile without -sym on failed: %s", plain_why ? plain_why : "?"));
    } else if (!of || !pf) {
        add_gap(&r->check, "code", avp_sprintf(a, "%.*s is not in both objects", AVP_SF(link)));
    }
    /* (a) DWARF */
    MwtDwVars dw = {0};
    bool have_dwarf = false;
    if (of && fns) {
        const CcDwFn *df = cc_dtk_find(fns, of->start, NULL);
        if (df) {
            have_dwarf = true;
            for (size_t i = 0; i < df->params.n; i++) {
                MwtDwVar v = {avp_str(df->params.v[i].name), avp_str(df->params.v[i].loc ? df->params.v[i].loc : ""), true};
                avp_vec_push(&dw, v);
            }
            for (size_t i = 0; i < df->locals.n; i++) {
                MwtDwVar v = {avp_str(df->locals.v[i].name), avp_str(df->locals.v[i].loc ? df->locals.v[i].loc : ""), false};
                avp_vec_push(&dw, v);
            }
            if (df->this_loc) {
                MwtDwVar v = {AVP_S("this"), avp_str(df->this_loc), true};
                avp_vec_push(&dw, v);
            }
        }
    }
    /* (b) prologue */
    u32 sg = 0, sf = 0;
    if (of) {
        AvpStr *rows = AVP_NEWN(a, AvpStr, of->nrows ? of->nrows : 1);
        for (size_t i = 0; i < of->nrows; i++) rows[i] = of->rows[i].text;
        mwt_ra_saved_regs(rows, of->nrows, &sg, &sf);
    }
    MwtRaGaps pre = r->check.gaps;
    r->check.gaps = (MwtRaGaps){0};
    mwt_ra_check(a, r->fn, &dw, have_dwarf, sg, sf, of != NULL, &r->check);
    for (size_t i = 0; i < pre.n; i++) avp_vec_push(&r->check.gaps, pre.v[i]);
    if (of && pf) {
        bool same = of->nrows == pf->nrows;
        size_t at = 0;
        for (; same && at < of->nrows; at++)
            if (!avp_str_eq(of->rows[at].norm, pf->rows[at].norm)) same = false;
        if (!same)
            add_gap(&r->check, "code", avp_sprintf(a, "-sym on changes the code of %.*s (row %zu)", AVP_SF(link), at));
    }
    if (!have_dwarf) add_gap(&r->check, "dwarf", "the -sym on object's DWARF has no entry for the function");
    for (size_t i = 0; i < r->trace.errors.n; i++)
        add_gap(&r->check, "trace", avp_sprintf(a, "the trace is incomplete: %.*s", AVP_SF(r->trace.errors.v[i])));
    for (size_t i = 0; i < r->fn->passes.n; i++) {
        const MwtRaPass *p = &r->fn->passes.v[i];
        if (mwt_ra_final(r->fn, p->cls) == p && p->ok != 1)
            add_gap(&r->check, "trace", avp_sprintf(a, "%s: the last attempt's select was not seen to colour", cls_name(p->cls)));
    }
}

/* The -sym on compile under the debugger with the spec limited to `only` (a function's source name; "" every
 * function), parsed. false with *why. */
static bool ra_compile(AvpArena *a, const char *unit, const char *src, const AvpStrVec *adds, const AvpStrVec *drops,
                       const char *outdir, AvpStr only, MwtJob *job, MwtRaTrace *trace, double *secs, const char **why) {
    AvpStrVec adds_sym = {0};
    for (size_t i = 0; adds && i < adds->n; i++) avp_vec_push(&adds_sym, adds->v[i]);
    avp_vec_push(&adds_sym, AVP_S("-sym on"));
    if (!mwt_job_init(a, job, unit, src, &adds_sym, drops, outdir, why)) return false;
    AvpStr body;
    if (!mwt_build_expand(a, job->table, avp_str(MWT_REGALLOC_SPEC), &body, why)) return false;
    AvpStr spec = avp_str(avp_sprintf(a, "only = \"%.*s\"\n%.*s", AVP_SF(only), AVP_SF(body)));
    AvpStr log;
    i64 code = -1;
    if (!mwt_job_run(a, job, spec, "regalloc.wdbg", &log, secs, &code, why)) return false;
    if (code != 0) {
        *why = avp_sprintf(a, "the compiler exited with %lld (log: %s/log.txt)", (long long)code, job->outdir);
        return false;
    }
    mwt_ra_parse(a, log, trace);
    return true;
}

static bool trace_one(AvpArena *a, const char *unit, const char *src, const AvpStrVec *adds, const AvpStrVec *drops,
                      const char *outdir, const char *sym, bool code_check, RaRun *r) {
    memset(r, 0, sizeof *r);
    double t_all = avp_perf_counter();
    const char *why = NULL;
    if (!ra_compile(a, unit, src, adds, drops, outdir, mwt_ra_source_name(avp_str(sym)), &r->job, &r->trace, &r->secs, &why)) {
        r->error = why;
        return false;
    }
    r->fn = mwt_ra_find(a, &r->trace, sym, &why);
    if (!r->fn) {
        r->error = why;
        return false;
    }
    /* the same compile without -sym on, for the code check */
    MwtJob plain;
    const char *pdir = avp_path_join(a, outdir, "plain");
    if (code_check) avp_makedirs(pdir, true);
    AvpStr plog;
    double psecs = 0;
    i64 pcode = -1;
    bool have_plain = code_check && mwt_job_init(a, &plain, unit, src, adds, drops, pdir, &why) &&
                      mwt_job_run(a, &plain, AVP_S("# no breakpoints: the compile without -sym on\n"), "plain.wdbg", &plog,
                                  &psecs, &pcode, &why) &&
                      pcode == 0;
    CcRepo cr = {r->job.repo->root, r->job.repo->version};
    const char *dtk = cc_repo_dtk(a, &cr);
    AvpStr err = {0};
    AvpMap *om = cc_load_object(a, avp_path_join(a, outdir, "o.o"), dtk, &err);
    AvpMap *pm = have_plain ? cc_load_object(a, avp_path_join(a, pdir, "o.o"), dtk, &err) : NULL;
    const CcDwFns *fns = NULL;
    AvpStr dtext;
    const char *dwhy = NULL;
    if (obj_fn(om, link_in(om, &r->trace, r->fn, sym)) &&
        cc_dwdump_run(a, dtk, avp_path_join(a, outdir, "o.o"), avp_path_join(a, outdir, "dwarf.txt"), r->job.repo->root,
                      &dtext, &dwhy)) {
        CcDwFns *f = AVP_NEW(a, CcDwFns);
        *f = cc_dtk_dump_parse(a, dtext);
        fns = f;
    }
    fn_checks(a, r, om, pm, code_check, have_plain, why, fns, sym);
    r->total_secs = avp_perf_counter() - t_all;
    return true;
}

bool mwt_ra_run(AvpArena *a, const char *unit, const char *src, const AvpStrVec *adds, const AvpStrVec *drops,
                const char *outdir, const char *sym, bool code_check, MwtRaRun *r) {
    AvpStrVec none = {0};
    return trace_one(a, unit, src, adds ? adds : &none, drops, outdir, sym, code_check, r);
}

bool mwt_ra_unit_run(AvpArena *a, const char *unit, const char *src, const AvpStrVec *adds, const AvpStrVec *drops,
                     const char *outdir, MwtRaUnit *u, const char **why) {
    memset(u, 0, sizeof *u);
    double t0 = avp_perf_counter();
    if (!ra_compile(a, unit, src, adds, drops, outdir, AVP_S(""), &u->job, &u->trace, &u->secs, why)) return false;
    CcRepo cr = {u->job.repo->root, u->job.repo->version};
    const char *dtk = cc_repo_dtk(a, &cr);
    AvpStr err = {0};
    u->om = cc_load_object(a, avp_path_join(a, outdir, "o.o"), dtk, &err);
    AvpStr dtext;
    const char *dwhy = NULL;
    if (u->om && cc_dwdump_run(a, dtk, avp_path_join(a, outdir, "o.o"), avp_path_join(a, outdir, "dwarf.txt"),
                               u->job.repo->root, &dtext, &dwhy)) {
        u->fns = cc_dtk_dump_parse(a, dtext);
        u->have_fns = true;
    }
    u->total_secs = avp_perf_counter() - t0;
    return true;
}

bool mwt_ra_unit_fn(AvpArena *a, const MwtRaUnit *u, const char *sym, MwtRaRun *r) {
    memset(r, 0, sizeof *r);
    r->job = u->job;
    r->trace = u->trace;
    r->secs = u->secs;
    const char *why = NULL;
    r->fn = mwt_ra_find(a, &r->trace, sym, &why);
    if (!r->fn) {
        r->error = why;
        return false;
    }
    fn_checks(a, r, u->om, NULL, false, false, NULL, u->have_fns ? &u->fns : NULL, sym);
    return true;
}
static const char *usage(void) {
    return "usage: mwtrace.exe regalloc <unit> --function SYM [--src FILE] [--against FILE2] [--flag \"<words>\"]...\n"
           "                                [--drop-flag W]... [--json] [--out DIR]\n"
           "\n"
           "Compiles the unit (or --src FILE with the unit's flags, -sym on added) under the debugger and prints the\n"
           "register allocation of one function, per class and colouring attempt: the nodes in select order (virtual\n"
           "register, variable or temp with its first definition, degree, spill cost, simplify position and how it was\n"
           "pushed, the registers its neighbours left free, the nonvolatile register added, the register), the nodes\n"
           "coalesced away, the simplify order, the optimistic choices, the coalescing merges and refusals. Self-check:\n"
           "every register variable against the -sym on object's DWARF, the nonvolatile registers against the ones\n"
           "the code saves, the -sym on code against a compile without it; a disagreement is TRACE-GAP (exit 1).\n"
           "--against FILE2 traces the function in a second text and prints the first simplify and select position\n"
           "whose node differs, and every named variable whose register differs. Output: <out>/regalloc.txt,\n"
           "regalloc.json (default out: build/mwtrace/<unit path>/regalloc/<SYM>; with --against, <out>/a and <out>/b).";
}

int mwt_regalloc_main(AvpArena *a, int argc, char **argv) {
    const char *unit = NULL, *sym = NULL, *src = NULL, *against = NULL, *out = NULL;
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
                    !strcmp(s, "--against") || !strcmp(s, "--out")) &&
                   i + 1 < argc) {
            const char *v = argv[++i];
            if (!strcmp(s, "--function")) sym = v;
            else if (!strcmp(s, "--flag")) avp_vec_push(&adds, avp_str(v));
            else if (!strcmp(s, "--drop-flag")) avp_vec_push(&drops, avp_str(v));
            else if (!strcmp(s, "--src")) src = v;
            else if (!strcmp(s, "--against")) against = v;
            else out = v;
        } else if (s[0] == '-' || unit) {
            fprintf(stderr, "mwtrace.exe regalloc: unexpected argument %s\n%s\n", s, usage());
            return 2;
        } else {
            unit = s;
        }
    }
    if (!unit || !sym) {
        fprintf(stderr, "%s\n", usage());
        return 2;
    }
    const ShsmRepo *repo = shsm_repo_find(NULL);
    const char *path = strncmp(unit, "main/", 5) == 0 ? unit + 5 : unit;
    const char *outdir = out ? avp_path_abspath(a, out) : avp_join(a, repo->root, "build", "mwtrace", path, "regalloc", sym);
    const char *build_dir = avp_path_abspath(a, avp_path_join(a, repo->root, "build"));
    if (strncmp(avp_path_normcase(a, outdir), avp_path_normcase(a, build_dir), strlen(build_dir)) != 0) {
        fprintf(stderr, "mwtrace.exe regalloc: --out must be under %s\n", build_dir);
        return 2;
    }
    if (against && src && !strcmp(avp_path_abspath(a, src), avp_path_abspath(a, against)))
        fprintf(stderr, "mwtrace.exe regalloc: --src and --against name the same file\n");
    RaRun A, B;
    const char *dir_a = against ? avp_path_join(a, outdir, "a") : outdir;
    avp_makedirs(dir_a, true);
    if (!trace_one(a, unit, src, &adds, &drops, dir_a, sym, true, &A)) {
        fprintf(stderr, "mwtrace.exe regalloc: %s\n", A.error);
        return 2;
    }
    AvpStr text = mwt_ra_text(a, &A.trace, A.fn, &A.check);
    AvpJson *js = mwt_ra_json(a, &A.trace, A.fn, &A.check);
    AvpBuf tb;
    avp_buf_init(&tb, a);
    avp_buf_printf(&tb, "mwtrace regalloc %s %s: %.2f s%s\n  source: %s\n", A.job.unit->name, sym, A.secs,
                   against ? " (A)" : "", A.job.source);
    avp_buf_put(&tb, text);
    set(a, js, "unit", avp_json_strz(a, A.job.unit->name));
    set(a, js, "function", avp_json_strz(a, sym));
    set(a, js, "source", avp_json_strz(a, A.job.source));
    set(a, js, "seconds", avp_json_float(a, avp_clock_round(A.secs, 2)));
    bool gaps = A.check.gaps.n > 0;
    if (against) {
        const char *dir_b = avp_path_join(a, outdir, "b");
        avp_makedirs(dir_b, true);
        if (!trace_one(a, unit, against, &adds, &drops, dir_b, sym, true, &B)) {
            fprintf(stderr, "mwtrace.exe regalloc: %s (--against)\n", B.error);
            return 2;
        }
        gaps = gaps || B.check.gaps.n > 0;
        avp_buf_printf(&tb, "\nB: %s: %.2f s\n", B.job.source, B.secs);
        avp_buf_put(&tb, mwt_ra_text(a, &B.trace, B.fn, &B.check));
        AvpStrVec d = mwt_ra_diff(a, A.fn, B.fn);
        avp_buf_putz(&tb, "\ndifferences (A: the first source, B: --against):\n");
        for (size_t i = 0; i < d.n; i++) avp_buf_printf(&tb, "  %.*s\n", AVP_SF(d.v[i]));
        AvpJson *root = avp_json_obj(a);
        AvpJson *jb = mwt_ra_json(a, &B.trace, B.fn, &B.check);
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
    avp_write_file(avp_path_join(a, outdir, "regalloc.txt"), all.p, all.n);
    avp_write_file(avp_path_join(a, outdir, "regalloc.json"), jtext.p, jtext.n);
    if (json) printf("%.*s\n", AVP_SF(jtext));
    else fwrite(all.p, 1, all.n, stdout);
    return gaps ? 1 : 0;
}
