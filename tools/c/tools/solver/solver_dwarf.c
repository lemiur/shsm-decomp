/* solver_dwarf: Phase 2's pure readings. The `dtk dwarf dump` text of our -sym on object (variable -> register),
 * sibling.exe's JSON of the sibling's DWARF, the locals comparison (missing / extra / order / scope / type), the
 * declaration-order rule of docs/compiler_regalloc.md as a prediction the tool checks against our own registers,
 * and the candidate filter. Nothing here compiles or reads files. */
#include "solver.h"

const char *const SV_LC_KEYS[6] = {"dwarf-local-missing", "dwarf-local-extra", "dwarf-order", "dwarf-scope",
                                   "dwarf-type",          "dwarf-typedef"};

/* ---- registers ------------------------------------------------------------------------------------------------- */

int sv_reg_num(const char *loc) {
    if (!loc || (loc[0] != 'r' && loc[0] != 'f')) return -1;
    const char *p = loc + 1;
    if (!*p) return -1;
    int n = 0;
    for (; *p; p++) {
        if (*p < '0' || *p > '9') return -1;
        n = n * 10 + (*p - '0');
        if (n > 31) return -1;
    }
    return loc[0] == 'f' ? 100 + n : n;
}

bool sv_reg_callee_saved(int reg) {
    int n = reg >= 100 ? reg - 100 : reg;
    return reg >= 0 && n >= 14 && n <= 31;
}

/* ---- dtk dwarf dump -------------------------------------------------------------------------------------------- */

static bool idch(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }

/* The dtk dwarf dump parser (sv_dtk_dump_parse, sv_dtk_find, sv_vars_find) is lib/cc_dwdump.c. */

/* ---- sibling.exe fn --json ------------------------------------------------------------------------------------- */

static SvVar sib_var(AvpArena *a, const AvpJson *o, int depth, const char *block, i64 index, bool param) {
    SvVar v = {0};
    v.name = avp_cz(a, avp_json_get_str(o, "name", AVP_S("")));
    v.type = avp_cz(a, avp_json_get_str(o, "type", AVP_S("")));
    v.loc = avp_cz(a, avp_json_get_str(o, "location", AVP_S("")));
    v.line = avp_json_get_i64(o, "line", 0);
    v.depth = depth;
    v.block = block;
    v.index = index;
    v.is_param = param;
    return v;
}

static void sib_locals(AvpArena *a, const AvpJson *arr, int depth, const char *block, SvVars *out) {
    for (size_t i = 0; arr && i < avp_json_len(arr); i++) {
        const AvpJson *o = avp_json_at(arr, i);
        const AvpJson *b = avp_json_getz(o, "block");
        if (b) {
            const char *key = avp_cz(a, avp_json_str_or(b, AVP_S("?")));
            sib_locals(a, avp_json_getz(o, "locals"), depth + 1, key, out);
            continue;
        }
        avp_vec_push(out, sib_var(a, o, depth, block, (i64)out->n, false));
    }
}

bool sv_sibling_parse(AvpArena *a, AvpStr json, SvSibFn *out) {
    memset(out, 0, sizeof *out);
    AvpJsonError err = {0};
    AvpJson *doc = avp_json_parse(a, json, &err);
    if (!doc || !avp_json_is_obj(doc)) return false;
    const AvpJson *f = avp_json_getz(doc, "function");
    if (!f) f = doc;
    if (!avp_json_is_obj(f) || !avp_json_getz(f, "name")) return false;
    out->name = avp_cz(a, avp_json_get_str(f, "name", AVP_S("")));
    AvpStr tier = avp_json_get_str(f, "tier", AVP_S(""));
    out->tier = tier.n ? avp_cz(a, tier) : NULL;
    out->offsets_reliable = avp_json_get_bool(f, "offsets_reliable", false);
    const AvpJson *ps = avp_json_getz(f, "parameters");
    for (size_t i = 0; ps && i < avp_json_len(ps); i++)
        avp_vec_push(&out->params, sib_var(a, avp_json_at(ps, i), 0, NULL, (i64)i, true));
    sib_locals(a, avp_json_getz(f, "locals"), 0, NULL, &out->locals);
    const AvpJson *od = avp_json_getz(f, "locals_only_in_debug");
    for (size_t i = 0; od && i < avp_json_len(od); i++)
        avp_vec_push(&out->only_in_debug, avp_cz(a, avp_json_str_or(avp_json_at(od, i), AVP_S(""))));
    out->ok = true;
    return true;
}

/* ---- the locals comparison ------------------------------------------------------------------------------------- */

static const char *type_norm(AvpArena *a, const char *t) {
    AvpBuf b;
    avp_buf_init(&b, a);
    const char *p = t ? t : "";
    while (*p) {
        if (idch(*p)) {
            const char *q = p;
            while (idch(*q)) q++;
            size_t n = (size_t)(q - p);
            bool kw = (n == 6 && !strncmp(p, "struct", 6)) || (n == 5 && !strncmp(p, "class", 5)) ||
                      (n == 5 && !strncmp(p, "union", 5)) || (n == 4 && !strncmp(p, "enum", 4));
            if (!kw) {
                if (b.n && idch(b.p[b.n - 1])) avp_buf_putc(&b, ' ');
                avp_buf_put(&b, avp_strn(p, n));
            }
            p = q;
        } else {
            if (*p != ' ' && *p != '\t') avp_buf_putc(&b, *p);
            p++;
        }
    }
    return b.p ? b.p : "";
}

bool sv_types_same(const char *x, const char *y) {
    AvpArena *a = avp_scratch();
    AvpArenaMark mk = avp_arena_mark(a);
    bool eq = strcmp(type_norm(a, x), type_norm(a, y)) == 0;
    avp_arena_reset(a, mk);
    return eq;
}

SvVars sv_dwarf_union(AvpArena *a, const SvVars *final_, const SvVars *debug) {
    AVP_UNUSED(a);
    SvVars u = {0};
    for (size_t i = 0; final_ && i < final_->n; i++)
        if (!sv_vars_find(&u, final_->v[i].name)) avp_vec_push(&u, final_->v[i]);
    for (size_t i = 0; debug && i < debug->n; i++)
        if (!sv_vars_find(&u, debug->v[i].name)) avp_vec_push(&u, debug->v[i]);
    return u;
}

static void add_finding(SvLcFindings *out, SvLcKind k, const char *name, const char *other, const char *text,
                        const SvVar *dw) {
    SvLcFinding f = {k, name, other, text, dw};
    avp_vec_push(out, f);
}

/* Where the DWARF entry comes from: "Final" or "Debug". */
static const char *which(const SvVars *final_, const char *name) {
    return sv_vars_find(final_, name) ? "Final" : "Debug";
}

/* Ours is one identifier that is no builtin type spelling (a typedef name) and the DWARF spells a struct, class, union,
 * enum or pointer type: the two differ by a typedef the DWARF has already resolved, so no type is compared. */
static bool typedef_only(const char *ours, const char *dwarf) {
    static const char *const BUILTIN[] = {"int",  "char", "short",  "long",     "float",   "double",
                                          "void", "bool", "signed", "unsigned", "wchar_t", "_Bool"};
    if (!ours || !*ours || !dwarf || !*dwarf) return false;
    for (const char *p = ours; *p; p++)
        if (!idch(*p)) return false;
    for (size_t i = 0; i < AVP_ARRAY_LEN(BUILTIN); i++)
        if (!strcmp(ours, BUILTIN[i])) return false;
    return strchr(dwarf, '*') != NULL || !strncmp(dwarf, "struct ", 7) || !strncmp(dwarf, "class ", 6) ||
           !strncmp(dwarf, "union ", 6) || !strncmp(dwarf, "enum ", 5);
}

SvLcFindings sv_locals_compare(AvpArena *a, const SvVars *ours, const SvVars *final_, const SvVars *debug,
                               const SvVars *our_types) {
    SvLcFindings out = {0};
    SvVars dw = sv_dwarf_union(a, final_, debug);
    /* missing: the DWARF lists it, ours does not declare it */
    for (size_t i = 0; i < dw.n; i++) {
        const SvVar *d = &dw.v[i];
        if (!*d->name || sv_vars_find(ours, d->name)) continue;
        add_finding(&out, SV_LC_MISSING, d->name, NULL,
                    avp_sprintf(a, "`%s` (%s, line %lld%s): the sibling's DWARF (%s) lists it, ours does not declare it",
                                d->name, d->type, (long long)d->line, d->depth ? ", in a block" : "",
                                which(final_, d->name)),
                    sv_vars_find(&dw, d->name));
    }
    /* extra: ours declares it, neither build lists it */
    for (size_t i = 0; i < ours->n; i++) {
        const SvVar *o = &ours->v[i];
        if (sv_vars_find(&dw, o->name)) continue;
        add_finding(&out, SV_LC_EXTRA, o->name, NULL,
                    avp_sprintf(a, "`%s`: ours declares it, the sibling's DWARF lists it in neither %s", o->name,
                                debug ? "the Final nor the Debug build" : "the Final build (Debug not read)"),
                    NULL);
    }
    /* scope and type of the common locals */
    for (size_t i = 0; i < ours->n; i++) {
        const SvVar *o = &ours->v[i];
        const SvVar *d = sv_vars_find(&dw, o->name);
        if (!d) continue;
        if ((o->depth == 0) != (d->depth == 0))
            add_finding(&out, SV_LC_SCOPE, o->name, NULL,
                        avp_sprintf(a, "`%s`: the DWARF (%s) has it %s, ours %s", o->name, which(final_, o->name),
                                    d->depth ? "in a block" : "at function scope",
                                    o->depth ? "in a block" : "at function scope"),
                        d);
        if (d->type && *d->type && o->type && !sv_types_same(o->type, d->type)) {
            const SvVar *ot = our_types ? sv_vars_find(our_types, o->name) : NULL;
            if (!ot && typedef_only(o->type, d->type))
                add_finding(&out, SV_LC_TYPEDEF, o->name, d->type,
                            avp_sprintf(a, "`%s`: ours `%s` is a typedef name, the DWARF (%s) spells it `%s`: typedef "
                                           "(not compared)",
                                        o->name, o->type, which(final_, o->name), d->type),
                            d);
            else if (!ot || !sv_types_same(ot->type, d->type))
                add_finding(&out, SV_LC_TYPE, o->name, d->type,
                            avp_sprintf(a, "`%s`: the DWARF (%s) types it `%s`, ours `%s`", o->name,
                                        which(final_, o->name), d->type, o->type),
                            d);
        }
    }
    /* order: pairs at the same scope in both (function scope, or one block in each), the DWARF by line */
    size_t norder = 0;
    for (size_t i = 0; i < ours->n && norder < 8; i++)
        for (size_t j = i + 1; j < ours->n && norder < 8; j++) {
            const SvVar *x = &ours->v[i], *y = &ours->v[j];
            const SvVar *dx = sv_vars_find(&dw, x->name), *dy = sv_vars_find(&dw, y->name);
            if (!dx || !dy || dx->line <= 0 || dy->line <= 0) continue;
            bool same_ours = (x->depth == 0 && y->depth == 0) ||
                             (x->depth && y->depth && x->block && y->block && !strcmp(x->block, y->block));
            bool same_dw = (dx->depth == 0 && dy->depth == 0) ||
                           (dx->depth && dy->depth && dx->block && dy->block && !strcmp(dx->block, dy->block));
            if (!same_ours || !same_dw) continue;
            if (dy->line < dx->line) {
                norder++;
                add_finding(&out, SV_LC_ORDER, y->name, x->name,
                            avp_sprintf(a, "`%s` before `%s`: the DWARF (%s) declares `%s` at line %lld and `%s` at "
                                           "line %lld; ours declares `%s` first",
                                        y->name, x->name, which(final_, y->name), y->name, (long long)dy->line,
                                        x->name, (long long)dx->line, x->name),
                            dy);
            }
        }
    avp_vec_free(&dw);
    return out;
}

/* ---- the declaration-order rule -------------------------------------------------------------------------------- */

int sv_order_model(AvpArena *a, const char *const *names, const int *regs, size_t n, const char **why) {
    *why = NULL;
    int seen = 0;
    for (int cls = 0; cls < 2; cls++) {
        int last = -1;
        const char *last_name = NULL;
        size_t cnt = 0;
        for (size_t i = 0; i < n; i++) {
            int r = regs[i];
            if (!sv_reg_callee_saved(r) || (r >= 100) != (cls == 1)) continue;
            cnt++;
            if (last >= 0 && r >= last) {
                *why = avp_sprintf(a, "`%s` is declared before `%s` but holds %c%d, below %c%d", last_name, names[i],
                                   cls ? 'f' : 'r', last % 100, cls ? 'f' : 'r', r % 100);
                return 0;
            }
            last = r;
            last_name = names[i];
        }
        if (cnt >= 2) seen = 1;
    }
    if (!seen) {
        *why = "fewer than two locals hold callee-saved registers";
        return -1;
    }
    return 1;
}

AvpCzVec sv_implied_order(AvpArena *a, const char *const *names, const int *regs, size_t n) {
    AVP_UNUSED(a);
    AvpCzVec out = {0};
    for (int cls = 0; cls < 2; cls++) {
        size_t start = out.n;
        for (size_t i = 0; i < n; i++)
            if (sv_reg_callee_saved(regs[i]) && (regs[i] >= 100) == (cls == 1)) avp_vec_push(&out, (char *)names[i]);
        /* sort this class by register, highest first (stable insertion sort; names are unique) */
        for (size_t i = start + 1; i < out.n; i++)
            for (size_t j = i; j > start; j--) {
                int rj = -1, rp = -1;
                for (size_t k = 0; k < n; k++) {
                    if (names[k] == out.v[j]) rj = regs[k];
                    if (names[k] == out.v[j - 1]) rp = regs[k];
                }
                if (rp >= rj) break;
                char *t = out.v[j];
                out.v[j] = out.v[j - 1];
                out.v[j - 1] = t;
            }
    }
    return out;
}

/* ---- the candidate filter -------------------------------------------------------------------------------------- */

static size_t count_of(LvS s, const char *needle) {
    size_t c = 0, n = strlen(needle);
    for (size_t i = 0; i + n <= s.n; i++)
        if (!memcmp(s.p + i, needle, n)) c++;
    return c;
}

AvpCzVec sv_candidate_rejects_with(AvpArena *a, const LvCounter *orig_forms, LvS orig_fn, LvS cand_fn,
                                   const char *symbol) {
    AvpCzVec why = {0};
    AvpTry t;
    if (AVP_TRY(&t)) {
        LvCounter orig = orig_forms ? *orig_forms : lv_fake_forms(a, orig_fn, symbol);
        why = lv_fake_reasons(a, &orig, cand_fn, symbol);
        avp_try_end(&t);
    } else {
        avp_vec_push(&why, "unparsable");
    }
    if (count_of(cand_fn, "RULE-EXCEPTION") > count_of(orig_fn, "RULE-EXCEPTION")) avp_vec_push(&why, "RULE-EXCEPTION");
    if (count_of(cand_fn, "__typeof__") > count_of(orig_fn, "__typeof__")) avp_vec_push(&why, "__typeof__");
    if (count_of(cand_fn, "#pragma") > count_of(orig_fn, "#pragma")) {
        bool have = false;
        for (size_t i = 0; i < why.n; i++) have = have || !strcmp(why.v[i], "pragma");
        if (!have) avp_vec_push(&why, "pragma");
    }
    return why;
}

AvpCzVec sv_candidate_rejects(AvpArena *a, LvS orig_fn, LvS cand_fn, const char *symbol) {
    return sv_candidate_rejects_with(a, NULL, orig_fn, cand_fn, symbol);
}
