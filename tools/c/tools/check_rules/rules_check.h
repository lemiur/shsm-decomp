/* rules_check: the Checker of tools/rules/check_rules.py (class Checker, L2030-3577 for this stage): the source
 * index, include resolution and closures, the project-wide facts the per-file checks read, Finding, and the
 * per-file checks (Checker.check_file and everything it calls). The cross-file rules, the R13 slot check,
 * RULE-EXCEPTION handling and the allow-list pass are stage 3 (rules_vtables.c, rules_cross.c, rules_final.c).
 * rules_check.md documents the API. */
#pragma once
#include "rules_sources.h"

/* One Finding (check_rules.py L1860). func.p == NULL is None. tier and status are static strings. */
typedef struct {
    AvpStr path;
    i32 line;
    const char *rule;
    const char *tier;
    AvpStr msg;
    AvpStr func;
    const char *status; /* "violation", "allowed" (check_asm, apply_allow) or "excepted" (apply_exceptions) */
    AvpStr reason;
    const AvpStr *units; /* compute_units (sorted); nunits 0 until then */
    size_t nunits;
} RlFinding;
typedef RL_VEC(RlFinding) RlFindings;

/* An R7 identifier record of check_identifier's `seen` dict: key (function name, name) -> [line, count, msg]. */
typedef struct {
    i32 line, count;
    AvpStr msg;
} RlIdent;

/* configure.py and docs/exceptions.md, as main() reads them. */
typedef struct {
    AvpMap *unit_status;  /* parse_units: path under src/ -> AvpStr* (Matching|NonMatching|Equivalent) */
    AvpMap *include_map;  /* parse_include_dirs: path under src/ -> AvpStrVec* */
    AvpSet *original_asm; /* Allow.original_asm: "unit\x01function" */
    AvpSet *asm_helpers;  /* Allow.asm_helpers: "unit\x01function" */
    AvpStr allow_source;  /* Allow.source ("" without an exceptions record) */
    AvpSet *rulings;      /* Allow.rulings: "rulefunction" */
    AvpSet *pending;      /* Allow.pending: function names (read by no check) */
} RlConfig;

/* parse_units(configure_text) and parse_include_dirs(configure_text) into cfg (maps created on demand). */
void rl_parse_units(AvpArena *a, RlConfig *cfg, AvpStr configure_text);
void rl_parse_include_dirs(AvpArena *a, RlConfig *cfg, AvpStr configure_text);
/* Allow.from_markdown(text, source): the ORIGINAL-ASM and inline-asm-helper sets (the per-file checks read only those). */
void rl_allow_from_markdown(AvpArena *a, RlConfig *cfg, AvpStr text, AvpStr source);
/* main(): configure.py (or `configure`) and docs/exceptions.md (or `allow`) under root; missing files leave the
 * tables empty, as main does. */
void rl_config_load(AvpArena *a, RlConfig *cfg, const char *root, const char *configure, const char *allow);

/* ---- stage 3: the state run() leaves (rules_check.md) ---- */
typedef struct {
    AvpStr cls;
    const AvpJson *retail, *vtable; /* NULL is None */
    AvpStr by;
} RlR13Match;
typedef struct {
    i64 classes, added, missing, order, override_, advisory;
    RL_VEC(RlR13Match) matched;
    RL_VEC(AvpStr) unmatched, skipped;
    AvpStr note; /* None as p == NULL */
} RlR13;
typedef struct {
    AvpStr path;
    i32 line;
    RL_VEC(AvpStr) rules;
    AvpStr reason, problem;
    bool valid, refused;
    i64 suppressed;
    i32 lo, hi; /* cover */
} RlException;
typedef struct {
    AvpStr path, name;
    i32 line, end_line;
    const char *status; /* ok, MATCH-PENDING, ORIGINAL-ASM */
    const char *tiers[5]; /* counts, in insertion order */
    i64 counts[5];
    int ntiers;
    RL_VEC(AvpStr) match_rules;
    AvpStr unit_status;
} RlFunctionRec;

typedef struct RlChecker {
    AvpArena *a;
    /* self.sources: load_sources(root), then the scanned paths main() reads on top (insertion order) */
    AvpStrVec paths, texts;
    AvpMap *index; /* path -> index */
    AvpMap *storage; /* Checker.parse's storage macros */
    RlParsed **parsed; /* per index, NULL until parsed */
    AvpArena **arenas;
    bool failed;       /* a parse raised (the lexer's AttributeError): the Python run fails */
    RlConfig cfg;
    AvpStrVec include_dirs; /* ("include",) */
    AvpStrVec *every_lib;   /* _every_library_dir memo */
    AvpStrVec **closure_memo; /* per index: sorted(closure(path)) */
    AvpSet *offset_names;   /* offset_macro_names() */
    AvpSet *noted_volatile; /* run(): _volatile_notes() */
    RlFindings findings;
    /* per-file state of check_file */
    RlParsed *P;
    AvpMap *ph;          /* check_identifier's seen: key -> index into ph_v */
    RL_VEC(RlIdent) ph_v;
    AvpStrVec ph_keys;   /* parallel to ph_v: the key's function and name (for nothing but order) */
    AvpMap *ptr_names;   /* _pointer_names memo: "line\x01name" -> AvpSet* */
    AvpMap *macro_bodies; /* _macro_bodies(P.path): macro name -> AvpSet* (NULL until first use) */
    /* run() (stage 3) */
    AvpStrVec scan;           /* the scanned paths, as given */
    AvpStrVec checked;        /* checked_paths */
    AvpSet *checked_set;
    const char *error;        /* the class of an exception the Python run raises, else NULL */
    AvpJson *classes_data;    /* the parsed classes.json, NULL for None */
    AvpStr classes_note;
    AvpMap *src_classes;      /* _src_classes(): name -> RlClassDefs* */
    AvpStrVec src_class_names;
    AvpMap *r13_models;       /* _r13_models: name -> model */
    RlR13 r13;
    RL_VEC(RlException) exceptions;
    RL_VEC(RlFunctionRec) functions;
} RlChecker;

/* Checker(sources, scan, unit_status, allow, include_map=...): the sources of load_sources(root) plus each extra
 * path (read from root/path when not loaded). Raises nothing: an unreadable extra makes the result NULL. */
RlChecker *rl_checker_new(AvpArena *a, const char *root, const AvpStrVec *extra, const RlConfig *cfg);
/* Checker(sources, ...) over in-memory sources (a {path: text} dict in order); cfg may be NULL. */
RlChecker *rl_checker_from(AvpArena *a, const AvpStrVec *paths, const AvpStrVec *texts, const RlConfig *cfg);
/* Checker.parse(path): the memoized parse (NULL for a path not in the sources). */
RlParsed *rl_checker_parse(RlChecker *ck, AvpStr path);
/* closure(path): sorted, as every caller uses it. */
const AvpStrVec *rl_checker_closure(RlChecker *ck, AvpStr path);
AvpStr rl_checker_status_of(RlChecker *ck, AvpStr path);
/* run()'s prelude: _set_header_languages() and noted_volatile = _volatile_notes() (also offset_macro_names()). */
void rl_checker_prepare(RlChecker *ck);
/* run(): checked_paths, the prelude, check_file of each checked path, check_cross_file, check_vtables,
 * apply_exceptions, apply_allow, compute_units, build_functions. ck->scan must be set; classes_data/classes_note
 * as main() passes them. A failure the Python raises leaves ck->error (or ck->failed for the lexer). */
void rl_checker_run(RlChecker *ck);
/* exit_code() and blocking(f) */
int rl_exit_code(RlChecker *ck);
bool rl_blocking(RlChecker *ck, const RlFinding *f);
/* The stage-3 phases (rules_vtables.c, rules_cross.c, rules_final.c). */
void rl_check_cross_file(RlChecker *ck);
void rl_check_vtables(RlChecker *ck);
void rl_apply_exceptions(RlChecker *ck);
void rl_apply_allow(RlChecker *ck);
void rl_compute_units(RlChecker *ck);
void rl_build_functions(RlChecker *ck);
/* closure membership: q in closure(path) */
bool rl_closure_has(RlChecker *ck, AvpStr path, AvpStr q);
/* resolve_include(from_path, name, library_dir, tu): the source path, or {NULL, 0} for None */
AvpStr rl_resolve_include(RlChecker *ck, AvpStr from, AvpStr name, bool library_dir, AvpStr tu);
/* path.lower().endswith(SRC_EXTS) */
bool rl_is_source_path(AvpArena *a, AvpStr path);
/* unit_key(path) */
AvpStr rl_unit_key(AvpArena *a, AvpStr path);
/* The run's state as the stage-3 dump writes it (rules_check.md "The final dump"). */
void rl_dump_final(RlChecker *ck, AvpBuf *b);
/* The JSON objects of one RULE-EXCEPTION record (cover: with its "cover" pair, as the dump writes it; main's
 * exception_comments leave it out), one build_functions record and ck.r13. */
void rl_json_exception(AvpJsonW *w, const RlException *ex, bool cover);
void rl_json_function(AvpJsonW *w, const RlFunctionRec *r);
void rl_json_r13(AvpJsonW *w, const RlR13 *s);

/* check_file(P) for the parse of path. */
void rl_check_file(RlChecker *ck, RlParsed *P);
/* add(): appends a finding (unless R17 in a `no copy in retail` body); returns its index, or -1 when dropped. */
i64 rl_add(RlChecker *ck, RlParsed *P, i32 line, const char *rule, const char *tier, AvpStr msg, AvpStr func);
/* tier_of(rule): RULES[rule][0], NULL for an unknown id (Python: KeyError). */
const char *rl_tier_of(const char *rule);
/* Finding.text() and to_dict() (without "units", always [] here). */
AvpStr rl_finding_text(AvpArena *a, const RlFinding *f);
void rl_finding_json(AvpJsonW *w, const RlFinding *f);
/* to_dict() with "units" */
void rl_finding_json_full(AvpJsonW *w, const RlFinding *f);

/* ---- helpers shared by the check modules (rules_check_*.c) ---- */
typedef struct {
    i32 lo, hi; /* a statement: the contiguous token range [lo, hi) */
} RlRange;
typedef struct {
    AvpStr name, type;
    i32 tok; /* index of the name token in the range's stream */
} RlDecl;

/* Checker.statements(toks, match, lo, hi): every statement is a contiguous range; *out in the scratch arena. */
size_t rl_statements(const RlTok *toks, const i32 *match, i32 lo, i32 hi, RlRange **out);
/* Checker.parse_decl(st): 0 for None, else the declarators in *out (scratch arena; tok = index relative to st). */
size_t rl_parse_decl(AvpArena *a, const RlTok *st, size_t n, RlDecl **out);
/* Checker._cast_info: the close index, or -1 for None (inner = toks[i+1 .. c)). */
i32 rl_cast_info(const RlTok *toks, size_t n, const i32 *match, size_t i, bool chained);
/* re.sub(r"\s+", "", s) */
AvpStr rl_strip_ws(AvpArena *a, AvpStr s);
AvpStr rl_fmt(AvpArena *a, const char *fmt, ...);

/* The check methods, in rules_check_tokens.c, rules_check_function.c and rules_check_file.c. */
void rl_check_tokens(RlChecker *ck, RlParsed *P);
void rl_check_identifier(RlChecker *ck, RlParsed *P, const RlTok *t);
void rl_flush_identifiers(RlChecker *ck, RlParsed *P);
void rl_check_r4(RlChecker *ck, RlParsed *P, const RlTok *toks, size_t n, const i32 *match, bool in_macro);
void rl_check_directives(RlChecker *ck, RlParsed *P);
void rl_check_comments(RlChecker *ck, RlParsed *P);
void rl_check_asm(RlChecker *ck, RlParsed *P);
void rl_check_declarations(RlChecker *ck, RlParsed *P);
void rl_check_function(RlChecker *ck, RlParsed *P, RlFunc *f);
void rl_check_function_meta(RlChecker *ck, RlParsed *P);

/* Word lists (NULL-terminated) the modules share. */
extern const char *const RL_SCALAR_WORDS[];
extern const char *const RL_DECL_KEYWORDS[];
extern const char *const RL_STMT_KEYWORDS[];
bool rl_volatile_type(AvpStr s); /* VOLATILE_TYPES.fullmatch */
