# rules_lex, rules_parse: check_rules' lexer and parse model

`#include "rules_lex.h"` (lexer), `"rules_parse.h"` (parse model), `"rules_sources.h"` (the source set). Library
`tool_check_rules`: another tool links it with `shsm_add_tool(<name> LIBS tool_check_rules)` in its own
`CMakeLists.txt` (README, "Unusual tools") and gets this directory on its include path. Ports
`tools/rules/check_rules.py` lines 96-1365 and 4674-4687 (constants, lexer, `Func`/`Agg`/`VarDecl`/`Proto`,
`uses_cpp_syntax`, `scan_storage_macros`, `Parsed`, `_Parser`, `load_sources`). Proven equal to the Python by the facts
dump (below): every file of `src/` and `include/` plus a synthetic corpus, 0 differences.

## Lexer (rules_lex.h)

| Python | C |
|---|---|
| `Tok(k, t, line)` | `RlTok {t, line, k}`; `k` is `RlKind` (`RL_ID RL_NUM RL_STR RL_CHR RL_OP`), `rl_kind_names[k]` the Python string |
| `Comment`, `Directive` | `RlComment {line, end_line, text, trailing}`, `RlDirective {line, end_line, name, rest, toks}` |
| `lex(text, line=1, top=True)` | `RlLexed rl_lex(a, text, line, top)`: `.toks`, `.comments`, `.directives`; `.ok == false` where the Python raises (below) |
| `_skip_quoted(text, i, q)` | `rl_skip_quoted(text, i, q)` (byte index) |
| `pair_brackets(toks)` (dict) | `i32 *rl_pair_brackets(a, toks, n)`: partner index or -1 per token |
| `ptext(toks)` | `rl_ptext(a, toks, n)` (contiguous), `rl_ptext_ptrs(a, ptrs, n)` (a list of tokens) |
| `open(p, encoding="utf-8", errors="replace").read()` | `rl_read_source(a, path, &text)` |
| `t.t == "x"`, `t.t in (...)` | `RL_TEQ(t, "x")`, `rl_tin(t, words)` (NULL-terminated list) |

Texts are UTF-8 views into the lexed text, so the text must outlive the tokens. Offsets are bytes; every Python
per-character step is one code point here (an escape skips the backslash and one whole code point; `\S` in `_OP`
takes one code point; `_NUM`'s `\d` is Unicode decimal, as `re` reads it). `ok == false` (with `error_line`) is the
Python's `AttributeError`: a Unicode whitespace code point outside the lexer's skip set (U+00A0, U+2028, ...) in code,
where `_OP.match` returns None. A file read with universal newlines has no `\r`; the lexer still treats `\r` as the
Python does when given one.

## Parse model (rules_parse.h)

| Python | C |
|---|---|
| `Parsed(path, text, storage_macros)` | `RlParsed *rl_parse(a, path, text, storage)`: `storage` is an `AvpMap` name -> `RL_STORAGE_STATIC`/`_EXTERN` (`.i`), may be NULL; free the heap parts with `rl_parsed_free` |
| `P.toks`, `P.match`, `P.comments`, `P.directives`, `P.lines` | `P->toks` (masked), `P->match`, `P->comments`, `P->directives`, `P->lines` |
| `P.funcs aggs vars fwds protos typedefs virtuals` | `P->p.funcs` ... `P->p.virtuals` (`RlFunc RlAgg RlVar RlFwd RlProto RlNameLine`) |
| `P.asm_sites`, `P.masks`, `P.asm_ids` (keyed by `id(token)`) | `P->asm_sites`, `P->masks`, `P->asm_ids` (`.tok` = index of the `__ASM_BODY__` token, `.ids` sorted) |
| `P.storage_macros.get(n)` | `rl_storage_get(P, n)` (the file's own definitions win over the argument) |
| `P.func_names`, `P.includes`, `P.is_header`, `P.is_cpp` | `P->func_names` (`AvpSet`), `P->includes`, `P->is_header`, `P->is_cpp` |
| `P.comments_on(line)` | `rl_comments_on(P, line, &n)`: indices into `P->comments` |
| `P.has_note(line, rx, above)`, `P.func_at(line)`, `P.no_copy_in_retail(fn)` | `rl_has_note(P, line, re, above)`, `rl_func_at(P, line)` (index or -1), `rl_no_copy_in_retail(P, fn)` |
| `P.cpp_syntax()`, `P.offset_macros()`, `P.volatile_noted()` | `rl_cpp_syntax(P)`, `rl_offset_macros(P)`, `rl_volatile_noted(P)` (memoized) |
| `P.cpp_classes()` | not yet (stage 3 ports `parse_cpp_classes`) |
| `uses_cpp_syntax(toks)`, `scan_storage_macros(text)` | `rl_uses_cpp_syntax(toks, n)`, `rl_scan_storage_macros(map, text)` |
| `Checker._volatile_decl_name(toks, i)` | `rl_volatile_decl_name(toks, n, i)` (`.p == NULL` for None) |
| `VOLATILE_NOTE DELAY_NOTE BYTEPTR_NOTE PAD_NOTE EXCEPTION_RE` | `rl_re_volatile_note()` ... `rl_re_exception()` |
| `load_sources(root)`; Checker.parse's `_storage_macros` | `rl_load_sources(a, root)`; `rl_project_storage_macros(S)`; `rl_source_text` |

A whole parse lives in the arena passed to `rl_parse` (`RL_VEC` vectors grow inside it), except the few `AvpMap`,
`AvpSet` and `AvpStrVec` members `rl_parsed_free` releases. Token indices in `RlFunc` (`head lparen rparen body
body_end`) index `P->toks`. Python `None` is -1 (`first_hidden`, `rl_func_at`) or a flag (`has_addr`).

## Facts dump

`check_rules --dump-facts FILE... [--root DIR]` or `--dump-facts-all` (hidden: stage 1's proof) writes one compact
JSON object per file and line (`json.dumps(d, separators=(",", ":"))`), the same bytes as
`diff/dump_facts.py` (the retired Python reference) gave for the same arguments. Keys in order: `path`, `is_header`,
`is_cpp`, `storage_macros` (sorted `[name, word]`), `nlines`, `toks` (`[k, t, line]`), `match` (`[i, j]` by i),
`comments` (`[line, end_line, text, trailing]`), `directives` (`[line, end_line, name, rest, toks]`), `includes`,
`asm_sites`, `masks`, `asm_ids` (`[token index, sorted ids]`), `funcs`, `aggs`, `vars` (objects with every attribute
of the Python class, in its `__init__` order), `fwds`, `protos`, `typedefs`, `virtuals`, `func_names` (sorted),
`comments_by_line` (`[line, [comment indices]]` by line), `cpp_syntax`, `offset_macros`, `volatile_noted` (sorted),
`func_at` (`[line, func index or -1]` at line 0 and every `f.line` and `f.end_line + 1`), `no_copy_in_retail` (per
function), `notes` (per regex, the lines 1..nlines where `has_note` holds). A file the lexer rejects is
`{"path": ..., "error": "AttributeError"}`.
