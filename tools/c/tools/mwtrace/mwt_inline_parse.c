/* mwt_inline_parse: the trace log of MWT_INLINE_SPEC into a MwtTrace. The lines (one event each):
 *
 *   compiler: <build> (...)                      the run's compiler build
 *   DEF <obj> <qual> <autoinline> <dont_inline> <name>   CheckInlineability's entry for a defined function
 *   DEFR <obj> <ok> <tflags> [qual4]             its answer (qual4: refused for qualifier 0x4)
 *   AUTO <obj> ok|fail-count|fail-locals <statements> <limit or locals>   the auto-inline size test
 *   BODY <obj> <callee> <datatype> <funcref flags> <name>   a call in a candidate's body (callee 0: indirect)
 *   KIND <obj> <kind> <reason>                   the inlinability classifier's answer
 *   EXPAND                                       CInline_Expand: the next CUR inlines one function (no call graph)
 *   CUR <obj> <autoinline> <maxauto> <depth> <bottomup> <always> <dont_inline> <x112>   a caller's passes start
 *   FILE <fp> <path>  STMT <stmt> <offset> <fp>  the statement the walker is in
 *   CHK <e> <cur> <callee> <level> <qual> <datatype> <tflags> <funcref flags> <name>   the decision's entry
 *   FD <cost> <kind>                             the callee's function data
 *   BU <size> <limit>                            bottom-up mode: the callee's estimated size and the size limit
 *   R <rule> [value]                             the decision's return block
 *   XP <e> <0|1>  Q <e> <queue length>  XS <e>   inlined in place / queued / expanded from the queue
 *   LN <obj> <link name>                         a link name
 *   CI <obj>  FC <callee> <datatype> <funcref flags> <name>  CIE   ComputeInlineability's walk: the calls of an IR
 *   REVERT <cur> <old cost> <new cost>  KEEP <cur>   the visitor's growth test
 * plus the debugger's own lines (HANDLER ERR, EXCEPTION, timeout), kept as errors. */
#include "mwt_inline.h"

static i64 num(AvpStr s) {
    i64 v = 0;
    bool ovf = false;
    bool neg = s.n && s.p[0] == '-';
    AvpStr d = neg ? avp_strn(s.p + 1, s.n - 1) : s;
    if (d.n > 2 && d.p[0] == '0' && (d.p[1] == 'x' || d.p[1] == 'X')) {
        if (!avp_parse_int(avp_strn(d.p + 2, d.n - 2), 16, &v, &ovf)) return -1;
    } else if (!avp_parse_int(d, 10, &v, &ovf)) {
        return -1;
    }
    return neg ? -v : v;
}

static MwtDef *def_get(AvpArena *a, MwtTrace *t, u64 obj, bool create) {
    AvpVal *v = avp_imap_get(t->defs, obj);
    if (v) return v->p;
    if (!create) return NULL;
    MwtDef *d = AVP_NEW(a, MwtDef);
    d->obj = obj;
    d->kind = -1;
    bool created;
    avp_imap_put(t->defs, obj, &created)->p = d;
    return d;
}

static MwtVisit *visit_new(MwtTrace *t, u64 cur) {
    MwtVisit v = {0};
    v.cur = cur;
    v.by_e = avp_imap_new();
    v.autoinline = v.maxauto = v.depth = v.bottomup = v.always = v.dont_inline = v.x112 = -1;
    avp_vec_push(&t->visits, v);
    return &avp_vec_last(&t->visits);
}

static MwtVisit *visit_of(MwtTrace *t, u64 cur) {
    for (size_t i = t->visits.n; i-- > 0;)
        if (t->visits.v[i].cur == cur) return &t->visits.v[i];
    return NULL;
}

static MwtSite *site_of(MwtVisit *v, u64 e) {
    AvpVal *p = avp_imap_get(v->by_e, e);
    return p ? &v->sites.v[p->u - 1] : NULL;
}

void mwt_inline_parse(AvpArena *a, AvpStr log, MwtTrace *t) {
    memset(t, 0, sizeof *t);
    t->defs = avp_imap_new();
    t->links = avp_imap_new();
    t->files = avp_imap_new();
    MwtVisit *v = NULL;
    MwtSite *last_site = NULL;
    MwtCheck *last = NULL;
    i64 cur_off = -1;
    u64 cur_fp = 0;
    size_t seq = 0;
    MwtBodyCalls *ir = NULL; /* the IR snapshot being read (between CI and CIE) */
    bool pending_expand = false; /* CInline_Expand was entered: the next CUR is a per-function inlining */
    AvpStrVec lines = avp_str_splitlines(log, false);
    for (size_t li = 0; li < lines.n; li++) {
        AvpStr line = lines.v[li];
        if (avp_str_startswith(line, AVP_S("compiler: "))) {
            AvpStrVec w = avp_str_split(line, AVP_S(" "), 2);
            if (w.n >= 2) t->build = avp_str_dup(a, w.v[1]);
            continue;
        }
        if (avp_str_startswith(line, AVP_S("HANDLER ERR")) || avp_str_startswith(line, AVP_S("EXCEPTION")) ||
            avp_str_eqz(line, "timeout")) {
            avp_vec_push(&t->errors, avp_str_dup(a, line));
            continue;
        }
        i64 sp = avp_str_findc(line, ' ');
        if (avp_str_eqz(line, "EXPAND")) {
            pending_expand = true;
            continue;
        }
        if (sp <= 0) continue;
        AvpStr tag = avp_strn(line.p, (size_t)sp);
        if (avp_str_eqz(tag, "DEF")) {
            AvpStrVec w = avp_str_split(line, AVP_S(" "), 5);
            if (w.n < 5) continue;
            MwtDef *d = def_get(a, t, (u64)num(w.v[1]), true);
            u64 obj = d->obj;
            memset(d, 0, sizeof *d);
            d->obj = obj;
            d->log_line = li;
            d->kind = -1;
            d->qual = (u32)num(w.v[2]);
            d->autoinline = num(w.v[3]) != 0;
            d->dont_inline = num(w.v[4]) != 0;
            d->name = w.n > 5 ? avp_str_dup(a, w.v[5]) : AVP_S("");
        } else if (avp_str_eqz(tag, "DEFR")) {
            AvpStrVec w = avp_str_split(line, AVP_S(" "), -1);
            if (w.n < 4) continue;
            MwtDef *d = def_get(a, t, (u64)num(w.v[1]), true);
            d->has_result = true;
            d->candidate = num(w.v[2]) != 0;
            d->tflags = (u32)num(w.v[3]);
            d->qual4 = w.n > 4 && avp_str_eqz(w.v[4], "qual4");
        } else if (avp_str_eqz(tag, "AUTO")) {
            AvpStrVec w = avp_str_split(line, AVP_S(" "), -1);
            if (w.n < 5) continue;
            MwtDef *d = def_get(a, t, (u64)num(w.v[1]), true);
            d->auto_status = avp_str_eqz(w.v[2], "ok") ? 1 : avp_str_eqz(w.v[2], "fail-count") ? 2 : 3;
            d->auto_count = num(w.v[3]);
            d->auto_value = num(w.v[4]);
        } else if (avp_str_eqz(tag, "BODY")) {
            AvpStrVec w = avp_str_split(line, AVP_S(" "), 5);
            if (w.n < 6) continue;
            MwtDef *d = def_get(a, t, (u64)num(w.v[1]), true);
            MwtBodyCall bc = {(u64)num(w.v[2]), avp_str_dup(a, w.v[5]), false};
            int dt = (int)num(w.v[3]);
            u32 frf = (u32)num(w.v[4]);
            bc.indirect = bc.callee == 0 || (dt == 4 && !(frf & 8));
            avp_vec_push(&d->body, bc);
        } else if (avp_str_eqz(tag, "CI")) {
            AvpStrVec w = avp_str_split(line, AVP_S(" "), -1);
            if (w.n < 2) continue;
            u64 obj = (u64)num(w.v[1]);
            if (v && v->cur == obj && !v->has_ir) { /* the visitor measures the caller right after its inlining */
                v->has_ir = true;
                ir = &v->ir;
            } else {
                MwtDef *d = def_get(a, t, obj, true);
                d->has_ir = true;
                d->ir.n = 0;
                ir = &d->ir;
            }
        } else if (avp_str_eqz(tag, "CIE")) {
            ir = NULL;
        } else if (avp_str_eqz(tag, "FC")) {
            AvpStrVec w = avp_str_split(line, AVP_S(" "), 4);
            if (!ir || w.n < 5) continue;
            MwtBodyCall bc = {(u64)num(w.v[1]), avp_str_dup(a, w.v[4]), false};
            bc.indirect = bc.callee == 0 || (num(w.v[2]) == 4 && !(num(w.v[3]) & 8));
            avp_vec_push(ir, bc);
        } else if (avp_str_eqz(tag, "KIND")) {
            AvpStrVec w = avp_str_split(line, AVP_S(" "), 3);
            if (w.n < 4) continue;
            MwtDef *d = def_get(a, t, (u64)num(w.v[1]), true);
            d->kind = (int)num(w.v[2]);
            d->kind_reason = avp_str_dup(a, w.v[3]);
        } else if (avp_str_eqz(tag, "CUR")) {
            AvpStrVec w = avp_str_split(line, AVP_S(" "), -1);
            v = visit_new(t, (u64)num(w.v[1]));
            v->per_function = pending_expand;
            pending_expand = false;
            if (w.n >= 9) {
                v->autoinline = (int)num(w.v[2]);
                v->maxauto = (int)num(w.v[3]);
                v->depth = (int)num(w.v[4]);
                v->bottomup = (int)num(w.v[5]);
                v->always = (int)num(w.v[6]);
                v->dont_inline = (int)num(w.v[7]);
                v->x112 = (int)num(w.v[8]);
            }
            last = NULL;
            last_site = NULL;
            seq = 0;
        } else if (avp_str_eqz(tag, "FILE")) {
            AvpStrVec w = avp_str_split(line, AVP_S(" "), 2);
            AvpStr *p = AVP_NEW(a, AvpStr);
            *p = w.n > 2 ? avp_str_dup(a, w.v[2]) : AVP_S("");
            bool created;
            avp_imap_put(t->files, (u64)num(w.v[1]), &created)->p = p;
        } else if (avp_str_eqz(tag, "STMT")) {
            AvpStrVec w = avp_str_split(line, AVP_S(" "), -1);
            if (w.n < 4) continue;
            cur_off = num(w.v[2]);
            cur_fp = (u64)num(w.v[3]);
        } else if (avp_str_eqz(tag, "CHK")) {
            AvpStrVec w = avp_str_split(line, AVP_S(" "), 9);
            if (w.n < 9) continue;
            u64 e = (u64)num(w.v[1]), cur = (u64)num(w.v[2]);
            if (!v || v->cur != cur) {
                MwtVisit *old = visit_of(t, cur);
                v = old ? old : visit_new(t, cur);
            }
            MwtSite *s = site_of(v, e);
            u64 callee = (u64)num(w.v[3]);
            if (!s || s->expanded || s->callee != callee) { /* an expanded node is gone: a new node at its address */
                MwtSite ns = {0};
                ns.e = e;
                ns.callee = callee;
                ns.callee_name = w.n > 9 ? avp_str_dup(a, w.v[9]) : AVP_S("");
                ns.first_index = v->sites.n;
                ns.first_level = (int)num(w.v[4]);
                ns.offset = cur_off;
                ns.fp = cur_fp;
                ns.via = -1;
                ns.cost = ns.limit = -1;
                avp_vec_push(&v->sites, ns);
                bool created;
                avp_imap_put(v->by_e, e, &created)->u = v->sites.n;
                s = &avp_vec_last(&v->sites);
            }
            s->qual = (u32)num(w.v[5]);
            s->dt = (int)num(w.v[6]);
            s->tflags = (u32)num(w.v[7]);
            s->frflags = (u32)num(w.v[8]);
            MwtCheck c = {(int)num(w.v[4]), AVP_S(""), -1, -1, -1, -1, seq, -1, -1, li};
            avp_vec_push(&s->checks, c);
            last = &avp_vec_last(&s->checks);
            last_site = s;
            if (c.level > v->max_level) v->max_level = c.level;
            seq++;
            t->nchecks++;
        } else if (avp_str_eqz(tag, "FD")) {
            AvpStrVec w = avp_str_split(line, AVP_S(" "), -1);
            if (!last || w.n < 3) continue;
            last->cost = num(w.v[1]);
            last->kind = (int)num(w.v[2]);
        } else if (avp_str_eqz(tag, "BU")) {
            AvpStrVec w = avp_str_split(line, AVP_S(" "), -1);
            if (!last || w.n < 3) continue;
            last->size = num(w.v[1]);
            last->limit = num(w.v[2]);
        } else if (avp_str_eqz(tag, "R")) {
            AvpStrVec w = avp_str_split(line, AVP_S(" "), -1);
            if (!last || w.n < 2) continue;
            last->rule = avp_str_dup(a, w.v[1]);
        } else if (avp_str_eqz(tag, "XP") || avp_str_eqz(tag, "Q")) {
            AvpStrVec w = avp_str_split(line, AVP_S(" "), -1);
            if (!last || !last_site || w.n < 3 || (u64)num(w.v[1]) != last_site->e) continue;
            if (tag.p[0] == 'Q') {
                last->queued = num(w.v[2]);
            } else {
                last->xp = (int)num(w.v[2]);
                if (last->xp == 1) {
                    last_site->expanded = true;
                    last_site->expanded_level = last->level;
                }
            }
        } else if (avp_str_eqz(tag, "XS")) {
            AvpStrVec w = avp_str_split(line, AVP_S(" "), -1);
            if (!v || w.n < 2) continue;
            MwtSite *s = site_of(v, (u64)num(w.v[1]));
            if (!s || !s->checks.n) continue;
            s->expanded = true;
            s->by_statements = true;
            s->expanded_level = avp_vec_last(&s->checks).level;
        } else if (avp_str_eqz(tag, "LN")) {
            AvpStrVec w = avp_str_split(line, AVP_S(" "), 2);
            if (w.n < 3) continue;
            AvpStr *p = AVP_NEW(a, AvpStr);
            *p = avp_str_dup(a, w.v[2]);
            bool created;
            avp_imap_put(t->links, (u64)num(w.v[1]), &created)->p = p;
        } else if (avp_str_eqz(tag, "REVERT") || avp_str_eqz(tag, "KEEP")) {
            AvpStrVec w = avp_str_split(line, AVP_S(" "), -1);
            MwtVisit *rv = visit_of(t, (u64)num(w.v[1]));
            if (!rv) continue;
            if (tag.p[0] == 'K') {
                rv->kept = true;
            } else if (w.n >= 4) {
                rv->reverted = true;
                rv->old_cost = num(w.v[2]);
                rv->new_cost = num(w.v[3]);
            }
        }
    }
}
