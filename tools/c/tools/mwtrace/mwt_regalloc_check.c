/* mwt_regalloc_check: the two self-checks of a regalloc trace (every register variable against the -sym on object's
 * DWARF; the nonvolatile registers against the registers the code saves) and the comparison of two traces. Pure:
 * the callers read the object and the DWARF. */
#include "mwt_regalloc.h"

#include "avp/difflib.h"

/* ---- saved registers ------------------------------------------------------------------------------------------- */

static int reg_of(AvpStr s, char kind) {
    s = avp_str_strip(s);
    if (s.n < 2 || s.p[0] != kind) return -1;
    int v = 0;
    for (size_t i = 1; i < s.n; i++) {
        if (s.p[i] < '0' || s.p[i] > '9') return -1;
        v = v * 10 + (s.p[i] - '0');
    }
    return v <= 31 ? v : -1;
}

/* "0x1c(r1)" -> 1 */
static int base_of(AvpStr s) {
    i64 o = avp_str_findc(s, '(');
    i64 c = avp_str_findc(s, ')');
    if (o < 0 || c < o) return -1;
    return reg_of(avp_str_slice(s, o + 1, c), 'r');
}

void mwt_ra_saved_regs(const AvpStr *rows, size_t n, u32 *gpr, u32 *fpr) {
    *gpr = *fpr = 0;
    u32 frame = 0; /* a nonvolatile register the code sets from r1 (`mr r31, r1`): the frame pointer, no allocator's */
    for (size_t i = 0; i < n; i++) {
        AvpStr t = avp_str_strip(rows[i]);
        i64 sp = avp_str_findc(t, ' ');
        if (sp <= 0) continue;
        AvpStr op = avp_str_slice(t, 0, sp);
        AvpStrVec ops = avp_str_split(avp_str_slice(t, sp + 1, (i64)t.n), AVP_S(","), -1);
        if (avp_str_eqz(op, "mr") && ops.n == 2 && reg_of(ops.v[1], 'r') == 1) {
            int r = reg_of(ops.v[0], 'r');
            if (r >= 14) frame |= 1u << r;
            continue;
        }
        if (avp_str_eqz(op, "bl") && ops.n >= 1) {
            AvpStr tg = avp_str_strip(ops.v[0]);
            bool g = avp_str_startswith(tg, AVP_S("_savegpr_")), f = avp_str_startswith(tg, AVP_S("_savefpr_"));
            if (g || f) {
                i64 r = 0;
                bool ovf = false;
                if (avp_parse_int(avp_str_slice(tg, 9, (i64)tg.n), 10, &r, &ovf) && r >= 14 && r <= 31)
                    for (i64 k = r; k <= 31; k++) *(g ? gpr : fpr) |= 1u << k;
            }
            continue;
        }
        if (ops.n < 2) continue;
        int base = base_of(ops.v[1]);
        if (base != 1 && base != 11) continue;
        if (avp_str_eqz(op, "stw") || avp_str_eqz(op, "stmw")) {
            int r = reg_of(ops.v[0], 'r');
            if (r < 14) continue;
            for (int k = r; k <= (avp_str_eqz(op, "stmw") ? 31 : r); k++) *gpr |= 1u << k;
        } else if (avp_str_eqz(op, "stfd") || avp_str_eqz(op, "psq_st")) {
            int r = reg_of(ops.v[0], 'f');
            if (r >= 14) *fpr |= 1u << r;
        }
    }
    *gpr &= ~frame;
}

/* ---- the self-checks ------------------------------------------------------------------------------------------- */

static void gap(AvpArena *a, MwtRaCheck *c, const char *what, const char *why) {
    MwtRaGap g = {avp_str(what), avp_str(why)};
    (void)a;
    avp_vec_push(&c->gaps, g);
}

static const MwtRaNode *pass_node(const MwtRaPass *p, int idx) {
    if (!p || idx < p->nreal || idx >= p->used) return NULL;
    return &p->nodes.v[idx - p->nreal];
}

const char *mwt_ra_mask(AvpArena *a, u32 m, int cls) {
    const char *kind = cls == MWT_RC_GPR ? "r" : cls == MWT_RC_FPR ? "f" : cls == MWT_RC_CR ? "cr" : cls == MWT_RC_VR ? "v" : "spr";
    AvpBuf b;
    avp_buf_init(&b, a);
    for (int r = 0; r < 32; r++) {
        if (!(m & (1u << r))) continue;
        int e = r;
        while (e + 1 < 32 && (m & (1u << (e + 1)))) e++;
        if (b.n) avp_buf_putc(&b, ' ');
        if (e > r) avp_buf_printf(&b, "%s%d-%s%d", kind, r, kind, e);
        else avp_buf_printf(&b, "%s%d", kind, r);
        r = e;
    }
    return b.n ? avp_cz(a, avp_buf_str(&b)) : "none";
}

/* A DWARF location as the dump prints it: a register ("r31", "f30"), a base register and offset ("r1+0x8": the
 * stack), or neither (the dump's "[invalid]": no readable location). */
static bool loc_is_reg(AvpStr loc) {
    if (loc.n < 2 || (loc.p[0] != 'r' && loc.p[0] != 'f')) return false;
    for (size_t i = 1; i < loc.n; i++)
        if (loc.p[i] < '0' || loc.p[i] > '9') return false;
    return true;
}
static bool loc_is_stack(AvpStr loc) {
    i64 plus = avp_str_findc(loc, '+');
    return plus > 0 && loc_is_reg(avp_str_slice(loc, 0, plus));
}

void mwt_ra_check(AvpArena *a, const MwtRaFunc *f, const MwtDwVars *dw, bool have_dwarf, u32 saved_gpr, u32 saved_fpr,
                  bool have_saved, MwtRaCheck *c) {
    memset(c, 0, sizeof *c);
    static const int CLS[2] = {MWT_RC_GPR, MWT_RC_FPR};
    /* (a) the register variables of the GPR and FPR classes */
    AvpSet *done = avp_set_new();
    AvpSet *memory = avp_set_new();
    for (int ci = 0; ci < 2; ci++) {
        const MwtRaPass *p = mwt_ra_final(f, CLS[ci]);
        if (!p) continue;
        for (size_t i = 0; i < p->vars.n; i++) {
            const MwtRaVar *v = &p->vars.v[i];
            if (!(v->vflags & 2)) { /* in memory: listed once, whatever its class byte */
                if (!avp_set_has(memory, v->name)) {
                    avp_set_add(memory, v->name);
                    avp_vec_push(&c->uncovered, avp_str(avp_sprintf(a, "%.*s: not in a register", AVP_SF(v->name))));
                }
                continue;
            }
            if (v->vclass != p->cls) continue;
            if (v->vflags & 4) {
                avp_vec_push(&c->uncovered, avp_str(avp_sprintf(a, "%.*s: a register pair", AVP_SF(v->name))));
                continue;
            }
            if (avp_set_has(done, v->name)) continue;
            avp_set_add(done, v->name);
            /* every trace variable of that name in this class, against every DWARF entry of that name */
            AvpStrVec tregs = {0}, dregs = {0};
            int kept = -1; /* a virtual register the rewrite left in VarInfo */
            for (size_t k = 0; k < p->vars.n; k++) {
                const MwtRaVar *u = &p->vars.v[k];
                if (u->vclass != p->cls || !(u->vflags & 2) || (u->vflags & 4) || !avp_str_eq(u->name, v->name)) continue;
                const MwtRaNode *un = pass_node(p, u->reg);
                if (un && !un->rewritten && !un->spilled) { /* not rewritten: VarInfo keeps the virtual register, the DWARF register number */
                    kept = u->reg;
                    avp_vec_push(&tregs, avp_str(u->reg < 32 ? avp_sprintf(a, "r%d", u->reg)
                                                  : u->reg < 64 ? avp_sprintf(a, "f%d", u->reg - 32)
                                                                : avp_sprintf(a, "dwarf register %d", u->reg)));
                    continue;
                }
                const MwtRaNode *n = pass_node(p, u->reg);
                int r = n ? n->final_reg : u->reg < p->nreal ? u->reg : -1;
                avp_vec_push(&tregs, avp_str(n && n->spilled ? "spilled" : mwt_ra_reg(a, p->cls, r)));
            }
            if (!have_dwarf) {
                avp_vec_push(&c->uncovered, avp_str(avp_sprintf(a, "%.*s: no DWARF", AVP_SF(v->name))));
                continue;
            }
            /* the DWARF entries of that name: a register location is compared; a stack location belongs to a variable
             * of the same name the trace has in memory (another scope); a location the dump could not read
             * ("[invalid]") cannot be compared */
            size_t in_stack = 0;
            AvpStrVec unread = {0};
            for (size_t k = 0; dw && k < dw->n; k++) {
                if (!avp_str_eq(dw->v[k].name, v->name)) continue;
                AvpStr loc = dw->v[k].loc;
                if (loc_is_reg(loc)) avp_vec_push(&dregs, loc);
                else if (loc_is_stack(loc)) in_stack++;
                else avp_vec_push(&unread, loc);
            }
            size_t trace_mem = 0;
            for (size_t k = 0; k < p->vars.n; k++)
                if (!(p->vars.v[k].vflags & 2) && avp_str_eq(p->vars.v[k].name, v->name)) trace_mem++;
            size_t kept_regs = 0; /* a spilled register variable lives on the stack too */
            for (size_t x = 0; x < tregs.n; x++)
                if (avp_str_eqz(tregs.v[x], "spilled")) trace_mem++;
                else tregs.v[kept_regs++] = tregs.v[x];
            size_t spilled = tregs.n - kept_regs;
            tregs.n = kept_regs;
            if (in_stack && in_stack > trace_mem) {
                gap(a, c, "dwarf", avp_sprintf(a, "%.*s: the -sym on DWARF has %zu stack location(s) of that name, the trace "
                                                  "%zu variable(s) of that name in memory",
                                               AVP_SF(v->name), in_stack, trace_mem));
                avp_vec_free(&tregs);
                avp_vec_free(&dregs);
                continue;
            }
            if (unread.n) {
                /* the readable register locations must still be the trace's; the others are not covered */
                bool sub = dregs.n + unread.n == tregs.n;
                bool usedu[64] = {0};
                for (size_t y = 0; sub && y < dregs.n; y++) {
                    bool hit = false;
                    for (size_t x = 0; x < tregs.n && x < 64 && !hit; x++)
                        if (!usedu[x] && avp_str_eq(tregs.v[x], dregs.v[y])) usedu[x] = hit = true;
                    if (!hit) sub = false;
                }
                AvpBuf tb, ub;
                avp_buf_init(&tb, a);
                avp_buf_init(&ub, a);
                for (size_t x = 0; x < tregs.n; x++) avp_buf_printf(&tb, "%s%.*s", x ? "," : "", AVP_SF(tregs.v[x]));
                for (size_t x = 0; x < unread.n; x++) avp_buf_printf(&ub, "%s%.*s", x ? "," : "", AVP_SF(unread.v[x]));
                if (sub)
                    avp_vec_push(&c->uncovered,
                                 avp_str(avp_sprintf(a, "%.*s: the -sym on DWARF has no readable location (%.*s; trace %.*s)",
                                                     AVP_SF(v->name), AVP_SF(avp_buf_str(&ub)), AVP_SF(avp_buf_str(&tb)))));
                else
                    gap(a, c, "dwarf", avp_sprintf(a, "%.*s: the trace gives %.*s, the -sym on DWARF's readable locations "
                                                      "disagree (%zu readable, %zu unreadable: %.*s)",
                                                   AVP_SF(v->name), AVP_SF(avp_buf_str(&tb)), dregs.n, unread.n,
                                                   AVP_SF(avp_buf_str(&ub))));
                avp_vec_free(&tregs);
                avp_vec_free(&dregs);
                avp_vec_free(&unread);
                continue;
            }
            if (!tregs.n && !dregs.n) { /* spilled only */
                if (in_stack)
                    avp_vec_push(&c->checked, avp_str(avp_sprintf(a, "%.*s spilled (%zu), on the stack in the DWARF",
                                                                  AVP_SF(v->name), spilled)));
                else
                    avp_vec_push(&c->uncovered, avp_str(avp_sprintf(a, "%.*s: spilled, not in the DWARF", AVP_SF(v->name))));
                continue;
            }
            if (!dregs.n) {
                const MwtRaNode *vn = pass_node(p, v->reg);
                const char *why = vn && vn->orig.n ? avp_sprintf(a, "an inlined object of %.*s (%.*s there)", AVP_SF(vn->inl_from),
                                                                  AVP_SF(vn->orig))
                                                    : vn && vn->name.n && vn->name.p[0] == '@' ? "made by the compiler, not in the DWARF" : "not in the DWARF";
                avp_vec_push(&c->uncovered, avp_str(avp_sprintf(a, "%.*s: %s (trace %.*s)", AVP_SF(v->name), why,
                                                                AVP_SF(tregs.v[0]))));
                continue;
            }
            /* the same multiset of registers */
            bool same = tregs.n == dregs.n;
            bool used[64] = {0};
            for (size_t x = 0; same && x < tregs.n; x++) {
                bool hit = false;
                for (size_t y = 0; y < dregs.n && y < 64 && !hit; y++)
                    if (!used[y] && avp_str_eq(tregs.v[x], dregs.v[y])) used[y] = hit = true;
                if (!hit) same = false;
            }
            AvpBuf tb, db;
            avp_buf_init(&tb, a);
            avp_buf_init(&db, a);
            for (size_t x = 0; x < tregs.n; x++) avp_buf_printf(&tb, "%s%.*s", x ? "," : "", AVP_SF(tregs.v[x]));
            for (size_t x = 0; x < dregs.n; x++) avp_buf_printf(&db, "%s%.*s", x ? "," : "", AVP_SF(dregs.v[x]));
            if (same) {
                avp_vec_push(&c->checked, avp_str(avp_sprintf(a, "%.*s %.*s%s%s", AVP_SF(v->name), AVP_SF(avp_buf_str(&tb)),
                                                              kept >= 0 ? avp_sprintf(a, " (virtual register %d kept: the rewrite skipped it)", kept) : "",
                                                              in_stack ? avp_sprintf(a, " (and %zu stack location(s) of that name: its namesakes in memory)", in_stack) : "")));
            } else {
                gap(a, c, "dwarf", avp_sprintf(a, "%.*s: the trace gives %.*s, the -sym on DWARF %.*s", AVP_SF(v->name),
                                               AVP_SF(avp_buf_str(&tb)), AVP_SF(avp_buf_str(&db))));
            }
            avp_vec_free(&tregs);
            avp_vec_free(&dregs);
        }
    }
    AvpSet *known = avp_set_new(); /* every variable the trace lists, in any class or none */
    for (size_t pi = 0; pi < f->passes.n; pi++)
        for (size_t i = 0; i < f->passes.v[pi].vars.n; i++) avp_set_add(known, f->passes.v[pi].vars.v[i].name);
    /* DWARF register variables the trace has no variable for */
    for (size_t k = 0; have_dwarf && dw && k < dw->n; k++) {
        AvpStr loc = dw->v[k].loc;
        bool reg = loc.n >= 2 && (loc.p[0] == 'r' || loc.p[0] == 'f') && loc.p[1] >= '0' && loc.p[1] <= '9' &&
                   avp_str_findc(loc, '+') < 0;
        if (!reg || avp_set_has(done, dw->v[k].name)) continue;
        if (avp_set_has(known, dw->v[k].name)) continue; /* listed by the trace as not covered */
        gap(a, c, "dwarf", avp_sprintf(a, "%.*s: the -sym on DWARF has it in %.*s, the trace has no register variable of that "
                                          "name", AVP_SF(dw->v[k].name), AVP_SF(loc)));
    }
    /* (b) the nonvolatile registers the trace assigns against the ones the code saves */
    for (int ci = 0; ci < 2; ci++) {
        const MwtRaPass *p = mwt_ra_final(f, CLS[ci]);
        u32 m = 0;
        for (size_t i = 0; p && i < p->nodes.n; i++) {
            int r = p->nodes.v[i].final_reg;
            if (r >= 14 && r <= 31) m |= 1u << r;
        }
        c->trace_nv[CLS[ci]] = m;
    }
    avp_set_free(done);
    avp_set_free(memory);
    avp_set_free(known);
    c->saved_nv[MWT_RC_GPR] = saved_gpr;
    c->saved_nv[MWT_RC_FPR] = saved_fpr;
    c->prologue_checked = have_saved;
    if (have_saved) {
        for (int ci = 0; ci < 2; ci++) {
            int cls = CLS[ci];
            char k = cls == MWT_RC_GPR ? 'r' : 'f';
            u32 t = c->trace_nv[cls], s = c->saved_nv[cls];
            if (t == s) continue;
            int first = -1;
            for (int r = 31; r >= 14 && first < 0; r--)
                if (((t ^ s) >> r) & 1) first = r;
            gap(a, c, "prologue",
                avp_sprintf(a, "%c%d: %s by the trace, %s by the code (trace %s, saved %s)", k, first,
                            (t >> first) & 1 ? "assigned" : "not assigned", (s >> first) & 1 ? "saved" : "not saved",
                            mwt_ra_mask(a, t, cls), mwt_ra_mask(a, s, cls)));
        }
    }
}

/* ---- two traces ------------------------------------------------------------------------------------------------ */

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

static const char *ord(AvpArena *a, int pos) {
    if (pos < 0) return "not";
    int n = pos + 1;
    const char *suf = (n % 100 >= 11 && n % 100 <= 13) ? "th" : n % 10 == 1 ? "st" : n % 10 == 2 ? "nd" : n % 10 == 3 ? "rd" : "th";
    return avp_sprintf(a, "%d%s", n, suf);
}

static const char *node_facts(AvpArena *a, const MwtRaPass *p, const MwtRaNode *n) {
    if (!n) return "none";
    const char *reg = n->spilled ? "spilled" : mwt_ra_reg(a, p->cls, n->final_reg);
    if (n->alias != n->index)
        return avp_sprintf(a, "node %d %.*s: degree %d, coalesced into %s%d, %s", n->index, AVP_SF(mwt_ra_label(a, n)),
                           n->degree, n->alias < p->nreal ? "physical " : "node ", n->alias, reg);
    const char *push = n->push_pos < 0 ? "not pushed"
                                       : avp_sprintf(a, "pushed %s (%s, degree %d)", ord(a, n->push_pos), how_name(n->push_how),
                                                     n->push_degree);
    return avp_sprintf(a, "node %d %.*s: degree %d, spill cost %lld, %s, selected %s, %s", n->index,
                       AVP_SF(mwt_ra_label(a, n)), n->degree, (long long)n->cost, push, ord(a, n->select_pos), reg);
}

static const char *var_reg(AvpArena *a, const MwtRaPass *p, const MwtRaNode *n) {
    return n->spilled ? "spilled" : mwt_ra_reg(a, p->cls, n->final_reg);
}

/* A node's identity across two traces: a named variable by its key; a temporary or a compiler-made object by the
 * opcode of its first definition and its number of definitions (their numbers and @ names move with an edit). */
static AvpStr identity(AvpArena *a, const MwtRaNode *n) {
    if (n->key.n) return avp_str(avp_sprintf(a, "v:%.*s", AVP_SF(n->key)));
    return avp_str(avp_sprintf(a, "%c:%.*s/%d", n->kind == 't' ? 't' : 'c', AVP_SF(n->def_op), n->ndefs));
}

/* Do two nodes (one per trace) differ in identity or numbers? (the node index itself moves with any added node) */
static bool node_differs(AvpArena *a, const MwtRaNode *x, const MwtRaNode *y) {
    if (!x || !y) return x != y;
    if (x->degree != y->degree || x->cost != y->cost || x->final_reg != y->final_reg || x->push_how != y->push_how ||
        x->push_degree != y->push_degree || x->spilled != y->spilled)
        return true;
    return !avp_str_eq(identity(a, x), identity(a, y));
}

/* The nodes in index order aligned by identity (difflib): the ranges one side adds or changes, then the aligned
 * nodes whose register differs. */
static void node_alignment(AvpArena *a, const MwtRaPass *A, const MwtRaPass *B, AvpStrVec *out) {
    size_t na = A->nodes.n, nb = B->nodes.n;
    AvpStr *ia = AVP_NEWN(a, AvpStr, na ? na : 1), *ib = AVP_NEWN(a, AvpStr, nb ? nb : 1);
    for (size_t i = 0; i < na; i++) ia[i] = identity(a, &A->nodes.v[i]);
    for (size_t i = 0; i < nb; i++) ib[i] = identity(a, &B->nodes.v[i]);
    AvpSeqMatcher *m = avp_seqm_new_str(a, ia, na, ib, nb, &avp_seqm_no_autojunk);
    size_t nops = 0;
    const AvpOpcode *ops = avp_seqm_opcodes(m, &nops);
    int shown = 0;
    AvpStrVec moved = {0};
    for (size_t k = 0; k < nops; k++) {
        const AvpOpcode *o = &ops[k];
        if (o->tag == AVP_OP_EQUAL) {
            for (size_t i = o->i1, j = o->j1; i < o->i2; i++, j++) {
                const MwtRaNode *x = &A->nodes.v[i], *y = &B->nodes.v[j];
                if (x->final_reg == y->final_reg && x->spilled == y->spilled) continue;
                avp_vec_push(&moved, avp_str(avp_sprintf(a, "%.*s %s in A (node %d), %s in B (node %d)",
                                                          AVP_SF(mwt_ra_label(a, x)), var_reg(a, A, x), x->index,
                                                          var_reg(a, B, y), y->index)));
            }
            continue;
        }
        if (shown++ >= 4) continue;
        AvpBuf b;
        avp_buf_init(&b, a);
        const char *cn = cls_name(A->cls);
        if (o->tag == AVP_OP_DELETE) avp_buf_printf(&b, "%s nodes only in A:", cn);
        else if (o->tag == AVP_OP_INSERT) avp_buf_printf(&b, "%s nodes only in B:", cn);
        else avp_buf_printf(&b, "%s nodes of A", cn);
        for (size_t i = o->i1; i < o->i2 && i < o->i1 + 4; i++) avp_buf_printf(&b, " [%s]", node_facts(a, A, &A->nodes.v[i]));
        if (o->i2 - o->i1 > 4) avp_buf_printf(&b, " ... (%zu nodes)", o->i2 - o->i1);
        if (o->tag == AVP_OP_REPLACE) avp_buf_putz(&b, " are in B");
        for (size_t j = o->j1; j < o->j2 && j < o->j1 + 4; j++) avp_buf_printf(&b, " [%s]", node_facts(a, B, &B->nodes.v[j]));
        if (o->j2 - o->j1 > 4) avp_buf_printf(&b, " ... (%zu nodes)", o->j2 - o->j1);
        avp_vec_push(out, avp_buf_str(&b));
    }
    if (shown > 4) avp_vec_push(out, avp_str(avp_sprintf(a, "%s nodes: %d more differing ranges", cls_name(A->cls), shown - 4)));
    if (!shown) avp_vec_push(out, avp_str(avp_sprintf(a, "%s nodes: the same sequence in A and B", cls_name(A->cls))));
    for (size_t i = 0; i < moved.n; i++)
        avp_vec_push(out, avp_str(avp_sprintf(a, "%s register moved: %.*s", cls_name(A->cls), AVP_SF(moved.v[i]))));
    avp_vec_free(&moved);
}

static void first_order_diff(AvpArena *a, const MwtRaPass *A, const MwtRaPass *B, const MwtInts *oa, const MwtInts *ob,
                             const char *what, AvpStrVec *out) {
    size_t n = oa->n > ob->n ? oa->n : ob->n;
    for (size_t i = 0; i < n; i++) {
        const MwtRaNode *x = i < oa->n ? pass_node(A, oa->v[i]) : NULL;
        const MwtRaNode *y = i < ob->n ? pass_node(B, ob->v[i]) : NULL;
        if (!node_differs(a, x, y)) continue;
        avp_vec_push(out, avp_str(avp_sprintf(a, "%s %s %s: A %s; B %s", cls_name(A->cls), what, ord(a, (int)i),
                                              node_facts(a, A, x), node_facts(a, B, y))));
        return;
    }
    avp_vec_push(out, avp_str(avp_sprintf(a, "%s %s: the same in A and B (%zu nodes)", cls_name(A->cls), what, oa->n)));
}

static const MwtRaNode *named(const MwtRaPass *p, AvpStr name) {
    for (size_t i = 0; i < p->nodes.n; i++)
        if (p->nodes.v[i].key.n && avp_str_eq(p->nodes.v[i].key, name))
            return &p->nodes.v[i];
    return NULL;
}

static const char *both(AvpArena *a, const char *what, i64 x, i64 y) {
    if (x == y) return avp_sprintf(a, "%s %lld in both", what, (long long)x);
    return avp_sprintf(a, "%s %lld in A, %lld in B", what, (long long)x, (long long)y);
}

AvpStrVec mwt_ra_diff(AvpArena *a, const MwtRaFunc *FA, const MwtRaFunc *FB) {
    AvpStrVec out = {0};
    static const int ORDER[MWT_RC_N] = {MWT_RC_GPR, MWT_RC_FPR, MWT_RC_CR, MWT_RC_VR, MWT_RC_SPR};
    for (int ci = 0; ci < MWT_RC_N; ci++) {
        int cls = ORDER[ci];
        const MwtRaPass *A = mwt_ra_final(FA, cls), *B = mwt_ra_final(FB, cls);
        if (!A && !B) continue;
        if (!A || !B) {
            avp_vec_push(&out, avp_str(avp_sprintf(a, "%s: allocated only in %s", cls_name(cls), A ? "A" : "B")));
            continue;
        }
        if (A->nodes.n != B->nodes.n)
            avp_vec_push(&out, avp_str(avp_sprintf(a, "%s: %zu virtual nodes in A, %zu in B", cls_name(cls), A->nodes.n,
                                                    B->nodes.n)));
        if (A->iter != B->iter)
            avp_vec_push(&out, avp_str(avp_sprintf(a, "%s: coloured at attempt %d in A, %d in B", cls_name(cls), A->iter,
                                                    B->iter)));
        node_alignment(a, A, B, &out);
        first_order_diff(a, A, B, &A->push, &B->push, "simplify", &out);
        first_order_diff(a, A, B, &A->select, &B->select, "select", &out);
        /* named variables: register, node, positions, numbers */
        AvpSet *seen = avp_set_new();
        for (int side = 0; side < 2; side++) {
            const MwtRaPass *P = side ? B : A;
            for (size_t i = 0; i < P->nodes.n; i++) {
                const MwtRaNode *n = &P->nodes.v[i];
                if (n->kind != 'p' && n->kind != 'l') continue;
                if (!n->key.n) continue; /* compiler-made: no name to match by */
                if (avp_set_has(seen, n->key)) continue;
                avp_set_add(seen, n->key);
                const MwtRaNode *x = named(A, n->key), *y = named(B, n->key);
                if (!x || !y) {
                    avp_vec_push(&out, avp_str(avp_sprintf(a, "%s `%.*s`: only in %s (node %d, %s)", cls_name(cls),
                                                            AVP_SF(n->key), x ? "A" : "B", n->index, var_reg(a, P, n))));
                    continue;
                }
                const char *rx = var_reg(a, A, x), *ry = var_reg(a, B, y);
                if (!strcmp(rx, ry)) continue;
                avp_vec_push(&out, avp_str(avp_sprintf(
                                       a, "`%.*s`: %s in A, %s in B; %s; selected %s; pushed %s; %s; %s", AVP_SF(n->key), rx,
                                       ry, both(a, "node", x->index, y->index),
                                       x->select_pos == y->select_pos
                                           ? avp_sprintf(a, "%s in both", ord(a, x->select_pos))
                                           : avp_sprintf(a, "%s in A, %s in B", ord(a, x->select_pos), ord(a, y->select_pos)),
                                       x->push_pos == y->push_pos
                                           ? avp_sprintf(a, "%s in both (%s)", ord(a, x->push_pos), how_name(x->push_how))
                                           : avp_sprintf(a, "%s (%s) in A, %s (%s) in B", ord(a, x->push_pos),
                                                         how_name(x->push_how), ord(a, y->push_pos), how_name(y->push_how)),
                                       both(a, "degree", x->degree, y->degree), both(a, "spill cost", x->cost, y->cost))));
            }
        }
        avp_set_free(seen);
    }
    return out;
}
