/* configure.exe: the built-ins, modules and methods of the configure.py subset (DESIGN.md section 1, cfg.h). */
#include <stdio.h>
#include <string.h>

#include "avp/args.h"
#include "avp/fmt.h"
#include "avp/fs.h"
#include "cfg.h"

/* ---- helpers ---- */

Pv *cfg_builtin(AvpArena *a, const char *name, CfgBuiltinFn fn, Pv *self, void *data) {
    CfgBuiltin *b = AVP_NEW(a, CfgBuiltin);
    b->name = name;
    b->fn = fn;
    b->self = self;
    b->data = data;
    return pv_opaque(a, "builtin", b);
}

static bool is_builtin(const Pv *v, const char *name) {
    return v->kind == PV_OPAQUE && strcmp(v->u.opaque.tag, "builtin") == 0 &&
           strcmp(((CfgBuiltin *)v->u.opaque.p)->name, name) == 0;
}

static bool is_tag(const Pv *v, const char *tag) { return v->kind == PV_OPAQUE && strcmp(v->u.opaque.tag, tag) == 0; }

#define ARGS CfgInterp *I, int line, Pv *self, Pv **args, size_t n, AvpStr *kwnames, Pv **kwvals, size_t nkw
#define UNUSED_ARGS (void)I, (void)line, (void)self, (void)args, (void)n, (void)kwnames, (void)kwvals, (void)nkw

static void no_kwargs(int line, const char *fn, size_t nkw, AvpStr *kwnames) {
    if (nkw) cfg_fail(line, "%s() got an unexpected keyword argument '%.*s'", fn, AVP_SF(kwnames[0]));
}

static void nargs_between(int line, const char *fn, size_t n, size_t lo, size_t hi) {
    if (n < lo || n > hi) {
        if (lo == hi) cfg_fail(line, "%s() takes exactly %zu argument%s (%zu given)", fn, lo, lo == 1 ? "" : "s", n);
        cfg_fail(line, "%s() takes from %zu to %zu arguments (%zu given)", fn, lo, hi, n);
    }
}

static AvpStr want_str(int line, const char *what, const Pv *v) {
    if (v->kind != PV_STR) cfg_fail(line, "%s must be a str, not %s", what, cfg_type_name(v));
    return v->u.s;
}

static Pv *new_path(AvpArena *a, AvpStr form) {
    Pv *p = pv_path(a, AVP_S(""));
    p->u.s = form;
    return p;
}

/* ---- the str type and its methods ---- */

static Pv *str_upper(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "upper", nkw, kwnames);
    Pv *s = self;
    if (!s) { /* str.upper(x) */
        nargs_between(line, "upper", n, 1, 1);
        s = args[0];
    } else {
        nargs_between(line, "upper", n, 0, 0);
    }
    return pv_str(I->a, avp_str_upper(I->a, want_str(line, "descriptor 'upper'", s)));
}

static Pv *str_lower(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "lower", nkw, kwnames);
    Pv *s = self;
    if (!s) {
        nargs_between(line, "lower", n, 1, 1);
        s = args[0];
    } else {
        nargs_between(line, "lower", n, 0, 0);
    }
    return pv_str(I->a, avp_str_lower(I->a, want_str(line, "descriptor 'lower'", s)));
}

static Pv *str_affix(ARGS, bool start) {
    UNUSED_ARGS;
    const char *fn = start ? "startswith" : "endswith";
    no_kwargs(line, fn, nkw, kwnames);
    nargs_between(line, fn, n, 1, 1);
    Pv **items = &args[0];
    size_t k = 1;
    if (args[0]->kind == PV_TUPLE) items = args[0]->u.list->v, k = args[0]->u.list->n;
    for (size_t i = 0; i < k; i++) {
        AvpStr x = want_str(line, "the prefix", items[i]);
        if (start ? avp_str_startswith(self->u.s, x) : avp_str_endswith(self->u.s, x)) return pv_bool(true);
    }
    return pv_bool(false);
}
static Pv *str_startswith(ARGS) { return str_affix(I, line, self, args, n, kwnames, kwvals, nkw, true); }
static Pv *str_endswith(ARGS) { return str_affix(I, line, self, args, n, kwnames, kwvals, nkw, false); }

static Pv *str_replace(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "replace", nkw, kwnames);
    nargs_between(line, "replace", n, 2, 2);
    return pv_str(I->a, avp_str_replace(I->a, self->u.s, want_str(line, "replace() argument 1", args[0]),
                                        want_str(line, "replace() argument 2", args[1]), -1));
}

static Pv *str_strip(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "strip", nkw, kwnames);
    nargs_between(line, "strip", n, 0, 0);
    return pv_str(I->a, avp_str_strip(self->u.s));
}

static Pv *str_join(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "join", nkw, kwnames);
    nargs_between(line, "join", n, 1, 1);
    Pv **items;
    size_t k;
    cfg_iterate(I, line, args[0], &items, &k);
    AvpBuf b;
    avp_buf_init(&b, I->a);
    for (size_t i = 0; i < k; i++) {
        if (i) avp_buf_put(&b, self->u.s);
        if (items[i]->kind != PV_STR)
            cfg_fail(line, "sequence item %zu: expected str instance, %s found", i, cfg_type_name(items[i]));
        avp_buf_put(&b, items[i]->u.s);
    }
    return pv_str(I->a, avp_buf_str(&b));
}

/* ---- list and dict methods ---- */

static Pv *list_append(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "append", nkw, kwnames);
    nargs_between(line, "list.append", n, 1, 1);
    pv_list_push(I->a, self, args[0]);
    return pv_none();
}

static Pv *list_extend(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "extend", nkw, kwnames);
    nargs_between(line, "list.extend", n, 1, 1);
    Pv **items;
    size_t k;
    cfg_iterate(I, line, args[0], &items, &k);
    for (size_t i = 0; i < k; i++) pv_list_push(I->a, self, items[i]);
    return pv_none();
}

static Pv *list_insert(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "insert", nkw, kwnames);
    nargs_between(line, "insert", n, 2, 2);
    if (args[0]->kind != PV_INT) cfg_fail(line, "list.insert() index must be an int");
    PvList *l = self->u.list;
    int64_t len = (int64_t)l->n, k = args[0]->u.i;
    if (k < 0) k = k + len < 0 ? 0 : k + len;
    if (k > len) k = len;
    pv_list_push(I->a, self, pv_none());
    memmove(&l->v[k + 1], &l->v[k], (size_t)(len - k) * sizeof(Pv *));
    l->v[k] = args[1];
    return pv_none();
}

static Pv *list_index(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "index", nkw, kwnames);
    nargs_between(line, "index", n, 1, 1);
    for (size_t i = 0; i < self->u.list->n; i++)
        if (pv_eq(self->u.list->v[i], args[0])) return pv_int(I->a, (int64_t)i);
    cfg_fail(line, "ValueError: %.*s is not in list", AVP_SF(pv_repr(I->a, args[0])));
}

static Pv *list_copy(ARGS) {
    UNUSED_ARGS;
    nargs_between(line, "copy", n, 0, 0);
    return pv_list_copy(I->a, self);
}

static Pv *dict_items(ARGS) {
    UNUSED_ARGS;
    nargs_between(line, "items", n, 0, 0);
    Pv *l = pv_list(I->a);
    for (size_t i = 0; i < self->u.dict->n; i++) {
        Pv *t = pv_tuple(I->a);
        pv_list_push(I->a, t, pv_str(I->a, self->u.dict->keys[i]));
        pv_list_push(I->a, t, self->u.dict->vals[i]);
        pv_list_push(I->a, l, t);
    }
    return l;
}

static Pv *dict_keys(ARGS) {
    UNUSED_ARGS;
    nargs_between(line, "keys", n, 0, 0);
    Pv *l = pv_list(I->a);
    for (size_t i = 0; i < self->u.dict->n; i++) pv_list_push(I->a, l, pv_str(I->a, self->u.dict->keys[i]));
    return l;
}

static Pv *dict_values(ARGS) {
    UNUSED_ARGS;
    nargs_between(line, "values", n, 0, 0);
    Pv *l = pv_list(I->a);
    for (size_t i = 0; i < self->u.dict->n; i++) pv_list_push(I->a, l, self->u.dict->vals[i]);
    return l;
}

static Pv *dict_get(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "get", nkw, kwnames);
    nargs_between(line, "get", n, 1, 2);
    Pv *v = args[0]->kind == PV_STR ? pv_dict_get(self, args[0]->u.s) : NULL;
    return v ? v : n > 1 ? args[1] : pv_none();
}

static Pv *dict_copy(ARGS) {
    UNUSED_ARGS;
    nargs_between(line, "copy", n, 0, 0);
    return pv_dict_copy(I->a, self);
}

static Pv *dict_update(ARGS) {
    UNUSED_ARGS;
    nargs_between(line, "update", n, 0, 1);
    if (n) {
        if (args[0]->kind != PV_DICT) cfg_fail(line, "dict.update() takes a dict in the subset");
        for (size_t i = 0; i < args[0]->u.dict->n; i++)
            pv_dict_set(I->a, self, args[0]->u.dict->keys[i], args[0]->u.dict->vals[i]);
    }
    for (size_t i = 0; i < nkw; i++) pv_dict_set(I->a, self, kwnames[i], kwvals[i]);
    return pv_none();
}

/* ---- pathlib ---- */

static Pv *path_new(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "Path", nkw, kwnames);
    AvpStr form = AVP_S(".");
    for (size_t i = 0; i < n; i++) {
        if (args[i]->kind != PV_STR && args[i]->kind != PV_PATH)
            cfg_fail(line, "argument should be a str or an os.PathLike object where __fspath__ returns a str, not '%s'",
                     cfg_type_name(args[i]));
        form = i == 0 ? pv_path_norm(I->a, args[i]->u.s) : pv_path_join(I->a, form, args[i]->u.s);
    }
    return new_path(I->a, form);
}

static Pv *path_exists(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "exists", nkw, kwnames);
    nargs_between(line, "exists", n, 0, 0);
    return pv_bool(avp_exists(avp_cz(I->a, self->u.s)));
}

static Pv *path_is_file(ARGS) {
    UNUSED_ARGS;
    nargs_between(line, "is_file", n, 0, 0);
    return pv_bool(avp_isfile(avp_cz(I->a, self->u.s)));
}

static Pv *path_is_dir(ARGS) {
    UNUSED_ARGS;
    nargs_between(line, "is_dir", n, 0, 0);
    return pv_bool(avp_isdir(avp_cz(I->a, self->u.s)));
}

static Pv *path_with_suffix(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "with_suffix", nkw, kwnames);
    nargs_between(line, "with_suffix", n, 1, 1);
    AvpStr r = pv_path_with_suffix(I->a, self->u.s, want_str(line, "suffix", args[0]));
    if (!r.p) cfg_fail(line, "ValueError: invalid suffix %.*s for %.*s", AVP_SF(pv_repr(I->a, args[0])), AVP_SF(pv_repr(I->a, self)));
    return new_path(I->a, r);
}

static Pv *path_as_posix(ARGS) {
    UNUSED_ARGS;
    nargs_between(line, "as_posix", n, 0, 0);
    return pv_str(I->a, pv_path_as_posix(I->a, self->u.s));
}

static Pv *path_is_absolute(ARGS) {
    UNUSED_ARGS;
    nargs_between(line, "is_absolute", n, 0, 0);
    return pv_bool(pv_path_is_absolute(self->u.s));
}

/* ---- argparse ---- */

typedef enum { TY_STR, TY_UPPER, TY_INT, TY_PATH } ArgType;

typedef struct {
    const char *dest;
    ArgType type;
    Pv *dflt; /* NULL: not given */
    AvpArgActionKind action;
} ArgAct;

typedef struct {
    AvpArgParser *p;
    AVP_VEC(ArgAct) acts;
} ParserState;

/* type=str.upper. With choices, the converter checks them (avp's argparse takes choices on string types only):
 * argparse's own message, after the conversion as in argparse. */
static bool conv_upper(void *ctx, AvpArena *a, AvpStr in, AvpArgValue *out, AvpStr *msg) {
    out->kind = AVP_V_STR;
    out->s = avp_str_upper(a, in);
    const char *const *choices = ctx;
    if (!choices) return true;
    for (const char *const *c = choices; *c; c++)
        if (avp_str_eqz(out->s, *c)) return true;
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, "invalid choice: ");
    avp_buf_put(&b, avp_repr_str(a, out->s));
    avp_buf_putz(&b, " (choose from ");
    for (const char *const *c = choices; *c; c++) {
        if (c != choices) avp_buf_putz(&b, ", ");
        avp_buf_putz(&b, *c);
    }
    avp_buf_putc(&b, ')');
    *msg = avp_buf_str(&b);
    return false;
}

static Pv *argparse_new(ARGS) {
    UNUSED_ARGS;
    if (n) cfg_fail(line, "ArgumentParser() takes keyword arguments only in the subset");
    AvpArgParserDesc d = {0};
    d.prog = I->opts->prog;
    for (size_t i = 0; i < nkw; i++) {
        const char *v = avp_cz(I->a, want_str(line, "ArgumentParser argument", kwvals[i]));
        if (avp_str_eqz(kwnames[i], "prog")) d.prog = v;
        else if (avp_str_eqz(kwnames[i], "description")) d.description = v;
        else if (avp_str_eqz(kwnames[i], "epilog")) d.epilog = v;
        else if (avp_str_eqz(kwnames[i], "usage")) d.usage = v;
        else cfg_fail(line, "ArgumentParser(%.*s=...) is not supported", AVP_SF(kwnames[i]));
    }
    ParserState *ps = AVP_NEW(I->a, ParserState);
    ps->p = avp_args_parser(NULL, &d);
    return pv_opaque(I->a, "ArgumentParser", ps);
}

static const char *const *str_array(CfgInterp *I, int line, Pv *v, const char *what) {
    if (v->kind != PV_LIST && v->kind != PV_TUPLE) cfg_fail(line, "%s must be a list or tuple of str", what);
    const char **arr = AVP_NEWN(I->a, const char *, v->u.list->n + 1);
    for (size_t i = 0; i < v->u.list->n; i++) arr[i] = avp_cz(I->a, want_str(line, what, v->u.list->v[i]));
    return arr;
}

static Pv *argparse_add_argument(ARGS) {
    UNUSED_ARGS;
    ParserState *ps = self->u.opaque.p;
    if (!n) cfg_fail(line, "add_argument() needs a name");
    AvpBuf names;
    avp_buf_init(&names, I->a);
    for (size_t i = 0; i < n; i++) {
        if (i) avp_buf_putc(&names, ' ');
        avp_buf_put(&names, want_str(line, "an argument name", args[i]));
    }
    AvpArgDesc d = {0};
    d.names = avp_buf_cz(&names);
    ArgAct act = {0};
    act.type = TY_STR;
    for (size_t k = 0; k < nkw; k++) {
        AvpStr key = kwnames[k];
        Pv *v = kwvals[k];
        if (avp_str_eqz(key, "action")) {
            if (is_tag(v, "BooleanOptionalAction")) d.action = AVP_ARG_BOOLEAN_OPTIONAL;
            else {
                static const struct { const char *s; AvpArgActionKind k; } kinds[] = {{"store", AVP_ARG_STORE},
                    {"store_true", AVP_ARG_STORE_TRUE}, {"store_false", AVP_ARG_STORE_FALSE}, {"count", AVP_ARG_COUNT},
                    {"append", AVP_ARG_APPEND}, {"extend", AVP_ARG_EXTEND}};
                AvpStr s = want_str(line, "action", v);
                size_t i = 0;
                while (i < sizeof kinds / sizeof kinds[0] && !avp_str_eqz(s, kinds[i].s)) i++;
                if (i == sizeof kinds / sizeof kinds[0]) cfg_fail(line, "action '%.*s' is not supported", AVP_SF(s));
                d.action = kinds[i].k;
            }
        } else if (avp_str_eqz(key, "choices")) {
            d.choices = str_array(I, line, v, "choices");
        } else if (avp_str_eqz(key, "type")) {
            if (is_builtin(v, "str")) act.type = TY_STR, d.type = AVP_T_STR;
            else if (is_builtin(v, "upper") && !((CfgBuiltin *)v->u.opaque.p)->self) {
                act.type = TY_UPPER, d.type = AVP_T_CUSTOM, d.conv = conv_upper, d.type_name = "upper";
            } else if (is_builtin(v, "int")) act.type = TY_INT, d.type = AVP_T_INT;
            else if (is_builtin(v, "Path")) act.type = TY_PATH, d.type = AVP_T_IDENT, d.type_name = "Path";
            else cfg_fail(line, "type=%s is not supported", cfg_type_name(v));
        } else if (avp_str_eqz(key, "default")) {
            act.dflt = v;
        } else if (avp_str_eqz(key, "help")) {
            d.help = avp_cz(I->a, want_str(line, "help", v));
        } else if (avp_str_eqz(key, "metavar")) {
            d.metavar = avp_cz(I->a, want_str(line, "metavar", v));
        } else if (avp_str_eqz(key, "dest")) {
            d.dest = avp_cz(I->a, want_str(line, "dest", v));
        } else if (avp_str_eqz(key, "required")) {
            if (v->kind != PV_BOOL) cfg_fail(line, "required must be a bool");
            d.required = v->u.b;
        } else if (avp_str_eqz(key, "nargs")) {
            if (v->kind == PV_INT) d.nargs = (int)v->u.i;
            else {
                AvpStr s = want_str(line, "nargs", v);
                if (avp_str_eqz(s, "?")) d.nargs = AVP_NARGS_OPT;
                else if (avp_str_eqz(s, "*")) d.nargs = AVP_NARGS_STAR;
                else if (avp_str_eqz(s, "+")) d.nargs = AVP_NARGS_PLUS;
                else cfg_fail(line, "nargs=%.*s is not supported", AVP_SF(s));
            }
        } else {
            cfg_fail(line, "add_argument(%.*s=...) is not supported", AVP_SF(key));
        }
    }
    if (act.dflt) {
        Pv *v = act.dflt;
        switch (v->kind) {
        case PV_NONE: d.dflt = AVP_DEF_NONE; break;
        case PV_STR: d.dflt = AVP_DEF_STR(avp_cz(I->a, v->u.s)); break;
        case PV_BOOL: d.dflt = AVP_DEF_BOOL(v->u.b); break;
        case PV_INT: d.dflt = AVP_DEF_INT(v->u.i); break;
        case PV_PATH: d.dflt = AVP_DEF_STR(avp_cz(I->a, v->u.s)); break; /* for %(default)s; the value is act.dflt */
        default: cfg_fail(line, "a default of type %s is not supported", cfg_type_name(v));
        }
    }
    if (d.type == AVP_T_CUSTOM && d.choices) {
        d.conv_ctx = (void *)d.choices;
        if (!d.metavar) { /* the metavar argparse formats from choices */
            AvpBuf mv;
            avp_buf_init(&mv, I->a);
            avp_buf_putc(&mv, '{');
            for (const char *const *c = d.choices; *c; c++) {
                if (c != d.choices) avp_buf_putc(&mv, ',');
                avp_buf_putz(&mv, *c);
            }
            avp_buf_putc(&mv, '}');
            d.metavar = avp_buf_cz(&mv);
        }
        d.choices = NULL;
    }
    act.action = d.action;
    (void)avp_arg(ps->p, &d);
    /* argparse's dest: given, the first long option, the first short one, or the positional's name */
    if (d.dest) {
        act.dest = d.dest;
    } else if (args[0]->u.s.n && args[0]->u.s.p[0] == '-') {
        AvpStr opt = args[0]->u.s;
        for (size_t i = 0; i < n; i++)
            if (avp_str_startswith(args[i]->u.s, AVP_S("--"))) {
                opt = args[i]->u.s;
                break;
            }
        size_t k = 0;
        while (k < opt.n && opt.p[k] == '-') k++;
        act.dest = avp_cz(I->a, avp_str_replace(I->a, avp_strn(opt.p + k, opt.n - k), AVP_S("-"), AVP_S("_"), -1));
    } else {
        act.dest = avp_cz(I->a, args[0]->u.s);
    }
    avp_vec_push(&ps->acts, act);
    return pv_none();
}

static Pv *arg_value(CfgInterp *I, const ArgAct *act, const AvpArgValue *v) {
    switch (v->kind) {
    case AVP_V_NONE: return pv_none();
    case AVP_V_STR: return act->type == TY_PATH ? pv_path(I->a, v->s) : pv_str(I->a, v->s);
    case AVP_V_INT: return pv_int(I->a, v->i);
    case AVP_V_BOOL: return pv_bool(v->i != 0);
    case AVP_V_FLOAT: cfg_fail(1, "float arguments are not supported");
    case AVP_V_LIST: {
        Pv *l = pv_list(I->a);
        for (size_t i = 0; i < v->n; i++) pv_list_push(I->a, l, arg_value(I, act, &v->items[i]));
        return l;
    }
    }
    return pv_none();
}

static Pv *argparse_parse_args(ARGS) {
    UNUSED_ARGS;
    ParserState *ps = self->u.opaque.p;
    if (n || nkw) cfg_fail(line, "parse_args() takes no arguments in the subset (it parses sys.argv)");
    const ProjectEnv *env = &I->opts->env;
    char **argv = AVP_NEWN(I->a, char *, env->argc + 2);
    argv[0] = (char *)I->opts->prog;
    for (size_t i = 0; i < env->argc; i++) argv[i + 1] = (char *)env->argv[i];
    AvpArgs *r = avp_args_parse(ps->p, (int)env->argc + 1, argv);
    Pv *ns = pv_instance(I->a, "Namespace");
    for (size_t i = 0; i < ps->acts.n; i++) {
        const ArgAct *act = &ps->acts.v[i];
        if (pv_getattr(ns, act->dest)) continue; /* a repeated dest keeps its first place */
        const AvpArgValue *v = avp_args_value(r, act->dest);
        Pv *val;
        if (avp_args_given(r, act->dest) || !act->dflt || act->dflt->kind == PV_STR) val = v ? arg_value(I, act, v) : pv_none();
        else val = act->dflt; /* a non-string default is not converted (argparse) */
        pv_setattr(I->a, ns, act->dest, val);
    }
    I->last_args = ns;
    return ns;
}

/* ---- builtins ---- */

static Pv *b_str(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "str", nkw, kwnames);
    nargs_between(line, "str", n, 0, 1);
    if (!n) return pv_str(I->a, AVP_S(""));
    if (args[0]->kind == PV_INSTANCE || args[0]->kind == PV_OPAQUE)
        cfg_fail(line, "str() of a %s is not supported", cfg_type_name(args[0]));
    return pv_str(I->a, pv_to_str(I->a, args[0]));
}

static Pv *b_int(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "int", nkw, kwnames);
    nargs_between(line, "int", n, 0, 1);
    if (!n) return pv_int(I->a, 0);
    if (args[0]->kind == PV_INT) return args[0];
    if (args[0]->kind == PV_BOOL) return pv_int(I->a, args[0]->u.b);
    if (args[0]->kind == PV_STR) {
        i64 v;
        bool overflow = false;
        if (!avp_parse_int(args[0]->u.s, 10, &v, &overflow) || overflow)
            cfg_fail(line, "ValueError: invalid literal for int() with base 10: %.*s", AVP_SF(pv_repr(I->a, args[0])));
        return pv_int(I->a, v);
    }
    cfg_fail(line, "int() argument must be a string or a number, not '%s'", cfg_type_name(args[0]));
}

static Pv *b_len(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "len", nkw, kwnames);
    nargs_between(line, "len", n, 1, 1);
    PvKind k = args[0]->kind;
    if (k != PV_STR && k != PV_LIST && k != PV_TUPLE && k != PV_DICT)
        cfg_fail(line, "object of type '%s' has no len()", cfg_type_name(args[0]));
    return pv_int(I->a, (int64_t)pv_len(args[0]));
}

static Pv *b_list(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "list", nkw, kwnames);
    nargs_between(line, "list", n, 0, 1);
    Pv *l = pv_list(I->a);
    if (n) {
        Pv **items;
        size_t k;
        cfg_iterate(I, line, args[0], &items, &k);
        for (size_t i = 0; i < k; i++) pv_list_push(I->a, l, items[i]);
    }
    return l;
}

static Pv *b_getattr(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "getattr", nkw, kwnames);
    nargs_between(line, "getattr", n, 2, 3);
    AvpStr name = want_str(line, "attribute name", args[1]);
    if (n == 3 && args[0]->kind == PV_INSTANCE) {
        Pv *v = pv_getattr(args[0], avp_cz(I->a, name));
        return v ? v : args[2];
    }
    return cfg_getattr(I, line, args[0], name);
}

static Pv *b_hasattr(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "hasattr", nkw, kwnames);
    nargs_between(line, "hasattr", n, 2, 2);
    AvpStr name = want_str(line, "attribute name", args[1]);
    if (args[0]->kind != PV_INSTANCE) cfg_fail(line, "hasattr() on a %s is not supported", cfg_type_name(args[0]));
    return pv_bool(pv_getattr(args[0], avp_cz(I->a, name)) != NULL);
}

static Pv *b_setattr(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "setattr", nkw, kwnames);
    nargs_between(line, "setattr", n, 3, 3);
    AvpStr name = want_str(line, "attribute name", args[1]);
    if (args[0]->kind != PV_INSTANCE || strcmp(args[0]->u.inst->cls, "module") == 0)
        cfg_fail(line, "setattr() on a %s is not supported", cfg_type_name(args[0]));
    pv_setattr(I->a, args[0], avp_cz(I->a, name), args[2]);
    return pv_none();
}

static Pv *b_sys_exit(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "exit", nkw, kwnames);
    nargs_between(line, "exit", n, 0, 1);
    Pv *v = n ? args[0] : pv_none();
    fflush(stdout);
    if (v->kind == PV_NONE) avp_exit(0);
    if (v->kind == PV_INT) avp_exit((int)v->u.i);
    if (v->kind == PV_BOOL) avp_exit(v->u.b ? 1 : 0);
    AvpStr msg = pv_to_str(I->a, v);
    fprintf(stderr, "%.*s\n", AVP_SF(msg));
    avp_exit(1);
}

/* ---- tools.project ---- */

static Pv *b_object(ARGS) {
    UNUSED_ARGS;
    nargs_between(line, "Object", n, 2, 2);
    if (args[0]->kind != PV_BOOL) cfg_fail(line, "Object(completed, ...) takes a bool in the subset, not %s", cfg_type_name(args[0]));
    AvpStr name = want_str(line, "Object name", args[1]);
    Pv *opts = NULL;
    if (nkw) {
        opts = pv_dict(I->a);
        for (size_t i = 0; i < nkw; i++) pv_dict_set(I->a, opts, kwnames[i], kwvals[i]);
    }
    return project_new_object(I->a, args[0]->u.b, name, opts);
}

static Pv *b_progress_category(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "ProgressCategory", nkw, kwnames);
    nargs_between(line, "ProgressCategory", n, 2, 2);
    return project_new_progress_category(I->a, want_str(line, "id", args[0]), want_str(line, "name", args[1]));
}

static Pv *b_project_config(ARGS) {
    UNUSED_ARGS;
    no_kwargs(line, "ProjectConfig", nkw, kwnames);
    nargs_between(line, "ProjectConfig", n, 0, 0);
    return project_new_config(I->a);
}

static Pv *b_is_windows(ARGS) {
    UNUSED_ARGS;
    nargs_between(line, "is_windows", n, 0, 0);
    return pv_bool(project_is_windows());
}

static Pv *entry_point(ARGS, bool build) {
    UNUSED_ARGS;
    const char *fn = build ? "generate_build" : "calculate_progress";
    no_kwargs(line, fn, nkw, kwnames);
    nargs_between(line, fn, n, 1, 1);
    if (!pv_isinstance(args[0], "ProjectConfig")) cfg_fail(line, "%s() needs a ProjectConfig", fn);
    if (I->opts->dump) {
        AvpStr doc = cfg_dump(I->a, I->last_args ? I->last_args : pv_none(), args[0]);
        fwrite(doc.p, 1, doc.n, stdout);
        return pv_none();
    }
    int rc = build ? project_generate_build(I->a, args[0], &I->opts->env)
                   : project_calculate_progress(I->a, args[0], &I->opts->env);
    if (rc) avp_exit(rc);
    return pv_none();
}
static Pv *b_generate_build(ARGS) { return entry_point(I, line, self, args, n, kwnames, kwvals, nkw, true); }
static Pv *b_calculate_progress(ARGS) { return entry_point(I, line, self, args, n, kwnames, kwvals, nkw, false); }

/* ---- modules ---- */

static Pv *module(CfgInterp *I, const char *name) {
    Pv *m = pv_instance(I->a, "module");
    pv_setattr(I->a, m, "__name__", pv_strz(I->a, name));
    return m;
}

static Pv *g_modules; /* dict of the importable modules */

Pv *cfg_import(CfgInterp *I, int line, AvpStr name) {
    (void)I;
    Pv *m = pv_dict_get(g_modules, name);
    if (!m) cfg_fail(line, "module '%.*s' is not available in the configure.py subset", AVP_SF(name));
    return m;
}

void cfg_install_builtins(CfgInterp *I) {
    AvpArena *a = I->a;
    Pv *b = I->builtins;
    Pv *str = cfg_builtin(a, "str", b_str, NULL, NULL);
    pv_dict_set(a, b, AVP_S("str"), str);
    pv_dict_set(a, b, AVP_S("int"), cfg_builtin(a, "int", b_int, NULL, NULL));
    pv_dict_set(a, b, AVP_S("len"), cfg_builtin(a, "len", b_len, NULL, NULL));
    pv_dict_set(a, b, AVP_S("list"), cfg_builtin(a, "list", b_list, NULL, NULL));
    pv_dict_set(a, b, AVP_S("getattr"), cfg_builtin(a, "getattr", b_getattr, NULL, NULL));
    pv_dict_set(a, b, AVP_S("hasattr"), cfg_builtin(a, "hasattr", b_hasattr, NULL, NULL));
    pv_dict_set(a, b, AVP_S("setattr"), cfg_builtin(a, "setattr", b_setattr, NULL, NULL));

    g_modules = pv_dict(a);
    Pv *argparse = module(I, "argparse");
    pv_setattr(a, argparse, "ArgumentParser", cfg_builtin(a, "ArgumentParser", argparse_new, NULL, NULL));
    pv_setattr(a, argparse, "BooleanOptionalAction", pv_opaque(a, "BooleanOptionalAction", NULL));
    pv_dict_set(a, g_modules, AVP_S("argparse"), argparse);

    Pv *sys = module(I, "sys");
    pv_setattr(a, sys, "exit", cfg_builtin(a, "exit", b_sys_exit, NULL, NULL));
    pv_setattr(a, sys, "platform", pv_strz(a, "win32"));
    Pv *argv = pv_list(a);
    pv_list_push(a, argv, pv_strz(a, I->opts->env.script));
    for (size_t i = 0; i < I->opts->env.argc; i++) pv_list_push(a, argv, pv_strz(a, I->opts->env.argv[i]));
    pv_setattr(a, sys, "argv", argv);
    pv_dict_set(a, g_modules, AVP_S("sys"), sys);

    Pv *pathlib = module(I, "pathlib");
    Pv *path = cfg_builtin(a, "Path", path_new, NULL, NULL);
    pv_setattr(a, pathlib, "Path", path);
    pv_setattr(a, pathlib, "PurePath", path);
    pv_setattr(a, pathlib, "PureWindowsPath", path);
    pv_setattr(a, pathlib, "WindowsPath", path);
    pv_dict_set(a, g_modules, AVP_S("pathlib"), pathlib);

    Pv *typing = module(I, "typing");
    static const char *const types[] = {"Any", "Dict", "List", "Optional", "Tuple", "Union", "Callable", "Set",
                                        "Iterable", "Sequence", NULL};
    for (const char *const *t = types; *t; t++) pv_setattr(a, typing, *t, pv_opaque(a, "typing", (void *)*t));
    pv_dict_set(a, g_modules, AVP_S("typing"), typing);

    Pv *project = module(I, "tools.project");
    pv_setattr(a, project, "Object", cfg_builtin(a, "Object", b_object, NULL, NULL));
    pv_setattr(a, project, "ProgressCategory", cfg_builtin(a, "ProgressCategory", b_progress_category, NULL, NULL));
    pv_setattr(a, project, "ProjectConfig", cfg_builtin(a, "ProjectConfig", b_project_config, NULL, NULL));
    pv_setattr(a, project, "is_windows", cfg_builtin(a, "is_windows", b_is_windows, NULL, NULL));
    pv_setattr(a, project, "generate_build", cfg_builtin(a, "generate_build", b_generate_build, NULL, NULL));
    pv_setattr(a, project, "calculate_progress", cfg_builtin(a, "calculate_progress", b_calculate_progress, NULL, NULL));
    pv_dict_set(a, g_modules, AVP_S("tools.project"), project);
}

/* ---- attributes ---- */

typedef struct {
    const char *name;
    CfgBuiltinFn fn;
} Method;

static Pv *method(CfgInterp *I, int line, Pv *obj, AvpStr name, const Method *ms) {
    for (; ms->name; ms++)
        if (avp_str_eqz(name, ms->name)) return cfg_builtin(I->a, ms->name, ms->fn, obj, NULL);
    cfg_fail(line, "'%s' object has no attribute '%.*s' in the configure.py subset", cfg_type_name(obj), AVP_SF(name));
}

Pv *cfg_getattr(CfgInterp *I, int line, Pv *obj, AvpStr name) {
    AvpArena *a = I->a;
    switch (obj->kind) {
    case PV_INSTANCE: {
        Pv *v = pv_getattr(obj, avp_cz(a, name));
        if (v) return v;
        if (strcmp(obj->u.inst->cls, "module") == 0)
            cfg_fail(line, "module '%.*s' has no attribute '%.*s' in the configure.py subset",
                     AVP_SF(pv_getattr(obj, "__name__")->u.s), AVP_SF(name));
        cfg_fail(line, "'%s' object has no attribute '%.*s'", obj->u.inst->cls, AVP_SF(name));
    }
    case PV_STR: {
        static const Method ms[] = {{"upper", str_upper}, {"lower", str_lower}, {"startswith", str_startswith},
                                    {"endswith", str_endswith}, {"replace", str_replace}, {"strip", str_strip},
                                    {"join", str_join}, {NULL, NULL}};
        return method(I, line, obj, name, ms);
    }
    case PV_LIST: {
        static const Method ms[] = {{"append", list_append}, {"extend", list_extend}, {"insert", list_insert},
                                    {"index", list_index}, {"copy", list_copy}, {NULL, NULL}};
        return method(I, line, obj, name, ms);
    }
    case PV_TUPLE: {
        static const Method ms[] = {{"index", list_index}, {NULL, NULL}};
        return method(I, line, obj, name, ms);
    }
    case PV_DICT: {
        static const Method ms[] = {{"items", dict_items}, {"keys", dict_keys}, {"values", dict_values},
                                    {"get", dict_get}, {"copy", dict_copy}, {"update", dict_update}, {NULL, NULL}};
        return method(I, line, obj, name, ms);
    }
    case PV_PATH: {
        if (avp_str_eqz(name, "parent")) return new_path(a, pv_path_parent(a, obj->u.s));
        if (avp_str_eqz(name, "name")) return pv_str(a, pv_path_name(obj->u.s));
        if (avp_str_eqz(name, "suffix")) return pv_str(a, pv_path_suffix(obj->u.s));
        if (avp_str_eqz(name, "stem")) return pv_str(a, pv_path_stem(obj->u.s));
        static const Method ms[] = {{"exists", path_exists}, {"is_file", path_is_file}, {"is_dir", path_is_dir},
                                    {"with_suffix", path_with_suffix}, {"as_posix", path_as_posix},
                                    {"is_absolute", path_is_absolute}, {NULL, NULL}};
        return method(I, line, obj, name, ms);
    }
    case PV_OPAQUE:
        if (is_tag(obj, "ArgumentParser")) {
            static const Method ms[] = {{"add_argument", argparse_add_argument}, {"parse_args", argparse_parse_args},
                                        {NULL, NULL}};
            return method(I, line, obj, name, ms);
        }
        if (is_builtin(obj, "str")) { /* unbound str methods: str.upper */
            if (avp_str_eqz(name, "upper")) return cfg_builtin(a, "upper", str_upper, NULL, NULL);
            if (avp_str_eqz(name, "lower")) return cfg_builtin(a, "lower", str_lower, NULL, NULL);
        }
        break;
    default: break;
    }
    cfg_fail(line, "'%s' object has no attribute '%.*s' in the configure.py subset", cfg_type_name(obj), AVP_SF(name));
}
