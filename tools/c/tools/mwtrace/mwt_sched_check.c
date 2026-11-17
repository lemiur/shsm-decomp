/* mwt_sched_check: the replay of a traced block (the ready sets the trace's own edges give, the selection re-made from
 * the logged numbers), the self-checks (the trace against itself, the emitted instructions against the object, each
 * pass-2 block's output order against the emitted order), the pair query and the comparison of two traces. */
#include "mwt_sched.h"

#include <stdlib.h>

static void gap(AvpArena *a, MwtSchedCheck *c, const char *stage, int pass, int block, int pos, const char *why) {
    MwtSchedGap g = {avp_str(stage), pass, block, pos, avp_str(avp_strdup(a, why))};
    avp_vec_push(&c->gaps, g);
}

/* ---- the replay ---- */
typedef struct {
    int *remaining;      /* unissued predecessors */
    int *earliest;       /* the earliest cycle its issued predecessors allow */
    bool *issued;
} State;

static void state_init(AvpArena *a, const MwtSchedBlock *b, State *s) {
    size_t n = b->nodes.n ? b->nodes.n : 1;
    s->remaining = AVP_NEWN(a, int, n);
    s->earliest = AVP_NEWN(a, int, n);
    s->issued = AVP_NEWN(a, bool, n);
    for (size_t i = 0; i < b->nodes.n; i++) {
        s->remaining[i] = (int)b->nodes.v[i].pred.n;
        s->earliest[i] = 0;
        s->issued[i] = false;
    }
}

static int succ_count(const MwtSchedBlock *b, const State *s, int n) {
    int k = 0;
    const MwtSchedNode *N = &b->nodes.v[n];
    for (size_t i = 0; i < N->succ.n; i++)
        if (s->remaining[b->edges.v[N->succ.v[i]].to] == 1) k++;
    return k;
}

static bool dep_ready(const State *s, int n, int cycle) { return !s->issued[n] && !s->remaining[n] && s->earliest[n] <= cycle; }

static void issue(const MwtSchedBlock *b, State *s, int n, int cycle) {
    s->issued[n] = true;
    const MwtSchedNode *N = &b->nodes.v[n];
    for (size_t i = 0; i < N->succ.n; i++) {
        const MwtSchedEdge *e = &b->edges.v[N->succ.v[i]];
        s->remaining[e->to]--;
        if (cycle + e->latency > s->earliest[e->to]) s->earliest[e->to] = cycle + e->latency;
    }
}

static bool has(const MwtSchedInts *v, int x) {
    for (size_t i = 0; i < v->n; i++)
        if (v->v[i] == x) return true;
    return false;
}

static const char *ints_text(AvpArena *a, const MwtSchedInts *v) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < v->n; i++) avp_buf_printf(&b, "%s#%d", i ? " " : "", v->v[i]);
    if (!v->n) avp_buf_putz(&b, "none");
    return avp_cz(a, avp_buf_str(&b));
}

static void check_block(AvpArena *a, const MwtSchedBlock *b, MwtSchedCheck *c) {
    State s;
    state_init(a, b, &s);
    for (size_t i = 0; i < b->nodes.n; i++)
        if (b->nodes.v[i].npreds != (int)b->nodes.v[i].pred.n) {
            gap(a, c, "trace", b->pass, b->seq, (int)i,
                avp_sprintf(a, "node #%zu has %d predecessors but the trace has %zu edges into it", i,
                            b->nodes.v[i].npreds, b->nodes.v[i].pred.n));
            return;
        }
    if ((int)b->nodes.n != b->count) {
        gap(a, c, "trace", b->pass, b->seq, -1,
            avp_sprintf(a, "the block has %d instructions but the trace made %zu nodes", b->count, b->nodes.n));
        return;
    }
    for (size_t si = 0; si < b->sels.n; si++) {
        const MwtSchedSel *sel = &b->sels.v[si];
        MwtSchedInts want = {0}, got = {0};
        for (size_t n = 0; n < b->nodes.n; n++)
            if (dep_ready(&s, (int)n, sel->cycle)) avp_vec_push(&want, (int)n);
        for (size_t n = 0; n < b->nodes.n; n++)
            if (has(&sel->ready, (int)n) || has(&sel->busy, (int)n)) avp_vec_push(&got, (int)n);
        bool same = want.n == got.n;
        for (size_t i = 0; same && i < want.n; i++) same = want.v[i] == got.v[i];
        if (!same) {
            gap(a, c, "trace", b->pass, b->seq, (int)si,
                avp_sprintf(a, "cycle %d slot %d: the trace's edges make %s ready, the selection looked at %s", sel->cycle,
                            sel->slot, ints_text(a, &want), ints_text(a, &got)));
            avp_vec_free(&want);
            avp_vec_free(&got);
            return;
        }
        avp_vec_free(&want);
        avp_vec_free(&got);
        /* re-make the selection */
        int inc = sel->ready.n ? sel->ready.v[0] : -1;
        if (inc != sel->incumbent) {
            gap(a, c, "trace", b->pass, b->seq, (int)si,
                avp_sprintf(a, "cycle %d slot %d: the incumbent is #%d, the first ready node #%d", sel->cycle, sel->slot,
                            sel->incumbent, inc));
            return;
        }
        if (sel->cmps.n + (inc >= 0 ? 1 : 0) != sel->ready.n) {
            gap(a, c, "trace", b->pass, b->seq, (int)si,
                avp_sprintf(a, "cycle %d slot %d: %zu comparisons for %zu ready nodes", sel->cycle, sel->slot, sel->cmps.n,
                            sel->ready.n));
            return;
        }
        for (size_t k = 0; k < sel->cmps.n; k++) {
            const MwtSchedCmp *m = &sel->cmps.v[k];
            int cs = succ_count(b, &s, m->cand), is = succ_count(b, &s, inc);
            if (m->inc != inc || m->cand != sel->ready.v[k + 1]) {
                gap(a, c, "trace", b->pass, b->seq, (int)si,
                    avp_sprintf(a, "cycle %d slot %d: comparison %zu is #%d against #%d, the replay has #%d against #%d",
                                sel->cycle, sel->slot, k, m->cand, m->inc, sel->ready.v[k + 1], inc));
                return;
            }
            if (m->cand_succ >= 0 && (m->cand_succ != cs || m->inc_succ != is)) {
                gap(a, c, "trace", b->pass, b->seq, (int)si,
                    avp_sprintf(a, "cycle %d slot %d: successor counts %d, %d logged for #%d, #%d; the trace's edges give "
                                   "%d, %d",
                                sel->cycle, sel->slot, m->cand_succ, m->inc_succ, m->cand, m->inc, cs, is));
                return;
            }
            bool wins = false;
            MwtSchedKey key = mwt_sched_compare(b, sel->cycle, m->cand, inc, cs, is, &wins);
            if (wins != m->taken || key != m->key) {
                gap(a, c, "trace", b->pass, b->seq, (int)si,
                    avp_sprintf(a, "cycle %d slot %d: #%d against #%d was %s, the numbers say %s by %s", sel->cycle,
                                sel->slot, m->cand, inc, m->taken ? "taken" : "kept out", wins ? "taken" : "kept out",
                                mwt_sched_key_name(key)));
                return;
            }
            if (wins) inc = m->cand;
        }
        if (inc != sel->chosen) {
            gap(a, c, "trace", b->pass, b->seq, (int)si,
                avp_sprintf(a, "cycle %d slot %d: issued #%d, the selection's comparisons end at #%d", sel->cycle,
                            sel->slot, sel->chosen, inc));
            return;
        }
        if (sel->chosen >= 0) {
            const MwtSchedNode *N = &b->nodes.v[sel->chosen];
            issue(b, &s, sel->chosen, sel->cycle);
            /* the logged successor updates */
            size_t nu = sel->upd.n / 3;
            if (nu != N->succ.n) {
                gap(a, c, "trace", b->pass, b->seq, (int)si,
                    avp_sprintf(a, "cycle %d: #%d has %zu successors, %zu updates were logged", sel->cycle, sel->chosen,
                                N->succ.n, nu));
                return;
            }
            for (size_t u = 0; u < nu; u++) {
                int to = sel->upd.v[3 * u], left = sel->upd.v[3 * u + 1];
                if (to < 0 || (size_t)to >= b->nodes.n || s.remaining[to] != left) {
                    gap(a, c, "trace", b->pass, b->seq, (int)si,
                        avp_sprintf(a, "cycle %d: issuing #%d leaves #%d with %d predecessors, the replay with %d",
                                    sel->cycle, sel->chosen, to, left, to >= 0 && (size_t)to < b->nodes.n ? s.remaining[to] : -1));
                    return;
                }
            }
        }
    }
    for (size_t n = 0; n < b->nodes.n; n++)
        if (!s.issued[n] || b->nodes.v[n].out < 0) {
            gap(a, c, "trace", b->pass, b->seq, (int)n, avp_sprintf(a, "node #%zu was never issued", n));
            return;
        }
    if (b->order.n != b->nodes.n)
        gap(a, c, "trace", b->pass, b->seq, -1,
            avp_sprintf(a, "%zu nodes issued for %zu nodes", b->order.n, b->nodes.n));
}

void mwt_sched_check_trace(AvpArena *a, const MwtSchedFunc *f, MwtSchedCheck *c) {
    if (!f->passes.n) gap(a, c, "trace", -1, -1, -1, "the function was never scheduled");
    for (size_t pi = 0; pi < f->passes.n; pi++)
        for (size_t bi = 0; bi < f->passes.v[pi].blocks.n; bi++) check_block(a, &f->passes.v[pi].blocks.v[bi], c);
}

/* ---- the object ---- */
static AvpStr mnemonic(AvpStr row) {
    size_t i = 0;
    while (i < row.n && (row.p[i] == ' ' || row.p[i] == '\t')) i++;
    size_t j = i;
    while (j < row.n && row.p[j] != ' ' && row.p[j] != '\t') j++;
    AvpStr m = avp_strn(row.p + i, j - i);
    while (m.n && (m.p[m.n - 1] == '+' || m.p[m.n - 1] == '-')) m.n--;
    return m;
}

static bool in_list(AvpStr m, const char *const *list) {
    for (size_t i = 0; list[i]; i++)
        if (avp_str_eqz(m, list[i])) return true;
    return false;
}

bool mwt_sched_op_matches(AvpStr op, AvpStr row) {
    char lo[32];
    size_t n = op.n < sizeof lo - 1 ? op.n : sizeof lo - 1;
    for (size_t i = 0; i < n; i++) lo[i] = (char)((op.p[i] >= 'A' && op.p[i] <= 'Z') ? op.p[i] + 32 : op.p[i]);
    lo[n] = 0;
    AvpStr l = avp_str(lo), m = mnemonic(row);
    if (avp_str_eq(l, m)) return true;
    /* the record form (rlwinm., subf.) is a flag of the instruction, not part of the opcode's name */
    if (l.n && l.p[l.n - 1] == '.') l.n--;
    if (m.n && m.p[m.n - 1] == '.') m.n--;
    if (avp_str_eq(l, m)) return true;
    static const char *const ADDI[] = {"li", "la", "subi", "addi", NULL};
    static const char *const ADDIS[] = {"lis", "subis", "addis", NULL};
    static const char *const RLWINM[] = {"slwi", "srwi", "clrlwi", "clrrwi", "rotlwi", "rotrwi", "extlwi", "extrwi",
                                         "clrlslwi", "rlwinm", NULL};
    static const char *const RLWIMI[] = {"inslwi", "insrwi", "rlwimi", NULL};
    static const char *const RLWNM[] = {"rotlw", "rlwnm", NULL};
    static const char *const OR[] = {"mr", "or", NULL};
    static const char *const NOR[] = {"not", "nor", NULL};
    static const char *const ORI[] = {"nop", "ori", NULL};
    static const char *const SUBF[] = {"sub", "subf", NULL};
    static const char *const SUBFC[] = {"subc", "subfc", NULL};
    static const char *const CRXOR[] = {"crclr", "crxor", NULL};
    static const char *const CREQV[] = {"crset", "creqv", NULL};
    static const char *const CROR[] = {"crmove", "cror", NULL};
    static const char *const CRNOR[] = {"crnot", "crnor", NULL};
    static const char *const MTSPR[] = {"mtlr", "mtctr", "mtxer", "mtspr", "mtgqr", NULL};
    static const char *const MFSPR[] = {"mflr", "mfctr", "mfxer", "mfspr", NULL};
    static const char *const TW[] = {"trap", "tw", "tweq", "twne", "twlt", "twgt", "twle", "twge", NULL};
    static const char *const CMP[] = {"cmpw", "cmp", NULL};
    static const char *const CMPI[] = {"cmpwi", "cmpi", NULL};
    static const char *const CMPL[] = {"cmplw", "cmpl", NULL};
    static const char *const CMPLI[] = {"cmplwi", "cmpli", NULL};
    static const char *const TWI[] = {"twi", "tweqi", "twnei", "twlti", "twgti", "twlei", "twgei", "twllti", "twlgti",
                                      NULL};
    struct {
        const char *op;
        const char *const *list;
    } fam[] = {{"addi", ADDI},   {"li", ADDI},     {"addis", ADDIS}, {"lis", ADDIS},   {"rlwinm", RLWINM},
               {"rlwimi", RLWIMI}, {"rlwnm", RLWNM}, {"or", OR},       {"mr", OR},       {"nor", NOR},
               {"ori", ORI},     {"nop", ORI},     {"subf", SUBF},   {"subfc", SUBFC}, {"crxor", CRXOR},
               {"creqv", CREQV}, {"cror", CROR},   {"crnor", CRNOR}, {"mtspr", MTSPR}, {"mfspr", MFSPR},
               {"tw", TW},       {"twi", TWI},     {"cmp", CMP},     {"cmpi", CMPI},   {"cmpl", CMPL},
               {"cmpli", CMPLI}};
    for (size_t i = 0; i < sizeof fam / sizeof fam[0]; i++)
        if (avp_str_eqz(l, fam[i].op)) return in_list(m, fam[i].list);
    /* conditional branches: the compiler's bc, bt, bf, bclr, bcctr and their link forms print as beq, bnelr,
     * bdnz and the like */
    static const char *const BR[] = {"bc", "bt", "bf", "bclr", "bcctr", "bclrl", "bcctrl", "btlr", "bflr", "btctr",
                                     "bfctr", "bdnz", "bdz", "bcl", NULL};
    if (in_list(l, BR)) return m.n && m.p[0] == 'b';
    return false;
}

/* whether a register name is a whole token of the row's text */
static bool row_has_reg(AvpStr row, AvpStr reg) {
    for (size_t i = 0; i + reg.n <= row.n; i++) {
        if (memcmp(row.p + i, reg.p, reg.n) != 0) continue;
        char before = i ? row.p[i - 1] : ' ', after = i + reg.n < row.n ? row.p[i + reg.n] : ' ';
        bool bok = !((before >= 'a' && before <= 'z') || (before >= '0' && before <= '9'));
        bool aok = !(after >= '0' && after <= '9') && !(after >= 'a' && after <= 'z');
        if (bok && aok) return true;
    }
    return false;
}

void mwt_sched_check_object(AvpArena *a, const MwtSchedFunc *f, const AvpStr *rows, size_t nrows, MwtSchedCheck *c) {
    c->emitted = (int)f->emitted.n;
    c->rows = rows ? (int)nrows : -1;
    if (!rows) {
        gap(a, c, "trace", -1, -1, -1, "the object has no code for the function");
        return;
    }
    if (!f->emitted.n) {
        gap(a, c, "trace", -1, -1, -1, "assemblefunction was not seen to emit the function");
        return;
    }
    size_t n = f->emitted.n < nrows ? f->emitted.n : nrows;
    int bad = -1, nbad = 0;
    AvpBuf others;
    avp_buf_init(&others, a);
    for (size_t i = 0; i < n; i++) {
        if (!mwt_sched_op_matches(f->emitted.v[i].op, rows[i])) {
            if (bad < 0) bad = (int)i;
            else if (nbad < 8)
                avp_buf_printf(&others, "; %zu %.*s \"%.*s\"", i, AVP_SF(f->emitted.v[i].op), AVP_SF(mnemonic(rows[i])));
            nbad++;
            continue;
        }
        c->ops_checked++;
    }
    if (bad >= 0) {
        gap(a, c, "trace", -1, -1, bad,
            avp_sprintf(a, "emitted instruction %d is %.*s, the object's row is \"%.*s\" (%d rows disagree%.*s)", bad,
                        AVP_SF(f->emitted.v[bad].op), AVP_SF(rows[bad]), nbad, AVP_SF(avp_buf_str(&others))));
        return;
    }
    if (f->emitted.n != nrows) {
        gap(a, c, "trace", -1, -1, (int)n,
            avp_sprintf(a, "%zu instructions emitted, the object has %zu rows", f->emitted.n, nrows));
        return;
    }
    const MwtSchedPass *p2 = mwt_sched_pass(f, 2);
    if (!p2) {
        avp_vec_push(&c->notes, AVP_S("no pass 2 ran (schedule mode off): nothing to check against the object"));
        return;
    }
    /* registers of pass 2's nodes, where the row writes them out */
    for (size_t bi = 0; bi < p2->blocks.n; bi++) {
        const MwtSchedBlock *b = &p2->blocks.v[bi];
        for (size_t ni = 0; ni < b->nodes.n; ni++) {
            const MwtSchedNode *N = &b->nodes.v[ni];
            if (N->fpos < 0 || (N->pflags & 9)) continue; /* a branch or call: its implicit registers are not written */
            AvpStrVec regs = avp_str_split(N->opnds, AVP_S(" "), -1);
            if (!regs.n || regs.n > 3 || !N->opnds.n) continue;
            for (size_t k = 0; k < regs.n; k++) {
                AvpStr r = regs.v[k];
                while (r.n && (r.p[r.n - 1] == '=' || r.p[r.n - 1] == '+')) r.n--;
                if (!(avp_str_startswith(r, AVP_S("r")) || avp_str_startswith(r, AVP_S("f")))) continue;
                if (avp_str_startswith(r, AVP_S("r")) && r.n >= 3 && atoi(avp_cz(a, avp_strn(r.p + 1, r.n - 1))) >= 32)
                    continue;
                if (!row_has_reg(rows[N->fpos], r)) {
                    gap(a, c, "trace", 2, b->seq, (int)N->out,
                        avp_sprintf(a, "node #%d %.*s has %.*s, the object's row %d is \"%.*s\"", N->index, AVP_SF(N->op),
                                    AVP_SF(r), N->fpos, AVP_SF(rows[N->fpos])));
                    return;
                }
                c->regs_checked++;
            }
        }
    }
    /* each block's output order against the emitted order */
    AvpISet *mine = avp_iset_new();
    int deleted = 0;
    for (size_t bi = 0; bi < p2->blocks.n; bi++) {
        const MwtSchedBlock *b = &p2->blocks.v[bi];
        int last = -1, lo = -1, hi = -1;
        for (size_t o = 0; o < b->order.n; o++) {
            const MwtSchedNode *N = &b->nodes.v[b->order.v[o]];
            avp_iset_add(mine, N->pcode);
            if (N->fpos < 0) {
                deleted++;
                avp_vec_push(&c->notes, avp_str(avp_sprintf(a, "pass 2 block %d: %s deleted after the scheduler", b->seq,
                                                           mwt_sched_node_label(a, N))));
                continue;
            }
            if (N->fpos < last) {
                gap(a, c, "after-scheduler", 2, b->seq, (int)o,
                    avp_sprintf(a, "output position %zu, %s, is emitted at %d, before the block's previous output at %d",
                                o, mwt_sched_node_label(a, N), N->fpos, last));
                avp_iset_free(mine);
                return;
            }
            last = N->fpos;
            if (lo < 0) lo = N->fpos;
            hi = N->fpos;
        }
        for (int k = lo; lo >= 0 && k <= hi; k++) {
            bool in = false;
            for (size_t o = 0; o < b->order.n && !in; o++) in = b->nodes.v[b->order.v[o]].fpos == k;
            if (!in)
                avp_vec_push(&c->notes, avp_str(avp_sprintf(a, "pass 2 block %d: emitted %d (%.*s) was added inside the "
                                                           "block after the scheduler",
                                                           b->seq, k, AVP_SF(f->emitted.v[k].op))));
        }
    }
    int unsched = 0, added = 0;
    for (size_t i = 0; i < f->emitted.n; i++) {
        if (avp_iset_has(mine, f->emitted.v[i].pcode)) continue;
        bool skipped = false;
        for (size_t v = 0; v < p2->visits.n && !skipped; v++)
            skipped = p2->visits.v[v].ptr == f->emitted.v[i].block && p2->visits.v[v].sched < 0;
        if (skipped) unsched++;
        else added++;
    }
    if (unsched)
        avp_vec_push(&c->notes, avp_str(avp_sprintf(a, "%d emitted instructions are in blocks pass 2 did not schedule (at "
                                                    "most 2 instructions: the driver skips them; their order is the input's)",
                                                    unsched)));
    if (added)
        avp_vec_push(&c->notes, avp_str(avp_sprintf(a, "%d emitted instructions are in no block pass 2 visited: added "
                                                    "after the scheduler",
                                                    added)));
    (void)deleted;
    avp_iset_free(mine);
}

/* ---- the pair query ---- */
static const MwtSchedBlock *find_block(const MwtSchedFunc *f, int pass, int seq) {
    const MwtSchedPass *p = mwt_sched_pass(f, pass);
    if (!p || seq < 0 || (size_t)seq >= p->blocks.n) return NULL;
    return &p->blocks.v[seq];
}

bool mwt_sched_locate(const MwtSchedFunc *f, int pass, int fpos, int *block, int *node) {
    const MwtSchedPass *p = mwt_sched_pass(f, pass);
    if (!p) return false;
    for (size_t bi = 0; bi < p->blocks.n; bi++)
        for (size_t ni = 0; ni < p->blocks.v[bi].nodes.n; ni++)
            if (p->blocks.v[bi].nodes.v[ni].fpos == fpos) {
                *block = (int)bi;
                *node = (int)ni;
                return true;
            }
    return false;
}

static const char *edge_words(AvpArena *a, const MwtSchedEdge *e) {
    AvpBuf b;
    avp_buf_init(&b, a);
    bool first = true;
    for (int k = 0; k < MWT_DEP_N; k++)
        if (e->kinds & (1u << k)) {
            avp_buf_printf(&b, "%s%s", first ? "" : "+", mwt_sched_dep_name((MwtDepKind)k));
            first = false;
        }
    if (e->regs.n) avp_buf_printf(&b, " %.*s", AVP_SF(e->regs));
    avp_buf_printf(&b, ", latency %d", e->latency);
    return avp_cz(a, avp_buf_str(&b));
}

static const char *cmp_words(AvpArena *a, const MwtSchedBlock *b, const MwtSchedSel *sel, int who) {
    for (size_t k = 0; k < sel->cmps.n; k++) {
        const MwtSchedCmp *m = &sel->cmps.v[k];
        if (m->cand != who && !(m->inc == who && m->taken)) continue;
        int win = m->taken ? m->cand : m->inc, lose = m->taken ? m->inc : m->cand;
        int ws = m->taken ? m->cand_succ : m->inc_succ, ls = m->taken ? m->inc_succ : m->cand_succ;
        const char *kw = mwt_sched_key_words(a, b, sel->cycle, m->key, win, lose, ws, ls);
        if (m->cand == who)
            return avp_sprintf(a, "the selection compared #%d with incumbent #%d: %s by %s", who, m->inc,
                               m->taken ? "took it" : "kept the incumbent", kw);
        return avp_sprintf(a, "#%d was the incumbent; #%d displaced it by %s", who, m->cand, kw);
    }
    if (sel->incumbent == who) return avp_sprintf(a, "#%d was the incumbent and stayed", who);
    return "no comparison of it was logged";
}

bool mwt_sched_pair(AvpArena *a, const MwtSchedFunc *f, int pass, int block, int na, int nb, MwtSchedPair *out,
                    const char **why) {
    memset(out, 0, sizeof *out);
    const MwtSchedBlock *b = find_block(f, pass, block);
    if (!b) {
        *why = avp_sprintf(a, "pass %d has no block %d", pass, block);
        return false;
    }
    if (na < 0 || nb < 0 || (size_t)na >= b->nodes.n || (size_t)nb >= b->nodes.n || na == nb) {
        *why = avp_sprintf(a, "pass %d block %d has nodes #0..#%zu; #%d and #%d are not two of them", pass, block,
                           b->nodes.n ? b->nodes.n - 1 : 0, na, nb);
        return false;
    }
    const MwtSchedNode *A = &b->nodes.v[na], *B = &b->nodes.v[nb];
    if (A->out < 0 || B->out < 0) {
        *why = avp_sprintf(a, "#%d or #%d was never issued", na, nb);
        return false;
    }
    out->pass = pass;
    out->block = block;
    out->a = na;
    out->b = nb;
    out->first = A->out < B->out ? na : nb;
    out->second = A->out < B->out ? nb : na;
    State s;
    state_init(a, b, &s);
    size_t si = 0;
    for (; si < b->sels.n; si++) {
        const MwtSchedSel *sel = &b->sels.v[si];
        if (sel->chosen == out->first) break;
        if (sel->chosen >= 0) issue(b, &s, sel->chosen, sel->cycle);
    }
    if (si == b->sels.n) {
        *why = avp_sprintf(a, "no selection issued #%d", out->first);
        return false;
    }
    const MwtSchedSel *sel = &b->sels.v[si];
    out->sel = (int)si;
    out->cycle = sel->cycle;
    out->slot = sel->slot;
    int x = out->first, y = out->second;
    out->key_words = AVP_S("");
    out->cmp_words = AVP_S("");
    out->held = AVP_S("");
    if (has(&sel->ready, y)) {
        out->both_ready = true;
        int xs = succ_count(b, &s, x), ys = succ_count(b, &s, y);
        bool cw = false;
        MwtSchedKey k;
        const char *words;
        if (x < y) {
            k = mwt_sched_compare(b, sel->cycle, y, x, ys, xs, &cw);
            words = cw ? avp_sprintf(a, "%s (pairwise #%d would win: the selection's comparisons are not transitive)",
                                     mwt_sched_key_words(a, b, sel->cycle, k, y, x, ys, xs), y)
                       : mwt_sched_key_words(a, b, sel->cycle, k, x, y, xs, ys);
        } else {
            k = mwt_sched_compare(b, sel->cycle, x, y, xs, ys, &cw);
            words = cw ? mwt_sched_key_words(a, b, sel->cycle, k, x, y, xs, ys)
                       : avp_sprintf(a, "%s (pairwise #%d would win: the selection's comparisons are not transitive)",
                                     mwt_sched_key_words(a, b, sel->cycle, k, y, x, ys, xs), y);
        }
        out->key = k;
        out->key_words = avp_str(words);
        out->cmp_words = avp_str(cmp_words(a, b, sel, y));
        return true;
    }
    out->key = MWT_KEY_NONE;
    if (has(&sel->busy, y)) {
        out->held = avp_str(avp_sprintf(a, "#%d was ready by its dependences, but the machine model's can-issue answered 0",
                                        y));
        return true;
    }
    AvpBuf hb;
    avp_buf_init(&hb, a);
    const MwtSchedNode *Y = &b->nodes.v[y];
    int nheld = 0;
    for (size_t i = 0; i < Y->pred.n; i++) {
        const MwtSchedEdge *e = &b->edges.v[Y->pred.v[i]];
        if (s.issued[e->from]) continue;
        avp_buf_printf(&hb, "%s#%d waits for %s (edge %s; not issued yet)", nheld ? "; " : "", y,
                       mwt_sched_node_label(a, &b->nodes.v[e->from]), edge_words(a, e));
        nheld++;
    }
    if (!nheld && s.earliest[y] > sel->cycle) {
        for (size_t i = 0; i < Y->pred.n; i++) {
            const MwtSchedEdge *e = &b->edges.v[Y->pred.v[i]];
            int ic = b->nodes.v[e->from].cycle;
            if (ic + e->latency == s.earliest[y])
                avp_buf_printf(&hb, "%s#%d's earliest cycle is %d: %s issued at cycle %d, edge %s", nheld++ ? "; " : "", y,
                               s.earliest[y], mwt_sched_node_label(a, &b->nodes.v[e->from]), ic, edge_words(a, e));
        }
    }
    if (!nheld) avp_buf_printf(&hb, "#%d is not ready and the replay finds no reason (trace incomplete)", y);
    out->held = avp_buf_str(&hb);
    return true;
}

AvpStr mwt_sched_pair_text(AvpArena *a, const MwtSchedFunc *f, const MwtSchedPair *p) {
    const MwtSchedBlock *b = find_block(f, p->pass, p->block);
    AvpBuf o;
    avp_buf_init(&o, a);
    if (!b) return AVP_S("");
    avp_buf_printf(&o, "pass %d block %d (index %d): %s issued at cycle %d slot %d, before %s\n", p->pass, p->block, b->index,
                   mwt_sched_node_label(a, &b->nodes.v[p->first]), p->cycle, p->slot,
                   mwt_sched_node_label(a, &b->nodes.v[p->second]));
    if (p->both_ready) {
        avp_buf_printf(&o, "  #%d was ready: decided by %s\n", p->second, avp_cz(a, p->key_words));
        avp_buf_printf(&o, "  %.*s\n", AVP_SF(p->cmp_words));
    } else {
        avp_buf_printf(&o, "  #%d was not ready: %.*s\n", p->second, AVP_SF(p->held));
    }
    return avp_buf_str(&o);
}

AvpJson *mwt_sched_pair_json(AvpArena *a, const MwtSchedFunc *f, const MwtSchedPair *p) {
    (void)f;
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("pass"), avp_json_int(a, p->pass));
    avp_json_set(a, o, AVP_S("block"), avp_json_int(a, p->block));
    avp_json_set(a, o, AVP_S("first"), avp_json_int(a, p->first));
    avp_json_set(a, o, AVP_S("second"), avp_json_int(a, p->second));
    avp_json_set(a, o, AVP_S("cycle"), avp_json_int(a, p->cycle));
    avp_json_set(a, o, AVP_S("slot"), avp_json_int(a, p->slot));
    avp_json_set(a, o, AVP_S("both_ready"), avp_json_bool(p->both_ready));
    avp_json_set(a, o, AVP_S("key"), avp_json_strz(a, mwt_sched_key_name(p->key)));
    avp_json_set(a, o, AVP_S("key_words"), avp_json_str(a, p->key_words));
    avp_json_set(a, o, AVP_S("comparison"), avp_json_str(a, p->cmp_words));
    avp_json_set(a, o, AVP_S("held"), avp_json_str(a, p->held));
    return o;
}

/* ---- comparison of two traces ---- */
/* a node's identity across two texts: its opcode and its occurrence among the block's nodes of that opcode */
static AvpStr node_key(AvpArena *a, const MwtSchedBlock *b, int n) {
    int occ = 0;
    for (int i = 0; i < n; i++)
        if (avp_str_eq(b->nodes.v[i].op, b->nodes.v[n].op)) occ++;
    return avp_str(avp_sprintf(a, "%.*s/%d", AVP_SF(b->nodes.v[n].op), occ));
}

static int node_of_key(AvpArena *a, const MwtSchedBlock *b, AvpStr key) {
    for (size_t i = 0; i < b->nodes.n; i++)
        if (avp_str_eq(node_key(a, b, (int)i), key)) return (int)i;
    return -1;
}

static const char *decision_words(AvpArena *a, const MwtSchedBlock *b, const MwtSchedSel *sel) {
    AvpBuf o;
    avp_buf_init(&o, a);
    if (sel->chosen < 0) avp_buf_printf(&o, "issued nothing (the cycle ends)");
    else avp_buf_printf(&o, "issued %s", mwt_sched_node_label(a, &b->nodes.v[sel->chosen]));
    avp_buf_printf(&o, "; ready: %s", ints_text(a, &sel->ready));
    if (sel->busy.n) avp_buf_printf(&o, "; can-issue 0: %s", ints_text(a, &sel->busy));
    for (size_t i = 0; i < sel->ready.n; i++) {
        int y = sel->ready.v[i];
        if (y == sel->chosen) continue;
        avp_buf_printf(&o, "; #%d: %s", y, cmp_words(a, b, sel, y));
    }
    return avp_cz(a, avp_buf_str(&o));
}

AvpStrVec mwt_sched_diff(AvpArena *a, const MwtSchedFunc *A, const MwtSchedFunc *B) {
    AvpStrVec out = {0};
    for (int pass = 1; pass <= 2; pass++) {
        const MwtSchedPass *pa = mwt_sched_pass(A, pass), *pb = mwt_sched_pass(B, pass);
        if (!pa || !pb) {
            if (pa || pb)
                avp_vec_push(&out, avp_str(avp_sprintf(a, "pass %d ran only in %s", pass, pa ? "A" : "B")));
            continue;
        }
        size_t nb = pa->blocks.n < pb->blocks.n ? pa->blocks.n : pb->blocks.n;
        if (pa->blocks.n != pb->blocks.n)
            avp_vec_push(&out, avp_str(avp_sprintf(a, "pass %d scheduled %zu blocks in A, %zu in B", pass, pa->blocks.n,
                                                  pb->blocks.n)));
        for (size_t bi = 0; bi < nb; bi++) {
            const MwtSchedBlock *ba = &pa->blocks.v[bi], *bb = &pb->blocks.v[bi];
            bool sets_differ = false;
            for (size_t i = 0; i < ba->nodes.n; i++) {
                AvpStr k = node_key(a, ba, (int)i);
                if (node_of_key(a, bb, k) < 0) {
                    avp_vec_push(&out, avp_str(avp_sprintf(a, "pass %d block %zu: node only in A: %s", pass, bi,
                                                          mwt_sched_node_label(a, &ba->nodes.v[i]))));
                    sets_differ = true;
                }
            }
            for (size_t i = 0; i < bb->nodes.n; i++) {
                AvpStr k = node_key(a, bb, (int)i);
                if (node_of_key(a, ba, k) < 0) {
                    avp_vec_push(&out, avp_str(avp_sprintf(a, "pass %d block %zu: node only in B: %s", pass, bi,
                                                          mwt_sched_node_label(a, &bb->nodes.v[i]))));
                    sets_differ = true;
                }
            }
            if (!sets_differ) {
                bool same_in = ba->nodes.n == bb->nodes.n;
                for (size_t i = 0; same_in && i < ba->nodes.n; i++) same_in = avp_str_eq(ba->nodes.v[i].op, bb->nodes.v[i].op);
                if (!same_in) {
                    AvpBuf ia, ib;
                    avp_buf_init(&ia, a);
                    avp_buf_init(&ib, a);
                    for (size_t i = 0; i < ba->nodes.n; i++) avp_buf_printf(&ia, " %.*s", AVP_SF(ba->nodes.v[i].op));
                    for (size_t i = 0; i < bb->nodes.n; i++) avp_buf_printf(&ib, " %.*s", AVP_SF(bb->nodes.v[i].op));
                    avp_vec_push(&out, avp_str(avp_sprintf(a, "pass %d block %zu: the input orders differ (A:%.*s; B:%.*s)",
                                                          pass, bi, AVP_SF(avp_buf_str(&ia)), AVP_SF(avp_buf_str(&ib)))));
                }
            }
            size_t ns = ba->sels.n < bb->sels.n ? ba->sels.n : bb->sels.n;
            for (size_t si = 0; si < ns; si++) {
                const MwtSchedSel *sa = &ba->sels.v[si], *sb = &bb->sels.v[si];
                AvpStr ka = sa->chosen >= 0 ? node_key(a, ba, sa->chosen) : AVP_S("-");
                AvpStr kb = sb->chosen >= 0 ? node_key(a, bb, sb->chosen) : AVP_S("-");
                if (avp_str_eq(ka, kb) && sa->cycle == sb->cycle && sa->slot == sb->slot) continue;
                avp_vec_push(&out, avp_str(avp_sprintf(a, "first difference: pass %d block %zu (index %d / %d), selection "
                                                          "%zu, cycle %d slot %d / cycle %d slot %d%s",
                                                          pass, bi, ba->index, bb->index, si, sa->cycle, sa->slot,
                                                          sb->cycle, sb->slot,
                                                          sets_differ ? " (the block's node sets differ)"
                                                                      : " (the block's node sets are the same)")));
                avp_vec_push(&out, avp_str(avp_sprintf(a, "A: %s", decision_words(a, ba, sa))));
                avp_vec_push(&out, avp_str(avp_sprintf(a, "B: %s", decision_words(a, bb, sb))));
                /* each side's choice against the other's, by this side's numbers */
                if (sa->chosen >= 0 && sb->chosen >= 0) {
                    int oa = node_of_key(a, ba, kb), ob = node_of_key(a, bb, ka);
                    if (oa >= 0)
                        avp_vec_push(&out, avp_str(avp_sprintf(a, "A on B's choice #%d: %s", oa,
                                                              has(&sa->ready, oa) ? cmp_words(a, ba, sa, oa)
                                                                                  : "not ready in A")));
                    if (ob >= 0)
                        avp_vec_push(&out, avp_str(avp_sprintf(a, "B on A's choice #%d: %s", ob,
                                                              has(&sb->ready, ob) ? cmp_words(a, bb, sb, ob)
                                                                                  : "not ready in B")));
                }
                return out;
            }
            if (ba->sels.n != bb->sels.n) {
                avp_vec_push(&out, avp_str(avp_sprintf(a, "first difference: pass %d block %zu: %zu selections in A, %zu "
                                                          "in B",
                                                          pass, bi, ba->sels.n, bb->sels.n)));
                return out;
            }
        }
    }
    if (!out.n) avp_vec_push(&out, AVP_S("no selection differs: both texts schedule every block the same way"));
    return out;
}
