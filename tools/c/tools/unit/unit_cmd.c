/* unit: the subcommands of tools/unit.py: new (1476-1651, with remove_stale_listings and regen 1654-1692), check
 * (1703-1892), park and flip (1899-2118). */
#include "unit_cmd.h"

#include "shsm_asm.h"
#include "shsm_re.h"

#include <stdio.h>
#include <string.h>

/* ==== new ==== */

static void print_lines(AvpStr text) {
    AvpStrVec l = avp_str_split(text, AVP_S("\n"), -1);
    for (size_t i = 0; i < l.n; i++) unit_out("  %.*s", (int)l.v[i].n, l.v[i].p);
    avp_vec_free(&l);
}

void unit_remove_stale_listings(AvpArena *a, const char *root, AvpStrVec *removed) {
    const char *asm_dir = avp_join(a, root, "build", UNIT_GAME, "asm");
    if (!avp_isdir(asm_dir)) return;
    ShsmListings *L = shsm_asm_listings(a, asm_dir);
    if (!L->record) {
        if (L->note) unit_out("stale asm listings not removed: %s", L->note);
        return;
    }
    for (size_t i = 0; i < L->stale.n; i++) {
        const char *rel = avp_cz(a, L->stale.v[i]);
        if (strchr(rel, '/') || strncmp(rel, "auto_", 5)) {
            unit_out("stale asm listing kept (not an auto_* unit): %s", rel);
            continue;
        }
        if (!avp_remove(avp_join(a, asm_dir, rel))) {
            unit_out("cannot remove stale asm listing %s: %s", rel, avp_cz(a, avp_io_err_str(a)));
            continue;
        }
        if (removed) avp_vec_push(removed, avp_str(rel));
        unit_out("removed stale asm listing: build/%s/asm/%s", UNIT_GAME, rel);
    }
}

int unit_regen(AvpArena *a, const char *root) {
    const char *exe = avp_join(a, root, "tools", "bin", "ninja.exe");
    if (!avp_exists(exe)) {
        unit_out("%s is missing", exe);
        return 2;
    }
    /* subprocess.call: the child writes to the inherited stdout; this process's buffered lines follow at exit, as
     * in the Python (no flush here) */
    const char *argv[] = {exe, "build.ninja", NULL};
    AvpProcResult r = avp_run(&(AvpProcSpec){.argv = argv, .cwd = root, .arena = a});
    if (r.status == AVP_PROC_START_FAILED) avp_throw(AVP_ERR_IO, "%s", r.message ? r.message : "cannot start ninja");
    return (int)r.returncode;
}

static const char *bad_ext_message(AvpArena *a, const char *path) {
    AvpStr base = avp_path_basename(path);
    i64 dot = avp_str_rfind(base, AVP_S("."), 0, (i64)base.n);
    if (dot < 0)
        return avp_sprintf(a, "PATH %s has no file extension: a unit is a .c, .cpp, .cp or .s file (for example "
                              "fmod/fmod_file.cpp)",
                           path);
    return avp_sprintf(a, "PATH %s has the extension \"%.*s\": a unit is a .c, .cpp, .cp or .s file", path,
                       (int)(base.n - (size_t)dot), base.p + dot);
}

static int do_new(UnitProject *p, const UnitNewArgs *args, bool resplit);

int unit_cmd_new(UnitProject *p, const UnitNewArgs *args) { return do_new(p, args, false); }
int unit_cmd_resplit(UnitProject *p, const UnitNewArgs *args) { return do_new(p, args, true); }

static int do_new(UnitProject *p, const UnitNewArgs *args, bool resplit) {
    AvpArena *a = p->a;
    AvpStr ps = avp_str_replace(a, avp_str(args->path), AVP_S("\\"), AVP_S("/"), -1);
    ps = avp_str_removeprefix(ps, AVP_S("src/"));
    const char *path = avp_cz(a, ps);
    if (!resplit && !unit_has_source_ext(path)) unit_fail("%s", bad_ext_message(a, path));
    bool has_text = args->text != NULL;
    i64 lo = 0, hi = 0;
    if (has_text) unit_parse_range(a, args->text, &lo, &hi);
    bool data_auto = !strcmp(args->data, "auto");
    if (!has_text && (!data_auto || !args->include_data.n))
        unit_fail("without --text the unit is data-only: pass --include-data SYMBOL... (the objects it defines) and "
                  "no --data none");
    UnitSplits splits = unit_project_splits(p);
    const char *cfg_eol;
    AvpStr cfg_text = unit_read_text(a, p->configure_path, &cfg_eol);
    UnitConfig *cfg = unit_parse_configure(a, cfg_text);
    size_t own_first = 0, own_last = 0;
    if (resplit) {
        /* the unit's own split is left out of every overlap and claim check, so the objects it holds are free to
         * be taken again by the new ranges */
        const UnitSplit *own = unit_find_split(p, args->path, &splits);
        path = own->path;
        own_first = own->first_line;
        own_last = own->last_line;
        const UnitObj *o = unit_config_find(cfg, path);
        if (!o) unit_fail("%s has no Object entry in configure.py: resplit re-splits an existing unit", path);
        if (!strcmp(o->status, "Matching"))
            unit_fail("refusing: %s is Matching in configure.py; resplit only re-splits a parked (NonMatching) unit: "
                      "`unit.exe park %s` first",
                      path, path);
        if (strcmp(o->status, "NonMatching"))
            unit_fail("refusing: %s is %s in configure.py; resplit only re-splits a NonMatching unit", path, o->status);
        UnitSplits rest = {0};
        for (size_t i = 0; i < splits.n; i++)
            if (&splits.v[i] != own) avp_vec_push(&rest, splits.v[i]);
        splits = rest;
    } else {
        for (size_t i = 0; i < splits.n; i++)
            if (!strcmp(splits.v[i].path, path)) unit_fail("%s already has a split", path);
        if (unit_config_find(cfg, path)) unit_fail("%s is already in configure.py", path);
    }
    AvpStrVec errors = {0};
    if (has_text) {
        UnitFindings f = unit_text_range_findings(p, path, lo, hi, &splits, cfg, NULL);
        for (size_t i = 0; i < f.n; i++) {
            unit_out("  [%s] %s", f.v[i].level, f.v[i].msg);
            if (!strcmp(f.v[i].level, "ERROR")) avp_vec_push(&errors, avp_str(f.v[i].msg));
        }
        for (size_t i = 0; i < splits.n; i++) {
            const UnitRange *t = unit_ranges_get(&splits.v[i].ranges, ".text");
            if (t && unit_overlaps(t->lo, t->hi, lo, hi)) {
                avp_vec_push(&errors, avp_str(avp_sprintf(a, ".text 0x%08llX-0x%08llX overlaps %s (0x%08llX-0x%08llX)",
                                                          lo, hi, splits.v[i].path, t->lo, t->hi)));
                unit_out("  [ERROR] .text overlaps %s", splits.v[i].path);
            }
        }
    }
    if (errors.n) unit_fail("refusing: %s", avp_cz(a, avp_str_join(a, AVP_S("; "), errors.v, errors.n)));
    UnitSymPtrVec funcs = {0};
    if (has_text) funcs = unit_text_funcs(p, lo, hi, ".text");
    UnitRanges ranges = {0};
    if (has_text) unit_ranges_set(&ranges, ".text", lo, hi);
    UnitDataResult *data = AVP_NEW(a, UnitDataResult);
    UnitIndex *idx = NULL;
    /* set(a.include_data) / set(a.exclude_data): each name once, in command-line order */
    AvpStrVec inc = {0}, exc = {0};
    for (size_t i = 0; i < args->include_data.n; i++) {
        bool dup = false;
        for (size_t k = 0; k < inc.n; k++) dup |= avp_str_eq(inc.v[k], args->include_data.v[i]);
        if (!dup) avp_vec_push(&inc, args->include_data.v[i]);
    }
    for (size_t i = 0; i < args->exclude_data.n; i++) {
        bool dup = false;
        for (size_t k = 0; k < exc.n; k++) dup |= avp_str_eq(exc.v[k], args->exclude_data.v[i]);
        if (!dup) avp_vec_push(&exc, args->exclude_data.v[i]);
    }
    if (data_auto) {
        idx = unit_project_index(p);
        data = unit_infer_data(p, idx, has_text, lo, hi, &splits, &inc, &exc);
        if (has_text) {
            i64 clo, chi;
            AvpStrVec cnotes = {0};
            bool cr = unit_find_ctors(p, &funcs, &clo, &chi, &cnotes);
            unit_out(".text 0x%08llX-0x%08llX: %zu functions", lo, hi, funcs.n);
            if (cr) {
                unit_ranges_set(&ranges, ".ctors", clo, chi);
                unit_out(".ctors 0x%08llX-0x%08llX", clo, chi);
            }
            for (size_t i = 0; i < cnotes.n; i++) unit_out_str(cnotes.v[i]);
        } else {
            unit_out("data-only unit (no .text): the --include-data objects and what only they reference");
        }
        for (int si = 0; si < UNIT_N_DATA_SECTIONS; si++) {
            const char *sec = UNIT_DATA_SECTIONS[si];
            const UnitRange *r = unit_ranges_get(&data->ranges, sec);
            if (r) {
                unit_ranges_set(&ranges, sec, r->lo, r->hi);
                unit_out("%s 0x%08llX-0x%08llX", sec, r->lo, r->hi);
                for (size_t i = 0; i < data->notes[si].n; i++) unit_out_str(data->notes[si].v[i]);
            } else if (data->has_notes[si] && data->notes[si].n) {
                unit_out("%s: no range inferred", sec);
                for (size_t i = 0; i < data->notes[si].n; i++) unit_out_str(data->notes[si].v[i]);
            }
        }
        if (data->no_asm && !args->allow_no_asm) {
            unit_out("  [ERROR] no asm blocks for this .text range in build/R5WEA4/asm: no data section can be inferred");
            unit_out("refusing: nothing written. Run the build so dtk writes the asm, or pass --allow-no-asm to write "
                     "the split without data sections (or --data none)");
            return 1;
        }
        for (size_t i = 0; i < data->warnings.n; i++)
            unit_out("  [WARN] %.*s", (int)data->warnings.v[i].n, data->warnings.v[i].p);
        if (!data->ranges.n) unit_out("  no data section is referenced privately by these functions");
        size_t taken = 0;
        for (size_t i = 0; i < data->owned.n; i++) taken += !unit_is_gap_name(data->owned.v[i]->name);
        unit_out("data owners: %zu object(s) in this unit's ranges, %zu left to other units, %zu shared and not taken",
                 taken, data->left.n, data->shared.n);
        for (size_t i = 0; i < data->left.n; i++) unit_out("  other unit: %.*s", (int)data->left.v[i].n, data->left.v[i].p);
        for (size_t i = 0; i < data->shared.n; i++)
            unit_out("  not taken: %.*s (shared; --include-data %.*s to take it)", (int)data->shared.v[i].n,
                     data->shared.v[i].p, (int)data->shared.v[i].n, data->shared.v[i].p);
    } else {
        unit_out(".text 0x%08llX-0x%08llX: %zu functions; --data none: no other section written", lo, hi, funcs.n);
        if (unit_sinit_funcs(p, &funcs).n)
            unit_out("  [WARN] the unit has a static initialiser: without its .ctors entry the unit is not linkable as "
                     "source");
    }
    AvpStrVec conflicts = {0};
    for (size_t r = 0; r < ranges.n; r++)
        for (size_t i = 0; i < splits.n; i++) {
            const UnitRange *o = unit_ranges_get(&splits.v[i].ranges, ranges.v[r].sec);
            if (o && unit_overlaps(o->lo, o->hi, ranges.v[r].lo, ranges.v[r].hi))
                avp_vec_push(&conflicts, avp_str(avp_sprintf(a, "%s 0x%08llX-0x%08llX overlaps %s (0x%08llX-0x%08llX)",
                                                             ranges.v[r].sec, ranges.v[r].lo, ranges.v[r].hi,
                                                             splits.v[i].path, o->lo, o->hi)));
        }
    if (conflicts.n) {
        for (size_t i = 0; i < conflicts.n; i++) unit_out("  [ERROR] %.*s", (int)conflicts.v[i].n, conflicts.v[i].p);
        unit_fail("refusing: split overlap (see above); adjust with --include-data/--exclude-data or --data none");
    }
    UnitSymPtrVec sinit = unit_sinit_funcs(p, &funcs);
    if (idx) {
        AvpSet *inside;
        AvpMap *ext;
        unit_xref(a, idx, &ranges, &inside, &ext);
        if (avp_map_len(ext)) {
            unit_out("force_active: %zu symbols of the unit are referenced from outside; not needed", avp_map_len(ext));
        } else {
            const char *reg = unit_registration_object(idx, &sinit);
            unit_out("force_active: nothing outside references the unit; once it is Matching, list its "
                     "class-registration object in config.yml (%s)",
                     reg ? reg : "not identified");
        }
    }
    size_t lib = resplit ? 0 : unit_infer_lib(a, cfg, path, args->lib);
    const UnitSplit *reader = NULL;
    if (!has_text && !resplit) {
        reader = unit_data_reader(idx, &splits, &args->include_data);
        if (reader) unit_out("read by %s: the new unit is placed after it", reader->path);
        else unit_out("  [WARN] no split's code reads the included objects: placed last");
    }
    i64 start = has_text ? lo : reader ? unit_ranges_get(&reader->ranges, ".text")->lo + 1 : 0xFFFFFFFF;
    AvpStr new_cfg = cfg_text;
    if (!resplit) {
        new_cfg = unit_insert_object(a, cfg, &splits, lib, path, start);
        const char *libname = cfg->libs.v[lib].name;
        unit_out("configure.py: Object(NonMatching, \"%s\") in library %s", path, libname);
        if (strcmp(libname, "game") && strcmp(libname, "climax"))
            unit_out("  note: library %s may need per-object flags or a Reference header comment", libname);
    }
    const char *seol;
    AvpStr stext = unit_read_text(a, p->splits_path, &seol);
    const char *entry = unit_split_entry_text(a, path, &ranges);
    size_t pos = 0;
    bool has_pos = false;
    if (resplit) {
        /* nothing moves: the entry replaces the old one where it stands */
    } else if (!has_text) {
        const char *why;
        has_pos = unit_data_only_position(&splits, &ranges, reader, &pos, &why);
        if (why) unit_out("  [WARN] %s", why);
    } else {
        for (size_t i = 0; i < splits.n; i++) {
            const UnitRange *t = unit_ranges_get(&splits.v[i].ranges, ".text");
            if (t && t->lo > lo) {
                pos = splits.v[i].first_line;
                has_pos = true;
                break;
            }
        }
    }
    AvpStr body;
    if (resplit) {
        AvpStrVec sl = avp_str_split(stext, AVP_S("\n"), -1);
        size_t last = own_last < sl.n ? own_last : sl.n;
        AvpStr head = avp_str_join(a, AVP_S("\n"), sl.v, own_first);
        AvpStr tail = avp_str_join(a, AVP_S("\n"), sl.v + last, sl.n - last);
        AvpStr e = avp_str_rstrip_chars(avp_str(entry), AVP_S("\n"));
        body = avp_str(avp_sprintf(a, "%.*s%s%.*s\n%.*s", (int)head.n, head.p, own_first ? "\n" : "", (int)e.n, e.p,
                                   (int)tail.n, tail.p));
        avp_vec_free(&sl);
        unit_out("splits.txt entry (replaces the unit's current one):");
    } else {
        body = unit_splits_with_entry(a, stext, entry, has_pos, pos);
        unit_out("splits.txt entry:");
    }
    print_lines(avp_str_rstrip_chars(avp_str(entry), AVP_S("\n")));
    AvpSet *vt = avp_set_new(), *fact = avp_set_new(), *names = avp_set_new(), *sinit_names = avp_set_new();
    for (size_t i = 0; i < funcs.n; i++) avp_set_add(names, avp_str(funcs.v[i]->name));
    for (size_t i = 0; i < sinit.n; i++) avp_set_add(sinit_names, avp_str(sinit.v[i]->name));
    if (idx) {
        for (size_t i = 0; i < sinit.n; i++) {
            UnitBlock *b = unit_index_block(idx, sinit.v[i]->name);
            if (b)
                for (size_t k = 0; k < b->nrefs; k++)
                    if (avp_set_has(names, avp_str(b->refs[k]))) avp_set_add(fact, avp_str(b->refs[k]));
        }
        for (size_t i = 0; i < data->owned.n; i++) {
            UnitBlock *b = data->owned.v[i];
            if (strcmp(b->kind, "obj")) continue;
            for (size_t k = 0; k < b->nrefs; k++)
                if (avp_set_has(names, avp_str(b->refs[k]))) avp_set_add(vt, avp_str(b->refs[k]));
        }
    }
    AvpStrVec data_note = {0};
    for (size_t i = 0; i < data->owned.n; i++) {
        UnitBlock *b = data->owned.v[i];
        if (!unit_is_gap_name(b->name))
            avp_vec_push(&data_note, avp_str(avp_sprintf(a, "%s 0x%08llX size 0x%llX %s", b->sec, b->addr, b->size, b->name)));
    }
    if (resplit) {
        if (args->dry_run) {
            unit_out("dry run: nothing written. Would rewrite the splits.txt entry of %s (configure.py and the source "
                     "are not touched)",
                     path);
            return 0;
        }
        unit_write_text(a, p->splits_path, body, seol);
        unit_out("splits.txt: the entry of %s is rewritten. configure.py and src/%s are not touched: bring the source "
                 "(the functions and data it defines) in line with the new ranges by hand.",
                 path, path);
        if (!args->regen) {
            unit_out("Tools that read the unit table see the new split after build.ninja is regenerated: `%s` (or rerun "
                     "with --regen)",
                     UNIT_REGEN_COMMAND);
            return 0;
        }
        int rrc = unit_regen(a, p->root);
        if (rrc == 0) unit_out("%s: ok", UNIT_REGEN_COMMAND);
        else unit_out("%s: FAILED (exit %d)", UNIT_REGEN_COMMAND, rrc);
        if (rrc == 0) unit_remove_stale_listings(a, p->root, NULL);
        return rrc == 0 ? 0 : 1;
    }
    const UnitDecls *decls = unit_is_game_path(path) ? NULL : unit_decls_scan(a, avp_join(a, p->root, "include"));
    const char *hdr_text;
    const char *src_text = unit_build_source(a, path, &funcs, args->confirmed, args->name_evidence, &data_note, fact, vt,
                                             sinit_names, decls, &hdr_text);
    const char *stem = unit_stem(a, path);
    const char *labels[2], *paths[2], *texts[2];
    size_t nplan = 0;
    labels[nplan] = avp_sprintf(a, "src/%s", path);
    paths[nplan] = avp_join(a, p->root, "src", path);
    texts[nplan++] = src_text;
    const char *hdr_path = avp_join(a, p->root, "include", avp_sprintf(a, "%s.h", stem));
    bool hdr_kept = hdr_text && avp_exists(hdr_path); /* an existing header is never replaced: the stub is written alone */
    if (hdr_text && !hdr_kept) {
        labels[nplan] = avp_sprintf(a, "include/%s.h", stem);
        paths[nplan] = hdr_path;
        texts[nplan++] = hdr_text;
    }
    for (size_t i = 0; i < nplan; i++)
        if (avp_exists(paths[i])) unit_fail("%s already exists: never overwritten", labels[i]);
    if (hdr_kept)
        unit_out("include/%s.h already exists: left as it is; only the source stub is %s", stem,
                 args->dry_run ? "to be written" : "written");
    if (args->dry_run) {
        unit_out("dry run: nothing written. Would write splits.txt, configure.py, %s%s%s", labels[0],
                 nplan > 1 ? ", " : "", nplan > 1 ? labels[1] : "");
        return 0;
    }
    unit_write_text(a, p->splits_path, body, seol);
    unit_write_text(a, p->configure_path, new_cfg, cfg_eol);
    for (size_t i = 0; i < nplan; i++) {
        const char *dir = avp_cz(a, avp_path_dirname(paths[i]));
        const char *eol = avp_exists(dir) ? unit_guess_eol(a, dir) : unit_guess_eol(a, avp_join(a, p->root, "src", "game"));
        unit_write_text(a, paths[i], avp_str(texts[i]), eol);
        unit_out("wrote %s", labels[i]);
    }
    unit_out("done. The unit is NonMatching; flesh out the stubs, then `unit.exe check` and `unit.exe flip`.");
    if (!args->regen) {
        unit_out("Tools that read the unit table (match.exe, wall.exe, draft.exe) see the unit after build.ninja is "
                 "regenerated: `%s` (or rerun with --regen)",
                 UNIT_REGEN_COMMAND);
        return 0;
    }
    int rc = unit_regen(a, p->root);
    if (rc == 0) unit_out("%s: ok", UNIT_REGEN_COMMAND);
    else unit_out("%s: FAILED (exit %d)", UNIT_REGEN_COMMAND, rc);
    if (rc == 0) unit_remove_stale_listings(a, p->root, NULL);
    return rc == 0 ? 0 : 1;
}

/* ==== check ==== */

typedef struct {
    AvpArena *a;
    int errs, warns;
    bool overlap_error;
} Check;

static void add(Check *c, const char *level, AVP_PRINTF_FMT const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    AvpBuf b;
    avp_buf_init(&b, c->a);
    avp_buf_vprintf(&b, fmt, ap);
    va_end(ap);
    const char *msg = avp_buf_cz(&b);
    if (!strcmp(level, "ERROR")) {
        c->errs++;
        if (strstr(msg, "overlaps")) c->overlap_error = true;
    }
    if (!strcmp(level, "WARN")) c->warns++;
    unit_out("[%s] %s", level, msg);
}

typedef struct {
    i64 addr;
    size_t line;
    bool trail;
} Comment;

static int cmp_comment(const void *x, const void *y, void *ctx) {
    (void)ctx;
    i64 l = ((const Comment *)x)->addr, r = ((const Comment *)y)->addr;
    return l < r ? -1 : l > r;
}

static bool blank_or_comment(AvpStr l) {
    return !avp_str_strip(l).n || avp_str_startswith(avp_str_lstrip(l), AVP_S("//"));
}

/* An assembly source unit (`.s`): the address comment is the assembler's `# <name>   // 0x8XXXXXXX` line, the last
 * line of the comment above the function's `.fn`; the `.fn` that follows must name the symbols.txt function. */
static void check_asm_source(Check *c, AvpStr stext, const UnitSymPtrVec *funcs) {
    AvpArena *a = c->a;
    AvpStrVec lines = avp_str_split(stext, AVP_S("\n"), -1);
    ShsmRe *cmt_re = shsm_re("^\\s*#.*//\\s*0x(8[0-9A-Fa-f]{7})\\s*$", 0);
    ShsmRe *fn_re = shsm_re("^\\s*\\.fn\\s+(\"[^\"]+\"|[^\\s,]+)\\s*,", 0);
    ShsmReMatch m;
    size_t ncomments = 0;
    int bad = 0;
    AvpI64Vec seen = {0};
    for (size_t n = 0; n < lines.n; n++) {
        if (!shsm_re_match(cmt_re, lines.v[n], 0, AVP_END, &m)) continue;
        u64 v = 0;
        avp_parse_uint(shsm_re_group(&m, 1), 16, &v);
        ncomments++;
        avp_vec_push(&seen, (i64)v);
        const UnitSym *f = NULL;
        for (size_t k = 0; k < funcs->n; k++)
            if (funcs->v[k]->addr == (i64)v) f = funcs->v[k];
        if (!f) {
            add(c, "ERROR", "line %zu: address comment 0x%08llX is not a function of this unit in symbols.txt", n + 1,
                (i64)v);
            bad++;
            continue;
        }
        for (size_t k = n + 1; k < lines.n; k++) {
            if (!shsm_re_match(fn_re, lines.v[k], 0, AVP_END, &m)) continue;
            AvpStr nm = avp_str_strip_chars(shsm_re_group(&m, 1), AVP_S("\""));
            if (!avp_str_eqz(nm, f->name)) {
                add(c, "WARN", "line %zu: comment 0x%08llX is followed by `.fn %.*s` but symbols.txt names %s", n + 1,
                    (i64)v, (int)nm.n, nm.p, f->name);
                bad++;
            }
            break;
        }
    }
    if (!ncomments && funcs->n) add(c, "WARN", "the source has no `# ... // 0x8XXXXXXX` address comments");
    AvpStrVec missing = {0};
    for (size_t i = 0; i < funcs->n; i++) {
        bool com = false;
        for (size_t k = 0; k < seen.n; k++) com |= seen.v[k] == funcs->v[i]->addr;
        if (!com) avp_vec_push(&missing, avp_str(funcs->v[i]->name));
    }
    if (ncomments && missing.n)
        add(c, "INFO", "%zu functions have no address comment in the source: %s%s", missing.n,
            avp_cz(a, avp_str_join(a, AVP_S(", "), missing.v, missing.n < 5 ? missing.n : 5)), missing.n > 5 ? " ..." : "");
    if (ncomments && !bad) add(c, "OK", "%zu address comments match symbols.txt", ncomments);
    avp_vec_free(&missing);
    avp_vec_free(&seen);
    avp_vec_free(&lines);
}

static void check_source(Check *c, UnitProject *p, AvpStr stext, const UnitSymPtrVec *funcs, bool game) {
    AvpArena *a = c->a;
    AvpStrVec lines = avp_str_split(stext, AVP_S("\n"), -1);
    ShsmRe *line_re = shsm_re("^\\s*//\\s*0x(8[0-9A-Fa-f]{7})\\s*$", 0);
    ShsmRe *trail_re = shsm_re("\\}\\s*//\\s*0x(8[0-9A-Fa-f]{7})\\s*$", 0);
    AVP_VEC(Comment) commented = {0};
    ShsmReMatch m;
    for (size_t n = 0; n < lines.n; n++) {
        bool trail = false;
        bool ok = shsm_re_match(line_re, lines.v[n], 0, AVP_END, &m);
        if (!ok) {
            ok = shsm_re_search(trail_re, lines.v[n], 0, AVP_END, &m);
            trail = true;
        }
        if (!ok) continue;
        u64 v = 0;
        avp_parse_uint(shsm_re_group(&m, 1), 16, &v);
        bool found = false;
        for (size_t k = 0; k < commented.n; k++) /* dict: a later comment of the same address replaces the entry */
            if (commented.v[k].addr == (i64)v) {
                commented.v[k].line = n;
                commented.v[k].trail = trail;
                found = true;
            }
        if (!found) {
            Comment cm = {(i64)v, n, trail};
            avp_vec_push(&commented, cm);
        }
    }
    if (!commented.n && funcs->n) add(c, "WARN", "the source has no `// 0x8XXXXXXX` address comments");
    avp_sort(commented.v, commented.n, sizeof(Comment), cmp_comment, NULL, false);
    int bad = 0;
    ShsmRe *macro_re = shsm_re("\\b(?:METHODDEF|GLOBAL|LOCAL|EXTERN)\\s*\\([^)]*\\)", 0);
    ShsmRe *call_re = shsm_re("([~\\w:]+)\\s*\\(", 0);
    for (size_t ci = 0; ci < commented.n; ci++) {
        i64 addr = commented.v[ci].addr;
        size_t n = commented.v[ci].line;
        const UnitSym *f = NULL;
        for (size_t k = 0; k < funcs->n; k++) /* {f.addr: f}: the last one */
            if (funcs->v[k]->addr == addr) f = funcs->v[k];
        if (!f) {
            add(c, "ERROR", "line %zu: address comment 0x%08llX is not a function of this unit in symbols.txt", n + 1,
                addr);
            bad++;
            continue;
        }
        UnitDemangled d = unit_demangle(a, f->name);
        if (!commented.v[ci].trail) {
            AvpStrVec code = {0};
            for (size_t k = n + 1; k < lines.n && k < n + 40; k++)
                if (!blank_or_comment(lines.v[k])) avp_vec_push(&code, avp_str_strip(lines.v[k]));
            AvpStr nxt = avp_str_join(a, AVP_S(" "), code.v, code.n < 4 ? code.n : 4);
            avp_vec_free(&code);
            nxt = shsm_re_sub(a, macro_re, AVP_S(""), nxt, 0);
            if (shsm_re_search(call_re, nxt, 0, AVP_END, &m)) {
                AvpStr g = shsm_re_group(&m, 1);
                AvpStrVec got = avp_str_split(g, AVP_S("::"), -1);
                AvpStr got_name = got.v[got.n - 1];
                AvpStr got_cls = got.n > 1 ? got.v[got.n - 2] : AVP_S("");
                const char *want = d.mangled ? unit_dm_fname(a, &d) : d.base;
                if (!avp_str_eqz(got_name, want) ||
                    (*d.cls && got_cls.n && !avp_str_eqz(got_cls, unit_dm_short_cls(a, &d)))) {
                    add(c, "WARN", "line %zu: comment 0x%08llX is followed by `%.*s` but symbols.txt names %s (%s)", n + 1,
                        addr, (int)g.n, g.p, d.mangled ? unit_dm_qualified(a, &d) : d.base, f->name);
                    bad++;
                }
                avp_vec_free(&got);
            }
            if (game) {
                i64 j = (i64)n - 1;
                while (j >= 0 && blank_or_comment(lines.v[j])) j--;
                bool ok = false;
                if (j >= 2 && avp_str_eqz(avp_str_strip(lines.v[j]), "*/")) {
                    i64 s = j - 12 > 0 ? j - 12 : 0;
                    AvpStr win = avp_str_join(a, AVP_S("\n"), lines.v + s, (size_t)(j - s));
                    ok = avp_str_contains(win, AVP_S("===================="));
                }
                if (!ok) {
                    add(c, "WARN", "line %zu: no banner comment above the function at 0x%08llX", n + 1, addr);
                    bad++;
                }
            }
        }
    }
    UnitSymPtrVec sinit = unit_sinit_funcs(p, funcs);
    AvpStrVec missing = {0};
    for (size_t i = 0; i < funcs->n; i++) {
        const UnitSym *f = funcs->v[i];
        bool com = false, si = false;
        for (size_t k = 0; k < commented.n; k++) com |= commented.v[k].addr == f->addr;
        for (size_t k = 0; k < sinit.n; k++) si |= sinit.v[k]->addr == f->addr;
        if (com || si) continue;
        UnitDemangled d = unit_demangle(a, f->name);
        if (!unit_dm_generated(&d)) avp_vec_push(&missing, avp_str(f->name));
    }
    if (missing.n)
        add(c, "INFO", "%zu functions have no address comment in the source (inline in a header, or not yet written): %s%s",
            missing.n, avp_cz(a, avp_str_join(a, AVP_S(", "), missing.v, missing.n < 5 ? missing.n : 5)),
            missing.n > 5 ? " ..." : "");
    if (commented.n && !bad)
        add(c, "OK", "%zu address comments match symbols.txt%s", commented.n, game ? " and have banners" : "");
    avp_vec_free(&missing);
    avp_vec_free(&lines);
}

int unit_cmd_check(UnitProject *p, const char *arg) {
    AvpArena *a = p->a;
    UnitSplits splits = unit_project_splits(p);
    const UnitSplit *split = unit_find_split(p, arg, &splits);
    const char *path = split->path;
    UnitConfig *cfg = unit_project_config(p);
    Check c = {a, 0, 0, false};
    unit_out("unit %s", path);
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, "");
    for (size_t i = 0; i < split->ranges.n; i++)
        avp_buf_printf(&b, "%s%s 0x%08llX-0x%08llX", i ? ", " : "", split->ranges.v[i].sec, split->ranges.v[i].lo,
                       split->ranges.v[i].hi);
    add(&c, "OK", "split present: %s", avp_buf_cz(&b));
    const UnitObj *obj = unit_config_find(cfg, path);
    if (obj) add(&c, "INFO", "configure.py status: %s (library %s)", obj->status, cfg->libs.v[obj->lib].name);
    else add(&c, "WARN", "no Object entry in configure.py: the unit is linked from dtk's asm");
    for (size_t r = 0; r < split->ranges.n; r++) {
        const UnitRange *rg = &split->ranges.v[r];
        for (size_t i = 0; i < splits.n; i++) {
            if (!strcmp(splits.v[i].path, path)) continue;
            const UnitRange *o = unit_ranges_get(&splits.v[i].ranges, rg->sec);
            if (o && unit_overlaps(o->lo, o->hi, rg->lo, rg->hi))
                add(&c, "ERROR", "%s overlaps %s (0x%08llX-0x%08llX)", rg->sec, splits.v[i].path, o->lo, o->hi);
        }
        if (rg->lo >= rg->hi) add(&c, "ERROR", "%s range is empty or reversed", rg->sec);
    }
    if (!c.overlap_error) add(&c, "OK", "no overlap with another split");
    const UnitRange *text = unit_ranges_get(&split->ranges, ".text");
    UnitRanges code = {0};
    const UnitRange *init = unit_ranges_get(&split->ranges, ".init");
    if (init) unit_ranges_set(&code, ".init", init->lo, init->hi);
    if (text) unit_ranges_set(&code, ".text", text->lo, text->hi);
    if (!code.n) add(&c, "INFO", "data-only unit: the split has no .text or .init range");
    if (text) {
        UnitFindings f = unit_text_range_findings(p, path, text->lo, text->hi, &splits, cfg, path);
        for (size_t i = 0; i < f.n; i++) add(&c, f.v[i].level, "%s", f.v[i].msg);
    }
    UnitSymPtrVec funcs = {0};
    for (size_t r = 0; r < code.n; r++) {
        UnitSymPtrVec fs = unit_text_funcs(p, code.v[r].lo, code.v[r].hi, code.v[r].sec);
        avp_vec_extend(&funcs, fs.v, fs.n);
        add(&c, "INFO", "%zu functions in symbols.txt for %s 0x%08llX-0x%08llX", fs.n, code.v[r].sec, code.v[r].lo,
            code.v[r].hi);
    }
    UnitIndex *idx = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        idx = unit_project_index(p);
        avp_try_end(&t);
    } else if (t.code == UNIT_TOOL_ERROR) {
        add(&c, "WARN", "%s", unit_error_message());
    } else {
        avp_throw(t.code, "%s", t.msg);
    }
    if (idx && idx->note) add(&c, "INFO", "%s", idx->note);
    if (idx && code.n) {
        UnitBlockVec asm_fns = {0};
        /* an assembly source unit (user's ruling 2026-11-14) assembles the bytes dtk covers with a generated pad_
         * function: that function has no symbols.txt entry and needs none */
        bool asm_unit = unit_is_asm_source(path);
        AvpStrVec pads = {0};
        for (size_t r = 0; r < code.n; r++) {
            UnitBlockVec in = unit_index_in_range(a, idx, code.v[r].sec, code.v[r].lo, code.v[r].hi);
            for (size_t i = 0; i < in.n; i++) {
                if (strcmp(in.v[i]->kind, "fn") || unit_is_gap_name(in.v[i]->name)) continue;
                if (asm_unit && unit_is_pad_name(in.v[i]->name)) avp_vec_push(&pads, avp_str(in.v[i]->name));
                else avp_vec_push(&asm_fns, in.v[i]);
            }
            avp_vec_free(&in);
        }
        if (pads.n)
            add(&c, "OK", "assembly source unit: dtk's generated %s cover%s bytes the source assembles",
                avp_cz(a, avp_str_join(a, AVP_S(", "), pads.v, pads.n)), pads.n == 1 ? "s" : "");
        size_t npads = pads.n;
        avp_vec_free(&pads);
        AvpSet *known = avp_set_new(), *asm_names = avp_set_new();
        for (size_t i = 0; i < funcs.n; i++) avp_set_add(known, avp_str(funcs.v[i]->name));
        for (size_t i = 0; i < asm_fns.n; i++) avp_set_add(asm_names, avp_str(asm_fns.v[i]->name));
        AvpStrVec miss = {0};
        for (size_t i = 0; i < asm_fns.n; i++)
            if (!avp_set_has(known, avp_str(asm_fns.v[i]->name))) avp_vec_push(&miss, avp_str(asm_fns.v[i]->name));
        if (miss.n)
            add(&c, "ERROR", "%zu functions in the retail asm but not in symbols.txt: %s", miss.n,
                avp_cz(a, avp_str_join(a, AVP_S(", "), miss.v, miss.n < 6 ? miss.n : 6)));
        else if (asm_fns.n)
            add(&c, "OK", "every function in the asm range (%zu) is in symbols.txt", asm_fns.n);
        else if (!npads)
            add(&c, "WARN", "no asm blocks for the range (stale or missing build/R5WEA4/asm)");
        AvpStrVec extra = {0};
        AvpStrVec ks = avp_set_sorted(known);
        for (size_t i = 0; i < ks.n; i++)
            if (!avp_set_has(asm_names, ks.v[i])) avp_vec_push(&extra, ks.v[i]);
        if (asm_fns.n && extra.n)
            add(&c, "WARN", "%zu symbols.txt functions have no asm block (stale asm?): %s", extra.n,
                avp_cz(a, avp_str_join(a, AVP_S(", "), extra.v, extra.n < 4 ? extra.n : 4)));
        AvpSet *inside;
        AvpMap *ext;
        unit_xref(a, idx, &split->ranges, &inside, &ext);
        AvpStrVec fa = unit_load_force_active(a, p->config_yml);
        AvpStrVec mine = {0};
        for (size_t i = 0; i < fa.n; i++)
            if (avp_set_has(inside, fa.v[i])) avp_vec_push(&mine, fa.v[i]);
        if (avp_map_len(ext)) {
            AvpMapIter it = {0};
            AvpStr kname = {0};
            AvpStrVec *kv = NULL;
            while (avp_map_next(ext, &it))
                if (!kv || ((AvpStrVec *)it.val->p)->n > kv->n) {
                    kname = it.key;
                    kv = it.val->p;
                }
            add(&c, "OK",
                "%zu symbols of the unit are referenced from outside (e.g. %.*s by %.*s); the linker keeps the unit "
                "without force_active",
                avp_map_len(ext), (int)kname.n, kname.p, (int)kv->v[0].n, kv->v[0].p);
            if (mine.n)
                add(&c, "INFO",
                    "config.yml force_active lists %s for this unit (redundant unless every outside reference is "
                    "itself stripped)",
                    avp_cz(a, avp_str_join(a, AVP_S(", "), mine.v, mine.n)));
        } else {
            UnitSymPtrVec sinit = unit_sinit_funcs(p, &funcs);
            const char *reg = unit_registration_object(idx, &sinit);
            if (mine.n) {
                add(&c, "OK",
                    "nothing outside references the unit; its class-registration object is already listed under "
                    "force_active: %s",
                    avp_cz(a, avp_str_join(a, AVP_S(", "), mine.v, mine.n)));
            } else {
                const char *hint = reg ? avp_sprintf(a, "the registration object looks like %s", reg)
                                       : "find the global its __sinit passes to unk_801A8F6C's constructor";
                add(&c, "WARN",
                    "nothing outside the unit references any of its symbols, so MWLD strips the object when it is "
                    "linked as source: list its class-registration object under force_active in config.yml (%s); not "
                    "listed",
                    hint);
            }
        }
    }
    bool game = unit_is_game_path(path);
    const char *src_path = avp_join(a, p->root, "src", path);
    if (!avp_exists(src_path)) {
        add(&c, "WARN", "src/%s does not exist", path);
    } else {
        AvpStr stext = unit_read_text(a, src_path, NULL);
        if (unit_is_asm_source(path)) check_asm_source(&c, stext, &funcs);
        else check_source(&c, p, stext, &funcs, game);
        AvpStr ps = avp_str(path);
        if (game && (avp_str_endswith(ps, AVP_S(".cpp")) || avp_str_endswith(ps, AVP_S(".cp")))) {
            const char *stem = unit_stem(a, path);
            bool hx = avp_exists(avp_join(a, p->root, "include", avp_sprintf(a, "%s.h", stem)));
            add(&c, hx ? "OK" : "WARN", "header include/%s.h %s", stem, hx ? "exists" : "does not exist");
        }
    }
    unit_out("summary: %d errors, %d warnings", c.errs, c.warns);
    return c.errs ? 1 : 0;
}

/* ==== park / flip ==== */

static AvpStr set_status(AvpArena *a, AvpStr cfg_text, const char *path, const char *new_status, const char **old) {
    UnitConfig *cfg = unit_parse_configure(a, cfg_text);
    const UnitObj *o = unit_config_find(cfg, path);
    if (!o) unit_fail("%s has no Object entry in configure.py", path);
    *old = o->status;
    return avp_str(avp_sprintf(a, "%.*s%s%.*s", (int)o->status_start, cfg_text.p, new_status,
                               (int)(cfg_text.n - o->status_end), cfg_text.p + o->status_end));
}

int unit_cmd_park(UnitProject *p, const char *arg) {
    AvpArena *a = p->a;
    UnitSplits splits = unit_project_splits(p);
    const char *path = unit_find_split(p, arg, &splits)->path;
    const char *eol, *old;
    AvpStr text = unit_read_text(a, p->configure_path, &eol);
    AvpStr nt = set_status(a, text, path, "NonMatching", &old);
    if (!strcmp(old, "NonMatching")) {
        unit_out("%s is already NonMatching", path);
        return 0;
    }
    unit_write_text(a, p->configure_path, nt, eol);
    unit_out("configure.py: %s: %s -> NonMatching", path, old);
    return 0;
}

static AvpStrVec git_dirty(AvpArena *a, const char *root, bool untracked) {
    const char *argv[] = {"git", "status", "--porcelain", "-uall", NULL};
    AvpProcResult r = avp_run(&(AvpProcSpec){.argv = argv, .cwd = root, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE,
                                             .text = true, .errors = AVP_ERRORS_REPLACE, .arena = a});
    if (r.status == AVP_PROC_START_FAILED) avp_throw(AVP_ERR_IO, "%s", r.message ? r.message : "cannot start git");
    if (r.returncode != 0) unit_fail("git status failed: %s", avp_cz(a, avp_str_strip(r.err)));
    AvpStrVec res = {0};
    AvpStrVec lines = avp_str_split(r.out, AVP_S("\n"), -1);
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr line = lines.v[i];
        if (line.n <= 3) continue;
        if (avp_str_startswith(line, AVP_S("??")) && !untracked) continue;
        AvpStr ps = avp_str_strip_chars(avp_str_strip(avp_str_slice(line, 3, (i64)line.n)), AVP_S("\""));
        i64 arrow;
        while ((arrow = avp_str_find(ps, AVP_S(" -> "), 0, (i64)ps.n)) >= 0) ps = avp_str_slice(ps, arrow + 4, (i64)ps.n);
        avp_vec_push(&res, avp_str_replace(a, ps, AVP_S("\\"), AVP_S("/"), -1));
    }
    avp_vec_free(&lines);
    return res;
}

/* run_logged: `$ cmd`, then the exit line; the output (stdout then stderr) in *text. */
static int run_logged(AvpArena *a, const char *const *argv, const char *shell_cmd, const char *root, AvpStr *text) {
    if (shell_cmd) unit_out("$ %s", shell_cmd);
    else {
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_putz(&b, "");
        for (size_t i = 0; argv[i]; i++) avp_buf_printf(&b, "%s%s", i ? " " : "", argv[i]);
        unit_out("$ %s", avp_buf_cz(&b));
    }
    double t0 = avp_time();
    AvpProcSpec spec = {.cwd = root, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE, .text = true,
                        .errors = AVP_ERRORS_REPLACE, .arena = a};
    if (shell_cmd) {
        spec.cmdline = shell_cmd;
        spec.shell = true;
    } else {
        spec.argv = argv;
    }
    AvpProcResult r = avp_run(&spec);
    if (r.status == AVP_PROC_START_FAILED) avp_throw(AVP_ERR_IO, "%s", r.message ? r.message : "cannot start");
    *text = avp_str_concat(a, r.out, r.err);
    AvpStrVec l = avp_str_splitlines(*text, false);
    unit_out("  exit %lld in %.0fs, %zu lines of output", r.returncode, avp_time() - t0, l.n);
    avp_vec_free(&l);
    return (int)r.returncode;
}

static AvpStr tail(AvpArena *a, AvpStr text, size_t n) {
    AvpStrVec l = avp_str_splitlines(text, false);
    size_t s = l.n > n ? l.n - n : 0;
    AvpStr res = avp_str_join(a, AVP_S("\n"), l.v + s, l.n - s);
    avp_vec_free(&l);
    return res;
}

static AvpStrVec gate_lines(AvpStr text) {
    AvpStrVec l = avp_str_splitlines(text, false), res = {0};
    for (size_t i = 0; i < l.n; i++) {
        AvpStr s = avp_str_strip(l.v[i]);
        if (avp_str_startswith(s, AVP_S("GATE "))) avp_vec_push(&res, s);
    }
    avp_vec_free(&l);
    return res;
}

const char *unit_gate_pending_ok(AvpArena *a, int rc, AvpStr text, const char *path) {
    AvpStrVec l = gate_lines(text);
    AvpStr last = l.n ? l.v[l.n - 1] : AVP_S("");
    avp_vec_free(&l);
    const char *pat = avp_sprintf(a, "^GATE OK-PENDING [0-9a-f]{40} units=\\d+ functions=\\d+ pending-flip=%s \\(",
                                  avp_cz(a, shsm_re_escape(a, avp_str(path))));
    ShsmReMatch m;
    if (rc == UNIT_GATE_PENDING_EXIT && shsm_re_match(shsm_re_try(pat, 0), last, 0, AVP_END, &m)) return avp_cz(a, last);
    return NULL;
}

static const char *last_gate_line(AvpArena *a, AvpStr text) {
    AvpStrVec l = gate_lines(text);
    const char *res = l.n ? avp_cz(a, l.v[l.n - 1]) : "(gate.exe printed no GATE line)";
    avp_vec_free(&l);
    return res;
}

static AvpStr gate_excerpt(AvpArena *a, AvpStr text, size_t n) {
    AvpStrVec l = avp_str_splitlines(text, false);
    AvpStr res;
    if (l.n <= n + 1) {
        res = avp_str_join(a, AVP_S("\n"), l.v, l.n);
    } else {
        AvpStr head = avp_str_join(a, AVP_S("\n"), l.v, n);
        res = avp_str(avp_sprintf(a, "%.*s\n... %zu more line(s) in build/gate/last.json ...\n%.*s", (int)head.n, head.p,
                                  l.n - n - 1, (int)l.v[l.n - 1].n, l.v[l.n - 1].p));
    }
    avp_vec_free(&l);
    return res;
}

const char *unit_tool_exe(AvpArena *a, const char *name) {
    if (!strcmp(name, "gate.exe")) {
        const char *env = avp_getenv_or(a, UNIT_GATE_ENV, NULL);
        if (env && *env) return env;
    }
    return avp_path_normpath(a, avp_path_join(a, avp_exe_dir(), name));
}

typedef struct {
    UnitProject *p;
    const char *path, *old;
    AvpBytes original;
    const UnitSplit *split;
    const char *gate_line; /* gate.exe's last GATE line: printed last, whatever else flip says */
} Flip;

static const char *size_hint(Flip *f);

static int revert(Flip *f, const char *why) {
    if (!avp_write_file(f->p->configure_path, f->original.v, f->original.n)) shsm_raise_io();
    unit_out("FAILED: %s", why);
    unit_out("configure.py restored: %s is %s again", f->path, f->old);
    const char *hint = f->gate_line ? size_hint(f) : NULL;
    if (hint) unit_out("hint: %s", hint);
    if (f->gate_line) unit_out("%s", f->gate_line);
    return 1;
}

/* The built DOL's size differs from retail's and nothing outside the unit references its symbols: MWLD drops the
 * unit's objects when it is linked as source (its static initialiser's .ctors entry and its class-registration
 * object are what keep it), so a `force_active` entry in config.yml is the usual cure. NULL when not the case. */
static const char *size_hint(Flip *f) {
    UnitProject *p = f->p;
    AvpArena *a = p->a;
    i64 built = 0, retail = 0;
    const char *dol = avp_join(a, p->root, "build", "gate", "tree", "build", UNIT_GAME, "main.dol");
    if (!avp_getsize(dol, &built) || !avp_getsize(p->dol_path, &retail) || built == retail) return NULL;
    UnitIndex *idx = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        idx = unit_project_index(p);
        avp_try_end(&t);
    } else if (t.code != UNIT_TOOL_ERROR) {
        return NULL;
    }
    if (!idx || !f->split) return NULL;
    AvpSet *inside;
    AvpMap *ext;
    unit_xref(a, idx, &f->split->ranges, &inside, &ext);
    if (avp_map_len(ext)) return NULL;
    const UnitRange *text = unit_ranges_get(&f->split->ranges, ".text");
    UnitSymPtrVec funcs = text ? unit_text_funcs(p, text->lo, text->hi, ".text") : (UnitSymPtrVec){0};
    UnitSymPtrVec sinit = unit_sinit_funcs(p, &funcs);
    const char *reg = unit_registration_object(idx, &sinit);
    const UnitRange *ctors = unit_ranges_get(&f->split->ranges, ".ctors");
    if (!reg && !ctors) return NULL;
    AvpStrVec fa = unit_load_force_active(a, p->config_yml);
    bool listed = false;
    for (size_t i = 0; reg && i < fa.n; i++) listed |= avp_str_eqz(fa.v[i], reg);
    return avp_sprintf(a,
                       "the gate's DOL is %lld bytes (retail %lld) and nothing outside %s references its symbols, so the "
                       "linker strips the unit's objects. Its %s %s: list %s under force_active in "
                       "config/%s/config.yml (the unit's class-registration object)%s",
                       built, retail, f->path, ctors ? ".ctors entry" : "class-registration object",
                       reg ? avp_sprintf(a, "is %s", reg) : "is not identified; find the global its __sinit passes to the "
                                                            "registration constructor",
                       reg ? reg : "it", UNIT_GAME, listed ? " (it is already listed: check the entry's spelling)" : "");
}

/* `gatecheck layout <unit>` on the built object when there is one: data-order problems named before the gate runs. */
static void layout_check(UnitProject *p, const char *stem) {
    AvpArena *a = p->a;
    const char *obj = avp_join(a, p->root, "build", UNIT_GAME, "src", avp_sprintf(a, "%s.o", stem));
    if (!avp_isfile(obj)) {
        unit_out("layout check skipped: no built object build/%s/src/%s.o (build the unit first)", UNIT_GAME, stem);
        return;
    }
    const char *exe = unit_tool_exe(a, "gatecheck.exe");
    if (!avp_isfile(exe)) {
        unit_out("layout check skipped: %s not found", exe);
        return;
    }
    const char *argv[] = {exe, "--root", p->root, "layout", avp_sprintf(a, "main/%s", stem), NULL};
    AvpProcResult r = avp_run(&(AvpProcSpec){.argv = argv, .cwd = p->root, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE,
                                             .text = true, .errors = AVP_ERRORS_REPLACE, .arena = a});
    if (r.status == AVP_PROC_START_FAILED) {
        unit_out("layout check skipped: cannot start gatecheck.exe");
        return;
    }
    AvpStr all = avp_str_concat(a, r.out, r.err);
    AvpStrVec l = avp_str_splitlines(all, false);
    unit_out("layout check (gatecheck layout main/%s, exit %lld):", stem, r.returncode);
    size_t n = l.n < 12 ? l.n : 12;
    for (size_t i = 0; i < n; i++) unit_out("  %.*s", (int)l.v[i].n, l.v[i].p);
    if (l.n > n) unit_out("  ... %zu more line(s): build/tools/gatecheck.exe layout main/%s -v", l.n - n, stem);
    if (r.returncode != 0)
        unit_out("  layout differs from retail: a data-order or size problem the gate will also report; fix it before "
                 "flipping");
    avp_vec_free(&l);
}

int unit_cmd_flip(UnitProject *p, const UnitFlipArgs *args) {
    AvpArena *a = p->a;
    UnitSplits splits = unit_project_splits(p);
    const UnitSplit *split = unit_find_split(p, args->path, &splits);
    const char *path = split->path;
    const char *stem = unit_stem(a, path);
    const char *eol;
    AvpStr cfg_text = unit_read_text(a, p->configure_path, &eol);
    UnitConfig *cfg = unit_parse_configure(a, cfg_text);
    const UnitObj *obj = unit_config_find(cfg, path);
    if (!obj) unit_fail("%s has no Object entry in configure.py", path);
    if (!strcmp(obj->status, "Matching")) unit_fail("%s is already Matching", path);
    if (strcmp(obj->status, "NonMatching"))
        unit_fail("%s is %s in configure.py: flip only promotes a NonMatching unit (set it to NonMatching by hand first "
                  "if it should become Matching)",
                  path, obj->status);
    bool custom = args->gate_cmd != NULL;
    if (!custom && ((args->rules_cmd && *args->rules_cmd) || args->ignore_untracked))
        unit_fail("--rules-cmd and --ignore-untracked apply only with --gate-cmd (gate.exe runs the rule checker and "
                  "refuses untracked files)");
    AvpStrVec dirty = git_dirty(a, p->root, !(custom && args->ignore_untracked));
    AvpStrVec problems = {0};
    if (dirty.n)
        avp_vec_push(&problems, avp_str(avp_sprintf(a, "the git tree is not clean (commit first): %s%s",
                                                    avp_cz(a, avp_str_join(a, AVP_S(", "), dirty.v, dirty.n < 8 ? dirty.n : 8)),
                                                    dirty.n > 8 ? " ..." : "")));
    AvpBuf buf;
    avp_buf_init(&buf, a);
    avp_buf_putz(&buf, "");
    unit_capture(&buf);
    int rc = unit_cmd_check(p, path);
    unit_capture(NULL);
    if (rc != 0) {
        AvpStrVec l = avp_str_splitlines(avp_buf_str(&buf), false);
        AvpBuf pb;
        avp_buf_init(&pb, a);
        avp_buf_putz(&pb, "`check` reports errors:\n");
        bool first = true;
        for (size_t i = 0; i < l.n; i++)
            if (avp_str_contains(l.v[i], AVP_S("[ERROR]"))) {
                avp_buf_printf(&pb, "%s    %.*s", first ? "" : "\n", (int)l.v[i].n, l.v[i].p);
                first = false;
            }
        avp_vec_push(&problems, avp_buf_str(&pb));
        avp_vec_free(&l);
    }
    /* the C gate and rule checker beside unit.exe */
    const char *gate_exe = unit_tool_exe(a, "gate.exe");
    const char *rules_exe = unit_tool_exe(a, "check_rules.exe");
    const char *gate_argv[] = {gate_exe, "--root", p->root, "--pending-flip", path, NULL};
    const char *gate_desc = custom ? args->gate_cmd
                                   : avp_sprintf(a, "%s --root %s --pending-flip %s", gate_exe, p->root, path);
    bool default_rules = !(args->rules_cmd && *args->rules_cmd);
    const char *rules_desc = default_rules ? avp_sprintf(a, "\"%s\" --units main/%s", rules_exe, stem) : args->rules_cmd;
    if (!custom && !avp_isfile(gate_exe))
        avp_vec_push(&problems, avp_str(avp_sprintf(a, "%s not found (build the tools with tools/build_c.cmd)", gate_exe)));
    if (args->dry_run) {
        unit_out("dry run for %s (Matching in configure.py: %s -> Matching)", path, obj->status);
        unit_out("would run, in order, from %s:", p->root);
        unit_out("  $ %s", gate_desc);
        if (custom) {
            unit_out("  require exit 0 and a line `build/%s/main.dol: OK`", UNIT_GAME);
            unit_out("  read build/%s/report.json: main/%s and every Matching unit at matched_code_percent 100", UNIT_GAME,
                     stem);
            unit_out("  $ %s", rules_desc);
        } else {
            unit_out("  require exit %d and a last line `GATE OK-PENDING ... pending-flip=%s (...)`",
                     UNIT_GATE_PENDING_EXIT, path);
        }
        if (problems.n) {
            unit_out("flip would REFUSE:");
            for (size_t i = 0; i < problems.n; i++) unit_out("  - %.*s", (int)problems.v[i].n, problems.v[i].p);
            return 1;
        }
        unit_out("pre-checks pass; nothing changed");
        return 0;
    }
    if (problems.n) {
        unit_out("refusing to flip:");
        for (size_t i = 0; i < problems.n; i++) unit_out("  - %.*s", (int)problems.v[i].n, problems.v[i].p);
        return 1;
    }
    Flip f = {p, path, NULL, {0}, split, NULL};
    unit_step_reset();
    if (!custom) {
        unit_step("layout", "gatecheck layout on the built object");
        layout_check(p, stem);
    }
    AvpStr new_text = set_status(a, cfg_text, path, "Matching", &f.old);
    if (!avp_read_all(a, p->configure_path, &f.original)) shsm_raise_io();
    unit_write_text(a, p->configure_path, new_text, eol);
    unit_out("configure.py: %s: %s -> Matching", path, f.old);
    AvpStr txt;
    if (!custom) {
        unit_step("gate", "gate.exe --pending-flip: clean rebuild in build/gate/tree, then the checks; silent until it ends");
        rc = run_logged(a, gate_argv, NULL, p->root, &txt);
        const char *last = last_gate_line(a, txt);
        if (!strncmp(last, "GATE ", 5)) f.gate_line = last;
        const char *line = unit_gate_pending_ok(a, rc, txt, path);
        if (!line) {
            unit_out_str(gate_excerpt(a, txt, 60));
            return revert(&f, avp_sprintf(a, "gate.exe exited %d: %s", rc, last));
        }
        unit_out("gate line: %s", line);
        unit_out("%s is Matching in configure.py (not committed).  The gate passed with this edit as the tree's only "
                 "change; it is not a banked gate until the commit gates green.",
                 path);
        unit_out("Commit it with the refreshed baseline and ledger (not done by this tool):");
        unit_out("  cp build/gate/tree/build/%s/report.json config/%s/baseline_report.json", UNIT_GAME, UNIT_GAME);
        unit_out("  build/tools/ledger.exe progress --report config/%s/baseline_report.json", UNIT_GAME);
        unit_out("  git add configure.py config/%s/baseline_report.json docs/progress.md docs/exceptions.md", UNIT_GAME);
        unit_out("  git commit -m \"unit: flip %s to Matching\"", stem);
        unit_out("  build/tools/gate.exe   (must print GATE OK on the commit)");
        unit_out("%s", f.gate_line ? f.gate_line : line);
        return 0;
    }
    double t_gate = avp_time();
    unit_step("gate", "the --gate-cmd command");
    rc = run_logged(a, NULL, args->gate_cmd, p->root, &txt);
    if (rc != 0)
        return revert(&f, avp_sprintf(a, "the gate command exited %d:\n%s", rc, avp_cz(a, tail(a, txt, 30))));
    ShsmReMatch m;
    if (!shsm_re_search(shsm_re("^\\s*build[/\\\\]R5WEA4[/\\\\]main\\.dol: OK\\s*$", SHSM_RE_M), txt, 0, AVP_END, &m))
        return revert(&f, avp_sprintf(a, "exit code 0 but the output has no `build/%s/main.dol: OK` line:\n%s", UNIT_GAME,
                                      avp_cz(a, tail(a, txt, 30))));
    AvpStr gl = avp_str_strip(shsm_re_group(&m, 0));
    unit_out("gate line: %.*s", (int)gl.n, gl.p);
    const char *rep = avp_join(a, p->root, "build", UNIT_GAME, "report.json");
    const char *objdiff_skipped = NULL;
    double mt = 0;
    if (avp_exists(rep) && avp_getmtime(rep, &mt) && mt < t_gate) {
        objdiff_skipped = "report.json is older than this run (custom gate command)";
        unit_out("%s: objdiff check SKIPPED", objdiff_skipped);
    } else if (avp_exists(rep)) {
        AvpStr js = unit_read_text(a, rep, NULL);
        AvpJson *data = avp_json_parse(a, js, NULL);
        AvpJson *units = avp_json_getz(data, "units");
        AvpStr cur_cfg = unit_read_text(a, p->configure_path, NULL);
        UnitConfig *c2 = unit_parse_configure(a, cur_cfg);
        AvpStrVec bad = {0};
        size_t nmatching = 0;
        for (size_t i = 0; i < c2->objects.n; i++) {
            if (strcmp(c2->objects.v[i].status, "Matching")) continue;
            nmatching++;
            const char *uname = avp_sprintf(a, "main/%s", unit_stem(a, c2->objects.v[i].path));
            AvpJson *u = NULL;
            for (size_t k = 0; units && k < avp_json_len(units); k++) {
                AvpJson *x = avp_json_at(units, k);
                if (avp_str_eqz(avp_json_str_or(avp_json_getz(x, "name"), AVP_S("")), uname)) u = x;
            }
            if (!u) {
                avp_vec_push(&bad, avp_str(avp_sprintf(a, "%s None", uname)));
                continue;
            }
            AvpJson *pv = avp_json_getz(avp_json_getz(u, "measures"), "matched_code_percent");
            double pct = pv ? avp_json_f64(pv, 0) : 0;
            if (pct < 100.0) {
                char fr[32];
                avp_float_repr(pct, fr);
                avp_vec_push(&bad, avp_str(avp_sprintf(a, "%s %s", uname, fr)));
            }
        }
        const char *me = avp_sprintf(a, "main/%s", stem);
        bool self_bad = false;
        AvpStrVec mine = {0};
        for (size_t i = 0; i < bad.n; i++) {
            AvpStrVec w = avp_str_split(bad.v[i], AVP_S(" "), -1);
            if (avp_str_eqz(w.v[0], me)) self_bad = true;
            avp_vec_free(&w);
            if (avp_str_startswith(bad.v[i], avp_str(me))) avp_vec_push(&mine, bad.v[i]);
        }
        if (self_bad)
            return revert(&f, avp_sprintf(a, "objdiff: the unit is not 100%% matched: %s",
                                          avp_cz(a, avp_str_join(a, AVP_S("; "), mine.v, mine.n))));
        if (bad.n)
            return revert(&f, avp_sprintf(a,
                                          "objdiff: %zu other Matching unit(s) below 100%% in the report (None = missing "
                                          "from the report): %s",
                                          bad.n, avp_cz(a, avp_str_join(a, AVP_S("; "), bad.v, bad.n))));
        unit_out("objdiff report: all %zu Matching units at matched_code_percent 100", nmatching);
    } else {
        objdiff_skipped = "no report.json (custom gate command)";
        unit_out("%s: objdiff check SKIPPED", objdiff_skipped);
    }
    unit_step("rules", "the rule checker");
    rc = run_logged(a, NULL, rules_desc, p->root, &txt);
    unit_out_str(tail(a, txt, 15));
    if (rc != 0)
        return revert(&f, avp_sprintf(a, "the rule checker exited %d: the unit has [match]-tier findings (MATCH-PENDING)",
                                      rc));
    if (objdiff_skipped)
        unit_out("%s is Matching: the custom gate printed `build/%s/main.dol: OK` and the rule checker passed; the "
                 "objdiff check was SKIPPED (%s): not verified.",
                 path, UNIT_GAME, objdiff_skipped);
    else
        unit_out("%s is Matching: the custom gate printed `build/%s/main.dol: OK`, objdiff and the rule checker passed.",
                 path, UNIT_GAME);
    unit_out("Not a gate.exe result: run `build/tools/gate.exe` on the commit.  Next (not done by this tool): refresh "
             "config/R5WEA4/baseline_report.json, docs/progress.md and docs/exceptions.md in the same commit.");
    return 0;
}
