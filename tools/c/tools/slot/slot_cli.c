/* slot: the argument parser and main() of tools/slot.py (argparse, then Refusal / ToolError / OSError handling). */
#include "slot.h"

#include <stdio.h>
#include <string.h>

static bool is_checkout(AvpArena *a, const char *p) {
    return avp_isfile(avp_join(a, p, "configure.py")) && avp_isdir(avp_join(a, p, "config")) &&
           avp_isdir(avp_join(a, p, "tools"));
}

/* --repo's default: the checkout this tool belongs to (Python: the repository slot.py is in). The executable lives
 * in <checkout>/build/tools/; a build elsewhere falls back to the checkout holding the current folder. */
static const char *default_repo(AvpArena *a) {
    const char *own = avp_path_abspath(a, avp_join(a, avp_exe_dir(), "..", ".."));
    if (is_checkout(a, own)) return own;
    for (const char *p = avp_getcwd(a);;) {
        if (is_checkout(a, p)) return p;
        const char *parent = avp_cz(a, avp_path_dirname(p));
        if (!strcmp(parent, p)) break;
        p = parent;
    }
    return avp_getcwd(a);
}

static AvpArgParser *build_parser(AvpArena *a) {
    AvpArgParser *ap = avp_args_parser(
        NULL, &(AvpArgParserDesc){.prog = "slot.exe", .description = "Worktree slot manager (see module docstring)."});
    avp_arg(ap, &(AvpArgDesc){.names = "--repo", .dflt = AVP_DEF_STR(default_repo(a))});
    avp_arg(ap, &(AvpArgDesc){.names = "--base", .dflt = AVP_DEF_STR(avp_getenv_or(a, "SHSM_WT_BASE", "E:/shsm-wt"))});
    avp_arg(ap, &(AvpArgDesc){.names = "--debug", .action = AVP_ARG_STORE_TRUE});
    AvpArgSubparsers *sub = avp_args_subparsers(ap, &(AvpArgSubparsersDesc){.dest = "cmd", .required = true});
    avp_args_subparser(sub, "list", NULL);
    AvpArgParser *s = avp_args_subparser(sub, "new", NULL);
    avp_arg(s, &(AvpArgDesc){.names = "slot"});
    avp_arg(s, &(AvpArgDesc){.names = "--from", .dest = "from_rev"});
    avp_arg(s, &(AvpArgDesc){.names = "--branch"});
    s = avp_args_subparser(sub, "check", NULL);
    avp_arg(s, &(AvpArgDesc){.names = "slot", .nargs = AVP_NARGS_OPT});
    avp_arg(s, &(AvpArgDesc){.names = "--fix", .action = AVP_ARG_STORE_TRUE,
                             .help = "create the links to the main checkout's shared inputs that a slot lacks (as `link`), then check"});
    s = avp_args_subparser(sub, "link", NULL);
    avp_arg(s, &(AvpArgDesc){.names = "slot"});
    static const char *const rr[] = {"reset", "remove"};
    for (size_t i = 0; i < 2; i++) {
        s = avp_args_subparser(sub, rr[i], NULL);
        avp_arg(s, &(AvpArgDesc){.names = "slot"});
        avp_arg(s, &(AvpArgDesc){.names = "--force", .action = AVP_ARG_STORE_TRUE});
        avp_arg(s, &(AvpArgDesc){.names = "--owner", .help = "your lock owner name (your own lock does not block)"});
        if (i == 0) avp_arg(s, &(AvpArgDesc){.names = "--to"});
    }
    s = avp_args_subparser(sub, "lock", NULL);
    avp_arg(s, &(AvpArgDesc){.names = "slot"});
    avp_arg(s, &(AvpArgDesc){.names = "--owner", .required = true});
    avp_arg(s, &(AvpArgDesc){.names = "--lane"});
    avp_arg(s, &(AvpArgDesc){.names = "--ttl", .type = AVP_T_FLOAT, .dflt = AVP_DEF_FLOAT(SLOT_DEFAULT_TTL_HOURS),
                             .help = "hours until the lock counts as stale"});
    avp_arg(s, &(AvpArgDesc){.names = "--force", .action = AVP_ARG_STORE_TRUE});
    s = avp_args_subparser(sub, "unlock", NULL);
    avp_arg(s, &(AvpArgDesc){.names = "slot"});
    avp_arg(s, &(AvpArgDesc){.names = "--owner"});
    avp_arg(s, &(AvpArgDesc){.names = "--force", .action = AVP_ARG_STORE_TRUE});
    avp_args_subparser(sub, "who", NULL);
    s = avp_args_subparser(sub, "conflicts", NULL);
    avp_arg(s, &(AvpArgDesc){.names = "branches", .nargs = AVP_NARGS_STAR});
    avp_arg(s, &(AvpArgDesc){.names = "--onto", .dflt = AVP_DEF_STR(SLOT_MAIN_REF)});
    return ap;
}

/* args.<dest> as a C string, NULL for None or an attribute the subcommand does not have. */
static const char *opt_str(AvpArena *a, const AvpArgs *r, const char *dest) {
    if (!avp_args_has(r, dest) || avp_args_is_none(r, dest)) return NULL;
    return avp_cz(a, avp_args_str(r, dest));
}

static bool opt_bool(const AvpArgs *r, const char *dest) { return avp_args_has(r, dest) && avp_args_bool(r, dest); }

typedef int (*SlotCmd)(AvpArena *a, SlotRepo *r, const SlotArgs *args);
static const struct {
    const char *name;
    SlotCmd fn;
} COMMANDS[] = {
    {"list", slot_cmd_list},       {"new", slot_cmd_new},       {"check", slot_cmd_check}, {"link", slot_cmd_link},
    {"reset", slot_cmd_reset},     {"remove", slot_cmd_remove}, {"lock", slot_cmd_lock},   {"unlock", slot_cmd_unlock},
    {"who", slot_cmd_who},         {"conflicts", slot_cmd_conflicts},
};

/* An exception slot.py did not catch (or re-raised under --debug): a traceback on stderr, exit code 1. */
static int traceback(const char *cls, const char *msg) {
    fflush(stdout);
    fprintf(stderr, "slot.exe: error: ");
    if (cls) fprintf(stderr, "%s: %s\n", cls, msg);
    else fprintf(stderr, "%s\n", msg);
    return 1;
}

int slot_main(int argc, char **argv) {
    AvpArena *a = avp_arena();
    AvpArgParser *ap = build_parser(a);
    AvpArgs *r = avp_args_parse(ap, argc, argv);
    SlotArgs args = {0};
    args.slot = opt_str(a, r, "slot");
    args.from_rev = opt_str(a, r, "from_rev");
    args.branch = opt_str(a, r, "branch");
    args.to = opt_str(a, r, "to");
    args.owner = opt_str(a, r, "owner");
    args.lane = opt_str(a, r, "lane");
    args.onto = opt_str(a, r, "onto");
    args.ttl = avp_args_has(r, "ttl") ? avp_args_float(r, "ttl") : SLOT_DEFAULT_TTL_HOURS;
    args.force = opt_bool(r, "force");
    args.fix = opt_bool(r, "fix");
    if (avp_args_has(r, "branches")) args.branches = avp_args_strs(r, "branches");
    bool debug = avp_args_bool(r, "debug");
    const char *cmd = opt_str(a, r, "cmd");
    SlotCmd fn = NULL;
    for (size_t i = 0; i < sizeof COMMANDS / sizeof COMMANDS[0]; i++)
        if (!strcmp(COMMANDS[i].name, cmd)) fn = COMMANDS[i].fn;
    AVP_ASSERT(fn);

    AvpTry t;
    if (AVP_TRY(&t)) {
        SlotRepo *repo = slot_repo_open(a, opt_str(a, r, "repo"), opt_str(a, r, "base"));
        int rc = fn(a, repo, &args);
        avp_try_end(&t);
        return rc;
    }
    if (t.code == SLOT_REFUSAL) {
        printf("REFUSED: %s\n", slot_error_message());
        return 1;
    }
    if (t.code == SLOT_UNCAUGHT) return traceback(NULL, slot_error_message());
    const char *msg = t.code == SLOT_TOOL_ERROR ? slot_error_message() : t.msg;
    if (t.code == SLOT_TOOL_ERROR || t.code == SHSM_EXC_USAGE || shsm_exc_is_os(t.code)) {
        if (debug) return traceback(t.code == SLOT_TOOL_ERROR ? "ToolError" : shsm_exc_class(t.code), msg);
        fflush(stdout);
        fprintf(stderr, "error: %s\n", msg);
        return 2;
    }
    return traceback(shsm_exc_class(t.code), msg);
}
