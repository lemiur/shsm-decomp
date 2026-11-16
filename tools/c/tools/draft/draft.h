/* draft.py in C: the draft pipeline (select, packet, twin, grade, promote, reject, rejects, sweep).
 *
 * Modules (one header for the tool):
 *   draft_lex.c    levers.py's tokenizer, bracket matcher and definition scanner (tokenize, code_tokens,
 *                  match_brackets, _skip_angle_back, _split_top, _fn_def, scan_definitions) and draft.py's source
 *                  scanning on top of them (scan_source, find_def, includes_of, class_header, extern_of,
 *                  top_level_statements, class_block)
 *   draft_text.c   names and paths (draft_root ... parse_range, split_mangled, demangled_parts, remangle, c_escape),
 *                  the substitution map (Subst, pair_relocations, own_address_name, build_subst, substitute),
 *                  clone_header, and the grading helpers screen, marked_functions, compiled_name_for, rename_explains
 *   draft_cli.c    the parser (build_parser) and the subcommand dispatch
 *   draft_world.c  World, brief's infer_profile
 *   draft_twin.c   class evidence and twin drafting (twin, twin --unit)
 *   draft_grade.c  Grader, grade, the rejects table, packet
 *   draft_select.c select, the queue by class, the select cache (shared with the Python), sweep
 *   draft_promote.c Promoter, promote
 *
 * Text offsets: the Python works on str (code point offsets); the C works on the UTF-8 bytes, so every offset here
 * (token s/e, definition start/end/banner, statement spans) is a byte offset into the same text. Slicing the text
 * with them gives the same strings as the Python's slices (DEVIATIONS.md, D-draft-1). Python None: an integer
 * field holds DRAFT_NONE, a string field has p == NULL (AvpStr) or is NULL (const char *). */
#pragma once
#include "avp/avp.h"
#include "shsm_asm.h"
#include "shsm_cli.h"
#include "shsm_repo.h"
#include "shsm_asmindex.h"
#include "shsm_symbols.h"
#include "shsm_units.h"
#include "hashes.h"
#include "twins.h"
#include "unit_plan.h"
#include "unit_stub.h" /* UnitDecls: the classes the headers of include/ define */

#define DRAFT_NONE SHSM_NONE

/* levers.ParseError (an unbalanced bracket): a plain Exception in the Python (uncaught by draft.py: a traceback). */
#define DRAFT_EXC_PARSE 0x4450

#define DRAFT_SMALL_MAX 32
#define DRAFT_LEAF_MAX 96

/* ==== draft_lex.c: tokens ======================================================================================= */

/* levers.Tok kinds */
typedef enum { DRAFT_TK_ID, DRAFT_TK_NUM, DRAFT_TK_STR, DRAFT_TK_CHR, DRAFT_TK_OP, DRAFT_TK_COMMENT, DRAFT_TK_PP } DraftTokKind;

/* levers.Tok(k, t, s, e): t is a view into the text, s/e byte offsets (t == text[s:e]). */
typedef struct {
    DraftTokKind k;
    AvpStr t;
    i64 s, e;
} DraftTok;
typedef AVP_VEC(DraftTok) DraftToks;

const char *draft_tok_kind_name(DraftTokKind k); /* "id" "num" "str" "chr" "op" "comment" "pp" */
bool draft_tok_is(const DraftTok *t, const char *text); /* t.t == text */

/* levers.tokenize(text): every token, comments and preprocessor lines included. */
DraftToks draft_tokenize(AvpStr text);
/* levers.code_tokens(toks): without comment and pp tokens. */
DraftToks draft_code_tokens(const DraftToks *toks);
/* levers.match_brackets(code): m[i] the index of the bracket matching code[i] (-1 for a non-bracket), allocated from
 * a. Raises DRAFT_EXC_PARSE "unbalanced ')' at offset N" / "unclosed '('" (N a byte offset). */
i64 *draft_match_brackets(AvpArena *a, const DraftToks *code);

/* levers._skip_angle_back(code, q, lo): the index of the '<' matching the '>' at q, or -1 (None). */
i64 draft_skip_angle_back(const DraftToks *code, i64 q, i64 lo);

typedef struct {
    i64 s, e; /* half-open token range */
} DraftSpan;
typedef AVP_VEC(DraftSpan) DraftSpans;
/* levers._split_top(code, a, b, m, sep): code[a:b] split at top-level `sep`. */
DraftSpans draft_split_top(const DraftToks *code, i64 a, i64 b, const i64 *m, const char *sep);

/* levers.FnDef(name, quals, cls, nparams, head, lbrace, rbrace, is_void, params). */
typedef struct {
    const char *name;  /* "~Foo", "operator=", "fn_800067CC" */
    AvpStrVec quals;   /* qualifiers before the name, outermost first */
    const char *cls;   /* NULL: None */
    i64 nparams;
    i64 head, lbrace, rbrace; /* token indices */
    bool is_void;
    DraftSpans params;
} DraftFnDef;
typedef AVP_VEC(DraftFnDef) DraftFnDefs;

/* levers._fn_def(code, m, s, e, cls_stack): false for None. */
bool draft_fn_def(AvpArena *a, const DraftToks *code, const i64 *m, i64 s, i64 e, const AvpStrVec *cls_stack,
                  DraftFnDef *out);
/* levers.scan_definitions(code, m). */
DraftFnDefs draft_scan_definitions(AvpArena *a, const DraftToks *code, const i64 *m);

/* ==== draft_lex.c: source scanning =============================================================================== */

/* One scan_source() dict: {name, cls, start, end, banner, address}. */
typedef struct {
    const char *name;
    const char *cls; /* NULL: None */
    i64 start, end;  /* the definition, head to closing brace */
    i64 banner;      /* start of the comment block right above it */
    i64 address;     /* the `// 0x8...` comment marking it, DRAFT_NONE when absent */
} DraftDef;
typedef AVP_VEC(DraftDef) DraftDefs;

/* draft.scan_source(text). Raises DRAFT_EXC_PARSE as match_brackets. */
DraftDefs draft_scan_source(AvpArena *a, AvpStr text);
/* {"name", "cls", "start", "end", "banner", "address"} of one definition (the Python dict; offsets in bytes). */
AvpJson *draft_def_json(AvpArena *a, const DraftDef *d);
/* draft.find_def(text, symbol, address): the definition, or NULL. address DRAFT_NONE: None. */
const DraftDef *draft_find_def(AvpArena *a, AvpStr text, const char *symbol, i64 address);
/* draft.find_def over an already scanned list (the same answer, no rescan). */
const DraftDef *draft_find_def_in(AvpArena *a, const DraftDefs *defs, const char *symbol, i64 address);
/* draft.includes_of(text): the names of every #include, in order (views into text). */
AvpStrVec draft_includes_of(AvpStr text);
/* draft.class_header(repo, source_text, cls, source_path): the include (relative to include/) declaring cls, or
 * NULL. include_dir is repo.include_dir; cls NULL is Python's None. */
const char *draft_class_header(AvpArena *a, const char *include_dir, AvpStr source_text, const char *cls,
                               const char *source_path);
/* draft.extern_of(stmt, name): the extern declaration, or NULL. */
const char *draft_extern_of(AvpArena *a, AvpStr stmt, const char *name);

/* One top_level_statements() tuple (start, end, text). */
typedef struct {
    i64 start, end; /* byte offsets into the text */
    AvpStr text;    /* the statement, comments blanked, stripped */
} DraftStmt;
typedef AVP_VEC(DraftStmt) DraftStmts;
/* draft.top_level_statements(text, defs). */
DraftStmts draft_top_level_statements(AvpArena *a, AvpStr text, const DraftDefs *defs);
/* draft.class_block(text, name): false for None; *start/*end byte offsets. Raises DRAFT_EXC_PARSE. */
bool draft_class_block(AvpArena *a, AvpStr text, const char *name, i64 *start, i64 *end);

/* ==== draft_text.c: names and paths =============================================================================== */

const char *draft_root(AvpArena *a, const ShsmRepo *r);         /* <root>\build\draft */
const char *draft_rejects_path(AvpArena *a, const ShsmRepo *r); /* <config_dir>\draft_rejects.tsv */
/* draft.rel_path(repo, path): relative to the root (forward slashes), else the absolute path (forward slashes). */
const char *draft_rel_path(AvpArena *a, const ShsmRepo *r, const char *path);
const char *draft_safe_name(AvpArena *a, const char *sym); /* [\\/:*?"<>|] -> _ */
const char *draft_strip_ext(AvpArena *a, const char *path);  /* NULL stays NULL */
const char *draft_plain_unit(AvpArena *a, const char *u);    /* NULL is "" */
/* draft.parse_range(text): false for None. */
bool draft_parse_range(AvpStr text, i64 *start, i64 *end);
/* draft.split_mangled(sym): the plain name and the rest after `__` (rest->p NULL: None). Views into sym. */
AvpStr draft_split_mangled(AvpStr sym, AvpStr *rest);
/* draft.demangled_parts(sym): the plain name; *cls the class or NULL. */
const char *draft_demangled_parts(AvpArena *a, const char *sym, const char **cls);
/* A twin draft is refused with this text at the start of its message when the target member already has its body in the
 * class of its header: a sweep counts it as SKIPPED, not FAIL. */
#define DRAFT_INLINE_DEFINED "already defined inline in "
/* True when the class block of `header` defines `member` (a name as draft_demangled_parts gives it, `~X` for a
 * destructor) with a body inside the class; a declaration (`;`) is not a definition. */
bool draft_class_defines_member(AvpArena *a, AvpStr header, const char *cls, const char *member);
/* `header` with the first in-class definition of `member` (a body without nested braces) cut to its declaration:
 * `virtual ~X() {}` becomes `virtual ~X();`. */
AvpStr draft_declare_member(AvpArena *a, AvpStr header, const char *member);
/* draft.c_escape(text). */
const char *draft_c_escape(AvpArena *a, AvpStr text);
/* draft.ADDR_NAME.match(name): `fn_XXXXXXXX` / `lbl_XXXXXXXX` (upper-case hex); *addr its address when non-NULL. */
bool draft_addr_name(AvpStr name, i64 *addr);

/* ==== draft_text.c: the substitution map ========================================================================== */

/* String maps (idents, classes, strings, renames) are AvpMap with each value's .p an arena AvpStr * (strings may
 * hold NUL bytes); these helpers read and write them. */
AvpStr draft_smap_get(const AvpMap *m, AvpStr key); /* p NULL when absent */
void draft_smap_set(AvpArena *a, AvpMap *m, AvpStr key, AvpStr val);

typedef struct {
    AvpStr kind, old, new_, why;
} DraftEvidence;
typedef struct {
    AvpStr kind, name, why;
} DraftUnmapped;

/* draft.Subst. */
typedef struct {
    AvpArena *a;
    AvpMap *idents, *classes, *strings, *renames;
    AvpIMap *addresses; /* twin address -> target address (.i) */
    AVP_VEC(DraftEvidence) evidence;
    AVP_VEC(DraftUnmapped) unmapped;
    AvpStrVec conflicts;
} DraftSubst;

DraftSubst *draft_subst_new(AvpArena *a);
/* Subst.add(table, kind, old, new, why). */
void draft_subst_add(DraftSubst *s, AvpMap *table, const char *kind, AvpStr old, AvpStr new_, AvpStr why);
void draft_subst_unmapped(DraftSubst *s, const char *kind, AvpStr name, AvpStr why);
/* Subst.to_dict() (same keys, same order). */
AvpJson *draft_subst_json(AvpArena *a, const DraftSubst *s);
/* Subst.tsv(). */
AvpStr draft_subst_tsv(AvpArena *a, const DraftSubst *s);

/* One pair_relocations() tuple (twin sym, twin off, target sym, target off). */
typedef struct {
    AvpStr tsym;
    i64 toff;
    AvpStr gsym;
    i64 goff;
} DraftPair;
typedef AVP_VEC(DraftPair) DraftPairs;
/* draft.pair_relocations(twin_rows, target_rows) over two asm blocks' rows (ShsmAsmBlock.rows / nrows). */
void draft_pair_relocations(AvpArena *a, const ShsmAsmRow *twin, size_t ntwin, const ShsmAsmRow *target,
                            size_t ntarget, DraftPairs *pairs, AvpStrVec *problems);
/* draft.own_address_name(sym, address); address DRAFT_NONE: None. */
bool draft_own_address_name(AvpStr sym, i64 address);
/* draft.remangle(sym, classes, idents). */
const char *draft_remangle(AvpArena *a, AvpStr sym, const AvpMap *classes, const AvpMap *idents);

/* The `info(sym, off)` callback of build_subst: {"address", "string"}. */
typedef struct {
    i64 address;   /* DRAFT_NONE: None */
    AvpStr string; /* the string content, p NULL: None */
} DraftInfo;
typedef DraftInfo (*DraftInfoFn)(void *ctx, AvpStr sym, i64 off);

/* draft.build_subst(pairs, info, twin_fn, target_fn, twin_class, target_class, class_why). twin_class /
 * target_class / class_why NULL: None. */
DraftSubst *draft_build_subst(AvpArena *a, const DraftPairs *pairs, DraftInfoFn info, void *info_ctx,
                              AvpStr twin_sym, i64 twin_addr, AvpStr target_sym, i64 target_addr,
                              const char *twin_class, const char *target_class, const char *class_why);
/* draft.substitute(text, s). */
AvpStr draft_substitute(AvpArena *a, AvpStr text, const DraftSubst *s);
/* The namespaces a member function's mangled name puts its class in (`__dt__Q24FMOD19ChannelRealManual3DFv`: FMOD). */
AvpStrVec draft_namespaces_of(AvpArena *a, const char *sym);
/* text with the block of class cls wrapped in `namespace X { }` for each of ns (outermost first); text itself when there
 * is none to wrap or no such class. */
AvpStr draft_wrap_class_block(AvpArena *a, AvpStr text, const char *cls, const AvpStrVec *ns);
/* draft.clone_header(hdr, hdr_inc, tcls, gcls, s, used, dropped): used a set of identifiers, dropped appended to.
 * hdr_inc / gcls NULL: None. */
AvpStr draft_clone_header(AvpArena *a, AvpStr hdr, const char *hdr_inc, const char *tcls, const char *gcls,
                          const DraftSubst *s, const AvpSet *used, AvpStrVec *dropped);

/* ==== draft_text.c: grading helpers =============================================================================== */

/* One screen() hit (line number, label, matched text). */
typedef struct {
    i64 line;
    const char *label;
    AvpStr text;
} DraftScreenHit;
typedef AVP_VEC(DraftScreenHit) DraftScreenHits;
/* draft.screen(text). */
DraftScreenHits draft_screen(AvpStr text);

/* One marked_functions() tuple (address, name, class). */
typedef struct {
    i64 address;
    const char *name, *cls;
} DraftMark;
typedef AVP_VEC(DraftMark) DraftMarks;
/* draft.marked_functions(text). Raises DRAFT_EXC_PARSE. */
DraftMarks draft_marked_functions(AvpArena *a, AvpStr text);
/* draft.compiled_name_for(names, name, cls): the compiled symbol, or NULL. cls NULL: None. */
const char *draft_compiled_name_for(AvpArena *a, const char *const *names, size_t n, const char *name,
                                    const char *cls);
/* draft.rename_explains(t, renames) for one target mismatch (t's retail, compiled and detail texts, p NULL for
 * None); renames maps a retail symbol to the draft's name (draft_smap_*). */
bool draft_rename_explains(AvpArena *a, AvpStr retail, AvpStr compiled, AvpStr detail, const AvpMap *renames);

/* ==== draft_world.c: World ======================================================================================= */

/* draft.World: lazy shared state. Each accessor opens its part on first use (as the Python's properties): the asm
 * index read-only (AsmIndex.open_readonly), the twins model (twins.Twins(root): fingerprints refreshed when stale),
 * symbols.txt with the splits attached, the unit table. Allocation from W->a (the thread's long-lived arena). */
typedef struct DraftWorld {
    AvpArena *a;
    const ShsmRepo *repo;
    ShsmAsmIndex *ix_;
    Twins *tw_;
    ShsmSymbolTable *st_;
    const ShsmUnits *units_;
    AvpMap *plans; /* unit_of's plan per "start-end" key: UpPlanDoc * (.p NULL: None) */
    const UnitDecls *decls_; /* the declaration scan of include/ (draft_world_decls) */
} DraftWorld;

DraftWorld *draft_world_new(const char *root); /* World(root): Repo.find(root); root NULL is the cwd's */
void draft_world_close(DraftWorld *W);
ShsmAsmIndex *draft_world_ix(DraftWorld *W);
Twins *draft_world_tw(DraftWorld *W);
const ShsmSymbolTable *draft_world_st(DraftWorld *W);
const ShsmUnits *draft_world_units(DraftWorld *W);
const UnitDecls *draft_world_decls(DraftWorld *W); /* the classes and namespaces include/ declares, scanned once */

/* World.info(sym, off): the address of a referenced symbol and the string literal at sym+off (a DraftInfoFn; ctx is
 * the World). */
DraftInfo draft_world_info(void *W, AvpStr sym, i64 off);

/* World.unit_of(address): (unit path, kind, confidence, range). */
typedef struct {
    const char *path; /* NULL: None */
    const char *kind; /* "unit", "plan", "asm", "none" */
    bool has_conf;    /* false: None; plan kind: the confidence of the cut at the end of the unit */
    double conf;
    bool has_range;   /* false: None */
    i64 start, end;
    /* plan kind: how sure the start of the unit is. start_edge: it is the edge of the dtk asm block (a configured
     * split or the image's edge lies there); else has_start_conf and start_conf: the confidence of the cut before
     * the unit (the previous unit's end). end_edge: the end is the edge of the block (has_conf is false). */
    bool start_edge, end_edge;
    bool has_start_conf;
    double start_conf;
} DraftUnitOf;
DraftUnitOf draft_world_unit_of(DraftWorld *W, i64 address);

/* unit_plan.plan_for(start, end, repo, min_conf="medium", ix=W.ix, refresh=False) with memo_plan_model's effect:
 * the model, evidence and calibration are loaded once per (root, cache stamp) in the process. NULL (None) on any
 * failure, as World.unit_of's `except Exception`. */
UpPlanDoc *draft_world_plan_for(DraftWorld *W, i64 start, i64 end);

/* brief.infer_profile(repo, address, lib) / profile_of: the nearest source unit (with cflags) of the same library,
 * else the same group, else any. lib_given false: lib from libraries.tsv (brief.World.library). */
typedef struct {
    const char *unit;     /* "main/game/PadRumble" */
    const char *library;  /* NULL: None */
    const char *compiler; /* NULL: None */
    AvpStrVec extra;      /* _base_words(cflags) */
    const char *lang;     /* NULL: None */
    const char *match;    /* "same library", "same group", "different library" */
    i64 distance;
} DraftProfile;
bool draft_infer_profile(AvpArena *a, const ShsmRepo *r, i64 address, const char *lib, bool lib_given,
                         DraftProfile *out);
AvpStrVec draft_base_words(AvpArena *a, const AvpStrVec *cflags); /* brief._base_words */
AvpJson *draft_profile_json(AvpArena *a, const DraftProfile *p);  /* the profile_of dict */

/* ==== draft_twin.c: class evidence and twin drafting ============================================================== */

/* draft.vtables_of(W, address): the objects holding a pointer to address (first-seen order). */
AvpStrVec draft_vtables_of(DraftWorld *W, i64 address);
/* draft.interned_name(W, addresses): the first `interned name` row with a text, or NULL. */
const HashesVarName *draft_interned_name(DraftWorld *W, const i64 *addresses, size_t n);
/* draft.target_class(...): the class name and its evidence (*why). */
const char *draft_target_class(DraftWorld *W, const char *target_sym, i64 target_addr, const DraftPairs *pairs,
                               const char *twin_unit, const char *twin_class, const char **why);
/* draft.pair_vtables(W, twin_addr, target_addr, twin_class, s). twin_class NULL: None. */
void draft_pair_vtables(DraftWorld *W, i64 twin_addr, i64 target_addr, const char *twin_class, DraftSubst *s);

/* draft.choose_twin(W, target): the target and its matched exact twins, best first. Raises SHSM_EXC_USAGE (already
 * matched, unknown) or SHSM_EXC_FINDING (no matched exact twin). */
TwinsFn *draft_choose_twin(DraftWorld *W, const char *target, TwinsFnVec *twins);
/* The strictest twin level at which tw is a twin of f: "ref" (same instructions and the same relocation targets) or
 * "exact" (same instructions, relocation targets may differ). The twins tool's ref groups lie inside its exact
 * groups, so an exact-level twin search already contains every ref-level twin. */
const char *draft_twin_level(const TwinsFn *f, const TwinsFn *tw);
/* draft.draft_paths(repo, out, target): abspath(out), else abspath(build/draft/<safe target>). */
const char *draft_paths(AvpArena *a, const ShsmRepo *r, const char *out, const char *target);

/* make_twin_draft's result: the meta dict (as written to meta.json) and the fields the commands read. */
typedef struct {
    AvpJson *meta; /* the meta dict, key order as the Python's */
    const char *target, *twin, *twin_source, *source, *header, *cls, *class_evidence, *unit_path, *unit_kind;
    bool has_range;
    i64 range_start, range_end;
    const DraftSubst *subst;
    const char *twin_level; /* "ref" or "exact": draft_twin_level of the twin used */
} DraftTwinMeta;
/* draft.make_twin_draft(W, target, out): writes the draft directory. out NULL: build/draft/<target>. */
DraftTwinMeta *draft_make_twin_draft(DraftWorld *W, const char *target, const char *out);
/* The twin loop of make_twin_draft: attempt(ctx, twin) for each twin in order until one returns a draft. An attempt
 * that raises SHSM_EXC_FINDING or SHSM_EXC_TYPE (the twin has no definition to clone) moves on to the next twin; any
 * other error propagates. When every twin fails it raises the first twin's error, with the number tried appended
 * when there was more than one. */
typedef DraftTwinMeta *(*DraftTwinAttempt)(void *ctx, TwinsFn *twin);
DraftTwinMeta *draft_first_twin_draft(const TwinsFnVec *twins, DraftTwinAttempt attempt, void *ctx);

/* draft.twin_unit_range(W, text): the range and how it was found. Raises SHSM_EXC_USAGE. */
void draft_twin_unit_range(DraftWorld *W, const char *text, i64 *start, i64 *end, const char **how);

/* The `twin` command (cmd_twin, cmd_twin_unit): lines and --json data into out; the exit code. function / unit /
 * out_dir NULL: not given. */
int draft_cmd_twin(DraftWorld *W, ShsmOutput *out, const char *function, const char *unit, const char *out_dir);

/* ==== draft_grade.c: grading ===================================================================================== */

/* cli.first_error(output): the compiler's first real error, `file:line: (code) message`; p NULL: None. */
AvpStr draft_first_error(AvpArena *a, AvpStr output);

/* Grader.local_copies(paths): each path on another drive than the checkout replaced by a copy under
 * build/draft/_rules/<sha1(lower(common dir))[:12]>/ (same relative layout); the others as given. */
const char **draft_local_copies(AvpArena *a, const ShsmRepo *r, const char *const *paths, size_t n);

/* Grader.rules(paths): check_rules over `--json --no-summary --root <root> <local copies>`, run in process. */
typedef struct {
    AvpJson *findings; /* the match-tier finding dicts of the document (an array); NULL when the checker failed */
    const char *error; /* the first line the failed run printed (rules_error), NULL when it ran */
    const char *r13;   /* the document's r13 note when it says R13 was skipped, else NULL */
} DraftRules;
DraftRules draft_rules(AvpArena *a, const ShsmRepo *r, const char *const *paths, size_t n);

/* draft.resolve_profile(W, target_addr, lib, meta, profile, as_unit): the unit whose flags compile the draft, *how
 * the reason. target_addr DRAFT_NONE / lib, meta, profile, as_unit NULL: None. Raises SHSM_EXC_USAGE ("no unit X",
 * "no profile: give --profile UNIT"). */
const ShsmUnit *draft_resolve_profile(DraftWorld *W, i64 target_addr, const char *lib, const AvpJson *meta,
                                      const char *profile, const char *as_unit, const char **how);

/* One entry of grade()'s functions list. */
typedef struct {
    const char *retail;   /* NULL: None */
    i64 address;
    bool marked;          /* false: a function of the range nothing marks (keys retail, address, status, rows) */
    const char *name, *cls, *compiled; /* NULL: None */
    const char *status;   /* "EXACT", "DIFF", "MISSING" */
    i64 rows;             /* DRAFT_NONE: None */
    const char *error;    /* the KeyError text, NULL: no key */
} DraftGradeFn;
typedef AVP_VEC(DraftGradeFn) DraftGradeFns;

/* grade()'s result: doc is the dict (key order as the Python's: target, file, profile, profile_how, compiler,
 * functions, screen, rules, verdict, [reason], r13_note, [rules_error], renames, blocking, lines); the other fields
 * are views of it for callers (sweep, promote). */
typedef struct {
    AvpJson *doc;
    const char *verdict;  /* "MATCH", "PARTIAL", "FAIL" */
    const char *reason;   /* NULL: no key */
    bool blocking;
    AvpStrVec lines;      /* at most 8 */
    DraftGradeFns functions;
    AvpJson *renames;     /* the doc's renames object (retail name -> draft name) */
    const char *profile, *profile_how, *compiler; /* compiler NULL: None */
    DraftRules rules;
} DraftGrade;

/* draft.grade(W, target, file, as_unit, profile, meta=meta, workdir=workdir): compile the draft with its profile,
 * compare every marked function with retail, run the rules and the screen. meta NULL: meta.json beside the file
 * (when present); workdir NULL: build/draft/_grade/<file stem>. Allocates from W->a; uses W's indexes (one World per
 * thread). Raises as the Python (SHSM_EXC_USAGE, I/O errors, DRAFT_EXC_PARSE). */
/* sweep's reason for a draft blocked only because the rule checker skipped R13 (draft.R13_SKIPPED) */
#define DRAFT_R13_SKIPPED "R13 not checked (classes.json absent or stale: run build/tools/classes.exe scan)"

DraftGrade *draft_grade(DraftWorld *W, const char *target, const char *file, const char *as_unit,
                        const char *profile, const AvpJson *meta, const char *workdir);
/* The folders of include/ a draft's cloned headers came from: a header cloned into an include folder of the draft
 * (`<draft>/include/fmod/x.h`, one of `roots`) keeps the bare-name includes it has in include/ (`#include
 * "fmod_channel_real.h"`), found beside the including file there and nowhere in the clone; the compile searches
 * `<repo_include>/fmod` as well. `headers`: the cloned files. Each folder once, in first-seen order. */
AvpStrVec draft_clone_origin_dirs(AvpArena *a, const char *repo_include, const AvpStrVec *roots,
                                 const char *const *headers, size_t nheaders);
int draft_grade_exit(const DraftGrade *g); /* 0 for MATCH without a blocking finding, else 1 */

/* ==== draft_grade.c: rejects ===================================================================================== */

typedef struct {
    const char *symbol, *approach, *reason, *basis;
    bool again; /* cmd_rejects: the twins changed since */
} DraftReject;
typedef AVP_VEC(DraftReject) DraftRejects;

DraftRejects draft_parse_rejects(AvpArena *a, AvpStr text);               /* load_rejects' parse of the text */
AvpStr draft_format_rejects(AvpArena *a, const DraftRejects *rows);       /* save_rejects' text */
DraftRejects draft_load_rejects(AvpArena *a, const ShsmRepo *r);          /* [] when the file does not exist */
void draft_save_rejects(AvpArena *a, const ShsmRepo *r, const DraftRejects *rows);
/* draft.basis_of(repo, twin_fns): sha1 of the names (sorted) and their source files, first 12 hex digits. */
const char *draft_basis_of(AvpArena *a, const ShsmRepo *r, TwinsFn *const *fns, size_t n);

/* The commands (lines and --json data into out; the exit code). */
int draft_cmd_grade(DraftWorld *W, ShsmOutput *out, const char *target, const char *file, const char *as_unit,
                    const char *profile);
int draft_cmd_reject(DraftWorld *W, ShsmOutput *out, const char *symbol, const char *reason, const char *approach);
int draft_cmd_rejects(DraftWorld *W, ShsmOutput *out);
/* cmd_packet: `python tools/brief.py <target> --lines 40 [--unit]` (a temporary Python bridge) plus the contract. */
int draft_cmd_packet(DraftWorld *W, ShsmOutput *out, const char *target, bool unit);

/* ==== draft_select.c: the queue, by function and by class, its cache, and sweep ================================== */

#define DRAFT_ADJACENT 0x100 /* a classless function this close after a class's last function belongs to its unit */

/* draft.select(W, kind, lib, limit, plan): the queue rows (dicts, key order as the Python's). kind / lib NULL: None;
 * limit 0: no cap. */
AvpJson *draft_select(DraftWorld *W, const char *kind, const char *lib, i64 limit, bool plan);

/* draft.group_by_class(entries, classes, interned, sweep, fn_info, addr_of): entries the select() rows, classes the
 * classes.json class dicts (an array; not modified), interned {function address: name} (AvpIMap of const char *,
 * NULL: none), sweep {symbol: verdict JSON} (AvpMap of AvpJson *, NULL: None), fn_info {address: TwinsFn *}
 * (AvpIMap, NULL: none), addr_of NULL: _addr_name. The groups (an array of dicts). */
typedef bool (*DraftAddrOf)(void *ctx, AvpStr name, i64 *addr);
AvpJson *draft_group_by_class(AvpArena *a, const AvpJson *entries, const AvpJson *classes, const AvpIMap *interned,
                              const AvpMap *sweep, const AvpIMap *fn_info, DraftAddrOf addr_of, void *addr_ctx);

/* draft.file_stamp(paths): sha1 of repr([(normcase(abspath(p)), st_size, st_mtime_ns) or (normcase(abspath(p)),
 * None)]). */
const char *draft_file_stamp(AvpArena *a, const char *const *paths, size_t n);
/* draft.select_inputs(repo): the files the select queue is derived from (the Python's own tools/draft.py is the
 * checkout's). */
AvpStrVec draft_select_inputs(AvpArena *a, const ShsmRepo *r);
/* draft.cached(repo, key, compute, inputs): compute(ctx) memoised in build/draft/select_cache.json (the file the
 * Python writes, same format); *hit as the Python's second value. inputs NULL: select_inputs(repo). */
typedef AvpJson *(*DraftComputeFn)(void *ctx);
AvpJson *draft_cached(AvpArena *a, const ShsmRepo *r, const char *key, DraftComputeFn compute, void *ctx,
                      const AvpStrVec *inputs, bool *hit);
/* draft.sweep_verdicts(repo): {symbol: verdict} (AvpMap of AvpJson *), or NULL with *why. */
AvpMap *draft_sweep_verdicts(AvpArena *a, const ShsmRepo *r, const char **why);
/* draft.select_classes(W, kind, lib): {"groups", "notes"}. */
AvpJson *draft_select_classes(DraftWorld *W, const char *kind, const char *lib);

/* draft.sweep(W, lib, limit, jobs): the result dicts (an array, the targets' order). limit 0: none. jobs > 1 grades
 * on a pool, one World per worker thread (the calling thread uses W). */
AvpJson *draft_sweep(DraftWorld *W, const char *lib, i64 limit, int jobs);

/* The commands. limit: args.limit (0: no cap); all: --all; limit_given: --limit was on the command line (main's
 * argv test). */
int draft_cmd_select(DraftWorld *W, ShsmOutput *out, const char *kind, const char *lib, bool functions, i64 limit,
                     bool all);
int draft_cmd_sweep(DraftWorld *W, ShsmOutput *out, const char *lib, i64 limit, bool limit_given, int jobs);

/* ==== draft_promote.c ============================================================================================= */

/* RuntimeError (names.py's refusal, a failed promote step): str(e) is the message. */
#define DRAFT_EXC_RUNTIME 0x4451

/* draft.Promoter: each external action is a function pointer so tests can stub it (the Python's tests stub the
 * methods). tool's argv[0] names the tool: "unit", "names", "match", "gatecheck", "check_rules", "neutral" run
 * <root>/build/tools/<name>.exe, "ninja" runs the checkout's tools/bin/ninja.exe (the Python ran each script with
 * its interpreter; DEVIATIONS.md, D-draft-9). unit_of and unit_source are World.unit_of and
 * units.get_unit(...).source (NULL for a KeyError or no source), here so a test can stub them as the Python's tests
 * patch them. */
typedef struct DraftPromoter {
    const ShsmRepo *repo;
    DraftWorld *W;
    void *ctx;
    AvpStrVec (*dirty)(struct DraftPromoter *p);
    ShsmRun (*tool)(struct DraftPromoter *p, const char *const *argv, size_t n);
    void (*rename)(struct DraftPromoter *p, const char *old, const char *new_); /* raises DRAFT_EXC_RUNTIME */
    AvpStrVec (*undo)(struct DraftPromoter *p, const AvpStrVec *before);
    DraftUnitOf (*unit_of)(struct DraftPromoter *p, i64 address);
    const char *(*unit_source)(struct DraftPromoter *p, const char *unit_name);
} DraftPromoter;
/* Promoter(repo): the real actions. */
void draft_promoter_init(DraftPromoter *p, DraftWorld *W);
/* The real methods (for a stub that wraps one). */
AvpStrVec draft_promoter_dirty(DraftPromoter *p);
ShsmRun draft_promoter_tool(DraftPromoter *p, const char *const *argv, size_t n);
void draft_promoter_rename(DraftPromoter *p, const char *old, const char *new_);
bool draft_promoter_local_only(DraftPromoter *p, const char *name);
AvpStrVec draft_promoter_undo(DraftPromoter *p, const AvpStrVec *before);

/* promote's grade_fn: the grade of the range (NULL: grade(W, range, source)). */
typedef DraftGrade *(*DraftPromoteGradeFn)(void *ctx, const char *range, const char *source);

/* draft.promote(W, path, source, header, text_range, dry_run, promoter, grade_fn): ok, and the lines. header NULL:
 * None; has_range false: text_range None; pr NULL: Promoter(repo). */
bool draft_promote(DraftWorld *W, const char *path, const char *source, const char *header, bool has_range,
                   i64 range_start, i64 range_end, bool dry_run, bool allow_small, DraftPromoter *pr,
                   DraftPromoteGradeFn grade_fn, void *grade_ctx, AvpStrVec *lines);
/* cmd_promote. text NULL: no --text. */
int draft_cmd_promote(DraftWorld *W, ShsmOutput *out, const char *path, const char *file, const char *header,
                      const char *text, bool dry_run, bool allow_small);

/* ==== draft_cli.c ================================================================================================= */

/* main(argv): the parser and the subcommands. */
int draft_main(int argc, char **argv);
