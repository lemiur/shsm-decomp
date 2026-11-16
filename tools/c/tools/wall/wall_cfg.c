/* wall_cfg: control-flow graphs of tools/wall.py (see wall_cfg.h). */
#include "wall_cfg.h"

/* ---- Cfg ---- */

static void cfg_back_edges(AvpArena *a, WallCfg *g) {
    /* _back_edges: iterative DFS from block 0; state 1 = on the stack, 2 = done. */
    size_t nb = g->nblocks;
    g->reachable = AVP_NEWN(a, bool, nb);
    g->nreachable = 0;
    g->back_edges = AVP_NEWN(a, WallEdge, 0);
    g->nback_edges = 0;
    if (!nb)
        return;
    u8 *state = avp_alloc(avp_scratch(), nb); /* freed with the caller's mark */
    int *stack = AVP_NEWN(avp_scratch(), int, nb);
    size_t *pos = AVP_NEWN(avp_scratch(), size_t, nb);
    AVP_VEC(WallEdge) out = {0};
    size_t sp = 0;
    state[0] = 1;
    stack[sp] = 0;
    pos[sp++] = 0;
    while (sp) {
        int u = stack[sp - 1];
        const WallBlock *bu = &g->blocks[u];
        if (pos[sp - 1] >= bu->nsucc) {
            state[u] = 2;
            sp--;
            continue;
        }
        int v = bu->succ[pos[sp - 1]++];
        if (state[v] == 1) {
            WallEdge e = {u, v};
            avp_vec_push(&out, e);
        } else if (!state[v]) {
            state[v] = 1;
            stack[sp] = v;
            pos[sp++] = 0;
        }
    }
    for (size_t i = 0; i < nb; i++)
        if (state[i]) {
            g->reachable[i] = true;
            g->nreachable++;
        }
    g->back_edges = AVP_NEWN(a, WallEdge, out.n);
    if (out.n)
        memcpy(g->back_edges, out.v, out.n * sizeof *out.v);
    g->nback_edges = out.n;
    avp_vec_free(&out);
}

static bool is_cond_kind(WallKind k) { return k == WALL_K_COND || k == WALL_K_CONDRET || k == WALL_K_CONDTAIL; }

static void cfg_loops(AvpArena *a, WallCfg *g) {
    size_t nb = g->nblocks;
    AvpArena *s = avp_scratch();
    bool **bodies = AVP_NEWN(s, bool *, nb);
    int *work = AVP_NEWN(s, int, nb + 1);
    for (size_t e = 0; e < g->nback_edges; e++) {
        int u = g->back_edges[e].u, v = g->back_edges[e].v;
        if (!bodies[v])
            bodies[v] = AVP_NEWN(s, bool, nb);
        /* body = {v, u}; work = [u]; preds walked back to v. Accumulating into bodies[v] directly gives the same
         * union: every block already in it (but v) had its predecessors added when it went in. */
        bool *body = bodies[v];
        body[v] = body[u] = true;
        size_t wn = 0;
        work[wn++] = u;
        while (wn) {
            int x = work[--wn];
            if (x == v)
                continue;
            const WallBlock *bx = &g->blocks[x];
            for (size_t k = 0; k < bx->npreds; k++) {
                int p = bx->preds[k];
                if (!body[p]) {
                    body[p] = true;
                    work[wn++] = p;
                }
            }
        }
    }
    size_t nloops = 0;
    for (size_t v = 0; v < nb; v++)
        nloops += bodies[v] != NULL;
    g->loops = AVP_NEWN(a, WallLoop, nloops);
    g->nloops = 0;
    for (size_t v = 0; v < nb; v++) {
        bool *body = bodies[v];
        if (!body)
            continue;
        WallLoop *lp = &g->loops[g->nloops++];
        lp->header = (int)v;
        size_t cnt = 0;
        for (size_t b = 0; b < nb; b++)
            cnt += body[b];
        lp->blocks = AVP_NEWN(a, int, cnt);
        int lo = -1, hi = -1;
        bool any_exit = false, all_hi = true, all_lo = true;
        for (size_t b = 0; b < nb; b++)
            if (body[b]) {
                lp->blocks[lp->nblocks++] = (int)b;
                if (lo < 0)
                    lo = (int)b;
                hi = (int)b;
            }
        for (size_t i = 0; i < lp->nblocks; i++) {
            const WallBlock *bb = &g->blocks[lp->blocks[i]];
            if (!is_cond_kind(bb->kind))
                continue;
            bool exits = false;
            for (size_t k = 0; k < bb->nsucc; k++)
                if (!body[bb->succ[k]])
                    exits = true;
            if (!exits)
                continue;
            any_exit = true;
            if (bb->id != hi)
                all_hi = false;
            if (bb->id != lo)
                all_lo = false;
        }
        if (any_exit && all_hi)
            lp->kind = "bottom-test";
        else if (any_exit && all_lo)
            lp->kind = "top-test";
        else if (any_exit)
            lp->kind = "middle-test";
        else
            lp->kind = "no-exit";
    }
}

WallCfg *wall_cfg_new(AvpArena *a, const CcRow *rows, size_t n) {
    AvpArena *s = avp_scratch();
    AvpArenaMark mark = avp_arena_mark(s);
    bool own = a != s; /* results on the scratch arena itself: leave the temporaries to the caller's reset */
    WallCfg *g = AVP_NEW(a, WallCfg);
    g->rows = rows;
    g->nrows = n;
    WallKind *kinds = AVP_NEWN(s, WallKind, n);
    bool *leader = AVP_NEWN(s, bool, n + 1);
    if (n)
        leader[0] = true;
    for (size_t i = 0; i < n; i++) {
        kinds[i] = wall_row_kind(&rows[i]);
        if (wall_kind_is_terminator(kinds[i]) && i + 1 < n)
            leader[i + 1] = true;
        if ((kinds[i] == WALL_K_COND || kinds[i] == WALL_K_GOTO) && rows[i].bidx >= 0 && (size_t)rows[i].bidx < n)
            leader[rows[i].bidx] = true;
    }
    size_t nb = 0;
    for (size_t i = 0; i < n; i++)
        nb += leader[i];
    g->blocks = AVP_NEWN(a, WallBlock, nb);
    g->nblocks = nb;
    g->block_of = AVP_NEWN(a, int, n);
    size_t b = 0;
    for (size_t i = 0; i < n; i++) {
        if (!leader[i])
            continue;
        size_t e = i + 1;
        while (e < n && !leader[e])
            e++;
        WallBlock *blk = &g->blocks[b];
        blk->id = (int)b;
        blk->start = i;
        blk->end = e;
        blk->kind = wall_kind_is_terminator(kinds[e - 1]) ? kinds[e - 1] : WALL_K_FALL;
        for (size_t r = i; r < e; r++)
            g->block_of[r] = (int)b;
        b++;
    }
    g->approx_switch = false;
    for (size_t k = 0; k < nb; k++) {
        WallBlock *blk = &g->blocks[k];
        int nxt = k + 1 < nb ? (int)k + 1 : -1;
        const CcRow *last = &rows[blk->end - 1];
        blk->succ = AVP_NEWN(a, int, 2);
        blk->nsucc = 0;
        if (blk->kind == WALL_K_COND) {
            blk->succ[blk->nsucc++] = g->block_of[last->bidx];
            if (nxt >= 0)
                blk->succ[blk->nsucc++] = nxt;
        } else if (blk->kind == WALL_K_GOTO) {
            blk->succ[blk->nsucc++] = g->block_of[last->bidx];
        } else if (blk->kind == WALL_K_CONDRET || blk->kind == WALL_K_CONDTAIL || blk->kind == WALL_K_FALL) {
            if (nxt >= 0)
                blk->succ[blk->nsucc++] = nxt;
        }
    }
    bool *has_pred = AVP_NEWN(s, bool, nb);
    for (size_t k = 0; k < nb; k++)
        for (size_t j = 0; j < g->blocks[k].nsucc; j++)
            has_pred[g->blocks[k].succ[j]] = true;
    size_t norph = 0;
    int *orphans = AVP_NEWN(s, int, nb);
    for (size_t k = 1; k < nb; k++)
        if (!has_pred[k])
            orphans[norph++] = (int)k;
    for (size_t k = 0; k < nb; k++) {
        WallBlock *blk = &g->blocks[k];
        if (blk->kind == WALL_K_SWITCH) {
            blk->succ = AVP_NEWN(a, int, norph);
            if (norph)
                memcpy(blk->succ, orphans, norph * sizeof(int));
            blk->nsucc = norph;
            g->approx_switch = true;
        }
    }
    size_t *npred = AVP_NEWN(s, size_t, nb);
    for (size_t k = 0; k < nb; k++)
        for (size_t j = 0; j < g->blocks[k].nsucc; j++)
            npred[g->blocks[k].succ[j]]++;
    for (size_t k = 0; k < nb; k++)
        g->blocks[k].preds = AVP_NEWN(a, int, npred[k]);
    for (size_t k = 0; k < nb; k++)
        for (size_t j = 0; j < g->blocks[k].nsucc; j++) {
            WallBlock *t = &g->blocks[g->blocks[k].succ[j]];
            t->preds[t->npreds++] = (int)k;
        }
    cfg_back_edges(a, g);
    cfg_loops(a, g);
    if (own)
        avp_arena_reset(s, mark);
    return g;
}

WallCfg *wall_cfg_of_fn(AvpArena *a, const CcFn *fn) { return wall_cfg_new(a, fn->rows, fn->nrows); }

AvpJson *wall_cfg_summary(AvpArena *a, const WallCfg *g) {
    size_t kinds[WALL_K_CALL + 1] = {0};
    size_t edges = 0;
    for (size_t k = 0; k < g->nblocks; k++) {
        kinds[g->blocks[k].kind]++;
        edges += g->blocks[k].nsucc;
    }
    AvpJson *loops = avp_json_obj(a);
    for (size_t i = 0; i < g->nloops; i++) {
        AvpStr key = avp_str(g->loops[i].kind);
        AvpJson *cur = avp_json_get(loops, key);
        avp_json_set(a, loops, key, avp_json_int(a, avp_json_i64(cur, 0) + 1));
    }
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("blocks"), avp_json_int(a, (i64)g->nblocks));
    avp_json_set(a, o, AVP_S("edges"), avp_json_int(a, (i64)edges));
    avp_json_set(a, o, AVP_S("back_edges"), avp_json_int(a, (i64)g->nback_edges));
    avp_json_set(a, o, AVP_S("loops"), loops);
    avp_json_set(a, o, AVP_S("exits"),
                 avp_json_int(a, (i64)(kinds[WALL_K_RET] + kinds[WALL_K_CONDRET] + kinds[WALL_K_TAIL] +
                                       kinds[WALL_K_CONDTAIL])));
    avp_json_set(a, o, AVP_S("returns"), avp_json_int(a, (i64)(kinds[WALL_K_RET] + kinds[WALL_K_CONDRET])));
    avp_json_set(a, o, AVP_S("switches"), avp_json_int(a, (i64)kinds[WALL_K_SWITCH]));
    avp_json_set(a, o, AVP_S("conds"), avp_json_int(a, (i64)kinds[WALL_K_COND]));
    return o;
}

static AvpJson *int_array(AvpArena *a, const int *v, size_t n) {
    AvpJson *arr = avp_json_arr_cap(a, n);
    for (size_t i = 0; i < n; i++)
        avp_json_arr_push(a, arr, avp_json_int(a, v[i]));
    return arr;
}

AvpJson *wall_block_to_dict(AvpArena *a, const WallBlock *b) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("id"), avp_json_int(a, b->id));
    AvpJson *rows = avp_json_arr_cap(a, 2);
    avp_json_arr_push(a, rows, avp_json_int(a, (i64)b->start));
    avp_json_arr_push(a, rows, avp_json_int(a, (i64)b->end));
    avp_json_set(a, o, AVP_S("rows"), rows);
    avp_json_set(a, o, AVP_S("kind"), avp_json_strz(a, wall_kind_name(b->kind)));
    avp_json_set(a, o, AVP_S("succ"), int_array(a, b->succ, b->nsucc));
    return o;
}

AvpJson *wall_loop_to_dict(AvpArena *a, const WallLoop *lp) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("header"), avp_json_int(a, lp->header));
    avp_json_set(a, o, AVP_S("blocks"), int_array(a, lp->blocks, lp->nblocks));
    avp_json_set(a, o, AVP_S("kind"), avp_json_strz(a, lp->kind));
    return o;
}

WallKind wall_iso_kind(WallKind k) { return (k == WALL_K_GOTO || k == WALL_K_FALL) ? WALL_K_GOTO : k; }

/* ---- match_cfgs ---- */

/* The Python's persistent pair stack ((a, b), rest) as nodes in one vector; next == -1 is None. */
typedef struct {
    int a, b, next;
} IsoNode;

/* One activation of the Python's go(): where its trail starts and, once it reached a two-way conditional block,
 * the two pairings to try. */
typedef struct {
    size_t trail_base;
    int cond;      /* the conditional block (for swaps) */
    int opt[2];    /* the stacks of the two options, in the order tried */
    bool swap[2];  /* option i pairs crosswise */
    int opt_i;     /* the option being tried */
} IsoFrame;

typedef struct {
    AVP_VEC(IsoNode) nodes;
    AVP_VEC(IsoFrame) frames;
    AvpI32Vec trail; /* mapped retail blocks, in insertion order */
    AvpI32Vec swaps;
    int *m, *r;      /* m[a] = b, r[b] = a; -1 where absent */
} IsoState;

static int iso_push(IsoState *st, int a, int b, int next) {
    IsoNode nd = {a, b, next};
    avp_vec_push(&st->nodes, nd);
    return (int)st->nodes.n - 1;
}

static void iso_undo(IsoState *st, size_t base) {
    while (st->trail.n > base) {
        int a = avp_vec_pop(&st->trail);
        st->r[st->m[a]] = -1;
        st->m[a] = -1;
    }
}

WallIso wall_match_cfgs(AvpArena *a, const WallCfg *ga, const WallCfg *gb, i64 budget, WallSimFn sim, void *ctx) {
    WallIso res = {0};
    if (!ga->nblocks || !gb->nblocks) {
        if (!ga->nblocks && !gb->nblocks) {
            res.status = WALL_ISO_MAPPING;
            res.map = AVP_NEWN(a, int, 0);
            res.order = AVP_NEWN(a, int, 0);
            res.swaps = AVP_NEWN(a, int, 0);
        }
        return res;
    }
    IsoState st = {0};
    st.m = avp_xmalloc(ga->nblocks * sizeof(int));
    st.r = avp_xmalloc(gb->nblocks * sizeof(int));
    for (size_t i = 0; i < ga->nblocks; i++)
        st.m[i] = -1;
    for (size_t i = 0; i < gb->nblocks; i++)
        st.r[i] = -1;
    i64 left = budget;
    int ok = -1; /* 1 True, 0 False, 2 budget */

    IsoFrame f0 = {0};
    avp_vec_push(&st.frames, f0);
    int s = iso_push(&st, 0, 0, -1);
    for (;;) {
        /* run the current (top) frame on stack s */
        bool fail = false;
        while (s != -1) {
            IsoNode nd = st.nodes.v[s];
            s = nd.next;
            int pa = nd.a, pb = nd.b;
            if (st.m[pa] != -1) {
                if (st.m[pa] != pb) {
                    fail = true;
                    break;
                }
                continue;
            }
            const WallBlock *ba = &ga->blocks[pa], *bb = &gb->blocks[pb];
            if (st.r[pb] != -1 || wall_iso_kind(ba->kind) != wall_iso_kind(bb->kind) || ba->nsucc != bb->nsucc) {
                fail = true;
                break;
            }
            st.m[pa] = pb;
            st.r[pb] = pa;
            avp_vec_push(&st.trail, pa);
            if (--left < 0) {
                ok = 2;
                break;
            }
            const int *sa = ba->succ, *sb = bb->succ;
            if (ba->nsucc == 2 && ba->kind == WALL_K_COND) {
                int straight = iso_push(&st, sa[1], sb[1], s);
                straight = iso_push(&st, sa[0], sb[0], straight);
                int crossed = iso_push(&st, sa[1], sb[0], s);
                crossed = iso_push(&st, sa[0], sb[1], crossed);
                IsoFrame *f = &avp_vec_last(&st.frames);
                f->cond = pa;
                f->opt[0] = straight;
                f->opt[1] = crossed;
                f->swap[0] = false;
                f->swap[1] = true;
                f->opt_i = 0;
                if (sim) {
                    i64 x = sim(ctx, sa[0], sb[1]);
                    x += sim(ctx, sa[1], sb[0]);
                    i64 y = sim(ctx, sa[0], sb[0]);
                    y += sim(ctx, sa[1], sb[1]);
                    if (x > y) { /* the contents say the successors are crossed */
                        f->opt[0] = crossed;
                        f->opt[1] = straight;
                        f->swap[0] = true;
                        f->swap[1] = false;
                    }
                }
                if (f->swap[0])
                    avp_vec_push(&st.swaps, pa);
                s = f->opt[0];
                IsoFrame child = {0};
                child.trail_base = st.trail.n;
                avp_vec_push(&st.frames, child);
                continue;
            }
            for (size_t k = ba->nsucc; k-- > 0;)
                s = iso_push(&st, sa[k], sb[k], s);
        }
        if (ok == 2)
            break;
        if (!fail) { /* this frame returned True, and so does every frame above it */
            ok = 1;
            break;
        }
        /* this frame returned False: undo it and try the parent's next option */
        bool resumed = false;
        while (!resumed) {
            IsoFrame done = avp_vec_pop(&st.frames);
            iso_undo(&st, done.trail_base);
            if (!st.frames.n) {
                ok = 0;
                break;
            }
            IsoFrame *p = &avp_vec_last(&st.frames);
            if (p->swap[p->opt_i])
                (void)avp_vec_pop(&st.swaps);
            if (++p->opt_i < 2) {
                if (p->swap[p->opt_i])
                    avp_vec_push(&st.swaps, p->cond);
                s = p->opt[p->opt_i];
                IsoFrame child = {0};
                child.trail_base = st.trail.n;
                avp_vec_push(&st.frames, child);
                resumed = true;
            }
            /* else both options failed: the parent fails too (loop: undo it) */
        }
        if (ok == 0)
            break;
    }

    if (ok == 2) {
        res.status = WALL_ISO_BUDGET;
    } else if (ok == 1 && st.trail.n == ga->nreachable && st.trail.n == gb->nreachable) {
        /* len(r) == len(m): every mapping is a pair */
        res.status = WALL_ISO_MAPPING;
        res.nmap_a = ga->nblocks;
        res.map = AVP_NEWN(a, int, ga->nblocks);
        memcpy(res.map, st.m, ga->nblocks * sizeof(int));
        res.norder = st.trail.n;
        res.order = AVP_NEWN(a, int, st.trail.n);
        if (st.trail.n)
            memcpy(res.order, st.trail.v, st.trail.n * sizeof(int));
        res.nswaps = st.swaps.n;
        res.swaps = AVP_NEWN(a, int, st.swaps.n);
        if (st.swaps.n)
            memcpy(res.swaps, st.swaps.v, st.swaps.n * sizeof(int));
    }
    avp_vec_free(&st.nodes);
    avp_vec_free(&st.frames);
    avp_vec_free(&st.trail);
    avp_vec_free(&st.swaps);
    avp_xfree(st.m);
    avp_xfree(st.r);
    return res;
}

bool wall_iso_mapping_equal(const WallIso *m1, const WallIso *m2) {
    if (m1->status != WALL_ISO_MAPPING || m2->status != WALL_ISO_MAPPING || m1->norder != m2->norder)
        return false;
    for (size_t i = 0; i < m1->norder; i++) {
        int k = m1->order[i];
        if ((size_t)k >= m2->nmap_a || m2->map[k] != m1->map[k])
            return false;
    }
    return true;
}

AvpJson *wall_iso_mapping_json(AvpArena *a, const WallIso *m) {
    AvpJson *o = avp_json_obj(a);
    for (size_t i = 0; i < m->norder; i++) {
        int k = m->order[i];
        AvpStr key = avp_str(avp_sprintf(a, "%d", k));
        avp_json_set(a, o, key, avp_json_int(a, m->map[k]));
    }
    return o;
}

/* ---- _lis_keep ---- */

bool *wall_lis_keep(AvpArena *a, const i64 *seq, size_t n) {
    bool *keep = AVP_NEWN(a, bool, n);
    if (!n)
        return keep;
    AvpArena *s = avp_scratch();
    AvpArenaMark mark = avp_arena_mark(s);
    i64 *tails = AVP_NEWN(s, i64, n);
    size_t *tails_i = AVP_NEWN(s, size_t, n);
    i64 *prev = AVP_NEWN(s, i64, n);
    size_t ntails = 0;
    for (size_t i = 0; i < n; i++) {
        i64 x = seq[i];
        size_t lo = 0, hi = ntails; /* bisect_left */
        while (lo < hi) {
            size_t mid = (lo + hi) / 2;
            if (tails[mid] < x)
                lo = mid + 1;
            else
                hi = mid;
        }
        size_t k = lo;
        tails[k] = x;
        tails_i[k] = i;
        if (k == ntails)
            ntails++;
        prev[i] = k ? (i64)tails_i[k - 1] : -1;
    }
    for (i64 i = (i64)tails_i[ntails - 1]; i >= 0; i = prev[i])
        keep[i] = true;
    if (a != s)
        avp_arena_reset(s, mark);
    return keep;
}
