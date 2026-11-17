/* cc_dwdump: the `dtk dwarf dump` parser (cc_dwdump.h). Moved from the solver (Phase 2), which keeps its names
 * through solver.h. */
#include "cc_dwdump.h"

#include "shsm_cli.h"

static bool idch(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }

/* The declared name of a declaration text ("unsigned char usedBoneList[256]", "void (* cb)(int)", "int x"). */
static AvpStr decl_name(AvpStr d, size_t *name_at) {
    d = avp_str_strip(d);
    i64 e = (i64)d.n;
    while (e > 0 && d.p[e - 1] == ']') { /* array dimensions */
        i64 k = e - 1;
        while (k > 0 && d.p[k] != '[') k--;
        e = k;
        while (e > 0 && d.p[e - 1] == ' ') e--;
    }
    if (e > 0 && d.p[e - 1] == ')') { /* a function pointer: the name inside "(* name)" */
        i64 k = avp_str_find(d, AVP_S("(*"), 0, e);
        if (k >= 0) {
            i64 s = k + 2;
            while (s < e && d.p[s] == ' ') s++;
            i64 t = s;
            while (t < e && idch(d.p[t])) t++;
            if (name_at) *name_at = (size_t)s;
            return avp_str_slice(d, s, t);
        }
    }
    i64 s = e;
    while (s > 0 && idch(d.p[s - 1])) s--;
    if (name_at) *name_at = (size_t)s;
    return avp_str_slice(d, s, e);
}

static const char *type_part(AvpArena *a, AvpStr d) {
    d = avp_str_strip(d);
    size_t at = 0;
    AvpStr nm = decl_name(d, &at);
    if (!nm.n) return avp_cz(a, d);
    AvpStr t = avp_str_strip(avp_str_slice(d, 0, (i64)at));
    AvpStr rest = avp_str_strip(avp_str_slice(d, (i64)(at + nm.n), (i64)d.n));
    if (t.n && t.p[t.n - 1] == '(') t = avp_str_strip(avp_str_slice(t, 0, (i64)t.n - 1)); /* "void (*" */
    return rest.n ? avp_sprintf(a, "%.*s %.*s", AVP_SF(t), AVP_SF(rest)) : avp_cz(a, t);
}

static i64 parse_hex(AvpStr s) {
    s = avp_str_strip(s);
    if (avp_str_startswith(s, AVP_S("0x")) || avp_str_startswith(s, AVP_S("0X"))) s = avp_str_slice(s, 2, (i64)s.n);
    if (!s.n) return -1;
    i64 v = 0;
    for (size_t i = 0; i < s.n; i++) {
        char c = s.p[i];
        int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0) return -1;
        v = v * 16 + d;
    }
    return v;
}

/* The function name of a signature line: the identifier run (with ::, ~) before the '(' matching its last ')'. */
static const char *sig_name(AvpArena *a, AvpStr l, i64 *open_at) {
    i64 close = -1;
    for (i64 i = (i64)l.n - 1; i >= 0; i--)
        if (l.p[i] == ')') {
            close = i;
            break;
        }
    if (close < 0) return NULL;
    int depth = 0;
    i64 open = -1;
    for (i64 i = close; i >= 0; i--) {
        if (l.p[i] == ')') depth++;
        else if (l.p[i] == '(' && --depth == 0) {
            open = i;
            break;
        }
    }
    if (open <= 0) return NULL;
    i64 e = open, s = open;
    while (s > 0 && (idch(l.p[s - 1]) || l.p[s - 1] == ':' || l.p[s - 1] == '~')) s--;
    *open_at = open;
    return avp_cz(a, avp_str_slice(l, s, e));
}

CcDwFns cc_dtk_dump_parse(AvpArena *a, AvpStr text) {
    CcDwFns out = {0};
    AvpStrVec ls = avp_str_splitlines(text, false);
    i64 lo = -1, hi = -1;
    const char *this_loc = NULL;
    bool pending = false; /* a Range line was read: the next code line is a signature */
    CcDwFn *cur = NULL;
    for (size_t i = 0; i < ls.n; i++) {
        AvpStr l = avp_str_rstrip(ls.v[i]);
        AvpStr s = avp_str_strip(l);
        if (avp_str_startswith(s, AVP_S("// Range:"))) {
            AvpStr r = avp_str_slice(s, 9, (i64)s.n);
            i64 k = avp_str_find(r, AVP_S("->"), 0, (i64)r.n);
            lo = parse_hex(k >= 0 ? avp_str_slice(r, 0, k) : r);
            hi = k >= 0 ? parse_hex(avp_str_slice(r, k + 2, (i64)r.n)) : -1;
            this_loc = NULL;
            pending = true;
            cur = NULL;
            continue;
        }
        if (pending && avp_str_startswith(s, AVP_S("// this:"))) {
            this_loc = avp_cz(a, avp_str_strip(avp_str_slice(s, 8, (i64)s.n)));
            continue;
        }
        if (pending && s.n && !avp_str_startswith(s, AVP_S("//"))) {
            pending = false;
            i64 open = -1;
            const char *nm = sig_name(a, s, &open);
            if (!nm) continue;
            CcDwFn f = {0};
            f.name = nm;
            f.lo = lo;
            f.hi = hi;
            f.this_loc = this_loc;
            /* parameters: every "/\* LOC *\/" names the identifier before it */
            i64 pos = open + 1;
            i64 seg = pos;
            for (;;) {
                i64 c0 = avp_str_find(s, AVP_S("/*"), pos, (i64)s.n);
                if (c0 < 0) break;
                i64 c1 = avp_str_find(s, AVP_S("*/"), c0 + 2, (i64)s.n);
                if (c1 < 0) break;
                AvpStr decl = avp_str_slice(s, seg, c0);
                CcDwVar v = {0};
                v.name = avp_cz(a, decl_name(decl, NULL));
                v.type = type_part(a, decl);
                v.loc = avp_cz(a, avp_str_strip(avp_str_slice(s, c0 + 2, c1)));
                v.is_param = true;
                v.index = (i64)f.params.n;
                avp_vec_push(&f.params, v);
                pos = c1 + 2;
                i64 comma = avp_str_find(s, AVP_S(","), pos, (i64)s.n);
                seg = comma >= 0 ? comma + 1 : pos;
            }
            avp_vec_push(&out, f);
            cur = &out.v[out.n - 1];
            if (avp_str_endswith(s, AVP_S("{}"))) cur = NULL;
            continue;
        }
        if (!cur) continue;
        if (l.n && l.p[0] == '}') {
            cur = NULL;
            continue;
        }
        if (!s.n || avp_str_startswith(s, AVP_S("//"))) continue;
        i64 semi = avp_str_find(s, AVP_S("; //"), 0, (i64)s.n);
        if (semi < 0) continue;
        AvpStr decl = avp_str_slice(s, 0, semi);
        if (avp_str_startswith(decl, AVP_S("static "))) continue; /* a static local: not a register variable */
        CcDwVar v = {0};
        v.name = avp_cz(a, decl_name(decl, NULL));
        v.type = type_part(a, decl);
        v.loc = avp_cz(a, avp_str_strip(avp_str_slice(s, semi + 4, (i64)s.n)));
        v.index = (i64)cur->locals.n;
        if (*v.name) avp_vec_push(&cur->locals, v);
    }
    avp_vec_free(&ls);
    return out;
}

static AvpStr last_component(AvpStr s) {
    i64 k = avp_str_rfind(s, AVP_S("::"), 0, (i64)s.n);
    return k >= 0 ? avp_str_slice(s, k + 2, (i64)s.n) : s;
}

const CcDwFn *cc_dtk_find(const CcDwFns *fns, i64 lo, const char *name) {
    if (lo >= 0)
        for (size_t i = 0; i < fns->n; i++)
            if (fns->v[i].lo == lo) return &fns->v[i];
    if (!name) return NULL;
    AvpStr want = last_component(avp_str(name));
    const CcDwFn *hit = NULL;
    for (size_t i = 0; i < fns->n; i++)
        if (avp_str_eq(last_component(avp_str(fns->v[i].name)), want)) {
            if (hit) return NULL; /* ambiguous */
            hit = &fns->v[i];
        }
    return hit;
}

const CcDwVar *cc_dw_vars_find(const CcDwVars *v, const char *name) {
    for (size_t i = 0; v && i < v->n; i++)
        if (v->v[i].name && !strcmp(v->v[i].name, name)) return &v->v[i];
    return NULL;
}


bool cc_dwdump_run(AvpArena *a, const char *dtk, const char *obj, const char *txt, const char *cwd, AvpStr *text,
                   const char **why) {
    const char *argv[] = {dtk, "dwarf", "dump", obj, "-o", txt};
    ShsmRun res = {0};
    AvpTry t;
    if (AVP_TRY(&t)) {
        res = shsm_run(a, argv, AVP_ARRAY_LEN(argv), cwd, 120);
        avp_try_end(&t);
    } else {
        *why = avp_sprintf(a, "dtk dwarf dump could not run: %s", t.msg ? t.msg : "?");
        return false;
    }
    AvpBytes b = {0};
    if (!shsm_run_ok(&res) || !avp_read_all(a, txt, &b)) {
        *why = avp_sprintf(a, "dtk dwarf dump failed: %.*s", AVP_SF(avp_str_strip(shsm_run_output(a, &res))));
        return false;
    }
    *text = avp_strn((const char *)b.v, b.n);
    return true;
}
