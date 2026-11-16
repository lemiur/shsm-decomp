/* args.h internals shared by args.c (building and parsing) and args_format.c (usage, help, textwrap, repr). */
#pragma once
#include "avp/args.h"

/* nargs inside the library: AVP_NARGS_* values for the special forms, ARGN_NONE for nargs=None, and 0, 1, 2, ... for
 * a number (flag actions have 0). */
#define ARGN_NONE (-100)

/* kinds beyond the public AvpArgActionKind */
#define ARGK_SUBPARSERS 100 /* the action add_subparsers creates (nargs PARSER, choices = the sub-parsers) */
#define ARGK_PSEUDO 101     /* the help-only entry of one sub-command in the command list */

typedef AVP_VEC(const char *) CzVec;
typedef AVP_VEC(AvpArgAction *) ActionVec;

struct AvpArgAction {
    AvpArgParser *parser;
    size_t index;            /* position in parser->actions */
    int kind;
    size_t nopts;
    const char **opts;       /* option_strings, in order given; nopts == 0 for a positional */
    const char *dest;        /* NULL: argparse.SUPPRESS */
    int nargs;
    AvpArgType type;
    AvpArgConv conv;
    void *conv_ctx;
    const char *type_name;   /* shown in "invalid <type> value" and by MetavarTypeHelpFormatter */
    const char *metavar;
    size_t nmetavars;        /* a metavar tuple */
    const char **metavars;
    const char **choices;    /* NULL when there are none; the sub-commands for ARGK_SUBPARSERS are in sub */
    size_t nchoices;
    AvpArgDefault dflt;      /* resolved: never AVP_DK_UNSET */
    bool has_dflt_list;
    size_t ndflt_list;
    const char **dflt_list;
    AvpArgDefault cnst;
    bool required;
    const char *help;        /* NULL = None */
    bool help_suppress;
    const char *version;
    AvpArgSection *section;
    AvpArgSubparsers *sub;
};

struct AvpArgSection {
    AvpArgParser *parser;
    const char *title;
    const char *description;
    ActionVec actions;       /* _group_actions */
};

struct AvpArgMutex {
    AvpArgParser *parser;
    AvpArgSection *container;
    bool required;
    ActionVec actions;
};

struct AvpArgSubparsers {
    AvpArgParser *owner;
    AvpArgAction *action;
    const char *prog_prefix;
    CzVec names;                       /* the name_parser_map keys, aliases included */
    AVP_VEC(AvpArgParser *) parsers;   /* parallel to names */
    ActionVec pseudo;                  /* _choices_actions: the help lines of the sub-commands */
};

typedef struct {
    const char *str;
    AvpArgAction *action;
} OptionEntry;

struct AvpArgParser {
    AvpArena *a;
    const char *prog;
    const char *usage;
    const char *description;
    const char *epilog;
    AvpArgFormatter formatter;
    bool allow_abbrev;
    ActionVec actions;                          /* _actions */
    AVP_VEC(OptionEntry) option_actions;        /* _option_string_actions in insertion order */
    AVP_VEC(AvpArgSection *) sections;          /* _action_groups */
    AVP_VEC(AvpArgMutex *) mutexes;             /* _mutually_exclusive_groups */
    AvpArgSection *positionals;
    AvpArgSection *optionals;
    AvpArgSubparsers *subparsers;
    bool has_negative_number_optionals;
};

/* ---- args_format.c ---- */
size_t argp_cplen(AvpStr s);                      /* len() of the str: code points */
size_t argp_cplen_z(const char *cz);
void argp_repr(AvpBuf *b, AvpStr s);              /* repr(str) */
void argp_float_repr(AvpBuf *b, double x);        /* repr(float) */
const char *argp_action_name(AvpArena *a, const AvpArgAction *act); /* _get_action_name, NULL for None */
AvpStr argp_format_usage(AvpArgParser *p);        /* parser.format_usage() */
AvpStr argp_format_help(AvpArgParser *p);         /* parser.format_help() */
/* The prog of a sub-parsers action: the usage of the positionals so far, without prefix, stripped. */
const char *argp_subparsers_prog(AvpArgParser *p);
/* The text a version action prints: formatter.add_text(version); formatter.format_help(). */
AvpStr argp_format_version(AvpArgParser *p, const char *version);
/* `s % {...}` for the one name help texts and usage use: replaces %(prog)s and %% (everything else is an error). */
AvpStr argp_percent_prog(AvpArena *a, const char *s, const char *prog);
