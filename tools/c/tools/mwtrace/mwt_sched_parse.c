/* mwt_sched_parse: the trace log of MWT_SCHED_SPEC into a MwtSchedTrace. The lines (one event each):
 *
 *   compiler: <build> (...)                      the run's compiler build
 *   FN <obj> <on> <name>                         CodeGen_Generator: a function's code generation (on: traced)
 *   LN <obj> <link name>                         a link name
 *   SD <arg> <coloring> <mode> <proc> <x112> <pre> 0 0 0   scheduleinstructions: one pass
 *   BV <block> <count> <flags> <index>           the driver's visit of a block
 *   SB <block> <index> <count> <flags> <first> <width> <m4> <m8>   the block scheduler starts on a block (and
 *                                                the machine model: +0 issue width, +4, +8)
 *   NC <node> <pcode> <op> <latency> <pflags> <pflags2> <prio>   a node made (from the block's last instruction up)
 *   OP <class> <reg> <effect>                    a register operand of the node being made
 *   EA <from> <to> <flag> <site>                 the edge adder asked (site: its return address)
 *   EN <latency>  EU <latency>                   a new edge, an edge's latency raised
 *   MA <ls|sl|ss> <from> <to> <answer>           may_alias's answer for a memory pair
 *   MP <ls|sl|ss> <from> <to> <answer>           the precheck's answer (nonzero: no dependence)
 *   N <node> <height> <slack> <preds> <maxh>     the built nodes, in program order
 *   S <cycle> <slot>                             a selection
 *   CI <node> <answer>  INC <node>  SC <cand> <inc> <cs> <is>  T <node>   its can-issue answers, incumbent,
 *                                                successor counts, takes
 *   I <node>  U <succ> <preds> <cycle+latency> <old earliest>  CY <cycle>  BE   issue, update, cycle end, block end
 *   RW  RI <pcode>                               the register allocator's instruction walk
 *   AS <pcode> <op> <block>                      an emitted instruction
 * plus the debugger's own lines (HANDLER ERR, EXCEPTION, timeout), kept as errors. */
#include "mwt_sched.h"

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

/* Edge sites as offsets of the edge adder's return address from the adder itself (the same in every build: the
 * code moves as one). */
static const struct {
    i64 off;
    MwtDepKind kind;
} SITES[] = {
    {-0xbf, MWT_DEP_RAW},       {-0x89, MWT_DEP_WAW},     {-0x32, MWT_DEP_WAR},       {-0x544, MWT_DEP_MEM_LS},
    {-0x494, MWT_DEP_MEM_SL},   {-0x444, MWT_DEP_MEM_SS}, {-0x670, MWT_DEP_ORDERED},  {-0x607, MWT_DEP_SERIAL},
    {-0x5ce, MWT_DEP_TO_SERIAL}, {-0x5a9, MWT_DEP_BRANCH},
};

static MwtDepKind site_kind(u32 edge_fn, u32 site) {
    for (size_t i = 0; i < sizeof SITES / sizeof SITES[0]; i++)
        if ((i64)site - (i64)edge_fn == SITES[i].off) return SITES[i].kind;
    return MWT_DEP_OTHER;
}

static const char *cls_prefix(int cls) {
    switch (cls) {
    case 4: return "r";
    case 3: return "f";
    case 1: return "cr";
    case 2: return "v";
    default: return "spr";
    }
}

/* the raw block while its events arrive (pointers not yet indices) */
typedef struct {
    u64 from, to;
    int latency;
    u32 kinds;
    AvpStrVec regs;
    u32 site;
} RawEdge;

typedef struct {
    MwtSchedBlock *b;
    AvpIMap *idx;        /* node ptr -> index + 1 (after the N lines) */
    AvpIMap *raw;        /* node ptr -> position in nodes (NC order) + 1, before the N lines */
    AVP_VEC(RawEdge) edges;
    AvpMap *edge_at;     /* "from:to" -> edge position + 1 */
    int cur_edge;        /* the edge the last EA named, -1 */
    AvpStr cur_reg;      /* the last OP's register */
    AVP_VEC(u64) nc_order;
    AVP_VEC(AvpStrVec) opnds; /* per NC node: its operand words */
    int nindexed;
    struct {
        u64 from, to;
        char kind;
        int pre, alias;
    } *mems;
    size_t nmems, capmems;
} Raw;

static int index_of(Raw *r, u64 ptr) {
    AvpVal *v = r->idx ? avp_imap_get(r->idx, ptr) : NULL;
    return v ? (int)v->i - 1 : -1;
}

static void raw_reset(Raw *r) {
    if (r->idx) avp_imap_free(r->idx);
    if (r->raw) avp_imap_free(r->raw);
    if (r->edge_at) avp_map_free(r->edge_at);
    for (size_t i = 0; i < r->edges.n; i++) avp_vec_free(&r->edges.v[i].regs);
    avp_vec_free(&r->edges);
    for (size_t i = 0; i < r->opnds.n; i++) avp_vec_free(&r->opnds.v[i]);
    avp_vec_free(&r->opnds);
    avp_vec_free(&r->nc_order);
    avp_xfree(r->mems);
    memset(r, 0, sizeof *r);
    r->cur_edge = -1;
}

static char mem_kind(AvpStr s) { return avp_str_eqz(s, "ls") ? 'l' : avp_str_eqz(s, "sl") ? 's' : 'S'; }

static void push_mem(Raw *r, u64 from, u64 to, char kind, int pre, int alias) {
    for (size_t i = 0; i < r->nmems; i++)
        if (r->mems[i].from == from && r->mems[i].to == to && r->mems[i].kind == kind) {
            if (pre >= 0) r->mems[i].pre = pre;
            if (alias >= 0) r->mems[i].alias = alias;
            return;
        }
    if (r->nmems == r->capmems) {
        r->capmems = r->capmems ? 2 * r->capmems : 16;
        r->mems = avp_xrealloc(r->mems, r->capmems * sizeof *r->mems);
    }
    r->mems[r->nmems].from = from;
    r->mems[r->nmems].to = to;
    r->mems[r->nmems].kind = kind;
    r->mems[r->nmems].pre = pre;
    r->mems[r->nmems].alias = alias;
    r->nmems++;
}

/* The block is complete (BE, or the next block or pass): pointers to indices, edges into nodes. */
static void finish_block(AvpArena *a, Raw *r, AvpStrVec *errors) {
    MwtSchedBlock *b = r->b;
    if (!b) return;
    for (size_t i = 0; i < r->edges.n; i++) {
        RawEdge *re = &r->edges.v[i];
        int f = index_of(r, re->from), t = index_of(r, re->to);
        if (f < 0 || t < 0) {
            avp_vec_push(errors, avp_str(avp_sprintf(a, "pass %d block %d: an edge names a node the block's node list "
                                                      "does not have (%#llx -> %#llx)",
                                                      b->pass, b->seq, (unsigned long long)re->from,
                                                      (unsigned long long)re->to)));
            continue;
        }
        MwtSchedEdge e = {0};
        e.from = f;
        e.to = t;
        e.latency = re->latency;
        e.kinds = re->kinds;
        e.site = re->site;
        e.regs = avp_str_join(a, AVP_S(" "), re->regs.v, re->regs.n);
        avp_vec_push(&b->edges, e);
        avp_vec_push(&b->nodes.v[f].succ, (int)b->edges.n - 1);
        avp_vec_push(&b->nodes.v[t].pred, (int)b->edges.n - 1);
    }
    for (size_t i = 0; i < r->nmems; i++) {
        MwtSchedMem m = {index_of(r, r->mems[i].from), index_of(r, r->mems[i].to), r->mems[i].kind, r->mems[i].pre,
                         r->mems[i].alias};
        if (m.from >= 0 && m.to >= 0) avp_vec_push(&b->mems, m);
    }
    /* the comparisons' keys from the logged numbers */
    for (size_t s = 0; s < b->sels.n; s++) {
        MwtSchedSel *sel = &b->sels.v[s];
        for (size_t k = 0; k < sel->cmps.n; k++) {
            MwtSchedCmp *c = &sel->cmps.v[k];
            bool wins = false;
            c->key = mwt_sched_compare(b, sel->cycle, c->cand, c->inc, c->cand_succ, c->inc_succ, &wins);
        }
    }
    raw_reset(r);
}

static MwtSchedNode *node_by_ptr(Raw *r, u64 ptr) {
    int i = index_of(r, ptr);
    return i >= 0 ? &r->b->nodes.v[i] : NULL;
}

void mwt_sched_parse(AvpArena *a, AvpStr log, MwtSchedTrace *t) {
    memset(t, 0, sizeof *t);
    t->links = avp_imap_new();
    MwtSchedFunc *f = NULL;
    MwtSchedPass *p = NULL;
    Raw r;
    memset(&r, 0, sizeof r);
    r.cur_edge = -1;
    MwtSchedSel *sel = NULL;
    int cur_inc = -1;     /* the selection's incumbent while its comparisons arrive */
    bool after_inc = false;
    u32 edge_fn = 0x5bc0b0;
    int ra_walks = 0, ra_n = 0;
    AvpStrVec lines = avp_str_splitlines(log, false);
    /* NC events carry nodes in reverse program order; they are kept in a scratch list and placed by the N lines */
    AVP_VEC(MwtSchedNode) made = {0};
    for (size_t li = 0; li < lines.n; li++) {
        AvpStr line = avp_str_rstrip(lines.v[li]);
        if (avp_str_startswith(line, AVP_S("compiler: "))) {
            AvpStrVec w = avp_str_split(line, AVP_S(" "), 2);
            if (w.n >= 2) t->build = avp_str_dup(a, w.v[1]);
            const MwtBuild *bt = mwt_build_find(avp_cz(a, t->build));
            u32 v;
            if (bt && mwt_build_value(bt, "SC_EDGE", &v)) edge_fn = v;
            continue;
        }
        if (avp_str_startswith(line, AVP_S("HANDLER ERR")) || avp_str_startswith(line, AVP_S("EXCEPTION")) ||
            avp_str_eqz(line, "timeout")) {
            avp_vec_push(&t->errors, avp_str_dup(a, line));
            continue;
        }
        AvpStrVec w = avp_str_split(line, AVP_S(" "), -1);
        if (!w.n) continue;
        AvpStr tag = w.v[0];
        if (avp_str_eqz(tag, "FN") && w.n >= 3) {
            finish_block(a, &r, &t->errors);
            p = NULL;
            sel = NULL;
            MwtSchedFunc nf = {0};
            nf.obj = (u64)num(w.v[1]);
            nf.traced = num(w.v[2]) != 0;
            AvpStrVec w3 = avp_str_split(line, AVP_S(" "), 3);
            nf.name = w3.n > 3 ? avp_str_dup(a, w3.v[3]) : AVP_S("");
            nf.ra = avp_imap_new();
            avp_vec_push(&t->funcs, nf);
            f = &avp_vec_last(&t->funcs);
            ra_walks = ra_n = 0;
            continue;
        }
        if (avp_str_eqz(tag, "LN") && w.n >= 3) {
            AvpStrVec w2 = avp_str_split(line, AVP_S(" "), 2);
            AvpStr *s = AVP_NEW(a, AvpStr);
            *s = avp_str_dup(a, w2.v[2]);
            bool created;
            avp_imap_put(t->links, (u64)num(w.v[1]), &created)->p = s;
            continue;
        }
        if (!f) continue;
        if (avp_str_eqz(tag, "SD") && w.n >= 10) {
            finish_block(a, &r, &t->errors);
            sel = NULL;
            MwtSchedPass np = {0};
            np.arg = (int)num(w.v[1]);
            np.pass = np.arg == 0 ? 1 : 2;
            np.coloring = (int)num(w.v[2]);
            np.mode = (int)num(w.v[3]);
            np.proc = (int)num(w.v[4]);
            np.x112 = (int)num(w.v[5]);
            np.pre = (int)num(w.v[6]);
            np.width = (int)num(w.v[7]);
            np.m4 = (int)num(w.v[8]);
            np.m8 = (int)num(w.v[9]);
            avp_vec_push(&f->passes, np);
            p = &avp_vec_last(&f->passes);
        } else if (avp_str_eqz(tag, "RW")) {
            ra_walks++;
        } else if (avp_str_eqz(tag, "RI") && w.n >= 2) {
            if (ra_walks == 1) {
                bool created;
                AvpVal *v = avp_imap_put(f->ra, (u64)num(w.v[1]), &created);
                if (created) v->i = ++ra_n;
            }
        } else if (avp_str_eqz(tag, "AS") && w.n >= 4) {
            MwtSchedEmit e = {(u64)num(w.v[1]), avp_str_dup(a, w.v[2]), (u64)num(w.v[3])};
            avp_vec_push(&f->emitted, e);
        } else if (!p) {
            continue;
        } else if (avp_str_eqz(tag, "BV") && w.n >= 5) {
            MwtSchedVisit v = {(u64)num(w.v[1]), (int)num(w.v[4]), (int)num(w.v[2]), (u32)num(w.v[3]), -1};
            avp_vec_push(&p->visits, v);
        } else if (avp_str_eqz(tag, "SB") && w.n >= 6) {
            finish_block(a, &r, &t->errors);
            sel = NULL;
            MwtSchedBlock nb = {0};
            nb.pass = p->pass;
            nb.seq = (int)p->blocks.n;
            nb.prio_key = p->coloring != 0;
            nb.ptr = (u64)num(w.v[1]);
            nb.index = (int)num(w.v[2]);
            nb.count = (int)num(w.v[3]);
            nb.flags = (u32)num(w.v[4]);
            if (w.n >= 9) { /* the machine model, set by the driver after its entry */
                p->width = (int)num(w.v[6]);
                p->m4 = (int)num(w.v[7]);
                p->m8 = (int)num(w.v[8]);
            }
            avp_vec_push(&p->blocks, nb);
            r.b = &avp_vec_last(&p->blocks);
            r.raw = avp_imap_new();
            r.edge_at = avp_map_new();
            r.cur_edge = -1;
            made.n = 0;
            for (size_t i = p->visits.n; i-- > 0;)
                if (p->visits.v[i].ptr == nb.ptr) {
                    p->visits.v[i].sched = nb.seq;
                    break;
                }
        } else if (!r.b) {
            continue;
        } else if (avp_str_eqz(tag, "NC") && w.n >= 8) {
            MwtSchedNode n = {0};
            n.ptr = (u64)num(w.v[1]);
            n.pcode = (u64)num(w.v[2]);
            n.op = avp_str_dup(a, w.v[3]);
            n.latency = (int)num(w.v[4]);
            n.pflags = (u32)num(w.v[5]);
            n.pflags2 = (u32)num(w.v[6]);
            n.prio = (int)num(w.v[7]);
            n.ra = n.out = n.cycle = n.slot = n.fpos = -1;
            n.opnds = AVP_S("");
            avp_vec_push(&made, n);
            AvpStrVec ow = {0};
            avp_vec_push(&r.opnds, ow);
            bool created;
            avp_imap_put(r.raw, n.ptr, &created)->i = (i64)made.n;
        } else if (avp_str_eqz(tag, "OP") && w.n >= 4) {
            int cls = (int)num(w.v[1]), reg = (int)(i16)(u16)num(w.v[2]), eff = (int)num(w.v[3]);
            r.cur_reg = avp_str(avp_sprintf(a, "%s%d", cls_prefix(cls), reg));
            if (r.opnds.n) {
                const char *mark = (eff & 3) == 3 ? "=+" : (eff & 2) ? "=" : "";
                avp_vec_push(&avp_vec_last(&r.opnds), avp_str(avp_sprintf(a, "%.*s%s", AVP_SF(r.cur_reg), mark)));
            }
        } else if (avp_str_eqz(tag, "EA") && w.n >= 5) {
            u64 from = (u64)num(w.v[1]), to = (u64)num(w.v[2]);
            r.cur_edge = -1;
            if (from == to) continue;
            AvpStr key = avp_str(avp_sprintf(a, "%llx:%llx", (unsigned long long)from, (unsigned long long)to));
            bool created;
            AvpVal *v = avp_map_put(r.edge_at, key, &created);
            if (created) {
                RawEdge e = {0};
                e.from = from;
                e.to = to;
                e.latency = 0;
                avp_vec_push(&r.edges, e);
                v->i = (i64)r.edges.n;
            }
            RawEdge *e = &r.edges.v[v->i - 1];
            u32 site = (u32)num(w.v[4]);
            MwtDepKind k = site_kind(edge_fn, site);
            e->kinds |= 1u << k;
            if (k == MWT_DEP_OTHER) e->site = site;
            if ((k == MWT_DEP_RAW || k == MWT_DEP_WAW || k == MWT_DEP_WAR) && r.cur_reg.n) {
                bool have = false;
                for (size_t i = 0; i < e->regs.n && !have; i++) have = avp_str_eq(e->regs.v[i], r.cur_reg);
                if (!have) avp_vec_push(&e->regs, r.cur_reg);
            }
            r.cur_edge = (int)v->i - 1;
        } else if ((avp_str_eqz(tag, "EN") || avp_str_eqz(tag, "EU")) && w.n >= 2) {
            if (r.cur_edge >= 0) r.edges.v[r.cur_edge].latency = (int)num(w.v[1]);
        } else if ((avp_str_eqz(tag, "MA") || avp_str_eqz(tag, "MP")) && w.n >= 5) {
            bool ma = avp_str_eqz(tag, "MA");
            int ans = (int)num(w.v[4]);
            push_mem(&r, (u64)num(w.v[2]), (u64)num(w.v[3]), mem_kind(w.v[1]), ma ? -1 : ans, ma ? ans : -1);
        } else if (avp_str_eqz(tag, "N") && w.n >= 6) {
            u64 ptr = (u64)num(w.v[1]);
            AvpVal *v = avp_imap_get(r.raw, ptr);
            if (!v) {
                avp_vec_push(&t->errors, avp_str(avp_sprintf(a, "pass %d block %d: node %#llx was not made", r.b->pass,
                                                          r.b->seq, (unsigned long long)ptr)));
                continue;
            }
            MwtSchedNode n = made.v[v->i - 1];
            AvpStrVec *ow = &r.opnds.v[v->i - 1];
            n.opnds = avp_str_join(a, AVP_S(" "), ow->v, ow->n);
            n.index = (int)r.b->nodes.n;
            n.height = (int)num(w.v[2]);
            n.slack = (int)num(w.v[3]);
            n.npreds = (int)num(w.v[4]);
            r.b->maxh = (int)num(w.v[5]);
            avp_vec_push(&r.b->nodes, n);
            if (!r.idx) r.idx = avp_imap_new();
            bool created;
            avp_imap_put(r.idx, ptr, &created)->i = (i64)r.b->nodes.n;
        } else if (avp_str_eqz(tag, "S") && w.n >= 3) {
            MwtSchedSel s = {0};
            s.cycle = (int)num(w.v[1]);
            s.slot = (int)num(w.v[2]);
            s.incumbent = s.chosen = -1;
            avp_vec_push(&r.b->sels, s);
            sel = &avp_vec_last(&r.b->sels);
            cur_inc = -1;
            after_inc = false;
        } else if (!sel) {
            if (avp_str_eqz(tag, "BE")) finish_block(a, &r, &t->errors);
            continue;
        } else if (avp_str_eqz(tag, "CI") && w.n >= 3) {
            int n = index_of(&r, (u64)num(w.v[1]));
            bool ok = num(w.v[2]) != 0;
            if (n < 0) continue;
            avp_vec_push(ok ? &sel->ready : &sel->busy, n);
            if (ok && after_inc) {
                MwtSchedCmp c = {n, cur_inc, -1, -1, false, MWT_KEY_NONE};
                avp_vec_push(&sel->cmps, c);
            }
        } else if (avp_str_eqz(tag, "INC") && w.n >= 2) {
            sel->incumbent = cur_inc = index_of(&r, (u64)num(w.v[1]));
            after_inc = true;
        } else if (avp_str_eqz(tag, "SC") && w.n >= 5) {
            if (sel->cmps.n) {
                MwtSchedCmp *c = &avp_vec_last(&sel->cmps);
                c->cand_succ = (int)num(w.v[3]);
                c->inc_succ = (int)num(w.v[4]);
            }
        } else if (avp_str_eqz(tag, "T") && w.n >= 2) {
            if (sel->cmps.n) {
                MwtSchedCmp *c = &avp_vec_last(&sel->cmps);
                c->taken = true;
                cur_inc = c->cand;
            }
        } else if (avp_str_eqz(tag, "I") && w.n >= 2) {
            MwtSchedNode *n = node_by_ptr(&r, (u64)num(w.v[1]));
            if (!n) continue;
            sel->chosen = n->index;
            n->cycle = sel->cycle;
            n->slot = sel->slot;
            n->out = (int)r.b->order.n;
            avp_vec_push(&r.b->order, n->index);
        } else if (avp_str_eqz(tag, "U") && w.n >= 5) {
            int s = index_of(&r, (u64)num(w.v[1]));
            avp_vec_push(&sel->upd, s);
            avp_vec_push(&sel->upd, (int)num(w.v[2]));
            avp_vec_push(&sel->upd, (int)num(w.v[3]));
        } else if (avp_str_eqz(tag, "CY") && w.n >= 2) {
            r.b->cycles = (int)num(w.v[1]) + 1;
        } else if (avp_str_eqz(tag, "BE")) {
            finish_block(a, &r, &t->errors);
            sel = NULL;
        }
    }
    finish_block(a, &r, &t->errors);
    avp_vec_free(&made);
    avp_vec_free(&lines);
    /* derived: regalloc numbers and emitted positions */
    for (size_t fi = 0; fi < t->funcs.n; fi++) {
        MwtSchedFunc *fn = &t->funcs.v[fi];
        AvpIMap *pos = avp_imap_new();
        for (size_t i = 0; i < fn->emitted.n; i++) {
            bool created;
            AvpVal *v = avp_imap_put(pos, fn->emitted.v[i].pcode, &created);
            if (created) v->i = (i64)i + 1;
        }
        for (size_t pi = 0; pi < fn->passes.n; pi++)
            for (size_t bi = 0; bi < fn->passes.v[pi].blocks.n; bi++) {
                MwtSchedBlock *b = &fn->passes.v[pi].blocks.v[bi];
                for (size_t ni = 0; ni < b->nodes.n; ni++) {
                    MwtSchedNode *n = &b->nodes.v[ni];
                    AvpVal *v = avp_imap_get(fn->ra, n->pcode);
                    n->ra = v ? (int)v->i : -1;
                    AvpVal *q = avp_imap_get(pos, n->pcode);
                    n->fpos = q ? (int)q->i - 1 : -1;
                }
            }
        avp_imap_free(pos);
    }
}

AvpStr mwt_sched_link(const MwtSchedTrace *t, const MwtSchedFunc *f) {
    AvpVal *v = avp_imap_get(t->links, f->obj);
    return v ? *(AvpStr *)v->p : f->name;
}

/* The source-level name of a mangled symbol (as mwt_ra_source_name). */
static AvpStr source_name(AvpStr sym) {
    size_t from = 0;
    if (avp_str_startswith(sym, AVP_S("__"))) from = 2;
    i64 k = avp_str_find(sym, AVP_S("__"), (i64)from, (i64)sym.n);
    if (k <= 0 || (size_t)k + 2 >= sym.n) return sym;
    char c = sym.p[k + 2];
    if ((c >= '0' && c <= '9') || c == 'Q' || c == 'F' || c == 'C') return avp_str_slice(sym, 0, k);
    return sym;
}

const MwtSchedFunc *mwt_sched_find(AvpArena *a, const MwtSchedTrace *t, const char *sym, const char **why) {
    const MwtSchedFunc *hit = NULL;
    int n = 0;
    for (size_t i = 0; i < t->funcs.n; i++) {
        const MwtSchedFunc *f = &t->funcs.v[i];
        if (!f->traced || !f->passes.n) continue;
        if (avp_str_eqz(mwt_sched_link(t, f), sym) || avp_str_eqz(f->name, sym)) {
            if (!hit) hit = f;
            n++;
        }
    }
    if (n == 1) return hit;
    if (!n) {
        AvpStr src = source_name(avp_str(sym));
        for (size_t i = 0; i < t->funcs.n; i++) {
            const MwtSchedFunc *f = &t->funcs.v[i];
            if (f->traced && f->passes.n && !avp_imap_get(t->links, f->obj) && avp_str_eq(f->name, src)) {
                if (!hit) hit = f;
                n++;
            }
        }
        if (n == 1) return hit;
    }
    if (!n) {
        *why = avp_sprintf(a, "%s: no function of that link or source name was scheduled", sym);
        return NULL;
    }
    *why = avp_sprintf(a, "%s: %d functions of that name were scheduled; give the link name", sym, n);
    return NULL;
}

const MwtSchedPass *mwt_sched_pass(const MwtSchedFunc *f, int pass) {
    for (size_t i = 0; i < f->passes.n; i++)
        if (f->passes.v[i].pass == pass) return &f->passes.v[i];
    return NULL;
}

const char *mwt_sched_dep_name(MwtDepKind k) {
    static const char *const N[] = {"raw", "waw", "war", "mem-load-store", "mem-store-load", "mem-store-store",
                                    "ordered", "serial", "to-serial", "branch", "other"};
    return k < MWT_DEP_N ? N[k] : "?";
}

const char *mwt_sched_key_name(MwtSchedKey k) {
    static const char *const N[] = {"none", "critical", "successors", "height", "priority", "order"};
    return k <= MWT_KEY_ORDER ? N[k] : "?";
}

MwtSchedKey mwt_sched_compare(const MwtSchedBlock *b, int cycle, int c, int i, int cs, int is, bool *c_wins) {
    *c_wins = false;
    if (c < 0 || i < 0 || (size_t)c >= b->nodes.n || (size_t)i >= b->nodes.n) return MWT_KEY_NONE;
    const MwtSchedNode *C = &b->nodes.v[c], *I = &b->nodes.v[i];
    bool ic = I->slack <= cycle, cc = C->slack <= cycle;
    if (ic && !cc) return MWT_KEY_CRITICAL;
    if (!ic && cc) {
        *c_wins = true;
        return MWT_KEY_CRITICAL;
    }
    if (cs < 0 || is < 0) return MWT_KEY_NONE;
    if (is > cs) return MWT_KEY_SUCCESSORS;
    if (is < cs) {
        *c_wins = true;
        return MWT_KEY_SUCCESSORS;
    }
    if (I->height > C->height) return MWT_KEY_HEIGHT;
    if (I->height < C->height) {
        *c_wins = true;
        return MWT_KEY_HEIGHT;
    }
    if (!b->prio_key) return MWT_KEY_ORDER;
    if (I->prio < C->prio) return MWT_KEY_PRIORITY;
    if (I->prio > C->prio) {
        *c_wins = true;
        return MWT_KEY_PRIORITY;
    }
    return MWT_KEY_ORDER;
}

const char *mwt_sched_key_words(AvpArena *a, const MwtSchedBlock *b, int cycle, MwtSchedKey k, int win, int lose,
                                int ws, int ls) {
    const MwtSchedNode *W = &b->nodes.v[win], *L = &b->nodes.v[lose];
    switch (k) {
    case MWT_KEY_CRITICAL:
        return avp_sprintf(a, "critical (slack %d <= cycle %d; #%d slack %d)", W->slack, cycle, L->index, L->slack);
    case MWT_KEY_SUCCESSORS: return avp_sprintf(a, "successors %d > %d", ws, ls);
    case MWT_KEY_HEIGHT: return avp_sprintf(a, "height %d > %d", W->height, L->height);
    case MWT_KEY_PRIORITY: return avp_sprintf(a, "priority %d < %d", W->prio, L->prio);
    case MWT_KEY_ORDER:
        return avp_sprintf(a, "program order (#%d before #%d; slack, successors %d, height %d%s equal)", W->index,
                           L->index, ws, W->height, b->prio_key ? ", priority" : "");
    default: return "undecided (the logged numbers are incomplete)";
    }
}

const char *mwt_sched_node_label(AvpArena *a, const MwtSchedNode *n) {
    AvpStrVec w = avp_str_split(n->opnds, AVP_S(" "), -1);
    if (n->opnds.n && w.n > 4) /* a call: the registers it clobbers */
        return avp_sprintf(a, "#%d %.*s (%zu registers)", n->index, AVP_SF(n->op), w.n);
    if (n->opnds.n) return avp_sprintf(a, "#%d %.*s %.*s", n->index, AVP_SF(n->op), AVP_SF(n->opnds));
    return avp_sprintf(a, "#%d %.*s", n->index, AVP_SF(n->op));
}
