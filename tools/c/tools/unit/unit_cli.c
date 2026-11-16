/* unit: the argument parser and main() of tools/unit.py (build_parser, main: lines 2125-2193). */
#include "unit_cmd.h"

#include <stdio.h>
#include <string.h>

static void root_arg(AvpArgParser *p) {
    avp_arg(p, &(AvpArgDesc){.names = "--root", .dflt = AVP_DEF_SUPPRESS});
}

static AvpArgParser *build_parser(void) {
    AvpArgParser *ap = avp_args_parser(
        NULL, &(AvpArgParserDesc){.prog = "unit.exe", .description = "Scaffold, check, flip and park translation units."});
    avp_arg(ap, &(AvpArgDesc){.names = "--root",
                              .help = "repository root (default: the repository containing this file)"});
    AvpArgSubparsers *sub = avp_args_subparsers(ap, &(AvpArgSubparsersDesc){.dest = "cmd", .required = true});
    static const char *const data_choices[] = {"auto", "none", NULL};
    static const char *const empty[] = {NULL};

    AvpArgParser *p = avp_args_subparser(sub, "new", &(AvpArgParserDesc){.help = "scaffold a unit"});
    root_arg(p);
    avp_arg(p, &(AvpArgDesc){.names = "path",
                             .help = "the unit's path under src/: a .c, .cpp, .cp or .s file (a .s path gets an assembly "
                                     "stub with one .fn/.endfn pair per function)"});
    avp_arg(p, &(AvpArgDesc){.names = "--text",
                             .help = "START-END of the unit's .text (hex); omit it for a data-only unit made of the "
                                     "--include-data objects (a *_cfg.c that holds config tables the reader's file "
                                     "does not)"});
    avp_arg(p, &(AvpArgDesc){.names = "--lib"});
    avp_arg(p, &(AvpArgDesc){.names = "--data", .choices = data_choices, .dflt = AVP_DEF_STR("auto")});
    avp_arg(p, &(AvpArgDesc){.names = "--confirmed", .action = AVP_ARG_STORE_TRUE,
                             .help = "the unit name has string evidence: omit the comment"});
    avp_arg(p, &(AvpArgDesc){.names = "--name-evidence", .metavar = "TEXT",
                             .help = "write TEXT as the unit's name comment instead of \"unit name unconfirmed\" (for "
                                     "example \"// Unit name and boundary: Gormiti link map (fmodex.a fmod_file.obj)\"; "
                                     "a .s stub gets it with # in place of //)"});
    avp_arg(p, &(AvpArgDesc){.names = "--include-data", .action = AVP_ARG_APPEND, .dflt_list = empty, .metavar = "SYM",
                             .help = "take this data object (and what only it references) into the unit; repeat"});
    avp_arg(p, &(AvpArgDesc){.names = "--exclude-data", .action = AVP_ARG_APPEND, .dflt_list = empty, .metavar = "SYM",
                             .help = "leave this data object to another unit (then `new <other> --include-data SYM` "
                                     "with no --text makes that unit); objects an existing split holds are left out "
                                     "automatically"});
    avp_arg(p, &(AvpArgDesc){.names = "--allow-no-asm", .action = AVP_ARG_STORE_TRUE,
                             .help = "write the split even when build/R5WEA4/asm has no blocks for the .text range (no "
                                     "data sections are inferred then); without it that case is an error"});
    avp_arg(p, &(AvpArgDesc){.names = "--dry-run", .action = AVP_ARG_STORE_TRUE,
                             .help = "print the plan and write nothing; every candidate data object next to the range is "
                                     "listed as taken (why) or not taken (why, and the --include-data that takes it)"});
    avp_arg(p, &(AvpArgDesc){.names = "--regen", .action = AVP_ARG_STORE_TRUE,
                             .help = "then regenerate build.ninja (tools/bin/ninja.exe build.ninja: dtk split and "
                                     "configure.py) so that the unit table shows the new unit, then delete the "
                                     "auto_*.s asm listings the new split no longer names"});

    p = avp_args_subparser(sub, "resplit", &(AvpArgParserDesc){.help = "re-split a parked (NonMatching) unit"});
    root_arg(p);
    avp_arg(p, &(AvpArgDesc){.names = "path", .help = "an existing NonMatching unit (refused when it is Matching)"});
    avp_arg(p, &(AvpArgDesc){.names = "--text", .help = "the unit's new START-END .text range (hex); omit it for a "
                                                         "data-only unit"});
    avp_arg(p, &(AvpArgDesc){.names = "--data", .choices = data_choices, .dflt = AVP_DEF_STR("auto")});
    avp_arg(p, &(AvpArgDesc){.names = "--include-data", .action = AVP_ARG_APPEND, .dflt_list = empty, .metavar = "SYM",
                             .help = "take this data object (and what only it references) into the unit; repeat"});
    avp_arg(p, &(AvpArgDesc){.names = "--exclude-data", .action = AVP_ARG_APPEND, .dflt_list = empty, .metavar = "SYM",
                             .help = "leave this data object to another unit; repeat"});
    avp_arg(p, &(AvpArgDesc){.names = "--allow-no-asm", .action = AVP_ARG_STORE_TRUE,
                             .help = "write the split even when build/R5WEA4/asm has no blocks for the .text range"});
    avp_arg(p, &(AvpArgDesc){.names = "--dry-run", .action = AVP_ARG_STORE_TRUE,
                             .help = "print the plan (data taken or not taken, with the reasons) and write nothing"});
    avp_arg(p, &(AvpArgDesc){.names = "--regen", .action = AVP_ARG_STORE_TRUE,
                             .help = "then regenerate build.ninja so the unit table shows the new split"});

    p = avp_args_subparser(sub, "check", &(AvpArgParserDesc){.help = "static checks of a unit"});
    root_arg(p);
    avp_arg(p, &(AvpArgDesc){.names = "path"});

    p = avp_args_subparser(sub, "flip", &(AvpArgParserDesc){.help = "set Matching after gate.exe --pending-flip passes"});
    root_arg(p);
    avp_arg(p, &(AvpArgDesc){.names = "path"});
    avp_arg(p, &(AvpArgDesc){.names = "--dry-run", .action = AVP_ARG_STORE_TRUE});
    avp_arg(p, &(AvpArgDesc){.names = "--gate-cmd",
                             .help = "replace gate.exe with this command (run through the shell; not a gate.exe "
                                     "result)"});
    avp_arg(p, &(AvpArgDesc){.names = "--rules-cmd",
                             .help = "with --gate-cmd: replace the rule checker command (run through the shell)"});
    avp_arg(p, &(AvpArgDesc){.names = "--ignore-untracked", .action = AVP_ARG_STORE_TRUE,
                             .help = "with --gate-cmd: do not count untracked files as a dirty tree"});

    p = avp_args_subparser(sub, "park", &(AvpArgParserDesc){.help = "set NonMatching"});
    root_arg(p);
    avp_arg(p, &(AvpArgDesc){.names = "path"});
    return ap;
}

static const char *opt(AvpArena *a, const AvpArgs *r, const char *dest) {
    if (!avp_args_has(r, dest) || avp_args_is_none(r, dest)) return NULL;
    return avp_cz(a, avp_args_str(r, dest));
}

static bool flag(const AvpArgs *r, const char *dest) { return avp_args_has(r, dest) && avp_args_bool(r, dest); }

/* The default --root: the checkout this executable belongs to (Python: the repository unit.py is in). */
static const char *default_root(AvpArena *a) {
    const char *r = avp_root();
    return r ? avp_strdup(a, r) : avp_getcwd(a);
}

int unit_main(int argc, char **argv) {
    AvpArena *a = avp_arena();
    AvpArgs *r = avp_args_parse(build_parser(), argc, argv);
    const char *cmd = opt(a, r, "cmd");
    const char *root = opt(a, r, "root");
    if (!root) root = default_root(a);
    AvpTry t;
    if (AVP_TRY(&t)) {
        UnitProject *p = unit_project_new(a, root);
        int rc;
        if (!strcmp(cmd, "new") || !strcmp(cmd, "resplit")) {
            bool re = !strcmp(cmd, "resplit");
            UnitNewArgs n = {0};
            n.path = opt(a, r, "path");
            n.text = opt(a, r, "text");
            n.lib = re ? NULL : opt(a, r, "lib");
            n.data = opt(a, r, "data");
            n.confirmed = re ? false : flag(r, "confirmed");
            n.name_evidence = re ? NULL : opt(a, r, "name_evidence");
            n.allow_no_asm = flag(r, "allow_no_asm");
            n.dry_run = flag(r, "dry_run");
            n.regen = flag(r, "regen");
            n.include_data = avp_args_strs(r, "include_data");
            n.exclude_data = avp_args_strs(r, "exclude_data");
            rc = re ? unit_cmd_resplit(p, &n) : unit_cmd_new(p, &n);
        } else if (!strcmp(cmd, "check")) {
            rc = unit_cmd_check(p, opt(a, r, "path"));
        } else if (!strcmp(cmd, "park")) {
            rc = unit_cmd_park(p, opt(a, r, "path"));
        } else {
            UnitFlipArgs f = {0};
            f.path = opt(a, r, "path");
            f.dry_run = flag(r, "dry_run");
            f.ignore_untracked = flag(r, "ignore_untracked");
            f.gate_cmd = opt(a, r, "gate_cmd");
            f.rules_cmd = opt(a, r, "rules_cmd");
            rc = unit_cmd_flip(p, &f);
        }
        avp_try_end(&t);
        return rc;
    }
    fflush(stdout);
    if (t.code == UNIT_TOOL_ERROR) {
        fprintf(stderr, "error: %s\n", unit_error_message());
        return 2;
    }
    /* an exception unit.py does not catch: Python prints a traceback and exits 1 */
    fprintf(stderr, "unit.exe: error: %s\n", t.msg);
    return 1;
}
