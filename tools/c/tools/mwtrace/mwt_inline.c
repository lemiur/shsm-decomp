/* mwt_inline: the sites of a parsed trace, their decisions and numbers, the self-check against the object, the
 * reports (mwt_inline.h). The rules and their numbers are the compiler's (docs/compiler_inline.md):
 *   - a callee is a candidate when it has the inline keyword (qualifier 0x10) or CheckInlineability gave it the
 *     auto-inline flag (type flag 0x800: auto-inline on, an inlinable body, at most inline_max_auto_size
 *     statements, at most 1024 bytes of locals);
 *   - in the default top-down mode (inline depth 0, not bottom-up) level 0 has no size limit; a call that came in
 *     with an expanded body is checked again at the next level with cost limits 40 (level 1), 28 (2), 12 (3);
 *     passes end after level 3, or after a pass that expanded nothing;
 *   - a candidate whose body the classifier calls kind 0 is checked but never expanded; kind 3 bodies are queued
 *     (at most 64 per statement) and expanded as statements, kind 6 in place as an expression;
 *   - with -ipa file the call-graph visitor inlines each caller after its callees and undoes the whole caller when
 *     its cost grew past 2000 and four times the old cost, or past 10000 and 1.25 times. */
#include "mwt_inline.h"

#include "avp/fmt.h"

#include <stdlib.h>

typedef AVP_VEC(i64) I64Vec;

/* ---- lookups ---- */

static const MwtDef *def_of(const MwtTrace *t, u64 obj) {
    AvpVal *v = avp_imap_get(t->defs, obj);
    return v ? v->p : NULL;
}

static AvpStr link_of(const MwtTrace *t, u64 obj) {
    AvpVal *v = avp_imap_get(t->links, obj);
    return v ? *(AvpStr *)v->p : AVP_S("");
}

/* The link name of a site's callee, else its source name. */
static AvpStr callee_label(const MwtTrace *t, const MwtSite *s) {
    AvpStr l = link_of(t, s->callee);
    return l.n ? l : s->callee_name;
}

static bool candidate_flags(u32 qual, u32 tflags) { return (qual & 0x10) || (tflags & 0x800); }

/* ---- the rule of a call the decision refused as no candidate ---- */
static void not_candidate(AvpArena *a, const MwtTrace *t, MwtSite *s, const MwtDef *d) {
    s->rule = AVP_S("not-candidate");
    if (!candidate_flags(s->qual, s->tflags)) {
        if (!d) {
            s->detail = AVP_S("no-body: not defined in this unit");
        } else if (s->tflags & 0x20000) {
            s->detail = AVP_S("never-inline: the never_inline attribute");
        } else if (d->dont_inline) {
            s->detail = AVP_S("dont-inline: inlining is off where it was defined");
        } else if (!d->autoinline) {
            s->detail = AVP_S("noauto: no inline keyword and auto-inline is off (-inline noauto)");
        } else if (d->kind == 0) {
            s->detail = avp_str(avp_sprintf(a, "auto-not-inlinable: kind 0 (%.*s)", AVP_SF(d->kind_reason)));
        } else if (d->auto_status == 2) {
            s->detail = AVP_S("auto-statements: more statements than inline_max_auto_size");
            s->cost = d->auto_count;
            s->limit = d->auto_value;
        } else if (d->auto_status == 3) {
            s->detail = AVP_S("auto-locals: locals larger than 1024 bytes");
            s->cost = d->auto_value;
            s->limit = 1024;
        } else {
            s->detail = AVP_S("unknown(CInline_CheckInlineability)");
        }
    } else if (s->dt != 3 && s->dt != 4) {
        s->detail = avp_str(avp_sprintf(a, "not-a-function: object datatype %d", s->dt));
    } else if (s->dt == 4 && !(s->frflags & 8)) {
        s->detail = AVP_S("virtual-call: called through the vtable");
        s->indirect = true;
    } else if (s->qual & 4) {
        s->detail = AVP_S("qualifier-0x4: unknown(decision +0x72)");
    } else {
        s->detail = AVP_S("unknown(decision)");
    }
    if (s->indirect && !avp_str_startswith(s->detail, AVP_S("virtual-call")))
        s->detail = avp_str(avp_sprintf(a, "%.*s; a virtual call through the vtable", AVP_SF(s->detail)));
    (void)t;
}

static i64 level_limit(int level) { return level == 1 ? 40 : level == 2 ? 28 : 12; }

static const MwtCheck *check_at(const MwtSite *s, int level) {
    for (size_t i = s->checks.n; i-- > 0;)
        if (s->checks.v[i].level == level) return &s->checks.v[i];
    return s->checks.n ? &s->checks.v[s->checks.n - 1] : NULL;
}

static void decide(AvpArena *a, const MwtTrace *t, const MwtVisit *v, MwtSite *s) {
    const MwtDef *d = def_of(t, s->callee);
    s->cost = s->limit = -1;
    s->indirect = s->dt == 4 && !(s->frflags & 8); /* a virtual function called through the vtable */
    if (s->expanded) {
        const MwtCheck *c = check_at(s, s->expanded_level);
        s->decision = AVP_S("expanded");
        s->cost = c ? c->cost : -1;
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_putz(&b, s->by_statements ? "as statements; " : "as an expression; ");
        if (s->qual & 0x10) {
            avp_buf_putz(&b, "candidate: inline keyword");
        } else if (d && d->auto_status == 1) {
            avp_buf_printf(&b, "candidate: auto-inline, statements %lld, limit %d", (long long)d->auto_count, v->maxauto);
        } else {
            avp_buf_putz(&b, "candidate: auto-inline");
        }
        if (v->always > 0) {
            s->rule = AVP_S("always-inline");
        } else if (v->bottomup > 0 && c && avp_str_eqz(c->rule, "bottom-up-last")) {
            s->rule = AVP_S("bottom-up-last");
            avp_buf_putz(&b, "; the last bottom-up level takes it without a size test");
        } else if (v->bottomup > 0) {
            s->rule = AVP_S("bottom-up-size");
            s->cost = c ? c->size : -1;
            s->limit = c ? c->limit : -1;
            avp_buf_putz(&b, "; cost: the estimated size of its body with its own inlines");
        } else if (s->expanded_level == 0) {
            s->rule = AVP_S("level0");
            avp_buf_putz(&b, "; level 0 has no size limit");
        } else if (v->depth > 0) {
            s->rule = avp_str(avp_sprintf(a, "depth%d", s->expanded_level));
            avp_buf_printf(&b, "; inline depth %d: no cost limit", v->depth);
        } else {
            s->rule = avp_str(avp_sprintf(a, "level%d-cost", s->expanded_level));
            s->limit = level_limit(s->expanded_level);
        }
        s->detail = avp_buf_str(&b);
        return;
    }
    s->decision = AVP_S("called");
    const MwtCheck *c = s->checks.n ? &s->checks.v[s->checks.n - 1] : NULL;
    if (!c) {
        s->rule = AVP_S("unchecked");
        return;
    }
    AvpStr r = c->rule;
    if (avp_str_eqz(r, "not-candidate")) {
        not_candidate(a, t, s, d);
        if (d && c->log_line < d->log_line && !candidate_flags(s->qual, s->tflags) && candidate_flags(d->qual, d->tflags))
            s->detail = AVP_S("not-yet-defined: the callee's definition comes later in the compile");
    } else if (avp_str_eqz(r, "bottom-up-size")) {
        s->rule = r;
        s->cost = c->size;
        s->limit = c->limit;
        s->detail = AVP_S("the estimated size of its body with its own inlines, against inline_max_size as halved for the caller");
    } else if (avp_str_endswith(r, AVP_S("-cost")) && avp_str_startswith(r, AVP_S("level"))) {
        s->rule = r;
        s->cost = c->cost;
        s->limit = level_limit(c->level);
    } else if (avp_str_eqz(r, "pass")) {
        s->cost = c->cost;
        if (c->kind == 0) {
            s->rule = AVP_S("kind0");
            s->detail = avp_str(avp_sprintf(a, "the body is not inlinable (%.*s)",
                                            AVP_SF(d && d->kind_reason.n ? d->kind_reason : AVP_S("unknown"))));
        } else if (c->kind == 3 && c->queued >= 64) {
            s->rule = AVP_S("queue-full");
            s->detail = AVP_S("the statement-inline queue of this statement holds 64 calls");
            s->cost = c->queued;
            s->limit = 64;
        } else {
            s->rule = avp_str(avp_sprintf(a, "unknown(pass, kind %d, not expanded)", c->kind));
        }
    } else if (avp_str_eqz(r, "no-funcdata")) {
        s->rule = r;
        s->detail = AVP_S("the callee has no function data here (not defined yet, or no body in this unit)");
    } else if (!r.n) {
        s->rule = AVP_S("unknown(no return block seen)");
    } else {
        s->rule = r;
    }
}

/* ---- where an expanded body's calls came from ---- */
typedef struct {
    size_t seq;
    i64 site;
} LvlItem;
typedef AVP_VEC(LvlItem) LvlItems;

static int lvl_cmp(const void *x, const void *y) {
    const LvlItem *p = x, *q = y;
    return p->seq < q->seq ? -1 : p->seq > q->seq;
}

static LvlItems level_list(const MwtVisit *v, int level) {
    LvlItems out = {0};
    for (size_t i = 0; i < v->sites.n; i++) {
        const MwtSite *s = &v->sites.v[i];
        for (size_t k = 0; k < s->checks.n; k++)
            if (s->checks.v[k].level == level) {
                avp_vec_push(&out, ((LvlItem){s->checks.v[k].seq, (i64)i}));
                break;
            }
    }
    if (out.n > 1) qsort(out.v, out.n, sizeof out.v[0], lvl_cmp);
    return out;
}

/* The calls an expanded body brings in: the callee's own called sites when a visit before index `before` inlined
 * it (its IR after its own inlining: what the expansion copies), else the calls of its body as defined. */
static MwtBodyCalls body_of(const MwtTrace *t, u64 callee, size_t before) {
    MwtBodyCalls out = {0};
    for (size_t vi = before; vi-- > 0;) {
        const MwtVisit *v = &t->visits.v[vi];
        if (v->cur != callee) continue;
        if (v->reverted) break; /* its IR is the original again */
        for (size_t i = 0; i < v->sites.n; i++) {
            const MwtSite *s = &v->sites.v[i];
            if (s->discarded || !avp_str_eqz(s->decision, "called")) continue;
            avp_vec_push(&out, ((MwtBodyCall){s->callee, s->callee_name, s->indirect}));
        }
        return out;
    }
    const MwtDef *d = def_of(t, callee);
    if (d)
        for (size_t i = 0; i < d->body.n; i++) avp_vec_push(&out, d->body.v[i]);
    return out;
}

static bool body_has(const MwtTrace *t, u64 body, size_t before, u64 callee) {
    MwtBodyCalls b = body_of(t, body, before);
    bool hit = false;
    for (size_t i = 0; i < b.n && !hit; i++) hit = b.v[i].callee == callee;
    avp_vec_free(&b);
    return hit;
}

static i64 prev_pos(const LvlItems *prev, i64 site) {
    for (size_t k = 0; k < prev->n; k++)
        if (prev->v[k].site == site) return (i64)k;
    return -1;
}

/* The number of call nodes a body brings in (every one is checked at the next level). */
static i64 body_size(const MwtTrace *t, u64 body, size_t before) {
    MwtBodyCalls b = body_of(t, body, before);
    i64 n = 0;
    for (size_t i = 0; i < b.n; i++) n += b.v[i].callee != 0;
    avp_vec_free(&b);
    return n;
}

/* Level L's new call nodes come from the bodies expanded at level L-1: a run of new nodes between two nodes that
 * were already there belongs to the expansions between those two at L-1, in order. */
static void chains(const MwtTrace *t, size_t vindex, MwtVisit *v) {
    for (int L = 1; L <= v->max_level; L++) {
        LvlItems prev = level_list(v, L - 1), cur = level_list(v, L);
        i64 last_old = -1;
        for (size_t i = 0; i < cur.n;) {
            i64 pos = prev_pos(&prev, cur.v[i].site);
            if (pos >= 0) {
                last_old = pos;
                i++;
                continue;
            }
            size_t j = i; /* the run [i, j) of new nodes, and the next old one */
            while (j < cur.n && prev_pos(&prev, cur.v[j].site) < 0) j++;
            i64 next = j < cur.n ? prev_pos(&prev, cur.v[j].site) : (i64)prev.n;
            I64Vec cands = {0};
            for (i64 k = last_old + 1; k < next; k++) {
                const MwtSite *c = &v->sites.v[prev.v[k].site];
                if (c->expanded && c->expanded_level == L - 1) avp_vec_push(&cands, prev.v[k].site);
            }
            /* several bodies: assign the run in order when their sizes add up to it */
            bool assigned = false;
            if (cands.n > 1) {
                i64 total = 0;
                for (size_t k = 0; k < cands.n; k++) total += body_size(t, v->sites.v[cands.v[k]].callee, vindex);
                if (total == (i64)(j - i)) {
                    size_t at = i;
                    assigned = true;
                    for (size_t k = 0; k < cands.n; k++) {
                        i64 m = body_size(t, v->sites.v[cands.v[k]].callee, vindex);
                        for (i64 q = 0; q < m; q++, at++) {
                            MwtSite *s = &v->sites.v[cur.v[at].site];
                            if (s->first_level == L) s->via = cands.v[k];
                        }
                    }
                }
            }
            for (size_t q = i; q < j && !assigned; q++) {
                MwtSite *s = &v->sites.v[cur.v[q].site];
                if (s->first_level != L) continue;
                I64Vec fit = {0}; /* those whose body calls this callee */
                for (size_t k = 0; k < cands.n; k++) {
                    const MwtSite *c = &v->sites.v[cands.v[k]];
                    if (c->callee == s->callee || body_has(t, c->callee, vindex, s->callee)) avp_vec_push(&fit, cands.v[k]);
                }
                const I64Vec *use = fit.n ? &fit : &cands;
                if (use->n == 1) s->via = use->v[0];
                else
                    for (size_t k = 0; k < use->n; k++) avp_vec_push(&s->via_alts, use->v[k]);
                avp_vec_free(&fit);
            }
            avp_vec_free(&cands);
            i = j;
        }
        avp_vec_free(&prev);
        avp_vec_free(&cur);
    }
}
/* ---- source lines ---- */
static AvpStr file_text(AvpArena *a, AvpMap *cache, AvpStr path) {
    AvpVal *v = avp_map_get(cache, path);
    if (v) return *(AvpStr *)v->p;
    AvpStr *p = AVP_NEW(a, AvpStr);
    AvpBytes b = {0};
    if (avp_read_all(a, avp_cz(a, path), &b)) *p = avp_strn((const char *)b.v, b.n);
    bool created;
    avp_map_put(cache, path, &created)->p = p;
    return *p;
}

static AvpStr display_path(AvpArena *a, AvpStr path, const char *root) {
    AvpStr p = avp_str_replace(a, path, AVP_S("\\"), AVP_S("/"), -1);
    if (!root) return p;
    AvpStr r = avp_str_replace(a, avp_str(root), AVP_S("\\"), AVP_S("/"), -1);
    if (p.n > r.n && p.p[r.n] == '/' && avp_str_eq(avp_str_lower(a, avp_strn(p.p, r.n)), avp_str_lower(a, r)))
        return avp_strn(p.p + r.n + 1, p.n - r.n - 1);
    return p;
}

/* The compiler's statement offset counts as many line ends as the statement's line number, and after a `#line N`
 * that number is N's (measured: a statement 3 lines after `#line 634` has an offset whose line count is 637). The
 * physical line of logical line L: the one #line region (or the start of the file, before any) that numbers a line
 * L; -1 when none or several do. */
static i64 physical_line(AvpStr text, i64 logical) {
    i64 phys = 1, region_phys = 1, region_logical = 1, found = -1;
    int hits = 0;
    size_t i = 0;
    bool any = false;
    while (i <= text.n) {
        size_t e = i;
        while (e < text.n && text.p[e] != '\n') e++;
        AvpStr ln = avp_str_lstrip(avp_strn(text.p + i, e - i));
        if (avp_str_startswith(ln, AVP_S("#line "))) {
            i64 n = 0;
            bool ovf = false;
            AvpStr num = avp_str_strip(avp_strn(ln.p + 6, ln.n - 6));
            i64 sp = avp_str_findc(num, ' ');
            if (sp > 0) num = avp_strn(num.p, (size_t)sp);
            if (avp_parse_int(num, 10, &n, &ovf)) {
                any = true;
                region_phys = phys + 1;
                region_logical = n;
            }
        } else if (region_logical + (phys - region_phys) == logical) {
            if (!hits++) found = phys;
        }
        phys++;
        i = e + 1;
    }
    if (!any) return logical;
    return hits == 1 ? found : -1;
}

static void site_line(AvpArena *a, const MwtTrace *t, AvpMap *cache, const char *root, MwtSite *s) {
    s->line = -1;
    if (s->offset < 0 || !s->fp) return;
    AvpVal *v = avp_imap_get(t->files, s->fp);
    if (!v) return;
    AvpStr path = *(AvpStr *)v->p;
    if (!path.n) return;
    s->file = display_path(a, path, root);
    AvpStr text = file_text(a, cache, path);
    /* an offset at or past the end: a #line number beyond the file's own lines (no line can be given) */
    if (!text.n || (size_t)s->offset >= text.n) return;
    i64 line = 1;
    for (i64 i = 0; i < s->offset; i++)
        if (text.p[i] == '\n') line++;
    s->line = physical_line(text, line);
}

/* ---- ordinals: per callee, by first level, line, then the order of the first check ---- */
static const MwtSites *g_sort_sites;
static int ord_cmp(const void *x, const void *y) {
    const MwtSite *p = &g_sort_sites->v[*(const size_t *)x], *q = &g_sort_sites->v[*(const size_t *)y];
    if (p->first_level != q->first_level) return p->first_level < q->first_level ? -1 : 1;
    if (p->line != q->line) return p->line < q->line ? -1 : 1;
    return p->first_index < q->first_index ? -1 : p->first_index > q->first_index;
}

static void ordinals(MwtSites *ss) {
    size_t *idx = avp_xcalloc(ss->n ? ss->n : 1, sizeof *idx);
    for (size_t i = 0; i < ss->n; i++) idx[i] = i;
    g_sort_sites = ss;
    if (ss->n > 1) qsort(idx, ss->n, sizeof *idx, ord_cmp);
    for (size_t i = 0; i < ss->n; i++) {
        MwtSite *s = &ss->v[idx[i]];
        int n = 0;
        for (size_t k = 0; k < i; k++) {
            const MwtSite *o = &ss->v[idx[k]];
            if (o->callee && s->callee ? o->callee == s->callee : avp_str_eq(o->callee_name, s->callee_name)) n++;
        }
        s->ordinal = n + 1;
    }
    free(idx);
}

/* ---- the self-check ---- */
typedef struct {
    AvpStr name;
    i64 n;
    u64 obj; /* one callee object of that name */
} Count;
typedef AVP_VEC(Count) Counts;

static void count_add(Counts *c, AvpStr name, u64 obj) {
    for (size_t i = 0; i < c->n; i++)
        if (avp_str_eq(c->v[i].name, name)) {
            c->v[i].n++;
            return;
        }
    avp_vec_push(c, ((Count){name, 1, obj}));
}

static i64 count_of(const Counts *c, AvpStr name) {
    for (size_t i = 0; i < c->n; i++)
        if (avp_str_eq(c->v[i].name, name)) return c->v[i].n;
    return 0;
}

/* Is the function of this link name an inline candidate (from its definition or any check of it)? */
static bool link_is_candidate(const MwtTrace *t, AvpStr link) {
    AvpIMapIter it = {0};
    while (avp_imap_next(t->links, &it)) {
        if (!avp_str_eq(*(AvpStr *)it.val->p, link)) continue;
        const MwtDef *d = def_of(t, it.key);
        if (d && candidate_flags(d->qual, d->tflags)) return true;
        for (size_t vi = 0; vi < t->visits.n; vi++) {
            const MwtVisit *v = &t->visits.v[vi];
            for (size_t i = 0; i < v->sites.n; i++)
                if (v->sites.v[i].callee == it.key && candidate_flags(v->sites.v[i].qual, v->sites.v[i].tflags)) return true;
        }
    }
    return false;
}

static u64 obj_of_link(const MwtTrace *t, AvpStr link) {
    AvpIMapIter it = {0};
    while (avp_imap_next(t->links, &it))
        if (avp_str_eq(*(AvpStr *)it.val->p, link)) return it.key;
    return 0;
}

static void gap(AvpArena *a, MwtResult *r, AvpStr caller, AvpStr callee, i64 tc, i64 ic, i64 oc, const char *stage,
                const char *why) {
    avp_vec_push(&r->gaps, ((MwtGap){caller, callee, tc, ic, oc, avp_str(stage), avp_str(avp_sprintf(a, "%s", why))}));
}

static MwtSite synth_site(AvpArena *a, MwtVisit *v, u64 callee, AvpStr name, bool indirect, const char *rule,
                          const char *detail) {
    MwtSite ns = {0};
    ns.callee = callee;
    ns.callee_name = name;
    ns.indirect = indirect;
    ns.first_index = v->sites.n;
    ns.first_level = v->max_level + 1;
    ns.offset = -1;
    ns.line = -1;
    ns.via = -1;
    ns.unchecked = true;
    ns.decision = AVP_S("called");
    ns.rule = avp_str(rule);
    ns.detail = avp_str(avp_sprintf(a, "%s", detail));
    ns.cost = ns.limit = -1;
    return ns;
}

/* The candidates for the body a last-pass call came from: the sites expanded at the visit's last level whose
 * body calls that callee. */
static void last_pass_parents(const MwtTrace *t, size_t vindex, const MwtVisit *v, u64 callee, MwtSite *ns) {
    for (size_t i = 0; i < v->sites.n; i++) {
        const MwtSite *s = &v->sites.v[i];
        if (!s->expanded || s->expanded_level != v->max_level || s->unchecked) continue;
        if (body_has(t, s->callee, vindex, callee)) avp_vec_push(&ns->via_alts, (i64)i);
    }
    if (ns->via_alts.n == 1) {
        ns->via = ns->via_alts.v[0];
        ns->via_alts.n = 0;
    }
}

/* Calls of the post-inline IR no check stands for: the bodies expanded at the last pass, whose calls are never
 * checked (the pass loop ends after the last level, and after a pass that queued no statement inline). */
static void ir_sites(AvpArena *a, const MwtTrace *t, size_t vindex, MwtVisit *v, MwtResult *r, AvpStr caller) {
    Counts tc = {0}, ic = {0};
    for (size_t i = 0; i < v->sites.n; i++) {
        const MwtSite *s = &v->sites.v[i];
        if (!s->discarded && !s->indirect && avp_str_eqz(s->decision, "called")) count_add(&tc, callee_label(t, s), s->callee);
    }
    for (size_t i = 0; i < v->ir.n; i++) {
        const MwtBodyCall *b = &v->ir.v[i];
        if (b->indirect) continue;
        AvpStr l = link_of(t, b->callee);
        count_add(&ic, l.n ? l : b->name, b->callee);
    }
    for (size_t i = 0; i < ic.n; i++) {
        i64 extra = ic.v[i].n - count_of(&tc, ic.v[i].name);
        for (i64 k = 0; k < extra; k++) {
            MwtSite ns = synth_site(a, v, ic.v[i].obj, ic.v[i].name, false, "last-pass",
                                    avp_sprintf(a, "in a body expanded at the last pass (level %d): never checked", v->max_level));
            last_pass_parents(t, vindex, v, ic.v[i].obj, &ns);
            if (ns.via < 0 && !ns.via_alts.n) {
                ns.rule = AVP_S("unseen");
                ns.detail = AVP_S("a call of the post-inline IR no check saw and no last-pass body explains");
                gap(a, r, caller, ic.v[i].name, count_of(&tc, ic.v[i].name), ic.v[i].n, -1, "trace-vs-ir",
                    "the post-inline IR has a call no check saw");
            }
            avp_vec_push(&v->sites, ns);
        }
    }
    avp_vec_free(&tc);
    avp_vec_free(&ic);
}

/* Without an IR snapshot (CInline_Expand at code generation): the bodies expanded at the last pass bring their
 * calls as the callee's own IR has them. */
static void pass_limit_sites(AvpArena *a, const MwtTrace *t, size_t vindex, MwtVisit *v) {
    size_t n = v->sites.n;
    for (size_t i = 0; i < n; i++) {
        if (!v->sites.v[i].expanded || v->sites.v[i].expanded_level != v->max_level || v->sites.v[i].unchecked) continue;
        MwtBodyCalls b = body_of(t, v->sites.v[i].callee, vindex);
        for (size_t k = 0; k < b.n; k++) {
            MwtSite ns = synth_site(a, v, b.v[k].callee, b.v[k].name, b.v[k].indirect, "last-pass",
                                    avp_sprintf(a, "in a body expanded at the last pass (level %d): never checked", v->max_level));
            ns.via = (i64)i;
            avp_vec_push(&v->sites, ns);
        }
        avp_vec_free(&b);
    }
}

static void growth_revert(AvpArena *a, MwtVisit *v) {
    for (size_t i = 0; i < v->sites.n; i++) {
        MwtSite *s = &v->sites.v[i];
        if (s->first_level > 0) {
            s->discarded = true;
            continue;
        }
        if (!s->expanded) continue;
        s->decision = AVP_S("called");
        s->rule = AVP_S("growth-revert");
        s->detail = avp_str(avp_sprintf(a, "the caller's cost after inlining %lld against %lld before: all its expansions undone",
                                        (long long)v->new_cost, (long long)v->old_cost));
        s->cost = v->new_cost;
        /* undone when (new > 2000 and new > 4 x old) or (new > 10000 and new > old + old / 4) */
        s->limit = v->new_cost > 10000 ? v->old_cost + v->old_cost / 4 : (4 * v->old_cost > 2000 ? 4 * v->old_cost : 2000);
    }
}

static void compare_object(AvpArena *a, const MwtTrace *t, const MwtCaller *c, const MwtObjFunc *f, MwtResult *r) {
    const MwtVisit *v = c->visit;
    Counts ec = {0}, ic = {0}, oc = {0};
    for (size_t i = 0; i < v->sites.n; i++) {
        const MwtSite *s = &v->sites.v[i];
        if (s->discarded || s->indirect || !avp_str_eqz(s->decision, "called")) continue;
        count_add(&ec, callee_label(t, s), s->callee);
    }
    bool ir = v->has_ir && !v->reverted;
    for (size_t i = 0; ir && i < v->ir.n; i++) {
        const MwtBodyCall *b = &v->ir.v[i];
        if (b->indirect) continue;
        AvpStr l = link_of(t, b->callee);
        count_add(&ic, l.n ? l : b->name, b->callee);
    }
    for (size_t k = 0; k < f->calls.n; k++) count_add(&oc, f->calls.v[k].target, 0);
    for (size_t i = 0; i < ec.n; i++) {
        AvpStr nm = ec.v[i].name;
        i64 e = ec.v[i].n, o = count_of(&oc, nm), n_ir = ir ? count_of(&ic, nm) : -1;
        if (e == o) continue;
        if (ir && n_ir < e)
            gap(a, r, c->caller, nm, e, n_ir, o, "simple-optimizer",
                "calls the trace leaves are gone from the post-inline IR: COpt_SimpleOptimizer runs between the passes and it");
        else if (ir && o < n_ir)
            gap(a, r, c->caller, nm, e, n_ir, o, "after-inliner",
                "the post-inline IR has the calls the trace leaves; the object has fewer: removed after the inliner");
        else
            gap(a, r, c->caller, nm, e, n_ir, o, ir ? "after-inliner" : "trace-vs-object",
                o < e ? "the trace leaves more calls than the object has" : "the object has more calls than the trace leaves");
    }
    for (size_t i = 0; i < oc.n; i++) {
        if (count_of(&ec, oc.v[i].name)) continue;
        if (link_is_candidate(t, oc.v[i].name))
            gap(a, r, c->caller, oc.v[i].name, 0, ir ? count_of(&ic, oc.v[i].name) : -1, oc.v[i].n, "trace-vs-object",
                "the object calls an inline candidate no trace site leaves");
        else
            avp_vec_push(&r->untraced, avp_str(avp_sprintf(a, "%.*s -> %.*s x%lld", AVP_SF(c->caller), AVP_SF(oc.v[i].name),
                                                           (long long)oc.v[i].n)));
    }
    avp_vec_free(&ec);
    avp_vec_free(&ic);
    avp_vec_free(&oc);
}

void mwt_inline_analyse(AvpArena *a, MwtTrace *t, const MwtObjFuncs *objs, const char *root, MwtResult *r) {
    memset(r, 0, sizeof *r);
    AvpMap *cache = avp_map_new();
    for (size_t e = 0; e < t->errors.n; e++)
        gap(a, r, AVP_S("(trace)"), AVP_S(""), -1, -1, -1, "trace",
            avp_sprintf(a, "the trace is incomplete: %.*s", AVP_SF(t->errors.v[e])));
    /* the last visit of each caller holds its final IR */
    for (size_t vi = 0; vi < t->visits.n; vi++) {
        MwtVisit *v = &t->visits.v[vi];
        bool later = false;
        for (size_t k = vi + 1; k < t->visits.n; k++)
            if (t->visits.v[k].cur == v->cur) later = true;
        if (later) continue;
        AvpStr caller = link_of(t, v->cur);
        if (!caller.n) { /* no link name made: a C name is its own link name (CMangler_GetLinkName returns it as is) */
            const MwtDef *d = def_of(t, v->cur);
            if (d && objs && mwt_obj_find(objs, d->name)) caller = d->name;
            else caller = avp_str(avp_sprintf(a, "?%.*s", AVP_SF(d ? d->name : AVP_S("unknown"))));
        }
        for (size_t i = 0; i < v->sites.n; i++) {
            site_line(a, t, cache, root, &v->sites.v[i]);
            decide(a, t, v, &v->sites.v[i]);
        }
        chains(t, vi, v);
        if (v->reverted) growth_revert(a, v);
        else if (v->has_ir) ir_sites(a, t, vi, v, r, caller);
        else pass_limit_sites(a, t, vi, v);
        ordinals(&v->sites);
        MwtCaller c = {caller, true, v, {0}, objs ? mwt_obj_find(objs, caller) != NULL : false};
        avp_vec_push(&r->callers, c);
        r->nsites += v->sites.n;
    }
    if (!objs) return;
    /* unvisited callers: the object's functions no visit names */
    for (size_t fi = 0; fi < objs->n; fi++) {
        const MwtObjFunc *f = &objs->v[fi];
        bool seen = false;
        for (size_t i = 0; i < r->callers.n && !seen; i++)
            if (avp_str_eq(r->callers.v[i].caller, f->name)) seen = true;
        if (seen) continue;
        MwtCaller c = {f->name, false, NULL, {0}, true};
        for (size_t k = 0; k < f->calls.n; k++) {
            MwtSite s = {0};
            s.callee = obj_of_link(t, f->calls.v[k].target);
            s.callee_name = f->calls.v[k].target;
            s.first_index = k;
            s.offset = -1;
            s.line = -1;
            s.via = -1;
            s.unchecked = true;
            s.decision = AVP_S("called");
            s.cost = s.limit = -1;
            const MwtDef *d = def_of(t, s.callee);
            s.qual = d ? d->qual : 0;
            s.tflags = d ? d->tflags : 0;
            if (d && !candidate_flags(d->qual, d->tflags)) {
                not_candidate(a, t, &s, d);
            } else if (!d) {
                s.detail = AVP_S("no-body: not defined in this unit");
            } else {
                s.detail = AVP_S("the callee is a candidate, but the visitor did not inline this caller");
            }
            s.rule = AVP_S("not-visited");
            avp_vec_push(&c.synth, s);
            if (link_is_candidate(t, f->calls.v[k].target))
                gap(a, r, f->name, f->calls.v[k].target, 0, -1, 1, "trace-vs-object",
                    "the inliner never visited this caller, but it calls an inline candidate");
        }
        ordinals(&c.synth);
        r->nsites += c.synth.n;
        avp_vec_push(&r->callers, c);
    }
    /* visited callers: called sites against the post-inline IR and the object's calls, callee by callee */
    for (size_t ci = 0; ci < r->callers.n; ci++) {
        const MwtCaller *c = &r->callers.v[ci];
        if (!c->visited) continue;
        const MwtObjFunc *f = mwt_obj_find(objs, c->caller);
        if (f) compare_object(a, t, c, f, r); /* else not emitted (every use inlined, or unused): nothing to compare */
    }
}

/* ---- reports ---- */
/* Why a visited caller has no function in the object. One the compiler gave no link name (a static function that is
 * never emitted) whose every call the trace expanded was inlined everywhere and dropped; else it is just absent. */
static const char *not_in_object(AvpArena *a, const MwtTrace *t, const MwtCaller *c) {
    if (!c->visit) return "not in the object";
    u64 obj = c->visit->cur;
    size_t expanded = 0, called = 0;
    for (size_t vi = 0; vi < t->visits.n; vi++)
        for (size_t i = 0; i < t->visits.v[vi].sites.n; i++) {
            const MwtSite *s = &t->visits.v[vi].sites.v[i];
            if (s->discarded || s->callee != obj) continue;
            if (s->expanded) expanded++;
            else if (avp_str_eqz(s->decision, "called")) called++;
        }
    if (link_of(t, obj).n) return "not in the object";
    if (expanded && !called)
        return avp_sprintf(a, "not emitted: no link name was made (a static function) and its %zu call%s %s expanded",
                           expanded, expanded == 1 ? "" : "s", expanded == 1 ? "was" : "were");
    return "not in the object: no link name was made";
}

static AvpStr site_label(AvpArena *a, const MwtTrace *t, const MwtSites *ss, i64 i) {
    if (i < 0 || (size_t)i >= ss->n) return AVP_S("");
    const MwtSite *s = &ss->v[i];
    return avp_str(avp_sprintf(a, "%.*s#%d", AVP_SF(callee_label(t, s)), s->ordinal));
}

static bool caller_selected(const MwtCaller *c, const MwtTrace *t, const char *only) {
    if (!only) return true;
    if (avp_str_eqz(c->caller, only)) return true;
    if (c->visit) {
        const MwtDef *d = def_of(t, c->visit->cur);
        if (d && avp_str_eqz(d->name, only)) return true;
    }
    return false;
}

/* The trace's sites of the first disagreement: the caller's sites of that callee. */
static void gap_sites(AvpArena *a, const MwtTrace *t, const MwtResult *r, const MwtGap *g, AvpBuf *b) {
    for (size_t ci = 0; ci < r->callers.n; ci++) {
        const MwtCaller *c = &r->callers.v[ci];
        if (!avp_str_eq(c->caller, g->caller)) continue;
        const MwtSites *ss = c->visited ? &c->visit->sites : &c->synth;
        const char *sep = "; its sites:";
        for (size_t i = 0; i < ss->n; i++) {
            const MwtSite *s = &ss->v[i];
            if (s->discarded || !avp_str_eq(callee_label(t, s), g->callee)) continue;
            avp_buf_printf(b, "%s #%d %.*s", sep, s->ordinal, AVP_SF(s->decision));
            if (s->line > 0) avp_buf_printf(b, " line %lld", (long long)s->line);
            if (s->via >= 0) avp_buf_printf(b, " via %.*s", AVP_SF(site_label(a, t, ss, s->via)));
            sep = ",";
        }
    }
}

static void numbers(AvpBuf *b, const MwtSite *s) {
    if (s->cost >= 0 && s->limit >= 0) avp_buf_printf(b, ": cost %lld, limit %lld", (long long)s->cost, (long long)s->limit);
    else if (s->cost >= 0) avp_buf_printf(b, ": cost %lld", (long long)s->cost);
}

AvpStr mwt_inline_text(AvpArena *a, const MwtJob *j, const MwtTrace *t, const MwtResult *r, const char *only,
                       double secs) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_printf(&b, "mwtrace inline %s: compiler %s, source %s%s, %.2f s\n", j->unit->name, j->build,
                   avp_cz(a, display_path(a, avp_str(j->source), j->repo->root)), j->scratch ? " (--src)" : "", secs);
    for (size_t i = 0; i < j->added.n; i++) avp_buf_printf(&b, "  flag added: %.*s\n", AVP_SF(j->added.v[i]));
    for (size_t i = 0; i < j->dropped.n; i++) avp_buf_printf(&b, "  flag dropped: %.*s\n", AVP_SF(j->dropped.v[i]));
    if (t->visits.n) {
        const MwtVisit *v = &t->visits.v[0];
        avp_buf_printf(&b, "  options: auto-inline %s, inline_max_auto_size %d, inline depth %d, bottom-up %s, inlining %s\n",
                       v->autoinline > 0 ? "on" : "off", v->maxauto, v->depth, v->bottomup > 0 ? "on" : "off",
                       v->per_function ? "per function (CInline_Expand)" : "by the call graph (-ipa file)");
    }
    for (size_t ci = 0; ci < r->callers.n; ci++) {
        const MwtCaller *c = &r->callers.v[ci];
        if (!caller_selected(c, t, only)) continue;
        const MwtSites *ss = c->visited ? &c->visit->sites : &c->synth;
        avp_buf_printf(&b, "%.*s", AVP_SF(c->caller));
        if (!c->visited) avp_buf_putz(&b, "  (not visited: none of its callees is an inline candidate)");
        else if (c->visit->reverted)
            avp_buf_printf(&b, "  (growth revert: cost %lld after inlining, %lld before)", (long long)c->visit->new_cost,
                           (long long)c->visit->old_cost);
        if (c->visited && !c->in_object) avp_buf_printf(&b, "  (%s)", not_in_object(a, t, c));
        avp_buf_putc(&b, '\n');
        for (size_t i = 0; i < ss->n; i++) {
            const MwtSite *s = &ss->v[i];
            if (s->discarded) continue;
            avp_buf_printf(&b, "  %.*s #%d", AVP_SF(callee_label(t, s)), s->ordinal);
            if (s->line > 0) avp_buf_printf(&b, " %.*s:%lld", AVP_SF(s->file), (long long)s->line);
            avp_buf_printf(&b, ": %.*s %.*s", AVP_SF(s->decision), AVP_SF(s->rule));
            numbers(&b, s);
            if (s->detail.n) avp_buf_printf(&b, " (%.*s)", AVP_SF(s->detail));
            if (s->via >= 0) avp_buf_printf(&b, " via %.*s", AVP_SF(site_label(a, t, ss, s->via)));
            else if (s->via_alts.n) {
                avp_buf_putz(&b, " via one of");
                for (size_t k = 0; k < s->via_alts.n; k++)
                    avp_buf_printf(&b, " %.*s", AVP_SF(site_label(a, t, ss, s->via_alts.v[k])));
            }
            avp_buf_putc(&b, '\n');
        }
    }
    if (r->untraced.n) {
        avp_buf_printf(&b, "object calls no trace site stands for (callee no inline candidate): %zu\n", r->untraced.n);
        for (size_t i = 0; i < r->untraced.n; i++) avp_buf_printf(&b, "  %.*s\n", AVP_SF(r->untraced.v[i]));
    }
    if (!r->gaps.n) {
        avp_buf_printf(&b, "self-check: OK (%zu callers, %zu sites; trace and object agree)\n", r->callers.n, r->nsites);
    } else {
        avp_buf_printf(&b, "self-check: TRACE-GAP (%zu disagreements; the first:)\n", r->gaps.n);
        for (size_t i = 0; i < r->gaps.n; i++) {
            const MwtGap *g = &r->gaps.v[i];
            avp_buf_printf(&b, "  TRACE-GAP [%.*s] %.*s -> %.*s: trace %lld calls", AVP_SF(g->stage), AVP_SF(g->caller),
                           AVP_SF(g->callee), (long long)g->trace_calls);
            if (g->ir_calls >= 0) avp_buf_printf(&b, ", post-inline IR %lld", (long long)g->ir_calls);
            avp_buf_printf(&b, ", object %lld (%.*s)", (long long)g->object_calls, AVP_SF(g->why));
            if (i == 0) gap_sites(a, t, r, g, &b);
            avp_buf_putc(&b, '\n');
        }
    }
    return avp_buf_str(&b);
}

static AvpJson *jstr(AvpArena *a, AvpStr s) { return avp_json_str(a, s); }
static AvpJson *jint_or_null(AvpArena *a, i64 v) { return v < 0 ? avp_json_null() : avp_json_int(a, v); }

static void set(AvpArena *a, AvpJson *o, const char *k, AvpJson *v) { avp_json_set(a, o, avp_str(k), v); }

AvpJson *mwt_inline_json(AvpArena *a, const MwtJob *j, const MwtTrace *t, const MwtResult *r, const char *only,
                         double secs) {
    AvpJson *root = avp_json_obj(a);
    set(a, root, "unit", avp_json_strz(a, j->unit->name));
    set(a, root, "compiler", avp_json_strz(a, j->build));
    set(a, root, "source", jstr(a, display_path(a, avp_str(j->source), j->repo->root)));
    set(a, root, "scratch", avp_json_bool(j->scratch));
    AvpJson *fa = avp_json_arr(a), *fd = avp_json_arr(a);
    for (size_t i = 0; i < j->added.n; i++) avp_json_arr_push(a, fa, jstr(a, j->added.v[i]));
    for (size_t i = 0; i < j->dropped.n; i++) avp_json_arr_push(a, fd, jstr(a, j->dropped.v[i]));
    set(a, root, "flags_added", fa);
    set(a, root, "flags_dropped", fd);
    set(a, root, "seconds", avp_json_float(a, avp_clock_round(secs, 2)));
    AvpJson *callers = avp_json_arr(a);
    for (size_t ci = 0; ci < r->callers.n; ci++) {
        const MwtCaller *c = &r->callers.v[ci];
        if (!caller_selected(c, t, only)) continue;
        const MwtSites *ss = c->visited ? &c->visit->sites : &c->synth;
        AvpJson *co = avp_json_obj(a);
        set(a, co, "caller", jstr(a, c->caller));
        set(a, co, "visited", avp_json_bool(c->visited));
        set(a, co, "in_object", avp_json_bool(c->in_object));
        if (c->visited && !c->in_object) set(a, co, "not_in_object", avp_json_strz(a, not_in_object(a, t, c)));
        set(a, co, "reverted", avp_json_bool(c->visited && c->visit->reverted));
        if (c->visited) {
            const MwtVisit *v = c->visit;
            AvpJson *op = avp_json_obj(a);
            set(a, op, "autoinline", avp_json_int(a, v->autoinline));
            set(a, op, "inline_max_auto_size", avp_json_int(a, v->maxauto));
            set(a, op, "inline_depth", avp_json_int(a, v->depth));
            set(a, op, "bottom_up", avp_json_int(a, v->bottomup));
            set(a, op, "call_graph", avp_json_bool(!v->per_function));
            set(a, op, "last_level", avp_json_int(a, v->max_level));
            if (v->reverted) {
                set(a, op, "cost_before", avp_json_int(a, v->old_cost));
                set(a, op, "cost_after", avp_json_int(a, v->new_cost));
            }
            set(a, co, "options", op);
        }
        AvpJson *sites = avp_json_arr(a);
        for (size_t i = 0; i < ss->n; i++) {
            const MwtSite *s = &ss->v[i];
            if (s->discarded) continue;
            AvpJson *so = avp_json_obj(a);
            set(a, so, "callee", jstr(a, callee_label(t, s)));
            set(a, so, "callee_name", jstr(a, s->callee_name));
            set(a, so, "ordinal", avp_json_int(a, s->ordinal));
            set(a, so, "file", s->file.n ? jstr(a, s->file) : avp_json_null());
            set(a, so, "line", jint_or_null(a, s->line));
            set(a, so, "decision", jstr(a, s->decision));
            set(a, so, "rule", jstr(a, s->rule));
            set(a, so, "detail", jstr(a, s->detail));
            set(a, so, "cost", jint_or_null(a, s->cost));
            set(a, so, "limit", jint_or_null(a, s->limit));
            set(a, so, "level", avp_json_int(a, s->expanded ? s->expanded_level : s->first_level));
            set(a, so, "unchecked", avp_json_bool(s->unchecked));
            set(a, so, "via", s->via >= 0 ? jstr(a, site_label(a, t, ss, s->via)) : avp_json_null());
            AvpJson *alts = avp_json_arr(a);
            for (size_t k = 0; k < s->via_alts.n; k++) avp_json_arr_push(a, alts, jstr(a, site_label(a, t, ss, s->via_alts.v[k])));
            set(a, so, "via_candidates", alts);
            /* the chain of expansions that brought this call in, outermost first */
            AvpJson *chain = avp_json_arr(a);
            AvpStrVec up = {0};
            for (i64 p = s->via, guard = 0; p >= 0 && guard < 64; p = ss->v[p].via, guard++)
                avp_vec_push(&up, site_label(a, t, ss, p));
            for (size_t k = up.n; k-- > 0;) avp_json_arr_push(a, chain, jstr(a, up.v[k]));
            avp_vec_free(&up);
            set(a, so, "chain", chain);
            AvpJson *checks = avp_json_arr(a);
            for (size_t k = 0; k < s->checks.n; k++) {
                const MwtCheck *ck = &s->checks.v[k];
                AvpJson *k1 = avp_json_obj(a);
                set(a, k1, "level", avp_json_int(a, ck->level));
                set(a, k1, "rule", jstr(a, ck->rule));
                set(a, k1, "cost", jint_or_null(a, ck->cost));
                set(a, k1, "kind", jint_or_null(a, ck->kind));
                set(a, k1, "inlined", ck->xp < 0 ? avp_json_null() : avp_json_bool(ck->xp == 1));
                avp_json_arr_push(a, checks, k1);
            }
            set(a, so, "checks", checks);
            avp_json_arr_push(a, sites, so);
        }
        set(a, co, "sites", sites);
        avp_json_arr_push(a, callers, co);
    }
    set(a, root, "callers", callers);
    AvpJson *sc = avp_json_obj(a);
    set(a, sc, "status", avp_json_strz(a, r->gaps.n ? "TRACE-GAP" : "OK"));
    AvpJson *gaps = avp_json_arr(a);
    for (size_t i = 0; i < r->gaps.n; i++) {
        AvpJson *g = avp_json_obj(a);
        set(a, g, "caller", jstr(a, r->gaps.v[i].caller));
        set(a, g, "callee", jstr(a, r->gaps.v[i].callee));
        set(a, g, "stage", jstr(a, r->gaps.v[i].stage));
        set(a, g, "trace_calls", avp_json_int(a, r->gaps.v[i].trace_calls));
        set(a, g, "ir_calls", jint_or_null(a, r->gaps.v[i].ir_calls));
        set(a, g, "object_calls", avp_json_int(a, r->gaps.v[i].object_calls));
        set(a, g, "why", jstr(a, r->gaps.v[i].why));
        avp_json_arr_push(a, gaps, g);
    }
    set(a, sc, "gaps", gaps);
    AvpJson *un = avp_json_arr(a);
    for (size_t i = 0; i < r->untraced.n; i++) avp_json_arr_push(a, un, jstr(a, r->untraced.v[i]));
    set(a, sc, "untraced_calls", un);
    set(a, root, "selfcheck", sc);
    return root;
}

/* ---- the subcommand ---- */
static const char *usage(void) {
    return "usage: mwtrace.exe inline <unit> [--function SYM] [--flag \"<words>\"]... [--drop-flag W]... [--src FILE]\n"
           "                              [--json] [--out DIR]\n"
           "\n"
           "Compiles the unit (or --src FILE with the unit's flags) under the debugger and prints, per caller, one line\n"
           "per call site: callee #ordinal file:line: decision rule: cost, limit (detail) [via the expanded site that\n"
           "brought the call in]. --flag adds flag words, --drop-flag removes every run of the given words. The trace\n"
           "is checked against the object of the same compile: called sites must be its calls, expanded ones absent;\n"
           "a disagreement is TRACE-GAP (exit 1). --function limits the listing to one caller (link or source name).\n"
           "Output files: <out>/inline.txt, inline.json, log.txt, inline.wdbg, o.o (default out:\n"
           "build/mwtrace/<unit path>).";
}

bool mwt_inline_run(AvpArena *a, const char *unit, const char *src, const AvpStrVec *adds, const AvpStrVec *drops,
                    const char *outdir, MwtInlineRun *out, const char **why) {
    memset(out, 0, sizeof *out);
    if (!mwt_job_init(a, &out->job, unit, src, adds, drops, outdir, why)) return false;
    AvpStr spec;
    if (!mwt_build_expand(a, out->job.table, avp_str(MWT_INLINE_SPEC), &spec, why)) return false;
    AvpStr log;
    i64 code = -1;
    if (!mwt_job_run(a, &out->job, spec, "inline.wdbg", &log, &out->secs, &code, why)) return false;
    if (code != 0) {
        *why = avp_sprintf(a, "the compiler exited with %lld (log: %s/log.txt)", (long long)code, out->job.outdir);
        return false;
    }
    mwt_inline_parse(a, log, &out->trace);
    if (!mwt_obj_calls(a, avp_path_join(a, out->job.outdir, "o.o"), &out->objs, why)) return false;
    mwt_inline_analyse(a, &out->trace, &out->objs, out->job.repo->root, &out->result);
    return true;
}

int mwt_inline_main(AvpArena *a, int argc, char **argv) {
    const char *unit = NULL, *only = NULL, *src = NULL, *out = NULL;
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
                    !strcmp(s, "--out")) && i + 1 < argc) {
            const char *v = argv[++i];
            if (!strcmp(s, "--function")) only = v;
            else if (!strcmp(s, "--flag")) avp_vec_push(&adds, avp_str(v));
            else if (!strcmp(s, "--drop-flag")) avp_vec_push(&drops, avp_str(v));
            else if (!strcmp(s, "--src")) src = v;
            else out = v;
        } else if (s[0] == '-' || unit) {
            fprintf(stderr, "mwtrace.exe inline: unexpected argument %s\n%s\n", s, usage());
            return 2;
        } else {
            unit = s;
        }
    }
    if (!unit) {
        fprintf(stderr, "%s\n", usage());
        return 2;
    }
    const ShsmRepo *repo = shsm_repo_find(NULL);
    const char *path = strncmp(unit, "main/", 5) == 0 ? unit + 5 : unit;
    const char *outdir = out ? avp_path_abspath(a, out) : avp_join(a, repo->root, "build", "mwtrace", path);
    const char *build_dir = avp_path_abspath(a, avp_path_join(a, repo->root, "build"));
    if (strncmp(avp_path_normcase(a, outdir), avp_path_normcase(a, build_dir), strlen(build_dir)) != 0) {
        fprintf(stderr, "mwtrace.exe inline: --out must be under %s\n", build_dir);
        return 2;
    }
    MwtInlineRun run;
    const char *why = NULL;
    if (!mwt_inline_run(a, unit, src, &adds, &drops, outdir, &run, &why)) {
        fprintf(stderr, "mwtrace.exe inline: %s\n", why);
        return 2;
    }
    const MwtJob job = run.job;
    MwtResult res = run.result;
    AvpStr text = mwt_inline_text(a, &job, &run.trace, &res, only, run.secs);
    AvpJson *js = mwt_inline_json(a, &job, &run.trace, &res, only, run.secs);
    AvpJsonDumpOpts o = AVP_JSON_INDENT(1);
    AvpStr jtext = avp_json_dumps(a, js, &o);
    avp_write_file(avp_path_join(a, job.outdir, "inline.txt"), text.p, text.n);
    avp_write_file(avp_path_join(a, job.outdir, "inline.json"), jtext.p, jtext.n);
    if (json) printf("%.*s\n", AVP_SF(jtext));
    else fwrite(text.p, 1, text.n, stdout);
    return res.gaps.n ? 1 : 0;
}
