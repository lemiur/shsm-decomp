/* rules_check_file: the per-file checks of check_rules.py outside the token stream and the function bodies:
 * check_directives (R3, R22, R37; L3428), check_define (R4, R17, R31; L3462), check_comments (R17, R21, R37; L3521),
 * check_asm (R18; L3536, with unit_key L1937) and check_declarations (R2, R13, R17; L3560). */
#include "rules_check.h"

#define TEQ RL_TEQ
static void add(RlChecker *ck, RlParsed *P, i32 line, const char *rule, const char *tier, AvpStr msg) {
    rl_add(ck, P, line, rule, tier, msg, (AvpStr){0});
}

static AvpStr first_word(AvpStr s) {
    AvpStrVec w = avp_str_split(s, AVP_S(""), 1);
    AvpStr r = w.n ? w.v[0] : AVP_S("");
    avp_vec_free(&w);
    return r;
}

/* ---- check_define ---- */
static void check_define(RlChecker *ck, RlParsed *P, const RlDirective *d) {
    const RlTok *toks = d->toks.v;
    size_t n = d->toks.n;
    if (!n || toks[0].k != RL_ID) return;
    AvpArena *a = ck->a;
    AvpStr mname = toks[0].t;
    AvpStr after = avp_str_lstrip_chars(d->rest, mname);
    bool fn_like = n > 1 && TEQ(&toks[1], "(") && toks[1].line == toks[0].line && after.n && after.p[0] == '(';
    size_t body_start = 1;
    size_t p0 = 0, p1 = 0; /* parameter token range */
    if (fn_like) {
        i32 *m = rl_pair_brackets(a, toks, n);
        if (m[1] >= 0) {
            p0 = 2;
            p1 = (size_t)m[1];
            body_start = (size_t)m[1] + 1;
        }
    }
    const RlTok *body = toks + body_start;
    size_t bn = body_start <= n ? n - body_start : 0;
    if (bn) {
        i32 *mm = rl_pair_brackets(a, body, bn);
        rl_check_r4(ck, P, body, bn, mm, true);
    }
    for (size_t q = 0; q < bn; q++) {
        if (TEQ(&body[q], "volatile") || rl_volatile_type(body[q].t)) {
            if (!rl_has_note(P, d->line, rl_re_volatile_note(), true) && !rl_has_note(P, d->end_line, rl_re_volatile_note(), true))
                add(ck, P, d->line, "R17", "match",
                    rl_fmt(a, "volatile in macro '%.*s' without a '// volatile: <evidence>' note", AVP_SF(mname)));
            break;
        }
    }
    if (fn_like && bn) {
        for (size_t pi = p0; pi < p1; pi++) {
            if (toks[pi].k != RL_ID) continue;
            AvpStr p = toks[pi].t;
            for (size_t q = 0; q < bn; q++) {
                const RlTok *x = &body[q];
                if (x->k != RL_ID || !avp_str_eq(x->t, p)) continue;
                AvpStr pv = q ? body[q - 1].t : AVP_S("");
                AvpStr nx = q + 1 < bn ? body[q + 1].t : AVP_S("");
                if (avp_str_eqz(pv, "#") || avp_str_eqz(pv, "##") || avp_str_eqz(nx, "##")) continue;
                if ((avp_str_eqz(pv, "(") || avp_str_eqz(pv, ",")) && (avp_str_eqz(nx, ")") || avp_str_eqz(nx, ",")))
                    continue;
                if (avp_str_eqz(pv, ".") || avp_str_eqz(pv, "->")) continue;
                add(ck, P, d->line, "R31", "free",
                    rl_fmt(a, "macro '%.*s': parameter '%.*s' is not parenthesised", AVP_SF(mname), AVP_SF(p)));
                break;
            }
        }
        bool semi = false;
        for (size_t q = 0; q < bn; q++) semi |= TEQ(&body[q], ";") || TEQ(&body[q], "{");
        if (!TEQ(&body[0], "(") && !semi && !TEQ(&body[0], "do") && !TEQ(&body[0], "if") && !TEQ(&body[0], "return")) {
            static const char *const OPS[] = {"+", "-", "*", "/", "%", "<<", ">>", "&", "|", "^", "?", "==", "!=",
                                              "<", ">", "&&", "||", NULL};
            i64 depth = 0;
            for (size_t q = 0; q < bn; q++) {
                const RlTok *x = &body[q];
                if (TEQ(x, "(") || TEQ(x, "[")) depth++;
                else if (TEQ(x, ")") || TEQ(x, "]")) depth--;
                else if (depth == 0 && rl_tin(x, OPS)) {
                    if (q == 0) continue;
                    add(ck, P, d->line, "R31", "free",
                        rl_fmt(a, "macro '%.*s': expansion is not parenthesised", AVP_SF(mname)));
                    break;
                }
            }
        }
    }
}

/* ---- check_directives ---- */
void rl_check_directives(RlChecker *ck, RlParsed *P) {
    AvpArena *a = ck->a;
    AvpStr status = rl_checker_status_of(ck, P->path);
    AvpStr low = avp_str_lower(a, P->path);
    if (avp_str_endswith(low, AVP_S(".h")) || avp_str_endswith(low, AVP_S(".hh")) || avp_str_endswith(low, AVP_S(".hpp")) ||
        avp_str_endswith(low, AVP_S(".hxx"))) {
        const RlDirectives *ds = &P->directives;
        bool guarded = false;
        if (ds->n) {
            AvpStr n0 = ds->v[0].name;
            guarded = (avp_str_eqz(n0, "pragma") && avp_str_eqz(first_word(ds->v[0].rest), "once")) ||
                      ((avp_str_eqz(n0, "ifndef") || avp_str_eqz(n0, "if")) && ds->n > 1 &&
                       avp_str_eqz(ds->v[1].name, "define"));
        }
        if (!guarded) add(ck, P, 1, "R3", "free", AVP_S("header without an include guard"));
    }
    const ShsmRe *nm = shsm_re("\\bNON_?MATCHING\\b", 0);
    const ShsmRe *inc = shsm_re("\\s*[<\"]([^>\"]+)[>\"]", 0);
    for (size_t di = 0; di < P->directives.n; di++) {
        const RlDirective *d = &P->directives.v[di];
        AvpStr name = d->name, rest = d->rest;
        if (avp_str_eqz(name, "if") || avp_str_eqz(name, "ifdef") || avp_str_eqz(name, "ifndef") ||
            avp_str_eqz(name, "elif")) {
            AvpStr cond = rl_strip_ws(a, rest);
            if (shsm_re_fullmatch(shsm_re("\\(?0\\)?", 0), cond, 0, AVP_END, NULL) ||
                shsm_re_fullmatch(shsm_re("\\(?1\\)?", 0), cond, 0, AVP_END, NULL)) {
                AvpStr r = avp_str_strip(rest);
                add(ck, P, d->line, "R37", "checked",
                    rl_fmt(a, "'#%.*s %.*s': dead or unconditional block is not source", AVP_SF(name), AVP_SF(r)));
            } else if (shsm_re_search(nm, rest, 0, AVP_END, NULL)) {
                const char *extra = avp_str_eqz(status, "Matching") ? " in a Matching unit" : "";
                add(ck, P, d->line, "R37", "checked",
                    rl_fmt(a, "NON_MATCHING parking via #%.*s%s: the project parks with Object(NonMatching)", AVP_SF(name),
                           extra));
            }
        }
        if (avp_str_eqz(name, "include")) {
            ShsmReMatch m;
            if (shsm_re_match(inc, rest, 0, AVP_END, &m)) {
                AvpStr g = shsm_re_group(&m, 1);
                if (avp_str_eqz(g, "stdint.h") || avp_str_eqz(g, "stdbool.h") || avp_str_eqz(g, "cstdint"))
                    add(ck, P, d->line, "R22", "free", rl_fmt(a, "<%.*s> is not in the era's SDK headers", AVP_SF(g)));
            }
            continue;
        }
        if (avp_str_eqz(name, "pragma")) continue;
        for (size_t q = 0; q < d->toks.n; q++)
            if (d->toks.v[q].k == RL_ID) rl_check_identifier(ck, P, &d->toks.v[q]);
        if (avp_str_eqz(name, "define")) check_define(ck, P, d);
    }
}

/* ---- check_comments ---- */
void rl_check_comments(RlChecker *ck, RlParsed *P) {
    const ShsmRe *probe = shsm_re("^\\s*(?i:probe|steer(?:ing)?|fakematch|fake-match|match hack)\\b\\s*[:\\-]|\\bPROBE\\b|"
                                  "\\bFAKEMATCH\\b|\\bfakematch\\b",
                                  0);
    const ShsmRe *addr = shsm_re("\\b0x8[0-9A-Fa-f]{7}\\b", 0);
    const ShsmRe *annot = shsm_re("^\\s*0x8[0-9A-Fa-f]{7}\\s*$", 0);
    const ShsmRe *hist = shsm_re("\\b20\\d\\d[-/]\\d\\d[-/]\\d\\d\\b|\\b\\d{1,3}(?:\\.\\d+)?\\s*%|\\b(?:permuter|objdiff|"
                                 "plateau)\\b|\\bmatch(?:ed)?\\s+(?:at|to)\\s+\\d",
                                 SHSM_RE_I);
    const ShsmRe *nm = shsm_re("\\bNON_?MATCHING\\b", 0);
    bool matching = avp_str_eqz(rl_checker_status_of(ck, P->path), "Matching");
    for (size_t i = 0; i < P->comments.n; i++) {
        const RlComment *c = &P->comments.v[i];
        AvpStr text = c->text;
        if (shsm_re_search(probe, text, 0, AVP_END, NULL))
            add(ck, P, c->line, "R17", "match", AVP_S("statement/comment marked as a probe or fakematch"));
        if (shsm_re_search(addr, text, 0, AVP_END, NULL) && !shsm_re_match(annot, text, 0, AVP_END, NULL))
            add(ck, P, c->line, "R21", "free", AVP_S("address in a comment (only the '// 0x8XXXXXXX' annotation is allowed)"));
        if (shsm_re_search(hist, text, 0, AVP_END, NULL))
            add(ck, P, c->line, "R21", "free",
                AVP_S("history, score or tooling note in a comment: that belongs in version control"));
        if (shsm_re_search(rl_re_exception(), text, 0, AVP_END, NULL)) continue;
        if (shsm_re_search(nm, text, 0, AVP_END, NULL) && matching)
            add(ck, P, c->line, "R37", "checked", AVP_S("NONMATCHING marker in a Matching unit"));
    }
}

/* ---- check_asm ---- */
AvpStr rl_unit_key(AvpArena *a, AvpStr path) {
    AvpStr p = avp_str_replace(a, path, AVP_S("\\"), AVP_S("/"), -1);
    if (avp_str_startswith(p, AVP_S("src/"))) p = avp_str_slice(p, 4, p.n);
    AvpStr root, ext;
    avp_path_splitext(avp_cz(a, p), &root, &ext);
    return root;
}

static AvpStr short_name(AvpStr name) {
    AvpStr before, after;
    while (avp_str_partition(name, AVP_S("::"), &before, &after)) name = after;
    return name;
}

/* The function whose body holds the only call of the helper funcs[h] in this file, or -1 (no call, or several).
 * Calls are counted inside function bodies other than the helper's own. */
static i64 sole_caller(const RlParsed *P, i64 h) {
    const RlFunc *hf = &P->p.funcs.v[h];
    AvpStr nm = short_name(hf->name);
    i64 caller = -1;
    int calls = 0;
    for (size_t fi = 0; fi < P->p.funcs.n; fi++) {
        const RlFunc *f = &P->p.funcs.v[fi];
        if ((i64)fi == h) continue;
        for (i64 k = f->body + 1; k < f->body_end; k++)
            if (P->toks.v[k].k == RL_ID && avp_str_eq(P->toks.v[k].t, nm) && TEQ(&P->toks.v[k + 1], "(")) {
                calls++;
                caller = (i64)fi;
            }
    }
    return calls == 1 ? caller : -1;
}

static bool marker_above(const RlParsed *P, i32 base) {
    const ShsmRe *marker = shsm_re("\\b(?:ORIGINAL-ASM|ASM-HELPER)\\b", 0);
    for (i32 ln = base - 6 > 1 ? base - 6 : 1; ln <= base; ln++) {
        size_t nc;
        const i32 *ix = rl_comments_on(P, ln, &nc);
        for (size_t q = 0; q < nc; q++)
            if (shsm_re_search(marker, P->comments.v[ix[q]].text, 0, AVP_END, NULL)) return true;
    }
    return false;
}

/* R18 (rulings of 2026-11-14 and 2026-11-15). A finding belongs to the function whose own body holds assembly:
 * an `asm` function, an `asm { }` block, or a helper defined in the same source file and called once (its block
 * is the caller's). A header-level `asm` inline is a helper of the original: listed under ORIGINAL-ASM by its
 * header path it is allowed and its callers are not findings; unlisted it is "helper not listed as ORIGINAL-ASM"
 * (no source marker or helper-table row excuses it). */
void rl_check_asm(RlChecker *ck, RlParsed *P) {
    AvpArena *a = ck->a;
    AvpStr unit = rl_unit_key(a, P->path);
    AvpStr src = ck->cfg.allow_source.n ? ck->cfg.allow_source : AVP_S("the allow list");
    for (size_t i = 0; i < P->asm_sites.n; i++) {
        const RlAsmSite *site = &P->asm_sites.v[i];
        AvpStr fname = site->name;
        AvpStr sh = fname.n ? short_name(fname) : AVP_S("");
        AvpStr key = rl_fmt(a, "%.*s\x01%.*s", AVP_SF(unit), AVP_SF(sh));
        i64 fx = rl_func_at(P, site->line);
        if (P->is_header && fname.n && fx >= 0) {
            bool listed = avp_set_has(ck->cfg.original_asm, key);
            AvpStr msg = listed ? rl_fmt(a, "assembly body: inline-asm helper '%.*s' in a header", AVP_SF(sh))
                                : rl_fmt(a,
                                         "helper not listed as ORIGINAL-ASM: header-level asm inline '%.*s' (list it "
                                         "by its header path, or move the assembly into its caller)",
                                         AVP_SF(sh));
            i64 fi = rl_add(ck, P, site->line, "R18", "match", msg, fname);
            if (listed && fi >= 0) {
                ck->findings.v[fi].status = "allowed";
                ck->findings.v[fi].reason = rl_fmt(a, "ORIGINAL-ASM listed in %.*s", AVP_SF(src));
            }
            continue;
        }
        /* an `asm` function, or a body listed as ORIGINAL-ASM, keeps its own finding */
        bool own = avp_str_eqz(site->kind, "function") || avp_set_has(ck->cfg.original_asm, key);
        i64 caller = (!P->is_header && !own && fname.n && fx >= 0) ? sole_caller(P, fx) : -1;
        AvpStr owner = fname, msg;
        i32 line = site->line;
        if (caller >= 0) {
            const RlFunc *cf = &P->p.funcs.v[caller];
            owner = cf->name;
            for (i64 k = cf->body + 1; k < cf->body_end; k++)
                if (P->toks.v[k].k == RL_ID && avp_str_eq(P->toks.v[k].t, sh) && TEQ(&P->toks.v[k + 1], "(")) {
                    line = P->toks.v[k].line;
                    break;
                }
            msg = rl_fmt(a,
                         "assembly body: inline-asm helper '%.*s' (line %d) is called only here, so its assembly is "
                         "this function's own",
                         AVP_SF(sh), (int)P->p.funcs.v[fx].line);
        } else
            msg = avp_str_eqz(site->kind, "function")
                      ? rl_fmt(a, "assembly body: function '%.*s' is written in assembly", AVP_SF(sh))
                      : rl_fmt(a, "assembly body: inline assembly in '%.*s'", sh.n ? (int)sh.n : 10,
                               sh.n ? sh.p : "file scope");
        i64 fi = rl_add(ck, P, line, "R18", "match", msg, owner.n ? owner : (AvpStr){0});
        AvpStr okey = caller >= 0 ? rl_fmt(a, "%.*s\x01%.*s", AVP_SF(unit), AVP_SF(short_name(owner))) : key;
        AvpStr allowed = {0};
        if (avp_set_has(ck->cfg.original_asm, key) || avp_set_has(ck->cfg.original_asm, okey)) {
            allowed = rl_fmt(a, "ORIGINAL-ASM listed in %.*s", AVP_SF(src));
        } else if (avp_set_has(ck->cfg.asm_helpers, key) || avp_set_has(ck->cfg.asm_helpers, okey)) {
            allowed = rl_fmt(a, "listed inline-asm helper in %.*s", AVP_SF(src));
        } else {
            bool hit = marker_above(P, fx >= 0 ? P->p.funcs.v[fx].line : site->line);
            if (!hit && caller >= 0) hit = marker_above(P, P->p.funcs.v[caller].line);
            if (hit) allowed = AVP_S("ORIGINAL-ASM/ASM-HELPER marker in the source");
        }
        if (allowed.n && fi >= 0) {
            ck->findings.v[fi].status = "allowed";
            ck->findings.v[fi].reason = allowed;
        }
    }
}

/* ---- check_declarations ---- */
void rl_check_declarations(RlChecker *ck, RlParsed *P) {
    AvpArena *a = ck->a;
    bool is_hdr = P->is_header;
    for (size_t i = 0; i < P->p.vars.n; i++) {
        const RlVar *v = &P->p.vars.v[i];
        if (!is_hdr && v->is_extern && !v->has_init)
            add(ck, P, v->line, "R2", "match",
                rl_fmt(a, "extern declaration of '%.*s' outside a header: declare it once in a header", AVP_SF(v->name)));
        if (is_hdr && !(v->is_extern || v->is_static || v->is_const || v->addr_bound))
            add(ck, P, v->line, "R2", "match",
                rl_fmt(a, "definition of '%.*s' in a header: define it once, in its owning unit", AVP_SF(v->name)));
    }
    const ShsmRe *pad = shsm_re("_{0,2}(?:pad|padding)\\d*", 0);
    for (size_t i = 0; i < P->p.aggs.n; i++) {
        const RlAgg *g = &P->p.aggs.v[i];
        for (size_t q = 0; q < g->member_decls.n; q++) {
            const RlMemberDecl *m = &g->member_decls.v[q];
            if (shsm_re_fullmatch(pad, m->name, 0, AVP_END, NULL) && !rl_has_note(P, m->line, rl_re_pad_note(), true))
                add(ck, P, m->line, "R17", "match",
                    rl_fmt(a, "padding member '%.*s' (use placeholder members named by offset, or note the SDK/format name)",
                           AVP_SF(m->name)));
        }
    }
    const ShsmRe *dummy = shsm_re("(?i)(?:pad|dummy)\\w*", 0);
    for (size_t i = 0; i < P->p.virtuals.n; i++) {
        AvpStr sh = short_name(P->p.virtuals.v[i].name);
        if (shsm_re_fullmatch(dummy, sh, 0, AVP_END, NULL))
            add(ck, P, P->p.virtuals.v[i].line, "R13", "match",
                rl_fmt(a, "padding/dummy virtual '%.*s': vtables never contain dummy slots", AVP_SF(sh)));
    }
}
