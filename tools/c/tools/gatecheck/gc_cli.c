/* gatecheck: the commands and the parser (gatecheck.py cmd_attribute, _p, _exists, find_units, cmd_layout,
 * build_parser, main). */
#include "gatecheck.h"

#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_BUILT "build/R5WEA4/main.dol"
#define DEFAULT_RETAIL "orig/R5WEA4/sys/main.dol"
#define DEFAULT_ELF "build/R5WEA4/main.elf"
#define DEFAULT_SPLITS "config/R5WEA4/splits.txt"
#define DEFAULT_SYMBOLS "config/R5WEA4/symbols.txt"
#define DEFAULT_CONFIGURE "configure.py"
#define DEFAULT_OBJ_DIR "build/R5WEA4/src"
#define DEFAULT_ASM_DIR "build/R5WEA4/asm"

/* ROOT: the checkout this tool belongs to (<root>/build/tools/gatecheck.exe), spelled as os.path.abspath does */
static const char *default_root(AvpArena *a) {
    char *d = avp_strdup(a, avp_exe_dir());
    for (int k = 0; k < 2; k++) {
        char *s = strrchr(d, '/');
        if (s) *s = 0;
    }
    for (char *p = d; *p; p++)
        if (*p == '/') *p = '\\';
    if (strlen(d) == 2 && d[1] == ':') d = avp_sprintf(a, "%s\\", d);
    return d;
}

static const char *pjoin(AvpArena *a, const char *root, const char *path) {
    return avp_path_isabs(path) ? path : avp_path_join(a, root, path);
}

/* args.x or default */
static const char *opt_or(AvpArena *a, const AvpArgs *r, const char *dest, const char *dflt) {
    AvpStr v = avp_args_str(r, dest);
    return v.p && v.n ? avp_cz(a, v) : dflt;
}

static void print_lines(const AvpStrVec *v) {
    for (size_t i = 0; i < v->n; i++) {
        fwrite(v->v[i].p, 1, v->v[i].n, stdout);
        fputc('\n', stdout);
    }
}

static void warn_line(const char *s) {
    fflush(stdout);
    fputs(s, stderr);
    fputc('\n', stderr);
    fflush(stderr);
}

/* ---- attribute --------------------------------------------------------------------------------------------------- */

static int cmd_attribute(AvpArena *a, const AvpArgs *r, const char *root) {
    const char *built_path = pjoin(a, root, opt_or(a, r, "built", DEFAULT_BUILT));
    const char *retail_path = pjoin(a, root, opt_or(a, r, "retail", DEFAULT_RETAIL));
    const ShsmDol *retail = shsm_dol_load(a, retail_path);
    const ShsmDol *built = shsm_dol_load(a, built_path);
    GcSplitVec splits = {0};
    const char *sp = pjoin(a, root, opt_or(a, r, "splits", DEFAULT_SPLITS));
    if (avp_exists(sp)) splits = gc_parse_splits(a, gc_read_text(a, sp));
    const char *cfg = pjoin(a, root, opt_or(a, r, "configure", DEFAULT_CONFIGURE));
    AvpMap *statuses = avp_exists(cfg) ? gc_parse_configure(a, gc_read_text(a, cfg)) : avp_map_new();
    GcUnitMap *units = gc_unit_map(a, &splits, statuses);
    const GcSymbolMap *built_syms = NULL;
    const char *elf_path = pjoin(a, root, opt_or(a, r, "elf", DEFAULT_ELF));
    if (avp_exists(elf_path)) {
        AvpTry t;
        if (AVP_TRY(&t)) {
            GcElf *e = gc_elf_load(a, elf_path);
            const char *stale = gc_elf_dol_mismatch(a, e, built);
            if (stale) {
                double me = 0, mb = 0;
                avp_getmtime(elf_path, &me);
                avp_getmtime(built_path, &mb);
                warn_line(avp_sprintf(a,
                                      "WARNING: %s does not belong to %s: %s%s. Its symbol names would be wrong; they "
                                      "are NOT used (falling back to symbols.txt names). Rebuild from a clean build/ "
                                      "to get the built names.",
                                      elf_path, built_path, stale,
                                      me < mb ? " (and the ELF is older than the DOL)" : ""));
            } else {
                built_syms = gc_elf_symbol_map(a, e);
                AVP_VEC(GcElfRange) rs = {0};
                for (size_t k = 0; k < e->nsecs; k++) {
                    const GcSec *s = &e->secs[k];
                    if ((s->flags & 2) && s->addr && s->size)
                        avp_vec_push(&rs, ((GcElfRange){s->addr, s->addr + s->size, s->name}));
                }
                /* sorted((addr, end, name)) */
                for (size_t i = 1; i < rs.n; i++)
                    for (size_t j = i; j > 0; j--) {
                        GcElfRange *x = &rs.v[j - 1], *y = &rs.v[j];
                        int c = avp_cmp_i64(x->addr, y->addr);
                        if (!c) c = avp_cmp_i64(x->end, y->end);
                        if (!c) c = strcmp(x->name, y->name);
                        if (c <= 0) break;
                        GcElfRange tmp = *x;
                        *x = *y;
                        *y = tmp;
                    }
                units->elf_sections = rs.v;
                units->nelf = rs.n;
            }
            avp_try_end(&t);
        } else {
            if (!(gc_is_gate(t.code) || t.code == GC_EXC_VALUE || t.code == GC_EXC_STRUCT))
                avp_throw(t.code, "%s", t.msg);
            warn_line(avp_sprintf(a, "warning: cannot use %s: %s", elf_path, t.msg));
        }
    }
    const GcSymbolMap *retail_syms = NULL;
    const char *sym_path = pjoin(a, root, opt_or(a, r, "symbols", DEFAULT_SYMBOLS));
    if (avp_exists(sym_path)) {
        GcSymTxtVec st = gc_parse_symbols_txt(a, gc_read_text(a, sym_path));
        GcEntry *ents = AVP_NEWN(a, GcEntry, st.n + 1);
        for (size_t i = 0; i < st.n; i++) ents[i] = (GcEntry){st.v[i].addr, st.v[i].size, st.v[i].name};
        retail_syms = gc_symbol_map(a, ents, st.n);
    }
    if (!built_syms) {
        built_syms = retail_syms;
        warn_line("warning: no built ELF symbols; falling back to symbols.txt names");
    }
    i64 words = avp_args_int(r, "words");
    GcAttribution *res = gc_attribute(a, retail, built, units, built_syms, retail_syms, words);
    AvpStrVec lines = gc_format_attribution(a, res, units, avp_args_int(r, "limit_symbols"),
                                            avp_args_int(r, "limit_units"), avp_args_bool(r, "all_symbols"),
                                            built_path, retail_path);
    print_lines(&lines);
    bool identical = !(res->header.n || res->runs.n || res->only_retail.n || res->only_built.n);
    fputs("\n", stdout);
    printf("result: %s\n", identical ? "identical" : "DIFFERENT");
    return identical ? 0 : 1;
}

/* ---- layout ------------------------------------------------------------------------------------------------------ */

typedef struct {
    const char *path, *status, *obj, *asm_;
} Target;
typedef AVP_VEC(Target) TargetVec;

static int cmp_target(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const Target *a = x, *b = y;
    int c = strcmp(a->path, b->path);
    if (!c) c = strcmp(a->status, b->status);
    if (!c) c = strcmp(a->obj, b->obj);
    if (!c) c = strcmp(a->asm_, b->asm_);
    return c;
}

static TargetVec find_units(AvpArena *a, const char *root, AvpMap *statuses, AvpSet *wanted) {
    TargetVec out = {0};
    AvpMapIter it = {0};
    while (avp_map_next(statuses, &it)) {
        const char *st = it.val->p;
        if (!avp_set_has(wanted, avp_str(st))) continue;
        const char *path = avp_cz(a, it.key);
        const char *key = gc_strip_ext(a, path);
        const char *obj = avp_join(a, root, DEFAULT_OBJ_DIR, avp_sprintf(a, "%s.o", key));
        const char *asm_ = avp_join(a, root, DEFAULT_ASM_DIR, avp_sprintf(a, "%s.s", key));
        if (avp_exists(obj) && avp_exists(asm_)) avp_vec_push(&out, ((Target){path, st, obj, asm_}));
    }
    avp_sort(out.v, out.n, sizeof(Target), cmp_target, NULL, false);
    return out;
}

typedef struct {
    const char *status;
    i64 n, ndiff;
} StatusCount;

static int cmp_status(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return strcmp(((const StatusCount *)x)->status, ((const StatusCount *)y)->status);
}

/* The target of one named unit: its configure.py path and status ("unlisted"), its object and listing (the
 * defaults under build/R5WEA4 unless --object / --asm override them). */
static Target unit_target(AvpArena *a, const char *root, AvpMap *statuses, const char *unit, const char *object,
                          const char *asm_over) {
    const char *key = gc_strip_ext(a, gc_unit_key(a, unit));
    const char *path = key;
    AvpMapIter it = {0};
    while (avp_map_next(statuses, &it)) {
        const char *p = avp_cz(a, it.key);
        if (strcmp(gc_strip_ext(a, p), key) == 0) {
            path = p;
            break;
        }
    }
    const char *obj = object ? object : avp_join(a, root, DEFAULT_OBJ_DIR, avp_sprintf(a, "%s.o", key));
    const char *asm_ = asm_over ? asm_over : avp_join(a, root, DEFAULT_ASM_DIR, avp_sprintf(a, "%s.s", key));
    AvpVal *v = avp_map_get(statuses, avp_str(path));
    return (Target){path, v ? v->p : "unlisted", obj, asm_};
}

/* The first of the target's object and listing that does not exist, or NULL. */
static const char *target_missing(const Target *t) {
    if (!avp_exists(t->obj)) return t->obj;
    if (!avp_exists(t->asm_)) return t->asm_;
    return NULL;
}

/* The weak-symbol dedupe index of splits.txt and symbols.txt (empty without them). */
static GcWeakVec load_dedupe(AvpArena *a, const char *root) {
    GcWeakVec dedupe = {0};
    const char *splits_path = avp_path_join(a, root, DEFAULT_SPLITS);
    const char *symbols_path = avp_path_join(a, root, DEFAULT_SYMBOLS);
    if (avp_exists(splits_path) && avp_exists(symbols_path)) {
        GcSymTxtVec syms = gc_parse_symbols_txt(a, gc_read_text(a, symbols_path));
        GcSplitVec splits = gc_parse_splits(a, gc_read_text(a, splits_path));
        dedupe = gc_weak_dedupe_index(a, &syms, &splits);
    }
    return dedupe;
}

static int cmd_layout(AvpArena *a, const AvpArgs *r, const char *root) {
    const char *retail_path = pjoin(a, root, opt_or(a, r, "retail", DEFAULT_RETAIL));
    const ShsmDol *retail = shsm_dol_load(a, retail_path);
    const char *cfg = pjoin(a, root, opt_or(a, r, "configure", DEFAULT_CONFIGURE));
    AvpMap *statuses = avp_exists(cfg) ? gc_parse_configure(a, gc_read_text(a, cfg)) : avp_map_new();
    AvpStr status_arg = avp_args_str(r, "status");
    AvpSet *wanted = avp_set_new();
    AvpStrVec sts = avp_str_split(status_arg, AVP_S(","), -1);
    for (size_t i = 0; i < sts.n; i++) avp_set_add(wanted, avp_str_strip(sts.v[i]));
    avp_vec_free(&sts);
    bool all = avp_args_bool(r, "all");
    AvpStrVec units = avp_args_strs(r, "units");
    const char *object = opt_or(a, r, "object", NULL), *asm_over = opt_or(a, r, "asm", NULL);
    TargetVec targets = {0};
    if ((object || asm_over) && (all || units.n != 1)) {
        warn_line("layout: --object and --asm override the files of a single unit; give exactly one unit (not --all)");
        return 2;
    }
    if (all) {
        targets = find_units(a, root, statuses, wanted);
        if (!targets.n) {
            warn_line(avp_sprintf(a,
                                  "layout: --all found no unit to check (status %.*s, each needs %s/<unit>.o and "
                                  "%s/<unit>.s): nothing was verified",
                                  AVP_SF(status_arg), DEFAULT_OBJ_DIR, DEFAULT_ASM_DIR));
            printf("units checked: 0\n");
            return 1;
        }
    } else {
        if (!units.n) {
            warn_line("layout: give a unit or --all");
            return 2;
        }
        for (size_t i = 0; i < units.n; i++) {
            Target t = unit_target(a, root, statuses, avp_cz(a, units.v[i]), object, asm_over);
            const char *missing = target_missing(&t);
            if (missing) {
                warn_line(avp_sprintf(a, "layout: missing %s", missing));
                return 2;
            }
            avp_vec_push(&targets, t);
        }
    }
    if (!object && !asm_over) {
        /* an object ninja has not rebuilt since an input changed describes the previous build: say so, do not judge it */
        GcStaleCheck *sc = gc_stale_open(a, root, pjoin(a, root, DEFAULT_SPLITS), pjoin(a, root, DEFAULT_SYMBOLS));
        size_t nstale = 0;
        const char *first_unit = NULL, *first_input = NULL, *first_obj = NULL;
        for (size_t ti = 0; ti < targets.n; ti++) {
            const Target *t = &targets.v[ti];
            const char *obj_rel = avp_sprintf(a, "%s/%s.o", DEFAULT_OBJ_DIR, gc_strip_ext(a, t->path));
            const char *newer = gc_stale_newer_input(a, sc, obj_rel, t->obj, avp_sprintf(a, "src/%s", t->path));
            if (!newer) continue;
            if (!nstale++) {
                first_unit = t->path;
                first_input = newer;
                first_obj = obj_rel;
            }
        }
        if (nstale) {
            if (!all && targets.n == 1)
                warn_line(avp_sprintf(a, "layout: STALE object %s is older than %s; rebuild it with `tools/bin/ninja.exe %s` "
                                         "and run again (nothing was verified)",
                                      first_obj, first_input, first_obj));
            else
                warn_line(avp_sprintf(a, "layout: STALE: %zu of %zu objects are older than their inputs (first: %s, older than "
                                         "%s); rebuild with `tools/bin/ninja.exe` and run again (nothing was verified)",
                                      nstale, targets.n, first_unit, first_input));
            return GC_EXIT_STALE;
        }
    }
    GcWeakVec dedupe = load_dedupe(a, root);
    const char *section = opt_or(a, r, "section", NULL);
    bool verbose = avp_args_bool(r, "verbose");
    i64 bad = 0, errors = 0;
    AVP_VEC(StatusCount) by_status = {0};
    for (size_t ti = 0; ti < targets.n; ti++) {
        const Target *t = &targets.v[ti];
        AvpArena *ua = avp_arena_new(0);
        GcLayoutResult *res = gc_check_layout(ua, t->path, t->status, t->obj, t->asm_, retail, section, &dedupe);
        if (res->errors.n) errors++;
        size_t ndiff = gc_layout_ndiffs(res);
        if (ndiff) bad++;
        StatusCount *sc = NULL;
        for (size_t q = 0; q < by_status.n; q++)
            if (strcmp(by_status.v[q].status, t->status) == 0) sc = &by_status.v[q];
        if (!sc) {
            sc = avp_vec_push_zero(&by_status);
            sc->status = t->status;
        }
        sc->n++;
        sc->ndiff += ndiff ? 1 : 0;
        if (all) {
            /* counts: kind -> n, sorted by kind */
            AVP_VEC(StatusCount) c = {0};
            for (size_t q = 0; q < res->findings.n; q++) {
                if (!res->findings.v[q].difference) continue;
                StatusCount *k = NULL;
                for (size_t z = 0; z < c.n; z++)
                    if (strcmp(c.v[z].status, res->findings.v[q].kind) == 0) k = &c.v[z];
                if (!k) {
                    k = avp_vec_push_zero(&c);
                    k->status = res->findings.v[q].kind;
                }
                k->n++;
            }
            avp_sort(c.v, c.n, sizeof(StatusCount), cmp_status, NULL, false);
            AvpBuf cb;
            avp_buf_init(&cb, ua);
            if (c.n) {
                avp_buf_putz(&cb, "  (");
                for (size_t z = 0; z < c.n; z++) {
                    if (z) avp_buf_putz(&cb, ", ");
                    avp_buf_printf(&cb, "%s %lld", c.v[z].status, (long long)c.v[z].n);
                }
                avp_buf_putz(&cb, ")");
            }
            avp_vec_free(&c);
            AvpStr line = avp_pyfmt(ua, "%-5s %-11s %s%s", ndiff ? "DIFF" : (res->errors.n ? "ERR" : "ok"), t->status,
                                    t->path, avp_buf_cz(&cb));
            fwrite(line.p, 1, line.n, stdout);
            fputc('\n', stdout);
        } else {
            printf("== %s [%s]  %s\n", t->path, t->status, ndiff ? "DIFFERENT" : "layout matches");
        }
        AvpStrVec lines = gc_format_layout(ua, res, verbose);
        print_lines(&lines);
        avp_vec_free(&lines);
        avp_map_free(res->sections);
        avp_arena_free(ua);
    }
    if (all) {
        avp_sort(by_status.v, by_status.n, sizeof(StatusCount), cmp_status, NULL, false);
        printf("\nunits checked: %zu; ", targets.n);
        for (size_t q = 0; q < by_status.n; q++)
            printf("%s%s %lld (%lld with differences)", q ? "; " : "", by_status.v[q].status,
                   (long long)by_status.v[q].n, (long long)by_status.v[q].ndiff);
        printf("\n");
    }
    if (errors && !bad) return 2;
    return bad ? 1 : 0;
}

/* ---- parser and main --------------------------------------------------------------------------------------------- */

static AvpArgParser *build_parser(AvpArena *a, const char *root) {
    AvpArgParser *ap = avp_args_parser(a, &(AvpArgParserDesc){
        .prog = "gatecheck",
        .description = "Gate diagnostics for the SHSM matching decomp: turn a red byte gate into named symbols,",
        .formatter = AVP_FORMAT_RAW_DESCRIPTION});
    avp_arg(ap, &(AvpArgDesc){.names = "--root", .dflt = AVP_DEF_STR(root),
                              .help = "repository root (default: this checkout)"});
    AvpArgSubparsers *sub = avp_args_subparsers(ap, &(AvpArgSubparsersDesc){.dest = "cmd"});
    AvpArgParser *at = avp_args_subparser(sub, "attribute",
                                          &(AvpArgParserDesc){.help = "name the symbols behind a DOL hash mismatch"});
    avp_arg(at, &(AvpArgDesc){.names = "--built", .help = "built DOL (default " DEFAULT_BUILT ")"});
    avp_arg(at, &(AvpArgDesc){.names = "--retail", .help = "retail DOL (default " DEFAULT_RETAIL ")"});
    avp_arg(at, &(AvpArgDesc){.names = "--elf", .help = "linked ELF for built symbol names (default " DEFAULT_ELF ")"});
    avp_arg(at, &(AvpArgDesc){.names = "--splits", .help = "splits.txt (default " DEFAULT_SPLITS ")"});
    avp_arg(at, &(AvpArgDesc){.names = "--symbols", .help = "symbols.txt (default " DEFAULT_SYMBOLS ")"});
    avp_arg(at, &(AvpArgDesc){.names = "--configure",
                              .help = "configure.py, parsed for unit status (default " DEFAULT_CONFIGURE ")"});
    avp_arg(at, &(AvpArgDesc){.names = "--words", .type = AVP_T_INT, .dflt = AVP_DEF_INT(3),
                              .help = "differing words shown per symbol"});
    avp_arg(at, &(AvpArgDesc){.names = "--limit-symbols", .type = AVP_T_INT, .dflt = AVP_DEF_INT(12),
                              .help = "symbols shown per unit"});
    avp_arg(at, &(AvpArgDesc){.names = "--limit-units", .type = AVP_T_INT, .dflt = AVP_DEF_INT(40),
                              .help = "units shown"});
    avp_arg(at, &(AvpArgDesc){.names = "--all-symbols", .action = AVP_ARG_STORE_TRUE,
                              .help = "do not limit symbols per unit"});
    AvpArgParser *l = avp_args_subparser(
        sub, "layout", &(AvpArgParserDesc){.help = "compare a unit's static data layout with the retail split"});
    avp_arg(l, &(AvpArgDesc){.names = "units", .nargs = AVP_NARGS_STAR,
                             .help = "unit, e.g. revolution/os/OSStateTM or main/game/PadRumble"});
    avp_arg(l, &(AvpArgDesc){.names = "--all", .action = AVP_ARG_STORE_TRUE,
                             .help = "every Matching/NonMatching unit with an object"});
    avp_arg(l, &(AvpArgDesc){.names = "--status", .dflt = AVP_DEF_STR("Matching,NonMatching"),
                             .help = "statuses for --all"});
    avp_arg(l, &(AvpArgDesc){.names = "--retail", .help = "retail DOL (default " DEFAULT_RETAIL ")"});
    avp_arg(l, &(AvpArgDesc){.names = "--configure", .help = "configure.py (default " DEFAULT_CONFIGURE ")"});
    avp_arg(l, &(AvpArgDesc){.names = "--object", .help = "object file override (single unit)"});
    avp_arg(l, &(AvpArgDesc){.names = "--asm", .help = "retail asm override (single unit)"});
    avp_arg(l, &(AvpArgDesc){.names = "--section", .help = "check one section only, e.g. .sdata2"});
    avp_arg(l, &(AvpArgDesc){.names = "-v --verbose", .action = AVP_ARG_STORE_TRUE,
                             .help = "section summaries and notes"});
    return ap;
}

static const char *exc_class(int code) {
    switch (code) {
    case GC_EXC_STRUCT: return "struct.error";
    case GC_EXC_VALUE: return "ValueError";
    case GC_EXC_INDEX: return "IndexError";
    default: return "Exception";
    }
}

int gc_main(int argc, char **argv) {
    AvpArena *a = avp_arena();
    const char *root0 = default_root(a);
    AvpArgParser *ap = build_parser(a, root0);
    AvpArgs *r = avp_args_parse(ap, argc, argv);
    AvpStr cmd = avp_args_str(r, "cmd");
    if (!cmd.p) {
        avp_args_print_help(ap);
        return 2;
    }
    const char *root = avp_cz(a, avp_args_str(r, "root"));
    int rc = 0;
    AvpTry t;
    if (AVP_TRY(&t)) {
        rc = avp_str_eqz(cmd, "attribute") ? cmd_attribute(a, r, root) : cmd_layout(a, r, root);
        avp_try_end(&t);
    } else if (gc_is_gate(t.code)) {
        fflush(stdout);
        fprintf(stderr, "gatecheck: %s\n", t.msg);
        rc = 2;
    } else {
        /* an unexpected internal error: one line on stderr, exit status 1 */
        fflush(stdout);
        if (t.code == GC_EXC_CRASH)
            fprintf(stderr, "gatecheck.exe: error: %s\n", t.msg);
        else
            fprintf(stderr, "gatecheck.exe: error: %s: %s\n", exc_class(t.code), t.msg);
        rc = 1;
    }
    return rc;
}

/* `gatecheck layout <unit> [--object OBJ]` in-process (see gatecheck.h). */
int gc_layout_lines(AvpArena *a, const char *root, const char *unit, const char *object, AvpStrVec *out,
                    AvpStrVec *err) {
    int rc = 0;
    AvpTry t;
    if (AVP_TRY(&t)) {
        const ShsmDol *retail = shsm_dol_load(a, pjoin(a, root, DEFAULT_RETAIL));
        const char *cfg = pjoin(a, root, DEFAULT_CONFIGURE);
        AvpMap *statuses = avp_exists(cfg) ? gc_parse_configure(a, gc_read_text(a, cfg)) : avp_map_new();
        Target tg = unit_target(a, root, statuses, unit, object, NULL);
        const char *missing = target_missing(&tg);
        if (missing) {
            avp_vec_push(err, avp_str(avp_sprintf(a, "layout: missing %s", missing)));
            rc = 2;
        } else {
            GcWeakVec dedupe = load_dedupe(a, root);
            GcLayoutResult *res = gc_check_layout(a, tg.path, tg.status, tg.obj, tg.asm_, retail, NULL, &dedupe);
            size_t ndiff = gc_layout_ndiffs(res);
            avp_vec_push(out, avp_str(avp_sprintf(a, "== %s [%s]  %s", tg.path, tg.status,
                                                  ndiff ? "DIFFERENT" : "layout matches")));
            AvpStrVec lines = gc_format_layout(a, res, false);
            for (size_t i = 0; i < lines.n; i++) avp_vec_push(out, lines.v[i]);
            avp_vec_free(&lines);
            rc = ndiff ? 1 : res->errors.n ? 2 : 0;
        }
        avp_try_end(&t);
    } else if (gc_is_gate(t.code)) {
        avp_vec_push(err, avp_str(avp_sprintf(a, "gatecheck: %s", t.msg)));
        rc = 2;
    } else {
        avp_vec_push(err, avp_str(t.code == GC_EXC_CRASH ? avp_sprintf(a, "gatecheck.exe: error: %s", t.msg)
                                                         : avp_sprintf(a, "gatecheck.exe: error: %s: %s", exc_class(t.code), t.msg)));
        rc = 1;
    }
    return rc;
}
