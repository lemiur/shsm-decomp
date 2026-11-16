/* Python's argparse (CPython 3.14.8) for command-line tools: a declarative parser that parses, converts, checks and
 * reports exactly as argparse does, and prints argparse's usage and help text byte for byte (without colour: Python
 * only colours a terminal). It covers what the project's tools use and the rest of the common surface:
 *   - options and positionals; actions store, store_const, store_true, store_false, append, append_const, extend,
 *     count, help, version, BooleanOptionalAction; nargs None, N, '?', '*', '+', REMAINDER ('...'), PARSER;
 *   - types str, int, int(v, 0), float, identity (Path), or a converter function; choices (strings); defaults
 *     (a string default goes through the type, as argparse does); const; dest; metavar (a name or a tuple);
 *     required options; help with %(default)s / %(default)r / %(prog)s / %(dest)s / %(choices)s / %%;
 *   - subparsers (add_subparsers(dest=, required=) / add_parser(name, help=, aliases=)), mutually exclusive groups
 *     (required or not), argument groups (title, description);
 *   - parse_args / parse_known_args semantics: the positional/optional interleaving and the greedy matching of
 *     nargs (the regular-expression matching of argparse, done by a small backtracking matcher), long-option
 *     prefix abbreviation (allow_abbrev), --opt=value, attached short values (-j8), short flag clusters (-abc),
 *     the negative-number rule, `--`, "ambiguous option", "unrecognized arguments", "expected one argument",
 *     "invalid int value: 'x'", "invalid choice: 'x' (choose from 'a', 'b')", "the following arguments are
 *     required: ...", "argument --x: not allowed with argument --y", "one of the arguments --a --b is required";
 *   - formatters HelpFormatter, RawDescriptionHelpFormatter, RawTextHelpFormatter, ArgumentDefaultsHelpFormatter,
 *     MetavarTypeHelpFormatter; usage wrapping and help wrapping (textwrap.wrap with hyphen breaking) at the width
 *     argparse uses: the COLUMNS environment variable if it is a positive integer, else the console width of
 *     stdout, else 80, minus 2.
 * An error prints argparse's usage and "<prog>: error: <message>" on stderr and exits with code 2; -h prints the
 * help on stdout and exits 0. prog is settable (the tools pass "gate.py"-style names so the messages stay the same).
 *
 *     static const char *const builds[] = {"dx9", "dx11", "both", NULL};
 *     AvpArgParser *p = avp_args_parser(NULL, &(AvpArgParserDesc){
 *         .prog = "gate.py", .description = "Run the gate.", .formatter = AVP_FORMAT_RAW_DESCRIPTION});
 *     avp_arg(p, &(AvpArgDesc){.names = "-j --jobs", .type = AVP_T_INT, .dflt = AVP_DEF_INT(4), .metavar = "N"});
 *     avp_arg(p, &(AvpArgDesc){.names = "--build", .choices = builds, .dflt = AVP_DEF_STR("both")});
 *     avp_arg(p, &(AvpArgDesc){.names = "units", .nargs = AVP_NARGS_STAR});
 *     AvpArgs *r = avp_args_parse(p, argc, argv);                 // argv[0] is the program: parse_args(argv[1:])
 *     i64 jobs = avp_args_int(r, "jobs");
 *     AvpStrVec units = avp_args_strs(r, "units");
 *
 * Differences from Python: an int is 64-bit (a bigger one is "invalid int value"); choices are strings, so a
 * choices list on a typed argument (int, float) is rejected when the parser is built; the repr of a str in messages
 * escapes the common non-printable code points only (C0/C1 controls, NBSP, soft hyphen and the Unicode space and
 * format ranges), not the whole Unicode printable table; fromfile_prefix_chars, parents, conflict_handler='resolve',
 * argument_default, set_defaults, FileType, parse_intermixed_args, suggest_on_error, deprecated and prefix_chars other
 * than "-" are not supported. A mistake in the declaration (a duplicate option string, a bad nargs) is an internal
 * error (avp_panic), where Python raised ValueError at add_argument. */
#pragma once
#include "str.h"

typedef struct AvpArgParser AvpArgParser;
typedef struct AvpArgAction AvpArgAction;
typedef struct AvpArgSection AvpArgSection;     /* add_argument_group */
typedef struct AvpArgMutex AvpArgMutex;         /* add_mutually_exclusive_group */
typedef struct AvpArgSubparsers AvpArgSubparsers;
typedef struct AvpArgs AvpArgs;                 /* the parse result: Python's Namespace */

/* ---- values ---------------------------------------------------------------------------------------------------- */
typedef enum {
    AVP_V_NONE,  /* Python None */
    AVP_V_STR,
    AVP_V_INT,
    AVP_V_FLOAT,
    AVP_V_BOOL,
    AVP_V_LIST   /* a list of values (nargs lists, append, extend) */
} AvpArgKind;
typedef struct AvpArgValue AvpArgValue;
struct AvpArgValue {
    AvpArgKind kind;
    AvpStr s;                  /* AVP_V_STR */
    i64 i;                     /* AVP_V_INT, AVP_V_BOOL (0 or 1) */
    double f;                  /* AVP_V_FLOAT */
    const AvpArgValue *items;  /* AVP_V_LIST */
    size_t n;
};

/* A default or a const. Build with AVP_DEF_*; the zero value is "not given". AVP_DEF_SUPPRESS is
 * default=argparse.SUPPRESS: the attribute does not exist unless the argument is given. */
typedef enum {
    AVP_DK_UNSET = 0,
    AVP_DK_NONE,
    AVP_DK_STR,
    AVP_DK_INT,
    AVP_DK_FLOAT,
    AVP_DK_BOOL,
    AVP_DK_SUPPRESS
} AvpDefaultKind;
typedef struct {
    AvpDefaultKind kind;
    const char *s;
    i64 i;
    double f;
} AvpArgDefault;
#define AVP_DEF_NONE ((AvpArgDefault){AVP_DK_NONE, NULL, 0, 0.0})
#define AVP_DEF_STR(str) ((AvpArgDefault){AVP_DK_STR, (str), 0, 0.0})
#define AVP_DEF_INT(v) ((AvpArgDefault){AVP_DK_INT, NULL, (v), 0.0})
#define AVP_DEF_FLOAT(v) ((AvpArgDefault){AVP_DK_FLOAT, NULL, 0, (v)})
#define AVP_DEF_BOOL(v) ((AvpArgDefault){AVP_DK_BOOL, NULL, (v) ? 1 : 0, 0.0})
#define AVP_DEF_SUPPRESS ((AvpArgDefault){AVP_DK_SUPPRESS, NULL, 0, 0.0})

/* ---- declarations ---------------------------------------------------------------------------------------------- */
typedef enum {
    AVP_FORMAT_DEFAULT = 0,        /* HelpFormatter */
    AVP_FORMAT_RAW_DESCRIPTION,    /* RawDescriptionHelpFormatter */
    AVP_FORMAT_RAW_TEXT,           /* RawTextHelpFormatter */
    AVP_FORMAT_ARGUMENT_DEFAULTS,  /* ArgumentDefaultsHelpFormatter */
    AVP_FORMAT_METAVAR_TYPE        /* MetavarTypeHelpFormatter */
} AvpArgFormatter;

typedef struct {
    const char *prog;           /* NULL: the tool name (avp_tool_name()); subparsers: "<parent prog> <name>" */
    const char *usage;          /* NULL: generated; "%(prog)s" is replaced */
    const char *description;
    const char *epilog;
    AvpArgFormatter formatter;
    bool no_help;               /* add_help=False */
    bool no_abbrev;             /* allow_abbrev=False */
    const char *help;           /* subparsers only: the help of this command in the command list (NULL: none) */
    const char *const *aliases; /* subparsers only: NULL-terminated */
} AvpArgParserDesc;

enum {
    AVP_NARGS_NONE = 0,       /* one value (nargs=None) */
    AVP_NARGS_OPT = -1,       /* '?' */
    AVP_NARGS_STAR = -2,      /* '*' */
    AVP_NARGS_PLUS = -3,      /* '+' */
    AVP_NARGS_REMAINDER = -4, /* argparse.REMAINDER ('...') */
    AVP_NARGS_PARSER = -5,    /* 'A...' (the subparsers action) */
    AVP_NARGS_SUPPRESS = -6
    /* a positive N is nargs=N: a list of N values */
};

typedef enum {
    AVP_ARG_STORE = 0,
    AVP_ARG_STORE_CONST,
    AVP_ARG_STORE_TRUE,
    AVP_ARG_STORE_FALSE,
    AVP_ARG_APPEND,
    AVP_ARG_APPEND_CONST,
    AVP_ARG_EXTEND,
    AVP_ARG_COUNT,
    AVP_ARG_HELP,
    AVP_ARG_VERSION,
    AVP_ARG_BOOLEAN_OPTIONAL  /* BooleanOptionalAction: --x and --no-x */
} AvpArgActionKind;

typedef enum {
    AVP_T_STR = 0,  /* type=None / str */
    AVP_T_INT,      /* int */
    AVP_T_INT0,     /* lambda v: int(v, 0) (the type name in messages is "<lambda>") */
    AVP_T_FLOAT,    /* float */
    AVP_T_IDENT,    /* Path and other types that accept every string (the value stays a string); type_name is shown */
    AVP_T_CUSTOM    /* conv */
} AvpArgType;

/* A converter for AVP_T_CUSTOM. Return true with *out set, or false: with msg set (msg->p != NULL) it is an
 * ArgumentTypeError (the message is shown as is), without it a ValueError ("invalid <type_name> value: '<arg>'"). */
typedef bool (*AvpArgConv)(void *ctx, AvpArena *a, AvpStr in, AvpArgValue *out, AvpStr *msg);

typedef struct {
    const char *names;           /* options "-j --jobs" (separated by spaces) or one positional name "units" */
    AvpArgActionKind action;
    AvpArgType type;
    AvpArgConv conv;
    void *conv_ctx;
    const char *type_name;       /* AVP_T_CUSTOM / AVP_T_IDENT: Python's type.__name__ for messages */
    int nargs;                   /* AVP_NARGS_* or N */
    const char *dest;            /* NULL: from the first long option (--foo-bar -> foo_bar) or the positional name */
    const char *metavar;
    const char *const *metavars; /* a metavar tuple, NULL-terminated (nargs=2, metavar=("OLD", "NEW")) */
    const char *const *choices;  /* NULL-terminated */
    AvpArgDefault dflt;
    const char *const *dflt_list; /* default=[...] of strings for append/extend, NULL-terminated; non-NULL even if empty */
    AvpArgDefault cnst;          /* const= */
    bool required;               /* options only */
    const char *help;            /* NULL: no help; AVP_ARG_SUPPRESS hides the argument */
    const char *version;         /* AVP_ARG_VERSION */
} AvpArgDesc;
extern const char avp_args_suppress[];
#define AVP_ARG_SUPPRESS avp_args_suppress

typedef struct {
    const char *dest;            /* NULL: SUPPRESS */
    bool required;
    const char *metavar;
    const char *help;
    const char *title;           /* with a title or description the commands get their own section */
    const char *description;
} AvpArgSubparsersDesc;

/* ---- building -------------------------------------------------------------------------------------------------- */
/* arena NULL: the calling thread's long-lived arena. A parser is used by one thread at a time. */
AvpArgParser *avp_args_parser(AvpArena *arena, const AvpArgParserDesc *d);
AvpArgAction *avp_arg(AvpArgParser *p, const AvpArgDesc *d);                                   /* add_argument */
AvpArgSection *avp_args_group(AvpArgParser *p, const char *title, const char *description);   /* add_argument_group */
AvpArgAction *avp_arg_in(AvpArgSection *g, const AvpArgDesc *d);
AvpArgMutex *avp_args_mutex(AvpArgParser *p, bool required);                  /* add_mutually_exclusive_group */
AvpArgMutex *avp_args_mutex_in(AvpArgSection *g, bool required);
AvpArgAction *avp_arg_mutex(AvpArgMutex *g, const AvpArgDesc *d);
AvpArgSubparsers *avp_args_subparsers(AvpArgParser *p, const AvpArgSubparsersDesc *d);
/* add_parser(name, help=..., aliases=...): d may be NULL; the prog is "<parent usage prefix> <name>". */
AvpArgParser *avp_args_subparser(AvpArgSubparsers *s, const char *name, const AvpArgParserDesc *d);
AvpArgParser *avp_args_subparser_get(AvpArgSubparsers *s, const char *name); /* subparsers.choices[name], or NULL */

/* ---- parsing --------------------------------------------------------------------------------------------------- */
/* parse_args(argv[1:]): on an error prints the usage and message on stderr and exits 2; on -h/--help prints the help
 * on stdout and exits 0. The result is allocated from the parser's arena. */
AvpArgs *avp_args_parse(AvpArgParser *p, int argc, char **argv);
/* parse_known_args(argv[1:]): the unrecognized arguments are avp_args_extras(). */
AvpArgs *avp_args_parse_known(AvpArgParser *p, int argc, char **argv);
/* The same on an explicit list (Python's parse_args(list)). */
AvpArgs *avp_args_parse_list(AvpArgParser *p, const AvpStr *args, size_t n, bool known);
/* Without exiting: returns NULL when Python would have exited, with the exit code and the texts it would have
 * written (usage and error line on err, help or version on out). */
typedef struct {
    int code;
    AvpStr out;
    AvpStr err;
} AvpArgsExit;
AvpArgs *avp_args_try_parse(AvpArgParser *p, const AvpStr *args, size_t n, bool known, AvpArgsExit *ex);

/* ---- results --------------------------------------------------------------------------------------------------- */
/* Accessors by dest. An unknown dest, or the wrong kind of value, is an internal error (Python's AttributeError or
 * TypeError). None reads as "" / 0 / false / empty list; use avp_args_is_none to tell. */
bool avp_args_has(const AvpArgs *r, const char *dest);     /* the attribute exists (a SUPPRESS default may leave it out) */
const AvpArgValue *avp_args_value(const AvpArgs *r, const char *dest); /* NULL if absent */
bool avp_args_is_none(const AvpArgs *r, const char *dest);
bool avp_args_given(const AvpArgs *r, const char *dest);   /* the argument appeared on the command line */
AvpStr avp_args_str(const AvpArgs *r, const char *dest);   /* str value; for a None the result has p == NULL, n == 0 */
i64 avp_args_int(const AvpArgs *r, const char *dest);      /* int, also count */
double avp_args_float(const AvpArgs *r, const char *dest);
bool avp_args_bool(const AvpArgs *r, const char *dest);
size_t avp_args_len(const AvpArgs *r, const char *dest);   /* len() of a list value */
const AvpArgValue *avp_args_item(const AvpArgs *r, const char *dest, size_t i);
AvpStrVec avp_args_strs(const AvpArgs *r, const char *dest); /* a list of str, as a vector (heap-backed) */
AvpI64Vec avp_args_ints(const AvpArgs *r, const char *dest);
AvpStrVec avp_args_extras(const AvpArgs *r);                /* parse_known_args' second result */

/* ---- usage, help and errors ------------------------------------------------------------------------------------ */
AvpStr avp_args_format_usage(AvpArgParser *p);              /* format_usage(): "usage: ...\n" */
AvpStr avp_args_format_help(AvpArgParser *p);               /* format_help() */
void avp_args_print_usage(AvpArgParser *p);                 /* stdout */
void avp_args_print_help(AvpArgParser *p);                  /* stdout */
/* parser.error(message): the usage and "<prog>: error: <message>" on stderr, exit code 2. */
AVP_NORETURN void avp_args_error(AvpArgParser *p, AVP_PRINTF_FMT const char *fmt, ...);
AvpStr avp_args_error_text(AvpArgParser *p, AvpStr message); /* what error() writes, without exiting */
const char *avp_args_prog(const AvpArgParser *p);
/* The width argparse formats to: COLUMNS if a positive integer, else the console width of stdout, else 80, minus 2. */
int avp_args_width(void);
