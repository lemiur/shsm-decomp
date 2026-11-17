/* mwt_regalloc_parse: the trace log of MWT_REGALLOC_SPEC into a MwtRaTrace. The lines (one event each):
 *
 *   compiler: <build> (...)                      the run's compiler build
 *   FN <obj> <on> <name>                         CodeGen_Generator: a function's code generation (on: traced)
 *   LN <obj> <link name>                         a link name
 *   IG <class> <attempt> <nreal> <used>          buildinterferencegraph: one class, one colouring attempt
 *   CX <a> <b> interferes|restriction|set        a copy's two roots not coalesced, and why
 *   CO <lo> <hi>                                 a copy's two roots coalesced: alias[hi] = lo
 *   ARG|LOC <obj> <vflags> <vclass> <reg> <reg2> <name>   a parameter or local and its VarInfo
 *   INH <root> <obj>                             a coalesced node's Object copied to its root
 *   DEF <reg> <instruction> <opcode>             a definition of a virtual register (instructions counted per walk)
 *   N <index> <alias> <flags> <degree> <colour> <obj> <degree copy> <remat>   a virtual node of the complete graph
 *   K <class> <K> <vle>                          simplify: the degree bound
 *   SW  P1 <i> <deg>  EST  CAND <i> <cost> <deg> <flags>  OPT <i> <cost> <deg>  P2 <i> <deg>   simplify's sweeps,
 *                                                pushes, spill-cost estimate, optimistic candidates and choice
 *   VOL <mask>  AVAIL <mask>                     select's start mask and the class's allocatable mask
 *   SEL <i> <flags> <cost>  E <i> <phys>  FREE <mask>  PICK <colour>  NV <reg>  SPILL <i>  SELR <ok>   select
 *   RWV <i>                                      the operand rewrite gives node i's variable its colour
 * plus the debugger's own lines (HANDLER ERR, EXCEPTION, timeout), kept as errors. */
#include "mwt_regalloc.h"

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

static i16 s16(i64 v) { return (i16)(u16)v; }

static MwtRaNode *node_of(MwtRaPass *p, i64 idx) {
    if (!p || idx < p->nreal || idx >= p->used) return NULL;
    return &p->nodes.v[idx - p->nreal];
}

static void push_unique(MwtInts *v, int x) {
    for (size_t i = 0; i < v->n; i++)
        if (v->v[i] == x) return;
    avp_vec_push(v, x);
}

static int root_of(const MwtRaPass *p, int idx) {
    for (int guard = 0; guard < 100000; guard++) {
        if (idx < p->nreal || idx >= p->used) return idx;
        int up = p->nodes.v[idx - p->nreal].alias;
        if (up == idx) return idx;
        idx = up;
    }
    return idx;
}

static const MwtRaVar *var_of(const MwtRaPass *p, u64 obj) {
    for (size_t i = 0; i < p->vars.n; i++)
        if (p->vars.v[i].obj == obj) return &p->vars.v[i];
    return NULL;
}

static void finish_pass(AvpArena *a, MwtRaPass *p) {
    for (size_t i = 0; i < p->nodes.n; i++) {
        MwtRaNode *n = &p->nodes.v[i];
        int r = root_of(p, n->index);
        if (r != n->index) {
            MwtRaNode *rn = node_of(p, r);
            if (rn) avp_vec_push(&rn->members, n->index);
        }
        const MwtRaVar *v = n->obj ? var_of(p, n->obj) : NULL;
        n->kind = n->inherited || !n->obj ? 't' : !v ? 'o' : v->is_param ? 'p' : 'l';
    }
    /* the keys two traces are matched by: an inlined object by its callee and name (its @ number moves with every
     * edit); a repeated key gets its occurrence among the nodes */
    AvpMap *seen = avp_map_new();
    for (size_t i = 0; i < p->nodes.n; i++) {
        MwtRaNode *n = &p->nodes.v[i];
        if (n->kind != 'p' && n->kind != 'l') continue;
        if (!n->orig.n && n->name.n && n->name.p[0] == '@') continue; /* made by the compiler: no source name */
        AvpStr base = n->orig.n ? avp_str(avp_sprintf(a, "%.*s:%.*s", AVP_SF(n->inl_from), AVP_SF(n->orig))) : n->name;
        bool created;
        AvpVal *c = avp_map_put(seen, base, &created);
        c->i++;
        n->key = c->i > 1 ? avp_str(avp_sprintf(a, "%.*s#%lld", AVP_SF(base), (long long)c->i)) : base;
    }
    avp_map_free(seen);
    for (size_t i = 0; i < p->nodes.n; i++) {
        MwtRaNode *n = &p->nodes.v[i];
        int r = root_of(p, n->index);
        if (r < p->nreal) n->final_reg = r;
        else {
            const MwtRaNode *rn = node_of(p, r);
            n->final_reg = rn && !rn->spilled ? rn->colour : -1;
        }
    }
    for (size_t i = 0; i < p->push.n; i++) {
        MwtRaNode *n = node_of(p, p->push.v[i]);
        if (n) n->push_pos = (int)i;
    }
    for (size_t i = 0; i < p->select.n; i++) {
        MwtRaNode *n = node_of(p, p->select.v[i]);
        if (n) n->select_pos = (int)i;
    }
}

static void pass_init(MwtRaPass *p, int cls, int iter, int nreal, int used) {
    memset(p, 0, sizeof *p);
    p->cls = cls;
    p->iter = iter;
    p->nreal = nreal;
    p->used = used;
    p->K = -1;
    p->ok = -1;
    for (int i = nreal; i < used; i++) {
        MwtRaNode n = {0};
        n.index = i;
        n.alias = i;
        n.colour = -1;
        n.def_at = -1;
        n.push_pos = n.select_pos = -1;
        n.nv_added = -1;
        n.final_reg = -1;
        n.cost = 0;
        n.name = AVP_S("");
        n.def_op = AVP_S("");
        avp_vec_push(&p->nodes, n);
    }
}

void mwt_ra_parse(AvpArena *a, AvpStr log, MwtRaTrace *t) {
    memset(t, 0, sizeof *t);
    t->links = avp_imap_new();
    AvpIMap *names = avp_imap_new(); /* obj -> AvpStr* name, from ARG/LOC lines */
    MwtRaFunc *f = NULL;
    MwtRaPass *p = NULL;
    MwtRaNode *sel = NULL; /* the node select is on */
    MwtRaCands cands = {0};
    int sweep = 0;
    AvpStrVec binders = {0};       /* the inliner's argument binder calls in progress: their callees */
    AvpMap *renames = avp_map_new(); /* "@NNN" -> AvpStr* "l|p callee orig" */
    AvpStrVec lines = avp_str_splitlines(log, false);
    for (size_t li = 0; li < lines.n; li++) {
        AvpStr line = avp_str_rstrip(lines.v[li]);
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
        AvpStrVec w = avp_str_split(line, AVP_S(" "), -1);
        if (!w.n) continue;
        AvpStr tag = w.v[0];
        if (avp_str_eqz(tag, "IB") && w.n >= 2) {
            AvpStrVec w2 = avp_str_split(line, AVP_S(" "), 2);
            avp_vec_push(&binders, w2.n > 2 ? avp_str_dup(a, w2.v[2]) : AVP_S("?"));
            continue;
        }
        if (avp_str_eqz(tag, "IBR")) {
            if (binders.n) binders.n--;
            continue;
        }
        if (avp_str_eqz(tag, "INL") && w.n >= 4) {
            AvpStr *s = AVP_NEW(a, AvpStr);
            *s = avp_str(avp_sprintf(a, "%.*s %.*s %.*s", AVP_SF(w.v[1]), AVP_SF(binders.n ? avp_vec_last(&binders) : AVP_S("?")),
                                     AVP_SF(w.v[3])));
            bool created;
            avp_map_put(renames, avp_str_dup(a, w.v[2]), &created)->p = s;
            continue;
        }
        if (avp_str_eqz(tag, "FN") && w.n >= 3) {
            if (p) finish_pass(a, p);
            p = NULL;
            sel = NULL;
            MwtRaFunc nf = {0};
            nf.obj = (u64)num(w.v[1]);
            nf.traced = num(w.v[2]) != 0;
            AvpStrVec w3 = avp_str_split(line, AVP_S(" "), 3);
            nf.name = w3.n > 3 ? avp_str_dup(a, w3.v[3]) : AVP_S("");
            avp_vec_push(&t->funcs, nf);
            f = &avp_vec_last(&t->funcs);
        } else if (avp_str_eqz(tag, "LN") && w.n >= 3) {
            AvpStrVec w2 = avp_str_split(line, AVP_S(" "), 2);
            AvpStr *s = AVP_NEW(a, AvpStr);
            *s = avp_str_dup(a, w2.v[2]);
            bool created;
            avp_imap_put(t->links, (u64)num(w.v[1]), &created)->p = s;
        } else if (avp_str_eqz(tag, "IG") && w.n >= 5 && f) {
            if (p) finish_pass(a, p);
            MwtRaPass np;
            pass_init(&np, (int)num(w.v[1]), (int)num(w.v[2]), (int)num(w.v[3]), (int)num(w.v[4]));
            avp_vec_push(&f->passes, np);
            p = &avp_vec_last(&f->passes);
            sel = NULL;
            sweep = 0;
            cands.n = 0;
        } else if (!p) {
            continue;
        } else if (avp_str_eqz(tag, "CO") && w.n >= 3) {
            MwtRaMerge m = {(int)num(w.v[1]), (int)num(w.v[2]), AVP_S("")};
            avp_vec_push(&p->merges, m);
        } else if (avp_str_eqz(tag, "CX") && w.n >= 4) {
            int x = (int)s16(num(w.v[1])), y = (int)s16(num(w.v[2]));
            MwtRaMerge m = {x < y ? x : y, x < y ? y : x, avp_str_dup(a, w.v[3])};
            bool dup = false;
            for (size_t k = 0; k < p->merges.n && !dup; k++)
                dup = p->merges.v[k].lo == m.lo && p->merges.v[k].hi == m.hi && avp_str_eq(p->merges.v[k].why, m.why);
            if (!dup) avp_vec_push(&p->merges, m);
        } else if ((avp_str_eqz(tag, "ARG") || avp_str_eqz(tag, "LOC")) && w.n >= 6) {
            MwtRaVar v = {0};
            v.obj = (u64)num(w.v[1]);
            v.vflags = (int)num(w.v[2]);
            v.vclass = (int)num(w.v[3]);
            v.reg = (int)s16(num(w.v[4]));
            v.reg2 = (int)s16(num(w.v[5]));
            AvpStrVec w6 = avp_str_split(line, AVP_S(" "), 6);
            v.name = w6.n > 6 ? avp_str_dup(a, w6.v[6]) : AVP_S("");
            v.is_param = avp_str_eqz(tag, "ARG");
            bool created;
            AvpStr *s = AVP_NEW(a, AvpStr);
            *s = v.name;
            avp_imap_put(names, v.obj, &created)->p = s;
            /* the walk repeats per pass: keep this pass's copy once */
            if (!var_of(p, v.obj)) avp_vec_push(&p->vars, v);
        } else if (avp_str_eqz(tag, "INH") && w.n >= 3) {
            MwtRaNode *n = node_of(p, num(w.v[1]));
            if (n) n->inherited = true;
        } else if (avp_str_eqz(tag, "DEF") && w.n >= 3) {
            MwtRaNode *n = node_of(p, num(w.v[1]));
            if (n) {
                if (n->def_at < 0) {
                    n->def_at = num(w.v[2]);
                    n->def_op = w.n > 3 ? avp_str_dup(a, w.v[3]) : AVP_S("");
                }
                n->ndefs++;
            }
        } else if (avp_str_eqz(tag, "N") && w.n >= 9) {
            MwtRaNode *n = node_of(p, num(w.v[1]));
            if (!n) continue;
            n->alias = (int)s16(num(w.v[2]));
            n->flags = (u32)num(w.v[3]);
            n->degree = (int)s16(num(w.v[4]));
            n->colour = (int)s16(num(w.v[5]));
            n->obj = (u64)num(w.v[6]);
            n->remat = (u64)num(w.v[8]);
            AvpVal *nm = n->obj ? avp_imap_get(names, n->obj) : NULL;
            n->name = nm ? *(AvpStr *)nm->p : AVP_S("");
            AvpVal *rn = n->name.n && n->name.p[0] == '@' ? avp_map_get(renames, n->name) : NULL;
            if (rn) {
                AvpStrVec r3 = avp_str_split(*(AvpStr *)rn->p, AVP_S(" "), 2);
                if (r3.n == 3) {
                    n->inl_param = avp_str_eqz(r3.v[0], "p");
                    n->inl_from = r3.v[1];
                    n->orig = r3.v[2];
                }
            }
        } else if (avp_str_eqz(tag, "K") && w.n >= 4) {
            p->K = (int)num(w.v[2]);
            p->vle = num(w.v[3]) != 0;
        } else if (avp_str_eqz(tag, "SW")) {
            sweep++;
            p->sweeps = sweep;
        } else if ((avp_str_eqz(tag, "P1") || avp_str_eqz(tag, "P2")) && w.n >= 3) {
            MwtRaNode *n = node_of(p, num(w.v[1]));
            if (!n) continue;
            n->push_how = avp_str_eqz(tag, "P1") ? MWT_PUSH_LOW : MWT_PUSH_AFTER;
            n->push_degree = (int)s16(num(w.v[2]));
            n->push_sweep = sweep;
            avp_vec_push(&p->push, n->index);
        } else if (avp_str_eqz(tag, "EST")) {
            p->estimated = true;
        } else if (avp_str_eqz(tag, "CAND") && w.n >= 5) {
            MwtRaCand c = {(int)num(w.v[1]), num(w.v[2]), (int)s16(num(w.v[3])), (u32)num(w.v[4])};
            avp_vec_push(&cands, c);
        } else if (avp_str_eqz(tag, "OPT") && w.n >= 4) {
            MwtRaNode *n = node_of(p, num(w.v[1]));
            MwtRaChoice ch = {0};
            ch.chosen = (int)num(w.v[1]);
            for (size_t i = 0; i < cands.n; i++) avp_vec_push(&ch.cands, cands.v[i]);
            cands.n = 0;
            avp_vec_push(&p->choices, ch);
            if (!n) continue;
            n->push_how = MWT_PUSH_OPTIMISTIC;
            n->push_degree = (int)s16(num(w.v[3]));
            n->push_sweep = sweep;
            avp_vec_push(&p->push, n->index);
        } else if (avp_str_eqz(tag, "VOL") && w.n >= 2) {
            p->vol = (u32)num(w.v[1]);
        } else if (avp_str_eqz(tag, "AVAIL") && w.n >= 2) {
            p->avail = (u32)num(w.v[1]);
        } else if (avp_str_eqz(tag, "SEL") && w.n >= 4) {
            sel = node_of(p, num(w.v[1]));
            if (!sel) continue;
            sel->cost = num(w.v[3]);
            push_unique(&p->select, sel->index);
        } else if (avp_str_eqz(tag, "E") && w.n >= 3) {
            MwtRaNode *n = node_of(p, num(w.v[1]));
            if (n) push_unique(&n->phys, (int)num(w.v[2]));
        } else if (avp_str_eqz(tag, "FREE") && w.n >= 2) {
            if (sel && !sel->free_seen) {
                sel->free_mask = (u32)num(w.v[1]);
                sel->free_seen = true;
            }
        } else if (avp_str_eqz(tag, "PICK") && w.n >= 2) {
            if (sel) sel->colour = (int)num(w.v[1]);
        } else if (avp_str_eqz(tag, "NV") && w.n >= 2) {
            int r = (int)num(w.v[1]);
            if (r >= 0) avp_vec_push(&p->nv, r);
            if (sel && r >= 0) sel->nv_added = r;
        } else if (avp_str_eqz(tag, "SPILL") && w.n >= 2) {
            MwtRaNode *n = node_of(p, num(w.v[1]));
            if (n) {
                n->spilled = true;
                avp_vec_push(&p->spills, n->index);
            }
        } else if (avp_str_eqz(tag, "RWV") && w.n >= 2) {
            MwtRaNode *n = node_of(p, num(w.v[1]));
            if (n) n->rewritten = true;
        } else if (avp_str_eqz(tag, "SELR") && w.n >= 2) {
            p->ok = (int)num(w.v[1]);
        }
    }
    if (p) finish_pass(a, p);
    avp_vec_free(&lines);
    avp_vec_free(&binders);
    avp_map_free(renames);
    avp_vec_free(&cands);
}

AvpStr mwt_ra_link(const MwtRaTrace *t, const MwtRaFunc *f) {
    AvpVal *v = avp_imap_get(t->links, f->obj);
    return v ? *(AvpStr *)v->p : f->name;
}

AvpStr mwt_ra_source_name(AvpStr sym) {
    size_t from = 0;
    if (avp_str_startswith(sym, AVP_S("__"))) from = 2;
    i64 k = avp_str_find(sym, AVP_S("__"), (i64)from, (i64)sym.n);
    if (k <= 0 || (size_t)k + 2 >= sym.n) return sym;
    char c = sym.p[k + 2];
    /* a mangled tail starts with a qualifier, a length, a const or a function signature */
    if ((c >= '0' && c <= '9') || c == 'Q' || c == 'F' || c == 'C') return avp_str_slice(sym, 0, k);
    return sym;
}

const MwtRaFunc *mwt_ra_find(AvpArena *a, const MwtRaTrace *t, const char *sym, const char **why) {
    const MwtRaFunc *hit = NULL;
    int n = 0;
    for (size_t i = 0; i < t->funcs.n; i++) {
        const MwtRaFunc *f = &t->funcs.v[i];
        if (!f->traced) continue;
        if (avp_str_eqz(mwt_ra_link(t, f), sym) || avp_str_eqz(f->name, sym)) {
            if (!hit) hit = f;
            n++;
        }
    }
    if (n == 1) return hit;
    if (!n) { /* no link name was logged for it (made before or outside the logged stores): its source name, when one
                 * traced function without a logged link name has it */
        AvpStr src = mwt_ra_source_name(avp_str(sym));
        for (size_t i = 0; i < t->funcs.n; i++) {
            const MwtRaFunc *f = &t->funcs.v[i];
            if (f->traced && !avp_imap_get(t->links, f->obj) && avp_str_eq(f->name, src)) {
                if (!hit) hit = f;
                n++;
            }
        }
        if (n == 1) return hit;
    }
    if (!n) {
        *why = avp_sprintf(a, "%s: no function of that link or source name was compiled with register allocation", sym);
        return NULL;
    }
    *why = avp_sprintf(a, "%s: %d functions of that name were compiled; give the link name", sym, n);
    return NULL;
}

const MwtRaPass *mwt_ra_final(const MwtRaFunc *f, int cls) {
    const MwtRaPass *last = NULL;
    for (size_t i = 0; i < f->passes.n; i++)
        if (f->passes.v[i].cls == cls) last = &f->passes.v[i];
    return last;
}

const char *mwt_ra_reg(AvpArena *a, int cls, int reg) {
    if (reg < 0) return "-";
    switch (cls) {
    case MWT_RC_GPR: return avp_sprintf(a, "r%d", reg);
    case MWT_RC_FPR: return avp_sprintf(a, "f%d", reg);
    case MWT_RC_CR: return avp_sprintf(a, "cr%d", reg);
    case MWT_RC_VR: return avp_sprintf(a, "v%d", reg);
    default: return avp_sprintf(a, "spr%d", reg);
    }
}

AvpStr mwt_ra_label(AvpArena *a, const MwtRaNode *n) {
    if (n->orig.n) return avp_str(avp_sprintf(a, "%.*s:%.*s (%.*s)", AVP_SF(n->inl_from), AVP_SF(n->orig), AVP_SF(n->name)));
    if (n->name.n && n->name.p[0] == '@') return avp_str(avp_sprintf(a, "%.*s (compiler-made)", AVP_SF(n->name)));
    if (n->kind == 'p' || n->kind == 'l' || n->kind == 'o') return n->name;
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, "temp");
    if (n->def_at >= 0) avp_buf_printf(&b, " (#%lld %.*s)", (long long)n->def_at, AVP_SF(n->def_op));
    else avp_buf_putz(&b, " (no definition)");
    return avp_buf_str(&b);
}
