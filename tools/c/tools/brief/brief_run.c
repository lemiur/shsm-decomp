/* brief.py run(), build_parser() and main(). */
#include "brief.h"
#include "pick.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#define brief_dup _dup
#define brief_dup2 _dup2
#define brief_close _close
#define brief_open_null() _open("NUL", _O_WRONLY)
#else
#include <unistd.h>
#define brief_dup dup
#define brief_dup2 dup2
#define brief_close close
#define brief_open_null() open("/dev/null", O_WRONLY)
#endif

static const char *const SECTIONS[] = {"identity", "status", "twins", "context", "shape", "listing", "start"};
static const char *const UNIT_SECTIONS[] = {"header", "functions", "classes", "includes", "notes", "start"};

static void print_line(AvpStr s) {
    fwrite(s.p, 1, s.n, stdout);
    fputc('\n', stdout);
}

static AvpStr opt_str(const AvpArgs *args, const char *dest) {
    if (!avp_args_has(args, dest) || avp_args_is_none(args, dest)) return (AvpStr){0};
    return avp_args_str(args, dest);
}

/* The briefing of one target under `o`, each output line passed to `emit` (run()'s printing, or brief_page's
 * collecting); raises as the Python's run() does. */
typedef void (*BriefEmit)(void *ctx, AvpStr line);

static void run_opts(BriefOpts *o, BriefEmit emit, void *ectx) {
    AvpArena *a = avp_arena();
    double t0 = avp_time();
    BriefWorld *W = brief_world_new(o->root);
    BriefSections secs = {0};
    BriefTimings tm = {0};
    const char *title;
    bool unit_mode = o->unit && *o->unit;
    AvpTry t;
    if (!AVP_TRY(&t)) {
        brief_world_close(W);
        avp_throw(t.code, "%s", t.msg);
    }
    if (unit_mode) {
        o->sections = brief_parse_sections(o->section, UNIT_SECTIONS, 6);
        title = brief_unit_briefing(W, o, &secs, &tm);
    } else {
        if (!o->target || !*o->target) shsm_raise(SHSM_EXC_USAGE, "give a symbol or address, or --unit UNIT|START-END");
        o->sections = brief_parse_sections(o->section, SECTIONS, 7);
        title = brief_function(W, o, &secs, &tm);
    }
    int levels[8];
    brief_layout(&secs, o->lines, o->full, levels);
    BriefLines lines = brief_render(&secs, levels);
    brief_timing(&tm, "total", avp_time() - t0);
    if (o->json) {
        AvpJson *doc = avp_json_obj(a), *sj = avp_json_obj(a), *tj = avp_json_obj(a);
        brief_set(doc, "target", avp_json_strz(a, title));
        brief_set(doc, "kind", avp_json_strz(a, unit_mode ? "unit" : "function"));
        for (size_t i = 0; i < secs.n; i++) brief_set(sj, secs.v[i]->name, brief_section_json(secs.v[i], levels[i]));
        brief_set(doc, "sections", sj);
        for (size_t i = 0; i < tm.n; i++) brief_set(tj, tm.keys[i], avp_json_float(a, avp_py_round(tm.vals[i], 3)));
        brief_set(doc, "timings", tj);
        AvpJsonDumpOpts opts = shsm_json_opts();
        emit(ectx, avp_json_dumps(a, doc, &opts));
    } else {
        const ShsmReport *rep = brief_report(W);
        const char *stale = rep && rep->path ? pick_stale_note(a, W->repo, rep->path, false) : NULL;
        if (stale) {
            size_t at = lines.n ? 1 : 0;
            avp_vec_push(&lines, AVP_S(""));
            memmove(&lines.v[at + 1], &lines.v[at], (lines.n - 1 - at) * sizeof(AvpStr));
            lines.v[at] = avp_str(stale);
        }
        for (size_t i = 0; i < lines.n; i++) emit(ectx, lines.v[i]);
        if (o->timings) {
            AvpStrVec parts = {0};
            for (size_t i = 0; i < tm.n; i++) avp_vec_push(&parts, avp_pyfmt(a, "%s %.2fs", tm.keys[i], tm.vals[i]));
            AvpStr j = avp_str_join(a, AVP_S(", "), parts.v, parts.n);
            emit(ectx, avp_pyfmt(a, "# timings: %s", avp_cz(a, j)));
            avp_vec_free(&parts);
        }
    }
    avp_try_end(&t);
    brief_world_close(W);
}

static void emit_print(void *ctx, AvpStr line) {
    (void)ctx;
    print_line(line);
}

static int run(void *vctx, AvpArgs *args) {
    (void)vctx;
    AvpArena *a = avp_arena();
    BriefOpts o = {0};
    AvpStr v;
    o.target = (v = opt_str(args, "target")).p ? avp_cz(a, v) : NULL;
    o.unit = (v = opt_str(args, "unit")).p ? avp_cz(a, v) : NULL;
    o.section = (v = opt_str(args, "section")).p ? avp_cz(a, v) : NULL;
    o.root = (v = opt_str(args, "root")).p ? avp_cz(a, v) : NULL;
    o.lines = avp_args_int(args, "lines");
    o.listing_lines = avp_args_int(args, "listing_lines");
    o.full = avp_args_bool(args, "full");
    o.asm_ = avp_args_bool(args, "asm");
    o.no_listing = avp_args_bool(args, "no_listing");
    o.no_compile = avp_args_bool(args, "no_compile");
    o.timings = avp_args_bool(args, "timings");
    o.json = avp_args_bool(args, "json");
    run_opts(&o, emit_print, NULL);
    return SHSM_EXIT_OK;
}

static void emit_collect(void *ctx, AvpStr line) {
    AvpStrVec *out = ctx;
    avp_vec_push(out, avp_str_dup(avp_arena(), line));
}

int brief_page(const char *root, const char *target, bool unit, i64 lines, AvpStrVec *out) {
    BriefOpts o = {0};
    if (unit) o.unit = target;
    else o.target = target;
    o.root = root;
    o.lines = lines;
    o.listing_lines = BRIEF_LISTING_LINES;
    /* the caller ran brief as a child and kept only its stdout: what brief writes to stderr (the error line,
     * unit_plan's stale-classes warning) is dropped for the call */
    fflush(stderr);
    int saved = brief_dup(2), null_fd = brief_open_null();
    if (saved >= 0 && null_fd >= 0) brief_dup2(null_fd, 2);
    int code = SHSM_EXIT_OK;
    AvpTry t;
    if (!AVP_TRY(&t)) {
        code = t.code == SHSM_EXC_FINDING ? SHSM_EXIT_FINDING : SHSM_EXIT_ERROR;
    } else {
        run_opts(&o, emit_collect, out);
        avp_try_end(&t);
    }
    fflush(stderr);
    if (saved >= 0 && null_fd >= 0) brief_dup2(saved, 2);
    if (saved >= 0) brief_close(saved);
    if (null_fd >= 0) brief_close(null_fd);
    return code;
}

int brief_main(int argc, char **argv) {
    AvpArena *a = avp_arena();
    AvpArgParser *ap = shsm_cli_parser("brief.exe", "The starting briefing for one function or unit.", true, true, false);
    avp_arg(ap, &(AvpArgDesc){.names = "target", .nargs = AVP_NARGS_OPT,
                              .help = "symbol, demangled or partial name, or address"});
    avp_arg(ap, &(AvpArgDesc){.names = "--unit", .metavar = "UNIT|START-END",
                              .help = "brief a unit or an unstarted address range"});
    avp_arg(ap, &(AvpArgDesc){.names = "--section", .metavar = "NAME[,NAME]",
                              .help = avp_sprintf(a, "only these sections (%s; with --unit: %s)",
                                                  "identity,status,twins,context,shape,listing,start",
                                                  "header,functions,classes,includes,notes,start")});
    avp_arg(ap, &(AvpArgDesc){.names = "--lines", .type = AVP_T_INT, .dflt = AVP_DEF_INT(BRIEF_DEFAULT_LINES),
                              .help = "page budget in lines (default 60)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--listing-lines", .type = AVP_T_INT, .dflt = AVP_DEF_INT(BRIEF_LISTING_LINES),
                              .help = "listing lines before the budget applies (default 40)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--full", .action = AVP_ARG_STORE_TRUE, .help = "lift every cap"});
    AvpArgMutex *g = avp_args_mutex(ap, false);
    avp_arg_mutex(g, &(AvpArgDesc){.names = "--asm", .action = AVP_ARG_STORE_TRUE,
                                   .help = "list the retail assembly, not the Ghidra listing"});
    avp_arg_mutex(g, &(AvpArgDesc){.names = "--no-listing", .action = AVP_ARG_STORE_TRUE, .help = "omit the listing"});
    avp_arg(ap, &(AvpArgDesc){.names = "--no-compile", .action = AVP_ARG_STORE_TRUE,
                              .help = "never run wall.exe or unit_plan.exe (stored data only)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--timings", .action = AVP_ARG_STORE_TRUE, .help = "print where the time went"});
    return shsm_cli_main(ap, argc, argv, run, NULL);
}
