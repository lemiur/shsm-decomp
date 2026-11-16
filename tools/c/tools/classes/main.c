/* classes.exe: tools/classes.py (mechanical recovery of C++ classes from the retail asm): main(), load_db and the
 * cmd_* front ends. The readers' text is built by classes_read.c, the scan driver is classes_stamp.c. */
#include "classes.h"
#include "classes_decl.h"
#include "shsm_cli.h"
#include "shsm_repo.h"

#include <stdio.h>
#include <string.h>

static const char *checkout_root(AvpArena *a) {
    const char *root = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        root = shsm_find_root(avp_exe_dir()); /* ROOT: the checkout of the tool itself */
        avp_try_end(&t);
    } else {
        root = NULL;
    }
    if (!root) root = shsm_find_root(NULL);
    return avp_path_abspath(a, root);
}

static void out_buf(const AvpBuf *b) {
    if (b->n) fwrite(b->p, 1, b->n, stdout);
}

/* An exception the Python does not catch: its traceback on stderr, exit 1. */
AVP_NORETURN static void traceback(const char *last) {
    fflush(stdout);
    fprintf(stderr, "classes.exe: error: %s\n", last);
    avp_exit(1);
}

typedef struct {
    ClassesScanArgs scan;
    bool strict;
} Opts;

static ClassesDb *load_db(AvpArena *a, const Opts *o) {
    if (!avp_exists(o->scan.paths.out)) {
        ClassesScanArgs quiet = o->scan;
        classes_run_scan(a, &quiet, true);
    }
    ClassesDb *db = classes_db_load(a, o->scan.paths.out);
    const char *why = classes_stale_reason(a, db, o->scan.paths.asm_dir, o->scan.paths.symbols);
    if (why) {
        fflush(stdout);
        fprintf(stderr, "classes.exe: warning: %s is stale: %s; run `build/tools/classes.exe scan`\n",
                o->scan.paths.out, why);
        if (o->strict) avp_exit(1);
    }
    return db;
}

static const ClassesClass *find_or_exit(const ClassesDb *db, const char *q) {
    const ClassesClass *c = classes_find(db, q);
    if (!c) avp_fatal("class not found: %s", q);
    return c;
}

static void finish(const AvpBuf *b, const char *crash) {
    out_buf(b);
    if (crash) traceback(crash);
}

static const char *opt_cz(AvpArena *a, const AvpArgs *r, const char *dest) {
    if (!avp_args_has(r, dest) || avp_args_is_none(r, dest)) return NULL;
    return avp_cz(a, avp_args_str(r, dest));
}

static int dispatch(AvpArena *a, const char *cmd, const AvpArgs *r, Opts *o) {
    if (strcmp(cmd, "scan") == 0) {
        o->scan.json = avp_args_bool(r, "json");
        o->scan.no_cache = avp_args_bool(r, "no_cache");
        const char *rule = opt_cz(a, r, "base_rule"); /* NULL: the default (classes_default_base_rule) */
        if (rule) o->scan.base_rule = strcmp(rule, "ctor") == 0 ? CLASSES_BASE_RULE_CTOR : CLASSES_BASE_RULE_SLOTS;
        classes_run_scan(a, &o->scan, false);
        return 0;
    }
    if (strcmp(cmd, "decl") == 0) {
        CdArgs d = {0};
        d.root = opt_cz(a, r, "root");
        if (d.root) {
            d.root = avp_path_abspath(a, d.root);
            ClassesPaths p = classes_default_paths(a, d.root);
            o->scan.paths.out = p.out;
            o->scan.paths.asm_dir = p.asm_dir;
            o->scan.paths.symbols = p.symbols;
        } else {
            d.root = o->scan.root;
        }
        d.symbols = o->scan.paths.symbols;
        d.cls = opt_cz(a, r, "cls");
        d.with_bases = avp_args_bool(r, "with_bases");
        d.header = opt_cz(a, r, "header");
        d.force = avp_args_bool(r, "force");
        d.stats = avp_args_bool(r, "stats");
        d.json = avp_args_bool(r, "json");
        d.all = avp_args_bool(r, "all");
        d.limit = avp_args_has(r, "limit") && !avp_args_is_none(r, "limit") ? avp_args_int(r, "limit") : SHSM_DEFAULT_LIMIT;
        if (!d.cls && !d.stats) shsm_raise(SHSM_EXC_USAGE, "decl: name a class, or pass --stats");
        ClassesDb *db = load_db(a, o);
        return classes_decl_run(a, db, &d);
    }
    ClassesDb *db = load_db(a, o);
    AvpBuf b;
    avp_buf_init(&b, a);
    if (strcmp(cmd, "show") == 0) {
        const ClassesClass *c = find_or_exit(db, opt_cz(a, r, "cls"));
        finish(&b, avp_args_bool(r, "json") ? classes_show_json(a, c, &b) : classes_show(a, db, c, &b));
    } else if (strcmp(cmd, "header") == 0) {
        const ClassesClass *c = find_or_exit(db, opt_cz(a, r, "cls"));
        finish(&b, classes_header(a, db, c, &b));
    } else {
        const char *q = opt_cz(a, r, "cls");
        const ClassesClass *c = q ? find_or_exit(db, q) : NULL;
        finish(&b, classes_tree(a, db, c, avp_args_bool(r, "all"), avp_args_bool(r, "json"), &b));
    }
    return 0;
}

static int run(int argc, char **argv) {
    AvpArena *a = avp_arena();
    const char *root = checkout_root(a);
    ClassesPaths def = classes_default_paths(a, root);
    AvpArgParser *ap = avp_args_parser(NULL, &(AvpArgParserDesc){
        .prog = "classes.exe", .description = "Recover C++ classes from the retail asm."});
    avp_arg(ap, &(AvpArgDesc){.names = "--asm", .dflt = AVP_DEF_STR(def.asm_dir)});
    avp_arg(ap, &(AvpArgDesc){.names = "--symbols", .dflt = AVP_DEF_STR(def.symbols)});
    avp_arg(ap, &(AvpArgDesc){.names = "--out", .dflt = AVP_DEF_STR(def.out)});
    avp_arg(ap, &(AvpArgDesc){.names = "--splits", .dflt = AVP_DEF_STR(def.splits),
                              .help = "splits.txt, for the .data regions in the statistics"});
    avp_arg(ap, &(AvpArgDesc){.names = "--strict", .action = AVP_ARG_STORE_TRUE,
                              .help = "show/header/tree: exit 1 when classes.json is stale instead of warning"});
    AvpArgSubparsers *sub = avp_args_subparsers(ap, &(AvpArgSubparsersDesc){.dest = "cmd", .required = true});
    AvpArgParser *p = avp_args_subparser(sub, "scan", NULL);
    avp_arg(p, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE});
    avp_arg(p, &(AvpArgDesc){.names = "--no-cache", .action = AVP_ARG_STORE_TRUE,
                             .help = "always rescan (also SHSM_CLASSES_CACHE=0); otherwise an unchanged input stamp "
                                     "keeps classes.json and a scan of identical inputs is restored from the "
                                     "machine-wide cache"});
    static const char *const RULES[] = {"slots", "ctor", NULL};
    avp_arg(p, &(AvpArgDesc){.names = "--base-rule", .choices = RULES,
                             .help = "base-class evidence: ctor (constructor and destructor calls, the default; "
                                     "C tool only) or slots (the slot-overlap rule of classes.exe); without "
                                     "the option SHSM_CLASSES_BASE_RULE=slots|ctor, else ctor"});
    p = avp_args_subparser(sub, "show", NULL);
    avp_arg(p, &(AvpArgDesc){.names = "cls"});
    avp_arg(p, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE});
    p = avp_args_subparser(sub, "header", NULL);
    avp_arg(p, &(AvpArgDesc){.names = "cls"});
    p = avp_args_subparser(sub, "decl", &(AvpArgParserDesc){
        .description = "A compilable class declaration draft from the retail vtable, the sibling pairing, the "
                       "sibling's __vt__ objects (vtable ownership and slot order) and its DWARF (members, return "
                       "types, parameter names); prints the symbols.txt changes it implies. Evidence only."});
    avp_arg(p, &(AvpArgDesc){.names = "cls", .nargs = AVP_NARGS_OPT,
                             .help = "class name, key, vtable symbol or address (classes.json), or a Gormiti class "
                                     "whose __vt__ a retail vtable equals"});
    avp_arg(p, &(AvpArgDesc){.names = "--with-bases", .action = AVP_ARG_STORE_TRUE,
                             .help = "also declare each base not yet defined in include/, base first"});
    avp_arg(p, &(AvpArgDesc){.names = "--header", .metavar = "PATH", .dflt = AVP_DEF_NONE,
                             .help = "write the declaration into a header file with include guards"});
    avp_arg(p, &(AvpArgDesc){.names = "--force", .action = AVP_ARG_STORE_TRUE,
                             .help = "--header: overwrite an existing file"});
    avp_arg(p, &(AvpArgDesc){.names = "--stats", .action = AVP_ARG_STORE_TRUE,
                             .help = "coverage per library group instead of one class: classes whose slots are "
                                     "named >= 90%%, identified as a Gormiti __vt__, with a DWARF layout"});
    avp_arg(p, &(AvpArgDesc){.names = "--root", .metavar = "DIR", .dflt = AVP_DEF_NONE,
                             .help = "checkout to work on (default: the one around the tool)"});
    avp_arg(p, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE});
    avp_arg(p, &(AvpArgDesc){.names = "--all", .action = AVP_ARG_STORE_TRUE,
                             .help = "do not cap the symbols.txt listing"});
    avp_arg(p, &(AvpArgDesc){.names = "--limit", .metavar = "N", .type = AVP_T_INT, .dflt = AVP_DEF_NONE,
                             .help = "cap the symbols.txt listing at N lines (default 40)"});
    p = avp_args_subparser(sub, "tree", NULL);
    avp_arg(p, &(AvpArgDesc){.names = "cls", .nargs = AVP_NARGS_OPT});
    avp_arg(p, &(AvpArgDesc){.names = "--all", .action = AVP_ARG_STORE_TRUE});
    avp_arg(p, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE});
    AvpArgs *r = avp_args_parse(ap, argc, argv);

    Opts o = {0};
    o.scan.paths.asm_dir = opt_cz(a, r, "asm");
    o.scan.paths.symbols = opt_cz(a, r, "symbols");
    o.scan.paths.out = opt_cz(a, r, "out");
    o.scan.paths.splits = opt_cz(a, r, "splits");
    o.scan.root = root;
    o.scan.base_rule = classes_default_base_rule(a); /* also the readers' quiet scan of a missing classes.json */
    o.strict = avp_args_bool(r, "strict");
    const char *cmd = opt_cz(a, r, "cmd");
    int rc = 0;
    AvpTry t;
    if (AVP_TRY(&t)) {
        rc = dispatch(a, cmd, r, &o);
        avp_try_end(&t);
    } else {
        fflush(stdout);
        if (t.code == SHSM_EXC_USAGE) { /* InputError */
            fprintf(stderr, "classes.exe: error: %s\n", t.msg ? t.msg : "");
            return 2;
        }
        AvpStr text = shsm_exc_str(a, t.code, t.msg);
        traceback(avp_cz(a, avp_pyfmt(a, "%s: %S", shsm_exc_class(t.code), &text)));
    }
    return rc;
}

AVP_MAIN(run, "classes")
