/* query.py fn, callers / callees and xref. */
#include "query.h"

/* ---- fn ---- */

static AvpStr label_at(const ShsmAsmBlock *b, i64 addr) {
    AvpStr name = {NULL, 0};
    for (size_t i = 0; i < b->labels.n; i++)
        if (b->labels.v[i].address == addr) name = b->labels.v[i].name; /* dict: the last value wins */
    return name;
}

int query_cmd_fn(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out) {
    AvpArena *a = ctx->a;
    const ShsmSymRow *s = query_resolve(ctx, avp_args_str(args, "sym"), "function");
    ShsmAsmFile *af = NULL;
    ShsmAsmBlock *b = shsm_asmindex_read(query_ix(ctx), s, &af);
    AvpStr unit = avp_str(shsm_symrow_unit_name(a, s));
    double p = 0;
    bool has_p = query_percent(ctx, s->name, unit, &p);
    AvpStr dm = (s->demangled.p && s->demangled.n) ? s->demangled : query_demangled(a, s->name);
    const char *status = query_status(ctx, unit);
    AvpJson *head = avp_json_obj(a);
    avp_json_set(a, head, AVP_S("name"), avp_json_str(a, s->name));
    avp_json_set(a, head, AVP_S("demangled"), shsm_json_opt_str(a, dm));
    avp_json_set(a, head, AVP_S("address"), shsm_json_opt_int(a, s->address));
    avp_json_set(a, head, AVP_S("size"), shsm_json_opt_int(a, s->size));
    avp_json_set(a, head, AVP_S("section"), shsm_json_opt_str(a, s->section));
    avp_json_set(a, head, AVP_S("unit"), avp_json_str(a, unit));
    avp_json_set(a, head, AVP_S("status"), avp_json_strz(a, status));
    avp_json_set(a, head, AVP_S("percent"), has_p ? avp_json_float(a, p) : avp_json_null());
    avp_json_set(a, head, AVP_S("file"), shsm_json_opt_str(a, s->file));
    avp_json_set(a, head, AVP_S("line"), shsm_json_opt_int(a, s->line_start));
    AvpStr dmtext = dm.p && dm.n ? (avp_pyfmt(a, "  {%S}", &dm)) : AVP_S("");
    shsm_output_line(out, (avp_pyfmt(a, "%S%S", &s->name, &dmtext)));
    i64 size = s->size == SHSM_NONE ? 0 : s->size;
    AvpStr section = s->section.p ? s->section : AVP_S("None");
    AvpStr uh = query_unit_header(ctx, s), ps = avp_str_strip(query_pct(a, has_p, p));
    shsm_output_line(out, (avp_pyfmt(a, "  0x%08X..0x%08X size 0x%X  %S  %S  match %S", s->address,
                                            s->address + size, size, &section, &uh, &ps)));
    const ShsmAsmRow *rows = b ? b->rows.v : NULL;
    size_t nrows = b ? b->rows.n : 0;
    QuerySummary summ = query_summarize(rows, nrows);
    avp_json_set(a, head, AVP_S("summary"), query_summary_json(a, &summ));
    if (avp_args_bool(args, "summary")) {
        AvpStr saves = summ.min_gpr >= 0 ? (avp_pyfmt(a, "r%d-r31", summ.min_gpr)) : AVP_S("no GPRs");
        AvpStr fprs = summ.min_fpr >= 0 ? (avp_pyfmt(a, ", f%d-f31", summ.min_fpr)) : AVP_S("");
        shsm_output_line(out, (avp_pyfmt(a,
                                                "  %d instructions, %d calls, %d tail calls, %d branches, frame 0x%X"
                                                ", saves %S%S%s",
                                                summ.instructions, summ.calls, summ.tail_calls, summ.branches,
                                                summ.frame, &saves, &fprs, summ.floats ? ", float code" : "")));
        shsm_output_data(out, "function", head);
        if (af) shsm_asm_free(af);
        return SHSM_EXIT_OK;
    }
    AvpStr file = s->file.p ? s->file : AVP_S("None");
    shsm_output_line(out, (avp_pyfmt(a, "  asm: build/%s/asm/%S:%d", ctx->repo->version, &file,
                                            s->line_start == SHSM_NONE ? 0 : s->line_start)));
    bool raw = avp_args_bool(args, "raw");
    AvpJson *jrows = avp_json_arr(a);
    for (size_t i = 0; i < nrows; i++) {
        const ShsmAsmRow *r = &rows[i];
        AvpStr lab = label_at(b, r->address);
        if (lab.p && r->address != s->address) shsm_output_line(out, (avp_pyfmt(a, "%S:", &lab)));
        AvpStr note = query_row_note(ctx, r);
        AvpStr target = r->sym;
        if (!target.p || !target.n) target = r->label != SHSM_NONE && r->label ? label_at(b, r->label) : (AvpStr){0};
        AvpJson *jr = avp_json_obj(a);
        avp_json_set(a, jr, AVP_S("address"), avp_json_int(a, r->address));
        avp_json_set(a, jr, AVP_S("bytes"), shsm_json_opt_str(a, r->bytes));
        avp_json_set(a, jr, AVP_S("mnemonic"), shsm_json_opt_str(a, r->mnemonic));
        avp_json_set(a, jr, AVP_S("operands"), shsm_json_opt_str(a, r->operands));
        avp_json_set(a, jr, AVP_S("target"), shsm_json_opt_str(a, target));
        avp_json_set(a, jr, AVP_S("note"), note.n ? avp_json_str(a, note) : avp_json_null());
        avp_json_arr_push(a, jrows, jr);
        AvpStr rawtext = raw ? (avp_pyfmt(a, "%S  ", &r->bytes)) : AVP_S("");
        AvpStr line = (avp_pyfmt(a, "  %08X  %S%-8S %S", r->address, &rawtext, &r->mnemonic, &r->operands));
        if (note.n) line = (avp_pyfmt(a, "%-58S ; %S", &line, &note));
        shsm_output_line(out, line);
    }
    avp_json_set(a, head, AVP_S("rows"), jrows);
    shsm_output_data(out, "function", head);
    if (af) shsm_asm_free(af);
    return SHSM_EXIT_OK;
}

/* ---- callers / callees ---- */

typedef struct {
    AvpStr other;   /* p NULL: None */
    i64 count;
    AvpStr kind;
    i64 site;       /* SHSM_NONE: None */
} CallEntry;
typedef AVP_VEC(CallEntry) CallEntries;

static int cmp_cstr(const void *x, const void *y, void *c) {
    (void)c;
    return strcmp(*(const char *const *)x, *(const char *const *)y);
}

/* _callers(ctx, target): grouped by caller (or "@site"), first-site order; kinds sorted and joined. */
static CallEntries callers_of(QueryCtx *ctx, ShsmTarget target) {
    AvpArena *a = ctx->a;
    ShsmCallers cs = shsm_asmindex_callers(a, query_ix(ctx), target, true);
    AvpMap *groups = avp_map_new();
    typedef struct {
        AvpStr caller;
        i64 first, n;
        AVP_VEC(const char *) kinds;
    } Group;
    AVP_VEC(Group) gs = {0};
    for (size_t i = 0; i < cs.n; i++) {
        AvpStr key = cs.v[i].name.p && cs.v[i].name.n ? cs.v[i].name
                                                      : (avp_pyfmt(a, "@%08X", cs.v[i].site));
        bool created = false;
        AvpVal *v = avp_map_put(groups, key, &created);
        if (created) {
            v->i = (i64)gs.n;
            avp_vec_push(&gs, ((Group){cs.v[i].name.n ? cs.v[i].name : (AvpStr){NULL, 0}, cs.v[i].site, 0, {0}}));
        }
        Group *g = &gs.v[v->i];
        g->n++;
        bool have = false;
        for (size_t k = 0; k < g->kinds.n; k++) have = have || strcmp(g->kinds.v[k], cs.v[i].kind) == 0;
        if (!have) avp_vec_push(&g->kinds, cs.v[i].kind);
    }
    CallEntries out = {0};
    for (size_t i = 0; i < gs.n; i++) {
        Group *g = &gs.v[i];
        avp_sort(g->kinds.v, g->kinds.n, sizeof *g->kinds.v, cmp_cstr, NULL, false);
        AvpBuf b = {0};
        for (size_t k = 0; k < g->kinds.n; k++) {
            if (k) avp_buf_putz(&b, "/");
            avp_buf_putz(&b, g->kinds.v[k]);
        }
        avp_vec_push(&out, ((CallEntry){g->caller, g->n, avp_str_dup(a, avp_buf_str(&b)), g->first}));
        avp_buf_free(&b);
        avp_vec_free(&g->kinds);
    }
    avp_vec_free(&gs);
    avp_map_free(groups);
    return out;
}

typedef struct {
    QueryCtx *ctx;
    ShsmOutput *out;
    bool callers;
    AvpMap *seen;
} Walk;

static void walk(Walk *w, ShsmTarget target, i64 depth, int indent, AvpJson *sink) {
    AvpArena *a = w->ctx->a;
    CallEntries es = {0};
    if (w->callers) es = callers_of(w->ctx, target);
    else {
        ShsmCallees cs = shsm_asmindex_callees(a, query_ix(w->ctx), target);
        for (size_t i = 0; i < cs.n; i++)
            avp_vec_push(&es, ((CallEntry){cs.v[i].name, cs.v[i].count, avp_str(cs.v[i].kind), SHSM_NONE}));
    }
    for (size_t i = 0; i < es.n; i++) {
        CallEntry *e = &es.v[i];
        bool named = e->other.p && e->other.n;
        QuerySymDesc d = query_describe_sym(w->ctx, named ? e->other : (AvpStr){NULL, 0}, e->site);
        AvpJson *jd = query_desc_json(a, &d);
        avp_json_set(a, jd, AVP_S("count"), avp_json_int(a, e->count));
        avp_json_set(a, jd, AVP_S("kind"), avp_json_str(a, e->kind));
        AvpJson *children = avp_json_arr(a);
        avp_json_set(a, jd, AVP_S("children"), children);
        bool again = named && avp_map_has(w->seen, e->other);
        AvpStr tag = (avp_pyfmt(a, "%3dx %-6S ", e->count, &e->kind));
        AvpBuf b = {0};
        for (int k = 0; k < indent; k++) avp_buf_putz(&b, "  ");
        avp_buf_put(&b, query_sym_line(a, &d, tag));
        if (again && depth > 1) avp_buf_putz(&b, "  (repeated)");
        shsm_output_line(w->out, avp_str_dup(a, avp_buf_str(&b)));
        avp_buf_free(&b);
        avp_json_arr_push(a, sink, jd);
        if (named && !again && depth > 1) {
            avp_map_put(w->seen, e->other, NULL);
            walk(w, shsm_target_name(e->other), depth - 1, indent + 1, children);
        } else if (named) {
            avp_map_put(w->seen, e->other, NULL);
        }
    }
    avp_vec_free(&es);
}

static int cmd_calls(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out, bool callers) {
    AvpArena *a = ctx->a;
    const ShsmSymRow *s = query_resolve(ctx, avp_args_str(args, "sym"), NULL);
    QuerySymDesc root = query_describe_sym(ctx, s->name, SHSM_NONE);
    shsm_output_line(out, query_sym_line(a, &root, AVP_S("")));
    AvpJson *items = avp_json_arr(a);
    Walk w = {ctx, out, callers, avp_map_new()};
    avp_map_put(w.seen, s->name, NULL);
    i64 depth = avp_args_int(args, "depth");
    walk(&w, shsm_target_sym(s), depth > 1 ? depth : 1, 1, items);
    const char *dir = callers ? "callers" : "callees";
    if (!avp_json_len(items)) shsm_output_line(out, (avp_pyfmt(a, "  (no %s)", dir)));
    shsm_output_data(out, "symbol", query_desc_json(a, &root));
    shsm_output_data(out, dir, items);
    avp_map_free(w.seen);
    return SHSM_EXIT_OK;
}

int query_cmd_callers(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out) { return cmd_calls(ctx, args, out, true); }
int query_cmd_callees(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out) { return cmd_calls(ctx, args, out, false); }

/* ---- xref ---- */

typedef struct {
    AvpStr from; /* p NULL: None */
    AvpI64Vec sites, offsets;
    AVP_VEC(const char *) kinds;
} XrefEntry;

static int kind_group(const char *kind) {
    if (!strcmp(kind, "call") || !strcmp(kind, "branch")) return 0;
    if (!strcmp(kind, "ptr") || !strcmp(kind, "jumptable")) return 2;
    return 1;
}

int query_cmd_xref(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out) {
    AvpArena *a = ctx->a;
    const ShsmSymRow *s = query_resolve(ctx, avp_args_str(args, "sym"), NULL);
    QuerySymDesc root = query_describe_sym(ctx, s->name, SHSM_NONE);
    shsm_output_line(out, query_sym_line(a, &root, AVP_S("")));
    AvpMap *keys[3] = {avp_map_new(), avp_map_new(), avp_map_new()};
    AVP_VEC(XrefEntry) groups[3] = {{0}, {0}, {0}};
    ShsmRefs refs = shsm_asmindex_refs_to(a, query_ix(ctx), shsm_target_sym(s), NULL, 0);
    for (size_t i = 0; i < refs.n; i++) {
        const ShsmAsmRef *r = &refs.v[i];
        int g = kind_group(r->kind);
        bool named = r->from_sym.p && r->from_sym.n;
        AvpStr key = named ? r->from_sym : (avp_pyfmt(a, "@%08X", r->from_addr));
        bool created = false;
        AvpVal *v = avp_map_put(keys[g], key, &created);
        if (created) {
            v->i = (i64)groups[g].n;
            avp_vec_push(&groups[g], ((XrefEntry){named ? r->from_sym : (AvpStr){NULL, 0}, {0}, {0}, {0}}));
        }
        XrefEntry *e = &groups[g].v[v->i];
        avp_vec_push(&e->sites, r->from_addr);
        bool have = false;
        for (size_t k = 0; k < e->kinds.n; k++) have = have || !strcmp(e->kinds.v[k], r->kind);
        if (!have) avp_vec_push(&e->kinds, r->kind);
        if (r->to_off != SHSM_NONE && r->to_off) {
            bool dup = false;
            for (size_t k = 0; k < e->offsets.n; k++) dup = dup || e->offsets.v[k] == r->to_off;
            if (!dup) avp_vec_push(&e->offsets, r->to_off);
        }
    }
    static const char *const gnames[3] = {"calls", "data", "pointers"};
    static const char *const titles[3] = {"calls (bl / tail b)", "data access (ha/l/h, sda21)",
                                          "pointers in data (vtables, tables, .ctors, jump tables)"};
    AvpJson *result[3];
    int total = 0;
    for (int g = 0; g < 3; g++) {
        result[g] = avp_json_arr(a);
        if (!groups[g].n) continue;
        i64 nsites = 0;
        for (size_t i = 0; i < groups[g].n; i++) nsites += (i64)groups[g].v[i].sites.n;
        shsm_output_line(out, (avp_pyfmt(a, "%s: %d referrers, %d sites", titles[g], groups[g].n, nsites)));
        for (size_t i = 0; i < groups[g].n; i++) {
            XrefEntry *e = &groups[g].v[i];
            QuerySymDesc d = query_describe_sym(ctx, e->from, e->sites.v[0]);
            AvpStr extra = AVP_S("");
            if (g == 2 && e->from.p) {
                i64 base = query_base(ctx, e->from);
                if (base != SHSM_NONE) extra = (avp_pyfmt(a, "+0x%X ", e->sites.v[0] - base));
            }
            AvpBuf kb = {0}, ob = {0};
            for (size_t k = 0; k < e->kinds.n; k++) {
                if (k) avp_buf_putz(&kb, "/");
                avp_buf_putz(&kb, e->kinds.v[k]);
            }
            if (e->offsets.n) {
                avp_buf_putz(&ob, " to+");
                for (size_t k = 0; k < e->offsets.n && k < 6; k++)
                    avp_buf_put(&ob, (avp_pyfmt(a, k ? ",0x%X" : "0x%X", e->offsets.v[k])));
                if (e->offsets.n > 6) avp_buf_put(&ob, (avp_pyfmt(a, ",... (%d)", e->offsets.n)));
            }
            AvpStr kinds = avp_str_dup(a, avp_buf_str(&kb)), off = avp_str_dup(a, avp_buf_str(&ob));
            avp_buf_free(&kb);
            avp_buf_free(&ob);
            AvpStr tag = (avp_pyfmt(a, "%3dx %-9S %S", e->sites.n, &kinds, &extra));
            AvpStr sl = query_sym_line(a, &d, tag);
            shsm_output_line(out, (avp_pyfmt(a, "  %S%S", &sl, &off)));
            AvpJson *jd = query_desc_json(a, &d);
            AvpJson *js = avp_json_arr(a), *jk = avp_json_arr(a), *jo = avp_json_arr(a);
            for (size_t k = 0; k < e->sites.n; k++) avp_json_arr_push(a, js, avp_json_int(a, e->sites.v[k]));
            for (size_t k = 0; k < e->kinds.n; k++) avp_json_arr_push(a, jk, avp_json_strz(a, e->kinds.v[k]));
            for (size_t k = 0; k < e->offsets.n; k++) avp_json_arr_push(a, jo, avp_json_int(a, e->offsets.v[k]));
            avp_json_set(a, jd, AVP_S("sites"), js);
            avp_json_set(a, jd, AVP_S("kinds"), jk);
            avp_json_set(a, jd, AVP_S("offsets"), jo);
            avp_json_arr_push(a, result[g], jd);
            total++;
            avp_vec_free(&e->sites);
            avp_vec_free(&e->offsets);
            avp_vec_free(&e->kinds);
        }
        avp_vec_free(&groups[g]);
    }
    if (!total) shsm_output_linez(out, "  (no references in the retail asm)");
    shsm_output_data(out, "symbol", query_desc_json(a, &root));
    for (int g = 0; g < 3; g++) shsm_output_data(out, gnames[g], result[g]);
    for (int g = 0; g < 3; g++) avp_map_free(keys[g]);
    return SHSM_EXIT_OK;
}
