/* names.py cmd_demangle, cmd_mangle, cmd_find, cmd_rename and show_plan. */
#include "names.h"
#include "names_priv.h"

#include "avp/fmt.h"
#include "shsm_mangle.h"

#include <string.h>

static AvpJson *str_or_null(AvpArena *a, const char *s) { return s ? avp_json_strz(a, s) : avp_json_null(); }

int names_cmd_demangle(const NamesArgs *na, ShsmOutput *out) {
    AvpArena *a = avp_arena();
    AvpJson *rows = avp_json_arr(a);
    size_t bad = 0;
    for (size_t i = 0; i < na->names.n; i++) {
        const char *n = avp_cz(a, na->names.v[i]);
        const char *d = shsm_demangle(a, n);
        AvpJson *r = avp_json_obj(a);
        avp_json_set(a, r, AVP_S("name"), avp_json_strz(a, n));
        avp_json_set(a, r, AVP_S("demangled"), str_or_null(a, d));
        avp_json_arr_push(a, rows, r);
        shsm_output_line(out, avp_pyfmt(a, "%s\t%s", n, d ? d : "(not a mangled name)"));
        bad += d == NULL;
    }
    shsm_output_data(out, "names", rows);
    return bad && bad == na->names.n ? SHSM_EXIT_FINDING : SHSM_EXIT_OK;
}

int names_cmd_mangle(const NamesArgs *na, ShsmOutput *out) {
    AvpArena *a = avp_arena();
    AvpStrVec params = {0};
    if (na->params && *na->params) {
        params = names_split_params(a, avp_str(na->params));
        for (size_t i = 0; i < params.n; i++) params.v[i] = avp_str_strip(params.v[i]);
    }
    if (params.n == 1 && avp_str_eqz(params.v[0], "void")) params.n = 0;
    const char **pz = AVP_NEWN(a, const char *, params.n + 1);
    for (size_t i = 0; i < params.n; i++) pz[i] = avp_cz(a, params.v[i]);
    const char **scope = NULL;
    size_t nscope = 0;
    if (na->cls && *na->cls) {
        AvpStrVec parts = avp_str_split(avp_str(na->cls), AVP_S("::"), -1);
        scope = AVP_NEWN(a, const char *, parts.n);
        for (size_t i = 0; i < parts.n; i++) scope[i] = avp_cz(a, parts.v[i]);
        nscope = parts.n;
        avp_vec_free(&parts);
    }
    const char *m = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        m = shsm_mangle_parts(a, na->name, scope, nscope, pz, params.n, na->is_const, false);
        avp_try_end(&t);
    } else {
        if (t.code == SHSM_EXC_VALUE || shsm_exc_is_key(t.code)) {
            AvpStr e = shsm_exc_str(a, t.code, t.msg ? t.msg : "");
            shsm_raise(SHSM_EXC_USAGE, "cannot mangle: %.*s", AVP_SF(e));
        }
        avp_throw(t.code, "%s", t.msg ? t.msg : "");
    }
    shsm_output_linez(out, m);
    shsm_output_data(out, "mangled", avp_json_strz(a, m));
    shsm_output_data(out, "demangled", str_or_null(a, shsm_demangle(a, m)));
    return SHSM_EXIT_OK;
}

int names_cmd_find(const NamesArgs *na, ShsmOutput *out, const ShsmRepo *repo) {
    AvpArena *a = avp_arena();
    const ShsmSymbolTable *st = shsm_load_symbols(repo, true);
    AvpStr needle = avp_str_lower(a, avp_str(na->text));
    AvpJson *hits = avp_json_arr(a);
    for (size_t i = 0; i < st->n; i++) {
        const ShsmSymbol *s = &st->symbols[i];
        const char *d = shsm_demangle(a, s->name);
        if (!(avp_str_contains(avp_str_lower(a, avp_str(s->name)), needle) ||
              (d && avp_str_contains(avp_str_lower(a, avp_str(d)), needle))))
            continue;
        const char *u = shsm_sym_unit_of(st, s->address);
        const char *unit = u ? avp_sprintf(a, "main/%s", u) : NULL;
        AvpStr size = s->has_size ? avp_pyfmt(a, "%X", s->size) : AVP_S("-");
        AvpStr extra = d ? avp_pyfmt(a, "  {%s}", d) : AVP_S("");
        shsm_output_line(out, avp_pyfmt(a, "%08X %6S %-8s %s%S  %s", s->address, &size, s->section, s->name, &extra,
                                        unit ? unit : "(asm-only)"));
        AvpJson *h = avp_json_obj(a);
        avp_json_set(a, h, AVP_S("name"), avp_json_strz(a, s->name));
        avp_json_set(a, h, AVP_S("demangled"), str_or_null(a, d));
        avp_json_set(a, h, AVP_S("address"), avp_json_uint(a, s->address));
        avp_json_set(a, h, AVP_S("size"), s->has_size ? avp_json_uint(a, s->size) : avp_json_null());
        avp_json_set(a, h, AVP_S("type"), str_or_null(a, s->type));
        avp_json_set(a, h, AVP_S("section"), str_or_null(a, s->section));
        avp_json_set(a, h, AVP_S("unit"), str_or_null(a, unit));
        avp_json_arr_push(a, hits, h);
    }
    shsm_output_data(out, "symbols", hits);
    if (!avp_json_len(hits)) {
        shsm_output_line(out, avp_pyfmt(a, "no symbol matches %r", na->text));
        return SHSM_EXIT_FINDING;
    }
    return SHSM_EXIT_OK;
}

/* The lines of the report, kept apart so that the last line (what happened, what to do) survives the output cap. */
static void add_line(AvpStrVec *v, AvpStr s) { avp_vec_push(v, s); }

static AvpStr show_plan(const NamesPlan *p, ShsmOutput *out, bool apply_requested, bool applied, const char *head) {
    AvpArena *a = avp_arena();
    AvpStrVec lines = {0};
    add_line(&lines, avp_str(head));
    size_t total = 0;
    for (size_t i = 0; i < p->files.n; i++) total += p->files.v[i]->edits.n;
    for (size_t i = 0; i < p->symbols.n; i++) {
        const NamesSymRename *r = &p->symbols.v[i];
        const char *d = shsm_demangle(a, r->new_name);
        AvpStr extra = d ? avp_pyfmt(a, "  {%s}", d) : AVP_S("");
        add_line(&lines, avp_pyfmt(a, "  %s -> %s  0x%08X%S", r->sym->name, r->new_name, r->sym->address, &extra));
    }
    for (size_t i = 0; i < p->conflicts.n; i++) add_line(&lines, avp_pyfmt(a, "CONFLICT: %S", &p->conflicts.v[i]));
    for (size_t i = 0; i < p->notes.n; i++) add_line(&lines, avp_pyfmt(a, "note: %S", &p->notes.v[i]));
    if (p->signature && p->sig_changes.n) {
        add_line(&lines, AVP_S("signature (old  ->  new):"));
        for (size_t i = 0; i < p->sig_changes.n; i++) add_line(&lines, avp_pyfmt(a, "  %S", &p->sig_changes.v[i]));
        add_line(&lines, avp_pyfmt(a, "to edit by hand: %d source site(s) that declare or define the function under the "
                                      "old signature (the tool rewrites symbols.txt, and source only where the plain "
                                      "name changed)",
                                   p->hand_sites.n));
        for (size_t i = 0; i < p->hand_sites.n; i++) add_line(&lines, avp_pyfmt(a, "  %S", &p->hand_sites.v[i]));
    }
    add_line(&lines, avp_pyfmt(a, "%s %d edit(s) in %d file(s)", applied ? "applied" : "planned", total, p->files.n));
    for (size_t i = 0; i < p->files.n; i++) {
        const NamesFile *f = p->files.v[i];
        add_line(&lines, avp_pyfmt(a, "%s (%d)", f->rel, f->edits.n));
        for (size_t k = 0; k < f->edits.n; k++) {
            AvpStr o = names_clip(a, f->edits.v[k].old, 150), n = names_clip(a, f->edits.v[k].new_, 150);
            add_line(&lines, avp_pyfmt(a, "  %5d - %S", f->edits.v[k].line, &o));
            add_line(&lines, avp_pyfmt(a, "        + %S", &n));
        }
    }
    AvpStr status;
    if (p->conflicts.n)
        status = avp_pyfmt(a, "REFUSED: %d conflict(s); NOTHING WAS WRITTEN%s.", p->conflicts.n,
                           apply_requested ? " (--apply did not apply)" : "");
    else if (applied)
        status = avp_pyfmt(a, "APPLIED: wrote %d edit(s) in %d file(s).", total, p->files.n);
    else
        status = avp_pyfmt(a, "DRY RUN: NOTHING WAS WRITTEN. Re-run with --apply to write these %d edit(s) in %d file(s).",
                           total, p->files.n);
    /* the status line is the last line even when the cap cuts the listing */
    bool capped = !out->show_all && out->limit > 0;
    size_t room = lines.n;
    if (capped && lines.n + 1 > (size_t)out->limit) room = out->limit > 2 ? (size_t)out->limit - 2 : 0;
    for (size_t i = 0; i < room && i < lines.n; i++) shsm_output_line(out, lines.v[i]);
    if (room < lines.n)
        shsm_output_line(out, avp_pyfmt(a, "... %d more lines, use --all", lines.n - room));
    shsm_output_line(out, status);
    avp_vec_free(&lines);
    return status;
}

int names_cmd_rename(const NamesArgs *na, ShsmOutput *out, const ShsmRepo *repo) {
    AvpArena *a = avp_arena();
    NamesCtx *cx = names_ctx_new(repo, NULL);
    NamesPlan *p;
    const char *head;
    if (na->cls_rename) {
        if (na->signature || na->family)
            shsm_raise(SHSM_EXC_USAGE, "--signature and --family apply to a function rename, not to --class");
        p = names_plan_class_rename(cx, na->old, na->new_);
        head = avp_cz(a, avp_pyfmt(a, "rename class %s -> %s (%d symbols)", na->old, na->new_, p->symbols.n));
    } else {
        NamesRenameOpts o = {.local = na->local, .symbols_only = na->symbols_only, .signature = na->signature,
                             .family = na->family};
        p = names_plan_rename_opts(cx, na->old, na->new_, &o);
        head = avp_cz(a, avp_pyfmt(a, "rename %s -> %s (%d symbol%s)", na->old, na->new_, p->symbols.n,
                                   p->symbols.n == 1 ? "" : "s"));
    }
    names_compute_edits(p);
    bool applied = na->apply && !p->conflicts.n;
    if (applied) names_write_plan(p);
    AvpStr status = show_plan(p, out, na->apply, applied, head);
    AvpJson *d = names_plan_to_json(a, p);
    for (size_t i = 0; i < avp_json_len(d); i++) {
        const AvpJsonMember *m = avp_json_member(d, i);
        shsm_output_data(out, avp_cz(a, m->key), m->val);
    }
    shsm_output_data(out, "applied", avp_json_bool(applied));
    shsm_output_data(out, "status", avp_json_str(a, status));
    return p->conflicts.n ? SHSM_EXIT_FINDING : SHSM_EXIT_OK;
}

int names_cmd_add(const NamesArgs *na, ShsmOutput *out, const ShsmRepo *repo) {
    AvpArena *a = avp_arena();
    NamesCtx *cx = names_ctx_new(repo, NULL);
    NamesPlan *p = names_plan_add(cx, &na->add);
    size_t total = 0;
    for (size_t i = 0; i < p->files.n; i++) total += p->files.v[i]->edits.n;
    const char *head = avp_cz(a, avp_pyfmt(a, "add %s", na->add.name));
    bool applied = na->apply && !p->conflicts.n && total;
    if (applied) names_write_plan(p);
    AvpStr status = show_plan(p, out, na->apply, applied, head);
    AvpJson *d = names_plan_to_json(a, p);
    for (size_t i = 0; i < avp_json_len(d); i++) {
        const AvpJsonMember *m = avp_json_member(d, i);
        shsm_output_data(out, avp_cz(a, m->key), m->val);
    }
    shsm_output_data(out, "applied", avp_json_bool(applied));
    shsm_output_data(out, "status", avp_json_str(a, status));
    return p->conflicts.n ? SHSM_EXIT_FINDING : SHSM_EXIT_OK;
}

int names_cmd_attr(const NamesArgs *na, ShsmOutput *out, const ShsmRepo *repo) {
    AvpArena *a = avp_arena();
    NamesCtx *cx = names_ctx_new(repo, NULL);
    NamesPlan *p = names_plan_attr(cx, &na->attr);
    size_t total = 0;
    for (size_t i = 0; i < p->files.n; i++) total += p->files.v[i]->edits.n;
    const char *head = avp_cz(a, avp_pyfmt(a, "attr %s", na->attr.symbol));
    bool applied = na->apply && !p->conflicts.n && total;
    if (applied) names_write_plan(p);
    AvpStr status;
    if (!p->conflicts.n && !total) {
        shsm_output_line(out, avp_str(head));
        status = AVP_S("NO CHANGE: the symbol already has these attributes; NOTHING WAS WRITTEN.");
        shsm_output_line(out, status);
    } else {
        status = show_plan(p, out, na->apply, applied, head);
    }
    AvpJson *d = names_plan_to_json(a, p);
    for (size_t i = 0; i < avp_json_len(d); i++) {
        const AvpJsonMember *m = avp_json_member(d, i);
        shsm_output_data(out, avp_cz(a, m->key), m->val);
    }
    shsm_output_data(out, "applied", avp_json_bool(applied));
    shsm_output_data(out, "status", avp_json_str(a, status));
    return p->conflicts.n ? SHSM_EXIT_FINDING : SHSM_EXIT_OK;
}
