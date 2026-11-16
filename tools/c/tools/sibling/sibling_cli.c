/* sibling: the command line. */
#include "sibling.h"
#include "sibling_dwarf.h"
#include "sibling_foreign.h"

#include "avp/fs.h"
#include "shsm_asm.h"
#include "sibling_lookup.h"

#include <string.h>

static const char DESCRIPTION[] =
    "Pair retail functions with the functions of a sibling binary that keeps its symbols (Gormiti, Wii), and report"
    " names, objects and libraries for them. Evidence only: nothing is edited.";

static bool limit_conv(void *ctx, AvpArena *a, AvpStr in, AvpArgValue *out, AvpStr *msg) {
    (void)ctx;
    i64 n = 0;
    if (!shsm_limit_type(a, in, &n, msg)) return false;
    out->kind = AVP_V_INT;
    out->i = n;
    return true;
}

static void add_shared_options(AvpArgParser *p) {
    avp_arg(p, &(AvpArgDesc){.names = "--root", .metavar = "DIR", .dflt = AVP_DEF_SUPPRESS,
                             .help = "checkout to work on (default: the one around the cwd)"});
    avp_arg(p, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE, .dflt = AVP_DEF_SUPPRESS,
                             .help = "print one JSON document instead of text"});
    avp_arg(p, &(AvpArgDesc){.names = "--all", .action = AVP_ARG_STORE_TRUE, .dflt = AVP_DEF_SUPPRESS,
                             .help = "do not cap the output"});
    avp_arg(p, &(AvpArgDesc){.names = "--limit", .type = AVP_T_CUSTOM, .conv = limit_conv, .type_name = "limit_type",
                             .dflt = AVP_DEF_SUPPRESS, .metavar = "N",
                             .help = "cap text output at N lines (default 40; 0 or 'all': no cap)"});
    avp_arg(p, &(AvpArgDesc){.names = "--debug", .action = AVP_ARG_STORE_TRUE, .dflt = AVP_DEF_SUPPRESS,
                             .help = "print tracebacks"});
    avp_arg(p, &(AvpArgDesc){.names = "--name", .metavar = "NAME", .dflt = AVP_DEF_SUPPRESS,
                             .help = "which sibling database (default gormiti)"});
}

static const char *opt_str(AvpArena *a, AvpArgs *args, const char *key) {
    if (!avp_args_has(args, key) || avp_args_is_none(args, key)) return NULL;
    return avp_cz(a, avp_args_str(args, key));
}

/* "exact,exact-seq,calls" -> bit set; "all": every tier */
static unsigned parse_tiers(const char *text) {
    if (!text || !*text) text = "exact,exact-seq"; /* the tiers measured at about 98% name agreement (stats) */
    if (strcmp(text, "all") == 0) return 0x1Eu;
    unsigned bits = 0;
    AvpStrVec parts = avp_str_split(avp_str(text), AVP_S(","), -1);
    for (size_t k = 0; k < parts.n; k++) {
        const char *t = avp_cz(avp_scratch(), avp_str_strip(parts.v[k]));
        int i = sibling_tier_index(t);
        if (!i) shsm_raise(SHSM_EXC_USAGE, "unknown tier %s (exact, exact-seq, calls, seq-size, all)", t);
        bits |= 1u << i;
    }
    avp_vec_free(&parts);
    return bits;
}

void sibling_link_peers(SibFnVec *s, SibFnVec *g) {
    AvpIMap *at = avp_imap_new();
    for (size_t j = 0; j < g->n; j++) avp_imap_put(at, (u64)g->v[j].address, NULL)->i = (i64)j;
    for (size_t i = 0; i < s->n; i++) {
        if (s->v[i].peer < 0) continue;
        AvpVal *v = avp_imap_get(at, (u64)s->v[i].peer);
        s->v[i].peer = v ? v->i : -1;
        if (!v) s->v[i].tier = SIB_NONE;
        else {
            g->v[v->i].peer = (i64)i;
            g->v[v->i].tier = s->v[i].tier;
        }
    }
    avp_imap_free(at);
}

static int run(void *ctx, AvpArgs *args) {
    (void)ctx;
    AvpArena *a = avp_arena();
    const ShsmRepo *repo = shsm_repo_find(opt_str(a, args, "root"));
    Sibling *sb = sibling_open(repo, opt_str(a, args, "name"));
    AvpStr cmd = avp_args_str(args, "cmd");
    if ((avp_str_eqz(cmd, "index") && sibling_elf_is_foreign(opt_str(a, args, "elf"))) || sibling_is_foreign(sb)) {
        ShsmOutput *fo = shsm_output_open(args, -1, -1, -1); /* a foreign-architecture sibling: sibling_foreign.c */
        int frc = sibling_foreign_run(sb, cmd, args, fo);
        shsm_output_close(fo);
        sibling_close(sb);
        return frc;
    }
    int jobs = avp_args_is_none(args, "jobs") ? 0 : (int)avp_args_int(args, "jobs");
    const char *elf = opt_str(a, args, "elf"), *map = opt_str(a, args, "map");
    if (!elf) elf = shsm_repo_shared_path(a, repo, "orig/_ref/gormiti_wii/Exe/GormitiFinal.elf");
    if (!map) map = shsm_repo_shared_path(a, repo, "orig/_ref/gormiti_wii/Exe/MAP/GormitiFinal.MAP");
    SibArgs sa = {0};
    bool is_apply = avp_str_eqz(cmd, "apply");
    sa.lib = is_apply ? NULL : opt_str(a, args, "lib");
    sa.tiers = parse_tiers(opt_str(a, args, "tier"));
    sa.target = opt_str(a, args, "target");
    sa.all = avp_args_has(args, "all") && avp_args_bool(args, "all");
    sa.no_splits = avp_args_has(args, "no_splits") && avp_args_bool(args, "no_splits");
    ShsmOutput *out = shsm_output_open(args, -1, -1, -1);
    int rc = 0;
    if (sibling_dwarf_is_cmd(cmd)) { /* the DWARF readers need no pairing run */
        rc = sibling_dwarf_run(sb, cmd, args, out);
        goto done;
    }
    bool is_pair = avp_str_eqz(cmd, "pair");
    if (avp_str_eqz(cmd, "index") || (is_pair && avp_args_bool(args, "rebuild"))) {
        sibling_index(sb, elf, map, jobs, out);
        if (!is_pair) goto done;
    }
    if (!sibling_has_index(sb)) {
        if (is_pair || !sibling_has_pairs(sb)) sibling_index(sb, elf, map, jobs, NULL);
    }
    SibFnVec g = sibling_load_sibling(a, sb), s;
    if (is_pair || !sibling_has_pairs(sb)) {
        s = sibling_load_shsm_asm(a, repo, NULL, jobs);
        SibPairCounts c;
        sibling_pair(&s, &g, &c);
        sibling_store_pairs(sb, &s, &g);
        if (is_pair) {
            rc = sibling_cmd_pair_report(sb, &s, &g, &c, &sa, out);
            goto done;
        }
    } else {
        s = sibling_load_shsm(a, sb);
        sibling_link_peers(&s, &g);
    }
    if (is_apply) {
        SibApplyArgs aa = {.base = sa, .all = sa.all};
        aa.dry_run = avp_args_bool(args, "dry_run");
        if (avp_args_has(args, "libs") && !avp_args_is_none(args, "libs")) aa.libs = avp_args_strs(args, "libs");
        rc = sibling_cmd_apply(sb, &s, &g, &aa, out);
    } else if (avp_str_eqz(cmd, "of")) rc = sibling_cmd_of(sb, &s, &g, &sa, out);
    else if (avp_str_eqz(cmd, "names")) rc = sibling_cmd_names(sb, &s, &g, &sa, out);
    else if (avp_str_eqz(cmd, "units")) rc = sibling_cmd_units(sb, &s, &g, &sa, out);
    else rc = sibling_cmd_stats(sb, &s, &g, &sa, out);
done:
    shsm_output_close(out);
    sibling_close(sb);
    return rc;
}

int sibling_main(int argc, char **argv) {
    AvpArgParser *ap = shsm_cli_parser("sibling.exe", DESCRIPTION, true, true, true);
    avp_arg(ap, &(AvpArgDesc){.names = "-j --jobs", .type = AVP_T_INT, .dflt = AVP_DEF_NONE,
                              .help = "parser threads (default: CPU count)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--name", .metavar = "NAME", .dflt = AVP_DEF_NONE,
                              .help = "which sibling database (default gormiti)"});
    AvpArgSubparsers *sub = avp_args_subparsers(ap, &(AvpArgSubparsersDesc){.dest = "cmd", .required = true});
    AvpArgParser *ix = avp_args_subparser(sub, "index", &(AvpArgParserDesc){.help = "build the sibling database"});
    AvpArgParser *pr = avp_args_subparser(sub, "pair", &(AvpArgParserDesc){.help = "pair the retail functions"});
    AvpArgParser *of = avp_args_subparser(sub, "of", &(AvpArgParserDesc){.help = "the counterpart of one function"});
    AvpArgParser *nm = avp_args_subparser(sub, "names", &(AvpArgParserDesc){.help = "proposed renames"});
    AvpArgParser *un = avp_args_subparser(sub, "units", &(AvpArgParserDesc){.help = "unit boundaries implied"});
    AvpArgParser *st = avp_args_subparser(sub, "stats", &(AvpArgParserDesc){.help = "coverage and precision"});
    AvpArgParser *ap2 = avp_args_subparser(sub, "apply", &(AvpArgParserDesc){.help = "rename the placeholder functions in bulk"});
    AvpArgParser *withfiles[] = {ix, pr};
    for (int i = 0; i < 2; i++) {
        avp_arg(withfiles[i], &(AvpArgDesc){.names = "--elf", .metavar = "PATH", .dflt = AVP_DEF_NONE,
                                            .help = "sibling ELF (default orig/_ref/gormiti_wii/Exe/GormitiFinal.elf)"});
        avp_arg(withfiles[i], &(AvpArgDesc){.names = "--map", .metavar = "PATH", .dflt = AVP_DEF_NONE,
                                            .help = "its CodeWarrior map (default .../Exe/MAP/GormitiFinal.MAP)"});
    }
    avp_arg(pr, &(AvpArgDesc){.names = "--rebuild", .action = AVP_ARG_STORE_TRUE,
                              .help = "re-index the sibling from --elf/--map first"});
    avp_arg(of, &(AvpArgDesc){.names = "target", .help = "retail symbol or address (0x8032C1E0, fn_8032C1E0), or a sibling function name"});
    AvpArgParser *withlib[] = {pr, nm, un, st};
    for (int i = 0; i < 4; i++)
        avp_arg(withlib[i], &(AvpArgDesc){.names = "--lib", .metavar = "L", .dflt = AVP_DEF_NONE,
                                          .help = "only this retail library (libraries.tsv)"});
    AvpArgParser *withtier[] = {nm, un, st};
    for (int i = 0; i < 3; i++)
        avp_arg(withtier[i], &(AvpArgDesc){.names = "--tier", .metavar = "T", .dflt = AVP_DEF_NONE,
                                           .help = "comma list of exact, exact-seq, calls, seq-size, or all"
                                                   " (default exact,exact-seq)"});
    avp_arg(un, &(AvpArgDesc){.names = "--no-splits", .action = AVP_ARG_STORE_TRUE,
                              .help = "propose unit ranges without the split units as evidence (what stats measures)"});
    avp_arg(ap2, &(AvpArgDesc){.names = "--lib", .dest = "libs", .action = AVP_ARG_APPEND, .metavar = "L",
                               .dflt = AVP_DEF_NONE, .help = "only this retail library (libraries.tsv); repeatable"});
    avp_arg(ap2, &(AvpArgDesc){.names = "--tier", .metavar = "T", .dflt = AVP_DEF_NONE,
                               .help = "comma list of exact, exact-seq, calls, seq-size, or all (default exact,exact-seq)"});
    avp_arg(ap2, &(AvpArgDesc){.names = "--dry-run", .action = AVP_ARG_STORE_TRUE,
                               .help = "print the summary and the first examples of each class; write nothing"});
    AvpArgParser *every[] = {ix, pr, of, nm, un, st, ap2};
    for (int i = 0; i < 7; i++) add_shared_options(every[i]);
    sibling_dwarf_add_parsers(sub, add_shared_options);
    return shsm_cli_main(ap, argc, argv, run, NULL);
}
