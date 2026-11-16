/* rules_cross: the Checker's project-wide rules (check_rules.py L4050-4258): _unit_private, _definition_guards,
 * _defined_at_first_include, _compiled_out_with, check_cross_file (R1, R2) and _cast_to. */
#include "rules_check.h"
#include "shsm_text.h"

/* re.sub(r"\s+", " ", s) */
static AvpStr squash_ws(AvpArena *a, AvpStr s) {
    AvpBuf b;
    avp_buf_init(&b, a);
    size_t i = 0;
    while (i < s.n) {
        size_t w = shsm_ws_len(s, i);
        if (w) {
            while (i < s.n && (w = shsm_ws_len(s, i)) > 0) i += w;
            avp_buf_putc(&b, ' ');
        } else {
            avp_buf_putc(&b, s.p[i++]);
        }
    }
    return avp_buf_str(&b);
}

bool rl_closure_has(RlChecker *ck, AvpStr path, AvpStr q) {
    const AvpStrVec *c = rl_checker_closure(ck, path);
    size_t lo = 0, hi = c->n;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        int r = avp_str_cmp(c->v[mid], q);
        if (r == 0) return true;
        if (r < 0) lo = mid + 1;
        else hi = mid;
    }
    return false;
}

typedef struct {
    AvpStr path;
    const RlAgg *agg;
} Def;
typedef RL_VEC(Def) Defs;

static bool unit_private(RlChecker *ck, const Defs *lst) {
    AvpArena *a = ck->a;
    size_t n = lst->n;
    AvpStr **norm = AVP_NEWN(a, AvpStr *, n);
    for (size_t x = 0; x < n; x++) {
        const RlAgg *g = lst->v[x].agg;
        norm[x] = AVP_NEWN(a, AvpStr, g->members.n + 1);
        for (size_t m = 0; m < g->members.n; m++) norm[x][m] = squash_ws(a, g->members.v[m]);
    }
    for (size_t x = 0; x < n; x++)
        for (size_t y = 0; y < n; y++) {
            if (x == y) continue;
            size_t nx = lst->v[x].agg->members.n, ny = lst->v[y].agg->members.n;
            if (ny < nx) continue; /* norm[y][:len(norm[x])] is shorter than norm[x] */
            bool eq = true;
            for (size_t m = 0; m < nx && eq; m++) eq = avp_str_eq(norm[y][m], norm[x][m]);
            if (eq) return false;
        }
    for (size_t i = 0; i < n; i++) {
        RlParsed *P = rl_checker_parse(ck, lst->v[i].path);
        if (P->is_header || P->is_cpp) return false;
    }
    for (size_t i = 0; i < n; i++)
        for (size_t k = 0; k < n; k++) {
            if (avp_str_eq(lst->v[k].path, lst->v[i].path)) continue;
            if (rl_closure_has(ck, lst->v[i].path, lst->v[k].path)) return false;
        }
    return true;
}

/* _definition_guards(P, line) as a set */
static AvpSet *definition_guards(RlParsed *P, i32 line) {
    static const ShsmRe *not_defined;
    if (!not_defined) not_defined = shsm_re("!\\s*defined\\s*(?:\\(\\s*(\\w+)\\s*\\)|(\\w+))", 0);
    RL_VEC(AvpStrVec) stack = {0};
    for (size_t i = 0; i < P->directives.n; i++) {
        const RlDirective *d = &P->directives.v[i];
        if (d->line >= line) break;
        if (avp_str_eqz(d->name, "ifndef") || avp_str_eqz(d->name, "ifdef") || avp_str_eqz(d->name, "if")) {
            AvpStrVec guards = {0};
            AvpStr cond = avp_str_strip(d->rest);
            if (avp_str_eqz(d->name, "ifndef")) {
                AvpStrVec w = avp_str_split(cond, (AvpStr){0}, -1);
                if (w.n) avp_vec_push(&guards, w.v[0]);
                avp_vec_free(&w);
            } else if (avp_str_eqz(d->name, "if") && !avp_str_contains(cond, AVP_S("||"))) {
                ShsmReIter it;
                ShsmReMatch m;
                shsm_re_iter_init(&it, not_defined, cond, 0, AVP_END);
                while (shsm_re_iter_next(&it, &m)) {
                    AvpStr g1 = shsm_re_group(&m, 1);
                    avp_vec_push(&guards, g1.n ? g1 : shsm_re_group(&m, 2));
                }
            }
            *RL_PUSH(P->a, &stack) = guards;
        } else if ((avp_str_eqz(d->name, "else") || avp_str_eqz(d->name, "elif")) && stack.n) {
            stack.v[stack.n - 1].n = 0;
        } else if (avp_str_eqz(d->name, "endif") && stack.n) {
            avp_vec_free(&stack.v[stack.n - 1]);
            stack.n--;
        }
    }
    AvpSet *out = avp_set_new();
    for (size_t i = 0; i < stack.n; i++) {
        for (size_t k = 0; k < stack.v[i].n; k++) avp_set_add(out, stack.v[i].v[k]);
        avp_vec_free(&stack.v[i]);
    }
    return out;
}

/* walk() of _defined_at_first_include: 1 true, 0 false, -1 None */
static int walk(RlChecker *ck, AvpStr path, AvpStr root, AvpStr target, AvpStr macro, AvpSet *defined, AvpSet *visited) {
    static const ShsmRe *inc;
    if (!inc) inc = shsm_re("\\s*[<\"]([^>\"]+)[>\"]", 0);
    avp_set_add(visited, path);
    RlParsed *P = rl_checker_parse(ck, path);
    for (size_t i = 0; i < P->directives.n; i++) {
        const RlDirective *d = &P->directives.v[i];
        if (avp_str_eqz(d->name, "define") && d->toks.n && d->toks.v[0].k == RL_ID) {
            avp_set_add(defined, d->toks.v[0].t);
        } else if (avp_str_eqz(d->name, "undef")) {
            AvpStrVec w = avp_str_split(d->rest, (AvpStr){0}, -1);
            if (w.n) avp_set_del(defined, w.v[0]);
            avp_vec_free(&w);
        } else if (avp_str_eqz(d->name, "include")) {
            ShsmReMatch m;
            if (!shsm_re_match(inc, d->rest, 0, AVP_END, &m)) continue;
            AvpStr q = rl_resolve_include(ck, path, shsm_re_group(&m, 1), true, root);
            if (!q.p) continue;
            if (avp_str_eq(q, target)) return avp_set_has(defined, macro) ? 1 : 0;
            if (!avp_set_has(visited, q)) {
                int r = walk(ck, q, root, target, macro, defined, visited);
                if (r >= 0) return r;
            }
        }
    }
    return -1;
}

static int defined_at_first_include(RlChecker *ck, AvpStr root, AvpStr target, AvpStr macro) {
    if (avp_str_eq(root, target)) return 0;
    AvpSet *defined = avp_set_new(), *visited = avp_set_new();
    int r = walk(ck, root, root, target, macro, defined, visited);
    avp_set_free(defined);
    avp_set_free(visited);
    return r;
}

static bool compiled_out_with(RlChecker *ck, AvpStr path, i32 line, AvpStr other) {
    AvpSet *guards = definition_guards(rl_checker_parse(ck, path), line);
    bool res = false;
    if (avp_set_len(guards)) {
        RL_VEC(AvpStr) roots = {0};
        for (size_t i = 0; i < ck->paths.n; i++) {
            AvpStr r = ck->paths.v[i];
            if (rl_closure_has(ck, r, path) && rl_closure_has(ck, r, other)) *RL_PUSH(ck->a, &roots) = r;
        }
        if (roots.n) {
            /* guards in set order: only any() over them is taken, which the order cannot change */
            res = true;
            for (size_t i = 0; i < roots.n && res; i++) {
                bool any = false;
                size_t pos = 0;
                AvpStr g;
                while (!any && avp_set_next(guards, &pos, &g))
                    any = defined_at_first_include(ck, roots.v[i], path, g) == 1;
                res = any;
            }
        }
    }
    avp_set_free(guards);
    return res;
}

static bool cast_to(RlChecker *ck, AvpStr path, AvpStr typename_) {
    RlParsed *P = rl_checker_parse(ck, path);
    const RlTok *toks = P->toks.v;
    for (size_t i = 0; i < P->toks.n; i++) {
        if (!RL_TEQ(&toks[i], "(")) continue;
        i32 c = rl_cast_info(toks, P->toks.n, P->match, i, false);
        if (c < 0) continue;
        if (!RL_TEQ(&toks[c - 1], "*")) continue;
        for (size_t k = i + 1; k < (size_t)c; k++)
            if (avp_str_eq(toks[k].t, typename_)) return true;
    }
    return false;
}

static bool is_kind(AvpStr k, bool with_union_enum) {
    return avp_str_eqz(k, "struct") || avp_str_eqz(k, "class") ||
           (with_union_enum && (avp_str_eqz(k, "union") || avp_str_eqz(k, "enum")));
}

static AvpStr join_sorted(AvpArena *a, AvpStrVec *v, const char *sep) {
    avp_sort_strs(v->v, v->n);
    return avp_str_join(a, avp_str(sep), v->v, v->n);
}

void rl_check_cross_file(RlChecker *ck) {
    AvpArena *a = ck->a;
    /* R1: one type defined in several files */
    AvpMap *defs = avp_map_new();
    AvpStrVec names = {0};
    for (size_t i = 0; i < ck->paths.n; i++) {
        RlParsed *P = rl_checker_parse(ck, ck->paths.v[i]);
        for (size_t k = 0; k < P->p.aggs.n; k++) {
            const RlAgg *g = &P->p.aggs.v[k];
            if (!g->name.n || !is_kind(g->kind, true)) continue;
            bool created = false;
            AvpVal *v = avp_map_put(defs, g->name, &created);
            if (created) {
                v->p = AVP_NEW(a, Defs);
                avp_vec_push(&names, g->name);
            }
            *RL_PUSH(a, (Defs *)v->p) = (Def){ck->paths.v[i], g};
        }
    }
    for (size_t ni = 0; ni < names.n; ni++) {
        Defs *lst = avp_map_get(defs, names.v[ni])->p;
        if (lst->n < 2) continue;
        AvpSet *paths = avp_set_new(), *bodies = avp_set_new();
        for (size_t k = 0; k < lst->n; k++) {
            avp_set_add(paths, lst->v[k].path);
            avp_set_add(bodies, squash_ws(a, lst->v[k].agg->body_text));
        }
        size_t npaths = avp_set_len(paths), nbodies = avp_set_len(bodies);
        avp_set_free(paths);
        avp_set_free(bodies);
        if (npaths < 2) continue;
        if (nbodies == lst->n && unit_private(ck, lst)) continue;
        for (size_t k = 0; k < lst->n; k++) {
            AvpStr p = lst->v[k].path;
            const RlAgg *ga = lst->v[k].agg;
            if (!avp_set_has(ck->checked_set, p)) continue;
            AvpSet *others = avp_set_new();
            for (size_t q = 0; q < lst->n; q++) {
                AvpStr qp = lst->v[q].path;
                if (avp_str_eq(qp, p)) continue;
                if (!compiled_out_with(ck, p, ga->line, qp) && !compiled_out_with(ck, qp, lst->v[q].agg->line, p))
                    avp_set_add(others, qp);
            }
            AvpStrVec ov = avp_set_sorted(others); /* views into the set's keys: freed after use */
            if (ov.n) {
                const char *kind = nbodies > 1 ? "different bodies" : "an identical copy";
                rl_add(ck, rl_checker_parse(ck, p), ga->line, "R1", "match",
                       rl_fmt(a,
                              "type '%.*s' is defined in more than one file (%s; also %.*s): declare it once in a "
                              "shared header",
                              AVP_SF(names.v[ni]), kind, AVP_SF(avp_str_join(a, AVP_S(", "), ov.v, ov.n))),
                       (AvpStr){0});
            }
            avp_vec_free(&ov);
            avp_set_free(others);
        }
    }
    /* R1: class/struct keyword mismatch */
    typedef struct {
        AvpStr path, kind;
        i32 line;
        bool def;
    } Kind;
    typedef RL_VEC(Kind) Kinds;
    AvpMap *kinds = avp_map_new();
    AvpStrVec knames = {0};
    for (size_t i = 0; i < ck->paths.n; i++) {
        RlParsed *P = rl_checker_parse(ck, ck->paths.v[i]);
        for (size_t k = 0; k < P->p.aggs.n + P->p.fwds.n; k++) {
            bool def = k < P->p.aggs.n;
            AvpStr nm = def ? P->p.aggs.v[k].name : P->p.fwds.v[k - P->p.aggs.n].name;
            AvpStr kd = def ? P->p.aggs.v[k].kind : P->p.fwds.v[k - P->p.aggs.n].kind;
            i32 ln = def ? P->p.aggs.v[k].line : P->p.fwds.v[k - P->p.aggs.n].line;
            if (def && !nm.n) continue;
            if (!is_kind(kd, false)) continue;
            bool created = false;
            AvpVal *v = avp_map_put(kinds, nm, &created);
            if (created) {
                v->p = AVP_NEW(a, Kinds);
                avp_vec_push(&knames, nm);
            }
            *RL_PUSH(a, (Kinds *)v->p) = (Kind){ck->paths.v[i], kd, ln, def};
        }
    }
    AvpSet *cpp_closure = avp_set_new();
    for (size_t i = 0; i < ck->paths.n; i++) {
        if (!rl_checker_parse(ck, ck->paths.v[i])->is_cpp) continue;
        const AvpStrVec *c = rl_checker_closure(ck, ck->paths.v[i]);
        for (size_t k = 0; k < c->n; k++) avp_set_add(cpp_closure, c->v[k]);
    }
    for (size_t ni = 0; ni < knames.n; ni++) {
        Kinds *lst = avp_map_get(kinds, knames.v[ni])->p;
        bool two = false, cpp = false;
        for (size_t k = 0; k < lst->n; k++) {
            if (!avp_str_eq(lst->v[k].kind, lst->v[0].kind)) two = true;
            if (avp_set_has(cpp_closure, lst->v[k].path)) cpp = true;
        }
        if (!two || !cpp) continue;
        for (size_t k = 0; k < lst->n; k++) {
            if (!avp_set_has(ck->checked_set, lst->v[k].path)) continue;
            rl_add(ck, rl_checker_parse(ck, lst->v[k].path), lst->v[k].line, "R1", "match",
                   rl_fmt(a,
                          "'%.*s %.*s' %s disagrees with another declaration's class/struct keyword (changes the "
                          "mangled name)",
                          AVP_SF(lst->v[k].kind), AVP_SF(knames.v[ni]),
                          lst->v[k].def ? "definition" : "forward declaration"),
                   (AvpStr){0});
        }
    }
    avp_set_free(cpp_closure);
    /* R1: layout views */
    AvpMap *by_name = avp_map_new();
    AvpStrVec bnames = {0};
    for (size_t i = 0; i < ck->paths.n; i++) {
        RlParsed *P = rl_checker_parse(ck, ck->paths.v[i]);
        for (size_t k = 0; k < P->p.aggs.n; k++) {
            const RlAgg *g = &P->p.aggs.v[k];
            if (!g->name.n || !is_kind(g->kind, false) || g->members.n < 2) continue;
            bool created = false;
            AvpVal *v = avp_map_put(by_name, g->name, &created);
            if (created) {
                Def *d = AVP_NEW(a, Def);
                *d = (Def){ck->paths.v[i], g};
                v->p = d;
                avp_vec_push(&bnames, g->name);
            }
        }
    }
    for (size_t bi = 0; bi < bnames.n; bi++) {
        const Def *bd = avp_map_get(by_name, bnames.v[bi])->p;
        if (!avp_set_has(ck->checked_set, bd->path)) continue;
        const RlAgg *b = bd->agg;
        for (size_t ai = 0; ai < bnames.n; ai++) {
            if (ai == bi) continue;
            const Def *ad = avp_map_get(by_name, bnames.v[ai])->p;
            const RlAgg *g = ad->agg;
            size_t k = b->members.n;
            if (b->first_hidden >= 0 || (g->first_hidden >= 0 && g->first_hidden < (i64)k)) continue;
            if (k < 2 || g->members.n <= k) continue;
            bool eq = true;
            for (size_t m = 0; m < k && eq; m++) eq = avp_str_eq(g->members.v[m], b->members.v[m]);
            if (!eq) continue;
            if (cast_to(ck, bd->path, bnames.v[bi]) || cast_to(ck, bd->path, bnames.v[ai]) ||
                cast_to(ck, ad->path, bnames.v[bi]))
                rl_add(ck, rl_checker_parse(ck, bd->path), b->line, "R1", "match",
                       rl_fmt(a, "'%.*s' is a layout view of '%.*s' (copies its first %zu members) read through a cast",
                              AVP_SF(bnames.v[bi]), AVP_SF(bnames.v[ai]), k),
                       (AvpStr){0});
        }
    }
    /* R2: one global declared with different types */
    typedef struct {
        AvpStr path;
        i32 line;
        AvpStr ty;
    } Decl;
    typedef RL_VEC(Decl) Decls;
    AvpMap *decls = avp_map_new();
    AvpStrVec dkeys = {0};
    for (size_t i = 0; i < ck->paths.n; i++) {
        RlParsed *P = rl_checker_parse(ck, ck->paths.v[i]);
        for (size_t k = 0; k < P->p.vars.n; k++) {
            const RlVar *v = &P->p.vars.v[k];
            if (v->is_static) continue;
            AvpStr key = rl_fmt(a, "%.*s\x01%.*s", AVP_SF(v->ns), AVP_SF(v->name));
            AvpStr ty = rl_strip_ws(a, v->type_text); /* the [] substitution that follows changes nothing */
            bool created = false;
            AvpVal *mv = avp_map_put(decls, key, &created);
            if (created) {
                mv->p = AVP_NEW(a, Decls);
                avp_vec_push(&dkeys, key);
            }
            *RL_PUSH(a, (Decls *)mv->p) = (Decl){ck->paths.v[i], v->line, ty};
        }
    }
    for (size_t di = 0; di < dkeys.n; di++) {
        Decls *lst = avp_map_get(decls, dkeys.v[di])->p;
        AvpSet *tys = avp_set_new();
        for (size_t k = 0; k < lst->n; k++) avp_set_add(tys, lst->v[k].ty);
        if (avp_set_len(tys) > 1) {
            AvpStrVec tv = avp_set_sorted(tys);
            AvpStr joined = avp_str_join(a, AVP_S(" vs "), tv.v, tv.n);
            AvpStr before, name;
            avp_str_partition(dkeys.v[di], AVP_S("\x01"), &before, &name);
            for (size_t k = 0; k < lst->n; k++) {
                if (!avp_set_has(ck->checked_set, lst->v[k].path)) continue;
                rl_add(ck, rl_checker_parse(ck, lst->v[k].path), lst->v[k].line, "R2", "match",
                       rl_fmt(a, "global '%.*s' is declared with different types (%.*s)", AVP_SF(name), AVP_SF(joined)),
                       (AvpStr){0});
            }
            avp_vec_free(&tv);
        }
        avp_set_free(tys);
    }
    (void)join_sorted;
}
