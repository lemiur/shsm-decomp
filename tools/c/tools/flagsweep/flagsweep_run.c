/* flagsweep_run: compiling the variants, the sweep, the report, the --json document and the command line (see
 * flagsweep.h). */
#include "flagsweep.h"

#include "permute.h"

/* ---- compilation ------------------------------------------------------------------------------------------------ */

typedef struct {
    const char *root, *source, *outroot;
    const AvpMap *retail;
    CcProject *project;
    AvpStr unit;
    bool targets;
    FsScore **scores; /* per variant, NULL when it did not compile */
} FsSweep;

static const char **compiler_cmd(AvpArena *a, const char *root, const FsVariant *v, const char *source,
                                 const char *obj)
{
    const char *exe = avp_sprintf(a, "%s\\tools\\compilers\\%s\\mwcceppc.exe", root,
                                  avp_cz(a, avp_str_replace(a, v->version, AVP_S("/"), AVP_S("\\"), -1)));
    const char *wrap = permute_join(a, root, "build\\tools\\sjiswrap.exe");
    AvpStrVec toks = fs_flag_tokens(&v->flags);
    const char **cmd = avp_alloc(a, (toks.n + 7) * sizeof *cmd);
    size_t n = 0;
    if (avp_exists(wrap)) cmd[n++] = wrap;
    cmd[n++] = exe;
    for (size_t k = 0; k < toks.n; k++) cmd[n++] = avp_cz(a, toks.v[k]);
    cmd[n++] = "-c";
    cmd[n++] = avp_path_abspath(a, source);
    cmd[n++] = "-o";
    cmd[n++] = obj;
    cmd[n] = NULL;
    avp_vec_free(&toks);
    return cmd;
}

/* compile_variant, then score_variant (the Python scored after the pool; each variant has its own context here) */
static int compile_one(void *ctx, size_t i, const void *item, void *result)
{
    (void)result;
    FsSweep *sw = ctx;
    FsVariant *v = (FsVariant *)item;
    AvpArena *a = avp_arena();
    const char *outdir = avp_sprintf(a, "%s\\%.*s", sw->outroot, AVP_SF(v->id));
    avp_rmtree(outdir, 0);
    if (!avp_makedirs(outdir, false)) avp_fatal("%s", avp_io_err_str(a).p);
    AvpStr stem, ext;
    avp_path_splitext(avp_cz(a, avp_path_basename(sw->source)), &stem, &ext);
    const char *obj = avp_sprintf(a, "%s\\%.*s.o", outdir, AVP_SF(stem));
    double t0 = avp_time();
    AvpProcResult r = avp_run(&(AvpProcSpec){.argv = compiler_cmd(a, sw->root, v, sw->source, obj), .cwd = sw->root,
                                             .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE, .timeout = 600,
                                             .arena = a});
    if (r.status == AVP_PROC_TIMEOUT) {
        v->error = AVP_S("compiler timeout");
        return 0;
    }
    if (r.status == AVP_PROC_START_FAILED) avp_fatal("FileNotFoundError: %s", r.message);
    if (r.returncode || !avp_exists(obj)) {
        AvpStr text = permute_universal_newlines(a, avp_str_concat(a, r.out, r.err));
        v->error = fs_first_error(a, text);
        if (!v->error.n) v->error = avp_str(avp_sprintf(a, "compiler exit %lld", (long long)r.returncode));
        return 0;
    }
    AvpStr err = {0};
    v->fns = cc_load_object(a, obj, NULL, &err);
    if (v->fns) v->compiled = true;
    else v->error = avp_str(avp_sprintf(a, "disassembly failed: %.*s", AVP_SF(err)));
    v->has_seconds = true;
    v->seconds = avp_py_round(avp_time() - t0, 2);
    if (v->compiled) {
        CcContext *cx = cc_context_new(sw->project, sw->unit);
        sw->scores[i] = fs_score_variant(a, sw->retail, v->fns, cx, sw->targets);
    }
    return 0;
}

static int rank_cmp(const FsEntry *e, const FsEntry *p)
{
    i64 ke[4] = {!e->ok, -(e->ok ? e->exact : 0), e->ok ? e->diff + e->targets : 0, !e->v->current};
    i64 kp[4] = {!p->ok, -(p->ok ? p->exact : 0), p->ok ? p->diff + p->targets : 0, !p->v->current};
    for (int q = 0; q < 4; q++)
        if (ke[q] != kp[q]) return ke[q] < kp[q] ? -1 : 1;
    return 0;
}

void fs_rank(FsResult *res)
{
    size_t n = res->nentries;
    res->ranking = avp_alloc(avp_arena(), (n + 1) * sizeof *res->ranking);
    for (size_t k = 0; k < n; k++) {
        /* stable insertion: after every earlier entry that is not worse */
        size_t j = k;
        while (j > 0 && rank_cmp(&res->entries[k], &res->entries[res->ranking[j - 1]]) < 0) {
            res->ranking[j] = res->ranking[j - 1];
            j--;
        }
        res->ranking[j] = k;
    }
}

static FsResult sweep(AvpArena *a, FsSweep *sw, FsVariants *vs, i64 jobs, const AvpStrVec *symbols)
{
    double t0 = avp_time();
    sw->scores = avp_alloc(a, (vs->n + 1) * sizeof *sw->scores);
    avp_pool_map(NULL, jobs < 1 ? 1 : (int)jobs, compile_one, sw, vs->v, vs->n, sizeof *vs->v, NULL, 0);
    FsResult res = {0};
    res.retail_functions = avp_map_len(sw->retail);
    /* universe: retail functions that some successful variant actually compiled */
    size_t idx = 0, npick = 0;
    size_t *pick = avp_alloc(a, (res.retail_functions + 1) * sizeof *pick);
    AvpMapIter it = {0};
    while (avp_map_next(sw->retail, &it)) {
        bool any = false;
        for (size_t v = 0; v < vs->n && !any; v++)
            any = sw->scores[v] && strcmp(sw->scores[v][idx].status, "missing") != 0;
        if (fs_wanted(it.key, symbols) && any) {
            avp_vec_push(&res.functions, it.key);
            pick[npick++] = idx;
        }
        idx++;
    }
    res.nentries = vs->n;
    res.entries = avp_alloc(a, (vs->n + 1) * sizeof *res.entries);
    for (size_t v = 0; v < vs->n; v++) {
        FsEntry *e = &res.entries[v];
        e->v = &vs->v[v];
        if (!sw->scores[v]) continue;
        e->ok = true;
        e->fns = avp_alloc(a, (npick + 1) * sizeof *e->fns);
        for (size_t k = 0; k < npick; k++) {
            const FsScore *f = &sw->scores[v][pick[k]];
            e->fns[k] = *f;
            e->exact += f->exact;
            e->diff += (i64)f->diff;
            e->targets += (i64)f->targets;
            e->unresolved += (i64)f->unresolved;
        }
    }
    fs_rank(&res);
    res.seconds = avp_py_round(avp_time() - t0, 2);
    return res;
}

/* ---- output ----------------------------------------------------------------------------------------------------- */

static void pad_right(AvpBuf *b, AvpStr s, size_t w)
{
    avp_buf_put(b, s);
    for (size_t k = avp_u8_len(s); k < w; k++) avp_buf_putc(b, ' ');
}

static void pad_left(AvpBuf *b, AvpStr s, size_t w)
{
    for (size_t k = avp_u8_len(s); k < w; k++) avp_buf_putc(b, ' ');
    avp_buf_put(b, s);
}

/* Python's // for a positive divisor */
static i64 floordiv(i64 x, i64 y) { return x >= 0 ? x / y : -((-x + y - 1) / y); }

AvpStr fs_format_report(AvpArena *a, const FsResult *res, AvpStr header, bool show_flags, i64 width)
{
    AvpBuf b;
    avp_buf_init(&b, a);
    size_t nf = res->functions.n;
    if (header.n) {
        avp_buf_put(&b, header);
        avp_buf_putc(&b, '\n');
    }
    avp_buf_put(&b, avp_pyfmt(a, "variants: %d   functions scored: %d (of %d in the retail listing)   %.1fs",
                              res->nentries, nf, res->retail_functions, res->seconds));
    avp_buf_putz(&b, "\n\nlegend (* = the unit's current configuration):");
    for (size_t k = 0; k < res->nentries; k++) {
        const FsEntry *e = &res->entries[k];
        avp_buf_putz(&b, "\n ");
        pad_right(&b, e->v->id, 4);
        avp_buf_putz(&b, e->v->current ? "* " : "  ");
        avp_buf_put(&b, e->v->label);
        if (show_flags) {
            AvpStrVec toks = fs_flag_tokens(&e->v->flags);
            avp_buf_putz(&b, "\n        ");
            avp_buf_put(&b, permute_shlex_join(a, toks.v, toks.n));
            avp_vec_free(&toks);
        }
        if (!e->ok && e->v->error.n) {
            avp_buf_putz(&b, "\n        compile error: ");
            avp_buf_put(&b, avp_u8_slice(e->v->error, 0, 150));
        }
    }
    avp_buf_putz(&b, "\n\nmatrix (cell = differing rows, Nt = N target mismatches, ok = exact, ok? = exact but some "
                     "targets unresolved, -- = function not compiled, ERR = compile failed):");
    size_t name_w = 8;
    for (size_t k = 0; k < nf; k++)
        if (avp_u8_len(res->functions.v[k]) > name_w) name_w = avp_u8_len(res->functions.v[k]);
    if (name_w > 44) name_w = 44;
    AvpStr *cells = avp_alloc(a, (res->nentries * nf + 1) * sizeof *cells);
    size_t col_w = 4;
    for (size_t v = 0; v < res->nentries; v++) {
        for (size_t k = 0; k < nf; k++) {
            AvpStr c = res->entries[v].ok ? fs_cell(a, &res->entries[v].fns[k]) : AVP_S("ERR");
            cells[v * nf + k] = c;
            if (c.n > col_w) col_w = c.n;
        }
        if (res->entries[v].v->id.n + 1 > col_w) col_w = res->entries[v].v->id.n + 1;
    }
    col_w += 1;
    i64 per = floordiv(width - (i64)name_w - 1, (i64)col_w);
    if (per < 1) per = 1;
    for (size_t k0 = 0; k0 < res->nentries; k0 += (size_t)per) {
        size_t k1 = k0 + (size_t)per < res->nentries ? k0 + (size_t)per : res->nentries;
        avp_buf_putc(&b, '\n');
        pad_right(&b, AVP_S("function"), name_w);
        avp_buf_putc(&b, ' ');
        for (size_t v = k0; v < k1; v++) {
            const FsVariant *var = res->entries[v].v;
            pad_left(&b, var->current ? avp_str_concat(a, var->id, AVP_S("*")) : var->id, col_w);
        }
        for (size_t k = 0; k < nf; k++) {
            AvpStr n = res->functions.v[k], label = n;
            i64 len = (i64)avp_u8_len(n);
            if (len > (i64)name_w) /* n[:20] + "~" + n[-(name_w - 21):] */
                label = avp_str(avp_sprintf(a, "%.*s~%.*s", AVP_SF(avp_u8_slice(n, 0, 20)),
                                            AVP_SF(avp_u8_slice(n, len - ((i64)name_w - 21), len))));
            avp_buf_putc(&b, '\n');
            pad_right(&b, label, name_w);
            avp_buf_putc(&b, ' ');
            for (size_t v = k0; v < k1; v++) pad_left(&b, cells[v * nf + k], col_w);
        }
        avp_buf_putc(&b, '\n');
    }
    avp_buf_putz(&b, "\nranking (functions exact, then fewer differing rows + target mismatches):");
    avp_buf_putz(&b, "\n rank var   exact    diffrows  targets  unres   configuration");
    for (size_t r = 0; r < res->nentries; r++) {
        const FsEntry *e = &res->entries[res->ranking[r]];
        AvpStr id = e->v->id;
        if (!e->ok) {
            AvpStr label = avp_str_concat(a, e->v->label, AVP_S("  [compile failed]"));
            avp_buf_put(&b, avp_pyfmt(a, "\n %-4d %-5S %-8s %-9s %-8s %-7s %S", r + 1, &id, "-", "-", "-", "-",
                                      &label));
        } else {
            AvpStr ex = avp_pyfmt(a, "%d/%d", e->exact, nf), label = e->v->label;
            avp_buf_put(&b, avp_pyfmt(a, "\n %-4d %-5S %-8S %-9d %-8d %-7d %S%s", r + 1, &id, &ex, e->diff, e->targets,
                                      e->unresolved, &label, e->v->current ? "  [current]" : ""));
        }
    }
    return avp_buf_str(&b);
}

/* ---- --json ----------------------------------------------------------------------------------------------------- */

static AvpJson *jstrs(AvpArena *a, const AvpStr *v, size_t n)
{
    AvpJson *arr = avp_json_arr(a);
    for (size_t k = 0; k < n; k++) avp_json_arr_push(a, arr, avp_json_str(a, v[k]));
    return arr;
}

static AvpJson *jint_or_null(AvpArena *a, bool ok, i64 v) { return ok ? avp_json_int(a, v) : avp_json_null(); }

static AvpJson *result_json(AvpArena *a, const FsResult *res, AvpStr unit, AvpStr source, AvpStr base_version,
                            const AvpStrVec *base_flags)
{
    AvpJson *doc = avp_json_obj(a);
    avp_json_set(a, doc, AVP_S("functions"), jstrs(a, res->functions.v, res->functions.n));
    avp_json_set(a, doc, AVP_S("retail_functions"), avp_json_int(a, (i64)res->retail_functions));
    AvpJson *vars = avp_json_arr(a);
    for (size_t k = 0; k < res->nentries; k++) {
        const FsEntry *e = &res->entries[k];
        const FsVariant *v = e->v;
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("id"), avp_json_str(a, v->id));
        avp_json_set(a, o, AVP_S("label"), avp_json_str(a, v->label));
        avp_json_set(a, o, AVP_S("version"), avp_json_str(a, v->version));
        avp_json_set(a, o, AVP_S("current"), avp_json_bool(v->current));
        avp_json_set(a, o, AVP_S("choices"), jstrs(a, v->choices.v, v->choices.n));
        AvpStrVec toks = fs_flag_tokens(&v->flags);
        avp_json_set(a, o, AVP_S("flags"), jstrs(a, toks.v, toks.n));
        avp_vec_free(&toks);
        avp_json_set(a, o, AVP_S("seconds"), v->has_seconds ? avp_json_float(a, v->seconds) : avp_json_null());
        avp_json_set(a, o, AVP_S("status"), avp_json_strz(a, e->ok ? "ok" : "compile failed"));
        if (!e->ok)
            avp_json_set(a, o, AVP_S("error"), avp_json_str(a, v->error.p ? v->error : AVP_S("")));
        avp_json_set(a, o, AVP_S("exact"), jint_or_null(a, e->ok, e->exact));
        avp_json_set(a, o, AVP_S("diff"), jint_or_null(a, e->ok, e->diff));
        avp_json_set(a, o, AVP_S("targets"), jint_or_null(a, e->ok, e->targets));
        avp_json_set(a, o, AVP_S("unresolved"), jint_or_null(a, e->ok, e->unresolved));
        AvpJson *fns = avp_json_obj(a);
        for (size_t f = 0; e->ok && f < res->functions.n; f++) {
            const FsScore *s = &e->fns[f];
            AvpJson *fo = avp_json_obj(a);
            avp_json_set(a, fo, AVP_S("exact"), avp_json_bool(s->exact));
            avp_json_set(a, fo, AVP_S("diff"), avp_json_int(a, (i64)s->diff));
            avp_json_set(a, fo, AVP_S("rows"), avp_json_int(a, (i64)s->rows));
            avp_json_set(a, fo, AVP_S("targets"), avp_json_int(a, (i64)s->targets));
            avp_json_set(a, fo, AVP_S("unresolved"), avp_json_int(a, (i64)s->unresolved));
            avp_json_set(a, fo, AVP_S("status"), avp_json_strz(a, s->status));
            avp_json_set(a, fns, res->functions.v[f], fo);
        }
        avp_json_set(a, o, AVP_S("functions"), fns);
        if (e->ok) avp_json_set(a, o, AVP_S("error"), avp_json_null());
        avp_json_arr_push(a, vars, o);
    }
    avp_json_set(a, doc, AVP_S("variants"), vars);
    AvpJson *rank = avp_json_arr(a);
    for (size_t r = 0; r < res->nentries; r++)
        avp_json_arr_push(a, rank, avp_json_str(a, res->entries[res->ranking[r]].v->id));
    avp_json_set(a, doc, AVP_S("ranking"), rank);
    avp_json_set(a, doc, AVP_S("seconds"), avp_json_float(a, res->seconds));
    avp_json_set(a, doc, AVP_S("unit"), avp_json_str(a, unit));
    avp_json_set(a, doc, AVP_S("source"), avp_json_str(a, source));
    avp_json_set(a, doc, AVP_S("base_version"), avp_json_str(a, base_version));
    avp_json_set(a, doc, AVP_S("base_flags"), jstrs(a, base_flags->v, base_flags->n));
    return doc;
}

/* ---- the command line ------------------------------------------------------------------------------------------- */

int flagsweep_main(int argc, char **argv)
{
    AvpArena *a = avp_arena();
    /* glue_values: "--vary VALUE" -> "--vary=VALUE" for the options whose values are flag text */
    char **glued = avp_alloc(a, ((size_t)argc + 1) * sizeof *glued);
    int ng = 0;
    glued[ng++] = argv[0];
    for (int i = 1; i < argc;) {
        if ((!strcmp(argv[i], "--vary") || !strcmp(argv[i], "--set") || !strcmp(argv[i], "--drop")) && i + 1 < argc) {
            glued[ng++] = avp_sprintf(a, "%s=%s", argv[i], argv[i + 1]);
            i += 2;
        } else {
            glued[ng++] = argv[i++];
        }
    }
    static const char *const none[] = {NULL};
    AvpArgParser *ap = avp_args_parser(a, &(AvpArgParserDesc){
        .prog = "flagsweep.exe", .description = "Sweep compiler versions x flags over a unit."});
    avp_arg(ap, &(AvpArgDesc){.names = "unit"});
    avp_arg(ap, &(AvpArgDesc){.names = "-v --versions"});
    avp_arg(ap, &(AvpArgDesc){.names = "--vary", .action = AVP_ARG_APPEND, .dflt_list = none});
    avp_arg(ap, &(AvpArgDesc){.names = "--set", .action = AVP_ARG_APPEND, .dflt_list = none, .dest = "sets"});
    avp_arg(ap, &(AvpArgDesc){.names = "--drop", .action = AVP_ARG_APPEND, .dflt_list = none});
    avp_arg(ap, &(AvpArgDesc){.names = "-s", .action = AVP_ARG_APPEND, .dflt_list = none, .dest = "symbols"});
    i64 cpus = avp_cpu_count();
    avp_arg(ap, &(AvpArgDesc){.names = "-j", .type = AVP_T_INT, .dflt = AVP_DEF_INT(cpus < 8 ? cpus : 8)});
    avp_arg(ap, &(AvpArgDesc){.names = "--max", .type = AVP_T_INT, .dflt = AVP_DEF_INT(64)});
    avp_arg(ap, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE});
    avp_arg(ap, &(AvpArgDesc){.names = "--show-flags", .action = AVP_ARG_STORE_TRUE});
    avp_arg(ap, &(AvpArgDesc){.names = "--no-base", .action = AVP_ARG_STORE_TRUE});
    avp_arg(ap, &(AvpArgDesc){.names = "--width", .type = AVP_T_INT, .dflt = AVP_DEF_INT(140)});
    avp_arg(ap, &(AvpArgDesc){.names = "--no-targets", .action = AVP_ARG_STORE_TRUE});
    AvpArgs *args = avp_args_parse(ap, ng, glued);

    const char *root = permute_repo();
    AvpStr unit = avp_args_str(args, "unit");
    if (avp_str_startswith(unit, AVP_S("main/"))) unit = avp_str_slice(unit, 5, (i64)unit.n);
    const char *base_ver;
    AvpStrVec base_tokens;
    permute_unit_flags(a, root, unit, &base_ver, &base_tokens);
    const char *source = permute_find_source(a, root, unit);
    const char *asm_path = permute_join(a, root, avp_sprintf(a, "build\\R5WEA4\\asm\\%.*s.s", AVP_SF(unit)));
    if (!avp_exists(asm_path)) avp_fatal("retail asm not found: %s", asm_path);
    AvpStr err = {0};
    AvpMap *retail = cc_load_asm(a, asm_path, &err);
    if (!retail) avp_fatal("%.*s", AVP_SF(err));
    AvpStrVec versions = {0};
    if (!avp_args_is_none(args, "versions"))
        versions = fs_resolve_versions(a, avp_args_str(args, "versions"), permute_join(a, root, "tools\\compilers"));
    FsFlags base_flags = fs_parse_flags(base_tokens.v, base_tokens.n);
    AvpStrVec vary = avp_args_strs(args, "vary"), sets = avp_args_strs(args, "sets"),
              drops = avp_args_strs(args, "drop"), symbols = avp_args_strs(args, "symbols");
    FsVariants variants = fs_build_variants(a, avp_str(base_ver), &base_flags, &versions, &vary, &sets, &drops,
                                            !avp_args_bool(args, "no_base"), avp_args_int(args, "max"));
    AvpStr flat = avp_str_replace(a, unit, AVP_S("/"), AVP_S("_"), -1);
    FsSweep sw = {.root = root, .source = source, .retail = retail, .project = cc_project_new(root), .unit = unit,
                  .targets = !avp_args_bool(args, "no_targets"),
                  .outroot = avp_sprintf(a, "%s\\build\\tmp\\flagsweep\\%.*s", root, AVP_SF(flat))};
    FsResult res = sweep(a, &sw, &variants, avp_args_int(args, "j"), &symbols);
    char *rel = NULL;
    if (!avp_path_relpath(a, source, root, &rel)) avp_fatal("%s", avp_io_err_str(a).p);
    AvpStr rel_src = avp_str_replace(a, avp_str(rel), AVP_S("\\"), AVP_S("/"), -1);
    if (avp_args_bool(args, "json")) {
        AvpJson *doc = result_json(a, &res, unit, rel_src, avp_str(base_ver), &base_tokens);
        AvpStr text = avp_json_dumps(a, doc, &AVP_JSON_INDENT(1));
        fwrite(text.p, 1, text.n, stdout);
        fputc('\n', stdout);
    } else {
        AvpStr header = avp_str(avp_sprintf(a, "flagsweep: %.*s  (%.*s)  base: %s", AVP_SF(unit), AVP_SF(rel_src),
                                            base_ver));
        AvpStr text = fs_format_report(a, &res, header, avp_args_bool(args, "show_flags"), avp_args_int(args, "width"));
        fwrite(text.p, 1, text.n, stdout);
        fputc('\n', stdout);
    }
    for (size_t k = 0; k < res.nentries; k++)
        if (res.entries[k].ok) return 0;
    return 1;
}
