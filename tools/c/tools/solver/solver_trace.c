/* solver_trace: the records' conditions in the compiler's own numbers, read from mwtrace's traces (linked as a
 * library: mwt_inline.h, mwt_regalloc.h).
 *   - inline and flag records: our compile of the unit is traced once per flag change (cached in the unit's work);
 *     the record gets the differing site's decision (rule, cost, limit, candidate reason, statements) and the
 *     condition retail's opposite decision implies in those numbers. A self-check gap of the trace for the record's
 *     caller and callee decides what the stop is: the inliner's calls equal retail's and a later stage removes them
 *     (not an inline decision: its own cause), or the trace disagrees with the object (a model gap, the stage named);
 *   - register records: the target is traced once (the unmodified scratch text); its self-check replaces the
 *     declaration-order prediction as the model check; the first differing named variable and the node that holds
 *     retail's register in ours are stated with their sweep, positions, degree and spill cost, and the condition is
 *     the allocator's model (select colours later sweeps first, the higher index first within one sweep); a probe or
 *     the best candidate is traced against ours (mwt_ra_diff) and its named variables that moved are printed;
 *   - the same-sweep filter of unevidenced declaration swaps.
 * The numbers only state conditions: no candidate is generated from them. Every trace compiles a copy under
 * build/tmp/solver/<unit>/trace/<n>; the time is summed for the run's time: line. */
#include "solver.h"
#include "wall.h"

#include <stdlib.h>

/* ---- small text helpers ---------------------------------------------------------------------------------------- */

static const char *ord(AvpArena *a, int i) {
    int n = i + 1, t = n % 100;
    const char *suf = (t >= 11 && t <= 13) ? "th" : n % 10 == 1 ? "st" : n % 10 == 2 ? "nd" : n % 10 == 3 ? "rd" : "th";
    return avp_sprintf(a, "%d%s", n, suf);
}

static const char *reg_name(AvpArena *a, int cls, int reg) {
    if (reg < 0) return "no register";
    return avp_sprintf(a, "%c%d", cls == MWT_RC_FPR ? 'f' : cls == MWT_RC_CR ? 'c' : 'r', reg);
}

static bool nonvolatile(int cls, int reg) {
    if (cls == MWT_RC_GPR || cls == MWT_RC_FPR) return reg >= 14 && reg <= 31;
    return false;
}

static const char *kind_word(char k) {
    return k == 'p' ? "parameter" : k == 'l' ? "local" : k == 'o' ? "object" : "temporary";
}

/* A variable of this function's own source (not a renamed @NNN object). */
static bool source_named(const SvRaNode *n) { return n->name && n->name[0] != '@'; }

static const char *var_label(AvpArena *a, const SvRaNode *n) {
    if (n->name && n->name[0] == '@' && n->orig)
        return avp_sprintf(a, "`%s` of the inlined %s (%s)", n->orig, n->inl_from ? n->inl_from : "callee", n->name);
    if (n->name && n->name[0] == '@') return avp_sprintf(a, "compiler-made object %s", n->name);
    if (n->name) return avp_sprintf(a, "`%s`", n->name);
    return avp_sprintf(a, "temporary node %d", n->index);
}

static const char *node_kind(const SvRaNode *n) {
    if (n->name && n->name[0] == '@') return n->orig ? (n->inl_param ? "inlined parameter" : "inlined local") : "compiler-made";
    return kind_word(n->kind);
}

/* ---- inline: pure ---------------------------------------------------------------------------------------------- */

static bool starts(const char *s, const char *p) { return s && !strncmp(s, p, strlen(p)); }

/* A call the inliner refused as no candidate (in a visited caller, or in one the visitor never inlined). */
static bool refused(const char *rule) { return !strcmp(rule, "not-candidate") || !strcmp(rule, "not-visited"); }

static const char *rule_phrase(AvpArena *a, const SvInlSite *s) {
    const char *r = s->rule ? s->rule : "";
    if (!strcmp(r, "level0")) return "level 0 has no size limit";
    if (starts(r, "level") && strstr(r, "-cost")) {
        if (s->cost >= 0 && s->limit >= 0)
            return avp_sprintf(a, "level %d: cost %lld, limit %lld", s->level >= 0 ? s->level : atoi(r + 5),
                               (long long)s->cost, (long long)s->limit);
        return avp_sprintf(a, "%s (cost not read)", r);
    }
    if (starts(r, "depth")) return "inline depth: no cost limit";
    if (!strcmp(r, "bottom-up-size"))
        return s->cost >= 0 ? avp_sprintf(a, "bottom-up: size %lld, limit %lld", (long long)s->cost, (long long)s->limit)
                            : "bottom-up: size not read";
    if (!strcmp(r, "bottom-up-last")) return "the last bottom-up level takes it without a size test";
    if (!strcmp(r, "always-inline")) return "always-inline";
    return avp_sprintf(a, "rule %s", r);
}

static const char *statements_text(AvpArena *a, const SvInlSite *s) {
    if (s->statements >= 0 && s->max_statements >= 0)
        return avp_sprintf(a, "%lld statements, limit %lld", (long long)s->statements, (long long)s->max_statements);
    if (s->statements >= 0) return avp_sprintf(a, "%lld statements, limit not read", (long long)s->statements);
    return "statements not read";
}

const char *sv_inl_site_text(AvpArena *a, const SvInlSite *s) {
    const char *c = s->callee ? s->callee : "?";
    const char *reason = s->reason ? s->reason : "";
    if (s->decision && !strcmp(s->decision, "expanded")) {
        if (!strcmp(reason, "inline keyword"))
            return avp_sprintf(a, "expands %s (candidate: inline keyword; %s)", c, rule_phrase(a, s));
        return avp_sprintf(a, "expands %s (auto-inline candidate: %s; %s)", c, statements_text(a, s), rule_phrase(a, s));
    }
    const char *r = s->rule ? s->rule : "";
    if (refused(r)) {
        if (!strcmp(reason, "auto-statements"))
            return avp_sprintf(a, "calls %s (not a candidate: %s)", c, statements_text(a, s));
        if (!strcmp(reason, "noauto"))
            return avp_sprintf(a, "calls %s (not a candidate: no inline keyword and auto-inline is off)", c);
        if (!strcmp(reason, "not-yet-defined"))
            return avp_sprintf(a, "calls %s (not a candidate yet: its definition comes later in the compile)", c);
        if (!strcmp(reason, "no-body")) return avp_sprintf(a, "calls %s (not a candidate: no body in this unit)", c);
        if (!strcmp(reason, "auto-locals"))
            return avp_sprintf(a, "calls %s (not a candidate: locals of %lld bytes, limit %lld)", c, (long long)s->cost,
                               (long long)s->limit);
        if (!strcmp(r, "not-visited") && !(s->detail && strchr(s->detail, ':')))
            return avp_sprintf(a, "calls %s (its caller was not inlined into: %s)", c, s->detail ? s->detail : "?");
        return avp_sprintf(a, "calls %s (not a candidate: %s)", c, s->detail && *s->detail ? s->detail : reason);
    }
    if (starts(r, "level") && strstr(r, "-cost"))
        return avp_sprintf(a, "calls %s (candidate refused by the cost test: %s)", c, rule_phrase(a, s));
    if (!strcmp(r, "bottom-up-size")) return avp_sprintf(a, "calls %s (refused: %s)", c, rule_phrase(a, s));
    if (!strcmp(r, "queue-full"))
        return avp_sprintf(a, "calls %s (the statement-inline queue holds %lld calls, limit %lld)", c, (long long)s->cost,
                           (long long)s->limit);
    if (!strcmp(r, "kind0"))
        return avp_sprintf(a, "calls %s (candidate, but %s)", c, s->detail && *s->detail ? s->detail : "the body is not inlinable");
    return avp_sprintf(a, "calls %s (rule %s%s%s)", c, *r ? r : "unknown", s->detail && *s->detail ? ": " : "",
                       s->detail ? s->detail : "");
}

const char *sv_inl_condition(AvpArena *a, const SvInlSite *s, bool retail_calls) {
    const char *c = s->callee ? s->callee : "?";
    const char *reason = s->reason ? s->reason : "";
    const char *r = s->rule ? s->rule : "";
    bool expanded = s->decision && !strcmp(s->decision, "expanded");
    if (retail_calls) {
        if (!expanded) return avp_sprintf(a, "retail calls %s and so does the trace: the inliner's decision agrees here", c);
        const char *cost = "";
        if (starts(r, "level") && strstr(r, "-cost") && s->limit >= 0)
            cost = avp_sprintf(a, ", or its cost at level %d must exceed %lld (ours %lld)", s->level, (long long)s->limit,
                               (long long)s->cost);
        if (!strcmp(reason, "inline keyword"))
            return avp_sprintf(a, "retail calls it: %s must not carry the inline keyword, or must not be defined before "
                                  "this call%s",
                               c, cost);
        if (s->max_statements >= 0)
            return avp_sprintf(a, "retail calls it: %s must not be a candidate (more than %lld statements, or no "
                                  "auto-inline) or must not be defined before this call%s",
                               c, (long long)s->max_statements, cost);
        return avp_sprintf(a, "retail calls it: %s must not be a candidate (the statement limit was not read) or must "
                              "not be defined before this call%s",
                           c, cost);
    }
    if (expanded) return avp_sprintf(a, "retail expands %s and so does the trace: the inliner's decision agrees here", c);
    if (refused(r)) {
        if (!strcmp(reason, "auto-statements"))
            return s->max_statements >= 0
                       ? avp_sprintf(a, "retail expands it: %s must have at most %lld statements (ours %lld)", c,
                                     (long long)s->max_statements, (long long)s->statements)
                       : avp_sprintf(a, "retail expands it: %s must have fewer statements (the limit was not read)", c);
        if (!strcmp(reason, "noauto"))
            return avp_sprintf(a, "retail expands it: %s must carry the inline keyword, or auto-inline must be on where "
                                  "it is defined",
                               c);
        if (!strcmp(reason, "not-yet-defined"))
            return avp_sprintf(a, "retail expands it: %s must be defined before this call", c);
        if (!strcmp(reason, "no-body"))
            return avp_sprintf(a, "retail expands it: %s's body must be visible to this unit before this call", c);
        if (!strcmp(reason, "auto-locals"))
            return avp_sprintf(a, "retail expands it: %s's locals must take at most %lld bytes (ours %lld)", c,
                               (long long)s->limit, (long long)s->cost);
        return avp_sprintf(a, "retail expands it: the trace gives no number for %s's refusal (%s)", c,
                           s->detail && *s->detail ? s->detail : reason);
    }
    if (starts(r, "level") && strstr(r, "-cost"))
        return s->limit >= 0 ? avp_sprintf(a, "retail expands it: %s's cost at level %d must be at most %lld (ours %lld)", c,
                                           s->level >= 0 ? s->level : atoi(r + 5), (long long)s->limit, (long long)s->cost)
                             : avp_sprintf(a, "retail expands it: %s's cost must pass the level test (not read)", c);
    if (!strcmp(r, "bottom-up-size") && s->limit >= 0)
        return avp_sprintf(a, "retail expands it: %s's estimated size must be at most %lld (ours %lld)", c,
                           (long long)s->limit, (long long)s->cost);
    if (!strcmp(r, "queue-full"))
        return avp_sprintf(a, "retail expands it: the statement-inline queue must hold fewer than %lld calls here (ours "
                              "%lld)",
                           (long long)s->limit, (long long)s->cost);
    if (!strcmp(r, "kind0"))
        return avp_sprintf(a, "retail expands it: %s's body must be inlinable (ours: %s)", c,
                           s->detail && *s->detail ? s->detail : "kind 0");
    return avp_sprintf(a, "retail expands it: the trace gives no number for rule %s", *r ? r : "unknown");
}

const char *sv_inl_rule_label(AvpArena *a, const SvInlSite *s) {
    const char *reason = s->reason ? s->reason : "";
    const char *r = s->rule ? s->rule : "";
    if (s->decision && !strcmp(s->decision, "expanded")) {
        if (!strcmp(reason, "inline keyword")) return "inline keyword";
        if (s->max_statements >= 0)
            return avp_sprintf(a, "auto-inline: under %lld statements", (long long)(s->max_statements + 1));
        return "auto-inline";
    }
    if (refused(r)) {
        if (!strcmp(reason, "auto-statements"))
            return s->max_statements >= 0 ? avp_sprintf(a, "auto-inline: over %lld statements", (long long)s->max_statements)
                                          : "auto-inline: too many statements";
        if (!strcmp(reason, "noauto")) return "no auto-inline";
        if (!strcmp(reason, "not-yet-defined")) return "definition order";
        if (!strcmp(reason, "no-body")) return "no body in this unit";
        return avp_sprintf(a, "not a candidate: %s", *reason ? reason : "unknown");
    }
    if (starts(r, "level") && strstr(r, "-cost") && s->limit >= 0)
        return avp_sprintf(a, "level %d cost limit %lld", s->level >= 0 ? s->level : atoi(r + 5), (long long)s->limit);
    return *r ? r : "unknown rule";
}

int sv_inl_gap_kind(const SvInlGap *g) {
    const char *st = g->stage ? g->stage : "";
    if (!strcmp(st, "after-inliner") || !strcmp(st, "simple-optimizer"))
        return g->trace_calls >= 0 && g->trace_calls == g->retail_calls ? 1 : 2;
    return 0;
}

static const char *count_or(AvpArena *a, i64 v) { return v < 0 ? "not known" : avp_sprintf(a, "%lld", (long long)v); }

const char *sv_inl_gap_counts(AvpArena *a, const SvInlGap *g) {
    return avp_sprintf(a, "trace %s, post-inline IR %s, object %s, retail %s", count_or(a, g->trace_calls),
                       count_or(a, g->ir_calls), count_or(a, g->object_calls), count_or(a, g->retail_calls));
}

/* ---- regalloc: pure -------------------------------------------------------------------------------------------- */

const char *sv_ra_node_text(AvpArena *a, int cls, const SvRaNode *n) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_printf(&b, "%s (node %d, %s", var_label(a, n), n->index, node_kind(n));
    if (!n->name && n->def_op) avp_buf_printf(&b, ", first defined by `%s` at instruction %lld", n->def_op, (long long)n->def_at);
    else if (!n->name) avp_buf_putz(&b, ", no defining instruction read");
    if (n->root != n->index) avp_buf_printf(&b, ", coalesced into node %d", n->root);
    avp_buf_putz(&b, "): ");
    if (n->sweep > 0)
        avp_buf_printf(&b, "sweep %d, pushed %s (%s), ", n->sweep, ord(a, n->push_pos), n->push_how && *n->push_how ? n->push_how : "?");
    else
        avp_buf_putz(&b, "not pushed, ");
    if (n->select_pos >= 0) avp_buf_printf(&b, "selected %s, ", ord(a, n->select_pos));
    avp_buf_printf(&b, "degree %d", n->degree);
    if (n->push_degree >= 0 && n->push_degree != n->degree) avp_buf_printf(&b, " (%d when pushed)", n->push_degree);
    if (n->cost >= 0) avp_buf_printf(&b, ", spill cost %lld", (long long)n->cost);
    avp_buf_printf(&b, "; %s in ours", reg_name(a, cls, n->reg));
    if (n->retail_reg >= 0) avp_buf_printf(&b, ", %s in retail", reg_name(a, cls, n->retail_reg));
    return b.p;
}

const char *sv_ra_condition(AvpArena *a, const SvRaFacts *f, const char **shape) {
    const SvRaNode *v = &f->var, *h = &f->holder;
    int cls = f->cls;
    const char *rr = reg_name(a, cls, v->retail_reg), *ro = reg_name(a, cls, v->reg);
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_printf(&b, "retail has %s in %s, ours in %s", var_label(a, v), rr, ro);
    if (!f->have_holder) {
        *shape = "no holder";
        avp_buf_printf(&b, "; no node of ours has %s (%s)", rr, sv_ra_node_text(a, cls, v));
        return b.p;
    }
    bool nv_r = nonvolatile(cls, v->retail_reg), nv_o = nonvolatile(cls, v->reg);
    if (nv_r != nv_o) {
        *shape = v->sweep == h->sweep ? "one sweep" : "across sweeps";
        avp_buf_printf(&b, ": retail gives it a %s register where ours gives a %s one, so %s", nv_r ? "nonvolatile" : "volatile",
                       nv_o ? "nonvolatile" : "volatile",
                       nv_r ? "no volatile register may be free to it when it is coloured (select hands out a nonvolatile "
                              "register only then)"
                            : "a volatile register must be free to it when it is coloured");
        avp_buf_printf(&b, "; %s; %s in ours: %s", sv_ra_node_text(a, cls, v), rr, sv_ra_node_text(a, cls, h));
        return b.p;
    }
    /* nonvolatile registers are handed out r31 first; the picker takes the lowest free volatile register */
    bool v_first = nv_r ? v->retail_reg > v->reg : v->retail_reg < v->reg;
    avp_buf_printf(&b, ": %s must be coloured %s %s (%s)", var_label(a, v), v_first ? "before" : "after", var_label(a, h),
                   nv_r ? "nonvolatile registers are handed out from r31 down" : "the picker takes the lowest free register");
    bool ours_v_first = v->select_pos >= 0 && h->select_pos >= 0 && v->select_pos < h->select_pos;
    const SvRaNode *first = ours_v_first ? v : h, *second = ours_v_first ? h : v;
    if (v->select_pos < 0 || h->select_pos < 0) {
        *shape = "order holds";
        avp_buf_putz(&b, "; the trace gives no select position for one of them");
    } else if (ours_v_first == v_first) {
        *shape = "order holds";
        avp_buf_printf(&b, "; in ours %s is already coloured %s (selected %s against %s): the order alone does not explain "
                           "it, the registers its neighbours leave free do",
                       var_label(a, v), v_first ? "first" : "second", ord(a, v->select_pos), ord(a, h->select_pos));
    } else if (v->sweep == h->sweep && v->sweep > 0) {
        *shape = "one sweep";
        avp_buf_printf(&b, "; in ours both are pushed in sweep %d, %s has the higher index (%d against %d) and is coloured "
                           "first",
                       v->sweep, var_label(a, first), first->index, second->index);
        const SvRaNode *odd = !source_named(v) || v->kind != 'l' ? v : h;
        if (v->kind == 'l' && h->kind == 'l' && source_named(v) && source_named(h))
            avp_buf_printf(&b, "; within one sweep the higher index is coloured first and locals are numbered in reverse "
                               "declaration order: declaring %s before %s gives %s the higher index",
                           var_label(a, second), var_label(a, first), var_label(a, second));
        else
            avp_buf_printf(&b, "; %s is not one of the function's declared locals (%s): its index does not come from their "
                               "declaration order%s",
                           var_label(a, odd), node_kind(odd),
                           odd->name && odd->name[0] == '@' && odd->orig ? " (it is numbered when the callee is expanded)" : "");
    } else {
        *shape = "across sweeps";
        avp_buf_printf(&b, "; %s is pushed in sweep %d (%s; degree %d when pushed, K %d) and is coloured first, %s in sweep "
                           "%d: select colours the later sweep first, the declaration order of the locals cannot move it",
                       var_label(a, first), first->sweep, first->push_how && *first->push_how ? first->push_how : "?",
                       first->push_degree, f->K, var_label(a, second), second->sweep);
    }
    avp_buf_printf(&b, " [%s; %s in ours: %s]", sv_ra_node_text(a, cls, v), rr, sv_ra_node_text(a, cls, h));
    return b.p;
}

const char *sv_ra_temps_condition(AvpArena *a, int cls, int ours_reg, int retail_reg, const SvRaNode *x, size_t nx,
                                  const SvRaNode *y, size_t ny) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_printf(&b, "the first difference is between temporaries only: ours has %s where retail has %s",
                   reg_name(a, cls, ours_reg), reg_name(a, cls, retail_reg));
    if (x) avp_buf_printf(&b, "; %s in ours: %s%s", reg_name(a, cls, ours_reg), sv_ra_node_text(a, cls, x),
                          nx > 1 ? avp_sprintf(a, " (first of %zu nodes in it, by select order)", nx) : "");
    else avp_buf_printf(&b, "; no node of ours is coloured %s", reg_name(a, cls, ours_reg));
    if (y) avp_buf_printf(&b, "; %s in ours: %s%s", reg_name(a, cls, retail_reg), sv_ra_node_text(a, cls, y),
                          ny > 1 ? avp_sprintf(a, " (first of %zu nodes in it, by select order)", ny) : "");
    else avp_buf_printf(&b, "; no node of ours is coloured %s", reg_name(a, cls, retail_reg));
    avp_buf_putz(&b, "; no named variable differs at this row");
    return b.p;
}

AvpCzVec sv_inverted_pairs(AvpArena *a, const char *const *from, size_t nfrom, const char *const *to, size_t nto) {
    AvpCzVec out = {0};
    for (size_t i = 0; i < nfrom; i++)
        for (size_t j = i + 1; j < nfrom; j++) {
            i64 pi = -1, pj = -1;
            for (size_t k = 0; k < nto; k++) {
                if (pi < 0 && !strcmp(to[k], from[i])) pi = (i64)k;
                if (pj < 0 && !strcmp(to[k], from[j])) pj = (i64)k;
            }
            if (pi < 0 || pj < 0 || !strcmp(from[i], from[j])) continue;
            if (pj < pi) avp_vec_push(&out, avp_sprintf(a, "%s\x1f%s", from[i], from[j]));
        }
    return out;
}

static const MwtRaNode *pass_node(const MwtRaPass *p, int idx) {
    if (idx < p->nreal) return NULL;
    size_t k = (size_t)(idx - p->nreal);
    return k < p->nodes.n ? &p->nodes.v[k] : NULL;
}

static const MwtRaNode *named_node(const MwtRaPass *p, AvpStr n) {
    for (size_t i = 0; i < p->nodes.n; i++) {
        const MwtRaNode *x = &p->nodes.v[i];
        if ((x->kind == 'p' || x->kind == 'l') && !x->inherited && avp_str_eq(x->name, n)) return x;
    }
    for (size_t i = 0; i < p->nodes.n; i++) {
        const MwtRaNode *x = &p->nodes.v[i];
        if ((x->kind == 'p' || x->kind == 'l') && avp_str_eq(x->name, n)) return x;
    }
    return NULL;
}

static int sweep_of(const MwtRaPass *p, const MwtRaNode *n) {
    const MwtRaNode *r = n->alias != n->index ? pass_node(p, n->alias) : n;
    return r ? r->push_sweep : -1;
}

int sv_swap_same_sweep(const MwtRaPass *p, const AvpCzVec *pairs) {
    bool any = false;
    for (size_t i = 0; i < pairs->n; i++) {
        const char *sep = strchr(pairs->v[i], '\x1f');
        if (!sep) continue;
        AvpStr x = avp_strn(pairs->v[i], (size_t)(sep - pairs->v[i])), y = avp_str(sep + 1);
        const MwtRaNode *nx = named_node(p, x), *ny = named_node(p, y);
        if (!nx || !ny) continue;
        int sx = sweep_of(p, nx), sy = sweep_of(p, ny);
        if (sx <= 0 || sy <= 0) continue;
        any = true;
        if (sx == sy) return 1;
    }
    return any ? 0 : -1;
}

SvRaNode sv_ra_node_of(AvpArena *a, const MwtRaPass *p, const MwtRaNode *n) {
    SvRaNode o = {0};
    const MwtRaNode *r = n->alias != n->index ? pass_node(p, n->alias) : n;
    o.name = (n->kind == 'p' || n->kind == 'l') && n->name.n ? avp_cz(a, n->name) : NULL;
    o.kind = (char)n->kind;
    o.def_op = n->def_op.n ? avp_cz(a, n->def_op) : NULL;
    o.orig = n->orig.n ? avp_cz(a, n->orig) : NULL;
    o.inl_from = n->inl_from.n ? avp_cz(a, n->inl_from) : NULL;
    o.inl_param = n->inl_param;
    o.def_at = n->def_at;
    o.index = n->index;
    o.root = n->alias;
    const MwtRaNode *src = r ? r : n;
    o.sweep = r ? r->push_sweep : 0;
    o.push_pos = r ? r->push_pos : -1;
    o.select_pos = r ? r->select_pos : -1;
    o.degree = src->degree;
    o.push_degree = r ? r->push_degree : -1;
    o.push_how = !r ? "" : r->push_how == MWT_PUSH_LOW ? "low" : r->push_how == MWT_PUSH_OPTIMISTIC ? "optimistic"
                        : r->push_how == MWT_PUSH_AFTER ? "after an optimistic push" : "";
    o.cost = src->cost;
    o.reg = n->final_reg;
    o.retail_reg = -1;
    return o;
}

/* ---- the traced compiles (cached in the unit's work) ----------------------------------------------------------- */

typedef struct {
    MwtInlineRun run;
    bool ok;
    const char *error;
} SvInlTraced;

typedef struct {
    MwtRaRun run;
    bool ok;
    const char *error;
} SvRaTraced;

static void jset(AvpArena *a, AvpJson *o, const char *k, AvpJson *v) { avp_json_set(a, o, avp_str(k), v); }
static AvpJson *jz(AvpArena *a, const char *s) { return s ? avp_json_strz(a, s) : avp_json_null(); }
static AvpJson *ji(AvpArena *a, i64 v) { return v < 0 ? avp_json_null() : avp_json_int(a, v); }

static const char *trace_dir(AvpArena *a, SvUnitWork *w) {
    return avp_path_join(a, avp_path_join(a, w->sc.workdir, "trace"), avp_sprintf(a, "%d", ++w->trace_counter));
}

/* The file our traces compile: the unmodified scratch copy (the stop's text, a --case edit included). */
static const char *our_source(const SvUnitWork *w) { return w->base && w->base->ok ? w->base->src : NULL; }

static const SvInlTraced *inl_trace(SvRun *run, SvUnitWork *w, const char *key, const AvpStrVec *adds,
                                    const AvpStrVec *drops) {
    AvpArena *a = run->a;
    if (!w->inl_traces) w->inl_traces = avp_map_new();
    bool created = false;
    AvpVal *v = avp_map_put(w->inl_traces, avp_str(key), &created);
    if (!created) return v->p;
    SvInlTraced *t = AVP_NEW(a, SvInlTraced);
    v->p = t;
    const char *src = our_source(w);
    if (!src) {
        t->error = "the unmodified scratch copy did not compile";
        run->traces_failed++;
        return t;
    }
    double t0 = avp_time();
    const char *why = NULL;
    t->ok = mwt_inline_run(a, w->u->name, src, adds, drops, trace_dir(a, w), &t->run, &why);
    run->trace_seconds += avp_time() - t0;
    run->trace_kind_seconds[SV_TRACE_INLINE] += avp_time() - t0;
    run->ntraces++;
    if (!t->ok) {
        t->error = why ? why : "?";
        run->traces_failed++;
    }
    return t;
}

/* The regalloc trace of fn in text (text.p NULL: our unmodified text), cached per function and text. */
static const SvRaTraced *ra_trace(SvRun *run, SvUnitWork *w, const char *fn, LvS text) {
    AvpArena *a = run->a;
    if (!w->ra_traces) w->ra_traces = avp_map_new();
    const char *sha = text.p ? avp_sha1_hex(a, text.p, text.n) : "";
    bool created = false;
    AvpVal *v = avp_map_put(w->ra_traces, avp_str(avp_sprintf(a, "%s\x1f%s", fn, sha)), &created);
    if (!created) return v->p;
    SvRaTraced *t = AVP_NEW(a, SvRaTraced);
    v->p = t;
    const char *dir = trace_dir(a, w), *src = our_source(w);
    if (!src) {
        t->error = "the unmodified scratch copy did not compile";
        run->traces_failed++;
        return t;
    }
    if (!text.p) {
        /* our text: every function of the unit in one compile (the debugger's cost is the unit's), then this one */
        if (!w->ra_unit && !w->ra_unit_error) {
            MwtRaUnit *u = AVP_NEW(a, MwtRaUnit);
            const char *why = NULL;
            double t0 = avp_time();
            if (mwt_ra_unit_run(a, w->u->name, src, NULL, NULL, avp_path_join(a, dir, "ra"), u, &why)) w->ra_unit = u;
            else w->ra_unit_error = why ? why : "?";
            run->trace_seconds += avp_time() - t0;
            run->trace_kind_seconds[SV_TRACE_REGALLOC] += avp_time() - t0;
            run->ntraces++;
            if (!w->ra_unit) run->traces_failed++;
        }
        if (!w->ra_unit) {
            t->error = w->ra_unit_error;
            return t;
        }
        t->ok = mwt_ra_unit_fn(a, w->ra_unit, fn, &t->run);
        if (!t->ok) t->error = t->run.error ? t->run.error : "?";
        return t;
    }
    if (text.p) {
        const char *sdir = avp_path_join(a, dir, "src");
        avp_makedirs(sdir, true);
        src = avp_path_join(a, sdir, w->sc.basename);
        if (!avp_write_file(src, text.p, text.n)) {
            t->error = avp_sprintf(a, "cannot write %s", src);
            run->traces_failed++;
            return t;
        }
    }
    double t0 = avp_time();
    /* no second compile without -sym on: the solver's control 4 checks that the -sym on code is the baseline's */
    t->ok = mwt_ra_run(a, w->u->name, src, NULL, NULL, avp_path_join(a, dir, "ra"), fn, false, &t->run);
    run->trace_seconds += avp_time() - t0;
    run->trace_kind_seconds[SV_TRACE_REGALLOC] += avp_time() - t0;
    run->ntraces++;
    if (!t->ok) {
        t->error = t->run.error ? t->run.error : "?";
        run->traces_failed++;
    }
    return t;
}

/* ---- inline: reading a trace ----------------------------------------------------------------------------------- */

static AvpStr site_callee(const MwtTrace *t, const MwtSite *s) {
    AvpVal *v = avp_imap_get(t->links, s->callee);
    AvpStr l = v ? *(AvpStr *)v->p : AVP_S("");
    return l.n ? l : s->callee_name;
}

static SvInlSite site_of(AvpArena *a, const MwtTrace *t, const MwtVisit *vis, const MwtSite *s) {
    SvInlSite o = {0};
    o.callee = avp_cz(a, site_callee(t, s));
    o.decision = avp_cz(a, s->decision);
    o.rule = avp_cz(a, s->rule);
    o.detail = avp_cz(a, s->detail);
    o.cost = s->cost;
    o.limit = s->limit;
    o.level = s->expanded ? s->expanded_level : -1;
    if (!s->expanded && avp_str_startswith(s->rule, AVP_S("level"))) o.level = atoi(o.rule + 5);
    o.line = s->line > 0 ? s->line : -1;
    o.statements = o.max_statements = -1;
    AvpVal *dv = avp_imap_get(t->defs, s->callee);
    const MwtDef *d = dv ? dv->p : NULL;
    if (d && (d->auto_status == 1 || d->auto_status == 2)) {
        o.statements = d->auto_count;
        if (d->auto_status == 2) o.max_statements = d->auto_value;
        else if (vis) o.max_statements = vis->maxauto;
        else if (t->visits.n) o.max_statements = t->visits.v[0].maxauto;
    }
    if (s->expanded) {
        o.reason = (s->qual & 0x10) ? "inline keyword" : "auto-inline";
    } else if (avp_str_eqz(s->rule, "not-candidate") || avp_str_eqz(s->rule, "not-visited")) {
        AvpStr dt = s->detail;
        size_t k = 0;
        while (k < dt.n && dt.p[k] != ':') k++;
        o.reason = avp_cz(a, avp_strn(dt.p, k));
    } else {
        o.reason = o.rule;
    }
    return o;
}

/* The sites of callee in caller (caller NULL: in every caller), in the trace's order. */
static SvInlSites inl_sites(AvpArena *a, const MwtInlineRun *r, const char *caller, const char *callee) {
    SvInlSites out = {0};
    const MwtTrace *t = &r->trace;
    for (size_t i = 0; i < r->result.callers.n; i++) {
        const MwtCaller *c = &r->result.callers.v[i];
        if (caller && !avp_str_eqz(c->caller, caller)) continue;
        const MwtSites *ss = c->visited && c->visit ? &c->visit->sites : &c->synth;
        for (size_t k = 0; k < ss->n; k++) {
            const MwtSite *s = &ss->v[k];
            if (s->discarded) continue;
            if (callee && !avp_str_eqz(site_callee(t, s), callee)) continue;
            avp_vec_push(&out, site_of(a, t, c->visited ? c->visit : NULL, s));
        }
    }
    return out;
}

static bool inl_gap(AvpArena *a, const MwtInlineRun *r, const char *caller, const char *callee, SvInlGap *g) {
    for (size_t i = 0; i < r->result.gaps.n; i++) {
        const MwtGap *x = &r->result.gaps.v[i];
        if (!avp_str_eqz(x->caller, caller) || !avp_str_eqz(x->callee, callee)) continue;
        g->stage = avp_cz(a, x->stage);
        g->trace_calls = x->trace_calls;
        g->ir_calls = x->ir_calls;
        g->object_calls = x->object_calls;
        g->why = avp_cz(a, x->why);
        g->retail_calls = -1;
        return true;
    }
    return false;
}

static const SvInlSite *first_with(const SvInlSites *ss, const char *decision) {
    for (size_t i = 0; i < ss->n; i++)
        if (!strcmp(ss->v[i].decision, decision)) return &ss->v[i];
    return NULL;
}

static size_t count_with(const SvInlSites *ss, const char *decision) {
    size_t n = 0;
    for (size_t i = 0; i < ss->n; i++) n += !strcmp(ss->v[i].decision, decision);
    return n;
}

static AvpJson *site_json(AvpArena *a, const SvInlSite *s) {
    AvpJson *o = avp_json_obj(a);
    jset(a, o, "callee", jz(a, s->callee));
    jset(a, o, "decision", jz(a, s->decision));
    jset(a, o, "rule", jz(a, s->rule));
    jset(a, o, "detail", jz(a, s->detail));
    jset(a, o, "reason", jz(a, s->reason));
    jset(a, o, "cost", ji(a, s->cost));
    jset(a, o, "limit", ji(a, s->limit));
    jset(a, o, "statements", ji(a, s->statements));
    jset(a, o, "max_statements", ji(a, s->max_statements));
    jset(a, o, "level", ji(a, s->level));
    jset(a, o, "line", ji(a, s->line));
    jset(a, o, "text", jz(a, sv_inl_site_text(a, s)));
    return o;
}

static AvpJson *gaps_json(AvpArena *a, const MwtInlineRun *r) {
    AvpJson *l = avp_json_arr(a);
    for (size_t i = 0; i < r->result.gaps.n; i++) {
        const MwtGap *g = &r->result.gaps.v[i];
        AvpJson *o = avp_json_obj(a);
        jset(a, o, "caller", avp_json_str(a, g->caller));
        jset(a, o, "callee", avp_json_str(a, g->callee));
        jset(a, o, "stage", avp_json_str(a, g->stage));
        jset(a, o, "trace_calls", ji(a, g->trace_calls));
        jset(a, o, "ir_calls", ji(a, g->ir_calls));
        jset(a, o, "object_calls", ji(a, g->object_calls));
        avp_json_arr_push(a, l, o);
    }
    return l;
}

static AvpJson *inl_json(AvpArena *a, const char *kind, const char *compile, const SvInlTraced *t) {
    AvpJson *o = avp_json_obj(a);
    jset(a, o, "kind", avp_json_strz(a, kind));
    jset(a, o, "compile", avp_json_strz(a, compile));
    jset(a, o, "ok", avp_json_bool(t->ok));
    jset(a, o, "error", jz(a, t->error));
    if (t->ok) {
        jset(a, o, "seconds", avp_json_float(a, avp_py_round(t->run.secs, 2)));
        jset(a, o, "self_check", avp_json_strz(a, t->run.result.gaps.n ? "TRACE-GAP" : "ok"));
    }
    return o;
}

static size_t retail_count(AvpArena *a, const CcCompareResult *res, const char *callee) {
    const CcFn *fn = res ? res->retail_fn : NULL;
    if (!fn) return 0;
    WallCall *calls = NULL;
    size_t n = wall_calls(a, fn->rows, fn->nrows, &calls), k = 0;
    for (size_t i = 0; i < n; i++) k += avp_str_eqz(calls[i].name, callee);
    return k;
}

/* ---- inline: the phase hooks ----------------------------------------------------------------------------------- */

void sv_trace_inline_record(SvRun *run, SvUnitWork *w, SvRecord *rec, const CcCompareResult *res) {
    if (!run->trace_kind[SV_TRACE_INLINE] || !rec->callee) return;
    AvpArena *a = run->a;
    const SvInlTraced *t = inl_trace(run, w, "", NULL, NULL);
    AvpJson *j = inl_json(a, "inline", "ours", t);
    rec->trace = j;
    if (!t->ok) {
        sv_note(a, rec, "inline trace: could not be made (%s): no compiler numbers", t->error);
        return;
    }
    const char *C = rec->callee;
    bool self = !strcmp(C, rec->function);
    bool retail_calls = rec->direction == SV_DIR_OURS_EXPANDED;
    SvInlSites ss = inl_sites(a, &t->run, self ? NULL : rec->function, C);
    AvpJson *js = avp_json_arr(a);
    for (size_t i = 0; i < ss.n; i++) avp_json_arr_push(a, js, site_json(a, &ss.v[i]));
    jset(a, j, "sites", js);
    jset(a, j, "gaps", gaps_json(a, &t->run));
    SvInlGap g = {0};
    if (!self && inl_gap(a, &t->run, rec->function, C, &g)) {
        g.retail_calls = (i64)retail_count(a, res, C);
        AvpJson *jg = avp_json_obj(a);
        jset(a, jg, "stage", avp_json_strz(a, g.stage));
        jset(a, jg, "trace_calls", ji(a, g.trace_calls));
        jset(a, jg, "ir_calls", ji(a, g.ir_calls));
        jset(a, jg, "object_calls", ji(a, g.object_calls));
        jset(a, jg, "retail_calls", ji(a, g.retail_calls));
        jset(a, jg, "why", jz(a, g.why));
        jset(a, j, "gap", jg);
        int kind = sv_inl_gap_kind(&g);
        const char *counts = sv_inl_gap_counts(a, &g);
        if (kind == 1) {
            /* the inliner decided as retail did: the calls go later, so the stop is no inline decision */
            sv_evidence(a, rec, "before the trace: class inline, ours: %s; condition: %s", rec->ours ? rec->ours : "-",
                        rec->condition ? rec->condition : "-");
            rec->cls = SV_OTHER;
            rec->model_gap = false;
            rec->cause = avp_sprintf(a, "calls removed after the inliner: %s", C);
            rec->ours = avp_sprintf(a, "the inliner leaves %lld call(s) of %s here; the object has %lld", (long long)g.trace_calls,
                                    C, (long long)g.object_calls);
            rec->trace_condition =
                avp_sprintf(a, "not an inline decision: the inliner calls %s %lld time(s) here, as retail does (%s); the "
                               "calls disappear at stage %s (%s), so the condition is on what removes them after the "
                               "inliner, not on its decision",
                            C, (long long)g.trace_calls, counts, g.stage, g.why ? g.why : "");
            rec->condition = rec->trace_condition;
            sv_note(a, rec, "inline trace: the self-check's %s gap (%s) explains the model check: reclassified from "
                            "inline",
                    g.stage, counts);
            jset(a, j, "reclassified", avp_json_strz(a, "calls removed after the inliner"));
        } else if (kind == 0) {
            rec->model_gap = true;
            rec->cause = avp_sprintf(a, "inline %s (trace gap: %s)", C, g.stage);
            rec->trace_condition = avp_sprintf(a, "the inline trace disagrees with the object for %s at stage %s (%s): "
                                                  "no condition in its numbers",
                                               C, g.stage, counts);
            sv_note(a, rec, "inline trace self-check: TRACE-GAP at stage %s for %s (%s): %s", g.stage, C, counts,
                    g.why ? g.why : "");
        } else {
            sv_note(a, rec, "inline trace: calls of %s are removed after the inliner (stage %s: %s), and the inliner's "
                            "count is not retail's",
                    C, g.stage, counts);
        }
        if (kind != 2) {
            jset(a, j, "condition", jz(a, rec->trace_condition));
            return;
        }
    }
    const SvInlSite *s = first_with(&ss, retail_calls ? "expanded" : "called");
    if (!s && ss.n) s = &ss.v[0];
    if (!s) {
        rec->trace_condition = avp_sprintf(a, "the inline trace has no call site of %s in %s", C, self ? "any caller" : rec->function);
        jset(a, j, "condition", jz(a, rec->trace_condition));
        return;
    }
    size_t same = count_with(&ss, s->decision);
    const char *where = s->line > 0 ? avp_sprintf(a, " (line %lld)", (long long)s->line) : " (line not known)";
    rec->trace_condition = avp_sprintf(a, "ours %s%s%s; %s", sv_inl_site_text(a, s), where,
                                       same > 1 ? avp_sprintf(a, ", %zu sites so decided", same) : "",
                                       sv_inl_condition(a, s, retail_calls));
    rec->cause = avp_sprintf(a, "inline %s (%s)", C, sv_inl_rule_label(a, s));
    jset(a, j, "site", site_json(a, s));
    jset(a, j, "condition", jz(a, rec->trace_condition));
}

static bool is_inline_flag(const AvpStrVec *g) { return g && g->n && avp_str_eqz(g->v[0], "-inline"); }

static const char *words_text(AvpArena *a, const AvpStrVec *w) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < w->n; i++) avp_buf_printf(&b, "%s%.*s", i ? " " : "", AVP_SF(w->v[i]));
    return b.p ? b.p : "";
}

void sv_trace_flag_record(SvRun *run, SvUnitWork *w, SvRecord *rec, const AvpStrVec *group) {
    if (!run->trace_kind[SV_TRACE_INLINE] || !is_inline_flag(group)) return;
    AvpArena *a = run->a;
    const char *flag = words_text(a, group);
    AvpStrVec drops = {0};
    avp_vec_push(&drops, avp_str(flag));
    const SvInlTraced *ours = inl_trace(run, w, "", NULL, NULL);
    const SvInlTraced *without = inl_trace(run, w, avp_sprintf(a, "drop\x1f%s", flag), NULL, &drops);
    AvpJson *j = inl_json(a, "inline-flag", avp_sprintf(a, "without %s", flag), without);
    jset(a, j, "flag", avp_json_strz(a, flag));
    jset(a, j, "ours_ok", avp_json_bool(ours->ok));
    rec->trace = j;
    if (!ours->ok || !without->ok) {
        sv_note(a, rec, "inline trace: could not be made (%s): no compiler numbers", !ours->ok ? ours->error : without->error);
        return;
    }
    /* the sites the dropped flag expands where the unit's flags call: in the function, else of it (the function
     * itself expanded into its callers) */
    AvpJson *changed = avp_json_arr(a);
    AvpBuf b;
    avp_buf_init(&b, a);
    const SvInlSite *first = NULL;
    size_t n = 0;
    bool of_it = false;
    for (int pass = 0; pass < 2 && !n; pass++) {
    of_it = pass == 1;
    SvInlSites wo = inl_sites(a, &without->run, of_it ? NULL : rec->function, of_it ? rec->function : NULL);
    AvpSet *seen = avp_set_new();
    for (size_t i = 0; i < wo.n; i++) {
        const SvInlSite *s = &wo.v[i];
        if (strcmp(s->decision, "expanded") || !avp_set_add(seen, avp_str(s->callee))) continue;
        SvInlSites os = inl_sites(a, &ours->run, of_it ? NULL : rec->function, s->callee);
        const SvInlSite *o = first_with(&os, "called");
        if (!o && os.n) o = &os.v[0];
        if (o && !strcmp(o->decision, "expanded")) continue; /* the flag does not change this one */
        AvpJson *x = avp_json_obj(a);
        jset(a, x, "without", site_json(a, s));
        jset(a, x, "with", o ? site_json(a, o) : avp_json_null());
        avp_json_arr_push(a, changed, x);
        if (n < 4)
            avp_buf_printf(&b, "%s%s%s; with it %s", n ? "; " : "", sv_inl_site_text(a, s),
                           s->line > 0 ? avp_sprintf(a, " (line %lld)", (long long)s->line) : "",
                           o ? sv_inl_site_text(a, o) : "the trace has no such site");
        if (!first) first = s;
        n++;
    }
    avp_set_free(seen);
    }
    jset(a, j, "changed", changed);
    jset(a, j, "of_the_function", avp_json_bool(of_it && n));
    jset(a, j, "gaps", gaps_json(a, &without->run));
    if (!n) {
        rec->trace_condition = avp_sprintf(a, "the trace of the compile without %s expands no call site in or of %s that "
                                              "the unit's flags call: the flag's effect is not an inline decision here",
                                           flag, rec->function);
    } else {
        rec->trace_condition = avp_sprintf(a, "without %s ours %s%s%s; retail calls what the unit's flags call", flag,
                                           of_it ? avp_sprintf(a, "expands %s into its callers: ", rec->function) : "", b.p,
                                           n > 4 ? avp_sprintf(a, "; %zu more site(s)", n - 4) : "");
        rec->cause = avp_sprintf(a, "flag %s (without it: %s)", flag, sv_inl_rule_label(a, first));
    }
    jset(a, j, "condition", jz(a, rec->trace_condition));
}

void sv_trace_probe_flag(SvRun *run, SvUnitWork *w, SvRecord *rec, SvProbe *pr, const AvpStrVec *words) {
    if (!run->trace_kind[SV_TRACE_INLINE] || !rec->callee || rec->cls != SV_INLINE || pr->error) return;
    AvpArena *a = run->a;
    const char *flag = words_text(a, words);
    AvpStrVec adds = {0};
    avp_vec_push(&adds, avp_str(flag));
    const SvInlTraced *ours = inl_trace(run, w, "", NULL, NULL);
    const SvInlTraced *with = inl_trace(run, w, avp_sprintf(a, "add\x1f%s", flag), &adds, NULL);
    if (!ours->ok || !with->ok) {
        pr->result = avp_sprintf(a, "%s; inline trace: could not be made (%s)", pr->result, !ours->ok ? ours->error : with->error);
        return;
    }
    bool self = !strcmp(rec->callee, rec->function);
    SvInlSites os = inl_sites(a, &ours->run, self ? NULL : rec->function, rec->callee);
    SvInlSites fs = inl_sites(a, &with->run, self ? NULL : rec->function, rec->callee);
    const char *want = rec->direction == SV_DIR_OURS_EXPANDED ? "expanded" : "called";
    const SvInlSite *o = first_with(&os, want);
    if (!o && os.n) o = &os.v[0];
    const SvInlSite *f = NULL;
    for (size_t i = 0; o && i < fs.n && !f; i++)
        if (fs.v[i].line == o->line) f = &fs.v[i];
    if (!f && fs.n) f = &fs.v[0];
    if (!o || !f)
        pr->result = avp_sprintf(a, "%s; inline trace: no site of %s %s", pr->result, rec->callee, !o ? "in ours" : "under the flag");
    else if (strcmp(o->decision, f->decision))
        pr->result = avp_sprintf(a, "%s; inline trace: the decision changes: under the flag %s (ours %s)", pr->result,
                                 sv_inl_site_text(a, f), sv_inl_site_text(a, o));
    else
        pr->result = avp_sprintf(a, "%s; inline trace: the decision is unchanged: %s", pr->result, sv_inl_site_text(a, f));
}

/* ---- regalloc: the phase hooks --------------------------------------------------------------------------------- */

static AvpJson *node_json(AvpArena *a, int cls, const SvRaNode *n) {
    AvpJson *o = avp_json_obj(a);
    jset(a, o, "name", jz(a, n->name));
    jset(a, o, "kind", avp_json_strz(a, kind_word(n->kind)));
    jset(a, o, "def_op", jz(a, n->def_op));
    jset(a, o, "def_at", ji(a, n->def_at));
    jset(a, o, "node", avp_json_int(a, n->index));
    jset(a, o, "root", avp_json_int(a, n->root));
    jset(a, o, "sweep", n->sweep > 0 ? avp_json_int(a, n->sweep) : avp_json_null());
    jset(a, o, "push_position", ji(a, n->push_pos));
    jset(a, o, "push_how", jz(a, n->push_how && *n->push_how ? n->push_how : NULL));
    jset(a, o, "select_position", ji(a, n->select_pos));
    jset(a, o, "degree", avp_json_int(a, n->degree));
    jset(a, o, "push_degree", ji(a, n->push_degree));
    jset(a, o, "spill_cost", ji(a, n->cost));
    jset(a, o, "register", n->reg >= 0 ? avp_json_strz(a, reg_name(a, cls, n->reg)) : avp_json_null());
    jset(a, o, "retail_register", n->retail_reg >= 0 ? avp_json_strz(a, reg_name(a, cls, n->retail_reg)) : avp_json_null());
    return o;
}

/* The roots of a pass coloured reg, in select order. */
static size_t holders(AvpArena *a, const MwtRaPass *p, int reg, const MwtRaNode **out, size_t max) {
    size_t n = 0;
    for (int pos = 0; pos < (int)p->select.n && n < max; pos++) {
        const MwtRaNode *x = pass_node(p, p->select.v[pos]);
        if (x && x->alias == x->index && !x->spilled && x->final_reg == reg) out[n++] = x;
    }
    for (size_t i = 0; i < p->nodes.n && n < max; i++) { /* roots select did not list (not expected) */
        const MwtRaNode *x = &p->nodes.v[i];
        bool listed = false;
        for (size_t k = 0; k < n; k++) listed = listed || out[k] == x;
        if (!listed && x->alias == x->index && !x->spilled && x->final_reg == reg && x->select_pos < 0) out[n++] = x;
    }
    AVP_UNUSED(a);
    return n;
}

static const char *cls_text(int cls) { return cls == MWT_RC_FPR ? "FPR" : cls == MWT_RC_GPR ? "GPR" : "other"; }

bool sv_trace_register(SvP2 *p, const char *var, int ours_reg, int retail_reg) {
    SvRun *run = p->run;
    if (!run->trace_kind[SV_TRACE_REGALLOC]) return false;
    AvpArena *a = run->a;
    SvRecord *rec = p->rec;
    const SvRaTraced *t = ra_trace(run, p->w, rec->function, (LvS){0});
    AvpJson *j = avp_json_obj(a);
    rec->trace = j;
    jset(a, j, "kind", avp_json_strz(a, "regalloc"));
    jset(a, j, "function", avp_json_strz(a, rec->function));
    jset(a, j, "ok", avp_json_bool(t->ok));
    jset(a, j, "error", jz(a, t->error));
    if (!t->ok) {
        sv_note(a, rec, "regalloc trace: could not be made (%s): the declaration-order rule is the model check", t->error);
        return false;
    }
    p->ra = &t->run;
    jset(a, j, "seconds", avp_json_float(a, avp_py_round(t->run.total_secs, 2)));
    const MwtRaCheck *ck = &t->run.check;
    jset(a, j, "self_check", avp_json_strz(a, ck->gaps.n ? "TRACE-GAP" : "ok"));
    AvpJson *jg = avp_json_arr(a);
    for (size_t i = 0; i < ck->gaps.n; i++) {
        AvpJson *o = avp_json_obj(a);
        jset(a, o, "what", avp_json_str(a, ck->gaps.v[i].what));
        jset(a, o, "why", avp_json_str(a, ck->gaps.v[i].why));
        avp_json_arr_push(a, jg, o);
    }
    jset(a, j, "gaps", jg);
    if (ck->gaps.n) {
        rec->model_gap = true;
        sv_note(a, rec, "regalloc trace self-check: TRACE-GAP (%.*s: %.*s)%s%s%s", AVP_SF(ck->gaps.v[0].what),
                AVP_SF(ck->gaps.v[0].why), var ? "; first differing variable `" : "", var ? var : "", var ? "`" : "");
    } else {
        sv_evidence(a, rec, "regalloc trace self-check holds: %zu variable(s) against the -sym on DWARF, the saved "
                            "nonvolatile registers against the prologue",
                    ck->checked.n);
    }
    /* a variable whose DWARF location the dump could not read: a check not made, not a disagreement */
    for (size_t i = 0; i < ck->uncovered.n; i++)
        if (avp_str_contains(ck->uncovered.v[i], AVP_S("no readable location")))
            sv_evidence(a, rec, "regalloc trace self-check: not covered: %.*s", AVP_SF(ck->uncovered.v[i]));
    int cls = ours_reg >= 100 || retail_reg >= 100 ? MWT_RC_FPR : MWT_RC_GPR;
    int ro = ours_reg >= 0 ? ours_reg % 100 : -1, rr = retail_reg >= 0 ? retail_reg % 100 : -1;
    jset(a, j, "class", avp_json_strz(a, cls_text(cls)));
    const MwtRaPass *pass = mwt_ra_final(t->run.fn, cls);
    const char *shape = NULL, *cond = NULL;
    const MwtRaNode *vn = NULL, *holder_node = NULL;
    if (!pass) {
        cond = avp_sprintf(a, "the regalloc trace has no %s allocation for the function", cls_text(cls));
    } else {
        if (var) {
            vn = named_node(pass, avp_str(var));
            if (!vn) cond = avp_sprintf(a, "the regalloc trace has no %s node named `%s` (an inlined copy is renamed)",
                                        cls_text(cls), var);
        } else if (ro >= 0) {
            /* no variable of our map: the trace may still name the node in ours' register */
            const MwtRaNode *hs[8];
            size_t n = holders(a, pass, ro, hs, 8), named = 0;
            for (size_t i = 0; i < n; i++)
                if (hs[i]->kind == 'p' || hs[i]->kind == 'l') {
                    named++;
                    vn = hs[i];
                }
            if (named != 1) vn = NULL;
        }
        if (vn && var && ro >= 0 && ro == rr) {
            SvRaNode v = sv_ra_node_of(a, pass, vn);
            v.retail_reg = rr;
            cond = avp_sprintf(a, "the record's reading gives `%s` %s in ours and in retail: no register difference of that "
                                  "variable to state (%s)",
                               var, reg_name(a, cls, ro), sv_ra_node_text(a, cls, &v));
            shape = "no register difference";
            jset(a, j, "variable", node_json(a, cls, &v));
            vn = NULL;
        } else if (vn && rr >= 0 && vn->final_reg == rr && ro >= 0 && ro != rr) {
            /* the variable's node already has retail's register: the row's register is another node's */
            const MwtRaNode *xs[8];
            size_t nx = holders(a, pass, ro, xs, 8);
            SvRaNode x = nx ? sv_ra_node_of(a, pass, xs[0]) : (SvRaNode){0}, v = sv_ra_node_of(a, pass, vn);
            v.retail_reg = rr;
            cond = avp_sprintf(a, "the trace colours `%s` %s in ours too, retail's register (%s), so the differing row's %s "
                                  "belongs to another node: %s",
                               var, reg_name(a, cls, rr), sv_ra_node_text(a, cls, &v), reg_name(a, cls, ro),
                               nx ? avp_sprintf(a, "%s%s", sv_ra_node_text(a, cls, &x),
                                                nx > 1 ? avp_sprintf(a, " (first of %zu nodes in it, by select order)", nx) : "")
                                  : avp_sprintf(a, "no node of ours is coloured %s", reg_name(a, cls, ro)));
            shape = "another node at the row";
            jset(a, j, "variable", node_json(a, cls, &v));
            if (nx) jset(a, j, "ours_holder", node_json(a, cls, &x));
            vn = NULL;
        } else if (vn && rr >= 0) {
            SvRaFacts f = {0};
            f.cls = cls;
            f.K = pass->K;
            f.var = sv_ra_node_of(a, pass, vn);
            f.var.retail_reg = rr;
            const MwtRaNode *hs[8];
            size_t n = holders(a, pass, rr, hs, 8);
            const MwtRaNode *h = NULL;
            int vroot = vn->alias;
            for (size_t i = 0; i < n && !h; i++)
                if (hs[i]->index != vroot && (hs[i]->kind == 'p' || hs[i]->kind == 'l')) h = hs[i];
            for (size_t i = 0; i < n && !h; i++)
                if (hs[i]->index != vroot) h = hs[i];
            if (h) {
                holder_node = h;
                f.have_holder = true;
                f.holder = sv_ra_node_of(a, pass, h);
                jset(a, j, "holder", node_json(a, cls, &f.holder));
            }
            jset(a, j, "variable", node_json(a, cls, &f.var));
            cond = sv_ra_condition(a, &f, &shape);
        } else if (!cond && ro >= 0 && rr >= 0) {
            const MwtRaNode *xs[8], *ys[8];
            size_t nx = holders(a, pass, ro, xs, 8), ny = holders(a, pass, rr, ys, 8);
            SvRaNode x = nx ? sv_ra_node_of(a, pass, xs[0]) : (SvRaNode){0}, y = ny ? sv_ra_node_of(a, pass, ys[0]) : (SvRaNode){0};
            cond = sv_ra_temps_condition(a, cls, ro, rr, nx ? &x : NULL, nx, ny ? &y : NULL, ny);
            shape = "temporaries only";
            /* which temporary is defined first is the order pass 1 of the scheduler issues their definitions in */
            if (nx && ny) {
                enum { ALL = 64 };
                const MwtRaNode *xs2[ALL], *ys2[ALL];
                size_t nx2 = holders(a, pass, ro, xs2, ALL), ny2 = holders(a, pass, rr, ys2, ALL);
                SvRaNode xa[ALL], ya[ALL];
                for (size_t i = 0; i < nx2; i++) xa[i] = sv_ra_node_of(a, pass, xs2[i]);
                for (size_t i = 0; i < ny2; i++) ya[i] = sv_ra_node_of(a, pass, ys2[i]);
                const char *order = sv_trace_temps_order(p, xa, nx2, ya, ny2);
                if (order) {
                    cond = avp_sprintf(a, "%s; %s", cond, order);
                    jset(a, j, "sched_temps", avp_json_strz(a, order));
                }
            }
            if (nx) jset(a, j, "ours_holder", node_json(a, cls, &x));
            if (ny) jset(a, j, "retail_holder", node_json(a, cls, &y));
        } else if (!cond) {
            cond = avp_sprintf(a, "the first differing row differs in no register (ours `%s`, retail `%s`): the trace has no "
                                  "node to state",
                               rec->ours ? rec->ours : "?", rec->retail ? rec->retail : "?");
        }
    }
    jset(a, j, "shape", jz(a, shape));
    jset(a, j, "condition", jz(a, cond));
    jset(a, j, "against", avp_json_arr(a));
    rec->trace_condition = cond;
    rec->condition = cond;
    if (shape) {
        bool named = vn && vn->name.n && vn->name.p[0] != '@' && (!holder_node || (holder_node->name.n && holder_node->name.p[0] != '@'));
        const char *key = !strcmp(shape, "temporaries only") ? "temporaries only"
                          : avp_sprintf(a, "%s, %s", named ? "named variables" : vn ? "inlined or compiler-made objects" : "nodes",
                                        shape);
        jset(a, j, "cause_key", avp_json_strz(a, key));
    }
    return true;
}

/* The named variables whose register differs between two traces of the function, per class, as "a\x1fb" pairs of the
 * swaps (each name with the other's register) and the lines of mwt_ra_diff that name a variable. */
static const char *against_text(AvpArena *a, const MwtRaFunc *A, const MwtRaFunc *B, AvpJson *lines_out) {
    AvpStrVec d = mwt_ra_diff(a, A, B);
    AvpBuf b, vb;
    avp_buf_init(&b, a);
    avp_buf_init(&vb, a);
    size_t named = 0, shown = 0;
    for (size_t i = 0; i < d.n; i++) {
        if (!d.v[i].n || d.v[i].p[0] != '`') continue;
        avp_json_arr_push(a, lines_out, avp_json_str(a, d.v[i]));
        if (shown < 6) avp_buf_printf(&vb, "%s%.*s", shown ? "; " : "", AVP_SF(d.v[i]));
        shown++;
        named++;
    }
    /* two variables that trade registers, with their sweeps */
    static const int CLS[] = {MWT_RC_GPR, MWT_RC_FPR};
    for (int ci = 0; ci < 2; ci++) {
        const MwtRaPass *pa = mwt_ra_final(A, CLS[ci]), *pb = mwt_ra_final(B, CLS[ci]);
        if (!pa || !pb) continue;
        for (size_t i = 0; i < pa->nodes.n; i++) {
            const MwtRaNode *x = &pa->nodes.v[i];
            if ((x->kind != 'p' && x->kind != 'l') || !x->key.n) continue;
            const MwtRaNode *xb = named_node(pb, x->name);
            if (!xb || xb->final_reg == x->final_reg || x->final_reg < 0) continue;
            for (size_t k = i + 1; k < pa->nodes.n; k++) {
                const MwtRaNode *y = &pa->nodes.v[k];
                if ((y->kind != 'p' && y->kind != 'l') || !y->key.n) continue;
                const MwtRaNode *yb = named_node(pb, y->name);
                if (!yb || yb->final_reg != x->final_reg || xb->final_reg != y->final_reg) continue;
                int sxa = sweep_of(pa, x), sya = sweep_of(pa, y), sxb = sweep_of(pb, xb), syb = sweep_of(pb, yb);
                const char *s = avp_sprintf(
                    a, "`%.*s` and `%.*s` trade %s and %s: in A nodes %d and %d (sweeps %d and %d), in B nodes %d and %d "
                       "(sweeps %d and %d)%s",
                    AVP_SF(x->name), AVP_SF(y->name), reg_name(a, CLS[ci], x->final_reg), reg_name(a, CLS[ci], y->final_reg),
                    x->index, y->index, sxa, sya, xb->index, yb->index, sxb, syb,
                    sxa == sya && sxb == syb && sxa == sxb ? avp_sprintf(a, ": they trade places in sweep %d", sxa) : "");
                avp_json_arr_push(a, lines_out, avp_json_strz(a, s));
                avp_buf_printf(&b, "%s%s", b.n ? "; " : "", s);
                named++;
            }
        }
    }
    if (vb.n) avp_buf_printf(&b, "%s%s%s", b.n ? "; " : "", vb.p,
                             shown > 6 ? avp_sprintf(a, "; %zu more variable(s) moved", shown - 6) : "");
    if (!named) {
        for (size_t i = 0; i < d.n; i++)
            if (avp_str_contains(d.v[i], AVP_S(" simplify ")) || avp_str_contains(d.v[i], AVP_S(" select "))) {
                avp_json_arr_push(a, lines_out, avp_json_str(a, d.v[i]));
                avp_buf_printf(&b, "%s%.*s", b.n ? "; " : "", AVP_SF(d.v[i]));
            }
        return avp_sprintf(a, "no named variable moves%s%s", b.n ? ": " : "", b.n ? b.p : "");
    }
    return b.p;
}

#define SV_AGAINST_MAX 4 /* regalloc --against traces per stop: the best candidate and the listed declaration probes */

const char *sv_trace_against(SvP2 *p, LvS text, const char *what) {
    SvRun *run = p->run;
    if (p->rec->cls == SV_SCHEDULE) return sv_trace_sched_against(p, text, what);
    if (!run->trace_kind[SV_TRACE_REGALLOC] || !p->ra || !p->ra->fn) return NULL;
    AvpArena *a = run->a;
    if (p->against_runs >= SV_AGAINST_MAX || (p->deadline && avp_time() > p->deadline)) {
        run->traces_skipped++;
        return "regalloc --against: not traced (the stop's trace budget is used)";
    }
    p->against_runs++;
    const SvRaTraced *t = ra_trace(run, p->w, p->symbol, text);
    if (!t->ok || !t->run.fn) return avp_sprintf(a, "regalloc --against: could not be made (%s)", t->error ? t->error : "?");
    AvpJson *lines = avp_json_arr(a);
    const char *txt = against_text(a, p->ra->fn, t->run.fn, lines);
    AvpJson *arr = p->rec->trace ? avp_json_getz(p->rec->trace, "against") : NULL;
    if (arr) {
        AvpJson *o = avp_json_obj(a);
        jset(a, o, "what", avp_json_strz(a, what));
        jset(a, o, "lines", lines);
        jset(a, o, "self_check", avp_json_strz(a, t->run.check.gaps.n ? "TRACE-GAP" : "ok"));
        avp_json_arr_push(a, arr, o);
    }
    return avp_sprintf(a, "regalloc --against (A ours, B the %s): %s%s", what, txt,
                       t->run.check.gaps.n ? " [B's self-check: TRACE-GAP]" : "");
}

bool sv_trace_keep_decl(SvP2 *p, LvS text) {
    if (!p->run->trace_kind[SV_TRACE_REGALLOC] || !p->ra || !p->ra->fn || p->helper) return true;
    AvpArena *a = p->run->a;
    LvCtx *cx = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        cx = lv_ctx_new(a, text, p->fn_symbol);
        avp_try_end(&t);
    } else {
        cx = NULL;
    }
    if (!cx) return true;
    SvVars nv = sv_source_locals(a, cx);
    const char **from = AVP_NEWN(a, const char *, p->ours.n + 1), **to = AVP_NEWN(a, const char *, nv.n + 1);
    for (size_t i = 0; i < p->ours.n; i++) from[i] = p->ours.v[i].name;
    for (size_t i = 0; i < nv.n; i++) to[i] = nv.v[i].name;
    AvpCzVec pairs = sv_inverted_pairs(a, from, p->ours.n, to, nv.n);
    if (!pairs.n) return true; /* no two locals change order (a scope move): the filter cannot tell */
    static const int CLS[] = {MWT_RC_GPR, MWT_RC_FPR};
    bool apart = false;
    for (int ci = 0; ci < 2; ci++) {
        const MwtRaPass *pass = mwt_ra_final(p->ra->fn, CLS[ci]);
        if (!pass) continue;
        int r = sv_swap_same_sweep(pass, &pairs);
        if (r == 1) return true;
        if (r == 0) apart = true;
    }
    if (!apart) return true;
    p->decl_dropped++;
    p->run->decl_dropped++;
    return false;
}
