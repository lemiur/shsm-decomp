/* configure.exe: the interpreter of configure.py's Python subset (DESIGN.md section 1).
 *
 * cfg_lex.c   tokens (Python's tokenizer: INDENT/DEDENT, implicit line joining, strings with prefixes r/f/u, triple
 *             quotes, comments, backslash continuation)
 * cfg_parse.c the AST of the statements and expressions of the subset; everything else is a hard error
 * cfg_eval.c  execution over pv.h values; cfg_builtins.c the built-ins and modules; cfg_dump.c the --dump document.
 *
 * Every error the subset cannot express is reported as `configure: error: <script>:<line>: <what>` with exit 2
 * (cfg_fail); Python's own exits (sys.exit, argparse errors) keep Python's messages and codes. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "avp/arena.h"
#include "avp/str.h"
#include "project.h"
#include "pv.h"

/* ---- errors ---- */
void cfg_set_script_name(const char *name); /* the name errors cite (the script's basename) */
AVP_NORETURN void cfg_fail(int line, AVP_PRINTF_FMT const char *fmt, ...);

/* ---- tokens ---- */
typedef enum {
    CFG_T_EOF,
    CFG_T_NEWLINE,
    CFG_T_INDENT,
    CFG_T_DEDENT,
    CFG_T_NAME,
    CFG_T_INT,
    CFG_T_STR, /* text: the decoded value; fstr: an f-string, text is the undecoded body */
    CFG_T_OP,
} CfgTokKind;

typedef struct {
    CfgTokKind kind;
    AvpStr text;
    int64_t ival;
    bool fstr;
    bool raw; /* r prefix (f-strings: the literal parts are not escape-decoded) */
    int line;
} CfgTok;
typedef AVP_VEC(CfgTok) CfgToks;

/* Tokenizes src (LF line ends). expr_only: the text of an f-string field (no INDENT/NEWLINE tokens, as inside
 * brackets). Line numbers start at first_line. */
CfgToks cfg_lex(AvpArena *a, AvpStr src, int first_line, bool expr_only);
/* Decodes the escapes of a non-raw string literal body. */
AvpStr cfg_unescape(AvpArena *a, AvpStr body, int line);

/* ---- AST ---- */
typedef enum {
    /* expressions */
    CFG_E_CONST,   /* value */
    CFG_E_NAME,    /* name */
    CFG_E_ATTR,    /* a.name */
    CFG_E_SUBSCR,  /* a[b] */
    CFG_E_CALL,    /* a(items: positional or STARRED or KEYWORD) */
    CFG_E_KEYWORD, /* name=a (call arguments only) */
    CFG_E_STARRED, /* *a */
    CFG_E_LIST,    /* [items] */
    CFG_E_TUPLE,   /* (items) */
    CFG_E_DICT,    /* {items[2i]: items[2i+1]} */
    CFG_E_FSTR,    /* items: CONST str parts and field expressions (op: 0, 'r' or 's' conversion) */
    CFG_E_BINOP,   /* a op b, op '+', '-', '*', '/', '%' */
    CFG_E_UNARY,   /* op 'n' (not), '-' or '+', on a */
    CFG_E_BOOLOP,  /* items joined by op 'a' (and) or 'o' (or) */
    CFG_E_COMPARE, /* a, then items[i] compared with ops[i] */
    CFG_E_IFEXP,   /* a if b else c */
    CFG_E_FIELD,   /* an f-string field: a, op the conversion */
    /* statements */
    CFG_S_EXPR,       /* a */
    CFG_S_ASSIGN,     /* items (targets, chained a = b = value) = a */
    CFG_S_ANNASSIGN,  /* target items[0], value a (may be NULL); the annotation is parsed and dropped */
    CFG_S_IF,         /* if a: body else: orelse */
    CFG_S_FOR,        /* for b in a: body */
    CFG_S_DEF,        /* def name(params): body */
    CFG_S_RETURN,     /* return a (NULL: None) */
    CFG_S_IMPORT,     /* import names[i] as asnames[i] */
    CFG_S_FROMIMPORT, /* from name import names[i] as asnames[i] */
    CFG_S_PASS,
    CFG_S_BREAK,
    CFG_S_CONTINUE,
} CfgKind;

/* comparison operators */
enum { CFG_CMP_EQ = 1, CFG_CMP_NE, CFG_CMP_LT, CFG_CMP_LE, CFG_CMP_GT, CFG_CMP_GE, CFG_CMP_IN, CFG_CMP_NOTIN,
       CFG_CMP_IS, CFG_CMP_ISNOT };

typedef struct CfgNode CfgNode;
typedef AVP_VEC(CfgNode *) CfgNodes;

typedef struct {
    AvpStrVec names;     /* positional parameters */
    CfgNodes defaults;   /* defaults of the last defaults.n parameters */
    AvpStr star;         /* *args name (p == NULL: none) */
} CfgParams;

struct CfgNode {
    CfgKind kind;
    int line;
    int op;
    AvpStr name;
    Pv *value;
    CfgNode *a, *b, *c;
    CfgNodes items;
    AVP_VEC(int) ops;
    CfgNodes body, orelse;
    CfgParams *params;
    AvpStrVec names, asnames;
};

/* Parses a whole file into its statements. */
CfgNodes cfg_parse_file(AvpArena *a, AvpStr src);

/* ---- interpreter ---- */
typedef struct CfgInterp CfgInterp;

typedef struct {
    ProjectEnv env;
    const char *script_path; /* the file to execute */
    const char *prog;        /* argparse's prog: basename of sys.argv[0] */
    bool dump;               /* --dump: generate_build / calculate_progress print the state instead */
} CfgOptions;

/* Executes the script; returns the exit code (0 when it ran to the end). sys.exit and argparse exits end the process
 * as Python's do. */
int cfg_run(AvpArena *a, const CfgOptions *opts);
/* Executes src (the text of a script) and returns its module globals (a dict): the unit tests' entry. */
Pv *cfg_exec_source(AvpArena *a, const CfgOptions *opts, AvpStr src);

/* ---- internals shared by the cfg_*.c files ---- */
typedef Pv *(*CfgBuiltinFn)(CfgInterp *I, int line, Pv *self, Pv **args, size_t n, AvpStr *kwnames, Pv **kwvals,
                            size_t nkw);
typedef struct {
    const char *name;
    CfgBuiltinFn fn;
    Pv *self; /* bound methods: the receiver */
    void *data;
} CfgBuiltin;

typedef struct {
    AvpStr name;
    CfgParams *params;
    CfgNodes *body;
    Pv **defaults; /* evaluated at def time */
} CfgFunction;

struct CfgInterp {
    AvpArena *a;
    const CfgOptions *opts;
    Pv *globals;  /* dict */
    Pv *builtins; /* dict */
    Pv *last_args; /* the namespace parse_args returned last */
};

Pv *cfg_builtin(AvpArena *a, const char *name, CfgBuiltinFn fn, Pv *self, void *data);
void cfg_install_builtins(CfgInterp *I);
Pv *cfg_import(CfgInterp *I, int line, AvpStr module);           /* a module (an instance of class "module") */
Pv *cfg_getattr(CfgInterp *I, int line, Pv *obj, AvpStr name);   /* attribute or method, or cfg_fail */
Pv *cfg_call(CfgInterp *I, int line, Pv *fn, Pv **args, size_t n, AvpStr *kwnames, Pv **kwvals, size_t nkw);
void cfg_iterate(CfgInterp *I, int line, Pv *v, Pv ***items, size_t *n); /* the items `for` visits */
const char *cfg_type_name(const Pv *v);
/* The --dump document of the namespace and the config, json.dumps(indent=1) plus a newline. */
AvpStr cfg_dump(AvpArena *a, Pv *args, Pv *config);
