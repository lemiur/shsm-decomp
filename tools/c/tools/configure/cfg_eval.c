/* configure.exe: execution of the parsed configure.py over pv.h values (cfg.h). */
#include <stdio.h>
#include <string.h>

#include "avp/fs.h"
#include "cfg.h"

typedef enum { FLOW_NORMAL, FLOW_RETURN, FLOW_BREAK, FLOW_CONTINUE } Flow;

typedef struct {
    Pv *locals; /* dict, NULL at module level */
    Pv *ret;
} Frame;

static Pv *eval(CfgInterp *I, Frame *f, CfgNode *e);
static Flow exec_block(CfgInterp *I, Frame *f, CfgNodes *body);

const char *cfg_type_name(const Pv *v) {
    if (v->kind == PV_OPAQUE) {
        if (strcmp(v->u.opaque.tag, "function") == 0) return "function";
        if (strcmp(v->u.opaque.tag, "builtin") == 0) return "builtin_function_or_method";
    }
    return pv_kind_name(v);
}

static Pv *lookup(CfgInterp *I, Frame *f, CfgNode *e) {
    Pv *v = NULL;
    if (f->locals) v = pv_dict_get(f->locals, e->name);
    if (!v) v = pv_dict_get(I->globals, e->name);
    if (!v) v = pv_dict_get(I->builtins, e->name);
    if (!v) cfg_fail(e->line, "name '%.*s' is not defined", AVP_SF(e->name));
    return v;
}

static void bind(CfgInterp *I, Frame *f, AvpStr name, Pv *v) { pv_dict_set(I->a, f->locals ? f->locals : I->globals, name, v); }

static void assign(CfgInterp *I, Frame *f, CfgNode *t, Pv *v) {
    switch (t->kind) {
    case CFG_E_NAME: bind(I, f, t->name, v); return;
    case CFG_E_ATTR: {
        Pv *obj = eval(I, f, t->a);
        if (obj->kind != PV_INSTANCE || strcmp(obj->u.inst->cls, "module") == 0)
            cfg_fail(t->line, "cannot set attribute '%.*s' of a %s", AVP_SF(t->name), cfg_type_name(obj));
        pv_setattr(I->a, obj, avp_cz(I->a, t->name), v);
        return;
    }
    case CFG_E_SUBSCR: {
        Pv *obj = eval(I, f, t->a);
        Pv *key = eval(I, f, t->b);
        if (obj->kind == PV_DICT) {
            if (key->kind != PV_STR) cfg_fail(t->line, "dict keys must be strings");
            pv_dict_set(I->a, obj, key->u.s, v);
            return;
        }
        if (obj->kind == PV_LIST && key->kind == PV_INT) {
            int64_t n = (int64_t)obj->u.list->n, k = key->u.i < 0 ? key->u.i + n : key->u.i;
            if (k < 0 || k >= n) cfg_fail(t->line, "list assignment index out of range");
            obj->u.list->v[k] = v;
            return;
        }
        cfg_fail(t->line, "'%s' object does not support item assignment", cfg_type_name(obj));
    }
    case CFG_E_TUPLE:
    case CFG_E_LIST: {
        Pv **items;
        size_t n;
        cfg_iterate(I, t->line, v, &items, &n);
        if (n != t->items.n)
            cfg_fail(t->line, n > t->items.n ? "too many values to unpack (expected %zu)" : "not enough values to unpack (expected %zu, got %zu)",
                     t->items.n, n);
        for (size_t i = 0; i < n; i++) assign(I, f, t->items.v[i], items[i]);
        return;
    }
    default: cfg_fail(t->line, "cannot assign to this expression");
    }
}

void cfg_iterate(CfgInterp *I, int line, Pv *v, Pv ***items, size_t *n) {
    switch (v->kind) {
    case PV_LIST:
    case PV_TUPLE: {
        /* a snapshot: a loop that appends to the list it walks would differ from Python (not in the subset) */
        *n = v->u.list->n;
        *items = avp_memdup(I->a, v->u.list->v, *n * sizeof(Pv *) + 1);
        return;
    }
    case PV_DICT: {
        *n = v->u.dict->n;
        *items = AVP_NEWN(I->a, Pv *, *n + 1);
        for (size_t i = 0; i < *n; i++) (*items)[i] = pv_str(I->a, v->u.dict->keys[i]);
        return;
    }
    case PV_STR: {
        Pv *l = pv_list(I->a);
        for (size_t i = 0; i < v->u.s.n;) {
            size_t j = i + 1;
            while (j < v->u.s.n && ((unsigned char)v->u.s.p[j] & 0xC0) == 0x80) j++;
            pv_list_push(I->a, l, pv_str(I->a, avp_strn(v->u.s.p + i, j - i)));
            i = j;
        }
        *n = l->u.list->n;
        *items = l->u.list->v;
        return;
    }
    default: cfg_fail(line, "'%s' object is not iterable", cfg_type_name(v));
    }
}

static void extend_star(CfgInterp *I, int line, Pv *out, Pv *v) {
    Pv **items;
    size_t n;
    cfg_iterate(I, line, v, &items, &n);
    for (size_t i = 0; i < n; i++) pv_list_push(I->a, out, items[i]);
}

static bool contains(CfgInterp *I, int line, Pv *container, Pv *x) {
    (void)I;
    switch (container->kind) {
    case PV_LIST:
    case PV_TUPLE:
        for (size_t i = 0; i < container->u.list->n; i++)
            if (pv_eq(container->u.list->v[i], x)) return true;
        return false;
    case PV_DICT: return x->kind == PV_STR && pv_dict_get(container, x->u.s) != NULL;
    case PV_STR:
        if (x->kind != PV_STR) cfg_fail(line, "'in <string>' requires string as left operand, not %s", cfg_type_name(x));
        return avp_str_find(container->u.s, x->u.s, 0, (i64)container->u.s.n) >= 0;
    default: cfg_fail(line, "argument of type '%s' is not iterable", cfg_type_name(container));
    }
}

static int order(CfgInterp *I, int line, Pv *x, Pv *y, const char *op) {
    (void)I;
    int64_t i, j;
    if ((x->kind == PV_INT || x->kind == PV_BOOL) && (y->kind == PV_INT || y->kind == PV_BOOL)) {
        i = x->kind == PV_BOOL ? x->u.b : x->u.i;
        j = y->kind == PV_BOOL ? y->u.b : y->u.i;
        return i < j ? -1 : i > j;
    }
    if (x->kind == PV_STR && y->kind == PV_STR) {
        size_t n = x->u.s.n < y->u.s.n ? x->u.s.n : y->u.s.n;
        int c = memcmp(x->u.s.p, y->u.s.p, n);
        if (c) return c < 0 ? -1 : 1;
        return x->u.s.n < y->u.s.n ? -1 : x->u.s.n > y->u.s.n;
    }
    cfg_fail(line, "'%s' not supported between instances of '%s' and '%s'", op, cfg_type_name(x), cfg_type_name(y));
}

static bool compare(CfgInterp *I, int line, int op, Pv *x, Pv *y) {
    switch (op) {
    case CFG_CMP_EQ: return pv_eq(x, y);
    case CFG_CMP_NE: return !pv_eq(x, y);
    case CFG_CMP_LT: return order(I, line, x, y, "<") < 0;
    case CFG_CMP_LE: return order(I, line, x, y, "<=") <= 0;
    case CFG_CMP_GT: return order(I, line, x, y, ">") > 0;
    case CFG_CMP_GE: return order(I, line, x, y, ">=") >= 0;
    case CFG_CMP_IN: return contains(I, line, y, x);
    case CFG_CMP_NOTIN: return !contains(I, line, y, x);
    case CFG_CMP_IS:
    case CFG_CMP_ISNOT: {
        bool same = x == y;
        /* CPython caches small ints and interns short strings; the subset compares those by value */
        if (!same && x->kind == PV_INT && y->kind == PV_INT && x->u.i == y->u.i && x->u.i >= -5 && x->u.i <= 256) same = true;
        return op == CFG_CMP_IS ? same : !same;
    }
    }
    return false;
}

static Pv *binop(CfgInterp *I, int line, int op, Pv *x, Pv *y) {
    AvpArena *a = I->a;
    if (op == '+') {
        if (x->kind == PV_INT && y->kind == PV_INT) return pv_int(a, x->u.i + y->u.i);
        if (x->kind == PV_STR && y->kind == PV_STR) {
            AvpBuf b;
            avp_buf_init(&b, a);
            avp_buf_put(&b, x->u.s);
            avp_buf_put(&b, y->u.s);
            return pv_str(a, avp_buf_str(&b));
        }
        if ((x->kind == PV_LIST && y->kind == PV_LIST) || (x->kind == PV_TUPLE && y->kind == PV_TUPLE)) {
            Pv *r = x->kind == PV_LIST ? pv_list(a) : pv_tuple(a);
            pv_list_extend(a, r, x);
            pv_list_extend(a, r, y);
            return r;
        }
    } else if (op == '-') {
        if (x->kind == PV_INT && y->kind == PV_INT) return pv_int(a, x->u.i - y->u.i);
    } else if (op == '*') {
        if (x->kind == PV_INT && y->kind == PV_INT) return pv_int(a, x->u.i * y->u.i);
    } else if (op == '/') {
        if (x->kind == PV_PATH && (y->kind == PV_STR || y->kind == PV_PATH)) {
            Pv *r = pv_path(a, AVP_S(""));
            r->u.s = pv_path_join(a, x->u.s, y->u.s);
            return r;
        }
        if (x->kind == PV_STR && y->kind == PV_PATH) {
            Pv *r = pv_path(a, AVP_S(""));
            r->u.s = pv_path_join(a, x->u.s, y->u.s);
            return r;
        }
        if (x->kind == PV_INT && y->kind == PV_INT) cfg_fail(line, "float division is not supported");
    }
    cfg_fail(line, "unsupported operand type(s) for %c: '%s' and '%s'", op, cfg_type_name(x), cfg_type_name(y));
}

static Pv *eval_call(CfgInterp *I, Frame *f, CfgNode *e) {
    Pv *fn = eval(I, f, e->a);
    Pv *pos = pv_list(I->a);
    AVP_VEC(AvpStr) kwn = {0};
    AVP_VEC(Pv *) kwv = {0};
    for (size_t i = 0; i < e->items.n; i++) {
        CfgNode *arg = e->items.v[i];
        if (arg->kind == CFG_E_KEYWORD) {
            for (size_t k = 0; k < kwn.n; k++)
                if (avp_str_eq(kwn.v[k], arg->name)) cfg_fail(arg->line, "keyword argument repeated: %.*s", AVP_SF(arg->name));
            avp_vec_push(&kwn, arg->name);
            avp_vec_push(&kwv, eval(I, f, arg->a));
        } else if (arg->kind == CFG_E_STARRED) {
            extend_star(I, arg->line, pos, eval(I, f, arg->a));
        } else {
            pv_list_push(I->a, pos, eval(I, f, arg));
        }
    }
    return cfg_call(I, e->line, fn, pos->u.list->v, pos->u.list->n, kwn.v, kwv.v, kwn.n);
}

static Pv *call_function(CfgInterp *I, int line, CfgFunction *fn, Pv **args, size_t n, AvpStr *kwnames, Pv **kwvals,
                         size_t nkw) {
    Frame fr = {pv_dict(I->a), pv_none()};
    CfgParams *ps = fn->params;
    size_t np = ps->names.n, nd = ps->defaults.n;
    if (n > np && !ps->star.p)
        cfg_fail(line, "%.*s() takes %zu positional arguments but %zu were given", AVP_SF(fn->name), np, n);
    Pv **slots = AVP_NEWN(I->a, Pv *, np + 1);
    for (size_t i = 0; i < np && i < n; i++) slots[i] = args[i];
    for (size_t k = 0; k < nkw; k++) {
        size_t i = 0;
        while (i < np && !avp_str_eq(ps->names.v[i], kwnames[k])) i++;
        if (i == np) cfg_fail(line, "%.*s() got an unexpected keyword argument '%.*s'", AVP_SF(fn->name), AVP_SF(kwnames[k]));
        if (slots[i]) cfg_fail(line, "%.*s() got multiple values for argument '%.*s'", AVP_SF(fn->name), AVP_SF(kwnames[k]));
        slots[i] = kwvals[k];
    }
    for (size_t i = 0; i < np; i++) {
        if (!slots[i] && i >= np - nd) slots[i] = fn->defaults[i - (np - nd)];
        if (!slots[i]) cfg_fail(line, "%.*s() missing required argument: '%.*s'", AVP_SF(fn->name), AVP_SF(ps->names.v[i]));
        pv_dict_set(I->a, fr.locals, ps->names.v[i], slots[i]);
    }
    if (ps->star.p) {
        Pv *rest = pv_tuple(I->a);
        for (size_t i = np; i < n; i++) pv_list_push(I->a, rest, args[i]);
        pv_dict_set(I->a, fr.locals, ps->star, rest);
    }
    Flow fl = exec_block(I, &fr, fn->body);
    if (fl == FLOW_BREAK || fl == FLOW_CONTINUE) cfg_fail(line, "'break' or 'continue' outside a loop");
    return fr.ret;
}

Pv *cfg_call(CfgInterp *I, int line, Pv *fn, Pv **args, size_t n, AvpStr *kwnames, Pv **kwvals, size_t nkw) {
    if (fn->kind == PV_OPAQUE && strcmp(fn->u.opaque.tag, "builtin") == 0) {
        CfgBuiltin *b = fn->u.opaque.p;
        return b->fn(I, line, b->self, args, n, kwnames, kwvals, nkw);
    }
    if (fn->kind == PV_OPAQUE && strcmp(fn->u.opaque.tag, "function") == 0)
        return call_function(I, line, fn->u.opaque.p, args, n, kwnames, kwvals, nkw);
    cfg_fail(line, "'%s' object is not callable", cfg_type_name(fn));
}

static Pv *subscript(CfgInterp *I, int line, Pv *obj, Pv *key) {
    if ((obj->kind == PV_LIST || obj->kind == PV_TUPLE) && (key->kind == PV_INT || key->kind == PV_BOOL)) {
        int64_t n = (int64_t)obj->u.list->n, k = key->kind == PV_BOOL ? key->u.b : key->u.i;
        if (k < 0) k += n;
        if (k < 0 || k >= n) cfg_fail(line, "%s index out of range", obj->kind == PV_LIST ? "list" : "tuple");
        return obj->u.list->v[k];
    }
    if (obj->kind == PV_DICT) {
        if (key->kind != PV_STR) cfg_fail(line, "dict keys must be strings");
        Pv *v = pv_dict_get(obj, key->u.s);
        if (!v) cfg_fail(line, "KeyError: %.*s", AVP_SF(pv_repr(I->a, key)));
        return v;
    }
    if (obj->kind == PV_STR && key->kind == PV_INT) {
        Pv **items;
        size_t n;
        cfg_iterate(I, line, obj, &items, &n);
        int64_t k = key->u.i < 0 ? key->u.i + (int64_t)n : key->u.i;
        if (k < 0 || k >= (int64_t)n) cfg_fail(line, "string index out of range");
        return items[k];
    }
    cfg_fail(line, "'%s' object is not subscriptable with '%s'", cfg_type_name(obj), cfg_type_name(key));
}

static Pv *eval(CfgInterp *I, Frame *f, CfgNode *e) {
    AvpArena *a = I->a;
    switch (e->kind) {
    case CFG_E_CONST: return e->value;
    case CFG_E_NAME: return lookup(I, f, e);
    case CFG_E_ATTR: return cfg_getattr(I, e->line, eval(I, f, e->a), e->name);
    case CFG_E_SUBSCR: {
        Pv *obj = eval(I, f, e->a);
        return subscript(I, e->line, obj, eval(I, f, e->b));
    }
    case CFG_E_CALL: return eval_call(I, f, e);
    case CFG_E_LIST:
    case CFG_E_TUPLE: {
        Pv *l = e->kind == CFG_E_LIST ? pv_list(a) : pv_tuple(a);
        for (size_t i = 0; i < e->items.n; i++) {
            CfgNode *it = e->items.v[i];
            if (it->kind == CFG_E_STARRED) extend_star(I, it->line, l, eval(I, f, it->a));
            else pv_list_push(a, l, eval(I, f, it));
        }
        return l;
    }
    case CFG_E_DICT: {
        Pv *d = pv_dict(a);
        for (size_t i = 0; i + 1 < e->items.n; i += 2) {
            Pv *k = eval(I, f, e->items.v[i]);
            if (k->kind != PV_STR) cfg_fail(e->items.v[i]->line, "dict keys must be strings, not %s", cfg_type_name(k));
            pv_dict_set(a, d, k->u.s, eval(I, f, e->items.v[i + 1]));
        }
        return d;
    }
    case CFG_E_FSTR: {
        AvpBuf b;
        avp_buf_init(&b, a);
        for (size_t i = 0; i < e->items.n; i++) {
            CfgNode *it = e->items.v[i];
            if (it->kind == CFG_E_CONST) {
                avp_buf_put(&b, it->value->u.s);
                continue;
            }
            Pv *v = eval(I, f, it->a);
            if (v->kind == PV_INSTANCE && it->op != 'r') cfg_fail(it->line, "str() of a %s instance is not supported", cfg_type_name(v));
            avp_buf_put(&b, it->op == 'r' ? pv_repr(a, v) : pv_to_str(a, v));
        }
        return pv_str(a, avp_buf_str(&b));
    }
    case CFG_E_BINOP: {
        Pv *x = eval(I, f, e->a);
        return binop(I, e->line, e->op, x, eval(I, f, e->b));
    }
    case CFG_E_UNARY: {
        Pv *x = eval(I, f, e->a);
        if (e->op == 'n') return pv_bool(!pv_truthy(x));
        if (x->kind == PV_INT || x->kind == PV_BOOL) {
            int64_t v = x->kind == PV_BOOL ? x->u.b : x->u.i;
            return pv_int(a, e->op == '-' ? -v : v);
        }
        cfg_fail(e->line, "bad operand type for unary %c: '%s'", e->op, cfg_type_name(x));
    }
    case CFG_E_BOOLOP: {
        Pv *v = NULL;
        for (size_t i = 0; i < e->items.n; i++) {
            v = eval(I, f, e->items.v[i]);
            if (e->op == 'a' ? !pv_truthy(v) : pv_truthy(v)) return v;
        }
        return v;
    }
    case CFG_E_COMPARE: {
        Pv *x = eval(I, f, e->a);
        for (size_t i = 0; i < e->items.n; i++) {
            Pv *y = eval(I, f, e->items.v[i]);
            if (!compare(I, e->line, e->ops.v[i], x, y)) return pv_bool(false);
            x = y;
        }
        return pv_bool(true);
    }
    case CFG_E_IFEXP: return pv_truthy(eval(I, f, e->b)) ? eval(I, f, e->a) : eval(I, f, e->c);
    case CFG_E_STARRED: cfg_fail(e->line, "can't use starred expression here");
    case CFG_E_KEYWORD: cfg_fail(e->line, "unexpected keyword argument");
    default: cfg_fail(e->line, "not an expression");
    }
}

static Flow exec_stmt(CfgInterp *I, Frame *f, CfgNode *s) {
    AvpArena *a = I->a;
    switch (s->kind) {
    case CFG_S_EXPR: (void)eval(I, f, s->a); return FLOW_NORMAL;
    case CFG_S_ASSIGN: {
        Pv *v = eval(I, f, s->a);
        for (size_t i = 0; i < s->items.n; i++) assign(I, f, s->items.v[i], v);
        return FLOW_NORMAL;
    }
    case CFG_S_ANNASSIGN:
        if (s->a) assign(I, f, s->items.v[0], eval(I, f, s->a));
        return FLOW_NORMAL;
    case CFG_S_IF:
        return exec_block(I, f, pv_truthy(eval(I, f, s->a)) ? &s->body : &s->orelse);
    case CFG_S_FOR: {
        Pv **items;
        size_t n;
        cfg_iterate(I, s->line, eval(I, f, s->a), &items, &n);
        for (size_t i = 0; i < n; i++) {
            assign(I, f, s->b, items[i]);
            Flow fl = exec_block(I, f, &s->body);
            if (fl == FLOW_RETURN) return fl;
            if (fl == FLOW_BREAK) break;
        }
        return FLOW_NORMAL;
    }
    case CFG_S_DEF: {
        CfgFunction *fn = AVP_NEW(a, CfgFunction);
        fn->name = s->name;
        fn->params = s->params;
        fn->body = &s->body;
        fn->defaults = AVP_NEWN(a, Pv *, s->params->defaults.n + 1);
        for (size_t i = 0; i < s->params->defaults.n; i++) fn->defaults[i] = eval(I, f, s->params->defaults.v[i]);
        bind(I, f, s->name, pv_opaque(a, "function", fn));
        return FLOW_NORMAL;
    }
    case CFG_S_RETURN:
        if (!f->locals) cfg_fail(s->line, "'return' outside function");
        f->ret = s->a ? eval(I, f, s->a) : pv_none();
        return FLOW_RETURN;
    case CFG_S_IMPORT:
        for (size_t i = 0; i < s->names.n; i++) {
            AvpStr mod = s->names.v[i];
            Pv *m = cfg_import(I, s->line, mod);
            if (s->asnames.v[i].p) {
                bind(I, f, s->asnames.v[i], m);
            } else {
                if (avp_str_find(mod, AVP_S("."), 0, (i64)mod.n) >= 0)
                    cfg_fail(s->line, "dotted 'import %.*s' is not supported (use 'from ... import')", AVP_SF(mod));
                bind(I, f, mod, m);
            }
        }
        return FLOW_NORMAL;
    case CFG_S_FROMIMPORT: {
        Pv *m = cfg_import(I, s->line, s->name);
        for (size_t i = 0; i < s->names.n; i++) {
            Pv *v = pv_getattr(m, avp_cz(a, s->names.v[i]));
            if (!v) cfg_fail(s->line, "cannot import name '%.*s' from '%.*s'", AVP_SF(s->names.v[i]), AVP_SF(s->name));
            bind(I, f, s->asnames.v[i].p ? s->asnames.v[i] : s->names.v[i], v);
        }
        return FLOW_NORMAL;
    }
    case CFG_S_PASS: return FLOW_NORMAL;
    case CFG_S_BREAK: return FLOW_BREAK;
    case CFG_S_CONTINUE: return FLOW_CONTINUE;
    default: cfg_fail(s->line, "not a statement");
    }
}

static Flow exec_block(CfgInterp *I, Frame *f, CfgNodes *body) {
    for (size_t i = 0; i < body->n; i++) {
        Flow fl = exec_stmt(I, f, body->v[i]);
        if (fl != FLOW_NORMAL) return fl;
    }
    return FLOW_NORMAL;
}

Pv *cfg_exec_source(AvpArena *a, const CfgOptions *opts, AvpStr src) {
    CfgInterp I = {0};
    I.a = a;
    I.opts = opts;
    I.globals = pv_dict(a);
    I.builtins = pv_dict(a);
    pv_dict_set(a, I.globals, AVP_S("__name__"), pv_strz(a, "__main__"));
    pv_dict_set(a, I.globals, AVP_S("__file__"), pv_strz(a, opts->script_path ? opts->script_path : ""));
    cfg_install_builtins(&I);
    CfgNodes stmts = cfg_parse_file(a, src);
    Frame top = {NULL, NULL};
    Flow fl = exec_block(&I, &top, &stmts);
    if (fl == FLOW_BREAK || fl == FLOW_CONTINUE) cfg_fail(1, "'break' or 'continue' outside a loop");
    return I.globals;
}

int cfg_run(AvpArena *a, const CfgOptions *opts) {
    AvpStr src;
    if (!avp_read_text(a, opts->script_path, AVP_TM_UTF8_SIG, &src)) {
        fflush(stdout);
        fprintf(stderr, "configure: error: cannot read %s\n", opts->script_path);
        return 2;
    }
    (void)cfg_exec_source(a, opts, src);
    return 0;
}
