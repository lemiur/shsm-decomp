/* names.py build_parser() and run(): every subcommand takes the shared options of cli.parser("") (argparse
 * parents=[common], conflict_handler="resolve") with their real defaults, so a value given after the subcommand, or
 * its default, replaces one given before it. */
#include "names.h"

#include <string.h>

static const char DESCRIPTION[] =
    "Names: demangle, mangle, find symbols, rename one symbol or a class consistently, check names.";

static bool limit_conv(void *ctx, AvpArena *a, AvpStr in, AvpArgValue *out, AvpStr *msg) {
    (void)ctx;
    i64 n = 0;
    if (!shsm_limit_type(a, in, &n, msg)) return false;
    out->kind = AVP_V_INT;
    out->i = n;
    return true;
}

static void add_common(AvpArgParser *p) {
    avp_arg(p, &(AvpArgDesc){.names = "--root", .metavar = "DIR", .dflt = AVP_DEF_NONE,
                             .help = "checkout to work on (default: the one around the cwd)"});
    avp_arg(p, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE,
                             .help = "print one JSON document instead of text"});
    avp_arg(p, &(AvpArgDesc){.names = "--all", .action = AVP_ARG_STORE_TRUE, .help = "do not cap the output"});
    avp_arg(p, &(AvpArgDesc){.names = "--limit", .type = AVP_T_CUSTOM, .conv = limit_conv, .type_name = "limit_type",
                             .dflt = AVP_DEF_INT(SHSM_DEFAULT_LIMIT), .metavar = "N",
                             .help = "cap text output at N lines (default 40; 0 or 'all': no cap)"});
    avp_arg(p, &(AvpArgDesc){.names = "--debug", .action = AVP_ARG_STORE_TRUE, .help = "print tracebacks"});
}

static AvpArgParser *add(AvpArgSubparsers *sub, const char *name, const char *help) {
    AvpArgParser *p = avp_args_subparser(sub, name, &(AvpArgParserDesc){.help = help, .description = help});
    add_common(p);
    return p;
}

static const char *opt_cz(AvpArena *a, const AvpArgs *args, const char *dest) {
    if (!avp_args_has(args, dest) || avp_args_is_none(args, dest)) return NULL;
    return avp_cz(a, avp_args_str(args, dest));
}

static bool opt_bool(const AvpArgs *args, const char *dest) {
    return avp_args_has(args, dest) && avp_args_bool(args, dest);
}

static int run(void *ctx, AvpArgs *args) {
    (void)ctx;
    AvpArena *a = avp_arena();
    NamesArgs na = {0};
    na.cmd = avp_cz(a, avp_args_str(args, "cmd"));
    if (strcmp(na.cmd, "demangle") == 0) {
        na.names = avp_args_strs(args, "names");
    } else if (strcmp(na.cmd, "mangle") == 0) {
        na.name = opt_cz(a, args, "name");
        na.cls = opt_cz(a, args, "cls");
        na.params = opt_cz(a, args, "params");
        na.is_const = opt_bool(args, "const");
    } else if (strcmp(na.cmd, "find") == 0) {
        na.text = opt_cz(a, args, "text");
    } else if (strcmp(na.cmd, "attr") == 0) {
        na.attr.symbol = opt_cz(a, args, "symbol");
        na.attr.scope = opt_cz(a, args, "scope");
        na.attr.has_size = avp_args_has(args, "size") && !avp_args_is_none(args, "size");
        if (na.attr.has_size) na.attr.size = avp_args_int(args, "size");
        na.attr.has_align = avp_args_has(args, "align") && !avp_args_is_none(args, "align");
        if (na.attr.has_align) na.attr.align = avp_args_int(args, "align");
        na.attr.resplit = opt_bool(args, "resplit");
        na.apply = opt_bool(args, "apply");
    } else if (strcmp(na.cmd, "add") == 0) {
        na.add.name = opt_cz(a, args, "name");
        na.add.section = opt_cz(a, args, "section");
        na.add.address = opt_cz(a, args, "address");
        na.add.size = opt_cz(a, args, "size");
        na.add.type = opt_cz(a, args, "type");
        na.add.scope = opt_cz(a, args, "scope");
        na.add.has_align = avp_args_has(args, "align") && !avp_args_is_none(args, "align");
        if (na.add.has_align) na.add.align = avp_args_int(args, "align");
        na.add.data = opt_cz(a, args, "data");
        na.apply = opt_bool(args, "apply");
    } else if (strcmp(na.cmd, "rename") == 0) {
        na.cls_rename = opt_bool(args, "cls");
        na.old = opt_cz(a, args, "old");
        na.new_ = opt_cz(a, args, "new");
        na.apply = opt_bool(args, "apply");
        na.local = opt_bool(args, "local");
        na.symbols_only = opt_bool(args, "symbols_only");
        na.signature = opt_bool(args, "signature");
        na.family = opt_bool(args, "family");
    }
    const ShsmRepo *repo = shsm_repo_find(opt_cz(a, args, "root"));
    ShsmOutput *out = shsm_output_open(args, -1, -1, -1);
    int rc;
    if (strcmp(na.cmd, "demangle") == 0) rc = names_cmd_demangle(&na, out);
    else if (strcmp(na.cmd, "mangle") == 0) rc = names_cmd_mangle(&na, out);
    else if (strcmp(na.cmd, "find") == 0) rc = names_cmd_find(&na, out, repo);
    else if (strcmp(na.cmd, "rename") == 0) rc = names_cmd_rename(&na, out, repo);
    else if (strcmp(na.cmd, "attr") == 0) rc = names_cmd_attr(&na, out, repo);
    else if (strcmp(na.cmd, "add") == 0) rc = names_cmd_add(&na, out, repo);
    else rc = names_cmd_check(&na, out, repo);
    shsm_output_close(out);
    return rc;
}

int names_main(int argc, char **argv) {
    AvpArgParser *ap = shsm_cli_parser("names.exe", DESCRIPTION, false, false, false);
    AvpArgSubparsers *sub = avp_args_subparsers(ap, &(AvpArgSubparsersDesc){.dest = "cmd", .required = true});
    AvpArgParser *p = add(sub, "demangle", "demangle CodeWarrior names");
    avp_arg(p, &(AvpArgDesc){.names = "names", .nargs = AVP_NARGS_PLUS});
    p = add(sub, "mangle", "mangle a function name");
    avp_arg(p, &(AvpArgDesc){.names = "--name", .required = true,
                             .help = "function name (the class name for a constructor, ~Class)"});
    avp_arg(p, &(AvpArgDesc){.names = "--class", .dest = "cls", .help = "enclosing scope, A::B for nested"});
    avp_arg(p, &(AvpArgDesc){.names = "--params", .dflt = AVP_DEF_STR(""),
                             .help = "parameter list, e.g. \"const char*, int\""});
    avp_arg(p, &(AvpArgDesc){.names = "--const", .action = AVP_ARG_STORE_TRUE, .help = "const member function"});
    p = add(sub, "find", "search symbols.txt by substring of the raw or demangled name");
    avp_arg(p, &(AvpArgDesc){.names = "text"});
    p = add(sub, "rename", "rename a symbol (or a class with --class) everywhere it must change together");
    avp_arg(p, &(AvpArgDesc){.names = "--class", .dest = "cls", .action = AVP_ARG_STORE_TRUE,
                             .help = "rename a class and all its member symbols"});
    avp_arg(p, &(AvpArgDesc){.names = "old"});
    avp_arg(p, &(AvpArgDesc){.names = "new"});
    avp_arg(p, &(AvpArgDesc){.names = "--apply", .action = AVP_ARG_STORE_TRUE,
                             .help = "write the edits (default: dry run)"});
    avp_arg(p, &(AvpArgDesc){.names = "--local", .action = AVP_ARG_STORE_TRUE,
                             .help = "the new name may exist as scope:local symbols (a per-unit static); the line "
                                     "becomes local"});
    avp_arg(p, &(AvpArgDesc){.names = "--symbols-only", .dest = "symbols_only", .action = AVP_ARG_STORE_TRUE,
                             .help = "edit only the symbols.txt lines (no source, header, doc or config edit); any "
                                     "symbol name"});
    avp_arg(p, &(AvpArgDesc){.names = "--signature", .action = AVP_ARG_STORE_TRUE,
                             .help = "the new name is a full mangled name that may change the class or the parameter "
                                     "codes; symbols.txt is rewritten, source only where the plain name changed, and the "
                                     "declarations and definitions under the old signature are listed to edit by hand"});
    avp_arg(p, &(AvpArgDesc){.names = "--family", .action = AVP_ARG_STORE_TRUE,
                             .help = "the same change for every symbol with the same plain name (the overrides of one "
                                     "virtual slot), each keeping its class; with --signature, old and new give the "
                                     "change for one member"});
    p = add(sub, "attr", "change the scope, size or align of one symbols.txt line");
    avp_arg(p, &(AvpArgDesc){.names = "symbol", .help = "a symbol name, or its address 0x8XXXXXXX when several symbols share the name"});
    avp_arg(p, &(AvpArgDesc){.names = "--scope", .choices = (const char *const[]){"global", "local", "weak", NULL},
                             .dflt = AVP_DEF_NONE, .help = "the new scope (local refuses a second local of the name in the unit)"});
    avp_arg(p, &(AvpArgDesc){.names = "--size", .type = AVP_T_INT0, .dflt = AVP_DEF_NONE, .metavar = "N",
                             .help = "the new size in bytes; it refuses to grow into the next symbol, and a size that crosses or falls short of the unit's section end (never edited on its own: use unit.exe resplit, or --resplit)"});
    avp_arg(p, &(AvpArgDesc){.names = "--resplit", .action = AVP_ARG_STORE_TRUE,
                             .help = "let --size move the unit's section end in splits.txt (listed in the dry run)"});
    avp_arg(p, &(AvpArgDesc){.names = "--align", .type = AVP_T_INT0, .dflt = AVP_DEF_NONE, .metavar = "N",
                             .help = "the new alignment (a power of two)"});
    avp_arg(p, &(AvpArgDesc){.names = "--apply", .action = AVP_ARG_STORE_TRUE,
                             .help = "write the edits (default: dry run)"});
    p = add(sub, "add", "add a symbols.txt line for a symbol that has none (an object left without a line by a resize)");
    avp_arg(p, &(AvpArgDesc){.names = "name", .help = "the symbol name"});
    avp_arg(p, &(AvpArgDesc){.names = "--section", .required = true, .help = "the section (.bss, .data, .text ...)"});
    avp_arg(p, &(AvpArgDesc){.names = "--address", .required = true, .metavar = "0x8XXXXXXX", .help = "the address"});
    avp_arg(p, &(AvpArgDesc){.names = "--size", .required = true, .metavar = "0xN", .help = "the size in bytes"});
    avp_arg(p, &(AvpArgDesc){.names = "--type", .choices = (const char *const[]){"object", "function", NULL},
                             .dflt = AVP_DEF_NONE, .help = "the symbol type (default object)"});
    avp_arg(p, &(AvpArgDesc){.names = "--scope", .choices = (const char *const[]){"global", "local", "weak", NULL},
                             .dflt = AVP_DEF_NONE, .help = "the scope token (default none)"});
    avp_arg(p, &(AvpArgDesc){.names = "--align", .type = AVP_T_INT0, .dflt = AVP_DEF_NONE, .metavar = "N",
                             .help = "the alignment (a power of two)"});
    avp_arg(p, &(AvpArgDesc){.names = "--data", .dflt = AVP_DEF_NONE, .help = "the data token (4byte, string ...)"});
    avp_arg(p, &(AvpArgDesc){.names = "--apply", .action = AVP_ARG_STORE_TRUE,
                             .help = "write the edit (default: dry run)"});
    add(sub, "check", "names in symbols.txt against source objects and address comments");
    return shsm_cli_main(ap, argc, argv, run, NULL);
}
