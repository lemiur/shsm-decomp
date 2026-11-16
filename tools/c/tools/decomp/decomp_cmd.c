/* decomp_cmd: the subcommands of tools/decomp.py (cmd_setup, select_functions, resolve, export, cmd_export,
 * listing, cmd_show, cmd_refresh, cmd_status, _state). */
#include "decomp.h"
#include "shsm_asmindex.h"
#include "shsm_re.h"
#include "shsm_units.h"

#include <stdio.h>
#include <string.h>

static void say(AvpStr s) {
    fwrite(s.p, 1, s.n, stdout);
    fputc('\n', stdout);
}
#define SAY(...) say(avp_pyfmt(a, __VA_ARGS__))

/* an AvpStr at a stable address, for avp_pyfmt's %S */
static const AvpStr *box(AvpArena *a, AvpStr s) {
    AvpStr *p = AVP_NEW(a, AvpStr);
    *p = s;
    return p;
}

static const char *jstr_cz(AvpArena *a, const AvpJson *obj, const char *key) {
    const AvpJson *v = avp_json_getz(obj, key);
    if (!v) shsm_raise(SHSM_EXC_KEY, "%s", key);
    if (v->kind != AVP_JSON_STRING) return avp_cz(a, dc_pystr(a, v));
    return avp_cz(a, v->u.str);
}

static const char *now_text(AvpArena *a) {
    AvpStr s;
    avp_strftime_now(a, "%Y-%m-%d %H:%M:%S", &s);
    return avp_cz(a, s);
}

static ShsmRepo *find_repo(const DcArgs *g) { return shsm_repo_find(g->root); }

AvpJson *dc_state(AvpArena *a, const DecompPaths *p, bool need_project) {
    AvpJson *st = dc_load_json(a, p->state, NULL);
    if (!st || avp_json_is_null(st) || (need_project && !decomp_project_exists(p)))
        shsm_raise(SHSM_EXC_USAGE, "no Ghidra project: run `build/tools/decomp.exe setup` first");
    return st;
}

/* ---- setup ---- */

int dc_cmd_setup(const DcArgs *g) {
    AvpArena *a = avp_arena();
    ShsmRepo *repo = find_repo(g);
    DecompPaths *p = decomp_paths(a, repo);
    const char *how = NULL;
    DcInstall info = dc_locate_ghidra(a, g->ghidra, avp_getenv(a, "GHIDRA_INSTALL_DIR"), NULL, 0, NULL, &how);
    i64 jv = dc_java_major();
    if (jv < 0)
        shsm_raise(SHSM_EXC_USAGE, "no java on PATH (Ghidra %s needs JDK %lld+)", info.version,
                   (long long)info.java_min);
    if (jv < info.java_min)
        shsm_raise(SHSM_EXC_USAGE, "java %lld on PATH, Ghidra %s needs JDK %lld+", (long long)jv, info.version,
                   (long long)info.java_min);
    if (decomp_project_exists(p) && !g->force)
        shsm_raise(SHSM_EXC_USAGE, "project exists (%s); use --force to rebuild it, or `refresh`",
                   shsm_repo_rel(a, repo, p->project));
    const char *elf = avp_join(a, repo->version_build_dir, "main.elf");
    const char *source = avp_isfile(elf) ? elf : repo->retail_dol;
    if (!source || !avp_isfile(source))
        shsm_raise(SHSM_EXC_USAGE, "no input: neither %s nor the retail DOL exists", shsm_repo_rel(a, repo, elf));
    if (!avp_makedirs(p->input, true)) shsm_raise_io();
    AvpStr root_part, ext;
    avp_path_splitext(source, &root_part, &ext);
    const char *snapshot = avp_path_join(a, p->input, avp_sprintf(a, "main%s", avp_cz(a, ext)));
    if (!avp_copyfile(source, snapshot)) shsm_raise_io(); /* the build may relink under us */
    ShsmSymbolTable *table = shsm_load_symbols(repo, true);
    DcRows rows = dc_symbol_rows(a, table);
    dc_write_rows(a, p->symbols_tsv, rows.v, rows.n);
    const char *stamp = decomp_symbols_stamp(a, repo->symbols_txt);
    u64 sda, sda2;
    dc_sda_bases(a, snapshot, table, &sda, &sda2);
    if (g->force && avp_isdir(p->project) && !avp_rmtree(p->project, 0)) shsm_raise_io();
    if (!avp_makedirs(p->project, true)) shsm_raise_io();
    bool is_elf = avp_str_endswith(avp_str_lower(a, avp_str(snapshot)), AVP_S(".elf"));
    const char *pre_args[] = {"pre", p->symbols_tsv, avp_sprintf(a, "%08llx", (unsigned long long)sda),
                              avp_sprintf(a, "%08llx", (unsigned long long)sda2)};
    const char *post_args[] = {"post", p->symbols_tsv};
    DcScript pre = {"ShsmApplySymbols.java", pre_args, 4}, post = {"ShsmApplySymbols.java", post_args, 2};
    DcHeadlessOpts o = {.import_path = snapshot, .processor = info.language, .cspec = "default",
                        /* named: other installed loaders claim the ELF too */
                        .loader = is_elf ? "ElfLoader" : NULL, .pre = &pre, .npre = 1, .post = &post, .npost = 1,
                        .overwrite = true};
    size_t argc;
    const char **argv = dc_headless_argv(a, info.install, p->project, &o, &argc);
    SAY("ghidra %s (%s) at %s, language %s; importing %s (%d symbols)", info.version, how, info.install,
        info.language, shsm_repo_rel(a, repo, source), (i64)rows.n);
    fflush(stdout);
    AvpStr log;
    double seconds = dc_run_headless(a, p, argv, argc, "setup", &log);
    AvpJson *applied = avp_json_arr(a);
    AvpStrVec lines = avp_str_splitlines(log, false);
    for (size_t i = 0; i < lines.n; i++) {
        if (!avp_str_contains(lines.v[i], AVP_S("APPLY DONE"))) continue;
        AvpStr ln = lines.v[i];
        i64 k = avp_str_rfind(ln, AVP_S("INFO  "), 0, AVP_END);
        avp_json_arr_push(a, applied, avp_json_str(a, k < 0 ? ln : avp_str_slice(ln, k + 6, AVP_END)));
    }
    avp_vec_free(&lines);
    ShsmReMatch m;
    bool has_analysis = shsm_re_search(shsm_re("Total Time\\s+(\\d+) secs", 0), log, 0, AVP_END, &m);
    if (!avp_copyfile(p->symbols_tsv, p->applied_tsv)) shsm_raise_io();
    AvpJson *st = avp_json_obj(a);
#define SETS(k, v) avp_json_set(a, st, AVP_S(k), avp_json_strz(a, (v)))
    SETS("install", info.install);
    SETS("version", info.version);
    SETS("release", info.release);
    avp_json_set(a, st, AVP_S("java_min"), avp_json_int(a, info.java_min));
    SETS("settings_dir", info.settings_dir);
    SETS("language", info.language);
    SETS("how", how);
    SETS("input", shsm_repo_rel(a, repo, source));
    SETS("snapshot", shsm_repo_rel(a, repo, snapshot));
    SETS("program", avp_cz(a, avp_path_basename(snapshot)));
    SETS("stamp", stamp);
    SETS("sda_base", avp_sprintf(a, "%08llX", (unsigned long long)sda));
    SETS("sda2_base", avp_sprintf(a, "%08llX", (unsigned long long)sda2));
    avp_json_set(a, st, AVP_S("context"), avp_json_int(a, DC_CONTEXT_VERSION));
    avp_json_set(a, st, AVP_S("setup_seconds"), avp_json_float(a, avp_py_round(seconds, 1)));
    AvpJson *analysis = has_analysis ? avp_json_int(a, dc_int10(shsm_re_group(&m, 1))) : avp_json_null();
    avp_json_set(a, st, AVP_S("analysis_seconds"), analysis);
    avp_json_set(a, st, AVP_S("applied"), applied);
    SETS("created", now_text(a));
#undef SETS
    dc_save_json(p->state, st);
    for (size_t i = 0; i < avp_json_len(applied); i++) say(avp_str_strip(avp_json_at(applied, i)->u.str));
    SAY("setup done in %.0f s (auto-analysis %s s); project %s", seconds, avp_cz(a, dc_pystr(a, analysis)),
        shsm_repo_rel(a, repo, p->project));
    return SHSM_EXIT_OK;
}

/* ---- export ---- */

const ShsmSymbol *dc_resolve(AvpArena *a, const ShsmSymbolTable *t, const char *query) {
    const ShsmSymbol *s = shsm_sym_by_name(t, avp_str(query));
    ShsmReMatch m;
    if (!s && shsm_re_fullmatch(shsm_re("(0x)?[0-9A-Fa-f]{8}", 0), avp_str(query), 0, AVP_END, &m)) {
        u64 addr = (u64)dc_int16(avp_str(query));
        AvpPtrVec at = {0};
        shsm_sym_at(t, addr, NULL, &at);
        for (size_t i = 0; i < at.n && !s; i++) {
            const ShsmSymbol *x = at.v[i];
            if (x->type && !strcmp(x->type, "function")) s = x;
        }
        avp_vec_free(&at);
        if (!s) s = shsm_sym_containing(t, addr);
    }
    if (!s) {
        AvpPtrVec fs = {0};
        shsm_sym_functions(t, &fs);
        const char *prefix = avp_sprintf(a, "%s__", query);
        size_t plen = strlen(prefix), count = 0;
        const ShsmSymbol *cand = NULL;
        for (size_t i = 0; i < fs.n; i++) {
            const ShsmSymbol *x = fs.v[i];
            if (!strncmp(x->name, prefix, plen)) cand = x, count++;
        }
        avp_vec_free(&fs);
        if (count == 1) s = cand;
    }
    if (!s || !s->type || strcmp(s->type, "function") != 0)
        shsm_raise(SHSM_EXC_KEY, "no function %s in symbols.txt", avp_pyfmt(a, "%r", query).p);
    return s;
}

static const ShsmSymbol *first_function_at(const ShsmSymbolTable *t, u64 addr) {
    AvpPtrVec at = {0};
    shsm_sym_at(t, addr, NULL, &at);
    const ShsmSymbol *s = NULL;
    for (size_t i = 0; i < at.n && !s; i++) {
        const ShsmSymbol *x = at.v[i];
        if (x->type && !strcmp(x->type, "function")) s = x;
    }
    avp_vec_free(&at);
    return s;
}

static AvpPtrVec select_functions(AvpArena *a, const DcArgs *g, const ShsmRepo *repo, const ShsmSymbolTable *t) {
    AvpPtrVec all = {0}, chosen = {0};
    shsm_sym_functions(t, &all);
    if (g->every) {
        AvpPtrVec funcs = {0};
        for (size_t i = 0; i < all.n; i++) {
            const ShsmSymbol *s = all.v[i];
            if (s->has_size && s->size) avp_vec_push(&funcs, (void *)s);
        }
        avp_vec_free(&all);
        return funcs;
    }
    avp_vec_free(&all);
    if ((g->unit && *g->unit) || (g->lib && *g->lib)) {
        AvpStrVec names = {0};
        if (g->unit && *g->unit) avp_vec_push(&names, avp_str(shsm_unit_name(a, g->unit)));
        if (g->lib && *g->lib) {
            const ShsmUnits *us = shsm_load_units(repo);
            size_t before = names.n;
            for (size_t i = 0; i < us->n; i++)
                if (!strcmp(us->v[i]->library ? us->v[i]->library : "", g->lib))
                    avp_vec_push(&names, avp_str(us->v[i]->name));
            if (names.n == before)
                shsm_raise(SHSM_EXC_USAGE, "no unit has library %s", avp_pyfmt(a, "%r", g->lib).p);
        }
        ShsmAsmIndex *ix = shsm_asmindex_open(&(ShsmAsmIndexOpts){.repo = repo});
        for (size_t i = 0; i < names.n; i++) {
            ShsmSymRows rows = shsm_asmindex_unit_symbols(a, ix, shsm_unit_path(a, avp_cz(a, names.v[i])), "function");
            for (size_t j = 0; j < rows.n; j++) {
                const ShsmSymbol *s = first_function_at(t, (u64)rows.v[j].address);
                if (s) avp_vec_push(&chosen, (void *)s);
            }
        }
        shsm_asmindex_close(ix);
        avp_vec_free(&names);
    }
    for (size_t i = 0; i < g->nsymbols; i++) avp_vec_push(&chosen, (void *)dc_resolve(a, t, g->symbols[i]));
    if (!chosen.n) shsm_raise(SHSM_EXC_USAGE, "nothing selected: name symbols or pass --unit, --lib or --all-functions");
    AvpISet *seen = avp_iset_new();
    AvpPtrVec out = {0};
    for (size_t i = 0; i < chosen.n; i++) {
        const ShsmSymbol *s = chosen.v[i];
        if (avp_iset_add(seen, s->address)) avp_vec_push(&out, (void *)s);
    }
    avp_iset_free(seen);
    avp_vec_free(&chosen);
    return out;
}

void dc_export(AvpArena *a, const DecompPaths *p, const AvpJson *state, const ShsmSymbol *const *funcs, size_t n,
               i64 jobs, i64 timeout, const char *label, i64 *ok, i64 *failed, double *seconds) {
    if (!avp_makedirs(p->decomp, true)) shsm_raise_io();
    const char *work = avp_join(a, p->root, "work");
    if (!avp_makedirs(work, true)) shsm_raise_io();
    const char *addr_file = avp_join(a, work, avp_sprintf(a, "%s_addresses.txt", label));
    const char *results = avp_join(a, work, avp_sprintf(a, "%s_results.tsv", label));
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < n; i++) avp_buf_printf(&b, "%08llX\n", (unsigned long long)funcs[i]->address);
    if (!avp_write_file(addr_file, b.p, b.n)) shsm_raise_io();
    AvpJson *stamp = avp_json_getz(state, "stamp");
    if (!stamp) stamp = avp_json_strz(a, "unknown");
    const char *install = jstr_cz(a, state, "install");
    const char *program = jstr_cz(a, state, "program");
    const char *post_args[] = {p->decomp, addr_file, results, avp_sprintf(a, "%lld", (long long)jobs),
                               avp_sprintf(a, "%lld", (long long)timeout), avp_cz(a, dc_pystr(a, stamp))};
    DcScript post = {"ShsmExportDecomp.java", post_args, 6};
    DcHeadlessOpts o = {.process = program, .no_analysis = true, .read_only = true, .post = &post, .npost = 1};
    size_t argc;
    const char **argv = dc_headless_argv(a, install, p->project, &o, &argc);
    if (avp_exists(results) && !avp_remove(results)) shsm_raise_io();
    AvpStr log;
    *seconds = dc_run_headless(a, p, argv, argc, label, &log);
    if (!avp_isfile(results))
        shsm_raise(SHSM_EXC_USAGE, "the export wrote no results (log: %s)",
                   shsm_repo_rel(a, p->repo, avp_join(a, p->logs, avp_sprintf(a, "%s.log", label))));
    AvpJson *index = decomp_load_index(a, p);
    dc_merge_results(a, index, results, stamp, avp_json_getz(state, "version"), ok, failed);
    AvpJson *last = avp_json_obj(a);
    avp_json_set(a, last, AVP_S("functions"), avp_json_int(a, *ok + *failed));
    avp_json_set(a, last, AVP_S("failed"), avp_json_int(a, *failed));
    avp_json_set(a, last, AVP_S("seconds"), avp_json_float(a, avp_py_round(*seconds, 1)));
    avp_json_set(a, last, AVP_S("jobs"), avp_json_int(a, jobs));
    avp_json_set(a, last, AVP_S("when"), avp_json_strz(a, now_text(a)));
    avp_json_set(a, index, AVP_S("last_export"), last);
    dc_save_json(p->index, index);
}

int dc_cmd_export(const DcArgs *g) {
    AvpArena *a = avp_arena();
    ShsmRepo *repo = find_repo(g);
    DecompPaths *p = decomp_paths(a, repo);
    AvpJson *state = dc_state(a, p, true);
    ShsmSymbolTable *t = shsm_load_symbols(repo, true);
    AvpPtrVec funcs = select_functions(a, g, repo, t);
    if (g->missing) {
        AvpJson *index = decomp_load_index(a, p);
        size_t k = 0;
        for (size_t i = 0; i < funcs.n; i++) {
            const ShsmSymbol *s = funcs.v[i];
            if (dc_json_ne_str(decomp_index_entry(index, s->address), "status", "ok")) funcs.v[k++] = (void *)s;
        }
        funcs.n = k;
    }
    if (!funcs.n) {
        say(AVP_S("nothing to export"));
        return SHSM_EXIT_OK;
    }
    i64 ok, failed;
    double seconds;
    dc_export(a, p, state, (const ShsmSymbol *const *)funcs.v, funcs.n, g->jobs, g->timeout, "export", &ok, &failed,
              &seconds);
    double rate = seconds != 0.0 ? (double)(ok + failed) / seconds : 0.0;
    SAY("exported %d functions, %d failed, in %.0f s (%.1f per second, including about 20 s of startup); "
        "listings in %s",
        ok + failed, failed, seconds, rate, shsm_repo_rel(a, repo, p->decomp));
    return failed ? SHSM_EXIT_FINDING : SHSM_EXIT_OK;
}

/* ---- show ---- */

DecompListing dc_listing_sym(AvpArena *a, const ShsmRepo *r, const ShsmSymbol *s, bool do_export, i64 timeout) {
    DecompPaths *p = decomp_paths(a, r);
    DecompListing out = {0};
    AvpJson *e = decomp_index_entry(decomp_load_index(a, p), s->address);
    if (!dc_entry_has_file(a, p, e)) {
        if (!do_export) {
            out.entry = e;
            return out;
        }
        AvpJson *state = dc_state(a, p, true);
        fprintf(stderr, "exporting %s (no listing yet; about 30 s of Ghidra startup)\n", s->name);
        fflush(stderr);
        i64 ok, failed;
        double seconds;
        dc_export(a, p, state, &s, 1, 1, timeout, "show", &ok, &failed, &seconds);
        AvpJson *index = decomp_load_index(a, p);
        if (!avp_json_getz(index, "functions")) shsm_raise(SHSM_EXC_KEY, "functions");
        e = decomp_index_entry(index, s->address);
        const AvpJson *f = e ? avp_json_getz(e, "file") : NULL;
        if (!e || !avp_json_truthy(f))
            shsm_raise(SHSM_EXC_FINDING, "Ghidra could not decompile %s: %s", s->name,
                       e ? avp_cz(a, dc_pystr(a, avp_json_getz(e, "reason"))) : "None");
    }
    const AvpJson *f = avp_json_getz(e, "file");
    if (!f) shsm_raise(SHSM_EXC_KEY, "file");
    const char *path = avp_path_join(a, p->decomp, avp_cz(a, dc_pystr(a, f)));
    if (!avp_read_text(a, path, AVP_TM_UTF8, &out.text)) shsm_raise_io();
    out.found = true;
    out.entry = e;
    out.stale = dc_json_ne_str(e, "stamp", decomp_symbols_stamp(a, r->symbols_txt));
    return out;
}

int dc_cmd_show(const DcArgs *g) {
    AvpArena *a = avp_arena();
    ShsmRepo *repo = find_repo(g);
    DecompPaths *p = decomp_paths(a, repo);
    const ShsmSymbol *s = dc_resolve(a, shsm_load_symbols(repo, true), g->symbol);
    const char *key = avp_sprintf(a, "%08llX", (unsigned long long)s->address);
    DecompListing L = dc_listing_sym(a, repo, s, true, g->timeout);
    const char *stamp = decomp_symbols_stamp(a, repo->symbols_txt);
    bool stale = dc_json_ne_str(L.entry, "stamp", stamp);
    ShsmOutput *out = shsm_output_open(g->args, -1, -1, -1);
    shsm_output_linez(out, DECOMP_NOTICE);
    if (stale)
        shsm_output_line(out, avp_pyfmt(a,
                                        "// WARNING: made from an older symbols.txt (%S, now %s): names may be stale; "
                                        "run `build/tools/decomp.exe refresh`",
                                        box(a, dc_pystr(a, avp_json_getz(L.entry, "stamp"))), stamp));
    AvpStr body = avp_str_rstrip_chars(L.text, AVP_S("\n"));
    AvpStrVec lines = avp_str_split(body, AVP_S("\n"), -1);
    shsm_output_lines(out, lines.v, lines.n);
    avp_vec_free(&lines);
    shsm_output_data(out, "symbol", avp_json_strz(a, s->name));
    shsm_output_data(out, "address", avp_json_strz(a, key));
    shsm_output_data(out, "file",
                     avp_json_strz(a, shsm_repo_rel(a, repo, avp_path_join(a, p->decomp,
                                                                           avp_cz(a, dc_pystr(a, avp_json_getz(L.entry, "file")))))));
    shsm_output_data(out, "stale", avp_json_bool(stale));
    AvpJson *status = avp_json_getz(L.entry, "status");
    shsm_output_data(out, "status", status ? status : avp_json_null());
    shsm_output_data(out, "listing", avp_json_str(a, L.text));
    shsm_output_close(out);
    return SHSM_EXIT_OK;
}

/* ---- refresh ---- */

int dc_cmd_refresh(const DcArgs *g) {
    AvpArena *a = avp_arena();
    ShsmRepo *repo = find_repo(g);
    DecompPaths *p = decomp_paths(a, repo);
    AvpJson *state = dc_state(a, p, true);
    ShsmSymbolTable *t = shsm_load_symbols(repo, true);
    DcRows new_ = dc_symbol_rows(a, t);
    DcRows old = dc_read_rows(a, p->applied_tsv);
    const char *stamp = decomp_symbols_stamp(a, repo->symbols_txt);
    DcRows delta;
    DcRanges ranges;
    AvpISet *funcs;
    dc_diff_rows(a, &old, &new_, &delta, &ranges, &funcs);
    const AvpJson *ctx = avp_json_getz(state, "context");
    bool context_old = !(ctx && avp_json_is_num(ctx) && avp_json_f64(ctx, -1) == (double)DC_CONTEXT_VERSION);
    if (delta.n || context_old) {
        const char *delta_path = avp_join(a, p->input, "delta.tsv");
        if (context_old) {
            DcRows w = {0};
            for (size_t i = 0; i < delta.n; i++)
                if (!strcmp(delta.v[i].kind, "R")) avp_vec_push(&w, delta.v[i]);
            for (size_t i = 0; i < new_.n; i++) avp_vec_push(&w, new_.v[i]);
            dc_write_rows(a, delta_path, w.v, w.n);
            avp_vec_free(&w);
        } else {
            dc_write_rows(a, delta_path, delta.v, delta.n);
        }
        const char *post_args[] = {"refresh", delta_path, jstr_cz(a, state, "sda_base"), jstr_cz(a, state, "sda2_base")};
        DcScript post = {"ShsmApplySymbols.java", post_args, 4};
        DcHeadlessOpts o = {.process = jstr_cz(a, state, "program"), .no_analysis = true, .post = &post, .npost = 1};
        const char *install = jstr_cz(a, state, "install");
        size_t argc;
        const char **argv = dc_headless_argv(a, install, p->project, &o, &argc);
        AvpStr log;
        double seconds = dc_run_headless(a, p, argv, argc, "refresh", &log);
        avp_json_set(a, state, AVP_S("context"), avp_json_int(a, DC_CONTEXT_VERSION));
        SAY("applied %d changed symbol rows%s in %.0f s", (i64)delta.n, context_old ? " and the register context" : "",
            seconds);
        if (context_old) {
            /* every listing was made under the old program state */
            ranges.n = 0;
            avp_vec_push(&ranges, ((DcRange){0, (i64)1 << 32}));
            avp_iset_free(funcs);
            funcs = avp_iset_new();
            AvpJson *fs = avp_json_getz(decomp_load_index(a, p), "functions");
            for (size_t i = 0; fs && i < avp_json_len(fs); i++)
                avp_iset_add(funcs, (u64)dc_int16(avp_json_member(fs, i)->key));
        }
    }
    dc_write_rows(a, p->applied_tsv, new_.v, new_.n);
    avp_json_set(a, state, AVP_S("stamp"), avp_json_strz(a, stamp));
    dc_save_json(p->state, state);
    AvpJson *index = decomp_load_index(a, p);
    AvpStrVec redo = dc_affected(a, index, &ranges, funcs);
    AvpSet *redo_set = avp_set_new();
    for (size_t i = 0; i < redo.n; i++) avp_set_add(redo_set, redo.v[i]);
    AvpJson *fs = avp_json_getz(index, "functions");
    for (size_t i = 0; fs && i < avp_json_len(fs); i++) {
        const AvpJsonMember *m = avp_json_member(fs, i);
        if (!avp_set_has(redo_set, m->key)) avp_json_set(a, m->val, AVP_S("stamp"), avp_json_strz(a, stamp));
    }
    dc_save_json(p->index, index);
    if (redo.n) {
        AvpPtrVec all = {0};
        shsm_sym_functions(t, &all);
        AvpIMap *by_addr = avp_imap_new();
        for (size_t i = 0; i < all.n; i++) {
            const ShsmSymbol *s = all.v[i];
            avp_imap_put(by_addr, s->address, NULL)->p = (void *)s;
        }
        AvpPtrVec now = {0};
        AvpStrVec gone = {0};
        for (size_t i = 0; i < redo.n; i++) {
            AvpVal *v = avp_imap_get(by_addr, (u64)dc_int16(redo.v[i]));
            if (v) avp_vec_push(&now, v->p);
            else avp_vec_push(&gone, redo.v[i]);
        }
        index = decomp_load_index(a, p);
        AvpJson *fs2 = avp_json_getz(index, "functions");
        if (gone.n && !fs2) shsm_raise(SHSM_EXC_KEY, "functions");
        for (size_t i = 0; i < gone.n; i++) avp_json_del(fs2, gone.v[i]);
        dc_save_json(p->index, index);
        if (now.n) {
            i64 ok, failed;
            double seconds;
            dc_export(a, p, state, (const ShsmSymbol *const *)now.v, now.n, g->jobs, g->timeout, "refresh_export", &ok,
                      &failed, &seconds);
            SAY("re-exported %d listings (%d failed) in %.0f s", ok + failed, failed, seconds);
        }
        avp_imap_free(by_addr);
        avp_vec_free(&all);
        avp_vec_free(&now);
        avp_vec_free(&gone);
    }
    SAY("refresh done: %d changed rows, %d listings re-exported, symbols.txt %s", (i64)delta.n, (i64)redo.n, stamp);
    avp_set_free(redo_set);
    return SHSM_EXIT_OK;
}

/* ---- status ---- */

static AvpJson *get_or_null(const AvpJson *obj, const char *key) {
    AvpJson *v = obj && avp_json_is_obj(obj) ? avp_json_getz(obj, key) : NULL;
    return v ? v : avp_json_null();
}

/* "%(functions)d" of a JSON number (a float truncates as %d does) */
static AvpStr pyfmt_d(AvpArena *a, const AvpJson *v) {
    if (!v) shsm_raise(SHSM_EXC_KEY, "functions");
    if (v->kind == AVP_JSON_INT && !v->big) return avp_pyfmt(a, "%d", v->u.i);
    if (v->kind == AVP_JSON_FLOAT) return avp_pyfmt(a, "%d", (i64)v->u.f);
    if (v->kind == AVP_JSON_BOOL) return avp_pyfmt(a, "%d", (i64)v->b);
    if (v->kind == AVP_JSON_INT) return v->u.str;
    shsm_raise(SHSM_EXC_TYPE, "%%d format: a real number is required, not %s",
               v->kind == AVP_JSON_STRING ? "str" : v->kind == AVP_JSON_NULL ? "NoneType" : v->kind == AVP_JSON_ARRAY ? "list" : "dict");
}

int dc_cmd_status(const DcArgs *g) {
    AvpArena *a = avp_arena();
    ShsmRepo *repo = find_repo(g);
    DecompPaths *p = decomp_paths(a, repo);
    AvpJson *state = dc_load_json(a, p->state, NULL);
    if (state && avp_json_is_null(state)) state = NULL;
    AvpJson *index = decomp_load_index(a, p);
    const char *stamp = decomp_symbols_stamp(a, repo->symbols_txt);
    AvpJson *funcs = avp_json_getz(index, "functions");
    AvpStrVec failed = {0};
    for (size_t i = 0; funcs && i < avp_json_len(funcs); i++) {
        const AvpJsonMember *m = avp_json_member(funcs, i);
        if (dc_json_ne_str(m->val, "status", "ok")) avp_vec_push(&failed, m->key);
    }
    avp_sort_strs(failed.v, failed.n);
    AvpStrVec stale = dc_stale_entries(a, index, stamp);
    AvpPtrVec all = {0};
    size_t total = shsm_sym_functions(shsm_load_symbols(repo, true), &all);
    avp_vec_free(&all);
    size_t nfuncs = funcs ? avp_json_len(funcs) : 0;
    bool have_state = state && avp_json_truthy(state);
    bool found = false, have_info = false;
    DcInstall info = {0};
    const char *how = NULL, *problem = NULL;
    if (have_state) {
        AvpJson *inst = avp_json_getz(state, "install");
        found = inst && avp_json_truthy(inst) && inst->kind == AVP_JSON_STRING && avp_isdir(avp_cz(a, inst->u.str));
    } else {
        AvpTry tr;
        if (AVP_TRY(&tr)) {
            info = dc_locate_ghidra(a, NULL, avp_getenv(a, "GHIDRA_INSTALL_DIR"), NULL, 0, NULL, &how);
            avp_try_end(&tr);
            found = have_info = true;
        } else if (tr.code == SHSM_EXC_USAGE) {
            problem = avp_strdup(a, tr.msg);
        } else {
            avp_throw(tr.code, "%s", tr.msg);
        }
    }
    ShsmOutput *out = shsm_output_open(g->args, -1, -1, -1);
    if (have_state) {
#define PS(k) avp_cz(a, dc_pystr(a, avp_json_getz(state, k)))
        shsm_output_line(out, avp_pyfmt(a, "ghidra:   %s (%s) at %s", PS("version"), PS("how"), PS("install")));
        shsm_output_line(out, avp_pyfmt(a, "language: %s", PS("language")));
        shsm_output_line(out, avp_pyfmt(a, "project:  %s (%s, input %s, setup %s s, analysis %s s)",
                                        shsm_repo_rel(a, repo, p->project),
                                        decomp_project_exists(p) ? "present" : "MISSING", PS("input"),
                                        PS("setup_seconds"), PS("analysis_seconds")));
        shsm_output_line(out, avp_pyfmt(a, "names:    applied from symbols.txt %s, now %s%s", PS("stamp"), stamp,
                                        dc_json_ne_str(state, "stamp", stamp) ? " (run refresh)" : ""));
#undef PS
    } else if (have_info) {
        shsm_output_line(out, avp_pyfmt(a, "ghidra:   %s (%s) at %s, language %s; no project (run setup)", info.version,
                                        how, info.install, info.language));
    } else {
        shsm_output_line(out, avp_pyfmt(a, "ghidra:   %s", problem));
    }
    AvpJson *last = avp_json_getz(index, "last_export");
    AvpStr tail = AVP_S("");
    if (last && avp_json_truthy(last)) {
        AvpJson *seconds = avp_json_getz(last, "seconds"), *jobs = avp_json_getz(last, "jobs");
        AvpStr fn = pyfmt_d(a, avp_json_getz(last, "functions"));
        if (!seconds) shsm_raise(SHSM_EXC_KEY, "seconds");
        if (!jobs) shsm_raise(SHSM_EXC_KEY, "jobs");
        tail = avp_pyfmt(a, "; last export %S in %S s (-j %S)", &fn, box(a, dc_pystr(a, seconds)),
                         box(a, dc_pystr(a, jobs)));
    }
    shsm_output_line(out, avp_pyfmt(a, "listings: %d of %d functions, %d stale, %d failed%S", (i64)nfuncs, (i64)total,
                                    (i64)stale.n, (i64)failed.n, &tail));
    if (g->failures) {
        for (size_t i = 0; i < failed.n; i++) {
            AvpJson *e = avp_json_get(funcs, failed.v[i]);
            shsm_output_line(out, avp_pyfmt(a, "  failed %S %S: %S", &failed.v[i],
                                            box(a, dc_pystr(a, get_or_null(e, "name"))),
                                            box(a, dc_pystr(a, get_or_null(e, "reason")))));
        }
    }
    AvpJson *src_install, *src_version, *src_language;
    if (have_state) {
        src_install = get_or_null(state, "install");
        src_version = get_or_null(state, "version");
        src_language = get_or_null(state, "language");
    } else if (have_info) {
        src_install = avp_json_strz(a, info.install);
        src_version = avp_json_strz(a, info.version);
        src_language = avp_json_strz(a, info.language);
    } else {
        src_install = src_version = src_language = avp_json_null();
    }
    shsm_output_data(out, "found", avp_json_bool(found));
    shsm_output_data(out, "install", src_install);
    shsm_output_data(out, "version", src_version);
    shsm_output_data(out, "language", src_language);
    shsm_output_data(out, "project", avp_json_bool(decomp_project_exists(p)));
    shsm_output_data(out, "listings", avp_json_int(a, (i64)nfuncs));
    shsm_output_data(out, "functions", avp_json_int(a, (i64)total));
    shsm_output_data(out, "stale", avp_json_int(a, (i64)stale.n));
    shsm_output_data(out, "failed", avp_json_int(a, (i64)failed.n));
    AvpJson *ff = avp_json_arr(a);
    for (size_t i = 0; i < failed.n; i++) avp_json_arr_push(a, ff, avp_json_str(a, failed.v[i]));
    shsm_output_data(out, "failed_functions", ff);
    shsm_output_data(out, "stamp", avp_json_strz(a, stamp));
    shsm_output_data(out, "problem", problem ? avp_json_strz(a, problem) : avp_json_null());
    shsm_output_data(out, "state", state ? state : avp_json_null());
    shsm_output_close(out);
    return stale.n && nfuncs ? SHSM_EXIT_FINDING : SHSM_EXIT_OK;
}
